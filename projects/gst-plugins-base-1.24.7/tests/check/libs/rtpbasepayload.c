/* GStreamer RTP base payloader unit tests
 * Copyright (C) 2014  Sebastian Rasmussen <sebras@hotmail.com>
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
#define BUFFER_BEFORE_LIST (10)

/* GstRtpDummyPay */

#define GST_TYPE_RTP_DUMMY_PAY \
  (gst_rtp_dummy_pay_get_type())
#define GST_RTP_DUMMY_PAY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_RTP_DUMMY_PAY,GstRtpDummyPay))
#define GST_RTP_DUMMY_PAY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_RTP_DUMMY_PAY,GstRtpDummyPayClass))
#define GST_IS_RTP_DUMMY_PAY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_RTP_DUMMY_PAY))
#define GST_IS_RTP_DUMMY_PAY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_RTP_DUMMY_PAY))

typedef struct _GstRtpDummyPay GstRtpDummyPay;
typedef struct _GstRtpDummyPayClass GstRtpDummyPayClass;

struct _GstRtpDummyPay
{
  GstRTPBasePayload payload;
};

struct _GstRtpDummyPayClass
{
  GstRTPBasePayloadClass parent_class;
};

GType gst_rtp_dummy_pay_get_type (void);

G_DEFINE_TYPE (GstRtpDummyPay, gst_rtp_dummy_pay, GST_TYPE_RTP_BASE_PAYLOAD);

static GstFlowReturn gst_rtp_dummy_pay_handle_buffer (GstRTPBasePayload * pay,
    GstBuffer * buffer);

static GstStaticPadTemplate gst_rtp_dummy_pay_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate gst_rtp_dummy_pay_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-rtp"));

static void
gst_rtp_dummy_pay_class_init (GstRtpDummyPayClass * klass)
{
  GstElementClass *gstelement_class;
  GstRTPBasePayloadClass *gstrtpbasepayload_class;

  gstelement_class = GST_ELEMENT_CLASS (klass);
  gstrtpbasepayload_class = GST_RTP_BASE_PAYLOAD_CLASS (klass);

  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_rtp_dummy_pay_sink_template);
  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_rtp_dummy_pay_src_template);

  gstrtpbasepayload_class->handle_buffer = gst_rtp_dummy_pay_handle_buffer;
}

static void
gst_rtp_dummy_pay_init (GstRtpDummyPay * pay)
{
  gst_rtp_base_payload_set_options (GST_RTP_BASE_PAYLOAD (pay), "application",
      TRUE, "dummy", DEFAULT_CLOCK_RATE);
}

static GstRtpDummyPay *
rtp_dummy_pay_new (void)
{
  return g_object_new (GST_TYPE_RTP_DUMMY_PAY, NULL);
}

static GstFlowReturn
gst_rtp_dummy_pay_handle_buffer (GstRTPBasePayload * pay, GstBuffer * buffer)
{
  GstBuffer *paybuffer;

  GST_LOG ("payloading %" GST_PTR_FORMAT, buffer);

  if (!gst_pad_has_current_caps (GST_RTP_BASE_PAYLOAD_SRCPAD (pay))) {
    if (!gst_rtp_base_payload_set_outcaps (GST_RTP_BASE_PAYLOAD (pay),
            "custom-caps", G_TYPE_UINT, DEFAULT_CLOCK_RATE, NULL)) {
      gst_buffer_unref (buffer);
      return GST_FLOW_NOT_NEGOTIATED;
    }
  }

  paybuffer =
      gst_rtp_base_payload_allocate_output_buffer (GST_RTP_BASE_PAYLOAD (pay),
      0, 0, 0);

  GST_BUFFER_PTS (paybuffer) = GST_BUFFER_PTS (buffer);
  GST_BUFFER_OFFSET (paybuffer) = GST_BUFFER_OFFSET (buffer);

  gst_buffer_append (paybuffer, buffer);

  GST_LOG ("payloaded %" GST_PTR_FORMAT, paybuffer);

  if (GST_BUFFER_PTS (paybuffer) < BUFFER_BEFORE_LIST) {
    return gst_rtp_base_payload_push (pay, paybuffer);
  } else {
    GstBufferList *list = gst_buffer_list_new ();
    gst_buffer_list_add (list, paybuffer);
    return gst_rtp_base_payload_push_list (pay, list);
  }
}

/* Helper functions and global state */

static GstStaticPadTemplate srctmpl = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate sinktmpl = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate special_sinktmpl = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-rtp, payload=(int)98, ssrc=(uint)24, "
        "timestamp-offset=(uint)212, seqnum-offset=(uint)2424"));

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
    } else if (!g_strcmp0 (field, "ssrc")) {
      guint expected = va_arg (var_args, guint);
      GstCaps *caps;
      guint ssrc;
      gst_event_parse_caps (event, &caps);
      fail_unless (gst_structure_get_uint (gst_caps_get_structure (caps, 0),
              "ssrc", &ssrc));
      fail_unless_equals_int (ssrc, expected);
    } else if (!g_strcmp0 (field, "a-framerate")) {
      const gchar *expected = va_arg (var_args, const gchar *);
      GstCaps *caps;
      const gchar *framerate;
      gst_event_parse_caps (event, &caps);
      framerate = gst_structure_get_string (gst_caps_get_structure (caps, 0),
          "a-framerate");
      fail_unless_equals_string (framerate, expected);
    } else if (!g_strcmp0 (field, "extmap-str")) {
      guint ext_id = va_arg (var_args, guint);
      const gchar *expected_ext_str = va_arg (var_args, const gchar *);
      GstCaps *caps;
      const gchar *ext_str;
      gchar *ext_field = g_strdup_printf ("extmap-%u", ext_id);
      gst_event_parse_caps (event, &caps);
      ext_str = gst_structure_get_string (gst_caps_get_structure (caps, 0),
          ext_field);
      fail_unless_equals_string (ext_str, expected_ext_str);
      g_free (ext_field);
    } else {
      fail ("test cannot validate unknown event field '%s'", field);
    }
    field = va_arg (var_args, const gchar *);
  }
  va_end (var_args);
}

static void
validate_normal_start_events (guint index)
{
  validate_event (index, "stream-start", NULL);

  validate_event (index + 1, "caps", "media-type", "application/x-rtp", NULL);

  validate_event (index + 2, "segment",
      "time", G_GUINT64_CONSTANT (0),
      "start", G_GUINT64_CONSTANT (0), "stop", G_MAXUINT64, NULL);
}

#define push_buffer(state, field, ...) \
	push_buffer_full ((state), GST_FLOW_OK, (field), __VA_ARGS__)
#define push_buffer_fails(state, error, field, ...) \
	push_buffer_full ((state), (error), (field), __VA_ARGS__)

static void
push_buffer_full (State * state, GstFlowReturn expected,
    const gchar * field, ...)
{
  GstBuffer *buf = gst_buffer_new_allocate (0, 0, 0);
  GstRTPBuffer rtp = GST_RTP_BUFFER_INIT;
  gboolean mapped = FALSE;
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
      if (!mapped) {
        gst_rtp_buffer_map (buf, GST_MAP_WRITE, &rtp);
        mapped = TRUE;
      }
      if (!g_strcmp0 (field, "rtptime")) {
        guint32 rtptime = va_arg (var_args, guint);
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
      } else {
        fail ("test cannot set unknown buffer field '%s'", field);
      }
    }
    field = va_arg (var_args, const gchar *);
  }
  va_end (var_args);

  if (mapped) {
    gst_rtp_buffer_unmap (&rtp);
  }

  fail_unless_equals_int (gst_pad_push (state->srcpad, buf), expected);
}

static void
push_buffer_list (State * state, const gchar * field, ...)
{
  GstBuffer *buf = gst_rtp_buffer_new_allocate (0, 0, 0);
  GstRTPBuffer rtp = GST_RTP_BUFFER_INIT;
  gboolean mapped = FALSE;
  GstBufferList *list;
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
      if (!mapped) {
        gst_rtp_buffer_map (buf, GST_MAP_WRITE, &rtp);
        mapped = TRUE;
      }
      if (!g_strcmp0 (field, "rtptime")) {
        guint32 rtptime = va_arg (var_args, guint);
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
      } else {
        fail ("test cannot set unknown buffer field '%s'", field);
      }
    }
    field = va_arg (var_args, const gchar *);
  }
  va_end (var_args);

  if (mapped) {
    gst_rtp_buffer_unmap (&rtp);
  }

  list = gst_buffer_list_new ();
  gst_buffer_list_add (list, buf);
  fail_unless_equals_int (gst_pad_push_list (state->srcpad, list), GST_FLOW_OK);
}

static void
validate_buffers_received (guint received_buffers)
{
  fail_unless_equals_int (g_list_length (buffers), received_buffers);
}

static void
validate_buffer_valist (GstBuffer * buf, const gchar * field, va_list var_args)
{
  GstRTPBuffer rtp = GST_RTP_BUFFER_INIT;
  gboolean mapped = FALSE;

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
    } else if (!g_strcmp0 (field, "size")) {
      gsize expected_size = va_arg (var_args, gsize);
      fail_unless_equals_int64 ((guint64) expected_size,
          gst_buffer_get_size (buf));
    } else {
      if (!mapped) {
        gst_rtp_buffer_map (buf, GST_MAP_READ, &rtp);
        mapped = TRUE;
      }
      if (!g_strcmp0 (field, "rtptime")) {
        guint32 rtptime = va_arg (var_args, guint);
        fail_unless_equals_int (gst_rtp_buffer_get_timestamp (&rtp), rtptime);
      } else if (!g_strcmp0 (field, "payload-type")) {
        guint pt = va_arg (var_args, guint);
        fail_unless_equals_int (gst_rtp_buffer_get_payload_type (&rtp), pt);
      } else if (!g_strcmp0 (field, "seq")) {
        guint seq = va_arg (var_args, guint);
        fail_unless_equals_int (gst_rtp_buffer_get_seq (&rtp), seq);
      } else if (!g_strcmp0 (field, "ssrc")) {
        guint32 ssrc = va_arg (var_args, guint);
        fail_unless_equals_int (gst_rtp_buffer_get_ssrc (&rtp), ssrc);
      } else if (!g_strcmp0 (field, "csrc")) {
        guint idx = va_arg (var_args, guint);
        guint csrc = va_arg (var_args, guint);
        fail_unless_equals_int (gst_rtp_buffer_get_csrc (&rtp, idx), csrc);
      } else if (!g_strcmp0 (field, "csrc-count")) {
        guint csrc_count = va_arg (var_args, guint);
        fail_unless_equals_int (gst_rtp_buffer_get_csrc_count (&rtp),
            csrc_count);
      } else if (!g_strcmp0 (field, "ext-data")) {
        guint expected_bits = va_arg (var_args, guint) & 0xFFFF;
        gsize expected_size = va_arg (var_args, gsize);
        gpointer data;
        guint word_len;
        guint16 ext_bits;
        gst_rtp_buffer_get_extension_data (&rtp, &ext_bits, &data, &word_len);
        GST_MEMDUMP ("ext data", data, word_len * 4);
        fail_unless_equals_int (expected_bits, ext_bits);
        fail_unless_equals_int64 ((guint64) expected_size,
            (guint64) word_len * 4);
      } else {
        fail ("test cannot validate unknown buffer field '%s'", field);
      }
    }
    field = va_arg (var_args, const gchar *);
  }

  if (mapped) {
    gst_rtp_buffer_unmap (&rtp);
  }
}

