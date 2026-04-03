#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>
#include <map>
#include <memory>
#include <vector>
#include <sys/stat.h>
#include <dirent.h>
#include "wasm-types.hpp"

namespace yos {

// Forward declarations
class VFS;
class Process;

// File descriptor info - tracks which filesystem owns each fd
struct FdInfo {
    int localFd;           // fd within the filesystem
    class Filesystem* fs;  // owning filesystem
    std::string path;      // original path (for debugging)
};

// Abstract filesystem interface
class Filesystem {
public:
    virtual ~Filesystem() = default;

    // File operations - return fd or -errno
    virtual int open(std::string_view path, int flags, int mode) = 0;
    virtual int close(int fd) = 0;
    virtual int read(int fd, void* buf, size_t count) = 0;
    virtual int write(int fd, const void* buf, size_t count) = 0;
    virtual int64_t lseek(int fd, int64_t offset, int whence) = 0;
    virtual int ftruncate(int fd, int64_t length) = 0;
    virtual int fsync(int fd) = 0;
    virtual int dup(int fd) = 0;
    virtual int fcntl(int fd, int cmd, int32_t arg) = 0;

    // Stat operations - return 0 or -errno
    virtual int stat(std::string_view path, struct stat* buf) = 0;
    virtual int lstat(std::string_view path, struct stat* buf) = 0;
    virtual int fstat(int fd, struct stat* buf) = 0;
    virtual int access(std::string_view path, int mode) = 0;
    virtual int chmod(std::string_view path, uint32_t mode) = 0;
    virtual int chown(std::string_view path, uint32_t owner, uint32_t group) = 0;
    virtual int fchmod(int fd, uint32_t mode) = 0;
    virtual int fchown(int fd, uint32_t owner, uint32_t group) = 0;

    // Directory operations
    virtual int mkdir(std::string_view path, uint32_t mode) = 0;
    virtual int rmdir(std::string_view path) = 0;
    virtual int getdents(int fd, void* buf, size_t count) = 0;

    // Link operations
    virtual int link(std::string_view oldpath, std::string_view newpath) = 0;
    virtual int unlink(std::string_view path) = 0;
    virtual int symlink(std::string_view target, std::string_view linkpath) = 0;
    virtual int readlink(std::string_view path, char* buf, size_t bufsiz) = 0;
    virtual int rename(std::string_view oldpath, std::string_view newpath) = 0;

    // Misc
    virtual int ioctl(int fd, uint32_t request, void* arg) = 0;
    virtual int isatty(int fd) = 0;

    // Pipe
    virtual int pipe(int pipefd[2]) = 0;

    // Directory iteration (higher level)
    virtual void* opendir(std::string_view path) = 0;
    virtual void* readdir(void* dir) = 0;
    virtual int closedir(void* dir) = 0;
};

// VFS - manages mount points and file descriptors
class VFS {
public:
    VFS();
    ~VFS();

    // Mount/unmount filesystems
    void mount(std::string_view path, std::shared_ptr<Filesystem> fs);
    void umount(std::string_view path);

    // Initialize stdin/stdout/stderr to use given filesystem
    void initStdio(Filesystem* fs);

    // Set current working directory
    int chdir(std::string_view path);
    int fchdir(int fd);
    int getcwd(char* buf, size_t size);

    // File operations - these resolve mount points and manage global fds
    int open(std::string_view path, int flags, int mode);
    int close(int fd);
    int read(int fd, void* buf, size_t count);
    int write(int fd, const void* buf, size_t count);
    int64_t lseek(int fd, int64_t offset, int whence);
    int ftruncate(int fd, int64_t length);
    int fsync(int fd);
    int dup(int oldfd);
    int dup2(int oldfd, int newfd);
    int fcntl(int fd, int cmd, int32_t arg);

    // Stat operations (host stat - 144 bytes on 64-bit)
    int stat(std::string_view path, struct stat* buf);
    int lstat(std::string_view path, struct stat* buf);
    int fstat(int fd, struct stat* buf);

    // WASM stat operations (wasm_stat - 64 bytes)
    int stat_wasm(std::string_view path, struct wasm_stat* buf);
    int lstat_wasm(std::string_view path, struct wasm_stat* buf);
    int fstat_wasm(int fd, struct wasm_stat* buf);
    int access(std::string_view path, int mode);
    int chmod(std::string_view path, uint32_t mode);
    int chown(std::string_view path, uint32_t owner, uint32_t group);
    int fchmod(int fd, uint32_t mode);
    int fchown(int fd, uint32_t owner, uint32_t group);
    uint32_t umask(uint32_t mask);

    // Directory operations
    int mkdir(std::string_view path, uint32_t mode);
    int rmdir(std::string_view path);
    int getdents(int fd, void* buf, size_t count);

    // Link operations
    int link(std::string_view oldpath, std::string_view newpath);
    int unlink(std::string_view path);
    int symlink(std::string_view target, std::string_view linkpath);
    int readlink(std::string_view path, char* buf, size_t bufsiz);
    int rename(std::string_view oldpath, std::string_view newpath);

    // Misc
    int ioctl(int fd, uint32_t request, void* arg);
    int isatty(int fd);

    // Pipe
    int pipe(int pipefd[2]);
    int pipe2(int pipefd[2], int flags);

    // opendir/readdir/closedir - higher level wrappers
    void* opendir(std::string_view path);
    void* readdir(void* dir, struct dirent* entry);
    void* readdir_wasm(void* dir, struct wasm_dirent* entry);  // WASM version
    int closedir(void* dir);

private:
    // Resolve path to filesystem and relative path
    struct ResolvedPath {
        Filesystem* fs;
        std::string relativePath;
    };
    ResolvedPath resolve(std::string_view path);

    // Make path absolute using cwd
    std::string makeAbsolute(std::string_view path);

    // Get fd info
    FdInfo* getFd(int fd);

    // Allocate new global fd
    int allocFd(int localFd, Filesystem* fs, std::string_view path);

    // Mount points: sorted by path length (longest first for matching)
    std::map<std::string, std::shared_ptr<Filesystem>, std::greater<>> _mounts;

    // File descriptor table: global fd -> FdInfo
    std::vector<FdInfo> _fdTable;

    // Current working directory
    std::string _cwd;

    // umask
    uint32_t _umask;

    // Next fd to allocate
    int _nextFd;

    // DIR* handle table: index -> DIR*
    std::vector<DIR*> _dirHandles;
};

} // namespace yos
