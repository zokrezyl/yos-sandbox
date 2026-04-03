// Test: getcwd() syscall
// Expected: returns a valid path starting with /, exits 0

__attribute__((import_module("yos"), import_name("getcwd")))
int yos_getcwd(char* buf, unsigned int size);

__attribute__((import_module("yos"), import_name("write")))
int yos_write(int fd, const void* buf, unsigned int count);

__attribute__((import_module("yos"), import_name("_exit")))
void yos_exit(int status);

static char cwdbuf[256];

void _start(void) {
    int ret = yos_getcwd(cwdbuf, sizeof(cwdbuf));

    // Should return the buffer pointer (non-zero) on success
    if (ret == 0) {
        yos_write(2, "E1\n", 3);
        yos_exit(1);
    }

    // Path should start with /
    if (cwdbuf[0] != '/') {
        yos_write(2, "E2\n", 3);
        yos_exit(2);
    }

    yos_write(1, "OK\n", 3);
    yos_exit(0);
}
