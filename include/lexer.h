#pragma once

#include "token.h"
#include <regex.h>

// Lexer state
typedef struct {
    const char *src;
    const char *cursor;
    int         line;
    int         column;
} Lexer;

Lexer   *lexer_create(const char *src);
Token   *lexer_next(Lexer *lx);
void     free_lexer(Lexer *lx);
