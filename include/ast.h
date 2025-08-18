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
    AstNodeArray *decls;
} AstProgram;

typedef struct {
    AstNode *type;           /* type node */
    char *name;
    AstNode *initializer;    /* optional */
} AstVariableDeclaration;

typedef struct {
    AstNode *return_type;    /* AstNode of AST_TYPE */
    char *name;
    AstNodeArray *params;     /* AstParam nodes */
    AstNode *body;           /* AstBlock */
} AstFunctionDeclaration;

typedef struct {
    char *name;
    AstNode *type;           /* AST_TYPE node */
} AstParam;

typedef struct {
    AstNodeArray *statements;
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
typedef struct { AstNode *callee; AstNodeArray *args; } AstCallExpr;
typedef struct { AstNode *target; AstNode *index; } AstSubscriptExpr;

/* 
 * AST representation of a type (syntactic). This structure handles complex type expressions
 * including base types, pointers, arrays, function types, and grouped (parenthesized) types.
 * 
 * DESIGN OVERVIEW:
 * ================
 * The AstType structure supports three main categories of types:
 * 
 * 1. REGULAR TYPES (base_type != NULL, is_function = 0, return_type = NULL)
 *    Examples: i32, const i64*, bool[10], i32*[5]*
 *    - base_type: the fundamental type name ("i32", "i64", etc.)
 *    - base_is_const: whether the base type has const qualifier
 *    - pre_stars: number of '*' that appear before array dimensions
 *    - sizes: array of expressions for array dimensions (NULL entries = empty [])
 *    - post_stars: number of '*' that appear after array dimensions
 * 
 * 2. FUNCTION TYPES (is_function = 1, base_type = "function")
 *    Examples: fn(i32, i64) -> bool, const fn() -> i32*
 *    - is_function: set to 1
 *    - base_type: set to "function" (string literal)
 *    - base_is_const: whether the function itself is const
 *    - param_types: array of AstType nodes for each parameter
 *    - return_type: AstType node for return type (may be NULL for void)
 *    - pre_stars/post_stars/sizes: suffixes applied to the function type itself
 *      (e.g., fn() -> i32*[10] has post_stars=0, sizes=[10] applied to return type)
 * 
 * 3. GROUPED TYPES (base_type = NULL, is_function = 0, return_type != NULL)
 *    Examples: (i32*), (fn(i32) -> bool)[5], ((i32*)[10])*
 *    - return_type: contains the inner grouped type
 *    - base_is_const: const qualifier applied to the grouped expression
 *    - pre_stars/sizes/post_stars: suffixes applied after the closing parenthesis
 *    
 *    This is used for precedence control. For example:
 *    - (i32*)[10] = array of pointers to i32
 *    - i32*[10] = pointer to array of i32
 *    - (fn(i32) -> bool)[5] = array of functions
 *    - fn(i32) -> bool[5] = function returning array
 * 
 * FIELD DETAILS:
 * ==============
 * 
 * base_type:
 *   - For regular types: the type name ("i32", "i64", "bool", "f32", "f64", custom identifier)
 *   - For function types: always "function" (string literal for identification)
 *   - For grouped types: always NULL
 * 
 * sizes:
 *   - Array of AstNode* representing array dimension expressions
 *   - Each entry can be:
 *     * NULL: empty dimension like []
 *     * Expression node: computed dimension like [10 + 5], [x * y]
 *   - Applied left-to-right after pre_stars but before post_stars
 *   - Examples:
 *     * i32[10][20] → sizes = [10, 20]
 *     * i32[] → sizes = [NULL]
 *     * i32[x+y][10] → sizes = [BinaryExpr(x+y), 10]
 * 
 * pre_stars:
 *   - Number of '*' (pointer indirections) that appear before any array dimensions
 *   - Example: i32**[10] → pre_stars = 2, sizes = [10]
 *   - For function types: stars that apply to the entire function before arrays
 * 
 * post_stars:
 *   - Number of '*' that appear after all array dimensions
 *   - Example: i32[10]** → sizes = [10], post_stars = 2
 *   - For function types: stars that apply to the entire function after arrays
 * 
 * base_is_const:
 *   - For regular types: const qualifier on the base type (const i32)
 *   - For function types: const qualifier on the function (const fn(...))
 *   - For grouped types: const qualifier on the grouped expression (const (i32*))
 * 
 * is_function:
 *   - 1 if this represents a function type (fn(...) -> ...)
 *   - 0 for regular types and grouped types
 * 
 * param_types:
 *   - Only used for function types (is_function = 1)
 *   - Array of AstType nodes, one for each parameter
 *   - NULL or empty array for functions with no parameters
 *   - Each entry is a complete AstType that can itself be function/grouped/regular
 * 
 * return_type:
 *   - For function types: the return type (may be NULL for void functions)
 *   - For grouped types: the inner type being grouped
 *   - For regular types: always NULL
 *   - This is a complete AstType node that can be arbitrarily complex
 * 
 * PARSING EXAMPLES:
 * =================
 * 
 * i32*[10]:
 *   base_type="i32", pre_stars=1, sizes=[10], post_stars=0
 * 
 * i32[10]*:
 *   base_type="i32", pre_stars=0, sizes=[10], post_stars=1
 * 
 * const fn(i32, i64) -> bool*:
 *   is_function=1, base_is_const=1, base_type="function"
 *   param_types=[{base_type="i32"}, {base_type="i64"}]
 *   return_type={base_type="bool", post_stars=1}
 * 
 * (fn(i32) -> bool)[5]:
 *   return_type={is_function=1, param_types=[{base_type="i32"}], return_type={base_type="bool"}}
 *   sizes=[5]
 * 
 * fn() -> (i32*)[10]:
 *   is_function=1, param_types=[]
 *   return_type={return_type={base_type="i32", pre_stars=1}, sizes=[10]}
 * 
 * SUFFIX PRECEDENCE:
 * ==================
 * The parser processes type suffixes left-to-right according to the grammar:
 *   <Type> ::= [CONST] <TypeAtom> {<TypeSuffix>}
 *   <TypeSuffix> ::= STAR | L_SQB [<ConstExpr>] R_SQB
 * 
 * This means:
 * - i32*[10] parses as (i32*)[10] = array of pointers
 * - i32[10]* parses as (i32[10])* = pointer to array
 * - Parentheses can override: (i32*)[10] vs (i32)[10]*
 * 
 * The pre_stars/sizes/post_stars fields are populated based on the actual
 * suffix order encountered, preserving the semantic meaning.
 */
typedef struct {
    char *base_type;         /* type name or "function" or NULL for grouped */
    AstNodeArray *sizes;     /* array dimension expressions */
    size_t pre_stars;        /* '*' before arrays */
    size_t post_stars;       /* '*' after arrays */
    int base_is_const;       /* const qualifier */
    int is_function;         /* 1 = function type, 0 = regular/grouped */
    AstNodeArray *param_types; /* function parameters (function types only) */
    AstNode *return_type;    /* function return type OR grouped inner type */
} AstType;

typedef struct {
    AstNodeArray *elements;
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
