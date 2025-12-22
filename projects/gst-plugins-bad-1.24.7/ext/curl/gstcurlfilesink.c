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
 * SECTION:element-curlfilesink
 * @title: curlfilesink
 * @short_description: sink that uploads data to a server using libcurl
 *
 * This is a network sink that uses libcurl as a client to upload data to
 * a local or network drive.
 *
 * ## Example launch line (upload a JPEG file to /home/test/images directory)
 * |[
 * gst-launch-1.0 filesrc location=image.jpg ! jpegparse ! curlfilesink  \
 *     file-name=image.jpg  \
 *     location=file:///home/test/images/
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
#include "gstcurlbasesink.h"
#include "gstcurlfilesink.h"

/* Default values */
#define GST_CAT_DEFAULT                gst_curl_file_sink_debug


/* Plugin specific settings */

GST_DEBUG_CATEGORY_STATIC (gst_curl_file_sink_debug);

enum
{
  PROP_0,
  PROP_CREATE_DIRS
};


/* Object class function declarations */


/* private functions */
static void gst_curl_file_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_curl_file_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean set_file_options_unlocked (GstCurlBaseSink * curlbasesink);
static gboolean set_file_dynamic_options_unlocked
    (GstCurlBaseSink * curlbasesink);
static gboolean gst_curl_file_sink_prepare_transfer (GstCurlBaseSink *
    curlbasesink);

#define gst_curl_file_sink_parent_class parent_class
G_DEFINE_TYPE (GstCurlFileSink, gst_curl_file_sink, GST_TYPE_CURL_BASE_SINK);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (curlfilesink, "curlfilesink",
    GST_RANK_NONE, GST_TYPE_CURL_FILE_SINK, curl_element_init (plugin));

static void
gst_curl_file_sink_class_init (GstCurlFileSinkClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstCurlBaseSinkClass *gstcurlbasesink_class = (GstCurlBaseSinkClass *) klass;
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  GST_DEBUG_CATEGORY_INIT (gst_curl_file_sink_debug, "curlfilesink", 0,
      "curl file sink element");

  gst_element_class_set_static_metadata (element_class,
      "Curl file sink",
      "Sink/Network",
      "Upload data over FILE protocol using libcurl",
      "Patricia Muscalu <patricia@axis.com>");

  gobject_class->set_property = gst_curl_file_sink_set_property;
  gobject_class->get_property = gst_curl_file_sink_get_property;

  gstcurlbasesink_class->set_protocol_dynamic_options_unlocked =
      set_file_dynamic_options_unlocked;
  gstcurlbasesink_class->set_options_unlocked = set_file_options_unlocked;
  gstcurlbasesink_class->prepare_transfer = gst_curl_file_sink_prepare_transfer;

  g_object_class_install_property (gobject_class, PROP_CREATE_DIRS,
      g_param_spec_boolean ("create-dirs", "Create missing directories",
          "Attempt to create missing directory included in the path",
          FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
gst_curl_file_sink_init (GstCurlFileSink * sink)
{
}

static void
gst_curl_file_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstCurlFileSink *sink;
  GstState cur_state;

  g_return_if_fail (GST_IS_CURL_FILE_SINK (object));
  sink = GST_CURL_FILE_SINK (object);

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
gst_curl_file_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstCurlFileSink *sink;

  g_return_if_fail (GST_IS_CURL_FILE_SINK (object));
  sink = GST_CURL_FILE_SINK (object);

  switch (prop_id) {
    case PROP_CREATE_DIRS:
      g_value_set_boolean (value, sink->create_dirs);
      break;
    default:
      GST_DEBUG_OBJECT (sink, "invalid property id");
      break;
  }
}

static gboolean
set_file_dynamic_options_unlocked (GstCurlBaseSink * basesink)
{
  gchar *tmp = g_strdup_printf ("%s%s", basesink->url, basesink->file_name);
  CURLcode res;

  res = curl_easy_setopt (basesink->curl, CURLOPT_URL, tmp);
  g_free (tmp);
  if (res != CURLE_OK) {
    basesink->error = g_strdup_printf ("failed to set URL: %s",
        curl_easy_strerror (res));
    return FALSE;
  }

  return TRUE;
}

static gboolean
set_file_options_unlocked (GstCurlBaseSink * basesink)
{
  CURLcode res;

  res = curl_easy_setopt (basesink->curl, CURLOPT_UPLOAD, 1L);
  if (res != CURLE_OK) {
    basesink->error = g_strdup_printf ("failed to prepare for upload: %s",
        curl_easy_strerror (res));
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_curl_file_sink_prepare_transfer (GstCurlBaseSink * basesink)
{
  GstCurlFileSink *sink = GST_CURL_FILE_SINK (basesink);

  if (sink->create_dirs) {
    gchar *file_name;
    gchar *last_slash;

    gchar *url = g_strdup_printf ("%s%s", basesink->url, basesink->file_name);
    file_name = g_filename_from_uri (url, NULL, NULL);
    if (file_name == NULL) {
      basesink->error = g_strdup_printf ("failed to parse file name '%s'", url);
      g_free (url);
      return FALSE;
    }
    g_free (url);

    last_slash = strrchr (file_name, G_DIR_SEPARATOR);
    if (last_slash != NULL) {
      /* create dir if file name contains dir component */
      gchar *dir_name = g_strndup (file_name, last_slash - file_name);
      if (g_mkdir_with_parents (dir_name, S_IRWXU) < 0) {
        basesink->error = g_strdup_printf ("failed to create directory '%s'",
            dir_name);
        g_free (file_name);
        g_free (dir_name);
        return FALSE;
      }
      g_free (dir_name);
    }
    g_free (file_name);
  }

  return TRUE;
}
