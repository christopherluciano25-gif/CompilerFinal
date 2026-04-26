#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "codegen.h"
#include "symtab.h"
#include "tac.h"

extern SymbolTable symtab;

/* Saved copy of the global-scope symbol table so globals remain
   accessible after per-function initSymTab() calls */
static SymbolTable globalSymtab;
static int globalSymtabSaved = 0;

/* Look up a variable offset — check local symtab first, then global */
static int lookupOffset(const char* name) {
    int off = getVarOffset((char*)name);
    if (off != -1) return off;
    if (globalSymtabSaved) {
        /* Search global symtab */
        for (int i = 0; i < globalSymtab.count; i++) {
            if (globalSymtab.vars[i].name &&
                strcmp(globalSymtab.vars[i].name, name) == 0)
                return globalSymtab.vars[i].offset;
        }
    }
    return -1;
}

FILE* output;
int tempReg = 0;
RegisterAllocator regAlloc;

// Function call argument tracking
#define MAX_ARGS 4
char* argList[MAX_ARGS];
int argCount = 0;
int paramIndex = 0;  // Track parameter index in current function

int getNextTemp() {
    int reg = tempReg++;
    if (tempReg > 7) tempReg = 0;  // Reuse $t0-$t7
    return reg;
}

/* REGISTER ALLOCATION ALGORITHM
 * Implements a graph-coloring inspired approach with LRU spilling
 * Key features:
 * 1. Tracks which variables/temporaries are in which registers
 * 2. Detects dirty registers (modified values)
 * 3. Uses LRU (Least Recently Used) for spill victim selection
 * 4. Automatically spills to memory when out of registers
 */

/* Initialize register allocator */
void initRegAlloc() {
    for (int i = 0; i < NUM_TEMP_REGS; i++) {
        regAlloc.regs[i].varName[0] = '\0';
        regAlloc.regs[i].isDirty = 0;
        regAlloc.regs[i].inUse = 0;
        regAlloc.regs[i].lastUsed = 0;
    }
    regAlloc.timestamp = 0;
    regAlloc.spillCount = 0;
    printf("MIPS: Register allocator initialized (10 temp registers: $t0-$t9)\n\n");
}

/* Find which register holds a variable (return -1 if not in register) */
int findVarReg(const char* varName) {
    for (int i = 0; i < NUM_TEMP_REGS; i++) {
        if (regAlloc.regs[i].inUse &&
            strcmp(regAlloc.regs[i].varName, varName) == 0) {
            regAlloc.regs[i].lastUsed = regAlloc.timestamp++;
            return i;
        }
    }
    return -1;
}

/* Select victim register for spilling using LRU */
int selectVictimReg() {
    int victim = 0;
    int oldestTime = regAlloc.regs[0].lastUsed;

    for (int i = 1; i < NUM_TEMP_REGS; i++) {
        if (regAlloc.regs[i].lastUsed < oldestTime) {
            oldestTime = regAlloc.regs[i].lastUsed;
            victim = i;
        }
    }

    return victim;
}

/* Spill a register to memory */
void spillReg(int regNum) {
    if (!regAlloc.regs[regNum].inUse) {
        return;  /* Nothing to spill */
    }

    /* Only spill if dirty (value was modified) */
    if (regAlloc.regs[regNum].isDirty) {
        int offset = getVarOffset(regAlloc.regs[regNum].varName);
        if (offset != -1) {
            fprintf(output, "    # Spilling $t%d (%s) to memory\n", regNum,
                    regAlloc.regs[regNum].varName);
            fprintf(output, "    sw $t%d, %d($sp)\n", regNum, offset);
            regAlloc.spillCount++;
        }
    }

    /* Mark register as free */
    regAlloc.regs[regNum].inUse = 0;
    regAlloc.regs[regNum].isDirty = 0;
    regAlloc.regs[regNum].varName[0] = '\0';
}

/* Allocate a register for a variable */
int allocReg(const char* varName) {
    /* Check if variable is already in a register */
    int existingReg = findVarReg(varName);
    if (existingReg != -1) {
        printf("    [REG ALLOC] Variable '%s' already in $t%d\n", varName, existingReg);
        return existingReg;
    }

    /* Find a free register */
    for (int i = 0; i < NUM_TEMP_REGS; i++) {
        if (!regAlloc.regs[i].inUse) {
            regAlloc.regs[i].inUse = 1;
            strncpy(regAlloc.regs[i].varName, varName, MAX_VAR_NAME - 1);
            regAlloc.regs[i].isDirty = 0;
            regAlloc.regs[i].lastUsed = regAlloc.timestamp++;
            printf("    [REG ALLOC] Allocated $t%d for '%s'\n", i, varName);
            return i;
        }
    }

    /* No free registers - must spill using LRU */
    int victim = selectVictimReg();
    printf("    [REG ALLOC] All registers full, spilling $t%d (%s)\n",
           victim, regAlloc.regs[victim].varName);
    spillReg(victim);

    /* Allocate the freed register */
    regAlloc.regs[victim].inUse = 1;
    strncpy(regAlloc.regs[victim].varName, varName, MAX_VAR_NAME - 1);
    regAlloc.regs[victim].isDirty = 0;
    regAlloc.regs[victim].lastUsed = regAlloc.timestamp++;
    printf("    [REG ALLOC] Allocated $t%d for '%s'\n", victim, varName);
    return victim;
}

/* Free a register */
void freeReg(int regNum) {
    if (regNum >= 0 && regNum < NUM_TEMP_REGS) {
        /* Spill if dirty before freeing */
        if (regAlloc.regs[regNum].isDirty) {
            spillReg(regNum);
        }
        regAlloc.regs[regNum].inUse = 0;
        regAlloc.regs[regNum].varName[0] = '\0';
        printf("    [REG ALLOC] Freed $t%d\n", regNum);
    }
}

/* Print register allocation statistics */
void printRegAllocStats() {
    printf("\n┌──────────────────────────────────────────────────────────┐\n");
    printf("│ REGISTER ALLOCATION STATISTICS                           │\n");
    printf("├──────────────────────────────────────────────────────────┤\n");
    printf("│ Total registers spilled:     %3d                         │\n", regAlloc.spillCount);
    printf("│ Final register usage:                                    │\n");

    int inUseCount = 0;
    for (int i = 0; i < NUM_TEMP_REGS; i++) {
        if (regAlloc.regs[i].inUse) {
            printf("│   $t%-2d: %-20s%s                      │\n",
                   i, regAlloc.regs[i].varName,
                   regAlloc.regs[i].isDirty ? " (dirty)" : "");
            inUseCount++;
        }
    }

    if (inUseCount == 0) {
        printf("│   (all registers free)                                   │\n");
    }

    printf("└──────────────────────────────────────────────────────────┘\n\n");
}

/* Generate expression code with register allocation */
int genExpr(ASTNode* node) {
    if (!node) return -1;

    switch(node->type) {
        case NODE_NUM: {
            /* Allocate register for constant */
            char tempName[32];
            sprintf(tempName, "const_%d", node->data.num);
            int reg = allocReg(tempName);
            fprintf(output, "    li $t%d, %d         # Load constant %d\n",
                    reg, node->data.num, node->data.num);
            return reg;
        }

        case NODE_VAR: {
            int offset = getVarOffset(node->data.name);
            if (offset == -1) {
                fprintf(stderr, "Error: Variable %s not declared\n", node->data.name);
                exit(1);
            }

            /* Check if variable is already in a register */
            int reg = findVarReg(node->data.name);
            if (reg == -1) {
                /* Not in register, allocate one and load from memory */
                reg = allocReg(node->data.name);
                fprintf(output, "    lw $t%d, %d($sp)     # Load variable '%s'\n",
                        reg, offset, node->data.name);
            }
            return reg;
        }

        case NODE_BINOP: {
            /* Generate left operand */
            int leftReg = genExpr(node->data.binop.left);

            /* Generate right operand */
            int rightReg = genExpr(node->data.binop.right);

            /* Perform operation, store result in leftReg */
            fprintf(output, "    add $t%d, $t%d, $t%d   # Binary operation\n",
                    leftReg, leftReg, rightReg);

            /* Mark result register as dirty */
            regAlloc.regs[leftReg].isDirty = 1;

            /* Free the right register as it's no longer needed */
            freeReg(rightReg);

            return leftReg;
        }

        default:
            return -1;
    }
}

