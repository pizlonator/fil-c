/* MPEG-PS muxer plugin for GStreamer
 * Copyright 2008 Lin YANG <oxcsnicho@gmail.com>
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
/*
 * Unless otherwise indicated, Source Code is licensed under MIT license.
 * See further explanation attached in License Statement (distributed in the file
 * LICENSE).
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <string.h>

#include "mpegpsmux.h"
#include "mpegpsmux_aac.h"
#include "mpegpsmux_h264.h"

GST_DEBUG_CATEGORY (mpegpsmux_debug);
#define GST_CAT_DEFAULT mpegpsmux_debug

enum
{
  PROP_AGGREGATE_GOPS = 1
};

#define DEFAULT_AGGREGATE_GOPS FALSE

static GstStaticPadTemplate mpegpsmux_sink_factory =
    GST_STATIC_PAD_TEMPLATE ("sink_%u",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS ("video/mpeg, "
        "mpegversion = (int) { 1, 2, 4 }, "
        "systemstream = (boolean) false; "
        "video/x-dirac;"
        "video/x-h264,stream-format=(string)byte-stream,"
        "alignment=(string){au, nal}; "
        "audio/mpeg, "
        "mpegversion = (int) { 1, 2 };"
        "audio/mpeg, "
        "mpegversion = (int) 4, stream-format = (string) { raw, adts }; "
        "audio/x-lpcm, "
        "width = (int) { 16, 20, 24 }, "
        "rate = (int) { 48000, 96000 }, "
        "channels = (int) [ 1, 8 ], "
        "dynamic_range = (int) [ 0, 255 ], "
        "emphasis = (boolean) { FALSE, TRUE }, "
        "mute = (boolean) { FALSE, TRUE }"));

static GstStaticPadTemplate mpegpsmux_src_factory =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/mpeg, "
        "mpegversion = (int) 2, " "systemstream = (boolean) true")
    );

static void gst_mpegpsmux_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_mpegpsmux_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static void mpegpsmux_finalize (GObject * object);
static gboolean new_packet_cb (guint8 * data, guint len, void *user_data);

static gboolean mpegpsdemux_prepare_srcpad (MpegPsMux * mux);
static GstFlowReturn mpegpsmux_collected (GstCollectPads * pads,
    MpegPsMux * mux);
static GstPad *mpegpsmux_request_new_pad (GstElement * element,
    GstPadTemplate * templ, const gchar * name, const GstCaps * caps);
static void mpegpsmux_release_pad (GstElement * element, GstPad * pad);
static GstStateChangeReturn mpegpsmux_change_state (GstElement * element,
    GstStateChange transition);

#define parent_class mpegpsmux_parent_class
G_DEFINE_TYPE (MpegPsMux, mpegpsmux, GST_TYPE_ELEMENT);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (mpegpsmux, "mpegpsmux", GST_RANK_PRIMARY,
    mpegpsmux_get_type (), GST_DEBUG_CATEGORY_INIT (mpegpsmux_debug,
        "mpegpsmux", 0, "MPEG Program Stream muxer"));
static void
mpegpsmux_class_init (MpegPsMuxClass * klass)
{
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = gst_mpegpsmux_set_property;
  gobject_class->get_property = gst_mpegpsmux_get_property;
  gobject_class->finalize = mpegpsmux_finalize;

  gstelement_class->request_new_pad = mpegpsmux_request_new_pad;
  gstelement_class->release_pad = mpegpsmux_release_pad;
  gstelement_class->change_state = mpegpsmux_change_state;

  g_object_class_install_property (gobject_class, PROP_AGGREGATE_GOPS,
      g_param_spec_boolean ("aggregate-gops", "Aggregate GOPs",
          "Whether to aggregate GOPs and push them out as buffer lists",
          DEFAULT_AGGREGATE_GOPS, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_add_static_pad_template (gstelement_class,
      &mpegpsmux_sink_factory);
  gst_element_class_add_static_pad_template (gstelement_class,
      &mpegpsmux_src_factory);

  gst_element_class_set_static_metadata (gstelement_class,
      "MPEG Program Stream Muxer", "Codec/Muxer",
      "Multiplexes media streams into an MPEG Program Stream",
      "Lin YANG <oxcsnicho@gmail.com>");
}

static void
mpegpsmux_init (MpegPsMux * mux)
{
  mux->srcpad = gst_pad_new_from_static_template (&mpegpsmux_src_factory,
      "src");
  gst_pad_use_fixed_caps (mux->srcpad);
  gst_element_add_pad (GST_ELEMENT (mux), mux->srcpad);

  mux->collect = gst_collect_pads_new ();
  gst_collect_pads_set_function (mux->collect,
      (GstCollectPadsFunction) GST_DEBUG_FUNCPTR (mpegpsmux_collected), mux);

  mux->psmux = psmux_new ();
  psmux_set_write_func (mux->psmux, new_packet_cb, mux);

  mux->first = TRUE;
  mux->last_flow_ret = GST_FLOW_OK;
  mux->last_ts = 0;             /* XXX: or -1? */
}

