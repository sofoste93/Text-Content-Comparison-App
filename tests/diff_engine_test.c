#include "diff_engine.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "Check failed at %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

static void test_changed_and_added_lines(void) {
    DiffResult result;
    char error[128];
    bool success = diff_compare("alpha\nbeta\ngamma", "alpha\nBETA\ngamma\ndelta",
                                (DiffOptions){0}, &result, error, sizeof(error));
    CHECK(success);
    CHECK(result.unchanged == 2);
    CHECK(result.changed == 1);
    CHECK(result.added == 1);
    CHECK(result.removed == 0);
    diff_result_free(&result);
}

static void test_normalization(void) {
    DiffResult result;
    char error[128];
    DiffOptions options = {.ignore_case = true, .ignore_whitespace = true};
    bool success = diff_compare("  Hello    World  ", "hello world", options,
                                &result, error, sizeof(error));
    CHECK(success);
    CHECK(result.unchanged == 1);
    CHECK(result.changed == 0);
    diff_result_free(&result);
}

static void test_report(void) {
    DiffResult result;
    char error[128];
    bool success = diff_compare("left", "right", (DiffOptions){0},
                                &result, error, sizeof(error));
    CHECK(success);
    char *report = diff_create_report(&result, "before.txt", "after.txt", (DiffOptions){0});
    CHECK(report != NULL);
    CHECK(strstr(report, "before.txt") != NULL);
    CHECK(strstr(report, "~") != NULL);
    free(report);
    diff_result_free(&result);
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    puts("test: changed and added lines");
    test_changed_and_added_lines();
    puts("test: normalization");
    test_normalization();
    puts("test: report");
    test_report();
    puts("diff_engine_test: all checks passed");
    return 0;
}
