/* Open a file, reusing a given stream, with error checking.
   Copyright (C) 2009-2023 Free Software Foundation, Inc.

   This file is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published
   by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   This file is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opens the file FILENAME with mode MODE, reusing the given stream FP.
   Upon failure, emits an error message and exits the program.  */
void xfreopen (char const *filename, char const *mode, FILE *fp);

#ifdef __cplusplus
}
#endif
