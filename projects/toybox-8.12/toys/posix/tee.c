/* tee.c - cat to multiple outputs.
 *
 * Copyright 2008 Rob Landley <rob@landley.net>
 *
 * See http://opengroup.org/onlinepubs/9699919799/utilities/tee.html

USE_TEE(NEWTOY(tee, "ia", TOYFLAG_USR|TOYFLAG_BIN))

config TEE
  bool "tee"
  default y
  help
    usage: tee [-ai] [FILE...]

    Copy stdin to each listed file, and also to stdout.
    Filename "-" is a synonym for stdout.

    -a	Append to files
    -i	Ignore SIGINT
*/

#define FOR_tee
#include "toys.h"

GLOBALS(
  void *outputs;
  int out;
)

struct fd_list {
  struct fd_list *next;
  int fd;
};

// Open each output file, saving filehandles to a linked list.

static void do_tee_open(int fd, char *name)
{
  struct fd_list *temp;

  temp = xmalloc(sizeof(struct fd_list));
  temp->next = TT.outputs;
  if (1 == (temp->fd = fd)) TT.out++;
  TT.outputs = temp;
}

void tee_main(void)
{
  struct fd_list *fdl;
  int len;

  if (FLAG(i)) xsignal(SIGINT, SIG_IGN);

  // Open output files (plus stdout if not already in output list)
  loopfiles_rw(toys.optargs,
    O_RDWR|O_CREAT|WARN_ONLY|(FLAG(a)?O_APPEND:O_TRUNC),
    0666, do_tee_open);
  if (!TT.out) do_tee_open(1, 0);

  // Read data from stdin, write to each output file.
  for (;;) {
    if (1>(len = xread(0, toybuf, sizeof(toybuf)))) break;
    for (fdl = TT.outputs; fdl;fdl = fdl->next)
      if (len != writeall(fdl->fd, toybuf, len)) toys.exitval = 1;
  }
}
