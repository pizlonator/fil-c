/* 
 * Copyright (C) 2012 Collabora Ltd.
 *     Author: Sebastian Dröge <sebastian.droege@collabora.co.uk>
 * Copyright (C) 2013 Sebastian Dröge <slomo@circular-chaos.org>
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
 */

/**
 * SECTION:element-openjpegenc
 * @title: openjpegenc
 * @see_also: openjpegdec
 *
 * openjpegenc encodes raw video stream.
 *
 * ## Example launch lines
 * |[
 * gst-launch-1.0 -v videotestsrc num-buffers=10 ! openjpegenc ! jpeg2000parse ! openjpegdec ! videoconvert ! autovideosink sync=false
 * ]| Encode and decode whole frames.
 * |[
 * gst-launch-1.0 -v videotestsrc num-buffers=10 ! openjpegenc num-threads=8 num-stripes=8 ! jpeg2000parse ! openjpegdec max-threads=8 ! videoconvert ! autovideosink sync=fals
 * ]| Encode and decode frame split with stripes.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstopenjpegenc.h"
#include <gst/codecparsers/gstjpeg2000sampling.h>

#include <string.h>
#include <math.h>

GST_DEBUG_CATEGORY_STATIC (gst_openjpeg_enc_debug);
#define GST_CAT_DEFAULT gst_openjpeg_enc_debug

#define GST_OPENJPEG_ENC_TYPE_PROGRESSION_ORDER (gst_openjpeg_enc_progression_order_get_type())
static GType
gst_openjpeg_enc_progression_order_get_type (void)
{
  static const GEnumValue values[] = {
    {OPJ_LRCP, "LRCP", "lrcp"},
    {OPJ_RLCP, "RLCP", "rlcp"},
    {OPJ_RPCL, "RPCL", "rpcl"},
    {OPJ_PCRL, "PCRL", "pcrl"},
    {OPJ_CPRL, "CPRL", "crpl"},
    {0, NULL, NULL}
  };
  static GType id = 0;

  if (g_once_init_enter ((gsize *) & id)) {
    GType _id;

    _id = g_enum_register_static ("GstOpenJPEGEncProgressionOrder", values);

    g_once_init_leave ((gsize *) & id, _id);
  }

  return id;
}

enum
{
  PROP_0,
  PROP_NUM_LAYERS,
  PROP_NUM_RESOLUTIONS,
  PROP_PROGRESSION_ORDER,
  PROP_TILE_OFFSET_X,
  PROP_TILE_OFFSET_Y,
  PROP_TILE_WIDTH,
  PROP_TILE_HEIGHT,
  PROP_NUM_STRIPES,
  PROP_NUM_THREADS,
  PROP_LAST
};


#define DEFAULT_NUM_LAYERS 1
#define DEFAULT_NUM_RESOLUTIONS 6
#define DEFAULT_PROGRESSION_ORDER OPJ_LRCP
#define DEFAULT_TILE_OFFSET_X 0
#define DEFAULT_TILE_OFFSET_Y 0
#define DEFAULT_TILE_WIDTH 0
#define DEFAULT_TILE_HEIGHT 0
#define GST_OPENJPEG_ENC_DEFAULT_NUM_STRIPES  1
#define GST_OPENJPEG_ENC_DEFAULT_NUM_THREADS 0

/* prototypes */
static void gst_openjpeg_enc_finalize (GObject * object);

static GstStateChangeReturn
gst_openjpeg_enc_change_state (GstElement * element, GstStateChange transition);

static void gst_openjpeg_enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_openjpeg_enc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_openjpeg_enc_start (GstVideoEncoder * encoder);
static gboolean gst_openjpeg_enc_stop (GstVideoEncoder * encoder);
static gboolean gst_openjpeg_enc_set_format (GstVideoEncoder * encoder,
    GstVideoCodecState * state);
static GstFlowReturn gst_openjpeg_enc_handle_frame (GstVideoEncoder * encoder,
    GstVideoCodecFrame * frame);
static gboolean gst_openjpeg_enc_propose_allocation (GstVideoEncoder * encoder,
    GstQuery * query);
static GstFlowReturn gst_openjpeg_enc_encode_frame_multiple (GstVideoEncoder *
    encoder, GstVideoCodecFrame * frame);
static GstFlowReturn gst_openjpeg_enc_encode_frame_single (GstVideoEncoder *
    encoder, GstVideoCodecFrame * frame);
static GstOpenJPEGCodecMessage
    * gst_openjpeg_encode_message_free (GstOpenJPEGCodecMessage * message);

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
#define GRAY16 "GRAY16_LE"
#define YUV10 "Y444_10LE, I422_10LE, I420_10LE"
#else
#define GRAY16 "GRAY16_BE"
#define YUV10 "Y444_10BE, I422_10BE, I420_10BE"
#endif

static GstStaticPadTemplate gst_openjpeg_enc_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("{ ARGB64, ARGB, xRGB, "
            "AYUV64, " YUV10 ", "
            "AYUV, Y444, Y42B, I420, Y41B, YUV9, " "GRAY8, " GRAY16 " }"))
    );

static GstStaticPadTemplate gst_openjpeg_enc_src_template =
    GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("image/x-j2c, "
        "width = (int) [1, MAX], "
        "height = (int) [1, MAX], "
        "num-components = (int) [1, 4], "
        GST_JPEG2000_SAMPLING_LIST ","
        GST_JPEG2000_COLORSPACE_LIST "; "
        "image/x-jpc, "
        "width = (int) [1, MAX], "
        "height = (int) [1, MAX], "
        "num-components = (int) [1, 4], "
        "num-stripes = (int) [1, MAX], "
        "alignment = (string) { frame, stripe }, "
        GST_JPEG2000_SAMPLING_LIST ","
        GST_JPEG2000_COLORSPACE_LIST "; "
        "image/jp2, " "width = (int) [1, MAX], "
        "height = (int) [1, MAX] ;"
        "image/x-jpc-striped, "
        "width = (int) [1, MAX], "
        "height = (int) [1, MAX], "
        "num-components = (int) [1, 4], "
        GST_JPEG2000_SAMPLING_LIST ", "
        GST_JPEG2000_COLORSPACE_LIST ", "
        "num-stripes = (int) [2, MAX], stripe-height = (int) [1 , MAX]")
    );

#define parent_class gst_openjpeg_enc_parent_class
G_DEFINE_TYPE (GstOpenJPEGEnc, gst_openjpeg_enc, GST_TYPE_VIDEO_ENCODER);
GST_ELEMENT_REGISTER_DEFINE (openjpegenc, "openjpegenc",
    GST_RANK_PRIMARY, GST_TYPE_OPENJPEG_ENC);

