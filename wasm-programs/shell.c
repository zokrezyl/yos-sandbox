// YOS Shell - minimal shell for the YOS multi-process emulator
// Compiled to wasm32-wasi, linked with yos-stubs for fork/exec/wait

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "yos-stubs.h"

#define MAX_LINE 256
#define MAX_ARGS 32

// Scratch area for passing command to child (copied on fork)
#define SCRATCH_ADDR ((char*)4096)

static void print_prompt(void) {
    char buf[64];
    int n = snprintf(buf, sizeof(buf), "yos[%d]$ ", yos_getpid());
    yos_write(1, buf, n);
}

static int parse_line(char* line, char* argv[]) {
    int argc = 0;
    while (*line && argc < MAX_ARGS - 1) {
        while (*line == ' ' || *line == '\t') line++;
        if (*line == '\0' || *line == '\n') break;
        argv[argc++] = line;
        while (*line && *line != ' ' && *line != '\t' && *line != '\n') line++;
        if (*line) { *line = '\0'; line++; }
    }
    argv[argc] = NULL;
    return argc;
}

static int run_builtin(int argc, char* argv[]) {
    if (strcmp(argv[0], "exit") == 0) {
        yos_exit(argc > 1 ? atoi(argv[1]) : 0);
    }
    if (strcmp(argv[0], "pid") == 0) {
        char buf[64];
        int n = snprintf(buf, sizeof(buf), "pid=%d ppid=%d\n", yos_getpid(), yos_getppid());
        yos_write(1, buf, n);
        return 1;
    }
    if (strcmp(argv[0], "help") == 0) {
        const char* msg =
            "YOS Shell v0.1\n"
            "  help         - this help\n"
            "  pid          - show pid/ppid\n"
            "  exit [code]  - exit\n"
            "  <program>    - fork + exec .wasm program\n";
        yos_write(1, msg, strlen(msg));
        return 1;
    }
    return 0; // not a builtin
}

static void run_external(int argc, char* argv[]) {
    // Use spawn (combined fork+exec) to run a wasm program as child
    int child_pid = yos_spawn(argv[0], (const char* const*)argv);
    if (child_pid < 0) {
        char buf[128];
        int n = snprintf(buf, sizeof(buf), "%s: command not found\n", argv[0]);
        yos_write(2, buf, n);
        return;
    }

    // Wait for child
    int code = yos_wait(child_pid);
    if (code != 0) {
        char buf[64];
        int n = snprintf(buf, sizeof(buf), "exit code: %d\n", code);
        yos_write(1, buf, n);
    }
}

int main(void) {
    // Shell main loop
    const char* banner = "YOS Shell v0.1 - type 'help' for commands\n";
    yos_write(1, banner, strlen(banner));

    char line[MAX_LINE];

    while (1) {
        print_prompt();

        // Read one line (byte at a time to handle pipes correctly)
        int n = 0;
        while (n < MAX_LINE - 1) {
            char c;
            int r = yos_read(0, &c, 1);
            if (r <= 0) { if (n == 0) goto done; break; }
            if (c == '\n') break;
            line[n++] = c;
        }
        line[n] = '\0';
        if (line[0] == '\0') continue;

        char* argv[MAX_ARGS];
        int argc = parse_line(line, argv);
        if (argc == 0) continue;

        if (!run_builtin(argc, argv)) {
            run_external(argc, argv);
        }
    }

done:
    ;
    const char* bye = "\nbye\n";
    yos_write(1, bye, strlen(bye));
    yos_exit(0);
    return 0;
}
