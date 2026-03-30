// wasm-compat.h - POSIX compatibility for compiling busybox on wasm32-wasip1
// Process functions are REAL implementations (in yos-stubs.c) linked to YOS runtime.
// Other stubs are minimal but NOT dead-code-eliminatable.

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>

// ---- pid_t ----
#ifndef __pid_t_defined
typedef int pid_t;
#define __pid_t_defined
#endif

// ---- uid/gid ----
#ifndef __uid_t_defined
typedef unsigned int uid_t;
typedef unsigned int gid_t;
#define __uid_t_defined
#endif

// ---- socklen_t ----
#ifndef __socklen_t_defined
typedef unsigned int socklen_t;
#define __socklen_t_defined
#endif

// =====================================================================
// REAL IMPLEMENTATIONS (extern, in yos-stubs.c, linked to YOS runtime)
// =====================================================================

// Process management
extern pid_t fork(void);
extern pid_t vfork(void);
extern pid_t getpid(void);
extern pid_t getppid(void);
extern int execvp(const char *file, char *const argv[]);
extern int execv(const char *path, char *const argv[]);
extern int execve(const char *path, char *const argv[], char *const envp[]);
extern pid_t waitpid(pid_t pid, int *status, int options);
extern pid_t wait(int *status);

// I/O
extern int pipe(int fd[2]);
extern int dup(int fd);
extern int dup2(int oldfd, int newfd);
extern int fcntl(int fd, int cmd, ...);
extern int ioctl(int fd, unsigned long req, ...);
extern int isatty(int fd);
extern int chdir(const char *path);
extern char *getcwd(char *buf, size_t size);

// User/group
extern uid_t getuid(void);
extern uid_t geteuid(void);
extern gid_t getgid(void);
extern gid_t getegid(void);
extern int setuid(uid_t uid);
extern int setgid(gid_t gid);
extern int getgroups(int size, gid_t list[]);

struct passwd { char *pw_name, *pw_passwd; uid_t pw_uid; gid_t pw_gid; char *pw_gecos, *pw_dir, *pw_shell; };
struct group { char *gr_name, *gr_passwd; gid_t gr_gid; char **gr_mem; };
extern struct passwd *getpwuid(uid_t uid);
extern struct passwd *getpwnam(const char *name);
extern struct passwd *getpwent(void);
extern void setpwent(void);
extern void endpwent(void);
extern struct group *getgrgid(gid_t gid);
extern struct group *getgrnam(const char *name);
extern void endgrent(void);

// Misc process
extern pid_t getpgrp(void);
extern pid_t getpgid(pid_t pid);
extern int setpgid(pid_t pid, pid_t pgid);
extern pid_t setsid(void);
extern pid_t tcgetpgrp(int fd);
extern int tcsetpgrp(int fd, pid_t pgrp);

// Resource limits
typedef unsigned long rlim_t;
#define RLIM_INFINITY (~0UL)
#ifndef RLIMIT_FSIZE
#define RLIMIT_FSIZE 1
#define RLIMIT_DATA 2
#define RLIMIT_STACK 3
#define RLIMIT_CORE 4
#define RLIMIT_RSS 5
#define RLIMIT_NPROC 6
#define RLIMIT_NOFILE 7
#define RLIMIT_MEMLOCK 8
#define RLIMIT_AS 9
#define RLIMIT_LOCKS 10
#define RLIMIT_SIGPENDING 11
#define RLIMIT_MSGQUEUE 12
#define RLIMIT_NICE 13
#define RLIMIT_RTPRIO 14
struct rlimit { rlim_t rlim_cur, rlim_max; };
extern int getrlimit(int resource, struct rlimit *rlim);
extern int setrlimit(int resource, const struct rlimit *rlim);
#endif

