// Test: lstat syscall (used by ls -l)

__attribute__((import_module("yos"), import_name("write")))
int yos_write(int fd, const void* buf, unsigned int count);

__attribute__((import_module("yos"), import_name("_exit")))
void yos_exit(int status);

__attribute__((import_module("yos"), import_name("lstat")))
int yos_lstat(const char* path, void* buf);

// Types matching wasm-compat.h
typedef unsigned long dev_t;
typedef unsigned long ino_t;
typedef unsigned int mode_t;
typedef unsigned long nlink_t;
typedef unsigned int uid_t;
typedef unsigned int gid_t;
typedef long long off_t;
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

void write_num(unsigned int n) {
    char buf[12];
    int i = 11;
    buf[i--] = 0;
    if (n == 0) buf[i--] = '0';
    while (n > 0) { buf[i--] = '0' + (n % 10); n /= 10; }
    write_str(&buf[i + 1]);
}

void _start(void) {
    struct stat st;
    
    int r = yos_lstat("/", &st);
    if (r != 0) {
        write_str("E1: lstat failed\n");
        yos_exit(1);
    }

    // Check directory
    if ((st.st_mode & 0170000) != 0040000) {
        write_str("E2: not dir mode=");
        write_num(st.st_mode);
        write_str("\n");
        yos_exit(2);
    }

    write_str("OK mode=");
    write_num(st.st_mode);
    write_str("\n");
    yos_exit(0);
}
