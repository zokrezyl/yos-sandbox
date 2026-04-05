// Unit tests for yos-runtime.c
// Tests the pure C VFS and process management implementation

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

#include "yos-runtime.h"

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

// ============================================================================
// Context Tests
// ============================================================================

TEST(ctx_create_destroy) {
    yos_ctx_t* ctx = yos_ctx_create();
    ASSERT(ctx != NULL);
    ASSERT_STR_EQ(ctx->cwd, "/");
    ASSERT_EQ(ctx->umask, 0022);

    // All fds should be -1 (unused)
    for (int i = 0; i < YOS_MAX_FDS; i++) {
        ASSERT_EQ(ctx->fds[i].host_fd, -1);
    }

    yos_ctx_destroy(ctx);
}

TEST(ctx_init_stdio) {
    yos_ctx_t* ctx = yos_ctx_create();
    ASSERT(ctx != NULL);

    yos_ctx_init_stdio(ctx);

    // stdin/stdout/stderr should be mapped
    ASSERT_EQ(ctx->fds[0].host_fd, 0);
    ASSERT_EQ(ctx->fds[1].host_fd, 1);
    ASSERT_EQ(ctx->fds[2].host_fd, 2);

    yos_ctx_destroy(ctx);
}

// ============================================================================
// Path Resolution Tests
// ============================================================================

TEST(resolve_absolute_path) {
    yos_ctx_t* ctx = yos_ctx_create();
    char buf[PATH_MAX];

    char* result = yos_resolve_path(ctx, "/usr/bin", buf, sizeof(buf));
    ASSERT(result != NULL);
    ASSERT_STR_EQ(result, "/usr/bin");

    yos_ctx_destroy(ctx);
}

TEST(resolve_relative_path) {
    yos_ctx_t* ctx = yos_ctx_create();
    char buf[PATH_MAX];

    // cwd is "/" by default
    char* result = yos_resolve_path(ctx, "tmp", buf, sizeof(buf));
    ASSERT(result != NULL);
    ASSERT_STR_EQ(result, "/tmp");

    yos_ctx_destroy(ctx);
}

TEST(resolve_relative_with_cwd) {
    yos_ctx_t* ctx = yos_ctx_create();
    char buf[PATH_MAX];

    strcpy(ctx->cwd, "/home/user");

    char* result = yos_resolve_path(ctx, "file.txt", buf, sizeof(buf));
    ASSERT(result != NULL);
    ASSERT_STR_EQ(result, "/home/user/file.txt");

    yos_ctx_destroy(ctx);
}

// ============================================================================
// File Operations Tests
// ============================================================================

TEST(open_close) {
    yos_ctx_t* ctx = yos_ctx_create();
    yos_ctx_init_stdio(ctx);

    // Open /dev/null
    int fd = yos_open(ctx, "/dev/null", O_RDONLY, 0);
    ASSERT(fd >= 3);  // 0,1,2 are stdio

    int r = yos_close(ctx, fd);
    ASSERT_EQ(r, 0);

    // Double close should fail
    r = yos_close(ctx, fd);
    ASSERT_EQ(r, -EBADF);

    yos_ctx_destroy(ctx);
}

TEST(open_nonexistent) {
    yos_ctx_t* ctx = yos_ctx_create();

    int fd = yos_open(ctx, "/nonexistent_file_12345", O_RDONLY, 0);
    ASSERT_EQ(fd, -ENOENT);

    yos_ctx_destroy(ctx);
}

TEST(read_write) {
    yos_ctx_t* ctx = yos_ctx_create();
    yos_ctx_init_stdio(ctx);

    // Create temp file
    char tmpfile[] = "/tmp/yos_test_XXXXXX";
    int host_fd = mkstemp(tmpfile);
    ASSERT(host_fd >= 0);
    close(host_fd);

    // Open for writing
    int fd = yos_open(ctx, tmpfile, O_WRONLY | O_TRUNC, 0);
    ASSERT(fd >= 3);

    // Write data
    const char* data = "Hello, YOS!";
    ssize_t written = yos_write(ctx, fd, data, strlen(data));
    ASSERT_EQ(written, (ssize_t)strlen(data));

    yos_close(ctx, fd);

    // Open for reading
    fd = yos_open(ctx, tmpfile, O_RDONLY, 0);
    ASSERT(fd >= 3);

    // Read data back
    char buf[64] = {0};
    ssize_t readn = yos_read(ctx, fd, buf, sizeof(buf));
    ASSERT_EQ(readn, (ssize_t)strlen(data));
    ASSERT_STR_EQ(buf, data);

    yos_close(ctx, fd);
    unlink(tmpfile);

    yos_ctx_destroy(ctx);
}

TEST(stat_file) {
    yos_ctx_t* ctx = yos_ctx_create();

    struct stat st;
    int r = yos_stat(ctx, "/dev/null", &st);
    ASSERT_EQ(r, 0);
    ASSERT(S_ISCHR(st.st_mode));  // /dev/null is a character device

    yos_ctx_destroy(ctx);
}

