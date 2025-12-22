/* GStreamer
 * Copyright (C) 2023 Seungha Yang <seungha@centricular.com>
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
 * SECTION:element-cudaipcsrc
 * @title: cudaipcsrc
 * @short_description: CUDA Inter Process Communication (IPC) src
 *
 * cudaipcsrc imports CUDA memory exported by peer cudaipcsrc element
 *
 * ## Example launch line
 * ```
 * gst-launch-1.0 cudaipcsrc ! queue ! cudadownload ! videoconvert ! queue ! autovideosink
 * ```
 *
 * Since: 1.24
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstcudaipcsrc.h"
#include "gstcudaformat.h"
#include <mutex>
#include <string>

#ifdef G_OS_WIN32
#include "gstcudaipcclient_win32.h"
#else
#include "gstcudaipcclient_unix.h"
#endif

GST_DEBUG_CATEGORY_STATIC (gst_cuda_ipc_src_debug);
#define GST_CAT_DEFAULT gst_cuda_ipc_src_debug

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE_WITH_FEATURES
        (GST_CAPS_FEATURE_MEMORY_CUDA_MEMORY,
            GST_CUDA_FORMATS)));

enum
{
  PROP_0,
  PROP_DEVICE_ID,
  PROP_ADDRESS,
  PROP_PROCESSING_DEADLINE,
  PROP_IO_MODE,
  PROP_CONN_TIMEOUT,
  PROP_BUFFER_SIZE,
};

#define DEFAULT_DEVICE_ID -1
#ifdef G_OS_WIN32
#define DEFAULT_ADDRESS "\\\\.\\pipe\\gst.cuda.ipc"
#else
#define DEFAULT_ADDRESS "/tmp/gst.cuda.ipc"
#endif
#define DEFAULT_PROCESSING_DEADLINE (20 * GST_MSECOND)
#define DEFAULT_IO_MODE GST_CUDA_IPC_IO_COPY
#define DEFAULT_CONN_TIMEOUT 5
#define DEFAULT_BUFFER_SIZE 3

/* *INDENT-OFF* */
struct GstCudaIpcSrcPrivate
{
  GstCudaContext *context = nullptr;
  GstCudaStream *stream = nullptr;

  GstCudaIpcClient *client = nullptr;
  GstCaps *caps = nullptr;

  GstVideoInfo info;
  std::mutex lock;
  bool flushing = false;

  /* properties */
  gint device_id = DEFAULT_DEVICE_ID;
  std::string address = DEFAULT_ADDRESS;
  GstClockTime processing_deadline = DEFAULT_PROCESSING_DEADLINE;
  GstCudaIpcIOMode io_mode = DEFAULT_IO_MODE;
  guint conn_timeout = DEFAULT_CONN_TIMEOUT;
  guint buffer_size = DEFAULT_BUFFER_SIZE;
};
/* *INDENT-ON* */

struct _GstCudaIpcSrc
{
  GstBaseSrc parent;

  GstCudaIpcSrcPrivate *priv;
};

static void gst_cuda_ipc_src_finalize (GObject * object);
static void gst_cuda_ipc_src_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_win32_video_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static GstClock *gst_cuda_ipc_src_provide_clock (GstElement * elem);
static void gst_cuda_ipc_src_set_context (GstElement * elem,
    GstContext * context);

static gboolean gst_cuda_ipc_src_start (GstBaseSrc * src);
static gboolean gst_cuda_ipc_src_stop (GstBaseSrc * src);
static gboolean gst_cuda_ipc_src_unlock (GstBaseSrc * src);
static gboolean gst_cuda_ipc_src_unlock_stop (GstBaseSrc * src);
static gboolean gst_cuda_ipc_src_query (GstBaseSrc * src, GstQuery * query);
static GstCaps *gst_cuda_ipc_src_get_caps (GstBaseSrc * src, GstCaps * filter);
static GstCaps *gst_cuda_ipc_src_fixate (GstBaseSrc * src, GstCaps * caps);
static GstFlowReturn gst_cuda_ipc_src_create (GstBaseSrc * src, guint64 offset,
    guint size, GstBuffer ** buf);

