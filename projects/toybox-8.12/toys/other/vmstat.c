/* vmstat.c - Report virtual memory statistics.
 *
 * Copyright 2012 Elie De Brauwer <eliedebrauwer@gmail.com>
 *
 * TODO: I have no idea how "system" category is calculated.
 * whatever we're doing isn't matching what other implementations are doing.

USE_VMSTAT(NEWTOY(vmstat, ">2n", TOYFLAG_BIN|TOYFLAG_LINEBUF))

config VMSTAT
  bool "vmstat"
  default y
  help
    usage: vmstat [-n] [DELAY [COUNT]]

    Print virtual memory statistics, repeating each DELAY seconds, COUNT times.
    (With no DELAY, prints one line. With no COUNT, repeats until killed.)

    Show processes running and blocked, kilobytes swapped, free, buffered, and
    cached, kilobytes swapped in and out per second, file disk blocks input and
    output per second, interrupts and context switches per second, percent
    of CPU time spent running user code, system code, idle, and awaiting I/O.
    First line is since system started, later lines are since last line.

    -n	Display the header only once
*/

#define FOR_vmstat
#include "toys.h"

struct vmstat_proc {
  unsigned long long
    // From /proc/stat (jiffies) 0-10
    user, nice, sys, idle, wait, irq, sirq, intr, ctxt, running, blocked,
    // From /proc/meminfo (units are kb) 11-16
    memfree, buffers, cached, swapfree, swaptotal, reclaimable,
    // From /proc/vmstat (units are kb) 17-18
    io_in, io_out,
    // From /proc/vmstat (units are pages) 19-20
    swap_in, swap_out;
};

// All the elements of vmstat_proc are the same size, so we can populate it as
// a big array, then read the elements back out by name
static void get_vmstat_proc(struct vmstat_proc *vmsp)
{
  char *vmstuff[] = { "/proc/stat", "cpu ", 0, 0, 0, 0, 0, 0, "intr ", "ctxt ",
    "procs_running ", "procs_blocked ", "/proc/meminfo", "MemFree:",
    "Buffers:", "Cached:", "SwapFree:", "SwapTotal:", "SReclaimable:",
    "/proc/vmstat", "pgpgin ", "pgpgout ", "pswpin ", "pswpout " };
  unsigned long long *new = (void *)vmsp;
  char *p = 0, *name = name, *file = 0;
  int i, j;

  // We use vmstuff to fill out vmstat_proc as an array of long long:
  //   Strings starting with / are the file to find next entries in
  //   Any other string is a key to search for, with decimal value right after
  //   0 means parse another value on same line as last key

  memset(new, 0, sizeof(struct vmstat_proc));
  for (i = j = 0; i<ARRAY_LEN(vmstuff); i++) {
    if (!vmstuff[i]) p++; // Read next entry on same line
    else if (*vmstuff[i] == '/') {
      free(file);
      file = xreadfile(name = vmstuff[i], 0, 0);

      continue;
    } else if (file && !(p = strafter(file, vmstuff[i]))) {
      free(file);
      file = 0;
    }
    if (!file) new++;
    else if (1==sscanf(p, "%llu%n", new++, &j)) p += j;
  }
  free(file);

  // combine some fields we display as aggregates
  vmsp->running--; // Don't include ourselves
  vmsp->user += vmsp->nice;
  vmsp->sys += vmsp->irq + vmsp->sirq;
  vmsp->swaptotal -= vmsp->swapfree;
  vmsp->cached += vmsp->reclaimable;
}

void vmstat_main(void)
{
  int i, loop_delay = 0, loop_max = 0;
  unsigned loop, rows = 25, page_kb = sysconf(_SC_PAGESIZE)/1024;
  unsigned long long units, total_hz, *ptr, *oldptr;
  char *headers = "r\0b\0swpd\0free\0buff\0cache\0si\0so\0bi\0bo\0in\0cs\0us\0"
                  "sy\0id\0wa", lengths[] = {2,2,7,7,6,7,4,4,5,5,4,4,2,2,2,2};

  if (toys.optc) loop_delay = atolx_range(toys.optargs[0], 0, INT_MAX);
  if (toys.optc>1) loop_max = atolx_range(toys.optargs[1], 1, INT_MAX);

  xreadfile("/proc/uptime", toybuf, sizeof(toybuf));
  sscanf(toybuf, "%*s %llu", &units);

  for (loop = 0; !loop_max || loop<loop_max; loop++) {
    unsigned offset = 0, expected = 0;

    if (loop && loop_delay) sleep(loop_delay);

    ptr = oldptr = (void *)toybuf;
    *((loop&1) ? &ptr : &oldptr) += sizeof(struct vmstat_proc);
    get_vmstat_proc((void *)ptr);

    // Print headers
    if (rows>3 && !(loop % (rows-3))) {
      char *header = headers;

      if (!FLAG(n) && isatty(1)) terminal_size(0, &rows);
      else rows = 0;

      printf("procs ------------memory------------ ---swap-- -----io---- --system- ----cpu----\n");
      for (i = 0; i<sizeof(lengths); i++) {
        printf(" %*s"+!i, lengths[i], header);
        header += strlen(header)+1;
      }
      xputc('\n');
    }

    if (loop) units = loop_delay;

    // add up user, sys, idle, and wait time used since last time
    // (Already appended nice to user)
    for (i = total_hz = 0; i<4; i++) total_hz += ptr[i+!!i] - oldptr[i+!!i];

    // Output values in order[]: running, blocked, swaptotal, memfree, buffers,
    // cache, swap_in, swap_out, io_in, io_out, intr, ctxt, user, sys, idle,wait

    for (i=0; i<sizeof(lengths); i++) {
      char order[] = {9, 10, 15, 11, 12, 13, 19, 20, 17, 18, 7, 8, 0, 2, 3, 4};
      unsigned long long out = ptr[order[i]];
      int len;

      // Adjust rate and units
      if (i>5) out -= oldptr[order[i]];
      if (order[i]<7) out = ((out*100) + (total_hz/2)) / total_hz;
      else if (order[i]>16) {
        if (order[i]>18) out *= page_kb;
        out = (out*page_kb+(units-1))/units;
      } else if (order[i]<9) out = (out+(units-1)) / units;

      // If a field was too big to fit in its slot, try to compensate later
      expected += lengths[i] + !!i;
      len = expected - offset - !!i;
      if (len < 0) len = 0;
      offset += printf(" %*llu"+!i, len, out);
    }
    xputc('\n');

    if (!loop_delay) break;
  }
}
