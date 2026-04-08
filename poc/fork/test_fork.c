// test_fork.c - debug local variable preservation

__attribute__((import_module("env"), import_name("fork")))
int fork(void);

__attribute__((import_module("env"), import_name("write")))
int write(int fd, const void* buf, unsigned int count);

__attribute__((import_module("env"), import_name("exit")))
void exit(int status);

__attribute__((import_module("env"), import_name("getpid")))
int getpid(void);

static void prints(const char* s) {
    int len = 0;
    while (s[len]) len++;
    write(1, s, len);
}

static void printi(int n) {
    if (n == 0) { write(1, "0", 1); return; }
    if (n < 0) { write(1, "-", 1); n = -n; }
    char buf[16];
    int i = 0;
    while (n > 0) { buf[i++] = '0' + (n % 10); n /= 10; }
    while (i > 0) write(1, &buf[--i], 1);
}

void _start(void) {
    int before = 42;
    prints("before fork: before="); printi(before); prints("\n");

    int pid = fork();

    prints("after fork: pid="); printi(getpid());
    prints(" fork_ret="); printi(pid);
    prints(" before="); printi(before);
    prints("\n");

    if (pid == 0) {
        prints("CHILD done\n");
    } else {
        prints("PARENT done\n");
    }
    exit(0);
}
