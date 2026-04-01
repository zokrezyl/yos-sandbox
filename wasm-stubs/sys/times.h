// sys/times.h - stub for wasm32
#pragma once
#include "../wasm-compat.h"

typedef long clock_t;

struct tms {
    clock_t tms_utime;   // user time
    clock_t tms_stime;   // system time
    clock_t tms_cutime;  // user time of children
    clock_t tms_cstime;  // system time of children
};

extern clock_t times(struct tms *buf);
