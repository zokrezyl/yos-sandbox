#pragma once

// YOS syscall imports - provided by the host runtime
// These are wasm imports from the "yos" module

__attribute__((import_module("yos"), import_name("fork")))
int yos_fork(void);

__attribute__((import_module("yos"), import_name("vfork")))
int yos_vfork(void);

// exec: path is null-terminated, argv is null-terminated array of null-terminated strings
__attribute__((import_module("yos"), import_name("execve")))
int yos_exec(const char* path, const char* const* argv, const char* const* envp);

__attribute__((import_module("yos"), import_name("getpid")))
int yos_getpid(void);

__attribute__((import_module("yos"), import_name("getppid")))
int yos_getppid(void);

__attribute__((import_module("yos"), import_name("getpgrp")))
int yos_getpgrp(void);

__attribute__((import_module("yos"), import_name("setpgid")))
int yos_setpgid(int pid, int pgid);

__attribute__((import_module("yos"), import_name("setsid")))
int yos_setsid(void);

__attribute__((import_module("yos"), import_name("getsid")))
int yos_getsid(int pid);

__attribute__((import_module("yos"), import_name("exit")))
void yos_exit(int code);

__attribute__((import_module("yos"), import_name("waitpid")))
int yos_wait(int pid, int* status, int options);

__attribute__((import_module("yos"), import_name("write")))
int yos_write(int fd, const void* buf, unsigned int len);

__attribute__((import_module("yos"), import_name("read")))
int yos_read(int fd, void* buf, unsigned int len);
