// YOS Virtual Filesystem Implementation
//
// All filesystem syscalls go through yos_exec_ctx_t which has per-process:
//   - fds[]: file descriptor table
//   - dirs[]: directory handle table
//   - cwd: current working directory

#define _GNU_SOURCE
#include "yos-vfs.h"
#include "yos-log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>

// ============================================================================
// Internal helpers
// ============================================================================

char* yos_resolve_path(yos_exec_ctx_t* ctx, const char* path, char* buf, size_t bufsiz) {
    if (!path || !buf || bufsiz == 0) return NULL;

    if (path[0] == '/') {
        // Absolute path
        strncpy(buf, path, bufsiz - 1);
        buf[bufsiz - 1] = '\0';
    } else {
        // Relative path - prepend cwd
        int n = snprintf(buf, bufsiz, "%s/%s", ctx->cwd, path);
        if (n < 0 || (size_t)n >= bufsiz) return NULL;
    }

    // TODO: normalize . and .. components
    return buf;
}

int yos_alloc_fd(yos_exec_ctx_t* ctx, int host_fd, const char* path) {
    for (int i = 0; i < YOS_MAX_FDS; i++) {
        if (ctx->fds[i].host_fd < 0) {
            ctx->fds[i].host_fd = host_fd;
            ctx->fds[i].flags = 0;
            if (path) {
                snprintf(ctx->fds[i].path, sizeof(ctx->fds[i].path), "%s", path);
            } else {
                ctx->fds[i].path[0] = '\0';
            }
            return i;
        }
    }
    return -EMFILE;
}

int yos_get_host_fd(yos_exec_ctx_t* ctx, int fd) {
    if (fd < 0 || fd >= YOS_MAX_FDS) return -EBADF;
    if (ctx->fds[fd].host_fd < 0) return -EBADF;
    return ctx->fds[fd].host_fd;
}

// ============================================================================
// File operations
// ============================================================================

int yos_open(yos_exec_ctx_t* ctx, const char* path, int flags, int mode) {
    char resolved[PATH_MAX];
    if (!yos_resolve_path(ctx, path, resolved, sizeof(resolved))) {
        return -ENAMETOOLONG;
    }

    YOS_DEBUG("path=%s flags=0x%x mode=0%o", resolved, flags, mode);

    int host_fd = open(resolved, flags, mode);
    if (host_fd < 0) {
        YOS_DEBUG("open failed: %s", strerror(errno));
        return -errno;
    }

    int fd = yos_alloc_fd(ctx, host_fd, resolved);
    if (fd < 0) {
        close(host_fd);
        return fd;
    }

    ctx->fds[fd].flags = flags;
    YOS_DEBUG("fd=%d -> host_fd=%d", fd, host_fd);
    return fd;
}

int yos_openat(yos_exec_ctx_t* ctx, int dirfd, const char* path, int flags, int mode) {
    YOS_DEBUG("dirfd=%d path=%s flags=0x%x", dirfd, path, flags);

    int host_dirfd;
    if (dirfd == -100) {  // AT_FDCWD
        host_dirfd = AT_FDCWD;
    } else {
        host_dirfd = yos_get_host_fd(ctx, dirfd);
        if (host_dirfd < 0) return host_dirfd;
    }

    int host_fd = openat(host_dirfd, path, flags, mode);
    if (host_fd < 0) return -errno;

    int fd = yos_alloc_fd(ctx, host_fd, path);
    if (fd < 0) {
        close(host_fd);
        return fd;
    }

    ctx->fds[fd].flags = flags;
    return fd;
}

int yos_close(yos_exec_ctx_t* ctx, int fd) {
    YOS_DEBUG("fd=%d", fd);

    if (fd < 0 || fd >= YOS_MAX_FDS) return -EBADF;
    if (ctx->fds[fd].host_fd < 0) return -EBADF;

    // Don't close stdio
    if (fd <= 2) {
        return 0;
    }

    int r = close(ctx->fds[fd].host_fd);
    ctx->fds[fd].host_fd = -1;
    ctx->fds[fd].path[0] = '\0';
    return r < 0 ? -errno : 0;
}

