/* GStreamer RTP base depayloader unit tests
 * Copyright (C) 2014 Sebastian Rasmussen <sebras@hotmail.com>
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
 * You should have received a copy of the GNU Library General
 * Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/check/gstcheck.h>
#include <gst/check/gstharness.h>
#include <gst/rtp/rtp.h>

#include "rtpdummyhdrextimpl.c"

#define DEFAULT_CLOCK_RATE (42)

/* GstRtpDummyDepay */

#define GST_TYPE_RTP_DUMMY_DEPAY \
  (gst_rtp_dummy_depay_get_type())
#define GST_RTP_DUMMY_DEPAY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_RTP_DUMMY_DEPAY,GstRtpDummyDepay))
#define GST_RTP_DUMMY_DEPAY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_RTP_DUMMY_DEPAY,GstRtpDummyDepayClass))
#define GST_IS_RTP_DUMMY_DEPAY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_RTP_DUMMY_DEPAY))
#define GST_IS_RTP_DUMMY_DEPAY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_RTP_DUMMY_DEPAY))

typedef enum
{
  GST_RTP_DUMMY_RETURN_TO_PUSH,
  GST_RTP_DUMMY_USE_PUSH_FUNC,
  GST_RTP_DUMMY_USE_PUSH_LIST_FUNC,
  GST_RTP_DUMMY_USE_PUSH_AGGREGATE_FUNC,
} GstRtpDummyPushMethod;

typedef enum
{
  GST_RTP_DUMMY_PUSH_AGGREGATE_DEFAULT,
  GST_RTP_DUMMY_PUSH_AGGREGATE_DROP,
  GST_RTP_DUMMY_PUSH_AGGREGATE_DELAYED,
  GST_RTP_DUMMY_PUSH_AGGREGATE_FLUSH,
} GstRtpDummyPushAggregateMethod;

typedef struct _GstRtpDummyDepay GstRtpDummyDepay;
typedef struct _GstRtpDummyDepayClass GstRtpDummyDepayClass;

struct _GstRtpDummyDepay
{
  GstRTPBaseDepayload depayload;
  guint64 rtptime;

  GstRtpDummyPushMethod push_method;
  guint num_buffers_in_blist;

  GstRtpDummyPushAggregateMethod aggregate_method;
  guint num_buffers_to_aggregate;
  guint num_buffers_aggregated;
};

struct _GstRtpDummyDepayClass
{
  GstRTPBaseDepayloadClass parent_class;
};

GType gst_rtp_dummy_depay_get_type (void);

G_DEFINE_TYPE (GstRtpDummyDepay, gst_rtp_dummy_depay,
    GST_TYPE_RTP_BASE_DEPAYLOAD);

static GstBuffer *gst_rtp_dummy_depay_process (GstRTPBaseDepayload * depayload,
    GstBuffer * buf);
static gboolean gst_rtp_dummy_depay_set_caps (GstRTPBaseDepayload * filter,
    GstCaps * caps);

static GstStaticPadTemplate gst_rtp_dummy_depay_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate gst_rtp_dummy_depay_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

static void
gst_rtp_dummy_depay_class_init (GstRtpDummyDepayClass * klass)
{
  GstElementClass *gstelement_class;
  GstRTPBaseDepayloadClass *gstrtpbasedepayload_class;

  gstelement_class = GST_ELEMENT_CLASS (klass);
  gstrtpbasedepayload_class = GST_RTP_BASE_DEPAYLOAD_CLASS (klass);

  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_rtp_dummy_depay_sink_template);
  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_rtp_dummy_depay_src_template);

  gstrtpbasedepayload_class->process = gst_rtp_dummy_depay_process;
  gstrtpbasedepayload_class->set_caps = gst_rtp_dummy_depay_set_caps;
}

static void
gst_rtp_dummy_depay_init (GstRtpDummyDepay * depay)
{
  depay->rtptime = 0;
  depay->num_buffers_in_blist = 1;
  depay->num_buffers_to_aggregate = 1;
  depay->num_buffers_aggregated = 0;
}

static GstRtpDummyDepay *
rtp_dummy_depay_new (void)
{
  return g_object_new (GST_TYPE_RTP_DUMMY_DEPAY, NULL);
}

static GstBuffer *
gst_rtp_dummy_depay_process (GstRTPBaseDepayload * depayload, GstBuffer * buf)
{
  GstRtpDummyDepay *self = GST_RTP_DUMMY_DEPAY (depayload);
  GstRTPBuffer rtp = GST_RTP_BUFFER_INIT;
  GstBuffer *outbuf;
  guint32 rtptime;
  guint i;

  GST_LOG ("depayloading buffer pts=%" GST_TIME_FORMAT " offset=%"
      G_GUINT64_FORMAT " memories=%d", GST_TIME_ARGS (GST_BUFFER_PTS (buf)),
      GST_BUFFER_OFFSET (buf), gst_buffer_n_memory (buf));

  for (i = 0; i < gst_buffer_n_memory (buf); i++) {
    GstMemory *mem = gst_buffer_get_memory (buf, 0);
    gsize size, offset, maxsize;
    size = gst_memory_get_sizes (mem, &offset, &maxsize);
    GST_LOG ("\tsize=%" G_GSIZE_FORMAT " offset=%" G_GSIZE_FORMAT " maxsize=%"
        G_GSIZE_FORMAT, size, offset, maxsize);
    gst_memory_unref (mem);
  }

  gst_rtp_buffer_map (buf, GST_MAP_READ, &rtp);
  outbuf = gst_rtp_buffer_get_payload_buffer (&rtp);
  rtptime = gst_rtp_buffer_get_timestamp (&rtp);
  gst_rtp_buffer_unmap (&rtp);

  GST_BUFFER_PTS (outbuf) = GST_BUFFER_PTS (buf);
  GST_BUFFER_OFFSET (outbuf) = GST_BUFFER_OFFSET (buf);

  GST_LOG ("depayloaded buffer pts=%" GST_TIME_FORMAT " offset=%"
      G_GUINT64_FORMAT " rtptime=%" G_GUINT32_FORMAT " memories=%d",
      GST_TIME_ARGS (GST_BUFFER_PTS (outbuf)),
      GST_BUFFER_OFFSET (outbuf), rtptime, gst_buffer_n_memory (buf));

  for (i = 0; i < gst_buffer_n_memory (buf); i++) {
    GstMemory *mem = gst_buffer_get_memory (buf, 0);
    gsize size, offset, maxsize;
    size = gst_memory_get_sizes (mem, &offset, &maxsize);
    GST_LOG ("\tsize=%" G_GSIZE_FORMAT " offset=%" G_GSIZE_FORMAT " maxsize=%"
        G_GSIZE_FORMAT, size, offset, maxsize);
    gst_memory_unref (mem);
  }

  switch (self->push_method) {
    case GST_RTP_DUMMY_USE_PUSH_FUNC:
      gst_rtp_base_depayload_push (depayload, outbuf);
      outbuf = NULL;
      break;
    case GST_RTP_DUMMY_USE_PUSH_LIST_FUNC:{
      GstBufferList *blist = gst_buffer_list_new ();
      gint i;
      gst_buffer_list_add (blist, outbuf);
      for (i = 0; i != self->num_buffers_in_blist - 1; ++i) {
        gst_buffer_list_add (blist, gst_buffer_copy (outbuf));
      }
      outbuf = NULL;
      gst_rtp_base_depayload_push_list (depayload, blist);
      break;
    }
    case GST_RTP_DUMMY_USE_PUSH_AGGREGATE_FUNC:
      ++self->num_buffers_aggregated;
      if (self->num_buffers_aggregated != self->num_buffers_to_aggregate) {
        switch (self->aggregate_method) {
          case GST_RTP_DUMMY_PUSH_AGGREGATE_DROP:
            gst_rtp_base_depayload_dropped (depayload);
            break;
          case GST_RTP_DUMMY_PUSH_AGGREGATE_DEFAULT:
          case GST_RTP_DUMMY_PUSH_AGGREGATE_DELAYED:
          case GST_RTP_DUMMY_PUSH_AGGREGATE_FLUSH:
            break;
        }
        gst_clear_buffer (&outbuf);
      } else {
        switch (self->aggregate_method) {
          case GST_RTP_DUMMY_PUSH_AGGREGATE_DELAYED:
            gst_rtp_base_depayload_delayed (depayload);
            break;
          case GST_RTP_DUMMY_PUSH_AGGREGATE_FLUSH:
            gst_rtp_base_depayload_flush (depayload, TRUE);
            break;
          case GST_RTP_DUMMY_PUSH_AGGREGATE_DROP:
          case GST_RTP_DUMMY_PUSH_AGGREGATE_DEFAULT:
            break;
        }
        self->num_buffers_aggregated = 0;
      }
      break;
    case GST_RTP_DUMMY_RETURN_TO_PUSH:
      break;
  }

  return outbuf;
}

static gboolean
gst_rtp_dummy_depay_set_caps (GstRTPBaseDepayload * filter, GstCaps * caps)
{
  GstEvent *event;
  event = gst_event_new_caps (caps);
  gst_pad_push_event (filter->srcpad, event);
  return TRUE;
}

/* Helper functions and global state */

static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

typedef struct State State;

struct State
{
  GstElement *element;
  GstPad *sinkpad;
  GstPad *srcpad;
};

static GList *events;

static gboolean
event_func (GstPad * pad, GstObject * noparent, GstEvent * event)
{
  events = g_list_append (events, gst_event_ref (event));
  return gst_pad_event_default (pad, noparent, event);
}

static void
drop_events (void)
{
  while (events != NULL) {
    gst_event_unref (GST_EVENT (events->data));
    events = g_list_delete_link (events, events);
  }
}

static void
validate_events_received (guint received)
{
  fail_unless_equals_int (g_list_length (events), received);
}

