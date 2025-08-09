/* tac.c - output lines in reverse order
 *
 * Copyright 2012 Rob Landley <rob@landley.net>

USE_TAC(NEWTOY(tac, NULL, TOYFLAG_USR|TOYFLAG_BIN))

config TAC
  bool "tac"
  default y
  help
    usage: tac [FILE...]

    Output lines in reverse order.
*/

#define FOR_tac
#include "toys.h"

GLOBALS(
  struct double_list *dl;
)

static void do_tac(char **pline, long len)
{
  if (pline) {
    dlist_add(&TT.dl, *pline);
    *pline = 0;
  } else while (TT.dl) {
    struct double_list *dl = dlist_lpop(&TT.dl);

    xprintf("%s", dl->data);
    free(dl->data);
    free(dl);
  }
}

void tac_main(void)
{
  loopfiles_lines(toys.optargs, do_tac);
}
