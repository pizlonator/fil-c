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
 * SECTION:element-cudaipcsink
 * @title: cudaipcsink
 * @short_description: CUDA Inter Process Communication (IPC) sink
 *
 * cudaipcsink exports CUDA memory for connected cudaipcsrc elements to be able
 * to import it
 *
 * ## Example launch line
 * ```
 * gst-launch-1.0 videotestsrc ! cudaupload ! cudaipcsink
 * ```
 *
 * Since: 1.24
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstcudaipcsink.h"
#include "gstcudaformat.h"
#include <gst/cuda/gstcuda-private.h>
#include <mutex>
#include <string>
#include <vector>

#ifdef G_OS_WIN32
#include "gstcudaipcserver_win32.h"
#else
#include "gstcudaipcserver_unix.h"
#endif

GST_DEBUG_CATEGORY_STATIC (cuda_ipc_sink_debug);
#define GST_CAT_DEFAULT cuda_ipc_sink_debug

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE_WITH_FEATURES
        (GST_CAPS_FEATURE_MEMORY_CUDA_MEMORY, GST_CUDA_FORMATS) "; "
        GST_VIDEO_CAPS_MAKE (GST_CUDA_FORMATS)));

enum
{
  PROP_0,
  PROP_DEVICE_ID,
  PROP_ADDRESS,
  PROP_IPC_MODE,
};

#define DEFAULT_DEVICE_ID -1
#ifdef G_OS_WIN32
#define DEFAULT_ADDRESS "\\\\.\\pipe\\gst.cuda.ipc"
#else
#define DEFAULT_ADDRESS "/tmp/gst.cuda.ipc"
#endif
#define DEFAULT_IPC_MODE GST_CUDA_IPC_LEGACY

/* *INDENT-OFF* */
struct GstCudaIpcSinkPrivate
{
  GstCudaIpcSinkPrivate ()
  {
    meta = g_byte_array_new ();
  }

  ~GstCudaIpcSinkPrivate ()
  {
    g_byte_array_unref (meta);
  }

  GstCudaContext *context = nullptr;
  GstCudaStream *stream = nullptr;

  GstBufferPool *fallback_pool = nullptr;
  GstVideoInfo info;

  GstCudaIpcServer *server = nullptr;
  GstCaps *caps = nullptr;
  GstSample *prepared_sample = nullptr;
  GstVideoInfo mem_info;
  CUipcMemHandle prepared_handle;
  GstCudaSharableHandle prepared_os_handle;
  GByteArray *meta;

  std::mutex lock;

  /* properties */
  gint device_id = DEFAULT_DEVICE_ID;
  std::string address = DEFAULT_ADDRESS;
  GstCudaIpcMode ipc_mode = DEFAULT_IPC_MODE;
  GstCudaIpcMode configured_ipc_mode = DEFAULT_IPC_MODE;
};
/* *INDENT-ON* */

struct _GstCudaIpcSink
{
  GstBaseSink parent;

  GstCudaIpcSinkPrivate *priv;
};

static void gst_cuda_ipc_sink_finalize (GObject * object);
static void gst_cuda_ipc_sink_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_win32_video_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static GstClock *gst_cuda_ipc_sink_provide_clock (GstElement * elem);
static void gst_cuda_ipc_sink_set_context (GstElement * elem,
    GstContext * context);

static gboolean gst_cuda_ipc_sink_start (GstBaseSink * sink);
static gboolean gst_cuda_ipc_sink_stop (GstBaseSink * sink);
static gboolean gst_cuda_ipc_sink_set_caps (GstBaseSink * sink, GstCaps * caps);
static void gst_cuda_ipc_sink_get_time (GstBaseSink * sink,
    GstBuffer * buf, GstClockTime * start, GstClockTime * end);
static gboolean gst_cuda_ipc_sink_propose_allocation (GstBaseSink * sink,
    GstQuery * query);
static gboolean gst_cuda_ipc_sink_query (GstBaseSink * sink, GstQuery * query);
static GstFlowReturn gst_cuda_ipc_sink_prepare (GstBaseSink * sink,
    GstBuffer * buf);
static GstFlowReturn gst_cuda_ipc_sink_render (GstBaseSink * sink,
    GstBuffer * buf);

#define gst_cuda_ipc_sink_parent_class parent_class
G_DEFINE_TYPE (GstCudaIpcSink, gst_cuda_ipc_sink, GST_TYPE_BASE_SINK);

