// Test: printf/sprintf varargs functionality

__attribute__((import_module("env"), import_name("write")))
int yos_write(int fd, const void* buf, unsigned int count);

__attribute__((import_module("env"), import_name("_exit")))
void yos_exit(int status);

__attribute__((import_module("env"), import_name("printf")))
int yos_printf(const char* fmt, ...);

__attribute__((import_module("env"), import_name("sprintf")))
int yos_sprintf(char* str, const char* fmt, ...);

void write_str(const char* s) {
    int len = 0;
    while (s[len]) len++;
    yos_write(1, s, len);
}

void _start(void) {
    // Test 1: Simple printf
    int r = yos_printf("hello\n");
    if (r != 6) {
        write_str("E1: printf failed\n");
        yos_exit(1);
    }

    // Test 2: printf with integer
    r = yos_printf("num=%d\n", 42);
    if (r != 7) {
        write_str("E2: printf int failed\n");
        yos_exit(2);
    }

    // Test 3: printf with string
    r = yos_printf("str=%s\n", "test");
    if (r != 9) {
        write_str("E3: printf str failed\n");
        yos_exit(3);
    }

    // Test 4: sprintf
    char buf[64];
    r = yos_sprintf(buf, "x=%d", 123);
    if (r != 5 || buf[0] != 'x' || buf[2] != '1') {
        write_str("E4: sprintf failed\n");
        yos_exit(4);
    }

    write_str("OK\n");
    yos_exit(0);
}
