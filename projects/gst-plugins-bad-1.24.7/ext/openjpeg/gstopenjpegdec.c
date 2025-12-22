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
 * SECTION:element-openjpegdec
 * @title: openjpegdec
 * @see_also: openjpegenc
 *
 * openjpegdec decodes openjpeg stream.
 *
 * ## Example launch lines
 * |[
 * gst-launch-1.0 -v videotestsrc num-buffers=10 ! openjpegenc ! jpeg2000parse ! openjpegdec ! videoconvert ! autovideosink sync=false
 * ]| Encode and decode whole frames.
 * |[
 * gst-launch-1.0 -v videotestsrc num-buffers=10 ! openjpegenc num-threads=8 num-stripes=8 ! jpeg2000parse ! openjpegdec max-slice-threads=8 ! videoconvert ! autovideosink sync=fals
 * ]| Encode and decode frame split with stripes.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstopenjpegdec.h"


#include <string.h>

GST_DEBUG_CATEGORY_STATIC (gst_openjpeg_dec_debug);
#define GST_CAT_DEFAULT gst_openjpeg_dec_debug

enum
{
  PROP_0,
  PROP_MAX_THREADS,
  PROP_MAX_SLICE_THREADS,
  PROP_LAST
};

#define GST_OPENJPEG_DEC_DEFAULT_MAX_THREADS		0

/* prototypes */
static void gst_openjpeg_dec_finalize (GObject * object);

static GstStateChangeReturn
gst_openjpeg_dec_change_state (GstElement * element, GstStateChange transition);

static gboolean gst_openjpeg_dec_start (GstVideoDecoder * decoder);
static gboolean gst_openjpeg_dec_stop (GstVideoDecoder * decoder);
static gboolean gst_openjpeg_dec_set_format (GstVideoDecoder * decoder,
    GstVideoCodecState * state);
static gboolean gst_openjpeg_dec_flush (GstVideoDecoder * decoder);
static GstFlowReturn gst_openjpeg_dec_finish (GstVideoDecoder * decoder);
static GstFlowReturn gst_openjpeg_dec_handle_frame (GstVideoDecoder * decoder,
    GstVideoCodecFrame * frame);
static gboolean gst_openjpeg_dec_decide_allocation (GstVideoDecoder * decoder,
    GstQuery * query);
static void gst_openjpeg_dec_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_openjpeg_dec_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);

static GstFlowReturn gst_openjpeg_dec_decode_frame_multiple (GstVideoDecoder *
    decoder, GstVideoCodecFrame * frame);
static GstFlowReturn gst_openjpeg_dec_decode_frame_single (GstVideoDecoder *
    decoder, GstVideoCodecFrame * frame);

static void gst_openjpeg_dec_pause_loop (GstOpenJPEGDec * self,
    GstFlowReturn flow_ret);

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
#define GRAY16 "GRAY16_LE"
#define YUV10 "Y444_10LE, I422_10LE, I420_10LE"
#else
#define GRAY16 "GRAY16_BE"
#define YUV10 "Y444_10BE, I422_10BE, I420_10BE"
#endif

static GstStaticPadTemplate gst_openjpeg_dec_sink_template =
    GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("image/x-j2c, "
        GST_JPEG2000_SAMPLING_LIST "; "
        "image/x-jpc,"
        GST_JPEG2000_SAMPLING_LIST "; image/jp2 ; "
        "image/x-jpc-striped, "
        "num-stripes = (int) [2, MAX], " GST_JPEG2000_SAMPLING_LIST)
    );

static GstStaticPadTemplate gst_openjpeg_dec_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("{ ARGB64, ARGB, xRGB, "
            "AYUV64, " YUV10 ", "
            "AYUV, Y444, Y42B, I420, Y41B, YUV9, " "GRAY8, " GRAY16 " }"))
    );

#define parent_class gst_openjpeg_dec_parent_class
G_DEFINE_TYPE (GstOpenJPEGDec, gst_openjpeg_dec, GST_TYPE_VIDEO_DECODER);
GST_ELEMENT_REGISTER_DEFINE (openjpegdec, "openjpegdec",
    GST_RANK_PRIMARY, GST_TYPE_OPENJPEG_DEC);

