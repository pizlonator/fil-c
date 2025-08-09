/* rfkill.c - Enable/disable wireless devices.
 *
 * Copyright 2014 Ranjan Kumar <ranjankumar.bth@gmail.com>
 * Copyright 2014 Kyungwan Han <asura321@gmail.com>
 *
 * No Standard

USE_RFKILL(NEWTOY(rfkill, "<1>2", TOYFLAG_USR|TOYFLAG_SBIN))

config RFKILL
  bool "rfkill"
  default y
  help
    usage: rfkill COMMAND [DEVICE]

    Enable/disable wireless devices.

    Commands:
    list [DEVICE]   List current state
    block DEVICE    Disable device
    unblock DEVICE  Enable device

    DEVICE is an index number, or one of:
    all, wlan(wifi), bluetooth, uwb(ultrawideband), wimax, wwan, gps, fm.
*/

#define FOR_rfkill
#include "toys.h"
#include <linux/rfkill.h>

void rfkill_main(void)
{
  struct rfkill_event rfevent;
  int fd, tvar, idx = -1, tid = RFKILL_TYPE_ALL;
  char **optargs = toys.optargs;

  // Parse command line options
  for (tvar = 0; tvar < 3; tvar++)
    if (!strcmp((char *[]){"list", "block", "unblock"}[tvar], *optargs)) break;
  if (tvar == 3) error_exit("unknown cmd '%s'", *optargs);
  if (tvar) {
    int i;
    struct arglist {
      char *name;
      int idx;
    } rftypes[] = {{"all", RFKILL_TYPE_ALL}, {"wifi", RFKILL_TYPE_WLAN},
      {"wlan", RFKILL_TYPE_WLAN}, {"bluetooth", RFKILL_TYPE_BLUETOOTH},
      {"uwb", RFKILL_TYPE_UWB}, {"ultrawideband", RFKILL_TYPE_UWB},
      {"wimax", RFKILL_TYPE_WIMAX}, {"wwan", RFKILL_TYPE_WWAN},
      {"gps", RFKILL_TYPE_GPS}, {"fm", 7}}; // RFKILL_TYPE_FM = 7

    if (!*++optargs) error_exit("'%s' needs IDENTIFIER", optargs[-1]);
    for (i = 0; i < ARRAY_LEN(rftypes); i++)
      if (!strcmp(rftypes[i].name, *optargs)) break;
    if (i == ARRAY_LEN(rftypes)) idx = atolx_range(*optargs, 0, INT_MAX);
    else tid = rftypes[i].idx;
  }

  // Perform requested action
  fd = xopen("/dev/rfkill", (tvar ? O_RDWR : O_RDONLY)|O_NONBLOCK);
  if (tvar) {
    // block/unblock
    memset(&rfevent, 0, sizeof(rfevent));
    rfevent.soft = tvar == 1;
    if (idx >= 0) {
      rfevent.idx = idx;
      rfevent.op = RFKILL_OP_CHANGE;
    } else {
      rfevent.type = tid;
      rfevent.op = RFKILL_OP_CHANGE_ALL;
    }
    xwrite(fd, &rfevent, sizeof(rfevent));
  } else {
    // show list.
    while (sizeof(rfevent) == readall(fd, &rfevent, sizeof(rfevent))) {
      char *line = 0, *name = 0, *type = 0;
      FILE *fp;
      size_t l = 0;
      ssize_t len;

      // filter list items
      if ((tid > 0 && tid != rfevent.type) || (idx != -1 && idx != rfevent.idx))
        continue;

      sprintf(toybuf, "/sys/class/rfkill/rfkill%u/uevent", rfevent.idx);
      fp = xfopen(toybuf, "r");
      while ((len = getline(&line, &l, fp)) > 0) {
        char *s = line;

        line[len-1] = 0;
        if (strstart(&s, "RFKILL_NAME=")) name = xstrdup(s);
        else if (strstart(&s, "RFKILL_TYPE=")) type = xstrdup(s);
      }
      free(line);
      fclose(fp);

      xprintf("%u: %s: %s\n", rfevent.idx, name, type);
      xprintf("\tSoft blocked: %s\n", rfevent.soft ? "yes" : "no");
      xprintf("\tHard blocked: %s\n", rfevent.hard ? "yes" : "no");
      free(name);
      free(type);
    }
  }
  xclose(fd);
}