#define gst_cuda_ipc_src_parent_class parent_class
G_DEFINE_TYPE (GstCudaIpcSrc, gst_cuda_ipc_src, GST_TYPE_BASE_SRC);

static void
gst_cuda_ipc_src_class_init (GstCudaIpcSrcClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstBaseSrcClass *src_class = GST_BASE_SRC_CLASS (klass);

  object_class->finalize = gst_cuda_ipc_src_finalize;
  object_class->set_property = gst_cuda_ipc_src_set_property;
  object_class->get_property = gst_win32_video_src_get_property;

  g_object_class_install_property (object_class, PROP_DEVICE_ID,
      g_param_spec_int ("cuda-device-id", "CUDA Device ID",
          "CUDA device id to use (-1 = auto)", -1, G_MAXINT, DEFAULT_DEVICE_ID,
          (GParamFlags) (G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
              G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (object_class, PROP_ADDRESS,
      g_param_spec_string ("address", "Address",
          "Server address. Specifies name of WIN32 named pipe "
          "or unix domain socket path on Linux",
          DEFAULT_ADDRESS, (GParamFlags) (G_PARAM_READWRITE |
              G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_READY)));

  g_object_class_install_property (object_class, PROP_PROCESSING_DEADLINE,
      g_param_spec_uint64 ("processing-deadline", "Processing deadline",
          "Maximum processing time for a buffer in nanoseconds", 0, G_MAXUINT64,
          DEFAULT_PROCESSING_DEADLINE, (GParamFlags) (G_PARAM_READWRITE |
              G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING)));

  g_object_class_install_property (object_class, PROP_IO_MODE,
      g_param_spec_enum ("io-mode", "IO Mode",
          "Memory I/O mode to use. This option will be ignored if the selected "
          "IPC mode is mmap",
          GST_TYPE_CUDA_IPC_IO_MODE, DEFAULT_IO_MODE,
          (GParamFlags) (G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
              G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (object_class, PROP_CONN_TIMEOUT,
      g_param_spec_uint ("connection-timeout", "Connection Timeout",
          "Connection timeout in seconds (0 = never timeout)", 0, G_MAXINT,
          DEFAULT_CONN_TIMEOUT,
          (GParamFlags) (G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
              G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (object_class, PROP_BUFFER_SIZE,
      g_param_spec_uint ("buffer-size", "Buffer Size",
          "Size of internal buffer", 1, G_MAXINT,
          DEFAULT_BUFFER_SIZE,
          (GParamFlags) (G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
              G_PARAM_STATIC_STRINGS)));

  gst_element_class_set_static_metadata (element_class,
      "CUDA IPC Src", "Source/Video",
      "Receive CUDA memory from the cudaipcsrc element",
      "Seungha Yang <seungha@centricular.com>");
  gst_element_class_add_static_pad_template (element_class, &src_template);

  element_class->provide_clock =
      GST_DEBUG_FUNCPTR (gst_cuda_ipc_src_provide_clock);
  element_class->set_context = GST_DEBUG_FUNCPTR (gst_cuda_ipc_src_set_context);

  src_class->start = GST_DEBUG_FUNCPTR (gst_cuda_ipc_src_start);
  src_class->stop = GST_DEBUG_FUNCPTR (gst_cuda_ipc_src_stop);
  src_class->unlock = GST_DEBUG_FUNCPTR (gst_cuda_ipc_src_unlock);
  src_class->unlock_stop = GST_DEBUG_FUNCPTR (gst_cuda_ipc_src_unlock_stop);
  src_class->query = GST_DEBUG_FUNCPTR (gst_cuda_ipc_src_query);
  src_class->get_caps = GST_DEBUG_FUNCPTR (gst_cuda_ipc_src_get_caps);
  src_class->fixate = GST_DEBUG_FUNCPTR (gst_cuda_ipc_src_fixate);
  src_class->create = GST_DEBUG_FUNCPTR (gst_cuda_ipc_src_create);

  GST_DEBUG_CATEGORY_INIT (gst_cuda_ipc_src_debug, "cudaipcsrc",
      0, "cudaipcsrc");

  gst_type_mark_as_plugin_api (GST_TYPE_CUDA_IPC_IO_MODE,
      (GstPluginAPIFlags) 0);
}

static void
gst_cuda_ipc_src_init (GstCudaIpcSrc * self)
{
  gst_base_src_set_format (GST_BASE_SRC (self), GST_FORMAT_TIME);
  gst_base_src_set_live (GST_BASE_SRC (self), TRUE);

  self->priv = new GstCudaIpcSrcPrivate ();

  GST_OBJECT_FLAG_SET (self, GST_ELEMENT_FLAG_PROVIDE_CLOCK);
  GST_OBJECT_FLAG_SET (self, GST_ELEMENT_FLAG_REQUIRE_CLOCK);
}

static void
gst_cuda_ipc_src_finalize (GObject * object)
{
  GstCudaIpcSrc *self = GST_CUDA_IPC_SRC (object);

  delete self->priv;

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_cuda_ipc_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstCudaIpcSrc *self = GST_CUDA_IPC_SRC (object);
  GstCudaIpcSrcPrivate *priv = self->priv;
  std::unique_lock < std::mutex > lk (priv->lock);

  switch (prop_id) {
    case PROP_DEVICE_ID:
      priv->device_id = g_value_get_int (value);
      break;
    case PROP_ADDRESS:
    {
      const gchar *address = g_value_get_string (value);
      priv->address.clear ();

      if (address)
        priv->address = address;
      break;
    }
    case PROP_PROCESSING_DEADLINE:
    {
      GstClockTime prev_val, new_val;
      prev_val = priv->processing_deadline;
      new_val = g_value_get_uint64 (value);
      priv->processing_deadline = new_val;

      if (prev_val != new_val) {
        lk.unlock ();
        GST_DEBUG_OBJECT (self, "Posting latency message");
        gst_element_post_message (GST_ELEMENT_CAST (self),
            gst_message_new_latency (GST_OBJECT_CAST (self)));
      }
      break;
    }
    case PROP_IO_MODE:
      priv->io_mode = (GstCudaIpcIOMode) g_value_get_enum (value);
      break;
    case PROP_CONN_TIMEOUT:
      priv->conn_timeout = g_value_get_uint (value);
      break;
    case PROP_BUFFER_SIZE:
      priv->buffer_size = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_win32_video_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstCudaIpcSrc *self = GST_CUDA_IPC_SRC (object);
  GstCudaIpcSrcPrivate *priv = self->priv;
  std::lock_guard < std::mutex > lk (priv->lock);

  switch (prop_id) {
    case PROP_DEVICE_ID:
      g_value_set_int (value, priv->device_id);
      break;
    case PROP_ADDRESS:
      g_value_set_string (value, priv->address.c_str ());
      break;
    case PROP_PROCESSING_DEADLINE:
      g_value_set_uint64 (value, priv->processing_deadline);
      break;
    case PROP_IO_MODE:
      g_value_set_enum (value, priv->io_mode);
      break;
    case PROP_CONN_TIMEOUT:
      g_value_set_uint (value, priv->conn_timeout);
      break;
    case PROP_BUFFER_SIZE:
      g_value_set_uint (value, priv->buffer_size);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstClock *
gst_cuda_ipc_src_provide_clock (GstElement * elem)
{
  return gst_system_clock_obtain ();
}

static void
gst_cuda_ipc_src_set_context (GstElement * elem, GstContext * context)
{
  GstCudaIpcSrc *self = GST_CUDA_IPC_SRC (elem);
  GstCudaIpcSrcPrivate *priv = self->priv;

  gst_cuda_handle_set_context (elem, context, priv->device_id, &priv->context);

  GST_ELEMENT_CLASS (parent_class)->set_context (elem, context);
}

static gboolean
gst_cuda_ipc_src_start (GstBaseSrc * src)
{
  GstCudaIpcSrc *self = GST_CUDA_IPC_SRC (src);
  GstCudaIpcSrcPrivate *priv = self->priv;

  GST_DEBUG_OBJECT (self, "Start");

  if (!gst_cuda_ensure_element_context (GST_ELEMENT_CAST (self),
          priv->device_id, &priv->context)) {
    GST_ERROR_OBJECT (self, "Couldn't get CUDA context");
    return FALSE;
  }

  priv->stream = gst_cuda_stream_new (priv->context);

  std::lock_guard < std::mutex > lk (priv->lock);
  priv->client = gst_cuda_ipc_client_new (priv->address.c_str (), priv->context,
      priv->stream, priv->io_mode, priv->conn_timeout, priv->buffer_size - 1);

  return TRUE;
}

static gboolean
gst_cuda_ipc_src_stop (GstBaseSrc * src)
{
  GstCudaIpcSrc *self = GST_CUDA_IPC_SRC (src);
  GstCudaIpcSrcPrivate *priv = self->priv;

  std::lock_guard < std::mutex > lk (priv->lock);

  GST_DEBUG_OBJECT (self, "Stop");

  if (priv->client)
    gst_cuda_ipc_client_stop (priv->client);

  gst_clear_object (&priv->client);
  gst_clear_cuda_stream (&priv->stream);
  gst_clear_object (&priv->context);
  gst_clear_caps (&priv->caps);

  return TRUE;
}

static gboolean
gst_cuda_ipc_src_unlock (GstBaseSrc * src)
{
  GstCudaIpcSrc *self = GST_CUDA_IPC_SRC (src);
  GstCudaIpcSrcPrivate *priv = self->priv;

  GST_DEBUG_OBJECT (self, "Unlock");

  std::lock_guard < std::mutex > lk (priv->lock);
  priv->flushing = true;
  if (priv->client)
    gst_cuda_ipc_client_set_flushing (priv->client, true);

  GST_DEBUG_OBJECT (self, "Unlocked");

  return TRUE;
}

static gboolean
gst_cuda_ipc_src_unlock_stop (GstBaseSrc * src)
{
  GstCudaIpcSrc *self = GST_CUDA_IPC_SRC (src);
  GstCudaIpcSrcPrivate *priv = self->priv;

  GST_DEBUG_OBJECT (self, "Unlock stop");

  std::lock_guard < std::mutex > lk (priv->lock);
  priv->flushing = false;
  if (priv->client)
    gst_cuda_ipc_client_set_flushing (priv->client, false);

  GST_DEBUG_OBJECT (self, "Unlock stopped");

  return TRUE;
}

static gboolean
gst_cuda_ipc_src_query (GstBaseSrc * src, GstQuery * query)
{
  GstCudaIpcSrc *self = GST_CUDA_IPC_SRC (src);
  GstCudaIpcSrcPrivate *priv = self->priv;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_LATENCY:
    {
      std::lock_guard < std::mutex > lk (priv->lock);
      if (GST_CLOCK_TIME_IS_VALID (priv->processing_deadline)) {
        gst_query_set_latency (query, TRUE, priv->processing_deadline,
            GST_CLOCK_TIME_NONE);
      } else {
        gst_query_set_latency (query, TRUE, 0, 0);
      }
      return TRUE;
    }
    case GST_QUERY_CONTEXT:
      if (gst_cuda_handle_context_query (GST_ELEMENT (self), query,
              priv->context)) {
        return TRUE;
      }
      break;
    default:
      break;
  }

  return GST_BASE_SRC_CLASS (parent_class)->query (src, query);
}

static GstCaps *
gst_cuda_ipc_src_get_caps (GstBaseSrc * src, GstCaps * filter)
{
  GstCudaIpcSrc *self = GST_CUDA_IPC_SRC (src);
  GstCudaIpcSrcPrivate *priv = self->priv;
  GstCudaIpcClient *client = nullptr;
  GstCaps *caps = nullptr;

  GST_DEBUG_OBJECT (self, "Get caps");

  priv->lock.lock ();
  if (priv->caps)
    caps = gst_caps_ref (priv->caps);
  else if (priv->client)
    client = (GstCudaIpcClient *) gst_object_ref (priv->client);
  priv->lock.unlock ();

  if (!caps && client)
    caps = gst_cuda_ipc_client_get_caps (priv->client);

  if (!caps)
    caps = gst_pad_get_pad_template_caps (GST_BASE_SRC_PAD (src));

  if (filter) {
    GstCaps *tmp = gst_caps_intersect_full (filter,
        caps, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (caps);
    caps = tmp;
  }

  gst_clear_object (&client);
  GST_DEBUG_OBJECT (self, "Returning caps %" GST_PTR_FORMAT, caps);

  return caps;
}

static GstCaps *
gst_cuda_ipc_src_fixate (GstBaseSrc * src, GstCaps * caps)
{
  /* We don't negotiate with server. In here, we do fixate resolution to
   * 320 x 240 (same as default of videotestsrc) which makes a little more
   * sense than 1x1 */
  caps = gst_caps_make_writable (caps);

  for (guint i = 0; i < gst_caps_get_size (caps); i++) {
    GstStructure *s = gst_caps_get_structure (caps, i);

    gst_structure_fixate_field_nearest_int (s, "width", 320);
    gst_structure_fixate_field_nearest_int (s, "height", 240);
  }

  return gst_caps_fixate (caps);
}

static GstFlowReturn
gst_cuda_ipc_src_create (GstBaseSrc * src, guint64 offset, guint size,
    GstBuffer ** buf)
{
  GstCudaIpcSrc *self = GST_CUDA_IPC_SRC (src);
  GstCudaIpcSrcPrivate *priv = self->priv;
  GstFlowReturn ret;
  GstSample *sample = nullptr;
  GstCaps *caps;
  GstClock *clock;
  bool is_system_clock = true;
  GstClockTime pts;
  GstClockTime base_time;
  GstClockTime now_system;
  GstClockTime now_gst;
  GstClockTime remote_pts;
  GstBuffer *buffer;

  ret = gst_cuda_ipc_client_run (priv->client);
  if (ret != GST_FLOW_OK)
    return ret;

  ret = gst_cuda_ipc_client_get_sample (priv->client, &sample);
  if (ret != GST_FLOW_OK)
    return ret;

  now_system = gst_util_get_timestamp ();
  clock = gst_element_get_clock (GST_ELEMENT_CAST (self));
  now_gst = gst_clock_get_time (clock);
  base_time = GST_ELEMENT_CAST (self)->base_time;
  is_system_clock = gst_cuda_ipc_clock_is_system (clock);
  gst_object_unref (clock);

  buffer = gst_sample_get_buffer (sample);
  remote_pts = GST_BUFFER_PTS (buffer);

  if (!is_system_clock) {
    GstClockTimeDiff now_pts = now_gst - base_time + remote_pts - now_system;

    if (now_pts >= 0)
      pts = now_pts;
    else
      pts = 0;
  } else {
    if (remote_pts >= base_time) {
      pts = remote_pts - base_time;
    } else {
      GST_WARNING_OBJECT (self,
          "Remote clock is smaller than our base time, remote %"
          GST_TIME_FORMAT ", base_time %" GST_TIME_FORMAT,
          GST_TIME_ARGS (remote_pts), GST_TIME_ARGS (base_time));
      pts = 0;
    }
  }

  GST_BUFFER_PTS (buffer) = pts;

  std::unique_lock < std::mutex > lk (priv->lock);
  caps = gst_sample_get_caps (sample);
  if (!priv->caps || !gst_caps_is_equal (priv->caps, caps)) {
    gst_caps_replace (&priv->caps, caps);
    lk.unlock ();
    gst_base_src_set_caps (src, priv->caps);
  }

  *buf = gst_buffer_ref (buffer);
  gst_sample_unref (sample);

  return GST_FLOW_OK;
}
