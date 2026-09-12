/* Verify that TLS blocks and the rseq area do not overlap.
   Copyright (C) 2025-2026 Free Software Foundation, Inc.

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

#include <array_length.h>
#include <elf.h>
#include <link.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <support/check.h>
#include <support/xdlfcn.h>
#include <sys/auxv.h>
#include <sys/param.h>
#include <sys/rseq.h>
#include <thread_pointer.h>
#include <unistd.h>

/* Used to keep track of address ranges.  The ranges are sorted and
   then checked for overlap.  */

struct address_range
{
  const char *prefix;
  const char *label;
  uintptr_t start;
  size_t length;
};

struct address_range ranges[20];
size_t range_count;

static void
add_range (const char *prefix, const char *label,
           const void *start, size_t length)
{
  TEST_VERIFY (start != NULL);
  TEST_VERIFY (length > 0);
  TEST_VERIFY_EXIT (range_count < array_length (ranges));
  ranges[range_count].prefix = prefix;
  ranges[range_count].label = label;
  ranges[range_count].start = (uintptr_t) start;
  ranges[range_count].length = length;
  ++range_count;
}

static int
range_compare (const void *a1, const void *b1)
{
  const struct address_range *a = a1;
  const struct address_range *b = b1;
  if (a->start < b->start)
    return -1;
  if (a->start > b->start)
    return 1;
  return 0;
}

static void
check_for_overlap (void)
{
  qsort (ranges, range_count, sizeof (ranges[0]), range_compare);
  uintptr_t previous_end = ranges[0].start + ranges[0].length - 1;
  for (size_t i = 1; i < range_count; ++i)
    {
      uintptr_t this_end = ranges[i].start + ranges[i].length - 1;
      if (ranges[i].start <= previous_end)
        {
          puts ("error: overlap between address ranges");
          printf ("  %s%s: [0x%lx, 0x%lx)\n",
                  ranges[i - 1].prefix, ranges[i - 1].label,
                  (unsigned long int) ranges[i - 1].start,
                  (unsigned long int) previous_end);
          printf ("  %s%s: [0x%lx, 0x%lx)\n",
                  ranges[i].prefix, ranges[i].label,
                  (unsigned long int) ranges[i].start,
                  (unsigned long int) this_end);
        }
      previous_end = this_end;
    }
}

static void
add_rseq (void)
{
  /* The initial size of 32 bytes is always allocated.  The value
     reported by __rseq_size does not include the alignment, which can
     be larger than 32 if requested by the kernel through the
     auxiliary vector.  */
  size_t size = 32;
  if (__rseq_size > 0)
    size = roundup (__rseq_size, MAX (getauxval (AT_RSEQ_ALIGN), 32));

  printf ("info: adding rseq area of %zu bytes\n", size);
  add_range ("", "rseq area",
             (char *) __thread_pointer () + __rseq_offset, size);
}

/* These functions add the TLS data for all loaded modules to the
   recorded address ranges.  */

static int
dlip_callback (struct dl_phdr_info *info, size_t size, void *ignored)
{
  /* If the dynamic linker does not provide TLS address information,
     there is nothing to register.  */
  if (info->dlpi_tls_data == NULL)
    return 0;

  for (int i = 0; i < info->dlpi_phnum; ++i)
    {
      if (info->dlpi_phdr[i].p_type == PT_TLS)
        {
          printf ("info: adding TLS range for \"%s\" (%zu bytes)\n",
                  info->dlpi_name, (size_t) info->dlpi_phdr[i].p_memsz);
          add_range ("TLS for ",
                     info->dlpi_name[0] != '\0' ? info->dlpi_name : "main",
                     info->dlpi_tls_data, info->dlpi_phdr[i].p_memsz);
          break;
        }
    }
  return 0;
}

/* Returns true if any TLS ranges were found.  */
static void
add_tls_ranges (void)
{
  dl_iterate_phdr (dlip_callback, NULL);
}

volatile __thread int thread_var __attribute__ ((aligned (MAIN_TLS_ALIGN)));

static int
do_test (void)
{
  void *original_brk = sbrk (0);
  void *initial_allocation = malloc (16);

  /* Ensure that the variable is not optimized away.  */
  thread_var = 0;

  printf ("info: rseq area size: %u\n", __rseq_size);

  puts ("info: checking address ranges with initially loaded modules");
  add_range ("", "program break", original_brk, 1);
  add_range ("", "malloc allocation", initial_allocation, 16);
  add_rseq ();
  add_tls_ranges ();
  printf ("info: %zu ranges found\n", range_count);
  check_for_overlap ();
  range_count = 0;

  puts ("info: checking address ranges after dlopen");
  void *handle = xdlopen ("tst-rseq-tls-range-mod.so", RTLD_NOW);
  int *mod_thread_var = xdlsym (handle, "mod_thread_var");
  add_range ("", "program break", original_brk, 1);
  add_range ("", "malloc allocation", initial_allocation, 16);
  add_rseq ();
  add_tls_ranges ();
  {
    bool found_objects = false;
    for (size_t i = 0; i < range_count; ++i)
      if (strchr (ranges[i].label, '/') != NULL)
        found_objects = true;
    if (!found_objects)
      /* __tls_get_addr does not fully work with static dlopen.
         Add some fall-back test data.  */
      add_range ("", "mod_thread_var",
                 mod_thread_var, sizeof (*mod_thread_var));
  }
  printf ("info: %zu ranges found\n", range_count);
  check_for_overlap ();
  xdlclose (handle);

  free (initial_allocation);
  return 0;
}

#include <support/test-driver.c>
