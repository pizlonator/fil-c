/* dos2unix.c - convert newline format
 *
 * Copyright 2012 Rob Landley <rob@landley.net>

USE_DOS2UNIX(NEWTOY(dos2unix, 0, TOYFLAG_BIN))
USE_UNIX2DOS(NEWTOY(unix2dos, 0, TOYFLAG_BIN))

config DOS2UNIX
  bool "dos2unix/unix2dos"
  default y
  help
    usage: dos2unix [FILE...]

    Convert newline format from dos "\r\n" to unix "\n".
    If no files listed copy from stdin, "-" is a synonym for stdin.

config UNIX2DOS
  bool "unix2dos"
  default y
  help
    usage: unix2dos [FILE...]

    Convert newline format from unix "\n" to dos "\r\n".
    If no files listed copy from stdin, "-" is a synonym for stdin.
*/

#define FOR_dos2unix
#include "toys.h"

GLOBALS(
  char *tempfile;
)

static void do_dos2unix(int fd, char *name)
{
  char c = toys.which->name[0];
  int outfd = 1, catch = 0;

  if (fd) outfd = copy_tempfile(fd, name, &TT.tempfile);

  for (;;) {
    int len, in, out;

    len = read(fd, toybuf+(sizeof(toybuf)/2), sizeof(toybuf)/2);
    if (len<0) perror_msg_raw(name);
    if (len<1) break;

    for (in = out = 0; in < len; in++) {
      char x = toybuf[in+sizeof(toybuf)/2];

      // Drop \r only if followed by \n in dos2unix mode
      if (catch) {
        if (c == 'u' || x != '\n') toybuf[out++] = '\r';
        catch = 0;
      // Add \r only if \n not after \r in unix2dos mode
      } else if (c == 'u' && x == '\n') toybuf[out++] = '\r';

      if (x == '\r') catch++;
      else toybuf[out++] = x;
    }
    xwrite(outfd, toybuf, out);
  }
  if (catch) xwrite(outfd, "\r", 1);

  if (fd) replace_tempfile(-1, outfd, &TT.tempfile);
}

void dos2unix_main(void)
{
  loopfiles(toys.optargs, do_dos2unix);
}

void unix2dos_main(void)
{
  dos2unix_main();
}
