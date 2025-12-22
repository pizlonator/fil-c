/* GStreamer
 * Copyright (C) 2014 David Schleef <ds@schleef.org>
 * Copyright (C) 2017 Make.TV, Inc. <info@make.tv>
 *   Contact: Jan Alexander Steffens (heftig) <jsteffens@make.tv>
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
 * Free Software Foundation, Inc., 51 Franklin Street, Suite 500,
 * Boston, MA 02110-1335, USA.
 */
/**
 * SECTION:element-rtmp2src
 *
 * The rtmp2src element receives input streams from an RTMP server.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v rtmp2src ! decodebin ! fakesink
 * ]|
 * FIXME Describe what the pipeline does.
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstrtmp2elements.h"
#include "gstrtmp2src.h"

#include "gstrtmp2locationhandler.h"
#include "rtmp/rtmpclient.h"
#include "rtmp/rtmpmessage.h"

#include <gst/base/gstpushsrc.h>
#include <string.h>

GST_DEBUG_CATEGORY_STATIC (gst_rtmp2_src_debug_category);
#define GST_CAT_DEFAULT gst_rtmp2_src_debug_category

/* prototypes */
#define GST_RTMP2_SRC(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_RTMP2_SRC,GstRtmp2Src))
#define GST_IS_RTMP2_SRC(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_RTMP2_SRC))

typedef struct
{
  GstPushSrc parent_instance;

  /* properties */
  GstRtmpLocation location;
  gboolean async_connect;
  GstStructure *stats;
  guint idle_timeout;
  gboolean no_eof_is_error;

  /* If both self->lock and OBJECT_LOCK are needed,
   * self->lock must be taken first */
  GMutex lock;
  GCond cond;

  gboolean running, flushing;
  gboolean timeout;
  gboolean started;
  /* TRUE if there was an error with the connection to the RTMP server */
  gboolean connection_error;

  GstTask *task;
  GRecMutex task_lock;

  GMainLoop *loop;
  GMainContext *context;

  GCancellable *cancellable;
  GstRtmpConnection *connection;
  guint32 stream_id;

  GstBuffer *message;
  gboolean sent_header;
  GstClockTime last_ts;
} GstRtmp2Src;

typedef struct
{
  GstPushSrcClass parent_class;
} GstRtmp2SrcClass;

/* GObject virtual functions */
static void gst_rtmp2_src_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_rtmp2_src_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_rtmp2_src_finalize (GObject * object);
static void gst_rtmp2_src_uri_handler_init (GstURIHandlerInterface * iface);

/* GstBaseSrc virtual functions */
static gboolean gst_rtmp2_src_start (GstBaseSrc * src);
static gboolean gst_rtmp2_src_stop (GstBaseSrc * src);
static gboolean gst_rtmp2_src_unlock (GstBaseSrc * src);
static gboolean gst_rtmp2_src_unlock_stop (GstBaseSrc * src);
static GstFlowReturn gst_rtmp2_src_create (GstBaseSrc * src, guint64 offset,
    guint size, GstBuffer ** outbuf);
static gboolean gst_rtmp2_src_query (GstBaseSrc * src, GstQuery * query);

/* Internal API */
static void gst_rtmp2_src_task_func (gpointer user_data);
static void client_connect_done (GObject * source, GAsyncResult * result,
    gpointer user_data);
static void start_play_done (GObject * object, GAsyncResult * result,
    gpointer user_data);
static void connect_task_done (GObject * object, GAsyncResult * result,
    gpointer user_data);

static GstStructure *gst_rtmp2_src_get_stats (GstRtmp2Src * self);

enum
{
  PROP_0,
  PROP_LOCATION,
  PROP_SCHEME,
  PROP_HOST,
  PROP_PORT,
  PROP_APPLICATION,
  PROP_STREAM,
  PROP_SECURE_TOKEN,
  PROP_USERNAME,
  PROP_PASSWORD,
  PROP_AUTHMOD,
  PROP_TIMEOUT,
  PROP_TLS_VALIDATION_FLAGS,
  PROP_FLASH_VERSION,
  PROP_ASYNC_CONNECT,
  PROP_STATS,
  PROP_IDLE_TIMEOUT,
  PROP_NO_EOF_IS_ERROR,
};

#define DEFAULT_IDLE_TIMEOUT 0

/* pad templates */

static GstStaticPadTemplate gst_rtmp2_src_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-flv")
    );

/* class initialization */

