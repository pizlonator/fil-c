/* time.c - time a simple command
 *
 * Copyright 2013 Rob Landley <rob@landley.net>
 *
 * See http://pubs.opengroup.org/onlinepubs/9699919799/utilities/time.html

USE_TIME(NEWTOY(time, "<1^pv[-pv]", TOYFLAG_USR|TOYFLAG_BIN|TOYFLAG_MAYFORK))

config TIME
  bool "time"
  default y
  help
    usage: time [-pv] COMMAND...

    Run command line and report real, user, and system time elapsed in seconds.
    (real = clock on the wall, user = cpu used by command's code,
    system = cpu used by OS on behalf of command.)

    -p	POSIX format output
    -v	Verbose
*/

#define FOR_time
#include "toys.h"


void time_main(void)
{
  struct timespec ts, ts2;
  struct rusage ru;
  long long sec[3];
  int stat, ii, idx, nano[3];
  pid_t pid;
  char *labels[] = {"\nreal"+FLAG(p), "user", "sys"}, **label = labels,
       *vlabels[] ={"Real", "User", "System"}, tab = toys.optflags ? ' ' : '\t';

  if (FLAG(v)) label = vlabels;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  if (!(pid = XVFORK())) xexec(toys.optargs);
  wait4(pid, &stat, 0, &ru);
  clock_gettime(CLOCK_MONOTONIC, &ts2);
  sec[0] = nanodiff(&ts, &ts2);
  nano[0] = (sec[0] % 1000000000)/(toys.optflags ? 1000 : 1000000);
  sec[0] /= 1000000000;
  sec[1] = ru.ru_utime.tv_sec, nano[1] = ru.ru_utime.tv_usec;
  sec[2] = ru.ru_stime.tv_sec, nano[2] = ru.ru_stime.tv_usec;
  for (ii = idx = 0; ii<3; ii++)
    idx += sprintf(toybuf+idx, "%s%s%c%lld.%0*d\n", label[ii],
                   FLAG(v) ? " time (s):" : "", tab, sec[ii],
                   6>>!toys.optflags, nano[ii]);
  if (FLAG(v)) idx += sprintf(toybuf+idx,
    "Max RSS (KiB): %ld\nMajor faults: %ld\n"
    "Minor faults: %ld\nFile system inputs: %ld\nFile system outputs: %ld\n"
    "Voluntary context switches: %ld\nInvoluntary context switches: %ld\n",
    ru.ru_maxrss, ru.ru_majflt, ru.ru_minflt, ru.ru_inblock,
    ru.ru_oublock, ru.ru_nvcsw, ru.ru_nivcsw);
  writeall(2, toybuf, idx);

  toys.exitval = WIFEXITED(stat) ? WEXITSTATUS(stat) : WTERMSIG(stat);
}
