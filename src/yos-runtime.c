// YOS Runtime - Memory, Signal, and User/Group operations
//
// VFS operations are in yos-vfs.c
// Process operations are in yos-process.c

#define _GNU_SOURCE
#include "yos-runtime.h"
#include "yos-log.h"

#include <string.h>
#include <errno.h>
#include <sys/utsname.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>

// ============================================================================
// Legacy Initialization (for backwards compatibility)
// ============================================================================

void yos_init(void) {
    yos_log_init();
    YOS_INFO("yos_init called (legacy)");
}

// ============================================================================
// System Info
// ============================================================================

int yos_uname(yos_exec_ctx_t* ctx, void* buf) {
    (void)ctx;
    struct utsname* u = (struct utsname*)buf;
    return uname(u);
}

// ============================================================================
// Varargs Support
// ============================================================================

int yos_varargs_call(yos_exec_ctx_t* ctx, int func_id, uint32_t arg1, uint32_t arg2, uint32_t arg3, void* args_ptr) {
    uint8_t* mem = (uint8_t*)ctx->wasm_memory;
    VarArgPack* pack = (VarArgPack*)args_ptr;

    char buf[4096];
    char* out = buf;
    char* end = buf + sizeof(buf);

    const char* fmt = NULL;
    FILE* stream = NULL;
    char* str_out = NULL;
    size_t str_size = 0;
    int fd = 1;

    // Helper macro to convert WASM offset to host pointer
    #define TO_PTR(offset) ((char*)(mem + (offset)))

    switch (func_id) {
        case VFUNC_PRINTF:
            fmt = TO_PTR(arg1);
            stream = stdout;
            break;
        case VFUNC_FPRINTF:
            stream = (FILE*)(mem + arg1);
            fmt = TO_PTR(arg2);
            break;
        case VFUNC_SPRINTF:
            str_out = TO_PTR(arg1);
            fmt = TO_PTR(arg2);
            str_size = SIZE_MAX;
            break;
        case VFUNC_SNPRINTF:
            str_out = TO_PTR(arg1);
            str_size = (size_t)arg2;
            fmt = TO_PTR(arg3);
            break;
        case VFUNC_DPRINTF:
            fd = (int)arg1;
            fmt = TO_PTR(arg2);
            break;
        case VFUNC_VASPRINTF:
            str_out = NULL;
            fmt = TO_PTR(arg2);
            str_size = SIZE_MAX;
            break;
        default:
            return -1;
    }

    if (!fmt) return -1;

    // Format string with packed arguments
    int arg_idx = 0;
    const char* p = fmt;
    int total = 0;

    while (*p && out < end - 1) {
        if (*p != '%') {
            *out++ = *p++;
            total++;
            continue;
        }

        const char* spec_start = p;
        p++;

        if (*p == '%') {
            *out++ = '%';
            p++;
            total++;
            continue;
        }

        // Skip flags, width, precision
        while (*p == '-' || *p == '+' || *p == ' ' || *p == '#' || *p == '0') p++;
        while (*p >= '0' && *p <= '9') p++;
        if (*p == '.') { p++; while (*p >= '0' && *p <= '9') p++; }

        // Length modifiers
        if (*p == 'l') { p++; if (*p == 'l') p++; }
        else if (*p == 'h') { p++; if (*p == 'h') p++; }
        else if (*p == 'z' || *p == 'j' || *p == 't') p++;

        char conv = *p++;

        // Copy format specifier
        size_t spec_len = p - spec_start;
        char spec[32];
        if (spec_len < sizeof(spec)) {
            memcpy(spec, spec_start, spec_len);
            spec[spec_len] = '\0';
        } else {
            spec[0] = '\0';
        }

        // Format with actual argument
        int n = 0;
        if (arg_idx < VARG_MAX && pack->types[arg_idx] != VARG_END) {
            switch (pack->types[arg_idx]) {
                case VARG_INT:
                    n = snprintf(out, end - out, spec, (int)pack->values[arg_idx]);
                    break;
                case VARG_UINT:
                    n = snprintf(out, end - out, spec, (unsigned int)pack->values[arg_idx]);
                    break;
                case VARG_LONG:
                    n = snprintf(out, end - out, spec, (long)pack->values[arg_idx]);
                    break;
                case VARG_ULONG:
                    n = snprintf(out, end - out, spec, (unsigned long)pack->values[arg_idx]);
                    break;
                case VARG_STR:
                    n = snprintf(out, end - out, spec, TO_PTR((uint32_t)pack->values[arg_idx]));
                    break;
                case VARG_PTR:
                    n = snprintf(out, end - out, spec, TO_PTR((uint32_t)pack->values[arg_idx]));
                    break;
                case VARG_CHAR:
                    n = snprintf(out, end - out, spec, (int)pack->values[arg_idx]);
                    break;
            }
            arg_idx++;
        }
        if (n > 0) {
            out += n;
            total += n;
        }
        (void)conv;
    }
    *out = '\0';

    #undef TO_PTR

    // Output result
    int result = total;
    switch (func_id) {
        case VFUNC_PRINTF:
        case VFUNC_FPRINTF:
            result = fputs(buf, stream);
            if (result >= 0) result = total;
            break;
        case VFUNC_SPRINTF:
            strcpy(str_out, buf);
            break;
        case VFUNC_SNPRINTF:
            strncpy(str_out, buf, str_size);
            if (str_size > 0) str_out[str_size - 1] = '\0';
            break;
        case VFUNC_DPRINTF:
            result = write(fd, buf, total);
            break;
        case VFUNC_VASPRINTF:
            result = -1;
            break;
    }

    return result;
}