ssize_t yos_read(yos_exec_ctx_t* ctx, int fd, void* buf, size_t count) {
    int host_fd = yos_get_host_fd(ctx, fd);
    if (host_fd < 0) return host_fd;

    ssize_t r = read(host_fd, buf, count);
    return r < 0 ? -errno : r;
}

ssize_t yos_write(yos_exec_ctx_t* ctx, int fd, const void* buf, size_t count) {
    int host_fd = yos_get_host_fd(ctx, fd);
    if (host_fd < 0) return host_fd;

    ssize_t r = write(host_fd, buf, count);
    return r < 0 ? -errno : r;
}

off_t yos_lseek(yos_exec_ctx_t* ctx, int fd, off_t offset, int whence) {
    int host_fd = yos_get_host_fd(ctx, fd);
    if (host_fd < 0) return host_fd;

    off_t r = lseek(host_fd, offset, whence);
    return r < 0 ? -errno : r;
}

int yos_dup(yos_exec_ctx_t* ctx, int oldfd) {
    int host_oldfd = yos_get_host_fd(ctx, oldfd);
    if (host_oldfd < 0) return host_oldfd;

    int host_newfd = dup(host_oldfd);
    if (host_newfd < 0) return -errno;

    int newfd = yos_alloc_fd(ctx, host_newfd, ctx->fds[oldfd].path);
    if (newfd < 0) {
        close(host_newfd);
        return newfd;
    }

    ctx->fds[newfd].flags = ctx->fds[oldfd].flags;
    return newfd;
}

int yos_dup2(yos_exec_ctx_t* ctx, int oldfd, int newfd) {
    if (oldfd == newfd) return newfd;
    if (newfd < 0 || newfd >= YOS_MAX_FDS) return -EBADF;

    int host_oldfd = yos_get_host_fd(ctx, oldfd);
    if (host_oldfd < 0) return host_oldfd;

    // Close newfd if open
    if (ctx->fds[newfd].host_fd >= 0) {
        close(ctx->fds[newfd].host_fd);
    }

    int host_newfd = dup(host_oldfd);
    if (host_newfd < 0) return -errno;

    ctx->fds[newfd].host_fd = host_newfd;
    ctx->fds[newfd].flags = ctx->fds[oldfd].flags;
    strncpy(ctx->fds[newfd].path, ctx->fds[oldfd].path, sizeof(ctx->fds[newfd].path));
    return newfd;
}

int yos_dup3(yos_exec_ctx_t* ctx, int oldfd, int newfd, int flags) {
    // flags is O_CLOEXEC, ignore for now
    return yos_dup2(ctx, oldfd, newfd);
}

int yos_pipe(yos_exec_ctx_t* ctx, int pipefd[2]) {
    int host_fds[2];
    if (pipe(host_fds) < 0) return -errno;

    int fd0 = yos_alloc_fd(ctx, host_fds[0], "pipe:read");
    if (fd0 < 0) {
        close(host_fds[0]);
        close(host_fds[1]);
        return fd0;
    }

    int fd1 = yos_alloc_fd(ctx, host_fds[1], "pipe:write");
    if (fd1 < 0) {
        close(host_fds[0]);
        close(host_fds[1]);
        ctx->fds[fd0].host_fd = -1;
        return fd1;
    }

    pipefd[0] = fd0;
    pipefd[1] = fd1;
    return 0;
}

int yos_pipe2(yos_exec_ctx_t* ctx, int pipefd[2], int flags) {
    // TODO: handle flags (O_NONBLOCK, O_CLOEXEC)
    return yos_pipe(ctx, pipefd);
}

int yos_fcntl(yos_exec_ctx_t* ctx, int fd, int cmd, int arg) {
    int host_fd = yos_get_host_fd(ctx, fd);
    if (host_fd < 0) return host_fd;

    int r = fcntl(host_fd, cmd, arg);
    return r < 0 ? -errno : r;
}

int yos_ioctl(yos_exec_ctx_t* ctx, int fd, unsigned long request, void* arg) {
    int host_fd = yos_get_host_fd(ctx, fd);
    if (host_fd < 0) return host_fd;

    int r = ioctl(host_fd, request, arg);
    return r < 0 ? -errno : r;
}

