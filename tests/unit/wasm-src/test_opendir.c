// Test: opendir/readdir/closedir for directory listing

__attribute__((import_module("yos"), import_name("write")))
int yos_write(int fd, const void* buf, unsigned int count);

__attribute__((import_module("yos"), import_name("_exit")))
void yos_exit(int status);

__attribute__((import_module("yos"), import_name("opendir")))
void* yos_opendir(const char* path);

__attribute__((import_module("yos"), import_name("readdir")))
void* yos_readdir(void* dir);

__attribute__((import_module("yos"), import_name("closedir")))
int yos_closedir(void* dir);

void write_str(const char* s) {
    int len = 0;
    while (s[len]) len++;
    yos_write(1, s, len);
}

void _start(void) {
    void* dir = yos_opendir("/");
    if (!dir) {
        write_str("E1: opendir failed\n");
        yos_exit(1);
    }

    int count = 0;
    void* ent;
    while ((ent = yos_readdir(dir)) != 0) {
        count++;
        if (count > 1000) {
            write_str("E2: too many entries\n");
            yos_exit(2);
        }
    }

    if (count == 0) {
        write_str("E3: no entries\n");
        yos_exit(3);
    }

    int r = yos_closedir(dir);
    if (r != 0) {
        write_str("E4: closedir failed\n");
        yos_exit(4);
    }

    write_str("OK\n");
    yos_exit(0);
}
