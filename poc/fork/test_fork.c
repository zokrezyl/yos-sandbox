// test_fork.c - WASM program that uses fork()
// Compile: clang --target=wasm32 -nostdlib -Wl,--no-entry -Wl,--export=_start -Wl,--allow-undefined -o test_fork_raw.wasm test_fork.c
// Then: wasm-opt --asyncify --pass-arg=asyncify-imports@env.fork -o test_fork.wasm test_fork_raw.wasm

__attribute__((import_module("env"), import_name("fork")))
int fork(void);

__attribute__((import_module("env"), import_name("write")))
int write(int fd, const void* buf, unsigned int count);

__attribute__((import_module("env"), import_name("exit")))
void exit(int status);

__attribute__((import_module("env"), import_name("getpid")))
int getpid(void);

static void print(const char* s) {
    int len = 0;
    while (s[len]) len++;
    write(1, s, len);
}

static void print_int(int n) {
    char buf[16];
    int i = 0;
    if (n == 0) { buf[i++] = '0'; }
    else {
        if (n < 0) { write(1, "-", 1); n = -n; }
        while (n > 0) { buf[i++] = '0' + (n % 10); n /= 10; }
    }
    while (i > 0) write(1, &buf[--i], 1);
}

void _start(void) {
    print("Before fork, pid=");
    print_int(getpid());
    print("\n");

    int pid = fork();

    print("After fork, returned ");
    print_int(pid);
    print("\n");

    if (pid == 0) {
        print("Child process, my pid=");
        print_int(getpid());
        print("\n");
        exit(0);
    } else {
        print("Parent process, child pid=");
        print_int(pid);
        print("\n");
        exit(0);
    }
}
