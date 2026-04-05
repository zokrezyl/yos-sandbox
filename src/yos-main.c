// YOS Main - Entry point for WASM runtime
//
// Usage: yos <program.wasm> [args...]

#include <stdio.h>
#include <stdlib.h>

#include "yos-types.h"
#include "yos-log.h"
#include "yos-exec-ctx.h"

int main(int argc, char** argv) {
    yos_log_init();

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <program.wasm> [args...]\n", argv[0]);
        return 1;
    }

    const char* wasm_path = argv[1];

    // Create global runtime
    yos_runtime_t* rt = yos_runtime_create();
    if (!rt) {
        fprintf(stderr, "Failed to create runtime\n");
        return 1;
    }

    // Store command line args for WASM program
    rt->argc = argc - 1;
    rt->argv = argv + 1;

    // Load WASM binary
    if (yos_runtime_load_wasm(rt, wasm_path) != 0) {
        yos_runtime_destroy(rt);
        return 1;
    }

    // Create init process exec context
    yos_exec_ctx_t* ctx = yos_exec_ctx_create(rt);
    if (!ctx) {
        fprintf(stderr, "Failed to create exec context\n");
        yos_runtime_destroy(rt);
        return 1;
    }

    // Initialize stdio
    yos_exec_ctx_init_stdio(ctx);

    // Run WASM program
    int exit_code = yos_exec_ctx_run(ctx);

    // Cleanup
    yos_exec_ctx_destroy(ctx);
    yos_runtime_destroy(rt);

    return exit_code;
}
