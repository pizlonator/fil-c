/* mktemp.c - Create a temporary file or directory.
 *
 * Copyright 2012 Elie De Brauwer <eliedebrauwer@gmail.com>
 *
 * http://refspecs.linuxfoundation.org/LSB_4.1.0/LSB-Core-generic/LSB-Core-generic/mktemp.html

USE_MKTEMP(NEWTOY(mktemp, ">1(tmpdir);:uqd(directory)p:t", TOYFLAG_BIN))

config MKTEMP
  bool "mktemp"
  default y
  help
    usage: mktemp [-dqtu] [-p DIR] [TEMPLATE]

    Safely create a new file "DIR/TEMPLATE" and print its name.

    -d	Create directory instead of file (--directory)
    -p	Put new file in DIR (--tmpdir)
    -q	Quiet, no error messages
    -t	Prefer $TMPDIR > DIR > /tmp (default DIR > $TMPDIR > /tmp)
    -u	Don't create anything, just print what would be created

    Each X in TEMPLATE is replaced with a random printable character. The
    default TEMPLATE is tmp.XXXXXXXXXX.
*/

#define FOR_mktemp
#include "toys.h"

GLOBALS(
  char *p, *tmpdir;
)

void mktemp_main(void)
{
  char *template = *toys.optargs, *dir, *te = getenv("TMPDIR");
  int len;

  // --tmpdir's argument is optional's but -p is mandatory, so can't combine
  if (!TT.p && FLAG(tmpdir)) {
    TT.p = TT.tmpdir ? TT.tmpdir : "";
    toys.optflags |= FLAG_p;
  }
  dir = TT.p;
  // if template, no prefix unless -pt. if !template, always prefix
  if (!dir || !*dir || (FLAG(t) && te && *te)) dir = te;
  if (!dir || !*dir) dir = "/tmp";
  if (!template) template = "tmp.XXXXXXXXXX";
  else {
    if (*template == '/' && TT.p && *TT.p) perror_exit("-p + /template");
    if (!FLAG(p) && !FLAG(t)) dir = 0;
  }

  // TODO: coreutils cleans paths, so -p /t/// would result in /t/xxx...
  template = dir ? xmprintf("%s/%s", dir, template) : xstrdup(template);
  len = strlen(template);
  if (len<3 || strcmp(template+len-3, "XXX")) perror_exit("need XXX");

  // In theory you just xputs(mktemp(template)) for -u, in practice there's
  // link-time deprecation warnings if you do that. So we fake up our own:
  if (FLAG(u)) {
    long long rr;
    char *s = template+len;

    // Replace X with 64 chars from posix portable character set (all but "_").
    xgetrandom(&rr, sizeof(rr));
    while (--s>template) {
      if (*s != 'X') break;
      *s = '-'+(rr&63);
      if (*s>'.') ++*s;
      if (*s>'9') (*s) += 7;
      if (*s>'Z') (*s) += 6;
      rr>>=6;
    }
  } else if (FLAG(d) ? !mkdtemp(template) : mkstemp(template) == -1) {
    if (FLAG(q)) {
      toys.exitval = 1;
      return;
    } else perror_exit("Failed to create %s %s",
        FLAG(d) ? "directory" : "file", template);
  }

  xputs(template);
  if (CFG_TOYBOX_FREE) free(template);
}
