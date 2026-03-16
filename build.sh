#!/bin/bash
# Simplex Language Build Script
# Builds the sxc compiler from source

set -e

echo "=== Simplex Compiler Build ==="
echo ""

# Detect platform
PLATFORM="unknown"
if [[ "$OSTYPE" == "darwin"* ]]; then
    PLATFORM="macos"
elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
    PLATFORM="linux"
elif [[ "$OSTYPE" == "msys" ]] || [[ "$OSTYPE" == "cygwin" ]]; then
    PLATFORM="windows"
fi

echo "Platform: $PLATFORM"
echo ""

# Check dependencies
echo "Checking dependencies..."

command -v clang >/dev/null 2>&1 || { echo "Error: clang is required but not installed."; exit 1; }
command -v python3 >/dev/null 2>&1 || { echo "Error: python3 is required but not installed."; exit 1; }

echo "  clang: $(clang --version | head -1)"
echo "  python3: $(python3 --version)"
echo ""

# Set library flags based on platform
if [[ "$PLATFORM" == "macos" ]]; then
    # macOS with Homebrew
    OPENSSL_PREFIX=$(brew --prefix openssl 2>/dev/null || echo "/usr/local/opt/openssl")
    LIBS="-lm -lssl -lcrypto -L$OPENSSL_PREFIX/lib"
    INCLUDES="-I$OPENSSL_PREFIX/include"
elif [[ "$PLATFORM" == "linux" ]]; then
    LIBS="-lm -lssl -lcrypto -lpthread"
    INCLUDES=""
else
    echo "Warning: Unsupported platform. Build may fail."
    LIBS="-lm"
    INCLUDES=""
fi

# Create build directory
mkdir -p build

# Step 1: Compile Simplex source to LLVM IR using Python bootstrap
echo "Step 1: Compiling Simplex source files..."
cd compiler/bootstrap

python3 stage0.py lexer.sx
echo "  lexer.sx -> lexer.ll"

python3 stage0.py parser.sx
echo "  parser.sx -> parser.ll"

python3 stage0.py error.sx
echo "  error.sx -> error.ll"

python3 stage0.py codegen.sx
echo "  codegen.sx -> codegen.ll"

python3 stage0.py main.sx
echo "  main.sx -> main.ll"

# Step 2: Merge LLVM IR files
echo ""
echo "Step 2: Merging LLVM IR files..."

python3 << 'MERGE_SCRIPT'
import re

def extract_function_defs(content):
    funcs = {}
    pattern = r'define\s+(?:internal\s+)?[^\s]+\s+@"?([^"(\s]+)"?\s*\('
    lines = content.split('\n')
    i = 0
    while i < len(lines):
        line = lines[i]
        m = re.match(pattern, line)
        if m:
            func_name = m.group(1)
            func_lines = [line]
            i += 1
            brace_count = line.count('{') - line.count('}')
            while i < len(lines) and brace_count > 0:
                func_lines.append(lines[i])
                brace_count += lines[i].count('{') - lines[i].count('}')
                i += 1
            if i < len(lines) and lines[i] == '}':
                func_lines.append(lines[i])
                i += 1
            funcs[func_name] = '\n'.join(func_lines)
        else:
            i += 1
    return funcs

def extract_strings(content):
    strings = {}
    for m in re.finditer(r'(@\.str\.[^\s]+)\s*=\s*private.*', content):
        strings[m.group(1)] = m.group(0)
    return strings

def get_base_header(content):
    lines = content.split('\n')
    header_lines = []
    for line in lines:
        if line.startswith('define ') or line.startswith('@.str'):
            continue
        if line.startswith('; ModuleID') or line.startswith('target ') or line.startswith('declare '):
            header_lines.append(line)
    return '\n'.join(header_lines)

with open('lexer.ll') as f: lexer = f.read()
with open('parser.ll') as f: parser = f.read()
with open('error.ll') as f: error = f.read()
with open('codegen.ll') as f: codegen = f.read()
with open('main.ll') as f: main = f.read()

header = get_base_header(lexer)
all_strings = {}
all_funcs = {}

for content in [lexer, parser, error, codegen, main]:
    all_strings.update(extract_strings(content))
    for fn, body in extract_function_defs(content).items():
        if fn not in all_funcs:
            all_funcs[fn] = body

combined = header + '\n\n'
for str_name, str_def in sorted(all_strings.items()):
    combined += str_def + '\n'
combined += '\n'
for fn_name, fn_body in sorted(all_funcs.items()):
    combined += fn_body + '\n\n'

with open('sxc_combined.ll', 'w') as f:
    f.write(combined)

print(f"  Created sxc_combined.ll ({len(all_funcs)} functions)")
MERGE_SCRIPT

# Clean up intermediate files
rm -f lexer.ll parser.ll error.ll codegen.ll main.ll

cd ../..

# Step 3: Link the compiler
echo ""
echo "Step 3: Linking sxc compiler..."

# Build with warnings as errors for production
clang -O2 -Wall -Wextra -Wformat-security -Werror \
    $INCLUDES \
    compiler/bootstrap/sxc_combined.ll \
    runtime/standalone_runtime.c \
    -o build/sxc \
    $LIBS 2>&1

echo "  Created build/sxc"

# Step 4: Verify
echo ""
echo "Step 4: Verifying build..."

if [[ -f "build/sxc" ]]; then
    chmod +x build/sxc
    echo "  Build successful!"
    echo ""
    echo "Compiler location: $(pwd)/build/sxc"
    echo ""
    echo "To use:"
    echo "  ./build/sxc <source.sx>     # Compile to LLVM IR"
    echo "  clang -O2 <source.ll> runtime/standalone_runtime.c -o <output> $LIBS"
else
    echo "  Build failed!"
    exit 1
fi
