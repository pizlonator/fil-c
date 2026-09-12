/* Test of semaphores.
   Copyright (C) 2007-2026 Free Software Foundation, Inc.
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

#include <time.h>
#include <sys/time.h>


#define TF_PREPARE \
  struct timespec ts; \
  struct timeval tv; \
  gettimeofday (&tv, NULL); \
  TIMEVAL_TO_TIMESPEC (&tv, &ts); \
  ts.tv_sec += 60;

#define SEM_WAIT(s) sem_timedwait (s, &ts)

#include "tst-sem11.c"
