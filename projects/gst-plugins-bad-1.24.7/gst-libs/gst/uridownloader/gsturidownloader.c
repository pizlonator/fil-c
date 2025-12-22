/* GStreamer
 * Copyright (C) 2011 Andoni Morales Alastruey <ylatuya@gmail.com>
 *
 * gstfragment.c:
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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>
#include "gstfragment.h"
#include "gsturidownloader.h"
#include "gsturidownloader_debug.h"

#define GST_CAT_DEFAULT uridownloader_debug
GST_DEBUG_CATEGORY (uridownloader_debug);

struct _GstUriDownloaderPrivate
{
  /* Fragments fetcher */
  GstElement *urisrc;
  GstBus *bus;
  GstPad *pad;
  GstFragment *download;
  gboolean got_buffer;
  GMutex download_lock;         /* used to restrict to one download only */

  GWeakRef parent;

  GError *err;

  GCond cond;
  gboolean cancelled;
};

static void gst_uri_downloader_finalize (GObject * object);
static void gst_uri_downloader_dispose (GObject * object);

static GstFlowReturn gst_uri_downloader_chain (GstPad * pad, GstObject * parent,
    GstBuffer * buf);
static gboolean gst_uri_downloader_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event);
static GstBusSyncReply gst_uri_downloader_bus_handler (GstBus * bus,
    GstMessage * message, gpointer data);

static gboolean gst_uri_downloader_ensure_src (GstUriDownloader * downloader,
    const gchar * uri);
static void gst_uri_downloader_destroy_src (GstUriDownloader * downloader);

static GstStaticPadTemplate sinkpadtemplate = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

#define _do_init \
{ \
  GST_DEBUG_CATEGORY_INIT (uridownloader_debug, "uridownloader", 0, "URI downloader"); \
}

G_DEFINE_TYPE_WITH_CODE (GstUriDownloader, gst_uri_downloader, GST_TYPE_OBJECT,
    G_ADD_PRIVATE (GstUriDownloader)
    _do_init);

static void
gst_uri_downloader_class_init (GstUriDownloaderClass * klass)
{
  GObjectClass *gobject_class;

  gobject_class = (GObjectClass *) klass;

  gobject_class->dispose = gst_uri_downloader_dispose;
  gobject_class->finalize = gst_uri_downloader_finalize;
}

static void
gst_uri_downloader_init (GstUriDownloader * downloader)
{
  downloader->priv = gst_uri_downloader_get_instance_private (downloader);

  /* Initialize the sink pad. This pad will be connected to the src pad of the
   * element created with gst_element_make_from_uri and will handle the download */
  downloader->priv->pad =
      gst_pad_new_from_static_template (&sinkpadtemplate, "sink");
  gst_pad_set_chain_function (downloader->priv->pad,
      GST_DEBUG_FUNCPTR (gst_uri_downloader_chain));
  gst_pad_set_event_function (downloader->priv->pad,
      GST_DEBUG_FUNCPTR (gst_uri_downloader_sink_event));
  gst_pad_set_element_private (downloader->priv->pad, downloader);
  gst_pad_set_active (downloader->priv->pad, TRUE);

  /* Create a bus to handle error and warning message from the source element */
  downloader->priv->bus = gst_bus_new ();

  g_mutex_init (&downloader->priv->download_lock);
  g_cond_init (&downloader->priv->cond);
}

static void
gst_uri_downloader_dispose (GObject * object)
{
  GstUriDownloader *downloader = GST_URI_DOWNLOADER (object);

  gst_uri_downloader_destroy_src (downloader);

  if (downloader->priv->bus != NULL) {
    gst_object_unref (downloader->priv->bus);
    downloader->priv->bus = NULL;
  }

  if (downloader->priv->pad) {
    gst_object_unref (downloader->priv->pad);
    downloader->priv->pad = NULL;
  }

  if (downloader->priv->download) {
    g_object_unref (downloader->priv->download);
    downloader->priv->download = NULL;
  }

  g_weak_ref_clear (&downloader->priv->parent);

  G_OBJECT_CLASS (gst_uri_downloader_parent_class)->dispose (object);
}

