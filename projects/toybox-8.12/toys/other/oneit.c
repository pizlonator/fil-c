/* oneit.c - tiny init replacement to launch a single child process.
 *
 * Copyright 2005, 2007 by Rob Landley <rob@landley.net>.

USE_ONEIT(NEWTOY(oneit, "^<1nc:p3[!pn]", TOYFLAG_SBIN))

config ONEIT
  bool "oneit"
  default y
  help
    usage: oneit [-prn3] [-c CONSOLE] [COMMAND...]

    Simple init program that runs a single supplied command line with a
    controlling tty (so CTRL-C can kill it).

    -c	Which console device to use (/dev/console doesn't do CTRL-C, etc)
    -p	Power off instead of rebooting when command exits
    -r	Restart child when it exits
    -n	No reboot, just relaunch command line
    -3	Write 32 bit PID of each exiting reparented process to fd 3 of child
    	(Blocking writes, child must read to avoid eventual deadlock.)

    Spawns a single child process (because PID 1 has signals blocked)
    in its own session, reaps zombies until the child exits, then
    reboots the system (or powers off with -p, or restarts the child with -r).

    Responds to SIGUSR1 by halting the system, SIGUSR2 by powering off,
    and SIGTERM or SIGINT reboot.
*/

#define FOR_oneit
#include "toys.h"
#include <sys/reboot.h>

GLOBALS(
  char *c;
)

// The minimum amount of work necessary to get ctrl-c and such to work is:
//
// - Fork a child (PID 1 is special: can't exit, has various signals blocked).
// - Do a setsid() (so we have our own session).
// - In the child, attach stdio to TT.c (/dev/console is special)
// - Exec the rest of the command line.
//
// PID 1 then reaps zombies until the child process it spawned exits, at which
// point it calls sync() and reboot().  I could stick a kill -1 in there.

// Perform actions in response to signals. (Only root can send us signals.)
static void oneit_signaled(int signal)
{
  int action = RB_AUTOBOOT;

  toys.signal = signal;
  if (signal == SIGUSR1) action = RB_HALT_SYSTEM;
  if (signal == SIGUSR2) action = RB_POWER_OFF;

  // PID 1 can't call reboot() because it kills the task that calls it,
  // which causes the kernel to panic before the actual reboot happens.
  sync();
  if (getpid()!=1) _exit(127+signal);
  if (!vfork()) reboot(action);
}

void oneit_main(void)
{
  int i, pid, pipes[] = {SIGUSR1, SIGUSR2, SIGTERM, SIGINT};
  char *ss = toybuf+5;

  // Setup signal handlers for signals of interest
  for (i = 0; i<ARRAY_LEN(pipes); i++) xsignal(pipes[i], oneit_signaled);

  // Autodetect console from sysfs if no -c
  memcpy(toybuf, "/dev/", 5);
  i = sizeof(toybuf)-6;
  if (!TT.c && (TT.c = readfile("/sys/class/tty/console/active", ss, i))) {
    // Take last entry, remove newline terminator
    while (TT.c[i = strcspn(TT.c, " \n")]) {
      TT.c[i++] = 0;
      if (TT.c[i]) TT.c += i;
      else break;
    }
    // Ensure exactly one /dev prefix
    strstart(&TT.c, "/dev/");
    memmove(toybuf+5, TT.c, strlen(TT.c));
    TT.c = toybuf;
  }

  // Redirect stdin/out/err. Remember, O_CLOEXEC is backwards for xopen()
  close(0);
  xopen_stdio(TT.c ? : "/dev/tty0", O_RDWR|O_CLOEXEC);
  close(1);
  (void)dup(0);
  close(2);
  (void)dup(0);

  if (FLAG(3)) {
    // Ensure next available filehandles are #3 and #4
    close(3);
    close(4);
    xpipe(pipes);
    fcntl(4, F_SETFD, FD_CLOEXEC);
  }

  while (!toys.signal) {

    // Create a new child process.
    pid = XVFORK();
    if (pid) {

      // pid 1 reaps zombies until it gets its child, then halts system.
      // We ignore the return value of write (what would we do with it?)
      // but save it in a variable we never read to make fortify shut up.
      // (Real problem is if pid2 never reads, write() fills pipe and blocks.)
      while (pid != wait(&i)) if (FLAG(3)) i = write(4, &pid, 4);
      if (FLAG(n)) continue;

      oneit_signaled(FLAG(p) ? SIGUSR2 : SIGTERM);
    } else {
      // new session ID in child, so ctrl-c works.
      setsid();

      // Can't xexec() here, we vforked so we don't want to error_exit().
      toy_exec(toys.optargs);
      execvp(*toys.optargs, toys.optargs);
      perror_msg("%s not in PATH=%s", *toys.optargs, getenv("PATH"));

      break;
    }
  }

  // Give reboot() time to kick in, or avoid rapid spinning if exec failed
  sleep(5);
  _exit(127);
}
