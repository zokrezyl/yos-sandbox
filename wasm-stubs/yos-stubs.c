// yos-stubs.c - Real POSIX implementations for WASM busybox
// Process functions call YOS runtime via wasm imports.
// Other functions have minimal but real implementations.

#include "yos-stubs.h"
#include "wasm-compat.h"
#include <string.h>
#include <errno.h>
#include <stdarg.h>

// ICF_GUARD prevents wasm-ld ICF from merging functions with identical bodies
#define ICF_GUARD(id) do { volatile int _icf = (id); (void)_icf; } while(0)

// =====================================================================
// Process management - calls YOS runtime
// fork and vfork have DIFFERENT semantics:
// - fork: copy memory, both run concurrently
// - vfork: parent blocks until child exec/exit
// =====================================================================

pid_t fork(void)  { return yos_fork(); }
pid_t vfork(void) { return yos_vfork(); }

__attribute__((used, noinline, visibility("default")))
pid_t getpid(void) { return yos_getpid(); }

__attribute__((used, noinline, visibility("default")))
pid_t getppid(void) { return yos_getppid(); }

int execvp(const char *file, char *const argv[]) {
    yos_exec(file, (const char *const *)argv);
    errno = ENOENT;
    return -1;
}
int execv(const char *path, char *const argv[]) { return execvp(path, argv); }
int execve(const char *path, char *const argv[], char *const envp[]) { return execvp(path, argv); }

pid_t waitpid(pid_t pid, int *status, int options) {
    if (options & WNOHANG) return 0;
    int code = yos_wait(pid);
    if (status) *status = (code & 0xff) << 8;
    return pid;
}
pid_t wait(int *status) { return waitpid(-1, status, 0); }

// =====================================================================
// Signals - stubs (no-op but real functions, not inlined away)
// =====================================================================

int sigaction(int sig, const struct sigaction *act, struct sigaction *oact) {
    if (oact) memset(oact, 0, sizeof(*oact));
    return 0;
}
int sigprocmask(int how, const sigset_t *set, sigset_t *oset) {
    if (oset) *oset = 0;
    return 0;
}
int sigpending(sigset_t *set) { if (set) *set = 0; return 0; }
int sigsuspend(const sigset_t *set) { errno = EINTR; return -1; }
int kill(pid_t pid, int sig) { ICF_GUARD(200); (void)pid; (void)sig; return 0; }
int killpg(pid_t pgrp, int sig) { ICF_GUARD(201); (void)pgrp; (void)sig; return 0; }
unsigned int alarm(unsigned int seconds) { ICF_GUARD(202); (void)seconds; return 0; }
int pause(void) { errno = EINTR; return -1; }

// signal/raise - normally from wasi-emulated-signal, we provide our own
typedef void (*sighandler_t)(int);
static sighandler_t _handlers[32] = {0};

sighandler_t signal(int sig, sighandler_t handler) {
    if (sig < 0 || sig >= 32) return (sighandler_t)-1;
    sighandler_t old = _handlers[sig];
    _handlers[sig] = handler;
    return old;
}

int raise(int sig) { return 0; }
int sigemptyset(sigset_t *set) { if(set) memset(set,0,sizeof(*set)); return 0; }
int sigfillset(sigset_t *set) { if(set) memset(set,0xff,sizeof(*set)); return 0; }
int sigaddset(sigset_t *set, int sig) { return 0; }
int sigdelset(sigset_t *set, int sig) { return 0; }
int sigismember(const sigset_t *set, int sig) { return 0; }

// =====================================================================
// Process groups / sessions - real implementations via YOS syscalls
// =====================================================================

pid_t getpgrp(void) { return yos_getpgrp(); }
pid_t getpgid(pid_t pid) { return pid ? yos_getsid(pid) : yos_getpgrp(); }
int setpgid(pid_t pid, pid_t pgid) { return yos_setpgid(pid, pgid); }
pid_t setsid(void) { return yos_setsid(); }
pid_t getsid(pid_t pid) { return yos_getsid(pid); }

