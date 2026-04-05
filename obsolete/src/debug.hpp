#pragma once

#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline int yos_debug_enabled(void) {
    static int enabled = -1;
    if (enabled < 0) enabled = (getenv("YOS_DEBUG") != NULL);
    return enabled;
}

static inline FILE* yos_debug_file(void) {
    static FILE* f = NULL;
    static int init = 0;
    if (!init) {
        init = 1;
        const char* path = getenv("YOS_TRACE_FILE");
        if (path) {
            f = fopen(path, "w");
        }
        if (!f) f = stderr;
    }
    return f;
}

#define YOS_DBG(fmt, ...) do { if (yos_debug_enabled()) fprintf(yos_debug_file(), fmt, ##__VA_ARGS__); } while(0)

#ifdef __cplusplus
}

namespace yos {
inline bool debugEnabled() { return yos_debug_enabled(); }
inline FILE* debugFile() { return yos_debug_file(); }
}
#endif
