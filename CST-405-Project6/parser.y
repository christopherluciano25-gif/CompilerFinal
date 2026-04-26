%{
/* SYNTAX ANALYZER (PARSER) - WITH FUNCTION SUPPORT
 * This is the second phase of compilation - checking grammar rules
 * Bison generates a parser that builds an Abstract Syntax Tree (AST)
 * Now supports functions, control flow, and more operators
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"

/* External declarations for lexer interface */
extern int yylex();
extern int yyparse();
extern FILE* yyin;
extern int yylineno;  /* Line number from scanner */

void yyerror(const char* s);
ASTNode* root = NULL;
%}

/* SEMANTIC VALUES UNION */
%union {
    int num;
    char* str;
    struct ASTNode* node;
}

/* TOKEN DECLARATIONS */
%token <num> NUM
%token <str> ID
%token INT PRINT RETURN IF ELSE WHILE FOR DO
%token SWITCH CASE DEFAULT BREAK AND OR NOT
%token LE GE EQ NE

/* NON-TERMINAL TYPES */
%type <node> program decl_or_func_list decl_or_func
%type <node> func_def params param_list param
%type <node> stmt_list stmt decl assign expr print_stmt return_stmt
%type <node> if_stmt while_stmt for_stmt do_while_stmt block
%type <node> func_call arg_list args

/* OPERATOR PRECEDENCE AND ASSOCIATIVITY */
%right NOT
%left AND
%left OR
%left EQ NE
%left '<' '>' LE GE
%left '+' '-'
%left '*' '/' '%'
%right UMINUS

%%

/* PROGRAM RULE - Entry point */
program:
    decl_or_func_list {
        root = $1;
    }
    ;

/* Declaration or function list */
decl_or_func_list:
    decl_or_func {
        $$ = $1;
    }
    | decl_or_func_list decl_or_func {
        $$ = createStmtList($1, $2);
    }
    ;

/* Can be either a function definition or a variable declaration */
decl_or_func:
    func_def { $$ = $1; }
    | decl { $$ = $1; }
    ;

/* FUNCTION DEFINITION - int name(params) { body } */
func_def:
    INT ID '(' params ')' block {
        $$ = createFuncDef($2, $4, $6);
        free($2);
    }
    | INT ID '(' ')' block {
        $$ = createFuncDef($2, NULL, $5);
        free($2);
    }
    ;

/* PARAMETERS */
params:
    param_list { $$ = $1; }
    ;

param_list:
    param {
        $$ = $1;
    }
    | param_list ',' param {
        $$ = createParamList($1, $3);
    }
    ;

param:
    INT ID {
        $$ = createParam($2);
        free($2);
    }
    | INT ID '[' ']' {
        $$ = createArrayParam($2);
        free($2);
    }
    ;

/* BLOCK STATEMENT */
block:
    '{' stmt_list '}' {
        $$ = createBlock($2);
    }
    | '{' '}' {
        $$ = createBlock(NULL);
    }
    ;

/* STATEMENT LIST */
stmt_list:
    stmt {
        $$ = $1;
    }
    | stmt_list stmt {
        $$ = createStmtList($1, $2);
    }
    ;

/* STATEMENT TYPES */
stmt:
    decl
    | assign
    | print_stmt
    | return_stmt
    | if_stmt
    | while_stmt
    | for_stmt
    | do_while_stmt
    | block
    | func_call ';' { $$ = $1; }
    ;

/* DECLARATION - int x; or int arr[10]; */
decl:
    INT ID ';' {
        $$ = createDecl($2);
        free($2);
    }
    | INT ID '[' NUM ']' ';' {
        $$ = createArrayDecl($2, $4);
        free($2);
    }
    ;

/* ASSIGNMENT - x = expr; or arr[i] = expr; */
assign:
    ID '=' expr ';' {
        $$ = createAssign($1, $3);
        free($1);
    }
    | ID '[' expr ']' '=' expr ';' {
        ASTNode* lhs = createArrayIndex($1, $3);
        $$ = createAssign(NULL, $6);
        $$->data.assign.arrayLHS = lhs;
        free($1);
    }
    ;

