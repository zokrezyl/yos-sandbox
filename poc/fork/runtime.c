// runtime.c - Host runtime with wasm3 + asyncify fork
// Compile: gcc -o runtime runtime.c -I../../build/_deps/wasm3-src/source ../../build/libwasm3.a -lpthread

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "wasm3.h"
#include "m3_env.h"

#define ASYNCIFY_NORMAL    0
#define ASYNCIFY_UNWINDING 1
#define ASYNCIFY_REWINDING 2
#define ASYNCIFY_BUF_SIZE  4096

typedef struct Process {
    int pid;
    IM3Runtime runtime;
    IM3Module module;
    uint32_t asyncify_ptr;      // Address in WASM memory
    int fork_return;            // What fork() should return
    uint8_t* mem_snapshot;      // Memory copy for child
    uint32_t mem_size;
} Process;

static int next_pid = 1;
static uint8_t* g_wasm_bytes;
static size_t g_wasm_size;

// Asyncify helper functions - call WASM exports
static void call_asyncify(IM3Runtime rt, const char* name, uint32_t arg) {
    IM3Function f;
    if (m3_FindFunction(&f, rt, name) == NULL) {
        if (arg != (uint32_t)-1)
            m3_CallV(f, arg);
        else
            m3_CallV(f);
    }
}

static int get_asyncify_state(IM3Runtime rt) {
    IM3Function f;
    if (m3_FindFunction(&f, rt, "asyncify_get_state")) return -1;
    m3_CallV(f);
    int32_t state;
    m3_GetResultsV(f, &state);
    return state;
}

// WASM imports
m3ApiRawFunction(impl_fork) {
    m3ApiReturnType(int32_t);
    Process* p = (Process*)m3_GetUserData(runtime);

    int state = get_asyncify_state(runtime);
    printf("[%d] fork called, asyncify_state=%d\n", p->pid, state);

    if (state == ASYNCIFY_REWINDING) {
        // Completing rewind - return the fork value
        call_asyncify(runtime, "asyncify_stop_rewind", -1);
        printf("[%d] rewind done, returning %d\n", p->pid, p->fork_return);
        m3ApiReturn(p->fork_return);
    }

    // First call - save state and unwind
    uint32_t mem_size;
    uint8_t* mem = m3_GetMemory(runtime, &mem_size, 0);

    // Allocate asyncify buffer at end of memory
    if (p->asyncify_ptr == 0) {
        p->asyncify_ptr = mem_size - ASYNCIFY_BUF_SIZE;
        uint32_t* buf = (uint32_t*)(mem + p->asyncify_ptr);
        buf[0] = p->asyncify_ptr + 8;  // stack start
        buf[1] = p->asyncify_ptr + ASYNCIFY_BUF_SIZE;  // stack end
    }

    // Save memory for child BEFORE unwind modifies it
    p->mem_snapshot = malloc(mem_size);
    p->mem_size = mem_size;
    memcpy(p->mem_snapshot, mem, mem_size);

    int child_pid = next_pid++;
    p->fork_return = child_pid;  // Parent gets child pid

    printf("[%d] starting unwind, child will be pid=%d\n", p->pid, child_pid);
    call_asyncify(runtime, "asyncify_start_unwind", p->asyncify_ptr);

    m3ApiReturn(child_pid);
}

m3ApiRawFunction(impl_write) {
    m3ApiReturnType(int32_t);
    m3ApiGetArg(int32_t, fd);
    m3ApiGetArgMem(const uint8_t*, buf);
    m3ApiGetArg(uint32_t, len);

    Process* p = (Process*)m3_GetUserData(runtime);
    printf("[pid %d] ", p->pid);
    fflush(stdout);

    m3ApiReturn((int32_t)write(fd, buf, len));
}

