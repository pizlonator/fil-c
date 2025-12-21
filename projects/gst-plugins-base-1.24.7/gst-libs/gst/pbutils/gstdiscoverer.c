/* GStreamer
 * Copyright (C) 2009 Edward Hervey <edward.hervey@collabora.co.uk>
 *               2009 Nokia Corporation
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
 * SECTION:gstdiscoverer
 * @title: GstDiscoverer
 * @short_description: Utility for discovering information on URIs.
 *
 * The #GstDiscoverer is a utility object which allows to get as much
 * information as possible from one or many URIs.
 *
 * It provides two APIs, allowing usage in blocking or non-blocking mode.
 *
 * The blocking mode just requires calling gst_discoverer_discover_uri()
 * with the URI one wishes to discover.
 *
 * The non-blocking mode requires a running #GMainLoop iterating a
 * #GMainContext, where one connects to the various signals, appends the
 * URIs to be processed (through gst_discoverer_discover_uri_async()) and then
 * asks for the discovery to begin (through gst_discoverer_start()).
 * By default this will use the GLib default main context unless you have
 * set a custom context using g_main_context_push_thread_default().
 *
 * All the information is returned in a #GstDiscovererInfo structure.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/video/video.h>
#include <gst/audio/audio.h>

#include <string.h>

#include "pbutils.h"
#include "pbutils-private.h"

/* For g_stat () */
#include <glib/gstdio.h>

GST_DEBUG_CATEGORY_STATIC (discoverer_debug);
#define GST_CAT_DEFAULT discoverer_debug
#define CACHE_DIRNAME "discoverer"

static GQuark _CAPS_QUARK;
static GQuark _TAGS_QUARK;
static GQuark _ELEMENT_SRCPAD_QUARK;
static GQuark _TOC_QUARK;
static GQuark _STREAM_ID_QUARK;
static GQuark _MISSING_PLUGIN_QUARK;
static GQuark _STREAM_TOPOLOGY_QUARK;
static GQuark _TOPOLOGY_PAD_QUARK;


typedef struct
{
  GstDiscoverer *dc;
  GstPad *pad;
  GstElement *queue;
  GstElement *sink;
  GstTagList *tags;
  GstToc *toc;
  gchar *stream_id;
  gulong probe_id;
} PrivateStream;

struct _GstDiscovererPrivate
{
  gboolean async;

  /* allowed time to discover each uri in nanoseconds */
  GstClockTime timeout;

  /* list of pending URI to process (current excluded) */
  GList *pending_uris;

  GMutex lock;
  /* TRUE if cleaning up discoverer */
  gboolean cleanup;

  /* TRUE if processing a URI */
  gboolean processing;

  /* TRUE if discoverer has been started */
  gboolean running;

  /* current items */
  GstDiscovererInfo *current_info;
  GError *current_error;
  GstStructure *current_topology;

  GstTagList *all_tags;
  GstTagList *global_tags;

  /* List of private streams */
  GList *streams;

  /* List of these sinks and their handler IDs (to remove the probe) */
  guint pending_subtitle_pads;

  /* Whether we received no_more_pads */
  gboolean no_more_pads;

  GstState target_state;
  GstState current_state;

  /* Global elements */
  GstBin *pipeline;
  GstElement *uridecodebin;
  GstBus *bus;

  /* Custom main context variables */
  GMainContext *ctx;
  GSource *bus_source;
  GSource *timeout_source;

  /* reusable queries */
  GstQuery *seeking_query;

  /* Handler ids for various callbacks */
  gulong pad_added_id;
  gulong pad_remove_id;
  gulong no_more_pads_id;
  gulong source_chg_id;
  gulong bus_cb_id;

  gboolean use_cache;
};

#define DISCO_LOCK(dc) g_mutex_lock (&dc->priv->lock);
#define DISCO_UNLOCK(dc) g_mutex_unlock (&dc->priv->lock);

static void
_do_init (void)
{
  GST_DEBUG_CATEGORY_INIT (discoverer_debug, "discoverer", 0, "Discoverer");

  _CAPS_QUARK = g_quark_from_static_string ("caps");
  _ELEMENT_SRCPAD_QUARK = g_quark_from_static_string ("element-srcpad");
  _TAGS_QUARK = g_quark_from_static_string ("tags");
  _TOC_QUARK = g_quark_from_static_string ("toc");
  _STREAM_ID_QUARK = g_quark_from_static_string ("stream-id");
  _MISSING_PLUGIN_QUARK = g_quark_from_static_string ("missing-plugin");
  _STREAM_TOPOLOGY_QUARK = g_quark_from_static_string ("stream-topology");
  _TOPOLOGY_PAD_QUARK = g_quark_from_static_string ("pad");
};

G_DEFINE_TYPE_EXTENDED (GstDiscoverer, gst_discoverer, G_TYPE_OBJECT, 0,
    G_ADD_PRIVATE (GstDiscoverer) _do_init ());

enum
{
  SIGNAL_FINISHED,
  SIGNAL_STARTING,
  SIGNAL_DISCOVERED,
  SIGNAL_SOURCE_SETUP,
  SIGNAL_LOAD_SERIALIZED_INFO,
  LAST_SIGNAL
};

#define DEFAULT_PROP_TIMEOUT 15 * GST_SECOND
#define DEFAULT_PROP_USE_CACHE FALSE

enum
{
  PROP_0,
  PROP_TIMEOUT,
  PROP_USE_CACHE
};

static guint gst_discoverer_signals[LAST_SIGNAL] = { 0 };

static void gst_discoverer_set_timeout (GstDiscoverer * dc,
    GstClockTime timeout);
static gboolean async_timeout_cb (GstDiscoverer * dc);

static void discoverer_bus_cb (GstBus * bus, GstMessage * msg,
    GstDiscoverer * dc);
static void uridecodebin_pad_added_cb (GstElement * uridecodebin, GstPad * pad,
    GstDiscoverer * dc);
static void uridecodebin_pad_removed_cb (GstElement * uridecodebin,
    GstPad * pad, GstDiscoverer * dc);
static void uridecodebin_no_more_pads_cb (GstElement * uridecodebin,
    GstDiscoverer * dc);
static void uridecodebin_source_changed_cb (GstElement * uridecodebin,
    GParamSpec * pspec, GstDiscoverer * dc);

static void gst_discoverer_dispose (GObject * dc);
static void gst_discoverer_finalize (GObject * dc);
static void gst_discoverer_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_discoverer_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static gboolean _setup_locked (GstDiscoverer * dc);
static void handle_current_async (GstDiscoverer * dc);
static gboolean emit_discovererd_and_next (GstDiscoverer * dc);
static GVariant *gst_discoverer_info_to_variant_recurse (GstDiscovererStreamInfo
    * sinfo, GstDiscovererSerializeFlags flags);
static GstDiscovererStreamInfo *_parse_discovery (GVariant * variant,
    GstDiscovererInfo * info);
static GstDiscovererInfo *load_serialized_info (GstDiscoverer * dc,
    gchar * uri);

static gboolean
_gst_discoverer_info_accumulator (GSignalInvocationHint * ihint,
    GValue * return_accu, const GValue * handler_return, gpointer dummy)
{
  GstDiscovererInfo *info;

  info = g_value_get_object (handler_return);
  GST_DEBUG ("got discoverer info %" GST_PTR_FORMAT, info);

  g_value_set_object (return_accu, info);

  /* stop emission if we have a discoverer info */
  return (info == NULL);
}

