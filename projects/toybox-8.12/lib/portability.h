// Workarounds for horrible build environment idiosyncrasies.

// Instead of polluting the code with strange #ifdefs to work around bugs
// in specific compiler, library, or OS versions, localize all that here
// and in portability.c

// Always use long file support.
// This must come before we #include any system header file to take effect!
#define _FILE_OFFSET_BITS 64

#ifdef __APPLE__
// macOS 10.13 doesn't have the POSIX 2008 direct access to timespec in
// struct stat, but we can ask it to give us something equivalent...
// (This must come before any #include!)
#define _DARWIN_C_SOURCE
// ...and then use macros to paper over the difference.
#define st_atim st_atimespec
#define st_ctim st_ctimespec
#define st_mtim st_mtimespec
#endif

// For musl
#define _ALL_SOURCE
#include <regex.h>
#ifndef REG_STARTEND
#define REG_STARTEND 0
#endif

// for some reason gnu/libc only sets these if you #define ia_ia_stallman_ftaghn
// despite FreeBSD and MacOS having both with the same value, and bionic's
// "upstream-openbsd" directory documenting them as "BSD extensions".
// (The flexible extension would have been an fnmatch() that returns length
// matched at location so we could check trailing data ourselves, but no.
// And it's ANSI only case matching instead of UTF8...)
#include <fnmatch.h>
#ifndef FNM_LEADING_DIR
#define FNM_LEADING_DIR   8
#endif
#ifndef FNM_CASEFOLD
#define FNM_CASEFOLD     16
#endif

// Test for gcc (using compiler builtin #define)

#ifdef __GNUC__
#define QUIET = 0 // shut up false positive "may be used uninitialized" warning
#define printf_format	__attribute__((format(printf, 1, 2)))
#else
#define QUIET
#define printf_format
#endif

// This lets us determine what libc we're using: systems that have <features.h>
// will transitively include it, and ones that don't (macOS) won't break.
#include <sys/types.h>

// Various constants old build environments might not have even if kernel does

#ifndef AT_FDCWD             // Kernel commit 5590ff0d5528 2006
#define AT_FDCWD -100
#endif

#ifndef AT_SYMLINK_NOFOLLOW
#define AT_SYMLINK_NOFOLLOW 0x100
#endif

#ifndef AT_REMOVEDIR
#define AT_REMOVEDIR 0x200
#endif

#ifndef RLIMIT_RTTIME // Commit 78f2c7db6068f 2008
#define RLIMIT_RTTIME 15
#endif

// Introduced in Linux 3.1 (Commit 982d816581eee 2011)
#ifndef SEEK_DATA
#define SEEK_DATA 3
#endif
#ifndef SEEK_HOLE
#define SEEK_HOLE 4
#endif

// We don't define GNU_dammit because we're not part of the gnu project, and
// don't want to get any FSF on us. Unfortunately glibc (gnu libc)
// won't give us Linux syscall wrappers without claiming to be part of the
// gnu project (because Stallman's "GNU owns Linux" revisionist history
// crusade includes the kernel, even though Linux was inspired by Minix).

// We use most non-posix Linux syscalls directly through the syscall() wrapper,
// but even many posix-2008 functions aren't provided by glibc unless you
// claim it's in the name of Gnu.

#if defined(__GLIBC__)
// Glibc violates posix: "Function prototypes shall be provided." but aren't.
// http://pubs.opengroup.org/onlinepubs/9699919799/basedefs/unistd.h.html
char *crypt(const char *key, const char *salt);

// According to posix, #include header, get a function definition. But glibc...
// http://pubs.opengroup.org/onlinepubs/9699919799/functions/wcwidth.html
#include <wchar.h>
int wcwidth(wchar_t wc);

// see http://pubs.opengroup.org/onlinepubs/9699919799/functions/strptime.html
#include <time.h>
char *strptime(const char *buf, const char *format, struct tm *tm);

// Gnu didn't like posix basename so they defined another function with the
// same name and if you include libgen.h it #defines basename to something
// else (where they implemented the real basename), and that define breaks
// the table entry for the basename command. They didn't make a new function
// with a different name for their new behavior because gnu.
//
// Solution: don't use their broken header and provide an inline to redirect
// the standard name to the renamed function with the standard behavior.

char *dirname(char *path);
char *__xpg_basename(char *path);
static inline char *basename(char *path) { return __xpg_basename(path); }
char *strcasestr(const char *haystack, const char *needle);
void *memmem(const void *haystack, size_t haystack_length,
  const void *needle, size_t needle_length);
