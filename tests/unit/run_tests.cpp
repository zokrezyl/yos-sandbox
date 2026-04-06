// Unit test runner for YOS WASM tests
// Usage: run_unit_tests <test.wasm> [test2.wasm ...]
// Exit code: 0 if all pass, non-zero on failure

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <fstream>
#include <vector>

#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/syscall.h>

#include "wasm3.h"
#include "m3_env.h"
#include "yos-types.h"

// Simple test context - no VFS, just pass through to host
struct TestContext {
    int exitCode = -1;
    bool exited = false;
};

static TestContext* g_ctx = nullptr;

// Minimal syscall implementations for tests
m3ApiRawFunction(test_write) {
    m3ApiReturnType(int32_t);
    m3ApiGetArg(int32_t, fd);
    m3ApiGetArgMem(const void*, buf);
    m3ApiGetArg(uint32_t, count);

    ssize_t ret = write(fd, buf, count);
    m3ApiReturn(ret < 0 ? -errno : (int32_t)ret);
}

m3ApiRawFunction(test_exit) {
    m3ApiGetArg(int32_t, status);

    if (g_ctx) {
        g_ctx->exitCode = status;
        g_ctx->exited = true;
    }

    m3ApiTrap(m3Err_trapExit);
}

// Shared heap state for sbrk/brk
static uint32_t g_heapPtr = 0;
static uint32_t g_heapBase = 4096;  // Default, may be overwritten by __heap_base

m3ApiRawFunction(test_sbrk) {
    m3ApiReturnType(uint32_t);
    m3ApiGetArg(int32_t, increment);

    // Get current memory
    uint32_t memSize = 0;
    uint8_t* mem = m3_GetMemory(runtime, &memSize, 0);
    (void)mem;

    // Initialize heap pointer on first call
    if (g_heapPtr == 0) {
        g_heapPtr = g_heapBase;
    }

    if (increment == 0) {
        m3ApiReturn(g_heapPtr);
    }

    uint32_t oldPtr = g_heapPtr;
    uint32_t newPtr = g_heapPtr + increment;

    // Check bounds (also catches negative wraparound)
    if (newPtr > memSize || (increment > 0 && newPtr < oldPtr)) {
        m3ApiReturn((uint32_t)-1);  // Out of memory
    }

    g_heapPtr = newPtr;
    m3ApiReturn(oldPtr);
}

m3ApiRawFunction(test_brk) {
    m3ApiReturnType(int32_t);
    m3ApiGetArg(uint32_t, addr);

    // Get current memory size
    uint32_t memSize = 0;
    uint8_t* mem = m3_GetMemory(runtime, &memSize, 0);
    (void)mem;

    // Initialize heap pointer on first call
    if (g_heapPtr == 0) {
        g_heapPtr = g_heapBase;
    }

    // brk(0) returns current break
    if (addr == 0) {
        m3ApiReturn((int32_t)g_heapPtr);
    }

    // Cannot go below heap base
    if (addr < g_heapBase) {
        m3ApiReturn(-1);  // ENOMEM
    }

    // Cannot exceed memory
    if (addr > memSize) {
        m3ApiReturn(-1);  // ENOMEM
    }

    g_heapPtr = addr;
    m3ApiReturn(0);  // Success
}

// Simple malloc/free using bump allocator with free list
// Block header: [size:4][next_free:4][data...]
#define BLOCK_HEADER_SIZE 8
#define BLOCK_USED_MARKER 0xFFFFFFFF

static uint32_t g_freeList = 0;  // Head of free list (WASM offset)

static inline uint32_t read32_at(uint8_t* mem, uint32_t offset) {
    return *(uint32_t*)(mem + offset);
}

static inline void write32_at(uint8_t* mem, uint32_t offset, uint32_t value) {
    *(uint32_t*)(mem + offset) = value;
}

