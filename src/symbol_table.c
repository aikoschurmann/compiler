#include "symbol_table.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>


/* djb2 string hash (returns size_t). Accepts char* */
static size_t default_str_hash(char* s) {
    if (!s) return 0;
    unsigned long hash = 5381;
    int c;
    while ((c = (unsigned char)*s++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    return (size_t)hash;
}

/* strcmp-style comparator: 0 if equal */
static int default_str_cmp(char* a, char* b) {
    if (a == b) return 0;
    if (!a) return -1;
    if (!b) return 1;
    return strcmp(a, b);
}


SymbolTable* symbol_table_create(size_t bucket_count) {
    SymbolTable *st = malloc(sizeof(*st));
    if (!st) return NULL;

    st->table = hashmap_create(bucket_count ? bucket_count : 16);
    if (!st->table) {
        free(st);
        return NULL;
    }

    /* sensible defaults (caller can override) */
    st->free_key = free;       /* strdup'd keys are freed by default */
    st->free_value = NULL;     /* no automatic free for values unless caller sets it */
    st->hash = default_str_hash;
    st->cmp = default_str_cmp;

    return st;
}

void symbol_table_destroy(SymbolTable* table) {
    if (!table) return;

    /* cast free callbacks to void(*)(void*) for hashmap_destroy */
    void (*fk)(void*) = table->free_key ? (void(*)(void*))table->free_key : NULL;
    void (*fv)(void*) = table->free_value ? (void(*)(void*))table->free_value : NULL;

    hashmap_destroy(table->table, fk, fv);
    free(table);
}

bool symbol_table_put(SymbolTable* table, const char* key, Type* value) {
    if (!table || !key) return false;

    /* duplicate key so caller keeps ownership of their original string */
    char *dup = strdup(key);
    if (!dup) return false;

    bool ok = hashmap_put(
        table->table,
        (void*)dup,
        (void*)value,
        (size_t (*)(void*)) table->hash,
        (int (*)(void*, void*)) table->cmp
    );

    if (!ok) {
        free(dup);
    }
    return ok;
}

Type* symbol_table_get(SymbolTable* table, const char* key) {
    if (!table || !key) return NULL;

    void *v = hashmap_get(
        table->table,
        (void*)key,
        (size_t (*)(void*)) table->hash,
        (int (*)(void*, void*)) table->cmp
    );
    return (Type*)v;
}

bool symbol_table_remove(SymbolTable* table, const char* key) {
    if (!table || !key) return false;

    return hashmap_remove(
        table->table,
        (void*)key,
        (size_t (*)(void*)) table->hash,
        (int (*)(void*, void*)) table->cmp,
        table->free_key ? (void(*)(void*))table->free_key : NULL,
        table->free_value ? (void(*)(void*))table->free_value : NULL
    );
}

bool symbol_table_rehash(SymbolTable* table, size_t new_bucket_count) {
    if (!table) return false;

    return hashmap_rehash(
        table->table,
        new_bucket_count,
        (size_t (*)(void*)) table->hash,
        (int (*)(void*, void*)) table->cmp
    );
}

size_t symbol_table_size(SymbolTable* table) {
    if (!table) return 0;
    return hashmap_size(table->table);
}

void symbol_table_foreach(
    SymbolTable* table,
    void (*func)(char* key, Type* value)
) {
    if (!table || !func) return;

    HashMap *map = table->table;
    if (!map) return;

    for (size_t i = 0; i < map->bucket_count; ++i) {
        DynArray *bucket = &map->buckets[i];
        for (size_t j = 0; j < bucket->count; ++j) {
            KeyValue *kv = (KeyValue*)dynarray_get(bucket, j);
            func((char*)kv->key, (Type*)kv->value);
        }
    }
}
