/* inotifyd.c - inotify daemon.
 *
 * Copyright 2013 Ashwini Kumar <ak.ashwini1981@gmail.com>
 * Copyright 2013 Kyungwan Han <asura321@gmail.com>
 *
 * No Standard.

USE_INOTIFYD(NEWTOY(inotifyd, "<2", TOYFLAG_USR|TOYFLAG_BIN|TOYFLAG_LINEBUF))

config INOTIFYD
  bool "inotifyd"
  default y
  help
    usage: inotifyd PROG FILE[:MASK] ...

    When a filesystem event matching MASK occurs to a FILE, run PROG as:

      PROG EVENTS FILE [DIRFILE]

    If PROG is "-" events are sent to stdout.

    This file is:
      a  accessed    c  modified    e  metadata change  w  closed (writable)
      r  opened      D  deleted     M  moved            0  closed (unwritable)
      u  unmounted   o  overflow    x  unwatchable

    A file in this directory is:
      m  moved in    y  moved out   n  created          d  deleted

    When x event happens for all FILEs, inotifyd exits (after waiting for PROG).
*/

#define FOR_inotifyd
#include "toys.h"
#include <sys/inotify.h>

void inotifyd_main(void)
{
  struct pollfd fds;
  char *prog_args[5], **ss = toys.optargs;
  char *masklist ="acew0rmyndDM uox";

  fds.events = POLLIN;

  *prog_args = *toys.optargs;
  prog_args[4] = 0;
  if ((fds.fd = inotify_init()) == -1) perror_exit(0);

  // Track number of watched files. First one was program to run.
  toys.optc--;

  while (*++ss) {
    char *path = *ss, *masks = strchr(*ss, ':');
    int i, mask = 0;

    if (!masks) mask = 0xfff; // default to all
    else{
      for (*masks++ = 0; *masks; masks++) {
        i = stridx(masklist, *masks);;
        if (i == -1) error_exit("bad mask '%c'", *masks);
        mask |= 1<<i;
      }
    }

    // This returns increasing numbers starting from 1, which coincidentally
    // is the toys.optargs position of the file. (0 is program to run.)
    if (inotify_add_watch(fds.fd, path, mask) < 0) perror_exit_raw(path);
  }

  for (;;) {
    int ret = 0, len;
    void *buf = 0;
    struct inotify_event *event;

    // Read next event(s)
    ret = poll(&fds, 1, -1);
    if (ret < 0 && errno == EINTR) continue;
    if (ret <= 0) break;
    xioctl(fds.fd, FIONREAD, &len);
    event = buf = xmalloc(len);
    len = readall(fds.fd, buf, len);

    // Loop through set of events.
    for (;;) {
      int left = len - (((char *)event)-(char *)buf),
          size = sizeof(struct inotify_event);

      // Don't dereference event if ->len is off end of bufer
      if (left >= size) size += event->len;
      if (left < size) break;

      if (event->mask) {
        char *s = toybuf, *m;

        for (m = masklist; *m; m++)
          if (event->mask & (1<<(m-masklist))) *s++ = *m;
        *s = 0;

        if (**prog_args == '-' && !prog_args[0][1]) {
          xprintf("%s\t%s\t%s\n" + 3*!event->len, toybuf,
              toys.optargs[event->wd], event->name);
        } else {
          prog_args[1] = toybuf;
          prog_args[2] = toys.optargs[event->wd];
          prog_args[3] = event->len ? event->name : 0;
          xrun(prog_args);
        }

        if (event->mask & IN_IGNORED) {
          if (--toys.optc <= 0) {
            free(buf);

            goto done;
          }
          inotify_rm_watch(fds.fd, event->wd);
        }
      }
      event = (void*)(size + (char*)event);
    }
    free(buf);
  }

done:
  toys.exitval = !!toys.signal;
}
