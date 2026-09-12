/* CPU Affinity inheritance test - common infrastructure.
   Copyright The GNU Toolchain Authors.
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

/* The general idea of this test is to verify that the set of CPUs assigned to
   a task gets inherited by a child (thread or process) of that task.  This is
   a framework that is included by specific APIs for the test, e.g.
   sched_getaffinity/sched_setaffinity and
   pthread_setaffinity_np/pthread_getaffinity_np.  This is a framework, actual
   tests entry points are in nptl/tst-pthread-affinity-inheritance.c and
   sysdeps/unix/sysv/linux/tst-sched-affinity-inheritance.c.

   There are two levels to the test with two different CPU masks.  The first
   level verifies that the affinity set on the main process is inherited by its
   children subprocess or thread.  The second level verifies that a subprocess
   or subthread passes on its affinity to their respective subprocess or
   subthread.  We set a slightly different mask in both levels to ensure that
   they're both inherited.  */

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <support/test-driver.h>
#include <support/xthread.h>
#include <support/xunistd.h>
#include <sys/sysinfo.h>
#include <sys/wait.h>

struct test_param
{
  int nproc_configured;
  cpu_set_t *set;
  cpu_set_t *set_at_startup;
  size_t size;
  bool entry;
};

void __attribute__((noinline))
set_cpu_mask (struct test_param *param, bool entry)
{
  /* Do not determine the number of CPUs based on get_nproc as the test process
     could already be taskset to less CPUs.  */
  int cpus = CPU_COUNT_S (param->size, param->set_at_startup);

  /* Less CPUS for the first level, if that's possible.  */
  if (entry && cpus > 1)
    cpus--;

  /* Only use CPUs which were in mask at start of the test process.  */
  CPU_ZERO_S (param->size, param->set);
  for (int cur = 0; cpus > 0; cur++)
    {
      if (CPU_ISSET_S (cur, param->size, param->set_at_startup))
	{
	  CPU_SET_S (cur, param->size, param->set);
	  cpus--;
	}
    }

  if (cpus != 0)
    FAIL_EXIT1 ("Failed to add all requested CPUs to the affinity set\n");
}

static void
verify_my_affinity (int nproc_configured, size_t size,
		    const cpu_set_t *expected_set)
{
  cpu_set_t *set = CPU_ALLOC (nproc_configured);
  cpu_set_t *xor_set = CPU_ALLOC (nproc_configured);

  if (set == NULL || xor_set == NULL)
    FAIL_EXIT1 ("verify_my_affinity: Failed to allocate cpuset: %m\n");

  get_my_affinity (size, set);

  CPU_XOR_S (size, xor_set, expected_set, set);

  int cpucount = CPU_COUNT_S (size, xor_set);

  if (cpucount > 0)
    {
      FAIL ("Affinity mask not inherited, "
	    "following %d CPUs mismatched in the expected and actual sets:\n",
	    cpucount);
      for (int cur = 0; cpucount > 0; cur++)
	if (CPU_ISSET_S (cur, size, xor_set))
	  {
	    printf ("%d ", cur);
	    cpucount--;
	  }
      printf ("\n");
    }

  CPU_FREE (set);
  CPU_FREE (xor_set);
}

static void *
child_test (void *arg)
{
  struct test_param *param = arg;

  printf ("%d:%d        child\n", getpid (), gettid ());
  verify_my_affinity (param->nproc_configured, param->size, param->set);
  return NULL;
}

void *
do_one_test (void *arg)
{
  void *(*child) (void *) = NULL;
  struct test_param *param = arg;
  bool entry = param->entry;

  if (entry)
    {
      printf ("%d:%d Start test run\n", getpid (), gettid ());
      /* First level: Reenter as a subprocess and then as a subthread.  */
      child = do_one_test;
      set_cpu_mask (param, true);
      set_my_affinity (param->size, param->set);
      /* Ensure that the affinity mask is really set as expected before forking
	 a new process or creating a new thread.  */
      verify_my_affinity (param->nproc_configured, param->size, param->set);
      param->entry = false;
    }
  else
    {
      /* Verification for the first level.  */
      verify_my_affinity (param->nproc_configured, param->size, param->set);

      /* Launch the second level test, launching CHILD_TEST as a subprocess and
	 then as a subthread.  Use a different mask to see if it gets
	 inherited.  */
      child = child_test;
      set_cpu_mask (param, false);
      set_my_affinity (param->size, param->set);
    }

  /* Verify that a child of a thread/process inherits the affinity mask.  */
  printf ("%d:%d%sdo_one_test: fork\n", getpid (), gettid (),
	  entry ? " " : "    ");
  int pid = xfork ();

  if (pid == 0)
    {
      child (param);
      return NULL;
    }

  xwaitpid (pid, NULL, 0);

  /* Verify that a subthread of a thread/process inherits the affinity
     mask.  */
  printf ("%d:%d%sdo_one_test: thread\n", getpid (), gettid (),
	  entry ? " " : "    ");
  pthread_t t = xpthread_create (NULL, child, param);
  xpthread_join (t);

  return NULL;
}

static int
do_test (void)
{
  /* Large enough in case the kernel decides to return the larger mask.  This
     seems to happen on some kernels for S390x.  */
  int num_configured_cpus = get_nprocs_conf ();

  struct test_param param =
    {
      .nproc_configured = num_configured_cpus,
      .set = CPU_ALLOC (num_configured_cpus),
      .set_at_startup = CPU_ALLOC (num_configured_cpus),
      .size = CPU_ALLOC_SIZE (num_configured_cpus),
      .entry = true,
    };

  if (param.set == NULL)
    FAIL_EXIT1 ("error: CPU_ALLOC (%d) for param.set failed\n",
		num_configured_cpus);
  if (param.set_at_startup == NULL)
    FAIL_EXIT1 ("error: CPU_ALLOC (%d) for param.set_at_startup failed\n",
		num_configured_cpus);

  /* Get a list of all available CPUs for this test.  */
  get_my_affinity (param.size, param.set_at_startup);

  do_one_test (&param);

  CPU_FREE (param.set);
  CPU_FREE (param.set_at_startup);

  return 0;
}

#include <support/test-driver.c>
