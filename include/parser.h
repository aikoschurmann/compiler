#pragma once

#include "token.h"

typedef struct {
    TokenArray tokens;
    size_t      current;
    size_t      end;          /* index of the last token in the current parse */
    char        *filename;
} Parser;

typedef struct {
    const char *message;      /* error message */
    Token *token;             /* token that caused the error, NULL if not applicable */
    size_t line;              /* line number of the error */
    size_t col;               /* column number of the error */
    Parser *p;
    const char *filename;     /* file where the error occurred */
    int underline_previous_token_line; /* if true, underline the previous token's line instead of the current one */
} ParseError;

Parser *parser_create(TokenArray tokens, const char *filename);

Token *consume(Parser *p, TokenType expected);

Token *current_token(Parser *p);

Token *peek(Parser *p, size_t offset);

void parser_free(Parser *parser);

void create_parse_error(ParseError *err_out, const char *message, Parser *p);

void print_parse_error(ParseError *error);

void parser_rewind(Parser *p, size_t steps);