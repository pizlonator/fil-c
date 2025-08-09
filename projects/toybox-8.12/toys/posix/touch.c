/* touch.c : change timestamp of a file
 *
 * Copyright 2012 Choubey Ji <warior.linux@gmail.com>
 *
 * See http://pubs.opengroup.org/onlinepubs/9699919799/utilities/touch.html
 *
 * -f is ignored for BSD/macOS compatibility. busybox/coreutils also support
 * this, but only coreutils documents it in --help output.

USE_TOUCH(NEWTOY(touch, "<1acd:fmr:t:h[!dtr]", TOYFLAG_BIN))

config TOUCH
  bool "touch"
  default y
  help
    usage: touch [-amch] [-d DATE] [-t TIME] [-r FILE] FILE...

    Update the access and modification times of each FILE to the current time.

    -a	Change access time
    -m	Change modification time
    -c	Don't create file
    -h	Change symlink
    -d	Set time to DATE (in YYYY-MM-DDThh:mm:SS[.frac][tz] format)
    -t	Set time to TIME (in [[CC]YY]MMDDhhmm[.ss][frac] format)
    -r	Set time same as reference FILE
*/

#define FOR_touch
#include "toys.h"

GLOBALS(
  char *t, *r, *d;
)

void touch_main(void)
{
  struct timespec ts[2];
  char **ss;
  int fd;

  // use current time if no -t or -d
  ts[0].tv_nsec = UTIME_NOW;

  if (FLAG(t) || FLAG(d)) {
    time_t t = time(0);
    unsigned nano;

    xparsedate(TT.t ? TT.t : TT.d, &t, &nano, 0);
    ts->tv_sec = t;
    ts->tv_nsec = nano;
  }
  ts[1]=ts[0];

  if (TT.r) {
    struct stat st;

    xstat(TT.r, &st);
    ts[0] = st.st_atim;
    ts[1] = st.st_mtim;
  }

  // Which time(s) should we actually change?
  if (FLAG(a)^FLAG(m)) ts[!FLAG(m)].tv_nsec = UTIME_OMIT;

  // Loop through files on command line
  for (ss = toys.optargs; *ss;) {
    char *s = *ss++;

    if (!strcmp(s, "-")) {
      if (!futimens(1, ts)) continue;
    } else {
      if (!utimensat(AT_FDCWD, s, ts, FLAG(h)*AT_SYMLINK_NOFOLLOW)) continue;
      if (FLAG(c)) continue;
      if (access(s, F_OK) && (-1!=(fd = open(s, O_CREAT, 0666)))) {
        close(fd);
        if (toys.optflags) ss--;
        continue;
      }
    }
    perror_msg("'%s'", s);
  }
}
