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

#define TIFF_SIGNATURE1 "MM\x00\x2a"
#define TIFF_SIGNATURE2 "II\x2a\x00"

GdkTexture *gdk_load_tiff         (GBytes           *bytes,
                                   GError          **error);

GBytes *    gdk_save_tiff         (GdkTexture       *texture);

static inline gboolean
gdk_is_tiff (GBytes *bytes)
{
  const char *data;
  gsize size;

  data = g_bytes_get_data (bytes, &size);

  return (size > strlen (TIFF_SIGNATURE1) &&
          memcmp (data, TIFF_SIGNATURE1, strlen (TIFF_SIGNATURE1)) == 0) ||
         (size > strlen (TIFF_SIGNATURE2) &&
          memcmp (data, TIFF_SIGNATURE2, strlen (TIFF_SIGNATURE2)) == 0);
}