TEST(fstat_fd) {
    yos_ctx_t* ctx = yos_ctx_create();
    yos_ctx_init_stdio(ctx);

    int fd = yos_open(ctx, "/dev/null", O_RDONLY, 0);
    ASSERT(fd >= 3);

    struct stat st;
    int r = yos_fstat(ctx, fd, &st);
    ASSERT_EQ(r, 0);
    ASSERT(S_ISCHR(st.st_mode));

    yos_close(ctx, fd);
    yos_ctx_destroy(ctx);
}

TEST(lseek_file) {
    yos_ctx_t* ctx = yos_ctx_create();
    yos_ctx_init_stdio(ctx);

    // Create temp file with content
    char tmpfile[] = "/tmp/yos_test_XXXXXX";
    int host_fd = mkstemp(tmpfile);
    write(host_fd, "0123456789", 10);
    close(host_fd);

    int fd = yos_open(ctx, tmpfile, O_RDONLY, 0);
    ASSERT(fd >= 3);

    // Seek to position 5
    off_t pos = yos_lseek(ctx, fd, 5, SEEK_SET);
    ASSERT_EQ(pos, 5);

    // Read from position 5
    char buf[10] = {0};
    ssize_t r = yos_read(ctx, fd, buf, 5);
    ASSERT_EQ(r, 5);
    ASSERT_STR_EQ(buf, "56789");

    yos_close(ctx, fd);
    unlink(tmpfile);
    yos_ctx_destroy(ctx);
}

// ============================================================================
// Directory Operations Tests
// ============================================================================

TEST(chdir_getcwd) {
    yos_ctx_t* ctx = yos_ctx_create();

    int r = yos_chdir(ctx, "/tmp");
    ASSERT_EQ(r, 0);

    char buf[PATH_MAX];
    char* cwd = yos_getcwd(ctx, buf, sizeof(buf));
    ASSERT(cwd != NULL);
    ASSERT_STR_EQ(cwd, "/tmp");

    yos_ctx_destroy(ctx);
}

TEST(chdir_nonexistent) {
    yos_ctx_t* ctx = yos_ctx_create();

    int r = yos_chdir(ctx, "/nonexistent_dir_12345");
    ASSERT_EQ(r, -ENOENT);

    // cwd should be unchanged
    ASSERT_STR_EQ(ctx->cwd, "/");

    yos_ctx_destroy(ctx);
}

TEST(mkdir_rmdir) {
    yos_ctx_t* ctx = yos_ctx_create();

    const char* dir = "/tmp/yos_test_dir_12345";

    // Remove if exists
    rmdir(dir);

    int r = yos_mkdir(ctx, dir, 0755);
    ASSERT_EQ(r, 0);

    // Verify it exists
    struct stat st;
    r = yos_stat(ctx, dir, &st);
    ASSERT_EQ(r, 0);
    ASSERT(S_ISDIR(st.st_mode));

    // Remove it
    r = yos_rmdir(ctx, dir);
    ASSERT_EQ(r, 0);

    // Verify it's gone
    r = yos_stat(ctx, dir, &st);
    ASSERT_EQ(r, -ENOENT);

    yos_ctx_destroy(ctx);
}

TEST(opendir_readdir_closedir) {
    yos_ctx_t* ctx = yos_ctx_create();

    void* dir = yos_opendir(ctx, "/tmp");
    ASSERT(dir != NULL);

    // Read at least one entry
    struct dirent* entry = yos_readdir(ctx, dir);
    ASSERT(entry != NULL);
    ASSERT(strlen(entry->d_name) > 0);

    int r = yos_closedir(ctx, dir);
    ASSERT_EQ(r, 0);

    yos_ctx_destroy(ctx);
}

// ============================================================================
// FD Operations Tests
// ============================================================================

TEST(dup_fd) {
    yos_ctx_t* ctx = yos_ctx_create();
    yos_ctx_init_stdio(ctx);

    int fd = yos_open(ctx, "/dev/null", O_RDONLY, 0);
    ASSERT(fd >= 3);

    int fd2 = yos_dup(ctx, fd);
    ASSERT(fd2 >= 3);
    ASSERT(fd2 != fd);

    // Both fds should be valid
    struct stat st1, st2;
    ASSERT_EQ(yos_fstat(ctx, fd, &st1), 0);
    ASSERT_EQ(yos_fstat(ctx, fd2, &st2), 0);

    yos_close(ctx, fd);
    yos_close(ctx, fd2);
    yos_ctx_destroy(ctx);
}

TEST(dup2_fd) {
    yos_ctx_t* ctx = yos_ctx_create();
    yos_ctx_init_stdio(ctx);

    int fd = yos_open(ctx, "/dev/null", O_RDONLY, 0);
    ASSERT(fd >= 3);

    // dup2 to specific fd
    int target_fd = 10;
    int r = yos_dup2(ctx, fd, target_fd);
    ASSERT_EQ(r, target_fd);

    // New fd should be valid
    struct stat st;
    ASSERT_EQ(yos_fstat(ctx, target_fd, &st), 0);

    yos_close(ctx, fd);
    yos_close(ctx, target_fd);
    yos_ctx_destroy(ctx);
}

