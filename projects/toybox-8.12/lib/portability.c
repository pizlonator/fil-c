/* portability.c - code to workaround the deficiencies of various platforms.
 *
 * Copyright 2012 Rob Landley <rob@landley.net>
 * Copyright 2012 Georgi Chorbadzhiyski <gf@unixsol.org>
 */

#include "toys.h"

// We can't fork() on nommu systems, and vfork() requires an exec() or exit()
// before resuming the parent (because they share a heap until then). And no,
// we can't implement our own clone() call that does the equivalent of fork()
// because nommu heaps use physical addresses so if we copy the heap all our
// pointers are wrong. (You need an mmu in order to map two heaps to the same
// address range without interfering with each other.) In the absence of
// a portable way to tell malloc() to start a new heap without freeing the old
// one, you pretty much need the exec().)

// So we exec ourselves (via /proc/self/exe, if anybody knows a way to
// re-exec self without depending on the filesystem, I'm all ears),
// and use the arguments to signal reentry.

#if CFG_TOYBOX_FORK
pid_t xfork(void)
{
  pid_t pid = fork();

  if (pid < 0) perror_exit("fork");

  return pid;
}
#endif

void xgetrandom(void *buf, unsigned buflen)
{
  int fd;

  // Linux keeps getrandom() in <sys/random.h> and getentropy() in <unistd.h>
  // BSD/macOS only has getentropy(), but it's in <sys/random.h> (to be fair,
  // they were there first). getrandom() and getentropy() both went into glibc
  // in the same release (2.25 in 2017), so this test still works.
#if __has_include(<sys/random.h>)
  while (buflen) {
    if (getentropy(buf, fd = buflen>256 ? 256 : buflen)) break;
    buflen -= fd;
    buf += fd;
  }
  if (!buflen) return;
  if (errno!=ENOSYS) perror_exit("getrandom");
#endif
  xreadall(fd = xopen("/dev/urandom", O_RDONLY), buf, buflen);
  close(fd);
}

// Get list of mounted filesystems, including stat and statvfs info.
// Returns a reversed list, which is good for finding overmounts and such.

#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__)

#include <sys/mount.h>

struct mtab_list *xgetmountlist(char *path)
{
  struct mtab_list *mtlist = 0, *mt;
  struct statfs *entries;
  int i, count;

  if (path) error_exit("xgetmountlist");
  if (!(count = getmntinfo(&entries, MNT_WAIT))) perror_exit("getmntinfo");

  // The "test" part of the loop is done before the first time through and
  // again after each "increment", so putting the actual load there avoids
  // duplicating it. If the load was NULL, the loop stops.

  for (i = 0; i < count; ++i) {
    struct statfs *me = &entries[i];

    mt = xzalloc(sizeof(struct mtab_list) + strlen(me->f_fstypename) +
      strlen(me->f_mntonname) + strlen(me->f_mntfromname) + strlen("") + 4);
    dlist_add_nomalloc((void *)&mtlist, (void *)mt);

    // Collect details about mounted filesystem.
    // Don't report errors, just leave data zeroed.
    stat(me->f_mntonname, &(mt->stat));
    statvfs(me->f_mntonname, &(mt->statvfs));

    // Remember information from struct statfs.
    mt->dir = stpcpy(mt->type, me->f_fstypename)+1;
    mt->device = stpcpy(mt->dir, me->f_mntonname)+1;
    mt->opts = stpcpy(mt->device, me->f_mntfromname)+1;
    strcpy(mt->opts, ""); /* TODO: reverse from f_flags? */
  }

  return mtlist;
}

#else

#include <mntent.h>

// Check if this type matches list.
// Odd syntax: typelist all yes = if any, typelist all no = if none.

int mountlist_istype(struct mtab_list *ml, char *typelist)
{
  int len, skip;
  char *t;

  if (!typelist) return 1;

  // leading "no" indicates whether entire list is inverted
  skip = strncmp(typelist, "no", 2);

  for (;;) {
    if (!(t = comma_iterate(&typelist, &len))) break;
    if (!skip) {
      // later "no" after first are ignored
      strstart(&t, "no");
      if (!strncmp(t, ml->type, len-2)) {
        skip = 1;
        break;
      }
    } else if (!strncmp(t, ml->type, len) && !ml->type[len]) {
      skip = 0;
      break;
    }
  }

  return !skip;
}