#endif // defined(glibc)

#if !defined(__GLIBC__)
// POSIX basename.
#include <libgen.h>
#endif

// Work out how to do endianness

#define IS_LITTLE_ENDIAN (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#define IS_BIG_ENDIAN (!IS_LITTLE_ENDIAN)

#ifdef __APPLE__

#include <libkern/OSByteOrder.h>
#define bswap_16(x) OSSwapInt16(x)
#define bswap_32(x) OSSwapInt32(x)
#define bswap_64(x) OSSwapInt64(x)
#elif defined(__FreeBSD__) || defined(__OpenBSD__)
#include <sys/endian.h>
#define bswap_16(x) bswap16(x)
#define bswap_32(x) bswap32(x)
#define bswap_64(x) bswap64(x)
#else
#include <byteswap.h>
#endif

#if IS_BIG_ENDIAN
#define SWAP_BE16(x) (x)
#define SWAP_BE32(x) (x)
#define SWAP_BE64(x) (x)
#define SWAP_LE16(x) bswap_16(x)
#define SWAP_LE32(x) bswap_32(x)
#define SWAP_LE64(x) bswap_64(x)
#else
#define SWAP_BE16(x) bswap_16(x)
#define SWAP_BE32(x) bswap_32(x)
#define SWAP_BE64(x) bswap_64(x)
#define SWAP_LE16(x) (x)
#define SWAP_LE32(x) (x)
#define SWAP_LE64(x) (x)
#endif

// Linux headers not listed by POSIX or LSB
#include <sys/mount.h>
#ifdef __linux__
#include <sys/statfs.h>
#include <sys/swap.h>
#include <sys/sysinfo.h>
#endif

#ifdef __APPLE__
#include <util.h>
#elif !defined(__FreeBSD__) && !defined(__OpenBSD__)
#include <pty.h>
#else
#include <termios.h>
#ifndef IUTF8
#define IUTF8 0
#endif
#endif

#ifdef __linux__
#include <sys/personality.h>
#else
#define PER_LINUX32 0
int personality(int);
#endif

#if defined(__APPLE__) || defined(__linux__)
// Linux and macOS has both have getxattr and friends in <sys/xattr.h>, but
// they aren't compatible.
#include <sys/xattr.h>
ssize_t xattr_get(const char *, const char *, void *, size_t);
ssize_t xattr_lget(const char *, const char *, void *, size_t);
ssize_t xattr_fget(int fd, const char *, void *, size_t);
ssize_t xattr_list(const char *, char *, size_t);
ssize_t xattr_llist(const char *, char *, size_t);
ssize_t xattr_flist(int, char *, size_t);
ssize_t xattr_set(const char*, const char*, const void*, size_t, int);
ssize_t xattr_lset(const char*, const char*, const void*, size_t, int);
ssize_t xattr_fset(int, const char*, const void*, size_t, int);
#endif

#if defined(__APPLE__)
// macOS doesn't have these functions, but we can fake them.
int mknodat(int, const char*, mode_t, dev_t);
int posix_fallocate(int, off_t, off_t);

// macOS keeps newlocale(3) in the non-POSIX <xlocale.h> rather than <locale.h>.
#include <xlocale.h>
#endif

#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__)
static inline long statfs_bsize(struct statfs *sf) { return sf->f_iosize; }
static inline long statfs_frsize(struct statfs *sf) { return sf->f_bsize; }
#else
static inline long statfs_bsize(struct statfs *sf) { return sf->f_bsize; }
static inline long statfs_frsize(struct statfs *sf) { return sf->f_frsize; }
#endif


// Android is missing some headers and functions
// "generated/config.h" is included first
#if __has_include(<shadow.h>)
#include <shadow.h>
#endif
#if __has_include(<utmpx.h>)
#include <utmpx.h>
#else
struct utmpx {int ut_type;};
#define USER_PROCESS 0
static inline struct utmpx *getutxent(void) {return 0;}
static inline void setutxent(void) {;}
static inline void endutxent(void) {;}
#endif

// Some systems don't define O_NOFOLLOW, and it varies by architecture, so...
#include <fcntl.h>
#if defined(__APPLE__)
#define O_PATH 0
#else
#ifndef O_NOFOLLOW
#define O_NOFOLLOW 0
#endif
#ifndef O_NOATIME
#define O_NOATIME 01000000
#endif
#ifndef O_CLOEXEC
#define O_CLOEXEC 02000000
#endif
#ifndef O_PATH
#define O_PATH   010000000
#endif
#ifndef SCHED_RESET_ON_FORK
#define SCHED_RESET_ON_FORK (1<<30)
#endif
#endif

