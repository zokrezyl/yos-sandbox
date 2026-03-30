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

// yos_vfork() -> i32
// Parent blocks until child calls exec or exit
m3ApiRawFunction(syscall_vfork) {
    m3ApiReturnType(int32_t);

    auto* ctx = getCtx(runtime);

    WasmProcess wp;
    wp.wrt = runtime;
    wp.ctx = ctx;

    Pid childPid = ctx->runtime->vforkProcess(wp);
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

// yos_getpgrp() -> i32
m3ApiRawFunction(syscall_getpgrp) {
    m3ApiReturnType(int32_t);
    auto* ctx = getCtx(runtime);
    m3ApiReturn(ctx->process->pgid);
}

// yos_setpgid(pid, pgid) -> i32 (0 on success, -1 on error)
m3ApiRawFunction(syscall_setpgid) {
    m3ApiReturnType(int32_t);
    m3ApiGetArg(int32_t, pid);
    m3ApiGetArg(int32_t, pgid);

    auto* ctx = getCtx(runtime);
    auto& table = ctx->runtime->processTable();

    // pid 0 means current process
    Pid targetPid = (pid == 0) ? ctx->process->pid : pid;
    // pgid 0 means use target's pid as pgid
    Pid targetPgid = (pgid == 0) ? targetPid : pgid;

    auto proc = table.get(targetPid);
    if (!proc) m3ApiReturn(-1);

    // Can only change own process or child's pgid
    if (targetPid != ctx->process->pid && proc->parentPid != ctx->process->pid) {
        m3ApiReturn(-1);
    }

    proc->pgid = targetPgid;
    m3ApiReturn(0);
}

// yos_setsid() -> i32 (new session id, or -1 on error)
m3ApiRawFunction(syscall_setsid) {
    m3ApiReturnType(int32_t);
    auto* ctx = getCtx(runtime);
    auto proc = ctx->process;

    // Can't call setsid if already a process group leader
    if (proc->pid == proc->pgid) {
        m3ApiReturn(-1);
    }

    // Create new session: become session leader and process group leader
    proc->sid = proc->pid;
    proc->pgid = proc->pid;
    m3ApiReturn(proc->sid);
}

// yos_getsid(pid) -> i32
m3ApiRawFunction(syscall_getsid) {
    m3ApiReturnType(int32_t);
    m3ApiGetArg(int32_t, pid);

    auto* ctx = getCtx(runtime);
    Pid targetPid = (pid == 0) ? ctx->process->pid : pid;

    auto proc = ctx->runtime->processTable().get(targetPid);
    if (!proc) m3ApiReturn(-1);

    m3ApiReturn(proc->sid);
}

// yos_exit(code: i32) -> void
m3ApiRawFunction(syscall_exit) {
    m3ApiGetArg(int32_t, code);
    auto* ctx = getCtx(runtime);

    // If this is a vfork child, signal parent before exiting
    ctx->runtime->vforkChildDone(ctx->process->pid);

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

// yos_read(fd: i32, buf: ptr, len: i32) -> i32 (bytes read)
m3ApiRawFunction(syscall_read) {
    m3ApiReturnType(int32_t);
    m3ApiGetArg(int32_t, fd);
    m3ApiGetArgMem(uint8_t*, buf);
    m3ApiGetArg(int32_t, len);

    m3ApiCheckMem(buf, len);

    int hostFd = (fd == 0) ? STDIN_FILENO : fd;
    ssize_t n = ::read(hostFd, buf, len);
    m3ApiReturn(static_cast<int32_t>(n));
}

// yos_exec(path: ptr, argv: ptr) -> i32
// path: null-terminated string
// argv: null-terminated array of wasm32 pointers to null-terminated strings
// Does not return on success.
m3ApiRawFunction(syscall_exec) {
    m3ApiReturnType(int32_t);
    m3ApiGetArg(uint32_t, pathOffset);
    m3ApiGetArg(uint32_t, argvOffset);

    uint32_t memSize = m3_GetMemorySize(runtime);

    // Read path
    const char* path = (const char*)((uint8_t*)_mem + pathOffset);
    // Bounds check path
    if (pathOffset >= memSize) m3ApiTrap(m3Err_trapOutOfBoundsMemoryAccess);

    auto* ctx = getCtx(runtime);
    ctx->execPath = std::string(path);

    // Read argv array from wasm memory
    ctx->execArgv.clear();
    if (argvOffset != 0 && argvOffset < memSize) {
        const uint32_t* argvPtr = (const uint32_t*)((uint8_t*)_mem + argvOffset);
        for (int i = 0; i < 256; i++) { // safety limit
            uint32_t ptrOffset = argvPtr[i];
            if (ptrOffset == 0) break;
            if (ptrOffset >= memSize) break;
            const char* arg = (const char*)((uint8_t*)_mem + ptrOffset);
            ctx->execArgv.push_back(std::string(arg));
        }
    }

    // If this is a vfork child, signal parent before exec
    ctx->runtime->vforkChildDone(ctx->process->pid);

    m3ApiTrap("yos_exec");
}

// yos_spawn(path: ptr, argv: ptr) -> i32 (child pid, or -1 on error)
// Combined fork+exec: creates child process running the given wasm program.
m3ApiRawFunction(syscall_spawn) {
    m3ApiReturnType(int32_t);
    m3ApiGetArg(uint32_t, pathOffset);
    m3ApiGetArg(uint32_t, argvOffset);

    uint32_t memSize = m3_GetMemorySize(runtime);
    if (pathOffset >= memSize) m3ApiReturn(-1);

    const char* path = (const char*)((uint8_t*)_mem + pathOffset);

    // Read argv
    std::vector<std::string> argv;
    if (argvOffset != 0 && argvOffset < memSize) {
        const uint32_t* argvPtr = (const uint32_t*)((uint8_t*)_mem + argvOffset);
        for (int i = 0; i < 256; i++) {
            uint32_t ptrOffset = argvPtr[i];
            if (ptrOffset == 0) break;
            if (ptrOffset >= memSize) break;
            argv.push_back(std::string((const char*)((uint8_t*)_mem + ptrOffset)));
        }
    }

    auto* ctx = getCtx(runtime);
    Pid childPid = ctx->runtime->spawnProcess(ctx->process->pid, std::string(path), std::move(argv));
    m3ApiReturn(childPid);
}

// ---- WASI syscalls ----

// Generic stub returning ENOSYS for unimplemented WASI functions
m3ApiRawFunction(wasi_stub_enosys) {
    m3ApiReturnType(int32_t);
    m3ApiReturn(76); // __WASI_ERRNO_NOSYS
}

// fd_fdstat_get(fd: i32, fdstat_ptr: i32) -> errno
m3ApiRawFunction(wasi_fd_fdstat_get) {
    m3ApiReturnType(int32_t);
    m3ApiGetArg(int32_t, fd);
    m3ApiGetArgMem(uint8_t*, fdstat);

    // fdstat struct: filetype(u8) + pad(1) + flags(u16) + pad(4) + rights_base(u64) + rights_inheriting(u64) = 24 bytes
    m3ApiCheckMem(fdstat, 24);
    memset(fdstat, 0, 24);

    if (fd <= 2) {
        fdstat[0] = 2; // FILETYPE_CHARACTER_DEVICE
        // Set all rights
        uint64_t rights = ~0ULL;
        memcpy(fdstat + 8, &rights, 8);
        memcpy(fdstat + 16, &rights, 8);
    }
    m3ApiReturn(0);
}

// fd_close(fd: i32) -> errno
m3ApiRawFunction(wasi_fd_close) {
    m3ApiReturnType(int32_t);
    m3ApiGetArg(int32_t, fd);
    // Don't actually close stdin/stdout/stderr
    if (fd <= 2) { m3ApiReturn(0); }
    m3ApiReturn(0);
}

// fd_seek(fd: i32, offset: i64, whence: i32, newoffset_ptr: i32) -> errno
m3ApiRawFunction(wasi_fd_seek) {
    m3ApiReturnType(int32_t);
    m3ApiGetArg(int32_t, fd);
    m3ApiGetArg(int64_t, offset);
    m3ApiGetArg(int32_t, whence);
    m3ApiGetArgMem(uint64_t*, newoffsetPtr);
    // Not supported for now
    m3ApiReturn(8); // EBADF
}

// WASI fd_write: (fd, iovs_ptr, iovs_len, nwritten_ptr) -> errno
// iovs is array of {buf_ptr: i32, buf_len: i32}
m3ApiRawFunction(wasi_fd_write) {
    m3ApiReturnType(int32_t);
    m3ApiGetArg(int32_t, fd);
    m3ApiGetArgMem(uint32_t*, iovs);
    m3ApiGetArg(int32_t, iovsLen);
    m3ApiGetArgMem(uint32_t*, nwrittenPtr);

    auto* ctx = getCtx(runtime);
    FILE* stream = (fd == 2) ? stderr : stdout;
    uint32_t totalWritten = 0;

    for (int32_t i = 0; i < iovsLen; i++) {
        uint32_t bufOffset = iovs[i * 2];
        uint32_t bufLen = iovs[i * 2 + 1];
        const uint8_t* buf = (const uint8_t*)_mem + bufOffset;

        // Write with pid prefix per line
        const uint8_t* p = buf;
        const uint8_t* end = buf + bufLen;
        while (p < end) {
            const uint8_t* nl = p;
            while (nl < end && *nl != '\n') nl++;
            bool hasNewline = (nl < end);
            fprintf(stream, "[pid %d] %.*s%s",
                    ctx->process->pid,
                    (int)(nl - p), p,
                    hasNewline ? "\n" : "");
            p = hasNewline ? nl + 1 : end;
        }
        totalWritten += bufLen;
    }
    fflush(stream);

    *nwrittenPtr = totalWritten;
    m3ApiReturn(0);
}

// WASI fd_read: (fd, iovs_ptr, iovs_len, nread_ptr) -> errno
m3ApiRawFunction(wasi_fd_read) {
    m3ApiReturnType(int32_t);
    m3ApiGetArg(int32_t, fd);
    m3ApiGetArgMem(uint32_t*, iovs);
    m3ApiGetArg(int32_t, iovsLen);
    m3ApiGetArgMem(uint32_t*, nreadPtr);

    uint32_t totalRead = 0;

    for (int32_t i = 0; i < iovsLen; i++) {
        uint32_t bufOffset = iovs[i * 2];
        uint32_t bufLen = iovs[i * 2 + 1];
        uint8_t* buf = (uint8_t*)_mem + bufOffset;

        int hostFd = (fd == 0) ? STDIN_FILENO : fd;
        ssize_t n = ::read(hostFd, buf, bufLen);
        if (n > 0) totalRead += n;
        if (n < (ssize_t)bufLen) break; // short read
    }

    *nreadPtr = totalRead;
    m3ApiReturn(0);
}

// args_sizes_get(argc_ptr, argv_buf_size_ptr) -> errno
m3ApiRawFunction(wasi_args_sizes_get) {
    m3ApiReturnType(int32_t);
    m3ApiGetArgMem(uint32_t*, argcPtr);
    m3ApiGetArgMem(uint32_t*, bufSizePtr);
    // TODO: pass real args from process context
    *argcPtr = 1;
    *bufSizePtr = 8; // "busybox\0"
    m3ApiReturn(0);
}

// args_get(argv_ptr, argv_buf_ptr) -> errno
m3ApiRawFunction(wasi_args_get) {
    m3ApiReturnType(int32_t);
    m3ApiGetArgMem(uint32_t*, argvPtr);
    m3ApiGetArgMem(char*, argvBuf);
    // TODO: pass real args
    memcpy(argvBuf, "busybox", 8);
    argvPtr[0] = m3ApiPtrToOffset(argvBuf);
    m3ApiReturn(0);
}

// proc_exit(code: i32) -> noreturn
m3ApiRawFunction(wasi_proc_exit) {
    m3ApiGetArg(int32_t, code);
    auto* ctx = getCtx(runtime);
    ctx->runtime->processTable().exit(ctx->process->pid, code);
    m3ApiTrap("yos_exit");
}

} // namespace yos