static void
gst_discoverer_class_init (GstDiscovererClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->dispose = gst_discoverer_dispose;
  gobject_class->finalize = gst_discoverer_finalize;

  gobject_class->set_property = gst_discoverer_set_property;
  gobject_class->get_property = gst_discoverer_get_property;

  klass->load_serialize_info = load_serialized_info;


  /* properties */
  /**
   * GstDiscoverer:timeout:
   *
   * The duration (in nanoseconds) after which the discovery of an individual
   * URI will timeout.
   *
   * If the discovery of a URI times out, the %GST_DISCOVERER_TIMEOUT will be
   * set on the result flags.
   */
  g_object_class_install_property (gobject_class, PROP_TIMEOUT,
      g_param_spec_uint64 ("timeout", "timeout", "Timeout",
          GST_SECOND, 3600 * GST_SECOND, DEFAULT_PROP_TIMEOUT,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  /**
   * GstDiscoverer::use-cache:
   *
   * Whether to use a serialized version of the discoverer info from our
   * own cache if accessible. This allows the discovery to be much faster
   * as when using this option, we do not need to create a #GstPipeline
   * and run it, but instead, just reload the #GstDiscovererInfo in its
   * serialized form.
   *
   * The cache files are saved in `$XDG_CACHE_DIR/gstreamer-1.0/discoverer/`.
   *
   * Since: 1.16
   */
  g_object_class_install_property (gobject_class, PROP_USE_CACHE,
      g_param_spec_boolean ("use-cache", "use cache", "Use cache",
          DEFAULT_PROP_USE_CACHE,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  /* signals */
  /**
   * GstDiscoverer::finished:
   * @discoverer: the #GstDiscoverer
   *
   * Will be emitted in async mode when all pending URIs have been processed.
   */
  gst_discoverer_signals[SIGNAL_FINISHED] =
      g_signal_new ("finished", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
      G_STRUCT_OFFSET (GstDiscovererClass, finished), NULL, NULL, NULL,
      G_TYPE_NONE, 0, G_TYPE_NONE);

  /**
   * GstDiscoverer::starting:
   * @discoverer: the #GstDiscoverer
   *
   * Will be emitted when the discover starts analyzing the pending URIs
   */
  gst_discoverer_signals[SIGNAL_STARTING] =
      g_signal_new ("starting", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
      G_STRUCT_OFFSET (GstDiscovererClass, starting), NULL, NULL, NULL,
      G_TYPE_NONE, 0, G_TYPE_NONE);

  /**
   * GstDiscoverer::discovered:
   * @discoverer: the #GstDiscoverer
   * @info: the results #GstDiscovererInfo
   * @error: (allow-none) (type GLib.Error): #GError, which will be non-NULL
   *                                         if an error occurred during
   *                                         discovery. You must not free
   *                                         this #GError, it will be freed by
   *                                         the discoverer.
   *
   * Will be emitted in async mode when all information on a URI could be
   * discovered, or an error occurred.
   *
   * When an error occurs, @info might still contain some partial information,
   * depending on the circumstances of the error.
   */
  gst_discoverer_signals[SIGNAL_DISCOVERED] =
      g_signal_new ("discovered", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
      G_STRUCT_OFFSET (GstDiscovererClass, discovered), NULL, NULL, NULL,
      G_TYPE_NONE, 2, GST_TYPE_DISCOVERER_INFO,
      G_TYPE_ERROR | G_SIGNAL_TYPE_STATIC_SCOPE);

  /**
   * GstDiscoverer::source-setup:
   * @discoverer: the #GstDiscoverer
   * @source: source element
   *
   * This signal is emitted after the source element has been created for, so
   * the URI being discovered, so it can be configured by setting additional
   * properties (e.g. set a proxy server for an http source, or set the device
   * and read speed for an audio cd source).
   *
   * This signal is usually emitted from the context of a GStreamer streaming
   * thread.
   */
  gst_discoverer_signals[SIGNAL_SOURCE_SETUP] =
      g_signal_new ("source-setup", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (GstDiscovererClass, source_setup),
      NULL, NULL, NULL, G_TYPE_NONE, 1, GST_TYPE_ELEMENT);

  /**
   * GstDiscoverer::load-serialized-info:
   * @discoverer: the #GstDiscoverer
   * @uri: THe URI to load the serialized info for
   *
   * Retrieves information about a URI from and external source of information,
   * like a cache file. This is used by the discoverer to speed up the
   * discovery.
   *
   * Returns: (nullable) (transfer full): The #GstDiscovererInfo representing
   * @uri, or %NULL if no information
   *
   * Since: 1.24
   */
  gst_discoverer_signals[SIGNAL_LOAD_SERIALIZED_INFO] =
      g_signal_new ("load-serialized-info", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (GstDiscovererClass,
          load_serialize_info), _gst_discoverer_info_accumulator, NULL, NULL,
      GST_TYPE_DISCOVERER_INFO, 1, G_TYPE_STRING);
}

static void
gst_discoverer_init (GstDiscoverer * dc)
{
  GstFormat format = GST_FORMAT_TIME;

  dc->priv = gst_discoverer_get_instance_private (dc);

  dc->priv->timeout = DEFAULT_PROP_TIMEOUT;
  dc->priv->use_cache = DEFAULT_PROP_USE_CACHE;
  dc->priv->async = FALSE;

  g_mutex_init (&dc->priv->lock);

  dc->priv->pending_subtitle_pads = 0;

  dc->priv->current_state = GST_STATE_NULL;
  dc->priv->target_state = GST_STATE_NULL;
  dc->priv->no_more_pads = FALSE;

  dc->priv->all_tags = NULL;
  dc->priv->global_tags = NULL;

  GST_LOG ("Creating pipeline");
  dc->priv->pipeline = (GstBin *) gst_pipeline_new ("Discoverer");
  GST_LOG_OBJECT (dc, "Creating uridecodebin");
  dc->priv->uridecodebin =
      gst_element_factory_make ("uridecodebin", "discoverer-uri");
  if (G_UNLIKELY (dc->priv->uridecodebin == NULL)) {
    GST_ERROR ("Can't create uridecodebin");
    return;
  }

  g_object_set (dc->priv->uridecodebin, "post-stream-topology", TRUE, NULL);

  GST_LOG_OBJECT (dc, "Adding uridecodebin to pipeline");
  gst_bin_add (dc->priv->pipeline, dc->priv->uridecodebin);

  dc->priv->pad_added_id =
      g_signal_connect_object (dc->priv->uridecodebin, "pad-added",
      G_CALLBACK (uridecodebin_pad_added_cb), dc, 0);
  dc->priv->pad_remove_id =
      g_signal_connect_object (dc->priv->uridecodebin, "pad-removed",
      G_CALLBACK (uridecodebin_pad_removed_cb), dc, 0);
  dc->priv->no_more_pads_id =
      g_signal_connect_object (dc->priv->uridecodebin, "no-more-pads",
      G_CALLBACK (uridecodebin_no_more_pads_cb), dc, 0);
  dc->priv->source_chg_id =
      g_signal_connect_object (dc->priv->uridecodebin, "notify::source",
      G_CALLBACK (uridecodebin_source_changed_cb), dc, 0);

  GST_LOG_OBJECT (dc, "Getting pipeline bus");
  dc->priv->bus = gst_pipeline_get_bus ((GstPipeline *) dc->priv->pipeline);

  dc->priv->bus_cb_id =
      g_signal_connect_object (dc->priv->bus, "message",
      G_CALLBACK (discoverer_bus_cb), dc, 0);

  GST_DEBUG_OBJECT (dc, "Done initializing Discoverer");

  /* create queries */
  dc->priv->seeking_query = gst_query_new_seeking (format);
}

static void
discoverer_reset (GstDiscoverer * dc)
{
  GST_DEBUG_OBJECT (dc, "Resetting");

  if (dc->priv->pending_uris) {
    g_list_foreach (dc->priv->pending_uris, (GFunc) g_free, NULL);
    g_list_free (dc->priv->pending_uris);
    dc->priv->pending_uris = NULL;
  }

  if (dc->priv->pipeline)
    gst_element_set_state ((GstElement *) dc->priv->pipeline, GST_STATE_NULL);
}

#define DISCONNECT_SIGNAL(o,i) G_STMT_START{           \
  if ((i) && g_signal_handler_is_connected ((o), (i))) \
    g_signal_handler_disconnect ((o), (i));            \
  (i) = 0;                                             \
}G_STMT_END

static void
gst_discoverer_dispose (GObject * obj)
{
  GstDiscoverer *dc = (GstDiscoverer *) obj;

  GST_DEBUG_OBJECT (dc, "Disposing");

  discoverer_reset (dc);

  if (G_LIKELY (dc->priv->pipeline)) {
    /* Workaround for bug #118536 */
    DISCONNECT_SIGNAL (dc->priv->uridecodebin, dc->priv->pad_added_id);
    DISCONNECT_SIGNAL (dc->priv->uridecodebin, dc->priv->pad_remove_id);
    DISCONNECT_SIGNAL (dc->priv->uridecodebin, dc->priv->no_more_pads_id);
    DISCONNECT_SIGNAL (dc->priv->uridecodebin, dc->priv->source_chg_id);
    DISCONNECT_SIGNAL (dc->priv->bus, dc->priv->bus_cb_id);

    /* pipeline was set to NULL in _reset */
    gst_object_unref (dc->priv->pipeline);
    if (dc->priv->bus)
      gst_object_unref (dc->priv->bus);

    dc->priv->pipeline = NULL;
    dc->priv->uridecodebin = NULL;
    dc->priv->bus = NULL;
  }

  gst_discoverer_stop (dc);

  if (dc->priv->seeking_query) {
    gst_query_unref (dc->priv->seeking_query);
    dc->priv->seeking_query = NULL;
  }

  G_OBJECT_CLASS (gst_discoverer_parent_class)->dispose (obj);
}

static void
gst_discoverer_finalize (GObject * obj)
{
  GstDiscoverer *dc = (GstDiscoverer *) obj;

  g_mutex_clear (&dc->priv->lock);

  G_OBJECT_CLASS (gst_discoverer_parent_class)->finalize (obj);
}

static void
gst_discoverer_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstDiscoverer *dc = (GstDiscoverer *) object;

  switch (prop_id) {
    case PROP_TIMEOUT:
      gst_discoverer_set_timeout (dc, g_value_get_uint64 (value));
      break;
    case PROP_USE_CACHE:
      DISCO_LOCK (dc);
      dc->priv->use_cache = g_value_get_boolean (value);
      DISCO_UNLOCK (dc);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_discoverer_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstDiscoverer *dc = (GstDiscoverer *) object;

  switch (prop_id) {
    case PROP_TIMEOUT:
      DISCO_LOCK (dc);
      g_value_set_uint64 (value, dc->priv->timeout);
      DISCO_UNLOCK (dc);
      break;
    case PROP_USE_CACHE:
      DISCO_LOCK (dc);
      g_value_set_boolean (value, dc->priv->use_cache);
      DISCO_UNLOCK (dc);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_discoverer_set_timeout (GstDiscoverer * dc, GstClockTime timeout)
{
  g_return_if_fail (GST_CLOCK_TIME_IS_VALID (timeout));

  GST_DEBUG_OBJECT (dc, "timeout : %" GST_TIME_FORMAT, GST_TIME_ARGS (timeout));

  /* FIXME : update current pending timeout if we're running */
  DISCO_LOCK (dc);
  dc->priv->timeout = timeout;
  DISCO_UNLOCK (dc);
}

static GstPadProbeReturn
_event_probe (GstPad * pad, GstPadProbeInfo * info, PrivateStream * ps)
{
  GstEvent *event = GST_PAD_PROBE_INFO_EVENT (info);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_TOC:{
      GstToc *tmp;

      gst_event_parse_toc (event, &tmp, NULL);
      GST_DEBUG_OBJECT (pad, "toc %" GST_PTR_FORMAT, tmp);
      DISCO_LOCK (ps->dc);
      ps->toc = tmp;
      if (G_LIKELY (ps->dc->priv->processing)) {
        GST_DEBUG_OBJECT (pad, "private stream %p toc %" GST_PTR_FORMAT, ps,
            tmp);
      } else
        GST_DEBUG_OBJECT (pad, "Dropping toc since preroll is done");
      DISCO_UNLOCK (ps->dc);
      break;
    }
    case GST_EVENT_STREAM_START:{
      const gchar *stream_id;

      gst_event_parse_stream_start (event, &stream_id);

      g_free (ps->stream_id);
      ps->stream_id = stream_id ? g_strdup (stream_id) : NULL;
      break;
    }
    default:
      break;
  }

  return GST_PAD_PROBE_OK;
}

static GstStaticCaps subtitle_caps =
    GST_STATIC_CAPS
    ("application/x-ssa; application/x-ass; application/x-kate");

static gboolean
is_subtitle_caps (const GstCaps * caps)
{
  GstCaps *subs_caps;
  GstStructure *s;
  const gchar *name;
  gboolean ret;

  s = gst_caps_get_structure (caps, 0);
  if (!s)
    return FALSE;

  name = gst_structure_get_name (s);
  if (g_str_has_prefix (name, "text/") ||
      g_str_has_prefix (name, "subpicture/") ||
      g_str_has_prefix (name, "subtitle/") ||
      g_str_has_prefix (name, "closedcaption/") ||
      g_str_has_prefix (name, "application/x-subtitle"))
    return TRUE;

  subs_caps = gst_static_caps_get (&subtitle_caps);
  ret = gst_caps_can_intersect (caps, subs_caps);
  gst_caps_unref (subs_caps);

  return ret;
}

static GstPadProbeReturn
got_subtitle_data (GstPad * pad, GstPadProbeInfo * info, GstDiscoverer * dc)
{
  GstMessage *msg;

  if (!(GST_IS_BUFFER (info->data) || (GST_IS_EVENT (info->data)
              && (GST_EVENT_TYPE ((GstEvent *) info->data) == GST_EVENT_GAP
                  || GST_EVENT_TYPE ((GstEvent *) info->data) ==
                  GST_EVENT_EOS))))
    return GST_PAD_PROBE_OK;


  DISCO_LOCK (dc);

  dc->priv->pending_subtitle_pads--;

  msg = gst_message_new_application (NULL,
      gst_structure_new_empty ("DiscovererDone"));
  gst_element_post_message ((GstElement *) dc->priv->pipeline, msg);

  DISCO_UNLOCK (dc);

  return GST_PAD_PROBE_REMOVE;

}

static void
uridecodebin_source_changed_cb (GstElement * uridecodebin,
    GParamSpec * pspec, GstDiscoverer * dc)
{
  GstElement *src;
  /* get a handle to the source */
  g_object_get (uridecodebin, pspec->name, &src, NULL);

  GST_DEBUG_OBJECT (dc, "got a new source %p", src);

  g_signal_emit (dc, gst_discoverer_signals[SIGNAL_SOURCE_SETUP], 0, src);
  gst_object_unref (src);
}

static void
uridecodebin_pad_added_cb (GstElement * uridecodebin, GstPad * pad,
    GstDiscoverer * dc)
{
  PrivateStream *ps;
  GstPad *sinkpad = NULL;
  GstCaps *caps;
  gchar *padname;
  gchar *tmpname;

  GST_DEBUG_OBJECT (dc, "pad %s:%s", GST_DEBUG_PAD_NAME (pad));

  DISCO_LOCK (dc);
  if (dc->priv->cleanup) {
    GST_WARNING_OBJECT (dc, "Cleanup, not adding pad");
    DISCO_UNLOCK (dc);
    return;
  }
  if (dc->priv->current_error) {
    GST_WARNING_OBJECT (dc, "Ongoing error, not adding more pads");
    DISCO_UNLOCK (dc);
    return;
  }
  ps = g_new0 (PrivateStream, 1);

  ps->dc = dc;
  ps->pad = pad;
  padname = gst_pad_get_name (pad);
  tmpname = g_strdup_printf ("discoverer-queue-%s", padname);
  ps->queue = gst_element_factory_make ("queue", tmpname);
  g_free (tmpname);
  tmpname = g_strdup_printf ("discoverer-sink-%s", padname);
  ps->sink = gst_element_factory_make ("fakesink", tmpname);
  g_free (tmpname);
  g_free (padname);

  if (G_UNLIKELY (ps->queue == NULL || ps->sink == NULL))
    goto error;

  g_object_set (ps->sink, "silent", TRUE, NULL);
  g_object_set (ps->queue, "max-size-buffers", 1, "silent", TRUE, NULL);

  sinkpad = gst_element_get_static_pad (ps->queue, "sink");
  if (sinkpad == NULL)
    goto error;

  caps = gst_pad_get_current_caps (pad);
  if (!caps) {
    GST_WARNING ("Couldn't get negotiated caps from %s:%s",
        GST_DEBUG_PAD_NAME (pad));
    caps = gst_pad_query_caps (pad, NULL);
  }

  if (caps && !gst_caps_is_empty (caps) && !gst_caps_is_any (caps)
      && is_subtitle_caps (caps)) {
    /* Subtitle streams are sparse and may not provide any information - don't
     * wait for data to preroll */
    ps->probe_id =
        gst_pad_add_probe (sinkpad, GST_PAD_PROBE_TYPE_DATA_DOWNSTREAM,
        (GstPadProbeCallback) got_subtitle_data, dc, NULL);
    g_object_set (ps->sink, "async", FALSE, NULL);
    dc->priv->pending_subtitle_pads++;
  }

  if (caps)
    gst_caps_unref (caps);

  gst_bin_add_many (dc->priv->pipeline, ps->queue, ps->sink, NULL);

  if (!gst_element_link_pads_full (ps->queue, "src", ps->sink, "sink",
          GST_PAD_LINK_CHECK_NOTHING))
    goto error;
  if (!gst_element_sync_state_with_parent (ps->sink))
    goto error;
  if (!gst_element_sync_state_with_parent (ps->queue))
    goto error;

  if (gst_pad_link_full (pad, sinkpad,
          GST_PAD_LINK_CHECK_NOTHING) != GST_PAD_LINK_OK)
    goto error;
  gst_object_unref (sinkpad);

  /* Add an event probe */
  gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM,
      (GstPadProbeCallback) _event_probe, ps, NULL);

  dc->priv->streams = g_list_append (dc->priv->streams, ps);
  DISCO_UNLOCK (dc);

  GST_DEBUG_OBJECT (dc, "Done handling pad");

  return;

error:
  GST_ERROR_OBJECT (dc, "Error while handling pad");
  if (sinkpad)
    gst_object_unref (sinkpad);
  if (ps->queue)
    gst_object_unref (ps->queue);
  if (ps->sink)
    gst_object_unref (ps->sink);
  g_free (ps);
  DISCO_UNLOCK (dc);
  return;
}

static void
uridecodebin_no_more_pads_cb (GstElement * uridecodebin, GstDiscoverer * dc)
{
  GstMessage *msg = gst_message_new_application (NULL,
      gst_structure_new_empty ("DiscovererDone"));

  DISCO_LOCK (dc);
  dc->priv->no_more_pads = TRUE;
  gst_element_post_message ((GstElement *) dc->priv->pipeline, msg);
  DISCO_UNLOCK (dc);
}

static void
uridecodebin_pad_removed_cb (GstElement * uridecodebin, GstPad * pad,
    GstDiscoverer * dc)
{
  GList *tmp;
  PrivateStream *ps;
  GstPad *sinkpad;

  GST_DEBUG_OBJECT (dc, "pad %s:%s", GST_DEBUG_PAD_NAME (pad));

  /* Find the PrivateStream */
  DISCO_LOCK (dc);
  for (tmp = dc->priv->streams; tmp; tmp = tmp->next) {
    ps = (PrivateStream *) tmp->data;
    if (ps->pad == pad)
      break;
  }

  if (tmp == NULL) {
    DISCO_UNLOCK (dc);
    GST_DEBUG ("The removed pad wasn't controlled by us !");
    return;
  }

  if (ps->probe_id)
    gst_pad_remove_probe (pad, ps->probe_id);

  dc->priv->streams = g_list_delete_link (dc->priv->streams, tmp);

  gst_element_set_state (ps->sink, GST_STATE_NULL);
  gst_element_set_state (ps->queue, GST_STATE_NULL);
  gst_element_unlink (ps->queue, ps->sink);

  sinkpad = gst_element_get_static_pad (ps->queue, "sink");
  gst_pad_unlink (pad, sinkpad);
  gst_object_unref (sinkpad);

  /* references removed here */
  gst_bin_remove_many (dc->priv->pipeline, ps->sink, ps->queue, NULL);

  DISCO_UNLOCK (dc);
  if (ps->tags) {
    gst_tag_list_unref (ps->tags);
  }
  if (ps->toc) {
    gst_toc_unref (ps->toc);
  }
  g_free (ps->stream_id);

  g_free (ps);

  GST_DEBUG ("Done handling pad");
}

static GstStructure *
collect_stream_information (GstDiscoverer * dc, PrivateStream * ps, guint idx)
{
  GstCaps *caps;
  GstStructure *st;
  gchar *stname;

  stname = g_strdup_printf ("stream-%02d", idx);
  st = gst_structure_new_empty (stname);
  g_free (stname);

  /* Get caps */
  caps = gst_pad_get_current_caps (ps->pad);
  if (!caps) {
    GST_WARNING ("Couldn't get negotiated caps from %s:%s",
        GST_DEBUG_PAD_NAME (ps->pad));
    caps = gst_pad_query_caps (ps->pad, NULL);
  }
  if (caps) {
    GST_DEBUG ("stream-%02d, got caps %" GST_PTR_FORMAT, idx, caps);
    gst_structure_id_set (st, _CAPS_QUARK, GST_TYPE_CAPS, caps, NULL);
    gst_caps_unref (caps);
  }
  if (ps->tags)
    gst_structure_id_set (st, _TAGS_QUARK, GST_TYPE_TAG_LIST, ps->tags, NULL);
  if (ps->toc)
    gst_structure_id_set (st, _TOC_QUARK, GST_TYPE_TOC, ps->toc, NULL);
  if (ps->stream_id)
    gst_structure_id_set (st, _STREAM_ID_QUARK, G_TYPE_STRING, ps->stream_id,
        NULL);

  return st;
}

/* takes ownership of new_tags, may replace *taglist with a new one */
static void
gst_discoverer_merge_and_replace_tags (GstTagList ** taglist,
    GstTagList * new_tags)
{
  if (new_tags == NULL)
    return;

  if (*taglist == NULL) {
    *taglist = new_tags;
    return;
  }

  gst_tag_list_insert (*taglist, new_tags, GST_TAG_MERGE_REPLACE);
  gst_tag_list_unref (new_tags);
}

static void
collect_common_information (GstDiscovererStreamInfo * info,
    const GstStructure * st)
{
  if (gst_structure_id_has_field (st, _TOC_QUARK)) {
    gst_structure_id_get (st, _TOC_QUARK, GST_TYPE_TOC, &info->toc, NULL);
  }

  if (gst_structure_id_has_field (st, _STREAM_ID_QUARK)) {
    gst_structure_id_get (st, _STREAM_ID_QUARK, G_TYPE_STRING, &info->stream_id,
        NULL);
  }
}

static GstDiscovererStreamInfo *
make_info (GstDiscovererStreamInfo * parent, GType type, GstCaps * caps)
{
  GstDiscovererStreamInfo *info;

  if (parent)
    info = gst_discoverer_stream_info_ref (parent);
  else {
    info = g_object_new (type, NULL);
    if (caps)
      info->caps = gst_caps_ref (caps);
  }
  return info;
}

/* Parses a set of caps and tags in st and populates a GstDiscovererStreamInfo
 * structure (parent, if !NULL, otherwise it allocates one)
 */
static GstDiscovererStreamInfo *
collect_information (GstDiscoverer * dc, const GstStructure * st,
    GstDiscovererStreamInfo * parent)
{
  GstPad *srcpad;
  GstCaps *caps = NULL;
  GstStructure *caps_st;
  GstTagList *tags_st;
  const gchar *name;
  gint tmp, tmp2;
  guint utmp;

  if (!st || (!gst_structure_id_has_field (st, _CAPS_QUARK)
          && !gst_structure_id_has_field (st, _ELEMENT_SRCPAD_QUARK))) {
    GST_WARNING ("Couldn't find caps !");
    return make_info (parent, GST_TYPE_DISCOVERER_STREAM_INFO, NULL);
  }

  if (gst_structure_id_get (st, _ELEMENT_SRCPAD_QUARK, GST_TYPE_PAD, &srcpad,
          NULL)) {
    caps = gst_pad_get_current_caps (srcpad);
    gst_object_unref (srcpad);
  }
  if (!caps) {
    gst_structure_id_get (st, _CAPS_QUARK, GST_TYPE_CAPS, &caps, NULL);
  }

  if (!caps || gst_caps_is_empty (caps) || gst_caps_is_any (caps)) {
    GST_WARNING ("Couldn't find caps !");
    if (caps)
      gst_caps_unref (caps);
    return make_info (parent, GST_TYPE_DISCOVERER_STREAM_INFO, NULL);
  }

  caps_st = gst_caps_get_structure (caps, 0);
  name = gst_structure_get_name (caps_st);

  if (g_str_has_prefix (name, "audio/")) {
    GstDiscovererAudioInfo *info;
    const gchar *format_str;
    guint64 channel_mask;

    info = (GstDiscovererAudioInfo *) make_info (parent,
        GST_TYPE_DISCOVERER_AUDIO_INFO, caps);

    if (gst_structure_get_int (caps_st, "rate", &tmp))
      info->sample_rate = (guint) tmp;

    if (gst_structure_get_int (caps_st, "channels", &tmp))
      info->channels = (guint) tmp;

    if (gst_structure_get (caps_st, "channel-mask", GST_TYPE_BITMASK,
            &channel_mask, NULL)) {
      info->channel_mask = channel_mask;
    } else if (info->channels) {
      info->channel_mask = gst_audio_channel_get_fallback_mask (info->channels);
    }

    /* FIXME: we only want to extract depth if raw audio is what's in the
     * container (i.e. not if there is a decoder involved) */
    format_str = gst_structure_get_string (caps_st, "format");
    if (format_str != NULL) {
      const GstAudioFormatInfo *finfo;
      GstAudioFormat format;

      format = gst_audio_format_from_string (format_str);
      finfo = gst_audio_format_get_info (format);
      if (finfo)
        info->depth = GST_AUDIO_FORMAT_INFO_DEPTH (finfo);
    }

    if (gst_structure_id_has_field (st, _TAGS_QUARK)) {
      gst_structure_id_get (st, _TAGS_QUARK, GST_TYPE_TAG_LIST, &tags_st, NULL);
      if (gst_tag_list_get_uint (tags_st, GST_TAG_BITRATE, &utmp) ||
          gst_tag_list_get_uint (tags_st, GST_TAG_NOMINAL_BITRATE, &utmp))
        info->bitrate = utmp;

      if (gst_tag_list_get_uint (tags_st, GST_TAG_MAXIMUM_BITRATE, &utmp))
        info->max_bitrate = utmp;

      /* FIXME: Is it worth it to remove the tags we've parsed? */
      gst_discoverer_merge_and_replace_tags (&info->parent.tags, tags_st);
    }

    collect_common_information (&info->parent, st);

    if (!info->language && ((GstDiscovererStreamInfo *) info)->tags) {
      gchar *language;
      if (gst_tag_list_get_string (((GstDiscovererStreamInfo *) info)->tags,
              GST_TAG_LANGUAGE_CODE, &language)) {
        info->language = language;
      }
    }

    gst_caps_unref (caps);
    return (GstDiscovererStreamInfo *) info;

  } else if (g_str_has_prefix (name, "video/") ||
      g_str_has_prefix (name, "image/")) {
    GstDiscovererVideoInfo *info;
    const gchar *caps_str;

    info = (GstDiscovererVideoInfo *) make_info (parent,
        GST_TYPE_DISCOVERER_VIDEO_INFO, caps);

    if (gst_structure_get_int (caps_st, "width", &tmp))
      info->width = (guint) tmp;
    if (gst_structure_get_int (caps_st, "height", &tmp))
      info->height = (guint) tmp;

    if (gst_structure_get_fraction (caps_st, "framerate", &tmp, &tmp2)) {
      info->framerate_num = (guint) tmp;
      info->framerate_denom = (guint) tmp2;
    } else {
      info->framerate_num = 0;
      info->framerate_denom = 1;
    }

    if (gst_structure_get_fraction (caps_st, "pixel-aspect-ratio", &tmp, &tmp2)) {
      info->par_num = (guint) tmp;
      info->par_denom = (guint) tmp2;
    } else {
      info->par_num = 1;
      info->par_denom = 1;
    }

    /* FIXME: we only want to extract depth if raw video is what's in the
     * container (i.e. not if there is a decoder involved) */
    caps_str = gst_structure_get_string (caps_st, "format");
    if (caps_str != NULL) {
      const GstVideoFormatInfo *finfo;
      GstVideoFormat format;

      format = gst_video_format_from_string (caps_str);
      finfo = gst_video_format_get_info (format);
      if (finfo)
        info->depth = finfo->bits * finfo->n_components;
    }

    caps_str = gst_structure_get_string (caps_st, "interlace-mode");
    if (!caps_str || strcmp (caps_str, "progressive") == 0)
      info->interlaced = FALSE;
    else
      info->interlaced = TRUE;

    if (gst_structure_id_has_field (st, _TAGS_QUARK)) {
      gst_structure_id_get (st, _TAGS_QUARK, GST_TYPE_TAG_LIST, &tags_st, NULL);
      if (gst_tag_list_get_uint (tags_st, GST_TAG_BITRATE, &utmp) ||
          gst_tag_list_get_uint (tags_st, GST_TAG_NOMINAL_BITRATE, &utmp))
        info->bitrate = utmp;

      if (gst_tag_list_get_uint (tags_st, GST_TAG_MAXIMUM_BITRATE, &utmp))
        info->max_bitrate = utmp;

      /* FIXME: Is it worth it to remove the tags we've parsed? */
      gst_discoverer_merge_and_replace_tags (&info->parent.tags, tags_st);
    }

    collect_common_information (&info->parent, st);

    gst_caps_unref (caps);
    return (GstDiscovererStreamInfo *) info;

  } else if (is_subtitle_caps (caps)) {
    GstDiscovererSubtitleInfo *info;

    info = (GstDiscovererSubtitleInfo *) make_info (parent,
        GST_TYPE_DISCOVERER_SUBTITLE_INFO, caps);

    if (gst_structure_id_has_field (st, _TAGS_QUARK)) {
      const gchar *language;

      gst_structure_id_get (st, _TAGS_QUARK, GST_TYPE_TAG_LIST, &tags_st, NULL);

      language = gst_structure_get_string (caps_st, GST_TAG_LANGUAGE_CODE);
      if (language)
        info->language = g_strdup (language);

      /* FIXME: Is it worth it to remove the tags we've parsed? */
      gst_discoverer_merge_and_replace_tags (&info->parent.tags, tags_st);
    }

    collect_common_information (&info->parent, st);

    if (!info->language && ((GstDiscovererStreamInfo *) info)->tags) {
      gchar *language;
      if (gst_tag_list_get_string (((GstDiscovererStreamInfo *) info)->tags,
              GST_TAG_LANGUAGE_CODE, &language)) {
        info->language = language;
      }
    }

    gst_caps_unref (caps);
    return (GstDiscovererStreamInfo *) info;

  } else {
    /* None of the above - populate what information we can */
    GstDiscovererStreamInfo *info;

    info = make_info (parent, GST_TYPE_DISCOVERER_STREAM_INFO, caps);

    if (gst_structure_id_get (st, _TAGS_QUARK, GST_TYPE_TAG_LIST, &tags_st,
            NULL)) {
      gst_discoverer_merge_and_replace_tags (&info->tags, tags_st);
    }

    collect_common_information (info, st);

    gst_caps_unref (caps);
    return info;
  }

}

static GstStructure *
find_stream_for_node (GstDiscoverer * dc, const GstStructure * topology)
{
  GstPad *pad;
  GstPad *target_pad = NULL;
  GstStructure *st = NULL;
  PrivateStream *ps;
  guint i;
  GList *tmp;

  if (!dc->priv->streams) {
    return NULL;
  }

  if (!gst_structure_id_has_field (topology, _TOPOLOGY_PAD_QUARK)) {
    GST_DEBUG ("Could not find pad for node %" GST_PTR_FORMAT, topology);
    return NULL;
  }

  gst_structure_id_get (topology, _TOPOLOGY_PAD_QUARK,
      GST_TYPE_PAD, &pad, NULL);

  for (i = 0, tmp = dc->priv->streams; tmp; tmp = tmp->next, i++) {
    ps = (PrivateStream *) tmp->data;

    target_pad = gst_ghost_pad_get_target (GST_GHOST_PAD (ps->pad));
    if (target_pad == NULL)
      continue;
    gst_object_unref (target_pad);

    if (target_pad == pad)
      break;
  }

  if (tmp)
    st = collect_stream_information (dc, ps, i);

  gst_object_unref (pad);

  return st;
}

/* this can fail due to {framed,parsed}={TRUE,FALSE} differences, thus we filter
 * the parent */
static gboolean
child_is_same_stream (const GstCaps * _parent, const GstCaps * child)
{
  GstCaps *parent;
  gboolean res;

  if (_parent == child)
    return TRUE;
  if (!_parent)
    return FALSE;
  if (!child)
    return FALSE;

  parent = copy_and_clean_caps (_parent);
  res = gst_caps_can_intersect (parent, child);
  gst_caps_unref (parent);
  return res;
}


static gboolean
child_is_raw_stream (const GstCaps * parent, const GstCaps * child)
{
  const GstStructure *st1, *st2;
  const gchar *name1, *name2;

  if (parent == child)
    return TRUE;
  if (!parent)
    return FALSE;
  if (!child)
    return FALSE;

  st1 = gst_caps_get_structure (parent, 0);
  name1 = gst_structure_get_name (st1);
  st2 = gst_caps_get_structure (child, 0);
  name2 = gst_structure_get_name (st2);

  if ((g_str_has_prefix (name1, "audio/") &&
          g_str_has_prefix (name2, "audio/x-raw")) ||
      ((g_str_has_prefix (name1, "video/") ||
              g_str_has_prefix (name1, "image/")) &&
          g_str_has_prefix (name2, "video/x-raw"))) {
    /* child is the "raw" sub-stream corresponding to parent */
    return TRUE;
  }

  if (is_subtitle_caps (parent))
    return TRUE;

  return FALSE;
}

/* If a parent is non-NULL, collected stream information will be appended to it
 * (and where the information exists, it will be overridden)
 */
static GstDiscovererStreamInfo *
parse_stream_topology (GstDiscoverer * dc, const GstStructure * topology,
    GstDiscovererStreamInfo * parent)
{
  GstDiscovererStreamInfo *res = NULL;
  GstCaps *caps = NULL;
  const GValue *nval = NULL;

  GST_DEBUG ("parsing: %" GST_PTR_FORMAT, topology);

  nval = gst_structure_get_value (topology, "next");

  if (nval == NULL || GST_VALUE_HOLDS_STRUCTURE (nval)) {
    GstStructure *st = find_stream_for_node (dc, topology);
    gboolean add_to_list = TRUE;

    if (st) {
      res = collect_information (dc, st, parent);
      gst_structure_free (st);
    } else {
      /* Didn't find a stream structure, so let's just use the caps we have */
      res = collect_information (dc, topology, parent);
    }

    if (nval == NULL) {
      /* FIXME : aggregate with information from main streams */
      GST_DEBUG ("Couldn't find 'next' ! might be the last entry");
    } else {
      GstPad *srcpad;

      st = (GstStructure *) gst_value_get_structure (nval);

      GST_DEBUG ("next is a structure %" GST_PTR_FORMAT, st);

      if (!parent)
        parent = res;

      if (gst_structure_id_get (st, _ELEMENT_SRCPAD_QUARK, GST_TYPE_PAD,
              &srcpad, NULL)) {
        caps = gst_pad_get_current_caps (srcpad);
        gst_object_unref (srcpad);
      }
      if (!caps) {
        gst_structure_id_get (st, _CAPS_QUARK, GST_TYPE_CAPS, &caps, NULL);
      }

      if (caps) {
        if (child_is_same_stream (parent->caps, caps)) {
          /* We sometimes get an extra sub-stream from the parser. If this is
           * the case, we just replace the parent caps with this stream's caps
           * since they might contain more information */
          gst_caps_replace (&parent->caps, caps);

          parse_stream_topology (dc, st, parent);
          add_to_list = FALSE;
        } else if (child_is_raw_stream (parent->caps, caps)) {
          /* This is the "raw" stream corresponding to the parent. This
           * contains more information than the parent, tags etc. */
          parse_stream_topology (dc, st, parent);
          add_to_list = FALSE;
        } else {
          GstDiscovererStreamInfo *next = parse_stream_topology (dc, st, NULL);
          res->next = next;
          next->previous = res;
        }
        gst_caps_unref (caps);
      }
    }

    if (add_to_list) {
      res->stream_number = dc->priv->current_info->stream_count++;
      dc->priv->current_info->stream_list =
          g_list_append (dc->priv->current_info->stream_list, res);
    } else {
      gst_discoverer_stream_info_unref (res);
    }

  } else if (GST_VALUE_HOLDS_LIST (nval)) {
    guint i, len;
    GstDiscovererContainerInfo *cont;
    GstPad *srcpad;

    if (gst_structure_id_get (topology, _ELEMENT_SRCPAD_QUARK, GST_TYPE_PAD,
            &srcpad, NULL)) {
      caps = gst_pad_get_current_caps (srcpad);
      gst_object_unref (srcpad);
    }
    if (!caps) {
      gst_structure_id_get (topology, _CAPS_QUARK, GST_TYPE_CAPS, &caps, NULL);
    }

    if (!caps)
      GST_WARNING ("Couldn't find caps !");

    len = gst_value_list_get_size (nval);
    GST_DEBUG ("next is a list of %d entries", len);

    cont = (GstDiscovererContainerInfo *)
        g_object_new (GST_TYPE_DISCOVERER_CONTAINER_INFO, NULL);
    cont->parent.caps = caps;
    if (dc->priv->global_tags)
      cont->tags = gst_tag_list_ref (dc->priv->global_tags);
    res = (GstDiscovererStreamInfo *) cont;

    for (i = 0; i < len; i++) {
      const GValue *subv = gst_value_list_get_value (nval, i);
      const GstStructure *subst = gst_value_get_structure (subv);
      GstDiscovererStreamInfo *substream;

      GST_DEBUG ("%d %" GST_PTR_FORMAT, i, subst);

      substream = parse_stream_topology (dc, subst, NULL);

      substream->previous = res;
      cont->streams =
          g_list_append (cont->streams,
          gst_discoverer_stream_info_ref (substream));
    }
  }

  return res;
}

/* Required DISCO_LOCK to be taken, and will release it */
static void
setup_next_uri_locked (GstDiscoverer * dc)
{
  if (dc->priv->pending_uris != NULL) {
    gboolean ready = _setup_locked (dc);
    DISCO_UNLOCK (dc);

    if (!ready) {
      /* Start timeout */
      if (dc->priv->processing)
        handle_current_async (dc);
    } else {
      g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
          (GSourceFunc) emit_discovererd_and_next, gst_object_ref (dc),
          gst_object_unref);
    }
  } else {
    /* We're done ! */
    DISCO_UNLOCK (dc);
    g_signal_emit (dc, gst_discoverer_signals[SIGNAL_FINISHED], 0);
  }
}

static GstDiscovererInfo *
_ensure_info_tags (GstDiscoverer * dc)
{
  GstDiscovererInfo *info = dc->priv->current_info;

  if (dc->priv->all_tags)
    info->tags = dc->priv->all_tags;
  dc->priv->all_tags = NULL;
  return info;
}

static void
emit_discovererd (GstDiscoverer * dc)
{
  GstDiscovererInfo *info = _ensure_info_tags (dc);
  GST_DEBUG_OBJECT (dc, "Emitting 'discoverered' %s", info->uri);
  g_signal_emit (dc, gst_discoverer_signals[SIGNAL_DISCOVERED], 0,
      info, dc->priv->current_error);
  /* Clients get a copy of current_info since it is a boxed type */
  gst_discoverer_info_unref (dc->priv->current_info);
  dc->priv->current_info = NULL;
}

static gboolean
emit_discovererd_and_next (GstDiscoverer * dc)
{
  emit_discovererd (dc);

  DISCO_LOCK (dc);
  setup_next_uri_locked (dc);

  return G_SOURCE_REMOVE;
}

/* Called when pipeline is pre-rolled */
static void
discoverer_collect (GstDiscoverer * dc)
{
  GST_DEBUG ("Collecting information");

  /* Stop the timeout handler if present */
  if (dc->priv->timeout_source) {
    g_source_destroy (dc->priv->timeout_source);
    g_source_unref (dc->priv->timeout_source);
    dc->priv->timeout_source = NULL;
  }

  if (dc->priv->use_cache && dc->priv->current_info
      && dc->priv->current_info->from_cache) {
    GST_DEBUG_OBJECT (dc,
        "Nothing to collect as the info was built from" " the cache");
    return;
  }

  if (dc->priv->streams) {
    /* FIXME : Make this querying optional */
    if (TRUE) {
      GstElement *pipeline = (GstElement *) dc->priv->pipeline;
      gint64 dur;

      GST_DEBUG ("Attempting to query duration");

      if (gst_element_query_duration (pipeline, GST_FORMAT_TIME, &dur)) {
        GST_DEBUG ("Got duration %" GST_TIME_FORMAT, GST_TIME_ARGS (dur));
        dc->priv->current_info->duration = (guint64) dur;
      } else if (dc->priv->current_info->result != GST_DISCOVERER_ERROR) {
        GstStateChangeReturn sret;
        /* Note: We don't switch to PLAYING if we previously saw an ERROR since
         * the state of various element isn't guaranteed anymore */

        /* Some parsers may not even return a rough estimate right away, e.g.
         * because they've only processed a single frame so far, so if we
         * didn't get a duration the first time, spin a bit and try again.
         * Ugly, but still better than making parsers or other elements return
         * completely bogus values. We need some API extensions to solve this
         * better. */
        GST_INFO ("No duration yet, try a bit harder..");
        /* Make sure we don't add/remove elements while switching to PLAYING itself */
        DISCO_LOCK (dc);
        sret = gst_element_set_state (pipeline, GST_STATE_PLAYING);
        DISCO_UNLOCK (dc);
        if (sret != GST_STATE_CHANGE_FAILURE) {
          int i;

          for (i = 0; i < 2; ++i) {
            g_usleep (G_USEC_PER_SEC / 20);
            if (gst_element_query_duration (pipeline, GST_FORMAT_TIME, &dur)
                && dur > 0) {
              GST_DEBUG ("Got duration %" GST_TIME_FORMAT, GST_TIME_ARGS (dur));
              dc->priv->current_info->duration = (guint64) dur;
              break;
            }
          }
          gst_element_set_state (pipeline, GST_STATE_PAUSED);
        }
      }

      if (dc->priv->seeking_query) {
        if (gst_element_query (pipeline, dc->priv->seeking_query)) {
          GstFormat format;
          gboolean seekable;

          gst_query_parse_seeking (dc->priv->seeking_query, &format,
              &seekable, NULL, NULL);
          if (format == GST_FORMAT_TIME) {
            GST_DEBUG ("Got seekable %d", seekable);
            dc->priv->current_info->seekable = seekable;
          }
        }
      }
    }

    if (dc->priv->target_state == GST_STATE_PAUSED)
      dc->priv->current_info->live = FALSE;
    else
      dc->priv->current_info->live = TRUE;

    if (dc->priv->current_topology) {
      dc->priv->current_info->stream_count = 1;
      dc->priv->current_info->stream_info = parse_stream_topology (dc,
          dc->priv->current_topology, NULL);
      if (dc->priv->current_info->stream_info)
        dc->priv->current_info->stream_info->stream_number = 0;
    }

    /*
     * Images need some special handling. They do not have a duration, have
     * caps named image/<foo> (th exception being MJPEG video which is also
     * type image/jpeg), and should consist of precisely one stream (actually
     * initially there are 2, the image and raw stream, but we squash these
     * while parsing the stream topology). At some point, if we find that these
     * conditions are not sufficient, we can count the number of decoders and
     * parsers in the chain, and if there's more than one decoder, or any
     * parser at all, we should not mark this as an image.
     */
    if (dc->priv->current_info->duration == 0 &&
        dc->priv->current_info->stream_info != NULL &&
        dc->priv->current_info->stream_info->next == NULL) {
      GstDiscovererStreamInfo *stream_info;
      GstStructure *st;

      stream_info = dc->priv->current_info->stream_info;
      st = gst_caps_get_structure (stream_info->caps, 0);

      if (g_str_has_prefix (gst_structure_get_name (st), "image/"))
        ((GstDiscovererVideoInfo *) stream_info)->is_image = TRUE;
    }
  }

  if (dc->priv->use_cache && dc->priv->current_info->cachefile &&
      dc->priv->current_info->result == GST_DISCOVERER_OK) {
    GVariant *variant = gst_discoverer_info_to_variant (dc->priv->current_info,
        GST_DISCOVERER_SERIALIZE_ALL);

    g_file_set_contents (dc->priv->current_info->cachefile,
        g_variant_get_data (variant), g_variant_get_size (variant), NULL);
    g_variant_unref (variant);
  }

  if (dc->priv->async)
    emit_discovererd (dc);
}

static void
get_async_cb (gpointer cb_data, GSource * source, GSourceFunc * func,
    gpointer * data)
{
  *func = (GSourceFunc) async_timeout_cb;
  *data = cb_data;
}

/* Wrapper since GSourceCallbackFuncs don't expect a return value from ref() */
static void
_void_g_object_ref (gpointer object)
{
  g_object_ref (G_OBJECT (object));
}

static void
handle_current_async (GstDiscoverer * dc)
{
  GSource *source;
  static GSourceCallbackFuncs cb_funcs = {
    _void_g_object_ref,
    g_object_unref,
    get_async_cb,
  };

  /* Attach a timeout to the main context */
  source = g_timeout_source_new (dc->priv->timeout / GST_MSECOND);
  g_source_set_callback_indirect (source, g_object_ref (dc), &cb_funcs);
  g_source_attach (source, dc->priv->ctx);
  dc->priv->timeout_source = source;
}


/* Returns TRUE if processing should stop */
static gboolean
handle_message (GstDiscoverer * dc, GstMessage * msg)
{
  gboolean done = FALSE;
  const gchar *dump_name = NULL;

  GST_DEBUG_OBJECT (GST_MESSAGE_SRC (msg), "got a %s message",
      GST_MESSAGE_TYPE_NAME (msg));

  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_ERROR:{
      GError *gerr;
      gchar *debug;

      gst_message_parse_error (msg, &gerr, &debug);
      GST_WARNING_OBJECT (GST_MESSAGE_SRC (msg),
          "Got an error [debug:%s], [message:%s]", debug, gerr->message);
      dc->priv->current_error = gerr;
      g_free (debug);

      /* We need to stop */
      done = TRUE;
      dump_name = "gst-discoverer-error";

      /* Don't override missing plugin result code for missing plugin errors */
      if (dc->priv->current_info->result != GST_DISCOVERER_MISSING_PLUGINS ||
          (!g_error_matches (gerr, GST_CORE_ERROR,
                  GST_CORE_ERROR_MISSING_PLUGIN) &&
              !g_error_matches (gerr, GST_STREAM_ERROR,
                  GST_STREAM_ERROR_CODEC_NOT_FOUND))) {
        GST_DEBUG ("Setting result to ERROR");
        dc->priv->current_info->result = GST_DISCOVERER_ERROR;
      }
    }
      break;

    case GST_MESSAGE_WARNING:{
      GError *err;
      gchar *debug = NULL;

      gst_message_parse_warning (msg, &err, &debug);
      GST_WARNING_OBJECT (GST_MESSAGE_SRC (msg),
          "Got a warning [debug:%s], [message:%s]", debug, err->message);
      g_clear_error (&err);
      g_free (debug);
      dump_name = "gst-discoverer-warning";
      break;
    }

    case GST_MESSAGE_EOS:
      GST_DEBUG ("Got EOS !");
      done = TRUE;
      dump_name = "gst-discoverer-eos";
      break;

    case GST_MESSAGE_APPLICATION:{
      const gchar *name =
          gst_structure_get_name (gst_message_get_structure (msg));

      if (g_strcmp0 (name, "DiscovererDone"))
        break;

      /* Maybe we already reached the target state, and all we're waiting for
       * is either the subtitle tags or no_more_pads
       */
      DISCO_LOCK (dc);
      if (dc->priv->pending_subtitle_pads == 0)
        done = dc->priv->no_more_pads
            && dc->priv->target_state == dc->priv->current_state;
      DISCO_UNLOCK (dc);

      if (done)
        dump_name = "gst-discoverer-application-message";
    }
      break;

    case GST_MESSAGE_STATE_CHANGED:{
      GstState old, new, pending;

      gst_message_parse_state_changed (msg, &old, &new, &pending);
      if (GST_MESSAGE_SRC (msg) == (GstObject *) dc->priv->pipeline) {
        DISCO_LOCK (dc);
        dc->priv->current_state = new;

        if (dc->priv->pending_subtitle_pads == 0)
          done = dc->priv->no_more_pads
              && dc->priv->target_state == dc->priv->current_state;
        /* Else we should get unblocked in GST_MESSAGE_APPLICATION */

        DISCO_UNLOCK (dc);
      }

      if (done)
        dump_name = "gst-discoverer-target-state";
    }
      break;

    case GST_MESSAGE_ELEMENT:
    {
      GQuark sttype;
      const GstStructure *structure;

      structure = gst_message_get_structure (msg);
      sttype = gst_structure_get_name_id (structure);
      GST_DEBUG_OBJECT (GST_MESSAGE_SRC (msg),
          "structure %" GST_PTR_FORMAT, structure);
      if (sttype == _MISSING_PLUGIN_QUARK) {
        GST_DEBUG_OBJECT (GST_MESSAGE_SRC (msg),
            "Setting result to MISSING_PLUGINS");
        dc->priv->current_info->result = GST_DISCOVERER_MISSING_PLUGINS;
        /* FIXME 2.0 Remove completely the ->misc
         * Keep the old behaviour for now.
         */
        if (dc->priv->current_info->misc)
          gst_structure_free (dc->priv->current_info->misc);
        dc->priv->current_info->misc = gst_structure_copy (structure);
        g_ptr_array_add (dc->priv->current_info->missing_elements_details,
            gst_missing_plugin_message_get_installer_detail (msg));
      } else if (sttype == _STREAM_TOPOLOGY_QUARK) {
        if (dc->priv->current_topology)
          gst_structure_free (dc->priv->current_topology);
        dc->priv->current_topology = gst_structure_copy (structure);
      }
    }
      break;

    case GST_MESSAGE_TAG:
    {
      GstTagList *tl, *tmp = NULL;
      GstTagScope scope;

      gst_message_parse_tag (msg, &tl);
      scope = gst_tag_list_get_scope (tl);
      GST_DEBUG_OBJECT (GST_MESSAGE_SRC (msg), "Got tags %" GST_PTR_FORMAT, tl);

      tmp = gst_tag_list_merge (dc->priv->all_tags, tl, GST_TAG_MERGE_APPEND);
      if (dc->priv->all_tags)
        gst_tag_list_unref (dc->priv->all_tags);
      dc->priv->all_tags = tmp;

      if (scope == GST_TAG_SCOPE_STREAM) {
        for (GList * curr = dc->priv->streams; curr; curr = curr->next) {
          PrivateStream *ps = (PrivateStream *) curr->data;
          if (GST_MESSAGE_SRC (msg) == GST_OBJECT_CAST (ps->sink)) {
            tmp = gst_tag_list_merge (ps->tags, tl, GST_TAG_MERGE_APPEND);
            if (ps->tags)
              gst_tag_list_unref (ps->tags);
            ps->tags = tmp;
            GST_DEBUG_OBJECT (ps->pad, "tags %" GST_PTR_FORMAT, ps->tags);
            break;
          }
        }
      } else {
        tmp =
            gst_tag_list_merge (dc->priv->global_tags, tl,
            GST_TAG_MERGE_APPEND);
        if (dc->priv->global_tags)
          gst_tag_list_unref (dc->priv->global_tags);
        dc->priv->global_tags = tmp;
      }
      gst_tag_list_unref (tl);
    }
      break;
    case GST_MESSAGE_TOC:
    {
      GstToc *tmp;

      gst_message_parse_toc (msg, &tmp, NULL);
      GST_DEBUG_OBJECT (GST_MESSAGE_SRC (msg), "Got toc %" GST_PTR_FORMAT, tmp);
      if (dc->priv->current_info->toc)
        gst_toc_unref (dc->priv->current_info->toc);
      dc->priv->current_info->toc = tmp;        /* transfer ownership */
      GST_DEBUG_OBJECT (GST_MESSAGE_SRC (msg), "Current info %p, toc %"
          GST_PTR_FORMAT, dc->priv->current_info, tmp);
    }
      break;

    default:
      break;
  }

  if (dump_name != NULL) {
    /* dump graph when done or for warnings */
    GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (dc->priv->pipeline),
        GST_DEBUG_GRAPH_SHOW_ALL, dump_name);
  }
  return done;
}

