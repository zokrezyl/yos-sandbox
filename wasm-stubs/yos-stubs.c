// yos-stubs.c - Supplementary POSIX implementations for WASM busybox
// Functions NOT provided by yos-generated.c (which handles syscalls from YAML)
// NO system headers - everything comes from wasm-compat.h

#include "wasm-compat.h"
#include "regex.h"
#include "sys/times.h"

// Locale struct
struct lconv {
    char *decimal_point;
    char *thousands_sep;
    char *grouping;
    char *int_curr_symbol;
    char *currency_symbol;
    char *mon_decimal_point;
    char *mon_thousands_sep;
    char *mon_grouping;
    char *positive_sign;
    char *negative_sign;
    char int_frac_digits;
    char frac_digits;
    char p_cs_precedes;
    char p_sep_by_space;
    char n_cs_precedes;
    char n_sep_by_space;
    char p_sign_posn;
    char n_sign_posn;
};

// errno storage
int errno;
int h_errno;

// ICF_GUARD prevents wasm-ld ICF from merging functions with identical bodies
#define ICF_GUARD(id) do { volatile int _icf = (id); (void)_icf; } while(0)

// =====================================================================
// Signals - stubs (no-op but real functions)
// =====================================================================

// sigaction, sigprocmask provided by yos-generated.c
int sigpending(sigset_t *set) { if (set) *set = 0; return 0; }
int sigsuspend(const sigset_t *set) { errno = EINTR; return -1; }
// kill provided by yos-generated.c
int killpg(pid_t pgrp, int sig) { ICF_GUARD(201); (void)pgrp; (void)sig; return 0; }
unsigned int alarm(unsigned int seconds) { ICF_GUARD(202); (void)seconds; return 0; }
int pause(void) { errno = EINTR; return -1; }

// signal provided by yos-generated.c

int raise(int sig) { return 0; }
int sigemptyset(sigset_t *set) { if(set) memset(set,0,sizeof(*set)); return 0; }
int sigfillset(sigset_t *set) { if(set) memset(set,0xff,sizeof(*set)); return 0; }
int sigaddset(sigset_t *set, int sig) { return 0; }
int sigdelset(sigset_t *set, int sig) { return 0; }
int sigismember(const sigset_t *set, int sig) { return 0; }

// =====================================================================
// Process groups - tcgetpgrp/tcsetpgrp not in generated code
// =====================================================================

pid_t tcgetpgrp(int fd) { (void)fd; return getpgrp(); }
int tcsetpgrp(int fd, pid_t pgrp) { (void)fd; (void)pgrp; return 0; }

// =====================================================================
// User / group lookups - not in generated code
// =====================================================================

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
// File operations not in generated code
// =====================================================================

// pipe, pipe2, dup2, ioctl provided by yos-generated.c
int dup3(int oldfd, int newfd, int flags) { errno = ENOSYS; return -1; }
// isatty provided by yos-generated.c
int flock(int fd, int op) { return 0; }
int lchown(const char *p, uid_t o, gid_t g) { return 0; }
int mknod(const char *p, mode_t m, dev_t d) { (void)p; (void)m; (void)d; errno = ENOSYS; return -1; }
int mkfifo(const char *p, mode_t m) { (void)p; (void)m; errno = ENOSYS; return -1; }
long sysconf(int name) { return -1; }

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
int ttyname_r(int fd, char *buf, size_t len) { if (buf && len > 0) strcpy(buf, "/dev/tty"); return 0; }

// =====================================================================
// Resource limits
// =====================================================================

// getrlimit, setrlimit provided by yos-generated.c

// =====================================================================
// Sleep
// =====================================================================

unsigned int sleep(unsigned int s) { return 0; }
int usleep(unsigned int us) { return 0; }
// nanosleep provided by yos-generated.c

// =====================================================================
// utsname
// =====================================================================

// uname provided by yos-generated.c

int gethostname(char *n, size_t l) { strcpy(n, "wasm"); return 0; }
int chroot(const char *p) { errno = ENOSYS; return -1; }
int nice(int i) { return 0; }

// =====================================================================
// mmap
// =====================================================================

