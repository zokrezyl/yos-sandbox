// Unit tests for yos-process.c
// Tests process management: fork, exec, wait, pid functions, signals

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>

#include "yos-types.h"
#include "yos-process.h"
#include "yos-exec-ctx.h"

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

// ============================================================================
// Test Fixtures
// ============================================================================

static yos_runtime_t* create_test_runtime(void) {
    yos_runtime_t* rt = yos_runtime_create();
    ASSERT(rt != NULL);
    return rt;
}

static void destroy_test_runtime(yos_runtime_t* rt) {
    yos_runtime_destroy(rt);
}

// ============================================================================
// Process Table Tests
// ============================================================================

TEST(proc_alloc_basic) {
    yos_runtime_t* rt = create_test_runtime();

    // Allocate first process (init, ppid=0)
    yos_proc_t* p1 = yos_proc_alloc(rt, 0);
    ASSERT(p1 != NULL);
    ASSERT_EQ(p1->pid, 1);
    ASSERT_EQ(p1->ppid, 0);
    ASSERT_EQ(p1->pgid, p1->pid);
    ASSERT_EQ(p1->sid, p1->pid);
    ASSERT_EQ(p1->state, YOS_PROC_READY);

    // Allocate second process
    yos_proc_t* p2 = yos_proc_alloc(rt, p1->pid);
    ASSERT(p2 != NULL);
    ASSERT_EQ(p2->pid, 2);
    ASSERT_EQ(p2->ppid, 1);
    ASSERT_EQ(p2->state, YOS_PROC_READY);

    destroy_test_runtime(rt);
}

TEST(proc_find) {
    yos_runtime_t* rt = create_test_runtime();

    yos_proc_t* p1 = yos_proc_alloc(rt, 0);
    yos_proc_t* p2 = yos_proc_alloc(rt, p1->pid);

    // Find existing processes
    ASSERT(yos_proc_find(rt, p1->pid) == p1);
    ASSERT(yos_proc_find(rt, p2->pid) == p2);

    // Find non-existent process
    ASSERT(yos_proc_find(rt, 999) == NULL);
    ASSERT(yos_proc_find(rt, -1) == NULL);

    destroy_test_runtime(rt);
}

TEST(proc_exit) {
    yos_runtime_t* rt = create_test_runtime();

    yos_proc_t* p = yos_proc_alloc(rt, 0);
    ASSERT_EQ(p->state, YOS_PROC_READY);
    ASSERT_EQ(p->exited, 0);

    yos_proc_exit(p, 42);

    ASSERT_EQ(p->state, YOS_PROC_ZOMBIE);
    ASSERT_EQ(p->exit_code, 42);
    ASSERT_EQ(p->exited, 1);

    destroy_test_runtime(rt);
}

TEST(proc_table_full) {
    yos_runtime_t* rt = create_test_runtime();

    // Fill process table
    yos_proc_t* procs[YOS_MAX_PROCS];
    for (int i = 0; i < YOS_MAX_PROCS; i++) {
        procs[i] = yos_proc_alloc(rt, 0);
        ASSERT(procs[i] != NULL);
    }

    // Next allocation should fail
    yos_proc_t* overflow = yos_proc_alloc(rt, 0);
    ASSERT(overflow == NULL);

    // Free one slot
    procs[5]->state = YOS_PROC_FREE;

    // Now allocation should succeed
    yos_proc_t* reuse = yos_proc_alloc(rt, 0);
    ASSERT(reuse != NULL);

    destroy_test_runtime(rt);
}

// ============================================================================
// PID Function Tests (require exec context)
// ============================================================================

// Create a minimal exec context for PID tests (no wasm needed)
static yos_exec_ctx_t* create_minimal_ctx(yos_runtime_t* rt) {
    yos_proc_t* proc = yos_proc_alloc(rt, 0);
    if (!proc) return NULL;
    proc->state = YOS_PROC_RUNNING;

    yos_exec_ctx_t* ctx = calloc(1, sizeof(yos_exec_ctx_t));
    if (!ctx) {
        proc->state = YOS_PROC_FREE;
        return NULL;
    }

    ctx->rt = rt;
    ctx->proc = proc;
    return ctx;
}

