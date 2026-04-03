#!/usr/bin/env python3
"""
Extract ALL function signatures from C headers using libclang.
Generates libc.yaml with function definitions.
Output: build/generated/libc.yaml
"""

import clang.cindex
from clang.cindex import CursorKind, TypeKind
import sys
import os
import tempfile

# Set libclang library path
clang.cindex.Config.set_library_file('/usr/lib/llvm-19/lib/libclang-19.so.19')

# Map C types to our YAML types
TYPE_MAP = {
    'int': 'i32',
    'unsigned int': 'u32',
    'long': 'i32',  # WASM32
    'unsigned long': 'u32',  # WASM32
    'long long': 'i64',
    'unsigned long long': 'u64',
    'size_t': 'u32',  # WASM32
    'ssize_t': 'i32',  # WASM32
    'off_t': 'i64',
    'off64_t': 'i64',
    'pid_t': 'i32',
    'uid_t': 'u32',
    'gid_t': 'u32',
    'mode_t': 'u32',
    'dev_t': 'u32',  # WASM32
    'ino_t': 'u32',  # WASM32
    'nlink_t': 'u32',
    'blksize_t': 'u32',
    'blkcnt_t': 'u32',
    'time_t': 'i32',  # WASM32
    'clock_t': 'i32',
    'socklen_t': 'u32',
    'nfds_t': 'u32',
    'void': 'void',
    'char *': 'str',
    'const char *': 'str',
    'void *': 'ptr',
    'const void *': 'ptr',
    'DIR *': 'handle',  # opaque handle
    'FILE *': 'handle',  # opaque handle
    'struct DIR *': 'handle',
    'struct FILE *': 'handle',
    'struct stat *': 'ptr',
    'struct stat64 *': 'ptr',
    'struct dirent *': 'ptr',
    'struct dirent64 *': 'ptr',
    'struct tm *': 'ptr',
    'const struct tm *': 'ptr',
    'struct timespec *': 'ptr',
    'const struct timespec *': 'ptr',
    'struct timeval *': 'ptr',
    'struct timezone *': 'ptr',
    'struct sockaddr *': 'ptr',
    'const struct sockaddr *': 'ptr',
    'struct iovec *': 'ptr',
    'const struct iovec *': 'ptr',
    'struct msghdr *': 'ptr',
    'const struct msghdr *': 'ptr',
    'struct pollfd *': 'ptr',
    'fd_set *': 'ptr',
    'sigset_t *': 'ptr',
    'const sigset_t *': 'ptr',
}

def get_yaml_type(clang_type):
    """Convert clang type to YAML type"""
    type_str = clang_type.spelling

    # Direct match
    if type_str in TYPE_MAP:
        return TYPE_MAP[type_str]

    # Handle restrict qualifier
    type_str_clean = type_str.replace('restrict ', '').replace('__restrict ', '').strip()
    if type_str_clean in TYPE_MAP:
        return TYPE_MAP[type_str_clean]

    # Pointer types
    if clang_type.kind == TypeKind.POINTER:
        pointee = clang_type.get_pointee()
        pointee_str = pointee.spelling

        # Check for char pointer (string)
        if 'char' in pointee_str and 'unsigned' not in pointee_str:
            return 'str'
        # Check for void pointer
        if pointee_str == 'void' or pointee_str == 'const void':
            return 'ptr'
        # Check for FILE/DIR
        if 'FILE' in pointee_str or 'DIR' in pointee_str:
            return 'handle'
        # Default pointer
        return 'ptr'

    # Function pointer
    if clang_type.kind == TypeKind.FUNCTIONPROTO or '(*)' in type_str:
        return 'ptr'

    # Array decays to pointer
    if clang_type.kind == TypeKind.INCOMPLETEARRAY or clang_type.kind == TypeKind.CONSTANTARRAY:
        return 'ptr'

    # Default to i32 for unknown types
    return 'i32'

