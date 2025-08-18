/* src/type.c
 *
 * Standard-C implementation for Type creation / printing / free.
 * No nested functions (all helpers are static top-level functions).
 *
 * Expects:
 *  - utils.h (xmalloc/xrealloc/xstrdup)
 *  - type.h
 *  - ast.h providing AST node helpers used below
 */

#include "type.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/* -----------------------
 * simple string builder
 * --------------------- */
typedef struct {
    char *buf;
    size_t len;
    size_t cap;
} sbuf;

static void sbuf_init(sbuf *s) {
    s->cap = 128;
    s->buf = xmalloc(s->cap);
    s->len = 0;
    s->buf[0] = '\0';
}

static void sbuf_free(sbuf *s) {
    if (!s) return;
    free(s->buf);
    s->buf = NULL;
    s->len = 0;
    s->cap = 0;
}

static void sbuf_ensure(sbuf *s, size_t extra) {
    if (s->len + extra + 1 > s->cap) {
        while (s->len + extra + 1 > s->cap) s->cap *= 2;
        s->buf = xrealloc(s->buf, s->cap);
    }
}

static void sbuf_append(sbuf *s, const char *str) {
    if (!str) return;
    size_t add = strlen(str);
    sbuf_ensure(s, add);
    memcpy(s->buf + s->len, str, add);
    s->len += add;
    s->buf[s->len] = '\0';
}

static void sbuf_appendf(sbuf *s, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    /* compute needed size */
    va_list ap2;
    va_copy(ap2, ap);
    int needed = vsnprintf(NULL, 0, fmt, ap2);
    va_end(ap2);
    if (needed < 0) { va_end(ap); return; }

    sbuf_ensure(s, (size_t)needed);
    vsnprintf(s->buf + s->len, s->cap - s->len, fmt, ap);
    s->len += (size_t)needed;
    va_end(ap);
}

/* -----------------------
 * Type creation helpers
 * --------------------- */

Type *type_make_primitive(const char *name, int is_const) {
    Type *t = xmalloc(sizeof(*t));
    t->kind = TYPE_PRIMITIVE;
    t->is_const = !!is_const;
    t->primitive.name = xstrdup(name ? name : "(anon)");
    return t;
}

Type *type_make_pointer(Type *to, int is_const) {
    Type *t = xmalloc(sizeof(*t));
    t->kind = TYPE_POINTER;
    t->is_const = !!is_const;
    t->pointer.to = to;
    return t;
}

Type *type_make_array(Type *of, size_t size, int is_const) {
    Type *t = xmalloc(sizeof(*t));
    t->kind = TYPE_ARRAY;
    t->is_const = !!is_const;
    t->array.of = of;
    t->array.size = size;
    return t;
}

Type *type_make_function(Type *return_type, Type **params, size_t param_count, int is_const) {
    Type *t = xmalloc(sizeof(*t));
    t->kind = TYPE_FUNCTION;
    t->is_const = !!is_const;
    t->function.return_type = return_type;
    t->function.params = params;
    t->function.param_count = param_count;
    return t;
}


static void type_to_string_append(Type *t, sbuf *sb) {
    if (!t) { sbuf_append(sb, "<null>"); return; }
    if (t->is_const) sbuf_append(sb, "const ");
    switch (t->kind) {
        case TYPE_PRIMITIVE:
            sbuf_append(sb, t->primitive.name);
            break;

        case TYPE_POINTER:
            if (t->pointer.to && t->pointer.to->kind == TYPE_FUNCTION) {
                sbuf_append(sb, "(");
                type_to_string_append(t->pointer.to, sb);
                sbuf_append(sb, ")*");
            } else {
                type_to_string_append(t->pointer.to, sb);
                sbuf_append(sb, "*");
            }
            break;

        case TYPE_ARRAY:
            if (t->array.of && t->array.of->kind == TYPE_FUNCTION) {
                sbuf_append(sb, "(");
                type_to_string_append(t->array.of, sb);
                sbuf_append(sb, ")");
            } else {
                type_to_string_append(t->array.of, sb);
            }
            if (t->array.size == 0) sbuf_append(sb, "[]");
            else sbuf_appendf(sb, "[%zu]", t->array.size);
            break;

        case TYPE_FUNCTION:
            sbuf_append(sb, "fn(");
            for (size_t i = 0; i < t->function.param_count; ++i) {
                if (t->function.params[i]) {
                    type_to_string_append(t->function.params[i], sb);
                } else {
                    sbuf_append(sb, "<unknown>");
                }
                if (i + 1 < t->function.param_count) sbuf_append(sb, ", ");
            }
            sbuf_append(sb, ")");
            if (t->function.return_type) {
                sbuf_append(sb, " -> ");
                type_to_string_append(t->function.return_type, sb);
            }
            break;

        default:
            sbuf_append(sb, "<unknown-type>");
            break;
    }
}