static void
mpegpsmux_finalize (GObject * object)
{
  MpegPsMux *mux = GST_MPEG_PSMUX (object);

  if (mux->collect) {
    gst_object_unref (mux->collect);
    mux->collect = NULL;
  }
  if (mux->psmux) {
    psmux_free (mux->psmux);
    mux->psmux = NULL;
  }

  if (mux->gop_list != NULL) {
    gst_buffer_list_unref (mux->gop_list);
    mux->gop_list = NULL;
  }

  G_OBJECT_CLASS (mpegpsmux_parent_class)->finalize (object);
}

static void
gst_mpegpsmux_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  MpegPsMux *mux = GST_MPEG_PSMUX (object);

  switch (prop_id) {
    case PROP_AGGREGATE_GOPS:
      mux->aggregate_gops = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_mpegpsmux_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  MpegPsMux *mux = GST_MPEG_PSMUX (object);

  switch (prop_id) {
    case PROP_AGGREGATE_GOPS:
      g_value_set_boolean (value, mux->aggregate_gops);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstFlowReturn
mpegpsmux_create_stream (MpegPsMux * mux, MpegPsPadData * ps_data, GstPad * pad)
{
  /* Create a steam. Fill in codec specific information */

  GstFlowReturn ret = GST_FLOW_ERROR;
  GstCaps *caps;
  GstStructure *s;
  gboolean is_video = FALSE;

  caps = gst_pad_get_current_caps (pad);
  if (caps == NULL) {
    GST_DEBUG_OBJECT (pad, "Sink pad caps were not set before pushing");
    return GST_FLOW_NOT_NEGOTIATED;
  }

  s = gst_caps_get_structure (caps, 0);

  if (gst_structure_has_name (s, "video/x-dirac")) {
    GST_DEBUG_OBJECT (pad, "Creating Dirac stream");
    ps_data->stream = psmux_create_stream (mux->psmux, PSMUX_ST_VIDEO_DIRAC);
    is_video = TRUE;
  } else if (gst_structure_has_name (s, "audio/x-ac3")) {
    GST_DEBUG_OBJECT (pad, "Creating AC3 stream");
    ps_data->stream = psmux_create_stream (mux->psmux, PSMUX_ST_PS_AUDIO_AC3);
  } else if (gst_structure_has_name (s, "audio/x-dts")) {
    GST_DEBUG_OBJECT (pad, "Creating DTS stream");
    ps_data->stream = psmux_create_stream (mux->psmux, PSMUX_ST_PS_AUDIO_DTS);
  } else if (gst_structure_has_name (s, "audio/x-lpcm")) {
    GST_DEBUG_OBJECT (pad, "Creating LPCM stream");
    ps_data->stream = psmux_create_stream (mux->psmux, PSMUX_ST_PS_AUDIO_LPCM);
  } else if (gst_structure_has_name (s, "video/x-h264")) {
    const GValue *value;
    GST_DEBUG_OBJECT (pad, "Creating H264 stream");
    /* Codec data contains SPS/PPS which need to go in stream for valid ES */
    value = gst_structure_get_value (s, "codec_data");
    if (value) {
      ps_data->codec_data = gst_buffer_ref (gst_value_get_buffer (value));
      GST_DEBUG_OBJECT (pad, "%" G_GSIZE_FORMAT " bytes of codec data",
          gst_buffer_get_size (ps_data->codec_data));
      ps_data->prepare_func = mpegpsmux_prepare_h264;
    } else {
      ps_data->codec_data = NULL;
    }
    ps_data->stream = psmux_create_stream (mux->psmux, PSMUX_ST_VIDEO_H264);
    is_video = TRUE;
  } else if (gst_structure_has_name (s, "audio/mpeg")) {
    gint mpegversion;
    if (!gst_structure_get_int (s, "mpegversion", &mpegversion)) {
      GST_ELEMENT_ERROR (pad, STREAM, FORMAT,
          ("Invalid data format presented"),
          ("Caps with type audio/mpeg did not have mpegversion"));
      goto beach;
    }

    switch (mpegversion) {
      case 1:
        GST_DEBUG_OBJECT (pad, "Creating MPEG Audio, version 1 stream");
        ps_data->stream =
            psmux_create_stream (mux->psmux, PSMUX_ST_AUDIO_MPEG1);
        break;
      case 2:
        GST_DEBUG_OBJECT (pad, "Creating MPEG Audio, version 2 stream");
        ps_data->stream =
            psmux_create_stream (mux->psmux, PSMUX_ST_AUDIO_MPEG2);
        break;
      case 4:
      {
        const GValue *value;
        /* Codec data contains SPS/PPS which need to go in stream for valid ES */
        GST_DEBUG_OBJECT (pad, "Creating MPEG Audio, version 4 stream");
        value = gst_structure_get_value (s, "codec_data");
        if (value) {
          ps_data->codec_data = gst_buffer_ref (gst_value_get_buffer (value));
          GST_DEBUG_OBJECT (pad, "%" G_GSIZE_FORMAT " bytes of codec data",
              gst_buffer_get_size (ps_data->codec_data));
          ps_data->prepare_func = mpegpsmux_prepare_aac;
        } else {
          ps_data->codec_data = NULL;
        }
        ps_data->stream = psmux_create_stream (mux->psmux, PSMUX_ST_AUDIO_AAC);
        break;
      }
      default:
        GST_WARNING_OBJECT (pad, "unsupported mpegversion %d", mpegversion);
        goto beach;
    }
  } else if (gst_structure_has_name (s, "video/mpeg")) {
    gint mpegversion;
    if (!gst_structure_get_int (s, "mpegversion", &mpegversion)) {
      GST_ELEMENT_ERROR (mux, STREAM, FORMAT,
          ("Invalid data format presented"),
          ("Caps with type video/mpeg did not have mpegversion"));
      goto beach;
    }

    if (mpegversion == 1) {
      GST_DEBUG_OBJECT (pad, "Creating MPEG Video, version 1 stream");
      ps_data->stream = psmux_create_stream (mux->psmux, PSMUX_ST_VIDEO_MPEG1);
    } else if (mpegversion == 2) {
      GST_DEBUG_OBJECT (pad, "Creating MPEG Video, version 2 stream");
      ps_data->stream = psmux_create_stream (mux->psmux, PSMUX_ST_VIDEO_MPEG2);
    } else {
      GST_DEBUG_OBJECT (pad, "Creating MPEG Video, version 4 stream");
      ps_data->stream = psmux_create_stream (mux->psmux, PSMUX_ST_VIDEO_MPEG4);
    }
    is_video = TRUE;
  }

  if (ps_data->stream != NULL) {
    ps_data->stream_id = ps_data->stream->stream_id;
    ps_data->stream_id_ext = ps_data->stream->stream_id_ext;
    GST_DEBUG_OBJECT (pad, "Stream created, stream_id=%04x, stream_id_ext=%04x",
        ps_data->stream_id, ps_data->stream_id_ext);

    gst_structure_get_int (s, "rate", &ps_data->stream->audio_sampling);
    gst_structure_get_int (s, "channels", &ps_data->stream->audio_channels);
    gst_structure_get_int (s, "bitrate", &ps_data->stream->audio_bitrate);

    ret = GST_FLOW_OK;

    if (is_video && mux->video_stream_id == 0) {
      mux->video_stream_id = ps_data->stream_id;
      GST_INFO_OBJECT (mux, "video pad stream_id 0x%02x", mux->video_stream_id);
    }
  }

beach:
  gst_caps_unref (caps);
  return ret;
}

static GstFlowReturn
mpegpsmux_create_streams (MpegPsMux * mux)
{
  /* Create stream for each pad */

  GstFlowReturn ret = GST_FLOW_OK;
  GSList *walk = mux->collect->data;

  /* Create the streams */
  while (walk) {
    GstCollectData *c_data = (GstCollectData *) walk->data;
    MpegPsPadData *ps_data = (MpegPsPadData *) walk->data;

    walk = g_slist_next (walk);

    if (ps_data->stream == NULL) {
      ret = mpegpsmux_create_stream (mux, ps_data, c_data->pad);
      if (ret != GST_FLOW_OK)
        goto no_stream;
    }
  }

  return GST_FLOW_OK;
no_stream:
  GST_ELEMENT_ERROR (mux, STREAM, MUX,
      ("Could not create handler for stream"), (NULL));
  return ret;
}

static GstBuffer *
mpegpsmux_queue_buffer_for_stream (MpegPsMux * mux, MpegPsPadData * ps_data)
{
  GstCollectData *c_data = (GstCollectData *) ps_data;
  GstBuffer *buf;

  g_assert (ps_data->queued.buf == NULL);

  buf = gst_collect_pads_peek (mux->collect, c_data);
  if (buf == NULL)
    return NULL;

  ps_data->queued.buf = buf;

  /* do any raw -> byte-stream format conversions (e.g. for H.264, AAC) */
  if (ps_data->prepare_func) {
    buf = ps_data->prepare_func (buf, ps_data, mux);
    if (buf) {                  /* Take the prepared buffer instead */
      gst_buffer_unref (ps_data->queued.buf);
      ps_data->queued.buf = buf;
    } else {                    /* If data preparation returned NULL, use unprepared one */
      buf = ps_data->queued.buf;
    }
  }

  ps_data->queued.pts = GST_BUFFER_PTS (buf);
  if (GST_CLOCK_TIME_IS_VALID (ps_data->queued.pts)) {
    ps_data->queued.pts = gst_segment_to_running_time (&c_data->segment,
        GST_FORMAT_TIME, ps_data->queued.pts);
  }

  ps_data->queued.dts = GST_BUFFER_DTS (buf);
  if (GST_CLOCK_TIME_IS_VALID (ps_data->queued.dts)) {
    ps_data->queued.dts = gst_segment_to_running_time (&c_data->segment,
        GST_FORMAT_TIME, ps_data->queued.dts);
  }

  if (GST_BUFFER_PTS_IS_VALID (buf) && GST_BUFFER_DTS_IS_VALID (buf)) {
    ps_data->queued.ts = MIN (ps_data->queued.dts, ps_data->queued.pts);
  } else if (GST_BUFFER_PTS_IS_VALID (buf) && !GST_BUFFER_DTS_IS_VALID (buf)) {
    ps_data->queued.ts = ps_data->queued.pts;
  } else if (GST_BUFFER_DTS_IS_VALID (buf) && !GST_BUFFER_PTS_IS_VALID (buf)) {
    GST_WARNING_OBJECT (c_data->pad, "got DTS without PTS");
    ps_data->queued.ts = ps_data->queued.dts;
  } else {
    ps_data->queued.ts = GST_CLOCK_TIME_NONE;
  }

  if (ps_data->queued.ts != GST_CLOCK_TIME_NONE)
    ps_data->last_ts = ps_data->queued.ts;

  GST_DEBUG_OBJECT (mux, "Queued buffer with ts %" GST_TIME_FORMAT ": "
      "uncorrected pts %" GST_TIME_FORMAT " dts %" GST_TIME_FORMAT ", "
      "buffer pts %" GST_TIME_FORMAT " dts %" GST_TIME_FORMAT " for PID 0x%04x",
      GST_TIME_ARGS (ps_data->queued.ts),
      GST_TIME_ARGS (GST_BUFFER_PTS (buf)),
      GST_TIME_ARGS (GST_BUFFER_DTS (buf)),
      GST_TIME_ARGS (ps_data->queued.pts),
      GST_TIME_ARGS (ps_data->queued.dts), ps_data->stream_id);

  return buf;
}

static MpegPsPadData *
mpegpsmux_choose_best_stream (MpegPsMux * mux)
{
  /* Choose from which stream to mux with */

  MpegPsPadData *best = NULL;
  GstCollectData *c_best = NULL;
  GSList *walk;

  for (walk = mux->collect->data; walk != NULL; walk = g_slist_next (walk)) {
    GstCollectData *c_data = (GstCollectData *) walk->data;
    MpegPsPadData *ps_data = (MpegPsPadData *) walk->data;

    if (ps_data->eos == FALSE) {
      if (ps_data->queued.buf == NULL) {
        GstBuffer *buf;

        buf = mpegpsmux_queue_buffer_for_stream (mux, ps_data);
        if (buf == NULL) {
          GST_DEBUG_OBJECT (mux, "we have EOS");
          ps_data->eos = TRUE;
          continue;
        }
      }

      /* If we don't yet have a best pad, take this one, otherwise take
       * whichever has the oldest timestamp */
      if (best != NULL) {
        if (ps_data->last_ts == GST_CLOCK_TIME_NONE ||
            (best->last_ts != GST_CLOCK_TIME_NONE &&
                ps_data->last_ts < best->last_ts)) {
          best = ps_data;
          c_best = c_data;
        }
      } else {
        best = ps_data;
        c_best = c_data;
      }
    }
  }
  if (c_best) {
    gst_buffer_unref (gst_collect_pads_pop (mux->collect, c_best));
  }

  return best;
}

static GstFlowReturn
mpegpsmux_push_gop_list (MpegPsMux * mux)
{
  GstFlowReturn flow;

  g_assert (mux->gop_list != NULL);

  GST_DEBUG_OBJECT (mux, "Sending pending GOP of %u buffers",
      gst_buffer_list_length (mux->gop_list));
  flow = gst_pad_push_list (mux->srcpad, mux->gop_list);
  mux->gop_list = NULL;
  return flow;
}

static GstFlowReturn
mpegpsmux_collected (GstCollectPads * pads, MpegPsMux * mux)
{
  /* main muxing function */

  GstFlowReturn ret = GST_FLOW_OK;
  MpegPsPadData *best = NULL;
  gboolean keyunit;

  GST_DEBUG_OBJECT (mux, "Pads collected");

  if (mux->first) {             /* process block for the first mux */
    /* iterate through the collect pads and add streams to @mux */
    ret = mpegpsmux_create_streams (mux);
    /* Assumption : all pads are already added at this time */

    if (G_UNLIKELY (ret != GST_FLOW_OK))
      return ret;

    best = mpegpsmux_choose_best_stream (mux);

    /* prepare the src pad (output), return if failed */
    if (!mpegpsdemux_prepare_srcpad (mux)) {
      GST_DEBUG_OBJECT (mux, "Failed to send new segment");
      goto new_seg_fail;
    }

    mux->first = FALSE;
  } else {
    best = mpegpsmux_choose_best_stream (mux);
  }

  if (best != NULL) {
    GstBuffer *buf = best->queued.buf;
    gint64 pts, dts;

    g_assert (buf != NULL);

    GST_LOG_OBJECT (mux,
        "Chose stream from pad %" GST_PTR_FORMAT " for output (PID: 0x%04x): "
        "adjusted pts: %" GST_TIME_FORMAT ", dts: %" GST_TIME_FORMAT,
        best->collect.pad, best->stream_id,
        GST_TIME_ARGS (best->queued.pts), GST_TIME_ARGS (best->queued.dts));

    /* and convert to mpeg time stamps */
    pts = GSTTIME_TO_MPEGTIME (best->queued.pts);
    dts = GSTTIME_TO_MPEGTIME (best->queued.dts);

    /* start of new GOP? */
    keyunit = !GST_BUFFER_FLAG_IS_SET (buf, GST_BUFFER_FLAG_DELTA_UNIT);

    if (keyunit && best->stream_id == mux->video_stream_id
        && mux->gop_list != NULL) {
      ret = mpegpsmux_push_gop_list (mux);
      if (ret != GST_FLOW_OK)
        goto done;
    }

    /* give the buffer to libpsmux for processing */
    psmux_stream_add_data (best->stream, buf, pts, dts, keyunit);

    best->queued.buf = NULL;

    /* write the data from libpsmux to stream */
    while (psmux_stream_bytes_in_buffer (best->stream) > 0) {
      GST_LOG_OBJECT (mux, "Before @psmux_write_stream_packet");
      if (!psmux_write_stream_packet (mux->psmux, best->stream)) {
        GST_DEBUG_OBJECT (mux, "Failed to write data packet");
        goto write_fail;
      }
    }
    mux->last_ts = best->last_ts;
  } else {
    /* FIXME: Drain all remaining streams */
    /* At EOS */
    if (mux->gop_list != NULL)
      mpegpsmux_push_gop_list (mux);

    if (!psmux_write_end_code (mux->psmux)) {
      GST_WARNING_OBJECT (mux, "Writing MPEG PS Program end code failed.");
    }
    gst_pad_push_event (mux->srcpad, gst_event_new_eos ());

    ret = GST_FLOW_EOS;
  }

done:

  return ret;
new_seg_fail:
  return GST_FLOW_ERROR;
write_fail:
  /* FIXME: Failed writing data for some reason. Should set appropriate error */
  return mux->last_flow_ret;
}

static GstPad *
mpegpsmux_request_new_pad (GstElement * element,
    GstPadTemplate * templ, const gchar * name, const GstCaps * caps)
{
  MpegPsMux *mux = GST_MPEG_PSMUX (element);
  GstPad *pad = NULL;
  MpegPsPadData *pad_data = NULL;

  pad = gst_pad_new_from_template (templ, name);

  pad_data = (MpegPsPadData *) gst_collect_pads_add_pad (mux->collect, pad,
      sizeof (MpegPsPadData), NULL, TRUE);
  if (pad_data == NULL)
    goto pad_failure;

  pad_data->last_ts = GST_CLOCK_TIME_NONE;
  pad_data->codec_data = NULL;
  pad_data->prepare_func = NULL;

  if (G_UNLIKELY (!gst_element_add_pad (element, pad)))
    goto could_not_add;

  return pad;

could_not_add:
  GST_ELEMENT_ERROR (element, STREAM, FAILED,
      ("Internal data stream error."), ("Could not add pad to element"));
  gst_collect_pads_remove_pad (mux->collect, pad);
  gst_object_unref (pad);
  return NULL;
pad_failure:
  GST_ELEMENT_ERROR (element, STREAM, FAILED,
      ("Internal data stream error."), ("Could not add pad to collectpads"));
  gst_object_unref (pad);
  return NULL;
}

static void
mpegpsmux_release_pad (GstElement * element, GstPad * pad)
{
  /* unref pad data (and codec data) */

  MpegPsMux *mux = GST_MPEG_PSMUX (element);
  MpegPsPadData *pad_data = NULL;

  GST_DEBUG_OBJECT (mux, "Pad %" GST_PTR_FORMAT " being released", pad);

  /* Get the MpegPsPadData out of the pad */
  GST_OBJECT_LOCK (pad);
  pad_data = (MpegPsPadData *) gst_pad_get_element_private (pad);
  if (G_LIKELY (pad_data)) {
    /* Free codec data reference if any */
    if (pad_data->codec_data) {
      GST_DEBUG_OBJECT (element, "releasing codec_data reference");
      gst_buffer_unref (pad_data->codec_data);
      pad_data->codec_data = NULL;
    }
    if (pad_data->stream_id == mux->video_stream_id)
      mux->video_stream_id = 0;
  }
  GST_OBJECT_UNLOCK (pad);

  gst_collect_pads_remove_pad (mux->collect, pad);
}

static gboolean
new_packet_cb (guint8 * data, guint len, void *user_data)
{
  /* Called when the PsMux has prepared a packet for output. Return FALSE
   * on error */

  MpegPsMux *mux = (MpegPsMux *) user_data;
  GstBuffer *buf;
  GstFlowReturn ret;

  GST_LOG_OBJECT (mux, "Outputting a packet of length %d", len);

  data = g_memdup2 (data, len);
  buf = gst_buffer_new_wrapped (data, len);

  GST_BUFFER_TIMESTAMP (buf) = mux->last_ts;

  if (mux->aggregate_gops) {
    if (mux->gop_list == NULL)
      mux->gop_list = gst_buffer_list_new ();

    gst_buffer_list_add (mux->gop_list, buf);
    return TRUE;
  }

  ret = gst_pad_push (mux->srcpad, buf);

  if (G_UNLIKELY (ret != GST_FLOW_OK)) {
    mux->last_flow_ret = ret;
    return FALSE;
  }

  return TRUE;
}

/* prepare the source pad for output */
static gboolean
mpegpsdemux_prepare_srcpad (MpegPsMux * mux)
{
  GstSegment segment;
  GValue val = { 0, };
  GList *headers, *l;
  GstCaps *caps;
  gchar s_id[32];

  /* stream-start (FIXME: create id based on input ids) */
  g_snprintf (s_id, sizeof (s_id), "mpegpsmux-%08x", g_random_int ());
  gst_pad_push_event (mux->srcpad, gst_event_new_stream_start (s_id));

  caps = gst_caps_new_simple ("video/mpeg",
      "mpegversion", G_TYPE_INT, 2, "systemstream", G_TYPE_BOOLEAN, TRUE, NULL);

  headers = psmux_get_stream_headers (mux->psmux);
  g_value_init (&val, GST_TYPE_ARRAY);
  for (l = headers; l != NULL; l = l->next) {
    GValue buf_val = { 0, };

    g_value_init (&buf_val, GST_TYPE_BUFFER);
    gst_value_take_buffer (&buf_val, GST_BUFFER (l->data));
    l->data = NULL;
    gst_value_array_append_value (&val, &buf_val);
    g_value_unset (&buf_val);
  }
  gst_caps_set_value (caps, "streamheader", &val);
  g_value_unset (&val);
  g_list_free (headers);

  /* Set caps on src pad and push new segment */
  gst_pad_push_event (mux->srcpad, gst_event_new_caps (caps));
  gst_caps_unref (caps);

  gst_segment_init (&segment, GST_FORMAT_BYTES);
  gst_pad_push_event (mux->srcpad, gst_event_new_segment (&segment));

  return TRUE;
}

static GstStateChangeReturn
mpegpsmux_change_state (GstElement * element, GstStateChange transition)
{
  /* control the collect pads */

  MpegPsMux *mux = GST_MPEG_PSMUX (element);
  GstStateChangeReturn ret;

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      gst_collect_pads_start (mux->collect);
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      gst_collect_pads_stop (mux->collect);
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    default:
      break;
  }

  return ret;
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  return GST_ELEMENT_REGISTER (mpegpsmux, plugin);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR, GST_VERSION_MINOR,
    mpegpsmux, "MPEG-PS muxer",
    plugin_init, VERSION, "LGPL", GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN);
