/* xwrap.c - library function wrappers that exit instead of returning error
 *
 * Functions with the x prefix either succeed or kill the program with an
 * error message, so the caller doesn't have to check for failure. They
 * usually have the same arguments and return value as the function they wrap.
 *
 * Copyright 2006 Rob Landley <rob@landley.net>
 */

#include "toys.h"

// strcpy and strncat with size checking. Size is the total space in "dest",
// including null terminator. Exit if there's not enough space for the string
// (including space for the null terminator), because silently truncating is
// still broken behavior. (And leaving the string unterminated is INSANE.)
void xstrncpy(char *dest, char *src, size_t size)
{
  if (strlen(src)+1 > size) error_exit("'%s' > %ld bytes", src, (long)size);
  strcpy(dest, src);
}

void xstrncat(char *dest, char *src, size_t size)
{
  long len = strlen(dest);

  if (len+strlen(src)+1 > size)
    error_exit("'%s%s' > %ld bytes", dest, src, (long)size);
  strcpy(dest+len, src);
}

// We replaced exit(), _exit(), and atexit() with xexit(), _xexit(), and
// sigatexit(). This gives _xexit() the option to siglongjmp(toys.rebound, 1)
// instead of exiting, lets xexit() report stdout flush failures to stderr
// and change the exit code to indicate error, lets our toys.exit function
// change happen for signal exit paths and lets us remove the functions
// after we've called them.

void _xexit(void)
{
  if (toys.rebound) siglongjmp(*toys.rebound, 1);

  _exit(toys.exitval);
}

void xexit(void)
{
  // Call toys.xexit functions in reverse order added.
  while (toys.xexit) {
    struct arg_list *al = llist_pop(&toys.xexit);

    // typecast xexit->arg to a function pointer, then call it using invalid
    // signal 0 to let signal handlers tell actual signal from regular exit.
    ((void (*)(int))(al->arg))(0);

    free(al);
  }
  if (fflush(0) || ferror(stdout)) if (!toys.exitval) perror_msg("write");
  _xexit();
}

void *xmmap(void *addr, size_t length, int prot, int flags, int fd, off_t off)
{
  void *ret = mmap(addr, length, prot, flags, fd, off);
  if (ret == MAP_FAILED) perror_exit("mmap");
  return ret;
}

// Die unless we can allocate memory.
void *xmalloc(size_t size)
{
  void *ret = malloc(size);
  if (!ret) error_exit("xmalloc(%ld)", (long)size);

  return ret;
}

// Die unless we can allocate prezeroed memory.
void *xzalloc(size_t size)
{
  void *ret = xmalloc(size);
  memset(ret, 0, size);
  return ret;
}

// Die unless we can change the size of an existing allocation, possibly
// moving it.  (Notice different arguments from libc function.)
void *xrealloc(void *ptr, size_t size)
{
  ptr = realloc(ptr, size);
  if (!ptr) error_exit("xrealloc");

  return ptr;
}

// Die unless we can allocate a copy of this many bytes of string.
char *xstrndup(char *s, size_t n)
{
  char *ret = strndup(s, n);
  if (!ret) error_exit("xstrndup");

  return ret;
}

// Die unless we can allocate a copy of this string.
char *xstrdup(char *s)
{
  long len = strlen(s);
  char *c = xmalloc(++len);

  memcpy(c, s, len);

  return c;
}

void *xmemdup(void *s, long len)
{
  void *ret = xmalloc(len);
  memcpy(ret, s, len);

  return ret;
}

// Die unless we can allocate enough space to sprintf() into.
char *xmprintf(char *format, ...)
{
  va_list va, va2;
  int len;
  char *ret;

  va_start(va, format);
  va_copy(va2, va);

  // How long is it?
  len = vsnprintf(0, 0, format, va)+1;
  va_end(va);

  // Allocate and do the sprintf()
  ret = xmalloc(len);
  vsnprintf(ret, len, format, va2);
  va_end(va2);

  return ret;
}

void xferror(FILE *fp)
{
  if (ferror(fp)) perror_exit(fp==stdout ? "stdout" : "write");
}

