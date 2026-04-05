// YOS Process Management
//
// Implements fork, exec, wait, exit and related syscalls.
// See yos-types.h for architecture overview.

#define _GNU_SOURCE
#include "yos-process.h"
#include "yos-log.h"
#include "yos-exec-ctx.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

// ============================================================================
// Process Table Operations
// ============================================================================

yos_proc_t* yos_proc_alloc(yos_runtime_t* rt, int32_t ppid) {
    pthread_mutex_lock(&rt->proc_lock);

    for (int i = 0; i < YOS_MAX_PROCS; i++) {
        if (rt->procs[i].state == YOS_PROC_FREE) {
            yos_proc_t* p = &rt->procs[i];
            memset(p, 0, sizeof(*p));
            p->pid = rt->next_pid++;
            p->ppid = ppid;
            p->pgid = p->pid;
            p->sid = p->pid;
            p->state = YOS_PROC_READY;
            p->vfork_parent_pid = -1;
            pthread_mutex_init(&p->lock, NULL);
            pthread_cond_init(&p->wait_cond, NULL);
            pthread_cond_init(&p->vfork_cond, NULL);

            pthread_mutex_unlock(&rt->proc_lock);
            YOS_DEBUG("allocated pid=%d ppid=%d", p->pid, ppid);
            return p;
        }
    }

    pthread_mutex_unlock(&rt->proc_lock);
    YOS_ERROR("process table full");
    return NULL;
}

yos_proc_t* yos_proc_find(yos_runtime_t* rt, int32_t pid) {
    pthread_mutex_lock(&rt->proc_lock);

    for (int i = 0; i < YOS_MAX_PROCS; i++) {
        if (rt->procs[i].pid == pid && rt->procs[i].state != YOS_PROC_FREE) {
            pthread_mutex_unlock(&rt->proc_lock);
            return &rt->procs[i];
        }
    }

    pthread_mutex_unlock(&rt->proc_lock);
    return NULL;
}

void yos_proc_exit(yos_proc_t* proc, int32_t exit_code) {
    pthread_mutex_lock(&proc->lock);
    proc->state = YOS_PROC_ZOMBIE;
    proc->exit_code = exit_code;
    proc->exited = 1;
    pthread_cond_broadcast(&proc->wait_cond);
    pthread_mutex_unlock(&proc->lock);
    YOS_DEBUG("pid=%d exited with code=%d", proc->pid, exit_code);
}

// ============================================================================
// Fork Implementation
//
// 1. Allocate child yos_proc_t in process table
// 2. Create child yos_exec_ctx_t with new wasm3 runtime
// 3. Copy parent's linear memory to child
// 4. Spawn pthread for child
// 5. Parent returns child_pid, child returns 0
// ============================================================================

// Thread argument for child process
typedef struct {
    yos_runtime_t* rt;
    yos_proc_t* proc;
    uint8_t* memory_snapshot;
    size_t memory_size;
} fork_thread_arg_t;

#include "yos-exec-ctx.h"

static void* fork_thread_func(void* arg) {
    fork_thread_arg_t* fta = (fork_thread_arg_t*)arg;

    // Create child execution context with copied memory
    yos_exec_ctx_t* child_ctx = yos_exec_ctx_fork(fta->rt, fta->proc,
                                                   fta->memory_snapshot,
                                                   fta->memory_size);
    free(fta);  // Thread owns fta, free it now

    if (!child_ctx) {
        YOS_ERROR("failed to create child exec_ctx");
        return NULL;
    }

    // Init stdio for child
    yos_exec_ctx_init_stdio(child_ctx);

    // Run child process
    yos_exec_ctx_run(child_ctx);

    // Cleanup
    yos_exec_ctx_destroy(child_ctx);
    return NULL;
}

int32_t yos_fork(yos_exec_ctx_t* ctx) {
    YOS_DEBUG("pid=%d", ctx->proc ? ctx->proc->pid : -1);

    if (!ctx->proc || !ctx->rt) {
        YOS_ERROR("invalid context");
        return -EINVAL;
    }

    // Get parent's memory
    uint8_t* parent_mem = (uint8_t*)ctx->wasm_memory;
    size_t mem_size = ctx->wasm_mem_size;
    if (!parent_mem || mem_size == 0) {
        YOS_ERROR("no wasm memory");
        return -ENOMEM;
    }

    // Allocate child process slot
    yos_proc_t* child_proc = yos_proc_alloc(ctx->rt, ctx->proc->pid);
    if (!child_proc) {
        return -EAGAIN;
    }
    child_proc->pgid = ctx->proc->pgid;
    child_proc->sid = ctx->proc->sid;

    // Copy parent's memory for child
    uint8_t* snapshot = malloc(mem_size);
    if (!snapshot) {
        YOS_ERROR("cannot allocate memory snapshot");
        child_proc->state = YOS_PROC_FREE;
        return -ENOMEM;
    }
    memcpy(snapshot, parent_mem, mem_size);

    // Prepare thread argument
    fork_thread_arg_t* fta = malloc(sizeof(fork_thread_arg_t));
    if (!fta) {
        free(snapshot);
        child_proc->state = YOS_PROC_FREE;
        return -ENOMEM;
    }
    fta->rt = ctx->rt;
    fta->proc = child_proc;
    fta->memory_snapshot = snapshot;
    fta->memory_size = mem_size;

    // Spawn child thread
    child_proc->state = YOS_PROC_RUNNING;
    int r = pthread_create(&child_proc->thread, NULL, fork_thread_func, fta);
    if (r != 0) {
        YOS_ERROR("pthread_create failed: %d", r);
        free(snapshot);
        free(fta);
        child_proc->state = YOS_PROC_FREE;
        return -EAGAIN;
    }
    pthread_detach(child_proc->thread);

    YOS_INFO("forked child pid=%d from parent pid=%d", child_proc->pid, ctx->proc->pid);
    return child_proc->pid;
}

