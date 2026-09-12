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

#include <support/next_to_fault.h>

#define LOCAL_NUM_CPUS 2048
#define LOCAL_CPU_SETSIZE LOCAL_NUM_CPUS / 8

#define PREPARE_CPU_SET(X) \
  X = CPU_ALLOC (LOCAL_NUM_CPUS);

/* Create a mapping so that access to the page before the cpuset generates a
   fault. The aim is to check the behavior for negative values since the
   interface accepts signed int. */
#define PREPARE_CPU_SET_TO_FAULT_BEFORE(X) \
  size_t local_sz_##X = CPU_ALLOC_SIZE(LOCAL_NUM_CPUS); \
  struct support_next_to_fault local_##X = support_next_to_fault_allocate_before(local_sz_##X); \
  X = (cpu_set_t *) local_##X.buffer;

/* Create a mapping so that access to the page after the cpuset generates a
   fault. The aim is to check the behavior for values above CPU count since the
   interface accepts signed int. */
#define PREPARE_CPU_SET_TO_FAULT(X) \
  size_t local_sz_##X = CPU_ALLOC_SIZE(LOCAL_NUM_CPUS); \
  struct support_next_to_fault local_##X = support_next_to_fault_allocate(local_sz_##X); \
  X = (cpu_set_t *) local_##X.buffer;

#define GET_SIZE() (size_t) CPU_ALLOC_SIZE(LOCAL_NUM_CPUS)

#define LOCAL_CPU_ZERO(sz, cpusetp) CPU_ZERO_S(sz, cpusetp)
#define LOCAL_CPU_SET(cpu, sz, cpusetp) CPU_SET_S(cpu, sz, cpusetp)
#define LOCAL_CPU_CLR(cpu, sz, cpusetp) CPU_CLR_S(cpu, sz, cpusetp)
#define LOCAL_CPU_ISSET(cpu, sz, cpusetp) CPU_ISSET_S(cpu, sz, cpusetp)
#define LOCAL_CPU_COUNT(sz, cpusetp) CPU_COUNT_S(sz, cpusetp)
#define LOCAL_CPU_AND(sz, destsetp, srcsetp1, srcsetp2) \
  CPU_AND_S(sz, destsetp, srcsetp1, srcsetp2)
#define LOCAL_CPU_OR(sz, destsetp, srcsetp1, srcsetp2) \
  CPU_OR_S(sz, destsetp, srcsetp1, srcsetp2)
#define LOCAL_CPU_XOR(sz, destsetp, srcsetp1, srcsetp2) \
  CPU_XOR_S(sz, destsetp, srcsetp1, srcsetp2)
#define LOCAL_CPU_EQUAL(sz, setp1, setp2) CPU_EQUAL_S(sz, setp1, setp2)

#define CLEAN_CPU_SET(cpusetp) CPU_FREE(cpusetp)
#define CLEAN_CPU_SET_TO_FAULT_BEFORE(X) support_next_to_fault_free(&local_##X)
#define CLEAN_CPU_SET_TO_FAULT(X) support_next_to_fault_free(&local_##X)

#include "tst-cpuset-skeleton.c"
