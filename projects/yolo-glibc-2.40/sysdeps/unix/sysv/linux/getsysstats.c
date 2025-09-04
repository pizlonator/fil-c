/* Determine various system internal values, Linux version.
   Copyright (C) 1996-2024 Free Software Foundation, Inc.
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

#include <array_length.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <ldsodefs.h>
#include <limits.h>
#include <not-cancel.h>
#include <stdio.h>
#include <stdio_ext.h>
#include <sys/mman.h>
#include <sys/sysinfo.h>
#include <sysdep.h>

static int
__get_nprocs_sched (void)
{
  enum
    {
      max_num_cpus = 32768,
      cpu_bits_size = CPU_ALLOC_SIZE (32768)
    };

  /* This cannot use malloc because it is used on malloc initialization.  */
  __cpu_mask cpu_bits[cpu_bits_size / sizeof (__cpu_mask)];
  int r = INTERNAL_SYSCALL_CALL (sched_getaffinity, 0, cpu_bits_size,
				 cpu_bits);
  if (r > 0)
    return CPU_COUNT_S (r, (cpu_set_t*) cpu_bits);
  else if (r == -EINVAL)
    /* The input buffer is still not enough to store the number of cpus.  This
       is an arbitrary values assuming such systems should be rare and there
       is no offline cpus.  */
    return max_num_cpus;
  /* Some other error.  */
  return 0;
}

static char *
next_line (int fd, char *const buffer, char **cp, char **re,
           char *const buffer_end)
{
  char *res = *cp;
  char *nl = memchr (*cp, '\n', *re - *cp);
  if (nl == NULL)
    {
      if (*cp != buffer)
        {
          if (*re == buffer_end)
            {
              memmove (buffer, *cp, *re - *cp);
              *re = buffer + (*re - *cp);
              *cp = buffer;

              ssize_t n = __read_nocancel (fd, *re, buffer_end - *re);
              if (n < 0)
                return NULL;

              *re += n;

              nl = memchr (*cp, '\n', *re - *cp);
              while (nl == NULL && *re == buffer_end)
                {
                  /* Truncate too long lines.  */
                  *re = buffer + 3 * (buffer_end - buffer) / 4;
                  n = __read_nocancel (fd, *re, buffer_end - *re);
                  if (n < 0)
                    return NULL;

                  nl = memchr (*re, '\n', n);
                  **re = '\n';
                  *re += n;
                }
            }
          else
            nl = memchr (*cp, '\n', *re - *cp);

          res = *cp;
        }

      if (nl == NULL)
        nl = *re - 1;
    }

  *cp = nl + 1;
  assert (*cp <= *re);

  return res == *re ? NULL : res;
}

static int
get_nproc_stat (void)
{
  enum { buffer_size = 1024 };
  char buffer[buffer_size];
  char *buffer_end = buffer + buffer_size;
  char *cp = buffer_end;
  char *re = buffer_end;
  int result = 0;

  const int flags = O_RDONLY | O_CLOEXEC;
  int fd = __open_nocancel ("/proc/stat", flags);
  if (fd != -1)
    {
      char *l;
      while ((l = next_line (fd, buffer, &cp, &re, buffer_end)) != NULL)
	/* The current format of /proc/stat has all the cpu* entries
	   at the front.  We assume here that stays this way.  */
	if (strncmp (l, "cpu", 3) != 0)
	  break;
	else if (isdigit (l[3]))
	  ++result;

      __close_nocancel_nostatus (fd);
    }

  return result;
}

static int
read_sysfs_file (const char *fname)
{
  enum { buffer_size = 1024 };
  char buffer[buffer_size];
  char *buffer_end = buffer + buffer_size;
  char *cp = buffer_end;
  char *re = buffer_end;

  const int flags = O_RDONLY | O_CLOEXEC;
  /* This file contains comma-separated ranges.  */
  int fd = __open_nocancel (fname, flags);
  char *l;
  int result = 0;
  if (fd != -1)
    {
      l = next_line (fd, buffer, &cp, &re, buffer_end);
      if (l != NULL)
	do
	  {
	    char *endp;
	    unsigned long int n = strtoul (l, &endp, 10);
	    if (l == endp)
	      {
		result = 0;
		break;
	      }

	    unsigned long int m = n;
	    if (*endp == '-')
	      {
		l = endp + 1;
		m = strtoul (l, &endp, 10);
		if (l == endp)
		  {
		    result = 0;
		    break;
		  }
	      }

	    if (m >= n)
	      result += m - n + 1;

	    l = endp;
	    if (l < re && *l == ',')
	      ++l;
	  }
	while (l < re && *l != '\n');

      __close_nocancel_nostatus (fd);
    }

  return result;
}

static int
get_nprocs_fallback (void)
{
  int result;

  /* Try /proc/stat first.  */
  result = get_nproc_stat ();
  if (result != 0)
    return result;

  /* Try sched_getaffinity.  */
  result = __get_nprocs_sched ();
  if (result != 0)
    return result;

  /* We failed to obtain an accurate number.  Be conservative: return
     the smallest number meaning that this is not a uniprocessor system,
     so atomics are needed.  */
  return 2;
}

int
__get_nprocs (void)
{
  int result = read_sysfs_file ("/sys/devices/system/cpu/online");
  if (result != 0)
    return result;

  /* Fall back to /proc/stat and sched_getaffinity.  */
  return get_nprocs_fallback ();
}
libc_hidden_def (__get_nprocs)
weak_alias (__get_nprocs, get_nprocs)

/* On some architectures it is possible to distinguish between configured
   and active cpus.  */
int
__get_nprocs_conf (void)
{
  int result = read_sysfs_file ("/sys/devices/system/cpu/possible");
  if (result != 0)
    return result;

  /* Fall back to /proc/stat and sched_getaffinity.  */
  return get_nprocs_fallback ();
}
libc_hidden_def (__get_nprocs_conf)
weak_alias (__get_nprocs_conf, get_nprocs_conf)


/* Compute (num*mem_unit)/pagesize, but avoid overflowing long int.
   In practice, mem_unit is never bigger than the page size, so after
   the first loop it is 1.  [In the kernel, it is initialized to
   PAGE_SIZE in mm/page_alloc.c:si_meminfo(), and then in
   kernel.sys.c:do_sysinfo() it is set to 1 if unsigned long can
   represent all the sizes measured in bytes].  */
static long int
sysinfo_mempages (unsigned long int num, unsigned int mem_unit)
{
  unsigned long int ps = __getpagesize ();

  while (mem_unit > 1 && ps > 1)
    {
      mem_unit >>= 1;
      ps >>= 1;
    }
  num *= mem_unit;
  while (ps > 1)
    {
      ps >>= 1;
      num >>= 1;
    }
  return num;
}

/* Return the number of pages of total/available physical memory in
   the system.  This used to be done by parsing /proc/meminfo, but
   that's unnecessarily expensive (and /proc is not always available).
   The sysinfo syscall provides the same information, and has been
   available at least since kernel 2.3.48.  */
long int
__get_phys_pages (void)
{
  struct sysinfo info;

  __sysinfo (&info);
  return sysinfo_mempages (info.totalram, info.mem_unit);
}
libc_hidden_def (__get_phys_pages)
weak_alias (__get_phys_pages, get_phys_pages)

long int
__get_avphys_pages (void)
{
  struct sysinfo info;

  __sysinfo (&info);
  return sysinfo_mempages (info.freeram, info.mem_unit);
}
libc_hidden_def (__get_avphys_pages)
weak_alias (__get_avphys_pages, get_avphys_pages)
