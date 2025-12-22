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
 * SECTION:element-nvvp8sldec
 * @title: nvvp8sldec
 *
 * GstCodecs based NVIDIA VP8 video decoder
 *
 * ## Example launch line
 * ```
 * gst-launch-1.0 filesrc location=/path/to/vp8/file ! parsebin ! nvvp8sldec ! videoconvert ! autovideosink
 * ```
 *
 * Since: 1.20
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstnvvp8dec.h"
#include "gstnvdecoder.h"

#include <string.h>

GST_DEBUG_CATEGORY_STATIC (gst_nv_vp8_dec_debug);
#define GST_CAT_DEFAULT gst_nv_vp8_dec_debug

typedef struct _GstNvVp8Dec
{
  GstVp8Decoder parent;

  GstNvDecoder *decoder;
  CUVIDPICPARAMS params;

  guint width, height;

  guint num_output_surfaces;
  guint init_max_width;
  guint init_max_height;
  gint max_display_delay;
} GstNvVp8Dec;

typedef struct _GstNvVp8DecClass
{
  GstVp8DecoderClass parent_class;
  guint cuda_device_id;
  gint64 adapter_luid;
  guint max_width;
  guint max_height;
} GstNvVp8DecClass;

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

#define GST_NV_VP8_DEC(object) ((GstNvVp8Dec *) (object))
#define GST_NV_VP8_DEC_GET_CLASS(object) \
    (G_TYPE_INSTANCE_GET_CLASS ((object),G_TYPE_FROM_INSTANCE (object),GstNvVp8DecClass))

static void gst_nv_vp8_dec_finalize (GObject * object);
static void gst_nv_vp8_dec_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_nv_vp8_dec_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static void gst_nv_vp8_dec_set_context (GstElement * element,
    GstContext * context);
static gboolean gst_nv_vp8_dec_open (GstVideoDecoder * decoder);
static gboolean gst_nv_vp8_dec_close (GstVideoDecoder * decoder);
static gboolean gst_nv_vp8_dec_stop (GstVideoDecoder * decoder);
static gboolean gst_nv_vp8_dec_negotiate (GstVideoDecoder * decoder);
static gboolean gst_nv_vp8_dec_decide_allocation (GstVideoDecoder *
    decoder, GstQuery * query);
static gboolean gst_nv_vp8_dec_sink_query (GstVideoDecoder * decoder,
    GstQuery * query);
static gboolean gst_nv_vp8_dec_src_query (GstVideoDecoder * decoder,
    GstQuery * query);
static gboolean gst_nv_vp8_dec_sink_event (GstVideoDecoder * decoder,
    GstEvent * event);

/* GstVp8Decoder */
static GstFlowReturn gst_nv_vp8_dec_new_sequence (GstVp8Decoder * decoder,
    const GstVp8FrameHdr * frame_hdr, gint max_dpb_size);
static GstFlowReturn gst_nv_vp8_dec_new_picture (GstVp8Decoder * decoder,
    GstVideoCodecFrame * frame, GstVp8Picture * picture);
static GstFlowReturn gst_nv_vp8_dec_decode_picture (GstVp8Decoder * decoder,
    GstVp8Picture * picture, GstVp8Parser * parser);
static GstFlowReturn gst_nv_vp8_dec_output_picture (GstVp8Decoder *
    decoder, GstVideoCodecFrame * frame, GstVp8Picture * picture);
static guint gst_nv_vp8_dec_get_preferred_output_delay (GstVp8Decoder * decoder,
    gboolean is_live);

