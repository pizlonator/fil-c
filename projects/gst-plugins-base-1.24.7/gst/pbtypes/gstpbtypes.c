/* GStreamer
 * Copyright (C) 2015 Jan Schmidt <jan@centricular.com>
 *
 * gstpbtypes: Plugin which registers extra caps types from plugins-base libs
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

#include <gst/video/video.h>
#include "gstpbtypes.h"

GST_DYNAMIC_TYPE_REGISTER_DEFINE (video_multiview_flagset,
    GST_TYPE_VIDEO_MULTIVIEW_FLAGSET);

static gboolean
plugin_init (GstPlugin * plugin)
{
  return GST_DYNAMIC_TYPE_REGISTER (video_multiview_flagset, plugin);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    pbtypes,
    "gst-plugins-base dynamic types",
    plugin_init, VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
