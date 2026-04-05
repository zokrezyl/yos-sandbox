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
┌─────────────────────────────────────────────────────┐
│                    Host (Linux)                      │
├─────────────────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  │
│  │  Thread 1   │  │  Thread 2   │  │  Thread N   │  │
│  │  (PID 1)    │  │  (PID 2)    │  │  (PID N)    │  │
│  ├─────────────┤  ├─────────────┤  ├─────────────┤  │
│  │ WasmProcess │  │ WasmProcess │  │ WasmProcess │  │
│  │  - wasm3 rt │  │  - wasm3 rt │  │  - wasm3 rt │  │
│  │  - memory   │  │  - memory   │  │  - memory   │  │
│  ├─────────────┤  ├─────────────┤  ├─────────────┤  │
│  │ProcessContext│ │ProcessContext│ │ProcessContext│ │
│  │  - VFS*     │  │  - VFS*     │  │  - VFS*     │  │
│  │  - Process* │  │  - Process* │  │  - Process* │  │
│  └─────────────┘  └─────────────┘  └─────────────┘  │
├─────────────────────────────────────────────────────┤
│                   ProcessTable                       │
│            (shared, mutex-protected)                 │
├─────────────────────────────────────────────────────┤
│                     Runtime                          │
│         (orchestrates everything)                    │
└─────────────────────────────────────────────────────┘
```

## Key Components

### Runtime (`runtime.cpp`)
- Loads WASM bytecode
- Creates/manages processes
- Implements fork via memory snapshot
- Entry point: `run(wasmPath, argc, argv)`

### ProcessTable (`process.cpp`)
- Tracks all processes (pid, state, parent, exit code)
- Thread-safe with mutex
- Handles wait/exit synchronization

### VFS (`vfs.cpp`)
- Per-process virtual filesystem
- Mounts (currently just HostFS at "/")
- File descriptor table
- cwd tracking

### ProcessContext
- Per-process state passed to wasm3 as user data
- Contains: Runtime*, Process*, VFS*, IM3Runtime
- Retrieved in syscall handlers via `m3_GetUserData()`

### Syscall Layer

#### Code Generation (`src/codegen/`)
- `syscalls.yaml` - Defines all syscalls
- `gen-syscalls.py` - Generates glue code

#### Handler Types
| Type | Description |
|------|-------------|
| `passthrough` | Direct call to native libc |
| `vfs` | Call through ctx->vfs->method() |
| `runtime` | Call through ctx->runtime->method() |
| `stub` | Return fixed value |
| `wasm_impl` | C code compiled into WASM |
| `extern` | Handler in syscalls.cpp |

#### Generated Files
- `yos-handlers-generated.hpp` - Native m3Api handlers
- `yos-link-generated.hpp` - wasm3 link table
- `yos-generated.c` - WASM-side stubs
- `yos-generated.h` - WASM-side header

## WASM Side

### Compilation
- BusyBox compiled with wasi-sdk clang
- `-nostdlib` - No wasi-libc
- Links only against generated YOS stubs

### Memory Layout
```
0x00000 ┌──────────────┐
        │    Stack     │ (grows down from __stack_pointer)
0x10000 ├──────────────┤ __stack_pointer init
        │    Data      │ (initialized data)
        │    BSS       │ (zero-initialized)
        ├──────────────┤ __heap_base
        │    Heap      │ (grows up via malloc)
        ├──────────────┤
        │   (unused)   │
0x1FFFF └──────────────┘ (2 pages = 128KB)
```

## Fork Implementation

1. Parent calls `yos_fork()`
2. Runtime snapshots parent's WASM memory
3. Creates new Process in ProcessTable
4. Spawns new OS thread
5. Child thread creates new WasmProcess with memory snapshot
6. Parent returns child PID, child returns 0

## Pointer Handling

**Critical**: WASM uses 32-bit addresses (offsets into linear memory). Host uses 64-bit pointers.

- `m3ApiGetArgMem(type, name)` - Converts WASM offset to host pointer
- Passthrough returns `ptr` - Must convert host pointer back to WASM offset
- Some functions (getenv, strerror) return host memory - Cannot convert, must stub or copy

## Variadic Functions (Generic Solution)

### Requirement

Guest WASM code calls libc variadic functions (printf, sprintf, execl, etc.) that must be dynamically linked to YOS-generated wrappers, which call the SAME native libc function (printf calls printf, execl calls execl - NOT v* variants).

### How WASM Compiles Varargs

WASM compiler transforms variadic function calls into a TWO-PARAMETER signature:

```c
// Source: printf("Hello %s, num %d", name, 42)

