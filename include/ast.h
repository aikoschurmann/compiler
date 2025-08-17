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

typedef enum {
    INT_LITERAL,
    FLOAT_LITERAL,
    BOOL_LITERAL,
    LIT_UNKNOWN /* used for error handling, not a real literal type */
} LiteralType;

typedef struct {
    LiteralType kind;
    union {
        long long   int_val;
        double      float_val;
        int         bool_val;   /* 0 or 1 */
    };
} ConstValue;


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
typedef struct { char *value; LiteralType type; } AstLiteral;
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

/* Represents one node in the abstract syntax tree (AST). */
/* Represents a node in the abstract syntax tree (AST). */
struct AstNode {
    AstNodeType node_type;    

    /*
     * is_const_expr:
     *   - Set to 1 if the language semantics guarantee that this node
     *     is a constant expression (i.e., it can be evaluated at compile time
     *     without side effects).
     *   - Set to 0 otherwise.
     *
     * Examples:
     *   const x = 5 + 6;    --> x.init_expr.is_const_expr = 1
     *   y = x + 2;          --> (x + 2).is_const_expr = 0
     */
    int is_const_expr;

    /*
     * sem_type:
     *   - The type determined during semantic analysis.
     *   - Nullable until the type is resolved.
     *   - Example: for `42`, sem_type = i64; for `true`, sem_type = bool.
     *   - Ownership: each AST node owns its Type, so it should be freed
     *     when the node is freed.
     */
    Type *sem_type;

    /*
     * const_value:
     *   - Stores the compile-time evaluated value of this expression, if known.
     *   - Can be set even if is_const_expr = 0 to support constant folding
     *     and other optimizations.
     *
     * Examples:
     *   a: i64 = 5;          --> a.is_const_expr = 0, a.const_value = 5
     *   b: i64 = a + 6;      --> b.is_const_expr = 0, b.const_value = 11
     *   const c = 2 + 3;     --> c.is_const_expr = 1, c.const_value = 5
     *
     *   Note: ConstValue is a struct capable of holding int, float, bool, etc.
     */
    ConstValue *const_value;

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
