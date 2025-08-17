#include "ast.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast_dyn_node_array.h"

AstNode *ast_create_node(AstNodeType type)
{
    AstNode *node = malloc(sizeof(*node));
    if (!node) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    node->type = type;
    memset(&node->data, 0, sizeof(node->data)); // Zero-initialize data
    return node;
}


/* Helper: free an array of AstNode* items (calls ast_node_free on each) */
static void free_node_array(AstNode **arr, size_t count) {
    if (!arr) return;
    for (size_t i = 0; i < count; ++i) {
        if (arr[i]) ast_node_free(arr[i]);
    }
    free(arr);
}

/* Main recursive free function */
void ast_node_free(AstNode *node) {
    if (!node) return;

    switch (node->type) {
        case AST_PROGRAM:
            astnode_array_free(&node->data.program.decls);
            break;

        case AST_VARIABLE_DECLARATION:
            if (node->data.variable_declaration.type)
                ast_node_free(node->data.variable_declaration.type);
            free(node->data.variable_declaration.name);
            if (node->data.variable_declaration.initializer)
                ast_node_free(node->data.variable_declaration.initializer);
            break;

        case AST_FUNCTION_DECLARATION:
            if (node->data.function_declaration.return_type)
                ast_node_free(node->data.function_declaration.return_type);
            free(node->data.function_declaration.name);
            /* params are AstNode* (AST_PARAM nodes) */
            astnode_array_free(&node->data.function_declaration.params);
            if (node->data.function_declaration.body)
                ast_node_free(node->data.function_declaration.body);
            break;

        case AST_PARAM:
            free(node->data.param.name);
            if (node->data.param.type)
                ast_node_free(node->data.param.type);
            break;

        case AST_BLOCK:
            astnode_array_free(&node->data.block.statements);
            break;

        case AST_IF_STATEMENT:
            if (node->data.if_statement.condition)
                ast_node_free(node->data.if_statement.condition);
            if (node->data.if_statement.then_branch)
                ast_node_free(node->data.if_statement.then_branch);
            if (node->data.if_statement.else_branch)
                ast_node_free(node->data.if_statement.else_branch);
            break;

        case AST_WHILE_STATEMENT:
            if (node->data.while_statement.condition)
                ast_node_free(node->data.while_statement.condition);
            if (node->data.while_statement.body)
                ast_node_free(node->data.while_statement.body);
            break;

        case AST_FOR_STATEMENT:
            if (node->data.for_statement.init)
                ast_node_free(node->data.for_statement.init);
            if (node->data.for_statement.condition)
                ast_node_free(node->data.for_statement.condition);
            if (node->data.for_statement.post)
                ast_node_free(node->data.for_statement.post);
            if (node->data.for_statement.body)
                ast_node_free(node->data.for_statement.body);
            break;

        case AST_RETURN_STATEMENT:
            if (node->data.return_statement.expression)
                ast_node_free(node->data.return_statement.expression);
            break;

        case AST_BREAK_STATEMENT:
        case AST_CONTINUE_STATEMENT:
            /* nothing inside */
            break;

        case AST_EXPR_STATEMENT:
            if (node->data.expr_statement.expression)
                ast_node_free(node->data.expr_statement.expression);
            break;

        /* Expressions */
        case AST_LITERAL:
            free(node->data.literal.value);
            break;

        case AST_IDENTIFIER:
            free(node->data.identifier.identifier);
            break;

        case AST_BINARY_EXPR:
            if (node->data.binary_expr.left)
                ast_node_free(node->data.binary_expr.left);
            if (node->data.binary_expr.right)
                ast_node_free(node->data.binary_expr.right);
            break;

        case AST_UNARY_EXPR:
            if (node->data.unary_expr.expr)
                ast_node_free(node->data.unary_expr.expr);
            break;

        case AST_POSTFIX_EXPR:
            if (node->data.postfix_expr.expr)
                ast_node_free(node->data.postfix_expr.expr);
            /* postfix op is an enum value only; nothing else to free */
            break;

        case AST_ASSIGNMENT_EXPR:
            if (node->data.assignment_expr.lvalue)
                ast_node_free(node->data.assignment_expr.lvalue);
            if (node->data.assignment_expr.rvalue)
                ast_node_free(node->data.assignment_expr.rvalue);
            break;

        case AST_CALL_EXPR:
            if (node->data.call_expr.callee)
                ast_node_free(node->data.call_expr.callee);
            astnode_array_free(&node->data.call_expr.args);
            break;

        case AST_SUBSCRIPT_EXPR:
            if (node->data.subscript_expr.target)
                ast_node_free(node->data.subscript_expr.target);
            if (node->data.subscript_expr.index)
                ast_node_free(node->data.subscript_expr.index);
            break;

        case AST_TYPE:
            free(node->data.type.base_type);
            astnode_array_free(&node->data.type.sizes);
            break;
        
        case AST_INITIALIZER_LIST:
            astnode_array_free(&node->data.initializer_list.elements);
            break;

        default:
            /* unknown node kind: nothing special to free */
            break;
    }

    free(node);
}


