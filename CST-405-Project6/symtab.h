#ifndef SYMTAB_H
#define SYMTAB_H

/* SYMBOL TABLE
 * Tracks all declared variables during compilation
 * Maps variable names to their memory locations (stack offsets)
 * Used for semantic checking and code generation
 */

#define MAX_VARS 100  /* Maximum number of variables supported */

/* SYMBOL ENTRY - Information about each variable */
typedef struct {
    char* name;      /* Variable identifier */
    int offset;      /* Stack offset in bytes (for MIPS stack frame) */
    int isArray;     /* 1 if this is an array, 0 for scalar */
    int arraySize;   /* Number of elements (0 for non-arrays) */
} Symbol;

/* SYMBOL TABLE STRUCTURE */
typedef struct {
    Symbol vars[MAX_VARS];  /* Array of all variables */
    int count;              /* Number of variables declared */
    int nextOffset;         /* Next available stack offset */
} SymbolTable;

/* SYMBOL TABLE OPERATIONS */
void initSymTab();               /* Initialize empty symbol table */
int addVar(char* name);          /* Add new variable, returns offset or -1 if duplicate */
int addArray(char* name, int size);  /* Add array, returns offset or -1 if duplicate */
int getVarOffset(char* name);    /* Get stack offset for variable, -1 if not found */
int getArraySize(char* name);    /* Get array size, -1 if not array */
int isVarDeclared(char* name);   /* Check if variable exists (1=yes, 0=no) */
int isArray(char* name);         /* Check if variable is an array (1=yes, 0=no) */
void printSymTab();              /* Print current symbol table contents for tracing */

#endif