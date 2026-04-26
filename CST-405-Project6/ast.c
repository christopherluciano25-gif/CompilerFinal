/* AST IMPLEMENTATION
 * Functions to create and manipulate Abstract Syntax Tree nodes
 * The AST is built during parsing and used for all subsequent phases
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"

/* External line number from scanner */
extern int yylineno;

/* Create a number literal node */
ASTNode* createNum(int value) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_NUM;
    node->lineno = yylineno;
    node->data.num = value;  /* Store the integer value */
    return node;
}

/* Create a variable reference node */
ASTNode* createVar(char* name) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_VAR;
    node->lineno = yylineno;
    node->data.name = strdup(name);  /* Copy the variable name */
    return node;
}

/* Create a binary operation node (for addition) */
ASTNode* createBinOp(char op, ASTNode* left, ASTNode* right) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_BINOP;
    node->lineno = yylineno;
    node->data.binop.op = op;        /* Store operator (+) */
    node->data.binop.left = left;    /* Left subtree */
    node->data.binop.right = right;  /* Right subtree */
    return node;
}

/* Create a variable declaration node */
ASTNode* createDecl(char* name) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_DECL;
    node->lineno = yylineno;
    node->data.name = strdup(name);  /* Store variable name */
    return node;
}

/*
ASTNode* createDeclWithAssgn(char* name, int value) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_DECL;
    node->data.name = strdup(name); 
    node->data.value = value;
    return node;
}

*/

/* Create an assignment statement node */
ASTNode* createAssign(char* var, ASTNode* value) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_ASSIGN;
    node->lineno = yylineno;
    node->data.assign.var = var ? strdup(var) : NULL;  /* Variable name (NULL for array assignments) */
    node->data.assign.value = value;      /* Expression tree */
    node->data.assign.arrayLHS = NULL;    /* Initialize to NULL */
    return node;
}

/* Create a print statement node */
ASTNode* createPrint(ASTNode* expr) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_PRINT;
    node->lineno = yylineno;
    node->data.expr = expr;  /* Expression to print */
    return node;
}

/* Create a statement list node (links statements together) */
ASTNode* createStmtList(ASTNode* stmt1, ASTNode* stmt2) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_STMT_LIST;
    node->lineno = stmt1 ? stmt1->lineno : (stmt2 ? stmt2->lineno : yylineno);
    node->data.stmtlist.stmt = stmt1;  /* First statement */
    node->data.stmtlist.next = stmt2;  /* Rest of list */
    return node;
}

/* Create a function definition node */
ASTNode* createFuncDef(char* name, ASTNode* params, ASTNode* body) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_FUNC_DEF;
    node->lineno = yylineno;
    node->data.func_def.name = strdup(name);
    node->data.func_def.params = params;
    node->data.func_def.body = body;
    return node;
}

/* Create a parameter node */
ASTNode* createParam(char* name) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_PARAM;
    node->lineno = yylineno;
    node->data.param.name = strdup(name);
    return node;
}

/* Create a parameter list node */
ASTNode* createParamList(ASTNode* param, ASTNode* next) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_PARAM_LIST;
    node->lineno = param ? param->lineno : yylineno;
    node->data.param_list.param = param;
    node->data.param_list.next = next;
    return node;
}

/* Create a function call node */
ASTNode* createFuncCall(char* name, ASTNode* args) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_FUNC_CALL;
    node->lineno = yylineno;
    node->data.func_call.name = strdup(name);
    node->data.func_call.args = args;
    return node;
}

/* Create an argument list node */
ASTNode* createArgList(ASTNode* expr, ASTNode* next) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_ARG_LIST;
    node->lineno = expr ? expr->lineno : yylineno;
    node->data.arg_list.expr = expr;
    node->data.arg_list.next = next;
    return node;
}

/* Create a return statement node */
ASTNode* createReturn(ASTNode* expr) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_RETURN;
    node->lineno = yylineno;
    node->data.ret.expr = expr;
    return node;
}

/* Create an if statement node */
ASTNode* createIf(ASTNode* condition, ASTNode* then_stmt, ASTNode* else_stmt) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_IF;
    node->lineno = yylineno;
    node->data.if_stmt.condition = condition;
    node->data.if_stmt.then_stmt = then_stmt;
    node->data.if_stmt.else_stmt = else_stmt;
    return node;
}

/* Create a while loop node */
ASTNode* createWhile(ASTNode* condition, ASTNode* body) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_WHILE;
    node->lineno = yylineno;
    node->data.while_stmt.condition = condition;
    node->data.while_stmt.body = body;
    return node;
}

/* Create a for loop node: for (init; condition; update) body */
ASTNode* createFor(ASTNode* init, ASTNode* condition, ASTNode* update, ASTNode* body) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_FOR;
    node->lineno = yylineno;
    node->data.for_stmt.init      = init;
    node->data.for_stmt.condition = condition;
    node->data.for_stmt.update    = update;
    node->data.for_stmt.body      = body;
    return node;
}