void genStmt(ASTNode* node) {
    if (!node) return;

    switch(node->type) {
        case NODE_DECL: {
            int offset = addVar(node->data.name);
            if (offset == -1) {
                fprintf(stderr, "Error: Variable %s already declared\n", node->data.name);
                exit(1);
            }
            fprintf(output, "    # Declared '%s' at offset %d\n", node->data.name, offset);
            break;
        }

        case NODE_ASSIGN: {
            int offset = getVarOffset(node->data.assign.var);
            if (offset == -1) {
                fprintf(stderr, "Error: Variable %s not declared\n", node->data.assign.var);
                exit(1);
            }

            /* Generate expression and get result register */
            int reg = genExpr(node->data.assign.value);

            /* Store to variable's memory location */
            fprintf(output, "    sw $t%d, %d($sp)     # Assign to '%s'\n",
                    reg, offset, node->data.assign.var);

            /* Update register descriptor - this register now holds the variable */
            strncpy(regAlloc.regs[reg].varName, node->data.assign.var, MAX_VAR_NAME - 1);
            regAlloc.regs[reg].isDirty = 0;  /* No longer dirty after store */

            break;
        }

        case NODE_PRINT: {
            /* Generate expression and get result register */
            int reg = genExpr(node->data.expr);

            /* Print the value */
            fprintf(output, "    # Print integer\n");
            fprintf(output, "    move $a0, $t%d\n", reg);
            fprintf(output, "    li $v0, 1\n");
            fprintf(output, "    syscall\n");

            /* Print newline */
            fprintf(output, "    # Print newline\n");
            fprintf(output, "    li $v0, 11\n");
            fprintf(output, "    li $a0, 10\n");
            fprintf(output, "    syscall\n");

            /* Free register after print */
            freeReg(reg);
            break;
        }

        case NODE_STMT_LIST:
            genStmt(node->data.stmtlist.stmt);
            genStmt(node->data.stmtlist.next);
            break;

        default:
            break;
    }
}

void generateMIPS(ASTNode* root, const char* filename) {
    output = fopen(filename, "w");
    if (!output) {
        fprintf(stderr, "Cannot open output file %s\n", filename);
        exit(1);
    }

    // Initialize symbol table and register allocator
    initSymTab();
    initRegAlloc();

    // MIPS program header
    fprintf(output, ".data\n");
    fprintf(output, "\n.text\n");
    fprintf(output, ".globl main\n");
    fprintf(output, "main:\n");

    // Allocate stack space (max 100 variables * 4 bytes)
    fprintf(output, "    # Allocate stack space\n");
    fprintf(output, "    addi $sp, $sp, -400\n\n");

    // Generate code for statements
    genStmt(root);

    // Spill any remaining dirty registers
    fprintf(output, "\n    # Spill any remaining registers\n");
    for (int i = 0; i < NUM_TEMP_REGS; i++) {
        if (regAlloc.regs[i].inUse && regAlloc.regs[i].isDirty) {
            spillReg(i);
        }
    }

    // Program exit
    fprintf(output, "\n    # Exit program\n");
    fprintf(output, "    addi $sp, $sp, 400\n");
    fprintf(output, "    li $v0, 10\n");
    fprintf(output, "    syscall\n");

    fclose(output);

    // Print register allocation statistics
    printRegAllocStats();
}

/* Helper function to check if a string is a TAC temporary (t0, t1, etc.) */
static int isTACTemp(const char* name) {
    return (name && name[0] == 't' && name[1] >= '0' && name[1] <= '9');
}

/* Helper function to check if a string is a constant */
static int isConstant(const char* str) {
    if (!str || str[0] == '\0') return 0;
    int i = 0;
    if (str[i] == '-' || str[i] == '+') i++;  /* Handle sign */
    while (str[i] != '\0') {
        if (str[i] < '0' || str[i] > '9') return 0;
        i++;
    }
    return 1;
}

/* GENERATE MIPS CODE FROM OPTIMIZED TAC
 * This function consumes the optimized TAC and produces MIPS assembly
 * Key advantages:
 * 1. Uses optimized TAC (constant folding, copy propagation already done)
 * 2. Simpler code generation logic (TAC is already linearized)
 * 3. Better code quality due to optimizations
 */