void xprintf(char *format, ...)
{
  va_list va;
  va_start(va, format);

  vprintf(format, va);
  va_end(va);

  xferror(stdout);
}

// Put string with length (does not append newline) with immediate flush
void xputsl(char *s, int len)
{
  fwrite(s, 1, len, stdout);
  fflush(stdout);
  xferror(stdout);
}

// xputs with no newline
void xputsn(char *s)
{
  xputsl(s, strlen(s));
}

// Write string to stdout with newline, checking for errors
void xputs(char *s)
{
  puts(s);
  xferror(stdout);
}

void xputc(char c)
{
  fputc(c, stdout);
  xferror(stdout);
}

// daemonize via vfork(). Does not chdir("/"), caller should do that first
// note: restarts process from command_main()
void xvdaemon(void)
{
  int fd;

  // vfork and exec /proc/self/exe
  if (toys.stacktop) {
    xpopen_both(0, 0);
    _exit(0);
  }

  // new session id, point fd 0-2 at /dev/null, detach from tty
  chdir("/");
  setsid();
  close(0);
  xopen_stdio("/dev/null", O_RDWR);
  dup2(0, 1);
  if (-1 != (fd = open("/dev/tty", O_RDONLY))) {
    ioctl(fd, TIOCNOTTY);
    close(fd);
  }
  dup2(0, 2);
}

// This is called through the XVFORK macro because parent/child of vfork
// share a stack, so child returning from a function would stomp the return
// address parent would need. Solution: make vfork() an argument so processes
// diverge before function gets called.
pid_t __attribute__((returns_twice)) xvforkwrap(pid_t pid)
{
  if (pid == -1) perror_exit("vfork");

  // Signal to xexec() and friends that we vforked so can't recurse
  if (!pid) toys.stacktop = 0;

  return pid;
}

// Die unless we can exec argv[] (or run builtin command).  Note that anything
// with a path isn't a builtin, so /bin/sh won't match the builtin sh.
void xexec(char **argv)
{
  // Only recurse to builtin when we have multiplexer and !vfork context.
  if (CFG_TOYBOX && !CFG_TOYBOX_NORECURSE)
    if (toys.stacktop && !strchr(*argv, '/')) toy_exec(argv);
  execvp(argv[0], argv);

  toys.exitval = 126+(errno == ENOENT);
  perror_msg("exec %s", argv[0]);
  if (!toys.stacktop) _exit(toys.exitval);
  xexit();
}

// Spawn child process, capturing stdin/stdout.
// argv[]: command to exec. If null, child re-runs original program with
//         toys.stacktop zeroed.
// pipes[2]: Filehandle to move to stdin/stdout of new process.
//           If -1, replace with pipe handle connected to stdin/stdout.
//           NULL treated as {0, 1}, I.E. leave stdin/stdout as is
// return: pid of child process
pid_t xpopen_setup(char **argv, int *pipes, void (*callback)(char **argv))
{
  int cestnepasun[4], pid;

  // Make the pipes?
  memset(cestnepasun, 0, sizeof(cestnepasun));
  if (pipes) for (pid = 0; pid < 2; pid++)
    if (pipes[pid]==-1 && pipe(cestnepasun+(2*pid))) perror_exit("pipe");

  if (!(pid = CFG_TOYBOX_FORK ? xfork() : XVFORK())) {
    // Child process: Dance of the stdin/stdout redirection.
    // cestnepasun[1]->cestnepasun[0] and cestnepasun[3]->cestnepasun[2]
    if (pipes) {
      // if we had no stdin/out, pipe handles could overlap, so test for it
      // and free up potentially overlapping pipe handles before reuse

      // in child, close read end of output pipe, use write end as new stdout
      if (cestnepasun[2]) {
        close(cestnepasun[2]);
        pipes[1] = cestnepasun[3];
      }

      // in child, close write end of input pipe, use read end as new stdin
      if (cestnepasun[1]) {
        close(cestnepasun[1]);
        pipes[0] = cestnepasun[0];
      }

      // If swapping stdin/stdout, dup a filehandle that gets closed before use
      if (!pipes[1]) pipes[1] = dup(0);

      // Are we redirecting stdin?
      if (pipes[0]) {
        dup2(pipes[0], 0);
        close(pipes[0]);
      }

      // Are we redirecting stdout?
      if (pipes[1] != 1) {
        dup2(pipes[1], 1);
        close(pipes[1]);
      }
    }
    if (callback) callback(argv);
    if (argv) xexec(argv);

    // In fork() case, force recursion because we know it's us.
    if (CFG_TOYBOX_FORK) {
      toy_init(toys.which, toys.argv);
      toys.stacktop = 0;
      toys.which->toy_main();
      xexit();
    // In vfork() case, exec /proc/self/exe with high bit of first letter set
    // to tell main() we reentered.
    } else {
      char *s = "/proc/self/exe";

      // We did a nommu-friendly vfork but must exec to continue.
      // setting high bit of argv[0][0] to let new process know
      **toys.argv |= 0x80;
      execv(s, toys.argv);
      if ((s = getenv("_"))) execv(s, toys.argv);
      perror_msg_raw(s);

      _exit(127);
    }
  }

  // Parent process: vfork had a shared environment, clean up.
  if (!CFG_TOYBOX_FORK) **toys.argv &= 0x7f;

  if (pipes) {
    if (cestnepasun[1]) {
      pipes[0] = cestnepasun[1];
      close(cestnepasun[0]);
    }
    if (cestnepasun[2]) {
      pipes[1] = cestnepasun[2];
      close(cestnepasun[3]);
    }
  }

  return pid;
}