/* RETURN STATEMENT */
return_stmt:
    RETURN expr ';' {
        $$ = createReturn($2);
    }
    | RETURN ';' {
        $$ = createReturn(NULL);
    }
    ;

/* IF STATEMENT */
if_stmt:
    IF '(' expr ')' stmt {
        $$ = createIf($3, $5, NULL);
    }
    | IF '(' expr ')' stmt ELSE stmt {
        $$ = createIf($3, $5, $7);
    }
    ;

/* WHILE LOOP */
while_stmt:
    WHILE '(' expr ')' stmt {
        $$ = createWhile($3, $5);
    }
    ;

/* FOR LOOP - for (init_assign ; condition ; update_assign) body
 * Supports: for (i = 0; i < 10; i = i + 1) { ... }
 * The init and update are full assignment statements (with semicolons in grammar)
 * We use a stripped assign rule (no trailing semicolon) for the update slot
 */
for_stmt:
    FOR '(' ID '=' expr ';' expr ';' ID '=' expr ')' stmt {
        ASTNode* init   = createAssign($3, $5);   free($3);
        ASTNode* update = createAssign($9, $11);  free($9);
        $$ = createFor(init, $7, update, $13);
    }
    ;

/* DO-WHILE LOOP - do body while (condition); */
do_while_stmt:
    DO stmt WHILE '(' expr ')' ';' {
        $$ = createDoWhile($2, $5);
    }
    ;

/* EXPRESSION RULES */
expr:
    NUM {
        $$ = createNum($1);
    }
    | ID {
        $$ = createVar($1);
        free($1);
    }
    | ID '[' expr ']' {
        $$ = createArrayIndex($1, $3);
        free($1);
    }
    | func_call {
        $$ = $1;
    }
    | expr '+' expr {
        $$ = createBinOp('+', $1, $3);
    }
    | expr '-' expr {
        $$ = createBinOp('-', $1, $3);
    }
    | expr '*' expr {
        $$ = createBinOp('*', $1, $3);
    }
    | expr '/' expr {
        $$ = createBinOp('/', $1, $3);
    }
    | expr '%' expr {
        $$ = createBinOp('%', $1, $3);  /* modulo */
    }
    | expr '<' expr {
        $$ = createBinOp('<', $1, $3);
    }
    | expr '>' expr {
        $$ = createBinOp('>', $1, $3);
    }
    | expr LE expr {
        $$ = createBinOp('l', $1, $3);  /* 'l' for <= */
    }
    | expr GE expr {
        $$ = createBinOp('g', $1, $3);  /* 'g' for >= */
    }
    | expr EQ expr {
        $$ = createBinOp('e', $1, $3);  /* 'e' for == */
    }
    | expr NE expr {
        $$ = createBinOp('n', $1, $3);  /* 'n' for != */
    }
    | '-' expr %prec UMINUS {
        $$ = createBinOp('u', $2, NULL);  /* 'u' for unary minus */
    }
    | expr AND expr {
        $$ = createAnd($1, $3);
    }
    | expr OR expr {
        $$ = createOr($1, $3);
    }
    | NOT expr {
        $$ = createNot($2);
    }
    | '(' expr ')' {
        $$ = $2;
    }
    ;

/* FUNCTION CALL */
func_call:
    ID '(' args ')' {
        $$ = createFuncCall($1, $3);
        free($1);
    }
    | ID '(' ')' {
        $$ = createFuncCall($1, NULL);
        free($1);
    }
    ;

/* ARGUMENTS */
args:
    arg_list { $$ = $1; }
    ;

arg_list:
    expr {
        $$ = $1;  /* Single argument becomes the arg node */
    }
    | arg_list ',' expr {
        $$ = createArgList($1, $3);
    }
    ;

/* PRINT STATEMENT */
print_stmt:
    PRINT '(' expr ')' ';' {
        $$ = createPrint($3);
    }
    ;

%%

/* ERROR HANDLING */
void yyerror(const char* s) {
    fprintf(stderr, "Syntax Error at line %d: %s\n", yylineno, s);
}