static void
gst_uri_downloader_finalize (GObject * object)
{
  GstUriDownloader *downloader = GST_URI_DOWNLOADER (object);

  g_mutex_clear (&downloader->priv->download_lock);
  g_cond_clear (&downloader->priv->cond);

  G_OBJECT_CLASS (gst_uri_downloader_parent_class)->finalize (object);
}

GstUriDownloader *
gst_uri_downloader_new (void)
{
  GstUriDownloader *downloader;

  downloader = g_object_new (GST_TYPE_URI_DOWNLOADER, NULL);
  gst_object_ref_sink (downloader);

  return downloader;
}

/**
 * gst_uri_downloader_set_parent:
 * @param downloader: the #GstUriDownloader
 * @param parent: the parent #GstElement
 *
 * Sets an element as parent of this #GstUriDownloader so that context
 * requests from the underlying source are proxied to the main pipeline
 * and set back if a context was provided.
 */
void
gst_uri_downloader_set_parent (GstUriDownloader * downloader,
    GstElement * parent)
{
  g_weak_ref_set (&downloader->priv->parent, parent);
}

static gboolean
gst_uri_downloader_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event)
{
  gboolean ret = FALSE;
  GstUriDownloader *downloader;

  downloader = GST_URI_DOWNLOADER (gst_pad_get_element_private (pad));

  switch (event->type) {
    case GST_EVENT_EOS:{
      GST_OBJECT_LOCK (downloader);
      GST_DEBUG_OBJECT (downloader, "Got EOS on the fetcher pad");
      if (downloader->priv->download != NULL) {
        /* signal we have fetched the URI */
        downloader->priv->download->completed = TRUE;
        downloader->priv->download->download_stop_time =
            gst_util_get_timestamp ();
        GST_DEBUG_OBJECT (downloader, "Signaling chain funtion");
        g_cond_signal (&downloader->priv->cond);
      }
      GST_OBJECT_UNLOCK (downloader);
      gst_event_unref (event);
      break;
    }
    case GST_EVENT_CUSTOM_DOWNSTREAM_STICKY:{
      const GstStructure *str;
      str = gst_event_get_structure (event);
      if (gst_structure_has_name (str, "http-headers")) {
        GST_OBJECT_LOCK (downloader);
        if (downloader->priv->download != NULL) {
          if (downloader->priv->download->headers)
            gst_structure_free (downloader->priv->download->headers);
          downloader->priv->download->headers = gst_structure_copy (str);
        }
        GST_OBJECT_UNLOCK (downloader);
      }
    }
      /* falls through */
    default:
      ret = gst_pad_event_default (pad, parent, event);
      break;
  }

  return ret;
}

