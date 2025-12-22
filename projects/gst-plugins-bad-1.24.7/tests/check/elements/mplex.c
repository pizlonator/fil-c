/* GStreamer
 *
 * unit test for mplex
 *
 * Copyright (C) <2008> Mark Nauwelaerts <mnauw@users.sourceforge.net>
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

#include <unistd.h>

#include <gst/check/gstcheck.h>

/* For ease of programming we use globals to keep refs for our floating
 * src and sink pads we create; otherwise we always have to do get_pad,
 * get_peer, and then remove references in every test function */
static GstPad *mysrcpad, *mysinkpad;

#define AUDIO_CAPS_STRING "audio/mpeg, " \
                           "mpegversion = (int) 1, " \
                           "layer = (int) 2, " \
                           "rate = (int) 48000, " \
                           "channels = (int) 1, " \
                           "framerate = (fraction) 25/1"

#define MPEG_CAPS_STRING "video/mpeg, " \
                           "systemstream = (bool) true"

static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (MPEG_CAPS_STRING));

static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (AUDIO_CAPS_STRING));


/* some global vars, makes it easy as for the ones above */
static GMutex mplex_mutex;
static GCond mplex_cond;
static gboolean arrived_eos;

/* another easy hack, some mp2 audio data that should please mplex
 * perhaps less would also do, but anyway ...
 */
/* begin binary data: */
guint8 mp2_data[] =             /* 384 */
{
  0xFF, 0xFD, 0x84, 0xC4, 0x75, 0x56, 0x46, 0x54, 0x54, 0x5B, 0x2E, 0xB0,
  0x80, 0x00, 0x00, 0xAB, 0xAA, 0xAE, 0x8A, 0xAC, 0xB4, 0xD7, 0x9D, 0xB6,
  0xDB, 0x5D, 0xB3, 0xDB, 0x8C, 0xF5, 0xCF, 0x8D, 0x38, 0xD2, 0xFB, 0xF3,
  0x66, 0x59, 0x6C, 0x62, 0x49, 0x16, 0x59, 0x65, 0xAC, 0xE8, 0x8C, 0x6F,
  0x18, 0x48, 0x6B, 0x96, 0xD0, 0xD2, 0x68, 0xA6, 0xC5, 0x42, 0x45, 0xA1,
  0x28, 0x42, 0xBC, 0xA3, 0x99, 0x39, 0x53, 0x20, 0xBA, 0x4A, 0x56, 0x30,
  0xC5, 0x81, 0xE6, 0x16, 0x6B, 0x77, 0x67, 0x24, 0x29, 0xA9, 0x11, 0x7E,
  0xA9, 0xA8, 0x41, 0xE1, 0x11, 0x48, 0x79, 0xB1, 0xC2, 0x30, 0x39, 0x2D,
  0x40, 0x9A, 0xEC, 0x12, 0x65, 0xC5, 0xDD, 0x68, 0x8D, 0x6A, 0xF4, 0x63,
  0x02, 0xAE, 0xE5, 0x1B, 0xAA, 0xA3, 0x87, 0x1B, 0xDE, 0xB8, 0x6B, 0x7A,
  0x9B, 0xAF, 0xF7, 0x1A, 0x39, 0x33, 0x9A, 0x17, 0x56, 0x64, 0x0D, 0xDC,
  0xE2, 0x15, 0xEF, 0x93, 0x24, 0x9A, 0x8E, 0x59, 0x49, 0x7D, 0x45, 0x68,
  0x2D, 0x9F, 0x85, 0x71, 0xA8, 0x99, 0xC4, 0x6D, 0x26, 0x46, 0x40, 0xBA,
  0x9A, 0xD6, 0x3D, 0xCF, 0x45, 0xB2, 0xC6, 0xF3, 0x16, 0x21, 0x8B, 0xA8,
  0xD5, 0x59, 0x78, 0x87, 0xB7, 0x42, 0x9A, 0x65, 0x59, 0x9A, 0x99, 0x58,
  0x71, 0x26, 0x20, 0x33, 0x76, 0xEE, 0x96, 0x70, 0xF2, 0xBC, 0xB3, 0x7D,
  0x6B, 0x35, 0x48, 0x37, 0x59, 0x21, 0xC4, 0x87, 0x8A, 0xD8, 0x05, 0x36,
  0xA5, 0x1A, 0x5C, 0x0A, 0x4F, 0x4B, 0x39, 0x40, 0x39, 0x9A, 0x17, 0xD9,
  0xAD, 0x21, 0xBE, 0x64, 0xB4, 0x6B, 0x13, 0x03, 0x20, 0x95, 0xDA, 0x18,
  0x89, 0x88, 0xB5, 0x44, 0xE2, 0x5D, 0x4F, 0x12, 0x19, 0xC4, 0x1A, 0x1A,
  0x07, 0x07, 0x91, 0xA8, 0x4C, 0x66, 0xB4, 0x81, 0x33, 0xDE, 0xDB, 0xD6,
  0x24, 0x17, 0xD2, 0x9A, 0x4E, 0xC9, 0x88, 0xAB, 0x44, 0xAA, 0x25, 0x4A,
  0x79, 0xA9, 0x39, 0x39, 0x0D, 0x2D, 0x20, 0x76, 0x68, 0x5F, 0x65, 0x25,
  0xCF, 0x29, 0x27, 0x67, 0xB3, 0x68, 0x6C, 0xE5, 0xDC, 0xA5, 0x79, 0xC9,
  0xAB, 0x46, 0x9D, 0x21, 0x35, 0x82, 0x98, 0xBA, 0x0E, 0x26, 0x39, 0x20,
  0xAE, 0x1B, 0x92, 0x3D, 0xF7, 0x9F, 0x29, 0xB5, 0xF3, 0xB6, 0x38, 0x68,
  0x65, 0x99, 0xAD, 0xD8, 0x98, 0x56, 0x5A, 0x61, 0x8D, 0xCB, 0x4A, 0x29,
  0x43, 0x0E, 0x2D, 0x33, 0x40, 0x6A, 0xB7, 0x5F, 0x49, 0xC9, 0x81, 0xE4,
  0x0D, 0x6F, 0x15, 0x58, 0x1B, 0x9E, 0x74, 0x20, 0x5D, 0x97, 0x5B, 0x5A,
  0xDF, 0x92, 0x2D, 0x5A, 0x98, 0xCE, 0x50, 0x20, 0x1A, 0x33, 0x6A, 0x67,
  0xE2, 0x18, 0x94, 0xA4, 0x70, 0x8F, 0x5F, 0x11, 0x85, 0xB0, 0xE5, 0xD8,
  0xD4, 0xAA, 0x86, 0xAE, 0x1C, 0x0D, 0xA1, 0x6B, 0x21, 0xB9, 0xC2, 0x17
};

