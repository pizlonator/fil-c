/* Test program for libdwfl symbol resolving
   Copyright (C) 2013 Red Hat, Inc.
   This file is part of elfutils.

   This file is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include <config.h>
#include <assert.h>
#include <inttypes.h>
#include ELFUTILS_HEADER(dwfl)
#include <elf.h>
#include <dwarf.h>
#include <argp.h>
#include <stdio.h>
#include <stdio_ext.h>
#include <stdlib.h>
#include <string.h>

static const char *
gelf_type (GElf_Sym *sym)
{
  switch (GELF_ST_TYPE (sym->st_info))
    {
    case STT_NOTYPE:
      return "NOTYPE";
    case STT_OBJECT:
      return "OBJECT";
    case STT_FUNC:
      return "FUNC";
    case STT_SECTION:
      return "SECTION";
    case STT_FILE:
      return "FILE";
    case STT_COMMON:
      return "COMMON";
    case STT_TLS:
      return "TLS";
    default:
      return "UNKNOWN";
    }
}

static const char *
gelf_bind (GElf_Sym *sym)
{
  switch (GELF_ST_BIND (sym->st_info))
    {
    case STB_LOCAL:
      return "LOCAL";
    case STB_GLOBAL:
      return "GLOBAL";
    case STB_WEAK:
      return "WEAK";
    default:
      return "UNKNOWN";
    }
}

static int
gelf_bind_order (GElf_Sym *sym)
{
  switch (GELF_ST_BIND (sym->st_info))
    {
    case STB_LOCAL:
      return 1;
    case STB_WEAK:
      return 2;
    case STB_GLOBAL:
      return 3;
    default:
      return 0;
    }
}

static const char *
elf_section_name (Elf *elf, GElf_Word shndx)
{
  GElf_Ehdr ehdr;
  GElf_Shdr shdr;
  Elf_Scn *scn = elf_getscn (elf, shndx);
  gelf_getshdr (scn, &shdr);
  gelf_getehdr (elf, &ehdr);
  return elf_strptr (elf, ehdr.e_shstrndx, shdr.sh_name);
}

bool
addr_in_section (Elf *elf, GElf_Word shndx, GElf_Addr addr)
{
  GElf_Shdr shdr;
  Elf_Scn *scn = elf_getscn (elf, shndx);
  gelf_getshdr (scn, &shdr);
  return addr >= shdr.sh_addr && addr < shdr.sh_addr + shdr.sh_size;
}

static int
list_syms (struct Dwfl_Module *mod,
	   void **user __attribute__ ((unused)), const char *mod_name,
	   Dwarf_Addr low_addr __attribute__ ((unused)),
	   void *arg __attribute__ ((unused)))
{
  int syms = dwfl_module_getsymtab (mod);
  if (syms < 0)
    {
      printf ("%s: %s\n", mod_name, dwfl_errmsg (-1));
      return DWARF_CB_OK;
    }

  for (int ndx = 0; ndx < syms; ndx++)
    {
      GElf_Sym sym;
      GElf_Word shndxp;
      Elf *elf;
      Dwarf_Addr bias;
      const char *name = dwfl_module_getsym (mod, ndx, &sym, &shndxp);

      printf("%4d: %s\t%s\t%s (%" PRIu64 ") %#" PRIx64,
	     ndx, gelf_type (&sym), gelf_bind (&sym), name,
	     sym.st_size, sym.st_value);

      /* The info variant doesn't adjust st_value but returns the (possible)
	 adjusted value separately. */
      GElf_Addr value;
      GElf_Sym isym;
      name = dwfl_module_getsym_info (mod, ndx, &isym, &value, &shndxp,
				      &elf, &bias);

      GElf_Ehdr ehdr;
      gelf_getehdr (elf, &ehdr);

      // getsym st_values might or might not be adjusted depending on section.
      // For ET_REL the adjustment is section relative.
      assert (sym.st_value == isym.st_value
	      || sym.st_value == isym.st_value + bias
	      || ehdr.e_type == ET_REL);

      /* And the reverse, which works for function symbols at least.
	 Note this only works because the st.value is adjusted by
	 dwfl_module_getsym ().  */
      if (GELF_ST_TYPE (sym.st_info) == STT_FUNC && shndxp != SHN_UNDEF)
	{
	  /* Make sure the adjusted value really falls in the elf section. */
          assert (addr_in_section (elf, shndxp, sym.st_value - bias));

	  GElf_Addr addr = value;
	  GElf_Sym asym;
	  GElf_Word ashndxp;
	  Elf *aelf;
	  Dwarf_Addr abias;
	  GElf_Off off;
	  const char *aname = dwfl_module_addrinfo (mod, addr, &off, &asym,
						    &ashndxp, &aelf, &abias);

	  /* Make sure the adjusted value really falls in the elf section. */
          assert (addr_in_section (aelf, ashndxp, asym.st_value)
		  || ehdr.e_type == ET_REL);

	  /* Either they are the same symbol (name), the binding of
	     asym is "stronger" (or equal) to sym or asym is more specific
	     (has a lower address) than sym.  */
	  assert ((strcmp (name, aname) == 0
		   || gelf_bind_order (&asym) >= gelf_bind_order (&sym))
		  && value <= sym.st_value);

	  addr = sym.st_value;
	  int res = dwfl_module_relocate_address (mod, &addr);
	  assert (res != -1);
	  if (shndxp < SHN_LORESERVE)
	    printf(", rel: %#" PRIx64 " (%s)", addr,
		   elf_section_name (elf, shndxp));
	  else
	    printf(", rel: %#" PRIx64 "", addr);

	  /* Print the section of the actual value if different from sym.  */
	  if (value != isym.st_value + bias && ehdr.e_type != ET_REL)
	    {
	      GElf_Addr ebias;
	      addr = value;
	      Elf_Scn *scn = dwfl_module_address_section (mod, &addr, &ebias);
	      GElf_Shdr shdr_mem;
	      GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);
	      Elf *melf = dwfl_module_getelf (mod, &ebias);
	      gelf_getehdr (melf, &ehdr);
	      const char *sname = elf_strptr (melf, ehdr.e_shstrndx,
					      shdr->sh_name);
	      printf (" [%#" PRIx64 ", rel: %#" PRIx64 " (%s)]",
		      value, addr, sname);
	    }

	}
      printf ("\n");
    }

  return DWARF_CB_OK;
}

int
main (int argc, char *argv[])
{
  int remaining;
  Dwfl *dwfl;
  error_t res;

  res = argp_parse (dwfl_standard_argp (), argc, argv, 0, &remaining, &dwfl);
  assert (res == 0 && dwfl != NULL);

  ptrdiff_t off = 0;
  do
    off = dwfl_getmodules (dwfl, list_syms, NULL, off);
  while (off > 0);

  dwfl_end (dwfl);

  return off;
}
