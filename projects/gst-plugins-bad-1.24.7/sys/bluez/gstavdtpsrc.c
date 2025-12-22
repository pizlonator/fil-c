/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2012  Collabora Ltd.
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

#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <poll.h>

#include <gst/rtp/gstrtppayloads.h>
#include "gstbluezelements.h"
#include "gstavdtpsrc.h"

GST_DEBUG_CATEGORY_STATIC (avdtpsrc_debug);
#define GST_CAT_DEFAULT (avdtpsrc_debug)

#define DEFAULT_VOLUME 127

enum
{
  PROP_0,
  PROP_TRANSPORT,
  PROP_TRANSPORT_VOLUME,
};

#define parent_class gst_avdtp_src_parent_class
G_DEFINE_TYPE (GstAvdtpSrc, gst_avdtp_src, GST_TYPE_BASE_SRC);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (avdtpsrc, "avdtpsrc", GST_RANK_NONE,
    GST_TYPE_AVDTP_SRC, bluez_element_init (plugin));

static GstStaticPadTemplate gst_avdtp_src_template =
    GST_STATIC_PAD_TEMPLATE ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-rtp, "
        "media = (string) \"audio\","
        "payload = (int) "
        GST_RTP_PAYLOAD_DYNAMIC_STRING ", "
        "clock-rate = (int) { 16000, 32000, "
        "44100, 48000 }, " "encoding-name = (string) \"SBC\"; "
        "application/x-rtp, "
        "media = (string) \"audio\","
        "payload = (int) "
        GST_RTP_PAYLOAD_DYNAMIC_STRING ", "
        "clock-rate = (int) { 8000, 11025, 12000, 16000, "
        "22050, 2400, 32000, 44100, 48000, 64000, 88200, 96000 }, "
        "encoding-name = (string) \"MP4A-LATM\"; "));

static void gst_avdtp_src_finalize (GObject * object);
static void gst_avdtp_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_avdtp_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);

static GstCaps *gst_avdtp_src_getcaps (GstBaseSrc * bsrc, GstCaps * filter);
static gboolean gst_avdtp_src_query (GstBaseSrc * bsrc, GstQuery * query);
static gboolean gst_avdtp_src_start (GstBaseSrc * bsrc);
static gboolean gst_avdtp_src_stop (GstBaseSrc * bsrc);
static GstFlowReturn gst_avdtp_src_create (GstBaseSrc * bsrc, guint64 offset,
    guint length, GstBuffer ** outbuf);
static gboolean gst_avdtp_src_unlock (GstBaseSrc * bsrc);
static gboolean gst_avdtp_src_unlock_stop (GstBaseSrc * bsrc);

static void
gst_avdtp_src_class_init (GstAvdtpSrcClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstBaseSrcClass *basesrc_class = GST_BASE_SRC_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_avdtp_src_finalize);
  gobject_class->set_property = GST_DEBUG_FUNCPTR (gst_avdtp_src_set_property);
  gobject_class->get_property = GST_DEBUG_FUNCPTR (gst_avdtp_src_get_property);

  basesrc_class->start = GST_DEBUG_FUNCPTR (gst_avdtp_src_start);
  basesrc_class->stop = GST_DEBUG_FUNCPTR (gst_avdtp_src_stop);
  basesrc_class->create = GST_DEBUG_FUNCPTR (gst_avdtp_src_create);
  basesrc_class->unlock = GST_DEBUG_FUNCPTR (gst_avdtp_src_unlock);
  basesrc_class->unlock_stop = GST_DEBUG_FUNCPTR (gst_avdtp_src_unlock_stop);
  basesrc_class->get_caps = GST_DEBUG_FUNCPTR (gst_avdtp_src_getcaps);
  basesrc_class->query = GST_DEBUG_FUNCPTR (gst_avdtp_src_query);

  g_object_class_install_property (gobject_class, PROP_TRANSPORT,
      g_param_spec_string ("transport",
          "Transport", "Use configured transport", NULL, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_TRANSPORT_VOLUME,
      g_param_spec_uint ("transport-volume",
          "Transport volume",
          "Volume of the transport (only valid if transport is acquired)",
          0, 127, DEFAULT_VOLUME, G_PARAM_READWRITE));

  gst_element_class_set_static_metadata (element_class,
      "Bluetooth AVDTP Source",
      "Source/Audio/Network/RTP",
      "Receives audio from an A2DP device",
      "Arun Raghavan <arun.raghavan@collabora.co.uk>");

  GST_DEBUG_CATEGORY_INIT (avdtpsrc_debug, "avdtpsrc", 0,
      "Bluetooth AVDTP Source");

  gst_element_class_add_static_pad_template (element_class,
      &gst_avdtp_src_template);
}

