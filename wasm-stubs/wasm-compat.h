// wasm-compat.h - POSIX compatibility for compiling busybox on wasm32
// Uses only our generated YOS stubs - no wasi-libc
// All types and functions defined here or in generated headers

#pragma once

// Include our stdint.h for basic integer types
#include "stdint.h"

// =====================================================================
// Basic types - freestanding (no libc)
// =====================================================================
typedef unsigned long size_t;
typedef long ssize_t;
typedef int ptrdiff_t;

#define NULL ((void*)0)

// =====================================================================
// POSIX types
// =====================================================================
typedef int pid_t;
typedef unsigned int uid_t;
typedef unsigned int gid_t;
typedef unsigned int mode_t;
typedef long long off_t;
typedef unsigned long dev_t;
typedef unsigned long ino_t;
typedef unsigned long nlink_t;
typedef unsigned long blksize_t;
typedef unsigned long blkcnt_t;
typedef long time_t;
typedef long suseconds_t;
typedef unsigned int socklen_t;
typedef unsigned long sigset_t;
typedef int clockid_t;

// =====================================================================
// Errno
// =====================================================================
extern int errno;

#define EPERM           1
#define ENOENT          2
#define ESRCH           3
#define EINTR           4
#define EIO             5
#define ENXIO           6
#define E2BIG           7
#define ENOEXEC         8
#define EBADF           9
#define ECHILD          10
#define EAGAIN          11
#define ENOMEM          12
#define EACCES          13
#define EFAULT          14
#define ENOTBLK         15
#define EBUSY           16
#define EEXIST          17
#define EXDEV           18
#define ENODEV          19
#define ENOTDIR         20
#define EISDIR          21
#define EINVAL          22
#define ENFILE          23
#define EMFILE          24
#define ENOTTY          25
#define ETXTBSY         26
#define EFBIG           27
#define ENOSPC          28
#define ESPIPE          29
#define EROFS           30
#define EMLINK          31
#define EPIPE           32
#define EDOM            33
#define ERANGE          34
#define EDEADLK         35
#define ENAMETOOLONG    36
#define ENOLCK          37
#define ENOSYS          38
#define ENOTEMPTY       39
#define ELOOP           40
#define EWOULDBLOCK     EAGAIN
#define ENOMSG          42
#define EIDRM           43
#define ENODATA         61
#define ETIME           62
#define ENOSR           63
#define ENOSTR          60
#define EOVERFLOW       75
#define EILSEQ          84
#define ENOTSOCK        88
#define EDESTADDRREQ    89
#define EMSGSIZE        90
#define EPROTOTYPE      91
#define ENOPROTOOPT     92
#define EPROTONOSUPPORT 93
#define EOPNOTSUPP      95
#define EAFNOSUPPORT    97
#define EADDRINUSE      98
#define EADDRNOTAVAIL   99
#define ENETDOWN        100
#define ENETUNREACH     101
#define ECONNABORTED    103
#define ECONNRESET      104
#define ENOBUFS         105
#define EISCONN         106
#define ENOTCONN        107
#define ETIMEDOUT       110
#define ECONNREFUSED    111
#define EHOSTUNREACH    113
#define EALREADY        114
#define EINPROGRESS     115

// =====================================================================
// String functions - provided by generated code
// =====================================================================
extern void *memcpy(void *dest, const void *src, size_t n);
extern void *memmove(void *dest, const void *src, size_t n);
extern void *memset(void *s, int c, size_t n);
extern int memcmp(const void *s1, const void *s2, size_t n);
extern void *memchr(const void *s, int c, size_t n);

extern size_t strlen(const char *s);
extern char *strcpy(char *dest, const char *src);
extern char *strncpy(char *dest, const char *src, size_t n);
extern char *strcat(char *dest, const char *src);
extern char *strncat(char *dest, const char *src, size_t n);
extern int strcmp(const char *s1, const char *s2);
extern int strncmp(const char *s1, const char *s2, size_t n);
extern char *strchr(const char *s, int c);
extern char *strrchr(const char *s, int c);
extern char *strchrnul(const char *s, int c);
extern char *strstr(const char *haystack, const char *needle);
extern char *strdup(const char *s);
extern char *strndup(const char *s, size_t n);
extern size_t strspn(const char *s, const char *accept);
extern size_t strcspn(const char *s, const char *reject);
extern char *strpbrk(const char *s, const char *accept);
extern char *strtok(char *str, const char *delim);
extern char *strtok_r(char *str, const char *delim, char **saveptr);
extern char *strerror(int errnum);
extern int strcasecmp(const char *s1, const char *s2);
extern int strncasecmp(const char *s1, const char *s2, size_t n);

// =====================================================================
// Ctype functions - provided by generated code
// =====================================================================
extern int isalnum(int c);
extern int isalpha(int c);
extern int isdigit(int c);
extern int isxdigit(int c);
extern int isspace(int c);
extern int isupper(int c);
extern int islower(int c);
extern int isprint(int c);
extern int isgraph(int c);
extern int iscntrl(int c);
extern int ispunct(int c);
extern int isblank(int c);
extern int toupper(int c);
extern int tolower(int c);

// =====================================================================
// Conversion functions
// =====================================================================
extern long strtol(const char *nptr, char **endptr, int base);
extern unsigned long strtoul(const char *nptr, char **endptr, int base);
extern long long strtoll(const char *nptr, char **endptr, int base);
extern unsigned long long strtoull(const char *nptr, char **endptr, int base);
extern int atoi(const char *nptr);
extern long atol(const char *nptr);
extern long long atoll(const char *nptr);

