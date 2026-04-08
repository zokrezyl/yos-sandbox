// runtime.c - Simple wasm3 host that uses fork.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "fork.h"

static uint8_t* g_wasm_bytes;
static size_t g_wasm_size;

m3ApiRawFunction(impl_write) {
    m3ApiReturnType(int32_t);
    m3ApiGetArg(int32_t, fd);
    m3ApiGetArgMem(const uint8_t*, buf);
    m3ApiGetArg(uint32_t, len);
    (void)fd;
    write(1, buf, len);
    m3ApiReturn((int32_t)len);
}

m3ApiRawFunction(impl_exit) {
    m3ApiGetArg(int32_t, code);
    (void)code;
    m3ApiTrap("exit");
}

m3ApiRawFunction(impl_getpid) {
    m3ApiReturnType(int32_t);
    ForkProcess* p = (ForkProcess*)m3_GetUserData(runtime);
    m3ApiReturn(p->pid);
}

// Link function - called for parent and all children
static void link_imports(IM3Module mod, ForkProcess* p) {
    (void)p;
    m3_LinkRawFunction(mod, "env", "fork", "i()", fork_impl);
    m3_LinkRawFunction(mod, "env", "write", "i(i*i)", impl_write);
    m3_LinkRawFunction(mod, "env", "exit", "v(i)", impl_exit);
    m3_LinkRawFunction(mod, "env", "getpid", "i()", impl_getpid);
}

int main(int argc, char** argv) {
    const char* file = argc > 1 ? argv[1] : "test_fork.wasm";
    FILE* f = fopen(file, "rb");
    if (!f) { perror(file); return 1; }
    fseek(f, 0, SEEK_END);
    g_wasm_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    g_wasm_bytes = malloc(g_wasm_size);
    fread(g_wasm_bytes, 1, g_wasm_size, f);
    fclose(f);

    // Create parent process
    ForkProcess* parent = calloc(1, sizeof(ForkProcess));
    parent->pid = fork_next_pid();
    parent->wasm_bytes = g_wasm_bytes;
    parent->wasm_size = g_wasm_size;
    parent->link_fn = link_imports;

    IM3Environment env = m3_NewEnvironment();
    parent->runtime = m3_NewRuntime(env, 64*1024, parent);
    IM3Module mod;
    m3_ParseModule(env, &mod, g_wasm_bytes, g_wasm_size);
    m3_LoadModule(parent->runtime, mod);
    parent->module = mod;
    link_imports(mod, parent);

    // Run _start
    IM3Function start;
    m3_FindFunction(&start, parent->runtime, "_start");
    m3_CallV(start);

    // Handle any forks (spawns children, resumes parent, loops if more forks)
    fork_pump(parent);

    free(g_wasm_bytes);
    return 0;
}
