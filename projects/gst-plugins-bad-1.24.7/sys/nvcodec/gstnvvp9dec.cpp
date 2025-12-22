/* GStreamer
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

/**
 * SECTION:element-nvvp9sldec
 * @title: nvvp9sldec
 *
 * GstCodecs based NVIDIA VP9 video decoder
 *
 * ## Example launch line
 * ```
 * gst-launch-1.0 filesrc location=/path/to/vp9/file ! parsebin ! nvvp9sldec ! videoconvert ! autovideosink
 * ```
 *
 * Since: 1.20
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstnvvp9dec.h"
#include "gstnvdecoder.h"

#include <string.h>

GST_DEBUG_CATEGORY_STATIC (gst_nv_vp9_dec_debug);
#define GST_CAT_DEFAULT gst_nv_vp9_dec_debug

typedef struct _GstNvVp9Dec
{
  GstVp9Decoder parent;

  GstNvDecoder *decoder;
  CUVIDPICPARAMS params;

  guint width, height;
  GstVP9Profile profile;

  guint num_output_surfaces;
  guint init_max_width;
  guint init_max_height;
  gint max_display_delay;
} GstNvVp9Dec;

typedef struct _GstNvVp9DecClass
{
  GstVp9DecoderClass parent_class;
  guint cuda_device_id;
  gint64 adapter_luid;
  guint max_width;
  guint max_height;
} GstNvVp9DecClass;

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

#define GST_NV_VP9_DEC(object) ((GstNvVp9Dec *) (object))
#define GST_NV_VP9_DEC_GET_CLASS(object) \
    (G_TYPE_INSTANCE_GET_CLASS ((object),G_TYPE_FROM_INSTANCE (object),GstNvVp9DecClass))

static void gst_nv_vp9_dec_finalize (GObject * object);
static void gst_nv_vp9_dec_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_nv_vp9_dec_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static void gst_nv_vp9_dec_set_context (GstElement * element,
    GstContext * context);
static gboolean gst_nv_vp9_dec_open (GstVideoDecoder * decoder);
static gboolean gst_nv_vp9_dec_close (GstVideoDecoder * decoder);
static gboolean gst_nv_vp9_dec_stop (GstVideoDecoder * decoder);
static gboolean gst_nv_vp9_dec_negotiate (GstVideoDecoder * decoder);
static gboolean gst_nv_vp9_dec_decide_allocation (GstVideoDecoder *
    decoder, GstQuery * query);
static gboolean gst_nv_vp9_dec_sink_query (GstVideoDecoder * decoder,
    GstQuery * query);
static gboolean gst_nv_vp9_dec_src_query (GstVideoDecoder * decoder,
    GstQuery * query);
static gboolean gst_nv_vp9_dec_sink_event (GstVideoDecoder * decoder,
    GstEvent * event);

/* GstVp9Decoder */
static GstFlowReturn gst_nv_vp9_dec_new_sequence (GstVp9Decoder * decoder,
    const GstVp9FrameHeader * frame_hdr, gint max_dpb_size);
static GstFlowReturn gst_nv_vp9_dec_new_picture (GstVp9Decoder * decoder,
    GstVideoCodecFrame * frame, GstVp9Picture * picture);
static GstVp9Picture *gst_nv_vp9_dec_duplicate_picture (GstVp9Decoder *
    decoder, GstVideoCodecFrame * frame, GstVp9Picture * picture);
static GstFlowReturn gst_nv_vp9_dec_decode_picture (GstVp9Decoder * decoder,
    GstVp9Picture * picture, GstVp9Dpb * dpb);
static GstFlowReturn gst_nv_vp9_dec_output_picture (GstVp9Decoder *
    decoder, GstVideoCodecFrame * frame, GstVp9Picture * picture);
static guint gst_nv_vp9_dec_get_preferred_output_delay (GstVp9Decoder * decoder,
    gboolean is_live);

