// Test: open(), read(), close() syscalls
// Expected: reads /etc/hostname or similar, exits 0

__attribute__((import_module("yos"), import_name("open")))
int yos_open(const char* path, int flags, int mode);

__attribute__((import_module("yos"), import_name("read")))
int yos_read(int fd, void* buf, unsigned int count);

__attribute__((import_module("yos"), import_name("close")))
int yos_close(int fd);

__attribute__((import_module("yos"), import_name("write")))
int yos_write(int fd, const void* buf, unsigned int count);

__attribute__((import_module("yos"), import_name("_exit")))
void yos_exit(int status);

#define O_RDONLY 0

static char buf[256];

void _start(void) {
    // Open a file that should exist on any Linux system
    int fd = yos_open("/etc/hostname", O_RDONLY, 0);
    if (fd < 0) {
        // Try /etc/passwd as fallback
        fd = yos_open("/etc/passwd", O_RDONLY, 0);
        if (fd < 0) {
            yos_write(2, "E1\n", 3);
            yos_exit(1);
        }
    }

    // Read some bytes
    int n = yos_read(fd, buf, sizeof(buf) - 1);
    if (n < 0) {
        yos_write(2, "E2\n", 3);
        yos_exit(2);
    }
    if (n == 0) {
        yos_write(2, "E3\n", 3);
        yos_exit(3);  // File was empty
    }

    // Close the file
    int ret = yos_close(fd);
    if (ret < 0) {
        yos_write(2, "E4\n", 3);
        yos_exit(4);
    }

    yos_write(1, "OK\n", 3);
    yos_exit(0);
}
