/* GStreamer
 * Copyright (C) 2004 Wim Taymans <wim@fluendo.com>
 * Copyright (c) 2012 Collabora Ltd.
 *	Author : Edward Hervey <edward@collabora.com>
 *      Author : Mark Nauwelaerts <mark.nauwelaerts@collabora.co.uk>
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
 * SECTION:element-theoraenc
 * @title: theoraenc
 * @see_also: theoradec, oggmux
 *
 * This element encodes raw video into a Theora stream.
 * [Theora](http://www.theora.org/) is a royalty-free
 * video codec maintained by the [Xiph.org Foundation](http://www.xiph.org/),
 * based on the VP3 codec.
 *
 * The theora codec internally only supports encoding of images that are a
 * multiple of 16 pixels in both X and Y direction. It is however perfectly
 * possible to encode images with other dimensions because an arbitrary
 * rectangular cropping region can be set up. This element will automatically
 * set up a correct cropping region if the dimensions are not multiples of 16
 * pixels.
 *
 * To control the quality of the encoding, the #GstTheoraEnc:bitrate and
 * #GstTheoraEnc:quality properties can be used. These two properties are
 * mutualy exclusive. Setting the bitrate property will produce a constant
 * bitrate (CBR) stream while setting the quality property will produce a
 * variable bitrate (VBR) stream.
 *
 * A videorate element is often required in front of theoraenc, especially
 * when transcoding and when putting Theora into the Ogg container.
 *
 * ## Example pipeline
 * |[
 * gst-launch-1.0 -v videotestsrc num-buffers=500 ! video/x-raw,width=1280,height=720 ! queue ! progressreport ! theoraenc ! oggmux ! filesink location=videotestsrc.ogg
 * ]|
 *  This example pipeline will encode a test video source to theora muxed in an
 * ogg container. Refer to the theoradec documentation to decode the create
 * stream.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <stdlib.h>             /* free */

#include <gst/tag/tag.h>
#include <gst/video/video.h>
#include <gst/video/gstvideometa.h>

#include "gsttheoraenc.h"

#define GST_CAT_DEFAULT theoraenc_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

#define GST_TYPE_MULTIPASS_MODE (gst_multipass_mode_get_type())
static GType
gst_multipass_mode_get_type (void)
{
  static GType multipass_mode_type = 0;
  static const GEnumValue multipass_mode[] = {
    {MULTIPASS_MODE_SINGLE_PASS, "Single pass", "single-pass"},
    {MULTIPASS_MODE_FIRST_PASS, "First pass", "first-pass"},
    {MULTIPASS_MODE_SECOND_PASS, "Second pass", "second-pass"},
    {0, NULL, NULL},
  };

  if (!multipass_mode_type) {
    multipass_mode_type =
        g_enum_register_static ("GstTheoraEncMultipassMode", multipass_mode);
  }
  return multipass_mode_type;
}

/* taken from theora/lib/toplevel.c */
static int
_ilog (unsigned int v)
{
  int ret = 0;

  while (v) {
    ret++;
    v >>= 1;
  }
  return (ret);
}

#define THEORA_DEF_BITRATE              0
#define THEORA_DEF_QUALITY              48
#define THEORA_DEF_KEYFRAME_AUTO        TRUE
#define THEORA_DEF_KEYFRAME_FREQ        64
#define THEORA_DEF_KEYFRAME_FREQ_FORCE  64
#define THEORA_DEF_SPEEDLEVEL           1
#define THEORA_DEF_VP3_COMPATIBLE       FALSE
#define THEORA_DEF_DROP_FRAMES          TRUE
#define THEORA_DEF_CAP_OVERFLOW         TRUE
#define THEORA_DEF_CAP_UNDERFLOW        FALSE
#define THEORA_DEF_RATE_BUFFER          0
#define THEORA_DEF_MULTIPASS_CACHE_FILE NULL
#define THEORA_DEF_MULTIPASS_MODE       MULTIPASS_MODE_SINGLE_PASS
enum
{
  PROP_0,
  PROP_BITRATE,
  PROP_QUALITY,
  PROP_KEYFRAME_AUTO,
  PROP_KEYFRAME_FREQ,
  PROP_KEYFRAME_FREQ_FORCE,
  PROP_SPEEDLEVEL,
  PROP_VP3_COMPATIBLE,
  PROP_DROP_FRAMES,
  PROP_CAP_OVERFLOW,
  PROP_CAP_UNDERFLOW,
  PROP_RATE_BUFFER,
  PROP_MULTIPASS_CACHE_FILE,
  PROP_MULTIPASS_MODE
      /* FILL ME */
};

/* this function does a straight granulepos -> timestamp conversion */
static GstClockTime
granulepos_to_timestamp (GstTheoraEnc * theoraenc, ogg_int64_t granulepos)
{
  guint64 iframe, pframe;
  int shift = theoraenc->info.keyframe_granule_shift;

  if (granulepos < 0)
    return GST_CLOCK_TIME_NONE;

  iframe = granulepos >> shift;
  pframe = granulepos - (iframe << shift);

  /* num and den are 32 bit, so we can safely multiply with GST_SECOND */
  return gst_util_uint64_scale ((guint64) (iframe + pframe),
      GST_SECOND * theoraenc->info.fps_denominator,
      theoraenc->info.fps_numerator);
}

static GstStaticPadTemplate theora_enc_sink_factory =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw, "
        "format = (string) { I420, Y42B, Y444 }, "
        "framerate = (fraction) [1/MAX, MAX], "
        "width = (int) [ 1, MAX ], " "height = (int) [ 1, MAX ]")
    );

