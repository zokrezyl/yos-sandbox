// Unit tests for yos-vfs.c
// Tests VFS operations: open, read, write, stat, directory ops, etc.

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

#include "yos-types.h"
#include "yos-vfs.h"

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) static void test_##name(void)
#define RUN_TEST(name) do { \
    printf("  %s...", #name); \
    fflush(stdout); \
    test_##name(); \
    tests_run++; \
    tests_passed++; \
    printf(" OK\n"); \
} while(0)

#define ASSERT(cond) do { \
    if (!(cond)) { \
        printf(" FAILED at %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        exit(1); \
    } \
} while(0)

#define ASSERT_EQ(a, b) do { \
    long long _a = (long long)(a); \
    long long _b = (long long)(b); \
    if (_a != _b) { \
        printf(" FAILED at %s:%d: %s == %s (%lld != %lld)\n", \
               __FILE__, __LINE__, #a, #b, _a, _b); \
        exit(1); \
    } \
} while(0)

#define ASSERT_STR_EQ(a, b) do { \
    if (strcmp((a), (b)) != 0) { \
        printf(" FAILED at %s:%d: %s == %s (\"%s\" != \"%s\")\n", \
               __FILE__, __LINE__, #a, #b, (a), (b)); \
        exit(1); \
    } \
} while(0)

// Create a minimal exec context for testing
static yos_exec_ctx_t* create_test_ctx(void) {
    yos_exec_ctx_t* ctx = calloc(1, sizeof(yos_exec_ctx_t));
    if (!ctx) return NULL;

    getcwd(ctx->cwd, sizeof(ctx->cwd));
    ctx->umask = 022;

    for (int i = 0; i < YOS_MAX_FDS; i++) {
        ctx->fds[i].host_fd = -1;
    }

    // Init stdio
    ctx->fds[0].host_fd = STDIN_FILENO;
    ctx->fds[1].host_fd = STDOUT_FILENO;
    ctx->fds[2].host_fd = STDERR_FILENO;

    return ctx;
}

static void destroy_test_ctx(yos_exec_ctx_t* ctx) {
    // Close non-stdio fds
    for (int i = 3; i < YOS_MAX_FDS; i++) {
        if (ctx->fds[i].host_fd >= 0) {
            close(ctx->fds[i].host_fd);
        }
    }
    free(ctx);
}

// ============================================================================
// Path Resolution Tests
// ============================================================================

TEST(resolve_absolute_path) {
    yos_exec_ctx_t* ctx = create_test_ctx();
    char buf[PATH_MAX];

    char* result = yos_resolve_path(ctx, "/usr/bin", buf, sizeof(buf));
    ASSERT(result != NULL);
    ASSERT_STR_EQ(result, "/usr/bin");

    destroy_test_ctx(ctx);
}

TEST(resolve_relative_path) {
    yos_exec_ctx_t* ctx = create_test_ctx();
    strcpy(ctx->cwd, "/home/user");
    char buf[PATH_MAX];

    char* result = yos_resolve_path(ctx, "test.txt", buf, sizeof(buf));
    ASSERT(result != NULL);
    ASSERT_STR_EQ(result, "/home/user/test.txt");

    destroy_test_ctx(ctx);
}

// ============================================================================
// File Descriptor Tests
// ============================================================================

TEST(alloc_fd) {
    yos_exec_ctx_t* ctx = create_test_ctx();

    // First available should be fd 3 (after stdio)
    int fd = yos_alloc_fd(ctx, 100, "/test/path");
    ASSERT_EQ(fd, 3);
    ASSERT_EQ(ctx->fds[3].host_fd, 100);
    ASSERT_STR_EQ(ctx->fds[3].path, "/test/path");

    // Next should be 4
    fd = yos_alloc_fd(ctx, 101, "/test/path2");
    ASSERT_EQ(fd, 4);

    destroy_test_ctx(ctx);
}

TEST(get_host_fd) {
    yos_exec_ctx_t* ctx = create_test_ctx();

    // Stdio should work
    ASSERT_EQ(yos_get_host_fd(ctx, 0), STDIN_FILENO);
    ASSERT_EQ(yos_get_host_fd(ctx, 1), STDOUT_FILENO);
    ASSERT_EQ(yos_get_host_fd(ctx, 2), STDERR_FILENO);

    // Invalid fd should return error
    ASSERT_EQ(yos_get_host_fd(ctx, 3), -EBADF);
    ASSERT_EQ(yos_get_host_fd(ctx, -1), -EBADF);
    ASSERT_EQ(yos_get_host_fd(ctx, 999), -EBADF);

    destroy_test_ctx(ctx);
}

// ============================================================================
// Open/Close Tests
// ============================================================================

TEST(open_close) {
    yos_exec_ctx_t* ctx = create_test_ctx();

    // Create temp file
    char tmpfile[] = "/tmp/yos_test_XXXXXX";
    int host_fd = mkstemp(tmpfile);
    ASSERT(host_fd >= 0);
    close(host_fd);

    // Open via VFS
    int fd = yos_open(ctx, tmpfile, O_RDONLY, 0);
    ASSERT(fd >= 3);  // After stdio
    ASSERT(ctx->fds[fd].host_fd >= 0);

    // Close
    int r = yos_close(ctx, fd);
    ASSERT_EQ(r, 0);
    ASSERT_EQ(ctx->fds[fd].host_fd, -1);

    // Cleanup
    unlink(tmpfile);
    destroy_test_ctx(ctx);
}

TEST(open_nonexistent) {
    yos_exec_ctx_t* ctx = create_test_ctx();

    int fd = yos_open(ctx, "/nonexistent/file/path", O_RDONLY, 0);
    ASSERT(fd < 0);
    ASSERT_EQ(fd, -ENOENT);

    destroy_test_ctx(ctx);
}

// ============================================================================
// Read/Write Tests
// ============================================================================

TEST(read_write) {
    yos_exec_ctx_t* ctx = create_test_ctx();

    // Create temp file
    char tmpfile[] = "/tmp/yos_test_XXXXXX";
    int host_fd = mkstemp(tmpfile);
    ASSERT(host_fd >= 0);
    close(host_fd);

    // Open for writing
    int fd = yos_open(ctx, tmpfile, O_WRONLY | O_TRUNC, 0);
    ASSERT(fd >= 3);

    // Write
    const char* data = "Hello, VFS!";
    ssize_t written = yos_write(ctx, fd, data, strlen(data));
    ASSERT_EQ(written, (ssize_t)strlen(data));

    yos_close(ctx, fd);

    // Open for reading
    fd = yos_open(ctx, tmpfile, O_RDONLY, 0);
    ASSERT(fd >= 3);

    // Read
    char buf[64] = {0};
    ssize_t read_bytes = yos_read(ctx, fd, buf, sizeof(buf) - 1);
    ASSERT_EQ(read_bytes, (ssize_t)strlen(data));
    ASSERT_STR_EQ(buf, data);

    yos_close(ctx, fd);

    // Cleanup
    unlink(tmpfile);
    destroy_test_ctx(ctx);
}

// ============================================================================
// Dup Tests
// ============================================================================

TEST(dup) {
    yos_exec_ctx_t* ctx = create_test_ctx();

    char tmpfile[] = "/tmp/yos_test_XXXXXX";
    int host_fd = mkstemp(tmpfile);
    close(host_fd);

    int fd1 = yos_open(ctx, tmpfile, O_RDWR, 0);
    ASSERT(fd1 >= 3);

    int fd2 = yos_dup(ctx, fd1);
    ASSERT(fd2 > fd1);
    ASSERT(ctx->fds[fd2].host_fd >= 0);

    // Both should work
    const char* data = "test";
    ASSERT_EQ(yos_write(ctx, fd1, data, 4), 4);
    ASSERT_EQ(yos_lseek(ctx, fd2, 0, SEEK_SET), 0);

    char buf[8] = {0};
    ASSERT_EQ(yos_read(ctx, fd2, buf, 4), 4);
    ASSERT_STR_EQ(buf, data);

    yos_close(ctx, fd1);
    yos_close(ctx, fd2);
    unlink(tmpfile);
    destroy_test_ctx(ctx);
}

TEST(dup2) {
    yos_exec_ctx_t* ctx = create_test_ctx();

    char tmpfile[] = "/tmp/yos_test_XXXXXX";
    int host_fd = mkstemp(tmpfile);
    close(host_fd);

    int fd1 = yos_open(ctx, tmpfile, O_RDWR, 0);
    ASSERT(fd1 >= 3);

    // Dup to specific fd
    int fd2 = yos_dup2(ctx, fd1, 10);
    ASSERT_EQ(fd2, 10);
    ASSERT(ctx->fds[10].host_fd >= 0);

    yos_close(ctx, fd1);
    yos_close(ctx, fd2);
    unlink(tmpfile);
    destroy_test_ctx(ctx);
}

// ============================================================================
// Pipe Tests
// ============================================================================

TEST(pipe) {
    yos_exec_ctx_t* ctx = create_test_ctx();

    int pipefd[2];
    int r = yos_pipe(ctx, pipefd);
    ASSERT_EQ(r, 0);
    ASSERT(pipefd[0] >= 3);
    ASSERT(pipefd[1] >= 3);
    ASSERT(pipefd[0] != pipefd[1]);

    // Write to pipe[1], read from pipe[0]
    const char* data = "pipe test";
    ASSERT_EQ(yos_write(ctx, pipefd[1], data, strlen(data)), (ssize_t)strlen(data));

    char buf[32] = {0};
    ASSERT_EQ(yos_read(ctx, pipefd[0], buf, sizeof(buf) - 1), (ssize_t)strlen(data));
    ASSERT_STR_EQ(buf, data);

    yos_close(ctx, pipefd[0]);
    yos_close(ctx, pipefd[1]);
    destroy_test_ctx(ctx);
}

// ============================================================================
// Stat Tests
// ============================================================================

TEST(stat) {
    yos_exec_ctx_t* ctx = create_test_ctx();

    struct stat st;
    int r = yos_stat(ctx, "/tmp", &st);
    ASSERT_EQ(r, 0);
    ASSERT(S_ISDIR(st.st_mode));

    destroy_test_ctx(ctx);
}

TEST(fstat) {
    yos_exec_ctx_t* ctx = create_test_ctx();

    char tmpfile[] = "/tmp/yos_test_XXXXXX";
    int host_fd = mkstemp(tmpfile);
    write(host_fd, "hello", 5);
    close(host_fd);

    int fd = yos_open(ctx, tmpfile, O_RDONLY, 0);
    ASSERT(fd >= 3);

    struct stat st;
    int r = yos_fstat(ctx, fd, &st);
    ASSERT_EQ(r, 0);
    ASSERT(S_ISREG(st.st_mode));
    ASSERT_EQ(st.st_size, 5);

    yos_close(ctx, fd);
    unlink(tmpfile);
    destroy_test_ctx(ctx);
}

// ============================================================================
// Directory Tests
// ============================================================================

TEST(chdir_getcwd) {
    yos_exec_ctx_t* ctx = create_test_ctx();

    int r = yos_chdir(ctx, "/tmp");
    ASSERT_EQ(r, 0);

    char buf[PATH_MAX];
    char* cwd = yos_getcwd(ctx, buf, sizeof(buf));
    ASSERT(cwd != NULL);
    ASSERT_STR_EQ(cwd, "/tmp");

    destroy_test_ctx(ctx);
}

TEST(mkdir_rmdir) {
    yos_exec_ctx_t* ctx = create_test_ctx();

    const char* testdir = "/tmp/yos_test_dir";

    // Remove if exists
    rmdir(testdir);

    int r = yos_mkdir(ctx, testdir, 0755);
    ASSERT_EQ(r, 0);

    struct stat st;
    ASSERT_EQ(stat(testdir, &st), 0);
    ASSERT(S_ISDIR(st.st_mode));

    r = yos_rmdir(ctx, testdir);
    ASSERT_EQ(r, 0);

    destroy_test_ctx(ctx);
}

TEST(opendir_readdir_closedir) {
    yos_exec_ctx_t* ctx = create_test_ctx();

    void* dir = yos_opendir(ctx, "/tmp");
    ASSERT(dir != NULL);

    // Should be able to read at least one entry
    void* entry = yos_readdir(ctx, dir);
    ASSERT(entry != NULL);

    int r = yos_closedir(ctx, dir);
    ASSERT_EQ(r, 0);

    destroy_test_ctx(ctx);
}

// ============================================================================
// Link Tests
// ============================================================================

TEST(symlink_readlink) {
    yos_exec_ctx_t* ctx = create_test_ctx();

    const char* target = "/tmp";
    const char* linkpath = "/tmp/yos_test_symlink";

    // Remove if exists
    unlink(linkpath);

    int r = yos_symlink(ctx, target, linkpath);
    ASSERT_EQ(r, 0);

    char buf[PATH_MAX];
    ssize_t len = yos_readlink(ctx, linkpath, buf, sizeof(buf) - 1);
    ASSERT(len > 0);
    buf[len] = '\0';
    ASSERT_STR_EQ(buf, target);

    unlink(linkpath);
    destroy_test_ctx(ctx);
}

TEST(rename) {
    yos_exec_ctx_t* ctx = create_test_ctx();

    char oldpath[] = "/tmp/yos_test_rename_old_XXXXXX";
    char newpath[] = "/tmp/yos_test_rename_new";

    int fd = mkstemp(oldpath);
    ASSERT(fd >= 0);
    close(fd);

    unlink(newpath);

    int r = yos_rename(ctx, oldpath, newpath);
    ASSERT_EQ(r, 0);

    struct stat st;
    ASSERT_EQ(stat(newpath, &st), 0);
    ASSERT(stat(oldpath, &st) < 0);

    unlink(newpath);
    destroy_test_ctx(ctx);
}

// ============================================================================
// Permission Tests
// ============================================================================

TEST(umask) {
    yos_exec_ctx_t* ctx = create_test_ctx();

    uint32_t old = yos_umask(ctx, 077);
    ASSERT_EQ(old, 022);
    ASSERT_EQ(ctx->umask, 077);

    old = yos_umask(ctx, 022);
    ASSERT_EQ(old, 077);

    destroy_test_ctx(ctx);
}

TEST(access) {
    yos_exec_ctx_t* ctx = create_test_ctx();

    int r = yos_access(ctx, "/tmp", R_OK | W_OK | X_OK);
    ASSERT_EQ(r, 0);

    r = yos_access(ctx, "/nonexistent", F_OK);
    ASSERT_EQ(r, -ENOENT);

    destroy_test_ctx(ctx);
}

// ============================================================================
// Main
// ============================================================================

int main(void) {
    printf("Running VFS tests...\n");

    // Path resolution
    RUN_TEST(resolve_absolute_path);
    RUN_TEST(resolve_relative_path);

    // File descriptors
    RUN_TEST(alloc_fd);
    RUN_TEST(get_host_fd);

    // Open/close
    RUN_TEST(open_close);
    RUN_TEST(open_nonexistent);

    // Read/write
    RUN_TEST(read_write);

    // Dup
    RUN_TEST(dup);
    RUN_TEST(dup2);

    // Pipe
    RUN_TEST(pipe);

    // Stat
    RUN_TEST(stat);
    RUN_TEST(fstat);

    // Directory
    RUN_TEST(chdir_getcwd);
    RUN_TEST(mkdir_rmdir);
    RUN_TEST(opendir_readdir_closedir);

    // Links
    RUN_TEST(symlink_readlink);
    RUN_TEST(rename);

    // Permissions
    RUN_TEST(umask);
    RUN_TEST(access);

    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
