/* GStreamer
 * Copyright (C) <2004> Thomas Vander Stichele <thomas at apestaart dot org>
 * Copyright (C) 2006 Andy Wingo <wingo@pobox.com>
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
 * SECTION:element-theoraparse
 * @title: theoraparse
 * @see_also: theoradec, oggdemux, vorbisparse
 *
 * The theoraparse element will parse the header packets of the Theora
 * stream and put them as the streamheader in the caps. This is used in the
 * multifdsink case where you want to stream live theora streams to multiple
 * clients, each client has to receive the streamheaders first before they can
 * consume the theora packets.
 *
 * This element also makes sure that the buffers that it pushes out are properly
 * timestamped and that their offset and offset_end are set. The buffers that
 * theoraparse outputs have all of the metadata that oggmux expects to receive,
 * which allows you to (for example) remux an ogg/theora file.
 *
 * In addition, this element allows you to fix badly synchronized streams. You
 * pass in an array of (granule time, buffer time) synchronization points via
 * the synchronization-points GValueArray property, and this element will adjust
 * the granulepos values that it outputs. The adjustment will be made by
 * offsetting all buffers that it outputs by a specified amount, and updating
 * that offset from the value array whenever a keyframe is processed.
 *
 * ## Example pipelines
 * |[
 * gst-launch-1.0 -v filesrc location=video.ogg ! oggdemux ! theoraparse ! fakesink
 * ]|
 *  This pipeline shows that the streamheader is set in the caps, and that each
 * buffer has the timestamp, duration, offset, and offset_end set.
 * |[
 * gst-launch-1.0 filesrc location=video.ogg ! oggdemux ! theoraparse \
 *            ! oggmux ! filesink location=video-remuxed.ogg
 * ]|
 *  This pipeline shows remuxing. video-remuxed.ogg might not be exactly the same
 * as video.ogg, but they should produce exactly the same decoded data.
 *
 */

/* FIXME 0.11: suppress warnings for deprecated API such as GValueArray
 * with newer GLib versions (>= 2.31.0) */
#define GLIB_DISABLE_DEPRECATION_WARNINGS

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "gsttheoraparse.h"

#define GST_CAT_DEFAULT theoraparse_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

static GstStaticPadTemplate theora_parse_sink_factory =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-theora")
    );

static GstStaticPadTemplate theora_parse_src_factory =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-theora")
    );

enum
{
  PROP_0,
  PROP_SYNCHRONIZATION_POINTS
};

#define gst_theora_parse_parent_class parent_class
G_DEFINE_TYPE (GstTheoraParse, gst_theora_parse, GST_TYPE_ELEMENT);
GST_ELEMENT_REGISTER_DEFINE (theoraparse, "theoraparse",
    GST_RANK_NONE, GST_TYPE_THEORA_PARSE);

static void theora_parse_dispose (GObject * object);

#if 0
static void theora_parse_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void theora_parse_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
#endif

static GstFlowReturn theora_parse_chain (GstPad * pad, GstObject * parent,
    GstBuffer * buffer);
static GstStateChangeReturn theora_parse_change_state (GstElement * element,
    GstStateChange transition);
static gboolean theora_parse_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event);
static gboolean theora_parse_src_query (GstPad * pad, GstObject * parent,
    GstQuery * query);

static void
gst_theora_parse_class_init (GstTheoraParseClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);

  gobject_class->dispose = theora_parse_dispose;