static GstStaticPadTemplate theora_enc_src_factory =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-theora, "
        "framerate = (fraction) [1/MAX, MAX], "
        "width = (int) [ 1, MAX ], " "height = (int) [ 1, MAX ]")
    );

#define gst_theora_enc_parent_class parent_class
G_DEFINE_TYPE (GstTheoraEnc, gst_theora_enc, GST_TYPE_VIDEO_ENCODER);
GST_ELEMENT_REGISTER_DEFINE (theoraenc, "theoraenc",
    GST_RANK_PRIMARY, GST_TYPE_THEORA_ENC);

static gboolean theora_enc_start (GstVideoEncoder * enc);
static gboolean theora_enc_stop (GstVideoEncoder * enc);
static gboolean theora_enc_flush (GstVideoEncoder * enc);
static gboolean theora_enc_set_format (GstVideoEncoder * enc,
    GstVideoCodecState * state);
static GstFlowReturn theora_enc_handle_frame (GstVideoEncoder * enc,
    GstVideoCodecFrame * frame);
static GstFlowReturn theora_enc_pre_push (GstVideoEncoder * benc,
    GstVideoCodecFrame * frame);
static GstFlowReturn theora_enc_finish (GstVideoEncoder * enc);
static gboolean theora_enc_propose_allocation (GstVideoEncoder * encoder,
    GstQuery * query);

static GstCaps *theora_enc_getcaps (GstVideoEncoder * encoder,
    GstCaps * filter);
static void theora_enc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void theora_enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void theora_enc_finalize (GObject * object);

static gboolean theora_enc_write_multipass_cache (GstTheoraEnc * enc,
    gboolean begin, gboolean eos);

