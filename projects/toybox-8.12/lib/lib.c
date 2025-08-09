/* lib.c - various reusable stuff.
 *
 * Copyright 2006 Rob Landley <rob@landley.net>
 */

#define SYSLOG_NAMES
#include "toys.h"

void verror_msg(char *msg, int err, va_list va)
{
  char *s = ": %s";

  // Exit silently in a pipeline
  if (err != EPIPE) {
    fprintf(stderr, "%s: ", toys.which->name);
    if (msg) vfprintf(stderr, msg, va);
    else s+=2;
    if (err>0) fprintf(stderr, s, strerror(err));
    if (err<0 && CFG_TOYBOX_HELP)
      fprintf(stderr, " (see \"%s --help\")", toys.which->name);
    if (msg || err) putc('\n', stderr);
  }
  if (!toys.exitval) toys.exitval = (toys.which->flags>>24) ? : 1;
}

// These functions don't collapse together because of the va_stuff.

void error_msg(char *msg, ...)
{
  va_list va;

  va_start(va, msg);
  verror_msg(msg, 0, va);
  va_end(va);
}

void perror_msg(char *msg, ...)
{
  va_list va;

  va_start(va, msg);
  verror_msg(msg, errno, va);
  va_end(va);
}

// Die with an error message.
void error_exit(char *msg, ...)
{
  va_list va;

  va_start(va, msg);
  verror_msg(msg, 0, va);
  va_end(va);

  xexit();
}

// Die with an error message and strerror(errno)
void perror_exit(char *msg, ...)
{
  va_list va;

  va_start(va, msg);
  verror_msg(msg, errno, va);
  va_end(va);

  xexit();
}

// Exit with an error message after showing help text.
void help_exit(char *msg, ...)
{
  va_list va;

  if (!msg) show_help(stdout, 1);
  else {
    va_start(va, msg);
    verror_msg(msg, -1, va);
    va_end(va);
  }

  xexit();
}

// If you want to explicitly disable the printf() behavior (because you're
// printing user-supplied data, or because android's static checker produces
// false positives for 'char *s = x ? "blah1" : "blah2"; printf(s);' and it's
// -Werror there for policy reasons).
void error_msg_raw(char *msg)
{
  error_msg("%s", msg);
}

void perror_msg_raw(char *msg)
{
  perror_msg("%s", msg);
}

void error_exit_raw(char *msg)
{
  error_exit("%s", msg);
}

void perror_exit_raw(char *msg)
{
  perror_exit("%s", msg);
}

// Keep reading until full or EOF. Note: assumes sigaction(SA_RESTART),
// otherwise SIGSTOP/SIGCONT can return 0 from read/write without EOF.
ssize_t readall(int fd, void *buf, size_t len)
{
  size_t count = 0;

  while (count<len) {
    int i = read(fd, (char *)buf+count, len-count);
    if (!i) break;
    if (i<0) return i;
    count += i;
  }

  return count;
}

// Keep writing until done or EOF
ssize_t writeall(int fd, void *buf, size_t len)
{
  size_t count = 0;

  while (count<len) {
    int i = write(fd, count+(char *)buf, len-count);
    if (i<1) return i;
    count += i;
  }

  return count;
}

// skip this many bytes of input. Return 0 for success, >0 means this much
// left after input skipped.
off_t lskip(int fd, off_t offset)
{
  off_t cur = lseek(fd, 0, SEEK_CUR);

  if (cur != -1) {
    off_t end = lseek(fd, 0, SEEK_END) - cur;

    if (end > 0 && end < offset) return offset - end;
    end = offset+cur;
    if (end == lseek(fd, end, SEEK_SET)) return 0;
    perror_exit("lseek");
  }

  while (offset>0) {
    int try = offset>sizeof(libbuf) ? sizeof(libbuf) : offset, or;

    or = readall(fd, libbuf, try);
    if (or < 0) perror_exit("lskip to %lld", (long long)offset);
    else offset -= or;
    if (or < try) break;
  }

  return offset;
}

// flags:
// MKPATHAT_MKLAST  make last dir (with mode lastmode, else skips last part)
// MKPATHAT_MAKE    make leading dirs (it's ok if they already exist)
// MKPATHAT_VERBOSE Print what got created to stderr
// returns 0 = path ok, 1 = error
int mkpathat(int atfd, char *dir, mode_t lastmode, int flags)
{
  struct stat buf;
  char *s;

  // mkdir -p one/two/three is not an error if the path already exists,
  // but is if "three" is a file. The others we dereference and catch
  // not-a-directory along the way, but the last one we must explicitly
  // test for. Might as well do it up front.

  if (!fstatat(atfd, dir, &buf, 0)) {
    // Note that mkdir should return EEXIST for already existed directory/file.
    if (!(flags&MKPATHAT_MAKE) || ((flags&MKPATHAT_MKLAST) && !S_ISDIR(buf.st_mode))) {
      errno = EEXIST;
      return 1;
    } else return 0;
  }

  for (s = dir; ;s++) {
    char save = 0;
    mode_t mode = (0777&~toys.old_umask)|0300;

    // find next '/', but don't try to mkdir "" at start of absolute path
    if (*s == '/' && (flags&MKPATHAT_MAKE) && s != dir) {
      save = *s;
      *s = 0;
    } else if (*s) continue;

    // Use the mode from the -m option only for the last directory.
    if (!save) {
      if (flags&MKPATHAT_MKLAST) mode = lastmode;
      else break;
    }

    if (mkdirat(atfd, dir, mode)) {
      if (!(flags&MKPATHAT_MAKE) || errno != EEXIST) return 1;
    } else if (flags&MKPATHAT_VERBOSE)
      fprintf(stderr, "%s: created directory '%s'\n", toys.which->name, dir);

    if (!(*s = save)) break;
  }

  return 0;
}

