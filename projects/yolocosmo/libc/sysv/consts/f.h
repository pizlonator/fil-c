#ifndef COSMOPOLITAN_LIBC_SYSV_CONSTS_F_H_
#define COSMOPOLITAN_LIBC_SYSV_CONSTS_F_H_

#define F_GETFL 3
#define F_SETFL 4

#define F_GETFD 1
#define F_SETFD 2

#define FD_CLOEXEC 1

#define F_DUPFD 0

#define F_DUPFD_CLOEXEC 0x0406

/* Fil-C additions: the Fil-C runtime (libpizlo) switches on fcntl command
   values, so the Linux command numbers must be compile-time constants. These
   mirror musl's fcntl.h for Linux. The `extern const int` objects below (and
   the self-referential macros they used to back) are kept for compatibility,
   but on Linux their values are exactly these. */
#define F_SETLK 6
#define F_SETLKW 7
#define F_GETLK 5

#define F_SETOWN 8
#define F_GETOWN 9
#define F_SETSIG 10
#define F_GETSIG 11

#define F_SETOWN_EX 15
#define F_GETOWN_EX 16
#define F_GETOWNER_UIDS 17

#define F_OFD_GETLK 36
#define F_OFD_SETLK 37
#define F_OFD_SETLKW 38
#define F_CANCELLK 39

#define F_SETLEASE 1024
#define F_GETLEASE 1025
#define F_NOTIFY 1026
#define F_SETPIPE_SZ 1031
#define F_GETPIPE_SZ 1032
#define F_ADD_SEALS 1033
#define F_GET_SEALS 1034
#define F_GET_RW_HINT 1035
#define F_SET_RW_HINT 1036
#define F_GET_FILE_RW_HINT 1037
#define F_SET_FILE_RW_HINT 1038

#define F_SEAL_SEAL         0x0001
#define F_SEAL_SHRINK       0x0002
#define F_SEAL_GROW         0x0004
#define F_SEAL_WRITE        0x0008
#define F_SEAL_FUTURE_WRITE 0x0010
#define F_SEAL_EXEC         0x0020

#define F_RDLCK F_RDLCK
#define F_WRLCK F_WRLCK
#define F_UNLCK 2

COSMOPOLITAN_C_START_

/* F_SETLK, F_SETLKW, and F_GETLK are now compile-time constants (see above),
   so they no longer have extern const backing objects declared here. */
extern const int F_RDLCK;
extern const int F_WRLCK;

int fcntl(int fd, int cmd, ...) libcesque;

int __fcntl_getfl(int) libcesque;
int __fcntl_getfd(int) libcesque;
int __fcntl_setfl(int, ...) libcesque;
int __fcntl_setfd(int, ...) libcesque;
int __fcntl_dupfd(int, ...) libcesque;
int __fcntl_dupfd_cloexec(int, ...) libcesque;
int __fcntl_lock(int, int, ...) libcesque;
int __fcntl_misc(int, int, ...) libcesque;

#if defined(__OPTIMIZE__) && !defined(__cplusplus) && \
    (defined(__GNUC__) || defined(__llvm__))
/* Undiamond fcntl() to avoid linking POSIX advisory locks */
#define fcntl(fd, cmd, ...)                                \
  (__extension__({                                         \
    int _rc;                                               \
    if (__fcntl_equivalent(cmd, F_GETFL)) {                \
      _rc = __fcntl_getfl(fd);                             \
    } else if (__fcntl_equivalent(cmd, F_GETFD)) {         \
      _rc = __fcntl_getfd(fd);                             \
    } else if (__fcntl_equivalent(cmd, F_SETFL)) {         \
      _rc = __fcntl_setfl(fd, ##__VA_ARGS__);              \
    } else if (__fcntl_equivalent(cmd, F_SETFD)) {         \
      _rc = __fcntl_setfd(fd, ##__VA_ARGS__);              \
    } else if (__fcntl_equivalent(cmd, F_DUPFD)) {         \
      _rc = __fcntl_dupfd(fd, ##__VA_ARGS__);              \
    } else if (__fcntl_equivalent(cmd, F_DUPFD_CLOEXEC)) { \
      _rc = __fcntl_dupfd_cloexec(fd, ##__VA_ARGS__);      \
    } else if (__fcntl_equivalent(cmd, F_GETLK) ||         \
               __fcntl_equivalent(cmd, F_SETLK) ||         \
               __fcntl_equivalent(cmd, F_SETLKW)) {        \
      _rc = __fcntl_lock(fd, cmd, ##__VA_ARGS__);          \
    } else {                                               \
      _rc = fcntl(fd, cmd, ##__VA_ARGS__);                 \
    }                                                      \
    _rc;                                                   \
  }))
#define __fcntl_equivalent(X, Y) \
  (__builtin_constant_p((X) == (Y)) && ((X) == (Y)))
#endif

COSMOPOLITAN_C_END_
#endif /* COSMOPOLITAN_LIBC_SYSV_CONSTS_F_H_ */
