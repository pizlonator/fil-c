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
 * SECTION:element-nvav1dec
 * @title: nvav1dec
 *
 * GstCodecs based NVIDIA AV1 video decoder
 *
 * ## Example launch line
 * ```
 * gst-launch-1.0 filesrc location=/path/to/av1/file ! parsebin ! nvav1dec ! videoconvert ! autovideosink
 * ```
 *
 * Since: 1.22
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstnvav1dec.h"
#include "gstnvdecoder.h"

#include <string.h>

GST_DEBUG_CATEGORY_STATIC (gst_nv_av1_dec_debug);
#define GST_CAT_DEFAULT gst_nv_av1_dec_debug

typedef struct _GstNvAV1Dec
{
  GstAV1Decoder parent;

  GstNvDecoder *decoder;

  GstAV1SequenceHeaderOBU seq_hdr;
  CUVIDPICPARAMS params;

  /* slice buffer which will be passed to CUVIDPICPARAMS::pBitstreamData */
  guint8 *bitstream_buffer;
  /* allocated memory size of bitstream_buffer */
  gsize bitstream_buffer_alloc_size;
  /* current offset of bitstream_buffer (per frame) */
  gsize bitstream_buffer_offset;

  guint *tile_offsets;
  guint tile_offsets_alloc_len;
  guint num_tiles;

  guint max_width;
  guint max_height;
  guint bitdepth;
  guint8 film_grain_params_present;

  guint num_output_surfaces;
  guint init_max_width;
  guint init_max_height;
  gint max_display_delay;
} GstNvAV1Dec;

typedef struct _GstNvAV1DecClass
{
  GstAV1DecoderClass parent_class;
  guint cuda_device_id;
  gint64 adapter_luid;
  guint max_width;
  guint max_height;
} GstNvAV1DecClass;

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

#define GST_NV_AV1_DEC(object) ((GstNvAV1Dec *) (object))
#define GST_NV_AV1_DEC_GET_CLASS(object) \
    (G_TYPE_INSTANCE_GET_CLASS ((object),G_TYPE_FROM_INSTANCE (object),GstNvAV1DecClass))

static void gst_nv_av1_dec_finalize (GObject * object);
static void gst_nv_av1_dec_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_nv_av1_dec_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static void gst_nv_av1_dec_set_context (GstElement * element,
    GstContext * context);
static gboolean gst_nv_av1_dec_open (GstVideoDecoder * decoder);
static gboolean gst_nv_av1_dec_close (GstVideoDecoder * decoder);
static gboolean gst_nv_av1_dec_stop (GstVideoDecoder * decoder);
static gboolean gst_nv_av1_dec_negotiate (GstVideoDecoder * decoder);
static gboolean gst_nv_av1_dec_decide_allocation (GstVideoDecoder *
    decoder, GstQuery * query);
static gboolean gst_nv_av1_dec_sink_query (GstVideoDecoder * decoder,
    GstQuery * query);
static gboolean gst_nv_av1_dec_src_query (GstVideoDecoder * decoder,
    GstQuery * query);
static gboolean gst_nv_av1_dec_sink_event (GstVideoDecoder * decoder,
    GstEvent * event);

static GstFlowReturn gst_nv_av1_dec_new_sequence (GstAV1Decoder * decoder,
    const GstAV1SequenceHeaderOBU * seq_hdr, gint max_dpb_size);
static GstFlowReturn gst_nv_av1_dec_new_picture (GstAV1Decoder * decoder,
    GstVideoCodecFrame * frame, GstAV1Picture * picture);
static GstAV1Picture *gst_nv_av1_dec_duplicate_picture (GstAV1Decoder *
    decoder, GstVideoCodecFrame * frame, GstAV1Picture * picture);
static GstFlowReturn gst_nv_av1_dec_start_picture (GstAV1Decoder * decoder,
    GstAV1Picture * picture, GstAV1Dpb * dpb);
static GstFlowReturn gst_nv_av1_dec_decode_tile (GstAV1Decoder * decoder,
    GstAV1Picture * picture, GstAV1Tile * tile);
static GstFlowReturn gst_nv_av1_dec_end_picture (GstAV1Decoder * decoder,
    GstAV1Picture * picture);
static GstFlowReturn gst_nv_av1_dec_output_picture (GstAV1Decoder *
    decoder, GstVideoCodecFrame * frame, GstAV1Picture * picture);
static guint gst_nv_av1_dec_get_preferred_output_delay (GstAV1Decoder * decoder,
    gboolean is_live);

