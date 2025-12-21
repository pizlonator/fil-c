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

#include <glib.h>

#include <vorbis/codec.h>
#include <vorbis/vorbisenc.h>

#include <gst/gsttagsetter.h>
#include <gst/check/gstcheck.h>

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
  0x01                          /* framing_flag */
};

static guchar artist_comment_header[] = {
  3,                            /* packet_type */
  'v', 'o', 'r', 'b', 'i', 's',
  2, 0, 0, 0,                   /* vendor_length */
  'm', 'e',
  1, 0, 0, 0,                   /* user_comment_list_length */
  9, 0, 0, 0,                   /* length comment[0] */
  'A', 'R', 'T', 'I', 'S', 'T', '=', 'm', 'e',
  0x01,                         /* framing bit */
};

static guchar title_comment_header[] = {
  3,                            /* packet_type */
  'v', 'o', 'r', 'b', 'i', 's',
  2, 0, 0, 0,                   /* vendor_length */
  'm', 'e',
  1, 0, 0, 0,                   /* user_comment_list_length */
  12, 0, 0, 0,                  /* length comment[0] */
  'T', 'I', 'T', 'L', 'E', '=', 'f', 'o', 'o', 'b', 'a', 'r',
  0x01,                         /* framing bit */
};

static guchar empty_comment_header[] = {
  3,                            /* packet_type */
  'v', 'o', 'r', 'b', 'i', 's',
  2, 0, 0, 0,                   /* vendor_length */
  'm', 'e',
  0, 0, 0, 0,                   /* user_comment_list_length */
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

static GstPad *mysrcpad, *mysinkpad;
static GAsyncQueue *pending_buffers;
static gulong id;


static GstElement *
setup_vorbistag (void)
{
  GstElement *vorbistag;
  GstCaps *caps;

  GST_DEBUG ("setup_vorbistag");
  vorbistag = gst_check_setup_element ("vorbistag");
  mysrcpad = gst_check_setup_src_pad (vorbistag, &srctemplate);
  mysinkpad = gst_check_setup_sink_pad (vorbistag, &sinktemplate);
  gst_pad_set_active (mysrcpad, TRUE);

  caps = gst_caps_new_empty_simple ("audio/x-vorbis");
  gst_check_setup_events (mysrcpad, vorbistag, caps, GST_FORMAT_TIME);
  gst_caps_unref (caps);

  gst_pad_set_active (mysinkpad, TRUE);

  return vorbistag;
}

static void
cleanup_vorbistag (GstElement * vorbistag)
{
  GST_DEBUG ("cleanup_vorbistag");
  gst_element_set_state (vorbistag, GST_STATE_NULL);

  gst_pad_set_active (mysrcpad, FALSE);
  gst_pad_set_active (mysinkpad, FALSE);
  gst_check_drop_buffers ();
  gst_check_teardown_src_pad (vorbistag);
  gst_check_teardown_sink_pad (vorbistag);
  gst_check_teardown_element (vorbistag);
}


static GstPadProbeReturn
buffer_probe (GstPad * pad, GstPadProbeInfo * info, gpointer unused)
{
  GstBuffer *buffer = GST_PAD_PROBE_INFO_BUFFER (info);

  g_async_queue_push (pending_buffers, gst_buffer_ref (buffer));

  return GST_PAD_PROBE_OK;
}

static void
start_pipeline (GstElement * element)
{
  id = gst_pad_add_probe (mysinkpad, GST_PAD_PROBE_TYPE_BUFFER,
      (GstPadProbeCallback) buffer_probe, NULL, NULL);

  pending_buffers = g_async_queue_new ();
  gst_element_set_state (element, GST_STATE_PLAYING);
}

static GstBuffer *
get_buffer (void)
{
  return GST_BUFFER (g_async_queue_pop (pending_buffers));
}

static void
stop_pipeline (GstElement * element)
{
  GstBuffer *buf;

  while ((buf = g_async_queue_try_pop (pending_buffers)))
    gst_buffer_unref (buf);

  gst_pad_remove_probe (mysinkpad, id);
  id = 0;

  gst_element_set_state (element, GST_STATE_NULL);

  while ((buf = g_async_queue_try_pop (pending_buffers)))
    gst_buffer_unref (buf);

  g_async_queue_unref (pending_buffers);
  pending_buffers = NULL;
}

static void
compare_buffer (GstBuffer * buf, const guint8 * data, gsize size)
{
  GstMapInfo map;

  gst_buffer_map (buf, &map, GST_MAP_READ);
  fail_unless_equals_int (map.size, size);
  fail_unless_equals_int (memcmp (map.data, data, size), 0);
  gst_buffer_unmap (buf, &map);
}

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
  float **vorbis_buffer G_GNUC_UNUSED;

  vorbis_buffer = vorbis_analysis_buffer (&vd, 0);
  vorbis_analysis_wrote (&vd, 0);
  vorbis_analysis_blockout (&vd, &vb);
  vorbis_analysis (&vb, NULL);
  vorbis_bitrate_addblock (&vb);
  vorbis_bitrate_flushpacket (&vd, &packet);
  buffer = gst_buffer_new_and_alloc (packet.bytes);
  gst_buffer_fill (buffer, 0, packet.packet, packet.bytes);
  GST_DEBUG ("%p %ld", packet.packet, packet.bytes);

  vorbis_comment_clear (&vc);
  vorbis_block_clear (&vb);
  vorbis_dsp_clear (&vd);
  vorbis_info_clear (&vi);

  return buffer;
}


