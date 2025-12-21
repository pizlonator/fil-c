/* GStreamer
 *
 * unit test for volume
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

#include <gst/base/gstbasetransform.h>
#include <gst/check/gstcheck.h>
#include <gst/audio/streamvolume.h>
#include <gst/controller/gstinterpolationcontrolsource.h>
#include <gst/controller/gstdirectcontrolbinding.h>

/* For ease of programming we use globals to keep refs for our floating
 * src and sink pads we create; otherwise we always have to do get_pad,
 * get_peer, and then remove references in every test function */
static GstPad *mysrcpad, *mysinkpad;

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
#define FORMATS1 "{ S8, S16LE, S24LE, S32LE, F32LE, F64LE }"
#define FORMATS2 "S8"
#define FORMATS3 "S16LE"
#define FORMATS4 "S24LE"
#define FORMATS5 "S32LE"
#define FORMATS6 "F32LE"
#define FORMATS7 "F64LE"
#define FORMATS8 "U16LE"
#else
#define FORMATS1 "{ S8, S16BE, S24BE, S32BE, F32BE, F64BE }"
#define FORMATS2 "S8"
#define FORMATS3 "S16BE"
#define FORMATS4 "S24BE"
#define FORMATS5 "S32BE"
#define FORMATS6 "F32BE"
#define FORMATS7 "F64BE"
#define FORMATS8 "U16BE"
#endif

#define VOLUME_CAPS_TEMPLATE_STRING     \
    "audio/x-raw, "                     \
    "format = (string) "FORMATS1", "    \
    "channels = (int) [ 1, MAX ], "     \
    "rate = (int) [ 1,  MAX ], "        \
    "layout = (string) interleaved"

#define VOLUME_CAPS_STRING_S8           \
    "audio/x-raw, "                     \
    "format = (string) "FORMATS2", "   \
    "channels = (int) 1, "              \
    "rate = (int) 44100,"               \
    "layout = (string) interleaved"

#define VOLUME_CAPS_STRING_S16          \
    "audio/x-raw, "                     \
    "format = (string) "FORMATS3", "   \
    "channels = (int) 1, "              \
    "rate = (int) 44100,"               \
    "layout = (string) interleaved"

#define VOLUME_CAPS_STRING_S24          \
    "audio/x-raw, "                     \
    "format = (string) "FORMATS4", "   \
    "channels = (int) 1, "              \
    "rate = (int) 44100,"               \
    "layout = (string) interleaved"

#define VOLUME_CAPS_STRING_S32          \
    "audio/x-raw, "                     \
    "format = (string) "FORMATS5", "   \
    "channels = (int) 1, "              \
    "rate = (int) 44100,"               \
    "layout = (string) interleaved"

#define VOLUME_CAPS_STRING_F32          \
    "audio/x-raw, "                     \
    "format = (string) "FORMATS6", "   \
    "channels = (int) 1, "              \
    "rate = (int) 44100,"               \
    "layout = (string) interleaved"

#define VOLUME_CAPS_STRING_F64          \
    "audio/x-raw, "                     \
    "format = (string) "FORMATS7", "   \
    "channels = (int) 1, "              \
    "rate = (int) 44100,"               \
    "layout = (string) interleaved"

#define VOLUME_WRONG_CAPS_STRING        \
    "audio/x-raw, "                     \
    "format = (string) "FORMATS8", "   \
    "channels = (int) 1, "              \
    "rate = (int) 44100,"               \
    "layout = (string) interleaved"


static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (VOLUME_CAPS_TEMPLATE_STRING)
    );
static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (VOLUME_CAPS_TEMPLATE_STRING)
    );

static GstElement *
setup_volume (void)
{
  GstElement *volume;

  GST_DEBUG ("setup_volume");
  volume = gst_check_setup_element ("volume");
  mysrcpad = gst_check_setup_src_pad (volume, &srctemplate);
  mysinkpad = gst_check_setup_sink_pad (volume, &sinktemplate);
  gst_pad_set_active (mysrcpad, TRUE);
  gst_pad_set_active (mysinkpad, TRUE);

  return volume;
}

static void
cleanup_volume (GstElement * volume)
{
  GST_DEBUG ("cleanup_volume");

  g_list_foreach (buffers, (GFunc) gst_mini_object_unref, NULL);
  g_list_free (buffers);
  buffers = NULL;

  gst_pad_set_active (mysrcpad, FALSE);
  gst_pad_set_active (mysinkpad, FALSE);
  gst_check_teardown_src_pad (volume);
  gst_check_teardown_sink_pad (volume);
  gst_check_teardown_element (volume);
}

GST_START_TEST (test_get_set)
{
  GstElement *volume = gst_element_factory_make ("volume", NULL);
  gdouble val;

  fail_unless (volume != NULL);
  g_object_get (G_OBJECT (volume), "volume", &val, NULL);
  fail_unless (val == 1.0);
  fail_unless (val == gst_stream_volume_get_volume (GST_STREAM_VOLUME (volume),
          GST_STREAM_VOLUME_FORMAT_LINEAR));

  g_object_set (G_OBJECT (volume), "volume", 0.5, NULL);
  g_object_get (G_OBJECT (volume), "volume", &val, NULL);
  fail_unless (val == 0.5);
  fail_unless (val == gst_stream_volume_get_volume (GST_STREAM_VOLUME (volume),
          GST_STREAM_VOLUME_FORMAT_LINEAR));

  gst_stream_volume_set_volume (GST_STREAM_VOLUME (volume),
      GST_STREAM_VOLUME_FORMAT_LINEAR, 1.0);
  g_object_get (G_OBJECT (volume), "volume", &val, NULL);
  fail_unless (val == 1.0);
  fail_unless (val == gst_stream_volume_get_volume (GST_STREAM_VOLUME (volume),
          GST_STREAM_VOLUME_FORMAT_LINEAR));

  gst_object_unref (volume);
}

GST_END_TEST;

GST_START_TEST (test_unity_s8)
{
  GstElement *volume;
  GstBuffer *inbuffer, *outbuffer;
  GstCaps *caps;
  gint8 in[2] = { 64, -16 };
  gint8 *res;
  GstMapInfo map;

  volume = setup_volume ();
  fail_unless (gst_element_set_state (volume,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  inbuffer = gst_buffer_new_and_alloc (2);
  gst_buffer_fill (inbuffer, 0, in, 2);
  caps = gst_caps_from_string (VOLUME_CAPS_STRING_S8);
  gst_check_setup_events (mysrcpad, volume, caps, GST_FORMAT_TIME);
  gst_caps_unref (caps);
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);

  /* pushing gives away my reference ... */
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);
  /* ... but it ends up being collected on the global buffer list */
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  fail_unless_equals_int (g_list_length (buffers), 1);
  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);
  fail_unless (inbuffer == outbuffer);
  gst_buffer_map (outbuffer, &map, GST_MAP_READ);
  res = (gint8 *) map.data;
  GST_INFO ("expected %+5d %+5d  real %+5d %+5d", in[0], in[1], res[0], res[1]);
  fail_unless (memcmp (res, in, 2) == 0);
  gst_buffer_unmap (outbuffer, &map);

  /* cleanup */
  cleanup_volume (volume);
}

GST_END_TEST;

GST_START_TEST (test_half_s8)
{
  GstElement *volume;
  GstBuffer *inbuffer;
  GstBuffer *outbuffer;
  GstCaps *caps;
  gint8 in[2] = { 64, -16 };
  gint8 out[2] = { 32, -8 };
  gint8 *res;
  GstMapInfo map;

  volume = setup_volume ();
  g_object_set (G_OBJECT (volume), "volume", 0.5, NULL);
  fail_unless (gst_element_set_state (volume,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  inbuffer = gst_buffer_new_and_alloc (2);
  gst_buffer_fill (inbuffer, 0, in, 2);
  caps = gst_caps_from_string (VOLUME_CAPS_STRING_S8);
  gst_check_setup_events (mysrcpad, volume, caps, GST_FORMAT_TIME);
  gst_caps_unref (caps);
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  /* FIXME: reffing the inbuffer should make the transformation not be
   * inplace
   gst_buffer_ref (inbuffer);
   */

  /* pushing gives away my reference ... */
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);
  /* ... but it ends up being modified inplace and
   * collected on the global buffer list */
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  fail_unless_equals_int (g_list_length (buffers), 1);
  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);
  fail_unless (inbuffer == outbuffer);
  gst_buffer_map (outbuffer, &map, GST_MAP_READ);
  res = (gint8 *) map.data;
  GST_INFO ("expected %+5d %+5d  real %+5d %+5d", out[0], out[1], res[0],
      res[1]);
  fail_unless (memcmp (res, out, 2) == 0);
  gst_buffer_unmap (outbuffer, &map);

  /* cleanup */
  cleanup_volume (volume);
}

GST_END_TEST;

GST_START_TEST (test_double_s8)
{
  GstElement *volume;
  GstBuffer *inbuffer;
  GstBuffer *outbuffer;
  GstCaps *caps;
  gint8 in[2] = { 64, -16 };
  gint8 out[2] = { 127, -32 };  /* notice the clamped sample */
  gint8 *res;
  GstMapInfo map;

  volume = setup_volume ();
  g_object_set (G_OBJECT (volume), "volume", 2.0, NULL);
  fail_unless (gst_element_set_state (volume,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  inbuffer = gst_buffer_new_and_alloc (2);
  gst_buffer_fill (inbuffer, 0, in, 2);
  caps = gst_caps_from_string (VOLUME_CAPS_STRING_S8);
  gst_check_setup_events (mysrcpad, volume, caps, GST_FORMAT_TIME);
  gst_caps_unref (caps);
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  /* FIXME: reffing the inbuffer should make the transformation not be
   * inplace
   gst_buffer_ref (inbuffer);
   */

  /* pushing gives away my reference ... */
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);
  /* ... but it ends up being modified inplace and
   * collected on the global buffer list */
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  fail_unless_equals_int (g_list_length (buffers), 1);
  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);
  fail_unless (inbuffer == outbuffer);
  gst_buffer_map (outbuffer, &map, GST_MAP_READ);
  res = (gint8 *) map.data;
  GST_INFO ("expected %+5d %+5d  real %+5d %+5d", out[0], out[1], res[0],
      res[1]);
  fail_unless (memcmp (res, out, 2) == 0);
  gst_buffer_unmap (outbuffer, &map);

  /* cleanup */
  cleanup_volume (volume);
}

GST_END_TEST;

GST_START_TEST (test_ten_s8)
{
  GstElement *volume;
  GstBuffer *inbuffer;
  GstBuffer *outbuffer;
  GstCaps *caps;
  gint8 in[2] = { 64, -10 };
  gint8 out[2] = { 127, -100 }; /* notice the clamped sample */
  gint8 *res;
  GstMapInfo map;

  volume = setup_volume ();
  g_object_set (G_OBJECT (volume), "volume", 10.0, NULL);
  fail_unless (gst_element_set_state (volume,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  inbuffer = gst_buffer_new_and_alloc (2);
  gst_buffer_fill (inbuffer, 0, in, 2);
  fail_unless (gst_buffer_memcmp (inbuffer, 0, in, 2) == 0);
  caps = gst_caps_from_string (VOLUME_CAPS_STRING_S8);
  gst_check_setup_events (mysrcpad, volume, caps, GST_FORMAT_TIME);
  gst_caps_unref (caps);
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  /* FIXME: reffing the inbuffer should make the transformation not be
   * inplace
   gst_buffer_ref (inbuffer);
   */

  /* pushing gives away my reference ... */
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);
  /* ... but it ends up being modified inplace and
   * collected on the global buffer list */
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  fail_unless_equals_int (g_list_length (buffers), 1);
  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);
  fail_unless (inbuffer == outbuffer);
  gst_buffer_map (outbuffer, &map, GST_MAP_READ);
  res = (gint8 *) map.data;
  GST_INFO ("expected %+5d %+5d  real %+5d %+5d", out[0], out[1], res[0],
      res[1]);
  fail_unless (memcmp (res, out, 2) == 0);
  gst_buffer_unmap (outbuffer, &map);

  /* cleanup */
  cleanup_volume (volume);
}

GST_END_TEST;

GST_START_TEST (test_fifteen_s8)
{
  GstElement *volume;
  GstBuffer *inbuffer;
  GstBuffer *outbuffer;
  GstCaps *caps;
  gint8 in[4] = { 1, -1, G_MAXINT8, G_MININT8 };
  gint8 out[4] = { 15, -15, G_MAXINT8, G_MININT8 };
  /* notice the clamped sample */
  gint8 *res;
  GstMapInfo map;

  volume = setup_volume ();
  // Maximal volume that is supported by the fixed point multiplication.
  g_object_set (G_OBJECT (volume), "volume-full-range", 15.0, NULL);
  fail_unless (gst_element_set_state (volume, GST_STATE_PLAYING) ==
      GST_STATE_CHANGE_SUCCESS, "could not set to playing");

  inbuffer = gst_buffer_new_and_alloc (4);
  gst_buffer_fill (inbuffer, 0, in, 4);
  fail_unless (gst_buffer_memcmp (inbuffer, 0, in, 4) == 0);
  caps = gst_caps_from_string (VOLUME_CAPS_STRING_S8);
  gst_check_setup_events (mysrcpad, volume, caps, GST_FORMAT_TIME);
  gst_caps_unref (caps);
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  /* FIXME: reffing the inbuffer should make the transformation not be
   * inplace
   gst_buffer_ref (inbuffer);
   */

  /* pushing gives away my reference ... */
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);
  /* ... but it ends up being modified inplace and
   * collected on the global buffer list */
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  fail_unless_equals_int (g_list_length (buffers), 1);
  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);
  fail_unless (inbuffer == outbuffer);
  gst_buffer_map (outbuffer, &map, GST_MAP_READ);
  res = (gint8 *) map.data;
  GST_INFO ("expected %+5d %+5d %+5d %+5d  real %+5d %+5d %+5d %+5d", out[0],
      out[1], out[2], out[3], res[0], res[1], res[2], res[3]);
  fail_unless (memcmp (res, out, 4) == 0);
  gst_buffer_unmap (outbuffer, &map);

  /* cleanup */
  cleanup_volume (volume);
}

