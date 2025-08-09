/* nsenter.c - Enter existing namespaces
 *
 * Copyright 2014 Andy Lutomirski <luto@amacapital.net>
 *
 * See http://man7.org/linux/man-pages/man1/nsenter.1.html
 *
 * unshare.c - run command in new context
 *
 * Copyright 2011 Rob Landley <rob@landley.net>
 *
 * See http://man7.org/linux/man-pages/man1/unshare.1.html
 *

// Note: flags go in same order (right to left) for shared subset
USE_NSENTER(NEWTOY(nsenter, "<1a(all)F(no-fork)t#<1(target)C(cgroup):; i(ipc):; m(mount):; n(net):; p(pid):; u(uts):; U(user):; ", TOYFLAG_USR|TOYFLAG_BIN|TOYFLAG_STAYROOT))
USE_UNSHARE(NEWTOY(unshare, "<1^a(all)f(fork)r(map-root-user)C(cgroup):; i(ipc):; m(mount):; n(net):; p(pid):; u(uts):; U(user):; ", TOYFLAG_USR|TOYFLAG_BIN|TOYFLAG_STAYROOT))

config UNSHARE
  bool "unshare"
  default y
  help
    usage: unshare [-imnpuUr] COMMAND...

    Create new container namespace(s) for this process and its children, allowing
    the new set of processes to have a different view of the system than the
    parent process.

    -a	Unshare all supported namespaces
    -f	Fork command in the background (--fork)
    -r	Become root (map current euid/egid to 0/0, implies -U) (--map-root-user)

    Available namespaces:
    -C	Control groups (--cgroup)
    -i	SysV IPC (message queues, semaphores, shared memory) (--ipc)
    -m	Mount/unmount tree (--mount)
    -n	Network address, sockets, routing, iptables (--net)
    -p	Process IDs and init (--pid)
    -u	Host and domain names (--uts)
    -U	UIDs, GIDs, capabilities (--user)

    Each namespace can take an optional argument, a persistent mountpoint usable
    by the nsenter command to add new processes to that the namespace. (Specify
    multiple namespaces to unshare separately, ala -c -i -m because -cim is -c
    with persistent mount "im".)

config NSENTER
  bool "nsenter"
  default y
  help
    usage: nsenter [-t pid] [-F] [-i] [-m] [-n] [-p] [-u] [-U] COMMAND...

    Run COMMAND in an existing (set of) namespace(s).

    -a	Enter all supported namespaces (--all)
    -F	don't fork, even if -p is used (--no-fork)
    -t	PID to take namespaces from    (--target)

    The namespaces to switch are:

    -C	Control groups (--cgroup)
    -i	SysV IPC: message queues, semaphores, shared memory (--ipc)
    -m	Mount/unmount tree (--mount)
    -n	Network address, sockets, routing, iptables (--net)
    -p	Process IDs and init, will fork unless -F is used (--pid)
    -u	Host and domain names (--uts)
    -U	UIDs, GIDs, capabilities (--user)

    If -t isn't specified, each namespace argument must provide a path
    to a namespace file, ala "-i=/proc/$PID/ns/ipc"
*/

#define FOR_nsenter
#include "toys.h"
#include <linux/sched.h>

#define unshare(flags) syscall(SYS_unshare, flags)
#define setns(fd, nstype) syscall(SYS_setns, fd, nstype)

GLOBALS(
  char *UupnmiC[7];
  long t;
)

// Code that must run in unshare's flag context
#define FOR_unshare
#include <generated/flags.h>

static void write_ugid_map(char *map, unsigned eugid)
{
  int fd = xopen(map, O_WRONLY);

  dprintf(fd, "0 %u 1", eugid);
  xclose(fd);
}

static int test_a() { return FLAG(a); }
static int test_r() { return FLAG(r); }
static int test_f() { return FLAG(f); }

// Shift back to the context GLOBALS lives in (I.E. matching the filename).
#define FOR_nsenter
#include <generated/flags.h>

void unshare_main(void)
{
  char *nsnames = "user\0uts\0pid\0net\0mnt\0ipc\0cgroup";
  unsigned flags[]={CLONE_NEWUSER, CLONE_NEWUTS, CLONE_NEWPID, CLONE_NEWNET,
                    CLONE_NEWNS, CLONE_NEWIPC, CLONE_NEWCGROUP}, f = 0;
  int i, fd;

  // Create new namespace(s)?
  if (CFG_UNSHARE && *toys.which->name=='u') {
    // For -r, we have to save our original [ug]id before calling unshare()
    int euid = geteuid(), egid = getegid();

    // unshare -U does not imply -r, so we cannot use [+rU]
    if (test_r()) toys.optflags |= FLAG_U;

    for (i = 0; i<ARRAY_LEN(flags); i++)
      if (test_a() || (toys.optflags & (1<<i))) f |= flags[i];
    if (unshare(f)) perror_exit(0);
    if (test_r()) {
      if ((fd = open("/proc/self/setgroups", O_WRONLY)) >= 0) {
        xwrite(fd, "deny", 4);
        close(fd);
      }

      write_ugid_map("/proc/self/uid_map", euid);
      write_ugid_map("/proc/self/gid_map", egid);
    }

    if (test_f()) {
      toys.exitval = xrun(toys.optargs);

      return;
    }
  // Bind to existing namespace(s)?
  } else if (CFG_NSENTER) {
    for (i = 0; i<ARRAY_LEN(flags); i++, nsnames += strlen(nsnames)+1) {
      if (FLAG(a) || (toys.optflags & (1<<i))) {
        char *filename = TT.UupnmiC[i];

        if (!filename || !*filename) {
          if (!FLAG(t)) error_exit("need -t or =filename");
          sprintf(filename = toybuf, "/proc/%ld/ns/%s", TT.t, nsnames);
        }

        if (setns(fd = xopenro(filename), flags[i])) perror_exit("setns");
        close(fd);
      }
    }

    if (FLAG(p) && !FLAG(F)) {
      toys.exitval = xrun(toys.optargs);

      return;
    }
  }

  xexec(toys.optargs);
}

void nsenter_main(void)
{
  unshare_main();
}
