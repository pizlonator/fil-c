/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2004-2010  Marcel Holtmann <marcel@holtmann.org>
 *  Copyright (C) 2012 Collabora Ltd.
 *
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <stdint.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <bluetooth/bluetooth.h>
#include "a2dp-codecs.h"

#include <gio/gunixfdlist.h>
#include <gst/gst.h>
#include "gstavdtputil.h"
#include "bluez.h"

#define TEMPLATE_MAX_BITPOOL 64

GST_DEBUG_CATEGORY_EXTERN (avdtp_debug);
#define GST_CAT_DEFAULT avdtp_debug

static void gst_avdtp_connection_transport_release (GstAvdtpConnection * conn);

static gboolean
on_state_change (BluezMediaTransport1 * proxy, GParamSpec * pspec,
    GstAvdtpConnection * conn)
{
  const gchar *newstate;
  gboolean is_idle;

  newstate = bluez_media_transport1_get_state (proxy);
  is_idle = g_str_equal (newstate, "idle");

  if (!conn->data.is_acquired && !is_idle) {
    GST_DEBUG ("Re-acquiring connection");
    gst_avdtp_connection_acquire (conn, TRUE);

  } else if (is_idle) {
    /* We don't know if we need to release the transport -- that may have been
     * done for us by bluez already! Or not ... so release it just in case, but
     * mark its stale beforehand to suppress any errors. */
    GST_DEBUG ("Marking connection stale");
    conn->data.is_acquired = FALSE;
    gst_avdtp_connection_transport_release (conn);

  } else
    GST_DEBUG ("State is %s, acquired is %s", newstate,
        conn->data.is_acquired ? "true" : "false");

  return TRUE;
}

gboolean
gst_avdtp_connection_acquire (GstAvdtpConnection * conn, gboolean use_try)
{
  GVariant *handle = NULL;
  GUnixFDList *fd_list = NULL;
  GError *err = NULL;
  int fd;
  uint16_t imtu, omtu;

  if (conn->transport == NULL) {
    GST_ERROR ("No transport specified");
    return FALSE;
  }

  if (conn->data.conn == NULL) {
    conn->data.conn =
        bluez_media_transport1_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
        G_DBUS_PROXY_FLAGS_NONE, "org.bluez", conn->transport, NULL, &err);

    if (conn->data.conn == NULL) {
      GST_ERROR ("Failed to create proxy for media transport: %s",
          err && err->message ? err->message : "Unknown error");
      g_clear_error (&err);
      return FALSE;
    }

    g_signal_connect (conn->data.conn, "notify::state",
        G_CALLBACK (on_state_change), conn);
  }

  if (conn->data.is_acquired) {
    GST_INFO ("Transport is already acquired");
    return TRUE;
  }

  if (use_try) {
    if (!bluez_media_transport1_call_try_acquire_sync (conn->data.conn,
            NULL, &handle, &imtu, &omtu, &fd_list, NULL, &err))
      goto fail;
  } else {
    if (!bluez_media_transport1_call_acquire_sync (conn->data.conn,
            NULL, &handle, &imtu, &omtu, &fd_list, NULL, &err))
      goto fail;
  }

  fd = g_unix_fd_list_get (fd_list, g_variant_get_handle (handle), &err);
  if (fd < 0)
    goto fail;

  g_variant_unref (handle);
  g_object_unref (fd_list);
  conn->stream = g_io_channel_unix_new (fd);
  g_io_channel_set_encoding (conn->stream, NULL, NULL);
  g_io_channel_set_close_on_unref (conn->stream, TRUE);
  conn->data.link_mtu = omtu;
  conn->data.is_acquired = TRUE;

  return TRUE;

fail:
  GST_ERROR ("Failed to %s transport stream: %s", use_try ? "try_acquire" :
      "acquire", err && err->message ? err->message : "unknown error");

  g_clear_error (&err);
  if (handle)
    g_variant_unref (handle);

  conn->data.is_acquired = FALSE;
  return FALSE;
}

