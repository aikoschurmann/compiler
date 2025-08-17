#include "hash_map.h"
#include <stdlib.h>

HashMap* hashmap_create(size_t bucket_count) {
    if (bucket_count == 0) bucket_count = 16; // default to 16 buckets
    HashMap *map = malloc(sizeof(HashMap));
    if (!map) return NULL;

    map->buckets = malloc(bucket_count * sizeof(DynArray));
    if (!map->buckets) {
        free(map);
        return NULL;
    }

    for (size_t i = 0; i < bucket_count; i++) {
        dynarray_init(&map->buckets[i], sizeof(KeyValue));
    }

    map->bucket_count = bucket_count;
    map->size = 0;
    return map;
}

void hashmap_destroy(
    HashMap* map,
    void (*free_key)(void*),
    void (*free_value)(void*)
) {
    if (!map) return;

    for (size_t i = 0; i < map->bucket_count; i++) {
        DynArray *bucket = &map->buckets[i];
        for (size_t j = 0; j < bucket->count; j++) {
            KeyValue *kv = (KeyValue*)dynarray_get(bucket, j);
            if (free_key) free_key(kv->key);
            if (free_value) free_value(kv->value);
        }
        dynarray_free(bucket);
    }
    free(map->buckets);
    free(map);
}

bool hashmap_put(
    HashMap* map,
    void* key,
    void* value,
    size_t (*hash)(void*),
    int (*cmp)(void*, void*)
) {
    if (!map || !key || !hash || !cmp) return false;

    size_t bucket_index = hash(key) % map->bucket_count;
    DynArray *bucket = &map->buckets[bucket_index];

    // Check if key already exists
    for (size_t i = 0; i < bucket->count; i++) {
        KeyValue *kv = (KeyValue*)dynarray_get(bucket, i);
        if (cmp(kv->key, key) == 0) {
            // Update existing value
            kv->value = value;
            return true;
        }
    }

    // Insert new key-value pair
    KeyValue kv = {key, value};
    if (dynarray_push_value(bucket, &kv) != 0) {
        return false; // OOM
    }
    
    map->size++;
    return true;
}

void* hashmap_get(
    HashMap* map,
    void* key,
    size_t (*hash)(void*),
    int (*cmp)(void*, void*)
) {
    if (!map || !key || !hash || !cmp) return NULL;

    size_t bucket_index = hash(key) % map->bucket_count;
    DynArray *bucket = &map->buckets[bucket_index];

    for (size_t i = 0; i < bucket->count; i++) {
        KeyValue *kv = (KeyValue*)dynarray_get(bucket, i);
        if (cmp(kv->key, key) == 0) {
            return kv->value; // Found
        }
    }
    return NULL; // Not found
}

bool hashmap_remove(
    HashMap* map,
    void* key,
    size_t (*hash)(void*),
    int (*cmp)(void*, void*),
    void (*free_key)(void*),
    void (*free_value)(void*)
) {
    if (!map || !key || !hash || !cmp) return false;

    size_t bucket_index = hash(key) % map->bucket_count;
    DynArray *bucket = &map->buckets[bucket_index];

    for (size_t i = 0; i < bucket->count; i++) {
        KeyValue *kv = (KeyValue*)dynarray_get(bucket, i);
        if (cmp(kv->key, key) == 0) {
            // Free key and value if provided
            if (free_key) free_key(kv->key);
            if (free_value) free_value(kv->value);

            // Remove from bucket
            remove_element(bucket, i);
            map->size--;
            return true; // Removed
        }
    }
    return false; // Not found
}

bool hashmap_rehash(
    HashMap* map,
    size_t new_bucket_count,
    size_t (*hash)(void*),
    int (*cmp)(void*, void*)
) {
    if (!map || new_bucket_count == 0 || !hash || !cmp) return false;

    DynArray *new_buckets = malloc(new_bucket_count * sizeof(DynArray));
    if (!new_buckets) return false;

    for (size_t i = 0; i < new_bucket_count; i++) {
        dynarray_init(&new_buckets[i], sizeof(KeyValue));
    }

    // Reinsert all key-value pairs into new buckets
    for (size_t i = 0; i < map->bucket_count; i++) {
        DynArray *bucket = &map->buckets[i];
        for (size_t j = 0; j < bucket->count; j++) {
            KeyValue *kv = (KeyValue*)dynarray_get(bucket, j);
            size_t new_index = hash(kv->key) % new_bucket_count;
            if (dynarray_push_value(&new_buckets[new_index], kv) != 0) {
                // Cleanup on failure
                for (size_t k = 0; k < new_bucket_count; k++) {
                    dynarray_free(&new_buckets[k]);
                }
                free(new_buckets);
                return false; // OOM
            }
        }
    }

    // Free old buckets
    for (size_t i = 0; i < map->bucket_count; i++) {
        dynarray_free(&map->buckets[i]);
    }
    free(map->buckets);

    map->buckets = new_buckets;
    map->bucket_count = new_bucket_count;
    return true;
}

void hashmap_foreach(
    HashMap* map,
    void (*callback)(void*, void*)
) {
    if (!map || !callback) return;

    for (size_t i = 0; i < map->bucket_count; i++) {
        DynArray *bucket = &map->buckets[i];
        for (size_t j = 0; j < bucket->count; j++) {
            KeyValue *kv = (KeyValue*)dynarray_get(bucket, j);
            callback(kv->key, kv->value);
        }
    }
}