void *mmap(void *a, size_t l, int p, int f, int fd, off_t o) { (void)a; (void)l; (void)p; (void)f; (void)fd; (void)o; return MAP_FAILED; }
int munmap(void *a, size_t l) { return -1; }
int mprotect(void *a, size_t l, int p) { return -1; }
int msync(void *a, size_t l, int f) { return -1; }
int madvise(void *a, size_t l, int adv) { return 0; }

// =====================================================================
// Network (stubs)
// =====================================================================

const char *hstrerror(int err) { return "not supported"; }
struct hostent *gethostbyname(const char *n) { return 0; }
struct hostent *gethostbyname2(const char *n, int af) { return 0; }
struct hostent *gethostbyaddr(const void *a, socklen_t l, int t) { return 0; }
struct servent *getservbyname(const char *n, const char *p) { return 0; }
struct servent *getservbyport(int p, const char *pr) { return 0; }
struct protoent *getprotobyname(const char *name) { return 0; }
struct protoent *getprotobynumber(int proto) { return 0; }
int getaddrinfo(const char *n, const char *s, const struct addrinfo *h, struct addrinfo **r) { return -1; }
void freeaddrinfo(struct addrinfo *r) {}
const char *gai_strerror(int e) { return "not supported"; }
int getnameinfo(const struct sockaddr *sa, socklen_t sl, char *h, socklen_t hl, char *s, socklen_t srvl, int f) { return -1; }

int socket(int d, int t, int p) { errno = ENOSYS; return -1; }
int bind(int s, const struct sockaddr *a, socklen_t l) { errno = ENOSYS; return -1; }
int listen(int s, int b) { errno = ENOSYS; return -1; }
int accept(int s, struct sockaddr *a, socklen_t *l) { errno = ENOSYS; return -1; }
int connect(int s, const struct sockaddr *a, socklen_t l) { errno = ENOSYS; return -1; }
ssize_t send(int s, const void *b, size_t l, int f) { errno = ENOSYS; return -1; }
ssize_t recv(int s, void *b, size_t l, int f) { errno = ENOSYS; return -1; }
ssize_t sendto(int s, const void *b, size_t l, int f, const struct sockaddr *a, socklen_t al) { errno = ENOSYS; return -1; }
ssize_t recvfrom(int s, void *b, size_t l, int f, struct sockaddr *a, socklen_t *al) { errno = ENOSYS; return -1; }
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
unsigned int if_nametoindex(const char *ifname) { return 0; }
char *if_indextoname(unsigned int ifindex, char *ifname) { return 0; }

// =====================================================================
// Poll / syslog / mntent
// =====================================================================

int poll(struct pollfd *fds, nfds_t nfds, int timeout) { errno = ENOSYS; return -1; }

void openlog(const char *i, int o, int f) {}
void closelog(void) {}
void syslog(int p, const char *fmt, ...) {}

FILE *setmntent(const char *f, const char *t) { return 0; }
struct mntent *getmntent(FILE *f) { return 0; }
int endmntent(FILE *f) { return 1; }

// =====================================================================
// setjmp/longjmp - minimal stubs
// =====================================================================

int setjmp(jmp_buf env) { env[0] = 0; return 0; }
void longjmp(jmp_buf env, int val) { __builtin_trap(); }
int _setjmp(jmp_buf env) { return setjmp(env); }
void _longjmp(jmp_buf env, int val) { longjmp(env, val); }
int sigsetjmp(jmp_buf env, int savemask) { return setjmp(env); }
void siglongjmp(jmp_buf env, int val) { longjmp(env, val); }
char *strsignal(int sig) { return (char*)"signal"; }
void __SIG_IGN(int sig) {}

// =====================================================================
// Process stubs
// =====================================================================

FILE *popen(const char *cmd, const char *mode) { errno = ENOSYS; return 0; }
int pclose(FILE *stream) { errno = ENOSYS; return -1; }
int execvp(const char *file, char *const argv[]) { errno = ENOSYS; return -1; }
int execv(const char *path, char *const argv[]) { errno = ENOSYS; return -1; }
pid_t getpgid(pid_t pid) { return pid == 0 ? getpgrp() : getsid(pid); }

