#pragma once

#include "process.hpp"
#include <string>
#include <unordered_map>
#include <mutex>
#include <vector>

namespace yos {

// Virtual /proc filesystem for WASM processes
// Provides /proc/[pid]/stat, /proc/[pid]/cmdline, etc.

class ProcessTable;

// Virtual file descriptor for /proc files
struct VirtualFd {
    enum Type { ProcStat, ProcCmdline, ProcDir, ProcPidDir };
    Type type;
    Pid targetPid;
    std::string content;  // pre-generated content
    size_t readPos = 0;   // current read position
    size_t dirIndex = 0;  // for directory reading
};

class VirtualProcFS {
public:
    VirtualProcFS(ProcessTable& procTable);

    // Returns virtual fd (>= 1000) if path is /proc/*, otherwise -1
    int32_t openPath(const std::string& path, Pid callerPid);

    // Returns bytes read, -1 if not a virtual fd
    int32_t read(int32_t fd, uint8_t* buf, int32_t len);

    // Close virtual fd, returns true if it was a virtual fd
    bool close(int32_t fd);

    // Check if fd is a virtual fd
    bool isVirtualFd(int32_t fd) const;

    // List pids in /proc (for readdir)
    std::vector<Pid> listPids();

    // Read directory entries (for fd_readdir)
    // Returns bytes written to buf, sets cookie for next read
    int32_t readdir(int32_t fd, uint8_t* buf, int32_t bufLen, uint64_t cookie, uint32_t* bufUsed);

private:
    ProcessTable& _procTable;
    mutable std::mutex _mutex;
    int32_t _nextVfd = 1000;
    std::unordered_map<int32_t, VirtualFd> _vfds;

    // Generate /proc/[pid]/stat content
    std::string generateStat(Pid pid);

    // Generate /proc/[pid]/cmdline content
    std::string generateCmdline(Pid pid);
};

} // namespace yos
