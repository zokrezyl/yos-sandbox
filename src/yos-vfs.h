// YOS Virtual Filesystem Operations
#ifndef YOS_VFS_H
#define YOS_VFS_H

#include "yos-types.h"
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

// File operations
int yos_open(yos_exec_ctx_t* ctx, const char* path, int flags, int mode);
int yos_openat(yos_exec_ctx_t* ctx, int dirfd, const char* path, int flags, int mode);
int yos_close(yos_exec_ctx_t* ctx, int fd);
ssize_t yos_read(yos_exec_ctx_t* ctx, int fd, void* buf, size_t count);
ssize_t yos_write(yos_exec_ctx_t* ctx, int fd, const void* buf, size_t count);
off_t yos_lseek(yos_exec_ctx_t* ctx, int fd, off_t offset, int whence);
int yos_dup(yos_exec_ctx_t* ctx, int oldfd);
int yos_dup2(yos_exec_ctx_t* ctx, int oldfd, int newfd);
int yos_dup3(yos_exec_ctx_t* ctx, int oldfd, int newfd, int flags);
int yos_pipe(yos_exec_ctx_t* ctx, int pipefd[2]);
int yos_pipe2(yos_exec_ctx_t* ctx, int pipefd[2], int flags);
int yos_fcntl(yos_exec_ctx_t* ctx, int fd, int cmd, int arg);
int yos_ioctl(yos_exec_ctx_t* ctx, int fd, unsigned long request, void* arg);
int yos_ftruncate(yos_exec_ctx_t* ctx, int fd, off_t length);
int yos_fsync(yos_exec_ctx_t* ctx, int fd);
int yos_fdatasync(yos_exec_ctx_t* ctx, int fd);
int yos_isatty(yos_exec_ctx_t* ctx, int fd);

// Stat operations
int yos_stat(yos_exec_ctx_t* ctx, const char* path, void* buf);
int yos_lstat(yos_exec_ctx_t* ctx, const char* path, void* buf);
int yos_fstat(yos_exec_ctx_t* ctx, int fd, void* buf);
int yos_fstatat(yos_exec_ctx_t* ctx, int dirfd, const char* path, void* buf, int flags);
int yos_access(yos_exec_ctx_t* ctx, const char* path, int mode);
int yos_faccessat(yos_exec_ctx_t* ctx, int dirfd, const char* path, int mode, int flags);

// Directory operations
int yos_chdir(yos_exec_ctx_t* ctx, const char* path);
int yos_fchdir(yos_exec_ctx_t* ctx, int fd);
char* yos_getcwd(yos_exec_ctx_t* ctx, char* buf, size_t size);
int yos_mkdir(yos_exec_ctx_t* ctx, const char* path, uint32_t mode);
int yos_mkdirat(yos_exec_ctx_t* ctx, int dirfd, const char* path, uint32_t mode);
int yos_rmdir(yos_exec_ctx_t* ctx, const char* path);
void* yos_opendir(yos_exec_ctx_t* ctx, const char* path);
void* yos_readdir(yos_exec_ctx_t* ctx, void* dir);
int yos_closedir(yos_exec_ctx_t* ctx, void* dir);
void yos_rewinddir(yos_exec_ctx_t* ctx, void* dir);
void yos_seekdir(yos_exec_ctx_t* ctx, void* dir, long loc);
long yos_telldir(yos_exec_ctx_t* ctx, void* dir);
ssize_t yos_getdents(yos_exec_ctx_t* ctx, int fd, void* buf, size_t count);
ssize_t yos_getdents64(yos_exec_ctx_t* ctx, int fd, void* buf, size_t count);

// Link operations
int yos_unlink(yos_exec_ctx_t* ctx, const char* path);
int yos_unlinkat(yos_exec_ctx_t* ctx, int dirfd, const char* path, int flags);
int yos_link(yos_exec_ctx_t* ctx, const char* oldpath, const char* newpath);
int yos_linkat(yos_exec_ctx_t* ctx, int olddirfd, const char* oldpath, int newdirfd, const char* newpath, int flags);
int yos_symlink(yos_exec_ctx_t* ctx, const char* target, const char* linkpath);
int yos_symlinkat(yos_exec_ctx_t* ctx, const char* target, int newdirfd, const char* linkpath);
ssize_t yos_readlink(yos_exec_ctx_t* ctx, const char* path, char* buf, size_t bufsiz);
ssize_t yos_readlinkat(yos_exec_ctx_t* ctx, int dirfd, const char* path, char* buf, size_t bufsiz);
int yos_rename(yos_exec_ctx_t* ctx, const char* oldpath, const char* newpath);
int yos_renameat(yos_exec_ctx_t* ctx, int olddirfd, const char* oldpath, int newdirfd, const char* newpath);

// Permission operations
int yos_chmod(yos_exec_ctx_t* ctx, const char* path, uint32_t mode);
int yos_fchmod(yos_exec_ctx_t* ctx, int fd, uint32_t mode);
int yos_fchmodat(yos_exec_ctx_t* ctx, int dirfd, const char* path, uint32_t mode, int flags);
int yos_chown(yos_exec_ctx_t* ctx, const char* path, uint32_t owner, uint32_t group);
int yos_fchown(yos_exec_ctx_t* ctx, int fd, uint32_t owner, uint32_t group);
int yos_fchownat(yos_exec_ctx_t* ctx, int dirfd, const char* path, uint32_t owner, uint32_t group, int flags);
int yos_lchown(yos_exec_ctx_t* ctx, const char* path, uint32_t owner, uint32_t group);
uint32_t yos_umask(yos_exec_ctx_t* ctx, uint32_t mask);

// Internal helpers
char* yos_resolve_path(yos_exec_ctx_t* ctx, const char* path, char* buf, size_t bufsiz);
int yos_alloc_fd(yos_exec_ctx_t* ctx, int host_fd, const char* path);
int yos_get_host_fd(yos_exec_ctx_t* ctx, int fd);

#ifdef __cplusplus
}
#endif

#endif // YOS_VFS_H