// =====================================================================
// Memory allocation - provided by generated code
// =====================================================================
extern void *malloc(size_t size);
extern void *calloc(size_t nmemb, size_t size);
extern void *realloc(void *ptr, size_t size);
extern void free(void *ptr);

// =====================================================================
// I/O functions
// =====================================================================
typedef struct _FILE FILE;
extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

extern int printf(const char *format, ...);
extern int fprintf(FILE *stream, const char *format, ...);
extern int dprintf(int fd, const char *format, ...);
extern int vdprintf(int fd, const char *format, __builtin_va_list ap);
extern int sprintf(char *str, const char *format, ...);
extern int snprintf(char *str, size_t size, const char *format, ...);
extern int vprintf(const char *format, __builtin_va_list ap);
extern int vfprintf(FILE *stream, const char *format, __builtin_va_list ap);
extern int vsprintf(char *str, const char *format, __builtin_va_list ap);
extern int vsnprintf(char *str, size_t size, const char *format, __builtin_va_list ap);

extern int scanf(const char *format, ...);
extern int fscanf(FILE *stream, const char *format, ...);
extern int sscanf(const char *str, const char *format, ...);

extern int fgetc(FILE *stream);
extern char *fgets(char *s, int size, FILE *stream);
extern int fputc(int c, FILE *stream);
extern int fputs(const char *s, FILE *stream);
extern int getc(FILE *stream);
extern int getchar(void);
extern int putc(int c, FILE *stream);
extern int putchar(int c);
extern int puts(const char *s);
extern int ungetc(int c, FILE *stream);

extern size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
extern size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);

extern FILE *fopen(const char *pathname, const char *mode);
extern FILE *fdopen(int fd, const char *mode);
extern FILE *freopen(const char *pathname, const char *mode, FILE *stream);
extern int fclose(FILE *stream);
extern int fflush(FILE *stream);
extern int feof(FILE *stream);
extern int ferror(FILE *stream);
extern void clearerr(FILE *stream);
extern int fileno(FILE *stream);

extern int fseek(FILE *stream, long offset, int whence);
extern long ftell(FILE *stream);
extern void rewind(FILE *stream);
extern int fseeko(FILE *stream, off_t offset, int whence);
extern off_t ftello(FILE *stream);

extern void perror(const char *s);
extern int remove(const char *pathname);
extern int rename(const char *oldpath, const char *newpath);
extern FILE *tmpfile(void);
extern char *tmpnam(char *s);

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#define EOF (-1)
#define BUFSIZ 8192
#define _IONBF 2
#define _IOLBF 1
#define _IOFBF 0
extern int setvbuf(FILE *stream, char *buf, int mode, size_t size);
extern void setbuf(FILE *stream, char *buf);
extern void setbuffer(FILE *stream, char *buf, size_t size);
extern void setlinebuf(FILE *stream);

// =====================================================================
// Process management
// =====================================================================
extern pid_t fork(void);
extern pid_t vfork(void);
extern pid_t getpid(void);
extern pid_t getppid(void);
extern int execvp(const char *file, char *const argv[]);
extern int execv(const char *path, char *const argv[]);
extern int execve(const char *path, char *const argv[], char *const envp[]);
extern pid_t waitpid(pid_t pid, int *status, int options);
extern pid_t wait(int *status);
extern void _exit(int status);
extern void exit(int status);

// =====================================================================
// Option parsing (getopt)
// =====================================================================
extern char *optarg;
extern int optind;
extern int opterr;
extern int optopt;

struct option {
    const char *name;
    int has_arg;
    int *flag;
    int val;
};

#define no_argument       0
#define required_argument 1
#define optional_argument 2

extern int getopt(int argc, char * const argv[], const char *optstring);
extern int getopt_long(int argc, char * const argv[], const char *optstring,
                       const struct option *longopts, int *longindex);
extern int getopt_long_only(int argc, char * const argv[], const char *optstring,
                            const struct option *longopts, int *longindex);

// =====================================================================
// File operations
// =====================================================================
extern int open(const char *pathname, int flags, ...);
extern int close(int fd);
extern ssize_t read(int fd, void *buf, size_t count);
extern ssize_t write(int fd, const void *buf, size_t count);
extern off_t lseek(int fd, off_t offset, int whence);
extern int dup(int oldfd);
extern int dup2(int oldfd, int newfd);
extern int fcntl(int fd, int cmd, ...);
extern int ioctl(int fd, unsigned long request, ...);
extern int isatty(int fd);
extern int pipe(int pipefd[2]);
extern int pipe2(int pipefd[2], int flags);
extern int dup3(int oldfd, int newfd, int flags);
extern int chdir(const char *path);
extern int fchdir(int fd);
extern char *getcwd(char *buf, size_t size);
extern int chown(const char *pathname, uid_t owner, gid_t group);
extern int fchown(int fd, uid_t owner, gid_t group);
extern int lchown(const char *pathname, uid_t owner, gid_t group);
extern int chmod(const char *pathname, mode_t mode);
extern int fchmod(int fd, mode_t mode);
extern mode_t umask(mode_t mask);
extern int link(const char *oldpath, const char *newpath);
extern int unlink(const char *pathname);
extern int symlink(const char *target, const char *linkpath);
extern ssize_t readlink(const char *pathname, char *buf, size_t bufsiz);
extern int mkdir(const char *pathname, mode_t mode);
extern int rmdir(const char *pathname);
extern int mknod(const char *pathname, mode_t mode, dev_t dev);
extern int mkfifo(const char *pathname, mode_t mode);
extern int access(const char *pathname, int mode);
extern int flock(int fd, int operation);
extern int ftruncate(int fd, off_t length);
extern int truncate(const char *path, off_t length);
extern int fsync(int fd);