m3ApiRawFunction(test_malloc) {
    m3ApiReturnType(uint32_t);
    m3ApiGetArg(uint32_t, size);

    if (size == 0) m3ApiReturn(0);

    uint32_t memSize = 0;
    uint8_t* mem = m3_GetMemory(runtime, &memSize, 0);

    if (g_heapPtr == 0) g_heapPtr = g_heapBase;

    // Align size to 8 bytes
    size = (size + 7) & ~7;

    // Try free list first
    uint32_t prev = 0;
    uint32_t curr = g_freeList;
    while (curr != 0) {
        uint32_t blkSize = read32_at(mem, curr);
        if (blkSize >= size) {
            uint32_t next = read32_at(mem, curr + 4);
            if (prev == 0) g_freeList = next;
            else write32_at(mem, prev + 4, next);
            write32_at(mem, curr + 4, BLOCK_USED_MARKER);
            m3ApiReturn(curr + BLOCK_HEADER_SIZE);
        }
        prev = curr;
        curr = read32_at(mem, curr + 4);
    }

    // Allocate new block
    uint32_t total = BLOCK_HEADER_SIZE + size;
    uint32_t oldHeap = g_heapPtr;
    uint32_t newHeap = oldHeap + total;
    if (newHeap > memSize || newHeap < oldHeap) m3ApiReturn(0);

    g_heapPtr = newHeap;
    write32_at(mem, oldHeap, size);
    write32_at(mem, oldHeap + 4, BLOCK_USED_MARKER);
    memset(mem + oldHeap + BLOCK_HEADER_SIZE, 0, size);
    m3ApiReturn(oldHeap + BLOCK_HEADER_SIZE);
}

m3ApiRawFunction(test_free) {
    m3ApiGetArg(uint32_t, ptr);

    if (ptr == 0) m3ApiSuccess();

    uint32_t memSize = 0;
    uint8_t* mem = m3_GetMemory(runtime, &memSize, 0);

    uint32_t block = ptr - BLOCK_HEADER_SIZE;
    if (block >= memSize) m3ApiSuccess();

    uint32_t marker = read32_at(mem, block + 4);
    if (marker != BLOCK_USED_MARKER) m3ApiSuccess();  // double free or corruption

    write32_at(mem, block + 4, g_freeList);
    g_freeList = block;
    m3ApiSuccess();
}

m3ApiRawFunction(test_calloc) {
    m3ApiReturnType(uint32_t);
    m3ApiGetArg(uint32_t, nmemb);
    m3ApiGetArg(uint32_t, size);

    uint32_t total = nmemb * size;
    if (nmemb != 0 && total / nmemb != size) m3ApiReturn(0);  // overflow

    // Our malloc already zeros memory
    uint32_t memSize = 0;
    uint8_t* mem = m3_GetMemory(runtime, &memSize, 0);

    if (g_heapPtr == 0) g_heapPtr = g_heapBase;
    total = (total + 7) & ~7;

    uint32_t needed = BLOCK_HEADER_SIZE + total;
    uint32_t oldHeap = g_heapPtr;
    uint32_t newHeap = oldHeap + needed;
    if (newHeap > memSize || newHeap < oldHeap) m3ApiReturn(0);

    g_heapPtr = newHeap;
    write32_at(mem, oldHeap, total);
    write32_at(mem, oldHeap + 4, BLOCK_USED_MARKER);
    memset(mem + oldHeap + BLOCK_HEADER_SIZE, 0, total);
    m3ApiReturn(oldHeap + BLOCK_HEADER_SIZE);
}