struct mtab_list *xgetmountlist(char *path)
{
  struct mtab_list *mtlist = 0, *mt;
  struct mntent *me;
  FILE *fp;
  char *p = path ? path : "/proc/mounts";

  if (!(fp = setmntent(p, "r"))) perror_exit("bad %s", p);

  // The "test" part of the loop is done before the first time through and
  // again after each "increment", so putting the actual load there avoids
  // duplicating it. If the load was NULL, the loop stops.

  while ((me = getmntent(fp))) {
    mt = xzalloc(sizeof(struct mtab_list) + strlen(me->mnt_fsname) +
      strlen(me->mnt_dir) + strlen(me->mnt_type) + strlen(me->mnt_opts) + 4);
    dlist_add_nomalloc((void *)&mtlist, (void *)mt);

    // Collect details about mounted filesystem
    // Don't report errors, just leave data zeroed
    if (!path) {
      stat(me->mnt_dir, &(mt->stat));
      statvfs(me->mnt_dir, &(mt->statvfs));
    }

    // Remember information from /proc/mounts
    mt->dir = stpcpy(mt->type, me->mnt_type)+1;
    mt->device = stpcpy(mt->dir, me->mnt_dir)+1;
    mt->opts = stpcpy(mt->device, me->mnt_fsname)+1;
    strcpy(mt->opts, me->mnt_opts);

    octal_deslash(mt->dir);
    octal_deslash(mt->device);
  }
  endmntent(fp);

  return mtlist;
}

#endif

#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__)

#include <sys/event.h>

struct xnotify *xnotify_init(int max)
{
  struct xnotify *not = xzalloc(sizeof(struct xnotify));

  not->max = max;
  if ((not->kq = kqueue()) == -1) perror_exit("kqueue");
  not->paths = xmalloc(max * sizeof(char *));
  not->fds = xmalloc(max * sizeof(int));

  return not;
}

int xnotify_add(struct xnotify *not, int fd, char *path)
{
  struct kevent event;

  if (not->count == not->max) error_exit("xnotify_add overflow");
  EV_SET(&event, fd, EVFILT_VNODE, EV_ADD|EV_CLEAR, NOTE_WRITE, 0, NULL);
  if (kevent(not->kq, &event, 1, NULL, 0, NULL) == -1 || event.flags & EV_ERROR)
    error_exit("xnotify_add failed on %s", path);
  not->paths[not->count] = path;
  not->fds[not->count++] = fd;

  return 0;
}

int xnotify_wait(struct xnotify *not, char **path)
{
  struct kevent event;
  int i;

  for (;;) {
    if (kevent(not->kq, NULL, 0, &event, 1, NULL) != -1) {
      // We get the fd for free, but still have to search for the path.
      for (i = 0; i<not->count; i++) if (not->fds[i]==event.ident) {
        *path = not->paths[i];

        return event.ident;
      }
    }
  }
}

#else

#include <sys/inotify.h>

struct xnotify *xnotify_init(int max)
{
  struct xnotify *not = xzalloc(sizeof(struct xnotify));

  not->max = max;
  if ((not->kq = inotify_init()) < 0) perror_exit("inotify_init");
  not->paths = xmalloc(max * sizeof(char *));
  not->fds = xmalloc(max * 2 * sizeof(int));

  return not;
}

int xnotify_add(struct xnotify *not, int fd, char *path)
{
  int i = 2*not->count;

  if (not->max == not->count) error_exit("xnotify_add overflow");
  if ((not->fds[i] = inotify_add_watch(not->kq, path, IN_MODIFY))==-1)
    perror_exit("xnotify_add failed on %s", path);
  not->fds[i+1] = fd;
  not->paths[not->count++] = path;

  return 0;
}

int xnotify_wait(struct xnotify *not, char **path)
{
  struct inotify_event ev;
  int i;

  for (;;) {
    if (sizeof(ev)!=read(not->kq, &ev, sizeof(ev))) perror_exit("inotify");

    for (i = 0; i<not->count; i++) if (ev.wd==not->fds[2*i]) {
      *path = not->paths[i];

      return not->fds[2*i+1];
    }
  }
}

#endif

#ifdef __APPLE__

