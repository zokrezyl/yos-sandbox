// Integration test: file I/O syscalls

__attribute__((import_module("env"), import_name("write")))
int env_write(int fd, const void* buf, unsigned int count);

__attribute__((import_module("env"), import_name("_exit")))
void env_exit(int status);

__attribute__((import_module("env"), import_name("open")))
int env_open(const char* path, int flags, int mode);

__attribute__((import_module("env"), import_name("read")))
int env_read(int fd, void* buf, unsigned int count);

__attribute__((import_module("env"), import_name("close")))
int env_close(int fd);

__attribute__((import_module("env"), import_name("stat")))
int env_stat(const char* path, void* statbuf);

__attribute__((import_module("env"), import_name("getcwd")))
char* env_getcwd(char* buf, unsigned int size);

#define O_RDONLY 0

static void print(const char* s) {
    int len = 0;
    while (s[len]) len++;
    env_write(1, s, len);
}

static void fail(const char* msg) {
    env_write(2, "FAIL: ", 6);
    print(msg);
    env_write(2, "\n", 1);
    env_exit(1);
}

void _start(void) {
    char buf[256];

    // Test 1: getcwd
    print("T1:getcwd ");
    char* cwd = env_getcwd(buf, sizeof(buf));
    if (cwd == 0) fail("getcwd failed");
    print("OK (");
    print(buf);
    print(")\n");

    // Test 2: stat on current dir
    print("T2:stat(.) ");
    char statbuf[128];  // enough for wasm_stat_t
    int ret = env_stat(".", statbuf);
    if (ret != 0) fail("stat . failed");
    print("OK\n");

    // Test 3: open /etc/passwd (should exist on Linux)
    print("T3:open ");
    int fd = env_open("/etc/passwd", O_RDONLY, 0);
    if (fd < 0) fail("open /etc/passwd failed");
    print("OK\n");

    // Test 4: read
    print("T4:read ");
    int n = env_read(fd, buf, 10);
    if (n <= 0) fail("read failed");
    print("OK\n");

    // Test 5: close
    print("T5:close ");
    ret = env_close(fd);
    if (ret != 0) fail("close failed");
    print("OK\n");

    print("\nAll file I/O tests passed!\n");
    env_exit(0);
}
