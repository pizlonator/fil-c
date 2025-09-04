/* long-options.h -- declaration for --help- and --version-handling function.
   Copyright (C) 1993-1994, 1998-1999, 2003, 2009-2024 Free Software
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

#ifndef LONG_OPTIONS_H_
# define LONG_OPTIONS_H_ 1

void parse_long_options (int _argc,
                         char **_argv,
                         const char *_command_name,
                         const char *_package,
                         const char *_version,
                         void (*_usage) (int),
                         /* const char *author1, ...*/ ...);

void parse_gnu_standard_options_only (int argc,
                                      char **argv,
                                      const char *command_name,
                                      const char *package,
                                      const char *version,
                                      bool scan_all,
                                      void (*usage_func) (int),
                                      /* const char *author1, ...*/ ...);

#endif /* LONG_OPTIONS_H_ */
