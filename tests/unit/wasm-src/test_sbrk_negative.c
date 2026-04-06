// Test: sbrk() with negative increment (shrink heap)
// Expected: exits 0 if heap shrinking works correctly

__attribute__((import_module("yos"), import_name("sbrk")))
void* yos_sbrk(int increment);

__attribute__((import_module("yos"), import_name("_exit")))
void yos_exit(int status);

__attribute__((import_module("yos"), import_name("write")))
int yos_write(int fd, const void* buf, unsigned int count);

void _start(void) {
    // Get initial break
    unsigned int brk0 = (unsigned int)yos_sbrk(0);
    if (brk0 == (unsigned int)-1 || brk0 == 0) {
        yos_write(2, "E1: sbrk(0) fail\n", 17);
        yos_exit(1);
    }

    // Allocate 256 bytes
    void* p1 = yos_sbrk(256);
    if (p1 == (void*)-1) {
        yos_write(2, "E2: sbrk(256) fail\n", 19);
        yos_exit(2);
    }
    if ((unsigned int)p1 != brk0) {
        yos_write(2, "E2: bad p1\n", 11);
        yos_exit(2);
    }

    // Verify break moved
    unsigned int brk1 = (unsigned int)yos_sbrk(0);
    if (brk1 != brk0 + 256) {
        yos_write(2, "E3: bad brk1\n", 13);
        yos_exit(3);
    }

    // Write to allocated memory
    char* cp = (char*)p1;
    cp[0] = 'A';
    cp[255] = 'Z';

    // Shrink by 128 bytes (sbrk with negative)
    void* p2 = yos_sbrk(-128);
    if (p2 == (void*)-1) {
        yos_write(2, "E4: sbrk(-128) fail\n", 20);
        yos_exit(4);
    }
    // p2 should be old break
    if ((unsigned int)p2 != brk1) {
        yos_write(2, "E4: bad p2\n", 11);
        yos_exit(4);
    }

    // Verify break shrunk
    unsigned int brk2 = (unsigned int)yos_sbrk(0);
    if (brk2 != brk0 + 128) {
        yos_write(2, "E5: bad brk2\n", 13);
        yos_exit(5);
    }

    // First 128 bytes should still be accessible
    if (cp[0] != 'A') {
        yos_write(2, "E6: data lost\n", 14);
        yos_exit(6);
    }

    // Shrink to original
    void* p3 = yos_sbrk(-128);
    if (p3 == (void*)-1) {
        yos_write(2, "E7: sbrk(-128) fail\n", 20);
        yos_exit(7);
    }

    // Verify back to original
    unsigned int brk3 = (unsigned int)yos_sbrk(0);
    if (brk3 != brk0) {
        yos_write(2, "E8: not back\n", 13);
        yos_exit(8);
    }

    yos_write(1, "OK\n", 3);
    yos_exit(0);
}
