/* GStreamer mpeg2enc (mjpegtools) wrapper
 * (c) 2003 Ronald Bultje <rbultje@ronald.bitfreak.net>
 * (c) 2006 Mark Nauwelaerts <manauw@skynet.be>
 *
 * gstmpeg2enc.cc: gstreamer mpeg2enc wrapping
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
 * SECTION:element-mpeg2enc
 * @see_also: mpeg2dec
 *
 * This element encodes raw video into an MPEG-1/2 elementary stream using the
 * [mjpegtools](http://mjpeg.sourceforge.net/) library.
 *
 * Documentation on MPEG encoding in general can be found in the 
 * [MJPEG Howto](https://sourceforge.net/docman/display_doc.php?docid=3456&group_id=5776)
 * and on the various available parameters in the documentation
 * of the mpeg2enc tool in particular, which shares options with this element.
 *
 * ## Example pipeline
 *
 * |[
 * gst-launch-1.0 videotestsrc num-buffers=1000 ! mpeg2enc ! filesink location=videotestsrc.m1v
 * ]| This example pipeline will encode a test video source to a an MPEG1
 * elementary stream (with Generic MPEG1 profile).
 *
 * Likely, the #GstMpeg2enc:format property
 * is most important, as it selects the type of MPEG stream that is produced.
 * In particular, default property values are dependent on the format,
 * and can even be forcibly restrained to certain pre-sets (and thereby ignored).
 * Note that the (S)VCD profiles also restrict the image size, so some scaling
 * may be needed to accommodate this.  The so-called generic profiles (as used
 * in the example above) allow most parameters to be adjusted.
 *
 * |[
 * gst-launch-1.0 videotestsrc num-buffers=1000 ! videoscale ! mpeg2enc format=1 norm=p ! filesink location=videotestsrc.m1v
 * ]| This will produce an MPEG1 profile stream according to VCD2.0 specifications
 * for PAL #GstMpeg2enc:norm (as the image height is dependent on video norm).
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/glib-compat-private.h>
#include "gstmpeg2enc.hh"

GST_DEBUG_CATEGORY (mpeg2enc_debug);

#define COMMON_VIDEO_CAPS \
  "width = (int) [ 16, 4096 ], " \
  "height = (int) [ 16, 4096 ], " \
  "framerate = " \
  " (fraction) { 24000/1001, 24/1, 25/1, 30000/1001, 30/1, 50/1, 60000/1001 }"

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw, format = (string) I420, " COMMON_VIDEO_CAPS)
    );

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/mpeg, "
        "systemstream = (boolean) false, "
        "mpegversion = (int) { 1, 2 }, " COMMON_VIDEO_CAPS)
    );


static void gst_mpeg2enc_finalize (GObject * object);
static void gst_mpeg2enc_reset (GstMpeg2enc * enc);

static gboolean gst_mpeg2enc_start (GstVideoEncoder * video_encoder);
static gboolean gst_mpeg2enc_stop (GstVideoEncoder * video_encoder);
static gboolean gst_mpeg2enc_set_format (GstVideoEncoder *
    video_encoder, GstVideoCodecState * state);
static GstCaps * gst_mpeg2enc_getcaps (GstVideoEncoder *
    video_encoder, GstCaps * filter);
static GstFlowReturn gst_mpeg2enc_handle_frame (GstVideoEncoder *
    video_encoder, GstVideoCodecFrame * frame);
static gboolean gst_mpeg2enc_sink_event (GstVideoEncoder *
    video_encoder, GstEvent * event);

static GstFlowReturn gst_mpeg2enc_finish (GstVideoEncoder * video_encoder);
static void gst_mpeg2enc_loop (GstVideoEncoder * video_encoder);

static void gst_mpeg2enc_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);
static void gst_mpeg2enc_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static gboolean gst_mpeg2enc_src_activate_mode (GstPad * pad, GstObject * parent,
    GstPadMode mode, gboolean active);
static gboolean mpeg2enc_element_init (GstPlugin * plugin);

#define gst_mpeg2enc_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstMpeg2enc, gst_mpeg2enc, GST_TYPE_VIDEO_ENCODER,
    G_IMPLEMENT_INTERFACE (GST_TYPE_PRESET, NULL));
GST_ELEMENT_REGISTER_DEFINE_CUSTOM (mpeg2enc, mpeg2enc_element_init);

static void
gst_mpeg2enc_class_init (GstMpeg2encClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstVideoEncoderClass *video_encoder_class = GST_VIDEO_ENCODER_CLASS (klass);

  GST_DEBUG_CATEGORY_INIT (mpeg2enc_debug, "mpeg2enc", 0, "MPEG1/2 encoder");

  object_class->set_property = gst_mpeg2enc_set_property;
  object_class->get_property = gst_mpeg2enc_get_property;

  /* register properties */
  GstMpeg2EncOptions::initProperties (object_class);

  object_class->finalize = GST_DEBUG_FUNCPTR (gst_mpeg2enc_finalize);