#define O_RDONLY    0
#define O_WRONLY    1
#define O_RDWR      2
#define O_CREAT     0100
#define O_EXCL      0200
#define O_NOCTTY    0400
#define O_TRUNC     01000
#define O_APPEND    02000
#define O_NONBLOCK  04000
#define O_CLOEXEC   02000000
#define O_DIRECTORY 0200000
#define O_NOFOLLOW  0400000
#define O_DSYNC     010000
#define O_SYNC      04010000
#define O_RSYNC     04010000
#define O_NDELAY    O_NONBLOCK
#define O_LARGEFILE 0100000

#define F_DUPFD         0
#define F_GETFD         1
#define F_SETFD         2
#define F_GETFL         3
#define F_SETFL         4
#define F_GETLK         5
#define F_SETLK         6
#define F_SETLKW        7
#define F_DUPFD_CLOEXEC 1030
#define FD_CLOEXEC      1

#define R_OK 4
#define W_OK 2
#define X_OK 1
#define F_OK 0

#define LOCK_SH 1
#define LOCK_EX 2
#define LOCK_NB 4
#define LOCK_UN 8

// AT_* constants for *at() functions
#define AT_FDCWD            -100
#define AT_SYMLINK_NOFOLLOW 0x100
#define AT_REMOVEDIR        0x200
#define AT_SYMLINK_FOLLOW   0x400
#define AT_NO_AUTOMOUNT     0x800
#define AT_EMPTY_PATH       0x1000
#define AT_EACCESS          0x200

// *at() system calls
extern int openat(int dirfd, const char *pathname, int flags, ...);
extern int mkdirat(int dirfd, const char *pathname, mode_t mode);
extern int mknodat(int dirfd, const char *pathname, mode_t mode, dev_t dev);
extern int fchownat(int dirfd, const char *pathname, uid_t owner, gid_t group, int flags);
extern int fchmodat(int dirfd, const char *pathname, mode_t mode, int flags);
extern int unlinkat(int dirfd, const char *pathname, int flags);
extern int renameat(int olddirfd, const char *oldpath, int newdirfd, const char *newpath);
extern int linkat(int olddirfd, const char *oldpath, int newdirfd, const char *newpath, int flags);
extern int symlinkat(const char *target, int newdirfd, const char *linkpath);
extern ssize_t readlinkat(int dirfd, const char *pathname, char *buf, size_t bufsiz);
extern int faccessat(int dirfd, const char *pathname, int mode, int flags);

// =====================================================================
// Timespec (must be before stat)
// =====================================================================
struct timespec {
    time_t tv_sec;
    long tv_nsec;
};

// utimensat needs timespec, so declared after
extern int utimensat(int dirfd, const char *pathname, const struct timespec times[2], int flags);
extern int futimens(int fd, const struct timespec times[2]);

// =====================================================================
// Stat
// =====================================================================
struct stat {
    dev_t st_dev;
    ino_t st_ino;
    mode_t st_mode;
    nlink_t st_nlink;
    uid_t st_uid;
    gid_t st_gid;
    dev_t st_rdev;
    off_t st_size;
    blksize_t st_blksize;
    blkcnt_t st_blocks;
    struct timespec st_atim;
    struct timespec st_mtim;
    struct timespec st_ctim;
};

// Legacy time_t aliases for compatibility
#define st_atime st_atim.tv_sec
#define st_mtime st_mtim.tv_sec
#define st_ctime st_ctim.tv_sec

extern int stat(const char *pathname, struct stat *statbuf);
extern int lstat(const char *pathname, struct stat *statbuf);
extern int fstat(int fd, struct stat *statbuf);

#define S_IFMT   0170000
#define S_IFSOCK 0140000
#define S_IFLNK  0120000
#define S_IFREG  0100000
#define S_IFBLK  0060000
#define S_IFDIR  0040000
#define S_IFCHR  0020000
#define S_IFIFO  0010000
#define S_ISUID  0004000
#define S_ISGID  0002000
#define S_ISVTX  0001000

#define S_ISREG(m)  (((m) & S_IFMT) == S_IFREG)
#define S_ISDIR(m)  (((m) & S_IFMT) == S_IFDIR)
#define S_ISCHR(m)  (((m) & S_IFMT) == S_IFCHR)
#define S_ISBLK(m)  (((m) & S_IFMT) == S_IFBLK)
#define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)
#define S_ISLNK(m)  (((m) & S_IFMT) == S_IFLNK)
#define S_ISSOCK(m) (((m) & S_IFMT) == S_IFSOCK)

#define S_IRWXU 0700
#define S_IRUSR 0400
#define S_IWUSR 0200
#define S_IXUSR 0100
#define S_IRWXG 0070
#define S_IRGRP 0040
#define S_IWGRP 0020
#define S_IXGRP 0010
#define S_IRWXO 0007
#define S_IROTH 0004
#define S_IWOTH 0002
#define S_IXOTH 0001

// =====================================================================
// Directory
// =====================================================================
struct dirent {
    ino_t d_ino;
    off_t d_off;
    unsigned short d_reclen;
    unsigned char d_type;
    char d_name[256];
};

typedef struct _DIR DIR;
extern DIR *opendir(const char *name);
extern struct dirent *readdir(DIR *dirp);
extern int closedir(DIR *dirp);
extern void rewinddir(DIR *dirp);
extern int dirfd(DIR *dirp);