GST_START_TEST (test_empty_tags_set)
{
  GstTagList *tags;
  GstElement *vorbistag;
  GstBuffer *inbuffer, *outbuffer;

  vorbistag = setup_vorbistag ();

  tags = gst_tag_list_new_empty ();
  gst_tag_list_add (tags, GST_TAG_MERGE_REPLACE, GST_TAG_TITLE, "foobar", NULL);
  gst_tag_setter_merge_tags (GST_TAG_SETTER (vorbistag), tags,
      GST_TAG_MERGE_REPLACE);
  gst_tag_setter_set_tag_merge_mode (GST_TAG_SETTER (vorbistag),
      GST_TAG_MERGE_KEEP_ALL);
  gst_tag_list_unref (tags);

  start_pipeline (vorbistag);

  /* send identification header */
  inbuffer = gst_buffer_new_and_alloc (sizeof (identification_header));
  gst_buffer_fill (inbuffer, 0, identification_header,
      sizeof (identification_header));
  fail_unless_equals_int (gst_pad_push (mysrcpad, inbuffer), GST_FLOW_OK);

  /* send empty comment buffer */
  inbuffer = gst_buffer_new_and_alloc (sizeof (empty_comment_header));
  gst_buffer_fill (inbuffer, 0, empty_comment_header,
      sizeof (empty_comment_header));
  fail_unless_equals_int (gst_pad_push (mysrcpad, inbuffer), GST_FLOW_OK);

  /* send minimal codebook header and audio packers */
  inbuffer = _create_codebook_header_buffer ();
  fail_unless_equals_int (gst_pad_push (mysrcpad, inbuffer), GST_FLOW_OK);
  inbuffer = _create_audio_buffer ();
  fail_unless_equals_int (gst_pad_push (mysrcpad, inbuffer), GST_FLOW_OK);


  /* check identification header is unchanged */
  outbuffer = get_buffer ();
  compare_buffer (outbuffer, identification_header,
      sizeof (identification_header));
  gst_buffer_unref (outbuffer);

  /* check comment header is correct */
  outbuffer = get_buffer ();
  compare_buffer (outbuffer, title_comment_header,
      sizeof (title_comment_header));
  gst_buffer_unref (outbuffer);

  stop_pipeline (vorbistag);
  cleanup_vorbistag (vorbistag);
}

GST_END_TEST;


