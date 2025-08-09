/* which.c - Find executable files in $PATH.
 *
 * Copyright 2006 Rob landley <rob@landley.net>

USE_WHICH(NEWTOY(which, "<1a", TOYFLAG_USR|TOYFLAG_BIN))

config WHICH
  bool "which"
  default y
  help
    usage: which [-a] filename ...

    Search $PATH for executable files matching filename(s).

    -a	Show all matches
*/
#include "toys.h"

// Find an exectuable file either at a path with a slash in it (absolute or
// relative to current directory), or in $PATH.  Returns absolute path to file,
// or NULL if not found.

static int which_in_path(char *filename)
{
  struct string_list *list;

  // If they gave us a path, don't worry about $PATH or -a

  if (strchr(filename, '/')) {
    // Confirm it has the executable bit set, and it's not a directory.
    if (!access(filename, X_OK)) {
      struct stat st;

      if (!stat(filename, &st) && S_ISREG(st.st_mode)) {
        puts(filename);
        return 0;
      }
      return 1;
    }
  }

  // Search $PATH for matches.
  list = find_in_path(getenv("PATH"), filename);
  if (!list) return 1;

  // Print out matches
  while (list) {
    if (!access(list->str, X_OK)) {
      puts(list->str);
      // If we should stop at one match, do so
      if (!toys.optflags) {
        llist_traverse(list, free);
        break;
      }
    }
    free(llist_pop(&list));
  }

  return 0;
}

void which_main(void)
{
  int i;

  for (i=0; toys.optargs[i]; i++)
    toys.exitval |= which_in_path(toys.optargs[i]);
}
