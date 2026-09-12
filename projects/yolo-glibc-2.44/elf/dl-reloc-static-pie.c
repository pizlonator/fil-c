/* Support for relocating static PIE.
   Copyright (C) 2017-2026 Free Software Foundation, Inc.
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

#if ENABLE_STATIC_PIE
/* Mark symbols hidden in static PIE for early self relocation to work.  */
# pragma GCC visibility push(hidden)
#include <assert.h>
#include <unistd.h>
#include <ldsodefs.h>

#include <dl-machine.h>
#include <dl-debug.h>
#include <dl-symbol-redir-ifunc.h>

#define RESOLVE_MAP(map, scope, sym, version, flags) map
#include "dynamic-link.h"
#include "get-dynamic-info.h"

/* Phase 1: relocate static PIE - non-IRELATIVE pass.  IFUNC resolvers are
   deferred to _dl_relocate_static_pie_ifunc so that csu/libc-start.c can
   initialise the TCB (and write the stack-protector canary into it) between
   the two passes.  */
void
_dl_relocate_static_pie (void)
{
  struct link_map *main_map = _dl_get_dl_main_map ();

  /* NB: elf_machine_load_address () returns the run-time load address
     of static PIE.  The l_addr field contains the difference between the
     link-time load address in the ELF file and the run-time load address
     in memory.  We must subtract the link-time load address of static PIE,
     which can be non-zero, when computing the l_addr field.  Since static
     PIE usually doesn't have PT_PHDR segment, use p_vaddr of the PT_LOAD
     segment with offset == 0 as the load address of static PIE.  */
  ElfW(Addr) file_p_vaddr = 0;
  const ElfW(Phdr) *ph, *phdr = GL(dl_phdr);
  size_t phnum = GL(dl_phnum);
  for (ph = phdr; ph < &phdr[phnum]; ++ph)
    switch (ph->p_type)
      {
      case PT_LOAD:
	/* Skip the empty PT_LOAD segment at offset 0.  */
	if (ph->p_filesz != 0 && ph->p_offset == 0)
	  file_p_vaddr = ph->p_vaddr;
	break;
      case PT_DYNAMIC:
	main_map->l_ld_readonly = (ph->p_flags & PF_W) == 0;
	break;
      default:
	break;
      }

  /* Figure out the run-time load address of static PIE.  */
  ElfW(Addr) l_addr = elf_machine_load_address ();
  main_map->l_addr = l_addr - file_p_vaddr;

  /* Read our own dynamic section and fill in the info array.  */
  main_map->l_ld = ((void *) l_addr + elf_machine_dynamic ());

  elf_get_dynamic_info (main_map, false, true);

# ifdef ELF_MACHINE_BEFORE_RTLD_RELOC
  ELF_MACHINE_BEFORE_RTLD_RELOC (main_map, main_map->l_info);
# endif

  /* Relocate ourselves so we can do normal function calls and data access
     using the global offset table.  IRELATIVE entries are deferred.  */
  ELF_DYNAMIC_RELOCATE_NOIFUNC (main_map, NULL, 0, 0);

  /* Initialize _r_debug_extended.  */
  struct r_debug *r = _dl_debug_initialize (0, LM_ID_BASE);
  r->r_state = RT_CONSISTENT;

  /* Set up debugging before the debugger is notified for the first
     time.  */
  elf_setup_debug_entry (main_map, r);
}

/* Phase 2: run the deferred IRELATIVE entries for the static-pie main map.
   Must be called after the TCB is set up and the stack-protector canary is
   written, so that an instrumented IFUNC resolver does not fault.  */
void
_dl_relocate_static_pie_ifunc (void)
{
  struct link_map *main_map = _dl_get_dl_main_map ();
  ELF_DYNAMIC_RELOCATE_IFUNC (main_map, NULL, 0, 0);
  main_map->l_relocated = 1;
}
#endif
