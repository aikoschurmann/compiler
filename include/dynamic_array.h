#pragma once
#include <stddef.h>

typedef struct {
    void  *data;       /* contiguous buffer */
    size_t elem_size;  /* size of each element in bytes */
    size_t count;      /* number of elements stored */
    size_t capacity;   /* allocated capacity (elements) */
} DynArray;

/* Initialize dynarray to hold elements of size `elem_size`. */
void dynarray_init(DynArray *da, size_t elem_size);

/* Free internal buffer. Does NOT free elements stored if they are pointers — user must do that. */
void dynarray_free(DynArray *da);

/* Ensure capacity for at least min_capacity elements. Returns 0 on success, -1 on OOM. */
int dynarray_reserve(DynArray *da, size_t min_capacity);

/* Push a raw value by copying `elem_size` bytes from `value` into the array.
 * Returns 0 on success, -1 on OOM.
 */
int dynarray_push_value(DynArray *da, const void *value);

/* Append an uninitialized element and return pointer to the element slot (NULL on OOM).
 * Caller may write into the returned memory.
 */
void *dynarray_push_uninit(DynArray *da);

/* Remove last element (does not free element memory). If empty does nothing. */
void dynarray_pop(DynArray *da);

/* Access pointer to element at index. */
void *dynarray_get(DynArray *da, size_t index);

/* Remove element at index  Does not free element memory. */
void remove_element(DynArray *da, size_t index);

/* Convenience: number of elements */
static inline size_t dynarray_count(DynArray *da) { return da->count; }
