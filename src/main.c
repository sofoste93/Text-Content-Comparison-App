#include "diff_engine.h"
#include "raylib.h"

#define RAYGUI_IMPLEMENTATION
#include "raygui.h"
#include "tinyfiledialogs.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEXT_CAPACITY 262144
#define PATH_CAPACITY 1024
#define STATUS_CAPACITY 256

typedef enum { THEME_DARK, THEME_LIGHT } AppTheme;
typedef enum { MODAL_NONE, MODAL_SETTINGS, MODAL_HELP } Modal;

typedef struct {
    char left_text[TEXT_CAPACITY];
    char right_text[TEXT_CAPACITY];
    char left_path[PATH_CAPACITY];
    char right_path[PATH_CAPACITY];
    char search[128];
    char status[STATUS_CAPACITY];
    bool edit_left;
    bool edit_right;
    bool edit_search;
    bool ignore_case;
    bool ignore_whitespace;
    bool changes_only;
    AppTheme theme;
    Modal modal;
    DiffResult diff;
    bool compared;
    float result_scroll;
} AppState;

typedef struct {
    Color background;
    Color surface;
    Color elevated;
    Color border;
    Color text;
    Color muted;
    Color accent;
    Color accent_soft;
    Color added;
    Color removed;
    Color changed;
    Color equal;
} Palette;

static Palette palette_for(AppTheme theme) {
    if (theme == THEME_LIGHT) return (Palette){
        .background = {243, 247, 250, 255}, .surface = {255, 255, 255, 255},
        .elevated = {234, 241, 246, 255}, .border = {199, 213, 224, 255},
        .text = {21, 39, 52, 255}, .muted = {89, 111, 126, 255},
        .accent = {0, 143, 168, 255}, .accent_soft = {206, 240, 244, 255},
        .added = {26, 127, 86, 255}, .removed = {190, 62, 76, 255},
        .changed = {183, 116, 26, 255}, .equal = {95, 116, 130, 255}
    };
    return (Palette){
        .background = {8, 18, 27, 255}, .surface = {14, 30, 42, 255},
        .elevated = {20, 41, 56, 255}, .border = {43, 70, 86, 255},
        .text = {226, 241, 247, 255}, .muted = {139, 166, 179, 255},
        .accent = {42, 201, 208, 255}, .accent_soft = {20, 71, 81, 255},
        .added = {76, 213, 151, 255}, .removed = {255, 112, 128, 255},
        .changed = {255, 190, 92, 255}, .equal = {130, 154, 168, 255}
    };
}

static void apply_gui_style(Palette p) {
    GuiLoadStyleDefault();
    GuiSetStyle(DEFAULT, TEXT_SIZE, 16);
    GuiSetStyle(DEFAULT, BORDER_WIDTH, 1);
    GuiSetStyle(DEFAULT, BACKGROUND_COLOR, ColorToInt(p.background));
    GuiSetStyle(DEFAULT, LINE_COLOR, ColorToInt(p.border));
    GuiSetStyle(DEFAULT, TEXT_COLOR_NORMAL, ColorToInt(p.text));
    GuiSetStyle(DEFAULT, TEXT_COLOR_FOCUSED, ColorToInt(p.text));
    GuiSetStyle(DEFAULT, TEXT_COLOR_PRESSED, ColorToInt(p.text));
    GuiSetStyle(DEFAULT, BASE_COLOR_NORMAL, ColorToInt(p.surface));
    GuiSetStyle(DEFAULT, BASE_COLOR_FOCUSED, ColorToInt(p.elevated));
    GuiSetStyle(DEFAULT, BASE_COLOR_PRESSED, ColorToInt(p.accent_soft));
    GuiSetStyle(DEFAULT, BORDER_COLOR_NORMAL, ColorToInt(p.border));
    GuiSetStyle(DEFAULT, BORDER_COLOR_FOCUSED, ColorToInt(p.accent));
    GuiSetStyle(DEFAULT, BORDER_COLOR_PRESSED, ColorToInt(p.accent));
    GuiSetStyle(TEXTBOX, TEXT_ALIGNMENT_VERTICAL, TEXT_ALIGN_TOP);
    GuiSetStyle(TEXTBOX, TEXT_PADDING, 9);
    GuiSetStyle(BUTTON, BORDER_WIDTH, 0);
}

