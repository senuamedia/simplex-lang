#!/usr/bin/env python3
"""
Simple LLVM IR merger for Simplex bootstrap compiler
Merges multiple .ll files while deduplicating declarations and renaming string constants.
"""

import sys
import re
import platform
from collections import defaultdict

def get_target_triple():
    """Get the LLVM target triple for the current platform."""
    system = platform.system().lower()
    machine = platform.machine().lower()

    if machine in ('x86_64', 'amd64'):
        arch = 'x86_64'
    elif machine in ('arm64', 'aarch64'):
        arch = 'aarch64'
    else:
        arch = 'x86_64'

    if system == 'darwin':
        version = platform.mac_ver()[0]
        if version:
            major = version.split('.')[0]
            return f'{arch}-apple-macosx{major}.0.0'
        return f'{arch}-apple-macosx14.0.0'
    elif system == 'windows':
        return f'{arch}-pc-windows-msvc'
    else:
        return f'{arch}-unknown-linux-gnu'

def merge_ll_files(input_files, output_file):
    declarations = set()  # Unique declarations
    all_functions = []  # (name, body, source_file) - preserve order
    functions_seen = set()  # Track which functions we've added

    # Per-file data
    file_strings = {}  # filename -> [(old_label, content)]
    file_functions = {}  # filename -> [(name, body)]

    for filename in input_files:
        with open(filename, 'r') as f:
            content = f.read()

        file_strings[filename] = []
        file_functions[filename] = []

        lines = content.split('\n')
        i = 0
        n = len(lines)

        while i < n:
            line = lines[i]

            # Skip module headers and target lines
            if line.startswith('; ModuleID') or line.startswith('target '):
                i += 1
                continue

            # Collect string constants with their original labels
            # Format: @.str.modulename.N = ... or @.str.N = ...
            if line.startswith('@.str'):
                match = re.match(r'(@\.str\.[a-zA-Z_0-9.]+)\s*=\s*(.*)', line)
                if match:
                    old_label = match.group(1)
                    rest = match.group(2)
                    file_strings[filename].append((old_label, rest))
                i += 1
                continue

            # Collect declarations
            if line.startswith('declare '):
                declarations.add(line.strip())
                i += 1
                continue

            # Collect function definitions
            if line.startswith('define '):
                # Extract function name
                match = re.search(r'@"?([^"(]+)"?\(', line)
                if match:
                    name = match.group(1)
                else:
                    name = f"unknown_{i}"

                # Collect function body
                body_lines = [line]
                i += 1
                brace_count = line.count('{') - line.count('}')

                while i < n and brace_count > 0:
                    body_lines.append(lines[i])
                    brace_count += lines[i].count('{') - lines[i].count('}')
                    i += 1

                body = '\n'.join(body_lines)
                file_functions[filename].append((name, body))

                # Only add to all_functions if not already seen
                if name not in functions_seen:
                    functions_seen.add(name)
                    all_functions.append((name, body, filename))
                continue

            i += 1

    # Create global string label mapping
    # Each file's strings get new unique IDs
    label_map = {}  # (filename, old_label) -> new_label
    new_strings = []  # (new_label, content)
    string_id = 0

    for filename in input_files:
        for old_label, content in file_strings[filename]:
            new_label = f"@.str.{string_id}"
            label_map[(filename, old_label)] = new_label
            new_strings.append((new_label, content))
            string_id += 1

    # Replace old string labels in functions with new ones
    final_functions = []
    for name, body, source_file in all_functions:
        # Get the string mappings for this file
        file_mappings = [(old, label_map[(source_file, old)])
                         for old, _ in file_strings[source_file]]

        # Sort by length descending to avoid partial matches
        # e.g., @.str.10 should be replaced before @.str.1
        file_mappings.sort(key=lambda x: len(x[0]), reverse=True)

        for old_label, new_label in file_mappings:
            body = body.replace(old_label, new_label)

        final_functions.append((name, body))

    # Write output
    with open(output_file, 'w') as f:
        f.write('; ModuleID = "simplex_program"\n')
        f.write(f'target triple = "{get_target_triple()}"\n')

        # Write declarations
        for decl in sorted(declarations):
            f.write(decl + '\n')
        f.write('\n')

        # Write functions
        for name, body in final_functions:
            f.write(body + '\n\n')

        # Write string constants
        if new_strings:
            f.write('\n; String constants\n')
            for label, content in new_strings:
                f.write(f'{label} = {content}\n')

    print(f"  Declarations: {len(declarations)}")
    print(f"  Functions: {len(final_functions)}")
    print(f"  String constants: {len(new_strings)}")

def main():
    if len(sys.argv) < 3:
        print("Usage: merge_ll.py output.ll input1.ll input2.ll ...")
        sys.exit(1)

    output_file = sys.argv[1]
    input_files = sys.argv[2:]

    print(f"Merging {len(input_files)} files into {output_file}")
    merge_ll_files(input_files, output_file)
    print(f"Done!")

if __name__ == '__main__':
    main()