#if 0
  gobject_class->get_property = theora_parse_get_property;
  gobject_class->set_property = theora_parse_set_property;

  /**
   * GstTheoraParse:sychronization-points
   *
   * An array of (granuletime, buffertime) pairs
   */
  g_object_class_install_property (gobject_class, PROP_SYNCHRONIZATION_POINTS,
      g_param_spec_value_array ("synchronization-points",
          "Synchronization points",
          "An array of (granuletime, buffertime) pairs",
          g_param_spec_uint64 ("time", "Time",
              "Time (either granuletime or buffertime)", 0, G_MAXUINT64, 0,
              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS),
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
#endif

  gst_element_class_add_static_pad_template (gstelement_class,
      &theora_parse_src_factory);
  gst_element_class_add_static_pad_template (gstelement_class,
      &theora_parse_sink_factory);
  gst_element_class_set_static_metadata (gstelement_class,
      "Theora video parser", "Codec/Parser/Video", "parse raw theora streams",
      "Andy Wingo <wingo@pobox.com>");

  gstelement_class->change_state = theora_parse_change_state;

  GST_DEBUG_CATEGORY_INIT (theoraparse_debug, "theoraparse", 0,
      "Theora parser");
}

static void
gst_theora_parse_init (GstTheoraParse * parse)
{
  parse->sinkpad =
      gst_pad_new_from_static_template (&theora_parse_sink_factory, "sink");
  gst_pad_set_chain_function (parse->sinkpad, theora_parse_chain);
  gst_pad_set_event_function (parse->sinkpad, theora_parse_sink_event);
  gst_element_add_pad (GST_ELEMENT (parse), parse->sinkpad);

  parse->srcpad =
      gst_pad_new_from_static_template (&theora_parse_src_factory, "src");
  gst_pad_set_query_function (parse->srcpad, theora_parse_src_query);
  gst_element_add_pad (GST_ELEMENT (parse), parse->srcpad);
}

static void
theora_parse_dispose (GObject * object)
{
  GstTheoraParse *parse = GST_THEORA_PARSE (object);

  g_free (parse->times);
  parse->times = NULL;

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

#if 0
static void
theora_parse_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstTheoraParse *parse = GST_THEORA_PARSE (object);

  switch (prop_id) {
    case PROP_SYNCHRONIZATION_POINTS:
    {
      GValueArray *array;
      guint i;

      array = g_value_get_boxed (value);

      if (array) {
        if (array->n_values % 2)
          goto odd_values;

        g_free (parse->times);
        parse->times = g_new (GstClockTime, array->n_values);
        parse->npairs = array->n_values / 2;
        for (i = 0; i < array->n_values; i++)
          parse->times[i] = g_value_get_uint64 (&array->values[i]);
      } else {
        g_free (parse->times);
        parse->npairs = 0;
      }
    }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  return;

odd_values:
  {
    g_critical ("expected an even number of time values for "
        "synchronization-points");
    return;
  }
}

static void
theora_parse_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstTheoraParse *parse = GST_THEORA_PARSE (object);

  switch (prop_id) {
    case PROP_SYNCHRONIZATION_POINTS:
    {
      GValueArray *array = NULL;
      guint i;

      array = g_value_array_new (parse->npairs * 2);

      for (i = 0; i < parse->npairs; i++) {
        GValue v = { 0, };

        g_value_init (&v, G_TYPE_UINT64);
        g_value_set_uint64 (&v, parse->times[i * 2]);
        g_value_array_append (array, &v);
        g_value_set_uint64 (&v, parse->times[i * 2 + 1]);
        g_value_array_append (array, &v);
        g_value_unset (&v);
      }

      g_value_take_boxed (value, array);
    }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}
#endif

static void
theora_parse_set_header_on_caps (GstTheoraParse * parse, GstCaps * caps)
{
  GstBuffer **bufs;
  GstStructure *structure;
  gint i;
  GValue array = { 0 };
  GValue value = { 0 };

  bufs = parse->streamheader;
  structure = gst_caps_get_structure (caps, 0);
  g_value_init (&array, GST_TYPE_ARRAY);

  for (i = 0; i < 3; i++) {
    if (bufs[i] == NULL)
      continue;

    bufs[i] = gst_buffer_make_writable (bufs[i]);
    GST_BUFFER_FLAG_SET (bufs[i], GST_BUFFER_FLAG_HEADER);

    g_value_init (&value, GST_TYPE_BUFFER);
    gst_value_set_buffer (&value, bufs[i]);
    gst_value_array_append_value (&array, &value);
    g_value_unset (&value);
  }

  gst_structure_take_value (structure, "streamheader", &array);
}

/* two tasks to do here: set the streamheader on the caps, and use libtheora to
   parse the headers */
static void
theora_parse_set_streamheader (GstTheoraParse * parse)
{
  GstCaps *caps;
  gint i;
  guint32 bitstream_version;
  th_setup_info *setup = NULL;

  g_assert (!parse->streamheader_received);

  caps = gst_caps_make_writable (gst_pad_query_caps (parse->srcpad, NULL));
  theora_parse_set_header_on_caps (parse, caps);
  GST_DEBUG_OBJECT (parse, "here are the caps: %" GST_PTR_FORMAT, caps);
  gst_pad_set_caps (parse->srcpad, caps);
  gst_caps_unref (caps);

  for (i = 0; i < 3; i++) {
    ogg_packet packet;
    GstBuffer *buf;
    int ret;
    GstMapInfo map;

    buf = parse->streamheader[i];
    if (buf == NULL)
      continue;

    gst_buffer_map (buf, &map, GST_MAP_READ);
    packet.packet = map.data;
    packet.bytes = map.size;
    packet.granulepos = GST_BUFFER_OFFSET_END (buf);
    packet.packetno = i + 1;
    packet.e_o_s = 0;
    packet.b_o_s = (i == 0);
    ret = th_decode_headerin (&parse->info, &parse->comment, &setup, &packet);
    gst_buffer_unmap (buf, &map);
    if (ret < 0) {
      GST_WARNING_OBJECT (parse, "Failed to decode Theora header %d: %d",
          i + 1, ret);
    }
  }
  if (setup) {
    th_setup_free (setup);
  }

  parse->fps_n = parse->info.fps_numerator;
  parse->fps_d = parse->info.fps_denominator;
  parse->shift = parse->info.keyframe_granule_shift;

  /* With libtheora-1.0beta1 the granulepos scheme was changed:
   * where earlier the granulepos referred to the index/beginning
   * of a frame, it now refers to the end, which matches the use
   * in vorbis/speex. We check the bitstream version from the header so
   * we know which way to interpret the incoming granuepos
   */
  bitstream_version = (parse->info.version_major << 16) |
      (parse->info.version_minor << 8) | parse->info.version_subminor;
  parse->is_old_bitstream = (bitstream_version <= 0x00030200);

  parse->streamheader_received = TRUE;
}

static void
theora_parse_drain_event_queue (GstTheoraParse * parse)
{
  while (parse->event_queue->length) {
    GstEvent *event;

    event = GST_EVENT_CAST (g_queue_pop_head (parse->event_queue));
    gst_pad_event_default (parse->sinkpad, GST_OBJECT_CAST (parse), event);
  }
}

static void
theora_parse_push_headers (GstTheoraParse * parse)
{
  gint i;

  if (!parse->streamheader_received)
    theora_parse_set_streamheader (parse);

  theora_parse_drain_event_queue (parse);

  /* ignore return values, we pass along the result of pushing data packets only
   */
  for (i = 0; i < 3; i++) {
    GstBuffer *buf;

    if ((buf = parse->streamheader[i])) {
      gst_pad_push (parse->srcpad, buf);
      parse->streamheader[i] = NULL;
    }
  }
}

static void
theora_parse_clear_queue (GstTheoraParse * parse)
{
  while (parse->buffer_queue->length) {
    GstBuffer *buf;

    buf = GST_BUFFER_CAST (g_queue_pop_head (parse->buffer_queue));
    gst_buffer_unref (buf);
  }
  while (parse->event_queue->length) {
    GstEvent *event;

    event = GST_EVENT_CAST (g_queue_pop_head (parse->event_queue));
    gst_event_unref (event);
  }
}

static gint64
make_granulepos (GstTheoraParse * parse, gint64 keyframe, gint64 frame)
{
  gint64 iframe;

  if (keyframe == -1)
    keyframe = 0;
  /* If using newer theora, offset the granulepos by +1, see comment in
   * theora_parse_set_streamheader.
   * 
   * We don't increment keyframe directly, as internally we always index frames
   * starting from 0 and we do some sanity checking below. */
  if (!parse->is_old_bitstream)
    iframe = keyframe + 1;
  else
    iframe = keyframe;

  g_return_val_if_fail (frame >= keyframe, -1);
  g_return_val_if_fail (frame - keyframe < 1 << parse->shift, -1);

  return (iframe << parse->shift) + (frame - keyframe);
}

static void
parse_granulepos (GstTheoraParse * parse, gint64 granulepos,
    gint64 * keyframe, gint64 * frame)
{
  gint64 kf;

  kf = granulepos >> parse->shift;
  /* If using newer theora, offset the granulepos by -1, see comment
   * in theora_parse_set_streamheader */
  if (!parse->is_old_bitstream)
    kf -= 1;
  if (keyframe)
    *keyframe = kf;
  if (frame)
    *frame = kf + (granulepos & ((1 << parse->shift) - 1));
}

static gboolean
is_keyframe (GstBuffer * buf)
{
  gsize size;
  guint8 data[1];

  size = gst_buffer_get_size (buf);
  if (size == 0)
    return FALSE;

  gst_buffer_extract (buf, 0, data, 1);

  return ((data[0] & 0x40) == 0);
}

static void
theora_parse_munge_granulepos (GstTheoraParse * parse, GstBuffer * buf,
    gint64 keyframe, gint64 frame)
{
  gint64 frames_diff;
  GstClockTimeDiff time_diff;

  if (keyframe == frame) {
    gint i;

    /* update granule_offset */
    for (i = 0; i < parse->npairs; i++) {
      if (parse->times[i * 2] >= GST_BUFFER_OFFSET (buf))
        break;
    }
    if (i > 0) {
      /* time_diff gets reset below */
      time_diff = parse->times[i * 2 - 1] - parse->times[i * 2 - 2];
      parse->granule_offset = gst_util_uint64_scale (time_diff,
          parse->fps_n, parse->fps_d * GST_SECOND);
      parse->granule_offset <<= parse->shift;
    }
  }

  frames_diff = parse->granule_offset >> parse->shift;
  time_diff = gst_util_uint64_scale_int (GST_SECOND * frames_diff,
      parse->fps_d, parse->fps_n);

  GST_DEBUG_OBJECT (parse, "offsetting theora stream by %" G_GINT64_FORMAT
      " frames (%" GST_TIME_FORMAT ")", frames_diff, GST_TIME_ARGS (time_diff));

  GST_BUFFER_OFFSET_END (buf) += parse->granule_offset;
  GST_BUFFER_OFFSET (buf) += time_diff;
  GST_BUFFER_TIMESTAMP (buf) += time_diff;
}

static GstFlowReturn
theora_parse_push_buffer (GstTheoraParse * parse, GstBuffer * buf,
    gint64 keyframe, gint64 frame)
{

  GstClockTime this_time, next_time;

  this_time = gst_util_uint64_scale_int (GST_SECOND * frame,
      parse->fps_d, parse->fps_n);

  next_time = gst_util_uint64_scale_int (GST_SECOND * (frame + 1),
      parse->fps_d, parse->fps_n);

  GST_BUFFER_OFFSET_END (buf) = make_granulepos (parse, keyframe, frame);
  GST_BUFFER_OFFSET (buf) = this_time;
  GST_BUFFER_TIMESTAMP (buf) = this_time;
  GST_BUFFER_DURATION (buf) = next_time - this_time;

  if (parse->times)
    theora_parse_munge_granulepos (parse, buf, keyframe, frame);

  GST_DEBUG_OBJECT (parse, "pushing buffer with granulepos %" G_GINT64_FORMAT
      "|%" G_GINT64_FORMAT, keyframe, frame - keyframe);

  return gst_pad_push (parse->srcpad, buf);
}

static GstFlowReturn
theora_parse_drain_queue_prematurely (GstTheoraParse * parse)
{
  GstFlowReturn ret = GST_FLOW_OK;

  /* got an EOS event, make sure to push out any buffers that were in the queue
   * -- won't normally be the case, but this catches the
   * didn't-get-a-granulepos-on-the-last-packet case. Assuming a continuous
   * stream. */

  GST_DEBUG_OBJECT (parse, "got EOS, draining queue");

  /* if we get an eos before pushing the streamheaders, drain our events before
   * eos */
  theora_parse_drain_event_queue (parse);

  while (!g_queue_is_empty (parse->buffer_queue)) {
    GstBuffer *buf;

    buf = GST_BUFFER_CAST (g_queue_pop_head (parse->buffer_queue));

    parse->prev_frame++;

    if (is_keyframe (buf))
      /* we have a keyframe */
      parse->prev_keyframe = parse->prev_frame;
    else
      GST_BUFFER_FLAGS (buf) |= GST_BUFFER_FLAG_DELTA_UNIT;

    if (parse->prev_keyframe < 0) {
      if (GST_BUFFER_OFFSET_END_IS_VALID (buf)) {
        parse_granulepos (parse, GST_BUFFER_OFFSET_END (buf),
            &parse->prev_keyframe, NULL);
      } else {
        /* No previous keyframe known; can't extract one from this frame. That
         * means we can't do any valid output for this frame, just continue to
         * the next frame.
         */
        gst_buffer_unref (buf);
        continue;
      }
    }

    ret = theora_parse_push_buffer (parse, buf, parse->prev_keyframe,
        parse->prev_frame);

    if (ret != GST_FLOW_OK)
      goto done;
  }

done:
  return ret;
}

static GstFlowReturn
theora_parse_drain_queue (GstTheoraParse * parse, gint64 granulepos)
{
  GstFlowReturn ret = GST_FLOW_OK;
  gint64 keyframe, prev_frame, frame;

  parse_granulepos (parse, granulepos, &keyframe, &frame);

  GST_DEBUG ("draining queue of length %d",
      g_queue_get_length (parse->buffer_queue));

  GST_LOG_OBJECT (parse, "gp %" G_GINT64_FORMAT ", kf %" G_GINT64_FORMAT
      ", frame %" G_GINT64_FORMAT, granulepos, keyframe, frame);

  prev_frame = frame - g_queue_get_length (parse->buffer_queue);

  GST_LOG_OBJECT (parse,
      "new prev %" G_GINT64_FORMAT ", prev %" G_GINT64_FORMAT, prev_frame,
      parse->prev_frame);

  if (prev_frame < parse->prev_frame) {
    GST_WARNING ("jumped %" G_GINT64_FORMAT
        " frames backwards! not sure what to do here",
        parse->prev_frame - prev_frame);
    parse->prev_frame = prev_frame;
  } else if (prev_frame > parse->prev_frame) {
    GST_INFO ("discontinuity detected (%" G_GINT64_FORMAT
        " frames)", prev_frame - parse->prev_frame);
    if (keyframe <= prev_frame && keyframe > parse->prev_keyframe)
      parse->prev_keyframe = keyframe;
    parse->prev_frame = prev_frame;
  }

  while (!g_queue_is_empty (parse->buffer_queue)) {
    GstBuffer *buf;

    parse->prev_frame++;
    g_assert (parse->prev_frame >= 0);

    buf = GST_BUFFER_CAST (g_queue_pop_head (parse->buffer_queue));

    if (is_keyframe (buf))
      /* we have a keyframe */
      parse->prev_keyframe = parse->prev_frame;
    else
      GST_BUFFER_FLAGS (buf) |= GST_BUFFER_FLAG_DELTA_UNIT;

    ret = theora_parse_push_buffer (parse, buf, parse->prev_keyframe,
        parse->prev_frame);

    if (ret != GST_FLOW_OK)
      goto done;
  }

done:
  return ret;
}

static GstFlowReturn
theora_parse_queue_buffer (GstTheoraParse * parse, GstBuffer * buf)
{
  GstFlowReturn ret = GST_FLOW_OK;

  buf = gst_buffer_make_writable (buf);

  g_queue_push_tail (parse->buffer_queue, buf);

  if (GST_BUFFER_OFFSET_END_IS_VALID (buf)) {
    if (parse->prev_keyframe < 0) {
      parse_granulepos (parse, GST_BUFFER_OFFSET_END (buf),
          &parse->prev_keyframe, NULL);
    }
    ret = theora_parse_drain_queue (parse, GST_BUFFER_OFFSET_END (buf));
  }

  return ret;
}

static GstFlowReturn
theora_parse_chain (GstPad * pad, GstObject * parent, GstBuffer * buffer)
{
  GstFlowReturn ret;
  GstTheoraParse *parse;
  GstMapInfo map;
  guint8 header;
  gboolean have_header;

  parse = GST_THEORA_PARSE (parent);

  have_header = FALSE;

  gst_buffer_map (buffer, &map, GST_MAP_READ);
  header = map.data[0];
  gst_buffer_unmap (buffer, &map);

  if (map.size >= 1) {
    if (header & 0x80)
      have_header = TRUE;
  }

  if (have_header) {
    if (parse->send_streamheader) {
      /* we need to collect the headers still */
      /* so put it on the streamheader list and return */
      if (header >= 0x80 && header <= 0x82)
        parse->streamheader[header - 0x80] = buffer;
    }
    ret = GST_FLOW_OK;
  } else {
    /* data packet, push the headers we collected before */
    if (parse->send_streamheader) {
      theora_parse_push_headers (parse);
      parse->send_streamheader = FALSE;
    }

    ret = theora_parse_queue_buffer (parse, buffer);
  }

  return ret;
}

static gboolean
theora_parse_queue_event (GstTheoraParse * parse, GstEvent * event)
{
  g_queue_push_tail (parse->event_queue, event);
  return TRUE;
}

static gboolean
theora_parse_sink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  gboolean ret;
  GstTheoraParse *parse;

  parse = GST_THEORA_PARSE (parent);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_FLUSH_STOP:
      theora_parse_clear_queue (parse);
      parse->prev_keyframe = -1;
      parse->prev_frame = -1;
      ret = gst_pad_event_default (pad, parent, event);
      break;
    case GST_EVENT_EOS:
      theora_parse_drain_queue_prematurely (parse);
      ret = gst_pad_event_default (pad, parent, event);
      break;
    default:
      if (parse->send_streamheader && GST_EVENT_IS_SERIALIZED (event)
          && GST_EVENT_TYPE (event) > GST_EVENT_CAPS)
        ret = theora_parse_queue_event (parse, event);
      else
        ret = gst_pad_event_default (pad, parent, event);
      break;
  }

  return ret;
}

