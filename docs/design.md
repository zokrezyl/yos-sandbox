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

## Variadic Functions

Printf-family functions use varargs bridge:
1. WASM wrapper packs args into `VarArgPack` struct
2. Calls `__yos_varargs_call(func_id, arg1, arg2, arg3, &pack)`
3. Native handler unpacks and calls real printf

## Thread Safety

- ProcessTable: Protected by mutex
- VFS: Per-process, no sharing
- wasm3 runtime: Per-process, no sharing
- Runtime: Shared, but methods that modify state are called from single thread or synchronized

## Limitations

1. No real signals (stub handlers)
2. No shared memory between processes
3. No pipes/sockets (yet)
4. FILE* stdio doesn't work (host concept)
5. Memory limited to WASM linear memory size
