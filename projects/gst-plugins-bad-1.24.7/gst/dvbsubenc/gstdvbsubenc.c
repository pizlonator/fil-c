/* GStreamer
 * Copyright (C) <2020> Jan Schmidt <jan@centricular.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstdvbsubenc.h"
#include <string.h>

/**
 * SECTION:element-dvbsubenc
 * @title: dvbsubenc
 * @see_also: dvbsuboverlay
 *
 * This element encodes AYUV video frames to DVB subpictures.
 *
 * ## Example pipelines
 * |[
 * gst-launch-1.0 videotestsrc num-buffers=900 ! video/x-raw,width=720,height=576,framerate=30/1 ! x264enc bitrate=500 ! h264parse ! mpegtsmux name=mux ! filesink location=test.ts  filesrc location=test-subtitles.srt ! subparse ! textrender ! dvbsubenc ! mux.
 * ]|
 * Encode a test video signal and an SRT subtitle file to MPEG-TS with a DVB subpicture track
 *
 */

#define DEFAULT_MAX_COLOURS 16
#define DEFAULT_TS_OFFSET 0

enum
{
  PROP_0,
  PROP_MAX_COLOURS,
  PROP_TS_OFFSET
};

#define gst_dvb_sub_enc_parent_class parent_class
G_DEFINE_TYPE (GstDvbSubEnc, gst_dvb_sub_enc, GST_TYPE_ELEMENT);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (dvbsubenc, "dvbsubenc", GST_RANK_NONE,
    GST_TYPE_DVB_SUB_ENC, GST_DEBUG_CATEGORY_INIT (gst_dvb_sub_enc_debug,
        "dvbsubenc", 0, "DVB subtitle encoder");
    );

static void gst_dvb_sub_enc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_dvb_sub_enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);

static gboolean gst_dvb_sub_enc_src_event (GstPad * srcpad, GstObject * parent,
    GstEvent * event);
static GstFlowReturn gst_dvb_sub_enc_chain (GstPad * pad, GstObject * parent,
    GstBuffer * buf);

static void gst_dvb_sub_enc_finalize (GObject * gobject);
static gboolean gst_dvb_sub_enc_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event);
static gboolean gst_dvb_sub_enc_sink_setcaps (GstPad * pad, GstCaps * caps);

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw, format = (string) { AYUV }")
    );

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("subpicture/x-dvb")
    );

GST_DEBUG_CATEGORY (gst_dvb_sub_enc_debug);