static void
validate_buffer1 (GstBuffer * buf, const gchar * field, ...)
{
  va_list var_args;

  va_start (var_args, field);
  validate_buffer_valist (buf, field, var_args);
  va_end (var_args);
}

static void
validate_buffer (guint index, const gchar * field, ...)
{
  GstBuffer *buf;
  va_list var_args;

  fail_if (index >= g_list_length (buffers));
  buf = GST_BUFFER (g_list_nth_data (buffers, index));
  fail_if (buf == NULL);

  GST_TRACE ("%" GST_PTR_FORMAT, buf);

  va_start (var_args, field);
  validate_buffer_valist (buf, field, var_args);
  va_end (var_args);
}

static void
get_buffer_field (guint index, const gchar * field, ...)
{
  GstBuffer *buf;
  GstRTPBuffer rtp = GST_RTP_BUFFER_INIT;
  gboolean mapped = FALSE;
  va_list var_args;

  fail_if (index >= g_list_length (buffers));
  buf = GST_BUFFER (g_list_nth_data (buffers, (index)));
  fail_if (buf == NULL);

  va_start (var_args, field);
  while (field) {
    if (!g_strcmp0 (field, "pts")) {
      GstClockTime *pts = va_arg (var_args, GstClockTime *);
      *pts = GST_BUFFER_PTS (buf);
    } else if (!g_strcmp0 (field, "offset")) {
      guint64 *offset = va_arg (var_args, guint64 *);
      *offset = GST_BUFFER_OFFSET (buf);
    } else if (!g_strcmp0 (field, "discont")) {
      gboolean *discont = va_arg (var_args, gboolean *);
      *discont = GST_BUFFER_FLAG_IS_SET (buf, GST_BUFFER_FLAG_DISCONT);
    } else {
      if (!mapped) {
        gst_rtp_buffer_map (buf, GST_MAP_READ, &rtp);
        mapped = TRUE;
      }
      if (!g_strcmp0 (field, "rtptime")) {
        guint32 *rtptime = va_arg (var_args, guint32 *);
        *rtptime = gst_rtp_buffer_get_timestamp (&rtp);
      } else if (!g_strcmp0 (field, "payload-type")) {
        guint *pt = va_arg (var_args, guint *);
        *pt = gst_rtp_buffer_get_payload_type (&rtp);
      } else if (!g_strcmp0 (field, "seq")) {
        guint16 *seq = va_arg (var_args, guint16 *);
        *seq = gst_rtp_buffer_get_seq (&rtp);
      } else if (!g_strcmp0 (field, "ssrc")) {
        guint32 *ssrc = va_arg (var_args, guint32 *);
        *ssrc = gst_rtp_buffer_get_ssrc (&rtp);
      } else {
        fail ("test retrieve validate unknown buffer field '%s'", field);
      }
    }
    field = va_arg (var_args, const gchar *);
  }
  va_end (var_args);

  if (mapped)
    gst_rtp_buffer_unmap (&rtp);
}