static void
gst_avdtp_connection_transport_release (GstAvdtpConnection * conn)
{
  GError *err = NULL;

  if (!bluez_media_transport1_call_release_sync (conn->data.conn, NULL, &err)) {
    /* We don't care about errors if the transport was already marked stale */
    if (!conn->data.is_acquired) {
      g_clear_error (&err);
      return;
    }

    GST_ERROR ("Failed to release transport stream: %s", err->message ?
        err->message : "unknown error");
    g_clear_error (&err);
  }
  conn->data.is_acquired = FALSE;
}

void
gst_avdtp_connection_release (GstAvdtpConnection * conn)
{
  if (conn->stream) {
    g_io_channel_shutdown (conn->stream, TRUE, NULL);
    g_io_channel_unref (conn->stream);
    conn->stream = NULL;
  }

  if (conn->data.uuid) {
    g_free (conn->data.uuid);
    conn->data.uuid = NULL;
  }

  if (conn->data.config) {
    g_free (conn->data.config);
    conn->data.config = NULL;
  }

  if (conn->data.conn) {
    if (conn->transport)
      gst_avdtp_connection_transport_release (conn);

    g_clear_object (&conn->data.conn);
  }
}

void
gst_avdtp_connection_reset (GstAvdtpConnection * conn)
{
  gst_avdtp_connection_release (conn);

  if (conn->device) {
    g_free (conn->device);
    conn->device = NULL;
  }

  if (conn->transport) {
    g_free (conn->transport);
    conn->transport = NULL;
  }
}

void
gst_avdtp_connection_set_device (GstAvdtpConnection * conn, const char *device)
{
  g_free (conn->device);

  conn->device = g_strdup (device);
}

void
gst_avdtp_connection_set_transport (GstAvdtpConnection * conn,
    const char *transport)
{
  g_free (conn->transport);

  conn->transport = g_strdup (transport);
}

gboolean
gst_avdtp_connection_get_properties (GstAvdtpConnection * conn)
{
  GVariant *var;

  conn->data.codec = bluez_media_transport1_get_codec (conn->data.conn);

  conn->data.uuid = bluez_media_transport1_dup_uuid (conn->data.conn);

  var = bluez_media_transport1_dup_configuration (conn->data.conn);
  conn->data.config_size = g_variant_get_size (var);
  conn->data.config = g_new0 (guint8, conn->data.config_size);
  g_variant_store (var, conn->data.config);
  g_variant_unref (var);

  return TRUE;
}

