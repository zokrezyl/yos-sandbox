// YOS Types - Shared type definitions
//
// ============================================================================
// ARCHITECTURE OVERVIEW
// ============================================================================
//
// Three main structures with DIFFERENT lifetimes and purposes:
//
// +-----------------+     +------------------+     +-------------------+
// | yos_runtime_t   |     | yos_proc_t       |     | yos_exec_ctx_t    |
// | (GLOBAL)        |     | (PROCESS ID)     |     | (EXECUTION STATE) |
// +-----------------+     +------------------+     +-------------------+
// | One per app     |     | One per process  |     | One per running   |
// | Lives forever   |     | in table         |     | process           |
// |                 |     |                  |     |                   |
// | - procs[] ------+---->| - pid            |<----+-- proc            |
// | - wasm_bytes    |     | - ppid           |     | - wasm_runtime    |
// | - argc/argv     |     | - state          |     | - wasm_memory     |
// |                 |     | - exit_code      |     | - fds[]           |
// |                 |     | - thread         |     | - cwd             |
// +-----------------+     +------------------+     +-------------------+
//
// ============================================================================
// RELATIONSHIP: yos_proc_t vs yos_exec_ctx_t
// ============================================================================
//
// yos_proc_t = WHO the process is (identity)
//   - Lives in global process table (runtime->procs[])
//   - Contains: pid, ppid, pgid, sid, state, exit_code
//   - Survives after process exits (ZOMBIE state) until reaped by wait()
//   - Used by: getpid, getppid, waitpid, kill
//
// yos_exec_ctx_t = WHAT the process is doing (execution)
//   - Created when process starts running
//   - Destroyed when process exits
//   - Contains: wasm runtime, memory, fd table, cwd, heap
//   - Points to its yos_proc_t via ctx->proc
//   - Used by: all syscalls that need execution state
//
// ANALOGY:
//   yos_proc_t = Driver's license (your identity, persists)
//   yos_exec_ctx_t = Your car (what you're driving, temporary)
//
// ============================================================================
// FORK OPERATION
// ============================================================================
//
//   PARENT (pid=1)                    CHILD (pid=2)
//   +----------------+                +----------------+
//   | yos_exec_ctx_t |   fork()       | yos_exec_ctx_t |
//   |   proc --------+---+            |   proc --------+--+
//   |   wasm_memory  |   |   COPY     |   wasm_memory  |  |
//   |   fds[]        |   |  =====>    |   fds[]        |  |
//   |   cwd          |   |            |   cwd          |  |
//   +----------------+   |            +----------------+  |
//                        v                               v
//   runtime->procs[0]    |            runtime->procs[1]  |
//   +----------------+   |            +----------------+ |
//   | yos_proc_t     |<--+            | yos_proc_t     |<+
//   |   pid=1        |                |   pid=2        |
//   |   ppid=0       |                |   ppid=1       |
//   |   state=RUN    |                |   state=RUN    |
//   +----------------+                +----------------+
//
// Fork creates:
//   1. New yos_proc_t in process table (child identity)
//   2. New yos_exec_ctx_t for child (child execution state)
//   3. New wasm3 runtime for child
//   4. COPY of parent's wasm_memory
//   5. New pthread running the child
//
// ============================================================================
//

#ifndef YOS_TYPES_H
#define YOS_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <pthread.h>
#include <limits.h>
#include <linux/limits.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Constants
// ============================================================================

#define YOS_MAX_FDS      256
#define YOS_MAX_PROCS    64
#define YOS_MAX_DIRS     64

// ============================================================================
// yos_proc_t - Process Identity (slot in process table)
//
// This is the UNIX process identity: pid, parent, state, exit code.
// Does NOT contain execution state (memory, fds, etc).
// Lives in yos_runtime_t.procs[] array.
// Multiple threads may read/write - protected by lock.
// ============================================================================

typedef enum {
    YOS_PROC_FREE = 0,   // Slot available
    YOS_PROC_READY,      // Created, not yet running
    YOS_PROC_RUNNING,    // Actively executing
    YOS_PROC_WAITING,    // Blocked (e.g., on I/O)
    YOS_PROC_ZOMBIE,     // Exited, waiting for parent to reap
} yos_proc_state_t;

typedef struct yos_proc {
    // Process identity
    int32_t pid;
    int32_t ppid;         // Parent PID
    int32_t pgid;         // Process group ID
    int32_t sid;          // Session ID
    yos_proc_state_t state;
    int32_t exit_code;    // Set when state becomes ZOMBIE

    // Synchronization (for waitpid)
    pthread_mutex_t lock;
    pthread_cond_t wait_cond;
    int exited;           // Flag: has this process exited?

    // vfork support (parent blocks until child exec/exit)
    int32_t vfork_parent_pid;
    pthread_cond_t vfork_cond;
    int vfork_child_done;

    // Host thread running this process
    pthread_t thread;
} yos_proc_t;

// ============================================================================
// yos_fd_t - File Descriptor Entry
// ============================================================================

typedef struct {
    int host_fd;        // Host OS fd, -1 = slot unused
    uint32_t flags;     // O_RDONLY, O_WRONLY, etc.
    char path[PATH_MAX];// Path for debugging/proc
} yos_fd_t;

