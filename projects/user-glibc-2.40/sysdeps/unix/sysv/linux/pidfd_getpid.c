/* pidfd_getpid - Get the associated pid from the pid file descriptor.
   Copyright (C) 2023-2024 Free Software Foundation, Inc.
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

#include <_itoa.h>
#include <errno.h>
#include <intprops.h>
#include <procutils.h>
#include <stdlib.h>
#include <string.h>
#include <sysdep.h>
#include <unistd.h>

#define FDINFO_TO_FILENAME_PREFIX "/proc/self/fdinfo/"

#define FDINFO_FILENAME_LEN \
  (sizeof (FDINFO_TO_FILENAME_PREFIX) + INT_STRLEN_BOUND (int))

struct parse_fdinfo_t
{
  bool found;
  pid_t pid;
};

/* Parse the PID field in the fdinfo entry, if existent.  Avoid strtol or
   similar to not be locale dependent.  */
static int
parse_fdinfo (const char *l, void *arg)
{
  enum { fieldlen = sizeof ("Pid:") - 1 };
  if (strncmp (l, "Pid:", fieldlen) != 0)
    return 0;

  l += fieldlen;

  /* Skip leading spaces.  */
  while (*l == ' ' || (unsigned int) (*l) -'\t' < 5)
    l++;

  bool neg = false;
  switch (*l)
    {
    case '-':
      neg = true;
      l++;
      break;
    case '+':
      return -1;
    }

  if (*l == '\0')
    return 0;

  int n = 0;
  while (*l != '\0')
    {
      /* Check if '*l' is a digit.  */
      if ('0' > *l || *l > '9')
        return -1;

      /* Ignore invalid large values.  */
      if (INT_MULTIPLY_WRAPV (10, n, &n)
          || INT_ADD_WRAPV (n, *l - '0', &n))
        return -1;

      l++;
    }

  /* -1 indicates that the process is terminated.  */
  if (neg && n != 1)
    return -1;

  struct parse_fdinfo_t *fdinfo = arg;
  fdinfo->pid = neg ? -n : n;
  fdinfo->found = true;

  return 1;
}

pid_t
pidfd_getpid (int fd)
{
  if (__glibc_unlikely (fd < 0))
    {
      __set_errno (EBADF);
      return -1;
    }

  char fdinfoname[FDINFO_FILENAME_LEN];

  char *p = mempcpy (fdinfoname, FDINFO_TO_FILENAME_PREFIX,
		     strlen (FDINFO_TO_FILENAME_PREFIX));
  *_fitoa_word (fd, p, 10, 0) = '\0';

  struct parse_fdinfo_t fdinfo = { .found = false, .pid = -1 };
  if (!__libc_procutils_read_file (fdinfoname, parse_fdinfo, &fdinfo))
    /* The fdinfo contains an invalid 'Pid:' value.  */
    return INLINE_SYSCALL_ERROR_RETURN_VALUE (EBADF);

  /* The FD does not have a 'Pid:' entry associated.  */
  if (!fdinfo.found)
    return INLINE_SYSCALL_ERROR_RETURN_VALUE (EBADF);

  /* The pidfd cannot be resolved because it is in a separate pid
     namespace.  */
  if (fdinfo.pid == 0)
    return INLINE_SYSCALL_ERROR_RETURN_VALUE (EREMOTE);

  /* A negative value means the process is terminated.  */
  if (fdinfo.pid < 0)
    return INLINE_SYSCALL_ERROR_RETURN_VALUE (ESRCH);

  return fdinfo.pid;
}
