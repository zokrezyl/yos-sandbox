// Manual wasm3 wrappers for memory allocation functions
// These need special handling because ptr is a WASM offset, not a host pointer
//
// Generated wrappers use m3ApiGetArgMem which converts WASM offset to host ptr,
// but yos_malloc/free/etc return/accept WASM offsets directly.

#include "wasm3.h"
#include "m3_env.h"
#include "yos-runtime.h"

// malloc(size) -> ptr (WASM offset)
m3ApiRawFunction(libc_malloc) {
    m3ApiReturnType(uint32_t);
    m3ApiGetArg(uint32_t, size);

    yos_exec_ctx_t* ctx = (yos_exec_ctx_t*)m3_GetUserData(runtime);
    void* result = yos_malloc(ctx, size);
    m3ApiReturn((uint32_t)(uintptr_t)result);
}

// free(ptr) - ptr is WASM offset
m3ApiRawFunction(libc_free) {
    m3ApiGetArg(uint32_t, ptr);

    yos_exec_ctx_t* ctx = (yos_exec_ctx_t*)m3_GetUserData(runtime);
    yos_free(ctx, (void*)(uintptr_t)ptr);
    m3ApiSuccess();
}

// calloc(nmemb, size) -> ptr (WASM offset)
m3ApiRawFunction(libc_calloc) {
    m3ApiReturnType(uint32_t);
    m3ApiGetArg(uint32_t, nmemb);
    m3ApiGetArg(uint32_t, size);

    yos_exec_ctx_t* ctx = (yos_exec_ctx_t*)m3_GetUserData(runtime);
    void* result = yos_calloc(ctx, nmemb, size);
    m3ApiReturn((uint32_t)(uintptr_t)result);
}

// realloc(ptr, size) -> ptr (WASM offset)
m3ApiRawFunction(libc_realloc) {
    m3ApiReturnType(uint32_t);
    m3ApiGetArg(uint32_t, ptr);
    m3ApiGetArg(uint32_t, size);

    yos_exec_ctx_t* ctx = (yos_exec_ctx_t*)m3_GetUserData(runtime);
    void* result = yos_realloc(ctx, (void*)(uintptr_t)ptr, size);
    m3ApiReturn((uint32_t)(uintptr_t)result);
}

// Link memory allocation functions
void linkMemoryFunctions(IM3Module module) {
    m3_LinkRawFunction(module, "env", "malloc", "*(i)", libc_malloc);
    m3_LinkRawFunction(module, "env", "free", "v(i)", libc_free);
    m3_LinkRawFunction(module, "env", "calloc", "*(ii)", libc_calloc);
    m3_LinkRawFunction(module, "env", "realloc", "*(ii)", libc_realloc);
}
