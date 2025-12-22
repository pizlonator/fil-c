/* GStreamer
 * Copyright (C) 2021 Sebastian Dröge <sebastian@centricular.com>
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
 * Free Software Foundation, Inc., 51 Franklin Street, Suite 500,
 * Boston, MA 02110-1335, USA.
 */

/**
 * SECTION:plugin-aja
 *
 * Plugin for [AJA](https://www.aja.com) capture and output cards.
 *
 * This plugin requires the AJA NTV2 SDK version 16 or newer and works with
 * both the [Open Source](https://github.com/aja-video/ntv2.git) and
 * [proprietary](https://www.aja.com/family/developer) version.
 *
 * Since: 1.24
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ajabase/system/debug.h>
#include <gst/gst.h>

#include "gstajacommon.h"
#include "gstajadeviceprovider.h"
#include "gstajasink.h"
#include "gstajasinkcombiner.h"
#include "gstajasrc.h"
#include "gstajasrcdemux.h"

static gboolean plugin_init(GstPlugin* plugin) {
  AJADebug::Open();

  gst_aja_common_init();

  gst_element_register(plugin, "ajasrc", GST_RANK_NONE, GST_TYPE_AJA_SRC);
  gst_element_register(plugin, "ajasrcdemux", GST_RANK_NONE,
                       GST_TYPE_AJA_SRC_DEMUX);
  gst_element_register(plugin, "ajasink", GST_RANK_NONE, GST_TYPE_AJA_SINK);
  gst_element_register(plugin, "ajasinkcombiner", GST_RANK_NONE,
                       GST_TYPE_AJA_SINK_COMBINER);

  gst_device_provider_register(plugin, "ajadeviceprovider", GST_RANK_PRIMARY,
                               GST_TYPE_AJA_DEVICE_PROVIDER);

  return TRUE;
}

GST_PLUGIN_DEFINE(GST_VERSION_MAJOR, GST_VERSION_MINOR, aja,
                  "GStreamer AJA plugin", plugin_init, VERSION, "LGPL",
                  PACKAGE_NAME, GST_PACKAGE_ORIGIN)
