#include "runtime.hpp"
#include "debug.hpp"
#include "hostfs.hpp"
#include "yos-link-generated.hpp"
#include "yos-varargs-generated.hpp"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <thread>
#include <sys/utsname.h>

namespace yos {

Runtime::Runtime()
    : _procfs(_processTable)
{}
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
    wp.ctx = new ProcessContext{};
    wp.ctx->runtime = this;
    wp.ctx->process = proc;
    wp.ctx->isChild = isChild;

    // Create VFS for this process
    wp.ctx->vfs = new VFS();

    // Mount root filesystem (sandbox directory or real /)
    auto hostfs = std::make_shared<HostFS>("/");
    wp.ctx->vfs->mount("/", hostfs);
    wp.ctx->vfs->initStdio(hostfs.get());

    wp.env = m3_NewEnvironment();
    wp.wrt = m3_NewRuntime(wp.env, 64 * 1024, wp.ctx);
    wp.ctx->wrt = wp.wrt;  // Store for syscall access

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
    // Link all generated yos syscalls
    linkGeneratedSyscalls(module);
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
            YOS_DBG("yos[%d]: _start not found: %s, trying main\n", proc->pid, result);
            result = m3_FindFunction(&startFn, wp.wrt, "main");
        }
        if (result) {
            YOS_DBG("yos[%d]: main not found: %s, trying __main_argc_argv\n", proc->pid, result);
            result = m3_FindFunction(&startFn, wp.wrt, "__main_argc_argv");
        }

        if (result) {
            YOS_DBG("yos[%d]: no entry point: %s\n", proc->pid, result);
            _processTable.exit(proc->pid, 127);
            delete wp.ctx;
            if (wp.wrt) m3_FreeRuntime(wp.wrt);
            if (wp.env) m3_FreeEnvironment(wp.env);
            return;
        }

        // Try to compile all functions to catch unlinked imports
        // Check for ALL missing imports
        YOS_DBG("BEFORE m3_CompileModule\n");
        M3Result compileResult = m3_CompileModule(wp.module);
        YOS_DBG("AFTER m3_CompileModule\n");
        while (compileResult) {
            YOS_DBG("yos[%d]: compile issue: %s\n", proc->pid, compileResult);
            M3ErrorInfo errInfo;
            m3_GetErrorInfo(wp.wrt, &errInfo);
            if (errInfo.message) YOS_DBG("  detail: %s\n", errInfo.message);
            break; // m3_CompileModule only reports one at a time
        }

        // Check entry point name to determine how to call it
        const char* entryName = m3_GetFunctionName(startFn);
        bool needsArgv = entryName && (strcmp(entryName, "__main_argc_argv") == 0 ||
                                       strcmp(entryName, "main") == 0);

        if (needsArgv && _argc > 0 && _argv) {
            // Set up argc/argv in WASM memory
            // Layout at end of memory: argv[0..argc], then strings
            uint32_t memSize = 0;
            uint8_t* mem = m3_GetMemory(wp.wrt, &memSize, 0);

            // Calculate total size needed
            size_t stringsSize = 0;
            for (int i = 0; i < _argc; i++) {
                stringsSize += strlen(_argv[i]) + 1;
            }
            size_t argvSize = (_argc + 1) * sizeof(uint32_t);
            size_t totalSize = argvSize + stringsSize;

            // Place at end of current memory, 16-byte aligned
            uint32_t argvPtr = (memSize - totalSize) & ~15;
            uint32_t strPtr = argvPtr + argvSize;

            // Write argv array and strings
            uint32_t* argvArray = (uint32_t*)(mem + argvPtr);
            for (int i = 0; i < _argc; i++) {
                argvArray[i] = strPtr;
                size_t len = strlen(_argv[i]) + 1;
                memcpy(mem + strPtr, _argv[i], len);
                strPtr += len;
            }
            argvArray[_argc] = 0;  // NULL terminator

            YOS_DBG("yos[%d]: calling %s(argc=%d, argv=%u)...\n", proc->pid, entryName, _argc, argvPtr);
            result = m3_CallV(startFn, _argc, argvPtr);
        } else {
            YOS_DBG("yos[%d]: calling _start...\n", proc->pid);
            result = m3_CallV(startFn);
        }
        YOS_DBG("yos[%d]: _start returned: %s\n", proc->pid, result ? result : "(ok)");

        // Print backtrace BEFORE cleanup for debugging
        if (result && strstr(result, "unreachable")) {
            IM3BacktraceInfo bt = m3_GetBacktrace(wp.wrt);
            YOS_DBG("yos[%d]: BACKTRACE (bt=%p):\n", proc->pid, (void*)bt);
            if (bt && bt->frames) {
                for (IM3BacktraceFrame f = bt->frames; f; f = f->next) {
                    const char* name = m3_GetFunctionName(f->function);
                    YOS_DBG("  %s (offset %u)\n", name ? name : "?", f->moduleOffset);
                }
            }
        }

        // Check if exec was requested
        std::string execPath = wp.ctx->execPath;

        // Cleanup current runtime
        if (wp.ctx->vfs) delete wp.ctx->vfs;
        delete wp.ctx;
        if (wp.wrt) m3_FreeRuntime(wp.wrt);
        if (wp.env) m3_FreeEnvironment(wp.env);

        bool isExit = result && (strcmp(result, "exit") == 0 ||
                                  strcmp(result, "_exit") == 0 ||
                                  strcmp(result, m3Err_trapExit) == 0);
        bool isExec = result && strcmp(result, "yos_exec") == 0;

        if (isExec && !execPath.empty()) {
            // exec: load new wasm and loop
            std::vector<uint8_t> newWasm;
            if (!loadWasm(execPath, newWasm)) {
                YOS_DBG("yos[%d]: exec failed: cannot load %s\n", proc->pid, execPath.c_str());
                _processTable.exit(proc->pid, 126);
                return;
            }
            wasmBytes = std::move(newWasm);
            isChild = false; // after exec, not a "forked child" anymore
            continue;
        }

        if (result && !isExit) {
            YOS_DBG("yos[%d]: runtime error: %s\n", proc->pid, result);
            _processTable.exit(proc->pid, 1);
        } else if (isExit) {
            // Exit code already recorded by our exit syscall handler
            // Process already marked as exited
        } else if (!result) {
            _processTable.exit(proc->pid, 0);
        }
        return;
    }
}

