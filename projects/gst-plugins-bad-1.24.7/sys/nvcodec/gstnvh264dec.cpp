/* GStreamer
 * Copyright (C) 2017 Ericsson AB. All rights reserved.
 * Copyright (C) 2019 Seungha Yang <seungha.yang@navercorp.com>
 * Copyright (C) 2020 Seungha Yang <seungha@centricular.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
 *
 * Copyright 2015 The Chromium Authors. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *    * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *    * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * SECTION:element-nvh264sldec
 * @title: nvh264sldec
 *
 * GstCodecs based NVIDIA H.264 video decoder
 *
 * ## Example launch line
 * ```
 * gst-launch-1.0 filesrc location=/path/to/h264/file ! parsebin ! nvh264sldec ! videoconvert ! autovideosink
 * ```
 *
 * Since: 1.18
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstnvh264dec.h"
#include "gstnvdecoder.h"

#include <string.h>

GST_DEBUG_CATEGORY_STATIC (gst_nv_h264_dec_debug);
#define GST_CAT_DEFAULT gst_nv_h264_dec_debug

typedef struct _GstNvH264Dec
{
  GstH264Decoder parent;

  GstNvDecoder *decoder;
  CUVIDPICPARAMS params;

  /* slice buffer which will be passed to CUVIDPICPARAMS::pBitstreamData */
  guint8 *bitstream_buffer;
  /* allocated memory size of bitstream_buffer */
  gsize bitstream_buffer_alloc_size;
  /* current offset of bitstream_buffer (per frame) */
  gsize bitstream_buffer_offset;

  guint *slice_offsets;
  guint slice_offsets_alloc_len;
  guint num_slices;

  guint width, height;
  guint coded_width, coded_height;
  guint bitdepth;
  guint chroma_format_idc;
  gint max_dpb_size;

  gboolean interlaced;

  GArray *ref_list;

  guint num_output_surfaces;
  guint init_max_width;
  guint init_max_height;
  gint max_display_delay;
} GstNvH264Dec;

typedef struct _GstNvH264DecClass
{
  GstH264DecoderClass parent_class;
  guint cuda_device_id;
  gint64 adapter_luid;
  guint max_width;
  guint max_height;
} GstNvH264DecClass;

enum
{
  PROP_0,
  PROP_CUDA_DEVICE_ID,
  PROP_NUM_OUTPUT_SURFACES,
  PROP_INIT_MAX_WIDTH,
  PROP_INIT_MAX_HEIGHT,
  PROP_MAX_DISPLAY_DELAY,
};

#define DEFAULT_NUM_OUTPUT_SURFACES 1
#define DEFAULT_MAX_DISPLAY_DELAY -1

static GTypeClass *parent_class = nullptr;

#define GST_NV_H264_DEC(object) ((GstNvH264Dec *) (object))
#define GST_NV_H264_DEC_GET_CLASS(object) \
    (G_TYPE_INSTANCE_GET_CLASS ((object),G_TYPE_FROM_INSTANCE (object),GstNvH264DecClass))

static void gst_nv_h264_decoder_finalize (GObject * object);
static void gst_nv_h264_dec_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_nv_h264_dec_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static void gst_nv_h264_dec_set_context (GstElement * element,
    GstContext * context);
static gboolean gst_nv_h264_dec_open (GstVideoDecoder * decoder);
static gboolean gst_nv_h264_dec_close (GstVideoDecoder * decoder);
static gboolean gst_nv_h264_dec_stop (GstVideoDecoder * decoder);
static gboolean gst_nv_h264_dec_negotiate (GstVideoDecoder * decoder);
static gboolean gst_nv_h264_dec_decide_allocation (GstVideoDecoder *
    decoder, GstQuery * query);
static gboolean gst_nv_h264_dec_sink_query (GstVideoDecoder * decoder,
    GstQuery * query);
static gboolean gst_nv_h264_dec_src_query (GstVideoDecoder * decoder,
    GstQuery * query);
static gboolean gst_nv_h264_dec_sink_event (GstVideoDecoder * decoder,
    GstEvent * event);

/* GstH264Decoder */
static GstFlowReturn gst_nv_h264_dec_new_sequence (GstH264Decoder * decoder,
    const GstH264SPS * sps, gint max_dpb_size);
static GstFlowReturn gst_nv_h264_dec_new_picture (GstH264Decoder * decoder,
    GstVideoCodecFrame * frame, GstH264Picture * picture);
static GstFlowReturn
gst_nv_h264_dec_new_field_picture (GstH264Decoder * decoder,
    GstH264Picture * first_field, GstH264Picture * second_field);
static GstFlowReturn gst_nv_h264_dec_output_picture (GstH264Decoder *
    decoder, GstVideoCodecFrame * frame, GstH264Picture * picture);
static GstFlowReturn gst_nv_h264_dec_start_picture (GstH264Decoder * decoder,
    GstH264Picture * picture, GstH264Slice * slice, GstH264Dpb * dpb);
static GstFlowReturn gst_nv_h264_dec_decode_slice (GstH264Decoder * decoder,
    GstH264Picture * picture, GstH264Slice * slice, GArray * ref_pic_list0,
    GArray * ref_pic_list1);
