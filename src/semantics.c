#include "semantics.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

Type *type_make_primitive(char *name, int is_const) {
    Type *type = malloc(sizeof(*type));
    if (!type) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    type->kind = TYPE_PRIMITIVE;
    type->is_const = is_const;

    // Duplicate the string to avoid dangling pointers
    type->primitive.name = strdup(name);
    if (!type->primitive.name) {
        perror("strdup");
        free(type);
        exit(EXIT_FAILURE);
    }

    return type;
}

Type *type_make_pointer(Type *to, int is_const){
    Type *type = malloc(sizeof(*type));
    if (!type) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    type->kind = TYPE_POINTER;
    type->is_const = is_const;

    type->pointer.to = to;

    return type;
}

Type *type_make_array(Type *of, size_t size, int is_const){
    Type *type = malloc(sizeof(*type));
    if (!type) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    type->kind = TYPE_ARRAY;
    type->is_const = is_const;

    type->array.of = of;
    type->array.size = size;

    return type;
}


Type *type_make_function(Type *return_type, Type **params, size_t param_count, int is_const){
    Type *type = malloc(sizeof(*type));
    if (!type) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    type->kind = TYPE_FUNCTION;
    type->is_const = is_const;
    type->function.return_type = return_type;

    type->function.param_count = param_count;
    type->function.params = params;

    return type;
}

Type *asttype_to_type(AstType *ast_node_type) {
    if (!ast_node_type) return NULL;
    
    // base type
    Type *base = malloc(sizeof(*base));
    if (!base) { perror("malloc"); exit(EXIT_FAILURE); }
    base->kind = TYPE_PRIMITIVE;
    base->is_const = ast_node_type->base_is_const;
    base->primitive.name = strdup(ast_node_type->base_type);

    // pre-array pointers
    for (size_t i = 0; i < ast_node_type->pre_stars; i++) {
        Type *ptr = malloc(sizeof(*ptr));
        if (!ptr) { perror("malloc"); exit(EXIT_FAILURE); }
        ptr->kind = TYPE_POINTER;
        ptr->is_const = 0; // or propagate const if you want "all-or-nothing"
        ptr->pointer.to = base;
        base = ptr;
    }

    // arrays
    size_t n = astnode_array_count(&ast_node_type->sizes);
    for (size_t i = 0; i < n; ++i) {
        AstNode *child = astnode_array_get(&ast_node_type->sizes, i);

        Type *arr = malloc(sizeof(*arr));
        if (!arr) { perror("malloc"); exit(EXIT_FAILURE); }
        arr->kind = TYPE_ARRAY;
        arr->is_const = 0; // or propagate const if desired
        arr->array.of = base;

        // Extract size from literal node if available
        if (child && child->type == AST_LITERAL && child->data.literal.value) {
            arr->array.size = (size_t)atoi(child->data.literal.value);
        } else {
            arr->array.size = 0; // Unknown size
        }

        base = arr;
    }

    // post-array pointers
    for (size_t i = 0; i < ast_node_type->post_stars; i++) {
        Type *ptr = malloc(sizeof(*ptr));
        if (!ptr) { perror("malloc"); exit(EXIT_FAILURE); }
        ptr->kind = TYPE_POINTER;
        ptr->is_const = 0;
        ptr->pointer.to = base;
        base = ptr;
    }

    return base;
}


Type *astfunction_to_type(AstFunctionDeclaration *ast_fn) {
    if (!ast_fn) return NULL;

    Type *t = malloc(sizeof(*t));
    if (!t) { perror("malloc"); exit(EXIT_FAILURE); }

    t->kind = TYPE_FUNCTION;
    t->is_const = 0; /* function types rarely carry const in typical designs */

    /* return type */
    if (ast_fn->return_type) {
        t->function.return_type = asttype_to_type(&ast_fn->return_type->data.type);
    } else {
        /* default to void or report error depending on your language */
        t->function.return_type = NULL;
    }

    /* params */
    size_t n = astnode_array_count(&ast_fn->params);
    t->function.param_count = n;

    if (n == 0) {
        t->function.params = NULL;
        return t;
    }

    /* allocate array for param Type* pointers */
    t->function.params = malloc(n * sizeof(Type *));
    if (!t->function.params) { perror("malloc"); exit(EXIT_FAILURE); }

    for (size_t i = 0; i < n; ++i) {
        AstNode *param_node = astnode_array_get(&ast_fn->params, i);
        if (param_node && param_node->type == AST_PARAM) {
            AstNode *param_type_node = param_node->data.param.type;
            if (param_type_node && param_type_node->type == AST_TYPE) {
                t->function.params[i] = asttype_to_type(&param_type_node->data.type);
            } else {
                t->function.params[i] = NULL;
            }
        } else {
            t->function.params[i] = NULL;
        }
    }

    return t;
}



