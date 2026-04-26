/* SEMANTIC ANALYSIS IMPLEMENTATION - WITH FUNCTION SUPPORT
 * Performs semantic checks on the Abstract Syntax Tree
 * Now supports functions, scopes, parameters, control flow
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "semantic.h"
#include "symtab.h"

#define MAX_FUNCTIONS 100
#define MAX_PARAMS 20
#define MAX_SCOPE_DEPTH 10

/* Function symbol table entry */
typedef struct {
    char* name;
    int paramCount;
    char* params[MAX_PARAMS];
    int paramIsArray[MAX_PARAMS];  /* Track which params are arrays */
    int isDefined;
} FunctionSymbol;

/* Scope for variables */
typedef struct {
    char* names[MAX_VARS];
    int count;
} Scope;

/* Global semantic information */
static SemanticInfo semInfo;
static FunctionSymbol functions[MAX_FUNCTIONS];
static int functionCount = 0;
static Scope scopes[MAX_SCOPE_DEPTH];
static int scopeDepth = 0;
static char* currentFunction = NULL;
static int inFunction = 0;

/* Initialize semantic analyzer */
void initSemantic() {
    semInfo.errorCount = 0;
    semInfo.warningCount = 0;
    functionCount = 0;
    scopeDepth = 0;
    currentFunction = NULL;
    inFunction = 0;

    /* Add built-in functions */
    functions[functionCount].name = strdup("print");
    functions[functionCount].paramCount = 1;
    functions[functionCount].isDefined = 1;  /* Built-in */
    functionCount++;

    printf("SEMANTIC ANALYZER: Initialized with function support\n\n");
}

/* Scope management */
static void enterScope() {
    if (scopeDepth >= MAX_SCOPE_DEPTH) {
        fprintf(stderr, "SEMANTIC ERROR: Maximum scope depth exceeded\n");
        semInfo.errorCount++;
        return;
    }
    scopes[scopeDepth].count = 0;
    scopeDepth++;
}

static void exitScope() {
    if (scopeDepth > 0) {
        /* Free variable names in this scope */
        for (int i = 0; i < scopes[scopeDepth - 1].count; i++) {
            free(scopes[scopeDepth - 1].names[i]);
        }
        scopeDepth--;
    }
}

/* Print current semantic scopes for debugging */
static void printSemanticScopes() {
    printf("\n┌─────────────────────────────────────────────────────────┐\n");
    printf("│ SEMANTIC SCOPE STACK (Depth: %d)                        \n", scopeDepth);
    printf("├─────────────────────────────────────────────────────────┤\n");

    if (scopeDepth == 0) {
        printf("│ (no active scopes)                                      │\n");
    } else {
        for (int depth = 0; depth < scopeDepth; depth++) {
            if (depth == 0) {
                printf("│ Scope[%d] GLOBAL (%d variables)                        \n", depth, scopes[depth].count);
            } else {
                if (inFunction && depth == scopeDepth - 1) {
                    printf("│ Scope[%d] Function '%s' (%d variables)              \n",
                           depth, currentFunction ? currentFunction : "unknown", scopes[depth].count);
                } else {
                    printf("│ Scope[%d] LOCAL (%d variables)                         \n", depth, scopes[depth].count);
                }
            }

            if (scopes[depth].count > 0) {
                printf("│   Variables: ");
                for (int i = 0; i < scopes[depth].count; i++) {
                    printf("%s", scopes[depth].names[i]);
                    if (i < scopes[depth].count - 1) printf(", ");
                }
                printf("\n");
            } else {
                printf("│   (empty)\n");
            }
        }
    }
    printf("└─────────────────────────────────────────────────────────┘\n\n");
}

