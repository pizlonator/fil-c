/* GStreamer
 *
 * Copyright (C) 2014-2015 Sebastian Dröge <sebastian@centricular.com>
 * Copyright (C) 2015 Thibault Saunier <tsaunier@gnome.org>
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
 * SECTION:gsttranscoder
 * @short_description: High level API to transcode media files
 * from one format to any other format using the GStreamer framework.
 * @symbols:
 *   - gst_transcoder_error_quark
 */

#include "gsttranscoder.h"
#include "gsttranscoder-private.h"

static GOnce once = G_ONCE_INIT;

GST_DEBUG_CATEGORY_STATIC (gst_transcoder_debug);
#define GST_CAT_DEFAULT gst_transcoder_debug

#define DEFAULT_URI NULL
#define DEFAULT_POSITION GST_CLOCK_TIME_NONE
#define DEFAULT_DURATION GST_CLOCK_TIME_NONE
#define DEFAULT_POSITION_UPDATE_INTERVAL_MS 100
#define DEFAULT_AVOID_REENCODING   FALSE

GQuark
gst_transcoder_error_quark (void)
{
  static GQuark quark;

  if (!quark)
    quark = g_quark_from_static_string ("gst-transcoder-error-quark");

  return quark;
}

enum
{
  PROP_0,
  PROP_SRC_URI,
  PROP_DEST_URI,
  PROP_PROFILE,
  PROP_POSITION,
  PROP_DURATION,
  PROP_PIPELINE,
  PROP_POSITION_UPDATE_INTERVAL,
  PROP_AVOID_REENCODING,
  PROP_LAST
};

struct _GstTranscoder
{
  GstObject parent;

  GstEncodingProfile *profile;
  gchar *source_uri;
  gchar *dest_uri;

  GThread *thread;
  GCond cond;
  GMainContext *context;
  GMainLoop *loop;

  GstElement *transcodebin;
  GstBus *bus;
  GstState target_state, current_state;
  gboolean is_live, is_eos;
  GSource *tick_source, *ready_timeout_source;

  guint position_update_interval_ms;
  gint wanted_cpu_usage;

  GstClockTime last_duration;

  GstTranscoderState app_state;

  GstBus *api_bus;
  GstTranscoderSignalAdapter *signal_adapter;
  GstTranscoderSignalAdapter *sync_signal_adapter;
};

struct _GstTranscoderClass
{
  GstObjectClass parent_class;
};

#define parent_class gst_transcoder_parent_class
G_DEFINE_TYPE (GstTranscoder, gst_transcoder, GST_TYPE_OBJECT);

static GParamSpec *param_specs[PROP_LAST] = { NULL, };

static void gst_transcoder_dispose (GObject * object);
static void gst_transcoder_finalize (GObject * object);
static void gst_transcoder_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_transcoder_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_transcoder_constructed (GObject * object);

static gpointer gst_transcoder_main (gpointer data);

static gboolean gst_transcoder_set_position_update_interval_internal (gpointer
    user_data);


/**
 * gst_transcoder_set_cpu_usage:
 * @self: The GstTranscoder to limit CPU usage on.
 * @cpu_usage: The percentage of the CPU the process running the transcoder
 * should try to use. It takes into account the number of cores available.
 *
 * Sets @cpu_usage as target percentage CPU usage of the process running the
 * transcoding task. It will modulate the transcoding speed to reach that target
 * usage.
 */
void
gst_transcoder_set_cpu_usage (GstTranscoder * self, gint cpu_usage)
{
  GST_OBJECT_LOCK (self);
  self->wanted_cpu_usage = cpu_usage;
  if (self->transcodebin)
    g_object_set (self->transcodebin, "cpu-usage", cpu_usage, NULL);
  GST_OBJECT_UNLOCK (self);
}

static void
gst_transcoder_init (GstTranscoder * self)
{
  GST_TRACE_OBJECT (self, "Initializing");

  self = gst_transcoder_get_instance_private (self);

  g_cond_init (&self->cond);

  self->context = g_main_context_new ();
  self->loop = g_main_loop_new (self->context, FALSE);
  self->api_bus = gst_bus_new ();
  self->wanted_cpu_usage = 100;

  self->position_update_interval_ms = DEFAULT_POSITION_UPDATE_INTERVAL_MS;

  GST_TRACE_OBJECT (self, "Initialized");
}