GST_END_TEST;

GST_START_TEST (test_sixteen_s8)
{
  GstElement *volume;
  GstBuffer *inbuffer;
  GstBuffer *outbuffer;
  GstCaps *caps;
  gint8 in[4] = { 1, -1, G_MAXINT8, G_MININT8 };
  gint8 out[4] = { 16, -16, G_MAXINT8, G_MININT8 };
  /* notice the clamped sample */
  gint8 *res;
  GstMapInfo map;

  volume = setup_volume ();
  // No longer using fixed point multiplication.
  g_object_set (G_OBJECT (volume), "volume-full-range", 16.0, NULL);
  fail_unless (gst_element_set_state (volume, GST_STATE_PLAYING) ==
      GST_STATE_CHANGE_SUCCESS, "could not set to playing");

  inbuffer = gst_buffer_new_and_alloc (4);
  gst_buffer_fill (inbuffer, 0, in, 4);
  fail_unless (gst_buffer_memcmp (inbuffer, 0, in, 4) == 0);
  caps = gst_caps_from_string (VOLUME_CAPS_STRING_S8);
  gst_check_setup_events (mysrcpad, volume, caps, GST_FORMAT_TIME);
  gst_caps_unref (caps);
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  /* FIXME: reffing the inbuffer should make the transformation not be
   * inplace
   gst_buffer_ref (inbuffer);
   */

  /* pushing gives away my reference ... */
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);
  /* ... but it ends up being modified inplace and
   * collected on the global buffer list */
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  fail_unless_equals_int (g_list_length (buffers), 1);
  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);
  fail_unless (inbuffer == outbuffer);
  gst_buffer_map (outbuffer, &map, GST_MAP_READ);
  res = (gint8 *) map.data;
  GST_INFO ("expected %+5d %+5d %+5d %+5d  real %+5d %+5d %+5d %+5d", out[0],
      out[1], out[2], out[3], res[0], res[1], res[2], res[3]);
  fail_unless (memcmp (res, out, 4) == 0);
  gst_buffer_unmap (outbuffer, &map);

  /* cleanup */
  cleanup_volume (volume);
}

GST_END_TEST;

GST_START_TEST (test_max_s8)
{
  GstElement *volume;
  GstBuffer *inbuffer;
  GstBuffer *outbuffer;
  GstCaps *caps;
  gint8 in[4] = { 1, -1, G_MAXINT8, G_MININT8 };
  gint8 out[4] = { G_MAXINT8, G_MININT8, G_MAXINT8, G_MININT8 };
  /* notice the clamped sample */
  gint8 *res;
  GstMapInfo map;

  volume = setup_volume ();
  // No longer using fixed point multiplication.
  g_object_set (G_OBJECT (volume), "volume-full-range", G_MAXDOUBLE, NULL);
  fail_unless (gst_element_set_state (volume, GST_STATE_PLAYING) ==
      GST_STATE_CHANGE_SUCCESS, "could not set to playing");

  inbuffer = gst_buffer_new_and_alloc (4);
  gst_buffer_fill (inbuffer, 0, in, 4);
  fail_unless (gst_buffer_memcmp (inbuffer, 0, in, 4) == 0);
  caps = gst_caps_from_string (VOLUME_CAPS_STRING_S8);
  gst_check_setup_events (mysrcpad, volume, caps, GST_FORMAT_TIME);
  gst_caps_unref (caps);
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  /* FIXME: reffing the inbuffer should make the transformation not be
   * inplace
   gst_buffer_ref (inbuffer);
   */

  /* pushing gives away my reference ... */
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);
  /* ... but it ends up being modified inplace and
   * collected on the global buffer list */
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  fail_unless_equals_int (g_list_length (buffers), 1);
  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);
  fail_unless (inbuffer == outbuffer);
  gst_buffer_map (outbuffer, &map, GST_MAP_READ);
  res = (gint8 *) map.data;
  GST_INFO ("expected %+5d %+5d %+5d %+5d  real %+5d %+5d %+5d %+5d", out[0],
      out[1], out[2], out[3], res[0], res[1], res[2], res[3]);
  fail_unless (memcmp (res, out, 4) == 0);
  gst_buffer_unmap (outbuffer, &map);

  /* cleanup */
  cleanup_volume (volume);
}

GST_END_TEST;

GST_START_TEST (test_mute_s8)
{
  GstElement *volume;
  GstBuffer *inbuffer;
  GstBuffer *outbuffer;
  GstCaps *caps;
  gint8 in[2] = { 64, -16 };
  gint8 out[2] = { 0, 0 };
  gint8 *res;
  GstMapInfo map;

  volume = setup_volume ();
  g_object_set (G_OBJECT (volume), "mute", TRUE, NULL);
  fail_unless (gst_element_set_state (volume,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  inbuffer = gst_buffer_new_and_alloc (2);
  gst_buffer_fill (inbuffer, 0, in, 2);
  caps = gst_caps_from_string (VOLUME_CAPS_STRING_S8);
  gst_check_setup_events (mysrcpad, volume, caps, GST_FORMAT_TIME);
  gst_caps_unref (caps);
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  /* FIXME: reffing the inbuffer should make the transformation not be
   * inplace
   gst_buffer_ref (inbuffer);
   */

  /* pushing gives away my reference ... */
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);
  /* ... but it ends up being modified inplace and
   * collected on the global buffer list */
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  fail_unless_equals_int (g_list_length (buffers), 1);
  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);
  fail_unless (inbuffer == outbuffer);
  gst_buffer_map (outbuffer, &map, GST_MAP_READ);
  res = (gint8 *) map.data;
  GST_INFO ("expected %+5d %+5d  real %+5d %+5d", out[0], out[1], res[0],
      res[1]);
  fail_unless (memcmp (map.data, out, 2) == 0);
  gst_buffer_unmap (outbuffer, &map);

  /* cleanup */
  cleanup_volume (volume);
}

GST_END_TEST;

GST_START_TEST (test_unity_s16)
{
  GstElement *volume;
  GstBuffer *inbuffer, *outbuffer;
  GstCaps *caps;
  gint16 in[2] = { 16384, -256 };
  gint16 *res;
  GstMapInfo map;

  volume = setup_volume ();
  fail_unless (gst_element_set_state (volume,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  inbuffer = gst_buffer_new_and_alloc (4);
  gst_buffer_fill (inbuffer, 0, in, 4);
  caps = gst_caps_from_string (VOLUME_CAPS_STRING_S16);
  gst_check_setup_events (mysrcpad, volume, caps, GST_FORMAT_TIME);
  gst_caps_unref (caps);
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);

  /* pushing gives away my reference ... */
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);
  /* ... but it ends up being collected on the global buffer list */
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  fail_unless_equals_int (g_list_length (buffers), 1);
  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);
  fail_unless (inbuffer == outbuffer);
  gst_buffer_map (outbuffer, &map, GST_MAP_READ);
  res = (gint16 *) map.data;
  GST_INFO ("expected %+5d %+5d  real %+5d %+5d", in[0], in[1], res[0], res[1]);
  fail_unless (memcmp (map.data, in, 4) == 0);
  gst_buffer_unmap (outbuffer, &map);

  /* cleanup */
  cleanup_volume (volume);
}

GST_END_TEST;

GST_START_TEST (test_half_s16)
{
  GstElement *volume;
  GstBuffer *inbuffer;
  GstBuffer *outbuffer;
  GstCaps *caps;
  gint16 in[2] = { 16384, -256 };
  gint16 out[2] = { 8192, -128 };
  gint16 *res;
  GstMapInfo map;

  volume = setup_volume ();
  g_object_set (G_OBJECT (volume), "volume", 0.5, NULL);
  fail_unless (gst_element_set_state (volume,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  inbuffer = gst_buffer_new_and_alloc (4);
  gst_buffer_fill (inbuffer, 0, in, 4);
  caps = gst_caps_from_string (VOLUME_CAPS_STRING_S16);
  gst_check_setup_events (mysrcpad, volume, caps, GST_FORMAT_TIME);
  gst_caps_unref (caps);
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  /* FIXME: reffing the inbuffer should make the transformation not be
   * inplace
   gst_buffer_ref (inbuffer);
   */

  /* pushing gives away my reference ... */
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);
  /* ... but it ends up being modified inplace and
   * collected on the global buffer list */
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  fail_unless_equals_int (g_list_length (buffers), 1);
  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);
  fail_unless (inbuffer == outbuffer);
  gst_buffer_map (outbuffer, &map, GST_MAP_READ);
  res = (gint16 *) map.data;
  GST_INFO ("expected %+5d %+5d  real %+5d %+5d", in[0], in[1], res[0], res[1]);
  fail_unless (memcmp (map.data, out, 4) == 0);
  gst_buffer_unmap (outbuffer, &map);

  /* cleanup */
  cleanup_volume (volume);
}

GST_END_TEST;

GST_START_TEST (test_double_s16)
{
  GstElement *volume;
  GstBuffer *inbuffer;
  GstBuffer *outbuffer;
  GstCaps *caps;
  gint16 in[2] = { 16384, -256 };
  gint16 out[2] = { 32767, -512 };      /* notice the clamped sample */
  gint16 *res;
  GstMapInfo map;

  volume = setup_volume ();
  g_object_set (G_OBJECT (volume), "volume", 2.0, NULL);
  fail_unless (gst_element_set_state (volume,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  inbuffer = gst_buffer_new_and_alloc (4);
  gst_buffer_fill (inbuffer, 0, in, 4);
  caps = gst_caps_from_string (VOLUME_CAPS_STRING_S16);
  gst_check_setup_events (mysrcpad, volume, caps, GST_FORMAT_TIME);
  gst_caps_unref (caps);
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  /* FIXME: reffing the inbuffer should make the transformation not be
   * inplace
   gst_buffer_ref (inbuffer);
   */

  /* pushing gives away my reference ... */
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);
  /* ... but it ends up being modified inplace and
   * collected on the global buffer list */
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  fail_unless_equals_int (g_list_length (buffers), 1);
  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);
  fail_unless (inbuffer == outbuffer);
  gst_buffer_map (outbuffer, &map, GST_MAP_READ);
  res = (gint16 *) map.data;
  GST_INFO ("expected %+5d %+5d  real %+5d %+5d", in[0], in[1], res[0], res[1]);
  fail_unless (memcmp (map.data, out, 4) == 0);
  gst_buffer_unmap (outbuffer, &map);

  /* cleanup */
  cleanup_volume (volume);
}

GST_END_TEST;

GST_START_TEST (test_ten_s16)
{
  GstElement *volume;
  GstBuffer *inbuffer;
  GstBuffer *outbuffer;
  GstCaps *caps;
  gint16 in[2] = { 16384, -10 };
  gint16 out[2] = { 32767, -100 };      /* notice the clamped sample */
  gint16 *res;
  GstMapInfo map;

  volume = setup_volume ();
  g_object_set (G_OBJECT (volume), "volume", 10.0, NULL);
  fail_unless (gst_element_set_state (volume,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  inbuffer = gst_buffer_new_and_alloc (4);
  gst_buffer_fill (inbuffer, 0, in, 4);
  fail_unless (gst_buffer_memcmp (inbuffer, 0, in, 4) == 0);
  caps = gst_caps_from_string (VOLUME_CAPS_STRING_S16);
  gst_check_setup_events (mysrcpad, volume, caps, GST_FORMAT_TIME);
  gst_caps_unref (caps);
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  /* FIXME: reffing the inbuffer should make the transformation not be
   * inplace
   gst_buffer_ref (inbuffer);
   */

  /* pushing gives away my reference ... */
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);
  /* ... but it ends up being modified inplace and
   * collected on the global buffer list */
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  fail_unless_equals_int (g_list_length (buffers), 1);
  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);
  fail_unless (inbuffer == outbuffer);
  gst_buffer_map (outbuffer, &map, GST_MAP_READ);
  res = (gint16 *) map.data;
  GST_INFO ("expected %+5d %+5d  real %+5d %+5d", in[0], in[1], res[0], res[1]);
  fail_unless (memcmp (map.data, out, 4) == 0);
  gst_buffer_unmap (outbuffer, &map);

  /* cleanup */
  cleanup_volume (volume);
}

GST_END_TEST;

GST_START_TEST (test_fifteen_s16)
{
  GstElement *volume;
  GstBuffer *inbuffer;
  GstBuffer *outbuffer;
  GstCaps *caps;
  gint16 in[4] = { 1, -1, G_MAXINT16, G_MININT16 };
  gint16 out[4] = { 15, -15, G_MAXINT16, G_MININT16 };
  /* notice the clamped sample */
  gint16 *res;
  GstMapInfo map;

  volume = setup_volume ();
  g_object_set (G_OBJECT (volume), "volume-full-range", 15.0, NULL);
  fail_unless (gst_element_set_state (volume, GST_STATE_PLAYING) ==
      GST_STATE_CHANGE_SUCCESS, "could not set to playing");

  inbuffer = gst_buffer_new_and_alloc (8);
  gst_buffer_fill (inbuffer, 0, in, 8);
  fail_unless (gst_buffer_memcmp (inbuffer, 0, in, 8) == 0);
  caps = gst_caps_from_string (VOLUME_CAPS_STRING_S16);
  gst_check_setup_events (mysrcpad, volume, caps, GST_FORMAT_TIME);
  gst_caps_unref (caps);
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  /* FIXME: reffing the inbuffer should make the transformation not be
   * inplace
   gst_buffer_ref (inbuffer);
   */

  /* pushing gives away my reference ... */
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);
  /* ... but it ends up being modified inplace and
   * collected on the global buffer list */
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  fail_unless_equals_int (g_list_length (buffers), 1);
  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);
  fail_unless (inbuffer == outbuffer);
  gst_buffer_map (outbuffer, &map, GST_MAP_READ);
  res = (gint16 *) map.data;
  GST_INFO ("expected %+5d %+5d %+5d %+5d  real %+5d %+5d %+5d %+5d", out[0],
      out[1], out[2], out[3], res[0], res[1], res[2], res[3]);
  fail_unless (memcmp (map.data, out, 8) == 0);
  gst_buffer_unmap (outbuffer, &map);

  /* cleanup */
  cleanup_volume (volume);
}

