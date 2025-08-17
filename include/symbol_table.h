#pragma once

#include <stddef.h>
#include <stdbool.h>
#include "hash_map.h"
#include "type.h"


/* SymbolTable: a typed wrapper around the generic HashMap (string -> Type*) */
typedef struct {
    HashMap *table;

    /* ownership helpers (can be replaced after create) */
    void (*free_key)(char*);
    void (*free_value)(Type*);

    /* hashing / comparison (can be replaced after create) */
    size_t (*hash)(char*);
    int (*cmp)(char*, char*);
} SymbolTable;

/* Constructor / Destructor */
SymbolTable* symbol_table_create(size_t bucket_count);
void symbol_table_destroy(SymbolTable* table);

/* Insert or update (key is duplicated) */
/* On success, ownership of `value` is transferred to the symbol table. */
bool symbol_table_put(SymbolTable* table, const char* key, Type* value);

/* Lookup: returns the stored Type* or NULL if not found (does not transfer ownership) */
Type* symbol_table_get(SymbolTable* table, const char* key);

/* Remove: frees key/value using configured free functions */
bool symbol_table_remove(SymbolTable* table, const char* key);

/* Resize / Rehash */
bool symbol_table_rehash(SymbolTable* table, size_t new_bucket_count);

/* Utility */
size_t symbol_table_size(SymbolTable* table);

/* Iterate: callback gets (key, value) for each entry */
void symbol_table_foreach(
    SymbolTable* table,
    void (*func)(char* key, Type* value)
);
