/* logger.c - Log messages.
 *
 * Copyright 2013 Ilya Kuzmich <ilya.kuzmich@gmail.com>
 *
 * See http://pubs.opengroup.org/onlinepubs/9699919799/utilities/logger.html
 *
 * Deviations from posix: specified manner and format, defined implementation.

USE_LOGGER(NEWTOY(logger, "t:p:s", TOYFLAG_USR|TOYFLAG_BIN))

config LOGGER
  bool "logger"
  default y
  help
    usage: logger [-s] [-t TAG] [-p [FACILITY.]PRIORITY] [MESSAGE...]

    Log message (or stdin) to syslog.

    -s	Also write message to stderr
    -t	Use TAG instead of username to identify message source
    -p	Specify PRIORITY with optional FACILITY. Default is "user.notice"
*/

#define FOR_logger
#include "toys.h"

GLOBALS(
  char *p, *t;

  int priority;
)

// find str in names[], accepting unambiguous short matches
// returns offset into array of match, or -1 if no match
// TODO: move to lib?
static int arrayfind(char *str, char *names[], int len)
{
  int j, i, ll = 0, maybe = -1;

  for (j = 0; j<len; j++) for (i=0; ; i++) {
    if (!str[i]) {
      if (!names[j][i]) return j;
      if (i>ll) maybe = j;
      else if (i==ll) maybe = -1;
      break;
    }
    if (!names[j][i] || toupper(str[i])!=toupper(names[j][i])) break;
  }

  return maybe;
}

static void syslog_line(char **pline, long len)
{
  if (!pline) return;
  syslog(TT.priority, "%s", *pline);
}

void logger_main(void)
{
  int facility = LOG_USER, len = 0;
  char *s1, *s2, **arg,
    *priorities[] = {"emerg", "alert", "crit", "error", "warning", "notice",
                     "info", "debug"},
    *facilities[] = {"kern", "user", "mail", "daemon", "auth", "syslog",
                     "lpr", "news", "uucp", "cron", "authpriv", "ftp"};

  if (!TT.t) TT.t = xgetpwuid(geteuid())->pw_name;
  TT.priority = LOG_NOTICE;
  if (TT.p) {
    if (!(s1 = strchr(TT.p, '.'))) s1 = TT.p;
    else {
      *s1++ = 0;
      facility = arrayfind(TT.p, facilities, ARRAY_LEN(facilities));
      if (facility<0) {
        if (sscanf(TT.p, "local%d", &facility)>0 && !(facility&~7))
          facility += 16;
        else error_exit("bad facility: %s", TT.p);
      }
      facility *= 8;
    }

    TT.priority = arrayfind(s1, priorities, ARRAY_LEN(priorities));
    if (TT.priority<0) error_exit("bad priority: %s", s1);
  }

  openlog(TT.t, LOG_PERROR*FLAG(s), facility);
  if (toys.optc) {
    for (arg = toys.optargs; *arg; arg++) len += strlen(*arg)+1;
    s1 = s2 = xmalloc(len);
    for (arg = toys.optargs; *arg; arg++) {
      if (arg != toys.optargs) *s2++ = ' ';
      s2 = stpcpy(s2, *arg);
    }
    syslog(TT.priority, "%s", s1);
  } else do_lines(0, '\n', syslog_line);
  closelog();
}
