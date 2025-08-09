/* ulimit.c - Modify resource limits
 *
 * Copyright 2015 Rob Landley <rob@landley.net>
 *
 * See http://pubs.opengroup.org/onlinepubs/9699919799/utilities/ulimit.html
 * And man prlimit(2).
 *
 * Deviations from posix: The units on -f are supposed to be 512 byte
 * "blocks" (no other options are specified, and even hard drives don't
 * do that anymore). Bash uses 1024 byte blocks, so they don't care either.
 * We consistently use bytes everywhere we can.
 *
 * Deviations from bash: Sizes are in bytes (instead of -p 512 and -f 1024).
 * Bash's -p value has been wrong since 2010 (git 35f3d14dbbc5).
 * The kernel implementation of RLIMIT_LOCKS (-x) was removed from Linux in
 * 2003. Bash never implemented -b (it's in the help but unrecognized at
 * runtime). We support -P to affect processes other than us.

USE_ULIMIT(NEWTOY(ulimit, ">1P#<1SHavutsrRqpnmlifedc[-SH][!apvutsrRqnmlifedc]", TOYFLAG_USR|TOYFLAG_BIN))
USE_ULIMIT(OLDTOY(prlimit, ulimit, TOYFLAG_USR|TOYFLAG_BIN))

config ULIMIT
  bool "ulimit"
  default y
  help
    usage: ulimit [-P PID] [-SHRacdefilmnpqrstuv] [LIMIT]

    Print or set resource limits for process number PID. If no LIMIT specified
    (or read-only -ap selected) display current value (sizes in bytes).
    Default is ulimit -P $PPID -Sf" (show soft filesize of your shell).

    -P  PID to affect (default $PPID)  -a  Show all limits
    -S  Set/show soft limit            -H  Set/show hard (maximum) limit

    -c  Core file size (blocks)        -d  Process data segment (KiB)
    -e  Max scheduling priority        -f  File size (KiB)
    -i  Pending signal count           -l  Locked memory (KiB)
    -m  Resident Set Size (KiB)        -n  Number of open files
    -p  Pipe buffer (512 bytes)        -q  POSIX message queues
    -r  Max realtime priority          -R  Realtime latency (us)
    -s  Stack size (KiB)               -t  Total CPU time (s)
    -u  Maximum processes (this UID)   -v  Virtual memory size (KiB)
*/

#define FOR_ulimit
#include "toys.h"

GLOBALS(
  long P;
)

// This is a linux kernel syscall added in 2.6.36 (git c022a0acad53) which
// glibc only exports a wrapper prototype for if you #define _FSF_HURD_RULZE.
int prlimit(pid_t pid, int resource, const struct rlimit *new_limit,
  struct rlimit *old_limit);

// I'd like to sort the RLIMIT values 0-15, but mips, alpha and sparc
// override the asm-generic values for 5-9, and alphabetical order is nice for
// humans anyway.
void ulimit_main(void)
{
  struct rlimit rr;
  int i;
  // map and desc are in the same order as flags.
  // The Linux kernel implementation of RLIMIT_LOCKS (-x) was removed in 2003.
  char *flags="cdefilmnpqRrstuv";
  char map[] = {RLIMIT_CORE, RLIMIT_DATA, RLIMIT_NICE,
    RLIMIT_FSIZE, RLIMIT_SIGPENDING, RLIMIT_MEMLOCK,
    RLIMIT_RSS, RLIMIT_NOFILE, 0, RLIMIT_MSGQUEUE,
    RLIMIT_RTTIME, RLIMIT_RTPRIO, RLIMIT_STACK, RLIMIT_CPU, RLIMIT_NPROC,
    RLIMIT_AS};
  char *desc[]={"core dump size (blocks)", "data size (KiB)", "max nice",
    "file size (blocks)", "pending signals", "locked memory (KiB)",
    "RSS (KiB)", "open files", "pipe size (512 bytes)", "message queues",
    "RT time (us)", "RT priority", "stack (KiB)", "cpu time (s)", "processes",
    "address space (KiB)"};

  if (!(toys.optflags&(FLAG_H-1))) toys.optflags |= FLAG_f;
  if ((FLAG(a)||FLAG(p)) && toys.optc) error_exit("can't set -ap");

  // Fetch data
  if (!FLAG(P)) TT.P = getppid();

  for (i=0; i<sizeof(map); i++) {
    int get = toys.optflags&(FLAG_a|(1<<i));

    if (get && prlimit(TT.P, map[i], 0, &rr)) perror_exit("-%c", flags[i]);
    if (!toys.optc) {
      if (FLAG(a)) printf("-%c: %-25s ", flags[i], desc[i]);
      if (get) {
        if ((1<<i)&FLAG_p) {
          if (FLAG(H))
            xreadfile("/proc/sys/fs/pipe-max-size", toybuf, sizeof(toybuf));
          else {
            int pp[2];

            xpipe(pp);
            sprintf(toybuf, "%d\n", fcntl(*pp, F_GETPIPE_SZ));
          }
          printf("%s", toybuf);
        } else {
          rlim_t rl = FLAG(H) ? rr.rlim_max : rr.rlim_cur;

          if (rl == RLIM_INFINITY) printf("unlimited\n");
          else printf("%ld\n", (long)rl);
        }
      }
    }
    if (toys.optflags&(1<<i)) break;
  }

  if (FLAG(a)||FLAG(p)) return;

  if (toys.optc) {
    rlim_t val;

    if (tolower(**toys.optargs) == 'u') val = RLIM_INFINITY;
    else val = atolx_range(*toys.optargs, 0, LONG_MAX);

    if (FLAG(H)) rr.rlim_max = val;
    else rr.rlim_cur = val;
    if (prlimit(TT.P, map[i], &rr, 0)) perror_exit(0);
  }
}
