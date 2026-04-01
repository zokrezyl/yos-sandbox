// assert.h - stub for wasm32
#pragma once
#include "wasm-compat.h"

#ifdef NDEBUG
#define assert(expr) ((void)0)
#else
extern void __assert_fail(const char *expr, const char *file, unsigned int line, const char *func);
#define assert(expr) ((expr) ? (void)0 : __assert_fail(#expr, __FILE__, __LINE__, __func__))
#endif

#define static_assert _Static_assert