static void
gst_theora_enc_class_init (GstTheoraEncClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *element_class = (GstElementClass *) klass;
  GstVideoEncoderClass *gstvideo_encoder_class =
      GST_VIDEO_ENCODER_CLASS (klass);

  gobject_class->set_property = theora_enc_set_property;
  gobject_class->get_property = theora_enc_get_property;
  gobject_class->finalize = theora_enc_finalize;

  gst_element_class_add_static_pad_template (element_class,
      &theora_enc_src_factory);
  gst_element_class_add_static_pad_template (element_class,
      &theora_enc_sink_factory);
  gst_element_class_set_static_metadata (element_class, "Theora video encoder",
      "Codec/Encoder/Video", "encode raw YUV video to a theora stream",
      "Wim Taymans <wim@fluendo.com>");

  gstvideo_encoder_class->start = GST_DEBUG_FUNCPTR (theora_enc_start);
  gstvideo_encoder_class->stop = GST_DEBUG_FUNCPTR (theora_enc_stop);
  gstvideo_encoder_class->flush = GST_DEBUG_FUNCPTR (theora_enc_flush);
  gstvideo_encoder_class->set_format =
      GST_DEBUG_FUNCPTR (theora_enc_set_format);
  gstvideo_encoder_class->handle_frame =
      GST_DEBUG_FUNCPTR (theora_enc_handle_frame);
  gstvideo_encoder_class->pre_push = GST_DEBUG_FUNCPTR (theora_enc_pre_push);
  gstvideo_encoder_class->finish = GST_DEBUG_FUNCPTR (theora_enc_finish);
  gstvideo_encoder_class->getcaps = GST_DEBUG_FUNCPTR (theora_enc_getcaps);
  gstvideo_encoder_class->propose_allocation =
      GST_DEBUG_FUNCPTR (theora_enc_propose_allocation);

  /* general encoding stream options */
  g_object_class_install_property (gobject_class, PROP_BITRATE,
      g_param_spec_int ("bitrate", "Bitrate", "Compressed video bitrate (kbps)",
          0, (1 << 24) - 1, THEORA_DEF_BITRATE,
          (GParamFlags) G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_PLAYING));
  g_object_class_install_property (gobject_class, PROP_QUALITY,
      g_param_spec_int ("quality", "Quality", "Video quality", 0, 63,
          THEORA_DEF_QUALITY,
          (GParamFlags) G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_PLAYING));
  g_object_class_install_property (gobject_class, PROP_KEYFRAME_AUTO,
      g_param_spec_boolean ("keyframe-auto", "Keyframe Auto",
          "Automatic keyframe detection", THEORA_DEF_KEYFRAME_AUTO,
          (GParamFlags) G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_KEYFRAME_FREQ,
      g_param_spec_int ("keyframe-freq", "Keyframe frequency",
          "Keyframe frequency", 1, 32768, THEORA_DEF_KEYFRAME_FREQ,
          (GParamFlags) G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_KEYFRAME_FREQ_FORCE,
      g_param_spec_int ("keyframe-force", "Keyframe force",
          "Force keyframe every N frames", 1, 32768,
          THEORA_DEF_KEYFRAME_FREQ_FORCE,
          (GParamFlags) G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_SPEEDLEVEL,
      g_param_spec_int ("speed-level", "Speed level",
          "Controls the amount of motion vector searching done while encoding",
          0, 3, THEORA_DEF_SPEEDLEVEL,
          (GParamFlags) G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_VP3_COMPATIBLE,
      g_param_spec_boolean ("vp3-compatible", "VP3 compatible",
          "Disables non-VP3 compatible features",
          THEORA_DEF_VP3_COMPATIBLE,
          (GParamFlags) G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_DROP_FRAMES,
      g_param_spec_boolean ("drop-frames", "Drop frames",
          "Allow or disallow frame dropping",
          THEORA_DEF_DROP_FRAMES,
          (GParamFlags) G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_CAP_OVERFLOW,
      g_param_spec_boolean ("cap-overflow", "Cap overflow",
          "Enable capping of bit reservoir overflows",
          THEORA_DEF_CAP_OVERFLOW,
          (GParamFlags) G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_CAP_UNDERFLOW,
      g_param_spec_boolean ("cap-underflow", "Cap underflow",
          "Enable capping of bit reservoir underflows",
          THEORA_DEF_CAP_UNDERFLOW,
          (GParamFlags) G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_RATE_BUFFER,
      g_param_spec_int ("rate-buffer", "Rate Control Buffer",
          "Sets the size of the rate control buffer, in units of frames.  "
          "The default value of 0 instructs the encoder to automatically "
          "select an appropriate value",
          0, 1000, THEORA_DEF_RATE_BUFFER,
          (GParamFlags) G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_MULTIPASS_CACHE_FILE,
      g_param_spec_string ("multipass-cache-file", "Multipass Cache File",
          "Multipass cache file", THEORA_DEF_MULTIPASS_CACHE_FILE,
          (GParamFlags) G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_MULTIPASS_MODE,
      g_param_spec_enum ("multipass-mode", "Multipass mode",
          "Single pass or first/second pass", GST_TYPE_MULTIPASS_MODE,
          THEORA_DEF_MULTIPASS_MODE,
          (GParamFlags) G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  GST_DEBUG_CATEGORY_INIT (theoraenc_debug, "theoraenc", 0, "Theora encoder");

  gst_type_mark_as_plugin_api (GST_TYPE_MULTIPASS_MODE, 0);
}

static void
gst_theora_enc_init (GstTheoraEnc * enc)
{
  GST_PAD_SET_ACCEPT_TEMPLATE (GST_VIDEO_ENCODER_SINK_PAD (enc));

  enc->video_bitrate = THEORA_DEF_BITRATE;
  enc->video_quality = THEORA_DEF_QUALITY;
  enc->keyframe_auto = THEORA_DEF_KEYFRAME_AUTO;
  enc->keyframe_freq = THEORA_DEF_KEYFRAME_FREQ;
  enc->keyframe_force = THEORA_DEF_KEYFRAME_FREQ_FORCE;

  enc->speed_level = THEORA_DEF_SPEEDLEVEL;
  enc->vp3_compatible = THEORA_DEF_VP3_COMPATIBLE;
  enc->drop_frames = THEORA_DEF_DROP_FRAMES;
  enc->cap_overflow = THEORA_DEF_CAP_OVERFLOW;
  enc->cap_underflow = THEORA_DEF_CAP_UNDERFLOW;
  enc->rate_buffer = THEORA_DEF_RATE_BUFFER;

  enc->multipass_mode = THEORA_DEF_MULTIPASS_MODE;
  enc->multipass_cache_file = THEORA_DEF_MULTIPASS_CACHE_FILE;
}

static void
theora_enc_clear_multipass_cache (GstTheoraEnc * enc)
{
  if (enc->multipass_cache_fd) {
    g_io_channel_shutdown (enc->multipass_cache_fd, TRUE, NULL);
    g_io_channel_unref (enc->multipass_cache_fd);
    enc->multipass_cache_fd = NULL;
  }

  if (enc->multipass_cache_adapter) {
    gst_object_unref (enc->multipass_cache_adapter);
    enc->multipass_cache_adapter = NULL;
  }
}

static void
theora_enc_finalize (GObject * object)
{
  GstTheoraEnc *enc = GST_THEORA_ENC (object);

  GST_DEBUG_OBJECT (enc, "Finalizing");
  if (enc->encoder)
    th_encode_free (enc->encoder);
  th_comment_clear (&enc->comment);
  th_info_clear (&enc->info);
  g_free (enc->multipass_cache_file);

  theora_enc_clear_multipass_cache (enc);

  if (enc->input_state)
    gst_video_codec_state_unref (enc->input_state);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean
theora_enc_flush (GstVideoEncoder * encoder)
{
  GstTheoraEnc *enc = GST_THEORA_ENC (encoder);
  ogg_uint32_t keyframe_force;
  int rate_flags;


  if (enc->input_state == NULL) {
    GST_INFO_OBJECT (enc, "Not configured yet, returning FALSE");

    return FALSE;
  }

  GST_OBJECT_LOCK (enc);
  enc->info.target_bitrate = enc->video_bitrate;
  enc->info.quality = enc->video_quality;
  enc->bitrate_changed = FALSE;
  enc->quality_changed = FALSE;
  GST_OBJECT_UNLOCK (enc);

  if (enc->encoder)
    th_encode_free (enc->encoder);

  enc->encoder = th_encode_alloc (&enc->info);
  /* We ensure this function cannot fail. */
  g_assert (enc->encoder != NULL);
  th_encode_ctl (enc->encoder, TH_ENCCTL_SET_SPLEVEL, &enc->speed_level,
      sizeof (enc->speed_level));
  th_encode_ctl (enc->encoder, TH_ENCCTL_SET_VP3_COMPATIBLE,
      &enc->vp3_compatible, sizeof (enc->vp3_compatible));

  rate_flags = 0;
  if (enc->drop_frames)
    rate_flags |= TH_RATECTL_DROP_FRAMES;
  if (enc->drop_frames)
    rate_flags |= TH_RATECTL_CAP_OVERFLOW;
  if (enc->drop_frames)
    rate_flags |= TH_RATECTL_CAP_UNDERFLOW;
  th_encode_ctl (enc->encoder, TH_ENCCTL_SET_RATE_FLAGS,
      &rate_flags, sizeof (rate_flags));

  if (enc->rate_buffer) {
    th_encode_ctl (enc->encoder, TH_ENCCTL_SET_RATE_BUFFER,
        &enc->rate_buffer, sizeof (enc->rate_buffer));
  } else {
    /* FIXME */
  }

  keyframe_force = enc->keyframe_auto ?
      enc->keyframe_force : enc->keyframe_freq;
  th_encode_ctl (enc->encoder, TH_ENCCTL_SET_KEYFRAME_FREQUENCY_FORCE,
      &keyframe_force, sizeof (keyframe_force));

  /* Get placeholder data */
  if (enc->multipass_cache_fd
      && enc->multipass_mode == MULTIPASS_MODE_FIRST_PASS)
    theora_enc_write_multipass_cache (enc, TRUE, FALSE);

  return TRUE;
}

static gboolean
theora_enc_start (GstVideoEncoder * benc)
{
  GstTheoraEnc *enc;

  GST_DEBUG_OBJECT (benc, "start: init theora");
  enc = GST_THEORA_ENC (benc);

  if (enc->multipass_mode >= MULTIPASS_MODE_FIRST_PASS) {
    GError *err = NULL;

    if (!enc->multipass_cache_file) {
      GST_ELEMENT_ERROR (enc, LIBRARY, SETTINGS, (NULL), (NULL));
      return FALSE;
    }
    enc->multipass_cache_fd =
        g_io_channel_new_file (enc->multipass_cache_file,
        (enc->multipass_mode == MULTIPASS_MODE_FIRST_PASS ? "w" : "r"), &err);

    if (enc->multipass_mode == MULTIPASS_MODE_SECOND_PASS)
      enc->multipass_cache_adapter = gst_adapter_new ();

    if (!enc->multipass_cache_fd) {
      GST_ELEMENT_ERROR (enc, RESOURCE, OPEN_READ, (NULL),
          ("Failed to open multipass cache file: %s", err->message));
      g_error_free (err);
      return FALSE;
    }

    g_io_channel_set_encoding (enc->multipass_cache_fd, NULL, NULL);
  }

  enc->packetno = 0;
  enc->initialised = FALSE;

  return TRUE;
}

static gboolean
theora_enc_stop (GstVideoEncoder * benc)
{
  GstTheoraEnc *enc;

  GST_DEBUG_OBJECT (benc, "stop: clearing theora state");
  enc = GST_THEORA_ENC (benc);

  if (enc->encoder)
    th_encode_free (enc->encoder);
  enc->encoder = NULL;
  th_comment_clear (&enc->comment);
  th_info_clear (&enc->info);

  if (enc->input_state)
    gst_video_codec_state_unref (enc->input_state);
  enc->input_state = NULL;

  /* Everything else is handled in reset() */
  theora_enc_clear_multipass_cache (enc);

  return TRUE;
}

static char *
theora_enc_get_supported_formats (void)
{
  th_enc_ctx *encoder;
  th_info info;
  struct
  {
    th_pixel_fmt pixelformat;
    const char *fourcc;
  } formats[] = {
    {
        TH_PF_420, "I420"}, {
        TH_PF_422, "Y42B"}, {
        TH_PF_444, "Y444"}
  };
  GString *string = NULL;
  guint i;

  th_info_init (&info);
  info.frame_width = 16;
  info.frame_height = 16;
  info.fps_numerator = 25;
  info.fps_denominator = 1;
  for (i = 0; i < G_N_ELEMENTS (formats); i++) {
    info.pixel_fmt = formats[i].pixelformat;

    encoder = th_encode_alloc (&info);
    if (encoder == NULL)
      continue;

    GST_LOG ("format %s is supported", formats[i].fourcc);
    th_encode_free (encoder);

    if (string == NULL) {
      string = g_string_new (formats[i].fourcc);
    } else {
      g_string_append (string, ", ");
      g_string_append (string, formats[i].fourcc);
    }
  }
  th_info_clear (&info);

  return string == NULL ? NULL : g_string_free (string, FALSE);
}

static GstCaps *
theora_enc_getcaps (GstVideoEncoder * encoder, GstCaps * filter)
{
  GstCaps *caps, *ret;
  char *supported_formats, *caps_string;

  supported_formats = theora_enc_get_supported_formats ();
  if (!supported_formats) {
    GST_WARNING ("no supported formats found. Encoder disabled?");
    return gst_caps_new_empty ();
  }

  caps_string = g_strdup_printf ("video/x-raw, "
      "format = (string) { %s }, "
      "framerate = (fraction) [1/MAX, MAX], "
      "width = (int) [ 1, MAX ], " "height = (int) [ 1, MAX ]",
      supported_formats);
  caps = gst_caps_from_string (caps_string);
  g_free (caps_string);
  g_free (supported_formats);
  GST_DEBUG ("Supported caps: %" GST_PTR_FORMAT, caps);

  ret = gst_video_encoder_proxy_getcaps (encoder, caps, filter);
  gst_caps_unref (caps);

  return ret;
}

static gboolean
theora_enc_set_format (GstVideoEncoder * benc, GstVideoCodecState * state)
{
  GstTheoraEnc *enc = GST_THEORA_ENC (benc);
  GstVideoInfo *info = &state->info;

  enc->width = GST_VIDEO_INFO_WIDTH (info);
  enc->height = GST_VIDEO_INFO_HEIGHT (info);

  th_info_clear (&enc->info);
  th_info_init (&enc->info);
  /* Theora has a divisible-by-sixteen restriction for the encoded video size but
   * we can define a picture area using pic_width/pic_height */
  enc->info.frame_width = GST_ROUND_UP_16 (enc->width);
  enc->info.frame_height = GST_ROUND_UP_16 (enc->height);
  enc->info.pic_width = enc->width;
  enc->info.pic_height = enc->height;
  switch (GST_VIDEO_INFO_FORMAT (info)) {
    case GST_VIDEO_FORMAT_I420:
      enc->info.pixel_fmt = TH_PF_420;
      break;
    case GST_VIDEO_FORMAT_Y42B:
      enc->info.pixel_fmt = TH_PF_422;
      break;
    case GST_VIDEO_FORMAT_Y444:
      enc->info.pixel_fmt = TH_PF_444;
      break;
    default:
      g_assert_not_reached ();
  }

  enc->info.fps_numerator = enc->fps_n = GST_VIDEO_INFO_FPS_N (info);
  enc->info.fps_denominator = enc->fps_d = GST_VIDEO_INFO_FPS_D (info);
  enc->info.aspect_numerator = GST_VIDEO_INFO_PAR_N (info);
  enc->info.aspect_denominator = GST_VIDEO_INFO_PAR_D (info);

  enc->info.colorspace = TH_CS_UNSPECIFIED;

  /* Save input state */
  if (enc->input_state)
    gst_video_codec_state_unref (enc->input_state);
  enc->input_state = gst_video_codec_state_ref (state);

  /* as done in theora */
  enc->info.keyframe_granule_shift = _ilog (enc->keyframe_force - 1);
  GST_DEBUG_OBJECT (enc,
      "keyframe_frequency_force is %d, granule shift is %d",
      enc->keyframe_force, enc->info.keyframe_granule_shift);

  theora_enc_flush (benc);
  enc->initialised = TRUE;

  return TRUE;
}

static GstFlowReturn
theora_enc_pre_push (GstVideoEncoder * benc, GstVideoCodecFrame * frame)
{
  GstTheoraEnc *enc = GST_THEORA_ENC (benc);
  guint64 pfn;

  /* see ext/ogg/README; OFFSET_END takes "our" granulepos, OFFSET its
   * time representation */
  /* granulepos from sync frame */
  pfn = frame->presentation_frame_number - frame->distance_from_sync;
  /* correct to correspond to linear running time */
  pfn -= enc->pfn_offset;
  pfn += enc->granulepos_offset + 1;
  /* granulepos */
  GST_BUFFER_OFFSET_END (frame->output_buffer) =
      (pfn << enc->info.keyframe_granule_shift) + frame->distance_from_sync;
  GST_BUFFER_OFFSET (frame->output_buffer) = granulepos_to_timestamp (enc,
      GST_BUFFER_OFFSET_END (frame->output_buffer));

  return GST_FLOW_OK;
}

static GstFlowReturn
theora_push_packet (GstTheoraEnc * enc, ogg_packet * packet)
{
  GstVideoEncoder *benc;
  GstFlowReturn ret;
  GstVideoCodecFrame *frame;

  benc = GST_VIDEO_ENCODER (enc);

  frame = gst_video_encoder_get_oldest_frame (benc);
  if (gst_video_encoder_allocate_output_frame (benc, frame,
          packet->bytes) != GST_FLOW_OK) {
    GST_WARNING_OBJECT (enc, "Could not allocate buffer");
    gst_video_codec_frame_unref (frame);
    ret = GST_FLOW_ERROR;
    goto done;
  }

  if (packet->bytes > 0)
    gst_buffer_fill (frame->output_buffer, 0, packet->packet, packet->bytes);

  /* the second most significant bit of the first data byte is cleared
   * for keyframes */
  if (packet->bytes > 0 && (packet->packet[0] & 0x40) == 0) {
    GST_VIDEO_CODEC_FRAME_SET_SYNC_POINT (frame);
  } else {
    GST_VIDEO_CODEC_FRAME_UNSET_SYNC_POINT (frame);
  }
  enc->packetno++;

  ret = gst_video_encoder_finish_frame (benc, frame);

done:
  return ret;
}

static GstCaps *
theora_set_header_on_caps (GstCaps * caps, GList * buffers)
{
  GstStructure *structure;
  GValue array = { 0 };
  GValue value = { 0 };
  GstBuffer *buffer;
  GList *walk;

  caps = gst_caps_make_writable (caps);
  structure = gst_caps_get_structure (caps, 0);

  /* put copies of the buffers in a fixed list */
  g_value_init (&array, GST_TYPE_ARRAY);

  for (walk = buffers; walk; walk = walk->next) {
    buffer = walk->data;
    g_value_init (&value, GST_TYPE_BUFFER);
    gst_value_set_buffer (&value, buffer);
    gst_value_array_append_value (&array, &value);
    g_value_unset (&value);
  }

  gst_structure_take_value (structure, "streamheader", &array);

  return caps;
}

static void
theora_enc_init_buffer (th_ycbcr_buffer buf, GstVideoFrame * frame)
{
  GstVideoInfo vinfo;
  guint i;

  /* According to Theora developer Timothy Terriberry, the Theora
   * encoder will not use memory outside of pic_width/height, even when
   * the frame size is bigger. The values outside this region will be encoded
   * to default values.
   * Due to this, setting the frame's width/height as the buffer width/height
   * is perfectly ok, even though it does not strictly look ok.
   */

  gst_video_info_init (&vinfo);
  gst_video_info_set_format (&vinfo, GST_VIDEO_FRAME_FORMAT (frame),
      GST_ROUND_UP_16 (GST_VIDEO_FRAME_WIDTH (frame)),
      GST_ROUND_UP_16 (GST_VIDEO_FRAME_HEIGHT (frame)));

  for (i = 0; i < 3; i++) {
    buf[i].width = GST_VIDEO_INFO_COMP_WIDTH (&vinfo, i);
    buf[i].height = GST_VIDEO_INFO_COMP_HEIGHT (&vinfo, i);
    buf[i].data = GST_VIDEO_FRAME_COMP_DATA (frame, i);
    buf[i].stride = GST_VIDEO_FRAME_COMP_STRIDE (frame, i);
  }
}

static gboolean
theora_enc_read_multipass_cache (GstTheoraEnc * enc)
{
  GstBuffer *cache_buf;
  const guint8 *cache_data;
  gsize bytes_read = 0;
  gssize bytes_consumed = 0;
  GIOStatus stat = G_IO_STATUS_NORMAL;
  gboolean done = FALSE;

  while (!done) {
    if (gst_adapter_available (enc->multipass_cache_adapter) == 0) {
      GstMapInfo minfo;

      cache_buf = gst_buffer_new_allocate (NULL, 512, NULL);

      gst_buffer_map (cache_buf, &minfo, GST_MAP_WRITE);

      stat = g_io_channel_read_chars (enc->multipass_cache_fd,
          (gchar *) minfo.data, minfo.size, &bytes_read, NULL);

      if (bytes_read <= 0) {
        gst_buffer_unmap (cache_buf, &minfo);
        gst_buffer_unref (cache_buf);
        break;
      } else {
        gst_buffer_unmap (cache_buf, &minfo);
        gst_buffer_resize (cache_buf, 0, bytes_read);

        gst_adapter_push (enc->multipass_cache_adapter, cache_buf);
      }
    }
    if (gst_adapter_available (enc->multipass_cache_adapter) == 0)
      break;

    bytes_read =
        MIN (gst_adapter_available (enc->multipass_cache_adapter), 512);

    cache_data = gst_adapter_map (enc->multipass_cache_adapter, bytes_read);

    bytes_consumed =
        th_encode_ctl (enc->encoder, TH_ENCCTL_2PASS_IN, (guint8 *) cache_data,
        bytes_read);
    gst_adapter_unmap (enc->multipass_cache_adapter);

    done = bytes_consumed <= 0;
    if (bytes_consumed > 0)
      gst_adapter_flush (enc->multipass_cache_adapter, bytes_consumed);
  }

  if (stat == G_IO_STATUS_ERROR || (stat == G_IO_STATUS_EOF && bytes_read == 0)
      || bytes_consumed < 0) {
    GST_ELEMENT_ERROR (enc, RESOURCE, READ, (NULL),
        ("Failed to read multipass cache file"));
    return FALSE;
  }
  return TRUE;
}

static gboolean
theora_enc_write_multipass_cache (GstTheoraEnc * enc, gboolean begin,
    gboolean eos)
{
  GError *err = NULL;
  GIOStatus stat = G_IO_STATUS_NORMAL;
  gint bytes_read = 0;
  gsize bytes_written = 0;
  gchar *buf;

  if (begin) {
    stat = g_io_channel_seek_position (enc->multipass_cache_fd, 0, G_SEEK_SET,
        &err);

    if (stat == G_IO_STATUS_ERROR) {
      if (eos)
        GST_ELEMENT_WARNING (enc, RESOURCE, WRITE, (NULL),
            ("Failed to seek to beginning of multipass cache file: %s",
                err->message));
      else
        GST_ELEMENT_ERROR (enc, RESOURCE, WRITE, (NULL),
            ("Failed to seek to beginning of multipass cache file: %s",
                err->message));
      g_error_free (err);
      return FALSE;
    }
  }


  do {
    bytes_read =
        th_encode_ctl (enc->encoder, TH_ENCCTL_2PASS_OUT, &buf, sizeof (buf));
    if (bytes_read > 0)
      g_io_channel_write_chars (enc->multipass_cache_fd, buf, bytes_read,
          &bytes_written, &err);
  } while (bytes_read > 0 && bytes_written > 0 && !err);

  if (bytes_read < 0 || err) {
    if (bytes_read < 0) {
      GST_ELEMENT_ERROR (enc, RESOURCE, WRITE, (NULL),
          ("Failed to read multipass cache data: %d", bytes_read));
    } else {
      GST_ELEMENT_ERROR (enc, RESOURCE, WRITE, (NULL),
          ("Failed to write multipass cache file: %s", err->message));
    }
    if (err)
      g_error_free (err);

    return FALSE;
  }

  return TRUE;
}

static void
theora_enc_reset_ts (GstTheoraEnc * enc, GstClockTime running_time, gint pfn)
{
  enc->granulepos_offset =
      gst_util_uint64_scale (running_time, enc->fps_n, GST_SECOND * enc->fps_d);
  enc->timestamp_offset = running_time;
  enc->pfn_offset = pfn;
}

static GstBuffer *
theora_enc_buffer_from_header_packet (GstTheoraEnc * enc, ogg_packet * packet)
{
  GstBuffer *outbuf;

  outbuf =
      gst_video_encoder_allocate_output_buffer (GST_VIDEO_ENCODER (enc),
      packet->bytes);
  gst_buffer_fill (outbuf, 0, packet->packet, packet->bytes);
  GST_BUFFER_OFFSET (outbuf) = 0;
  GST_BUFFER_OFFSET_END (outbuf) = 0;
  GST_BUFFER_TIMESTAMP (outbuf) = GST_CLOCK_TIME_NONE;
  GST_BUFFER_DURATION (outbuf) = GST_CLOCK_TIME_NONE;
  GST_BUFFER_FLAG_SET (outbuf, GST_BUFFER_FLAG_HEADER);

  GST_DEBUG ("created header packet buffer, %u bytes",
      (guint) gst_buffer_get_size (outbuf));
  return outbuf;
}

static GstFlowReturn
theora_enc_handle_frame (GstVideoEncoder * benc, GstVideoCodecFrame * frame)
{
  GstTheoraEnc *enc;
  ogg_packet op;
  GstClockTime timestamp, running_time;
  GstFlowReturn ret;
  gboolean force_keyframe;

  enc = GST_THEORA_ENC (benc);

  /* we keep track of two timelines.
   * - The timestamps from the incoming buffers, which we copy to the outgoing
   *   encoded buffers as-is. We need to do this as we simply forward the
   *   newsegment events.
   * - The running_time of the buffers, which we use to construct the granulepos
   *   in the packets.
   */
  timestamp = frame->pts;

  /* incoming buffers are clipped, so this should be positive */
  running_time =
      gst_segment_to_running_time (&GST_VIDEO_ENCODER_INPUT_SEGMENT (enc),
      GST_FORMAT_TIME, timestamp);

  GST_OBJECT_LOCK (enc);
  if (enc->bitrate_changed) {
    long int bitrate = enc->video_bitrate;

    th_encode_ctl (enc->encoder, TH_ENCCTL_SET_BITRATE, &bitrate,
        sizeof (long int));
    enc->bitrate_changed = FALSE;
  }

  if (enc->quality_changed) {
    long int quality = enc->video_quality;

    th_encode_ctl (enc->encoder, TH_ENCCTL_SET_QUALITY, &quality,
        sizeof (long int));
    enc->quality_changed = FALSE;
  }

  /* see if we need to schedule a keyframe */
  force_keyframe = GST_VIDEO_CODEC_FRAME_IS_FORCE_KEYFRAME (frame);
  GST_OBJECT_UNLOCK (enc);

  if (enc->packetno == 0) {
    /* no packets written yet, setup headers */
    GstCaps *caps;
    GstBuffer *buf;
    GList *buffers = NULL;
    int result;
    GstVideoCodecState *state;

    enc->granulepos_offset = 0;
    enc->timestamp_offset = 0;

    GST_DEBUG_OBJECT (enc, "output headers");
    /* Theora streams begin with three headers; the initial header (with
       most of the codec setup parameters) which is mandated by the Ogg
       bitstream spec.  The second header holds any comment fields.  The
       third header holds the bitstream codebook.  We merely need to
       make the headers, then pass them to libtheora one at a time;
       libtheora handles the additional Ogg bitstream constraints */

    /* create the remaining theora headers */
    th_comment_clear (&enc->comment);
    th_comment_init (&enc->comment);

    while ((result =
            th_encode_flushheader (enc->encoder, &enc->comment, &op)) > 0) {
      buf = theora_enc_buffer_from_header_packet (enc, &op);
      buffers = g_list_prepend (buffers, buf);
    }
    if (result < 0) {
      g_list_foreach (buffers, (GFunc) gst_buffer_unref, NULL);
      g_list_free (buffers);
      goto encoder_disabled;
    }

    buffers = g_list_reverse (buffers);

    /* mark buffers and put on caps */
    caps = gst_caps_new_empty_simple ("video/x-theora");
    caps = theora_set_header_on_caps (caps, buffers);
    state = gst_video_encoder_set_output_state (benc, caps, enc->input_state);

    GST_DEBUG ("here are the caps: %" GST_PTR_FORMAT, state->caps);

    gst_video_codec_state_unref (state);

    gst_video_encoder_negotiate (GST_VIDEO_ENCODER (enc));

    gst_video_encoder_set_headers (benc, buffers);

    theora_enc_reset_ts (enc, running_time, frame->presentation_frame_number);
  }

  {
    th_ycbcr_buffer ycbcr;
    gint res, keyframe_interval;
    GstVideoFrame vframe;

    if (force_keyframe) {
      /* if we want a keyframe, temporarily reset the max keyframe interval
       * to 1, which will cause libtheora to emit one. There is no API to
       * request a keyframe at the moment. */
      keyframe_interval = 1;
      th_encode_ctl (enc->encoder, TH_ENCCTL_SET_KEYFRAME_FREQUENCY_FORCE,
          &keyframe_interval, sizeof (keyframe_interval));
    }

    if (enc->multipass_cache_fd
        && enc->multipass_mode == MULTIPASS_MODE_SECOND_PASS) {
      if (!theora_enc_read_multipass_cache (enc)) {
        ret = GST_FLOW_ERROR;
        goto multipass_read_failed;
      }
    }

    gst_video_frame_map (&vframe, &enc->input_state->info, frame->input_buffer,
        GST_MAP_READ);
    theora_enc_init_buffer (ycbcr, &vframe);

    res = th_encode_ycbcr_in (enc->encoder, ycbcr);
    gst_video_frame_unmap (&vframe);

    /* none of the failure cases can happen here */
    g_assert (res == 0);

    if (enc->multipass_cache_fd
        && enc->multipass_mode == MULTIPASS_MODE_FIRST_PASS) {
      if (!theora_enc_write_multipass_cache (enc, FALSE, FALSE)) {
        ret = GST_FLOW_ERROR;
        goto multipass_write_failed;
      }
    }

    ret = GST_FLOW_OK;
    while (th_encode_packetout (enc->encoder, 0, &op)) {
      /* Reset the max keyframe interval to its original state, and reset
       * the flag so we don't create more keyframes if we loop */
      if (force_keyframe) {
        keyframe_interval =
            enc->keyframe_auto ? enc->keyframe_force : enc->keyframe_freq;
        th_encode_ctl (enc->encoder, TH_ENCCTL_SET_KEYFRAME_FREQUENCY_FORCE,
            &keyframe_interval, sizeof (keyframe_interval));
        force_keyframe = FALSE;
      }

      ret = theora_push_packet (enc, &op);
      if (ret != GST_FLOW_OK)
        goto beach;
    }
  }

beach:
  gst_video_codec_frame_unref (frame);
  return ret;

  /* ERRORS */
multipass_read_failed:
  {
    gst_video_codec_frame_unref (frame);
    return ret;
  }
multipass_write_failed:
  {
    gst_video_codec_frame_unref (frame);
    return ret;
  }
encoder_disabled:
  {
    gst_video_codec_frame_unref (frame);
    GST_ELEMENT_ERROR (enc, STREAM, ENCODE, (NULL),
        ("libtheora has been compiled with the encoder disabled"));
    return GST_FLOW_ERROR;
  }
}

static GstFlowReturn
theora_enc_finish (GstVideoEncoder * benc)
{
  GstTheoraEnc *enc;
  ogg_packet op;

  enc = GST_THEORA_ENC (benc);

  if (enc->initialised) {
    /* push last packet with eos flag, should not be called */
    while (th_encode_packetout (enc->encoder, 1, &op)) {
      theora_push_packet (enc, &op);
    }
  }
  if (enc->initialised && enc->multipass_cache_fd
      && enc->multipass_mode == MULTIPASS_MODE_FIRST_PASS)
    theora_enc_write_multipass_cache (enc, TRUE, TRUE);

  theora_enc_clear_multipass_cache (enc);

  return GST_FLOW_OK;
}

static gboolean
theora_enc_propose_allocation (GstVideoEncoder * encoder, GstQuery * query)
{
  gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL);

  return GST_VIDEO_ENCODER_CLASS (parent_class)->propose_allocation (encoder,
      query);
}

static void
theora_enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstTheoraEnc *enc = GST_THEORA_ENC (object);

  switch (prop_id) {
    case PROP_BITRATE:
      GST_OBJECT_LOCK (enc);
      enc->video_bitrate = g_value_get_int (value) * 1000;
      enc->video_quality = 0;
      enc->bitrate_changed = TRUE;
      GST_OBJECT_UNLOCK (enc);
      break;
    case PROP_QUALITY:
      GST_OBJECT_LOCK (enc);
      if (GST_STATE (enc) >= GST_STATE_PAUSED && enc->video_quality == 0) {
        GST_WARNING_OBJECT (object, "Can't change from bitrate to quality mode"
            " while playing");
      } else {
        enc->video_quality = g_value_get_int (value);
        enc->video_bitrate = 0;
        enc->quality_changed = TRUE;
      }
      GST_OBJECT_UNLOCK (enc);
      break;
    case PROP_KEYFRAME_AUTO:
      enc->keyframe_auto = g_value_get_boolean (value);
      break;
    case PROP_KEYFRAME_FREQ:
      enc->keyframe_freq = g_value_get_int (value);
      break;
    case PROP_KEYFRAME_FREQ_FORCE:
      enc->keyframe_force = g_value_get_int (value);
      break;
    case PROP_SPEEDLEVEL:
      enc->speed_level = g_value_get_int (value);
      break;
    case PROP_VP3_COMPATIBLE:
      enc->vp3_compatible = g_value_get_boolean (value);
      break;
    case PROP_DROP_FRAMES:
      enc->drop_frames = g_value_get_boolean (value);
      break;
    case PROP_CAP_OVERFLOW:
      enc->cap_overflow = g_value_get_boolean (value);
      break;
    case PROP_CAP_UNDERFLOW:
      enc->cap_underflow = g_value_get_boolean (value);
      break;
    case PROP_RATE_BUFFER:
      enc->rate_buffer = g_value_get_int (value);
      break;
    case PROP_MULTIPASS_CACHE_FILE:
      enc->multipass_cache_file = g_value_dup_string (value);
      break;
    case PROP_MULTIPASS_MODE:
      enc->multipass_mode = g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
theora_enc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstTheoraEnc *enc = GST_THEORA_ENC (object);

  switch (prop_id) {
    case PROP_BITRATE:
      GST_OBJECT_LOCK (enc);
      g_value_set_int (value, enc->video_bitrate / 1000);
      GST_OBJECT_UNLOCK (enc);
      break;
    case PROP_QUALITY:
      GST_OBJECT_LOCK (enc);
      g_value_set_int (value, enc->video_quality);
      GST_OBJECT_UNLOCK (enc);
      break;
    case PROP_KEYFRAME_AUTO:
      g_value_set_boolean (value, enc->keyframe_auto);
      break;
    case PROP_KEYFRAME_FREQ:
      g_value_set_int (value, enc->keyframe_freq);
      break;
    case PROP_KEYFRAME_FREQ_FORCE:
      g_value_set_int (value, enc->keyframe_force);
      break;
    case PROP_SPEEDLEVEL:
      g_value_set_int (value, enc->speed_level);
      break;
    case PROP_VP3_COMPATIBLE:
      g_value_set_boolean (value, enc->vp3_compatible);
      break;
    case PROP_DROP_FRAMES:
      g_value_set_boolean (value, enc->drop_frames);
      break;
    case PROP_CAP_OVERFLOW:
      g_value_set_boolean (value, enc->cap_overflow);
      break;
    case PROP_CAP_UNDERFLOW:
      g_value_set_boolean (value, enc->cap_underflow);
      break;
    case PROP_RATE_BUFFER:
      g_value_set_int (value, enc->rate_buffer);
      break;
    case PROP_MULTIPASS_CACHE_FILE:
      g_value_set_string (value, enc->multipass_cache_file);
      break;
    case PROP_MULTIPASS_MODE:
      g_value_set_enum (value, enc->multipass_mode);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}