static void
validate_event (guint index, const gchar * name, const gchar * field, ...)
{
  GstEvent *event;
  va_list var_args;

  fail_if (index >= g_list_length (events));
  event = GST_EVENT (g_list_nth_data (events, index));
  fail_if (event == NULL);

  GST_TRACE ("%" GST_PTR_FORMAT, event);

  fail_unless_equals_string (GST_EVENT_TYPE_NAME (event), name);

  va_start (var_args, field);
  while (field) {
    if (!g_strcmp0 (field, "timestamp")) {
      GstClockTime expected = va_arg (var_args, GstClockTime);
      GstClockTime timestamp, duration;
      gst_event_parse_gap (event, &timestamp, &duration);
      fail_unless_equals_uint64 (timestamp, expected);
    } else if (!g_strcmp0 (field, "duration")) {
      GstClockTime expected = va_arg (var_args, GstClockTime);
      GstClockTime timestamp, duration;
      gst_event_parse_gap (event, &timestamp, &duration);
      fail_unless_equals_uint64 (duration, expected);
    } else if (!g_strcmp0 (field, "time")) {
      GstClockTime expected = va_arg (var_args, GstClockTime);
      const GstSegment *segment;
      gst_event_parse_segment (event, &segment);
      fail_unless_equals_uint64 (segment->time, expected);
    } else if (!g_strcmp0 (field, "start")) {
      GstClockTime expected = va_arg (var_args, GstClockTime);
      const GstSegment *segment;
      gst_event_parse_segment (event, &segment);
      fail_unless_equals_uint64 (segment->start, expected);
    } else if (!g_strcmp0 (field, "stop")) {
      GstClockTime expected = va_arg (var_args, GstClockTime);
      const GstSegment *segment;
      gst_event_parse_segment (event, &segment);
      fail_unless_equals_uint64 (segment->stop, expected);
    } else if (!g_strcmp0 (field, "applied-rate")) {
      gdouble expected = va_arg (var_args, gdouble);
      const GstSegment *segment;
      gst_event_parse_segment (event, &segment);
      fail_unless_equals_float (segment->applied_rate, expected);
    } else if (!g_strcmp0 (field, "rate")) {
      gdouble expected = va_arg (var_args, gdouble);
      const GstSegment *segment;
      gst_event_parse_segment (event, &segment);
      fail_unless_equals_float (segment->rate, expected);
    } else if (!g_strcmp0 (field, "base")) {
      GstClockTime expected = va_arg (var_args, GstClockTime);
      const GstSegment *segment;
      gst_event_parse_segment (event, &segment);
      fail_unless_equals_uint64 (segment->base, expected);
    } else if (!g_strcmp0 (field, "media-type")) {
      const gchar *expected = va_arg (var_args, const gchar *);
      GstCaps *caps;
      const gchar *media_type;
      gst_event_parse_caps (event, &caps);
      media_type = gst_structure_get_name (gst_caps_get_structure (caps, 0));
      fail_unless_equals_string (media_type, expected);
    } else if (!g_strcmp0 (field, "npt-start")) {
      GstClockTime expected = va_arg (var_args, GstClockTime);
      GstCaps *caps;
      GstClockTime start;
      gst_event_parse_caps (event, &caps);
      fail_unless (gst_structure_get_clock_time (gst_caps_get_structure (caps,
                  0), "npt-start", &start));
      fail_unless_equals_uint64 (start, expected);
    } else if (!g_strcmp0 (field, "npt-stop")) {
      GstClockTime expected = va_arg (var_args, GstClockTime);
      GstCaps *caps;
      GstClockTime stop;
      gst_event_parse_caps (event, &caps);
      fail_unless (gst_structure_get_clock_time (gst_caps_get_structure (caps,
                  0), "npt-stop", &stop));
      fail_unless_equals_uint64 (stop, expected);
    } else if (!g_strcmp0 (field, "play-speed")) {
      gdouble expected = va_arg (var_args, gdouble);
      GstCaps *caps;
      gdouble speed;
      gst_event_parse_caps (event, &caps);
      fail_unless (gst_structure_get_double (gst_caps_get_structure (caps, 0),
              "play-speed", &speed));
      fail_unless (speed == expected);
    } else if (!g_strcmp0 (field, "play-scale")) {
      gdouble expected = va_arg (var_args, gdouble);
      GstCaps *caps;
      gdouble scale;
      gst_event_parse_caps (event, &caps);
      fail_unless (gst_structure_get_double (gst_caps_get_structure (caps, 0),
              "play-scale", &scale));
      fail_unless (scale == expected);
    } else if (!g_strcmp0 (field, "clock-base")) {
      guint expected = va_arg (var_args, guint);
      GstCaps *caps;
      guint clock_base;
      gst_event_parse_caps (event, &caps);
      fail_unless (gst_structure_get_uint (gst_caps_get_structure (caps, 0),
              "clock-base", &clock_base));
      fail_unless (clock_base == expected);

    } else {
      fail ("test cannot validate unknown event field '%s'", field);
    }
    field = va_arg (var_args, const gchar *);
  }
  va_end (var_args);
}

static void
rtp_buffer_set_valist (GstBuffer * buf, const gchar * field, va_list var_args,
    gboolean * extra_ref_)
{
  GstRTPBuffer rtp = GST_RTP_BUFFER_INIT;
  gboolean mapped = FALSE;
  gboolean extra_ref = FALSE;

  while (field) {
    if (!g_strcmp0 (field, "pts")) {
      GstClockTime pts = va_arg (var_args, GstClockTime);
      GST_BUFFER_PTS (buf) = pts;
    } else if (!g_strcmp0 (field, "offset")) {
      guint64 offset = va_arg (var_args, guint64);
      GST_BUFFER_OFFSET (buf) = offset;
    } else if (!g_strcmp0 (field, "discont")) {
      gboolean discont = va_arg (var_args, gboolean);
      if (discont) {
        GST_BUFFER_FLAG_SET (buf, GST_BUFFER_FLAG_DISCONT);
      } else {
        GST_BUFFER_FLAG_UNSET (buf, GST_BUFFER_FLAG_DISCONT);
      }
    } else {
      if (!mapped) {
        gst_rtp_buffer_map (buf, GST_MAP_WRITE, &rtp);
        mapped = TRUE;
      }
      if (!g_strcmp0 (field, "rtptime")) {
        guint32 rtptime = va_arg (var_args, guint64);
        gst_rtp_buffer_set_timestamp (&rtp, rtptime);
      } else if (!g_strcmp0 (field, "payload-type")) {
        guint payload_type = va_arg (var_args, guint);
        gst_rtp_buffer_set_payload_type (&rtp, payload_type);
      } else if (!g_strcmp0 (field, "seq")) {
        guint seq = va_arg (var_args, guint);
        gst_rtp_buffer_set_seq (&rtp, seq);
      } else if (!g_strcmp0 (field, "ssrc")) {
        guint32 ssrc = va_arg (var_args, guint);
        gst_rtp_buffer_set_ssrc (&rtp, ssrc);
      } else if (!g_strcmp0 (field, "extra-ref")) {
        extra_ref = va_arg (var_args, gboolean);
        if (extra_ref_)
          *extra_ref_ = extra_ref;
      } else if (!g_strcmp0 (field, "csrc")) {
        guint idx = va_arg (var_args, guint);
        guint csrc = va_arg (var_args, guint);
        gst_rtp_buffer_set_csrc (&rtp, idx, csrc);
      } else if (g_str_has_prefix (field, "hdrext-")) {
        GstRTPHeaderExtension *ext = va_arg (var_args, GstRTPHeaderExtension *);
        guint id = gst_rtp_header_extension_get_id (ext);
        gsize size = gst_rtp_header_extension_get_max_size (ext, buf);
        guint8 *data = g_malloc0 (size);

        if (!g_strcmp0 (field, "hdrext-1")) {
          fail_unless (gst_rtp_header_extension_write (ext, buf,
                  GST_RTP_HEADER_EXTENSION_ONE_BYTE, buf, data, size) > 0);
          fail_unless (gst_rtp_buffer_add_extension_onebyte_header (&rtp, id,
                  data, size));
        } else if (!g_strcmp0 (field, "hdrext-2")) {
          fail_unless (gst_rtp_header_extension_write (ext, buf,
                  GST_RTP_HEADER_EXTENSION_TWO_BYTE, buf, data, size) > 0);
          fail_unless (gst_rtp_buffer_add_extension_twobytes_header (&rtp, 0,
                  id, data, size));
        }

        g_free (data);
      } else {
        fail ("test cannot set unknown buffer field '%s'", field);
      }
    }
    field = va_arg (var_args, const gchar *);
  }

  if (mapped) {
    gst_rtp_buffer_unmap (&rtp);
  }

  if (extra_ref)
    gst_buffer_ref (buf);
}

static void
rtp_buffer_set (GstBuffer * buf, const gchar * field, ...)
{
  va_list var_args;

  va_start (var_args, field);
  rtp_buffer_set_valist (buf, field, var_args, NULL);
  va_end (var_args);
}

#define push_rtp_buffer(state, field, ...) \
    push_rtp_buffer_full ((state), GST_FLOW_OK, (field), __VA_ARGS__)
#define push_rtp_buffer_fails(state, error, field, ...) \
    push_rtp_buffer_full ((state), (error), (field), __VA_ARGS__)

static void
push_rtp_buffer_full (State * state, GstFlowReturn expected,
    const gchar * field, ...)
{
  GstBuffer *buf = gst_rtp_buffer_new_allocate (0, 0, 0);
  va_list var_args;
  gboolean extra_ref = FALSE;

  va_start (var_args, field);
  rtp_buffer_set_valist (buf, field, var_args, &extra_ref);
  va_end (var_args);

  fail_unless_equals_int (gst_pad_push (state->srcpad, buf), expected);

  if (extra_ref)
    gst_buffer_unref (buf);
}

#define push_buffer(state, field, ...) \
    push_buffer_full ((state), GST_FLOW_OK, (field), __VA_ARGS__)

