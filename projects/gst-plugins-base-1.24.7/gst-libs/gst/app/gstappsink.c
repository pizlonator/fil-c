/* GStreamer
 * Copyright (C) 2007 David Schleef <ds@schleef.org>
 *           (C) 2008 Wim Taymans <wim.taymans@gmail.com>
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
 * SECTION:gstappsink
 * @title: GstAppSink
 * @short_description: Easy way for applications to extract samples from a
 *     pipeline
 * @see_also: #GstSample, #GstBaseSink, appsrc
 *
 * Appsink is a sink plugin that supports many different methods for making
 * the application get a handle on the GStreamer data in a pipeline. Unlike
 * most GStreamer elements, Appsink provides external API functions.
 *
 * appsink can be used by linking to the gstappsink.h header file to access the
 * methods or by using the appsink action signals and properties.
 *
 * The normal way of retrieving samples from appsink is by using the
 * gst_app_sink_pull_sample() and gst_app_sink_pull_preroll() methods.
 * These methods block until a sample becomes available in the sink or when the
 * sink is shut down or reaches EOS. There are also timed variants of these
 * methods, gst_app_sink_try_pull_sample() and gst_app_sink_try_pull_preroll(),
 * which accept a timeout parameter to limit the amount of time to wait.
 *
 * Appsink will internally use a queue to collect buffers from the streaming
 * thread. If the application is not pulling samples fast enough, this queue
 * will consume a lot of memory over time. The "max-buffers", "max-time" and "max-bytes"
 * properties can be used to limit the queue size. The "drop" property controls whether the
 * streaming thread blocks or if older buffers are dropped when the maximum
 * queue size is reached. Note that blocking the streaming thread can negatively
 * affect real-time performance and should be avoided.
 *
 * If a blocking behaviour is not desirable, setting the "emit-signals" property
 * to %TRUE will make appsink emit the "new-sample" and "new-preroll" signals
 * when a sample can be pulled without blocking.
 *
 * The "caps" property on appsink can be used to control the formats that
 * appsink can receive. This property can contain non-fixed caps, the format of
 * the pulled samples can be obtained by getting the sample caps.
 *
 * If one of the pull-preroll or pull-sample methods return %NULL, the appsink
 * is stopped or in the EOS state. You can check for the EOS state with the
 * "eos" property or with the gst_app_sink_is_eos() method.
 *
 * The eos signal can also be used to be informed when the EOS state is reached
 * to avoid polling.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/base/base.h>

#include <string.h>

#include "gstappsink.h"
#include "gstapputils.h"

typedef enum
{
  NOONE_WAITING = 0,
  STREAM_WAITING = 1 << 0,      /* streaming thread is waiting for application thread */
  APP_WAITING = 1 << 1,         /* application thread is waiting for streaming thread */
} GstAppSinkWaitStatus;

typedef struct
{
  GstAppSinkCallbacks callbacks;
  gpointer user_data;
  GDestroyNotify destroy_notify;
  gint ref_count;
} Callbacks;

static Callbacks *
callbacks_ref (Callbacks * callbacks)
{
  g_atomic_int_inc (&callbacks->ref_count);

  return callbacks;
}

static void
callbacks_unref (Callbacks * callbacks)
{
  if (!g_atomic_int_dec_and_test (&callbacks->ref_count))
    return;

  if (callbacks->destroy_notify)
    callbacks->destroy_notify (callbacks->user_data);

  g_free (callbacks);
}

struct _GstAppSinkPrivate
{
  GstCaps *caps;
  gboolean emit_signals;
  guint64 max_buffers;
  GstClockTime max_time;
  guint64 max_bytes;
  gboolean drop;
  gboolean wait_on_eos;
  GstAppSinkWaitStatus wait_status;
  GstQueueStatusInfo queue_status_info;

  GCond cond;
  GMutex mutex;
  GstQueueArray *queue;
  GstBuffer *preroll_buffer;
  GstCaps *preroll_caps;
  GstCaps *last_caps;
  GstSegment preroll_segment;
  GstSegment last_segment;
  gboolean flushing;
  gboolean unlock;
  gboolean started;
  gboolean is_eos;
  gboolean buffer_lists_supported;

  Callbacks *callbacks;

  GstSample *sample;
};

GST_DEBUG_CATEGORY_STATIC (app_sink_debug);
#define GST_CAT_DEFAULT app_sink_debug

enum
{
  /* signals */
  SIGNAL_EOS,
  SIGNAL_NEW_PREROLL,
  SIGNAL_NEW_SAMPLE,
  SIGNAL_NEW_SERIALIZED_EVENT,

  /* actions */
  SIGNAL_PULL_PREROLL,
  SIGNAL_PULL_SAMPLE,
  SIGNAL_TRY_PULL_PREROLL,
  SIGNAL_TRY_PULL_SAMPLE,
  SIGNAL_TRY_PULL_OBJECT,
  SIGNAL_PROPOSE_ALLOCATION,

  LAST_SIGNAL
};

#define DEFAULT_PROP_EOS		TRUE
#define DEFAULT_PROP_EMIT_SIGNALS	FALSE
#define DEFAULT_PROP_MAX_BUFFERS	0
#define DEFAULT_PROP_MAX_TIME	0
#define DEFAULT_PROP_MAX_BYTES	0
#define DEFAULT_PROP_DROP		FALSE
#define DEFAULT_PROP_WAIT_ON_EOS	TRUE
#define DEFAULT_PROP_BUFFER_LIST	FALSE

enum
{
  PROP_0,
  PROP_CAPS,
  PROP_EOS,
  PROP_EMIT_SIGNALS,
  PROP_MAX_BUFFERS,
  PROP_DROP,
  PROP_WAIT_ON_EOS,
  PROP_BUFFER_LIST,
  PROP_MAX_TIME,
  PROP_MAX_BYTES,
  PROP_LAST
};

static GstStaticPadTemplate gst_app_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

static void gst_app_sink_uri_handler_init (gpointer g_iface,
    gpointer iface_data);

static void gst_app_sink_dispose (GObject * object);
static void gst_app_sink_finalize (GObject * object);

static void gst_app_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_app_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_app_sink_unlock_start (GstBaseSink * bsink);
static gboolean gst_app_sink_unlock_stop (GstBaseSink * bsink);
static gboolean gst_app_sink_start (GstBaseSink * psink);
static gboolean gst_app_sink_stop (GstBaseSink * psink);
static gboolean gst_app_sink_event (GstBaseSink * sink, GstEvent * event);
static gboolean gst_app_sink_query (GstBaseSink * bsink, GstQuery * query);
static GstFlowReturn gst_app_sink_preroll (GstBaseSink * psink,
    GstBuffer * buffer);
static GstFlowReturn gst_app_sink_render_common (GstBaseSink * psink,
    GstMiniObject * data, gboolean is_list);
static GstFlowReturn gst_app_sink_render (GstBaseSink * psink,
    GstBuffer * buffer);
static GstFlowReturn gst_app_sink_render_list (GstBaseSink * psink,
    GstBufferList * list);
static gboolean gst_app_sink_setcaps (GstBaseSink * sink, GstCaps * caps);
static GstCaps *gst_app_sink_getcaps (GstBaseSink * psink, GstCaps * filter);
static gboolean gst_app_sink_propose_allocation (GstBaseSink * bsink,
    GstQuery * query);

static guint gst_app_sink_signals[LAST_SIGNAL] = { 0 };

#define gst_app_sink_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstAppSink, gst_app_sink, GST_TYPE_BASE_SINK,
    G_ADD_PRIVATE (GstAppSink)
    G_IMPLEMENT_INTERFACE (GST_TYPE_URI_HANDLER,
        gst_app_sink_uri_handler_init));

