/* GStreamer video parsers
 * Copyright (C) 2011 Mark Nauwelaerts <mark.nauwelaerts@collabora.co.uk>
 * Copyright (C) 2009 Tim-Philipp Müller <tim centricular net>
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

#include "gstvideoparserselements.h"

static gboolean
plugin_init (GstPlugin * plugin)
{
  gboolean ret = FALSE;

  ret |= GST_ELEMENT_REGISTER (h263parse, plugin);
  ret |= GST_ELEMENT_REGISTER (h264parse, plugin);
  ret |= GST_ELEMENT_REGISTER (diracparse, plugin);
  ret |= GST_ELEMENT_REGISTER (mpegvideoparse, plugin);
  ret |= GST_ELEMENT_REGISTER (mpeg4videoparse, plugin);
  ret |= GST_ELEMENT_REGISTER (pngparse, plugin);
  ret |= GST_ELEMENT_REGISTER (jpeg2000parse, plugin);
  ret |= GST_ELEMENT_REGISTER (h265parse, plugin);
  ret |= GST_ELEMENT_REGISTER (vc1parse, plugin);
  /**
   * element-vp9parse:
   *
   * Since: 1.20
   */
  ret |= GST_ELEMENT_REGISTER (vp9parse, plugin);
  /**
   * element-av1parse:
   *
   * Since: 1.20
   */
  ret |= GST_ELEMENT_REGISTER (av1parse, plugin);

  return ret;
}


GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    videoparsersbad,
    "videoparsers",
    plugin_init, VERSION, "LGPL", GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN);