static void
push_buffer_full (State * state, GstFlowReturn expected,
    const gchar * field, ...)
{
  GstBuffer *buf = gst_buffer_new_allocate (0, 0, 0);
  va_list var_args;

  va_start (var_args, field);
  while (field) {
    if (!g_strcmp0 (field, "pts")) {
      GstClockTime pts = va_arg (var_args, GstClockTime);
      GST_BUFFER_PTS (buf) = pts;
    } else if (!g_strcmp0 (field, "offset")) {
      guint64 offset = va_arg (var_args, guint64);
      GST_BUFFER_OFFSET (buf) = offset;
    } else if (!g_strcmp0 (field, "discont")) {
      gboolean discont = va_arg (var_args, gboolean);
      if (discont) {
        GST_BUFFER_FLAG_SET (buf, GST_BUFFER_FLAG_DISCONT);
      } else {
        GST_BUFFER_FLAG_UNSET (buf, GST_BUFFER_FLAG_DISCONT);
      }
    } else {
      fail ("test cannot set unknown buffer field '%s'", field);
    }
    field = va_arg (var_args, const gchar *);
  }
  va_end (var_args);

  fail_unless_equals_int (gst_pad_push (state->srcpad, buf), expected);
}

static void
validate_buffers_received (guint received)
{
  fail_unless_equals_int (g_list_length (buffers), received);
}

static void
validate_buffer (guint index, const gchar * field, ...)
{
  GstBuffer *buf;
  va_list var_args;

  fail_if (index >= g_list_length (buffers));
  buf = GST_BUFFER (g_list_nth_data (buffers, (index)));
  fail_if (buf == NULL);

  GST_TRACE ("%" GST_PTR_FORMAT, buf);

  va_start (var_args, field);
  while (field) {
    if (!g_strcmp0 (field, "pts")) {
      GstClockTime pts = va_arg (var_args, GstClockTime);
      fail_unless_equals_uint64 (GST_BUFFER_PTS (buf), pts);
    } else if (!g_strcmp0 (field, "offset")) {
      guint64 offset = va_arg (var_args, guint64);
      fail_unless_equals_uint64 (GST_BUFFER_OFFSET (buf), offset);
    } else if (!g_strcmp0 (field, "discont")) {
      gboolean discont = va_arg (var_args, gboolean);
      if (discont) {
        fail_unless (GST_BUFFER_FLAG_IS_SET (buf, GST_BUFFER_FLAG_DISCONT));
      } else {
        fail_if (GST_BUFFER_FLAG_IS_SET (buf, GST_BUFFER_FLAG_DISCONT));
      }
    } else {
      fail ("test cannot validate unknown buffer field '%s'", field);
    }
    field = va_arg (var_args, const gchar *);
  }
  va_end (var_args);
}

static State *
create_depayloader (const gchar * caps_str, const gchar * property, ...)
{
  va_list var_args;
  GstCaps *caps;
  State *state;

  state = g_new0 (State, 1);

  state->element = GST_ELEMENT (rtp_dummy_depay_new ());
  fail_unless (GST_IS_RTP_DUMMY_DEPAY (state->element));

  va_start (var_args, property);
  g_object_set_valist (G_OBJECT (state->element), property, var_args);
  va_end (var_args);

  state->srcpad = gst_check_setup_src_pad (state->element, &srctemplate);
  state->sinkpad = gst_check_setup_sink_pad (state->element, &sinktemplate);

  fail_unless (gst_pad_set_active (state->srcpad, TRUE));
  fail_unless (gst_pad_set_active (state->sinkpad, TRUE));

  if (caps_str) {
    caps = gst_caps_from_string (caps_str);
  } else {
    caps = NULL;
  }
  gst_check_setup_events (state->srcpad, state->element, caps, GST_FORMAT_TIME);
  if (caps) {
    gst_caps_unref (caps);
  }

  gst_pad_set_chain_function (state->sinkpad, gst_check_chain_func);
  gst_pad_set_event_function (state->sinkpad, event_func);

  return state;
}

static void
set_state (State * state, GstState new_state)
{
  fail_unless_equals_int (gst_element_set_state (state->element, new_state),
      GST_STATE_CHANGE_SUCCESS);
}

static void
packet_lost (State * state, GstClockTime timestamp, GstClockTime duration,
    gboolean might_have_been_fec)
{
  GstEvent *event;
  guint seqnum = 0x4243;
  gboolean late = TRUE;
  guint retries = 42;

  event = gst_event_new_custom (GST_EVENT_CUSTOM_DOWNSTREAM,
      gst_structure_new ("GstRTPPacketLost",
          "seqnum", G_TYPE_UINT, seqnum,
          "timestamp", G_TYPE_UINT64, timestamp,
          "duration", G_TYPE_UINT64, duration,
          "might-have-been-fec", G_TYPE_BOOLEAN, might_have_been_fec,
          "late", G_TYPE_BOOLEAN, late, "retry", G_TYPE_UINT, retries, NULL));

  fail_unless (gst_pad_push_event (state->srcpad, event));
}

static void
reconfigure_caps (State * state, const gchar * caps_str)
{
  GstCaps *newcaps;
  GstEvent *event;
  newcaps = gst_caps_from_string (caps_str);
  event = gst_event_new_caps (newcaps);
  gst_caps_unref (newcaps);
  fail_unless (gst_pad_push_event (state->srcpad, event));
}

static void
flush_pipeline (State * state)
{
  GstEvent *event;
  GstSegment segment;
  event = gst_event_new_flush_start ();
  fail_unless (gst_pad_push_event (state->srcpad, event));
  event = gst_event_new_flush_stop (TRUE);
  fail_unless (gst_pad_push_event (state->srcpad, event));
  gst_segment_init (&segment, GST_FORMAT_TIME);
  event = gst_event_new_segment (&segment);
  fail_unless (gst_pad_push_event (state->srcpad, event));
}

static void
destroy_depayloader (State * state)
{
  gst_check_teardown_sink_pad (state->element);
  gst_check_teardown_src_pad (state->element);

  gst_check_drop_buffers ();
  drop_events ();

  g_object_unref (state->element);

  g_free (state);
}

/* Tests */

/* send two RTP packets having sequential sequence numbers and timestamps
 * differing by DEFAULT_CLOCK_RATE. the depayloader first pushes the normal
 * stream-start, caps and segment events downstream before processing each RTP
 * packet and pushing a corresponding buffer. PTS will be carried over from the
 * RTP packets by the payloader to the buffers. because the sequence numbers are
 * sequential then GST_BUFFER_FLAG_DISCONT will not be set for either buffer.
 */
GST_START_TEST (rtp_base_depayload_buffer_test)
{
  State *state;

  state = create_depayloader ("application/x-rtp", NULL);

  set_state (state, GST_STATE_PLAYING);

  push_rtp_buffer (state,
      "pts", 0 * GST_SECOND,
      "rtptime", G_GUINT64_CONSTANT (0x1234), "seq", 0x4242, NULL);

  push_rtp_buffer (state,
      "pts", 1 * GST_SECOND,
      "rtptime", G_GUINT64_CONSTANT (0x1234) + 1 * DEFAULT_CLOCK_RATE,
      "seq", 0x4242 + 1, NULL);

  set_state (state, GST_STATE_NULL);

  validate_buffers_received (2);

  validate_buffer (0, "pts", 0 * GST_SECOND, "discont", FALSE, NULL);

  validate_buffer (1, "pts", 1 * GST_SECOND, "discont", FALSE, NULL);

  validate_events_received (3);

  validate_event (0, "stream-start", NULL);

  validate_event (1, "caps", "media-type", "application/x-rtp", NULL);

  validate_event (2, "segment",
      "time", G_GUINT64_CONSTANT (0),
      "start", G_GUINT64_CONSTANT (0), "stop", G_MAXUINT64, NULL);

  destroy_depayloader (state);
}

GST_END_TEST
/* the intent with this test is to provide the depayloader with a buffer that
 * does not contain an RTP header. this makes it impossible for the depayloader
 * to depayload the incoming RTP packet, yet the stream-start and caps events
 * will still be pushed.
 */
GST_START_TEST (rtp_base_depayload_invalid_rtp_packet_test)
{
  State *state;

  state = create_depayloader ("application/x-rtp", NULL);

  set_state (state, GST_STATE_PLAYING);

  push_buffer (state,
      "pts", 0 * GST_SECOND, "offset", GST_BUFFER_OFFSET_NONE, NULL);

  set_state (state, GST_STATE_NULL);

  validate_buffers_received (0);

  validate_events_received (2);

  validate_event (0, "stream-start", NULL);

  validate_event (1, "caps", "media-type", "application/x-rtp", NULL);

  destroy_depayloader (state);
}

GST_END_TEST
/* validate what happens when a depayloader is provided with two RTP packets
 * sent after each other that do not have sequential sequence numbers. in this
 * case the depayloader should be able to depayload both first and the second
 * buffer, but the second buffer will have GST_BUFFER_FLAG_DISCONT set to
 * indicate that the was a discontinuity in the stream. the initial events are
 * pushed prior to the buffers arriving so they should be unaffected by the gap
 * in sequence numbers.
 */
GST_START_TEST (rtp_base_depayload_with_gap_test)
{
  State *state;

  state = create_depayloader ("application/x-rtp", NULL);

  set_state (state, GST_STATE_PLAYING);

  push_rtp_buffer (state,
      "pts", 0 * GST_SECOND,
      "rtptime", G_GUINT64_CONSTANT (0x43214321), "seq", 0x4242, NULL);

  push_rtp_buffer (state,
      "pts", 1 * GST_SECOND,
      "rtptime", G_GUINT64_CONSTANT (0x43214321) + 1 * DEFAULT_CLOCK_RATE,
      "seq", 0x4242 + 2, NULL);

  set_state (state, GST_STATE_NULL);

  validate_buffers_received (2);

  validate_buffer (0, "pts", 0 * GST_SECOND, "discont", FALSE, NULL);

  validate_buffer (1, "pts", 1 * GST_SECOND, "discont", TRUE, NULL);

  validate_events_received (3);

  validate_event (0, "stream-start", NULL);

  validate_event (1, "caps", "media-type", "application/x-rtp", NULL);

  validate_event (2, "segment",
      "time", G_GUINT64_CONSTANT (0),
      "start", G_GUINT64_CONSTANT (0), "stop", G_MAXUINT64, NULL);

  destroy_depayloader (state);
}