#if 0
  element_class->change_state = GST_DEBUG_FUNCPTR (gst_mpeg2enc_change_state);
#endif

  gst_element_class_add_static_pad_template (element_class, &src_template);
  gst_element_class_add_static_pad_template (element_class, &sink_template);

  gst_element_class_set_static_metadata (element_class,
      "mpeg2enc video encoder", "Codec/Encoder/Video",
      "High-quality MPEG-1/2 video encoder",
      "Andrew Stevens <andrew.stevens@nexgo.de>, "
      "Ronald Bultje <rbultje@ronald.bitfreak.net>");

  video_encoder_class->start = GST_DEBUG_FUNCPTR (gst_mpeg2enc_start);
  video_encoder_class->stop = GST_DEBUG_FUNCPTR (gst_mpeg2enc_stop);
  video_encoder_class->handle_frame = GST_DEBUG_FUNCPTR (gst_mpeg2enc_handle_frame);
  video_encoder_class->set_format = GST_DEBUG_FUNCPTR (gst_mpeg2enc_set_format);
  video_encoder_class->finish = GST_DEBUG_FUNCPTR (gst_mpeg2enc_finish);
  //video_encoder_class->pre_push = GST_DEBUG_FUNCPTR (gst_mpeg2enc_pre_push);
  video_encoder_class->sink_event = GST_DEBUG_FUNCPTR (gst_mpeg2enc_sink_event);
  video_encoder_class->getcaps = GST_DEBUG_FUNCPTR (gst_mpeg2enc_getcaps);
}

