#include "ast_dyn_node_array.h"
#include "ast.h"


/* Initialize the array. */
void astnode_array_init(AstNodeArray *arr) {
    dynarray_init(&arr->da, sizeof(AstNode *));
}

/* Push a node pointer. Returns 0 on success, -1 on OOM. */
int astnode_array_push(AstNodeArray *arr, AstNode *node) {
    return dynarray_push_value(&arr->da, &node);
}

/* Pop last element (does not free the element). If you want pop+free,
 * call ast_node_free(astnode_array_get(...)) then astnode_array_pop(...). */
void astnode_array_pop(AstNodeArray *arr) {
    dynarray_pop(&arr->da);
}

/* Number of elements. */
size_t astnode_array_count(AstNodeArray *arr) {
    return dynarray_count((DynArray*)&arr->da);
}

/* Get element by index (no bounds check). */
AstNode *astnode_array_get(AstNodeArray *arr, size_t i) {
    return *(AstNode **)dynarray_get((DynArray*)&arr->da, i);
}

/* Free the array and all stored AstNode* elements.
 * If you prefer NOT to free the contained nodes, use dynarray_free(&arr->da) directly.
 */
void astnode_array_free(AstNodeArray *arr) {
    size_t n = astnode_array_count(arr);
    for (size_t i = 0; i < n; ++i) {
        AstNode *node = astnode_array_get(arr, i);
        if (node) ast_node_free(node);
    }
    dynarray_free(&arr->da);
}

