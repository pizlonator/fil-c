/* GStreamer
 * Copyright (C) 2011 Axis Communications <dev-gstreamer@axis.com>
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

/**
 * SECTION:element-curlsftpsink
 * @title: curlsftpsink
 * @short_description: sink that uploads data to a server using libcurl
 *
 * This is a network sink that uses libcurl as a client to upload data to
 * a SFTP (SSH File Transfer Protocol) server.
 *
 * ## Example launch line
 *
 * Upload a file to /home/john/sftp_tests/
 *
 * |[
 * gst-launch-1.0 filesrc location=/home/jdoe/some.file ! curlsftpsink  \
 *     file-name=some.file.backup  \
 *     user=john location=sftp://192.168.0.1/~/sftp_tests/  \
 *     ssh-auth-type=1 ssh-key-passphrase=blabla  \
 *     ssh-pub-keyfile=/home/jdoe/.ssh/id_rsa.pub  \
 *     ssh-priv-keyfile=/home/jdoe/.ssh/id_rsa  \
 *     create-dirs=TRUE
 * ]|
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstcurlelements.h"
#include "gstcurlsshsink.h"
#include "gstcurlsftpsink.h"

#include <curl/curl.h>
#include <string.h>
#include <stdio.h>

#ifdef G_OS_WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#endif
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

/* Default values */
#define GST_CAT_DEFAULT    gst_curl_sftp_sink_debug


/* Plugin specific settings */

GST_DEBUG_CATEGORY_STATIC (gst_curl_sftp_sink_debug);

enum
{
  PROP_0,
  PROP_CREATE_DIRS
};

/* private functions */
static void gst_curl_sftp_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_curl_sftp_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_curl_sftp_sink_finalize (GObject * gobject);

static gboolean set_sftp_options_unlocked (GstCurlBaseSink * curlbasesink);
static gboolean set_sftp_dynamic_options_unlocked (GstCurlBaseSink *
    curlbasesink);


#define gst_curl_sftp_sink_parent_class parent_class
G_DEFINE_TYPE (GstCurlSftpSink, gst_curl_sftp_sink, GST_TYPE_CURL_SSH_SINK);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (curlsftpsink, "curlsftpsink",
    GST_RANK_NONE, GST_TYPE_CURL_SFTP_SINK, curl_element_init (plugin));

static void
gst_curl_sftp_sink_class_init (GstCurlSftpSinkClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstCurlBaseSinkClass *gstcurlbasesink_class = (GstCurlBaseSinkClass *) klass;
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  GST_DEBUG_CATEGORY_INIT (gst_curl_sftp_sink_debug, "curlsftpsink", 0,
      "curl sftp sink element");

  GST_DEBUG ("class_init");

  gst_element_class_set_static_metadata (element_class,
      "Curl sftp sink",
      "Sink/Network",
      "Upload data over the SFTP protocol using libcurl",
      "Sorin L. <sorin@axis.com>");

  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_curl_sftp_sink_finalize);

  gobject_class->set_property = gst_curl_sftp_sink_set_property;
  gobject_class->get_property = gst_curl_sftp_sink_get_property;

  gstcurlbasesink_class->set_protocol_dynamic_options_unlocked =
      set_sftp_dynamic_options_unlocked;
  gstcurlbasesink_class->set_options_unlocked = set_sftp_options_unlocked;

  g_object_class_install_property (gobject_class, PROP_CREATE_DIRS,
      g_param_spec_boolean ("create-dirs", "Create missing directories",
          "Attempt to create missing directories",
          FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
gst_curl_sftp_sink_init (GstCurlSftpSink * sink)
{
}

static void
gst_curl_sftp_sink_finalize (GObject * gobject)
{
  GST_DEBUG ("finalizing curlsftpsink");
  G_OBJECT_CLASS (parent_class)->finalize (gobject);
}

static gboolean
set_sftp_dynamic_options_unlocked (GstCurlBaseSink * basesink)
{
  gchar *tmp = g_strdup_printf ("%s%s", basesink->url, basesink->file_name);
  CURLcode curl_err = CURLE_OK;

  curl_err = curl_easy_setopt (basesink->curl, CURLOPT_URL, tmp);
  g_free (tmp);
  if (curl_err != CURLE_OK) {
    basesink->error = g_strdup_printf ("failed to set URL: %s",
        curl_easy_strerror (curl_err));
    return FALSE;
  }

  return TRUE;
}

static gboolean
set_sftp_options_unlocked (GstCurlBaseSink * basesink)
{
  GstCurlSftpSink *sink = GST_CURL_SFTP_SINK (basesink);
  GstCurlSshSinkClass *parent_class;
  CURLcode curl_err = CURLE_OK;

  if ((curl_err =
          curl_easy_setopt (basesink->curl, CURLOPT_UPLOAD, 1L)) != CURLE_OK) {
    basesink->error = g_strdup_printf ("failed to prepare for upload: %s",
        curl_easy_strerror (curl_err));
    return FALSE;
  }

  if (sink->create_dirs) {
    if ((curl_err = curl_easy_setopt (basesink->curl,
                CURLOPT_FTP_CREATE_MISSING_DIRS, 1L)) != CURLE_OK) {
      basesink->error =
          g_strdup_printf ("failed to set create missing dirs: %s",
          curl_easy_strerror (curl_err));
      return FALSE;
    }
  }

  parent_class = GST_CURL_SSH_SINK_GET_CLASS (sink);

  /* chain to parent as well */
  return parent_class->set_options_unlocked (basesink);
}

static void
gst_curl_sftp_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstCurlSftpSink *sink;
  GstState cur_state;

  g_return_if_fail (GST_IS_CURL_SFTP_SINK (object));
  sink = GST_CURL_SFTP_SINK (object);

  gst_element_get_state (GST_ELEMENT (sink), &cur_state, NULL, 0);
  if (cur_state != GST_STATE_PLAYING && cur_state != GST_STATE_PAUSED) {
    GST_OBJECT_LOCK (sink);

    switch (prop_id) {
      case PROP_CREATE_DIRS:
        sink->create_dirs = g_value_get_boolean (value);
        GST_DEBUG_OBJECT (sink, "create-dirs set to %d", sink->create_dirs);
        break;

      default:
        GST_DEBUG_OBJECT (sink, "invalid property id %d", prop_id);
        break;
    }

    GST_OBJECT_UNLOCK (sink);
  }
}

static void
gst_curl_sftp_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstCurlSftpSink *sink;

  g_return_if_fail (GST_IS_CURL_SFTP_SINK (object));
  sink = GST_CURL_SFTP_SINK (object);

  switch (prop_id) {
    case PROP_CREATE_DIRS:
      g_value_set_boolean (value, sink->create_dirs);
      break;

    default:
      GST_DEBUG_OBJECT (sink, "invalid property id");
      break;
  }
}
