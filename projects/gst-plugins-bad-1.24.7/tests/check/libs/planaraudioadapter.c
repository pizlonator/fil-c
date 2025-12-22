/* GStreamer
 * Copyright (C) 2018 Collabora Ltd.
 *   @author George Kiagiadakis <george.kiagiadakis@collabora.com>
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
#include <gst/audio/gstplanaraudioadapter.h>

static GstBuffer *
generate_buffer (GstAudioInfo * info, gsize nsamples,
    gsize dummy_start, gsize dummy_end, gpointer * data_ret)
{
  gpointer data;
  GstBuffer *buf;
  gsize buf_sz;
  gsize offsets[8];
  gint c, bps;

  fail_unless (info->channels <= 8);

  bps = info->finfo->width / 8;
  buf_sz = info->channels * (nsamples + dummy_start + dummy_end) * bps;
  data = g_malloc (buf_sz);
  fail_unless (data);
  buf = gst_buffer_new_wrapped (data, buf_sz);
  fail_unless (buf);

  for (c = 0; c < info->channels; c++) {
    offsets[c] =
        dummy_start * bps + c * (nsamples + dummy_start + dummy_end) * bps;

    /* dummy samples at the beginning of each channel plane */
    gst_buffer_memset (buf, offsets[c] - dummy_start * bps, 0xBF,
        dummy_start * bps);
    /* valid channel samples */
    gst_buffer_memset (buf, offsets[c], c | 0xF0, nsamples * bps);
    /* dummy samples at the end of each channel plane */
    gst_buffer_memset (buf, offsets[c] + nsamples * bps, 0xEF, dummy_end * bps);
  }
  gst_buffer_add_audio_meta (buf, info, nsamples, offsets);

  if (data_ret)
    *data_ret = data;
  return buf;
}

static void
verify_buffer_contents (GstBuffer * buf, GstAudioInfo * info,
    gint expect_n_planes, gsize expect_plane_size,
    gpointer base, gsize real_plane_size, gsize expect_plane_start_offset)
{
  GstAudioBuffer abuf;
  gint i;
  guint8 *byte;

  gst_audio_buffer_map (&abuf, info, buf, GST_MAP_READ);
  fail_unless_equals_int (GST_AUDIO_BUFFER_N_PLANES (&abuf), expect_n_planes);
  fail_unless_equals_int (GST_AUDIO_BUFFER_PLANE_SIZE (&abuf),
      expect_plane_size);

  for (i = 0; i < GST_AUDIO_BUFFER_N_PLANES (&abuf); i++) {
    if (base) {
      /* if we have a base pointer, verify the plane pointer
       * points to the right place */
      fail_unless_equals_pointer (abuf.planes[i],
          ((guint8 *) base) + i * real_plane_size + expect_plane_start_offset);
    }

    /* verify all contents */
    byte = abuf.planes[i];
    while (byte < ((guint8 *) abuf.planes[i]) + expect_plane_size) {
      GST_TRACE ("%d | %p", i, byte);
      fail_unless_equals_int_hex (*byte, i | 0xF0);
      ++byte;
    }
  }
  gst_audio_buffer_unmap (&abuf);
}

