/* GStreamer
 * Copyright (C) <2011> Stefan Kost <ensonic@users.sf.net>
 *
 * unit test for the baseaudiovisualizer class
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

#undef GST_CAT_DEFAULT

#include <gst/check/gstcheck.h>
#include <gst/pbutils/gstaudiovisualizer.h>
#include <string.h>

/* dummy subclass for testing */

#define GST_TYPE_TEST_SCOPE            (gst_test_scope_get_type())
#define GST_TEST_SCOPE(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_TEST_SCOPE,GstTestScope))
#define GST_TEST_SCOPE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_TEST_SCOPE,GstTestScopeClass))
typedef struct _GstTestScope GstTestScope;
typedef struct _GstTestScopeClass GstTestScopeClass;

struct _GstTestScope
{
  GstAudioVisualizer parent;
};

struct _GstTestScopeClass
{
  GstAudioVisualizerClass parent_class;
};

static GstStaticPadTemplate gst_test_scope_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("xRGB"))
    );

static GstStaticPadTemplate gst_test_scope_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw, "
        "format = (string) " GST_AUDIO_NE (S16) ", "
        "layout = (string) interleaved, "
        "channels = (int) 2, "
        "channel-mask = (bitmask) 3, " "rate = (int) 44100")
    );

static GType gst_test_scope_get_type (void);

G_DEFINE_TYPE (GstTestScope, gst_test_scope, GST_TYPE_AUDIO_VISUALIZER);

static void
gst_test_scope_class_init (GstTestScopeClass * g_class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_set_static_metadata (element_class, "test scope",
      "Visualization",
      "Dummy test scope", "Stefan Kost <ensonic@users.sf.net>");

  gst_element_class_add_static_pad_template (element_class,
      &gst_test_scope_src_template);
  gst_element_class_add_static_pad_template (element_class,
      &gst_test_scope_sink_template);
}

static void
gst_test_scope_init (GstTestScope * scope)
{
  /* do nothing */
}

/* tests */
#define CAPS "audio/x-raw, "  \
        "format = (string) " GST_AUDIO_NE (S16) ", " \
        "layout = (string) interleaved, " \
        "rate = (int) 44100, " \
        "channels = (int) 2, " \
        "channel-mask = (bitmask) 3"

static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw, "
        "format = (string) xRGB, "
        "width = (int) 320, "
        "height = (int) 240, " "framerate = (fraction) 30/1")
    );
static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (CAPS)
    );

GST_START_TEST (count_in_out)
{
  GstElement *elem;
  GstPad *srcpad, *sinkpad;
  GstBuffer *buffer;
  GstCaps *caps;

  /* setup up */
  elem = gst_check_setup_element ("testscope");
  srcpad = gst_check_setup_src_pad (elem, &srctemplate);
  sinkpad = gst_check_setup_sink_pad (elem, &sinktemplate);
  gst_pad_set_active (srcpad, TRUE);
  gst_pad_set_active (sinkpad, TRUE);

  fail_unless (gst_element_set_state (elem,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  caps = gst_caps_from_string (CAPS);
  gst_check_setup_events (srcpad, elem, caps, GST_FORMAT_TIME);
  gst_caps_unref (caps);

  /* push 1s audio to get 30 video-frames */
  buffer = gst_buffer_new_and_alloc (44100 * 2 * sizeof (gint16));
  ASSERT_BUFFER_REFCOUNT (buffer, "buffer", 1);

  /* pushing gives away my reference ... */
  fail_unless (gst_pad_push (srcpad, buffer) == GST_FLOW_OK);
  /* ... but it ends up being collected on the global buffer list */
  ASSERT_BUFFER_REFCOUNT (buffer, "buffer", 1);
  fail_unless_equals_int (g_list_length (buffers), 30);

  /* clean up */
  g_list_foreach (buffers, (GFunc) gst_mini_object_unref, NULL);
  g_list_free (buffers);
  buffers = NULL;

  gst_pad_set_active (srcpad, FALSE);
  gst_pad_set_active (sinkpad, FALSE);
  gst_check_teardown_src_pad (elem);
  gst_check_teardown_sink_pad (elem);
  gst_check_teardown_element (elem);
}

GST_END_TEST;

static void
baseaudiovisualizer_init (void)
{
  gst_element_register (NULL, "testscope", GST_RANK_NONE, GST_TYPE_TEST_SCOPE);
}

static Suite *
baseaudiovisualizer_suite (void)
{
  Suite *s = suite_create ("baseaudiovisualizer");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_checked_fixture (tc_chain, baseaudiovisualizer_init, NULL);

  tcase_add_test (tc_chain, count_in_out);

  return s;
}


GST_CHECK_MAIN (baseaudiovisualizer);
