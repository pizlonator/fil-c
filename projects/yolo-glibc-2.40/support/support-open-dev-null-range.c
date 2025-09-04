/* Return a range of open file descriptors.
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
#include <fcntl.h>
#include <support/support.h>
#include <support/check.h>
#include <support/xunistd.h>
#include <stdlib.h>
#include <sys/resource.h>

static void
increase_nofile (void)
{
  struct rlimit rl;
  if (getrlimit (RLIMIT_NOFILE, &rl) == -1)
    FAIL_EXIT1 ("getrlimit (RLIMIT_NOFILE): %m");

  rl.rlim_cur += 128;

  if (setrlimit (RLIMIT_NOFILE, &rl) == 1)
    FAIL_EXIT1 ("setrlimit (RLIMIT_NOFILE): %m");
}

static int
open_dev_null (int flags, mode_t mode)
{
  int fd = open64 ("/dev/null", flags, mode);
  if (fd >= 0)
    return fd;

  if (fd < 0 && errno != EMFILE)
    FAIL_EXIT1 ("open64 (\"/dev/null\", 0x%x, 0%o): %m", flags, mode);

  increase_nofile ();

  return xopen ("/dev/null", flags, mode);
}

struct range
{
  int lowfd;
  size_t len;
};

struct range_list
{
  size_t total;
  size_t used;
  struct range *ranges;
};

static void
range_init (struct range_list *r)
{
  r->total = 8;
  r->used = 0;
  r->ranges = xmalloc (r->total * sizeof (struct range));
}

static void
range_add (struct range_list *r, int lowfd, size_t len)
{
  if (r->used == r->total)
    {
      r->total *= 2;
      r->ranges = xrealloc (r->ranges, r->total * sizeof (struct range));
    }
  r->ranges[r->used].lowfd = lowfd;
  r->ranges[r->used].len = len;
  r->used++;
}

static void
range_close (struct range_list *r)
{
  for (size_t i = 0; i < r->used; i++)
    {
      int minfd = r->ranges[i].lowfd;
      int maxfd = r->ranges[i].lowfd + r->ranges[i].len;
      for (int fd = minfd; fd < maxfd; fd++)
	xclose (fd);
    }
  free (r->ranges);
}

int
support_open_dev_null_range (int num, int flags, mode_t mode)
{
  /* We keep track of the ranges that hit an already opened descriptor, so
     we close them after we get a working range.  */
  struct range_list rl;
  range_init (&rl);

  int lowfd = open_dev_null (flags, mode);
  int prevfd = lowfd;
  while (true)
    {
      int i = 1;
      for (; i < num; i++)
	{
	  int fd = open_dev_null (flags, mode);
	  if (fd != lowfd + i)
	    {
	      range_add (&rl, lowfd, prevfd - lowfd + 1);

	      prevfd = lowfd = fd;
	      break;
	    }
	  prevfd = fd;
	}
      if (i == num)
	break;
    }

  range_close (&rl);

  return lowfd;
}
