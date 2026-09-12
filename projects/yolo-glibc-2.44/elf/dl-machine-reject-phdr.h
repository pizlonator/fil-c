/* Machine-dependent program header inspection for the ELF loader.
   Copyright (C) 2014-2026 Free Software Foundation, Inc.
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

#ifndef _DL_MACHINE_REJECT_PHDR_H
#define _DL_MACHINE_REJECT_PHDR_H 1

#include <stdbool.h>

/* Machine-specific data collected during the program-header scan for use
   by elf_machine_reject_phdr_p.  Ports that override elf_machine_reject_phdr_p
   must define their own layout; this generic version carries no data.  */
struct dl_machine_phdr_info
{
};

/* Initialize INFO before the program-header scan begins.  */
static inline void
elf_machine_phdr_info_init (struct dl_machine_phdr_info *info
			    __attribute__ ((__unused__)))
{
}

/* Called once per ELF program header PH during the scan.  Records any
   machine-specific data from PH that elf_machine_reject_phdr_p needs.  */
static inline void
elf_machine_phdr_collect (struct dl_machine_phdr_info *info
			  __attribute__ ((__unused__)),
			  const ElfW(Phdr) *ph __attribute__ ((__unused__)))
{
}

/* Return true iff the program-header data collected in INFO is incompatible
   with the running host.  */
static inline bool
elf_machine_reject_phdr_p (const struct dl_machine_phdr_info *info
			   __attribute__ ((__unused__)),
			   struct link_map *map __attribute__ ((__unused__)),
			   int fd __attribute__ ((__unused__)))
{
  return false;
}

#endif /* dl-machine-reject-phdr.h */
