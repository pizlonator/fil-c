/*
 * Gstreamer
 *
 * Copyright (C) 2023 Collabora
 *   Author: Benjamin Gaignard <benjamin.gaignard@collabora.com>
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
 * plugin-codec2json:
 *
 * Since: 1.24
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstav12json.h"
#include "gsth2642json.h"
#include "gsth2652json.h"
#include "gstvp82json.h"

static gboolean
plugin_init (GstPlugin * plugin)
{
  if (!gst_element_register (plugin, "vp82json", GST_RANK_NONE,
          GST_TYPE_VP8_2_JSON))
    return FALSE;

  if (!gst_element_register (plugin, "av12json", GST_RANK_NONE,
          GST_TYPE_AV1_2_JSON))
    return FALSE;

  if (!gst_element_register (plugin, "h2642json", GST_RANK_NONE,
          GST_TYPE_H264_2_JSON))
    return FALSE;

  if (!gst_element_register (plugin, "h2652json", GST_RANK_NONE,
          GST_TYPE_H265_2_JSON))
    return FALSE;

  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    codec2json,
    "Plugin with feature to annotate and format CODEC bitstream in JSON",
    plugin_init, VERSION, "LGPL", GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
