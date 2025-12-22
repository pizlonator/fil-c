/* GStreamer
 * Copyright (C) 2020 Seungha Yang <seungha@centricular.com>
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
#include <config.h>
#endif

#include <winapifamily.h>

#include "gstwasapi2sink.h"
#include "gstwasapi2src.h"
#include "gstwasapi2device.h"
#include "gstwasapi2util.h"
#include <mfapi.h>

GST_DEBUG_CATEGORY (gst_wasapi2_debug);
GST_DEBUG_CATEGORY (gst_wasapi2_client_debug);

static void
plugin_deinit (gpointer data)
{
  MFShutdown ();
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  GstRank rank = GST_RANK_PRIMARY + 1;
  HRESULT hr;

  /**
   * plugin-wasapi2:
   *
   * Since: 1.18
   */

  hr = MFStartup (MF_VERSION, MFSTARTUP_NOSOCKET);
  if (!gst_wasapi2_result (hr)) {
    GST_WARNING ("MFStartup failure, hr: 0x%x", hr);
    return TRUE;
  }

  GST_DEBUG_CATEGORY_INIT (gst_wasapi2_debug, "wasapi2", 0, "wasapi2");
  GST_DEBUG_CATEGORY_INIT (gst_wasapi2_client_debug, "wasapi2client",
      0, "wasapi2client");

  gst_element_register (plugin, "wasapi2sink", rank, GST_TYPE_WASAPI2_SINK);
  gst_element_register (plugin, "wasapi2src", rank, GST_TYPE_WASAPI2_SRC);

  gst_wasapi2_device_provider_register (plugin, rank);

  g_object_set_data_full (G_OBJECT (plugin),
      "plugin-wasapi2-shutdown", "shutdown-data",
      (GDestroyNotify) plugin_deinit);

  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    wasapi2,
    "Windows audio session API plugin",
    plugin_init, VERSION, "LGPL", GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
