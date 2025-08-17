#pragma once

#include <stddef.h>
#include "token.h"
#include "ast_dyn_node_array.h" /* your dynamic array of AstNode* */
#include "type.h"

typedef enum {
    AST_PROGRAM,

    /* declarations */
    AST_VARIABLE_DECLARATION,
    AST_FUNCTION_DECLARATION,
    AST_PARAM,

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
    AST_UNARY_EXPR,
    AST_POSTFIX_EXPR,
    AST_ASSIGNMENT_EXPR,
    AST_CALL_EXPR,
    AST_SUBSCRIPT_EXPR,

    /* types */
    AST_TYPE,

    AST_INITIALIZER_LIST

} AstNodeType;

typedef enum {
    OP_NULL,
    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD,
    OP_EQ, OP_NEQ, OP_LT, OP_GT, OP_LE, OP_GE,
    OP_AND, OP_OR, OP_NOT,
    OP_ASSIGN, OP_PLUS_EQ, OP_MINUS_EQ,
    OP_DEREF, OP_ADRESS,
    OP_POST_INC, OP_POST_DEC, OP_PRE_INC, OP_PRE_DEC
} OpKind;

typedef struct AstNode AstNode;

typedef struct {
    AstNodeArray decls;
} AstProgram;

typedef struct {
    AstNode *type;           /* type node */
    char *name;
    AstNode *initializer;    /* optional */
} AstVariableDeclaration;

typedef struct {
    AstNode *return_type;    /* AstNode of AST_TYPE */
    char *name;
    AstNodeArray params;     /* AstParam nodes */
    AstNode *body;           /* AstBlock */
} AstFunctionDeclaration;

typedef struct {
    char *name;
    AstNode *type;           /* AST_TYPE node */
} AstParam;

typedef struct {
    AstNodeArray statements;
} AstBlock;

typedef struct {
    AstNode *condition;
    AstNode *then_branch;
    AstNode *else_branch;
} AstIfStatement;

typedef struct {
    AstNode *condition;
    AstNode *body;
} AstWhileStatement;

typedef struct {
    AstNode *init;
    AstNode *condition;
    AstNode *post;
    AstNode *body;
} AstForStatement;

typedef struct {
    AstNode *expression;
} AstReturnStatement;

typedef struct { } AstBreakStatement;
typedef struct { } AstContinueStatement;

typedef struct {
    AstNode *expression;
} AstExprStatement;

/* small expression structs */
typedef struct { char *value; } AstLiteral;
typedef struct { char *identifier; } AstIdentifier;
typedef struct { AstNode *left; AstNode *right; OpKind op; } AstBinaryExpr;
typedef struct { OpKind op; AstNode *expr; } AstUnaryExpr;
typedef struct { AstNode *expr; OpKind op; } AstPostfixExpr;
typedef struct { AstNode *lvalue; AstNode *rvalue; OpKind op; } AstAssignmentExpr;
typedef struct { AstNode *callee; AstNodeArray args; } AstCallExpr;
typedef struct { AstNode *target; AstNode *index; } AstSubscriptExpr;

/* AST representation of a type (syntactic): e.g. i64*[10]*  */
typedef struct {
    char *base_type;         /* name like "i32", "i64" */
    AstNodeArray sizes;      /* array of expressions for sizes (may be empty) */
    size_t pre_stars;        /* '*' before first '[' */
    size_t post_stars;       /* '*' after last ']' */
    int base_is_const;       /* const on base */
} AstType;

typedef struct {
    AstNodeArray elements;
} AstInitializeList;

struct AstNode {
    AstNodeType node_type;   /* variant tag */
    int is_const_expr;       /* set by semantic analysis (1 if expression is compile-time const) */
    Type *sem_type;          /* semantic type computed in analysis (nullable) */

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

        AstType ast_type;
        AstInitializeList initializer_list;
    } data;
};

/* AST helper prototypes (implementations are up to you) */
AstNode *ast_create_node(AstNodeType type);
void ast_node_free(AstNode *node);
void print_ast(AstNode *node, int indent);
void ast_program_push(AstProgram *program, AstNode *declaration);
void ast_argument_push(AstCallExpr *list, AstNode *argument);
int is_lvalue_node(AstNode *node);
int is_assignment_op(TokenType type);
