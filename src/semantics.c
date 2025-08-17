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
    // base type
    Type *base = malloc(sizeof(*base));
    if (!base) { perror("malloc"); exit(EXIT_FAILURE); }
    base->kind = TYPE_PRIMITIVE;
    base->is_const = ast_node_type->base_is_const;
    base->primitive.name = strdup(ast_node_type->base_type);

    // pre-array pointers
    for (size_t i = 0; i < ast_node_type->pre_stars; i++) {
        Type *ptr = malloc(sizeof(*ptr));
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
        arr->kind = TYPE_ARRAY;
        arr->is_const = 0; // or propagate const if desired
        arr->array.of = base;

        // TODO: evaluate array size expression in typechecker
        arr->array.size = NULL;

        base = arr;
    }

    // post-array pointers
    for (size_t i = 0; i < ast_node_type->post_stars; i++) {
        Type *ptr = malloc(sizeof(*ptr));
        ptr->kind = TYPE_POINTER;
        ptr->is_const = 0;
        ptr->pointer.to = base;
        base = ptr;
    }

    return base;
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
            if (t->array.size == NULL) {
                printf("[]");
            } else {
                printf("[%zu]", t->array.size);
            }
            break;

        case TYPE_FUNCTION:
            type_print(t->function.return_type);
            printf("(");
            for (size_t i = 0; i < t->function.param_count; i++) {
                type_print(t->function.params[i]);
                if (i + 1 < t->function.param_count) {
                    printf(", ");
                }
            }
            printf(")");
            break;

        default:
            printf("<unknown type>");
            break;
    }
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