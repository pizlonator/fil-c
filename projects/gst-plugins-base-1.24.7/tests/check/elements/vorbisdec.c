/* GStreamer
 *
 * unit test for vorbisdec
 *
 * Copyright (C) <2005> Thomas Vander Stichele <thomas at apestaart dot org>
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

#include <gst/check/gstcheck.h>

#include <vorbis/codec.h>
#include <vorbis/vorbisenc.h>

/* For ease of programming we use globals to keep refs for our floating
 * src and sink pads we create; otherwise we always have to do get_pad,
 * get_peer, and then remove references in every test function */
static GstPad *mysrcpad, *mysinkpad;

/* a valid first header packet */
static guchar identification_header[30] = {
  1,                            /* packet_type */
  'v', 'o', 'r', 'b', 'i', 's',
  0, 0, 0, 0,                   /* vorbis_version */
  2,                            /* audio_channels */
  0x44, 0xac, 0, 0,             /* sample_rate */
  0xff, 0xff, 0xff, 0xff,       /* bitrate_maximum */
  0x00, 0xee, 0x02, 0x00,       /* bitrate_nominal */
  0xff, 0xff, 0xff, 0xff,       /* bitrate_minimum */
  0xb8,                         /* blocksize_0, blocksize_1 */
  0x01,                         /* framing_flag */
};

static guchar comment_header[] = {
  3,                            /* packet_type */
  'v', 'o', 'r', 'b', 'i', 's',
  2, 0, 0, 0,                   /* vendor_length */
  'm', 'e',
  1, 0, 0, 0,                   /* user_comment_list_length */
  9, 0, 0, 0,                   /* length comment[0] */
  'A', 'R', 'T', 'I', 'S', 'T', '=', 'm', 'e',
  0x01,                         /* framing bit */
};

static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);
static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

static GstElement *
setup_vorbisdec (void)
{
  GstElement *vorbisdec;
  GstCaps *caps;

  GST_DEBUG ("setup_vorbisdec");
  vorbisdec = gst_check_setup_element ("vorbisdec");
  mysrcpad = gst_check_setup_src_pad (vorbisdec, &srctemplate);
  mysinkpad = gst_check_setup_sink_pad (vorbisdec, &sinktemplate);
  gst_pad_set_active (mysrcpad, TRUE);

  caps = gst_caps_new_empty_simple ("audio/x-vorbis");
  gst_check_setup_events (mysrcpad, vorbisdec, caps, GST_FORMAT_TIME);
  gst_caps_unref (caps);

  gst_pad_set_active (mysinkpad, TRUE);

  return vorbisdec;
}

static void
cleanup_vorbisdec (GstElement * vorbisdec)
{
  GST_DEBUG ("cleanup_vorbisdec");
  gst_element_set_state (vorbisdec, GST_STATE_NULL);

  gst_pad_set_active (mysrcpad, FALSE);
  gst_pad_set_active (mysinkpad, FALSE);
  gst_check_teardown_src_pad (vorbisdec);
  gst_check_teardown_sink_pad (vorbisdec);
  gst_check_teardown_element (vorbisdec);

  g_list_free_full (buffers, (GDestroyNotify) gst_buffer_unref);
  buffers = NULL;
}

/* FIXME: also tests comment header */
GST_START_TEST (test_identification_header)
{
  GstElement *vorbisdec;
  GstBuffer *inbuffer;
  GstBus *bus;
  GstMessage *message;

  vorbisdec = setup_vorbisdec ();

  fail_unless (gst_element_set_state (vorbisdec,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");
  bus = gst_bus_new ();

  inbuffer = gst_buffer_new_and_alloc (30);
  gst_buffer_fill (inbuffer, 0, identification_header, 30);
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  gst_buffer_ref (inbuffer);

  gst_element_set_bus (vorbisdec, bus);
  /* pushing gives away my reference ... */
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);
  /* ... and nothing ends up on the global buffer list */
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  gst_buffer_unref (inbuffer);
  fail_unless (g_list_length (buffers) == 0);
  fail_if ((message = gst_bus_pop (bus)) != NULL);

  inbuffer = gst_buffer_new_and_alloc (sizeof (comment_header));
  gst_buffer_fill (inbuffer, 0, comment_header, sizeof (comment_header));
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  gst_buffer_ref (inbuffer);

  /* pushing gives away my reference ... */
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);
  /* ... and nothing ends up on the global buffer list */
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  gst_buffer_unref (inbuffer);
  fail_unless (g_list_length (buffers) == 0);

#if 0
  /* there's a tag message waiting */
  fail_if ((message = gst_bus_pop (bus)) == NULL);
  gst_message_parse_tag (message, &tag_list);
  fail_unless_equals_int (gst_tag_list_get_tag_size (tag_list, GST_TAG_ARTIST),
      1);
  fail_unless (gst_tag_list_get_string (tag_list, GST_TAG_ARTIST, &artist));
  fail_unless_equals_string (artist, "me");
  g_free (artist);
  fail_unless_equals_int (gst_tag_list_get_tag_size (tag_list, "album"), 0);
  gst_tag_list_unref (tag_list);
  gst_message_unref (message);
#endif

  /* make sure there's no error on the bus */
  message = gst_bus_pop_filtered (bus, GST_MESSAGE_ERROR);
  fail_if (message != NULL);

  /* cleanup */
  gst_bus_set_flushing (bus, TRUE);
  gst_element_set_bus (vorbisdec, NULL);
  gst_object_unref (GST_OBJECT (bus));
  cleanup_vorbisdec (vorbisdec);
}

