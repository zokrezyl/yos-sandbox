#include "process.hpp"

namespace yos {

ProcessTable::ProcessTable() = default;

std::shared_ptr<Process> ProcessTable::createInit() {
    std::lock_guard lock(_mutex);
    auto proc = std::make_shared<Process>();
    proc->pid = _nextPid++;
    proc->parentPid = 0;
    proc->pgid = proc->pid;  // init is its own process group leader
    proc->sid = proc->pid;   // init is its own session leader
    proc->state = ProcessState::Running;
    _processes[proc->pid] = proc;
    return proc;
}

std::shared_ptr<Process> ProcessTable::fork(Pid parentPid) {
    std::lock_guard lock(_mutex);

    auto parentIt = _processes.find(parentPid);
    if (parentIt == _processes.end()) return nullptr;

    auto& parent = parentIt->second;
    auto child = std::make_shared<Process>();
    child->pid = _nextPid++;
    child->parentPid = parentPid;
    child->pgid = parent->pgid;  // inherit process group
    child->sid = parent->sid;    // inherit session
    child->state = ProcessState::Ready;

    // Eager copy of parent's memory
    child->memory = parent->memory;

    _processes[child->pid] = child;
    return child;
}

std::shared_ptr<Process> ProcessTable::get(Pid pid) {
    std::lock_guard lock(_mutex);
    auto it = _processes.find(pid);
    if (it == _processes.end()) return nullptr;
    return it->second;
}

void ProcessTable::exit(Pid pid, int32_t code) {
    std::shared_ptr<Process> proc;
    {
        std::lock_guard lock(_mutex);
        auto it = _processes.find(pid);
        if (it == _processes.end()) return;
        proc = it->second;
    }

    std::lock_guard procLock(proc->mutex);
    proc->state = ProcessState::Exited;
    proc->exitCode = code;
    proc->exited = true;
    proc->cv.notify_all();
}

int32_t ProcessTable::wait(Pid parentPid, Pid childPid) {
    std::shared_ptr<Process> child;
    {
        std::lock_guard lock(_mutex);
        auto it = _processes.find(childPid);
        if (it == _processes.end()) return -1;
        child = it->second;
        if (child->parentPid != parentPid) return -1;
    }

    std::unique_lock procLock(child->mutex);
    child->cv.wait(procLock, [&] { return child->exited; });
    return child->exitCode;
}

Pid ProcessTable::nextPid() {
    std::lock_guard lock(_mutex);
    return _nextPid;
}

std::vector<ProcessTable::ProcInfo> ProcessTable::listProcesses() {
    std::lock_guard lock(_mutex);
    std::vector<ProcInfo> result;
    result.reserve(_processes.size());
    for (const auto& [pid, proc] : _processes) {
        result.push_back({
            proc->pid,
            proc->parentPid,
            proc->pgid,
            proc->state
        });
    }
    return result;
}

} // namespace yos