void type_print(Type *t) {
    if (!t) {
        printf("NULL");
        return;
    }

    // print const qualifier
    if (t->is_const) {
        printf("const ");
    }

    switch (t->kind) {
        case TYPE_PRIMITIVE:
            printf("%s", t->primitive.name);
            break;

        case TYPE_POINTER:
            type_print(t->pointer.to);
            printf("*");
            break;

        case TYPE_ARRAY:
            type_print(t->array.of);
            if (t->array.size == 0) {
                printf("[]");
            } else {
                printf("[%zu]", t->array.size);
            }
            break;

        case TYPE_FUNCTION:
            
            printf("(");
            for (size_t i = 0; i < t->function.param_count; i++) {
                type_print(t->function.params[i]);
                if (i + 1 < t->function.param_count) {
                    printf(", ");
                }
            }
            printf(")");
            if(t->function.return_type){
                printf(" -> ");
                type_print(t->function.return_type);

            }
            break;

        default:
            printf("<unknown type>");
            break;
    }
}

char *type_to_string(Type *t) {
    if (!t) return strdup("(null)");
    
    char *result = malloc(512);  // Fixed buffer for now
    if (!result) return NULL;
    
    result[0] = '\0';  // Start with empty string
    
    if (t->is_const) {
        strcat(result, "const ");
    }
    
    switch (t->kind) {
        case TYPE_PRIMITIVE:
            strcat(result, t->primitive.name);
            break;
            
        case TYPE_POINTER: {
            char *inner = type_to_string(t->pointer.to);
            if (inner) {
                strcat(result, inner);
                strcat(result, "*");
                free(inner);
            }
            break;
        }
        
        case TYPE_ARRAY: {
            char *inner = type_to_string(t->array.of);
            if (inner) {
                strcat(result, inner);
                if (t->array.size == 0) {
                    strcat(result, "[]");
                } else {
                    char size_buf[32];
                    snprintf(size_buf, sizeof(size_buf), "[%zu]", t->array.size);
                    strcat(result, size_buf);
                }
                free(inner);
            }
            break;
        }
        
        case TYPE_FUNCTION:
            strcat(result, "function");
            break;
            
        default:
            strcat(result, "<unknown>");
            break;
    }
    
    return result;
}

void type_free(Type *t) {
    if (!t) return;

    switch (t->kind) {
        case TYPE_PRIMITIVE:
            free(t->primitive.name);
            break;

        case TYPE_POINTER:
            type_free(t->pointer.to);
            break;

        case TYPE_ARRAY:
            type_free(t->array.of);
            break;

        case TYPE_FUNCTION:
            type_free(t->function.return_type);
            if (t->function.params) {
                for (size_t i = 0; i < t->function.param_count; i++) {
                    type_free(t->function.params[i]);
                }
                free(t->function.params);
            }
            break;

        default:
            break;
    }

    free(t);
}

/* Generic string hash (djb2) and comparator adapted to HashMap's void* API */
static size_t str_hash(void *key) {
    const unsigned char *s = (const unsigned char *)key;
    if (!s) return 0;
    unsigned long h = 5381;
    int c;
    while ((c = *s++))
        h = ((h << 5) + h) + c; /* h * 33 + c */
    return (size_t)h;
}

static int str_cmp(void *a, void *b) {
    if (a == b) return 0;
    if (!a) return -1;
    if (!b) return 1;
    return strcmp((const char*)a, (const char*)b);
}

/* Ensure the scope has initialized maps for functions and variables */
static int ensure_scope_maps(Scope *scope) {
    if (!scope) return -1;
    if (!scope->functions) {
        scope->functions = hashmap_create(128);
        if (!scope->functions) return -1;
    }
    if (!scope->variables) {
        scope->variables = hashmap_create(128);
        if (!scope->variables) {
            /* If functions was created by us, leave it — caller should destroy scope later */
            return -1;
        }
    }
    return 0;
}

/*
 * Walk the top-level declarations and register function signatures in global_scope->functions.
 * Returns 0 on success, non-zero on error.
 */