static State *
create_payloader (const gchar * caps_str,
    GstStaticPadTemplate * sinktmpl, const gchar * property, ...)
{
  va_list var_args;
  GstCaps *caps;
  State *state;

  state = g_new0 (State, 1);

  state->element = GST_ELEMENT (rtp_dummy_pay_new ());
  fail_unless (GST_IS_RTP_DUMMY_PAY (state->element));

  va_start (var_args, property);
  g_object_set_valist (G_OBJECT (state->element), property, var_args);
  va_end (var_args);

  state->srcpad = gst_check_setup_src_pad (state->element, &srctmpl);
  state->sinkpad = gst_check_setup_sink_pad (state->element, sinktmpl);

  fail_unless (gst_pad_set_active (state->srcpad, TRUE));
  fail_unless (gst_pad_set_active (state->sinkpad, TRUE));

  caps = gst_caps_from_string (caps_str);
  gst_check_setup_events (state->srcpad, state->element, caps, GST_FORMAT_TIME);
  gst_caps_unref (caps);

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
validate_would_not_be_filled (State * state, guint size, GstClockTime duration)
{
  GstRTPBasePayload *basepay;
  basepay = GST_RTP_BASE_PAYLOAD (state->element);
  fail_if (gst_rtp_base_payload_is_filled (basepay, size, duration));
}

static void
validate_would_be_filled (State * state, guint size, GstClockTime duration)
{
  GstRTPBasePayload *basepay;
  basepay = GST_RTP_BASE_PAYLOAD (state->element);
  fail_unless (gst_rtp_base_payload_is_filled (basepay, size, duration));
}

static void
ssrc_collision (State * state, guint ssrc,
    gboolean have_new_ssrc, guint new_ssrc)
{
  GstStructure *s;
  GstEvent *event;
  if (have_new_ssrc) {
    s = gst_structure_new ("GstRTPCollision",
        "ssrc", G_TYPE_UINT, ssrc,
        "suggested-ssrc", G_TYPE_UINT, new_ssrc, NULL);
  } else {
    s = gst_structure_new ("GstRTPCollision", "ssrc", G_TYPE_UINT, ssrc, NULL);
  }
  event = gst_event_new_custom (GST_EVENT_CUSTOM_UPSTREAM, s);
  fail_unless (gst_pad_push_event (state->sinkpad, event));
}

static void
reconfigure (State * state)
{
  GstEvent *event;
  event = gst_event_new_reconfigure ();
  fail_unless (gst_pad_push_event (state->sinkpad, event));
}

static void
validate_stats (State * state, guint clock_rate,
    GstClockTime running_time, guint16 seq, guint32 rtptime)
{
  GstStructure *stats;

  g_object_get (state->element, "stats", &stats, NULL);

  fail_unless_equals_int (g_value_get_uint (gst_structure_get_value (stats,
              "clock-rate")), clock_rate);
  fail_unless_equals_uint64 (g_value_get_uint64 (gst_structure_get_value (stats,
              "running-time")), running_time);
  fail_unless_equals_int (g_value_get_uint (gst_structure_get_value (stats,
              "seqnum")), seq);
  fail_unless_equals_int (g_value_get_uint (gst_structure_get_value (stats,
              "timestamp")), rtptime);

  gst_structure_free (stats);
}

static void
destroy_payloader (State * state)
{
  gst_check_teardown_sink_pad (state->element);
  gst_check_teardown_src_pad (state->element);

  gst_check_drop_buffers ();
  drop_events ();

  g_object_unref (state->element);

  g_free (state);
}

/* Tests */

/* push two buffers to the payloader which should successfully payload them
 * into RTP packets. the first packet will have a random rtptime and sequence
 * number, but the last packet should have an rtptime incremented by
 * DEFAULT_CLOCK_RATE and a sequence number incremented by one because the
 * packets are sequential. besides the two payloaded RTP packets there should
 * be the three events initial events: stream-start, caps and segment.
 */
GST_START_TEST (rtp_base_payload_buffer_test)
{
  State *state;
  guint32 rtptime;
  guint16 seq;

  state = create_payloader ("application/x-rtp", &sinktmpl,
      "perfect-rtptime", FALSE, NULL);

  set_state (state, GST_STATE_PLAYING);

  push_buffer (state, "pts", 0 * GST_SECOND, NULL);

  push_buffer (state, "pts", 1 * GST_SECOND, NULL);

  set_state (state, GST_STATE_NULL);

  validate_buffers_received (2);

  validate_buffer (0, "pts", 0 * GST_SECOND, NULL);
  get_buffer_field (0, "rtptime", &rtptime, "seq", &seq, NULL);

  validate_buffer (1,
      "pts", 1 * GST_SECOND,
      "rtptime", rtptime + 1 * DEFAULT_CLOCK_RATE, "seq", seq + 1, NULL);

  validate_events_received (3);

  validate_normal_start_events (0);

  destroy_payloader (state);
}

GST_END_TEST;

/* push single buffers in buffer lists to the payloader to be payloaded into
 * RTP packets. the dummy payloader will start pushing buffer lists itself
 * after BUFFER_BEFORE_LIST payloaded RTP packets. any RTP packets included in
 * buffer lists should have rtptime and sequence numbers incrementting in the
 * same way as for separate RTP packets.
 */
GST_START_TEST (rtp_base_payload_buffer_list_test)
{
  State *state;
  guint32 rtptime;
  guint16 seq;
  guint i;

  state = create_payloader ("application/x-rtp", &sinktmpl, NULL);

  set_state (state, GST_STATE_PLAYING);

  for (i = 0; i < BUFFER_BEFORE_LIST + 1; i++) {
    push_buffer_list (state, "pts", i * GST_SECOND, NULL);
  }

  set_state (state, GST_STATE_NULL);

  validate_buffers_received (11);

  validate_buffer (0, "pts", 0 * GST_SECOND, NULL);
  get_buffer_field (0, "rtptime", &rtptime, "seq", &seq, NULL);

  for (i = 1; i < BUFFER_BEFORE_LIST + 1; i++) {
    validate_buffer (i,
        "pts", i * GST_SECOND,
        "rtptime", rtptime + i * DEFAULT_CLOCK_RATE, "seq", seq + i, NULL);
  }

  validate_events_received (3);

  validate_normal_start_events (0);

  destroy_payloader (state);
}

GST_END_TEST;

/* push two buffers. because the payloader is using non-perfect rtptime the
 * second buffer will be timestamped with the default clock and ignore any
 * offset set on the buffers being payloaded.
 */
GST_START_TEST (rtp_base_payload_normal_rtptime_test)
{
  guint32 rtptime;
  State *state;

  state = create_payloader ("application/x-rtp", &sinktmpl,
      "perfect-rtptime", FALSE, NULL);

  set_state (state, GST_STATE_PLAYING);

  push_buffer (state,
      "pts", 0 * GST_SECOND, "offset", GST_BUFFER_OFFSET_NONE, NULL);

  push_buffer (state,
      "pts", 1 * GST_SECOND, "offset", GST_BUFFER_OFFSET_NONE, NULL);

  set_state (state, GST_STATE_NULL);

  validate_buffers_received (2);

  validate_buffer (0,
      "pts", 0 * GST_SECOND, "offset", GST_BUFFER_OFFSET_NONE, NULL);
  get_buffer_field (0, "rtptime", &rtptime, NULL);

  validate_buffer (1,
      "pts", 1 * GST_SECOND,
      "offset", GST_BUFFER_OFFSET_NONE,
      "rtptime", rtptime + DEFAULT_CLOCK_RATE, NULL);

  validate_events_received (3);

  validate_normal_start_events (0);

  destroy_payloader (state);
}

GST_END_TEST;

/* push two buffers. because the payloader is using perfect rtptime the
 * second buffer will be timestamped with a timestamp incremented with the
 * difference in offset between the first and second buffer. the pts will be
 * ignored for any buffer after the first buffer.
 */
GST_START_TEST (rtp_base_payload_perfect_rtptime_test)
{
  guint32 rtptime;
  State *state;

  state = create_payloader ("application/x-rtp", &sinktmpl,
      "perfect-rtptime", TRUE, NULL);

  set_state (state, GST_STATE_PLAYING);

  push_buffer (state, "pts", 0 * GST_SECOND, "offset", G_GINT64_CONSTANT (0),
      NULL);

  push_buffer (state, "pts", GST_CLOCK_TIME_NONE, "offset",
      G_GINT64_CONSTANT (21), NULL);

  set_state (state, GST_STATE_NULL);

  validate_buffers_received (2);

  validate_buffer (0, "pts", 0 * GST_SECOND, "offset", G_GINT64_CONSTANT (0),
      NULL);
  get_buffer_field (0, "rtptime", &rtptime, NULL);

  validate_buffer (1,
      "pts", GST_CLOCK_TIME_NONE, "offset", G_GINT64_CONSTANT (21), "rtptime",
      rtptime + 21, NULL);

  validate_events_received (3);

  validate_normal_start_events (0);

  destroy_payloader (state);
}

GST_END_TEST;

/* validate that a payloader will re-use the last used timestamp when a buffer
 * is using perfect rtptime and both the pushed buffers timestamp and the offset
 * is NONE. the payloader is configuered to start with a specific timestamp.
 * then a buffer is sent with a valid timestamp but without any offset. the
 * payloded RTP packet is expected to use the specific timestamp. next another
 * buffer is pushed with a normal timestamp set to illustrate that the payloaded
 * RTP packet will have an increased timestamp. finally a buffer without any
 * timestamp or offset is pushed. in this case the payloaded RTP packet is
 * expected to have the same timestamp as the previously payloaded RTP packet.
 */
GST_START_TEST (rtp_base_payload_no_pts_no_offset_test)
{
  State *state;

  state = create_payloader ("application/x-rtp", &sinktmpl,
      "timestamp-offset", 0x42, NULL);

  set_state (state, GST_STATE_PLAYING);

  push_buffer (state,
      "pts", 0 * GST_SECOND, "offset", GST_BUFFER_OFFSET_NONE, NULL);

  push_buffer (state,
      "pts", 1 * GST_SECOND, "offset", GST_BUFFER_OFFSET_NONE, NULL);

  push_buffer (state,
      "pts", GST_CLOCK_TIME_NONE, "offset", GST_BUFFER_OFFSET_NONE, NULL);

  set_state (state, GST_STATE_NULL);

  validate_buffers_received (3);

  validate_buffer (0,
      "pts", 0 * GST_SECOND,
      "offset", GST_BUFFER_OFFSET_NONE, "rtptime", 0x42, NULL);

  validate_buffer (1,
      "pts", 1 * GST_SECOND,
      "offset", GST_BUFFER_OFFSET_NONE,
      "rtptime", 0x42 + 1 * DEFAULT_CLOCK_RATE, NULL);

  validate_buffer (2,
      "pts", GST_CLOCK_TIME_NONE,
      "offset", GST_BUFFER_OFFSET_NONE,
      "rtptime", 0x42 + 1 * DEFAULT_CLOCK_RATE, NULL);

  validate_events_received (3);

  validate_normal_start_events (0);

  destroy_payloader (state);
}

GST_END_TEST;

/* validate that a downstream element with caps on its sink pad can effectively
 * configure the payloader's payload-type, ssrc, timestamp-offset and
 * seqnum-offset properties and therefore also affect the payloaded RTP packets.
 * this is done by connecting to a sink pad with template caps setting the
 * relevant fields and then pushing a buffer and making sure that the payloaded
 * RTP packet has the expected properties.
 */
GST_START_TEST (rtp_base_payload_downstream_caps_test)
{
  State *state;

  state = create_payloader ("application/x-rtp", &special_sinktmpl, NULL);

  set_state (state, GST_STATE_PLAYING);

  push_buffer (state, "pts", 0 * GST_SECOND, NULL);

  set_state (state, GST_STATE_NULL);

  validate_buffers_received (1);

  validate_buffer (0,
      "pts", 0 * GST_SECOND,
      "seq", 2424, "payload-type", 98, "ssrc", 24, "rtptime", 212, NULL);

  validate_events_received (3);

  validate_normal_start_events (0);

  destroy_payloader (state);
}

GST_END_TEST;

/* when a payloader receives a GstRTPCollision upstream event it should try to
 * switch to a new ssrc for the next payloaded RTP packets. GstRTPCollision can
 * supply a suggested new ssrc. if a suggested new ssrc is supplied then the
 * payloaded is supposed to use this new ssrc, otherwise it should generate a
 * new random ssrc which is not identical to the one that collided.
 *
 * this is tested by first setting the ssrc to a specific value and pushing a
 * buffer. the payloaded RTP packet is validate to have the set ssrc. then a
 * GstRTPCollision event is generated to instruct the payloader that the
 * previously set ssrc collided. this event suggests a new ssrc and it is
 * verified that a pushed buffer results in a payloaded RTP packet that actually
 * uses this new ssrc. finally a new GstRTPCollision event is generated to
 * indicate another ssrc collision. this time the event does not suggest a new
 * ssrc. the payloaded RTP packet is then expected to have a new random ssrc
 * different from the collided one.
 */
GST_START_TEST (rtp_base_payload_ssrc_collision_test)
{
  State *state;
  guint32 ssrc;

  state = create_payloader ("application/x-rtp", &sinktmpl, NULL);

  g_object_set (state->element, "ssrc", 0x4242, NULL);
  g_object_get (state->element, "ssrc", &ssrc, NULL);
  fail_unless_equals_int (ssrc, 0x4242);

  set_state (state, GST_STATE_PLAYING);

  push_buffer (state, "pts", 0 * GST_SECOND, NULL);

  ssrc_collision (state, 0x4242, TRUE, 0x4343);

  push_buffer (state, "pts", 1 * GST_SECOND, NULL);

  ssrc_collision (state, 0x4343, FALSE, 0);

  push_buffer (state, "pts", 2 * GST_SECOND, NULL);

  set_state (state, GST_STATE_NULL);

  validate_buffers_received (3);

  validate_buffer (0, "pts", 0 * GST_SECOND, "ssrc", 0x4242, NULL);

  validate_buffer (1, "pts", 1 * GST_SECOND, "ssrc", 0x4343, NULL);

  validate_buffer (2, "pts", 2 * GST_SECOND, NULL);
  get_buffer_field (2, "ssrc", &ssrc, NULL);
  fail_if (ssrc == 0x4343);

  validate_events_received (5);

  validate_normal_start_events (0);

  validate_event (3, "caps",
      "media-type", "application/x-rtp", "ssrc", 0x4343, NULL);

  validate_event (4, "caps",
      "media-type", "application/x-rtp", "ssrc", ssrc, NULL);

  destroy_payloader (state);
}

GST_END_TEST;

/* validate that an upstream event different from GstRTPCollision is successfully
 * forwarded to upstream elements. in this test a caps reconfiguration event is
 * pushed upstream to validate the behaviour.
 */
GST_START_TEST (rtp_base_payload_reconfigure_test)
{
  State *state;

  state = create_payloader ("application/x-rtp", &sinktmpl, NULL);

  set_state (state, GST_STATE_PLAYING);

  push_buffer (state, "pts", 0 * GST_SECOND, NULL);

  reconfigure (state);

  push_buffer (state, "pts", 1 * GST_SECOND, NULL);

  set_state (state, GST_STATE_NULL);

  validate_buffers_received (2);

  validate_buffer (0, "pts", 0 * GST_SECOND, NULL);

  validate_buffer (1, "pts", 1 * GST_SECOND, NULL);

  validate_events_received (4);

  validate_normal_start_events (0);

  destroy_payloader (state);
}

GST_END_TEST;

/* validate that changing the mtu actually affects whether buffers are
 * considered to be filled. first detect the default mtu and check that having
 * buffers slightly less or equal to the size will not be considered to be
 * filled, and that going over this size will be filling the buffers. then
 * change the mtu slightly and validate that the boundary actually changed.
 * lastly try the boundary values and make sure that they work as expected.
 */
GST_START_TEST (rtp_base_payload_property_mtu_test)
{
  State *state;
  guint mtu, check;

  state = create_payloader ("application/x-rtp", &sinktmpl, NULL);

  g_object_get (state->element, "mtu", &mtu, NULL);
  validate_would_not_be_filled (state, mtu - 1, GST_CLOCK_TIME_NONE);
  validate_would_not_be_filled (state, mtu, GST_CLOCK_TIME_NONE);
  validate_would_be_filled (state, mtu + 1, GST_CLOCK_TIME_NONE);

  g_object_set (state->element, "mtu", mtu - 1, NULL);
  g_object_get (state->element, "mtu", &check, NULL);
  fail_unless_equals_int (check, mtu - 1);
  validate_would_not_be_filled (state, mtu - 1, GST_CLOCK_TIME_NONE);
  validate_would_be_filled (state, mtu, GST_CLOCK_TIME_NONE);
  validate_would_be_filled (state, mtu + 1, GST_CLOCK_TIME_NONE);

  g_object_set (state->element, "mtu", 28, NULL);
  g_object_get (state->element, "mtu", &check, NULL);
  fail_unless_equals_int (check, 28);
  validate_would_not_be_filled (state, 28, GST_CLOCK_TIME_NONE);
  validate_would_be_filled (state, 29, GST_CLOCK_TIME_NONE);

  g_object_set (state->element, "mtu", G_MAXUINT, NULL);
  g_object_get (state->element, "mtu", &check, NULL);
  fail_unless_equals_int (check, G_MAXUINT);
  validate_would_not_be_filled (state, G_MAXUINT - 1, GST_CLOCK_TIME_NONE);
  validate_would_not_be_filled (state, G_MAXUINT, GST_CLOCK_TIME_NONE);

  destroy_payloader (state);
}

GST_END_TEST;

/* validate that changing the payload-type will actually affect the
 * payload-type of the payloaded RTP packets. first get the default, then send
 * a buffer with this payload-type. increment the payload-type and send another
 * buffer. then test the boundary values for the payload-type and make sure
 * that these are all carried over to the payloaded RTP packets.
 */
GST_START_TEST (rtp_base_payload_property_pt_test)
{
  State *state;
  guint payload_type, check;

  state = create_payloader ("application/x-rtp", &sinktmpl, NULL);

  set_state (state, GST_STATE_PLAYING);

  g_object_get (state->element, "pt", &payload_type, NULL);
  push_buffer (state, "pts", 0 * GST_SECOND, NULL);

  g_object_set (state->element, "pt", payload_type + 1, NULL);
  g_object_get (state->element, "pt", &check, NULL);
  fail_unless_equals_int (check, payload_type + 1);
  push_buffer (state, "pts", 1 * GST_SECOND, NULL);

  g_object_set (state->element, "pt", 0, NULL);
  g_object_get (state->element, "pt", &check, NULL);
  fail_unless_equals_int (check, 0);
  push_buffer (state, "pts", 2 * GST_SECOND, NULL);

  g_object_set (state->element, "pt", 0x7f, NULL);
  g_object_get (state->element, "pt", &check, NULL);
  fail_unless_equals_int (check, 0x7f);
  push_buffer (state, "pts", 3 * GST_SECOND, NULL);

  set_state (state, GST_STATE_NULL);

  validate_buffers_received (4);

  validate_buffer (0,
      "pts", 0 * GST_SECOND, "payload-type", payload_type, NULL);

  validate_buffer (1,
      "pts", 1 * GST_SECOND, "payload-type", payload_type + 1, NULL);

  validate_buffer (2, "pts", 2 * GST_SECOND, "payload-type", 0, NULL);

  validate_buffer (3, "pts", 3 * GST_SECOND, "payload-type", 0x7f, NULL);

  validate_events_received (3);

  validate_normal_start_events (0);

  destroy_payloader (state);
}

GST_END_TEST;

/* validate that changing the ssrc will actually affect the ssrc of the
 * payloaded RTP packets. first get the current ssrc which should indicate
 * random ssrcs. send two buffers and expect their ssrcs to be random but
 * identical. since setting the ssrc will only take effect when the pipeline
 * goes READY->PAUSED, bring the pipeline to NULL state, set the ssrc to a given
 * value and make sure that this is carried over to the payloaded RTP packets.
 * the last step is to test the boundary values.
 */
GST_START_TEST (rtp_base_payload_property_ssrc_test)
{
  State *state;
  guint32 ssrc;

  state = create_payloader ("application/x-rtp", &sinktmpl, NULL);

  set_state (state, GST_STATE_PLAYING);

  g_object_get (state->element, "ssrc", &ssrc, NULL);
  fail_unless_equals_int (ssrc, -1);

  push_buffer (state, "pts", 0 * GST_SECOND, NULL);

  push_buffer (state, "pts", 1 * GST_SECOND, NULL);

  set_state (state, GST_STATE_NULL);
  g_object_set (state->element, "ssrc", 0x4242, NULL);
  g_object_get (state->element, "ssrc", &ssrc, NULL);
  fail_unless_equals_int (ssrc, 0x4242);
  set_state (state, GST_STATE_PLAYING);

  push_buffer (state, "pts", 2 * GST_SECOND, NULL);

  set_state (state, GST_STATE_NULL);
  g_object_set (state->element, "ssrc", 0, NULL);
  g_object_get (state->element, "ssrc", &ssrc, NULL);
  fail_unless_equals_int (ssrc, 0);
  set_state (state, GST_STATE_PLAYING);

  push_buffer (state, "pts", 3 * GST_SECOND, NULL);

  set_state (state, GST_STATE_NULL);
  g_object_set (state->element, "ssrc", G_MAXUINT32, NULL);
  g_object_get (state->element, "ssrc", &ssrc, NULL);
  fail_unless_equals_int (ssrc, G_MAXUINT32);
  set_state (state, GST_STATE_PLAYING);

  push_buffer (state, "pts", 4 * GST_SECOND, NULL);

  set_state (state, GST_STATE_NULL);

  validate_buffers_received (5);

  validate_buffer (0, "pts", 0 * GST_SECOND, NULL);
  get_buffer_field (0, "ssrc", &ssrc, NULL);

  validate_buffer (1, "pts", 1 * GST_SECOND, "ssrc", ssrc, NULL);

  validate_buffer (2, "pts", 2 * GST_SECOND, "ssrc", 0x4242, NULL);

  validate_buffer (3, "pts", 3 * GST_SECOND, "ssrc", 0, NULL);

  validate_buffer (4, "pts", 4 * GST_SECOND, "ssrc", G_MAXUINT32, NULL);

  validate_events_received (12);

  validate_normal_start_events (0);

  validate_normal_start_events (3);

  validate_normal_start_events (6);

  validate_normal_start_events (9);

  destroy_payloader (state);
}

GST_END_TEST;

/* validate that changing the timestamp-offset will actually effect the rtptime
 * of the payloaded RTP packets. unfortunately setting the timestamp-offset
 * property will only take effect when the payloader goes from READY to PAUSED.
 * so the test starts by making sure that the default timestamp-offset indicates
 * random timestamps. then a buffer is pushed which is expected to be payloaded
 * as an RTP packet with a random timestamp. then the timestamp-offset is
 * modified without changing the state of the pipeline. therefore the next
 * buffer pushed is expected to result in an RTP packet with a timestamp equal
 * to the previous RTP packet incremented by DEFAULT_CLOCK_RATE. next the
 * pipeline is brought to NULL state and the timestamp-offset is set to a
 * specific value, the pipeline is then brought back to PLAYING state and the
 * two buffers pushed are expected to result in payloaded RTP packets that have
 * timestamps based on the set timestamp-offset incremented by multiples of
 * DEFAULT_CLOCK_RATE. next the boundary values of the timestamp-offset are
 * tested. again the pipeline state needs to be modified and buffers are pushed
 * and the resulting payloaded RTP packets' timestamps are validated. note that
 * the maximum timestamp-offset value will wrap around for the very last
 * payloaded RTP packet.
 */
GST_START_TEST (rtp_base_payload_property_timestamp_offset_test)
{
  guint32 rtptime;
  guint32 offset;
  State *state;

  state = create_payloader ("application/x-rtp", &sinktmpl, NULL);

  set_state (state, GST_STATE_PLAYING);

  g_object_get (state->element, "timestamp-offset", &offset, NULL);
  fail_unless_equals_int (offset, -1);

  push_buffer (state, "pts", 0 * GST_SECOND, NULL);

  g_object_set (state->element, "timestamp-offset", 0x42, NULL);
  g_object_get (state->element, "timestamp-offset", &offset, NULL);
  fail_unless_equals_int (offset, 0x42);
  push_buffer (state, "pts", 1 * GST_SECOND, NULL);

  set_state (state, GST_STATE_NULL);
  g_object_set (state->element, "timestamp-offset", 0x4242, NULL);
  g_object_get (state->element, "timestamp-offset", &offset, NULL);
  fail_unless_equals_int (offset, 0x4242);
  set_state (state, GST_STATE_PLAYING);

  push_buffer (state, "pts", 2 * GST_SECOND, NULL);

  push_buffer (state, "pts", 3 * GST_SECOND, NULL);

  set_state (state, GST_STATE_NULL);
  g_object_set (state->element, "timestamp-offset", 0, NULL);
  set_state (state, GST_STATE_PLAYING);

  push_buffer (state, "pts", 4 * GST_SECOND, NULL);

  push_buffer (state, "pts", 5 * GST_SECOND, NULL);

  set_state (state, GST_STATE_NULL);
  g_object_set (state->element, "timestamp-offset", G_MAXUINT32, NULL);
  set_state (state, GST_STATE_PLAYING);

  push_buffer (state, "pts", 6 * GST_SECOND, NULL);

  push_buffer (state, "pts", 7 * GST_SECOND, NULL);

  set_state (state, GST_STATE_NULL);

  validate_buffers_received (8);

  validate_buffer (0, "pts", 0 * GST_SECOND, NULL);
  get_buffer_field (0, "rtptime", &rtptime, NULL);

  validate_buffer (1,
      "pts", 1 * GST_SECOND, "rtptime", rtptime + 1 * DEFAULT_CLOCK_RATE, NULL);

  validate_buffer (2,
      "pts", 2 * GST_SECOND, "rtptime", 0x4242 + 2 * DEFAULT_CLOCK_RATE, NULL);

  validate_buffer (3,
      "pts", 3 * GST_SECOND, "rtptime", 0x4242 + 3 * DEFAULT_CLOCK_RATE, NULL);

  validate_buffer (4,
      "pts", 4 * GST_SECOND, "rtptime", 0 + 4 * DEFAULT_CLOCK_RATE, NULL);

  validate_buffer (5,
      "pts", 5 * GST_SECOND, "rtptime", 0 + 5 * DEFAULT_CLOCK_RATE, NULL);

  validate_buffer (6,
      "pts", 6 * GST_SECOND, "rtptime", 6 * DEFAULT_CLOCK_RATE - 1, NULL);

  validate_buffer (7,
      "pts", 7 * GST_SECOND, "rtptime", 7 * DEFAULT_CLOCK_RATE - 1, NULL);

  validate_events_received (12);

  validate_normal_start_events (0);

  validate_normal_start_events (3);

  validate_normal_start_events (6);

  validate_normal_start_events (9);

  destroy_payloader (state);
}

GST_END_TEST;

/* as for timestamp-offset above setting the seqnum-offset property of a
 * payloader will only take effect when the payloader goes from READY to PAUSED
 * state. this test starts by validating that seqnum-offset indicates random
 * sequence numbers and that the random sequence numbers increment by one for
 * each payloaded RTP packet. also it is verified that setting seqnum-offset
 * without bringing the pipeline to READY will not affect the payloaded RTP
 * packets' sequence numbers. next the pipeline is brought to NULL state,
 * seqnum-offset is set to a specific value before bringing the pipeline back to
 * PLAYING state. the next two buffers pushed are expected to resulting in
 * payloaded RTP packets that start with sequence numbers relating to the set
 * seqnum-offset value, and that again increment by one for each packet. finally
 * the boundary values of seqnum-offset are tested. this means bringing the
 * pipeline to NULL state, setting the seqnum-offset and bringing the pipeline
 * back to PLAYING state. note that for the very last payloded RTP packet the
 * sequence number will have wrapped around because the previous packet is
 * expected to have the maximum sequence number value.
 */
GST_START_TEST (rtp_base_payload_property_seqnum_offset_test)
{
  State *state;
  guint16 seq;
  gint offset;

  state = create_payloader ("application/x-rtp", &sinktmpl, NULL);

  set_state (state, GST_STATE_PLAYING);

  g_object_get (state->element, "seqnum-offset", &offset, NULL);
  fail_unless_equals_int (offset, -1);

  push_buffer (state, "pts", 0 * GST_SECOND, NULL);

  g_object_set (state->element, "seqnum-offset", 0x42, NULL);
  g_object_get (state->element, "seqnum-offset", &offset, NULL);
  fail_unless_equals_int (offset, 0x42);
  push_buffer (state, "pts", 1 * GST_SECOND, NULL);

  set_state (state, GST_STATE_NULL);
  g_object_set (state->element, "seqnum-offset", 0x4242, NULL);
  g_object_get (state->element, "seqnum-offset", &offset, NULL);
  fail_unless_equals_int (offset, 0x4242);
  set_state (state, GST_STATE_PLAYING);

  push_buffer (state, "pts", 2 * GST_SECOND, NULL);

  push_buffer (state, "pts", 3 * GST_SECOND, NULL);

  set_state (state, GST_STATE_NULL);
  g_object_set (state->element, "seqnum-offset", -1, NULL);
  set_state (state, GST_STATE_PLAYING);

  push_buffer (state, "pts", 4 * GST_SECOND, NULL);

  push_buffer (state, "pts", 5 * GST_SECOND, NULL);

  set_state (state, GST_STATE_NULL);
  g_object_set (state->element, "seqnum-offset", G_MAXUINT16, NULL);
  set_state (state, GST_STATE_PLAYING);

  push_buffer (state, "pts", 6 * GST_SECOND, NULL);

  push_buffer (state, "pts", 7 * GST_SECOND, NULL);

  set_state (state, GST_STATE_NULL);

  validate_buffers_received (8);

  validate_buffer (0, "pts", 0 * GST_SECOND, NULL);
  get_buffer_field (0, "seq", &seq, NULL);

  validate_buffer (1, "pts", 1 * GST_SECOND, "seq", seq + 1, NULL);

  validate_buffer (2, "pts", 2 * GST_SECOND, "seq", 0x4242, NULL);

  validate_buffer (3, "pts", 3 * GST_SECOND, "seq", 0x4242 + 1, NULL);

  validate_buffer (4, "pts", 4 * GST_SECOND, NULL);
  get_buffer_field (4, "seq", &seq, NULL);

  validate_buffer (5, "pts", 5 * GST_SECOND, "seq", seq + 1, NULL);

  validate_buffer (6, "pts", 6 * GST_SECOND, "seq", G_MAXUINT16, NULL);

  validate_buffer (7, "pts", 7 * GST_SECOND, "seq", 0, NULL);

  validate_events_received (12);

  validate_normal_start_events (0);

  validate_normal_start_events (3);

  validate_normal_start_events (6);

  validate_normal_start_events (9);

  destroy_payloader (state);
}

GST_END_TEST;

/* a payloader's max-ptime property is linked to its MTU property. whenever a
 * packet is larger than MTU or has a duration longer than max-ptime it will be
 * considered to be full. so this test first validates that the default value of
 * max-ptime is unspecified. then it retrieves the MTU and validates that a
 * packet of size MTU will not be considered full even if the duration is at its
 * maximum value. however incrementing the size to exceed the MTU will result in
 * the packet being full. next max-ptime is set to a value and it is verified
 * that only if both the size and duration are below the allowed values then the
 * packet will be considered not to be full, otherwise it will be reported as
 * being full. finally the boundary values of the property are tested in a
 * similar fashion.
 */
GST_START_TEST (rtp_base_payload_property_max_ptime_test)
{
  gint64 max_ptime;
  State *state;
  guint mtu;

  state = create_payloader ("application/x-rtp", &sinktmpl, NULL);

  g_object_get (state->element, "max-ptime", &max_ptime, NULL);
  fail_unless_equals_int64 (max_ptime, -1);
  g_object_get (state->element, "mtu", &mtu, NULL);
  validate_would_not_be_filled (state, mtu, G_MAXINT64 - 1);
  validate_would_be_filled (state, mtu + 1, G_MAXINT64 - 1);

  g_object_set (state->element, "max-ptime", GST_SECOND, NULL);
  g_object_get (state->element, "max-ptime", &max_ptime, NULL);
  fail_unless_equals_int64 (max_ptime, GST_SECOND);
  validate_would_not_be_filled (state, mtu, GST_SECOND - 1);
  validate_would_be_filled (state, mtu, GST_SECOND);
  validate_would_be_filled (state, mtu + 1, GST_SECOND - 1);
  validate_would_be_filled (state, mtu + 1, GST_SECOND);

  g_object_set (state->element, "max-ptime", G_MAXUINT64, NULL);
  g_object_get (state->element, "max-ptime", &max_ptime, NULL);
  fail_unless_equals_int64 (max_ptime, G_MAXUINT64);
  validate_would_not_be_filled (state, mtu, G_MAXINT64 - 1);
  validate_would_be_filled (state, mtu + 1, G_MAXINT64 - 1);

  g_object_set (state->element, "max-ptime", G_MAXINT64, NULL);
  g_object_get (state->element, "max-ptime", &max_ptime, NULL);
  fail_unless_equals_int64 (max_ptime, G_MAXINT64);
  validate_would_be_filled (state, mtu, G_MAXINT64);

  destroy_payloader (state);
}

GST_END_TEST;

/* a basepayloader has a min-ptime property with an allowed range, the property
 * itself is never checked by the payloader but is meant to be used by
 * inheriting classes. therefore this test only validates that setting the
 * property will mean that retrieveing the property results in the value
 * previously being set. first the default value is validated, then a new
 * specific value, before finally testing the boundary values.
 */
GST_START_TEST (rtp_base_payload_property_min_ptime_test)
{
  State *state;
  guint64 reference, min_ptime;

  state = create_payloader ("application/x-rtp", &sinktmpl, NULL);

  g_object_get (state->element, "min-ptime", &reference, NULL);
  fail_unless_equals_int (reference, 0);

  g_object_set (state->element, "min-ptime", reference + 1, NULL);
  g_object_get (state->element, "min-ptime", &min_ptime, NULL);
  fail_unless_equals_int (min_ptime, reference + 1);

  g_object_set (state->element, "min-ptime", G_GUINT64_CONSTANT (0), NULL);
  g_object_get (state->element, "min-ptime", &min_ptime, NULL);
  fail_unless_equals_int (min_ptime, 0);

  g_object_set (state->element, "min-ptime", G_MAXINT64, NULL);
  g_object_get (state->element, "min-ptime", &min_ptime, NULL);
  fail_unless_equals_int64 (min_ptime, G_MAXINT64);

  destroy_payloader (state);
}

GST_END_TEST;

/* paylaoders have a timestamp property that reflects the timestamp of the last
 * payloaded RTP packet. in this test the timestamp-offset is set to a specific
 * value so that when the first buffer is pushed its timestamp can be predicted
 * and thus that the timestamp property also has this value. (if
 * timestamp-offset was not set the timestamp would be random). another buffer
 * is then pushed and its timestamp is expected to increment by
 * DEFAULT_CLOCK_RATE.
 */
GST_START_TEST (rtp_base_payload_property_timestamp_test)
{
  State *state;
  guint32 timestamp;

  state = create_payloader ("application/x-rtp", &sinktmpl,
      "timestamp-offset", 0, NULL);

  set_state (state, GST_STATE_PLAYING);

  push_buffer (state, "pts", 0 * GST_SECOND, NULL);
  g_object_get (state->element, "timestamp", &timestamp, NULL);
  fail_unless_equals_int (timestamp, 0);

  push_buffer (state, "pts", 1 * GST_SECOND, NULL);
  g_object_get (state->element, "timestamp", &timestamp, NULL);
  fail_unless_equals_int (timestamp, DEFAULT_CLOCK_RATE);

  set_state (state, GST_STATE_NULL);

  validate_buffers_received (2);

  validate_buffer (0, "pts", 0 * GST_SECOND, "rtptime", 0, NULL);

  validate_buffer (1,
      "pts", 1 * GST_SECOND, "rtptime", DEFAULT_CLOCK_RATE, NULL);

  validate_events_received (3);

  validate_normal_start_events (0);

  destroy_payloader (state);
}

GST_END_TEST;

/* basepayloaders have a seqnum property that is supposed to contain the
 * sequence number of the last payloaded RTP packet. so therefore this test
 * initializes the seqnum-offset property to a know value and pushes a buffer.
 * the payloaded RTP packet is expected to have a sequence number equal to the
 * set seqnum-offset, as is the seqnum property. next another buffer is pushed
 * and then both the payloaded RTP packet and the seqnum property value are
 * expected to increment by one compared to the previous packet.
 */
GST_START_TEST (rtp_base_payload_property_seqnum_test)
{
  State *state;
  guint seq;

  state = create_payloader ("application/x-rtp", &sinktmpl,
      "seqnum-offset", 0, NULL);

  set_state (state, GST_STATE_PLAYING);

  push_buffer (state, "pts", 0 * GST_SECOND, NULL);
  g_object_get (state->element, "seqnum", &seq, NULL);
  fail_unless_equals_int (seq, 0);

  push_buffer (state, "pts", 1 * GST_SECOND, NULL);
  g_object_get (state->element, "seqnum", &seq, NULL);
  fail_unless_equals_int (seq, 1);

  set_state (state, GST_STATE_NULL);

  validate_buffers_received (2);

  validate_buffer (0, "pts", 0 * GST_SECOND, "seq", 0, NULL);

  validate_buffer (1, "pts", 1 * GST_SECOND, "seq", 1, NULL);

  validate_events_received (3);

  validate_normal_start_events (0);

  destroy_payloader (state);
}

GST_END_TEST;

/* basepayloader has a perfect-rtptime property when it is set to FALSE
 * the timestamps of payloaded RTP packets will determined by initial
 * timestamp-offset (usually random) as well as the clock-rate. when
 * perfect-rtptime is set to TRUE the timestamps of payloaded RTP packets are
 * instead determined by the timestamp of the first packet and then the
 * difference in offset of the input buffers.
 *
 * to verify that this test starts by setting the timestamp-offset to a specific
 * value to prevent random timestamps of the RTP packets. next perfect-rtptime
 * is set to FALSE. the two buffers pushed will result in two payloaded RTP
 * packets whose timestamps differ based on the current clock-rate
 * DEFAULT_CLOCK_RATE. the next step is to set perfect-rtptime to TRUE. the two
 * buffers that are pushed will result in two payloaded RTP packets. the first
 * of these RTP packets has a timestamp that relates to the previous packet and
 * the difference in offset between the middle two input buffers. the latter of
 * the two RTP packets has a timestamp that instead relates to the offset of the
 * last two input buffers.
 */
GST_START_TEST (rtp_base_payload_property_perfect_rtptime_test)
{
  State *state;
  guint32 timestamp_base = 0;
  gboolean perfect;

  state = create_payloader ("application/x-rtp", &sinktmpl,
      "timestamp-offset", timestamp_base, NULL);

  set_state (state, GST_STATE_PLAYING);

  g_object_set (state->element, "perfect-rtptime", FALSE, NULL);
  g_object_get (state->element, "perfect-rtptime", &perfect, NULL);
  fail_unless (!perfect);

  push_buffer (state, "pts", 0 * GST_SECOND, "offset", G_GINT64_CONSTANT (0),
      NULL);

  push_buffer (state, "pts", 1 * GST_SECOND, "offset", G_GINT64_CONSTANT (17),
      NULL);

  g_object_set (state->element, "perfect-rtptime", TRUE, NULL);
  g_object_get (state->element, "perfect-rtptime", &perfect, NULL);
  fail_unless (perfect);

  push_buffer (state, "pts", 2 * GST_SECOND, "offset", G_GINT64_CONSTANT (31),
      NULL);

  push_buffer (state, "pts", 3 * GST_SECOND, "offset", G_GINT64_CONSTANT (67),
      NULL);

  set_state (state, GST_STATE_NULL);

  validate_buffers_received (4);

  validate_buffer (0,
      "pts", 0 * GST_SECOND, "offset", G_GINT64_CONSTANT (0), "rtptime",
      timestamp_base, NULL);

  validate_buffer (1,
      "pts", 1 * GST_SECOND,
      "offset", G_GINT64_CONSTANT (17), "rtptime",
      timestamp_base + 1 * DEFAULT_CLOCK_RATE, NULL);

  validate_buffer (2,
      "pts", 2 * GST_SECOND,
      "offset", G_GINT64_CONSTANT (31),
      "rtptime", timestamp_base + 1 * DEFAULT_CLOCK_RATE + (31 - 17), NULL);

  validate_buffer (3,
      "pts", 3 * GST_SECOND,
      "offset", G_GINT64_CONSTANT (67),
      "rtptime", timestamp_base + 1 * DEFAULT_CLOCK_RATE + (67 - 17), NULL);

  validate_events_received (3);

  validate_normal_start_events (0);

  destroy_payloader (state);
}

GST_END_TEST;

/* basepayloaders have a ptime-multiple property but its value does not affect
 * any payloaded RTP packets as this is supposed to be done by inherited
 * classes. therefore this test only validates the default value of the
 * property, makes sure that a set value actually sticks and that the boundary
 * values are indeed allowed to be set.
 */
GST_START_TEST (rtp_base_payload_property_ptime_multiple_test)
{
  State *state;
  gint64 multiple;

  state = create_payloader ("application/x-rtp", &sinktmpl, NULL);

  g_object_get (state->element, "ptime-multiple", &multiple, NULL);
  fail_unless_equals_int64 (multiple, 0);

  g_object_set (state->element, "ptime-multiple", G_GINT64_CONSTANT (42), NULL);
  g_object_get (state->element, "ptime-multiple", &multiple, NULL);
  fail_unless_equals_int64 (multiple, 42);

  g_object_set (state->element, "ptime-multiple", G_GINT64_CONSTANT (0), NULL);
  g_object_get (state->element, "ptime-multiple", &multiple, NULL);
  fail_unless_equals_int64 (multiple, 0);

  g_object_set (state->element, "ptime-multiple", G_MAXINT64, NULL);
  g_object_get (state->element, "ptime-multiple", &multiple, NULL);
  fail_unless_equals_int64 (multiple, G_MAXINT64);

  destroy_payloader (state);
}

GST_END_TEST;

/* basepayloaders have a property called stats that is used to atomically
 * retrieve several values (clock-rate, running-time, seqnum and timestamp) that
 * relate to the stream and its current progress. this test is meant to test
 * retrieval of these values.
 *
 * first of all perfect-rtptime is set to TRUE, next the the test starts out by
 * setting seqnum-offset and timestamp-offset to known values to prevent that
 * sequence numbers and timestamps of payloaded RTP packets are random. next the
 * stats property is retrieved. the clock-rate must be at the default
 * DEFAULT_CLOCK_RATE, while running-time must be equal to the first buffers
 * PTS. the sequence number should be equal to the initialized value of
 * seqnum-offset and the timestamp should be equal to the initialized value of
 * timestamp-offset. after pushing a second buffer the stats property is
 * validate again. this time running-time, seqnum and timestamp should have
 * advanced as expected. next the pipeline is brought to NULL state to be able
 * to change the perfect-rtptime property to FALSE before going back to PLAYING
 * state. this is done to validate that the stats values reflect normal
 * timestamp updates that are not based on input buffer offsets as expected.
 * lastly two buffers are pushed and the stats property retrieved after each
 * time. here it is expected that the sequence numbers values are restarted at
 * the initial value while the timestamps and running-time reflect the input
 * buffers.
 */
GST_START_TEST (rtp_base_payload_property_stats_test)
{
  State *state;

  state = create_payloader ("application/x-rtp", &sinktmpl,
      "perfect-rtptime", TRUE, "seqnum-offset", 0, "timestamp-offset", 0, NULL);

  set_state (state, GST_STATE_PLAYING);

  push_buffer (state, "pts", 0 * GST_SECOND, NULL);
  validate_stats (state,
      DEFAULT_CLOCK_RATE, 0 * GST_SECOND, 0, 0 * DEFAULT_CLOCK_RATE);

  push_buffer (state, "pts", 1 * GST_SECOND, NULL);
  validate_stats (state,
      DEFAULT_CLOCK_RATE, 1 * DEFAULT_CLOCK_RATE, 1, 1 * DEFAULT_CLOCK_RATE);

  set_state (state, GST_STATE_NULL);
  g_object_set (state->element, "perfect-rtptime", FALSE, NULL);
  set_state (state, GST_STATE_PLAYING);

  push_buffer (state, "pts", 2 * GST_SECOND, NULL);
  validate_stats (state,
      DEFAULT_CLOCK_RATE, 2 * GST_SECOND, 0, 2 * DEFAULT_CLOCK_RATE);

  push_buffer (state, "pts", 3 * GST_SECOND, NULL);
  validate_stats (state,
      DEFAULT_CLOCK_RATE, 3 * GST_SECOND, 1, 3 * DEFAULT_CLOCK_RATE);
  set_state (state, GST_STATE_NULL);

  validate_buffers_received (4);

  validate_buffer (0, "pts", 0 * GST_SECOND, NULL);

  validate_buffer (1, "pts", 1 * GST_SECOND, NULL);

  validate_buffer (2, "pts", 2 * GST_SECOND, NULL);

  validate_buffer (3, "pts", 3 * GST_SECOND, NULL);

  validate_events_received (6);

  validate_normal_start_events (0);

  validate_normal_start_events (3);

  destroy_payloader (state);
}

GST_END_TEST;

/* basepayloader has a property source-info that makes it aware of RTP
 * source information passed as GstRTPSourceMeta on the input buffers. All
 * sources found in the meta will be added to the list of CSRCs in the RTP
 * header. A useful scenario for this is, for instance, to signal which
 * sources contributed to a mixed audio stream. */
GST_START_TEST (rtp_base_payload_property_source_info_test)
{
  GstHarness *h;
  GstRtpDummyPay *pay;
  GstBuffer *buffer;
  guint csrc_count = 2;
  const guint32 csrc[] = { 0x11, 0x22 };
  const guint32 ssrc = 0x33;

  pay = rtp_dummy_pay_new ();
  h = gst_harness_new_with_element (GST_ELEMENT_CAST (pay), "sink", "src");
  gst_harness_set_src_caps_str (h, "application/x-rtp");

  /* Input buffer has no meta, payloader should not add CSRC */
  g_object_set (pay, "source-info", TRUE, NULL);
  buffer = gst_rtp_buffer_new_allocate (0, 0, 0);
  buffer = gst_harness_push_and_pull (h, buffer);
  validate_buffer1 (buffer, "csrc-count", 0, NULL);
  fail_if (gst_buffer_get_rtp_source_meta (buffer));
  gst_buffer_unref (buffer);

  /* Input buffer has meta, payloader should add CSRC */
  buffer = gst_rtp_buffer_new_allocate (0, 0, 0);
  fail_unless (gst_buffer_add_rtp_source_meta (buffer, &ssrc, csrc,
          csrc_count));
  buffer = gst_harness_push_and_pull (h, buffer);
  /* The meta SSRC should be added as the last contributing source */
  validate_buffer1 (buffer, "csrc-count", 3, "csrc", 0, csrc[0],
      "csrc", 1, csrc[1], "csrc", 2, ssrc, NULL);
  fail_if (gst_buffer_get_rtp_source_meta (buffer));
  gst_buffer_unref (buffer);

  /* When property is disabled, the meta should be ignored and no CSRC
   * added. */
  g_object_set (pay, "source-info", FALSE, NULL);
  buffer = gst_rtp_buffer_new_allocate (0, 0, 0);
  fail_unless (gst_buffer_add_rtp_source_meta (buffer, NULL, csrc, csrc_count));
  buffer = gst_harness_push_and_pull (h, buffer);
  validate_buffer1 (buffer, "csrc-count", 0, NULL);
  fail_if (gst_buffer_get_rtp_source_meta (buffer));
  gst_buffer_unref (buffer);

  g_object_unref (pay);
  gst_harness_teardown (h);
}

GST_END_TEST;

/* push a single buffer to the payloader which should successfully payload it
 * into an RTP packet. besides the payloaded RTP packet there should be the
 * three events initial events: stream-start, caps and segment. because of that
 * the input caps has framerate this will be propagated to an a-framerate field
 * on the output caps.
 */
GST_START_TEST (rtp_base_payload_framerate_attribute)
{
  State *state;

  state = create_payloader ("video/x-raw,framerate=(fraction)1/4", &sinktmpl,
      "perfect-rtptime", FALSE, NULL);

  set_state (state, GST_STATE_PLAYING);

  push_buffer (state, "pts", 0 * GST_SECOND, NULL);

  set_state (state, GST_STATE_NULL);

  validate_buffers_received (1);

  validate_buffer (0, "pts", 0 * GST_SECOND, NULL);

  validate_events_received (3);

  validate_normal_start_events (0);

  validate_event (1, "caps", "a-framerate", "0.25", NULL);

  destroy_payloader (state);
}

GST_END_TEST;

/* push a single buffer to the payloader which should successfully payload it
 * into an RTP packet. besides the payloaded RTP packet there should be the
 * three events initial events: stream-start, caps and segment. because of that
 * the input caps has both framerate and max-framerate set the a-framerate field
 * on the output caps will correspond to the value of the max-framerate field.
 */
GST_START_TEST (rtp_base_payload_max_framerate_attribute)
{
  State *state;

  state =
      create_payloader
      ("video/x-raw,framerate=(fraction)0/1,max-framerate=(fraction)1/8",
      &sinktmpl, "perfect-rtptime", FALSE, NULL);

  set_state (state, GST_STATE_PLAYING);

  push_buffer (state, "pts", 0 * GST_SECOND, NULL);

  set_state (state, GST_STATE_NULL);

  validate_buffers_received (1);

  validate_buffer (0, "pts", 0 * GST_SECOND, NULL);

  validate_events_received (3);

  validate_normal_start_events (0);

  validate_event (1, "caps", "a-framerate", "0.125", NULL);

  destroy_payloader (state);
}

GST_END_TEST;

GST_START_TEST (rtp_base_payload_segment_time)
{
  State *state;
  guint32 timestamp_base = 0;
  guint segment_time = 10;
  GstEvent *event;
  GstSegment *segment = gst_segment_new ();

  state =
      create_payloader
      ("application/x-rtp",
      &sinktmpl, "onvif-no-rate-control", TRUE, "timestamp-offset",
      timestamp_base, NULL);

  set_state (state, GST_STATE_PLAYING);

  gst_segment_init (segment, GST_FORMAT_TIME);
  segment->time = segment_time * GST_SECOND;
  event = gst_event_new_segment (segment);
  fail_unless (gst_pad_push_event (state->srcpad, event));

  push_buffer (state, "pts", 0 * GST_SECOND, NULL);
  push_buffer (state, "pts", 1 * GST_SECOND, NULL);
  push_buffer (state, "pts", 2 * GST_SECOND, NULL);
  push_buffer (state, "pts", 3 * GST_SECOND, NULL);

  set_state (state, GST_STATE_NULL);

  validate_buffers_received (4);

  validate_buffer (0, "rtptime",
      timestamp_base + (segment_time) * DEFAULT_CLOCK_RATE, NULL);
  validate_buffer (1, "rtptime",
      timestamp_base + (1 + segment_time) * DEFAULT_CLOCK_RATE, NULL);
  validate_buffer (2, "rtptime",
      timestamp_base + (2 + segment_time) * DEFAULT_CLOCK_RATE, NULL);
  validate_buffer (3, "rtptime",
      timestamp_base + (3 + segment_time) * DEFAULT_CLOCK_RATE, NULL);

  destroy_payloader (state);
  gst_segment_free (segment);
}

GST_END_TEST;

GST_START_TEST (rtp_base_payload_one_byte_hdr_ext)
{
  GstRTPHeaderExtension *ext;
  State *state;

  state = create_payloader ("application/x-rtp", &sinktmpl, NULL);
  ext = rtp_dummy_hdr_ext_new ();
  GST_RTP_DUMMY_HDR_EXT (ext)->supported_flags =
      GST_RTP_HEADER_EXTENSION_ONE_BYTE;
  gst_rtp_header_extension_set_id (ext, 1);

  g_signal_emit_by_name (state->element, "add-extension", ext);

  set_state (state, GST_STATE_PLAYING);

  push_buffer (state, "pts", 0 * GST_SECOND, NULL);

  set_state (state, GST_STATE_NULL);

  validate_buffers_received (1);

  validate_buffer (0, "pts", 0 * GST_SECOND, NULL);

  validate_events_received (3);

  validate_normal_start_events (0);

  fail_unless_equals_int (GST_RTP_DUMMY_HDR_EXT (ext)->write_count, 1);

  gst_object_unref (ext);
  destroy_payloader (state);
}

GST_END_TEST;

GST_START_TEST (rtp_base_payload_two_byte_hdr_ext)
{
  GstRTPHeaderExtension *ext;
  State *state;

  state = create_payloader ("application/x-rtp", &sinktmpl, NULL);
  ext = rtp_dummy_hdr_ext_new ();
  GST_RTP_DUMMY_HDR_EXT (ext)->supported_flags =
      GST_RTP_HEADER_EXTENSION_TWO_BYTE;
  gst_rtp_header_extension_set_id (ext, 1);

  g_signal_emit_by_name (state->element, "add-extension", ext);

  set_state (state, GST_STATE_PLAYING);

  push_buffer (state, "pts", 0 * GST_SECOND, NULL);

  set_state (state, GST_STATE_NULL);

  validate_buffers_received (1);

  validate_buffer (0, "pts", 0 * GST_SECOND, NULL);

  validate_events_received (3);

  validate_normal_start_events (0);

  fail_unless_equals_int (GST_RTP_DUMMY_HDR_EXT (ext)->write_count, 1);

  gst_object_unref (ext);
  destroy_payloader (state);
}

GST_END_TEST;

GST_START_TEST (rtp_base_payload_clear_extensions)
{
  GstRTPHeaderExtension *ext;
  State *state;

  state = create_payloader ("application/x-rtp", &sinktmpl, NULL);
  ext = rtp_dummy_hdr_ext_new ();
  gst_rtp_header_extension_set_id (ext, 1);

  g_signal_emit_by_name (state->element, "add-extension", ext);

  set_state (state, GST_STATE_PLAYING);

  push_buffer (state, "pts", 0 * GST_SECOND, NULL);
  g_signal_emit_by_name (state->element, "clear-extensions", ext);
  push_buffer (state, "pts", 1 * GST_SECOND, NULL);

  set_state (state, GST_STATE_NULL);

  validate_buffers_received (2);

  validate_buffer (0, "pts", 0 * GST_SECOND, NULL);
  validate_buffer (1, "pts", 1 * GST_SECOND, NULL);

  validate_events_received (3);

  validate_normal_start_events (0);

  fail_unless_equals_int (GST_RTP_DUMMY_HDR_EXT (ext)->write_count, 1);

  gst_object_unref (ext);
  destroy_payloader (state);
}

GST_END_TEST;

GST_START_TEST (rtp_base_payload_multiple_exts)
{
  GstRTPHeaderExtension *ext1, *ext2;
  State *state;

  state = create_payloader ("application/x-rtp", &sinktmpl, NULL);
  ext1 = rtp_dummy_hdr_ext_new ();
  gst_rtp_header_extension_set_id (ext1, 1);
  ext2 = rtp_dummy_hdr_ext_new ();
  gst_rtp_header_extension_set_id (ext2, 2);

  g_signal_emit_by_name (state->element, "add-extension", ext1);
  g_signal_emit_by_name (state->element, "add-extension", ext2);

  set_state (state, GST_STATE_PLAYING);

  push_buffer (state, "pts", 0 * GST_SECOND, NULL);

  set_state (state, GST_STATE_NULL);

  validate_buffers_received (1);

  validate_buffer (0, "pts", 0 * GST_SECOND, NULL);

  validate_events_received (3);

  validate_normal_start_events (0);

  fail_unless_equals_int (GST_RTP_DUMMY_HDR_EXT (ext1)->write_count, 1);
  fail_unless_equals_int (GST_RTP_DUMMY_HDR_EXT (ext2)->write_count, 1);

  gst_object_unref (ext1);
  gst_object_unref (ext2);
  destroy_payloader (state);
}

GST_END_TEST;

static GstStaticPadTemplate sinktmpl_with_extmap_str =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-rtp, payload=(int)98, ssrc=(uint)24, "
        "timestamp-offset=(uint)212, seqnum-offset=(uint)2424, extmap-4=(string)"
        DUMMY_HDR_EXT_URI));

