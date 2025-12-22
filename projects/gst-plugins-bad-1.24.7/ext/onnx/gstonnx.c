
/*
 * GStreamer gstreamer-onnx
 * Copyright (C) 2021 Collabora Ltd
 *
 * gstonnx.c
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

#include "decoders/gstssdobjectdetector.h"
#include "gstonnxinference.h"
#include "tensor/gsttensormeta.h"

static gboolean
plugin_init (GstPlugin * plugin)
{
  gboolean success = GST_ELEMENT_REGISTER (ssd_object_detector, plugin);
  success |= GST_ELEMENT_REGISTER (onnx_inference, plugin);

  return success;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    onnx,
    "ONNX neural network plugin",
    plugin_init, VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN);
