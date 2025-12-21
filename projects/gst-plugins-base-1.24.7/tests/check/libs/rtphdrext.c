/* GStreamer RTP header extension unit tests
 * Copyright (C) 2020 Matthew Waters <matthew@centricular.com>
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
 * You should have received a copy of the GNU Library General
 * Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */
#include "gst/gstcaps.h"
#include "gst/gstvalue.h"
#include "gst/rtp/gstrtphdrext.h"
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/check/check.h>
#include <gst/rtp/rtp.h>

#include "rtpdummyhdrextimpl.c"

GST_START_TEST (rtp_header_ext_write)
{
  GstRTPHeaderExtension *dummy;
  GstBuffer *buffer;
  guint8 *data;
  gsize size;
  gssize written;

  dummy = rtp_dummy_hdr_ext_new ();
  gst_rtp_header_extension_set_id (dummy, 1);

  buffer = gst_buffer_new ();
  size = gst_rtp_header_extension_get_max_size (dummy, buffer);
  fail_unless (size > 0);

  data = g_malloc0 (size);
  fail_unless (data != NULL);

  written =
      gst_rtp_header_extension_write (dummy, buffer, 0, buffer, data, size);
  fail_unless (written > 0 && written <= size);
  fail_unless_equals_int (GST_RTP_DUMMY_HDR_EXT (dummy)->write_count, 1);

  fail_unless (gst_rtp_header_extension_read (dummy, 0, data, size, buffer));
  fail_unless_equals_int (GST_RTP_DUMMY_HDR_EXT (dummy)->read_count, 1);

  g_free (data);
  gst_buffer_unref (buffer);
  g_object_unref (dummy);
}

GST_END_TEST;

GST_START_TEST (rtp_header_ext_create_from_uri)
{
  GstElementFactory *factory;
  GstRTPHeaderExtension *dummy;

  fail_unless (gst_element_register (NULL, "test-dummyrtphdrext",
          GST_RANK_MARGINAL, GST_TYPE_RTP_DUMMY_HDR_EXT));

  dummy = gst_rtp_header_extension_create_from_uri (DUMMY_HDR_EXT_URI);
  fail_unless (GST_IS_RTP_DUMMY_HDR_EXT (dummy));

  factory = gst_element_get_factory (GST_ELEMENT (dummy));
  gst_registry_remove_feature (gst_registry_get (),
      GST_PLUGIN_FEATURE (factory));
  gst_object_unref (dummy);
}

GST_END_TEST;

GST_START_TEST (rtp_header_ext_caps_with_attributes)
{
  GstRTPHeaderExtension *dummy;
  GstCaps *caps = gst_caps_new_empty_simple ("application/x-rtp");
  GstStructure *s = gst_caps_get_structure (caps, 0);
  const GValue *arr, *val;
  const gchar *attributes = "attr0 attr1";
  const gchar *direction = "recvonly";

  dummy = rtp_dummy_hdr_ext_new ();

  gst_rtp_header_extension_set_id (dummy, 1);

  gst_rtp_header_extension_set_direction (dummy,
      GST_RTP_HEADER_EXTENSION_DIRECTION_RECVONLY);
  GST_RTP_DUMMY_HDR_EXT (dummy)->attributes = g_strdup (attributes);

  fail_unless (gst_rtp_header_extension_set_caps_from_attributes (dummy, caps));
  fail_unless (gst_structure_has_field_typed (s, "extmap-1", GST_TYPE_ARRAY));
  arr = gst_structure_get_value (s, "extmap-1");
  fail_unless (GST_VALUE_HOLDS_ARRAY (arr));
  fail_unless_equals_int (gst_value_array_get_size (arr), 3);
  val = gst_value_array_get_value (arr, 0);
  fail_unless_equals_string (g_value_get_string (val), direction);
  val = gst_value_array_get_value (arr, 1);
  fail_unless_equals_string (g_value_get_string (val),
      gst_rtp_header_extension_get_uri (dummy));
  val = gst_value_array_get_value (arr, 2);
  fail_unless_equals_string (g_value_get_string (val), attributes);

  gst_rtp_header_extension_set_direction (dummy,
      GST_RTP_HEADER_EXTENSION_DIRECTION_SENDRECV |
      GST_RTP_HEADER_EXTENSION_DIRECTION_INHERITED);
  g_free (GST_RTP_DUMMY_HDR_EXT (dummy)->attributes);
  GST_RTP_DUMMY_HDR_EXT (dummy)->attributes = NULL;

  fail_unless (gst_rtp_header_extension_set_attributes_from_caps (dummy, caps));

  fail_unless_equals_string (GST_RTP_DUMMY_HDR_EXT (dummy)->attributes,
      attributes);
  fail_unless_equals_int (gst_rtp_header_extension_get_direction (dummy),
      GST_RTP_HEADER_EXTENSION_DIRECTION_RECVONLY);

  gst_caps_unref (caps);
  gst_object_unref (dummy);
}

GST_END_TEST;

static Suite *
rtp_header_extension_suite (void)
{
  Suite *s = suite_create ("rtp_header_extension_test");
  TCase *tc_chain = tcase_create ("header extension test");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, rtp_header_ext_write);
  tcase_add_test (tc_chain, rtp_header_ext_create_from_uri);
  tcase_add_test (tc_chain, rtp_header_ext_caps_with_attributes);

  return s;
}

GST_CHECK_MAIN (rtp_header_extension)
