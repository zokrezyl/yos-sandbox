#pragma once

// YOS syscall imports - provided by the host runtime
// These are wasm imports from the "env" module

__attribute__((import_module("env"), import_name("yos_fork")))
int yos_fork(void);

__attribute__((import_module("env"), import_name("yos_vfork")))
int yos_vfork(void);

__attribute__((import_module("env"), import_name("yos_fork_result")))
int yos_fork_result(void);

// exec: path is null-terminated, argv is null-terminated array of null-terminated strings
__attribute__((import_module("env"), import_name("yos_exec")))
int yos_exec(const char* path, const char* const* argv);

// spawn: combined fork+exec, returns child pid to parent
__attribute__((import_module("env"), import_name("yos_spawn")))
int yos_spawn(const char* path, const char* const* argv);

__attribute__((import_module("env"), import_name("yos_getpid")))
int yos_getpid(void);

__attribute__((import_module("env"), import_name("yos_getppid")))
int yos_getppid(void);

__attribute__((import_module("env"), import_name("yos_getpgrp")))
int yos_getpgrp(void);

__attribute__((import_module("env"), import_name("yos_setpgid")))
int yos_setpgid(int pid, int pgid);

__attribute__((import_module("env"), import_name("yos_setsid")))
int yos_setsid(void);

__attribute__((import_module("env"), import_name("yos_getsid")))
int yos_getsid(int pid);

__attribute__((import_module("env"), import_name("yos_exit")))
void yos_exit(int code);

__attribute__((import_module("env"), import_name("yos_wait")))
int yos_wait(int child_pid);

__attribute__((import_module("env"), import_name("yos_write")))
int yos_write(int fd, const char* buf, int len);

__attribute__((import_module("env"), import_name("yos_read")))
int yos_read(int fd, char* buf, int len);
