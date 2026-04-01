// malloc.h - stub for wasm32
#pragma once
#include "wasm-compat.h"

// All malloc functions are already declared in wasm-compat.h:
// extern void *malloc(size_t size);
// extern void *calloc(size_t nmemb, size_t size);
// extern void *realloc(void *ptr, size_t size);
// extern void free(void *ptr);

// GNU extensions
extern void *memalign(size_t alignment, size_t size);
extern void *valloc(size_t size);
extern void *pvalloc(size_t size);
extern int posix_memalign(void **memptr, size_t alignment, size_t size);
extern void *aligned_alloc(size_t alignment, size_t size);

// Memory info (stubs)
struct mallinfo {
    int arena;
    int ordblks;
    int smblks;
    int hblks;
    int hblkhd;
    int usmblks;
    int fsmblks;
    int uordblks;
    int fordblks;
    int keepcost;
};

extern struct mallinfo mallinfo(void);
extern int mallopt(int param, int value);
extern void malloc_stats(void);
extern size_t malloc_usable_size(void *ptr);
