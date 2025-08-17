/* src/tests.c
 *
 * Test harness that writes source to a temp file, runs the compiler, captures stderr,
 * and prints concise diagnostics on failure. Calls run_compiler from driver.c.
 */

#include "driver.h"   /* provides CompilerOptions and run_compiler */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <unistd.h>    /* dup, dup2, close, unlink, write */
#include <fcntl.h>     /* mkstemp */
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>

#define MAX_CAPTURE_BYTES 8192
#define TAIL_BYTES 4096

static int g_tests_run = 0;
static int g_tests_passed = 0;

/* Simple wall-clock timer using gettimeofday */
static double now_seconds(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
}

/* Helper: safely write whole buffer to fd */
static bool safe_write_all(int fd, const char *buf, size_t len) {
    size_t written = 0;
    while (written < len) {
        ssize_t w = write(fd, buf + written, len - written);
        if (w < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        written += (size_t)w;
    }
    return true;
}

/* Helper: read the last up to `tail_bytes` bytes from a FILE* (cursor at end).
 * Returns malloc'd buffer (null-terminated) and sets out_len. Caller frees.
 * On error returns NULL (out_len = 0). */
static char *read_file_tail(FILE *f, size_t tail_bytes, size_t *out_len) {
    if (!f || !out_len) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) {
        *out_len = 0;
        return NULL;
    }
    long total = ftell(f);
    if (total < 0) { *out_len = 0; return NULL; }

    long start = (long)total - (long)tail_bytes;
    if (start < 0) start = 0;
    long to_read = (long)total - start;
    if (to_read <= 0) { *out_len = 0; return NULL; }

    if (fseek(f, start, SEEK_SET) != 0) { *out_len = 0; return NULL; }

    char *buf = malloc((size_t)to_read + 1);
    if (!buf) { *out_len = 0; return NULL; }

    size_t r = fread(buf, 1, (size_t)to_read, f);
    buf[r] = '\0';
    *out_len = r;
    return buf;
}

/* Forward declaration of run_test used by macro below */
static void run_test(const char* src, const char* label, bool should_fail);

/* Convenient macro to mirror your RUN(...) usage */
#define RUN(src, label, should_fail) run_test(src, label, should_fail)

