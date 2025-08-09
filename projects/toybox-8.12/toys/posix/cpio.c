/* cpio.c - a basic cpio
 *
 * Copyright 2013 Isaac Dunham <ibid.ag@gmail.com>
 * Copyright 2015 Frontier Silicon Ltd.
 *
 * see https://www.kernel.org/doc/Documentation/early-userspace/buffer-format.txt
 * and http://refspecs.linuxfoundation.org/LSB_4.1.0/LSB-Core-generic/LSB-Core-generic/cpio.html
 * and http://pubs.opengroup.org/onlinepubs/7908799/xcu/cpio.html
 *
 * Yes, that's SUSv2, newer versions removed it, but RPM and initramfs use
 * this archive format. We implement (only) the modern "-H newc" variant which
 * expanded headers to 110 bytes (first field 6 bytes, rest are 8).
 * In order: magic ino mode uid gid nlink mtime filesize devmajor devminor
 * rdevmajor rdevminor namesize check
 * This is the equivalent of mode -H newc in other implementations.
 * We always do --quiet, but accept it as a compatibility NOP.
 *
 * TODO: export/import linux file list text format ala gen_initramfs_list.sh
 * TODO: hardlink support, -A, -0, -a, -L, --sparse
 * TODO: --renumber-archives (probably always?) --ignore-devno --reproducible

USE_CPIO(NEWTOY(cpio, "(ignore-devno)(renumber-inodes)(quiet)(no-preserve-owner)R(owner):md(make-directories)uLH:p|i|t|F:v(verbose)o|[!pio][!pot][!pF]", TOYFLAG_BIN))

config CPIO
  bool "cpio"
  default y
  help
    usage: cpio -{o|t|i|p DEST} [-dLtuv] [--verbose] [-F FILE] [-R [USER][:GROUP] [--no-preserve-owner]

    Copy files into and out of a "newc" format cpio archive.

    -d	Create directories if needed
    -F FILE	Use archive FILE instead of stdin/stdout
    -i	Extract from archive into file system (stdin=archive)
    -L	Follow symlinks
    -o	Create archive (stdin=list of files, stdout=archive)
    -p DEST	Copy-pass mode, copy stdin file list to directory DEST
    -R USER	Replace owner with USER[:GROUP]
    -t	Test files (list only, stdin=archive, stdout=list of files)
    -u	Unlink existing files when extracting
    -v	Verbose
    --no-preserve-owner     Don't set ownership during extract
*/

#define FOR_cpio
#include "toys.h"

GLOBALS(
  char *F, *H, *R;
)

// Read strings, tail padded to 4 byte alignment. Argument "align" is amount
// by which start of string isn't aligned (usually 0, but header is 110 bytes
// which is 2 bytes off because the first field wasn't expanded from 6 to 8).
static char *strpad(int fd, unsigned len, unsigned align)
{
  char *str;

  align = (align + len) & 3;
  if (align) len += (4-align);
  xreadall(fd, str = xmalloc(len+1), len);
  str[len]=0; // redundant, in case archive is bad

  return str;
}

//convert hex to uint; mostly to allow using bits of non-terminated strings
static unsigned x8u(char *hex)
{
  unsigned val, inpos = 8, outpos;
  char pattern[6];

  while (*hex == '0') {
    hex++;
    if (!--inpos) return 0;
  }
  // Because scanf gratuitously treats %*X differently than printf does.
  sprintf(pattern, "%%%dX%%n", inpos);
  sscanf(hex, pattern, &val, &outpos);
  if (inpos != outpos) error_exit("bad hex");

  return val;
}