static void
gst_nv_vp8_dec_class_init (GstNvVp8DecClass * klass,
    GstNvDecoderClassData * cdata)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstVideoDecoderClass *decoder_class = GST_VIDEO_DECODER_CLASS (klass);
  GstVp8DecoderClass *vp8decoder_class = GST_VP8_DECODER_CLASS (klass);

  object_class->finalize = gst_nv_vp8_dec_finalize;
  object_class->set_property = gst_nv_vp8_dec_set_property;
  object_class->get_property = gst_nv_vp8_dec_get_property;

  /**
   * GstNvVp8Dec:cuda-device-id:
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
   * GstNvVp8Dec:num-output-surfaces:
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
   * GstNvVp8Dec:init-max-width:
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
   * GstNvVp8Dec:init-max-height:
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
   * GstNvVp8Dec:max-display-delay:
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

  element_class->set_context = GST_DEBUG_FUNCPTR (gst_nv_vp8_dec_set_context);

  parent_class = (GTypeClass *) g_type_class_peek_parent (klass);
  gst_element_class_set_metadata (element_class,
      "NVDEC VP8 Decoder",
      "Codec/Decoder/Video/Hardware",
      "NVIDIA VP8 video decoder", "Seungha Yang <seungha@centricular.com>");

  gst_element_class_add_pad_template (element_class,
      gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
          cdata->sink_caps));
  gst_element_class_add_pad_template (element_class,
      gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
          cdata->src_caps));

  decoder_class->open = GST_DEBUG_FUNCPTR (gst_nv_vp8_dec_open);
  decoder_class->close = GST_DEBUG_FUNCPTR (gst_nv_vp8_dec_close);
  decoder_class->stop = GST_DEBUG_FUNCPTR (gst_nv_vp8_dec_stop);
  decoder_class->negotiate = GST_DEBUG_FUNCPTR (gst_nv_vp8_dec_negotiate);
  decoder_class->decide_allocation =
      GST_DEBUG_FUNCPTR (gst_nv_vp8_dec_decide_allocation);
  decoder_class->sink_query = GST_DEBUG_FUNCPTR (gst_nv_vp8_dec_sink_query);
  decoder_class->src_query = GST_DEBUG_FUNCPTR (gst_nv_vp8_dec_src_query);
  decoder_class->sink_event = GST_DEBUG_FUNCPTR (gst_nv_vp8_dec_sink_event);

  vp8decoder_class->new_sequence =
      GST_DEBUG_FUNCPTR (gst_nv_vp8_dec_new_sequence);
  vp8decoder_class->new_picture =
      GST_DEBUG_FUNCPTR (gst_nv_vp8_dec_new_picture);
  vp8decoder_class->decode_picture =
      GST_DEBUG_FUNCPTR (gst_nv_vp8_dec_decode_picture);
  vp8decoder_class->output_picture =
      GST_DEBUG_FUNCPTR (gst_nv_vp8_dec_output_picture);
  vp8decoder_class->get_preferred_output_delay =
      GST_DEBUG_FUNCPTR (gst_nv_vp8_dec_get_preferred_output_delay);

  klass->cuda_device_id = cdata->cuda_device_id;
  klass->adapter_luid = cdata->adapter_luid;
  klass->max_width = cdata->max_width;
  klass->max_height = cdata->max_height;

  gst_caps_unref (cdata->sink_caps);
  gst_caps_unref (cdata->src_caps);
  g_free (cdata);
}

static void
gst_nv_vp8_dec_init (GstNvVp8Dec * self)
{
  GstNvVp8DecClass *klass = GST_NV_VP8_DEC_GET_CLASS (self);

  self->decoder =
      gst_nv_decoder_new (klass->cuda_device_id, klass->adapter_luid);

  self->num_output_surfaces = DEFAULT_NUM_OUTPUT_SURFACES;
  self->max_display_delay = DEFAULT_MAX_DISPLAY_DELAY;
}

static void
gst_nv_vp8_dec_finalize (GObject * object)
{
  GstNvVp8Dec *self = GST_NV_VP8_DEC (object);

  gst_object_unref (self->decoder);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_nv_vp8_dec_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstNvVp8Dec *self = GST_NV_VP8_DEC (object);

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
gst_nv_vp8_dec_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstNvVp8Dec *self = GST_NV_VP8_DEC (object);
  GstNvVp8DecClass *klass = GST_NV_VP8_DEC_GET_CLASS (object);

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
gst_nv_vp8_dec_set_context (GstElement * element, GstContext * context)
{
  GstNvVp8Dec *self = GST_NV_VP8_DEC (element);

  gst_nv_decoder_handle_set_context (self->decoder, element, context);

  GST_ELEMENT_CLASS (parent_class)->set_context (element, context);
}

static gboolean
gst_nv_vp8_dec_open (GstVideoDecoder * decoder)
{
  GstNvVp8Dec *self = GST_NV_VP8_DEC (decoder);

  return gst_nv_decoder_open (self->decoder, GST_ELEMENT (decoder));
}

static gboolean
gst_nv_vp8_dec_close (GstVideoDecoder * decoder)
{
  GstNvVp8Dec *self = GST_NV_VP8_DEC (decoder);

  return gst_nv_decoder_close (self->decoder);
}

static gboolean
gst_nv_vp8_dec_stop (GstVideoDecoder * decoder)
{
  GstNvVp8Dec *self = GST_NV_VP8_DEC (decoder);
  gboolean ret;

  ret = GST_VIDEO_DECODER_CLASS (parent_class)->stop (decoder);

  gst_nv_decoder_reset (self->decoder);

  return ret;
}

static gboolean
gst_nv_vp8_dec_negotiate (GstVideoDecoder * decoder)
{
  GstNvVp8Dec *self = GST_NV_VP8_DEC (decoder);
  GstVp8Decoder *vp8dec = GST_VP8_DECODER (decoder);

  GST_DEBUG_OBJECT (self, "negotiate");

  if (!gst_nv_decoder_negotiate (self->decoder, decoder, vp8dec->input_state))
    return FALSE;

  return GST_VIDEO_DECODER_CLASS (parent_class)->negotiate (decoder);
}

static gboolean
gst_nv_vp8_dec_decide_allocation (GstVideoDecoder * decoder, GstQuery * query)
{
  GstNvVp8Dec *self = GST_NV_VP8_DEC (decoder);

  if (!gst_nv_decoder_decide_allocation (self->decoder, decoder, query)) {
    GST_WARNING_OBJECT (self, "Failed to handle decide allocation");
    return FALSE;
  }

  return GST_VIDEO_DECODER_CLASS (parent_class)->decide_allocation
      (decoder, query);
}

static gboolean
gst_nv_vp8_dec_sink_query (GstVideoDecoder * decoder, GstQuery * query)
{
  GstNvVp8Dec *self = GST_NV_VP8_DEC (decoder);

  if (gst_nv_decoder_handle_query (self->decoder, GST_ELEMENT (decoder), query))
    return TRUE;

  return GST_VIDEO_DECODER_CLASS (parent_class)->sink_query (decoder, query);
}

static gboolean
gst_nv_vp8_dec_src_query (GstVideoDecoder * decoder, GstQuery * query)
{
  GstNvVp8Dec *self = GST_NV_VP8_DEC (decoder);

  if (gst_nv_decoder_handle_query (self->decoder, GST_ELEMENT (decoder), query))
    return TRUE;

  return GST_VIDEO_DECODER_CLASS (parent_class)->src_query (decoder, query);
}

static gboolean
gst_nv_vp8_dec_sink_event (GstVideoDecoder * decoder, GstEvent * event)
{
  GstNvVp8Dec *self = GST_NV_VP8_DEC (decoder);

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
gst_nv_vp8_dec_new_sequence (GstVp8Decoder * decoder,
    const GstVp8FrameHdr * frame_hdr, gint max_dpb_size)
{
  GstNvVp8Dec *self = GST_NV_VP8_DEC (decoder);
  GstNvVp8DecClass *klass = GST_NV_VP8_DEC_GET_CLASS (self);
  gboolean modified = FALSE;
  guint max_width, max_height;

  GST_LOG_OBJECT (self, "new sequence");

  if (self->width != frame_hdr->width || self->height != frame_hdr->height) {
    if (self->decoder) {
      GST_INFO_OBJECT (self, "resolution changed %dx%d -> %dx%d",
          self->width, self->height, frame_hdr->width, frame_hdr->height);
    }

    self->width = frame_hdr->width;
    self->height = frame_hdr->height;

    modified = TRUE;
  }

  if (modified || !gst_nv_decoder_is_configured (self->decoder)) {
    GstVideoInfo info;

    gst_video_info_set_format (&info,
        GST_VIDEO_FORMAT_NV12, self->width, self->height);

    max_width = gst_nv_decoder_get_max_output_size (self->width,
        self->init_max_width, klass->max_width);
    max_height = gst_nv_decoder_get_max_output_size (self->height,
        self->init_max_height, klass->max_height);

    if (!gst_nv_decoder_configure (self->decoder,
            cudaVideoCodec_VP8, &info, self->width, self->height, 8,
            max_dpb_size, FALSE, self->num_output_surfaces, max_width,
            max_height)) {
      GST_ERROR_OBJECT (self, "Failed to configure decoder");
      return GST_FLOW_NOT_NEGOTIATED;
    }

    if (!gst_video_decoder_negotiate (GST_VIDEO_DECODER (self))) {
      GST_ERROR_OBJECT (self, "Failed to negotiate with downstream");
      return GST_FLOW_NOT_NEGOTIATED;
    }

    memset (&self->params, 0, sizeof (CUVIDPICPARAMS));

    self->params.PicWidthInMbs = GST_ROUND_UP_16 (self->width) >> 4;
    self->params.FrameHeightInMbs = GST_ROUND_UP_16 (self->height) >> 4;

    self->params.CodecSpecific.vp8.width = self->width;
    self->params.CodecSpecific.vp8.height = self->height;
  }

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_nv_vp8_dec_new_picture (GstVp8Decoder * decoder,
    GstVideoCodecFrame * frame, GstVp8Picture * picture)
{
  GstNvVp8Dec *self = GST_NV_VP8_DEC (decoder);

  return gst_nv_decoder_new_picture (self->decoder,
      GST_CODEC_PICTURE (picture));
}

static GstNvDecSurface *
gst_nv_vp8_dec_get_decoder_frame_from_picture (GstNvVp8Dec * self,
    GstVp8Picture * picture)
{
  GstNvDecSurface *surface;

  surface = (GstNvDecSurface *) gst_vp8_picture_get_user_data (picture);
  if (!surface)
    GST_DEBUG_OBJECT (self, "current picture does not have decoder surface");

  return surface;
}

static GstFlowReturn
gst_nv_vp8_dec_decode_picture (GstVp8Decoder * decoder,
    GstVp8Picture * picture, GstVp8Parser * parser)
{
  GstNvVp8Dec *self = GST_NV_VP8_DEC (decoder);
  GstVp8FrameHdr *frame_hdr = &picture->frame_hdr;
  GstNvDecSurface *surface;
  GstNvDecSurface *other_surface;
  guint offset = 0;

  GST_LOG_OBJECT (self, "Decode picture, size %" G_GSIZE_FORMAT, picture->size);

  surface = gst_nv_vp8_dec_get_decoder_frame_from_picture (self, picture);
  if (!surface) {
    GST_ERROR_OBJECT (self, "Decoder frame is unavailable");
    return GST_FLOW_ERROR;
  }

  self->params.nBitstreamDataLen = picture->size;
  self->params.pBitstreamData = picture->data;
  self->params.nNumSlices = 1;
  self->params.pSliceDataOffsets = &offset;

  self->params.CurrPicIdx = surface->index;

  self->params.CodecSpecific.vp8.first_partition_size =
      frame_hdr->first_part_size;

  if (decoder->alt_ref_picture) {
    other_surface =
        gst_nv_vp8_dec_get_decoder_frame_from_picture (self,
        decoder->alt_ref_picture);
    if (!other_surface) {
      GST_ERROR_OBJECT (self, "Couldn't get decoder frame for AltRef");
      return GST_FLOW_ERROR;
    }

    self->params.CodecSpecific.vp8.AltRefIdx = other_surface->index;
  } else {
    self->params.CodecSpecific.vp8.AltRefIdx = 0xff;
  }

  if (decoder->golden_ref_picture) {
    other_surface =
        gst_nv_vp8_dec_get_decoder_frame_from_picture (self,
        decoder->golden_ref_picture);
    if (!other_surface) {
      GST_ERROR_OBJECT (self, "Couldn't get decoder frame for GoldenRef");
      return GST_FLOW_ERROR;
    }

    self->params.CodecSpecific.vp8.GoldenRefIdx = other_surface->index;
  } else {
    self->params.CodecSpecific.vp8.GoldenRefIdx = 0xff;
  }

  if (decoder->last_picture) {
    other_surface =
        gst_nv_vp8_dec_get_decoder_frame_from_picture (self,
        decoder->last_picture);
    if (!other_surface) {
      GST_ERROR_OBJECT (self, "Couldn't get decoder frame for LastRef");
      return GST_FLOW_ERROR;
    }

    self->params.CodecSpecific.vp8.LastRefIdx = other_surface->index;
  } else {
    self->params.CodecSpecific.vp8.LastRefIdx = 0xff;
  }

  self->params.CodecSpecific.vp8.vp8_frame_tag.frame_type =
      frame_hdr->key_frame ? 0 : 1;
  self->params.CodecSpecific.vp8.vp8_frame_tag.version = frame_hdr->version;
  self->params.CodecSpecific.vp8.vp8_frame_tag.show_frame =
      frame_hdr->show_frame;
  self->params.CodecSpecific.vp8.vp8_frame_tag.update_mb_segmentation_data =
      parser->segmentation.segmentation_enabled ?
      parser->segmentation.update_segment_feature_data : 0;

  if (!gst_nv_decoder_decode (self->decoder, &self->params))
    return GST_FLOW_ERROR;

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_nv_vp8_dec_output_picture (GstVp8Decoder * decoder,
    GstVideoCodecFrame * frame, GstVp8Picture * picture)
{
  GstNvVp8Dec *self = GST_NV_VP8_DEC (decoder);

  return gst_nv_decoder_output_picture (self->decoder,
      GST_VIDEO_DECODER (decoder), frame, GST_CODEC_PICTURE (picture), 0);
}

static guint
gst_nv_vp8_dec_get_preferred_output_delay (GstVp8Decoder * decoder,
    gboolean is_live)
{
  GstNvVp8Dec *self = GST_NV_VP8_DEC (decoder);

  if (self->max_display_delay >= 0)
    return self->max_display_delay;

  /* Prefer to zero latency for live pipeline */
  if (is_live)
    return 0;

  return 2;
}