static void
handle_current_sync (GstDiscoverer * dc)
{
  GTimer *timer;
  gdouble deadline = ((gdouble) dc->priv->timeout) / GST_SECOND;
  GstMessage *msg;
  gboolean done = FALSE;

  timer = g_timer_new ();
  g_timer_start (timer);

  do {
    /* poll bus with timeout */
    /* FIXME : make the timeout more fine-tuned */
    if ((msg = gst_bus_timed_pop (dc->priv->bus, GST_SECOND / 2))) {
      done = handle_message (dc, msg);
      gst_message_unref (msg);
    }
  } while (!done && (g_timer_elapsed (timer, NULL) < deadline));

  /* return result */
  if (!done) {
    GST_DEBUG ("we timed out! Setting result to TIMEOUT");
    dc->priv->current_info->result = GST_DISCOVERER_TIMEOUT;
  }

  DISCO_LOCK (dc);
  dc->priv->processing = FALSE;
  DISCO_UNLOCK (dc);


  GST_DEBUG ("Done");

  g_timer_stop (timer);
  g_timer_destroy (timer);
}

static gchar *
_serialized_info_get_path (GstDiscoverer * dc, gchar * uri)
{
  GChecksum *cs = NULL;
  GStatBuf file_status;
  gchar *location = NULL, *res = NULL, *cache_dir = NULL, *tmp = NULL,
      *protocol = gst_uri_get_protocol (uri), hash_dirname[3] = "00";
  const gchar *checksum;

  if (g_ascii_strcasecmp (protocol, "file") != 0) {
    GST_DEBUG_OBJECT (dc, "Can not work with serialized DiscovererInfo"
        " on non local files - protocol: %s", protocol);

    goto done;
  }

  location = gst_uri_get_location (uri);
  if (g_stat (location, &file_status) < 0) {
    GST_DEBUG_OBJECT (dc, "Could not get stat for file: %s", location);

    goto done;
  }

  tmp = g_strdup_printf ("%s-%" G_GSIZE_FORMAT "-%" G_GINT64_FORMAT,
      location, (gsize) file_status.st_size, (gint64) file_status.st_mtime);
  cs = g_checksum_new (G_CHECKSUM_SHA1);
  g_checksum_update (cs, (const guchar *) tmp, strlen (tmp));
  checksum = g_checksum_get_string (cs);

  hash_dirname[0] = checksum[0];
  hash_dirname[1] = checksum[1];
  cache_dir =
      g_build_filename (g_get_user_cache_dir (), "gstreamer-" GST_API_VERSION,
      CACHE_DIRNAME, hash_dirname, NULL);
  g_mkdir_with_parents (cache_dir, 0777);

  res = g_build_filename (cache_dir, &checksum[2], NULL);

done:
  g_checksum_free (cs);
  g_free (cache_dir);
  g_free (location);
  g_free (tmp);
  g_free (protocol);

  return res;
}

