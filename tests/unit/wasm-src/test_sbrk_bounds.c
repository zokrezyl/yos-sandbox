// Test: sbrk() returns addresses within valid heap bounds
// This catches the out-of-bounds memory access issue we saw with busybox
// Expected: all allocations stay within memory bounds, exits 0

__attribute__((import_module("yos"), import_name("sbrk")))
void* yos_sbrk(int increment);

__attribute__((import_module("yos"), import_name("write")))
int yos_write(int fd, const void* buf, unsigned int count);

__attribute__((import_module("yos"), import_name("_exit")))
void yos_exit(int status);

// We'll allocate in chunks and verify each is accessible
#define CHUNK_SIZE 4096
#define NUM_CHUNKS 8

void _start(void) {
    void* base = yos_sbrk(0);
    if (base == (void*)-1) {
        yos_write(2, "E1\n", 3);
        yos_exit(1);
    }

    // Allocate multiple chunks
    for (int i = 0; i < NUM_CHUNKS; i++) {
        void* p = yos_sbrk(CHUNK_SIZE);
        if (p == (void*)-1) {
            // Out of memory - that's expected eventually
            // But we should get at least a few chunks
            if (i < 2) {
                yos_write(2, "E2\n", 3);
                yos_exit(2);
            }
            break;
        }

        // Verify we can write to the entire chunk
        char* cp = (char*)p;
        for (int j = 0; j < CHUNK_SIZE; j++) {
            cp[j] = (char)(j & 0xFF);
        }

        // Verify what we wrote
        for (int j = 0; j < CHUNK_SIZE; j++) {
            if (cp[j] != (char)(j & 0xFF)) {
                yos_write(2, "E3\n", 3);
                yos_exit(3);
            }
        }
    }

    yos_write(1, "OK\n", 3);
    yos_exit(0);
}