#define DT_UNKNOWN 0
#define DT_FIFO    1
#define DT_CHR     2
#define DT_DIR     4
#define DT_BLK     6
#define DT_REG     8
#define DT_LNK     10
#define DT_SOCK    12

// =====================================================================
// User/group
// =====================================================================
extern uid_t getuid(void);
extern uid_t geteuid(void);
extern gid_t getgid(void);
extern gid_t getegid(void);
extern int setuid(uid_t uid);
extern int setgid(gid_t gid);
extern int seteuid(uid_t uid);
extern int setegid(gid_t gid);
extern int setreuid(uid_t ruid, uid_t euid);
extern int setregid(gid_t rgid, gid_t egid);
extern int setresuid(uid_t ruid, uid_t euid, uid_t suid);
extern int setresgid(gid_t rgid, gid_t egid, gid_t sgid);
extern int getresuid(uid_t *ruid, uid_t *euid, uid_t *suid);
extern int getresgid(gid_t *rgid, gid_t *egid, gid_t *sgid);
extern int getgroups(int size, gid_t list[]);
extern int setgroups(size_t size, const gid_t *list);
extern int initgroups(const char *user, gid_t group);

struct passwd {
    char *pw_name;
    char *pw_passwd;
    uid_t pw_uid;
    gid_t pw_gid;
    char *pw_gecos;
    char *pw_dir;
    char *pw_shell;
};

struct group {
    char *gr_name;
    char *gr_passwd;
    gid_t gr_gid;
    char **gr_mem;
};

extern struct passwd *getpwuid(uid_t uid);
extern struct passwd *getpwnam(const char *name);
extern struct passwd *getpwent(void);
extern void setpwent(void);
extern void endpwent(void);
extern struct group *getgrgid(gid_t gid);
extern struct group *getgrnam(const char *name);
extern void endgrent(void);
extern int getgrouplist(const char *user, gid_t group, gid_t *groups, int *ngroups);

// =====================================================================
// Process groups / sessions
// =====================================================================
extern pid_t getpgrp(void);
extern pid_t getpgid(pid_t pid);
extern int setpgid(pid_t pid, pid_t pgid);
extern pid_t setsid(void);
extern pid_t getsid(pid_t pid);
extern pid_t tcgetpgrp(int fd);
extern int tcsetpgrp(int fd, pid_t pgrp);

// =====================================================================
// Wait macros
// =====================================================================
#define WEXITSTATUS(s) (((s) >> 8) & 0xff)
#define WTERMSIG(s) ((s) & 0x7f)
#define WIFEXITED(s) (WTERMSIG(s) == 0)
#define WIFSIGNALED(s) (WTERMSIG(s) != 0)
#define WIFSTOPPED(s) 0
#define WCOREDUMP(s) 0
#define WSTOPSIG(s) 0
#define WNOHANG 1
#define WUNTRACED 2

// =====================================================================
// Signal
// =====================================================================
#define SIGHUP    1
#define SIGINT    2
#define SIGQUIT   3
#define SIGILL    4
#define SIGTRAP   5
#define SIGABRT   6
#define SIGBUS    7
#define SIGFPE    8
#define SIGKILL   9
#define SIGUSR1   10
#define SIGSEGV   11
#define SIGUSR2   12
#define SIGPIPE   13
#define SIGALRM   14
#define SIGTERM   15
#define SIGSTKFLT 16
#define SIGCHLD   17
#define SIGCONT   18
#define SIGSTOP   19
#define SIGTSTP   20
#define SIGTTIN   21
#define SIGTTOU   22
#define SIGURG    23
#define SIGXCPU   24
#define SIGXFSZ   25
#define SIGVTALRM 26
#define SIGPROF   27
#define SIGWINCH  28
#define SIGIO     29
#define SIGPWR    30
#define SIGSYS    31
#define NSIG      32

#define SIG_DFL ((void(*)(int))0)
#define SIG_IGN ((void(*)(int))1)
#define SIG_ERR ((void(*)(int))-1)

#define SA_RESTART    0x10000000
#define SA_NOCLDSTOP  1
#define SA_SIGINFO    4
#define SA_NOCLDWAIT  2
#define SA_NODEFER    0x40000000
#define SA_RESETHAND  0x80000000
#define SIG_BLOCK     0
#define SIG_UNBLOCK   1
#define SIG_SETMASK   2

struct sigaction {
    void (*sa_handler)(int);
    sigset_t sa_mask;
    int sa_flags;
};

typedef void (*sighandler_t)(int);
extern sighandler_t signal(int signum, sighandler_t handler);
extern int raise(int sig);
extern int kill(pid_t pid, int sig);
extern int killpg(pid_t pgrp, int sig);
extern unsigned int alarm(unsigned int seconds);
extern int pause(void);
extern int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact);
extern int sigprocmask(int how, const sigset_t *set, sigset_t *oldset);
extern int sigpending(sigset_t *set);
extern int sigsuspend(const sigset_t *mask);
extern int sigemptyset(sigset_t *set);
extern int sigfillset(sigset_t *set);
extern int sigaddset(sigset_t *set, int signum);
extern int sigdelset(sigset_t *set, int signum);
extern int sigismember(const sigset_t *set, int signum);
extern char *strsignal(int sig);

// =====================================================================
// Time
// =====================================================================
struct timeval {
    time_t tv_sec;
    suseconds_t tv_usec;
};

struct timezone {
    int tz_minuteswest;
    int tz_dsttime;
};