pid_t xpopen_both(char **argv, int *pipes)
{
  return xpopen_setup(argv, pipes, 0);
}


// Wait for child process to exit, then return adjusted exit code.
int xwaitpid(pid_t pid)
{
  int status = 127<<8;

  while (-1 == waitpid(pid, &status, 0) && errno == EINTR) errno = 0;

  return WIFEXITED(status) ? WEXITSTATUS(status) : WTERMSIG(status)+128;
}

int xpclose_both(pid_t pid, int *pipes)
{
  if (pipes) {
    if (pipes[0]) close(pipes[0]);
    if (pipes[1]>1) close(pipes[1]);
  }

  return xwaitpid(pid);
}

// Wrapper to xpopen with a pipe for just one of stdin/stdout
pid_t xpopen(char **argv, int *pipe, int isstdout)
{
  int pipes[2], pid;

  pipes[0] = isstdout ? 0 : -1;
  pipes[1] = isstdout ? -1 : 1;
  pid = xpopen_both(argv, pipes);
  *pipe = pid ? pipes[!!isstdout] : -1;

  return pid;
}

int xpclose(pid_t pid, int pipe)
{
  close(pipe);

  return xpclose_both(pid, 0);
}

// Call xpopen and wait for it to finish, keeping existing stdin/stdout.
int xrun(char **argv)
{
  return xpclose_both(xpopen_both(argv, 0), 0);
}

// Run child, writing to_stdin, returning stdout or NULL, pass through stderr
char *xrunread(char *argv[], char *to_stdin)
{
  char *result = 0;
  int pipe[] = {-1, -1}, total = 0, len;
  pid_t pid;

  pid = xpopen_both(argv, pipe);
  if (to_stdin && *to_stdin) writeall(*pipe, to_stdin, strlen(to_stdin));
  close(*pipe);
  for (;;) {
    if (0>=(len = readall(pipe[1], libbuf, sizeof(libbuf)))) break;
    memcpy((result = xrealloc(result, 1+total+len))+total, libbuf, len);
    total += len;
    if (len != sizeof(libbuf)) break;
  }
  if (result) result[total] = 0;
  close(pipe[1]);

  if (xwaitpid(pid)) {
    free(result);

    return 0;
  }

  return result;
}

void xaccess(char *path, int flags)
{
  if (access(path, flags)) perror_exit("Can't access '%s'", path);
}

// Die unless we can delete a file.  (File must exist to be deleted.)
void xunlink(char *path)
{
  if (unlink(path)) perror_exit("unlink '%s'", path);
}