static void
gst_nv_vp9_dec_class_init (GstNvVp9DecClass * klass,
    GstNvDecoderClassData * cdata)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstVideoDecoderClass *decoder_class = GST_VIDEO_DECODER_CLASS (klass);
  GstVp9DecoderClass *vp9decoder_class = GST_VP9_DECODER_CLASS (klass);

  object_class->finalize = gst_nv_vp9_dec_finalize;
  object_class->set_property = gst_nv_vp9_dec_set_property;
  object_class->get_property = gst_nv_vp9_dec_get_property;

  /**
   * GstNvVp9Dec:cuda-device-id:
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
   * GstNvVp9Dec:num-output-surfaces:
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
   * GstNvVp9Dec:init-max-width:
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
   * GstNvVp9Dec:init-max-height:
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
   * GstNvVp9Dec:max-display-delay:
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

  element_class->set_context = GST_DEBUG_FUNCPTR (gst_nv_vp9_dec_set_context);

  parent_class = (GTypeClass *) g_type_class_peek_parent (klass);
  gst_element_class_set_metadata (element_class,
      "NVDEC VP9 Decoder",
      "Codec/Decoder/Video/Hardware",
      "NVIDIA VP9 video decoder", "Seungha Yang <seungha@centricular.com>");

  gst_element_class_add_pad_template (element_class,
      gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
          cdata->sink_caps));
  gst_element_class_add_pad_template (element_class,
      gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
          cdata->src_caps));

  decoder_class->open = GST_DEBUG_FUNCPTR (gst_nv_vp9_dec_open);
  decoder_class->close = GST_DEBUG_FUNCPTR (gst_nv_vp9_dec_close);
  decoder_class->stop = GST_DEBUG_FUNCPTR (gst_nv_vp9_dec_stop);
  decoder_class->negotiate = GST_DEBUG_FUNCPTR (gst_nv_vp9_dec_negotiate);
  decoder_class->decide_allocation =
      GST_DEBUG_FUNCPTR (gst_nv_vp9_dec_decide_allocation);
  decoder_class->sink_query = GST_DEBUG_FUNCPTR (gst_nv_vp9_dec_sink_query);
  decoder_class->src_query = GST_DEBUG_FUNCPTR (gst_nv_vp9_dec_src_query);
  decoder_class->sink_event = GST_DEBUG_FUNCPTR (gst_nv_vp9_dec_sink_event);

  vp9decoder_class->new_sequence =
      GST_DEBUG_FUNCPTR (gst_nv_vp9_dec_new_sequence);
  vp9decoder_class->new_picture =
      GST_DEBUG_FUNCPTR (gst_nv_vp9_dec_new_picture);
  vp9decoder_class->duplicate_picture =
      GST_DEBUG_FUNCPTR (gst_nv_vp9_dec_duplicate_picture);
  vp9decoder_class->decode_picture =
      GST_DEBUG_FUNCPTR (gst_nv_vp9_dec_decode_picture);
  vp9decoder_class->output_picture =
      GST_DEBUG_FUNCPTR (gst_nv_vp9_dec_output_picture);
  vp9decoder_class->get_preferred_output_delay =
      GST_DEBUG_FUNCPTR (gst_nv_vp9_dec_get_preferred_output_delay);

  klass->cuda_device_id = cdata->cuda_device_id;
  klass->adapter_luid = cdata->adapter_luid;
  klass->max_width = cdata->max_width;
  klass->max_height = cdata->max_height;

  gst_caps_unref (cdata->sink_caps);
  gst_caps_unref (cdata->src_caps);
  g_free (cdata);
}

static void
gst_nv_vp9_dec_init (GstNvVp9Dec * self)
{
  GstNvVp9DecClass *klass = GST_NV_VP9_DEC_GET_CLASS (self);

  self->decoder =
      gst_nv_decoder_new (klass->cuda_device_id, klass->adapter_luid);

  self->num_output_surfaces = DEFAULT_NUM_OUTPUT_SURFACES;
  self->max_display_delay = DEFAULT_MAX_DISPLAY_DELAY;
}

static void
gst_nv_vp9_dec_finalize (GObject * object)
{
  GstNvVp9Dec *self = GST_NV_VP9_DEC (object);

  gst_object_unref (self->decoder);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_nv_vp9_dec_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstNvVp9Dec *self = GST_NV_VP9_DEC (object);

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
gst_nv_vp9_dec_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstNvVp9Dec *self = GST_NV_VP9_DEC (object);
  GstNvVp9DecClass *klass = GST_NV_VP9_DEC_GET_CLASS (object);

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
gst_nv_vp9_dec_set_context (GstElement * element, GstContext * context)
{
  GstNvVp9Dec *self = GST_NV_VP9_DEC (element);

  gst_nv_decoder_handle_set_context (self->decoder, element, context);

  GST_ELEMENT_CLASS (parent_class)->set_context (element, context);
}

static gboolean
gst_nv_vp9_dec_open (GstVideoDecoder * decoder)
{
  GstVp9Decoder *vp9dec = GST_VP9_DECODER (decoder);
  GstNvVp9Dec *self = GST_NV_VP9_DEC (decoder);

  /* NVDEC doesn't support non-keyframe resolution change and it will result
   * in outputting broken frames */
  gst_vp9_decoder_set_non_keyframe_format_change_support (vp9dec, FALSE);

  return gst_nv_decoder_open (self->decoder, GST_ELEMENT (decoder));
}