int yos_ftruncate(yos_exec_ctx_t* ctx, int fd, off_t length) {
    int host_fd = yos_get_host_fd(ctx, fd);
    if (host_fd < 0) return host_fd;

    int r = ftruncate(host_fd, length);
    return r < 0 ? -errno : 0;
}

int yos_fsync(yos_exec_ctx_t* ctx, int fd) {
    int host_fd = yos_get_host_fd(ctx, fd);
    if (host_fd < 0) return host_fd;

    int r = fsync(host_fd);
    return r < 0 ? -errno : 0;
}

int yos_fdatasync(yos_exec_ctx_t* ctx, int fd) {
    int host_fd = yos_get_host_fd(ctx, fd);
    if (host_fd < 0) return host_fd;

    int r = fdatasync(host_fd);
    return r < 0 ? -errno : 0;
}

int yos_isatty(yos_exec_ctx_t* ctx, int fd) {
    int host_fd = yos_get_host_fd(ctx, fd);
    if (host_fd < 0) return 0;
    return isatty(host_fd);
}

// ============================================================================
// Stat operations
// ============================================================================

int yos_stat(yos_exec_ctx_t* ctx, const char* path, void* buf) {
    char resolved[PATH_MAX];
    if (!yos_resolve_path(ctx, path, resolved, sizeof(resolved))) {
        return -ENAMETOOLONG;
    }

    YOS_DEBUG("path=%s", resolved);
    int r = stat(resolved, (struct stat*)buf);
    return r < 0 ? -errno : 0;
}

int yos_lstat(yos_exec_ctx_t* ctx, const char* path, void* buf) {
    char resolved[PATH_MAX];
    if (!yos_resolve_path(ctx, path, resolved, sizeof(resolved))) {
        return -ENAMETOOLONG;
    }

    YOS_DEBUG("path=%s", resolved);
    int r = lstat(resolved, (struct stat*)buf);
    return r < 0 ? -errno : 0;
}

int yos_fstat(yos_exec_ctx_t* ctx, int fd, void* buf) {
    int host_fd = yos_get_host_fd(ctx, fd);
    if (host_fd < 0) return host_fd;

    int r = fstat(host_fd, (struct stat*)buf);
    return r < 0 ? -errno : 0;
}

int yos_fstatat(yos_exec_ctx_t* ctx, int dirfd, const char* path, void* buf, int flags) {
    int host_dirfd = (dirfd == -100) ? AT_FDCWD : yos_get_host_fd(ctx, dirfd);
    if (dirfd != -100 && host_dirfd < 0) return host_dirfd;

    int r = fstatat(host_dirfd, path, (struct stat*)buf, flags);
    return r < 0 ? -errno : 0;
}

int yos_access(yos_exec_ctx_t* ctx, const char* path, int mode) {
    char resolved[PATH_MAX];
    if (!yos_resolve_path(ctx, path, resolved, sizeof(resolved))) {
        return -ENAMETOOLONG;
    }

    int r = access(resolved, mode);
    return r < 0 ? -errno : 0;
}

int yos_faccessat(yos_exec_ctx_t* ctx, int dirfd, const char* path, int mode, int flags) {
    int host_dirfd = (dirfd == -100) ? AT_FDCWD : yos_get_host_fd(ctx, dirfd);
    if (dirfd != -100 && host_dirfd < 0) return host_dirfd;

    int r = faccessat(host_dirfd, path, mode, flags);
    return r < 0 ? -errno : 0;
}

// ============================================================================
// Directory operations
// ============================================================================

int yos_chdir(yos_exec_ctx_t* ctx, const char* path) {
    char resolved[PATH_MAX];
    if (!yos_resolve_path(ctx, path, resolved, sizeof(resolved))) {
        return -ENAMETOOLONG;
    }

    // Verify it's a directory
    struct stat st;
    if (stat(resolved, &st) < 0) return -errno;
    if (!S_ISDIR(st.st_mode)) return -ENOTDIR;

    strncpy(ctx->cwd, resolved, sizeof(ctx->cwd) - 1);
    ctx->cwd[sizeof(ctx->cwd) - 1] = '\0';
    YOS_DEBUG("cwd=%s", ctx->cwd);
    return 0;
}