static GstRTPHeaderExtension *
request_extension (GstRTPBasePayload * depayload, guint ext_id,
    const gchar * ext_uri, gpointer user_data)
{
  GstRTPHeaderExtension *ext = user_data;

  if (ext && gst_rtp_header_extension_get_id (ext) == ext_id
      && g_strcmp0 (ext_uri, gst_rtp_header_extension_get_uri (ext)) == 0)
    return gst_object_ref (ext);

  return NULL;
}

GST_START_TEST (rtp_base_payload_caps_request)
{
  GstRTPHeaderExtension *ext;
  GstRTPDummyHdrExt *dummy;
  State *state;

  state =
      create_payloader ("application/x-rtp", &sinktmpl_with_extmap_str, NULL);

  ext = rtp_dummy_hdr_ext_new ();
  dummy = GST_RTP_DUMMY_HDR_EXT (ext);
  gst_rtp_header_extension_set_id (ext, 4);
  g_signal_connect (state->element, "request-extension",
      G_CALLBACK (request_extension), ext);

  fail_unless (dummy->set_attributes_count == 0);

  set_state (state, GST_STATE_PLAYING);

  push_buffer (state, "pts", 0 * GST_SECOND, NULL);

  set_state (state, GST_STATE_NULL);

  validate_buffers_received (1);

  validate_buffer (0, "pts", 0 * GST_SECOND, NULL);

  validate_events_received (3);

  validate_normal_start_events (0);

  fail_unless_equals_int (GST_RTP_DUMMY_HDR_EXT (ext)->write_count, 1);
  fail_unless (dummy->set_attributes_count == 1);

  gst_object_unref (ext);
  destroy_payloader (state);
}