extern time_t time(time_t *tloc);
extern int gettimeofday(struct timeval *tv, struct timezone *tz);
extern int settimeofday(const struct timeval *tv, const struct timezone *tz);
extern int clock_gettime(clockid_t clk_id, struct timespec *tp);
extern int clock_settime(clockid_t clk_id, const struct timespec *tp);
extern int nanosleep(const struct timespec *req, struct timespec *rem);
extern unsigned int sleep(unsigned int seconds);
extern int usleep(unsigned int usec);

#define CLOCK_REALTIME  0
#define CLOCK_MONOTONIC 1

struct tm {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
    long tm_gmtoff;
    const char *tm_zone;
};

extern struct tm *localtime(const time_t *timep);
extern struct tm *localtime_r(const time_t *timep, struct tm *result);
extern struct tm *gmtime(const time_t *timep);
extern struct tm *gmtime_r(const time_t *timep, struct tm *result);
extern time_t mktime(struct tm *tm);
extern char *asctime(const struct tm *tm);
extern char *asctime_r(const struct tm *tm, char *buf);
extern char *ctime(const time_t *timep);
extern char *ctime_r(const time_t *timep, char *buf);
extern size_t strftime(char *s, size_t max, const char *format, const struct tm *tm);
extern char *strptime(const char *s, const char *format, struct tm *tm);
extern void tzset(void);

extern char *tzname[2];
extern long timezone;
extern int daylight;

// =====================================================================
// Terminal
// =====================================================================
#define TIOCGWINSZ 0x5413
#define TIOCSWINSZ 0x5414
#define TIOCSCTTY  0x540E
#define TIOCNOTTY  0x5422
#define TIOCGPGRP  0x540F
#define TIOCSPGRP  0x5410

struct winsize {
    unsigned short ws_row;
    unsigned short ws_col;
    unsigned short ws_xpixel;
    unsigned short ws_ypixel;
};

#define NCCS 32
typedef unsigned int tcflag_t;
typedef unsigned char cc_t;
typedef unsigned int speed_t;

struct termios {
    tcflag_t c_iflag;
    tcflag_t c_oflag;
    tcflag_t c_cflag;
    tcflag_t c_lflag;
    cc_t c_cc[NCCS];
    speed_t c_ispeed;
    speed_t c_ospeed;
};

#define ECHO    0000010
#define ECHOE   0000020
#define ECHOK   0000040
#define ECHONL  0000100
#define ICANON  0000002
#define ISIG    0000001
#define TCSANOW   0
#define TCSADRAIN 1
#define TCSAFLUSH 2

#define TCIFLUSH  0
#define TCOFLUSH  1
#define TCIOFLUSH 2
#define TCIOFF    2
#define TCION     3
#define TCOOFF    0
#define TCOON     1

#define VEOF   4
#define VEOL   11
#define VERASE 2
#define VINTR  0
#define VKILL  3
#define VMIN   6
#define VQUIT  1
#define VSTART 8
#define VSTOP  9
#define VSUSP  10
#define VTIME  5

#define BRKINT 0000002
#define ICRNL  0000400
#define IGNBRK 0000001
#define IGNCR  0000200
#define INLCR  0000100
#define ISTRIP 0000040
#define IXON   0002000
#define IXOFF  0010000
#define IXANY  0004000
#define OPOST  0000001
#define ONLCR  0000004
#define CS5    0000000
#define CS6    0000020
#define CS7    0000040
#define CS8    0000060
#define CSIZE  0000060
#define CSTOPB 0000100
#define CREAD  0000200
#define PARENB 0000400
#define PARODD 0001000
#define HUPCL  0002000
#define CLOCAL 0004000
#define CRTSCTS 020000000000

#define B0      0
#define B50     1
#define B75     2
#define B110    3
#define B134    4
#define B150    5
#define B200    6
#define B300    7
#define B600    8
#define B1200   9
#define B1800   10
#define B2400   11
#define B4800   12
#define B9600   13
#define B19200  14
#define B38400  15
#define B57600  4097
#define B115200 4098
#define B230400 4099
#define B460800 4100
#define B500000 4101
#define B576000 4102
#define B921600 4103
#define B1000000 4104
#define B1152000 4105
#define B1500000 4106
#define B2000000 4107
#define B2500000 4108
#define B3000000 4109
#define B3500000 4110
#define B4000000 4111

extern int tcgetattr(int fd, struct termios *termios_p);
extern int tcsetattr(int fd, int optional_actions, const struct termios *termios_p);
extern speed_t cfgetispeed(const struct termios *termios_p);
extern speed_t cfgetospeed(const struct termios *termios_p);
extern int cfsetispeed(struct termios *termios_p, speed_t speed);
extern int cfsetospeed(struct termios *termios_p, speed_t speed);
extern int tcdrain(int fd);
extern int tcflush(int fd, int queue_selector);
extern int tcsendbreak(int fd, int duration);
extern int tcflow(int fd, int action);
extern char *ttyname(int fd);
extern int ttyname_r(int fd, char *buf, size_t buflen);

// =====================================================================
// Resource limits
// =====================================================================
typedef unsigned long rlim_t;
#define RLIM_INFINITY (~0UL)

#define RLIMIT_FSIZE    1
#define RLIMIT_DATA     2
#define RLIMIT_STACK    3
#define RLIMIT_CORE     4
#define RLIMIT_RSS      5
#define RLIMIT_NPROC    6
#define RLIMIT_NOFILE   7
#define RLIMIT_MEMLOCK  8
#define RLIMIT_AS       9
#define RLIMIT_LOCKS    10

struct rlimit {
    rlim_t rlim_cur;
    rlim_t rlim_max;
};

extern int getrlimit(int resource, struct rlimit *rlim);
extern int setrlimit(int resource, const struct rlimit *rlim);

