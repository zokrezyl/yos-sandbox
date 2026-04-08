// fork.h - Fork emulation interface
#ifndef FORK_H
#define FORK_H

#include <stdint.h>
#include "wasm3.h"

struct ForkProcess;

// Function to link WASM imports - provided by runtime
typedef void (*fork_link_fn)(IM3Module mod, struct ForkProcess* p);

// Process state
typedef struct ForkProcess {
    int pid;
    int fork_return;
    uint32_t asyncify_ptr;
    IM3Runtime runtime;

    // For spawning children
    uint8_t* wasm_bytes;
    size_t wasm_size;
    fork_link_fn link_fn;
    int fork_pending;
    IM3Module module;  // Store module pointer
} ForkProcess;

int fork_next_pid(void);
m3ApiRawFunction(fork_impl);
void fork_stop_unwind(IM3Runtime rt);
void fork_start_rewind(IM3Runtime rt, uint32_t asyncify_ptr);

// Call after m3_CallV(_start) - handles fork spawning and resumption
void fork_pump(ForkProcess* p);

#endif
