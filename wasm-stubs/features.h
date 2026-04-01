// features.h - stub for wasm32 (glibc compatibility)
#pragma once

// Feature test macros - all features enabled for compatibility
#define _DEFAULT_SOURCE 1
#define _BSD_SOURCE     1
#define _SVID_SOURCE    1
#define _POSIX_SOURCE   1
#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE   700
#define _XOPEN_SOURCE_EXTENDED 1
#define _GNU_SOURCE     1
#define _ATFILE_SOURCE  1

// Version macros
#define __GLIBC__       2
#define __GLIBC_MINOR__ 17
