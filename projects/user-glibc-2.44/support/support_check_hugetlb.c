/* Runtime detection of huge-page support for malloc tests.
   Copyright (C) 2026 Free Software Foundation, Inc.
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

#include <fcntl.h>
#include <intprops.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <elf/dl-tunables.h>
#include <support/support_check_hugetlb.h>
#include <support/check.h>

enum thp_mode_t
support_get_thp_mode (void)
{
  int fd = open ("/sys/kernel/mm/transparent_hugepage/enabled",
		 O_RDONLY, 0);
  if (fd == -1)
    return thp_mode_not_supported;

  static const char mode_always[]  = "[always] madvise never\n";
  static const char mode_madvise[] = "always [madvise] never\n";
  static const char mode_never[]   = "always madvise [never]\n";

  char str[sizeof(mode_always)];
  ssize_t s = read (fd, str, sizeof (str));
  close (fd);
  if (s < 0 || s >= sizeof str)
    return thp_mode_not_supported;
  str[s] = '\0';

  if (s == sizeof (mode_always) - 1)
    {
      if (strcmp (str, mode_always) == 0)
	return thp_mode_always;
      else if (strcmp (str, mode_madvise) == 0)
	return thp_mode_madvise;
      else if (strcmp (str, mode_never) == 0)
	return thp_mode_never;
    }
  return thp_mode_not_supported;
}

unsigned long int
support_get_thp_size (void)
{
  int fd = open ("/sys/kernel/mm/transparent_hugepage/hpage_pmd_size",
                 O_RDONLY, 0);
  if (fd == -1)
    return 0;

  char str[INT_BUFSIZE_BOUND (unsigned long int)];
  ssize_t s = read (fd, str, sizeof (str));
  close (fd);
  if (s < 0)
    return 0;

  unsigned long int r = 0;
  for (ssize_t i = 0; i < s; i++)
    {
      if (str[i] == '\n')
    break;
      r *= 10;
      r += str[i] - '0';
    }
  return r;
}

bool
support_thp_work_madvise (void)
{
  enum thp_mode_t mode = support_get_thp_mode ();
  return mode == thp_mode_always || mode == thp_mode_madvise;
}

bool
support_hugepages_reserved (void)
{
  int fd = open ("/proc/sys/vm/nr_hugepages", O_RDONLY);
  if (fd == -1)
    return false;

  char str[INT_BUFSIZE_BOUND(unsigned long int)];
  ssize_t s = read (fd, str, sizeof (str));
  close (fd);
  if (s < 0 || s >= sizeof str)
    return false;
  str[s] = '\0';
  unsigned long int n = 0;
  return sscanf (str, "%lu", &n) == 1 && n > 0;
}

void
support_check_malloc_hugetlb (void)
{
  if (!TUNABLE_IS_INITIALIZED (glibc, malloc, hugetlb))
    return;

  size_t hugetlb = TUNABLE_GET_FULL (glibc, malloc, hugetlb, size_t, NULL);
  if (hugetlb == 1 && !support_thp_work_madvise ())
    FAIL_UNSUPPORTED ("glibc.malloc.hugetlb=1 requires"
                      " /sys/kernel/mm/transparent_hugepage/enabled"
                      " either always or madvise");
  if (hugetlb == 2 && !support_hugepages_reserved ())
    FAIL_UNSUPPORTED ("glibc.malloc.hugetlb=2 requires"
                      " /proc/sys/vm/nr_hugepages > 0");
}
