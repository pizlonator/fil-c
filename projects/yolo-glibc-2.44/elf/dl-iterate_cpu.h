/* Iterate over all CPUs, for CPU-specific diagnostics.
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

#ifndef DL_ITERATE_CPU_H
#define DL_ITERATE_CPU_H

#include <dl-affinity.h>
#include <stdbool.h>

struct dl_iterate_cpu
{
  /* Sequential iteration count, starting at 0.  */
  unsigned int processor_index;

  /* Requested CPU.  Can be -1 if affinity could not be set.  */
  int requested_cpu;

  /* Observed current CPU.  -1 if unavailable.  */
  int actual_cpu;

  /* Observed node ID for the CPU.  -1 if unavailable.  */
  int actual_node;

  /* Internal fields to implement the iteration.   */

  /* Affinity as obtained by _dl_iterate_cpu_init, using
     _dl_getaffinity.  Space for 8,192 CPUs.  */
  unsigned long int mask_reference[8192 / sizeof (unsigned long int) / 8];

  /* This array is used by _dl_setaffinity calls.  */
  unsigned long int mask_request[8192 / sizeof (unsigned long int) / 8];

  /* Return value from the initial _dl_getaffinity call.   */
  int length_reference;
};

static void
_dl_iterate_cpu_init (struct dl_iterate_cpu *dic)
{
  dic->length_reference
    = _dl_getaffinity (dic->mask_reference, sizeof (dic->mask_reference));
  /* Prepare for the first _dl_iterate_cpu_next call.  */
  dic->processor_index = -1;
  dic->requested_cpu = -1;
}

static bool
_dl_iterate_cpu_next (struct dl_iterate_cpu *dic)
{
  ++dic->processor_index;

  if (dic->length_reference > 0)
    {
      /* Search for the next CPU to switch to.  */
      while (true)
        {
          ++dic->requested_cpu;

          /* Array index and bit number within the array.  */
          unsigned int long_index
            = dic->requested_cpu / sizeof (unsigned long int) / 8;
          unsigned int bit_index
            = dic->requested_cpu % (sizeof (unsigned long int) * 8);

          if (long_index * sizeof (unsigned long int) >= dic->length_reference)
            /* All possible CPUs have been covered.  */
            return false;

          unsigned long int bit = 1UL << bit_index;
          if (dic->mask_reference[long_index] & bit)
            {
              /* The CPU is available.  Try to select it.  */
              dic->mask_request[long_index] = bit;
              if (_dl_setaffinity (dic->mask_request,
                                   (long_index + 1)
                                   * sizeof (unsigned long int)) < 0)
                {
                  /* Record that we could not perform a CPU request.  */
                  dic->length_reference = -1;

                  if (dic->processor_index > 0)
                    /* We already reported something.  There is no need to
                       continue because the new data is probably not useful.  */
                    return false;
                }

              /* Clear the bit in case the next iteration switches to the
                 next long value.  */
              dic->mask_request[long_index] = 0;

              /* We found a CPU to run on.  */
              break;
            }
        }
    }
  else
    {
      /* No way to set CPU affinity.  Iterate just once.  */
      if (dic->processor_index > 0)
        return false;
    }

  /* Fill in the actual CPU information.  CPU pinning may not actually
     be effective, depending on the container host.  */
  unsigned int cpu, node;
  if (_dl_getcpu (&cpu, &node) < 0)
    {
      /* No CPU information available.  */
      dic->actual_cpu = -1;
      dic->actual_node = -1;
    }
  else
    {
      dic->actual_cpu = cpu;
      dic->actual_node = node;
    }

  return true;
}

#endif /* DL_ITERATE_CPU_H */