// Die unless we can open/create a file, returning file descriptor.
// The meaning of O_CLOEXEC is reversed (it defaults on, pass it to disable)
// and WARN_ONLY tells us not to exit.
int xcreate_stdio(char *path, int flags, int mode)
{
  int fd = open(path, (flags^O_CLOEXEC)&~WARN_ONLY, mode);

  if (fd == -1) ((flags&WARN_ONLY) ? perror_msg_raw : perror_exit_raw)(path);
  return fd;
}

// Die unless we can open a file, returning file descriptor.
int xopen_stdio(char *path, int flags)
{
  return xcreate_stdio(path, flags, 0);
}

void xpipe(int *pp)
{
  if (pipe(pp)) perror_exit("xpipe");
}

void xclose(int fd)
{
  if (fd != -1 && close(fd)) perror_exit("xclose");
}

int xdup(int fd)
{
  if (fd != -1) {
    fd = dup(fd);
    if (fd == -1) perror_exit("xdup");
  }
  return fd;
}

int xnotstdio(int fd)
{
  if (fd<0) return fd;

  while (fd<3) {
    int fd2 = xdup(fd);

    close(fd);
    xopen_stdio("/dev/null", O_RDWR);
    fd = fd2;
  }

  return fd;
}

void xrename(char *from, char *to)
{
  if (rename(from, to)) perror_exit("rename %s -> %s", from, to);
}

int xtempfile(char *name, char **tempname)
{
  int fd;

  *tempname = xmprintf("%s%s", name, "XXXXXX");
  if(-1 == (fd = mkstemp(*tempname))) error_exit("no temp file");

  return fd;
}

// Create a file but don't return stdin/stdout/stderr
int xcreate(char *path, int flags, int mode)
{
  return xnotstdio(xcreate_stdio(path, flags, mode));
}

// Open a file descriptor NOT in stdin/stdout/stderr
int xopen(char *path, int flags)
{
  return xnotstdio(xopen_stdio(path, flags));
}

// Open read only, treating "-" as a synonym for stdin, defaulting to warn only
int openro(char *path, int flags)
{
  if (!strcmp(path, "-")) return 0;

  return xopen(path, flags^WARN_ONLY);
}

// Open read only, treating "-" as a synonym for stdin.
int xopenro(char *path)
{
  return openro(path, O_RDONLY|WARN_ONLY);
}

FILE *xfdopen(int fd, char *mode)
{
  FILE *f = fdopen(fd, mode);

  if (!f) perror_exit("xfdopen");

  return f;
}

// Die unless we can open/create a file, returning FILE *.
FILE *xfopen(char *path, char *mode)
{
  FILE *f = fopen(path, mode);
  if (!f) perror_exit("No file %s", path);
  return f;
}

// Die if there's an error other than EOF.
size_t xread(int fd, void *buf, size_t len)
{
  ssize_t ret = read(fd, buf, len);
  if (ret < 0) perror_exit("xread");

  return ret;
}

void xreadall(int fd, void *buf, size_t len)
{
  if (len != readall(fd, buf, len)) perror_exit("xreadall");
}

// There's no xwriteall(), just xwrite().  When we read, there may or may not
// be more data waiting.  When we write, there is data and it had better go
// somewhere.

void xwrite(int fd, void *buf, size_t len)
{
  if (len != writeall(fd, buf, len)) perror_exit("xwrite");
}

// Die if lseek fails, probably due to being called on a pipe.

off_t xlseek(int fd, off_t offset, int whence)
{
  offset = lseek(fd, offset, whence);
  if (offset<0) perror_exit("lseek");

  return offset;
}

char *xgetcwd(void)
{
  char *buf = getcwd(NULL, 0);
  if (!buf) perror_exit("xgetcwd");

  return buf;
}

void xstat(char *path, struct stat *st)
{
  if(stat(path, st)) perror_exit("Can't stat %s", path);
}

