/* Test program for libdwfl basic module tracking, relocation.
   Copyright (C) 2005, 2007 Red Hat, Inc.
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
#include <sys/types.h>
#include <stdio.h>
#include <stdio_ext.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <argp.h>
#include ELFUTILS_HEADER(dwfl)
#include <dwarf.h>
#include "system.h"

static bool show_inlines;

struct info
{
  Dwarf_Die *cudie;
  Dwarf_Addr dwbias;
};

static int
print_instance (Dwarf_Die *instance, void *arg)
{
  const struct info *info = arg;

  printf ("    inlined");

  Dwarf_Files *files;
  if (dwarf_getsrcfiles (info->cudie, &files, NULL) == 0)
    {
      Dwarf_Attribute attr_mem;
      Dwarf_Word val;
      if (dwarf_formudata (dwarf_attr (instance, DW_AT_call_file,
				       &attr_mem), &val) == 0)
	{
	  const char *file = dwarf_filesrc (files, val, NULL, NULL);
	  int lineno = 0, colno = 0;
	  if (dwarf_formudata (dwarf_attr (instance, DW_AT_call_line,
					   &attr_mem), &val) == 0)
	    lineno = val;
	  if (dwarf_formudata (dwarf_attr (instance, DW_AT_call_column,
					   &attr_mem), &val) == 0)
	    colno = val;
	  if (lineno == 0)
	    {
	      if (file != NULL)
		printf (" from %s", file);
	    }
	  else if (colno == 0)
	    printf (" at %s:%u", file, lineno);
	  else
	    printf (" at %s:%u:%u", file, lineno, colno);
	}
    }

  Dwarf_Addr lo = -1, hi = -1, entry = -1;
  if (dwarf_lowpc (instance, &lo) == 0)
    lo += info->dwbias;
  else
    printf (" (lowpc => %s)", dwarf_errmsg (-1));
  if (dwarf_highpc (instance, &hi) == 0)
    hi += info->dwbias;
  else
    printf (" (highpc => %s)", dwarf_errmsg (-1));

  Dwarf_Attribute attr_mem;
  Dwarf_Attribute *attr = dwarf_attr (instance, DW_AT_entry_pc, &attr_mem);
  if (attr != NULL)
    {
      if (dwarf_formaddr (attr, &entry) == 0)
	entry += info->dwbias;
      else
	printf (" (entrypc => %s)", dwarf_errmsg (-1));
    }

  if (lo != (Dwarf_Addr) -1 || hi != (Dwarf_Addr) -1)
    printf (" %#" PRIx64 "..%#" PRIx64, lo, hi);
  if (entry != (Dwarf_Addr) -1)
    printf (" => %#" PRIx64 "\n", entry);
  else
    puts ("");

  return DWARF_CB_OK;
}

static void
print_inline (Dwarf_Die *func, void *arg)
{
  if (dwarf_func_inline_instances (func, &print_instance, arg) != 0)
    printf ("  error finding instances: %s\n", dwarf_errmsg (-1));
}

static int
print_func (Dwarf_Die *func, void *arg)
{
  const struct info *info = arg;

  const char *file = dwarf_decl_file (func);
  int line = -1;
  dwarf_decl_line (func, &line);
  const char *fct = dwarf_diename (func);

  printf ("  %s:%d: %s:", file, line, fct);

  if (dwarf_func_inline (func))
    {
      puts (" inline function");
      if (show_inlines)
	print_inline (func, arg);
    }
  else
    {
      Dwarf_Addr lo = -1, hi = -1, entry = -1;
      if (dwarf_lowpc (func, &lo) == 0)
	lo += info->dwbias;
      else
	printf (" (lowpc => %s)", dwarf_errmsg (-1));
      if (dwarf_highpc (func, &hi) == 0)
	hi += info->dwbias;
      else
	printf (" (highpc => %s)", dwarf_errmsg (-1));
      if (dwarf_entrypc (func, &entry) == 0)
	entry += info->dwbias;
      else
	printf (" (entrypc => %s)", dwarf_errmsg (-1));

      if (lo != (Dwarf_Addr) -1 || hi != (Dwarf_Addr) -1
	  || entry != (Dwarf_Addr) -1)
	printf (" %#" PRIx64 "..%#" PRIx64 " => %#" PRIx64 "\n",
		lo, hi, entry);
      else
	puts ("");
    }

  return DWARF_CB_OK;
}

static int
list_module (Dwfl_Module *mod __attribute__ ((unused)),
	     void **userdata __attribute__ ((unused)),
	     const char *name, Dwarf_Addr base,
	     void *arg __attribute__ ((unused)))
{
  Dwarf_Addr start;
  Dwarf_Addr end;
  const char *file;
  const char *debug;
  if (dwfl_module_info (mod, NULL, &start, &end,
			NULL, NULL, &file, &debug) != name
      || start != base)
    abort ();
  printf ("module: %30s %08" PRIx64 "..%08" PRIx64 " %s %s\n",
	  name, start, end, file, debug);
  return DWARF_CB_OK;
}

static int
print_module (Dwfl_Module *mod __attribute__ ((unused)),
	      void **userdata __attribute__ ((unused)),
	      const char *name, Dwarf_Addr base,
	      Dwarf *dw, Dwarf_Addr bias,
	      void *arg)
{
  printf ("module: %30s %08" PRIx64 " %s %" PRIx64 " (%s)\n",
	  name, base, dw == NULL ? "no" : "DWARF", bias, dwfl_errmsg (-1));

  if (dw != NULL && *(const bool *) arg)
    {
      Dwarf_Off off = 0;
      size_t cuhl;
      Dwarf_Off noff;

      while (dwarf_nextcu (dw, off, &noff, &cuhl, NULL, NULL, NULL) == 0)
	{
	  Dwarf_Die die_mem;
	  struct info info = { dwarf_offdie (dw, off + cuhl, &die_mem), bias };
	  (void) dwarf_getfuncs (info.cudie, print_func, &info, 0);

	  off = noff;
	}
    }

  return DWARF_CB_OK;
}

static bool show_functions;

static const struct argp_option options[] =
  {
    { "functions", 'f', NULL, 0, N_("Additionally show function names"), 0 },
    { "inlines", 'i', NULL, 0, N_("Show instances of inlined functions"), 0 },
    { NULL, 0, NULL, 0, NULL, 0 }
  };

static error_t
parse_opt (int key, char *arg __attribute__ ((unused)),
	   struct argp_state *state __attribute__ ((unused)))
{
  switch (key)
    {
    case ARGP_KEY_INIT:
      state->child_inputs[0] = state->input;
      break;

    case 'f':
      show_functions = true;
      break;

    case 'i':
      show_inlines = show_functions = true;
      break;

    default:
      return ARGP_ERR_UNKNOWN;
    }
  return 0;
}

int
main (int argc, char **argv)
{
  /* We use no threads here which can interfere with handling a stream.  */
  (void) __fsetlocking (stdout, FSETLOCKING_BYCALLER);

  /* Set locale.  */
  (void) setlocale (LC_ALL, "");

  Dwfl *dwfl = NULL;
  const struct argp_child argp_children[] =
    {
      { .argp = dwfl_standard_argp () },
      { .argp = NULL }
    };
  const struct argp argp =
    {
      options, parse_opt, NULL, NULL, argp_children, NULL, NULL
    };
  (void) argp_parse (&argp, argc, argv, 0, NULL, &dwfl);
  assert (dwfl != NULL);

  ptrdiff_t p = 0;
  do
    p = dwfl_getmodules (dwfl, &list_module, NULL, p);
  while (p > 0);
  if (p < 0)
    error (2, 0, "dwfl_getmodules: %s", dwfl_errmsg (-1));

  do
    p = dwfl_getdwarf (dwfl, &print_module, &show_functions, p);
  while (p > 0);
  if (p < 0)
    error (2, 0, "dwfl_getdwarf: %s", dwfl_errmsg (-1));

  p = 0;
  do
    p = dwfl_getmodules (dwfl, &list_module, NULL, p);
  while (p > 0);
  if (p < 0)
    error (2, 0, "dwfl_getmodules: %s", dwfl_errmsg (-1));

  dwfl_end (dwfl);

  return 0;
}
