#pragma once

#include <stdbool.h>
#include <stddef.h>

/* Keep CompilerOptions matching your previous struct. */
typedef struct {
    bool dump_tokens;
    bool dump_ast;
    bool show_time;
    bool show_symbol_table;
    bool show_hierarchical_types;
    const char *filename;
} CompilerOptions;

/* Public API: run the compiler on options. Returns EXIT_SUCCESS/EXIT_FAILURE */
int run_compiler(const CompilerOptions *opts);

/* Helper used by tests to call run_compiler */
int run_compiler_once(const CompilerOptions *opts);
