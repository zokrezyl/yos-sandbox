// YOS Runtime - Pure C Implementation
// Function declarations for syscall handlers
//
// Types are defined in yos-types.h
// VFS functions are in yos-vfs.h
// Process functions are in yos-process.h

#ifndef YOS_RUNTIME_H
#define YOS_RUNTIME_H

#include "yos-types.h"
#include "yos-log.h"
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Memory Hooks
// ============================================================================

void* yos_sbrk(yos_exec_ctx_t* ctx, intptr_t increment);
int yos_brk(yos_exec_ctx_t* ctx, void* addr);
void* yos_mmap(yos_exec_ctx_t* ctx, void* addr, size_t length, int prot, int flags, int fd, off_t offset);
int yos_munmap(yos_exec_ctx_t* ctx, void* addr, size_t length);
int yos_mprotect(yos_exec_ctx_t* ctx, void* addr, size_t len, int prot);
void* yos_mremap(yos_exec_ctx_t* ctx, void* old_addr, size_t old_size, size_t new_size, int flags, void* new_addr);

// ============================================================================
// Signal Hooks (minimal implementation for sandbox)
// ============================================================================

typedef void (*yos_sighandler_t)(int);

yos_sighandler_t yos_signal(yos_exec_ctx_t* ctx, int signum, yos_sighandler_t handler);
int yos_sigaction(yos_exec_ctx_t* ctx, int signum, const void* act, void* oldact);
int yos_sigprocmask(yos_exec_ctx_t* ctx, int how, const void* set, void* oldset);
int yos_sigemptyset(yos_exec_ctx_t* ctx, void* set);
int yos_sigfillset(yos_exec_ctx_t* ctx, void* set);
int yos_sigaddset(yos_exec_ctx_t* ctx, void* set, int signum);
int yos_sigdelset(yos_exec_ctx_t* ctx, void* set, int signum);
int yos_sigismember(yos_exec_ctx_t* ctx, const void* set, int signum);
int yos_sigpending(yos_exec_ctx_t* ctx, void* set);
int yos_sigsuspend(yos_exec_ctx_t* ctx, const void* mask);

// ============================================================================
// User/Group Hooks (sandbox returns fixed values)
// ============================================================================

uint32_t yos_getuid(yos_exec_ctx_t* ctx);
uint32_t yos_geteuid(yos_exec_ctx_t* ctx);
uint32_t yos_getgid(yos_exec_ctx_t* ctx);
uint32_t yos_getegid(yos_exec_ctx_t* ctx);
int yos_setuid(yos_exec_ctx_t* ctx, uint32_t uid);
int yos_seteuid(yos_exec_ctx_t* ctx, uint32_t euid);
int yos_setgid(yos_exec_ctx_t* ctx, uint32_t gid);
int yos_setegid(yos_exec_ctx_t* ctx, uint32_t egid);
int yos_setreuid(yos_exec_ctx_t* ctx, uint32_t ruid, uint32_t euid);
int yos_setregid(yos_exec_ctx_t* ctx, uint32_t rgid, uint32_t egid);
int yos_getgroups(yos_exec_ctx_t* ctx, int size, uint32_t* list);
int yos_setgroups(yos_exec_ctx_t* ctx, size_t size, const uint32_t* list);

// ============================================================================
// System Info
// ============================================================================

int yos_uname(yos_exec_ctx_t* ctx, void* buf);

// ============================================================================
// Varargs Support
// ============================================================================

// VarArgPack constants (must match WASM-side definitions)
#define VARG_END    0
#define VARG_INT    1
#define VARG_LONG   2
#define VARG_STR    3
#define VARG_PTR    4
#define VARG_UINT   5
#define VARG_ULONG  6
#define VARG_CHAR   7
#define VARG_MAX    16

// VarArgPack structure (must match WASM-side)
typedef struct {
    unsigned char types[VARG_MAX];
    unsigned long values[VARG_MAX];
} VarArgPack;

// VFUNC IDs for varargs functions
#define VFUNC_PRINTF    1
#define VFUNC_FPRINTF   2
#define VFUNC_SPRINTF   3
#define VFUNC_SNPRINTF  4
#define VFUNC_DPRINTF   5
#define VFUNC_VASPRINTF 6

int yos_varargs_call(yos_exec_ctx_t* ctx, int func_id, uint32_t arg1, uint32_t arg2, uint32_t arg3, void* args);

// ============================================================================
// Initialization (legacy - use yos_runtime_create instead)
// ============================================================================

void yos_init(void);

#ifdef __cplusplus
}
#endif

#endif // YOS_RUNTIME_H
