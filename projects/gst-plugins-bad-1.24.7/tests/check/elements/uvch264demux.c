/* GStreamer
 *
 * unit test for uvch264_demux
 *
 * Copyright (C) <2012> Collabora Ltd.
 *   Author: Youness Alaoui <youness.alaoui@collabora.co.uk>
 * Copyright (C) <2008> Sebastian Dröge <sebastian.droege@collabora.co.uk>
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

#include <gst/check/gstcheck.h>
#include <string.h>

static GstElement *demux;
static GstPad *mjpg_pad, *h264_pad, *yuy2_pad, *nv12_pad, *jpg_pad;
static gboolean have_h264_eos, have_yuy2_eos, have_nv12_eos, have_jpg_eos;
static GstBuffer *buffer_h264, *buffer_yuy2, *buffer_nv12, *buffer_jpg;
static GstCaps *negotiated_caps_h264, *negotiated_caps_yuy2;
static GstCaps *negotiated_caps_nv12, *negotiated_caps_jpg;
static GError *gerror;
static gchar *error_debug;

static GstStaticPadTemplate mjpg_template =
GST_STATIC_PAD_TEMPLATE ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("image/jpeg, width=640, height=480, framerate=15/1"));

static GstStaticPadTemplate sink_template =
GST_STATIC_PAD_TEMPLATE ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

#define STRINGIFY_(x) #x
#define STRINGIFY(x) STRINGIFY_ (x)
#define DATADIR STRINGIFY (UVCH264DEMUX_DATADIR)
#define VALID_H264_JPG_MJPG_FILENAME  DATADIR "/valid_h264_jpg.mjpg"
#define VALID_H264_JPG_JPG_FILENAME   DATADIR "/valid_h264_jpg.jpg"
#define VALID_H264_JPG_H264_FILENAME  DATADIR "/valid_h264_jpg.h264"
#define VALID_H264_YUY2_MJPG_FILENAME DATADIR "/valid_h264_yuy2.mjpg"
#define VALID_H264_YUY2_YUY2_FILENAME DATADIR "/valid_h264_yuy2.yuy2"
#define VALID_H264_YUY2_H264_FILENAME DATADIR "/valid_h264_yuy2.h264"

#define _sink_chain_func(type)                                          \
static GstFlowReturn                                                    \
 _sink_##type##_chain (GstPad * pad, GstObject * parent, GstBuffer * buffer) \
{                                                                       \
  fail_unless (negotiated_caps_##type != NULL);                         \
                                                                        \
  buffer_##type = buffer;                                               \
                                                                        \
  return GST_FLOW_OK;                                                   \
}

#define _sink_event_func(type)                          \
static gboolean                                         \
 _sink_##type##_event (GstPad * pad, GstObject * parent, GstEvent * event)  \
{                                                       \
  if (GST_EVENT_TYPE (event) == GST_EVENT_EOS)          \
    have_##type##_eos = TRUE;                           \
                                                        \
  if (GST_EVENT_TYPE (event) == GST_EVENT_CAPS) {       \
    gst_event_parse_caps (event, &negotiated_caps_##type); \
    gst_caps_ref (negotiated_caps_##type);              \
  }                                                     \
                                                        \
  gst_event_unref (event);                              \
                                                        \
  return TRUE;                                          \
}


_sink_chain_func (h264);
_sink_chain_func (yuy2);
_sink_chain_func (nv12);
_sink_chain_func (jpg);

_sink_event_func (h264);
_sink_event_func (yuy2);
_sink_event_func (nv12);
_sink_event_func (jpg);


static GstBusSyncReply
_bus_sync_handler (GstBus * bus, GstMessage * message, gpointer data)
{
  if (GST_MESSAGE_TYPE (message) == GST_MESSAGE_ERROR) {
    fail_unless (gerror == NULL && error_debug == NULL);
    fail_unless (GST_MESSAGE_SRC (message) == GST_OBJECT (demux));
    gst_message_parse_error (message, &gerror, &error_debug);
  }
  return GST_BUS_PASS;
}

static void
_teardown_test (void)
{
  GstBus *bus;
  gst_element_set_state (demux, GST_STATE_NULL);

  bus = GST_ELEMENT_BUS (demux);
  gst_bus_set_flushing (bus, TRUE);
  gst_object_unref (bus);

  gst_pad_set_active (mjpg_pad, FALSE);
  gst_object_unref (mjpg_pad);
  if (h264_pad) {
    gst_pad_set_active (h264_pad, FALSE);
    gst_object_unref (h264_pad);
  }
  if (yuy2_pad) {
    gst_pad_set_active (yuy2_pad, FALSE);
    gst_object_unref (yuy2_pad);
  }
  if (nv12_pad) {
    gst_pad_set_active (nv12_pad, FALSE);
    gst_object_unref (nv12_pad);
  }
  if (jpg_pad) {
    gst_pad_set_active (jpg_pad, FALSE);
    gst_object_unref (jpg_pad);
  }
  if (gerror) {
    g_error_free (gerror);
    gerror = NULL;
  }
  if (error_debug) {
    g_free (error_debug);
    error_debug = NULL;
  }

  gst_object_unref (demux);
  mjpg_pad = h264_pad = yuy2_pad = nv12_pad = jpg_pad = NULL;
  demux = NULL;

  gst_caps_replace (&negotiated_caps_h264, NULL);
  gst_caps_replace (&negotiated_caps_yuy2, NULL);
  gst_caps_replace (&negotiated_caps_nv12, NULL);
  gst_caps_replace (&negotiated_caps_jpg, NULL);
}

static void
_setup_test (gboolean link_h264, gboolean link_yuy2, gboolean link_nv12,
    gboolean link_jpg)
{
  GstCaps *caps;
  GstPad *sinkpad, *h264pad, *yuy2pad, *nv12pad, *jpgpad;
  GstBus *bus;

  bus = gst_bus_new ();

  have_h264_eos = have_yuy2_eos = have_nv12_eos = have_jpg_eos = FALSE;
  buffer_h264 = buffer_yuy2 = buffer_nv12 = buffer_jpg = NULL;

  demux = gst_element_factory_make ("uvch264mjpgdemux", NULL);
  fail_unless (demux != NULL);

  gst_element_set_bus (demux, bus);
  gst_bus_set_sync_handler (bus, _bus_sync_handler, NULL, NULL);

  mjpg_pad = gst_pad_new_from_static_template (&mjpg_template, "src");
  fail_unless (mjpg_pad != NULL);
  sinkpad = gst_element_get_static_pad (demux, "sink");
  fail_unless (sinkpad != NULL);
  fail_unless (gst_pad_link (mjpg_pad, sinkpad) == GST_PAD_LINK_OK);
  gst_object_unref (sinkpad);

  gst_pad_set_active (mjpg_pad, TRUE);

  if (link_h264) {
    h264pad = gst_element_get_static_pad (demux, "h264");
    fail_unless (h264pad != NULL);

    h264_pad = gst_pad_new_from_static_template (&sink_template, "h264");
    fail_unless (h264_pad != NULL);
    gst_pad_set_chain_function (h264_pad, _sink_h264_chain);
    gst_pad_set_event_function (h264_pad, _sink_h264_event);

    fail_unless (gst_pad_link (h264pad, h264_pad) == GST_PAD_LINK_OK);
    gst_object_unref (h264pad);

    gst_pad_set_active (h264_pad, TRUE);
  }
  if (link_yuy2) {
    yuy2pad = gst_element_get_static_pad (demux, "yuy2");
    fail_unless (yuy2pad != NULL);

    yuy2_pad = gst_pad_new_from_static_template (&sink_template, "yuy2");
    fail_unless (yuy2_pad != NULL);
    gst_pad_set_chain_function (yuy2_pad, _sink_yuy2_chain);
    gst_pad_set_event_function (yuy2_pad, _sink_yuy2_event);

    fail_unless (gst_pad_link (yuy2pad, yuy2_pad) == GST_PAD_LINK_OK);
    gst_object_unref (yuy2pad);

    gst_pad_set_active (yuy2_pad, TRUE);
  }
  if (link_nv12) {
    nv12pad = gst_element_get_static_pad (demux, "nv12");
    fail_unless (nv12pad != NULL);

    nv12_pad = gst_pad_new_from_static_template (&sink_template, "nv12");
    fail_unless (nv12_pad != NULL);
    gst_pad_set_chain_function (nv12_pad, _sink_nv12_chain);
    gst_pad_set_event_function (nv12_pad, _sink_nv12_event);

    fail_unless (gst_pad_link (nv12pad, nv12_pad) == GST_PAD_LINK_OK);
    gst_object_unref (nv12pad);
    gst_pad_set_active (nv12_pad, TRUE);
  }
  if (link_jpg) {
    jpgpad = gst_element_get_static_pad (demux, "jpeg");
    fail_unless (jpgpad != NULL);

    jpg_pad = gst_pad_new_from_static_template (&sink_template, "jpeg");
    fail_unless (jpg_pad != NULL);
    gst_pad_set_chain_function (jpg_pad, _sink_jpg_chain);
    gst_pad_set_event_function (jpg_pad, _sink_jpg_event);

    fail_unless (gst_pad_link (jpgpad, jpg_pad) == GST_PAD_LINK_OK);
    gst_object_unref (jpgpad);

    gst_pad_set_active (jpg_pad, TRUE);
  }

  gst_element_set_state (demux, GST_STATE_PLAYING);

  caps = gst_static_pad_template_get_caps (&mjpg_template);
  gst_check_setup_events_with_stream_id (mjpg_pad, demux, caps,
      GST_FORMAT_TIME, "uvch264demux-test");
  gst_caps_unref (caps);
}

static GstBuffer *
_buffer_from_file (const gchar * filename)
{
  GstBuffer *buffer;
  gchar *contents = NULL;
  gsize length = 0;

  fail_unless (g_file_get_contents (filename, &contents, &length, NULL));

  buffer = gst_buffer_new_wrapped (contents, length);
  GST_BUFFER_OFFSET (buffer) = 0;

  return buffer;
}

GST_START_TEST (test_valid_h264_jpg)
{
  GstCaps *mjpg_caps = gst_static_pad_template_get_caps (&mjpg_template);
  GstCaps *h264_caps;
  GstBuffer *buffer;
  gchar *h264_data, *jpg_data;
  gsize h264_size, jpg_size;

  _setup_test (TRUE, TRUE, TRUE, TRUE);

  h264_caps = gst_caps_new_simple ("video/x-h264",
      "width", G_TYPE_INT, 640, "height", G_TYPE_INT, 480,
      "framerate", GST_TYPE_FRACTION, 15, 1, NULL);
  buffer = _buffer_from_file (VALID_H264_JPG_MJPG_FILENAME);

  fail_unless (g_file_get_contents (VALID_H264_JPG_H264_FILENAME,
          &h264_data, &h264_size, NULL));
  fail_unless (g_file_get_contents (VALID_H264_JPG_JPG_FILENAME,
          &jpg_data, &jpg_size, NULL));

  fail_unless (gst_pad_push (mjpg_pad, buffer) == GST_FLOW_OK);
  fail_unless (gst_pad_push_event (mjpg_pad, gst_event_new_eos ()));

  fail_unless (have_h264_eos);
  fail_unless (have_yuy2_eos);
  fail_unless (have_nv12_eos);
  fail_unless (have_jpg_eos);
  fail_unless (buffer_h264 != NULL);
  fail_unless (buffer_jpg != NULL);
  fail_unless (buffer_nv12 == NULL);
  fail_unless (buffer_yuy2 == NULL);
  fail_unless (gerror == NULL && error_debug == NULL);
  fail_unless (gst_caps_is_always_compatible (negotiated_caps_h264, h264_caps));
  fail_unless (gst_caps_is_always_compatible (negotiated_caps_jpg, mjpg_caps));
  fail_unless (gst_buffer_get_size (buffer_h264) == h264_size);
  fail_unless (gst_buffer_get_size (buffer_jpg) == jpg_size);
  fail_unless (gst_buffer_memcmp (buffer_h264, 0, h264_data, h264_size) == 0);
  fail_unless (gst_buffer_memcmp (buffer_jpg, 0, jpg_data, jpg_size) == 0);

  gst_caps_unref (mjpg_caps);
  gst_caps_unref (h264_caps);
  g_free (h264_data);
  g_free (jpg_data);
  gst_buffer_unref (buffer_h264);
  gst_buffer_unref (buffer_jpg);
  _teardown_test ();
}

GST_END_TEST;

GST_START_TEST (test_valid_h264_yuy2)
{
  GstCaps *h264_caps;
  GstCaps *yuy2_caps;
  GstBuffer *buffer;
  gchar *h264_data, *yuy2_data;
  gsize h264_size, yuy2_size;

  _setup_test (TRUE, TRUE, TRUE, TRUE);

  h264_caps = gst_caps_new_simple ("video/x-h264",
      "width", G_TYPE_INT, 640, "height", G_TYPE_INT, 480,
      "framerate", GST_TYPE_FRACTION, 15, 1, NULL);
  yuy2_caps = gst_caps_new_simple ("video/x-raw",
      "format", G_TYPE_STRING, "YUY2",
      "width", G_TYPE_INT, 160, "height", G_TYPE_INT, 90,
      "framerate", GST_TYPE_FRACTION, 15, 1, NULL);
  buffer = _buffer_from_file (VALID_H264_YUY2_MJPG_FILENAME);
  fail_unless (g_file_get_contents (VALID_H264_YUY2_H264_FILENAME,
          &h264_data, &h264_size, NULL));
  fail_unless (g_file_get_contents (VALID_H264_YUY2_YUY2_FILENAME,
          &yuy2_data, &yuy2_size, NULL));

  fail_unless (gst_pad_push (mjpg_pad, buffer) == GST_FLOW_OK);
  fail_unless (gst_pad_push_event (mjpg_pad, gst_event_new_eos ()));

  fail_unless (have_h264_eos);
  fail_unless (have_yuy2_eos);
  fail_unless (have_nv12_eos);
  fail_unless (have_jpg_eos);
  fail_unless (buffer_h264 != NULL);
  fail_unless (buffer_jpg == NULL);
  fail_unless (buffer_nv12 == NULL);
  fail_unless (buffer_yuy2 != NULL);
  fail_unless (gerror == NULL && error_debug == NULL);
  fail_unless (gst_caps_is_always_compatible (negotiated_caps_h264, h264_caps));
  fail_unless (gst_caps_is_always_compatible (negotiated_caps_yuy2, yuy2_caps));
  fail_unless (gst_buffer_get_size (buffer_h264) == h264_size);
  fail_unless (gst_buffer_get_size (buffer_yuy2) == yuy2_size);
  fail_unless (gst_buffer_memcmp (buffer_h264, 0, h264_data, h264_size) == 0);
  fail_unless (gst_buffer_memcmp (buffer_yuy2, 0, yuy2_data, yuy2_size) == 0);

  gst_caps_unref (yuy2_caps);
  gst_caps_unref (h264_caps);
  g_free (h264_data);
  g_free (yuy2_data);
  gst_buffer_unref (buffer_h264);
  gst_buffer_unref (buffer_yuy2);
  _teardown_test ();
}

GST_END_TEST;

GST_START_TEST (test_no_data)
{
  GstBuffer *buffer = gst_buffer_new ();

  _setup_test (TRUE, TRUE, TRUE, TRUE);

  fail_unless (gst_pad_push (mjpg_pad, buffer) == GST_FLOW_OK);
  fail_unless (gst_pad_push_event (mjpg_pad, gst_event_new_eos ()));

  fail_unless (have_h264_eos && have_yuy2_eos && have_nv12_eos && have_jpg_eos);
  fail_unless (buffer_h264 == NULL && buffer_jpg != NULL);
  fail_unless (buffer_nv12 == NULL && buffer_yuy2 == NULL);
  fail_unless (gerror == NULL && error_debug == NULL);
  gst_buffer_unref (buffer_jpg);

  _teardown_test ();
}

GST_END_TEST;

GST_START_TEST (test_data_zero)
{
  GstBuffer *buffer = gst_buffer_new_and_alloc (1024);

  _setup_test (TRUE, TRUE, TRUE, TRUE);

  gst_buffer_memset (buffer, 0, 0, 1024);
  fail_unless (gst_pad_push (mjpg_pad, buffer) == GST_FLOW_OK);
  fail_unless (gst_pad_push_event (mjpg_pad, gst_event_new_eos ()));

  fail_unless (have_h264_eos && have_yuy2_eos && have_nv12_eos && have_jpg_eos);
  fail_unless (buffer_h264 == NULL && buffer_jpg == NULL);
  fail_unless (buffer_nv12 == NULL && buffer_yuy2 == NULL);

  _teardown_test ();
}

GST_END_TEST;

GST_START_TEST (test_no_marker_size)
{
  GstBuffer *buffer = gst_buffer_new_and_alloc (1024);
  const guchar data[] = {
    0xff, 0xd8, 0xff, 0xe4, 0x00
  };

  _setup_test (TRUE, TRUE, TRUE, TRUE);

  gst_buffer_fill (buffer, 0, data, sizeof (data));
  gst_buffer_set_size (buffer, sizeof (data));
  fail_unless (gst_pad_push (mjpg_pad, buffer) == GST_FLOW_ERROR);
  fail_unless (gst_pad_push_event (mjpg_pad, gst_event_new_eos ()));

  fail_unless (have_h264_eos && have_yuy2_eos && have_nv12_eos && have_jpg_eos);
  fail_unless (buffer_h264 == NULL && buffer_jpg == NULL);
  fail_unless (buffer_nv12 == NULL && buffer_yuy2 == NULL);
  fail_unless (gerror != NULL);
  fail_unless (gerror->domain == GST_STREAM_ERROR);
  fail_unless (gerror->code == GST_STREAM_ERROR_DEMUX);
  fail_unless (memcmp (gerror->message,
          "Not enough data to read marker size",
          strlen (gerror->message)) == 0);

  _teardown_test ();
}

GST_END_TEST;

GST_START_TEST (test_not_enough_data)
{
  GstBuffer *buffer = gst_buffer_new_and_alloc (1024);
  const guchar data[] = {
    0xff, 0xd8, 0xff, 0xe4, 0x00, 0xff, 0x00, 0x00
  };

  _setup_test (TRUE, TRUE, TRUE, TRUE);

  gst_buffer_fill (buffer, 0, data, sizeof (data));
  gst_buffer_set_size (buffer, sizeof (data));
  fail_unless (gst_pad_push (mjpg_pad, buffer) == GST_FLOW_ERROR);
  fail_unless (gst_pad_push_event (mjpg_pad, gst_event_new_eos ()));

  fail_unless (have_h264_eos && have_yuy2_eos && have_nv12_eos && have_jpg_eos);
  fail_unless (buffer_h264 == NULL && buffer_jpg == NULL);
  fail_unless (buffer_nv12 == NULL && buffer_yuy2 == NULL);
  fail_unless (gerror != NULL);
  fail_unless (gerror->domain == GST_STREAM_ERROR);
  fail_unless (gerror->code == GST_STREAM_ERROR_DEMUX);
  fail_unless (memcmp (gerror->message,
          "Not enough data to read marker content",
          strlen (gerror->message)) == 0);

  _teardown_test ();
}

GST_END_TEST;

GST_START_TEST (test_no_aux_header)
{
  GstBuffer *buffer = gst_buffer_new_and_alloc (1024);
  const guchar data[] = {
    0xff, 0xd8, 0xff, 0xe4, 0x00, 0x02, 0x00, 0x00,
    0xff, 0xd9
  };

  _setup_test (TRUE, TRUE, TRUE, TRUE);

  gst_buffer_fill (buffer, 0, data, sizeof (data));
  gst_buffer_set_size (buffer, sizeof (data));
  fail_unless (gst_pad_push (mjpg_pad, buffer) == GST_FLOW_ERROR);
  fail_unless (gst_pad_push_event (mjpg_pad, gst_event_new_eos ()));

  fail_unless (have_h264_eos && have_yuy2_eos && have_nv12_eos && have_jpg_eos);
  fail_unless (buffer_h264 == NULL && buffer_jpg == NULL);
  fail_unless (buffer_nv12 == NULL && buffer_yuy2 == NULL);
  fail_unless (gerror != NULL);
  fail_unless (gerror->domain == GST_STREAM_ERROR);
  fail_unless (gerror->code == GST_STREAM_ERROR_DEMUX);
  fail_unless (memcmp (gerror->message,
          "Not enough data to read aux header", strlen (gerror->message)) == 0);

  _teardown_test ();
}

GST_END_TEST;

GST_START_TEST (test_empty_aux_data)
{
  GstBuffer *buffer = gst_buffer_new_and_alloc (1024);
  const guchar data[] = {
    0xff, 0xd8, 0xff, 0xe4, 0x00, 0x1C, 0x00, 0x01,
    0x16, 0x00, 0x48, 0x32, 0x36, 0x34, 0x80, 0x07,
    0x38, 0x04, 0x2a, 0x2c, 0x0a, 0x00, 0x1b, 0x00,
    0x40, 0x62, 0xcb, 0x0a, 0x00, 0x00, 0x00, 0x00,
    0xff, 0xd9
  };

  _setup_test (TRUE, TRUE, TRUE, TRUE);

  gst_buffer_fill (buffer, 0, data, sizeof (data));
  gst_buffer_set_size (buffer, sizeof (data));
  fail_unless (gst_pad_push (mjpg_pad, buffer) == GST_FLOW_OK);
  fail_unless (gst_pad_push_event (mjpg_pad, gst_event_new_eos ()));

  fail_unless (have_h264_eos && have_yuy2_eos && have_nv12_eos && have_jpg_eos);
  fail_unless (buffer_h264 == NULL && buffer_jpg == NULL);
  fail_unless (buffer_nv12 == NULL && buffer_yuy2 == NULL);
  fail_unless (gerror == NULL);

  _teardown_test ();
}

GST_END_TEST;

GST_START_TEST (test_unknown_fcc)
{
  GstBuffer *buffer = gst_buffer_new_and_alloc (1024);
  const guchar data[] = {
    0xff, 0xd8, 0xff, 0xe4, 0x00, 0x2C, 0x00, 0x01,
    0x16, 0x00, 0x48, 0x30, 0x30, 0x30, 0x80, 0x07,
    0x38, 0x04, 0x2a, 0x2c, 0x0a, 0x00, 0x1b, 0x00,
    0x40, 0x62, 0xcb, 0x0a, 0x10, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xff, 0xd9
  };

  _setup_test (TRUE, TRUE, TRUE, TRUE);

  gst_buffer_fill (buffer, 0, data, sizeof (data));
  gst_buffer_set_size (buffer, sizeof (data));
  fail_unless (gst_pad_push (mjpg_pad, buffer) == GST_FLOW_ERROR);
  fail_unless (gst_pad_push_event (mjpg_pad, gst_event_new_eos ()));

  fail_unless (have_h264_eos && have_yuy2_eos && have_nv12_eos && have_jpg_eos);
  fail_unless (buffer_h264 == NULL && buffer_jpg == NULL);
  fail_unless (buffer_nv12 == NULL && buffer_yuy2 == NULL);
  fail_unless (gerror != NULL);
  fail_unless (gerror->domain == GST_STREAM_ERROR);
  fail_unless (gerror->code == GST_STREAM_ERROR_DEMUX);
  fail_unless (memcmp (gerror->message,
          "Unknown auxiliary stream format : H000",
          strlen (gerror->message)) == 0);

  _teardown_test ();
}

GST_END_TEST;

GST_START_TEST (test_not_enough_aux_data)
{
  GstBuffer *buffer = gst_buffer_new_and_alloc (1024);
  const guchar data[] = {
    0xff, 0xd8, 0xff, 0xe4, 0x00, 0x1C, 0x00, 0x01,
    0x16, 0x00, 0x48, 0x32, 0x36, 0x34, 0x80, 0x07,
    0x38, 0x04, 0x2a, 0x2c, 0x0a, 0x00, 0x1b, 0x00,
    0x40, 0x62, 0xcb, 0x0a, 0x10, 0x00, 0x00, 0x00,
    0xff, 0xd9
  };

  _setup_test (TRUE, TRUE, TRUE, TRUE);

  gst_buffer_fill (buffer, 0, data, sizeof (data));
  gst_buffer_set_size (buffer, sizeof (data));
  /* It's actually silently ignored */
  fail_unless (gst_pad_push (mjpg_pad, buffer) == GST_FLOW_OK);
  fail_unless (gst_pad_push_event (mjpg_pad, gst_event_new_eos ()));

  fail_unless (have_h264_eos && have_yuy2_eos && have_nv12_eos && have_jpg_eos);
  fail_unless (buffer_h264 == NULL && buffer_jpg == NULL);
  fail_unless (buffer_nv12 == NULL && buffer_yuy2 == NULL);

  _teardown_test ();
}

