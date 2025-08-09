/* gzip.c - gzip/gunzip/zcat
 *
 * Copyright 2017 The Android Open Source Project
 *
 * See http://refspecs.linuxfoundation.org/LSB_4.1.0/LSB-Core-generic/LSB-Core-generic/gzip.html
 * GZIP RFC: http://www.ietf.org/rfc/rfc1952.txt
 *
 * TODO: qv --rsyncable

// gzip.net version allows all options for all commands.
USE_GZIP(NEWTOY(gzip,    "n(no-name)cdfkt123456789[-123456789]", TOYFLAG_USR|TOYFLAG_BIN))
USE_GUNZIP(NEWTOY(gunzip, "cdfkt123456789[-123456789]", TOYFLAG_USR|TOYFLAG_BIN))
USE_ZCAT(NEWTOY(zcat,     "cdfkt123456789[-123456789]", TOYFLAG_USR|TOYFLAG_BIN))

config GZIP
  bool "gzip"
  default n
  help
    usage: gzip [-19cdfkt] [FILE...]

    Compress files. With no files, compresses stdin to stdout.
    On success, the input files are removed and replaced by new
    files with the .gz suffix.

    -c	Output to stdout
    -d	Decompress (act as gunzip)
    -f	Force: allow overwrite of output file
    -k	Keep input files (default is to remove)
    -t	Test integrity
    -#	Compression level 1-9 (1:fastest, 6:default, 9:best)

config GUNZIP
  bool "gunzip"
  default y
  help
    usage: gunzip [-cfkt] [FILE...]

    Decompress files. With no files, decompresses stdin to stdout.
    On success, the input files are removed and replaced by new
    files without the .gz suffix.

    -c	Output to stdout (act as zcat)
    -f	Force: allow read from tty
    -k	Keep input files (default is to remove)
    -t	Test integrity

config ZCAT
  bool "zcat"
  default y
  help
    usage: zcat [-f] [FILE...]

    Decompress files to stdout. Like `gzip -dc`.

    -f	Force: allow read from tty
*/

#define FORCE_FLAGS
#define FOR_gzip
#include "toys.h"

GLOBALS(
  int level;
)

// Use assembly optimized zlib code?
#if CFG_TOYBOX_LIBZ
#include <zlib.h>

// Read from in_fd, write to out_fd, decompress if dd else compress
static int do_deflate(int in_fd, int out_fd, int dd, int level)
{
  int len, err = 0;
  char *b = "r";
  gzFile gz;

  if (!dd) {
    sprintf(b = toybuf, "w%d", level);
    if (out_fd == 1) out_fd = xdup(out_fd);
  }
  if (!(gz = gzdopen(dd ? in_fd : out_fd, b))) perror_exit("gzdopen");
  if (dd) {
    if (gzdirect(gz)) error_exit("not gzip");
    while ((len = gzread(gz, toybuf, sizeof(toybuf))) > 0)
      if (len != writeall(out_fd, toybuf, len)) break;
  } else {
    while ((len = read(in_fd, toybuf, sizeof(toybuf))) > 0)
      if (len != gzwrite(gz, toybuf, len))  break;
  }

  err = !!len;
  if (len>0 || err == Z_ERRNO) perror_msg(dd ? "write" : "read");
  if (len<0)
    error_msg("%s%s: %s", "gz", dd ? "read" : "write", gzerror(gz, &len));

  if (gzclose(gz) != Z_OK) perror_msg("gzclose"), err++;

  return err;
}

// Use toybox's builtin lib/deflate.c
#else

// Read from in_fd, write to out_fd, decompress if dd else compress
static int do_deflate(int in_fd, int out_fd, int dd, int level)
{
  int x;

  if (dd) WOULD_EXIT(x, gunzip_fd(in_fd, out_fd));
  else WOULD_EXIT(x, gzip_fd(in_fd, out_fd));

  return x;
}

#endif

static void do_gzip(int ifd, char *in)
{
  struct stat sb;
  char *out = 0;
  int ofd = FLAG(t) ? xopen("/dev/null", O_WRONLY) :  0;

  // Are we writing to stdout?
  if (!ifd || FLAG(c)) ofd = 1;
  if (isatty(ifd)) {
    if (!FLAG(f)) return error_msg("%s:need -f to read TTY"+3*!!ifd, in);
    else ofd = 1;
  }

  // Are we reading file.gz to write to file?
  if (!ofd) {
    if (fstat(ifd, &sb)) return perror_msg_raw(in);

    // Add or remove .gz suffix as necessary
    if (!FLAG(d)) out = xmprintf("%s%s", in, ".gz");
    else if ((out = strend(in, ".gz"))>in) out = xstrndup(in, out-in);
    else if ((out = strend(in, ".tgz"))>in)
      out = xmprintf("%.*s.tar", (int)(out-in), in);
    else return error_msg("no .gz: %s", in);
    ofd = xcreate(out, O_CREAT|O_WRONLY|WARN_ONLY|O_EXCL*!FLAG(f), sb.st_mode);
    if (ofd == -1) return;
  }

  if (do_deflate(ifd, ofd, FLAG(d), TT.level)) in = out;

  if (out) {
    struct timespec times[] = {sb.st_atim, sb.st_mtim};

    if (utimensat(AT_FDCWD, out, times, 0)) perror_exit("utimensat");
    if (chmod(out, sb.st_mode)) perror_exit("chmod");
    close(ofd);
    if (!FLAG(k) && in && unlink(in)) perror_msg("unlink %s", in);
    free(out);
  }
}

void gzip_main(void)
{
  // This depends on 1-9 being at the end of the option list
  for (TT.level = 0; TT.level<9; TT.level++)
    if ((toys.optflags>>TT.level)&1) break;
  if (!(TT.level = 9-TT.level)) TT.level = 6;

  if (FLAG(t)) toys.optflags |= FLAG_d;

  loopfiles(toys.optargs, do_gzip);
}

void gunzip_main(void)
{
  toys.optflags |= FLAG_d;
  gzip_main();
}

void zcat_main(void)
{
  toys.optflags |= (FLAG_c|FLAG_d);
  gzip_main();
}
