/* GStreamer
 * Copyright (C) 2022 Seungha Yang <seungha@centricular.com>
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
 * plugin-directshow:
 *
 * Since: 1.22
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>

#include "dshowvideosink.h"
#include "gstdshowaudiodec.h"
#include "gstdshowvideodec.h"
#include "gstdshowaudiosrc.h"
#include "gstdshowvideosrc.h"
#include "dshowdeviceprovider.h"

GST_DEBUG_CATEGORY (dshowdec_debug);
GST_DEBUG_CATEGORY (dshowsrcwrapper_debug);
GST_DEBUG_CATEGORY (dshowvideosrc_debug);

static gboolean
plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (dshowdec_debug, "dshowdec", 0, "DirectShow decoder");
  GST_DEBUG_CATEGORY_INIT (dshowsrcwrapper_debug, "dshowsrcwrapper", 0,
      "DirectShow source wrapper");
  GST_DEBUG_CATEGORY_INIT (dshowvideosrc_debug, "dshowvideosrc", 0,
      "Directshow video source");

  dshow_adec_register (plugin);
  dshow_vdec_register (plugin);

  gst_element_register (plugin, "dshowvideosink",
      GST_RANK_MARGINAL, GST_TYPE_DSHOWVIDEOSINK);
  gst_element_register (plugin, "dshowaudiosrc",
      GST_RANK_NONE, GST_TYPE_DSHOWAUDIOSRC);
  gst_element_register (plugin, "dshowvideosrc",
      GST_RANK_NONE, GST_TYPE_DSHOWVIDEOSRC);

  gst_device_provider_register (plugin, "dshowdeviceprovider",
      GST_RANK_NONE, GST_TYPE_DSHOW_DEVICE_PROVIDER);

  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    directshow,
    "DirectShow plugin",
    plugin_init, VERSION, "LGPL", GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
