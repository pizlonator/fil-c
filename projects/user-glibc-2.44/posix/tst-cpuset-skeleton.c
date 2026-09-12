/* Test that CPU_* macros comply with their specifications.

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

#include <limits.h>
#include <sched.h>
#include <stdio.h>

#include <support/check.h>
#include <support/test-driver.h>

static int
do_test (void)
{
  cpu_set_t *cpusetp_A = NULL;
  cpu_set_t *cpusetp_B = NULL;
  cpu_set_t *cpusetp_C = NULL;

  size_t setsz __attribute__ ((unused)) = GET_SIZE();

  TEST_VERIFY (CPU_ALLOC_SIZE (-1) == 0);
  TEST_VERIFY (CPU_ALLOC_SIZE (0) == 0);
  TEST_VERIFY (CPU_ALLOC_SIZE (1) == sizeof (__cpu_mask));
  TEST_VERIFY (CPU_ALLOC_SIZE (INT_MAX) > 0);

  PREPARE_CPU_SET_TO_FAULT_BEFORE(cpusetp_A);
  PREPARE_CPU_SET_TO_FAULT(cpusetp_B);
  PREPARE_CPU_SET(cpusetp_C);

  /* Bad actor access, negative CPU number */
  LOCAL_CPU_SET (-1, setsz, cpusetp_A);
  TEST_VERIFY (!LOCAL_CPU_ISSET (-1, setsz, cpusetp_A));

  /* Bad actor access, above CPU number */
  LOCAL_CPU_SET (LOCAL_NUM_CPUS, setsz, cpusetp_B);
  TEST_VERIFY (!LOCAL_CPU_ISSET (LOCAL_NUM_CPUS, setsz, cpusetp_B));

  LOCAL_CPU_ZERO (setsz, cpusetp_A);
  LOCAL_CPU_ZERO (setsz, cpusetp_B);
  LOCAL_CPU_ZERO (setsz, cpusetp_C);

  for (int cpu = 0; cpu < LOCAL_NUM_CPUS; cpu += 2)
    {
      /* Set A = 0x55..55 */
      LOCAL_CPU_SET (cpu, setsz, cpusetp_A);
      TEST_VERIFY (LOCAL_CPU_ISSET (cpu, setsz, cpusetp_A));
    }
  for (int cpu = 1; cpu < LOCAL_NUM_CPUS; cpu += 2)
    {
      /* Set B = 0xAA..AA */
      LOCAL_CPU_SET (cpu, setsz, cpusetp_B);
      TEST_VERIFY (LOCAL_CPU_ISSET (cpu, setsz, cpusetp_B));
    }

  /* Ensure CPU_COUNT matches expected count */
  TEST_VERIFY (LOCAL_CPU_COUNT (setsz, cpusetp_A) == LOCAL_CPU_COUNT (setsz, cpusetp_B));
  TEST_VERIFY (LOCAL_CPU_COUNT (setsz, cpusetp_A) == LOCAL_NUM_CPUS / 2);

  LOCAL_CPU_AND (setsz, cpusetp_C, cpusetp_A, cpusetp_B);
  for (int cpu = 0; cpu < LOCAL_NUM_CPUS; cpu++)
    {
      /* A setsz,  B == 0 */
      TEST_VERIFY (!LOCAL_CPU_ISSET (cpu, setsz, cpusetp_C));
    }

  LOCAL_CPU_OR (setsz, cpusetp_C, cpusetp_A, cpusetp_B);
  for (int cpu = 0; cpu < LOCAL_NUM_CPUS; cpu++)
    {
      /* A | B == 0xFF..FF */
      TEST_VERIFY (LOCAL_CPU_ISSET (cpu, setsz, cpusetp_C));
    }

  /* Check that CPU_ZERO actually does something */
  TEST_VERIFY (LOCAL_CPU_COUNT (setsz, cpusetp_C) == LOCAL_NUM_CPUS);
  LOCAL_CPU_ZERO (setsz, cpusetp_C);
  TEST_VERIFY (LOCAL_CPU_COUNT (setsz, cpusetp_C) == 0);

  LOCAL_CPU_XOR (setsz, cpusetp_C, cpusetp_A, cpusetp_A);
  for (int cpu = 0; cpu < LOCAL_NUM_CPUS; cpu++)
    {
      /* A ^ A == 0 */
      TEST_VERIFY (!LOCAL_CPU_ISSET (cpu, setsz, cpusetp_C));
    }

  LOCAL_CPU_XOR (setsz, cpusetp_C, cpusetp_A, cpusetp_B);
  for (int cpu = 0; cpu < LOCAL_NUM_CPUS; cpu++)
    {
      /* C = A ^ B == 0xFF..FF */
      TEST_VERIFY (LOCAL_CPU_ISSET (cpu, setsz, cpusetp_C));
    }

  for (int cpu = 1; cpu < LOCAL_NUM_CPUS; cpu += 2)
    {
      /* C = 0x55..55 */
      LOCAL_CPU_CLR (cpu, setsz, cpusetp_C);
      TEST_VERIFY (!LOCAL_CPU_ISSET (cpu, setsz, cpusetp_C));
    }

  TEST_VERIFY (LOCAL_CPU_EQUAL (setsz, cpusetp_A, cpusetp_C));

  CLEAN_CPU_SET(cpusetp_C);
  CLEAN_CPU_SET_TO_FAULT(cpusetp_B);
  CLEAN_CPU_SET_TO_FAULT_BEFORE(cpusetp_A);

  return 0;
}

#include <support/test-driver.c>
