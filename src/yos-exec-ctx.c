// YOS Execution Context Management Implementation
//
// Creates and manages per-process execution contexts with wasm3 runtimes.
//

#define _GNU_SOURCE
#include "yos-exec-ctx.h"
#include "yos-log.h"
#include "yos-process.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>

#include "wasm3.h"
#include "m3_env.h"

// Generated libc wrappers
#include "libc-wrappers.h"

// ============================================================================
// yos_runtime_t functions
// ============================================================================

yos_runtime_t* yos_runtime_create(void) {
    yos_runtime_t* rt = calloc(1, sizeof(yos_runtime_t));
    if (!rt) return NULL;

    pthread_mutex_init(&rt->proc_lock, NULL);
    rt->next_pid = 1;

    for (int i = 0; i < YOS_MAX_PROCS; i++) {
        rt->procs[i].state = YOS_PROC_FREE;
    }

    YOS_DEBUG("runtime created");
    return rt;
}

int yos_runtime_load_wasm(yos_runtime_t* rt, const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        YOS_ERROR("cannot open: %s (%s)", path, strerror(errno));
        return -1;
    }

    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);

    uint8_t* bytes = malloc(size);
    if (!bytes) {
        fclose(f);
        return -1;
    }

    if (fread(bytes, 1, size, f) != size) {
        YOS_ERROR("failed to read: %s", path);
        free(bytes);
        fclose(f);
        return -1;
    }
    fclose(f);

    rt->wasm_bytes = bytes;
    rt->wasm_bytes_size = size;

    // Store base path for exec
    char* dir = strdup(path);
    char* slash = strrchr(dir, '/');
    if (slash) {
        *slash = '\0';
        strncpy(rt->base_path, dir, sizeof(rt->base_path) - 1);
    } else {
        getcwd(rt->base_path, sizeof(rt->base_path));
    }
    free(dir);

    YOS_INFO("loaded %zu bytes from %s", size, path);
    return 0;
}

void yos_runtime_destroy(yos_runtime_t* rt) {
    if (!rt) return;

    free(rt->wasm_bytes);
    pthread_mutex_destroy(&rt->proc_lock);
    free(rt);

    YOS_DEBUG("runtime destroyed");
}

// ============================================================================
// yos_exec_ctx_t functions - internal helpers
// ============================================================================

// Internal: create wasm3 runtime and load module
static int exec_ctx_init_wasm(yos_exec_ctx_t* ctx, uint8_t* memory_snapshot, size_t memory_size) {
    yos_runtime_t* rt = ctx->rt;

    IM3Environment env = m3_NewEnvironment();
    if (!env) {
        YOS_ERROR("m3_NewEnvironment failed");
        return -1;
    }

    // ctx is passed as userdata - syscall handlers get it via m3_GetUserData(runtime)
    IM3Runtime wrt = m3_NewRuntime(env, 128 * 1024, ctx);
    if (!wrt) {
        YOS_ERROR("m3_NewRuntime failed");
        m3_FreeEnvironment(env);
        return -1;
    }

    IM3Module module = NULL;
    M3Result result = m3_ParseModule(env, &module, rt->wasm_bytes, rt->wasm_bytes_size);
    if (result) {
        YOS_ERROR("parse error: %s", result);
        m3_FreeRuntime(wrt);
        m3_FreeEnvironment(env);
        return -1;
    }

    result = m3_LoadModule(wrt, module);
    if (result) {
        YOS_ERROR("load error: %s", result);
        m3_FreeModule(module);
        m3_FreeRuntime(wrt);
        m3_FreeEnvironment(env);
        return -1;
    }

    linkLibcFunctions(module);

    ctx->wasm_env = env;
    ctx->wasm_runtime = wrt;
    ctx->wasm_module = module;

    uint32_t mem_size = 0;
    ctx->wasm_memory = m3_GetMemory(wrt, &mem_size, 0);
    ctx->wasm_mem_size = mem_size;

    // Restore memory snapshot if provided (fork case)
    if (memory_snapshot && memory_size > 0) {
        if (mem_size >= memory_size) {
            memcpy(ctx->wasm_memory, memory_snapshot, memory_size);
            YOS_DEBUG("restored memory snapshot (%zu bytes)", memory_size);
        }
        free(memory_snapshot);  // We take ownership
    }

    YOS_DEBUG("wasm initialized: mem=%p size=%u", ctx->wasm_memory, mem_size);
    return 0;
}

// ============================================================================
// yos_exec_ctx_t functions - public API
// ============================================================================

yos_exec_ctx_t* yos_exec_ctx_create(yos_runtime_t* rt) {
    // Allocate init process (pid=1, ppid=0)
    yos_proc_t* proc = yos_proc_alloc(rt, 0);
    if (!proc) return NULL;

    proc->state = YOS_PROC_RUNNING;

    yos_exec_ctx_t* ctx = calloc(1, sizeof(yos_exec_ctx_t));
    if (!ctx) {
        proc->state = YOS_PROC_FREE;
        return NULL;
    }

    ctx->rt = rt;
    ctx->proc = proc;
    ctx->is_child = 0;
    ctx->umask = 022;
    getcwd(ctx->cwd, sizeof(ctx->cwd));

    for (int i = 0; i < YOS_MAX_FDS; i++) {
        ctx->fds[i].host_fd = -1;
    }

    if (exec_ctx_init_wasm(ctx, NULL, 0) != 0) {
        free(ctx);
        proc->state = YOS_PROC_FREE;
        return NULL;
    }

    YOS_INFO("created exec_ctx pid=%d", proc->pid);
    return ctx;
}

