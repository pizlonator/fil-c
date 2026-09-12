/* Runtime detection of huge-page support for malloc tests.
   Copyright (C) 2026 Free Software Foundation, Inc.
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

#ifndef SUPPORT_SUPPORT_CHECK_HUGETLB_H
#define SUPPORT_SUPPORT_CHECK_HUGETLB_H

#include <stdbool.h>
#include <sys/cdefs.h>
#undef attribute_hidden
#define attribute_hidden
#include <hugepages.h>	/* For enum thp_mode_t.  */
#undef attribute_hidden

__BEGIN_DECLS

/* Returns true if /sys/kernel/mm/transparent_hugepage/enabled selects
   `madvise' as the active mode (i.e. MADV_HUGEPAGE is honored, but
   THP is not applied automatically).  Returns false on any other
   configuration, on read failure, or on non-Linux systems.  */
bool support_thp_is_madvise (void);

/* Returns true if /proc/sys/vm/nr_hugepages reports a strictly
   positive number of pre-allocated huge pages (the prerequisite for
   MAP_HUGETLB allocations).  Returns false on read failure or on
   non-Linux systems.  */
bool support_hugepages_reserved (void);

/* If the current process is running with GLIBC_TUNABLES requesting
   glibc.malloc.hugetlb=1 or glibc.malloc.hugetlb=2, verifies that the
   kernel can actually satisfy the requested mode.  If not, terminates
   the test with EXIT_UNSUPPORTED.  No-op when no such tunable is set,
   so it is safe to call unconditionally.  */
void support_check_malloc_hugetlb (void);

/* Returns Transparent Huge Pages (THP) mode from
   /sys/kernel/mm/transparent_hugepage/enabled.  */
extern enum thp_mode_t support_get_thp_mode (void);

/* Returns Transparent Huge Pages (THP) page size from
   /sys/kernel/mm/transparent_hugepage/hpage_pmd_size.  */
extern unsigned long int support_get_thp_size (void);

__END_DECLS

#endif /* SUPPORT_SUPPORT_CHECK_HUGETLB_H */
