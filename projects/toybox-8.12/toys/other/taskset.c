/* taskset.c - Retrieve or set the CPU affinity of a process.
 *
 * Copyright 2012 Elie De Brauwer <eliedebrauwer@gmail.com>

USE_TASKSET(NEWTOY(taskset, "<1^pa", TOYFLAG_USR|TOYFLAG_BIN))
USE_NPROC(NEWTOY(nproc, "(all)", TOYFLAG_USR|TOYFLAG_BIN))

config NPROC
  bool "nproc"
  default y
  help
    usage: nproc [--all]

    Print number of processors.

    --all	Show all processors, not just ones this task can run on

config TASKSET
  bool "taskset"
  default y
  help
    usage: taskset [-ap] [mask] [PID | cmd [args...]]

    Launch a new task which may only run on certain processors, or change
    the processor affinity of an existing PID.

    Mask is a hex string where each bit represents a processor the process
    is allowed to run on. PID without a mask displays existing affinity.

    -p	Set/get the affinity of given PID instead of a new command
    -a	Set/get the affinity of all threads of the PID
*/

#define FOR_taskset
#include "toys.h"

// mask is array of long which makes layout a bit weird on big endian systems
#define sched_setaffinity(pid, size, cpuset) \
  syscall(__NR_sched_setaffinity, (pid_t)pid, (size_t)size, (void *)cpuset)
#define sched_getaffinity(pid, size, cpuset) \
  syscall(__NR_sched_getaffinity, (pid_t)pid, (size_t)size, (void *)cpuset)

static void do_taskset(pid_t pid, int quiet)
{
  unsigned long *mask = (unsigned long *)toybuf;
  char *s, *failed = "failed to %s pid %d's affinity";
  int i, j, k;

  // loop through twice to display before/after affinity masks
  for (i=0; ; i++) {
    if (!quiet) {
      if (-1 == sched_getaffinity(pid, sizeof(toybuf), (void *)mask))
        perror_exit(failed, "get", pid);

      printf("pid %d's %s affinity mask: ", pid, i ? "new" : "current");

      for (j = sizeof(toybuf)/sizeof(long), k = 0; --j>=0;) {
        if (k) printf("%0*lx", (int)(2*sizeof(long)), mask[j]);
        else if (mask[j]) {
          k++;
          printf("%lx", mask[j]);
        }
      }
      putchar('\n');
    }

    if (i || toys.optc < 2) return;

    // Convert hex strong to mask[] bits
    memset(toybuf, 0, sizeof(toybuf));
    k = strlen(s = *toys.optargs);
    s += k;
    for (j = 0; j<k; j++) {
      unsigned long digit = *(--s) - '0';

      if (digit > 9) digit = 10 + tolower(*s)-'a';
      if (digit > 15) error_exit("bad mask '%s'", *toys.optargs);
      mask[j/(2*sizeof(long))] |= digit << 4*(j&((2*sizeof(long))-1));
    }

    if (-1 == sched_setaffinity(pid, sizeof(toybuf), (void *)mask))
      perror_exit(failed, "set", pid);
  }
}

static int task_callback(struct dirtree *new)
{
  if (!new->parent) return DIRTREE_RECURSE|DIRTREE_SHUTUP|DIRTREE_PROC;
  do_taskset(atoi(new->name), 0);

  return 0;
}

void taskset_main(void)
{
  if (!FLAG(p)) {
    if (toys.optc < 2) error_exit("Needs 2 args");
    do_taskset(getpid(), 1);
    xexec(toys.optargs+1);
  } else {
    char *c, buf[33];
    pid_t pid = strtol(toys.optargs[toys.optc-1], &c, 10);

    if (*c) error_exit("Not int %s", toys.optargs[toys.optc-1]);

    if (FLAG(a)) {
      sprintf(buf, "/proc/%ld/task/", (long)pid);
      dirtree_read(buf, task_callback);
    } else do_taskset(pid, 0);
  }
}

void nproc_main(void)
{
  unsigned i, j, nproc = 0;
  DIR *dd;

  // This can only detect 32768 processors. Call getaffinity and count bits.
  if (!toys.optflags && -1!=sched_getaffinity(getpid(), 4096, toybuf))
    for (i = 0; i<4096; i++)
      if (toybuf[i]) for (j=0; j<8; j++) if (toybuf[i]&(1<<j)) nproc++;

  // If getaffinity failed or --all, count cpu entries in sysfs
  // (/proc/cpuinfo filters out hot-unplugged CPUs, sysfs doesn't)
  if (!nproc && (dd = opendir("/sys/devices/system/cpu"))) {
    struct dirent *de;
    char *ss;

    while ((de = readdir(dd))) {
      if (smemcmp(de->d_name, "cpu", 3)) continue;
      for (ss = de->d_name+3; isdigit(*ss); ss++);
      if (!*ss) nproc++;
    }
    closedir(dd);
  }

  xprintf("%u\n", nproc ? : 1);
}
