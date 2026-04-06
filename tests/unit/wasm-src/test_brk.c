// Test: brk() syscall for heap management
// Tests brk() alongside sbrk() for heap control
// Expected: exits 0 if all tests pass

__attribute__((import_module("yos"), import_name("sbrk")))
void* yos_sbrk(int increment);

__attribute__((import_module("yos"), import_name("brk")))
int yos_brk(unsigned int addr);

__attribute__((import_module("yos"), import_name("_exit")))
void yos_exit(int status);

__attribute__((import_module("yos"), import_name("write")))
int yos_write(int fd, const void* buf, unsigned int count);

void _start(void) {
    // Test 1: Get current break with sbrk(0)
    unsigned int brk0 = (unsigned int)yos_sbrk(0);
    if (brk0 == (unsigned int)-1 || brk0 == 0) {
        yos_write(2, "E1: sbrk(0) failed\n", 19);
        yos_exit(1);
    }

    // Test 2: brk(0) should also return current break
    int brk_result = yos_brk(0);
    if (brk_result != (int)brk0) {
        yos_write(2, "E2: brk(0) mismatch\n", 20);
        yos_exit(2);
    }

    // Test 3: Set new break with brk()
    unsigned int new_brk = brk0 + 128;
    int ret = yos_brk(new_brk);
    if (ret != 0) {
        yos_write(2, "E3: brk() failed\n", 17);
        yos_exit(3);
    }

    // Test 4: Verify break moved
    unsigned int brk1 = (unsigned int)yos_sbrk(0);
    if (brk1 != new_brk) {
        yos_write(2, "E4: brk didn't move\n", 20);
        yos_exit(4);
    }

    // Test 5: Write to new memory
    char* p = (char*)brk0;
    for (int i = 0; i < 128; i++) {
        p[i] = (char)(i & 0xFF);
    }

    // Test 6: Verify written data
    for (int i = 0; i < 128; i++) {
        if (p[i] != (char)(i & 0xFF)) {
            yos_write(2, "E6: data corrupt\n", 17);
            yos_exit(6);
        }
    }

    // Test 7: Shrink heap with brk() back to original
    ret = yos_brk(brk0);
    if (ret != 0) {
        yos_write(2, "E7: brk shrink fail\n", 20);
        yos_exit(7);
    }

    // Test 8: Verify heap shrunk
    unsigned int brk2 = (unsigned int)yos_sbrk(0);
    if (brk2 != brk0) {
        yos_write(2, "E8: brk not shrunk\n", 19);
        yos_exit(8);
    }

    // Test 9: brk to very large value should fail
    ret = yos_brk(0xFFFFFFFF);
    if (ret == 0) {
        yos_write(2, "E9: huge brk ok?!\n", 18);
        yos_exit(9);
    }

    // Test 10: sbrk and brk interop - allocate with sbrk, check with brk
    void* alloc = yos_sbrk(256);
    if (alloc == (void*)-1) {
        yos_write(2, "E10: sbrk(256) fail\n", 20);
        yos_exit(10);
    }
    unsigned int expected_brk = (unsigned int)alloc + 256;
    brk_result = yos_brk(0);
    if ((unsigned int)brk_result != expected_brk) {
        yos_write(2, "E10: brk/sbrk off\n", 18);
        yos_exit(10);
    }

    yos_write(1, "OK\n", 3);
    yos_exit(0);
}
