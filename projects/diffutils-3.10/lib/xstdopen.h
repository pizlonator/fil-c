/* Ensure that stdin, stdout, stderr are open.
   Copyright (C) 2019-2023 Free Software Foundation, Inc.

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

#ifdef __cplusplus
extern "C" {
#endif

/* Ensures that the file descriptors of stdin, stdout, stderr (0, 1, 2) are
   open.  Exits the program with an error message upon failure; the error
   message may not appear if stderr is closed.  */
extern void xstdopen (void);

#ifdef __cplusplus
}
#endif
