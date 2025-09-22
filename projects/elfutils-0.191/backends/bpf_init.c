/* Initialization of BPF specific backend library.
   This file is part of elfutils.

   This file is free software; you can redistribute it and/or modify
   it under the terms of either

     * the GNU Lesser General Public License as published by the Free
       Software Foundation; either version 3 of the License, or (at
       your option) any later version

   or

     * the GNU General Public License as published by the Free
       Software Foundation; either version 2 of the License, or (at
       your option) any later version

   or both in parallel, as here.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received copies of the GNU General Public License and
   the GNU Lesser General Public License along with this program.  If
   not, see <http://www.gnu.org/licenses/>.  */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#define BACKEND		bpf_
#define RELOC_PREFIX	R_BPF_
#include "libebl_CPU.h"

/* This defines the common reloc hooks based on bpf_reloc.def.  */
#define NO_RELATIVE_RELOC
#define NO_COPY_RELOC
#include "common-reloc.c"


Ebl *
bpf_init (Elf *elf __attribute__ ((unused)),
	  GElf_Half machine __attribute__ ((unused)),
	  Ebl *eh)
{
  /* We handle it.  */
  bpf_init_reloc (eh);
  HOOK (eh, register_info);
  HOOK (eh, disasm);
  HOOK (eh, reloc_simple_type);

  return eh;
}
