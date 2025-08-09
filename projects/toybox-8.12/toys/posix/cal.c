/* cal.c - show calendar.
 *
 * Copyright 2011 Rob Landley <rob@landley.net>
 *
 * See http://opengroup.org/onlinepubs/9699919799/utilities/cal.html

USE_CAL(NEWTOY(cal, ">3h", TOYFLAG_USR|TOYFLAG_BIN))

config CAL
  bool "cal"
  default y
  help
    usage: cal [-h] [[[DAY] MONTH] YEAR]

    Print a calendar.

    With one argument, prints all months of the specified year.
    With two arguments, prints calendar for month and year.
    With three arguments, highlights day within month and year.

    -h	Don't highlight today
*/

#define FOR_cal
#include "toys.h"

GLOBALS(
  struct tm *now;
)

// Thirty days hath september april june and november. February is just weird.
static int monthlen(struct tm *tm)
{
  int len = 31, month = tm->tm_mon, year = tm->tm_year;

  if (tm->tm_mon==1) {
    len = 28;
    if (!(year&3) && !((year%100) && !(year%400))) len++;
  } else if ((month+(month>6))&1) len = 30;

  return len;
}

// Write calendar into buffer: each line is 20 chars wide, end indicated
// by empty string.

static char *calstrings(char *buf, struct tm *tm)
{
  int wday, mday, start, len, line;
  char temp[21];

  // header
  len = strftime(temp, 21, "%B %Y", tm);
  len += (20-len)/2;
  buf += sprintf(buf, "%*s%*s ", len, temp, 20-len, "")+1;
  buf += sprintf(buf, "Su Mo Tu We Th Fr Sa ")+1;

  // What day of the week does this month start on?
  if (tm->tm_mday>1) start = (36+tm->tm_wday-tm->tm_mday)%7;
  else start = tm->tm_wday;

  // What day does this month end on?  Alas, libc doesn't tell us...
  len = monthlen(tm);

  for (mday = line = 0; line<6; line++) {
    for (wday=0; wday<7; wday++) {
      char *pat = "   ";
      if (!mday ? wday==start : mday<len) {
        pat = "%2d ";
        if (!FLAG(h) && tm->tm_year == TT.now->tm_year &&
            tm->tm_mon == TT.now->tm_mon && mday == TT.now->tm_mday-1) {
          pat = "\x1b[7m%2d\x1b[m ";
        }
        mday++;
      }
      buf += sprintf(buf, pat, mday);
    }
    buf++;
  }

  return buf;
}

// Worst case scenario toybuf usage: sizeof(struct tm) plus 21 bytes/line
// plus 8 lines/month plus 12 months, plus the escape sequences to highlight
// today comes to a bit over 2k of our 4k buffer.

void cal_main(void)
{
  time_t now = time(0);
  struct tm *tm = localtime(&now);
  char *buf = toybuf;

  TT.now = tm;
  if (!isatty(1)) toys.optflags |= FLAG_h;

  if (toys.optc) {
    // Conveniently starts zeroed
    tm = (struct tm *)toybuf;
    buf += sizeof(struct tm);

    // Last argument is year, one before that (if any) is month.
    tm->tm_year = atolx_range(toys.optargs[--toys.optc], 1, 9999) - 1900;
    tm->tm_mday = 1;
    tm->tm_hour = 12;  // noon to avoid timezone weirdness
    if (toys.optc) {
      tm->tm_mon = atolx_range(toys.optargs[--toys.optc], 1, 12)-1;
      if (toys.optc) {
        tm->tm_mday = atolx_range(toys.optargs[--toys.optc], 1, monthlen(tm));
        TT.now = tm;
      }

    // Print 12 months of the year

    } else {
      char *bufs[12];
      int i, j, k;

      for (i=0; i<12; i++) {
        tm->tm_mon=i;
        mktime(tm);
        buf = calstrings(bufs[i]=buf, tm);
      }

      // 4 rows, 6 lines each, 3 columns
      for (i=0; i<4; i++) {
        for (j=0; j<8; j++) {
          for(k=0; k<3; k++) {
            char **b = bufs+(k+i*3);
            *b += printf("%s ", *b);
          }
          puts("");
        }
      }
      return;
    }

    // What day of the week does that start on?
    mktime(tm);
  }

  calstrings(buf, tm);
  while (*buf) buf += printf("%s\n", buf);
}