static void
gst_cuda_ipc_sink_class_init (GstCudaIpcSinkClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstBaseSinkClass *sink_class = GST_BASE_SINK_CLASS (klass);

  object_class->finalize = gst_cuda_ipc_sink_finalize;
  object_class->set_property = gst_cuda_ipc_sink_set_property;
  object_class->get_property = gst_win32_video_sink_get_property;

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

  g_object_class_install_property (object_class, PROP_IPC_MODE,
      g_param_spec_enum ("ipc-mode", "IPC Mode",
          "IPC mode to use", GST_TYPE_CUDA_IPC_MODE, DEFAULT_IPC_MODE,
          (GParamFlags) (G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
              G_PARAM_STATIC_STRINGS)));

  gst_element_class_set_static_metadata (element_class,
      "CUDA IPC Sink", "Sink/Video",
      "Send CUDA memory to peer cudaipcsrc elements",
      "Seungha Yang <seungha@centricular.com>");
  gst_element_class_add_static_pad_template (element_class, &sink_template);

  element_class->provide_clock =
      GST_DEBUG_FUNCPTR (gst_cuda_ipc_sink_provide_clock);
  element_class->set_context =
      GST_DEBUG_FUNCPTR (gst_cuda_ipc_sink_set_context);

  sink_class->start = GST_DEBUG_FUNCPTR (gst_cuda_ipc_sink_start);
  sink_class->stop = GST_DEBUG_FUNCPTR (gst_cuda_ipc_sink_stop);
  sink_class->set_caps = GST_DEBUG_FUNCPTR (gst_cuda_ipc_sink_set_caps);
  sink_class->propose_allocation =
      GST_DEBUG_FUNCPTR (gst_cuda_ipc_sink_propose_allocation);
  sink_class->query = GST_DEBUG_FUNCPTR (gst_cuda_ipc_sink_query);
  sink_class->get_times = GST_DEBUG_FUNCPTR (gst_cuda_ipc_sink_get_time);
  sink_class->prepare = GST_DEBUG_FUNCPTR (gst_cuda_ipc_sink_prepare);
  sink_class->render = GST_DEBUG_FUNCPTR (gst_cuda_ipc_sink_render);

  GST_DEBUG_CATEGORY_INIT (cuda_ipc_sink_debug, "cudaipcsink",
      0, "cudaipcsink");

  gst_type_mark_as_plugin_api (GST_TYPE_CUDA_IPC_MODE, (GstPluginAPIFlags) 0);
}

static void
gst_cuda_ipc_sink_init (GstCudaIpcSink * self)
{
  self->priv = new GstCudaIpcSinkPrivate ();

  GST_OBJECT_FLAG_SET (self, GST_ELEMENT_FLAG_PROVIDE_CLOCK);
  GST_OBJECT_FLAG_SET (self, GST_ELEMENT_FLAG_REQUIRE_CLOCK);
}