/* end binary data. size = 384 bytes */

static gboolean
test_sink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_EOS:
      g_mutex_lock (&mplex_mutex);
      arrived_eos = TRUE;
      g_cond_signal (&mplex_cond);
      g_mutex_unlock (&mplex_mutex);
      break;
    default:
      break;
  }

  return gst_pad_event_default (pad, parent, event);
}

/* setup and teardown needs some special handling for muxer */
static GstPad *
setup_src_pad (GstElement * element,
    GstStaticPadTemplate * template, GstCaps * caps, const gchar * sinkname)
{
  GstPad *srcpad, *sinkpad;

  GST_DEBUG_OBJECT (element, "setting up sending pad");
  /* sending pad */
  srcpad = gst_pad_new_from_static_template (template, "src");
  fail_if (srcpad == NULL, "Could not create a srcpad");
  ASSERT_OBJECT_REFCOUNT (srcpad, "srcpad", 1);

  sinkpad = gst_element_request_pad_simple (element, sinkname);
  fail_if (sinkpad == NULL, "Could not get sink pad from %s",
      GST_ELEMENT_NAME (element));
  /* references are owned by: 1) us, 2) mplex, 3) mplex list */
  ASSERT_OBJECT_REFCOUNT (sinkpad, "sinkpad", 3);
  if (caps)
    fail_unless (gst_pad_set_caps (srcpad, caps));
  fail_unless (gst_pad_link (srcpad, sinkpad) == GST_PAD_LINK_OK,
      "Could not link source and %s sink pads", GST_ELEMENT_NAME (element));
  gst_object_unref (sinkpad);   /* because we got it higher up */

  /* references are owned by: 1) mplex, 2) mplex list */
  ASSERT_OBJECT_REFCOUNT (sinkpad, "sinkpad", 2);

  return srcpad;
}

static void
teardown_src_pad (GstElement * element, const gchar * sinkname)
{
  GstPad *srcpad, *sinkpad;
  gchar *padname;

  /* clean up floating src pad */
  padname = g_strdup (sinkname);
  memcpy (strchr (padname, '%'), "0", 2);
  sinkpad = gst_element_get_static_pad (element, padname);
  g_free (padname);
  /* pad refs held by 1) mplex 2) mplex list and 3) us (through _get) */
  ASSERT_OBJECT_REFCOUNT (sinkpad, "sinkpad", 3);
  srcpad = gst_pad_get_peer (sinkpad);

  gst_pad_unlink (srcpad, sinkpad);

  /* after unlinking, pad refs still held by
   * 1) mplex and 2) mplex list and 3) us (through _get) */
  ASSERT_OBJECT_REFCOUNT (sinkpad, "sinkpad", 3);
  gst_object_unref (sinkpad);
  /* one more ref is held by element itself */

  /* pad refs held by both creator and this function (through _get_peer) */
  ASSERT_OBJECT_REFCOUNT (srcpad, "srcpad", 2);
  gst_object_unref (srcpad);
  gst_object_unref (srcpad);

}