int symbol_table_construction(Scope *global_scope, AstProgram *program) {
    if (!global_scope || !program) {
        fprintf(stderr, "symbol_table_construction: null arguments\n");
        return -1;
    }

    if (ensure_scope_maps(global_scope) != 0) {
        fprintf(stderr, "symbol_table_construction: failed to allocate hash maps\n");
        return -1;
    }


    size_t n = astnode_array_count(&program->decls);
    for (size_t i = 0; i < n; ++i) {
        AstNode *decl = astnode_array_get(&program->decls, i);
        if (!decl) continue;

        switch (decl->type) {
            case AST_FUNCTION_DECLARATION: {
                AstFunctionDeclaration *fd = &decl->data.function_declaration;
                const char *name = fd->name;
                if (!name) {
                    fprintf(stderr, "symbol_table_construction: function with no name at index %zu\n", i);
                    return -1;
                }

                /* Check for duplicate */
                void *existing = hashmap_get(
                    global_scope->functions,
                    (void*)name,
                    str_hash,
                    str_cmp
                );
                if (existing) {
                    fprintf(stderr, "symbol_table_construction: duplicate function '%s'\n", name);
                    return -1;
                }

                /* Build the function Type* (ownership transferred to symbol table on success) */
                Type *ftype = astfunction_to_type(fd);
                if (!ftype) {
                    fprintf(stderr, "symbol_table_construction: failed to create type for function '%s'\n", name);
                    return -1;
                }

                /* Duplicate the key (HashMap stores keys by pointer, so duplicate to own it) */
                char *key_dup = strdup(name);
                if (!key_dup) {
                    perror("strdup");
                    type_free(ftype);
                    return -1;
                }

                bool ok = hashmap_put(
                    global_scope->functions,
                    (void*)key_dup,
                    (void*)ftype,
                    str_hash,
                    str_cmp
                );

                if (!ok) {
                    fprintf(stderr, "symbol_table_construction: hashmap_put failed for '%s'\n", name);
                    free(key_dup);
                    type_free(ftype);
                    return -1;
                }

                break;
            }

            /* If you want to register top-level variables/globals, add a case here:
               case AST_GLOBAL_VARIABLE_DECLARATION: { ... }
               Use asttype_to_type on the variable type and insert into global_scope->variables.
            */

            default:
                /* ignore other top-level declarations for now */
                break;
        }
    }

    return 0;
}


/* Helper: print indentation */
static void print_indent(int indent) {
    for (int i = 0; i < indent; ++i) putchar(' ');
}

static void hashmap_print_entries(HashMap *map, int indent) {
    if (!map) {
        print_indent(indent);
        printf("<none>\n");
        return;
    }

    for (size_t b = 0; b < map->bucket_count; ++b) {
        DynArray *bucket = &map->buckets[b];
        for (size_t j = 0; j < bucket->count; ++j) {
            KeyValue *kv = (KeyValue*)dynarray_get(bucket, j);
            char *key = (char*)kv->key;
            Type *val = (Type*)kv->value;

            print_indent(indent);
            if (key) printf("%s : ", key);
            else printf("<anon> : ");

            if (val) {
                type_print(val);   /* <-- uses your implementation */
            } else {
                printf("<NULL-type>");
            }
            printf("\n");
        }
    }
}

/* Public: print a single scope and its parents */
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
        hashmap_print_entries(s->functions, indent + 4);

        print_indent(indent + 2);
        printf("Variables:\n");
        hashmap_print_entries(s->variables, indent + 4);

        s = s->parent;
        indent += 2;
        if (s) {
            print_indent(indent);
            printf("Parent ->\n");
            indent += 2;
        }
    }
}

/* Free a scope's maps (keys copied with strdup; values are Type* and
   should be freed with type_free). This is safe if symbol_table_construction
   used strdup for keys and stored Type* values. */
void free_scope_maps(Scope *s) {
    if (!s) return;

    if (s->functions) {
        /* keys are char* (strdup'd) -> free with free
           values are Type* -> free with type_free (declared in semantics.h) */
        hashmap_destroy(s->functions,
                        (void(*)(void*))free,
                        (void(*)(void*))type_free);
        s->functions = NULL;
    }

    if (s->variables) {
        hashmap_destroy(s->variables,
                        (void(*)(void*))free,
                        (void(*)(void*))type_free);
        s->variables = NULL;
    }
}