#define RUSAGE_SELF     0
#define RUSAGE_CHILDREN (-1)

struct rusage {
    struct timeval ru_utime;
    struct timeval ru_stime;
    long ru_maxrss, ru_ixrss, ru_idrss, ru_isrss;
    long ru_minflt, ru_majflt, ru_nswap;
    long ru_inblock, ru_oublock;
    long ru_msgsnd, ru_msgrcv;
    long ru_nsignals, ru_nvcsw, ru_nivcsw;
};

extern int getrusage(int who, struct rusage *usage);

// =====================================================================
// System info
// =====================================================================
struct utsname {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
};

extern int uname(struct utsname *buf);
extern int gethostname(char *name, size_t len);
extern int sethostname(const char *name, size_t len);
extern long sysconf(int name);
extern int nice(int inc);
extern int chroot(const char *path);

struct sysinfo {
    long uptime;
    unsigned long loads[3];
    unsigned long totalram;
    unsigned long freeram;
    unsigned long sharedram;
    unsigned long bufferram;
    unsigned long totalswap;
    unsigned long freeswap;
    unsigned short procs;
    unsigned short pad;
    unsigned long totalhigh;
    unsigned long freehigh;
    unsigned int mem_unit;
    char _f[20 - 2*sizeof(long) - sizeof(int)];
};

extern int sysinfo(struct sysinfo *info);

#define _SC_CLK_TCK         2
#define _SC_PAGESIZE       30
#define _SC_NPROCESSORS_ONLN 84

// =====================================================================
// Priority
// =====================================================================
#define PRIO_PROCESS 0
#define PRIO_PGRP    1
#define PRIO_USER    2

extern int getpriority(int which, int who);
extern int setpriority(int which, int who, int prio);

// =====================================================================
// Poll/Select
// =====================================================================
#define POLLIN     0x001
#define POLLPRI    0x002
#define POLLOUT    0x004
#define POLLERR    0x008
#define POLLHUP    0x010
#define POLLNVAL   0x020
#define POLLRDNORM 0x040
#define POLLWRNORM 0x100

typedef unsigned long nfds_t;

struct pollfd {
    int fd;
    short events;
    short revents;
};

extern int poll(struct pollfd *fds, nfds_t nfds, int timeout);

#define FD_SETSIZE 1024

typedef struct {
    unsigned long fds_bits[FD_SETSIZE / (8 * sizeof(long))];
} fd_set;

#define FD_ZERO(s) __builtin_memset((s), 0, sizeof(fd_set))
#define FD_SET(d, s) ((s)->fds_bits[(d) / (8 * sizeof(long))] |= (1UL << ((d) % (8 * sizeof(long)))))
#define FD_CLR(d, s) ((s)->fds_bits[(d) / (8 * sizeof(long))] &= ~(1UL << ((d) % (8 * sizeof(long)))))
#define FD_ISSET(d, s) (((s)->fds_bits[(d) / (8 * sizeof(long))] & (1UL << ((d) % (8 * sizeof(long))))) != 0)

extern int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);
extern int pselect(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, const struct timespec *timeout, const sigset_t *sigmask);

// =====================================================================
// mmap
// =====================================================================
#define PROT_NONE  0
#define PROT_READ  1
#define PROT_WRITE 2
#define PROT_EXEC  4
#define MAP_SHARED  1
#define MAP_PRIVATE 2
#define MAP_ANONYMOUS 0x20
#define MAP_ANON MAP_ANONYMOUS
#define MAP_FAILED ((void*)-1)
#define MS_SYNC 4
#define MADV_DONTNEED 4

extern void *mmap(void *addr, size_t len, int prot, int flags, int fd, off_t offset);
extern int munmap(void *addr, size_t len);
extern int mprotect(void *addr, size_t len, int prot);
extern int msync(void *addr, size_t len, int flags);
extern int madvise(void *addr, size_t len, int advice);

// =====================================================================
// Network (minimal stubs)
// =====================================================================
typedef unsigned short sa_family_t;

struct sockaddr {
    sa_family_t sa_family;
    char sa_data[14];
};

struct in_addr {
    unsigned int s_addr;
};

struct sockaddr_in {
    unsigned short sin_family;
    unsigned short sin_port;
    struct in_addr sin_addr;
    char sin_zero[8];
};

struct sockaddr_un {
    sa_family_t sun_family;
    char sun_path[108];
};

struct sockaddr_in6 {
    unsigned short sin6_family;
    unsigned short sin6_port;
    unsigned int sin6_flowinfo;
    unsigned char sin6_addr[16];
    unsigned int sin6_scope_id;
};

#define AF_UNIX   1
#define AF_INET   2
#define AF_INET6  10
#define AF_UNSPEC 0
#define SOCK_STREAM    1
#define SOCK_DGRAM     2
#define SOCK_RAW       3
#define SOCK_RDM       4
#define SOCK_SEQPACKET 5
#define SOCK_CLOEXEC   02000000
#define SOCK_NONBLOCK  04000
#define INADDR_ANY     0

// Socket options
#define SOL_SOCKET     1
#define IPPROTO_TCP    6
#define IPPROTO_UDP    17

#define SO_DEBUG       1
#define SO_REUSEADDR   2
#define SO_TYPE        3
#define SO_ERROR       4
#define SO_DONTROUTE   5
#define SO_BROADCAST   6
#define SO_SNDBUF      7
#define SO_RCVBUF      8
#define SO_KEEPALIVE   9
#define SO_OOBINLINE   10
#define SO_LINGER      13
#define SO_RCVLOWAT    18
#define SO_SNDLOWAT    19
#define SO_RCVTIMEO    20
#define SO_SNDTIMEO    21
#define SO_ACCEPTCONN  30
#define SO_PASSCRED    16
#define SO_PEERCRED    17
#define SO_BINDTODEVICE 25