static GstBusSyncReply
gst_uri_downloader_bus_handler (GstBus * bus,
    GstMessage * message, gpointer data)
{
  GstUriDownloader *downloader = (GstUriDownloader *) (data);

  if (GST_MESSAGE_TYPE (message) == GST_MESSAGE_ERROR) {
    GError *err = NULL;
    gchar *dbg_info = NULL;
    gchar *new_error = NULL;

    gst_message_parse_error (message, &err, &dbg_info);
    GST_WARNING_OBJECT (downloader,
        "Received error: %s from %s, the download will be cancelled",
        err->message, GST_OBJECT_NAME (message->src));
    GST_DEBUG ("Debugging info: %s", (dbg_info) ? dbg_info : "none");

    if (dbg_info)
      new_error = g_strdup_printf ("%s: %s\n", err->message, dbg_info);
    if (new_error) {
      g_free (err->message);
      err->message = new_error;
    }

    if (!downloader->priv->err)
      downloader->priv->err = err;
    else
      g_error_free (err);

    g_free (dbg_info);

    /* remove the sync handler to avoid duplicated messages */
    gst_bus_set_sync_handler (downloader->priv->bus, NULL, NULL, NULL);

    /* stop the download */
    GST_OBJECT_LOCK (downloader);
    if (downloader->priv->download != NULL) {
      GST_DEBUG_OBJECT (downloader, "Stopping download");
      g_object_unref (downloader->priv->download);
      downloader->priv->download = NULL;
      downloader->priv->cancelled = TRUE;
      g_cond_signal (&downloader->priv->cond);
    }
    GST_OBJECT_UNLOCK (downloader);
  } else if (GST_MESSAGE_TYPE (message) == GST_MESSAGE_WARNING) {
    GError *err = NULL;
    gchar *dbg_info = NULL;

    gst_message_parse_warning (message, &err, &dbg_info);
    GST_WARNING_OBJECT (downloader,
        "Received warning: %s from %s",
        GST_OBJECT_NAME (message->src), err->message);
    GST_DEBUG ("Debugging info: %s", (dbg_info) ? dbg_info : "none");
    g_error_free (err);
    g_free (dbg_info);
  } else if (GST_MESSAGE_TYPE (message) == GST_MESSAGE_NEED_CONTEXT) {
    GstElement *parent = g_weak_ref_get (&downloader->priv->parent);

    /* post the same need-context as if it was from the parent and then
     * get it to our internal element that requested it */
    if (parent && GST_IS_ELEMENT (GST_MESSAGE_SRC (message))) {
      const gchar *context_type;
      GstContext *context;
      GstElement *msg_src = GST_ELEMENT_CAST (GST_MESSAGE_SRC (message));

      gst_message_parse_context_type (message, &context_type);
      context = gst_element_get_context (parent, context_type);

      /* No context, request one */
      if (!context) {
        GstMessage *need_context_msg =
            gst_message_new_need_context (GST_OBJECT_CAST (parent),
            context_type);
        gst_element_post_message (parent, need_context_msg);
        context = gst_element_get_context (parent, context_type);
      }

      if (context) {
        gst_element_set_context (msg_src, context);
        gst_context_unref (context);
      }
    }
    if (parent)
      gst_object_unref (parent);
  }

  gst_message_unref (message);
  return GST_BUS_DROP;
}

static GstFlowReturn
gst_uri_downloader_chain (GstPad * pad, GstObject * parent, GstBuffer * buf)
{
  GstUriDownloader *downloader;

  downloader = GST_URI_DOWNLOADER (gst_pad_get_element_private (pad));

  /* HTML errors (404, 500, etc...) are also pushed through this pad as
   * response but the source element will also post a warning or error message
   * in the bus, which is handled synchronously cancelling the download.
   */
  GST_OBJECT_LOCK (downloader);
  if (downloader->priv->download == NULL) {
    /* Download cancelled, quit */
    gst_buffer_unref (buf);
    GST_OBJECT_UNLOCK (downloader);
    goto done;
  }

  GST_LOG_OBJECT (downloader, "The uri fetcher received a new buffer "
      "of size %" G_GSIZE_FORMAT, gst_buffer_get_size (buf));
  downloader->priv->got_buffer = TRUE;
  if (!gst_fragment_add_buffer (downloader->priv->download, buf)) {
    GST_WARNING_OBJECT (downloader, "Could not add buffer to fragment");
    gst_buffer_unref (buf);
  }
  GST_OBJECT_UNLOCK (downloader);

done:
  {
    return GST_FLOW_OK;
  }
}

void
gst_uri_downloader_reset (GstUriDownloader * downloader)
{
  g_return_if_fail (downloader != NULL);

  GST_OBJECT_LOCK (downloader);
  downloader->priv->cancelled = FALSE;
  GST_OBJECT_UNLOCK (downloader);
}

void
gst_uri_downloader_cancel (GstUriDownloader * downloader)
{
  GST_OBJECT_LOCK (downloader);
  if (downloader->priv->download != NULL) {
    GST_DEBUG_OBJECT (downloader, "Cancelling download");
    g_object_unref (downloader->priv->download);
    downloader->priv->download = NULL;
    downloader->priv->cancelled = TRUE;
    GST_DEBUG_OBJECT (downloader, "Signaling chain funtion");
    g_cond_signal (&downloader->priv->cond);
  } else {
    gboolean cancelled;

    cancelled = downloader->priv->cancelled;
    downloader->priv->cancelled = TRUE;
    if (cancelled)
      GST_DEBUG_OBJECT (downloader,
          "Trying to cancel a download that was alredy cancelled");
  }
  GST_OBJECT_UNLOCK (downloader);
}