GST_END_TEST;

GST_START_TEST (test_sixteen_s16)
{
  GstElement *volume;
  GstBuffer *inbuffer;
  GstBuffer *outbuffer;
  GstCaps *caps;
  gint16 in[4] = { 1, -1, G_MAXINT16, G_MININT16 };
  gint16 out[4] = { 16, -16, G_MAXINT16, G_MININT16 };
  /* notice the clamped sample */
  gint16 *res;
  GstMapInfo map;

  volume = setup_volume ();
  g_object_set (G_OBJECT (volume), "volume-full-range", 16.0, NULL);
  fail_unless (gst_element_set_state (volume, GST_STATE_PLAYING) ==
      GST_STATE_CHANGE_SUCCESS, "could not set to playing");

  inbuffer = gst_buffer_new_and_alloc (8);
  gst_buffer_fill (inbuffer, 0, in, 8);
  fail_unless (gst_buffer_memcmp (inbuffer, 0, in, 8) == 0);
  caps = gst_caps_from_string (VOLUME_CAPS_STRING_S16);
  gst_check_setup_events (mysrcpad, volume, caps, GST_FORMAT_TIME);
  gst_caps_unref (caps);
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  /* FIXME: reffing the inbuffer should make the transformation not be
   * inplace
   gst_buffer_ref (inbuffer);
   */

  /* pushing gives away my reference ... */
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);
  /* ... but it ends up being modified inplace and
   * collected on the global buffer list */
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  fail_unless_equals_int (g_list_length (buffers), 1);
  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);
  fail_unless (inbuffer == outbuffer);
  gst_buffer_map (outbuffer, &map, GST_MAP_READ);
  res = (gint16 *) map.data;
  GST_INFO ("expected %+5d %+5d %+5d %+5d  real %+5d %+5d %+5d %+5d", out[0],
      out[1], out[2], out[3], res[0], res[1], res[2], res[3]);
  fail_unless (memcmp (map.data, out, 8) == 0);
  gst_buffer_unmap (outbuffer, &map);

  /* cleanup */
  cleanup_volume (volume);
}

GST_END_TEST;

GST_START_TEST (test_max_s16)
{
  GstElement *volume;
  GstBuffer *inbuffer;
  GstBuffer *outbuffer;
  GstCaps *caps;
  gint16 in[4] = { 1, -1, G_MAXINT16, G_MININT16 };
  gint16 out[4] = { G_MAXINT16, G_MININT16, G_MAXINT16, G_MININT16 };
  /* notice the clamped sample */
  gint16 *res;
  GstMapInfo map;

  volume = setup_volume ();
  g_object_set (G_OBJECT (volume), "volume-full-range", G_MAXDOUBLE, NULL);
  fail_unless (gst_element_set_state (volume, GST_STATE_PLAYING) ==
      GST_STATE_CHANGE_SUCCESS, "could not set to playing");

  inbuffer = gst_buffer_new_and_alloc (8);
  gst_buffer_fill (inbuffer, 0, in, 8);
  fail_unless (gst_buffer_memcmp (inbuffer, 0, in, 8) == 0);
  caps = gst_caps_from_string (VOLUME_CAPS_STRING_S16);
  gst_check_setup_events (mysrcpad, volume, caps, GST_FORMAT_TIME);
  gst_caps_unref (caps);
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  /* FIXME: reffing the inbuffer should make the transformation not be
   * inplace
   gst_buffer_ref (inbuffer);
   */

  /* pushing gives away my reference ... */
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);
  /* ... but it ends up being modified inplace and
   * collected on the global buffer list */
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  fail_unless_equals_int (g_list_length (buffers), 1);
  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);
  fail_unless (inbuffer == outbuffer);
  gst_buffer_map (outbuffer, &map, GST_MAP_READ);
  res = (gint16 *) map.data;
  GST_INFO ("expected %+5d %+5d %+5d %+5d  real %+5d %+5d %+5d %+5d", out[0],
      out[1], out[2], out[3], res[0], res[1], res[2], res[3]);
  fail_unless (memcmp (map.data, out, 8) == 0);
  gst_buffer_unmap (outbuffer, &map);

  /* cleanup */
  cleanup_volume (volume);
}

GST_END_TEST;

GST_START_TEST (test_mute_s16)
{
  GstElement *volume;
  GstBuffer *inbuffer;
  GstBuffer *outbuffer;
  GstCaps *caps;
  gint16 in[2] = { 16384, -256 };
  gint16 out[2] = { 0, 0 };
  gint16 *res;
  GstMapInfo map;

  volume = setup_volume ();
  g_object_set (G_OBJECT (volume), "mute", TRUE, NULL);
  fail_unless (gst_element_set_state (volume,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  inbuffer = gst_buffer_new_and_alloc (4);
  gst_buffer_fill (inbuffer, 0, in, 4);
  caps = gst_caps_from_string (VOLUME_CAPS_STRING_S16);
  gst_check_setup_events (mysrcpad, volume, caps, GST_FORMAT_TIME);
  gst_caps_unref (caps);
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  /* FIXME: reffing the inbuffer should make the transformation not be
   * inplace
   gst_buffer_ref (inbuffer);
   */

  /* pushing gives away my reference ... */
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);
  /* ... but it ends up being modified inplace and
   * collected on the global buffer list */
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  fail_unless_equals_int (g_list_length (buffers), 1);
  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);
  fail_unless (inbuffer == outbuffer);
  gst_buffer_map (outbuffer, &map, GST_MAP_READ);
  res = (gint16 *) map.data;
  GST_INFO ("expected %+5d %+5d  real %+5d %+5d", in[0], in[1], res[0], res[1]);
  fail_unless (memcmp (map.data, out, 4) == 0);
  gst_buffer_unmap (outbuffer, &map);

  /* cleanup */
  cleanup_volume (volume);
}

GST_END_TEST;

#if (G_BYTE_ORDER == G_LITTLE_ENDIAN)
#define get_unaligned_i24(_x) ( (((guint8*)_x)[0]) | ((((guint8*)_x)[1]) << 8) | ((((gint8*)_x)[2]) << 16) )
#define write_unaligned_u24(_x,samp) do { (((guint8*)_x)[0]) = samp & 0xFF; (((guint8*)_x)[1]) = (samp >> 8) & 0xFF; (((guint8*)_x)[2]) = (samp >> 16) & 0xFF; } while (0)
#else /* BIG ENDIAN */
#define get_unaligned_i24(_x) ( (((guint8*)_x)[2]) | ((((guint8*)_x)[1]) << 8) | ((((gint8*)_x)[0]) << 16) )
#define write_unaligned_u24(_x,samp) do { (((guint8*)_x)[0]) = (samp >> 16) & 0xFF; (((guint8*)_x)[1]) = (samp >> 8) & 0xFF; (((guint8*)_x)[2]) = samp & 0xFF; } while (0)
#endif

GST_START_TEST (test_unity_s24)
{
  GstElement *volume;
  GstBuffer *inbuffer, *outbuffer;
  GstCaps *caps;
  gint32 in_32[2] = { 4194304, -4096 };
  guint8 in[6];
  GstMapInfo map;
  gint32 res_32[2];

  write_unaligned_u24 (in, in_32[0]);
  write_unaligned_u24 (in + 3, in_32[1]);

  volume = setup_volume ();
  fail_unless (gst_element_set_state (volume,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  inbuffer = gst_buffer_new_and_alloc (6);
  gst_buffer_fill (inbuffer, 0, in, 6);
  caps = gst_caps_from_string (VOLUME_CAPS_STRING_S24);
  gst_check_setup_events (mysrcpad, volume, caps, GST_FORMAT_TIME);
  gst_caps_unref (caps);
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);

  /* pushing gives away my reference ... */
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);
  /* ... but it ends up being collected on the global buffer list */
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  fail_unless_equals_int (g_list_length (buffers), 1);
  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);
  fail_unless (inbuffer == outbuffer);
  gst_buffer_map (outbuffer, &map, GST_MAP_READ);

  res_32[0] = get_unaligned_i24 (map.data);
  res_32[1] = get_unaligned_i24 ((map.data + 3));

  GST_INFO ("expected %+5d %+5d  real %+5d %+5d", in_32[0], in_32[1], res_32[0],
      res_32[1]);
  fail_unless (memcmp (map.data, in, 6) == 0);
  gst_buffer_unmap (outbuffer, &map);

  /* cleanup */
  cleanup_volume (volume);
}

GST_END_TEST;

GST_START_TEST (test_half_s24)
{
  GstElement *volume;
  GstBuffer *inbuffer;
  GstBuffer *outbuffer;
  GstCaps *caps;
  gint32 in_32[2] = { 4194304, -4096 };
  guint8 in[6];
  GstMapInfo map;
  gint32 res_32[2];
  gint32 out_32[2] = { 2097152, -2048 };

  write_unaligned_u24 (in, in_32[0]);
  write_unaligned_u24 (in + 3, in_32[1]);

  volume = setup_volume ();
  g_object_set (G_OBJECT (volume), "volume", 0.5, NULL);
  fail_unless (gst_element_set_state (volume,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  inbuffer = gst_buffer_new_and_alloc (6);
  gst_buffer_fill (inbuffer, 0, in, 6);
  caps = gst_caps_from_string (VOLUME_CAPS_STRING_S24);
  gst_check_setup_events (mysrcpad, volume, caps, GST_FORMAT_TIME);
  gst_caps_unref (caps);
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  /* FIXME: reffing the inbuffer should make the transformation not be
   * inplace
   gst_buffer_ref (inbuffer);
   */

  /* pushing gives away my reference ... */
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);
  /* ... but it ends up being modified inplace and
   * collected on the global buffer list */
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  fail_unless_equals_int (g_list_length (buffers), 1);
  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);
  fail_unless (inbuffer == outbuffer);
  gst_buffer_map (outbuffer, &map, GST_MAP_READ);

  res_32[0] = get_unaligned_i24 (map.data);
  res_32[1] = get_unaligned_i24 ((map.data + 3));

  GST_INFO ("expected %+5d %+5d  real %+5d %+5d", out_32[0], out_32[1],
      res_32[0], res_32[1]);
  fail_unless (memcmp (res_32, out_32, 8) == 0);
  gst_buffer_unmap (outbuffer, &map);

  /* cleanup */
  cleanup_volume (volume);
}

GST_END_TEST;

GST_START_TEST (test_double_s24)
{
  GstElement *volume;
  GstBuffer *inbuffer;
  GstBuffer *outbuffer;
  GstCaps *caps;
  gint32 in_32[2] = { 4194304, -4096 };
  guint8 in[6];
  GstMapInfo map;
  gint32 res_32[2];
  gint32 out_32[2] = { 8388607, -8192 };        /* notice the clamped sample */

  write_unaligned_u24 (in, in_32[0]);
  write_unaligned_u24 (in + 3, in_32[1]);

  volume = setup_volume ();
  g_object_set (G_OBJECT (volume), "volume", 2.0, NULL);
  fail_unless (gst_element_set_state (volume,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  inbuffer = gst_buffer_new_and_alloc (6);
  gst_buffer_fill (inbuffer, 0, in, 6);
  caps = gst_caps_from_string (VOLUME_CAPS_STRING_S24);
  gst_check_setup_events (mysrcpad, volume, caps, GST_FORMAT_TIME);
  gst_caps_unref (caps);
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  /* FIXME: reffing the inbuffer should make the transformation not be
   * inplace
   gst_buffer_ref (inbuffer);
   */

  /* pushing gives away my reference ... */
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);
  /* ... but it ends up being modified inplace and
   * collected on the global buffer list */
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  fail_unless_equals_int (g_list_length (buffers), 1);
  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);
  fail_unless (inbuffer == outbuffer);
  gst_buffer_map (outbuffer, &map, GST_MAP_READ);

  res_32[0] = get_unaligned_i24 (map.data);
  res_32[1] = get_unaligned_i24 ((map.data + 3));

  GST_INFO ("expected %+5d %+5d  real %+5d %+5d", out_32[0], out_32[1],
      res_32[0], res_32[1]);
  fail_unless (memcmp (res_32, out_32, 8) == 0);
  gst_buffer_unmap (outbuffer, &map);

  /* cleanup */
  cleanup_volume (volume);
}

GST_END_TEST;

GST_START_TEST (test_ten_s24)
{
  GstElement *volume;
  GstBuffer *inbuffer;
  GstBuffer *outbuffer;
  GstCaps *caps;
  gint32 in_32[2] = { 4194304, -10 };
  guint8 in[6];
  GstMapInfo map;
  gint32 res_32[2];
  gint32 out_32[2] = { 8388607, -100 }; /* notice the clamped sample */

  write_unaligned_u24 (in, in_32[0]);
  write_unaligned_u24 (in + 3, in_32[1]);

  volume = setup_volume ();
  g_object_set (G_OBJECT (volume), "volume", 10.0, NULL);
  fail_unless (gst_element_set_state (volume,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  inbuffer = gst_buffer_new_and_alloc (6);
  gst_buffer_fill (inbuffer, 0, in, 6);
  fail_unless (gst_buffer_memcmp (inbuffer, 0, in, 6) == 0);
  caps = gst_caps_from_string (VOLUME_CAPS_STRING_S24);
  gst_check_setup_events (mysrcpad, volume, caps, GST_FORMAT_TIME);
  gst_caps_unref (caps);
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  /* FIXME: reffing the inbuffer should make the transformation not be
   * inplace
   gst_buffer_ref (inbuffer);
   */

  /* pushing gives away my reference ... */
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);
  /* ... but it ends up being modified inplace and
   * collected on the global buffer list */
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  fail_unless_equals_int (g_list_length (buffers), 1);
  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);
  fail_unless (inbuffer == outbuffer);
  gst_buffer_map (outbuffer, &map, GST_MAP_READ);

  res_32[0] = get_unaligned_i24 (map.data);
  res_32[1] = get_unaligned_i24 ((map.data + 3));

  GST_INFO ("expected %+5d %+5d  real %+5d %+5d", out_32[0], out_32[1],
      res_32[0], res_32[1]);
  fail_unless (memcmp (res_32, out_32, 8) == 0);
  gst_buffer_unmap (outbuffer, &map);

  /* cleanup */
  cleanup_volume (volume);
}