def extract_all_functions(headers):
    """Extract ALL function signatures from headers"""
    # Create a temporary C file that includes all headers
    includes = '\n'.join([f'#include <{h}>' for h in headers])
    tmp_content = f"""
#define _GNU_SOURCE
#define _FILE_OFFSET_BITS 64
{includes}
"""

    with tempfile.NamedTemporaryFile(suffix='.c', mode='w', delete=False) as f:
        f.write(tmp_content)
        tmp_path = f.name

    try:
        index = clang.cindex.Index.create()
        args = ['-xc', '-std=c11']
        tu = index.parse(tmp_path, args=args)

        # Check for parse errors
        errors = 0
        for diag in tu.diagnostics:
            if diag.severity >= clang.cindex.Diagnostic.Error:
                errors += 1
        if errors > 0:
            print(f"  Warning: {errors} parse errors", file=sys.stderr)

        functions = {}

        def visit(cursor):
            if cursor.kind == CursorKind.FUNCTION_DECL:
                name = cursor.spelling
                # Skip internal/private functions
                if name.startswith('_') and not name.startswith('__'):
                    pass  # Keep single underscore for now
                if name.startswith('__') and not name.startswith('__builtin'):
                    pass  # Keep double underscore libc functions

                # Skip if already seen (first declaration wins)
                if name in functions:
                    return

                params = []
                for arg in cursor.get_arguments():
                    param_name = arg.spelling or f'arg{len(params)}'
                    param_type = get_yaml_type(arg.type)
                    params.append((param_name, param_type))

                ret_type = get_yaml_type(cursor.result_type)
                loc = cursor.location

                # Get header file
                header = ""
                if loc.file:
                    fpath = str(loc.file)
                    if '/usr/include/' in fpath:
                        header = fpath.split('/usr/include/')[-1]
                    else:
                        header = os.path.basename(fpath)

                # Check if variadic
                is_variadic = cursor.type.is_function_variadic()

                functions[name] = {
                    'name': name,
                    'params': params,
                    'returns': ret_type,
                    'header': header,
                    'line': loc.line if loc.file else 0,
                    'variadic': is_variadic
                }

            for child in cursor.get_children():
                visit(child)

        visit(tu.cursor)
        return functions
    finally:
        os.unlink(tmp_path)

def format_yaml(functions, header_filter=None):
    """Format functions as YAML, optionally filtered by header"""
    lines = []

    # Group by header
    by_header = {}
    for f in functions.values():
        h = f['header'] or 'unknown'
        if header_filter and h != header_filter:
            continue
        if h not in by_header:
            by_header[h] = []
        by_header[h].append(f)

    for header in sorted(by_header.keys()):
        funcs = by_header[header]
        lines.append(f"# From <{header}>")
        for f in sorted(funcs, key=lambda x: x['name']):
            lines.append(f"  - name: {f['name']}")
            if f['params']:
                params_str = ', '.join([f"{{{p[0]}: {p[1]}}}" for p in f['params']])
                lines.append(f"    params: [{params_str}]")
            else:
                lines.append(f"    params: []")
            lines.append(f"    returns: {f['returns']}")
            if f.get('variadic'):
                lines.append(f"    variadic: true")
            lines.append("")

    return '\n'.join(lines)

if __name__ == '__main__':
    output_dir = 'build/generated'
    os.makedirs(output_dir, exist_ok=True)
    output_file = os.path.join(output_dir, 'libc.yaml')

    # All standard C/POSIX headers
    headers = [
        # C standard library
        'stdio.h', 'stdlib.h', 'string.h', 'strings.h', 'ctype.h', 'wctype.h',
        'wchar.h', 'locale.h', 'math.h', 'complex.h', 'fenv.h', 'errno.h',
        'assert.h', 'stdarg.h', 'stddef.h', 'stdint.h', 'inttypes.h',
        'stdbool.h', 'limits.h', 'float.h', 'iso646.h', 'setjmp.h',
        'signal.h', 'time.h',

        # POSIX
        'unistd.h', 'fcntl.h', 'sys/types.h', 'sys/stat.h', 'sys/wait.h',
        'sys/time.h', 'sys/times.h', 'sys/resource.h', 'sys/utsname.h',
        'sys/mman.h', 'sys/ioctl.h', 'sys/socket.h', 'sys/select.h',
        'sys/un.h', 'sys/uio.h', 'sys/file.h', 'sys/statvfs.h',
        'poll.h', 'dirent.h', 'termios.h', 'pwd.h', 'grp.h',
        'netinet/in.h', 'arpa/inet.h', 'netdb.h',
        'dlfcn.h', 'fnmatch.h', 'glob.h', 'wordexp.h',
        'regex.h', 'sched.h', 'semaphore.h', 'pthread.h',
        'spawn.h', 'syslog.h', 'utime.h', 'utmp.h',

        # Linux-specific
        'sys/prctl.h', 'sys/epoll.h', 'sys/eventfd.h', 'sys/signalfd.h',
        'sys/timerfd.h', 'sys/inotify.h', 'sys/sendfile.h',
        'sys/syscall.h', 'linux/limits.h',
    ]

    print(f"Extracting functions from {len(headers)} headers...")
    functions = extract_all_functions(headers)
    print(f"Found {len(functions)} unique functions")

    output = ["# Auto-generated libc function signatures from Linux headers",
              "# Generated by tools/extract-signatures.py",
              "# DO NOT EDIT - regenerate with: uv run python tools/extract-signatures.py",
              "",
              f"# Total functions: {len(functions)}",
              "",
              "# Note: Types are mapped to WASM32 equivalents",
              "# - size_t, ssize_t, long, time_t -> 32-bit on WASM32",
              "# - DIR*, FILE* -> handle (opaque, not memory pointer)",
              ""]

    output.append(format_yaml(functions))

    with open(output_file, 'w') as f:
        f.write('\n'.join(output))

    print(f"Generated: {output_file}")
