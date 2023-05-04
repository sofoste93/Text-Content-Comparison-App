from tkinter import filedialog, messagebox


def browse_file(entry):
    file_path = filedialog.askopenfilename()
    entry.delete(0, 'end')
    entry.insert(0, file_path)


def save_report(report_content):
    save_path = filedialog.asksaveasfilename(defaultextension=".txt")

    try:
        with open(save_path, "w", encoding='utf-8') as report_file:
            report_file.write(report_content)
        messagebox.showinfo("Success", "Report saved successfully.")

    except Exception as e:
        messagebox.showerror("Error", str(e))


def _show_help():
    help_text = """1. Browse or paste the text of the files you want to compare.
2. (Optional) Select comparison options: Ignore Whitespace or Ignore Case.
3. Click the "Compare Files" button to start the comparison.
4. View the comparison results in the rightmost text box.
5. (Optional) Click the "Save Report" button to save the comparison results to a file.
6. (Optional) Use the "Clear" button to clear the text boxes and start a new comparison."""

    messagebox.showinfo("How to use", help_text)