GST_END_TEST;

GST_START_TEST (test_fifteen_s24)
{
  GstElement *volume;
  GstBuffer *inbuffer;
  GstBuffer *outbuffer;
  GstCaps *caps;
  gint32 in_32[4] = { 1, -1, 8388607, -8388608 };
  guint8 in[12];
  GstMapInfo map;
  gint32 res_32[4];
  gint32 out_32[4] = { 15, -15, 8388607, -8388608 };
  /* notice the clamped sample */

  write_unaligned_u24 (in, in_32[0]);
  write_unaligned_u24 (in + 3, in_32[1]);
  write_unaligned_u24 (in + 6, in_32[2]);
  write_unaligned_u24 (in + 9, in_32[3]);

  volume = setup_volume ();
  g_object_set (G_OBJECT (volume), "volume-full-range", 15.0, NULL);
  fail_unless (gst_element_set_state (volume, GST_STATE_PLAYING) ==
      GST_STATE_CHANGE_SUCCESS, "could not set to playing");

  inbuffer = gst_buffer_new_and_alloc (12);
  gst_buffer_fill (inbuffer, 0, in, 12);
  fail_unless (gst_buffer_memcmp (inbuffer, 0, in, 12) == 0);
  caps = gst_caps_from_string (VOLUME_CAPS_STRING_S24);
  gst_check_setup_events (mysrcpad, volume, caps, GST_FORMAT_TIME);
  gst_caps_unref (caps);
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  /* FIXME: reffing the inbuffer should make the transformation not be
   * inplace
   gst_buffer_ref (inbuffer);
   */

  /* pushing gives away my reference ... */
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);
  /* ... but it ends up being modified inplace and
   * collected on the global buffer list */
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  fail_unless_equals_int (g_list_length (buffers), 1);
  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);
  fail_unless (inbuffer == outbuffer);
  gst_buffer_map (outbuffer, &map, GST_MAP_READ);

  res_32[0] = get_unaligned_i24 (map.data);
  res_32[1] = get_unaligned_i24 ((map.data + 3));
  res_32[2] = get_unaligned_i24 ((map.data + 6));
  res_32[3] = get_unaligned_i24 ((map.data + 9));

  GST_INFO ("expected %+5d %+5d %+5d %+5d  real %+5d %+5d %+5d %+5d", out_32[0],
      out_32[1], out_32[2], out_32[3], res_32[0], res_32[1], res_32[2],
      res_32[3]);
  fail_unless (memcmp (res_32, out_32, 16) == 0);
  gst_buffer_unmap (outbuffer, &map);

  /* cleanup */
  cleanup_volume (volume);
}

GST_END_TEST;

GST_START_TEST (test_sixteen_s24)
{
  GstElement *volume;
  GstBuffer *inbuffer;
  GstBuffer *outbuffer;
  GstCaps *caps;
  gint32 in_32[4] = { 1, -1, 8388607, -8388608 };
  guint8 in[12];
  GstMapInfo map;
  gint32 res_32[4];
  gint32 out_32[4] = { 16, -16, 8388607, -8388608 };
  /* notice the clamped sample */

  write_unaligned_u24 (in, in_32[0]);
  write_unaligned_u24 (in + 3, in_32[1]);
  write_unaligned_u24 (in + 6, in_32[2]);
  write_unaligned_u24 (in + 9, in_32[3]);

  volume = setup_volume ();
  g_object_set (G_OBJECT (volume), "volume-full-range", 16.0, NULL);
  fail_unless (gst_element_set_state (volume, GST_STATE_PLAYING) ==
      GST_STATE_CHANGE_SUCCESS, "could not set to playing");

  inbuffer = gst_buffer_new_and_alloc (12);
  gst_buffer_fill (inbuffer, 0, in, 12);
  fail_unless (gst_buffer_memcmp (inbuffer, 0, in, 12) == 0);
  caps = gst_caps_from_string (VOLUME_CAPS_STRING_S24);
  gst_check_setup_events (mysrcpad, volume, caps, GST_FORMAT_TIME);
  gst_caps_unref (caps);
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  /* FIXME: reffing the inbuffer should make the transformation not be
   * inplace
   gst_buffer_ref (inbuffer);
   */

  /* pushing gives away my reference ... */
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);
  /* ... but it ends up being modified inplace and
   * collected on the global buffer list */
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  fail_unless_equals_int (g_list_length (buffers), 1);
  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);
  fail_unless (inbuffer == outbuffer);
  gst_buffer_map (outbuffer, &map, GST_MAP_READ);

  res_32[0] = get_unaligned_i24 (map.data);
  res_32[1] = get_unaligned_i24 ((map.data + 3));
  res_32[2] = get_unaligned_i24 ((map.data + 6));
  res_32[3] = get_unaligned_i24 ((map.data + 9));

  GST_INFO ("expected %+5d %+5d %+5d %+5d  real %+5d %+5d %+5d %+5d", out_32[0],
      out_32[1], out_32[2], out_32[3], res_32[0], res_32[1], res_32[2],
      res_32[3]);
  fail_unless (memcmp (res_32, out_32, 16) == 0);
  gst_buffer_unmap (outbuffer, &map);

  /* cleanup */
  cleanup_volume (volume);
}

GST_END_TEST;

GST_START_TEST (test_4095_s24)
{
  GstElement *volume;
  GstBuffer *inbuffer;
  GstBuffer *outbuffer;
  GstCaps *caps;
  gint32 in_32[4] = { 1, -1, 8388607, -8388608 };
  guint8 in[12];
  GstMapInfo map;
  gint32 res_32[4];
  gint32 out_32[4] = { 4095, -4095, 8388607, -8388608 };
  /* notice the clamped sample */

  write_unaligned_u24 (in, in_32[0]);
  write_unaligned_u24 (in + 3, in_32[1]);
  write_unaligned_u24 (in + 6, in_32[2]);
  write_unaligned_u24 (in + 9, in_32[3]);

  volume = setup_volume ();
  g_object_set (G_OBJECT (volume), "volume-full-range", 4095.0, NULL);
  fail_unless (gst_element_set_state (volume, GST_STATE_PLAYING) ==
      GST_STATE_CHANGE_SUCCESS, "could not set to playing");

  inbuffer = gst_buffer_new_and_alloc (12);
  gst_buffer_fill (inbuffer, 0, in, 12);
  fail_unless (gst_buffer_memcmp (inbuffer, 0, in, 12) == 0);
  caps = gst_caps_from_string (VOLUME_CAPS_STRING_S24);
  gst_check_setup_events (mysrcpad, volume, caps, GST_FORMAT_TIME);
  gst_caps_unref (caps);
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  /* FIXME: reffing the inbuffer should make the transformation not be
   * inplace
   gst_buffer_ref (inbuffer);
   */

  /* pushing gives away my reference ... */
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);
  /* ... but it ends up being modified inplace and
   * collected on the global buffer list */
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  fail_unless_equals_int (g_list_length (buffers), 1);
  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);
  fail_unless (inbuffer == outbuffer);
  gst_buffer_map (outbuffer, &map, GST_MAP_READ);

  res_32[0] = get_unaligned_i24 (map.data);
  res_32[1] = get_unaligned_i24 ((map.data + 3));
  res_32[2] = get_unaligned_i24 ((map.data + 6));
  res_32[3] = get_unaligned_i24 ((map.data + 9));

  GST_INFO ("expected %+5d %+5d %+5d %+5d  real %+5d %+5d %+5d %+5d", out_32[0],
      out_32[1], out_32[2], out_32[3], res_32[0], res_32[1], res_32[2],
      res_32[3]);
  fail_unless (memcmp (res_32, out_32, 16) == 0);
  gst_buffer_unmap (outbuffer, &map);

  /* cleanup */
  cleanup_volume (volume);
}

GST_END_TEST;

GST_START_TEST (test_4096_s24)
{
  GstElement *volume;
  GstBuffer *inbuffer;
  GstBuffer *outbuffer;
  GstCaps *caps;
  gint32 in_32[4] = { 1, -1, 8388607, -8388608 };
  guint8 in[12];
  GstMapInfo map;
  gint32 res_32[4];
  gint32 out_32[4] = { 4096, -4096, 8388607, -8388608 };
  /* notice the clamped sample */

  write_unaligned_u24 (in, in_32[0]);
  write_unaligned_u24 (in + 3, in_32[1]);
  write_unaligned_u24 (in + 6, in_32[2]);
  write_unaligned_u24 (in + 9, in_32[3]);

  volume = setup_volume ();
  g_object_set (G_OBJECT (volume), "volume-full-range", 4096.0, NULL);
  fail_unless (gst_element_set_state (volume, GST_STATE_PLAYING) ==
      GST_STATE_CHANGE_SUCCESS, "could not set to playing");

  inbuffer = gst_buffer_new_and_alloc (12);
  gst_buffer_fill (inbuffer, 0, in, 12);
  fail_unless (gst_buffer_memcmp (inbuffer, 0, in, 12) == 0);
  caps = gst_caps_from_string (VOLUME_CAPS_STRING_S24);
  gst_check_setup_events (mysrcpad, volume, caps, GST_FORMAT_TIME);
  gst_caps_unref (caps);
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  /* FIXME: reffing the inbuffer should make the transformation not be
   * inplace
   gst_buffer_ref (inbuffer);
   */

  /* pushing gives away my reference ... */
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);
  /* ... but it ends up being modified inplace and
   * collected on the global buffer list */
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  fail_unless_equals_int (g_list_length (buffers), 1);
  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);
  fail_unless (inbuffer == outbuffer);
  gst_buffer_map (outbuffer, &map, GST_MAP_READ);

  res_32[0] = get_unaligned_i24 (map.data);
  res_32[1] = get_unaligned_i24 ((map.data + 3));
  res_32[2] = get_unaligned_i24 ((map.data + 6));
  res_32[3] = get_unaligned_i24 ((map.data + 9));

  GST_INFO ("expected %+5d %+5d %+5d %+5d  real %+5d %+5d %+5d %+5d", out_32[0],
      out_32[1], out_32[2], out_32[3], res_32[0], res_32[1], res_32[2],
      res_32[3]);
  fail_unless (memcmp (res_32, out_32, 16) == 0);
  gst_buffer_unmap (outbuffer, &map);

  /* cleanup */
  cleanup_volume (volume);
}

GST_END_TEST;

GST_START_TEST (test_max_s24)
{
  GstElement *volume;
  GstBuffer *inbuffer;
  GstBuffer *outbuffer;
  GstCaps *caps;
  gint32 in_32[4] = { 1, -1, 8388607, -8388608 };
  guint8 in[12];
  GstMapInfo map;
  gint32 res_32[4];
  gint32 out_32[4] = { 8388607, -8388608, 8388607, -8388608 };
  /* notice the clamped sample */

  write_unaligned_u24 (in, in_32[0]);
  write_unaligned_u24 (in + 3, in_32[1]);
  write_unaligned_u24 (in + 6, in_32[2]);
  write_unaligned_u24 (in + 9, in_32[3]);

  volume = setup_volume ();
  g_object_set (G_OBJECT (volume), "volume-full-range", G_MAXDOUBLE, NULL);
  fail_unless (gst_element_set_state (volume, GST_STATE_PLAYING) ==
      GST_STATE_CHANGE_SUCCESS, "could not set to playing");

  inbuffer = gst_buffer_new_and_alloc (12);
  gst_buffer_fill (inbuffer, 0, in, 12);
  fail_unless (gst_buffer_memcmp (inbuffer, 0, in, 12) == 0);
  caps = gst_caps_from_string (VOLUME_CAPS_STRING_S24);
  gst_check_setup_events (mysrcpad, volume, caps, GST_FORMAT_TIME);
  gst_caps_unref (caps);
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  /* FIXME: reffing the inbuffer should make the transformation not be
   * inplace
   gst_buffer_ref (inbuffer);
   */

  /* pushing gives away my reference ... */
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);
  /* ... but it ends up being modified inplace and
   * collected on the global buffer list */
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  fail_unless_equals_int (g_list_length (buffers), 1);
  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);
  fail_unless (inbuffer == outbuffer);
  gst_buffer_map (outbuffer, &map, GST_MAP_READ);

  res_32[0] = get_unaligned_i24 (map.data);
  res_32[1] = get_unaligned_i24 ((map.data + 3));
  res_32[2] = get_unaligned_i24 ((map.data + 6));
  res_32[3] = get_unaligned_i24 ((map.data + 9));

  GST_INFO ("expected %+5d %+5d %+5d %+5d  real %+5d %+5d %+5d %+5d", out_32[0],
      out_32[1], out_32[2], out_32[3], res_32[0], res_32[1], res_32[2],
      res_32[3]);
  fail_unless (memcmp (res_32, out_32, 16) == 0);
  gst_buffer_unmap (outbuffer, &map);

  /* cleanup */
  cleanup_volume (volume);
}

GST_END_TEST;

GST_START_TEST (test_mute_s24)
{
  GstElement *volume;
  GstBuffer *inbuffer;
  GstBuffer *outbuffer;
  GstCaps *caps;
  gint32 in_32[2] = { 4194304, -4096 };
  guint8 in[6];
  GstMapInfo map;
  gint32 res_32[2];
  gint32 out_32[2] = { 0, 0 };  /* notice the clamped sample */

  write_unaligned_u24 (in, in_32[0]);
  write_unaligned_u24 (in + 3, in_32[1]);

  volume = setup_volume ();
  g_object_set (G_OBJECT (volume), "mute", TRUE, NULL);
  fail_unless (gst_element_set_state (volume,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  inbuffer = gst_buffer_new_and_alloc (6);
  gst_buffer_fill (inbuffer, 0, in, 6);
  caps = gst_caps_from_string (VOLUME_CAPS_STRING_S24);
  gst_check_setup_events (mysrcpad, volume, caps, GST_FORMAT_TIME);
  gst_caps_unref (caps);
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  /* FIXME: reffing the inbuffer should make the transformation not be
   * inplace
   gst_buffer_ref (inbuffer);
   */

  /* pushing gives away my reference ... */
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);
  /* ... but it ends up being modified inplace and
   * collected on the global buffer list */
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  fail_unless_equals_int (g_list_length (buffers), 1);
  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);
  fail_unless (inbuffer == outbuffer);

  gst_buffer_map (outbuffer, &map, GST_MAP_READ);

  res_32[0] = get_unaligned_i24 (map.data);
  res_32[1] = get_unaligned_i24 ((map.data + 3));

  GST_INFO ("expected %+5d %+5d  real %+5d %+5d", out_32[0], out_32[1],
      res_32[0], res_32[1]);
  fail_unless (memcmp (res_32, out_32, 8) == 0);
  gst_buffer_unmap (outbuffer, &map);

  /* cleanup */
  cleanup_volume (volume);
}

