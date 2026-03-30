// setjmp.h - stub for wasm without exception handling
// Real setjmp/longjmp requires wasm exceptions which wasm3 doesn't support
#ifndef _SETJMP_H
#define _SETJMP_H

typedef int jmp_buf[16];
typedef int sigjmp_buf[16];

int setjmp(jmp_buf env);
void longjmp(jmp_buf env, int val) __attribute__((noreturn));
int _setjmp(jmp_buf env);
void _longjmp(jmp_buf env, int val) __attribute__((noreturn));
int sigsetjmp(sigjmp_buf env, int savemask);
void siglongjmp(sigjmp_buf env, int val) __attribute__((noreturn));

#endif