/* Return 1 if node is a syntactic lvalue (can appear on left side of =),
 * 0 otherwise.
 *
 * Important: this only answers the *syntactic* question. Semantic checks
 * (is the lvalue actually assignable? is it const? type compatibility?)
 * must be done in a later semantic pass.
 */
int is_lvalue_node(AstNode *node) {
    if (!node) return 0;

    switch (node->type) {
        case AST_IDENTIFIER:
            /* Simple variable name is an lvalue. */
            return 1;

        case AST_SUBSCRIPT_EXPR:
            /* a[b] yields an lvalue (array element). */
            return 1;

        case AST_UNARY_EXPR:
            /* Unary '*' (dereference) produces an lvalue: *(p) = ... */
            if (node->data.unary_expr.op == OP_DEREF) return 1;
            /* '&' (address-of), '+', '-', '!' are not lvalues. */
            return 0;

        case AST_POSTFIX_EXPR: {
            /* Postfix expressions like 'a++' or 'a--' are rvalues,
               since they yield the value of 'a' before the increment/decrement. not a storage location.
             */

            OpKind k = node->data.postfix_expr.op;
            if(k == OP_POST_INC || k == OP_POST_DEC) {
                return 0; // postfix increment/decrement is not an lvalue
            }
            /* Other postfix expressions (like 'a.b' or 'a->b') are lvalues.
               (not implemented yet) */

            return 0; // not implemented yet
            
        }

        case AST_ASSIGNMENT_EXPR:
        case AST_CALL_EXPR:
        case AST_LITERAL:
        case AST_BINARY_EXPR:
        case AST_EXPR_STATEMENT:
        case AST_RETURN_STATEMENT:
        case AST_IF_STATEMENT:
        case AST_WHILE_STATEMENT:
        case AST_FOR_STATEMENT:
        case AST_BREAK_STATEMENT:
        case AST_CONTINUE_STATEMENT:
        case AST_PROGRAM:
        case AST_VARIABLE_DECLARATION:
        case AST_FUNCTION_DECLARATION:
        case AST_PARAM:
        case AST_BLOCK:
        case AST_TYPE:
        default:
            return 0;
    }
}

int is_assignment_op(TokenType type) {
    return (type == TOK_ASSIGN || type == TOK_PLUS_EQ || 
            type == TOK_MINUS_EQ || type == TOK_STAR_EQ ||
            type == TOK_SLASH_EQ || type == TOK_PERCENT_EQ);
}

static void print_indent(int level)
{
    for (int i = 0; i < level; i++) {
        fputs("  ", stdout);
    }
}