static gboolean
gst_nv_vp9_dec_close (GstVideoDecoder * decoder)
{
  GstNvVp9Dec *self = GST_NV_VP9_DEC (decoder);

  return gst_nv_decoder_close (self->decoder);
}

static gboolean
gst_nv_vp9_dec_stop (GstVideoDecoder * decoder)
{
  GstNvVp9Dec *self = GST_NV_VP9_DEC (decoder);
  gboolean ret;

  ret = GST_VIDEO_DECODER_CLASS (parent_class)->stop (decoder);

  gst_nv_decoder_reset (self->decoder);

  return ret;
}

static gboolean
gst_nv_vp9_dec_negotiate (GstVideoDecoder * decoder)
{
  GstNvVp9Dec *self = GST_NV_VP9_DEC (decoder);
  GstVp9Decoder *vp9dec = GST_VP9_DECODER (decoder);

  GST_DEBUG_OBJECT (self, "negotiate");

  if (!gst_nv_decoder_negotiate (self->decoder, decoder, vp9dec->input_state))
    return FALSE;

  return GST_VIDEO_DECODER_CLASS (parent_class)->negotiate (decoder);
}

static gboolean
gst_nv_vp9_dec_decide_allocation (GstVideoDecoder * decoder, GstQuery * query)
{
  GstNvVp9Dec *self = GST_NV_VP9_DEC (decoder);

  if (!gst_nv_decoder_decide_allocation (self->decoder, decoder, query)) {
    GST_WARNING_OBJECT (self, "Failed to handle decide allocation");
    return FALSE;
  }

  return GST_VIDEO_DECODER_CLASS (parent_class)->decide_allocation
      (decoder, query);
}

static gboolean
gst_nv_vp9_dec_sink_query (GstVideoDecoder * decoder, GstQuery * query)
{
  GstNvVp9Dec *self = GST_NV_VP9_DEC (decoder);

  if (gst_nv_decoder_handle_query (self->decoder, GST_ELEMENT (decoder), query))
    return TRUE;

  return GST_VIDEO_DECODER_CLASS (parent_class)->sink_query (decoder, query);
}

static gboolean
gst_nv_vp9_dec_src_query (GstVideoDecoder * decoder, GstQuery * query)
{
  GstNvVp9Dec *self = GST_NV_VP9_DEC (decoder);

  if (gst_nv_decoder_handle_query (self->decoder, GST_ELEMENT (decoder), query))
    return TRUE;

  return GST_VIDEO_DECODER_CLASS (parent_class)->src_query (decoder, query);
}

