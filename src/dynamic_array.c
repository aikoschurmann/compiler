#include "dynamic_array.h"
#include <stdlib.h>
#include <string.h>

void dynarray_init(DynArray *da, size_t elem_size) {
    da->data = NULL;
    da->elem_size = elem_size;
    da->count = 0;
    da->capacity = 0;
}

void dynarray_free(DynArray *da) {
    if (!da) return;
    free(da->data);
    da->data = NULL;
    da->count = 0;
    da->capacity = 0;
    da->elem_size = 0;
}

int dynarray_reserve(DynArray *da, size_t min_capacity) {
    if (da->capacity >= min_capacity) return 0;
    size_t newcap = da->capacity ? da->capacity * 2 : 4;
    while (newcap < min_capacity) newcap *= 2;
    void *newbuf = realloc(da->data, newcap * da->elem_size);
    if (!newbuf) return -1;
    da->data = newbuf;
    da->capacity = newcap;
    return 0;
}

int dynarray_push_value(DynArray *da, const void *value) {
    if (dynarray_reserve(da, da->count + 1) != 0) return -1;
    void *dst = (char*)da->data + da->count * da->elem_size;
    da->count += 1;
    if (value) {
        memcpy(dst, value, da->elem_size);
    } else {
        memset(dst, 0, da->elem_size);
    }
    return 0;
}

void *dynarray_push_uninit(DynArray *da) {
    if (dynarray_reserve(da, da->count + 1) != 0) return NULL;
    void *slot = (char*)da->data + da->count * da->elem_size;
    da->count += 1;
    return slot;
}

void dynarray_pop(DynArray *da) {
    if (da->count == 0) return;
    da->count -= 1;
    // Do NOT free individual element — user owns memory
}

void remove_element(DynArray *da, size_t index) {
    if (index >= da->count) return; // bounds check
    if (index < da->count - 1) {
        void *dst = (char*)da->data + index * da->elem_size;
        void *src = (char*)da->data + (index + 1) * da->elem_size;
        size_t n = da->count - index - 1;
        memmove(dst, src, n * da->elem_size);
    }
    da->count -= 1;
}

// optional helpers
void *dynarray_get(DynArray *da, size_t index) {
    if (index >= da->count || index < 0) {
        return NULL; // out of bounds
    }
    return (char*)da->data + index * da->elem_size;
}

int dynarray_set(DynArray *da, size_t index, void *value) {
    if (index >= da->count) return -1;
    void *dst = (char*)da->data + index * da->elem_size;
    memcpy(dst, value, da->elem_size);
    return 0;
}