static void
gst_app_sink_class_init (GstAppSinkClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *element_class = (GstElementClass *) klass;
  GstBaseSinkClass *basesink_class = (GstBaseSinkClass *) klass;

  GST_DEBUG_CATEGORY_INIT (app_sink_debug, "appsink", 0, "appsink element");

  gobject_class->dispose = gst_app_sink_dispose;
  gobject_class->finalize = gst_app_sink_finalize;

  gobject_class->set_property = gst_app_sink_set_property;
  gobject_class->get_property = gst_app_sink_get_property;

  g_object_class_install_property (gobject_class, PROP_CAPS,
      g_param_spec_boxed ("caps", "Caps",
          "The allowed caps for the sink pad", GST_TYPE_CAPS,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_EOS,
      g_param_spec_boolean ("eos", "EOS",
          "Check if the sink is EOS or not started", DEFAULT_PROP_EOS,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_EMIT_SIGNALS,
      g_param_spec_boolean ("emit-signals", "Emit signals",
          "Emit new-preroll and new-sample signals",
          DEFAULT_PROP_EMIT_SIGNALS,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstAppSink:max-buffers:
   *
   * Maximum amount of buffers in the queue (0 = unlimited).
   */
  g_object_class_install_property (gobject_class, PROP_MAX_BUFFERS,
      g_param_spec_uint ("max-buffers", "Max Buffers",
          "The maximum number of buffers to queue internally (0 = unlimited)",
          0, G_MAXUINT, DEFAULT_PROP_MAX_BUFFERS,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstAppSink:max-time:
   *
   * Maximum total duration of data in the queue (0 = unlimited)
   *
   * Since: 1.24
   */
  g_object_class_install_property (gobject_class, PROP_MAX_TIME,
      g_param_spec_uint64 ("max-time", "Max time",
          "The maximum total duration to queue internally (in ns, 0 = unlimited)",
          0, G_MAXUINT64, DEFAULT_PROP_MAX_TIME,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstAppSink:max-bytes:
   *
   * Maximum amount of bytes in the queue (0 = unlimited)
   *
   * Since: 1.24
   */
  g_object_class_install_property (gobject_class, PROP_MAX_BYTES,
      g_param_spec_uint64 ("max-bytes", "Max bytes",
          "The maximum amount of bytes to queue internally (0 = unlimited)",
          0, G_MAXUINT64, DEFAULT_PROP_MAX_BYTES,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_DROP,
      g_param_spec_boolean ("drop", "Drop",
          "Drop old buffers when the buffer queue is filled", DEFAULT_PROP_DROP,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_BUFFER_LIST,
      g_param_spec_boolean ("buffer-list", "Buffer List",
          "Use buffer lists", DEFAULT_PROP_BUFFER_LIST,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstAppSink:wait-on-eos:
   *
   * Wait for all buffers to be processed after receiving an EOS.
   *
   * In cases where it is uncertain if an @appsink will have a consumer for its buffers
   * when it receives an EOS, set to %FALSE to ensure that the @appsink will not hang.
   *
   * Since: 1.8
   */
  g_object_class_install_property (gobject_class, PROP_WAIT_ON_EOS,
      g_param_spec_boolean ("wait-on-eos", "Wait on EOS",
          "Wait for all buffers to be processed after receiving an EOS",
          DEFAULT_PROP_WAIT_ON_EOS,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstAppSink::eos:
   * @appsink: the appsink element that emitted the signal
   *
   * Signal that the end-of-stream has been reached. This signal is emitted from
   * the streaming thread.
   */
  gst_app_sink_signals[SIGNAL_EOS] =
      g_signal_new ("eos", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
      G_STRUCT_OFFSET (GstAppSinkClass, eos),
      NULL, NULL, NULL, G_TYPE_NONE, 0, G_TYPE_NONE);
  /**
   * GstAppSink::new-preroll:
   * @appsink: the appsink element that emitted the signal
   *
   * Signal that a new preroll sample is available.
   *
   * This signal is emitted from the streaming thread and only when the
   * "emit-signals" property is %TRUE.
   *
   * The new preroll sample can be retrieved with the "pull-preroll" action
   * signal or gst_app_sink_pull_preroll() either from this signal callback
   * or from any other thread.
   *
   * Note that this signal is only emitted when the "emit-signals" property is
   * set to %TRUE, which it is not by default for performance reasons.
   */
  gst_app_sink_signals[SIGNAL_NEW_PREROLL] =
      g_signal_new ("new-preroll", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
      G_STRUCT_OFFSET (GstAppSinkClass, new_preroll),
      NULL, NULL, NULL, GST_TYPE_FLOW_RETURN, 0, G_TYPE_NONE);
  /**
   * GstAppSink::new-sample:
   * @appsink: the appsink element that emitted the signal
   *
   * Signal that a new sample is available.
   *
   * This signal is emitted from the streaming thread and only when the
   * "emit-signals" property is %TRUE.
   *
   * The new sample can be retrieved with the "pull-sample" action
   * signal or gst_app_sink_pull_sample() either from this signal callback
   * or from any other thread.
   *
   * Note that this signal is only emitted when the "emit-signals" property is
   * set to %TRUE, which it is not by default for performance reasons.
   */
  gst_app_sink_signals[SIGNAL_NEW_SAMPLE] =
      g_signal_new ("new-sample", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
      G_STRUCT_OFFSET (GstAppSinkClass, new_sample),
      NULL, NULL, NULL, GST_TYPE_FLOW_RETURN, 0, G_TYPE_NONE);

  /**
   * GstAppSink::propose-allocation:
   * @appsink: the appsink element that emitted the signal
   * @query: the allocation query
   *
   * Signal that a new propose_allocation query is available.
   *
   * This signal is emitted from the streaming thread and only when the
   * "emit-signals" property is %TRUE.
   *
   * Since: 1.24
   */
  gst_app_sink_signals[SIGNAL_PROPOSE_ALLOCATION] =
      g_signal_new_class_handler ("propose-allocation",
      G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST, NULL, NULL, NULL, NULL,
      G_TYPE_BOOLEAN, 1, GST_TYPE_QUERY | G_SIGNAL_TYPE_STATIC_SCOPE);
  /**
   * GstAppSink::new-serialized-event:
   * @appsink: the appsink element that emitted the signal
   *
   * Signal that a new downstream serialized event is available.
   *
   * This signal is emitted from the streaming thread and only when the
   * "emit-signals" property is %TRUE.
   *
   * The new event can be retrieved with the "try-pull-object" action
   * signal or gst_app_sink_pull_object() either from this signal callback
   * or from any other thread.
   *
   * EOS will not be notified using this signal, use #GstAppSink::eos instead.
   * EOS cannot be pulled either, use gst_app_sink_is_eos() to check for it.
   *
   * Note that this signal is only emitted when the "emit-signals" property is
   * set to %TRUE, which it is not by default for performance reasons.
   *
   * The callback should return %TRUE if the event has been handled, which will
   * skip basesink handling of the event, %FALSE otherwise.
   *
   * Since: 1.20
   */
  gst_app_sink_signals[SIGNAL_NEW_SERIALIZED_EVENT] =
      g_signal_new ("new-serialized-event", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_BOOLEAN, 0, G_TYPE_NONE);

  /**
   * GstAppSink::pull-preroll:
   * @appsink: the appsink element to emit this signal on
   *
   * Get the last preroll sample in @appsink. This was the sample that caused the
   * appsink to preroll in the PAUSED state.
   *
   * This function is typically used when dealing with a pipeline in the PAUSED
   * state. Calling this function after doing a seek will give the sample right
   * after the seek position.
   *
   * Calling this function will clear the internal reference to the preroll
   * buffer.
   *
   * Note that the preroll sample will also be returned as the first sample
   * when calling gst_app_sink_pull_sample() or the "pull-sample" action signal.
   *
   * If an EOS event was received before any buffers, this function returns
   * %NULL. Use gst_app_sink_is_eos () to check for the EOS condition.
   *
   * This function blocks until a preroll sample or EOS is received or the appsink
   * element is set to the READY/NULL state.
   *
   * Returns: (nullable) (transfer full): a #GstSample or %NULL when the appsink is stopped or EOS.
   */
  gst_app_sink_signals[SIGNAL_PULL_PREROLL] =
      g_signal_new ("pull-preroll", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION, G_STRUCT_OFFSET (GstAppSinkClass,
          pull_preroll), NULL, NULL, NULL, GST_TYPE_SAMPLE, 0, G_TYPE_NONE);
  /**
   * GstAppSink::pull-sample:
   * @appsink: the appsink element to emit this signal on
   *
   * This function blocks until a sample or EOS becomes available or the appsink
   * element is set to the READY/NULL state.
   *
   * This function will only return samples when the appsink is in the PLAYING
   * state. All rendered samples will be put in a queue so that the application
   * can pull samples at its own rate.
   *
   * Note that when the application does not pull samples fast enough, the
   * queued samples could consume a lot of memory, especially when dealing with
   * raw video frames. It's possible to control the behaviour of the queue with
   * the "drop" and "max-buffers" / "max-bytes" / "max-time" set of properties.
   *
   * If an EOS event was received before any buffers, this function returns
   * %NULL. Use gst_app_sink_is_eos () to check for the EOS condition.
   *
   * Returns: (nullable) (transfer full): a #GstSample or %NULL when the appsink is stopped or EOS.
   */
  gst_app_sink_signals[SIGNAL_PULL_SAMPLE] =
      g_signal_new ("pull-sample", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION, G_STRUCT_OFFSET (GstAppSinkClass,
          pull_sample), NULL, NULL, NULL, GST_TYPE_SAMPLE, 0, G_TYPE_NONE);

  /**
   * GstAppSink::try-pull-preroll:
   * @appsink: the appsink element to emit this signal on
   * @timeout: the maximum amount of time to wait for the preroll sample
   *
   * Get the last preroll sample in @appsink. This was the sample that caused the
   * appsink to preroll in the PAUSED state.
   *
   * This function is typically used when dealing with a pipeline in the PAUSED
   * state. Calling this function after doing a seek will give the sample right
   * after the seek position.
   *
   * Calling this function will clear the internal reference to the preroll
   * buffer.
   *
   * Note that the preroll sample will also be returned as the first sample
   * when calling gst_app_sink_pull_sample() or the "pull-sample" action signal.
   *
   * If an EOS event was received before any buffers or the timeout expires,
   * this function returns %NULL. Use gst_app_sink_is_eos () to check for the EOS
   * condition.
   *
   * This function blocks until a preroll sample or EOS is received, the appsink
   * element is set to the READY/NULL state, or the timeout expires.
   *
   * Returns: (nullable) (transfer full): a #GstSample or %NULL when the appsink
   * is stopped or EOS or the timeout expires.
   *
   * Since: 1.10
   */
  gst_app_sink_signals[SIGNAL_TRY_PULL_PREROLL] =
      g_signal_new ("try-pull-preroll", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
      G_STRUCT_OFFSET (GstAppSinkClass, try_pull_preroll), NULL, NULL, NULL,
      GST_TYPE_SAMPLE, 1, GST_TYPE_CLOCK_TIME);
  /**
   * GstAppSink::try-pull-sample:
   * @appsink: the appsink element to emit this signal on
   * @timeout: the maximum amount of time to wait for a sample
   *
   * This function blocks until a sample or EOS becomes available or the appsink
   * element is set to the READY/NULL state or the timeout expires.
   *
   * This function will only return samples when the appsink is in the PLAYING
   * state. All rendered samples will be put in a queue so that the application
   * can pull samples at its own rate.
   *
   * Note that when the application does not pull samples fast enough, the
   * queued samples could consume a lot of memory, especially when dealing with
   * raw video frames. It's possible to control the behaviour of the queue with
   * the "drop" and "max-buffers" / "max-bytes" / "max-time" set of properties.
   *
   * If an EOS event was received before any buffers or the timeout expires,
   * this function returns %NULL. Use gst_app_sink_is_eos () to check
   * for the EOS condition.
   *
   * Returns: (nullable): a #GstSample or NULL when the appsink is stopped or EOS or the timeout expires.
   *
   * Since: 1.10
   */
  gst_app_sink_signals[SIGNAL_TRY_PULL_SAMPLE] =
      g_signal_new ("try-pull-sample", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
      G_STRUCT_OFFSET (GstAppSinkClass, try_pull_sample), NULL, NULL, NULL,
      GST_TYPE_SAMPLE, 1, GST_TYPE_CLOCK_TIME);

  /**
   * GstAppSink::try-pull-object:
   * @appsink: the appsink element to emit this signal on
   * @timeout: the maximum amount of time to wait for a sample
   *
   * This function blocks until a sample or an event becomes available or the appsink
   * element is set to the READY/NULL state or the timeout expires.
   *
   * This function will only return samples when the appsink is in the PLAYING
   * state. All rendered samples and events will be put in a queue so that the application
   * can pull them at its own rate.
   * Events can be pulled when the appsink is in the READY, PAUSED or PLAYING state.
   *
   * Note that when the application does not pull samples fast enough, the
   * queued samples could consume a lot of memory, especially when dealing with
   * raw video frames. It's possible to control the behaviour of the queue with
   * the "drop" and "max-buffers" / "max-bytes" / "max-time" set of properties.
   *
   * This function will only pull serialized events, excluding
   * the EOS event for which this functions returns
   * %NULL. Use gst_app_sink_is_eos() to check for the EOS condition.
   *
   * This signal is a variant of #GstAppSink::try-pull-sample: that can be used
   * to handle incoming events as well as samples.
   *
   * Note that future releases may extend this API to return other object types
   * so make sure that your code is checking for the actual type it is handling.
   *
   * Returns: (nullable) (transfer full): a #GstSample or a #GstEvent or NULL when the appsink is stopped or EOS or the timeout expires.
   *
   * Since: 1.20
   */
  gst_app_sink_signals[SIGNAL_TRY_PULL_OBJECT] =
      g_signal_new ("try-pull-object", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
      G_STRUCT_OFFSET (GstAppSinkClass, try_pull_object), NULL, NULL, NULL,
      GST_TYPE_MINI_OBJECT, 1, GST_TYPE_CLOCK_TIME);

  gst_element_class_set_static_metadata (element_class, "AppSink",
      "Generic/Sink", "Allow the application to get access to raw buffer",
      "David Schleef <ds@schleef.org>, Wim Taymans <wim.taymans@gmail.com>");

  gst_element_class_add_static_pad_template (element_class,
      &gst_app_sink_template);

  basesink_class->unlock = gst_app_sink_unlock_start;
  basesink_class->unlock_stop = gst_app_sink_unlock_stop;
  basesink_class->start = gst_app_sink_start;
  basesink_class->stop = gst_app_sink_stop;
  basesink_class->event = gst_app_sink_event;
  basesink_class->preroll = gst_app_sink_preroll;
  basesink_class->render = gst_app_sink_render;
  basesink_class->render_list = gst_app_sink_render_list;
  basesink_class->get_caps = gst_app_sink_getcaps;
  basesink_class->set_caps = gst_app_sink_setcaps;
  basesink_class->query = gst_app_sink_query;
  basesink_class->propose_allocation = gst_app_sink_propose_allocation;

  klass->pull_preroll = gst_app_sink_pull_preroll;
  klass->pull_sample = gst_app_sink_pull_sample;
  klass->try_pull_preroll = gst_app_sink_try_pull_preroll;
  klass->try_pull_sample = gst_app_sink_try_pull_sample;
  klass->try_pull_object = gst_app_sink_try_pull_object;
}

static void
gst_app_sink_init (GstAppSink * appsink)
{
  GstAppSinkPrivate *priv;

  priv = appsink->priv = gst_app_sink_get_instance_private (appsink);

  g_mutex_init (&priv->mutex);
  g_cond_init (&priv->cond);
  priv->queue = gst_queue_array_new (16);
  priv->sample = gst_sample_new (NULL, NULL, NULL, NULL);

  priv->emit_signals = DEFAULT_PROP_EMIT_SIGNALS;
  priv->max_buffers = DEFAULT_PROP_MAX_BUFFERS;
  priv->max_bytes = DEFAULT_PROP_MAX_BYTES;
  priv->max_time = DEFAULT_PROP_MAX_TIME;
  priv->drop = DEFAULT_PROP_DROP;
  priv->wait_on_eos = DEFAULT_PROP_WAIT_ON_EOS;
  priv->buffer_lists_supported = DEFAULT_PROP_BUFFER_LIST;
  priv->wait_status = NOONE_WAITING;
}

static void
gst_app_sink_dispose (GObject * obj)
{
  GstAppSink *appsink = GST_APP_SINK_CAST (obj);
  GstAppSinkPrivate *priv = appsink->priv;
  GstMiniObject *queue_obj;
  Callbacks *callbacks = NULL;

  GST_OBJECT_LOCK (appsink);
  if (priv->caps) {
    gst_caps_unref (priv->caps);
    priv->caps = NULL;
  }
  GST_OBJECT_UNLOCK (appsink);

  g_mutex_lock (&priv->mutex);
  if (priv->callbacks)
    callbacks = g_steal_pointer (&priv->callbacks);
  while ((queue_obj = gst_queue_array_pop_head (priv->queue)))
    gst_mini_object_unref (queue_obj);
  gst_buffer_replace (&priv->preroll_buffer, NULL);
  gst_caps_replace (&priv->preroll_caps, NULL);
  gst_caps_replace (&priv->last_caps, NULL);
  if (priv->sample) {
    gst_sample_unref (priv->sample);
    priv->sample = NULL;
  }
  g_mutex_unlock (&priv->mutex);

  g_clear_pointer (&callbacks, callbacks_unref);

  G_OBJECT_CLASS (parent_class)->dispose (obj);
}

static void
gst_app_sink_finalize (GObject * obj)
{
  GstAppSink *appsink = GST_APP_SINK_CAST (obj);
  GstAppSinkPrivate *priv = appsink->priv;

  g_mutex_clear (&priv->mutex);
  g_cond_clear (&priv->cond);
  gst_queue_array_free (priv->queue);

  G_OBJECT_CLASS (parent_class)->finalize (obj);
}

static void
gst_app_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstAppSink *appsink = GST_APP_SINK_CAST (object);

  switch (prop_id) {
    case PROP_CAPS:
      gst_app_sink_set_caps (appsink, gst_value_get_caps (value));
      break;
    case PROP_EMIT_SIGNALS:
      gst_app_sink_set_emit_signals (appsink, g_value_get_boolean (value));
      break;
    case PROP_MAX_BUFFERS:
      gst_app_sink_set_max_buffers (appsink, g_value_get_uint (value));
      break;
    case PROP_MAX_TIME:
      gst_app_sink_set_max_time (appsink, g_value_get_uint64 (value));
      break;
    case PROP_MAX_BYTES:
      gst_app_sink_set_max_bytes (appsink, g_value_get_uint64 (value));
      break;
    case PROP_DROP:
      gst_app_sink_set_drop (appsink, g_value_get_boolean (value));
      break;
    case PROP_BUFFER_LIST:
      gst_app_sink_set_buffer_list_support (appsink,
          g_value_get_boolean (value));
      break;
    case PROP_WAIT_ON_EOS:
      gst_app_sink_set_wait_on_eos (appsink, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_app_sink_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstAppSink *appsink = GST_APP_SINK_CAST (object);

  switch (prop_id) {
    case PROP_CAPS:
    {
      GstCaps *caps;

      caps = gst_app_sink_get_caps (appsink);
      gst_value_set_caps (value, caps);
      if (caps)
        gst_caps_unref (caps);
      break;
    }
    case PROP_EOS:
      g_value_set_boolean (value, gst_app_sink_is_eos (appsink));
      break;
    case PROP_EMIT_SIGNALS:
      g_value_set_boolean (value, gst_app_sink_get_emit_signals (appsink));
      break;
    case PROP_MAX_BUFFERS:
      g_value_set_uint (value, gst_app_sink_get_max_buffers (appsink));
      break;
    case PROP_MAX_TIME:
      g_value_set_uint64 (value, gst_app_sink_get_max_time (appsink));
      break;
    case PROP_MAX_BYTES:
      g_value_set_uint64 (value, gst_app_sink_get_max_bytes (appsink));
      break;
    case PROP_DROP:
      g_value_set_boolean (value, gst_app_sink_get_drop (appsink));
      break;
    case PROP_BUFFER_LIST:
      g_value_set_boolean (value,
          gst_app_sink_get_buffer_list_support (appsink));
      break;
    case PROP_WAIT_ON_EOS:
      g_value_set_boolean (value, gst_app_sink_get_wait_on_eos (appsink));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_app_sink_unlock_start (GstBaseSink * bsink)
{
  GstAppSink *appsink = GST_APP_SINK_CAST (bsink);
  GstAppSinkPrivate *priv = appsink->priv;

  g_mutex_lock (&priv->mutex);
  GST_DEBUG_OBJECT (appsink, "unlock start");
  priv->unlock = TRUE;
  g_cond_signal (&priv->cond);
  g_mutex_unlock (&priv->mutex);

  return TRUE;
}

static gboolean
gst_app_sink_unlock_stop (GstBaseSink * bsink)
{
  GstAppSink *appsink = GST_APP_SINK_CAST (bsink);
  GstAppSinkPrivate *priv = appsink->priv;

  g_mutex_lock (&priv->mutex);
  GST_DEBUG_OBJECT (appsink, "unlock stop");
  priv->unlock = FALSE;
  g_cond_signal (&priv->cond);
  g_mutex_unlock (&priv->mutex);

  return TRUE;
}

static void
gst_app_sink_flush_unlocked (GstAppSink * appsink)
{
  GstMiniObject *obj;
  GstAppSinkPrivate *priv = appsink->priv;

  GST_DEBUG_OBJECT (appsink, "flush stop appsink");
  priv->is_eos = FALSE;
  gst_buffer_replace (&priv->preroll_buffer, NULL);
  while ((obj = gst_queue_array_pop_head (priv->queue)))
    gst_mini_object_unref (obj);

  gst_queue_status_info_reset (&priv->queue_status_info);

  gst_caps_replace (&priv->last_caps, NULL);
  g_cond_signal (&priv->cond);
}

static gboolean
gst_app_sink_start (GstBaseSink * psink)
{
  GstAppSink *appsink = GST_APP_SINK_CAST (psink);
  GstAppSinkPrivate *priv = appsink->priv;

  g_mutex_lock (&priv->mutex);
  GST_DEBUG_OBJECT (appsink, "starting");
  priv->wait_status = NOONE_WAITING;
  priv->flushing = FALSE;
  priv->started = TRUE;
  gst_segment_init (&priv->preroll_segment, GST_FORMAT_TIME);
  gst_segment_init (&priv->last_segment, GST_FORMAT_TIME);
  priv->sample = gst_sample_make_writable (priv->sample);
  gst_sample_set_buffer (priv->sample, NULL);
  gst_sample_set_buffer_list (priv->sample, NULL);
  gst_sample_set_caps (priv->sample, NULL);
  gst_sample_set_segment (priv->sample, NULL);
  g_mutex_unlock (&priv->mutex);

  return TRUE;
}

static gboolean
gst_app_sink_stop (GstBaseSink * psink)
{
  GstAppSink *appsink = GST_APP_SINK_CAST (psink);
  GstAppSinkPrivate *priv = appsink->priv;

  g_mutex_lock (&priv->mutex);
  GST_DEBUG_OBJECT (appsink, "stopping");
  priv->flushing = TRUE;
  priv->started = FALSE;
  priv->wait_status = NOONE_WAITING;
  gst_app_sink_flush_unlocked (appsink);
  gst_buffer_replace (&priv->preroll_buffer, NULL);
  gst_caps_replace (&priv->preroll_caps, NULL);
  gst_caps_replace (&priv->last_caps, NULL);
  gst_segment_init (&priv->preroll_segment, GST_FORMAT_UNDEFINED);
  gst_segment_init (&priv->last_segment, GST_FORMAT_UNDEFINED);
  priv->sample = gst_sample_make_writable (priv->sample);
  gst_sample_set_buffer (priv->sample, NULL);
  gst_sample_set_buffer_list (priv->sample, NULL);
  gst_sample_set_caps (priv->sample, NULL);
  gst_sample_set_segment (priv->sample, NULL);
  g_mutex_unlock (&priv->mutex);

  return TRUE;
}

static gboolean
gst_app_sink_setcaps (GstBaseSink * sink, GstCaps * caps)
{
  GstAppSink *appsink = GST_APP_SINK_CAST (sink);
  GstAppSinkPrivate *priv = appsink->priv;

  g_mutex_lock (&priv->mutex);
  GST_DEBUG_OBJECT (appsink, "receiving CAPS");

  gst_queue_array_push_tail (priv->queue, gst_event_new_caps (caps));
  gst_queue_status_info_push_event (&priv->queue_status_info);

  if (!priv->preroll_buffer)
    gst_caps_replace (&priv->preroll_caps, caps);
  g_mutex_unlock (&priv->mutex);

  return TRUE;
}

static gboolean
gst_app_sink_event (GstBaseSink * sink, GstEvent * event)
{
  GstAppSink *appsink = GST_APP_SINK_CAST (sink);
  GstAppSinkPrivate *priv = appsink->priv;

  GST_DEBUG_OBJECT (appsink, "%" GST_PTR_FORMAT, event);

  switch (event->type) {
    case GST_EVENT_SEGMENT:
      g_mutex_lock (&priv->mutex);
      GST_DEBUG_OBJECT (appsink, "receiving SEGMENT");
      if (!priv->preroll_buffer)
        gst_event_copy_segment (event, &priv->preroll_segment);
      g_mutex_unlock (&priv->mutex);
      break;
    case GST_EVENT_EOS:{
      gboolean emit = TRUE;
      Callbacks *callbacks = NULL;

      g_mutex_lock (&priv->mutex);
      GST_DEBUG_OBJECT (appsink, "receiving EOS");
      priv->is_eos = TRUE;
      g_cond_signal (&priv->cond);
      g_mutex_unlock (&priv->mutex);

      g_mutex_lock (&priv->mutex);

      /* Wait until all buffers are consumed, or we're flushing.
       * Otherwise, we might signal EOS before all buffers are
       * consumed, which is a bit confusing for the application
       */
      while (priv->queue_status_info.queued_buffers > 0 && !priv->flushing
          && priv->wait_on_eos) {
        if (priv->unlock) {
          /* we are asked to unlock, call the wait_preroll method */
          g_mutex_unlock (&priv->mutex);
          if (gst_base_sink_wait_preroll (sink) != GST_FLOW_OK) {
            /* Directly go out of here */
            gst_event_unref (event);
            return FALSE;
          }

          /* we are allowed to continue now */
          g_mutex_lock (&priv->mutex);
          continue;
        }

        priv->wait_status |= STREAM_WAITING;
        g_cond_wait (&priv->cond, &priv->mutex);
        priv->wait_status &= ~STREAM_WAITING;
      }
      if (priv->flushing)
        emit = FALSE;

      if (emit && priv->callbacks)
        callbacks = callbacks_ref (priv->callbacks);
      g_mutex_unlock (&priv->mutex);

      if (emit) {
        /* emit EOS now */
        if (callbacks && callbacks->callbacks.eos)
          callbacks->callbacks.eos (appsink, callbacks->user_data);
        else
          g_signal_emit (appsink, gst_app_sink_signals[SIGNAL_EOS], 0);

        g_clear_pointer (&callbacks, callbacks_unref);
      }

      break;
    }
    case GST_EVENT_FLUSH_START:
      /* we don't have to do anything here, the base class will call unlock
       * which will make sure we exit the _render method */
      GST_DEBUG_OBJECT (appsink, "received FLUSH_START");
      break;
    case GST_EVENT_FLUSH_STOP:
      g_mutex_lock (&priv->mutex);
      GST_DEBUG_OBJECT (appsink, "received FLUSH_STOP");
      gst_app_sink_flush_unlocked (appsink);
      g_mutex_unlock (&priv->mutex);
      break;
    default:
      break;
  }

  if (GST_EVENT_TYPE (event) != GST_EVENT_EOS
      && GST_EVENT_IS_SERIALIZED (event)) {
    gboolean emit;
    Callbacks *callbacks = NULL;
    gboolean ret;

    g_mutex_lock (&priv->mutex);

    emit = priv->emit_signals;
    if (priv->callbacks)
      callbacks = callbacks_ref (priv->callbacks);

    gst_queue_array_push_tail (priv->queue, gst_event_ref (event));
    gst_queue_status_info_push_event (&priv->queue_status_info);

    if ((priv->wait_status & APP_WAITING))
      g_cond_signal (&priv->cond);

    g_mutex_unlock (&priv->mutex);

    if (callbacks && callbacks->callbacks.new_event) {
      ret = callbacks->callbacks.new_event (appsink, callbacks->user_data);
    } else {
      ret = FALSE;
      if (emit)
        g_signal_emit (appsink,
            gst_app_sink_signals[SIGNAL_NEW_SERIALIZED_EVENT], 0, &ret);
    }
    g_clear_pointer (&callbacks, callbacks_unref);

    if (ret) {
      gst_event_unref (event);
      return TRUE;
    }
  }

  return GST_BASE_SINK_CLASS (parent_class)->event (sink, event);
}

static GstFlowReturn
gst_app_sink_preroll (GstBaseSink * psink, GstBuffer * buffer)
{
  GstFlowReturn res;
  GstAppSink *appsink = GST_APP_SINK_CAST (psink);
  GstAppSinkPrivate *priv = appsink->priv;
  gboolean emit;
  Callbacks *callbacks = NULL;

  g_mutex_lock (&priv->mutex);
  if (priv->flushing)
    goto flushing;

  GST_DEBUG_OBJECT (appsink, "setting preroll buffer %p", buffer);
  gst_buffer_replace (&priv->preroll_buffer, buffer);

  if ((priv->wait_status & APP_WAITING))
    g_cond_signal (&priv->cond);

  emit = priv->emit_signals;
  if (priv->callbacks)
    callbacks = callbacks_ref (priv->callbacks);
  g_mutex_unlock (&priv->mutex);

  if (callbacks && callbacks->callbacks.new_preroll) {
    res = callbacks->callbacks.new_preroll (appsink, callbacks->user_data);
  } else {
    res = GST_FLOW_OK;
    if (emit)
      g_signal_emit (appsink, gst_app_sink_signals[SIGNAL_NEW_PREROLL], 0,
          &res);
  }

  g_clear_pointer (&callbacks, callbacks_unref);

  return res;

flushing:
  {
    GST_DEBUG_OBJECT (appsink, "we are flushing");
    g_mutex_unlock (&priv->mutex);
    return GST_FLOW_FLUSHING;
  }
}

static GstMiniObject *
dequeue_object (GstAppSink * appsink)
{
  GstAppSinkPrivate *priv = appsink->priv;
  GstMiniObject *obj;

  obj = gst_queue_array_pop_head (priv->queue);

  if (GST_IS_BUFFER (obj) || GST_IS_BUFFER_LIST (obj)) {
    GST_DEBUG_OBJECT (appsink, "dequeued buffer/list %p", obj);
  } else if (GST_IS_EVENT (obj)) {
    GstEvent *event = GST_EVENT_CAST (obj);

    switch (GST_EVENT_TYPE (obj)) {
      case GST_EVENT_CAPS:
      {
        GstCaps *caps;

        gst_event_parse_caps (event, &caps);
        GST_DEBUG_OBJECT (appsink, "activating caps %" GST_PTR_FORMAT, caps);
        gst_caps_replace (&priv->last_caps, caps);
        priv->sample = gst_sample_make_writable (priv->sample);
        gst_sample_set_caps (priv->sample, priv->last_caps);
        break;
      }
      case GST_EVENT_SEGMENT:
        gst_event_copy_segment (event, &priv->last_segment);
        priv->sample = gst_sample_make_writable (priv->sample);
        gst_sample_set_segment (priv->sample, &priv->last_segment);
        GST_DEBUG_OBJECT (appsink, "activated segment %" GST_SEGMENT_FORMAT,
            &priv->last_segment);
        break;
      default:
        break;
    }
  }

  /* We don't have last/current segment differentiation in appsink, so pass last_segment twice */
  gst_queue_status_info_pop (&priv->queue_status_info, obj,
      &priv->last_segment, &priv->last_segment, GST_OBJECT_CAST (appsink));

  return obj;
}

static GstMiniObject *
dequeue_buffer (GstAppSink * appsink)
{
  GstMiniObject *obj;

  do {
    obj = dequeue_object (appsink);

    if (GST_IS_BUFFER (obj) || GST_IS_BUFFER_LIST (obj)) {
      break;
    }

    gst_mini_object_unref (obj);
  } while (TRUE);

  return obj;
}

static GstFlowReturn
gst_app_sink_render_common (GstBaseSink * psink, GstMiniObject * data,
    gboolean is_list)
{
  GstFlowReturn ret;
  GstAppSink *appsink = GST_APP_SINK_CAST (psink);
  GstAppSinkPrivate *priv = appsink->priv;
  gboolean emit;
  Callbacks *callbacks = NULL;

restart:
  g_mutex_lock (&priv->mutex);
  if (priv->flushing)
    goto flushing;

  /* queue holding caps event might have been FLUSHed,
   * but caps state still present in pad caps */
  if (G_UNLIKELY (!priv->last_caps &&
          gst_pad_has_current_caps (GST_BASE_SINK_PAD (psink)))) {
    priv->last_caps = gst_pad_get_current_caps (GST_BASE_SINK_PAD (psink));
    priv->sample = gst_sample_make_writable (priv->sample);
    gst_sample_set_caps (priv->sample, priv->last_caps);
    GST_DEBUG_OBJECT (appsink, "activating pad caps %" GST_PTR_FORMAT,
        priv->last_caps);
  }

  GST_DEBUG_OBJECT (appsink,
      "pushing render buffer/list %p on queue (%" G_GUINT64_FORMAT ")", data,
      priv->queue_status_info.queued_buffers);

  while (gst_queue_status_info_is_full (&priv->queue_status_info,
          priv->max_buffers, priv->max_bytes, priv->max_time)) {
    if (priv->drop) {
      GstMiniObject *old;

      /* we need to drop the oldest buffer/list and try again */
      if ((old = dequeue_buffer (appsink))) {
        GST_DEBUG_OBJECT (appsink, "dropping old buffer/list %p", old);
        gst_mini_object_unref (old);
      }
    } else {
      GST_DEBUG_OBJECT (appsink,
          "waiting for free space: have %" G_GUINT64_FORMAT "  buffers (max %"
          G_GUINT64_FORMAT "), %" G_GUINT64_FORMAT " bytes (max %"
          G_GUINT64_FORMAT "), %" G_GUINT64_FORMAT " time (max %"
          G_GUINT64_FORMAT ")", priv->queue_status_info.queued_buffers,
          priv->max_buffers, priv->queue_status_info.queued_bytes,
          priv->max_bytes, priv->queue_status_info.queued_time, priv->max_time);

      if (priv->unlock) {
        /* we are asked to unlock, call the wait_preroll method */
        g_mutex_unlock (&priv->mutex);
        if ((ret = gst_base_sink_wait_preroll (psink)) != GST_FLOW_OK)
          goto stopping;

        /* we are allowed to continue now */
        goto restart;
      }

      /* wait for a buffer to be removed or flush */
      priv->wait_status |= STREAM_WAITING;
      g_cond_wait (&priv->cond, &priv->mutex);
      priv->wait_status &= ~STREAM_WAITING;

      if (priv->flushing)
        goto flushing;
    }
  }
  /* we need to ref the buffer/list when pushing it in the queue */
  gst_queue_array_push_tail (priv->queue, gst_mini_object_ref (data));
  gst_queue_status_info_push (&priv->queue_status_info, data,
      &priv->last_segment, GST_OBJECT_CAST (appsink));

  if ((priv->wait_status & APP_WAITING))
    g_cond_signal (&priv->cond);

  emit = priv->emit_signals;
  if (priv->callbacks)
    callbacks = callbacks_ref (priv->callbacks);
  g_mutex_unlock (&priv->mutex);

  if (callbacks && callbacks->callbacks.new_sample) {
    ret = callbacks->callbacks.new_sample (appsink, callbacks->user_data);
  } else {
    ret = GST_FLOW_OK;
    if (emit)
      g_signal_emit (appsink, gst_app_sink_signals[SIGNAL_NEW_SAMPLE], 0, &ret);
  }
  g_clear_pointer (&callbacks, callbacks_unref);

  return ret;

flushing:
  {
    GST_DEBUG_OBJECT (appsink, "we are flushing");
    g_mutex_unlock (&priv->mutex);
    return GST_FLOW_FLUSHING;
  }
stopping:
  {
    GST_DEBUG_OBJECT (appsink, "we are stopping");
    return ret;
  }
}

static GstFlowReturn
gst_app_sink_render (GstBaseSink * psink, GstBuffer * buffer)
{
  return gst_app_sink_render_common (psink, GST_MINI_OBJECT_CAST (buffer),
      FALSE);
}

static GstFlowReturn
gst_app_sink_render_list (GstBaseSink * sink, GstBufferList * list)
{
  GstFlowReturn flow;
  GstAppSink *appsink;
  GstBuffer *buffer;
  guint i, len;

  appsink = GST_APP_SINK_CAST (sink);

  if (appsink->priv->buffer_lists_supported)
    return gst_app_sink_render_common (sink, GST_MINI_OBJECT_CAST (list), TRUE);

  /* The application doesn't support buffer lists, extract individual buffers
   * then and push them one-by-one */
  GST_INFO_OBJECT (sink, "chaining each group in list as a merged buffer");

  len = gst_buffer_list_length (list);

  flow = GST_FLOW_OK;
  for (i = 0; i < len; i++) {
    buffer = gst_buffer_list_get (list, i);
    flow = gst_app_sink_render (sink, buffer);
    if (flow != GST_FLOW_OK)
      break;
  }

  return flow;
}

static GstCaps *
gst_app_sink_getcaps (GstBaseSink * psink, GstCaps * filter)
{
  GstCaps *caps;
  GstAppSink *appsink = GST_APP_SINK_CAST (psink);
  GstAppSinkPrivate *priv = appsink->priv;

  GST_OBJECT_LOCK (appsink);
  if ((caps = priv->caps)) {
    if (filter)
      caps = gst_caps_intersect_full (filter, caps, GST_CAPS_INTERSECT_FIRST);
    else
      gst_caps_ref (caps);
  }
  GST_DEBUG_OBJECT (appsink, "got caps %" GST_PTR_FORMAT, caps);
  GST_OBJECT_UNLOCK (appsink);

  return caps;
}

static gboolean
gst_app_sink_query (GstBaseSink * bsink, GstQuery * query)
{
  GstAppSink *appsink = GST_APP_SINK_CAST (bsink);
  GstAppSinkPrivate *priv = appsink->priv;
  gboolean ret;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_DRAIN:
    {
      g_mutex_lock (&priv->mutex);
      GST_DEBUG_OBJECT (appsink, "waiting buffers to be consumed");
      while (priv->queue_status_info.queued_buffers > 0 || priv->preroll_buffer) {
        if (priv->unlock) {
          /* we are asked to unlock, call the wait_preroll method */
          g_mutex_unlock (&priv->mutex);
          if (gst_base_sink_wait_preroll (bsink) != GST_FLOW_OK) {
            /* Directly go out of here */
            return FALSE;
          }

          /* we are allowed to continue now */
          g_mutex_lock (&priv->mutex);
          continue;
        }

        priv->wait_status |= STREAM_WAITING;
        g_cond_wait (&priv->cond, &priv->mutex);
        priv->wait_status &= ~STREAM_WAITING;

        if (priv->flushing)
          break;
      }
      g_mutex_unlock (&priv->mutex);
      ret = GST_BASE_SINK_CLASS (parent_class)->query (bsink, query);
      break;
    }
    case GST_QUERY_SEEKING:{
      GstFormat fmt;

      /* we don't supporting seeking */
      gst_query_parse_seeking (query, &fmt, NULL, NULL, NULL);
      gst_query_set_seeking (query, fmt, FALSE, 0, -1);
      ret = TRUE;
      break;
    }

    default:
      ret = GST_BASE_SINK_CLASS (parent_class)->query (bsink, query);
      break;
  }

  return ret;
}

/* external API */

/**
 * gst_app_sink_set_caps:
 * @appsink: a #GstAppSink
 * @caps: (nullable): caps to set
 *
 * Set the capabilities on the appsink element.  This function takes
 * a copy of the caps structure. After calling this method, the sink will only
 * accept caps that match @caps. If @caps is non-fixed, or incomplete,
 * you must check the caps on the samples to get the actual used caps.
 */
void
gst_app_sink_set_caps (GstAppSink * appsink, const GstCaps * caps)
{
  GstCaps *old;
  GstAppSinkPrivate *priv;

  g_return_if_fail (GST_IS_APP_SINK (appsink));

  priv = appsink->priv;

  GST_OBJECT_LOCK (appsink);
  GST_DEBUG_OBJECT (appsink, "setting caps to %" GST_PTR_FORMAT, caps);
  if ((old = priv->caps) != caps) {
    if (caps)
      priv->caps = gst_caps_copy (caps);
    else
      priv->caps = NULL;
    if (old)
      gst_caps_unref (old);
  }
  GST_OBJECT_UNLOCK (appsink);
}

/**
 * gst_app_sink_get_caps:
 * @appsink: a #GstAppSink
 *
 * Get the configured caps on @appsink.
 *
 * Returns: (nullable) (transfer full): the #GstCaps accepted by the sink. gst_caps_unref() after usage.
 */
GstCaps *
gst_app_sink_get_caps (GstAppSink * appsink)
{
  GstCaps *caps;
  GstAppSinkPrivate *priv;

  g_return_val_if_fail (GST_IS_APP_SINK (appsink), NULL);

  priv = appsink->priv;

  GST_OBJECT_LOCK (appsink);
  if ((caps = priv->caps))
    gst_caps_ref (caps);
  GST_DEBUG_OBJECT (appsink, "getting caps of %" GST_PTR_FORMAT, caps);
  GST_OBJECT_UNLOCK (appsink);

  return caps;
}

/**
 * gst_app_sink_is_eos:
 * @appsink: a #GstAppSink
 *
 * Check if @appsink is EOS, which is when no more samples can be pulled because
 * an EOS event was received.
 *
 * This function also returns %TRUE when the appsink is not in the PAUSED or
 * PLAYING state.
 *
 * Returns: %TRUE if no more samples can be pulled and the appsink is EOS.
 */
gboolean
gst_app_sink_is_eos (GstAppSink * appsink)
{
  gboolean ret;
  GstAppSinkPrivate *priv;

  g_return_val_if_fail (GST_IS_APP_SINK (appsink), FALSE);

  priv = appsink->priv;

  g_mutex_lock (&priv->mutex);
  if (!priv->started)
    goto not_started;

  if (priv->is_eos && priv->queue_status_info.queued_buffers == 0) {
    GST_DEBUG_OBJECT (appsink, "we are EOS and the queue is empty");
    ret = TRUE;
  } else {
    GST_DEBUG_OBJECT (appsink, "we are not yet EOS");
    ret = FALSE;
  }
  g_mutex_unlock (&priv->mutex);

  return ret;

not_started:
  {
    GST_DEBUG_OBJECT (appsink, "we are stopped, return TRUE");
    g_mutex_unlock (&priv->mutex);
    return TRUE;
  }
}

/**
 * gst_app_sink_set_emit_signals:
 * @appsink: a #GstAppSink
 * @emit: the new state
 *
 * Make appsink emit the "new-preroll" and "new-sample" signals. This option is
 * by default disabled because signal emission is expensive and unneeded when
 * the application prefers to operate in pull mode.
 */
void
gst_app_sink_set_emit_signals (GstAppSink * appsink, gboolean emit)
{
  GstAppSinkPrivate *priv;

  g_return_if_fail (GST_IS_APP_SINK (appsink));

  priv = appsink->priv;

  g_mutex_lock (&priv->mutex);
  priv->emit_signals = emit;
  g_mutex_unlock (&priv->mutex);
}

/**
 * gst_app_sink_get_emit_signals:
 * @appsink: a #GstAppSink
 *
 * Check if appsink will emit the "new-preroll" and "new-sample" signals.
 *
 * Returns: %TRUE if @appsink is emitting the "new-preroll" and "new-sample"
 * signals.
 */
gboolean
gst_app_sink_get_emit_signals (GstAppSink * appsink)
{
  gboolean result;
  GstAppSinkPrivate *priv;

  g_return_val_if_fail (GST_IS_APP_SINK (appsink), FALSE);

  priv = appsink->priv;

  g_mutex_lock (&priv->mutex);
  result = priv->emit_signals;
  g_mutex_unlock (&priv->mutex);

  return result;
}

#define GST_APP_SINK_GET_PROPERTY(prop_name)               \
G_STMT_START {                                             \
  GstAppSinkPrivate *priv;                                 \
  guint result;                                            \
                                                           \
  g_return_val_if_fail (GST_IS_APP_SINK (appsink), 0);     \
                                                           \
  priv = appsink->priv;                                    \
                                                           \
  g_mutex_lock (&priv->mutex);                             \
  result = priv->prop_name;                                \
  g_mutex_unlock (&priv->mutex);                           \
                                                           \
  return result;                                           \
} G_STMT_END

#define GST_APP_SINK_SET_PROPERTY(prop_name, value)        \
G_STMT_START {                                             \
  GstAppSinkPrivate *priv;                                 \
                                                           \
  g_return_if_fail (GST_IS_APP_SINK (appsink));            \
                                                           \
  priv = appsink->priv;                                    \
                                                           \
  g_mutex_lock (&priv->mutex);                             \
                                                           \
  if (value != priv->prop_name) {                          \
    priv->prop_name = value;                               \
    /* signal the change */                                \
    g_cond_signal (&priv->cond);                           \
  }                                                        \
                                                           \
  g_mutex_unlock (&priv->mutex);                           \
} G_STMT_END

/**
 * gst_app_sink_set_max_buffers:
 * @appsink: a #GstAppSink
 * @max: the maximum number of buffers to queue
 *
 * Set the maximum amount of buffers that can be queued in @appsink. After this
 * amount of buffers are queued in appsink, any more buffers will block upstream
 * elements until a sample is pulled from @appsink, unless 'drop' is set, in which
 * case new buffers will be discarded.
 */
void
gst_app_sink_set_max_buffers (GstAppSink * appsink, guint max)
{
  GST_APP_SINK_SET_PROPERTY (max_buffers, max);
}

/**
 * gst_app_sink_set_max_time:
 * @appsink: a #GstAppSink
 * @max: the maximum total duration to queue
 *
 * Set the maximum total duration that can be queued in @appsink. After this
 * amount of buffers are queued in appsink, any more buffers will block upstream
 * elements until a sample is pulled from @appsink, unless 'drop' is set, in which
 * case new buffers will be discarded.
 *
 * Since: 1.24
 */
void
gst_app_sink_set_max_time (GstAppSink * appsink, GstClockTime max)
{
  GST_APP_SINK_SET_PROPERTY (max_time, max);
}

/**
 * gst_app_sink_set_max_bytes:
 * @appsink: a #GstAppSink
 * @max: the maximum total size of buffers to queue, in bytes
 *
 * Set the maximum total size that can be queued in @appsink. After this
 * amount of buffers are queued in appsink, any more buffers will block upstream
 * elements until a sample is pulled from @appsink, unless 'drop' is set, in which
 * case new buffers will be discarded.
 *
 * Since: 1.24
 */
void
gst_app_sink_set_max_bytes (GstAppSink * appsink, guint64 max)
{
  GST_APP_SINK_SET_PROPERTY (max_bytes, max);
}

/**
 * gst_app_sink_get_max_buffers:
 * @appsink: a #GstAppSink
 *
 * Get the maximum amount of buffers that can be queued in @appsink.
 *
 * Returns: The maximum amount of buffers that can be queued.
 */
guint
gst_app_sink_get_max_buffers (GstAppSink * appsink)
{
  GST_APP_SINK_GET_PROPERTY (max_buffers);
}

/**
 * gst_app_sink_get_max_time:
 * @appsink: a #GstAppSink
 *
 * Get the maximum total duration that can be queued in @appsink.
 *
 * Returns: The maximum total duration that can be queued.
 *
 * Since: 1.24
 */
GstClockTime
gst_app_sink_get_max_time (GstAppSink * appsink)
{
  GST_APP_SINK_GET_PROPERTY (max_time);
}

/**
 * gst_app_sink_get_max_bytes:
 * @appsink: a #GstAppSink
 *
 * Get the maximum total size, in bytes, that can be queued in @appsink.
 *
 * Returns: The maximum amount of bytes that can be queued
 *
 * Since: 1.24
 */
guint64
gst_app_sink_get_max_bytes (GstAppSink * appsink)
{
  GST_APP_SINK_GET_PROPERTY (max_bytes);
}

#undef GST_APP_SINK_GET_PROPERTY
#undef GST_APP_SINK_SET_PROPERTY

/**
 * gst_app_sink_set_drop:
 * @appsink: a #GstAppSink
 * @drop: the new state
 *
 * Instruct @appsink to drop old buffers when the maximum amount of queued
 * data is reached, that is, when any configured limit is hit (max-buffers, max-time or max-bytes).
 */
void
gst_app_sink_set_drop (GstAppSink * appsink, gboolean drop)
{
  GstAppSinkPrivate *priv;

  g_return_if_fail (GST_IS_APP_SINK (appsink));

  priv = appsink->priv;

  g_mutex_lock (&priv->mutex);
  if (priv->drop != drop) {
    priv->drop = drop;
    /* signal the change */
    g_cond_signal (&priv->cond);
  }
  g_mutex_unlock (&priv->mutex);
}

/**
 * gst_app_sink_get_drop:
 * @appsink: a #GstAppSink
 *
 * Check if @appsink will drop old buffers when the maximum amount of queued
 * data is reached (meaning max buffers, time or bytes limit, whichever is hit first).
 *
 * Returns: %TRUE if @appsink is dropping old buffers when the queue is
 * filled.
 */
gboolean
gst_app_sink_get_drop (GstAppSink * appsink)
{
  gboolean result;
  GstAppSinkPrivate *priv;

  g_return_val_if_fail (GST_IS_APP_SINK (appsink), FALSE);

  priv = appsink->priv;

  g_mutex_lock (&priv->mutex);
  result = priv->drop;
  g_mutex_unlock (&priv->mutex);

  return result;
}

/**
 * gst_app_sink_set_buffer_list_support:
 * @appsink: a #GstAppSink
 * @enable_lists: enable or disable buffer list support
 *
 * Instruct @appsink to enable or disable buffer list support.
 *
 * For backwards-compatibility reasons applications need to opt in
 * to indicate that they will be able to handle buffer lists.
 *
 * Since: 1.12
 */
void
gst_app_sink_set_buffer_list_support (GstAppSink * appsink,
    gboolean enable_lists)
{
  GstAppSinkPrivate *priv;

  g_return_if_fail (GST_IS_APP_SINK (appsink));

  priv = appsink->priv;

  g_mutex_lock (&priv->mutex);
  if (priv->buffer_lists_supported != enable_lists) {
    priv->buffer_lists_supported = enable_lists;
  }
  g_mutex_unlock (&priv->mutex);
}

/**
 * gst_app_sink_get_buffer_list_support:
 * @appsink: a #GstAppSink
 *
 * Check if @appsink supports buffer lists.
 *
 * Returns: %TRUE if @appsink supports buffer lists.
 *
 * Since: 1.12
 */
gboolean
gst_app_sink_get_buffer_list_support (GstAppSink * appsink)
{
  gboolean result;
  GstAppSinkPrivate *priv;

  g_return_val_if_fail (GST_IS_APP_SINK (appsink), FALSE);

  priv = appsink->priv;

  g_mutex_lock (&priv->mutex);
  result = priv->buffer_lists_supported;
  g_mutex_unlock (&priv->mutex);

  return result;
}

/**
 * gst_app_sink_set_wait_on_eos:
 * @appsink: a #GstAppSink
 * @wait: the new state
 *
 * Instruct @appsink to wait for all buffers to be consumed when an EOS is received.
 *
 */
void
gst_app_sink_set_wait_on_eos (GstAppSink * appsink, gboolean wait)
{
  GstAppSinkPrivate *priv;

  g_return_if_fail (GST_IS_APP_SINK (appsink));

  priv = appsink->priv;

  g_mutex_lock (&priv->mutex);
  if (priv->wait_on_eos != wait) {
    priv->wait_on_eos = wait;
    /* signal the change */
    g_cond_signal (&priv->cond);
  }
  g_mutex_unlock (&priv->mutex);
}

/**
 * gst_app_sink_get_wait_on_eos:
 * @appsink: a #GstAppSink
 *
 * Check if @appsink will wait for all buffers to be consumed when an EOS is
 * received.
 *
 * Returns: %TRUE if @appsink will wait for all buffers to be consumed when an
 * EOS is received.
 */
gboolean
gst_app_sink_get_wait_on_eos (GstAppSink * appsink)
{
  gboolean result;
  GstAppSinkPrivate *priv;

  g_return_val_if_fail (GST_IS_APP_SINK (appsink), FALSE);

  priv = appsink->priv;

  g_mutex_lock (&priv->mutex);
  result = priv->wait_on_eos;
  g_mutex_unlock (&priv->mutex);

  return result;
}

/**
 * gst_app_sink_pull_preroll:
 * @appsink: a #GstAppSink
 *
 * Get the last preroll sample in @appsink. This was the sample that caused the
 * appsink to preroll in the PAUSED state.
 *
 * This function is typically used when dealing with a pipeline in the PAUSED
 * state. Calling this function after doing a seek will give the sample right
 * after the seek position.
 *
 * Calling this function will clear the internal reference to the preroll
 * buffer.
 *
 * Note that the preroll sample will also be returned as the first sample
 * when calling gst_app_sink_pull_sample().
 *
 * If an EOS event was received before any buffers, this function returns
 * %NULL. Use gst_app_sink_is_eos () to check for the EOS condition.
 *
 * This function blocks until a preroll sample or EOS is received or the appsink
 * element is set to the READY/NULL state.
 *
 * Returns: (transfer full) (nullable): a #GstSample or NULL when the appsink is stopped or EOS.
 *          Call gst_sample_unref() after usage.
 */
GstSample *
gst_app_sink_pull_preroll (GstAppSink * appsink)
{
  return gst_app_sink_try_pull_preroll (appsink, GST_CLOCK_TIME_NONE);
}

/**
 * gst_app_sink_pull_sample:
 * @appsink: a #GstAppSink
 *
 * This function blocks until a sample or EOS becomes available or the appsink
 * element is set to the READY/NULL state.
 *
 * This function will only return samples when the appsink is in the PLAYING
 * state. All rendered buffers will be put in a queue so that the application
 * can pull samples at its own rate. Note that when the application does not
 * pull samples fast enough, the queued buffers could consume a lot of memory,
 * especially when dealing with raw video frames.
 *
 * If an EOS event was received before any buffers, this function returns
 * %NULL. Use gst_app_sink_is_eos () to check for the EOS condition.
 *
 * Returns: (transfer full) (nullable): a #GstSample or NULL when the appsink is stopped or EOS.
 *          Call gst_sample_unref() after usage.
 */
GstSample *
gst_app_sink_pull_sample (GstAppSink * appsink)
{
  return gst_app_sink_try_pull_sample (appsink, GST_CLOCK_TIME_NONE);
}

/**
 * gst_app_sink_pull_object: (skip)
 * @appsink: a #GstAppSink
 *
 * This function blocks until a sample or an event becomes available or the appsink
 * element is set to the READY/NULL state.
 *
 * This function will only return samples when the appsink is in the PLAYING
 * state. All rendered buffers and events will be put in a queue so that the application
 * can pull them at its own rate. Note that when the application does not
 * pull samples fast enough, the queued buffers could consume a lot of memory,
 * especially when dealing with raw video frames.
 * Events can be pulled when the appsink is in the READY, PAUSED or PLAYING state.
 *
 * This function will only pull serialized events, excluding
 * the EOS event for which this functions returns
 * %NULL. Use gst_app_sink_is_eos() to check for the EOS condition.
 *
 * This method is a variant of gst_app_sink_pull_sample() that can be used
 * to handle incoming events events as well as samples.
 *
 * Note that future releases may extend this API to return other object types
 * so make sure that your code is checking for the actual type it is handling.
 *
 * Returns: (transfer full) (nullable): a #GstSample, or a #GstEvent or NULL when the appsink is stopped or EOS.
 *          Call gst_mini_object_unref() after usage.
 *
 * Since: 1.20
 */
GstMiniObject *
gst_app_sink_pull_object (GstAppSink * appsink)
{
  return gst_app_sink_try_pull_object (appsink, GST_CLOCK_TIME_NONE);
}

/**
 * gst_app_sink_try_pull_preroll:
 * @appsink: a #GstAppSink
 * @timeout: the maximum amount of time to wait for the preroll sample
 *
 * Get the last preroll sample in @appsink. This was the sample that caused the
 * appsink to preroll in the PAUSED state.
 *
 * This function is typically used when dealing with a pipeline in the PAUSED
 * state. Calling this function after doing a seek will give the sample right
 * after the seek position.
 *
 * Calling this function will clear the internal reference to the preroll
 * buffer.
 *
 * Note that the preroll sample will also be returned as the first sample
 * when calling gst_app_sink_pull_sample().
 *
 * If an EOS event was received before any buffers or the timeout expires,
 * this function returns %NULL. Use gst_app_sink_is_eos () to check for the EOS
 * condition.
 *
 * This function blocks until a preroll sample or EOS is received, the appsink
 * element is set to the READY/NULL state, or the timeout expires.
 *
 * Returns: (transfer full) (nullable): a #GstSample or NULL when the appsink is stopped or EOS or the timeout expires.
 *          Call gst_sample_unref() after usage.
 *
 * Since: 1.10
 */
GstSample *
gst_app_sink_try_pull_preroll (GstAppSink * appsink, GstClockTime timeout)
{
  GstAppSinkPrivate *priv;
  GstSample *sample = NULL;
  gboolean timeout_valid;
  gint64 end_time;

  g_return_val_if_fail (GST_IS_APP_SINK (appsink), NULL);

  priv = appsink->priv;

  timeout_valid = GST_CLOCK_TIME_IS_VALID (timeout);

  if (timeout_valid)
    end_time =
        g_get_monotonic_time () + timeout / (GST_SECOND / G_TIME_SPAN_SECOND);

  g_mutex_lock (&priv->mutex);

  while (TRUE) {
    GST_DEBUG_OBJECT (appsink, "trying to grab a buffer");
    if (!priv->started)
      goto not_started;

    if (priv->preroll_buffer != NULL)
      break;

    if (priv->is_eos)
      goto eos;

    /* nothing to return, wait */
    GST_DEBUG_OBJECT (appsink, "waiting for the preroll buffer");
    priv->wait_status |= APP_WAITING;
    if (timeout_valid) {
      if (!g_cond_wait_until (&priv->cond, &priv->mutex, end_time))
        goto expired;
    } else {
      g_cond_wait (&priv->cond, &priv->mutex);
    }
    priv->wait_status &= ~APP_WAITING;
  }
  sample =
      gst_sample_new (priv->preroll_buffer, priv->preroll_caps,
      &priv->preroll_segment, NULL);
  gst_buffer_replace (&priv->preroll_buffer, NULL);
  GST_DEBUG_OBJECT (appsink, "we have the preroll sample %p", sample);
  g_mutex_unlock (&priv->mutex);

  return sample;

  /* special conditions */
expired:
  {
    GST_DEBUG_OBJECT (appsink, "timeout expired, return NULL");
    priv->wait_status &= ~APP_WAITING;
    g_mutex_unlock (&priv->mutex);
    return NULL;
  }
eos:
  {
    GST_DEBUG_OBJECT (appsink, "we are EOS, return NULL");
    g_mutex_unlock (&priv->mutex);
    return NULL;
  }
not_started:
  {
    GST_DEBUG_OBJECT (appsink, "we are stopped, return NULL");
    g_mutex_unlock (&priv->mutex);
    return NULL;
  }
}

/**
 * gst_app_sink_try_pull_sample:
 * @appsink: a #GstAppSink
 * @timeout: the maximum amount of time to wait for a sample
 *
 * This function blocks until a sample or EOS becomes available or the appsink
 * element is set to the READY/NULL state or the timeout expires.
 *
 * This function will only return samples when the appsink is in the PLAYING
 * state. All rendered buffers will be put in a queue so that the application
 * can pull samples at its own rate. Note that when the application does not
 * pull samples fast enough, the queued buffers could consume a lot of memory,
 * especially when dealing with raw video frames.
 *
 * If an EOS event was received before any buffers or the timeout expires,
 * this function returns %NULL. Use gst_app_sink_is_eos () to check for the EOS
 * condition.
 *
 * Returns: (transfer full) (nullable): a #GstSample or NULL when the appsink is stopped or EOS or the timeout expires.
 *          Call gst_sample_unref() after usage.
 *
 * Since: 1.10
 */
GstSample *
gst_app_sink_try_pull_sample (GstAppSink * appsink, GstClockTime timeout)
{
  while (TRUE) {
    GstMiniObject *obj;

    obj = gst_app_sink_try_pull_object (appsink, timeout);

    if (!obj) {
      return NULL;
    } else if (GST_IS_SAMPLE (obj)) {
      return GST_SAMPLE_CAST (obj);
    } else {
      gst_mini_object_unref (obj);
    }
  }
}

/**
 * gst_app_sink_try_pull_object: (skip)
 * @appsink: a #GstAppSink
 * @timeout: the maximum amount of time to wait for a sample
 *
 * This function blocks until a sample or an event or EOS becomes available or the appsink
 * element is set to the READY/NULL state or the timeout expires.
 *
 * This function will only return samples when the appsink is in the PLAYING
 * state. All rendered buffers and events will be put in a queue so that the application
 * can pull them at its own rate. Note that when the application does not
 * pull samples fast enough, the queued buffers could consume a lot of memory,
 * especially when dealing with raw video frames.
 * Events can be pulled when the appsink is in the READY, PAUSED or PLAYING state.
 *
 * This function will only pull serialized events, excluding
 * the EOS event for which this functions returns
 * %NULL. Use gst_app_sink_is_eos() to check for the EOS condition.
 *
 * This method is a variant of gst_app_sink_try_pull_sample() that can be used
 * to handle incoming events events as well as samples.
 *
 * Note that future releases may extend this API to return other object types
 * so make sure that your code is checking for the actual type it is handling.
 *
 * Returns: (transfer full) (nullable): a #GstSample, or #GstEvent or NULL when the appsink is stopped or EOS or the timeout expires.
 * Call gst_mini_object_unref() after usage.
 *
 * Since: 1.20
 */
GstMiniObject *
gst_app_sink_try_pull_object (GstAppSink * appsink, GstClockTime timeout)
{
  GstAppSinkPrivate *priv;
  GstMiniObject *obj = NULL, *ret;
  gboolean timeout_valid;
  gint64 end_time;

  g_return_val_if_fail (GST_IS_APP_SINK (appsink), NULL);

  timeout_valid = GST_CLOCK_TIME_IS_VALID (timeout);

  if (timeout_valid)
    end_time =
        g_get_monotonic_time () + timeout / (GST_SECOND / G_TIME_SPAN_SECOND);

  priv = appsink->priv;

  g_mutex_lock (&priv->mutex);
  gst_buffer_replace (&priv->preroll_buffer, NULL);

  while (TRUE) {
    GST_DEBUG_OBJECT (appsink, "trying to grab an object");
    if (!priv->started)
      goto not_started;

    if (priv->queue_status_info.queued_buffers > 0
        || priv->queue_status_info.num_events > 0)
      break;

    if (priv->is_eos)
      goto eos;

    /* nothing to return, wait */
    GST_DEBUG_OBJECT (appsink, "waiting for an object");
    priv->wait_status |= APP_WAITING;
    if (timeout_valid) {
      if (!g_cond_wait_until (&priv->cond, &priv->mutex, end_time))
        goto expired;
    } else {
      g_cond_wait (&priv->cond, &priv->mutex);
    }
    priv->wait_status &= ~APP_WAITING;
  }

  obj = dequeue_object (appsink);

  /* convert buffer and buffer list to sample */
  if (GST_IS_BUFFER (obj)) {
    GST_DEBUG_OBJECT (appsink, "we have a buffer %p", obj);
    priv->sample = gst_sample_make_writable (priv->sample);
    gst_sample_set_buffer_list (priv->sample, NULL);
    gst_sample_set_buffer (priv->sample, GST_BUFFER_CAST (obj));
    ret = GST_MINI_OBJECT_CAST (gst_sample_ref (priv->sample));
    gst_mini_object_unref (obj);
  } else if (GST_IS_BUFFER_LIST (obj)) {
    GST_DEBUG_OBJECT (appsink, "we have a list %p", obj);
    priv->sample = gst_sample_make_writable (priv->sample);
    gst_sample_set_buffer (priv->sample, NULL);
    gst_sample_set_buffer_list (priv->sample, GST_BUFFER_LIST_CAST (obj));
    ret = GST_MINI_OBJECT_CAST (gst_sample_ref (priv->sample));
    gst_mini_object_unref (obj);
  } else {
    ret = obj;
  }

  if ((priv->wait_status & STREAM_WAITING))
    g_cond_signal (&priv->cond);

  g_mutex_unlock (&priv->mutex);

  return ret;

  /* special conditions */
expired:
  {
    GST_DEBUG_OBJECT (appsink, "timeout expired, return NULL");
    priv->wait_status &= ~APP_WAITING;
    g_mutex_unlock (&priv->mutex);
    return NULL;
  }
eos:
  {
    GST_DEBUG_OBJECT (appsink, "we are EOS, return NULL");
    g_mutex_unlock (&priv->mutex);
    return NULL;
  }
not_started:
  {
    GST_DEBUG_OBJECT (appsink, "we are stopped, return NULL");
    g_mutex_unlock (&priv->mutex);
    return NULL;
  }
}

/**
 * gst_app_sink_set_callbacks: (skip)
 * @appsink: a #GstAppSink
 * @callbacks: the callbacks
 * @user_data: a user_data argument for the callbacks
 * @notify: a destroy notify function
 *
 * Set callbacks which will be executed for each new preroll, new sample and eos.
 * This is an alternative to using the signals, it has lower overhead and is thus
 * less expensive, but also less flexible.
 *
 * If callbacks are installed, no signals will be emitted for performance
 * reasons.
 *
 * Before 1.16.3 it was not possible to change the callbacks in a thread-safe
 * way.
 */
void
gst_app_sink_set_callbacks (GstAppSink * appsink,
    GstAppSinkCallbacks * callbacks, gpointer user_data, GDestroyNotify notify)
{
  Callbacks *old_callbacks, *new_callbacks = NULL;
  GstAppSinkPrivate *priv;

  g_return_if_fail (GST_IS_APP_SINK (appsink));
  g_return_if_fail (callbacks != NULL);

  priv = appsink->priv;

  if (callbacks) {
    new_callbacks = g_new0 (Callbacks, 1);
    new_callbacks->callbacks = *callbacks;
    new_callbacks->user_data = user_data;
    new_callbacks->destroy_notify = notify;
    new_callbacks->ref_count = 1;
  }

  g_mutex_lock (&priv->mutex);
  old_callbacks = g_steal_pointer (&priv->callbacks);
  priv->callbacks = g_steal_pointer (&new_callbacks);
  g_mutex_unlock (&priv->mutex);

  g_clear_pointer (&old_callbacks, callbacks_unref);
}

/*** GSTURIHANDLER INTERFACE *************************************************/

static GstURIType
gst_app_sink_uri_get_type (GType type)
{
  return GST_URI_SINK;
}

static const gchar *const *
gst_app_sink_uri_get_protocols (GType type)
{
  static const gchar *protocols[] = { "appsink", NULL };

  return protocols;
}

static gchar *
gst_app_sink_uri_get_uri (GstURIHandler * handler)
{
  return g_strdup ("appsink");
}

static gboolean
gst_app_sink_uri_set_uri (GstURIHandler * handler, const gchar * uri,
    GError ** error)
{
  /* GstURIHandler checks the protocol for us */
  return TRUE;
}

static void
gst_app_sink_uri_handler_init (gpointer g_iface, gpointer iface_data)
{
  GstURIHandlerInterface *iface = (GstURIHandlerInterface *) g_iface;

  iface->get_type = gst_app_sink_uri_get_type;
  iface->get_protocols = gst_app_sink_uri_get_protocols;
  iface->get_uri = gst_app_sink_uri_get_uri;
  iface->set_uri = gst_app_sink_uri_set_uri;

}

static gboolean
gst_app_sink_propose_allocation (GstBaseSink * bsink, GstQuery * query)
{
  gboolean ret = FALSE;
  GstAppSink *appsink = GST_APP_SINK_CAST (bsink);
  GstAppSinkPrivate *priv = appsink->priv;
  Callbacks *callbacks = NULL;
  gboolean emit;

  g_mutex_lock (&priv->mutex);
  emit = priv->emit_signals;
  if (priv->callbacks)
    callbacks = callbacks_ref (priv->callbacks);
  g_mutex_unlock (&priv->mutex);

  if (callbacks && callbacks->callbacks.propose_allocation) {
    ret =
        callbacks->callbacks.propose_allocation (appsink, query,
        callbacks->user_data);
  } else if (emit) {
    g_signal_emit (appsink, gst_app_sink_signals[SIGNAL_PROPOSE_ALLOCATION], 0,
        query, &ret);
  }

  g_clear_pointer (&callbacks, callbacks_unref);

  return ret;
}