static GstDiscovererInfo *
_get_info_from_cachefile (GstDiscoverer * dc, gchar * cachefile)
{
  gchar *data;
  gsize length;

  if (g_file_get_contents (cachefile, &data, &length, NULL)) {
    GstDiscovererInfo *info = NULL;
    GVariant *variant =
        g_variant_new_from_data (G_VARIANT_TYPE ("v"), data, length,
        TRUE, NULL, NULL);

    info = gst_discoverer_info_from_variant (variant);
    g_variant_unref (variant);

    if (info) {
      info->cachefile = cachefile;
      info->from_cache = (gpointer) 0x01;
    } else {
      g_free (cachefile);
    }

    GST_INFO_OBJECT (dc, "Got info from cache: %p", info);
    g_free (data);

    return info;
  } else {
    g_free (cachefile);
  }

  return NULL;
}

static GstDiscovererInfo *
load_serialized_info (GstDiscoverer * dc, gchar * uri)
{
  GstDiscovererInfo *res = NULL;

  if (dc->priv->use_cache) {
    gchar *cachefile = _serialized_info_get_path (dc, uri);

    if (cachefile) {
      res = _get_info_from_cachefile (dc, cachefile);
    }
  }

  return res;
}

static gboolean
_setup_locked (GstDiscoverer * dc)
{
  GstStateChangeReturn ret;
  gchar *uri = (gchar *) dc->priv->pending_uris->data;

  dc->priv->pending_uris =
      g_list_delete_link (dc->priv->pending_uris, dc->priv->pending_uris);


  GST_DEBUG ("Setting up");

  g_signal_emit (dc, gst_discoverer_signals[SIGNAL_LOAD_SERIALIZED_INFO], 0,
      uri, &dc->priv->current_info);
  if (dc->priv->current_info) {
    /* Make sure the URI is exactly what the user passed in */
    g_free (dc->priv->current_info->uri);
    dc->priv->current_info->uri = uri;

    dc->priv->processing = FALSE;
    dc->priv->target_state = GST_STATE_NULL;

    return TRUE;
  }

  /* Pop URI off the pending URI list */
  dc->priv->current_info =
      (GstDiscovererInfo *) g_object_new (GST_TYPE_DISCOVERER_INFO, NULL);
  if (dc->priv->use_cache)
    dc->priv->current_info->cachefile = _serialized_info_get_path (dc, uri);
  dc->priv->current_info->uri = uri;

  /* set uri on uridecodebin */
  g_object_set (dc->priv->uridecodebin, "uri", dc->priv->current_info->uri,
      NULL);

  GST_DEBUG ("Current is now %s", dc->priv->current_info->uri);

  dc->priv->processing = TRUE;

  dc->priv->target_state = GST_STATE_PAUSED;

  /* set pipeline to PAUSED */
  DISCO_UNLOCK (dc);
  GST_DEBUG ("Setting pipeline to PAUSED");
  ret =
      gst_element_set_state ((GstElement *) dc->priv->pipeline,
      dc->priv->target_state);

  if (ret == GST_STATE_CHANGE_NO_PREROLL) {
    GST_DEBUG ("Source is live, switching to PLAYING");
    dc->priv->target_state = GST_STATE_PLAYING;
    ret =
        gst_element_set_state ((GstElement *) dc->priv->pipeline,
        dc->priv->target_state);
  }
  DISCO_LOCK (dc);


  GST_DEBUG_OBJECT (dc, "Pipeline going to PAUSED : %s",
      gst_element_state_change_return_get_name (ret));

  return FALSE;
}

