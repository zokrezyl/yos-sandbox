#pragma once

#include "wasm3.h"

namespace yos {

// Raw wasm3 syscall implementations
// All follow the M3RawCall signature: (IM3Runtime, IM3ImportContext, uint64_t*, void*) -> const void*

m3ApiRawFunction(syscall_fork);
m3ApiRawFunction(syscall_vfork);       // parent blocks until child exec/exit
m3ApiRawFunction(syscall_fork_result); // returns 0 in child, -1 in parent
m3ApiRawFunction(syscall_getpid);
m3ApiRawFunction(syscall_getppid);
m3ApiRawFunction(syscall_getpgrp);
m3ApiRawFunction(syscall_setpgid);
m3ApiRawFunction(syscall_setsid);
m3ApiRawFunction(syscall_getsid);
m3ApiRawFunction(syscall_exit);
m3ApiRawFunction(syscall_wait);
m3ApiRawFunction(syscall_write);
m3ApiRawFunction(syscall_read);
m3ApiRawFunction(syscall_exec);
m3ApiRawFunction(syscall_spawn); // fork+exec in one call

// WASI syscalls (minimal set for wasi-libc stdio)
m3ApiRawFunction(wasi_stub_enosys);
m3ApiRawFunction(wasi_fd_fdstat_get);
m3ApiRawFunction(wasi_fd_close);
m3ApiRawFunction(wasi_fd_seek);
m3ApiRawFunction(wasi_fd_write);
m3ApiRawFunction(wasi_fd_read);
m3ApiRawFunction(wasi_args_sizes_get);
m3ApiRawFunction(wasi_args_get);
m3ApiRawFunction(wasi_proc_exit);

} // namespace yos
