/* GStreamer
 *
 * unit test for autoconvert element
 * Copyright (C) 2009 Jan Schmidt <thaytan@noraisin.net>
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

#include <gst/gst.h>
#include <gst/check/gstcheck.h>

/* Define 2 element factories for testing with */
typedef GstBin TestElement1;
typedef GstBinClass TestElement1Class;
typedef GstBin TestElement2;
typedef GstBinClass TestElement2Class;

GType test_element1_get_type (void);
G_DEFINE_TYPE (TestElement1, test_element1, GST_TYPE_BIN);
GType test_element2_get_type (void);
G_DEFINE_TYPE (TestElement2, test_element2, GST_TYPE_BIN);

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC, GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("test/caps,type=(int)[1,2]"));
static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK, GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("test/caps,type=(int)[1,2]"));

static void
setup (void)
{
  /* Register our test elements */
  fail_unless (gst_element_register (NULL, "testelement1", GST_RANK_NONE,
          test_element1_get_type ()));
  fail_unless (gst_element_register (NULL, "testelement2", GST_RANK_NONE,
          test_element2_get_type ()));
}

static void
teardown (void)
{
}

static void
set_autoconvert_factories (GstElement * autoconvert)
{
  const gchar *desired_features[] = { "testelement1", "testelement2" };
  GstElementFactory *feature;
  GList *factories = NULL;
  gint i;

  for (i = 0; i < G_N_ELEMENTS (desired_features); i++) {
    feature =
        GST_ELEMENT_FACTORY_CAST (gst_registry_find_feature
        (gst_registry_get (), desired_features[i], GST_TYPE_ELEMENT_FACTORY));
    fail_if (feature == NULL, "Test element %s was not found in registry",
        desired_features[i]);
    factories = g_list_prepend (factories, feature);
  }

  g_object_set (G_OBJECT (autoconvert), "factories", factories, NULL);

  g_list_free_full (factories, gst_object_unref);
}

GST_START_TEST (test_autoconvert_simple)
{
  GstPad *test_src_pad, *test_sink_pad;
  GstElement *autoconvert = gst_check_setup_element ("autoconvert");
  GstBus *bus = gst_bus_new ();
  GstCaps *caps;
  guint i;

  set_autoconvert_factories (autoconvert);

  test_src_pad = gst_check_setup_src_pad (autoconvert, &src_factory);
  gst_pad_set_active (test_src_pad, TRUE);
  test_sink_pad = gst_check_setup_sink_pad (autoconvert, &sink_factory);
  gst_pad_set_active (test_sink_pad, TRUE);

  gst_element_set_state (GST_ELEMENT_CAST (autoconvert), GST_STATE_PLAYING);

  /* Setting original caps */
  caps = gst_caps_from_string ("test/caps,type=(int)1");
  gst_check_setup_events (test_src_pad, autoconvert, caps, GST_FORMAT_BYTES);
  gst_caps_unref (caps);

  /* Push 10 items */
  for (i = 0; i < 10; i++) {
    GST_LOG ("Pushing test buffer %d, caps 1", i);
    fail_unless (gst_pad_push (test_src_pad, gst_buffer_new_and_alloc (4096))
        == GST_FLOW_OK);
  }

  GST_LOG ("Changing caps to caps 2");
  caps = gst_caps_from_string ("test/caps,type=(int)2");
  fail_unless (gst_pad_set_caps (test_src_pad, caps));
  gst_caps_unref (caps);

  /* Push 10 more items */
  for (i = 0; i < 10; i++) {
    GST_LOG ("Pushing test buffer %d, caps 2", i);
    fail_unless (gst_pad_push (test_src_pad, gst_buffer_new_and_alloc (4096))
        == GST_FLOW_OK);
  }

  /* Check all the items arrived */
  fail_unless_equals_int (g_list_length (buffers), 20);

  while (TRUE) {
    GstMessage *msg = gst_bus_pop (bus);
    if (!msg)
      break;

    GST_DEBUG ("got message %s",
        gst_message_type_get_name (GST_MESSAGE_TYPE (msg)));
    fail_if (GST_MESSAGE_TYPE (msg) == GST_MESSAGE_ERROR);
  }

  gst_element_set_state ((GstElement *) autoconvert, GST_STATE_NULL);

  gst_bus_set_flushing (bus, TRUE);
  gst_object_unref (bus);

  gst_check_drop_buffers ();
  gst_pad_set_active (test_src_pad, FALSE);
  gst_pad_set_active (test_sink_pad, FALSE);
  gst_check_teardown_src_pad (autoconvert);
  gst_check_teardown_sink_pad (autoconvert);
  gst_check_teardown_element (autoconvert);
}

GST_END_TEST;

static Suite *
autoconvert_suite (void)
{
  Suite *s = suite_create ("autoconvert");
  TCase *tc_basic = tcase_create ("general");

  suite_add_tcase (s, tc_basic);
  tcase_add_checked_fixture (tc_basic, setup, teardown);
  tcase_add_test (tc_basic, test_autoconvert_simple);

  return s;
}

/* Implementation of the test elements */

static void
configure_test_element (GstBin * bin, const gchar * capsfilter)
{
  GstElement *filter;
  GstElement *identity;
  GstPad *pad, *ghostpad;
  GstPadTemplate *test_static_templ;

  filter = gst_element_factory_make ("capsfilter", NULL);
  fail_unless (filter != NULL);
  gst_util_set_object_arg (G_OBJECT (filter), "caps", capsfilter);

  identity = gst_element_factory_make ("identity", NULL);
  fail_unless (identity != NULL);

  gst_bin_add_many (bin, filter, identity, NULL);
  fail_unless (gst_element_link_many (filter, identity, NULL) == TRUE);


  test_static_templ = gst_static_pad_template_get (&sink_factory);
  pad = gst_element_get_static_pad (filter, "sink");
  ghostpad = gst_ghost_pad_new_from_template ("sink", pad, test_static_templ);
  gst_element_add_pad (GST_ELEMENT_CAST (bin), ghostpad);
  gst_object_unref (pad);
  gst_object_unref (test_static_templ);

  test_static_templ = gst_static_pad_template_get (&src_factory);
  pad = gst_element_get_static_pad (identity, "src");
  ghostpad = gst_ghost_pad_new_from_template ("src", pad, test_static_templ);
  gst_element_add_pad (GST_ELEMENT_CAST (bin), ghostpad);
  gst_object_unref (pad);
  gst_object_unref (test_static_templ);
}

static void
test_element1_class_init (TestElement1Class * klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  gst_element_class_add_static_pad_template (element_class, &src_factory);
  gst_element_class_add_static_pad_template (element_class, &sink_factory);
}

static void
test_element1_init (TestElement1 * elem)
{
  configure_test_element (GST_BIN_CAST (elem), "test/caps,type=(int)1");
}

static void
test_element2_class_init (TestElement2Class * klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  gst_element_class_add_static_pad_template (element_class, &src_factory);

  gst_element_class_add_static_pad_template (element_class, &sink_factory);
}

static void
test_element2_init (TestElement2 * elem)
{
  configure_test_element (GST_BIN_CAST (elem), "test/caps,type=(int)2");
}

GST_CHECK_MAIN (autoconvert);