// Syscall implementations for generated handlers

Pid Runtime::fork(ProcessContext* ctx) {
    WasmProcess wp;
    wp.wrt = ctx->wrt;
    wp.ctx = ctx;
    return forkProcess(wp);
}

Pid Runtime::vfork(ProcessContext* ctx) {
    WasmProcess wp;
    wp.wrt = ctx->wrt;
    wp.ctx = ctx;
    return vforkProcess(wp);
}

int Runtime::execve(ProcessContext* ctx, const char* path, void* argv, void* envp) {
    // Set exec path in context - runProcess will pick it up
    ctx->execPath = path;
    // TODO: Parse argv/envp from wasm memory
    return 0;  // exec triggers trap
}

void Runtime::exit(ProcessContext* ctx, int status) {
    _processTable.exit(ctx->process->pid, status);
}

void Runtime::_exit(ProcessContext* ctx, int status) {
    _processTable.exit(ctx->process->pid, status);
}

Pid Runtime::getpid(ProcessContext* ctx) {
    return ctx->process->pid;
}

Pid Runtime::getppid(ProcessContext* ctx) {
    return ctx->process->parentPid;
}

Pid Runtime::getpgrp(ProcessContext* ctx) {
    return ctx->process->pgid;
}

int Runtime::setpgid(ProcessContext* ctx, Pid pid, Pid pgid) {
    Pid targetPid = (pid == 0) ? ctx->process->pid : pid;
    Pid newPgid = (pgid == 0) ? targetPid : pgid;

    auto proc = _processTable.get(targetPid);
    if (!proc) return -ESRCH;

    proc->pgid = newPgid;
    return 0;
}

Pid Runtime::setsid(ProcessContext* ctx) {
    // Create new session
    ctx->process->sid = ctx->process->pid;
    ctx->process->pgid = ctx->process->pid;
    return ctx->process->pid;
}