static void
gst_mpeg2enc_finalize (GObject * object)
{
  GstMpeg2enc *enc = GST_MPEG2ENC (object);

  /* clean up */
  gst_mpeg2enc_reset (enc);

  delete enc->options;

  g_mutex_clear (&enc->tlock);
  g_cond_clear (&enc->cond);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_mpeg2enc_init (GstMpeg2enc * enc)
{
  enc->options = new GstMpeg2EncOptions ();
  enc->encoder = NULL;

  g_mutex_init (&enc->tlock);
  g_cond_init (&enc->cond);
  enc->started = FALSE;

  gst_pad_set_activatemode_function (GST_VIDEO_ENCODER_SRC_PAD (enc),
      GST_DEBUG_FUNCPTR (gst_mpeg2enc_src_activate_mode));

  gst_mpeg2enc_reset (enc);
}

static gboolean
gst_mpeg2enc_src_activate_mode (GstPad * pad, GstObject * parent,
    GstPadMode mode, gboolean active)
{
  gboolean result = TRUE;
  GstMpeg2enc *enc;

  enc = GST_MPEG2ENC (parent);

  if (mode != GST_PAD_MODE_PUSH)
    return FALSE;

  if (active) {
    /* setcaps will start task once encoder is setup */
  } else {
    /* can only end the encoding loop by forcing eos */
    GST_MPEG2ENC_MUTEX_LOCK (enc);
    enc->eos = TRUE;
    enc->srcresult = GST_FLOW_FLUSHING;
    GST_MPEG2ENC_SIGNAL (enc);
    GST_MPEG2ENC_MUTEX_UNLOCK (enc);
  }

  return result;
}

static void
gst_mpeg2enc_reset (GstMpeg2enc * enc)
{
  enc->eos = FALSE;
  enc->srcresult = GST_FLOW_OK;

  /* in case of error'ed ending */
  if (enc->pending_frame) {
    gst_video_encoder_finish_frame (GST_VIDEO_ENCODER (enc), enc->pending_frame);
    enc->pending_frame = NULL;
  }

  if (enc->encoder) {
    delete enc->encoder;

    enc->encoder = NULL;
  }
}

static gboolean
gst_mpeg2enc_start (GstVideoEncoder * video_encoder)
{
  GstMpeg2enc *enc = GST_MPEG2ENC (video_encoder);

  GST_DEBUG_OBJECT (video_encoder, "start");

  if (!enc->options) {
    GST_ELEMENT_ERROR (enc, LIBRARY, INIT,
        ("Failed to get default encoder options"), (NULL));
    return FALSE;
  }
  /* start task to create multiplexor and start muxing */
  if (G_UNLIKELY (enc->srcresult != GST_FLOW_OK)) {
    GST_ELEMENT_ERROR (enc, LIBRARY, INIT,
        ("Invalid encoder state"), (NULL));
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_mpeg2enc_stop (GstVideoEncoder * video_encoder)
{
  gboolean result = TRUE;
  GstMpeg2enc *enc;

  GST_DEBUG_OBJECT (video_encoder, "stop");

  enc = GST_MPEG2ENC (video_encoder);

  /* can only end the encoding loop by forcing eos */
  GST_MPEG2ENC_MUTEX_LOCK (enc);
  enc->eos = TRUE;
  enc->srcresult = GST_FLOW_FLUSHING;
  GST_MPEG2ENC_SIGNAL (enc);
  GST_MPEG2ENC_MUTEX_UNLOCK (enc);

  /* encoding loop should have ended now and can be joined */
  if (enc->started) {
    result = gst_pad_stop_task (video_encoder->srcpad);
    enc->started = FALSE;
  }


  GST_MPEG2ENC_MUTEX_LOCK (enc);
  gst_mpeg2enc_reset (enc);
  GST_MPEG2ENC_MUTEX_UNLOCK (enc);

  return result;
}



/* some (!) coding to get caps depending on the video norm and chosen format */
static void
gst_mpeg2enc_add_fps (GstStructure * structure, gint fpss[])
{
  GValue list = { 0, }, fps = {
  0,};
  guint n;

  g_value_init (&list, GST_TYPE_LIST);
  g_value_init (&fps, GST_TYPE_FRACTION);
  for (n = 0; fpss[n] != 0; n += 2) {
    gst_value_set_fraction (&fps, fpss[n], fpss[n + 1]);
    gst_value_list_append_value (&list, &fps);
  }
  gst_structure_set_value (structure, "framerate", &list);
  g_value_unset (&list);
  g_value_unset (&fps);
}

static inline gint *
gst_mpeg2enc_get_fps (GstMpeg2enc * enc)
{
  static gint fps_pal[]
  = { 24, 1, 25, 1, 50, 1, 0 };
  static gint fps_ntsc[]
  = { 24000, 1001, 24, 1, 30000, 1001, 30, 1, 60000, 1001, 0 };
  static gint fps_all[]
  = { 24000, 1001, 24, 1, 30000, 1001, 30, 1, 60000, 1001, 25, 1, 50, 1, 0 };

  if (enc->options->norm == 'n')
    return fps_ntsc;
  else if (enc->options->norm == 0)
    return fps_all;
  else
    return fps_pal;
}

static gboolean
gst_mpeg2enc_set_format (GstVideoEncoder * video_encoder, GstVideoCodecState * state)
{
  GstVideoCodecState *output_state;
  GstMpeg2enc *enc;
  GstCaps *caps;

  enc = GST_MPEG2ENC (video_encoder);

  GST_DEBUG_OBJECT (video_encoder, "set_format");
  /* Store input state */
  if (enc->input_state)
    gst_video_codec_state_unref (enc->input_state);
  enc->input_state = gst_video_codec_state_ref (state);

  /* does not go well to restart stream mid-way */
  if (enc->encoder != NULL)
    goto refuse_renegotiation;


  /* since mpeg encoder does not really check, let's check caps */
  if (GST_VIDEO_INFO_FORMAT (&state->info) != GST_VIDEO_FORMAT_I420)
    goto refuse_caps;

  caps = gst_caps_new_simple ("video/mpeg",
      "systemstream", G_TYPE_BOOLEAN, FALSE,
      "mpegversion", G_TYPE_INT, (enc->options->mpeg == 1)?1:2, NULL);

  output_state =
    gst_video_encoder_set_output_state (video_encoder,
    caps, state);
  gst_video_codec_state_unref (output_state);

  gst_video_encoder_negotiate (GST_VIDEO_ENCODER (enc));

  return TRUE;

refuse_caps:
  {
    GST_WARNING_OBJECT (enc, "refused caps %" GST_PTR_FORMAT, state->caps);

    return FALSE;
  }
refuse_renegotiation:
  {
    GST_WARNING_OBJECT (enc, "refused renegotiation (to %" GST_PTR_FORMAT ")",
        state->caps);

    return FALSE;
  }
}

static GstStructure *
gst_mpeg2enc_structure_from_norm (GstMpeg2enc * enc, gint horiz,
    gint pal_v, gint ntsc_v)
{
  GstStructure *structure;

  structure = gst_structure_new ("video/x-raw",
      "format", G_TYPE_STRING, "I420", NULL);

  switch (enc->options->norm) {
    case 0:
    {
      GValue list = { 0, }
      , val = {
      0,};

      g_value_init (&list, GST_TYPE_LIST);
      g_value_init (&val, G_TYPE_INT);
      g_value_set_int (&val, pal_v);
      gst_value_list_append_value (&list, &val);
      g_value_set_int (&val, ntsc_v);
      gst_value_list_append_value (&list, &val);
      gst_structure_set_value (structure, "height", &list);
      g_value_unset (&list);
      g_value_unset (&val);
      break;
    }
    case 'n':
      gst_structure_set (structure, "height", G_TYPE_INT, ntsc_v,
          (void *) NULL);
      break;
    default:
      gst_structure_set (structure, "height", G_TYPE_INT, pal_v, (void *) NULL);
      break;
  }
  gst_structure_set (structure, "width", G_TYPE_INT, horiz, (void *) NULL);
  gst_mpeg2enc_add_fps (structure, gst_mpeg2enc_get_fps (enc));

  return structure;
}

static GstCaps *
gst_mpeg2enc_getcaps (GstVideoEncoder * video_encoder, GstCaps * filter)
{
  GstCaps *caps;
  GstMpeg2enc *enc = GST_MPEG2ENC (video_encoder);

  caps = gst_pad_get_current_caps (video_encoder->sinkpad);
  if (caps)
    return caps;

  switch (enc->options->format) {
    case 1:                    /* vcd */
    case 2:                    /* user vcd */
      caps = gst_caps_new_full (gst_mpeg2enc_structure_from_norm (enc,
              352, 288, 240), NULL);
      break;
    case 4:                    /* svcd */
    case 5:                    /* user svcd */
      caps = gst_caps_new_full (gst_mpeg2enc_structure_from_norm (enc,
              480, 576, 480), NULL);
      break;
    case 6:                    /* vcd stills */
      /* low resolution */
      caps = gst_caps_new_full (gst_mpeg2enc_structure_from_norm (enc,
              352, 288, 240), NULL);
      /* high resolution */
      gst_caps_append_structure (caps,
          gst_mpeg2enc_structure_from_norm (enc, 704, 576, 480));
      break;
    case 7:                    /* svcd stills */
      /* low resolution */
      caps = gst_caps_new_full (gst_mpeg2enc_structure_from_norm (enc,
              480, 576, 480), NULL);
      /* high resolution */
      gst_caps_append_structure (caps,
          gst_mpeg2enc_structure_from_norm (enc, 704, 576, 480));
      break;
    case 0:
    case 3:
    case 8:
    case 9:
    default:
      caps = gst_caps_copy (gst_pad_get_pad_template_caps (video_encoder->sinkpad));
      gst_mpeg2enc_add_fps (gst_caps_get_structure (caps, 0),
          gst_mpeg2enc_get_fps (enc));
      break;
  }

  return caps;
}

static GstFlowReturn
gst_mpeg2enc_finish (GstVideoEncoder * video_encoder)
{
  GstMpeg2enc *enc;

  enc = GST_MPEG2ENC (video_encoder);

  GST_DEBUG_OBJECT (video_encoder, "finish");

  /* inform the encoding task that it can stop now */
  GST_MPEG2ENC_MUTEX_LOCK (enc);
  enc->eos = TRUE;
  GST_MPEG2ENC_SIGNAL (enc);
  GST_MPEG2ENC_MUTEX_UNLOCK (enc);

  return GST_FLOW_OK;
}

static gboolean
gst_mpeg2enc_sink_event (GstVideoEncoder * video_encoder, GstEvent * event)
{
  GstMpeg2enc *enc;
  gboolean result = TRUE;

  enc = GST_MPEG2ENC (video_encoder);
  GST_DEBUG_OBJECT (video_encoder, "sink_event");

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_FLUSH_START:
      /* forward event */
      result = GST_VIDEO_ENCODER_CLASS (parent_class)->sink_event (video_encoder, event);

      /* no special action as there is not much to flush;
       * neither is it possible to halt the mpeg encoding loop */
      goto done;
    case GST_EVENT_FLUSH_STOP:
      /* forward event */
      result = GST_VIDEO_ENCODER_CLASS (parent_class)->sink_event (video_encoder, event);
      if (!result)
        goto done;

      /* this clears the error state in case of a failure in encoding task;
       * so handle_frame function can carry on again */
      GST_MPEG2ENC_MUTEX_LOCK (enc);
      enc->srcresult = GST_FLOW_OK;
      GST_MPEG2ENC_MUTEX_UNLOCK (enc);
      goto done;
    case GST_EVENT_EOS:
      /* inform the encoding task that it can stop now */
      GST_MPEG2ENC_MUTEX_LOCK (enc);
      enc->eos = TRUE;
      GST_MPEG2ENC_SIGNAL (enc);
      GST_MPEG2ENC_MUTEX_UNLOCK (enc);

      /* eat this event for now, task will send eos when finished */
      gst_event_unref (event);
      goto done;
    default:
      /* for a serialized event, wait until an earlier buffer is gone,
       * though this is no guarantee as to when the encoder is done with it */
      if (GST_EVENT_IS_SERIALIZED (event)) {
        GST_MPEG2ENC_MUTEX_LOCK (enc);
        while (enc->pending_frame != NULL)
          GST_MPEG2ENC_WAIT (enc);
        GST_MPEG2ENC_MUTEX_UNLOCK (enc);
      }
      break;
  }

  result = GST_VIDEO_ENCODER_CLASS (parent_class)->sink_event (video_encoder, event);

done:
  return result;
}

static void
gst_mpeg2enc_loop (GstVideoEncoder * video_encoder)
{
  GstMpeg2enc *enc = GST_MPEG2ENC (video_encoder);

  GST_DEBUG_OBJECT (enc, "encoding task loop:START");
  /* do not try to resume or start when output problems;
   * also ensures a proper (forced) state change */
  if (enc->srcresult != GST_FLOW_OK) {
    GST_MPEG2ENC_MUTEX_LOCK (enc);
    enc->srcresult = GST_FLOW_ERROR;
    GST_MPEG2ENC_SIGNAL (enc);
    GST_MPEG2ENC_MUTEX_UNLOCK (enc);
    goto ignore;
  }
  GST_DEBUG_OBJECT (enc, "encoding task loop: flow OK");

  if (!enc->encoder) {
    gboolean ret;
    GstClockTime latency;
    GstVideoInfo *info = &enc->input_state->info;

    /* create new encoder with these settings */
    enc->encoder = new GstMpeg2Encoder (enc->options, GST_ELEMENT (video_encoder),
                        gst_pad_get_current_caps(video_encoder->sinkpad));

    ret = enc->encoder->setup ();

    /* We must have a fixated max GOP size after setting up */
    g_assert (enc->options->max_GOP_size != -1);

    /* mjpeg tools outputs encoded data on a GOP basis, our latency is
     * thus at least max_GOP_size. It also introduces a 5-frame delay on
     * top of that, this was determined empirically, a surface level inspection
     * of the code didn't show where and why that delay is introduced exactly,
     * and this is not specifically documented, but it needs to be taken
     * into account when calculating the latency
     */
    if (GST_VIDEO_INFO_FPS_D (info) == 0 || GST_VIDEO_INFO_FPS_N (info) == 0) {
      /* Assume 25fps for unknown framerates. Better than reporting
       * that we introduce no latency while we actually do
       */
      latency = gst_util_uint64_scale (enc->options->max_GOP_size + 5,
          1 * GST_SECOND, 25);
    } else {
      latency = gst_util_uint64_scale (enc->options->max_GOP_size + 5,
          GST_VIDEO_INFO_FPS_D (info) * GST_SECOND, GST_VIDEO_INFO_FPS_N (info));
    }

    gst_video_encoder_set_latency (video_encoder, latency, latency);

    /* SeqEncoder init requires at least two frames */
    enc->encoder->init ();

    if (!ret) {
      GST_MPEG2ENC_MUTEX_LOCK (enc);
      enc->srcresult = GST_FLOW_NOT_NEGOTIATED;
      GST_MPEG2ENC_SIGNAL (enc);
      GST_MPEG2ENC_MUTEX_UNLOCK (enc);
      goto encoder;
    }
  }
  GST_DEBUG_OBJECT (enc, "encoding task loop: setup and init DONE");

  /* note that init performs a pre-fill and therefore needs buffers */
  /* task will stay in here during all of the encoding */
  enc->encoder->encode ();
  GST_DEBUG_OBJECT (enc, "encoding task loop: encode DONE");
  /* if not well and truly eos, something strange happened  */
  if (!enc->eos) {
    GST_ERROR_OBJECT (enc, "encoding task ended without being eos");
    /* notify the handle_frame function that it's over */
    GST_MPEG2ENC_MUTEX_LOCK (enc);
    enc->srcresult = GST_FLOW_ERROR;
    GST_MPEG2ENC_SIGNAL (enc);
    GST_MPEG2ENC_MUTEX_UNLOCK (enc);
  } else {
    /* send eos if this was not a forced stop or other problem */
    GST_DEBUG_OBJECT (enc, "encoding task loop: eos REACHED");
    if (enc->srcresult == GST_FLOW_OK) {
      gst_pad_push_event (video_encoder->srcpad, gst_event_new_eos ());
      GST_DEBUG_OBJECT (enc, "encoding task loop: eos SENT");
    }
    goto eos;
  }

  /* fall-through */
done:
  {
    /* no need to run wildly, stopped elsewhere, e.g. state change */
    GST_DEBUG_OBJECT (enc, "pausing encoding task");
    gst_pad_pause_task (video_encoder->srcpad);

    return;
  }
encoder:
  {
    GST_ELEMENT_ERROR (enc, CORE, NEGOTIATION, (NULL),
       ("encoder setup failed"));
    if (enc->encoder) {
      delete enc->encoder;

      enc->encoder = NULL;
    }

    goto done;
  }
eos:
  {
    GST_DEBUG_OBJECT (enc, "encoding task reached eos");
    goto done;
  }
ignore:
  {
    GST_DEBUG_OBJECT (enc, "not looping because encoding task encountered %s",
        gst_flow_get_name (enc->srcresult));
    goto done;
  }
}

static GstFlowReturn
gst_mpeg2enc_handle_frame (GstVideoEncoder *video_encoder, GstVideoCodecFrame *frame)
{
  GstMpeg2enc *enc = GST_MPEG2ENC (video_encoder);

  GST_DEBUG_OBJECT (video_encoder, "handle_frame");
  GST_MPEG2ENC_MUTEX_LOCK (enc);

  if (G_UNLIKELY (enc->eos))
    goto eos;
  GST_DEBUG_OBJECT (video_encoder, "handle_frame: NOT eos");

  if (G_UNLIKELY (enc->srcresult != GST_FLOW_OK))
    goto ignore;
  GST_DEBUG_OBJECT (video_encoder, "handle_frame: flow OK");

  /* If the encoder is busy with a previous frame still, wait
   * for it to be done */
  if (enc->pending_frame != NULL) {
    do {
      GST_VIDEO_ENCODER_STREAM_UNLOCK (enc);
      GST_MPEG2ENC_WAIT (enc);
      GST_VIDEO_ENCODER_STREAM_LOCK (enc);

      /* Re-check the srcresult, since we waited */
      if (G_UNLIKELY (enc->srcresult != GST_FLOW_OK))
        goto ignore;
    } while (enc->pending_frame != NULL);
  }

  /* frame will be released by task */
  enc->pending_frame = frame;

  if (!enc->started) {
    GST_DEBUG_OBJECT (video_encoder, "handle_frame: START task");
    gst_pad_start_task (video_encoder->srcpad, (GstTaskFunction) gst_mpeg2enc_loop, enc, NULL);
    enc->started = TRUE;
  }

  /* things look good, now inform the encoding task that a frame is ready */
  GST_MPEG2ENC_SIGNAL (enc);
  GST_MPEG2ENC_MUTEX_UNLOCK (enc);

  return GST_FLOW_OK;
eos:
  {
    GST_DEBUG_OBJECT (enc, "ignoring frame at end-of-stream");
    GST_MPEG2ENC_MUTEX_UNLOCK (enc);

    gst_video_encoder_finish_frame (video_encoder, frame);

    return GST_FLOW_EOS;
  }
ignore:
  {
    GstFlowReturn ret = enc->srcresult;

    GST_DEBUG_OBJECT (enc,
        "ignoring frame because encoding task encountered %s",
        gst_flow_get_name (enc->srcresult));

    enc->eos = TRUE;

    GST_MPEG2ENC_MUTEX_UNLOCK (enc);

    gst_video_encoder_finish_frame (video_encoder, frame);

    return ret;
  }
}

static void
gst_mpeg2enc_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GST_MPEG2ENC (object)->options->getProperty (prop_id, value);
}

static void
gst_mpeg2enc_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GST_MPEG2ENC (object)->options->setProperty (prop_id, value);
}

