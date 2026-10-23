import argparse
import os

DEFAULT_IGNORE_DIRS = {
    ".git",
    ".svn",
    ".vscode",
    ".idea",
    "build",
    "bin",
    "obj",
    "cmake-build-debug",
    "cmake-build-release",
    "x64",
    "x86",
    "Debug",
    "Release",
}

EXTENSIONS = (".cpp", ".h", ".hpp", ".cxx", ".cc", ".hxx")


def build_tree_lines(current_dir, ignore_dirs, extensions, prefix=""):
    """Recursively generates an ASCII tree structure of matching files and relevant directories."""
    lines = []
    try:
        entries = sorted(os.listdir(current_dir))
    except PermissionError:
        return [f"{prefix}[Permission Denied]"]

    # Filter directories and matching source files
    valid_dirs = [
        e
        for e in entries
        if os.path.isdir(os.path.join(current_dir, e)) and e not in ignore_dirs
    ]
    matching_files = [
        e
        for e in entries
        if os.path.isfile(os.path.join(current_dir, e))
        and e.lower().endswith(extensions)
    ]

    items = [
        (d, True) for d in valid_dirs
    ] + [(f, False) for f in matching_files]

    for index, (name, is_dir) in enumerate(items):
        is_last = index == len(items) - 1
        connector = "└── " if is_last else "├── "
        extension_prefix = "    " if is_last else "│   "

        if is_dir:
            sub_path = os.path.join(current_dir, name)
            sub_lines = build_tree_lines(
                sub_path, ignore_dirs, extensions, prefix + extension_prefix
            )
            # Only display the directory if it contains relevant files
            if sub_lines:
                lines.append(f"{prefix}{connector}{name}/")
                lines.extend(sub_lines)
        else:
            lines.append(f"{prefix}{connector}{name}")

    return lines


def combine_source_files(source_dir, output_file):
    source_path = os.path.abspath(source_dir)

    if not os.path.isdir(source_path):
        print(f"Error: Folder '{source_path}' does not exist.")
        return

    # 1. Collect target files
    matched_files = []
    for root, dirs, files in os.walk(source_path):
        dirs[:] = [d for d in dirs if d not in DEFAULT_IGNORE_DIRS]
        for f in sorted(files):
            if f.lower().endswith(EXTENSIONS):
                matched_files.append(os.path.join(root, f))

    # 2. Generate tree representation
    root_name = os.path.basename(source_path) or source_path
    tree_lines = [f"{root_name}/"] + build_tree_lines(
        source_path, DEFAULT_IGNORE_DIRS, EXTENSIONS
    )

    # 3. Write output file
    with open(output_file, "w", encoding="utf-8", errors="replace") as outfile:
        # Write File Structure Header
        outfile.write("=" * 80 + "\n")
        outfile.write("PROJECT DIRECTORY STRUCTURE\n")
        outfile.write("=" * 80 + "\n\n")
        outfile.write("\n".join(tree_lines))
        outfile.write(f"\n\nTotal Files Found: {len(matched_files)}\n\n")

        # Write File Contents
        outfile.write("=" * 80 + "\n")
        outfile.write("SOURCE CODE CONTENTS\n")
        outfile.write("=" * 80 + "\n")

        for filepath in matched_files:
            relpath = os.path.relpath(filepath, source_path)
            outfile.write(f"\n{'=' * 80}\n")
            outfile.write(f"FILE: {relpath}\n")
            outfile.write(f"{'=' * 80}\n\n")

            try:
                with open(
                    filepath, "r", encoding="utf-8", errors="replace"
                ) as infile:
                    outfile.write(infile.read())
                    outfile.write("\n")
            except Exception as err:
                outfile.write(f"[Error reading file: {err}]\n")

    print(
        f"Done! Scanned and wrote {len(matched_files)} files with directory tree to '{os.path.abspath(output_file)}'."
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Recursively gather C/C++ source code with folder tree structure into one file."
    )
    parser.add_argument(
        "folder",
        nargs="?",
        default=None,
        help="Path to the folder you want to scan",
    )
    parser.add_argument(
        "-o",
        "--output",
        default="combined_cpp_code.txt",
        help="Output text filename (default: combined_cpp_code.txt)",
    )

    args = parser.parse_args()

    target_folder = args.folder
    if not target_folder:
        target_folder = (
            input("Enter the path to the folder you want to scan: ")
            .strip()
            .strip('"\'')
        )

    combine_source_files(target_folder, args.output)