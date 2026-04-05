// Smoke test runner for generated libc wrappers
// Loads a WASM file and runs _start, linking all generated libc functions

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "wasm3.h"
#include "m3_env.h"
#include "yos-runtime.h"

// Generated libc wrappers
void linkLibcFunctions(IM3Module module);

static int runTest(const char* wasmPath) {
    FILE* f = fopen(wasmPath, "rb");
    if (!f) {
        fprintf(stderr, "Cannot open: %s\n", wasmPath);
        return 1;
    }

    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);

    uint8_t* wasm = malloc(size);
    if (!wasm) {
        fclose(f);
        return 1;
    }
    if (fread(wasm, 1, size, f) != size) {
        fprintf(stderr, "Failed to read: %s\n", wasmPath);
        free(wasm);
        fclose(f);
        return 1;
    }
    fclose(f);

    // Create YOS context for hooked functions
    yos_ctx_t* ctx = yos_ctx_create();
    if (!ctx) {
        fprintf(stderr, "Failed to create yos context\n");
        free(wasm);
        return 1;
    }
    yos_ctx_init_stdio(ctx);

    IM3Environment env = m3_NewEnvironment();
    // Pass ctx as user data to wasm3 runtime
    IM3Runtime runtime = m3_NewRuntime(env, 64 * 1024, ctx);

    // Store wasm runtime handle in ctx for memory access
    ctx->wasm_runtime = runtime;

    IM3Module module = NULL;
    M3Result result = m3_ParseModule(env, &module, wasm, size);
    if (result) {
        fprintf(stderr, "Parse error: %s\n", result);
        free(wasm);
        yos_ctx_destroy(ctx);
        m3_FreeRuntime(runtime);
        m3_FreeEnvironment(env);
        return 1;
    }

    result = m3_LoadModule(runtime, module);
    if (result) {
        fprintf(stderr, "Load error: %s\n", result);
        free(wasm);
        yos_ctx_destroy(ctx);
        m3_FreeRuntime(runtime);
        m3_FreeEnvironment(env);
        return 1;
    }

    // Link our generated libc wrappers
    linkLibcFunctions(module);

    IM3Function startFn = NULL;
    result = m3_FindFunction(&startFn, runtime, "_start");
    if (result) {
        fprintf(stderr, "No _start: %s\n", result);
        free(wasm);
        yos_ctx_destroy(ctx);
        m3_FreeRuntime(runtime);
        m3_FreeEnvironment(env);
        return 1;
    }

    result = m3_CallV(startFn);

    int exitCode = 0;
    if (result) {
        if (strstr(result, "exit")) {
            exitCode = 0; // Normal exit
        } else {
            fprintf(stderr, "Runtime error: %s\n", result);
            exitCode = 1;
        }
    }

    free(wasm);
    yos_ctx_destroy(ctx);
    m3_FreeRuntime(runtime);
    m3_FreeEnvironment(env);
    return exitCode;
}

int main(int argc, char** argv) {
    // Initialize YOS runtime (logging, process table, etc.)
    yos_init();

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <test.wasm>\n", argv[0]);
        return 1;
    }
    return runTest(argv[1]);
}