static gboolean
gst_uri_downloader_set_range (GstUriDownloader * downloader,
    gint64 range_start, gint64 range_end)
{
  g_return_val_if_fail (range_start >= 0, FALSE);
  g_return_val_if_fail (range_end >= -1, FALSE);

  if (range_start || (range_end >= 0)) {
    GstEvent *seek;

    seek = gst_event_new_seek (1.0, GST_FORMAT_BYTES, GST_SEEK_FLAG_FLUSH,
        GST_SEEK_TYPE_SET, range_start, GST_SEEK_TYPE_SET, range_end);

    return gst_element_send_event (downloader->priv->urisrc, seek);
  }
  return TRUE;
}

static gboolean
gst_uri_downloader_ensure_src (GstUriDownloader * downloader, const gchar * uri)
{
  if (downloader->priv->urisrc) {
    gchar *old_protocol, *new_protocol;
    gchar *old_uri;

    old_uri =
        gst_uri_handler_get_uri (GST_URI_HANDLER (downloader->priv->urisrc));
    old_protocol = gst_uri_get_protocol (old_uri);
    new_protocol = gst_uri_get_protocol (uri);

    if (!g_str_equal (old_protocol, new_protocol)) {
      gst_uri_downloader_destroy_src (downloader);
      GST_DEBUG_OBJECT (downloader, "Can't re-use old source element");
    } else {
      GError *err = NULL;

      GST_DEBUG_OBJECT (downloader, "Re-using old source element");
      if (!gst_uri_handler_set_uri
          (GST_URI_HANDLER (downloader->priv->urisrc), uri, &err)) {
        GST_DEBUG_OBJECT (downloader,
            "Failed to re-use old source element: %s", err->message);
        g_clear_error (&err);
        gst_uri_downloader_destroy_src (downloader);
      }
    }
    g_free (old_uri);
    g_free (old_protocol);
    g_free (new_protocol);
  }

  if (!downloader->priv->urisrc) {
    GST_DEBUG_OBJECT (downloader, "Creating source element for the URI:%s",
        uri);
    downloader->priv->urisrc =
        gst_element_make_from_uri (GST_URI_SRC, uri, NULL, NULL);
    if (downloader->priv->urisrc) {
      /* gst_element_make_from_uri returns a floating reference
       * and we are not going to transfer the ownership, so we
       * should take it.
       */
      gst_object_ref_sink (downloader->priv->urisrc);
    }
  }

  return downloader->priv->urisrc != NULL;
}

static void
gst_uri_downloader_destroy_src (GstUriDownloader * downloader)
{
  if (!downloader->priv->urisrc)
    return;

  gst_element_set_state (downloader->priv->urisrc, GST_STATE_NULL);
  gst_object_unref (downloader->priv->urisrc);
  downloader->priv->urisrc = NULL;
}

static gboolean
gst_uri_downloader_set_uri (GstUriDownloader * downloader, const gchar * uri,
    const gchar * referer, gboolean compress,
    gboolean refresh, gboolean allow_cache)
{
  GstPad *pad;
  GObjectClass *gobject_class;

  if (!gst_uri_is_valid (uri))
    return FALSE;

  if (!gst_uri_downloader_ensure_src (downloader, uri))
    return FALSE;

  gobject_class = G_OBJECT_GET_CLASS (downloader->priv->urisrc);
  if (g_object_class_find_property (gobject_class, "compress"))
    g_object_set (downloader->priv->urisrc, "compress", compress, NULL);
  if (g_object_class_find_property (gobject_class, "keep-alive"))
    g_object_set (downloader->priv->urisrc, "keep-alive", TRUE, NULL);
  if (g_object_class_find_property (gobject_class, "extra-headers")) {
    if (referer || refresh || !allow_cache) {
      GstStructure *extra_headers = gst_structure_new_empty ("headers");

      if (referer)
        gst_structure_set (extra_headers, "Referer", G_TYPE_STRING, referer,
            NULL);

      if (!allow_cache)
        gst_structure_set (extra_headers, "Cache-Control", G_TYPE_STRING,
            "no-cache", NULL);
      else if (refresh)
        gst_structure_set (extra_headers, "Cache-Control", G_TYPE_STRING,
            "max-age=0", NULL);

      g_object_set (downloader->priv->urisrc, "extra-headers", extra_headers,
          NULL);

      gst_structure_free (extra_headers);
    } else {
      g_object_set (downloader->priv->urisrc, "extra-headers", NULL, NULL);
    }
  }

  /* add a sync handler for the bus messages to detect errors in the download */
  gst_element_set_bus (GST_ELEMENT (downloader->priv->urisrc),
      downloader->priv->bus);
  gst_bus_set_sync_handler (downloader->priv->bus,
      gst_uri_downloader_bus_handler, downloader, NULL);

  pad = gst_element_get_static_pad (downloader->priv->urisrc, "src");
  if (!pad)
    return FALSE;
  gst_pad_link (pad, downloader->priv->pad);
  gst_object_unref (pad);
  return TRUE;
}

