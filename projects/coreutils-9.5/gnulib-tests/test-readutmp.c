/* Test of readutmp module.
   Copyright (C) 2023-2024 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* Written by Bruno Haible <bruno@clisp.org>, 2023.  */

#include <config.h>

#include "readutmp.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "idx.h"
#include "xalloc.h"

#define ELEMENT STRUCT_UTMP
#define COMPARE(entry1, entry2) \
  _GL_CMP (UT_TIME_MEMBER (entry1), UT_TIME_MEMBER (entry2))
#define STATIC static
#include "array-mergesort.h"

#include "macros.h"

int
main (int argc, char *argv[])
{
  STRUCT_UTMP *entries;
  idx_t num_entries;

  if (read_utmp (UTMP_FILE, &num_entries, &entries, 0) < 0)
    {
      #if READ_UTMP_SUPPORTED
      fprintf (stderr, "Skipping test: cannot open %s\n", UTMP_FILE);
      #else
      fprintf (stderr, "Skipping test: neither <utmpx.h> nor <utmp.h> is available\n");
      #endif
      return 77;
    }

  printf ("Here are the read_utmp results.\n");
  printf ("Flags: B = Boot, U = User Process\n");
  printf ("\n");
  printf ("                                                              Termi‐      Flags\n");
  printf ("    Time (GMT)             User          Device        PID    nation Exit  B U  Host\n");
  printf ("------------------- ------------------ ----------- ---------- ------ ----  - -  ----\n");

  /* What do the results look like?
     * On Alpine Linux, Cygwin, Android, the output is empty.
     * The entries are usually not sorted according to the Time column, so
       we do it here.
     * In the User column, special values exist:
       - The empty string denotes a system event.
         Seen on glibc, macOS, FreeBSD, NetBSD, OpenBSD, AIX
       - The value "reboot" denotes a reboot.
         Seen on glibc
       - The value "runlevel" denotes a runlevel change.
         Seen on glibc
       - The value "LOGIN" denotes the start of a virtual console.
         Seen on glibc, Solaris
       - The value "/usr/libexec/getty" denotes the start of a virtual console.
         Seen on NetBSD
     * In the Device column:
       - The empty string denotes a system event.
         Seen on macOS, FreeBSD, AIX
       - The value "~" denotes an event with no associated device.
         Seen on glibc
       - The values "system boot", "system down", "run-level N" are
         seen on NetBSD, AIX, Solaris.
       - The values "old time", "new time" are
         seen on Solaris.
       - Common devices are:
         - On glibc: "ttyN" (console) and "pts/N" (pseudo-terminals).
         - On macOS: "ttysNNN" (pseudo-terminals).
         - On FreeBSD: "ttyvN" (console).
         - On NetBSD: "ttyEN", "constty" (console).
         - On OpenBSD: "ttyCN", "console" (console) and "ttypN" (pseudo-terminals).
         - on AIX: "vtyN" (console) and "pts/N" (pseudo-terminals).
         - On Solaris: "vt/N", "console" (console) and "pts/N" (pseudo-terminals).
     * The PID column is zero on platforms without a 'ut_pid' field: OpenBSD.
   */
  if (num_entries > 0)
    {
      /* Sort the entries according to increasing UT_TIME_MEMBER (entry).
         Use a stable sort algorithm.  */
      merge_sort_inplace (entries, num_entries,
                          XNMALLOC (num_entries, STRUCT_UTMP));

      idx_t boot_time_count = 0;
      idx_t i;
      for (i = 0; i < num_entries; i++)
        {
          const STRUCT_UTMP *entry = &entries[i];

          char *user = extract_trimmed_name (entry);
          const char *device = entry->ut_line;
          long pid = UT_PID (entry);
          int termination = UT_EXIT_E_TERMINATION (entry);
          int exit = UT_EXIT_E_EXIT (entry);
          const char *host = entry->ut_host;

          time_t tim = UT_TIME_MEMBER (entry);
          struct tm *gmt = gmtime (&tim);
          char timbuf[100];
          if (gmt == NULL
              || strftime (timbuf, sizeof (timbuf), "%Y-%m-%d %H:%M:%S", gmt)
                 == 0)
            strcpy (timbuf, "---");

          printf ("%-19s %-18s %-11s %10ld %4d   %3d   %c %c  %s\n",
                  timbuf,
                  user,
                  device,
                  pid,
                  termination,
                  exit,
                  UT_TYPE_BOOT_TIME (entry) ? 'X' : ' ',
                  UT_TYPE_USER_PROCESS (entry) ? 'X' : ' ',
                  host);

          if (UT_TYPE_BOOT_TIME (entry))
            boot_time_count++;
        }
      fflush (stdout);

      /* If the first time is more than 5 years in the past or the last time
         is more than a week in the future, the time_t members are wrong.  */
      time_t first = UT_TIME_MEMBER (&entries[0]);
      time_t last = UT_TIME_MEMBER (&entries[num_entries - 1]);
      time_t now = time (NULL);
      ASSERT (first >= now - 157680000);
      ASSERT (last <= now + 604800);

      /* read_utmp should not produce multiple BOOT_TIME entries.  */
      ASSERT (boot_time_count <= 1);

      /* read_utmp should fake a BOOT_TIME entry if needed.
         Platform specific hacks go into lib/boot-time-aux.h.  */
      ASSERT (boot_time_count >= 1);
    }

  free (entries);

  return 0;
}
