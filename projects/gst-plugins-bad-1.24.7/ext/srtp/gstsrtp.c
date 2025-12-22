/*
 * GStreamer - GStreamer SRTP encoder and decoder
 *
 * Copyright 2009-2013 Collabora Ltd.
 *  @author: Gabriel Millaire <gabriel.millaire@collabora.co.uk>
 *  @author: Olivier Crete <olivier.crete@collabora.com>
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


#define GLIB_DISABLE_DEPRECATION_WARNINGS

#include "gstsrtp.h"

#include <gst/rtp/gstrtcpbuffer.h>

#include "gstsrtpenc.h"
#include "gstsrtpdec.h"

#ifndef HAVE_SRTP2
srtp_err_status_t
srtp_set_stream_roc (srtp_t session, guint32 ssrc, guint32 roc)
{
  srtp_stream_t stream;

  stream = srtp_get_stream (session, htonl (ssrc));
  if (stream == NULL) {
    return srtp_err_status_bad_param;
  }

  rdbx_set_roc (&stream->rtp_rdbx, roc);
  return srtp_err_status_ok;
}

srtp_err_status_t
srtp_get_stream_roc (srtp_t session, guint32 ssrc, guint32 * roc)
{
  srtp_stream_t stream;

  stream = srtp_get_stream (session, htonl (ssrc));
  if (stream == NULL) {
    return srtp_err_status_bad_param;
  }

  *roc = stream->rtp_rdbx.index >> 16;
  return srtp_err_status_ok;
}
#endif

static void free_reporter_data (gpointer data);

GPrivate current_callback = G_PRIVATE_INIT (free_reporter_data);

struct GstSrtpEventReporterData
{
  gboolean soft_limit_reached;
};

static void
free_reporter_data (gpointer data)
{
  g_free (data);
}


static void
srtp_event_reporter (srtp_event_data_t * data)
{
  struct GstSrtpEventReporterData *dat = g_private_get (&current_callback);

  if (!dat)
    return;

  switch (data->event) {
    case event_key_soft_limit:
      dat->soft_limit_reached = TRUE;
      break;

    default:
      break;
  }
}

void
gst_srtp_init_event_reporter (void)
{
  struct GstSrtpEventReporterData *dat = g_private_get (&current_callback);

  if (!dat) {
    dat = g_new (struct GstSrtpEventReporterData, 1);
    g_private_set (&current_callback, dat);
  }

  dat->soft_limit_reached = FALSE;

  srtp_install_event_handler (srtp_event_reporter);
}

const gchar *
enum_nick_from_value (GType enum_gtype, gint value)
{
  GEnumClass *enum_class = g_type_class_ref (enum_gtype);
  GEnumValue *enum_value;
  const gchar *nick;

  if (!enum_gtype)
    return NULL;

  enum_value = g_enum_get_value (enum_class, value);
  if (!enum_value)
    return NULL;
  nick = enum_value->value_nick;
  g_type_class_unref (enum_class);

  return nick;
}


gint
enum_value_from_nick (GType enum_gtype, const gchar * nick)
{
  GEnumClass *enum_class = g_type_class_ref (enum_gtype);
  GEnumValue *enum_value;
  gint value;

  if (!enum_gtype)
    return -1;

  enum_value = g_enum_get_value_by_nick (enum_class, nick);
  if (!enum_value)
    return -1;
  value = enum_value->value;
  g_type_class_unref (enum_class);

  return value;
}

gboolean
gst_srtp_get_soft_limit_reached (void)
{
  struct GstSrtpEventReporterData *dat = g_private_get (&current_callback);

  if (dat)
    return dat->soft_limit_reached;
  return FALSE;
}

/* Get SSRC from RTCP buffer
 */