/* Improved run_test: secure temp file, capture stderr, quiet on success, print only useful diagnostics on failure */
static void run_test(const char* src, const char* label, bool should_fail) {
    g_tests_run++;
    CompilerOptions opts = {0};
    opts.dump_tokens = false;
    opts.dump_ast = false;
    opts.show_time = false;
    opts.show_symbol_table = false;

    /* Create secure temporary file for source using mkstemp */
    char template[] = "/tmp/testsrc_XXXXXX";
    int src_fd = mkstemp(template);
    if (src_fd < 0) {
        fprintf(stderr, "Failed to create temp source for test '%s': %s\n", label, strerror(errno));
        return;
    }

    /* Write source and close fd */
    if (!safe_write_all(src_fd, src, strlen(src))) {
        fprintf(stderr, "Failed to write temp source for test '%s'\n", label);
        close(src_fd);
        unlink(template);
        return;
    }
    close(src_fd);

    /* Set filename in opts (make a heap copy so we can free after) */
    opts.filename = strdup(template);
    if (!opts.filename) {
        fprintf(stderr, "Out of memory for filename strdup\n");
        unlink(template);
        return;
    }

    /* Prepare capture file for stderr */
    FILE *capture = tmpfile();
    if (!capture) {
        fprintf(stderr, "Failed to create capture tmpfile for test '%s'\n", label);
        free((char*)opts.filename);
        unlink(template);
        return;
    }

    /* Save old stderr fd */
    int old_stderr_fd = dup(fileno(stderr));
    if (old_stderr_fd < 0) {
        fprintf(stderr, "Failed to dup stderr for test '%s'\n", label);
        fclose(capture);
        free((char*)opts.filename);
        unlink(template);
        return;
    }

    /* Redirect stderr -> capture */
    fflush(stderr);
    if (dup2(fileno(capture), fileno(stderr)) < 0) {
        fprintf(stderr, "Failed to redirect stderr for test '%s'\n", label);
        /* restore and cleanup */
        dup2(old_stderr_fd, fileno(stderr));
        close(old_stderr_fd);
        fclose(capture);
        free((char*)opts.filename);
        unlink(template);
        return;
    }

    /* Run compiler while timing this test */
    double t0 = now_seconds();
    int exit_code = run_compiler(&opts);
    double t1 = now_seconds();

    /* Restore stderr */
    fflush(stderr);
    dup2(old_stderr_fd, fileno(stderr));
    close(old_stderr_fd);

    /* Read captured stderr tail */
    size_t cap_len = 0;
    char *captured = read_file_tail(capture, MAX_CAPTURE_BYTES, &cap_len);
    fclose(capture);

    bool passed = ((exit_code == EXIT_SUCCESS) && !should_fail) ||
                  ((exit_code != EXIT_SUCCESS) && should_fail);

    if (passed) {
        g_tests_passed++;
        /* Quiet on success — uncomment the next line to show progress dots */
        /* putchar('.'); fflush(stdout); */
    } else {
        /* Failure: print concise diagnostics */
        printf("❌ Test '%s' failed (%.3f s)\n", label, t1 - t0);
        printf("   expected: %s\n", should_fail ? "failure" : "success");
        printf("   got     : %s (exit code %d)\n",
               (exit_code == EXIT_SUCCESS) ? "success" : "failure", exit_code);

        if (cap_len > 0 && captured) {
            /* If captured output is huge, show the last TAIL_BYTES */
            if (cap_len > TAIL_BYTES) {
                printf("---- captured stderr (last %d bytes) ----\n", TAIL_BYTES);
                size_t start = cap_len - TAIL_BYTES;
                fwrite(captured + start, 1, TAIL_BYTES, stdout);
                printf("\n---- (truncated, %zu bytes omitted earlier) ----\n", cap_len - TAIL_BYTES);
            } else {
                printf("---- captured stderr ----\n%s\n", captured);
            }
        } else {
            printf("(no captured diagnostics)\n");
        }
    }

    free(captured);
    free((char*)opts.filename);
    unlink(template);
}