m3ApiRawFunction(test_realloc) {
    m3ApiReturnType(uint32_t);
    m3ApiGetArg(uint32_t, ptr);
    m3ApiGetArg(uint32_t, size);

    uint32_t memSize = 0;
    uint8_t* mem = m3_GetMemory(runtime, &memSize, 0);

    if (ptr == 0) {
        // realloc(NULL, size) = malloc(size)
        if (size == 0) m3ApiReturn(0);
        if (g_heapPtr == 0) g_heapPtr = g_heapBase;
        size = (size + 7) & ~7;
        uint32_t total = BLOCK_HEADER_SIZE + size;
        uint32_t oldHeap = g_heapPtr;
        uint32_t newHeap = oldHeap + total;
        if (newHeap > memSize || newHeap < oldHeap) m3ApiReturn(0);
        g_heapPtr = newHeap;
        write32_at(mem, oldHeap, size);
        write32_at(mem, oldHeap + 4, BLOCK_USED_MARKER);
        memset(mem + oldHeap + BLOCK_HEADER_SIZE, 0, size);
        m3ApiReturn(oldHeap + BLOCK_HEADER_SIZE);
    }

    if (size == 0) {
        // realloc(ptr, 0) = free(ptr)
        uint32_t block = ptr - BLOCK_HEADER_SIZE;
        if (block < memSize) {
            write32_at(mem, block + 4, g_freeList);
            g_freeList = block;
        }
        m3ApiReturn(0);
    }

    uint32_t block = ptr - BLOCK_HEADER_SIZE;
    uint32_t oldSize = read32_at(mem, block);

    size = (size + 7) & ~7;
    if (size <= oldSize) m3ApiReturn(ptr);  // fits in existing block

    // Allocate new block
    if (g_heapPtr == 0) g_heapPtr = g_heapBase;
    uint32_t total = BLOCK_HEADER_SIZE + size;
    uint32_t oldHeap = g_heapPtr;
    uint32_t newHeap = oldHeap + total;
    if (newHeap > memSize || newHeap < oldHeap) m3ApiReturn(0);

    g_heapPtr = newHeap;
    write32_at(mem, oldHeap, size);
    write32_at(mem, oldHeap + 4, BLOCK_USED_MARKER);
    memcpy(mem + oldHeap + BLOCK_HEADER_SIZE, mem + ptr, oldSize);

    // Free old block
    write32_at(mem, block + 4, g_freeList);
    g_freeList = block;

    m3ApiReturn(oldHeap + BLOCK_HEADER_SIZE);
}

m3ApiRawFunction(test_open) {
    m3ApiReturnType(int32_t);
    m3ApiGetArgMem(const char*, path);
    m3ApiGetArg(int32_t, flags);
    m3ApiGetArg(int32_t, mode);

    int fd = open(path, flags, mode);
    m3ApiReturn(fd < 0 ? -errno : fd);
}

m3ApiRawFunction(test_read) {
    m3ApiReturnType(int32_t);
    m3ApiGetArg(int32_t, fd);
    m3ApiGetArgMem(void*, buf);
    m3ApiGetArg(uint32_t, count);

    ssize_t ret = read(fd, buf, count);
    m3ApiReturn(ret < 0 ? -errno : (int32_t)ret);
}

m3ApiRawFunction(test_close) {
    m3ApiReturnType(int32_t);
    m3ApiGetArg(int32_t, fd);

    int ret = close(fd);
    m3ApiReturn(ret < 0 ? -errno : 0);
}

static void stat_to_wasm(const struct stat& src, wasm_stat_t& dst) {
    dst.wasm_st_dev = static_cast<uint32_t>(src.st_dev);
    dst.wasm_st_ino = static_cast<uint32_t>(src.st_ino);
    dst.wasm_st_mode = static_cast<uint32_t>(src.st_mode);
    dst.wasm_st_nlink = static_cast<uint32_t>(src.st_nlink);
    dst.wasm_st_uid = static_cast<uint32_t>(src.st_uid);
    dst.wasm_st_gid = static_cast<uint32_t>(src.st_gid);
    dst.wasm_st_rdev = static_cast<uint32_t>(src.st_rdev);
    dst._pad0 = 0;
    dst.wasm_st_size = static_cast<int64_t>(src.st_size);
    dst.wasm_st_blksize = static_cast<uint32_t>(src.st_blksize);
    dst.wasm_st_blocks = static_cast<uint32_t>(src.st_blocks);
    dst.wasm_st_atim_sec = static_cast<int32_t>(src.st_atim.tv_sec);
    dst.wasm_st_atim_nsec = static_cast<int32_t>(src.st_atim.tv_nsec);
    dst.wasm_st_mtim_sec = static_cast<int32_t>(src.st_mtim.tv_sec);
    dst.wasm_st_mtim_nsec = static_cast<int32_t>(src.st_mtim.tv_nsec);
    dst.wasm_st_ctim_sec = static_cast<int32_t>(src.st_ctim.tv_sec);
    dst.wasm_st_ctim_nsec = static_cast<int32_t>(src.st_ctim.tv_nsec);
}

