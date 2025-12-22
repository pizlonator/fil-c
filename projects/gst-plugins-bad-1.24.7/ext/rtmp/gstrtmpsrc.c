/* GStreamer
 * Copyright (C) 1999,2000 Erik Walthinsen <omega@cse.ogi.edu>
 *                    2000 Wim Taymans <wtay@chello.be>
 *                    2002 Kristian Rietveld <kris@gtk.org>
 *                    2002,2003 Colin Walters <walters@gnu.org>
 *                    2001,2010 Bastien Nocera <hadess@hadess.net>
 *                    2010 Sebastian Dröge <sebastian.droege@collabora.co.uk>
 *
 * rtmpsrc.c:
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
 * SECTION:element-rtmpsrc
 * @title: rtmpsrc
 *
 * This plugin reads data from a local or remote location specified
 * by an URI. This location can be specified using any protocol supported by
 * the RTMP library, i.e. rtmp, rtmpt, rtmps, rtmpe, rtmfp, rtmpte and rtmpts.
 * The URL/location can contain extra connection or session parameters
 * for librtmp, such as 'flashver=version'. See the librtmp documentation
 * for more detail. Of particular interest can be setting `live=1` to certain
 * RTMP streams that don't seem to be playing otherwise.

 *
 * ## Example launch lines
 * |[
 * gst-launch-1.0 -v rtmpsrc location=rtmp://somehost/someurl ! fakesink
 * ]| Open an RTMP location and pass its content to fakesink.
 * 
 * |[
 * gst-launch-1.0 rtmpsrc location="rtmp://somehost/someurl live=1" ! fakesink
 * ]| Open an RTMP location and pass its content to fakesink while passing the
 * live=1 flag to librtmp
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib/gi18n-lib.h>

#include "gstrtmpelements.h"
#include "gstrtmpsrc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gst/gst.h>

#ifdef G_OS_WIN32
#include <winsock2.h>
#endif

GST_DEBUG_CATEGORY_STATIC (rtmpsrc_debug);
#define GST_CAT_DEFAULT rtmpsrc_debug

static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

enum
{
  PROP_0,
  PROP_LOCATION,
  PROP_TIMEOUT
#if 0
      PROP_SWF_URL,
  PROP_PAGE_URL
#endif
};

#define DEFAULT_LOCATION NULL
#define DEFAULT_TIMEOUT 120

static void gst_rtmp_src_uri_handler_init (gpointer g_iface,
    gpointer iface_data);

static void gst_rtmp_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_rtmp_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_rtmp_src_finalize (GObject * object);

static gboolean gst_rtmp_src_connect (GstRTMPSrc * src);
static gboolean gst_rtmp_src_unlock (GstBaseSrc * src);
static gboolean gst_rtmp_src_stop (GstBaseSrc * src);
static gboolean gst_rtmp_src_start (GstBaseSrc * src);
static gboolean gst_rtmp_src_is_seekable (GstBaseSrc * src);
static gboolean gst_rtmp_src_prepare_seek_segment (GstBaseSrc * src,
    GstEvent * event, GstSegment * segment);
static gboolean gst_rtmp_src_do_seek (GstBaseSrc * src, GstSegment * segment);
static GstFlowReturn gst_rtmp_src_create (GstPushSrc * pushsrc,
    GstBuffer ** buffer);
static gboolean gst_rtmp_src_query (GstBaseSrc * src, GstQuery * query);

#define gst_rtmp_src_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstRTMPSrc, gst_rtmp_src, GST_TYPE_PUSH_SRC,
    G_IMPLEMENT_INTERFACE (GST_TYPE_URI_HANDLER,
        gst_rtmp_src_uri_handler_init));
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (rtmpsrc, "rtmpsrc", GST_RANK_PRIMARY,
    GST_TYPE_RTMP_SRC, rtmp_element_init (plugin));

static void
gst_rtmp_src_class_init (GstRTMPSrcClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseSrcClass *gstbasesrc_class;
  GstPushSrcClass *gstpushsrc_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gstelement_class = GST_ELEMENT_CLASS (klass);
  gstbasesrc_class = GST_BASE_SRC_CLASS (klass);
  gstpushsrc_class = GST_PUSH_SRC_CLASS (klass);

  gobject_class->finalize = gst_rtmp_src_finalize;
  gobject_class->set_property = gst_rtmp_src_set_property;
  gobject_class->get_property = gst_rtmp_src_get_property;

  /* properties */
  g_object_class_install_property (gobject_class, PROP_LOCATION,
      g_param_spec_string ("location", "RTMP Location",
          "Location of the RTMP url to read",
          DEFAULT_LOCATION, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_TIMEOUT,
      g_param_spec_int ("timeout", "RTMP Timeout",
          "Time without receiving any data from the server to wait before to timeout the session",
          0, G_MAXINT,
          DEFAULT_TIMEOUT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_add_static_pad_template (gstelement_class, &srctemplate);

  gst_element_class_set_static_metadata (gstelement_class,
      "RTMP Source",
      "Source/File",
      "Read RTMP streams",
      "Bastien Nocera <hadess@hadess.net>, "
      "Sebastian Dröge <sebastian.droege@collabora.co.uk>");

  gstbasesrc_class->start = GST_DEBUG_FUNCPTR (gst_rtmp_src_start);
  gstbasesrc_class->stop = GST_DEBUG_FUNCPTR (gst_rtmp_src_stop);
  gstbasesrc_class->unlock = GST_DEBUG_FUNCPTR (gst_rtmp_src_unlock);
  gstbasesrc_class->is_seekable = GST_DEBUG_FUNCPTR (gst_rtmp_src_is_seekable);
  gstbasesrc_class->prepare_seek_segment =
      GST_DEBUG_FUNCPTR (gst_rtmp_src_prepare_seek_segment);
  gstbasesrc_class->do_seek = GST_DEBUG_FUNCPTR (gst_rtmp_src_do_seek);
  gstpushsrc_class->create = GST_DEBUG_FUNCPTR (gst_rtmp_src_create);
  gstbasesrc_class->query = GST_DEBUG_FUNCPTR (gst_rtmp_src_query);

  GST_DEBUG_CATEGORY_INIT (rtmpsrc_debug, "rtmpsrc", 0, "RTMP Source");
}

static void
gst_rtmp_src_init (GstRTMPSrc * rtmpsrc)
{
#ifdef G_OS_WIN32
  WSADATA wsa_data;

  if (WSAStartup (MAKEWORD (2, 2), &wsa_data) != 0) {
    GST_ERROR_OBJECT (rtmpsrc, "WSAStartup failed: 0x%08x", WSAGetLastError ());
  }
#endif

  rtmpsrc->cur_offset = 0;
  rtmpsrc->last_timestamp = 0;
  rtmpsrc->timeout = DEFAULT_TIMEOUT;

  gst_base_src_set_format (GST_BASE_SRC (rtmpsrc), GST_FORMAT_TIME);
}

static void
gst_rtmp_src_finalize (GObject * object)
{
  GstRTMPSrc *rtmpsrc = GST_RTMP_SRC (object);

  g_free (rtmpsrc->uri);
  rtmpsrc->uri = NULL;

#ifdef G_OS_WIN32
  WSACleanup ();
#endif

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

/*
 * URI interface support.
 */

static GstURIType
gst_rtmp_src_uri_get_type (GType type)
{
  return GST_URI_SRC;
}

static const gchar *const *
gst_rtmp_src_uri_get_protocols (GType type)
{
  static const gchar *protocols[] =
      { "rtmp", "rtmpt", "rtmps", "rtmpe", "rtmfp", "rtmpte", "rtmpts", NULL };

  return protocols;
}

static gchar *
gst_rtmp_src_uri_get_uri (GstURIHandler * handler)
{
  GstRTMPSrc *src = GST_RTMP_SRC (handler);

  /* FIXME: make thread-safe */
  return g_strdup (src->uri);
}

static gboolean
gst_rtmp_src_uri_set_uri (GstURIHandler * handler, const gchar * uri,
    GError ** error)
{
  GstRTMPSrc *src = GST_RTMP_SRC (handler);

  if (GST_STATE (src) >= GST_STATE_PAUSED) {
    g_set_error (error, GST_URI_ERROR, GST_URI_ERROR_BAD_STATE,
        "Changing the URI on rtmpsrc when it is running is not supported");
    return FALSE;
  }

  g_free (src->uri);
  src->uri = NULL;

  if (uri != NULL) {
    int protocol;
    AVal host;
    unsigned int port;
    AVal playpath, app;

    if (!RTMP_ParseURL (uri, &protocol, &host, &port, &playpath, &app) ||
        !host.av_len || !playpath.av_len) {
      GST_ERROR_OBJECT (src, "Failed to parse URI %s", uri);
      g_set_error (error, GST_URI_ERROR, GST_URI_ERROR_BAD_URI,
          "Could not parse RTMP URI");
      /* FIXME: we should not be freeing RTMP internals to avoid leaking */
      free (playpath.av_val);
      return FALSE;
    }
    free (playpath.av_val);
    src->uri = g_strdup (uri);
  }

  GST_DEBUG_OBJECT (src, "Changed URI to %s", GST_STR_NULL (uri));

  return TRUE;
}

static void
gst_rtmp_src_uri_handler_init (gpointer g_iface, gpointer iface_data)
{
  GstURIHandlerInterface *iface = (GstURIHandlerInterface *) g_iface;

  iface->get_type = gst_rtmp_src_uri_get_type;
  iface->get_protocols = gst_rtmp_src_uri_get_protocols;
  iface->get_uri = gst_rtmp_src_uri_get_uri;
  iface->set_uri = gst_rtmp_src_uri_set_uri;
}

static void
gst_rtmp_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstRTMPSrc *src;

  src = GST_RTMP_SRC (object);

  switch (prop_id) {
    case PROP_LOCATION:{
      gst_rtmp_src_uri_set_uri (GST_URI_HANDLER (src),
          g_value_get_string (value), NULL);
      break;
    }
    case PROP_TIMEOUT:{
      src->timeout = g_value_get_int (value);
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_rtmp_src_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstRTMPSrc *src;

  src = GST_RTMP_SRC (object);

  switch (prop_id) {
    case PROP_LOCATION:
      g_value_set_string (value, src->uri);
      break;
    case PROP_TIMEOUT:
      g_value_set_int (value, src->timeout);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/*
 * Read a new buffer from src->reqoffset, takes care of events
 * and seeking and such.
 */
static GstFlowReturn
gst_rtmp_src_create (GstPushSrc * pushsrc, GstBuffer ** buffer)
{
  GstRTMPSrc *src;
  GstBuffer *buf;
  GstMapInfo map;
  guint8 *data;
  guint todo;
  gsize bsize;
  int size;

  src = GST_RTMP_SRC (pushsrc);

  g_return_val_if_fail (src->rtmp != NULL, GST_FLOW_ERROR);

  if (!RTMP_IsConnected (src->rtmp)) {
    GST_DEBUG_OBJECT (src, "reconnecting");
    if (!gst_rtmp_src_connect (src))
      return GST_FLOW_ERROR;
  }

  size = GST_BASE_SRC_CAST (pushsrc)->blocksize;

  GST_DEBUG ("reading from %" G_GUINT64_FORMAT
      ", size %u", src->cur_offset, size);

  buf = gst_buffer_new_allocate (NULL, size, NULL);
  if (G_UNLIKELY (buf == NULL)) {
    GST_ERROR_OBJECT (src, "Failed to allocate %u bytes", size);
    return GST_FLOW_ERROR;
  }

  todo = size;
  gst_buffer_map (buf, &map, GST_MAP_WRITE);
  data = map.data;
  bsize = 0;

  while (todo > 0) {
    int read = RTMP_Read (src->rtmp, (char *) data, todo);

    if (G_UNLIKELY (read == 0 && todo == size))
      goto eos;

    if (G_UNLIKELY (read == 0))
      break;

    if (G_UNLIKELY (read < 0))
      goto read_failed;

    if (read < todo) {
      data += read;
      todo -= read;
      bsize += read;
    } else {
      bsize += todo;
      todo = 0;
    }
    GST_LOG ("  got size %d", read);
  }
  gst_buffer_unmap (buf, &map);
  gst_buffer_resize (buf, 0, bsize);

  if (src->discont) {
    GST_BUFFER_FLAG_SET (buf, GST_BUFFER_FLAG_DISCONT);
    src->discont = FALSE;
  }

  GST_BUFFER_TIMESTAMP (buf) = src->last_timestamp;
  GST_BUFFER_OFFSET (buf) = src->cur_offset;

  src->cur_offset += size;
  if (src->last_timestamp == GST_CLOCK_TIME_NONE)
    src->last_timestamp = src->rtmp->m_mediaStamp * GST_MSECOND;
  else
    src->last_timestamp =
        MAX (src->last_timestamp, src->rtmp->m_mediaStamp * GST_MSECOND);

  GST_LOG_OBJECT (src, "Created buffer of size %u at %" G_GINT64_FORMAT
      " with timestamp %" GST_TIME_FORMAT, size, GST_BUFFER_OFFSET (buf),
      GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buf)));


  /* we're done, return the buffer */
  *buffer = buf;

  return GST_FLOW_OK;

read_failed:
  {
    gst_buffer_unmap (buf, &map);
    gst_buffer_unref (buf);
    GST_ELEMENT_ERROR (src, RESOURCE, READ, (NULL), ("Failed to read data"));
    return GST_FLOW_ERROR;
  }
eos:
  {
    gst_buffer_unmap (buf, &map);
    gst_buffer_unref (buf);
    if (src->cur_offset == 0) {
      GST_ELEMENT_ERROR (src, RESOURCE, READ, (NULL),
          ("Failed to read any data from stream, check your URL"));
      return GST_FLOW_ERROR;
    } else {
      GST_DEBUG_OBJECT (src, "Reading data gave EOS");
      return GST_FLOW_EOS;
    }
  }
}

static gboolean
gst_rtmp_src_query (GstBaseSrc * basesrc, GstQuery * query)
{
  gboolean ret = FALSE;
  GstRTMPSrc *src = GST_RTMP_SRC (basesrc);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_URI:
      gst_query_set_uri (query, src->uri);
      ret = TRUE;
      break;
    case GST_QUERY_POSITION:{
      GstFormat format;

      gst_query_parse_position (query, &format, NULL);
      if (format == GST_FORMAT_TIME) {
        gst_query_set_position (query, format, src->last_timestamp);
        ret = TRUE;
      }
      break;
    }
    case GST_QUERY_DURATION:{
      GstFormat format;
      gdouble duration;

      gst_query_parse_duration (query, &format, NULL);
      if (format == GST_FORMAT_TIME && src->rtmp) {
        duration = RTMP_GetDuration (src->rtmp);
        if (duration != 0.0) {
          gst_query_set_duration (query, format, duration * GST_SECOND);
          ret = TRUE;
        }
      }
      break;
    }
    case GST_QUERY_SCHEDULING:{
      gst_query_set_scheduling (query,
          GST_SCHEDULING_FLAG_SEQUENTIAL |
          GST_SCHEDULING_FLAG_BANDWIDTH_LIMITED, 1, -1, 0);
      gst_query_add_scheduling_mode (query, GST_PAD_MODE_PUSH);

      ret = TRUE;
      break;
    }
    default:
      ret = FALSE;
      break;
  }

  if (!ret)
    ret = GST_BASE_SRC_CLASS (parent_class)->query (basesrc, query);

  return ret;
}

static gboolean
gst_rtmp_src_is_seekable (GstBaseSrc * basesrc)
{
  GstRTMPSrc *src;

  src = GST_RTMP_SRC (basesrc);

  return src->seekable;
}

static gboolean
gst_rtmp_src_prepare_seek_segment (GstBaseSrc * basesrc, GstEvent * event,
    GstSegment * segment)
{
  GstRTMPSrc *src;
  GstSeekType cur_type, stop_type;
  gint64 cur, stop;
  GstSeekFlags flags;
  GstFormat format;
  gdouble rate;

  src = GST_RTMP_SRC (basesrc);

  gst_event_parse_seek (event, &rate, &format, &flags,
      &cur_type, &cur, &stop_type, &stop);

  if (!src->seekable) {
    GST_LOG_OBJECT (src, "Not a seekable stream");
    return FALSE;
  }

  if (!src->rtmp) {
    GST_LOG_OBJECT (src, "Not connected yet");
    return FALSE;
  }

  if (format != GST_FORMAT_TIME) {
    GST_LOG_OBJECT (src, "Seeking only supported in TIME format");
    return FALSE;
  }

  if (stop_type != GST_SEEK_TYPE_NONE) {
    GST_LOG_OBJECT (src, "Setting a stop position is not supported");
    return FALSE;
  }

  gst_segment_init (segment, GST_FORMAT_TIME);
  gst_segment_do_seek (segment, rate, format, flags, cur_type, cur, stop_type,
      stop, NULL);

  return TRUE;
}

static gboolean
gst_rtmp_src_do_seek (GstBaseSrc * basesrc, GstSegment * segment)
{
  GstRTMPSrc *src;

  src = GST_RTMP_SRC (basesrc);

  if (segment->format != GST_FORMAT_TIME) {
    GST_LOG_OBJECT (src, "Only time based seeks are supported");
    return FALSE;
  }

  if (!src->rtmp) {
    GST_LOG_OBJECT (src, "Not connected yet");
    return FALSE;
  }

  /* Initial seek */
  if (src->cur_offset == 0 && segment->start == 0)
    goto success;

  if (!src->seekable) {
    GST_LOG_OBJECT (src, "Not a seekable stream");
    return FALSE;
  }

  /* If we have just disconnected in unlock(), we need to re-connect
   * and also let librtmp read some data before sending a seek,
   * otherwise it will stall. Calling create() does both. */
  if (!RTMP_IsConnected (src->rtmp)) {
    GstBuffer *buffer = NULL;
    gst_rtmp_src_create (GST_PUSH_SRC (basesrc), &buffer);
    gst_buffer_replace (&buffer, NULL);
  }

  src->last_timestamp = GST_CLOCK_TIME_NONE;
  if (!RTMP_SendSeek (src->rtmp, segment->start / GST_MSECOND)) {
    GST_ERROR_OBJECT (src, "Seeking failed");
    src->seekable = FALSE;
    return FALSE;
  }

success:
  /* This is set here so that the call to create() above doesn't clear it */
  src->discont = TRUE;

  GST_DEBUG_OBJECT (src, "Seek to %" GST_TIME_FORMAT " successful",
      GST_TIME_ARGS (segment->start));

  return TRUE;
}

static gboolean
gst_rtmp_src_connect (GstRTMPSrc * src)
{
  RTMP_Init (src->rtmp);
  src->rtmp->Link.timeout = src->timeout;
  if (!RTMP_SetupURL (src->rtmp, src->uri)) {
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ, (NULL),
        ("Failed to setup URL '%s'", src->uri));
    return FALSE;
  }
  src->seekable = !(src->rtmp->Link.lFlags & RTMP_LF_LIVE);
  GST_INFO_OBJECT (src, "seekable %d", src->seekable);

  /* open if required */
  if (!RTMP_IsConnected (src->rtmp)) {
    if (!RTMP_Connect (src->rtmp, NULL)) {
      GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ, (NULL),
          ("Could not connect to RTMP stream \"%s\" for reading", src->uri));
      return FALSE;
    }
  }

  return TRUE;
}

/* open the file, do stuff necessary to go to PAUSED state */
static gboolean
gst_rtmp_src_start (GstBaseSrc * basesrc)
{
  GstRTMPSrc *src;

  src = GST_RTMP_SRC (basesrc);

  if (!src->uri) {
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ, (NULL), ("No filename given"));
    return FALSE;
  }

  src->cur_offset = 0;
  src->last_timestamp = 0;
  src->discont = TRUE;

  src->rtmp = RTMP_Alloc ();
  if (!src->rtmp) {
    GST_ERROR_OBJECT (src, "Could not allocate librtmp's RTMP context");
    goto error;
  }

  if (!gst_rtmp_src_connect (src))
    goto error;

  return TRUE;

error:
  if (src->rtmp) {
    RTMP_Free (src->rtmp);
    src->rtmp = NULL;
  }
  return FALSE;
}

static gboolean
gst_rtmp_src_unlock (GstBaseSrc * basesrc)
{
  GstRTMPSrc *rtmpsrc = GST_RTMP_SRC (basesrc);

  GST_DEBUG_OBJECT (rtmpsrc, "unlock");

  /* This closes the socket, which means that any pending socket calls
   * error out. */
  if (rtmpsrc->rtmp) {
    RTMP_Close (rtmpsrc->rtmp);
  }

  return TRUE;
}


static gboolean
gst_rtmp_src_stop (GstBaseSrc * basesrc)
{
  GstRTMPSrc *src;

  src = GST_RTMP_SRC (basesrc);

  if (src->rtmp) {
    RTMP_Free (src->rtmp);
    src->rtmp = NULL;
  }

  src->cur_offset = 0;
  src->last_timestamp = 0;
  src->discont = TRUE;

  return TRUE;
}
