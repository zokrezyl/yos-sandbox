#pragma once

#include "wasm3.h"

namespace yos {

// Raw wasm3 syscall implementations
// All follow the M3RawCall signature: (IM3Runtime, IM3ImportContext, uint64_t*, void*) -> const void*

m3ApiRawFunction(syscall_fork);
m3ApiRawFunction(syscall_fork_result); // returns 0 in child, -1 in parent
m3ApiRawFunction(syscall_getpid);
m3ApiRawFunction(syscall_getppid);
m3ApiRawFunction(syscall_exit);
m3ApiRawFunction(syscall_wait);
m3ApiRawFunction(syscall_write);
m3ApiRawFunction(syscall_exec);

} // namespace yos
