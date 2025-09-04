/* Test of getting user name.
   Copyright (C) 2010-2024 Free Software Foundation, Inc.

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

/* Written by Bruno Haible and Paul Eggert.  */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#if !(defined _WIN32 && !defined __CYGWIN__)
# include <pwd.h>
#endif

#include <sys/stat.h>
#include <sys/types.h>

#include "macros.h"

static void
test_getlogin_result (const char *buf, int err)
{
  if (err != 0)
    {
      if (err == ENOENT)
        {
          /* This can happen on GNU/Linux.  */
          fprintf (stderr, "Skipping test: no entry in utmp file.\n");
          exit (77);
        }

      ASSERT (err == ENOTTY
              || err == EINVAL /* seen on Linux/SPARC */
              || err == ENXIO
             );

#if defined __linux__
      /* On Linux, it is possible to set up a chroot environment in such a way
         that stdin is connected to a tty and nevertheless /proc/self/loginuid
         contains the value (uid_t)(-1).  In this situation, getlogin() and
         getlogin_r() fail; this is expected.  */
      bool loginuid_undefined = false;
      /* Does the special file /proc/self/loginuid contain the value
         (uid_t)(-1)?  */
      FILE *fp = fopen ("/proc/self/loginuid", "r");
      if (fp != NULL)
        {
          char fread_buf[21];
          size_t n = fread (fread_buf, 1, sizeof fread_buf, fp);
          if (n > 0 && n < sizeof fread_buf)
            {
              fread_buf[n] = '\0';
              errno = 0;
              char *endptr;
              unsigned long value = strtoul (fread_buf, &endptr, 10);
              if (*endptr == '\0' && errno == 0)
                loginuid_undefined = ((uid_t) value == (uid_t)(-1));
            }
          fclose (fp);
        }
      if (loginuid_undefined)
        {
          fprintf (stderr, "Skipping test: loginuid is undefined.\n");
          exit (77);
        }
#endif

      /* It fails when stdin is not connected to a tty.  */
#if !defined __hpux /* On HP-UX 11.11 it fails anyway.  */
      ASSERT (! isatty (0));
#endif
      fprintf (stderr, "Skipping test: stdin is not a tty.\n");
      exit (77);
    }

  /* Compare against the value from the environment.  */
#if !(defined _WIN32 && !defined __CYGWIN__)
  /* Unix platform */
  {
    struct stat stat_buf;
    struct passwd *pwd;

    if (!isatty (STDIN_FILENO))
      {
        /* We get here, for example, when running under 'nohup' or as part of a
           non-interactive ssh command.  */
        fprintf (stderr, "Skipping test: stdin is not a tty.\n");
        exit (77);
      }

    ASSERT (fstat (STDIN_FILENO, &stat_buf) == 0);

    pwd = getpwnam (buf);
    if (! pwd)
      {
        fprintf (stderr, "Skipping test: %s: no such user\n", buf);
        exit (77);
      }

    ASSERT (pwd->pw_uid == stat_buf.st_uid);
  }
#endif
#if defined _WIN32 && !defined __CYGWIN__
  /* Native Windows platform.
     Note: This test would fail on Cygwin in an ssh session, because sshd
     sets USERNAME=SYSTEM.  */
  {
    const char *name = getenv ("USERNAME");
    if (name != NULL && name[0] != '\0')
      ASSERT (strcmp (buf, name) == 0);
  }
#endif
}
