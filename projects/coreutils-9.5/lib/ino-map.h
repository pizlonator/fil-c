/* Maintain a mapping of ino_t numbers to small integers.
   Copyright (C) 2010-2024 Free Software Foundation, Inc.

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

/* Written by Paul Eggert, 2010.  */

#ifndef _GL_INO_MAP_H
# define _GL_INO_MAP_H

/* This file uses _GL_ATTRIBUTE_DEALLOC, _GL_ATTRIBUTE_MALLOC,
   _GL_ATTRIBUTE_NONNULL.  */
# if !_GL_CONFIG_H_INCLUDED
#  error "Please include config.h first."
# endif

# include <sys/types.h>

# define INO_MAP_INSERT_FAILURE ((size_t) -1)

struct ino_map;

void ino_map_free (struct ino_map *) _GL_ATTRIBUTE_NONNULL ((1));

struct ino_map *ino_map_alloc (size_t)
  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC (ino_map_free, 1);

size_t ino_map_insert (struct ino_map *, ino_t) _GL_ATTRIBUTE_NONNULL ((1));

#endif
