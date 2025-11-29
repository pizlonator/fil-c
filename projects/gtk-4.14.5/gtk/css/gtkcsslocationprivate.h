/*
 * Copyright © 2019 Benjamin Otte
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */


#pragma once

#include "gtkcsslocation.h"

G_BEGIN_DECLS

static inline void
gtk_css_location_init (GtkCssLocation *location)
{
  memset (location, 0, sizeof (GtkCssLocation));
}

static inline void
gtk_css_location_advance (GtkCssLocation *location,
                          gsize           bytes,
                          gsize           chars)
{
  location->bytes += bytes;
  location->chars += chars;
  location->line_bytes += bytes;
  location->line_chars += chars;
}

static inline void
gtk_css_location_advance_newline (GtkCssLocation *location,
                                  gboolean        is_windows)
{
  location->bytes += is_windows ? 2 : 1;
  location->chars += is_windows ? 2 : 1;
  location->line_bytes = 0;
  location->line_chars = 0;
  location->lines++;
}

G_END_DECLS