void generateMIPSFromTAC(const char* filename) {
    output = fopen(filename, "w");
    if (!output) {
        fprintf(stderr, "Cannot open output file %s\n", filename);
        exit(1);
    }

    // Initialize symbol table and register allocator
    initSymTab();
    initRegAlloc();

    // Get optimized TAC list
    TACList* tac = getOptimizedTAC();
    if (!tac || !tac->head) {
        fprintf(stderr, "Error: No optimized TAC available\n");
        fclose(output);
        return;
    }

    printf("\n┌──────────────────────────────────────────────────────────┐\n");
    printf("│ TRANSLATING TAC TO MIPS ASSEMBLY                        │\n");
    printf("├──────────────────────────────────────────────────────────┤\n");
    printf("│ Each TAC instruction will be converted to MIPS code     │\n");
    printf("└──────────────────────────────────────────────────────────┘\n\n");

    // MIPS program header
    fprintf(output, ".data\n");
    fprintf(output, "\n.text\n");
    fprintf(output, ".globl main\n");
    fprintf(output, "main:\n");

    // Allocate stack space (max 100 variables * 4 bytes)
    fprintf(output, "    # Allocate stack space\n");
    fprintf(output, "    addi $sp, $sp, -400\n");
    fprintf(output, "    j main_start       # Jump to main function\n\n");

    printf("Generated program entry point and stack allocation\n\n");

    // Process each TAC instruction
    TACInstr* curr = tac->head;
    int tacLineNum = 1;
    while (curr) {
        switch(curr->op) {
            case TAC_FUNC_BEGIN: {
                // Function begin - reset symbol table for new function scope
                // Each function has its own local symbol table
                printf("\n[TAC %3d] ═══════════════════════════════════════════\n", tacLineNum);
                printf("[TAC %3d] FUNC_BEGIN: %s\n", tacLineNum, curr->result);
                printf("[TAC %3d] ═══════════════════════════════════════════\n", tacLineNum);
                printf("          → Initializing new symbol table for function scope\n");
                /* Save global symtab before wiping it */
                if (!globalSymtabSaved) {
                    globalSymtab = symtab;  /* struct copy */
                    globalSymtabSaved = 1;
                    printf("          → Global symbol table saved (%d globals)\n", globalSymtab.count);
                }
                initSymTab();
                initRegAlloc();
                paramIndex = 0;  // Reset parameter index for new function

                // Pre-pass: scan ahead and register ALL params/decls/arrays
                // so every variable has a correct stack offset before code emits
                printf("          → Pre-scanning function body to build symbol table\n");
                TACInstr* scan = curr->next;
                while (scan && scan->op != TAC_FUNC_END) {
                    if (scan->op == TAC_PARAM && scan->result && getVarOffset(scan->result) == -1)
                        addVar(scan->result);
                    else if (scan->op == TAC_DECL && scan->result && getVarOffset(scan->result) == -1)
                        addVar(scan->result);
                    else if (scan->op == TAC_ARRAY_DECL && scan->result && scan->arg1
                             && getVarOffset(scan->result) == -1)
                        addArray(scan->result, atoi(scan->arg1));
                    scan = scan->next;
                }
                printf("          → Symbol table built with %d entries\n", symtab.count);

                fprintf(output, "\n# Function: %s\n", curr->result);
                if (strcmp(curr->result, "main") != 0) {
                    fprintf(output, "func_%s:\n", curr->result);
                    fprintf(output, "    # Save return address and allocate local stack frame\n");
                    printf("          → Generated label: func_%s\n", curr->result);
                } else {
                    fprintf(output, "main_start:\n");
                    printf("          → Generated label: main_start\n");
                }
                break;
            }

            case TAC_FUNC_END: {
                // Function end
                fprintf(output, "    # End of function %s\n", curr->result);
                if (strcmp(curr->result, "main") != 0) {
                    fprintf(output, "    # Restore and return\n");
                    fprintf(output, "    jr $ra\n");
                }
                break;
            }

            case TAC_PARAM: {
                // Function parameter - already in symbol table from pre-pass
                // Just emit the store from $a register to stack
                printf("[TAC %3d] PARAM: %s\n", tacLineNum, curr->result);
                int offset = lookupOffset(curr->result);
                fprintf(output, "    # Parameter '%s' at offset %d\n", curr->result, offset);

                // Copy parameter from $a register to stack
                if (paramIndex < MAX_ARGS) {
                    printf("          → Receiving from $a%d, storing at offset %d\n",
                           paramIndex, offset);
                    fprintf(output, "    sw $a%d, %d($sp)     # Store parameter '%s'\n",
                            paramIndex, offset, curr->result);
                    paramIndex++;
                }
                break;
            }

            case TAC_DECL: {
                // If we haven't entered any function yet, this is a global — add it now
                if (!globalSymtabSaved) {
                    printf("[TAC %3d] DECL: %s (global variable)\n", tacLineNum, curr->result);
                    int offset = addVar(curr->result);
                    fprintf(output, "    # Declared global '%s' at offset %d\n", curr->result, offset);
                } else {
                    // Variable already in symbol table from pre-pass, just annotate
                    printf("[TAC %3d] DECL: %s (local variable)\n", tacLineNum, curr->result);
                    int offset = lookupOffset(curr->result);
                    fprintf(output, "    # Declared '%s' at offset %d\n", curr->result, offset);
                }
                break;
            }

            case TAC_ARRAY_DECL: {
                int size = atoi(curr->arg1);
                if (!globalSymtabSaved) {
                    // Global array — add it now
                    printf("[TAC %3d] ARRAY_DECL: %s[%d] (global array)\n", tacLineNum, curr->result, size);
                    int offset = addArray(curr->result, size);
                    fprintf(output, "    # Declared global array '%s[%d]' at offset %d (%d bytes)\n",
                            curr->result, size, offset, size * 4);
                } else {
                    // Array already in symbol table from pre-pass, just annotate
                    printf("[TAC %3d] ARRAY_DECL: %s[%d]\n", tacLineNum, curr->result, size);
                    int offset = lookupOffset(curr->result);
                    fprintf(output, "    # Declared array '%s[%d]' at offset %d (%d bytes)\n",
                            curr->result, size, offset, size * 4);
                }
                break;
            }

            case TAC_LABEL: {
                // Emit a jump label
                printf("[TAC %3d] LABEL: %s\n", tacLineNum, curr->result);
                fprintf(output, "%s:\n", curr->result);
                break;
            }

            case TAC_GOTO: {
                // Unconditional jump
                printf("[TAC %3d] GOTO: %s\n", tacLineNum, curr->arg1);
                fprintf(output, "    j %s        # Unconditional jump\n", curr->arg1);
                break;
            }

            case TAC_IF_FALSE: {
                // if arg1 == 0 goto arg2
                printf("[TAC %3d] IF_FALSE: if !%s goto %s\n", tacLineNum, curr->arg1, curr->arg2);
                printf("          → Loading condition into register\n");
                int regCond;
                if (isConstant(curr->arg1)) {
                    char tempName[32];
                    sprintf(tempName, "const_%s", curr->arg1);
                    regCond = allocReg(tempName);
                    fprintf(output, "    li $t%d, %s         # Load constant condition\n",
                            regCond, curr->arg1);
                } else {
                    regCond = findVarReg(curr->arg1);
                    if (regCond == -1) {
                        int offset = lookupOffset(curr->arg1);
                        regCond = allocReg(curr->arg1);
                        if (offset != -1)
                            fprintf(output, "    lw $t%d, %d($sp)     # Load condition '%s'\n",
                                    regCond, offset, curr->arg1);
                        else
                            fprintf(output, "    li $t%d, 0           # TAC temp not in memory\n", regCond);
                    }
                }
                printf("          → Branch to %s if condition is false (zero)\n", curr->arg2);
                fprintf(output, "    beq $t%d, $zero, %s   # if !%s goto %s\n",
                        regCond, curr->arg2, curr->arg1, curr->arg2);
                break;
            }

            case TAC_IF_TRUE: {
                // if arg1 != 0 goto arg2
                printf("[TAC %3d] IF_TRUE: if %s goto %s\n", tacLineNum, curr->arg1, curr->arg2);
                printf("          → Loading condition into register\n");
                int regCond;
                if (isConstant(curr->arg1)) {
                    char tempName[32];
                    sprintf(tempName, "const_%s", curr->arg1);
                    regCond = allocReg(tempName);
                    fprintf(output, "    li $t%d, %s         # Load constant condition\n",
                            regCond, curr->arg1);
                } else {
                    regCond = findVarReg(curr->arg1);
                    if (regCond == -1) {
                        int offset = lookupOffset(curr->arg1);
                        regCond = allocReg(curr->arg1);
                        if (offset != -1)
                            fprintf(output, "    lw $t%d, %d($sp)     # Load condition '%s'\n",
                                    regCond, offset, curr->arg1);
                        else
                            fprintf(output, "    li $t%d, 0           # TAC temp not in memory\n", regCond);
                    }
                }
                printf("          → Branch to %s if condition is true (non-zero)\n", curr->arg2);
                fprintf(output, "    bne $t%d, $zero, %s   # if %s goto %s\n",
                        regCond, curr->arg2, curr->arg1, curr->arg2);
                break;
            }

            case TAC_NEG: {
                // result = -arg1
                printf("[TAC %3d] NEG: %s = -%s\n", tacLineNum, curr->result, curr->arg1);
                printf("          → Loading operand into register\n");
                int regSrc;
                if (isConstant(curr->arg1)) {
                    char tempName[32];
                    sprintf(tempName, "const_%s", curr->arg1);
                    regSrc = allocReg(tempName);
                    fprintf(output, "    li $t%d, %s         # Load constant %s\n",
                            regSrc, curr->arg1, curr->arg1);
                } else {
                    regSrc = findVarReg(curr->arg1);
                    if (regSrc == -1) {
                        int offset = lookupOffset(curr->arg1);
                        regSrc = allocReg(curr->arg1);
                        if (offset != -1)
                            fprintf(output, "    lw $t%d, %d($sp)     # Load variable '%s'\n",
                                    regSrc, offset, curr->arg1);
                        else
                            fprintf(output, "    li $t%d, 0           # TAC temp not in memory\n", regSrc);
                    }
                }
                int regResult = allocReg(curr->result);
                printf("          → Executing: $t%d = 0 - $t%d (negate)\n", regResult, regSrc);
                fprintf(output, "    sub $t%d, $zero, $t%d   # %s = -%s\n",
                        regResult, regSrc, curr->result, curr->arg1);
                if (!isTACTemp(curr->result)) {
                    regAlloc.regs[regResult].isDirty = 1;
                    int offset = lookupOffset(curr->result);
                    if (offset != -1) {
                        fprintf(output, "    sw $t%d, %d($sp)     # Store to '%s'\n",
                                regResult, offset, curr->result);
                        regAlloc.regs[regResult].isDirty = 0;
                    }
                }
                freeReg(regSrc);
                printf("          → Result stored in $t%d\n\n", regResult);
                break;
            }

                        case TAC_AND: {
                // result = arg1 && arg2  (logical AND: 1 if both non-zero, else 0)
                printf("[TAC %3d] AND: %s = %s && %s\n", tacLineNum, curr->result, curr->arg1, curr->arg2);
                printf("          → Loading operands, emitting MIPS AND + normalize\n");
                int reg1, reg2, regResult;

                if (isConstant(curr->arg1)) {
                    char tempName[32]; sprintf(tempName, "const_%s", curr->arg1);
                    reg1 = allocReg(tempName);
                    fprintf(output, "    li $t%d, %s         # Load constant %s\n", reg1, curr->arg1, curr->arg1);
                } else {
                    reg1 = findVarReg(curr->arg1);
                    if (reg1 == -1) {
                        int offset = lookupOffset(curr->arg1);
                        reg1 = allocReg(curr->arg1);
                        if (offset != -1)
                            fprintf(output, "    lw $t%d, %d($sp)     # Load '%s'\n", reg1, offset, curr->arg1);
                        else
                            fprintf(output, "    li $t%d, 0           # TAC temp not in memory\n", reg1);
                    }
                }
                if (isConstant(curr->arg2)) {
                    char tempName[32]; sprintf(tempName, "const_%s", curr->arg2);
                    reg2 = allocReg(tempName);
                    fprintf(output, "    li $t%d, %s         # Load constant %s\n", reg2, curr->arg2, curr->arg2);
                } else {
                    reg2 = findVarReg(curr->arg2);
                    if (reg2 == -1) {
                        int offset = lookupOffset(curr->arg2);
                        reg2 = allocReg(curr->arg2);
                        if (offset != -1)
                            fprintf(output, "    lw $t%d, %d($sp)     # Load '%s'\n", reg2, offset, curr->arg2);
                        else
                            fprintf(output, "    li $t%d, 0           # TAC temp not in memory\n", reg2);
                    }
                }
                regResult = allocReg(curr->result);
                // Normalize both to 0/1 then AND them
                fprintf(output, "    sne $t%d, $t%d, $zero   # normalize LHS to 0/1\n", reg1, reg1);
                fprintf(output, "    sne $t%d, $t%d, $zero   # normalize RHS to 0/1\n", reg2, reg2);
                fprintf(output, "    and $t%d, $t%d, $t%d    # %s = %s && %s\n",
                        regResult, reg1, reg2, curr->result, curr->arg1, curr->arg2);
                if (!isTACTemp(curr->result)) {
                    regAlloc.regs[regResult].isDirty = 1;
                    int offset = lookupOffset(curr->result);
                    if (offset != -1) {
                        fprintf(output, "    sw $t%d, %d($sp)     # Store to '%s'\n", regResult, offset, curr->result);
                        regAlloc.regs[regResult].isDirty = 0;
                    }
                }
                freeReg(reg1); freeReg(reg2);
                printf("          → Result (1=true, 0=false) stored in $t%d\n\n", regResult);
                break;
            }

            case TAC_OR: {
                // result = arg1 || arg2  (logical OR: 1 if either non-zero)
                printf("[TAC %3d] OR: %s = %s || %s\n", tacLineNum, curr->result, curr->arg1, curr->arg2);
                printf("          → Loading operands, emitting MIPS OR + normalize\n");
                int reg1, reg2, regResult;

                if (isConstant(curr->arg1)) {
                    char tempName[32]; sprintf(tempName, "const_%s", curr->arg1);
                    reg1 = allocReg(tempName);
                    fprintf(output, "    li $t%d, %s         # Load constant %s\n", reg1, curr->arg1, curr->arg1);
                } else {
                    reg1 = findVarReg(curr->arg1);
                    if (reg1 == -1) {
                        int offset = lookupOffset(curr->arg1);
                        reg1 = allocReg(curr->arg1);
                        if (offset != -1)
                            fprintf(output, "    lw $t%d, %d($sp)     # Load '%s'\n", reg1, offset, curr->arg1);
                        else
                            fprintf(output, "    li $t%d, 0           # TAC temp not in memory\n", reg1);
                    }
                }
                if (isConstant(curr->arg2)) {
                    char tempName[32]; sprintf(tempName, "const_%s", curr->arg2);
                    reg2 = allocReg(tempName);
                    fprintf(output, "    li $t%d, %s         # Load constant %s\n", reg2, curr->arg2, curr->arg2);
                } else {
                    reg2 = findVarReg(curr->arg2);
                    if (reg2 == -1) {
                        int offset = lookupOffset(curr->arg2);
                        reg2 = allocReg(curr->arg2);
                        if (offset != -1)
                            fprintf(output, "    lw $t%d, %d($sp)     # Load '%s'\n", reg2, offset, curr->arg2);
                        else
                            fprintf(output, "    li $t%d, 0           # TAC temp not in memory\n", reg2);
                    }
                }
                regResult = allocReg(curr->result);
                fprintf(output, "    or $t%d, $t%d, $t%d     # bitwise OR\n", regResult, reg1, reg2);
                fprintf(output, "    sne $t%d, $t%d, $zero   # %s = %s || %s (normalize to 0/1)\n",
                        regResult, regResult, curr->result, curr->arg1, curr->arg2);
                if (!isTACTemp(curr->result)) {
                    regAlloc.regs[regResult].isDirty = 1;
                    int offset = lookupOffset(curr->result);
                    if (offset != -1) {
                        fprintf(output, "    sw $t%d, %d($sp)     # Store to '%s'\n", regResult, offset, curr->result);
                        regAlloc.regs[regResult].isDirty = 0;
                    }
                }
                freeReg(reg1); freeReg(reg2);
                printf("          → Result (1=true, 0=false) stored in $t%d\n\n", regResult);
                break;
            }

            case TAC_NOT: {
                // result = !arg1  (logical NOT: 1 if arg1 is zero, else 0)
                printf("[TAC %3d] NOT: %s = !%s\n", tacLineNum, curr->result, curr->arg1);
                printf("          → seq with zero gives logical NOT\n");
                int regSrc;
                if (isConstant(curr->arg1)) {
                    char tempName[32]; sprintf(tempName, "const_%s", curr->arg1);
                    regSrc = allocReg(tempName);
                    fprintf(output, "    li $t%d, %s         # Load constant %s\n", regSrc, curr->arg1, curr->arg1);
                } else {
                    regSrc = findVarReg(curr->arg1);
                    if (regSrc == -1) {
                        int offset = lookupOffset(curr->arg1);
                        regSrc = allocReg(curr->arg1);
                        if (offset != -1)
                            fprintf(output, "    lw $t%d, %d($sp)     # Load '%s'\n", regSrc, offset, curr->arg1);
                        else
                            fprintf(output, "    li $t%d, 0           # TAC temp not in memory\n", regSrc);
                    }
                }
                int regResult = allocReg(curr->result);
                fprintf(output, "    seq $t%d, $t%d, $zero   # %s = !%s (1 if zero, 0 if non-zero)\n",
                        regResult, regSrc, curr->result, curr->arg1);
                if (!isTACTemp(curr->result)) {
                    regAlloc.regs[regResult].isDirty = 1;
                    int offset = lookupOffset(curr->result);
                    if (offset != -1) {
                        fprintf(output, "    sw $t%d, %d($sp)     # Store to '%s'\n", regResult, offset, curr->result);
                        regAlloc.regs[regResult].isDirty = 0;
                    }
                }
                freeReg(regSrc);
                printf("          → Result (1=true, 0=false) stored in $t%d\n\n", regResult);
                break;
            }

            case TAC_ADD: {
                // result = arg1 + arg2
                printf("[TAC %3d] ADD: %s = %s + %s\n", tacLineNum, curr->result, curr->arg1, curr->arg2);
                printf("          → Loading operands into registers\n");
                int reg1, reg2, regResult;

                // Load arg1 into register
                if (isConstant(curr->arg1)) {
                    char tempName[32];
                    sprintf(tempName, "const_%s", curr->arg1);
                    reg1 = allocReg(tempName);
                    fprintf(output, "    li $t%d, %s         # Load constant %s\n",
                            reg1, curr->arg1, curr->arg1);
                } else {
                    reg1 = findVarReg(curr->arg1);
                    if (reg1 == -1) {
                        int offset = lookupOffset(curr->arg1);
                        reg1 = allocReg(curr->arg1);
                        if (offset != -1)
                            fprintf(output, "    lw $t%d, %d($sp)     # Load variable '%s'\n",
                                    reg1, offset, curr->arg1);
                        else
                            fprintf(output, "    li $t%d, 0           # TAC temp not in memory\n", reg1);
                    }
                }

                // Load arg2 into register
                if (isConstant(curr->arg2)) {
                    char tempName[32];
                    sprintf(tempName, "const_%s", curr->arg2);
                    reg2 = allocReg(tempName);
                    fprintf(output, "    li $t%d, %s         # Load constant %s\n",
                            reg2, curr->arg2, curr->arg2);
                } else {
                    reg2 = findVarReg(curr->arg2);
                    if (reg2 == -1) {
                        int offset = lookupOffset(curr->arg2);
                        reg2 = allocReg(curr->arg2);
                        if (offset != -1)
                            fprintf(output, "    lw $t%d, %d($sp)     # Load variable '%s'\n",
                                    reg2, offset, curr->arg2);
                        else
                            fprintf(output, "    li $t%d, 0           # TAC temp not in memory\n", reg2);
                    }
                }

                regResult = allocReg(curr->result);

                printf("          → Executing: $t%d = $t%d + $t%d\n", regResult, reg1, reg2);
                fprintf(output, "    add $t%d, $t%d, $t%d   # %s = %s + %s\n",
                        regResult, reg1, reg2, curr->result, curr->arg1, curr->arg2);

                if (!isTACTemp(curr->result)) {
                    regAlloc.regs[regResult].isDirty = 1;
                    int offset = lookupOffset(curr->result);
                    if (offset != -1) {
                        fprintf(output, "    sw $t%d, %d($sp)     # Store to '%s'\n",
                                regResult, offset, curr->result);
                        regAlloc.regs[regResult].isDirty = 0;
                    }
                }

                freeReg(reg1);
                freeReg(reg2);
                printf("          → Result stored in $t%d\n\n", regResult);
                break;
            }

            case TAC_SUB: {
                // result = arg1 - arg2
                printf("[TAC %3d] SUB: %s = %s - %s\n", tacLineNum, curr->result, curr->arg1, curr->arg2);
                printf("          → Loading operands into registers\n");
                int reg1, reg2, regResult;

                if (isConstant(curr->arg1)) {
                    char tempName[32]; sprintf(tempName, "const_%s", curr->arg1);
                    reg1 = allocReg(tempName);
                    fprintf(output, "    li $t%d, %s         # Load constant %s\n", reg1, curr->arg1, curr->arg1);
                } else {
                    reg1 = findVarReg(curr->arg1);
                    if (reg1 == -1) {
                        int offset = lookupOffset(curr->arg1);
                        reg1 = allocReg(curr->arg1);
                        if (offset != -1)
                            fprintf(output, "    lw $t%d, %d($sp)     # Load variable '%s'\n", reg1, offset, curr->arg1);
                        else
                            fprintf(output, "    li $t%d, 0           # TAC temp not in memory\n", reg1);
                    }
                }

                if (isConstant(curr->arg2)) {
                    char tempName[32]; sprintf(tempName, "const_%s", curr->arg2);
                    reg2 = allocReg(tempName);
                    fprintf(output, "    li $t%d, %s         # Load constant %s\n", reg2, curr->arg2, curr->arg2);
                } else {
                    reg2 = findVarReg(curr->arg2);
                    if (reg2 == -1) {
                        int offset = lookupOffset(curr->arg2);
                        reg2 = allocReg(curr->arg2);
                        if (offset != -1)
                            fprintf(output, "    lw $t%d, %d($sp)     # Load variable '%s'\n", reg2, offset, curr->arg2);
                        else
                            fprintf(output, "    li $t%d, 0           # TAC temp not in memory\n", reg2);
                    }
                }

                regResult = allocReg(curr->result);
                printf("          → Executing: $t%d = $t%d - $t%d\n", regResult, reg1, reg2);
                fprintf(output, "    sub $t%d, $t%d, $t%d   # %s = %s - %s\n",
                        regResult, reg1, reg2, curr->result, curr->arg1, curr->arg2);

                if (!isTACTemp(curr->result)) {
                    regAlloc.regs[regResult].isDirty = 1;
                    int offset = lookupOffset(curr->result);
                    if (offset != -1) {
                        fprintf(output, "    sw $t%d, %d($sp)     # Store to '%s'\n", regResult, offset, curr->result);
                        regAlloc.regs[regResult].isDirty = 0;
                    }
                }
                freeReg(reg1); freeReg(reg2);
                printf("          → Result stored in $t%d\n\n", regResult);
                break;
            }

            case TAC_MUL: {
                // result = arg1 * arg2
                printf("[TAC %3d] MUL: %s = %s * %s\n", tacLineNum, curr->result, curr->arg1, curr->arg2);
                printf("          → Loading operands into registers\n");
                int reg1, reg2, regResult;

                if (isConstant(curr->arg1)) {
                    char tempName[32]; sprintf(tempName, "const_%s", curr->arg1);
                    reg1 = allocReg(tempName);
                    fprintf(output, "    li $t%d, %s         # Load constant %s\n", reg1, curr->arg1, curr->arg1);
                } else {
                    reg1 = findVarReg(curr->arg1);
                    if (reg1 == -1) {
                        int offset = lookupOffset(curr->arg1);
                        reg1 = allocReg(curr->arg1);
                        if (offset != -1)
                            fprintf(output, "    lw $t%d, %d($sp)     # Load variable '%s'\n", reg1, offset, curr->arg1);
                        else
                            fprintf(output, "    li $t%d, 0           # TAC temp not in memory\n", reg1);
                    }
                }

                if (isConstant(curr->arg2)) {
                    char tempName[32]; sprintf(tempName, "const_%s", curr->arg2);
                    reg2 = allocReg(tempName);
                    fprintf(output, "    li $t%d, %s         # Load constant %s\n", reg2, curr->arg2, curr->arg2);
                } else {
                    reg2 = findVarReg(curr->arg2);
                    if (reg2 == -1) {
                        int offset = lookupOffset(curr->arg2);
                        reg2 = allocReg(curr->arg2);
                        if (offset != -1)
                            fprintf(output, "    lw $t%d, %d($sp)     # Load variable '%s'\n", reg2, offset, curr->arg2);
                        else
                            fprintf(output, "    li $t%d, 0           # TAC temp not in memory\n", reg2);
                    }
                }

                regResult = allocReg(curr->result);
                printf("          → Executing: $t%d = $t%d * $t%d\n", regResult, reg1, reg2);
                fprintf(output, "    mul $t%d, $t%d, $t%d   # %s = %s * %s\n",
                        regResult, reg1, reg2, curr->result, curr->arg1, curr->arg2);

                if (!isTACTemp(curr->result)) {
                    regAlloc.regs[regResult].isDirty = 1;
                    int offset = lookupOffset(curr->result);
                    if (offset != -1) {
                        fprintf(output, "    sw $t%d, %d($sp)     # Store to '%s'\n", regResult, offset, curr->result);
                        regAlloc.regs[regResult].isDirty = 0;
                    }
                }
                freeReg(reg1); freeReg(reg2);
                printf("          → Result stored in $t%d\n\n", regResult);
                break;
            }

            case TAC_DIV: {
                // result = arg1 / arg2
                printf("[TAC %3d] DIV: %s = %s / %s\n", tacLineNum, curr->result, curr->arg1, curr->arg2);
                printf("          → Loading operands into registers\n");
                int reg1, reg2, regResult;

                if (isConstant(curr->arg1)) {
                    char tempName[32]; sprintf(tempName, "const_%s", curr->arg1);
                    reg1 = allocReg(tempName);
                    fprintf(output, "    li $t%d, %s         # Load constant %s\n", reg1, curr->arg1, curr->arg1);
                } else {
                    reg1 = findVarReg(curr->arg1);
                    if (reg1 == -1) {
                        int offset = lookupOffset(curr->arg1);
                        reg1 = allocReg(curr->arg1);
                        if (offset != -1)
                            fprintf(output, "    lw $t%d, %d($sp)     # Load variable '%s'\n", reg1, offset, curr->arg1);
                        else
                            fprintf(output, "    li $t%d, 0           # TAC temp not in memory\n", reg1);
                    }
                }

                if (isConstant(curr->arg2)) {
                    char tempName[32]; sprintf(tempName, "const_%s", curr->arg2);
                    reg2 = allocReg(tempName);
                    fprintf(output, "    li $t%d, %s         # Load constant %s\n", reg2, curr->arg2, curr->arg2);
                } else {
                    reg2 = findVarReg(curr->arg2);
                    if (reg2 == -1) {
                        int offset = lookupOffset(curr->arg2);
                        reg2 = allocReg(curr->arg2);
                        if (offset != -1)
                            fprintf(output, "    lw $t%d, %d($sp)     # Load variable '%s'\n", reg2, offset, curr->arg2);
                        else
                            fprintf(output, "    li $t%d, 0           # TAC temp not in memory\n", reg2);
                    }
                }

                regResult = allocReg(curr->result);
                printf("          → Executing: div $t%d, $t%d then mflo\n", reg1, reg2);
                fprintf(output, "    div $t%d, $t%d         # %s / %s\n", reg1, reg2, curr->arg1, curr->arg2);
                fprintf(output, "    mflo $t%d              # %s = quotient\n", regResult, curr->result);

                if (!isTACTemp(curr->result)) {
                    regAlloc.regs[regResult].isDirty = 1;
                    int offset = lookupOffset(curr->result);
                    if (offset != -1) {
                        fprintf(output, "    sw $t%d, %d($sp)     # Store to '%s'\n", regResult, offset, curr->result);
                        regAlloc.regs[regResult].isDirty = 0;
                    }
                }
                freeReg(reg1); freeReg(reg2);
                printf("          → Result stored in $t%d\n\n", regResult);
                break;
            }

            case TAC_MOD: {
                // result = arg1 % arg2 (modulo - uses MIPS div then mfhi for remainder)
                printf("[TAC %3d] MOD: %s = %s %% %s\n", tacLineNum, curr->result, curr->arg1, curr->arg2);
                printf("          → Loading operands into registers\n");
                int reg1, reg2, regResult;

                if (isConstant(curr->arg1)) {
                    char tempName[32]; sprintf(tempName, "const_%s", curr->arg1);
                    reg1 = allocReg(tempName);
                    fprintf(output, "    li $t%d, %s         # Load constant %s\n", reg1, curr->arg1, curr->arg1);
                } else {
                    reg1 = findVarReg(curr->arg1);
                    if (reg1 == -1) {
                        int offset = lookupOffset(curr->arg1);
                        reg1 = allocReg(curr->arg1);
                        if (offset != -1)
                            fprintf(output, "    lw $t%d, %d($sp)     # Load variable '%s'\n", reg1, offset, curr->arg1);
                        else
                            fprintf(output, "    li $t%d, 0           # TAC temp not in memory\n", reg1);
                    }
                }

                if (isConstant(curr->arg2)) {
                    char tempName[32]; sprintf(tempName, "const_%s", curr->arg2);
                    reg2 = allocReg(tempName);
                    fprintf(output, "    li $t%d, %s         # Load constant %s\n", reg2, curr->arg2, curr->arg2);
                } else {
                    reg2 = findVarReg(curr->arg2);
                    if (reg2 == -1) {
                        int offset = lookupOffset(curr->arg2);
                        reg2 = allocReg(curr->arg2);
                        if (offset != -1)
                            fprintf(output, "    lw $t%d, %d($sp)     # Load variable '%s'\n", reg2, offset, curr->arg2);
                        else
                            fprintf(output, "    li $t%d, 0           # TAC temp not in memory\n", reg2);
                    }
                }

                regResult = allocReg(curr->result);
                printf("          → Executing: div $t%d, $t%d then mfhi (remainder)\n", reg1, reg2);
                fprintf(output, "    div $t%d, $t%d         # %s %% %s\n", reg1, reg2, curr->arg1, curr->arg2);
                fprintf(output, "    mfhi $t%d              # %s = remainder\n", regResult, curr->result);

                if (!isTACTemp(curr->result)) {
                    regAlloc.regs[regResult].isDirty = 1;
                    int offset = lookupOffset(curr->result);
                    if (offset != -1) {
                        fprintf(output, "    sw $t%d, %d($sp)     # Store to '%s'\n", regResult, offset, curr->result);
                        regAlloc.regs[regResult].isDirty = 0;
                    }
                }
                freeReg(reg1); freeReg(reg2);
                printf("          → Result (remainder) stored in $t%d\n\n", regResult);
                break;
            }

            case TAC_LT:
            case TAC_GT:
            case TAC_LE:
            case TAC_GE:
            case TAC_EQ:
            case TAC_NE: {
                // result = arg1 <op> arg2  (produces 1 if true, 0 if false)
                const char* opName =
                    curr->op == TAC_LT ? "<" : curr->op == TAC_GT ? ">" :
                    curr->op == TAC_LE ? "<=" : curr->op == TAC_GE ? ">=" :
                    curr->op == TAC_EQ ? "==" : "!=";
                const char* mipsOp =
                    curr->op == TAC_LT ? "slt" : curr->op == TAC_GT ? "sgt" :
                    curr->op == TAC_LE ? "sle" : curr->op == TAC_GE ? "sge" :
                    curr->op == TAC_EQ ? "seq" : "sne";
                printf("[TAC %3d] CMP: %s = %s %s %s\n",
                       tacLineNum, curr->result, curr->arg1, opName, curr->arg2);
                printf("          → Loading operands into registers\n");
                int reg1, reg2, regResult;

                if (isConstant(curr->arg1)) {
                    char tempName[32]; sprintf(tempName, "const_%s", curr->arg1);
                    reg1 = allocReg(tempName);
                    fprintf(output, "    li $t%d, %s         # Load constant %s\n", reg1, curr->arg1, curr->arg1);
                } else {
                    reg1 = findVarReg(curr->arg1);
                    if (reg1 == -1) {
                        int offset = lookupOffset(curr->arg1);
                        reg1 = allocReg(curr->arg1);
                        if (offset != -1)
                            fprintf(output, "    lw $t%d, %d($sp)     # Load variable '%s'\n", reg1, offset, curr->arg1);
                        else
                            fprintf(output, "    li $t%d, 0           # TAC temp not in memory\n", reg1);
                    }
                }

                if (isConstant(curr->arg2)) {
                    char tempName[32]; sprintf(tempName, "const_%s", curr->arg2);
                    reg2 = allocReg(tempName);
                    fprintf(output, "    li $t%d, %s         # Load constant %s\n", reg2, curr->arg2, curr->arg2);
                } else {
                    reg2 = findVarReg(curr->arg2);
                    if (reg2 == -1) {
                        int offset = lookupOffset(curr->arg2);
                        reg2 = allocReg(curr->arg2);
                        if (offset != -1)
                            fprintf(output, "    lw $t%d, %d($sp)     # Load variable '%s'\n", reg2, offset, curr->arg2);
                        else
                            fprintf(output, "    li $t%d, 0           # TAC temp not in memory\n", reg2);
                    }
                }

                regResult = allocReg(curr->result);
                printf("          → Executing: $t%d = $t%d %s $t%d\n", regResult, reg1, opName, reg2);
                fprintf(output, "    %s $t%d, $t%d, $t%d   # %s = %s %s %s\n",
                        mipsOp, regResult, reg1, reg2,
                        curr->result, curr->arg1, opName, curr->arg2);

                if (!isTACTemp(curr->result)) {
                    regAlloc.regs[regResult].isDirty = 1;
                    int offset = lookupOffset(curr->result);
                    if (offset != -1) {
                        fprintf(output, "    sw $t%d, %d($sp)     # Store to '%s'\n", regResult, offset, curr->result);
                        regAlloc.regs[regResult].isDirty = 0;
                    }
                }
                freeReg(reg1); freeReg(reg2);
                printf("          → Result (1=true, 0=false) stored in $t%d\n\n", regResult);
                break;
            }

            case TAC_ASSIGN: {
                // result = arg1
                printf("[TAC %3d] ASSIGN: %s = %s\n", tacLineNum, curr->result, curr->arg1);
                int regSrc, regDest;
                int isTempResult = isTACTemp(curr->result);
                int offset = -1;

                // Get offset for real variables (not TAC temporaries)
                if (!isTempResult) {
                    offset = lookupOffset(curr->result);
                    if (offset == -1) {
                        fprintf(stderr, "Error: Variable %s not declared\n", curr->result);
                        exit(1);
                    }
                }

                // Load source value into register
                if (isConstant(curr->arg1)) {
                    // Source is a constant
                    regDest = allocReg(curr->result);
                    fprintf(output, "    li $t%d, %s         # %s = %s (constant)\n",
                            regDest, curr->arg1, curr->result, curr->arg1);
                } else {
                    // Source is a variable or TAC temporary
                    regSrc = findVarReg(curr->arg1);
                    if (regSrc == -1) {
                        int srcOffset = lookupOffset(curr->arg1);
                        regSrc = allocReg(curr->arg1);
                        if (srcOffset != -1)
                            fprintf(output, "    lw $t%d, %d($sp)     # Load variable '%s'\n",
                                    regSrc, srcOffset, curr->arg1);
                        else
                            fprintf(output, "    li $t%d, 0           # TAC temp not in memory\n", regSrc);
                    }

                    // For real variables, we might need a different register
                    if (!isTempResult) {
                        regDest = allocReg(curr->result);
                        if (regDest != regSrc) {
                            fprintf(output, "    move $t%d, $t%d       # %s = %s\n",
                                    regDest, regSrc, curr->result, curr->arg1);
                        }
                    } else {
                        // For TAC temporaries, reuse the source register
                        regDest = regSrc;
                        // Update register descriptor
                        strncpy(regAlloc.regs[regDest].varName, curr->result, MAX_VAR_NAME - 1);
                    }
                }

                // Store to memory only for real variables (not TAC temporaries)
                if (!isTempResult) {
                    fprintf(output, "    sw $t%d, %d($sp)     # Store to '%s'\n",
                            regDest, offset, curr->result);
                    // Update register descriptor
                    strncpy(regAlloc.regs[regDest].varName, curr->result, MAX_VAR_NAME - 1);
                    regAlloc.regs[regDest].isDirty = 0;  /* No longer dirty after store */
                }

                break;
            }

            case TAC_PRINT: {
                // PRINT arg1
                printf("[TAC %3d] PRINT: output %s\n", tacLineNum, curr->arg1);
                printf("          → Using syscall 1 (print integer)\n");
                int regPrint;

                // Load value to print
                if (isConstant(curr->arg1)) {
                    // Constant value
                    char tempName[32];
                    sprintf(tempName, "const_%s", curr->arg1);
                    regPrint = allocReg(tempName);
                    fprintf(output, "    li $t%d, %s         # Load constant %s for print\n",
                            regPrint, curr->arg1, curr->arg1);
                } else {
                    // Variable or TAC temporary
                    regPrint = findVarReg(curr->arg1);
                    if (regPrint == -1) {
                        int offset = lookupOffset(curr->arg1);
                        regPrint = allocReg(curr->arg1);
                        if (offset != -1)
                            fprintf(output, "    lw $t%d, %d($sp)     # Load variable '%s' for print\n",
                                    regPrint, offset, curr->arg1);
                        else
                            fprintf(output, "    li $t%d, 0           # TAC temp not in memory\n", regPrint);
                    }
                }

                // Print the value
                fprintf(output, "    # Print integer\n");
                fprintf(output, "    move $a0, $t%d\n", regPrint);
                fprintf(output, "    li $v0, 1\n");
                fprintf(output, "    syscall\n");

                // Print newline
                fprintf(output, "    # Print newline\n");
                fprintf(output, "    li $v0, 11\n");
                fprintf(output, "    li $a0, 10\n");
                fprintf(output, "    syscall\n");

                // Free register after print
                freeReg(regPrint);

                break;
            }

            case TAC_ARG: {
                // Collect arguments for the upcoming function call
                fprintf(output, "    # Argument: %s\n", curr->arg1);
                if (argCount < MAX_ARGS) {
                    argList[argCount] = strdup(curr->arg1);
                    argCount++;
                }
                break;
            }

            case TAC_CALL: {
                // Function call: result = CALL name, argCount
                printf("[TAC %3d] CALL: %s = call %s with %s arguments\n",
                       tacLineNum, curr->result, curr->arg1, curr->arg2);
                printf("          → Passing arguments in $a0-$a3\n");
                fprintf(output, "    # Call function %s with %s arguments\n", curr->arg1, curr->arg2);

                // Pass arguments: first 4 in $a0-$a3, extras pushed below $sp
                for (int i = 0; i < argCount; i++) {
                    char* arg = argList[i];
                    if (i < 4) {
                        // Standard: pass in $a0-$a3
                        if (isConstant(arg)) {
                            fprintf(output, "    li $a%d, %s\n", i, arg);
                        } else {
                            int argReg = findVarReg(arg);
                            if (argReg == -1) {
                                int offset = lookupOffset(arg);
                                if (offset != -1)
                                    fprintf(output, "    lw $a%d, %d($sp)     # Load argument '%s'\n",
                                            i, offset, arg);
                            } else {
                                fprintf(output, "    move $a%d, $t%d      # Pass argument '%s'\n",
                                        i, argReg, arg);
                            }
                        }
                    } else {
                        // Extra args: store below current $sp so callee can find them
                        int extraOffset = -(i - 3) * 4;
                        printf("          -> Arg %d ('%s') pushed to stack at %d($sp)\n", i, arg, extraOffset);
                        if (isConstant(arg)) {
                            fprintf(output, "    li $t9, %s              # Extra arg %d value\n", arg, i);
                        } else {
                            int argReg = findVarReg(arg);
                            if (argReg == -1) {
                                int offset = lookupOffset(arg);
                                if (offset != -1)
                                    fprintf(output, "    lw $t9, %d($sp)     # Load extra arg '%s'\n",
                                            offset, arg);
                            } else {
                                fprintf(output, "    move $t9, $t%d       # Extra arg '%s'\n", argReg, arg);
                            }
                        }
                        fprintf(output, "    sw $t9, %d($sp)       # Push extra arg '%s' to stack\n",
                                extraOffset, arg);
                    }
                    free(argList[i]);
                }
                argCount = 0;  // Reset for next call

                // Call the function with jal
                printf("          → Jumping to function func_%s\n", curr->arg1);
                if (strcmp(curr->arg1, "main") == 0)
                    fprintf(output, "    jal main_start\n");
                else
                    fprintf(output, "    jal func_%s\n", curr->arg1);

                // Retrieve return value from $v0 and store in result register
                int regResult = allocReg(curr->result);
                fprintf(output, "    move $t%d, $v0      # Get return value\n", regResult);
                printf("          → Return value stored in $t%d\n\n", regResult);
                break;
            }

            case TAC_RETURN: {
                // Return statement
                if (curr->arg1) {
                    // Load return value into register
                    int regReturn;
                    if (isConstant(curr->arg1)) {
                        char tempName[32];
                        sprintf(tempName, "const_%s", curr->arg1);
                        regReturn = allocReg(tempName);
                        fprintf(output, "    li $t%d, %s         # Load return value\n",
                                regReturn, curr->arg1);
                    } else {
                        regReturn = findVarReg(curr->arg1);
                        if (regReturn == -1) {
                            int offset = lookupOffset(curr->arg1);
                            regReturn = allocReg(curr->arg1);
                            if (offset != -1)
                                fprintf(output, "    lw $t%d, %d($sp)     # Load return value '%s'\n",
                                        regReturn, offset, curr->arg1);
                            else
                                fprintf(output, "    li $t%d, 0           # TAC temp not in memory\n", regReturn);
                        }
                    }
                    fprintf(output, "    move $v0, $t%d       # Move to return register\n", regReturn);
                    freeReg(regReturn);
                }
                fprintf(output, "    # Return from function\n");
                break;
            }

            case TAC_ARRAY_LOAD: {
                // result = array[index]
                printf("[TAC %3d] ARRAY_LOAD: %s = %s[%s]\n",
                       tacLineNum, curr->result, curr->arg1, curr->arg2);

                // Load or compute index
                int indexReg;
                if (isConstant(curr->arg2)) {
                    indexReg = allocReg("const_index");
                    fprintf(output, "    li $t%d, %s         # Load index\n", indexReg, curr->arg2);
                } else {
                    indexReg = findVarReg(curr->arg2);
                    if (indexReg == -1) {
                        int offset = lookupOffset(curr->arg2);
                        indexReg = allocReg(curr->arg2);
                        if (offset != -1)
                            fprintf(output, "    lw $t%d, %d($sp)     # Load index\n", indexReg, offset);
                        else
                            fprintf(output, "    li $t%d, 0           # TAC temp index not in memory\n", indexReg);
                    }
                }

                // Get array base address:
                // - For local arrays: sp + offset gives the base
                // - For array params: sp + offset holds a pointer value, must dereference
                int arrayOffset = lookupOffset(curr->arg1);
                int isArrayParam = isArray((char*)curr->arg1) == 0
                                   && arrayOffset != -1;
                // Check symtab to see if it's a real array or a scalar param holding a ptr
                int arrIsParam = 0;
                for (int si = 0; si < symtab.count; si++) {
                    if (symtab.vars[si].name &&
                        strcmp(symtab.vars[si].name, curr->arg1) == 0) {
                        arrIsParam = !symtab.vars[si].isArray;
                        break;
                    }
                }

                // Calculate element address: base + (index * 4)
                int offsetReg = allocReg("offset_temp");
                int addrReg = allocReg("temp_addr");
                fprintf(output, "    sll $t%d, $t%d, 2\n", offsetReg, indexReg);  // offset = index * 4
                if (arrIsParam) {
                    // Array param: load the pointer value, then add offset
                    fprintf(output, "    lw $t%d, %d($sp)    # Load array param pointer\n", addrReg, arrayOffset);
                } else {
                    fprintf(output, "    addi $t%d, $sp, %d\n", addrReg, arrayOffset); // addr = sp + base
                }
                fprintf(output, "    add $t%d, $t%d, $t%d\n", addrReg, addrReg, offsetReg); // addr = base + offset

                // Load value
                int resultReg = allocReg(curr->result);
                fprintf(output, "    lw $t%d, 0($t%d)\n", resultReg, addrReg);

                freeReg(offsetReg);

                freeReg(indexReg);
                freeReg(addrReg);
                break;
            }

            case TAC_ARRAY_STORE: {
                // array[index] = value
                printf("[TAC %3d] ARRAY_STORE: %s[%s] = %s\n",
                       tacLineNum, curr->arg1, curr->arg2, curr->result);

                // Get array base offset
                int arrayOffset = lookupOffset(curr->arg1);
                if (arrayOffset == -1) {
                    fprintf(stderr, "Error: Array '%s' not found in symbol table\n", curr->arg1);
                    exit(1);
                }

                // Load index
                int indexReg;
                if (isConstant(curr->arg2)) {
                    indexReg = allocReg("const_index");
                    fprintf(output, "    li $t%d, %s         # Load index\n", indexReg, curr->arg2);
                } else {
                    indexReg = findVarReg(curr->arg2);
                    if (indexReg == -1) {
                        int offset = lookupOffset(curr->arg2);
                        if (offset == -1) {
                            fprintf(stderr, "Error: Index variable '%s' not found\n", curr->arg2);
                            exit(1);
                        }
                        indexReg = allocReg(curr->arg2);
                        fprintf(output, "    lw $t%d, %d($sp)     # Load index\n", indexReg, offset);
                    }
                }

                // Load value
                int valueReg;
                if (isConstant(curr->result)) {
                    valueReg = allocReg("const_value");
                    fprintf(output, "    li $t%d, %s         # Load value\n", valueReg, curr->result);
                } else {
                    valueReg = findVarReg(curr->result);
                    if (valueReg == -1) {
                        int offset = lookupOffset(curr->result);
                        valueReg = allocReg(curr->result);
                        if (offset != -1)
                            fprintf(output, "    lw $t%d, %d($sp)     # Load value\n", valueReg, offset);
                        else
                            fprintf(output, "    li $t%d, 0           # TAC temp value not in memory\n", valueReg);
                    }
                }

                // Check if array param (scalar entry holding a pointer) vs real local array
                int storeIsParam = 0;
                for (int si = 0; si < symtab.count; si++) {
                    if (symtab.vars[si].name &&
                        strcmp(symtab.vars[si].name, curr->arg1) == 0) {
                        storeIsParam = !symtab.vars[si].isArray;
                        break;
                    }
                }

                // Calculate element address: base + (index * 4)
                int offsetReg = allocReg("offset_temp");
                int addrReg = allocReg("temp_addr");
                fprintf(output, "    sll $t%d, $t%d, 2     # offset = index * 4\n", offsetReg, indexReg);
                if (storeIsParam) {
                    // Array param: dereference the pointer stored at arrayOffset
                    fprintf(output, "    lw $t%d, %d($sp)    # Load array param pointer\n", addrReg, arrayOffset);
                } else {
                    fprintf(output, "    addi $t%d, $sp, %d    # addr = sp + base\n", addrReg, arrayOffset);
                }
                fprintf(output, "    add $t%d, $t%d, $t%d  # addr = base + offset\n", addrReg, addrReg, offsetReg);

                // Store value
                fprintf(output, "    sw $t%d, 0($t%d)      # Store to array element\n", valueReg, addrReg);

                printf("          → Stored value into %s[%s]\n\n", curr->arg1, curr->arg2);

                freeReg(offsetReg);
                freeReg(indexReg);
                freeReg(valueReg);
                freeReg(addrReg);
                break;
            }

            default:
                break;
        }

        curr = curr->next;
        tacLineNum++;
    }

    // Spill any remaining dirty registers
    fprintf(output, "\n    # Spill any remaining registers\n");
    for (int i = 0; i < NUM_TEMP_REGS; i++) {
        if (regAlloc.regs[i].inUse && regAlloc.regs[i].isDirty) {
            spillReg(i);
        }
    }

    // Program exit
    fprintf(output, "\n    # Exit program\n");
    fprintf(output, "    addi $sp, $sp, 400\n");
    fprintf(output, "    li $v0, 10\n");
    fprintf(output, "    syscall\n");

    fclose(output);

    // Print register allocation statistics
    printRegAllocStats();
}