static void
gst_dvb_sub_enc_class_init (GstDvbSubEncClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->finalize = gst_dvb_sub_enc_finalize;

  gst_element_class_add_static_pad_template (gstelement_class, &sink_template);
  gst_element_class_add_static_pad_template (gstelement_class, &src_template);

  gst_element_class_set_static_metadata (gstelement_class,
      "DVB subtitle encoder", "Codec/Decoder/Video",
      "Encodes AYUV video frames streams into DVB subtitles",
      "Jan Schmidt <jan@centricular.com>");

  gobject_class->set_property = gst_dvb_sub_enc_set_property;
  gobject_class->get_property = gst_dvb_sub_enc_get_property;

 /**
  * GstDvbSubEnc:max-colours
  *
  * Set the maximum number of colours to output into the DVB subpictures.
  * Good choices are 4, 16 or 256 - as they correspond to the 2-bit, 4-bit
  * and 8-bit palette modes that the DVB subpicture encoding supports.
  */
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_MAX_COLOURS,
      g_param_spec_int ("max-colours", "Maximum Colours",
          "Maximum Number of Colours to output", 1, 256, DEFAULT_MAX_COLOURS,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

 /**
  * GstDvbSubEnc:ts-offset
  *
  * Advance or delay the output subpicture time-line. This is a
  * convenience property for setting the src pad offset.
  */
  g_object_class_install_property (gobject_class, PROP_TS_OFFSET,
      g_param_spec_int64 ("ts-offset", "Subtitle Timestamp Offset",
          "Apply an offset to incoming timestamps before output (in nanoseconds)",
          G_MININT64, G_MAXINT64, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

}

static void
gst_dvb_sub_enc_init (GstDvbSubEnc * enc)
{
  GstPadTemplate *tmpl;

  enc->sinkpad = gst_pad_new_from_static_template (&sink_template, "sink");
  gst_pad_set_chain_function (enc->sinkpad,
      GST_DEBUG_FUNCPTR (gst_dvb_sub_enc_chain));
  gst_pad_set_event_function (enc->sinkpad,
      GST_DEBUG_FUNCPTR (gst_dvb_sub_enc_sink_event));
  gst_element_add_pad (GST_ELEMENT (enc), enc->sinkpad);

  tmpl = gst_static_pad_template_get (&src_template);
  enc->srcpad = gst_pad_new_from_template (tmpl, "src");
  gst_pad_set_event_function (enc->srcpad,
      GST_DEBUG_FUNCPTR (gst_dvb_sub_enc_src_event));
  gst_pad_use_fixed_caps (enc->srcpad);
  gst_object_unref (tmpl);
  gst_element_add_pad (GST_ELEMENT (enc), enc->srcpad);

  enc->max_colours = DEFAULT_MAX_COLOURS;
  enc->ts_offset = DEFAULT_TS_OFFSET;

  enc->current_end_time = GST_CLOCK_TIME_NONE;
}

static void
gst_dvb_sub_enc_finalize (GObject * gobject)
{
  //GstDvbSubEnc *enc = GST_DVB_SUB_ENC (gobject);

  G_OBJECT_CLASS (parent_class)->finalize (gobject);
}

static void
gst_dvb_sub_enc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstDvbSubEnc *enc = GST_DVB_SUB_ENC (object);

  switch (prop_id) {
    case PROP_MAX_COLOURS:
      g_value_set_int (value, enc->max_colours);
      break;
    case PROP_TS_OFFSET:
      g_value_set_int64 (value, enc->ts_offset);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_dvb_sub_enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstDvbSubEnc *enc = GST_DVB_SUB_ENC (object);

  switch (prop_id) {
    case PROP_MAX_COLOURS:
      enc->max_colours = g_value_get_int (value);
      break;
    case PROP_TS_OFFSET:
      enc->ts_offset = g_value_get_int64 (value);
      gst_pad_set_offset (enc->srcpad, enc->ts_offset);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_dvb_sub_enc_src_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  gboolean res = FALSE;

  switch (GST_EVENT_TYPE (event)) {
    default:
      res = gst_pad_event_default (pad, parent, event);
      break;
  }

  return res;
}

static void
find_largest_subregion (guint8 * pixels, guint stride, guint pixel_stride,
    gint width, gint height, guint * out_left, guint * out_right,
    guint * out_top, guint * out_bottom)
{
  guint left = width, right = 0, top = height, bottom = 0;
  gint y, x;
  guint8 *p = pixels;

  for (y = 0; y < height; y++) {
    gboolean visible_pixels = FALSE;
    guint8 *l = p;
    guint8 *r = p + (width - 1) * pixel_stride;

    for (x = 0; x < width; x++) {
      /* AYUV data = byte 0 = A */
      if (l[0] != 0) {
        visible_pixels = TRUE;
        left = MIN (left, x);
      }
      if (r[0] != 0) {
        visible_pixels = TRUE;
        right = MAX (right, width - 1 - x);
      }

      l += pixel_stride;
      r -= pixel_stride;

      if (l >= r)               /* Stop when we've scanned to the middle */
        break;
    }

    if (visible_pixels) {
      if (top > y)
        top = y;
      if (bottom < y)
        bottom = y;
    }
    p += stride;
  }

  *out_left = left;
  *out_right = right;
  *out_top = top;
  *out_bottom = bottom;
}

/* Create and map a new buffer containing the indicated subregion of the input
 * image, returning the result in the 'out' GstVideoFrame */
static gboolean
create_cropped_frame (GstDvbSubEnc * enc, GstVideoFrame * in,
    GstVideoFrame * out, guint x, guint y, guint width, guint height)
{
  GstBuffer *cropped_buffer;
  GstVideoInfo cropped_info;
  guint8 *out_pixels, *in_pixels;
  guint out_stride, in_stride, p_stride;
  guint bottom = y + height;

  g_return_val_if_fail (GST_VIDEO_INFO_FORMAT (&in->info) ==
      GST_VIDEO_FORMAT_AYUV, FALSE);

  gst_video_info_set_format (&cropped_info, GST_VIDEO_INFO_FORMAT (&in->info),
      width, height);
  cropped_buffer =
      gst_buffer_new_allocate (NULL, GST_VIDEO_INFO_SIZE (&cropped_info), NULL);

  if (!gst_video_frame_map (out, &cropped_info, cropped_buffer, GST_MAP_WRITE)) {
    gst_buffer_unref (cropped_buffer);
    return FALSE;
  }

  p_stride = GST_VIDEO_FRAME_COMP_PSTRIDE (in, 0);
  in_stride = GST_VIDEO_FRAME_PLANE_STRIDE (in, 0);
  in_pixels = GST_VIDEO_FRAME_PLANE_DATA (in, 0);

  out_stride = GST_VIDEO_FRAME_PLANE_STRIDE (out, 0);
  out_pixels = GST_VIDEO_FRAME_PLANE_DATA (out, 0);

  in_pixels += y * in_stride + x * p_stride;

  while (y < bottom) {
    memcpy (out_pixels, in_pixels, width * p_stride);

    in_pixels += in_stride;
    out_pixels += out_stride;
    y++;
  }

  /* Remap the video frame as read-only, and then drop our ref to the
   * underlying buffer so the video frame holds the only remaining ref */
  gst_video_frame_unmap (out);
  if (!gst_video_frame_map (out, &cropped_info, cropped_buffer, GST_MAP_READ)) {
    gst_buffer_unref (cropped_buffer);
    return FALSE;
  }
  gst_buffer_unref (cropped_buffer);
  return TRUE;
}

static GstFlowReturn
process_largest_subregion (GstDvbSubEnc * enc, GstVideoFrame * vframe)
{
  GstFlowReturn ret = GST_FLOW_ERROR;

  guint8 *pixels = GST_VIDEO_FRAME_PLANE_DATA (vframe, 0);
  guint stride = GST_VIDEO_FRAME_PLANE_STRIDE (vframe, 0);
  guint pixel_stride = GST_VIDEO_FRAME_COMP_PSTRIDE (vframe, 0);
  guint left, right, top, bottom;
  GstBuffer *ayuv8p_buffer;
  GstVideoInfo ayuv8p_info;
  GstVideoFrame cropped_frame, ayuv8p_frame;
  guint32 num_colours;
  GstClockTime end_ts = GST_CLOCK_TIME_NONE, duration;

  find_largest_subregion (pixels, stride, pixel_stride, enc->in_info.width,
      enc->in_info.height, &left, &right, &top, &bottom);

  /* Don't crash if subtitle is empty */
  if (right < left || bottom < top)
    goto skip;

  GST_LOG_OBJECT (enc, "Found subregion %u,%u -> %u,%u w %u, %u", left, top,
      right, bottom, right - left + 1, bottom - top + 1);

  if (!create_cropped_frame (enc, vframe, &cropped_frame, left, top,
          right - left + 1, bottom - top + 1)) {
    GST_WARNING_OBJECT (enc, "Failed to map frame conversion input buffer");
    goto fail;
  }

  /* FIXME: RGB8P is the same size as what we're building, so this is fine,
   * but it'd be better if we had an explicit paletted format for YUV8P */
  gst_video_info_set_format (&ayuv8p_info, GST_VIDEO_FORMAT_RGB8P,
      right - left + 1, bottom - top + 1);
  ayuv8p_buffer =
      gst_buffer_new_allocate (NULL, GST_VIDEO_INFO_SIZE (&ayuv8p_info), NULL);

  /* Pass buffer ref to the frame, and remove the ref that we have - now frame owns the only ref */
  if (!gst_video_frame_map (&ayuv8p_frame, &ayuv8p_info, ayuv8p_buffer,
          GST_MAP_WRITE)) {
    GST_WARNING_OBJECT (enc, "Failed to map frame conversion output buffer");
    gst_video_frame_unmap (&cropped_frame);
    gst_buffer_unref (ayuv8p_buffer);
    goto fail;
  }
  gst_buffer_unref (ayuv8p_buffer);

  if (!gst_dvbsubenc_ayuv_to_ayuv8p (&cropped_frame, &ayuv8p_frame,
          enc->max_colours, &num_colours)) {
    GST_ERROR_OBJECT (enc,
        "Failed to convert subpicture region to paletted 8-bit");
    gst_video_frame_unmap (&cropped_frame);
    gst_video_frame_unmap (&ayuv8p_frame);
    goto skip;
  }

  gst_video_frame_unmap (&cropped_frame);

  duration = GST_BUFFER_DURATION (vframe->buffer);

  if (GST_CLOCK_TIME_IS_VALID (duration)) {
    end_ts = GST_BUFFER_PTS (vframe->buffer);
    if (GST_CLOCK_TIME_IS_VALID (end_ts)) {
      end_ts += duration;
    }
  }

  /* Encode output buffer and push it */
  {
    SubpictureRect s;
    GstBuffer *packet;

    s.frame = &ayuv8p_frame;
    s.nb_colours = num_colours;
    s.x = left;
    s.y = top;

    packet =
        gst_dvbenc_encode (enc->object_version & 0xF, 1, enc->display_version,
        enc->in_info.width, enc->in_info.height, &s, 1);
    if (packet == NULL) {
      gst_video_frame_unmap (&ayuv8p_frame);
      goto fail;
    }

    enc->object_version++;

    gst_buffer_copy_into (packet, vframe->buffer, GST_BUFFER_COPY_METADATA, 0,
        -1);

    if (!GST_BUFFER_DTS_IS_VALID (packet))
      GST_BUFFER_DTS (packet) = GST_BUFFER_PTS (packet);

    ret = gst_pad_push (enc->srcpad, packet);
  }

  if (GST_CLOCK_TIME_IS_VALID (end_ts)) {
    GST_LOG_OBJECT (enc, "Scheduling subtitle end packet for %" GST_TIME_FORMAT,
        GST_TIME_ARGS (end_ts));
    enc->current_end_time = end_ts;
  }

  gst_video_frame_unmap (&ayuv8p_frame);

  return ret;
skip:
  return GST_FLOW_OK;
fail:
  return GST_FLOW_ERROR;
}

static GstFlowReturn
gst_dvb_sub_enc_generate_end_packet (GstDvbSubEnc * enc, GstClockTime pts)
{
  GstBuffer *packet;
  GstFlowReturn ret;

  if (!GST_CLOCK_TIME_IS_VALID (enc->current_end_time))
    return GST_FLOW_OK;

  if (enc->current_end_time >= pts)
    return GST_FLOW_OK;         /* Didn't hit the end of the current subtitle yet */

  GST_DEBUG_OBJECT (enc, "Outputting end of page at TS %" GST_TIME_FORMAT,
      GST_TIME_ARGS (enc->current_end_time));

  packet =
      gst_dvbenc_encode (enc->object_version & 0xF, 1, enc->display_version,
      enc->in_info.width, enc->in_info.height, NULL, 0);
  if (packet == NULL) {
    GST_ELEMENT_ERROR (enc, STREAM, FAILED,
        ("Internal data stream error."),
        ("Failed to encode end of subtitle packet"));
    return GST_FLOW_ERROR;
  }

  enc->object_version++;

  GST_BUFFER_DTS (packet) = GST_BUFFER_PTS (packet) = enc->current_end_time;
  enc->current_end_time = GST_CLOCK_TIME_NONE;

  ret = gst_pad_push (enc->srcpad, packet);

  return ret;
}

static GstFlowReturn
gst_dvb_sub_enc_chain (GstPad * pad, GstObject * parent, GstBuffer * buf)
{
  GstFlowReturn ret = GST_FLOW_OK;
  GstDvbSubEnc *enc = GST_DVB_SUB_ENC (parent);
  GstVideoFrame vframe;
  GstClockTime pts = GST_BUFFER_PTS (buf);

  GST_DEBUG_OBJECT (enc, "Have buffer of size %" G_GSIZE_FORMAT ", ts %"
      GST_TIME_FORMAT ", dur %" G_GINT64_FORMAT, gst_buffer_get_size (buf),
      GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buf)), GST_BUFFER_DURATION (buf));

  if (GST_CLOCK_TIME_IS_VALID (pts)) {
    ret = gst_dvb_sub_enc_generate_end_packet (enc, pts);
    if (ret != GST_FLOW_OK)
      goto fail;
  }

  /* FIXME: Allow GstVideoOverlayComposition input, so we can directly encode the
   * overlays passed */

  /* Scan the input buffer for regions to encode */
  /* FIXME: Could use the blob extents tracking code from OpenHMD here to collect
   * multiple regions*/
  if (!gst_video_frame_map (&vframe, &enc->in_info, buf, GST_MAP_READ)) {
    GST_ERROR_OBJECT (enc, "Failed to map input buffer for reading");
    ret = GST_FLOW_ERROR;
    goto fail;
  }

  ret = process_largest_subregion (enc, &vframe);
  gst_video_frame_unmap (&vframe);

fail:
  gst_buffer_unref (buf);
  return ret;
}

static gboolean
gst_dvb_sub_enc_sink_setcaps (GstPad * pad, GstCaps * caps)
{
  GstDvbSubEnc *enc = GST_DVB_SUB_ENC (gst_pad_get_parent (pad));
  gboolean ret = FALSE;
  GstVideoInfo in_info;
  GstCaps *out_caps = NULL;

  GST_DEBUG_OBJECT (enc, "setcaps called with %" GST_PTR_FORMAT, caps);
  if (!gst_video_info_from_caps (&in_info, caps)) {
    GST_ERROR_OBJECT (enc, "Failed to parse input caps");
    return FALSE;
  }

  if (!enc->in_info.finfo || !gst_video_info_is_equal (&in_info, &enc->in_info)) {
    enc->in_info = in_info;
    enc->display_version++;

    out_caps = gst_caps_new_simple ("subpicture/x-dvb",
        "width", G_TYPE_INT, enc->in_info.width,
        "height", G_TYPE_INT, enc->in_info.height,
        "framerate", GST_TYPE_FRACTION, enc->in_info.fps_n, enc->in_info.fps_d,
        NULL);

    if (!gst_pad_set_caps (enc->srcpad, out_caps)) {
      GST_WARNING_OBJECT (enc, "failed setting downstream caps");
      gst_caps_unref (out_caps);
      goto beach;
    }

    gst_caps_unref (out_caps);
  }

  ret = TRUE;

beach:
  gst_object_unref (enc);
  return ret;
}

static gboolean
gst_dvb_sub_enc_sink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstDvbSubEnc *enc = GST_DVB_SUB_ENC (parent);
  gboolean ret = FALSE;

  GST_LOG_OBJECT (enc, "%s event", GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:
    {
      GstCaps *caps;

      gst_event_parse_caps (event, &caps);
      ret = gst_dvb_sub_enc_sink_setcaps (pad, caps);
      gst_event_unref (event);
      break;
    }
    case GST_EVENT_GAP:
    {
      if (!GST_CLOCK_TIME_IS_VALID (enc->current_end_time)) {
        ret = gst_pad_event_default (pad, parent, event);
      } else {
        GstClockTime start, duration;

        gst_event_parse_gap (event, &start, &duration);

        if (GST_CLOCK_TIME_IS_VALID (start)) {
          if (GST_CLOCK_TIME_IS_VALID (duration))
            start += duration;
          /* we do not expect another buffer until after gap,
           * so that is our position now */
          GST_DEBUG_OBJECT (enc,
              "Got GAP event, advancing time to %" GST_TIME_FORMAT,
              GST_TIME_ARGS (start));
          gst_dvb_sub_enc_generate_end_packet (enc, start);
        } else {
          GST_WARNING_OBJECT (enc, "Got GAP event with invalid position");
        }

        gst_event_unref (event);
        ret = TRUE;
      }
      break;
    }
    case GST_EVENT_SEGMENT:
    {
      GstSegment seg;

      gst_event_copy_segment (event, &seg);

      ret = gst_pad_event_default (pad, parent, event);
      break;
    }
    case GST_EVENT_FLUSH_STOP:{
      enc->current_end_time = GST_CLOCK_TIME_NONE;

      ret = gst_pad_event_default (pad, parent, event);
      break;
    }
    default:{
      ret = gst_pad_event_default (pad, parent, event);
      break;
    }
  }
  return ret;
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  return GST_ELEMENT_REGISTER (dvbsubenc, plugin);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    dvbsubenc,
    "DVB subtitle parser and encoder", plugin_init,
    VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN);