// Canonicalize path, even to file with one or more missing components at end.
// Returns allocated string for pathname or NULL if doesn't exist. Flags are:
// ABS_PATH:path to last component must exist ABS_FILE: whole path must exist
// ABS_KEEP:keep symlinks in path ABS_LAST: keep symlink at end of path
char *xabspath(char *path, int flags)
{
  struct string_list *todo, *done = 0, *new, **tail;
  int fd, track, len, try = 9999, dirfd = -1, missing = 0;
  char *str;

  // If the last file must exist, path to it must exist.
  if (flags&ABS_FILE) flags |= ABS_PATH;
  // If we don't resolve path's symlinks, don't resolve last symlink.
  if (flags&ABS_KEEP) flags |= ABS_LAST;

  // If this isn't an absolute path, start with cwd or $PWD.
  if (*path != '/') {
    if ((flags & ABS_KEEP) && (str = getenv("PWD")))
      splitpath(path, splitpath(str, &todo));
    else {
      splitpath(path, splitpath(str = xgetcwd(), &todo));
      free(str);
    }
  } else splitpath(path, &todo);

  // Iterate through path components in todo, prepend processed ones to done.
  while (todo) {
    // break out of endless symlink loops
    if (!try--) {
      errno = ELOOP;
      goto error;
    }

    // Remove . or .. component, tracking dirfd back up tree as necessary
    str = (new = llist_pop(&todo))->str;
    // track dirfd if this component must exist or we're resolving symlinks
    track = ((flags>>!todo) & (ABS_PATH|ABS_KEEP)) ^ ABS_KEEP;
    if (!done && track) dirfd = open("/", O_PATH);
    if (*str=='.' && !str[1+((fd = str[1])=='.')]) {
      free(new);
      if (fd) {
        if (done) free(llist_pop(&done));
        if (missing) missing--;
        else if (track) {
          if (-1 == (fd = openat(dirfd, "..", O_PATH))) goto error;
          close(dirfd);
          dirfd = fd;
        }
      }
      continue;
    }

    // Is this a symlink?
    if (flags & (ABS_KEEP<<!todo)) len = 0, errno = EINVAL;
    else len = readlinkat(dirfd, str, libbuf, sizeof(libbuf));
    if (len>4095) goto error;

    // Not a symlink: add to linked list, move dirfd, fail if error
    if (len<1) {
      new->next = done;
      done = new;
      if (errno == ENOENT && !(flags & (ABS_PATH<<!todo))) missing++;
      else if (errno != EINVAL && (flags & (ABS_PATH<<!todo))) goto error;
      else if (track) {
        if (-1 == (fd = openat(dirfd, new->str, O_PATH))) goto error;
        close(dirfd);
        dirfd = fd;
      }
      continue;
    }

    // If this symlink is to an absolute path, discard existing resolved path
    libbuf[len] = 0;
    if (*libbuf == '/') {
      llist_traverse(done, free);
      done = 0;
      close(dirfd);
      dirfd = -1;
    }
    free(new);

    // prepend components of new path. Note symlink to "/" will leave new = NULL
    tail = splitpath(libbuf, &new);

    // symlink to "/" will return null and leave tail alone
    if (new) {
      *tail = todo;
      todo = new;
    }
  }
  xclose(dirfd);

  // At this point done has the path, in reverse order. Reverse list
  // (into todo) while calculating buffer length.
  try = 2;
  while (done) {
    struct string_list *temp = llist_pop(&done);

    if (todo) try++;
    try += strlen(temp->str);
    temp->next = todo;
    todo = temp;
  }

  // Assemble return buffer
  *(str = xmalloc(try)) = '/';
  str[try = 1] = 0;
  while (todo) {
    if (try>1) str[try++] = '/';
    try = stpcpy(str+try, todo->str) - str;
    free(llist_pop(&todo));
  }

  return str;

error:
  xclose(dirfd);
  llist_traverse(todo, free);
  llist_traverse(done, free);

  return 0;
}

void xchdir(char *path)
{
  if (chdir(path)) perror_exit("chdir '%s'", path);
}

void xchroot(char *path)
{
  if (chroot(path)) error_exit("chroot '%s'", path);
  xchdir("/");
}

struct passwd *xgetpwuid(uid_t uid)
{
  struct passwd *pwd = getpwuid(uid);
  if (!pwd) error_exit("bad uid %ld", (long)uid);
  return pwd;
}

