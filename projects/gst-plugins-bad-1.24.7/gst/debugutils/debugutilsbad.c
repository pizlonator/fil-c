/* GStreamer
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
#  include "config.h"
#endif

#include <gst/gst.h>


#include "gstdebugutilsbadelements.h"

static gboolean
plugin_init (GstPlugin * plugin)
{
  gboolean ret = FALSE;

  ret |= GST_ELEMENT_REGISTER (checksumsink, plugin);
  ret |= GST_ELEMENT_REGISTER (chopmydata, plugin);
  ret |= GST_ELEMENT_REGISTER (clockselect, plugin);
  ret |= GST_ELEMENT_REGISTER (compare, plugin);
  ret |= GST_ELEMENT_REGISTER (debugspy, plugin);
  ret |= GST_ELEMENT_REGISTER (errorignore, plugin);
  ret |= GST_ELEMENT_REGISTER (fakeaudiosink, plugin);
  ret |= GST_ELEMENT_REGISTER (fakevideosink, plugin);
  ret |= GST_ELEMENT_REGISTER (fpsdisplaysink, plugin);
  ret |= GST_ELEMENT_REGISTER (testsrcbin, plugin);
  ret |= GST_ELEMENT_REGISTER (videocodectestsink, plugin);
  ret |= GST_ELEMENT_REGISTER (watchdog, plugin);

  return ret;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    debugutilsbad,
    "Collection of elements that may or may not be useful for debugging",
    plugin_init, VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