GST_END_TEST;

static GstRTPHeaderExtension *
request_extension_ignored (GstRTPBasePayload * depayload, guint ext_id,
    const gchar * ext_uri, gpointer user_data)
{
  guint *request_counter = user_data;

  *request_counter += 1;

  return NULL;
}

GST_START_TEST (rtp_base_payload_caps_request_ignored)
{
  State *state;
  guint request_counter = 0;

  state =
      create_payloader ("application/x-rtp", &sinktmpl_with_extmap_str, NULL);

  g_signal_connect (state->element, "request-extension",
      G_CALLBACK (request_extension_ignored), &request_counter);

  set_state (state, GST_STATE_PLAYING);

  push_buffer (state, "pts", 0 * GST_SECOND, NULL);

  set_state (state, GST_STATE_NULL);

  fail_unless_equals_int (request_counter, 1);

  validate_buffers_received (1);

  destroy_payloader (state);
}

GST_END_TEST;

GST_START_TEST (rtp_base_payload_extensions_in_output_caps)
{
  GstRTPHeaderExtension *ext;
  State *state;

  state = create_payloader ("application/x-rtp", &sinktmpl, NULL);
  ext = rtp_dummy_hdr_ext_new ();
  GST_RTP_DUMMY_HDR_EXT (ext)->supported_flags =
      GST_RTP_HEADER_EXTENSION_TWO_BYTE;
  gst_rtp_header_extension_set_id (ext, 1);

  g_signal_emit_by_name (state->element, "add-extension", ext);

  set_state (state, GST_STATE_PLAYING);

  push_buffer (state, "pts", 0 * GST_SECOND, NULL);

  set_state (state, GST_STATE_NULL);

  validate_buffers_received (1);

  validate_buffer (0, "pts", 0 * GST_SECOND, NULL);

  validate_events_received (3);

  validate_normal_start_events (0);

  validate_event (1, "caps", "extmap-str", 1, DUMMY_HDR_EXT_URI, NULL);

  fail_unless_equals_int (GST_RTP_DUMMY_HDR_EXT (ext)->write_count, 1);
  gst_object_unref (ext);
  ext = NULL;

  destroy_payloader (state);
}

