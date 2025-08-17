#pragma once
#include <stddef.h>

/* Allocation helpers that exit on failure */
void *xmalloc(size_t n);
void *xcalloc(size_t nmemb, size_t size);
void *xrealloc(void *p, size_t n);
char *xstrdup(const char *s);