GST_END_TEST;

GST_START_TEST (test_too_much_aux_data)
{
  GstBuffer *buffer = gst_buffer_new_and_alloc (1024);
  const guchar data[] = {
    0xff, 0xd8, 0xff, 0xe4, 0x00, 0x3C, 0x00, 0x01,
    0x16, 0x00, 0x48, 0x32, 0x36, 0x34, 0x80, 0x07,
    0x38, 0x04, 0x2a, 0x2c, 0x0a, 0x00, 0x1b, 0x00,
    0x40, 0x62, 0xcb, 0x0a, 0x10, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xff, 0xd9
  };

  _setup_test (TRUE, TRUE, TRUE, TRUE);

  gst_buffer_fill (buffer, 0, data, sizeof (data));
  gst_buffer_set_size (buffer, sizeof (data));
  fail_unless (gst_pad_push (mjpg_pad, buffer) == GST_FLOW_ERROR);
  fail_unless (gst_pad_push_event (mjpg_pad, gst_event_new_eos ()));

  fail_unless (have_h264_eos && have_yuy2_eos && have_nv12_eos && have_jpg_eos);
  fail_unless (buffer_h264 == NULL && buffer_jpg == NULL);
  fail_unless (buffer_nv12 == NULL && buffer_yuy2 == NULL);
  fail_unless (gerror != NULL);
  fail_unless (gerror->domain == GST_STREAM_ERROR);
  fail_unless (gerror->code == GST_STREAM_ERROR_DEMUX);
  fail_unless (memcmp (gerror->message,
          "Expected 16 auxiliary data, got 32 bytes",
          strlen (gerror->message)) == 0);

  _teardown_test ();
}

