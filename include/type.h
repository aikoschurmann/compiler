#pragma once

#include <stddef.h>

/* Minimal Type definition. Does NOT include ast.h to avoid circular includes. */

typedef enum {
    TYPE_PRIMITIVE,
    TYPE_POINTER,
    TYPE_ARRAY,
    TYPE_FUNCTION,
} TypeKind;

typedef struct Type Type;

typedef struct { char *name; } TypePrimitive;
typedef struct { Type *to; } TypePointer;
typedef struct { Type *of; size_t size; } TypeArray;
typedef struct { Type *return_type; Type **params; size_t param_count; } TypeFunction;

struct Type {
    TypeKind kind;
    int is_const;   /* type-level const qualifier */
    union {
        TypePrimitive primitive;
        TypePointer   pointer;
        TypeArray     array;
        TypeFunction  function;
    };
};

/* creation / helpers */
Type *type_make_primitive(const char *name, int is_const);
Type *type_make_pointer(Type *to, int is_const);
Type *type_make_array(Type *of, size_t size, int is_const);
Type *type_make_function(Type *return_type, Type **params, size_t param_count, int is_const);

void type_print(Type *t);
void type_print_hierarchical(Type *t);
void type_print_hierarchical_with_indent(Type *t, int base_indent);
char *type_to_string(Type *t);
void type_free(Type *t);