GST_END_TEST;

GST_START_TEST (rtp_base_payload_extensions_shrink_ext_data)
{
  GstRTPHeaderExtension *ext;
  State *state;

  state = create_payloader ("application/x-rtp", &sinktmpl, NULL);
  ext = rtp_dummy_hdr_ext_new ();
  GST_RTP_DUMMY_HDR_EXT (ext)->supported_flags =
      GST_RTP_HEADER_EXTENSION_ONE_BYTE;
  GST_RTP_DUMMY_HDR_EXT (ext)->max_size = 5;
  gst_rtp_header_extension_set_id (ext, 1);

  g_signal_emit_by_name (state->element, "add-extension", ext);

  set_state (state, GST_STATE_PLAYING);

  push_buffer (state, "pts", 0 * GST_SECOND, NULL);

  set_state (state, GST_STATE_NULL);

  validate_buffers_received (1);

  validate_buffer (0, "pts", 0 * GST_SECOND, "size", (gsize) 20, "ext-data",
      (guint) 0xBEDE, (gsize) 4, NULL);

  validate_events_received (3);

  validate_normal_start_events (0);

  fail_unless_equals_int (GST_RTP_DUMMY_HDR_EXT (ext)->write_count, 1);
  gst_object_unref (ext);
  ext = NULL;

  destroy_payloader (state);
}

