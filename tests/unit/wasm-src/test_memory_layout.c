// Test: WASM memory layout verification
// Tests that we can access the full memory range without out-of-bounds
// Uses __builtin_wasm_memory_size to get actual memory size

__attribute__((import_module("yos"), import_name("write")))
int yos_write(int fd, const void* buf, unsigned int count);

__attribute__((import_module("yos"), import_name("_exit")))
void yos_exit(int status);

// Helper to write a number
void write_num(unsigned int n) {
    char buf[12];
    int i = 11;
    buf[i--] = '\n';
    if (n == 0) {
        buf[i--] = '0';
    } else {
        while (n > 0 && i >= 0) {
            buf[i--] = '0' + (n % 10);
            n /= 10;
        }
    }
    yos_write(1, &buf[i+1], 11 - i);
}

void _start(void) {
    // Get memory size in pages (64KB each)
    unsigned int pages = __builtin_wasm_memory_size(0);
    unsigned int mem_size = pages * 65536;

    // Test 1: Memory should be at least 2 pages (128KB)
    if (pages < 2) {
        yos_write(2, "E1: mem < 2 pages\n", 18);
        yos_exit(1);
    }

    // Test 2: Can write to address 4096 (above stack, in data area)
    unsigned char* p = (unsigned char*)4096;
    p[0] = 0xAA;
    if (p[0] != 0xAA) {
        yos_write(2, "E2: cannot write 4096\n", 22);
        yos_exit(2);
    }

    // Test 3: Can write to address 65536 (start of data section)
    p = (unsigned char*)65536;
    unsigned char old = p[0];  // Save original
    p[0] = 0xBB;
    if (p[0] != 0xBB) {
        yos_write(2, "E3: cannot write 65536\n", 23);
        yos_exit(3);
    }
    p[0] = old;  // Restore

    // Test 4: Can write near end of memory
    p = (unsigned char*)(mem_size - 16);
    p[0] = 0xCC;
    if (p[0] != 0xCC) {
        yos_write(2, "E4: cannot write near end\n", 26);
        yos_exit(4);
    }

    // Test 5: Can write to 102608 (typical heap_base for busybox)
    p = (unsigned char*)102608;
    if ((unsigned int)p < mem_size) {
        p[0] = 0xDD;
        if (p[0] != 0xDD) {
            yos_write(2, "E5: cannot write heap_base\n", 27);
            yos_exit(5);
        }
    }

    // Output info
    yos_write(1, "pages=", 6);
    write_num(pages);
    yos_write(1, "mem_size=", 9);
    write_num(mem_size);

    yos_write(1, "OK\n", 3);
    yos_exit(0);
}
