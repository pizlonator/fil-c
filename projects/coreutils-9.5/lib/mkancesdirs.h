/* Ensure the existence of the ancestor directories of a file.
   Copyright (C) 2006-2024 Free Software Foundation, Inc.

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

/* Written by Paul Eggert, 2006.  */

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct savewd;
ptrdiff_t mkancesdirs (char *, struct savewd *,
                       int (*) (char const *, char const *, void *), void *);

#ifdef __cplusplus
}
#endif
