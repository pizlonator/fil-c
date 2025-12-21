/* GStreamer unit tests for the MIKEY support library
 *
 * Copyright (C) 2014 Wim Taymans <wim.taymans@gmail.com>
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/check/gstcheck.h>

#include <gst/sdp/gstmikey.h>

GST_START_TEST (create_common)
{
  GstMIKEYMessage *msg;
  const guint8 test_data[] =
      { 0x01, 0x00, 0x00, 0x00, 0x12, 0x34, 0x56, 0x78, 0x00, 0x00 };
  const guint8 test_data2[] =
      { 0x01, 0x12, 0x34, 0x56, 0x78, 0x00, 0x00, 0x00, 0x00,
    0x02, 0x23, 0x45, 0x67, 0x89, 0x00, 0x00, 0x00, 0x01
  };
  GBytes *bytes;
  const guint8 *data;
  gsize size;
  const GstMIKEYMapSRTP *mi;
  GstMIKEYMapSRTP srtp;

  msg = gst_mikey_message_new ();
  fail_unless (msg != NULL);

  fail_unless (gst_mikey_message_set_info (msg, 1, GST_MIKEY_TYPE_PSK_INIT,
          FALSE, GST_MIKEY_PRF_MIKEY_1, 0x12345678, GST_MIKEY_MAP_TYPE_SRTP));
  fail_unless (gst_mikey_message_get_n_cs (msg) == 0);

  fail_unless (msg->version == 1);
  fail_unless (msg->type == GST_MIKEY_TYPE_PSK_INIT);
  fail_unless (msg->V == FALSE);
  fail_unless (msg->prf_func == GST_MIKEY_PRF_MIKEY_1);
  fail_unless (msg->CSB_id == 0x12345678);
  fail_unless (msg->map_type == GST_MIKEY_MAP_TYPE_SRTP);

  bytes = gst_mikey_message_to_bytes (msg, NULL, NULL);
  data = g_bytes_get_data (bytes, &size);
  fail_unless (data != NULL);
  fail_unless (size == 10);
  fail_unless (memcmp (data, test_data, 10) == 0);
  g_bytes_unref (bytes);

  fail_unless (gst_mikey_message_add_cs_srtp (msg, 1, 0x12345678, 0));
  fail_unless (gst_mikey_message_get_n_cs (msg) == 1);
  fail_unless (gst_mikey_message_add_cs_srtp (msg, 2, 0x23456789, 1));
  fail_unless (gst_mikey_message_get_n_cs (msg) == 2);

  bytes = gst_mikey_message_to_bytes (msg, NULL, NULL);
  data = g_bytes_get_data (bytes, &size);
  fail_unless (size == 28);
  fail_unless (memcmp (data + 10, test_data2, 18) == 0);
  g_bytes_unref (bytes);

  fail_unless ((mi = gst_mikey_message_get_cs_srtp (msg, 0)) != NULL);
  fail_unless (mi->policy == 1);
  fail_unless (mi->ssrc == 0x12345678);
  fail_unless (mi->roc == 0);
  fail_unless ((mi = gst_mikey_message_get_cs_srtp (msg, 1)) != NULL);
  fail_unless (mi->policy == 2);
  fail_unless (mi->ssrc == 0x23456789);
  fail_unless (mi->roc == 1);

  fail_unless (gst_mikey_message_remove_cs_srtp (msg, 0));
  fail_unless (gst_mikey_message_get_n_cs (msg) == 1);
  fail_unless ((mi = gst_mikey_message_get_cs_srtp (msg, 0)) != NULL);
  fail_unless (mi->policy == 2);
  fail_unless (mi->ssrc == 0x23456789);
  fail_unless (mi->roc == 1);
  srtp.policy = 1;
  srtp.ssrc = 0x12345678;
  srtp.roc = 0;
  fail_unless (gst_mikey_message_insert_cs_srtp (msg, 0, &srtp));
  fail_unless ((mi = gst_mikey_message_get_cs_srtp (msg, 0)) != NULL);
  fail_unless (mi->policy == 1);
  fail_unless (mi->ssrc == 0x12345678);
  fail_unless (mi->roc == 0);
  fail_unless ((mi = gst_mikey_message_get_cs_srtp (msg, 1)) != NULL);
  fail_unless (mi->policy == 2);
  fail_unless (mi->ssrc == 0x23456789);
  fail_unless (mi->roc == 1);

  fail_unless (gst_mikey_message_remove_cs_srtp (msg, 1));
  fail_unless (gst_mikey_message_get_n_cs (msg) == 1);
  fail_unless (gst_mikey_message_remove_cs_srtp (msg, 0));
  fail_unless (gst_mikey_message_get_n_cs (msg) == 0);

  gst_mikey_message_unref (msg);
}

GST_END_TEST
GST_START_TEST (create_payloads)
{
  GstMIKEYMessage *msg;
  GstMIKEYPayload *payload, *kp;
  const GstMIKEYPayload *cp, *cp2;
  const GstMIKEYPayloadKEMAC *p;
  const GstMIKEYPayloadT *pt;
  const GstMIKEYPayloadKeyData *pkd;
  const guint8 ntp_data[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 };
  const guint8 edata[] = { 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80,
    0x90, 0xa0, 0xb0, 0xc0, 0xd0, 0xe0, 0xf0, 0x10
  };
  GBytes *bytes;
  const guint8 *data;
  gsize size;

  msg = gst_mikey_message_new ();
  fail_unless (msg != NULL);

  fail_unless (gst_mikey_message_set_info (msg, 1, GST_MIKEY_TYPE_PSK_INIT,
          FALSE, GST_MIKEY_PRF_MIKEY_1, 0x12345678, GST_MIKEY_MAP_TYPE_SRTP));
  fail_unless (gst_mikey_message_get_n_cs (msg) == 0);

  fail_unless (gst_mikey_message_get_n_payloads (msg) == 0);

  payload = gst_mikey_payload_new (GST_MIKEY_PT_T);
  fail_unless (payload->type == GST_MIKEY_PT_T);
  fail_unless (payload->len == sizeof (GstMIKEYPayloadT));
  fail_unless (gst_mikey_payload_t_set (payload, GST_MIKEY_TS_TYPE_NTP,
          ntp_data));
  pt = (GstMIKEYPayloadT *) payload;
  fail_unless (pt->type == GST_MIKEY_TS_TYPE_NTP);
  fail_unless (memcmp (pt->ts_value, ntp_data, 8) == 0);

  fail_unless (gst_mikey_message_add_payload (msg, payload));
  fail_unless (payload->type == GST_MIKEY_PT_T);
  fail_unless (gst_mikey_message_get_n_payloads (msg) == 1);

  bytes = gst_mikey_message_to_bytes (msg, NULL, NULL);
  data = g_bytes_get_data (bytes, &size);
  fail_unless (data != NULL);
  fail_unless (size == 20);
  g_bytes_unref (bytes);

  payload = gst_mikey_payload_new (GST_MIKEY_PT_KEMAC);
  fail_unless (gst_mikey_payload_kemac_set (payload, GST_MIKEY_ENC_NULL,
          GST_MIKEY_MAC_NULL));
  /* add the edata as a key payload */
  kp = gst_mikey_payload_new (GST_MIKEY_PT_KEY_DATA);
  gst_mikey_payload_key_data_set_key (kp, GST_MIKEY_KD_TEK,
      sizeof (edata), edata);
  fail_unless (gst_mikey_payload_kemac_add_sub (payload, kp));
  fail_unless (gst_mikey_message_add_payload (msg, payload));
  fail_unless (gst_mikey_message_get_n_payloads (msg) == 2);

  p = (GstMIKEYPayloadKEMAC *) gst_mikey_message_get_payload (msg, 1);
  fail_unless (p->enc_alg == GST_MIKEY_ENC_NULL);
  fail_unless (p->mac_alg == GST_MIKEY_MAC_NULL);
  fail_unless (gst_mikey_payload_kemac_get_n_sub (&p->pt) == 1);

  fail_unless ((cp = gst_mikey_message_get_payload (msg, 0)) != NULL);
  fail_unless (cp->type == GST_MIKEY_PT_T);
  fail_unless ((cp = gst_mikey_message_get_payload (msg, 1)) != NULL);
  fail_unless (cp->type == GST_MIKEY_PT_KEMAC);

  bytes = gst_mikey_message_to_bytes (msg, NULL, NULL);
  gst_mikey_message_unref (msg);

  msg = gst_mikey_message_new_from_bytes (bytes, NULL, NULL);
  fail_unless (msg != NULL);
  g_bytes_unref (bytes);
  fail_unless (gst_mikey_message_get_n_payloads (msg) == 2);
  fail_unless ((cp = gst_mikey_message_get_payload (msg, 0)) != NULL);
  fail_unless (cp->type == GST_MIKEY_PT_T);
  fail_unless ((cp = gst_mikey_message_get_payload (msg, 1)) != NULL);
  fail_unless (cp->type == GST_MIKEY_PT_KEMAC);

  fail_unless ((cp2 = gst_mikey_payload_kemac_get_sub (cp, 0)) != NULL);
  fail_unless (cp2->type == GST_MIKEY_PT_KEY_DATA);
  pkd = (GstMIKEYPayloadKeyData *) cp2;

  fail_unless (pkd->key_type == GST_MIKEY_KD_TEK);
  fail_unless (pkd->key_len == sizeof (edata));
  fail_unless (memcmp (pkd->key_data, edata, sizeof (edata)) == 0);
  fail_unless (pkd->salt_len == 0);
  fail_unless (pkd->salt_data == 0);
  fail_unless (pkd->kv_type == GST_MIKEY_KV_NULL);


  gst_mikey_message_unref (msg);
}

GST_END_TEST
/*
 * End of test cases
 */
static Suite *
mikey_suite (void)
{
  Suite *s = suite_create ("mikey");
  TCase *tc_chain = tcase_create ("mikey");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, create_common);
  tcase_add_test (tc_chain, create_payloads);

  return s;
}

GST_CHECK_MAIN (mikey);