// Terminal process group - returns foreground pgrp of terminal
pid_t tcgetpgrp(int fd) { (void)fd; return yos_getpgrp(); }
int tcsetpgrp(int fd, pid_t pgrp) { (void)fd; (void)pgrp; return 0; }

// =====================================================================
// User / group - single user system, always root (uid/gid 0)
// =====================================================================

uid_t getuid(void)  { ICF_GUARD(100); return 0; }
uid_t geteuid(void) { ICF_GUARD(101); return 0; }
gid_t getgid(void)  { ICF_GUARD(102); return 0; }
gid_t getegid(void) { ICF_GUARD(103); return 0; }
int setuid(uid_t u)  { ICF_GUARD(104); (void)u; return 0; }
int setgid(gid_t g)  { ICF_GUARD(105); (void)g; return 0; }
int seteuid(uid_t u) { ICF_GUARD(106); (void)u; return 0; }
int setegid(gid_t g) { ICF_GUARD(107); (void)g; return 0; }
int setreuid(uid_t r, uid_t e)  { ICF_GUARD(108); (void)r; (void)e; return 0; }
int setregid(gid_t r, gid_t e)  { ICF_GUARD(109); (void)r; (void)e; return 0; }
int setresuid(uid_t r, uid_t e, uid_t s) { ICF_GUARD(110); (void)r; (void)e; (void)s; return 0; }
int setresgid(gid_t r, gid_t e, gid_t s) { ICF_GUARD(111); (void)r; (void)e; (void)s; return 0; }
int getresuid(uid_t *r, uid_t *e, uid_t *s) { *r=*e=*s=0; return 0; }
int getresgid(gid_t *r, gid_t *e, gid_t *s) { *r=*e=*s=0; return 0; }
int getgroups(int sz, gid_t list[]) { ICF_GUARD(114); (void)sz; (void)list; return 0; }
int setgroups(size_t sz, const gid_t *list) { ICF_GUARD(115); (void)sz; (void)list; return 0; }
int initgroups(const char *user, gid_t group) { ICF_GUARD(116); (void)user; (void)group; return 0; }

static struct passwd _pw = {
    (char*)"root", (char*)"x", 0, 0,
    (char*)"root", (char*)"/", (char*)"/bin/sh"
};
static struct group _gr = { (char*)"root", (char*)"x", 0, 0 };

struct passwd *getpwuid(uid_t u) { return &_pw; }
struct passwd *getpwnam(const char *n) { return &_pw; }
struct passwd *getpwent(void) { return 0; }
void setpwent(void) {}
void endpwent(void) {}
struct group *getgrgid(gid_t g) { return &_gr; }
struct group *getgrnam(const char *n) { return &_gr; }
void endgrent(void) {}

// =====================================================================
// File operations
// =====================================================================

int pipe(int fd[2]) { errno = ENOSYS; return -1; }
int pipe2(int fd[2], int flags) { errno = ENOSYS; return -1; }
int dup(int fd) { errno = ENOSYS; return -1; }
int dup2(int oldfd, int newfd) { errno = ENOSYS; return -1; }
int dup3(int oldfd, int newfd, int flags) { errno = ENOSYS; return -1; }
int fcntl(int fd, int cmd, ...) { return 0; }
int ioctl(int fd, unsigned long req, ...) { return -1; }
int isatty(int fd) { return fd <= 2; }
int flock(int fd, int op) { return 0; }
int chown(const char *p, uid_t o, gid_t g) { return 0; }
int fchown(int fd, uid_t o, gid_t g) { return 0; }
int lchown(const char *p, uid_t o, gid_t g) { return 0; }
int mknod(const char *p, int m, int d) { errno = ENOSYS; return -1; }
int mkfifo(const char *p, int m) { errno = ENOSYS; return -1; }
// link, symlink, readlink, access, utime provided by wasi-libc
int umask(int m) { return 0022; }
int chdir(const char *path) { errno = ENOSYS; return -1; }
int fchdir(int fd) { errno = ENOSYS; return -1; }
long sysconf(int name) { return -1; }