yos_exec_ctx_t* yos_exec_ctx_fork(yos_runtime_t* rt, yos_proc_t* child_proc,
                                   uint8_t* memory_snapshot, size_t memory_size) {
    yos_exec_ctx_t* ctx = calloc(1, sizeof(yos_exec_ctx_t));
    if (!ctx) {
        free(memory_snapshot);
        return NULL;
    }

    ctx->rt = rt;
    ctx->proc = child_proc;
    ctx->is_child = 1;
    ctx->umask = 022;
    getcwd(ctx->cwd, sizeof(ctx->cwd));

    for (int i = 0; i < YOS_MAX_FDS; i++) {
        ctx->fds[i].host_fd = -1;
    }

    if (exec_ctx_init_wasm(ctx, memory_snapshot, memory_size) != 0) {
        free(ctx);
        return NULL;
    }

    YOS_INFO("created forked exec_ctx pid=%d", child_proc->pid);
    return ctx;
}

void yos_exec_ctx_init_stdio(yos_exec_ctx_t* ctx) {
    ctx->fds[0].host_fd = STDIN_FILENO;
    strncpy(ctx->fds[0].path, "/dev/stdin", sizeof(ctx->fds[0].path));

    ctx->fds[1].host_fd = STDOUT_FILENO;
    strncpy(ctx->fds[1].path, "/dev/stdout", sizeof(ctx->fds[1].path));

    ctx->fds[2].host_fd = STDERR_FILENO;
    strncpy(ctx->fds[2].path, "/dev/stderr", sizeof(ctx->fds[2].path));

    YOS_DEBUG("stdio initialized");
}

int yos_exec_ctx_run(yos_exec_ctx_t* ctx) {
    IM3Runtime wrt = (IM3Runtime)ctx->wasm_runtime;

    IM3Function func = NULL;
    M3Result result = m3_FindFunction(&func, wrt, "_start");
    if (result) {
        result = m3_FindFunction(&func, wrt, "main");
    }
    if (result) {
        result = m3_FindFunction(&func, wrt, "__main_argc_argv");
    }
    if (result) {
        YOS_ERROR("no entry point found: %s", result);
        return 127;
    }

    const char* entry_name = m3_GetFunctionName(func);
    int needs_argv = entry_name && (strcmp(entry_name, "main") == 0 ||
                                    strcmp(entry_name, "__main_argc_argv") == 0);

    if (needs_argv && ctx->rt->argc > 0 && ctx->rt->argv) {
        uint32_t mem_size = 0;
        uint8_t* mem = m3_GetMemory(wrt, &mem_size, 0);

        size_t strings_size = 0;
        for (int i = 0; i < ctx->rt->argc; i++) {
            strings_size += strlen(ctx->rt->argv[i]) + 1;
        }
        size_t argv_size = (ctx->rt->argc + 1) * sizeof(uint32_t);
        size_t total = argv_size + strings_size;

        uint32_t argv_ptr = (mem_size - total) & ~15;
        uint32_t str_ptr = argv_ptr + argv_size;

        uint32_t* argv_array = (uint32_t*)(mem + argv_ptr);
        for (int i = 0; i < ctx->rt->argc; i++) {
            argv_array[i] = str_ptr;
            size_t len = strlen(ctx->rt->argv[i]) + 1;
            memcpy(mem + str_ptr, ctx->rt->argv[i], len);
            str_ptr += len;
        }
        argv_array[ctx->rt->argc] = 0;

        YOS_DEBUG("calling %s(argc=%d, argv=%u)", entry_name, ctx->rt->argc, argv_ptr);
        result = m3_CallV(func, ctx->rt->argc, argv_ptr);
    } else {
        YOS_DEBUG("calling _start");
        result = m3_CallV(func);
    }

    if (result) {
        if (strstr(result, "exit") || strcmp(result, m3Err_trapExit) == 0) {
            return ctx->proc->exit_code;
        }
        YOS_ERROR("runtime error: %s", result);
        return 1;
    }

    return 0;
}

void yos_exec_ctx_destroy(yos_exec_ctx_t* ctx) {
    if (!ctx) return;

    for (int i = 3; i < YOS_MAX_FDS; i++) {
        if (ctx->fds[i].host_fd >= 0) {
            close(ctx->fds[i].host_fd);
            ctx->fds[i].host_fd = -1;
        }
    }

    for (int i = 0; i < YOS_MAX_DIRS; i++) {
        if (ctx->dirs[i].host_dir) {
            closedir(ctx->dirs[i].host_dir);
            ctx->dirs[i].host_dir = NULL;
        }
    }

    if (ctx->wasm_runtime) {
        m3_FreeRuntime((IM3Runtime)ctx->wasm_runtime);
    }
    if (ctx->wasm_env) {
        m3_FreeEnvironment((IM3Environment)ctx->wasm_env);
    }

    YOS_DEBUG("destroyed exec_ctx pid=%d", ctx->proc ? ctx->proc->pid : -1);
    free(ctx);
}