// ============================================================================
// yos_dir_t - Directory Handle
// ============================================================================

typedef struct {
    void* host_dir;     // DIR* from opendir()
} yos_dir_t;

// ============================================================================
// yos_exec_ctx_t - Per-Process Execution Context
//
// This is the EXECUTION STATE for a running process.
// Created when process starts, destroyed when process exits.
//
// INVARIANT: ctx->proc is NEVER NULL (abort if violated)
// INVARIANT: ctx->rt is NEVER NULL (abort if violated)
//
// Each forked child gets its OWN yos_exec_ctx_t with:
//   - Its own wasm3 runtime (wasm_runtime)
//   - Its own linear memory (wasm_memory) - COPIED from parent on fork
//   - Its own fd table (fds[]) - COPIED from parent on fork
//   - Its own cwd, heap pointer, etc.
//
// Passed to wasm3 as userdata, retrieved in syscall handlers via:
//   yos_exec_ctx_t* ctx = (yos_exec_ctx_t*)m3_GetUserData(runtime);
// ============================================================================

struct yos_runtime;  // Forward declaration

typedef struct yos_exec_ctx {
    // === MANDATORY LINKS (never NULL) ===
    struct yos_runtime* rt;         // Global runtime (shared by all processes)
    yos_proc_t* proc;               // This process's identity in rt->procs[]

    // === PER-PROCESS STATE (each fork gets independent copy) ===

    // File descriptors - inherited on fork, replaced on exec
    yos_fd_t fds[YOS_MAX_FDS];
    yos_dir_t dirs[YOS_MAX_DIRS];

    // Filesystem state
    char cwd[PATH_MAX];             // Current working directory
    uint32_t umask;                 // File creation mask

    // Memory management
    uint32_t heap_end;              // Program break for sbrk()

    // === WASM RUNTIME (each process has its own instance) ===

    void* wasm_env;                 // IM3Environment - wasm3 environment
    void* wasm_runtime;             // IM3Runtime - wasm3 runtime
    void* wasm_module;              // IM3Module - loaded module
    void* wasm_memory;              // uint8_t* - linear memory base
    size_t wasm_mem_size;           // Linear memory size

    // === EXEC STATE ===

    char exec_path[PATH_MAX];       // Path to exec (set by execve)
    int exec_pending;               // Flag: execve was called, reload wasm

    // === FORK STATE ===

    int is_child;                   // True if this ctx was created by fork
                                    // (child returns 0 from fork syscall)
} yos_exec_ctx_t;

// ============================================================================
// yos_runtime_t - Global Runtime State
//
// ONE instance for the entire application.
// Contains the process table and shared resources.
// All yos_ctx_t instances point back here via ctx->rt.
// ============================================================================

typedef struct yos_runtime {
    // Process table - array of process identity slots
    yos_proc_t procs[YOS_MAX_PROCS];
    pthread_mutex_t proc_lock;      // Protects procs[] and next_pid
    int32_t next_pid;               // Next PID to allocate

    // WASM binary - shared by all processes (read-only after load)
    uint8_t* wasm_bytes;
    size_t wasm_bytes_size;

    // Command line arguments (for main and after exec)
    int argc;
    char** argv;

    // Base path for resolving relative WASM paths in exec
    char base_path[PATH_MAX];
} yos_runtime_t;

// ============================================================================
// WASM32 Structures - Types used by WASM modules
// ============================================================================

// WASM32 stat structure - matches wasm-compat.h layout (72 bytes)
typedef struct {
    uint32_t wasm_st_dev;        // offset 0
    uint32_t wasm_st_ino;        // offset 4
    uint32_t wasm_st_mode;       // offset 8
    uint32_t wasm_st_nlink;      // offset 12
    uint32_t wasm_st_uid;        // offset 16
    uint32_t wasm_st_gid;        // offset 20
    uint32_t wasm_st_rdev;       // offset 24
    uint32_t _pad0;              // offset 28 - padding for 8-byte alignment
    int64_t  wasm_st_size;       // offset 32
    uint32_t wasm_st_blksize;    // offset 40
    uint32_t wasm_st_blocks;     // offset 44
    int32_t  wasm_st_atim_sec;   // offset 48
    int32_t  wasm_st_atim_nsec;  // offset 52
    int32_t  wasm_st_mtim_sec;   // offset 56
    int32_t  wasm_st_mtim_nsec;  // offset 60
    int32_t  wasm_st_ctim_sec;   // offset 64
    int32_t  wasm_st_ctim_nsec;  // offset 68
} wasm_stat_t;

// WASM32 dirent structure (268 bytes)
#pragma pack(push, 1)
typedef struct {
    uint32_t d_ino;         // offset 0
    uint32_t d_off;         // offset 4
    uint16_t d_reclen;      // offset 8
    uint8_t  d_type;        // offset 10
    char     d_name[256];   // offset 11
    uint8_t  _pad;          // offset 267, total 268
} wasm_dirent_t;
#pragma pack(pop)

#ifdef __cplusplus
}
#endif

#endif // YOS_TYPES_H
