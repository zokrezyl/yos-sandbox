// Integration test: basic syscalls through actual yos runtime
// Compile to WASM and run with ./yos

__attribute__((import_module("env"), import_name("write")))
int env_write(int fd, const void* buf, unsigned int count);

__attribute__((import_module("env"), import_name("_exit")))
void env_exit(int status);

__attribute__((import_module("env"), import_name("malloc")))
void* env_malloc(unsigned int size);

__attribute__((import_module("env"), import_name("free")))
void env_free(void* ptr);

__attribute__((import_module("env"), import_name("sbrk")))
void* env_sbrk(int increment);

__attribute__((import_module("env"), import_name("getpid")))
int env_getpid(void);

static void print(const char* s) {
    int len = 0;
    while (s[len]) len++;
    env_write(1, s, len);
}

static void fail(const char* msg) {
    env_write(2, "FAIL: ", 6);
    print(msg);
    env_write(2, "\n", 1);
    env_exit(1);
}

void _start(void) {
    // Test 1: write works
    if (env_write(1, "T1:write ", 9) != 9) fail("write returned wrong count");
    print("OK\n");

    // Test 2: sbrk(0) returns valid heap pointer
    print("T2:sbrk(0) ");
    void* heap = env_sbrk(0);
    if (heap == (void*)-1 || heap == 0) fail("sbrk(0) failed");
    print("OK\n");

    // Test 3: sbrk allocation
    print("T3:sbrk(64) ");
    void* p1 = env_sbrk(64);
    if (p1 == (void*)-1) fail("sbrk(64) failed");
    if (p1 != heap) fail("sbrk didn't return old break");
    print("OK\n");

    // Test 4: write to sbrk memory
    print("T4:sbrk write ");
    char* cp = (char*)p1;
    cp[0] = 'A';
    cp[63] = 'Z';
    if (cp[0] != 'A' || cp[63] != 'Z') fail("sbrk memory corrupt");
    print("OK\n");

    // Test 5: malloc
    print("T5:malloc ");
    char* m1 = (char*)env_malloc(128);
    if (m1 == 0) fail("malloc returned null");
    print("OK\n");

    // Test 6: write to malloc memory
    print("T6:malloc write ");
    m1[0] = 'X';
    m1[127] = 'Y';
    if (m1[0] != 'X' || m1[127] != 'Y') fail("malloc memory corrupt");
    print("OK\n");

    // Test 7: free
    print("T7:free ");
    env_free(m1);
    print("OK\n");

    // Test 8: getpid
    print("T8:getpid ");
    int pid = env_getpid();
    if (pid != 1) fail("getpid != 1");
    print("OK\n");

    print("\nAll tests passed!\n");
    env_exit(0);
}
