#pragma once

#include <stddef.h>
#include "ast.h"
#include "dynamic_array.h"
#include "hash_map.h"

typedef enum {
    TYPE_PRIMITIVE,
    TYPE_POINTER,
    TYPE_ARRAY,
    TYPE_FUNCTION,
} TypeKind;

typedef struct Type Type;

typedef struct {
    char *name;
} TypePrimitive;

typedef struct {
    Type *to;
} TypePointer;

typedef struct {
    Type *of;
    size_t size; /* 0 => unspecified */
} TypeArray;

typedef struct {
    Type *return_type;
    Type **params;
    size_t param_count;
} TypeFunction;

struct Type {
    TypeKind kind;
    int is_const;
    union {
        TypePrimitive primitive;
        TypePointer   pointer;
        TypeArray     array;
        TypeFunction  function;
    };
};

/* creation */
Type *type_make_primitive(const char *name, int is_const);
Type *type_make_pointer(Type *to, int is_const);
Type *type_make_array(Type *of, size_t size, int is_const);
Type *type_make_function(Type *return_type, Type **params, size_t param_count, int is_const);

/* AST -> Type */
Type *asttype_to_type(AstType *ast_node_type);
Type *astfunction_to_type(AstFunctionDeclaration *ast_fn);

/* utilities */
int type_equals(Type *a, Type *b);
int type_compatible(Type *a, Type *b);

/* printing */
void type_print(Type *t);
char *type_to_string(Type *t);

/* memory */
void type_free(Type *t);