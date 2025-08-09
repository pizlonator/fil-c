/* md5sum.c - Calculate hashes md5, sha1, sha224, sha256, sha384, sha512.
 *
 * Copyright 2012, 2021 Rob Landley <rob@landley.net>
 *
 * See http://refspecs.linuxfoundation.org/LSB_4.1.0/LSB-Core-generic/LSB-Core-generic/md5sum.html
 * and http://www.ietf.org/rfc/rfc1321.txt
 * and http://www.ietf.org/rfc/rfc4634.txt
 *
 * They're combined this way to share infrastructure, and because md5sum is
 * a LSB standard command (but sha1sum and newer hashes are a good idea,
 * see http://valerieaurora.org/hash.html).
 *
 * We optionally use openssl (or equivalent) to access assembly optimized
 * versions of these functions, but provide a built-in version to reduce
 * required dependencies.
 *
 * coreutils supports --status but not -s, busybox supports -s but not --status

USE_MD5SUM(NEWTOY(md5sum, "bc(check)s(status)[!bc]", TOYFLAG_USR|TOYFLAG_BIN))
USE_SHA1SUM(OLDTOY(sha1sum, md5sum, TOYFLAG_USR|TOYFLAG_BIN))
USE_SHA224SUM(OLDTOY(sha224sum, md5sum, TOYFLAG_USR|TOYFLAG_BIN))
USE_SHA256SUM(OLDTOY(sha256sum, md5sum, TOYFLAG_USR|TOYFLAG_BIN))
USE_SHA384SUM(OLDTOY(sha384sum, md5sum, TOYFLAG_USR|TOYFLAG_BIN))
USE_SHA512SUM(OLDTOY(sha512sum, md5sum, TOYFLAG_USR|TOYFLAG_BIN))

config MD5SUM
  bool "md5sum"
  default y
  help
    usage: ???sum [-bcs] [FILE]...

    Calculate hash for each input file, reading from stdin if none, writing
    hexadecimal digits to stdout for each input file (md5=32 hex digits,
    sha1=40, sha224=56, sha256=64, sha384=96, sha512=128) followed by filename.

    -b	Brief (hash only, no filename)
    -c	Check each line of each FILE is the same hash+filename we'd output
    -s	No output, exit status 0 if all hashes match, 1 otherwise

config SHA1SUM
  bool "sha1sum"
  default y
  help
    See md5sum

config SHA224SUM
  bool "sha224sum"
  default y
  help
    See md5sum

config SHA256SUM
  bool "sha256sum"
  default y
  help
    See md5sum

config SHA384SUM
  bool "sha384sum"
  default y
  help
    See md5sum

config SHA512SUM
  bool "sha512sum"
  default y
  help
    See md5sum
*/

#define FORCE_FLAGS
#define FOR_md5sum
#include "toys.h"

GLOBALS(
  int sawline;
)

// Callback for loopfiles()
// Call builtin or lib hash function, then display output if necessary
static void do_hash(int fd, char *name)
{
  hash_by_name(fd, toys.which->name, toybuf);

  if (name) printf("%s  %s\n"+4*FLAG(b), toybuf, name);
}

static void do_c_line(char *line)
{
  int space = 0, fail = 0, fd;
  char *name;

  for (name = line; *name; name++) {
    if (isspace(*name)) {
      space++;
      *name = 0;
    } else if (space) break;
  }
  if (!space || !*line || !*name) return error_msg("bad line %s", line);

  fd = !strcmp(name, "-") ? 0 : open(name, O_RDONLY);

  TT.sawline = 1;
  if (fd==-1) {
    perror_msg_raw(name);
    *toybuf = 0;
  } else do_hash(fd, 0);
  if (strcasecmp(line, toybuf)) toys.exitval = fail = 1;
  if (!FLAG(s)) printf("%s: %s\n", name, fail ? "FAILED" : "OK");
  if (fd>0) close(fd);
}

// Used instead of loopfiles_line to report error on files containing no hashes.
static void do_c_file(char *name)
{
  FILE *fp = !strcmp(name, "-") ? stdin : fopen(name, "r");
  char *line;

  if (!fp) return perror_msg_raw(name);

  TT.sawline = 0;

  for (;;) {
    if (!(line = xgetline(fp))) break;
    do_c_line(line);
    free(line);
  }
  if (fp!=stdin) fclose(fp);

  if (!TT.sawline) error_msg("%s: no lines", name);
}

void md5sum_main(void)
{
  int i;

  if (FLAG(c)) for (i = 0; toys.optargs[i]; i++) do_c_file(toys.optargs[i]);
  else {
    if (FLAG(s)) error_exit("-s only with -c");
    loopfiles(toys.optargs, do_hash);
  }
}