static char _cwd[] = "/";
char *getcwd(char *buf, size_t size) {
    if (size < 2) { errno = ERANGE; return 0; }
    strcpy(buf, _cwd);
    return buf;
}

// =====================================================================
// Terminal
// =====================================================================

int tcgetattr(int fd, struct termios *t) { memset(t, 0, sizeof(*t)); return 0; }
int tcsetattr(int fd, int act, const struct termios *t) { return 0; }
speed_t cfgetispeed(const struct termios *t) { return B38400; }
speed_t cfgetospeed(const struct termios *t) { return B38400; }
int cfsetispeed(struct termios *t, speed_t s) { return 0; }
int cfsetospeed(struct termios *t, speed_t s) { return 0; }
int tcdrain(int fd) { return 0; }
int tcflush(int fd, int q) { return 0; }
int tcsendbreak(int fd, int d) { return 0; }
int tcflow(int fd, int action) { return 0; }

char *ttyname(int fd) { return (char*)"/dev/tty"; }

// =====================================================================
// Resource limits
// =====================================================================

int getrlimit(int r, struct rlimit *l) { l->rlim_cur = l->rlim_max = 1024; return 0; }
int setrlimit(int r, const struct rlimit *l) { return 0; }

// =====================================================================
// Sleep
// =====================================================================

unsigned int sleep(unsigned int s) { return 0; }
int usleep(unsigned int us) { return 0; }

// =====================================================================
// utsname
// =====================================================================

int uname(struct utsname *buf) {
    memset(buf, 0, sizeof(*buf));
    strcpy(buf->sysname, "YOS");
    strcpy(buf->nodename, "wasm");
    strcpy(buf->release, "0.1.0");
    strcpy(buf->version, "YOS WASM Runtime");
    strcpy(buf->machine, "wasm32");
    return 0;
}

int gethostname(char *n, size_t l) { strcpy(n, "wasm"); return 0; }
int chroot(const char *p) { errno = ENOSYS; return -1; }
int nice(int i) { return 0; }

// =====================================================================
// mmap
// =====================================================================

void *mmap(void *a, size_t l, int p, int f, int fd, long o) { return MAP_FAILED; }
int munmap(void *a, size_t l) { return -1; }
int mprotect(void *a, size_t l, int p) { return -1; }
int msync(void *a, size_t l, int f) { return -1; }
int madvise(void *a, size_t l, int adv) { return 0; }

// =====================================================================
// Network (stubs - not functional but not dead-code-eliminated)
// =====================================================================

const char *hstrerror(int err) { return "not supported"; }
struct hostent *gethostbyname(const char *n) { return 0; }
struct hostent *gethostbyaddr(const void *a, unsigned int l, int t) { return 0; }
struct servent *getservbyname(const char *n, const char *p) { return 0; }
struct servent *getservbyport(int p, const char *pr) { return 0; }
int getaddrinfo(const char *n, const char *s, const struct addrinfo *h, struct addrinfo **r) { return -1; }
void freeaddrinfo(struct addrinfo *r) {}
const char *gai_strerror(int e) { return "not supported"; }
int getnameinfo(const struct sockaddr *sa, socklen_t sl, char *h, socklen_t hl, char *s, socklen_t srvl, int f) { return -1; }

int socket(int d, int t, int p) { errno = ENOSYS; return -1; }
int bind(int s, const struct sockaddr *a, socklen_t l) { errno = ENOSYS; return -1; }
int listen(int s, int b) { errno = ENOSYS; return -1; }
int accept(int s, struct sockaddr *a, socklen_t *l) { errno = ENOSYS; return -1; }
int connect(int s, const struct sockaddr *a, socklen_t l) { errno = ENOSYS; return -1; }
long send(int s, const void *b, size_t l, int f) { errno = ENOSYS; return -1; }
long recv(int s, void *b, size_t l, int f) { errno = ENOSYS; return -1; }
long sendto(int s, const void *b, size_t l, int f, const struct sockaddr *a, socklen_t al) { errno = ENOSYS; return -1; }
long recvfrom(int s, void *b, size_t l, int f, struct sockaddr *a, socklen_t *al) { errno = ENOSYS; return -1; }
int setsockopt(int s, int lv, int n, const void *v, socklen_t l) { errno = ENOSYS; return -1; }
int getsockopt(int s, int lv, int n, void *v, socklen_t *l) { errno = ENOSYS; return -1; }
int getsockname(int s, struct sockaddr *a, socklen_t *l) { errno = ENOSYS; return -1; }
int getpeername(int s, struct sockaddr *a, socklen_t *l) { errno = ENOSYS; return -1; }
int shutdown(int s, int h) { errno = ENOSYS; return -1; }

