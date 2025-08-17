#pragma once

#include <stddef.h>
#include "token.h"
#include "ast_dyn_node_array.h"

typedef enum {
    AST_PROGRAM,

    /* declarations */
    AST_VARIABLE_DECLARATION,
    AST_FUNCTION_DECLARATION,
    AST_PARAM,                /* function parameter */

    /* statements */
    AST_BLOCK,
    AST_IF_STATEMENT,
    AST_WHILE_STATEMENT,
    AST_FOR_STATEMENT,
    AST_RETURN_STATEMENT,
    AST_BREAK_STATEMENT,
    AST_CONTINUE_STATEMENT,
    AST_EXPR_STATEMENT,

    /* expressions */
    AST_LITERAL,
    AST_IDENTIFIER,
    AST_BINARY_EXPR,
    AST_UNARY_EXPR,           /* includes !expr, -expr, +expr, &expr ++expr --expr */
    AST_POSTFIX_EXPR,         /* for ++/-- and generic postfix chaining */
    AST_ASSIGNMENT_EXPR,      /* includes =, +=, -=, ... via op field */
    AST_CALL_EXPR,            /* functtion(args...) */
    AST_SUBSCRIPT_EXPR,       /* array[index] */

    /* types */
    AST_TYPE,

    AST_INITIALIZER_LIST

} AstNodeType;

typedef enum { OP_NULL,
               OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD,
               OP_EQ, OP_NEQ, OP_LT, OP_GT, OP_LE, OP_GE,
               OP_AND, OP_OR, OP_NOT,
               OP_ASSIGN, OP_PLUS_EQ, OP_MINUS_EQ, 
               OP_DEREF, OP_ADRESS, 
               OP_POST_INC, OP_POST_DEC, OP_PRE_INC, OP_PRE_DEC } OpKind;


typedef struct AstNode AstNode;

typedef struct {
    AstNodeArray decls;
} AstProgram;

typedef struct {
    AstNode *type;           /* type of the variable */
    char *name;              /* variable name */
    AstNode *initializer;    /* optional initializer expression */
} AstVariableDeclaration;

typedef struct {
    AstNode *return_type;    /* return type of the function */
    char *name;              /* function name */
    AstNodeArray params;     /* function parameters */
    AstNode *body;           /* function body (block) */
} AstFunctionDeclaration;



typedef struct {
    char *name;              /* parameter name */
    AstNode *type;           /* parameter type */
} AstParam;

typedef struct {
    AstNodeArray statements; /* statements in the block */
} AstBlock;

typedef struct {
    AstNode *condition;      /* condition expression */
    AstNode *then_branch;    /* then branch (block) */
    AstNode *else_branch;    /* else branch (block), optional */
} AstIfStatement;

typedef struct {
    AstNode *condition;      /* condition expression */
    AstNode *body;           /* body of the loop (block) */
} AstWhileStatement;

typedef struct {
    AstNode *init;           /* initialization statement (optional) */
    AstNode *condition;      /* loop condition (optional) */
    AstNode *post;           /* post-expression (optional) */
    AstNode *body;           /* body of the loop (block) */
} AstForStatement;

typedef struct {
    AstNode *expression;     /* expression to return */
} AstReturnStatement;

typedef struct {
    // Break statement, typically used to exit loops
} AstBreakStatement;

typedef struct {
    // Continue statement, typically used to skip to the next iteration of a loop
} AstContinueStatement;

typedef struct {
    AstNode *expression;     /* expression to evaluate */
} AstExprStatement;


typedef struct { char *value;}      AstLiteral;
typedef struct { char *identifier; } AstIdentifier;
typedef struct { AstNode *left; AstNode *right; OpKind op; } AstBinaryExpr;
typedef struct { OpKind op; AstNode *expr; } AstUnaryExpr;
typedef struct { AstNode *expr; OpKind op; } AstPostfixExpr;

typedef struct {
    AstNode *lvalue;         /* left-hand side (variable, array, etc.) */
    AstNode *rvalue;         /* right-hand side (expression) */
    OpKind op;               /* assignment operator (e.g., =, +=, -=) */
} AstAssignmentExpr;

typedef struct {
    AstNode *callee;         /* function being called */
    AstNodeArray args;
} AstCallExpr;

typedef struct {
    AstNode *target;         /* expression that evaluates to array/pointer */
    AstNode *index;          /* index expression */
} AstSubscriptExpr;


// int64*[10][20]**
// pointer pointer to an [10][20] array of int64 pointers
typedef struct {
    char *base_type;              /* base type name (e.g., "i32", "f64") */
    
    AstNodeArray sizes;

    size_t pre_stars;        /* number of '*' before the first '['  */
    size_t post_stars;       /* number of '*' after the last ']'     */

    /* const qualifiers */
    int base_is_const;       /* 'const' on the base type (const i32) */

} AstType;

typedef struct {
    AstNodeArray elements;
} AstInitilizeList;

struct AstNode {
    AstNodeType type;        /* type of the AST node */
    union {
        AstProgram program;
        AstVariableDeclaration variable_declaration;
        AstFunctionDeclaration function_declaration;
        AstParam param;
        AstBlock block;
        AstIfStatement if_statement;
        AstWhileStatement while_statement;
        AstForStatement for_statement;
        AstReturnStatement return_statement;
        AstBreakStatement break_statement;
        AstContinueStatement continue_statement;
        AstExprStatement expr_statement;

        AstLiteral literal;
        AstIdentifier identifier;
        AstBinaryExpr binary_expr;
        AstUnaryExpr unary_expr;
        AstPostfixExpr postfix_expr;
        AstAssignmentExpr assignment_expr;
        AstCallExpr call_expr;
        AstSubscriptExpr subscript_expr;

        AstType type;          /* type node */
        AstInitilizeList initializer_list; /* initializer list for arrays or structs */
    } data;                  /* union to hold different node types */
};

AstNode *ast_create_node(AstNodeType type);
void print_ast(AstNode *node, int indent);

void ast_node_free(AstNode *node);

void ast_program_push(AstProgram *program, AstNode *declaration);
void ast_argument_push(AstCallExpr *list, AstNode *argument);

int is_lvalue_node(AstNode *node);

int is_assignment_op(TokenType type);

void print_ast(AstNode *node, int indent);