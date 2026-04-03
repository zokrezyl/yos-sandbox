// Test: isatty() syscall
// Expected: fd 0,1,2 may or may not be tty depending on how we run, exits 0

__attribute__((import_module("yos"), import_name("isatty")))
int yos_isatty(int fd);

__attribute__((import_module("yos"), import_name("write")))
int yos_write(int fd, const void* buf, unsigned int count);

__attribute__((import_module("yos"), import_name("_exit")))
void yos_exit(int status);

void _start(void) {
    // isatty should return 0 or 1, not negative
    int ret0 = yos_isatty(0);
    if (ret0 < 0) {
        yos_write(2, "E1\n", 3);
        yos_exit(1);
    }

    int ret1 = yos_isatty(1);
    if (ret1 < 0) {
        yos_write(2, "E2\n", 3);
        yos_exit(2);
    }

    int ret2 = yos_isatty(2);
    if (ret2 < 0) {
        yos_write(2, "E3\n", 3);
        yos_exit(3);
    }

    // Invalid fd should return 0 (not a tty)
    int ret99 = yos_isatty(99);
    if (ret99 != 0) {
        yos_write(2, "E4\n", 3);
        yos_exit(4);
    }

    yos_write(1, "OK\n", 3);
    yos_exit(0);
}
