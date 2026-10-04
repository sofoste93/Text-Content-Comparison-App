#include "diff_engine.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_MATRIX_CELLS 16000000ULL

typedef struct {
    char **items;
    size_t count;
} Lines;

typedef struct {
    DiffKind kind;
    size_t index;
} RawStep;

static char *copy_range(const char *start, size_t length) {
    char *copy = malloc(length + 1);
    if (copy == NULL) return NULL;
    memcpy(copy, start, length);
    copy[length] = '\0';
    return copy;
}

static void lines_free(Lines *lines) {
    if (lines == NULL) return;
    for (size_t i = 0; i < lines->count; ++i) free(lines->items[i]);
    free(lines->items);
    lines->items = NULL;
    lines->count = 0;
}

static bool split_lines(const char *text, Lines *lines) {
    memset(lines, 0, sizeof(*lines));
    size_t capacity = 16;
    lines->items = calloc(capacity, sizeof(char *));
    if (lines->items == NULL) return false;

    const char *start = text;
    const char *cursor = text;
    for (;;) {
        if (*cursor == '\n' || *cursor == '\0') {
            size_t length = (size_t)(cursor - start);
            if (length > 0 && start[length - 1] == '\r') --length;
            if (lines->count == capacity) {
                capacity *= 2;
                char **grown = realloc(lines->items, capacity * sizeof(char *));
                if (grown == NULL) {
                    lines_free(lines);
                    return false;
                }
                lines->items = grown;
            }
            lines->items[lines->count] = copy_range(start, length);
            if (lines->items[lines->count] == NULL) {
                lines_free(lines);
                return false;
            }
            ++lines->count;
            if (*cursor == '\0') break;
            start = cursor + 1;
        }
        ++cursor;
    }
    return true;
}

static char *normalize_line(const char *line, DiffOptions options) {
    const size_t length = strlen(line);
    char *normalized = malloc(length + 1);
    if (normalized == NULL) return NULL;

    size_t out = 0;
    bool pending_space = false;
    for (size_t i = 0; i < length; ++i) {
        unsigned char value = (unsigned char)line[i];
        if (options.ignore_whitespace && isspace(value)) {
            if (out > 0) pending_space = true;
            continue;
        }
        if (pending_space) normalized[out++] = ' ';
        pending_space = false;
        normalized[out++] = options.ignore_case ? (char)tolower(value) : (char)value;
    }
    normalized[out] = '\0';
    return normalized;
}

static void normalized_free(char **items, size_t count) {
    if (items == NULL) return;
    for (size_t i = 0; i < count; ++i) free(items[i]);
    free(items);
}

static char **normalize_lines(const Lines *lines, DiffOptions options) {
    char **items = calloc(lines->count, sizeof(char *));
    if (items == NULL) return NULL;
    for (size_t i = 0; i < lines->count; ++i) {
        items[i] = normalize_line(lines->items[i], options);
        if (items[i] == NULL) {
            normalized_free(items, lines->count);
            return NULL;
        }
    }
    return items;
}

static bool append_row(DiffResult *result, DiffKind kind, int left_line,
                       int right_line, const char *left, const char *right) {
    DiffRow *grown = realloc(result->rows, (result->count + 1) * sizeof(DiffRow));
    if (grown == NULL) return false;
    result->rows = grown;
    DiffRow *row = &result->rows[result->count];
    row->kind = kind;
    row->left_line = left_line;
    row->right_line = right_line;
    row->left_text = copy_range(left == NULL ? "" : left, strlen(left == NULL ? "" : left));
    row->right_text = copy_range(right == NULL ? "" : right, strlen(right == NULL ? "" : right));
    if (row->left_text == NULL || row->right_text == NULL) {
        free(row->left_text);
        free(row->right_text);
        return false;
    }
    ++result->count;
    if (kind == DIFF_EQUAL) ++result->unchanged;
    else if (kind == DIFF_ADDED) ++result->added;
    else if (kind == DIFF_REMOVED) ++result->removed;
    else ++result->changed;
    return true;
}

static bool create_linear_fallback(const Lines *left, const Lines *right,
                                   char **left_norm, char **right_norm,
                                   DiffResult *result) {
    const size_t maximum = left->count > right->count ? left->count : right->count;
    for (size_t i = 0; i < maximum; ++i) {
        if (i >= left->count) {
            if (!append_row(result, DIFF_ADDED, 0, (int)i + 1, "", right->items[i])) return false;
        } else if (i >= right->count) {
            if (!append_row(result, DIFF_REMOVED, (int)i + 1, 0, left->items[i], "")) return false;
        } else if (strcmp(left_norm[i], right_norm[i]) == 0) {
            if (!append_row(result, DIFF_EQUAL, (int)i + 1, (int)i + 1, left->items[i], right->items[i])) return false;
        } else if (!append_row(result, DIFF_CHANGED, (int)i + 1, (int)i + 1,
                               left->items[i], right->items[i])) return false;
    }
    return true;
}