// Glibc won't give you linux-kernel constants unless you say "no, a BUD lite"
// even though linux has nothing to do with the FSF and never has.
#ifndef F_SETPIPE_SZ
#define F_SETPIPE_SZ 1031
#endif

#ifndef F_GETPIPE_SZ
#define F_GETPIPE_SZ 1032
#endif

#if defined(__SIZEOF_DOUBLE__) && defined(__SIZEOF_LONG__) \
    && __SIZEOF_DOUBLE__ <= __SIZEOF_LONG__
typedef double FLOAT;
#else
typedef float FLOAT;
#endif

#ifndef __uClinux__
pid_t xfork(void);
#endif

// gratuitously memsets ALL the extra space with zeroes (not just a terminator)
// but to make up for it truncating doesn't null terminate the output at all.
// There are occasions to use it, but it is NOT A GENERAL PURPOSE FUNCTION.
// #define strncpy(...) @@strncpyisbadmmkay@@
// strncat writes a null terminator one byte PAST the buffer size it's given.
#define strncat(...) strncatisbadmmkay(__VA_ARGS__)

// Support building the Android tools on glibc, so hermetic AOSP builds can
// use toybox before they're ready to switch to host bionic.
#ifdef __BIONIC__
#include <android/log.h>
#else
typedef enum android_LogPriority {
  ANDROID_LOG_UNKNOWN = 0,
  ANDROID_LOG_DEFAULT,
  ANDROID_LOG_VERBOSE,
  ANDROID_LOG_DEBUG,
  ANDROID_LOG_INFO,
  ANDROID_LOG_WARN,
  ANDROID_LOG_ERROR,
  ANDROID_LOG_FATAL,
  ANDROID_LOG_SILENT,
} android_LogPriority;
#endif
#if !defined(__BIONIC__) || defined(__ANDROID_NDK__)
// Android NDKv18 has liblog.so but not liblog.a for static builds.
static inline int stub_out_log_write(int pri, const char *tag, const char *msg)
{
  return -1;
}
#ifdef __ANDROID_NDK__
#define __android_log_write(a, b, c) stub_out_log_write(a, b, c)
#endif

#endif

#ifndef SYSLOG_NAMES
typedef struct {char *c_name; int c_val;} CODE;
extern CODE prioritynames[], facilitynames[];
#endif

#if __has_include (<sys/random.h>)
#include <sys/random.h>
#endif
void xgetrandom(void *buf, unsigned len);

// Android's bionic libc doesn't have confstr.
#ifdef __BIONIC__
#define _CS_PATH	0
#define _CS_V7_ENV	1
#include <string.h>
static inline void confstr(int a, char *b, int c) {strcpy(b, a ? "POSIXLY_CORRECT=1" : "/bin:/usr/bin");}
#endif

// Paper over the differences between BSD kqueue and Linux inotify for tail.

struct xnotify {
  char **paths;
  int max, *fds, count, kq;
};

struct xnotify *xnotify_init(int max);
int xnotify_add(struct xnotify *not, int fd, char *path);
int xnotify_wait(struct xnotify *not, char **path);

int sig_to_num(char *s);
char *num_to_sig(int sig);

struct signame {
  int num;
  char *name;
};
void xsignal_all_killers(void *handler);

// Different OSes encode major/minor device numbers differently.
int dev_minor(int dev);
int dev_major(int dev);
int dev_makedev(int major, int minor);

char *fs_type_name(struct statfs *statfs);

int get_block_device_size(int fd, unsigned long long *size);
int rename_exchange(char *file1, char *file2);

#ifdef __APPLE__
// Apple doesn't have POSIX timers; this is "just enough" for timeout(1).
typedef int timer_t;
struct itimerspec {
  struct timespec it_value;
};
int timer_create(clock_t c, struct sigevent *se, timer_t *t);
int timer_settime(timer_t t, int flags, struct itimerspec *new, void *old);
#elif defined(__GLIBC__)
// Work around a glibc bug that interacts badly with a gcc bug.
#include <syscall.h>
#include <signal.h>
#include <time.h>
int timer_create_wrap(clockid_t c, struct sigevent *se, timer_t *t);
#define timer_create(...) timer_create_wrap(__VA_ARGS__)
int timer_settime_wrap(timer_t t, int flags, struct itimerspec *val,
  struct itimerspec *old);
#define timer_settime(...) timer_settime_wrap(__VA_ARGS__)
#endif
