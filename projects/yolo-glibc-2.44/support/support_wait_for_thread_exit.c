/* Wait until all threads except the current thread has exited.
   Copyright (C) 2021-2026 Free Software Foundation, Inc.
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

#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <support/check.h>
#include <support/support.h>
#include <unistd.h>

void
support_wait_for_thread_exit (void)
{
#ifdef __linux__
  DIR *proc_self_task = opendir ("/proc/self/task");
  TEST_VERIFY_EXIT (proc_self_task != NULL);

  while (true)
    {
      errno = 0;
      struct dirent *e = readdir (proc_self_task);
      if (e == NULL && errno != 0)
        FAIL_EXIT1 ("readdir: %m");
      if (e == NULL)
        {
          /* Only the main thread remains.  Testing may continue.  */
          closedir (proc_self_task);
          return;
        }

      /* In some kernels, "0" entries denote a thread that has just
         exited.  */
      if (strcmp (e->d_name, ".") == 0 || strcmp (e->d_name, "..") == 0
          || strcmp (e->d_name, "0") == 0)
        continue;

      int task_tid = atoi (e->d_name);
      if (task_tid <= 0)
        FAIL_EXIT1 ("Invalid /proc/self/task entry: %s", e->d_name);

      if (task_tid == gettid ())
        /* The current thread.  Keep scanning for other
           threads.  */
        continue;

      /* task_tid does not refer to this thread here, i.e., there is
         another running thread.  */

      /* Small timeout to give the thread a chance to exit.  */
      usleep (50 * 1000);

      /* Start scanning the directory from the start.  */
      rewinddir (proc_self_task);
    }
#else
  /* Use a large timeout because we cannot verify that the thread has
     exited.  */
  usleep (5 * 1000 * 1000);
#endif
}