static GstStructure *
gst_avdtp_util_parse_sbc_raw (void *config)
{
  a2dp_sbc_t *sbc = (a2dp_sbc_t *) config;
  GstStructure *structure;
  GValue *value;
  GValue *list;
  gboolean mono, stereo;

  structure = gst_structure_new_empty ("audio/x-sbc");
  value = g_value_init (g_new0 (GValue, 1), G_TYPE_STRING);
  list = g_value_init (g_new0 (GValue, 1), GST_TYPE_LIST);

  /* mode */
  if (sbc->channel_mode & SBC_CHANNEL_MODE_MONO) {
    g_value_set_static_string (value, "mono");
    gst_value_list_prepend_value (list, value);
  }
  if (sbc->channel_mode & SBC_CHANNEL_MODE_STEREO) {
    g_value_set_static_string (value, "stereo");
    gst_value_list_prepend_value (list, value);
  }
  if (sbc->channel_mode & SBC_CHANNEL_MODE_DUAL_CHANNEL) {
    g_value_set_static_string (value, "dual");
    gst_value_list_prepend_value (list, value);
  }
  if (sbc->channel_mode & SBC_CHANNEL_MODE_JOINT_STEREO) {
    g_value_set_static_string (value, "joint");
    gst_value_list_prepend_value (list, value);
  }
  if (gst_value_list_get_size (list) == 1)
    gst_structure_set_value (structure, "channel-mode", value);
  else
    gst_structure_take_value (structure, "channel-mode", list);

  g_value_unset (value);
  g_value_reset (list);

  /* subbands */
  value = g_value_init (value, G_TYPE_INT);
  if (sbc->subbands & SBC_SUBBANDS_4) {
    g_value_set_int (value, 4);
    gst_value_list_prepend_value (list, value);
  }
  if (sbc->subbands & SBC_SUBBANDS_8) {
    g_value_set_int (value, 8);
    gst_value_list_prepend_value (list, value);
  }
  if (gst_value_list_get_size (list) == 1)
    gst_structure_set_value (structure, "subbands", value);
  else
    gst_structure_take_value (structure, "subbands", list);

  g_value_unset (value);
  g_value_reset (list);

  /* blocks */
  value = g_value_init (value, G_TYPE_INT);
  if (sbc->block_length & SBC_BLOCK_LENGTH_16) {
    g_value_set_int (value, 16);
    gst_value_list_prepend_value (list, value);
  }
  if (sbc->block_length & SBC_BLOCK_LENGTH_12) {
    g_value_set_int (value, 12);
    gst_value_list_prepend_value (list, value);
  }
  if (sbc->block_length & SBC_BLOCK_LENGTH_8) {
    g_value_set_int (value, 8);
    gst_value_list_prepend_value (list, value);
  }
  if (sbc->block_length & SBC_BLOCK_LENGTH_4) {
    g_value_set_int (value, 4);
    gst_value_list_prepend_value (list, value);
  }
  if (gst_value_list_get_size (list) == 1)
    gst_structure_set_value (structure, "blocks", value);
  else
    gst_structure_take_value (structure, "blocks", list);

  g_value_unset (value);
  g_value_reset (list);

  /* allocation */
  g_value_init (value, G_TYPE_STRING);
  if (sbc->allocation_method & SBC_ALLOCATION_LOUDNESS) {
    g_value_set_static_string (value, "loudness");
    gst_value_list_prepend_value (list, value);
  }
  if (sbc->allocation_method & SBC_ALLOCATION_SNR) {
    g_value_set_static_string (value, "snr");
    gst_value_list_prepend_value (list, value);
  }
  if (gst_value_list_get_size (list) == 1)
    gst_structure_set_value (structure, "allocation-method", value);
  else
    gst_structure_take_value (structure, "allocation-method", list);

  g_value_unset (value);
  g_value_reset (list);

  /* rate */
  g_value_init (value, G_TYPE_INT);
  if (sbc->frequency & SBC_SAMPLING_FREQ_48000) {
    g_value_set_int (value, 48000);
    gst_value_list_prepend_value (list, value);
  }
  if (sbc->frequency & SBC_SAMPLING_FREQ_44100) {
    g_value_set_int (value, 44100);
    gst_value_list_prepend_value (list, value);
  }
  if (sbc->frequency & SBC_SAMPLING_FREQ_32000) {
    g_value_set_int (value, 32000);
    gst_value_list_prepend_value (list, value);
  }
  if (sbc->frequency & SBC_SAMPLING_FREQ_16000) {
    g_value_set_int (value, 16000);
    gst_value_list_prepend_value (list, value);
  }
  if (gst_value_list_get_size (list) == 1)
    gst_structure_set_value (structure, "rate", value);
  else
    gst_structure_take_value (structure, "rate", list);

  g_value_unset (value);
  g_value_reset (list);

  /* bitpool */
  /* The reported bitpool can be a fixed value instead
   * of a range, but GStreamer ranges do not allow for
   * the min == max case, so use int as value in such
   * cases instead. The bitpool value is used by
   * sbcenc, sbcdec, rtpsbcpay, and all of them use
   * a fixated version of that value, so there is no
   * problem with int vs. int-range type mismatches. */
  if (sbc->min_bitpool == sbc->max_bitpool) {
    value = g_value_init (value, G_TYPE_INT);
    g_value_set_int (value, MIN (sbc->max_bitpool, TEMPLATE_MAX_BITPOOL));
  } else {
    value = g_value_init (value, GST_TYPE_INT_RANGE);
    gst_value_set_int_range (value,
        MIN (sbc->min_bitpool, TEMPLATE_MAX_BITPOOL),
        MIN (sbc->max_bitpool, TEMPLATE_MAX_BITPOOL));
  }
  gst_structure_set_value (structure, "bitpool", value);
  g_value_unset (value);

  /* channels */
  mono = FALSE;
  stereo = FALSE;
  if (sbc->channel_mode & SBC_CHANNEL_MODE_MONO)
    mono = TRUE;
  if ((sbc->channel_mode & SBC_CHANNEL_MODE_STEREO) ||
      (sbc->channel_mode &
          SBC_CHANNEL_MODE_DUAL_CHANNEL) ||
      (sbc->channel_mode & SBC_CHANNEL_MODE_JOINT_STEREO))
    stereo = TRUE;

  if (mono && stereo) {
    g_value_init (value, GST_TYPE_INT_RANGE);
    gst_value_set_int_range (value, 1, 2);
  } else {
    g_value_init (value, G_TYPE_INT);
    if (mono)
      g_value_set_int (value, 1);
    else if (stereo)
      g_value_set_int (value, 2);
    else {
      GST_ERROR ("Unexpected number of channels");
      g_value_set_int (value, 0);
    }
  }

  gst_structure_set_value (structure, "channels", value);

  g_value_unset (value);
  g_free (value);
  g_value_unset (list);
  g_free (list);

  return structure;
}