struct group *xgetgrgid(gid_t gid)
{
  struct group *group = getgrgid(gid);

  if (!group) perror_exit("gid %ld", (long)gid);
  return group;
}

unsigned xgetuid(char *name)
{
  struct passwd *up = getpwnam(name);
  char *s = 0;
  long uid;

  if (up) return up->pw_uid;

  uid = estrtol(name, &s, 10);
  if (!errno && s && !*s && uid>=0 && uid<=UINT_MAX) return uid;

  error_exit("bad user '%s'", name);
}

unsigned xgetgid(char *name)
{
  struct group *gr = getgrnam(name);
  char *s = 0;
  long gid;

  if (gr) return gr->gr_gid;

  gid = estrtol(name, &s, 10);
  if (!errno && s && !*s && gid>=0 && gid<=UINT_MAX) return gid;

  error_exit("bad group '%s'", name);
}

struct passwd *xgetpwnam(char *name)
{
  struct passwd *up = getpwnam(name);

  if (!up) perror_exit("user '%s'", name);
  return up;
}

struct group *xgetgrnam(char *name)
{
  struct group *gr = getgrnam(name);

  if (!gr) perror_exit("group '%s'", name);
  return gr;
}

// setuid() can fail (for example, too many processes belonging to that user),
// which opens a security hole if the process continues as the original user.

void xsetuser(struct passwd *pwd)
{
  if (initgroups(pwd->pw_name, pwd->pw_gid) || setgid(pwd->pw_uid)
      || setuid(pwd->pw_uid)) perror_exit("xsetuser '%s'", pwd->pw_name);
}

// This can return null (meaning file not found).  It just won't return null
// for memory allocation reasons.
char *xreadlinkat(int dir, char *name)
{
  int len, size = 0;
  char *buf = 0;

  // Grow by 64 byte chunks until it's big enough.
  for(;;) {
    size +=64;
    buf = xrealloc(buf, size);
    len = readlinkat(dir, name, buf, size);

    if (len<0) {
      free(buf);
      return 0;
    }
    if (len<size) {
      buf[len]=0;
      return buf;
    }
  }
}

char *xreadlink(char *name)
{
  return xreadlinkat(AT_FDCWD, name);
}


char *xreadfile(char *name, char *buf, off_t len)
{
  if (!(buf = readfile(name, buf, len))) perror_exit("Bad '%s'", name);

  return buf;
}

// The data argument to ioctl() is actually long, but it's usually used as
// a pointer. If you need to feed in a number, do (void *)(long) typecast.
int xioctl(int fd, int request, void *data)
{
  int rc;

  errno = 0;
  rc = ioctl(fd, request, data);
  if (rc == -1 && errno) perror_exit("ioctl %x", request);

  return rc;
}

// Open a /var/run/NAME.pid file, dying if we can't write it or if it currently
// exists and is this executable.
void xpidfile(char *name)
{
  char pidfile[256], spid[32];
  int i, fd;
  pid_t pid;

  sprintf(pidfile, "/var/run/%s.pid", name);
  // Try three times to open the sucker.
  for (i=0; i<3; i++) {
    fd = open(pidfile, O_CREAT|O_EXCL|O_WRONLY, 0644);
    if (fd != -1) break;

    // If it already existed, read it.  Loop for race condition.
    fd = open(pidfile, O_RDONLY);
    if (fd == -1) continue;

    // Is the old program still there?
    spid[xread(fd, spid, sizeof(spid)-1)] = 0;
    close(fd);
    pid = atoi(spid);
    if (pid < 1 || (kill(pid, 0) && errno == ESRCH)) unlink(pidfile);

    // An else with more sanity checking might be nice here.
  }

  if (i == 3) error_exit("xpidfile %s", name);

  xwrite(fd, spid, sprintf(spid, "%ld\n", (long)getpid()));
  close(fd);
}

// error_exit if we couldn't copy all bytes
long long xsendfile_len(int in, int out, long long bytes)
{
  long long len = sendfile_len(in, out, bytes, 0);

  if (bytes != -1 && bytes != len) {
    if (out == 1 && len<0) xexit();
    error_exit("short %s", (len<0) ? "write" : "read");
  }

  return len;
}