/* helper: pretty-print OpKind */
static const char *opkind_to_string(OpKind op) {
    switch (op) {
        case OP_ADD: return "+";
        case OP_SUB: return "-";
        case OP_MUL: return "*";
        case OP_DIV: return "/";
        case OP_MOD: return "%";
        case OP_EQ: return "==";
        case OP_NEQ: return "!=";
        case OP_LT: return "<";
        case OP_GT: return ">";
        case OP_LE: return "<=";
        case OP_GE: return ">=";
        case OP_AND: return "&&";
        case OP_OR: return "||";
        case OP_NOT: return "!";
        case OP_ASSIGN: return "=";
        case OP_PLUS_EQ: return "+=";
        case OP_MINUS_EQ: return "-=";
        case OP_DEREF: return "* (deref)";
        case OP_ADRESS: return "&";
        case OP_POST_INC: return "++ (post)";
        case OP_POST_DEC: return "-- (post)";
        case OP_PRE_INC: return "++ (pre)";
        case OP_PRE_DEC: return "-- (pre)";
        case OP_NULL: default: return "(op-null)";
    }
}

/* print function parameters (AstParam) */
/* print function parameters (AstParam) -- dynarray version */
static void print_params(AstNodeArray *params, int indent) {
    print_indent(indent);
    printf("Parameters:\n");
    size_t n = astnode_array_count(params);
    for (size_t i = 0; i < n; ++i) {
        AstNode *p = astnode_array_get(params, i);
        if (!p) continue;
        print_indent(indent + 1);
        printf("Param: %s", p->data.param.name ? p->data.param.name : "(anon)");
        if (p->data.param.type) {
            printf("\n");
            /* Print type node inline if available (simple) */
            print_ast(p->data.param.type, indent + 2);
        }
    }
}


/* print argument list (call) */
static void print_call_args(AstNodeArray *args, int indent) {
    print_indent(indent);
    printf("Args:\n");
    size_t n = astnode_array_count(args);
    for (size_t i = 0; i < n; ++i) {
        
        AstNode *arg = astnode_array_get(args, i);
        if (!arg) continue;
        print_ast(arg, indent + 1);
    }
}


