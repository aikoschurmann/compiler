#include "semantic_analysis.h"
#include "scope.h"
#include "utils.h"


/* scope_fill_function_declarations: register top-level functions */
int scope_fill_function_declarations(Scope *global_scope, AstProgram *program) {
    if (!global_scope || !program) {
        fprintf(stderr, "scope_fill_function_declarations: null arguments\n");
        return -1;
    }
    if (ensure_scope_maps(global_scope) != 0) {
        fprintf(stderr, "scope_fill_function_declarations: failed to allocate symbol tables\n");
        return -1;
    }

    size_t n = astnode_array_count(program->decls);
    for (size_t i = 0; i < n; ++i) {
        AstNode *decl = astnode_array_get(program->decls, i);
        if (!decl) continue;
        switch (decl->node_type)
        {
            case AST_FUNCTION_DECLARATION: {

                AstFunctionDeclaration *fd = &decl->data.function_declaration;
                const char *name = fd->name;
                if (!name) {
                    fprintf(stderr, "scope_fill_function_declarations: function with no name at index %zu\n", i);
                    return -1;
                }

                if (symbol_table_get(global_scope->functions, name)) {
                    fprintf(stderr, "scope_fill_function_declarations: duplicate function '%s'\n", name);
                    return -1;
                }

                Type *ftype = astfunction_to_type(fd);
                if (!ftype) {
                    fprintf(stderr, "scope_fill_function_declarations: failed to create type for function '%s'\n", name);
                    return -1;
                }
                Symbol *sym = xmalloc(sizeof(*sym));
                sym->name = (char*)name; /* not owning */
                sym->sem_type = ftype;
                sym->is_const_expr = 0;

                if (!symbol_table_put(global_scope->functions, name, sym)) {
                    fprintf(stderr, "scope_fill_function_declarations: symbol_table_put failed for '%s'\n", name);
                    type_free(ftype);
                    free(sym);
                    return -1;
                }
                break;
            }

            //case AST_VARIABLE_DECLARATION: {
            //    AstVariableDeclaration *vd = &decl->data.variable_declaration;
            //    const char *name = vd->name;
            //    if (!name) {
            //        fprintf(stderr, "scope_fill_function_declarations: variable with no name at index %zu\n", i);
            //        return -1;
            //    }
//
            //    if (symbol_table_get(global_scope->variables, name)) {
            //        fprintf(stderr, "scope_fill_function_declarations: duplicate variable '%s'\n", name);
            //        return -1;
            //    }
//
            //    Type *vtype = asttype_to_type(&vd->type->data.ast_type);
            //    if (!vtype) {
            //        fprintf(stderr, "scope_fill_function_declarations: failed to create type for variable '%s'\n", name);
            //        return -1;
            //    }
            //    Symbol *sym = xmalloc(sizeof(*sym));
            //    sym->name = (char*)name; /* not owning */
            //    sym->sem_type = vtype;
            //    sym->is_const_expr = 0;
//
            //    if (!symbol_table_put(global_scope->variables, name, sym)) {
            //        fprintf(stderr, "scope_fill_function_declarations: symbol_table_put failed for '%s'\n", name);
            //        type_free(vtype);
            //        free(sym);
            //        return -1;
            //    }
            //    break;
            //}

            default:
                break;
        }
    }
    return 0;
}


// expects a prior call to scope_fill_function_declarations
// function declarations are already in the global scope
int semantic_analysis_run(Scope *scope, AstNode *node) {
    if (!scope || !node) {
        fprintf(stderr, "semantic_analysis_run: null arguments\n");
        return -1;
    }

    switch (node->node_type) {
        case AST_PROGRAM: {
            AstProgram *program = &node->data.program;
            size_t n = astnode_array_count(program->decls);
            for (size_t i = 0; i < n; ++i) {
                AstNode *decl = astnode_array_get(program->decls, i);
                if (!decl) continue;
                if (semantic_analysis_run(scope, decl) != 0) {
                    fprintf(stderr, "semantic_analysis_run: failed to analyze declaration at index %zu\n", i);
                    return -1;
                }
            }
            break;
        }
        case AST_FUNCTION_DECLARATION: {
            // later open scope for function body
            // and analyze the body
            break;
        }

        case AST_VARIABLE_DECLARATION: {
            AstVariableDeclaration *vd = &node->data.variable_declaration;
            const char *name = vd->name;
            if (!name) {
                fprintf(stderr, "semantic_analysis_run: variable with no name\n");
                return -1;
            }

            if (symbol_table_get(scope->variables, name)) {
                fprintf(stderr, "semantic_analysis_run: duplicate variable '%s'\n", name);
                return -1;
            }

            Type *vtype = asttype_to_type(&vd->type->data.ast_type);
            if (!vtype) {
                fprintf(stderr, "semantic_analysis_run: failed to create type for variable '%s'\n", name);
                return -1;
            }
            Symbol *sym = xmalloc(sizeof(*sym));
            sym->name = (char*)name; /* not owning */
            sym->sem_type = vtype;
            sym->is_const_expr = 0;

            if (!symbol_table_put(scope->variables, name, sym)) {
                fprintf(stderr, "semantic_analysis_run: symbol_table_put failed for '%s'\n", name);
                type_free(vtype);
                free(sym);
                return -1;
            }

            // If the variable has an initializer, analyze it
            if (vd->initializer) {
                if (semantic_analysis_run(scope, vd->initializer) != 0) {
                    fprintf(stderr, "semantic_analysis_run: failed to analyze initializer for variable '%s'\n", name);
                    return -1;
                }
            }
            break;
        }

        // Handle other AST node types as needed
        default:
            fprintf(stderr, "semantic_analysis_run: unsupported AST node type %d\n", node->node_type);
            return -1;
    }


    return 0;
}