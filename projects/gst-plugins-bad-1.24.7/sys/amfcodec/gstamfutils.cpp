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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <core/Factory.h>
#include "gstamfutils.h"
#include <gmodule.h>

using namespace amf;

AMFFactory *_factory = nullptr;
static gboolean loaded = FALSE;

static gboolean
gst_amf_load_library (void)
{
  AMF_RESULT result;
  GModule *amf_module = nullptr;
  AMFInit_Fn init_func = nullptr;

  amf_module = g_module_open (AMF_DLL_NAMEA, G_MODULE_BIND_LAZY);
  if (!amf_module)
    return FALSE;

  if (!g_module_symbol (amf_module, AMF_INIT_FUNCTION_NAME, (gpointer *)
          & init_func)) {
    g_module_close (amf_module);
    amf_module = nullptr;

    return FALSE;
  }

  result = init_func (AMF_FULL_VERSION, &_factory);
  if (result != AMF_OK) {
    g_module_close (amf_module);
    amf_module = nullptr;
    _factory = nullptr;
    return FALSE;
  }

  return TRUE;
}

gboolean
gst_amf_init_once (void)
{
  static gsize init_once = 0;

  if (g_once_init_enter (&init_once)) {
    loaded = gst_amf_load_library ();
    g_once_init_leave (&init_once, 1);
  }

  return loaded;
}

gpointer
gst_amf_get_factory (void)
{
  return (gpointer) _factory;
}

const gchar *
gst_amf_result_to_string (AMF_RESULT result)
{
#define CASE(err) \
    case err: \
    return G_STRINGIFY (err);

  switch (result) {
      CASE (AMF_OK);
      CASE (AMF_FAIL);
      CASE (AMF_UNEXPECTED);
      CASE (AMF_ACCESS_DENIED);
      CASE (AMF_INVALID_ARG);
      CASE (AMF_OUT_OF_RANGE);
      CASE (AMF_OUT_OF_MEMORY);
      CASE (AMF_INVALID_POINTER);
      CASE (AMF_NO_INTERFACE);
      CASE (AMF_NOT_IMPLEMENTED);
      CASE (AMF_NOT_SUPPORTED);
      CASE (AMF_NOT_FOUND);
      CASE (AMF_ALREADY_INITIALIZED);
      CASE (AMF_NOT_INITIALIZED);
      CASE (AMF_INVALID_FORMAT);
      CASE (AMF_WRONG_STATE);
      CASE (AMF_FILE_NOT_OPEN);
      CASE (AMF_NO_DEVICE);
      CASE (AMF_DIRECTX_FAILED);
      CASE (AMF_OPENCL_FAILED);
      CASE (AMF_GLX_FAILED);
      CASE (AMF_XV_FAILED);
      CASE (AMF_ALSA_FAILED);
      CASE (AMF_EOF);
      CASE (AMF_REPEAT);
      CASE (AMF_INPUT_FULL);
      CASE (AMF_RESOLUTION_CHANGED);
      CASE (AMF_RESOLUTION_UPDATED);
      CASE (AMF_INVALID_DATA_TYPE);
      CASE (AMF_INVALID_RESOLUTION);
      CASE (AMF_CODEC_NOT_SUPPORTED);
      CASE (AMF_SURFACE_FORMAT_NOT_SUPPORTED);
      CASE (AMF_SURFACE_MUST_BE_SHARED);
      CASE (AMF_DECODER_NOT_PRESENT);
      CASE (AMF_DECODER_SURFACE_ALLOCATION_FAILED);
      CASE (AMF_DECODER_NO_FREE_SURFACES);
      CASE (AMF_ENCODER_NOT_PRESENT);
      CASE (AMF_DEM_ERROR);
      CASE (AMF_DEM_PROPERTY_READONLY);
      CASE (AMF_DEM_REMOTE_DISPLAY_CREATE_FAILED);
      CASE (AMF_DEM_START_ENCODING_FAILED);
      CASE (AMF_DEM_QUERY_OUTPUT_FAILED);
      CASE (AMF_TAN_CLIPPING_WAS_REQUIRED);
      CASE (AMF_TAN_UNSUPPORTED_VERSION);
      CASE (AMF_NEED_MORE_INPUT);
    default:
      break;
  }
#undef CASE
  return "Unknown";
}