static bool create_lcs_diff(const Lines *left, const Lines *right,
                            char **left_norm, char **right_norm,
                            DiffResult *result) {
    const size_t columns = right->count + 1;
    uint32_t *previous = calloc(columns, sizeof(uint32_t));
    uint32_t *current = calloc(columns, sizeof(uint32_t));
    uint8_t *directions = calloc((left->count + 1) * columns, sizeof(uint8_t));
    if (previous == NULL || current == NULL || directions == NULL) {
        free(previous); free(current); free(directions);
        return false;
    }

    for (size_t i = 1; i <= left->count; ++i) {
        for (size_t j = 1; j <= right->count; ++j) {
            const size_t position = i * columns + j;
            if (strcmp(left_norm[i - 1], right_norm[j - 1]) == 0) {
                current[j] = previous[j - 1] + 1;
                directions[position] = 0;
            } else if (previous[j] >= current[j - 1]) {
                current[j] = previous[j];
                directions[position] = 1;
            } else {
                current[j] = current[j - 1];
                directions[position] = 2;
            }
        }
        uint32_t *swap = previous; previous = current; current = swap;
        memset(current, 0, columns * sizeof(uint32_t));
    }
    free(previous); free(current);

    const size_t raw_capacity = left->count + right->count + 1;
    RawStep *raw = calloc(raw_capacity, sizeof(RawStep));
    if (raw == NULL) { free(directions); return false; }
    size_t raw_count = 0;
    size_t i = left->count, j = right->count;
    while (i > 0 || j > 0) {
        if (i > 0 && j > 0 && directions[i * columns + j] == 0 &&
            strcmp(left_norm[i - 1], right_norm[j - 1]) == 0) {
            raw[raw_count++] = (RawStep){DIFF_EQUAL, i - 1}; --i; --j;
        } else if (i > 0 && (j == 0 || directions[i * columns + j] == 1)) {
            raw[raw_count++] = (RawStep){DIFF_REMOVED, i - 1}; --i;
        } else {
            raw[raw_count++] = (RawStep){DIFF_ADDED, j - 1}; --j;
        }
    }
    free(directions);
    for (size_t a = 0; a < raw_count / 2; ++a) {
        RawStep swap = raw[a]; raw[a] = raw[raw_count - 1 - a]; raw[raw_count - 1 - a] = swap;
    }

    size_t cursor = 0;
    while (cursor < raw_count) {
        if (raw[cursor].kind == DIFF_EQUAL) {
            size_t left_index = raw[cursor].index;
            size_t right_index = 0;
            for (size_t k = 0; k <= cursor; ++k) if (raw[k].kind != DIFF_REMOVED) ++right_index;
            --right_index;
            if (!append_row(result, DIFF_EQUAL, (int)left_index + 1, (int)right_index + 1,
                            left->items[left_index], right->items[right_index])) goto failure;
            ++cursor;
            continue;
        }

        size_t end = cursor;
        while (end < raw_count && raw[end].kind != DIFF_EQUAL) ++end;
        size_t remove_count = 0, add_count = 0;
        for (size_t k = cursor; k < end; ++k) {
            if (raw[k].kind == DIFF_REMOVED) ++remove_count; else ++add_count;
        }
        size_t *removes = calloc(remove_count, sizeof(size_t));
        size_t *adds = calloc(add_count, sizeof(size_t));
        if ((remove_count && removes == NULL) || (add_count && adds == NULL)) {
            free(removes); free(adds); goto failure;
        }
        size_t ri = 0, ai = 0;
        for (size_t k = cursor; k < end; ++k) {
            if (raw[k].kind == DIFF_REMOVED) removes[ri++] = raw[k].index;
            else adds[ai++] = raw[k].index;
        }
        const size_t paired = remove_count < add_count ? remove_count : add_count;
        for (size_t k = 0; k < paired; ++k) {
            if (!append_row(result, DIFF_CHANGED, (int)removes[k] + 1, (int)adds[k] + 1,
                            left->items[removes[k]], right->items[adds[k]])) {
                free(removes); free(adds); goto failure;
            }
        }
        for (size_t k = paired; k < remove_count; ++k)
            if (!append_row(result, DIFF_REMOVED, (int)removes[k] + 1, 0,
                            left->items[removes[k]], "")) { free(removes); free(adds); goto failure; }
        for (size_t k = paired; k < add_count; ++k)
            if (!append_row(result, DIFF_ADDED, 0, (int)adds[k] + 1,
                            "", right->items[adds[k]])) { free(removes); free(adds); goto failure; }
        free(removes); free(adds);
        cursor = end;
    }
    free(raw);
    return true;

failure:
    free(raw);
    return false;
}