GST_START_TEST (test_retrieve_same)
{
  GstPlanarAudioAdapter *adapter;
  GstAudioInfo info;
  GstBuffer *buf;

  adapter = gst_planar_audio_adapter_new ();

  gst_audio_info_init (&info);
  gst_audio_info_set_format (&info, GST_AUDIO_FORMAT_S32, 100, 5, NULL);
  info.layout = GST_AUDIO_LAYOUT_NON_INTERLEAVED;

  gst_planar_audio_adapter_configure (adapter, &info);
  buf = generate_buffer (&info, 20, 0, 0, NULL);
  gst_planar_audio_adapter_push (adapter, buf);
  fail_unless_equals_int (gst_planar_audio_adapter_available (adapter), 20);

  buf = generate_buffer (&info, 20, 10, 5, NULL);
  gst_planar_audio_adapter_push (adapter, buf);
  fail_unless_equals_int (gst_planar_audio_adapter_available (adapter), 40);

  buf = gst_planar_audio_adapter_get_buffer (adapter, 20, GST_MAP_READ);
  fail_unless (buf);
  /* this buffer is shared between the adapter and us, we just ref'ed it */
  fail_unless_equals_int (GST_MINI_OBJECT_REFCOUNT_VALUE (buf), 2);
  /* the adapter still has 40 samples */
  fail_unless_equals_int (gst_planar_audio_adapter_available (adapter), 40);
  gst_planar_audio_adapter_flush (adapter, 20);
  /* the adapter must have dropped this buffer internally */
  fail_unless_equals_int (GST_MINI_OBJECT_REFCOUNT_VALUE (buf), 1);
  fail_unless_equals_int (gst_planar_audio_adapter_available (adapter), 20);
  gst_buffer_unref (buf);

  buf = gst_planar_audio_adapter_take_buffer (adapter, 20, GST_MAP_READ);
  fail_unless (buf);
  fail_unless_equals_int (GST_MINI_OBJECT_REFCOUNT_VALUE (buf), 1);
  fail_unless_equals_int (gst_planar_audio_adapter_available (adapter), 0);
  gst_buffer_unref (buf);

  g_object_unref (adapter);
}

GST_END_TEST;

GST_START_TEST (test_retrieve_smaller_for_read)
{
  GstPlanarAudioAdapter *adapter;
  GstAudioInfo info;
  GstBuffer *buf;
  gpointer data1, data2;

  adapter = gst_planar_audio_adapter_new ();

  gst_audio_info_init (&info);
  gst_audio_info_set_format (&info, GST_AUDIO_FORMAT_S16, 100, 8, NULL);
  info.layout = GST_AUDIO_LAYOUT_NON_INTERLEAVED;

  gst_planar_audio_adapter_configure (adapter, &info);
  buf = generate_buffer (&info, 40, 0, 0, &data1);
  gst_planar_audio_adapter_push (adapter, buf);
  fail_unless_equals_int (gst_planar_audio_adapter_available (adapter), 40);

  buf = generate_buffer (&info, 20, 10, 10, &data2);
  gst_planar_audio_adapter_push (adapter, buf);
  fail_unless_equals_int (gst_planar_audio_adapter_available (adapter), 60);

  /* the the first 20 samples */

  buf = gst_planar_audio_adapter_take_buffer (adapter, 20, GST_MAP_READ);
  fail_unless (buf);
  fail_unless_equals_int (GST_MINI_OBJECT_REFCOUNT_VALUE (buf), 1);
  fail_unless_equals_int (gst_planar_audio_adapter_available (adapter), 40);
  verify_buffer_contents (buf, &info, 8, 20 * sizeof (gint16),
      data1, 40 * sizeof (gint16), 0);
  gst_buffer_unref (buf);

  /* now the next 20 samples */

  buf = gst_planar_audio_adapter_take_buffer (adapter, 20, GST_MAP_READ);
  fail_unless (buf);
  fail_unless_equals_int (GST_MINI_OBJECT_REFCOUNT_VALUE (buf), 1);
  fail_unless_equals_int (gst_planar_audio_adapter_available (adapter), 20);
  /* still the same memory, with a 20 sample offset on each plane */
  verify_buffer_contents (buf, &info, 8, 20 * sizeof (gint16),
      data1, 40 * sizeof (gint16), 20 * sizeof (gint16));
  gst_buffer_unref (buf);

  /* 5 samples from the second buffer */

  buf = gst_planar_audio_adapter_take_buffer (adapter, 5, GST_MAP_READ);
  fail_unless (buf);
  fail_unless_equals_int (GST_MINI_OBJECT_REFCOUNT_VALUE (buf), 1);
  fail_unless_equals_int (gst_planar_audio_adapter_available (adapter), 15);
  /* original buffer had an offset of 10 samples on its own and
   * was 40 samples long, with only 20 samples valid */
  verify_buffer_contents (buf, &info, 8, 5 * sizeof (gint16),
      data2, 40 * sizeof (gint16), 10 * sizeof (gint16));
  gst_buffer_unref (buf);

  /* and the last 15 samples */

  buf = gst_planar_audio_adapter_take_buffer (adapter, 15, GST_MAP_READ);
  fail_unless (buf);
  fail_unless_equals_int (GST_MINI_OBJECT_REFCOUNT_VALUE (buf), 1);
  fail_unless_equals_int (gst_planar_audio_adapter_available (adapter), 0);
  verify_buffer_contents (buf, &info, 8, 15 * sizeof (gint16),
      data2, 40 * sizeof (gint16), 15 * sizeof (gint16));
  gst_buffer_unref (buf);

  g_object_unref (adapter);
}