static gboolean
gst_nv_vp9_dec_sink_event (GstVideoDecoder * decoder, GstEvent * event)
{
  GstNvVp9Dec *self = GST_NV_VP9_DEC (decoder);

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
gst_nv_vp9_dec_new_sequence (GstVp9Decoder * decoder,
    const GstVp9FrameHeader * frame_hdr, gint max_dpb_size)
{
  GstNvVp9Dec *self = GST_NV_VP9_DEC (decoder);
  GstNvVp9DecClass *klass = GST_NV_VP9_DEC_GET_CLASS (self);
  GstVideoFormat out_format = GST_VIDEO_FORMAT_UNKNOWN;
  GstVideoInfo info;
  guint max_width, max_height;

  GST_LOG_OBJECT (self, "new sequence");

  self->width = frame_hdr->width;
  self->height = frame_hdr->height;
  self->profile = (GstVP9Profile) frame_hdr->profile;

  if (self->profile == GST_VP9_PROFILE_0) {
    out_format = GST_VIDEO_FORMAT_NV12;
  } else if (self->profile == GST_VP9_PROFILE_2) {
    if (frame_hdr->bit_depth == 10) {
      out_format = GST_VIDEO_FORMAT_P010_10LE;
    } else {
      out_format = GST_VIDEO_FORMAT_P012_LE;
    }
  }

  if (out_format == GST_VIDEO_FORMAT_UNKNOWN) {
    GST_ERROR_OBJECT (self, "Could not support profile %d", self->profile);
    return GST_FLOW_NOT_NEGOTIATED;
  }

  gst_video_info_set_format (&info, out_format, self->width, self->height);

  max_width = gst_nv_decoder_get_max_output_size (self->width,
      self->init_max_width, klass->max_width);
  max_height = gst_nv_decoder_get_max_output_size (self->height,
      self->init_max_height, klass->max_height);

  if (!gst_nv_decoder_configure (self->decoder,
          cudaVideoCodec_VP9, &info, self->width, self->height,
          frame_hdr->bit_depth, max_dpb_size, FALSE,
          self->num_output_surfaces, max_width, max_height)) {
    GST_ERROR_OBJECT (self, "Failed to configure decoder");
    return GST_FLOW_NOT_NEGOTIATED;
  }

  if (!gst_video_decoder_negotiate (GST_VIDEO_DECODER (self))) {
    GST_ERROR_OBJECT (self, "Failed to negotiate with downstream");
    return GST_FLOW_NOT_NEGOTIATED;
  }

  memset (&self->params, 0, sizeof (CUVIDPICPARAMS));

  self->params.CodecSpecific.vp9.colorSpace = frame_hdr->color_space;

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_nv_vp9_dec_new_picture (GstVp9Decoder * decoder,
    GstVideoCodecFrame * frame, GstVp9Picture * picture)
{
  GstNvVp9Dec *self = GST_NV_VP9_DEC (decoder);

  return gst_nv_decoder_new_picture (self->decoder,
      GST_CODEC_PICTURE (picture));
}

static GstNvDecSurface *
gst_nv_vp9_dec_get_decoder_frame_from_picture (GstNvVp9Dec * self,
    GstVp9Picture * picture)
{
  GstNvDecSurface *surface;

  surface = (GstNvDecSurface *) gst_vp9_picture_get_user_data (picture);
  if (!surface)
    GST_DEBUG_OBJECT (self, "current picture does not have decoder surface");

  return surface;
}

static GstVp9Picture *
gst_nv_vp9_dec_duplicate_picture (GstVp9Decoder * decoder,
    GstVideoCodecFrame * frame, GstVp9Picture * picture)
{
  GstNvVp9Dec *self = GST_NV_VP9_DEC (decoder);
  GstNvDecSurface *surface;
  GstVp9Picture *new_picture;

  surface = gst_nv_vp9_dec_get_decoder_frame_from_picture (self, picture);

  if (!surface) {
    GST_ERROR_OBJECT (self, "Parent picture does not have decoder surface");
    return nullptr;
  }

  new_picture = gst_vp9_picture_new ();
  new_picture->frame_hdr = picture->frame_hdr;

  gst_vp9_picture_set_user_data (new_picture,
      gst_nv_dec_surface_ref (surface),
      (GDestroyNotify) gst_nv_dec_surface_unref);

  return new_picture;
}

static GstFlowReturn
gst_nv_vp9_dec_decode_picture (GstVp9Decoder * decoder,
    GstVp9Picture * picture, GstVp9Dpb * dpb)
{
  GstNvVp9Dec *self = GST_NV_VP9_DEC (decoder);
  const GstVp9FrameHeader *frame_hdr = &picture->frame_hdr;
  const GstVp9LoopFilterParams *lfp = &frame_hdr->loop_filter_params;
  const GstVp9SegmentationParams *sp = &frame_hdr->segmentation_params;
  const GstVp9QuantizationParams *qp = &frame_hdr->quantization_params;
  CUVIDPICPARAMS *params = &self->params;
  CUVIDVP9PICPARAMS *vp9_params = &params->CodecSpecific.vp9;
  GstNvDecSurface *surface;
  GstNvDecSurface *other_surface;
  guint offset = 0;
  guint8 ref_frame_map[GST_VP9_REF_FRAMES];
  gint i;

  G_STATIC_ASSERT (G_N_ELEMENTS (vp9_params->mbRefLfDelta) ==
      GST_VP9_MAX_REF_LF_DELTAS);
  G_STATIC_ASSERT (G_N_ELEMENTS (vp9_params->mbModeLfDelta) ==
      GST_VP9_MAX_MODE_LF_DELTAS);
  G_STATIC_ASSERT (G_N_ELEMENTS (vp9_params->mb_segment_tree_probs) ==
      GST_VP9_SEG_TREE_PROBS);
  G_STATIC_ASSERT (sizeof (vp9_params->mb_segment_tree_probs) ==
      sizeof (sp->segmentation_tree_probs));
  G_STATIC_ASSERT (G_N_ELEMENTS (vp9_params->segment_pred_probs) ==
      GST_VP9_PREDICTION_PROBS);
  G_STATIC_ASSERT (sizeof (vp9_params->segment_pred_probs) ==
      sizeof (sp->segmentation_pred_prob));
  G_STATIC_ASSERT (G_N_ELEMENTS (vp9_params->refFrameSignBias) ==
      GST_VP9_REFS_PER_FRAME + 1);
  G_STATIC_ASSERT (sizeof (vp9_params->refFrameSignBias) ==
      sizeof (frame_hdr->ref_frame_sign_bias));
  G_STATIC_ASSERT (G_N_ELEMENTS (vp9_params->activeRefIdx) ==
      GST_VP9_REFS_PER_FRAME);
  G_STATIC_ASSERT (G_N_ELEMENTS (vp9_params->segmentFeatureEnable) ==
      GST_VP9_MAX_SEGMENTS);
  G_STATIC_ASSERT (sizeof (vp9_params->segmentFeatureEnable) ==
      sizeof (sp->feature_enabled));
  G_STATIC_ASSERT (G_N_ELEMENTS (vp9_params->segmentFeatureData) ==
      GST_VP9_MAX_SEGMENTS);
  G_STATIC_ASSERT (sizeof (vp9_params->segmentFeatureData) ==
      sizeof (sp->feature_data));

  GST_LOG_OBJECT (self, "Decode picture, size %" G_GSIZE_FORMAT, picture->size);

  surface = gst_nv_vp9_dec_get_decoder_frame_from_picture (self, picture);
  if (!surface) {
    GST_ERROR_OBJECT (self, "Decoder frame is unavailable");
    return GST_FLOW_ERROR;
  }

  params->nBitstreamDataLen = picture->size;
  params->pBitstreamData = picture->data;
  params->nNumSlices = 1;
  params->pSliceDataOffsets = &offset;

  params->PicWidthInMbs = GST_ROUND_UP_16 (frame_hdr->width) >> 4;
  params->FrameHeightInMbs = GST_ROUND_UP_16 (frame_hdr->height) >> 4;
  params->CurrPicIdx = surface->index;

  vp9_params->width = frame_hdr->width;
  vp9_params->height = frame_hdr->height;

  for (i = 0; i < GST_VP9_REF_FRAMES; i++) {
    if (dpb->pic_list[i]) {
      other_surface = gst_nv_vp9_dec_get_decoder_frame_from_picture (self,
          dpb->pic_list[i]);
      if (!other_surface) {
        GST_ERROR_OBJECT (self, "Couldn't get decoder frame from picture");
        return GST_FLOW_ERROR;
      }

      ref_frame_map[i] = other_surface->index;
    } else {
      ref_frame_map[i] = 0xff;
    }
  }

  vp9_params->LastRefIdx = ref_frame_map[frame_hdr->ref_frame_idx[0]];
  vp9_params->GoldenRefIdx = ref_frame_map[frame_hdr->ref_frame_idx[1]];
  vp9_params->AltRefIdx = ref_frame_map[frame_hdr->ref_frame_idx[2]];

  vp9_params->profile = frame_hdr->profile;
  vp9_params->frameContextIdx = frame_hdr->frame_context_idx;
  vp9_params->frameType = frame_hdr->frame_type;
  vp9_params->showFrame = frame_hdr->show_frame;
  vp9_params->errorResilient = frame_hdr->error_resilient_mode;
  vp9_params->frameParallelDecoding = frame_hdr->frame_parallel_decoding_mode;
  vp9_params->subSamplingX = frame_hdr->subsampling_x;
  vp9_params->subSamplingY = frame_hdr->subsampling_y;
  vp9_params->intraOnly = frame_hdr->intra_only;
  vp9_params->allow_high_precision_mv = frame_hdr->allow_high_precision_mv;
  vp9_params->refreshEntropyProbs = frame_hdr->refresh_frame_context;
  vp9_params->bitDepthMinus8Luma = frame_hdr->bit_depth - 8;
  vp9_params->bitDepthMinus8Chroma = frame_hdr->bit_depth - 8;

  vp9_params->loopFilterLevel = lfp->loop_filter_level;
  vp9_params->loopFilterSharpness = lfp->loop_filter_sharpness;
  vp9_params->modeRefLfEnabled = lfp->loop_filter_delta_enabled;

  vp9_params->log2_tile_columns = frame_hdr->tile_cols_log2;
  vp9_params->log2_tile_rows = frame_hdr->tile_rows_log2;

  vp9_params->segmentEnabled = sp->segmentation_enabled;
  vp9_params->segmentMapUpdate = sp->segmentation_update_map;
  vp9_params->segmentMapTemporalUpdate = sp->segmentation_temporal_update;
  vp9_params->segmentFeatureMode = sp->segmentation_abs_or_delta_update;

  vp9_params->qpYAc = qp->base_q_idx;
  vp9_params->qpYDc = qp->delta_q_y_dc;
  vp9_params->qpChDc = qp->delta_q_uv_dc;
  vp9_params->qpChAc = qp->delta_q_uv_ac;

  vp9_params->resetFrameContext = frame_hdr->reset_frame_context;
  vp9_params->mcomp_filter_type = frame_hdr->interpolation_filter;
  vp9_params->frameTagSize = frame_hdr->frame_header_length_in_bytes;
  vp9_params->offsetToDctParts = frame_hdr->header_size_in_bytes;

  for (i = 0; i < GST_VP9_MAX_REF_LF_DELTAS; i++)
    vp9_params->mbRefLfDelta[i] = lfp->loop_filter_ref_deltas[i];

  for (i = 0; i < GST_VP9_MAX_MODE_LF_DELTAS; i++)
    vp9_params->mbModeLfDelta[i] = lfp->loop_filter_mode_deltas[i];

  memcpy (vp9_params->mb_segment_tree_probs, sp->segmentation_tree_probs,
      sizeof (sp->segmentation_tree_probs));
  memcpy (vp9_params->segment_pred_probs, sp->segmentation_pred_prob,
      sizeof (sp->segmentation_pred_prob));
  memcpy (vp9_params->refFrameSignBias, frame_hdr->ref_frame_sign_bias,
      sizeof (frame_hdr->ref_frame_sign_bias));

  for (i = 0; i < GST_VP9_REFS_PER_FRAME; i++) {
    vp9_params->activeRefIdx[i] = frame_hdr->ref_frame_idx[i];
  }

  memcpy (vp9_params->segmentFeatureEnable, sp->feature_enabled,
      sizeof (sp->feature_enabled));
  memcpy (vp9_params->segmentFeatureData, sp->feature_data,
      sizeof (sp->feature_data));

  if (!gst_nv_decoder_decode (self->decoder, &self->params))
    return GST_FLOW_ERROR;

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_nv_vp9_dec_output_picture (GstVp9Decoder * decoder,
    GstVideoCodecFrame * frame, GstVp9Picture * picture)
{
  GstNvVp9Dec *self = GST_NV_VP9_DEC (decoder);

  return gst_nv_decoder_output_picture (self->decoder,
      GST_VIDEO_DECODER (decoder), frame, GST_CODEC_PICTURE (picture), 0);
}

static guint
gst_nv_vp9_dec_get_preferred_output_delay (GstVp9Decoder * decoder,
    gboolean is_live)
{
  GstNvVp9Dec *self = GST_NV_VP9_DEC (decoder);

  if (self->max_display_delay >= 0)
    return self->max_display_delay;

  /* Prefer to zero latency for live pipeline */
  if (is_live)
    return 0;

  return 2;
}

void
gst_nv_vp9_dec_register (GstPlugin * plugin, guint device_id,
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
    sizeof (GstNvVp9DecClass),
    nullptr,
    nullptr,
    (GClassInitFunc) gst_nv_vp9_dec_class_init,
    nullptr,
    nullptr,
    sizeof (GstNvVp9Dec),
    0,
    (GInstanceInitFunc) gst_nv_vp9_dec_init,
  };

  GST_DEBUG_CATEGORY_INIT (gst_nv_vp9_dec_debug, "nvvp9dec", 0, "nvvp9dec");

  cdata = g_new0 (GstNvDecoderClassData, 1);

  s = gst_caps_get_structure (sink_caps, 0);
  value = gst_structure_get_value (s, "width");
  cdata->max_width = (guint) gst_value_get_int_range_max (value);

  value = gst_structure_get_value (s, "height");
  cdata->max_height = (guint) gst_value_get_int_range_max (value);

  cdata->sink_caps = gst_caps_copy (sink_caps);
  gst_caps_set_simple (cdata->sink_caps,
      "alignment", G_TYPE_STRING, "frame", nullptr);
  GST_MINI_OBJECT_FLAG_SET (cdata->sink_caps,
      GST_MINI_OBJECT_FLAG_MAY_BE_LEAKED);
  cdata->src_caps = gst_caps_ref (src_caps);
  cdata->cuda_device_id = device_id;
  cdata->adapter_luid = adapter_luid;

  type_name = g_strdup ("GstNvVp9Dec");
  feature_name = g_strdup ("nvvp9dec");

  while (g_type_from_name (type_name)) {
    index++;
    g_free (type_name);
    g_free (feature_name);
    type_name = g_strdup_printf ("GstNvVp9Device%dDec", index);
    feature_name = g_strdup_printf ("nvvp9device%ddec", index);
  }

  type_info.class_data = cdata;

  type = g_type_register_static (GST_TYPE_VP9_DECODER,
      type_name, &type_info, (GTypeFlags) 0);

  /* make lower rank than default device */
  if (rank > 0 && index > 0)
    rank--;

  if (!gst_element_register (plugin, feature_name, rank, type))
    GST_WARNING ("Failed to register plugin '%s'", type_name);

  g_free (type_name);
  g_free (feature_name);
}
