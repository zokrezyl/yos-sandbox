#include "procfs.hpp"
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <regex>

static bool debugEnabled() {
    static int enabled = -1;
    if (enabled < 0) enabled = (getenv("YOS_DEBUG") != nullptr);
    return enabled;
}
#define PROCFS_DBG(fmt, ...) do { if (debugEnabled()) fprintf(stderr, "procfs: " fmt, ##__VA_ARGS__); } while(0)

namespace yos {

VirtualProcFS::VirtualProcFS(ProcessTable& procTable)
    : _procTable(procTable)
{}

int32_t VirtualProcFS::openPath(const std::string& path, Pid callerPid) {
    PROCFS_DBG("openPath: '%s' (caller pid %d)\n", path.c_str(), callerPid);

    // Parse /proc paths
    // /proc/[pid]/stat
    // /proc/[pid]/cmdline
    // /proc (directory listing - special)
    // /proc/self/... (caller's own pid)

    if (path.substr(0, 5) != "/proc") {
        PROCFS_DBG("  not a /proc path\n");
        return -1;  // not a /proc path
    }

    std::lock_guard<std::mutex> lock(_mutex);

    VirtualFd vfd;
    vfd.readPos = 0;

    // Handle /proc/self
    std::string normalizedPath = path;
    size_t selfPos = path.find("/proc/self");
    if (selfPos != std::string::npos) {
        normalizedPath = "/proc/" + std::to_string(callerPid) + path.substr(selfPos + 10);
    }

    // /proc/[pid]/stat
    std::regex statRegex("^/proc/(\\d+)/stat$");
    std::smatch match;
    if (std::regex_match(normalizedPath, match, statRegex)) {
        Pid pid = std::stoi(match[1]);
        auto proc = _procTable.get(pid);
        if (!proc) {
            PROCFS_DBG("  pid %d not found\n", pid);
            return -1;  // process doesn't exist
        }

        vfd.type = VirtualFd::ProcStat;
        vfd.targetPid = pid;
        vfd.content = generateStat(pid);

        int32_t fd = _nextVfd++;
        _vfds[fd] = vfd;
        PROCFS_DBG("  opened /proc/%d/stat as fd %d, content='%s'\n", pid, fd, vfd.content.c_str());
        return fd;
    }

    // /proc/[pid]/cmdline
    std::regex cmdlineRegex("^/proc/(\\d+)/cmdline$");
    if (std::regex_match(normalizedPath, match, cmdlineRegex)) {
        Pid pid = std::stoi(match[1]);
        auto proc = _procTable.get(pid);
        if (!proc) return -1;

        vfd.type = VirtualFd::ProcCmdline;
        vfd.targetPid = pid;
        vfd.content = generateCmdline(pid);

        int32_t fd = _nextVfd++;
        _vfds[fd] = vfd;
        return fd;
    }

    // /proc/uptime
    if (normalizedPath == "/proc/uptime") {
        vfd.type = VirtualFd::ProcStat;  // reuse type for generic files
        vfd.targetPid = -1;
        // uptime format: uptime_seconds idle_seconds
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        char buf[64];
        snprintf(buf, sizeof(buf), "%ld.%02ld 0.00\n", now.tv_sec, now.tv_nsec / 10000000);
        vfd.content = buf;

        int32_t fd = _nextVfd++;
        _vfds[fd] = vfd;
        PROCFS_DBG("  opened /proc/uptime as fd %d, content='%s'\n", fd, vfd.content.c_str());
        return fd;
    }

    // /proc/[pid] (directory)
    std::regex pidDirRegex("^/proc/(\\d+)/?$");
    if (std::regex_match(normalizedPath, match, pidDirRegex)) {
        Pid pid = std::stoi(match[1]);
        auto proc = _procTable.get(pid);
        if (!proc) return -1;

        vfd.type = VirtualFd::ProcPidDir;
        vfd.targetPid = pid;
        vfd.dirIndex = 0;

        int32_t fd = _nextVfd++;
        _vfds[fd] = vfd;
        return fd;
    }

    // /proc (root directory)
    if (normalizedPath == "/proc" || normalizedPath == "/proc/") {
        vfd.type = VirtualFd::ProcDir;
        vfd.targetPid = -1;
        vfd.dirIndex = 0;

        int32_t fd = _nextVfd++;
        _vfds[fd] = vfd;
        PROCFS_DBG("  opened /proc directory as fd %d\n", fd);
        return fd;
    }

    return -1;  // unknown /proc path
}

int32_t VirtualProcFS::read(int32_t fd, uint8_t* buf, int32_t len) {
    std::lock_guard<std::mutex> lock(_mutex);

    auto it = _vfds.find(fd);
    if (it == _vfds.end()) {
        return -1;  // not a virtual fd
    }

    VirtualFd& vfd = it->second;
    if (vfd.readPos >= vfd.content.size()) {
        return 0;  // EOF
    }

    size_t available = vfd.content.size() - vfd.readPos;
    size_t toRead = (len < (int32_t)available) ? len : available;
    memcpy(buf, vfd.content.data() + vfd.readPos, toRead);
    vfd.readPos += toRead;
    return static_cast<int32_t>(toRead);
}

bool VirtualProcFS::close(int32_t fd) {
    std::lock_guard<std::mutex> lock(_mutex);
    return _vfds.erase(fd) > 0;
}

bool VirtualProcFS::isVirtualFd(int32_t fd) const {
    std::lock_guard<std::mutex> lock(_mutex);
    return _vfds.count(fd) > 0;
}

std::vector<Pid> VirtualProcFS::listPids() {
    std::vector<Pid> pids;
    // Get all processes from process table
    // We need access to all pids - this uses the same data structures as fork
    auto procs = _procTable.listProcesses();
    for (const auto& p : procs) {
        pids.push_back(p.pid);
    }
    return pids;
}

std::string VirtualProcFS::generateStat(Pid pid) {
    auto proc = _procTable.get(pid);
    if (!proc) return "";

    // Linux /proc/[pid]/stat format:
    // pid (comm) state ppid pgrp session tty tpgid flags minflt cminflt majflt cmajflt
    // utime stime cutime cstime priority nice num_threads itrealvalue starttime vsize rss ...

    char stateChar;
    switch (proc->state) {
        case ProcessState::Running: stateChar = 'R'; break;
        case ProcessState::Ready:   stateChar = 'S'; break;  // sleeping/waiting to run
        case ProcessState::Waiting: stateChar = 'S'; break;  // waiting on waitpid
        case ProcessState::Exited:  stateChar = 'Z'; break;  // zombie
        default: stateChar = '?';
    }

    // Generate minimal stat line with required fields
    // Most values are 0 since we don't track them
    char buf[512];
    snprintf(buf, sizeof(buf),
        "%d (wasm) %c %d %d %d 0 0 0 0 0 0 0 0 0 0 0 1 0 0 0 0\n",
        proc->pid,
        stateChar,
        proc->parentPid,
        proc->pgid,
        proc->sid
    );

    return std::string(buf);
}

std::string VirtualProcFS::generateCmdline(Pid pid) {
    auto proc = _procTable.get(pid);
    if (!proc) return "";

    // cmdline is NUL-separated arguments
    // For now, just return "wasm" since we don't track argv per-process
    return std::string("wasm\0", 5);
}

int32_t VirtualProcFS::readdir(int32_t fd, uint8_t* buf, int32_t bufLen, uint64_t cookie, uint32_t* bufUsed) {
    PROCFS_DBG("readdir: fd=%d bufLen=%d cookie=%llu\n", fd, bufLen, (unsigned long long)cookie);

    std::lock_guard<std::mutex> lock(_mutex);

    auto it = _vfds.find(fd);
    if (it == _vfds.end()) {
        PROCFS_DBG("  fd not found in virtual fds\n");
        return -1;  // not a virtual fd
    }

    VirtualFd& vfd = it->second;
    *bufUsed = 0;

    // dirent structure (24 bytes header + name):
    // d_next: u64 (cookie for next entry)
    // d_ino: u64 (inode)
    // d_namlen: u32 (name length)
    // d_type: u8 (file type: 3=directory, 4=regular)
    // followed by name bytes (no null terminator needed in buffer)

    if (vfd.type == VirtualFd::ProcDir) {
        // Listing /proc - return pid directories
        auto pids = listPids();
        PROCFS_DBG("  ProcDir: %zu pids available\n", pids.size());
        size_t idx = static_cast<size_t>(cookie);

        while (idx < pids.size() && *bufUsed + 24 < (uint32_t)bufLen) {
            Pid pid = pids[idx];
            std::string name = std::to_string(pid);
            size_t entrySize = 24 + name.size();

            if (*bufUsed + entrySize > (uint32_t)bufLen) break;

            uint8_t* p = buf + *bufUsed;

            // d_next (next cookie = idx + 1)
            uint64_t nextCookie = idx + 1;
            memcpy(p, &nextCookie, 8);

            // d_ino (use pid as inode)
            uint64_t ino = pid;
            memcpy(p + 8, &ino, 8);

            // d_namlen
            uint32_t namlen = static_cast<uint32_t>(name.size());
            memcpy(p + 16, &namlen, 4);

            // d_type (3 = directory)
            p[20] = 3;

            // name
            memcpy(p + 24, name.data(), name.size());

            PROCFS_DBG("  entry: name='%s' d_next=%llu d_ino=%llu d_namlen=%u d_type=%d entrySize=%zu\n",
                       name.c_str(), (unsigned long long)nextCookie, (unsigned long long)ino, namlen, 3, entrySize);
            *bufUsed += static_cast<uint32_t>(entrySize);
            idx++;
        }
        PROCFS_DBG("  ProcDir: returned %u bytes, idx now %zu\n", *bufUsed, idx);
        return 0;
    }
    else if (vfd.type == VirtualFd::ProcPidDir) {
        // Listing /proc/[pid] - return stat, cmdline, etc.
        static const char* entries[] = {"stat", "cmdline", "status"};
        static const size_t numEntries = 3;
        size_t idx = static_cast<size_t>(cookie);

        while (idx < numEntries && *bufUsed + 24 < (uint32_t)bufLen) {
            const char* name = entries[idx];
            size_t nameLen = strlen(name);
            size_t entrySize = 24 + nameLen;

            if (*bufUsed + entrySize > (uint32_t)bufLen) break;

            uint8_t* p = buf + *bufUsed;

            // d_next
            uint64_t nextCookie = idx + 1;
            memcpy(p, &nextCookie, 8);

            // d_ino
            uint64_t ino = vfd.targetPid * 100 + idx;
            memcpy(p + 8, &ino, 8);

            // d_namlen
            uint32_t namlen = static_cast<uint32_t>(nameLen);
            memcpy(p + 16, &namlen, 4);

            // d_type (4 = regular file)
            p[20] = 4;

            // name
            memcpy(p + 24, name, nameLen);

            *bufUsed += static_cast<uint32_t>(entrySize);
            idx++;
        }
        return 0;
    }

    return -1;  // not a directory fd
}

} // namespace yos