// WASM compilation result:
// 1. Pack varargs onto stack in linear memory
// 2. Call printf(fmt_ptr, va_ptr)

// Memory layout at va_ptr:
// offset 0: pointer to "name" string (i32)
// offset 4: value 42 (i32)
```

The function signature becomes `(i32 fmt_ptr, i32 va_ptr) -> i32` where `va_ptr` points to packed arguments in WASM linear memory.

### Generic Solution

WASM linear memory IS host memory. The packed arguments at `mem + va_ptr` can be used directly.

**Assembly trampoline** loads args from memory into registers/stack and calls the native function:

```asm
; x86_64 calling convention:
; Integer args: RDI, RSI, RDX, RCX, R8, R9, then stack
; Float args: XMM0-XMM7

call_native_varargs:
    ; Input: RDI=func_ptr, RSI=args_array, RDX=arg_count, RCX=type_mask
    ; Load args from array into appropriate registers
    ; Push overflow args to stack
    ; Call function
    ; Return result
```

### Generated Wrapper Pattern

For ANY variadic function, the generator produces:

```c
m3ApiRawFunction(libc_{name}) {
    m3ApiReturnType({return_type});

    // Get fixed params from extracted signature
    m3ApiGetArg{Mem}({type}, {param_name});  // for each param

    // Get va_ptr (WASM adds this for varargs)
    m3ApiGetArg(uint32_t, _va_ptr);

    // Get WASM memory - args are packed at mem + va_ptr
    uint32_t _mem_size;
    uint8_t* _mem = m3_GetMemory(runtime, &_mem_size, 0);

    // Call native {name} with args from WASM memory
    // (implementation via assembly trampoline or platform-specific mechanism)
    {return_type} _result = call_native_varargs({name}, _mem + _va_ptr, ...);
    m3ApiReturn(_result);
}
```

### Key Points

1. **Same function** - printf calls printf, execl calls execl, NOT v* variants
2. **Generic** - ONE pattern for ALL variadic functions
3. **Signature-driven** - Generated from extracted function signatures
4. **Args in memory** - WASM packs args, we read and forward them
5. **Platform trampoline** - Assembly forwards args to native calling convention

## Thread Safety

- ProcessTable: Protected by mutex
- VFS: Per-process, no sharing
- wasm3 runtime: Per-process, no sharing
- Runtime: Shared, but methods that modify state are called from single thread or synchronized

## Hook Architecture

The code generator supports a **hook mechanism** for injecting platform-specific runtime code without affecting the generic libc wrapper generation.

### Overview

```
┌─────────────────┐     ┌──────────────────┐     ┌─────────────────┐
│  libc.yaml      │     │  hooks.yaml      │     │  yos-runtime.c  │
│  (3000+ funcs)  │────▶│  (per-platform)  │────▶│  (hook impls)   │
└─────────────────┘     └──────────────────┘     └─────────────────┘
         │                       │
         ▼                       ▼
┌─────────────────────────────────────────────────────────────────┐
│                    generate-libc-calls.py                        │
│  - Reads libc.yaml for signatures                               │
│  - Reads hooks.yaml to know which funcs are hooked              │
│  - Generates: libc_<func> calls yos_<func> OR native libc       │
└─────────────────────────────────────────────────────────────────┘
         │
         ▼
┌─────────────────────────────────────────────────────────────────┐
│                    libc-wrappers.c (generated)                   │
│                                                                  │
│  m3ApiRawFunction(libc_fork) {                                  │
│      return yos_fork(ctx);  // HOOKED - calls yos-runtime.c     │
│  }                                                               │
│                                                                  │
│  m3ApiRawFunction(libc_strlen) {                                │
│      return strlen(s);      // PASSTHROUGH - calls libc         │
│  }                                                               │
└─────────────────────────────────────────────────────────────────┘
```

### hooks.yaml (per-platform)

```yaml
# Platform: linux-host (development)
# Functions that need YOS runtime instead of direct libc passthrough

process:
  - fork       # needs process table
  - vfork
  - execve
  - _exit
  - exit
  - getpid
  - getppid
  - wait
  - waitpid
  - wait3
  - wait4