static void destroy_minimal_ctx(yos_exec_ctx_t* ctx) {
    if (ctx->proc) {
        ctx->proc->state = YOS_PROC_FREE;
    }
    free(ctx);
}

TEST(getpid) {
    yos_runtime_t* rt = create_test_runtime();
    yos_exec_ctx_t* ctx = create_minimal_ctx(rt);

    int32_t pid = yos_getpid(ctx);
    ASSERT_EQ(pid, ctx->proc->pid);

    destroy_minimal_ctx(ctx);
    destroy_test_runtime(rt);
}

TEST(getppid) {
    yos_runtime_t* rt = create_test_runtime();

    // Create parent
    yos_proc_t* parent = yos_proc_alloc(rt, 0);
    parent->state = YOS_PROC_RUNNING;

    // Create child
    yos_proc_t* child = yos_proc_alloc(rt, parent->pid);
    child->state = YOS_PROC_RUNNING;

    yos_exec_ctx_t* ctx = calloc(1, sizeof(yos_exec_ctx_t));
    ctx->rt = rt;
    ctx->proc = child;

    ASSERT_EQ(yos_getppid(ctx), parent->pid);

    free(ctx);
    destroy_test_runtime(rt);
}

TEST(getpgrp) {
    yos_runtime_t* rt = create_test_runtime();
    yos_exec_ctx_t* ctx = create_minimal_ctx(rt);

    int32_t pgrp = yos_getpgrp(ctx);
    ASSERT_EQ(pgrp, ctx->proc->pgid);

    destroy_minimal_ctx(ctx);
    destroy_test_runtime(rt);
}

TEST(setpgid) {
    yos_runtime_t* rt = create_test_runtime();
    yos_exec_ctx_t* ctx = create_minimal_ctx(rt);

    // setpgid(0, 0) should set pgid to pid
    int r = yos_setpgid(ctx, 0, 0);
    ASSERT_EQ(r, 0);
    ASSERT_EQ(ctx->proc->pgid, ctx->proc->pid);

    // setpgid(pid, newpgid)
    r = yos_setpgid(ctx, ctx->proc->pid, 100);
    ASSERT_EQ(r, 0);
    ASSERT_EQ(ctx->proc->pgid, 100);

    // setpgid for non-existent process
    r = yos_setpgid(ctx, 999, 100);
    ASSERT_EQ(r, -ESRCH);

    destroy_minimal_ctx(ctx);
    destroy_test_runtime(rt);
}

TEST(setsid) {
    yos_runtime_t* rt = create_test_runtime();

    // Create parent
    yos_proc_t* parent = yos_proc_alloc(rt, 0);
    parent->state = YOS_PROC_RUNNING;

    // Create child with different sid
    yos_proc_t* child = yos_proc_alloc(rt, parent->pid);
    child->state = YOS_PROC_RUNNING;
    child->sid = parent->sid;  // Child inherits parent's session

    yos_exec_ctx_t* ctx = calloc(1, sizeof(yos_exec_ctx_t));
    ctx->rt = rt;
    ctx->proc = child;

    // Child can create new session (not already session leader)
    int32_t new_sid = yos_setsid(ctx);
    ASSERT_EQ(new_sid, child->pid);
    ASSERT_EQ(child->sid, child->pid);
    ASSERT_EQ(child->pgid, child->pid);

    // Calling setsid again should fail (already session leader)
    int32_t r = yos_setsid(ctx);
    ASSERT_EQ(r, -EPERM);

    free(ctx);
    destroy_test_runtime(rt);
}

TEST(getsid) {
    yos_runtime_t* rt = create_test_runtime();
    yos_exec_ctx_t* ctx = create_minimal_ctx(rt);

    // getsid(0) returns own sid
    int32_t sid = yos_getsid(ctx, 0);
    ASSERT_EQ(sid, ctx->proc->sid);

    // getsid(pid) returns that process's sid
    sid = yos_getsid(ctx, ctx->proc->pid);
    ASSERT_EQ(sid, ctx->proc->sid);

    // getsid for non-existent process
    sid = yos_getsid(ctx, 999);
    ASSERT_EQ(sid, -ESRCH);

    destroy_minimal_ctx(ctx);
    destroy_test_runtime(rt);
}

