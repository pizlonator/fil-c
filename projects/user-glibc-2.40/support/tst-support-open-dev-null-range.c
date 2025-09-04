/* Tests for support_open_dev_null_range.
   Copyright (C) 2021-2024 Free Software Foundation, Inc.
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

#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <support/check.h>
#include <support/support.h>
#include <support/xunistd.h>
#include <sys/resource.h>
#include <stdlib.h>

#ifndef PATH_MAX
# define PATH_MAX 1024
#endif

#include <stdio.h>

static void
check_path (int fd)
{
  char *proc_fd_path = xasprintf ("/proc/self/fd/%d", fd);
  char file_path[PATH_MAX];
  ssize_t file_path_length
    = readlink (proc_fd_path, file_path, sizeof (file_path));
  if (file_path_length < 0)
    FAIL_EXIT1 ("readlink (%s, %p, %zu)", proc_fd_path, file_path,
		sizeof (file_path));

  free (proc_fd_path);
  file_path[file_path_length] = '\0';
  TEST_COMPARE_STRING (file_path, "/dev/null");
}

static int
number_of_opened_files (void)
{
  DIR *fds = opendir ("/proc/self/fd");
  if (fds == NULL)
    FAIL_EXIT1 ("opendir (\"/proc/self/fd\"): %m");

  int r = 0;
  while (true)
    {
      errno = 0;
      struct dirent64 *e = readdir64 (fds);
      if (e == NULL)
        {
          if (errno != 0)
            FAIL_EXIT1 ("readdir: %m");
          break;
        }

      if (e->d_name[0] == '.')
        continue;

      char *endptr;
      long int fd = strtol (e->d_name, &endptr, 10);
      if (*endptr != '\0' || fd < 0 || fd > INT_MAX)
        FAIL_EXIT1 ("readdir: invalid file descriptor name: /proc/self/fd/%s",
                    e->d_name);

      /* Skip the descriptor which is used to enumerate the
         descriptors.  */
      if (fd == dirfd (fds))
        continue;

      r = r + 1;
    }

  closedir (fds);

  return r;
}

static int
do_test (void)
{
  const int nfds1 = 8;
  int lowfd = support_open_dev_null_range (nfds1, O_RDONLY, 0600);
  for (int i = 0; i < nfds1; i++)
    {
      TEST_VERIFY (fcntl (lowfd + i, F_GETFL) > -1);
      check_path (lowfd + i);
    }

  /* create some gaps.  */
  xclose (lowfd + 1);
  xclose (lowfd + 5);
  xclose (lowfd + 6);

  const int nfds2 = 16;
  int lowfd2 = support_open_dev_null_range (nfds2, O_RDONLY, 0600);
  for (int i = 0; i < nfds2; i++)
    {
      TEST_VERIFY (fcntl (lowfd2 + i, F_GETFL) > -1);
      check_path (lowfd2 + i);
    }

  /* Decrease the maximum number of files.  */
  {
    struct rlimit rl;
    if (getrlimit (RLIMIT_NOFILE, &rl) == -1)
      FAIL_EXIT1 ("getrlimit (RLIMIT_NOFILE): %m");

    rl.rlim_cur = number_of_opened_files ();

    if (setrlimit (RLIMIT_NOFILE, &rl) == 1)
      FAIL_EXIT1 ("setrlimit (RLIMIT_NOFILE): %m");
  }

  const int nfds3 = 16;
  int lowfd3 = support_open_dev_null_range (nfds3, O_RDONLY, 0600);
  for (int i = 0; i < nfds3; i++)
    {
      TEST_VERIFY (fcntl (lowfd3 + i, F_GETFL) > -1);
      check_path (lowfd3 + i);
    }

  /* create a lot of gaps to trigger the range extension.  */
  xclose (lowfd3 + 1);
  xclose (lowfd3 + 3);
  xclose (lowfd3 + 5);
  xclose (lowfd3 + 7);
  xclose (lowfd3 + 9);
  xclose (lowfd3 + 11);
  xclose (lowfd3 + 13);

  const int nfds4 = 16;
  int lowfd4 = support_open_dev_null_range (nfds4, O_RDONLY, 0600);
  for (int i = 0; i < nfds4; i++)
    {
      TEST_VERIFY (fcntl (lowfd4 + i, F_GETFL) > -1);
      check_path (lowfd4 + i);
    }

  return 0;
}

#include <support/test-driver.c>