GST_END_TEST;

GST_START_TEST (test_unity_s32)
{
  GstElement *volume;
  GstBuffer *inbuffer, *outbuffer;
  GstCaps *caps;
  gint32 in[2] = { 1073741824, -65536 };
  gint32 *res;
  GstMapInfo map;

  volume = setup_volume ();
  fail_unless (gst_element_set_state (volume,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  inbuffer = gst_buffer_new_and_alloc (8);
  gst_buffer_fill (inbuffer, 0, in, 8);
  caps = gst_caps_from_string (VOLUME_CAPS_STRING_S32);
  gst_check_setup_events (mysrcpad, volume, caps, GST_FORMAT_TIME);
  gst_caps_unref (caps);
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);

  /* pushing gives away my reference ... */
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);
  /* ... but it ends up being collected on the global buffer list */
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  fail_unless_equals_int (g_list_length (buffers), 1);
  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);
  fail_unless (inbuffer == outbuffer);
  gst_buffer_map (outbuffer, &map, GST_MAP_READ);
  res = (gint32 *) map.data;
  GST_INFO ("expected %+5d %+5d  real %+5d %+5d", in[0], in[1], res[0], res[1]);
  fail_unless (memcmp (map.data, in, 8) == 0);
  gst_buffer_unmap (outbuffer, &map);

  /* cleanup */
  cleanup_volume (volume);
}

GST_END_TEST;

GST_START_TEST (test_half_s32)
{
  GstElement *volume;
  GstBuffer *inbuffer;
  GstBuffer *outbuffer;
  GstCaps *caps;
  gint32 in[2] = { 1073741824, -65536 };
  gint32 out[2] = { 536870912, -32768 };
  gint32 *res;
  GstMapInfo map;

  volume = setup_volume ();
  g_object_set (G_OBJECT (volume), "volume", 0.5, NULL);
  fail_unless (gst_element_set_state (volume,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  inbuffer = gst_buffer_new_and_alloc (8);
  gst_buffer_fill (inbuffer, 0, in, 8);
  caps = gst_caps_from_string (VOLUME_CAPS_STRING_S32);
  gst_check_setup_events (mysrcpad, volume, caps, GST_FORMAT_TIME);
  gst_caps_unref (caps);
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  /* FIXME: reffing the inbuffer should make the transformation not be
   * inplace
   gst_buffer_ref (inbuffer);
   */

  /* pushing gives away my reference ... */
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);
  /* ... but it ends up being modified inplace and
   * collected on the global buffer list */
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  fail_unless_equals_int (g_list_length (buffers), 1);
  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);
  fail_unless (inbuffer == outbuffer);
  gst_buffer_map (outbuffer, &map, GST_MAP_READ);
  res = (gint32 *) map.data;
  GST_INFO ("expected %+5d %+5d  real %+5d %+5d", in[0], in[1], res[0], res[1]);
  fail_unless (memcmp (map.data, out, 8) == 0);
  gst_buffer_unmap (outbuffer, &map);

  /* cleanup */
  cleanup_volume (volume);
}

GST_END_TEST;

GST_START_TEST (test_double_s32)
{
  GstElement *volume;
  GstBuffer *inbuffer;
  GstBuffer *outbuffer;
  GstCaps *caps;
  gint32 in[2] = { 1073741824, -65536 };
  gint32 out[2] = { 2147483647, -131072 };      /* notice the clamped sample */
  gint32 *res;
  GstMapInfo map;

  volume = setup_volume ();
  g_object_set (G_OBJECT (volume), "volume", 2.0, NULL);
  fail_unless (gst_element_set_state (volume,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  inbuffer = gst_buffer_new_and_alloc (8);
  gst_buffer_fill (inbuffer, 0, in, 8);
  caps = gst_caps_from_string (VOLUME_CAPS_STRING_S32);
  gst_check_setup_events (mysrcpad, volume, caps, GST_FORMAT_TIME);
  gst_caps_unref (caps);
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  /* FIXME: reffing the inbuffer should make the transformation not be
   * inplace
   gst_buffer_ref (inbuffer);
   */

  /* pushing gives away my reference ... */
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);
  /* ... but it ends up being modified inplace and
   * collected on the global buffer list */
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  fail_unless_equals_int (g_list_length (buffers), 1);
  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);
  fail_unless (inbuffer == outbuffer);
  gst_buffer_map (outbuffer, &map, GST_MAP_READ);
  res = (gint32 *) map.data;
  GST_INFO ("expected %+5d %+5d  real %+5d %+5d", in[0], in[1], res[0], res[1]);
  fail_unless (memcmp (map.data, out, 8) == 0);
  gst_buffer_unmap (outbuffer, &map);

  /* cleanup */
  cleanup_volume (volume);
}

GST_END_TEST;

GST_START_TEST (test_ten_s32)
{
  GstElement *volume;
  GstBuffer *inbuffer;
  GstBuffer *outbuffer;
  GstCaps *caps;
  gint32 in[2] = { 1073741824, -10 };
  gint32 out[2] = { 2147483647, -100 }; /* notice the clamped sample */
  gint32 *res;
  GstMapInfo map;

  volume = setup_volume ();
  g_object_set (G_OBJECT (volume), "volume", 10.0, NULL);
  fail_unless (gst_element_set_state (volume,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  inbuffer = gst_buffer_new_and_alloc (8);
  gst_buffer_fill (inbuffer, 0, in, 8);
  fail_unless (gst_buffer_memcmp (inbuffer, 0, in, 8) == 0);
  caps = gst_caps_from_string (VOLUME_CAPS_STRING_S32);
  gst_check_setup_events (mysrcpad, volume, caps, GST_FORMAT_TIME);
  gst_caps_unref (caps);
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  /* FIXME: reffing the inbuffer should make the transformation not be
   * inplace
   gst_buffer_ref (inbuffer);
   */

  /* pushing gives away my reference ... */
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);
  /* ... but it ends up being modified inplace and
   * collected on the global buffer list */
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  fail_unless_equals_int (g_list_length (buffers), 1);
  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);
  fail_unless (inbuffer == outbuffer);
  gst_buffer_map (outbuffer, &map, GST_MAP_READ);
  res = (gint32 *) map.data;
  GST_INFO ("expected %+5d %+5d  real %+5d %+5d", in[0], in[1], res[0], res[1]);
  fail_unless (memcmp (map.data, out, 8) == 0);
  gst_buffer_unmap (outbuffer, &map);

  /* cleanup */
  cleanup_volume (volume);
}

GST_END_TEST;

GST_START_TEST (test_fifteen_s32)
{
  GstElement *volume;
  GstBuffer *inbuffer;
  GstBuffer *outbuffer;
  GstCaps *caps;
  gint32 in[4] = { 1, -1, G_MAXINT32, G_MININT32 };
  gint32 out[4] = { 15, -15, G_MAXINT32, G_MININT32 };
  /* notice the clamped sample */
  gint32 *res;
  GstMapInfo map;

  volume = setup_volume ();
  g_object_set (G_OBJECT (volume), "volume-full-range", 15.0, NULL);
  fail_unless (gst_element_set_state (volume, GST_STATE_PLAYING) ==
      GST_STATE_CHANGE_SUCCESS, "could not set to playing");

  inbuffer = gst_buffer_new_and_alloc (16);
  gst_buffer_fill (inbuffer, 0, in, 16);
  fail_unless (gst_buffer_memcmp (inbuffer, 0, in, 16) == 0);
  caps = gst_caps_from_string (VOLUME_CAPS_STRING_S32);
  gst_check_setup_events (mysrcpad, volume, caps, GST_FORMAT_TIME);
  gst_caps_unref (caps);
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  /* FIXME: reffing the inbuffer should make the transformation not be
   * inplace
   gst_buffer_ref (inbuffer);
   */

  /* pushing gives away my reference ... */
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);
  /* ... but it ends up being modified inplace and
   * collected on the global buffer list */
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  fail_unless_equals_int (g_list_length (buffers), 1);
  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);
  fail_unless (inbuffer == outbuffer);
  gst_buffer_map (outbuffer, &map, GST_MAP_READ);
  res = (gint32 *) map.data;
  GST_INFO ("expected %+5d %+5d %+5d %+5d  real %+5d %+5d %+5d %+5d", out[0],
      out[1], out[2], out[3], res[0], res[1], res[2], res[3]);
  fail_unless (memcmp (map.data, out, 16) == 0);
  gst_buffer_unmap (outbuffer, &map);

  /* cleanup */
  cleanup_volume (volume);
}

GST_END_TEST;

GST_START_TEST (test_sixteen_s32)
{
  GstElement *volume;
  GstBuffer *inbuffer;
  GstBuffer *outbuffer;
  GstCaps *caps;
  gint32 in[4] = { 1, -1, G_MAXINT32, G_MININT32 };
  gint32 out[4] = { 16, -16, G_MAXINT32, G_MININT32 };
  /* notice the clamped sample */
  gint32 *res;
  GstMapInfo map;

  volume = setup_volume ();
  g_object_set (G_OBJECT (volume), "volume-full-range", 16.0, NULL);
  fail_unless (gst_element_set_state (volume, GST_STATE_PLAYING) ==
      GST_STATE_CHANGE_SUCCESS, "could not set to playing");

  inbuffer = gst_buffer_new_and_alloc (16);
  gst_buffer_fill (inbuffer, 0, in, 16);
  fail_unless (gst_buffer_memcmp (inbuffer, 0, in, 16) == 0);
  caps = gst_caps_from_string (VOLUME_CAPS_STRING_S32);
  gst_check_setup_events (mysrcpad, volume, caps, GST_FORMAT_TIME);
  gst_caps_unref (caps);
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  /* FIXME: reffing the inbuffer should make the transformation not be
   * inplace
   gst_buffer_ref (inbuffer);
   */

  /* pushing gives away my reference ... */
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);
  /* ... but it ends up being modified inplace and
   * collected on the global buffer list */
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  fail_unless_equals_int (g_list_length (buffers), 1);
  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);
  fail_unless (inbuffer == outbuffer);
  gst_buffer_map (outbuffer, &map, GST_MAP_READ);
  res = (gint32 *) map.data;
  GST_INFO ("expected %+5d %+5d %+5d %+5d  real %+5d %+5d %+5d %+5d", out[0],
      out[1], out[2], out[3], res[0], res[1], res[2], res[3]);
  fail_unless (memcmp (map.data, out, 16) == 0);
  gst_buffer_unmap (outbuffer, &map);

  /* cleanup */
  cleanup_volume (volume);
}

GST_END_TEST;

GST_START_TEST (test_max_s32)
{
  GstElement *volume;
  GstBuffer *inbuffer;
  GstBuffer *outbuffer;
  GstCaps *caps;
  gint32 in[4] = { 1, -1, G_MAXINT32, G_MININT32 };
  gint32 out[4] = { G_MAXINT32, G_MININT32, G_MAXINT32, G_MININT32 };
  /* notice the clamped sample */
  gint32 *res;
  GstMapInfo map;

  volume = setup_volume ();
  g_object_set (G_OBJECT (volume), "volume-full-range", G_MAXDOUBLE, NULL);
  fail_unless (gst_element_set_state (volume, GST_STATE_PLAYING) ==
      GST_STATE_CHANGE_SUCCESS, "could not set to playing");

  inbuffer = gst_buffer_new_and_alloc (16);
  gst_buffer_fill (inbuffer, 0, in, 16);
  fail_unless (gst_buffer_memcmp (inbuffer, 0, in, 16) == 0);
  caps = gst_caps_from_string (VOLUME_CAPS_STRING_S32);
  gst_check_setup_events (mysrcpad, volume, caps, GST_FORMAT_TIME);
  gst_caps_unref (caps);
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  /* FIXME: reffing the inbuffer should make the transformation not be
   * inplace
   gst_buffer_ref (inbuffer);
   */

  /* pushing gives away my reference ... */
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);
  /* ... but it ends up being modified inplace and
   * collected on the global buffer list */
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  fail_unless_equals_int (g_list_length (buffers), 1);
  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);
  fail_unless (inbuffer == outbuffer);
  gst_buffer_map (outbuffer, &map, GST_MAP_READ);
  res = (gint32 *) map.data;
  GST_INFO ("expected %+5d %+5d %+5d %+5d  real %+5d %+5d %+5d %+5d", out[0],
      out[1], out[2], out[3], res[0], res[1], res[2], res[3]);
  fail_unless (memcmp (map.data, out, 16) == 0);
  gst_buffer_unmap (outbuffer, &map);

  /* cleanup */
  cleanup_volume (volume);
}

GST_END_TEST;