GST_END_TEST
/* two RTP packets are pushed in this test, and while the sequence numbers are
 * sequential they are reversed. the expectation is that the depayloader will be
 * able to depayload the first RTP packet, but once the second RTP packet
 * arrives it will be discarded because it arrived too late. the initial events
 * should be unaffected by the reversed buffers.
 */
GST_START_TEST (rtp_base_depayload_reversed_test)
{
  State *state;

  state = create_depayloader ("application/x-rtp", NULL);

  set_state (state, GST_STATE_PLAYING);

  push_rtp_buffer (state,
      "pts", 0 * GST_SECOND,
      "rtptime", G_GUINT64_CONSTANT (0x43214321), "seq", 0x4242, NULL);

  push_rtp_buffer (state,
      "pts", 1 * GST_SECOND,
      "rtptime", G_GUINT64_CONSTANT (0x43214321) + 1 * DEFAULT_CLOCK_RATE,
      "seq", 0x4242 - 1, NULL);

  set_state (state, GST_STATE_NULL);

  validate_buffers_received (1);

  validate_buffer (0, "pts", 0 * GST_SECOND, "discont", FALSE, NULL);

  validate_events_received (3);

  validate_event (0, "stream-start", NULL);

  validate_event (1, "caps", "media-type", "application/x-rtp", NULL);

  validate_event (2, "segment",
      "time", G_GUINT64_CONSTANT (0),
      "start", G_GUINT64_CONSTANT (0), "stop", G_MAXUINT64, NULL);

  destroy_depayloader (state);
}

GST_END_TEST
/* The same scenario as in rtp_base_depayload_reversed_test
 * except that SSRC is changed for the 2nd packet that is why
 * it should not be discarded.
 */
GST_START_TEST (rtp_base_depayload_ssrc_changed_test)
{
  State *state;

  state = create_depayloader ("application/x-rtp", NULL);

  set_state (state, GST_STATE_PLAYING);

  push_rtp_buffer (state,
      "pts", 0 * GST_SECOND,
      "rtptime", G_GUINT64_CONSTANT (0x43214321),
      "seq", 0x4242, "ssrc", 0xabe2b0b, NULL);

  push_rtp_buffer (state,
      "pts", 1 * GST_SECOND,
      "rtptime", G_GUINT64_CONSTANT (0x43214321) + 1 * DEFAULT_CLOCK_RATE,
      "seq", 0x4242 - 1, "ssrc", 0xcafebabe, NULL);

  set_state (state, GST_STATE_NULL);

  validate_buffers_received (2);

  validate_buffer (0, "pts", 0 * GST_SECOND, "discont", FALSE, NULL);

  validate_buffer (1, "pts", 1 * GST_SECOND, "discont", TRUE, NULL);

  validate_events_received (3);

  validate_event (0, "stream-start", NULL);

  validate_event (1, "caps", "media-type", "application/x-rtp", NULL);

  validate_event (2, "segment",
      "time", G_GUINT64_CONSTANT (0),
      "start", G_GUINT64_CONSTANT (0), "stop", G_MAXUINT64, NULL);

  destroy_depayloader (state);
}

GST_END_TEST
/* the intent of this test is to push two RTP packets that have reverse sequence
 * numbers that differ significantly. the depayloader will consider RTP packets
 * where the sequence numbers differ by more than 1000 to indicate that the
 * source of the RTP packets has been restarted. therefore it will let both
 * depayloaded buffers through, but the latter buffer marked
 * GST_BUFFER_FLAG_DISCONT to indicate the discontinuity in the stream. the
 * initial events should be unaffected by the reversed buffers.
 */
GST_START_TEST (rtp_base_depayload_old_reversed_test)
{
  State *state;

  state = create_depayloader ("application/x-rtp", NULL);

  set_state (state, GST_STATE_PLAYING);

  push_rtp_buffer (state,
      "pts", 0 * GST_SECOND,
      "rtptime", G_GUINT64_CONSTANT (0x43214321), "seq", 0x4242, NULL);

  push_rtp_buffer (state,
      "pts", 1 * GST_SECOND,
      "rtptime", G_GUINT64_CONSTANT (0x43214321) + 1 * DEFAULT_CLOCK_RATE,
      "seq", 0x4242 - 1000, NULL);

  set_state (state, GST_STATE_NULL);

  validate_buffers_received (2);

  validate_buffer (0, "pts", 0 * GST_SECOND, "discont", FALSE, NULL);

  validate_buffer (1, "pts", 1 * GST_SECOND, "discont", TRUE, NULL);

  validate_events_received (3);

  validate_event (0, "stream-start", NULL);

  validate_event (1, "caps", "media-type", "application/x-rtp", NULL);

  validate_event (2, "segment",
      "time", G_GUINT64_CONSTANT (0),
      "start", G_GUINT64_CONSTANT (0), "stop", G_MAXUINT64, NULL);

  destroy_depayloader (state);
}

GST_END_TEST
/* a depayloader that has not received any caps event will not be able to
 * process any incoming RTP packet. instead pushing an RTP packet should result
 * in the expected error.
 */
GST_START_TEST (rtp_base_depayload_without_negotiation_test)
{
  State *state;

  state = create_depayloader (NULL, NULL);

  set_state (state, GST_STATE_PLAYING);

  push_rtp_buffer_fails (state, GST_FLOW_NOT_NEGOTIATED,
      "pts", 0 * GST_SECOND,
      "rtptime", G_GUINT64_CONSTANT (0x1234), "seq", 0x4242, NULL);

  set_state (state, GST_STATE_NULL);

  validate_buffers_received (0);

  validate_events_received (1);

  validate_event (0, "stream-start", NULL);

  destroy_depayloader (state);
}

GST_END_TEST
/* a depayloader that receives the downstream event GstRTPPacketLost should
 * respond by emitting a gap event with the corresponding timestamp and
 * duration. the initial events are unaffected, but are succeeded by the added
 * gap event.
 */
GST_START_TEST (rtp_base_depayload_packet_lost_test)
{
  State *state;

  state = create_depayloader ("application/x-rtp", NULL);

  set_state (state, GST_STATE_PLAYING);

  push_rtp_buffer (state,
      "pts", 0 * GST_SECOND,
      "rtptime", G_GUINT64_CONSTANT (0x1234), "seq", 0x4242, NULL);

  packet_lost (state, 1 * GST_SECOND, GST_SECOND, FALSE);

  /* If a packet was lost but we don't know whether it was a FEC packet,
   * the depayloader should not generate gap events */
  packet_lost (state, 2 * GST_SECOND, GST_SECOND, TRUE);

  push_rtp_buffer (state,
      "pts", 2 * GST_SECOND,
      "rtptime", G_GUINT64_CONSTANT (0x1234) + 2 * DEFAULT_CLOCK_RATE,
      "seq", 0x4242 + 2, NULL);

  set_state (state, GST_STATE_NULL);

  validate_buffers_received (2);

  validate_buffer (0, "pts", 0 * GST_SECOND, "discont", FALSE, NULL);

  validate_buffer (1, "pts", 2 * GST_SECOND, "discont", TRUE, NULL);

  validate_events_received (4);

  validate_event (0, "stream-start", NULL);

  validate_event (1, "caps", "media-type", "application/x-rtp", NULL);

  validate_event (2, "segment",
      "time", G_GUINT64_CONSTANT (0),
      "start", G_GUINT64_CONSTANT (0), "stop", G_MAXUINT64, NULL);

  validate_event (3, "gap",
      "timestamp", 1 * GST_SECOND, "duration", GST_SECOND, NULL);

  destroy_depayloader (state);
}

GST_END_TEST
/* If a lost event is received before the first buffer, the rtp base
 * depayloader will not send a gap event downstream. Alternatively it should
 * make sure that stream-start, caps and segment events are sent in correct
 * order before the gap event so that packet loss concealment can take place
 * downstream, but this is more complicated and without any real benefit since
 * concealment before any data is received is not very useful. */
GST_START_TEST (rtp_base_depayload_packet_lost_before_first_buffer_test)
{
  GstHarness *h;
  GstEvent *event;
  GstRtpDummyDepay *depay;
  const GstEventType etype[] = {
    GST_EVENT_STREAM_START, GST_EVENT_CAPS, GST_EVENT_SEGMENT
  };
  gint i;

  depay = rtp_dummy_depay_new ();
  h = gst_harness_new_with_element (GST_ELEMENT_CAST (depay), "sink", "src");
  gst_harness_set_src_caps_str (h, "application/x-rtp");

  /* Verify that depayloader has received setup events */
  for (i = 0; i < 3; i++) {
    event = gst_pad_get_sticky_event (h->srcpad, etype[i], 0);
    fail_unless (event != NULL);
    gst_event_unref (event);
  }

  /* Send loss event to depayloader */
  gst_harness_push_event (h, gst_event_new_custom (GST_EVENT_CUSTOM_DOWNSTREAM,
          gst_structure_new ("GstRTPPacketLost",
              "seqnum", G_TYPE_UINT, (guint) 0,
              "timestamp", G_TYPE_UINT64, (guint64) 0,
              "duration", G_TYPE_UINT64, (guint64) 10 * GST_MSECOND, NULL)));

  /* When a buffer is pushed, an updated (and more accurate) segment event
   * should also be sent. */
  gst_harness_push (h, gst_rtp_buffer_new_allocate (0, 0, 0));

  /* Verify that setup events are sent before gap event */
  for (i = 0; i < 3; i++) {
    fail_unless (event = gst_harness_pull_event (h));
    fail_unless_equals_int (GST_EVENT_TYPE (event), etype[i]);
    gst_event_unref (event);
  }
  fail_unless_equals_int (gst_harness_events_in_queue (h), 0);

  gst_buffer_unref (gst_harness_pull (h));
  fail_unless_equals_int (gst_harness_buffers_in_queue (h), 0);

  g_object_unref (depay);
  gst_harness_teardown (h);
}

