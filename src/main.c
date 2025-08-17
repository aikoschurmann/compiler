/* main.c — tiny entry point. Parses CLI and dispatches to driver or tests. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "driver.h"

/* tests_run_all provided by tests.c */
extern void tests_run_all(void);

static void print_usage(const char *progname) {
    fprintf(stderr,
        "Usage: %s [options] <source-file>\n"
        "Options:\n"
        "  --tokens        Dump tokens after lexing\n"
        "  --ast           Dump AST after parsing\n"
        "  --time          Print timing for each phase (ms)\n"
        "  --test          Run the built-in test suite\n"
        "  --sym-table     Print symbol table\n"
        "  --help, -h      Show this message\n",
        progname);
}

int main(int argc, char **argv) {
    if (argc < 2) { print_usage(argv[0]); return EXIT_FAILURE; }

    /* quick flag parsing - handle options in any order */
    bool run_tests = false;
    CompilerOptions opts = {0};
    opts.dump_tokens = false;
    opts.dump_ast = false;
    opts.show_time = false;
    opts.show_symbol_table = false;
    opts.filename = NULL;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--tokens") == 0) {
            opts.dump_tokens = true;
        } else if (strcmp(argv[i], "--ast") == 0) {
            opts.dump_ast = true;
        } else if (strcmp(argv[i], "--time") == 0) {
            opts.show_time = true;
        } else if (strcmp(argv[i], "--test") == 0) {
            run_tests = true;
        } else if (strcmp(argv[i], "--sym-table") == 0) {
            opts.show_symbol_table = true;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]); 
            return EXIT_SUCCESS;
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "Unknown option: %s\n", argv[i]); 
            print_usage(argv[0]); 
            return EXIT_FAILURE;
        } else {
            /* Non-option argument - treat as filename */
            if (opts.filename == NULL) {
                opts.filename = argv[i];
            } else {
                fprintf(stderr, "Multiple input files specified: '%s' and '%s'\n", 
                        opts.filename, argv[i]);
                print_usage(argv[0]);
                return EXIT_FAILURE;
            }
        }
    }

    if (run_tests) {
        tests_run_all();
        return EXIT_SUCCESS;
    }

    if (!opts.filename) { print_usage(argv[0]); return EXIT_FAILURE; }
    return run_compiler(&opts);
}