m3ApiRawFunction(test_stat) {
    m3ApiReturnType(int32_t);
    m3ApiGetArgMem(const char*, path);
    m3ApiGetArgMem(wasm_stat_t*, statbuf);

    struct stat st;
    int ret = stat(path, &st);
    if (ret == 0 && statbuf) {
        stat_to_wasm(st, *statbuf);
    }
    m3ApiReturn(ret < 0 ? -errno : 0);
}

m3ApiRawFunction(test_getpid) {
    m3ApiReturnType(int32_t);
    m3ApiReturn(1);  // Always return 1 (init process) for tests
}

m3ApiRawFunction(test_getdents) {
    m3ApiReturnType(int32_t);
    m3ApiGetArg(int32_t, fd);
    m3ApiGetArgMem(void*, dirp);
    m3ApiGetArg(uint32_t, count);

    // Use syscall directly
    long ret = syscall(SYS_getdents64, fd, dirp, count);
    m3ApiReturn(ret < 0 ? -errno : (int32_t)ret);
}

m3ApiRawFunction(test_getcwd) {
    m3ApiReturnType(int32_t);
    m3ApiGetArgMem(char*, buf);
    m3ApiGetArg(uint32_t, size);

    char* ret = getcwd(buf, size);
    m3ApiReturn(ret ? (int32_t)(uintptr_t)buf : 0);
}

m3ApiRawFunction(test_isatty) {
    m3ApiReturnType(int32_t);
    m3ApiGetArg(int32_t, fd);

    m3ApiReturn(isatty(fd) ? 1 : 0);
}

m3ApiRawFunction(test_lstat) {
    m3ApiReturnType(int32_t);
    m3ApiGetArgMem(const char*, path);
    m3ApiGetArgMem(wasm_stat_t*, statbuf);

    struct stat st;
    int ret = lstat(path, &st);
    if (ret == 0 && statbuf) {
        stat_to_wasm(st, *statbuf);
    }
    m3ApiReturn(ret < 0 ? -errno : 0);
}

// Simple DIR* handle table for tests
static DIR* g_dirs[16] = {0};

m3ApiRawFunction(test_opendir) {
    m3ApiReturnType(uint32_t);
    m3ApiGetArgMem(const char*, path);

    DIR* dir = opendir(path);
    if (!dir) m3ApiReturn(0);

    // Find free slot (1-based to avoid NULL confusion)
    for (int i = 0; i < 16; i++) {
        if (!g_dirs[i]) {
            g_dirs[i] = dir;
            m3ApiReturn(i + 1);
        }
    }
    closedir(dir);
    m3ApiReturn(0);
}

m3ApiRawFunction(test_readdir) {
    m3ApiReturnType(uint32_t);
    m3ApiGetArg(uint32_t, handle);

    if (handle == 0 || handle > 16 || !g_dirs[handle-1]) m3ApiReturn(0);
    struct dirent* ent = readdir(g_dirs[handle-1]);
    m3ApiReturn(ent ? 1 : 0);  // Just return 1 for "has entry", 0 for "done"
}

m3ApiRawFunction(test_closedir) {
    m3ApiReturnType(int32_t);
    m3ApiGetArg(uint32_t, handle);

    if (handle == 0 || handle > 16 || !g_dirs[handle-1]) m3ApiReturn(-EBADF);
    int ret = closedir(g_dirs[handle-1]);
    g_dirs[handle-1] = nullptr;
    m3ApiReturn(ret < 0 ? -errno : 0);
}

