#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


void create_parse_error(ParseError *err_out, const char *message, Parser *p) {
    if (!err_out) return;
    err_out->message = message ? strdup(message) : NULL;
    err_out->token = (p && p->current < p->tokens.size) ? p->tokens.data[p->current] : NULL;
    err_out->line = err_out->token ? err_out->token->line : 0;
    err_out->col  = err_out->token ? err_out->token->col  : 0;
    err_out->filename = p && p->filename ? strdup(p->filename) : NULL;
    err_out->p = p;
}

static void print_header(ParseError *error) {
    const char *msg = error && error->message ? error->message : "parse error";
    fprintf(stderr, "Error: %s\n", msg);
}

static void print_token_info(ParseError *error) {
    const char *filename = error && error->filename ? error->filename : "<unknown file>";
    size_t line = error ? error->line : 0;
    size_t col  = error ? error->col  : 0;
    if (error && error->token) {
        const char *tokname = token_type_to_string(error->token->type);
        const char *lexeme = error->token->lexeme ? error->token->lexeme : "";
        if (lexeme[0])
            fprintf(stderr, "Found Token: %s (\"%s\") at %s:%zu:%zu\n", tokname, lexeme, filename, line, col);
        else
            fprintf(stderr, "Found Token: %s at %s:%zu:%zu\n", tokname, filename, line, col);
    } else {
        fprintf(stderr, "At %s:%zu:%zu\n", filename, line, col);
    }
}

/* Read (and return a heap-allocated copy of) `target_line` from `f`.
 * Lines are 1-based: target_line == 1 returns first line.
 * Returns strdup'd string without trailing newline/carriage-return on success,
 * or NULL if file ends before that line or on allocation error.
 * Caller must free() the returned buffer.
 */
static char *read_file_line(FILE *f, size_t target_line) {
    if (!f || target_line == 0) return NULL;
    rewind(f);
    char *buf = NULL;
    size_t buflen = 0;
    ssize_t n;
    size_t current = 0;

    while ((n = getline(&buf, &buflen, f)) != -1) {
        current++;
        if (current == target_line) {
            /* trim trailing newline/carriage return(s) */
            while (n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r')) {
                buf[--n] = '\0';
            }
            char *ret = strdup(buf ? buf : "");
            free(buf);
            return ret; /* strdup'd string */
        }
    }

    free(buf);
    return NULL;
}

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Print the caret line aligned under `linebuf` (1-based caret_col).
 * caret_col is clamped to [1, strlen(linebuf)+1]. The function prints
 * the same 4-space indent used for code lines in the original function.
 */
static void print_caret_line(const char *linebuf, size_t caret_col) {
    size_t line_len = linebuf ? strlen(linebuf) : 0;

    /* normalize/clamp caret_col (1-based) */
    if (caret_col == 0) caret_col = 1;
    if (caret_col > line_len + 1) caret_col = line_len + 1;

    /* print 4-space indent used for code lines */
    fprintf(stderr, "    ");

    /* print (caret_col - 1) characters' worth of spacing, preserving tabs
       from the original line where possible; treat missing characters as spaces */
    for (size_t i = 0; i + 1 < caret_col; ++i) {
        if (i < line_len && linebuf[i] == '\t') {
            fputc('\t', stderr);
        } else {
            fputc(' ', stderr);
        }
    }

    fprintf(stderr, "^\n");
}

/* Print the source snippet for the ParseError in a modular way. */
static void print_source_snippet(ParseError *error) {
    if (!error || !error->filename) return;

    /* primary line/column from the error (1-based semantics for column) */
    size_t primary_line = error->line ? error->line : 0;
    size_t primary_col  = error->col  ? error->col  : 1;

    size_t display_line = primary_line;

    /* Attempt to use the previous token's line if requested and available */
    int use_prev_line = 0;
    Token *prev_token = NULL;
    size_t prev_col = 0;

    if (error->underline_previous_token_line && error->p) {
        Parser *p = error->p;
        if (p->current > 0 && p->tokens.data) {
            size_t prev_index = p->current - 1;
            prev_token = p->tokens.data[prev_index];
        }

        if (prev_token && prev_token->line > 0 && prev_token->line < primary_line) {
            display_line = prev_token->line;
            use_prev_line = 1;
            prev_col = prev_token->col ? prev_token->col : 1;
        }
    }

    if (display_line == 0) return;

    FILE *f = fopen(error->filename, "r");
    if (!f) return;

    char *linebuf = read_file_line(f, display_line);
    fclose(f);
    if (!linebuf) return;

    size_t caret_col = 1; /* 1-based */

    if (use_prev_line) {
        /* If we have a lexeme for the token that we want to show, append it
           to the printed line and point the caret at the start of the appended lexeme.
           Otherwise, prefer to point at the previous token's column (clamped). */
        if (error->token && error->token->lexeme && error->token->lexeme[0] != '\0') {
            fprintf(stderr, "    %s%s\n", linebuf, error->token->lexeme);
            caret_col = strlen(linebuf) + 1; /* caret points at start of appended lexeme */
        } else {
            fprintf(stderr, "    %s\n", linebuf);
            size_t line_len = strlen(linebuf);
            caret_col = (prev_col && prev_col <= line_len + 1) ? prev_col : (line_len + 1);
        }
    } else {
        /* Normal case: print the line and underline at the error column (clamped) */
        fprintf(stderr, "    %s\n", linebuf);
        size_t line_len = strlen(linebuf);
        caret_col = primary_col ? primary_col : 1;
        if (caret_col > line_len + 1) caret_col = line_len + 1;
    }

    print_caret_line(linebuf, caret_col);
    free(linebuf);
}

void print_parse_error(ParseError *error) {
    if (!error) return;
    print_header(error);
    print_token_info(error);
    print_source_snippet(error);
    fflush(stderr);
}

Token *current_token(Parser *p) {
    if (!p) return NULL;
    return (p->current >= p->end) ? NULL : p->tokens.data[p->current];
}

Token *peek(Parser *p, size_t offset) {
    if (!p) return NULL;
    size_t idx = p->current + offset;
    return (idx < p->end) ? p->tokens.data[idx] : NULL;
}

Token *consume(Parser *p, TokenType expected) {
    Token *tok = current_token(p);
    if (!tok || tok->type != expected) return NULL;
    p->current++;
    return tok;
}

Parser *parser_create(TokenArray tokens, const char *filename) {
    Parser *p = malloc(sizeof(Parser));
    if (!p) {
        fprintf(stderr, "memory allocation failed\n");
        exit(EXIT_FAILURE);
    }
    p->tokens = tokens;
    p->current = 0;
    p->end = tokens.size;
    p->filename = filename ? strdup(filename) : NULL;
    return p;
}

void parser_free(Parser *parser) {
    if (!parser) return;
    free(parser->filename);
    free(parser);
}   

void parser_rewind(Parser *p, size_t steps) {
    if (!p || steps > p->current) return; // cannot rewind beyond start
    p->current -= steps;
}