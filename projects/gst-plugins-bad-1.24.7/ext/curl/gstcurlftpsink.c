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
 * SECTION:element-curlftpsink
 * @title: curlftpsink
 * @short_description: sink that uploads data to a server using libcurl
 *
 * This is a network sink that uses libcurl as a client to upload data to
 * an FTP server.
 *
 * ## Example launch line
 *
 * Upload a JPEG file to /home/test/images * directory)
 *
 * |[
 * gst-launch-1.0 filesrc location=image.jpg ! jpegparse ! curlftpsink  \
 *     file-name=image.jpg  \
 *     location=ftp://192.168.0.1/images/
 * ]|
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <curl/curl.h>
#include <string.h>
#include <stdio.h>

#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#include <sys/types.h>
#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#include <unistd.h>
#if HAVE_NETINET_IP_H
#include <netinet/ip.h>
#endif
#if HAVE_NETINET_TCP_H
#include <netinet/tcp.h>
#endif
#include <sys/stat.h>
#include <fcntl.h>

#include "gstcurlelements.h"
#include "gstcurltlssink.h"
#include "gstcurlftpsink.h"

/* Default values */
#define GST_CAT_DEFAULT                gst_curl_ftp_sink_debug
#define RENAME_TO                          "RNTO "
#define RENAME_FROM			   "RNFR "

/* Plugin specific settings */

GST_DEBUG_CATEGORY_STATIC (gst_curl_ftp_sink_debug);

enum
{
  PROP_0,
  PROP_FTP_PORT_ARG,
  PROP_EPSV_MODE,
  PROP_CREATE_TEMP_FILE,
  PROP_CREATE_TEMP_FILE_NAME,
  PROP_CREATE_DIRS
};


/* Object class function declarations */


/* private functions */
static void gst_curl_ftp_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_curl_ftp_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_curl_ftp_sink_finalize (GObject * gobject);

static gboolean set_ftp_options_unlocked (GstCurlBaseSink * curlbasesink);
static gboolean set_ftp_dynamic_options_unlocked
    (GstCurlBaseSink * curlbasesink);

#define gst_curl_ftp_sink_parent_class parent_class
G_DEFINE_TYPE (GstCurlFtpSink, gst_curl_ftp_sink, GST_TYPE_CURL_TLS_SINK);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (curlftpsink, "curlftpsink",
    GST_RANK_NONE, GST_TYPE_CURL_FTP_SINK, curl_element_init (plugin));

