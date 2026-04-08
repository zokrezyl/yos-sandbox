// fork.c - Fork implementation using asyncify

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include "fork.h"
#include "m3_env.h"  // For internal structures

#define ASYNCIFY_NORMAL    0
#define ASYNCIFY_UNWINDING 1
#define ASYNCIFY_REWINDING 2
#define ASYNCIFY_BUF_SIZE  16384  // 16KB - reused for each fork
#define MAX_GLOBALS        16

static int next_pid = 1;

int fork_next_pid(void) {
    return next_pid++;
}

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

m3ApiRawFunction(fork_impl) {
    m3ApiReturnType(int32_t);
    ForkProcess* p = (ForkProcess*)m3_GetUserData(runtime);

    int state = get_asyncify_state(runtime);

    if (state == ASYNCIFY_REWINDING) {
        call_asyncify(runtime, "asyncify_stop_rewind", -1);
        m3ApiReturn(p->fork_return);
    }

    // First call
    uint32_t mem_size;
    uint8_t* mem = m3_GetMemory(runtime, &mem_size, 0);

    // Allocate buffer once, reuse for all forks
    if (p->asyncify_ptr == 0) {
        p->asyncify_ptr = mem_size - ASYNCIFY_BUF_SIZE;
    }
    // Reset buffer header for this fork
    uint32_t* buf = (uint32_t*)(mem + p->asyncify_ptr);
    buf[0] = p->asyncify_ptr + 8;
    buf[1] = p->asyncify_ptr + ASYNCIFY_BUF_SIZE;

    int child_pid = next_pid++;
    p->fork_return = child_pid;
    p->fork_pending = 1;

    call_asyncify(runtime, "asyncify_start_unwind", p->asyncify_ptr);
    m3ApiReturn(child_pid);
}

// Child thread args
typedef struct {
    uint8_t* wasm_bytes;
    size_t wasm_size;
    fork_link_fn link_fn;
    uint8_t* mem_snapshot;
    uint32_t mem_size;
    uint32_t asyncify_ptr;
    int child_pid;
    // Saved globals - dynamically allocated
    int64_t* globals;
    uint32_t num_globals;
} ChildArgs;

static void* child_thread(void* arg) {
    ChildArgs* a = (ChildArgs*)arg;

    IM3Environment env = m3_NewEnvironment();
    ForkProcess* child = calloc(1, sizeof(ForkProcess));
    child->pid = a->child_pid;
    child->fork_return = 0;
    child->asyncify_ptr = a->asyncify_ptr;
    child->wasm_bytes = a->wasm_bytes;
    child->wasm_size = a->wasm_size;
    child->link_fn = a->link_fn;

    child->runtime = m3_NewRuntime(env, 64*1024, child);
    IM3Module mod;
    m3_ParseModule(env, &mod, a->wasm_bytes, a->wasm_size);
    m3_LoadModule(child->runtime, mod);
    child->module = mod;
    a->link_fn(mod, child);

    // Restore memory
    uint32_t sz;
    uint8_t* mem = m3_GetMemory(child->runtime, &sz, 0);
    memcpy(mem, a->mem_snapshot, a->mem_size);

    // Restore globals
    for (uint32_t i = 0; i < a->num_globals && i < mod->numGlobals; i++) {
        mod->globals[i].intValue = a->globals[i];
    }

    call_asyncify(child->runtime, "asyncify_start_rewind", a->asyncify_ptr);

    IM3Function start;
    m3_FindFunction(&start, child->runtime, "_start");
    m3_CallV(start);

    fork_pump(child);

    free(a->mem_snapshot);
    free(a->globals);
    free(a);
    return NULL;
}

void fork_pump(ForkProcess* p) {
    while (p->fork_pending) {
        p->fork_pending = 0;

        call_asyncify(p->runtime, "asyncify_stop_unwind", -1);

        uint32_t mem_size;
        uint8_t* mem = m3_GetMemory(p->runtime, &mem_size, 0);

        uint8_t* mem_copy = malloc(mem_size);
        memcpy(mem_copy, mem, mem_size);

        ChildArgs* ca = malloc(sizeof(ChildArgs));
        ca->wasm_bytes = p->wasm_bytes;
        ca->wasm_size = p->wasm_size;
        ca->link_fn = p->link_fn;
        ca->mem_snapshot = mem_copy;
        ca->mem_size = mem_size;
        ca->asyncify_ptr = p->asyncify_ptr;
        ca->child_pid = p->fork_return;

        // Save globals - dynamically allocated
        IM3Module mod = p->module;
        ca->num_globals = mod->numGlobals;
        ca->globals = malloc(mod->numGlobals * sizeof(int64_t));
        for (uint32_t i = 0; i < mod->numGlobals; i++) {
            ca->globals[i] = mod->globals[i].intValue;
        }

        pthread_t t;
        pthread_create(&t, NULL, child_thread, ca);
        pthread_join(t, NULL);

        call_asyncify(p->runtime, "asyncify_start_rewind", p->asyncify_ptr);

        IM3Function start;
        m3_FindFunction(&start, p->runtime, "_start");
        m3_CallV(start);
    }
}