GST_START_TEST (test_mute_s32)
{
  GstElement *volume;
  GstBuffer *inbuffer;
  GstBuffer *outbuffer;
  GstCaps *caps;
  gint32 in[2] = { 1073741824, -65536 };
  gint32 out[2] = { 0, 0 };
  gint32 *res;
  GstMapInfo map;

  volume = setup_volume ();
  g_object_set (G_OBJECT (volume), "mute", TRUE, NULL);
  fail_unless (gst_element_set_state (volume,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  inbuffer = gst_buffer_new_and_alloc (8);
  gst_buffer_fill (inbuffer, 0, in, 8);
  caps = gst_caps_from_string (VOLUME_CAPS_STRING_S32);
  gst_check_setup_events (mysrcpad, volume, caps, GST_FORMAT_TIME);
  gst_caps_unref (caps);
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  /* FIXME: reffing the inbuffer should make the transformation not be
   * inplace
   gst_buffer_ref (inbuffer);
   */

  /* pushing gives away my reference ... */
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);
  /* ... but it ends up being modified inplace and
   * collected on the global buffer list */
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  fail_unless_equals_int (g_list_length (buffers), 1);
  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);
  fail_unless (inbuffer == outbuffer);
  gst_buffer_map (outbuffer, &map, GST_MAP_READ);
  res = (gint32 *) map.data;
  GST_INFO ("expected %+5d %+5d  real %+5d %+5d", in[0], in[1], res[0], res[1]);
  fail_unless (memcmp (map.data, out, 8) == 0);
  gst_buffer_unmap (outbuffer, &map);

  /* cleanup */
  cleanup_volume (volume);
}

GST_END_TEST;

GST_START_TEST (test_unity_f32)
{
  GstElement *volume;
  GstBuffer *inbuffer, *outbuffer;
  GstCaps *caps;
  gfloat in[2] = { 0.75, -0.25 }, *res;
  GstMapInfo map;

  volume = setup_volume ();
  fail_unless (gst_element_set_state (volume,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  inbuffer = gst_buffer_new_and_alloc (8);
  gst_buffer_fill (inbuffer, 0, in, sizeof (in));
  caps = gst_caps_from_string (VOLUME_CAPS_STRING_F32);
  gst_check_setup_events (mysrcpad, volume, caps, GST_FORMAT_TIME);
  gst_caps_unref (caps);
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);

  /* pushing gives away my reference ... */
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);
  /* ... but it ends up being collected on the global buffer list */
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  fail_unless_equals_int (g_list_length (buffers), 1);
  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);
  fail_unless (inbuffer == outbuffer);
  gst_buffer_map (outbuffer, &map, GST_MAP_READ);
  res = (gfloat *) map.data;
  GST_INFO ("expected %+1.4f %+1.4f  real %+1.4f %+1.4f", in[0], in[1],
      res[0], res[1]);
  fail_unless_equals_float (res[0], in[0]);
  fail_unless_equals_float (res[1], in[1]);
  gst_buffer_unmap (outbuffer, &map);

  /* cleanup */
  cleanup_volume (volume);
}

GST_END_TEST;

GST_START_TEST (test_half_f32)
{
  GstElement *volume;
  GstBuffer *inbuffer;
  GstBuffer *outbuffer;
  GstCaps *caps;
  gfloat in[2] = { 0.75, -0.25 };
  gfloat out[2] = { 0.375, -0.125 };
  gfloat *res;
  GstMapInfo map;

  volume = setup_volume ();
  g_object_set (G_OBJECT (volume), "volume", 0.5, NULL);
  fail_unless (gst_element_set_state (volume,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  inbuffer = gst_buffer_new_and_alloc (8);
  gst_buffer_fill (inbuffer, 0, in, 8);
  caps = gst_caps_from_string (VOLUME_CAPS_STRING_F32);
  gst_check_setup_events (mysrcpad, volume, caps, GST_FORMAT_TIME);
  gst_caps_unref (caps);
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  /* FIXME: reffing the inbuffer should make the transformation not be
   * inplace
   gst_buffer_ref (inbuffer);
   */

  /* pushing gives away my reference ... */
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);
  /* ... but it ends up being modified inplace and
   * collected on the global buffer list */
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  fail_unless_equals_int (g_list_length (buffers), 1);
  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);
  fail_unless (inbuffer == outbuffer);
  gst_buffer_map (outbuffer, &map, GST_MAP_READ);
  res = (gfloat *) map.data;
  GST_INFO ("expected %+1.4f %+1.4f  real %+1.4f %+1.4f", out[0], out[1],
      res[0], res[1]);
  fail_unless_equals_float (res[0], out[0]);
  fail_unless_equals_float (res[1], out[1]);
  gst_buffer_unmap (outbuffer, &map);

  /* cleanup */
  cleanup_volume (volume);
}

GST_END_TEST;

GST_START_TEST (test_double_f32)
{
  GstElement *volume;
  GstBuffer *inbuffer;
  GstBuffer *outbuffer;
  GstCaps *caps;
  gfloat in[2] = { 0.75, -0.25 };
  gfloat out[2] = { 1.5, -0.5 };        /* nothing is clamped */
  gfloat *res;
  GstMapInfo map;

  volume = setup_volume ();
  g_object_set (G_OBJECT (volume), "volume", 2.0, NULL);
  fail_unless (gst_element_set_state (volume,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  inbuffer = gst_buffer_new_and_alloc (8);
  gst_buffer_fill (inbuffer, 0, in, 8);
  caps = gst_caps_from_string (VOLUME_CAPS_STRING_F32);
  gst_check_setup_events (mysrcpad, volume, caps, GST_FORMAT_TIME);
  gst_caps_unref (caps);
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  /* FIXME: reffing the inbuffer should make the transformation not be
   * inplace
   gst_buffer_ref (inbuffer);
   */

  /* pushing gives away my reference ... */
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);
  /* ... but it ends up being modified inplace and
   * collected on the global buffer list */
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  fail_unless_equals_int (g_list_length (buffers), 1);
  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);
  fail_unless (inbuffer == outbuffer);
  gst_buffer_map (outbuffer, &map, GST_MAP_READ);
  res = (gfloat *) map.data;
  GST_INFO ("expected %+1.4f %+1.4f  real %+1.4f %+1.4f", out[0], out[1],
      res[0], res[1]);
  fail_unless_equals_float (res[0], out[0]);
  fail_unless_equals_float (res[1], out[1]);
  gst_buffer_unmap (outbuffer, &map);

  /* cleanup */
  cleanup_volume (volume);
}

GST_END_TEST;

GST_START_TEST (test_ten_f32)
{
  GstElement *volume;
  GstBuffer *inbuffer;
  GstBuffer *outbuffer;
  GstCaps *caps;
  gfloat in[2] = { 0.75, -0.25 };
  gfloat out[2] = { 7.5, -2.5 };        /* nothing is clamped */
  gfloat *res;
  GstMapInfo map;

  volume = setup_volume ();
  g_object_set (G_OBJECT (volume), "volume", 10.0, NULL);
  fail_unless (gst_element_set_state (volume,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  inbuffer = gst_buffer_new_and_alloc (8);
  gst_buffer_fill (inbuffer, 0, in, 8);
  fail_unless (gst_buffer_memcmp (inbuffer, 0, in, 8) == 0);
  caps = gst_caps_from_string (VOLUME_CAPS_STRING_F32);
  gst_check_setup_events (mysrcpad, volume, caps, GST_FORMAT_TIME);
  gst_caps_unref (caps);
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  /* FIXME: reffing the inbuffer should make the transformation not be
   * inplace
   gst_buffer_ref (inbuffer);
   */

  /* pushing gives away my reference ... */
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);
  /* ... but it ends up being modified inplace and
   * collected on the global buffer list */
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  fail_unless_equals_int (g_list_length (buffers), 1);
  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);
  fail_unless (inbuffer == outbuffer);
  gst_buffer_map (outbuffer, &map, GST_MAP_READ);
  res = (gfloat *) map.data;
  GST_INFO ("expected %+1.4f %+1.4f  real %+1.4f %+1.4f", out[0], out[1],
      res[0], res[1]);
  fail_unless_equals_float (res[0], out[0]);
  fail_unless_equals_float (res[1], out[1]);
  gst_buffer_unmap (outbuffer, &map);

  /* cleanup */
  cleanup_volume (volume);
}

GST_END_TEST;

GST_START_TEST (test_fifteen_f32)
{
  GstElement *volume;
  GstBuffer *inbuffer;
  GstBuffer *outbuffer;
  GstCaps *caps;
  gfloat in[4] = { 1.0, -1.0, 2.0, -2.0 };
  gfloat out[4] = { 15.0, -15.0, 30.0, -30.0 }; /* nothing is clamped */
  gfloat *res;
  GstMapInfo map;

  volume = setup_volume ();
  g_object_set (G_OBJECT (volume), "volume-full-range", 15.0, NULL);
  fail_unless (gst_element_set_state (volume, GST_STATE_PLAYING) ==
      GST_STATE_CHANGE_SUCCESS, "could not set to playing");

  inbuffer = gst_buffer_new_and_alloc (16);
  gst_buffer_fill (inbuffer, 0, in, 16);
  fail_unless (gst_buffer_memcmp (inbuffer, 0, in, 16) == 0);
  caps = gst_caps_from_string (VOLUME_CAPS_STRING_F32);
  gst_check_setup_events (mysrcpad, volume, caps, GST_FORMAT_TIME);
  gst_caps_unref (caps);
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  /* FIXME: reffing the inbuffer should make the transformation not be
   * inplace
   gst_buffer_ref (inbuffer);
   */

  /* pushing gives away my reference ... */
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);
  /* ... but it ends up being modified inplace and
   * collected on the global buffer list */
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  fail_unless_equals_int (g_list_length (buffers), 1);
  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);
  fail_unless (inbuffer == outbuffer);
  gst_buffer_map (outbuffer, &map, GST_MAP_READ);
  res = (gfloat *) map.data;
  GST_INFO
      ("expected %+1.4f %+1.4f %+1.4f %+1.4f  real %+1.4f %+1.4f %+1.4f %+1.4f",
      out[0], out[1], out[2], out[3], res[0], res[1], res[2], res[3]);
  fail_unless_equals_float (res[0], out[0]);
  fail_unless_equals_float (res[1], out[1]);
  fail_unless_equals_float (res[2], out[2]);
  fail_unless_equals_float (res[3], out[3]);
  gst_buffer_unmap (outbuffer, &map);

  /* cleanup */
  cleanup_volume (volume);
}

GST_END_TEST;

GST_START_TEST (test_sixteen_f32)
{
  GstElement *volume;
  GstBuffer *inbuffer;
  GstBuffer *outbuffer;
  GstCaps *caps;
  gfloat in[4] = { 1.0, -1.0, 2.0, -2.0 };
  gfloat out[4] = { 16.0, -16.0, 32.0, -32.0 }; /* nothing is clamped */
  gfloat *res;
  GstMapInfo map;

  volume = setup_volume ();
  g_object_set (G_OBJECT (volume), "volume-full-range", 16.0, NULL);
  fail_unless (gst_element_set_state (volume, GST_STATE_PLAYING) ==
      GST_STATE_CHANGE_SUCCESS, "could not set to playing");

  inbuffer = gst_buffer_new_and_alloc (16);
  gst_buffer_fill (inbuffer, 0, in, 16);
  fail_unless (gst_buffer_memcmp (inbuffer, 0, in, 16) == 0);
  caps = gst_caps_from_string (VOLUME_CAPS_STRING_F32);
  gst_check_setup_events (mysrcpad, volume, caps, GST_FORMAT_TIME);
  gst_caps_unref (caps);
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  /* FIXME: reffing the inbuffer should make the transformation not be
   * inplace
   gst_buffer_ref (inbuffer);
   */

  /* pushing gives away my reference ... */
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);
  /* ... but it ends up being modified inplace and
   * collected on the global buffer list */
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  fail_unless_equals_int (g_list_length (buffers), 1);
  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);
  fail_unless (inbuffer == outbuffer);
  gst_buffer_map (outbuffer, &map, GST_MAP_READ);
  res = (gfloat *) map.data;
  GST_INFO
      ("expected %+1.4f %+1.4f %+1.4f %+1.4f  real %+1.4f %+1.4f %+1.4f %+1.4f",
      out[0], out[1], out[2], out[3], res[0], res[1], res[2], res[3]);
  fail_unless_equals_float (res[0], out[0]);
  fail_unless_equals_float (res[1], out[1]);
  fail_unless_equals_float (res[2], out[2]);
  fail_unless_equals_float (res[3], out[3]);
  gst_buffer_unmap (outbuffer, &map);

  /* cleanup */
  cleanup_volume (volume);
}

GST_END_TEST;

GST_START_TEST (test_max_f32)
{
  GstElement *volume;
  GstBuffer *inbuffer;
  GstBuffer *outbuffer;
  GstCaps *caps;
  gfloat in[4] = { 1.0, -1.0, 2.0, -2.0 };
  gfloat out[4] = { INFINITY, -INFINITY, INFINITY,
    -INFINITY
  };                            /* nothing is clamped */
  gfloat *res;
  GstMapInfo map;

  volume = setup_volume ();
  g_object_set (G_OBJECT (volume), "volume-full-range", G_MAXDOUBLE, NULL);
  fail_unless (gst_element_set_state (volume, GST_STATE_PLAYING) ==
      GST_STATE_CHANGE_SUCCESS, "could not set to playing");

  inbuffer = gst_buffer_new_and_alloc (16);
  gst_buffer_fill (inbuffer, 0, in, 16);
  fail_unless (gst_buffer_memcmp (inbuffer, 0, in, 16) == 0);
  caps = gst_caps_from_string (VOLUME_CAPS_STRING_F32);
  gst_check_setup_events (mysrcpad, volume, caps, GST_FORMAT_TIME);
  gst_caps_unref (caps);
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  /* FIXME: reffing the inbuffer should make the transformation not be
   * inplace
   gst_buffer_ref (inbuffer);
   */

  /* pushing gives away my reference ... */
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);
  /* ... but it ends up being modified inplace and
   * collected on the global buffer list */
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  fail_unless_equals_int (g_list_length (buffers), 1);
  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);
  fail_unless (inbuffer == outbuffer);
  gst_buffer_map (outbuffer, &map, GST_MAP_READ);
  res = (gfloat *) map.data;
  GST_INFO
      ("expected %+1.4f %+1.4f %+1.4f %+1.4f  real %+1.4f %+1.4f %+1.4f %+1.4f",
      out[0], out[1], out[2], out[3], res[0], res[1], res[2], res[3]);
  fail_unless (memcmp (map.data, out, 16) == 0);
  gst_buffer_unmap (outbuffer, &map);

  /* cleanup */
  cleanup_volume (volume);
}