GST_START_TEST (test_filled_tags_unset)
{
  GstTagList *tags;
  GstElement *vorbistag;
  GstBuffer *inbuffer, *outbuffer;

  vorbistag = setup_vorbistag ();

  tags = gst_tag_list_new_empty ();
  gst_tag_setter_merge_tags (GST_TAG_SETTER (vorbistag), tags,
      GST_TAG_MERGE_REPLACE);
  gst_tag_setter_set_tag_merge_mode (GST_TAG_SETTER (vorbistag),
      GST_TAG_MERGE_KEEP_ALL);
  gst_tag_list_unref (tags);

  start_pipeline (vorbistag);

  /* send identification header */
  inbuffer = gst_buffer_new_and_alloc (sizeof (identification_header));
  gst_buffer_fill (inbuffer, 0, identification_header,
      sizeof (identification_header));
  fail_unless_equals_int (gst_pad_push (mysrcpad, inbuffer), GST_FLOW_OK);

  /* send empty comment buffer */
  inbuffer = gst_buffer_new_and_alloc (sizeof (title_comment_header));
  gst_buffer_fill (inbuffer, 0, title_comment_header,
      sizeof (title_comment_header));
  fail_unless_equals_int (gst_pad_push (mysrcpad, inbuffer), GST_FLOW_OK);

  /* send minimal codebook header and audio packers */
  inbuffer = _create_codebook_header_buffer ();
  fail_unless_equals_int (gst_pad_push (mysrcpad, inbuffer), GST_FLOW_OK);
  inbuffer = _create_audio_buffer ();
  fail_unless_equals_int (gst_pad_push (mysrcpad, inbuffer), GST_FLOW_OK);


  /* check identification header is unchanged */
  outbuffer = get_buffer ();
  compare_buffer (outbuffer, identification_header,
      sizeof (identification_header));
  gst_buffer_unref (outbuffer);

  /* check comment header is correct */
  outbuffer = get_buffer ();
  compare_buffer (outbuffer, empty_comment_header,
      sizeof (empty_comment_header));
  gst_buffer_unref (outbuffer);

  stop_pipeline (vorbistag);
  cleanup_vorbistag (vorbistag);
}

GST_END_TEST;


GST_START_TEST (test_filled_tags_change)
{
  GstTagList *tags;
  GstElement *vorbistag;
  GstBuffer *inbuffer, *outbuffer;

  vorbistag = setup_vorbistag ();

  tags = gst_tag_list_new_empty ();
  gst_tag_list_add (tags, GST_TAG_MERGE_REPLACE, GST_TAG_TITLE, "foobar", NULL);
  gst_tag_setter_merge_tags (GST_TAG_SETTER (vorbistag), tags,
      GST_TAG_MERGE_REPLACE);
  gst_tag_setter_set_tag_merge_mode (GST_TAG_SETTER (vorbistag),
      GST_TAG_MERGE_KEEP_ALL);
  gst_tag_list_unref (tags);

  start_pipeline (vorbistag);

  /* send identification header */
  inbuffer = gst_buffer_new_and_alloc (sizeof (identification_header));
  gst_buffer_fill (inbuffer, 0, identification_header,
      sizeof (identification_header));
  fail_unless_equals_int (gst_pad_push (mysrcpad, inbuffer), GST_FLOW_OK);

  /* send empty comment buffer */
  inbuffer = gst_buffer_new_and_alloc (sizeof (artist_comment_header));
  gst_buffer_fill (inbuffer, 0, artist_comment_header,
      sizeof (artist_comment_header));
  fail_unless_equals_int (gst_pad_push (mysrcpad, inbuffer), GST_FLOW_OK);

  /* send minimal codebook header and audio packers */
  inbuffer = _create_codebook_header_buffer ();
  fail_unless_equals_int (gst_pad_push (mysrcpad, inbuffer), GST_FLOW_OK);
  inbuffer = _create_audio_buffer ();
  fail_unless_equals_int (gst_pad_push (mysrcpad, inbuffer), GST_FLOW_OK);


  /* check identification header is unchanged */
  outbuffer = get_buffer ();
  compare_buffer (outbuffer, identification_header,
      sizeof (identification_header));
  gst_buffer_unref (outbuffer);

  /* check comment header is correct */
  outbuffer = get_buffer ();
  compare_buffer (outbuffer, title_comment_header,
      sizeof (title_comment_header));
  gst_buffer_unref (outbuffer);

  stop_pipeline (vorbistag);
  cleanup_vorbistag (vorbistag);
}

GST_END_TEST;



static Suite *
vorbistag_suite (void)
{
  Suite *s = suite_create ("vorbistag");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_empty_tags_set);
  tcase_add_test (tc_chain, test_filled_tags_unset);
  tcase_add_test (tc_chain, test_filled_tags_change);

  return s;
}

GST_CHECK_MAIN (vorbistag)
