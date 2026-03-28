#include "syscalls.hpp"
#include "runtime.hpp"

#include <cstdio>
#include <cstring>
#include <unistd.h>

namespace yos {

static ProcessContext* getCtx(IM3Runtime rt) {
    return static_cast<ProcessContext*>(m3_GetUserData(rt));
}

// yos_fork() -> i32
m3ApiRawFunction(syscall_fork) {
    m3ApiReturnType(int32_t);

    auto* ctx = getCtx(runtime);

    // Build a temporary WasmProcess to pass to forkProcess
    WasmProcess wp;
    wp.wrt = runtime;
    wp.ctx = ctx;

    Pid childPid = ctx->runtime->forkProcess(wp);
    m3ApiReturn(childPid);
}

// yos_fork_result() -> i32
// Returns 0 if this process was created by fork, -1 otherwise
m3ApiRawFunction(syscall_fork_result) {
    m3ApiReturnType(int32_t);
    auto* ctx = getCtx(runtime);
    m3ApiReturn(ctx->isChild ? 0 : -1);
}

// yos_getpid() -> i32
m3ApiRawFunction(syscall_getpid) {
    m3ApiReturnType(int32_t);
    auto* ctx = getCtx(runtime);
    m3ApiReturn(ctx->process->pid);
}

// yos_getppid() -> i32
m3ApiRawFunction(syscall_getppid) {
    m3ApiReturnType(int32_t);
    auto* ctx = getCtx(runtime);
    m3ApiReturn(ctx->process->parentPid);
}

// yos_exit(code: i32) -> void
m3ApiRawFunction(syscall_exit) {
    m3ApiGetArg(int32_t, code);
    auto* ctx = getCtx(runtime);
    ctx->runtime->processTable().exit(ctx->process->pid, code);
    m3ApiTrap("yos_exit");
}

// yos_wait(child_pid: i32) -> i32
m3ApiRawFunction(syscall_wait) {
    m3ApiReturnType(int32_t);
    m3ApiGetArg(int32_t, childPid);
    auto* ctx = getCtx(runtime);
    int32_t exitCode = ctx->runtime->processTable().wait(ctx->process->pid, childPid);
    m3ApiReturn(exitCode);
}

// yos_write(fd: i32, buf: ptr, len: i32) -> i32
m3ApiRawFunction(syscall_write) {
    m3ApiReturnType(int32_t);
    m3ApiGetArg(int32_t, fd);
    m3ApiGetArgMem(const uint8_t*, buf);
    m3ApiGetArg(int32_t, len);

    m3ApiCheckMem(buf, len);

    auto* ctx = getCtx(runtime);
    FILE* stream = (fd == 2) ? stderr : stdout;

    // Build complete line: [pid N] <data>
    // Only prefix at start of each line within the buffer
    const uint8_t* p = buf;
    const uint8_t* end = buf + len;
    int32_t total = 0;

    while (p < end) {
        // Find end of line
        const uint8_t* nl = p;
        while (nl < end && *nl != '\n') nl++;
        bool hasNewline = (nl < end);

        fprintf(stream, "[pid %d] %.*s%s",
                ctx->process->pid,
                (int)(nl - p), p,
                hasNewline ? "\n" : "");
        total += static_cast<int32_t>(nl - p) + (hasNewline ? 1 : 0);

        p = hasNewline ? nl + 1 : end;
    }
    fflush(stream);

    m3ApiReturn(total);
}

// yos_exec(path: ptr, path_len: i32) -> i32
// Replaces current program with a new .wasm binary. Does not return on success.
m3ApiRawFunction(syscall_exec) {
    m3ApiReturnType(int32_t);
    m3ApiGetArgMem(const char*, path);
    m3ApiGetArg(int32_t, pathLen);

    m3ApiCheckMem(path, pathLen);

    auto* ctx = getCtx(runtime);

    // Store the exec path for runProcess to pick up
    ctx->execPath = std::string(path, pathLen);

    // Trap to break out of the interpreter - runProcess will handle the exec
    m3ApiTrap("yos_exec");
}

} // namespace yos
