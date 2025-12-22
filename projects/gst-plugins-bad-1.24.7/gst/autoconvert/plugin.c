/* GStreamer
 * Copyright 2010 ST-Ericsson SA
 *  @author: Benjamin Gaignard <benjamin.gaignard@stericsson.com>
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

#include "gstautoconvert.h"
#include "gstautovideoconvert.h"
#include "gstautovideoflip.h"
#include "gstautodeinterlace.h"

static gboolean
plugin_init (GstPlugin * plugin)
{
  gboolean ret = FALSE;

  ret |= GST_ELEMENT_REGISTER (autoconvert, plugin);
  ret |= GST_ELEMENT_REGISTER (autovideoconvert, plugin);
  ret |= GST_ELEMENT_REGISTER (autodeinterlace, plugin);
  ret |= GST_ELEMENT_REGISTER (autovideoflip, plugin);
  gst_type_mark_as_plugin_api (GST_TYPE_BASE_AUTO_CONVERT, 0);

  return ret;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    autoconvert,
    "Selects converter element based on caps",
    plugin_init, VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