GST_END_TEST;
/* rtp base depayloader should set DISCONT flag on buffer in case of a large
 * sequence number gap, and it's not set already by upstream. This tests a
 * certain code path where the buffer needs to be made writable to set the
 * DISCONT flag.
 */
GST_START_TEST (rtp_base_depayload_seq_discont_test)
{
  State *state;

  state = create_depayloader ("application/x-rtp", NULL);

  set_state (state, GST_STATE_PLAYING);

  push_rtp_buffer (state,
      "pts", 0 * GST_SECOND,
      "rtptime", G_GUINT64_CONSTANT (0x1234), "seq", 1, NULL);

  push_rtp_buffer (state,
      "extra-ref", TRUE,
      "pts", 2 * GST_SECOND,
      "rtptime", G_GUINT64_CONSTANT (0x1234) + DEFAULT_CLOCK_RATE / 2,
      "seq", 33333, NULL);

  set_state (state, GST_STATE_NULL);

  validate_buffers_received (2);

  validate_buffer (0, "pts", 0 * GST_SECOND, "discont", FALSE, NULL);

  validate_buffer (1, "pts", 2 * GST_SECOND, "discont", TRUE, NULL);

  destroy_depayloader (state);
}

GST_END_TEST
/* a depayloader that receives identical caps events simply ignores the latter
 * events without propagating them downstream.
 */
GST_START_TEST (rtp_base_depayload_repeated_caps_test)
{
  State *state;

  state = create_depayloader ("application/x-rtp", NULL);

  set_state (state, GST_STATE_PLAYING);

  push_rtp_buffer (state,
      "pts", 0 * GST_SECOND,
      "rtptime", G_GUINT64_CONSTANT (0x1234), "seq", 0x4242, NULL);

  reconfigure_caps (state, "application/x-rtp");

  push_rtp_buffer (state,
      "pts", 1 * GST_SECOND,
      "rtptime", G_GUINT64_CONSTANT (0x1234) + 1 * DEFAULT_CLOCK_RATE,
      "seq", 0x4242 + 1, NULL);

  set_state (state, GST_STATE_NULL);

  validate_buffers_received (2);

  validate_buffer (0, "pts", 0 * GST_SECOND, "discont", FALSE, NULL);

  validate_buffer (1, "pts", 1 * GST_SECOND, "discont", FALSE, NULL);

  validate_events_received (3);

  validate_event (0, "stream-start", NULL);

  validate_event (1, "caps", "media-type", "application/x-rtp", NULL);

  validate_event (2, "segment",
      "time", G_GUINT64_CONSTANT (0),
      "start", G_GUINT64_CONSTANT (0), "stop", G_MAXUINT64, NULL);

  destroy_depayloader (state);
}

GST_END_TEST
/* when a depayloader receives new caps events with npt-start and npt-stop times
 * it should save these timestamps as they should affect the next segment event
 * being pushed by the depayloader. a new segment event is not pushed by the
 * depayloader until a flush_stop event and a succeeding segment event are
 * received. of course the initial event are unaffected, as is the incoming caps
 * event.
 */
GST_START_TEST (rtp_base_depayload_npt_test)
{
  State *state;

  state = create_depayloader ("application/x-rtp", NULL);

  set_state (state, GST_STATE_PLAYING);

  push_rtp_buffer (state,
      "pts", 0 * GST_SECOND,
      "rtptime", G_GUINT64_CONSTANT (0x1234), "seq", 0x4242, NULL);

  reconfigure_caps (state,
      "application/x-rtp, npt-start=(guint64)1234, npt-stop=(guint64)4321");

  flush_pipeline (state);

  push_rtp_buffer (state,
      "pts", 1 * GST_SECOND,
      "rtptime", G_GUINT64_CONSTANT (0x1234) + 1 * DEFAULT_CLOCK_RATE,
      "seq", 0x4242 + 1, NULL);

  set_state (state, GST_STATE_NULL);

  validate_buffers_received (2);

  validate_buffer (0, "pts", 0 * GST_SECOND, "discont", FALSE, NULL);

  validate_buffer (1, "pts", 1 * GST_SECOND, "discont", FALSE, NULL);

  validate_events_received (7);

  validate_event (0, "stream-start", NULL);

  validate_event (1, "caps", "media-type", "application/x-rtp", NULL);

  validate_event (2, "segment",
      "time", G_GUINT64_CONSTANT (0),
      "start", G_GUINT64_CONSTANT (0), "stop", G_MAXUINT64, NULL);

  validate_event (3, "caps",
      "media-type", "application/x-rtp",
      "npt-start", G_GUINT64_CONSTANT (1234),
      "npt-stop", G_GUINT64_CONSTANT (4321), NULL);

  validate_event (4, "flush-start", NULL);

  validate_event (5, "flush-stop", NULL);

  validate_event (6, "segment",
      "time", G_GUINT64_CONSTANT (1234),
      "start", G_GUINT64_CONSTANT (0),
      "stop", G_GUINT64_CONSTANT (4321 - 1234), NULL);

  destroy_depayloader (state);
}

GST_END_TEST
/* when a depayloader receives a new caps event with play-scale it should save
 * this rate as it should affect the next segment event being pushed by the
 * depayloader. a new segment event is not pushed by the depayloader until a
 * flush_stop event and a succeeding segment event are received. of course the
 * initial event are unaffected, as is the incoming caps event.
 */
GST_START_TEST (rtp_base_depayload_play_scale_test)
{
  State *state;

  state = create_depayloader ("application/x-rtp", NULL);

  set_state (state, GST_STATE_PLAYING);

  push_rtp_buffer (state,
      "pts", 0 * GST_SECOND,
      "rtptime", G_GUINT64_CONSTANT (0x1234), "seq", 0x4242, NULL);

  reconfigure_caps (state, "application/x-rtp, play-scale=(double)2.0");

  flush_pipeline (state);

  push_rtp_buffer (state,
      "pts", 1 * GST_SECOND,
      "rtptime", G_GUINT64_CONSTANT (0x1234) + 1 * DEFAULT_CLOCK_RATE,
      "seq", 0x4242 + 1, NULL);

  set_state (state, GST_STATE_NULL);

  validate_buffers_received (2);

  validate_buffer (0, "pts", 0 * GST_SECOND, "discont", FALSE, NULL);

  validate_buffer (1, "pts", 1 * GST_SECOND, "discont", FALSE, NULL);

  validate_events_received (7);

  validate_event (0, "stream-start", NULL);

  validate_event (1, "caps", "media-type", "application/x-rtp", NULL);

  validate_event (2, "segment",
      "time", G_GUINT64_CONSTANT (0),
      "start", G_GUINT64_CONSTANT (0), "stop", G_MAXUINT64, NULL);

  validate_event (3, "caps",
      "media-type", "application/x-rtp", "play-scale", 2.0, NULL);

  validate_event (4, "flush-start", NULL);

  validate_event (5, "flush-stop", NULL);

  validate_event (6, "segment",
      "time", G_GUINT64_CONSTANT (0),
      "start", G_GUINT64_CONSTANT (0),
      "stop", G_MAXUINT64, "rate", 1.0, "applied-rate", 2.0, NULL);

  destroy_depayloader (state);
}

GST_END_TEST
/* when a depayloader receives a new caps event with play-speed it should save
 * this rate as it should affect the next segment event being pushed by the
 * depayloader. a new segment event is not pushed by the depayloader until a
 * flush_stop event and a succeeding segment event are received. of course the
 * initial event are unaffected, as is the incoming caps event.
 */
GST_START_TEST (rtp_base_depayload_play_speed_test)
{
  State *state;

  state = create_depayloader ("application/x-rtp", NULL);

  set_state (state, GST_STATE_PLAYING);

  push_rtp_buffer (state,
      "pts", 0 * GST_SECOND,
      "rtptime", G_GUINT64_CONSTANT (0x1234), "seq", 0x4242, NULL);

  reconfigure_caps (state, "application/x-rtp, play-speed=(double)2.0");

  flush_pipeline (state);

  push_rtp_buffer (state,
      "pts", 1 * GST_SECOND,
      "rtptime", G_GUINT64_CONSTANT (0x1234) + 1 * DEFAULT_CLOCK_RATE,
      "seq", 0x4242 + 1, NULL);

  set_state (state, GST_STATE_NULL);

  validate_buffers_received (2);

  validate_buffer (0, "pts", 0 * GST_SECOND, "discont", FALSE, NULL);

  validate_buffer (1, "pts", 1 * GST_SECOND, "discont", FALSE, NULL);

  validate_events_received (7);

  validate_event (0, "stream-start", NULL);

  validate_event (1, "caps", "media-type", "application/x-rtp", NULL);

  validate_event (2, "segment",
      "time", G_GUINT64_CONSTANT (0),
      "start", G_GUINT64_CONSTANT (0), "stop", G_MAXUINT64, NULL);

  validate_event (3, "caps",
      "media-type", "application/x-rtp", "play-speed", 2.0, NULL);

  validate_event (4, "flush-start", NULL);

  validate_event (5, "flush-stop", NULL);

  validate_event (6, "segment",
      "time", G_GUINT64_CONSTANT (0),
      "start", G_GUINT64_CONSTANT (0),
      "stop", G_MAXUINT64, "rate", 2.0, "applied-rate", 1.0, NULL);

  destroy_depayloader (state);
}

GST_END_TEST
/* when a depayloader receives new caps events with npt-start, npt-stop and
 * clock-base it should save these timestamps as they should affect the next
 * segment event being pushed by the depayloader. the produced segment should
 * make the position of the stream reflect the position from clock-base instead
 * of reflecting the running time (for RTSP).
 */
