# YOS Sandbox - Design Document

## Overview

YOS (Your OS Sandbox) emulates a multi-process POSIX environment for WebAssembly programs. It allows running programs like BusyBox compiled to WASM on platforms without `fork()` (iOS, WebAssembly, embedded).

## Goals

1. **Fork Emulation** - `fork()` via memory snapshot + new thread
2. **Process Isolation** - Each process has own memory, VFS, cwd
3. **POSIX Compatibility** - Support common syscalls
4. **No Native fork()** - Works on iOS, WASM, etc.

## Architecture

```
+-----------------------------------------------------+
|                    Host (Linux)                      |
+-----------------------------------------------------+
|  +--------------+  +--------------+  +--------------+
|  |  Thread 1    |  |  Thread 2    |  |  Thread N    |
|  |  (PID 1)     |  |  (PID 2)     |  |  (PID N)     |
|  +--------------+  +--------------+  +--------------+
|  | yos_exec_ctx | | yos_exec_ctx | | yos_exec_ctx  |
|  |  - wasm3 rt  |  |  - wasm3 rt  |  |  - wasm3 rt  |
|  |  - memory    |  |  - memory    |  |  - memory    |
|  |  - fd table  |  |  - fd table  |  |  - fd table  |
|  +--------------+  +--------------+  +--------------+
+-----------------------------------------------------+
|              yos_runtime_t (global)                  |
|         - process table (yos_proc_t[])              |
|         - WASM bytecode                              |
+-----------------------------------------------------+
```

## Three-Tier Architecture

### 1. yos_runtime_t (Global)
- Single instance for entire application
- Holds WASM bytecode (shared, read-only)
- Process table with all yos_proc_t entries
- Thread-safe with mutex protection

### 2. yos_proc_t (Process Identity)
- One per emulated process
- Contains: pid, ppid, pgid, state, exit_code
- Survives across exec() calls
- Shared state for wait/waitpid synchronization

### 3. yos_exec_ctx_t (Execution Context)
- One per running WASM instance
- Contains: wasm3 runtime, memory, fd table, cwd, heap pointer
- Created fresh on exec(), destroyed on exit
- Passed to all yos_* hook functions

## Code Generation System

### Pipeline

```
+-------------------+     +------------------+     +------------------+
| extract-          |     | libc.yaml        |     | hooks.yaml       |
| signatures.py     |---->| (3489 funcs)     |     | (hooked funcs)   |
| (libclang parser) |     | (signatures)     |     | (manual list)    |
+-------------------+     +--------+---------+     +--------+---------+
                                   |                        |
                                   |                        |
+-------------------+              |                        |
| nm -D libc.so.6   |              |                        |
| grep -E " [TiW] " |------------->|                        |
| (symbol extract)  |              |                        |
+-------------------+              v                        v
        |              +-----------------------------------------+
        |              |        generate-libc-calls.py           |
        +------------->|                                         |
glibc-exports.txt      |  1. Load libc.yaml (all signatures)     |
(2700 symbols)         |  2. Load hooks.yaml (hooked funcs)      |
                       |  3. Load glibc-exports.txt (real syms)  |
                       |  4. For each function:                  |
                       |     - HOOKED: generate yos_* call       |
                       |     - PASSTHROUGH: check glibc-exports  |
                       |       - IN exports: generate libc call  |
                       |       - NOT in exports: SKIP            |
                       +-----------------------------------------+
                                           |
                                           v
                       +-----------------------------------------+
                       |        libc-wrappers.c (generated)      |
                       |  - ~1600 m3ApiRawFunction wrappers      |
                       |  - linkLibcFunctions() registration     |
                       +-----------------------------------------+
```

### Why glibc-exports.txt Filtering?

**Problem**: libc.yaml contains 3489 function signatures extracted from headers. But:
1. Many are internal functions (prefixed with `__`)
2. Many are macros that don't exist as real symbols
3. Some are inline functions compiled away
4. Some exist only in certain glibc versions

**Solution**: Extract ACTUAL exported symbols from the system's libc.so.6:

```bash
nm -D --defined-only /lib/x86_64-linux-gnu/libc.so.6 | grep -E " [TiW] " | ...
```

Symbol types:
- `T` = Text (normal function)
- `i` = Indirect/IFUNC (CPU-optimized, e.g., strlen picks SSE/AVX version at runtime)
- `W` = Weak (can be overridden)

**Critical**: Without `i` symbols, basic functions like `strlen`, `strcpy`, `memcpy` are MISSING because glibc implements them as IFUNCs for performance.

**Result**: ~2700 real symbols vs 3489 header declarations.

### Hooked Functions Bypass Export Check

Hooked functions (in hooks.yaml) call `yos_*` implementations, NOT native libc. They don't need to exist in glibc-exports.txt:

```python
is_hooked = func['name'] in HOOKED_FUNCTIONS
if not is_hooked and func['name'] not in glibc_exports:
    skip()  # Passthrough needs real symbol
    continue
# Hooked functions always generate, even if not in exports
```

### Input Files

#### libc.yaml (auto-generated from Linux headers)
```yaml
- name: open
  params: [{__file: str: const char *}, {__oflag: i32: int}]
  returns: i32
  returns_c: int
  variadic: true

- name: strlen
  params: [{__s: str: const char *}]
  returns: u32
  returns_c: size_t
```

#### hooks.yaml (manually maintained)
```yaml
process:
  - fork
  - vfork
  - execve
  - _exit
  - getpid
  - wait
  - waitpid

filesystem:
  - open
  - openat
  - close
  - read
  - write
  - stat
  - lstat
  - fstat

memory:
  - sbrk
  - brk
```

#### glibc-exports.txt (auto-generated from nm)
```
# Generated by: nm -D --defined-only libc.so.6 | grep -E " [TiW] "
# Includes: T (text), i (indirect/ifunc), W (weak)
strlen
strcpy
memcpy
printf
...
```

### Filtering Logic

```python
for func in libc_yaml:
    is_hooked = func['name'] in HOOKED_FUNCTIONS

    # Hooked functions bypass glibc-exports check
    # (they call yos_*, not native libc)
    if not is_hooked and func['name'] not in glibc_exports:
        skip("not in glibc exports")
        continue

    generate_wrapper(func)
```

## Wrapper Types

### 1. PASSTHROUGH (calls native libc)

For functions NOT in hooks.yaml:

```c
m3ApiRawFunction(libc_strlen) {
    m3ApiReturnType(size_t);
    m3ApiGetArgMem(const char *, __s);
    size_t _result = strlen(__s);
    m3ApiReturn(_result);
}
```

### 2. HOOKED (calls yos_* functions)

For functions IN hooks.yaml:

```c
m3ApiRawFunction(libc_open) {
    m3ApiReturnType(int32_t);
    m3ApiGetArgMem(const char *, __file);
    m3ApiGetArg(int, __oflag);
    m3ApiGetArg(int, __mode);
    yos_exec_ctx_t* _yos_ctx = (yos_exec_ctx_t*)m3_GetUserData(runtime);
    int _result = yos_open(_yos_ctx, __file, __oflag, __mode);
    m3ApiReturn(_result);
}
```

### 3. VARARGS PASSTHROUGH (uses assembly trampoline)

For variadic functions NOT in hooks.yaml:

```c
m3ApiRawFunction(libc_printf) {
    m3ApiReturnType(int32_t);
    m3ApiGetArgMem(const char *, __format);
    m3ApiGetArg(uint32_t, _va_ptr);

    uint32_t _wasm_mem_size;
    uint8_t* _wasm_mem = m3_GetMemory(runtime, &_wasm_mem_size, 0);

    // Pack args into array
    uint64_t _args[32];
    int _argc = 0;
    _args[_argc++] = (uint64_t)(uintptr_t)__format;

    // Extract varargs from WASM memory
    uint32_t _va_off = 0;
    for (int _i = 0; _i < 16 && _argc < 32; _i++) {
        uint32_t _v = *(uint32_t*)(_wasm_mem + _va_ptr + _va_off);
        _args[_argc++] = (uint64_t)_v;
        _va_off += 4;
    }

    // Call via assembly trampoline
    int _result = (int)(uintptr_t)call_native_varargs((void*)printf, _args, _argc);
    m3ApiReturn(_result);
}
```

### 4. VARARGS HOOKED (extracts first vararg, calls yos_*)

For variadic functions IN hooks.yaml (e.g., openat):

```c
m3ApiRawFunction(libc_openat) {
    m3ApiReturnType(int32_t);
    m3ApiGetArg(int, __fd);
    m3ApiGetArgMem(const char *, __file);
    m3ApiGetArg(int, __oflag);
    m3ApiGetArg(uint32_t, _va_ptr);

    uint32_t _wasm_mem_size;
    uint8_t* _wasm_mem = m3_GetMemory(runtime, &_wasm_mem_size, 0);

    yos_exec_ctx_t* _yos_ctx = (yos_exec_ctx_t*)m3_GetUserData(runtime);
    int _va_arg0 = (_va_ptr && _wasm_mem) ? *(int*)(_wasm_mem + _va_ptr) : 0;

    // yos_openat has fixed signature: (ctx, fd, path, flags, mode)
    int _result = yos_openat(_yos_ctx, __fd, __file, __oflag, _va_arg0);
    m3ApiReturn(_result);
}
```

## Function Pointer Handling

### Detection

Function pointers are detected by:
1. Raw syntax: `(*)` or `(*` in C type
2. Typedefs: ends with `_function`, `_fn`, `_func`
3. Known types: `__sighandler_t`, `sighandler_t`