static GstStructure *
gst_avdtp_util_parse_mpeg_raw (void *config)
{
  a2dp_mpeg_t *mpeg = (a2dp_mpeg_t *) config;
  GstStructure *structure;
  GValue *value;
  GValue *list;
  gboolean valid_layer = FALSE;
  gboolean mono, stereo;

  structure = gst_structure_new_empty ("audio/mpeg");
  value = g_new0 (GValue, 1);
  g_value_init (value, G_TYPE_INT);

  list = g_value_init (g_new0 (GValue, 1), GST_TYPE_LIST);
  g_value_set_int (value, 1);
  gst_value_list_prepend_value (list, value);
  g_value_set_int (value, 2);
  gst_value_list_prepend_value (list, value);
  gst_structure_set_value (structure, "mpegversion", list);
  g_free (list);

  /* layer */
  list = g_value_init (g_new0 (GValue, 1), GST_TYPE_LIST);
  if (mpeg->layer & MPEG_LAYER_MP1) {
    g_value_set_int (value, 1);
    gst_value_list_prepend_value (list, value);
    valid_layer = TRUE;
  }
  if (mpeg->layer & MPEG_LAYER_MP2) {
    g_value_set_int (value, 2);
    gst_value_list_prepend_value (list, value);
    valid_layer = TRUE;
  }
  if (mpeg->layer & MPEG_LAYER_MP3) {
    g_value_set_int (value, 3);
    gst_value_list_prepend_value (list, value);
    valid_layer = TRUE;
  }
  if (list) {
    if (gst_value_list_get_size (list) == 1)
      gst_structure_set_value (structure, "layer", value);
    else
      gst_structure_set_value (structure, "layer", list);
    g_free (list);
    list = NULL;
  }

  if (!valid_layer) {
    gst_structure_free (structure);
    g_free (value);
    return NULL;
  }

  /* rate */
  list = g_value_init (g_new0 (GValue, 1), GST_TYPE_LIST);
  if (mpeg->frequency & MPEG_SAMPLING_FREQ_48000) {
    g_value_set_int (value, 48000);
    gst_value_list_prepend_value (list, value);
  }
  if (mpeg->frequency & MPEG_SAMPLING_FREQ_44100) {
    g_value_set_int (value, 44100);
    gst_value_list_prepend_value (list, value);
  }
  if (mpeg->frequency & MPEG_SAMPLING_FREQ_32000) {
    g_value_set_int (value, 32000);
    gst_value_list_prepend_value (list, value);
  }
  if (mpeg->frequency & MPEG_SAMPLING_FREQ_24000) {
    g_value_set_int (value, 24000);
    gst_value_list_prepend_value (list, value);
  }
  if (mpeg->frequency & MPEG_SAMPLING_FREQ_22050) {
    g_value_set_int (value, 22050);
    gst_value_list_prepend_value (list, value);
  }
  if (mpeg->frequency & MPEG_SAMPLING_FREQ_16000) {
    g_value_set_int (value, 16000);
    gst_value_list_prepend_value (list, value);
  }
  g_value_unset (value);
  if (list) {
    if (gst_value_list_get_size (list) == 1)
      gst_structure_set_value (structure, "rate", value);
    else
      gst_structure_set_value (structure, "rate", list);
    g_free (list);
    list = NULL;
  }

  /* channels */
  mono = FALSE;
  stereo = FALSE;
  if (mpeg->channel_mode & MPEG_CHANNEL_MODE_MONO)
    mono = TRUE;
  if ((mpeg->channel_mode & MPEG_CHANNEL_MODE_STEREO) ||
      (mpeg->channel_mode &
          MPEG_CHANNEL_MODE_DUAL_CHANNEL) ||
      (mpeg->channel_mode & MPEG_CHANNEL_MODE_JOINT_STEREO))
    stereo = TRUE;

  if (mono && stereo) {
    g_value_init (value, GST_TYPE_INT_RANGE);
    gst_value_set_int_range (value, 1, 2);
  } else {
    g_value_init (value, G_TYPE_INT);
    if (mono)
      g_value_set_int (value, 1);
    else if (stereo)
      g_value_set_int (value, 2);
    else {
      GST_ERROR ("Unexpected number of channels");
      g_value_set_int (value, 0);
    }
  }
  gst_structure_set_value (structure, "channels", value);
  g_free (value);

  return structure;
}

