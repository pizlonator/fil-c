/* A posix_memalign() function that works around platform bugs.
   Copyright (C) 2020-2024 Free Software Foundation, Inc.

   This file is free software: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation; either version 2.1 of the
   License, or (at your option) any later version.

   This file is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

#include <config.h>

/* Specification.  */
#include <stdlib.h>

#include <errno.h>

int
posix_memalign (void **memptr, size_t alignment, size_t size)
#undef posix_memalign
{
  /* Round up SIZE to the next multiple of ALIGNMENT, namely
     (SIZE + ALIGNMENT - 1) & ~(ALIGNMENT - 1).  */
  size += alignment - 1;
  if (size >= alignment - 1) /* no overflow? */
    return posix_memalign (memptr, alignment, size & ~(size_t)(alignment - 1));
  else
    return ENOMEM;
}
