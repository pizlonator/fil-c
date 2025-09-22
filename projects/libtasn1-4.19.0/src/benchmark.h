/*
 * Copyright (C) 2011-2022 Free Software Foundation, Inc.
 *
 * This file is part of GnuTLS.
 *
 * GnuTLS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * GnuTLS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef BENCHMARK_H
# define BENCHMARK_H

# include <sys/time.h>
# include <time.h>
# include <signal.h>
# if defined _WIN32
#  include <windows.h>
# endif
# include <time.h>
# include "timespec.h"		/* gnulib gettime */

typedef void (*sighandler_t) (int);

struct benchmark_st
{
  struct timespec start;
  unsigned long size;
  sighandler_t old_handler;
# if defined _WIN32
  HANDLE wtimer;
  HANDLE wthread;
  LARGE_INTEGER alarm_timeout;
# endif
};

extern int benchmark_must_finish;

void start_benchmark (struct benchmark_st *st);
double stop_benchmark (struct benchmark_st *st, const char *metric);

inline static unsigned int
timespec_sub_ms (struct timespec *a, struct timespec *b)
{
  return (a->tv_sec * 1000 + a->tv_nsec / (1000 * 1000) -
	  (b->tv_sec * 1000 + b->tv_nsec / (1000 * 1000)));
}

#endif /* BENCHMARK_H */
