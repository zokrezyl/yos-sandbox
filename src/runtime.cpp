#include "runtime.hpp"
#include "syscalls.hpp"

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

int Runtime::run(std::string_view wasmPath) {
    auto absPath = std::filesystem::absolute(std::string(wasmPath));
    _basePath = absPath.parent_path().string();

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
    m3_LinkRawFunction(module, "env", "yos_fork",        "i()",    syscall_fork);
    m3_LinkRawFunction(module, "env", "yos_fork_result", "i()",    syscall_fork_result);
    m3_LinkRawFunction(module, "env", "yos_getpid",      "i()",    syscall_getpid);
    m3_LinkRawFunction(module, "env", "yos_getppid",     "i()",    syscall_getppid);
    m3_LinkRawFunction(module, "env", "yos_exit",        "v(i)",   syscall_exit);
    m3_LinkRawFunction(module, "env", "yos_wait",        "i(i)",   syscall_wait);
    m3_LinkRawFunction(module, "env", "yos_write",       "i(i*i)", syscall_write);
    m3_LinkRawFunction(module, "env", "yos_exec",        "i(*i)",  syscall_exec);
}

Pid Runtime::forkProcess(WasmProcess& parent) {
    uint32_t memSize = 0;
    uint8_t* mem = m3_GetMemory(parent.wrt, &memSize, 0);
    std::vector<uint8_t> snapshot(mem, mem + memSize);

    auto child = _processTable.fork(parent.ctx->process->pid);
    if (!child) return -1;

    child->state = ProcessState::Running;
    Pid childPid = child->pid;

    child->thread = std::thread([this, child, snapshot = std::move(snapshot)]() {
        runProcess(child, _wasmBytes, true);
    });
    child->thread.detach();

    return childPid;
}

void Runtime::runProcess(std::shared_ptr<Process> proc, std::vector<uint8_t> wasmBytes, bool isChild) {
    // Loop: run _start, if exec is requested, reload and run again
    while (true) {
        auto wp = createWasmProcess(proc, wasmBytes, {}, isChild);

        IM3Function startFn = nullptr;
        M3Result result = m3_FindFunction(&startFn, wp.wrt, "_start");
        if (result) {
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

        result = m3_CallV(startFn);

        // Check if exec was requested
        std::string execPath = wp.ctx->execPath;

        // Cleanup current runtime
        delete wp.ctx;
        if (wp.wrt) m3_FreeRuntime(wp.wrt);
        if (wp.env) m3_FreeEnvironment(wp.env);

        if (result && strcmp(result, "yos_exec") == 0 && !execPath.empty()) {
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

        if (result && strcmp(result, "yos_exit") != 0) {
            fprintf(stderr, "yos[%d]: runtime error: %s\n", proc->pid, result);
            _processTable.exit(proc->pid, 1);
        } else if (!result) {
            _processTable.exit(proc->pid, 0);
        }
        return;
    }
}

} // namespace yos
