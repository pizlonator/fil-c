/* FUSE-based test for mkstemp.  Parallel collision statistics.
   Copyright (C) 2024-2026 Free Software Foundation, Inc.
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

#include <stdlib.h>

#include <array_length.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <support/check.h>
#include <support/fuse.h>
#include <support/support.h>
#include <support/xthread.h>
#include <support/xunistd.h>

/* The number of subprocesses that call mkstemp.  */
static pid_t processes[4];

/* Enough space to record the expected number of replies (62**3) for
   each process.  */
enum { results_allocated = array_length (processes) * 62 * 62 * 62  };

/* The thread will store the results there.  */
static uint64_t *results;

/* Currently used part of the results array.  */
static size_t results_used;

/* Fail with EEXIST (so that mkstemp tries again).  Record observed
   names for later statistical analysis.  */
static void
fuse_thread (struct support_fuse *f, void *closure)
{
  struct fuse_in_header *inh;
  while ((inh = support_fuse_next (f)) != NULL)
    {
      if (support_fuse_handle_mountpoint (f)
          || (inh->nodeid == 1 && support_fuse_handle_directory (f)))
        continue;
      if (inh->opcode != FUSE_LOOKUP || results_used >= results_allocated)
        {
          support_fuse_reply_error (f, EIO);
          continue;
        }

      char *name = support_fuse_cast (LOOKUP, inh);
      TEST_COMPARE_BLOB (name, 3, "new", 3);
      TEST_COMPARE (strlen (name), 9);
      /* Extract 8 bytes of the name: 'w', the X replacements, and the
         null terminator.  Treat it as an uint64_t for easy sorting
         below.  Endianess does not matter because the relative order
         of the entries is not important; sorting is only used to find
         duplicates.  */
      TEST_VERIFY_EXIT (results_used < results_allocated);
      memcpy (&results[results_used], name + 2, 8);
      ++results_used;
      struct fuse_entry_out *out = support_fuse_prepare_entry (f, 2);
      out->attr.mode = S_IFREG | 0600;
      support_fuse_reply_prepared (f);
    }
}

/* Used to sort the results array, to find duplicates.  */
static int
results_sort (const void *a1, const void *b1)
{
  const uint64_t *a = a1;
  const uint64_t *b = b1;
  if (*a < *b)
    return -1;
  if (*a == *b)
    return 0;
  return 1;
}

/* Number of occurrences of certain streak lengths.  */
static size_t streak_lengths[6];

/* Called for every encountered streak.  */
static inline void
report_streak (uint64_t current, size_t length)
{
  if (length > 1)
    {
      printf ("info: name \"ne%.8s\" repeats: %zu\n",
              (char *) &current, length);
      TEST_VERIFY_EXIT (length < array_length (streak_lengths));
    }
  TEST_VERIFY_EXIT (length < array_length (streak_lengths));
  ++streak_lengths[length];
}

static int
do_test (void)
{
  support_fuse_init ();

  results = xmalloc (results_allocated * sizeof (*results));

  struct shared
  {
    /* Used to synchronize the start of all subprocesses, to make it
       more likely to expose concurrency-related bugs.  */
    pthread_barrier_t barrier1;
    pthread_barrier_t barrier2;

    /* Filled in after fork.  */
    char mountpoint[4096];
  };

  /* Used to synchronize the start of all subprocesses, to make it
     more likely to expose concurrency-related bugs.  */
  struct shared *pshared = support_shared_allocate (sizeof (*pshared));
  {
    pthread_barrierattr_t attr;
    xpthread_barrierattr_init (&attr);
    xpthread_barrierattr_setpshared (&attr, PTHREAD_PROCESS_SHARED);
    xpthread_barrierattr_destroy (&attr);
    xpthread_barrier_init (&pshared->barrier1, &attr,
                           array_length (processes) + 1);
    xpthread_barrier_init (&pshared->barrier2, &attr,
                           array_length (processes) + 1);
    xpthread_barrierattr_destroy (&attr);
  }

  for (int i = 0; i < array_length (processes); ++i)
    {
      processes[i] = xfork ();
      if (processes[i] == 0)
        {
          /* Wait for mountpoint initialization.  */
          xpthread_barrier_wait (&pshared->barrier1);
          char *path = xasprintf ("%s/newXXXXXX", pshared->mountpoint);

          /* Park this process until all processes have started.  */
          xpthread_barrier_wait (&pshared->barrier2);
          errno = 0;
          TEST_COMPARE (mkstemp (path), -1);
          TEST_COMPARE (errno, EEXIST);
          free (path);
          _exit (0);
        }
    }

  /* Do this after the forking, to minimize initialization inteference.  */
  struct support_fuse *f = support_fuse_mount (fuse_thread, NULL);
  TEST_VERIFY (strlcpy (pshared->mountpoint, support_fuse_mountpoint (f),
                        sizeof (pshared->mountpoint))
               < sizeof (pshared->mountpoint));
  xpthread_barrier_wait (&pshared->barrier1);

  puts ("info: performing mkstemp calls");
  xpthread_barrier_wait (&pshared->barrier2);

  for (int i = 0; i < array_length (processes); ++i)
    {
      int status;
      xwaitpid (processes[i], &status, 0);
      TEST_COMPARE (status, 0);
    }

  support_fuse_unmount (f);
  xpthread_barrier_destroy (&pshared->barrier2);
  xpthread_barrier_destroy (&pshared->barrier1);

  printf ("info: checking results (count %zu)\n", results_used);
  qsort (results, results_used, sizeof (*results), results_sort);

  uint64_t current = -1;
  size_t streak = 0;
  for (size_t i = 0; i < results_used; ++i)
    if (results[i] == current)
      ++streak;
    else
      {
        report_streak (current, streak);
        current = results[i];
        streak = 1;
      }
  report_streak (current, streak);

  puts ("info: repetition count distribution:");
  for (int i = 1; i < array_length (streak_lengths); ++i)
    printf ("  length %d: %zu\n", i, streak_lengths[i]);
  /* Some arbitrary threshold, hopefully unlikely enough.  In over
     260,000 runs of a simulation of this test, at most 26 pairs were
     observed, and only one three-way collisions.  */
  if (streak_lengths[2] > 30)
    FAIL ("unexpected repetition count 2: %zu", streak_lengths[2]);
  if (streak_lengths[3] > 2)
    FAIL ("unexpected repetition count 3: %zu", streak_lengths[3]);
  for (int i = 4; i < array_length (streak_lengths); ++i)
    if (streak_lengths[i] > 0)
      FAIL ("too many repeats of count %d: %zu", i, streak_lengths[i]);

  free (results);

  return 0;
}

#include <support/test-driver.c>