GST_START_TEST (rtp_base_depayload_clock_base_test)
{
  State *state;

  state = create_depayloader ("application/x-rtp", NULL);

  set_state (state, GST_STATE_PLAYING);

  push_rtp_buffer (state,
      "pts", 0 * GST_SECOND,
      "rtptime", G_GUINT64_CONSTANT (1234), "seq", 0x4242, NULL);

  reconfigure_caps (state,
      "application/x-rtp, npt-start=(guint64)1234, npt-stop=(guint64)4321, clock-base=(guint)1234");

  flush_pipeline (state);

  push_rtp_buffer (state,
      "pts", 1 * GST_SECOND,
      "rtptime", G_GUINT64_CONSTANT (1234) + 1 * DEFAULT_CLOCK_RATE,
      "seq", 0x4242 + 1, NULL);

  set_state (state, GST_STATE_NULL);

  validate_buffers_received (2);

  validate_buffer (0, "pts", 0 * GST_SECOND, "discont", FALSE, NULL);

  validate_buffer (1, "pts", 1 * GST_SECOND, "discont", FALSE, NULL);

  validate_events_received (7);

  validate_event (0, "stream-start", NULL);

  validate_event (1, "caps", "media-type", "application/x-rtp", NULL);

  validate_event (2, "segment",
      "time", G_GUINT64_CONSTANT (0),
      "start", G_GUINT64_CONSTANT (0), "stop", G_MAXUINT64, NULL);

  validate_event (3, "caps",
      "media-type", "application/x-rtp",
      "npt-start", G_GUINT64_CONSTANT (1234),
      "npt-stop", G_GUINT64_CONSTANT (4321), "clock-base", 1234, NULL);

  validate_event (4, "flush-start", NULL);

  validate_event (5, "flush-stop", NULL);

  validate_event (6, "segment",
      "time", G_GUINT64_CONSTANT (1234),
      "start", GST_SECOND,
      "stop", GST_SECOND + G_GUINT64_CONSTANT (4321 - 1234),
      "base", GST_SECOND, NULL);

  destroy_depayloader (state);
}

GST_END_TEST
/* basedepayloader has a property source-info that will add
 * GstRTPSourceMeta to the output buffer with RTP source information, such as
 * SSRC and CSRCs. The is useful for letting downstream know about the origin
 * of the stream. */
GST_START_TEST (rtp_base_depayload_source_info_test)
{
  GstHarness *h;
  GstRtpDummyDepay *depay;
  GstBuffer *buffer;
  GstRTPSourceMeta *meta;
  guint seq = 0;

  depay = rtp_dummy_depay_new ();
  h = gst_harness_new_with_element (GST_ELEMENT_CAST (depay), "sink", "src");
  gst_harness_set_src_caps_str (h, "application/x-rtp");

  /* Property enabled should always add meta, also when there is only SSRC and
   * no CSRC. */
  g_object_set (depay, "source-info", TRUE, NULL);
  buffer = gst_rtp_buffer_new_allocate (0, 0, 0);
  rtp_buffer_set (buffer, "seq", seq++, "ssrc", 0x11, NULL);
  buffer = gst_harness_push_and_pull (h, buffer);
  fail_unless ((meta = gst_buffer_get_rtp_source_meta (buffer)));
  fail_unless (meta->ssrc_valid);
  fail_unless_equals_int (meta->ssrc, 0x11);
  fail_unless_equals_int (meta->csrc_count, 0);
  gst_buffer_unref (buffer);

  /* Both SSRC and CSRC should be added to the meta */
  buffer = gst_rtp_buffer_new_allocate (0, 0, 2);
  rtp_buffer_set (buffer, "seq", seq++, "ssrc", 0x11, "csrc", 0, 0x22,
      "csrc", 1, 0x33, NULL);
  buffer = gst_harness_push_and_pull (h, buffer);
  fail_unless ((meta = gst_buffer_get_rtp_source_meta (buffer)));
  fail_unless (meta->ssrc_valid);
  fail_unless_equals_int (meta->ssrc, 0x11);
  fail_unless_equals_int (meta->csrc_count, 2);
  fail_unless_equals_int (meta->csrc[0], 0x22);
  fail_unless_equals_int (meta->csrc[1], 0x33);
  gst_buffer_unref (buffer);

  /* Property disabled should never add meta */
  g_object_set (depay, "source-info", FALSE, NULL);
  buffer = gst_rtp_buffer_new_allocate (0, 0, 0);
  rtp_buffer_set (buffer, "seq", seq++, "ssrc", 0x11, NULL);
  buffer = gst_harness_push_and_pull (h, buffer);
  fail_if (gst_buffer_get_rtp_source_meta (buffer));
  gst_buffer_unref (buffer);

  g_object_unref (depay);
  gst_harness_teardown (h);
}

GST_END_TEST;

/* verify that if a buffer arriving in the depayloader already has source-info
   meta on it, that this does not affect the source-info coming out of the
   depayloder, which should be all derived from the rtp-header */
GST_START_TEST (rtp_base_depayload_source_info_from_rtp_only)
{
  GstHarness *h;
  GstRtpDummyDepay *depay;
  GstBuffer *buffer;
  GstRTPSourceMeta *meta;
  guint rtp_ssrc = 0x11;
  guint rtp_csrc = 0x22;
  guint32 meta_ssrc = 0x55;
  guint32 meta_csrc = 0x66;

  depay = rtp_dummy_depay_new ();
  h = gst_harness_new_with_element (GST_ELEMENT_CAST (depay), "sink", "src");
  gst_harness_set_src_caps_str (h, "application/x-rtp");

  g_object_set (depay, "source-info", TRUE, NULL);
  buffer = gst_rtp_buffer_new_allocate (0, 0, 1);
  rtp_buffer_set (buffer, "seq", 0, "ssrc", rtp_ssrc, "csrc", 0, rtp_csrc,
      NULL);
  meta = gst_buffer_add_rtp_source_meta (buffer, &meta_ssrc, &meta_csrc, 1);

  buffer = gst_harness_push_and_pull (h, buffer);
  fail_unless ((meta = gst_buffer_get_rtp_source_meta (buffer)));
  fail_unless (meta->ssrc_valid);
  fail_unless_equals_int (meta->ssrc, rtp_ssrc);
  fail_unless_equals_int (meta->csrc_count, 1);
  fail_unless_equals_int (meta->csrc[0], rtp_csrc);
  gst_buffer_unref (buffer);

  g_object_unref (depay);
  gst_harness_teardown (h);
}

GST_END_TEST;

/* Test max-reorder property. Reordered packets with a gap less than
 * max-reordered will be dropped, reordered packets with gap larger than
 * max-reorder is considered coming fra a restarted sender and should not be
 * dropped. */
GST_START_TEST (rtp_base_depayload_max_reorder)
{
  GstHarness *h;
  GstRtpDummyDepay *depay;
  guint seq = 1000;

  depay = rtp_dummy_depay_new ();
  h = gst_harness_new_with_element (GST_ELEMENT_CAST (depay), "sink", "src");
  gst_harness_set_src_caps_str (h, "application/x-rtp");

#define PUSH_AND_CHECK(seqnum, pushed) G_STMT_START {                   \
    GstBuffer *buffer = gst_rtp_buffer_new_allocate (0, 0, 0);          \
    rtp_buffer_set (buffer, "seq", seqnum, "ssrc", 0x11, NULL);         \
    fail_unless_equals_int (GST_FLOW_OK, gst_harness_push (h, buffer)); \
    fail_unless_equals_int (gst_harness_buffers_in_queue (h), pushed);  \
    if (pushed)                                                         \
      gst_buffer_unref (gst_harness_pull (h));                          \
  } G_STMT_END;

  /* By default some reordering is accepted. Old seqnums should be
   * dropped, but not too old */
  PUSH_AND_CHECK (seq, TRUE);
  PUSH_AND_CHECK (seq - 50, FALSE);
  PUSH_AND_CHECK (seq - 100, TRUE);

  /* Update property to allow less reordering */
  g_object_set (depay, "max-reorder", 3, NULL);

  /* Gaps up to max allowed reordering is dropped. */
  PUSH_AND_CHECK (seq, TRUE);
  PUSH_AND_CHECK (seq - 2, FALSE);
  PUSH_AND_CHECK (seq - 3, TRUE);

  /* After a push the initial state should be reset, so a duplicate of the
   * last packet should be dropped */
  PUSH_AND_CHECK (seq - 3, FALSE);

  /* Update property to minimum value. Should never drop buffers. */
  g_object_set (depay, "max-reorder", 0, NULL);

  /* Duplicate buffer should now be pushed. */
  PUSH_AND_CHECK (seq, TRUE);
  PUSH_AND_CHECK (seq, TRUE);

  g_object_unref (depay);
  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (rtp_base_depayload_flow_return_push_func)
{
  State *state;

  state = create_depayloader ("application/x-rtp", NULL);

  GST_RTP_DUMMY_DEPAY (state->element)->push_method =
      GST_RTP_DUMMY_USE_PUSH_LIST_FUNC;

  set_state (state, GST_STATE_PLAYING);

  GST_PAD_SET_FLUSHING (state->sinkpad);

  push_rtp_buffer_fails (state, GST_FLOW_FLUSHING,
      "pts", 0 * GST_SECOND,
      "rtptime", G_GUINT64_CONSTANT (0x1234), "seq", 0x4242, NULL);

  set_state (state, GST_STATE_NULL);

  destroy_depayloader (state);
}

GST_END_TEST;

GST_START_TEST (rtp_base_depayload_flow_return_push_list_func)
{
  State *state;

  state = create_depayloader ("application/x-rtp", NULL);

  GST_RTP_DUMMY_DEPAY (state->element)->push_method =
      GST_RTP_DUMMY_USE_PUSH_FUNC;

  set_state (state, GST_STATE_PLAYING);

  GST_PAD_SET_FLUSHING (state->sinkpad);

  push_rtp_buffer_fails (state, GST_FLOW_FLUSHING,
      "pts", 0 * GST_SECOND,
      "rtptime", G_GUINT64_CONSTANT (0x1234), "seq", 0x4242, NULL);

  set_state (state, GST_STATE_NULL);

  destroy_depayloader (state);
}

