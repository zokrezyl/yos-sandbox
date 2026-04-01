#pragma once

#include "vfs.hpp"
#include <string>

namespace yos {

// HostFS - passes through to host filesystem with optional root chroot
class HostFS : public Filesystem {
public:
    // Create with a root path that becomes "/" in the virtual fs
    explicit HostFS(std::string_view root = "/");
    ~HostFS() override;

    // File operations
    int open(std::string_view path, int flags, int mode) override;
    int close(int fd) override;
    int read(int fd, void* buf, size_t count) override;
    int write(int fd, const void* buf, size_t count) override;
    int64_t lseek(int fd, int64_t offset, int whence) override;
    int ftruncate(int fd, int64_t length) override;
    int fsync(int fd) override;
    int dup(int fd) override;
    int fcntl(int fd, int cmd, int64_t arg) override;

    // Stat operations
    int stat(std::string_view path, struct stat* buf) override;
    int lstat(std::string_view path, struct stat* buf) override;
    int fstat(int fd, struct stat* buf) override;
    int access(std::string_view path, int mode) override;
    int chmod(std::string_view path, uint32_t mode) override;
    int chown(std::string_view path, uint32_t owner, uint32_t group) override;
    int fchmod(int fd, uint32_t mode) override;
    int fchown(int fd, uint32_t owner, uint32_t group) override;

    // Directory operations
    int mkdir(std::string_view path, uint32_t mode) override;
    int rmdir(std::string_view path) override;
    int getdents(int fd, void* buf, size_t count) override;

    // Link operations
    int link(std::string_view oldpath, std::string_view newpath) override;
    int unlink(std::string_view path) override;
    int symlink(std::string_view target, std::string_view linkpath) override;
    int readlink(std::string_view path, char* buf, size_t bufsiz) override;
    int rename(std::string_view oldpath, std::string_view newpath) override;

    // Misc
    int ioctl(int fd, uint64_t request, void* arg) override;
    int isatty(int fd) override;

    // Pipe
    int pipe(int pipefd[2]) override;

    // Directory iteration
    void* opendir(std::string_view path) override;
    void* readdir(void* dir) override;
    int closedir(void* dir) override;

private:
    std::string toHostPath(std::string_view path);
    std::string _root;
};

} // namespace yos
