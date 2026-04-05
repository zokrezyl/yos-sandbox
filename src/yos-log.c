// YOS Logging Implementation
#define _GNU_SOURCE
#include "yos-log.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>

int yos_log_level = YOS_LOG_INFO;

static const char* level_names[] = {"ERROR", "WARN", "INFO", "DEBUG", "TRACE"};

void yos_log_init(void) {
    const char* env = getenv("YOS_LOG_LEVEL");
    if (env) {
        if (env[0] >= '0' && env[0] <= '4') {
            yos_log_level = env[0] - '0';
        } else if (strcasecmp(env, "error") == 0) yos_log_level = YOS_LOG_ERROR;
        else if (strcasecmp(env, "warn") == 0)  yos_log_level = YOS_LOG_WARN;
        else if (strcasecmp(env, "info") == 0)  yos_log_level = YOS_LOG_INFO;
        else if (strcasecmp(env, "debug") == 0) yos_log_level = YOS_LOG_DEBUG;
        else if (strcasecmp(env, "trace") == 0) yos_log_level = YOS_LOG_TRACE;
    }
}

void yos_log(int level, const char* func, const char* fmt, ...) {
    if (level > yos_log_level) return;

    fprintf(stderr, "[YOS %s] %s: ", level_names[level], func);

    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    fprintf(stderr, "\n");
}
