/* GStreamer
 * Copyright (C) 2021 Seungha Yang <seungha@centricular.com>
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

#include "gstasiodeviceprovider.h"
#include "gstasiosrc.h"
#include "gstasiosink.h"

static gboolean
plugin_init (GstPlugin * plugin)
{
  GstRank rank = GST_RANK_SECONDARY;

  if (!gst_element_register (plugin, "asiosrc", rank, GST_TYPE_ASIO_SRC))
    return FALSE;
  if (!gst_element_register (plugin, "asiosink", rank, GST_TYPE_ASIO_SINK))
    return FALSE;
  if (!gst_device_provider_register (plugin, "asiodeviceprovider",
          rank, GST_TYPE_ASIO_DEVICE_PROVIDER))
    return FALSE;

  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    asio,
    "Steinberg ASIO plugin",
    plugin_init, VERSION, "LGPL", GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
