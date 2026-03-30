# YOS Sandbox - Project State

## Objective

Build a multi-process environment emulator for platforms where `fork()` is unavailable (iOS, WebAssembly). The architecture:

- **wasm3** (pure C interpreter) = the CPU, runs WASM bytecode, works on iOS
- **YOS host runtime** (C++) = the kernel, implements fork/exec/wait by creating new wasm3 instances on host threads with copied linear memory
- **wasi-libc** (musl-based) = standard C library compiled into each .wasm binary, provides printf/malloc/string ops as WASM code
- **WASI syscalls** = the ~45 function imports that wasi-libc calls for I/O, filesystem, clock, etc. - provided by wasm3's built-in WASI or our stubs
- **YOS syscalls** = our custom imports (yos_fork, yos_exec, yos_wait, etc.) for process management

The goal: compile real programs (busybox) to WASM and run them in our emulated multi-process environment.

## Source Tree

```
yos-sandbox/
├── CMakeLists.txt              # Main build - yos runtime + wasm3 (via CPM) + wasi-sdk download
├── Makefile                    # Wrapper: `make build`, `make busybox`
├── CLAUDE.md                   # Project instructions
│
├── src/                        # YOS host runtime (C++)
│   ├── main.cpp                # Entry point - loads .wasm and runs it
│   ├── process.hpp/cpp         # Process table (pid, parent, state, exit code, threading)
│   ├── runtime.hpp/cpp         # WASM runtime wrapper (createProcess, forkProcess, spawnProcess, runProcess)
│   └── syscalls.hpp/cpp        # YOS + WASI syscall implementations linked into wasm3
│
├── wasm-stubs/                 # Headers + stubs for compiling C to wasm32-wasi
│   ├── wasm-compat.h           # POSIX compatibility: declares fork/exec/wait/signals/ioctl/termios/sockets/etc
│   ├── yos-stubs.h             # YOS wasm import declarations (__attribute__((import_module("env"))))
│   ├── yos-stubs.c             # Real implementations: fork→yos_fork, exec→yos_exec, plus signal/user/terminal stubs
│   ├── sys/                    # Stub headers (sys/wait.h, sys/socket.h, sys/ioctl.h, etc)
│   ├── netdb.h, termios.h, ... # Stub headers that #include "wasm-compat.h"
│   └── netinet/, arpa/, net/   # More stub headers
│
├── wasm-programs/              # C programs compiled to WASM
│   └── shell.c                 # Minimal YOS shell (uses yos_spawn for fork+exec)
│
├── cmake/
│   ├── CPM.cmake               # CMake package manager
│   ├── build-busybox.cmake     # Script to cross-compile busybox to wasm32-wasip1
│   └── wasm-cc.sh.in           # CC wrapper template for busybox build (adds flags, filters unsupported linker args)
│
├── busybox.config              # Busybox kconfig (unused - config is inline in build-busybox.cmake)
│
├── test/                       # WAT (WebAssembly Text) test programs
│   ├── fork-test.wat           # Tests fork: parent forks child, child detects via yos_fork_result
│   ├── exec-test.wat           # Tests fork+exec: parent forks, child execs hello.wasm
│   └── hello.wat               # Simple program: prints pid/ppid, exits with code 7
│
├── sandbox/                    # Standalone exploration examples
│   ├── browser-wasm-example/   # Same C++ compiled to WASM for browser via emscripten
│   └── desktop-wasm-example/   # Same C++ compiled to wasm32-wasi, run with standalone wasm3 runner
│
├── docs/
│   └── state.md                # This file
│
├── build/                      # Build output (gitignored)
│   ├── yos                     # Host runtime binary
│   ├── wasm/                   # All .wasm outputs (shell.wasm, busybox.wasm, test .wasm files)
│   └── _deps/                  # CPM downloads: wasm3-src, busybox-src, wasi-sdk
│
└── tmp/                        # Research repos (gitignored): wasm3, wasmer clones
```

## Build

```bash
make build       # Build YOS runtime + test wasm files + shell.wasm
make busybox     # Build busybox.wasm (cross-compile busybox to wasm32-wasip1)
make clean       # Delete build/
make reconfigure # Full clean rebuild
```

Busybox build uses `cmake -P cmake/build-busybox.cmake` which:
1. Copies busybox source to `build/wasm/bb-build/`
2. Generates `wasm-cc` wrapper from `cmake/wasm-cc.sh.in` (clang → wasm32-wasip1 with all flags)
3. Runs `make allnoconfig`, enables ~65 applets (ash shell, coreutils, grep, sed, awk, find, ps, kill, etc)
4. Patches trylink script to disable `--gc-sections`
5. Removes x86 assembly files
6. Compiles yos-stubs.o
7. Builds busybox

## What Works

