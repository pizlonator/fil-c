/* killall.c - Send signal (default: TERM) to all processes with given names.
 *
 * Copyright 2012 Andreas Heck <aheck@gmx.de>
 *
 * http://refspecs.linuxfoundation.org/LSB_4.1.0/LSB-Core-generic/LSB-Core-generic/killall.html

USE_KILLALL(NEWTOY(killall, "?s:ilqvw", TOYFLAG_USR|TOYFLAG_BIN))

config KILLALL
  bool "killall"
  default y
  help
    usage: killall [-l] [-iqv] [-SIGNAL|-s SIGNAL] PROCESS_NAME...

    Send a signal (default: TERM) to all processes with the given names.

    -i	Ask for confirmation before killing
    -l	Print list of all available signals
    -q	Don't print any warnings or error messages
    -s	Send SIGNAL instead of SIGTERM
    -v	Report if the signal was successfully sent
    -w	Wait until all signaled processes are dead
*/

#define FOR_killall
#include "toys.h"

GLOBALS(
  char *s;

  int signum;
  pid_t cur_pid;
  char **names;
  short *err;
  struct int_list { struct int_list *next; int val; } *pids;
)

static int kill_process(pid_t pid, char *name)
{
  int offset = 0;

  if (pid == TT.cur_pid) return 0;

  if (FLAG(i)) {
    fprintf(stderr, "Signal %s(%d)", name, (int)pid);
    if (!yesno(0)) return 0;
  }

  errno = 0;
  kill(pid, TT.signum);
  if (FLAG(w)) {
    struct int_list *new = xmalloc(sizeof(*TT.pids));

    new->val = pid;
    new->next = TT.pids;
    TT.pids = new;
  }
  for (;;) {
    if (TT.names[offset] == name) {
      TT.err[offset] = errno;
      break;
    } else offset++;
  }
  if (errno) {
    if (!FLAG(q)) perror_msg("pid %d", (int)pid);
  } else if (FLAG(v))
    printf("Killed %s(%d) with signal %d\n", name, pid, TT.signum);

  return 0;
}

void killall_main(void)
{
  int i;

  TT.names = toys.optargs;
  TT.signum = SIGTERM;

  if (FLAG(l)) {
    list_signals();
    return;
  }

  if (TT.s || (*TT.names && **TT.names == '-')) {
    if (0 > (TT.signum = sig_to_num(TT.s ? TT.s : (*TT.names)+1))) {
      if (FLAG(q)) exit(1);
      error_exit("Invalid signal");
    }
    if (!TT.s) {
      TT.names++;
      toys.optc--;
    }
  }

  if (!toys.optc) help_exit("no name");

  TT.cur_pid = getpid();

  TT.err = xmalloc(2*toys.optc);
  for (i=0; i<toys.optc; i++) TT.err[i] = ESRCH;
  names_to_pid(TT.names, kill_process, 1);
  for (i=0; i<toys.optc; i++) {
    if (TT.err[i]) {
      toys.exitval = 1;
      errno = TT.err[i];
      perror_msg_raw(TT.names[i]);
    }
  }
  if (FLAG(w)) {
    for (;;) {
      struct int_list *p = TT.pids;
      int c = 0;

      for (; p; p=p->next) if (kill(p->val, 0) != -1 || errno != ESRCH) ++c;
      if (!c) break;
      sleep(1);
    }
  }
  if (CFG_TOYBOX_FREE) {
    free(TT.err);
    llist_traverse(TT.pids, free);
  }
}