static const char *file_name_or(const char *path, const char *fallback) {
    if (path[0] == '\0') return fallback;
    const char *name = path;
    for (const char *cursor = path; *cursor != '\0'; ++cursor)
        if (*cursor == '/' || *cursor == '\\') name = cursor + 1;
    return name;
}

static bool read_text_file(const char *path, char *buffer, size_t capacity,
                           char *error, size_t error_size) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        snprintf(error, error_size, "Could not open %s", file_name_or(path, "the file"));
        return false;
    }
    size_t length = fread(buffer, 1, capacity - 1, file);
    bool too_large = !feof(file);
    fclose(file);
    if (too_large) {
        buffer[0] = '\0';
        snprintf(error, error_size, "The file is larger than 256 KiB.");
        return false;
    }
    buffer[length] = '\0';
    if (length >= 3 && (unsigned char)buffer[0] == 0xef &&
        (unsigned char)buffer[1] == 0xbb && (unsigned char)buffer[2] == 0xbf) {
        memmove(buffer, buffer + 3, length - 2);
    }
    return true;
}

static void open_document(AppState *app, bool left_side) {
    const char *filters[] = {"*.txt", "*.md", "*.csv", "*.json", "*.xml", "*.log", "*.c", "*.h", "*.py", "*.java"};
    const char *path = tinyfd_openFileDialog(left_side ? "Open the left document" : "Open the right document",
                                              "", 10, filters, "Text and source files", 0);
    if (path == NULL) return;
    char *text = left_side ? app->left_text : app->right_text;
    char *saved_path = left_side ? app->left_path : app->right_path;
    if (read_text_file(path, text, TEXT_CAPACITY, app->status, sizeof(app->status))) {
        snprintf(saved_path, PATH_CAPACITY, "%s", path);
        snprintf(app->status, sizeof(app->status), "%s loaded", file_name_or(path, "Document"));
        app->compared = false;
    }
}

static void compare_documents(AppState *app) {
    if (app->left_text[0] == '\0' || app->right_text[0] == '\0') {
        snprintf(app->status, sizeof(app->status), "Add text or open a file on both sides first.");
        return;
    }
    diff_result_free(&app->diff);
    DiffOptions options = {app->ignore_case, app->ignore_whitespace};
    if (diff_compare(app->left_text, app->right_text, options, &app->diff,
                     app->status, sizeof(app->status))) {
        app->compared = true;
        app->result_scroll = 0;
        size_t differences = app->diff.added + app->diff.removed + app->diff.changed;
        snprintf(app->status, sizeof(app->status), differences == 0
                 ? "Documents match with the current options."
                 : "%zu difference rows found.", differences);
    }
}

static void export_report(AppState *app) {
    if (!app->compared) {
        snprintf(app->status, sizeof(app->status), "Run a comparison before exporting a report.");
        return;
    }
    const char *filters[] = {"*.txt"};
    const char *path = tinyfd_saveFileDialog("Save comparison report", "text-orbit-report.txt",
                                             1, filters, "Text report");
    if (path == NULL) return;
    char *report = diff_create_report(&app->diff,
                                      file_name_or(app->left_path, "Left text"),
                                      file_name_or(app->right_path, "Right text"),
                                      (DiffOptions){app->ignore_case, app->ignore_whitespace});
    if (report == NULL) {
        snprintf(app->status, sizeof(app->status), "Not enough memory to create the report.");
        return;
    }
    FILE *file = fopen(path, "wb");
    if (file == NULL) snprintf(app->status, sizeof(app->status), "Could not save the report.");
    else {
        fwrite(report, 1, strlen(report), file);
        fclose(file);
        snprintf(app->status, sizeof(app->status), "Report saved to %s", file_name_or(path, "file"));
    }
    free(report);
}