ssize_t xattr_get(const char *path, const char *name, void *value, size_t size)
{
  return getxattr(path, name, value, size, 0, 0);
}

ssize_t xattr_lget(const char *path, const char *name, void *value, size_t size)
{
  return getxattr(path, name, value, size, 0, XATTR_NOFOLLOW);
}

ssize_t xattr_fget(int fd, const char *name, void *value, size_t size)
{
  return fgetxattr(fd, name, value, size, 0, 0);
}

ssize_t xattr_list(const char *path, char *list, size_t size)
{
  return listxattr(path, list, size, 0);
}

ssize_t xattr_llist(const char *path, char *list, size_t size)
{
  return listxattr(path, list, size, XATTR_NOFOLLOW);
}

ssize_t xattr_flist(int fd, char *list, size_t size)
{
  return flistxattr(fd, list, size, 0);
}

ssize_t xattr_set(const char* path, const char* name,
                  const void* value, size_t size, int flags)
{
  return setxattr(path, name, value, size, 0, flags);
}

ssize_t xattr_lset(const char* path, const char* name,
                   const void* value, size_t size, int flags)
{
  return setxattr(path, name, value, size, 0, flags | XATTR_NOFOLLOW);
}

ssize_t xattr_fset(int fd, const char* name,
                   const void* value, size_t size, int flags)
{
  return fsetxattr(fd, name, value, size, 0, flags);
}

#elif !defined(__FreeBSD__) && !defined(__OpenBSD__)

ssize_t xattr_get(const char *path, const char *name, void *value, size_t size)
{
  return getxattr(path, name, value, size);
}

ssize_t xattr_lget(const char *path, const char *name, void *value, size_t size)
{
  return lgetxattr(path, name, value, size);
}

ssize_t xattr_fget(int fd, const char *name, void *value, size_t size)
{
  return fgetxattr(fd, name, value, size);
}

ssize_t xattr_list(const char *path, char *list, size_t size)
{
  return listxattr(path, list, size);
}

ssize_t xattr_llist(const char *path, char *list, size_t size)
{
  return llistxattr(path, list, size);
}

ssize_t xattr_flist(int fd, char *list, size_t size)
{
  return flistxattr(fd, list, size);
}

ssize_t xattr_set(const char* path, const char* name,
                  const void* value, size_t size, int flags)
{
  return setxattr(path, name, value, size, flags);
}

ssize_t xattr_lset(const char* path, const char* name,
                   const void* value, size_t size, int flags)
{
  return lsetxattr(path, name, value, size, flags);
}

ssize_t xattr_fset(int fd, const char* name,
                   const void* value, size_t size, int flags)
{
  return fsetxattr(fd, name, value, size, flags);
}


#endif

#ifdef __APPLE__
// In the absence of a mknodat system call, fchdir to dirfd and back
// around a regular mknod call...
int mknodat(int dirfd, const char *path, mode_t mode, dev_t dev)
{
  int old_dirfd = open(".", O_RDONLY), result;

  if (old_dirfd == -1 || fchdir(dirfd) == -1) return -1;
  result = mknod(path, mode, dev);
  if (fchdir(old_dirfd) == -1) perror_exit("mknodat couldn't return");
  return result;
}

// As of 10.15, macOS offers an fcntl F_PREALLOCATE rather than fallocate()
// or posix_fallocate() calls. The fcntl only (as the name implies)
// pre-allocates, so we also need to ftruncate() afterwards.
int posix_fallocate(int fd, off_t offset, off_t length)
{
  int e = errno, result = 0;
  fstore_t f;

  f.fst_flags = F_ALLOCATEALL;
  f.fst_posmode = F_PEOFPOSMODE;
  f.fst_offset = 0;
  f.fst_length = offset + length;
  if (fcntl(fd, F_PREALLOCATE, &f) == -1) result = errno;
  else if (ftruncate(fd, maxof(offset+length, fdlength(fd)))) result = errno;
  errno = e;
  return result;
}
#endif

// Signals required by POSIX 2008:
// http://pubs.opengroup.org/onlinepubs/9699919799/basedefs/signal.h.html

#define SIGNIFY(x) {SIG##x, #x}

