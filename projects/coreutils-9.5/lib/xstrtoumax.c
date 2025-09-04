/* A more useful interface to strtoumax.

   Copyright (C) 1999-2024 Free Software Foundation, Inc.

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

#define __strtol strtoumax
#define __strtol_t uintmax_t
#define __xstrtol xstrtoumax
#define STRTOL_T_MINIMUM 0
#define STRTOL_T_MAXIMUM UINTMAX_MAX
#define XSTRTOL_INCLUDE_INTTYPES_H 1
#include "xstrtol.c"
