#pragma once
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <regex.h>

#define X(name, str, regex) name,
typedef enum {
    #include "tokens.def"
    NUM_TOKENS
} TokenType;
#undef X

typedef struct {
    TokenType type;
    const char *lexeme;
    int line, col;
} Token;

typedef struct {
    TokenType type;
    const char *printable;
    const char *pattern; // regex
    regex_t re; // compiled regex, if applicable
} TokenInfo;

extern TokenInfo token_info[NUM_TOKENS];

/* A growable array of Token pointers */
typedef struct {
    Token **data;
    size_t size, capacity;
} TokenArray;

Token *create_token(TokenType type, const char *value, size_t len, int line, int column);
void   free_token(Token *tok);
const char *token_type_to_string(TokenType t);
void   print_token(Token *tok);
void   print_token_colored(Token *tok);
void   token_array_init(TokenArray *arr);
void   token_array_push(TokenArray *arr, Token *tok);
void   token_array_free(TokenArray *arr);
void   dump_tokens_json_fp(FILE *out, Token **tokens, size_t n);
void   dump_tokens_json_file(const char *filename, Token **tokens, size_t n);