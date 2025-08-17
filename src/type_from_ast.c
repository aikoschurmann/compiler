#include "type_from_ast.h"
#include "utils.h"

/* -----------------------
 * AST -> Type conversion
 * --------------------- */

Type *asttype_to_type(AstType *ast_node_type) {
    if (!ast_node_type) return NULL;

    /* start from base */
    Type *base = type_make_primitive(
        ast_node_type->base_type ? ast_node_type->base_type : "(anon)",
        ast_node_type->base_is_const
    );

    /* pre-stars (e.g., **int) */
    for (size_t i = 0; i < ast_node_type->pre_stars; ++i) {
        base = type_make_pointer(base, 0);
    }

    /* arrays (left-to-right in sizes array) */
    size_t n = astnode_array_count(&ast_node_type->sizes);
    for (size_t i = 0; i < n; ++i) {
        AstNode *child = astnode_array_get(&ast_node_type->sizes, i);
        size_t arrsize = 0;
        if (child && child->node_type == AST_LITERAL && child->data.literal.value) {
            arrsize = (size_t)atoi(child->data.literal.value);
        }
        base = type_make_array(base, arrsize, 0);
    }

    /* post-stars (e.g., int *[] * ) */
    for (size_t i = 0; i < ast_node_type->post_stars; ++i) {
        base = type_make_pointer(base, 0);
    }

    return base;
}

Type *astfunction_to_type(AstFunctionDeclaration *ast_fn) {
    if (!ast_fn) return NULL;

    Type *ret = NULL;
    if (ast_fn->return_type) {
        ret = asttype_to_type(&ast_fn->return_type->data.ast_type);
    }

    size_t n = astnode_array_count(&ast_fn->params);
    if (n == 0) {
        return type_make_function(ret, NULL, 0, 0);
    }

    Type **params = xmalloc(n * sizeof(Type *));
    for (size_t i = 0; i < n; ++i) {
        AstNode *pnode = astnode_array_get(&ast_fn->params, i);
        if (pnode && pnode->node_type == AST_PARAM) {
            AstNode *ptype = pnode->data.param.type;
            if (ptype && ptype->node_type == AST_TYPE) {
                params[i] = asttype_to_type(&ptype->data.ast_type);
            } else {
                params[i] = NULL;
            }
        } else {
            params[i] = NULL;
        }
    }

    return type_make_function(ret, params, n, 0);
}