GST_END_TEST;

GST_START_TEST (rtp_base_payload_extensions_get_enabled)
{
  GstElement *pay = GST_ELEMENT (rtp_dummy_pay_new ());
  GstRTPHeaderExtension *ext = rtp_dummy_hdr_ext_new ();
  GValue extensions = G_VALUE_INIT;
  const GValue *returned_ext;

  g_signal_emit_by_name (pay, "add-extension", ext, NULL);
  g_object_get_property (G_OBJECT (pay), "extensions", &extensions);

  fail_unless_equals_int (gst_value_array_get_size (&extensions), 1);

  returned_ext = gst_value_array_get_value (&extensions, 0);

  fail_unless (G_VALUE_HOLDS (returned_ext, GST_TYPE_RTP_HEADER_EXTENSION));
  fail_unless (g_value_get_object (returned_ext) == ext);

  g_value_unset (&extensions);
  gst_object_unref (ext);
  gst_object_unref (pay);
}

GST_END_TEST;

static void
on_extensions_changed (GObject * sender, GParamSpec * pspec,
    guint * invocation_count)
{
  (void) sender;
  (void) pspec;
  (*invocation_count)++;
}

GST_START_TEST (rtp_base_payload_extensions_notify_on_change)
{
  GstElement *pay = GST_ELEMENT (rtp_dummy_pay_new ());
  GstRTPHeaderExtension *ext = rtp_dummy_hdr_ext_new ();
  guint invocation_count = 0;

  g_signal_connect (pay, "notify::extensions",
      G_CALLBACK (on_extensions_changed), &invocation_count);

  g_signal_emit_by_name (pay, "add-extension", ext, NULL);
  fail_unless_equals_int (invocation_count, 1);

  g_signal_emit_by_name (pay, "clear-extensions", NULL);
  fail_unless_equals_int (invocation_count, 2);

  gst_object_unref (ext);
  gst_object_unref (pay);
}

