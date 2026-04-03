#pragma once

#include <cstdint>

namespace yos {

// WASM32 stat structure - matches wasm-compat.h layout with natural alignment
// Layout (72 bytes total):
//   dev_t (4), ino_t (4), mode_t (4), nlink_t (4),
//   uid_t (4), gid_t (4), rdev (4), PADDING (4),
//   size (off_t = 8), blksize (4), blocks (4),
//   atim (8), mtim (8), ctim (8)
struct wasm_stat {
    uint32_t wasm_st_dev;        // offset 0
    uint32_t wasm_st_ino;        // offset 4
    uint32_t wasm_st_mode;       // offset 8
    uint32_t wasm_st_nlink;      // offset 12
    uint32_t wasm_st_uid;        // offset 16
    uint32_t wasm_st_gid;        // offset 20
    uint32_t wasm_st_rdev;       // offset 24
    uint32_t _pad0;              // offset 28 - padding for 8-byte alignment of st_size
    int64_t  wasm_st_size;       // offset 32
    uint32_t wasm_st_blksize;    // offset 40
    uint32_t wasm_st_blocks;     // offset 44
    int32_t  wasm_st_atim_sec;   // offset 48
    int32_t  wasm_st_atim_nsec;  // offset 52
    int32_t  wasm_st_mtim_sec;   // offset 56
    int32_t  wasm_st_mtim_nsec;  // offset 60
    int32_t  wasm_st_ctim_sec;   // offset 64
    int32_t  wasm_st_ctim_nsec;  // offset 68
};

static_assert(sizeof(wasm_stat) == 72, "wasm_stat must be 72 bytes");

// WASM32 dirent structure - matches wasm-compat.h layout
// Layout: d_ino(4) + d_off(4) + d_reclen(2) + d_type(1) + d_name(256) + pad(1) = 268
#pragma pack(push, 1)
struct wasm_dirent {
    uint32_t d_ino;         // offset 0
    uint32_t d_off;         // offset 4
    uint16_t d_reclen;      // offset 8
    uint8_t  d_type;        // offset 10
    char     d_name[256];   // offset 11
    uint8_t  _pad;          // offset 267, total 268
};
#pragma pack(pop)

static_assert(sizeof(wasm_dirent) == 268, "wasm_dirent must be 268 bytes");

} // namespace yos
