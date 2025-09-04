/* Check the VMA name decoration.
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

#include <stdlib.h>
#include <string.h>
#include <support/check.h>
#include <support/support.h>
#include <support/test-driver.h>
#include <support/xstdio.h>
#include <support/xthread.h>
#include <support/xunistd.h>
#include <sys/mman.h>

#ifndef MAP_STACK
# define MAP_STACK 0
#endif

static pthread_barrier_t b;

static int expected_n_arenas;

static void *
tf (void *closure)
{
  void *p = xmalloc (1024);

  /* Wait the thread startup, so thread stack is allocated.  */
  xpthread_barrier_wait (&b);

  /* Wait the test to read the process mapping.  */
  xpthread_barrier_wait (&b);

  free (p);

  return NULL;
}

struct proc_maps_t
{
  int n_def_threads;
  int n_user_threads;
  int n_arenas;
  int n_malloc_mmap;
};

static struct proc_maps_t
read_proc_maps (void)
{
  if (test_verbose)
    printf ("=== print process %jd memory mapping ===\n",
	    (intmax_t) getpid ());
  struct proc_maps_t r = { 0 };

  FILE *f = xfopen ("/proc/self/maps", "r");
  char *line = NULL;
  size_t line_len = 0;
  while (xgetline (&line, &line_len, f))
    {
      if (test_verbose)
	printf ("%s", line);
      if (strstr (line, "[anon: glibc: pthread stack:") != NULL)
	r.n_def_threads++;
      else if (strstr (line, "[anon: glibc: pthread user stack:") != NULL)
	r.n_user_threads++;
      else if (strstr (line, "[anon: glibc: malloc arena]") != NULL)
	r.n_arenas++;
      else if (strstr (line, "[anon: glibc: malloc]") != NULL)
	r.n_malloc_mmap++;
      /* On some architectures and depending on the page size, the loader can
	 also allocate some memory during dependencies loading and it will be
	 marked as 'loader malloc'.  However, if the system page size is large
	 enough, the initial data page will be enough for all required
	 allocation and there will be no extra loader mmap.  To avoid false
	 negatives, the test does not check for such pages.  */
    }
  free (line);
  xfclose (f);

  if (test_verbose)
    printf ("===\n");
  return r;
}

static void
do_test_threads (bool set_guard)
{
  enum
    {
      num_def_threads  = 8,
      num_user_threads = 2,
      num_threads = num_def_threads + num_user_threads,
    };

  xpthread_barrier_init (&b, NULL, num_threads + 1);

  /* Issue a large malloc to trigger a mmap call.  */
  void *p = xmalloc (256 * 1024);

  pthread_t thr[num_threads];
  {
    int i = 0;
    for (; i < num_threads - num_user_threads; i++)
      {
	pthread_attr_t attr;
	xpthread_attr_init (&attr);
	/* The guard page is not annotated.  */
	if (!set_guard)
	  xpthread_attr_setguardsize (&attr, 0);
	thr[i] = xpthread_create (&attr, tf, NULL);
	xpthread_attr_destroy (&attr);
      }
    for (; i < num_threads; i++)
      {
	pthread_attr_t attr;
	xpthread_attr_init (&attr);
	size_t stacksize = support_small_thread_stack_size ();
	void *stack = xmmap (0,
			     stacksize,
			     PROT_READ | PROT_WRITE,
			     MAP_PRIVATE|MAP_ANONYMOUS|MAP_STACK,
			     -1);
	xpthread_attr_setstack (&attr, stack, stacksize);
	if (!set_guard)
	  xpthread_attr_setguardsize (&attr, 0);
	thr[i] = xpthread_create (&attr, tf, NULL);
	xpthread_attr_destroy (&attr);
      }
  }

  /* Wait all threads to finshed statup and stack allocation.  */
  xpthread_barrier_wait (&b);

  {
    struct proc_maps_t r = read_proc_maps ();
    TEST_COMPARE (r.n_def_threads, num_def_threads);
    TEST_COMPARE (r.n_user_threads, num_user_threads);
    TEST_COMPARE (r.n_arenas, expected_n_arenas);
    TEST_COMPARE (r.n_malloc_mmap, 1);
  }

  /* Let the threads finish.  */
  xpthread_barrier_wait (&b);

  for (int i = 0; i < num_threads; i++)
    xpthread_join (thr[i]);

  {
    struct proc_maps_t r = read_proc_maps ();
    TEST_COMPARE (r.n_def_threads, 0);
    TEST_COMPARE (r.n_user_threads, 0);
    TEST_COMPARE (r.n_arenas, expected_n_arenas);
    TEST_COMPARE (r.n_malloc_mmap, 1);
  }

  free (p);
}

static void
do_prepare (int argc, char *argv[])
{
  TEST_VERIFY_EXIT (argc == 2);
  expected_n_arenas = strtol (argv[1], NULL, 10);
  expected_n_arenas = expected_n_arenas - 1;
}
#define PREPARE do_prepare

static int
do_test (void)
{
  support_need_proc ("Reads /proc/self/maps to get stack names.");

  if (!support_set_vma_name_supported ())
    FAIL_UNSUPPORTED ("kernel does not support PR_SET_VMA_ANON_NAME");

  do_test_threads (false);
  do_test_threads (true);

  return 0;
}

#include <support/test-driver.c>