Pid Runtime::getsid(ProcessContext* ctx, Pid pid) {
    Pid targetPid = (pid == 0) ? ctx->process->pid : pid;
    auto proc = _processTable.get(targetPid);
    if (!proc) return -ESRCH;
    return proc->sid;
}

Pid Runtime::wait(ProcessContext* ctx, void* status) {
    return waitpid(ctx, -1, status, 0);
}

Pid Runtime::waitpid(ProcessContext* ctx, Pid pid, void* status, int options) {
    // Use existing ProcessTable::wait
    if (pid > 0) {
        int32_t exitCode = _processTable.wait(ctx->process->pid, pid);
        if (exitCode >= 0 && status) {
            *static_cast<int*>(status) = (exitCode & 0xff) << 8;
        }
        return exitCode >= 0 ? pid : -ECHILD;
    }

    // pid == -1: wait for any child
    // For now, simplified - just check all processes
    auto procs = _processTable.listProcesses();
    for (auto& info : procs) {
        if (info.ppid == ctx->process->pid && info.state == ProcessState::Exited) {
            int32_t exitCode = _processTable.wait(ctx->process->pid, info.pid);
            if (exitCode >= 0 && status) {
                *static_cast<int*>(status) = (exitCode & 0xff) << 8;
            }
            return info.pid;
        }
    }

    if (options & 1) {  // WNOHANG
        return 0;
    }

    return -ECHILD;
}

int Runtime::uname(ProcessContext* ctx, void* buf) {
    struct utsname* u = static_cast<struct utsname*>(buf);
    strncpy(u->sysname, "YOS", sizeof(u->sysname));
    strncpy(u->nodename, "localhost", sizeof(u->nodename));
    strncpy(u->release, "1.0.0", sizeof(u->release));
    strncpy(u->version, "YOS Sandbox", sizeof(u->version));
    strncpy(u->machine, "wasm32", sizeof(u->machine));
    return 0;
}

// Varargs constants and VarArgPack are now in yos-varargs-generated.hpp

int Runtime::varargs_call(ProcessContext* ctx, int func_id, void* arg1, void* arg2, void* arg3, void* args_ptr) {
    auto* pack = static_cast<VarArgPack*>(args_ptr);

    // Build argument array for formatting
    // We'll use snprintf with reconstructed format
    char buf[4096];
    char* out = buf;
    char* end = buf + sizeof(buf);

    const char* fmt = nullptr;
    FILE* stream = nullptr;
    char* str_out = nullptr;
    size_t str_size = 0;
    int fd = 1;

    switch (func_id) {
        case VFUNC_PRINTF:
            fmt = static_cast<const char*>(arg1);
            stream = stdout;
            break;
        case VFUNC_FPRINTF:
            stream = static_cast<FILE*>(arg1);
            fmt = static_cast<const char*>(arg2);
            break;
        case VFUNC_SPRINTF:
            str_out = static_cast<char*>(arg1);
            fmt = static_cast<const char*>(arg2);
            str_size = SIZE_MAX;
            break;
        case VFUNC_SNPRINTF:
            str_out = static_cast<char*>(arg1);
            str_size = reinterpret_cast<size_t>(arg2);
            fmt = static_cast<const char*>(arg3);
            break;
        case VFUNC_DPRINTF:
            fd = static_cast<int>(reinterpret_cast<intptr_t>(arg1));
            fmt = static_cast<const char*>(arg2);
            break;
        case VFUNC_VASPRINTF:
            str_out = nullptr;  // Will allocate
            fmt = static_cast<const char*>(arg2);
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

        // Found %, scan the format specifier
        const char* spec_start = p;
        p++; // skip %

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

        // Get conversion char
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
                    n = snprintf(out, end - out, spec, (const char*)pack->values[arg_idx]);
                    break;
                case VARG_PTR:
                    n = snprintf(out, end - out, spec, (void*)pack->values[arg_idx]);
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
    }
    *out = '\0';

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
        case VFUNC_VASPRINTF: {
            // arg1 is char** strp - allocate and copy
            char** strp = static_cast<char**>(arg1);
            if (strp) {
                *strp = strdup(buf);
                if (!*strp) result = -1;
            }
            break;
        }
    }

    return result;
}

} // namespace yos
