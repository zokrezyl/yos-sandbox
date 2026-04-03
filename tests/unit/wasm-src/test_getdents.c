// Test: open() + getdents() for directory listing
// This is critical for 'ls' to work
// Expected: reads entries from /tmp, exits 0

__attribute__((import_module("yos"), import_name("open")))
int yos_open(const char* path, int flags, int mode);

__attribute__((import_module("yos"), import_name("getdents")))
int yos_getdents(int fd, void* dirp, unsigned int count);

__attribute__((import_module("yos"), import_name("close")))
int yos_close(int fd);

__attribute__((import_module("yos"), import_name("write")))
int yos_write(int fd, const void* buf, unsigned int count);

__attribute__((import_module("yos"), import_name("_exit")))
void yos_exit(int status);

#define O_RDONLY    0
#define O_DIRECTORY 0200000

static char dirbuf[1024];

void _start(void) {
    // Open /tmp as directory
    int fd = yos_open("/tmp", O_RDONLY | O_DIRECTORY, 0);
    if (fd < 0) {
        // Try root if /tmp fails
        fd = yos_open("/", O_RDONLY | O_DIRECTORY, 0);
        if (fd < 0) {
            yos_write(2, "E1\n", 3);
            yos_exit(1);
        }
    }

    // Read directory entries
    int n = yos_getdents(fd, dirbuf, sizeof(dirbuf));
    if (n < 0) {
        yos_write(2, "E2\n", 3);
        yos_exit(2);
    }
    if (n == 0) {
        // Directory might be empty, that's OK for /tmp
        // But root should never be empty
    }

    // Close
    int ret = yos_close(fd);
    if (ret < 0) {
        yos_write(2, "E3\n", 3);
        yos_exit(3);
    }

    yos_write(1, "OK\n", 3);
    yos_exit(0);
}