// warn and pad with zeroes if we couldn't copy all bytes
void xsendfile_pad(int in, int out, long long len)
{
  len -= xsendfile_len(in, out, len);
  if (len) {
    perror_msg("short read");
    memset(libbuf, 0, sizeof(libbuf));
    while (len) {
      int i = len>sizeof(libbuf) ? sizeof(libbuf) : len;

      xwrite(out, libbuf, i);
      len -= i;
    }
  }
}

// copy all of in to out
long long xsendfile(int in, int out)
{
  return xsendfile_len(in, out, -1);
}

double xstrtod(char *s)
{
  char *end;
  double d;

  errno = 0;
  d = strtod(s, &end);
  if (!errno && *end) errno = E2BIG;
  if (errno) perror_exit("strtod %s", s);

  return d;
}

// parse fractional seconds with optional s/m/h/d suffix
long xparsetime(char *arg, long zeroes, long *fraction)
{
  long l, fr = 0, mask = 1;
  char *end;

  if (*arg != '.' && !isdigit(*arg)) error_exit("Not a number '%s'", arg);
  l = strtoul(arg, &end, 10);
  if (*end == '.') {
    end++;
    while (zeroes--) {
      fr *= 10;
      mask *= 10;
      if (isdigit(*end)) fr += *end++-'0';
    }
    while (isdigit(*end)) end++;
  }

  // Parse suffix
  if (*end) {
    int ismhd[]={1,60,3600,86400}, i = stridx("smhd", *end);

    if (i == -1 || *(end+1)) error_exit("Unknown suffix '%s'", end);
    l *= ismhd[i];
    fr *= ismhd[i];
    l += fr/mask;
    fr %= mask;
  }
  if (fraction) *fraction = fr;

  return l;
}

long long xparsemillitime(char *arg)
{
  long l, ll;

  l = xparsetime(arg, 3, &ll);

  return (l*1000LL)+ll;
}

void xparsetimespec(char *arg, struct timespec *ts)
{
  ts->tv_sec = xparsetime(arg, 9, &ts->tv_nsec);
}


// Compile a regular expression into a regex_t
void xregcomp(regex_t *preg, char *regex, int cflags)
{
  int rc;

  // BSD regex implementations don't support the empty regex (which isn't
  // allowed in the POSIX grammar), but glibc does. Fake it for BSD.
  if (!*regex) {
    regex = "()";
    cflags |= REG_EXTENDED;
  }

  if ((rc = regcomp(preg, regex, cflags))) {
    regerror(rc, preg, libbuf, sizeof(libbuf));
    error_exit("bad regex '%s': %s", regex, libbuf);
  }
}

char *xtzset(char *new)
{
  char *old = getenv("TZ");

  if (old) old = xstrdup(old);
  if (new ? setenv("TZ", new, 1) : unsetenv("TZ")) perror_exit("setenv");
  tzset();

  return old;
}

// Set a signal handler
void xsignal_flags(int signal, void *handler, int flags)
{
  struct sigaction *sa = (void *)libbuf;

  memset(sa, 0, sizeof(struct sigaction));
  sa->sa_handler = handler;
  sa->sa_flags = flags;

  if (sigaction(signal, sa, 0)) perror_exit("xsignal %d", signal);
}

void xsignal(int signal, void *handler)
{
  xsignal_flags(signal, handler, 0);
}


time_t xvali_date(struct tm *tm, char *str)
{
  time_t t;

  if (tm && (unsigned)tm->tm_sec<=60 && (unsigned)tm->tm_min<=59
     && (unsigned)tm->tm_hour<=23 && tm->tm_mday && (unsigned)tm->tm_mday<=31
     && (unsigned)tm->tm_mon<=11 && (t = mktime(tm)) != -1) return t;

  error_exit("bad date %s", str);
}

