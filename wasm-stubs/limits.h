// limits.h - stub for wasm32
#pragma once

#define CHAR_BIT   8
#define SCHAR_MIN  (-128)
#define SCHAR_MAX  127
#define UCHAR_MAX  255
#define CHAR_MIN   0
#define CHAR_MAX   255
#define MB_LEN_MAX 4

#define SHRT_MIN   (-32768)
#define SHRT_MAX   32767
#define USHRT_MAX  65535

#define INT_MIN    (-2147483647-1)
#define INT_MAX    2147483647
#define UINT_MAX   4294967295U

#define LONG_MIN   (-2147483647L-1)
#define LONG_MAX   2147483647L
#define ULONG_MAX  4294967295UL

#define LLONG_MIN  (-9223372036854775807LL-1)
#define LLONG_MAX  9223372036854775807LL
#define ULLONG_MAX 18446744073709551615ULL

#define SSIZE_MAX  INT_MAX
#define PATH_MAX   4096
#define NAME_MAX   255
#define LINE_MAX   2048
#define PIPE_BUF   4096
#define NGROUPS_MAX 32
#define ARG_MAX    131072
#define LINK_MAX   127
#define HOST_NAME_MAX 64
#define LOGIN_NAME_MAX 256
#define TTY_NAME_MAX 32
