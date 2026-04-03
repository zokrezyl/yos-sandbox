// Test: Verify stat structure field offsets match runtime expectations

__attribute__((import_module("yos"), import_name("write")))
int yos_write(int fd, const void* buf, unsigned int count);

__attribute__((import_module("yos"), import_name("_exit")))
void yos_exit(int status);

// Types matching wasm-compat.h
typedef unsigned int uid_t;
typedef unsigned int gid_t;
typedef unsigned int mode_t;
typedef long long off_t;
typedef unsigned long dev_t;
typedef unsigned long ino_t;
typedef unsigned long nlink_t;
typedef unsigned long blksize_t;
typedef unsigned long blkcnt_t;
typedef long time_t;

struct timespec { time_t tv_sec; long tv_nsec; };

struct stat {
    dev_t st_dev;
    ino_t st_ino;
    mode_t st_mode;
    nlink_t st_nlink;
    uid_t st_uid;
    gid_t st_gid;
    dev_t st_rdev;
    off_t st_size;
    blksize_t st_blksize;
    blkcnt_t st_blocks;
    struct timespec st_atim;
    struct timespec st_mtim;
    struct timespec st_ctim;
};

void write_str(const char* s) {
    int len = 0;
    while (s[len]) len++;
    yos_write(1, s, len);
}

void write_num(int n) {
    char buf[12];
    int i = 11;
    buf[i--] = 0;
    if (n == 0) { buf[i--] = '0'; }
    else { while (n > 0) { buf[i--] = '0' + (n % 10); n /= 10; } }
    write_str(&buf[i + 1]);
}

#define CHECK_OFFSET(field, expected) \
    do { \
        int off = (char*)&s.field - (char*)&s; \
        if (off != expected) { \
            write_str("E: " #field " offset="); \
            write_num(off); \
            write_str(" expected="); \
            write_num(expected); \
            write_str("\n"); \
            yos_exit(1); \
        } \
    } while(0)

void _start(void) {
    struct stat s;

    // Check total size
    if (sizeof(struct stat) != 72) {
        write_str("E: sizeof(stat)=");
        write_num(sizeof(struct stat));
        write_str(" expected=72\n");
        yos_exit(1);
    }

    // Check all field offsets match runtime's wasm_stat
    CHECK_OFFSET(st_dev, 0);
    CHECK_OFFSET(st_ino, 4);
    CHECK_OFFSET(st_mode, 8);
    CHECK_OFFSET(st_nlink, 12);
    CHECK_OFFSET(st_uid, 16);
    CHECK_OFFSET(st_gid, 20);
    CHECK_OFFSET(st_rdev, 24);
    CHECK_OFFSET(st_size, 32);
    CHECK_OFFSET(st_blksize, 40);
    CHECK_OFFSET(st_blocks, 44);
    CHECK_OFFSET(st_atim, 48);
    CHECK_OFFSET(st_mtim, 56);
    CHECK_OFFSET(st_ctim, 64);

    write_str("OK: stat struct layout verified (72 bytes)\n");
    yos_exit(0);
}