static GstFlowReturn gst_nv_h264_dec_end_picture (GstH264Decoder * decoder,
    GstH264Picture * picture);
static guint
gst_nv_h264_dec_get_preferred_output_delay (GstH264Decoder * decoder,
    gboolean live);

static void
gst_nv_h264_dec_class_init (GstNvH264DecClass * klass,
    GstNvDecoderClassData * cdata)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstVideoDecoderClass *decoder_class = GST_VIDEO_DECODER_CLASS (klass);
  GstH264DecoderClass *h264decoder_class = GST_H264_DECODER_CLASS (klass);

  object_class->finalize = gst_nv_h264_decoder_finalize;
  object_class->set_property = gst_nv_h264_dec_set_property;
  object_class->get_property = gst_nv_h264_dec_get_property;

  /**
   * GstNvH264Dec:cuda-device-id:
   *
   * Assigned CUDA device id
   *
   * Since: 1.22
   */
  g_object_class_install_property (object_class, PROP_CUDA_DEVICE_ID,
      g_param_spec_uint ("cuda-device-id", "CUDA device id",
          "Assigned CUDA device id", 0, G_MAXINT, 0,
          (GParamFlags) (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)));

  /**
   * GstNvH264Dec:num-output-surfaces:
   *
   * The number of output surfaces (0 = auto, 1 = always copy).
   * This property will be used to calculate
   * the CUVIDDECODECREATEINFO.ulNumOutputSurfaces parameter in case of
   * CUDA output mode.
   *
   * Since: 1.24
   */
  g_object_class_install_property (object_class, PROP_NUM_OUTPUT_SURFACES,
      g_param_spec_uint ("num-output-surfaces", "Num Output Surfaces",
          "Maximum number of output surfaces simultaneously mapped in CUDA "
          "output mode (0 = auto, 1 = always copy)",
          0, 64, DEFAULT_NUM_OUTPUT_SURFACES,
          (GParamFlags) (GST_PARAM_MUTABLE_READY | G_PARAM_READWRITE |
              G_PARAM_STATIC_STRINGS)));

  /**
   * GstNvH264Dec:init-max-width:
   *
   * Initial CUVIDDECODECREATEINFO.ulMaxWidth value
   *
   * Since: 1.24
   */
  g_object_class_install_property (object_class, PROP_INIT_MAX_WIDTH,
      g_param_spec_uint ("init-max-width", "Initial Maximum Width",
          "Expected maximum coded width of stream. This value is used to "
          "pre-allocate higher dimension of output surfaces than "
          "that of input stream, in order to help decoder reconfiguration",
          0, cdata->max_width, 0,
          (GParamFlags) (GST_PARAM_MUTABLE_READY | G_PARAM_READWRITE |
              G_PARAM_STATIC_STRINGS)));

  /**
   * GstNvH264Dec:init-max-height:
   *
   * Initial CUVIDDECODECREATEINFO.ulMaxHeight value
   *
   * Since: 1.24
   */
  g_object_class_install_property (object_class, PROP_INIT_MAX_HEIGHT,
      g_param_spec_uint ("init-max-height", "Initial Maximum Height",
          "Expected maximum coded height of stream. This value is used to "
          "pre-allocate higher dimension of output surfaces than "
          "that of input stream, in order to help decoder reconfiguration",
          0, cdata->max_height, 0,
          (GParamFlags) (GST_PARAM_MUTABLE_READY | G_PARAM_READWRITE |
              G_PARAM_STATIC_STRINGS)));

  /**
   * GstNvH264Dec:max-display-delay:
   *
   * Maximum display delay
   *
   * Since: 1.24
   */
  g_object_class_install_property (object_class, PROP_MAX_DISPLAY_DELAY,
      g_param_spec_int ("max-display-delay", "Max Display Delay",
          "Improves pipelining of decode with display, 0 means no delay "
          "(auto = -1)", -1, 16, DEFAULT_MAX_DISPLAY_DELAY,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  element_class->set_context = GST_DEBUG_FUNCPTR (gst_nv_h264_dec_set_context);

  parent_class = (GTypeClass *) g_type_class_peek_parent (klass);
  gst_element_class_set_static_metadata (element_class,
      "NVDEC H.264 Decoder",
      "Codec/Decoder/Video/Hardware",
      "NVIDIA H.264 video decoder", "Seungha Yang <seungha@centricular.com>");

  gst_element_class_add_pad_template (element_class,
      gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
          cdata->sink_caps));
  gst_element_class_add_pad_template (element_class,
      gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
          cdata->src_caps));

  decoder_class->open = GST_DEBUG_FUNCPTR (gst_nv_h264_dec_open);
  decoder_class->close = GST_DEBUG_FUNCPTR (gst_nv_h264_dec_close);
  decoder_class->stop = GST_DEBUG_FUNCPTR (gst_nv_h264_dec_stop);
  decoder_class->negotiate = GST_DEBUG_FUNCPTR (gst_nv_h264_dec_negotiate);
  decoder_class->decide_allocation =
      GST_DEBUG_FUNCPTR (gst_nv_h264_dec_decide_allocation);
  decoder_class->sink_query = GST_DEBUG_FUNCPTR (gst_nv_h264_dec_sink_query);
  decoder_class->src_query = GST_DEBUG_FUNCPTR (gst_nv_h264_dec_src_query);
  decoder_class->sink_event = GST_DEBUG_FUNCPTR (gst_nv_h264_dec_sink_event);

  h264decoder_class->new_sequence =
      GST_DEBUG_FUNCPTR (gst_nv_h264_dec_new_sequence);
  h264decoder_class->new_picture =
      GST_DEBUG_FUNCPTR (gst_nv_h264_dec_new_picture);
  h264decoder_class->new_field_picture =
      GST_DEBUG_FUNCPTR (gst_nv_h264_dec_new_field_picture);
  h264decoder_class->output_picture =
      GST_DEBUG_FUNCPTR (gst_nv_h264_dec_output_picture);
  h264decoder_class->start_picture =
      GST_DEBUG_FUNCPTR (gst_nv_h264_dec_start_picture);
  h264decoder_class->decode_slice =
      GST_DEBUG_FUNCPTR (gst_nv_h264_dec_decode_slice);
  h264decoder_class->end_picture =
      GST_DEBUG_FUNCPTR (gst_nv_h264_dec_end_picture);
  h264decoder_class->get_preferred_output_delay =
      GST_DEBUG_FUNCPTR (gst_nv_h264_dec_get_preferred_output_delay);

  klass->cuda_device_id = cdata->cuda_device_id;
  klass->adapter_luid = cdata->adapter_luid;
  klass->max_width = cdata->max_width;
  klass->max_height = cdata->max_height;

  gst_caps_unref (cdata->sink_caps);
  gst_caps_unref (cdata->src_caps);
  g_free (cdata);
}