static GstStructure *
gst_avdtp_util_parse_aac_raw (void *config)
{
  GstStructure *structure;
  GValue value = G_VALUE_INIT;
  GValue value_str = G_VALUE_INIT;
  GValue list = G_VALUE_INIT;
  a2dp_aac_t aac_local = { 0 };
  a2dp_aac_t *aac = &aac_local;

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
  uint8_t *raw = (uint8_t *) config;
  aac->object_type = raw[0];
  aac->frequency = (raw[1] << 4) | ((raw[2] & 0xFF) >> 4);
  aac->channels = (raw[2] >> 2) & 0x3;
  aac->rfa = raw[2] & 0x3;
  aac->vbr = (raw[3] >> 7) & 0x1;
  aac->bitrate = (raw[4] << 16) | (raw[3] << 8) | raw[4];
  aac->bitrate &= ~0x800000;
#elif G_BYTE_ORDER == G_BIG_ENDIAN
  *aac = *((a2dp_aac_t *) config);
#else
#error "Unknown byte order"
#endif

  GST_LOG ("aac objtype=%x freq=%x rfa=%x channels=%x vbr=%x bitrate=%x",
      aac->object_type, aac->frequency, aac->rfa, aac->channels, aac->vbr,
      aac->bitrate);

  structure = gst_structure_new_empty ("audio/mpeg");
  g_value_init (&value, G_TYPE_INT);
  g_value_init (&value_str, G_TYPE_STRING);

  /* mpegversion */
  g_value_init (&list, GST_TYPE_LIST);
  if (aac->object_type & AAC_OBJECT_TYPE_MPEG2_AAC_LC) {
    g_value_set_int (&value, 2);
    gst_value_list_prepend_value (&list, &value);
  }
  if ((aac->object_type & AAC_OBJECT_TYPE_MPEG4_AAC_LC)
      || (aac->object_type & AAC_OBJECT_TYPE_MPEG4_AAC_LTP)
      || (aac->object_type & AAC_OBJECT_TYPE_MPEG4_AAC_SCALABLE)) {
    g_value_set_int (&value, 4);
    gst_value_list_prepend_value (&list, &value);
  }
  if (gst_value_list_get_size (&list) == 1)
    gst_structure_set_value (structure, "mpegversion", &value);
  else
    gst_structure_set_value (structure, "mpegversion", &list);

  g_value_reset (&list);

  /* base-profile */
  if (aac->object_type & AAC_OBJECT_TYPE_MPEG2_AAC_LC
      || aac->object_type & AAC_OBJECT_TYPE_MPEG4_AAC_LC) {
    g_value_set_string (&value_str, "lc");
    gst_value_list_prepend_value (&list, &value_str);
  }
  if (aac->object_type & AAC_OBJECT_TYPE_MPEG4_AAC_LTP) {
    g_value_set_string (&value_str, "ltp");
    gst_value_list_prepend_value (&list, &value_str);
  }
  if (aac->object_type & AAC_OBJECT_TYPE_MPEG4_AAC_SCALABLE) {
    g_value_set_string (&value_str, "ssr");
    gst_value_list_prepend_value (&list, &value_str);
  }
  if (gst_value_list_get_size (&list) == 1)
    gst_structure_set_value (structure, "base-profile", &value_str);
  else
    gst_structure_set_value (structure, "base-profile", &list);

  g_value_reset (&list);

  /* rate */
  g_value_init (&list, GST_TYPE_LIST);
  if (aac->frequency & AAC_SAMPLING_FREQ_8000) {
    g_value_set_int (&value, 8000);
    gst_value_list_prepend_value (&list, &value);
  }
  if (aac->frequency & AAC_SAMPLING_FREQ_11025) {
    g_value_set_int (&value, 11025);
    gst_value_list_prepend_value (&list, &value);
  }
  if (aac->frequency & AAC_SAMPLING_FREQ_12000) {
    g_value_set_int (&value, 12000);
    gst_value_list_prepend_value (&list, &value);
  }
  if (aac->frequency & AAC_SAMPLING_FREQ_16000) {
    g_value_set_int (&value, 16000);
    gst_value_list_prepend_value (&list, &value);
  }
  if (aac->frequency & AAC_SAMPLING_FREQ_22050) {
    g_value_set_int (&value, 22050);
    gst_value_list_prepend_value (&list, &value);
  }
  if (aac->frequency & AAC_SAMPLING_FREQ_24000) {
    g_value_set_int (&value, 24000);
    gst_value_list_prepend_value (&list, &value);
  }
  if (aac->frequency & AAC_SAMPLING_FREQ_32000) {
    g_value_set_int (&value, 32000);
    gst_value_list_prepend_value (&list, &value);
  }
  if (aac->frequency & AAC_SAMPLING_FREQ_44100) {
    g_value_set_int (&value, 44100);
    gst_value_list_prepend_value (&list, &value);
  }
  if (aac->frequency & AAC_SAMPLING_FREQ_48000) {
    g_value_set_int (&value, 48000);
    gst_value_list_prepend_value (&list, &value);
  }
  if (aac->frequency & AAC_SAMPLING_FREQ_64000) {
    g_value_set_int (&value, 64000);
    gst_value_list_prepend_value (&list, &value);
  }
  if (aac->frequency & AAC_SAMPLING_FREQ_88200) {
    g_value_set_int (&value, 88200);
    gst_value_list_prepend_value (&list, &value);
  }
  if (aac->frequency & AAC_SAMPLING_FREQ_96000) {
    g_value_set_int (&value, 96000);
    gst_value_list_prepend_value (&list, &value);
  }
  if (gst_value_list_get_size (&list) == 1)
    gst_structure_set_value (structure, "rate", &value);
  else
    gst_structure_set_value (structure, "rate", &list);

  g_value_reset (&list);

  /* channels */
  g_value_init (&list, GST_TYPE_LIST);
  if (aac->channels & AAC_CHANNELS_1) {
    g_value_set_int (&value, 1);
    gst_value_list_prepend_value (&list, &value);
  }
  if (aac->channels & AAC_CHANNELS_2) {
    g_value_set_int (&value, 2);
    gst_value_list_prepend_value (&list, &value);
  }
  if (gst_value_list_get_size (&list) == 1)
    gst_structure_set_value (structure, "channels", &value);
  else
    gst_structure_set_value (structure, "channels", &list);

  GST_LOG ("AAC caps: %" GST_PTR_FORMAT, structure);

  g_value_unset (&list);
  g_value_unset (&value);
  g_value_unset (&value_str);

  return structure;
}