static GstElement *
setup_mplex (void)
{
  GstElement *mplex;

  GST_DEBUG ("setup_mplex");
  mplex = gst_check_setup_element ("mplex");
  mysrcpad = setup_src_pad (mplex, &srctemplate, NULL, "audio_%u");
  mysinkpad = gst_check_setup_sink_pad (mplex, &sinktemplate);
  gst_pad_set_active (mysrcpad, TRUE);
  gst_pad_set_active (mysinkpad, TRUE);

  /* need to know when we are eos */
  gst_pad_set_event_function (mysinkpad, test_sink_event);

  /* and notify the test run */
  g_mutex_init (&mplex_mutex);
  g_cond_init (&mplex_cond);

  return mplex;
}

static void
cleanup_mplex (GstElement * mplex)
{
  GST_DEBUG ("cleanup_mplex");
  gst_element_set_state (mplex, GST_STATE_NULL);

  gst_pad_set_active (mysrcpad, FALSE);
  gst_pad_set_active (mysinkpad, FALSE);
  teardown_src_pad (mplex, "audio_%u");
  gst_check_teardown_sink_pad (mplex);
  gst_check_teardown_element (mplex);

  g_mutex_clear (&mplex_mutex);
  g_cond_clear (&mplex_cond);

  gst_deinit ();
}

GST_START_TEST (test_audio_pad)
{
  GstElement *mplex;
  GstBuffer *inbuffer, *outbuffer;
  GstCaps *caps;
  int i, num_buffers;

  /* PES pack_start_code */
  guint8 data0[] = { 0x00, 0x00, 0x01, 0xba };
  /* MPEG_program_end_code */
  guint8 data1[] = { 0x00, 0x00, 0x01, 0xb9 };

  mplex = setup_mplex ();
  fail_unless (gst_element_set_state (mplex,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  caps = gst_caps_from_string (AUDIO_CAPS_STRING);
  gst_check_setup_events_with_stream_id (mysrcpad, mplex, caps,
      GST_FORMAT_TIME, "mplex-test");
  gst_caps_unref (caps);

  /* corresponds to I420 buffer for the size mentioned in the caps */
  inbuffer = gst_buffer_new_and_alloc (sizeof (mp2_data));
  gst_buffer_fill (inbuffer, 0, mp2_data, sizeof (mp2_data));
  GST_BUFFER_TIMESTAMP (inbuffer) = 0;
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);

  /* need to force eos and state change to make sure the encoding task ends */
  fail_unless (gst_pad_push_event (mysrcpad, gst_event_new_eos ()) == TRUE);
  /* need to wait a bit to make sure mplex task digested all this */
  g_mutex_lock (&mplex_mutex);
  while (!arrived_eos)
    g_cond_wait (&mplex_cond, &mplex_mutex);
  g_mutex_unlock (&mplex_mutex);

  num_buffers = g_list_length (buffers);
  /* well, we do not really know much with mplex, but at least something ... */
  fail_unless (num_buffers >= 1);

  /* clean up buffers */
  for (i = 0; i < num_buffers; ++i) {
    outbuffer = GST_BUFFER (buffers->data);
    fail_if (outbuffer == NULL);

    if (i == 0) {
      fail_unless (gst_buffer_get_size (outbuffer) >= sizeof (data0));
      fail_unless (gst_buffer_memcmp (outbuffer, 0, data0,
              sizeof (data0)) == 0);
    }
    if (i == num_buffers - 1) {
      fail_unless (gst_buffer_get_size (outbuffer) >= sizeof (data1));
      fail_unless (gst_buffer_memcmp (outbuffer,
              gst_buffer_get_size (outbuffer) - sizeof (data1), data1,
              sizeof (data1)) == 0);
    }
    buffers = g_list_remove (buffers, outbuffer);

    ASSERT_BUFFER_REFCOUNT (outbuffer, "outbuffer", 1);
    gst_buffer_unref (outbuffer);
    outbuffer = NULL;
  }

  cleanup_mplex (mplex);
  g_list_free (buffers);
  buffers = NULL;
}

GST_END_TEST;

static Suite *
mplex_suite (void)
{
  Suite *s = suite_create ("mplex");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_audio_pad);

  return s;
}

GST_CHECK_MAIN (mplex);