static const struct signame signames[] = {
  {0, "0"},
  // POSIX
  SIGNIFY(ABRT), SIGNIFY(ALRM), SIGNIFY(BUS),
  SIGNIFY(FPE), SIGNIFY(HUP), SIGNIFY(ILL), SIGNIFY(INT), SIGNIFY(KILL),
  SIGNIFY(PIPE), SIGNIFY(QUIT), SIGNIFY(SEGV), SIGNIFY(TERM),
  SIGNIFY(USR1), SIGNIFY(USR2), SIGNIFY(SYS), SIGNIFY(TRAP),
  SIGNIFY(VTALRM), SIGNIFY(XCPU), SIGNIFY(XFSZ),
  // Non-POSIX signals that cause termination
  SIGNIFY(PROF), SIGNIFY(IO),
  // signals only present/absent on some targets (mips and macos)
#ifdef SIGEMT
  SIGNIFY(EMT),
#endif
#ifdef SIGINFO
  SIGNIFY(INFO),
#endif
#ifdef SIGPOLL
  SIGNIFY(POLL),
#endif
#ifdef SIGPWR
  SIGNIFY(PWR),
#endif
#ifdef SIGSTKFLT
  SIGNIFY(STKFLT),
#endif

  // Note: sigatexit relies on all the signals with a default disposition that
  // terminates the process coming *before* SIGCHLD.

  // POSIX signals that don't cause termination
  SIGNIFY(CHLD), SIGNIFY(CONT), SIGNIFY(STOP), SIGNIFY(TSTP),
  SIGNIFY(TTIN), SIGNIFY(TTOU), SIGNIFY(URG),
  // Non-POSIX signals that don't cause termination
  SIGNIFY(WINCH),
};

#undef SIGNIFY

void xsignal_all_killers(void *handler)
{
  int i;

  for (i = 1; signames[i].num != SIGCHLD; i++)
    if (signames[i].num != SIGKILL) xsignal(signames[i].num, handler);
}

// Convert a string like "9", "KILL", "SIGHUP", or "SIGRTMIN+2" to a number.
int sig_to_num(char *sigstr)
{
  int i, offset;
  char *s;

  // Numeric?
  offset = estrtol(sigstr, &s, 10);
  if (!errno && !*s) return offset;

  // Skip leading "SIG".
  strcasestart(&sigstr, "sig");

  // Named signal?
  for (i=0; i<ARRAY_LEN(signames); i++)
    if (!strcasecmp(sigstr, signames[i].name)) return signames[i].num;

  // Real-time signal?
#ifdef SIGRTMIN
  if (strcasestart(&sigstr, "rtmin")) i = SIGRTMIN;
  else if (strcasestart(&sigstr, "rtmax")) i = SIGRTMAX;
  else return -1;

  // No offset?
  if (!*sigstr) return i;

  // We allow any offset that's still a real-time signal: SIGRTMIN+20 is fine.
  // Others are more restrictive, only accepting what they show with -l.
  offset = estrtol(sigstr, &s, 10);
  if (errno || *s) return -1;
  i += offset;
  if (i >= SIGRTMIN && i <= SIGRTMAX) return i;
#endif

  return -1;
}

char *num_to_sig(int sig)
{
  int i;

  // A named signal?
  for (i=0; i<ARRAY_LEN(signames); i++)
    if (signames[i].num == sig) return signames[i].name;

  // A real-time signal?
#ifdef SIGRTMIN
  if (sig == SIGRTMIN) return "RTMIN";
  if (sig == SIGRTMAX) return "RTMAX";
  if (sig > SIGRTMIN && sig < SIGRTMAX) {
    if (sig-SIGRTMIN <= SIGRTMAX-sig) sprintf(libbuf, "RTMIN+%d", sig-SIGRTMIN);
    else sprintf(libbuf, "RTMAX-%d", SIGRTMAX-sig);
    return libbuf;
  }
#endif

  return NULL;
}

int dev_minor(int dev)
{
#if defined(__linux__)
  return ((dev&0xfff00000)>>12)|(dev&0xff);
#elif defined(__APPLE__)
  return dev&0xffffff;
#elif defined(__FreeBSD__) || defined(__OpenBSD__)
  return minor(dev);
#else
#error
#endif
}

