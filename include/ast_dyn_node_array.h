#pragma once

#include <stddef.h>
#include "dynamic_array.h"

/* Forward declaration to avoid circular includes */
typedef struct AstNode AstNode;

typedef struct {
    DynArray da;   /* element type = AstNode* */
} AstNodeArray;

/* Initialize the array. */
void astnode_array_init(AstNodeArray *arr);

/* Create and initialize a new AstNodeArray on the heap. Returns NULL on OOM. */
AstNodeArray *astnode_array_create(void);

/* Push a node pointer. Returns 0 on success, -1 on OOM. */
int astnode_array_push(AstNodeArray *arr, AstNode *node);

/* Pop last element (does not free the element). If you want pop+free,
 * call ast_node_free(astnode_array_get(...)) then astnode_array_pop(...). */
void astnode_array_pop(AstNodeArray *arr);

/* Number of elements. */
size_t astnode_array_count(AstNodeArray *arr);

/* Get element by index (no bounds check). */
AstNode *astnode_array_get(AstNodeArray *arr, size_t i);

/* Free the array and all stored AstNode* elements.
 * If you prefer NOT to free the contained nodes, use dynarray_free(&arr->da) directly.
 */
void astnode_array_free(AstNodeArray *arr);

/* Iteration macro:
 *   AstNode *node;
 *   ASTNODE_ARRAY_FOR_EACH(&arr, idx, node) { ... use node ... }
 */
#define ASTNODE_ARRAY_FOR_EACH(arr_ptr, idx_var, out_node_var)                    \
    for (size_t idx_var = 0;                                                     \
         idx_var < astnode_array_count(arr_ptr) && ((out_node_var = astnode_array_get(arr_ptr, idx_var)) || 1); \
         ++idx_var)
