#include "parser.h"
#include <gst/check/gstcheck.h>
#include <gst/check/gstharness.h>

static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("raw/x-pcap"));

static GstStaticPadTemplate sinktemplate_rtp = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-rtp"));

static guint8 pcap_header[] = {
  0xd4, 0xc3, 0xb2, 0xa1, 0x02, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
};

static const guint pcap_frame_with_eth_padding_offset = 16 + 14 + 20 + 8;
static guint8 pcap_frame_with_eth_padding[] = {
  0x5f, 0x12, 0x4e, 0x54, 0x57, 0x70, 0x00, 0x00, 0x3c, 0x00, 0x00, 0x00,
  0x3c, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x29, 0xa6, 0x13, 0x41, 0x00, 0x0c,
  0x29, 0xb2, 0x93, 0x7d, 0x08, 0x00, 0x45, 0x00, 0x00, 0x2c, 0x00, 0x00,
  0x40, 0x00, 0x32, 0x11, 0x25, 0xb9, 0x52, 0xc5, 0x4d, 0xd6, 0xb9, 0x23,
  0xc9, 0x49, 0x44, 0x66, 0x9f, 0xf2, 0x00, 0x18, 0x75, 0xe8, 0x80, 0xe3,
  0x7c, 0xca, 0x79, 0xba, 0x09, 0xc0, 0x70, 0x6e, 0x8b, 0x33, 0x05, 0x0a,
  0x00, 0xa0, 0x00, 0x00
};

static gboolean
verify_buffer (buffer_verify_data_s * vdata, GstBuffer * buffer)
{
  guint offset = 0;
  guint size = 0;

  if (vdata->data_to_verify == pcap_frame_with_eth_padding) {
    offset = pcap_frame_with_eth_padding_offset;
    size = sizeof (pcap_frame_with_eth_padding) -
        pcap_frame_with_eth_padding_offset - 2;
  }

  fail_unless_equals_int (gst_buffer_get_size (buffer), size);
  fail_unless (gst_buffer_memcmp (buffer, 0, vdata->data_to_verify + offset,
          size) == 0);

  return TRUE;
}

static GstElement *
setup_element (const gchar * desc)
{
  GstElement *element;
  GstCaps *caps;

  (void) desc;

  caps = gst_caps_from_string ("application/x-rtp");
  element = gst_check_setup_element ("pcapparse");
  g_object_set (G_OBJECT (element), "caps", caps, NULL);
  gst_caps_unref (caps);

  return element;
}

GST_START_TEST (test_parse_frames_with_eth_padding)
{
  gst_parser_test_split (pcap_frame_with_eth_padding,
      sizeof (pcap_frame_with_eth_padding));
}

GST_END_TEST;

static const guint8 zerosize_data[] = {
  0xd4, 0xc3, 0xb2, 0xa1, 0x02, 0x00, 0x04, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x04, 0x00, 0x01, 0x00, 0x00, 0x00,
  0xd3, 0xff, 0x7a, 0x56, 0xbb, 0xd8, 0x0e, 0x00,
  0x2a, 0x00, 0x00, 0x00, 0x2a, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x45, 0x00,
  0x00, 0x1c, 0x06, 0xe7, 0x40, 0x00, 0x40, 0x11,
  0x35, 0xe8, 0x7f, 0x00, 0x00, 0x01, 0x7f, 0x00,
  0x00, 0x01, 0xd2, 0xa3, 0x13, 0x8c, 0x00, 0x08,
  0xfe, 0x1b
};

GST_START_TEST (test_parse_zerosize_frames)
{
  GstBuffer *in_buf, *out_buf;
  GstHarness *h;
  gsize data_size;

  h = gst_harness_new ("pcapparse");

  gst_harness_set_src_caps_str (h, "raw/x-pcap");

  data_size = sizeof (zerosize_data);

  in_buf = gst_buffer_new_memdup (zerosize_data, data_size);

  gst_harness_push (h, in_buf);
  gst_harness_play (h);
  gst_harness_push_event (h, gst_event_new_eos ());

  /* check that a buffer comes out and that it is 0 bytes in size */
  out_buf = gst_harness_pull (h);

  fail_unless (gst_buffer_get_size (out_buf) == 0);

  gst_buffer_unref (out_buf);
  gst_harness_teardown (h);
}

GST_END_TEST;

static Suite *
pcapparse_suite (void)
{
  Suite *s = suite_create ("pcapparse");
  TCase *tc_chain = tcase_create ("general");

  ctx_factory = "pcapparse";
  ctx_setup = setup_element;
  ctx_sink_template = &sinktemplate_rtp;
  ctx_src_template = &srctemplate;
  ctx_headers[0].data = pcap_header;
  ctx_headers[0].size = sizeof (pcap_header);
  ctx_no_metadata = TRUE;
  ctx_verify_buffer = verify_buffer;

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_parse_frames_with_eth_padding);
  tcase_add_test (tc_chain, test_parse_zerosize_frames);

  return s;
}

GST_CHECK_MAIN (pcapparse);