static void
gst_curl_ftp_sink_class_init (GstCurlFtpSinkClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstCurlBaseSinkClass *gstcurlbasesink_class = (GstCurlBaseSinkClass *) klass;
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  GST_DEBUG_CATEGORY_INIT (gst_curl_ftp_sink_debug, "curlftpsink", 0,
      "curl ftp sink element");

  gst_element_class_set_static_metadata (element_class,
      "Curl ftp sink",
      "Sink/Network",
      "Upload data over FTP protocol using libcurl",
      "Patricia Muscalu <patricia@axis.com>");

  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_curl_ftp_sink_finalize);

  gobject_class->set_property = gst_curl_ftp_sink_set_property;
  gobject_class->get_property = gst_curl_ftp_sink_get_property;

  gstcurlbasesink_class->set_protocol_dynamic_options_unlocked =
      set_ftp_dynamic_options_unlocked;
  gstcurlbasesink_class->set_options_unlocked = set_ftp_options_unlocked;

  g_object_class_install_property (gobject_class, PROP_FTP_PORT_ARG,
      g_param_spec_string ("ftp-port", "IP address for FTP PORT instruction",
          "The PORT instruction tells the remote server to connect to"
          " the IP address", "", G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_EPSV_MODE,
      g_param_spec_boolean ("epsv-mode", "Extended passive mode",
          "Enable the use of the EPSV command when doing passive FTP transfers",
          TRUE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_CREATE_TEMP_FILE,
      g_param_spec_boolean ("create-tmp-file", "Enable or disable temporary file transfer", "Use a temporary file name when uploading a a file. When the transfer is complete,  \
          this temporary file is renamed to the final file name. This is useful for ensuring \
          that remote systems do not read a partially uploaded file", FALSE, G_PARAM_READWRITE |
          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_CREATE_TEMP_FILE_NAME,
      g_param_spec_string ("temp-file-name",
          "Creates a temporary file name with date and time",
          "Filename pattern to use when generating a temporary filename for uploads",
          NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_CREATE_DIRS,
      g_param_spec_boolean ("create-dirs", "Create missing directories",
          "Attempt to create missing directory included in the path", FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
gst_curl_ftp_sink_init (GstCurlFtpSink * sink)
{
}

static void
gst_curl_ftp_sink_finalize (GObject * gobject)
{
  GstCurlFtpSink *this = GST_CURL_FTP_SINK (gobject);

  GST_DEBUG ("finalizing curlftpsink");
  g_free (this->ftp_port_arg);
  g_free (this->tmpfile_name);

  if (this->headerlist) {
    curl_slist_free_all (this->headerlist);
    this->headerlist = NULL;
  }

  G_OBJECT_CLASS (parent_class)->finalize (gobject);
}

static gboolean
set_ftp_dynamic_options_unlocked (GstCurlBaseSink * basesink)
{
  gchar *tmp = NULL;
  GstCurlFtpSink *sink = GST_CURL_FTP_SINK (basesink);
  CURLcode res;

  if (sink->tmpfile_create) {
    gchar *rename_from = NULL;
    gchar *rename_to = NULL;
    gchar *uploadfile_as = NULL;
    gchar *last_slash = NULL;
    gchar *tmpfile_name = NULL;

    if (sink->headerlist != NULL) {
      curl_slist_free_all (sink->headerlist);
      sink->headerlist = NULL;
    }

    if (sink->tmpfile_name != NULL) {
      tmpfile_name = g_strdup_printf ("%s", sink->tmpfile_name);
    } else {
      tmpfile_name =
          g_strdup_printf (".tmp.%04X%04X", g_random_int (), g_random_int ());
    }

    rename_from = g_strdup_printf ("%s%s", RENAME_FROM, tmpfile_name);

    last_slash = strrchr (basesink->file_name, '/');
    if (last_slash != NULL) {
      gchar *dir_name =
          g_strndup (basesink->file_name, last_slash - basesink->file_name);
      rename_to = g_strdup_printf ("%s%s", RENAME_TO, last_slash + 1);
      uploadfile_as = g_strdup_printf ("%s/%s", dir_name, tmpfile_name);
      g_free (dir_name);
    } else {
      rename_to = g_strdup_printf ("%s%s", RENAME_TO, basesink->file_name);
      uploadfile_as = g_strdup_printf ("%s", tmpfile_name);
    }
    g_free (tmpfile_name);

    tmp = g_strdup_printf ("%s%s", basesink->url, uploadfile_as);
    g_free (uploadfile_as);

    sink->headerlist = curl_slist_append (sink->headerlist, rename_from);
    sink->headerlist = curl_slist_append (sink->headerlist, rename_to);
    g_free (rename_from);
    g_free (rename_to);

    res = curl_easy_setopt (basesink->curl, CURLOPT_URL, tmp);
    g_free (tmp);
    if (res != CURLE_OK) {
      basesink->error = g_strdup_printf ("failed to set URL: %s",
          curl_easy_strerror (res));
      return FALSE;
    }

    res =
        curl_easy_setopt (basesink->curl, CURLOPT_POSTQUOTE, sink->headerlist);
    if (res != CURLE_OK) {
      basesink->error = g_strdup_printf ("failed to set post quote: %s",
          curl_easy_strerror (res));
      return FALSE;
    }

    if (last_slash != NULL) {
      *last_slash = '\0';
    }
  } else {
    tmp = g_strdup_printf ("%s%s", basesink->url, basesink->file_name);
    res = curl_easy_setopt (basesink->curl, CURLOPT_URL, tmp);
    g_free (tmp);
    if (res != CURLE_OK) {
      basesink->error = g_strdup_printf ("failed to set URL: %s",
          curl_easy_strerror (res));
      return FALSE;
    }
  }

  return TRUE;
}

static gboolean
set_ftp_options_unlocked (GstCurlBaseSink * basesink)
{
  GstCurlFtpSink *sink = GST_CURL_FTP_SINK (basesink);
  CURLcode res;

  res = curl_easy_setopt (basesink->curl, CURLOPT_UPLOAD, 1L);
  if (res != CURLE_OK) {
    basesink->error = g_strdup_printf ("failed to prepare for upload: %s",
        curl_easy_strerror (res));
    return FALSE;
  }

  if (sink->ftp_port_arg != NULL && (strlen (sink->ftp_port_arg) > 0)) {
    /* Connect data stream actively. */
    res = curl_easy_setopt (basesink->curl, CURLOPT_FTPPORT,
        sink->ftp_port_arg);

    if (res != CURLE_OK) {
      basesink->error = g_strdup_printf ("failed to set up active mode: %s",
          curl_easy_strerror (res));
      return FALSE;
    }
  } else {
    /* Connect data stream passively.
     * libcurl will always attempt to use EPSV before PASV.
     */
    if (!sink->epsv_mode) {
      /* send only plain PASV command */
      res = curl_easy_setopt (basesink->curl, CURLOPT_FTP_USE_EPSV, 0);
      if (res != CURLE_OK) {
        basesink->error =
            g_strdup_printf ("failed to set extended passive mode: %s",
            curl_easy_strerror (res));
        return FALSE;
      }
    }
  }

  if (sink->create_dirs) {
    res = curl_easy_setopt (basesink->curl, CURLOPT_FTP_CREATE_MISSING_DIRS,
        1L);
    if (res != CURLE_OK) {
      basesink->error =
          g_strdup_printf ("failed to set create missing dirs: %s",
          curl_easy_strerror (res));
      return FALSE;
    }
  }

  return TRUE;
}

static void
gst_curl_ftp_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstCurlFtpSink *sink;
  GstState cur_state;

  g_return_if_fail (GST_IS_CURL_FTP_SINK (object));
  sink = GST_CURL_FTP_SINK (object);

  gst_element_get_state (GST_ELEMENT (sink), &cur_state, NULL, 0);
  if (cur_state != GST_STATE_PLAYING && cur_state != GST_STATE_PAUSED) {
    GST_OBJECT_LOCK (sink);

    switch (prop_id) {
      case PROP_FTP_PORT_ARG:
        g_free (sink->ftp_port_arg);
        sink->ftp_port_arg = g_value_dup_string (value);
        GST_DEBUG_OBJECT (sink, "ftp-port set to %s", sink->ftp_port_arg);
        break;
      case PROP_EPSV_MODE:
        sink->epsv_mode = g_value_get_boolean (value);
        GST_DEBUG_OBJECT (sink, "epsv-mode set to %d", sink->epsv_mode);
        break;
      case PROP_CREATE_TEMP_FILE:
        sink->tmpfile_create = g_value_get_boolean (value);
        GST_DEBUG_OBJECT (sink, "create-tmp-file set to %d",
            sink->tmpfile_create);
        break;
      case PROP_CREATE_TEMP_FILE_NAME:
        g_free (sink->tmpfile_name);
        sink->tmpfile_name = g_value_dup_string (value);
        GST_DEBUG_OBJECT (sink, "tmp-file-name set to %s", sink->tmpfile_name);
        break;
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
gst_curl_ftp_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstCurlFtpSink *sink;

  g_return_if_fail (GST_IS_CURL_FTP_SINK (object));
  sink = GST_CURL_FTP_SINK (object);

  switch (prop_id) {
    case PROP_FTP_PORT_ARG:
      g_value_set_string (value, sink->ftp_port_arg);
      break;
    case PROP_EPSV_MODE:
      g_value_set_boolean (value, sink->epsv_mode);
      break;
    case PROP_CREATE_TEMP_FILE:
      g_value_set_boolean (value, sink->tmpfile_create);
      break;
    case PROP_CREATE_TEMP_FILE_NAME:
      g_value_set_string (value, sink->tmpfile_name);
      break;
    case PROP_CREATE_DIRS:
      g_value_set_boolean (value, sink->create_dirs);
      break;
    default:
      GST_DEBUG_OBJECT (sink, "invalid property id");
      break;
  }
}