GST_END_TEST;

GST_START_TEST (test_retrieve_smaller_for_write)
{
  GstPlanarAudioAdapter *adapter;
  GstAudioInfo info;
  GstBuffer *buf;

  adapter = gst_planar_audio_adapter_new ();

  gst_audio_info_init (&info);
  gst_audio_info_set_format (&info, GST_AUDIO_FORMAT_S16, 100, 8, NULL);
  info.layout = GST_AUDIO_LAYOUT_NON_INTERLEAVED;

  gst_planar_audio_adapter_configure (adapter, &info);
  buf = generate_buffer (&info, 40, 0, 0, NULL);
  gst_planar_audio_adapter_push (adapter, buf);
  fail_unless_equals_int (gst_planar_audio_adapter_available (adapter), 40);

  buf = generate_buffer (&info, 20, 10, 10, NULL);
  gst_planar_audio_adapter_push (adapter, buf);
  fail_unless_equals_int (gst_planar_audio_adapter_available (adapter), 60);

  /* the the first 20 samples */

  buf = gst_planar_audio_adapter_take_buffer (adapter, 20, GST_MAP_WRITE);
  fail_unless (buf);
  fail_unless_equals_int (GST_MINI_OBJECT_REFCOUNT_VALUE (buf), 1);
  fail_unless_equals_int (gst_planar_audio_adapter_available (adapter), 40);
  verify_buffer_contents (buf, &info, 8, 20 * sizeof (gint16), NULL, 0, 0);
  gst_buffer_unref (buf);

  /* now the next 20 samples */

  buf = gst_planar_audio_adapter_take_buffer (adapter, 20, GST_MAP_WRITE);
  fail_unless (buf);
  fail_unless_equals_int (GST_MINI_OBJECT_REFCOUNT_VALUE (buf), 1);
  fail_unless_equals_int (gst_planar_audio_adapter_available (adapter), 20);
  verify_buffer_contents (buf, &info, 8, 20 * sizeof (gint16), NULL, 0, 0);
  gst_buffer_unref (buf);

  /* 5 samples from the second buffer */

  buf = gst_planar_audio_adapter_take_buffer (adapter, 5, GST_MAP_WRITE);
  fail_unless (buf);
  fail_unless_equals_int (GST_MINI_OBJECT_REFCOUNT_VALUE (buf), 1);
  fail_unless_equals_int (gst_planar_audio_adapter_available (adapter), 15);
  verify_buffer_contents (buf, &info, 8, 5 * sizeof (gint16), NULL, 0, 0);
  gst_buffer_unref (buf);

  /* and the last 15 samples */

  buf = gst_planar_audio_adapter_take_buffer (adapter, 15, GST_MAP_WRITE);
  fail_unless (buf);
  fail_unless_equals_int (GST_MINI_OBJECT_REFCOUNT_VALUE (buf), 1);
  fail_unless_equals_int (gst_planar_audio_adapter_available (adapter), 0);
  verify_buffer_contents (buf, &info, 8, 15 * sizeof (gint16), NULL, 0, 0);
  gst_buffer_unref (buf);

  g_object_unref (adapter);
}

GST_END_TEST;

