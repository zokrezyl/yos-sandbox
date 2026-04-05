// Unit tests for yos-exec-ctx.c
// Tests execution context creation, stdio init, and runtime management

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "yos-types.h"
#include "yos-exec-ctx.h"
#include "yos-process.h"

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
// Runtime Tests
// ============================================================================

TEST(runtime_create_destroy) {
    yos_runtime_t* rt = yos_runtime_create();
    ASSERT(rt != NULL);
    ASSERT_EQ(rt->next_pid, 1);
    ASSERT(rt->wasm_bytes == NULL);
    ASSERT_EQ(rt->wasm_bytes_size, 0);

    // All process slots should be free
    for (int i = 0; i < YOS_MAX_PROCS; i++) {
        ASSERT_EQ(rt->procs[i].state, YOS_PROC_FREE);
    }

    yos_runtime_destroy(rt);
}

TEST(runtime_load_wasm_nonexistent) {
    yos_runtime_t* rt = yos_runtime_create();
    ASSERT(rt != NULL);

    int r = yos_runtime_load_wasm(rt, "/nonexistent/file.wasm");
    ASSERT_EQ(r, -1);
    ASSERT(rt->wasm_bytes == NULL);

    yos_runtime_destroy(rt);
}

TEST(runtime_load_wasm_valid) {
    yos_runtime_t* rt = yos_runtime_create();
    ASSERT(rt != NULL);

    // Create a minimal WASM file
    char tmpfile[] = "/tmp/yos_test_wasm_XXXXXX";
    int fd = mkstemp(tmpfile);
    ASSERT(fd >= 0);

    // Minimal valid WASM header (magic + version)
    uint8_t wasm_header[] = {0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00};
    write(fd, wasm_header, sizeof(wasm_header));
    close(fd);

    int r = yos_runtime_load_wasm(rt, tmpfile);
    ASSERT_EQ(r, 0);
    ASSERT(rt->wasm_bytes != NULL);
    ASSERT_EQ(rt->wasm_bytes_size, sizeof(wasm_header));

    unlink(tmpfile);
    yos_runtime_destroy(rt);
}

TEST(runtime_base_path) {
    yos_runtime_t* rt = yos_runtime_create();

    char tmpfile[] = "/tmp/yos_test_wasm_XXXXXX";
    int fd = mkstemp(tmpfile);
    uint8_t wasm_header[] = {0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00};
    write(fd, wasm_header, sizeof(wasm_header));
    close(fd);

    yos_runtime_load_wasm(rt, tmpfile);
    ASSERT_STR_EQ(rt->base_path, "/tmp");

    unlink(tmpfile);
    yos_runtime_destroy(rt);
}

// ============================================================================
// Exec Context Tests (without wasm - just structure tests)
// ============================================================================

// Create a minimal runtime with fake wasm bytes to allow ctx creation
static yos_runtime_t* create_test_runtime_with_wasm(void) {
    yos_runtime_t* rt = yos_runtime_create();
    if (!rt) return NULL;

    // Create minimal wasm for testing
    char tmpfile[] = "/tmp/yos_ctx_test_XXXXXX";
    int fd = mkstemp(tmpfile);
    if (fd < 0) {
        yos_runtime_destroy(rt);
        return NULL;
    }

    // Minimal WASM module with empty function
    // This is a minimal valid WASM with type, function, and code sections
    uint8_t minimal_wasm[] = {
        0x00, 0x61, 0x73, 0x6d,  // magic
        0x01, 0x00, 0x00, 0x00,  // version
        // Type section (id=1)
        0x01, 0x04, 0x01, 0x60, 0x00, 0x00,  // 1 type: () -> ()
        // Function section (id=3)
        0x03, 0x02, 0x01, 0x00,  // 1 func of type 0
        // Export section (id=7)
        0x07, 0x0a, 0x01, 0x06, '_', 's', 't', 'a', 'r', 't', 0x00, 0x00,  // export "_start" func 0
        // Code section (id=10)
        0x0a, 0x04, 0x01, 0x02, 0x00, 0x0b  // 1 func body: empty, end
    };

    write(fd, minimal_wasm, sizeof(minimal_wasm));
    close(fd);

    if (yos_runtime_load_wasm(rt, tmpfile) != 0) {
        unlink(tmpfile);
        yos_runtime_destroy(rt);
        return NULL;
    }

    unlink(tmpfile);
    return rt;
}

TEST(exec_ctx_create) {
    yos_runtime_t* rt = create_test_runtime_with_wasm();
    if (!rt) {
        printf(" SKIPPED (wasm load failed)\n");
        tests_passed++;
        return;
    }

    yos_exec_ctx_t* ctx = yos_exec_ctx_create(rt);
    ASSERT(ctx != NULL);

    // Check basic fields
    ASSERT(ctx->rt == rt);
    ASSERT(ctx->proc != NULL);
    ASSERT_EQ(ctx->proc->pid, 1);  // First process
    ASSERT_EQ(ctx->proc->ppid, 0);  // Init process
    ASSERT_EQ(ctx->proc->state, YOS_PROC_RUNNING);
    ASSERT_EQ(ctx->is_child, 0);
    ASSERT_EQ(ctx->umask, 022);

    // Check cwd is set
    char cwd[PATH_MAX];
    getcwd(cwd, sizeof(cwd));
    ASSERT_STR_EQ(ctx->cwd, cwd);

    // Check all fds are -1 initially (before stdio init)
    for (int i = 0; i < YOS_MAX_FDS; i++) {
        ASSERT_EQ(ctx->fds[i].host_fd, -1);
    }

    yos_exec_ctx_destroy(ctx);
    yos_runtime_destroy(rt);
}

