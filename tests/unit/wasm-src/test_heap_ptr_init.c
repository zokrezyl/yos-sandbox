// Test: Check that heap pointer location is zero-initialized
// Busybox malloc expects address 102604 to be 0 on first call
// This tests the exact condition that causes malloc to fail

__attribute__((import_module("yos"), import_name("write")))
int yos_write(int fd, const void* buf, unsigned int count);

__attribute__((import_module("yos"), import_name("_exit")))
void yos_exit(int status);

// Helper to write hex value
void write_hex(unsigned int n) {
    char buf[12] = "0x00000000\n";
    const char* hex = "0123456789abcdef";
    for (int i = 9; i >= 2; i--) {
        buf[i] = hex[n & 0xf];
        n >>= 4;
    }
    yos_write(1, buf, 11);
}

// Busybox malloc addresses
#define HEAP_PTR_ADDR 102604
#define HEAP_BASE     102608
#define HEAP_END      131072

void _start(void) {
    // Read the heap pointer location - this is what malloc does first
    unsigned int* heap_ptr_loc = (unsigned int*)HEAP_PTR_ADDR;
    unsigned int initial_value = *heap_ptr_loc;

    yos_write(1, "heap_ptr@102604=", 16);
    write_hex(initial_value);

    // Test 1: Should be zero initially (BSS area)
    if (initial_value != 0) {
        yos_write(2, "E1: heap_ptr not zero!\n", 23);
        yos_exit(1);
    }

    // Test 2: Initialize like malloc does
    *heap_ptr_loc = HEAP_BASE;

    // Test 3: Verify the write worked
    if (*heap_ptr_loc != HEAP_BASE) {
        yos_write(2, "E2: write to 102604 failed\n", 27);
        yos_exit(2);
    }

    // Test 4: Simulate malloc - bump and return
    unsigned int size = 72;  // What func[332] requests
    unsigned int aligned = (size + 7) & ~7;  // 72 -> 72
    unsigned int old_ptr = *heap_ptr_loc;
    *heap_ptr_loc = old_ptr + aligned;

    yos_write(1, "allocated at ", 13);
    write_hex(old_ptr);

    // Test 5: Write to allocated memory (what xzalloc does)
    unsigned char* mem = (unsigned char*)old_ptr;
    for (unsigned int i = 0; i < size; i++) {
        mem[i] = 0;
    }

    // Test 6: Verify writes
    for (unsigned int i = 0; i < size; i++) {
        if (mem[i] != 0) {
            yos_write(2, "E3: memset failed\n", 18);
            yos_exit(3);
        }
    }

    yos_write(1, "OK\n", 3);
    yos_exit(0);
}
