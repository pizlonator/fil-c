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

#pragma once

#include <components/Component.h>
#include <components/PreAnalysis.h>
#include <gst/gst.h>
#include <gst/video/video.h>
#include "gstamfutils.h"

G_BEGIN_DECLS

#define GST_TYPE_AMF_ENCODER            (gst_amf_encoder_get_type())
#define GST_AMF_ENCODER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_AMF_ENCODER, GstAmfEncoder))
#define GST_AMF_ENCODER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_AMF_ENCODER, GstAmfEncoderClass))
#define GST_IS_AMF_ENCODER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_AMF_ENCODER))
#define GST_IS_AMF_ENCODER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_AMF_ENCODER))
#define GST_AMF_ENCODER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GST_TYPE_AMF_ENCODER, GstAmfEncoderClass))
#define GST_AMF_ENCODER_CAST(obj)       ((GstAmfEncoder *)obj)

typedef struct _GstAmfEncoder GstAmfEncoder;
typedef struct _GstAmfEncoderClass GstAmfEncoderClass;
typedef struct _GstAmfEncoderPrivate GstAmfEncoderPrivate;

typedef struct _GstAmfEncoderPASupportedOptions {
  gboolean activity_type;
  gboolean scene_change_detection;
  gboolean scene_change_detection_sensitivity;
  gboolean static_scene_detection;
  gboolean static_scene_detection_sensitivity;
  gboolean initial_qp;
  gboolean max_qp;
  gboolean caq_strength;
  gboolean frame_sad;
  gboolean ltr;
  gboolean lookahead_buffer_depth;
  gboolean paq_mode;
  gboolean taq_mode;
  gboolean hmqb_mode;
} GstAmfEncoderPASupportedOptions;

typedef struct _GstAmfEncoderPreAnalysis
{
  gboolean pre_analysis;
  gint activity_type;
  gboolean scene_change_detection;
  gint scene_change_detection_sensitivity;
  gboolean static_scene_detection;
  gint static_scene_detection_sensitivity;
  guint initial_qp;
  guint max_qp;
  gint caq_strength;
  gboolean frame_sad;
  gboolean ltr;
  guint lookahead_buffer_depth;
  gint paq_mode;
  gint taq_mode;
  gint hmqb_mode;
} GstAmfEncoderPreAnalysis;

struct _GstAmfEncoder
{
  GstVideoEncoder parent;

  GstAmfEncoderPrivate *priv;
};

struct _GstAmfEncoderClass
{
  GstVideoEncoderClass parent_class;
  GstAmfEncoderPASupportedOptions pa_supported;
  gboolean    (*set_format)           (GstAmfEncoder * encoder,
                                       GstVideoCodecState * state,
                                       gpointer component,
                                       guint * num_reorder_frames);

  gboolean    (*set_output_state)     (GstAmfEncoder * encoder,
                                       GstVideoCodecState * state,
                                       gpointer component);

  gboolean    (*set_surface_prop)     (GstAmfEncoder * encoder,
                                       GstVideoCodecFrame * frame,
                                       gpointer surface);

  GstBuffer * (*create_output_buffer) (GstAmfEncoder * encoder,
                                       gpointer data,
                                       gboolean * sync_point);

  gboolean    (*check_reconfigure)    (GstAmfEncoder * encoder);
};

GType gst_amf_encoder_get_type (void);

void  gst_amf_encoder_set_subclass_data (GstAmfEncoder * encoder,
                                         gint64 adapter_luid,
                                         const wchar_t * codec_id);

AMF_RESULT gst_amf_encoder_set_pre_analysis_options (GstAmfEncoder * self,
                                                     amf::AMFComponent * comp,
                                                     const GstAmfEncoderPreAnalysis * pa,
                                                     GstAmfEncoderPASupportedOptions * pa_supported);

void gst_amf_encoder_check_pa_supported_options (GstAmfEncoderPASupportedOptions * pa_supported,
                                                 amf::AMFComponent * comp);

// Pre-analysis settings
#define DEFAULT_PRE_ANALYSIS FALSE
#define DEFAULT_PA_ACTIVITY_TYPE AMF_PA_ACTIVITY_Y
#define DEFAULT_PA_SCENE_CHANGE_DETECTION TRUE
#define DEFAULT_PA_SCENE_CHANGE_DETECTION_SENSITIVITY AMF_PA_SCENE_CHANGE_DETECTION_SENSITIVITY_MEDIUM
#define DEFAULT_PA_STATIC_SCENE_DETECTION FALSE
#define DEFAULT_PA_STATIC_SCENE_DETECTION_SENSITIVITY AMF_PA_STATIC_SCENE_DETECTION_SENSITIVITY_HIGH

#define DEFAULT_PA_INITIAL_QP 0
#define DEFAULT_PA_MAX_QP 35
#define DEFAULT_PA_CAQ_STRENGTH AMF_PA_CAQ_STRENGTH_MEDIUM
#define DEFAULT_PA_FRAME_SAD TRUE
#define DEFAULT_PA_LTR FALSE
#define DEFAULT_PA_LOOKAHEAD_BUFFER_DEPTH 0
#define DEFAULT_PA_PAQ_MODE AMF_PA_PAQ_MODE_NONE
#define DEFAULT_PA_TAQ_MODE AMF_PA_TAQ_MODE_NONE
#define DEFAULT_PA_HQMB_MODE AMF_PA_HIGH_MOTION_QUALITY_BOOST_MODE_NONE

#define GST_TYPE_AMF_ENC_PA_ACTIVITY_TYPE (gst_amf_enc_pa_activity_get_type ())
GType gst_amf_enc_pa_activity_get_type (void);

#define GST_TYPE_AMF_ENC_PA_SCENE_CHANGE_DETECTION_SENSITIVITY (gst_amf_enc_pa_scene_change_detection_sensitivity_get_type ())
GType gst_amf_enc_pa_scene_change_detection_sensitivity_get_type (void);

#define GST_TYPE_AMF_ENC_PA_STATIC_SCENE_DETECTION_SENSITIVITY (gst_amf_enc_pa_static_scene_detection_sensitivity_get_type ())
GType gst_amf_enc_pa_static_scene_detection_sensitivity_get_type (void);

#define GST_TYPE_AMF_ENC_PA_CAQ_STRENGTH (gst_amf_enc_pa_caq_strength_get_type ())
GType gst_amf_enc_pa_caq_strength_get_type (void);

#define GST_TYPE_AMF_ENC_PA_PAQ_MODE (gst_amf_enc_pa_paq_mode_get_type ())
GType gst_amf_enc_pa_paq_mode_get_type (void);

#define GST_TYPE_AMF_ENC_PA_TAQ_MODE (gst_amf_enc_pa_taq_mode_get_type ())
GType gst_amf_enc_pa_taq_mode_get_type (void);

#define GST_TYPE_AMF_ENC_PA_HQMB_MODE (gst_amf_enc_pa_hmbq_mode_get_type ())
GType gst_amf_enc_pa_hmbq_mode_get_type (void);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GstAmfEncoder, gst_object_unref)

G_END_DECLS
