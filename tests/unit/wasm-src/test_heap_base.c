// Test: heap base initialization from __heap_base global
// Verifies that sbrk(0) returns a reasonable non-zero value
// Expected: exits 0 if heap_base is properly initialized

__attribute__((import_module("yos"), import_name("sbrk")))
void* yos_sbrk(int increment);

__attribute__((import_module("yos"), import_name("_exit")))
void yos_exit(int status);

__attribute__((import_module("yos"), import_name("write")))
int yos_write(int fd, const void* buf, unsigned int count);

// These globals may be exported by the linker
extern char __heap_base;
extern char __data_end;

void _start(void) {
    // Test 1: sbrk(0) should return non-zero value
    unsigned int heap = (unsigned int)yos_sbrk(0);

    if (heap == 0) {
        yos_write(2, "E1: heap is 0!\n", 15);
        yos_exit(1);
    }

    if (heap == (unsigned int)-1) {
        yos_write(2, "E2: sbrk failed\n", 16);
        yos_exit(2);
    }

    // Test 2: heap should be reasonable (> 1KB, < 1MB for typical WASM)
    if (heap < 1024) {
        yos_write(2, "E3: heap < 1KB\n", 15);
        yos_exit(3);
    }

    if (heap > 1024 * 1024) {
        yos_write(2, "E4: heap > 1MB\n", 15);
        yos_exit(4);
    }

    // Test 3: Multiple sbrk(0) calls should return same value
    unsigned int heap2 = (unsigned int)yos_sbrk(0);
    if (heap2 != heap) {
        yos_write(2, "E5: heap changed\n", 17);
        yos_exit(5);
    }

    // Test 4: First allocation should return heap base
    void* p = yos_sbrk(64);
    if ((unsigned int)p != heap) {
        yos_write(2, "E6: alloc != base\n", 18);
        yos_exit(6);
    }

    // Test 5: Memory at heap base should be writable
    char* cp = (char*)p;
    cp[0] = 'X';
    cp[63] = 'Y';
    if (cp[0] != 'X' || cp[63] != 'Y') {
        yos_write(2, "E7: write fail\n", 15);
        yos_exit(7);
    }

    yos_write(1, "OK\n", 3);
    yos_exit(0);
}