// ============================================================================
// Signal Tests
// ============================================================================

TEST(kill_signal_zero) {
    yos_runtime_t* rt = create_test_runtime();
    yos_exec_ctx_t* ctx = create_minimal_ctx(rt);

    // Signal 0 just checks if process exists
    int r = yos_kill(ctx, ctx->proc->pid, 0);
    ASSERT_EQ(r, 0);

    // Non-existent process
    r = yos_kill(ctx, 999, 0);
    ASSERT_EQ(r, -ESRCH);

    destroy_minimal_ctx(ctx);
    destroy_test_runtime(rt);
}

TEST(kill_sigterm) {
    yos_runtime_t* rt = create_test_runtime();

    yos_proc_t* parent = yos_proc_alloc(rt, 0);
    parent->state = YOS_PROC_RUNNING;

    yos_proc_t* child = yos_proc_alloc(rt, parent->pid);
    child->state = YOS_PROC_RUNNING;

    yos_exec_ctx_t* ctx = calloc(1, sizeof(yos_exec_ctx_t));
    ctx->rt = rt;
    ctx->proc = parent;

    // Kill child with SIGTERM (15)
    int r = yos_kill(ctx, child->pid, 15);
    ASSERT_EQ(r, 0);
    ASSERT_EQ(child->state, YOS_PROC_ZOMBIE);
    ASSERT_EQ(child->exit_code, 128 + 15);

    free(ctx);
    destroy_test_runtime(rt);
}

TEST(kill_sigkill) {
    yos_runtime_t* rt = create_test_runtime();
    yos_exec_ctx_t* ctx = create_minimal_ctx(rt);

    yos_proc_t* target = yos_proc_alloc(rt, ctx->proc->pid);
    target->state = YOS_PROC_RUNNING;

    // Kill with SIGKILL (9)
    int r = yos_kill(ctx, target->pid, 9);
    ASSERT_EQ(r, 0);
    ASSERT_EQ(target->state, YOS_PROC_ZOMBIE);
    ASSERT_EQ(target->exit_code, 128 + 9);

    destroy_minimal_ctx(ctx);
    destroy_test_runtime(rt);
}

TEST(raise) {
    yos_runtime_t* rt = create_test_runtime();
    yos_exec_ctx_t* ctx = create_minimal_ctx(rt);

    // Raise signal 0 (just check)
    int r = yos_raise(ctx, 0);
    ASSERT_EQ(r, 0);

    destroy_minimal_ctx(ctx);
    destroy_test_runtime(rt);
}

// ============================================================================
// Wait Tests (with zombies, no actual fork)
// ============================================================================

TEST(wait_no_children) {
    yos_runtime_t* rt = create_test_runtime();
    yos_exec_ctx_t* ctx = create_minimal_ctx(rt);

    int status;
    int32_t r = yos_wait(ctx, &status);
    ASSERT_EQ(r, -ECHILD);

    destroy_minimal_ctx(ctx);
    destroy_test_runtime(rt);
}

TEST(waitpid_specific_child) {
    yos_runtime_t* rt = create_test_runtime();

    // Create parent
    yos_proc_t* parent = yos_proc_alloc(rt, 0);
    parent->state = YOS_PROC_RUNNING;

    // Create child and make it zombie
    yos_proc_t* child = yos_proc_alloc(rt, parent->pid);
    child->state = YOS_PROC_RUNNING;
    yos_proc_exit(child, 42);  // Makes it zombie

    yos_exec_ctx_t* ctx = calloc(1, sizeof(yos_exec_ctx_t));
    ctx->rt = rt;
    ctx->proc = parent;

    int status = 0;
    int32_t r = yos_waitpid(ctx, child->pid, &status, 0);
    ASSERT_EQ(r, child->pid);
    ASSERT_EQ(WEXITSTATUS(status), 42);
    ASSERT_EQ(child->state, YOS_PROC_FREE);  // Reaped

    free(ctx);
    destroy_test_runtime(rt);
}

