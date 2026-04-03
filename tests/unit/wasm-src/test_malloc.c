// Test: malloc works and returns valid addresses
// Expected: exits 0 if malloc works, 1 if it fails

extern void _exit(int status);
extern void* malloc(unsigned int size);

int _start(void) {
    // Test 1: Small allocation
    void* p1 = malloc(64);
    if (!p1) _exit(1);

    // Test 2: Another allocation should return different address
    void* p2 = malloc(64);
    if (!p2) _exit(2);
    if (p1 == p2) _exit(3);

    // Test 3: Can write to allocated memory
    char* s = (char*)p1;
    s[0] = 'H';
    s[1] = 'i';
    s[2] = 0;
    if (s[0] != 'H') _exit(4);

    _exit(0);
    return 0;
}
