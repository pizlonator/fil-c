/* readlink.c - Return string representation of a symbolic link.
 *
 * Copyright 2007 Rob Landley <rob@landley.net>

USE_READLINK(NEWTOY(readlink, "<1vnf(canonicalize)emqz[-mef][-qv]", TOYFLAG_USR|TOYFLAG_BIN))
USE_REALPATH(NEWTOY(realpath, "<1(relative-base):R(relative-to):s(no-symlinks)LPemqz[-Ps][-LP][-me]", TOYFLAG_USR|TOYFLAG_BIN))

config READLINK
  bool "readlink"
  default y
  help
    usage: readlink [-efmnqz] FILE...

    With no options, show what symlink points to, return error if not symlink.

    Options for producing canonical paths (all symlinks/./.. resolved):

    -e	Canonical path to existing entry (fail if missing)
    -f	Full path (fail if directory missing)
    -m	Ignore missing entries, show where it would be
    -n	No trailing newline
    -q	Quiet (no error messages)
    -z	NUL instead of newline

config REALPATH
  bool "realpath"
  default y
  help
    usage: realpath [-LPemqsz] [--relative-base DIR] [-R DIR] FILE...

    Display the canonical absolute pathname

    -R Show ../path relative to DIR (--relative-to)
    -L Logical path (resolve .. before symlinks)
    -P Physical path (default)
    -e Canonical path to existing entry (fail if missing)
    -m Ignore missing entries, show where it would be
    -q Quiet (no error messages)
    -s Don't expand symlinks
    -z NUL instead of newline
    --relative-base  If path under DIR trim off prefix
*/

#define FOR_realpath
#define FORCE_FLAGS
#include "toys.h"

GLOBALS(
  char *R, *relative_base;
)

// test TT.relative_base -RsmLP
// Trim .. out early for -s and -L. TODO: in place in the input string.

static char *resolve(char *arg)
{
  int flags = FLAG(e) ? ABS_FILE : FLAG(m) ? 0 : ABS_PATH;
  char *s, *ss = 0, *dd = 0;

  if (FLAG(s)) flags |= ABS_KEEP;
  else if (FLAG(L)) arg = dd = xabspath(arg, ABS_KEEP);
  if (!(s = xabspath(arg, flags)) && !FLAG(q)) perror_msg_raw(arg);
  free(dd);

  // Trim off this prefix if path under here

  if (TT.relative_base) {
    ss = s;
    if (strstart(&ss, TT.relative_base) && (!*ss || *ss=='/')) {
      if (*ss=='/') ss++;
      ss = xstrdup(!*ss ? "." : ss);
    } else ss = 0;
  } else if (TT.R) ss = relative_path(TT.R, s, 0);
  if (ss) {
    free(s);
    s = ss;
  }

  return s;
}

// Resolve command line arguments that can't take part in their own resolution
static char *presolve(char **s)
{
  char *ss = *s;

  if (ss) {
    *s = 0;
    if (!(*s = resolve(ss))) xexit();
  }

  return ss;
}

// Uses realpath flag context: flags (1 = resolve, 2 = -n)
static void do_paths(int flags)
{
  char **arg, *s;

  if (!presolve(&TT.relative_base)) presolve(&TT.R);

  for (arg = toys.optargs; *arg; arg++) {
    if (!(s = (flags&1) ? resolve(*arg) : xreadlink(*arg))) toys.exitval = 1;
    else xprintf(((flags&2) && !arg[1]) ? "%s" : "%s%c", s, '\n'*!FLAG(z));
    free(s);
  }
}

void realpath_main(void)
{
  do_paths(1);
}

#define FOR_readlink
#include "generated/flags.h"

// Convert readlink flag context to realpath (feeding in -nf separately)
void readlink_main(void)
{
  int nf = (toys.optflags/FLAG_f)|!!(FLAG(m)|FLAG(e));

  toys.optflags &= FLAG_f-1;
  if (!FLAG(v)) toys.optflags |= FLAG_q;
  do_paths(nf);
}
