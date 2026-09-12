/* FUSE-based test for mkstemp.
   Copyright (C) 2024-2026 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

#include <stdlib.h>

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <support/check.h>
#include <support/fuse.h>
#include <support/support.h>
#include <support/xunistd.h>

/* Set to true in do_test to cause the first FUSE_CREATE attempt to fail.  */
static _Atomic bool simulate_creat_race;

/* Basic tests with eventually successful creation.  */
static void
fuse_thread_basic (struct support_fuse *f, void *closure)
{
  char *previous_name = NULL;
  int state = 0;
  struct fuse_in_header *inh;
  while ((inh = support_fuse_next (f)) != NULL)
    {
      if (support_fuse_handle_mountpoint (f)
          || (inh->nodeid == 1 && support_fuse_handle_directory (f)))
        continue;

      switch (inh->opcode)
        {
        case FUSE_LOOKUP:
          /* File does not exist initially.  */
          TEST_COMPARE (inh->nodeid, 1);
          if (simulate_creat_race)
            {
              if (state < 3)
                ++state;
              else
                FAIL ("invalid state: %d", state);
            }
          else
            {
              TEST_COMPARE (state, 0);
              state = 3;
            }
          support_fuse_reply_error (f, ENOENT);
          break;
        case FUSE_CREATE:
          {
            TEST_COMPARE (inh->nodeid, 1);
            char *name;
            struct fuse_create_in *p
              = support_fuse_cast_name (CREATE, inh, &name);
            /* Name follows after struct fuse_create_in.  */
            TEST_COMPARE (p->flags & O_ACCMODE, O_RDWR);
            TEST_VERIFY (p->flags & O_EXCL);
            TEST_VERIFY (p->flags & O_CREAT);
            TEST_COMPARE (p->mode & 07777, 0600);
            TEST_VERIFY (S_ISREG (p->mode));
            TEST_COMPARE_BLOB (name, 3, "new", 3);

            if (state != 3 && simulate_creat_race)
              {
                ++state;
                support_fuse_reply_error (f, EEXIST);
              }
            else
              {
                if (previous_name != NULL)
                  /* This test has a very small probability of failure
                     due to a harmless collision (one in 62**6 tests).  */
                  TEST_VERIFY (strcmp (name, previous_name) != 0);
                TEST_COMPARE (state, 3);
                ++state;
                struct fuse_entry_out *entry;
                struct fuse_open_out *open;
                support_fuse_prepare_create (f, 2, &entry, &open);
                entry->attr.mode = S_IFREG | 0600;
                support_fuse_reply_prepared (f);
              }
            free (previous_name);
            previous_name = xstrdup (name);
          }
          break;
        case FUSE_FLUSH:
        case FUSE_RELEASE:
          TEST_COMPARE (state, 4);
          TEST_COMPARE (inh->nodeid, 2);
          support_fuse_reply_empty (f);
          break;
        default:
          support_fuse_reply_error (f, EIO);
        }
    }
  free (previous_name);
}

/* Reply that all files exist.  */
static void
fuse_thread_eexist (struct support_fuse *f, void *closure)
{
  uint64_t counter = 0;
  struct fuse_in_header *inh;
  while ((inh = support_fuse_next (f)) != NULL)
    {
      if (support_fuse_handle_mountpoint (f)
          || (inh->nodeid == 1 && support_fuse_handle_directory (f)))
        continue;

      switch (inh->opcode)
        {
        case FUSE_LOOKUP:
          ++counter;
          TEST_COMPARE (inh->nodeid, 1);
          char *name = support_fuse_cast (LOOKUP, inh);
          TEST_COMPARE_BLOB (name, 3, "new", 3);
          TEST_COMPARE (strlen (name), 9);
          for (int i = 3; i <= 8; ++i)
            {
              /* The glibc implementation uses letters and digits only.  */
              char ch = name[i];
              TEST_VERIFY (('0' <= ch && ch <= '9')
                           || ('a' <= ch && ch <= 'z')
                           || ('A' <= ch && ch <= 'Z'));
            }
          struct fuse_entry_out out =
            {
              .nodeid = 2,
              .attr = {
                .mode = S_IFREG | 0600,
                .ino = 2,
              },
            };
          support_fuse_reply (f, &out, sizeof (out));
          break;
        default:
          support_fuse_reply_error (f, EIO);
        }
    }
  /* Verify that mkstemp has retried a lot.  The current
     implementation tries 62 * 62 * 62 times until it goves up.  */
  TEST_VERIFY (counter >= 200000);
}

static int
do_test (void)
{
  support_fuse_init ();

  for (int do_simulate_creat_race = 0; do_simulate_creat_race < 2;
       ++do_simulate_creat_race)
    {
      simulate_creat_race = do_simulate_creat_race;
      printf ("info: testing with simulate_creat_race == %d\n",
              (int) simulate_creat_race);
      struct support_fuse *f = support_fuse_mount (fuse_thread_basic, NULL);
      char *path = xasprintf ("%s/newXXXXXX", support_fuse_mountpoint (f));
      int fd = mkstemp (path);
      TEST_VERIFY (fd > 2);
      xclose (fd);
      free (path);
      support_fuse_unmount (f);
    }

  puts ("info: testing EEXIST failure case for mkstemp");
  {
    struct support_fuse *f = support_fuse_mount (fuse_thread_eexist, NULL);
    char *path = xasprintf ("%s/newXXXXXX", support_fuse_mountpoint (f));
    errno = 0;
    TEST_COMPARE (mkstemp (path), -1);
    TEST_COMPARE (errno, EEXIST);
    free (path);
    support_fuse_unmount (f);
  }

  return 0;
}

#include <support/test-driver.c>