/* Updated run_all_tests: include all your original RUN tests */
static void run_all_tests(void) {
    g_tests_run = 0;
    g_tests_passed = 0;

    double t_start = now_seconds();

    /* ----- Declarations & simple constructs ----- */
    RUN("x: i32 = 10;", "simple variable declaration", false);
    RUN("x: i32;", "variable declaration without initializer", false);
    RUN("y: i32 = 10 + 15 + x;", "variable declaration with expression", false);

    /* Parse errors for malformed declarations */
    RUN("x = 10;", "missing type declaration", true);
    RUN("123: i32 = 10;", "invalid identifier (number)", true);

    /* ----- Arrays & initializers (parser-level) ----- */
    RUN("arr: i32[5] = { 1, 2, 3, 4, 5 };", "simple array declaration", false);
    RUN("arr: i32[5] = { 1, 2, 3, };", "trailing comma in initializer (malformed)", true);

    /* ----- Functions & params ----- */
    RUN("fn add(a: i32, b: i32) -> i32 { return a + b; }", "simple function", false);
    RUN("fn no_params() -> i32 { return 42; }", "function with no params", false);
    RUN("fn test( { }", "missing parameter list closing", true);
    RUN("fn test(a b: i32) { }", "missing colon in parameter", true);

    /* ----- Expressions (parser-level) ----- */
    RUN("fn main() { x: i32 = 1 + 2 * 3 - 4 / 2; }", "arithmetic precedence", false);
    RUN("fn main() { x: i32 = func(a, b, c); }", "function call expression", false);
    RUN("fn main() { x: i32 = arr[i + 1]; }", "array access expression", false);
    RUN("fn main() { x: i32 = (1 + 2; }", "unmatched parenthesis", true);

    /* ----- If/else: only allowed with braces in this parser ----- */
    RUN("fn main() { if (a > b) { return a; } else { return b; } }", "if-else with braces", false);
    RUN("fn main() { if (1) return; }", "if without braces (not supported)", true);

    /* ----- Comments & strings (single-line comments only) ----- */
    RUN("// single comment\nfn main() { return; }", "single line comment", false);
    RUN("fn main() { s: str = \"unterminated; }", "unterminated string literal", true);

    /* ----- Edge cases ----- */
    RUN("", "empty program", false);
    RUN("   \n\t  ", "whitespace only program", false);
    RUN("fn main() { }", "empty main function", false);
    RUN("fn main() {} junk", "trailing tokens after program", true);
    RUN(";", "single semicolon token", true);


    /* 1) Deeply nested declarations & blocks */
    RUN(
        "fn main() { "
        "a1: i32 = 1; { b1: i32 = 2; { c1: i32 = 3; { d1: i32 = 4; { e1: i32 = 5; "
        "{ f1: i32 = 6; { g1: i32 = 7; { h1: i32 = 8; { i1: i32 = 9; { j1: i32 = 10; } } } } } } } } } } ",
        "deeply nested declarations and blocks", false
    );

    /* 2) Long recursive function with nested expressions (synthetic heavy parser load) */
    RUN(
        "fn long_rec(n: i32) -> i32 { "
        "if (n <= 1) { return n; } else { "
        "return long_rec(n-1) + ( (n * (n-1)) / ((n-2) + 1) ) - ( (n+1) - (n-3) ); "
        "} } "
        "fn main() { x: i32 = long_rec(10); }",
        "long recursive function with nested arithmetic", false
    );

    /* 3) Large synthetic algorithm: many statements and nested blocks */
    RUN(
        "fn big_algo() -> i32 { "
        "a: i32 = 0; b: i32 = 1; c: i32 = 2; d: i32 = 3; e: i32 = 4; f: i32 = 5; "
        "{ x1: i32 = a + b + c + d + e + f; { y1: i32 = x1 * (a + 1); { z1: i32 = y1 - (b + 2); } } } "
        "{ x2: i32 = a - b + c - d + e - f; { y2: i32 = x2 * (b + 3); { z2: i32 = y2 / (c + 1); } } } "
        "return a + b + c + d + e + f; } "
        "fn main() { r: i32 = big_algo(); }",
        "large synthetic algorithm with many declarations and nested blocks", false
    );

    /* 4) Recursive binary search (returns index or -1) */
    RUN(
      "fn bin_search_rec(arr: i32[], lo: i32, hi: i32, key: i32) -> i32 { "
      "if (lo > hi) { return -1; } "
      "mid: i32 = lo + (hi - lo) / 2; "
      "if (arr[mid] == key) { return mid; } else { "
      "if (arr[mid] < key) { return bin_search_rec(arr, mid + 1, hi, key); } "
      "else { return bin_search_rec(arr, lo, mid - 1, key); } } } "
      "fn main() { a: i32[9] = { 1,2,3,4,5,6,7,8,9 }; idx: i32 = bin_search_rec(a, 0, 8, 7); }",
      "recursive binary search (parser stress)",
      false
    );

    /* 5) Recursive insertion sort (sorts first n elements) */
    RUN(
      "fn insert_into_sorted(a: i32[], n: i32) { "
      "if (n <= 1) { return; } "
      "insert_into_sorted(a, n - 1); "
      "key: i32 = a[n - 1]; i: i32 = n - 2; "
      "while_shift: i32 = 0;"
      "idx: i32 = n - 1; "
      "loop_shift: i32 = -1;"
      "tmp_i: i32 = idx; "
      "while_swap: i32 = 0;"
      "rec_bubble_swap(a, idx, key); "
      "return; } "
      "fn rec_bubble_swap(a: i32[], pos: i32, key: i32) { "
      "if (pos <= 0) { a[0] = key; return; } "
      "if (a[pos - 1] <= key) { a[pos] = key; return; } "
      "tmp: i32 = a[pos - 1]; a[pos] = tmp; rec_bubble_swap(a, pos - 1, key); } "
      "fn insertion_sort(a: i32[], n: i32) { "
      "if (n <= 1) {return;} insertion_sort(a, n - 1); insert_into_sorted(a, n); } "
      "fn main() { arr: i32[7] = { 5,3,8,1,2,7,4 }; insertion_sort(arr, 7); }",
      "recursive insertion-sort (parser-only)",
      false
    );

    /* 6) Recursive selection sort (select max and place at end via recursion) */
    RUN(
      "fn find_max_index(a: i32[], n: i32, i: i32, current_max: i32, current_idx: i32) -> i32 { "
      "if (i >= n) { return current_idx; } "
      "if (a[i] > current_max) { return find_max_index(a, n, i + 1, a[i], i); } "
      "else { return find_max_index(a, n, i + 1, current_max, current_idx); } } "
      "fn sel_sort_recursive(a: i32[], n: i32) { "
      "if (n <= 1) {return;} "
      "max_idx: i32 = find_max_index(a, n, 0, a[0], 0); "
      "tmp: i32 = a[max_idx]; a[max_idx] = a[n - 1]; a[n - 1] = tmp; "
      "sel_sort_recursive(a, n - 1); } "
      "fn main() { a: i32[8] = { 9,4,6,1,8,2,7,3 }; sel_sort_recursive(a, 8); }",
      "recursive selection-sort (parser-only)",
      false
    );

    /* 7) Quicksort (recursive) with partition using indexes and swaps */
    RUN(
      "fn partition(a: i32[], lo: i32, hi: i32) -> i32 { "
      "pivot: i32 = a[hi]; i: i32 = lo - 1; j: i32 = lo; "
      "part_loop: i32 = lo;"
      "return partition_rec(a, lo, hi, lo, lo - 1, pivot); } "
      "fn partition_rec(a: i32[], lo: i32, hi: i32, j: i32, i: i32, pivot: i32) -> i32 { "
      "if (j >= hi) { tmp: i32 = a[i + 1]; a[i + 1] = a[hi]; a[hi] = tmp; return i + 1; } "
      "if (a[j] <= pivot) { i2: i32 = i + 1; tmp2: i32 = a[i2]; a[i2] = a[j]; a[j] = tmp2; return partition_rec(a, lo, hi, j + 1, i2, pivot); } "
      "else { return partition_rec(a, lo, hi, j + 1, i, pivot); } } "
      "fn quicksort(a: i32[], lo: i32, hi: i32) { "
      "if (lo < hi) { p: i32 = partition(a, lo, hi); quicksort(a, lo, p - 1); quicksort(a, p + 1, hi); } } "
      "fn main() { arr: i32[9] = { 30,3,4,20,5,1,17,12,9 }; quicksort(arr, 0, 8); }",
      "recursive quicksort with recursive partition (parser-heavy)",
      false
    );

    /* 8) Large combined test: binary search + quicksort + selection on one big array */
    RUN(
      "fn combined_test() { "
      "a: i32[25] = { 25,24,23,22,21,20,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1 }; "
      "quicksort(a, 0, 24); "
      "idx: i32 = bin_search_rec(a, 0, 24, 13); "
      "sel_sort_recursive(a, 25);"
      "return; } "
      "fn main() { combined_test(); }",
      "combined stress: quicksort + binary search + selection sort",
      false
    );


    double t_end = now_seconds();

    int failed = g_tests_run - g_tests_passed;
    printf("Tests: %d run, %d passed, %d failed (%.3f s total)\n",
           g_tests_run, g_tests_passed, failed, t_end - t_start);
}

/* Expose run_all_tests for main.c */
void tests_run_all(void) {
    run_all_tests();
}
