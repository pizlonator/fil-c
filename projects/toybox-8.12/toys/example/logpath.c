/* logpath.c - Record commands called out of $PATH to a log
 *
 * Copyright 2019 Rob Landley <rob@landley.net>
 *
 * I made it up. Must be built standalone to work. (Is its own multiplexer.)

USE_LOGPATH(NEWTOY(logpath, 0, TOYFLAG_NOHELP|TOYFLAG_USR|TOYFLAG_BIN))

config LOGPATH
  bool "logpath"
  default n
  help
    usage: logpath ...

    Append command line to $LOGPATH, then call second instance
    of command in $PATH.
*/

#define FOR_logpath
#include "toys.h"

#if CFG_TOYBOX
#warning Must be built standalone to work.
#endif

void logpath_main(void)
{
  char *log = getenv("LOGPATH"), *omnom = basename(*toys.argv), *s, *ss, *sss;
  struct string_list *list;
  int i, len;

  // Log the command line
  if (!log) error_exit("no $LOGPATH");
  len = strlen(omnom)+2;
  for (i = 0; i<toys.optc; i++) len += 2*strlen(toys.optargs[i])+3;
  ss = stpcpy(s = xmalloc(len), omnom);

  // Copy arguments surrounded by quotes with \ escapes for " \ or \n
  for (i = 0; i<toys.optc; i++) {
    *(ss++) = ' ';
    *(ss++) = '"';
    for (sss = toys.optargs[i]; *sss; sss++) {
      if (-1 == (len = stridx("\n\\\"", *sss))) *(ss++) = *sss;
      else {
        *(ss++) = '\\';
        *(ss++) = "n\\\""[len];
      }
    }
    *(ss++) = '"';
  }
  *(ss++) = '\n';

  // Atomically append to log and free buffer
  i = xcreate(log, O_WRONLY|O_CREAT|O_APPEND, 0644);
  xwrite(i, s, ss-s);
  close(i);
  free(s);

  // Run next instance in $PATH after this one. If we were called via absolute
  // path search for this instance, otherwise assume we're first instance
  list = find_in_path(getenv("PATH"), omnom);
  if (**toys.argv == '/') {
    while (list) {
      if (!strcmp(list->str, *toys.argv)) break;
      free(llist_pop(&list));
    }
  }

  // Skip first instance and try to run next one, continuing until one works
  for (;list; free(llist_pop(&list))) {
    free(llist_pop(&list));
    *toys.argv = list->str;
    execve(list->str, toys.argv, environ);
  }
  error_exit("no %s after %s in $PATH=%s", omnom,
    **toys.argv == '/' ? *toys.argv : "logpath", getenv("PATH"));
}
