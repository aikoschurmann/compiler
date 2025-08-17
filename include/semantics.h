#pragma once

#include "hash_map.h"
#include "dynamic_array.h"
#include "ast.h"

typedef enum {
    TYPE_PRIMITIVE,
    TYPE_POINTER,
    TYPE_ARRAY,
    TYPE_FUNCTION,
} TypeKind;

/* === Individual type payloads === */

typedef struct {
    char *name;  // "i32", "f64"
} TypePrimitive;

typedef struct Type Type;  // forward-declare self for recursion

typedef struct {
    Type *to;
} TypePointer;

typedef struct {
    Type *of;
    size_t size;   // or SIZE_UNKNOWN for []
} TypeArray;

typedef struct {
    Type *return_type;
    Type **params;
    size_t param_count;
} TypeFunction;

/* === Unified type === */

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

/* === Scope === */



typedef struct Scope {
    HashMap *variables;
    HashMap *functions;
    struct Scope *parent;
} Scope;

// Prototypes

Type *type_make_primitive(char *name, int is_const);
Type *type_make_pointer(Type *to, int is_const);
Type *type_make_array(Type *of, size_t size, int is_const);
Type *type_make_function(Type *return_type, Type **params, size_t param_count, int is_const);


Type *asttype_to_type(AstType *ast_node_type);
Type *astfunction_to_type(AstFunctionDeclaration *ast_fn);



// type comparison
int type_equals(Type *a, Type *b);
int type_compatible(Type *a, Type *b);

// debugging / printing
void type_print(Type *t);
char *type_to_string(Type *t);

// memory management
void type_free(Type *t);

int symbol_table_construction(Scope *global_scope, AstProgram *program);

void scope_print(Scope *scope);

void free_scope_maps(Scope *s);