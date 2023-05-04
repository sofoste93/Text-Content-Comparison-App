import tkinter as tk
from tkinter import messagebox
from utils import browse_file, save_report, _show_help
from Comparison import compare_files


class FileComparisonGUI(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("Sofostech File Comparison Tool")
        self.geometry("800x600")

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

        file1_label = tk.Label(file_frame, text="Select file 1:")
        file1_label.grid(row=0, column=0, padx=5, pady=5)
        self.file1_entry = tk.Entry(file_frame, width=60)
        self.file1_entry.grid(row=0, column=1, padx=5, pady=5)
        file1_button = tk.Button(file_frame, text="Browse", command=lambda: browse_file(self.file1_entry))
        file1_button.grid(row=0, column=2, padx=5, pady=5)

        file2_label = tk.Label(file_frame, text="Select file 2:")
        file2_label.grid(row=1, column=0, padx=5, pady=5)
        self.file2_entry = tk.Entry(file_frame, width=60)
        self.file2_entry.grid(row=1, column=1, padx=5, pady=5)
        file2_button = tk.Button(file_frame, text="Browse", command=lambda: browse_file(self.file2_entry))
        file2_button.grid(row=1, column=2, padx=5, pady=5)

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

        cancel_button = tk.Button(button_frame, text="Cancel", command=self.destroy)
        cancel_button.pack(side="left", padx=5)

        toggle_dark_mode_button = tk.Button(button_frame, text="Toggle Dark Mode", command=self._toggle_dark_mode)
        toggle_dark_mode_button.pack(side="left", padx=5)

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

    def _compare_files(self):
        file1_path = self.file1_entry.get()
        file2_path = self.file2_entry.get()

        ignore_whitespace = self.ignore_whitespace_var.get()
        ignore_case = self.ignore_case_var.get()

        try:
            comparison = compare_files(file1_path, file2_path, ignore_whitespace, ignore_case)

            self.file1_text.delete(1.0, tk.END)
            self.file2_text.delete(1.0, tk.END)
            self.result_text.delete(1.0, tk.END)

            with open(file1_path, 'r', encoding='utf-8') as file1, open(file2_path, 'r', encoding='utf-8') as file2:
                file1_content = file1.read()
                file2_content = file2.read()

            self.file1_text.insert(tk.END, file1_content)
            self.file2_text.insert(tk.END, file2_content)

            for line in comparison:
                self.result_text.insert(tk.END, line)

        except Exception as e:
            messagebox.showerror("Error", str(e))

    def _save_report(self):
        report_content = self.result_text.get(1.0, tk.END)
        save_report(report_content)
