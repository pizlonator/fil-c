/* Miscellaneous support functions for dynamic linker
   Copyright (C) 1997-2024 Free Software Foundation, Inc.
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
#include <fcntl.h>
#include <ldsodefs.h>
#include <link.h>
#include <not-cancel.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

/* Read the whole contents of FILE into new mmap'd space with given
   protections.  *SIZEP gets the size of the file.  On error MAP_FAILED
   is returned.  */

void *
_dl_sysdep_read_whole_file (const char *file, size_t *sizep, int prot)
{
  void *result = MAP_FAILED;
  struct __stat64_t64 st;
  int fd = __open64_nocancel (file, O_RDONLY | O_CLOEXEC);
  if (fd >= 0)
    {
      if (__fstat64_time64 (fd, &st) >= 0)
	{
	  *sizep = st.st_size;

	  /* No need to map the file if it is empty.  */
	  if (*sizep != 0)
	    /* Map a copy of the file contents.  */
	    result = __mmap (NULL, *sizep, prot,
#ifdef MAP_COPY
			     MAP_COPY
#else
			     MAP_PRIVATE
#endif
#ifdef MAP_FILE
			     | MAP_FILE
#endif
			     , fd, 0);
	}
      __close_nocancel (fd);
    }
  return result;
}

/* Test whether given NAME matches any of the names of the given object.  */
int
_dl_name_match_p (const char *name, const struct link_map *map)
{
  if (strcmp (name, map->l_name) == 0)
    return 1;

  struct libname_list *runp = map->l_libname;

  while (runp != NULL)
    if (strcmp (name, runp->name) == 0)
      return 1;
    else
      /* Synchronize with the release MO store in add_name_to_object.
	 See CONCURRENCY NOTES in add_name_to_object in dl-load.c.  */
      runp = atomic_load_acquire (&runp->next);

  return 0;
}

unsigned long int
_dl_higher_prime_number (unsigned long int n)
{
  /* These are primes that are near, but slightly smaller than, a
     power of two.  */
  static const uint32_t primes[] = {
    UINT32_C (7),
    UINT32_C (13),
    UINT32_C (31),
    UINT32_C (61),
    UINT32_C (127),
    UINT32_C (251),
    UINT32_C (509),
    UINT32_C (1021),
    UINT32_C (2039),
    UINT32_C (4093),
    UINT32_C (8191),
    UINT32_C (16381),
    UINT32_C (32749),
    UINT32_C (65521),
    UINT32_C (131071),
    UINT32_C (262139),
    UINT32_C (524287),
    UINT32_C (1048573),
    UINT32_C (2097143),
    UINT32_C (4194301),
    UINT32_C (8388593),
    UINT32_C (16777213),
    UINT32_C (33554393),
    UINT32_C (67108859),
    UINT32_C (134217689),
    UINT32_C (268435399),
    UINT32_C (536870909),
    UINT32_C (1073741789),
    UINT32_C (2147483647),
				       /* 4294967291L */
    UINT32_C (2147483647) + UINT32_C (2147483644)
  };

  const uint32_t *low = &primes[0];
  const uint32_t *high = &primes[sizeof (primes) / sizeof (primes[0])];

  while (low != high)
    {
      const uint32_t *mid = low + (high - low) / 2;
      if (n > *mid)
       low = mid + 1;
      else
       high = mid;
    }

#if 0
  /* If we've run out of primes, abort.  */
  if (n > *low)
    {
      fprintf (stderr, "Cannot find prime bigger than %lu\n", n);
      abort ();
    }
#endif

  return *low;
}

/* A stripped down strtoul-like implementation for very early use.  It
   does not set errno if the result is outside bounds because it may get
   called before errno may have been set up.  */

uint64_t
_dl_strtoul (const char *nptr, char **endptr)
{
  uint64_t result = 0;
  bool positive = true;
  unsigned max_digit;

  while (*nptr == ' ' || *nptr == '\t')
    ++nptr;

  if (*nptr == '-')
    {
      positive = false;
      ++nptr;
    }
  else if (*nptr == '+')
    ++nptr;

  if (*nptr < '0' || *nptr > '9')
    {
      if (endptr != NULL)
	*endptr = (char *) nptr;
      return 0UL;
    }

  uint64_t cutoff = (UINT64_MAX * 2UL + 1UL) / 10;
  uint64_t cutlim = (UINT64_MAX * 2UL + 1UL) % 10;

  int base = 10;
  max_digit = 9;
  if (*nptr == '0')
    {
      if (nptr[1] == 'x' || nptr[1] == 'X')
	{
	  base = 16;
	  nptr += 2;
	  cutoff = (UINT64_MAX * 2UL + 1UL) / 16;
	  cutlim = (UINT64_MAX * 2UL + 1UL) % 16;
	}
      else
	{
	  base = 8;
	  max_digit = 7;
	  cutoff = (UINT64_MAX * 2UL + 1UL) / 8;
	  cutlim = (UINT64_MAX * 2UL + 1UL) % 8;
	}
    }

  while (1)
    {
      int digval;
      if (*nptr >= '0' && *nptr <= '0' + max_digit)
        digval = *nptr - '0';
      else if (base == 16)
        {
	  if (*nptr >= 'a' && *nptr <= 'f')
	    digval = *nptr - 'a' + 10;
	  else if (*nptr >= 'A' && *nptr <= 'F')
	    digval = *nptr - 'A' + 10;
	  else
	    break;
	}
      else
        break;

      if (result > cutoff || (result == cutoff && digval > cutlim))
	{
	  if (endptr != NULL)
	    *endptr = (char *) nptr;
	  return UINT64_MAX;
	}
      result *= base;
      result += digval;
      ++nptr;
    }

  if (endptr != NULL)
    *endptr = (char *) nptr;

  /* Avoid 64-bit multiplication.  */
  if (!positive)
    result = -result;

  return result;
}