// File operations
extern int chown(const char *path, uid_t owner, gid_t group);
extern int fchown(int fd, uid_t owner, gid_t group);
extern int lchown(const char *path, uid_t owner, gid_t group);
extern int link(const char *old, const char *new_path);
extern int symlink(const char *target, const char *linkpath);
extern long readlink(const char *path, char *buf, size_t bufsiz);
extern int umask(int mask);
extern int access(const char *path, int mode);
extern int mknod(const char *path, int mode, int dev);
extern int mkfifo(const char *path, int mode);
extern unsigned int sleep(unsigned int seconds);
extern int usleep(unsigned int usec);
extern char *ttyname(int fd);
extern int utime(const char *filename, const void *times);
extern int seteuid(uid_t uid);
extern int setegid(gid_t gid);
extern int setreuid(uid_t ruid, uid_t euid);
extern int setregid(gid_t rgid, gid_t egid);
extern int setresuid(uid_t ruid, uid_t euid, uid_t suid);
extern int setresgid(gid_t rgid, gid_t egid, gid_t sgid);
extern int getresuid(uid_t *ruid, uid_t *euid, uid_t *suid);
extern int getresgid(gid_t *rgid, gid_t *egid, gid_t *sgid);
extern int initgroups(const char *user, gid_t group);
extern int setgroups(size_t size, const gid_t *list);
extern int gethostname(char *name, size_t len);
extern int chroot(const char *path);
extern int nice(int inc);
extern int fchdir(int fd);
extern long sysconf(int name);
extern int flock(int fd, int operation);
extern int pipe2(int fd[2], int flags);
extern int dup3(int oldfd, int newfd, int flags);

// =====================================================================
// SIGNAL (wasi-libc provides signal/raise/sigset_t with _WASI_EMULATED_SIGNAL)
// We add what wasi-libc gates behind __wasilibc_unmodified_upstream
// =====================================================================

#ifndef SA_RESTART
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

extern int sigaction(int sig, const struct sigaction *act, struct sigaction *oact);
extern int sigprocmask(int how, const sigset_t *set, sigset_t *oset);
extern int sigpending(sigset_t *set);
extern int sigsuspend(const sigset_t *set);
extern int kill(pid_t pid, int sig);
extern int killpg(pid_t pgrp, int sig);
extern unsigned int alarm(unsigned int seconds);
extern int pause(void);
#endif

// =====================================================================
// WAIT
// =====================================================================
#ifndef WEXITSTATUS
#define WEXITSTATUS(s) (((s) >> 8) & 0xff)
#define WTERMSIG(s) ((s) & 0x7f)
#define WIFEXITED(s) (WTERMSIG(s) == 0)
#define WIFSIGNALED(s) (WTERMSIG(s) != 0)
#define WIFSTOPPED(s) 0
#define WCOREDUMP(s) 0
#define WSTOPSIG(s) 0
#define WNOHANG 1
#define WUNTRACED 2
#endif

// =====================================================================
// TERMINAL
// =====================================================================
// ioctl constants
#ifndef TIOCGWINSZ
#define TIOCGWINSZ 0x5413
#define TIOCSWINSZ 0x5414
#define TIOCSCTTY  0x540E
#define TIOCNOTTY  0x5422
#define TIOCGPGRP  0x540F
#define TIOCSPGRP  0x5410
struct winsize { unsigned short ws_row, ws_col, ws_xpixel, ws_ypixel; };
#endif

#ifndef _TERMIOS_H
#define _TERMIOS_H
#define NCCS 32
typedef unsigned int tcflag_t;
typedef unsigned char cc_t;
typedef unsigned int speed_t;
struct termios { tcflag_t c_iflag, c_oflag, c_cflag, c_lflag; cc_t c_cc[NCCS]; speed_t c_ispeed, c_ospeed; };
#define ECHO    0000010
#define ECHOE   0000020
#define ECHOK   0000040
#define ECHONL  0000100
#define ICANON  0000002
#define ISIG    0000001
#define TCSANOW   0
#define TCSADRAIN 1
#define TCSAFLUSH 2
#define VEOF 4
#define VEOL 11
#define VERASE 2
#define VINTR 0
#define VKILL 3
#define VMIN 6
#define VQUIT 1
#define VSTART 8
#define VSTOP 9
#define VSUSP 10
#define VTIME 5
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
#define TCIFLUSH 0
#define TCOFLUSH 1
#define TCIOFLUSH 2
#define TCION 0
#define TCIOFF 1
#define B0 0
#define B50 1
#define B75 2
#define B110 3
#define B134 4
#define B150 5
#define B200 6
#define B300 7
#define B600 8
#define B1200 9
#define B1800 10
#define B2400 11
#define B4800 12
#define B9600 13
#define B19200 14
#define B38400 15
#define B57600 4097
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
#define CBAUD 0010017
extern int tcgetattr(int fd, struct termios *t);
extern int tcsetattr(int fd, int act, const struct termios *t);
extern speed_t cfgetispeed(const struct termios *t);
extern speed_t cfgetospeed(const struct termios *t);
extern int cfsetispeed(struct termios *t, speed_t speed);
extern int cfsetospeed(struct termios *t, speed_t speed);
extern int tcdrain(int fd);
extern int tcflush(int fd, int queue);
extern int tcsendbreak(int fd, int dur);
extern int tcflow(int fd, int action);
#endif