#ifndef GST_DISABLE_GST_DEBUG

static mjpeg_log_handler_t old_handler = NULL;

/* note that this will affect all mjpegtools elements/threads */
static void
gst_mpeg2enc_log_callback (log_level_t level, const char *message)
{
  GstDebugLevel gst_level;

#if GST_MJPEGTOOLS_API >= 10903
  static const gint mjpeg_log_error = mjpeg_loglev_t ("error");
  static const gint mjpeg_log_warn = mjpeg_loglev_t ("warn");
  static const gint mjpeg_log_info = mjpeg_loglev_t ("info");
  static const gint mjpeg_log_debug = mjpeg_loglev_t ("debug");
#else
  static const gint mjpeg_log_error = LOG_ERROR;
  static const gint mjpeg_log_warn = LOG_WARN;
  static const gint mjpeg_log_info = LOG_INFO;
  static const gint mjpeg_log_debug = LOG_DEBUG;
#endif

  if (level == mjpeg_log_error) {
    gst_level = GST_LEVEL_ERROR;
  } else if (level == mjpeg_log_warn) {
    gst_level = GST_LEVEL_WARNING;
  } else if (level == mjpeg_log_info) {
    gst_level = GST_LEVEL_INFO;
  } else if (level == mjpeg_log_debug) {
    gst_level = GST_LEVEL_DEBUG;
  } else {
    gst_level = GST_LEVEL_INFO;
  }

  /* message could have a % in it, do not segfault in such case */
  gst_debug_log (mpeg2enc_debug, gst_level, "", "", 0, NULL, "%s", message);

  /* chain up to the old handler;
   * this could actually be a handler from another mjpegtools based
   * plugin; in which case messages can come out double or from
   * the wrong plugin (element)... */
  old_handler (level, message);
}
#endif

static gboolean
mpeg2enc_element_init (GstPlugin * plugin)
{
#ifndef GST_DISABLE_GST_DEBUG
  old_handler = mjpeg_log_set_handler (gst_mpeg2enc_log_callback);
  g_assert (old_handler != NULL);
#endif
  /* in any case, we do not want default handler output */
  mjpeg_default_handler_verbosity (0);

  return gst_element_register (plugin, "mpeg2enc",
      GST_RANK_MARGINAL, GST_TYPE_MPEG2ENC);
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  return GST_ELEMENT_REGISTER (mpeg2enc, plugin);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    mpeg2enc,
    "High-quality MPEG-1/2 video encoder",
    plugin_init, VERSION, "GPL", GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
