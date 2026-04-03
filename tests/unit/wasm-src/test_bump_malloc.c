// Test: Replicate EXACT busybox malloc behavior
// Busybox malloc at func[1491]:
//   - Heap pointer stored at address 102604
//   - Initializes to 102608 (__heap_base) if zero
//   - Bumps by (size + 7) & ~7
//   - NO bounds checking - this causes out-of-bounds crash

__attribute__((import_module("yos"), import_name("write")))
int yos_write(int fd, const void* buf, unsigned int count);

__attribute__((import_module("yos"), import_name("_exit")))
void yos_exit(int status);

// Exact addresses from busybox.wasm analysis:
// __heap_base = 102608
// __heap_end = 131072 (2 pages)
// heap pointer location = 102604

#define HEAP_PTR_ADDR  102604
#define HEAP_BASE      102608
#define HEAP_END       131072

// Replicate busybox malloc exactly
void* bb_malloc(unsigned int size) {
    unsigned int* heap_ptr_loc = (unsigned int*)HEAP_PTR_ADDR;

    // Initialize on first call (exactly like busybox)
    if (*heap_ptr_loc == 0) {
        *heap_ptr_loc = HEAP_BASE;
    }

    unsigned int old_ptr = *heap_ptr_loc;

    // Align to 8 bytes (busybox: (size + 7) & ~7)
    unsigned int aligned_size = (size + 7) & ~7;

    // Bump pointer - busybox does NOT check bounds here!
    *heap_ptr_loc = old_ptr + aligned_size;

    return (void*)old_ptr;
}

// What busybox SHOULD do - malloc with bounds check
void* bb_malloc_safe(unsigned int size) {
    unsigned int* heap_ptr_loc = (unsigned int*)HEAP_PTR_ADDR;

    if (*heap_ptr_loc == 0) {
        *heap_ptr_loc = HEAP_BASE;
    }

    unsigned int old_ptr = *heap_ptr_loc;
    unsigned int aligned_size = (size + 7) & ~7;

    // Bounds check that busybox is missing
    if (old_ptr + aligned_size > HEAP_END) {
        return (void*)0;
    }

    *heap_ptr_loc = old_ptr + aligned_size;
    return (void*)old_ptr;
}

// memset - what xzalloc does after malloc
void my_memset(void* dest, int val, unsigned int count) {
    unsigned char* d = (unsigned char*)dest;
    for (unsigned int i = 0; i < count; i++) {
        d[i] = (unsigned char)val;
    }
}

void _start(void) {
    // Reset heap pointer to simulate fresh start
    *(unsigned int*)HEAP_PTR_ADDR = 0;

    // Test with safe malloc (bounds checking)

    // Test 1: Small allocation
    void* p1 = bb_malloc_safe(64);
    if (!p1) {
        yos_write(2, "E1\n", 3);
        yos_exit(1);
    }
    my_memset(p1, 0, 64);

    // Test 2: Medium allocation
    void* p2 = bb_malloc_safe(1024);
    if (!p2) {
        yos_write(2, "E2\n", 3);
        yos_exit(2);
    }
    my_memset(p2, 0, 1024);

    // Test 3: Allocation that fits (heap is 28464 bytes)
    void* p3 = bb_malloc_safe(20000);
    if (!p3) {
        yos_write(2, "E3\n", 3);
        yos_exit(3);
    }
    my_memset(p3, 0, 20000);

    // Test 4: This should fail - not enough space left
    void* p4 = bb_malloc_safe(10000);
    if (p4) {
        yos_write(2, "E4\n", 3);  // Should have failed
        yos_exit(4);
    }

    yos_write(1, "OK\n", 3);
    yos_exit(0);
}