/* main printer */
void print_ast(AstNode *node, int indent) {
    if (!node) {
        print_indent(indent);
        printf("(null)\n");
        return;
    }

    switch (node->type) {
        case AST_PROGRAM: {
            print_indent(indent);
            printf("Program:\n");

            size_t n = astnode_array_count(&node->data.program.decls);
            for (size_t i = 0; i < n; ++i) {
                AstNode *child = astnode_array_get(&node->data.program.decls, i);
                print_ast(child, indent + 1);
            }
            break;
        }           

        case AST_VARIABLE_DECLARATION: {
            print_indent(indent);
            printf("Declaration:\n");
            print_indent(indent + 1);
            printf("Variable: %s\n", node->data.variable_declaration.name ? node->data.variable_declaration.name : "(anon)");
            if (node->data.variable_declaration.type) {
                print_indent(indent + 2);
                printf("Type:\n");
                print_ast(node->data.variable_declaration.type, indent + 3);
            }
            if (node->data.variable_declaration.initializer) {
                print_indent(indent + 2);
                printf("Initializer:\n");
                print_ast(node->data.variable_declaration.initializer, indent + 3);
            }
            break;
        }

        case AST_FUNCTION_DECLARATION: {
            print_indent(indent);
            printf("Function: %s\n", node->data.function_declaration.name ? node->data.function_declaration.name : "(anon)");
            if (node->data.function_declaration.return_type) {
                print_indent(indent + 1);
                printf("ReturnType:\n");
                print_ast(node->data.function_declaration.return_type, indent + 2);
            }
        
            /* use dynarray for params now */
            if (astnode_array_count(&node->data.function_declaration.params) > 0) {
                print_params(&node->data.function_declaration.params, indent + 1);
            } else {
                print_indent(indent + 1);
                printf("Parameters: (none)\n");
            }
        
            if (node->data.function_declaration.body) {
                print_indent(indent + 1);
                printf("Body:\n");
                print_ast(node->data.function_declaration.body, indent + 2);
            }
            break;
        }

        case AST_PARAM: {
            print_indent(indent);
            printf("Param: %s\n", node->data.param.name ? node->data.param.name : "(anon)");
            if (node->data.param.type) {
                print_indent(indent + 1);
                printf("Type:\n");
                print_ast(node->data.param.type, indent + 2);
            }
            break;
        }

        case AST_BLOCK: {
            print_indent(indent);
            printf("Block:\n");
            size_t n = astnode_array_count(&node->data.block.statements);
            for (size_t i = 0; i < n; ++i) {
                AstNode *stmt = astnode_array_get(&node->data.block.statements, i);
                print_ast(stmt, indent + 1);
            }
            break;
            
        }

        case AST_IF_STATEMENT: {
            print_indent(indent);
            printf("IfStatement:\n");
            if (node->data.if_statement.condition) {
                print_indent(indent + 1); printf("Condition:\n");
                print_ast(node->data.if_statement.condition, indent + 2);
            }
            if (node->data.if_statement.then_branch) {
                print_indent(indent + 1); printf("Then:\n");
                print_ast(node->data.if_statement.then_branch, indent + 2);
            }
            if (node->data.if_statement.else_branch) {
                print_indent(indent + 1); printf("Else:\n");
                print_ast(node->data.if_statement.else_branch, indent + 2);
            }
            break;
        }

        case AST_WHILE_STATEMENT: {
            print_indent(indent);
            printf("WhileLoop:\n");
            if (node->data.while_statement.condition) {
                print_indent(indent + 1); printf("Condition:\n");
                print_ast(node->data.while_statement.condition, indent + 2);
            }
            if (node->data.while_statement.body) {
                print_indent(indent + 1); printf("Body:\n");
                print_ast(node->data.while_statement.body, indent + 2);
            }
            break;
        }

        case AST_FOR_STATEMENT: {
            print_indent(indent);
            printf("ForLoop:\n");
            if (node->data.for_statement.init) {
                print_indent(indent + 1); printf("Init:\n");
                print_ast(node->data.for_statement.init, indent + 2);
            }
            if (node->data.for_statement.condition) {
                print_indent(indent + 1); printf("Condition:\n");
                print_ast(node->data.for_statement.condition, indent + 2);
            }
            if (node->data.for_statement.post) {
                print_indent(indent + 1); printf("Post:\n");
                print_ast(node->data.for_statement.post, indent + 2);
            }
            if (node->data.for_statement.body) {
                print_indent(indent + 1); printf("Body:\n");
                print_ast(node->data.for_statement.body, indent + 2);
            }
            break;
        }

        case AST_RETURN_STATEMENT: {
            print_indent(indent);
            printf("ReturnStatement:\n");
            if (node->data.return_statement.expression) {
                print_ast(node->data.return_statement.expression, indent + 1);
            }
            break;
        }

        case AST_BREAK_STATEMENT: {
            print_indent(indent);
            printf("BreakStatement\n");
            break;
        }

        case AST_CONTINUE_STATEMENT: {
            print_indent(indent);
            printf("ContinueStatement\n");
            break;
        }

        case AST_EXPR_STATEMENT: {
            print_indent(indent);
            printf("ExprStatement:\n");
            if (node->data.expr_statement.expression) {
                print_ast(node->data.expr_statement.expression, indent + 1);
            }
            break;
        }

        case AST_LITERAL: {
            print_indent(indent);
            printf("Literal: %s\n", node->data.literal.value ? node->data.literal.value : "(null)");
            break;
        }

        case AST_IDENTIFIER: {
            print_indent(indent);
            printf("Variable: %s\n", node->data.identifier.identifier ? node->data.identifier.identifier : "(anon)");
            break;
        }

        case AST_BINARY_EXPR: {
            print_indent(indent);
            printf("BinaryOp: %s\n", opkind_to_string(node->data.binary_expr.op));
            if (node->data.binary_expr.left) print_ast(node->data.binary_expr.left, indent + 1);
            if (node->data.binary_expr.right) print_ast(node->data.binary_expr.right, indent + 1);
            break;
        }

        case AST_UNARY_EXPR: {
            print_indent(indent);
            printf("UnaryOp: %s\n", opkind_to_string(node->data.unary_expr.op));
            if (node->data.unary_expr.expr) print_ast(node->data.unary_expr.expr, indent + 1);
            break;
        }

        case AST_POSTFIX_EXPR: {
            print_indent(indent);
            printf("PostfixOp: %s\n", opkind_to_string(node->data.postfix_expr.op));
            if (node->data.postfix_expr.expr) print_ast(node->data.postfix_expr.expr, indent + 1);
            break;
        }

        case AST_ASSIGNMENT_EXPR: {
            print_indent(indent);
            printf("Assignment: %s\n", opkind_to_string(node->data.assignment_expr.op));
            if (node->data.assignment_expr.lvalue) print_ast(node->data.assignment_expr.lvalue, indent + 1);
            if (node->data.assignment_expr.rvalue) print_ast(node->data.assignment_expr.rvalue, indent + 1);
            break;
        }

        case AST_CALL_EXPR: {
            print_indent(indent);
            printf("Call:\n");
            if (node->data.call_expr.callee) {
                print_indent(indent + 1); printf("Callee:\n");
                print_ast(node->data.call_expr.callee, indent + 2);
            }
            /* use dynarray helpers for args */
            size_t nargs = astnode_array_count(&node->data.call_expr.args);
            if (nargs) {
                print_call_args(&node->data.call_expr.args, indent + 1);
            } else {
                print_indent(indent + 1);
                printf("Args: (none)\n");
            }
            break;
        }

        case AST_SUBSCRIPT_EXPR: {
            print_indent(indent);
            printf("Subscript:\n");
            if (node->data.subscript_expr.target) {
                print_indent(indent + 1);
                printf("Target:\n");
                print_ast(node->data.subscript_expr.target, indent + 2);
            }
            if (node->data.subscript_expr.index) {
                print_indent(indent + 1);
                printf("Index:\n");
                print_ast(node->data.subscript_expr.index, indent + 2);
            }
            break;
        }

        case AST_TYPE: {
            print_indent(indent);

            const char *base = node->data.type.base_type ? node->data.type.base_type : "(anon)";
            size_t pre = node->data.type.pre_stars;
            size_t post = node->data.type.post_stars;


            printf("Base-type: ");
            printf("%s", base);   // base-type
            for (size_t i = 0; i < pre; ++i) putchar('*');   /* pre-stars before base */


            /* array dimensions inline */
            size_t n = astnode_array_count(&node->data.type.sizes);
            for (size_t i = 0; i < n; ++i) {
                AstNode *dim = astnode_array_get(&node->data.type.sizes, i);
                putchar('[');
                if (dim) {
                   printf("dim %d", i);
                }
                putchar(']');
            }
        
            /* post-stars after arrays */
            for (size_t i = 0; i < post; ++i) putchar('*');
            printf("\n");
        
            /* --- Detailed breakdown (keeps clarity) --- */
            if (n) {
                print_indent(indent);
                printf("ArrayDims:\n");
                for (size_t i = 0; i < n; ++i) {
                    AstNode *dim = astnode_array_get(&node->data.type.sizes, i);
                    print_indent(indent + 1);
                    printf("Dim %zu:\n", i);
                    if (dim) {
                        print_ast(dim, indent + 2);
                    } else {
                        print_indent(indent + 2);
                        printf("(unspecified)\n");
                    }
                }
            }        
            break;
        }
        case AST_INITIALIZER_LIST: {
            print_indent(indent);
            printf("InitializerList:\n");
            size_t n = astnode_array_count(&node->data.initializer_list.elements);
            for (size_t i = 0; i < n; ++i) {
                AstNode *child = astnode_array_get(&node->data.initializer_list.elements, i);
                print_ast(child, indent + 1);
            }
            break;
        }

        default: {
            print_indent(indent);
            printf("Unhandled node type: %d\n", (int)node->type);
            break;
        }
    }
}

