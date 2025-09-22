/* Test program for DWARF (.debug_frame) CFI handling.
   Copyright (C) 2009-2010, 2013, 2015, 2018 Red Hat, Inc.
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
#include ELFUTILS_HEADER(dw)
#include <dwarf.h>
#include <argp.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>

#include "system.h"
#include "../libdw/known-dwarf.h"

static const char *
op_name (unsigned int code)
{
  static const char *const known[] =
    {
#define DWARF_ONE_KNOWN_DW_OP(NAME, CODE) [CODE] = #NAME,
      DWARF_ALL_KNOWN_DW_OP
#undef DWARF_ONE_KNOWN_DW_OP
    };

  if (likely (code < sizeof (known) / sizeof (known[0])))
    return known[code];

  return NULL;
}

static void
print_detail (int result, const Dwarf_Op *ops, size_t nops)
{
  if (result < 0)
    printf ("indeterminate (%s)\n", dwarf_errmsg (-1));
  else if (nops == 0)
    printf ("%s\n", ops == NULL ? "same_value" : "undefined");
  else
    {
      printf ("%s expression:", result == 0 ? "location" : "value");
      for (size_t i = 0; i < nops; ++i)
	{
	  printf (" %s", op_name(ops[i].atom));
	  if (ops[i].number2 == 0)
	    {
	      if (ops[i].atom == DW_OP_addr)
		printf ("(%#" PRIx64 ")", ops[i].number);
	      else if (ops[i].number != 0)
		printf ("(%" PRId64 ")", ops[i].number);
	    }
	  else
	    printf ("(%" PRId64 ",%" PRId64 ")",
		    ops[i].number, ops[i].number2);
	}
      puts ("");
    }
}

static int
handle_address (Dwarf_CFI *cfi, GElf_Addr pc)
{
  Dwarf_Frame *frame;
  int result = dwarf_cfi_addrframe (cfi, pc, &frame);
  if (result != 0)
    {
      printf ("dwarf_cfi_addrframe: %s\n", dwarf_errmsg (-1));
      return 1;
    }

  Dwarf_Addr start = pc;
  Dwarf_Addr end = pc;
  bool signalp;
  int ra_regno = dwarf_frame_info (frame, &start, &end, &signalp);

  printf ("%#" PRIx64 " => [%#" PRIx64 ", %#" PRIx64 "):\n",
	  pc, start, end);

  if (ra_regno < 0)
    printf ("\treturn address register unavailable (%s)\n",
	    dwarf_errmsg (-1));
  else
    printf ("\treturn address in reg%u%s\n",
	    ra_regno, signalp ? " (signal frame)" : "");

  // Point cfa_ops to dummy to match print_detail expectations.
  // (nops == 0 && cfa_ops != NULL => "undefined")
  Dwarf_Op dummy;
  Dwarf_Op *cfa_ops = &dummy;
  size_t cfa_nops;
  result = dwarf_frame_cfa (frame, &cfa_ops, &cfa_nops);

  printf ("\tCFA ");
  print_detail (result, cfa_ops, cfa_nops);

  // Print the location of the first 10 (DWARF nr) registers
  for (int r = 0; r < 10; r++)
    {
      Dwarf_Op ops_mem[3];
      Dwarf_Op *ops;
      size_t nops;
      printf ("\treg%d: ", r);
      int reg_result = dwarf_frame_register (frame, r, ops_mem, &ops, &nops);
      print_detail (reg_result, ops, nops);
      result |= reg_result;
    }

  free (frame);
  return result;
}

int
main (int argc, char *argv[])
{
  if (argc <= 2)
    error (EXIT_FAILURE, 0, "need file name argument and addresses");

  int fd = open (argv[1], O_RDONLY);
  if (fd == -1)
    error (EXIT_FAILURE, errno, "cannot open input file `%s'", argv[1]);

  elf_version (EV_CURRENT);

  Elf *elf = elf_begin (fd, ELF_C_READ, NULL);
  if (elf == NULL)
    error (EXIT_FAILURE, 0, "cannot create ELF descriptor: %s",
	   elf_errmsg (-1));

  Dwarf *dwarf = dwarf_begin_elf (elf, DWARF_C_READ, NULL);
  if (dwarf == NULL)
    error (EXIT_FAILURE, 0, "cannot create DWARF descriptor: %s",
	   dwarf_errmsg (-1));

  Dwarf_CFI *cfi = dwarf_getcfi (dwarf);
  if (cfi == NULL)
    error (EXIT_FAILURE, 0, "cannot get DWARF CFI from .dwarf_frame: %s",
	   dwarf_errmsg (-1));

  int result = 0;
  int args = 2;
  do
    {
      char *endp;
      uintmax_t addr = strtoumax (argv[args], &endp, 0);
      if (endp != argv[args])
	result |= handle_address (cfi, addr);
      else
	result = 1;
    }
  while (args++ < argc - 1);

  dwarf_end (dwarf);
  elf_end (elf);

  return result;
}