int dev_major(int dev)
{
#if defined(__linux__)
  return (dev&0xfff00)>>8;
#elif defined(__APPLE__)
  return (dev>>24)&0xff;
#elif defined(__FreeBSD__) || defined(__OpenBSD__)
  return major(dev);
#else
#error
#endif
}

int dev_makedev(int major, int minor)
{
#if defined(__linux__)
  return (minor&0xff)|((major&0xfff)<<8)|((minor&0xfff00)<<12);
#elif defined(__APPLE__)
  return (minor&0xffffff)|((major&0xff)<<24);
#elif defined(__FreeBSD__) || defined(__OpenBSD__)
  return makedev(major, minor);
#else
#error
#endif
}

char *fs_type_name(struct statfs *statfs)
{
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__)
  // macOS has an `f_type` field, but assigns values dynamically as filesystems
  // are registered. They do give you the name directly though, so use that.
  return statfs->f_fstypename;
#else
  char *s = NULL;
  struct {unsigned num; char *name;} nn[] = {
    {0xADF5, "adfs"}, {0xADFF, "affs"}, {0x5346414F, "afs"}, {0x187, "autofs"},
    {0x1BADFACE, "bfs"}, {0x6C6F6F70, "binder"}, {0x9123683E, "btrfs"},
    {0xFF534D42, "cifs"}, {0x27E0EB, "cgroup"}, {0x63677270, "cgroup2"},
    {0x73757245, "coda"}, {0x28cd3d45, "cramfs"}, {0x1CD1, "devpts"},
    {0xF15F, "ecryptfs"}, {0x414A53, "efs"}, {0xE0F5E1E2, "erofs"},
    {0x2011BAB0, "exfat"}, {0x137D, "ext"}, {0xEF51, "ext2"},
    {0xEF53, "ext3/4"}, {0xF2F52010, "f2fs"}, {0xBAD1DEA, "futexfs"},
    {0x00C0FFEE, "hostfs"}, {0xF995E849, "hpfs"},
    {0x9660, "isofs"}, {0x72B6, "jffs2"}, {0x3153464a, "jfs"},
    {0x137F, "minix"}, {0x2468, "minix2"}, {0x4D5A, "minix3"},
    {0x4D44, "vfat"}, {0x6969, "nfs"}, {0x3434, "nilfs2"},
    {0x5346544E, "ntfs"}, {0x7461636F, "ocfs2"}, {0x9FA1, "openpromfs"},
    {0x794C7630, "overlay"}, {0x9FA0, "proc"}, {0x002f, "qnx4"},
    {0x68191122, "qnx6"}, {0x7275, "romfs"}, {0x7655821, "resctrl"},
    {0x534F434B, "sockfs"}, {0x62656572, "sysfs"}, {0x517B, "smb"},
    {0x01021994, "tmpfs"}, {0x15013346, "udf"}, {0x43415d53, "smackfs"},
    {0x73717368, "squashfs"}, {0xabba1974, "xenfs"}, {0x58465342, "xfs"}
  };
  int i;

  for (i=0; i<ARRAY_LEN(nn); i++)
    if (nn[i].num == statfs->f_type) s = nn[i].name;
  if (!s) sprintf(s = libbuf, "0x%x", (unsigned)statfs->f_type);
  return s;
#endif
}

#if defined(__APPLE__)
#include <sys/disk.h>
int get_block_device_size(int fd, unsigned long long* size)
{
  unsigned long block_size, block_count;

  if (!ioctl(fd, DKIOCGETBLOCKSIZE, &block_size) &&
      !ioctl(fd, DKIOCGETBLOCKCOUNT, &block_count)) {
    *size = block_count * block_size;
    return 1;
  }
  return 0;
}
#elif defined(__linux__)
int get_block_device_size(int fd, unsigned long long* size)
{
  return (ioctl(fd, BLKGETSIZE64, size) >= 0);
}
#elif defined(__OpenBSD__)
#include <sys/dkio.h>
#include <sys/disklabel.h>
int get_block_device_size(int fd, unsigned long long* size)
{
  struct disklabel lab;
  int status = (ioctl(fd, DIOCGDINFO, &lab) >= 0);
  *size = lab.d_secsize * lab.d_nsectors;
  return status;
}
#else
int get_block_device_size(int fd, unsigned long long* size)
{
  return 0;
}
#endif

