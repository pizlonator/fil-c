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

#define LOCAL_NUM_CPUS CPU_SETSIZE

/* Create a mapping so that access to the page before the cpuset generates a
   fault. The aim is to check the behavior for negative values since the
   interface accepts signed int. */
#define PREPARE_CPU_SET_TO_FAULT_BEFORE(X) \
  struct support_next_to_fault local_##X = support_next_to_fault_allocate_before(sizeof(*X)); \
  X = (cpu_set_t *) local_##X.buffer;

/* Create a mapping so that access to the page after the cpuset generates a
   fault. The aim is to check the behavior for values above CPU count since the
   interface accepts signed int. */
#define PREPARE_CPU_SET_TO_FAULT(X) \
  struct support_next_to_fault local_##X = support_next_to_fault_allocate(sizeof(*X)); \
  X = (cpu_set_t *) local_##X.buffer;

#define PREPARE_CPU_SET(X) \
  cpu_set_t local_##X = {}; \
  X = &local_##X;

#define GET_SIZE() (size_t) sizeof (cpu_set_t)

#define LOCAL_CPU_ZERO(sz, cpusetp) CPU_ZERO(cpusetp)
#define LOCAL_CPU_SET(cpu, sz, cpusetp) CPU_SET(cpu, cpusetp)
#define LOCAL_CPU_CLR(cpu, sz, cpusetp) CPU_CLR(cpu, cpusetp)
#define LOCAL_CPU_ISSET(cpu, sz, cpusetp) CPU_ISSET(cpu, cpusetp)
#define LOCAL_CPU_COUNT(sz, cpusetp) CPU_COUNT(cpusetp)
#define LOCAL_CPU_AND(sz, destsetp, srcsetp1, srcsetp2) \
  CPU_AND(destsetp, srcsetp1, srcsetp2)
#define LOCAL_CPU_OR(sz, destsetp, srcsetp1, srcsetp2) \
  CPU_OR(destsetp, srcsetp1, srcsetp2)
#define LOCAL_CPU_XOR(sz, destsetp, srcsetp1, srcsetp2) \
  CPU_XOR(destsetp, srcsetp1, srcsetp2)
#define LOCAL_CPU_EQUAL(sz, setp1, setp2) CPU_EQUAL(setp1, setp2)

#define CLEAN_CPU_SET(X)
#define CLEAN_CPU_SET_TO_FAULT_BEFORE(X) support_next_to_fault_free(&local_##X)
#define CLEAN_CPU_SET_TO_FAULT(X) support_next_to_fault_free(&local_##X)

#include "tst-cpuset-skeleton.c"
