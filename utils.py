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
