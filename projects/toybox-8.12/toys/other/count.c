/* count.c - Progress indicator from stdin to stdout
 *
 * Copyright 2002 Rob Landley <rob@landley.net>

USE_COUNT(NEWTOY(count, "<0>0l", TOYFLAG_USR|TOYFLAG_BIN))

config COUNT
  bool "count"
  default y
  help
    usage: count [-l]

    -l	Long output (total bytes, human readable, transfer rate, elapsed time)

    Copy stdin to stdout, displaying simple progress indicator to stderr.
*/

#define FOR_count
#include "toys.h"

GLOBALS(
  unsigned long long size, start;
  unsigned tick, *slice;
)

static void display(unsigned long long now)
{
  unsigned long long bb;
  unsigned seconds, ii, len = TT.tick ? minof(TT.tick, 63): 1;

  // bytes, human readable size, elapsed time, rate over last 16 seconds
  if (FLAG(l)) {
    human_readable(toybuf+256, TT.size, 0);
    seconds = (now - TT.start)/1000;

    // We zero new period and count it immediately, which lowers the average
    // if we haven't had time to fill it yet. Compensate by keeping one extra
    // old period and adding prorated fraction of it.
    bb = (TT.slice[63&(TT.tick-len)]*(250-(now%250)))/250;

    // Add up periods, convert to seconds via elapsed milliseconds
    for (ii = 0; ii<len;) bb += TT.slice[63&(TT.tick-ii++)];
    len *= 250;
    if (now>TT.start && len>now-TT.start) len = now-TT.start;
    human_readable(toybuf+512, (bb*1000)/len, 0);

    sprintf(toybuf+1024, ", %sb, %sb/s, %um%02us", toybuf+256, toybuf+512,
      seconds/60, seconds%60);
  }
  dprintf(2, "%llu bytes%s   \r", TT.size, toybuf+1024);
}

void count_main(void)
{
  struct pollfd pfd = {0, POLLIN, 0};
  unsigned long long now, then;
  char *buf = xmalloc(65536);
  int len;

  TT.slice = (void *)toybuf;
  TT.start = now = then = millitime();

  // poll, print if data not ready, update 4x/second otherwise
  for (;;) {
    // Update display 4x/second
    if ((now = millitime())>=then) {
      display(now);
      while (then<=now) {
        then += 250;
        if (FLAG(l)) TT.slice[63&++TT.tick] = 0;
      }
    }
    if (!(len = poll(&pfd, 1, then-now))) continue;
    if (len<0 && errno != EINTR && errno != ENOMEM) perror_exit(0);
    if ((len = xread(0, buf, 65536))) {
      xwrite(1, buf, len);
      TT.size += len;
      TT.slice[63&TT.tick] += len;
      if ((now = millitime())-then<250) continue;
    }
    if (!len) break;
  }
  display(now);
  dprintf(2, "\n");
}
