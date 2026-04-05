#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <unordered_map>
#include <vector>

namespace yos {

using Pid = int32_t;

enum class ProcessState {
    Ready,
    Running,
    Waiting,   // waiting on waitpid
    Exited
};

struct Process {
    Pid pid = -1;
    Pid parentPid = -1;
    Pid pgid = -1;   // process group ID
    Pid sid = -1;    // session ID
    ProcessState state = ProcessState::Ready;
    int32_t exitCode = 0;

    // WASM linear memory snapshot (copied on fork)
    std::vector<uint8_t> memory;

    // WASM globals snapshot
    std::vector<uint8_t> globals;

    // Host thread running this process
    std::thread thread;

    // Signaling for wait/exit
    std::mutex mutex;
    std::condition_variable cv;
    bool exited = false;

    // vfork: parent blocks until child exec/exit
    Pid vforkParentPid = -1;  // if >0, this is a vfork child; signal parent on exec/exit
    std::condition_variable vforkCv;
    bool vforkChildDone = false;
};

class ProcessTable {
public:
    ProcessTable();

    // Create the init process (pid 1)
    std::shared_ptr<Process> createInit();

    // Fork: create child as copy of parent
    std::shared_ptr<Process> fork(Pid parentPid);

    // Look up process by pid
    std::shared_ptr<Process> get(Pid pid);

    // Mark process as exited
    void exit(Pid pid, int32_t code);

    // Wait for child to exit, returns exit code. -1 if no such child.
    int32_t wait(Pid parentPid, Pid childPid);

    // Get list of all processes (for ps command)
    struct ProcInfo {
        Pid pid;
        Pid ppid;
        Pid pgid;
        ProcessState state;
    };
    std::vector<ProcInfo> listProcesses();

    Pid nextPid();

private:
    std::mutex _mutex;
    Pid _nextPid = 1;
    std::unordered_map<Pid, std::shared_ptr<Process>> _processes;
};

} // namespace yos
