#include "vfs.hpp"
#include <algorithm>
#include <cerrno>
#include <cstring>

namespace yos {

VFS::VFS() : _cwd("/"), _umask(0022), _nextFd(3) {
    // Reserve fds 0,1,2 for stdin/stdout/stderr
    _fdTable.resize(3);
}

VFS::~VFS() = default;

void VFS::initStdio(Filesystem* fs) {
    // Set up fd 0,1,2 to use the host's stdin/stdout/stderr
    // The HostFS will pass these through to the actual fds
    _fdTable[0] = {0, fs, "/dev/stdin"};
    _fdTable[1] = {1, fs, "/dev/stdout"};
    _fdTable[2] = {2, fs, "/dev/stderr"};
}

void VFS::mount(std::string_view path, std::shared_ptr<Filesystem> fs) {
    std::string p(path);
    // Normalize: ensure starts with /, remove trailing /
    if (p.empty() || p[0] != '/') p = "/" + p;
    while (p.size() > 1 && p.back() == '/') p.pop_back();
    _mounts[p] = std::move(fs);
}

void VFS::umount(std::string_view path) {
    std::string p(path);
    if (p.empty() || p[0] != '/') p = "/" + p;
    while (p.size() > 1 && p.back() == '/') p.pop_back();
    _mounts.erase(p);
}

std::string VFS::makeAbsolute(std::string_view path) {
    if (path.empty()) return _cwd;
    if (path[0] == '/') return std::string(path);

    // Relative path - join with cwd
    std::string result = _cwd;
    if (result.back() != '/') result += '/';
    result += path;

    // Normalize: resolve . and ..
    std::vector<std::string> parts;
    size_t start = 0;
    for (size_t i = 0; i <= result.size(); ++i) {
        if (i == result.size() || result[i] == '/') {
            if (i > start) {
                std::string part = result.substr(start, i - start);
                if (part == "..") {
                    if (!parts.empty()) parts.pop_back();
                } else if (part != ".") {
                    parts.push_back(std::move(part));
                }
            }
            start = i + 1;
        }
    }

    if (parts.empty()) return "/";
    result = "";
    for (const auto& p : parts) {
        result += "/" + p;
    }
    return result;
}

VFS::ResolvedPath VFS::resolve(std::string_view path) {
    std::string absPath = makeAbsolute(path);

    // Find longest matching mount point
    // _mounts is sorted by path length descending (std::greater<>)
    for (auto& [mountPath, fs] : _mounts) {
        if (absPath == mountPath ||
            (absPath.size() > mountPath.size() &&
             absPath.substr(0, mountPath.size()) == mountPath &&
             (mountPath == "/" || absPath[mountPath.size()] == '/'))) {

            std::string relPath;
            if (mountPath == "/") {
                relPath = absPath;
            } else {
                relPath = absPath.substr(mountPath.size());
                if (relPath.empty()) relPath = "/";
            }
            return {fs.get(), relPath};
        }
    }

    return {nullptr, absPath};
}

FdInfo* VFS::getFd(int fd) {
    if (fd < 0 || fd >= static_cast<int>(_fdTable.size())) return nullptr;
    if (_fdTable[fd].fs == nullptr) return nullptr;
    return &_fdTable[fd];
}

int VFS::allocFd(int localFd, Filesystem* fs, std::string_view path) {
    // Find first free slot or expand
    for (size_t i = 3; i < _fdTable.size(); ++i) {
        if (_fdTable[i].fs == nullptr) {
            _fdTable[i] = {localFd, fs, std::string(path)};
            return static_cast<int>(i);
        }
    }
    int fd = static_cast<int>(_fdTable.size());
    _fdTable.push_back({localFd, fs, std::string(path)});
    return fd;
}

// File operations

int VFS::open(std::string_view path, int flags, int mode) {
    auto [fs, relPath] = resolve(path);
    if (!fs) return -ENOENT;

    int localFd = fs->open(relPath, flags, mode & ~_umask);
    if (localFd < 0) return localFd;

    return allocFd(localFd, fs, path);
}

int VFS::close(int fd) {
    auto* info = getFd(fd);
    if (!info) return -EBADF;

    int result = info->fs->close(info->localFd);
    info->fs = nullptr;
    info->localFd = -1;
    info->path.clear();
    return result;
}

int VFS::read(int fd, void* buf, size_t count) {
    auto* info = getFd(fd);
    if (!info) return -EBADF;
    return info->fs->read(info->localFd, buf, count);
}

int VFS::write(int fd, const void* buf, size_t count) {
    auto* info = getFd(fd);
    if (!info) return -EBADF;
    return info->fs->write(info->localFd, buf, count);
}

int64_t VFS::lseek(int fd, int64_t offset, int whence) {
    auto* info = getFd(fd);
    if (!info) return -EBADF;
    return info->fs->lseek(info->localFd, offset, whence);
}

int VFS::ftruncate(int fd, int64_t length) {
    auto* info = getFd(fd);
    if (!info) return -EBADF;
    return info->fs->ftruncate(info->localFd, length);
}

int VFS::fsync(int fd) {
    auto* info = getFd(fd);
    if (!info) return -EBADF;
    return info->fs->fsync(info->localFd);
}

int VFS::dup(int oldfd) {
    auto* info = getFd(oldfd);
    if (!info) return -EBADF;

    int localFd = info->fs->dup(info->localFd);
    if (localFd < 0) return localFd;

    return allocFd(localFd, info->fs, info->path);
}

int VFS::dup2(int oldfd, int newfd) {
    auto* info = getFd(oldfd);
    if (!info) return -EBADF;
    if (newfd < 0) return -EBADF;
    if (oldfd == newfd) return newfd;

    // Close newfd if open
    if (newfd < static_cast<int>(_fdTable.size()) && _fdTable[newfd].fs) {
        close(newfd);
    }

    int localFd = info->fs->dup(info->localFd);
    if (localFd < 0) return localFd;

    // Ensure table is large enough
    while (static_cast<int>(_fdTable.size()) <= newfd) {
        _fdTable.push_back({-1, nullptr, ""});
    }

    _fdTable[newfd] = {localFd, info->fs, info->path};
    return newfd;
}

int VFS::fcntl(int fd, int cmd, int64_t arg) {
    auto* info = getFd(fd);
    if (!info) return -EBADF;
    return info->fs->fcntl(info->localFd, cmd, arg);
}

// Stat operations

int VFS::stat(std::string_view path, struct stat* buf) {
    auto [fs, relPath] = resolve(path);
    if (!fs) return -ENOENT;
    return fs->stat(relPath, buf);
}

int VFS::lstat(std::string_view path, struct stat* buf) {
    auto [fs, relPath] = resolve(path);
    if (!fs) return -ENOENT;
    return fs->lstat(relPath, buf);
}

int VFS::fstat(int fd, struct stat* buf) {
    auto* info = getFd(fd);
    if (!info) return -EBADF;
    return info->fs->fstat(info->localFd, buf);
}

int VFS::access(std::string_view path, int mode) {
    auto [fs, relPath] = resolve(path);
    if (!fs) return -ENOENT;
    return fs->access(relPath, mode);
}

int VFS::chmod(std::string_view path, uint32_t mode) {
    auto [fs, relPath] = resolve(path);
    if (!fs) return -ENOENT;
    return fs->chmod(relPath, mode);
}

int VFS::chown(std::string_view path, uint32_t owner, uint32_t group) {
    auto [fs, relPath] = resolve(path);
    if (!fs) return -ENOENT;
    return fs->chown(relPath, owner, group);
}

int VFS::fchmod(int fd, uint32_t mode) {
    auto* info = getFd(fd);
    if (!info) return -EBADF;
    return info->fs->fchmod(info->localFd, mode);
}

int VFS::fchown(int fd, uint32_t owner, uint32_t group) {
    auto* info = getFd(fd);
    if (!info) return -EBADF;
    return info->fs->fchown(info->localFd, owner, group);
}

uint32_t VFS::umask(uint32_t mask) {
    uint32_t old = _umask;
    _umask = mask & 0777;
    return old;
}

// Directory operations

int VFS::chdir(std::string_view path) {
    std::string absPath = makeAbsolute(path);
    auto [fs, relPath] = resolve(absPath);
    if (!fs) return -ENOENT;

    // Check if it's a valid directory
    struct stat st;
    int r = fs->stat(relPath, &st);
    if (r < 0) return r;
    if (!S_ISDIR(st.st_mode)) return -ENOTDIR;

    _cwd = absPath;
    return 0;
}

int VFS::fchdir(int fd) {
    auto* info = getFd(fd);
    if (!info) return -EBADF;

    struct stat st;
    int r = info->fs->fstat(info->localFd, &st);
    if (r < 0) return r;
    if (!S_ISDIR(st.st_mode)) return -ENOTDIR;

    _cwd = info->path;
    return 0;
}

int VFS::getcwd(char* buf, size_t size) {
    if (_cwd.size() + 1 > size) return -ERANGE;
    std::memcpy(buf, _cwd.c_str(), _cwd.size() + 1);
    return 0;
}

int VFS::mkdir(std::string_view path, uint32_t mode) {
    auto [fs, relPath] = resolve(path);
    if (!fs) return -ENOENT;
    return fs->mkdir(relPath, mode & ~_umask);
}

int VFS::rmdir(std::string_view path) {
    auto [fs, relPath] = resolve(path);
    if (!fs) return -ENOENT;
    return fs->rmdir(relPath);
}

int VFS::getdents(int fd, void* buf, size_t count) {
    auto* info = getFd(fd);
    if (!info) return -EBADF;
    return info->fs->getdents(info->localFd, buf, count);
}

// Link operations

int VFS::link(std::string_view oldpath, std::string_view newpath) {
    auto [fs1, relOld] = resolve(oldpath);
    auto [fs2, relNew] = resolve(newpath);
    if (!fs1 || !fs2) return -ENOENT;
    if (fs1 != fs2) return -EXDEV;  // Cross-device link
    return fs1->link(relOld, relNew);
}

int VFS::unlink(std::string_view path) {
    auto [fs, relPath] = resolve(path);
    if (!fs) return -ENOENT;
    return fs->unlink(relPath);
}

int VFS::symlink(std::string_view target, std::string_view linkpath) {
    auto [fs, relPath] = resolve(linkpath);
    if (!fs) return -ENOENT;
    // Note: target is stored as-is, not resolved
    return fs->symlink(target, relPath);
}

int VFS::readlink(std::string_view path, char* buf, size_t bufsiz) {
    auto [fs, relPath] = resolve(path);
    if (!fs) return -ENOENT;
    return fs->readlink(relPath, buf, bufsiz);
}

int VFS::rename(std::string_view oldpath, std::string_view newpath) {
    auto [fs1, relOld] = resolve(oldpath);
    auto [fs2, relNew] = resolve(newpath);
    if (!fs1 || !fs2) return -ENOENT;
    if (fs1 != fs2) return -EXDEV;
    return fs1->rename(relOld, relNew);
}

// Misc

int VFS::ioctl(int fd, uint64_t request, void* arg) {
    auto* info = getFd(fd);
    if (!info) return -EBADF;
    return info->fs->ioctl(info->localFd, request, arg);
}

int VFS::isatty(int fd) {
    auto* info = getFd(fd);
    if (!info) return -EBADF;
    return info->fs->isatty(info->localFd);
}

// Pipe

int VFS::pipe(int pipefd[2]) {
    // Find first mounted fs that supports pipe
    for (auto& [path, fs] : _mounts) {
        int localFds[2];
        int r = fs->pipe(localFds);
        if (r == 0) {
            pipefd[0] = allocFd(localFds[0], fs.get(), "[pipe:read]");
            pipefd[1] = allocFd(localFds[1], fs.get(), "[pipe:write]");
            return 0;
        }
    }
    return -ENOSYS;
}

int VFS::pipe2(int pipefd[2], int flags) {
    // For now, ignore flags and use regular pipe
    return pipe(pipefd);
}

// opendir/readdir/closedir - higher level wrappers

void* VFS::opendir(std::string_view path) {
    auto [fs, relPath] = resolve(path);
    if (!fs) return nullptr;
    return fs->opendir(relPath);
}

void* VFS::readdir(void* dir) {
    // Need to track which fs owns this DIR*
    // For now, this is a limitation - we assume HostFS
    return nullptr;  // TODO: implement properly
}

int VFS::closedir(void* dir) {
    // Same issue as readdir
    return -ENOSYS;  // TODO: implement properly
}

} // namespace yos