static void clear_documents(AppState *app) {
    app->left_text[0] = app->right_text[0] = '\0';
    app->left_path[0] = app->right_path[0] = '\0';
    app->search[0] = '\0';
    app->compared = false;
    app->result_scroll = 0;
    diff_result_free(&app->diff);
    snprintf(app->status, sizeof(app->status), "Workspace cleared. Ready for two new documents.");
}

static void swap_documents(AppState *app) {
    char *text_copy = malloc(TEXT_CAPACITY);
    if (text_copy == NULL) return;
    memcpy(text_copy, app->left_text, TEXT_CAPACITY);
    memcpy(app->left_text, app->right_text, TEXT_CAPACITY);
    memcpy(app->right_text, text_copy, TEXT_CAPACITY);
    free(text_copy);
    char path_copy[PATH_CAPACITY];
    memcpy(path_copy, app->left_path, PATH_CAPACITY);
    memcpy(app->left_path, app->right_path, PATH_CAPACITY);
    memcpy(app->right_path, path_copy, PATH_CAPACITY);
    app->compared = false;
    snprintf(app->status, sizeof(app->status), "Left and right documents swapped.");
}

static bool contains_case_insensitive(const char *text, const char *query) {
    if (query[0] == '\0') return true;
    for (const char *start = text; *start; ++start) {
        size_t i = 0;
        while (query[i] && start[i] && tolower((unsigned char)start[i]) == tolower((unsigned char)query[i])) ++i;
        if (query[i] == '\0') return true;
    }
    return false;
}

static bool row_is_visible(const AppState *app, const DiffRow *row) {
    if (app->changes_only && row->kind == DIFF_EQUAL) return false;
    return contains_case_insensitive(row->left_text, app->search) ||
           contains_case_insensitive(row->right_text, app->search);
}

static size_t visible_row_count(const AppState *app) {
    size_t count = 0;
    for (size_t i = 0; i < app->diff.count; ++i)
        if (row_is_visible(app, &app->diff.rows[i])) ++count;
    return count;
}

static void draw_logo(Vector2 center, float size, Palette p) {
    DrawCircleV(center, size, p.accent);
    DrawCircleV((Vector2){center.x + size * 0.18f, center.y - size * 0.08f}, size * 0.60f, p.surface);
    DrawLineEx((Vector2){center.x - size * 0.58f, center.y + size * 0.22f},
               (Vector2){center.x + size * 0.42f, center.y + size * 0.22f}, size * 0.16f, p.text);
    DrawCircleV((Vector2){center.x - size * 0.56f, center.y + size * 0.22f}, size * 0.12f, p.changed);
    DrawCircleV((Vector2){center.x + size * 0.45f, center.y + size * 0.22f}, size * 0.12f, p.added);
}

static void draw_panel(Rectangle bounds, Palette p) {
    DrawRectangleRounded(bounds, 0.035f, 10, p.surface);
    DrawRectangleRoundedLinesEx(bounds, 0.035f, 10, 1.0f, p.border);
}

static void draw_stat_card(Rectangle bounds, const char *label, size_t value, Color color, Palette p) {
    DrawRectangleRounded(bounds, 0.16f, 8, p.elevated);
    DrawCircle((int)bounds.x + 14, (int)bounds.y + 14, 4, color);
    DrawText(TextFormat("%zu", value), (int)bounds.x + 26, (int)bounds.y + 6, 18, p.text);
    DrawText(label, (int)bounds.x + 12, (int)bounds.y + 31, 12, p.muted);
}

