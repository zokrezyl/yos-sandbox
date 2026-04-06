// Test sbrk with actual memory access

__attribute__((import_module("env"), import_name("write")))
int env_write(int fd, const void* buf, unsigned int count);

__attribute__((import_module("env"), import_name("_exit")))
void env_exit(int status);

__attribute__((import_module("env"), import_name("sbrk")))
void* env_sbrk(int increment);

void _start(void) {
    // Test 1: Get current heap pointer
    void* heap0 = env_sbrk(0);
    if ((unsigned int)heap0 == 0xffffffff || heap0 == 0) {
        env_write(2, "E1: sbrk(0) failed\n", 19);
        env_exit(1);
    }
    
    // Test 2: Allocate 64 bytes
    void* mem1 = env_sbrk(64);
    if ((unsigned int)mem1 == 0xffffffff) {
        env_write(2, "E2: sbrk(64) failed\n", 20);
        env_exit(2);
    }
    
    // Test 3: Verify heap moved
    void* heap1 = env_sbrk(0);
    if ((unsigned int)heap1 != (unsigned int)mem1 + 64) {
        env_write(2, "E3: heap didn't move\n", 21);
        env_exit(3);
    }
    
    // Test 4: Write to allocated memory
    char* buf = (char*)mem1;
    buf[0] = 'O';
    buf[1] = 'K';
    buf[2] = '\n';
    buf[3] = 0;
    
    // Test 5: Read back
    if (buf[0] != 'O' || buf[1] != 'K') {
        env_write(2, "E5: memory corrupt\n", 19);
        env_exit(5);
    }
    
    env_write(1, buf, 3);
    env_exit(0);
}
