/* klogd.c - Klogd, The kernel log Dameon.
 *
 * Copyright 2013 Sandeep Sharma <sandeep.jack2756@gmail.com>
 * Copyright 2013 Kyungwan Han <asura321@gmail.com>
 *
 * No standard

USE_KLOGD(NEWTOY(klogd, "c#<1>8ns", TOYFLAG_SBIN))

config KLOGD
  bool "klogd"
  default n
  help
  usage: klogd [-n] [-c PRIORITY]

  Forward messages from the kernel ring buffer (read by dmesg) to syslogd.

  -c	Print to console messages more urgent than PRIORITY (1-8)
  -n	Run in foreground
  -s	Use syscall instead of /proc
*/

#define FOR_klogd
#include "toys.h"
#include <sys/klog.h>

GLOBALS(
  long level;

  int fd;
)

static void set_log_level(int level)
{
  if (FLAG(s)) klogctl(8, 0, level);
  else {
    FILE *fptr = xfopen("/proc/sys/kernel/printk", "w");

    fprintf(fptr, "%u\n", level);
    fclose(fptr);
  }
}

static void handle_signal(int sig)
{
  // TODO: level 7 hardwired? How to read old value...?
  if (FLAG(s)) klogctl(8, 0, 7);
  else {
    if (FLAG(c)) set_log_level(7);
    xclose(TT.fd);
  }
  syslog(LOG_NOTICE, "KLOGD: Daemon exiting......");

  toys.exitval = 1;
  xexit();
}

// Read kernel ring buffer in local buff and keep track of
// "used" amount to track next read to start.
void klogd_main(void)
{
  int prio, size, used = 0;
  char *start, *line_start;

  if (!FLAG(n)) xvdaemon();
  sigatexit(handle_signal);
  if (FLAG(c)) set_log_level(TT.level);    //set log level

  if (!FLAG(s)) TT.fd = xopenro("/proc/kmsg"); //_PATH_KLOG in paths.h
  syslog(LOG_NOTICE, "KLOGD: started with %s as log source\n",
    FLAG(s) ? "Kernel ring buffer" : "/proc/kmsg");
  openlog("Kernel", 0, LOG_KERN);    //open connection to system logger..

  for (;;) {
    start = toybuf + used; //start updated for re-read.
    size = sizeof(toybuf)-used-1;
    if (FLAG(s)) size = klogctl(2, start, size);
    else size = xread(TT.fd, start, size);
    if (size < 0) perror_exit("error reading file:");
    start[size] = 0;
    if (used) start = toybuf;
    while (start) {
      if ((line_start = strsep(&start, "\n")) && start) used = 0;
      else {      //Incomplete line, copy it to start of buf
        used = stpcpy(toybuf, line_start)-toybuf;
        if (used < (sizeof(toybuf) - 1)) break;
        used = 0; //we have buffer full, log it as it is.
      }
      prio = LOG_INFO;  //we dont know priority, mark it INFO
      if (*line_start == '<') {  //we have new line to syslog
        line_start++;
        if (line_start) prio = strtoul(line_start, &line_start, 10);
        if (*line_start == '>') line_start++;
      }
      if (*line_start) syslog(prio, "%s", line_start);
    }
  }
}
