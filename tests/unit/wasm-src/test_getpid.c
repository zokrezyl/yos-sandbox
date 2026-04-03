// Test: getpid() syscall
// Expected: returns positive pid, exits 0

__attribute__((import_module("yos"), import_name("getpid")))
int yos_getpid(void);

__attribute__((import_module("yos"), import_name("write")))
int yos_write(int fd, const void* buf, unsigned int count);

__attribute__((import_module("yos"), import_name("_exit")))
void yos_exit(int status);

void _start(void) {
    int pid = yos_getpid();

    // PID should be positive (init process is 1)
    if (pid <= 0) {
        yos_write(2, "E1\n", 3);
        yos_exit(1);
    }

    // Calling again should return same value
    int pid2 = yos_getpid();
    if (pid != pid2) {
        yos_write(2, "E2\n", 3);
        yos_exit(2);
    }

    yos_write(1, "OK\n", 3);
    yos_exit(0);
}
