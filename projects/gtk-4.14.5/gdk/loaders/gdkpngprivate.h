/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2021 Red Hat, Inc.
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

#pragma once

#include "gdktexture.h"
#include <gio/gio.h>

#define PNG_SIGNATURE "\x89PNG"

GdkTexture *gdk_load_png        (GBytes         *bytes,
                                 GHashTable     *options,
                                 GError        **error);

GBytes     *gdk_save_png        (GdkTexture     *texture);

static inline gboolean
gdk_is_png (GBytes *bytes)
{
  const char *data;
  gsize size;

  data = g_bytes_get_data (bytes, &size);

  return size > strlen (PNG_SIGNATURE) &&
         memcmp (data, PNG_SIGNATURE, strlen (PNG_SIGNATURE)) == 0;
}

