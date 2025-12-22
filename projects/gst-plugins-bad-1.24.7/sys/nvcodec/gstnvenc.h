/* GStreamer NVENC plugin
 * Copyright (C) 2015 Centricular Ltd
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

#ifndef __GST_NVENC_H_INCLUDED__
#define __GST_NVENC_H_INCLUDED__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/cuda/gstcuda.h>

#include "nvEncodeAPI.h"

G_BEGIN_DECLS

gboolean                gst_nvenc_cmp_guid (GUID g1, GUID g2);

NV_ENC_BUFFER_FORMAT    gst_nvenc_get_nv_buffer_format (GstVideoFormat fmt);

gboolean                gst_nvenc_get_supported_input_formats (gpointer encoder,
                                                               GUID codec_id,
                                                               GValue ** formats);

GValue *                gst_nvenc_get_interlace_modes (gpointer enc,
                                                       GUID codec_id);

GValue *                gst_nvenc_get_supported_codec_profiles (gpointer enc,
                                                                GUID codec_id);

void                    gst_nvenc_plugin_init (GstPlugin * plugin,
                                               guint device_index,
                                               CUcontext cuda_ctx);

guint32                 gst_nvenc_get_api_version (void);

guint32                 gst_nvenc_get_caps_param_version (void);

guint32                 gst_nvenc_get_encode_out_params_version (void);

guint32                 gst_nvenc_get_create_input_buffer_version (void);

guint32                 gst_nvenc_get_create_bitstream_buffer_version (void);

guint32                 gst_nvenc_get_create_mv_buffer_version (void);

guint32                 gst_nvenc_get_rc_params_version (void);

guint32                 gst_nvenc_get_config_version (void);

guint32                 gst_nvenc_get_initialize_params_version (void);

guint32                 gst_nvenc_get_reconfigure_params_version (void);

guint32                 gst_nvenc_get_preset_config_version (void);

guint32                 gst_nvenc_get_pic_params_version (void);

guint32                 gst_nvenc_get_meonly_params_version (void);

guint32                 gst_nvenc_get_lock_bitstream_version (void);

guint32                 gst_nvenc_get_lock_input_buffer_version (void);

guint32                 gst_nvenc_get_map_input_resource_version (void);

guint32                 gst_nvenc_get_register_resource_version (void);

guint32                 gst_nvenc_get_stat_version (void);

guint32                 gst_nvenc_get_sequence_param_payload_version (void);

guint32                 gst_nvenc_get_event_params_version (void);

guint32                 gst_nvenc_get_open_encode_session_ex_params_version (void);

gboolean                gst_nvenc_have_set_io_cuda_streams (void);

gboolean                gst_nvenc_load_library (guint * api_major_ver,
                                                guint * api_minor_ver);

G_END_DECLS

#endif /* __GST_NVENC_H_INCLUDED__ */
