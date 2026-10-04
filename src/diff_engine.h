#ifndef TEXT_ORBIT_DIFF_ENGINE_H
#define TEXT_ORBIT_DIFF_ENGINE_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    DIFF_EQUAL,
    DIFF_ADDED,
    DIFF_REMOVED,
    DIFF_CHANGED
} DiffKind;

typedef struct {
    bool ignore_case;
    bool ignore_whitespace;
} DiffOptions;

typedef struct {
    DiffKind kind;
    int left_line;
    int right_line;
    char *left_text;
    char *right_text;
} DiffRow;

typedef struct {
    DiffRow *rows;
    size_t count;
    size_t unchanged;
    size_t added;
    size_t removed;
    size_t changed;
} DiffResult;

bool diff_compare(const char *left, const char *right, DiffOptions options,
                  DiffResult *result, char *error, size_t error_size);
void diff_result_free(DiffResult *result);
char *diff_create_report(const DiffResult *result, const char *left_name,
                         const char *right_name, DiffOptions options);

#endif