GST_END_TEST;

static Suite *
rtp_basepayloading_suite (void)
{
  Suite *s = suite_create ("rtp_base_payloading_test");
  TCase *tc_chain = tcase_create ("payloading tests");

  tcase_set_timeout (tc_chain, 60);

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, rtp_base_payload_buffer_test);
  tcase_add_test (tc_chain, rtp_base_payload_buffer_list_test);

  tcase_add_test (tc_chain, rtp_base_payload_normal_rtptime_test);
  tcase_add_test (tc_chain, rtp_base_payload_perfect_rtptime_test);
  tcase_add_test (tc_chain, rtp_base_payload_no_pts_no_offset_test);

  tcase_add_test (tc_chain, rtp_base_payload_downstream_caps_test);

  tcase_add_test (tc_chain, rtp_base_payload_ssrc_collision_test);
  tcase_add_test (tc_chain, rtp_base_payload_reconfigure_test);

  tcase_add_test (tc_chain, rtp_base_payload_property_mtu_test);
  tcase_add_test (tc_chain, rtp_base_payload_property_pt_test);
  tcase_add_test (tc_chain, rtp_base_payload_property_ssrc_test);
  tcase_add_test (tc_chain, rtp_base_payload_property_timestamp_offset_test);
  tcase_add_test (tc_chain, rtp_base_payload_property_seqnum_offset_test);
  tcase_add_test (tc_chain, rtp_base_payload_property_max_ptime_test);
  tcase_add_test (tc_chain, rtp_base_payload_property_min_ptime_test);
  tcase_add_test (tc_chain, rtp_base_payload_property_timestamp_test);
  tcase_add_test (tc_chain, rtp_base_payload_property_seqnum_test);
  tcase_add_test (tc_chain, rtp_base_payload_property_perfect_rtptime_test);
  tcase_add_test (tc_chain, rtp_base_payload_property_ptime_multiple_test);
  tcase_add_test (tc_chain, rtp_base_payload_property_stats_test);
  tcase_add_test (tc_chain, rtp_base_payload_property_source_info_test);

  tcase_add_test (tc_chain, rtp_base_payload_framerate_attribute);
  tcase_add_test (tc_chain, rtp_base_payload_max_framerate_attribute);

  tcase_add_test (tc_chain, rtp_base_payload_segment_time);

  tcase_add_test (tc_chain, rtp_base_payload_one_byte_hdr_ext);
  tcase_add_test (tc_chain, rtp_base_payload_two_byte_hdr_ext);
  tcase_add_test (tc_chain, rtp_base_payload_clear_extensions);
  tcase_add_test (tc_chain, rtp_base_payload_multiple_exts);
  tcase_add_test (tc_chain, rtp_base_payload_caps_request);
  tcase_add_test (tc_chain, rtp_base_payload_caps_request_ignored);
  tcase_add_test (tc_chain, rtp_base_payload_extensions_in_output_caps);
  tcase_add_test (tc_chain, rtp_base_payload_extensions_shrink_ext_data);
  tcase_add_test (tc_chain, rtp_base_payload_extensions_get_enabled);
  tcase_add_test (tc_chain, rtp_base_payload_extensions_notify_on_change);

  return s;
}

GST_CHECK_MAIN (rtp_basepayloading)
