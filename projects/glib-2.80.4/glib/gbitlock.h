/*
 * Copyright © 2008 Ryan Lortie
 * Copyright © 2010 Codethink Limited
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#ifndef __G_BITLOCK_H__
#define __G_BITLOCK_H__

#include <glib/gtypes.h>

#if !defined (__GLIB_H_INSIDE__) && !defined (GLIB_COMPILATION)
#error "Only <glib.h> can be included directly."
#endif

G_BEGIN_DECLS

GLIB_AVAILABLE_IN_ALL
void      g_bit_lock                      (volatile gint *address,
                                           gint           lock_bit);
GLIB_AVAILABLE_IN_ALL
gboolean  g_bit_trylock                   (volatile gint *address,
                                           gint           lock_bit);
GLIB_AVAILABLE_IN_ALL
void      g_bit_unlock                    (volatile gint *address,
                                           gint           lock_bit);

GLIB_AVAILABLE_IN_ALL
void      g_pointer_bit_lock              (volatile void *address,
                                           gint           lock_bit);

GLIB_AVAILABLE_IN_2_80
void      g_pointer_bit_lock_and_get      (gpointer address,
                                           guint lock_bit,
                                           gpointer *out_ptr);

GLIB_AVAILABLE_IN_ALL
gboolean  g_pointer_bit_trylock           (volatile void *address,
                                           gint           lock_bit);
GLIB_AVAILABLE_IN_ALL
void      g_pointer_bit_unlock            (volatile void *address,
                                           gint           lock_bit);

GLIB_AVAILABLE_IN_2_80
gpointer  g_pointer_bit_lock_mask_ptr     (gpointer ptr,
                                           guint lock_bit,
                                           gboolean set,
                                           guintptr preserve_mask,
                                           gpointer preserve_ptr);

GLIB_AVAILABLE_IN_2_80
void g_pointer_bit_unlock_and_set         (void *address,
                                           guint lock_bit,
                                           gpointer ptr,
                                           guintptr preserve_mask);

#ifdef __GNUC__

#define g_pointer_bit_lock(address, lock_bit) \
  (G_GNUC_EXTENSION ({                                                       \
    G_STATIC_ASSERT (sizeof *(address) == sizeof (gpointer));                \
    g_pointer_bit_lock ((address), (lock_bit));                              \
  }))

#define g_pointer_bit_lock_and_get(address, lock_bit, out_ptr)     \
  (G_GNUC_EXTENSION ({                                             \
    G_STATIC_ASSERT (sizeof *(address) == sizeof (gpointer));      \
    g_pointer_bit_lock_and_get ((address), (lock_bit), (out_ptr)); \
  }))

#define g_pointer_bit_trylock(address, lock_bit) \
  (G_GNUC_EXTENSION ({                                                       \
    G_STATIC_ASSERT (sizeof *(address) == sizeof (gpointer));                \
    g_pointer_bit_trylock ((address), (lock_bit));                           \
  }))

#define g_pointer_bit_unlock(address, lock_bit) \
  (G_GNUC_EXTENSION ({                                                       \
    G_STATIC_ASSERT (sizeof *(address) == sizeof (gpointer));                \
    g_pointer_bit_unlock ((address), (lock_bit));                            \
  }))

#define g_pointer_bit_unlock_and_set(address, lock_bit, ptr, preserve_mask)       \
  (G_GNUC_EXTENSION ({                                                            \
    G_STATIC_ASSERT (sizeof *(address) == sizeof (gpointer));                     \
    g_pointer_bit_unlock_and_set ((address), (lock_bit), (ptr), (preserve_mask)); \
  }))

#endif

G_END_DECLS

#endif /* __G_BITLOCK_H_ */
