/*
 * GStreamer
 * Copyright (C) 2009 Nokia Corporation <multimedia@maemo.org>
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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstcamerabin-enum.h"

/**
 * GstCameraBin2Mode:
 */
GType
gst_camerabin_mode_get_type (void)
{
  static GType gtype = 0;

  if (gtype == 0) {
    static const GEnumValue values[] = {
      /* {MODE_PREVIEW, "Preview mode (should be default?)", "mode-preview"}, */
      {MODE_IMAGE, "Still image capture (default)", "mode-image"},
      {MODE_VIDEO, "Video recording", "mode-video"},
      {0, NULL, NULL}
    };

    gtype = g_enum_register_static ("GstCameraBin2Mode", values);
  }
  return gtype;
}