/* Create a do-while loop node: do body while (condition); */
ASTNode* createDoWhile(ASTNode* body, ASTNode* condition) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_DO_WHILE;
    node->lineno = yylineno;
    node->data.do_while_stmt.body      = body;
    node->data.do_while_stmt.condition = condition;
    return node;
}

/* ── Project 5: Switch, Case, Logical Operators ─────────────────── */

ASTNode* createSwitch(ASTNode* expr, ASTNode* cases) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_SWITCH;
    node->lineno = yylineno;
    node->data.switch_stmt.expr  = expr;
    node->data.switch_stmt.cases = cases;
    return node;
}

ASTNode* createCase(int isDefault, int value, ASTNode* body, ASTNode* next) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_CASE;
    node->lineno = yylineno;
    node->data.case_clause.isDefault = isDefault;
    node->data.case_clause.value     = value;
    node->data.case_clause.body      = body;
    node->data.case_clause.next      = next;
    return node;
}

ASTNode* createAnd(ASTNode* left, ASTNode* right) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_AND;
    node->lineno = yylineno;
    /* Reuse binop fields for left/right */
    node->data.binop.op    = '&';
    node->data.binop.left  = left;
    node->data.binop.right = right;
    return node;
}

ASTNode* createOr(ASTNode* left, ASTNode* right) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_OR;
    node->lineno = yylineno;
    node->data.binop.op    = '|';
    node->data.binop.left  = left;
    node->data.binop.right = right;
    return node;
}

ASTNode* createNot(ASTNode* expr) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_NOT;
    node->lineno = yylineno;
    node->data.not_expr = expr;
    return node;
}

/* Create a block statement node */
ASTNode* createBlock(ASTNode* stmt_list) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_BLOCK;
    node->lineno = yylineno;
    node->data.block.stmt_list = stmt_list;
    return node;
}

/* Create an array declaration node */
ASTNode* createArrayDecl(char* name, int size) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_ARRAY_DECL;
    node->lineno = yylineno;
    node->data.array_decl.name = strdup(name);
    node->data.array_decl.size = size;
    node->data.array_decl.isParam = 0;
    return node;
}

/* Create an array parameter node (size unknown) */
ASTNode* createArrayParam(char* name) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_ARRAY_DECL;
    node->lineno = yylineno;
    node->data.array_decl.name = strdup(name);
    node->data.array_decl.size = 0;  /* Size unknown for parameters */
    node->data.array_decl.isParam = 1;
    return node;
}

/* Create an array indexing node */
ASTNode* createArrayIndex(char* name, ASTNode* index) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_ARRAY_INDEX;
    node->lineno = yylineno;
    node->data.array_index.name = strdup(name);
    node->data.array_index.index = index;
    return node;
}