/* Add variable to current scope */
static int addVarToScope(char* name) {
    if (scopeDepth == 0) {
        fprintf(stderr, "SEMANTIC ERROR: No scope to add variable to\n");
        return -1;
    }

    Scope* currentScope = &scopes[scopeDepth - 1];

    /* Check if already declared in current scope */
    for (int i = 0; i < currentScope->count; i++) {
        if (strcmp(currentScope->names[i], name) == 0) {
            return -1;  /* Already declared in this scope */
        }
    }

    /* Add to current scope */
    if (currentScope->count >= MAX_VARS) {
        fprintf(stderr, "SEMANTIC ERROR: Too many variables in scope\n");
        return -1;
    }

    currentScope->names[currentScope->count] = strdup(name);
    currentScope->count++;
    return 0;
}

/* Add array to current scope */
static int addArrayToScope(char* name, int size) {
    if (scopeDepth == 0) {
        fprintf(stderr, "SEMANTIC ERROR: No scope to add array to\n");
        return -1;
    }

    Scope* currentScope = &scopes[scopeDepth - 1];

    /* Check if already declared in current scope */
    for (int i = 0; i < currentScope->count; i++) {
        if (strcmp(currentScope->names[i], name) == 0) {
            return -1;  /* Already declared in this scope */
        }
    }

    /* Add to current scope */
    if (currentScope->count >= MAX_VARS) {
        fprintf(stderr, "SEMANTIC ERROR: Too many variables in scope\n");
        return -1;
    }

    currentScope->names[currentScope->count] = strdup(name);
    currentScope->count++;
    return 0;
}

/* Check if variable is declared in any visible scope */
static int isVarDeclaredInScope(char* name) {
    /* Search from innermost to outermost scope */
    for (int depth = scopeDepth - 1; depth >= 0; depth--) {
        for (int i = 0; i < scopes[depth].count; i++) {
            if (strcmp(scopes[depth].names[i], name) == 0) {
                return 1;
            }
        }
    }
    return 0;
}

/* Function management */
static FunctionSymbol* findFunction(char* name) {
    for (int i = 0; i < functionCount; i++) {
        if (strcmp(functions[i].name, name) == 0) {
            return &functions[i];
        }
    }
    return NULL;
}

static int addFunction(char* name, int paramCount, char** params, int* paramIsArray) {
    if (functionCount >= MAX_FUNCTIONS) {
        fprintf(stderr, "SEMANTIC ERROR: Too many functions\n");
        return -1;
    }

    /* Check if function already exists */
    if (findFunction(name)) {
        fprintf(stderr, "\n╔════════════════════════════════════════════════════════════╗\n");
        fprintf(stderr, "║ SEMANTIC ERROR - Duplicate Function Definition            ║\n");
        fprintf(stderr, "╚════════════════════════════════════════════════════════════╝\n");
        fprintf(stderr, "  📍 Location: Function '%s'\n", name);
        fprintf(stderr, "  ❌ Error: This function is already defined elsewhere in the program\n");
        fprintf(stderr, "  💡 Suggestion: Each function can only be defined once\n");
        fprintf(stderr, "     → Option 1: Remove one of the duplicate definitions\n");
        fprintf(stderr, "     → Option 2: Rename one function: %s2(), my%s(), etc.\n", name, name);
        fprintf(stderr, "  📖 Note: If you want to use the function in multiple places,\n");
        fprintf(stderr, "     define it once and call it multiple times\n\n");
        semInfo.errorCount++;
        return -1;
    }

    functions[functionCount].name = strdup(name);
    functions[functionCount].paramCount = paramCount;
    for (int i = 0; i < paramCount; i++) {
        functions[functionCount].params[i] = strdup(params[i]);
        functions[functionCount].paramIsArray[i] = paramIsArray ? paramIsArray[i] : 0;
    }
    functions[functionCount].isDefined = 1;
    functionCount++;

    printf("  ✓ Function '%s' defined with %d parameter(s)\n", name, paramCount);
    return 0;
}