m3ApiRawFunction(impl_exit) {
    m3ApiGetArg(int32_t, code);
    Process* p = (Process*)m3_GetUserData(runtime);
    printf("[%d] exit(%d)\n", p->pid, code);
    m3ApiTrap("exit");
}

m3ApiRawFunction(impl_getpid) {
    m3ApiReturnType(int32_t);
    Process* p = (Process*)m3_GetUserData(runtime);
    m3ApiReturn(p->pid);
}

// Create a process from WASM bytes
static Process* create_process(int pid, uint8_t* mem_snapshot, uint32_t mem_size, uint32_t asyncify_ptr) {
    Process* p = calloc(1, sizeof(Process));
    p->pid = pid;

    IM3Environment env = m3_NewEnvironment();
    p->runtime = m3_NewRuntime(env, 64*1024, p);

    m3_ParseModule(env, &p->module, g_wasm_bytes, g_wasm_size);
    m3_LoadModule(p->runtime, p->module);

    m3_LinkRawFunction(p->module, "env", "fork", "i()", impl_fork);
    m3_LinkRawFunction(p->module, "env", "write", "i(i*i)", impl_write);
    m3_LinkRawFunction(p->module, "env", "exit", "v(i)", impl_exit);
    m3_LinkRawFunction(p->module, "env", "getpid", "i()", impl_getpid);

    // Restore memory if this is a child
    if (mem_snapshot) {
        uint32_t cur_size;
        uint8_t* mem = m3_GetMemory(p->runtime, &cur_size, 0);
        memcpy(mem, mem_snapshot, mem_size);
        p->asyncify_ptr = asyncify_ptr;
        printf("[%d] memory restored (%u bytes)\n", pid, mem_size);
    }

    return p;
}

// Child thread
typedef struct {
    int pid;
    uint8_t* mem;
    uint32_t mem_size;
    uint32_t asyncify_ptr;
} ChildArgs;

static void* child_thread(void* arg) {
    ChildArgs* a = (ChildArgs*)arg;

    Process* p = create_process(a->pid, a->mem, a->mem_size, a->asyncify_ptr);
    p->fork_return = 0;  // Child returns 0

    printf("[%d] child starting rewind\n", p->pid);
    call_asyncify(p->runtime, "asyncify_start_rewind", p->asyncify_ptr);

    IM3Function start;
    m3_FindFunction(&start, p->runtime, "_start");
    M3Result r = m3_CallV(start);

    if (r && !strstr(r, "exit")) {
        fprintf(stderr, "[%d] error: %s\n", p->pid, r);
    }

    free(a->mem);
    free(a);
    return NULL;
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

    printf("Loaded %s (%zu bytes)\n", file, g_wasm_size);

    // Create parent process
    Process* parent = create_process(next_pid++, NULL, 0, 0);

    IM3Function start;
    m3_FindFunction(&start, parent->runtime, "_start");

    printf("Running _start (pid=%d)\n", parent->pid);
    M3Result r = m3_CallV(start);

    // If fork happened, state is UNWINDING
    if (parent->mem_snapshot) {
        call_asyncify(parent->runtime, "asyncify_stop_unwind", -1);

        // Spawn child
        ChildArgs* ca = malloc(sizeof(ChildArgs));
        ca->pid = parent->fork_return;
        ca->mem = parent->mem_snapshot;
        ca->mem_size = parent->mem_size;
        ca->asyncify_ptr = parent->asyncify_ptr;
        parent->mem_snapshot = NULL;

        pthread_t t;
        pthread_create(&t, NULL, child_thread, ca);

        // Resume parent
        printf("[%d] parent resuming\n", parent->pid);
        call_asyncify(parent->runtime, "asyncify_start_rewind", parent->asyncify_ptr);
        r = m3_CallV(start);

        pthread_join(t, NULL);
    }

    if (r && !strstr(r, "exit")) {
        fprintf(stderr, "Error: %s\n", r);
    }

    printf("Done\n");
    free(g_wasm_bytes);
    return 0;
}
