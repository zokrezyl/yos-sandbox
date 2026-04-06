// YOS Memory Allocation for WASM
// Implements malloc/free/realloc/calloc within WASM linear memory

#define _GNU_SOURCE
#include "yos-memory.h"
#include "yos-log.h"

#include <string.h>
#include <stdint.h>

// Simple bump allocator with free list
// Each allocation has a header: [size:4][next_free:4][data...]
// Free blocks are linked via next_free, allocated blocks have next_free=0xFFFFFFFF

#define BLOCK_HEADER_SIZE 8
#define BLOCK_SIZE_OFFSET 0
#define BLOCK_NEXT_OFFSET 4
#define BLOCK_USED_MARKER 0xFFFFFFFF
#define MIN_BLOCK_SIZE 16  // Minimum allocation including header

// Get pointer to WASM memory at offset
static inline uint8_t* wasm_ptr(yos_exec_ctx_t* ctx, uint32_t offset) {
    if (offset == 0 || offset >= ctx->wasm_mem_size) return NULL;
    return (uint8_t*)ctx->wasm_memory + offset;
}

// Read/write 32-bit values in WASM memory
static inline uint32_t read32(yos_exec_ctx_t* ctx, uint32_t offset) {
    uint8_t* p = wasm_ptr(ctx, offset);
    if (!p) return 0;
    return *(uint32_t*)p;
}

static inline void write32(yos_exec_ctx_t* ctx, uint32_t offset, uint32_t value) {
    uint8_t* p = wasm_ptr(ctx, offset);
    if (p) *(uint32_t*)p = value;
}

// Initialize free list head in context if not done
static void init_free_list(yos_exec_ctx_t* ctx) {
    if (ctx->free_list_head == 0) {
        // No free blocks initially - all allocation via sbrk
        ctx->free_list_head = 0;
    }
}

// Find a free block of at least 'size' bytes (not including header)
// Returns offset to block header, or 0 if none found
static uint32_t find_free_block(yos_exec_ctx_t* ctx, uint32_t size) {
    uint32_t prev = 0;
    uint32_t curr = ctx->free_list_head;

    while (curr != 0) {
        uint32_t block_size = read32(ctx, curr + BLOCK_SIZE_OFFSET);
        if (block_size >= size) {
            // Found a block - remove from free list
            uint32_t next = read32(ctx, curr + BLOCK_NEXT_OFFSET);
            if (prev == 0) {
                ctx->free_list_head = next;
            } else {
                write32(ctx, prev + BLOCK_NEXT_OFFSET, next);
            }
            // Mark as used
            write32(ctx, curr + BLOCK_NEXT_OFFSET, BLOCK_USED_MARKER);
            return curr;
        }
        prev = curr;
        curr = read32(ctx, curr + BLOCK_NEXT_OFFSET);
    }
    return 0;
}

// Add block to free list
static void add_to_free_list(yos_exec_ctx_t* ctx, uint32_t block) {
    write32(ctx, block + BLOCK_NEXT_OFFSET, ctx->free_list_head);
    ctx->free_list_head = block;
}

void* yos_malloc(yos_exec_ctx_t* ctx, size_t size) {
    if (size == 0) return NULL;

    init_free_list(ctx);

    // Align size to 8 bytes
    size = (size + 7) & ~7;

    // Try to find a free block first
    uint32_t block = find_free_block(ctx, size);
    if (block != 0) {
        YOS_TRACE("malloc(%zu) = %u (reused)", size, block + BLOCK_HEADER_SIZE);
        return (void*)(uintptr_t)(block + BLOCK_HEADER_SIZE);
    }

    // Allocate new block via sbrk
    uint32_t total = BLOCK_HEADER_SIZE + size;
    uint32_t old_heap = ctx->heap_end;
    uint32_t new_heap = old_heap + total;

    if (new_heap > ctx->wasm_mem_size || new_heap < old_heap) {
        YOS_ERROR("malloc(%zu): out of memory (heap=%u, need=%u, max=%zu)",
                  size, old_heap, new_heap, ctx->wasm_mem_size);
        return NULL;
    }

    ctx->heap_end = new_heap;

    // Write block header
    write32(ctx, old_heap + BLOCK_SIZE_OFFSET, size);
    write32(ctx, old_heap + BLOCK_NEXT_OFFSET, BLOCK_USED_MARKER);

    // Zero the memory
    uint8_t* data = wasm_ptr(ctx, old_heap + BLOCK_HEADER_SIZE);
    if (data) memset(data, 0, size);

    YOS_TRACE("malloc(%zu) = %u (new)", size, old_heap + BLOCK_HEADER_SIZE);
    return (void*)(uintptr_t)(old_heap + BLOCK_HEADER_SIZE);
}

void yos_free(yos_exec_ctx_t* ctx, void* ptr) {
    uint32_t offset = (uint32_t)(uintptr_t)ptr;
    if (offset == 0) return;

    // Get block header
    uint32_t block = offset - BLOCK_HEADER_SIZE;
    if (block < ctx->heap_end && block >= 4096) {  // Sanity check
        uint32_t marker = read32(ctx, block + BLOCK_NEXT_OFFSET);
        if (marker == BLOCK_USED_MARKER) {
            add_to_free_list(ctx, block);
            YOS_TRACE("free(%u)", offset);
        } else {
            YOS_WARN("free(%u): double free or corruption", offset);
        }
    } else {
        YOS_WARN("free(%u): invalid pointer", offset);
    }
}

void* yos_calloc(yos_exec_ctx_t* ctx, size_t nmemb, size_t size) {
    size_t total = nmemb * size;
    if (nmemb != 0 && total / nmemb != size) {
        // Overflow
        return NULL;
    }

    void* ptr = yos_malloc(ctx, total);
    // malloc already zeros memory
    return ptr;
}

void* yos_realloc(yos_exec_ctx_t* ctx, void* ptr, size_t size) {
    if (ptr == NULL) {
        return yos_malloc(ctx, size);
    }

    if (size == 0) {
        yos_free(ctx, ptr);
        return NULL;
    }

    uint32_t offset = (uint32_t)(uintptr_t)ptr;
    uint32_t block = offset - BLOCK_HEADER_SIZE;
    uint32_t old_size = read32(ctx, block + BLOCK_SIZE_OFFSET);

    // If new size fits in old block, just return same pointer
    size = (size + 7) & ~7;
    if (size <= old_size) {
        return ptr;
    }

    // Allocate new block and copy
    void* new_ptr = yos_malloc(ctx, size);
    if (new_ptr == NULL) return NULL;

    // Copy old data
    uint8_t* src = wasm_ptr(ctx, offset);
    uint8_t* dst = wasm_ptr(ctx, (uint32_t)(uintptr_t)new_ptr);
    if (src && dst) {
        memcpy(dst, src, old_size);
    }

    yos_free(ctx, ptr);
    return new_ptr;
}