#define TCP_NODELAY    1
#define TCP_MAXSEG     2
#define TCP_CORK       3
#define TCP_KEEPIDLE   4
#define TCP_KEEPINTVL  5
#define TCP_KEEPCNT    6

// Shutdown flags
#define SHUT_RD   0
#define SHUT_WR   1
#define SHUT_RDWR 2

// Message flags
#define MSG_OOB       0x01
#define MSG_PEEK      0x02
#define MSG_DONTROUTE 0x04
#define MSG_DONTWAIT  0x40
#define MSG_NOSIGNAL  0x4000

// Network interface
#define IFNAMSIZ 16
#define IF_NAMESIZE 16

struct ifreq {
    char ifr_name[IFNAMSIZ];
    union {
        struct sockaddr ifr_addr;
        struct sockaddr ifr_dstaddr;
        struct sockaddr ifr_broadaddr;
        struct sockaddr ifr_netmask;
        struct sockaddr ifr_hwaddr;
        short ifr_flags;
        int ifr_ifindex;
        int ifr_metric;
        int ifr_mtu;
        char ifr_slave[IFNAMSIZ];
        char ifr_newname[IFNAMSIZ];
        void *ifr_data;
    } ifr_ifru;
};

#define ifr_addr      ifr_ifru.ifr_addr
#define ifr_dstaddr   ifr_ifru.ifr_dstaddr
#define ifr_broadaddr ifr_ifru.ifr_broadaddr
#define ifr_netmask   ifr_ifru.ifr_netmask
#define ifr_hwaddr    ifr_ifru.ifr_hwaddr
#define ifr_flags     ifr_ifru.ifr_flags
#define ifr_ifindex   ifr_ifru.ifr_ifindex
#define ifr_metric    ifr_ifru.ifr_metric
#define ifr_mtu       ifr_ifru.ifr_mtu

// Ioctl commands for network interfaces
#define SIOCGIFADDR    0x8915
#define SIOCSIFADDR    0x8916
#define SIOCGIFFLAGS   0x8913
#define SIOCSIFFLAGS   0x8914
#define SIOCGIFBRDADDR 0x8919
#define SIOCSIFBRDADDR 0x891a
#define SIOCGIFNETMASK 0x891b
#define SIOCSIFNETMASK 0x891c
#define SIOCGIFMTU     0x8921
#define SIOCSIFMTU     0x8922
#define SIOCGIFHWADDR  0x8927
#define SIOCSIFHWADDR  0x8924
#define SIOCGIFINDEX   0x8933

extern unsigned int if_nametoindex(const char *ifname);
extern char *if_indextoname(unsigned int ifindex, char *ifname);

extern int socket(int domain, int type, int protocol);
extern int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
extern int listen(int sockfd, int backlog);
extern int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
extern int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
extern ssize_t send(int sockfd, const void *buf, size_t len, int flags);
extern ssize_t recv(int sockfd, void *buf, size_t len, int flags);
extern int shutdown(int sockfd, int how);
extern int getsockname(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
extern int getpeername(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
extern int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen);
extern int getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen);

extern unsigned int htonl(unsigned int hostlong);
extern unsigned short htons(unsigned short hostshort);
extern unsigned int ntohl(unsigned int netlong);
extern unsigned short ntohs(unsigned short netshort);
extern int inet_aton(const char *cp, struct in_addr *inp);
extern char *inet_ntoa(struct in_addr in);

struct hostent {
    char *h_name;
    char **h_aliases;
    int h_addrtype;
    int h_length;
    char **h_addr_list;
};

extern struct hostent *gethostbyname(const char *name);
extern struct hostent *gethostbyname2(const char *name, int af);
extern struct hostent *gethostbyaddr(const void *addr, socklen_t len, int type);

// DNS resolver error handling
extern int h_errno;
extern const char *hstrerror(int err);

#define HOST_NOT_FOUND 1
#define TRY_AGAIN      2
#define NO_RECOVERY    3
#define NO_DATA        4
#define NO_ADDRESS     NO_DATA

struct servent {
    char *s_name;
    char **s_aliases;
    int s_port;
    char *s_proto;
};

extern struct servent *getservbyname(const char *name, const char *proto);
extern struct servent *getservbyport(int port, const char *proto);

struct protoent {
    char *p_name;
    char **p_aliases;
    int p_proto;
};

extern struct protoent *getprotobyname(const char *name);
extern struct protoent *getprotobynumber(int proto);

struct addrinfo {
    int ai_flags;
    int ai_family;
    int ai_socktype;
    int ai_protocol;
    socklen_t ai_addrlen;
    struct sockaddr *ai_addr;
    char *ai_canonname;
    struct addrinfo *ai_next;
};

#define AI_PASSIVE     0x01
#define AI_CANONNAME   0x02
#define AI_NUMERICHOST 0x04
#define AI_V4MAPPED    0x08
#define AI_ALL         0x10
#define AI_ADDRCONFIG  0x20
#define AI_NUMERICSERV 0x400

#define NI_NUMERICHOST 0x01
#define NI_NUMERICSERV 0x02
#define NI_NOFQDN      0x04
#define NI_NAMEREQD    0x08
#define NI_DGRAM       0x10
#define NI_MAXHOST     1025
#define NI_MAXSERV     32