static void
gst_openjpeg_dec_class_init (GstOpenJPEGDecClass * klass)
{
  GstElementClass *element_class;
  GstVideoDecoderClass *video_decoder_class;
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  element_class = (GstElementClass *) klass;
  video_decoder_class = (GstVideoDecoderClass *) klass;

  gst_element_class_add_static_pad_template (element_class,
      &gst_openjpeg_dec_src_template);
  gst_element_class_add_static_pad_template (element_class,
      &gst_openjpeg_dec_sink_template);

  gst_element_class_set_static_metadata (element_class,
      "OpenJPEG JPEG2000 decoder",
      "Codec/Decoder/Video",
      "Decode JPEG2000 streams",
      "Sebastian Dröge <sebastian.droege@collabora.co.uk>");

  element_class->change_state =
      GST_DEBUG_FUNCPTR (gst_openjpeg_dec_change_state);

  video_decoder_class->start = GST_DEBUG_FUNCPTR (gst_openjpeg_dec_start);
  video_decoder_class->stop = GST_DEBUG_FUNCPTR (gst_openjpeg_dec_stop);
  video_decoder_class->flush = GST_DEBUG_FUNCPTR (gst_openjpeg_dec_flush);
  video_decoder_class->finish = GST_DEBUG_FUNCPTR (gst_openjpeg_dec_finish);
  video_decoder_class->set_format =
      GST_DEBUG_FUNCPTR (gst_openjpeg_dec_set_format);
  video_decoder_class->handle_frame =
      GST_DEBUG_FUNCPTR (gst_openjpeg_dec_handle_frame);
  video_decoder_class->decide_allocation = gst_openjpeg_dec_decide_allocation;
  gobject_class->set_property = gst_openjpeg_dec_set_property;
  gobject_class->get_property = gst_openjpeg_dec_get_property;
  gobject_class->finalize = gst_openjpeg_dec_finalize;

  /**
   * GstOpenJPEGDec:max-slice-threads:
   *
   * Maximum number of worker threads to spawn. (0 = auto)
   *
   * Since: 1.20
   */
  g_object_class_install_property (G_OBJECT_CLASS (klass),
      PROP_MAX_SLICE_THREADS, g_param_spec_int ("max-slice-threads",
          "Maximum slice decoding threads",
          "Maximum number of worker threads to spawn according to the frame boundary. (0 = no thread)",
          0, G_MAXINT, GST_OPENJPEG_DEC_DEFAULT_MAX_THREADS,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  /**
   * GstOpenJPEGDec:max-threads:
   *
   * Maximum number of worker threads to spawn used by openjpeg internally. (0 = no thread)
   *
   * Since: 1.18
   */
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_MAX_THREADS,
      g_param_spec_int ("max-threads", "Maximum openjpeg threads",
          "Maximum number of worker threads to spawn used by openjpeg internally. (0 = no thread)",
          0, G_MAXINT, GST_OPENJPEG_DEC_DEFAULT_MAX_THREADS,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  GST_DEBUG_CATEGORY_INIT (gst_openjpeg_dec_debug, "openjpegdec", 0,
      "OpenJPEG Decoder");
}

static void
gst_openjpeg_dec_init (GstOpenJPEGDec * self)
{
  GstVideoDecoder *decoder = (GstVideoDecoder *) self;

  gst_video_decoder_set_packetized (decoder, TRUE);
  gst_video_decoder_set_needs_format (decoder, TRUE);
  gst_video_decoder_set_use_default_pad_acceptcaps (GST_VIDEO_DECODER_CAST
      (self), TRUE);
  GST_PAD_SET_ACCEPT_TEMPLATE (GST_VIDEO_DECODER_SINK_PAD (self));
  opj_set_default_decoder_parameters (&self->params);
  self->sampling = GST_JPEG2000_SAMPLING_NONE;
  self->max_slice_threads = GST_OPENJPEG_DEC_DEFAULT_MAX_THREADS;
  self->available_threads = GST_OPENJPEG_DEC_DEFAULT_MAX_THREADS;
  self->num_procs = g_get_num_processors ();
  g_mutex_init (&self->messages_lock);
  g_mutex_init (&self->decoding_lock);
  g_cond_init (&self->messages_cond);
  g_queue_init (&self->messages);
}

static gboolean
gst_openjpeg_dec_start (GstVideoDecoder * decoder)
{
  GstOpenJPEGDec *self = GST_OPENJPEG_DEC (decoder);

  GST_DEBUG_OBJECT (self, "Starting");
  self->available_threads = self->max_slice_threads;
  self->decode_frame = gst_openjpeg_dec_decode_frame_single;
  if (self->available_threads) {
    if (gst_video_decoder_get_subframe_mode (decoder))
      self->decode_frame = gst_openjpeg_dec_decode_frame_multiple;
    else
      GST_INFO_OBJECT (self,
          "Multiple threads decoding only available in subframe mode.");
  }

  return TRUE;
}

static gboolean
gst_openjpeg_dec_stop (GstVideoDecoder * video_decoder)
{
  GstOpenJPEGDec *self = GST_OPENJPEG_DEC (video_decoder);

  GST_DEBUG_OBJECT (self, "Stopping");
  g_mutex_lock (&self->messages_lock);
  gst_pad_stop_task (GST_VIDEO_DECODER_SRC_PAD (video_decoder));

  if (self->output_state) {
    gst_video_codec_state_unref (self->output_state);
    self->output_state = NULL;
  }

  if (self->input_state) {
    gst_video_codec_state_unref (self->input_state);
    self->input_state = NULL;
  }
  g_mutex_unlock (&self->messages_lock);
  GST_DEBUG_OBJECT (self, "Stopped");

  return TRUE;
}

static void
gst_openjpeg_dec_finalize (GObject * object)
{
  GstOpenJPEGDec *self = GST_OPENJPEG_DEC (object);

  g_mutex_clear (&self->messages_lock);
  g_mutex_clear (&self->decoding_lock);
  g_cond_clear (&self->messages_cond);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static GstStateChangeReturn
gst_openjpeg_dec_change_state (GstElement * element, GstStateChange transition)
{
  GstOpenJPEGDec *self;
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;

  g_return_val_if_fail (GST_IS_OPENJPEG_DEC (element),
      GST_STATE_CHANGE_FAILURE);
  self = GST_OPENJPEG_DEC (element);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      self->draining = FALSE;
      self->started = FALSE;
      self->flushing = FALSE;
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      self->flushing = TRUE;
      g_mutex_lock (&self->drain_lock);
      self->draining = FALSE;
      g_cond_broadcast (&self->drain_cond);
      g_mutex_unlock (&self->drain_lock);
      break;
    default:
      break;
  }

  ret =
      GST_ELEMENT_CLASS (gst_openjpeg_dec_parent_class)->change_state
      (element, transition);

  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      self->started = FALSE;
      self->downstream_flow_ret = GST_FLOW_FLUSHING;
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      break;
    default:
      break;
  }

  return ret;
}

static void
gst_openjpeg_dec_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GstOpenJPEGDec *dec = (GstOpenJPEGDec *) object;

  switch (prop_id) {
    case PROP_MAX_SLICE_THREADS:
      g_atomic_int_set (&dec->max_slice_threads, g_value_get_int (value));
      break;
    case PROP_MAX_THREADS:
      g_atomic_int_set (&dec->max_threads, g_value_get_int (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_openjpeg_dec_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstOpenJPEGDec *dec = (GstOpenJPEGDec *) object;

  switch (prop_id) {
    case PROP_MAX_SLICE_THREADS:
      g_value_set_int (value, g_atomic_int_get (&dec->max_slice_threads));
      break;
    case PROP_MAX_THREADS:
      g_value_set_int (value, g_atomic_int_get (&dec->max_threads));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_openjpeg_dec_set_format (GstVideoDecoder * decoder,
    GstVideoCodecState * state)
{
  GstOpenJPEGDec *self = GST_OPENJPEG_DEC (decoder);
  GstStructure *s;

  GST_DEBUG_OBJECT (self, "Setting format: %" GST_PTR_FORMAT, state->caps);

  s = gst_caps_get_structure (state->caps, 0);

  self->color_space = OPJ_CLRSPC_UNKNOWN;

  if (gst_structure_has_name (s, "image/jp2")) {
    self->codec_format = OPJ_CODEC_JP2;
    self->is_jp2c = FALSE;
  } else if (gst_structure_has_name (s, "image/x-j2c")) {
    self->codec_format = OPJ_CODEC_J2K;
    self->is_jp2c = TRUE;
  } else if (gst_structure_has_name (s, "image/x-jpc") ||
      gst_structure_has_name (s, "image/x-jpc-striped")) {
    self->codec_format = OPJ_CODEC_J2K;
    self->is_jp2c = FALSE;
  } else {
    g_return_val_if_reached (FALSE);
  }

  if (gst_structure_has_name (s, "image/x-jpc-striped")) {
    gst_structure_get_int (s, "num-stripes", &self->num_stripes);
    gst_video_decoder_set_subframe_mode (decoder, TRUE);
  } else {
    self->num_stripes = 1;
    gst_video_decoder_set_subframe_mode (decoder, FALSE);
  }

  self->sampling =
      gst_jpeg2000_sampling_from_string (gst_structure_get_string (s,
          "sampling"));
  if (gst_jpeg2000_sampling_is_rgb (self->sampling))
    self->color_space = OPJ_CLRSPC_SRGB;
  else if (gst_jpeg2000_sampling_is_mono (self->sampling))
    self->color_space = OPJ_CLRSPC_GRAY;
  else if (gst_jpeg2000_sampling_is_yuv (self->sampling))
    self->color_space = OPJ_CLRSPC_SYCC;

  self->ncomps = 0;
  gst_structure_get_int (s, "num-components", &self->ncomps);

  if (self->input_state)
    gst_video_codec_state_unref (self->input_state);
  self->input_state = gst_video_codec_state_ref (state);

  return TRUE;
}

static gboolean
reverse_rgb_channels (GstJPEG2000Sampling sampling)
{
  return sampling == GST_JPEG2000_SAMPLING_BGR
      || sampling == GST_JPEG2000_SAMPLING_BGRA;
}

static void
fill_frame_packed8_4 (GstOpenJPEGDec * self, GstVideoFrame * frame,
    opj_image_t * image)
{
  gint x, y, y0, y1, w, c;
  guint8 *data_out, *tmp;
  const gint *data_in[4];
  gint dstride;
  gint off[4];

  w = GST_VIDEO_FRAME_WIDTH (frame);
  data_out = GST_VIDEO_FRAME_PLANE_DATA (frame, 0);
  dstride = GST_VIDEO_FRAME_PLANE_STRIDE (frame, 0);

  for (c = 0; c < 4; c++) {
    data_in[c] = image->comps[c].data;
    off[c] = 0x80 * image->comps[c].sgnd;
  }

  /* copy only the stripe content (image) to the full size frame */
  y0 = image->y0;
  y1 = image->y1;
  GST_DEBUG_OBJECT (self, "yo=%d y1=%d", y0, y1);
  data_out += y0 * dstride;
  for (y = y0; y < y1; y++) {
    tmp = data_out;
    for (x = 0; x < w; x++) {
      /* alpha, from 4'th input channel */
      tmp[0] = off[3] + *data_in[3];
      /* colour channels */
      tmp[1] = off[0] + *data_in[0];
      tmp[2] = off[1] + *data_in[1];
      tmp[3] = off[2] + *data_in[2];

      tmp += 4;
      data_in[0]++;
      data_in[1]++;
      data_in[2]++;
      data_in[3]++;
    }
    data_out += dstride;
  }
}

static void
fill_frame_packed16_4 (GstOpenJPEGDec * self, GstVideoFrame * frame,
    opj_image_t * image)
{
  gint x, y, y0, y1, w, c;
  guint16 *data_out, *tmp;
  const gint *data_in[4];
  gint dstride;
  gint shift[4], off[4];

  w = GST_VIDEO_FRAME_WIDTH (frame);
  data_out = GST_VIDEO_FRAME_PLANE_DATA (frame, 0);
  dstride = GST_VIDEO_FRAME_PLANE_STRIDE (frame, 0) / 2;

  for (c = 0; c < 4; c++) {
    data_in[c] = image->comps[c].data;
    off[c] = (1 << (image->comps[c].prec - 1)) * image->comps[c].sgnd;
    shift[c] =
        MAX (MIN (GST_VIDEO_FRAME_COMP_DEPTH (frame, c) - image->comps[c].prec,
            8), 0);
  }

  y0 = image->y0;
  y1 = image->y1;
  data_out += y0 * dstride;
  for (y = y0; y < y1; y++) {
    tmp = data_out;
    for (x = 0; x < w; x++) {
      /* alpha, from 4'th input channel */
      tmp[0] = off[3] + (*data_in[3] << shift[3]);
      /* colour channels */
      tmp[1] = off[0] + (*data_in[0] << shift[0]);
      tmp[2] = off[1] + (*data_in[1] << shift[1]);
      tmp[3] = off[2] + (*data_in[2] << shift[2]);

      tmp += 4;
      data_in[0]++;
      data_in[1]++;
      data_in[2]++;
      data_in[3]++;
    }
    data_out += dstride;
  }
}

static void
fill_frame_packed8_3 (GstOpenJPEGDec * self, GstVideoFrame * frame,
    opj_image_t * image)
{
  gint x, y, y0, y1, w, c;
  guint8 *data_out, *tmp;
  const gint *data_in[3];
  gint dstride;
  gint off[3];

  w = GST_VIDEO_FRAME_WIDTH (frame);
  data_out = GST_VIDEO_FRAME_PLANE_DATA (frame, 0);
  dstride = GST_VIDEO_FRAME_PLANE_STRIDE (frame, 0);

  for (c = 0; c < 3; c++) {
    data_in[c] = image->comps[c].data;
    off[c] = 0x80 * image->comps[c].sgnd;
  };
  y0 = image->y0;
  y1 = image->y1;
  data_out += y0 * dstride;
  for (y = y0; y < y1; y++) {
    tmp = data_out;
    for (x = 0; x < w; x++) {
      tmp[0] = off[0] + *data_in[0];
      tmp[1] = off[1] + *data_in[1];
      tmp[2] = off[2] + *data_in[2];
      data_in[0]++;
      data_in[1]++;
      data_in[2]++;
      tmp += 3;
    }
    data_out += dstride;
  }
}

static void
fill_frame_packed16_3 (GstOpenJPEGDec * self, GstVideoFrame * frame,
    opj_image_t * image)
{
  gint x, y, y0, y1, w, c;
  guint16 *data_out, *tmp;
  const gint *data_in[3];
  gint dstride;
  gint shift[3], off[3];

  w = GST_VIDEO_FRAME_WIDTH (frame);
  data_out = GST_VIDEO_FRAME_PLANE_DATA (frame, 0);
  dstride = GST_VIDEO_FRAME_PLANE_STRIDE (frame, 0) / 2;

  for (c = 0; c < 3; c++) {
    data_in[c] = image->comps[c].data;
    off[c] = (1 << (image->comps[c].prec - 1)) * image->comps[c].sgnd;
    shift[c] =
        MAX (MIN (GST_VIDEO_FRAME_COMP_DEPTH (frame, c) - image->comps[c].prec,
            8), 0);
  }

  y0 = image->y0;
  y1 = image->y1;
  data_out += y0 * dstride;
  for (y = y0; y < y1; y++) {
    tmp = data_out;
    for (x = 0; x < w; x++) {
      tmp[1] = off[0] + (*data_in[0] << shift[0]);
      tmp[2] = off[1] + (*data_in[1] << shift[1]);
      tmp[3] = off[2] + (*data_in[2] << shift[2]);

      tmp += 4;
      data_in[0]++;
      data_in[1]++;
      data_in[2]++;
    }
    data_out += dstride;
  }
}

/* for grayscale with alpha */
static void
fill_frame_packed8_2 (GstOpenJPEGDec * self, GstVideoFrame * frame,
    opj_image_t * image)
{
  gint x, y, y0, y1, w, c;
  guint8 *data_out, *tmp;
  const gint *data_in[2];
  gint dstride;
  gint off[2];

  w = GST_VIDEO_FRAME_WIDTH (frame);
  data_out = GST_VIDEO_FRAME_PLANE_DATA (frame, 0);
  dstride = GST_VIDEO_FRAME_PLANE_STRIDE (frame, 0);

  for (c = 0; c < 2; c++) {
    data_in[c] = image->comps[c].data;
    off[c] = 0x80 * image->comps[c].sgnd;
  };

  y0 = image->y0;
  y1 = image->y1;
  data_out += y0 * dstride;
  for (y = y0; y < y1; y++) {
    tmp = data_out;
    for (x = 0; x < w; x++) {
      /* alpha, from 2nd input channel */
      tmp[0] = off[1] + *data_in[1];
      /* luminance, from first input channel */
      tmp[1] = off[0] + *data_in[0];
      tmp[2] = tmp[1];
      tmp[3] = tmp[1];
      data_in[0]++;
      data_in[1]++;
      tmp += 4;
    }
    data_out += dstride;
  }
}

/* for grayscale with alpha */
static void
fill_frame_packed16_2 (GstOpenJPEGDec * self, GstVideoFrame * frame,
    opj_image_t * image)
{
  gint x, y, y0, y1, w, c;
  guint16 *data_out, *tmp;
  const gint *data_in[2];
  gint dstride;
  gint shift[2], off[2];

  w = GST_VIDEO_FRAME_WIDTH (frame);
  data_out = GST_VIDEO_FRAME_PLANE_DATA (frame, 0);
  dstride = GST_VIDEO_FRAME_PLANE_STRIDE (frame, 0) / 2;

  for (c = 0; c < 2; c++) {
    data_in[c] = image->comps[c].data;
    off[c] = (1 << (image->comps[c].prec - 1)) * image->comps[c].sgnd;
    shift[c] =
        MAX (MIN (GST_VIDEO_FRAME_COMP_DEPTH (frame, c) - image->comps[c].prec,
            8), 0);
  }

  y0 = image->y0;
  y1 = image->y1;
  data_out += y0 * dstride;
  for (y = y0; y < y1; y++) {
    tmp = data_out;
    for (x = 0; x < w; x++) {
      /* alpha, from 2nd input channel */
      tmp[0] = off[1] + (*data_in[1] << shift[1]);
      /* luminance, from first input channel  */
      tmp[1] = off[0] + (*data_in[0] << shift[0]);
      tmp[2] = tmp[1];
      tmp[3] = tmp[1];
      tmp += 4;
      data_in[0]++;
      data_in[1]++;
    }
    data_out += dstride;
  }
}


static void
fill_frame_planar8_1 (GstOpenJPEGDec * self, GstVideoFrame * frame,
    opj_image_t * image)
{
  gint x, y, y0, y1, w;
  guint8 *data_out, *tmp;
  const gint *data_in;
  gint dstride;
  gint off;

  w = GST_VIDEO_FRAME_WIDTH (frame);
  data_out = GST_VIDEO_FRAME_PLANE_DATA (frame, 0);
  dstride = GST_VIDEO_FRAME_PLANE_STRIDE (frame, 0);

  data_in = image->comps[0].data;
  off = 0x80 * image->comps[0].sgnd;

  y0 = image->y0;
  y1 = image->y1;
  data_out += y0 * dstride;
  for (y = y0; y < y1; y++) {
    tmp = data_out;
    for (x = 0; x < w; x++)
      *tmp++ = off + *data_in++;
    data_out += dstride;
  }
}

static void
fill_frame_planar16_1 (GstOpenJPEGDec * self, GstVideoFrame * frame,
    opj_image_t * image)
{
  gint x, y, y0, y1, w;
  guint16 *data_out, *tmp;
  const gint *data_in;
  gint dstride;
  gint shift, off;

  w = GST_VIDEO_FRAME_WIDTH (frame);
  data_out = GST_VIDEO_FRAME_PLANE_DATA (frame, 0);
  dstride = GST_VIDEO_FRAME_PLANE_STRIDE (frame, 0) / 2;

  data_in = image->comps[0].data;

  off = (1 << (image->comps[0].prec - 1)) * image->comps[0].sgnd;
  shift =
      MAX (MIN (GST_VIDEO_FRAME_COMP_DEPTH (frame, 0) - image->comps[0].prec,
          8), 0);

  y0 = image->y0;
  y1 = image->y1;
  data_out += y0 * dstride;
  for (y = y0; y < y1; y++) {
    tmp = data_out;
    for (x = 0; x < w; x++)
      *tmp++ = off + (*data_in++ << shift);
    data_out += dstride;
  }
}

static void
fill_frame_planar8_3 (GstOpenJPEGDec * self, GstVideoFrame * frame,
    opj_image_t * image)
{
  gint c, x, y, y0, y1, w;
  guint8 *data_out, *tmp;
  const gint *data_in;
  gint dstride, off;

  for (c = 0; c < 3; c++) {
    opj_image_comp_t *comp = image->comps + c;

    w = GST_VIDEO_FRAME_COMP_WIDTH (frame, c);
    dstride = GST_VIDEO_FRAME_COMP_STRIDE (frame, c);
    data_out = GST_VIDEO_FRAME_COMP_DATA (frame, c);
    data_in = comp->data;
    off = 0x80 * comp->sgnd;

    /* copy only the stripe content (image) to the full size frame */
    y0 = comp->y0;
    y1 = comp->y0 + comp->h;
    data_out += y0 * dstride;
    for (y = y0; y < y1; y++) {
      tmp = data_out;
      for (x = 0; x < w; x++)
        *tmp++ = off + *data_in++;
      data_out += dstride;
    }
  }
}

static void
fill_frame_planar16_3 (GstOpenJPEGDec * self, GstVideoFrame * frame,
    opj_image_t * image)
{
  gint c, x, y, y0, y1, w;
  guint16 *data_out, *tmp;
  const gint *data_in;
  gint dstride;
  gint shift, off;

  for (c = 0; c < 3; c++) {
    opj_image_comp_t *comp = image->comps + c;

    w = GST_VIDEO_FRAME_COMP_WIDTH (frame, c);
    dstride = GST_VIDEO_FRAME_COMP_STRIDE (frame, c) / 2;
    data_out = (guint16 *) GST_VIDEO_FRAME_COMP_DATA (frame, c);
    data_in = comp->data;
    off = (1 << (comp->prec - 1)) * comp->sgnd;
    shift =
        MAX (MIN (GST_VIDEO_FRAME_COMP_DEPTH (frame, c) - comp->prec, 8), 0);

    /* copy only the stripe content (image) to the full size frame */
    y0 = comp->y0;
    y1 = comp->y0 + comp->h;
    data_out += y0 * dstride;
    for (y = y0; y < y1; y++) {
      tmp = data_out;
      for (x = 0; x < w; x++)
        *tmp++ = off + (*data_in++ << shift);
      data_out += dstride;
    }
  }
}

static void
fill_frame_planar8_3_generic (GstOpenJPEGDec * self, GstVideoFrame * frame,
    opj_image_t * image)
{
  gint x, y, y0, y1, w, c;
  guint8 *data_out, *tmp;
  const gint *data_in[3];
  gint dstride;
  gint dx[3], dy[3], off[3];

  w = GST_VIDEO_FRAME_WIDTH (frame);
  data_out = GST_VIDEO_FRAME_PLANE_DATA (frame, 0);
  dstride = GST_VIDEO_FRAME_PLANE_STRIDE (frame, 0);

  for (c = 0; c < 3; c++) {
    data_in[c] = image->comps[c].data;
    dx[c] = image->comps[c].dx;
    dy[c] = image->comps[c].dy;
    off[c] = 0x80 * image->comps[c].sgnd;
  }

  y0 = image->y0;
  y1 = image->y1;
  data_out += y0 * dstride;
  for (y = y0; y < y1; y++) {
    tmp = data_out;
    for (x = 0; x < w; x++) {
      tmp[0] = 0xff;
      tmp[1] = off[0] + data_in[0][((y / dy[0]) * w + x) / dx[0]];
      tmp[2] = off[1] + data_in[1][((y / dy[1]) * w + x) / dx[1]];
      tmp[3] = off[2] + data_in[2][((y / dy[2]) * w + x) / dx[2]];
      tmp += 4;
    }
    data_out += dstride;
  }
}

static void
fill_frame_planar16_3_generic (GstOpenJPEGDec * self, GstVideoFrame * frame,
    opj_image_t * image)
{
  gint x, y, y0, y1, w, c;
  guint16 *data_out, *tmp;
  const gint *data_in[3];
  gint dstride;
  gint dx[3], dy[3], shift[3], off[3];

  w = GST_VIDEO_FRAME_WIDTH (frame);
  data_out = (guint16 *) GST_VIDEO_FRAME_PLANE_DATA (frame, 0);
  dstride = GST_VIDEO_FRAME_PLANE_STRIDE (frame, 0) / 2;

  for (c = 0; c < 3; c++) {
    dx[c] = image->comps[c].dx;
    dy[c] = image->comps[c].dy;
    data_in[c] = image->comps[c].data;
    off[c] = (1 << (image->comps[c].prec - 1)) * image->comps[c].sgnd;
    shift[c] =
        MAX (MIN (GST_VIDEO_FRAME_COMP_DEPTH (frame, c) - image->comps[c].prec,
            8), 0);
  }

  y0 = image->y0;
  y1 = image->y1;
  data_out += y0 * dstride;
  for (y = y0; y < y1; y++) {
    tmp = data_out;
    for (x = 0; x < w; x++) {
      tmp[0] = 0xff;
      tmp[1] = off[0] + (data_in[0][((y / dy[0]) * w + x) / dx[0]] << shift[0]);
      tmp[2] = off[1] + (data_in[1][((y / dy[1]) * w + x) / dx[1]] << shift[1]);
      tmp[3] = off[2] + (data_in[2][((y / dy[2]) * w + x) / dx[2]] << shift[2]);
      tmp += 4;
    }
    data_out += dstride;
  }
}

static gint
get_highest_prec (opj_image_t * image)
{
  gint i;
  gint ret = 0;

  for (i = 0; i < image->numcomps; i++)
    ret = MAX (image->comps[i].prec, ret);

  return ret;
}


static GstFlowReturn
gst_openjpeg_dec_negotiate (GstOpenJPEGDec * self, opj_image_t * image)
{
  GstVideoFormat format;

  if (image->color_space == OPJ_CLRSPC_UNKNOWN || image->color_space == 0)
    image->color_space = self->color_space;

  if (!self->input_state)
    return GST_FLOW_FLUSHING;

  switch (image->color_space) {
    case OPJ_CLRSPC_SRGB:
      if (image->numcomps == 4) {
        if (image->comps[0].dx != 1 || image->comps[0].dy != 1 ||
            image->comps[1].dx != 1 || image->comps[1].dy != 1 ||
            image->comps[2].dx != 1 || image->comps[2].dy != 1 ||
            image->comps[3].dx != 1 || image->comps[3].dy != 1) {
          GST_ERROR_OBJECT (self, "Sub-sampling for RGBA not supported");
          return GST_FLOW_NOT_NEGOTIATED;
        }

        if (get_highest_prec (image) == 8) {
          self->fill_frame = fill_frame_packed8_4;
          format =
              reverse_rgb_channels (self->sampling) ? GST_VIDEO_FORMAT_ABGR :
              GST_VIDEO_FORMAT_ARGB;

        } else if (get_highest_prec (image) <= 16) {
          self->fill_frame = fill_frame_packed16_4;
          format = GST_VIDEO_FORMAT_ARGB64;
        } else {
          GST_ERROR_OBJECT (self, "Unsupported depth %d", image->comps[3].prec);
          return GST_FLOW_NOT_NEGOTIATED;
        }
      } else if (image->numcomps == 3) {
        if (image->comps[0].dx != 1 || image->comps[0].dy != 1 ||
            image->comps[1].dx != 1 || image->comps[1].dy != 1 ||
            image->comps[2].dx != 1 || image->comps[2].dy != 1) {
          GST_ERROR_OBJECT (self, "Sub-sampling for RGB not supported");
          return GST_FLOW_NOT_NEGOTIATED;
        }

        if (get_highest_prec (image) == 8) {
          self->fill_frame = fill_frame_packed8_3;
          format =
              reverse_rgb_channels (self->sampling) ? GST_VIDEO_FORMAT_BGR :
              GST_VIDEO_FORMAT_RGB;
        } else if (get_highest_prec (image) <= 16) {
          self->fill_frame = fill_frame_packed16_3;
          format = GST_VIDEO_FORMAT_ARGB64;
        } else {
          GST_ERROR_OBJECT (self, "Unsupported depth %d",
              get_highest_prec (image));
          return GST_FLOW_NOT_NEGOTIATED;
        }
      } else {
        GST_ERROR_OBJECT (self, "Unsupported number of RGB components: %d",
            image->numcomps);
        return GST_FLOW_NOT_NEGOTIATED;
      }
      break;
    case OPJ_CLRSPC_GRAY:
      if (image->numcomps == 1) {
        if (image->comps[0].dx != 1 && image->comps[0].dy != 1) {
          GST_ERROR_OBJECT (self, "Sub-sampling for GRAY not supported");
          return GST_FLOW_NOT_NEGOTIATED;
        }

        if (get_highest_prec (image) == 8) {
          self->fill_frame = fill_frame_planar8_1;
          format = GST_VIDEO_FORMAT_GRAY8;
        } else if (get_highest_prec (image) <= 16) {
          self->fill_frame = fill_frame_planar16_1;
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
          format = GST_VIDEO_FORMAT_GRAY16_LE;
#else
          format = GST_VIDEO_FORMAT_GRAY16_BE;
#endif
        } else {
          GST_ERROR_OBJECT (self, "Unsupported depth %d",
              get_highest_prec (image));
          return GST_FLOW_NOT_NEGOTIATED;
        }
      } else if (image->numcomps == 2) {
        if ((image->comps[0].dx != 1 && image->comps[0].dy != 1) ||
            (image->comps[1].dx != 1 && image->comps[1].dy != 1)) {
          GST_ERROR_OBJECT (self, "Sub-sampling for GRAY not supported");
          return GST_FLOW_NOT_NEGOTIATED;
        }
        if (get_highest_prec (image) == 8) {
          self->fill_frame = fill_frame_packed8_2;
          format = GST_VIDEO_FORMAT_ARGB;
        } else if (get_highest_prec (image) <= 16) {
          self->fill_frame = fill_frame_packed16_2;
          format = GST_VIDEO_FORMAT_ARGB64;
        } else {
          GST_ERROR_OBJECT (self, "Unsupported depth %d",
              get_highest_prec (image));
          return GST_FLOW_NOT_NEGOTIATED;
        }
      } else {
        GST_ERROR_OBJECT (self, "Unsupported number of GRAY components: %d",
            image->numcomps);
        return GST_FLOW_NOT_NEGOTIATED;
      }
      break;
    case OPJ_CLRSPC_SYCC:
      if (image->numcomps != 3 && image->numcomps != 4) {
        GST_ERROR_OBJECT (self, "Unsupported number of YUV components: %d",
            image->numcomps);
        return GST_FLOW_NOT_NEGOTIATED;
      }

      if (image->comps[0].dx != 1 || image->comps[0].dy != 1) {
        GST_ERROR_OBJECT (self, "Sub-sampling of luma plane not supported");
        return GST_FLOW_NOT_NEGOTIATED;
      }

      if (image->comps[1].dx != image->comps[2].dx ||
          image->comps[1].dy != image->comps[2].dy) {
        GST_ERROR_OBJECT (self,
            "Different sub-sampling of chroma planes not supported");
        return GST_FLOW_ERROR;
      }

      if (image->numcomps == 4) {
        if (image->comps[3].dx != 1 || image->comps[3].dy != 1) {
          GST_ERROR_OBJECT (self, "Sub-sampling of alpha plane not supported");
          return GST_FLOW_NOT_NEGOTIATED;
        }

        if (get_highest_prec (image) == 8) {
          self->fill_frame = fill_frame_packed8_4;
          format = GST_VIDEO_FORMAT_AYUV;
        } else if (image->comps[3].prec <= 16) {
          self->fill_frame = fill_frame_packed16_4;
          format = GST_VIDEO_FORMAT_AYUV64;
        } else {
          GST_ERROR_OBJECT (self, "Unsupported depth %d", image->comps[0].prec);
          return GST_FLOW_NOT_NEGOTIATED;
        }
      } else if (image->numcomps == 3) {
        if (get_highest_prec (image) == 8) {
          if (image->comps[1].dx == 1 && image->comps[1].dy == 1) {
            self->fill_frame = fill_frame_planar8_3;
            format = GST_VIDEO_FORMAT_Y444;
          } else if (image->comps[1].dx == 2 && image->comps[1].dy == 1) {
            self->fill_frame = fill_frame_planar8_3;
            format = GST_VIDEO_FORMAT_Y42B;
          } else if (image->comps[1].dx == 2 && image->comps[1].dy == 2) {
            self->fill_frame = fill_frame_planar8_3;
            format = GST_VIDEO_FORMAT_I420;
          } else if (image->comps[1].dx == 4 && image->comps[1].dy == 1) {
            self->fill_frame = fill_frame_planar8_3;
            format = GST_VIDEO_FORMAT_Y41B;
          } else if (image->comps[1].dx == 4 && image->comps[1].dy == 4) {
            self->fill_frame = fill_frame_planar8_3;
            format = GST_VIDEO_FORMAT_YUV9;
          } else {
            self->fill_frame = fill_frame_planar8_3_generic;
            format = GST_VIDEO_FORMAT_AYUV;
          }
        } else if (get_highest_prec (image) <= 16) {
          if (image->comps[0].prec == 10 &&
              image->comps[1].prec == 10 && image->comps[2].prec == 10) {
            if (image->comps[1].dx == 1 && image->comps[1].dy == 1) {
              self->fill_frame = fill_frame_planar16_3;
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
              format = GST_VIDEO_FORMAT_Y444_10LE;
#else
              format = GST_VIDEO_FORMAT_Y444_10BE;
#endif
            } else if (image->comps[1].dx == 2 && image->comps[1].dy == 1) {
              self->fill_frame = fill_frame_planar16_3;
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
              format = GST_VIDEO_FORMAT_I422_10LE;
#else
              format = GST_VIDEO_FORMAT_I422_10BE;
#endif
            } else if (image->comps[1].dx == 2 && image->comps[1].dy == 2) {
              self->fill_frame = fill_frame_planar16_3;
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
              format = GST_VIDEO_FORMAT_I420_10LE;
#else
              format = GST_VIDEO_FORMAT_I420_10BE;
#endif
            } else {
              self->fill_frame = fill_frame_planar16_3_generic;
              format = GST_VIDEO_FORMAT_AYUV64;
            }
          } else {
            self->fill_frame = fill_frame_planar16_3_generic;
            format = GST_VIDEO_FORMAT_AYUV64;
          }
        } else {
          GST_ERROR_OBJECT (self, "Unsupported depth %d",
              get_highest_prec (image));
          return GST_FLOW_NOT_NEGOTIATED;
        }
      } else {
        GST_ERROR_OBJECT (self, "Unsupported number of YUV components: %d",
            image->numcomps);
        return GST_FLOW_NOT_NEGOTIATED;
      }
      break;
    default:
      GST_ERROR_OBJECT (self, "Unsupported colorspace %d", image->color_space);
      return GST_FLOW_NOT_NEGOTIATED;
  }

  if (!self->output_state ||
      self->output_state->info.finfo->format != format ||
      self->output_state->info.width != self->input_state->info.width ||
      self->output_state->info.height != self->input_state->info.height) {
    if (self->output_state)
      gst_video_codec_state_unref (self->output_state);
    self->output_state =
        gst_video_decoder_set_output_state (GST_VIDEO_DECODER (self), format,
        self->input_state->info.width, self->input_state->info.height,
        self->input_state);
    if (!gst_video_decoder_negotiate (GST_VIDEO_DECODER (self)))
      return GST_FLOW_NOT_NEGOTIATED;
  }

  return GST_FLOW_OK;
}

static void
gst_openjpeg_dec_opj_error (const char *msg, void *userdata)
{
  GstOpenJPEGDec *self = GST_OPENJPEG_DEC (userdata);
  gchar *trimmed = g_strchomp (g_strdup (msg));
  GST_TRACE_OBJECT (self, "openjpeg error: %s", trimmed);
  g_free (trimmed);
}

static void
gst_openjpeg_dec_opj_warning (const char *msg, void *userdata)
{
  GstOpenJPEGDec *self = GST_OPENJPEG_DEC (userdata);
  gchar *trimmed = g_strchomp (g_strdup (msg));
  GST_TRACE_OBJECT (self, "openjpeg warning: %s", trimmed);
  g_free (trimmed);
}

static void
gst_openjpeg_dec_opj_info (const char *msg, void *userdata)
{
  GstOpenJPEGDec *self = GST_OPENJPEG_DEC (userdata);
  gchar *trimmed = g_strchomp (g_strdup (msg));
  GST_TRACE_OBJECT (self, "openjpeg info: %s", trimmed);
  g_free (trimmed);
}

typedef struct
{
  guint8 *data;
  guint offset, size;
} MemStream;

static OPJ_SIZE_T
read_fn (void *p_buffer, OPJ_SIZE_T p_nb_bytes, void *p_user_data)
{
  MemStream *mstream = p_user_data;
  OPJ_SIZE_T read;

  if (mstream->offset == mstream->size)
    return -1;

  if (mstream->offset + p_nb_bytes > mstream->size)
    read = mstream->size - mstream->offset;
  else
    read = p_nb_bytes;

  memcpy (p_buffer, mstream->data + mstream->offset, read);
  mstream->offset += read;

  return read;
}

static OPJ_SIZE_T
write_fn (void *p_buffer, OPJ_SIZE_T p_nb_bytes, void *p_user_data)
{
  g_return_val_if_reached (-1);
}

static OPJ_OFF_T
skip_fn (OPJ_OFF_T p_nb_bytes, void *p_user_data)
{
  MemStream *mstream = p_user_data;
  OPJ_OFF_T skip;

  if (mstream->offset + p_nb_bytes > mstream->size)
    skip = mstream->size - mstream->offset;
  else
    skip = p_nb_bytes;

  mstream->offset += skip;

  return skip;
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
gst_openjpeg_dec_is_last_input_subframe (GstVideoDecoder * dec,
    GstOpenJPEGCodecMessage * message)
{
  GstOpenJPEGDec *self = GST_OPENJPEG_DEC (dec);

  return (message->last_subframe || message->stripe == self->num_stripes);
}

static gboolean
gst_openjpeg_dec_is_last_output_subframe (GstVideoDecoder * dec,
    GstOpenJPEGCodecMessage * message)
{
  GstOpenJPEGDec *self = GST_OPENJPEG_DEC (dec);

  return (gst_video_decoder_get_processed_subframe_index (dec,
          message->frame) == (self->num_stripes - 1));
}


static gboolean
gst_openjpeg_dec_has_pending_job_to_finish (GstOpenJPEGDec * self)
{
  gboolean res = FALSE;
  if (self->downstream_flow_ret != GST_FLOW_OK)
    return res;
  g_mutex_lock (&self->messages_lock);
  res = (!g_queue_is_empty (&self->messages)
      || (self->available_threads < self->max_slice_threads));
  g_mutex_unlock (&self->messages_lock);
  return res;
}

static GstOpenJPEGCodecMessage *
gst_openjpeg_decode_message_new (GstOpenJPEGDec * self,
    GstVideoCodecFrame * frame, int num_stripe)
{
  GstOpenJPEGCodecMessage *message = g_new0 (GstOpenJPEGCodecMessage, 1);
  GST_DEBUG_OBJECT (self, "message: %p", message);
  message->frame = gst_video_codec_frame_ref (frame);
  message->stripe = num_stripe;
  message->last_error = OPENJPEG_ERROR_NONE;
  message->input_buffer = gst_buffer_ref (frame->input_buffer);
  message->last_subframe = GST_BUFFER_FLAG_IS_SET (frame->input_buffer,
      GST_BUFFER_FLAG_MARKER);
  return message;
}

static GstOpenJPEGCodecMessage *
gst_openjpeg_decode_message_free (GstOpenJPEGDec * self,
    GstOpenJPEGCodecMessage * message)
{
  if (!message)
    return message;
  gst_buffer_unref (message->input_buffer);
  gst_video_codec_frame_unref (message->frame);
  GST_DEBUG_OBJECT (self, "message: %p", message);
  g_free (message);
  return NULL;
}

static GstOpenJPEGCodecMessage *
gst_openjpeg_dec_wait_for_new_message (GstOpenJPEGDec * self, gboolean dry_run)
{
  GstOpenJPEGCodecMessage *message = NULL;
  g_mutex_lock (&self->messages_lock);
  if (dry_run && self->available_threads == self->max_slice_threads)
    goto done;
  if (!g_queue_is_empty (&self->messages) && !dry_run) {
    message = g_queue_pop_head (&self->messages);
  } else {
    g_cond_wait (&self->messages_cond, &self->messages_lock);
  }

done:
  g_mutex_unlock (&self->messages_lock);
  return message;
}

static void
gst_openjpeg_dec_pause_loop (GstOpenJPEGDec * self, GstFlowReturn flow_ret)
{
  g_mutex_lock (&self->drain_lock);
  GST_DEBUG_OBJECT (self, "Pause the loop draining %d flow_ret %s",
      self->draining, gst_flow_get_name (flow_ret));
  if (self->draining) {
    self->draining = FALSE;
    g_cond_broadcast (&self->drain_cond);
  }
  gst_pad_pause_task (GST_VIDEO_DECODER_SRC_PAD (self));
  self->downstream_flow_ret = flow_ret;
  self->started = FALSE;
  g_mutex_unlock (&self->drain_lock);
}

static void
gst_openjpeg_dec_loop (GstOpenJPEGDec * self)
{
  GstOpenJPEGCodecMessage *message;
  GstVideoDecoder *decoder = GST_VIDEO_DECODER (self);
  GstFlowReturn flow_ret = GST_FLOW_OK;

  message = gst_openjpeg_dec_wait_for_new_message (self, FALSE);
  if (message) {
    GST_DEBUG_OBJECT (self,
        "received message for frame %p stripe %d last_error %d threads %d",
        message->frame, message->stripe, message->last_error,
        self->available_threads);

    if (self->flushing)
      goto flushing;

    if (message->last_error != OPENJPEG_ERROR_NONE)
      goto decode_error;

    g_mutex_lock (&self->decoding_lock);

    if (gst_openjpeg_dec_is_last_output_subframe (decoder, message))
      flow_ret = gst_video_decoder_finish_frame (decoder, message->frame);
    else
      gst_video_decoder_finish_subframe (decoder, message->frame);
    g_mutex_unlock (&self->decoding_lock);
    message = gst_openjpeg_decode_message_free (self, message);
    g_cond_broadcast (&self->messages_cond);
  }

  if (flow_ret != GST_FLOW_OK)
    goto flow_error;

  if (self->draining && !gst_openjpeg_dec_has_pending_job_to_finish (self))
    gst_openjpeg_dec_pause_loop (self, GST_FLOW_OK);

  if (self->flushing)
    goto flushing;

  return;

decode_error:
  {
    GST_ELEMENT_ERROR (self, LIBRARY, FAILED, (NULL),
        ("OPEN JPEG decode fail %d", message->last_error));
    gst_video_codec_frame_unref (message->frame);
    gst_pad_push_event (GST_VIDEO_DECODER_SRC_PAD (self), gst_event_new_eos ());
    gst_openjpeg_dec_pause_loop (self, GST_FLOW_ERROR);
    gst_openjpeg_decode_message_free (self, message);
    return;
  }

flushing:
  {
    GST_DEBUG_OBJECT (self, "Flushing -- stopping task");
    if (message) {
      gst_video_codec_frame_unref (message->frame);
      gst_openjpeg_decode_message_free (self, message);
    }
    gst_openjpeg_dec_pause_loop (self, GST_FLOW_FLUSHING);
    return;
  }

flow_error:
  {
    if (flow_ret == GST_FLOW_EOS) {
      GST_DEBUG_OBJECT (self, "EOS");

      gst_pad_push_event (GST_VIDEO_DECODER_SRC_PAD (self),
          gst_event_new_eos ());
    } else if (flow_ret < GST_FLOW_EOS) {
      GST_ELEMENT_ERROR (self, STREAM, FAILED,
          ("Internal data stream error."), ("stream stopped, reason %s",
              gst_flow_get_name (flow_ret)));

      gst_pad_push_event (GST_VIDEO_DECODER_SRC_PAD (self),
          gst_event_new_eos ());
    } else if (flow_ret == GST_FLOW_FLUSHING) {
      GST_DEBUG_OBJECT (self, "Flushing -- stopping task");
    }
    gst_openjpeg_dec_pause_loop (self, flow_ret);

    return;
  }

}

#define DECODE_ERROR(self, message, err_code, mutex_unlock) { \
      GST_WARNING_OBJECT(self, "An error occurred err_code=%d", err_code);\
      message->last_error = err_code; \
      if (mutex_unlock) \
        g_mutex_unlock (&self->decoding_lock);\
      goto done; \
}

static void
gst_openjpeg_dec_decode_stripe (GstElement * element, gpointer user_data)
{
  GstOpenJPEGDec *self = GST_OPENJPEG_DEC (element);
  GstVideoDecoder *decoder = GST_VIDEO_DECODER (element);
  GstOpenJPEGCodecMessage *message = (GstOpenJPEGCodecMessage *) user_data;
  GstMapInfo map;
  GstVideoFrame vframe;
  opj_codec_t *dec = NULL;
  opj_stream_t *stream = NULL;
  MemStream mstream;
  opj_image_t *image = NULL;
  opj_dparameters_t params;
  gint max_threads;

  GstFlowReturn ret;
  gint i;

  GST_DEBUG_OBJECT (self, "Start to decode stripe %p %d", message->frame,
      message->stripe);

  dec = opj_create_decompress (self->codec_format);
  if (!dec)
    DECODE_ERROR (self, message, OPENJPEG_ERROR_INIT, FALSE);

  if (G_UNLIKELY (gst_debug_category_get_threshold (GST_CAT_DEFAULT) >=
          GST_LEVEL_TRACE)) {
    opj_set_info_handler (dec, gst_openjpeg_dec_opj_info, self);
    opj_set_warning_handler (dec, gst_openjpeg_dec_opj_warning, self);
    opj_set_error_handler (dec, gst_openjpeg_dec_opj_error, self);
  } else {
    opj_set_info_handler (dec, NULL, NULL);
    opj_set_warning_handler (dec, NULL, NULL);
    opj_set_error_handler (dec, NULL, NULL);
  }

  params = self->params;
  if (self->ncomps)
    params.jpwl_exp_comps = self->ncomps;
  if (!opj_setup_decoder (dec, &params))
    DECODE_ERROR (self, message, OPENJPEG_ERROR_OPEN, FALSE);

  max_threads = g_atomic_int_get (&self->max_threads);
  if (max_threads > self->num_procs)
    max_threads = self->num_procs;
  if (!opj_codec_set_threads (dec, max_threads))
    GST_WARNING_OBJECT (self, "Failed to set %d number of threads",
        max_threads);

  if (!gst_buffer_map (message->input_buffer, &map, GST_MAP_READ))
    DECODE_ERROR (self, message, OPENJPEG_ERROR_MAP_READ, FALSE);


  if (self->is_jp2c && map.size < 8)
    DECODE_ERROR (self, message, OPENJPEG_ERROR_MAP_READ, FALSE);

  stream = opj_stream_create (4096, OPJ_TRUE);
  if (!stream)
    DECODE_ERROR (self, message, OPENJPEG_ERROR_OPEN, FALSE);

  mstream.data = map.data + (self->is_jp2c ? 8 : 0);
  mstream.offset = 0;
  mstream.size = map.size - (self->is_jp2c ? 8 : 0);

  opj_stream_set_read_function (stream, read_fn);
  opj_stream_set_write_function (stream, write_fn);
  opj_stream_set_skip_function (stream, skip_fn);
  opj_stream_set_seek_function (stream, seek_fn);
  opj_stream_set_user_data (stream, &mstream, NULL);
  opj_stream_set_user_data_length (stream, mstream.size);

  image = NULL;
  if (!opj_read_header (stream, dec, &image))
    DECODE_ERROR (self, message, OPENJPEG_ERROR_DECODE, FALSE);

  if (!opj_decode (dec, stream, image))
    DECODE_ERROR (self, message, OPENJPEG_ERROR_DECODE, FALSE);

  for (i = 0; i < image->numcomps; i++) {
    if (image->comps[i].data == NULL)
      DECODE_ERROR (self, message, OPENJPEG_ERROR_DECODE, FALSE);
  }

  gst_buffer_unmap (message->input_buffer, &map);

  g_mutex_lock (&self->decoding_lock);

  ret = gst_openjpeg_dec_negotiate (self, image);
  if (ret != GST_FLOW_OK)
    DECODE_ERROR (self, message, OPENJPEG_ERROR_NEGOCIATE, TRUE);

  if (message->frame->output_buffer == NULL) {
    ret = gst_video_decoder_allocate_output_frame (decoder, message->frame);
    if (ret != GST_FLOW_OK)
      DECODE_ERROR (self, message, OPENJPEG_ERROR_ALLOCATE, TRUE);
  }

  if (!gst_video_frame_map (&vframe, &self->output_state->info,
          message->frame->output_buffer, GST_MAP_WRITE))
    DECODE_ERROR (self, message, OPENJPEG_ERROR_MAP_WRITE, TRUE);

  if (message->stripe)
    self->fill_frame (self, &vframe, image);
  else {
    GST_ERROR_OBJECT (decoder, " current_stripe should be greater than 0");
    DECODE_ERROR (self, message, OPENJPEG_ERROR_MAP_WRITE, TRUE);
  }
  gst_video_frame_unmap (&vframe);
  g_mutex_unlock (&self->decoding_lock);
  message->last_error = OPENJPEG_ERROR_NONE;
  GST_DEBUG_OBJECT (self, "Finished to decode stripe message=%p stripe=%d",
      message->frame, message->stripe);
done:
  if (!message->direct) {
    g_mutex_lock (&self->messages_lock);
    self->available_threads++;
    g_queue_push_tail (&self->messages, message);
    g_mutex_unlock (&self->messages_lock);
    g_cond_broadcast (&self->messages_cond);
  }

  if (stream) {
    opj_end_decompress (dec, stream);
    opj_stream_destroy (stream);
  }
  if (image)
    opj_image_destroy (image);
  if (dec)
    opj_destroy_codec (dec);
}

static GstFlowReturn
gst_openjpeg_dec_decode_frame_multiple (GstVideoDecoder * decoder,
    GstVideoCodecFrame * frame)
{
  GstOpenJPEGDec *self = GST_OPENJPEG_DEC (decoder);
  GstOpenJPEGCodecMessage *message = NULL;
  guint current_stripe =
      gst_video_decoder_get_input_subframe_index (decoder, frame);

  if (!self->started) {
    GST_DEBUG_OBJECT (self, "Starting task");
    gst_pad_start_task (GST_VIDEO_DECODER_SRC_PAD (self),
        (GstTaskFunction) gst_openjpeg_dec_loop, decoder, NULL);
    self->started = TRUE;
  }
  /* Make sure to release the base class stream lock, otherwise
   * _loop() can't call _finish_frame() and we might block forever
   * because no input buffers are released */
  GST_VIDEO_DECODER_STREAM_UNLOCK (self);

  while (!self->available_threads)
    gst_openjpeg_dec_wait_for_new_message (self, TRUE);

  GST_VIDEO_DECODER_STREAM_LOCK (self);

  if (self->downstream_flow_ret != GST_FLOW_OK)
    return self->downstream_flow_ret;

  g_mutex_lock (&self->messages_lock);
  message = gst_openjpeg_decode_message_new (self, frame, current_stripe);
  GST_LOG_OBJECT (self,
      "About to enqueue a decoding message from frame %p stripe %d", frame,
      message->stripe);

  if (self->available_threads)
    self->available_threads--;
  g_mutex_unlock (&self->messages_lock);

  gst_element_call_async (GST_ELEMENT (self),
      (GstElementCallAsyncFunc) gst_openjpeg_dec_decode_stripe, message, NULL);
  if (gst_video_decoder_get_subframe_mode (decoder)
      && gst_openjpeg_dec_is_last_input_subframe (decoder, message))
    gst_video_decoder_have_last_subframe (decoder, frame);
  return GST_FLOW_OK;
}

static GstFlowReturn
gst_openjpeg_dec_decode_frame_single (GstVideoDecoder * decoder,
    GstVideoCodecFrame * frame)
{
  GstOpenJPEGDec *self = GST_OPENJPEG_DEC (decoder);
  GstOpenJPEGCodecMessage *message = NULL;
  guint current_stripe =
      gst_video_decoder_get_input_subframe_index (decoder, frame);
  GstFlowReturn ret = GST_FLOW_OK;

  message = gst_openjpeg_decode_message_new (self, frame, current_stripe);
  message->direct = TRUE;
  gst_openjpeg_dec_decode_stripe (GST_ELEMENT (decoder), message);
  if (message->last_error != OPENJPEG_ERROR_NONE) {
    GST_WARNING_OBJECT
        (self, "An error occured %d during the JPEG decoding",
        message->last_error);
    self->last_error = message->last_error;
    ret = GST_FLOW_ERROR;
    goto done;
  }
  if (gst_openjpeg_dec_is_last_output_subframe (decoder, message))
    ret = gst_video_decoder_finish_frame (decoder, message->frame);
  else
    gst_video_decoder_finish_subframe (decoder, message->frame);

done:
  gst_openjpeg_decode_message_free (self, message);
  return ret;
}

static gboolean
gst_openjpeg_dec_flush (GstVideoDecoder * decoder)
{
  GstOpenJPEGDec *self = GST_OPENJPEG_DEC (decoder);

  GST_DEBUG_OBJECT (self, "Flushing decoder");

  /* 2) Wait until the srcpad loop is stopped,
   * unlock GST_VIDEO_DECODER_STREAM_LOCK to prevent deadlocks
   * caused by using this lock from inside the loop function */
  GST_VIDEO_DECODER_STREAM_UNLOCK (self);
  gst_pad_stop_task (GST_VIDEO_DECODER_SRC_PAD (decoder));
  GST_DEBUG_OBJECT (self, "Flushing -- task stopped");
  GST_VIDEO_DECODER_STREAM_LOCK (self);

  /* Reset our state */
  self->started = FALSE;
  GST_DEBUG_OBJECT (self, "Flush finished");

  return TRUE;
}

static GstFlowReturn
gst_openjpeg_dec_handle_frame (GstVideoDecoder * decoder,
    GstVideoCodecFrame * frame)
{
  GstOpenJPEGDec *self = GST_OPENJPEG_DEC (decoder);
  GstFlowReturn ret = GST_FLOW_OK;
  gint64 deadline;
  guint current_stripe =
      gst_video_decoder_get_input_subframe_index (decoder, frame);

  if (self->downstream_flow_ret != GST_FLOW_OK) {
    gst_video_codec_frame_unref (frame);
    return self->downstream_flow_ret;
  }

  GST_DEBUG_OBJECT (self, "Handling frame with current stripe %d",
      current_stripe);

  deadline = gst_video_decoder_get_max_decode_time (decoder, frame);
  if (self->drop_subframes || deadline < 0) {
    GST_INFO_OBJECT (self,
        "Dropping too late frame: deadline %" G_GINT64_FORMAT, deadline);
    self->drop_subframes = TRUE;
    if (current_stripe == self->num_stripes ||
        GST_BUFFER_FLAG_IS_SET (frame->input_buffer, GST_BUFFER_FLAG_MARKER)) {
      ret = gst_video_decoder_drop_frame (decoder, frame);
      self->drop_subframes = FALSE;
    } else {
      gst_video_decoder_drop_subframe (decoder, frame);
    }

    goto done;
  }

  ret = self->decode_frame (decoder, frame);
  if (ret != GST_FLOW_OK) {
    GST_WARNING_OBJECT (self, "Unable to decode the frame with flow error: %s",
        gst_flow_get_name (ret));
    goto error;
  }

done:
  return ret;

error:
  switch (self->last_error) {
    case OPENJPEG_ERROR_INIT:
      GST_ELEMENT_ERROR (self, LIBRARY, INIT,
          ("Failed to initialize OpenJPEG decoder"), (NULL));
      break;
    case OPENJPEG_ERROR_MAP_READ:
      GST_ELEMENT_ERROR (self, CORE, FAILED,
          ("Failed to map input buffer"), (NULL));
      break;
    case OPENJPEG_ERROR_MAP_WRITE:
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
    case OPENJPEG_ERROR_DECODE:
      GST_ELEMENT_ERROR (self, LIBRARY, INIT,
          ("Failed to decode OpenJPEG data"), (NULL));
      break;
    case OPENJPEG_ERROR_NEGOCIATE:
      GST_ELEMENT_ERROR (self, LIBRARY, INIT,
          ("Failed to negociate OpenJPEG data"), (NULL));
      break;
    case OPENJPEG_ERROR_ALLOCATE:
      GST_ELEMENT_ERROR (self, LIBRARY, INIT,
          ("Failed to allocate OpenJPEG data"), (NULL));
      break;
    default:
      GST_ELEMENT_ERROR (self, LIBRARY, INIT,
          ("Failed to encode OpenJPEG data"), (NULL));
      break;
  }

  return GST_FLOW_ERROR;
}

static GstFlowReturn
gst_openjpeg_dec_finish (GstVideoDecoder * decoder)
{
  GstOpenJPEGDec *self = GST_OPENJPEG_DEC (decoder);

  GST_DEBUG_OBJECT (self, "Draining component");

  if (!self->started) {
    GST_DEBUG_OBJECT (self, "Component not started yet");
    return GST_FLOW_OK;
  }

  self->draining = TRUE;
  if (!gst_openjpeg_dec_has_pending_job_to_finish (self)) {
    GST_DEBUG_OBJECT (self, "Component ready");
    g_cond_broadcast (&self->messages_cond);
    return GST_FLOW_OK;
  }

  /* Make sure to release the base class stream lock, otherwise
   * _loop() can't call _finish_frame() and we might block forever
   * because no input buffers are released */
  GST_VIDEO_DECODER_STREAM_UNLOCK (self);

  g_mutex_lock (&self->drain_lock);
  GST_DEBUG_OBJECT (self, "Waiting until component is drained");

  while (self->draining)
    g_cond_wait (&self->drain_cond, &self->drain_lock);

  GST_DEBUG_OBJECT (self, "Drained component");

  g_mutex_unlock (&self->drain_lock);
  GST_VIDEO_DECODER_STREAM_LOCK (self);
  self->started = FALSE;
  return GST_FLOW_OK;
}

static gboolean
gst_openjpeg_dec_decide_allocation (GstVideoDecoder * decoder, GstQuery * query)
{
  GstBufferPool *pool;
  GstStructure *config;

  if (!GST_VIDEO_DECODER_CLASS (parent_class)->decide_allocation (decoder,
          query))
    return FALSE;

  g_assert (gst_query_get_n_allocation_pools (query) > 0);
  gst_query_parse_nth_allocation_pool (query, 0, &pool, NULL, NULL, NULL);
  g_assert (pool != NULL);

  config = gst_buffer_pool_get_config (pool);
  if (gst_query_find_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL)) {
    gst_buffer_pool_config_add_option (config,
        GST_BUFFER_POOL_OPTION_VIDEO_META);
  }
  gst_buffer_pool_set_config (pool, config);
  gst_object_unref (pool);

  return TRUE;
}
