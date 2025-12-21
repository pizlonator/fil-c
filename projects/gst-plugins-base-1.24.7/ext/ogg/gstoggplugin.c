/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
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

#include <gst/gst.h>

#include "gstoggelements.h"
#include "gstoggdemux.h"
#include "gstoggmux.h"

static gboolean
plugin_init (GstPlugin * plugin)
{
  gboolean ret = FALSE;

  ret |= GST_ELEMENT_REGISTER (oggdemux, plugin);
  ret |= GST_ELEMENT_REGISTER (oggmux, plugin);
  ret |= GST_ELEMENT_REGISTER (ogmaudioparse, plugin);
  ret |= GST_ELEMENT_REGISTER (ogmvideoparse, plugin);
  ret |= GST_ELEMENT_REGISTER (ogmtextparse, plugin);
  ret |= GST_ELEMENT_REGISTER (oggparse, plugin);
  ret |= GST_ELEMENT_REGISTER (oggaviparse, plugin);

  return ret;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    ogg,
    "ogg stream manipulation (info about ogg: http://xiph.org)",
    plugin_init, VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
