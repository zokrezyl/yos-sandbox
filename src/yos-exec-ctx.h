// YOS Execution Context Management
//
// Functions to create, initialize, run, and destroy execution contexts.
// See yos-types.h for the relationship between yos_exec_ctx_t and yos_proc_t.
//
#ifndef YOS_EXEC_CTX_H
#define YOS_EXEC_CTX_H

#include "yos-types.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// yos_runtime_t functions
// ============================================================================

yos_runtime_t* yos_runtime_create(void);
int yos_runtime_load_wasm(yos_runtime_t* rt, const char* path);
void yos_runtime_destroy(yos_runtime_t* rt);

// ============================================================================
// yos_exec_ctx_t functions
// ============================================================================

// Create initial exec context (pid=1, the init process)
yos_exec_ctx_t* yos_exec_ctx_create(yos_runtime_t* rt);

// Create child exec context from fork (with copied memory)
yos_exec_ctx_t* yos_exec_ctx_fork(yos_runtime_t* rt, yos_proc_t* child_proc,
                                   uint8_t* memory_snapshot, size_t memory_size);

// Initialize stdio (fd 0,1,2)
void yos_exec_ctx_init_stdio(yos_exec_ctx_t* ctx);

// Run WASM entry point, returns exit code
int yos_exec_ctx_run(yos_exec_ctx_t* ctx);

// Destroy exec context and free resources
void yos_exec_ctx_destroy(yos_exec_ctx_t* ctx);

#ifdef __cplusplus
}
#endif

#endif // YOS_EXEC_CTX_H