// The common case
int mkpath(char *dir)
{
  return mkpathat(AT_FDCWD, dir, 0, MKPATHAT_MAKE);
}

// Split a path into linked list of components, tracking head and tail of list.
// Assigns head of list to *list, returns address of ->next entry to extend list
// Filters out // entries with no contents.
struct string_list **splitpath(char *path, struct string_list **list)
{
  char *new = path;

  *list = 0;
  do {
    int len;

    if (*path && *path != '/') continue;
    len = path-new;
    if (len > 0) {
      *list = xmalloc(sizeof(struct string_list) + len + 1);
      (*list)->next = 0;
      memcpy((*list)->str, new, len);
      (*list)->str[len] = 0;
      list = &(*list)->next;
    }
    new = path+1;
  } while (*path++);

  return list;
}

// Find all file in a colon-separated path with access type "type" (generally
// X_OK or R_OK).  Returns a list of absolute paths to each file found, in
// order.

struct string_list *find_in_path(char *path, char *filename)
{
  struct string_list *rlist = NULL, **prlist=&rlist;
  char *cwd;

  if (!path) return 0;

  cwd = xgetcwd();
  for (;;) {
    char *res, *next = strchr(path, ':');
    int len = next ? next-path : strlen(path);
    struct string_list *rnext;
    struct stat st;

    rnext = xmalloc(sizeof(void *) + strlen(filename)
      + (len ? len : strlen(cwd)) + 2);
    if (!len) sprintf(rnext->str, "%s/%s", cwd, filename);
    else {
      memcpy(res = rnext->str, path, len);
      res += len;
      *(res++) = '/';
      strcpy(res, filename);
    }

    // Confirm it's not a directory.
    if (!stat(rnext->str, &st) && S_ISREG(st.st_mode)) {
      *prlist = rnext;
      rnext->next = NULL;
      prlist = &(rnext->next);
    } else free(rnext);

    if (!next) break;
    path += len;
    path++;
  }
  free(cwd);

  return rlist;
}

long long estrtol(char *str, char **end, int base)
{
  errno = 0;

  return strtoll(str, end, base);
}

long long xstrtol(char *str, char **end, int base)
{
  long long l = estrtol(str, end, base);

  if (errno) perror_exit_raw(str);

  return l;
}

// atol() with the kilo/mega/giga/tera/peta/exa extensions, plus word and block.
// (zetta and yotta don't fit in 64 bits.)
long long atolx(char *numstr)
{
  char *c = numstr, *suffixes="cwbkmgtpe", *end;
  long long val;

  val = xstrtol(numstr, &c, 0);
  if (c != numstr && *c && (end = strchr(suffixes, tolower(*c)))) {
    int shift = end-suffixes-2;
    ++c;
    if (shift==-1) val *= 2;
    else if (!shift) val *= 512;
    else if (shift>0) {
      if (*c && tolower(*c)=='d') {
        c++;
        while (shift--) val *= 1000;
      } else val *= 1LL<<(shift*10);
    }
  }
  while (isspace(*c)) c++;
  if (c==numstr || *c) error_exit("not integer: %s", numstr);

  return val;
}

long long atolx_range(char *numstr, long long low, long long high)
{
  long long val = atolx(numstr);

  if (val < low) error_exit("%lld < %lld", val, low);
  if (val > high) error_exit("%lld > %lld", val, high);

  return val;
}

int stridx(char *haystack, char needle)
{
  char *off;

  if (!needle) return -1;
  off = strchr(haystack, needle);
  if (!off) return -1;

  return off-haystack;
}

// Convert wc to utf8, returning bytes written. Does not null terminate.
int wctoutf8(char *s, unsigned wc)
{
  int len = (wc>0x7ff)+(wc>0xffff), i;

  if (wc<128) {
    *s = wc;
    return 1;
  } else {
    i = len;
    do {
      s[1+i] = 0x80+(wc&0x3f);
      wc >>= 6;
    } while (i--);
    *s = (((signed char) 0x80) >> (len+1)) | wc;
  }

  return 2+len;
}

// Convert utf8 sequence to a unicode wide character
// returns bytes consumed, or -1 if err, or -2 if need more data.
int utf8towc(unsigned *wc, char *str, unsigned len)
{
  unsigned result, mask, first;
  char *s, c;

  // fast path ASCII
  if (len && *str<128) return !!(*wc = *str);

  result = first = *(s = str++);
  if (result<0xc2 || result>0xf4) return -1;
  for (mask = 6; (first&0xc0)==0xc0; mask += 5, first <<= 1) {
    if (!--len) return -2;
    if (((c = *(str++))&0xc0) != 0x80) return -1;
    result = (result<<6)|(c&0x3f);
  }
  result &= (1<<mask)-1;
  c = str-s;

  // Avoid overlong encodings
  if (result<(unsigned []){0x80,0x800,0x10000}[c-2]) return -1;

  // Limit unicode so it can't encode anything UTF-16 can't.
  if (result>0x10ffff || (result>=0xd800 && result<=0xdfff)) return -1;
  *wc = result;

  return str-s;
}