static void
gst_nv_h264_dec_init (GstNvH264Dec * self)
{
  GstNvH264DecClass *klass = GST_NV_H264_DEC_GET_CLASS (self);

  self->decoder =
      gst_nv_decoder_new (klass->cuda_device_id, klass->adapter_luid);
  self->ref_list = g_array_sized_new (FALSE, TRUE,
      sizeof (GstH264Picture *), 16);
  g_array_set_clear_func (self->ref_list,
      (GDestroyNotify) gst_clear_h264_picture);

  self->num_output_surfaces = DEFAULT_NUM_OUTPUT_SURFACES;
  self->max_display_delay = DEFAULT_MAX_DISPLAY_DELAY;
}

static void
gst_nv_h264_decoder_finalize (GObject * object)
{
  GstNvH264Dec *self = GST_NV_H264_DEC (object);

  g_array_unref (self->ref_list);
  gst_object_unref (self->decoder);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_nv_h264_dec_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstNvH264Dec *self = GST_NV_H264_DEC (object);

  switch (prop_id) {
    case PROP_NUM_OUTPUT_SURFACES:
      self->num_output_surfaces = g_value_get_uint (value);
      break;
    case PROP_INIT_MAX_WIDTH:
      self->init_max_width = g_value_get_uint (value);
      break;
    case PROP_INIT_MAX_HEIGHT:
      self->init_max_height = g_value_get_uint (value);
      break;
    case PROP_MAX_DISPLAY_DELAY:
      self->max_display_delay = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_nv_h264_dec_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstNvH264Dec *self = GST_NV_H264_DEC (object);
  GstNvH264DecClass *klass = GST_NV_H264_DEC_GET_CLASS (object);

  switch (prop_id) {
    case PROP_CUDA_DEVICE_ID:
      g_value_set_uint (value, klass->cuda_device_id);
      break;
    case PROP_NUM_OUTPUT_SURFACES:
      g_value_set_uint (value, self->num_output_surfaces);
      break;
    case PROP_INIT_MAX_WIDTH:
      g_value_set_uint (value, self->init_max_width);
      break;
    case PROP_INIT_MAX_HEIGHT:
      g_value_set_uint (value, self->init_max_height);
      break;
    case PROP_MAX_DISPLAY_DELAY:
      g_value_set_int (value, self->max_display_delay);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_nv_h264_dec_set_context (GstElement * element, GstContext * context)
{
  GstNvH264Dec *self = GST_NV_H264_DEC (element);

  gst_nv_decoder_handle_set_context (self->decoder, element, context);

  GST_ELEMENT_CLASS (parent_class)->set_context (element, context);
}

/* Clear all codec specific (e.g., SPS) data */
static void
gst_d3d11_h264_dec_reset (GstNvH264Dec * self)
{
  self->width = 0;
  self->height = 0;
  self->coded_width = 0;
  self->coded_height = 0;
  self->bitdepth = 0;
  self->chroma_format_idc = 0;
  self->max_dpb_size = 0;
  self->interlaced = FALSE;
}

static gboolean
gst_nv_h264_dec_open (GstVideoDecoder * decoder)
{
  GstNvH264Dec *self = GST_NV_H264_DEC (decoder);

  gst_d3d11_h264_dec_reset (self);

  return gst_nv_decoder_open (self->decoder, GST_ELEMENT (decoder));
}

static gboolean
gst_nv_h264_dec_close (GstVideoDecoder * decoder)
{
  GstNvH264Dec *self = GST_NV_H264_DEC (decoder);

  g_clear_pointer (&self->bitstream_buffer, g_free);
  g_clear_pointer (&self->slice_offsets, g_free);

  self->bitstream_buffer_alloc_size = 0;
  self->slice_offsets_alloc_len = 0;

  return gst_nv_decoder_close (self->decoder);
}

static gboolean
gst_nv_h264_dec_stop (GstVideoDecoder * decoder)
{
  GstNvH264Dec *self = GST_NV_H264_DEC (decoder);
  gboolean ret;

  ret = GST_VIDEO_DECODER_CLASS (parent_class)->stop (decoder);

  gst_nv_decoder_reset (self->decoder);

  return ret;
}

static gboolean
gst_nv_h264_dec_negotiate (GstVideoDecoder * decoder)
{
  GstNvH264Dec *self = GST_NV_H264_DEC (decoder);
  GstH264Decoder *h264dec = GST_H264_DECODER (decoder);

  GST_DEBUG_OBJECT (self, "negotiate");

  if (!gst_nv_decoder_negotiate (self->decoder, decoder, h264dec->input_state))
    return FALSE;

  return GST_VIDEO_DECODER_CLASS (parent_class)->negotiate (decoder);
}

static gboolean
gst_nv_h264_dec_decide_allocation (GstVideoDecoder * decoder, GstQuery * query)
{
  GstNvH264Dec *self = GST_NV_H264_DEC (decoder);

  if (!gst_nv_decoder_decide_allocation (self->decoder, decoder, query)) {
    GST_WARNING_OBJECT (self, "Failed to handle decide allocation");
    return FALSE;
  }

  return GST_VIDEO_DECODER_CLASS (parent_class)->decide_allocation
      (decoder, query);
}

static gboolean
gst_nv_h264_dec_sink_query (GstVideoDecoder * decoder, GstQuery * query)
{
  GstNvH264Dec *self = GST_NV_H264_DEC (decoder);

  if (gst_nv_decoder_handle_query (self->decoder, GST_ELEMENT (decoder), query))
    return TRUE;

  return GST_VIDEO_DECODER_CLASS (parent_class)->sink_query (decoder, query);
}

static gboolean
gst_nv_h264_dec_src_query (GstVideoDecoder * decoder, GstQuery * query)
{
  GstNvH264Dec *self = GST_NV_H264_DEC (decoder);

  if (gst_nv_decoder_handle_query (self->decoder, GST_ELEMENT (decoder), query))
    return TRUE;

  return GST_VIDEO_DECODER_CLASS (parent_class)->src_query (decoder, query);
}

static gboolean
gst_nv_h264_dec_sink_event (GstVideoDecoder * decoder, GstEvent * event)
{
  GstNvH264Dec *self = GST_NV_H264_DEC (decoder);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_FLUSH_START:
      gst_nv_decoder_set_flushing (self->decoder, TRUE);
      break;
    case GST_EVENT_FLUSH_STOP:
      gst_nv_decoder_set_flushing (self->decoder, FALSE);
      break;
    default:
      break;
  }

  return GST_VIDEO_DECODER_CLASS (parent_class)->sink_event (decoder, event);
}

static GstFlowReturn
gst_nv_h264_dec_new_sequence (GstH264Decoder * decoder, const GstH264SPS * sps,
    gint max_dpb_size)
{
  GstNvH264Dec *self = GST_NV_H264_DEC (decoder);
  GstNvH264DecClass *klass = GST_NV_H264_DEC_GET_CLASS (self);
  guint crop_width, crop_height;
  gboolean modified = FALSE;
  gboolean interlaced;
  guint max_width, max_height;

  GST_LOG_OBJECT (self, "new sequence");

  if (sps->frame_cropping_flag) {
    crop_width = sps->crop_rect_width;
    crop_height = sps->crop_rect_height;
  } else {
    crop_width = sps->width;
    crop_height = sps->height;
  }

  if (self->width != crop_width || self->height != crop_height ||
      self->coded_width != (guint) sps->width ||
      self->coded_height != (guint) sps->height) {
    GST_INFO_OBJECT (self, "resolution changed %dx%d (%dx%d)",
        crop_width, crop_height, sps->width, sps->height);
    self->width = crop_width;
    self->height = crop_height;
    self->coded_width = sps->width;
    self->coded_height = sps->height;
    modified = TRUE;
  }

  if (self->bitdepth != (guint) sps->bit_depth_luma_minus8 + 8) {
    GST_INFO_OBJECT (self, "bitdepth changed");
    self->bitdepth = sps->bit_depth_luma_minus8 + 8;
    modified = TRUE;
  }

  if (self->chroma_format_idc != sps->chroma_format_idc) {
    GST_INFO_OBJECT (self, "chroma format changed");
    self->chroma_format_idc = sps->chroma_format_idc;
    modified = TRUE;
  }

  interlaced = !sps->frame_mbs_only_flag;
  if (self->interlaced != interlaced) {
    GST_INFO_OBJECT (self, "interlaced sequence changed");
    self->interlaced = interlaced;
    modified = TRUE;
  }

  if (self->max_dpb_size < max_dpb_size) {
    GST_INFO_OBJECT (self, "Requires larger DPB size (%d -> %d)",
        self->max_dpb_size, max_dpb_size);
    modified = TRUE;
  }

  if (modified || !gst_nv_decoder_is_configured (self->decoder)) {
    GstVideoInfo info;
    GstVideoFormat out_format = GST_VIDEO_FORMAT_UNKNOWN;

    if (self->bitdepth == 8) {
      if (self->chroma_format_idc == 1)
        out_format = GST_VIDEO_FORMAT_NV12;
      else {
        GST_FIXME_OBJECT (self, "Could not support 8bits non-4:2:0 format");
      }
    } else if (self->bitdepth == 10) {
      if (self->chroma_format_idc == 1)
        out_format = GST_VIDEO_FORMAT_P010_10LE;
      else {
        GST_FIXME_OBJECT (self, "Could not support 10bits non-4:2:0 format");
      }
    }

    if (out_format == GST_VIDEO_FORMAT_UNKNOWN) {
      GST_ERROR_OBJECT (self, "Could not support bitdepth/chroma format");
      return GST_FLOW_NOT_NEGOTIATED;
    }

    gst_video_info_set_format (&info, out_format, self->width, self->height);
    if (self->interlaced)
      GST_VIDEO_INFO_INTERLACE_MODE (&info) = GST_VIDEO_INTERLACE_MODE_MIXED;

    self->max_dpb_size = max_dpb_size;
    max_width = gst_nv_decoder_get_max_output_size (self->coded_width,
        self->init_max_width, klass->max_width);
    max_height = gst_nv_decoder_get_max_output_size (self->coded_height,
        self->init_max_height, klass->max_height);

    /* FIXME: add support cudaVideoCodec_H264_SVC and cudaVideoCodec_H264_MVC */
    if (!gst_nv_decoder_configure (self->decoder,
            cudaVideoCodec_H264, &info, self->coded_width, self->coded_height,
            self->bitdepth, max_dpb_size, FALSE, self->num_output_surfaces,
            max_width, max_height)) {
      GST_ERROR_OBJECT (self, "Failed to configure decoder");
      return GST_FLOW_NOT_NEGOTIATED;
    }

    if (!gst_video_decoder_negotiate (GST_VIDEO_DECODER (self))) {
      GST_ERROR_OBJECT (self, "Failed to negotiate with downstream");
      return GST_FLOW_NOT_NEGOTIATED;
    }

    memset (&self->params, 0, sizeof (CUVIDPICPARAMS));
  }

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_nv_h264_dec_new_picture (GstH264Decoder * decoder,
    GstVideoCodecFrame * frame, GstH264Picture * picture)
{
  GstNvH264Dec *self = GST_NV_H264_DEC (decoder);

  return gst_nv_decoder_new_picture (self->decoder,
      GST_CODEC_PICTURE (picture));
}

static GstFlowReturn
gst_nv_h264_dec_new_field_picture (GstH264Decoder * decoder,
    GstH264Picture * first_field, GstH264Picture * second_field)
{
  GstNvDecSurface *surface;

  surface = (GstNvDecSurface *)
      gst_h264_picture_get_user_data (first_field);
  if (!surface) {
    GST_ERROR_OBJECT (decoder,
        "No decoder frame in the first picture %p", first_field);
    return GST_FLOW_ERROR;
  }

  gst_h264_picture_set_user_data (second_field,
      gst_nv_dec_surface_ref (surface),
      (GDestroyNotify) gst_nv_dec_surface_unref);

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_nv_h264_dec_output_picture (GstH264Decoder * decoder,
    GstVideoCodecFrame * frame, GstH264Picture * picture)
{
  GstNvH264Dec *self = GST_NV_H264_DEC (decoder);

  return gst_nv_decoder_output_picture (self->decoder,
      GST_VIDEO_DECODER (decoder), frame, GST_CODEC_PICTURE (picture),
      picture->buffer_flags);
}

static GstNvDecSurface *
gst_nv_h264_dec_get_decoder_surface_from_picture (GstNvH264Dec * self,
    GstH264Picture * picture)
{
  GstNvDecSurface *surface;

  surface = (GstNvDecSurface *) gst_h264_picture_get_user_data (picture);
  if (!surface)
    GST_DEBUG_OBJECT (self, "current picture does not have decoder surface");

  return surface;
}

static void
gst_nv_h264_dec_fill_scaling_list_4x4 (const GstH264PPS * pps,
    CUVIDH264PICPARAMS * params)
{
  guint i;

  for (i = 0; i < G_N_ELEMENTS (params->WeightScale4x4); i++)
    gst_h264_quant_matrix_4x4_get_raster_from_zigzag (params->WeightScale4x4[i],
        pps->scaling_lists_4x4[i]);
}

static void
gst_nv_h264_dec_fill_scaling_list_8x8 (const GstH264PPS * pps,
    CUVIDH264PICPARAMS * params)
{
  guint i;

  for (i = 0; i < G_N_ELEMENTS (params->WeightScale8x8); i++) {
    gst_h264_quant_matrix_8x8_get_raster_from_zigzag (params->WeightScale8x8[i],
        pps->scaling_lists_8x8[i]);
  }
}

static void
gst_nv_h264_dec_picture_params_from_sps (GstNvH264Dec * self,
    const GstH264SPS * sps, gboolean field_pic, CUVIDH264PICPARAMS * params)
{
  params->residual_colour_transform_flag = sps->separate_colour_plane_flag;
  params->MbaffFrameFlag = sps->mb_adaptive_frame_field_flag && !field_pic;

#define COPY_FIELD(f) \
  (params)->f = (sps)->f

  COPY_FIELD (log2_max_frame_num_minus4);
  COPY_FIELD (pic_order_cnt_type);
  COPY_FIELD (log2_max_pic_order_cnt_lsb_minus4);
  COPY_FIELD (delta_pic_order_always_zero_flag);
  COPY_FIELD (frame_mbs_only_flag);
  COPY_FIELD (direct_8x8_inference_flag);
  COPY_FIELD (num_ref_frames);
  COPY_FIELD (bit_depth_luma_minus8);
  COPY_FIELD (bit_depth_chroma_minus8);
  COPY_FIELD (qpprime_y_zero_transform_bypass_flag);

#undef COPY_FIELD
}

static void
gst_nv_h264_dec_picture_params_from_pps (GstNvH264Dec * self,
    const GstH264PPS * pps, CUVIDH264PICPARAMS * params)
{
  params->second_chroma_qp_index_offset = pps->second_chroma_qp_index_offset;

#define COPY_FIELD(f) \
  (params)->f = (pps)->f

  COPY_FIELD (entropy_coding_mode_flag);
  COPY_FIELD (pic_order_present_flag);
  COPY_FIELD (num_ref_idx_l0_active_minus1);
  COPY_FIELD (num_ref_idx_l1_active_minus1);
  COPY_FIELD (pic_init_qp_minus26);
  COPY_FIELD (weighted_pred_flag);
  COPY_FIELD (weighted_bipred_idc);
  COPY_FIELD (pic_init_qp_minus26);
  COPY_FIELD (deblocking_filter_control_present_flag);
  COPY_FIELD (redundant_pic_cnt_present_flag);
  COPY_FIELD (transform_8x8_mode_flag);
  COPY_FIELD (constrained_intra_pred_flag);
  COPY_FIELD (chroma_qp_index_offset);
#undef COPY_FIELD

  gst_nv_h264_dec_fill_scaling_list_4x4 (pps, params);
  gst_nv_h264_dec_fill_scaling_list_8x8 (pps, params);
}

static void
gst_nv_h264_dec_reset_bitstream_params (GstNvH264Dec * self)
{
  self->bitstream_buffer_offset = 0;
  self->num_slices = 0;

  self->params.nBitstreamDataLen = 0;
  self->params.pBitstreamData = nullptr;
  self->params.nNumSlices = 0;
  self->params.pSliceDataOffsets = nullptr;
}

static void
gst_nv_h264_dec_fill_dpb (GstNvH264Dec * self, GstH264Picture * ref,
    CUVIDH264DPBENTRY * dpb)
{
  GstNvDecSurface *surface;

  dpb->not_existing = ref->nonexisting;
  dpb->PicIdx = -1;

  surface = gst_nv_h264_dec_get_decoder_surface_from_picture (self, ref);
  if (!surface) {
    dpb->not_existing = 1;
  } else if (!dpb->not_existing) {
    dpb->PicIdx = surface->index;
  }

  if (dpb->not_existing)
    return;

  if (GST_H264_PICTURE_IS_LONG_TERM_REF (ref)) {
    dpb->FrameIdx = ref->long_term_frame_idx;
    dpb->is_long_term = 1;
  } else {
    dpb->FrameIdx = ref->frame_num;
    dpb->is_long_term = 0;
  }

  switch (ref->field) {
    case GST_H264_PICTURE_FIELD_FRAME:
      dpb->FieldOrderCnt[0] = ref->top_field_order_cnt;
      dpb->FieldOrderCnt[1] = ref->bottom_field_order_cnt;
      dpb->used_for_reference = 0x3;
      break;
    case GST_H264_PICTURE_FIELD_TOP_FIELD:
      dpb->FieldOrderCnt[0] = ref->top_field_order_cnt;
      dpb->used_for_reference = 0x1;
      if (ref->other_field) {
        dpb->FieldOrderCnt[1] = ref->other_field->bottom_field_order_cnt;
        dpb->used_for_reference |= 0x2;
      } else {
        dpb->FieldOrderCnt[1] = 0;
      }
      break;
    case GST_H264_PICTURE_FIELD_BOTTOM_FIELD:
      dpb->FieldOrderCnt[1] = ref->bottom_field_order_cnt;
      dpb->used_for_reference = 0x2;
      if (ref->other_field) {
        dpb->FieldOrderCnt[0] = ref->other_field->bottom_field_order_cnt;
        dpb->used_for_reference |= 0x1;
      } else {
        dpb->FieldOrderCnt[0] = 0;
      }
      break;
    default:
      dpb->FieldOrderCnt[0] = 0;
      dpb->FieldOrderCnt[1] = 0;
      dpb->used_for_reference = 0;
      break;
  }
}

static GstFlowReturn
gst_nv_h264_dec_start_picture (GstH264Decoder * decoder,
    GstH264Picture * picture, GstH264Slice * slice, GstH264Dpb * dpb)
{
  GstNvH264Dec *self = GST_NV_H264_DEC (decoder);
  CUVIDPICPARAMS *params = &self->params;
  CUVIDH264PICPARAMS *h264_params = &params->CodecSpecific.h264;
  const GstH264SliceHdr *slice_header = &slice->header;
  const GstH264SPS *sps;
  const GstH264PPS *pps;
  GstNvDecSurface *surface;
  GArray *ref_list = self->ref_list;
  guint i, ref_frame_idx;

  g_return_val_if_fail (slice_header->pps != nullptr, GST_FLOW_ERROR);
  g_return_val_if_fail (slice_header->pps->sequence != nullptr, GST_FLOW_ERROR);

  surface = gst_nv_h264_dec_get_decoder_surface_from_picture (self, picture);
  if (!surface) {
    GST_ERROR_OBJECT (self,
        "Couldn't get decoder frame frame picture %p", picture);
    return GST_FLOW_ERROR;
  }

  gst_nv_h264_dec_reset_bitstream_params (self);

  sps = slice_header->pps->sequence;
  pps = slice_header->pps;

  /* FIXME: update sps/pps related params only when it's required */
  params->PicWidthInMbs = sps->pic_width_in_mbs_minus1 + 1;
  if (!sps->frame_mbs_only_flag) {
    params->FrameHeightInMbs = (sps->pic_height_in_map_units_minus1 + 1) << 1;
  } else {
    params->FrameHeightInMbs = sps->pic_height_in_map_units_minus1 + 1;
  }
  params->CurrPicIdx = surface->index;
  params->field_pic_flag = slice_header->field_pic_flag;
  params->bottom_field_flag =
      picture->field == GST_H264_PICTURE_FIELD_BOTTOM_FIELD;
  params->second_field = picture->second_field;

  if (picture->field == GST_H264_PICTURE_FIELD_TOP_FIELD) {
    h264_params->CurrFieldOrderCnt[0] = picture->top_field_order_cnt;
    h264_params->CurrFieldOrderCnt[1] = 0;
  } else if (picture->field == GST_H264_PICTURE_FIELD_BOTTOM_FIELD) {
    h264_params->CurrFieldOrderCnt[0] = 0;
    h264_params->CurrFieldOrderCnt[1] = picture->bottom_field_order_cnt;
  } else {
    h264_params->CurrFieldOrderCnt[0] = picture->top_field_order_cnt;
    h264_params->CurrFieldOrderCnt[1] = picture->bottom_field_order_cnt;
  }

  /* nBitstreamDataLen, pBitstreamData, nNumSlices and pSliceDataOffsets
   * will be set later */

  params->ref_pic_flag = GST_H264_PICTURE_IS_REF (picture);
  /* will be updated later, if any slices belong to this frame is not
   * intra slice */
  params->intra_pic_flag = 1;

  h264_params->frame_num = picture->frame_num;
  h264_params->ref_pic_flag = GST_H264_PICTURE_IS_REF (picture);

  gst_nv_h264_dec_picture_params_from_sps (self,
      sps, slice_header->field_pic_flag, h264_params);
  gst_nv_h264_dec_picture_params_from_pps (self, pps, h264_params);

  ref_frame_idx = 0;
  g_array_set_size (ref_list, 0);

  memset (&h264_params->dpb, 0, sizeof (h264_params->dpb));
  gst_h264_dpb_get_pictures_short_term_ref (dpb, FALSE, FALSE, ref_list);
  for (i = 0; ref_frame_idx < 16 && i < ref_list->len; i++) {
    GstH264Picture *other = g_array_index (ref_list, GstH264Picture *, i);
    gst_nv_h264_dec_fill_dpb (self, other, &h264_params->dpb[ref_frame_idx]);
    ref_frame_idx++;
  }
  g_array_set_size (ref_list, 0);

  gst_h264_dpb_get_pictures_long_term_ref (dpb, FALSE, ref_list);
  for (i = 0; ref_frame_idx < 16 && i < ref_list->len; i++) {
    GstH264Picture *other = g_array_index (ref_list, GstH264Picture *, i);
    gst_nv_h264_dec_fill_dpb (self, other, &h264_params->dpb[ref_frame_idx]);
    ref_frame_idx++;
  }
  g_array_set_size (ref_list, 0);

  for (i = ref_frame_idx; i < 16; i++)
    h264_params->dpb[i].PicIdx = -1;

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_nv_h264_dec_decode_slice (GstH264Decoder * decoder,
    GstH264Picture * picture, GstH264Slice * slice, GArray * ref_pic_list0,
    GArray * ref_pic_list1)
{
  GstNvH264Dec *self = GST_NV_H264_DEC (decoder);
  gsize new_size;

  GST_LOG_OBJECT (self, "Decode slice, nalu size %u", slice->nalu.size);

  if (self->slice_offsets_alloc_len < self->num_slices + 1) {
    self->slice_offsets_alloc_len = 2 * (self->num_slices + 1);

    self->slice_offsets = (guint *) g_realloc_n (self->slice_offsets,
        self->slice_offsets_alloc_len, sizeof (guint));
  }
  self->slice_offsets[self->num_slices] = self->bitstream_buffer_offset;
  GST_LOG_OBJECT (self, "Slice offset %u for slice %d",
      self->slice_offsets[self->num_slices], self->num_slices);

  self->num_slices++;

  new_size = self->bitstream_buffer_offset + slice->nalu.size + 3;
  if (self->bitstream_buffer_alloc_size < new_size) {
    self->bitstream_buffer_alloc_size = 2 * new_size;

    self->bitstream_buffer = (guint8 *) g_realloc (self->bitstream_buffer,
        self->bitstream_buffer_alloc_size);
  }

  self->bitstream_buffer[self->bitstream_buffer_offset] = 0;
  self->bitstream_buffer[self->bitstream_buffer_offset + 1] = 0;
  self->bitstream_buffer[self->bitstream_buffer_offset + 2] = 1;

  memcpy (self->bitstream_buffer + self->bitstream_buffer_offset + 3,
      slice->nalu.data + slice->nalu.offset, slice->nalu.size);
  self->bitstream_buffer_offset = new_size;

  if (!GST_H264_IS_I_SLICE (&slice->header) &&
      !GST_H264_IS_SI_SLICE (&slice->header))
    self->params.intra_pic_flag = 0;

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_nv_h264_dec_end_picture (GstH264Decoder * decoder, GstH264Picture * picture)
{
  GstNvH264Dec *self = GST_NV_H264_DEC (decoder);
  gboolean ret;
  CUVIDPICPARAMS *params = &self->params;

  params->nBitstreamDataLen = self->bitstream_buffer_offset;
  params->pBitstreamData = self->bitstream_buffer;
  params->nNumSlices = self->num_slices;
  params->pSliceDataOffsets = self->slice_offsets;

  GST_LOG_OBJECT (self, "End picture, bitstream len: %" G_GSIZE_FORMAT
      ", num slices %d", self->bitstream_buffer_offset, self->num_slices);

  ret = gst_nv_decoder_decode (self->decoder, &self->params);

  if (!ret) {
    GST_ERROR_OBJECT (self, "Failed to decode picture");
    return GST_FLOW_ERROR;
  }

  return GST_FLOW_OK;
}

static guint
gst_nv_h264_dec_get_preferred_output_delay (GstH264Decoder * decoder,
    gboolean live)
{
  GstNvH264Dec *self = GST_NV_H264_DEC (decoder);

  if (self->max_display_delay >= 0)
    return self->max_display_delay;

  /* Prefer to zero latency for live pipeline */
  if (live)
    return 0;

  return 2;
}

void
gst_nv_h264_dec_register (GstPlugin * plugin, guint device_id,
    gint64 adapter_luid, guint rank, GstCaps * sink_caps, GstCaps * src_caps)
{
  GType type;
  gchar *type_name;
  gchar *feature_name;
  GstNvDecoderClassData *cdata;
  gint index = 0;
  const GValue *value;
  GstStructure *s;
  GTypeInfo type_info = {
    sizeof (GstNvH264DecClass),
    nullptr,
    nullptr,
    (GClassInitFunc) gst_nv_h264_dec_class_init,
    nullptr,
    nullptr,
    sizeof (GstNvH264Dec),
    0,
    (GInstanceInitFunc) gst_nv_h264_dec_init,
  };

  GST_DEBUG_CATEGORY_INIT (gst_nv_h264_dec_debug, "nvh264dec", 0, "nvh264dec");

  cdata = g_new0 (GstNvDecoderClassData, 1);
  cdata->sink_caps = gst_caps_from_string ("video/x-h264, "
      "stream-format= (string) { avc, avc3, byte-stream }, "
      "alignment= (string) au, "
      "profile = (string) { high, main, constrained-high, constrained-baseline, baseline, progressive-high }, "
      "framerate = " GST_VIDEO_FPS_RANGE);

  s = gst_caps_get_structure (sink_caps, 0);
  value = gst_structure_get_value (s, "width");
  cdata->max_width = (guint) gst_value_get_int_range_max (value);
  gst_caps_set_value (cdata->sink_caps, "width", value);

  value = gst_structure_get_value (s, "height");
  cdata->max_height = (guint) gst_value_get_int_range_max (value);
  gst_caps_set_value (cdata->sink_caps, "height", value);

  GST_MINI_OBJECT_FLAG_SET (cdata->sink_caps,
      GST_MINI_OBJECT_FLAG_MAY_BE_LEAKED);
  cdata->src_caps = gst_caps_ref (src_caps);
  cdata->cuda_device_id = device_id;
  cdata->adapter_luid = adapter_luid;

  type_name = g_strdup ("GstNvH264Dec");
  feature_name = g_strdup ("nvh264dec");

  while (g_type_from_name (type_name)) {
    index++;
    g_free (type_name);
    g_free (feature_name);
    type_name = g_strdup_printf ("GstNvH264Device%dDec", index);
    feature_name = g_strdup_printf ("nvh264device%ddec", index);
  }

  type_info.class_data = cdata;

  type = g_type_register_static (GST_TYPE_H264_DECODER,
      type_name, &type_info, (GTypeFlags) 0);

  /* make lower rank than default device */
  if (rank > 0 && index > 0)
    rank--;

  if (!gst_element_register (plugin, feature_name, rank, type))
    GST_WARNING ("Failed to register plugin '%s'", type_name);

  g_free (type_name);
  g_free (feature_name);
}
