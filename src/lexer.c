#include "lexer.h"
#include <ctype.h>

void compile_regex_patterns() {
    for (int i = 0; i < NUM_TOKENS; i++) {
        if (token_info[i].pattern && token_info[i].type != TOK_COMMENT && token_info[i].type != TOK_EOF && token_info[i].type != TOK_UNKNOWN) {
            if (regcomp(&token_info[i].re, token_info[i].pattern, REG_EXTENDED) != 0) {
                fprintf(stderr, "Failed to compile regex for token %s\n", token_type_to_string(token_info[i].type));
                exit(EXIT_FAILURE);
            }
        }
    }
}


Lexer *lexer_create(const char *src) {
    Lexer *lx = malloc(sizeof(Lexer));
    if (!lx) {
        fprintf(stderr, "Memory allocation failed for lexer\n");
        exit(EXIT_FAILURE);
    }
    lx->src = src;
    lx->cursor = src;
    lx->line = 1;
    lx->column = 1;
    compile_regex_patterns(); // Precompile regex patterns for tokens
    return lx;
}

void free_lexer(Lexer *lx) {
    if (lx) {
        free(lx);
    }
}

Token* lexer_handle_comment(Lexer *lx) {
    if (*lx->cursor == '/' && *(lx->cursor + 1) == '/') {
        const char *start = lx->cursor;
        while (*lx->cursor && *lx->cursor != '\n') {
            (lx->cursor)++;
            (lx->column)++;
        }
        start += 2; // skip "//"
        return create_token(TOK_COMMENT, start, lx->cursor - start, lx->line, lx->column);
    }
    return NULL; // Not a comment
}

Token* lexer_handle_literal_comparisons(Lexer *lx) {
    int best_idx = -1;
    size_t best_len = 0;

    for (int i = 0; i < NUM_TOKENS; i++) {
        if (token_info[i].printable && token_info[i].pattern == NULL) {
            const char *lit = token_info[i].printable;
            size_t len = strlen(lit);

            
            if (strncmp(lx->cursor, lit, len) != 0) continue;

            /* Only enforce identifier-boundary if the literal starts like an identifier
               (letter or underscore). For punctuation tokens (e.g. '[') do not enforce. */
            unsigned char first = (unsigned char)lit[0];
            if ((isalpha(first) || first == '_')) {
                unsigned char next = (unsigned char)lx->cursor[len];
                if (next && (isalnum(next) || next == '_')) {
                    /* It's a prefix of a longer identifier, skip this literal match */
                    continue;
                }
            }

            /* prefer the longest printable token (handles overlapping literals) */
            if (len > best_len) {
                best_len = len;
                best_idx = i;
            }
        }
    }

    if (best_idx != -1) {
        Token* t = create_token(token_info[best_idx].type,
                                lx->cursor, best_len,
                                lx->line, lx->column);
        lx->cursor += best_len;
        lx->column += best_len;
        return t;
    }

    return NULL; // No match found
}

Token* lexer_handle_regex(Lexer *lx) {
    for (int i = 0; i < NUM_TOKENS; i++) {
        if (token_info[i].pattern != NULL) {
            regex_t *re = &token_info[i].re;

            regmatch_t match;
            if (regexec(re, lx->cursor, 1, &match, 0) == 0) {
                size_t len = match.rm_eo - match.rm_so;
                Token* t = create_token(token_info[i].type,
                                        lx->cursor + match.rm_so, len,
                                        lx->line, lx->column);
                lx->cursor += match.rm_eo;
                lx->column += len;
                return t;
            }
        }
    }
    return NULL; // No regex match found
}



Token* lexer_next(Lexer *lx) {
    // 1. Skip whitespace
    while (isspace(*lx->cursor)) {
        if (*lx->cursor == '\n') { (lx->line)++; lx->column = 1; }
        else (lx->column)++;
        (lx->cursor)++;
    }

    // 2. Check for end of input
    if (!*lx->cursor) {
        return create_token(TOK_EOF, "", 0, lx->line, lx->column);
    }

    // 3. Check for comments
    Token *comment = lexer_handle_comment(lx);
    if (comment) {
        return comment;
    }

    // 4. Try fixed spelling tokens (literal comparisons)
    Token *fixed_token = lexer_handle_literal_comparisons(lx);
    if (fixed_token) {
        return fixed_token;
    }

    // 5. Try regex-based tokens
    Token *regex_token = lexer_handle_regex(lx);
    if (regex_token) {
        return regex_token;
    }

    // 6. If no match, consume one character as an unknown token
    return create_token(TOK_UNKNOWN, lx->cursor++, 1, lx->line, lx->column++);
}