/* Count parameters in parameter list */
static int countParams(ASTNode* params, char** paramNames, int* paramIsArray) {
    if (!params) return 0;

    int count = 0;
    ASTNode* current = params;

    while (current && count < MAX_PARAMS) {
        if (current->type == NODE_PARAM) {
            paramNames[count] = current->data.param.name;
            paramIsArray[count] = 0;
            count++;
            break;
        } else if (current->type == NODE_ARRAY_DECL && current->data.array_decl.isParam) {
            paramNames[count] = current->data.array_decl.name;
            paramIsArray[count] = 1;
            count++;
            break;
        } else if (current->type == NODE_PARAM_LIST) {
            if (current->data.param_list.param) {
                if (current->data.param_list.param->type == NODE_PARAM) {
                    paramNames[count] = current->data.param_list.param->data.param.name;
                    paramIsArray[count] = 0;
                    count++;
                } else if (current->data.param_list.param->type == NODE_ARRAY_DECL) {
                    paramNames[count] = current->data.param_list.param->data.array_decl.name;
                    paramIsArray[count] = 1;
                    count++;
                } else if (current->data.param_list.param->type == NODE_PARAM_LIST) {
                    // Recursively process nested param list
                    int nestedCount = countParams(current->data.param_list.param,
                                                  &paramNames[count],
                                                  &paramIsArray[count]);
                    count += nestedCount;
                }
            }
            current = current->data.param_list.next;
        } else {
            break;
        }
    }

    return count;
}

/* Count arguments in argument list */
static int countArgs(ASTNode* args) {
    if (!args) return 0;

    if (args->type == NODE_ARG_LIST) {
        // Check if expr is also an ARG_LIST (nested)
        if (args->data.arg_list.expr && args->data.arg_list.expr->type == NODE_ARG_LIST) {
            // Nested ARG_LIST - recursively process
            return countArgs(args->data.arg_list.expr) + countArgs(args->data.arg_list.next);
        } else {
            // expr is actual expression, count it + rest
            return 1 + countArgs(args->data.arg_list.next);
        }
    } else {
        // Single argument (expression)
        return 1;
    }
}

/* Forward declarations */
static void checkExpr(ASTNode* node);
static void checkStmt(ASTNode* node);
static void checkStmtList(ASTNode* node);
static int addArrayToScope(char* name, int size);

