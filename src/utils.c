#include "utils.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void *xmalloc(size_t n) {
    void *p = malloc(n);
    if (!p) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    return p;
}

void *xcalloc(size_t nmemb, size_t size) {
    void *p = calloc(nmemb, size);
    if (!p) {
        perror("calloc");
        exit(EXIT_FAILURE);
    }
    return p;
}

void *xrealloc(void *p, size_t n) {
    void *r = realloc(p, n);
    if (!r) {
        perror("realloc");
        exit(EXIT_FAILURE);
    }
    return r;
}

char *xstrdup(const char *s){
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) {
        perror("strdup");
        exit(EXIT_FAILURE);
    }
    return d;
}