static GstStructure *
gst_avdtp_util_parse_ldac_raw (void *config)
{
  /* We assume the vendor/codec ID have been verified already */
  a2dp_ldac_t *ldac = (a2dp_ldac_t *) config;
  GstStructure *structure;
  GValue value = G_VALUE_INIT;
  GValue list = G_VALUE_INIT;
  gboolean mono, stereo;

  structure = gst_structure_new_empty ("audio/x-ldac");

  g_value_init (&list, GST_TYPE_LIST);
  g_value_init (&value, G_TYPE_INT);

  /* rate */
  if (ldac->frequency & LDAC_SAMPLING_FREQ_44100) {
    g_value_set_int (&value, 44100);
    gst_value_list_prepend_value (&list, &value);
  }
  if (ldac->frequency & LDAC_SAMPLING_FREQ_48000) {
    g_value_set_int (&value, 48000);
    gst_value_list_prepend_value (&list, &value);
  }
  if (ldac->frequency & LDAC_SAMPLING_FREQ_88200) {
    g_value_set_int (&value, 88200);
    gst_value_list_prepend_value (&list, &value);
  }
  if (ldac->frequency & LDAC_SAMPLING_FREQ_96000) {
    g_value_set_int (&value, 96000);
    gst_value_list_prepend_value (&list, &value);
  }

  if (gst_value_list_get_size (&list) == 1)
    gst_structure_set_value (structure, "rate", &value);
  else
    gst_structure_set_value (structure, "rate", &list);

  g_value_unset (&value);
  g_value_reset (&list);

  /* channels */
  mono = FALSE;
  stereo = FALSE;
  if (ldac->channel_mode & LDAC_CHANNEL_MODE_MONO)
    mono = TRUE;
  if ((ldac->channel_mode & LDAC_CHANNEL_MODE_STEREO) ||
      (ldac->channel_mode & LDAC_CHANNEL_MODE_DUAL))
    stereo = TRUE;

  if (mono && stereo) {
    g_value_init (&value, GST_TYPE_INT_RANGE);
    gst_value_set_int_range (&value, 1, 2);
  } else {
    g_value_init (&value, G_TYPE_INT);
    if (mono)
      g_value_set_int (&value, 1);
    else if (stereo)
      g_value_set_int (&value, 2);
    else {
      GST_ERROR ("Unexpected number of channels");
      g_value_set_int (&value, 0);
    }
  }
  gst_structure_set_value (structure, "channels", &value);

  g_value_unset (&value);
  g_value_init (&value, G_TYPE_STRING);

  /* channel mode */
  if (ldac->channel_mode & LDAC_CHANNEL_MODE_MONO) {
    g_value_set_static_string (&value, "mono");
    gst_value_list_prepend_value (&list, &value);
  }
  if (ldac->channel_mode & LDAC_CHANNEL_MODE_STEREO) {
    g_value_set_static_string (&value, "stereo");
    gst_value_list_prepend_value (&list, &value);
  }
  if (ldac->channel_mode & LDAC_CHANNEL_MODE_DUAL) {
    g_value_set_static_string (&value, "dual");
    gst_value_list_prepend_value (&list, &value);
  }

  if (gst_value_list_get_size (&list) == 1)
    gst_structure_set_value (structure, "channel-mode", &value);
  else
    gst_structure_take_value (structure, "channel-mode", &list);

  g_value_unset (&value);
  g_value_unset (&list);

  return structure;
}