bool diff_compare(const char *left_text, const char *right_text, DiffOptions options,
                  DiffResult *result, char *error, size_t error_size) {
    if (result == NULL || left_text == NULL || right_text == NULL) return false;
    memset(result, 0, sizeof(*result));
    if (error != NULL && error_size > 0) error[0] = '\0';

    Lines left = {0}, right = {0};
    if (!split_lines(left_text, &left) || !split_lines(right_text, &right)) {
        snprintf(error, error_size, "Not enough memory to split the documents.");
        lines_free(&left); lines_free(&right); return false;
    }
    char **left_norm = normalize_lines(&left, options);
    char **right_norm = normalize_lines(&right, options);
    if (left_norm == NULL || right_norm == NULL) {
        snprintf(error, error_size, "Not enough memory to normalize the documents.");
        normalized_free(left_norm, left.count); normalized_free(right_norm, right.count);
        lines_free(&left); lines_free(&right); return false;
    }

    const uint64_t cells = (uint64_t)(left.count + 1) * (uint64_t)(right.count + 1);
    bool success = cells <= MAX_MATRIX_CELLS
        ? create_lcs_diff(&left, &right, left_norm, right_norm, result)
        : create_linear_fallback(&left, &right, left_norm, right_norm, result);
    if (!success) {
        snprintf(error, error_size, "Not enough memory to create the comparison.");
        diff_result_free(result);
    }
    normalized_free(left_norm, left.count); normalized_free(right_norm, right.count);
    lines_free(&left); lines_free(&right);
    return success;
}

void diff_result_free(DiffResult *result) {
    if (result == NULL) return;
    for (size_t i = 0; i < result->count; ++i) {
        free(result->rows[i].left_text);
        free(result->rows[i].right_text);
    }
    free(result->rows);
    memset(result, 0, sizeof(*result));
}

static bool report_append(char **buffer, size_t *length, size_t *capacity, const char *text) {
    const size_t addition = strlen(text);
    if (*length + addition + 1 > *capacity) {
        while (*length + addition + 1 > *capacity) *capacity *= 2;
        char *grown = realloc(*buffer, *capacity);
        if (grown == NULL) return false;
        *buffer = grown;
    }
    memcpy(*buffer + *length, text, addition + 1);
    *length += addition;
    return true;
}

char *diff_create_report(const DiffResult *result, const char *left_name,
                         const char *right_name, DiffOptions options) {
    size_t capacity = 4096, length = 0;
    char *report = calloc(capacity, 1);
    if (report == NULL) return NULL;
    char header[1024];
    snprintf(header, sizeof(header),
             "Text Orbit Compare 2.0\nLeft: %s\nRight: %s\nOptions: case %s, whitespace %s\n"
             "Summary: %zu changed, %zu added, %zu removed, %zu unchanged\n\n",
             left_name, right_name, options.ignore_case ? "ignored" : "significant",
             options.ignore_whitespace ? "ignored" : "significant", result->changed,
             result->added, result->removed, result->unchanged);
    if (!report_append(&report, &length, &capacity, header)) { free(report); return NULL; }
    for (size_t i = 0; i < result->count; ++i) {
        const DiffRow *row = &result->rows[i];
        char prefix[64];
        if (row->kind == DIFF_EQUAL || row->kind == DIFF_REMOVED) {
            snprintf(prefix, sizeof(prefix), "%c %4d | ", row->kind == DIFF_EQUAL ? ' ' : '-', row->left_line);
            if (!report_append(&report, &length, &capacity, prefix) ||
                !report_append(&report, &length, &capacity, row->left_text) ||
                !report_append(&report, &length, &capacity, "\n")) { free(report); return NULL; }
        } else if (row->kind == DIFF_ADDED) {
            snprintf(prefix, sizeof(prefix), "+ %4d | ", row->right_line);
            if (!report_append(&report, &length, &capacity, prefix) ||
                !report_append(&report, &length, &capacity, row->right_text) ||
                !report_append(&report, &length, &capacity, "\n")) { free(report); return NULL; }
        } else {
            snprintf(prefix, sizeof(prefix), "~ %4d | ", row->left_line);
            if (!report_append(&report, &length, &capacity, prefix) ||
                !report_append(&report, &length, &capacity, row->left_text) ||
                !report_append(&report, &length, &capacity, "\n")) { free(report); return NULL; }
            snprintf(prefix, sizeof(prefix), "~ %4d | ", row->right_line);
            if (!report_append(&report, &length, &capacity, prefix) ||
                !report_append(&report, &length, &capacity, row->right_text) ||
                !report_append(&report, &length, &capacity, "\n")) { free(report); return NULL; }
        }
    }
    return report;
}
