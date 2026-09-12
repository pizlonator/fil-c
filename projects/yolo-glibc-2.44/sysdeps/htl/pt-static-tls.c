/* Initialize the static TLS space for the link map in all existing threads.
   Copyright (C) 2000-2026 Free Software Foundation, Inc.
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
   License along with the GNU C Library;  if not, see
   <https://www.gnu.org/licenses/>.  */

#include <pthread.h>
#include <string.h>

#include <pt-internal.h>
#include <ldsodefs.h>

static inline void __attribute__((always_inline))
init_one_static_tls (tcbhead_t *curp, struct link_map *map)
{
# if TLS_TCB_AT_TP
  void *dest = (char *) curp - map->l_tls_offset;
# elif TLS_DTV_AT_TP
  void *dest = (char *) curp + map->l_tls_offset + TLS_PRE_TCB_SIZE;
# else
#  error "Either TLS_TCB_AT_TP or TLS_DTV_AT_TP must be defined"
# endif

  /* Initialize the memory.  */
  memset (__mempcpy (dest, map->l_tls_initimage, map->l_tls_initimage_size),
	  '\0', map->l_tls_blocksize - map->l_tls_initimage_size);
}


void
_dl_init_static_tls (struct link_map *map)
{
  int i;

  if (!GL (dl_pthread_num_threads))
    {
      init_one_static_tls (THREAD_SELF, map);
      return;
    }

  __mach_rwlock_wrlock (GL (dl_pthread_threads_lock));
  for (i = 0; i < GL (dl_pthread_num_threads); ++i)
    {
      struct __pthread *t = GL (dl_pthread_threads)[i];

      if (t == NULL)
	continue;

      init_one_static_tls (t->tcb, map);
    }
  __mach_rwlock_unlock (GL (dl_pthread_threads_lock));
}