unsigned int htonl(unsigned int h) { return __builtin_bswap32(h); }
unsigned short htons(unsigned short h) { return __builtin_bswap16(h); }
unsigned int ntohl(unsigned int n) { return __builtin_bswap32(n); }
unsigned short ntohs(unsigned short n) { return __builtin_bswap16(n); }
int inet_aton(const char *cp, struct in_addr *inp) { return 0; }
char *inet_ntoa(struct in_addr in) { return (char*)"0.0.0.0"; }
const char *inet_ntop(int af, const void *s, char *d, socklen_t sz) { strcpy(d, "0.0.0.0"); return d; }
int inet_pton(int af, const char *s, void *d) { return 0; }

// =====================================================================
// Poll / syslog / mntent
// =====================================================================

int poll(struct pollfd *fds, nfds_t nfds, int timeout) { errno = ENOSYS; return -1; }

void openlog(const char *i, int o, int f) {}
void closelog(void) {}
void syslog(int p, const char *fmt, ...) {}

void *setmntent(const char *f, const char *t) { return 0; }
struct mntent *getmntent(void *f) { return 0; }
int endmntent(void *f) { return 1; }

// =====================================================================
// main wrapper - wasi-libc expects 'main' but clang renames main(argc,argv)
// to __main_argc_argv. Use export_name to force "main" symbol.
// =====================================================================

extern int __main_argc_argv(int argc, char **argv);

__attribute__((export_name("main")))
int __yos_main_wrapper(int argc, char **argv) {
    return __main_argc_argv(argc, argv);
}

// =====================================================================
// Missing POSIX stubs
// =====================================================================
int mkstemp(char *tmpl) { errno = ENOSYS; return -1; }
int ttyname_r(int fd, char *buf, size_t len) { if (buf && len > 0) buf[0] = 0; return 0; }
int settimeofday(const void *tv, const void *tz) { return 0; }

// Signal - __SIG_IGN must be a function, not a pointer
void __SIG_IGN(int sig) { (void)sig; }

// setjmp/longjmp - minimal stubs (limited functionality without wasm exceptions)
// jmp_buf is typically an array - we use it to store a marker
typedef int jmp_buf[16];
int setjmp(jmp_buf env) { env[0] = 0; return 0; }
void longjmp(jmp_buf env, int val) { (void)env; (void)val; __builtin_trap(); }
int _setjmp(jmp_buf env) { return setjmp(env); }
void _longjmp(jmp_buf env, int val) { longjmp(env, val); }
int sigsetjmp(jmp_buf env, int savemask) { (void)savemask; return setjmp(env); }
void siglongjmp(jmp_buf env, int val) { longjmp(env, val); }
char *strsignal(int sig) { (void)sig; return (char*)"signal"; }

// Process
void *popen(const char *cmd, const char *mode) { (void)cmd; (void)mode; errno = ENOSYS; return 0; }
int pclose(void *stream) { (void)stream; errno = ENOSYS; return -1; }

// Time
int clock_settime(int clk, const void *tp) { (void)clk; (void)tp; return 0; }

// User/group
int getgrouplist(const char *user, gid_t group, gid_t *groups, int *ngroups) {
    (void)user;
    if (groups && ngroups && *ngroups > 0) { groups[0] = group; *ngroups = 1; }
    return 1;
}

// Device numbers
unsigned int major(unsigned long dev) { return (dev >> 8) & 0xff; }
unsigned int minor(unsigned long dev) { return dev & 0xff; }
