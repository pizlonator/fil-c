/*
 * Copyright (C) 2016 Sebastian Dröge <sebastian@centricular.com>
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

#ifndef __GST_FDKAACENC_H__
#define __GST_FDKAACENC_H__

#include <gst/gst.h>
#include <gst/audio/gstaudioencoder.h>

#include <fdk-aac/aacenc_lib.h>

G_BEGIN_DECLS

#define GST_TYPE_FDKAACENC \
  (gst_fdkaacenc_get_type())
#define GST_FDKAACENC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_FDKAACENC, GstFdkAacEnc))
#define GST_FDKAACENC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_FDKAACENC, GstFdkAacEncClass))
#define GST_IS_FDKAACENC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_FDKAACENC))
#define GST_IS_FDKAACENC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_FDKAACENC))

typedef struct _GstFdkAacEnc GstFdkAacEnc;
typedef struct _GstFdkAacEncClass GstFdkAacEncClass;

/**
 * GstFdkAacVbrPreset:
 * @GST_FDK_AAC_VBR_PRESET_VERY_LOW: Very Low Variable Bitrate.
 * @GST_FDK_AAC_VBR_PRESET_LOW: Low Variable Bitrate.
 * @GST_FDK_AAC_VBR_PRESET_MEDIUM: Medium Variable Bitrate.
 * @GST_FDK_AAC_VBR_PRESET_HIGH: High Variable Bitrate.
 * @GST_FDK_AAC_VBR_PRESET_VERY_HIGH: Very High Variable Bitrate.
 *
 * Since: 1.22
 */
typedef enum
{
  GST_FDK_AAC_VBR_PRESET_VERY_LOW = 1,
  GST_FDK_AAC_VBR_PRESET_LOW,
  GST_FDK_AAC_VBR_PRESET_MEDIUM,
  GST_FDK_AAC_VBR_PRESET_HIGH,
  GST_FDK_AAC_VBR_PRESET_VERY_HIGH
} GstFdkAacVbrPreset;

/**
 * GstFdkAacRateControl:
 * @GST_FDK_AAC_RATE_CONTROL_CONSTANT_BITRATE: Constant Bitrate.
 * @GST_FDK_AAC_RATE_CONTROL_VARIABLE_BITRATE: Variable Bitrate.
 *
 * Since: 1.22
 */
typedef enum
{
  GST_FDK_AAC_RATE_CONTROL_CONSTANT_BITRATE = 0,
  GST_FDK_AAC_RATE_CONTROL_VARIABLE_BITRATE,
} GstFdkAacRateControl;

struct _GstFdkAacEnc {
  GstAudioEncoder element;

  HANDLE_AACENCODER enc;
  gint bitrate;

  guint outbuf_size, samples_per_frame;
  gboolean need_reorder;
  const GstAudioChannelPosition *aac_positions;
  gboolean is_drained;

  guint peak_bitrate;
  gboolean afterburner;

  GstFdkAacRateControl rate_control;
  GstFdkAacVbrPreset vbr_preset;
};

struct _GstFdkAacEncClass {
  GstAudioEncoderClass parent_class;
};

GType gst_fdkaacenc_get_type (void);

GST_ELEMENT_REGISTER_DECLARE (fdkaacenc);

G_END_DECLS

#endif /* __GST_FDKAACENC_H__ */