static void
gst_avdtp_src_init (GstAvdtpSrc * avdtpsrc)
{
  avdtpsrc->poll = gst_poll_new (TRUE);

  avdtpsrc->duration = GST_CLOCK_TIME_NONE;
  avdtpsrc->transport_volume = DEFAULT_VOLUME;

  gst_base_src_set_format (GST_BASE_SRC (avdtpsrc), GST_FORMAT_TIME);
  gst_base_src_set_live (GST_BASE_SRC (avdtpsrc), TRUE);
  gst_base_src_set_do_timestamp (GST_BASE_SRC (avdtpsrc), TRUE);
}

static void
gst_avdtp_src_finalize (GObject * object)
{
  GstAvdtpSrc *avdtpsrc = GST_AVDTP_SRC (object);

  gst_poll_free (avdtpsrc->poll);

  gst_avdtp_connection_reset (&avdtpsrc->conn);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_avdtp_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstAvdtpSrc *avdtpsrc = GST_AVDTP_SRC (object);

  switch (prop_id) {
    case PROP_TRANSPORT:
      g_value_set_string (value, avdtpsrc->conn.transport);
      break;

    case PROP_TRANSPORT_VOLUME:
      g_value_set_uint (value, avdtpsrc->transport_volume);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_avdtp_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstAvdtpSrc *avdtpsrc = GST_AVDTP_SRC (object);

  switch (prop_id) {
    case PROP_TRANSPORT:
      gst_avdtp_connection_set_transport (&avdtpsrc->conn,
          g_value_get_string (value));
      break;

    case PROP_TRANSPORT_VOLUME:
      avdtpsrc->transport_volume = g_value_get_uint (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_avdtp_src_query (GstBaseSrc * bsrc, GstQuery * query)
{
  GstAvdtpSrc *avdtpsrc = GST_AVDTP_SRC (bsrc);
  gboolean ret = FALSE;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_DURATION:{
      GstFormat format;

      if (avdtpsrc->duration != GST_CLOCK_TIME_NONE) {
        gst_query_parse_duration (query, &format, NULL);

        if (format == GST_FORMAT_TIME) {
          gst_query_set_duration (query, format, (gint64) avdtpsrc->duration);
          ret = TRUE;
        }
      }

      break;
    }

    default:
      ret = GST_BASE_SRC_CLASS (parent_class)->query (bsrc, query);
  }

  return ret;
}

static GstCaps *
gst_avdtp_src_getcaps (GstBaseSrc * bsrc, GstCaps * filter)
{
  GstAvdtpSrc *avdtpsrc = GST_AVDTP_SRC (bsrc);
  GstCaps *caps = NULL, *ret = NULL;

  if (avdtpsrc->dev_caps) {
    const GValue *value;
    const char *format;
    int rate;
    GstStructure *structure = gst_caps_get_structure (avdtpsrc->dev_caps, 0);

    format = gst_structure_get_name (structure);

    if (g_str_equal (format, "audio/x-sbc")) {
      /* FIXME: we can return a fixed payload type once we
       * are in PLAYING */
      caps = gst_caps_new_simple ("application/x-rtp",
          "media", G_TYPE_STRING, "audio",
          "payload", GST_TYPE_INT_RANGE, 96, 127,
          "encoding-name", G_TYPE_STRING, "SBC", NULL);
    } else if (g_str_equal (format, "audio/mpeg")) {
      caps = gst_caps_new_simple ("application/x-rtp",
          "media", G_TYPE_STRING, "audio",
          "payload", GST_TYPE_INT_RANGE, 96, 127,
          "encoding-name", G_TYPE_STRING, "MP4A-LATM", NULL);

      value = gst_structure_get_value (structure, "mpegversion");
      if (!value || !G_VALUE_HOLDS_INT (value)) {
        GST_ERROR_OBJECT (avdtpsrc, "Failed to get mpegversion");
        gst_caps_unref (caps);
        return NULL;
      }
      gst_caps_set_simple (caps, "mpegversion", G_TYPE_INT,
          g_value_get_int (value), NULL);

      value = gst_structure_get_value (structure, "channels");
      if (!value || !G_VALUE_HOLDS_INT (value)) {
        GST_ERROR_OBJECT (avdtpsrc, "Failed to get channels");
        gst_caps_unref (caps);
        return NULL;
      }
      gst_caps_set_simple (caps, "channels", G_TYPE_INT,
          g_value_get_int (value), NULL);

      value = gst_structure_get_value (structure, "base-profile");
      if (!value || !G_VALUE_HOLDS_STRING (value)) {
        GST_ERROR_OBJECT (avdtpsrc, "Failed to get base-profile");
        gst_caps_unref (caps);
        return NULL;
      }
      gst_caps_set_simple (caps, "base-profile", G_TYPE_STRING,
          g_value_get_string (value), NULL);

    } else {
      GST_ERROR_OBJECT (avdtpsrc,
          "Only SBC and MPEG-2/4 are supported at the moment");
    }

    value = gst_structure_get_value (structure, "rate");
    if (!value || !G_VALUE_HOLDS_INT (value)) {
      GST_ERROR_OBJECT (avdtpsrc, "Failed to get sample rate");
      gst_caps_unref (caps);
      return NULL;
    }
    rate = g_value_get_int (value);

    gst_caps_set_simple (caps, "clock-rate", G_TYPE_INT, rate, NULL);

    if (filter) {
      ret = gst_caps_intersect_full (filter, caps, GST_CAPS_INTERSECT_FIRST);
      gst_caps_unref (caps);
    } else
      ret = caps;
  } else {
    GST_DEBUG_OBJECT (avdtpsrc, "device not open, using template caps");
    ret = GST_BASE_SRC_CLASS (parent_class)->get_caps (bsrc, filter);
  }

  return ret;
}

static void
avrcp_metadata_cb (GstAvrcpConnection * avrcp, GstTagList * taglist,
    gpointer user_data)
{
  GstAvdtpSrc *src = GST_AVDTP_SRC (user_data);
  guint64 duration;

  if (gst_tag_list_get_uint64 (taglist, GST_TAG_DURATION, &duration)) {
    src->duration = duration;
    gst_element_post_message (GST_ELEMENT (src),
        gst_message_new_duration_changed (GST_OBJECT (src)));
  }

  gst_pad_push_event (GST_BASE_SRC_PAD (src),
      gst_event_new_tag (gst_tag_list_copy (taglist)));
  gst_element_post_message (GST_ELEMENT (src),
      gst_message_new_tag (GST_OBJECT (src), taglist));
}

static void
gst_avdtp_src_start_avrcp (GstAvdtpSrc * src)
{
  gchar *path, **strv;
  int i;

  /* Strip out the /fdX in /org/bluez/dev_.../fdX */
  strv = g_strsplit (src->conn.transport, "/", -1);

  for (i = 0; strv[i]; i++);
  g_return_if_fail (i > 0);

  g_free (strv[i - 1]);
  strv[i - 1] = NULL;

  path = g_strjoinv ("/", strv);
  g_strfreev (strv);

  src->avrcp = gst_avrcp_connection_new (path, avrcp_metadata_cb, src, NULL);

  g_free (path);
}

static void
gst_avdtp_src_stop_avrcp (GstAvdtpSrc * src)
{
  gst_avrcp_connection_free (src->avrcp);
}

static gboolean
gst_avdtp_src_start (GstBaseSrc * bsrc)
{
  GstAvdtpSrc *avdtpsrc = GST_AVDTP_SRC (bsrc);

  /* None of this can go into prepare() since we need to set up the
   * connection to figure out what format the device is going to send us.
   */

  if (!gst_avdtp_connection_acquire (&avdtpsrc->conn, FALSE)) {
    GST_ERROR_OBJECT (avdtpsrc, "Failed to acquire connection");
    return FALSE;
  }

  if (!gst_avdtp_connection_get_properties (&avdtpsrc->conn)) {
    GST_ERROR_OBJECT (avdtpsrc, "Failed to get transport properties");
    goto fail;
  }

  if (!gst_avdtp_connection_conf_recv_stream_fd (&avdtpsrc->conn)) {
    GST_ERROR_OBJECT (avdtpsrc, "Failed to configure stream fd");
    goto fail;
  }

  GST_DEBUG_OBJECT (avdtpsrc, "Setting block size to link MTU (%d)",
      avdtpsrc->conn.data.link_mtu);
  gst_base_src_set_blocksize (GST_BASE_SRC (avdtpsrc),
      avdtpsrc->conn.data.link_mtu);

  avdtpsrc->dev_caps = gst_avdtp_connection_get_caps (&avdtpsrc->conn);
  if (!avdtpsrc->dev_caps) {
    GST_ERROR_OBJECT (avdtpsrc, "Failed to get device caps");
    goto fail;
  }

  gst_poll_fd_init (&avdtpsrc->pfd);
  avdtpsrc->pfd.fd = g_io_channel_unix_get_fd (avdtpsrc->conn.stream);

  gst_poll_add_fd (avdtpsrc->poll, &avdtpsrc->pfd);
  gst_poll_fd_ctl_read (avdtpsrc->poll, &avdtpsrc->pfd, TRUE);
  gst_poll_set_flushing (avdtpsrc->poll, FALSE);

  g_atomic_int_set (&avdtpsrc->unlocked, FALSE);

  /* The life time of the connection is shorter than the src object, so we
   * don't need to worry about memory management */
  gst_avdtp_connection_notify_volume (&avdtpsrc->conn, G_OBJECT (avdtpsrc),
      "transport-volume");

  gst_avdtp_src_start_avrcp (avdtpsrc);

  return TRUE;

fail:
  gst_avdtp_connection_release (&avdtpsrc->conn);
  return FALSE;
}

static gboolean
gst_avdtp_src_stop (GstBaseSrc * bsrc)
{
  GstAvdtpSrc *avdtpsrc = GST_AVDTP_SRC (bsrc);

  gst_poll_remove_fd (avdtpsrc->poll, &avdtpsrc->pfd);
  gst_poll_set_flushing (avdtpsrc->poll, TRUE);

  gst_avdtp_src_stop_avrcp (avdtpsrc);
  gst_avdtp_connection_release (&avdtpsrc->conn);

  if (avdtpsrc->dev_caps) {
    gst_caps_unref (avdtpsrc->dev_caps);
    avdtpsrc->dev_caps = NULL;
  }

  return TRUE;
}

static GstFlowReturn
gst_avdtp_src_create (GstBaseSrc * bsrc, guint64 offset, guint length,
    GstBuffer ** outbuf)
{
  GstAvdtpSrc *avdtpsrc = GST_AVDTP_SRC (bsrc);
  GstBuffer *buf = NULL;
  GstMapInfo info;
  int ret;

  if (g_atomic_int_get (&avdtpsrc->unlocked))
    return GST_FLOW_FLUSHING;

  /* We don't operate in GST_FORMAT_BYTES, so offset is ignored */

  while ((ret = gst_poll_wait (avdtpsrc->poll, GST_CLOCK_TIME_NONE))) {
    if (g_atomic_int_get (&avdtpsrc->unlocked))
      /* We're unlocked, time to gtfo */
      return GST_FLOW_FLUSHING;

    if (ret < 0)
      /* Something went wrong */
      goto read_error;

    if (ret > 0)
      /* Got some data */
      break;
  }

  ret = GST_BASE_SRC_CLASS (parent_class)->alloc (bsrc, offset, length, outbuf);
  if (G_UNLIKELY (ret != GST_FLOW_OK))
    goto alloc_failed;

  buf = *outbuf;

  gst_buffer_map (buf, &info, GST_MAP_WRITE);

  ret = read (avdtpsrc->pfd.fd, info.data, length);

  if (ret < 0)
    goto read_error;
  else if (ret == 0) {
    GST_INFO_OBJECT (avdtpsrc, "Got EOF on the transport fd");
    goto eof;
  }

  if (ret < length)
    gst_buffer_set_size (buf, ret);

  GST_LOG_OBJECT (avdtpsrc, "Read %d bytes", ret);

  gst_buffer_unmap (buf, &info);
  *outbuf = buf;

  return GST_FLOW_OK;

alloc_failed:
  {
    GST_DEBUG_OBJECT (bsrc, "alloc failed: %s", gst_flow_get_name (ret));
    return ret;
  }

read_error:
  GST_ERROR_OBJECT (avdtpsrc, "Error while reading audio data: %s",
      strerror (errno));
  gst_buffer_unref (buf);
  return GST_FLOW_ERROR;

eof:
  gst_buffer_unref (buf);
  return GST_FLOW_EOS;
}

static gboolean
gst_avdtp_src_unlock (GstBaseSrc * bsrc)
{
  GstAvdtpSrc *avdtpsrc = GST_AVDTP_SRC (bsrc);

  g_atomic_int_set (&avdtpsrc->unlocked, TRUE);

  gst_poll_set_flushing (avdtpsrc->poll, TRUE);

  return TRUE;
}

static gboolean
gst_avdtp_src_unlock_stop (GstBaseSrc * bsrc)
{
  GstAvdtpSrc *avdtpsrc = GST_AVDTP_SRC (bsrc);

  g_atomic_int_set (&avdtpsrc->unlocked, FALSE);

  gst_poll_set_flushing (avdtpsrc->poll, FALSE);

  /* Flush out any stale data that might be buffered */
  gst_avdtp_connection_conf_recv_stream_fd (&avdtpsrc->conn);

  return TRUE;
}