filesystem:
  - open       # needs VFS layer
  - openat
  - close
  - read
  - write
  - lseek
  - stat
  - fstat
  - lstat
  - fstatat
  - access
  - faccessat
  - dup
  - dup2
  - dup3
  - pipe
  - pipe2

memory:
  - sbrk       # needs heap tracking
  - brk
  - mmap       # stub or emulate
  - munmap

# Everything NOT listed: passthrough to libc
```

### Generated Code Patterns

**HOOKED function** (calls yos-runtime.c):

```c
m3ApiRawFunction(libc_fork) {
    m3ApiReturnType(int32_t)
    m3ApiGetUserData(yos_ctx_t*, ctx)
    m3ApiReturn(yos_fork(ctx));
}

m3ApiRawFunction(libc_open) {
    m3ApiReturnType(int32_t)
    m3ApiGetUserData(yos_ctx_t*, ctx)
    m3ApiGetArgMem(const char*, path)
    m3ApiGetArg(int32_t, flags)
    m3ApiGetArg(int32_t, mode)
    m3ApiReturn(yos_open(ctx, path, flags, mode));
}
```

**PASSTHROUGH function** (calls libc directly):

```c
m3ApiRawFunction(libc_strlen) {
    m3ApiReturnType(uint32_t)
    m3ApiGetArgMem(const char*, s)
    m3ApiReturn(strlen(s));
}

m3ApiRawFunction(libc_memcpy) {
    m3ApiReturnType(uint32_t)
    m3ApiGetArgMem(void*, dest)
    m3ApiGetArgMem(const void*, src)
    m3ApiGetArg(uint32_t, n)
    memcpy(dest, src, n);
    m3ApiReturn(m3ApiPtrToOffset(dest));
}
```

### yos-runtime.c (Pure C Implementation)

```c
// No C++ overhead - fixed-size structures

#define YOS_MAX_FDS 256
#define YOS_MAX_PROCS 64
#define YOS_MAX_MOUNTS 16

typedef struct {
    int host_fd;        // -1 = unused
    int mount_idx;      // which mount owns this fd
    uint32_t flags;
} yos_fd_t;

typedef struct {
    int pid;
    int ppid;
    int pgid;
    int state;          // RUNNING, STOPPED, ZOMBIE
    int exit_code;
    uint8_t* memory;    // WASM memory snapshot (for fork)
    size_t mem_size;
    pthread_mutex_t lock;
    pthread_cond_t wait_cond;
} yos_proc_t;

typedef struct {
    yos_proc_t* proc;           // current process
    yos_fd_t fds[YOS_MAX_FDS];  // per-process fd table
    char cwd[PATH_MAX];         // current working directory
    uint32_t heap_end;          // for sbrk
    IM3Runtime wasm_rt;         // wasm3 runtime handle
} yos_ctx_t;

// Global process table
static yos_proc_t g_procs[YOS_MAX_PROCS];
static pthread_mutex_t g_proc_lock = PTHREAD_MUTEX_INITIALIZER;

// Hook implementations
int yos_fork(yos_ctx_t* ctx);
int yos_open(yos_ctx_t* ctx, const char* path, int flags, int mode);
int yos_read(yos_ctx_t* ctx, int fd, void* buf, size_t count);
int yos_write(yos_ctx_t* ctx, int fd, const void* buf, size_t count);
// ... etc
```

### Platform-Specific Hooks

Different platforms may need different hooks:

| Platform | Hooked Functions | Notes |
|----------|-----------------|-------|
| linux-host | fork, exec, VFS ops | Full emulation for dev/test |
| ios | ALL process + most FS | No fork/exec allowed |
| wasm-browser | ALL | Everything emulated |
| embedded | Subset | Minimal footprint |

### Benefits

1. **Generator stays generic** - Just reads YAML files, no platform logic
2. **Clean separation** - Signatures in libc.yaml, behavior in hooks.yaml
3. **Pure C runtime** - No C++ overhead for hooks
4. **Platform flexibility** - Different hooks.yaml per target
5. **Gradual migration** - Start with all passthrough, hook as needed

## Limitations

1. No real signals (stub handlers)
2. No shared memory between processes
3. No pipes/sockets (yet)
4. FILE* stdio doesn't work (host concept)
5. Memory limited to WASM linear memory size
