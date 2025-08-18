#include "scope.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>


static void free_symbol(Symbol *sym) {
    if (!sym) return;
    if (sym->sem_type) type_free(sym->sem_type);
    free(sym); /* sym->name not freed (owned by AST or duplicated elsewhere) */
}

static size_t str_hash(void *key) {
    const unsigned char *s = (const unsigned char *)key;
    if (!s) return 0;
    unsigned long h = 5381;
    int c;
    while ((c = *s++))
        h = ((h << 5) + h) + c;
    return (size_t)h;
}

static int str_cmp(void *a, void *b) {
    if (a == b) return 0;
    if (!a) return -1;
    if (!b) return 1;
    return strcmp((const char*)a, (const char*)b);
}

/* Ensure maps exist */
static int ensure_scope_maps(Scope *scope) {
    if (!scope) return -1;
    if (!scope->functions) {
        scope->functions = symbol_table_create(128);
        if (!scope->functions) return -1;
    }
    if (!scope->variables) {
        scope->variables = symbol_table_create(128);
        if (!scope->variables) {
            return -1;
        }
    }
    return 0;
}


static void print_indent(int indent) {
    for (int i = 0; i < indent; ++i) putchar(' ');
}

static void hashmap_print_entries(HashMap *map, int indent) {
    if (!map) {
        print_indent(indent);
        printf("hashmap_print_entries empty hashmap");
        return;
    }

    for (size_t b = 0; b < map->bucket_count; ++b) {
        DynArray *bucket = &map->buckets[b];
        for (size_t j = 0; j < bucket->count; ++j) {
            KeyValue *kv = (KeyValue*)dynarray_get(bucket, j);
            char *key = (char*)kv->key;
            Symbol *sym = (Symbol*)kv->value;

            print_indent(indent);
            if (key) printf("%s : ", key);
            else printf("<anon> : ");

            if (sym && sym->sem_type) {
                type_print(sym->sem_type);
            } else {
                printf("<NULL-symbol>");
            }
            printf("\n");
        }
    }
}

void scope_print(Scope *scope) {
    if (!scope) {
        printf("<null-scope>\n");
        return;
    }
    int indent = 0;
    Scope *s = scope;
    while (s) {
        print_indent(indent);
        printf("globalScope\n");

        print_indent(indent + 2);
        printf("Functions:\n");
        hashmap_print_entries(s->functions ? s->functions->table : NULL, indent + 4);

        print_indent(indent + 2);
        printf("Variables:\n");
        hashmap_print_entries(s->variables ? s->variables->table : NULL, indent + 4);

        s = s->parent;
        indent += 2;
        if (s) {
            print_indent(indent);
            printf("Parent ->\n");
            indent += 2;
        }
    }
}

void free_scope_maps(Scope *s) {
    if (!s) return;
    if (s->functions) {
        s->functions->free_value = (void(*)(Symbol*))free_symbol;
        symbol_table_destroy(s->functions);
        s->functions = NULL;
    }
    if (s->variables) {
        s->variables->free_value = (void(*)(Symbol*))free_symbol;
        symbol_table_destroy(s->variables);
        s->variables = NULL;
    }
}

/* symbol_table_construction: register top-level functions */
int symbol_table_construction(Scope *global_scope, AstProgram *program) {
    if (!global_scope || !program) {
        fprintf(stderr, "symbol_table_construction: null arguments\n");
        return -1;
    }
    if (ensure_scope_maps(global_scope) != 0) {
        fprintf(stderr, "symbol_table_construction: failed to allocate symbol tables\n");
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
                    fprintf(stderr, "symbol_table_construction: function with no name at index %zu\n", i);
                    return -1;
                }

                if (symbol_table_get(global_scope->functions, name)) {
                    fprintf(stderr, "symbol_table_construction: duplicate function '%s'\n", name);
                    return -1;
                }

                Type *ftype = astfunction_to_type(fd);
                if (!ftype) {
                    fprintf(stderr, "symbol_table_construction: failed to create type for function '%s'\n", name);
                    return -1;
                }
                Symbol *sym = xmalloc(sizeof(*sym));
                sym->name = (char*)name; /* not owning */
                sym->sem_type = ftype;
                sym->is_const_expr = 0;

                if (!symbol_table_put(global_scope->functions, name, sym)) {
                    fprintf(stderr, "symbol_table_construction: symbol_table_put failed for '%s'\n", name);
                    type_free(ftype);
                    free(sym);
                    return -1;
                }
                break;
            }

            case AST_VARIABLE_DECLARATION: {
                AstVariableDeclaration *vd = &decl->data.variable_declaration;
                const char *name = vd->name;
                if (!name) {
                    fprintf(stderr, "symbol_table_construction: variable with no name at index %zu\n", i);
                    return -1;
                }

                if (symbol_table_get(global_scope->variables, name)) {
                    fprintf(stderr, "symbol_table_construction: duplicate variable '%s'\n", name);
                    return -1;
                }

                Type *vtype = asttype_to_type(&vd->type->data.ast_type);
                if (!vtype) {
                    fprintf(stderr, "symbol_table_construction: failed to create type for variable '%s'\n", name);
                    return -1;
                }
                Symbol *sym = xmalloc(sizeof(*sym));
                sym->name = (char*)name; /* not owning */
                sym->sem_type = vtype;
                sym->is_const_expr = 0;

                if (!symbol_table_put(global_scope->variables, name, sym)) {
                    fprintf(stderr, "symbol_table_construction: symbol_table_put failed for '%s'\n", name);
                    type_free(vtype);
                    free(sym);
                    return -1;
                }
                break;
            }

            default:
                break;
        }
    }
    return 0;
}
