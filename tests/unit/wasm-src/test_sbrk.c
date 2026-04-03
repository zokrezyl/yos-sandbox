// Test: sbrk() syscall for heap allocation
// Expected: exits 0 if sbrk works correctly

__attribute__((import_module("yos"), import_name("sbrk")))
void* yos_sbrk(int increment);

__attribute__((import_module("yos"), import_name("_exit")))
void yos_exit(int status);

__attribute__((import_module("yos"), import_name("write")))
int yos_write(int fd, const void* buf, unsigned int count);

void _start(void) {
    // sbrk(0) should return current break
    void* brk1 = yos_sbrk(0);
    if (brk1 == (void*)-1) {
        yos_write(2, "E1\n", 3);
        yos_exit(1);
    }

    // sbrk(64) should return old break and extend by 64
    void* old = yos_sbrk(64);
    if (old == (void*)-1) {
        yos_write(2, "E2\n", 3);
        yos_exit(2);
    }
    if (old != brk1) {
        yos_write(2, "E3\n", 3);
        yos_exit(3);
    }

    // New break should be old + 64
    void* brk2 = yos_sbrk(0);
    if (brk2 != (char*)old + 64) {
        yos_write(2, "E4\n", 3);
        yos_exit(4);
    }

    // Should be able to write to allocated memory
    char* p = (char*)old;
    p[0] = 'A';
    p[63] = 'Z';
    if (p[0] != 'A' || p[63] != 'Z') {
        yos_write(2, "E5\n", 3);
        yos_exit(5);
    }

    yos_write(1, "OK\n", 3);
    yos_exit(0);
}