GST_END_TEST;


GST_START_TEST (test_no_sos_marker)
{
  GstBuffer *buffer = gst_buffer_new_and_alloc (1024);
  const guchar data[] = {
    0xff, 0xd8, 0xff, 0xe4, 0x00, 0x2C, 0x00, 0x01,
    0x16, 0x00, 0x48, 0x32, 0x36, 0x34, 0x80, 0x07,
    0x38, 0x04, 0x2a, 0x2c, 0x0a, 0x00, 0x1b, 0x00,
    0x40, 0x62, 0xcb, 0x0a, 0x10, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xff, 0xd9
  };
  const guchar h264_data[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  };

  _setup_test (TRUE, TRUE, TRUE, TRUE);

  gst_buffer_fill (buffer, 0, data, sizeof (data));
  gst_buffer_set_size (buffer, sizeof (data));
  fail_unless (gst_pad_push (mjpg_pad, buffer) == GST_FLOW_OK);
  fail_unless (gst_pad_push_event (mjpg_pad, gst_event_new_eos ()));

  fail_unless (have_h264_eos && have_yuy2_eos && have_nv12_eos && have_jpg_eos);
  fail_unless (buffer_h264 != NULL && buffer_jpg == NULL);
  fail_unless (buffer_nv12 == NULL && buffer_yuy2 == NULL);
  fail_unless (gerror == NULL);
  fail_unless (gst_buffer_get_size (buffer_h264) == sizeof (h264_data));
  fail_if (gst_buffer_memcmp (buffer_h264, 0, h264_data, sizeof (h264_data)));
  gst_buffer_unref (buffer_h264);

  _teardown_test ();
}

GST_END_TEST;

static Suite *
uvch264demux_suite (void)
{
  Suite *s = suite_create ("uvch264demux");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_set_timeout (tc_chain, 180);
  tcase_add_test (tc_chain, test_valid_h264_jpg);
  tcase_add_test (tc_chain, test_valid_h264_yuy2);
  tcase_add_test (tc_chain, test_no_data);
  tcase_add_test (tc_chain, test_data_zero);
  tcase_add_test (tc_chain, test_no_marker_size);
  tcase_add_test (tc_chain, test_not_enough_data);
  tcase_add_test (tc_chain, test_no_aux_header);
  tcase_add_test (tc_chain, test_empty_aux_data);
  tcase_add_test (tc_chain, test_unknown_fcc);
  tcase_add_test (tc_chain, test_no_sos_marker);
  tcase_add_test (tc_chain, test_not_enough_aux_data);
  tcase_add_test (tc_chain, test_too_much_aux_data);

  return s;
}

GST_CHECK_MAIN (uvch264demux);