static void draw_clipped_text(const char *text, int x, int y, int width, int font_size, Color color) {
    int max_chars = width / (font_size / 2 + 1);
    if ((int)strlen(text) <= max_chars) DrawText(text, x, y, font_size, color);
    else if (max_chars > 3) {
        char shortened[512];
        int count = max_chars - 3;
        if (count > (int)sizeof(shortened) - 4) count = (int)sizeof(shortened) - 4;
        memcpy(shortened, text, (size_t)count);
        memcpy(shortened + count, "...", 4);
        DrawText(shortened, x, y, font_size, color);
    }
}

static void draw_diff_rows(AppState *app, Rectangle bounds, Palette p) {
    const float row_height = 28.0f;
    const size_t visible_count = visible_row_count(app);
    float maximum_scroll = (float)visible_count * row_height - bounds.height;
    if (maximum_scroll < 0) maximum_scroll = 0;
    if (CheckCollisionPointRec(GetMousePosition(), bounds)) app->result_scroll -= GetMouseWheelMove() * row_height * 3;
    if (app->result_scroll < 0) app->result_scroll = 0;
    if (app->result_scroll > maximum_scroll) app->result_scroll = maximum_scroll;

    BeginScissorMode((int)bounds.x, (int)bounds.y, (int)bounds.width, (int)bounds.height);
    size_t visible_index = 0;
    for (size_t i = 0; i < app->diff.count; ++i) {
        const DiffRow *row = &app->diff.rows[i];
        if (!row_is_visible(app, row)) continue;
        float y = bounds.y + visible_index * row_height - app->result_scroll;
        ++visible_index;
        if (y + row_height < bounds.y || y > bounds.y + bounds.height) continue;
        Color marker = p.equal;
        Color background = p.surface;
        if (row->kind == DIFF_ADDED) { marker = p.added; background = Fade(p.added, 0.10f); }
        else if (row->kind == DIFF_REMOVED) { marker = p.removed; background = Fade(p.removed, 0.10f); }
        else if (row->kind == DIFF_CHANGED) { marker = p.changed; background = Fade(p.changed, 0.11f); }
        if ((visible_index % 2 == 0) && row->kind == DIFF_EQUAL) background = Fade(p.elevated, 0.55f);
        DrawRectangle((int)bounds.x, (int)y, (int)bounds.width, (int)row_height - 1, background);
        DrawRectangle((int)bounds.x, (int)y, 3, (int)row_height - 1, marker);
        const float half = bounds.width / 2.0f;
        DrawLine((int)(bounds.x + half), (int)y, (int)(bounds.x + half), (int)(y + row_height), p.border);
        const char *symbol = row->kind == DIFF_EQUAL ? " " : row->kind == DIFF_ADDED ? "+" : row->kind == DIFF_REMOVED ? "-" : "~";
        DrawText(symbol, (int)bounds.x + 9, (int)y + 6, 15, marker);
        DrawText(row->left_line ? TextFormat("%d", row->left_line) : "", (int)bounds.x + 28, (int)y + 7, 13, p.muted);
        DrawText(row->right_line ? TextFormat("%d", row->right_line) : "", (int)(bounds.x + half) + 12, (int)y + 7, 13, p.muted);
        draw_clipped_text(row->left_text, (int)bounds.x + 66, (int)y + 6, (int)half - 76, 15, p.text);
        draw_clipped_text(row->right_text, (int)(bounds.x + half) + 50, (int)y + 6, (int)half - 60, 15, p.text);
    }
    EndScissorMode();
    if (visible_count == 0) {
        const char *message = app->compared ? "No rows match this filter" : "Open or paste two documents, then compare";
        int width = MeasureText(message, 18);
        DrawText(message, (int)(bounds.x + bounds.width / 2 - width / 2),
                 (int)(bounds.y + bounds.height / 2), 18, p.muted);
    }
    if (maximum_scroll > 0) {
        float thumb_height = bounds.height * bounds.height / ((float)visible_count * row_height);
        if (thumb_height < 32) thumb_height = 32;
        float thumb_y = bounds.y + (bounds.height - thumb_height) * app->result_scroll / maximum_scroll;
        DrawRectangleRounded((Rectangle){bounds.x + bounds.width - 6, thumb_y, 4, thumb_height}, 1.0f, 4, p.accent);
    }
}