// =====================================================================
// FCNTL constants
// =====================================================================
#ifndef F_DUPFD
#define F_DUPFD 0
#define F_GETFD 1
#define F_SETFD 2
#define F_GETFL 3
#define F_SETFL 4
#define F_GETLK 5
#define F_SETLK 6
#define F_SETLKW 7
#define F_DUPFD_CLOEXEC 1030
#define FD_CLOEXEC 1
#define O_NONBLOCK 04000
#define O_CLOEXEC 02000000
#define O_NOCTTY 0400
#endif

// =====================================================================
// MMAP
// =====================================================================
#ifndef MAP_FAILED
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
extern void *mmap(void *addr, size_t len, int prot, int flags, int fd, long offset);
extern int munmap(void *addr, size_t len);
extern int mprotect(void *addr, size_t len, int prot);
extern int msync(void *addr, size_t len, int flags);
extern int madvise(void *addr, size_t len, int advice);
#endif

// =====================================================================
// UTSNAME
// =====================================================================
#ifndef _SYS_UTSNAME_H
#define _SYS_UTSNAME_H
struct utsname { char sysname[65]; char nodename[65]; char release[65]; char version[65]; char machine[65]; };
extern int uname(struct utsname *buf);
#endif

// =====================================================================
// NETWORK (stubs but extern so not dead-code-eliminated)
// =====================================================================
#ifndef _NETDB_H
#define _NETDB_H
#define h_errno errno
struct hostent { char *h_name; char **h_aliases; int h_addrtype; int h_length; char **h_addr_list; };
struct servent { char *s_name; char **s_aliases; int s_port; char *s_proto; };
struct protoent { char *p_name; char **p_aliases; int p_proto; };
struct addrinfo { int ai_flags; int ai_family; int ai_socktype; int ai_protocol;
    unsigned int ai_addrlen; struct sockaddr *ai_addr; char *ai_canonname; struct addrinfo *ai_next; };
extern const char *hstrerror(int err);
extern struct hostent *gethostbyname(const char *name);
extern struct hostent *gethostbyaddr(const void *addr, unsigned int len, int type);
extern struct servent *getservbyname(const char *name, const char *proto);
extern struct servent *getservbyport(int port, const char *proto);
extern int getaddrinfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **res);
extern void freeaddrinfo(struct addrinfo *res);
extern const char *gai_strerror(int errcode);
extern int getnameinfo(const struct sockaddr *sa, socklen_t salen, char *host, socklen_t hostlen, char *serv, socklen_t servlen, int flags);
#define AI_PASSIVE 1
#define AI_CANONNAME 2
#define AI_NUMERICHOST 4
#define AI_NUMERICSERV 0x400
#define NI_NUMERICHOST 1
#define NI_NUMERICSERV 2
#define NI_MAXHOST 1025
#define NI_MAXSERV 32
#define NI_NAMEREQD 8
#define NI_DGRAM 16
#endif

