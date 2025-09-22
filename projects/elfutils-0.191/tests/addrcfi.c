/* Test program for CFI handling.
   Copyright (C) 2009-2010, 2013, 2015 Red Hat, Inc.
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
#include <dwarf.h>
#include <argp.h>
#include <stdio.h>
#include <stdio_ext.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>

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
print_detail (int result, const Dwarf_Op *ops, size_t nops, Dwarf_Addr bias)
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
		printf ("(%#" PRIx64 ")", ops[i].number + bias);
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

struct stuff
{
  Dwarf_Frame *frame;
  Dwarf_Addr bias;
};

static int
print_register (void *arg,
		int regno,
		const char *setname,
		const char *prefix,
		const char *regname,
		int bits __attribute__ ((unused)),
		int type __attribute__ ((unused)))
{
  struct stuff *stuff = arg;

  printf ("\t%s reg%u (%s%s): ", setname, regno, prefix, regname);

  Dwarf_Op ops_mem[3];
  Dwarf_Op *ops;
  size_t nops;
  int result = dwarf_frame_register (stuff->frame, regno, ops_mem, &ops, &nops);
  print_detail (result, ops, nops, stuff->bias);

  return DWARF_CB_OK;
}

static int
handle_cfi (Dwfl *dwfl, const char *which, Dwarf_CFI *cfi,
	    GElf_Addr pc, struct stuff *stuff)
{
  if (cfi == NULL)
    {
      printf ("handle_cfi no CFI (%s): %s\n", which, dwarf_errmsg (-1));
      return -1;
    }

  int result = dwarf_cfi_addrframe (cfi, pc - stuff->bias, &stuff->frame);
  if (result != 0)
    {
      printf ("dwarf_cfi_addrframe (%s): %s\n", which, dwarf_errmsg (-1));
      return 1;
    }

  Dwarf_Addr start = pc;
  Dwarf_Addr end = pc;
  bool signalp;
  int ra_regno = dwarf_frame_info (stuff->frame, &start, &end, &signalp);
  if (ra_regno >= 0)
    {
      start += stuff->bias;
      end += stuff->bias;
    }

  printf ("%s has %#" PRIx64 " => [%#" PRIx64 ", %#" PRIx64 "):\n",
	  which, pc, start, end);

  if (ra_regno < 0)
    printf ("\treturn address register unavailable (%s)\n",
	    dwarf_errmsg (0));
  else
    printf ("\treturn address in reg%u%s\n",
	    ra_regno, signalp ? " (signal frame)" : "");

  // Point cfa_ops to dummy to match print_detail expectations.
  // (nops == 0 && cfa_ops != NULL => "undefined")
  Dwarf_Op dummy;
  Dwarf_Op *cfa_ops = &dummy;
  size_t cfa_nops;
  result = dwarf_frame_cfa (stuff->frame, &cfa_ops, &cfa_nops);

  printf ("\tCFA ");
  print_detail (result, cfa_ops, cfa_nops, stuff->bias);

  (void) dwfl_module_register_names (dwfl_addrmodule (dwfl, pc),
				     &print_register, stuff);

  return 0;
}

static int
handle_address (GElf_Addr pc, Dwfl *dwfl)
{
  Dwfl_Module *mod = dwfl_addrmodule (dwfl, pc);

  struct stuff stuff;
  stuff.frame = NULL;
  stuff.bias = 0;
  int res = handle_cfi (dwfl, ".eh_frame",
			dwfl_module_eh_cfi (mod, &stuff.bias), pc, &stuff);
  free (stuff.frame);

  stuff.frame = NULL;
  stuff.bias = 0;
  res &= handle_cfi (dwfl, ".debug_frame",
		     dwfl_module_dwarf_cfi (mod, &stuff.bias), pc, &stuff);
  free (stuff.frame);

  return res;
}

int
main (int argc, char *argv[])
{
  int remaining;

  /* Set locale.  */
  (void) setlocale (LC_ALL, "");

  Dwfl *dwfl = NULL;
  (void) argp_parse (dwfl_standard_argp (), argc, argv, 0, &remaining, &dwfl);
  assert (dwfl != NULL);

  int result = 0;

  /* Now handle the addresses.  In case none are given on the command
     line, read from stdin.  */
  if (remaining == argc)
    {
      /* We use no threads here which can interfere with handling a stream.  */
      (void) __fsetlocking (stdin, FSETLOCKING_BYCALLER);

      char *buf = NULL;
      size_t len = 0;
      while (!feof_unlocked (stdin))
	{
	  if (getline (&buf, &len, stdin) < 0)
	    break;

	  char *endp;
	  uintmax_t addr = strtoumax (buf, &endp, 0);
	  if (endp != buf)
	    result |= handle_address (addr, dwfl);
	  else
	    result = 1;
	}

      free (buf);
    }
  else
    {
      do
	{
	  char *endp;
	  uintmax_t addr = strtoumax (argv[remaining], &endp, 0);
	  if (endp != argv[remaining])
	    result |= handle_address (addr, dwfl);
	  else
	    result = 1;
	}
      while (++remaining < argc);
    }

  dwfl_end (dwfl);

  return result;
}