void cpio_main(void)
{
  int pipe, afd = FLAG(o), reown = !geteuid() && !FLAG(no_preserve_owner),
      empty = 1;
  pid_t pid = 0;
  long Ruid = -1, Rgid = -1;
  char *tofree = 0;

  if (TT.R) {
    char *group = TT.R+strcspn(TT.R, ":.");

    if (*group) {
      Rgid = xgetgid(group+1);
      *group = 0;
    }
    if (group != TT.R) Ruid = xgetuid(TT.R);
  }

  // In passthrough mode, parent stays in original dir and generates archive
  // to pipe, child does chdir to new dir and reads archive from stdin (pipe).
  if (FLAG(p)) {
    if (FLAG(d)) {
      if (!*toys.optargs) error_exit("need directory for -p");
      if (mkdir(*toys.optargs, 0700) == -1 && errno != EEXIST)
        perror_msg("mkdir %s", *toys.optargs);
    }
    if (toys.stacktop) {
      // xpopen() doesn't return from child due to vfork(), instead restarts
      // with !toys.stacktop
      pid = xpopen(0, &pipe, 0);
      afd = pipe;
    } else {
      // child
      toys.optflags |= FLAG_i;
      xchdir(*toys.optargs);
    }
  }

  if (TT.F) {
    int perm = FLAG(o) ? O_CREAT|O_WRONLY|O_TRUNC : O_RDONLY;

    afd = xcreate(TT.F, perm, 0644);
  }

  // read cpio archive

  if (FLAG(i) || FLAG(t)) for (;; empty = 0) {
    char *name, *data;
    unsigned mode, uid, gid, timestamp;
    int test = FLAG(t), err = 0, size = 0, len;

    free(tofree);
    tofree = 0;
    // read header, skipping arbitrary leading NUL bytes (concatenated archives)
    for (;;) {
      if (1>(len = readall(afd, toybuf+size, 110-size))) break;
      if (size || *toybuf) {
        size += len;
        break;
      }
      for (size = 0; size<len; size++) if (toybuf[size]) break;
      memmove(toybuf, toybuf+size, len-size);
      size = len-size;
    }
    if (!size) {
      if (empty) error_exit("empty archive");
      else break;
    }
    if (size != 110 || smemcmp(toybuf, "070701", 6)) error_exit("bad header");
    tofree = name = strpad(afd, x8u(toybuf+94), 110);
    // TODO: this flushes hardlink detection via major/minor/ino match
    if (!strcmp("TRAILER!!!", name)) continue;

    // If you want to extract absolute paths, "cd /" and run cpio.
    while (*name == '/') name++;
    // TODO: remove .. entries

    size = x8u(toybuf+54);
    mode = x8u(toybuf+14);
    uid = (Ruid>=0) ? Ruid : x8u(toybuf+22);
    gid = (Rgid>=0) ? Rgid : x8u(toybuf+30);
    timestamp = x8u(toybuf+46); // unsigned 32 bit, so year 2100 problem

    // (This output is unaffected by --quiet.)
    if (FLAG(t) || FLAG(v)) puts(name);

    if (FLAG(u) && !test) if (unlink(name) && errno == EISDIR) rmdir(name);

    if (!test && FLAG(d) && strrchr(name, '/') && mkpath(name)) {
      perror_msg("mkpath '%s'", name);
      test++;
    }

    // Consume entire record even if it couldn't create file, so we're
    // properly aligned with next file.

    if (S_ISDIR(mode)) {
      if (test) continue;
      err = mkdir(name, mode) && (errno != EEXIST && !FLAG(u));

      // Creading dir/dev doesn't give us a filehandle, we have to refer to it
      // by name to chown/utime, but how do we know it's the same item?
      // Check that we at least have the right type of entity open, and do
      // NOT restore dropped suid bit in this case.
      if (S_ISDIR(mode) && reown) {
        int fd = open(name, O_RDONLY|O_NOFOLLOW);
        struct stat st;

        if (fd != -1 && !fstat(fd, &st) && (st.st_mode&S_IFMT) == (mode&S_IFMT))
          err = fchown(fd, uid, gid);
        else err = 1;

        close(fd);
      }
    } else if (S_ISREG(mode)) {
      int fd = test ? 0 : open(name, O_CREAT|O_WRONLY|O_EXCL|O_NOFOLLOW, mode);

      // If write fails, we still need to read/discard data to continue with
      // archive. Since doing so overwrites errno, report error now
      if (fd < 0) {
        perror_msg("create %s", name);
        test++;
      }

      data = toybuf;
      while (size) {
        if (size < sizeof(toybuf)) data = strpad(afd, size, 0);
        else xreadall(afd, toybuf, sizeof(toybuf));
        if (!test) xwrite(fd, data, data == toybuf ? sizeof(toybuf) : size);
        if (data != toybuf) {
          free(data);
          break;
        }
        size -= sizeof(toybuf);
      }

      if (!test) {
        // set owner, restore dropped suid bit
        if (reown) err = fchown(fd, uid, gid) && fchmod(fd, mode);
        close(fd);
      }
    } else {
      data = S_ISLNK(mode) ? strpad(afd, size, 0) : 0;
      if (!test) {
        err = data ? symlink(data, name)
          : mknod(name, mode, dev_makedev(x8u(toybuf+78), x8u(toybuf+86)));

        // Can't get a filehandle to a symlink or a node on nodev mount,
        // so do special chown that at least doesn't follow symlinks.
        // We also don't chmod after, so dropped suid bit isn't restored
        if (!err && reown) err = lchown(name, uid, gid);
      }
      free(data);
    }

    // Set timestamp.
    if (!test && !err) {
      struct timespec times[2];

      memset(times, 0, sizeof(struct timespec)*2);
      times[0].tv_sec = times[1].tv_sec = timestamp;
      err = utimensat(AT_FDCWD, name, times, AT_SYMLINK_NOFOLLOW);
    }

    if (err) perror_msg_raw(name);

  // Output cpio archive

  } else {
    char *name = 0;
    size_t size = 0;
    unsigned inode = 0;

    for (;;) {
      struct stat st;
      unsigned nlen, error = 0, zero = 0;
      int len, fd = -1;
      char *link = 0;
      ssize_t llen;

      len = getline(&name, &size, stdin);
      if (len<1) break;
      if (name[len-1] == '\n') name[--len] = 0;
      if (!len) continue;
      nlen = len+1;
      if ((FLAG(L)?stat:lstat)(name, &st) || (S_ISREG(st.st_mode)
          && st.st_size && (fd = open(name, O_RDONLY))<0)
          || (S_ISLNK(st.st_mode) && !(link = xreadlink(name))))
      {
        perror_msg_raw(name);
        continue;
      }
      // encrypted filesystems can stat the wrong link size
      if (link) st.st_size = strlen(link);

      if (Ruid>=0) st.st_uid = Ruid;
      if (Rgid>=0) st.st_gid = Rgid;
      if (FLAG(no_preserve_owner)) st.st_uid = st.st_gid = 0;
      if (!S_ISREG(st.st_mode) && !S_ISLNK(st.st_mode)) st.st_size = 0;
      if (st.st_size >> 32) perror_msg("skipping >2G file '%s'", name);
      else {
        if (FLAG(renumber_inodes)) st.st_ino = ++inode;
        if (FLAG(ignore_devno)) st.st_rdev = 0;
        llen = sprintf(toybuf,
          "070701%08X%08X%08X%08X%08X%08X%08X%08X%08X%08X%08X%08X%08X",
          (int)st.st_ino, st.st_mode, st.st_uid, st.st_gid, (int)st.st_nlink,
          (int)st.st_mtime, (int)st.st_size, dev_major(st.st_dev),
          dev_minor(st.st_dev), dev_major(st.st_rdev), dev_minor(st.st_rdev),
          nlen, 0);
        xwrite(afd, toybuf, llen);
        xwrite(afd, name, nlen);

        // NUL Pad header up to 4 multiple bytes.
        llen = (llen + nlen) & 3;
        if (llen) xwrite(afd, &zero, 4-llen);

        // Write out body for symlink or regular file
        if (link) xwrite(afd, link, st.st_size);
        else for (llen = st.st_size; llen; llen -= nlen) {
          nlen = llen > sizeof(toybuf) ? sizeof(toybuf) : llen;
          // If read fails, write anyway (already wrote size in header)
          if (nlen != readall(fd, toybuf, nlen))
            if (!error++) perror_msg("bad read from file '%s'", name);
          xwrite(afd, toybuf, nlen);
        }
        llen = st.st_size & 3;
        if (llen) xwrite(afd, &zero, 4-llen);
      }
      free(link);
      xclose(fd);
    }
    if (CFG_TOYBOX_FREE) free(name);

    // nlink=1, namesize=11, with padding
    dprintf(afd, "070701%040X%056X%08XTRAILER!!!%c%c%c%c", 1, 11, 0, 0, 0, 0,0);
  }
  if (TT.F) xclose(afd);

  if (FLAG(p) && pid) toys.exitval |= xpclose(pid, pipe);
}
