// Test: Verify dirent structure layout matches runtime expectations

__attribute__((import_module("yos"), import_name("write")))
int yos_write(int fd, const void* buf, unsigned int count);

__attribute__((import_module("yos"), import_name("_exit")))
void yos_exit(int status);

// WASM dirent from wasm-compat.h
struct dirent {
    unsigned long d_ino;
    unsigned long d_off;
    unsigned short d_reclen;
    unsigned char d_type;
    char d_name[256];
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

#define CHECK(cond, msg) if (!(cond)) { write_str("E: " msg "\n"); yos_exit(1); }

void _start(void) {
    struct dirent d;

    write_str("sizeof(dirent)=");
    write_num(sizeof(struct dirent));
    write_str("\n");

    write_str("d_ino offset=");
    write_num((char*)&d.d_ino - (char*)&d);
    write_str("\n");

    write_str("d_off offset=");
    write_num((char*)&d.d_off - (char*)&d);
    write_str("\n");

    write_str("d_reclen offset=");
    write_num((char*)&d.d_reclen - (char*)&d);
    write_str("\n");

    write_str("d_type offset=");
    write_num((char*)&d.d_type - (char*)&d);
    write_str("\n");

    write_str("d_name offset=");
    write_num((char*)&d.d_name - (char*)&d);
    write_str("\n");

    CHECK((char*)&d.d_ino - (char*)&d == 0, "d_ino offset");
    CHECK((char*)&d.d_off - (char*)&d == 4, "d_off offset");
    CHECK((char*)&d.d_reclen - (char*)&d == 8, "d_reclen offset");
    CHECK((char*)&d.d_type - (char*)&d == 10, "d_type offset");
    CHECK((char*)&d.d_name - (char*)&d == 11, "d_name offset");

    write_str("OK\n");
    yos_exit(0);
}