// Parse date string (relative to current *t). Sets time_t and nanoseconds.
void xparsedate(char *str, time_t *t, unsigned *nano, int endian)
{
  struct tm tm;
  time_t now = *t;
  int len = 0, i = 0;
  long long ll;
  // Formats with seconds come first. Posix can't agree on whether 12 digits
  // has year before (touch -t) or year after (date), so support both.
  char *s = str, *p, *oldtz = 0, *formats[] = {"%Y-%m-%d %T", "%Y-%m-%dT%T",
    "%a %b %e %H:%M:%S %Z %Y", // date(1) output format in POSIX/C locale.
    "%H:%M:%S", "%Y-%m-%d %H:%M", "%Y-%m-%d", "%H:%M", "%m%d%H%M",
    endian ? "%m%d%H%M%y" : "%y%m%d%H%M",
    endian ? "%m%d%H%M%C%y" : "%C%y%m%d%H%M"};

  *nano = 0;

  // Parse @UNIXTIME[.FRACTION]
  if (1 == sscanf(s, "@%lld%n", &ll, &len)) {
    if (*(s+=len)=='.') for (len = 0, s++; len<9; len++) {
      *nano *= 10;
      if (isdigit(*s)) *nano += *s++-'0';
    }
    // Can't be sure t is 64 bit (yet) for %lld above
    *t = ll;
    if (!*s) return;
    xvali_date(0, str);
  }

  // Try each format
  for (i = 0; i<ARRAY_LEN(formats); i++) {
    localtime_r(&now, &tm);
    tm.tm_hour = tm.tm_min = tm.tm_sec = 0;
    tm.tm_isdst = -endian;

    if ((p = strptime(s, formats[i], &tm))) {
      // Handle optional fractional seconds.
      if (*p == '.') {
        p++;
        // If format didn't already specify seconds, grab seconds
        if (i>2) {
          len = 0;
          sscanf(p, "%2u%n", &tm.tm_sec, &len);
          p += len;
        }
        // nanoseconds
        for (len = 0; len<9; len++) {
          *nano *= 10;
          if (isdigit(*p)) *nano += *p++-'0';
        }
      }

      // Handle optional Z or +HH[[:]MM] timezone
      while (isspace(*p)) p++;
      if (*p && strchr("Z+-", *p)) {
        unsigned uu[3] = {0}, n = 0, nn = 0;
        char *tz = 0, sign = *p++;

        if (sign == 'Z') tz = "UTC0";
        else if (0<sscanf(p, " %u%n : %u%n : %u%n", uu,&n,uu+1,&nn,uu+2,&nn)) {
          if (n>2) {
            uu[1] += uu[0]%100;
            uu[0] /= 100;
          }
          if (n>nn) nn = n;
          if (!nn) continue;

          // flip sign because POSIX UTC offsets are backwards
          sprintf(tz = libbuf, "UTC%c%02u:%02u:%02u", "+-"[sign=='+'],
            uu[0], uu[1], uu[2]);
          p += nn;
        }

        if (!oldtz) {
          oldtz = getenv("TZ");
          if (oldtz) oldtz = xstrdup(oldtz);
        }
        if (tz) setenv("TZ", tz, 1);
      }
      while (isspace(*p)) p++;

      if (!*p) break;
    }
  }

  // Sanity check field ranges
  *t = xvali_date((i!=ARRAY_LEN(formats)) ? &tm : 0, str);

  if (oldtz) setenv("TZ", oldtz, 1);
  free(oldtz);
}

// Return line of text from file.
char *xgetdelim(FILE *fp, int delim)
{
  char *new = 0;
  size_t len = 0;
  long ll;

  errno = 0;
  if (1>(ll = getdelim(&new, &len, delim, fp))) {
    if (errno && errno != EINTR) perror_msg("getline");
    free(new);
    new = 0;
  }

  return new;
}

// Return line of text from file. Strips trailing newline (if any).
char *xgetline(FILE *fp)
{
  return chomp(xgetdelim(fp, '\n'));
}

time_t xmktime(struct tm *tm, int utc)
{
  char *old_tz = utc ? xtzset("UTC0") : 0;
  time_t result;

  if ((result = mktime(tm)) < 0) error_exit("mktime");
  if (utc) {
    free(xtzset(old_tz));
    free(old_tz);
  }
  return result;
}