G_DEFINE_TYPE_WITH_CODE (GstRtmp2Src, gst_rtmp2_src, GST_TYPE_PUSH_SRC,
    G_IMPLEMENT_INTERFACE (GST_TYPE_URI_HANDLER,
        gst_rtmp2_src_uri_handler_init);
    G_IMPLEMENT_INTERFACE (GST_TYPE_RTMP_LOCATION_HANDLER, NULL));
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (rtmp2src, "rtmp2src",
    GST_RANK_PRIMARY + 1, GST_TYPE_RTMP2_SRC, rtmp2_element_init (plugin));

static void
gst_rtmp2_src_class_init (GstRtmp2SrcClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstBaseSrcClass *base_src_class = GST_BASE_SRC_CLASS (klass);

  gst_element_class_add_static_pad_template (GST_ELEMENT_CLASS (klass),
      &gst_rtmp2_src_src_template);

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
      "RTMP source element", "Source", "Source element for RTMP streams",
      "Make.TV, Inc. <info@make.tv>");

  gobject_class->set_property = gst_rtmp2_src_set_property;
  gobject_class->get_property = gst_rtmp2_src_get_property;
  gobject_class->finalize = gst_rtmp2_src_finalize;
  base_src_class->start = GST_DEBUG_FUNCPTR (gst_rtmp2_src_start);
  base_src_class->stop = GST_DEBUG_FUNCPTR (gst_rtmp2_src_stop);
  base_src_class->unlock = GST_DEBUG_FUNCPTR (gst_rtmp2_src_unlock);
  base_src_class->unlock_stop = GST_DEBUG_FUNCPTR (gst_rtmp2_src_unlock_stop);
  base_src_class->create = GST_DEBUG_FUNCPTR (gst_rtmp2_src_create);
  base_src_class->query = GST_DEBUG_FUNCPTR (gst_rtmp2_src_query);

  g_object_class_override_property (gobject_class, PROP_LOCATION, "location");
  g_object_class_override_property (gobject_class, PROP_SCHEME, "scheme");
  g_object_class_override_property (gobject_class, PROP_HOST, "host");
  g_object_class_override_property (gobject_class, PROP_PORT, "port");
  g_object_class_override_property (gobject_class, PROP_APPLICATION,
      "application");
  g_object_class_override_property (gobject_class, PROP_STREAM, "stream");
  g_object_class_override_property (gobject_class, PROP_SECURE_TOKEN,
      "secure-token");
  g_object_class_override_property (gobject_class, PROP_USERNAME, "username");
  g_object_class_override_property (gobject_class, PROP_PASSWORD, "password");
  g_object_class_override_property (gobject_class, PROP_AUTHMOD, "authmod");
  g_object_class_override_property (gobject_class, PROP_TIMEOUT, "timeout");
  g_object_class_override_property (gobject_class, PROP_TLS_VALIDATION_FLAGS,
      "tls-validation-flags");
  g_object_class_override_property (gobject_class, PROP_FLASH_VERSION,
      "flash-version");

  g_object_class_install_property (gobject_class, PROP_ASYNC_CONNECT,
      g_param_spec_boolean ("async-connect", "Async connect",
          "Connect on READY, otherwise on first push", TRUE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_STATS,
      g_param_spec_boxed ("stats", "Stats", "Retrieve a statistics structure",
          GST_TYPE_STRUCTURE, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_IDLE_TIMEOUT,
      g_param_spec_uint ("idle-timeout", "Idle timeout",
          "The maximum allowed time in seconds for valid packets not to arrive "
          "from the peer (0 = no timeout)",
          0, G_MAXUINT, DEFAULT_IDLE_TIMEOUT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstRtmp2Src:no-eof-is-error:
   *
   * If set, an error is raised if the connection is closed without receiving an EOF RTMP message first.
   " If not set, those are reported using EOS.
   *
   * Since: 1.24
   */
  g_object_class_install_property (gobject_class, PROP_NO_EOF_IS_ERROR,
      g_param_spec_boolean ("no-eof-is-error",
          "No EOF is error",
          "If set, an error is raised if the connection is closed without receiving an EOF RTMP message first. "
          "If not set, those are reported using EOS", FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  GST_DEBUG_CATEGORY_INIT (gst_rtmp2_src_debug_category, "rtmp2src", 0,
      "debug category for rtmp2src element");
}

static void
gst_rtmp2_src_init (GstRtmp2Src * self)
{
  self->async_connect = TRUE;
  self->idle_timeout = DEFAULT_IDLE_TIMEOUT;

  g_mutex_init (&self->lock);
  g_cond_init (&self->cond);

  self->task = gst_task_new (gst_rtmp2_src_task_func, self, NULL);
  g_rec_mutex_init (&self->task_lock);
  gst_task_set_lock (self->task, &self->task_lock);
}

static void
gst_rtmp2_src_uri_handler_init (GstURIHandlerInterface * iface)
{
  gst_rtmp_location_handler_implement_uri_handler (iface, GST_URI_SRC);
}

static void
gst_rtmp2_src_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstRtmp2Src *self = GST_RTMP2_SRC (object);

  switch (property_id) {
    case PROP_LOCATION:
      gst_rtmp_location_handler_set_uri (GST_RTMP_LOCATION_HANDLER (self),
          g_value_get_string (value));
      break;
    case PROP_SCHEME:
      GST_OBJECT_LOCK (self);
      self->location.scheme = g_value_get_enum (value);
      GST_OBJECT_UNLOCK (self);
      break;
    case PROP_HOST:
      GST_OBJECT_LOCK (self);
      g_free (self->location.host);
      self->location.host = g_value_dup_string (value);
      GST_OBJECT_UNLOCK (self);
      break;
    case PROP_PORT:
      GST_OBJECT_LOCK (self);
      self->location.port = g_value_get_int (value);
      GST_OBJECT_UNLOCK (self);
      break;
    case PROP_APPLICATION:
      GST_OBJECT_LOCK (self);
      g_free (self->location.application);
      self->location.application = g_value_dup_string (value);
      GST_OBJECT_UNLOCK (self);
      break;
    case PROP_STREAM:
      GST_OBJECT_LOCK (self);
      g_free (self->location.stream);
      self->location.stream = g_value_dup_string (value);
      GST_OBJECT_UNLOCK (self);
      break;
    case PROP_SECURE_TOKEN:
      GST_OBJECT_LOCK (self);
      g_free (self->location.secure_token);
      self->location.secure_token = g_value_dup_string (value);
      GST_OBJECT_UNLOCK (self);
      break;
    case PROP_USERNAME:
      GST_OBJECT_LOCK (self);
      g_free (self->location.username);
      self->location.username = g_value_dup_string (value);
      GST_OBJECT_UNLOCK (self);
      break;
    case PROP_PASSWORD:
      GST_OBJECT_LOCK (self);
      g_free (self->location.password);
      self->location.password = g_value_dup_string (value);
      GST_OBJECT_UNLOCK (self);
      break;
    case PROP_AUTHMOD:
      GST_OBJECT_LOCK (self);
      self->location.authmod = g_value_get_enum (value);
      GST_OBJECT_UNLOCK (self);
      break;
    case PROP_TIMEOUT:
      GST_OBJECT_LOCK (self);
      self->location.timeout = g_value_get_uint (value);
      GST_OBJECT_UNLOCK (self);
      break;
    case PROP_TLS_VALIDATION_FLAGS:
      GST_OBJECT_LOCK (self);
      self->location.tls_flags = g_value_get_flags (value);
      GST_OBJECT_UNLOCK (self);
      break;
    case PROP_FLASH_VERSION:
      GST_OBJECT_LOCK (self);
      g_free (self->location.flash_ver);
      self->location.flash_ver = g_value_dup_string (value);
      GST_OBJECT_UNLOCK (self);
      break;
    case PROP_ASYNC_CONNECT:
      GST_OBJECT_LOCK (self);
      self->async_connect = g_value_get_boolean (value);
      GST_OBJECT_UNLOCK (self);
      break;
    case PROP_IDLE_TIMEOUT:
      GST_OBJECT_LOCK (self);
      self->idle_timeout = g_value_get_uint (value);
      GST_OBJECT_UNLOCK (self);
      break;
    case PROP_NO_EOF_IS_ERROR:
      GST_OBJECT_LOCK (self);
      self->no_eof_is_error = g_value_get_boolean (value);
      GST_OBJECT_UNLOCK (self);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
gst_rtmp2_src_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstRtmp2Src *self = GST_RTMP2_SRC (object);

  switch (property_id) {
    case PROP_LOCATION:
      GST_OBJECT_LOCK (self);
      g_value_take_string (value, gst_rtmp_location_get_string (&self->location,
              TRUE));
      GST_OBJECT_UNLOCK (self);
      break;
    case PROP_SCHEME:
      GST_OBJECT_LOCK (self);
      g_value_set_enum (value, self->location.scheme);
      GST_OBJECT_UNLOCK (self);
      break;
    case PROP_HOST:
      GST_OBJECT_LOCK (self);
      g_value_set_string (value, self->location.host);
      GST_OBJECT_UNLOCK (self);
      break;
    case PROP_PORT:
      GST_OBJECT_LOCK (self);
      g_value_set_int (value, self->location.port);
      GST_OBJECT_UNLOCK (self);
      break;
    case PROP_APPLICATION:
      GST_OBJECT_LOCK (self);
      g_value_set_string (value, self->location.application);
      GST_OBJECT_UNLOCK (self);
      break;
    case PROP_STREAM:
      GST_OBJECT_LOCK (self);
      g_value_set_string (value, self->location.stream);
      GST_OBJECT_UNLOCK (self);
      break;
    case PROP_SECURE_TOKEN:
      GST_OBJECT_LOCK (self);
      g_value_set_string (value, self->location.secure_token);
      GST_OBJECT_UNLOCK (self);
      break;
    case PROP_USERNAME:
      GST_OBJECT_LOCK (self);
      g_value_set_string (value, self->location.username);
      GST_OBJECT_UNLOCK (self);
      break;
    case PROP_PASSWORD:
      GST_OBJECT_LOCK (self);
      g_value_set_string (value, self->location.password);
      GST_OBJECT_UNLOCK (self);
      break;
    case PROP_AUTHMOD:
      GST_OBJECT_LOCK (self);
      g_value_set_enum (value, self->location.authmod);
      GST_OBJECT_UNLOCK (self);
      break;
    case PROP_TIMEOUT:
      GST_OBJECT_LOCK (self);
      g_value_set_uint (value, self->location.timeout);
      GST_OBJECT_UNLOCK (self);
      break;
    case PROP_TLS_VALIDATION_FLAGS:
      GST_OBJECT_LOCK (self);
      g_value_set_flags (value, self->location.tls_flags);
      GST_OBJECT_UNLOCK (self);
      break;
    case PROP_FLASH_VERSION:
      GST_OBJECT_LOCK (self);
      g_value_set_string (value, self->location.flash_ver);
      GST_OBJECT_UNLOCK (self);
      break;
    case PROP_ASYNC_CONNECT:
      GST_OBJECT_LOCK (self);
      g_value_set_boolean (value, self->async_connect);
      GST_OBJECT_UNLOCK (self);
      break;
    case PROP_STATS:
      g_value_take_boxed (value, gst_rtmp2_src_get_stats (self));
      break;
    case PROP_IDLE_TIMEOUT:
      GST_OBJECT_LOCK (self);
      g_value_set_uint (value, self->idle_timeout);
      GST_OBJECT_UNLOCK (self);
      break;
    case PROP_NO_EOF_IS_ERROR:
      GST_OBJECT_LOCK (self);
      g_value_set_boolean (value, self->no_eof_is_error);
      GST_OBJECT_UNLOCK (self);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
gst_rtmp2_src_finalize (GObject * object)
{
  GstRtmp2Src *self = GST_RTMP2_SRC (object);

  gst_buffer_replace (&self->message, NULL);

  g_clear_object (&self->cancellable);
  g_clear_object (&self->connection);

  g_clear_object (&self->task);
  g_rec_mutex_clear (&self->task_lock);

  g_mutex_clear (&self->lock);
  g_cond_clear (&self->cond);

  g_clear_pointer (&self->stats, gst_structure_free);
  gst_rtmp_location_clear (&self->location);

  G_OBJECT_CLASS (gst_rtmp2_src_parent_class)->finalize (object);
}

static gboolean
gst_rtmp2_src_start (GstBaseSrc * src)
{
  GstRtmp2Src *self = GST_RTMP2_SRC (src);
  gboolean async;

  GST_OBJECT_LOCK (self);
  async = self->async_connect;
  GST_OBJECT_UNLOCK (self);

  GST_INFO_OBJECT (self, "Starting (%s)", async ? "async" : "delayed");

  g_clear_object (&self->cancellable);

  self->running = TRUE;
  self->cancellable = g_cancellable_new ();
  self->stream_id = 0;
  self->sent_header = FALSE;
  self->last_ts = GST_CLOCK_TIME_NONE;
  self->timeout = FALSE;
  self->started = FALSE;
  self->connection_error = FALSE;

  if (async) {
    gst_task_start (self->task);
  }

  return TRUE;
}

static gboolean
quit_invoker (gpointer user_data)
{
  g_main_loop_quit (user_data);
  return G_SOURCE_REMOVE;
}

static void
stop_task (GstRtmp2Src * self)
{
  gst_task_stop (self->task);
  self->running = FALSE;

  if (self->cancellable) {
    GST_DEBUG_OBJECT (self, "Cancelling");
    g_cancellable_cancel (self->cancellable);
  }

  if (self->loop) {
    GST_DEBUG_OBJECT (self, "Stopping loop");
    g_main_context_invoke_full (self->context, G_PRIORITY_DEFAULT_IDLE,
        quit_invoker, g_main_loop_ref (self->loop),
        (GDestroyNotify) g_main_loop_unref);
  }

  g_cond_broadcast (&self->cond);
}

static gboolean
gst_rtmp2_src_stop (GstBaseSrc * src)
{
  GstRtmp2Src *self = GST_RTMP2_SRC (src);

  GST_DEBUG_OBJECT (self, "stop");

  g_mutex_lock (&self->lock);
  stop_task (self);
  g_mutex_unlock (&self->lock);

  gst_task_join (self->task);

  return TRUE;
}

static gboolean
gst_rtmp2_src_unlock (GstBaseSrc * src)
{
  GstRtmp2Src *self = GST_RTMP2_SRC (src);

  GST_DEBUG_OBJECT (self, "unlock");

  g_mutex_lock (&self->lock);
  self->flushing = TRUE;
  g_cond_broadcast (&self->cond);
  g_mutex_unlock (&self->lock);

  return TRUE;
}

static gboolean
gst_rtmp2_src_unlock_stop (GstBaseSrc * src)
{
  GstRtmp2Src *self = GST_RTMP2_SRC (src);

  GST_DEBUG_OBJECT (self, "unlock_stop");

  g_mutex_lock (&self->lock);
  self->flushing = FALSE;
  g_mutex_unlock (&self->lock);

  return TRUE;
}

static gboolean
on_timeout (GstRtmp2Src * self)
{
  g_mutex_lock (&self->lock);
  self->timeout = TRUE;
  g_cond_broadcast (&self->cond);
  g_mutex_unlock (&self->lock);

  return G_SOURCE_REMOVE;
}

static GstFlowReturn
gst_rtmp2_src_create (GstBaseSrc * src, guint64 offset, guint size,
    GstBuffer ** outbuf)
{
  GstRtmp2Src *self = GST_RTMP2_SRC (src);
  GstBuffer *message, *buffer;
  GstRtmpMeta *meta;
  guint32 timestamp = 0;
  GSource *timeout = NULL;
  GstFlowReturn ret = GST_FLOW_OK;

  static const guint8 flv_header_data[] = {
    0x46, 0x4c, 0x56, 0x01, 0x01, 0x00, 0x00, 0x00,
    0x09, 0x00, 0x00, 0x00, 0x00,
  };

  GST_LOG_OBJECT (self, "create");

  g_mutex_lock (&self->lock);

  if (self->running) {
    gst_task_start (self->task);
  }

  /* wait until GMainLoop begins running so that we can attach
   * timeout source safely.
   * If the task stopped meanwhile, "running" will be FALSE
   * than stop_task() will wake up us as well
   */
  while ((!self->started && self->running) && (!self->loop
          || !g_main_loop_is_running (self->loop)))
    g_cond_wait (&self->cond, &self->lock);

  GST_OBJECT_LOCK (self);
  if (self->idle_timeout && self->context) {
    timeout = g_timeout_source_new_seconds (self->idle_timeout);

    g_source_set_callback (timeout, (GSourceFunc) on_timeout, self, NULL);
    g_source_attach (timeout, self->context);
  }
  GST_OBJECT_UNLOCK (self);

  while (!self->message) {
    if (!self->running) {
      if (self->no_eof_is_error && self->connection_error) {
        GST_DEBUG_OBJECT (self,
            "stopped because of connection error, return ERROR");
        ret = GST_FLOW_ERROR;
      } else {
        GST_DEBUG_OBJECT (self, "stopped, return EOS");
        ret = GST_FLOW_EOS;
      }

      goto out;
    }
    if (self->flushing) {
      ret = GST_FLOW_FLUSHING;
      goto out;
    }
    if (self->timeout) {
      GST_DEBUG_OBJECT (self, "Idle timeout, return EOS");
      ret = GST_FLOW_EOS;
      goto out;
    }
    g_cond_wait (&self->cond, &self->lock);
  }

  if (timeout) {
    g_source_destroy (timeout);
    g_source_unref (timeout);
  }

  message = self->message;
  self->message = NULL;
  g_cond_signal (&self->cond);
  g_mutex_unlock (&self->lock);

  meta = gst_buffer_get_rtmp_meta (message);
  if (!meta) {
    GST_ELEMENT_ERROR (self, CORE, FAILED,
        ("Internal error: No RTMP meta on buffer"),
        ("No RTMP meta on %" GST_PTR_FORMAT, message));
    gst_buffer_unref (message);
    return GST_FLOW_ERROR;
  }

  if (GST_BUFFER_DTS_IS_VALID (message)) {
    GstClockTime last_ts = self->last_ts, ts = GST_BUFFER_DTS (message);

    if (GST_CLOCK_TIME_IS_VALID (last_ts) && last_ts > ts) {
      GST_LOG_OBJECT (self, "Timestamp regression: %" GST_TIME_FORMAT
          " > %" GST_TIME_FORMAT, GST_TIME_ARGS (last_ts), GST_TIME_ARGS (ts));
    }

    self->last_ts = ts;
    timestamp = ts / GST_MSECOND;
  }

  buffer = gst_buffer_copy_region (message, GST_BUFFER_COPY_MEMORY, 0, -1);

  {
    guint8 *tag_header = g_malloc (11);
    GstMemory *memory =
        gst_memory_new_wrapped (0, tag_header, 11, 0, 11, tag_header, g_free);
    GST_WRITE_UINT8 (tag_header, meta->type);
    GST_WRITE_UINT24_BE (tag_header + 1, meta->size);
    GST_WRITE_UINT24_BE (tag_header + 4, timestamp);
    GST_WRITE_UINT8 (tag_header + 7, timestamp >> 24);
    GST_WRITE_UINT24_BE (tag_header + 8, 0);
    gst_buffer_prepend_memory (buffer, memory);
  }

  {
    guint8 *tag_footer = g_malloc (4);
    GstMemory *memory =
        gst_memory_new_wrapped (0, tag_footer, 4, 0, 4, tag_footer, g_free);
    GST_WRITE_UINT32_BE (tag_footer, meta->size + 11);
    gst_buffer_append_memory (buffer, memory);
  }

  if (!self->sent_header) {
    GstMemory *memory = gst_memory_new_wrapped (GST_MEMORY_FLAG_READONLY,
        (guint8 *) flv_header_data, sizeof flv_header_data, 0,
        sizeof flv_header_data, NULL, NULL);
    gst_buffer_prepend_memory (buffer, memory);
    self->sent_header = TRUE;
  }

  GST_BUFFER_DTS (buffer) = self->last_ts;

  *outbuf = buffer;

  gst_buffer_unref (message);
  return GST_FLOW_OK;

out:
  if (timeout) {
    g_source_destroy (timeout);
    g_source_unref (timeout);
  }
  /* Keep the unlock after the destruction of the timeout source to workaround
   * https://gitlab.gnome.org/GNOME/glib/-/issues/803
   */
  g_mutex_unlock (&self->lock);

  return ret;
}

static gboolean
gst_rtmp2_src_query (GstBaseSrc * basesrc, GstQuery * query)
{
  gboolean ret = FALSE;

  switch (GST_QUERY_TYPE (query)) {
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
    ret =
        GST_BASE_SRC_CLASS (gst_rtmp2_src_parent_class)->query (basesrc, query);

  return ret;
}

static gboolean
main_loop_running_cb (GstRtmp2Src * self)
{
  GST_TRACE_OBJECT (self, "Main loop running now");

  g_mutex_lock (&self->lock);
  self->started = TRUE;
  g_cond_broadcast (&self->cond);
  g_mutex_unlock (&self->lock);

  return G_SOURCE_REMOVE;
}

/* Mainloop task */
static void
gst_rtmp2_src_task_func (gpointer user_data)
{
  GstRtmp2Src *self = GST_RTMP2_SRC (user_data);
  GMainContext *context;
  GMainLoop *loop;
  GTask *connector;
  GSource *source;

  GST_DEBUG_OBJECT (self, "gst_rtmp2_src_task starting");
  g_mutex_lock (&self->lock);

  context = self->context = g_main_context_new ();
  g_main_context_push_thread_default (context);
  loop = self->loop = g_main_loop_new (context, TRUE);

  source = g_idle_source_new ();
  g_source_set_callback (source, (GSourceFunc) main_loop_running_cb, self,
      NULL);
  g_source_attach (source, self->context);
  g_source_unref (source);

  connector = g_task_new (self, self->cancellable, connect_task_done, NULL);

  g_clear_pointer (&self->stats, gst_structure_free);

  GST_OBJECT_LOCK (self);
  gst_rtmp_client_connect_async (&self->location, self->cancellable,
      client_connect_done, connector);
  GST_OBJECT_UNLOCK (self);

  /* Run loop */
  g_mutex_unlock (&self->lock);
  g_main_loop_run (loop);
  g_mutex_lock (&self->lock);

  if (self->connection) {
    self->stats = gst_rtmp_connection_get_stats (self->connection);
  }

  g_clear_pointer (&self->loop, g_main_loop_unref);
  g_clear_pointer (&self->connection, gst_rtmp_connection_close_and_unref);
  g_cond_broadcast (&self->cond);

  /* Run loop cleanup */
  g_mutex_unlock (&self->lock);
  while (g_main_context_pending (context)) {
    GST_DEBUG_OBJECT (self, "iterating main context to clean up");
    g_main_context_iteration (context, FALSE);
  }
  g_main_context_pop_thread_default (context);
  g_mutex_lock (&self->lock);

  g_clear_pointer (&self->context, g_main_context_unref);
  gst_buffer_replace (&self->message, NULL);

  g_mutex_unlock (&self->lock);
  GST_DEBUG_OBJECT (self, "gst_rtmp2_src_task exiting");
}

static void
client_connect_done (GObject * source, GAsyncResult * result,
    gpointer user_data)
{
  GTask *task = user_data;
  GstRtmp2Src *self = g_task_get_source_object (task);
  GError *error = NULL;
  GstRtmpConnection *connection;

  connection = gst_rtmp_client_connect_finish (result, &error);
  if (!connection) {
    g_task_return_error (task, error);
    g_object_unref (task);
    return;
  }

  g_task_set_task_data (task, connection, g_object_unref);

  if (g_task_return_error_if_cancelled (task)) {
    g_object_unref (task);
    return;
  }

  GST_OBJECT_LOCK (self);
  gst_rtmp_client_start_play_async (connection, self->location.stream,
      g_task_get_cancellable (task), start_play_done, task);
  GST_OBJECT_UNLOCK (self);
}

static void
start_play_done (GObject * source, GAsyncResult * result, gpointer user_data)
{
  GTask *task = G_TASK (user_data);
  GstRtmp2Src *self = g_task_get_source_object (task);
  GstRtmpConnection *connection = g_task_get_task_data (task);
  GError *error = NULL;

  if (g_task_return_error_if_cancelled (task)) {
    g_object_unref (task);
    return;
  }

  if (gst_rtmp_client_start_play_finish (connection, result,
          &self->stream_id, &error)) {
    g_task_return_pointer (task, g_object_ref (connection),
        gst_rtmp_connection_close_and_unref);
  } else {
    g_task_return_error (task, error);
  }

  g_task_set_task_data (task, NULL, NULL);
  g_object_unref (task);
}

static void
got_message (GstRtmpConnection * connection, GstBuffer * buffer,
    gpointer user_data)
{
  GstRtmp2Src *self = GST_RTMP2_SRC (user_data);
  GstRtmpMeta *meta = gst_buffer_get_rtmp_meta (buffer);
  guint32 min_size = 1;

  g_return_if_fail (meta);

  if (meta->mstream != self->stream_id) {
    GST_DEBUG_OBJECT (self, "Ignoring %s message with stream %" G_GUINT32_FORMAT
        " != %" G_GUINT32_FORMAT, gst_rtmp_message_type_get_nick (meta->type),
        meta->mstream, self->stream_id);
    return;
  }

  switch (meta->type) {
    case GST_RTMP_MESSAGE_TYPE_VIDEO:
      min_size = 6;
      break;

    case GST_RTMP_MESSAGE_TYPE_AUDIO:
      min_size = 2;
      break;

    case GST_RTMP_MESSAGE_TYPE_DATA_AMF0:
      break;

    default:
      GST_DEBUG_OBJECT (self, "Ignoring %s message, wrong type",
          gst_rtmp_message_type_get_nick (meta->type));
      return;
  }

  if (meta->size < min_size) {
    GST_DEBUG_OBJECT (self, "Ignoring too small %s message (%" G_GUINT32_FORMAT
        " < %" G_GUINT32_FORMAT ")",
        gst_rtmp_message_type_get_nick (meta->type), meta->size, min_size);
    return;
  }

  g_mutex_lock (&self->lock);
  while (self->message) {
    if (!self->running) {
      goto out;
    }
    g_cond_wait (&self->cond, &self->lock);
  }

  self->message = gst_buffer_ref (buffer);
  g_cond_signal (&self->cond);

out:
  g_mutex_unlock (&self->lock);
  return;
}

static void
error_callback (GstRtmpConnection * connection, const GError * error,
    GstRtmp2Src * self)
{
  g_mutex_lock (&self->lock);
  if (self->cancellable) {
    g_cancellable_cancel (self->cancellable);
  } else if (self->loop) {
    GST_INFO_OBJECT (self, "Connection error: %s %d %s",
        g_quark_to_string (error->domain), error->code, error->message);
    self->connection_error = TRUE;
    stop_task (self);
  }
  g_mutex_unlock (&self->lock);
}

static void
control_callback (GstRtmpConnection * connection, gint uc_type,
    guint stream_id, GstRtmp2Src * self)
{
  GST_INFO_OBJECT (self, "stream %u got %s", stream_id,
      gst_rtmp_user_control_type_get_nick (uc_type));

  if (uc_type == GST_RTMP_USER_CONTROL_TYPE_STREAM_EOF && stream_id == 1) {
    GST_INFO_OBJECT (self, "went EOS");
    stop_task (self);
  }
}

static void
send_connect_error (GstRtmp2Src * self, GError * error)
{
  if (!error) {
    GST_ERROR_OBJECT (self, "Connect failed with NULL error");
    GST_ELEMENT_ERROR (self, RESOURCE, FAILED, ("Failed to connect"), (NULL));
    return;
  }

  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
    GST_DEBUG_OBJECT (self, "Connection was cancelled: %s", error->message);
    return;
  }

  GST_ERROR_OBJECT (self, "Failed to connect: %s %d %s",
      g_quark_to_string (error->domain), error->code, error->message);

  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED)) {
    GST_ELEMENT_ERROR (self, RESOURCE, NOT_AUTHORIZED,
        ("Not authorized to connect: %s", error->message), (NULL));
  } else if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CONNECTION_REFUSED)) {
    GST_ELEMENT_ERROR (self, RESOURCE, OPEN_READ,
        ("Connection refused: %s", error->message), (NULL));
  } else {
    GST_ELEMENT_ERROR (self, RESOURCE, FAILED,
        ("Failed to connect: %s", error->message),
        ("domain %s, code %d", g_quark_to_string (error->domain), error->code));
  }
}

static void
connect_task_done (GObject * object, GAsyncResult * result, gpointer user_data)
{
  GstRtmp2Src *self = GST_RTMP2_SRC (object);
  GTask *task = G_TASK (result);
  GError *error = NULL;

  g_mutex_lock (&self->lock);

  g_warn_if_fail (g_task_is_valid (task, object));

  if (self->cancellable == g_task_get_cancellable (task)) {
    g_clear_object (&self->cancellable);
  }

  self->connection = g_task_propagate_pointer (task, &error);
  if (self->connection) {
    gst_rtmp_connection_set_input_handler (self->connection,
        got_message, g_object_ref (self), g_object_unref);
    g_signal_connect_object (self->connection, "error",
        G_CALLBACK (error_callback), self, 0);
    g_signal_connect_object (self->connection, "stream-control",
        G_CALLBACK (control_callback), self, 0);
  } else {
    send_connect_error (self, error);
    self->connection_error = TRUE;
    stop_task (self);
    g_error_free (error);
  }

  g_cond_broadcast (&self->cond);
  g_mutex_unlock (&self->lock);
}

static GstStructure *
gst_rtmp2_src_get_stats (GstRtmp2Src * self)
{
  GstStructure *s;

  g_mutex_lock (&self->lock);

  if (self->connection) {
    s = gst_rtmp_connection_get_stats (self->connection);
  } else if (self->stats) {
    s = gst_structure_copy (self->stats);
  } else {
    s = gst_rtmp_connection_get_null_stats ();
  }

  g_mutex_unlock (&self->lock);

  return s;
}
