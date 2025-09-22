/* Inspect nvidia extended linemap with dwarf_next_lines.
   Copyright (C) 2002, 2004, 2018 Red Hat, Inc.
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

#include <fcntl.h>
#include <inttypes.h>
#include <libelf.h>
#include ELFUTILS_HEADER(dw)
#include <stdio.h>
#include <string.h>
#include <unistd.h>


int
main (int argc, char *argv[])
{
  int result = 0;
  int cnt;

  for (cnt = 1; cnt < argc; ++cnt)
    {
      int fd = open (argv[cnt], O_RDONLY);

      Dwarf *dbg = dwarf_begin (fd, DWARF_C_READ);
      if  (dbg == NULL)
	{
	  printf ("%s not usable: %s\n", argv[cnt], dwarf_errmsg (-1));
	  close  (fd);
	  continue;
	}

      Dwarf_Off off;
      Dwarf_Off next_off = 0;
      Dwarf_CU *cu = NULL;
      Dwarf_Lines *lb;
      size_t nlb;
      int res;
      while ((res = dwarf_next_lines (dbg, off = next_off, &next_off, &cu,
				      NULL, NULL, &lb, &nlb)) == 0)
	{
	  printf ("off = %" PRIu64 "\n", off);
	  printf (" %zu lines\n", nlb);

	  for (size_t i = 0; i < nlb; ++i)
	    {
	      Dwarf_Line *l = dwarf_onesrcline (lb, i);
	      if (l == NULL)
		{
		  printf ("%s: cannot get individual line\n", argv[cnt]);
		  result = 1;
		  break;
		}

	      Dwarf_Addr addr;
	      if (dwarf_lineaddr (l, &addr) != 0)
		addr = 0;
	      const char *file = dwarf_linesrc (l, NULL, NULL);
	      int line;
	      if (dwarf_lineno (l, &line) != 0)
		line = 0;

	      printf ("%" PRIx64 ": %s:%d:", (uint64_t) addr,
		      file ?: "???", line);

	      /* Getting the file path through the Dwarf_Files should
		 result in the same path.  */
	      Dwarf_Files *files;
	      size_t idx;
	      if (dwarf_line_file (l, &files, &idx) != 0)
		{
		  printf ("%s: cannot get file from line (%zd): %s\n",
			  argv[cnt], i, dwarf_errmsg (-1));
		  result = 1;
		  break;
		}
	      const char *path = dwarf_filesrc (files, idx, NULL, NULL);
	      if ((path == NULL && file != NULL)
		  || (path != NULL && file == NULL)
		  || (strcmp (file, path) != 0))
		{
		  printf ("%s: line %zd srcline (%s) != file srcline (%s)\n",
			  argv[cnt], i, file ?: "???", path ?: "???");
		  result = 1;
		  break;
		}

	      int column;
	      if (dwarf_linecol (l, &column) != 0)
		column = 0;
	      if (column >= 0)
		printf ("%d:", column);

	      bool is_stmt;
	      if (dwarf_linebeginstatement (l, &is_stmt) != 0)
		is_stmt = false;
	      bool end_sequence;
	      if (dwarf_lineendsequence (l, &end_sequence) != 0)
		end_sequence = false;
	      bool basic_block;
	      if (dwarf_lineblock (l, &basic_block) != 0)
		basic_block = false;
	      bool prologue_end;
	      if (dwarf_lineprologueend (l, &prologue_end) != 0)
		prologue_end = false;
	      bool epilogue_begin;
	      if (dwarf_lineepiloguebegin (l, &epilogue_begin) != 0)
		epilogue_begin = false;
	      printf (" is_stmt:%s, end_seq:%s, bb:%s, prologue:%s, epilogue:%s\n",
		      is_stmt ? "yes" : "no", end_sequence ? "yes" : "no",
		      basic_block ? "yes" : "no", prologue_end  ? "yes" : "no",
		      epilogue_begin ? "yes" : "no");

	      Dwarf_Line* callee_context = l;
	      Dwarf_Line* caller_context = dwarf_linecontext (lb, l);
	      unsigned int depth = 0;
	      while (caller_context != NULL)
		{
		  depth++;
		  for (unsigned int x = 0; x < depth; x++)
		    printf ("  ");

		  const char *inlined_file = dwarf_linesrc (caller_context,
							    NULL, NULL);
		  int inlined_line;
		  if (dwarf_lineno (caller_context, &inlined_line) != 0)
		    inlined_line = 0;

		  printf ("%s inlined at %s:%d\n",
			  dwarf_linefunctionname(dbg, callee_context),
			  inlined_file ?: "???", inlined_line);

		  callee_context = caller_context;
		  caller_context = dwarf_linecontext (lb, callee_context);
	        }
	    }
	}

      if (res < 0)
	{
	  printf ("dwarf_next_lines failed: %s\n", dwarf_errmsg (-1));
	  result = 1;
	}

      dwarf_end (dbg);
      close (fd);
    }

  return result;
}