// Convert string to lower case, utf8 aware.
char *strlower(char *s)
{
  char *try, *new;
  int len, mlen = (strlen(s)|7)+9;
  unsigned c;

  try = new = xmalloc(mlen);

  while (*s) {

    if (1>(len = utf8towc(&c, s, MB_CUR_MAX))) {
      *(new++) = *(s++);

      continue;
    }

    s += len;
    // squash title case too
    c = towlower(c);

    // if we had a valid utf8 sequence, convert it to lower case, and can't
    // encode back to utf8, something is wrong with your libc. But just
    // in case somebody finds an exploit...
    len = wcrtomb(new, c, 0);
    if (len < 1) error_exit("bad utf8 %x", (int)c);
    new += len;

    // Case conversion can expand utf8 representation, but with extra mlen
    // space above we should basically never need to realloc
    if (mlen > (len = new-try)+4) continue;
    try = xrealloc(try, mlen = len+16);
    new = try+len;
  }
  *new = 0;

  return try;
}

// strstr but returns pointer after match
char *strafter(char *haystack, char *needle)
{
  char *s = strstr(haystack, needle);

  return s ? s+strlen(needle) : s;
}

// Remove trailing \n
char *chomp(char *s)
{
  char *p;

  if (s) for (p = s+strlen(s); p>s && (p[-1]=='\r' || p[-1]=='\n'); *--p = 0);

  return s;
}

int unescape(char c)
{
  char *from = "\\abefnrtv", *to = "\\\a\b\e\f\n\r\t\v";
  int idx = stridx(from, c);

  return (idx == -1) ? 0 : to[idx];
}

// parse next character advancing pointer. echo requires leading 0 in octal esc
int unescape2(char **c, int echo)
{
  int idx = *((*c)++), i, off;

  if (idx != '\\' || !**c) return idx;
  if (**c == 'c') return 31&*(++*c);
  for (i = 0; i<4; i++) {
    if (sscanf(*c, (char *[]){"0%3o%n"+!echo, "x%2x%n", "u%4x%n", "U%6x%n"}[i],
        &idx, &off) > 0)
    {
      *c += off;

      return idx;
    }
  }

  if (-1 == (idx = stridx("\\abeEfnrtv'\"?0", **c))) return '\\';
  ++*c;

  return "\\\a\b\e\e\f\n\r\t\v'\"?"[idx];
}

// If string ends with suffix return pointer to start of suffix in string,
// else NULL
char *strend(char *str, char *suffix)
{
  long a = strlen(str), b = strlen(suffix);

  if (a>b && !strcmp(str += a-b, suffix)) return str;

  return 0;
}

// If *a starts with b, advance *a past it and return 1, else return 0;
int strstart(char **a, char *b)
{
  char *c = *a;

  while (*b && *c == *b) b++, c++;
  if (!*b) *a = c;

  return !*b;
}

// If *a starts with b, advance *a past it and return 1, else return 0;
int strcasestart(char **a, char *b)
{
  int len = strlen(b), i = !strncasecmp(*a, b, len);

  if (i) *a += len;

  return i;
}

// return length of match found at this point (try is null terminated array)
int anystart(char *s, char **try)
{
  char *ss = s;

  while (*try) if (strstart(&s, *try++)) return s-ss;

  return 0;
}

// does this entire string match one of the strings in try[]?
// Returns 0 if not, index+1 if so
int anystr(char *s, char **try)
{
  char **and = try;

  while (*try) if (!strcmp(s, *try++)) return try-and;

  return 0;
}

int same_file(struct stat *st1, struct stat *st2)
{
  return st1->st_ino==st2->st_ino && st1->st_dev==st2->st_dev;
}

int same_dev_ino(struct stat *st, struct dev_ino *di)
{
  return st->st_ino==di->ino && st->st_dev==di->dev;
}



// Return how long the file at fd is, if there's any way to determine it.
off_t fdlength(int fd)
{
  struct stat st;
  off_t base = 0, range = 1, expand = 1, old;
  unsigned long long size;

  if (!fstat(fd, &st) && S_ISREG(st.st_mode)) return st.st_size;

  // If the ioctl works for this, return it.
  if (get_block_device_size(fd, &size)) return size;

  // If not, do a binary search for the last location we can read.  (Some
  // block devices don't do BLKGETSIZE right.)  This should probably have
  // a CONFIG option...
  old = lseek(fd, 0, SEEK_CUR);
  do {
    char temp;
    off_t pos = base + range / 2;

    if (lseek(fd, pos, 0)>=0 && read(fd, &temp, 1)==1) {
      off_t delta = (pos + 1) - base;

      base += delta;
      if (expand) range = (expand <<= 1) - base;
      else range -= delta;
    } else {
      expand = 0;
      range = pos - base;
    }
  } while (range > 0);

  lseek(fd, old, SEEK_SET);

  return base;
}

char *readfd(int fd, char *ibuf, off_t *plen)
{
  off_t len, rlen;
  char *buf, *rbuf;

  // Unsafe to probe for size with a supplied buffer, don't ever do that.
  if (CFG_TOYBOX_DEBUG && (ibuf ? !*plen : *plen)) error_exit("bad readfileat");

  // If we dunno the length, probe it. If we can't probe, start with 1 page.
  if (!*plen) {
    if ((len = fdlength(fd))>0) *plen = len;
    else len = 4096;
  } else len = *plen-1;

  if (!ibuf) buf = xmalloc(len+1);
  else buf = ibuf;

  for (rbuf = buf;;) {
    rlen = readall(fd, rbuf, len);
    if (*plen || rlen<len) break;

    // If reading unknown size, expand buffer by 1.5 each time we fill it up.
    rlen += rbuf-buf;
    buf = xrealloc(buf, len = (rlen*3)/2);
    rbuf = buf+rlen;
    len -= rlen;
  }
  *plen = len = rlen+(rbuf-buf);

  if (rlen<0) {
    if (ibuf != buf) free(buf);
    buf = 0;
  } else buf[len] = 0;

  return buf;
}

