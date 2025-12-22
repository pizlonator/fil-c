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
 * plugin-wic:
 *
 * Since: 1.22
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include "gstwicimagingfactory.h"
#include "gstwicjpegdec.h"
#include "gstwicpngdec.h"

GST_DEBUG_CATEGORY (gst_wic_debug);
GST_DEBUG_CATEGORY (gst_wic_utils_debug);

#define GST_CAT_DEFAULT gst_wic_debug

static gboolean
plugin_init (GstPlugin * plugin)
{
  GstWicImagingFactory *factory;
  HRESULT hr;

  GST_DEBUG_CATEGORY_INIT (gst_wic_debug,
      "wic", 0, "Windows Imaging Component");
  GST_DEBUG_CATEGORY_INIT (gst_wic_utils_debug, "wicutils", 0, "wicutils");

  factory = gst_wic_imaging_factory_new ();
  if (!factory)
    return TRUE;

  hr = gst_wic_imaging_factory_check_codec_support (factory,
      TRUE, GUID_ContainerFormatJpeg);
  if (SUCCEEDED (hr)) {
    gst_element_register (plugin,
        "wicjpegdec", GST_RANK_SECONDARY, GST_TYPE_WIC_JPEG_DEC);
  } else {
    GST_INFO_OBJECT (factory,
        "Jpeg decoder is not supported, hr: 0x%x", (guint) hr);
  }

  hr = gst_wic_imaging_factory_check_codec_support (factory,
      TRUE, GUID_ContainerFormatPng);
  if (SUCCEEDED (hr)) {
    gst_element_register (plugin,
        "wicpngdec", GST_RANK_SECONDARY, GST_TYPE_WIC_PNG_DEC);
  } else {
    GST_INFO_OBJECT (factory,
        "Png decoder is not supported, hr: 0x%x", (guint) hr);
  }

  gst_object_unref (factory);

  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    wic,
    "Windows Imaging Component (WIC) plugin",
    plugin_init, VERSION, "LGPL", GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
