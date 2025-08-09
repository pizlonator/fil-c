/* rm.c - remove files
 *
 * Copyright 2012 Rob Landley <rob@landley.net>
 *
 * See http://pubs.opengroup.org/onlinepubs/9699919799/utilities/rm.html

USE_RM(NEWTOY(rm, "f(force)iRrv[-fi]", TOYFLAG_BIN))

config RM
  bool "rm"
  default y
  help
    usage: rm [-fiRrv] FILE...

    Remove each argument from the filesystem.

    -f	Force: remove without confirmation, no error if it doesn't exist
    -i	Interactive: prompt for confirmation
    -rR	Recursive: remove directory contents
    -v	Verbose
*/

#define FOR_rm
#include "toys.h"

static int do_rm(struct dirtree *try)
{
  int fd=dirtree_parentfd(try), dir=S_ISDIR(try->st.st_mode), or=0, using=0;

  // Skip . and .. (yes, even explicitly on the command line: posix says to)
  if (isdotdot(try->name)) return 0;

  // Intentionally fail non-recursive attempts to remove even an empty dir
  // (via wrong flags to unlinkat) because POSIX says to.
  if (dir && !FLAG(r) && !FLAG(R)) goto skip;

  // This is either the posix section 2(b) prompt or the section 3 prompt.
  if (!FLAG(f)
    && (!S_ISLNK(try->st.st_mode) && faccessat(fd, try->name, W_OK, 0))) or++;

  // Posix section 1(a), don't prompt for nonexistent.
  if (or && errno == ENOENT) goto skip;

  if (!(dir && try->again) && ((or && isatty(0)) || FLAG(i))) {
    char *s = dirtree_path(try, 0);

    fprintf(stderr, "rm %s%s%s", or ? "ro " : "", dir ? "dir " : "", s);
    free(s);
    or = yesno(0);
    if (!or) goto nodelete;
  }

  // handle directory recursion
  if (dir) {
    using = AT_REMOVEDIR;
    // Handle chmod 000 directories when -f
    if (faccessat(fd, try->name, R_OK, 0)) {
      if (FLAG(f)) wfchmodat(fd, try->name, 0700);
      else goto skip;
    }
    if (!try->again) return DIRTREE_COMEAGAIN;
    if (try->symlink) goto skip;
    if (FLAG(i)) {
      char *s = dirtree_path(try, 0);

      // This is the section 2(d) prompt. (Yes, posix says to prompt twice.)
      fprintf(stderr, "rmdir %s", s);
      free(s);
      or = yesno(0);
      if (!or) goto nodelete;
    }
  }

skip:
  if (!unlinkat(fd, try->name, using)) {
    if (FLAG(v)) {
      char *s = dirtree_path(try, 0);
      printf("%s%s '%s'\n", toys.which->name, dir ? "dir" : "", s);
      free(s);
    }
  } else {
    if (!dir || try->symlink != (char *)2) perror_msg_raw(try->name);
nodelete:
    if (try->parent) try->parent->symlink = (char *)2;
  }

  return 0;
}

void rm_main(void)
{
  char **s;

  // Can't use <1 in optstring because zero arguments with -f isn't an error
  if (!toys.optc && !FLAG(f)) help_exit("Needs 1 argument");

  for (s = toys.optargs; *s; s++) {
    if (!strcmp(*s, "/")) {
      error_msg("rm /. if you mean it");
      continue;
    }
    // "rm dir/.*" can expand to include .. which generally isn't what you want
    if (!strcmp("..", basename(*s))) {
      error_msg("bad path %s", *s);
      continue;
    }

    // Files that already don't exist aren't errors for -f. Use lstat() instead
    // of faccessat() because bionic doesn't support AT_SYMLINK_NOFOLLOW
    if (FLAG(f) && lstat(*s, (void *)toybuf) && errno == ENOENT) continue;

    // There's a race here where a file removed between the above check and
    // dirtree's stat would report the nonexistence as an error, but that's
    // not a normal "it didn't exist" so I'm ok with it.
    dirtree_read(*s, do_rm);
  }
}