static void
discoverer_cleanup (GstDiscoverer * dc)
{
  GST_DEBUG ("Cleaning up");

  DISCO_LOCK (dc);
  dc->priv->cleanup = TRUE;
  DISCO_UNLOCK (dc);

  gst_bus_set_flushing (dc->priv->bus, TRUE);

  DISCO_LOCK (dc);
  if (dc->priv->current_error) {
    g_error_free (dc->priv->current_error);
    DISCO_UNLOCK (dc);
    gst_element_set_state ((GstElement *) dc->priv->pipeline, GST_STATE_NULL);
  } else {
    DISCO_UNLOCK (dc);
  }

  gst_element_set_state ((GstElement *) dc->priv->pipeline, GST_STATE_READY);
  gst_bus_set_flushing (dc->priv->bus, FALSE);

  DISCO_LOCK (dc);
  dc->priv->current_error = NULL;
  if (dc->priv->current_topology) {
    gst_structure_free (dc->priv->current_topology);
    dc->priv->current_topology = NULL;
  }

  dc->priv->current_info = NULL;

  if (dc->priv->all_tags) {
    gst_tag_list_unref (dc->priv->all_tags);
    dc->priv->all_tags = NULL;
  }

  if (dc->priv->global_tags) {
    gst_tag_list_unref (dc->priv->global_tags);
    dc->priv->global_tags = NULL;
  }

  dc->priv->pending_subtitle_pads = 0;

  dc->priv->current_state = GST_STATE_NULL;
  dc->priv->target_state = GST_STATE_NULL;
  dc->priv->no_more_pads = FALSE;
  dc->priv->cleanup = FALSE;


  /* Try popping the next uri */
  if (dc->priv->async) {
    setup_next_uri_locked (dc);
  } else
    DISCO_UNLOCK (dc);

  GST_DEBUG ("out");
}