// ============================================================================
// Vfork - like fork but parent blocks until child exec/exit
// ============================================================================

int32_t yos_vfork(yos_exec_ctx_t* ctx) {
    YOS_DEBUG("pid=%d", ctx->proc ? ctx->proc->pid : -1);

    // For now, implement as regular fork
    // TODO: block parent until child calls exec or exit
    return yos_fork(ctx);
}

// ============================================================================
// Exec - replace current process image
// ============================================================================

int yos_execve(yos_exec_ctx_t* ctx, const char* path, char* const argv[], char* const envp[]) {
    YOS_DEBUG("path=%s", path ? path : "(null)");

    if (!path || !ctx) {
        return -EINVAL;
    }

    // Check if file exists and is executable
    if (access(path, X_OK) != 0) {
        YOS_ERROR("cannot execute: %s (%s)", path, strerror(errno));
        return -errno;
    }

    // Store exec path - runtime loop will pick it up
    strncpy(ctx->exec_path, path, sizeof(ctx->exec_path) - 1);
    ctx->exec_path[sizeof(ctx->exec_path) - 1] = '\0';
    ctx->exec_pending = 1;

    // Signal vfork parent if applicable
    if (ctx->proc && ctx->proc->vfork_parent_pid > 0) {
        yos_proc_t* parent = yos_proc_find(ctx->rt, ctx->proc->vfork_parent_pid);
        if (parent) {
            pthread_mutex_lock(&parent->lock);
            parent->vfork_child_done = 1;
            pthread_cond_signal(&parent->vfork_cond);
            pthread_mutex_unlock(&parent->lock);
        }
        ctx->proc->vfork_parent_pid = -1;
    }

    YOS_INFO("exec pending: %s", path);
    return 0;  // Runtime will trap with "yos_exec"
}

// ============================================================================
// Exit
// ============================================================================

void yos__exit(yos_exec_ctx_t* ctx, int status) {
    YOS_DEBUG("status=%d", status);
    if (ctx->proc) {
        yos_proc_exit(ctx->proc, status);
    }
}

void yos_exit(yos_exec_ctx_t* ctx, int status) {
    yos__exit(ctx, status);
}

// ============================================================================
// Process ID functions
// ============================================================================

int32_t yos_getpid(yos_exec_ctx_t* ctx) {
    // ctx->proc must NEVER be NULL - it's an invariant
    // If it is, we have a bug in context creation
    if (!ctx->proc) {
        YOS_ERROR("BUG: exec context has no process!");
        abort();
    }
    return ctx->proc->pid;
}

int32_t yos_getppid(yos_exec_ctx_t* ctx) {
    if (!ctx->proc) {
        YOS_ERROR("BUG: exec context has no process!");
        abort();
    }
    return ctx->proc->ppid;
}

int32_t yos_getpgrp(yos_exec_ctx_t* ctx) {
    if (!ctx->proc) {
        YOS_ERROR("BUG: exec context has no process!");
        abort();
    }
    return ctx->proc->pgid;
}

int yos_setpgid(yos_exec_ctx_t* ctx, int32_t pid, int32_t pgid) {
    if (!ctx->proc) return -ESRCH;

    int32_t target = (pid == 0) ? ctx->proc->pid : pid;
    int32_t newpgid = (pgid == 0) ? target : pgid;

    yos_proc_t* proc = yos_proc_find(ctx->rt, target);
    if (!proc) return -ESRCH;

    proc->pgid = newpgid;
    return 0;
}

int32_t yos_setsid(yos_exec_ctx_t* ctx) {
    if (!ctx->proc) return -EPERM;
    if (ctx->proc->pid == ctx->proc->sid) return -EPERM;

    ctx->proc->sid = ctx->proc->pid;
    ctx->proc->pgid = ctx->proc->pid;
    return ctx->proc->sid;
}

