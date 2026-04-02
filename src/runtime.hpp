#pragma once

#include "process.hpp"
#include "procfs.hpp"
#include "vfs.hpp"
#include "wasm3.h"

#include <string>
#include <vector>

namespace yos {

class Runtime;

// Stored as wasm3 runtime userdata - one per wasm instance
struct ProcessContext {
    Runtime* runtime = nullptr;
    std::shared_ptr<Process> process;
    VFS* vfs = nullptr;  // Virtual filesystem for this process
    IM3Runtime wrt = nullptr;  // wasm3 runtime for memory access
    bool isChild = false;

    // Set by yos_exec syscall - signals runProcess to reload
    std::string execPath;
    std::vector<std::string> execArgv;

    // Heap management for sbrk
    uint32_t heapEnd = 0;     // Current program break (0 = uninitialized)
};

// Per-process WASM runtime context
struct WasmProcess {
    IM3Environment env = nullptr;
    IM3Runtime wrt = nullptr;
    IM3Module module = nullptr;
    ProcessContext* ctx = nullptr;

    std::vector<uint8_t> wasmBytes;
};

class Runtime {
public:
    Runtime();
    ~Runtime();

    int run(std::string_view wasmPath, int argc = 0, char** argv = nullptr);

    Pid forkProcess(WasmProcess& parent);

    // vfork: parent blocks until child calls exec/exit
    Pid vforkProcess(WasmProcess& parent);

    // Signal that vfork child has exec'd or exited - unblocks parent
    void vforkChildDone(Pid childPid);

    // Spawn: fork+exec in one step. Creates child running a different wasm program.
    Pid spawnProcess(Pid parentPid, std::string path, std::vector<std::string> argv);

    // Load a .wasm file into bytes. Resolves relative to _basePath.
    bool loadWasm(std::string_view path, std::vector<uint8_t>& out);

    ProcessTable& processTable() { return _processTable; }
    VirtualProcFS& procfs() { return _procfs; }

    // Syscall interface (called by generated handlers via ProcessContext)
    Pid fork(ProcessContext* ctx);
    Pid vfork(ProcessContext* ctx);
    int execve(ProcessContext* ctx, const char* path, void* argv, void* envp);
    void exit(ProcessContext* ctx, int status);
    void _exit(ProcessContext* ctx, int status);
    Pid getpid(ProcessContext* ctx);
    Pid getppid(ProcessContext* ctx);
    Pid getpgrp(ProcessContext* ctx);
    int setpgid(ProcessContext* ctx, Pid pid, Pid pgid);
    Pid setsid(ProcessContext* ctx);
    Pid getsid(ProcessContext* ctx, Pid pid);
    Pid wait(ProcessContext* ctx, void* status);
    Pid waitpid(ProcessContext* ctx, Pid pid, void* status, int options);
    int uname(ProcessContext* ctx, void* buf);
    int varargs_call(ProcessContext* ctx, int func_id, void* arg1, void* arg2, void* arg3, void* args);
    void* sbrk(ProcessContext* ctx, int64_t increment);
    int brk(ProcessContext* ctx, void* addr);

private:
    WasmProcess createWasmProcess(
        std::shared_ptr<Process> proc,
        const std::vector<uint8_t>& wasmBytes,
        const std::vector<uint8_t>& memorySnapshot,
        bool isChild
    );

    void linkSyscalls(IM3Module module);
    void runProcess(std::shared_ptr<Process> proc, std::vector<uint8_t> wasmBytes,
                    bool isChild, std::vector<uint8_t> memorySnapshot = {});

    ProcessTable _processTable;
    VirtualProcFS _procfs;
    std::vector<uint8_t> _wasmBytes;
    std::string _basePath; // directory for resolving exec paths

    // Command line args
    int _argc = 0;
    char** _argv = nullptr;
};

} // namespace yos