static void process_shortcuts(AppState *app) {
    bool control = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    if (control && IsKeyPressed(KEY_ONE)) open_document(app, true);
    if (control && IsKeyPressed(KEY_TWO)) open_document(app, false);
    if (control && IsKeyPressed(KEY_ENTER)) compare_documents(app);
    if (control && IsKeyPressed(KEY_S)) export_report(app);
    if (control && IsKeyPressed(KEY_L)) clear_documents(app);
    if (IsKeyPressed(KEY_F1)) app->modal = MODAL_HELP;
}

static void process_file_drop(AppState *app) {
    if (!IsFileDropped()) return;
    FilePathList files = LoadDroppedFiles();
    for (unsigned int i = 0; i < files.count && i < 2; ++i) {
        bool left_side = i == 0 && app->left_text[0] == '\0';
        if (i == 1 || !left_side) left_side = false;
        char *text = left_side ? app->left_text : app->right_text;
        char *path = left_side ? app->left_path : app->right_path;
        if (read_text_file(files.paths[i], text, TEXT_CAPACITY, app->status, sizeof(app->status)))
            snprintf(path, PATH_CAPACITY, "%s", files.paths[i]);
    }
    UnloadDroppedFiles(files);
    app->compared = false;
}

static void draw_modal(AppState *app, Palette p) {
    if (app->modal == MODAL_NONE) return;
    GuiLock();
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.56f));
    GuiUnlock();
    float width = 560, height = app->modal == MODAL_HELP ? 420 : 270;
    Rectangle box = {(GetScreenWidth() - width) / 2, (GetScreenHeight() - height) / 2, width, height};
    DrawRectangleRounded(box, 0.035f, 10, p.surface);
    DrawRectangleRoundedLinesEx(box, 0.035f, 10, 1, p.border);
    const char *title = app->modal == MODAL_HELP ? "Flight manual" : "Settings";
    DrawText(title, (int)box.x + 28, (int)box.y + 24, 26, p.text);
    if (GuiButton((Rectangle){box.x + box.width - 48, box.y + 18, 30, 30}, "x")) app->modal = MODAL_NONE;

    if (app->modal == MODAL_SETTINGS) {
        DrawText("Appearance", (int)box.x + 28, (int)box.y + 80, 16, p.muted);
        bool dark = app->theme == THEME_DARK;
        bool light = app->theme == THEME_LIGHT;
        if (GuiToggle((Rectangle){box.x + 28, box.y + 110, 145, 42}, "Deep space", &dark) && dark) {
            app->theme = THEME_DARK; apply_gui_style(palette_for(app->theme));
        }
        if (GuiToggle((Rectangle){box.x + 184, box.y + 110, 145, 42}, "Daylight", &light) && light) {
            app->theme = THEME_LIGHT; apply_gui_style(palette_for(app->theme));
        }
        DrawText("Appearance applies immediately to this session.", (int)box.x + 28,
                 (int)box.y + 178, 15, p.muted);
    } else {
        const char *steps[] = {
            "1  Open or paste a document into each editor.",
            "2  Select case and whitespace options.",
            "3  Compare, filter the result, then export the report.",
            "", "Shortcuts", "Ctrl+1 / Ctrl+2   Open left / right",
            "Ctrl+Enter          Compare", "Ctrl+S              Export report",
            "Ctrl+L              Clear", "F1                  Open this manual"
        };
        for (int i = 0; i < 10; ++i)
            DrawText(steps[i], (int)box.x + 28, (int)box.y + 76 + i * 29, i == 4 ? 18 : 15,
                     i == 4 ? p.accent : (i < 3 ? p.text : p.muted));
    }
}