GST_END_TEST;

GST_START_TEST (test_mute_f32)
{
  GstElement *volume;
  GstBuffer *inbuffer;
  GstBuffer *outbuffer;
  GstCaps *caps;
  gfloat in[2] = { 0.75, -0.25 };
  gfloat out[2] = { 0, 0 };
  gfloat *res;
  GstMapInfo map;

  volume = setup_volume ();
  g_object_set (G_OBJECT (volume), "mute", TRUE, NULL);
  fail_unless (gst_element_set_state (volume,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  inbuffer = gst_buffer_new_and_alloc (8);
  gst_buffer_fill (inbuffer, 0, in, 8);
  caps = gst_caps_from_string (VOLUME_CAPS_STRING_F32);
  gst_check_setup_events (mysrcpad, volume, caps, GST_FORMAT_TIME);
  gst_caps_unref (caps);
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  /* FIXME: reffing the inbuffer should make the transformation not be
   * inplace
   gst_buffer_ref (inbuffer);
   */

  /* pushing gives away my reference ... */
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);
  /* ... but it ends up being modified inplace and
   * collected on the global buffer list */
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  fail_unless_equals_int (g_list_length (buffers), 1);
  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);
  fail_unless (inbuffer == outbuffer);
  gst_buffer_map (outbuffer, &map, GST_MAP_READ);
  res = (gfloat *) map.data;
  GST_INFO ("expected %+1.4f %+1.4f  real %+1.4f %+1.4f", out[0], out[1],
      res[0], res[1]);
  fail_unless_equals_float (res[0], out[0]);
  fail_unless_equals_float (res[1], out[1]);
  gst_buffer_unmap (outbuffer, &map);

  /* cleanup */
  cleanup_volume (volume);
}

GST_END_TEST;

GST_START_TEST (test_unity_f64)
{
  GstElement *volume;
  GstBuffer *inbuffer, *outbuffer;
  GstCaps *caps;
  gdouble in[2] = { 0.75, -0.25 };
  gdouble *res;
  GstMapInfo map;

  volume = setup_volume ();
  fail_unless (gst_element_set_state (volume,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  inbuffer = gst_buffer_new_and_alloc (16);
  gst_buffer_fill (inbuffer, 0, in, 16);
  caps = gst_caps_from_string (VOLUME_CAPS_STRING_F64);
  gst_check_setup_events (mysrcpad, volume, caps, GST_FORMAT_TIME);
  gst_caps_unref (caps);
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);

  /* pushing gives away my reference ... */
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);
  /* ... but it ends up being collected on the global buffer list */
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  fail_unless_equals_int (g_list_length (buffers), 1);
  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);
  fail_unless (inbuffer == outbuffer);
  gst_buffer_map (outbuffer, &map, GST_MAP_READ);
  res = (gdouble *) map.data;
  GST_INFO ("expected %+1.4f %+1.4f  real %+1.4f %+1.4f", in[0], in[1],
      res[0], res[1]);
  fail_unless_equals_float (res[0], in[0]);
  fail_unless_equals_float (res[1], in[1]);
  gst_buffer_unmap (outbuffer, &map);

  /* cleanup */
  cleanup_volume (volume);
}

GST_END_TEST;

GST_START_TEST (test_half_f64)
{
  GstElement *volume;
  GstBuffer *inbuffer;
  GstBuffer *outbuffer;
  GstCaps *caps;
  gdouble in[2] = { 0.75, -0.25 };
  gdouble out[2] = { 0.375, -0.125 };
  gdouble *res;
  GstMapInfo map;

  volume = setup_volume ();
  g_object_set (G_OBJECT (volume), "volume", 0.5, NULL);
  fail_unless (gst_element_set_state (volume,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  inbuffer = gst_buffer_new_and_alloc (16);
  gst_buffer_fill (inbuffer, 0, in, 16);
  caps = gst_caps_from_string (VOLUME_CAPS_STRING_F64);
  gst_check_setup_events (mysrcpad, volume, caps, GST_FORMAT_TIME);
  gst_caps_unref (caps);
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  /* FIXME: reffing the inbuffer should make the transformation not be
   * inplace
   gst_buffer_ref (inbuffer);
   */

  /* pushing gives away my reference ... */
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);
  /* ... but it ends up being modified inplace and
   * collected on the global buffer list */
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  fail_unless_equals_int (g_list_length (buffers), 1);
  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);
  fail_unless (inbuffer == outbuffer);
  gst_buffer_map (outbuffer, &map, GST_MAP_READ);
  res = (gdouble *) map.data;
  GST_INFO ("expected %+1.4f %+1.4f  real %+1.4f %+1.4f", out[0], out[1],
      res[0], res[1]);
  fail_unless_equals_float (res[0], out[0]);
  fail_unless_equals_float (res[1], out[1]);
  gst_buffer_unmap (outbuffer, &map);

  /* cleanup */
  cleanup_volume (volume);
}

GST_END_TEST;

GST_START_TEST (test_double_f64)
{
  GstElement *volume;
  GstBuffer *inbuffer;
  GstBuffer *outbuffer;
  GstCaps *caps;
  gdouble in[2] = { 0.75, -0.25 };
  gdouble out[2] = { 1.5, -0.5 };       /* nothing is clamped */
  gdouble *res;
  GstMapInfo map;

  volume = setup_volume ();
  g_object_set (G_OBJECT (volume), "volume", 2.0, NULL);
  fail_unless (gst_element_set_state (volume,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  inbuffer = gst_buffer_new_and_alloc (16);
  gst_buffer_fill (inbuffer, 0, in, 16);
  caps = gst_caps_from_string (VOLUME_CAPS_STRING_F64);
  gst_check_setup_events (mysrcpad, volume, caps, GST_FORMAT_TIME);
  gst_caps_unref (caps);
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  /* FIXME: reffing the inbuffer should make the transformation not be
   * inplace
   gst_buffer_ref (inbuffer);
   */

  /* pushing gives away my reference ... */
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);
  /* ... but it ends up being modified inplace and
   * collected on the global buffer list */
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  fail_unless_equals_int (g_list_length (buffers), 1);
  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);
  fail_unless (inbuffer == outbuffer);
  gst_buffer_map (outbuffer, &map, GST_MAP_READ);
  res = (gdouble *) map.data;
  GST_INFO ("expected %+1.4f %+1.4f  real %+1.4f %+1.4f", out[0], out[1],
      res[0], res[1]);
  fail_unless_equals_float (res[0], out[0]);
  fail_unless_equals_float (res[1], out[1]);
  gst_buffer_unmap (outbuffer, &map);

  /* cleanup */
  cleanup_volume (volume);
}

GST_END_TEST;

GST_START_TEST (test_ten_f64)
{
  GstElement *volume;
  GstBuffer *inbuffer;
  GstBuffer *outbuffer;
  GstCaps *caps;
  gdouble in[2] = { 0.75, -0.25 };
  gdouble out[2] = { 7.5, -2.5 };       /* nothing is clamped */
  gdouble *res;
  GstMapInfo map;

  volume = setup_volume ();
  g_object_set (G_OBJECT (volume), "volume", 10.0, NULL);
  fail_unless (gst_element_set_state (volume,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  inbuffer = gst_buffer_new_and_alloc (16);
  gst_buffer_fill (inbuffer, 0, in, 16);
  fail_unless (gst_buffer_memcmp (inbuffer, 0, in, 16) == 0);
  caps = gst_caps_from_string (VOLUME_CAPS_STRING_F64);
  gst_check_setup_events (mysrcpad, volume, caps, GST_FORMAT_TIME);
  gst_caps_unref (caps);
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  /* FIXME: reffing the inbuffer should make the transformation not be
   * inplace
   gst_buffer_ref (inbuffer);
   */

  /* pushing gives away my reference ... */
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);
  /* ... but it ends up being modified inplace and
   * collected on the global buffer list */
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  fail_unless_equals_int (g_list_length (buffers), 1);
  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);
  fail_unless (inbuffer == outbuffer);
  gst_buffer_map (outbuffer, &map, GST_MAP_READ);
  res = (gdouble *) map.data;
  GST_INFO ("expected %+1.4f %+1.4f  real %+1.4f %+1.4f", out[0], out[1],
      res[0], res[1]);
  fail_unless_equals_float (res[0], out[0]);
  fail_unless_equals_float (res[1], out[1]);
  gst_buffer_unmap (outbuffer, &map);

  /* cleanup */
  cleanup_volume (volume);
}

GST_END_TEST;

GST_START_TEST (test_fifteen_f64)
{
  GstElement *volume;
  GstBuffer *inbuffer;
  GstBuffer *outbuffer;
  GstCaps *caps;
  gdouble in[4] = { 1.0, -1.0, 2.0, -2.0 };
  gdouble out[4] = { 15.0, -15.0, 30.0, -30.0 };        /* nothing is clamped */
  gdouble *res;
  GstMapInfo map;

  volume = setup_volume ();
  g_object_set (G_OBJECT (volume), "volume-full-range", 15.0, NULL);
  fail_unless (gst_element_set_state (volume, GST_STATE_PLAYING) ==
      GST_STATE_CHANGE_SUCCESS, "could not set to playing");

  inbuffer = gst_buffer_new_and_alloc (32);
  gst_buffer_fill (inbuffer, 0, in, 32);
  fail_unless (gst_buffer_memcmp (inbuffer, 0, in, 32) == 0);
  caps = gst_caps_from_string (VOLUME_CAPS_STRING_F64);
  gst_check_setup_events (mysrcpad, volume, caps, GST_FORMAT_TIME);
  gst_caps_unref (caps);
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  /* FIXME: reffing the inbuffer should make the transformation not be
   * inplace
   gst_buffer_ref (inbuffer);
   */

  /* pushing gives away my reference ... */
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);
  /* ... but it ends up being modified inplace and
   * collected on the global buffer list */
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  fail_unless_equals_int (g_list_length (buffers), 1);
  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);
  fail_unless (inbuffer == outbuffer);
  gst_buffer_map (outbuffer, &map, GST_MAP_READ);
  res = (gdouble *) map.data;
  GST_INFO
      ("expected %+1.4f %+1.4f %+1.4f %+1.4f  real %+1.4f %+1.4f %+1.4f %+1.4f",
      out[0], out[1], out[2], out[3], res[0], res[1], res[2], res[3]);
  fail_unless_equals_float (res[0], out[0]);
  fail_unless_equals_float (res[1], out[1]);
  fail_unless_equals_float (res[2], out[2]);
  fail_unless_equals_float (res[3], out[3]);
  gst_buffer_unmap (outbuffer, &map);

  /* cleanup */
  cleanup_volume (volume);
}

GST_END_TEST;

GST_START_TEST (test_sixteen_f64)
{
  GstElement *volume;
  GstBuffer *inbuffer;
  GstBuffer *outbuffer;
  GstCaps *caps;
  gdouble in[4] = { 1.0, -1.0, 2.0, -2.0 };
  gdouble out[4] = { 16.0, -16.0, 32.0, -32.0 };        /* nothing is clamped */
  gdouble *res;
  GstMapInfo map;

  volume = setup_volume ();
  g_object_set (G_OBJECT (volume), "volume-full-range", 16.0, NULL);
  fail_unless (gst_element_set_state (volume, GST_STATE_PLAYING) ==
      GST_STATE_CHANGE_SUCCESS, "could not set to playing");

  inbuffer = gst_buffer_new_and_alloc (32);
  gst_buffer_fill (inbuffer, 0, in, 32);
  fail_unless (gst_buffer_memcmp (inbuffer, 0, in, 32) == 0);
  caps = gst_caps_from_string (VOLUME_CAPS_STRING_F64);
  gst_check_setup_events (mysrcpad, volume, caps, GST_FORMAT_TIME);
  gst_caps_unref (caps);
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  /* FIXME: reffing the inbuffer should make the transformation not be
   * inplace
   gst_buffer_ref (inbuffer);
   */

  /* pushing gives away my reference ... */
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);
  /* ... but it ends up being modified inplace and
   * collected on the global buffer list */
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  fail_unless_equals_int (g_list_length (buffers), 1);
  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);
  fail_unless (inbuffer == outbuffer);
  gst_buffer_map (outbuffer, &map, GST_MAP_READ);
  res = (gdouble *) map.data;
  GST_INFO
      ("expected %+1.4f %+1.4f %+1.4f %+1.4f  real %+1.4f %+1.4f %+1.4f %+1.4f",
      out[0], out[1], out[2], out[3], res[0], res[1], res[2], res[3]);
  fail_unless_equals_float (res[0], out[0]);
  fail_unless_equals_float (res[1], out[1]);
  fail_unless_equals_float (res[2], out[2]);
  fail_unless_equals_float (res[3], out[3]);
  gst_buffer_unmap (outbuffer, &map);

  /* cleanup */
  cleanup_volume (volume);
}

GST_END_TEST;

GST_START_TEST (test_max_f64)
{
  GstElement *volume;
  GstBuffer *inbuffer;
  GstBuffer *outbuffer;
  GstCaps *caps;
  gdouble in[4] = { 1.0, -1.0, 2.0, -2.0 };
  gdouble out[4] = { INFINITY, -INFINITY, INFINITY,
    -INFINITY
  };                            /* nothing is clamped */
  gdouble *res;
  GstMapInfo map;

  volume = setup_volume ();
  g_object_set (G_OBJECT (volume), "volume-full-range", G_MAXDOUBLE, NULL);
  fail_unless (gst_element_set_state (volume, GST_STATE_PLAYING) ==
      GST_STATE_CHANGE_SUCCESS, "could not set to playing");

  inbuffer = gst_buffer_new_and_alloc (32);
  gst_buffer_fill (inbuffer, 0, in, 32);
  fail_unless (gst_buffer_memcmp (inbuffer, 0, in, 32) == 0);
  caps = gst_caps_from_string (VOLUME_CAPS_STRING_F64);
  gst_check_setup_events (mysrcpad, volume, caps, GST_FORMAT_TIME);
  gst_caps_unref (caps);
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  /* FIXME: reffing the inbuffer should make the transformation not be
   * inplace
   gst_buffer_ref (inbuffer);
   */

  /* pushing gives away my reference ... */
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);
  /* ... but it ends up being modified inplace and
   * collected on the global buffer list */
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  fail_unless_equals_int (g_list_length (buffers), 1);
  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);
  fail_unless (inbuffer == outbuffer);
  gst_buffer_map (outbuffer, &map, GST_MAP_READ);
  res = (gdouble *) map.data;
  GST_INFO
      ("expected %+1.4f %+1.4f %+1.4f %+1.4f  real %+1.4f %+1.4f %+1.4f %+1.4f",
      out[0], out[1], out[2], out[3], res[0], res[1], res[2], res[3]);
  fail_unless (memcmp (map.data, out, 32) == 0);
  gst_buffer_unmap (outbuffer, &map);

  /* cleanup */
  cleanup_volume (volume);
}