// Read contents of file as a single nul-terminated string.
// measure file size if !len, allocate buffer if !buf
// Existing buffers need len in *plen
// Returns amount of data read in *plen
char *readfileat(int dirfd, char *name, char *ibuf, off_t *plen)
{
  if (-1 == (dirfd = openat(dirfd, name, O_RDONLY))) return 0;

  ibuf = readfd(dirfd, ibuf, plen);
  close(dirfd);

  return ibuf;
}

char *readfile(char *name, char *ibuf, off_t len)
{
  return readfileat(AT_FDCWD, name, ibuf, &len);
}

// Sleep for this many thousandths of a second
void msleep(long milliseconds)
{
  struct timespec ts;

  ts.tv_sec = milliseconds/1000;
  ts.tv_nsec = (milliseconds%1000)*1000000;
  nanosleep(&ts, &ts);
}

// Adjust timespec by nanosecond offset
void nanomove(struct timespec *ts, long long offset)
{
  long long nano = ts->tv_nsec + offset, secs = nano/1000000000;

  ts->tv_sec += secs;
  nano %= 1000000000;
  if (nano<0) {
    ts->tv_sec--;
    nano += 1000000000;
  }
  ts->tv_nsec = nano;
}

// return difference between two timespecs in nanosecs
long long nanodiff(struct timespec *old, struct timespec *new)
{
  return (new->tv_sec - old->tv_sec)*1000000000LL+(new->tv_nsec - old->tv_nsec);
}

// return 1<<x of highest bit set
int highest_bit(unsigned long l)
{
  int i;

  for (i = 0; l; i++) l >>= 1;

  return i-1;
}

// Inefficient, but deals with unaligned access
long long peek_le(void *ptr, unsigned size)
{
  long long ret = 0;
  char *c = ptr;
  int i;

  for (i=0; i<size; i++) ret |= ((long long)c[i])<<(i*8);
  return ret;
}

long long peek_be(void *ptr, unsigned size)
{
  long long ret = 0;
  char *c = ptr;
  int i;

  for (i=0; i<size; i++) ret = (ret<<8)|(c[i]&0xff);
  return ret;
}

long long peek(void *ptr, unsigned size)
{
  return (IS_BIG_ENDIAN ? peek_be : peek_le)(ptr, size);
}

void poke_le(void *ptr, long long val, unsigned size)
{
  char *c = ptr;

  while (size--) {
    *c++ = val&255;
    val >>= 8;
  }
}

void poke_be(void *ptr, long long val, unsigned size)
{
  char *c = ptr + size;

  while (size--) {
    *--c = val&255;
    val >>=8;
  }
}

void poke(void *ptr, long long val, unsigned size)
{
  (IS_BIG_ENDIAN ? poke_be : poke_le)(ptr, val, size);
}

// Iterate through an array of files, opening each one and calling a function
// on that filehandle and name. The special filename "-" means stdin if
// flags is O_RDONLY, stdout otherwise. An empty argument list calls
// function() on just stdin/stdout.
//
// Note: pass O_CLOEXEC to automatically close filehandles when function()
// returns, otherwise filehandles must be closed by function().
// pass WARN_ONLY to produce warning messages about files it couldn't
// open/create, and skip them. Otherwise function is called with fd -1.
void loopfiles_rw(char **argv, int flags, int permissions,
  void (*function)(int fd, char *name))
{
  int fd, failok = !(flags&WARN_ONLY), anyway = flags & LOOPFILES_ANYWAY;

  flags &= ~(WARN_ONLY|LOOPFILES_ANYWAY);

  // If no arguments, read from stdin.
  if (!*argv) function((flags & O_ACCMODE) != O_RDONLY ? 1 : 0, "-");
  else do {
    // Filename "-" means read from stdin.
    // Inability to open a file prints a warning, but doesn't exit.

    if (!strcmp(*argv, "-")) fd = 0;
    else if (0>(fd = xnotstdio(open(*argv, flags, permissions))) && !failok) {
      perror_msg_raw(*argv);
      if (!anyway) continue;
    }
    function(fd, *argv);
    if ((flags & O_CLOEXEC) && fd>0) close(fd);
  } while (*++argv);
}

// Call loopfiles_rw with O_RDONLY|O_CLOEXEC|WARN_ONLY (common case)
void loopfiles(char **argv, void (*function)(int fd, char *name))
{
  loopfiles_rw(argv, O_RDONLY|O_CLOEXEC|WARN_ONLY, 0, function);
}

// glue to call do_lines() from loopfiles
static void (*do_lines_bridge)(char **pline, long len);
static void loopfile_lines_bridge(int fd, char *name)
{
  do_lines(fd, '\n', do_lines_bridge);
}

void loopfiles_lines(char **argv, void (*function)(char **pline, long len))
{
  do_lines_bridge = function;
  // No O_CLOEXEC because we need to call fclose.
  loopfiles_rw(argv, O_RDONLY|WARN_ONLY, 0, loopfile_lines_bridge);
}

int wfchmodat(int fd, char *name, mode_t mode)
{
  int rc = fchmodat(fd, name, mode, 0);

  if (rc) {
    perror_msg("chmod '%s' to %04o", name, mode);
    toys.exitval=1;
  }
  return rc;
}

static char *tempfile2zap;
static void tempfile_handler(void)
{
  if (1 < (long)tempfile2zap) unlink(tempfile2zap);
}

