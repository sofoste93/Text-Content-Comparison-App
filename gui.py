import tkinter as tk
from tkinter import messagebox

import pyperclip

from utils import browse_file, save_report, _show_help
from Comparison import compare_files, compare_files_from_text


class FileComparisonGUI(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("Sofostech File Comparison Tool")
        self.geometry("1280x800")

        self._build_gui()

    def _build_gui(self):
        self._create_menu_bar()
        self._create_file_frame()
        self._create_comparison_options_frame()
        self._create_comparison_text_widgets()
        self._create_button_frame()

    def _create_menu_bar(self):
        menu_bar = tk.Menu(self)

        help_menu = tk.Menu(menu_bar, tearoff=0)
        help_menu.add_command(label="How to use", command=_show_help)
        menu_bar.add_cascade(label="Help", menu=help_menu)

        self.config(menu=menu_bar)

    def _create_file_frame(self):
        file_frame = tk.Frame(self)
        file_frame.pack(padx=10, pady=10)

        file1_label = tk.Label(file_frame, text="Select file 1 or paste text:")
        file1_label.grid(row=0, column=0, padx=5, pady=5)
        self.file1_entry = tk.Entry(file_frame, width=60)
        self.file1_entry.grid(row=0, column=1, padx=5, pady=5)
        file1_button = tk.Button(file_frame, text="Browse", command=lambda: browse_file(self.file1_entry))
        file1_button.grid(row=0, column=2, padx=5, pady=5)
        paste1_button = tk.Button(file_frame, text="Paste", command=lambda: self._paste_text(self.file1_text))
        paste1_button.grid(row=0, column=3, padx=5, pady=5)

        file2_label = tk.Label(file_frame, text="Select file 2 or paste text:")
        file2_label.grid(row=1, column=0, padx=5, pady=5)
        self.file2_entry = tk.Entry(file_frame, width=60)
        self.file2_entry.grid(row=1, column=1, padx=5, pady=5)
        file2_button = tk.Button(file_frame, text="Browse", command=lambda: browse_file(self.file2_entry))
        file2_button.grid(row=1, column=2, padx=5, pady=5)
        paste2_button = tk.Button(file_frame, text="Paste", command=lambda: self._paste_text(self.file2_text))
        paste2_button.grid(row=1, column=3, padx=5, pady=5)

    def _create_comparison_options_frame(self):
        options_frame = tk.Frame(self)
        options_frame.pack(padx=10, pady=10)

        self.ignore_whitespace_var = tk.BooleanVar()
        self.ignore_whitespace_checkbox = tk.Checkbutton(options_frame, text="Ignore Whitespace",
                                                         variable=self.ignore_whitespace_var)
        self.ignore_whitespace_checkbox.pack(side="left", padx=5)

        self.ignore_case_var = tk.BooleanVar()
        self.ignore_case_checkbox = tk.Checkbutton(options_frame, text="Ignore Case", variable=self.ignore_case_var)
        self.ignore_case_checkbox.pack(side="left", padx=5)

    def _create_comparison_text_widgets(self):
        text_frame = tk.Frame(self)
        text_frame.pack(expand=True, fill=tk.BOTH, padx=10, pady=10)

        self.file1_text = tk.Text(text_frame, wrap=tk.WORD)
        self.file1_text.pack(side="left", expand=True, fill=tk.BOTH)
        self.file2_text = tk.Text(text_frame, wrap=tk.WORD)
        self.file2_text.pack(side="left", expand=True, fill=tk.BOTH)

        scrollbar = tk.Scrollbar(text_frame, command=self._sync_scroll)
        scrollbar.pack(side="right", fill="y")

        self.result_text = tk.Text(text_frame, wrap=tk.WORD, yscrollcommand=scrollbar.set)
        self.result_text.pack(side="left", expand=True, fill=tk.BOTH)

        self.file1_text.config(yscrollcommand=scrollbar.set)
        self.file2_text.config(yscrollcommand=scrollbar.set)

    def _sync_scroll(self, *args):
        self.file1_text.yview_moveto(args[0])
        self.file2_text.yview_moveto(args[0])
        self.result_text.yview_moveto(args[0])

    def _create_button_frame(self):
        button_frame = tk.Frame(self)
        button_frame.pack(padx=10, pady=10)

        compare_button = tk.Button(button_frame, text="Compare Files", command=self._compare_files)
        compare_button.pack(side="left", padx=5)

        save_button = tk.Button(button_frame, text="Save Report", command=self._save_report)
        save_button.pack(side="left", padx=5)

        clear_button = tk.Button(button_frame, text="Clear", command=self._clear_all)
        clear_button.pack(side="left", padx=5)

        cancel_button = tk.Button(button_frame, text="Cancel", command=self.destroy)
        cancel_button.pack(side="left", padx=5)

        toggle_dark_mode_button = tk.Button(button_frame, text="Toggle Dark Mode", command=self._toggle_dark_mode)
        toggle_dark_mode_button.pack(side="left", padx=5)

    def _clear_all(self):
        self.file1_entry.delete(0, tk.END)
        self.file2_entry.delete(0, tk.END)
        self.file1_text.delete(1.0, tk.END)
        self.file2_text.delete(1.0, tk.END)
        self.result_text.delete(1.0, tk.END)

    def _toggle_dark_mode(self):
        dark_mode = {
            "bg": "#2b2b2b",
            "fg": "#ffffff",
            "insertbackground": "#ffffff",
            "selectbackground": "#4a4a4a",
            "selectforeground": "#ffffff",
        }

        light_mode = {
            "bg": "#ffffff",
            "fg": "#000000",
            "insertbackground": "#000000",
            "selectbackground": "#c0c0c0",
            "selectforeground": "#000000",
        }

        current_bg = self["bg"]

        if current_bg == light_mode["bg"]:
            new_mode = dark_mode
        else:
            new_mode = light_mode

        widgets_without_fg = [self, self.file1_entry, self.file2_entry, self.file1_text, self.file2_text,
                              self.result_text]

        for widget in widgets_without_fg:
            if widget != self:
                widget.config(bg=new_mode["bg"], fg=new_mode["fg"], insertbackground=new_mode["insertbackground"],
                              selectbackground=new_mode["selectbackground"],
                              selectforeground=new_mode["selectforeground"])
            else:
                widget.config(bg=new_mode["bg"])

    def _insert_text_with_line_numbers(self, text_widget, text):
        lines = text.split("\n")
        for i, line in enumerate(lines, start=1):
            text_widget.insert(tk.END, f"{i}- ")
            text_widget.insert(tk.END, f"{line}\n")

    def _compare_files(self):
        file1_path = self.file1_entry.get()
        file2_path = self.file2_entry.get()

        ignore_whitespace = self.ignore_whitespace_var.get()
        ignore_case = self.ignore_case_var.get()

        try:
            # Check if there is text in the Text widgets
            file1_content = self.file1_text.get(1.0, tk.END).strip()
            file2_content = self.file2_text.get(1.0, tk.END).strip()

            # If the Text widgets are empty, read the contents from the files
            if not file1_content:
                if file1_path:
                    with open(file1_path, 'r', encoding='utf-8') as file1:
                        file1_content = file1.read()
                else:
                    messagebox.showerror("Error", "No input provided for text 1. Please enter text or select a file to compare.")
                    return

            if not file2_content:
                if file2_path:
                    with open(file2_path, 'r', encoding='utf-8') as file2:
                        file2_content = file2.read()
                else:
                    messagebox.showerror("Error", "No input provided for text 2. Please enter text or select a file to compare.")
                    return

            comparison = compare_files_from_text(file1_content, file2_content, ignore_whitespace, ignore_case)

            # Display texts with line numbers
            self.file1_text.delete(1.0, tk.END)
            self._insert_text_with_line_numbers(self.file1_text, file1_content)

            self.file2_text.delete(1.0, tk.END)
            self._insert_text_with_line_numbers(self.file2_text, file2_content)

            self.result_text.delete(1.0, tk.END)
            self.result_text.tag_configure("added", foreground="green")
            self.result_text.tag_configure("removed", foreground="red")
            self.result_text.tag_configure("context", foreground="gray")
            self.result_text.tag_configure("lineno", foreground="blue")

            no_difference = True
            for line in comparison:
                if line.startswith('+') or line.startswith('-'):
                    no_difference = False

                if line.startswith('+'):
                    tag = "added"
                elif line.startswith('-'):
                    tag = "removed"
                elif line.startswith('@'):
                    tag = "context"
                else:
                    tag = "lineno"
                self.result_text.insert(tk.END, line + '\n', tag)

            if no_difference:
                self.result_text.tag_configure("no_difference", foreground="green", font=("Arial", 16, "bold"))
                self.result_text.insert(tk.END, "No differences found!", "no_difference")

        except FileNotFoundError:
            messagebox.showerror("Error", "File not found. Please make sure you've entered the correct file path.")
        except Exception as e:
            messagebox.showerror("Error", str(e))

    def _save_report(self):
        report_content = self.result_text.get(1.0, tk.END)
        save_report(report_content)

    def _paste_text(self, text_widget):
        clipboard_text = pyperclip.paste()
        text_widget.delete(1.0, tk.END)
        text_widget.insert(tk.END, clipboard_text)
