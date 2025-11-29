/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#pragma once

#include <glib.h>
#include <gtk/gtkenums.h>

G_BEGIN_DECLS

/* Cache as many ranges of height-for-width
 * (or width-for-height) as can be rational
 * for a said widget to have, if a label can
 * only wrap to 3 lines, only 3 caches will
 * ever be allocated for it.
 */
#define GTK_SIZE_REQUEST_CACHED_SIZES   (64)

typedef struct {
  int minimum_size;
  int natural_size;
} CachedSizeX;

typedef struct {
  int minimum_size;
  int natural_size;
  int minimum_baseline;
  int natural_baseline;
} CachedSizeY;

typedef struct
{
  int        lower_for_size; /* The minimum for_size with the same result */
  int        upper_for_size; /* The maximum for_size with the same result */
  CachedSizeX cached_size;
} SizeRequestX;

typedef struct
{
  int        lower_for_size; /* The minimum for_size with the same result */
  int        upper_for_size; /* The maximum for_size with the same result */
  CachedSizeY cached_size;
} SizeRequestY;

typedef struct {
  SizeRequestX **requests_x;
  SizeRequestY **requests_y;

  CachedSizeX  cached_size_x;
  CachedSizeY  cached_size_y;

  guint       request_mode          : 3; /* GtkSizeRequestMode */
  guint       request_mode_valid    : 1;
  struct {
    guint       n_cached_requests   : 15;
    guint       last_cached_request : 15;
    guint       cached_size_valid   : 1;
  }           flags[2];
} SizeRequestCache;

void            _gtk_size_request_cache_init                    (SizeRequestCache       *cache);
void            _gtk_size_request_cache_free                    (SizeRequestCache       *cache);

void            _gtk_size_request_cache_clear                   (SizeRequestCache       *cache);
void            _gtk_size_request_cache_commit                  (SizeRequestCache       *cache,
                                                                 GtkOrientation          orientation,
                                                                 int                     for_size,
                                                                 int                     minimum_size,
                                                                 int                     natural_size,
                                                                 int                     minimum_baseline,
                                                                 int                     natural_baseline);
gboolean        _gtk_size_request_cache_lookup                  (const SizeRequestCache *cache,
                                                                 GtkOrientation          orientation,
                                                                 int                     for_size,
                                                                 int                    *minimum,
                                                                 int                    *natural,
                                                                 int                    *minimum_baseline,
                                                                 int                    *natural_baseline);

G_END_DECLS

