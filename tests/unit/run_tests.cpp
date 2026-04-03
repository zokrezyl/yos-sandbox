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
#include "wasm-types.hpp"

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

m3ApiRawFunction(test_sbrk) {
    m3ApiReturnType(uint32_t);
    m3ApiGetArg(int32_t, increment);

    // Get current memory
    uint32_t memSize = 0;
    uint8_t* mem = m3_GetMemory(runtime, &memSize, 0);

    // Use a static heap pointer within WASM memory
    // Start heap at 1KB into memory (after any potential data)
    static uint32_t heapPtr = 0;
    if (heapPtr == 0) {
        heapPtr = 4096;  // Start heap at 4KB
    }

    if (increment == 0) {
        m3ApiReturn(heapPtr);
    }

    uint32_t oldPtr = heapPtr;
    uint32_t newPtr = heapPtr + increment;

    // Check bounds
    if (newPtr > memSize || newPtr < heapPtr) {
        m3ApiReturn((uint32_t)-1);  // Out of memory
    }

    heapPtr = newPtr;
    m3ApiReturn(oldPtr);
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

static void stat_to_wasm(const struct stat& src, yos::wasm_stat& dst) {
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
    m3ApiGetArgMem(yos::wasm_stat*, statbuf);

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
    m3ApiGetArgMem(yos::wasm_stat*, statbuf);

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
    // Simple printf - just output the format string for now
    int len = printf("%s", fmt);
    m3ApiReturn(len);
}

m3ApiRawFunction(test_sprintf) {
    m3ApiReturnType(int32_t);
    m3ApiGetArgMem(char*, str);
    m3ApiGetArgMem(const char*, fmt);
    // Simple sprintf - just copy format string
    int len = sprintf(str, "%s", fmt);
    m3ApiReturn(len);
}

void linkTestSyscalls(IM3Module module) {
    m3_LinkRawFunction(module, "yos", "write", "i(i*i)", test_write);
    m3_LinkRawFunction(module, "yos", "_exit", "v(i)", test_exit);
    m3_LinkRawFunction(module, "yos", "sbrk", "*(i)", test_sbrk);
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
    m3_LinkRawFunction(module, "yos", "printf", "i(*)", test_printf);
    m3_LinkRawFunction(module, "yos", "sprintf", "i(**)", test_sprintf);
}

int runTest(const char* wasmPath) {
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