TEST(waitpid_wnohang) {
    yos_runtime_t* rt = create_test_runtime();

    yos_proc_t* parent = yos_proc_alloc(rt, 0);
    parent->state = YOS_PROC_RUNNING;

    // Child is running, not zombie
    yos_proc_t* child = yos_proc_alloc(rt, parent->pid);
    child->state = YOS_PROC_RUNNING;

    yos_exec_ctx_t* ctx = calloc(1, sizeof(yos_exec_ctx_t));
    ctx->rt = rt;
    ctx->proc = parent;

    int status = 0;
    // WNOHANG = 1
    int32_t r = yos_waitpid(ctx, child->pid, &status, 1);
    ASSERT_EQ(r, 0);  // Child exists but not ready
    ASSERT_EQ(child->state, YOS_PROC_RUNNING);  // Not reaped

    free(ctx);
    destroy_test_runtime(rt);
}

TEST(waitpid_not_my_child) {
    yos_runtime_t* rt = create_test_runtime();

    yos_proc_t* p1 = yos_proc_alloc(rt, 0);
    p1->state = YOS_PROC_RUNNING;

    yos_proc_t* p2 = yos_proc_alloc(rt, 0);  // ppid=0, not child of p1
    p2->state = YOS_PROC_RUNNING;
    yos_proc_exit(p2, 0);

    yos_exec_ctx_t* ctx = calloc(1, sizeof(yos_exec_ctx_t));
    ctx->rt = rt;
    ctx->proc = p1;

    int status = 0;
    int32_t r = yos_waitpid(ctx, p2->pid, &status, 0);
    ASSERT_EQ(r, -ECHILD);  // Not our child

    free(ctx);
    destroy_test_runtime(rt);
}

TEST(wait_any_child) {
    yos_runtime_t* rt = create_test_runtime();

    yos_proc_t* parent = yos_proc_alloc(rt, 0);
    parent->state = YOS_PROC_RUNNING;

    // Create multiple children
    yos_proc_t* c1 = yos_proc_alloc(rt, parent->pid);
    c1->state = YOS_PROC_RUNNING;

    yos_proc_t* c2 = yos_proc_alloc(rt, parent->pid);
    c2->state = YOS_PROC_RUNNING;

    // Make c2 a zombie
    yos_proc_exit(c2, 99);

    yos_exec_ctx_t* ctx = calloc(1, sizeof(yos_exec_ctx_t));
    ctx->rt = rt;
    ctx->proc = parent;

    int status = 0;
    int32_t r = yos_wait(ctx, &status);  // wait(-1, ...)
    ASSERT_EQ(r, c2->pid);  // Should reap c2
    ASSERT_EQ(WEXITSTATUS(status), 99);

    free(ctx);
    destroy_test_runtime(rt);
}

// ============================================================================
// Exit Tests
// ============================================================================

TEST(exit_sets_proc_state) {
    yos_runtime_t* rt = create_test_runtime();
    yos_exec_ctx_t* ctx = create_minimal_ctx(rt);

    ASSERT_EQ(ctx->proc->state, YOS_PROC_RUNNING);

    yos_exit(ctx, 123);

    ASSERT_EQ(ctx->proc->state, YOS_PROC_ZOMBIE);
    ASSERT_EQ(ctx->proc->exit_code, 123);

    destroy_minimal_ctx(ctx);
    destroy_test_runtime(rt);
}

// ============================================================================
// Main
// ============================================================================

int main(void) {
    printf("Running process tests...\n");

    // Process table
    RUN_TEST(proc_alloc_basic);
    RUN_TEST(proc_find);
    RUN_TEST(proc_exit);
    RUN_TEST(proc_table_full);

    // PID functions
    RUN_TEST(getpid);
    RUN_TEST(getppid);
    RUN_TEST(getpgrp);
    RUN_TEST(setpgid);
    RUN_TEST(setsid);
    RUN_TEST(getsid);

    // Signals
    RUN_TEST(kill_signal_zero);
    RUN_TEST(kill_sigterm);
    RUN_TEST(kill_sigkill);
    RUN_TEST(raise);

    // Wait
    RUN_TEST(wait_no_children);
    RUN_TEST(waitpid_specific_child);
    RUN_TEST(waitpid_wnohang);
    RUN_TEST(waitpid_not_my_child);
    RUN_TEST(wait_any_child);

    // Exit
    RUN_TEST(exit_sets_proc_state);

    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
