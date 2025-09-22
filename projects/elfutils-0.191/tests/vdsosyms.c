/* Test program for getting symbol table from vdso module.
   Copyright (C) 2014 Red Hat, Inc.
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
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include ELFUTILS_HEADER(dwfl)
#include "system.h"

#ifndef __linux__
int
main (int argc __attribute__ ((unused)), char **argv __attribute__ ((unused)))
{
  printf ("Getting the vdso is unsupported.\n");
  return 77;
}
#else /* __linux__ */
static int vdso_syms = 0;

static int
module_callback (Dwfl_Module *mod, void **userdata __attribute__((unused)),
		 const char *name, Dwarf_Addr start __attribute__((unused)),
		 void *arg __attribute__((unused)))
{
  /* We can only recognize the vdso by inspecting the "magic name".  */
  printf ("module name: %s\n", name);
  if (startswith (name, "[vdso: "))
    {
      vdso_syms = dwfl_module_getsymtab (mod);
      printf ("vdso syms: %d\n", vdso_syms);
      if (vdso_syms < 0)
	error (2, 0, "dwfl_module_getsymtab: %s", dwfl_errmsg (-1));

      for (int i = 0; i < vdso_syms; i++)
	{
	  GElf_Sym sym;
	  GElf_Addr addr;
	  const char *sname = dwfl_module_getsym_info (mod, i, &sym, &addr,
						       NULL, NULL, NULL);
	  assert (sname != NULL);
	  printf ("%d: '%s' %" PRIx64 " (%" PRIx64 ")\n",
		  i, sname, sym.st_value, addr);
	}
    }

  return DWARF_CB_OK;
}

int
main (int argc __attribute__ ((unused)), char **argv __attribute__ ((unused)))
{
  static char *debuginfo_path;
  static const Dwfl_Callbacks proc_callbacks =
    {
      .find_debuginfo = dwfl_standard_find_debuginfo,
      .debuginfo_path = &debuginfo_path,

      .find_elf = dwfl_linux_proc_find_elf,
    };
  Dwfl *dwfl = dwfl_begin (&proc_callbacks);
  if (dwfl == NULL)
    error (2, 0, "dwfl_begin: %s", dwfl_errmsg (-1));

  /* Take ourself as "arbitrary" process to inspect.  This should work
     even with "restricted ptrace".  */
  pid_t pid = getpid();

  int result = dwfl_linux_proc_report (dwfl, pid);
  if (result < 0)
    error (2, 0, "dwfl_linux_proc_report: %s", dwfl_errmsg (-1));
  else if (result > 0)
    error (2, result, "dwfl_linux_proc_report");

  /* Also explicitly attach for older kernels (cannot read vdso otherwise).  */
  result = dwfl_linux_proc_attach (dwfl, pid, false);
  if (result < 0)
    error (2, 0, "dwfl_linux_proc_attach: %s", dwfl_errmsg (-1));
  else if (result > 0)
    error (2, result, "dwfl_linux_proc_attach");

  if (dwfl_report_end (dwfl, NULL, NULL) != 0)
    error (2, 0, "dwfl_report_end: %s", dwfl_errmsg (-1));

  if (dwfl_getmodules (dwfl, module_callback, NULL, 0) != 0)
    error (1, 0, "dwfl_getmodules: %s", dwfl_errmsg (-1));

  dwfl_end (dwfl);

  /* No symbols is ok, then we haven't seen the vdso at all on this arch.  */
  return vdso_syms >= 0 ? 0 : -1;
}

#endif /* ! __linux__ */
