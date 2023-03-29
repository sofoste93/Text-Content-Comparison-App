# comparison.py

from difflib import ndiff


def compare_files(file1_path, file2_path, ignore_whitespace=False, ignore_case=False):
    try:
        with open(file1_path, 'r', encoding='utf-8') as file1, open(file2_path, 'r', encoding='utf-8') as file2:
            file1_content = file1.readlines()
            file2_content = file2.readlines()

            if ignore_whitespace:
                file1_content = [line.strip() for line in file1_content]
                file2_content = [line.strip() for line in file2_content]

            if ignore_case:
                file1_content = [line.lower() for line in file1_content]
                file2_content = [line.lower() for line in file2_content]

            comparison = ndiff(file1_content, file2_content)

        return comparison

    except Exception as e:
        raise e