static void
discoverer_bus_cb (GstBus * bus, GstMessage * msg, GstDiscoverer * dc)
{
  if (dc->priv->processing) {
    if (handle_message (dc, msg)) {
      GST_DEBUG ("Stopping asynchronously");
      /* Serialise with _event_probe() */
      DISCO_LOCK (dc);
      dc->priv->processing = FALSE;
      DISCO_UNLOCK (dc);
      discoverer_collect (dc);
      discoverer_cleanup (dc);
    }
  }
}

static gboolean
async_timeout_cb (GstDiscoverer * dc)
{
  if (!g_source_is_destroyed (g_main_current_source ())) {
    GST_DEBUG ("Setting result to TIMEOUT");
    dc->priv->current_info->result = GST_DISCOVERER_TIMEOUT;
    dc->priv->processing = FALSE;
    discoverer_collect (dc);
    discoverer_cleanup (dc);
  }
  return FALSE;
}

/* If there is a pending URI, it will pop it from the list of pending
 * URIs and start the discovery on it.
 *
 * Returns GST_DISCOVERER_OK if the next URI was popped and is processing,
 * else a error flag.
 */
static GstDiscovererResult
start_discovering (GstDiscoverer * dc)
{
  gboolean ready;
  GstDiscovererResult res = GST_DISCOVERER_OK;

  GST_DEBUG ("Starting");

  DISCO_LOCK (dc);
  if (dc->priv->pending_uris == NULL) {
    GST_WARNING ("No URI to process");
    res = GST_DISCOVERER_URI_INVALID;
    DISCO_UNLOCK (dc);
    goto beach;
  }

  if (dc->priv->current_info != NULL) {
    GST_WARNING ("Already processing a file");
    res = GST_DISCOVERER_BUSY;
    DISCO_UNLOCK (dc);
    goto beach;
  }

  g_signal_emit (dc, gst_discoverer_signals[SIGNAL_STARTING], 0);

  ready = _setup_locked (dc);

  DISCO_UNLOCK (dc);

  if (dc->priv->async) {
    if (ready) {
      GSource *source;

      source = g_idle_source_new ();
      g_source_set_callback (source,
          (GSourceFunc) emit_discovererd_and_next, gst_object_ref (dc),
          gst_object_unref);
      g_source_attach (source, dc->priv->ctx);
      goto beach;
    }
    if (dc->priv->processing)
      handle_current_async (dc);
  } else {
    if (!ready)
      handle_current_sync (dc);
  }

beach:
  return res;
}