GST_END_TEST;

GST_START_TEST (rtp_base_depayload_one_byte_hdr_ext)
{
  GstRTPHeaderExtension *ext;
  State *state;

  state = create_depayloader ("application/x-rtp", NULL);
  ext = rtp_dummy_hdr_ext_new ();
  gst_rtp_header_extension_set_id (ext, 1);

  GST_RTP_DUMMY_DEPAY (state->element)->push_method =
      GST_RTP_DUMMY_RETURN_TO_PUSH;

  g_signal_emit_by_name (state->element, "add-extension", ext);

  set_state (state, GST_STATE_PLAYING);

  push_rtp_buffer (state, "pts", 0 * GST_SECOND,
      "rtptime", G_GUINT64_CONSTANT (0x1234), "seq", 0x4242, "hdrext-1", ext,
      NULL);

  set_state (state, GST_STATE_NULL);

  validate_buffers_received (1);

  validate_buffer (0, "pts", 0 * GST_SECOND, "discont", FALSE, NULL);

  fail_unless_equals_int (GST_RTP_DUMMY_HDR_EXT (ext)->read_count, 1);

  gst_object_unref (ext);
  destroy_depayloader (state);
}

GST_END_TEST;

GST_START_TEST (rtp_base_depayload_two_byte_hdr_ext)
{
  GstRTPHeaderExtension *ext;
  State *state;

  state = create_depayloader ("application/x-rtp", NULL);
  ext = rtp_dummy_hdr_ext_new ();
  gst_rtp_header_extension_set_id (ext, 1);

  GST_RTP_DUMMY_DEPAY (state->element)->push_method =
      GST_RTP_DUMMY_RETURN_TO_PUSH;

  g_signal_emit_by_name (state->element, "add-extension", ext);

  set_state (state, GST_STATE_PLAYING);

  push_rtp_buffer (state, "pts", 0 * GST_SECOND,
      "rtptime", G_GUINT64_CONSTANT (0x1234), "seq", 0x4242, "hdrext-2", ext,
      NULL);

  set_state (state, GST_STATE_NULL);

  validate_buffers_received (1);

  validate_buffer (0, "pts", 0 * GST_SECOND, "discont", FALSE, NULL);

  fail_unless_equals_int (GST_RTP_DUMMY_HDR_EXT (ext)->read_count, 1);

  gst_object_unref (ext);
  destroy_depayloader (state);
}

GST_END_TEST;

static GstRTPHeaderExtension *
request_extension (GstRTPBaseDepayload * depayload, guint ext_id,
    const gchar * ext_uri, gpointer user_data)
{
  GstRTPHeaderExtension *ext = user_data;

  if (ext && gst_rtp_header_extension_get_id (ext) == ext_id
      && g_strcmp0 (ext_uri, gst_rtp_header_extension_get_uri (ext)) == 0)
    return gst_object_ref (ext);

  return NULL;
}

GST_START_TEST (rtp_base_depayload_request_extension)
{
  GstRTPHeaderExtension *ext;
  GstRTPDummyHdrExt *dummy;
  State *state;

  state =
      create_depayloader ("application/x-rtp,extmap-3=(string)"
      DUMMY_HDR_EXT_URI, NULL);
  ext = rtp_dummy_hdr_ext_new ();
  dummy = GST_RTP_DUMMY_HDR_EXT (ext);
  gst_rtp_header_extension_set_id (ext, 3);

  GST_RTP_DUMMY_DEPAY (state->element)->push_method =
      GST_RTP_DUMMY_RETURN_TO_PUSH;

  g_signal_connect (state->element, "request-extension",
      G_CALLBACK (request_extension), ext);

  fail_unless (dummy->set_attributes_count == 0);

  set_state (state, GST_STATE_PLAYING);

  push_rtp_buffer (state, "pts", 0 * GST_SECOND,
      "rtptime", G_GUINT64_CONSTANT (0x1234), "seq", 0x4242, "hdrext-1", ext,
      NULL);

  set_state (state, GST_STATE_NULL);

  validate_buffers_received (1);

  validate_buffer (0, "pts", 0 * GST_SECOND, "discont", FALSE, NULL);

  fail_unless_equals_int (GST_RTP_DUMMY_HDR_EXT (ext)->read_count, 1);
  fail_unless (dummy->set_attributes_count == 1);

  gst_object_unref (ext);
  destroy_depayloader (state);
}

GST_END_TEST;

GST_START_TEST (rtp_base_depayload_clear_extensions)
{
  GstRTPHeaderExtension *ext;
  State *state;

  state = create_depayloader ("application/x-rtp", NULL);
  ext = rtp_dummy_hdr_ext_new ();
  gst_rtp_header_extension_set_id (ext, 1);

  GST_RTP_DUMMY_DEPAY (state->element)->push_method =
      GST_RTP_DUMMY_RETURN_TO_PUSH;

  g_signal_emit_by_name (state->element, "add-extension", ext);

  set_state (state, GST_STATE_PLAYING);

  push_rtp_buffer (state, "pts", 0 * GST_SECOND,
      "rtptime", G_GUINT64_CONSTANT (0x1234), "seq", 0x4242, "hdrext-1", ext,
      NULL);

  g_signal_emit_by_name (state->element, "clear-extensions");

  push_rtp_buffer (state, "pts", 1 * GST_SECOND,
      "rtptime", G_GUINT64_CONSTANT (0x1234) + 1 * DEFAULT_CLOCK_RATE,
      "seq", 0x4242 + 1, "hdrext-1", ext, NULL);

  set_state (state, GST_STATE_NULL);

  validate_buffers_received (2);

  validate_buffer (0, "pts", 0 * GST_SECOND, "discont", FALSE, NULL);
  validate_buffer (1, "pts", 1 * GST_SECOND, "discont", FALSE, NULL);

  fail_unless_equals_int (GST_RTP_DUMMY_HDR_EXT (ext)->read_count, 1);

  gst_object_unref (ext);
  destroy_depayloader (state);
}

GST_END_TEST;

GST_START_TEST (rtp_base_depayload_multiple_exts)
{
  GstRTPHeaderExtension *ext1;
  GstRTPHeaderExtension *ext2;
  State *state;

  state = create_depayloader ("application/x-rtp", NULL);
  ext1 = rtp_dummy_hdr_ext_new ();
  gst_rtp_header_extension_set_id (ext1, 1);
  ext2 = rtp_dummy_hdr_ext_new ();
  gst_rtp_header_extension_set_id (ext2, 2);

  GST_RTP_DUMMY_DEPAY (state->element)->push_method =
      GST_RTP_DUMMY_RETURN_TO_PUSH;

  g_signal_emit_by_name (state->element, "add-extension", ext1);
  g_signal_emit_by_name (state->element, "add-extension", ext2);

  set_state (state, GST_STATE_PLAYING);

  push_rtp_buffer (state, "pts", 0 * GST_SECOND,
      "rtptime", G_GUINT64_CONSTANT (0x1234), "seq", 0x4242, "hdrext-1", ext1,
      "hdrext-1", ext2, NULL);

  set_state (state, GST_STATE_NULL);

  validate_buffers_received (1);

  validate_buffer (0, "pts", 0 * GST_SECOND, "discont", FALSE, NULL);

  fail_unless_equals_int (GST_RTP_DUMMY_HDR_EXT (ext1)->read_count, 1);
  fail_unless_equals_int (GST_RTP_DUMMY_HDR_EXT (ext2)->read_count, 1);

  gst_object_unref (ext1);
  gst_object_unref (ext2);
  destroy_depayloader (state);
}

GST_END_TEST;

static GstRTPHeaderExtension *
request_extension_ignored (GstRTPBaseDepayload * depayload, guint ext_id,
    const gchar * ext_uri, gpointer user_data)
{
  guint *request_counter = user_data;

  *request_counter += 1;

  return NULL;
}

GST_START_TEST (rtp_base_depayload_caps_request_ignored)
{
  State *state;
  guint request_counter = 0;

  state =
      create_depayloader ("application/x-rtp,extmap-3=(string)"
      DUMMY_HDR_EXT_URI, NULL);

  GST_RTP_DUMMY_DEPAY (state->element)->push_method =
      GST_RTP_DUMMY_RETURN_TO_PUSH;
  g_signal_connect (state->element, "request-extension",
      G_CALLBACK (request_extension_ignored), &request_counter);

  set_state (state, GST_STATE_PLAYING);

  push_rtp_buffer (state,
      "pts", 0 * GST_SECOND,
      "rtptime", G_GUINT64_CONSTANT (0x1234), "seq", 0x4242, NULL);

  fail_unless_equals_int (request_counter, 1);

  set_state (state, GST_STATE_NULL);

  validate_buffers_received (1);

  destroy_depayloader (state);
}

GST_END_TEST;

static GstFlowReturn
hdr_ext_caps_change_chain_func (GstPad * pad, GstObject * parent,
    GstBuffer * buffer)
{
  GstFlowReturn res;
  GstCaps *caps;
  guint val;
  static guint expected_caps_val = 0;

  res = gst_check_chain_func (pad, parent, buffer);
  if (res != GST_FLOW_OK) {
    return res;
  }

  caps = gst_pad_get_current_caps (pad);

  fail_unless (gst_structure_get_uint (gst_caps_get_structure (caps, 0),
          "dummy-hdrext-val", &val));

  /* Every fifth buffer increments "dummy-hdrext-val". */
  if (g_list_length (buffers) % 5 == 1) {
    expected_caps_val++;
  }

  fail_unless_equals_int (expected_caps_val, val);

  gst_caps_unref (caps);

  return res;
}

