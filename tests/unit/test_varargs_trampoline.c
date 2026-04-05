// Unit tests for varargs trampoline
// Tests that call_native_varargs correctly forwards arguments to native functions

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>

// Assembly trampoline
extern void* call_native_varargs(void* func, void* args, int arg_count);

// Test helper: count passed tests
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) static void test_##name(void)
#define RUN_TEST(name) do { \
    printf("  %-40s ", #name); \
    fflush(stdout); \
    test_##name(); \
    printf("PASS\n"); \
    fflush(stdout); \
    tests_passed++; \
} while(0)

#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        printf("FAIL\n    %s:%d: %lld != %lld\n", __FILE__, __LINE__, (long long)(a), (long long)(b)); \
        tests_failed++; \
        return; \
    } \
} while(0)

#define ASSERT_STREQ(a, b) do { \
    if (strcmp((a), (b)) != 0) { \
        printf("FAIL\n    %s:%d: '%s' != '%s'\n", __FILE__, __LINE__, (a), (b)); \
        tests_failed++; \
        return; \
    } \
} while(0)

// Test functions to call via trampoline
static int add_two(int a, int b) {
    return a + b;
}

static int add_six(int a, int b, int c, int d, int e, int f) {
    return a + b + c + d + e + f;
}

static int add_eight(int a, int b, int c, int d, int e, int f, int g, int h) {
    return a + b + c + d + e + f + g + h;
}

static char sprintf_buf[256];

// Simple variadic function to test trampoline
static int sum_ints(int count, ...) {
    va_list ap;
    va_start(ap, count);
    int sum = 0;
    for (int i = 0; i < count; i++) {
        sum += va_arg(ap, int);
    }
    va_end(ap);
    return sum;
}

// Tests

TEST(add_two_integers) {
    uint64_t args[2] = {10, 20};
    int result = (int)(intptr_t)call_native_varargs((void*)add_two, args, 2);
    ASSERT_EQ(result, 30);
}

TEST(add_six_integers_in_registers) {
    // First 6 args go in registers on x86_64
    uint64_t args[6] = {1, 2, 3, 4, 5, 6};
    int result = (int)(intptr_t)call_native_varargs((void*)add_six, args, 6);
    ASSERT_EQ(result, 21);
}

TEST(add_eight_integers_with_stack) {
    // Args 7 and 8 must go on stack
    uint64_t args[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    int result = (int)(intptr_t)call_native_varargs((void*)add_eight, args, 8);
    ASSERT_EQ(result, 36);
}

TEST(custom_varargs) {
    // Test our own variadic function via trampoline
    // sum_ints(3, 10, 20, 30) = 60
    uint64_t args[4] = {3, 10, 20, 30};
    int result = (int)(intptr_t)call_native_varargs((void*)sum_ints, args, 4);
    ASSERT_EQ(result, 60);
}

TEST(sprintf_direct) {
    // First verify sprintf works directly - use %d to prevent optimization
    sprintf(sprintf_buf, "Direct %d", 42);
    ASSERT_STREQ(sprintf_buf, "Direct 42");
}

TEST(sprintf_simple) {
    // sprintf(buf, "Hello") - 2 args total
    uint64_t args[2];
    args[0] = (uint64_t)(uintptr_t)sprintf_buf;
    args[1] = (uint64_t)(uintptr_t)"Hello";
    int result = (int)(intptr_t)call_native_varargs((void*)sprintf, args, 2);
    ASSERT_STREQ(sprintf_buf, "Hello");
    (void)result;
}

TEST(printf_one_string) {
    uint64_t args[3];
    args[0] = (uint64_t)(uintptr_t)sprintf_buf;
    args[1] = (uint64_t)(uintptr_t)"Hello %s";
    args[2] = (uint64_t)(uintptr_t)"World";
    int result = (int)(intptr_t)call_native_varargs((void*)sprintf, args, 3);
    ASSERT_STREQ(sprintf_buf, "Hello World");
    (void)result;
}

TEST(printf_one_integer) {
    uint64_t args[3];
    args[0] = (uint64_t)(uintptr_t)sprintf_buf;
    args[1] = (uint64_t)(uintptr_t)"Number: %d";
    args[2] = (uint64_t)42;
    int result = (int)(intptr_t)call_native_varargs((void*)sprintf, args, 3);
    ASSERT_STREQ(sprintf_buf, "Number: 42");
    (void)result;
}

TEST(printf_mixed_args) {
    // sprintf(buf, "%s=%d", "x", 99) - 4 args
    uint64_t args[4];
    args[0] = (uint64_t)(uintptr_t)sprintf_buf;
    args[1] = (uint64_t)(uintptr_t)"%s=%d";
    args[2] = (uint64_t)(uintptr_t)"x";
    args[3] = (uint64_t)99;
    int result = (int)(intptr_t)call_native_varargs((void*)sprintf, args, 4);
    ASSERT_STREQ(sprintf_buf, "x=99");
    (void)result;
}

TEST(printf_hex) {
    uint64_t args[3];
    args[0] = (uint64_t)(uintptr_t)sprintf_buf;
    args[1] = (uint64_t)(uintptr_t)"0x%x";
    args[2] = (uint64_t)0xDEAD;
    int result = (int)(intptr_t)call_native_varargs((void*)sprintf, args, 3);
    ASSERT_STREQ(sprintf_buf, "0xdead");
    (void)result;
}

int main(int argc, char** argv) {
    printf("Varargs Trampoline Tests:\n");

    RUN_TEST(add_two_integers);
    RUN_TEST(add_six_integers_in_registers);
    RUN_TEST(add_eight_integers_with_stack);
    RUN_TEST(custom_varargs);
    RUN_TEST(sprintf_direct);
    RUN_TEST(sprintf_simple);
    RUN_TEST(printf_one_string);
    RUN_TEST(printf_one_integer);
    RUN_TEST(printf_mixed_args);
    RUN_TEST(printf_hex);

    printf("\nResults: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