GST_START_TEST (test_retrieve_combined)
{
  GstPlanarAudioAdapter *adapter;
  GstAudioInfo info;
  GstBuffer *buf;
  gpointer data2;

  adapter = gst_planar_audio_adapter_new ();

  gst_audio_info_init (&info);
  gst_audio_info_set_format (&info, GST_AUDIO_FORMAT_U24_32, 100, 4, NULL);
  info.layout = GST_AUDIO_LAYOUT_NON_INTERLEAVED;

  gst_planar_audio_adapter_configure (adapter, &info);
  buf = generate_buffer (&info, 20, 0, 0, NULL);
  gst_planar_audio_adapter_push (adapter, buf);
  fail_unless_equals_int (gst_planar_audio_adapter_available (adapter), 20);

  buf = generate_buffer (&info, 20, 10, 15, NULL);
  gst_planar_audio_adapter_push (adapter, buf);
  fail_unless_equals_int (gst_planar_audio_adapter_available (adapter), 40);

  buf = generate_buffer (&info, 80, 0, 5, &data2);
  gst_planar_audio_adapter_push (adapter, buf);
  fail_unless_equals_int (gst_planar_audio_adapter_available (adapter), 120);

  /* take the first 60 samples - buffers are combined here */

  buf = gst_planar_audio_adapter_take_buffer (adapter, 60, GST_MAP_READ);
  fail_unless (buf);
  fail_unless_equals_int (GST_MINI_OBJECT_REFCOUNT_VALUE (buf), 1);
  fail_unless_equals_int (gst_planar_audio_adapter_available (adapter), 60);
  verify_buffer_contents (buf, &info, 4, 60 * sizeof (gint32), NULL, 0, 0);
  gst_buffer_unref (buf);

  /* now the next 60 samples, for reading */

  buf = gst_planar_audio_adapter_get_buffer (adapter, 60, GST_MAP_READ);
  fail_unless (buf);
  fail_unless_equals_int (GST_MINI_OBJECT_REFCOUNT_VALUE (buf), 1);
  /* note we didn't take the buffer, the data is still in the adapter */
  fail_unless_equals_int (gst_planar_audio_adapter_available (adapter), 60);
  verify_buffer_contents (buf, &info, 4, 60 * sizeof (gint32),
      data2, 85 * sizeof (gint32), 20 * sizeof (gint32));
  gst_buffer_unref (buf);

  /* flush a few */

  gst_planar_audio_adapter_flush (adapter, 10);
  fail_unless_equals_int (gst_planar_audio_adapter_available (adapter), 50);

  /* add some more */

  buf = generate_buffer (&info, 20, 10, 0, NULL);
  gst_planar_audio_adapter_push (adapter, buf);
  fail_unless_equals_int (gst_planar_audio_adapter_available (adapter), 70);

  /* now take 60 again */

  buf = gst_planar_audio_adapter_take_buffer (adapter, 60, GST_MAP_READ);
  fail_unless (buf);
  fail_unless_equals_int (GST_MINI_OBJECT_REFCOUNT_VALUE (buf), 1);
  fail_unless_equals_int (gst_planar_audio_adapter_available (adapter), 10);
  verify_buffer_contents (buf, &info, 4, 60 * sizeof (gint32), NULL, 0, 0);
  gst_buffer_unref (buf);

  gst_planar_audio_adapter_clear (adapter);
  fail_unless_equals_int (gst_planar_audio_adapter_available (adapter), 0);

  g_object_unref (adapter);
}

GST_END_TEST;

static Suite *
planar_audio_adapter_suite (void)
{
  Suite *s = suite_create ("GstPlanarAudioAdapter");

  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_retrieve_same);
  tcase_add_test (tc_chain, test_retrieve_smaller_for_read);
  tcase_add_test (tc_chain, test_retrieve_smaller_for_write);
  tcase_add_test (tc_chain, test_retrieve_combined);

  return s;
}

GST_CHECK_MAIN (planar_audio_adapter);