static void
gst_transcoder_class_init (GstTranscoderClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->set_property = gst_transcoder_set_property;
  gobject_class->get_property = gst_transcoder_get_property;
  gobject_class->dispose = gst_transcoder_dispose;
  gobject_class->finalize = gst_transcoder_finalize;
  gobject_class->constructed = gst_transcoder_constructed;

  param_specs[PROP_SRC_URI] =
      g_param_spec_string ("src-uri", "URI", "Source URI", DEFAULT_URI,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_DEST_URI] =
      g_param_spec_string ("dest-uri", "URI", "Source URI", DEFAULT_URI,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_PROFILE] =
      g_param_spec_object ("profile", "Profile",
      "The GstEncodingProfile to use", GST_TYPE_ENCODING_PROFILE,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_POSITION] =
      g_param_spec_uint64 ("position", "Position", "Current Position",
      0, G_MAXUINT64, DEFAULT_POSITION,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_DURATION] =
      g_param_spec_uint64 ("duration", "Duration", "Duration",
      0, G_MAXUINT64, DEFAULT_DURATION,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_PIPELINE] =
      g_param_spec_object ("pipeline", "Pipeline",
      "GStreamer pipeline that is used",
      GST_TYPE_ELEMENT, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_POSITION_UPDATE_INTERVAL] =
      g_param_spec_uint ("position-update-interval", "Position update interval",
      "Interval in milliseconds between two position-updated signals."
      "Pass 0 to stop updating the position.",
      0, 10000, DEFAULT_POSITION_UPDATE_INTERVAL_MS,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /**
   * GstTranscoder:avoid-reencoding:
   *
   * See #encodebin:avoid-reencoding
   */
  param_specs[PROP_AVOID_REENCODING] =
      g_param_spec_boolean ("avoid-reencoding", "Avoid re-encoding",
      "Whether to re-encode portions of compatible video streams that lay on segment boundaries",
      DEFAULT_AVOID_REENCODING, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, param_specs);
}

static void
gst_transcoder_dispose (GObject * object)
{
  GstTranscoder *self = GST_TRANSCODER (object);

  GST_TRACE_OBJECT (self, "Stopping main thread");

  GST_OBJECT_LOCK (self);
  if (self->loop) {
    g_main_loop_quit (self->loop);
    GST_OBJECT_UNLOCK (self);

    g_thread_join (self->thread);

    GST_OBJECT_LOCK (self);
    self->thread = NULL;

    g_main_loop_unref (self->loop);
    self->loop = NULL;

    g_main_context_unref (self->context);
    self->context = NULL;

    gst_clear_object (&self->signal_adapter);
    gst_clear_object (&self->sync_signal_adapter);
    GST_OBJECT_UNLOCK (self);
  } else {
    GST_OBJECT_UNLOCK (self);
  }

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_transcoder_finalize (GObject * object)
{
  GstTranscoder *self = GST_TRANSCODER (object);

  GST_TRACE_OBJECT (self, "Finalizing");

  g_free (self->source_uri);
  g_free (self->dest_uri);
  g_cond_clear (&self->cond);
  gst_object_unref (self->api_bus);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_transcoder_constructed (GObject * object)
{
  GstTranscoder *self = GST_TRANSCODER (object);

  GST_TRACE_OBJECT (self, "Constructed");

  self->transcodebin =
      gst_element_factory_make ("uritranscodebin", "uritranscodebin");

  g_object_set (self->transcodebin, "source-uri", self->source_uri,
      "dest-uri", self->dest_uri, "profile", self->profile,
      "cpu-usage", self->wanted_cpu_usage, NULL);

  GST_OBJECT_LOCK (self);
  self->thread = g_thread_new ("GstTranscoder", gst_transcoder_main, self);
  while (!self->loop || !g_main_loop_is_running (self->loop))
    g_cond_wait (&self->cond, GST_OBJECT_GET_LOCK (self));
  GST_OBJECT_UNLOCK (self);

  G_OBJECT_CLASS (parent_class)->constructed (object);
}

static void
gst_transcoder_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstTranscoder *self = GST_TRANSCODER (object);

  switch (prop_id) {
    case PROP_SRC_URI:{
      GST_OBJECT_LOCK (self);
      g_free (self->source_uri);
      self->source_uri = g_value_dup_string (value);
      GST_DEBUG_OBJECT (self, "Set source_uri=%s", self->source_uri);
      GST_OBJECT_UNLOCK (self);
      break;
    }
    case PROP_DEST_URI:{
      GST_OBJECT_LOCK (self);
      g_free (self->dest_uri);
      self->dest_uri = g_value_dup_string (value);
      GST_DEBUG_OBJECT (self, "Set dest_uri=%s", self->dest_uri);
      GST_OBJECT_UNLOCK (self);
      break;
    }
    case PROP_POSITION_UPDATE_INTERVAL:
      GST_OBJECT_LOCK (self);
      self->position_update_interval_ms = g_value_get_uint (value);
      GST_DEBUG_OBJECT (self, "Set position update interval=%u ms",
          g_value_get_uint (value));
      GST_OBJECT_UNLOCK (self);

      gst_transcoder_set_position_update_interval_internal (self);
      break;
    case PROP_PROFILE:
      GST_OBJECT_LOCK (self);
      self->profile = g_value_dup_object (value);
      GST_OBJECT_UNLOCK (self);
      break;
    case PROP_AVOID_REENCODING:
      g_object_set (self->transcodebin, "avoid-reencoding",
          g_value_get_boolean (value), NULL);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_transcoder_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstTranscoder *self = GST_TRANSCODER (object);

  switch (prop_id) {
    case PROP_SRC_URI:
      GST_OBJECT_LOCK (self);
      g_value_set_string (value, self->source_uri);
      GST_OBJECT_UNLOCK (self);
      break;
    case PROP_DEST_URI:
      GST_OBJECT_LOCK (self);
      g_value_set_string (value, self->dest_uri);
      GST_OBJECT_UNLOCK (self);
      break;
    case PROP_POSITION:{
      gint64 position = 0;

      if (self->is_eos)
        position = self->last_duration;
      else
        gst_element_query_position (self->transcodebin, GST_FORMAT_TIME,
            &position);
      g_value_set_uint64 (value, position);
      GST_TRACE_OBJECT (self, "Returning position=%" GST_TIME_FORMAT,
          GST_TIME_ARGS (g_value_get_uint64 (value)));
      break;
    }
    case PROP_DURATION:{
      gint64 duration = 0;

      gst_element_query_duration (self->transcodebin, GST_FORMAT_TIME,
          &duration);
      g_value_set_uint64 (value, duration);
      GST_TRACE_OBJECT (self, "Returning duration=%" GST_TIME_FORMAT,
          GST_TIME_ARGS (g_value_get_uint64 (value)));
      break;
    }
    case PROP_PIPELINE:
      g_value_set_object (value, self->transcodebin);
      break;
    case PROP_POSITION_UPDATE_INTERVAL:
      GST_OBJECT_LOCK (self);
      g_value_set_uint (value,
          gst_transcoder_get_position_update_interval (self));
      GST_OBJECT_UNLOCK (self);
      break;
    case PROP_PROFILE:
      GST_OBJECT_LOCK (self);
      g_value_set_object (value, self->profile);
      GST_OBJECT_UNLOCK (self);
      break;
    case PROP_AVOID_REENCODING:
    {
      gboolean avoid_reencoding;

      g_object_get (self->transcodebin, "avoid-reencoding", &avoid_reencoding,
          NULL);
      g_value_set_boolean (value, avoid_reencoding);
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/*
 * Works same as gst_structure_set to set field/type/value triplets on message data
 */
static void
api_bus_post_message (GstTranscoder * self, GstTranscoderMessage message_type,
    const gchar * firstfield, ...)
{
  GstStructure *message_data = NULL;
  GstMessage *msg = NULL;
  va_list varargs;

  GST_INFO ("Posting API-bus message-type: %s",
      gst_transcoder_message_get_name (message_type));
  message_data = gst_structure_new (GST_TRANSCODER_MESSAGE_DATA,
      GST_TRANSCODER_MESSAGE_DATA_TYPE, GST_TYPE_TRANSCODER_MESSAGE,
      message_type, NULL);

  va_start (varargs, firstfield);
  gst_structure_set_valist (message_data, firstfield, varargs);
  va_end (varargs);

  msg = gst_message_new_custom (GST_MESSAGE_APPLICATION,
      GST_OBJECT (self), message_data);
  GST_DEBUG ("Created message with payload: [ %" GST_PTR_FORMAT " ]",
      message_data);
  gst_bus_post (self->api_bus, msg);
}

static gboolean
main_loop_running_cb (gpointer user_data)
{
  GstTranscoder *self = GST_TRANSCODER (user_data);

  GST_TRACE_OBJECT (self, "Main loop running now");

  GST_OBJECT_LOCK (self);
  g_cond_signal (&self->cond);
  GST_OBJECT_UNLOCK (self);

  return G_SOURCE_REMOVE;
}

static gboolean
tick_cb (gpointer user_data)
{
  GstTranscoder *self = GST_TRANSCODER (user_data);
  gint64 position;

  if (self->target_state < GST_STATE_PAUSED)
    return G_SOURCE_CONTINUE;

  if (!gst_element_query_position (self->transcodebin, GST_FORMAT_TIME,
          &position)) {
    GST_LOG_OBJECT (self, "Could not query position");
    return G_SOURCE_CONTINUE;
  }

  GST_LOG_OBJECT (self, "Position %" GST_TIME_FORMAT, GST_TIME_ARGS (position));

  api_bus_post_message (self, GST_TRANSCODER_MESSAGE_POSITION_UPDATED,
      GST_TRANSCODER_MESSAGE_DATA_POSITION, GST_TYPE_CLOCK_TIME, position,
      NULL);

  return G_SOURCE_CONTINUE;
}

static void
add_tick_source (GstTranscoder * self)
{
  if (self->tick_source)
    return;

  if (!self->position_update_interval_ms)
    return;

  self->tick_source = g_timeout_source_new (self->position_update_interval_ms);
  g_source_set_callback (self->tick_source, (GSourceFunc) tick_cb, self, NULL);
  g_source_attach (self->tick_source, self->context);
}

static void
remove_tick_source (GstTranscoder * self)
{
  if (!self->tick_source)
    return;

  g_source_destroy (self->tick_source);
  g_source_unref (self->tick_source);
  self->tick_source = NULL;
}

static void
dump_dot_file (GstTranscoder * self, const gchar * name)
{
  gchar *full_name;

  full_name = g_strdup_printf ("gst-transcoder.%p.%s", self, name);

  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (self->transcodebin),
      GST_DEBUG_GRAPH_SHOW_ALL, full_name);

  g_free (full_name);
}

static void
error_cb (G_GNUC_UNUSED GstBus * bus, GstMessage * msg, gpointer user_data)
{
  GError *err;
  GstTranscoder *self = GST_TRANSCODER (user_data);
  gchar *name, *debug, *message;
  GstStructure *details = NULL;

  dump_dot_file (self, "error");

  gst_message_parse_error (msg, &err, &debug);
  gst_message_parse_error_details (msg, (const GstStructure **) &details);

  if (!details)
    details = gst_structure_new_empty ("details");
  else
    details = gst_structure_copy (details);

  name = gst_object_get_path_string (msg->src);
  message = gst_error_get_message (err->domain, err->code);

  gst_structure_set (details, "debug", G_TYPE_STRING, debug,
      "msg-source-element-name", G_TYPE_STRING, "name",
      "msg-source-type", G_TYPE_GTYPE, G_OBJECT_TYPE (msg->src),
      "msg-error", G_TYPE_STRING, message, NULL);

  api_bus_post_message (self, GST_TRANSCODER_MESSAGE_ERROR,
      GST_TRANSCODER_MESSAGE_DATA_ERROR, G_TYPE_ERROR, err,
      GST_TRANSCODER_MESSAGE_DATA_ISSUE_DETAILS, GST_TYPE_STRUCTURE, details,
      NULL);

  gst_structure_free (details);
  g_clear_error (&err);
  g_free (debug);
  g_free (name);
  g_free (message);
}

static void
warning_cb (G_GNUC_UNUSED GstBus * bus, GstMessage * msg, gpointer user_data)
{
  GstTranscoder *self = GST_TRANSCODER (user_data);
  GError *err, *transcoder_err;
  gchar *name, *debug, *message, *full_message;
  const GstStructure *details = NULL;

  dump_dot_file (self, "warning");

  gst_message_parse_warning (msg, &err, &debug);
  gst_message_parse_warning_details (msg, &details);

  name = gst_object_get_path_string (msg->src);
  message = gst_error_get_message (err->domain, err->code);

  if (debug)
    full_message =
        g_strdup_printf ("Warning from element %s: %s\n%s\n%s", name, message,
        err->message, debug);
  else
    full_message =
        g_strdup_printf ("Warning from element %s: %s\n%s", name, message,
        err->message);

  GST_WARNING_OBJECT (self, "WARNING: from element %s: %s", name, err->message);
  if (debug != NULL)
    GST_WARNING_OBJECT (self, "Additional debug info: %s", debug);

  transcoder_err =
      g_error_new_literal (GST_TRANSCODER_ERROR, GST_TRANSCODER_ERROR_FAILED,
      full_message);

  api_bus_post_message (self, GST_TRANSCODER_MESSAGE_WARNING,
      GST_TRANSCODER_MESSAGE_DATA_WARNING, G_TYPE_ERROR, transcoder_err,
      GST_TRANSCODER_MESSAGE_DATA_ISSUE_DETAILS, GST_TYPE_STRUCTURE, details,
      NULL);

  g_clear_error (&transcoder_err);
  g_clear_error (&err);
  g_free (debug);
  g_free (name);
  g_free (full_message);
  g_free (message);
}

static void
notify_state_changed (GstTranscoder * self, GstTranscoderState new_state)
{
  if (new_state == self->app_state)
    return;

  GST_DEBUG_OBJECT (self, "Notifying new state: %s",
      gst_transcoder_state_get_name (new_state));
  self->app_state = new_state;
  api_bus_post_message (self, GST_TRANSCODER_MESSAGE_STATE_CHANGED,
      GST_TRANSCODER_MESSAGE_DATA_STATE, GST_TYPE_TRANSCODER_STATE, new_state,
      NULL);
}

static void
eos_cb (G_GNUC_UNUSED GstBus * bus, G_GNUC_UNUSED GstMessage * msg,
    gpointer user_data)
{
  GstTranscoder *self = GST_TRANSCODER (user_data);

  GST_DEBUG_OBJECT (self, "End of stream");

  gst_element_query_duration (self->transcodebin, GST_FORMAT_TIME,
      (gint64 *) & self->last_duration);
  tick_cb (self);
  remove_tick_source (self);

  notify_state_changed (self, GST_TRANSCODER_STATE_STOPPED);
  api_bus_post_message (self, GST_TRANSCODER_MESSAGE_DONE, NULL, NULL);
  self->is_eos = TRUE;
}

static void
clock_lost_cb (G_GNUC_UNUSED GstBus * bus, G_GNUC_UNUSED GstMessage * msg,
    gpointer user_data)
{
  GstTranscoder *self = GST_TRANSCODER (user_data);
  GstStateChangeReturn state_ret;

  GST_DEBUG_OBJECT (self, "Clock lost");
  if (self->target_state >= GST_STATE_PLAYING) {
    state_ret = gst_element_set_state (self->transcodebin, GST_STATE_PAUSED);
    if (state_ret != GST_STATE_CHANGE_FAILURE)
      state_ret = gst_element_set_state (self->transcodebin, GST_STATE_PLAYING);

    if (state_ret == GST_STATE_CHANGE_FAILURE) {
      GError *err = g_error_new (GST_TRANSCODER_ERROR,
          GST_TRANSCODER_ERROR_FAILED, "Failed to handle clock loss");
      api_bus_post_message (self, GST_TRANSCODER_MESSAGE_ERROR,
          GST_TRANSCODER_MESSAGE_DATA_ERROR, G_TYPE_ERROR, err, NULL);
      g_error_free (err);
    }
  }
}

static void
state_changed_cb (G_GNUC_UNUSED GstBus * bus, GstMessage * msg,
    gpointer user_data)
{
  GstTranscoder *self = GST_TRANSCODER (user_data);
  GstState old_state, new_state, pending_state;

  gst_message_parse_state_changed (msg, &old_state, &new_state, &pending_state);

  if (GST_MESSAGE_SRC (msg) == GST_OBJECT (self->transcodebin)) {
    gchar *transition_name;

    GST_DEBUG_OBJECT (self, "Changed state old: %s new: %s pending: %s",
        gst_element_state_get_name (old_state),
        gst_element_state_get_name (new_state),
        gst_element_state_get_name (pending_state));

    transition_name = g_strdup_printf ("%s_%s",
        gst_element_state_get_name (old_state),
        gst_element_state_get_name (new_state));
    dump_dot_file (self, transition_name);
    g_free (transition_name);

    self->current_state = new_state;

    if (new_state == GST_STATE_PAUSED
        && pending_state == GST_STATE_VOID_PENDING) {
      remove_tick_source (self);
      notify_state_changed (self, GST_TRANSCODER_STATE_PAUSED);
    }

    if (new_state == GST_STATE_PLAYING
        && pending_state == GST_STATE_VOID_PENDING) {
      add_tick_source (self);
      notify_state_changed (self, GST_TRANSCODER_STATE_PLAYING);
    }
  }
}

static void
duration_changed_cb (G_GNUC_UNUSED GstBus * bus, G_GNUC_UNUSED GstMessage * msg,
    gpointer user_data)
{
  GstTranscoder *self = GST_TRANSCODER (user_data);
  gint64 duration;

  if (gst_element_query_duration (self->transcodebin, GST_FORMAT_TIME,
          &duration)) {
    api_bus_post_message (self, GST_TRANSCODER_MESSAGE_DURATION_CHANGED,
        GST_TRANSCODER_MESSAGE_DATA_DURATION, GST_TYPE_CLOCK_TIME,
        duration, NULL);
  }
}

static void
latency_cb (G_GNUC_UNUSED GstBus * bus, G_GNUC_UNUSED GstMessage * msg,
    gpointer user_data)
{
  GstTranscoder *self = GST_TRANSCODER (user_data);

  GST_DEBUG_OBJECT (self, "Latency changed");

  gst_bin_recalculate_latency (GST_BIN (self->transcodebin));
}

static void
request_state_cb (G_GNUC_UNUSED GstBus * bus, GstMessage * msg,
    gpointer user_data)
{
  GstTranscoder *self = GST_TRANSCODER (user_data);
  GstState state;
  GstStateChangeReturn state_ret;

  gst_message_parse_request_state (msg, &state);

  GST_DEBUG_OBJECT (self, "State %s requested",
      gst_element_state_get_name (state));

  self->target_state = state;
  state_ret = gst_element_set_state (self->transcodebin, state);
  if (state_ret == GST_STATE_CHANGE_FAILURE) {
    GError *err = g_error_new (GST_TRANSCODER_ERROR,
        GST_TRANSCODER_ERROR_FAILED,
        "Failed to change to requested state %s",
        gst_element_state_get_name (state));

    api_bus_post_message (self, GST_TRANSCODER_MESSAGE_ERROR,
        GST_TRANSCODER_MESSAGE_DATA_ERROR, G_TYPE_ERROR, err, NULL);
    g_error_free (err);
  }
}

static void
element_cb (G_GNUC_UNUSED GstBus * bus, GstMessage * msg, gpointer user_data)
{
  GstTranscoder *self = GST_TRANSCODER (user_data);
  const GstStructure *s;

  s = gst_message_get_structure (msg);
  if (gst_structure_has_name (s, "redirect")) {
    const gchar *new_location;

    new_location = gst_structure_get_string (s, "new-location");
    if (!new_location) {
      const GValue *locations_list, *location_val;
      guint i, size;

      locations_list = gst_structure_get_value (s, "locations");
      size = gst_value_list_get_size (locations_list);
      for (i = 0; i < size; ++i) {
        const GstStructure *location_s;

        location_val = gst_value_list_get_value (locations_list, i);
        if (!GST_VALUE_HOLDS_STRUCTURE (location_val))
          continue;

        location_s = (const GstStructure *) g_value_get_boxed (location_val);
        if (!gst_structure_has_name (location_s, "redirect"))
          continue;

        new_location = gst_structure_get_string (location_s, "new-location");
        if (new_location)
          break;
      }
    }

    if (new_location) {
      GST_FIXME_OBJECT (self, "Handle redirection to '%s'", new_location);
    }
  }
}


static gpointer
gst_transcoder_main (gpointer data)
{
  GstTranscoder *self = GST_TRANSCODER (data);
  GstBus *bus;
  GSource *source;

  GST_TRACE_OBJECT (self, "Starting main thread");

  g_main_context_push_thread_default (self->context);

  source = g_idle_source_new ();
  g_source_set_callback (source, (GSourceFunc) main_loop_running_cb, self,
      NULL);
  g_source_attach (source, self->context);
  g_source_unref (source);

  self->bus = bus = gst_element_get_bus (self->transcodebin);
  gst_bus_add_signal_watch (bus);

  g_signal_connect (G_OBJECT (bus), "message::error", G_CALLBACK (error_cb),
      self);
  g_signal_connect (G_OBJECT (bus), "message::warning", G_CALLBACK (warning_cb),
      self);
  g_signal_connect (G_OBJECT (bus), "message::eos", G_CALLBACK (eos_cb), self);
  g_signal_connect (G_OBJECT (bus), "message::state-changed",
      G_CALLBACK (state_changed_cb), self);
  g_signal_connect (G_OBJECT (bus), "message::clock-lost",
      G_CALLBACK (clock_lost_cb), self);
  g_signal_connect (G_OBJECT (bus), "message::duration-changed",
      G_CALLBACK (duration_changed_cb), self);
  g_signal_connect (G_OBJECT (bus), "message::latency",
      G_CALLBACK (latency_cb), self);
  g_signal_connect (G_OBJECT (bus), "message::request-state",
      G_CALLBACK (request_state_cb), self);
  g_signal_connect (G_OBJECT (bus), "message::element",
      G_CALLBACK (element_cb), self);

  self->target_state = GST_STATE_NULL;
  self->current_state = GST_STATE_NULL;
  self->is_eos = FALSE;
  self->is_live = FALSE;
  self->app_state = GST_TRANSCODER_STATE_STOPPED;

  GST_TRACE_OBJECT (self, "Starting main loop");
  g_main_loop_run (self->loop);
  GST_TRACE_OBJECT (self, "Stopped main loop");

  gst_bus_remove_signal_watch (bus);
  gst_object_unref (bus);

  remove_tick_source (self);

  g_main_context_pop_thread_default (self->context);

  self->target_state = GST_STATE_NULL;
  self->current_state = GST_STATE_NULL;
  if (self->transcodebin) {
    gst_element_set_state (self->transcodebin, GST_STATE_NULL);
    g_clear_object (&self->transcodebin);
  }

  GST_TRACE_OBJECT (self, "Stopped main thread");

  return NULL;
}

static gpointer
gst_transcoder_init_once (G_GNUC_UNUSED gpointer user_data)
{
  GST_DEBUG_CATEGORY_INIT (gst_transcoder_debug, "gst-transcoder", 0,
      "GstTranscoder");
  gst_transcoder_error_quark ();

  return NULL;
}

static GstEncodingProfile *
create_encoding_profile (const gchar * pname)
{
  GstEncodingProfile *profile;
  GValue value = G_VALUE_INIT;

  g_value_init (&value, GST_TYPE_ENCODING_PROFILE);

  if (!gst_value_deserialize (&value, pname)) {
    g_value_reset (&value);

    return NULL;
  }

  profile = g_value_dup_object (&value);
  g_value_reset (&value);

  return profile;
}

/**
 * gst_transcoder_new:
 * @source_uri: The URI of the media stream to transcode
 * @dest_uri: The URI of the destination of the transcoded stream
 * @encoding_profile: The serialized #GstEncodingProfile defining the output
 * format. Have a look at the #GstEncodingProfile documentation to find more
 * about the serialization format.
 *
 * Returns: a new #GstTranscoder instance
 */
GstTranscoder *
gst_transcoder_new (const gchar * source_uri,
    const gchar * dest_uri, const gchar * encoding_profile)
{
  GstEncodingProfile *profile;

  g_once (&once, gst_transcoder_init_once, NULL);

  g_return_val_if_fail (source_uri, NULL);
  g_return_val_if_fail (dest_uri, NULL);
  g_return_val_if_fail (encoding_profile, NULL);

  profile = create_encoding_profile (encoding_profile);

  return gst_transcoder_new_full (source_uri, dest_uri, profile);
}

/**
 * gst_transcoder_new_full:
 * @source_uri: The URI of the media stream to transcode
 * @dest_uri: The URI of the destination of the transcoded stream
 * @profile: The #GstEncodingProfile defining the output format
 * have a look at the #GstEncodingProfile documentation to find more
 * about the serialization format.
 *
 * Returns: a new #GstTranscoder instance
 */
GstTranscoder *
gst_transcoder_new_full (const gchar * source_uri,
    const gchar * dest_uri, GstEncodingProfile * profile)
{
  g_once (&once, gst_transcoder_init_once, NULL);

  g_return_val_if_fail (source_uri, NULL);
  g_return_val_if_fail (dest_uri, NULL);

  return g_object_new (GST_TYPE_TRANSCODER, "src-uri", source_uri,
      "dest-uri", dest_uri, "profile", profile, NULL);
}

typedef struct
{
  GError *error;
  GMainLoop *loop;
} RunSyncData;

static void
_error_cb (RunSyncData * data, GError * error, GstStructure * details)
{
  if (data->error == NULL)
    data->error = g_error_copy (error);

  if (data->loop) {
    g_main_loop_quit (data->loop);
    g_main_loop_unref (data->loop);
    data->loop = NULL;
  }
}

static void
_done_cb (RunSyncData * data)
{
  if (data->loop) {
    g_main_loop_quit (data->loop);
    g_main_loop_unref (data->loop);
    data->loop = NULL;
  }
}

/**
 * gst_transcoder_run:
 * @self: The GstTranscoder to run
 * @error: (allow-none): An error to be set if transcoding fails
 *
 * Run the transcoder task synchonously. You can connect
 * to the 'position' signal to get information about the
 * progress of the transcoding.
 */
gboolean
gst_transcoder_run (GstTranscoder * self, GError ** error)
{
  RunSyncData data = { 0, };
  GstTranscoderSignalAdapter *signal_adapter;

  g_return_val_if_fail (GST_IS_TRANSCODER (self), FALSE);

  signal_adapter = gst_transcoder_get_signal_adapter (self, NULL);

  data.loop = g_main_loop_new (NULL, FALSE);
  g_signal_connect_swapped (signal_adapter, "error", G_CALLBACK (_error_cb),
      &data);
  g_signal_connect_swapped (signal_adapter, "done", G_CALLBACK (_done_cb),
      &data);
  gst_transcoder_run_async (self);

  if (!data.error)
    g_main_loop_run (data.loop);

  gst_element_set_state (self->transcodebin, GST_STATE_NULL);
  g_object_unref (signal_adapter);

  if (data.error) {
    if (error)
      g_propagate_error (error, data.error);

    return FALSE;
  }

  return TRUE;
}

/**
 * gst_transcoder_run_async:
 * @self: The GstTranscoder to run
 *
 * Run the transcoder task asynchronously. You should connect
 * to the 'done' signal to be notified about when the
 * transcoding is done, and to the 'error' signal to be
 * notified about any error.
 */
void
gst_transcoder_run_async (GstTranscoder * self)
{
  GstStateChangeReturn state_ret;

  g_return_if_fail (GST_IS_TRANSCODER (self));

  GST_DEBUG_OBJECT (self, "Play");

  if (!self->profile) {
    GError *err = g_error_new (GST_TRANSCODER_ERROR,
        GST_TRANSCODER_ERROR_FAILED, "No \"profile\" provided");

    api_bus_post_message (self, GST_TRANSCODER_MESSAGE_ERROR,
        GST_TRANSCODER_MESSAGE_DATA_ERROR, G_TYPE_ERROR, err, NULL);
    g_error_free (err);

    return;
  }

  self->target_state = GST_STATE_PLAYING;
  state_ret = gst_element_set_state (self->transcodebin, GST_STATE_PLAYING);

  if (state_ret == GST_STATE_CHANGE_FAILURE) {
    GError *err = g_error_new (GST_TRANSCODER_ERROR,
        GST_TRANSCODER_ERROR_FAILED, "Could not start transcoding");
    api_bus_post_message (self, GST_TRANSCODER_MESSAGE_ERROR,
        GST_TRANSCODER_MESSAGE_DATA_ERROR, G_TYPE_ERROR, err, NULL);
    g_error_free (err);

    return;
  } else if (state_ret == GST_STATE_CHANGE_NO_PREROLL) {
    self->is_live = TRUE;
    GST_DEBUG_OBJECT (self, "Pipeline is live");
  }

  return;
}

static gboolean
gst_transcoder_set_position_update_interval_internal (gpointer user_data)
{
  GstTranscoder *self = user_data;

  GST_OBJECT_LOCK (self);

  if (self->tick_source) {
    remove_tick_source (self);
    add_tick_source (self);
  }

  GST_OBJECT_UNLOCK (self);

  return G_SOURCE_REMOVE;
}

/**
 * gst_transcoder_set_position_update_interval:
 * @self: #GstTranscoder instance
 * @interval: interval in ms
 *
 * Set interval in milliseconds between two position-updated signals.
 * Pass 0 to stop updating the position.
 */
void
gst_transcoder_set_position_update_interval (GstTranscoder * self,
    guint interval)
{
  g_return_if_fail (GST_IS_TRANSCODER (self));
  g_return_if_fail (interval <= 10000);

  GST_OBJECT_LOCK (self);
  self->position_update_interval_ms = interval;
  GST_OBJECT_UNLOCK (self);

  gst_transcoder_set_position_update_interval_internal (self);
}

/**
 * gst_transcoder_get_position_update_interval:
 * @self: #GstTranscoder instance
 *
 * Returns: current position update interval in milliseconds
 */
guint
gst_transcoder_get_position_update_interval (GstTranscoder * self)
{
  g_return_val_if_fail (GST_IS_TRANSCODER (self),
      DEFAULT_POSITION_UPDATE_INTERVAL_MS);

  return self->position_update_interval_ms;
}

/**
 * gst_transcoder_get_source_uri:
 * @self: #GstTranscoder instance
 *
 * Gets the URI of the currently-transcoding stream.
 *
 * Returns: (transfer full): a string containing the URI of the
 * source stream. g_free() after usage.
 */
gchar *
gst_transcoder_get_source_uri (GstTranscoder * self)
{
  gchar *val;

  g_return_val_if_fail (GST_IS_TRANSCODER (self), DEFAULT_URI);

  g_object_get (self, "src-uri", &val, NULL);

  return val;
}

/**
 * gst_transcoder_get_dest_uri:
 * @self: #GstTranscoder instance
 *
 * Gets the URI of the destination of the transcoded stream.
 *
 * Returns: (transfer full): a string containing the URI of the
 * destination of the transcoded stream. g_free() after usage.
 */
gchar *
gst_transcoder_get_dest_uri (GstTranscoder * self)
{
  gchar *val;

  g_return_val_if_fail (GST_IS_TRANSCODER (self), DEFAULT_URI);

  g_object_get (self, "dest-uri", &val, NULL);

  return val;
}

/**
 * gst_transcoder_get_position:
 * @self: #GstTranscoder instance
 *
 * Returns: the absolute position time, in nanoseconds, of the
 * transcoding stream.
 */
GstClockTime
gst_transcoder_get_position (GstTranscoder * self)
{
  GstClockTime val;

  g_return_val_if_fail (GST_IS_TRANSCODER (self), DEFAULT_POSITION);

  g_object_get (self, "position", &val, NULL);

  return val;
}

/**
 * gst_transcoder_get_duration:
 * @self: #GstTranscoder instance
 *
 * Retrieves the duration of the media stream that self represents.
 *
 * Returns: the duration of the transcoding media stream, in
 * nanoseconds.
 */
GstClockTime
gst_transcoder_get_duration (GstTranscoder * self)
{
  GstClockTime val;

  g_return_val_if_fail (GST_IS_TRANSCODER (self), DEFAULT_DURATION);

  g_object_get (self, "duration", &val, NULL);

  return val;
}

/**
 * gst_transcoder_get_pipeline:
 * @self: #GstTranscoder instance
 *
 * Returns: (transfer full): The internal uritranscodebin instance
 */
GstElement *
gst_transcoder_get_pipeline (GstTranscoder * self)
{
  GstElement *val;

  g_return_val_if_fail (GST_IS_TRANSCODER (self), NULL);

  g_object_get (self, "pipeline", &val, NULL);

  return val;
}

/**
 * gst_transcoder_get_avoid_reencoding:
 * @self: The #GstTranscoder to check whether reencoding is avoided or not.
 *
 * Returns: %TRUE if the transcoder tries to avoid reencoding streams where
 * reencoding is not strictly needed, %FALSE otherwise.
 */
gboolean
gst_transcoder_get_avoid_reencoding (GstTranscoder * self)
{
  gboolean val;

  g_return_val_if_fail (GST_IS_TRANSCODER (self), FALSE);

  g_object_get (self->transcodebin, "avoid-reencoding", &val, NULL);

  return val;
}

/**
 * gst_transcoder_set_avoid_reencoding:
 * @self: The #GstTranscoder to set whether reencoding should be avoided or not.
 * @avoid_reencoding: %TRUE if the transcoder should try to avoid reencoding
 * streams where * reencoding is not strictly needed, %FALSE otherwise.
 */
void
gst_transcoder_set_avoid_reencoding (GstTranscoder * self,
    gboolean avoid_reencoding)
{
  g_return_if_fail (GST_IS_TRANSCODER (self));

  g_object_set (self->transcodebin, "avoid-reencoding", avoid_reencoding, NULL);
}

/**
 * gst_transcoder_error_get_name:
 * @error: a #GstTranscoderError
 *
 * Gets a string representing the given error.
 *
 * Returns: (transfer none): a string with the given error.
 */
const gchar *
gst_transcoder_error_get_name (GstTranscoderError error)
{
  switch (error) {
    case GST_TRANSCODER_ERROR_FAILED:
      return "failed";
  }

  g_assert_not_reached ();
  return NULL;
}

/**
 * gst_transcoder_get_message_bus:
 * @transcoder: #GstTranscoder instance
 *
 * GstTranscoder API exposes a #GstBus instance which purpose is to provide data
 * structures representing transcoder-internal events in form of #GstMessage-s of
 * type GST_MESSAGE_APPLICATION.
 *
 * Each message carries a "transcoder-message" field of type #GstTranscoderMessage.
 * Further fields of the message data are specific to each possible value of
 * that enumeration.
 *
 * Applications can consume the messages asynchronously within their own
 * event-loop / UI-thread etc. Note that in case the application does not
 * consume the messages, the bus will accumulate these internally and eventually
 * fill memory. To avoid that, the bus has to be set "flushing".
 *
 * Returns: (transfer full): The transcoder message bus instance
 *
 * Since: 1.20
 */
GstBus *
gst_transcoder_get_message_bus (GstTranscoder * self)
{
  g_return_val_if_fail (GST_IS_TRANSCODER (self), NULL);

  return g_object_ref (self->api_bus);
}

/**
 * gst_transcoder_get_sync_signal_adapter:
 * @self: (transfer none): #GstTranscoder instance to emit signals synchronously
 * for.
 *
 * Gets the #GstTranscoderSignalAdapter attached to @self to emit signals from
 * its thread of emission.
 *
 * Returns: (transfer full): The #GstTranscoderSignalAdapter to connect signal
 * handlers to.
 *
 * Since: 1.20
 */
GstTranscoderSignalAdapter *
gst_transcoder_get_sync_signal_adapter (GstTranscoder * self)
{
  g_return_val_if_fail (GST_IS_TRANSCODER (self), NULL);

  GST_OBJECT_LOCK (self);
  if (!self->sync_signal_adapter)
    self->sync_signal_adapter =
        gst_transcoder_signal_adapter_new_sync_emit (self);
  GST_OBJECT_UNLOCK (self);

  return g_object_ref (self->sync_signal_adapter);
}

/**
 * gst_transcoder_get_signal_adapter:
 * @self: (transfer none): #GstTranscoder instance to emit signals for.
 * @context: (nullable): A #GMainContext on which the main-loop will process
 *                       transcoder bus messages on. Can be NULL (thread-default
 *                       context will be used then).
 *
 * Gets the #GstTranscoderSignalAdapter attached to @self if it is attached to
 * the right #GMainContext. If no #GstTranscoderSignalAdapter has been created
 * yet, it will be created and returned, other calls will return that same
 * adapter until it is destroyed, at which point, a new one can be attached the
 * same way.
 *
 * Returns: (transfer full)(nullable): The #GstTranscoderSignalAdapter to
 * connect signal handlers to.
 *
 * Since: 1.20
 */
GstTranscoderSignalAdapter *
gst_transcoder_get_signal_adapter (GstTranscoder * self, GMainContext * context)
{
  g_return_val_if_fail (GST_IS_TRANSCODER (self), NULL);

  if (!context)
    context = g_main_context_get_thread_default ();
  if (!context)
    context = g_main_context_default ();

  GST_OBJECT_LOCK (self);
  if (!self->signal_adapter) {
    self->signal_adapter = gst_transcoder_signal_adapter_new (self, context);
  } else if (g_source_get_context (self->signal_adapter->source) != context) {
    GST_WARNING_OBJECT (self, "Trying to get an adapter for a different "
        "GMainContext than the one attached, this is not possible");
    GST_OBJECT_UNLOCK (self);

    return NULL;
  }
  GST_OBJECT_UNLOCK (self);

  return g_object_ref (self->signal_adapter);
}

/**
 * gst_transcoder_message_get_name:
 * @message: a #GstTranscoderMessage
 *
 * Returns (transfer none): The message name
 *
 * Since: 1.20
 */
const gchar *
gst_transcoder_message_get_name (GstTranscoderMessage message)
{
  GEnumClass *enum_class;
  GEnumValue *enum_value;
  enum_class = g_type_class_ref (GST_TYPE_TRANSCODER_MESSAGE);
  enum_value = g_enum_get_value (enum_class, message);
  g_assert (enum_value != NULL);
  g_type_class_unref (enum_class);
  return enum_value->value_name;
}


#define PARSE_MESSAGE_FIELD(msg, field, value_type, value) G_STMT_START { \
    const GstStructure *data = NULL;                                      \
    g_return_if_fail (gst_transcoder_is_transcoder_message (msg));                \
    data = gst_message_get_structure (msg);                               \
    if (!gst_structure_get (data, field, value_type, value, NULL)) {      \
      g_error ("Could not parse field from structure: %s", field);        \
    }                                                                     \
} G_STMT_END

/**
 * gst_transcoder_is_transcoder_message:
 * @msg: A #GstMessage
 *
 * Returns: A #gboolean indicating whether the passes message represents a #GstTranscoder message or not.
 *
 * Since: 1.20
 */
gboolean
gst_transcoder_is_transcoder_message (GstMessage * msg)
{
  const GstStructure *data = NULL;
  g_return_val_if_fail (GST_IS_MESSAGE (msg), FALSE);

  data = gst_message_get_structure (msg);
  g_return_val_if_fail (data, FALSE);

  return g_str_equal (gst_structure_get_name (data),
      GST_TRANSCODER_MESSAGE_DATA);
}

/**
 * gst_transcoder_message_parse_duration:
 * @msg: A #GstMessage
 * @duration: (out): the resulting duration
 *
 * Parse the given duration @msg and extract the corresponding #GstClockTime
 *
 * Since: 1.20
 */
void
gst_transcoder_message_parse_duration (GstMessage * msg,
    GstClockTime * duration)
{
  PARSE_MESSAGE_FIELD (msg, GST_TRANSCODER_MESSAGE_DATA_DURATION,
      GST_TYPE_CLOCK_TIME, duration);
}

/**
 * gst_transcoder_message_parse_position:
 * @msg: A #GstMessage
 * @position: (out): the resulting position
 *
 * Parse the given position @msg and extract the corresponding #GstClockTime
 *
 * Since: 1.20
 */
void
gst_transcoder_message_parse_position (GstMessage * msg,
    GstClockTime * position)
{
  PARSE_MESSAGE_FIELD (msg, GST_TRANSCODER_MESSAGE_DATA_POSITION,
      GST_TYPE_CLOCK_TIME, position);
}

/**
 * gst_transcoder_message_parse_state:
 * @msg: A #GstMessage
 * @state: (out): the resulting state
 *
 * Parse the given state @msg and extract the corresponding #GstTranscoderState
 *
 * Since: 1.20
 */
void
gst_transcoder_message_parse_state (GstMessage * msg,
    GstTranscoderState * state)
{
  PARSE_MESSAGE_FIELD (msg, GST_TRANSCODER_MESSAGE_DATA_STATE,
      GST_TYPE_TRANSCODER_STATE, state);
}

/**
 * gst_transcoder_message_parse_error:
 * @msg: A #GstMessage
 * @error: (out) (optional) (transfer full): the resulting error
 * @details: (out): (transfer full): A GstStructure containing extra details about the error
 *
 * Parse the given error @msg and extract the corresponding #GError
 *
 * Since: 1.20
 */
void
gst_transcoder_message_parse_error (GstMessage * msg, GError * error,
    GstStructure ** details)
{
  PARSE_MESSAGE_FIELD (msg, GST_TRANSCODER_MESSAGE_DATA_ERROR, G_TYPE_ERROR,
      error);
  PARSE_MESSAGE_FIELD (msg, GST_TRANSCODER_MESSAGE_DATA_ISSUE_DETAILS,
      GST_TYPE_STRUCTURE, details);
}

/**
 * gst_transcoder_message_parse_warning:
 * @msg: A #GstMessage
 * @error: (out) (optional) (transfer full): the resulting warning
 * @details: (out): (transfer full): A GstStructure containing extra details about the warning
 *
 * Parse the given error @msg and extract the corresponding #GError warning
 *
 * Since: 1.20
 */
void
gst_transcoder_message_parse_warning (GstMessage * msg, GError * error,
    GstStructure ** details)
{
  PARSE_MESSAGE_FIELD (msg, GST_TRANSCODER_MESSAGE_DATA_WARNING, G_TYPE_ERROR,
      error);
  PARSE_MESSAGE_FIELD (msg, GST_TRANSCODER_MESSAGE_DATA_ISSUE_DETAILS,
      GST_TYPE_STRUCTURE, details);
}

/**
 * gst_transcoder_state_get_name:
 * @state: a #GstTranscoderState
 *
 * Gets a string representing the given state.
 *
 * Returns: (transfer none): a string with the name of the state.
 *
 * Since: 1.20
 */
const gchar *
gst_transcoder_state_get_name (GstTranscoderState state)
{
  switch (state) {
    case GST_TRANSCODER_STATE_STOPPED:
      return "stopped";
    case GST_TRANSCODER_STATE_PAUSED:
      return "paused";
    case GST_TRANSCODER_STATE_PLAYING:
      return "playing";
  }

  g_assert_not_reached ();
  return NULL;
}