static GstStructure *
gst_avdtp_util_parse_vendor_raw (void *config)
{
  a2dp_vendor_codec_t *vendor = (a2dp_vendor_codec_t *) config;

  if (A2DP_GET_VENDOR_ID (*vendor) == SONY_VENDOR_ID &&
      A2DP_GET_CODEC_ID (*vendor) == LDAC_CODEC_ID)
    return gst_avdtp_util_parse_ldac_raw (config);
  else
    return NULL;
}

GstCaps *
gst_avdtp_connection_get_caps (GstAvdtpConnection * conn)
{
  GstCaps *caps;
  GstStructure *structure;

  if (conn->data.config_size == 0 || conn->data.config == NULL)
    return NULL;

  switch (conn->data.codec) {
    case A2DP_CODEC_SBC:
      structure = gst_avdtp_util_parse_sbc_raw (conn->data.config);
      break;
    case A2DP_CODEC_MPEG12:
      structure = gst_avdtp_util_parse_mpeg_raw (conn->data.config);
      break;
    case A2DP_CODEC_MPEG24:
      structure = gst_avdtp_util_parse_aac_raw (conn->data.config);
      break;
    case A2DP_CODEC_VENDOR:
      structure = gst_avdtp_util_parse_vendor_raw (conn->data.config);
      break;
    default:
      GST_ERROR ("Unsupported configuration");
      return NULL;
  }

  if (structure == NULL)
    return FALSE;

  caps = gst_caps_new_full (structure, NULL);

  return caps;
}