/* Check expression for semantic correctness */
static void checkExpr(ASTNode* node) {
    if (!node) return;

    switch(node->type) {
        case NODE_NUM:
            /* Literal numbers are always valid */
            break;

        case NODE_VAR:
            if (!isVarDeclaredInScope(node->data.name)) {
                fprintf(stderr, "\n╔════════════════════════════════════════════════════════════╗\n");
                fprintf(stderr, "║ SEMANTIC ERROR - Undeclared Variable                      ║\n");
                fprintf(stderr, "╚════════════════════════════════════════════════════════════╝\n");
                fprintf(stderr, "  📍 Location: Line %d\n", node->lineno);
                fprintf(stderr, "  ❌ Error: Variable '%s' is used before being declared\n", node->data.name);
                fprintf(stderr, "  💡 Suggestion: Add a declaration before using this variable:\n");
                fprintf(stderr, "     → int %s;  (add this before line %d)\n", node->data.name, node->lineno);
                fprintf(stderr, "  📖 Note: In C-minus, all variables must be declared before use\n\n");
                semInfo.errorCount++;
            }
            break;

        case NODE_BINOP:
            checkExpr(node->data.binop.left);
            if (node->data.binop.right) {  /* Unary minus has no right operand */
                checkExpr(node->data.binop.right);
            }
            break;

        case NODE_FUNC_CALL: {
            FunctionSymbol* func = findFunction(node->data.func_call.name);
            if (!func) {
                fprintf(stderr, "\n╔════════════════════════════════════════════════════════════╗\n");
                fprintf(stderr, "║ SEMANTIC ERROR - Undeclared Function                      ║\n");
                fprintf(stderr, "╚════════════════════════════════════════════════════════════╝\n");
                fprintf(stderr, "  📍 Location: Line %d\n", node->lineno);
                fprintf(stderr, "  ❌ Error: Function '%s()' is called but not declared\n", node->data.func_call.name);
                fprintf(stderr, "  💡 Suggestion: Make sure the function is defined before calling it\n");
                fprintf(stderr, "  📖 Example function definition:\n");
                fprintf(stderr, "     int %s(...params...) {\n", node->data.func_call.name);
                fprintf(stderr, "         // function body\n");
                fprintf(stderr, "         return value;\n");
                fprintf(stderr, "     }\n\n");
                semInfo.errorCount++;
            } else {
                int argCount = countArgs(node->data.func_call.args);
                if (argCount != func->paramCount) {
                    fprintf(stderr, "\n╔════════════════════════════════════════════════════════════╗\n");
                    fprintf(stderr, "║ SEMANTIC ERROR - Incorrect Argument Count                 ║\n");
                    fprintf(stderr, "╚════════════════════════════════════════════════════════════╝\n");
                    fprintf(stderr, "  📍 Location: Line %d\n", node->lineno);
                    fprintf(stderr, "  ❌ Error: Function '%s()' expects %d argument(s), but got %d\n",
                            node->data.func_call.name, func->paramCount, argCount);
                    fprintf(stderr, "  💡 Suggestion: Check the function definition and match the parameters:\n");
                    fprintf(stderr, "     → Function defined with %d parameter(s)\n", func->paramCount);
                    fprintf(stderr, "     → You provided %d argument(s) in the call\n", argCount);
                    if (argCount < func->paramCount) {
                        fprintf(stderr, "     → Add %d more argument(s) to the function call\n",
                                func->paramCount - argCount);
                    } else {
                        fprintf(stderr, "     → Remove %d argument(s) from the function call\n",
                                argCount - func->paramCount);
                    }
                    fprintf(stderr, "\n");
                    semInfo.errorCount++;
                } else {
                    printf("  ✓ Function call '%s' has correct argument count\n",
                           node->data.func_call.name);
                }

                /* Check argument expressions */
                ASTNode* arg = node->data.func_call.args;
                while (arg) {
                    if (arg->type == NODE_ARG_LIST) {
                        checkExpr(arg->data.arg_list.expr);
                        arg = arg->data.arg_list.next;
                    } else {
                        checkExpr(arg);
                        break;
                    }
                }
            }
            break;
        }

        case NODE_ARRAY_INDEX:
            /* Check if array is declared */
            if (!isVarDeclaredInScope(node->data.array_index.name)) {
                fprintf(stderr, "\n╔════════════════════════════════════════════════════════════╗\n");
                fprintf(stderr, "║ SEMANTIC ERROR - Undeclared Array                         ║\n");
                fprintf(stderr, "╚════════════════════════════════════════════════════════════╝\n");
                fprintf(stderr, "  📍 Location: Line %d\n", node->lineno);
                fprintf(stderr, "  ❌ Error: Array '%s' is used before being declared\n", node->data.array_index.name);
                fprintf(stderr, "  💡 Suggestion: Declare the array before using it:\n");
                fprintf(stderr, "     → int %s[SIZE];  (replace SIZE with the desired array length)\n", node->data.array_index.name);
                fprintf(stderr, "  📖 Example: int %s[10];  // Declares an array of 10 integers\n\n", node->data.array_index.name);
                semInfo.errorCount++;
            }
            /* Check index expression */
            checkExpr(node->data.array_index.index);
            break;

        case NODE_AND:
            printf("    → Checking && (logical AND) operands\n");
            checkExpr(node->data.binop.left);
            checkExpr(node->data.binop.right);
            break;
        case NODE_OR:
            printf("    → Checking || (logical OR) operands\n");
            checkExpr(node->data.binop.left);
            checkExpr(node->data.binop.right);
            break;
        case NODE_NOT:
            printf("    → Checking ! (logical NOT) operand\n");
            checkExpr(node->data.not_expr);
            break;
        default:
            break;
    }
}

