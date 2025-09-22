/* Copyright (C) 2013 Red Hat, Inc.
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <inttypes.h>
#include <assert.h>
#include ELFUTILS_HEADER(dw)
#include ELFUTILS_HEADER(dwfl)
#include <dwarf.h>
#include <argp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "system.h"

int
main (int argc, char *argv[])
{
  int cnt;

  Dwfl *dwfl = NULL;
  (void) argp_parse (dwfl_standard_argp (), argc, argv, 0, &cnt, &dwfl);
  assert (dwfl != NULL);

  Dwarf_Die *cu = NULL;
  Dwarf_Addr bias;
  do
    {
      cu = dwfl_nextcu (dwfl, cu, &bias);
      if (cu != NULL)
	{
	  Dwfl_Module *mod = dwfl_cumodule (cu);
	  const char *modname = (dwfl_module_info (mod, NULL, NULL, NULL,
						   NULL, NULL, NULL, NULL)
				 ?: "<unknown>");
	  const char *cuname = (dwarf_diename (cu) ?: "<unknown>");

	  printf ("mod: %s CU: [%" PRIx64 "] %s\n", modname,
		  dwarf_dieoffset (cu), cuname);

	  size_t lines;
	  if (dwfl_getsrclines (cu, &lines) != 0)
	    continue; // No lines...

	  for (size_t i = 0; i < lines; i++)
	    {
	      Dwfl_Line *line = dwfl_onesrcline (cu, i);

	      Dwarf_Addr addr;
	      int lineno;
	      int colno;
	      Dwarf_Word mtime;
	      Dwarf_Word length;
	      const char *src = dwfl_lineinfo (line, &addr, &lineno, &colno,
					       &mtime, &length);

	      Dwarf_Addr dw_bias;
	      Dwarf_Line *dw_line = dwfl_dwarf_line (line, &dw_bias);
	      assert (bias == dw_bias);

	      Dwarf_Addr dw_addr;
	      if (dwarf_lineaddr (dw_line, &dw_addr) != 0)
		error (EXIT_FAILURE, 0, "dwarf_lineaddr: %s",
		       dwarf_errmsg (-1));
	      assert (addr == dw_addr + dw_bias);

	      unsigned int dw_op_index;
	      if (dwarf_lineop_index (dw_line, &dw_op_index) != 0)
		error (EXIT_FAILURE, 0, "dwarf_lineop_index: %s",
		       dwarf_errmsg (-1));

	      int dw_lineno;
	      if (dwarf_lineno (dw_line, &dw_lineno) != 0)
		error (EXIT_FAILURE, 0, "dwarf_lineno: %s",
		       dwarf_errmsg (-1));
	      assert (lineno == dw_lineno);

	      int dw_colno;
	      if (dwarf_linecol (dw_line, &dw_colno) != 0)
		error (EXIT_FAILURE, 0, "dwarf_lineno: %s",
		       dwarf_errmsg (-1));
	      assert (colno == dw_colno);

	      bool begin;
	      if (dwarf_linebeginstatement (dw_line, &begin) != 0)
		error (EXIT_FAILURE, 0, "dwarf_linebeginstatement: %s",
		       dwarf_errmsg (-1));

	      bool end;
	      if (dwarf_lineendsequence (dw_line, &end) != 0)
		error (EXIT_FAILURE, 0, "dwarf_lineendsequence: %s",
		       dwarf_errmsg (-1));

	      bool pend;
	      if (dwarf_lineprologueend (dw_line, &pend) != 0)
		error (EXIT_FAILURE, 0, "dwarf_lineprologueend: %s",
		       dwarf_errmsg (-1));

	      bool ebegin;
	      if (dwarf_lineepiloguebegin (dw_line, &ebegin) != 0)
		error (EXIT_FAILURE, 0, "dwarf_lineepiloguebegin: %s",
		       dwarf_errmsg (-1));

	      bool block;
	      if (dwarf_lineblock (dw_line, &block) != 0)
		error (EXIT_FAILURE, 0, "dwarf_lineblock: %s",
		       dwarf_errmsg (-1));

	      unsigned int isa;
	      if (dwarf_lineisa (dw_line, &isa) != 0)
		error (EXIT_FAILURE, 0, "dwarf_lineisa: %s",
		       dwarf_errmsg (-1));

	      unsigned int disc;
	      if (dwarf_linediscriminator (dw_line, &disc) != 0)
		error (EXIT_FAILURE, 0, "dwarf_linediscriminator: %s",
		       dwarf_errmsg (-1));

	      const char *dw_src;
	      Dwarf_Word dw_mtime;
	      Dwarf_Word dw_length;
	      dw_src = dwarf_linesrc (dw_line, &dw_mtime, &dw_length);
	      assert (strcmp (src, dw_src) == 0);
	      assert (mtime == dw_mtime);
	      assert (length == dw_length);

	      printf ("%zd %#" PRIx64 " %s:%d:%d\n"
		      " time: %#" PRIX64 ", len: %" PRIu64
		      ", idx: %d, b: %d, e: %d"
		      ", pe: %d, eb: %d, block: %d"
		      ", isa: %d, disc: %d\n",
		      i, addr, src, lineno, colno, mtime, length,
		      dw_op_index, begin, end, pend, ebegin, block, isa, disc);

	      Dwarf_Die *linecu = dwfl_linecu (line);
	      assert (cu == linecu);

	      Dwfl_Module *linemod = dwfl_linemodule (line);
	      assert (mod == linemod);
	    }
	}
    }
  while (cu != NULL);

  dwfl_end (dwfl);

  return 0;
}