static gboolean
theora_parse_src_convert (GstPad * pad,
    GstFormat src_format, gint64 src_value,
    GstFormat * dest_format, gint64 * dest_value)
{
  gboolean res = TRUE;
  GstTheoraParse *parse;
  guint64 scale = 1;

  if (src_format == *dest_format) {
    *dest_value = src_value;
    return TRUE;
  }

  parse = GST_THEORA_PARSE (gst_pad_get_parent (pad));

  /* we need the info part before we can done something */
  if (!parse->streamheader_received)
    goto no_header;

  switch (src_format) {
    case GST_FORMAT_BYTES:
      switch (*dest_format) {
        case GST_FORMAT_DEFAULT:
          *dest_value = gst_util_uint64_scale_int (src_value, 2,
              parse->info.pic_height * parse->info.pic_width * 3);
          break;
        case GST_FORMAT_TIME:
          /* seems like a rather silly conversion, implement me if you like */
        default:
          res = FALSE;
      }
      break;
    case GST_FORMAT_TIME:
      switch (*dest_format) {
        case GST_FORMAT_BYTES:
          scale = 3 * (parse->info.pic_width * parse->info.pic_height) / 2;
        case GST_FORMAT_DEFAULT:
          *dest_value = scale * gst_util_uint64_scale (src_value,
              parse->info.fps_numerator,
              parse->info.fps_denominator * GST_SECOND);
          break;
        default:
          GST_DEBUG_OBJECT (parse, "cannot convert to format %s",
              gst_format_get_name (*dest_format));
          res = FALSE;
      }
      break;
    case GST_FORMAT_DEFAULT:
      switch (*dest_format) {
        case GST_FORMAT_TIME:
          *dest_value = gst_util_uint64_scale (src_value,
              GST_SECOND * parse->info.fps_denominator,
              parse->info.fps_numerator);
          break;
        case GST_FORMAT_BYTES:
          *dest_value = gst_util_uint64_scale_int (src_value,
              3 * parse->info.pic_width * parse->info.pic_height, 2);
          break;
        default:
          res = FALSE;
      }
      break;
    default:
      res = FALSE;
  }
done:
  gst_object_unref (parse);
  return res;

  /* ERRORS */
no_header:
  {
    GST_DEBUG_OBJECT (parse, "no header yet, cannot convert");
    res = FALSE;
    goto done;
  }
}

