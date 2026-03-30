#include "runtime.hpp"
#include "syscalls.hpp"
#include "m3_api_wasi.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <thread>

namespace yos {

Runtime::Runtime() = default;
Runtime::~Runtime() = default;

bool Runtime::loadWasm(std::string_view path, std::vector<uint8_t>& out) {
    // Resolve relative paths against _basePath
    std::filesystem::path p(path);
    if (p.is_relative() && !_basePath.empty()) {
        p = std::filesystem::path(_basePath) / p;
    }

    std::ifstream file(p, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        fprintf(stderr, "yos: cannot open %s\n", p.c_str());
        return false;
    }

    auto size = file.tellg();
    file.seekg(0);
    out.resize(static_cast<size_t>(size));
    file.read(reinterpret_cast<char*>(out.data()), size);
    return true;
}

int Runtime::run(std::string_view wasmPath, int argc, char** argv) {
    auto absPath = std::filesystem::absolute(std::string(wasmPath));
    _basePath = absPath.parent_path().string();
    _argc = argc;
    _argv = argv;

    if (!loadWasm(absPath.string(), _wasmBytes)) return 1;

    auto initProc = _processTable.createInit();
    runProcess(initProc, _wasmBytes, false);

    return initProc->exitCode;
}

WasmProcess Runtime::createWasmProcess(
    std::shared_ptr<Process> proc,
    const std::vector<uint8_t>& wasmBytes,
    const std::vector<uint8_t>& memorySnapshot,
    bool isChild
) {
    WasmProcess wp;
    wp.wasmBytes = wasmBytes;
    wp.ctx = new ProcessContext{this, proc, isChild, {}};

    wp.env = m3_NewEnvironment();
    wp.wrt = m3_NewRuntime(wp.env, 64 * 1024, wp.ctx);

    IM3Module module = nullptr;
    M3Result result = m3_ParseModule(wp.env, &module, wp.wasmBytes.data(),
                                     static_cast<uint32_t>(wp.wasmBytes.size()));
    if (result) {
        fprintf(stderr, "yos: parse error: %s\n", result);
        return wp;
    }

    result = m3_LoadModule(wp.wrt, module);
    if (result) {
        fprintf(stderr, "yos: load error: %s\n", result);
        m3_FreeModule(module);
        return wp;
    }
    wp.module = module;

    linkSyscalls(wp.module);

    if (!memorySnapshot.empty()) {
        uint32_t memSize = 0;
        uint8_t* mem = m3_GetMemory(wp.wrt, &memSize, 0);
        if (mem && memSize >= memorySnapshot.size()) {
            memcpy(mem, memorySnapshot.data(), memorySnapshot.size());
        }
    }

    return wp;
}

void Runtime::linkSyscalls(IM3Module module) {
    // Link wasm3's built-in WASI implementation (all 45 syscalls)
    m3_LinkWASI(module);

    // Set up WASI args context from command line
    m3_wasi_context_t* wasiCtx = m3_GetWasiContext();
    if (wasiCtx && _argc > 0 && _argv) {
        wasiCtx->argc = _argc;
        wasiCtx->argv = (const char**)_argv;
    }

    // Stub any WASI imports that wasm3 doesn't implement
    const char* wasi = "wasi_snapshot_preview1";
    auto stubWasi = [&](const char* name, const char* sig) {
        m3_LinkRawFunction(module, wasi, name, sig, wasi_stub_enosys);
    };
    stubWasi("fd_advise", "i(iIIi)");
    stubWasi("fd_allocate", "i(iII)");
    stubWasi("fd_datasync", "i(i)");
    stubWasi("fd_fdstat_set_flags", "i(ii)");
    stubWasi("fd_fdstat_set_rights", "i(iII)");
    stubWasi("fd_filestat_get", "i(i*)");
    stubWasi("fd_filestat_set_size", "i(iI)");
    stubWasi("fd_filestat_set_times", "i(iIIi)");
    stubWasi("fd_pread", "i(i*iI*)");
    stubWasi("fd_pwrite", "i(i*iI*)");
    stubWasi("fd_readdir", "i(i*iI*)");
    stubWasi("fd_renumber", "i(ii)");
    stubWasi("fd_sync", "i(i)");
    stubWasi("fd_tell", "i(i*)");
    stubWasi("path_create_directory", "i(i*i)");
    stubWasi("path_filestat_get", "i(ii*i*)");
    stubWasi("path_filestat_set_times", "i(ii*iIIi)");
    stubWasi("path_link", "i(ii*ii*i)");
    stubWasi("path_readlink", "i(i*i*i*)");
    stubWasi("path_remove_directory", "i(i*i)");
    stubWasi("path_rename", "i(i*ii*i)");
    stubWasi("path_symlink", "i(*ii*i)");
    stubWasi("path_unlink_file", "i(i*i)");
    stubWasi("poll_oneoff", "i(iiii)");
    stubWasi("sched_yield", "i()");
    stubWasi("sock_accept", "i(ii*)");
    stubWasi("sock_recv", "i(i*ii**)");
    stubWasi("sock_send", "i(i*ii*)");
    stubWasi("sock_shutdown", "i(ii)");

    // Link our YOS process management on top
    m3_LinkRawFunction(module, "env", "yos_fork",        "i()",    syscall_fork);
    m3_LinkRawFunction(module, "env", "yos_vfork",       "i()",    syscall_vfork);
    m3_LinkRawFunction(module, "env", "yos_fork_result", "i()",    syscall_fork_result);
    m3_LinkRawFunction(module, "env", "yos_getpid",      "i()",    syscall_getpid);
    m3_LinkRawFunction(module, "env", "yos_getppid",     "i()",    syscall_getppid);
    m3_LinkRawFunction(module, "env", "yos_getpgrp",     "i()",    syscall_getpgrp);
    m3_LinkRawFunction(module, "env", "yos_setpgid",     "i(ii)",  syscall_setpgid);
    m3_LinkRawFunction(module, "env", "yos_setsid",      "i()",    syscall_setsid);
    m3_LinkRawFunction(module, "env", "yos_getsid",      "i(i)",   syscall_getsid);
    m3_LinkRawFunction(module, "env", "yos_exit",        "v(i)",   syscall_exit);
    m3_LinkRawFunction(module, "env", "yos_wait",        "i(i)",   syscall_wait);
    m3_LinkRawFunction(module, "env", "yos_write",       "i(i*i)", syscall_write);
    m3_LinkRawFunction(module, "env", "yos_read",        "i(i*i)", syscall_read);
    m3_LinkRawFunction(module, "env", "yos_exec",        "i(ii)",  syscall_exec);
    m3_LinkRawFunction(module, "env", "yos_spawn",       "i(ii)",  syscall_spawn);
}

Pid Runtime::forkProcess(WasmProcess& parent) {
    uint32_t memSize = 0;
    uint8_t* mem = m3_GetMemory(parent.wrt, &memSize, 0);
    std::vector<uint8_t> snapshot(mem, mem + memSize);

    auto child = _processTable.fork(parent.ctx->process->pid);
    if (!child) return -1;

    child->state = ProcessState::Running;
    Pid childPid = child->pid;

    child->thread = std::thread([this, child, snapshot = std::move(snapshot)]() mutable {
        runProcess(child, _wasmBytes, true, std::move(snapshot));
    });
    child->thread.detach();

    return childPid;
}

Pid Runtime::vforkProcess(WasmProcess& parent) {
    // vfork: no memory copy, parent blocks until child exec/exit
    auto parentProc = parent.ctx->process;
    auto child = _processTable.fork(parentProc->pid);
    if (!child) return -1;

    child->state = ProcessState::Running;
    child->vforkParentPid = parentProc->pid;
    Pid childPid = child->pid;

    // Start child on new thread (no memory snapshot - it will exec anyway)
    child->thread = std::thread([this, child]() {
        runProcess(child, _wasmBytes, true);
    });
    child->thread.detach();

    // Parent blocks until child signals vforkChildDone
    {
        std::unique_lock<std::mutex> lock(parentProc->mutex);
        parentProc->vforkCv.wait(lock, [&parentProc] {
            return parentProc->vforkChildDone;
        });
        parentProc->vforkChildDone = false; // reset for next vfork
    }

    return childPid;
}

void Runtime::vforkChildDone(Pid childPid) {
    auto child = _processTable.get(childPid);
    if (!child || child->vforkParentPid < 0) return;

    auto parent = _processTable.get(child->vforkParentPid);
    if (!parent) return;

    // Signal parent to wake up
    {
        std::lock_guard<std::mutex> lock(parent->mutex);
        parent->vforkChildDone = true;
    }
    parent->vforkCv.notify_one();
}

Pid Runtime::spawnProcess(Pid parentPid, std::string path, std::vector<std::string> argv) {
    // Load the target wasm
    std::vector<uint8_t> wasmBytes;
    if (!loadWasm(path, wasmBytes)) return -1;

    // Create child process in process table
    auto child = _processTable.fork(parentPid);
    if (!child) return -1;

    child->state = ProcessState::Running;
    Pid childPid = child->pid;

    // Run child on new thread with the new wasm binary (no memory snapshot - fresh start)
    child->thread = std::thread([this, child, wasmBytes = std::move(wasmBytes)]() {
        runProcess(child, wasmBytes, false);
    });
    child->thread.detach();

    return childPid;
}

void Runtime::runProcess(std::shared_ptr<Process> proc, std::vector<uint8_t> wasmBytes,
                         bool isChild, std::vector<uint8_t> memorySnapshot) {
    // Loop: run _start, if exec is requested, reload and run again
    while (true) {
        auto wp = createWasmProcess(proc, wasmBytes, memorySnapshot, isChild);
        memorySnapshot.clear(); // only used on first iteration (fork)

        IM3Function startFn = nullptr;
        M3Result result = m3_FindFunction(&startFn, wp.wrt, "_start");
        if (result) {
            fprintf(stderr, "yos[%d]: _start not found: %s, trying main\n", proc->pid, result);
            result = m3_FindFunction(&startFn, wp.wrt, "main");
        }

        if (result) {
            fprintf(stderr, "yos[%d]: no entry point: %s\n", proc->pid, result);
            _processTable.exit(proc->pid, 127);
            delete wp.ctx;
            if (wp.wrt) m3_FreeRuntime(wp.wrt);
            if (wp.env) m3_FreeEnvironment(wp.env);
            return;
        }

        // Try to compile all functions to catch unlinked imports
        // Check for ALL missing imports
        M3Result compileResult = m3_CompileModule(wp.module);
        while (compileResult) {
            fprintf(stderr, "yos[%d]: compile issue: %s\n", proc->pid, compileResult);
            M3ErrorInfo errInfo;
            m3_GetErrorInfo(wp.wrt, &errInfo);
            if (errInfo.message) fprintf(stderr, "  detail: %s\n", errInfo.message);
            break; // m3_CompileModule only reports one at a time
        }
        fprintf(stderr, "yos[%d]: calling _start...\n", proc->pid);
        result = m3_CallV(startFn);
        fprintf(stderr, "yos[%d]: _start returned: %s\n", proc->pid, result ? result : "(ok)");

        // Check if exec was requested
        std::string execPath = wp.ctx->execPath;

        // Cleanup current runtime
        delete wp.ctx;
        if (wp.wrt) m3_FreeRuntime(wp.wrt);
        if (wp.env) m3_FreeEnvironment(wp.env);

        bool isExit = result && (strcmp(result, "yos_exit") == 0 ||
                                  strcmp(result, m3Err_trapExit) == 0);
        bool isExec = result && strcmp(result, "yos_exec") == 0;

        if (result) {
            // Print backtrace for unexpected errors
            IM3BacktraceInfo bt = m3_GetBacktrace(wp.wrt);
            if (bt) {
                fprintf(stderr, "yos[%d]: backtrace:\n", proc->pid);
                for (IM3BacktraceFrame f = bt->frames; f; f = f->next) {
                    const char* name = m3_GetFunctionName(f->function);
                    fprintf(stderr, "  %s (offset %u)\n", name ? name : "?", f->moduleOffset);
                }
            }
        }

        if (isExec && !execPath.empty()) {
            // exec: load new wasm and loop
            std::vector<uint8_t> newWasm;
            if (!loadWasm(execPath, newWasm)) {
                fprintf(stderr, "yos[%d]: exec failed: cannot load %s\n", proc->pid, execPath.c_str());
                _processTable.exit(proc->pid, 126);
                return;
            }
            wasmBytes = std::move(newWasm);
            isChild = false; // after exec, not a "forked child" anymore
            continue;
        }

        if (result && !isExit) {
            fprintf(stderr, "yos[%d]: runtime error: %s\n", proc->pid, result);
            _processTable.exit(proc->pid, 1);
        } else if (isExit) {
            // Get exit code from WASI context
            m3_wasi_context_t* wasiCtx = m3_GetWasiContext();
            int code = wasiCtx ? wasiCtx->exit_code : 0;
            _processTable.exit(proc->pid, code);
        } else if (!result) {
            _processTable.exit(proc->pid, 0);
        }
        return;
    }
}

} // namespace yos