/* Serializing code */

static GVariant *
_serialize_common_stream_info (GstDiscovererStreamInfo * sinfo,
    GstDiscovererSerializeFlags flags)
{
  GVariant *common;
  GVariant *nextv = NULL;
  gchar *caps_str = NULL, *tags_str = NULL, *misc_str = NULL;

  if (sinfo->caps && (flags & GST_DISCOVERER_SERIALIZE_CAPS))
    caps_str = gst_caps_to_string (sinfo->caps);

  if (sinfo->tags && (flags & GST_DISCOVERER_SERIALIZE_TAGS))
    tags_str = gst_tag_list_to_string (sinfo->tags);

  if (sinfo->misc && (flags & GST_DISCOVERER_SERIALIZE_MISC))
    misc_str = gst_structure_to_string (sinfo->misc);


  if (sinfo->next)
    nextv = gst_discoverer_info_to_variant_recurse (sinfo->next, flags);
  else
    nextv = g_variant_new ("()");

  common =
      g_variant_new ("(msmsmsmsv)", sinfo->stream_id, caps_str, tags_str,
      misc_str, nextv);

  g_free (caps_str);
  g_free (tags_str);
  g_free (misc_str);

  return common;
}

static GVariant *
_serialize_info (GstDiscovererInfo * info, GstDiscovererSerializeFlags flags)
{
  gchar *tags_str = NULL;
  GVariant *ret;

  if (info->tags && (flags & GST_DISCOVERER_SERIALIZE_TAGS))
    tags_str = gst_tag_list_to_string (info->tags);

  ret =
      g_variant_new ("(mstbmsb)", info->uri, info->duration, info->seekable,
      tags_str, info->live);

  g_free (tags_str);

  return ret;
}

static GVariant *
_serialize_audio_stream_info (GstDiscovererAudioInfo * ainfo)
{
  return g_variant_new ("(uuuuumst)",
      ainfo->channels,
      ainfo->sample_rate,
      ainfo->bitrate, ainfo->max_bitrate, ainfo->depth, ainfo->language,
      ainfo->channel_mask);
}

static GVariant *
_serialize_video_stream_info (GstDiscovererVideoInfo * vinfo)
{
  return g_variant_new ("(uuuuuuubuub)",
      vinfo->width,
      vinfo->height,
      vinfo->depth,
      vinfo->framerate_num,
      vinfo->framerate_denom,
      vinfo->par_num,
      vinfo->par_denom,
      vinfo->interlaced, vinfo->bitrate, vinfo->max_bitrate, vinfo->is_image);
}

static GVariant *
_serialize_subtitle_stream_info (GstDiscovererSubtitleInfo * sinfo)
{
  return g_variant_new ("ms", sinfo->language);
}

static GVariant *
gst_discoverer_info_to_variant_recurse (GstDiscovererStreamInfo * sinfo,
    GstDiscovererSerializeFlags flags)
{
  GVariant *stream_variant = NULL;
  GVariant *common_stream_variant =
      _serialize_common_stream_info (sinfo, flags);

  if (GST_IS_DISCOVERER_CONTAINER_INFO (sinfo)) {
    GList *tmp;
    GList *streams =
        gst_discoverer_container_info_get_streams (GST_DISCOVERER_CONTAINER_INFO
        (sinfo));

    if (g_list_length (streams) > 0) {
      GVariantBuilder children;
      GVariant *child_variant;
      g_variant_builder_init (&children, G_VARIANT_TYPE_ARRAY);

      for (tmp = streams; tmp; tmp = tmp->next) {
        child_variant =
            gst_discoverer_info_to_variant_recurse (tmp->data, flags);
        g_variant_builder_add (&children, "v", child_variant);
      }
      stream_variant =
          g_variant_new ("(yvav)", 'c', common_stream_variant, &children);
    } else {
      stream_variant =
          g_variant_new ("(yvav)", 'c', common_stream_variant, NULL);
    }

    gst_discoverer_stream_info_list_free (streams);
  } else if (GST_IS_DISCOVERER_AUDIO_INFO (sinfo)) {
    GVariant *audio_stream_info =
        _serialize_audio_stream_info (GST_DISCOVERER_AUDIO_INFO (sinfo));
    stream_variant =
        g_variant_new ("(yvv)", 'a', common_stream_variant, audio_stream_info);
  } else if (GST_IS_DISCOVERER_VIDEO_INFO (sinfo)) {
    GVariant *video_stream_info =
        _serialize_video_stream_info (GST_DISCOVERER_VIDEO_INFO (sinfo));
    stream_variant =
        g_variant_new ("(yvv)", 'v', common_stream_variant, video_stream_info);
  } else if (GST_IS_DISCOVERER_SUBTITLE_INFO (sinfo)) {
    GVariant *subtitle_stream_info =
        _serialize_subtitle_stream_info (GST_DISCOVERER_SUBTITLE_INFO (sinfo));
    stream_variant =
        g_variant_new ("(yvv)", 's', common_stream_variant,
        subtitle_stream_info);
  } else {
    GVariant *nextv = NULL;
    GstDiscovererStreamInfo *ninfo =
        gst_discoverer_stream_info_get_next (sinfo);

    if (ninfo) {
      nextv = gst_discoverer_info_to_variant_recurse (ninfo, flags);
      stream_variant =
          g_variant_new ("(yvv)", 'n', common_stream_variant,
          g_variant_new ("v", nextv));
    } else {
      stream_variant = g_variant_new ("(yv)", 'n', common_stream_variant);
    }
  }

  return stream_variant;
}

/* Parsing code */

#define GET_FROM_TUPLE(v, t, n, val) G_STMT_START{         \
  GVariant *child = g_variant_get_child_value (v, n); \
  *val = g_variant_get_##t(child); \
  g_variant_unref (child); \
}G_STMT_END

static const gchar *
_maybe_get_string_from_tuple (GVariant * tuple, guint index)
{
  const gchar *ret = NULL;
  GVariant *maybe;
  GET_FROM_TUPLE (tuple, maybe, index, &maybe);
  if (maybe) {
    ret = g_variant_get_string (maybe, NULL);
    g_variant_unref (maybe);
  }

  return ret;
}

static void
_parse_info (GstDiscovererInfo * info, GVariant * info_variant)
{
  const gchar *str;

  str = _maybe_get_string_from_tuple (info_variant, 0);
  if (str)
    info->uri = g_strdup (str);

  GET_FROM_TUPLE (info_variant, uint64, 1, &info->duration);
  GET_FROM_TUPLE (info_variant, boolean, 2, &info->seekable);

  str = _maybe_get_string_from_tuple (info_variant, 3);
  if (str)
    info->tags = gst_tag_list_new_from_string (str);

  GET_FROM_TUPLE (info_variant, boolean, 4, &info->live);
}

static void
_parse_common_stream_info (GstDiscovererStreamInfo * sinfo, GVariant * common,
    GstDiscovererInfo * info)
{
  const gchar *str;

  str = _maybe_get_string_from_tuple (common, 0);
  if (str)
    sinfo->stream_id = g_strdup (str);

  str = _maybe_get_string_from_tuple (common, 1);
  if (str)
    sinfo->caps = gst_caps_from_string (str);

  str = _maybe_get_string_from_tuple (common, 2);
  if (str)
    sinfo->tags = gst_tag_list_new_from_string (str);

  str = _maybe_get_string_from_tuple (common, 3);
  if (str)
    sinfo->misc = gst_structure_new_from_string (str);

  if (g_variant_n_children (common) > 4) {
    GVariant *nextv;

    GET_FROM_TUPLE (common, variant, 4, &nextv);
    if (g_variant_n_children (nextv) > 0) {
      sinfo->next = _parse_discovery (nextv, info);
    }
    g_variant_unref (nextv);
  }

  g_variant_unref (common);
}

static void
_parse_audio_stream_info (GstDiscovererAudioInfo * ainfo, GVariant * specific)
{
  const gchar *str;

  GET_FROM_TUPLE (specific, uint32, 0, &ainfo->channels);
  GET_FROM_TUPLE (specific, uint32, 1, &ainfo->sample_rate);
  GET_FROM_TUPLE (specific, uint32, 2, &ainfo->bitrate);
  GET_FROM_TUPLE (specific, uint32, 3, &ainfo->max_bitrate);
  GET_FROM_TUPLE (specific, uint32, 4, &ainfo->depth);

  str = _maybe_get_string_from_tuple (specific, 5);

  if (str)
    ainfo->language = g_strdup (str);

  GET_FROM_TUPLE (specific, uint64, 6, &ainfo->channel_mask);

  g_variant_unref (specific);
}

static void
_parse_video_stream_info (GstDiscovererVideoInfo * vinfo, GVariant * specific)
{
  GET_FROM_TUPLE (specific, uint32, 0, &vinfo->width);
  GET_FROM_TUPLE (specific, uint32, 1, &vinfo->height);
  GET_FROM_TUPLE (specific, uint32, 2, &vinfo->depth);
  GET_FROM_TUPLE (specific, uint32, 3, &vinfo->framerate_num);
  GET_FROM_TUPLE (specific, uint32, 4, &vinfo->framerate_denom);
  GET_FROM_TUPLE (specific, uint32, 5, &vinfo->par_num);
  GET_FROM_TUPLE (specific, uint32, 6, &vinfo->par_denom);
  GET_FROM_TUPLE (specific, boolean, 7, &vinfo->interlaced);
  GET_FROM_TUPLE (specific, uint32, 8, &vinfo->bitrate);
  GET_FROM_TUPLE (specific, uint32, 9, &vinfo->max_bitrate);
  GET_FROM_TUPLE (specific, boolean, 10, &vinfo->is_image);

  g_variant_unref (specific);
}

static void
_parse_subtitle_stream_info (GstDiscovererSubtitleInfo * sinfo,
    GVariant * specific)
{
  GVariant *maybe;

  maybe = g_variant_get_maybe (specific);
  if (maybe) {
    sinfo->language = g_strdup (g_variant_get_string (maybe, NULL));
    g_variant_unref (maybe);
  }

  g_variant_unref (specific);
}