/* Check statement */
static void checkStmt(ASTNode* node) {
    if (!node) return;

    switch(node->type) {
        case NODE_DECL:
            if (addVarToScope(node->data.name) == -1) {
                fprintf(stderr, "\n╔════════════════════════════════════════════════════════════╗\n");
                fprintf(stderr, "║ SEMANTIC ERROR - Duplicate Variable Declaration           ║\n");
                fprintf(stderr, "╚════════════════════════════════════════════════════════════╝\n");
                fprintf(stderr, "  📍 Location: Line %d\n", node->lineno);
                fprintf(stderr, "  ❌ Error: Variable '%s' is already declared in the current scope\n", node->data.name);
                fprintf(stderr, "  💡 Suggestion: Choose a different variable name or remove the duplicate declaration\n");
                fprintf(stderr, "  📖 Note: Each variable can only be declared once per scope\n");
                fprintf(stderr, "     → Use assignment (=) to change the value of an existing variable\n");
                fprintf(stderr, "     → Or use a different name: %s2, my%s, etc.\n\n", node->data.name, node->data.name);
                semInfo.errorCount++;
            } else {
                printf("  ✓ Variable '%s' declared (line %d)\n", node->data.name, node->lineno);
            }
            break;

        case NODE_ASSIGN:
            if (node->data.assign.arrayLHS) {
                /* Array element assignment: arr[i] = expr */
                checkExpr(node->data.assign.arrayLHS);
                printf("  ✓ Array element assignment is valid (line %d)\n", node->lineno);
            } else {
                /* Scalar assignment: x = expr */
                if (!isVarDeclaredInScope(node->data.assign.var)) {
                    fprintf(stderr, "\n╔════════════════════════════════════════════════════════════╗\n");
                    fprintf(stderr, "║ SEMANTIC ERROR - Assignment to Undeclared Variable        ║\n");
                    fprintf(stderr, "╚════════════════════════════════════════════════════════════╝\n");
                    fprintf(stderr, "  📍 Location: Line %d\n", node->lineno);
                    fprintf(stderr, "  ❌ Error: Trying to assign to variable '%s' which hasn't been declared\n", node->data.assign.var);
                    fprintf(stderr, "  💡 Suggestion: Declare the variable before assigning to it:\n");
                    fprintf(stderr, "     → Add this before line %d:\n", node->lineno);
                    fprintf(stderr, "       int %s;\n", node->data.assign.var);
                    fprintf(stderr, "  📖 Remember: Variables must be declared before use in C-minus\n\n");
                    semInfo.errorCount++;
                } else {
                    printf("  ✓ Assignment to '%s' is valid (line %d)\n", node->data.assign.var, node->lineno);
                }
            }
            checkExpr(node->data.assign.value);
            break;

        case NODE_PRINT:
            checkExpr(node->data.expr);
            printf("  ✓ Print statement is valid\n");
            break;

        case NODE_RETURN:
            if (!inFunction) {
                fprintf(stderr, "\n╔════════════════════════════════════════════════════════════╗\n");
                fprintf(stderr, "║ SEMANTIC ERROR - Misplaced Return Statement               ║\n");
                fprintf(stderr, "╚════════════════════════════════════════════════════════════╝\n");
                fprintf(stderr, "  📍 Location: Line %d\n", node->lineno);
                fprintf(stderr, "  ❌ Error: Return statement found outside of a function\n");
                fprintf(stderr, "  💡 Suggestion: Return statements can only appear inside functions\n");
                fprintf(stderr, "     → Move this return statement inside a function body\n");
                fprintf(stderr, "     → Or remove it if it's not needed\n");
                fprintf(stderr, "  📖 Example of correct usage:\n");
                fprintf(stderr, "     int myFunction() {\n");
                fprintf(stderr, "         return 42;  ✓  (inside function - OK)\n");
                fprintf(stderr, "     }\n\n");
                semInfo.errorCount++;
            } else {
                printf("  ✓ Return statement in function '%s'\n", currentFunction);
                checkExpr(node->data.ret.expr);
            }
            break;

        case NODE_IF:
            printf("  ✓ Checking if statement\n");
            checkExpr(node->data.if_stmt.condition);
            checkStmt(node->data.if_stmt.then_stmt);
            if (node->data.if_stmt.else_stmt) {
                checkStmt(node->data.if_stmt.else_stmt);
            }
            break;

        case NODE_WHILE:
            printf("  ✓ Checking while loop\n");
            checkExpr(node->data.while_stmt.condition);
            checkStmt(node->data.while_stmt.body);
            break;

        case NODE_SWITCH: {
            printf("  ✓ Checking switch statement (line %d)\n", node->lineno);
            printf("    → Checking switch expression\n");
            checkExpr(node->data.switch_stmt.expr);
            ASTNode* c = node->data.switch_stmt.cases;
            int caseNum = 0;
            while (c) {
                if (c->data.case_clause.isDefault)
                    printf("    → Checking default case\n");
                else
                    printf("    → Checking case %d\n", c->data.case_clause.value);
                if (c->data.case_clause.body)
                    checkStmt(c->data.case_clause.body);
                c = c->data.case_clause.next;
                caseNum++;
            }
            printf("    → Switch has %d case(s)\n", caseNum);
            break;
        }

        case NODE_FOR: {
            /*
             * Semantic checks for for loop:
             * - init assignment: check variable is declared, check expr
             * - condition: must be a valid expression
             * - update assignment: check variable is declared, check expr
             * - body: check all statements
             */
            printf("  ✓ Checking for loop (line %d)\n", node->lineno);
            if (node->data.for_stmt.init) {
                printf("    → Checking for-loop init assignment\n");
                checkStmt(node->data.for_stmt.init);
            }
            printf("    → Checking for-loop condition\n");
            checkExpr(node->data.for_stmt.condition);
            if (node->data.for_stmt.update) {
                printf("    → Checking for-loop update assignment\n");
                checkStmt(node->data.for_stmt.update);
            }
            printf("    → Checking for-loop body\n");
            checkStmt(node->data.for_stmt.body);
            break;
        }

        case NODE_DO_WHILE:
            /*
             * Semantic checks for do-while:
             * - body runs first (at least once), check all statements
             * - condition must be a valid expression
             */
            printf("  ✓ Checking do-while loop (line %d)\n", node->lineno);
            printf("    → Checking do-while body\n");
            checkStmt(node->data.do_while_stmt.body);
            printf("    → Checking do-while condition\n");
            checkExpr(node->data.do_while_stmt.condition);
            break;

        case NODE_BLOCK:
            enterScope();
            checkStmtList(node->data.block.stmt_list);
            exitScope();
            break;

        case NODE_FUNC_CALL:
            /* Function call as statement */
            checkExpr(node);
            break;

        case NODE_STMT_LIST:
            checkStmtList(node);
            break;

        case NODE_ARRAY_DECL:
            if (node->data.array_decl.isParam) {
                /* Array parameter - don't check size */
                if (addArrayToScope(node->data.array_decl.name, 0) == -1) {
                    fprintf(stderr, "  ✗ SEMANTIC ERROR (line %d): Array parameter '%s' already declared\n",
                            node->lineno, node->data.array_decl.name);
                    semInfo.errorCount++;
                } else {
                    printf("  ✓ Array parameter '%s[]' declared (line %d)\n",
                           node->data.array_decl.name, node->lineno);
                }
            } else {
                /* Array declaration - check size is positive */
                if (node->data.array_decl.size <= 0) {
                    fprintf(stderr, "\n╔════════════════════════════════════════════════════════════╗\n");
                    fprintf(stderr, "║ SEMANTIC ERROR - Invalid Array Size                       ║\n");
                    fprintf(stderr, "╚════════════════════════════════════════════════════════════╝\n");
                    fprintf(stderr, "  📍 Location: Line %d\n", node->lineno);
                    fprintf(stderr, "  ❌ Error: Array '%s' declared with invalid size: %d\n",
                            node->data.array_decl.name, node->data.array_decl.size);
                    fprintf(stderr, "  💡 Suggestion: Array size must be a positive integer (> 0)\n");
                    fprintf(stderr, "     → Change: int %s[%d];  ✗\n", node->data.array_decl.name, node->data.array_decl.size);
                    fprintf(stderr, "     → To:     int %s[10]; ✓  (or any positive number)\n", node->data.array_decl.name);
                    fprintf(stderr, "  📖 Common mistake: Using 0 or negative numbers for array size\n\n");
                    semInfo.errorCount++;
                } else if (addArrayToScope(node->data.array_decl.name, node->data.array_decl.size) == -1) {
                    fprintf(stderr, "\n╔════════════════════════════════════════════════════════════╗\n");
                    fprintf(stderr, "║ SEMANTIC ERROR - Duplicate Array Declaration              ║\n");
                    fprintf(stderr, "╚════════════════════════════════════════════════════════════╝\n");
                    fprintf(stderr, "  📍 Location: Line %d\n", node->lineno);
                    fprintf(stderr, "  ❌ Error: Array '%s' is already declared in this scope\n", node->data.array_decl.name);
                    fprintf(stderr, "  💡 Suggestion: Remove the duplicate declaration or use a different name\n");
                    fprintf(stderr, "     → Option 1: Delete this line if it's redundant\n");
                    fprintf(stderr, "     → Option 2: Use a different name: %s2, my%s, etc.\n",
                            node->data.array_decl.name, node->data.array_decl.name);
                    fprintf(stderr, "  📖 Note: Each array can only be declared once per scope\n\n");
                    semInfo.errorCount++;
                } else {
                    printf("  ✓ Array '%s[%d]' declared (line %d)\n",
                           node->data.array_decl.name, node->data.array_decl.size, node->lineno);
                }
            }
            break;

        default:
            break;
    }
}