m3ApiRawFunction(test_printf) {
    m3ApiReturnType(int32_t);
    m3ApiGetArgMem(const char*, fmt);
    m3ApiGetArg(uint32_t, va_ptr);

    uint32_t memSize = 0;
    uint8_t* mem = m3_GetMemory(runtime, &memSize, 0);

    // Simple implementation - handle %d and %s
    char out[256];
    int outIdx = 0;
    int vaOff = 0;

    for (const char* p = fmt; *p && outIdx < 255; p++) {
        if (*p == '%' && *(p+1)) {
            p++;
            if (*p == 'd') {
                int val = va_ptr ? *(int32_t*)(mem + va_ptr + vaOff) : 0;
                vaOff += 4;
                outIdx += snprintf(out + outIdx, 256 - outIdx, "%d", val);
            } else if (*p == 's') {
                uint32_t strPtr = va_ptr ? *(uint32_t*)(mem + va_ptr + vaOff) : 0;
                vaOff += 4;
                const char* str = strPtr ? (const char*)(mem + strPtr) : "(null)";
                outIdx += snprintf(out + outIdx, 256 - outIdx, "%s", str);
            } else if (*p == '%') {
                out[outIdx++] = '%';
            } else {
                out[outIdx++] = '%';
                out[outIdx++] = *p;
            }
        } else {
            out[outIdx++] = *p;
        }
    }
    out[outIdx] = 0;
    printf("%s", out);
    m3ApiReturn(outIdx);
}

m3ApiRawFunction(test_sprintf) {
    m3ApiReturnType(int32_t);
    m3ApiGetArgMem(char*, str);
    m3ApiGetArgMem(const char*, fmt);
    m3ApiGetArg(uint32_t, va_ptr);

    uint32_t memSize = 0;
    uint8_t* mem = m3_GetMemory(runtime, &memSize, 0);

    int outIdx = 0;
    int vaOff = 0;

    for (const char* p = fmt; *p && outIdx < 255; p++) {
        if (*p == '%' && *(p+1)) {
            p++;
            if (*p == 'd') {
                int val = va_ptr ? *(int32_t*)(mem + va_ptr + vaOff) : 0;
                vaOff += 4;
                outIdx += sprintf(str + outIdx, "%d", val);
            } else if (*p == 's') {
                uint32_t strPtr = va_ptr ? *(uint32_t*)(mem + va_ptr + vaOff) : 0;
                vaOff += 4;
                const char* s = strPtr ? (const char*)(mem + strPtr) : "(null)";
                outIdx += sprintf(str + outIdx, "%s", s);
            } else if (*p == '%') {
                str[outIdx++] = '%';
            } else {
                str[outIdx++] = '%';
                str[outIdx++] = *p;
            }
        } else {
            str[outIdx++] = *p;
        }
    }
    str[outIdx] = 0;
    m3ApiReturn(outIdx);
}

void linkTestSyscalls(IM3Module module) {
    m3_LinkRawFunction(module, "yos", "write", "i(i*i)", test_write);
    m3_LinkRawFunction(module, "yos", "_exit", "v(i)", test_exit);
    m3_LinkRawFunction(module, "yos", "sbrk", "*(i)", test_sbrk);
    m3_LinkRawFunction(module, "yos", "brk", "i(i)", test_brk);
    m3_LinkRawFunction(module, "yos", "malloc", "*(i)", test_malloc);
    m3_LinkRawFunction(module, "yos", "free", "v(i)", test_free);
    m3_LinkRawFunction(module, "yos", "calloc", "*(ii)", test_calloc);
    m3_LinkRawFunction(module, "yos", "realloc", "*(ii)", test_realloc);
    m3_LinkRawFunction(module, "yos", "open", "i(*ii)", test_open);
    m3_LinkRawFunction(module, "yos", "read", "i(i*i)", test_read);
    m3_LinkRawFunction(module, "yos", "close", "i(i)", test_close);
    m3_LinkRawFunction(module, "yos", "stat", "i(**)", test_stat);
    m3_LinkRawFunction(module, "yos", "lstat", "i(**)", test_lstat);
    m3_LinkRawFunction(module, "yos", "getpid", "i()", test_getpid);
    m3_LinkRawFunction(module, "yos", "getdents", "i(i*i)", test_getdents);
    m3_LinkRawFunction(module, "yos", "getcwd", "i(*i)", test_getcwd);
    m3_LinkRawFunction(module, "yos", "isatty", "i(i)", test_isatty);
    m3_LinkRawFunction(module, "yos", "opendir", "i(*)", test_opendir);
    m3_LinkRawFunction(module, "yos", "readdir", "i(i)", test_readdir);
    m3_LinkRawFunction(module, "yos", "closedir", "i(i)", test_closedir);
    m3_LinkRawFunction(module, "yos", "printf", "i(*i)", test_printf);
    m3_LinkRawFunction(module, "yos", "sprintf", "i(**i)", test_sprintf);
}