// ============================================================================
// Memory Hooks
// ============================================================================

void* yos_sbrk(yos_exec_ctx_t* ctx, intptr_t increment) {
    YOS_DEBUG("increment=%ld heap_end=%u", (long)increment, ctx->heap_end);

    if (increment == 0) {
        // Return current break
        return ctx->wasm_memory ? (uint8_t*)ctx->wasm_memory + ctx->heap_end : NULL;
    }

    uint32_t old_end = ctx->heap_end;
    uint32_t new_end = old_end + increment;

    // Check bounds
    if (new_end > ctx->wasm_mem_size || new_end < old_end) {
        YOS_ERROR("sbrk: out of memory (requested %u, have %zu)", new_end, ctx->wasm_mem_size);
        errno = ENOMEM;
        return (void*)-1;
    }

    ctx->heap_end = new_end;

    // Zero the new memory
    if (increment > 0 && ctx->wasm_memory) {
        memset((uint8_t*)ctx->wasm_memory + old_end, 0, increment);
    }

    YOS_TRACE("sbrk: %u -> %u", old_end, new_end);
    return ctx->wasm_memory ? (uint8_t*)ctx->wasm_memory + old_end : NULL;
}

int yos_brk(yos_exec_ctx_t* ctx, void* addr) {
    YOS_DEBUG("addr=%p", addr);

    if (!ctx->wasm_memory) {
        return -ENOMEM;
    }

    uintptr_t target = (uintptr_t)addr - (uintptr_t)ctx->wasm_memory;
    if (target > ctx->wasm_mem_size) {
        return -ENOMEM;
    }

    ctx->heap_end = (uint32_t)target;
    return 0;
}

void* yos_mmap(yos_exec_ctx_t* ctx, void* addr, size_t length, int prot, int flags, int fd, off_t offset) {
    YOS_DEBUG("addr=%p len=%zu prot=%d flags=%d fd=%d off=%ld", addr, length, prot, flags, fd, (long)offset);
    (void)ctx;

    // For now, mmap always fails in sandbox
    // TODO: implement anonymous mmap using linear memory
    YOS_WARN("mmap not implemented");
    errno = ENOSYS;
    return (void*)-1;
}

int yos_munmap(yos_exec_ctx_t* ctx, void* addr, size_t length) {
    YOS_DEBUG("addr=%p len=%zu", addr, length);
    (void)ctx;

    // No-op for sandbox
    return 0;
}

int yos_mprotect(yos_exec_ctx_t* ctx, void* addr, size_t len, int prot) {
    YOS_DEBUG("addr=%p len=%zu prot=%d", addr, len, prot);
    (void)ctx;

    // No-op for sandbox - we can't actually change protection
    return 0;
}

void* yos_mremap(yos_exec_ctx_t* ctx, void* old_addr, size_t old_size, size_t new_size, int flags, void* new_addr) {
    YOS_DEBUG("old=%p old_size=%zu new_size=%zu flags=%d new=%p", old_addr, old_size, new_size, flags, new_addr);
    (void)ctx;

    YOS_WARN("mremap not implemented");
    errno = ENOSYS;
    return (void*)-1;
}

