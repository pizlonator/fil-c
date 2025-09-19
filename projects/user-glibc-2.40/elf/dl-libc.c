/* Handle loading and unloading shared objects for internal libc purposes.
   Copyright (C) 1999-2024 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

#include <dlfcn.h>
#include <stdlib.h>
#include <pizlonated_syscalls.h>

/* FIXME: This sets the dlerror even though in yolo-glibc, these functions throw away the error.
 
   I'm not sure that matters? */

void *
__libc_dlopen_mode (const char *name, int mode)
{
  return zsys_dlopen (name, mode);
}

void *
__libc_dlsym (void *map, const char *name)
{
  return zsys_dlsym (map, name);
}

/* Replacement for dlvsym.  MAP must be a real map.  This function
   returns NULL without setting the dlerror value in case of static
   dlopen from an old binary.  */
void *
__libc_dlvsym (void *map, const char *name, const char *version)
{
  return zsys_dlvsym (map, name, version);
}

int
__libc_dlclose (void *map)
{
  return 0;
}