int yos_fchdir(yos_exec_ctx_t* ctx, int fd) {
    if (fd < 0 || fd >= YOS_MAX_FDS) return -EBADF;
    if (ctx->fds[fd].host_fd < 0) return -EBADF;

    // Use the stored path
    if (ctx->fds[fd].path[0]) {
        strncpy(ctx->cwd, ctx->fds[fd].path, sizeof(ctx->cwd) - 1);
        ctx->cwd[sizeof(ctx->cwd) - 1] = '\0';
        return 0;
    }
    return -EBADF;
}

char* yos_getcwd(yos_exec_ctx_t* ctx, char* buf, size_t size) {
    if (!buf || size == 0) return NULL;

    size_t len = strlen(ctx->cwd);
    if (len >= size) return NULL;

    memcpy(buf, ctx->cwd, len + 1);
    return buf;
}

int yos_mkdir(yos_exec_ctx_t* ctx, const char* path, uint32_t mode) {
    char resolved[PATH_MAX];
    if (!yos_resolve_path(ctx, path, resolved, sizeof(resolved))) {
        return -ENAMETOOLONG;
    }

    int r = mkdir(resolved, mode);
    return r < 0 ? -errno : 0;
}

int yos_mkdirat(yos_exec_ctx_t* ctx, int dirfd, const char* path, uint32_t mode) {
    int host_dirfd = (dirfd == -100) ? AT_FDCWD : yos_get_host_fd(ctx, dirfd);
    if (dirfd != -100 && host_dirfd < 0) return host_dirfd;

    int r = mkdirat(host_dirfd, path, mode);
    return r < 0 ? -errno : 0;
}

int yos_rmdir(yos_exec_ctx_t* ctx, const char* path) {
    char resolved[PATH_MAX];
    if (!yos_resolve_path(ctx, path, resolved, sizeof(resolved))) {
        return -ENAMETOOLONG;
    }

    int r = rmdir(resolved);
    return r < 0 ? -errno : 0;
}

// ============================================================================
// Directory handle operations
// ============================================================================

static int alloc_dir(yos_exec_ctx_t* ctx, DIR* d) {
    for (int i = 0; i < YOS_MAX_DIRS; i++) {
        if (!ctx->dirs[i].host_dir) {
            ctx->dirs[i].host_dir = d;
            return i + 1;  // 1-based handle
        }
    }
    return -1;
}

static DIR* get_dir(yos_exec_ctx_t* ctx, int handle) {
    if (handle <= 0 || handle > YOS_MAX_DIRS) return NULL;
    return (DIR*)ctx->dirs[handle - 1].host_dir;
}

void* yos_opendir(yos_exec_ctx_t* ctx, const char* path) {
    char resolved[PATH_MAX];
    if (!yos_resolve_path(ctx, path, resolved, sizeof(resolved))) {
        errno = ENAMETOOLONG;
        return NULL;
    }

    DIR* d = opendir(resolved);
    if (!d) return NULL;

    int handle = alloc_dir(ctx, d);
    if (handle < 0) {
        closedir(d);
        errno = ENOMEM;
        return NULL;
    }

    return (void*)(intptr_t)handle;
}

void* yos_readdir(yos_exec_ctx_t* ctx, void* dir) {
    DIR* d = get_dir(ctx, (int)(intptr_t)dir);
    if (!d) return NULL;
    return readdir(d);
}

int yos_closedir(yos_exec_ctx_t* ctx, void* dir) {
    int handle = (int)(intptr_t)dir;
    DIR* d = get_dir(ctx, handle);
    if (!d) return -EBADF;

    int r = closedir(d);
    ctx->dirs[handle - 1].host_dir = NULL;
    return r < 0 ? -errno : 0;
}

void yos_rewinddir(yos_exec_ctx_t* ctx, void* dir) {
    DIR* d = get_dir(ctx, (int)(intptr_t)dir);
    if (d) rewinddir(d);
}

void yos_seekdir(yos_exec_ctx_t* ctx, void* dir, long loc) {
    DIR* d = get_dir(ctx, (int)(intptr_t)dir);
    if (d) seekdir(d, loc);
}

long yos_telldir(yos_exec_ctx_t* ctx, void* dir) {
    DIR* d = get_dir(ctx, (int)(intptr_t)dir);
    if (!d) return -1;
    return telldir(d);
}