### YOS Runtime
- Process table with PID tracking, parent-child relationships
- `yos_fork`: snapshots WASM linear memory, creates new wasm3 instance on host thread
- `yos_exec`: traps out of interpreter, runProcess loop loads new .wasm and re-enters _start
- `yos_spawn`: combined fork+exec (avoids re-entry problems with WASI libc reinit)
- `yos_wait`: parent blocks on child exit via condition variable
- `yos_getpid`, `yos_getppid`, `yos_exit`, `yos_read`, `yos_write`
- WASI syscalls via wasm3's built-in m3_LinkWASI (filesystem, stdio, clock, etc)
- Missing WASI stubs (fd_advise, sock_*, etc) return ENOSYS

### Test Programs (WAT)
All pass:
```bash
./build/yos build/wasm/fork-test.wasm   # fork + wait
./build/yos build/wasm/exec-test.wasm   # fork + exec hello.wasm
```

### Shell
Works - spawns child processes running other .wasm programs:
```bash
echo -e "pid\nhello.wasm\nhello.wasm\nexit" | ./build/yos build/wasm/shell.wasm
```

### Busybox Compilation
Busybox 1.36.1 compiles to wasm32-wasip1 successfully:
- 73KB .wasm binary
- 127 functions, 50 imports (5 YOS + 45 WASI)
- Ash shell + ~50 applets enabled

## Current Problem: Busybox Runtime Crash

**Symptom:** `./build/yos build/wasm/busybox.wasm` → `[trap] unreachable executed`

**Root Cause:** wasm-ld Identical Code Folding (ICF) merges functions with identical bodies.

### The Problem

In `yos-stubs.c`:
```c
pid_t fork(void)  { return yos_fork(); }
pid_t vfork(void) { return yos_fork(); }  // IDENTICAL BODY
```

Both functions compile to the same WASM bytecode (call yos_fork; return). The linker merges them into ONE function and arbitrarily picks one name. Result:
- Export "fork" → points to function named "vfork"
- Export "getpid" → points to function named "setsid"
- Export "__wasm_call_ctors" → points to "__stdio_exit"

The function BODIES are correct (they call yos_fork/yos_getpid), but the export NAME mappings are wrong. This corrupts the symbol table.

### Validation Tests

```bash
./tests/validate-busybox.sh      # Check final binary for corruption
./tests/validate-yos-stubs.sh    # Check yos-stubs.o before linking
./tests/trace-symbol-corruption.sh  # Trace corruption through build
./tests/run-busybox.sh           # Runtime test
```

### What Was Tried (ALL FAILED)

1. **`--allow-multiple-definition`** → causes even worse corruption, mixes unrelated functions
2. **Removing `-lwasi-emulated-signal`** → linker errors, wasi-libc headers require it
3. **Link order (yos-stubs.o first)** → ICF still merges after linking
4. **`__attribute__((export_name("fork")))`** → ignored by wasm-ld during ICF
5. **`__attribute__((used, noinline, visibility("default")))`** → doesn't prevent ICF
6. **`-Wl,--no-icf`** → wasm-ld error: "unknown argument"
7. **`-Wl,--icf=none`** → wasm-ld error: "unknown argument"

### The Actual Problem

**wasm-ld does NOT support disabling ICF.** Unlike ELF lld which has `--icf=none`, the WebAssembly port of lld has no such option. ICF is always enabled and cannot be turned off.

### Potential Solutions (NOT YET TRIED)

1. **Make function bodies unique** - Add dummy operations to prevent identical bytecode:
   ```c
   pid_t fork(void)  { volatile int x = 1; (void)x; return yos_fork(); }
   pid_t vfork(void) { volatile int x = 2; (void)x; return yos_fork(); }
   ```

2. **Use inline assembly** - Insert unique nops or instructions per function

3. **Compiler flag `-fno-merge-functions`** - May prevent LLVM from merging before linking (needs testing)

4. **Post-process the .wasm** - Use wasm-tools or binaryen to fix export table after linking

5. **Patch wasm-ld** - Add `--icf=none` support (upstream contribution)

6. **Use different linker** - Check if emscripten's linker or other WASM linkers have this option

### Current State

- yos-stubs.o: Functions are correctly defined as separate (func[6]=fork, func[7]=vfork, etc.)
- Final busybox.wasm: Exports corrupted (fork→vfork, getpid→setsid, __wasm_call_ctors→__stdio_exit)
- Runtime: Crashes with "unreachable executed" because initialization calls wrong function

## Architecture Insights

### Why wasi-libc is baked into .wasm
Every libc function (printf, malloc, memcpy, etc) operates on WASM linear memory. The pointers are offsets into a byte array. Native libc can't work with these directly. So libc is compiled TO WASM and runs interpreted alongside the application code.

The only "bridge" to the host is the ~45 WASI syscall imports (fd_write, path_open, etc) — analogous to Linux syscalls. The host runtime provides these.

### Emscripten vs wasi-sdk
Same WASM bytecode. Different syscall targets:
- Emscripten: libc → JS glue → browser APIs
- wasi-sdk: libc → WASI imports → any WASI runtime

### WASIX
WASIX extends WASI with process management (fork, exec, threads, signals). We don't use WASIX — we provide our own process management via YOS syscalls. WASIX's value would be a libc sysroot that already declares fork/exec, but we achieve the same with `wasm-compat.h` + `yos-stubs.c`.
