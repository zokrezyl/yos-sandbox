// Test: write() and _exit() syscalls work
// Expected: prints "OK" and exits 0

__attribute__((import_module("yos"), import_name("write")))
int yos_write(int fd, const void* buf, unsigned int count);

__attribute__((import_module("yos"), import_name("_exit")))
void yos_exit(int status);

void _start(void) {
    const char msg[] = "OK\n";
    int ret = yos_write(1, msg, 3);

    // write should return 3 (bytes written)
    if (ret != 3) {
        yos_exit(1);
    }

    yos_exit(0);
}
