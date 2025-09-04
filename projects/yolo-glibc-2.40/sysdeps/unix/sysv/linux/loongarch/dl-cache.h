/* Support for reading /etc/ld.so.cache files written by Linux ldconfig.
   Copyright (C) 2022-2024 Free Software Foundation, Inc.
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
   License along with the GNU C Library.  If not, see
   <https://www.gnu.org/licenses/>.  */

#include <ldconfig.h>
#include <assert.h>

#if defined __loongarch_double_float
# define _DL_CACHE_DEFAULT_ID    (FLAG_LARCH_FLOAT_ABI_DOUBLE | FLAG_ELF_LIBC6)
#else
# define _DL_CACHE_DEFAULT_ID    (FLAG_LARCH_FLOAT_ABI_SOFT | FLAG_ELF_LIBC6)
#endif

#define _dl_cache_check_flags(flags)                    \
  ((flags) == _DL_CACHE_DEFAULT_ID)

/* If given a path to one of our library directories, adds every library
   directory via add_dir (), otherwise just adds the giver directory.  On
   LoongArch, libraries can be found in paths ending in:
     - /lib64
     - /lib64/sf
   so this will add all of those paths.  */

#define add_system_dir(dir) 						\
  do							    		\
    {									\
      static const char* lib_dirs[] = {					\
	"/lib64",							\
	"/lib64/sf",							\
	NULL,								\
      };								\
      const size_t lib_len = sizeof ("/lib") - 1;			\
      size_t len = strlen (dir);					\
      char path[len + 6];						\
      const char **ptr;							\
									\
      memcpy (path, dir, len + 1);					\
									\
      for (ptr = lib_dirs; *ptr != NULL; ptr++)				\
	{								\
	  const char *lib_dir = *ptr;					\
	  size_t dir_len = strlen (lib_dir);				\
									\
	  if (len >= dir_len						\
	      && !memcmp (path + len - dir_len, lib_dir, dir_len))	\
	    {								\
	      len -= dir_len - lib_len;					\
	      path[len] = '\0';						\
	      break;							\
	    }								\
	}								\
      add_dir (path);							\
      if (len >= lib_len						\
	  && !memcmp (path + len - lib_len, "/lib", lib_len))		\
	for (ptr = lib_dirs; *ptr != NULL; ptr++)			\
	  {								\
	    const char *lib_dir = *ptr;					\
	    size_t dir_len = strlen (lib_dir);				\
									\
	    assert (dir_len >= lib_len);				\
	    memcpy (path + len, lib_dir + lib_len,			\
		    dir_len - lib_len + 1);				\
	    add_dir (path);						\
	  }								\
    } while (0)


#include_next <dl-cache.h>
