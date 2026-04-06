// YOS Memory Allocation for WASM
#ifndef YOS_MEMORY_H
#define YOS_MEMORY_H

#include "yos-exec-ctx.h"
#include <stddef.h>

// WASM-aware memory allocation functions
// These allocate within WASM linear memory, returning WASM offsets

void* yos_malloc(yos_exec_ctx_t* ctx, size_t size);
void  yos_free(yos_exec_ctx_t* ctx, void* ptr);
void* yos_calloc(yos_exec_ctx_t* ctx, size_t nmemb, size_t size);
void* yos_realloc(yos_exec_ctx_t* ctx, void* ptr, size_t size);

#endif // YOS_MEMORY_H
