/* w.c - shows logged in users
 *
 * Copyright 2012 Gaurang Shastri <gmshastri@gmail.com>

USE_W(NEWTOY(w, NULL, TOYFLAG_USR|TOYFLAG_BIN))

config W
  bool "w"
  default y
  help
    usage: w

    Show who is logged on and since how long they logged in.
*/

#include "toys.h"

void w_main(void)
{
  struct utmpx *x;

  xprintf("USER     TTY             LOGIN@              FROM");
  setutxent();
  while ((x=getutxent()) != NULL) {
    if (x->ut_type==7) {
      time_t tt = x->ut_tv.tv_sec;

      xprintf("\n%-9.8s%-9.8s %-4.24s (%-1.12s)", x->ut_user, x->ut_line,
        ctime(&tt), x->ut_host);
    }
  }
  xputc('\n');
}