GST_END_TEST;

static vorbis_comment vc;
static vorbis_dsp_state vd;
static vorbis_info vi;
static vorbis_block vb;

static GstBuffer *
_create_codebook_header_buffer (void)
{
  GstBuffer *buffer;
  ogg_packet header;
  ogg_packet header_comm;
  ogg_packet header_code;

  vorbis_info_init (&vi);
  vorbis_encode_setup_vbr (&vi, 1, 44000, 0.5);
  vorbis_encode_setup_init (&vi);
  vorbis_analysis_init (&vd, &vi);
  vorbis_block_init (&vd, &vb);
  vorbis_comment_init (&vc);
  vorbis_analysis_headerout (&vd, &vc, &header, &header_comm, &header_code);

  buffer = gst_buffer_new_and_alloc (header_code.bytes);
  gst_buffer_fill (buffer, 0, header_code.packet, header_code.bytes);

  return buffer;
}

static GstBuffer *
_create_audio_buffer (void)
{
  GstBuffer *buffer;
  ogg_packet packet;
  float **vorbis_buffer;
  gint i;

  vorbis_buffer = vorbis_analysis_buffer (&vd, 44100);
  for (i = 0; i < 44100 * 1; ++i)
    vorbis_buffer[0][i] = 0.0;
  vorbis_analysis_wrote (&vd, 44100);
  vorbis_analysis_blockout (&vd, &vb);
  vorbis_analysis (&vb, NULL);
  vorbis_bitrate_addblock (&vb);
  vorbis_bitrate_flushpacket (&vd, &packet);
  buffer = gst_buffer_new_and_alloc (packet.bytes);
  gst_buffer_fill (buffer, 0, packet.packet, packet.bytes);

  vorbis_comment_clear (&vc);
  vorbis_block_clear (&vb);
  vorbis_dsp_clear (&vd);
  vorbis_info_clear (&vi);

  return buffer;
}

GST_START_TEST (test_empty_vorbis_packet)
{
  GstElement *vorbisdec;
  GstBuffer *inbuffer;
  GstMessage *message;
  GstBus *bus;

  vorbisdec = setup_vorbisdec ();
  fail_unless_equals_int (gst_element_set_state (vorbisdec, GST_STATE_PLAYING),
      GST_STATE_CHANGE_SUCCESS);

  bus = gst_bus_new ();

  inbuffer = gst_buffer_new_and_alloc (30);
  gst_buffer_fill (inbuffer, 0, identification_header, 30);
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  gst_buffer_ref (inbuffer);

  gst_element_set_bus (vorbisdec, bus);

  /* pushing gives away my reference ... */
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);
  /* ... and nothing ends up on the global buffer list */
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  gst_buffer_unref (inbuffer);
  fail_unless (g_list_length (buffers) == 0);
  fail_if ((message = gst_bus_pop (bus)) != NULL);

  inbuffer = gst_buffer_new_and_alloc (sizeof (comment_header));
  gst_buffer_fill (inbuffer, 0, comment_header, sizeof (comment_header));
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  gst_buffer_ref (inbuffer);

  /* pushing gives away my reference ... */
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);
  /* ... and nothing ends up on the global buffer list */
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  gst_buffer_unref (inbuffer);
  fail_unless (g_list_length (buffers) == 0);

  /* send minimal codebook header and audio packers */
  inbuffer = _create_codebook_header_buffer ();
  fail_unless_equals_int (gst_pad_push (mysrcpad, inbuffer), GST_FLOW_OK);

  /* now send an empty vorbis packet, which should just be skipped */
  inbuffer = gst_buffer_new_and_alloc (0);
  gst_buffer_ref (inbuffer);
  fail_unless_equals_int (gst_pad_push (mysrcpad, inbuffer), GST_FLOW_OK);
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  gst_buffer_unref (inbuffer);
  fail_unless (g_list_length (buffers) == 0);

  /* create and push an encoded audio packet */
  inbuffer = _create_audio_buffer ();
  fail_unless_equals_int (gst_pad_push (mysrcpad, inbuffer), GST_FLOW_OK);

  /* now send another empty vorbis packet, which should just be skipped */
  inbuffer = gst_buffer_new_and_alloc (0);
  gst_buffer_ref (inbuffer);
  fail_unless_equals_int (gst_pad_push (mysrcpad, inbuffer), GST_FLOW_OK);
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  gst_buffer_unref (inbuffer);

  /* make sure there's no error on the bus */
  message = gst_bus_poll (bus, GST_MESSAGE_ERROR, 0);
  fail_if (message != NULL);

  /* cleanup */
  gst_bus_set_flushing (bus, TRUE);
  gst_element_set_bus (vorbisdec, NULL);
  gst_object_unref (GST_OBJECT (bus));
  cleanup_vorbisdec (vorbisdec);
}

GST_END_TEST;

static Suite *
vorbisdec_suite (void)
{
  Suite *s = suite_create ("vorbisdec");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_identification_header);
  tcase_add_test (tc_chain, test_empty_vorbis_packet);

  return s;
}

GST_CHECK_MAIN (vorbisdec);