// ============================================================================
// Signal Hooks (minimal stub implementation for sandbox)
// ============================================================================

yos_sighandler_t yos_signal(yos_exec_ctx_t* ctx, int signum, yos_sighandler_t handler) {
    YOS_DEBUG("signum=%d handler=%p", signum, (void*)handler);
    (void)ctx;

    // Return old handler (SIG_DFL = 0)
    return (yos_sighandler_t)0;
}

int yos_sigaction(yos_exec_ctx_t* ctx, int signum, const void* act, void* oldact) {
    YOS_DEBUG("signum=%d act=%p oldact=%p", signum, act, oldact);
    (void)ctx;

    // Zero out oldact if provided
    if (oldact) {
        memset(oldact, 0, 152);  // sizeof(struct sigaction) on Linux
    }
    return 0;
}

int yos_sigprocmask(yos_exec_ctx_t* ctx, int how, const void* set, void* oldset) {
    YOS_DEBUG("how=%d set=%p oldset=%p", how, set, oldset);
    (void)ctx;

    if (oldset) {
        memset(oldset, 0, 128);  // sizeof(sigset_t)
    }
    return 0;
}

int yos_sigemptyset(yos_exec_ctx_t* ctx, void* set) {
    (void)ctx;
    if (set) memset(set, 0, 128);
    return 0;
}

int yos_sigfillset(yos_exec_ctx_t* ctx, void* set) {
    (void)ctx;
    if (set) memset(set, 0xFF, 128);
    return 0;
}

int yos_sigaddset(yos_exec_ctx_t* ctx, void* set, int signum) {
    (void)ctx; (void)set; (void)signum;
    return 0;
}

int yos_sigdelset(yos_exec_ctx_t* ctx, void* set, int signum) {
    (void)ctx; (void)set; (void)signum;
    return 0;
}

int yos_sigismember(yos_exec_ctx_t* ctx, const void* set, int signum) {
    (void)ctx; (void)set; (void)signum;
    return 0;
}

int yos_sigpending(yos_exec_ctx_t* ctx, void* set) {
    (void)ctx;
    if (set) memset(set, 0, 128);
    return 0;
}

int yos_sigsuspend(yos_exec_ctx_t* ctx, const void* mask) {
    (void)ctx; (void)mask;
    // Just return interrupted
    return -EINTR;
}

// ============================================================================
// User/Group Hooks (sandbox returns fixed values)
// ============================================================================

uint32_t yos_getuid(yos_exec_ctx_t* ctx) {
    (void)ctx;
    return 1000;
}

uint32_t yos_geteuid(yos_exec_ctx_t* ctx) {
    (void)ctx;
    return 1000;
}

uint32_t yos_getgid(yos_exec_ctx_t* ctx) {
    (void)ctx;
    return 1000;
}

uint32_t yos_getegid(yos_exec_ctx_t* ctx) {
    (void)ctx;
    return 1000;
}

int yos_setuid(yos_exec_ctx_t* ctx, uint32_t uid) {
    (void)ctx; (void)uid;
    return 0;  // Pretend it worked
}

int yos_seteuid(yos_exec_ctx_t* ctx, uint32_t euid) {
    (void)ctx; (void)euid;
    return 0;
}

int yos_setgid(yos_exec_ctx_t* ctx, uint32_t gid) {
    (void)ctx; (void)gid;
    return 0;
}

int yos_setegid(yos_exec_ctx_t* ctx, uint32_t egid) {
    (void)ctx; (void)egid;
    return 0;
}

int yos_setreuid(yos_exec_ctx_t* ctx, uint32_t ruid, uint32_t euid) {
    (void)ctx; (void)ruid; (void)euid;
    return 0;
}

int yos_setregid(yos_exec_ctx_t* ctx, uint32_t rgid, uint32_t egid) {
    (void)ctx; (void)rgid; (void)egid;
    return 0;
}

int yos_getgroups(yos_exec_ctx_t* ctx, int size, uint32_t* list) {
    (void)ctx;
    if (size == 0) return 1;  // Just one group
    if (list && size >= 1) {
        list[0] = 1000;  // Same as gid
    }
    return 1;
}

int yos_setgroups(yos_exec_ctx_t* ctx, size_t size, const uint32_t* list) {
    (void)ctx; (void)size; (void)list;
    return 0;  // Pretend it worked
}