GST_START_TEST (rtp_base_depayload_hdr_ext_caps_change)
{
  GstRTPHeaderExtension *ext;
  State *state;

  state = create_depayloader ("application/x-rtp", NULL);
  gst_pad_set_chain_function (state->sinkpad, hdr_ext_caps_change_chain_func);

  ext = rtp_dummy_hdr_ext_new ();
  gst_rtp_header_extension_set_id (ext, 1);

  GST_RTP_DUMMY_DEPAY (state->element)->push_method =
      GST_RTP_DUMMY_USE_PUSH_LIST_FUNC;
  GST_RTP_DUMMY_DEPAY (state->element)->num_buffers_in_blist = 15;

  g_signal_emit_by_name (state->element, "add-extension", ext);

  set_state (state, GST_STATE_PLAYING);

  push_rtp_buffer (state, "pts", 0 * GST_SECOND,
      "rtptime", G_GUINT64_CONSTANT (0x1234), "seq", 0x4242, "hdrext-1", ext,
      NULL);

  set_state (state, GST_STATE_NULL);

  validate_buffers_received (15);

  gst_object_unref (ext);
  destroy_depayloader (state);
}

GST_END_TEST;

static GstFlowReturn
hdr_ext_aggregate_chain_func (GstPad * pad, GstObject * parent,
    GstBuffer * buffer)
{
  GstFlowReturn res;
  GstCaps *caps;
  guint val;
  GstPad *srcpad;
  GstElement *depay;
  static gboolean first = TRUE;
  static guint expected_caps_val = 0;

  res = gst_check_chain_func (pad, parent, buffer);
  if (res != GST_FLOW_OK) {
    return res;
  }

  caps = gst_pad_get_current_caps (pad);

  fail_unless (gst_structure_get_uint (gst_caps_get_structure (caps, 0),
          "dummy-hdrext-val", &val));

  srcpad = gst_pad_get_peer (pad);
  depay = gst_pad_get_parent_element (srcpad);

  switch (GST_RTP_DUMMY_DEPAY (depay)->aggregate_method) {
    case GST_RTP_DUMMY_PUSH_AGGREGATE_DEFAULT:
      /* Every fifth buffer increments "dummy-hdrext-val", but we
         aggregate 5 buffers per output buffer so we increment for every
         output buffer. */
      expected_caps_val++;
      break;
    case GST_RTP_DUMMY_PUSH_AGGREGATE_DROP:
      /* We aggregate 5 buffers per output buffer but drop 4 of them
         from the buffer cache. */
      if (g_list_length (buffers) % 5 == 1) {
        expected_caps_val++;
      }
      break;
    case GST_RTP_DUMMY_PUSH_AGGREGATE_DELAYED:
      /* We aggregate 6 buffers per output buffer but delay the 6th one
         which will then account to the 2nd output buffer. Thus the 1st
         output buffer will process 5 header extensions (val increments
         by one) whereas the 2nd buffer will process 6 (val increments
         by two)! */
      if (first) {
        first = FALSE;
        expected_caps_val++;
      } else {
        expected_caps_val += 2;
      }
      break;
    case GST_RTP_DUMMY_PUSH_AGGREGATE_FLUSH:
      /* We aggregate 5 buffers per output buffer but flush 4 of them
         from the hdr ext buffer cache. */
      if (g_list_length (buffers) % 5 == 1) {
        expected_caps_val++;
      }
      break;
  }

  gst_object_unref (depay);
  gst_object_unref (srcpad);

  fail_unless_equals_int (expected_caps_val, val);

  gst_caps_unref (caps);

  return res;
}

static void
hdr_ext_aggregate_test (gint n_buffers, gint n_aggregate,
    GstRtpDummyPushAggregateMethod method)
{
  GstRTPHeaderExtension *ext;
  State *state;
  guint i;

  state = create_depayloader ("application/x-rtp", NULL);
  gst_rtp_base_depayload_set_aggregate_hdrext_enabled (GST_RTP_BASE_DEPAYLOAD
      (state->element), TRUE);
  gst_pad_set_chain_function (state->sinkpad, hdr_ext_aggregate_chain_func);
  ext = rtp_dummy_hdr_ext_new ();
  gst_rtp_header_extension_set_id (ext, 1);

  GST_RTP_DUMMY_DEPAY (state->element)->push_method =
      GST_RTP_DUMMY_USE_PUSH_AGGREGATE_FUNC;
  GST_RTP_DUMMY_DEPAY (state->element)->num_buffers_to_aggregate = n_aggregate;
  GST_RTP_DUMMY_DEPAY (state->element)->aggregate_method = method;

  g_signal_emit_by_name (state->element, "add-extension", ext);
  set_state (state, GST_STATE_PLAYING);

  for (i = 0; i < n_buffers; ++i) {
    push_rtp_buffer (state, "pts", 0 * GST_SECOND,
        "rtptime", G_GUINT64_CONSTANT (0x1234), "seq", 0x4242 + i, "hdrext-1",
        ext, NULL);
  }

  set_state (state, GST_STATE_NULL);
  validate_buffers_received (n_buffers / n_aggregate);
  gst_object_unref (ext);
  destroy_depayloader (state);
}

GST_START_TEST (rtp_base_depayload_hdr_ext_aggregate)
{
  const gint num_buffers = 30;
  const gint num_buffers_aggregate = 5; /* must match the modulo from
                                           hdrext */

  fail_unless_equals_int (num_buffers % num_buffers_aggregate, 0);

  hdr_ext_aggregate_test (num_buffers, num_buffers_aggregate,
      GST_RTP_DUMMY_PUSH_AGGREGATE_DEFAULT);
}

GST_END_TEST;

GST_START_TEST (rtp_base_depayload_hdr_ext_aggregate_drop)
{
  const gint num_buffers = 30;
  const gint num_buffers_aggregate = 5; /* must match the modulo from
                                           hdrext */

  fail_unless_equals_int (num_buffers % num_buffers_aggregate, 0);

  hdr_ext_aggregate_test (num_buffers, num_buffers_aggregate,
      GST_RTP_DUMMY_PUSH_AGGREGATE_DROP);
}

GST_END_TEST;

GST_START_TEST (rtp_base_depayload_hdr_ext_aggregate_delayed)
{
  const gint num_buffers = 12;  /* must be two times
                                   num_buffers_aggregate */
  const gint num_buffers_aggregate = 6; /* must match the modulo from
                                           hdrext + 1 */

  fail_unless_equals_int (num_buffers % num_buffers_aggregate, 0);
  fail_unless_equals_int (num_buffers / num_buffers_aggregate, 2);

  hdr_ext_aggregate_test (num_buffers, num_buffers_aggregate,
      GST_RTP_DUMMY_PUSH_AGGREGATE_DELAYED);
}

GST_END_TEST;

GST_START_TEST (rtp_base_depayload_hdr_ext_aggregate_flush)
{
  const gint num_buffers = 30;
  const gint num_buffers_aggregate = 5; /* must match the modulo from
                                           hdrext */

  fail_unless_equals_int (num_buffers % num_buffers_aggregate, 0);

  hdr_ext_aggregate_test (num_buffers, num_buffers_aggregate,
      GST_RTP_DUMMY_PUSH_AGGREGATE_FLUSH);
}

GST_END_TEST;

static Suite *
rtp_basepayloading_suite (void)
{
  Suite *s = suite_create ("rtp_base_depayloading_test");
  TCase *tc_chain = tcase_create ("depayloading tests");

  tcase_set_timeout (tc_chain, 60);

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, rtp_base_depayload_buffer_test);

  tcase_add_test (tc_chain, rtp_base_depayload_invalid_rtp_packet_test);
  tcase_add_test (tc_chain, rtp_base_depayload_with_gap_test);
  tcase_add_test (tc_chain, rtp_base_depayload_reversed_test);
  tcase_add_test (tc_chain, rtp_base_depayload_ssrc_changed_test);
  tcase_add_test (tc_chain, rtp_base_depayload_old_reversed_test);

  tcase_add_test (tc_chain, rtp_base_depayload_without_negotiation_test);

  tcase_add_test (tc_chain, rtp_base_depayload_packet_lost_test);
  tcase_add_test (tc_chain,
      rtp_base_depayload_packet_lost_before_first_buffer_test);
  tcase_add_test (tc_chain, rtp_base_depayload_seq_discont_test);

  tcase_add_test (tc_chain, rtp_base_depayload_repeated_caps_test);
  tcase_add_test (tc_chain, rtp_base_depayload_npt_test);
  tcase_add_test (tc_chain, rtp_base_depayload_play_scale_test);
  tcase_add_test (tc_chain, rtp_base_depayload_play_speed_test);
  tcase_add_test (tc_chain, rtp_base_depayload_clock_base_test);

  tcase_add_test (tc_chain, rtp_base_depayload_source_info_test);
  tcase_add_test (tc_chain, rtp_base_depayload_source_info_from_rtp_only);
  tcase_add_test (tc_chain, rtp_base_depayload_max_reorder);

  tcase_add_test (tc_chain, rtp_base_depayload_flow_return_push_func);
  tcase_add_test (tc_chain, rtp_base_depayload_flow_return_push_list_func);

  tcase_add_test (tc_chain, rtp_base_depayload_one_byte_hdr_ext);
  tcase_add_test (tc_chain, rtp_base_depayload_two_byte_hdr_ext);
  tcase_add_test (tc_chain, rtp_base_depayload_request_extension);
  tcase_add_test (tc_chain, rtp_base_depayload_clear_extensions);
  tcase_add_test (tc_chain, rtp_base_depayload_multiple_exts);
  tcase_add_test (tc_chain, rtp_base_depayload_caps_request_ignored);
  tcase_add_test (tc_chain, rtp_base_depayload_hdr_ext_caps_change);
  tcase_add_test (tc_chain, rtp_base_depayload_hdr_ext_aggregate);
  tcase_add_test (tc_chain, rtp_base_depayload_hdr_ext_aggregate_drop);
  tcase_add_test (tc_chain, rtp_base_depayload_hdr_ext_aggregate_delayed);
  tcase_add_test (tc_chain, rtp_base_depayload_hdr_ext_aggregate_flush);
  return s;
}

GST_CHECK_MAIN (rtp_basepayloading)
