/* sendevent.c - Send Linux input events.
 *
 * Copyright 2016 The Android Open Source Project

USE_SENDEVENT(NEWTOY(sendevent, "<4>4", TOYFLAG_USR|TOYFLAG_SBIN))

config SENDEVENT
  bool "sendevent"
  default y
  depends on TOYBOX_ON_ANDROID
  help
    usage: sendevent DEVICE TYPE CODE VALUE

    Sends a Linux input event.
*/

#define FOR_sendevent
#include "toys.h"

#include <linux/input.h>

void sendevent_main(void)
{
  int fd = xopen(*toys.optargs, O_RDWR);
  int version;
  struct input_event ev;

  if (ioctl(fd, EVIOCGVERSION, &version))
    perror_exit("EVIOCGVERSION failed for %s", *toys.optargs);

  memset(&ev, 0, sizeof(ev));
  // TODO: error checking and support for named constants.
  ev.type = atoi(toys.optargs[1]);
  ev.code = atoi(toys.optargs[2]);
  ev.value = atoi(toys.optargs[3]);
  xwrite(fd, &ev, sizeof(ev));
}