static GstDiscovererStreamInfo *
_parse_discovery (GVariant * variant, GstDiscovererInfo * info)
{
  gchar type;
  GVariant *common = g_variant_get_child_value (variant, 1);
  GVariant *specific = NULL;
  GstDiscovererStreamInfo *sinfo = NULL;

  if (g_variant_n_children (variant) > 2)
    specific = g_variant_get_child_value (variant, 2);

  GET_FROM_TUPLE (variant, byte, 0, &type);
  switch (type) {
    case 'c':
      sinfo = g_object_new (GST_TYPE_DISCOVERER_CONTAINER_INFO, NULL);
      break;
    case 'a':
      sinfo = g_object_new (GST_TYPE_DISCOVERER_AUDIO_INFO, NULL);
      _parse_audio_stream_info (GST_DISCOVERER_AUDIO_INFO (sinfo),
          g_variant_get_child_value (specific, 0));
      break;
    case 'v':
      sinfo = g_object_new (GST_TYPE_DISCOVERER_VIDEO_INFO, NULL);
      _parse_video_stream_info (GST_DISCOVERER_VIDEO_INFO (sinfo),
          g_variant_get_child_value (specific, 0));
      break;
    case 's':
      sinfo = g_object_new (GST_TYPE_DISCOVERER_SUBTITLE_INFO, NULL);
      _parse_subtitle_stream_info (GST_DISCOVERER_SUBTITLE_INFO (sinfo),
          g_variant_get_child_value (specific, 0));
      break;
    case 'n':
      sinfo = g_object_new (GST_TYPE_DISCOVERER_STREAM_INFO, NULL);
      break;
    default:
      GST_WARNING ("Unexpected discoverer info type %d", type);
      goto out;
  }

  _parse_common_stream_info (sinfo, g_variant_get_child_value (common, 0),
      info);

  if (!GST_IS_DISCOVERER_CONTAINER_INFO (sinfo)) {
    info->stream_list = g_list_append (info->stream_list, sinfo);
  }

  if (!info->stream_info) {
    info->stream_info = sinfo;
  }

  if (GST_IS_DISCOVERER_CONTAINER_INFO (sinfo)) {
    GVariantIter iter;
    GVariant *child;

    GstDiscovererContainerInfo *cinfo = GST_DISCOVERER_CONTAINER_INFO (sinfo);
    g_variant_iter_init (&iter, specific);
    cinfo->tags = sinfo->tags;
    while ((child = g_variant_iter_next_value (&iter))) {
      GstDiscovererStreamInfo *child_info;
      child_info = _parse_discovery (g_variant_get_variant (child), info);
      if (child_info != NULL) {
        cinfo->streams =
            g_list_append (cinfo->streams,
            gst_discoverer_stream_info_ref (child_info));
      }
      g_variant_unref (child);
    }
  }

out:

  g_variant_unref (common);
  if (specific)
    g_variant_unref (specific);
  g_variant_unref (variant);
  return sinfo;
}

/**
 * gst_discoverer_start:
 * @discoverer: A #GstDiscoverer
 *
 * Allow asynchronous discovering of URIs to take place.
 * A #GMainLoop must be available for #GstDiscoverer to properly work in
 * asynchronous mode.
 */
void
gst_discoverer_start (GstDiscoverer * discoverer)
{
  GSource *source;
  GMainContext *ctx = NULL;

  g_return_if_fail (GST_IS_DISCOVERER (discoverer));

  GST_DEBUG_OBJECT (discoverer, "Starting...");

  if (discoverer->priv->async) {
    GST_DEBUG_OBJECT (discoverer, "We were already started");
    return;
  }

  discoverer->priv->async = TRUE;
  discoverer->priv->running = TRUE;

  ctx = g_main_context_get_thread_default ();

  /* Connect to bus signals */
  if (ctx == NULL)
    ctx = g_main_context_default ();

  source = gst_bus_create_watch (discoverer->priv->bus);
  g_source_set_callback (source, (GSourceFunc) gst_bus_async_signal_func,
      NULL, NULL);
  g_source_attach (source, ctx);
  discoverer->priv->bus_source = source;
  discoverer->priv->ctx = g_main_context_ref (ctx);

  start_discovering (discoverer);
  GST_DEBUG_OBJECT (discoverer, "Started");
}

/**
 * gst_discoverer_stop:
 * @discoverer: A #GstDiscoverer
 *
 * Stop the discovery of any pending URIs and clears the list of
 * pending URIS (if any).
 */
void
gst_discoverer_stop (GstDiscoverer * discoverer)
{
  g_return_if_fail (GST_IS_DISCOVERER (discoverer));

  GST_DEBUG_OBJECT (discoverer, "Stopping...");

  if (!discoverer->priv->async) {
    GST_DEBUG_OBJECT (discoverer,
        "We were already stopped, or running synchronously");
    return;
  }

  DISCO_LOCK (discoverer);
  if (discoverer->priv->processing) {
    /* We prevent any further processing by setting the bus to
     * flushing and setting the pipeline to READY.
     * _reset() will take care of the rest of the cleanup */
    if (discoverer->priv->bus)
      gst_bus_set_flushing (discoverer->priv->bus, TRUE);
    if (discoverer->priv->pipeline)
      gst_element_set_state ((GstElement *) discoverer->priv->pipeline,
          GST_STATE_READY);
  }
  discoverer->priv->running = FALSE;
  DISCO_UNLOCK (discoverer);

  /* Remove timeout handler */
  if (discoverer->priv->timeout_source) {
    g_source_destroy (discoverer->priv->timeout_source);
    g_source_unref (discoverer->priv->timeout_source);
    discoverer->priv->timeout_source = NULL;
  }
  /* Remove signal watch */
  if (discoverer->priv->bus_source) {
    g_source_destroy (discoverer->priv->bus_source);
    g_source_unref (discoverer->priv->bus_source);
    discoverer->priv->bus_source = NULL;
  }
  /* Unref main context */
  if (discoverer->priv->ctx) {
    g_main_context_unref (discoverer->priv->ctx);
    discoverer->priv->ctx = NULL;
  }
  discoverer_reset (discoverer);

  discoverer->priv->async = FALSE;

  GST_DEBUG_OBJECT (discoverer, "Stopped");
}

/**
 * gst_discoverer_discover_uri_async:
 * @discoverer: A #GstDiscoverer
 * @uri: the URI to add.
 *
 * Appends the given @uri to the list of URIs to discoverer. The actual
 * discovery of the @uri will only take place if gst_discoverer_start() has
 * been called.
 *
 * A copy of @uri will be made internally, so the caller can safely g_free()
 * afterwards.
 *
 * Returns: %TRUE if the @uri was successfully appended to the list of pending
 * uris, else %FALSE
 */
gboolean
gst_discoverer_discover_uri_async (GstDiscoverer * discoverer,
    const gchar * uri)
{
  gboolean can_run;

  g_return_val_if_fail (GST_IS_DISCOVERER (discoverer), FALSE);

  GST_DEBUG_OBJECT (discoverer, "uri : %s", uri);

  DISCO_LOCK (discoverer);
  can_run = (discoverer->priv->pending_uris == NULL);
  discoverer->priv->pending_uris =
      g_list_append (discoverer->priv->pending_uris, g_strdup (uri));
  DISCO_UNLOCK (discoverer);

  if (can_run)
    start_discovering (discoverer);

  return TRUE;
}


/* Synchronous mode */
/**
 * gst_discoverer_discover_uri:
 * @discoverer: A #GstDiscoverer
 * @uri: The URI to run on.
 * @err: If an error occurred, this field will be filled in.
 *
 * Synchronously discovers the given @uri.
 *
 * A copy of @uri will be made internally, so the caller can safely g_free()
 * afterwards.
 *
 * Returns: (transfer full): the result of the scanning. Can be %NULL if an
 * error occurred.
 */
GstDiscovererInfo *
gst_discoverer_discover_uri (GstDiscoverer * discoverer, const gchar * uri,
    GError ** err)
{
  GstDiscovererResult res = 0;
  GstDiscovererInfo *info;

  g_return_val_if_fail (GST_IS_DISCOVERER (discoverer), NULL);
  g_return_val_if_fail (uri, NULL);

  GST_DEBUG_OBJECT (discoverer, "uri:%s", uri);

  DISCO_LOCK (discoverer);
  if (G_UNLIKELY (discoverer->priv->current_info)) {
    DISCO_UNLOCK (discoverer);
    GST_WARNING_OBJECT (discoverer, "Already handling a uri");
    if (err)
      *err = g_error_new (GST_CORE_ERROR, GST_CORE_ERROR_FAILED,
          "Already handling a uri");
    return NULL;
  }

  discoverer->priv->pending_uris =
      g_list_append (discoverer->priv->pending_uris, g_strdup (uri));
  DISCO_UNLOCK (discoverer);

  res = start_discovering (discoverer);
  discoverer_collect (discoverer);

  /* Get results */
  if (err) {
    if (discoverer->priv->current_error)
      *err = g_error_copy (discoverer->priv->current_error);
    else
      *err = NULL;
  }
  if (res != GST_DISCOVERER_OK) {
    GST_DEBUG ("Setting result to %d (was %d)", res,
        discoverer->priv->current_info->result);
    discoverer->priv->current_info->result = res;
  }
  info = _ensure_info_tags (discoverer);

  discoverer_cleanup (discoverer);

  return info;
}

/**
 * gst_discoverer_new:
 * @timeout: timeout per file, in nanoseconds. Allowed are values between
 *     one second (#GST_SECOND) and one hour (3600 * #GST_SECOND)
 * @err: a pointer to a #GError. can be %NULL
 *
 * Creates a new #GstDiscoverer with the provided timeout.
 *
 * Returns: (transfer full): The new #GstDiscoverer.
 * If an error occurred when creating the discoverer, @err will be set
 * accordingly and %NULL will be returned. If @err is set, the caller must
 * free it when no longer needed using g_error_free().
 */
GstDiscoverer *
gst_discoverer_new (GstClockTime timeout, GError ** err)
{
  GstDiscoverer *res;

  g_return_val_if_fail (GST_CLOCK_TIME_IS_VALID (timeout), NULL);

  res = g_object_new (GST_TYPE_DISCOVERER, "timeout", timeout, NULL);
  if (res->priv->uridecodebin == NULL) {
    if (err)
      *err = g_error_new (GST_CORE_ERROR, GST_CORE_ERROR_MISSING_PLUGIN,
          "Couldn't create 'uridecodebin' element");
    gst_object_unref (res);
    res = NULL;
  }
  return res;
}

/**
 * gst_discoverer_info_to_variant:
 * @info: A #GstDiscovererInfo
 * @flags: A combination of #GstDiscovererSerializeFlags to specify
 * what needs to be serialized.
 *
 * Serializes @info to a #GVariant that can be parsed again
 * through gst_discoverer_info_from_variant().
 *
 * Note that any #GstToc (s) that might have been discovered will not be serialized
 * for now.
 *
 * Returns: (transfer full): A newly-allocated #GVariant representing @info.
 *
 * Since: 1.6
 */
GVariant *
gst_discoverer_info_to_variant (GstDiscovererInfo * info,
    GstDiscovererSerializeFlags flags)
{
  /* FIXME: implement TOC support */
  GVariant *stream_variant;
  GVariant *variant, *info_variant;
  GstDiscovererStreamInfo *sinfo;
  GVariant *wrapper;

  g_return_val_if_fail (GST_IS_DISCOVERER_INFO (info), NULL);
  g_return_val_if_fail (gst_discoverer_info_get_result (info) ==
      GST_DISCOVERER_OK
      || gst_discoverer_info_get_result (info) ==
      GST_DISCOVERER_MISSING_PLUGINS, NULL);

  sinfo = gst_discoverer_info_get_stream_info (info);
  stream_variant = gst_discoverer_info_to_variant_recurse (sinfo, flags);
  info_variant = _serialize_info (info, flags);

  variant = g_variant_new ("(vv)", info_variant, stream_variant);

  /* Returning a wrapper implies some small overhead, but simplifies
   * deserializing from bytes */
  wrapper = g_variant_new_variant (variant);

  gst_discoverer_stream_info_unref (sinfo);
  return wrapper;
}

/**
 * gst_discoverer_info_from_variant:
 * @variant: A #GVariant to deserialize into a #GstDiscovererInfo.
 *
 * Parses a #GVariant as produced by gst_discoverer_info_to_variant()
 * back to a #GstDiscovererInfo.
 *
 * Returns: (transfer full): A newly-allocated #GstDiscovererInfo.
 *
 * Since: 1.6
 */
GstDiscovererInfo *
gst_discoverer_info_from_variant (GVariant * variant)
{
  GstDiscovererInfo *info = g_object_new (GST_TYPE_DISCOVERER_INFO, NULL);
  GVariant *info_variant = g_variant_get_variant (variant);
  GVariant *info_specific_variant;
  GVariant *wrapped;

  GET_FROM_TUPLE (info_variant, variant, 0, &info_specific_variant);
  GET_FROM_TUPLE (info_variant, variant, 1, &wrapped);

  _parse_info (info, info_specific_variant);
  _parse_discovery (wrapped, info);
  g_variant_unref (info_specific_variant);
  g_variant_unref (info_variant);

  return info;
}