// =====================================================================
// Time
// =====================================================================

int settimeofday(const struct timeval *tv, const struct timezone *tz) { return 0; }
int clock_settime(clockid_t clk, const struct timespec *tp) { return 0; }
clock_t times(struct tms *buf) { if (buf) memset(buf, 0, sizeof(*buf)); return 0; }

// =====================================================================
// User/group
// =====================================================================

int getgrouplist(const char *user, gid_t group, gid_t *groups, int *ngroups) {
    if (groups && ngroups && *ngroups > 0) { groups[0] = group; *ngroups = 1; }
    return 1;
}

// setgroups provided by yos-generated.c
int initgroups(const char *user, gid_t group) { return 0; }
int setreuid(uid_t r, uid_t e) { return 0; }
int setregid(gid_t r, gid_t e) { return 0; }
int seteuid(uid_t u) { return 0; }
int setegid(gid_t g) { return 0; }
int setresuid(uid_t r, uid_t e, uid_t s) { return 0; }
int setresgid(gid_t r, gid_t e, gid_t s) { return 0; }
int getresuid(uid_t *r, uid_t *e, uid_t *s) { if(r)*r=0; if(e)*e=0; if(s)*s=0; return 0; }
int getresgid(gid_t *r, gid_t *e, gid_t *s) { if(r)*r=0; if(e)*e=0; if(s)*s=0; return 0; }

// =====================================================================
// Device numbers
// =====================================================================

unsigned int gnu_dev_major(unsigned long dev) { return (dev >> 8) & 0xff; }
unsigned int gnu_dev_minor(unsigned long dev) { return dev & 0xff; }
unsigned long gnu_dev_makedev(unsigned int maj, unsigned int min) { return ((maj & 0xff) << 8) | (min & 0xff); }
#define major(dev) gnu_dev_major(dev)
#define minor(dev) gnu_dev_minor(dev)
#define makedev(maj, min) gnu_dev_makedev(maj, min)

// =====================================================================
// Rusage
// =====================================================================

// getrusage provided by yos-generated.c

// =====================================================================
// Priority
// =====================================================================

int getpriority(int which, int who) { return 0; }
int setpriority(int which, int who, int prio) { return 0; }

// =====================================================================
// Hostname
// =====================================================================

static char _hostname[256] = "wasm";
int sethostname(const char *name, size_t len) {
    if (!name || len >= sizeof(_hostname)) { errno = EINVAL; return -1; }
    memcpy(_hostname, name, len);
    _hostname[len] = 0;
    return 0;
}

// =====================================================================
// Mktemp
// =====================================================================

static unsigned int _rand_seed = 12345;
static unsigned int _simple_rand(void) {
    _rand_seed = _rand_seed * 1103515245 + 12345;
    return (_rand_seed >> 16) & 0x7fff;
}

char *mktemp(char *tmpl) {
    if (!tmpl) { errno = EINVAL; return tmpl; }
    size_t len = strlen(tmpl);
    if (len < 6) { errno = EINVAL; return tmpl; }
    char *p = tmpl + len - 6;
    for (int i = 0; i < 6; i++) {
        if (p[i] != 'X') { errno = EINVAL; return tmpl; }
    }
    static const char chars[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    for (int i = 0; i < 6; i++) {
        p[i] = chars[_simple_rand() % (sizeof(chars) - 1)];
    }
    return tmpl;
}

char *mkdtemp(char *tmpl) {
    mktemp(tmpl);
    if (mkdir(tmpl, 0700) < 0) return 0;
    return tmpl;
}

int mkstemp(char *tmpl) {
    mktemp(tmpl);
    return open(tmpl, O_RDWR | O_CREAT | O_EXCL, 0600);
}

// =====================================================================
// Sched
// =====================================================================

int sched_getaffinity(pid_t pid, size_t cpusetsize, cpu_set_t *mask) {
    if (mask && cpusetsize >= sizeof(cpu_set_t)) {
        memset(mask, 0, sizeof(cpu_set_t));
        CPU_SET(0, mask);
    }
    return 0;
}

int sched_setaffinity(pid_t pid, size_t cpusetsize, const cpu_set_t *mask) { return 0; }

// =====================================================================
// Sysinfo
// =====================================================================

// sysinfo provided by yos-generated.c

// =====================================================================
// Select
// =====================================================================

int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout) {
    if (readfds) FD_ZERO(readfds);
    if (writefds) FD_ZERO(writefds);
    if (exceptfds) FD_ZERO(exceptfds);
    return 0;
}