#define EAI_AGAIN    -3
#define EAI_BADFLAGS -1
#define EAI_FAIL     -4
#define EAI_FAMILY   -6
#define EAI_MEMORY   -10
#define EAI_NODATA   -5
#define EAI_NONAME   -2
#define EAI_SERVICE  -8
#define EAI_SOCKTYPE -7
#define EAI_SYSTEM   -11

extern int getaddrinfo(const char *node, const char *service,
                       const struct addrinfo *hints, struct addrinfo **res);
extern void freeaddrinfo(struct addrinfo *res);
extern const char *gai_strerror(int errcode);
extern int getnameinfo(const struct sockaddr *sa, socklen_t salen,
                       char *host, socklen_t hostlen,
                       char *serv, socklen_t servlen, int flags);

// =====================================================================
// Syslog (stubs)
// =====================================================================
#define LOG_EMERG   0
#define LOG_ALERT   1
#define LOG_CRIT    2
#define LOG_ERR     3
#define LOG_WARNING 4
#define LOG_NOTICE  5
#define LOG_INFO    6
#define LOG_DEBUG   7
#define LOG_PID     0x01
#define LOG_DAEMON  (3<<3)
#define LOG_USER    (1<<3)

extern void openlog(const char *ident, int option, int facility);
extern void closelog(void);
extern void syslog(int priority, const char *format, ...);

// =====================================================================
// Mntent (stubs)
// =====================================================================
struct mntent {
    char *mnt_fsname;
    char *mnt_dir;
    char *mnt_type;
    char *mnt_opts;
    int mnt_freq;
    int mnt_passno;
};

extern FILE *setmntent(const char *filename, const char *type);
extern struct mntent *getmntent(FILE *stream);
extern int endmntent(FILE *streamp);

// =====================================================================
// Paths
// =====================================================================
#define _PATH_TTY      "/dev/tty"
#define _PATH_DEVNULL  "/dev/null"
#define _PATH_CONSOLE  "/dev/console"
#define _PATH_VARRUN   "/var/run/"
#define _PATH_DEFPATH  "/usr/bin:/bin"

// =====================================================================
// Byteswap
// =====================================================================
#define bswap_16(x) __builtin_bswap16(x)
#define bswap_32(x) __builtin_bswap32(x)
#define bswap_64(x) __builtin_bswap64(x)

// =====================================================================
// Misc
// =====================================================================
extern void abort(void);
extern int abs(int j);
extern long labs(long j);
extern long long llabs(long long j);

#define RAND_MAX 2147483647
extern int rand(void);
extern int rand_r(unsigned int *seedp);
extern void srand(unsigned int seed);
extern long random(void);
extern void srandom(unsigned int seed);
extern char *initstate(unsigned int seed, char *state, size_t n);
extern char *setstate(char *state);

extern char *getenv(const char *name);
extern int setenv(const char *name, const char *value, int overwrite);
extern int unsetenv(const char *name);
extern int putenv(char *string);
extern int clearenv(void);

extern int system(const char *command);

extern void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *));
extern void *bsearch(const void *key, const void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *));

typedef int jmp_buf[16];
extern int setjmp(jmp_buf env);
extern void longjmp(jmp_buf env, int val);
extern int _setjmp(jmp_buf env);
extern void _longjmp(jmp_buf env, int val);
extern int sigsetjmp(jmp_buf env, int savemask);
extern void siglongjmp(jmp_buf env, int val);

extern char *mktemp(char *tmpl);
extern char *mkdtemp(char *tmpl);
extern int mkstemp(char *tmpl);

extern unsigned int major(dev_t dev);
extern unsigned int minor(dev_t dev);
#define makedev(maj, min) (((maj) << 8) | (min))

// Scheduler
typedef struct { unsigned long __bits[128 / sizeof(long)]; } cpu_set_t;
#define CPU_SETSIZE 1024
#define CPU_ZERO(s) __builtin_memset((s), 0, sizeof(cpu_set_t))
#define CPU_SET(c, s) ((s)->__bits[(c) / (8 * sizeof(long))] |= (1UL << ((c) % (8 * sizeof(long)))))
#define CPU_ISSET(c, s) (((s)->__bits[(c) / (8 * sizeof(long))] & (1UL << ((c) % (8 * sizeof(long))))) != 0)
#define CPU_COUNT(s) __builtin_popcountl((s)->__bits[0])

extern int sched_getaffinity(pid_t pid, size_t cpusetsize, cpu_set_t *mask);
extern int sched_setaffinity(pid_t pid, size_t cpusetsize, const cpu_set_t *mask);

// popen/pclose
extern FILE *popen(const char *command, const char *type);
extern int pclose(FILE *stream);

// Varargs
#define va_list __builtin_va_list
#define va_start __builtin_va_start
#define va_end __builtin_va_end
#define va_arg __builtin_va_arg
#define va_copy __builtin_va_copy

// Assert
#define assert(x) ((void)0)

// Offset
#define offsetof(type, member) __builtin_offsetof(type, member)

// Limits
#define INT_MAX  2147483647
#define INT_MIN  (-2147483647-1)
#define UINT_MAX 4294967295U
#define LONG_MAX 2147483647L
#define LONG_MIN (-2147483647L-1)
#define ULONG_MAX 4294967295UL
#define LLONG_MAX 9223372036854775807LL
#define LLONG_MIN (-9223372036854775807LL-1)
#define ULLONG_MAX 18446744073709551615ULL
#define SSIZE_MAX INT_MAX
#define PATH_MAX 4096
#define NAME_MAX 255
#define LINE_MAX 2048

// Boolean
#define bool _Bool
#define true 1
#define false 0