/* Check statement list */
static void checkStmtList(ASTNode* node) {
    if (!node) return;

    if (node->type == NODE_STMT_LIST) {
        checkStmt(node->data.stmtlist.stmt);
        checkStmtList(node->data.stmtlist.next);
    } else {
        checkStmt(node);
    }
}

/* Recursively add parameters to function scope */
static void addParamsToScope(ASTNode* param) {
    if (!param) return;

    if (param->type == NODE_PARAM) {
        addVarToScope(param->data.param.name);
        printf("  ✓ Parameter '%s' added to function scope\n", param->data.param.name);
    } else if (param->type == NODE_ARRAY_DECL && param->data.array_decl.isParam) {
        addArrayToScope(param->data.array_decl.name, 0);
        printf("  ✓ Array parameter '%s[]' added to function scope\n",
               param->data.array_decl.name);
    } else if (param->type == NODE_PARAM_LIST) {
        // Recursively process nested param list
        addParamsToScope(param->data.param_list.param);
        addParamsToScope(param->data.param_list.next);
    }
}

/* Check function definition */
static void checkFuncDef(ASTNode* node) {
    if (!node || node->type != NODE_FUNC_DEF) return;

    /* Function should already be registered from first pass */
    /* Enter function scope */
    currentFunction = node->data.func_def.name;
    inFunction = 1;
    enterScope();
    printf("  Entered function '%s' scope\n", currentFunction);
    printSemanticScopes();

    /* Add all parameters to scope */
    int paramAdded = 0;
    if (node->data.func_def.params) {
        addParamsToScope(node->data.func_def.params);
        paramAdded = 1;
    }

    if (paramAdded) {
        printSemanticScopes();
    }

    /* Check function body */
    checkStmt(node->data.func_def.body);

    /* Exit function scope */
    printf("  Exiting function '%s' scope\n", currentFunction);
    exitScope();
    printSemanticScopes();
    inFunction = 0;
    currentFunction = NULL;
}