// Open a temporary file to copy an existing file into.
int copy_tempfile(int fdin, char *name, char **tempname)
{
  struct stat statbuf;
  int fd = xtempfile(name, tempname), ignored __attribute__((__unused__));

  // Record tempfile for exit cleanup if interrupted
  if (!tempfile2zap) sigatexit(tempfile_handler);
  tempfile2zap = *tempname;

  // Set permissions of output file.
  if (!fstat(fdin, &statbuf)) fchmod(fd, statbuf.st_mode);

  // We chmod before chown, which strips the suid bit. Caller has to explicitly
  // switch it back on if they want to keep suid.

  // Suppress warn-unused-result. Both gcc and clang clutch their pearls about
  // this but it's _supposed_ to fail when we're not root.
  ignored = fchown(fd, statbuf.st_uid, statbuf.st_gid);

  return fd;
}

// Abort the copy and delete the temporary file.
void delete_tempfile(int fdin, int fdout, char **tempname)
{
  close(fdin);
  close(fdout);
  if (*tempname) unlink(*tempname);
  tempfile2zap = (char *)1;
  free(*tempname);
  *tempname = NULL;
}

// Copy the rest of the data and replace the original with the copy.
void replace_tempfile(int fdin, int fdout, char **tempname)
{
  char *temp = xstrdup(*tempname);

  temp[strlen(temp)-6]=0;
  if (fdin != -1) {
    xsendfile(fdin, fdout);
    xclose(fdin);
  }
  xclose(fdout);
  xrename(*tempname, temp);
  tempfile2zap = (char *)1;
  free(*tempname);
  free(temp);
  *tempname = NULL;
}

// Create a 256 entry CRC32 lookup table.

void crc_init(unsigned *crc_table, int little_endian)
{
  unsigned int i;

  // Init the CRC32 table (big endian)
  for (i=0; i<256; i++) {
    unsigned int j, c = little_endian ? i : i<<24;
    for (j=8; j; j--)
      if (little_endian) c = (c&1) ? (c>>1)^0xEDB88320 : c>>1;
      else c=c&0x80000000 ? (c<<1)^0x04c11db7 : (c<<1);
    crc_table[i] = c;
  }
}

// Init base64 table

void base64_init(char *p)
{
  int i;

  for (i = 'A'; i != ':'; i++) {
    if (i == 'Z'+1) i = 'a';
    if (i == 'z'+1) i = '0';
    *(p++) = i;
  }
  *(p++) = '+';
  *(p++) = '/';
}

int yesno(int def)
{
  return fyesno(stdin, def);
}

int fyesno(FILE *in, int def)
{
  char buf;

  fprintf(stderr, " (%c/%c):", def ? 'Y' : 'y', def ? 'n' : 'N');
  fflush(stderr);
  while (fread(&buf, 1, 1, in)) {
    int new;

    // The letter changes the value, the newline (or space) returns it.
    if (isspace(buf)) break;
    if (-1 != (new = stridx("ny", tolower(buf)))) def = new;
  }

  return def;
}

// Handler that sets toys.signal, and writes to toys.signalfd if set
void generic_signal(int sig)
{
  if (toys.signalfd) {
    char c = sig;

    writeall(toys.signalfd, &c, 1);
  }
  toys.signal = sig;
}

// More or less SIG_DFL that runs our atexit list and can siglongjmp.
void exit_signal(int sig)
{
  sigset_t sigset;

  if (sig) toys.exitval = sig|128;
  sigfillset(&sigset);
  sigprocmask(SIG_BLOCK, &sigset, 0);
  xexit();
}

// Install an atexit handler. Also install the same handler on every signal
// that defaults to killing the process, calling the handler on the way out.
// Calling multiple times adds the handlers to a list, to be called in LIFO
// order.
void sigatexit(void *handler)
{
  struct arg_list *al = 0;

  xsignal_all_killers(handler ? exit_signal : SIG_DFL);
  if (handler) {
    al = xmalloc(sizeof(struct arg_list));
    al->next = toys.xexit;
    al->arg = handler;
  } else llist_traverse(toys.xexit, free);
  toys.xexit = al;
}

// Output a nicely formatted table of all the signals.
void list_signals(void)
{
  int i = 1, count = 0;
  unsigned cols = 80;
  char *name;

  terminal_size(&cols, 0);
  cols /= 16;
  for (; i<=NSIG; i++) {
    if ((name = num_to_sig(i))) {
      printf("%2d) SIG%-9s", i, name);
      if (++count % cols == 0) putchar('\n');
    }
  }
  putchar('\n');
}

// premute mode bits based on posix mode strings.
unsigned string_to_mode(char *modestr, unsigned mode)
{
  char *whos = "ogua", *hows = "=+-", *whats = "xwrstX", *whys = "ogu",
       *s, *str = modestr;
  unsigned extrabits = mode & ~(07777), bit;

  // Handle octal mode
  if (isdigit(*str)) {
    mode = estrtol(str, &s, 8);
    if (errno || *s || (mode & ~(07777))) goto barf;

    return mode | extrabits;
  }

  // Gaze into the bin of permission...
  for (;;) {
    int i, j, dowho = 0, dohow = 0, dowhat, amask = 0;

    // Find the who, how, and what stanzas, in that order
    while (*str && (s = strchr(whos, *str))) {
      dowho |= 1<<(s-whos);
      str++;
    }
    // If who isn't specified, like "a" but honoring umask.
    if (!dowho) {
      dowho = 8;
      umask(amask = umask(0));
    }

    // Repeated "hows" are allowed; something like "a=r+w+s" is valid.
    for (;;) {
      if (-1 == stridx(hows, dohow = *str)) goto barf;
      dowhat = 0;
      while (*++str && (s = strchr(whats, *str))) dowhat |= 1<<(s-whats);

      // Convert X to x for directory or if already executable somewhere
      if ((dowhat&32) && (S_ISDIR(mode) || (mode&0111))) dowhat |= 1;

      // Copy mode from another category?
      if (!dowhat && -1 != (i = stridx(whys, *str))) {
        dowhat = (mode>>(3*i))&7;
        str++;
      }

      // Loop through what=xwrs and who=ogu to apply bits to the mode.
      for (i=0; i<4; i++) {
        for (j=0; j<3; j++) {
          int where = 1<<((3*i)+j);

          if (amask & where) continue;

          // Figure out new value at this location
          bit = 0;
          if (i == 3) {
            // suid and sticky
            if (!j) bit = dowhat&16; // o+s = t but a+s doesn't set t, hence t
            else if ((dowhat&8) && (dowho&(8|(1<<j)))) bit++;
          } else {
            if (!(dowho&(8|(1<<i)))) continue;
            else if (dowhat&(1<<j)) bit++;
          }

          // When selection active, modify bit
          if (dohow == '=' || (bit && dohow == '-')) mode &= ~where;
          if (bit && dohow != '-') mode |= where;
        }
      }
      if (!*str) return mode|extrabits;
      if (*str == ',') {
        str++;
        break;
      }
    }
  }

barf:
  error_exit("bad mode '%s'", modestr);
}

