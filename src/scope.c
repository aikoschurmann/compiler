#include "scope.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>


static void free_symbol(Symbol *sym) {
    if (!sym) return;
    if (sym->sem_type) type_free(sym->sem_type);
    free(sym); /* sym->name not freed (owned by AST or duplicated elsewhere) */
}

static size_t str_hash(void *key) {
    const unsigned char *s = (const unsigned char *)key;
    if (!s) return 0;
    unsigned long h = 5381;
    int c;
    while ((c = *s++))
        h = ((h << 5) + h) + c;
    return (size_t)h;
}

static int str_cmp(void *a, void *b) {
    if (a == b) return 0;
    if (!a) return -1;
    if (!b) return 1;
    return strcmp((const char*)a, (const char*)b);
}

/* Ensure maps exist */
int ensure_scope_maps(Scope *scope) {
    if (!scope) return -1;
    if (!scope->functions) {
        scope->functions = symbol_table_create(128);
        if (!scope->functions) return -1;
    }
    if (!scope->variables) {
        scope->variables = symbol_table_create(128);
        if (!scope->variables) {
            return -1;
        }
    }
    return 0;
}


static void print_indent(int indent) {
    for (int i = 0; i < indent; ++i) putchar(' ');
}

static void hashmap_print_entries(HashMap *map, int indent) {
    if (!map) {
        print_indent(indent);
        printf("hashmap_print_entries empty hashmap");
        return;
    }

    for (size_t b = 0; b < map->bucket_count; ++b) {
        DynArray *bucket = &map->buckets[b];
        for (size_t j = 0; j < bucket->count; ++j) {
            KeyValue *kv = (KeyValue*)dynarray_get(bucket, j);
            char *key = (char*)kv->key;
            Symbol *sym = (Symbol*)kv->value;

            print_indent(indent);
            if (key) printf("%s : ", key);
            else printf("<anon> : ");

            if (sym && sym->sem_type) {
                type_print(sym->sem_type);
            } else {
                printf("<NULL-symbol>");
            }
            printf("\n");
        }
    }
}

void scope_print(Scope *scope) {
    if (!scope) {
        printf("<null-scope>\n");
        return;
    }
    int indent = 0;
    Scope *s = scope;
    while (s) {
        print_indent(indent);
        printf("globalScope\n");

        print_indent(indent + 2);
        printf("Functions:\n");
        hashmap_print_entries(s->functions ? s->functions->table : NULL, indent + 4);

        print_indent(indent + 2);
        printf("Variables:\n");
        hashmap_print_entries(s->variables ? s->variables->table : NULL, indent + 4);

        s = s->parent;
        indent += 2;
        if (s) {
            print_indent(indent);
            printf("Parent ->\n");
            indent += 2;
        }
    }
}

static void scope_print_bucket_hierarchical(DynArray *bucket, int indent) {
    if (!bucket) return;
    for (size_t j = 0; j < bucket->count; ++j) {
        KeyValue *kv = (KeyValue*)dynarray_get(bucket, j);
        char *key = (char*)kv->key;
        Symbol *sym = (Symbol*)kv->value;

        print_indent(indent);
        if (key) printf("%s:\n", key);
        else printf("<anon>:\n");

        if (sym && sym->sem_type) {
            type_print_hierarchical_with_indent(sym->sem_type, indent + 2);
        } else {
            for (int i = 0; i < indent + 2; i++) printf(" ");
            printf("<NULL-symbol>\n");
        }
    }
}

void scope_print_hierarchical(Scope *scope) {
    if (!scope) {
        printf("NULL scope\n");
        return;
    }

    printf("globalScope\n");

    /* Functions */
    if (scope->functions) {
        printf("  Functions:\n");
        for (size_t i = 0; i < scope->functions->table->bucket_count; ++i) {
            DynArray *bucket = &scope->functions->table->buckets[i];
            scope_print_bucket_hierarchical(bucket, 4);
        }
    }

    /* Variables */
    if (scope->variables) {
        printf("  Variables:\n");
        for (size_t i = 0; i < scope->variables->table->bucket_count; ++i) {
            DynArray *bucket = &scope->variables->table->buckets[i];
            scope_print_bucket_hierarchical(bucket, 4);
        }
    }
}

void free_scope_maps(Scope *s) {
    if (!s) return;
    if (s->functions) {
        s->functions->free_value = (void(*)(Symbol*))free_symbol;
        symbol_table_destroy(s->functions);
        s->functions = NULL;
    }
    if (s->variables) {
        s->variables->free_value = (void(*)(Symbol*))free_symbol;
        symbol_table_destroy(s->variables);
        s->variables = NULL;
    }
}
