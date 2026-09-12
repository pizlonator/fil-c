/* Initialization code for TLS in statically linked application.
   Copyright (C) 2002-2026 Free Software Foundation, Inc.
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

#include <startup.h>
#include <errno.h>
#include <ldsodefs.h>
#include <tls.h>
#include <dl-tls.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/param.h>
#include <array_length.h>
#include <pthreadP.h>
#include <dl-call_tls_init_tp.h>
#include <dl-extra_tls.h>
#include <array_length.h>
#include <elf/dl-tls_block_align.h>
#include <dl-symbol-redir-ifunc.h>

#ifdef SHARED
 #error makefile bug, this file is for static only
#endif

dtv_t _dl_static_dtv[2 + TLS_SLOTINFO_SURPLUS];


static struct dtv_slotinfo_list static_slotinfo =
  {
   /* Allocate an array of 2 + TLS_SLOTINFO_SURPLUS elements.  */
   .slotinfo =  { [array_length (_dl_static_dtv) - 1] = { 0 } },
  };

/* Highest dtv index currently needed.  */
size_t _dl_tls_max_dtv_idx;
/* Flag signalling whether there are gaps in the module ID allocation.  */
bool _dl_tls_dtv_gaps;
/* Information about the dtv slots.  */
struct dtv_slotinfo_list *_dl_tls_dtv_slotinfo_list;
/* Number of modules in the static TLS block.  */
size_t _dl_tls_static_nelem;
/* Size of the static TLS block.  */
size_t _dl_tls_static_size;
/* Size actually allocated in the static TLS block.  */
size_t _dl_tls_static_used;
/* Alignment requirement of the static TLS block.  */
size_t _dl_tls_static_align;
/* Size of surplus space in the static TLS area for dynamically
   loaded modules with IE-model TLS or for TLSDESC optimization.
   See comments in elf/dl-tls.c where it is initialized.  */
size_t _dl_tls_static_surplus;
/* Remaining amount of static TLS that may be used for optimizing
   dynamic TLS access (e.g. with TLSDESC).  */
size_t _dl_tls_static_optional;

/* Generation counter for the dtv.  */
size_t _dl_tls_generation;


/* Additional definitions needed by TLS initialization.  */
#ifdef TLS_INIT_HELPER
TLS_INIT_HELPER
#endif

static void
init_slotinfo (struct link_map *main_map)
{
  /* Create the slotinfo list.  Note that the type of static_slotinfo
     has effectively a zero-length array, so we cannot use the size of
     static_slotinfo to determine the array length.  */
  static_slotinfo.len = array_length (_dl_static_dtv);
  /* static_slotinfo.next = NULL; -- Already zero.  */

  main_map->l_tls_modid = 1;
  static_slotinfo.slotinfo[1].map = main_map;
  main_map->l_tls_in_slotinfo = 1;

  /* The slotinfo list.  Will be extended by the code doing dynamic
     linking.  */
  GL(dl_tls_max_dtv_idx) = 1;
  GL(dl_tls_dtv_slotinfo_list) = &static_slotinfo;
}

/* Perform TLS setup for statically linked binaries.  Similar to
   init_tls in elf/rtld.c.  */
void
__libc_setup_tls (void)
{
  size_t memsz = 0;
  size_t filesz = 0;
  void *initimage = NULL;
  size_t align = 0;
  size_t max_align = TCB_ALIGNMENT;
  const ElfW(Phdr) *phdr;

  struct link_map *main_map = GL(dl_ns)[LM_ID_BASE]._ns_loaded;

  __tls_pre_init_tp ();

  /* Look through the TLS segment if there is any.  */
  for (phdr = _dl_phdr; phdr < &_dl_phdr[_dl_phnum]; ++phdr)
    if (phdr->p_type == PT_TLS)
      {
	/* Remember the values we need.  */
	memsz = phdr->p_memsz;
	filesz = phdr->p_filesz;
	initimage = (void *) phdr->p_vaddr + main_map->l_addr;
	align = phdr->p_align;
	if (phdr->p_align > max_align)
	  max_align = phdr->p_align;

	main_map->l_tls_align = align;
	main_map->l_tls_blocksize = memsz;
	main_map->l_tls_initimage = initimage;
	main_map->l_tls_initimage_size = filesz;
	init_slotinfo (main_map);
	break;
      }

  /* Number of elements in the static TLS block.  */
  GL(dl_tls_static_nelem) = GL(dl_tls_max_dtv_idx);

  /* Calculate the size of the static TLS surplus, with 0 auditors.  */
  _dl_tls_static_surplus_init (0);

  /* Calculate the TLS block size.  */
  _dl_determine_tlsoffset ();

  /* See _dl_allocate_tls_storage in elf/dl-tls.c.  */
  void *tcbp;
  {
    size_t size = _dl_tls_block_size_with_pre ();
    void *allocated = _dl_early_allocate (size + GLRO (dl_tls_static_align));
    if (allocated == NULL)
      _startup_fatal_tls_error ();
    tcbp = _dl_tls_block_align (size, allocated);
  }

  /* Initialize the dtv.  [0] is the length, [1] the generation counter.  */
  _dl_static_dtv[0].counter = array_length (_dl_static_dtv) - 2;

  /* Install the pointer to the DTV.  See allocate_dtv in elf/dl-tls.c.  */
  INSTALL_DTV (tcbp, _dl_static_dtv);

  /* _dl_allocate_tls_init uses recursive locking and the TCB, so this
     has to come first. */
  call_tls_init_tp (tcbp);

  /* Initialize the TLS image for the allocated TCB.  */
  _dl_allocate_tls_init (tcbp, true);
}
