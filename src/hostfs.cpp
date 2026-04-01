#include "hostfs.hpp"
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <dirent.h>

namespace yos {

HostFS::HostFS(std::string_view root) : _root(root) {
    // Normalize root: ensure no trailing slash (except for "/")
    while (_root.size() > 1 && _root.back() == '/') {
        _root.pop_back();
    }
}

HostFS::~HostFS() = default;

std::string HostFS::toHostPath(std::string_view path) {
    if (_root == "/") return std::string(path);

    std::string result = _root;
    if (!path.empty() && path[0] != '/') {
        result += '/';
    }
    result += path;
    return result;
}

// File operations

int HostFS::open(std::string_view path, int flags, int mode) {
    std::string hostPath = toHostPath(path);
    int fd = ::open(hostPath.c_str(), flags, mode);
    return fd < 0 ? -errno : fd;
}

int HostFS::close(int fd) {
    int r = ::close(fd);
    return r < 0 ? -errno : 0;
}

int HostFS::read(int fd, void* buf, size_t count) {
    ssize_t r = ::read(fd, buf, count);
    return r < 0 ? -errno : static_cast<int>(r);
}

int HostFS::write(int fd, const void* buf, size_t count) {
    ssize_t r = ::write(fd, buf, count);
    return r < 0 ? -errno : static_cast<int>(r);
}

int64_t HostFS::lseek(int fd, int64_t offset, int whence) {
    off_t r = ::lseek(fd, offset, whence);
    return r < 0 ? -errno : r;
}

int HostFS::ftruncate(int fd, int64_t length) {
    int r = ::ftruncate(fd, length);
    return r < 0 ? -errno : 0;
}

int HostFS::fsync(int fd) {
    int r = ::fsync(fd);
    return r < 0 ? -errno : 0;
}

int HostFS::dup(int fd) {
    int r = ::dup(fd);
    return r < 0 ? -errno : r;
}

int HostFS::fcntl(int fd, int cmd, int64_t arg) {
    int r = ::fcntl(fd, cmd, arg);
    return r < 0 ? -errno : r;
}

// Stat operations

int HostFS::stat(std::string_view path, struct stat* buf) {
    std::string hostPath = toHostPath(path);
    int r = ::stat(hostPath.c_str(), buf);
    return r < 0 ? -errno : 0;
}

int HostFS::lstat(std::string_view path, struct stat* buf) {
    std::string hostPath = toHostPath(path);
    int r = ::lstat(hostPath.c_str(), buf);
    return r < 0 ? -errno : 0;
}

int HostFS::fstat(int fd, struct stat* buf) {
    int r = ::fstat(fd, buf);
    return r < 0 ? -errno : 0;
}

int HostFS::access(std::string_view path, int mode) {
    std::string hostPath = toHostPath(path);
    int r = ::access(hostPath.c_str(), mode);
    return r < 0 ? -errno : 0;
}

int HostFS::chmod(std::string_view path, uint32_t mode) {
    std::string hostPath = toHostPath(path);
    int r = ::chmod(hostPath.c_str(), mode);
    return r < 0 ? -errno : 0;
}

int HostFS::chown(std::string_view path, uint32_t owner, uint32_t group) {
    std::string hostPath = toHostPath(path);
    int r = ::chown(hostPath.c_str(), owner, group);
    return r < 0 ? -errno : 0;
}

int HostFS::fchmod(int fd, uint32_t mode) {
    int r = ::fchmod(fd, mode);
    return r < 0 ? -errno : 0;
}

int HostFS::fchown(int fd, uint32_t owner, uint32_t group) {
    int r = ::fchown(fd, owner, group);
    return r < 0 ? -errno : 0;
}

// Directory operations

int HostFS::mkdir(std::string_view path, uint32_t mode) {
    std::string hostPath = toHostPath(path);
    int r = ::mkdir(hostPath.c_str(), mode);
    return r < 0 ? -errno : 0;
}

int HostFS::rmdir(std::string_view path) {
    std::string hostPath = toHostPath(path);
    int r = ::rmdir(hostPath.c_str());
    return r < 0 ? -errno : 0;
}

int HostFS::getdents(int fd, void* buf, size_t count) {
    // Use syscall directly for getdents64
    // On Linux, getdents64 is syscall 217 on x86_64
    ssize_t r = syscall(SYS_getdents64, fd, buf, count);
    return r < 0 ? -errno : static_cast<int>(r);
}

// Link operations

int HostFS::link(std::string_view oldpath, std::string_view newpath) {
    std::string hostOld = toHostPath(oldpath);
    std::string hostNew = toHostPath(newpath);
    int r = ::link(hostOld.c_str(), hostNew.c_str());
    return r < 0 ? -errno : 0;
}

int HostFS::unlink(std::string_view path) {
    std::string hostPath = toHostPath(path);
    int r = ::unlink(hostPath.c_str());
    return r < 0 ? -errno : 0;
}

int HostFS::symlink(std::string_view target, std::string_view linkpath) {
    std::string hostLink = toHostPath(linkpath);
    int r = ::symlink(std::string(target).c_str(), hostLink.c_str());
    return r < 0 ? -errno : 0;
}

int HostFS::readlink(std::string_view path, char* buf, size_t bufsiz) {
    std::string hostPath = toHostPath(path);
    ssize_t r = ::readlink(hostPath.c_str(), buf, bufsiz);
    return r < 0 ? -errno : static_cast<int>(r);
}

int HostFS::rename(std::string_view oldpath, std::string_view newpath) {
    std::string hostOld = toHostPath(oldpath);
    std::string hostNew = toHostPath(newpath);
    int r = ::rename(hostOld.c_str(), hostNew.c_str());
    return r < 0 ? -errno : 0;
}

// Misc

int HostFS::ioctl(int fd, uint64_t request, void* arg) {
    int r = ::ioctl(fd, request, arg);
    return r < 0 ? -errno : r;
}

int HostFS::isatty(int fd) {
    return ::isatty(fd) ? 1 : 0;
}

// Pipe

int HostFS::pipe(int pipefd[2]) {
    int r = ::pipe(pipefd);
    return r < 0 ? -errno : 0;
}

// Directory iteration

void* HostFS::opendir(std::string_view path) {
    std::string hostPath = toHostPath(path);
    return ::opendir(hostPath.c_str());
}

void* HostFS::readdir(void* dir) {
    return ::readdir(static_cast<DIR*>(dir));
}

int HostFS::closedir(void* dir) {
    int r = ::closedir(static_cast<DIR*>(dir));
    return r < 0 ? -errno : 0;
}

} // namespace yos