/* Register function (first pass) */
static void registerFunction(ASTNode* node) {
    if (!node || node->type != NODE_FUNC_DEF) return;

    char* paramNames[MAX_PARAMS];
    int paramIsArray[MAX_PARAMS];
    int paramCount = countParams(node->data.func_def.params, paramNames, paramIsArray);
    addFunction(node->data.func_def.name, paramCount, paramNames, paramIsArray);
}

/* Helper to recursively register all functions in AST */
static void registerFunctions(ASTNode* node) {
    if (!node) return;
    if (node->type == NODE_STMT_LIST) {
        registerFunctions(node->data.stmtlist.stmt);
        registerFunctions(node->data.stmtlist.next);
    } else if (node->type == NODE_FUNC_DEF) {
        registerFunction(node);
    }
}

/* Helper to recursively check all functions/statements in AST */
static void checkFunctions(ASTNode* node);  /* Forward declaration */

static void checkFunctions(ASTNode* node) {
    if (!node) return;
    if (node->type == NODE_STMT_LIST) {
        checkFunctions(node->data.stmtlist.stmt);
        checkFunctions(node->data.stmtlist.next);
    } else if (node->type == NODE_FUNC_DEF) {
        printf("─── Checking function: %s ───\n", node->data.func_def.name);
        checkFuncDef(node);
        printf("\n");
    } else if (node->type == NODE_DECL) {
        extern SemanticInfo semInfo;  /* Access global */
        if (addVarToScope(node->data.name) == -1) {
            fprintf(stderr, "  ✗ SEMANTIC ERROR: Global variable '%s' already declared\n",
                    node->data.name);
            semInfo.errorCount++;
        } else {
            printf("  ✓ Global variable '%s' declared\n", node->data.name);
        }
    } else {
        checkStmt(node);
    }
}

