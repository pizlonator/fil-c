/* GStreamer
 * Copyright (C) 2020 Seungha Yang <seungha.yang@navercorp.com>
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

#ifndef __GST_MF_TRANSFORM_OBJECT_H__
#define __GST_MF_TRANSFORM_OBJECT_H__

#include <gst/gst.h>
#include "gstmfutils.h"
#include <codecapi.h>
#include "gststrmif.h"

G_BEGIN_DECLS

#define GST_TYPE_MF_TRANSFORM_OBJECT  (gst_mf_transform_get_type())
G_DECLARE_FINAL_TYPE (GstMFTransform, gst_mf_transform,
    GST, MF_TRANSFORM, GstObject);

#define GST_MF_TRANSFORM_FLOW_NEED_DATA GST_FLOW_CUSTOM_SUCCESS

/* NOTE: This GUID is defined in mfapi.h header but it's available only for
 * at least Windows 10 RS1. So defining the GUID here again so that
 * make use even if build target (e.g., WINVER) wasn't for Windows 10 */
DEFINE_GUID(GST_GUID_MFT_ENUM_ADAPTER_LUID,
    0x1d39518c, 0xe220, 0x4da8, 0xa0, 0x7f, 0xba, 0x17, 0x25, 0x52, 0xd6, 0xb1);

/* below GUIDs are defined in mftransform.h for Windows 8 or greater
 * FIXME: remove below defines when we bump minimum supported OS version to
 * Windows 10 */
DEFINE_GUID(GST_GUID_MF_SA_D3D11_AWARE,
    0x206b4fc8, 0xfcf9, 0x4c51, 0xaf, 0xe3, 0x97, 0x64, 0x36, 0x9e, 0x33, 0xa0);
DEFINE_GUID(GST_GUID_MF_SA_BUFFERS_PER_SAMPLE,
    0x873c5171, 0x1e3d, 0x4e25, 0x98, 0x8d, 0xb4, 0x33, 0xce, 0x04, 0x19, 0x83);
DEFINE_GUID(GST_GUID_MF_SA_D3D11_USAGE,
    0xe85fe442, 0x2ca3, 0x486e, 0xa9, 0xc7, 0x10, 0x9d, 0xda, 0x60, 0x98, 0x80);
DEFINE_GUID(GST_GUID_MF_SA_D3D11_SHARED_WITHOUT_MUTEX,
    0x39dbd44d, 0x2e44, 0x4931, 0xa4, 0xc8, 0x35, 0x2d, 0x3d, 0xc4, 0x21, 0x15);
DEFINE_GUID(GST_GUID_MF_SA_D3D11_BINDFLAGS,
    0xeacf97ad, 0x065c, 0x4408, 0xbe, 0xe3, 0xfd, 0xcb, 0xfd, 0x12, 0x8b, 0xe2);

typedef struct _GstMFTransformEnumParams
{
  GUID category;
  guint32 enum_flags;
  MFT_REGISTER_TYPE_INFO *input_typeinfo;
  MFT_REGISTER_TYPE_INFO *output_typeinfo;

  guint device_index;
  gint64 adapter_luid;
} GstMFTransformEnumParams;

typedef HRESULT (*GstMFTransformNewSampleCallback) (GstMFTransform * object,
                                                    IMFSample * sample,
                                                    gpointer user_data);

GstMFTransform * gst_mf_transform_new             (GstMFTransformEnumParams * params);

gboolean        gst_mf_transform_open             (GstMFTransform * object);

gboolean        gst_mf_transform_set_device_manager (GstMFTransform * object,
                                                     IMFDXGIDeviceManager * manager);

void            gst_mf_transform_set_new_sample_callback (GstMFTransform * object,
                                                          GstMFTransformNewSampleCallback callback,
                                                          gpointer user_data);

IMFActivate *   gst_mf_transform_get_activate_handle (GstMFTransform * object);

IMFTransform *  gst_mf_transform_get_transform_handle (GstMFTransform * object);

ICodecAPI *     gst_mf_transform_get_codec_api_handle (GstMFTransform * object);

gboolean        gst_mf_transform_process_input    (GstMFTransform * object,
                                                   IMFSample * sample);

GstFlowReturn   gst_mf_transform_get_output       (GstMFTransform * object,
                                                   IMFSample ** sample);

gboolean        gst_mf_transform_flush            (GstMFTransform * object);

gboolean        gst_mf_transform_drain            (GstMFTransform * object);

gboolean        gst_mf_transform_get_input_available_types  (GstMFTransform * object,
                                                             GList ** input_types);

gboolean        gst_mf_transform_get_output_available_types (GstMFTransform * object,
                                                             GList ** output_types);

gboolean        gst_mf_transform_set_input_type  (GstMFTransform * object,
                                                  IMFMediaType * input_type);

gboolean        gst_mf_transform_set_output_type (GstMFTransform * object,
                                                  IMFMediaType * output_type);

gboolean        gst_mf_transform_get_input_current_type  (GstMFTransform * object,
                                                          IMFMediaType ** input_type);

gboolean        gst_mf_transform_get_output_current_type (GstMFTransform * object,
                                                          IMFMediaType ** output_type);

gboolean        gst_mf_transform_set_codec_api_uint32  (GstMFTransform * object,
                                                        const GUID * api,
                                                        guint32 value);

gboolean        gst_mf_transform_set_codec_api_uint64  (GstMFTransform * object,
                                                        const GUID * api,
                                                        guint64 value);

gboolean        gst_mf_transform_set_codec_api_boolean (GstMFTransform * object,
                                                        const GUID * api,
                                                        gboolean value);

G_END_DECLS

#endif /* __GST_MF_TRANSFORM_OBJECT_H__ */