int32_t yos_getsid(yos_exec_ctx_t* ctx, int32_t pid) {
    int32_t target = (pid == 0) ? ctx->proc->pid : pid;
    yos_proc_t* proc = yos_proc_find(ctx->rt, target);
    return proc ? proc->sid : -ESRCH;
}

// ============================================================================
// Wait Implementation
//
// Internal function does the actual work. Public functions call it with
// appropriate parameters.
// ============================================================================

#include <sys/resource.h>

// Internal: core wait implementation
// pid: >0 = specific child, -1 = any child, 0 = any in pgrp, <-1 = any in pgrp -pid
// Returns: child pid on success, 0 if WNOHANG and no child ready, -errno on error
static int32_t wait_internal(yos_exec_ctx_t* ctx, int32_t pid, int* status,
                             int options, struct rusage* rusage) {
    if (!ctx->proc) {
        YOS_ERROR("BUG: exec context has no process!");
        abort();
    }

    int32_t my_pid = ctx->proc->pid;
    int wnohang = options & 1;  // WNOHANG = 0x01

    // Helper: fill rusage with zeros (we don't track real usage yet)
    if (rusage) {
        memset(rusage, 0, sizeof(*rusage));
    }

    // Wait for specific child
    if (pid > 0) {
        yos_proc_t* child = yos_proc_find(ctx->rt, pid);
        if (!child || child->ppid != my_pid) {
            YOS_DEBUG("no such child pid=%d", pid);
            return -ECHILD;
        }

        pthread_mutex_lock(&child->lock);

        // If child hasn't exited and we're not doing WNOHANG, block
        if (!child->exited && !wnohang) {
            YOS_DEBUG("blocking wait for pid=%d", pid);
            while (!child->exited) {
                pthread_cond_wait(&child->wait_cond, &child->lock);
            }
        }

        if (child->exited) {
            int32_t code = child->exit_code;
            pthread_mutex_unlock(&child->lock);

            // Set status in WEXITSTATUS format
            if (status) *status = (code & 0xff) << 8;

            // Reap the child (mark slot as free)
            YOS_DEBUG("reaped child pid=%d exit_code=%d", pid, code);
            child->state = YOS_PROC_FREE;
            return pid;
        }

        // WNOHANG: child exists but hasn't exited yet
        pthread_mutex_unlock(&child->lock);
        return 0;
    }

    // pid == -1: wait for any child
    // pid == 0: wait for any child in same process group (TODO)
    // pid < -1: wait for any child in process group -pid (TODO)

    pthread_mutex_lock(&ctx->rt->proc_lock);
    for (int i = 0; i < YOS_MAX_PROCS; i++) {
        yos_proc_t* child = &ctx->rt->procs[i];
        if (child->ppid == my_pid && child->state == YOS_PROC_ZOMBIE) {
            int32_t child_pid = child->pid;
            int32_t code = child->exit_code;
            child->state = YOS_PROC_FREE;
            pthread_mutex_unlock(&ctx->rt->proc_lock);

            if (status) *status = (code & 0xff) << 8;
            YOS_DEBUG("reaped zombie child pid=%d exit_code=%d", child_pid, code);
            return child_pid;
        }
    }
    pthread_mutex_unlock(&ctx->rt->proc_lock);

    // No zombie children found
    return wnohang ? 0 : -ECHILD;
}

// Public wait functions - all delegate to wait_internal

int32_t yos_wait(yos_exec_ctx_t* ctx, int* status) {
    return wait_internal(ctx, -1, status, 0, NULL);
}

int32_t yos_waitpid(yos_exec_ctx_t* ctx, int32_t pid, int* status, int options) {
    return wait_internal(ctx, pid, status, options, NULL);
}

int32_t yos_wait3(yos_exec_ctx_t* ctx, int* status, int options, struct rusage* rusage) {
    return wait_internal(ctx, -1, status, options, rusage);
}

int32_t yos_wait4(yos_exec_ctx_t* ctx, int32_t pid, int* status, int options, struct rusage* rusage) {
    return wait_internal(ctx, pid, status, options, rusage);
}

// ============================================================================
// Signals (minimal)
// ============================================================================

int yos_kill(yos_exec_ctx_t* ctx, int32_t pid, int sig) {
    YOS_DEBUG("pid=%d sig=%d", pid, sig);

    if (sig == 0) {
        // Check if process exists
        return yos_proc_find(ctx->rt, pid) ? 0 : -ESRCH;
    }

    yos_proc_t* proc = yos_proc_find(ctx->rt, pid);
    if (!proc) return -ESRCH;

    // For SIGKILL/SIGTERM, mark process as exited
    if (sig == 9 || sig == 15) {
        yos_proc_exit(proc, 128 + sig);
    }

    return 0;
}

int yos_raise(yos_exec_ctx_t* ctx, int sig) {
    if (!ctx->proc) {
        YOS_ERROR("BUG: exec context has no process!");
        abort();
    }
    return yos_kill(ctx, ctx->proc->pid, sig);
}