static void
gst_openjpeg_enc_class_init (GstOpenJPEGEncClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class;
  GstVideoEncoderClass *video_encoder_class;

  gobject_class = (GObjectClass *) klass;
  element_class = (GstElementClass *) klass;
  video_encoder_class = (GstVideoEncoderClass *) klass;

  gobject_class->set_property = gst_openjpeg_enc_set_property;
  gobject_class->get_property = gst_openjpeg_enc_get_property;
  gobject_class->finalize = gst_openjpeg_enc_finalize;

  element_class->change_state =
      GST_DEBUG_FUNCPTR (gst_openjpeg_enc_change_state);

  g_object_class_install_property (gobject_class, PROP_NUM_LAYERS,
      g_param_spec_int ("num-layers", "Number of layers",
          "Number of layers", 1, 10, DEFAULT_NUM_LAYERS,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_NUM_RESOLUTIONS,
      g_param_spec_int ("num-resolutions", "Number of resolutions",
          "Number of resolutions", 1, 10, DEFAULT_NUM_RESOLUTIONS,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_PROGRESSION_ORDER,
      g_param_spec_enum ("progression-order", "Progression Order",
          "Progression order", GST_OPENJPEG_ENC_TYPE_PROGRESSION_ORDER,
          DEFAULT_PROGRESSION_ORDER,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_TILE_OFFSET_X,
      g_param_spec_int ("tile-offset-x", "Tile Offset X",
          "Tile Offset X", G_MININT, G_MAXINT, DEFAULT_TILE_OFFSET_X,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_TILE_OFFSET_Y,
      g_param_spec_int ("tile-offset-y", "Tile Offset Y",
          "Tile Offset Y", G_MININT, G_MAXINT, DEFAULT_TILE_OFFSET_Y,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_TILE_WIDTH,
      g_param_spec_int ("tile-width", "Tile Width",
          "Tile Width", 0, G_MAXINT, DEFAULT_TILE_WIDTH,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_TILE_HEIGHT,
      g_param_spec_int ("tile-height", "Tile Height",
          "Tile Height", 0, G_MAXINT, DEFAULT_TILE_HEIGHT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstOpenJPEGEnc:num-stripes:
   *
   * Number of stripes to use for low latency encoding . (1 = low latency disabled)
   *
   * Since: 1.18
   */
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_NUM_STRIPES,
      g_param_spec_int ("num-stripes", "Number of stripes",
          "Number of stripes for low latency encoding. (1 = low latency disabled)",
          1, G_MAXINT, GST_OPENJPEG_ENC_DEFAULT_NUM_STRIPES,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  /**
   * GstOpenJPEGEnc:num-threads:
   *
   * Max number of simultaneous threads to encode stripes, default: encode with streaming thread
   *
   * Since: 1.20
   */
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_NUM_THREADS,
      g_param_spec_uint ("num-threads", "Number of threads",
          "Max number of simultaneous threads to encode stripe or frame, default: encode with streaming thread.",
          0, G_MAXINT, GST_OPENJPEG_ENC_DEFAULT_NUM_THREADS,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_add_static_pad_template (element_class,
      &gst_openjpeg_enc_src_template);
  gst_element_class_add_static_pad_template (element_class,
      &gst_openjpeg_enc_sink_template);

  gst_element_class_set_static_metadata (element_class,
      "OpenJPEG JPEG2000 encoder",
      "Codec/Encoder/Video",
      "Encode JPEG2000 streams",
      "Sebastian Dröge <sebastian.droege@collabora.co.uk>");

  video_encoder_class->start = GST_DEBUG_FUNCPTR (gst_openjpeg_enc_start);
  video_encoder_class->stop = GST_DEBUG_FUNCPTR (gst_openjpeg_enc_stop);
  video_encoder_class->set_format =
      GST_DEBUG_FUNCPTR (gst_openjpeg_enc_set_format);
  video_encoder_class->handle_frame =
      GST_DEBUG_FUNCPTR (gst_openjpeg_enc_handle_frame);
  video_encoder_class->propose_allocation = gst_openjpeg_enc_propose_allocation;

  GST_DEBUG_CATEGORY_INIT (gst_openjpeg_enc_debug, "openjpegenc", 0,
      "OpenJPEG Encoder");

  gst_type_mark_as_plugin_api (GST_OPENJPEG_ENC_TYPE_PROGRESSION_ORDER, 0);
}

static void
gst_openjpeg_enc_init (GstOpenJPEGEnc * self)
{
  GST_PAD_SET_ACCEPT_TEMPLATE (GST_VIDEO_ENCODER_SINK_PAD (self));

  opj_set_default_encoder_parameters (&self->params);

  self->params.cp_fixed_quality = 1;
  self->params.cp_disto_alloc = 0;
  self->params.cp_fixed_alloc = 0;

  /*
   * TODO: Add properties / caps fields for these
   *
   * self->params.csty;
   * self->params.tcp_rates;
   * self->params.tcp_distoratio;
   * self->params.mode;
   * self->params.irreversible;
   * self->params.cp_cinema;
   * self->params.cp_rsiz;
   */

  self->params.tcp_numlayers = DEFAULT_NUM_LAYERS;
  self->params.numresolution = DEFAULT_NUM_RESOLUTIONS;
  self->params.prog_order = DEFAULT_PROGRESSION_ORDER;
  self->params.cp_tx0 = DEFAULT_TILE_OFFSET_X;
  self->params.cp_ty0 = DEFAULT_TILE_OFFSET_Y;
  self->params.cp_tdx = DEFAULT_TILE_WIDTH;
  self->params.cp_tdy = DEFAULT_TILE_HEIGHT;
  self->params.tile_size_on = (self->params.cp_tdx != 0
      && self->params.cp_tdy != 0);

  self->num_stripes = GST_OPENJPEG_ENC_DEFAULT_NUM_STRIPES;
  g_cond_init (&self->messages_cond);
  g_queue_init (&self->messages);

  self->available_threads = GST_OPENJPEG_ENC_DEFAULT_NUM_THREADS;
}

static void
gst_openjpeg_enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstOpenJPEGEnc *self = GST_OPENJPEG_ENC (object);

  switch (prop_id) {
    case PROP_NUM_LAYERS:
      self->params.tcp_numlayers = g_value_get_int (value);
      break;
    case PROP_NUM_RESOLUTIONS:
      self->params.numresolution = g_value_get_int (value);
      break;
    case PROP_PROGRESSION_ORDER:
      self->params.prog_order = g_value_get_enum (value);
      break;
    case PROP_TILE_OFFSET_X:
      self->params.cp_tx0 = g_value_get_int (value);
      break;
    case PROP_TILE_OFFSET_Y:
      self->params.cp_ty0 = g_value_get_int (value);
      break;
    case PROP_TILE_WIDTH:
      self->params.cp_tdx = g_value_get_int (value);
      self->params.tile_size_on = (self->params.cp_tdx != 0
          && self->params.cp_tdy != 0);
      break;
    case PROP_TILE_HEIGHT:
      self->params.cp_tdy = g_value_get_int (value);
      self->params.tile_size_on = (self->params.cp_tdx != 0
          && self->params.cp_tdy != 0);
      break;
    case PROP_NUM_STRIPES:
      self->num_stripes = g_value_get_int (value);
      break;
    case PROP_NUM_THREADS:
      self->available_threads = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_openjpeg_enc_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstOpenJPEGEnc *self = GST_OPENJPEG_ENC (object);

  switch (prop_id) {
    case PROP_NUM_LAYERS:
      g_value_set_int (value, self->params.tcp_numlayers);
      break;
    case PROP_NUM_RESOLUTIONS:
      g_value_set_int (value, self->params.numresolution);
      break;
    case PROP_PROGRESSION_ORDER:
      g_value_set_enum (value, self->params.prog_order);
      break;
    case PROP_TILE_OFFSET_X:
      g_value_set_int (value, self->params.cp_tx0);
      break;
    case PROP_TILE_OFFSET_Y:
      g_value_set_int (value, self->params.cp_ty0);
      break;
    case PROP_TILE_WIDTH:
      g_value_set_int (value, self->params.cp_tdx);
      break;
    case PROP_TILE_HEIGHT:
      g_value_set_int (value, self->params.cp_tdy);
      break;
    case PROP_NUM_STRIPES:
      g_value_set_int (value, self->num_stripes);
      break;
    case PROP_NUM_THREADS:
      g_value_set_uint (value, self->available_threads);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_openjpeg_enc_start (GstVideoEncoder * encoder)
{
  GstOpenJPEGEnc *self = GST_OPENJPEG_ENC (encoder);

  GST_DEBUG_OBJECT (self, "Starting");
  if (self->available_threads)
    self->encode_frame = gst_openjpeg_enc_encode_frame_multiple;
  else
    self->encode_frame = gst_openjpeg_enc_encode_frame_single;

  return TRUE;
}

static gboolean
gst_openjpeg_enc_stop (GstVideoEncoder * video_encoder)
{
  GstOpenJPEGEnc *self = GST_OPENJPEG_ENC (video_encoder);

  GST_DEBUG_OBJECT (self, "Stopping");

  if (self->output_state) {
    gst_video_codec_state_unref (self->output_state);
    self->output_state = NULL;
  }

  if (self->input_state) {
    gst_video_codec_state_unref (self->input_state);
    self->input_state = NULL;
  }

  GST_DEBUG_OBJECT (self, "Stopped");

  return TRUE;
}

static void
gst_openjpeg_enc_flush_messages (GstOpenJPEGEnc * self)
{
  GstOpenJPEGCodecMessage *enc_params;

  GST_OBJECT_LOCK (self);
  while ((enc_params = g_queue_pop_head (&self->messages))) {
    gst_openjpeg_encode_message_free (enc_params);
  }
  g_cond_broadcast (&self->messages_cond);
  GST_OBJECT_UNLOCK (self);
}

static void
gst_openjpeg_enc_finalize (GObject * object)
{
  GstOpenJPEGEnc *self = GST_OPENJPEG_ENC (object);

  gst_openjpeg_enc_flush_messages (self);
  g_cond_clear (&self->messages_cond);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static GstStateChangeReturn
gst_openjpeg_enc_change_state (GstElement * element, GstStateChange transition)
{
  GstOpenJPEGEnc *self;

  g_return_val_if_fail (GST_IS_OPENJPEG_ENC (element),
      GST_STATE_CHANGE_FAILURE);
  self = GST_OPENJPEG_ENC (element);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      gst_openjpeg_enc_flush_messages (self);
      break;
    default:
      break;
  }

  return GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
}

static guint
get_stripe_height (GstOpenJPEGEnc * self, guint slice_num, guint frame_height)
{
  guint nominal_stripe_height = frame_height / self->num_stripes;
  return (slice_num <
      self->num_stripes -
      1) ? nominal_stripe_height : frame_height -
      (slice_num * nominal_stripe_height);
}

static void
fill_image_packed16_4 (opj_image_t * image, GstVideoFrame * frame)
{
  gint x, y, w, h;
  const guint16 *data_in, *tmp;
  gint *data_out[4];
  gint sstride;

  w = GST_VIDEO_FRAME_WIDTH (frame);
  h = image->y1 - image->y0;
  sstride = GST_VIDEO_FRAME_PLANE_STRIDE (frame, 0) / 2;
  data_in =
      (guint16 *) GST_VIDEO_FRAME_PLANE_DATA (frame, 0) + image->y0 * sstride;

  data_out[0] = image->comps[0].data;
  data_out[1] = image->comps[1].data;
  data_out[2] = image->comps[2].data;
  data_out[3] = image->comps[3].data;

  for (y = 0; y < h; y++) {
    tmp = data_in;

    for (x = 0; x < w; x++) {
      *data_out[3] = tmp[0];
      *data_out[0] = tmp[1];
      *data_out[1] = tmp[2];
      *data_out[2] = tmp[3];

      tmp += 4;
      data_out[0]++;
      data_out[1]++;
      data_out[2]++;
      data_out[3]++;
    }
    data_in += sstride;
  }
}

static void
fill_image_packed8_4 (opj_image_t * image, GstVideoFrame * frame)
{
  gint x, y, w, h;
  const guint8 *data_in, *tmp;
  gint *data_out[4];
  gint sstride;

  w = GST_VIDEO_FRAME_WIDTH (frame);
  h = image->y1 - image->y0;
  sstride = GST_VIDEO_FRAME_PLANE_STRIDE (frame, 0);
  data_in =
      (guint8 *) GST_VIDEO_FRAME_PLANE_DATA (frame, 0) + image->y0 * sstride;

  data_out[0] = image->comps[0].data;
  data_out[1] = image->comps[1].data;
  data_out[2] = image->comps[2].data;
  data_out[3] = image->comps[3].data;

  for (y = 0; y < h; y++) {
    tmp = data_in;

    for (x = 0; x < w; x++) {
      *data_out[3] = tmp[0];
      *data_out[0] = tmp[1];
      *data_out[1] = tmp[2];
      *data_out[2] = tmp[3];

      tmp += 4;
      data_out[0]++;
      data_out[1]++;
      data_out[2]++;
      data_out[3]++;
    }
    data_in += sstride;
  }
}

static void
fill_image_packed8_3 (opj_image_t * image, GstVideoFrame * frame)
{
  gint x, y, w, h;
  const guint8 *data_in, *tmp;
  gint *data_out[3];
  gint sstride;

  w = GST_VIDEO_FRAME_WIDTH (frame);
  h = image->y1 - image->y0;
  sstride = GST_VIDEO_FRAME_PLANE_STRIDE (frame, 0);
  data_in =
      (guint8 *) GST_VIDEO_FRAME_PLANE_DATA (frame, 0) + image->y0 * sstride;

  data_out[0] = image->comps[0].data;
  data_out[1] = image->comps[1].data;
  data_out[2] = image->comps[2].data;

  for (y = 0; y < h; y++) {
    tmp = data_in;

    for (x = 0; x < w; x++) {
      *data_out[0] = tmp[1];
      *data_out[1] = tmp[2];
      *data_out[2] = tmp[3];

      tmp += 4;
      data_out[0]++;
      data_out[1]++;
      data_out[2]++;
    }
    data_in += sstride;
  }
}

static void
fill_image_planar16_3 (opj_image_t * image, GstVideoFrame * frame)
{
  gint c, x, y, w, h;
  const guint16 *data_in, *tmp;
  gint *data_out;
  gint sstride;

  for (c = 0; c < 3; c++) {
    opj_image_comp_t *comp = image->comps + c;

    w = GST_VIDEO_FRAME_COMP_WIDTH (frame, c);
    h = comp->h;
    sstride = GST_VIDEO_FRAME_PLANE_STRIDE (frame, c) / 2;
    data_in =
        (guint16 *) GST_VIDEO_FRAME_COMP_DATA (frame,
        c) + (image->y0 / comp->dy) * sstride;
    data_out = comp->data;

    for (y = 0; y < h; y++) {
      tmp = data_in;
      for (x = 0; x < w; x++) {
        *data_out = *tmp;
        data_out++;
        tmp++;
      }
      data_in += sstride;
    }
  }
}

static void
fill_image_planar8_3 (opj_image_t * image, GstVideoFrame * frame)
{
  gint c, x, y, w, h;
  const guint8 *data_in, *tmp;
  gint *data_out;
  gint sstride;

  for (c = 0; c < 3; c++) {
    opj_image_comp_t *comp = image->comps + c;

    w = GST_VIDEO_FRAME_COMP_WIDTH (frame, c);
    h = comp->h;
    sstride = GST_VIDEO_FRAME_PLANE_STRIDE (frame, c);
    data_in =
        (guint8 *) GST_VIDEO_FRAME_COMP_DATA (frame,
        c) + (image->y0 / comp->dy) * sstride;
    data_out = comp->data;

    for (y = 0; y < h; y++) {
      tmp = data_in;
      for (x = 0; x < w; x++) {
        *data_out = *tmp;
        data_out++;
        tmp++;
      }
      data_in += sstride;
    }
  }
}

static void
fill_image_planar8_1 (opj_image_t * image, GstVideoFrame * frame)
{
  gint x, y, w, h;
  const guint8 *data_in, *tmp;
  gint *data_out;
  gint sstride;
  opj_image_comp_t *comp = image->comps;

  w = GST_VIDEO_FRAME_COMP_WIDTH (frame, 0);
  h = comp->h;
  sstride = GST_VIDEO_FRAME_PLANE_STRIDE (frame, 0);
  data_in =
      (guint8 *) GST_VIDEO_FRAME_COMP_DATA (frame,
      0) + (image->y0 / comp->dy) * sstride;
  data_out = image->comps[0].data;

  for (y = 0; y < h; y++) {
    tmp = data_in;
    for (x = 0; x < w; x++) {
      *data_out = *tmp;
      data_out++;
      tmp++;
    }
    data_in += sstride;
  }
}

static void
fill_image_planar16_1 (opj_image_t * image, GstVideoFrame * frame)
{
  gint x, y, w, h;
  const guint16 *data_in, *tmp;
  gint *data_out;
  gint sstride;
  opj_image_comp_t *comp = image->comps;

  w = GST_VIDEO_FRAME_COMP_WIDTH (frame, 0);
  h = comp->h;
  sstride = GST_VIDEO_FRAME_PLANE_STRIDE (frame, 0) / 2;
  data_in =
      (guint16 *) GST_VIDEO_FRAME_COMP_DATA (frame,
      0) + (image->y0 / comp->dy) * sstride;
  data_out = comp->data;

  for (y = 0; y < h; y++) {
    tmp = data_in;
    for (x = 0; x < w; x++) {
      *data_out = *tmp;
      data_out++;
      tmp++;
    }
    data_in += sstride;
  }
}

static gboolean
gst_openjpeg_enc_set_format (GstVideoEncoder * encoder,
    GstVideoCodecState * state)
{
  GstOpenJPEGEnc *self = GST_OPENJPEG_ENC (encoder);
  GstCaps *allowed_caps, *caps;
  GstStructure *s;
  const gchar *colorspace = NULL;
  GstJPEG2000Sampling sampling = GST_JPEG2000_SAMPLING_NONE;
  gint ncomps;
  gboolean stripe_mode =
      self->num_stripes != GST_OPENJPEG_ENC_DEFAULT_NUM_STRIPES;

  GST_DEBUG_OBJECT (self, "Setting format: %" GST_PTR_FORMAT, state->caps);

  if (self->input_state)
    gst_video_codec_state_unref (self->input_state);
  self->input_state = gst_video_codec_state_ref (state);

  if (stripe_mode) {
    GstCaps *template_caps = gst_caps_new_empty_simple ("image/x-jpc-striped");
    GstCaps *my_caps;

    my_caps = gst_pad_query_caps (GST_VIDEO_ENCODER_SRC_PAD (encoder),
        template_caps);
    gst_caps_unref (template_caps);

    allowed_caps = gst_pad_peer_query_caps (GST_VIDEO_ENCODER_SRC_PAD (encoder),
        my_caps);
    gst_caps_unref (my_caps);

    if (gst_caps_is_empty (allowed_caps)) {
      gst_caps_unref (allowed_caps);
      GST_WARNING_OBJECT (self, "Striped JPEG 2000 not accepted downstream");
      return FALSE;
    }

    self->codec_format = OPJ_CODEC_J2K;
    self->is_jp2c = FALSE;
    allowed_caps = gst_caps_truncate (allowed_caps);
    s = gst_caps_get_structure (allowed_caps, 0);
  } else {
    allowed_caps =
        gst_pad_get_allowed_caps (GST_VIDEO_ENCODER_SRC_PAD (encoder));
    allowed_caps = gst_caps_truncate (allowed_caps);

    s = gst_caps_get_structure (allowed_caps, 0);
    if (gst_structure_has_name (s, "image/jp2")) {
      self->codec_format = OPJ_CODEC_JP2;
      self->is_jp2c = FALSE;
    } else if (gst_structure_has_name (s, "image/x-j2c")) {
      self->codec_format = OPJ_CODEC_J2K;
      self->is_jp2c = TRUE;
    } else if (gst_structure_has_name (s, "image/x-jpc")) {
      self->codec_format = OPJ_CODEC_J2K;
      self->is_jp2c = FALSE;
    } else {
      g_return_val_if_reached (FALSE);
    }
  }

  switch (state->info.finfo->format) {
    case GST_VIDEO_FORMAT_ARGB64:
      self->fill_image = fill_image_packed16_4;
      ncomps = 4;
      break;
    case GST_VIDEO_FORMAT_ARGB:
    case GST_VIDEO_FORMAT_AYUV:
      self->fill_image = fill_image_packed8_4;
      ncomps = 4;
      break;
    case GST_VIDEO_FORMAT_xRGB:
      self->fill_image = fill_image_packed8_3;
      ncomps = 3;
      break;
    case GST_VIDEO_FORMAT_AYUV64:
      self->fill_image = fill_image_packed16_4;
      ncomps = 4;
      break;
    case GST_VIDEO_FORMAT_Y444_10LE:
    case GST_VIDEO_FORMAT_Y444_10BE:
    case GST_VIDEO_FORMAT_I422_10LE:
    case GST_VIDEO_FORMAT_I422_10BE:
    case GST_VIDEO_FORMAT_I420_10LE:
    case GST_VIDEO_FORMAT_I420_10BE:
      self->fill_image = fill_image_planar16_3;
      ncomps = 3;
      break;
    case GST_VIDEO_FORMAT_Y444:
    case GST_VIDEO_FORMAT_Y42B:
    case GST_VIDEO_FORMAT_I420:
    case GST_VIDEO_FORMAT_Y41B:
    case GST_VIDEO_FORMAT_YUV9:
      self->fill_image = fill_image_planar8_3;
      ncomps = 3;
      break;
    case GST_VIDEO_FORMAT_GRAY8:
      self->fill_image = fill_image_planar8_1;
      ncomps = 1;
      break;
    case GST_VIDEO_FORMAT_GRAY16_LE:
    case GST_VIDEO_FORMAT_GRAY16_BE:
      self->fill_image = fill_image_planar16_1;
      ncomps = 1;
      break;
    default:
      g_assert_not_reached ();
  }

  /* sampling */
  /* note: encoder re-orders channels so that alpha channel is encoded as the last channel */
  switch (state->info.finfo->format) {
    case GST_VIDEO_FORMAT_ARGB64:
    case GST_VIDEO_FORMAT_ARGB:
      sampling = GST_JPEG2000_SAMPLING_RGBA;
      break;
    case GST_VIDEO_FORMAT_AYUV64:
    case GST_VIDEO_FORMAT_AYUV:
      sampling = GST_JPEG2000_SAMPLING_YBRA4444_EXT;
      break;
    case GST_VIDEO_FORMAT_xRGB:
      sampling = GST_JPEG2000_SAMPLING_RGB;
      break;
    case GST_VIDEO_FORMAT_Y444_10LE:
    case GST_VIDEO_FORMAT_Y444_10BE:
    case GST_VIDEO_FORMAT_Y444:
      sampling = GST_JPEG2000_SAMPLING_YBR444;
      break;

    case GST_VIDEO_FORMAT_I422_10LE:
    case GST_VIDEO_FORMAT_I422_10BE:
    case GST_VIDEO_FORMAT_Y42B:
      sampling = GST_JPEG2000_SAMPLING_YBR422;
      break;
    case GST_VIDEO_FORMAT_YUV9:
      sampling = GST_JPEG2000_SAMPLING_YBR410;
      break;
    case GST_VIDEO_FORMAT_Y41B:
      sampling = GST_JPEG2000_SAMPLING_YBR411;
      break;
    case GST_VIDEO_FORMAT_I420_10LE:
    case GST_VIDEO_FORMAT_I420_10BE:
    case GST_VIDEO_FORMAT_I420:
      sampling = GST_JPEG2000_SAMPLING_YBR420;
      break;
    case GST_VIDEO_FORMAT_GRAY8:
    case GST_VIDEO_FORMAT_GRAY16_LE:
    case GST_VIDEO_FORMAT_GRAY16_BE:
      sampling = GST_JPEG2000_SAMPLING_GRAYSCALE;
      break;
    default:
      break;
  }

  if ((state->info.finfo->flags & GST_VIDEO_FORMAT_FLAG_YUV)) {
    colorspace = "sYUV";
  } else if ((state->info.finfo->flags & GST_VIDEO_FORMAT_FLAG_RGB)) {
    colorspace = "sRGB";
  } else if ((state->info.finfo->flags & GST_VIDEO_FORMAT_FLAG_GRAY)) {
    colorspace = "GRAY";
  } else
    g_return_val_if_reached (FALSE);

  if (stripe_mode) {
    caps = gst_caps_new_simple ("image/x-jpc-striped",
        "colorspace", G_TYPE_STRING, colorspace,
        "sampling", G_TYPE_STRING, gst_jpeg2000_sampling_to_string (sampling),
        "num-components", G_TYPE_INT, ncomps,
        "num-stripes", G_TYPE_INT, self->num_stripes,
        "stripe-height", G_TYPE_INT,
        get_stripe_height (self, 0,
            GST_VIDEO_INFO_COMP_HEIGHT (&state->info, 0)), NULL);
  } else if (sampling != GST_JPEG2000_SAMPLING_NONE) {
    caps = gst_caps_new_simple (gst_structure_get_name (s),
        "colorspace", G_TYPE_STRING, colorspace,
        "sampling", G_TYPE_STRING, gst_jpeg2000_sampling_to_string (sampling),
        "num-components", G_TYPE_INT, ncomps, NULL);
  } else {
    caps = gst_caps_new_simple (gst_structure_get_name (s),
        "colorspace", G_TYPE_STRING, colorspace,
        "num-components", G_TYPE_INT, ncomps, NULL);
  }
  gst_caps_unref (allowed_caps);

  if (self->output_state)
    gst_video_codec_state_unref (self->output_state);
  self->output_state =
      gst_video_encoder_set_output_state (encoder, caps, state);

  gst_video_encoder_negotiate (GST_VIDEO_ENCODER (encoder));

  return TRUE;
}

static opj_image_t *
gst_openjpeg_enc_fill_image (GstOpenJPEGEnc * self, GstVideoFrame * frame,
    guint slice_num)
{
  gint i, ncomps, temp, min_height = INT_MAX;
  opj_image_cmptparm_t *comps;
  OPJ_COLOR_SPACE colorspace;
  opj_image_t *image;

  ncomps = GST_VIDEO_FRAME_N_COMPONENTS (frame);
  comps = g_new0 (opj_image_cmptparm_t, ncomps);

  for (i = 0; i < ncomps; i++) {
    comps[i].prec = GST_VIDEO_FRAME_COMP_DEPTH (frame, i);
#if (OPJ_VERSION_MAJOR == 2 && OPJ_VERSION_MINOR < 5)
    comps[i].bpp = GST_VIDEO_FRAME_COMP_DEPTH (frame, i);
#endif
    comps[i].sgnd = 0;
    comps[i].w = GST_VIDEO_FRAME_COMP_WIDTH (frame, i);
    comps[i].dx =
        (guint) ((float) GST_VIDEO_FRAME_WIDTH (frame) /
        GST_VIDEO_FRAME_COMP_WIDTH (frame, i) + 0.5f);
    comps[i].dy =
        (guint) ((float) GST_VIDEO_FRAME_HEIGHT (frame) /
        GST_VIDEO_FRAME_COMP_HEIGHT (frame, i) + 0.5f);
    temp =
        (GST_VIDEO_FRAME_COMP_HEIGHT (frame,
            i) / self->num_stripes) * comps[i].dy;
    if (temp < min_height)
      min_height = temp;
  }

  for (i = 0; i < ncomps; i++) {
    gint nominal_height = min_height / comps[i].dy;

    comps[i].h = (slice_num < self->num_stripes) ?
        nominal_height
        : GST_VIDEO_FRAME_COMP_HEIGHT (frame,
        i) - (self->num_stripes - 1) * nominal_height;

  }

  if ((frame->info.finfo->flags & GST_VIDEO_FORMAT_FLAG_YUV))
    colorspace = OPJ_CLRSPC_SYCC;
  else if ((frame->info.finfo->flags & GST_VIDEO_FORMAT_FLAG_RGB))
    colorspace = OPJ_CLRSPC_SRGB;
  else if ((frame->info.finfo->flags & GST_VIDEO_FORMAT_FLAG_GRAY))
    colorspace = OPJ_CLRSPC_GRAY;
  else
    g_return_val_if_reached (NULL);

  image = opj_image_create (ncomps, comps, colorspace);
  if (!image) {
    GST_WARNING_OBJECT (self,
        "Unable to create a JPEG image. first component height=%d",
        ncomps ? comps[0].h : 0);
    return NULL;
  }

  g_free (comps);

  image->x0 = 0;
  image->x1 = GST_VIDEO_FRAME_WIDTH (frame);
  image->y0 = (slice_num - 1) * min_height;
  image->y1 =
      (slice_num <
      self->num_stripes) ? image->y0 +
      min_height : GST_VIDEO_FRAME_HEIGHT (frame);
  self->fill_image (image, frame);

  return image;
}

static void
gst_openjpeg_enc_opj_error (const char *msg, void *userdata)
{
  GstOpenJPEGEnc *self = GST_OPENJPEG_ENC (userdata);
  gchar *trimmed = g_strchomp (g_strdup (msg));
  GST_TRACE_OBJECT (self, "openjpeg error: %s", trimmed);
  g_free (trimmed);
}

static void
gst_openjpeg_enc_opj_warning (const char *msg, void *userdata)
{
  GstOpenJPEGEnc *self = GST_OPENJPEG_ENC (userdata);
  gchar *trimmed = g_strchomp (g_strdup (msg));
  GST_TRACE_OBJECT (self, "openjpeg warning: %s", trimmed);
  g_free (trimmed);
}

static void
gst_openjpeg_enc_opj_info (const char *msg, void *userdata)
{
  GstOpenJPEGEnc *self = GST_OPENJPEG_ENC (userdata);
  gchar *trimmed = g_strchomp (g_strdup (msg));
  GST_TRACE_OBJECT (self, "openjpeg info: %s", trimmed);
  g_free (trimmed);
}

typedef struct
{
  guint8 *data;
  guint allocsize;
  guint offset;
  guint size;
} MemStream;

static OPJ_SIZE_T
read_fn (void *p_buffer, OPJ_SIZE_T p_nb_bytes, void *p_user_data)
{
  g_return_val_if_reached (-1);
}

static OPJ_SIZE_T
write_fn (void *p_buffer, OPJ_SIZE_T p_nb_bytes, void *p_user_data)
{
  MemStream *mstream = p_user_data;

  if (mstream->offset + p_nb_bytes > mstream->allocsize) {
    while (mstream->offset + p_nb_bytes > mstream->allocsize)
      mstream->allocsize *= 2;
    mstream->data = g_realloc (mstream->data, mstream->allocsize);
  }

  memcpy (mstream->data + mstream->offset, p_buffer, p_nb_bytes);

  if (mstream->offset + p_nb_bytes > mstream->size)
    mstream->size = mstream->offset + p_nb_bytes;
  mstream->offset += p_nb_bytes;

  return p_nb_bytes;
}

static OPJ_OFF_T
skip_fn (OPJ_OFF_T p_nb_bytes, void *p_user_data)
{
  MemStream *mstream = p_user_data;

  if (mstream->offset + p_nb_bytes > mstream->allocsize) {
    while (mstream->offset + p_nb_bytes > mstream->allocsize)
      mstream->allocsize *= 2;
    mstream->data = g_realloc (mstream->data, mstream->allocsize);
  }

  if (mstream->offset + p_nb_bytes > mstream->size)
    mstream->size = mstream->offset + p_nb_bytes;

  mstream->offset += p_nb_bytes;

  return p_nb_bytes;
}

static OPJ_BOOL
seek_fn (OPJ_OFF_T p_nb_bytes, void *p_user_data)
{
  MemStream *mstream = p_user_data;

  if (p_nb_bytes > mstream->size)
    return OPJ_FALSE;

  mstream->offset = p_nb_bytes;

  return OPJ_TRUE;
}

static gboolean
gst_openjpeg_encode_is_last_subframe (GstVideoEncoder * enc, int stripe)
{
  GstOpenJPEGEnc *self = GST_OPENJPEG_ENC (enc);

  return (stripe == self->num_stripes);
}

static GstOpenJPEGCodecMessage *
gst_openjpeg_encode_message_new (GstOpenJPEGEnc * self,
    GstVideoCodecFrame * frame, int num_stripe)
{
  GstOpenJPEGCodecMessage *message = g_new0 (GstOpenJPEGCodecMessage, 1);

  message->frame = gst_video_codec_frame_ref (frame);
  message->stripe = num_stripe;
  message->last_error = OPENJPEG_ERROR_NONE;

  return message;
}

static GstOpenJPEGCodecMessage *
gst_openjpeg_encode_message_free (GstOpenJPEGCodecMessage * message)
{
  if (message) {
    gst_video_codec_frame_unref (message->frame);
    if (message->output_buffer)
      gst_buffer_unref (message->output_buffer);
    g_free (message);
  }
  return NULL;
}

#define ENCODE_ERROR(encode_params, err_code) { \
      encode_params->last_error = err_code; \
      goto done; \
}

/* callback method to be called asynchronously or not*/
static void
gst_openjpeg_enc_encode_stripe (GstElement * element, gpointer user_data)
{
  GstOpenJPEGCodecMessage *message = (GstOpenJPEGCodecMessage *) user_data;
  GstOpenJPEGEnc *self = GST_OPENJPEG_ENC (element);
  opj_codec_t *enc = NULL;
  opj_stream_t *stream = NULL;
  MemStream mstream;
  opj_image_t *image = NULL;
  GstVideoFrame vframe;

  GST_INFO_OBJECT (self, "Encode stripe %d/%d", message->stripe,
      self->num_stripes);

  mstream.data = NULL;
  enc = opj_create_compress (self->codec_format);
  if (!enc)
    ENCODE_ERROR (message, OPENJPEG_ERROR_INIT);

  if (G_UNLIKELY (gst_debug_category_get_threshold (GST_CAT_DEFAULT) >=
          GST_LEVEL_TRACE)) {
    opj_set_info_handler (enc, gst_openjpeg_enc_opj_info, self);
    opj_set_warning_handler (enc, gst_openjpeg_enc_opj_warning, self);
    opj_set_error_handler (enc, gst_openjpeg_enc_opj_error, self);
  } else {
    opj_set_info_handler (enc, NULL, NULL);
    opj_set_warning_handler (enc, NULL, NULL);
    opj_set_error_handler (enc, NULL, NULL);
  }
  if (!gst_video_frame_map (&vframe, &self->input_state->info,
          message->frame->input_buffer, GST_MAP_READ))
    ENCODE_ERROR (message, OPENJPEG_ERROR_MAP_READ);
  image = gst_openjpeg_enc_fill_image (self, &vframe, message->stripe);
  gst_video_frame_unmap (&vframe);
  if (!image)
    ENCODE_ERROR (message, OPENJPEG_ERROR_FILL_IMAGE);

  if (vframe.info.finfo->flags & GST_VIDEO_FORMAT_FLAG_RGB) {
    self->params.tcp_mct = 1;
  }
  opj_setup_encoder (enc, &self->params, image);
  stream = opj_stream_create (4096, OPJ_FALSE);
  if (!stream)
    ENCODE_ERROR (message, OPENJPEG_ERROR_OPEN);

  mstream.allocsize = 4096;
  mstream.data = g_malloc (mstream.allocsize);
  mstream.offset = 0;
  mstream.size = 0;

  opj_stream_set_read_function (stream, read_fn);
  opj_stream_set_write_function (stream, write_fn);
  opj_stream_set_skip_function (stream, skip_fn);
  opj_stream_set_seek_function (stream, seek_fn);
  opj_stream_set_user_data (stream, &mstream, NULL);
  opj_stream_set_user_data_length (stream, mstream.size);

  if (!opj_start_compress (enc, image, stream))
    ENCODE_ERROR (message, OPENJPEG_ERROR_ENCODE);

  if (!opj_encode (enc, stream))
    ENCODE_ERROR (message, OPENJPEG_ERROR_ENCODE);

  if (!opj_end_compress (enc, stream))
    ENCODE_ERROR (message, OPENJPEG_ERROR_ENCODE);

  opj_image_destroy (image);
  image = NULL;
  opj_stream_destroy (stream);
  stream = NULL;
  opj_destroy_codec (enc);
  enc = NULL;

  message->output_buffer = gst_buffer_new ();

  if (self->is_jp2c) {
    GstMapInfo map;
    GstMemory *mem;

    mem = gst_allocator_alloc (NULL, 8, NULL);
    gst_memory_map (mem, &map, GST_MAP_WRITE);
    GST_WRITE_UINT32_BE (map.data, mstream.size + 8);
    GST_WRITE_UINT32_BE (map.data + 4, GST_MAKE_FOURCC ('j', 'p', '2', 'c'));
    gst_memory_unmap (mem, &map);
    gst_buffer_append_memory (message->output_buffer, mem);
  }

  gst_buffer_append_memory (message->output_buffer,
      gst_memory_new_wrapped (0, mstream.data, mstream.allocsize, 0,
          mstream.size, mstream.data, (GDestroyNotify) g_free));
  message->last_error = OPENJPEG_ERROR_NONE;

  GST_INFO_OBJECT (self,
      "Stripe %d encoded successfully, pass it to the streaming thread",
      message->stripe);

done:
  if (message->last_error != OPENJPEG_ERROR_NONE) {
    if (mstream.data)
      g_free (mstream.data);
    if (enc)
      opj_destroy_codec (enc);
    if (image)
      opj_image_destroy (image);
    if (stream)
      opj_stream_destroy (stream);
  }
  if (!message->direct) {
    GST_OBJECT_LOCK (self);
    g_queue_push_tail (&self->messages, message);
    g_cond_signal (&self->messages_cond);
    GST_OBJECT_UNLOCK (self);
  }

}

static GstOpenJPEGCodecMessage *
gst_openjpeg_enc_wait_for_new_message (GstOpenJPEGEnc * self)
{
  GstOpenJPEGCodecMessage *message = NULL;

  GST_OBJECT_LOCK (self);
  while (g_queue_is_empty (&self->messages))
    g_cond_wait (&self->messages_cond, GST_OBJECT_GET_LOCK (self));
  message = g_queue_pop_head (&self->messages);
  GST_OBJECT_UNLOCK (self);

  return message;
}

static GstFlowReturn
gst_openjpeg_enc_encode_frame_multiple (GstVideoEncoder * encoder,
    GstVideoCodecFrame * frame)
{
  GstOpenJPEGEnc *self = GST_OPENJPEG_ENC (encoder);
  GstFlowReturn ret = GST_FLOW_OK;
  guint i;
  guint encoded_stripes = 0;
  guint enqueued_stripes = 0;
  GstOpenJPEGCodecMessage *message = NULL;

  /* The method receives a frame and split it into n stripes and
   * and create a thread per stripe to encode it.
   * As the number of stripes can be greater than the
   * available threads to encode, there is two loop, one to
   * count the enqueues stripes and one to count the encoded
   * stripes.
   */
  while (encoded_stripes < self->num_stripes) {
    for (i = 1;
        i <= self->available_threads
        && enqueued_stripes < (self->num_stripes - encoded_stripes); i++) {
      message =
          gst_openjpeg_encode_message_new (self, frame, i + encoded_stripes);
      GST_LOG_OBJECT (self,
          "About to enqueue an encoding message from frame %p stripe %d", frame,
          message->stripe);
      gst_element_call_async (GST_ELEMENT (self),
          (GstElementCallAsyncFunc) gst_openjpeg_enc_encode_stripe, message,
          NULL);
      enqueued_stripes++;
    }
    while (enqueued_stripes > 0) {
      message = gst_openjpeg_enc_wait_for_new_message (self);
      if (!message)
        continue;
      enqueued_stripes--;
      if (message->last_error == OPENJPEG_ERROR_NONE) {
        GST_LOG_OBJECT (self,
            "About to push frame %p stripe %d", frame, message->stripe);
        frame->output_buffer = gst_buffer_ref (message->output_buffer);
        if (gst_openjpeg_encode_is_last_subframe (encoder, encoded_stripes + 1)) {
          GST_VIDEO_CODEC_FRAME_SET_SYNC_POINT (frame);
          ret = gst_video_encoder_finish_frame (encoder, frame);
        } else
          ret = gst_video_encoder_finish_subframe (encoder, frame);
        if (ret != GST_FLOW_OK) {
          GST_WARNING_OBJECT
              (self, "An error occurred pushing the frame %s",
              gst_flow_get_name (ret));
          goto done;
        }
        encoded_stripes++;
        message = gst_openjpeg_encode_message_free (message);
      } else {
        GST_WARNING_OBJECT
            (self, "An error occurred %d during the JPEG encoding",
            message->last_error);
        gst_video_codec_frame_unref (frame);
        self->last_error = message->last_error;
        ret = GST_FLOW_ERROR;
        goto done;
      }
    }
  }

done:
  gst_openjpeg_encode_message_free (message);
  return ret;
}

static GstFlowReturn
gst_openjpeg_enc_encode_frame_single (GstVideoEncoder * encoder,
    GstVideoCodecFrame * frame)
{
  GstOpenJPEGEnc *self = GST_OPENJPEG_ENC (encoder);
  GstFlowReturn ret = GST_FLOW_OK;
  guint i;
  GstOpenJPEGCodecMessage *message = NULL;

  for (i = 1; i <= self->num_stripes; ++i) {
    message = gst_openjpeg_encode_message_new (self, frame, i);
    message->direct = TRUE;
    gst_openjpeg_enc_encode_stripe (GST_ELEMENT (self), message);
    if (message->last_error != OPENJPEG_ERROR_NONE) {
      GST_WARNING_OBJECT
          (self, "An error occured %d during the JPEG encoding",
          message->last_error);
      gst_video_codec_frame_unref (frame);
      self->last_error = message->last_error;
      ret = GST_FLOW_ERROR;
      goto done;
    }
    frame->output_buffer = gst_buffer_ref (message->output_buffer);
    if (gst_openjpeg_encode_is_last_subframe (encoder, message->stripe)) {
      GST_VIDEO_CODEC_FRAME_SET_SYNC_POINT (frame);
      ret = gst_video_encoder_finish_frame (encoder, frame);
    } else
      ret = gst_video_encoder_finish_subframe (encoder, frame);
    if (ret != GST_FLOW_OK) {
      GST_WARNING_OBJECT
          (self, "An error occurred pushing the frame %s",
          gst_flow_get_name (ret));
      goto done;
    }
    message = gst_openjpeg_encode_message_free (message);
  }

done:
  gst_openjpeg_encode_message_free (message);
  return ret;
}

static GstFlowReturn
gst_openjpeg_enc_handle_frame (GstVideoEncoder * encoder,
    GstVideoCodecFrame * frame)
{
  GstOpenJPEGEnc *self = GST_OPENJPEG_ENC (encoder);
  GstFlowReturn ret = GST_FLOW_OK;
  GstVideoFrame vframe;
  gboolean subframe_mode =
      self->num_stripes != GST_OPENJPEG_ENC_DEFAULT_NUM_STRIPES;

  GST_DEBUG_OBJECT (self, "Handling frame");

  if (subframe_mode) {
    gint min_res;

    /* due to limitations in openjpeg library,
     * number of wavelet resolutions must not exceed floor(log(stripe height)) + 1 */
    if (!gst_video_frame_map (&vframe, &self->input_state->info,
            frame->input_buffer, GST_MAP_READ)) {
      gst_video_codec_frame_unref (frame);
      self->last_error = OPENJPEG_ERROR_MAP_READ;
      goto error;
    }
    /* find stripe with least height */
    min_res =
        get_stripe_height (self, self->num_stripes - 1,
        GST_VIDEO_FRAME_COMP_HEIGHT (&vframe, 0));
    min_res = MIN (min_res, get_stripe_height (self, 0,
            GST_VIDEO_FRAME_COMP_HEIGHT (&vframe, 0)));
    /* take log to find correct number of wavelet resolutions */
    min_res = min_res > 1 ? (gint) log (min_res) + 1 : 1;
    self->params.numresolution = MIN (min_res + 1, self->params.numresolution);
    gst_video_frame_unmap (&vframe);
  }
  if (self->encode_frame (encoder, frame) != GST_FLOW_OK)
    goto error;

  return ret;

error:
  switch (self->last_error) {
    case OPENJPEG_ERROR_INIT:
      GST_ELEMENT_ERROR (self, LIBRARY, INIT,
          ("Failed to initialize OpenJPEG encoder"), (NULL));
      break;
    case OPENJPEG_ERROR_MAP_READ:
      GST_ELEMENT_ERROR (self, CORE, FAILED,
          ("Failed to map input buffer"), (NULL));
      break;
    case OPENJPEG_ERROR_FILL_IMAGE:
      GST_ELEMENT_ERROR (self, LIBRARY, INIT,
          ("Failed to fill OpenJPEG image"), (NULL));
      break;
    case OPENJPEG_ERROR_OPEN:
      GST_ELEMENT_ERROR (self, LIBRARY, INIT,
          ("Failed to open OpenJPEG data"), (NULL));
      break;
    case OPENJPEG_ERROR_ENCODE:
      GST_ELEMENT_ERROR (self, LIBRARY, INIT,
          ("Failed to encode OpenJPEG data"), (NULL));
      break;
    default:
      GST_ELEMENT_ERROR (self, LIBRARY, INIT,
          ("Failed to encode OpenJPEG data"), (NULL));
      break;
  }
  gst_openjpeg_enc_flush_messages (self);
  return GST_FLOW_ERROR;
}

static gboolean
gst_openjpeg_enc_propose_allocation (GstVideoEncoder * encoder,
    GstQuery * query)
{
  gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL);

  return GST_VIDEO_ENCODER_CLASS (parent_class)->propose_allocation (encoder,
      query);
}