static void
gst_cuda_ipc_sink_finalize (GObject * object)
{
  GstCudaIpcSink *self = GST_CUDA_IPC_SINK (object);

  delete self->priv;

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_cuda_ipc_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstCudaIpcSink *self = GST_CUDA_IPC_SINK (object);
  GstCudaIpcSinkPrivate *priv = self->priv;
  std::lock_guard < std::mutex > lk (priv->lock);

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
    case PROP_IPC_MODE:
      priv->ipc_mode = (GstCudaIpcMode) g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_win32_video_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstCudaIpcSink *self = GST_CUDA_IPC_SINK (object);
  GstCudaIpcSinkPrivate *priv = self->priv;
  std::lock_guard < std::mutex > lk (priv->lock);

  switch (prop_id) {
    case PROP_DEVICE_ID:
      g_value_set_int (value, priv->device_id);
      break;
    case PROP_ADDRESS:
      g_value_set_string (value, priv->address.c_str ());
      break;
    case PROP_IPC_MODE:
      g_value_set_enum (value, priv->ipc_mode);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstClock *
gst_cuda_ipc_sink_provide_clock (GstElement * elem)
{
  return gst_system_clock_obtain ();
}

static void
gst_cuda_ipc_sink_set_context (GstElement * elem, GstContext * context)
{
  GstCudaIpcSink *self = GST_CUDA_IPC_SINK (elem);
  GstCudaIpcSinkPrivate *priv = self->priv;

  gst_cuda_handle_set_context (elem, context, priv->device_id, &priv->context);

  GST_ELEMENT_CLASS (parent_class)->set_context (elem, context);
}

static gboolean
gst_cuda_ipc_sink_start (GstBaseSink * sink)
{
  GstCudaIpcSink *self = GST_CUDA_IPC_SINK (sink);
  GstCudaIpcSinkPrivate *priv = self->priv;
  gboolean virtual_memory = FALSE;
  gboolean os_handle = FALSE;

  GST_DEBUG_OBJECT (self, "Start");

  if (!gst_cuda_ensure_element_context (GST_ELEMENT_CAST (self),
          priv->device_id, &priv->context)) {
    GST_ERROR_OBJECT (self, "Couldn't get CUDA context");
    return FALSE;
  }

  g_object_get (priv->context, "virtual-memory", &virtual_memory,
      "os-handle", &os_handle, nullptr);

  GST_DEBUG_OBJECT (self,
      "virtual-memory: %d, OS-handle: %d, requested IPC mode: %d",
      virtual_memory, os_handle, priv->ipc_mode);

  priv->configured_ipc_mode = priv->ipc_mode;
  if (priv->configured_ipc_mode == GST_CUDA_IPC_MMAP &&
      (!virtual_memory || !os_handle)) {
    GST_ELEMENT_WARNING (self, RESOURCE, SETTINGS, ("Not supported IPC mode"),
        ("MMAP mode IPC is not supported by device"));
    priv->configured_ipc_mode = GST_CUDA_IPC_LEGACY;
  }

  GST_DEBUG_OBJECT (self, "Selected IPC mode: %d", priv->configured_ipc_mode);

  priv->server = gst_cuda_ipc_server_new (priv->address.c_str (),
      priv->context, priv->configured_ipc_mode);
  if (!priv->server) {
    gst_clear_object (&priv->context);
    GST_ERROR_OBJECT (self, "Couldn't create server object");
    return FALSE;
  }

  priv->stream = gst_cuda_stream_new (priv->context);

  return TRUE;
}

static gboolean
gst_cuda_ipc_sink_stop (GstBaseSink * sink)
{
  GstCudaIpcSink *self = GST_CUDA_IPC_SINK (sink);
  GstCudaIpcSinkPrivate *priv = self->priv;

  GST_DEBUG_OBJECT (self, "Stop");

  if (priv->server)
    gst_cuda_ipc_server_stop (priv->server);
  gst_clear_object (&priv->server);

  GST_DEBUG_OBJECT (self, "Server cleared");

  if (priv->fallback_pool) {
    gst_buffer_pool_set_active (priv->fallback_pool, FALSE);
    gst_clear_object (&priv->fallback_pool);
  }

  gst_clear_sample (&priv->prepared_sample);
  gst_clear_cuda_stream (&priv->stream);
  gst_clear_object (&priv->context);

  return TRUE;
}

static void
gst_cuda_ipc_sink_get_time (GstBaseSink * sink, GstBuffer * buf,
    GstClockTime * start, GstClockTime * end)
{
  GstCudaIpcSink *self = GST_CUDA_IPC_SINK (sink);
  GstCudaIpcSinkPrivate *priv = self->priv;
  GstClockTime timestamp;

  timestamp = GST_BUFFER_PTS (buf);
  if (!GST_CLOCK_TIME_IS_VALID (timestamp))
    timestamp = GST_BUFFER_DTS (buf);

  if (GST_CLOCK_TIME_IS_VALID (timestamp)) {
    *start = timestamp;
    if (GST_BUFFER_DURATION_IS_VALID (buf)) {
      *end = timestamp + GST_BUFFER_DURATION (buf);
    } else if (priv->info.fps_n > 0) {
      *end = timestamp +
          gst_util_uint64_scale_int (GST_SECOND, priv->info.fps_d,
          priv->info.fps_n);
    } else if (sink->segment.rate < 0) {
      *end = timestamp;
    }
  }
}

static gboolean
gst_cuda_ipc_sink_set_caps (GstBaseSink * sink, GstCaps * caps)
{
  GstCudaIpcSink *self = GST_CUDA_IPC_SINK (sink);
  GstCudaIpcSinkPrivate *priv = self->priv;
  GstStructure *config;
  GstCaps *new_caps;
  GstStructure *s;
  const gchar *str;

  GST_DEBUG_OBJECT (self, "New caps %" GST_PTR_FORMAT, caps);

  if (!gst_video_info_from_caps (&priv->info, caps)) {
    GST_ERROR_OBJECT (self, "Invalid caps %" GST_PTR_FORMAT, caps);
    return FALSE;
  }

  s = gst_caps_get_structure (caps, 0);

  /* Takes values we know it's always deserializable */
  new_caps = gst_caps_new_empty_simple ("video/x-raw");
  gst_caps_set_simple (new_caps, "format", G_TYPE_STRING,
      gst_video_format_to_string (GST_VIDEO_INFO_FORMAT (&priv->info)),
      "width", G_TYPE_INT, priv->info.width,
      "height", G_TYPE_INT, priv->info.height,
      "framerate", GST_TYPE_FRACTION, priv->info.fps_n, priv->info.fps_d,
      "pixel-aspect-ratio", GST_TYPE_FRACTION, priv->info.par_n,
      priv->info.par_d, nullptr);

  str = gst_structure_get_string (s, "colorimetry");
  if (str)
    gst_caps_set_simple (new_caps, "colorimetry", G_TYPE_STRING, str, nullptr);

  str = gst_structure_get_string (s, "mastering-display-info");
  if (str) {
    gst_caps_set_simple (new_caps, "mastering-display-info", G_TYPE_STRING,
        str, nullptr);
  }

  str = gst_structure_get_string (s, "content-light-level");
  if (str) {
    gst_caps_set_simple (new_caps, "content-light-level", G_TYPE_STRING,
        str, nullptr);
  }

  gst_caps_set_features_simple (new_caps,
      gst_caps_features_new (GST_CAPS_FEATURE_MEMORY_CUDA_MEMORY, nullptr));

  gst_clear_caps (&priv->caps);
  priv->caps = new_caps;

  if (priv->fallback_pool) {
    gst_buffer_pool_set_active (priv->fallback_pool, FALSE);
    gst_object_unref (priv->fallback_pool);
  }

  priv->fallback_pool = gst_cuda_buffer_pool_new (priv->context);
  config = gst_buffer_pool_get_config (priv->fallback_pool);

  gst_buffer_pool_config_add_option (config, GST_BUFFER_POOL_OPTION_VIDEO_META);
  gst_buffer_pool_config_set_params (config, priv->caps,
      GST_VIDEO_INFO_SIZE (&priv->info), 0, 0);
  if (priv->stream)
    gst_buffer_pool_config_set_cuda_stream (config, priv->stream);

  if (priv->configured_ipc_mode == GST_CUDA_IPC_MMAP) {
    gst_buffer_pool_config_set_cuda_alloc_method (config,
        GST_CUDA_MEMORY_ALLOC_MMAP);
  }

  if (!gst_buffer_pool_set_config (priv->fallback_pool, config)) {
    GST_ERROR_OBJECT (self, "Couldn't set pool config");
    gst_clear_object (&priv->fallback_pool);
    return FALSE;
  }

  if (!gst_buffer_pool_set_active (priv->fallback_pool, TRUE)) {
    GST_ERROR_OBJECT (self, "Couldn't active pool");
    gst_clear_object (&priv->fallback_pool);
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_cuda_ipc_sink_propose_allocation (GstBaseSink * sink, GstQuery * query)
{
  GstCudaIpcSink *self = GST_CUDA_IPC_SINK (sink);
  GstCudaIpcSinkPrivate *priv = self->priv;
  GstCaps *caps;
  GstBufferPool *pool = nullptr;
  GstVideoInfo info;
  guint size;
  gboolean need_pool;

  gst_query_parse_allocation (query, &caps, &need_pool);
  if (!caps) {
    GST_WARNING_OBJECT (sink, "No caps specified");
    return FALSE;
  }

  if (!gst_video_info_from_caps (&info, caps)) {
    GST_WARNING_OBJECT (sink, "Invalid caps %" GST_PTR_FORMAT, caps);
    return FALSE;
  }

  /* the normal size of a frame */
  size = info.size;
  if (need_pool) {
    GstStructure *config;

    pool = gst_cuda_buffer_pool_new (priv->context);

    config = gst_buffer_pool_get_config (pool);
    gst_buffer_pool_config_add_option (config,
        GST_BUFFER_POOL_OPTION_VIDEO_META);
    if (priv->stream)
      gst_buffer_pool_config_set_cuda_stream (config, priv->stream);

    if (priv->configured_ipc_mode == GST_CUDA_IPC_MMAP) {
      gst_buffer_pool_config_set_cuda_alloc_method (config,
          GST_CUDA_MEMORY_ALLOC_MMAP);
    }

    size = GST_VIDEO_INFO_SIZE (&info);
    gst_buffer_pool_config_set_params (config, caps, size, 0, 0);

    if (!gst_buffer_pool_set_config (pool, config)) {
      GST_ERROR_OBJECT (pool, "Couldn't set config");
      gst_object_unref (pool);

      return FALSE;
    }
  }

  gst_query_add_allocation_pool (query, pool, size, 0, 0);
  gst_clear_object (&pool);

  gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL);

  return TRUE;
}

static gboolean
gst_cuda_ipc_sink_query (GstBaseSink * sink, GstQuery * query)
{
  GstCudaIpcSink *self = GST_CUDA_IPC_SINK (sink);
  GstCudaIpcSinkPrivate *priv = self->priv;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CONTEXT:
      if (gst_cuda_handle_context_query (GST_ELEMENT (self), query,
              priv->context)) {
        return TRUE;
      }
      break;
    default:
      break;
  }

  return GST_BASE_SINK_CLASS (parent_class)->query (sink, query);
}

static gboolean
gst_cuda_ipc_sink_foreach_meta (GstBuffer * buffer, GstMeta ** meta,
    GstCudaIpcSink * self)
{
  auto priv = self->priv;

  if (!gst_meta_info_is_custom ((*meta)->info))
    return TRUE;

  gst_meta_serialize_simple (*meta, priv->meta);

  return TRUE;
}

static GstFlowReturn
gst_cuda_ipc_sink_prepare (GstBaseSink * sink, GstBuffer * buf)
{
  GstCudaIpcSink *self = GST_CUDA_IPC_SINK (sink);
  GstCudaIpcSinkPrivate *priv = self->priv;
  GstBuffer *cuda_buf;
  GstMemory *mem;
  GstCudaMemory *cmem;
  GstMapInfo info;
  CUresult ret;
  CUdeviceptr ptr;
  std::string handle_dump;

  gst_clear_sample (&priv->prepared_sample);

  cuda_buf = buf;
  mem = gst_buffer_peek_memory (cuda_buf, 0);
  if (!gst_is_cuda_memory (mem) ||
      GST_CUDA_MEMORY_CAST (mem)->context != priv->context) {
    if (gst_buffer_pool_acquire_buffer (priv->fallback_pool, &cuda_buf,
            nullptr) != GST_FLOW_OK) {
      GST_ERROR_OBJECT (self, "Couldn't acquire fallback buffer");
      return GST_FLOW_ERROR;
    }

    if (!gst_cuda_buffer_copy (cuda_buf, GST_CUDA_BUFFER_COPY_CUDA,
            &priv->info, buf, GST_CUDA_BUFFER_COPY_SYSTEM, &priv->info,
            priv->context, priv->stream)) {
      GST_ERROR_OBJECT (self, "Couldn't copy memory");
      goto error;
    }

    mem = gst_buffer_peek_memory (cuda_buf, 0);
  } else {
    GstCudaMemory *cmem = GST_CUDA_MEMORY_CAST (mem);
    GstCudaMemoryAllocMethod alloc_method =
        gst_cuda_memory_get_alloc_method (cmem);

    /* Copy into fallback pool if the memory belongs to fixed size buffer pool
     * (e.g., decoder output) or mem allocation type is different */
    if (gst_cuda_memory_is_from_fixed_pool (mem) ||
        (priv->configured_ipc_mode == GST_CUDA_IPC_MMAP &&
            alloc_method != GST_CUDA_MEMORY_ALLOC_MMAP) ||
        (priv->configured_ipc_mode == GST_CUDA_IPC_LEGACY &&
            alloc_method != GST_CUDA_MEMORY_ALLOC_MALLOC)) {
      if (gst_buffer_pool_acquire_buffer (priv->fallback_pool, &cuda_buf,
              nullptr) != GST_FLOW_OK) {
        GST_ERROR_OBJECT (self, "Couldn't acquire fallback buffer");
        return GST_FLOW_ERROR;
      }

      if (!gst_cuda_buffer_copy (cuda_buf, GST_CUDA_BUFFER_COPY_CUDA,
              &priv->info, buf, GST_CUDA_BUFFER_COPY_CUDA, &priv->info,
              priv->context, priv->stream)) {
        GST_ERROR_OBJECT (self, "Couldn't copy memory");
        goto error;
      }

      mem = gst_buffer_peek_memory (cuda_buf, 0);
    }
  }

  cmem = GST_CUDA_MEMORY_CAST (mem);
  priv->mem_info = cmem->info;
  if (!gst_memory_map (mem, &info, (GstMapFlags) (GST_MAP_READ | GST_MAP_CUDA))) {
    GST_ERROR_OBJECT (self, "Couldn't map memory");
    goto error;
  }

  ptr = (CUdeviceptr) info.data;
  gst_memory_unmap (mem, &info);
  gst_cuda_memory_sync (cmem);

  if (priv->configured_ipc_mode == GST_CUDA_IPC_MMAP) {
    if (!gst_cuda_memory_export (cmem, (void *) &priv->prepared_os_handle)) {
      GST_ERROR_OBJECT (self, "Couldn't export memory");
      goto error;
    }
  } else {
    if (!gst_cuda_context_push (cmem->context)) {
      GST_ERROR_OBJECT (self, "Couldn't push context");
      goto error;
    }

    ret = CuIpcGetMemHandle (&priv->prepared_handle, ptr);
    gst_cuda_context_pop (nullptr);

    if (!gst_cuda_result (ret)) {
      GST_ERROR_OBJECT (self, "Couldn't get IPC handle");
      goto error;
    }

    handle_dump = gst_cuda_ipc_mem_handle_to_string (priv->prepared_handle);
    GST_TRACE_OBJECT (self, "Exported handle value for %" G_GUINTPTR_FORMAT
        " %s", ptr, handle_dump.c_str ());
  }

  priv->prepared_sample = gst_sample_new (cuda_buf,
      priv->caps, nullptr, nullptr);

  g_byte_array_set_size (priv->meta, 0);
  gst_buffer_foreach_meta (buf,
      (GstBufferForeachMetaFunc) gst_cuda_ipc_sink_foreach_meta, self);

  if (cuda_buf != buf)
    gst_buffer_unref (cuda_buf);

  return GST_FLOW_OK;

error:
  if (cuda_buf != buf)
    gst_buffer_unref (cuda_buf);

  return GST_FLOW_ERROR;
}

static GstFlowReturn
gst_cuda_ipc_sink_render (GstBaseSink * sink, GstBuffer * buf)
{
  GstCudaIpcSink *self = GST_CUDA_IPC_SINK (sink);
  GstCudaIpcSinkPrivate *priv = self->priv;
  GstClockTime pts;
  GstClockTime now_system;
  GstClockTime buf_pts;
  GstClockTime buffer_clock = GST_CLOCK_TIME_NONE;
  GstFlowReturn ret;

  if (!priv->prepared_sample) {
    GST_ERROR_OBJECT (self, "Have no prepared sample");
    return GST_FLOW_ERROR;
  }

  pts = now_system = gst_util_get_timestamp ();
  buf_pts = GST_BUFFER_PTS (buf);
  if (!GST_CLOCK_TIME_IS_VALID (buf_pts))
    buf_pts = GST_BUFFER_DTS (buf);

  if (GST_CLOCK_TIME_IS_VALID (buf_pts)) {
    buffer_clock = gst_segment_to_running_time (&sink->segment,
        GST_FORMAT_TIME, buf_pts) +
        GST_ELEMENT_CAST (sink)->base_time + gst_base_sink_get_latency (sink);
  }

  if (GST_CLOCK_TIME_IS_VALID (buffer_clock)) {
    GstClock *clock = gst_element_get_clock (GST_ELEMENT_CAST (sink));
    if (!gst_cuda_ipc_clock_is_system (clock)) {
      GstClockTime now_gst = gst_clock_get_time (clock);
      GstClockTimeDiff converted = buffer_clock;

      converted -= now_gst;
      converted += now_system;

      if (converted < 0) {
        /* Shouldn't happen */
        GST_WARNING_OBJECT (self, "Negative buffer clock");
        pts = 0;
      } else {
        pts = converted;
      }
    } else {
      /* buffer clock is already system time */
      pts = buffer_clock;
    }
    gst_object_unref (clock);
  }

  if (priv->ipc_mode == GST_CUDA_IPC_LEGACY) {
    ret = gst_cuda_ipc_server_send_data (priv->server, priv->prepared_sample,
        priv->mem_info, priv->prepared_handle, pts, priv->meta);
  } else {
    ret = gst_cuda_ipc_server_send_mmap_data (priv->server,
        priv->prepared_sample, priv->mem_info, priv->prepared_os_handle, pts,
        priv->meta);
  }

  return ret;
}
