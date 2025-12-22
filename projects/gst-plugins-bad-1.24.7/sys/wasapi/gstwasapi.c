/*
 * Copyright (C) 2008 Ole André Vadla Ravnås <ole.andre.ravnas@tandberg.com>
 * Copyright (C) 2018 Centricular Ltd.
 *   Author: Nirbheek Chauhan <nirbheek@centricular.com>
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
#  include <config.h>
#endif

#include "gstwasapisink.h"
#include "gstwasapisrc.h"
#include "gstwasapidevice.h"

GST_DEBUG_CATEGORY (gst_wasapi_debug);

static gboolean
plugin_init (GstPlugin * plugin)
{
  if (!gst_element_register (plugin, "wasapisink", GST_RANK_PRIMARY,
          GST_TYPE_WASAPI_SINK))
    return FALSE;

  if (!gst_element_register (plugin, "wasapisrc", GST_RANK_PRIMARY,
          GST_TYPE_WASAPI_SRC))
    return FALSE;

  if (!gst_device_provider_register (plugin, "wasapideviceprovider",
          GST_RANK_PRIMARY, GST_TYPE_WASAPI_DEVICE_PROVIDER))
    return FALSE;

  GST_DEBUG_CATEGORY_INIT (gst_wasapi_debug, "wasapi",
      0, "Windows audio session API generic");

  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    wasapi,
    "Windows audio session API plugin",
    plugin_init, VERSION, "LGPL", GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