static void
gst_nv_av1_dec_class_init (GstNvAV1DecClass * klass,
    GstNvDecoderClassData * cdata)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstVideoDecoderClass *decoder_class = GST_VIDEO_DECODER_CLASS (klass);
  GstAV1DecoderClass *av1decoder_class = GST_AV1_DECODER_CLASS (klass);

  object_class->finalize = gst_nv_av1_dec_finalize;
  object_class->set_property = gst_nv_av1_dec_set_property;
  object_class->get_property = gst_nv_av1_dec_get_property;

  g_object_class_install_property (object_class, PROP_CUDA_DEVICE_ID,
      g_param_spec_uint ("cuda-device-id", "CUDA device id",
          "Assigned CUDA device id", 0, G_MAXINT, 0,
          (GParamFlags) (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)));

  /**
   * GstNvAV1Dec:num-output-surfaces:
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
   * GstNvAV1Dec:init-max-width:
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
   * GstNvAV1Dec:init-max-height:
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
   * GstNvAV1Dec:max-display-delay:
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

  element_class->set_context = GST_DEBUG_FUNCPTR (gst_nv_av1_dec_set_context);

  parent_class = (GTypeClass *) g_type_class_peek_parent (klass);
  gst_element_class_set_static_metadata (element_class, "NVDEC AV1 Decoder",
      "Codec/Decoder/Video/Hardware",
      "NVIDIA AV1 video decoder", "Seungha Yang <seungha@centricular.com>");

  gst_element_class_add_pad_template (element_class,
      gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
          cdata->sink_caps));
  gst_element_class_add_pad_template (element_class,
      gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
          cdata->src_caps));

  decoder_class->open = GST_DEBUG_FUNCPTR (gst_nv_av1_dec_open);
  decoder_class->close = GST_DEBUG_FUNCPTR (gst_nv_av1_dec_close);
  decoder_class->stop = GST_DEBUG_FUNCPTR (gst_nv_av1_dec_stop);
  decoder_class->negotiate = GST_DEBUG_FUNCPTR (gst_nv_av1_dec_negotiate);
  decoder_class->decide_allocation =
      GST_DEBUG_FUNCPTR (gst_nv_av1_dec_decide_allocation);
  decoder_class->sink_query = GST_DEBUG_FUNCPTR (gst_nv_av1_dec_sink_query);
  decoder_class->src_query = GST_DEBUG_FUNCPTR (gst_nv_av1_dec_src_query);
  decoder_class->sink_event = GST_DEBUG_FUNCPTR (gst_nv_av1_dec_sink_event);

  av1decoder_class->new_sequence =
      GST_DEBUG_FUNCPTR (gst_nv_av1_dec_new_sequence);
  av1decoder_class->new_picture =
      GST_DEBUG_FUNCPTR (gst_nv_av1_dec_new_picture);
  av1decoder_class->duplicate_picture =
      GST_DEBUG_FUNCPTR (gst_nv_av1_dec_duplicate_picture);
  av1decoder_class->start_picture =
      GST_DEBUG_FUNCPTR (gst_nv_av1_dec_start_picture);
  av1decoder_class->decode_tile =
      GST_DEBUG_FUNCPTR (gst_nv_av1_dec_decode_tile);
  av1decoder_class->end_picture =
      GST_DEBUG_FUNCPTR (gst_nv_av1_dec_end_picture);
  av1decoder_class->output_picture =
      GST_DEBUG_FUNCPTR (gst_nv_av1_dec_output_picture);
  av1decoder_class->get_preferred_output_delay =
      GST_DEBUG_FUNCPTR (gst_nv_av1_dec_get_preferred_output_delay);

  klass->cuda_device_id = cdata->cuda_device_id;
  klass->adapter_luid = cdata->adapter_luid;
  klass->max_width = cdata->max_width;
  klass->max_height = cdata->max_height;

  gst_caps_unref (cdata->sink_caps);
  gst_caps_unref (cdata->src_caps);
  g_free (cdata);
}

static void
gst_nv_av1_dec_init (GstNvAV1Dec * self)
{
  GstNvAV1DecClass *klass = GST_NV_AV1_DEC_GET_CLASS (self);

  self->decoder =
      gst_nv_decoder_new (klass->cuda_device_id, klass->adapter_luid);

  self->num_output_surfaces = DEFAULT_NUM_OUTPUT_SURFACES;
  self->max_display_delay = DEFAULT_MAX_DISPLAY_DELAY;
}

static void
gst_nv_av1_dec_finalize (GObject * object)
{
  GstNvAV1Dec *self = GST_NV_AV1_DEC (object);

  gst_object_unref (self->decoder);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_nv_av1_dec_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstNvAV1Dec *self = GST_NV_AV1_DEC (object);

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
gst_nv_av1_dec_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstNvAV1Dec *self = GST_NV_AV1_DEC (object);
  GstNvAV1DecClass *klass = GST_NV_AV1_DEC_GET_CLASS (object);

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
gst_nv_av1_dec_set_context (GstElement * element, GstContext * context)
{
  GstNvAV1Dec *self = GST_NV_AV1_DEC (element);

  gst_nv_decoder_handle_set_context (self->decoder, element, context);

  GST_ELEMENT_CLASS (parent_class)->set_context (element, context);
}

static gboolean
gst_nv_av1_dec_open (GstVideoDecoder * decoder)
{
  GstNvAV1Dec *self = GST_NV_AV1_DEC (decoder);

  return gst_nv_decoder_open (self->decoder, GST_ELEMENT (decoder));
}

static void
gst_nv_av1_dec_reset_bitstream_params (GstNvAV1Dec * self)
{
  self->bitstream_buffer_offset = 0;
  self->num_tiles = 0;

  self->params.nBitstreamDataLen = 0;
  self->params.pBitstreamData = nullptr;
  self->params.nNumSlices = 0;
  self->params.pSliceDataOffsets = nullptr;
}

static gboolean
gst_nv_av1_dec_close (GstVideoDecoder * decoder)
{
  GstNvAV1Dec *self = GST_NV_AV1_DEC (decoder);

  gst_nv_av1_dec_reset_bitstream_params (self);

  g_free (self->bitstream_buffer);
  self->bitstream_buffer = nullptr;

  g_free (self->tile_offsets);
  self->tile_offsets = nullptr;

  self->bitstream_buffer_alloc_size = 0;
  self->tile_offsets_alloc_len = 0;

  return gst_nv_decoder_close (self->decoder);
}

static gboolean
gst_nv_av1_dec_stop (GstVideoDecoder * decoder)
{
  GstNvAV1Dec *self = GST_NV_AV1_DEC (decoder);
  gboolean ret;

  ret = GST_VIDEO_DECODER_CLASS (parent_class)->stop (decoder);

  gst_nv_decoder_reset (self->decoder);

  return ret;
}

static gboolean
gst_nv_av1_dec_negotiate (GstVideoDecoder * decoder)
{
  GstNvAV1Dec *self = GST_NV_AV1_DEC (decoder);
  GstAV1Decoder *av1dec = GST_AV1_DECODER (decoder);

  GST_DEBUG_OBJECT (self, "negotiate");

  if (!gst_nv_decoder_negotiate (self->decoder, decoder, av1dec->input_state))
    return FALSE;

  return GST_VIDEO_DECODER_CLASS (parent_class)->negotiate (decoder);
}

static gboolean
gst_nv_av1_dec_decide_allocation (GstVideoDecoder * decoder, GstQuery * query)
{
  GstNvAV1Dec *self = GST_NV_AV1_DEC (decoder);

  if (!gst_nv_decoder_decide_allocation (self->decoder, decoder, query)) {
    GST_WARNING_OBJECT (self, "Failed to handle decide allocation");
    return FALSE;
  }

  return GST_VIDEO_DECODER_CLASS (parent_class)->decide_allocation
      (decoder, query);
}

static gboolean
gst_nv_av1_dec_sink_query (GstVideoDecoder * decoder, GstQuery * query)
{
  GstNvAV1Dec *self = GST_NV_AV1_DEC (decoder);

  if (gst_nv_decoder_handle_query (self->decoder, GST_ELEMENT (decoder), query))
    return TRUE;

  return GST_VIDEO_DECODER_CLASS (parent_class)->sink_query (decoder, query);
}

static gboolean
gst_nv_av1_dec_src_query (GstVideoDecoder * decoder, GstQuery * query)
{
  GstNvAV1Dec *self = GST_NV_AV1_DEC (decoder);

  if (gst_nv_decoder_handle_query (self->decoder, GST_ELEMENT (decoder), query))
    return TRUE;

  return GST_VIDEO_DECODER_CLASS (parent_class)->src_query (decoder, query);
}

static gboolean
gst_nv_av1_dec_sink_event (GstVideoDecoder * decoder, GstEvent * event)
{
  GstNvAV1Dec *self = GST_NV_AV1_DEC (decoder);

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
gst_nv_av1_dec_new_sequence (GstAV1Decoder * decoder,
    const GstAV1SequenceHeaderOBU * seq_hdr, gint max_dpb_size)
{
  GstNvAV1Dec *self = GST_NV_AV1_DEC (decoder);
  GstNvAV1DecClass *klass = GST_NV_AV1_DEC_GET_CLASS (self);
  gboolean modified = FALSE;
  guint max_width, max_height;

  GST_LOG_OBJECT (self, "new sequence");

  if (seq_hdr->seq_profile != GST_AV1_PROFILE_0) {
    GST_WARNING_OBJECT (self, "Unsupported profile %d", seq_hdr->seq_profile);
    return GST_FLOW_NOT_NEGOTIATED;
  }

  if (seq_hdr->num_planes != 3) {
    GST_WARNING_OBJECT (self, "Monochrome is not supported");
    return GST_FLOW_NOT_NEGOTIATED;
  }

  self->seq_hdr = *seq_hdr;

  if (self->bitdepth != seq_hdr->bit_depth) {
    GST_INFO_OBJECT (self, "Bitdepth changed %d -> %d", self->bitdepth,
        seq_hdr->bit_depth);
    self->bitdepth = seq_hdr->bit_depth;
    modified = TRUE;
  }

  max_width = seq_hdr->max_frame_width_minus_1 + 1;
  max_height = seq_hdr->max_frame_height_minus_1 + 1;

  if (self->max_width != max_width || self->max_height != max_height) {
    GST_INFO_OBJECT (self, "Resolution changed %dx%d -> %dx%d",
        self->max_width, self->max_height, max_width, max_height);
    self->max_width = max_width;
    self->max_height = max_height;
    modified = TRUE;
  }

  if (self->film_grain_params_present != seq_hdr->film_grain_params_present) {
    GST_INFO_OBJECT (self, "Film grain present changed %d -> %d",
        self->film_grain_params_present, seq_hdr->film_grain_params_present);
    self->film_grain_params_present = seq_hdr->film_grain_params_present;
    modified = TRUE;
  }

  if (modified || !gst_nv_decoder_is_configured (self->decoder)) {
    GstVideoInfo info;
    GstVideoFormat out_format = GST_VIDEO_FORMAT_UNKNOWN;

    if (self->bitdepth == 8) {
      out_format = GST_VIDEO_FORMAT_NV12;
    } else if (self->bitdepth == 10) {
      out_format = GST_VIDEO_FORMAT_P010_10LE;
    } else {
      GST_WARNING_OBJECT (self, "Invalid bit-depth %d", seq_hdr->bit_depth);
      return GST_FLOW_NOT_NEGOTIATED;
    }

    gst_video_info_set_format (&info,
        out_format, self->max_width, self->max_height);

    max_width = gst_nv_decoder_get_max_output_size (self->max_width,
        self->init_max_width, klass->max_width);
    max_height = gst_nv_decoder_get_max_output_size (self->max_height,
        self->init_max_height, klass->max_height);

    if (!gst_nv_decoder_configure (self->decoder, cudaVideoCodec_AV1,
            &info, self->max_width, self->max_height, self->bitdepth,
            max_dpb_size, self->film_grain_params_present ? TRUE : FALSE,
            self->num_output_surfaces, max_width, max_height)) {
      GST_ERROR_OBJECT (self, "Failed to create decoder");
      return GST_FLOW_NOT_NEGOTIATED;
    }

    if (!gst_video_decoder_negotiate (GST_VIDEO_DECODER (self))) {
      GST_ERROR_OBJECT (self, "Failed to negotiate with downstream");
      return GST_FLOW_NOT_NEGOTIATED;
    }
  }

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_nv_av1_dec_new_picture (GstAV1Decoder * decoder,
    GstVideoCodecFrame * frame, GstAV1Picture * picture)
{
  GstNvAV1Dec *self = GST_NV_AV1_DEC (decoder);

  return gst_nv_decoder_new_picture (self->decoder,
      GST_CODEC_PICTURE (picture));
}

static GstNvDecSurface *
gst_nv_av1_dec_get_decoder_surface_from_picture (GstNvAV1Dec * self,
    GstAV1Picture * picture)
{
  GstNvDecSurface *surface;

  surface = (GstNvDecSurface *) gst_av1_picture_get_user_data (picture);
  if (!surface)
    GST_DEBUG_OBJECT (self, "current picture does not have decoder surface");

  return surface;
}

static GstAV1Picture *
gst_nv_av1_dec_duplicate_picture (GstAV1Decoder * decoder,
    GstVideoCodecFrame * frame, GstAV1Picture * picture)
{
  GstNvAV1Dec *self = GST_NV_AV1_DEC (decoder);
  GstNvDecSurface *surface;
  GstAV1Picture *new_picture;

  surface = gst_nv_av1_dec_get_decoder_surface_from_picture (self, picture);

  if (!surface) {
    GST_ERROR_OBJECT (self, "Parent picture does not have decoder surface");
    return nullptr;
  }

  new_picture = gst_av1_picture_new ();
  new_picture->frame_hdr = picture->frame_hdr;

  gst_av1_picture_set_user_data (new_picture,
      gst_nv_dec_surface_ref (surface),
      (GDestroyNotify) gst_nv_dec_surface_unref);

  return new_picture;
}

static inline guint8
gst_nv_av1_dec_get_lr_unit_size (guint size)
{
  switch (size) {
    case 32:
      return 0;
    case 64:
      return 1;
    case 128:
      return 2;
    case 256:
      return 3;
    default:
      break;
  }

  return 3;
}

static GstFlowReturn
gst_nv_av1_dec_start_picture (GstAV1Decoder * decoder, GstAV1Picture * picture,
    GstAV1Dpb * dpb)
{
  GstNvAV1Dec *self = GST_NV_AV1_DEC (decoder);
  CUVIDPICPARAMS *params = &self->params;
  CUVIDAV1PICPARAMS *av1_params = &params->CodecSpecific.av1;
  const GstAV1SequenceHeaderOBU *seq_hdr = &self->seq_hdr;
  const GstAV1FrameHeaderOBU *frame_hdr = &picture->frame_hdr;
  const GstAV1GlobalMotionParams *gmp = &frame_hdr->global_motion_params;
  const GstAV1QuantizationParams *qp = &frame_hdr->quantization_params;
  const GstAV1TileInfo *ti = &frame_hdr->tile_info;
  const GstAV1CDEFParams *cp = &frame_hdr->cdef_params;
  const GstAV1SegmenationParams *sp = &frame_hdr->segmentation_params;
  const GstAV1LoopFilterParams *lp = &frame_hdr->loop_filter_params;
  const GstAV1LoopRestorationParams *lrp = &frame_hdr->loop_restoration_params;
  const GstAV1FilmGrainParams *fgp = &frame_hdr->film_grain_params;
  GstNvDecSurface *surface;
  GstNvDecSurface *other_surface;
  GstAV1Picture *other_pic;
  guint i, j;

  surface = gst_nv_av1_dec_get_decoder_surface_from_picture (self, picture);
  if (!surface) {
    GST_ERROR_OBJECT (self, "Decoder frame is unavailable");
    return GST_FLOW_ERROR;
  }

  memset (params, 0, sizeof (CUVIDPICPARAMS));

  params->PicWidthInMbs = GST_ROUND_UP_16 (frame_hdr->frame_width) >> 4;
  params->FrameHeightInMbs = GST_ROUND_UP_16 (frame_hdr->frame_height) >> 4;
  params->CurrPicIdx = surface->index;
  params->intra_pic_flag = frame_hdr->frame_is_intra;

  av1_params->width = frame_hdr->frame_width;
  av1_params->height = frame_hdr->frame_height;
  av1_params->frame_offset = frame_hdr->order_hint;
  av1_params->decodePicIdx = surface->decode_frame_index;

  /* sequence header */
  av1_params->profile = seq_hdr->seq_profile;
  av1_params->use_128x128_superblock = seq_hdr->use_128x128_superblock;
  av1_params->subsampling_x = seq_hdr->color_config.subsampling_x;
  av1_params->subsampling_y = seq_hdr->color_config.subsampling_y;
  av1_params->mono_chrome = seq_hdr->color_config.mono_chrome;
  av1_params->bit_depth_minus8 = seq_hdr->bit_depth - 8;
  av1_params->enable_filter_intra = seq_hdr->enable_filter_intra;
  av1_params->enable_intra_edge_filter = seq_hdr->enable_intra_edge_filter;
  av1_params->enable_interintra_compound = seq_hdr->enable_interintra_compound;
  av1_params->enable_masked_compound = seq_hdr->enable_masked_compound;
  av1_params->enable_dual_filter = seq_hdr->enable_dual_filter;
  av1_params->enable_order_hint = seq_hdr->enable_order_hint;
  av1_params->order_hint_bits_minus1 = seq_hdr->order_hint_bits_minus_1;
  av1_params->enable_jnt_comp = seq_hdr->enable_jnt_comp;
  av1_params->enable_superres = seq_hdr->enable_superres;
  av1_params->enable_cdef = seq_hdr->enable_cdef;
  av1_params->enable_restoration = seq_hdr->enable_restoration;
  av1_params->enable_fgs = seq_hdr->film_grain_params_present;

  /* frame header */
  av1_params->frame_type = frame_hdr->frame_type;
  av1_params->show_frame = frame_hdr->show_frame;
  av1_params->disable_cdf_update = frame_hdr->disable_cdf_update;
  av1_params->allow_screen_content_tools =
      frame_hdr->allow_screen_content_tools;
  if (frame_hdr->force_integer_mv || frame_hdr->frame_is_intra)
    av1_params->force_integer_mv = 1;
  else
    av1_params->force_integer_mv = 0;
  if (frame_hdr->use_superres) {
    av1_params->coded_denom =
        frame_hdr->superres_denom - GST_AV1_SUPERRES_DENOM_MIN;
  } else {
    av1_params->coded_denom = 0;
  }
  av1_params->allow_intrabc = frame_hdr->allow_intrabc;
  av1_params->allow_high_precision_mv = frame_hdr->allow_high_precision_mv;
  av1_params->interp_filter = frame_hdr->interpolation_filter;
  av1_params->switchable_motion_mode = frame_hdr->is_motion_mode_switchable;
  av1_params->use_ref_frame_mvs = frame_hdr->use_ref_frame_mvs;
  av1_params->disable_frame_end_update_cdf =
      frame_hdr->disable_frame_end_update_cdf;
  av1_params->delta_q_present = qp->delta_q_present;
  av1_params->delta_q_res = qp->delta_q_res;
  av1_params->using_qmatrix = qp->using_qmatrix;
  av1_params->coded_lossless = frame_hdr->coded_lossless;
  av1_params->use_superres = frame_hdr->use_superres;
  av1_params->tx_mode = frame_hdr->tx_mode;
  av1_params->reference_mode = frame_hdr->reference_select;
  av1_params->allow_warped_motion = frame_hdr->allow_warped_motion;
  av1_params->reduced_tx_set = frame_hdr->reduced_tx_set;
  av1_params->skip_mode = frame_hdr->skip_mode_present;

  /* tiling info */
  av1_params->num_tile_cols = ti->tile_cols;
  av1_params->num_tile_rows = ti->tile_rows;
  av1_params->context_update_tile_id = ti->context_update_tile_id;
  for (i = 0; i < ti->tile_cols; i++)
    av1_params->tile_widths[i] = ti->width_in_sbs_minus_1[i] + 1;

  for (i = 0; i < ti->tile_rows; i++)
    av1_params->tile_heights[i] = ti->height_in_sbs_minus_1[i] + 1;

  /* CDEF */
  av1_params->cdef_damping_minus_3 = cp->cdef_damping - 3;
  av1_params->cdef_bits = cp->cdef_bits;
  for (i = 0; i < GST_AV1_CDEF_MAX; i++) {
    guint8 primary;
    guint8 secondary;

    primary = cp->cdef_y_pri_strength[i];
    secondary = cp->cdef_y_sec_strength[i];
    if (secondary == 4)
      secondary--;

    av1_params->cdef_y_strength[i] = (primary & 0x0f) | (secondary << 4);

    primary = cp->cdef_uv_pri_strength[i];
    secondary = cp->cdef_uv_sec_strength[i];
    if (secondary == 4)
      secondary--;

    av1_params->cdef_uv_strength[i] = (primary & 0x0f) | (secondary << 4);
  }

  /* SkipModeFrames */
  if (frame_hdr->skip_mode_present) {
    av1_params->SkipModeFrame0 = frame_hdr->skip_mode_frame[0];
    av1_params->SkipModeFrame1 = frame_hdr->skip_mode_frame[1];
  }

  /* qp information */
  av1_params->base_qindex = qp->base_q_idx;
  av1_params->qp_y_dc_delta_q = qp->delta_q_y_dc;
  av1_params->qp_u_dc_delta_q = qp->delta_q_u_dc;
  av1_params->qp_u_ac_delta_q = qp->delta_q_u_ac;
  av1_params->qp_v_dc_delta_q = qp->delta_q_v_dc;
  av1_params->qp_v_ac_delta_q = qp->delta_q_v_ac;
  av1_params->qm_y = qp->qm_y;
  av1_params->qm_u = qp->qm_u;
  av1_params->qm_v = qp->qm_v;

  /* segmentation */
  av1_params->segmentation_enabled = sp->segmentation_enabled;
  av1_params->segmentation_update_map = sp->segmentation_update_map;
  av1_params->segmentation_update_data = sp->segmentation_update_data;
  av1_params->segmentation_temporal_update = sp->segmentation_temporal_update;
  for (i = 0; i < GST_AV1_MAX_SEGMENTS; i++) {
    for (j = 0; j < GST_AV1_SEG_LVL_MAX; j++) {
      av1_params->segmentation_feature_data[i][j] = sp->feature_data[i][j];
      av1_params->segmentation_feature_mask[i] |=
          sp->feature_enabled[i][j] << j;
    }
  }

  /* loopfilter */
  av1_params->loop_filter_level[0] = lp->loop_filter_level[0];
  av1_params->loop_filter_level[1] = lp->loop_filter_level[1];
  av1_params->loop_filter_level_u = lp->loop_filter_level[2];
  av1_params->loop_filter_level_v = lp->loop_filter_level[3];
  av1_params->loop_filter_sharpness = lp->loop_filter_sharpness;
  for (i = 0; i < GST_AV1_TOTAL_REFS_PER_FRAME; i++) {
    av1_params->loop_filter_ref_deltas[i] = lp->loop_filter_ref_deltas[i];
  }
  av1_params->loop_filter_mode_deltas[0] = lp->loop_filter_mode_deltas[0];
  av1_params->loop_filter_mode_deltas[1] = lp->loop_filter_mode_deltas[1];
  av1_params->loop_filter_delta_enabled = lp->loop_filter_delta_enabled;
  av1_params->loop_filter_delta_update = lp->loop_filter_delta_update;
  av1_params->delta_lf_present = lp->delta_lf_present;
  av1_params->delta_lf_res = lp->delta_lf_res;
  av1_params->delta_lf_multi = lp->delta_lf_multi;

  /* restoration */
  for (i = 0; i < 3; i++) {
    av1_params->lr_unit_size[i] =
        gst_nv_av1_dec_get_lr_unit_size (lrp->loop_restoration_size[i]);
  }
  av1_params->lr_type[0] = lrp->frame_restoration_type[0];
  av1_params->lr_type[1] = lrp->frame_restoration_type[1];
  av1_params->lr_type[2] = lrp->frame_restoration_type[2];

  /* reference frames */
  for (i = 0; i < GST_AV1_TOTAL_REFS_PER_FRAME; i++) {
    guint8 ref_idx = 0xff;

    other_pic = dpb->pic_list[i];
    if (other_pic) {
      other_surface =
          gst_nv_av1_dec_get_decoder_surface_from_picture (self, other_pic);
      if (!other_surface) {
        GST_ERROR_OBJECT (self, "reference frame is unavailable");
        return GST_FLOW_ERROR;
      }

      ref_idx = other_surface->decode_frame_index;
    }

    av1_params->ref_frame_map[i] = ref_idx;
  }

  if (frame_hdr->primary_ref_frame == GST_AV1_PRIMARY_REF_NONE) {
    av1_params->primary_ref_frame = 0xff;
  } else {
    guint8 primary_ref_idx;

    g_assert (frame_hdr->primary_ref_frame < 8);

    primary_ref_idx = frame_hdr->ref_frame_idx[frame_hdr->primary_ref_frame];
    av1_params->primary_ref_frame = av1_params->ref_frame_map[primary_ref_idx];
  }
  av1_params->temporal_layer_id = picture->temporal_id;
  av1_params->spatial_layer_id = picture->spatial_id;

  /* ref frame list and global motion */
  for (i = 0; i < GST_AV1_REFS_PER_FRAME; i++) {
    gint8 ref_idx = frame_hdr->ref_frame_idx[i];

    other_pic = nullptr;

    if (ref_idx >= 0)
      other_pic = dpb->pic_list[ref_idx];

    if (other_pic) {
      other_surface =
          gst_nv_av1_dec_get_decoder_surface_from_picture (self, other_pic);

      av1_params->ref_frame[i].index = other_surface->decode_frame_index;
      av1_params->ref_frame[i].width = other_pic->frame_hdr.frame_width;
      av1_params->ref_frame[i].height = other_pic->frame_hdr.frame_height;
    } else {
      av1_params->ref_frame[i].index = 0xff;
    }

    av1_params->global_motion[i].invalid = gmp->invalid[i];
    av1_params->global_motion[i].wmtype =
        gmp->gm_type[GST_AV1_REF_LAST_FRAME + i];
    for (j = 0; j < 6; j++) {
      av1_params->global_motion[i].wmmat[j] =
          gmp->gm_params[GST_AV1_REF_LAST_FRAME + i][j];
    }
  }

  /* film grain params */
  if (seq_hdr->film_grain_params_present) {
    av1_params->apply_grain = fgp->apply_grain;
    av1_params->overlap_flag = fgp->overlap_flag;
    av1_params->scaling_shift_minus8 = fgp->grain_scaling_minus_8;
    av1_params->chroma_scaling_from_luma = fgp->chroma_scaling_from_luma;
    av1_params->ar_coeff_lag = fgp->ar_coeff_lag;
    av1_params->ar_coeff_shift_minus6 = fgp->ar_coeff_shift_minus_6;
    av1_params->grain_scale_shift = fgp->grain_scale_shift;
    av1_params->clip_to_restricted_range = fgp->clip_to_restricted_range;
    av1_params->num_y_points = fgp->num_y_points;
    for (i = 0; i < fgp->num_y_points && i < 14; i++) {
      av1_params->scaling_points_y[i][0] = fgp->point_y_value[i];
      av1_params->scaling_points_y[i][1] = fgp->point_y_scaling[i];
    }

    av1_params->num_cb_points = fgp->num_cb_points;
    for (i = 0; i < fgp->num_cb_points && i < 10; i++) {
      av1_params->scaling_points_cb[i][0] = fgp->point_cb_value[i];
      av1_params->scaling_points_cb[i][1] = fgp->point_cb_scaling[i];
    }

    av1_params->num_cr_points = fgp->num_cr_points;
    for (i = 0; i < fgp->num_cr_points && i < 10; i++) {
      av1_params->scaling_points_cr[i][0] = fgp->point_cr_value[i];
      av1_params->scaling_points_cr[i][1] = fgp->point_cr_scaling[i];
    }

    av1_params->random_seed = fgp->grain_seed;
    for (i = 0; i < 24; i++) {
      av1_params->ar_coeffs_y[i] = (short) fgp->ar_coeffs_y_plus_128[i] - 128;
    }

    for (i = 0; i < 25; i++) {
      av1_params->ar_coeffs_cb[i] = (short) fgp->ar_coeffs_cb_plus_128[i] - 128;
      av1_params->ar_coeffs_cr[i] = (short) fgp->ar_coeffs_cr_plus_128[i] - 128;
    }
    av1_params->cb_mult = fgp->cb_mult;
    av1_params->cb_luma_mult = fgp->cb_luma_mult;
    av1_params->cb_offset = fgp->cb_offset;
    av1_params->cr_mult = fgp->cr_mult;
    av1_params->cr_luma_mult = fgp->cr_luma_mult;
    av1_params->cr_offset = fgp->cr_offset;
  }

  gst_nv_av1_dec_reset_bitstream_params (self);

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_nv_av1_dec_decode_tile (GstAV1Decoder * decoder,
    GstAV1Picture * picture, GstAV1Tile * tile)
{
  GstNvAV1Dec *self = GST_NV_AV1_DEC (decoder);
  GstAV1TileGroupOBU *tile_group = &tile->tile_group;
  guint i;
  guint buffer_size;

  if (tile_group->num_tiles * 2 > self->tile_offsets_alloc_len) {
    self->tile_offsets_alloc_len = tile_group->num_tiles * 2;

    self->tile_offsets = (guint *) g_realloc_n (self->tile_offsets,
        self->tile_offsets_alloc_len, sizeof (guint));
  }

  self->num_tiles = tile_group->num_tiles;

  for (i = tile_group->tg_start; i <= tile_group->tg_end; i++) {
    guint offset = self->bitstream_buffer_offset +
        tile_group->entry[i].tile_offset;

    self->tile_offsets[i * 2] = offset;
    self->tile_offsets[i * 2 + 1] = offset + tile_group->entry[i].tile_size;
  }

  buffer_size = self->bitstream_buffer_offset + tile->obu.obu_size;
  if (buffer_size > self->bitstream_buffer_alloc_size) {
    guint alloc_size = buffer_size * 2;

    self->bitstream_buffer = (guint8 *) g_realloc (self->bitstream_buffer,
        alloc_size);
    self->bitstream_buffer_alloc_size = alloc_size;
  }

  memcpy (self->bitstream_buffer + self->bitstream_buffer_offset,
      tile->obu.data, tile->obu.obu_size);

  self->bitstream_buffer_offset += tile->obu.obu_size;

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_nv_av1_dec_end_picture (GstAV1Decoder * decoder, GstAV1Picture * picture)
{
  GstNvAV1Dec *self = GST_NV_AV1_DEC (decoder);
  gboolean ret;
  CUVIDPICPARAMS *params = &self->params;

  params->nBitstreamDataLen = self->bitstream_buffer_offset;
  params->pBitstreamData = self->bitstream_buffer;
  params->nNumSlices = self->num_tiles;
  params->pSliceDataOffsets = self->tile_offsets;

  ret = gst_nv_decoder_decode (self->decoder, params);

  if (!ret) {
    GST_ERROR_OBJECT (self, "Failed to decode picture");
    return GST_FLOW_ERROR;
  }

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_nv_av1_dec_output_picture (GstAV1Decoder * decoder,
    GstVideoCodecFrame * frame, GstAV1Picture * picture)
{
  GstNvAV1Dec *self = GST_NV_AV1_DEC (decoder);

  return gst_nv_decoder_output_picture (self->decoder,
      GST_VIDEO_DECODER (decoder), frame, GST_CODEC_PICTURE (picture), 0);
}

static guint
gst_nv_av1_dec_get_preferred_output_delay (GstAV1Decoder * decoder,
    gboolean is_live)
{
  GstNvAV1Dec *self = GST_NV_AV1_DEC (decoder);

  if (self->max_display_delay >= 0)
    return self->max_display_delay;

  /* Prefer to zero latency for live pipeline */
  if (is_live)
    return 0;

  return 2;
}

void
gst_nv_av1_dec_register (GstPlugin * plugin, guint device_id,
    gint64 adapter_luid, guint rank, GstCaps * sink_caps, GstCaps * src_caps)
{
  GType type;
  gchar *type_name;
  gchar *feature_name;
  guint index = 0;
  const GValue *value;
  GstStructure *s;
  GTypeInfo type_info = {
    sizeof (GstNvAV1DecClass),
    nullptr,
    nullptr,
    (GClassInitFunc) gst_nv_av1_dec_class_init,
    nullptr,
    nullptr,
    sizeof (GstNvAV1Dec),
    0,
    (GInstanceInitFunc) gst_nv_av1_dec_init,
  };
  GstNvDecoderClassData *cdata;

  GST_DEBUG_CATEGORY_INIT (gst_nv_av1_dec_debug, "nvav1dec", 0, "nvav1dec");

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

  type_info.class_data = cdata;

  type_name = g_strdup ("GstNvAV1Dec");
  feature_name = g_strdup ("nvav1dec");

  while (g_type_from_name (type_name)) {
    index++;
    g_free (type_name);
    g_free (feature_name);
    type_name = g_strdup_printf ("GstNvAV1Device%dDec", index);
    feature_name = g_strdup_printf ("nvav1device%ddec", index);
  }

  type = g_type_register_static (GST_TYPE_AV1_DECODER,
      type_name, &type_info, (GTypeFlags) 0);

  /* make lower rank than default device */
  if (rank > 0 && index != 0)
    rank--;

  if (index != 0)
    gst_element_type_set_skip_documentation (type);

  if (!gst_element_register (plugin, feature_name, rank, type))
    GST_WARNING ("Failed to register plugin '%s'", type_name);

  g_free (type_name);
  g_free (feature_name);
}
