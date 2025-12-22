/* GStreamer bz2 plugin
 * Copyright (C) 2006 Lutz Müller <lutz topfrose de>

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

#include "gstbz2enc.h"
#include "gstbz2dec.h"

#include <gst/gstplugin.h>

#include <string.h>

static gboolean
plugin_init (GstPlugin * p)
{
  gboolean ret = FALSE;

  ret |= GST_ELEMENT_REGISTER (bz2enc, p);
  ret |= GST_ELEMENT_REGISTER (bz2dec, p);

  return ret;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR, GST_VERSION_MINOR, bz2,
    "Compress or decompress streams",
    plugin_init, VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
