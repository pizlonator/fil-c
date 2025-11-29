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

#include "gdkmemorytexture.h"
#include <gio/gio.h>

#define JPEG_SIGNATURE "\xff\xd8"

GdkTexture *gdk_load_jpeg         (GBytes           *bytes,
                                   GError          **error);

GBytes     *gdk_save_jpeg         (GdkTexture     *texture);

static inline gboolean
gdk_is_jpeg (GBytes *bytes)
{
  const char *data;
  gsize size;

  data = g_bytes_get_data (bytes, &size);

  return size > strlen (JPEG_SIGNATURE) &&
         memcmp (data, JPEG_SIGNATURE, strlen (JPEG_SIGNATURE)) == 0;
}

