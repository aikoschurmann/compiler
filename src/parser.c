// parser_impl.c
// Improved parser helpers and parse-error printing/snippet display.
// Drop next to parser.h / token.h etc.

#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ANSI colors (if terminal supports). Define PP_NO_COLOR to disable. */
#ifndef PP_NO_COLOR
#define COL_RESET   "\x1b[0m"
#define COL_ERROR   "\x1b[1;31m"  /* bold red for error header */
#define COL_LINENO  "\x1b[38;5;240m"  /* dark gray for line numbers */
#define COL_CARET   "\x1b[1;91m"  /* bright red caret */
#define COL_CODE    "\x1b[0m"   /* default for code */
#define COL_FILENAME "\x1b[1;36m"  /* bright cyan for filename */
#define COL_TOKEN   "\x1b[1;33m"  /* bright yellow for token info */
#define COL_LEXEME  "\x1b[32m"   /* green for lexeme text */
#define COL_LABEL   "\x1b[1;37m"  /* bright white for labels */
#define COL_PIPE    "\x1b[38;5;240m"  /* dark gray for pipe separator */
#else
#define COL_RESET   ""
#define COL_ERROR   ""
#define COL_LINENO  ""
#define COL_CARET   ""
#define COL_CODE    ""
#define COL_FILENAME ""
#define COL_TOKEN   ""
#define COL_LEXEME  ""
#define COL_LABEL   ""
#define COL_PIPE    ""
#endif

/* -------------------------
 * Public: create_parse_error
 * ------------------------- */
void create_parse_error(ParseError *err_out, const char *message, Parser *p) {
    if (!err_out) return;
    err_out->message = message ? strdup(message) : NULL;
    err_out->token = (p && p->current < p->tokens.size) ? p->tokens.data[p->current] : NULL;
    err_out->line = err_out->token ? err_out->token->line : 0;
    err_out->col  = err_out->token ? err_out->token->col  : 0;
    err_out->filename = p && p->filename ? strdup(p->filename) : NULL;
    err_out->p = p;
}

/* -------------------------
 * Private: header & token info printing
 * ------------------------- */
static void print_header(ParseError *error) {
    const char *msg = error && error->message ? error->message : "parse error";
    fprintf(stderr, "\n" COL_ERROR "✗ Error:" COL_RESET " %s\n", msg);
}

static void print_token_info(ParseError *error) {
    const char *filename = error && error->filename ? error->filename : "<unknown file>";
    size_t line = error ? error->line : 0;
    size_t col  = error ? error->col  : 0;
    if (error && error->token) {
        const char *tokname = token_type_to_string(error->token->type);
        const char *lexeme = error->token->lexeme ? error->token->lexeme : "";
        if (lexeme[0])
            fprintf(stderr, COL_LABEL "Found:" COL_RESET " " COL_TOKEN "%s" COL_RESET " " COL_LEXEME "\"%s\"" COL_RESET " at " COL_FILENAME "%s" COL_RESET ":" COL_LINENO "%zu:%zu" COL_RESET "\n", 
                    tokname, lexeme, filename, line, col);
        else
            fprintf(stderr, COL_LABEL "Found:" COL_RESET " " COL_TOKEN "%s" COL_RESET " at " COL_FILENAME "%s" COL_RESET ":" COL_LINENO "%zu:%zu" COL_RESET "\n", 
                    tokname, filename, line, col);
    } else {
        fprintf(stderr, COL_LABEL "Location:" COL_RESET " " COL_FILENAME "%s" COL_RESET ":" COL_LINENO "%zu:%zu" COL_RESET "\n", filename, line, col);
    }
}

/* -------------------------
 * Read a single (1-based) line from a file and return strdup'd string (no newline)
 * Caller must free. Returns NULL on EOF/error.
 * ------------------------- */
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
            /* strip trailing newline/carriage returns */
            while (n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r')) {
                buf[--n] = '\0';
            }
            char *ret = strdup(buf ? buf : "");
            free(buf);
            return ret;
        }
    }

    free(buf);
    return NULL;
}

/* -------------------------
 * Print a single source line with gutter "  <line_no> | <code>"
 * line_no_width must be enough digits to align numbers.
 * ------------------------- */
static void print_line_with_gutter(size_t line_no, const char *linebuf, int line_no_width) {
    if (!linebuf) linebuf = "";
    fprintf(stderr, " " COL_LINENO "%*zu" COL_RESET " " COL_PIPE "│" COL_RESET " " COL_CODE "%s" COL_RESET "\n",
            line_no_width, line_no, linebuf);
}

/* -------------------------
 * Print caret line aligned with the gutter above.
 * caret_col is 1-based byte index into linebuf (after tab expansion we preserve tabs).
 *
 * Strategy:
 *  - Print same gutter spacing as print_line_with_gutter.
 *  - Reproduce spacing up to (caret_col - 1) bytes: print same tabs or spaces
 *    so caret lines up visually under the code.
 * ------------------------- */