```python
is_func_ptr = ('(*)' in c_type or '(*' in c_type or
              c_type.endswith('_function') or c_type.endswith('_fn') or
              c_type.endswith('_func') or c_type in ('__sighandler_t', 'sighandler_t'))
```

**NOT matched** (structs containing function pointers):
- `cookie_io_functions_t` - struct, not function pointer

### Generated Code for Function Pointer Parameters

```c
m3ApiRawFunction(libc_qsort) {
    m3ApiGetArgMem(void *, __base);
    m3ApiGetArg(size_t, __nmemb);
    m3ApiGetArg(size_t, __size);
    m3ApiGetArg(uint32_t, ___compar_idx);  // Function pointer = table index
    (void)___compar_idx; // TODO: implement callback trampoline

    // Currently passes NULL for callbacks - they don't work yet
    qsort(__base, __nmemb, __size, NULL);
    m3ApiSuccess();
}
```

### TODO: Callback Trampolines

To support callbacks (qsort comparator, signal handlers, etc.):
1. WASM function table index comes as uint32_t
2. Create native trampoline that calls m3_Call() with that index
3. Pass trampoline to native function

## Assembly Trampoline (varargs_trampoline.S)

### Purpose
Call any native variadic function with arguments from an array.

### Signature
```c
void* call_native_varargs(void* func, uint64_t* args, int arg_count);
```

### Implementation (x86_64)
```asm
call_native_varargs:
    ; RDI = func pointer
    ; RSI = args array
    ; RDX = arg count

    ; Load up to 6 args into registers (x86_64 calling convention)
    ; RDI, RSI, RDX, RCX, R8, R9

    ; Push remaining args to stack (in reverse order)

    ; Call function
    call *%rax

    ; Return result in RAX
    ret
```

## yos_* Hook Implementations

### Location
- `src/yos-vfs.c` - File system operations
- `src/yos-process.c` - Process management
- `src/yos-runtime.c` - Memory, signals, misc

### Signature Pattern
All yos_* functions take `yos_exec_ctx_t*` as first parameter:

```c
int yos_open(yos_exec_ctx_t* ctx, const char* path, int flags, int mode);
int yos_read(yos_exec_ctx_t* ctx, int fd, void* buf, size_t count);
int yos_fork(yos_exec_ctx_t* ctx);
void yos__exit(yos_exec_ctx_t* ctx, int status);
```

### Why Hooked?
- **VFS ops**: Need fd table translation, path resolution, cwd handling
- **Process ops**: Need process table, fork emulation, wait synchronization
- **Memory ops**: Need heap tracking within WASM memory

## WASM Side (wasm-stubs/)

### yos-stubs.c
Manual implementations of libc functions that run IN WASM:
- `malloc/free` - Bump allocator using `sbrk`
- `memcpy/memset/memmove` - Byte-by-byte loops
- `__stack_chk_fail` - Stub

### wasm-compat.h
Extern declarations for imports:
```c
extern int open(const char* path, int flags, ...);
extern int read(int fd, void* buf, unsigned int count);
extern int write(int fd, const void* buf, unsigned int count);
```

### Compilation
```bash
clang --target=wasm32 -nostdlib -Wl,--allow-undefined \
    -include wasm-compat.h yos-stubs.c -o yos-stubs.o
```

## Current Status

### Working
- Basic libc wrappers (string, memory, math functions)
- VFS passthrough to host filesystem
- Process table with pid allocation
- sbrk heap management
- Variadic functions via trampoline
- Hooked variadic functions (openat, ioctl, etc.)
- BusyBox compilation to WASM

### TODO
- [ ] Fork implementation (memory snapshot)
- [ ] Exec implementation
- [ ] Wait/waitpid synchronization
- [ ] Callback trampolines (qsort, signal, etc.)
- [ ] Pipe/socket emulation
- [ ] /proc filesystem

## Build System

### Makefile Targets
```bash
make build          # Full build
make glibc-exports  # Extract libc symbols
make smoke_tests    # Run wrapper smoke tests
make run-tests      # Run unit tests
```

### Generated Files
```
build/generated/
  glibc-exports.txt    # Symbols from libc.so
  libc.yaml            # Function signatures
  libc-wrappers.c      # m3Api wrappers
  libc-wrappers.h      # Header with linkLibcFunctions()
  smoke-tests/         # Per-function test .c files
```

## Limitations

1. **No callback support** - qsort, signal handlers don't work yet
2. **No real signals** - Stub handlers only
3. **No shared memory** - Each process has isolated memory
4. **No pipes/sockets** - Not implemented
5. **Limited varargs** - Max 32 args, no float/double detection
6. **FILE* doesn't work** - Host stdio concept doesn't map to WASM