// Format access mode into a drwxrwxrwx string
void mode_to_string(unsigned mode, char *buf)
{
  char c, d;
  int i, bit;

  buf[10]=0;
  for (i=0; i<9; i++) {
    bit = mode & (1<<i);
    c = i%3;
    if (!c && (mode & (1<<((d=i/3)+9)))) {
      c = "tss"[d];
      if (!bit) c &= ~0x20;
    } else c = bit ? "xwr"[c] : '-';
    buf[9-i] = c;
  }

  if (S_ISDIR(mode)) c = 'd';
  else if (S_ISBLK(mode)) c = 'b';
  else if (S_ISCHR(mode)) c = 'c';
  else if (S_ISLNK(mode)) c = 'l';
  else if (S_ISFIFO(mode)) c = 'p';
  else if (S_ISSOCK(mode)) c = 's';
  else c = '-';
  *buf = c;
}

// basename() can modify its argument or return a pointer to a constant string
// This just gives after the last '/' or the whole stirng if no /
char *getbasename(char *name)
{
  char *s = strrchr(name, '/');

  if (s) return s+1;

  return name;
}

// Return pointer to xabspath(file) if file is under dir, else 0
char *fileunderdir(char *file, char *dir)
{
  char *s1 = xabspath(dir, ABS_FILE), *s2 = xabspath(file, 0), *ss = s2;
  int rc = s1 && s2 && strstart(&ss, s1) && (!s1[1] || s2[strlen(s1)] == '/');

  free(s1);
  if (!rc) free(s2);

  return rc ? s2 : 0;
}

void *mepcpy(void *to, void *from, unsigned long len)
{
  memcpy(to, from, len);

  return ((char *)to)+len;
}

// return (malloced) relative path to get between two normalized absolute paths
// normalized: no duplicate / or trailing / or .. or . (symlinks optional)
char *relative_path(char *from, char *to, int abs)
{
  char *s, *ret = 0;
  int i, j, k;

  if (abs) {
    if (!(from = xabspath(from, 0))) return 0;
    if (!(to = xabspath(to, 0))) goto error;
  }

  for (i = j = 0;; i++) {
    if (!from[i] || !to[i]) {
      if (from[i]=='/' || to[i]=='/' || from[i]==to[i]) j = i;
      break;
    }
    if (from[i] != to[i]) break;
    if (from[i] == '/') j = i;
  }

  // count remaining destination directories
  for (i = j, k = 0; from[i]; i++) if (from[i] == '/') k++;
  if (!k) {
    if (to[j]=='/') j++;
    ret = xstrdup(to[j] ? to+j : ".");
  } else {
    s = ret = xmprintf("%*c%s", 3*k-!!k, ' ', to+j);
    for (i = 0; i<k; i++) s = mepcpy(s, "/.."+!i, 3-!i);
  }

error:
  if (abs) {
    free(from);
    free(to);
  }

  return ret;
}

// Execute a callback for each PID that matches a process name from a list.
void names_to_pid(char **names, int (*callback)(pid_t pid, char *name),
    int scripts)
{
  DIR *dp;
  struct dirent *entry;

  if (!(dp = opendir("/proc"))) perror_exit("no /proc");

  while ((entry = readdir(dp))) {
    unsigned u = atoi(entry->d_name);
    char *cmd = 0, *comm = 0, **cur;
    off_t len;

    if (!u) continue;

    // Comm is original name of executable (argv[0] could be #! interpreter)
    // but it's limited to 15 characters
    if (scripts) {
      sprintf(libbuf, "/proc/%u/comm", u);
      len = sizeof(libbuf);
      if (!(comm = readfileat(AT_FDCWD, libbuf, libbuf, &len)) || !len)
        continue;
      if (libbuf[len-1] == '\n') libbuf[--len] = 0;
    }

    for (cur = names; *cur; cur++) {
      struct stat st1, st2;
      char *bb = getbasename(*cur);
      off_t len = strlen(bb);

      // Fast path: only matching a filename (no path) that fits in comm.
      // `len` must be 14 or less because with a full 15 bytes we don't
      // know whether the name fit or was truncated.
      if (scripts && len<=14 && bb==*cur && !strcmp(comm, bb)) goto match;

      // If we have a path to existing file only match if same inode
      if (bb!=*cur && !stat(*cur, &st1)) {
        char buf[32];

        sprintf(buf, "/proc/%u/exe", u);
        if (stat(buf, &st2) || !same_file(&st1, &st2)) continue;
        goto match;
      }

      // Nope, gotta read command line to confirm
      if (!cmd) {
        sprintf(cmd = libbuf+16, "/proc/%u/cmdline", u);
        len = sizeof(libbuf)-17;
        if (!(cmd = readfileat(AT_FDCWD, cmd, cmd, &len))) continue;
        // readfile only guarantees one null terminator and we need two
        // (yes the kernel should do this for us, don't care)
        cmd[len] = 0;
      }
      if (!strcmp(bb, getbasename(cmd))) goto match;
      if (scripts && !strcmp(bb, getbasename(cmd+strlen(cmd)+1))) goto match;
      continue;
match:
      if (callback(u, *cur)) goto done;
    }
  }
done:
  closedir(dp);
}

