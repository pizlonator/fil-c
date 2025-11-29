/* gtksecurememoryprivate.h - Allocator for non-pageable memory

   Copyright 2007  Stefan Walter
   Copyright 2020  GNOME Foundation

   SPDX-License-Identifier: LGPL-2.0-or-later

   The Gnome Keyring Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Keyring Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   see <http://www.gnu.org/licenses/>.

   Author: Stef Walter <stef@memberwebs.com>
*/

#pragma once

#include <stdlib.h>
#include <glib.h>

/*
 * Main functionality
 *
 * Allocations return NULL on failure.
 */

#define GTK_SECURE_USE_FALLBACK     0x0001

void *          gtk_secure_alloc_full   (const char *tag,
                                         size_t length,
                                         int options);

void *          gtk_secure_realloc_full (const char *tag,
                                         void *p,
                                         size_t length,
                                         int options);

void            gtk_secure_free         (void *p);

void            gtk_secure_free_full    (void *p,
                                         int fallback);

void            gtk_secure_clear        (void *p,
                                         size_t length);

int             gtk_secure_check        (const void *p);

void            gtk_secure_validate     (void);

char *          gtk_secure_strdup_full  (const char *tag,
                                         const char *str,
                                         int options);

char *          gtk_secure_strndup_full (const char *tag,
                                         const char *str,
                                         size_t length,
                                         int options);

void            gtk_secure_strclear     (char *str);

void            gtk_secure_strfree      (char *str);

/* Simple wrappers */

static inline void *gtk_secure_alloc (size_t length) {
  return gtk_secure_alloc_full ("gtk", length, GTK_SECURE_USE_FALLBACK);
}

static inline void *gtk_secure_realloc (void *p, size_t length) {
  return gtk_secure_realloc_full ("gtk", p, length, GTK_SECURE_USE_FALLBACK);
}

static inline void *gtk_secure_strdup (const char *str) {
  return gtk_secure_strdup_full ("gtk", str, GTK_SECURE_USE_FALLBACK);
}

static inline void *gtk_secure_strndup (const char *str, size_t length) {
  return gtk_secure_strndup_full ("gtk", str, length, GTK_SECURE_USE_FALLBACK);
}

typedef struct {
  const char *tag;
  size_t request_length;
  size_t block_length;
} gtk_secure_rec;

gtk_secure_rec *   gtk_secure_records    (unsigned int *count);