int pselect(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, const struct timespec *timeout, const sigset_t *sigmask) {
    return select(nfds, readfds, writefds, exceptfds, 0);
}

int __ppoll_time64(struct pollfd *fds, nfds_t nfds, const struct timespec *tmo_p, const sigset_t *sigmask) {
    for (nfds_t i = 0; i < nfds; i++) {
        fds[i].revents = fds[i].events & (POLLIN | POLLOUT);
    }
    return (int)nfds;
}

// =====================================================================
// Glob / fnmatch
// =====================================================================

int fnmatch(const char *pattern, const char *string, int flags) {
    // Simple implementation - just check if equal for now
    return strcmp(pattern, string) == 0 ? 0 : 1;
}

// =====================================================================
// Assert
// =====================================================================

void __assert_fail(const char *expr, const char *file, unsigned int line, const char *func) {
    __builtin_trap();
}

// =====================================================================
// Regex - minimal stubs
// =====================================================================

int regcomp(regex_t *preg, const char *regex, int cflags) { return 0; }
int regexec(const regex_t *preg, const char *string, size_t nmatch, regmatch_t pmatch[], int eflags) { return 1; }
size_t regerror(int errcode, const regex_t *preg, char *errbuf, size_t errbuf_size) { if (errbuf && errbuf_size) errbuf[0] = 0; return 0; }
void regfree(regex_t *preg) {}

// =====================================================================
// Locale - minimal stubs
// =====================================================================

char *setlocale(int category, const char *locale) { return (char*)"C"; }
struct lconv *localeconv(void) { static struct lconv lc = {0}; return &lc; }

// =====================================================================
// Getopt - minimal implementation
// =====================================================================

char *optarg = 0;
int optind = 1;
int opterr = 1;
int optopt = '?';

int getopt(int argc, char * const argv[], const char *optstring) {
    static int optpos = 1;

    if (optind >= argc || argv[optind] == 0 || argv[optind][0] != '-' || argv[optind][1] == 0) {
        return -1;
    }
    if (argv[optind][1] == '-' && argv[optind][2] == 0) {
        optind++;
        return -1;
    }

    int c = argv[optind][optpos];
    const char *p = strchr(optstring, c);

    if (!p || c == ':') {
        optopt = c;
        if (optstring[0] != ':') {
            // Error message would go here
        }
        if (argv[optind][++optpos] == 0) {
            optind++;
            optpos = 1;
        }
        return '?';
    }

    if (p[1] == ':') {
        if (argv[optind][optpos + 1]) {
            optarg = &argv[optind][optpos + 1];
            optind++;
            optpos = 1;
        } else if (optind + 1 < argc) {
            optarg = argv[++optind];
            optind++;
            optpos = 1;
        } else {
            optopt = c;
            if (optstring[0] == ':') return ':';
            return '?';
        }
    } else {
        if (argv[optind][++optpos] == 0) {
            optind++;
            optpos = 1;
        }
        optarg = 0;
    }

    return c;
}

int getopt_long(int argc, char * const argv[], const char *optstring,
                const struct option *longopts, int *longindex) {
    return getopt(argc, argv, optstring);
}

int getopt_long_only(int argc, char * const argv[], const char *optstring,
                     const struct option *longopts, int *longindex) {
    return getopt(argc, argv, optstring);
}

// =====================================================================
// strchrnul - GNU extension
// =====================================================================

char *strchrnul(const char *s, int c) {
    while (*s && *s != c) s++;
    return (char*)s;
}

// All C library functions are now provided by yos-generated.c
// via passthrough to native libc
