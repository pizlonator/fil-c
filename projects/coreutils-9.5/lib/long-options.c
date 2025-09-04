/* Utility to accept --help and --version options as unobtrusively as possible.

   Copyright (C) 1993-1994, 1998-2000, 2002-2006, 2009-2024 Free Software
   Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* Written by Jim Meyering.  */

#include <config.h>

/* Specification.  */
#include "long-options.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

#include "version-etc.h"
#include "exitfail.h"

static struct option const long_options[] =
{
  {"help", no_argument, NULL, 'h'},
  {"version", no_argument, NULL, 'v'},
  {NULL, 0, NULL, 0}
};

/* Process long options --help and --version, but only if argc == 2.
   Be careful not to gobble up "--".  */

void
parse_long_options (int argc,
                    char **argv,
                    const char *command_name,
                    const char *package,
                    const char *version,
                    void (*usage_func) (int),
                    /* const char *author1, ...*/ ...)
{
  int c;
  int saved_opterr;

  saved_opterr = opterr;

  /* Don't print an error message for unrecognized options.  */
  opterr = 0;

  if (argc == 2
      && (c = getopt_long (argc, argv, "+", long_options, NULL)) != -1)
    {
      switch (c)
        {
        case 'h':
          (*usage_func) (EXIT_SUCCESS);
          break;

        case 'v':
          {
            va_list authors;
            va_start (authors, usage_func);
            version_etc_va (stdout, command_name, package, version, authors);
            exit (EXIT_SUCCESS);
          }

        default:
          /* Don't process any other long-named options.  */
          break;
        }
    }

  /* Restore previous value.  */
  opterr = saved_opterr;

  /* Reset this to zero so that getopt internals get initialized from
     the probably-new parameters when/if getopt is called later.  */
  optind = 0;
}

/* Process the GNU default long options --help and --version (see also
   https://gnu.org/prep/standards/html_node/Command_002dLine-Interfaces.html),
   and fail for any other unknown long or short option.
   Use with SCAN_ALL=true to scan until "--", or with SCAN_ALL=false to stop
   at the first non-option argument (or "--", whichever comes first).  */
void
parse_gnu_standard_options_only (int argc,
                                 char **argv,
                                 const char *command_name,
                                 const char *package,
                                 const char *version,
                                 bool scan_all,
                                 void (*usage_func) (int),
                                 /* const char *author1, ...*/ ...)
{
  int c;
  int saved_opterr = opterr;

  /* Print an error message for unrecognized options.  */
  opterr = 1;

  const char *optstring = scan_all ? "" : "+";

  if ((c = getopt_long (argc, argv, optstring, long_options, NULL)) != -1)
    {
      switch (c)
        {
        case 'h':
          (*usage_func) (EXIT_SUCCESS);
          break;

        case 'v':
          {
            va_list authors;
            va_start (authors, usage_func);
            version_etc_va (stdout, command_name, package, version, authors);
            exit (EXIT_SUCCESS);
          }

        default:
          (*usage_func) (exit_failure);
          break;
        }
    }

  /* Restore previous value.  */
  opterr = saved_opterr;
}
