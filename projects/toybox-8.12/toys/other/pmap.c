/* pmap.c - Reports the memory map of a process or processes.
 *
 * Copyright 2013 Ranjan Kumar <ranjankumar.bth@gmail.com>
 * Copyright 2013 Kyungwan Han <asura321@gmail.com>
 *
 * No Standard.
 *
 * TODO: two passes so we can auto-size the columns?

USE_PMAP(NEWTOY(pmap, "<1pqx", TOYFLAG_USR|TOYFLAG_BIN))

config PMAP
  bool "pmap"
  default y
  help
    usage: pmap [-pqx] PID...

    Report the memory map of a process or processes.

    -p	Show full paths
    -q	Do not show header or footer
    -x	Show the extended format
*/

#define FOR_pmap
#include "toys.h"

void pmap_main(void)
{
  char **optargs, *line = 0;
  size_t len = 0;

  for (optargs = toys.optargs; *optargs; optargs++) {
    long long start, end, pss, tpss=0, dirty, tdirty=0, swap, tswap=0, total=0;
    char *name = 0, *k = "K"+FLAG(x), mode[5];
    pid_t pid = atolx(*optargs);
    int extras = 0, off, count;
    FILE *fp;

    sprintf(toybuf, "/proc/%u/cmdline", pid);
    if (!(name = readfile(toybuf, 0, 0))) {
      error_msg("no %s", toybuf);
      continue;
    }
    xprintf("%d: %s\n", pid, name);
    free(name);

    // Only bother scanning the more verbose smaps file in -x mode.
    sprintf(toybuf, "/proc/%u/%smaps", pid, "s"+!FLAG(x));
    if (!(fp = fopen(toybuf, "r"))) {
      error_msg("no %s", toybuf);
      continue;
    }

    if (FLAG(x) && !FLAG(q))
      xprintf("Address%*cKbytes     PSS   Dirty    Swap Mode  Mapping\n",
          (int)(sizeof(long)*2)-5, ' ');

    while (getline(&line, &len, fp) > 0) {
      count = sscanf(line, "%llx-%llx %4s %*s %*s %*s %n", &start, &end, mode,
          &off);
      if (count == 3) {
        name = line[off] ? line+off : "  [anon]\n";
        if (mode[3] == 'p') mode[3] = '-';
        total += end = (end-start)/1024;
        printf("%0*llx % *lld%s ", (int)(2*sizeof(long)), start, 6+FLAG(x),
            end, k);
        if (FLAG(x)) {
          strcpy(toybuf, name);
          name = toybuf;
          continue;
        }
      } else {
        if (sscanf(line, "Pss: %lld", &pss) ||
            sscanf(line, "Private_Dirty: %lld", &dirty) ||
            sscanf(line, "Swap: %lld", &swap)) extras++;
        if (extras==3) {
          printf("% 7lld %7lld %7lld ", pss, dirty, swap);
          tpss += pss;
          tdirty += dirty;
          tswap += swap;
          extras = 0;
        } else continue;
      }

      xprintf("%s- %s%s", mode, *name == '[' ? "  " : "",
              FLAG(p) ? name : basename(name));
    }

    if (!FLAG(q)) {
      if (FLAG(x)) {
        xprintf("----------------  ------  ------  ------  ------\n" +
            ((sizeof(long)==4)?8:0));
      }
      printf("total% *lld%s", 2*(int)(sizeof(long)+1)+FLAG(x), total, k);
      if (FLAG(x)) printf("% 8lld% 8lld% 8lld", tpss, tdirty, tswap);
      xputc('\n');
    }

    fclose(fp);
  }
  free(line);
}