void
gst_nv_vp8_dec_register (GstPlugin * plugin, guint device_id,
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
    sizeof (GstNvVp8DecClass),
    nullptr,
    nullptr,
    (GClassInitFunc) gst_nv_vp8_dec_class_init,
    nullptr,
    nullptr,
    sizeof (GstNvVp8Dec),
    0,
    (GInstanceInitFunc) gst_nv_vp8_dec_init,
  };

  GST_DEBUG_CATEGORY_INIT (gst_nv_vp8_dec_debug, "nvvp8dec", 0, "nvvp8dec");

  cdata = g_new0 (GstNvDecoderClassData, 1);

  s = gst_caps_get_structure (sink_caps, 0);
  value = gst_structure_get_value (s, "width");
  cdata->max_width = (guint) gst_value_get_int_range_max (value);

  value = gst_structure_get_value (s, "height");
  cdata->max_height = (guint) gst_value_get_int_range_max (value);

  cdata->sink_caps = gst_caps_ref (sink_caps);
  cdata->src_caps = gst_caps_ref (src_caps);
  cdata->cuda_device_id = device_id;
  cdata->adapter_luid = adapter_luid;

  type_name = g_strdup ("GstNvVp8Dec");
  feature_name = g_strdup ("nvvp8dec");

  while (g_type_from_name (type_name)) {
    index++;
    g_free (type_name);
    g_free (feature_name);
    type_name = g_strdup_printf ("GstNvVp8Device%dDec", index);
    feature_name = g_strdup_printf ("nvvp8device%ddec", index);
  }

  type_info.class_data = cdata;
  type = g_type_register_static (GST_TYPE_VP8_DECODER,
      type_name, &type_info, (GTypeFlags) 0);

  /* make lower rank than default device */
  if (rank > 0 && index > 0)
    rank--;

  if (!gst_element_register (plugin, feature_name, rank, type))
    GST_WARNING ("Failed to register plugin '%s'", type_name);

  g_free (type_name);
  g_free (feature_name);
}
