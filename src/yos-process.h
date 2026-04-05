// YOS Process Management
#ifndef YOS_PROCESS_H
#define YOS_PROCESS_H

#include "yos-types.h"
#include <sys/resource.h>

#ifdef __cplusplus
extern "C" {
#endif

// Process table operations
yos_proc_t* yos_proc_alloc(yos_runtime_t* rt, int32_t ppid);
yos_proc_t* yos_proc_find(yos_runtime_t* rt, int32_t pid);
void yos_proc_exit(yos_proc_t* proc, int32_t exit_code);

// Syscall implementations
int32_t yos_fork(yos_exec_ctx_t* ctx);
int32_t yos_vfork(yos_exec_ctx_t* ctx);
int yos_execve(yos_exec_ctx_t* ctx, const char* path, char* const argv[], char* const envp[]);
void yos__exit(yos_exec_ctx_t* ctx, int status);
void yos_exit(yos_exec_ctx_t* ctx, int status);
int32_t yos_getpid(yos_exec_ctx_t* ctx);
int32_t yos_getppid(yos_exec_ctx_t* ctx);
int32_t yos_getpgrp(yos_exec_ctx_t* ctx);
int yos_setpgid(yos_exec_ctx_t* ctx, int32_t pid, int32_t pgid);
int32_t yos_setsid(yos_exec_ctx_t* ctx);
int32_t yos_getsid(yos_exec_ctx_t* ctx, int32_t pid);
int32_t yos_wait(yos_exec_ctx_t* ctx, int* status);
int32_t yos_waitpid(yos_exec_ctx_t* ctx, int32_t pid, int* status, int options);
int32_t yos_wait3(yos_exec_ctx_t* ctx, int* status, int options, struct rusage* rusage);
int32_t yos_wait4(yos_exec_ctx_t* ctx, int32_t pid, int* status, int options, struct rusage* rusage);
int yos_kill(yos_exec_ctx_t* ctx, int32_t pid, int sig);
int yos_raise(yos_exec_ctx_t* ctx, int sig);

#ifdef __cplusplus
}
#endif

#endif // YOS_PROCESS_H