#if defined(__ANDROID__)
static int android_api_level(void)
{
  // Cached so we don't do a system property lookup on every call.
  static int api_level;

  if (!api_level) api_level = android_get_device_api_level();
  return api_level;
}
#endif

static int check_copy_file_range(void)
{
#if defined(__ANDROID__)
  // Android's had the constant for years, but seccomp means you'll get
  // SIGSYS if you try the system call before 2023's Android U.
  return (android_api_level() >= __ANDROID_API_U__) ? __NR_copy_file_range : 0;
#elif defined(__NR_copy_file_range)
  // glibc added this constant in git at the end of 2017, shipped 2018-02.
  return __NR_copy_file_range;
#else
  return 0;
#endif
}

// Return bytes copied from in to out. If bytes <0 copy all of in to out.
// If consumed isn't null, amount read saved there (return is written or error)
long long sendfile_len(int in, int out, long long bytes, long long *consumed)
{
  long long total = 0, len, ww;
  int try_cfr = check_copy_file_range();

  if (consumed) *consumed = 0;
  if (in>=0) while (bytes != total) {
    ww = 0;
    len = bytes-total;

    errno = 0;
    if (try_cfr) {
      if (bytes<0 || len>(1<<30)) len = (1<<30);
      len = syscall(try_cfr, in, 0, out, 0, len, 0);
      if (len < 0) {
        try_cfr = 0;

        continue;
      }
    } else {
      if (bytes<0 || len>sizeof(libbuf)) len = sizeof(libbuf);
      ww = len = read(in, libbuf, len);
    }
    if (len<1 && errno==EAGAIN) continue;
    if (len<1) break;
    if (consumed) *consumed += len;
    if (ww && writeall(out, libbuf, len) != len) return -1;
    total += len;
  }

  return total;
}

#ifdef __APPLE__
// The absolute minimum POSIX timer implementation to build timeout(1).
// Note that although timeout(1) uses POSIX timers to get the monotonic clock,
// that doesn't seem to be an option on macOS (without using other libraries),
// so we just mangle that back into a regular setitimer(ITIMER_REAL) call.
int timer_create(clock_t c, struct sigevent *se, timer_t *t)
{
  if (se->sigev_notify != SIGEV_SIGNAL || se->sigev_signo != SIGALRM)
    error_exit("unimplemented");
  *t = 1;
  return 0;
}

int timer_settime(timer_t t, int flags, struct itimerspec *new, void *old)
{
  struct itimerval mangled;

  if (flags != 0 || old != 0) error_exit("unimplemented");
  memset(&mangled, 0, sizeof(mangled));
  mangled.it_value.tv_sec = new->it_value.tv_sec;
  mangled.it_value.tv_usec = new->it_value.tv_nsec / 1000;
  return setitimer(ITIMER_REAL, &mangled, NULL);
}
// glibc requires -lrt for linux syscalls, which pulls in libgcc_eh.a for
// static linking, and gcc 9.3 leaks pthread calls from that breaking the build
// These are both just linux syscalls: wrap them ourselves
#elif defined(__GLIBC__)
int timer_create_wrap(clockid_t c, struct sigevent *se, timer_t *t)
{
  // convert overengineered structure to what kernel actually uses
  struct ksigevent { void *sv; int signo, notify, tid; } kk = {
    0, se->sigev_signo, se->sigev_notify, 0
  };
  int timer;

  if (syscall(SYS_timer_create, c, &kk, &timer)<0) return -1;
  *t = (timer_t)(long)timer;

  return 0;
}

#if !defined(SYS_timer_settime) && defined(SYS_timer_settime64)
// glibc does not define defines SYS_timer_settime on 32-bit systems
// with 64-bit time_t defaults e.g. riscv32
#define SYS_timer_settime SYS_timer_settime64
#endif

int timer_settime_wrap(timer_t t, int flags, struct itimerspec *val,
  struct itimerspec *old)
{
  return syscall(SYS_timer_settime, t, flags, val, old);
}
#endif

// Atomically swap two files
int rename_exchange(char *file1, char *file2)
{
#if defined(__linux__)
  // 2 is RENAME_EXCHANGE
  return syscall(SYS_renameat2, AT_FDCWD, file1, AT_FDCWD, file2, 2);
#else
  return ENOSYS;
#endif
}
