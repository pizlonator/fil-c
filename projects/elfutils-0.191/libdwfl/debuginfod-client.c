/* Try to get an ELF or debug file through the debuginfod.
   Copyright (C) 2019 Red Hat, Inc.
   Copyright (C) 2022 Mark J. Wielaard <mark@klomp.org>
   This file is part of elfutils.

   This file is free software; you can redistribute it and/or modify
   it under the terms of either

     * the GNU Lesser General Public License as published by the Free
       Software Foundation; either version 3 of the License, or (at
       your option) any later version

   or

     * the GNU General Public License as published by the Free
       Software Foundation; either version 2 of the License, or (at
       your option) any later version

   or both in parallel, as here.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received copies of the GNU General Public License and
   the GNU Lesser General Public License along with this program.  If
   not, see <http://www.gnu.org/licenses/>.  */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "libdwflP.h"

#ifdef ENABLE_LIBDEBUGINFOD

#include "debuginfod.h"

#include <pthread.h>
#include <dlfcn.h>

static __typeof__ (debuginfod_begin) *fp_debuginfod_begin;
static __typeof__ (debuginfod_find_executable) *fp_debuginfod_find_executable;
static __typeof__ (debuginfod_find_debuginfo) *fp_debuginfod_find_debuginfo;
static __typeof__ (debuginfod_end) *fp_debuginfod_end;

static void __libdwfl_debuginfod_init (void);

static pthread_once_t init_control = PTHREAD_ONCE_INIT;

/* NB: this is slightly thread-unsafe */

debuginfod_client *
dwfl_get_debuginfod_client (Dwfl *dwfl)
{
  if (dwfl->debuginfod != NULL)
    return dwfl->debuginfod;

  pthread_once (&init_control, __libdwfl_debuginfod_init);

  if (fp_debuginfod_begin != NULL)
    {
      dwfl->debuginfod = (*fp_debuginfod_begin) ();
      return dwfl->debuginfod;
    }

  return NULL;
}
INTDEF(dwfl_get_debuginfod_client)

int
__libdwfl_debuginfod_find_executable (Dwfl *dwfl,
				      const unsigned char *build_id_bits,
				      size_t build_id_len)
{
  int fd = -1;
  if (build_id_len > 0)
    {
      debuginfod_client *c = INTUSE (dwfl_get_debuginfod_client) (dwfl);
      if (c != NULL)
	fd = (*fp_debuginfod_find_executable) (c, build_id_bits,
					       build_id_len, NULL);
    }

  return fd;
}

int
__libdwfl_debuginfod_find_debuginfo (Dwfl *dwfl,
				     const unsigned char *build_id_bits,
				     size_t build_id_len)
{
  int fd = -1;
  if (build_id_len > 0)
    {
      debuginfod_client *c = INTUSE (dwfl_get_debuginfod_client) (dwfl);
      if (c != NULL)
	fd = (*fp_debuginfod_find_debuginfo) (c, build_id_bits,
					      build_id_len, NULL);
    }

  return fd;
}

void
__libdwfl_debuginfod_end (debuginfod_client *c)
{
  if (c != NULL)
    (*fp_debuginfod_end) (c);
}

/* Try to get the libdebuginfod library functions.
   Only needs to be called once from dwfl_get_debuginfod_client.  */
static void
__libdwfl_debuginfod_init (void)
{
  void *debuginfod_so = dlopen(DEBUGINFOD_SONAME, RTLD_LAZY);

  if (debuginfod_so != NULL)
    {
      fp_debuginfod_begin = dlsym (debuginfod_so, "debuginfod_begin");
      fp_debuginfod_find_executable = dlsym (debuginfod_so,
					     "debuginfod_find_executable");
      fp_debuginfod_find_debuginfo = dlsym (debuginfod_so,
					    "debuginfod_find_debuginfo");
      fp_debuginfod_end = dlsym (debuginfod_so, "debuginfod_end");

      /* We either get them all, or we get none.  */
      if (fp_debuginfod_begin == NULL
	  || fp_debuginfod_find_executable == NULL
	  || fp_debuginfod_find_debuginfo == NULL
	  || fp_debuginfod_end == NULL)
	{
	  fp_debuginfod_begin = NULL;
	  fp_debuginfod_find_executable = NULL;
	  fp_debuginfod_find_debuginfo = NULL;
	  fp_debuginfod_end = NULL;
	  dlclose (debuginfod_so);
	}
    }
}

#else // ENABLE_LIBDEBUGINFOD

debuginfod_client *
dwfl_get_debuginfod_client (Dwfl *dummy __attribute__ ((unused)))
{
  return NULL;
}

#endif // ENABLE_LIBDEBUGINFOD
