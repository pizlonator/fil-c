/* Helpers for utimes/utimens conversions.
   Copyright (C) 2015-2026 Free Software Foundation, Inc.
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
#include <hurd/hurd_types.h>
#include <stddef.h>
#include <sys/time.h>

static inline bool
check_tval (const struct timeval *tvp)
{
  return tvp->tv_usec >= 0 && tvp->tv_usec < USEC_PER_SEC;
}

/* Initializes atime/mtime timespec structures from an array of timeval.  */
static inline error_t
utime_ts_from_tval (const struct timeval tvp[2],
                    struct timespec *atime, struct timespec *mtime)
{
  if (tvp == NULL)
    {
      /* Setting the number of nanoseconds to UTIME_NOW tells the
         underlying filesystems to use the current time.  */
      atime->tv_sec = 0;
      atime->tv_nsec = UTIME_NOW;
      mtime->tv_sec = 0;
      mtime->tv_nsec = UTIME_NOW;
    }
  else
    {
      if (!check_tval (&tvp[0]))
	return EINVAL;
      if (!check_tval (&tvp[1]))
	return EINVAL;

      TIMEVAL_TO_TIMESPEC (&tvp[0], atime);
      TIMEVAL_TO_TIMESPEC (&tvp[1], mtime);
    }
  return 0;
}

/* Initializes atime/mtime time_value_t structures from an array of timeval.  */
static inline error_t
utime_tvalue_from_tval (const struct timeval tvp[2],
                        time_value_t *atime, time_value_t *mtime)
{
  if (tvp == NULL)
    /* Setting the number of microseconds to `-1' tells the
       underlying filesystems to use the current time.  */
    atime->microseconds = mtime->microseconds = -1;
  else
    {
      if (!check_tval (&tvp[0]))
	return EINVAL;
      if (!check_tval (&tvp[1]))
	return EINVAL;

      atime->seconds = tvp[0].tv_sec;
      atime->microseconds = tvp[0].tv_usec;
      mtime->seconds = tvp[1].tv_sec;
      mtime->microseconds = tvp[1].tv_usec;
    }
  return 0;
}

/* Changes the access time of the file behind PORT using a timeval array.  */
static inline error_t
hurd_futimes (const file_t port, const struct timeval tvp[2])
{
  error_t err;
  struct timespec atime, mtime;

  err = utime_ts_from_tval (tvp, &atime, &mtime);
  if (err)
    return err;

  err = __file_utimens (port, atime, mtime);

  if (err == MIG_BAD_ID || err == EOPNOTSUPP)
    {
      time_value_t atim, mtim;

      err = utime_tvalue_from_tval (tvp, &atim, &mtim);
      if (err)
	return err;

      err = __file_utimes (port, atim, mtim);
    }

  return err;
}

static inline bool
check_tspec (const struct timespec *tsp)
{
  return tsp->tv_nsec == UTIME_NOW
      || tsp->tv_nsec == UTIME_OMIT
      || tsp->tv_nsec >= 0 && tsp->tv_nsec < NSEC_PER_SEC;
}

/* Initializes atime/mtime timespec structures from an array of timespec.  */
static inline error_t
utime_ts_from_tspec (const struct timespec tsp[2],
                     struct timespec *atime, struct timespec *mtime)
{
  if (tsp == NULL)
    {
      /* Setting the number of nanoseconds to UTIME_NOW tells the
         underlying filesystems to use the current time.  */
      atime->tv_sec = 0;
      atime->tv_nsec = UTIME_NOW;
      mtime->tv_sec = 0;
      mtime->tv_nsec = UTIME_NOW;
    }
  else
    {
      if (!check_tspec (&tsp[0]))
	return EINVAL;
      if (!check_tspec (&tsp[1]))
	return EINVAL;

      *atime = tsp[0];
      *mtime = tsp[1];
    }
  return 0;
}

/* Initializes atime/mtime time_value_t structures from an array of timespec.  */
static inline error_t
utime_tvalue_from_tspec (const struct timespec tsp[2],
                         time_value_t *atime, time_value_t *mtime)
{
  if (tsp == NULL)
    /* Setting the number of microseconds to `-1' tells the
       underlying filesystems to use the current time.  */
    atime->microseconds = mtime->microseconds = -1;
  else
    {
      if (!check_tspec (&tsp[0]))
	return EINVAL;
      if (!check_tspec (&tsp[1]))
	return EINVAL;

      if (tsp[0].tv_nsec == UTIME_NOW)
	atime->microseconds = -1;
      else if (tsp[0].tv_nsec == UTIME_OMIT)
	atime->microseconds = -2;
      else
	TIMESPEC_TO_TIME_VALUE (atime, &(tsp[0]));
      if (tsp[1].tv_nsec == UTIME_NOW)
	mtime->microseconds = -1;
      else if (tsp[1].tv_nsec == UTIME_OMIT)
	mtime->microseconds = -2;
      else
	TIMESPEC_TO_TIME_VALUE (mtime, &(tsp[1]));
    }
  return 0;
}

/* Changes the access time of the file behind PORT using a timespec array.  */
static inline error_t
hurd_futimens (const file_t port, const struct timespec tsp[2])
{
  error_t err;
  struct timespec atime, mtime;

  err = utime_ts_from_tspec (tsp, &atime, &mtime);
  if (err)
    return err;

  err = __file_utimens (port, atime, mtime);

  if (err == MIG_BAD_ID || err == EOPNOTSUPP)
    {
      time_value_t atim, mtim;

      err = utime_tvalue_from_tspec (tsp, &atim, &mtim);
      if (err)
	return err;

      err = __file_utimes (port, atim, mtim);
    }

  return err;
}
