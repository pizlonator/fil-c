/* modechange.h -- definitions for file mode manipulation

   Copyright (C) 1989-1990, 1997, 2003-2006, 2009-2024 Free Software
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

#if ! defined MODECHANGE_H_
# define MODECHANGE_H_

/* This file uses _GL_ATTRIBUTE_MALLOC.  */
# if !_GL_CONFIG_H_INCLUDED
#  error "Please include config.h first."
# endif

# include <stdlib.h>
# include <sys/types.h>

struct mode_change *mode_compile (const char *)
  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC_FREE;
struct mode_change *mode_create_from_ref (const char *)
  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC_FREE;
mode_t mode_adjust (mode_t, bool, mode_t, struct mode_change const *,
                    mode_t *)
  _GL_ATTRIBUTE_PURE;

#endif
