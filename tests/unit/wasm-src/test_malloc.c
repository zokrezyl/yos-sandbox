// Test: malloc/free/calloc/realloc work within WASM linear memory
// Expected: exits 0 if all tests pass

__attribute__((import_module("yos"), import_name("_exit")))
void yos_exit(int status);

__attribute__((import_module("yos"), import_name("write")))
int yos_write(int fd, const void* buf, unsigned int count);

__attribute__((import_module("yos"), import_name("sbrk")))
void* yos_sbrk(int increment);

// Test runner uses "yos" module for all syscalls including malloc
__attribute__((import_module("yos"), import_name("malloc")))
void* yos_malloc(unsigned int size);

__attribute__((import_module("yos"), import_name("free")))
void yos_free(void* ptr);

__attribute__((import_module("yos"), import_name("realloc")))
void* yos_realloc(void* ptr, unsigned int size);

__attribute__((import_module("yos"), import_name("calloc")))
void* yos_calloc(unsigned int nmemb, unsigned int size);

void _start(void) {
    // Get initial heap position
    unsigned int heap0 = (unsigned int)yos_sbrk(0);

    // Test 1: malloc returns non-null
    char* p1 = (char*)yos_malloc(64);
    if (p1 == 0) {
        yos_write(2, "E1: null\n", 9);
        yos_exit(1);
    }

    // Test 2: returned pointer is reasonable (between heap_base and memory end)
    unsigned int p1_val = (unsigned int)p1;
    if (p1_val < heap0 || p1_val > 1024*1024) {
        yos_write(2, "E2: range\n", 10);
        yos_exit(2);
    }

    // Test 3: can write to allocated memory
    for (int i = 0; i < 64; i++) {
        p1[i] = (char)(i & 0xFF);
    }

    // Test 4: can read back
    for (int i = 0; i < 64; i++) {
        if (p1[i] != (char)(i & 0xFF)) {
            yos_write(2, "E4: corrupt\n", 12);
            yos_exit(4);
        }
    }

    // Test 5: second malloc returns different address
    char* p2 = (char*)yos_malloc(64);
    if (p2 == 0 || p2 == p1) {
        yos_write(2, "E5: same\n", 9);
        yos_exit(5);
    }

    // Test 6: free doesn't crash
    yos_free(p1);

    // Test 7: malloc after free works (may reuse)
    char* p3 = (char*)yos_malloc(32);
    if (p3 == 0) {
        yos_write(2, "E7: null\n", 9);
        yos_exit(7);
    }

    // Test 8: calloc zeros memory
    char* p4 = (char*)yos_calloc(8, 8);  // 64 bytes
    if (p4 == 0) {
        yos_write(2, "E8: null\n", 9);
        yos_exit(8);
    }
    for (int i = 0; i < 64; i++) {
        if (p4[i] != 0) {
            yos_write(2, "E8: not zero\n", 13);
            yos_exit(8);
        }
    }

    // Test 9: realloc preserves data
    p3[0] = 'X';
    p3[31] = 'Y';
    char* p5 = (char*)yos_realloc(p3, 128);
    if (p5 == 0) {
        yos_write(2, "E9: null\n", 9);
        yos_exit(9);
    }
    if (p5[0] != 'X' || p5[31] != 'Y') {
        yos_write(2, "E9: lost\n", 9);
        yos_exit(9);
    }

    // Cleanup
    yos_free(p2);
    yos_free(p4);
    yos_free(p5);

    yos_write(1, "OK\n", 3);
    yos_exit(0);
}