/* Display the AST structure (for debugging and education) */
void printAST(ASTNode* node, int level) {
    if (!node) return;

    /* Indent based on tree depth */
    for (int i = 0; i < level; i++) printf("  ");

    /* Print node based on its type */
    switch(node->type) {
        case NODE_NUM:
            printf("NUM: %d\n", node->data.num);
            break;
        case NODE_VAR:
            printf("VAR: %s\n", node->data.name);
            break;
        case NODE_BINOP:
            printf("BINOP: %c\n", node->data.binop.op);
            printAST(node->data.binop.left, level + 1);
            printAST(node->data.binop.right, level + 1);
            break;
        case NODE_DECL:
            printf("DECL: %s\n", node->data.name);
            break;
        case NODE_ASSIGN:
            if (node->data.assign.arrayLHS) {
                printf("ASSIGN (array element)\n");
                for (int i = 0; i < level + 1; i++) printf("  ");
                printf("LHS:\n");
                printAST(node->data.assign.arrayLHS, level + 2);
            } else {
                printf("ASSIGN: %s\n", node->data.assign.var);
            }
            for (int i = 0; i < level + 1; i++) printf("  ");
            printf("VALUE:\n");
            printAST(node->data.assign.value, level + 2);
            break;
        case NODE_PRINT:
            printf("PRINT\n");
            printAST(node->data.expr, level + 1);
            break;
        case NODE_STMT_LIST:
            /* Print statements in sequence at same level */
            printAST(node->data.stmtlist.stmt, level);
            printAST(node->data.stmtlist.next, level);
            break;
        case NODE_FUNC_DEF:
            printf("FUNC_DEF: %s\n", node->data.func_def.name);
            if (node->data.func_def.params) {
                for (int i = 0; i < level + 1; i++) printf("  ");
                printf("PARAMS:\n");
                printAST(node->data.func_def.params, level + 2);
            }
            for (int i = 0; i < level + 1; i++) printf("  ");
            printf("BODY:\n");
            printAST(node->data.func_def.body, level + 2);
            break;
        case NODE_PARAM:
            printf("PARAM: %s\n", node->data.param.name);
            break;
        case NODE_PARAM_LIST:
            printAST(node->data.param_list.param, level);
            printAST(node->data.param_list.next, level);
            break;
        case NODE_FUNC_CALL:
            printf("FUNC_CALL: %s\n", node->data.func_call.name);
            if (node->data.func_call.args) {
                for (int i = 0; i < level + 1; i++) printf("  ");
                printf("ARGS:\n");
                printAST(node->data.func_call.args, level + 2);
            }
            break;
        case NODE_ARG_LIST:
            printAST(node->data.arg_list.expr, level);
            printAST(node->data.arg_list.next, level);
            break;
        case NODE_RETURN:
            printf("RETURN\n");
            if (node->data.ret.expr) {
                printAST(node->data.ret.expr, level + 1);
            }
            break;
        case NODE_IF:
            printf("IF\n");
            for (int i = 0; i < level + 1; i++) printf("  ");
            printf("CONDITION:\n");
            printAST(node->data.if_stmt.condition, level + 2);
            for (int i = 0; i < level + 1; i++) printf("  ");
            printf("THEN:\n");
            printAST(node->data.if_stmt.then_stmt, level + 2);
            if (node->data.if_stmt.else_stmt) {
                for (int i = 0; i < level + 1; i++) printf("  ");
                printf("ELSE:\n");
                printAST(node->data.if_stmt.else_stmt, level + 2);
            }
            break;
        case NODE_WHILE:
            printf("WHILE\n");
            for (int i = 0; i < level + 1; i++) printf("  ");
            printf("CONDITION:\n");
            printAST(node->data.while_stmt.condition, level + 2);
            for (int i = 0; i < level + 1; i++) printf("  ");
            printf("BODY:\n");
            printAST(node->data.while_stmt.body, level + 2);
            break;
        case NODE_FOR:
            printf("FOR\n");
            if (node->data.for_stmt.init) {
                for (int i = 0; i < level + 1; i++) printf("  ");
                printf("INIT:\n");
                printAST(node->data.for_stmt.init, level + 2);
            }
            for (int i = 0; i < level + 1; i++) printf("  ");
            printf("CONDITION:\n");
            printAST(node->data.for_stmt.condition, level + 2);
            if (node->data.for_stmt.update) {
                for (int i = 0; i < level + 1; i++) printf("  ");
                printf("UPDATE:\n");
                printAST(node->data.for_stmt.update, level + 2);
            }
            for (int i = 0; i < level + 1; i++) printf("  ");
            printf("BODY:\n");
            printAST(node->data.for_stmt.body, level + 2);
            break;
        case NODE_DO_WHILE:
            printf("DO_WHILE\n");
            for (int i = 0; i < level + 1; i++) printf("  ");
            printf("BODY:\n");
            printAST(node->data.do_while_stmt.body, level + 2);
            for (int i = 0; i < level + 1; i++) printf("  ");
            printf("CONDITION:\n");
            printAST(node->data.do_while_stmt.condition, level + 2);
            break;
        case NODE_BLOCK:
            printf("BLOCK\n");
            printAST(node->data.block.stmt_list, level + 1);
            break;
        case NODE_ARRAY_DECL:
            if (node->data.array_decl.isParam) {
                printf("ARRAY_PARAM: %s[]\n", node->data.array_decl.name);
            } else {
                printf("ARRAY_DECL: %s[%d]\n",
                       node->data.array_decl.name,
                       node->data.array_decl.size);
            }
            break;
        case NODE_ARRAY_INDEX:
            printf("ARRAY_INDEX: %s\n", node->data.array_index.name);
            for (int i = 0; i < level + 1; i++) printf("  ");
            printf("INDEX:\n");
            printAST(node->data.array_index.index, level + 2);
            break;
        case NODE_SWITCH:
            printf("SWITCH\n");
            for (int i = 0; i < level + 1; i++) printf("  ");
            printf("EXPR:\n");
            printAST(node->data.switch_stmt.expr, level + 2);
            for (int i = 0; i < level + 1; i++) printf("  ");
            printf("CASES:\n");
            printAST(node->data.switch_stmt.cases, level + 2);
            break;
        case NODE_CASE: {
            ASTNode* c = node;
            while (c) {
                for (int i = 0; i < level; i++) printf("  ");
                if (c->data.case_clause.isDefault)
                    printf("DEFAULT:\n");
                else
                    printf("CASE %d:\n", c->data.case_clause.value);
                printAST(c->data.case_clause.body, level + 1);
                c = c->data.case_clause.next;
            }
            break;
        }
        case NODE_AND:
            printf("AND (&&)\n");
            printAST(node->data.binop.left,  level + 1);
            printAST(node->data.binop.right, level + 1);
            break;
        case NODE_OR:
            printf("OR (||)\n");
            printAST(node->data.binop.left,  level + 1);
            printAST(node->data.binop.right, level + 1);
            break;
        case NODE_NOT:
            printf("NOT (!)\n");
            printAST(node->data.not_expr, level + 1);
            break;
    }
}