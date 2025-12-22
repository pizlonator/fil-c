/* GStreamer
 *
 * Copyright (C) 2023 Alexander Slobodeniuk <aslobodeniuk@fluendo.com>
 *
 * insertbin: a GstBin to insertally link filter-like elements.
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
 * plugin-insertbin:
 *
 * Since: 1.24
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gst/insertbin/gstinsertbin.h>

GST_ELEMENT_REGISTER_DECLARE (insertbin);
GST_ELEMENT_REGISTER_DEFINE (insertbin, "insertbin", GST_RANK_NONE,
    GST_TYPE_INSERT_BIN);

static gboolean
plugin_init (GstPlugin * plugin)
{
  return GST_ELEMENT_REGISTER (insertbin, plugin);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    insertbin,
    "insertbin",
    plugin_init, VERSION, "LGPL", GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
