/* SYMBOL TABLE IMPLEMENTATION
 * Manages variable declarations and lookups
 * Essential for semantic analysis (checking if variables are declared)
 * Provides memory layout information for code generation
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "symtab.h"

/* Global symbol table instance */
SymbolTable symtab;

/* Initialize an empty symbol table */
void initSymTab() {
    symtab.count = 0;       /* No variables yet */
    symtab.nextOffset = 0;  /* Start at stack offset 0 */
    printf("SYMBOL TABLE: Initialized (empty, starting at offset 0)\n");
}

/* Add a new variable to the symbol table */
int addVar(char* name) {
    /* Check for duplicate declaration */
    if (isVarDeclared(name)) {
        printf("SYMBOL TABLE: ✗ Failed to add '%s' - already declared\n", name);
        return -1;  /* Error: variable already exists */
    }

    /* Add new symbol entry */
    symtab.vars[symtab.count].name = strdup(name);
    symtab.vars[symtab.count].offset = symtab.nextOffset;
    symtab.vars[symtab.count].isArray = 0;     /* Scalar variable */
    symtab.vars[symtab.count].arraySize = 0;   /* Not an array */

    /* Advance offset by 4 bytes (size of int in MIPS) */
    symtab.nextOffset += 4;
    symtab.count++;

    printf("SYMBOL TABLE: ✓ Added '%s' at stack offset %d (4 bytes)\n",
           name, symtab.vars[symtab.count - 1].offset);

    /* Return the offset for this variable */
    return symtab.vars[symtab.count - 1].offset;
}

/* Add a new array to the symbol table */
int addArray(char* name, int size) {
    /* Check for duplicate declaration */
    if (isVarDeclared(name)) {
        printf("SYMBOL TABLE: ✗ Failed to add array '%s' - already declared\n", name);
        return -1;
    }

    /* Add new symbol entry */
    symtab.vars[symtab.count].name = strdup(name);
    symtab.vars[symtab.count].offset = symtab.nextOffset;
    symtab.vars[symtab.count].isArray = 1;      /* This is an array */
    symtab.vars[symtab.count].arraySize = size; /* Store element count */

    /* Allocate size * 4 bytes (each int is 4 bytes) */
    int totalBytes = size * 4;
    symtab.nextOffset += totalBytes;
    symtab.count++;

    printf("SYMBOL TABLE: ✓ Added array '%s[%d]' at stack offset %d (%d bytes)\n",
           name, size, symtab.vars[symtab.count - 1].offset, totalBytes);

    return symtab.vars[symtab.count - 1].offset;
}

/* Look up a variable's stack offset */
int getVarOffset(char* name) {
    /* Linear search through symbol table */
    for (int i = 0; i < symtab.count; i++) {
        if (strcmp(symtab.vars[i].name, name) == 0) {
            // Only print for debugging if needed - too verbose otherwise
            // printf("SYMBOL TABLE: Found '%s' at offset %d\n", name, symtab.vars[i].offset);
            return symtab.vars[i].offset;  /* Found it */
        }
    }
    printf("SYMBOL TABLE: ✗ Variable '%s' not found (undeclared)\n", name);
    return -1;  /* Variable not found - semantic error */
}

/* Check if a variable has been declared */
int isVarDeclared(char* name) {
    return getVarOffset(name) != -1;  /* True if found, false otherwise */
}

/* Get array size */
int getArraySize(char* name) {
    for (int i = 0; i < symtab.count; i++) {
        if (strcmp(symtab.vars[i].name, name) == 0) {
            return symtab.vars[i].isArray ? symtab.vars[i].arraySize : -1;
        }
    }
    return -1;  /* Not found or not an array */
}

/* Check if variable is an array */
int isArray(char* name) {
    for (int i = 0; i < symtab.count; i++) {
        if (strcmp(symtab.vars[i].name, name) == 0) {
            return symtab.vars[i].isArray;
        }
    }
    return 0;  /* Not found, so not an array */
}

/* Print current symbol table contents for debugging/tracing */
void printSymTab() {
    printf("\n=== SYMBOL TABLE STATE ===\n");
    printf("Count: %d, Next Offset: %d\n", symtab.count, symtab.nextOffset);
    if (symtab.count == 0) {
        printf("(empty)\n");
    } else {
        printf("Variables:\n");
        for (int i = 0; i < symtab.count; i++) {
            if (symtab.vars[i].isArray) {
                printf("  [%d] %s[%d] -> offset %d (%d bytes)\n",
                       i, symtab.vars[i].name, symtab.vars[i].arraySize,
                       symtab.vars[i].offset, symtab.vars[i].arraySize * 4);
            } else {
                printf("  [%d] %s -> offset %d\n",
                       i, symtab.vars[i].name, symtab.vars[i].offset);
            }
        }
    }
    printf("==========================\n\n");
}