/* Perform complete semantic analysis */
int performSemanticAnalysis(ASTNode* root) {
    if (!root) {
        fprintf(stderr, "SEMANTIC ERROR: No AST to analyze\n");
        return -1;
    }

    printf("Running semantic analysis with function support...\n\n");

    /* Enter global scope */
    enterScope();
    printf("Entered global scope\n");
    printSemanticScopes();

    /* FIRST PASS: Register all function signatures */
    printf("Pass 1: Registering all functions\n");
    printf("───────────────────────────────\n");
    registerFunctions(root);
    printf("\n");

    /* SECOND PASS: Check function bodies and other statements */
    printf("Pass 2: Checking function bodies\n");
    printf("─────────────────────────────────\n");
    checkFunctions(root);

    /* Exit global scope */
    exitScope();

    return semInfo.errorCount > 0 ? -1 : 0;
}

/* Print semantic analysis summary */
void printSemanticSummary() {
    printf("═══════════════════════════════════════════\n");
    printf("SEMANTIC ANALYSIS SUMMARY\n");
    printf("═══════════════════════════════════════════\n");
    printf("Functions defined:  %d\n", functionCount);
    printf("Errors found:       %d\n", semInfo.errorCount);
    printf("Warnings found:     %d\n", semInfo.warningCount);
    printf("\n");

    if (semInfo.errorCount == 0) {
        printf("✓ Semantic analysis passed - program is semantically correct!\n");
    } else {
        printf("✗ Semantic analysis failed - fix errors before proceeding\n");
    }
    printf("═══════════════════════════════════════════\n\n");
}
