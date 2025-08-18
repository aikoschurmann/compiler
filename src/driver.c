#include "driver.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "file.h"
#include "lexer.h"
#include "parser.h"
#include "ast_parse_statements.h"
#include "scope.h"


typedef struct {
    struct timespec start;
    double ms;
} Timer;

static void timer_start(Timer *t) {
    t->ms = 0.0;
    clock_gettime(CLOCK_MONOTONIC, &t->start);
}

static void timer_stop(Timer *t) {
    struct timespec end;
    clock_gettime(CLOCK_MONOTONIC, &end);
    double sec = (double)(end.tv_sec - t->start.tv_sec);
    double nsec = (double)(end.tv_nsec - t->start.tv_nsec);
    t->ms = sec * 1000.0 + nsec / 1e6;
}

/* Print timings if user requested them */
static void print_timings_if_requested(const CompilerOptions *opts,
                                       const Timer *t_load,
                                       const Timer *t_lex,
                                       const Timer *t_parse,
                                       const Timer *t_sem,
                                       const Timer *t_total) {
    if (!opts->show_time) return;
    fprintf(stderr,
            "Timings (ms): load=%.3f lex=%.3f parse=%.3f symbol-table(global)=%.3f total=%.3f\n",
            t_load->ms, t_lex->ms, t_parse->ms, t_sem->ms, t_total->ms);
}

/* Load source file into heap string (caller frees) */
static char *load_source(const char *filename) {
    const char *src = read_file(filename);
    if (!src) return NULL;
    char *buf = strdup(src);
    free((char*)src);
    return buf;
}

/* Lex input into TokenArray. Returns 0 on success, non-zero on failure. */
static int lex_source(const char *code, TokenArray *out_tokens) {
    Lexer *lx = lexer_create(code);
    if (!lx) return -1;

    token_array_init(out_tokens);
    while (1) {
        Token *tok = lexer_next(lx);
        if (!tok) {
            free_lexer(lx);
            token_array_free(out_tokens);
            return -1;
        }
        if (tok->type == TOK_COMMENT) {
            free_token(tok);
            continue;
        }
        if (tok->type == TOK_UNKNOWN) {
            fprintf(stderr, "error: unknown token '%s' at line %d, column %d\n",
                    tok->lexeme, tok->line, tok->col);
            free_token(tok);
            free_lexer(lx);
            token_array_free(out_tokens);
            return -1;
        }

        token_array_push(out_tokens, tok);
        if (tok->type == TOK_EOF) break;
    }
    free_lexer(lx);
    return 0;
}

/* The public driver function */
int run_compiler_once(const CompilerOptions *opts) {
    Timer t_total, t_load = {0}, t_lex = {0}, t_parse = {0}, t_sem = {0};
    int exit_code = EXIT_FAILURE;
    timer_start(&t_total);

    char *code = NULL;
    TokenArray tokens;
    bool lex_ok = false;
    Parser *parser = NULL;
    AstNode *program = NULL;
    ParseError perr = {0};

    /* load */
    timer_start(&t_load);
    code = load_source(opts->filename);
    timer_stop(&t_load);

    if (!code) {
        fprintf(stderr, "error: failed to read '%s'\n", opts->filename);
        goto finish;
    }

    /* lex */
    timer_start(&t_lex);
    if (lex_source(code, &tokens) != 0) {
        fprintf(stderr, "error: lexing failed\n");
        goto finish;
    }
    lex_ok = true;
    timer_stop(&t_lex);

    if (opts->dump_tokens) {
        for (size_t i = 0; i < tokens.size; ++i) print_token_colored(tokens.data[i]);
    }

    /* parse */
    timer_start(&t_parse);
    parser = parser_create(tokens, opts->filename);
    program = parse_program(parser, &perr);
    timer_stop(&t_parse);

    if (perr.message) {
        print_parse_error(&perr);
        free(perr.message);
        goto finish;
    }

    if (opts->dump_ast) {
        puts("=== AST ===");
        print_ast(program, 0);
    }

    /* semantics: collect function signatures (symbol table construction) */
    timer_start(&t_sem);
    {
        Scope global_scope = {0};

        if (symbol_table_construction(&global_scope, &program->data.program) != 0) {
            fprintf(stderr, "error: symbol table construction failed\n");
            free_scope_maps(&global_scope); /* optional; safe if implemented */
            goto finish;
        }
        if(opts->show_symbol_table){
            puts("=== Symbol Table ===");
            scope_print(&global_scope);
        }
        if(opts->show_hierarchical_types){
            puts("=== Hierarchical Type Structure ===");
            scope_print_hierarchical(&global_scope);
        }
        

        /* free symbol maps now; if you need them later, don't free here */
        free_scope_maps(&global_scope);
    }
    timer_stop(&t_sem);

    exit_code = EXIT_SUCCESS;

finish:
    if (program) ast_node_free(program);
    if (parser) parser_free(parser);
    if (lex_ok) token_array_free(&tokens);
    if (code) free(code);

    timer_stop(&t_total);
    print_timings_if_requested(opts, &t_load, &t_lex, &t_parse, &t_sem, &t_total);
    return exit_code;
}

/* small wrapper kept for API parity */
int run_compiler(const CompilerOptions *opts) {
    return run_compiler_once(opts);
}