void
gst_avdtp_connection_notify_volume (GstAvdtpConnection * conn,
    GObject * source, const gchar * property)
{
  g_object_bind_property (source, property, conn->data.conn, "volume",
      G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
}

gboolean
gst_avdtp_connection_conf_recv_stream_fd (GstAvdtpConnection * conn)
{
  struct bluetooth_data *data = &conn->data;
  GIOStatus status;
  GIOFlags flags;
  int fd;
  int priority;

  /* Proceed if stream was already acquired */
  if (conn->stream == NULL) {
    GST_ERROR ("Error while configuring device: "
        "could not acquire audio socket");
    return FALSE;
  }

  /* set stream socket to nonblock */
  flags = g_io_channel_get_flags (conn->stream);
  flags |= G_IO_FLAG_NONBLOCK;
  status = g_io_channel_set_flags (conn->stream, flags, NULL);
  if (status != G_IO_STATUS_NORMAL)
    GST_WARNING ("Error while setting server socket to nonblock");

  fd = g_io_channel_unix_get_fd (conn->stream);

  /* It is possible there is some outstanding
     data in the pipe - we have to empty it */
  while (1) {
    ssize_t bread = read (fd, data->buffer, data->link_mtu);
    if (bread <= 0)
      break;
  }

  /* set stream socket to block */
  flags = g_io_channel_get_flags (conn->stream);
  flags &= ~G_IO_FLAG_NONBLOCK;
  status = g_io_channel_set_flags (conn->stream, flags, NULL);
  if (status != G_IO_STATUS_NORMAL)
    GST_WARNING ("Error while setting server socket to block");

  priority = 6;
  if (setsockopt (fd, SOL_SOCKET, SO_PRIORITY, (const void *) &priority,
          sizeof (priority)) < 0)
    GST_WARNING ("Unable to set socket to low delay");

  memset (data->buffer, 0, sizeof (data->buffer));

  return TRUE;
}
