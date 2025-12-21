/* GStreamer
 * Copyright (C) <2007> Wim Taymans <wim.taymans@gmail.com>
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
 * SECTION:plugin-playback:
 * @short_description: Set of elements to create dynamic pipelines (or part of it) to play
 * media files.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <gst/gst.h>

#include <glib/gi18n-lib.h>
#include <gst/pbutils/pbutils.h>

#include "gstplaybackelements.h"


static gboolean
plugin_init (GstPlugin * plugin)
{
  gboolean res = FALSE;

  res |= GST_ELEMENT_REGISTER (playbin, plugin);
  res |= GST_ELEMENT_REGISTER (playbin3, plugin);
  res |= GST_ELEMENT_REGISTER (playsink, plugin);
  res |= GST_ELEMENT_REGISTER (subtitleoverlay, plugin);
  res |= GST_ELEMENT_REGISTER (streamsynchronizer, plugin);
  res |= GST_ELEMENT_REGISTER (decodebin, plugin);
  res |= GST_ELEMENT_REGISTER (decodebin3, plugin);
  res |= GST_ELEMENT_REGISTER (uridecodebin, plugin);
  res |= GST_ELEMENT_REGISTER (uridecodebin3, plugin);
  res |= GST_ELEMENT_REGISTER (urisourcebin, plugin);
  res |= GST_ELEMENT_REGISTER (parsebin, plugin);

  return res;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    playback,
    "various playback elements", plugin_init, VERSION, GST_LICENSE,
    GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
