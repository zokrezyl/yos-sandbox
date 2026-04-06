// Test: malloc/free using "env" module (like busybox does)
// Expected: exits 0 if malloc/free work correctly

__attribute__((import_module("env"), import_name("malloc")))
void* env_malloc(unsigned int size);

__attribute__((import_module("env"), import_name("free")))
void env_free(void* ptr);

__attribute__((import_module("env"), import_name("write")))
int env_write(int fd, const void* buf, unsigned int count);

__attribute__((import_module("env"), import_name("_exit")))
void env_exit(int status);

void _start(void) {
    // Test 1: malloc returns non-null
    char* p1 = (char*)env_malloc(64);
    if (p1 == 0) {
        env_write(2, "E1: null\n", 9);
        env_exit(1);
    }

    // Test 2: can write to memory
    p1[0] = 'H';
    p1[1] = 'i';
    p1[2] = '\n';
    p1[3] = 0;

    // Test 3: write works
    int ret = env_write(1, p1, 3);
    if (ret != 3) {
        env_write(2, "E3: write\n", 10);
        env_exit(3);
    }

    // Test 4: free doesn't crash
    env_free(p1);

    env_write(1, "OK\n", 3);
    env_exit(0);
}