gboolean
rtcp_buffer_get_ssrc (GstBuffer * buf, guint32 * ssrc)
{
  gboolean ret = FALSE;
  GstRTCPBuffer rtcpbuf = GST_RTCP_BUFFER_INIT;
  GstRTCPPacket packet;

  /* Get SSRC from RR or SR packet (RTCP) */

  if (!gst_rtcp_buffer_map (buf, GST_MAP_READ, &rtcpbuf))
    return FALSE;

  if (gst_rtcp_buffer_get_first_packet (&rtcpbuf, &packet)) {
    GstRTCPType type;
    do {
      type = gst_rtcp_packet_get_type (&packet);
      switch (type) {
        case GST_RTCP_TYPE_RR:
          *ssrc = gst_rtcp_packet_rr_get_ssrc (&packet);
          ret = TRUE;
          break;
        case GST_RTCP_TYPE_SR:
          gst_rtcp_packet_sr_get_sender_info (&packet, ssrc, NULL, NULL, NULL,
              NULL);
          ret = TRUE;
          break;
        case GST_RTCP_TYPE_RTPFB:
        case GST_RTCP_TYPE_PSFB:
          *ssrc = gst_rtcp_packet_fb_get_sender_ssrc (&packet);
          ret = TRUE;
          break;
        case GST_RTCP_TYPE_APP:
          *ssrc = gst_rtcp_packet_app_get_ssrc (&packet);
          ret = TRUE;
          break;
        case GST_RTCP_TYPE_BYE:
          *ssrc = gst_rtcp_packet_bye_get_nth_ssrc (&packet, 0);
          ret = TRUE;
          break;
        default:
          break;
      }
    } while ((ret == FALSE) && (type != GST_RTCP_TYPE_INVALID) &&
        gst_rtcp_packet_move_to_next (&packet));
  }

  gst_rtcp_buffer_unmap (&rtcpbuf);

  return ret;
}

void
set_crypto_policy_cipher_auth (GstSrtpCipherType cipher,
    GstSrtpAuthType auth, srtp_crypto_policy_t * policy)
{
  switch (cipher) {
    case GST_SRTP_CIPHER_AES_128_ICM:
      policy->cipher_type = SRTP_AES_ICM_128;
      break;
    case GST_SRTP_CIPHER_AES_256_ICM:
      policy->cipher_type = SRTP_AES_ICM_256;
      break;
    case GST_SRTP_CIPHER_AES_128_GCM:
      policy->cipher_type = SRTP_AES_GCM_128;
      break;
    case GST_SRTP_CIPHER_AES_256_GCM:
      policy->cipher_type = SRTP_AES_GCM_256;
      break;
    case GST_SRTP_CIPHER_NULL:
      policy->cipher_type = SRTP_NULL_CIPHER;
      break;
    default:
      g_assert_not_reached ();
  }

  policy->cipher_key_len = cipher_key_size (cipher);

  switch (auth) {
    case GST_SRTP_AUTH_HMAC_SHA1_80:
      policy->auth_type = SRTP_HMAC_SHA1;
      policy->auth_key_len = 20;
      policy->auth_tag_len = 10;
      break;
    case GST_SRTP_AUTH_HMAC_SHA1_32:
      policy->auth_type = SRTP_HMAC_SHA1;
      policy->auth_key_len = 20;
      policy->auth_tag_len = 4;
      break;
    case GST_SRTP_AUTH_NULL:
      policy->auth_type = SRTP_NULL_AUTH;
      policy->auth_key_len = 0;
      if (cipher == GST_SRTP_CIPHER_AES_128_GCM
          || cipher == GST_SRTP_CIPHER_AES_256_GCM) {
        policy->auth_tag_len = 16;
      } else {
        policy->auth_tag_len = 0;
      }
      break;
  }

  if (cipher == GST_SRTP_CIPHER_NULL && auth == GST_SRTP_AUTH_NULL)
    policy->sec_serv = sec_serv_none;
  else if (cipher == GST_SRTP_CIPHER_NULL)
    policy->sec_serv = sec_serv_auth;
  else if (auth == GST_SRTP_AUTH_NULL)
    policy->sec_serv = sec_serv_conf;
  else
    policy->sec_serv = sec_serv_conf_and_auth;
}

guint
cipher_key_size (GstSrtpCipherType cipher)
{
  guint size = 0;

  switch (cipher) {
    case GST_SRTP_CIPHER_AES_128_ICM:
      size = SRTP_AES_ICM_128_KEY_LEN_WSALT;
      break;
    case GST_SRTP_CIPHER_AES_256_ICM:
      size = SRTP_AES_ICM_256_KEY_LEN_WSALT;
      break;
    case GST_SRTP_CIPHER_AES_128_GCM:
      size = SRTP_AES_GCM_128_KEY_LEN_WSALT;
      break;
    case GST_SRTP_CIPHER_AES_256_GCM:
      size = SRTP_AES_GCM_256_KEY_LEN_WSALT;
      break;
    case GST_SRTP_CIPHER_NULL:
      break;
    default:
      g_assert_not_reached ();
  }

  return size;
}