ssize_t yos_getdents(yos_exec_ctx_t* ctx, int fd, void* buf, size_t count) {
    int host_fd = yos_get_host_fd(ctx, fd);
    if (host_fd < 0) return host_fd;

    ssize_t r = syscall(SYS_getdents64, host_fd, buf, count);
    return r < 0 ? -errno : r;
}

ssize_t yos_getdents64(yos_exec_ctx_t* ctx, int fd, void* buf, size_t count) {
    return yos_getdents(ctx, fd, buf, count);
}

// ============================================================================
// Link operations
// ============================================================================

int yos_unlink(yos_exec_ctx_t* ctx, const char* path) {
    char resolved[PATH_MAX];
    if (!yos_resolve_path(ctx, path, resolved, sizeof(resolved))) {
        return -ENAMETOOLONG;
    }

    int r = unlink(resolved);
    return r < 0 ? -errno : 0;
}

int yos_unlinkat(yos_exec_ctx_t* ctx, int dirfd, const char* path, int flags) {
    int host_dirfd = (dirfd == -100) ? AT_FDCWD : yos_get_host_fd(ctx, dirfd);
    if (dirfd != -100 && host_dirfd < 0) return host_dirfd;

    int r = unlinkat(host_dirfd, path, flags);
    return r < 0 ? -errno : 0;
}

int yos_link(yos_exec_ctx_t* ctx, const char* oldpath, const char* newpath) {
    char resolved_old[PATH_MAX], resolved_new[PATH_MAX];
    if (!yos_resolve_path(ctx, oldpath, resolved_old, sizeof(resolved_old))) {
        return -ENAMETOOLONG;
    }
    if (!yos_resolve_path(ctx, newpath, resolved_new, sizeof(resolved_new))) {
        return -ENAMETOOLONG;
    }

    int r = link(resolved_old, resolved_new);
    return r < 0 ? -errno : 0;
}

int yos_linkat(yos_exec_ctx_t* ctx, int olddirfd, const char* oldpath,
               int newdirfd, const char* newpath, int flags) {
    int host_olddirfd = (olddirfd == -100) ? AT_FDCWD : yos_get_host_fd(ctx, olddirfd);
    int host_newdirfd = (newdirfd == -100) ? AT_FDCWD : yos_get_host_fd(ctx, newdirfd);

    if (olddirfd != -100 && host_olddirfd < 0) return host_olddirfd;
    if (newdirfd != -100 && host_newdirfd < 0) return host_newdirfd;

    int r = linkat(host_olddirfd, oldpath, host_newdirfd, newpath, flags);
    return r < 0 ? -errno : 0;
}

int yos_symlink(yos_exec_ctx_t* ctx, const char* target, const char* linkpath) {
    char resolved[PATH_MAX];
    if (!yos_resolve_path(ctx, linkpath, resolved, sizeof(resolved))) {
        return -ENAMETOOLONG;
    }

    int r = symlink(target, resolved);
    return r < 0 ? -errno : 0;
}

int yos_symlinkat(yos_exec_ctx_t* ctx, const char* target, int newdirfd, const char* linkpath) {
    int host_dirfd = (newdirfd == -100) ? AT_FDCWD : yos_get_host_fd(ctx, newdirfd);
    if (newdirfd != -100 && host_dirfd < 0) return host_dirfd;

    int r = symlinkat(target, host_dirfd, linkpath);
    return r < 0 ? -errno : 0;
}

ssize_t yos_readlink(yos_exec_ctx_t* ctx, const char* path, char* buf, size_t bufsiz) {
    char resolved[PATH_MAX];
    if (!yos_resolve_path(ctx, path, resolved, sizeof(resolved))) {
        return -ENAMETOOLONG;
    }

    ssize_t r = readlink(resolved, buf, bufsiz);
    return r < 0 ? -errno : r;
}

ssize_t yos_readlinkat(yos_exec_ctx_t* ctx, int dirfd, const char* path,
                       char* buf, size_t bufsiz) {
    int host_dirfd = (dirfd == -100) ? AT_FDCWD : yos_get_host_fd(ctx, dirfd);
    if (dirfd != -100 && host_dirfd < 0) return host_dirfd;

    ssize_t r = readlinkat(host_dirfd, path, buf, bufsiz);
    return r < 0 ? -errno : r;
}

