#include "type_from_ast.h"
#include "utils.h"

/* -----------------------
 * AST -> Type conversion
 * --------------------- */

Type *asttype_to_type(AstType *ast_node_type) {
    if (!ast_node_type) return NULL;

    Type *base = NULL;

    if (ast_node_type->is_function) {
        /* FUNCTION TYPES: fn(params) -> return_type */
        
        /* Convert return type */
        Type *ret_type = NULL;
        if (ast_node_type->return_type && ast_node_type->return_type->node_type == AST_TYPE) {
            ret_type = asttype_to_type(&ast_node_type->return_type->data.ast_type);
        }
        
        /* Convert parameter types */
        size_t n_params = 0;
        Type **param_types = NULL;
        
        if (ast_node_type->param_types) {
            n_params = astnode_array_count(ast_node_type->param_types);
            if (n_params > 0) {
                param_types = xmalloc(n_params * sizeof(Type *));
                for (size_t i = 0; i < n_params; ++i) {
                    AstNode *param_node = astnode_array_get(ast_node_type->param_types, i);
                    if (param_node && param_node->node_type == AST_TYPE) {
                        param_types[i] = asttype_to_type(&param_node->data.ast_type);
                    } else {
                        param_types[i] = NULL;
                    }
                }
            }
        }
        
        base = type_make_function(ret_type, param_types, n_params, ast_node_type->base_is_const);
        
        /* Apply suffixes to the function type itself */
        for (size_t i = 0; i < ast_node_type->pre_stars; ++i) {
            base = type_make_pointer(base, 0);
        }
        if (ast_node_type->sizes) {
            size_t n = astnode_array_count(ast_node_type->sizes);
            for (size_t i = 0; i < n; ++i) {
                AstNode *child = astnode_array_get(ast_node_type->sizes, i);
                size_t arrsize = 0;
                
                /* Try to extract array size from expression (basic literal support) */
                if (child && child->node_type == AST_LITERAL && child->data.literal.value) {
                    arrsize = (size_t)atoi(child->data.literal.value);
                }
                base = type_make_array(base, arrsize, 0);
            }
        }
        for (size_t i = 0; i < ast_node_type->post_stars; ++i) {
            base = type_make_pointer(base, 0);
        }
    } else if (ast_node_type->base_type == NULL && ast_node_type->return_type != NULL) {
        /* GROUPED TYPES: (inner_type) with potential suffixes */
        
        if (ast_node_type->return_type->node_type == AST_TYPE) {
            /* Extract the inner type without applying any suffixes from the group level */
            base = asttype_to_type(&ast_node_type->return_type->data.ast_type);
        } else {
            /* Fallback for malformed grouped type */
            base = type_make_primitive("unknown", ast_node_type->base_is_const);
        }
        
        /* Apply the group's suffixes to the extracted inner type */
        for (size_t i = 0; i < ast_node_type->pre_stars; ++i) {
            base = type_make_pointer(base, 0);
        }
        if (ast_node_type->sizes) {
            size_t n = astnode_array_count(ast_node_type->sizes);
            for (size_t i = 0; i < n; ++i) {
                AstNode *child = astnode_array_get(ast_node_type->sizes, i);
                size_t arrsize = 0;
                
                /* Try to extract array size from expression (basic literal support) */
                if (child && child->node_type == AST_LITERAL && child->data.literal.value) {
                    arrsize = (size_t)atoi(child->data.literal.value);
                }
                /* For empty dimensions, use 0 to indicate unknown/dynamic size */
                base = type_make_array(base, arrsize, 0);
            }
        }
        for (size_t i = 0; i < ast_node_type->post_stars; ++i) {
            base = type_make_pointer(base, 0);
        }
    } else {
        /* REGULAR TYPES: base type with potential suffixes */
        
        const char *type_name = ast_node_type->base_type ? ast_node_type->base_type : "unknown";
        base = type_make_primitive(type_name, ast_node_type->base_is_const);
        for (size_t i = 0; i < ast_node_type->pre_stars; ++i) {
            base = type_make_pointer(base, 0);
        }
        if (ast_node_type->sizes) {
            size_t n = astnode_array_count(ast_node_type->sizes);
            for (size_t i = 0; i < n; ++i) {
                AstNode *child = astnode_array_get(ast_node_type->sizes, i);
                size_t arrsize = 0;
                
                /* Try to extract array size from expression (basic literal support) */
                if (child && child->node_type == AST_LITERAL && child->data.literal.value) {
                    arrsize = (size_t)atoi(child->data.literal.value);
                }
                base = type_make_array(base, arrsize, 0);
            }
        }
        for (size_t i = 0; i < ast_node_type->post_stars; ++i) {
            base = type_make_pointer(base, 0);
        }
    }

    return base;
}

Type *astfunction_to_type(AstFunctionDeclaration *ast_fn) {
    if (!ast_fn) return NULL;

    /* Convert return type */
    Type *ret = NULL;
    if (ast_fn->return_type) {
        ret = asttype_to_type(&ast_fn->return_type->data.ast_type);
    }

    /* Convert parameter types */
    size_t n = 0;
    if (ast_fn->params) {
        n = astnode_array_count(ast_fn->params);
    }
    
    if (n == 0) {
        return type_make_function(ret, NULL, 0, 0);
    }

    Type **params = xmalloc(n * sizeof(Type *));
    for (size_t i = 0; i < n; ++i) {
        AstNode *pnode = astnode_array_get(ast_fn->params, i);
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