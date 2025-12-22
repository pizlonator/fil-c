#include <gst/check/gstharness.h>
#include <gst/check/gstcheck.h>

GST_START_TEST (netsim_stress)
{
  GstHarness *h = gst_harness_new ("netsim");
  GstCaps *caps = gst_caps_from_string ("mycaps");
  GstBuffer *buf = gst_harness_create_buffer (h, 100);
  GstHarnessThread *state, *push;
  GstSegment segment;

  gst_segment_init (&segment, GST_FORMAT_TIME);
  state = gst_harness_stress_statechange_start (h);
  push = gst_harness_stress_push_buffer_start (h, caps, &segment, buf);

  g_usleep (G_USEC_PER_SEC * 1);

  gst_harness_stress_thread_stop (state);
  gst_harness_stress_thread_stop (push);

  gst_caps_unref (caps);
  gst_buffer_unref (buf);
  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (netsim_stress_delayed)
{
  GstHarness *h = gst_harness_new_parse ("netsim delay-probability=0.5");
  GstCaps *caps = gst_caps_from_string ("mycaps");
  GstBuffer *buf;
  GstHarnessThread *state, *push;
  GstSegment segment;

  gst_harness_set_src_caps (h, gst_caps_ref (caps));
  buf = gst_harness_create_buffer (h, 100);
  gst_segment_init (&segment, GST_FORMAT_TIME);
  state = gst_harness_stress_statechange_start (h);
  push = gst_harness_stress_push_buffer_start (h, caps, &segment, buf);

  g_usleep (G_USEC_PER_SEC * 1);

  gst_harness_stress_thread_stop (state);
  gst_harness_stress_thread_stop (push);

  gst_caps_unref (caps);
  gst_buffer_unref (buf);
  gst_harness_teardown (h);
}

GST_END_TEST;

static Suite *
netsim_suite (void)
{
  Suite *s = suite_create ("netsim");
  TCase *tc_chain;

  suite_add_tcase (s, (tc_chain = tcase_create ("general")));
  tcase_add_test (tc_chain, netsim_stress);
  tcase_add_test (tc_chain, netsim_stress_delayed);

  return s;
}

GST_CHECK_MAIN (netsim)