TEST(pipe_create) {
    yos_ctx_t* ctx = yos_ctx_create();
    yos_ctx_init_stdio(ctx);

    int pipefd[2];
    int r = yos_pipe(ctx, pipefd);
    ASSERT_EQ(r, 0);
    ASSERT(pipefd[0] >= 3);
    ASSERT(pipefd[1] >= 3);
    ASSERT(pipefd[0] != pipefd[1]);

    // Write to pipe
    const char* msg = "pipe test";
    ssize_t w = yos_write(ctx, pipefd[1], msg, strlen(msg));
    ASSERT_EQ(w, (ssize_t)strlen(msg));

    // Read from pipe
    char buf[64] = {0};
    ssize_t rd = yos_read(ctx, pipefd[0], buf, sizeof(buf));
    ASSERT_EQ(rd, (ssize_t)strlen(msg));
    ASSERT_STR_EQ(buf, msg);

    yos_close(ctx, pipefd[0]);
    yos_close(ctx, pipefd[1]);
    yos_ctx_destroy(ctx);
}

// ============================================================================
// Access/Permission Tests
// ============================================================================

TEST(access_file) {
    yos_ctx_t* ctx = yos_ctx_create();

    // /dev/null should be readable and writable
    int r = yos_access(ctx, "/dev/null", R_OK | W_OK);
    ASSERT_EQ(r, 0);

    // Nonexistent file
    r = yos_access(ctx, "/nonexistent_12345", F_OK);
    ASSERT_EQ(r, -ENOENT);

    yos_ctx_destroy(ctx);
}

TEST(umask_set) {
    yos_ctx_t* ctx = yos_ctx_create();

    // Default umask is 0022
    uint32_t old = yos_umask(ctx, 0077);
    ASSERT_EQ(old, 0022);

    old = yos_umask(ctx, 0022);
    ASSERT_EQ(old, 0077);

    yos_ctx_destroy(ctx);
}

TEST(isatty_check) {
    yos_ctx_t* ctx = yos_ctx_create();
    yos_ctx_init_stdio(ctx);

    // /dev/null is not a tty
    int fd = yos_open(ctx, "/dev/null", O_RDONLY, 0);
    ASSERT(fd >= 3);

    int r = yos_isatty(ctx, fd);
    ASSERT_EQ(r, 0);

    yos_close(ctx, fd);
    yos_ctx_destroy(ctx);
}

// ============================================================================
// User/Group Tests
// ============================================================================

TEST(getuid_gid) {
    yos_ctx_t* ctx = yos_ctx_create();

    // Sandbox returns fixed values
    ASSERT_EQ(yos_getuid(ctx), 1000);
    ASSERT_EQ(yos_geteuid(ctx), 1000);
    ASSERT_EQ(yos_getgid(ctx), 1000);
    ASSERT_EQ(yos_getegid(ctx), 1000);

    yos_ctx_destroy(ctx);
}

// ============================================================================
// Process Tests
// ============================================================================

TEST(getpid_getppid) {
    yos_ctx_t* ctx = yos_ctx_create();

    // Without a process attached, returns 1
    ASSERT_EQ(yos_getpid(ctx), 1);
    ASSERT_EQ(yos_getppid(ctx), 0);

    yos_ctx_destroy(ctx);
}

// ============================================================================
// Main
// ============================================================================

int main(void) {
    printf("YOS Runtime Unit Tests\n");
    printf("======================\n\n");

    // Initialize YOS
    yos_init();

    printf("Context tests:\n");
    RUN_TEST(ctx_create_destroy);
    RUN_TEST(ctx_init_stdio);

    printf("\nPath resolution tests:\n");
    RUN_TEST(resolve_absolute_path);
    RUN_TEST(resolve_relative_path);
    RUN_TEST(resolve_relative_with_cwd);

    printf("\nFile operations tests:\n");
    RUN_TEST(open_close);
    RUN_TEST(open_nonexistent);
    RUN_TEST(read_write);
    RUN_TEST(stat_file);
    RUN_TEST(fstat_fd);
    RUN_TEST(lseek_file);

    printf("\nDirectory operations tests:\n");
    RUN_TEST(chdir_getcwd);
    RUN_TEST(chdir_nonexistent);
    RUN_TEST(mkdir_rmdir);
    RUN_TEST(opendir_readdir_closedir);

    printf("\nFD operations tests:\n");
    RUN_TEST(dup_fd);
    RUN_TEST(dup2_fd);
    RUN_TEST(pipe_create);

    printf("\nAccess/Permission tests:\n");
    RUN_TEST(access_file);
    RUN_TEST(umask_set);
    RUN_TEST(isatty_check);

    printf("\nUser/Group tests:\n");
    RUN_TEST(getuid_gid);

    printf("\nProcess tests:\n");
    RUN_TEST(getpid_getppid);

    printf("\n======================\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_run);

    return tests_passed == tests_run ? 0 : 1;
}