static void draw_application(AppState *app) {
    Palette p = palette_for(app->theme);
    const float width = (float)GetScreenWidth();
    const float height = (float)GetScreenHeight();
    const float margin = 24;
    ClearBackground(p.background);

    DrawRectangleGradientH(0, 0, (int)width, 82, p.surface, p.elevated);
    draw_logo((Vector2){48, 41}, 22, p);
    DrawText("TEXT ORBIT", 82, 19, 25, p.text);
    DrawText("content comparison console", 83, 49, 14, p.muted);
    DrawText("C / NATIVE", (int)width - 126, 20, 13, p.accent);
    DrawText("v2.0", (int)width - 75, 46, 13, p.muted);

    float toolbar_y = 96;
    if (GuiButton((Rectangle){margin, toolbar_y, 108, 38}, "Open left")) open_document(app, true);
    if (GuiButton((Rectangle){margin + 116, toolbar_y, 108, 38}, "Open right")) open_document(app, false);
    if (GuiButton((Rectangle){margin + 232, toolbar_y, 76, 38}, "Swap")) swap_documents(app);
    if (GuiButton((Rectangle){margin + 320, toolbar_y, 108, 38}, "Compare")) compare_documents(app);
    if (GuiButton((Rectangle){margin + 436, toolbar_y, 98, 38}, "Export")) export_report(app);
    if (GuiButton((Rectangle){margin + 542, toolbar_y, 76, 38}, "Clear")) clear_documents(app);
    if (GuiButton((Rectangle){width - 206, toolbar_y, 86, 38}, "Settings")) app->modal = MODAL_SETTINGS;
    if (GuiButton((Rectangle){width - 110, toolbar_y, 86, 38}, "Help")) app->modal = MODAL_HELP;

    const float gap = 16;
    const float column_width = (width - margin * 2 - gap) / 2;
    const float editor_y = 154;
    const float editor_height = height < 760 ? 170 : 220;
    Rectangle left_panel = {margin, editor_y, column_width, editor_height};
    Rectangle right_panel = {margin + column_width + gap, editor_y, column_width, editor_height};
    draw_panel(left_panel, p); draw_panel(right_panel, p);
    DrawText("A  ORIGINAL", (int)left_panel.x + 14, (int)left_panel.y + 12, 14, p.accent);
    DrawText(file_name_or(app->left_path, "Paste text or open a file"), (int)left_panel.x + 118,
             (int)left_panel.y + 12, 14, p.muted);
    DrawText("B  REVISED", (int)right_panel.x + 14, (int)right_panel.y + 12, 14, p.accent);
    DrawText(file_name_or(app->right_path, "Paste text or open a file"), (int)right_panel.x + 108,
             (int)right_panel.y + 12, 14, p.muted);
    Rectangle left_editor = {left_panel.x + 10, left_panel.y + 38, left_panel.width - 20, left_panel.height - 48};
    Rectangle right_editor = {right_panel.x + 10, right_panel.y + 38, right_panel.width - 20, right_panel.height - 48};
    if (GuiTextBox(left_editor, app->left_text, TEXT_CAPACITY, app->edit_left)) app->edit_left = !app->edit_left;
    if (GuiTextBox(right_editor, app->right_text, TEXT_CAPACITY, app->edit_right)) app->edit_right = !app->edit_right;

    float options_y = editor_y + editor_height + 14;
    GuiCheckBox((Rectangle){margin, options_y + 5, 20, 20}, "Ignore case", &app->ignore_case);
    GuiCheckBox((Rectangle){margin + 148, options_y + 5, 20, 20}, "Ignore whitespace", &app->ignore_whitespace);
    GuiCheckBox((Rectangle){margin + 338, options_y + 5, 20, 20}, "Changes only", &app->changes_only);
    DrawText("Search result", (int)width - 330, (int)options_y + 8, 14, p.muted);
    Rectangle search_box = {width - 220, options_y, 196, 32};
    if (GuiTextBox(search_box, app->search, sizeof(app->search), app->edit_search)) app->edit_search = !app->edit_search;

    float result_y = options_y + 46;
    float footer_height = 34;
    Rectangle result_panel = {margin, result_y, width - margin * 2, height - result_y - footer_height - 12};
    draw_panel(result_panel, p);
    DrawText("COMPARISON STREAM", (int)result_panel.x + 14, (int)result_panel.y + 15, 15, p.accent);
    const float card_width = 104;
    draw_stat_card((Rectangle){result_panel.x + result_panel.width - card_width * 4 - 38,
                               result_panel.y + 8, card_width - 8, 55}, "changed", app->diff.changed, p.changed, p);
    draw_stat_card((Rectangle){result_panel.x + result_panel.width - card_width * 3 - 38,
                               result_panel.y + 8, card_width - 8, 55}, "added", app->diff.added, p.added, p);
    draw_stat_card((Rectangle){result_panel.x + result_panel.width - card_width * 2 - 38,
                               result_panel.y + 8, card_width - 8, 55}, "removed", app->diff.removed, p.removed, p);
    draw_stat_card((Rectangle){result_panel.x + result_panel.width - card_width - 38,
                               result_panel.y + 8, card_width - 8, 55}, "same", app->diff.unchanged, p.equal, p);
    float table_y = result_panel.y + 72;
    DrawText("LEFT", (int)result_panel.x + 66, (int)table_y, 12, p.muted);
    DrawText("RIGHT", (int)(result_panel.x + result_panel.width / 2) + 50, (int)table_y, 12, p.muted);
    draw_diff_rows(app, (Rectangle){result_panel.x + 8, table_y + 20, result_panel.width - 16,
                                    result_panel.height - 100}, p);

    DrawCircle(32, (int)height - 17, 4, app->compared ? p.added : p.accent);
    draw_clipped_text(app->status, 44, (int)height - 24, (int)width - 220, 14, p.muted);
    DrawText("Drop files anywhere", (int)width - 166, (int)height - 24, 14, p.muted);
    draw_modal(app, p);
}

