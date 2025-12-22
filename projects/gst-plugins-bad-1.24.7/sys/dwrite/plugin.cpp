/* GStreamer
 * Copyright (C) 2023 Seungha Yang <seungha@centricular.com>
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

/**
 * plugin-dwrite:
 *
 * Since: 1.24
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstdwritesubtitlemux.h"
#include "gstdwriteclockoverlay.h"
#include "gstdwritetextoverlay.h"
#include "gstdwritetimeoverlay.h"
#include "gstdwritesubtitleoverlay.h"

GST_DEBUG_CATEGORY (gst_dwrite_debug);

static gboolean
plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (gst_dwrite_debug, "dwrite", 0, "dwrite");

  gst_element_register (plugin, "dwritesubtitlemux", GST_RANK_NONE,
      GST_TYPE_DWRITE_SUBTITLE_MUX);
  gst_element_register (plugin, "dwriteclockoverlay", GST_RANK_NONE,
      GST_TYPE_DWRITE_CLOCK_OVERLAY);
  gst_element_register (plugin, "dwritetextoverlay", GST_RANK_NONE,
      GST_TYPE_DWRITE_TEXT_OVERLAY);
  gst_element_register (plugin, "dwritetimeoverlay", GST_RANK_NONE,
      GST_TYPE_DWRITE_TIME_OVERLAY);
  gst_element_register (plugin, "dwritesubtitleoverlay", GST_RANK_NONE,
      GST_TYPE_DWRITE_SUBTITLE_OVERLAY);

  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    dwrite,
    "dwrite",
    plugin_init, VERSION, "LGPL", GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN);