static gboolean
theora_parse_src_query (GstPad * pad, GstObject * parent, GstQuery * query)
{
  GstTheoraParse *parse;
  gboolean res = FALSE;

  parse = GST_THEORA_PARSE (parent);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_POSITION:
    {
      gint64 frame, value;
      GstFormat my_format, format;
      gint64 time;

      frame = parse->prev_frame;

      GST_LOG_OBJECT (parse,
          "query %p: we have current frame: %" G_GINT64_FORMAT, query, frame);

      /* parse format */
      gst_query_parse_position (query, &format, NULL);

      /* and convert to the final format in two steps with time as the 
       * intermediate step */
      my_format = GST_FORMAT_TIME;
      if (!(res =
              theora_parse_src_convert (parse->sinkpad, GST_FORMAT_DEFAULT,
                  frame, &my_format, &time)))
        goto error;

      /* fixme: handle segments
         time = (time - parse->segment.start) + parse->segment.time;
       */

      GST_LOG_OBJECT (parse,
          "query %p: our time: %" GST_TIME_FORMAT " (conv to %s)",
          query, GST_TIME_ARGS (time), gst_format_get_name (format));

      if (!(res =
              theora_parse_src_convert (pad, my_format, time, &format, &value)))
        goto error;

      gst_query_set_position (query, format, value);

      GST_LOG_OBJECT (parse,
          "query %p: we return %" G_GINT64_FORMAT " (format %u)", query, value,
          format);

      break;
    }
    case GST_QUERY_DURATION:
      /* forward to peer for total */
      if (!(res = gst_pad_query (GST_PAD_PEER (parse->sinkpad), query)))
        goto error;
      break;
    case GST_QUERY_CONVERT:
    {
      GstFormat src_fmt, dest_fmt;
      gint64 src_val, dest_val;

      gst_query_parse_convert (query, &src_fmt, &src_val, &dest_fmt, &dest_val);
      if (!(res =
              theora_parse_src_convert (pad, src_fmt, src_val, &dest_fmt,
                  &dest_val)))
        goto error;

      gst_query_set_convert (query, src_fmt, src_val, dest_fmt, dest_val);
      break;
    }
    default:
      res = gst_pad_query_default (pad, parent, query);
      break;
  }
done:

  return res;

  /* ERRORS */
error:
  {
    GST_DEBUG_OBJECT (parse, "query failed");
    goto done;
  }
}

static GstStateChangeReturn
theora_parse_change_state (GstElement * element, GstStateChange transition)
{
  GstTheoraParse *parse = GST_THEORA_PARSE (element);
  GstStateChangeReturn ret;
  gint i;

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      th_info_init (&parse->info);
      th_comment_init (&parse->comment);
      parse->send_streamheader = TRUE;
      parse->buffer_queue = g_queue_new ();
      parse->event_queue = g_queue_new ();
      parse->prev_keyframe = -1;
      parse->prev_frame = -1;
      parse->granule_offset = 0;
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      th_info_clear (&parse->info);
      th_comment_clear (&parse->comment);
      theora_parse_clear_queue (parse);
      g_queue_free (parse->buffer_queue);
      g_queue_free (parse->event_queue);
      parse->buffer_queue = NULL;
      for (i = 0; i < 3; i++) {
        if (parse->streamheader[i]) {
          gst_buffer_unref (parse->streamheader[i]);
          parse->streamheader[i] = NULL;
        }
      }
      parse->streamheader_received = FALSE;
      break;
    default:
      break;
  }

  return ret;
}