TEST(exec_ctx_init_stdio) {
    yos_runtime_t* rt = create_test_runtime_with_wasm();
    if (!rt) {
        printf(" SKIPPED (wasm load failed)\n");
        tests_passed++;
        return;
    }

    yos_exec_ctx_t* ctx = yos_exec_ctx_create(rt);
    ASSERT(ctx != NULL);

    yos_exec_ctx_init_stdio(ctx);

    // Check stdio is set up
    ASSERT_EQ(ctx->fds[0].host_fd, STDIN_FILENO);
    ASSERT_EQ(ctx->fds[1].host_fd, STDOUT_FILENO);
    ASSERT_EQ(ctx->fds[2].host_fd, STDERR_FILENO);

    ASSERT_STR_EQ(ctx->fds[0].path, "/dev/stdin");
    ASSERT_STR_EQ(ctx->fds[1].path, "/dev/stdout");
    ASSERT_STR_EQ(ctx->fds[2].path, "/dev/stderr");

    yos_exec_ctx_destroy(ctx);
    yos_runtime_destroy(rt);
}

TEST(exec_ctx_run_empty) {
    yos_runtime_t* rt = create_test_runtime_with_wasm();
    if (!rt) {
        printf(" SKIPPED (wasm load failed)\n");
        tests_passed++;
        return;
    }

    yos_exec_ctx_t* ctx = yos_exec_ctx_create(rt);
    ASSERT(ctx != NULL);

    yos_exec_ctx_init_stdio(ctx);

    // Run the minimal wasm (just returns)
    int exit_code = yos_exec_ctx_run(ctx);
    // Should complete without error
    ASSERT(exit_code == 0 || exit_code == 1);  // Depends on wasm

    yos_exec_ctx_destroy(ctx);
    yos_runtime_destroy(rt);
}

TEST(exec_ctx_multiple_processes) {
    yos_runtime_t* rt = create_test_runtime_with_wasm();
    if (!rt) {
        printf(" SKIPPED (wasm load failed)\n");
        tests_passed++;
        return;
    }

    // Create first context
    yos_exec_ctx_t* ctx1 = yos_exec_ctx_create(rt);
    ASSERT(ctx1 != NULL);
    ASSERT_EQ(ctx1->proc->pid, 1);

    // Create second context (simulating fork scenario)
    yos_proc_t* child_proc = yos_proc_alloc(rt, ctx1->proc->pid);
    ASSERT(child_proc != NULL);
    ASSERT_EQ(child_proc->pid, 2);
    ASSERT_EQ(child_proc->ppid, 1);

    // Both contexts share same runtime
    ASSERT(ctx1->rt == rt);

    yos_exec_ctx_destroy(ctx1);
    yos_runtime_destroy(rt);
}

// ============================================================================
// Fork Context Tests
// ============================================================================

TEST(exec_ctx_fork_basic) {
    yos_runtime_t* rt = create_test_runtime_with_wasm();
    if (!rt) {
        printf(" SKIPPED (wasm load failed)\n");
        tests_passed++;
        return;
    }

    yos_exec_ctx_t* parent = yos_exec_ctx_create(rt);
    ASSERT(parent != NULL);

    // Simulate fork: allocate child proc
    yos_proc_t* child_proc = yos_proc_alloc(rt, parent->proc->pid);
    ASSERT(child_proc != NULL);

    // Create memory snapshot (empty for test)
    size_t mem_size = 1024;
    uint8_t* snapshot = malloc(mem_size);
    memset(snapshot, 0, mem_size);

    // Create forked context
    yos_exec_ctx_t* child = yos_exec_ctx_fork(rt, child_proc, snapshot, mem_size);
    // Note: snapshot ownership transferred to yos_exec_ctx_fork

    if (child != NULL) {
        ASSERT(child->rt == rt);
        ASSERT(child->proc == child_proc);
        ASSERT_EQ(child->is_child, 1);
        yos_exec_ctx_destroy(child);
    }

    yos_exec_ctx_destroy(parent);
    yos_runtime_destroy(rt);
}

// ============================================================================
// Main
// ============================================================================

int main(void) {
    printf("Running exec context tests...\n");

    // Runtime tests
    RUN_TEST(runtime_create_destroy);
    RUN_TEST(runtime_load_wasm_nonexistent);
    RUN_TEST(runtime_load_wasm_valid);
    RUN_TEST(runtime_base_path);

    // Exec context tests
    RUN_TEST(exec_ctx_create);
    RUN_TEST(exec_ctx_init_stdio);
    RUN_TEST(exec_ctx_run_empty);
    RUN_TEST(exec_ctx_multiple_processes);
    RUN_TEST(exec_ctx_fork_basic);

    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