#ifndef _SYS_SOCKET_H
#define _SYS_SOCKET_H
#define AF_UNIX 1
#define AF_INET 2
#define AF_INET6 10
#define AF_LOCAL 1
#define AF_UNSPEC 0
#define PF_UNIX AF_UNIX
#define PF_INET AF_INET
#define PF_INET6 AF_INET6
#define SOCK_STREAM 1
#define SOCK_DGRAM 2
#define SOCK_RAW 3
#define SOCK_RDM 4
#define SOCK_SEQPACKET 5
#define SOCK_CLOEXEC 02000000
#define SOL_SOCKET 1
#define SO_REUSEADDR 2
#define SO_BROADCAST 6
#define SO_KEEPALIVE 9
#define SO_RCVBUF 8
#define SO_SNDBUF 7
#define SO_LINGER 13
#define SO_ERROR 4
#define SO_TYPE 3
#define MSG_DONTWAIT 0x40
#define MSG_NOSIGNAL 0x4000
#define IPPROTO_TCP 6
#define IPPROTO_UDP 17
#define TCP_NODELAY 1
typedef unsigned short sa_family_t;
struct sockaddr { sa_family_t sa_family; char sa_data[14]; };
struct sockaddr_un { sa_family_t sun_family; char sun_path[108]; };
struct sockaddr_storage { unsigned short ss_family; char __pad[126]; };
extern int socket(int domain, int type, int protocol);
extern int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
extern int listen(int sockfd, int backlog);
extern int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
extern int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
extern long send(int sockfd, const void *buf, size_t len, int flags);
extern long recv(int sockfd, void *buf, size_t len, int flags);
extern long sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen);
extern long recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen);
extern int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen);
extern int getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen);
extern int getsockname(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
extern int getpeername(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
extern int shutdown(int sockfd, int how);
#endif

#ifndef _NETINET_IN_H
#define _NETINET_IN_H
struct in_addr { unsigned int s_addr; };
struct sockaddr_in { unsigned short sin_family; unsigned short sin_port; struct in_addr sin_addr; char sin_zero[8]; };
struct in6_addr { unsigned char s6_addr[16]; };
struct sockaddr_in6 { unsigned short sin6_family; unsigned short sin6_port; unsigned int sin6_flowinfo;
    struct in6_addr sin6_addr; unsigned int sin6_scope_id; };
#define INADDR_ANY 0
#define INADDR_LOOPBACK 0x7f000001
#define INADDR_NONE 0xffffffff
extern unsigned int htonl(unsigned int hostlong);
extern unsigned short htons(unsigned short hostshort);
extern unsigned int ntohl(unsigned int netlong);
extern unsigned short ntohs(unsigned short netshort);
extern int inet_aton(const char *cp, struct in_addr *inp);
extern char *inet_ntoa(struct in_addr in);
extern const char *inet_ntop(int af, const void *src, char *dst, socklen_t size);
extern int inet_pton(int af, const char *src, void *dst);
#endif

// =====================================================================
// POLL
// =====================================================================
#ifndef _POLL_H
#define _POLL_H
#define POLLIN 1
#define POLLOUT 4
#define POLLERR 8
#define POLLHUP 16
#define POLLNVAL 32
typedef unsigned long nfds_t;
struct pollfd { int fd; short events; short revents; };
extern int poll(struct pollfd *fds, nfds_t nfds, int timeout);
#endif

// =====================================================================
// SYSLOG
// =====================================================================
#ifndef _SYSLOG_H
#define _SYSLOG_H
#define LOG_EMERG 0
#define LOG_ALERT 1
#define LOG_CRIT 2
#define LOG_ERR 3
#define LOG_WARNING 4
#define LOG_NOTICE 5
#define LOG_INFO 6
#define LOG_DEBUG 7
#define LOG_PID 0x01
#define LOG_DAEMON (3<<3)
#define LOG_USER (1<<3)
extern void openlog(const char *ident, int option, int facility);
extern void closelog(void);
extern void syslog(int priority, const char *format, ...);
#endif

// =====================================================================
// MNTENT
// =====================================================================
#ifndef _MNTENT_H
#define _MNTENT_H
struct mntent { char *mnt_fsname, *mnt_dir, *mnt_type, *mnt_opts; int mnt_freq, mnt_passno; };
extern void *setmntent(const char *filename, const char *type);
extern struct mntent *getmntent(void *stream);
extern int endmntent(void *streamp);
#endif

// =====================================================================
// PATHS
// =====================================================================
#ifndef _PATHS_H
#define _PATHS_H
#define _PATH_TTY "/dev/tty"
#define _PATH_DEVNULL "/dev/null"
#define _PATH_CONSOLE "/dev/console"
#define _PATH_VARRUN "/var/run/"
#define _PATH_UTMP "/var/run/utmp"
#define _PATH_WTMP "/var/log/wtmp"
#define _PATH_MOUNTED "/etc/mtab"
#define _PATH_DEFPATH "/usr/bin:/bin"
#endif