// display first "dgt" many digits of number plus unit (kilo-exabytes)
int human_readable_long(char *buf, unsigned long long num, int dgt, int unit,
  int style)
{
  unsigned long long snap = 0;
  int len, divisor = (style&HR_1000) ? 1000 : 1024;

  // Divide rounding up until we have 3 or fewer digits. Since the part we
  // print is decimal, the test is 999 even when we divide by 1024.
  // The largest unit we can detect is 1<<64 = 18 Exabytes, but we added
  // Zettabyte and Yottabyte in case "unit" starts above zero.
  for (;;unit++) {
    if ((len = snprintf(0, 0, "%llu", num))<=dgt) break;
    num = ((snap = num)+(divisor/2))/divisor;
  }
  if (CFG_TOYBOX_DEBUG && unit>8) return sprintf(buf, "%.*s", dgt, "TILT");

  len = sprintf(buf, "%llu", num);
  if (!(style & HR_NODOT) && unit && len == 1) {
    // Redo rounding for 1.2M case, this works with and without HR_1000.
    num = snap/divisor;
    snap -= num*divisor;
    snap = ((snap*100)+50)/divisor;
    snap /= 10;
    len = sprintf(buf, "%llu.%llu", num, snap);
  }
  if (style & HR_SPACE) buf[len++] = ' ';
  if (unit) {
    unit = " kMGTPEZY"[unit];

    if (!(style&HR_1000)) unit = toupper(unit);
    buf[len++] = unit;
  } else if (style & HR_B) buf[len++] = 'B';
  buf[len] = 0;

  return len;
}

// Give 3 digit estimate + units ala 999M or 1.7T
int human_readable(char *buf, unsigned long long num, int style)
{
  return human_readable_long(buf, num, 3, 0, style);
}

// The qsort man page says you can use alphasort, the posix committee
// disagreed, and doubled down: http://austingroupbugs.net/view.php?id=142
// So just do our own. (The const is entirely to humor the stupid compiler.)
int qstrcmp(const void *a, const void *b)
{
  return strcmp(*(char **)a, *(char **)b);
}

// See https://tools.ietf.org/html/rfc4122, specifically section 4.4
// "Algorithms for Creating a UUID from Truly Random or Pseudo-Random
// Numbers".
void create_uuid(char *uuid)
{
  // "Set all the ... bits to randomly (or pseudo-randomly) chosen values".
  xgetrandom(uuid, 16);

  // "Set the four most significant bits ... of the time_hi_and_version
  // field to the 4-bit version number [4]".
  uuid[6] = (uuid[6] & 0x0F) | 0x40;
  // "Set the two most significant bits (bits 6 and 7) of
  // clock_seq_hi_and_reserved to zero and one, respectively".
  uuid[8] = (uuid[8] & 0x3F) | 0x80;
}

char *show_uuid(char *uuid)
{
  char *out = libbuf;
  int i;

  for (i=0; i<16; i++) out+=sprintf(out, "-%02x"+!(0x550&(1<<i)), uuid[i]);
  *out = 0;

  return libbuf;
}

// Returns pointer to letter at end, 0 if none. *start = initial %
char *next_printf(char *s, char **start)
{
  for (; *s; s++) {
    if (*s != '%') continue;
    if (*++s == '%') continue;
    if (start) *start = s-1;
    while (0 <= stridx("0'#-+ ", *s)) s++;
    while (isdigit(*s)) s++;
    if (*s == '.') s++;
    while (isdigit(*s)) s++;

    return s;
  }

  return 0;
}

// Return cached passwd entries.
struct passwd *bufgetpwnamuid(char *name, uid_t uid)
{
  struct pwuidbuf_list {
    struct pwuidbuf_list *next;
    struct passwd pw;
  } *list = 0;
  struct passwd *temp;
  static struct pwuidbuf_list *pwuidbuf;
  unsigned size = 256;

  // If we already have this one, return it.
  for (list = pwuidbuf; list; list = list->next)
    if (name ? !strcmp(name, list->pw.pw_name) : list->pw.pw_uid==uid)
      return &(list->pw);

  for (;;) {
    list = xrealloc(list, size *= 2);
    if (name) errno = getpwnam_r(name, &list->pw, sizeof(*list)+(char *)list,
      size-sizeof(*list), &temp);
    else errno = getpwuid_r(uid, &list->pw, sizeof(*list)+(char *)list,
      size-sizeof(*list), &temp);
    if (errno != ERANGE) break;
  }

  if (!temp) {
    free(list);

    return 0;
  }
  list->next = pwuidbuf;
  pwuidbuf = list;

  return &list->pw;
}

struct passwd *bufgetpwuid(uid_t uid)
{
  return bufgetpwnamuid(0, uid);
}

// Return cached group entries.
struct group *bufgetgrnamgid(char *name, gid_t gid)
{
  struct grgidbuf_list {
    struct grgidbuf_list *next;
    struct group gr;
  } *list = 0;
  struct group *temp;
  static struct grgidbuf_list *grgidbuf;
  unsigned size = 256;

