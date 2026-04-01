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

#define YOS_DBG(fmt, ...) do { if (yos_debug_enabled()) fprintf(stderr, fmt, ##__VA_ARGS__); } while(0)

#ifdef __cplusplus
}

namespace yos {
inline bool debugEnabled() { return yos_debug_enabled(); }
}
#endif