int runTest(const char* wasmPath) {
    // Reset per-test state
    g_heapPtr = 0;
    g_freeList = 0;
    for (int i = 0; i < 16; i++) {
        if (g_dirs[i]) {
            closedir(g_dirs[i]);
            g_dirs[i] = nullptr;
        }
    }

    // Load WASM file
    std::ifstream file(wasmPath, std::ios::binary | std::ios::ate);
    if (!file) {
        fprintf(stderr, "Cannot open: %s\n", wasmPath);
        return 1;
    }

    auto size = file.tellg();
    file.seekg(0);
    std::vector<uint8_t> wasmBytes(static_cast<size_t>(size));
    file.read(reinterpret_cast<char*>(wasmBytes.data()), size);

    // Create runtime
    TestContext ctx;
    g_ctx = &ctx;

    IM3Environment env = m3_NewEnvironment();
    IM3Runtime runtime = m3_NewRuntime(env, 64 * 1024, &ctx);

    IM3Module module = nullptr;
    M3Result result = m3_ParseModule(env, &module, wasmBytes.data(),
                                      static_cast<uint32_t>(wasmBytes.size()));
    if (result) {
        fprintf(stderr, "Parse error: %s\n", result);
        m3_FreeRuntime(runtime);
        m3_FreeEnvironment(env);
        return 1;
    }

    result = m3_LoadModule(runtime, module);
    if (result) {
        fprintf(stderr, "Load error: %s\n", result);
        m3_FreeRuntime(runtime);
        m3_FreeEnvironment(env);
        return 1;
    }

    // Link syscalls
    linkTestSyscalls(module);

    // Find and call _start
    IM3Function startFn = nullptr;
    result = m3_FindFunction(&startFn, runtime, "_start");
    if (result) {
        fprintf(stderr, "Cannot find _start: %s\n", result);
        m3_FreeRuntime(runtime);
        m3_FreeEnvironment(env);
        return 1;
    }

    result = m3_CallV(startFn);

    // Check result
    int exitCode;
    if (ctx.exited) {
        exitCode = ctx.exitCode;
    } else if (result && strstr(result, "exit")) {
        exitCode = ctx.exitCode;
    } else if (result) {
        fprintf(stderr, "Runtime error: %s\n", result);
        exitCode = 1;
    } else {
        exitCode = 0;
    }

    m3_FreeRuntime(runtime);
    m3_FreeEnvironment(env);
    g_ctx = nullptr;

    return exitCode;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <test.wasm> [test2.wasm ...]\n", argv[0]);
        return 1;
    }

    int failed = 0;
    for (int i = 1; i < argc; i++) {
        const char* testPath = argv[i];
        const char* testName = strrchr(testPath, '/');
        testName = testName ? testName + 1 : testPath;

        printf("Running: %s ... ", testName);
        fflush(stdout);

        int result = runTest(testPath);

        if (result == 0) {
            printf("PASS\n");
        } else {
            printf("FAIL (exit=%d)\n", result);
            failed++;
        }
    }

    if (failed > 0) {
        printf("\n%d test(s) FAILED\n", failed);
        return 1;
    }

    printf("\nAll tests PASSED\n");
    return 0;
}
