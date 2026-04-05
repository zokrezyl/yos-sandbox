// YOS Logging
#ifndef YOS_LOG_H
#define YOS_LOG_H

#ifdef __cplusplus
extern "C" {
#endif

// Log levels
#define YOS_LOG_ERROR   0
#define YOS_LOG_WARN    1
#define YOS_LOG_INFO    2
#define YOS_LOG_DEBUG   3
#define YOS_LOG_TRACE   4

extern int yos_log_level;

void yos_log_init(void);
void yos_log(int level, const char* func, const char* fmt, ...);

#define YOS_ERROR(fmt, ...) yos_log(YOS_LOG_ERROR, __func__, fmt, ##__VA_ARGS__)
#define YOS_WARN(fmt, ...)  yos_log(YOS_LOG_WARN,  __func__, fmt, ##__VA_ARGS__)
#define YOS_INFO(fmt, ...)  yos_log(YOS_LOG_INFO,  __func__, fmt, ##__VA_ARGS__)
#define YOS_DEBUG(fmt, ...) yos_log(YOS_LOG_DEBUG, __func__, fmt, ##__VA_ARGS__)
#define YOS_TRACE(fmt, ...) yos_log(YOS_LOG_TRACE, __func__, fmt, ##__VA_ARGS__)

#define YOS_NOT_IMPL() do { \
    YOS_ERROR("NOT IMPLEMENTED"); \
    return -38; /* ENOSYS */ \
} while(0)

#ifdef __cplusplus
}
#endif

#endif // YOS_LOG_H
