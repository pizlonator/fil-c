/* GStreamer
 * Copyright (C) 2020 Igalia, S.L.
 *     Author: Víctor Jáquez <vjaquez@igalia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#include <gst/gst.h>

G_BEGIN_DECLS

#include <gst/va/gstva.h>

#define GST_TYPE_VA_DEVICE    (gst_va_device_get_type())
#define GST_IS_VA_DEVICE(obj) (GST_IS_MINI_OBJECT_TYPE((obj), GST_TYPE_VA_DEVICE))
#define GST_VA_DEVICE(obj)    ((GstVaDevice *)(obj))

typedef struct
{
  GstMiniObject mini_object;

  GstVaDisplay *display;
  gchar *render_device_path;
  gint index;
} GstVaDevice;

GType                 gst_va_device_get_type              (void);
GList *               gst_va_device_find_devices          (void);
void                  gst_va_device_list_free             (GList * devices);

G_END_DECLS