int yos_rename(yos_exec_ctx_t* ctx, const char* oldpath, const char* newpath) {
    char resolved_old[PATH_MAX], resolved_new[PATH_MAX];
    if (!yos_resolve_path(ctx, oldpath, resolved_old, sizeof(resolved_old))) {
        return -ENAMETOOLONG;
    }
    if (!yos_resolve_path(ctx, newpath, resolved_new, sizeof(resolved_new))) {
        return -ENAMETOOLONG;
    }

    int r = rename(resolved_old, resolved_new);
    return r < 0 ? -errno : 0;
}

int yos_renameat(yos_exec_ctx_t* ctx, int olddirfd, const char* oldpath,
                 int newdirfd, const char* newpath) {
    int host_olddirfd = (olddirfd == -100) ? AT_FDCWD : yos_get_host_fd(ctx, olddirfd);
    int host_newdirfd = (newdirfd == -100) ? AT_FDCWD : yos_get_host_fd(ctx, newdirfd);

    if (olddirfd != -100 && host_olddirfd < 0) return host_olddirfd;
    if (newdirfd != -100 && host_newdirfd < 0) return host_newdirfd;

    int r = renameat(host_olddirfd, oldpath, host_newdirfd, newpath);
    return r < 0 ? -errno : 0;
}

// ============================================================================
// Permission operations
// ============================================================================

int yos_chmod(yos_exec_ctx_t* ctx, const char* path, uint32_t mode) {
    char resolved[PATH_MAX];
    if (!yos_resolve_path(ctx, path, resolved, sizeof(resolved))) {
        return -ENAMETOOLONG;
    }

    int r = chmod(resolved, mode);
    return r < 0 ? -errno : 0;
}

int yos_fchmod(yos_exec_ctx_t* ctx, int fd, uint32_t mode) {
    int host_fd = yos_get_host_fd(ctx, fd);
    if (host_fd < 0) return host_fd;

    int r = fchmod(host_fd, mode);
    return r < 0 ? -errno : 0;
}

int yos_fchmodat(yos_exec_ctx_t* ctx, int dirfd, const char* path, uint32_t mode, int flags) {
    int host_dirfd = (dirfd == -100) ? AT_FDCWD : yos_get_host_fd(ctx, dirfd);
    if (dirfd != -100 && host_dirfd < 0) return host_dirfd;

    int r = fchmodat(host_dirfd, path, mode, flags);
    return r < 0 ? -errno : 0;
}

int yos_chown(yos_exec_ctx_t* ctx, const char* path, uint32_t owner, uint32_t group) {
    char resolved[PATH_MAX];
    if (!yos_resolve_path(ctx, path, resolved, sizeof(resolved))) {
        return -ENAMETOOLONG;
    }

    int r = chown(resolved, owner, group);
    return r < 0 ? -errno : 0;
}

int yos_fchown(yos_exec_ctx_t* ctx, int fd, uint32_t owner, uint32_t group) {
    int host_fd = yos_get_host_fd(ctx, fd);
    if (host_fd < 0) return host_fd;

    int r = fchown(host_fd, owner, group);
    return r < 0 ? -errno : 0;
}

int yos_fchownat(yos_exec_ctx_t* ctx, int dirfd, const char* path,
                 uint32_t owner, uint32_t group, int flags) {
    int host_dirfd = (dirfd == -100) ? AT_FDCWD : yos_get_host_fd(ctx, dirfd);
    if (dirfd != -100 && host_dirfd < 0) return host_dirfd;

    int r = fchownat(host_dirfd, path, owner, group, flags);
    return r < 0 ? -errno : 0;
}

int yos_lchown(yos_exec_ctx_t* ctx, const char* path, uint32_t owner, uint32_t group) {
    char resolved[PATH_MAX];
    if (!yos_resolve_path(ctx, path, resolved, sizeof(resolved))) {
        return -ENAMETOOLONG;
    }

    int r = lchown(resolved, owner, group);
    return r < 0 ? -errno : 0;
}

uint32_t yos_umask(yos_exec_ctx_t* ctx, uint32_t mask) {
    uint32_t old = ctx->umask;
    ctx->umask = mask & 0777;
    return old;
}