GST_END_TEST;

GST_START_TEST (test_mute_f64)
{
  GstElement *volume;
  GstBuffer *inbuffer;
  GstBuffer *outbuffer;
  GstCaps *caps;
  gdouble in[2] = { 0.75, -0.25 };
  gdouble out[2] = { 0, 0 };
  gdouble *res;
  GstMapInfo map;

  volume = setup_volume ();
  g_object_set (G_OBJECT (volume), "mute", TRUE, NULL);
  fail_unless (gst_element_set_state (volume,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  inbuffer = gst_buffer_new_and_alloc (16);
  gst_buffer_fill (inbuffer, 0, in, 16);
  caps = gst_caps_from_string (VOLUME_CAPS_STRING_F64);
  gst_check_setup_events (mysrcpad, volume, caps, GST_FORMAT_TIME);
  gst_caps_unref (caps);
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  /* FIXME: reffing the inbuffer should make the transformation not be
   * inplace
   gst_buffer_ref (inbuffer);
   */

  /* pushing gives away my reference ... */
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);
  /* ... but it ends up being modified inplace and
   * collected on the global buffer list */
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  fail_unless_equals_int (g_list_length (buffers), 1);
  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);
  fail_unless (inbuffer == outbuffer);
  gst_buffer_map (outbuffer, &map, GST_MAP_READ);
  res = (gdouble *) map.data;
  GST_INFO ("expected %+1.4f %+1.4f  real %+1.4f %+1.4f", out[0], out[1],
      res[0], res[1]);
  fail_unless_equals_float (res[0], out[0]);
  fail_unless_equals_float (res[1], out[1]);
  gst_buffer_unmap (outbuffer, &map);

  /* cleanup */
  cleanup_volume (volume);
}

GST_END_TEST;

GST_START_TEST (test_wrong_caps)
{
  GstElement *volume;
  GstBuffer *inbuffer;
  gint16 in[2] = { 16384, -256 };
  GstBus *bus;
  GstMessage *message;
  GstCaps *caps;

  volume = setup_volume ();
  bus = gst_bus_new ();

  fail_unless (gst_element_set_state (volume,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  inbuffer = gst_buffer_new_and_alloc (4);
  gst_buffer_fill (inbuffer, 0, in, 4);
  caps = gst_caps_from_string (VOLUME_WRONG_CAPS_STRING);
  gst_check_setup_events (mysrcpad, volume, caps, GST_FORMAT_TIME);
  gst_caps_unref (caps);
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  gst_buffer_ref (inbuffer);

  /* set a bus here so we avoid getting state change messages */
  gst_element_set_bus (volume, bus);

  /* pushing gives an error because it can't negotiate with wrong caps */
  fail_unless_equals_int (gst_pad_push (mysrcpad, inbuffer),
      GST_FLOW_NOT_NEGOTIATED);
  /* ... and the buffer would have been lost if we didn't ref it ourselves */
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  gst_buffer_unref (inbuffer);
  fail_unless_equals_int (g_list_length (buffers), 0);

  /* volume_set_caps should not have been called since basetransform caught
   * the negotiation problem */
  fail_if ((message = gst_bus_pop (bus)) != NULL);

  /* cleanup */
  gst_element_set_bus (volume, NULL);
  gst_object_unref (GST_OBJECT (bus));
  cleanup_volume (volume);
}

GST_END_TEST;

GST_START_TEST (test_passthrough)
{
  GstElement *volume;
  GstBuffer *inbuffer, *outbuffer;
  GstCaps *caps;
  gint16 *out, in[2] = { 16384, -256 };
  GstMapInfo map;

  volume = setup_volume ();
  g_object_set (G_OBJECT (volume), "volume", 1.0, NULL);
  fail_unless (gst_element_set_state (volume,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  inbuffer = gst_buffer_new_and_alloc (4);
  gst_buffer_fill (inbuffer, 0, in, 4);
  caps = gst_caps_from_string (VOLUME_CAPS_STRING_S16);
  gst_check_setup_events (mysrcpad, volume, caps, GST_FORMAT_TIME);
  gst_caps_unref (caps);
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);

  /* pushing gives away my reference ... */
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);
  /* ... but it ends up being collected on the global buffer list */
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  fail_unless_equals_int (g_list_length (buffers), 1);
  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);
  fail_unless (inbuffer == outbuffer);
  gst_buffer_map (outbuffer, &map, GST_MAP_READ);
  out = (gint16 *) map.data;
  GST_INFO ("expected %+5d %+5d  real %+5d %+5d", in[0], in[1], out[0], out[1]);
  fail_unless (memcmp (map.data, in, 4) == 0);
  gst_buffer_unmap (outbuffer, &map);

  /* cleanup */
  cleanup_volume (volume);
}

GST_END_TEST;

GST_START_TEST (test_controller_usability)
{
  GstControlSource *cs;
  GstTimedValueControlSource *tvcs;
  GstControlBinding *cb;
  GstElement *volume;

  volume = setup_volume ();

  /* this shouldn't crash, whether this mode is implemented or not */
  cs = gst_interpolation_control_source_new ();
  g_object_set (cs, "mode", GST_INTERPOLATION_MODE_CUBIC, NULL);
  cb = gst_direct_control_binding_new (GST_OBJECT_CAST (volume), "volume", cs);
  gst_object_add_control_binding (GST_OBJECT_CAST (volume), cb);

  tvcs = (GstTimedValueControlSource *) cs;
  gst_timed_value_control_source_set (tvcs, 0 * GST_SECOND, 0.0);
  gst_timed_value_control_source_set (tvcs, 5 * GST_SECOND, 1.0);
  gst_timed_value_control_source_set (tvcs, 10 * GST_SECOND, 0.0);

  gst_object_unref (cs);
  gst_object_remove_control_binding (GST_OBJECT_CAST (volume), cb);

  cleanup_volume (volume);
}

GST_END_TEST;

GST_START_TEST (test_controller_processing)
{
  GstControlSource *cs;
  GstTimedValueControlSource *tvcs;
  GstElement *volume;
  GstBuffer *inbuffer, *outbuffer;
  GstCaps *caps;
  gint16 *out, in[2] = { +32767, -32768 };
  GstMapInfo map;
  GstSegment seg;

  volume = setup_volume ();

  cs = gst_interpolation_control_source_new ();
  g_object_set (cs, "mode", GST_INTERPOLATION_MODE_CUBIC, NULL);
  gst_object_add_control_binding (GST_OBJECT_CAST (volume),
      gst_direct_control_binding_new (GST_OBJECT_CAST (volume), "volume", cs));

  /* the value range for volume is 0.0 ... 10.0 */
  tvcs = (GstTimedValueControlSource *) cs;
  gst_timed_value_control_source_set (tvcs, 0 * GST_SECOND, 0.1);

  fail_unless (gst_element_set_state (volume,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  inbuffer = gst_buffer_new_and_alloc (4);
  gst_buffer_fill (inbuffer, 0, in, 4);
  caps = gst_caps_from_string (VOLUME_CAPS_STRING_S16);
  gst_check_setup_events (mysrcpad, volume, caps, GST_FORMAT_TIME);
  GST_BUFFER_TIMESTAMP (inbuffer) = 0;
  gst_caps_unref (caps);
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);

  gst_segment_init (&seg, GST_FORMAT_TIME);
  fail_unless (gst_pad_push_event (mysrcpad,
          gst_event_new_segment (&seg)) == TRUE);

  /* pushing gives away my reference ... */
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);
  /* ... but it ends up being collected on the global buffer list */
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  fail_unless_equals_int (g_list_length (buffers), 1);
  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);
  fail_unless (inbuffer == outbuffer);
  gst_buffer_map (outbuffer, &map, GST_MAP_READ);
  out = (gint16 *) map.data;
  GST_INFO ("expected %+5d %+5d  real %+5d %+5d", in[0], in[1], out[0], out[1]);
  fail_unless (memcmp (map.data, in, 4) == 0);
  gst_buffer_unmap (outbuffer, &map);

  gst_object_unref (cs);
  cleanup_volume (volume);
}

GST_END_TEST;

GST_START_TEST (test_controller_defaults_at_ts0)
{
  GstControlSource *cs;
  GstTimedValueControlSource *tvcs;
  GstElement *volume;
  GstBuffer *inbuffer;
  GstCaps *caps;
  GstSegment seg;

  volume = setup_volume ();

  cs = gst_interpolation_control_source_new ();
  g_object_set (cs, "mode", GST_INTERPOLATION_MODE_LINEAR, NULL);
  gst_object_add_control_binding (GST_OBJECT_CAST (volume),
      gst_direct_control_binding_new (GST_OBJECT_CAST (volume), "volume", cs));

  /* make a control curve that does not start at ts=0, the element will use
   * the current property value (default) until the control curve starts
   */
  tvcs = (GstTimedValueControlSource *) cs;
  gst_timed_value_control_source_set (tvcs, GST_SECOND / 100, 0.1);
  gst_timed_value_control_source_set (tvcs, GST_SECOND, 1.0);

  fail_unless (gst_element_set_state (volume,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  /* controller curve starts at sample: 441 */
  inbuffer = gst_buffer_new_and_alloc (1000 * sizeof (gint16));
  gst_buffer_memset (inbuffer, 0, 0, 1000 * sizeof (gint16));
  caps = gst_caps_from_string (VOLUME_CAPS_STRING_S16);
  gst_check_setup_events (mysrcpad, volume, caps, GST_FORMAT_TIME);
  GST_BUFFER_TIMESTAMP (inbuffer) = 0;
  gst_caps_unref (caps);

  gst_segment_init (&seg, GST_FORMAT_TIME);
  fail_unless (gst_pad_push_event (mysrcpad,
          gst_event_new_segment (&seg)) == TRUE);

  /* pushing gives away my reference ... */
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);

  gst_object_unref (cs);
  cleanup_volume (volume);
}

GST_END_TEST;

static Suite *
volume_suite (void)
{
  Suite *s = suite_create ("volume");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_get_set);
  tcase_add_test (tc_chain, test_unity_s8);
  tcase_add_test (tc_chain, test_half_s8);
  tcase_add_test (tc_chain, test_double_s8);
  tcase_add_test (tc_chain, test_ten_s8);
  tcase_add_test (tc_chain, test_fifteen_s8);
  tcase_add_test (tc_chain, test_sixteen_s8);
  tcase_add_test (tc_chain, test_max_s8);
  tcase_add_test (tc_chain, test_mute_s8);
  tcase_add_test (tc_chain, test_unity_s16);
  tcase_add_test (tc_chain, test_half_s16);
  tcase_add_test (tc_chain, test_double_s16);
  tcase_add_test (tc_chain, test_ten_s16);
  tcase_add_test (tc_chain, test_fifteen_s16);
  tcase_add_test (tc_chain, test_sixteen_s16);
  tcase_add_test (tc_chain, test_max_s16);
  tcase_add_test (tc_chain, test_mute_s16);
  tcase_add_test (tc_chain, test_unity_s24);
  tcase_add_test (tc_chain, test_half_s24);
  tcase_add_test (tc_chain, test_double_s24);
  tcase_add_test (tc_chain, test_ten_s24);
  tcase_add_test (tc_chain, test_fifteen_s24);
  tcase_add_test (tc_chain, test_sixteen_s24);
  tcase_add_test (tc_chain, test_4095_s24);
  tcase_add_test (tc_chain, test_4096_s24);
  tcase_add_test (tc_chain, test_max_s24);
  tcase_add_test (tc_chain, test_mute_s24);
  tcase_add_test (tc_chain, test_unity_s32);
  tcase_add_test (tc_chain, test_half_s32);
  tcase_add_test (tc_chain, test_double_s32);
  tcase_add_test (tc_chain, test_ten_s32);
  tcase_add_test (tc_chain, test_fifteen_s32);
  tcase_add_test (tc_chain, test_sixteen_s32);
  tcase_add_test (tc_chain, test_max_s32);
  tcase_add_test (tc_chain, test_mute_s32);
  tcase_add_test (tc_chain, test_unity_f32);
  tcase_add_test (tc_chain, test_half_f32);
  tcase_add_test (tc_chain, test_double_f32);
  tcase_add_test (tc_chain, test_ten_f32);
  tcase_add_test (tc_chain, test_fifteen_f32);
  tcase_add_test (tc_chain, test_sixteen_f32);
  tcase_add_test (tc_chain, test_max_f32);
  tcase_add_test (tc_chain, test_mute_f32);
  tcase_add_test (tc_chain, test_unity_f64);
  tcase_add_test (tc_chain, test_half_f64);
  tcase_add_test (tc_chain, test_double_f64);
  tcase_add_test (tc_chain, test_ten_f64);
  tcase_add_test (tc_chain, test_fifteen_f64);
  tcase_add_test (tc_chain, test_sixteen_f64);
  tcase_add_test (tc_chain, test_max_f64);
  tcase_add_test (tc_chain, test_mute_f64);
  tcase_add_test (tc_chain, test_wrong_caps);
  tcase_add_test (tc_chain, test_passthrough);
  tcase_add_test (tc_chain, test_controller_usability);
  tcase_add_test (tc_chain, test_controller_processing);
  tcase_add_test (tc_chain, test_controller_defaults_at_ts0);

  return s;
}

GST_CHECK_MAIN (volume)
