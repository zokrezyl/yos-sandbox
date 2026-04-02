# YOS Sandbox - Current Status

## Working Commands

| Command | Status | Notes |
|---------|--------|-------|
| `echo hello` | WORKS | Fixed stpcpy pointer return, varargs size bug |
| `true` | WORKS | Exit code 0 |
| `false` | WORKS | Exit code 1 |
| `pwd` | WORKS | Fixed getcwd to return buf pointer on success |

## Broken Commands

| Command | Status | Issue |
|---------|--------|-------|
| `ls /` | BROKEN | Prints "ls:" with no output - likely opendir/readdir issue |
| `cat file` | BROKEN | Not tested yet |

## Fixed Bugs

1. **stpcpy pointer return** - Was returning host pointer, WASM expected WASM address. Changed to `wasm_impl`.

2. **varargs_call size vs pointer** - For `snprintf(buf, SIZE, fmt)`, arg2 (SIZE) was being converted as pointer via `m3ApiGetArgMem`. Changed args to `u32` and manually convert pointers.

3. **fflush crash** - Was passing WASM pointer as FILE*. Stubbed to return 0.

4. **getcwd return value** - POSIX returns buf pointer on success, NULL on error. Was returning 0 (success code). Fixed with custom extern handler.

## Known Issues

1. **Per-process cwd inheritance** - Fork doesn't copy parent's cwd to child. Each process starts with cwd="/".

2. **FILE* handling** - stdio functions (fopen, fclose, fprintf with FILE*) don't work. FILE* is host concept, not WASM.

3. **Passthrough ptr returns** - Functions like `getenv`, `strerror` return host memory pointers. Need to copy to WASM memory or stub.

4. **opendir/readdir** - Likely broken, needs investigation for `ls` to work.

## Architecture Issues

- Each process runs on OS thread (correct)
- Each process gets own VFS instance (correct)
- Each process gets own ProcessContext (correct)
- wasm3 user data set to ProcessContext (correct)

## Next Steps

1. Debug `ls` - likely opendir/readdir returning wrong values
2. Fix FILE* based stdio or stub it
3. Add cwd inheritance on fork
