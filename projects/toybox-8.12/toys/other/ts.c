/* ts.c - timestamp input lines
 *
 * Copyright 2023 Oliver Webb <aquahobbyist@proton.me>
 *
 * No standard.

USE_TS(NEWTOY(ts, "ims", TOYFLAG_USR|TOYFLAG_BIN|TOYFLAG_LINEBUF))

config TS
  bool "ts"
  default y
  help
    usage: ts [-is] [FORMAT]

    Add timestamps to each line in pipeline. Default format without options
    "%b %d %H:%M:%S", with -i or -s "%H:%M:%S".

    -i	Incremental (since previous line)
    -m	Add milliseconds
    -s	Since start
*/

#define FOR_ts
#include "toys.h"

// because millitime() is monotonic, which returns uptime.
static long long millinow(void)
{
  struct timespec ts;

  clock_gettime(CLOCK_REALTIME, &ts);

  return ts.tv_sec*1000+ts.tv_nsec/1000000;
}

void ts_main(void)
{
  char *line, *mm = toybuf+sizeof(toybuf)-8,
       *format = toys.optflags ? "%T" : "%b %d %T";
  long long start = millinow(), now, diff, rel = FLAG(i) || FLAG(s);
  struct tm *tm;
  time_t tt;

  for (; (line = xgetline(stdin)); free(line)) {
    now = millinow();
    diff = now - start*rel;
    if (FLAG(m)) sprintf(mm, ".%03lld", diff%1000);
    tt = diff/1000;
    tm = rel ? gmtime(&tt) : localtime(&tt);
    if (FLAG(i)) start = now;
    strftime(toybuf, sizeof(toybuf)-16, *toys.optargs ? : format, tm);
    xprintf("%s%s %s\n", toybuf, mm, line);
  }
}