  for (list = grgidbuf; list; list = list->next)
    if (name ? !strcmp(name, list->gr.gr_name) : list->gr.gr_gid==gid)
      return &(list->gr);

  for (;;) {
    list = xrealloc(list, size *= 2);
    if (name) errno = getgrnam_r(name, &list->gr, sizeof(*list)+(char *)list,
      size-sizeof(*list), &temp);
    else errno = getgrgid_r(gid, &list->gr, sizeof(*list)+(char *)list,
      size-sizeof(*list), &temp);
    if (errno != ERANGE) break;
  }
  if (!temp) {
    free(list);

    return 0;
  }
  list->next = grgidbuf;
  grgidbuf = list;

  return &list->gr;
}

struct group *bufgetgrgid(gid_t gid)
{
  return bufgetgrnamgid(0, gid);
}


// Always null terminates, returns 0 for failure, len for success
int readlinkat0(int dirfd, char *path, char *buf, int len)
{
  if (!len) return 0;

  len = readlinkat(dirfd, path, buf, len-1);
  if (len<0) len = 0;
  buf[len] = 0;

  return len;
}

int readlink0(char *path, char *buf, int len)
{
  return readlinkat0(AT_FDCWD, path, buf, len);
}

// Do regex matching with len argument to handle embedded NUL bytes in string
int regexec0(regex_t *preg, char *string, long len, int nmatch,
  regmatch_t *pmatch, int eflags)
{
  regmatch_t backup;

  if (!nmatch) pmatch = &backup;
  pmatch->rm_so = 0;
  pmatch->rm_eo = len;
  return regexec(preg, string, nmatch, pmatch, eflags|REG_STARTEND);
}

// Return user name or string representation of number, returned buffer
// lasts until next call.
char *getusername(uid_t uid)
{
  struct passwd *pw = bufgetpwuid(uid);
  static char unum[12];

  sprintf(unum, "%u", (unsigned)uid);
  return pw ? pw->pw_name : unum;
}

// Return group name or string representation of number, returned buffer
// lasts until next call.
char *getgroupname(gid_t gid)
{
  struct group *gr = bufgetgrgid(gid);
  static char gnum[12];

  sprintf(gnum, "%u", (unsigned)gid);
  return gr ? gr->gr_name : gnum;
}

// Iterate over lines in file, calling function. Function can write 0 to
// the line pointer if they want to keep it, or 1 to terminate processing,
// otherwise line is freed. Passed file descriptor is closed at the end.
// At EOF calls function(0, 0)
void do_lines(int fd, char delim, void (*call)(char **pline, long len))
{
  FILE *fp = fd ? xfdopen(fd, "r") : stdin;

  for (;;) {
    char *line = 0;
    ssize_t len;

    len = getdelim(&line, (void *)&len, delim, fp);
    if (len > 0) {
      call(&line, len);
      if (line == (void *)1) break;
      free(line);
    } else break;
  }
  call(0, 0);

  if (fd) fclose(fp);
}

// Return unix time in milliseconds
long long millitime(void)
{
  struct timespec ts;

  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec*1000+ts.tv_nsec/1000000;
}

// Formats `ts` in ISO format ("2018-06-28 15:08:58.846386216 -0700").
char *format_iso_time(char *buf, size_t len, struct timespec *ts)
{
  char *s = buf;

  s += strftime(s, len, "%F %T", localtime(&(ts->tv_sec)));
  s += sprintf(s, ".%09ld ", ts->tv_nsec);
  s += strftime(s, len-strlen(buf), "%z", localtime(&(ts->tv_sec)));

  return buf;
}

// Syslog with the openlog/closelog, autodetecting daemon status via no tty

void loggit(int priority, char *format, ...)
{
  int i, facility = LOG_DAEMON;
  va_list va;

  for (i = 0; i<3; i++) if (isatty(i)) facility = LOG_AUTH;
  openlog(toys.which->name, LOG_PID, facility);
  va_start(va, format);
  vsyslog(priority, format, va);
  va_end(va);
  closelog();
}

// Calculate tar packet checksum, with cksum field treated as 8 spaces
unsigned tar_cksum(void *data)
{
  unsigned i, cksum = 8*' ';

  for (i = 0; i<500; i += (i==147) ? 9 : 1) cksum += ((char *)data)[i];

  return cksum;
}

// is this a valid tar header?
int is_tar_header(void *pkt)
{
  char *p = pkt;
  int i = 0;

  if (p[257] && smemcmp("ustar", p+257, 5)) return 0;
  if (p[148] != '0' && p[148] != ' ') return 0;
  sscanf(p+148, "%8o", &i);

  return i && tar_cksum(pkt) == i;
}

// Remove octal escapes from string (common in kernel exports)
void octal_deslash(char *s)
{
  char *o = s;

  while (*s) {
    if (*s == '\\') {
      int i, oct = 0;

      for (i = 1; i < 4; i++) {
        if (!isdigit(s[i])) break;
        oct = (oct<<3)+s[i]-'0';
      }
      if (i == 4) {
        *o++ = oct;
        s += i;
        continue;
      }
    }
    *o++ = *s++;
  }

  *o = 0;
}

// ASAN flips out about memcmp("a", "abc", 4) but the result is well-defined.
// This one's guaranteed to stop at len _or_ the first difference.
int smemcmp(char *one, char *two, unsigned long len)
{
  int ii = 0;

  // NULL sorts after anything else
  if (one == two) return 0;
  if (!one) return 1;
  if (!two) return -1;

  while (len--) if ((ii = *one++ - *two++)) break;

  return ii;
}