static gboolean
gst_uri_downloader_set_method (GstUriDownloader * downloader,
    const gchar * method)
{
  GObjectClass *gobject_class;

  if (!downloader->priv->urisrc)
    return FALSE;

  gobject_class = G_OBJECT_GET_CLASS (downloader->priv->urisrc);
  if (g_object_class_find_property (gobject_class, "method")) {
    g_object_set (downloader->priv->urisrc, "method", method, NULL);
    return TRUE;
  }
  return FALSE;
}

GstFragment *
gst_uri_downloader_fetch_uri (GstUriDownloader * downloader,
    const gchar * uri, const gchar * referer, gboolean compress,
    gboolean refresh, gboolean allow_cache, GError ** err)
{
  return gst_uri_downloader_fetch_uri_with_range (downloader, uri,
      referer, compress, refresh, allow_cache, 0, -1, err);
}

/**
 * gst_uri_downloader_fetch_uri_with_range:
 * @downloader: the #GstUriDownloader
 * @uri: the uri
 * @range_start: the starting byte index
 * @range_end: the final byte index, use -1 for unspecified
 *
 * Returns the downloaded #GstFragment
 */
GstFragment *
gst_uri_downloader_fetch_uri_with_range (GstUriDownloader *
    downloader, const gchar * uri, const gchar * referer, gboolean compress,
    gboolean refresh, gboolean allow_cache,
    gint64 range_start, gint64 range_end, GError ** err)
{
  GstStateChangeReturn ret;
  GstFragment *download = NULL;

  GST_DEBUG_OBJECT (downloader, "Fetching URI %s", uri);

  g_mutex_lock (&downloader->priv->download_lock);
  downloader->priv->err = NULL;
  downloader->priv->got_buffer = FALSE;

  GST_OBJECT_LOCK (downloader);
  if (downloader->priv->cancelled) {
    GST_DEBUG_OBJECT (downloader, "Cancelled, aborting fetch");
    goto quit;
  }

  if (!gst_uri_downloader_set_uri (downloader, uri, referer, compress, refresh,
          allow_cache)) {
    GST_WARNING_OBJECT (downloader, "Failed to set URI");
    goto quit;
  }

  gst_bus_set_flushing (downloader->priv->bus, FALSE);
  if (downloader->priv->download)
    g_object_unref (downloader->priv->download);
  downloader->priv->download = gst_fragment_new ();
  downloader->priv->download->range_start = range_start;
  downloader->priv->download->range_end = range_end;
  GST_OBJECT_UNLOCK (downloader);
  ret = gst_element_set_state (downloader->priv->urisrc, GST_STATE_READY);
  GST_OBJECT_LOCK (downloader);
  if (ret == GST_STATE_CHANGE_FAILURE || downloader->priv->download == NULL) {
    GST_WARNING_OBJECT (downloader, "Failed to set src to READY");
    goto quit;
  }

  /* might have been cancelled because of failures in state change */
  if (downloader->priv->cancelled) {
    goto quit;
  }

  if (range_start < 0 && range_end < 0) {
    if (!gst_uri_downloader_set_method (downloader, "HEAD")) {
      GST_WARNING_OBJECT (downloader, "Failed to set HTTP method");
      goto quit;
    }
  } else {
    if (!gst_uri_downloader_set_range (downloader, range_start, range_end)) {
      GST_WARNING_OBJECT (downloader, "Failed to set range");
      goto quit;
    }
  }

  GST_OBJECT_UNLOCK (downloader);
  ret = gst_element_set_state (downloader->priv->urisrc, GST_STATE_PLAYING);
  GST_OBJECT_LOCK (downloader);
  if (ret == GST_STATE_CHANGE_FAILURE) {
    if (downloader->priv->download) {
      g_object_unref (downloader->priv->download);
      downloader->priv->download = NULL;
    }
    goto quit;
  }

  /* might have been cancelled because of failures in state change */
  if (downloader->priv->cancelled) {
    goto quit;
  }

  /* wait until:
   *   - the download succeed (EOS in the src pad)
   *   - the download failed (Error message on the fetcher bus)
   *   - the download was canceled
   */
  GST_DEBUG_OBJECT (downloader, "Waiting to fetch the URI %s", uri);
  while (!downloader->priv->cancelled && !downloader->priv->download->completed)
    g_cond_wait (&downloader->priv->cond, GST_OBJECT_GET_LOCK (downloader));

  if (downloader->priv->cancelled) {
    if (downloader->priv->download) {
      g_object_unref (downloader->priv->download);
      downloader->priv->download = NULL;
    }
    goto quit;
  }

  download = downloader->priv->download;
  downloader->priv->download = NULL;
  if (!downloader->priv->got_buffer) {
    if (download->range_start < 0 && download->range_end < 0) {
      /* HEAD request, so we don't expect a response */
    } else {
      g_object_unref (download);
      download = NULL;
      GST_ERROR_OBJECT (downloader, "Didn't retrieve a buffer before EOS");
    }
  }

  if (download != NULL)
    GST_INFO_OBJECT (downloader, "URI fetched successfully");
  else
    GST_INFO_OBJECT (downloader, "Error fetching URI");

quit:
  {
    if (downloader->priv->urisrc) {
      GstPad *pad;
      GstElement *urisrc;

      urisrc = downloader->priv->urisrc;

      GST_DEBUG_OBJECT (downloader, "Stopping source element %s",
          GST_ELEMENT_NAME (urisrc));

      /* remove the bus' sync handler */
      gst_bus_set_sync_handler (downloader->priv->bus, NULL, NULL, NULL);
      gst_bus_set_flushing (downloader->priv->bus, TRUE);

      /* set the element state to NULL */
      GST_OBJECT_UNLOCK (downloader);
      if (download == NULL) {
        gst_element_set_state (urisrc, GST_STATE_NULL);
      } else {
        GstQuery *query;

        /* Download successful, let's query the URI */
        query = gst_query_new_uri ();
        if (gst_element_query (urisrc, query)) {
          gst_query_parse_uri (query, &download->uri);
          gst_query_parse_uri_redirection (query, &download->redirect_uri);
          gst_query_parse_uri_redirection_permanent (query,
              &download->redirect_permanent);
        }
        gst_query_unref (query);
        gst_element_set_state (urisrc, GST_STATE_READY);
      }
      GST_OBJECT_LOCK (downloader);
      gst_element_set_bus (urisrc, NULL);

      /* unlink the source element from the internal pad */
      pad = gst_pad_get_peer (downloader->priv->pad);
      if (pad) {
        gst_pad_unlink (pad, downloader->priv->pad);
        gst_object_unref (pad);
      }
    }
    GST_OBJECT_UNLOCK (downloader);

    if (download == NULL) {
      if (!downloader->priv->err) {
        g_set_error (err, GST_RESOURCE_ERROR, GST_RESOURCE_ERROR_OPEN_READ,
            "Failed to download '%s'", uri);
      } else {
        g_propagate_error (err, downloader->priv->err);
        downloader->priv->err = NULL;
      }
    }

    downloader->priv->cancelled = FALSE;

    g_mutex_unlock (&downloader->priv->download_lock);
    return download;
  }
}