void type_print(Type *t) {
    if (!t) { printf("NULL"); return; }
    if (t->is_const) printf("const ");
    switch (t->kind) {
        case TYPE_PRIMITIVE:
            printf("%s", t->primitive.name);
            break;
        case TYPE_POINTER:
            if (t->pointer.to && t->pointer.to->kind == TYPE_FUNCTION) {
                printf("(");
                type_print(t->pointer.to);
                printf(")*");
            } else {
                type_print(t->pointer.to);
                printf("*");
            }
            break;
        case TYPE_ARRAY:
            if (t->array.of && t->array.of->kind == TYPE_FUNCTION) {
                printf("(");
                type_print(t->array.of);
                printf(")");
            } else {
                type_print(t->array.of);
            }
            if (t->array.size == 0) printf("[]");
            else printf("[%zu]", t->array.size);
            break;
        case TYPE_FUNCTION:
            printf("fn(");
            for (size_t i = 0; i < t->function.param_count; ++i) {
                if (t->function.params[i]) type_print(t->function.params[i]);
                else printf("<unknown>");
                if (i + 1 < t->function.param_count) printf(", ");
            }
            printf(")");
            if (t->function.return_type) {
                printf(" -> ");
                type_print(t->function.return_type);
            }
            break;
        default:
            printf("<unknown-type>");
            break;
    }
}

char *type_to_string(Type *t) {
    if (!t) return xstrdup("(null)");
    sbuf sb; sbuf_init(&sb);
    type_to_string_append(t, &sb);
    char *out = xstrdup(sb.buf);
    sbuf_free(&sb);
    return out;
}

/* -----------------------
 * free
 * --------------------- */

void type_free(Type *t) {
    if (!t) return;
    switch (t->kind) {
        case TYPE_PRIMITIVE:
            free(t->primitive.name);
            break;
        case TYPE_POINTER:
            type_free(t->pointer.to);
            break;
        case TYPE_ARRAY:
            type_free(t->array.of);
            break;
        case TYPE_FUNCTION:
            type_free(t->function.return_type);
            if (t->function.params) {
                for (size_t i = 0; i < t->function.param_count; ++i) {
                    type_free(t->function.params[i]);
                }
                free(t->function.params);
            }
            break;
        default:
            break;
    }
    free(t);
}

/* -----------------------
 * Hierarchical type printer
 * --------------------- */

static void type_print_hierarchical_impl(Type *t, int depth) {
    if (!t) {
        for (int i = 0; i < depth; i++) printf(" ");
        printf("NULL\n");
        return;
    }
    
    for (int i = 0; i < depth; i++) printf(" ");
    
    if (t->is_const) printf("const ");
    
    switch (t->kind) {
        case TYPE_PRIMITIVE:
            printf("Primitive: %s\n", t->primitive.name);
            break;
            
        case TYPE_POINTER:
            printf("Pointer to:\n");
            type_print_hierarchical_impl(t->pointer.to, depth + 2);
            break;
            
        case TYPE_ARRAY:
            if (t->array.size == 0) {
                printf("Array[dynamic] of:\n");
            } else {
                printf("Array[%zu] of:\n", t->array.size);
            }
            type_print_hierarchical_impl(t->array.of, depth + 2);
            break;
        case TYPE_FUNCTION:
            printf("Function type:\n");
            
            for (int i = 0; i < depth + 2; i++) printf(" ");
            printf("Parameters (%zu):\n", t->function.param_count);
            if (t->function.param_count == 0) {
                for (int i = 0; i < depth + 4; i++) printf(" ");
                printf("(none)\n");
            } else {
                for (size_t j = 0; j < t->function.param_count; ++j) {
                    for (int i = 0; i < depth + 4; i++) printf(" ");
                    printf("Param[%zu]:\n", j);
                    type_print_hierarchical_impl(t->function.params[j], depth + 6);
                }
            }
            
            for (int i = 0; i < depth + 2; i++) printf(" ");
            printf("Return type:\n");
            if (t->function.return_type) {
                type_print_hierarchical_impl(t->function.return_type, depth + 4);
            } else {
                for (int i = 0; i < depth + 4; i++) printf(" ");
                printf("void\n");
            }
            break;
            
        default:
            printf("Unknown Type\n");
            break;
    }
}

void type_print_hierarchical(Type *t) {
    if (!t) {
        printf("NULL\n");
        return;
    }
    type_print_hierarchical_impl(t, 0);
}

void type_print_hierarchical_with_indent(Type *t, int base_indent) {
    if (!t) {
        for (int i = 0; i < base_indent; i++) printf(" ");
        printf("NULL\n");
        return;
    }
    type_print_hierarchical_impl(t, base_indent);
}