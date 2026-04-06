// Integration test: string functions (used heavily by busybox)

__attribute__((import_module("env"), import_name("write")))
int env_write(int fd, const void* buf, unsigned int count);

__attribute__((import_module("env"), import_name("_exit")))
void env_exit(int status);

__attribute__((import_module("env"), import_name("strlen")))
unsigned int env_strlen(const char* s);

__attribute__((import_module("env"), import_name("strcmp")))
int env_strcmp(const char* s1, const char* s2);

__attribute__((import_module("env"), import_name("strcpy")))
char* env_strcpy(char* dst, const char* src);

__attribute__((import_module("env"), import_name("strrchr")))
char* env_strrchr(const char* s, int c);

__attribute__((import_module("env"), import_name("strchr")))
char* env_strchr(const char* s, int c);

__attribute__((import_module("env"), import_name("memcpy")))
void* env_memcpy(void* dst, const void* src, unsigned int n);

__attribute__((import_module("env"), import_name("memset")))
void* env_memset(void* s, int c, unsigned int n);

static void print(const char* s) {
    int len = 0;
    while (s[len]) len++;
    env_write(1, s, len);
}

static void fail(const char* msg) {
    env_write(2, "FAIL: ", 6);
    print(msg);
    env_write(2, "\n", 1);
    env_exit(1);
}

void _start(void) {
    char buf[64];

    // Test 1: strlen
    print("T1:strlen ");
    if (env_strlen("hello") != 5) fail("strlen wrong");
    if (env_strlen("") != 0) fail("strlen empty wrong");
    print("OK\n");

    // Test 2: strcmp
    print("T2:strcmp ");
    if (env_strcmp("abc", "abc") != 0) fail("strcmp equal wrong");
    if (env_strcmp("abc", "abd") >= 0) fail("strcmp less wrong");
    if (env_strcmp("abd", "abc") <= 0) fail("strcmp greater wrong");
    print("OK\n");

    // Test 3: strcpy
    print("T3:strcpy ");
    env_strcpy(buf, "test");
    if (env_strcmp(buf, "test") != 0) fail("strcpy wrong");
    print("OK\n");

    // Test 4: strrchr
    print("T4:strrchr ");
    char* p = env_strrchr("/path/to/file", '/');
    if (p == 0) fail("strrchr null");
    if (env_strcmp(p, "/file") != 0) fail("strrchr wrong");
    print("OK\n");

    // Test 5: strchr
    print("T5:strchr ");
    p = env_strchr("hello", 'l');
    if (p == 0) fail("strchr null");
    if (env_strcmp(p, "llo") != 0) fail("strchr wrong");
    print("OK\n");

    // Test 6: memcpy
    print("T6:memcpy ");
    env_memcpy(buf, "ABCD", 4);
    buf[4] = 0;
    if (env_strcmp(buf, "ABCD") != 0) fail("memcpy wrong");
    print("OK\n");

    // Test 7: memset
    print("T7:memset ");
    env_memset(buf, 'X', 5);
    buf[5] = 0;
    if (env_strcmp(buf, "XXXXX") != 0) fail("memset wrong");
    print("OK\n");

    print("\nAll string tests passed!\n");
    env_exit(0);
}
