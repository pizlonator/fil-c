/* who.c - display who is on the system
 *
 * Copyright 2012 ProFUSION Embedded Systems
 *
 * by Luis Felipe Strano Moraes <lfelipe@profusion.mobi>
 *
 * See http://opengroup.org/onlinepubs/9699919799/utilities/who.html
 *
 * Posix says to support many options (-abdHlmpqrstTu) but this
 * isn't aimed at minicomputers with modem pools.
 *
 * TODO: -a doesn't know how to format other entries

USE_WHO(NEWTOY(who, "a", TOYFLAG_USR|TOYFLAG_BIN))

config WHO
  bool "who"
  default y
  help
    usage: who

    Print information about logged in users.
*/

#define FOR_who
#include "toys.h"

void who_main(void)
{
  struct utmpx *entry;

  setutxent();
  while ((entry = getutxent())) {
    if (FLAG(a) || entry->ut_type == USER_PROCESS) {
      time_t t = entry->ut_tv.tv_sec;
      struct tm *tm = localtime(&t);

      strftime(toybuf, sizeof(toybuf), "%F %H:%M", tm);
      printf("%s\t%s\t%s (%s)\n", entry->ut_user, entry->ut_line,
        toybuf, entry->ut_host);
    }
  }
  endutxent();
}