static void print_caret_line_for(const char *linebuf, size_t caret_col, int line_no_width) {
    if (!linebuf) linebuf = "";
    size_t line_len = strlen(linebuf);

    if (caret_col == 0) caret_col = 1;
    if (caret_col > line_len + 1) caret_col = line_len + 1;

    /* gutter width: " %*zu │ " -> line_no_width + 4 chars for indent */
    int gutter = line_no_width + 4;
    for (int i = 0; i < gutter; ++i) fputc(' ', stderr);

    /* Print spacing that matches the source up to caret_col - 1 bytes (preserve tabs) */
    size_t printed = 0;
    for (size_t i = 0; i + 1 < caret_col && i < line_len; ++i) {
        if (linebuf[i] == '\t') fputc('\t', stderr);
        else fputc(' ', stderr);
        printed++;
    }

    /* If caret beyond line bytes, add remaining spaces */
    while (printed < caret_col - 1) { fputc(' ', stderr); printed++; }

    /* Print caret in color to stderr */
    fprintf(stderr, COL_CARET "^" COL_RESET "\n");
}


/* -------------------------
 * print_source_snippet implementation (fixed)
 *
 * Behavior:
 *  - If error->underline_previous_token_line is true and a previous token exists on an earlier line:
 *      prints previous line once with caret placed *after* the previous token lexeme (if available),
 *      then prints the primary/current line for context (no caret).
 *  - Otherwise prints the primary line and a caret at primary column.
 * ------------------------- */
static void print_source_snippet(ParseError *error) {
    if (!error || !error->filename) return;

    size_t primary_line = error->line ? error->line : 0;
    size_t primary_col  = error->col  ? error->col  : 1;

    /* check prev token */
    int use_prev_line = 0;
    Token *prev_token = NULL;
    size_t prev_line = 0;
    size_t prev_col  = 0;

    if (error->underline_previous_token_line && error->p) {
        Parser *p = error->p;
        if (p->current > 0 && p->tokens.data) {
            size_t prev_index = p->current - 1;
            if (prev_index < p->tokens.size) prev_token = p->tokens.data[prev_index];
        }
        
        if (prev_token && prev_token->line > 0) {
            prev_line = prev_token->line;
            prev_col  = prev_token->col ? prev_token->col : 1;
            if (primary_line == 0 || prev_line < primary_line) use_prev_line = 1;
        }
    }


    /* open file */
    FILE *f = fopen(error->filename, "r");
    if (!f) return;

    /* compute gutter width based on max line number we'll print */
    size_t max_line = primary_line;
    if (use_prev_line && prev_line > max_line) max_line = prev_line;
    if (max_line == 0) { fclose(f); return; }
    int line_no_width = 1;
    size_t tmp = max_line;
    while (tmp >= 10) { tmp /= 10; ++line_no_width; }

    fprintf(stderr, "\n" COL_LABEL "Source:" COL_RESET "\n");

    if (use_prev_line) {
        /* print previous line once */
        char *prev_buf = read_file_line(f, prev_line);
        if (!prev_buf) {
            /* fallback to single-line primary printing */
            use_prev_line = 0;
        } else {
            /* compute caret position after previous token lexeme if available,
               otherwise use prev_col (start column). prev_col is 1-based.
             */
            size_t caret_col = 1;
            if (prev_token && prev_token->lexeme && prev_token->lexeme[0] != '\0') {
                size_t lex_len = strlen(prev_token->lexeme);
                caret_col = prev_col + lex_len; /* caret after last char of token */
            } else {
                caret_col = prev_col ? prev_col : 1;
            }

            /* clamp caret */
            size_t line_len = strlen(prev_buf);
            if (caret_col > line_len + 1) caret_col = line_len + 1;

            /* print prev line + caret */
            print_line_with_gutter(prev_line, prev_buf, line_no_width);
            print_caret_line_for(prev_buf, caret_col, line_no_width);

            free(prev_buf);

            /* print primary line for context if different */
            if (primary_line > 0 && primary_line != prev_line) {
                char *prim = read_file_line(f, primary_line);
                if (prim) {
                    print_line_with_gutter(primary_line, prim, line_no_width);
                    free(prim);
                }
            }

            fclose(f);
            return;
        }
    }

    /* single-line (normal) mode: primary line + caret at primary_col */
    if (primary_line == 0) { fclose(f); return; }
    char *prim = read_file_line(f, primary_line);
    if (!prim) { fclose(f); return; }

    /* clamp caret to line */
    size_t line_len = strlen(prim);
    size_t caret = primary_col ? primary_col : 1;
    if (caret > line_len + 1) caret = line_len + 1;

    print_line_with_gutter(primary_line, prim, line_no_width);
    print_caret_line_for(prim, caret, line_no_width);

    free(prim);
    fclose(f);
}

/* -------------------------
 * Public: print_parse_error - top-level printer
 * ------------------------- */
void print_parse_error(ParseError *error) {
    if (!error) return;
    print_header(error);
    print_token_info(error);
    print_source_snippet(error);
    fprintf(stderr, "\n");
    fflush(stderr);
}

/* -------------------------
 * Small parser/token helpers (thin wrappers)
 * ------------------------- */
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

/* -------------------------
 * Parser lifecycle
 * ------------------------- */
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
