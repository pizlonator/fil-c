/* _dl_executable_postprocess.  Linux version.
   Copyright (C) 2026 Free Software Foundation, Inc.
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

#include "dl-thp-madvise.h"

static inline void
_dl_get_thp_config (void)
{
  /* Check if there is GLIBC_TUNABLES=glibc.elf.thp=[0|1].  */
  if (TUNABLE_GET_FULL (glibc, elf, thp, int32_t, NULL) == 0)
    GL(dl_elf_thp_control) = dl_elf_thp_control_disabled;
  else
    GL(dl_elf_thp_control) = dl_elf_thp_control_enabled;

  /* Return if the tunable is not set or THP is disabled by the
     tunable.  */
  if (GL(dl_elf_thp_control) == dl_elf_thp_control_disabled)
    return;

  verify (DL_MAP_DEFAULT_THP_PAGESIZE <= MAX_THP_PAGESIZE);

  /* NB: Accessing /sys/kernel/mm files is quite expensive and the file
     may not be accessible in containers.  If DL_MAP_DEFAULT_THP_PAGESIZE
     is non-zero, assume THP mode is madvise and always call madvise.
     Since madvise is a fast system call, it adds only a small overhead
     compared to the cost of accessing /sys/kernel/mm files.  */
  if (DL_MAP_DEFAULT_THP_PAGESIZE != 0)
    {
      GL(dl_elf_thp_pagesize) = DL_MAP_DEFAULT_THP_PAGESIZE;
      GL(dl_thp_mode) = thp_mode_madvise;
    }
  else
    {
      GL(dl_thp_mode) = __get_thp_mode ();
      if (GL(dl_thp_mode) == thp_mode_always
	  || GL(dl_thp_mode) == thp_mode_madvise)
	GL(dl_elf_thp_pagesize) = get_capped_thp_size ();
      else
	GL(dl_elf_thp_pagesize) = 0;

      if (GL(dl_elf_thp_pagesize) == 0)
	{
	  GL(dl_elf_thp_control) = dl_elf_thp_control_disabled;
	  GL(dl_thp_mode) = thp_mode_not_supported;
	}
    }
}

static inline void
_dl_executable_postprocess (struct link_map *main_map,
			    const ElfW(Phdr) *phdr, ElfW(Word) phnum)
{
  /* NB: In static executable, PT_GNU_PROPERTY is processed in target
     libc-start.h if it is needed by target.  When ld.so is used, if
     a target doesn't need PT_GNU_PROPERTY, _dl_process_pt_gnu_property
     is an empty function.  */
#ifdef SHARED
  /* Process program headers again, but scan them backwards since
     PT_GNU_PROPERTY is close to the end of program headers.   */
  for (const ElfW(Phdr) *ph = &phdr[phnum]; ph != phdr; --ph)
    if (ph[-1].p_type == PT_GNU_PROPERTY)
      {
	_dl_process_pt_gnu_property (main_map, -1, &ph[-1]);
	break;
      }
#endif

  /* If THP state was not yet initialized, the main executable was mapped
     by the kernel; in that case this function is the only place that can
     apply MADV_HUGEPAGE to the main executable's segments.  Otherwise,
     _dl_get_thp_config has already run earlier in dl_main and
     _dl_map_segments has just mapped the main executable, so
     _dl_postprocess_loadcmd_extra has already done the madvise pass; do
     not repeat it here.  */
  if (GL(dl_elf_thp_control) != dl_elf_thp_control_default)
    return;

   _dl_get_thp_config ();

  /* Return if THP segment load isn't enabled.  */
  if (GL(dl_elf_thp_control) != dl_elf_thp_control_enabled)
    return;

  /* NB: If DL_MAP_DEFAULT_THP_PAGESIZE is non-zero, dl_thp_mode is set
     to thp_mode_madvise.  */
  if (DL_MAP_DEFAULT_THP_PAGESIZE == 0
      && GL(dl_thp_mode) != thp_mode_madvise)
    return;

  /* When we get here, the main executable have been mapped in.  Call
     madvise with MADV_HUGEPAGE for all THP eligible PT_LOAD segments.  */

  const ElfW(Phdr) *ph;

  size_t thp_pagesize = GL(dl_elf_thp_pagesize);

  /* Call __madvise if offset and address of the PT_LOAD segment are
     aligned to THP page size and it is read-only.  */
  for (ph = phdr; ph < &phdr[phnum]; ++ph)
    if (ph->p_type == PT_LOAD
	&& ph->p_memsz >= thp_pagesize
	&& ((ph->p_vaddr | ph->p_offset) & (thp_pagesize - 1)) == 0
	&& (ph->p_flags & (PF_W | PF_R)) == PF_R)
      {
	void *addr = (void *) (main_map->l_addr + ph->p_vaddr);
	int ret = _dl_thp_madvise (addr, ph->p_memsz);
	if (__glibc_unlikely (GLRO(dl_debug_mask) & DL_DEBUG_FILES))
	  _dl_debug_printf ("\
madvise (0x%0*lx, 0x%0*lx, MADV_HUGEPAGE) returns %d\n",
(int) sizeof (void *) * 2,
(unsigned long int) (main_map->l_addr + ph->p_vaddr),
(int) sizeof (void *) * 2,
(unsigned long int) ph->p_memsz,
ret);
      }
}