static void load_demo(AppState *app) {
    snprintf(app->left_path, PATH_CAPACITY, "mission-before.txt");
    snprintf(app->right_path, PATH_CAPACITY, "mission-after.txt");
    snprintf(app->left_text, TEXT_CAPACITY,
             "Cargo manifest\nRoute: Hamburg > Rotterdam\nContainers: 18\nDeparture: 08:30\nStatus: ready\nCrew: Thor");
    snprintf(app->right_text, TEXT_CAPACITY,
             "Cargo manifest\nRoute: Hamburg > Antwerp > Rotterdam\nContainers: 21\nDeparture: 08:30\nStatus: cleared\nCrew: Thor\nPriority: medical");
    compare_documents(app);
}

int main(int argc, char **argv) {
    AppState app = {0};
    app.theme = THEME_DARK;
    snprintf(app.status, sizeof(app.status), "Ready. Open, paste or drop two text documents.");
    const char *screenshot_path = NULL;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) screenshot_path = argv[++i];
    }

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT | FLAG_WINDOW_HIGHDPI);
    InitWindow(1440, 940, "Text Orbit Compare");
    SetWindowMinSize(1040, 720);
    SetTargetFPS(60);
    apply_gui_style(palette_for(app.theme));
    if (screenshot_path != NULL) load_demo(&app);

    int screenshot_frame = 0;
    while (!WindowShouldClose()) {
        process_shortcuts(&app);
        process_file_drop(&app);
        BeginDrawing();
        draw_application(&app);
        EndDrawing();
        if (screenshot_path != NULL && ++screenshot_frame == 8) {
            TakeScreenshot(screenshot_path);
            break;
        }
    }
    diff_result_free(&app.diff);
    CloseWindow();
    return 0;
}
