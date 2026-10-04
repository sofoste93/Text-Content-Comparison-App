# Learning tour: from two strings to a visual diff

This tutorial follows one comparison through Text Orbit Compare. The application is deliberately split so the C algorithm can be studied without learning graphics first.

## 1. The public contract

`src/diff_engine.h` contains the data exchanged with the UI:

- `DiffOptions` selects case and whitespace normalization.
- `DiffRow` represents one aligned row from the two documents.
- `DiffResult` owns the row array and its summary counts.
- `diff_compare()` creates a result; `diff_result_free()` releases it.

This pairing is important in C. The function that allocates a result also documents the function that must free it.

## 2. Split and normalize

The engine first splits each input into independently allocated lines. It then creates a normalized copy of every line for matching:

```c
DiffOptions options = {
    .ignore_case = true,
    .ignore_whitespace = true
};
```

Normalization changes only the matching copies. Reports and GUI rows retain the original text.

Whitespace normalization trims the ends and replaces each internal whitespace sequence with one space. Case normalization currently uses the C locale's byte-oriented lowercase operation, which is predictable for ASCII source and configuration files.

## 3. Align lines with LCS

For normal document sizes, the engine calculates the **longest common subsequence**. Imagine these inputs:

```text
alpha       alpha
beta        BETA
gamma       gamma
            delta
```

The common lines become anchors. Runs between anchors are paired as changed rows; remaining left lines are removals and remaining right lines are additions.

The implementation stores only two score rows plus one byte of direction information per matrix cell. A fixed cell limit prevents a pasted machine-generated file from requesting unbounded memory. Beyond that limit, a predictable line-by-line fallback is used.

## 4. Render without owning the algorithm

`src/main.c` calls the engine and renders each `DiffRow`:

- turquoise/green: added
- coral: removed
- amber: changed
- muted: unchanged

Filtering and scrolling only decide which rows to draw. They never mutate the result, so exporting a report still includes the full comparison.

## 5. Native file dialogs

tinyfiledialogs provides the Windows common dialog and selects an installed desktop helper on Linux. Loaded files are capped before the terminating null byte is written. This avoids a classic C buffer overflow and gives the user a clear size message.

## 6. Run the headless tests

```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

The tests cover changed/added alignment, normalization and report generation. They link only `text_orbit_core`; raylib and a display are unnecessary.

## 7. Suggested learner missions

1. Add Unicode-aware case folding through a small UTF-8 library.
2. Replace the bounded fallback with the Myers algorithm for huge documents.
3. Highlight changed words inside a changed line.
4. Persist theme and comparison options in the user's configuration directory.
5. Add a directory-comparison layer while keeping this engine unchanged.
