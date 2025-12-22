/* GStreamer
 * Copyright (C) 2022 Seungha Yang <seungha@centricular.com>
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
 * SECTION:element-win32ipcvideosrc
 * @title: win32ipcvideosrc
 * @short_description: Windows shared memory video source
 *
 * win32ipcvideosrc receives raw video frames from win32ipcvideosink
 * and outputs the received video frames
 *
 * ## Example launch line
 * ```
 * gst-launch-1.0 win32ipcvideosrc ! queue ! videoconvert ! d3d11videosink
 * ```
 *
 * Since: 1.22
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstwin32ipcvideosrc.h"
#include "gstwin32ipcutils.h"
#include "protocol/win32ipcpipeclient.h"
#include <string>

GST_DEBUG_CATEGORY_STATIC (gst_win32_ipc_video_src_debug);
#define GST_CAT_DEFAULT gst_win32_ipc_video_src_debug

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE (GST_VIDEO_FORMATS_ALL)));

enum
{
  PROP_0,
  PROP_PIPE_NAME,
  PROP_PROCESSING_DEADLINE,
};

#define DEFAULT_PIPE_NAME "\\\\.\\pipe\\gst.win32.ipc.video"
#define DEFAULT_PROCESSING_DEADLINE (20 * GST_MSECOND)

struct _GstWin32IpcVideoSrc
{
  GstBaseSrc parent;

  GstVideoInfo info;

  Win32IpcPipeClient *pipe;
  GstCaps *caps;
  gboolean flushing;
  SRWLOCK lock;
  gboolean have_video_meta;
  gsize offset[GST_VIDEO_MAX_PLANES];
  gint stride[GST_VIDEO_MAX_PLANES];
  GstBufferPool *pool;

  /* properties */
  gchar *pipe_name;
  GstClockTime processing_deadline;
};

static void gst_win32_ipc_video_src_finalize (GObject * object);
static void gst_win32_ipc_video_src_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_win32_video_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static GstClock *gst_win32_video_src_provide_clock (GstElement * elem);

static gboolean gst_win32_ipc_video_src_start (GstBaseSrc * src);
static gboolean gst_win32_ipc_video_src_stop (GstBaseSrc * src);
static gboolean gst_win32_ipc_video_src_unlock (GstBaseSrc * src);
static gboolean gst_win32_ipc_video_src_unlock_stop (GstBaseSrc * src);
static gboolean gst_win32_ipc_video_src_query (GstBaseSrc * src,
    GstQuery * query);
static gboolean gst_win32_ipc_video_src_decide_allocation (GstBaseSrc * src,
    GstQuery * query);
static GstFlowReturn gst_win32_ipc_video_src_create (GstBaseSrc * src,
    guint64 offset, guint size, GstBuffer ** buf);

#define gst_win32_ipc_video_src_parent_class parent_class
G_DEFINE_TYPE (GstWin32IpcVideoSrc, gst_win32_ipc_video_src, GST_TYPE_BASE_SRC);

static void
gst_win32_ipc_video_src_class_init (GstWin32IpcVideoSrcClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstBaseSrcClass *src_class = GST_BASE_SRC_CLASS (klass);

  object_class->finalize = gst_win32_ipc_video_src_finalize;
  object_class->set_property = gst_win32_ipc_video_src_set_property;
  object_class->get_property = gst_win32_video_src_get_property;

  g_object_class_install_property (object_class, PROP_PIPE_NAME,
      g_param_spec_string ("pipe-name", "Pipe Name",
          "The name of Win32 named pipe to communicate with server. "
          "Validation of the pipe name is caller's responsibility",
          DEFAULT_PIPE_NAME, (GParamFlags) (G_PARAM_READWRITE |
              G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_READY)));
  g_object_class_install_property (object_class, PROP_PROCESSING_DEADLINE,
      g_param_spec_uint64 ("processing-deadline", "Processing deadline",
          "Maximum processing time for a buffer in nanoseconds", 0, G_MAXUINT64,
          DEFAULT_PROCESSING_DEADLINE, (GParamFlags) (G_PARAM_READWRITE |
              G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING)));

  gst_element_class_set_static_metadata (element_class,
      "Win32 IPC Video Source", "Source/Video",
      "Receive video frames from the win32ipcvideosink",
      "Seungha Yang <seungha@centricular.com>");
  gst_element_class_add_static_pad_template (element_class, &src_template);

  element_class->provide_clock =
      GST_DEBUG_FUNCPTR (gst_win32_video_src_provide_clock);

  src_class->start = GST_DEBUG_FUNCPTR (gst_win32_ipc_video_src_start);
  src_class->stop = GST_DEBUG_FUNCPTR (gst_win32_ipc_video_src_stop);
  src_class->unlock = GST_DEBUG_FUNCPTR (gst_win32_ipc_video_src_unlock);
  src_class->unlock_stop =
      GST_DEBUG_FUNCPTR (gst_win32_ipc_video_src_unlock_stop);
  src_class->query = GST_DEBUG_FUNCPTR (gst_win32_ipc_video_src_query);
  src_class->decide_allocation =
      GST_DEBUG_FUNCPTR (gst_win32_ipc_video_src_decide_allocation);
  src_class->create = GST_DEBUG_FUNCPTR (gst_win32_ipc_video_src_create);

  GST_DEBUG_CATEGORY_INIT (gst_win32_ipc_video_src_debug, "win32ipcvideosrc",
      0, "win32ipcvideosrc");
}

static void
gst_win32_ipc_video_src_init (GstWin32IpcVideoSrc * self)
{
  gst_base_src_set_format (GST_BASE_SRC (self), GST_FORMAT_TIME);
  gst_base_src_set_live (GST_BASE_SRC (self), TRUE);
  self->pipe_name = g_strdup (DEFAULT_PIPE_NAME);
  self->processing_deadline = DEFAULT_PROCESSING_DEADLINE;

  GST_OBJECT_FLAG_SET (self, GST_ELEMENT_FLAG_PROVIDE_CLOCK);
  GST_OBJECT_FLAG_SET (self, GST_ELEMENT_FLAG_REQUIRE_CLOCK);
}

static void
gst_win32_ipc_video_src_finalize (GObject * object)
{
  GstWin32IpcVideoSrc *self = GST_WIN32_IPC_VIDEO_SRC (object);

  g_free (self->pipe_name);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_win32_ipc_video_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstWin32IpcVideoSrc *self = GST_WIN32_IPC_VIDEO_SRC (object);

  switch (prop_id) {
    case PROP_PIPE_NAME:
      GST_OBJECT_LOCK (self);
      g_free (self->pipe_name);
      self->pipe_name = g_value_dup_string (value);
      if (!self->pipe_name)
        self->pipe_name = g_strdup (DEFAULT_PIPE_NAME);
      GST_OBJECT_UNLOCK (self);
      break;
    case PROP_PROCESSING_DEADLINE:
    {
      GstClockTime prev_val, new_val;
      GST_OBJECT_LOCK (self);
      prev_val = self->processing_deadline;
      new_val = g_value_get_uint64 (value);
      self->processing_deadline = new_val;
      GST_OBJECT_UNLOCK (self);

      if (prev_val != new_val) {
        GST_DEBUG_OBJECT (self, "Posting latency message");
        gst_element_post_message (GST_ELEMENT_CAST (self),
            gst_message_new_latency (GST_OBJECT_CAST (self)));
      }
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_win32_video_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstWin32IpcVideoSrc *self = GST_WIN32_IPC_VIDEO_SRC (object);

  switch (prop_id) {
    case PROP_PIPE_NAME:
      GST_OBJECT_LOCK (self);
      g_value_set_string (value, self->pipe_name);
      GST_OBJECT_UNLOCK (self);
      break;
    case PROP_PROCESSING_DEADLINE:
      GST_OBJECT_LOCK (self);
      g_value_set_uint64 (value, self->processing_deadline);
      GST_OBJECT_UNLOCK (self);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstClock *
gst_win32_video_src_provide_clock (GstElement * elem)
{
  return gst_system_clock_obtain ();
}

static gboolean
gst_win32_ipc_video_src_start (GstBaseSrc * src)
{
  GstWin32IpcVideoSrc *self = GST_WIN32_IPC_VIDEO_SRC (src);

  GST_DEBUG_OBJECT (self, "Start");

  gst_video_info_init (&self->info);

  return TRUE;
}

static gboolean
gst_win32_ipc_video_src_stop (GstBaseSrc * src)
{
  GstWin32IpcVideoSrc *self = GST_WIN32_IPC_VIDEO_SRC (src);

  GST_DEBUG_OBJECT (self, "Stop");

  if (self->pipe) {
    win32_ipc_pipe_client_stop (self->pipe);
    g_clear_pointer (&self->pipe, win32_ipc_pipe_client_unref);
  }

  gst_clear_caps (&self->caps);
  if (self->pool) {
    gst_buffer_pool_set_active (self->pool, FALSE);
    gst_clear_object (&self->pool);
  }

  return TRUE;
}

static gboolean
gst_win32_ipc_video_src_unlock (GstBaseSrc * src)
{
  GstWin32IpcVideoSrc *self = GST_WIN32_IPC_VIDEO_SRC (src);

  GST_DEBUG_OBJECT (self, "Unlock");

  AcquireSRWLockExclusive (&self->lock);
  self->flushing = TRUE;
  if (self->pipe)
    win32_ipc_pipe_client_set_flushing (self->pipe, TRUE);
  ReleaseSRWLockExclusive (&self->lock);

  return TRUE;
}

static gboolean
gst_win32_ipc_video_src_unlock_stop (GstBaseSrc * src)
{
  GstWin32IpcVideoSrc *self = GST_WIN32_IPC_VIDEO_SRC (src);

  GST_DEBUG_OBJECT (self, "Unlock stop");

  AcquireSRWLockExclusive (&self->lock);
  self->flushing = FALSE;
  if (self->pipe)
    win32_ipc_pipe_client_set_flushing (self->pipe, FALSE);
  ReleaseSRWLockExclusive (&self->lock);

  return TRUE;
}

static gboolean
gst_win32_ipc_video_src_query (GstBaseSrc * src, GstQuery * query)
{
  GstWin32IpcVideoSrc *self = GST_WIN32_IPC_VIDEO_SRC (src);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_LATENCY:
    {
      GST_OBJECT_LOCK (self);
      if (GST_CLOCK_TIME_IS_VALID (self->processing_deadline)) {
        gst_query_set_latency (query, TRUE, self->processing_deadline,
            /* pipe server can hold up to 5 memory objects */
            5 * self->processing_deadline);
      } else {
        gst_query_set_latency (query, TRUE, 0, 0);
      }
      GST_OBJECT_UNLOCK (self);
      return TRUE;
    }
    default:
      break;
  }

  return GST_BASE_SRC_CLASS (parent_class)->query (src, query);
}

static gboolean
gst_win32_ipc_video_src_decide_allocation (GstBaseSrc * src, GstQuery * query)
{
  GstWin32IpcVideoSrc *self = GST_WIN32_IPC_VIDEO_SRC (src);
  gboolean ret;

  ret = GST_BASE_SRC_CLASS (parent_class)->decide_allocation (src, query);
  if (!ret)
    return ret;

  self->have_video_meta = gst_query_find_allocation_meta (query,
      GST_VIDEO_META_API_TYPE, nullptr);
  GST_DEBUG_OBJECT (self, "Downstream supports video meta: %d",
      self->have_video_meta);

  return TRUE;
}

static GstCaps *
gst_win32_ipc_video_src_update_info_and_get_caps (GstWin32IpcVideoSrc * self,
    const Win32IpcVideoInfo * info)
{
  GstVideoInfo vinfo;

  gst_video_info_set_format (&vinfo, (GstVideoFormat) info->format,
      info->width, info->height);
  vinfo.fps_n = info->fps_n;
  vinfo.fps_d = info->fps_d;
  vinfo.par_n = info->par_n;
  vinfo.par_d = info->par_d;

  if (!self->caps || !gst_video_info_is_equal (&self->info, &vinfo)) {
    self->info = vinfo;
    return gst_video_info_to_caps (&vinfo);
  }

  return nullptr;
}

static gboolean
gst_win32_ipc_ensure_fallback_pool (GstWin32IpcVideoSrc * self)
{
  GstStructure *config;

  if (self->pool)
    return TRUE;

  self->pool = gst_video_buffer_pool_new ();
  config = gst_buffer_pool_get_config (self->pool);
  gst_buffer_pool_config_set_params (config, self->caps,
      GST_VIDEO_INFO_SIZE (&self->info), 0, 0);
  if (!gst_buffer_pool_set_config (self->pool, config)) {
    GST_ERROR_OBJECT (self, "Couldn't set config");
    goto error;
  }

  if (!gst_buffer_pool_set_active (self->pool, TRUE)) {
    GST_ERROR_OBJECT (self, "Couldn't set active");
    goto error;
  }

  return TRUE;

error:
  gst_clear_object (&self->pool);
  return FALSE;
}

struct MmfReleaseData
{
  Win32IpcPipeClient *pipe;
  Win32IpcMmf *mmf;
};

static void
gst_win32_ipc_video_src_release_mmf (MmfReleaseData * data)
{
  win32_ipc_pipe_client_release_mmf (data->pipe, data->mmf);
  win32_ipc_pipe_client_unref (data->pipe);
  delete data;
}

static GstFlowReturn
gst_win32_ipc_video_src_create (GstBaseSrc * src, guint64 offset, guint size,
    GstBuffer ** buf)
{
  GstWin32IpcVideoSrc *self = GST_WIN32_IPC_VIDEO_SRC (src);
  GstCaps *caps;
  Win32IpcMmf *mmf;
  Win32IpcVideoInfo info;
  GstFlowReturn ret = GST_FLOW_OK;
  GstBuffer *buffer;
  GstClock *clock;
  GstClockTime pts;
  GstClockTime base_time;
  GstClockTime now_qpc;
  GstClockTime now_gst;
  gboolean is_qpc = TRUE;
  gboolean need_video_meta = FALSE;

  AcquireSRWLockExclusive (&self->lock);
  if (self->flushing) {
    ReleaseSRWLockExclusive (&self->lock);
    return GST_FLOW_FLUSHING;
  }

  if (!self->pipe) {
    self->pipe = win32_ipc_pipe_client_new (self->pipe_name);
    if (!self->pipe) {
      ReleaseSRWLockExclusive (&self->lock);
      GST_ERROR_OBJECT (self, "Couldn't create pipe");
      return GST_FLOW_ERROR;
    }
  }
  ReleaseSRWLockExclusive (&self->lock);

  if (!win32_ipc_pipe_client_get_mmf (self->pipe, &mmf, &info)) {
    AcquireSRWLockExclusive (&self->lock);
    if (self->flushing) {
      ret = GST_FLOW_FLUSHING;
      GST_DEBUG_OBJECT (self, "Flushing");
    } else {
      ret = GST_FLOW_EOS;
      GST_WARNING_OBJECT (self, "Couldn't get buffer from server");
    }
    ReleaseSRWLockExclusive (&self->lock);
    return ret;
  }

  caps = gst_win32_ipc_video_src_update_info_and_get_caps (self, &info);
  for (guint i = 0; i < GST_VIDEO_INFO_N_PLANES (&self->info); i++) {
    self->offset[i] = (gsize) info.offset[i];
    self->stride[i] = (gint) info.stride[i];

    if (self->offset[i] != self->info.offset[i] ||
        self->stride[i] != self->info.stride[i]) {
      need_video_meta = TRUE;
    }
  }

  if (caps) {
    if (self->pool) {
      gst_buffer_pool_set_active (self->pool, FALSE);
      gst_clear_object (&self->pool);
    }

    gst_caps_replace (&self->caps, caps);
    GST_DEBUG_OBJECT (self, "Setting caps %" GST_PTR_FORMAT, caps);
    gst_pad_set_caps (GST_BASE_SRC_PAD (src), caps);
    gst_caps_unref (caps);
  }

  if (self->have_video_meta || !need_video_meta) {
    MmfReleaseData *data = new MmfReleaseData ();
    data->pipe = win32_ipc_pipe_client_ref (self->pipe);
    data->mmf = mmf;

    buffer = gst_buffer_new_wrapped_full (GST_MEMORY_FLAG_READONLY,
        win32_ipc_mmf_get_raw (mmf), win32_ipc_mmf_get_size (mmf),
        0, win32_ipc_mmf_get_size (mmf), data,
        (GDestroyNotify) gst_win32_ipc_video_src_release_mmf);

    if (self->have_video_meta) {
      gst_buffer_add_video_meta_full (buffer,
          GST_VIDEO_FRAME_FLAG_NONE, GST_VIDEO_INFO_FORMAT (&self->info),
          GST_VIDEO_INFO_WIDTH (&self->info),
          GST_VIDEO_INFO_HEIGHT (&self->info),
          GST_VIDEO_INFO_N_PLANES (&self->info), self->offset, self->stride);
    }
  } else {
    GstVideoFrame mmf_frame, frame;

    if (!gst_win32_ipc_ensure_fallback_pool (self)) {
      win32_ipc_mmf_unref (mmf);
      return GST_FLOW_ERROR;
    }

    ret = gst_buffer_pool_acquire_buffer (self->pool, &buffer, nullptr);
    if (ret != GST_FLOW_OK) {
      GST_ERROR_OBJECT (self, "Couldn't acquire buffer");
      win32_ipc_mmf_unref (mmf);
      return GST_FLOW_ERROR;
    }

    gst_video_frame_map (&frame, &self->info, buffer, GST_MAP_WRITE);
    mmf_frame.info = self->info;

    for (guint i = 0; i < GST_VIDEO_FRAME_N_PLANES (&frame); i++) {
      mmf_frame.info.offset[i] = self->offset[i];
      mmf_frame.info.stride[i] = self->stride[i];
      mmf_frame.data[i] = (guint8 *) win32_ipc_mmf_get_raw (mmf) +
          self->offset[i];
    }

    gst_video_frame_copy (&frame, &mmf_frame);
    gst_video_frame_unmap (&frame);
    win32_ipc_mmf_unref (mmf);
  }

  now_qpc = gst_util_get_timestamp ();
  clock = gst_element_get_clock (GST_ELEMENT_CAST (self));
  now_gst = gst_clock_get_time (clock);
  base_time = GST_ELEMENT_CAST (self)->base_time;

  is_qpc = gst_win32_ipc_clock_is_qpc (clock);
  gst_object_unref (clock);

  if (!is_qpc) {
    GstClockTimeDiff now_pts = now_gst - base_time + info.qpc - now_qpc;

    if (now_pts >= 0)
      pts = now_pts;
    else
      pts = 0;
  } else {
    if (info.qpc >= base_time) {
      /* Our base_time is also QPC */
      pts = info.qpc - base_time;
    } else {
      GST_WARNING_OBJECT (self, "Server QPC is smaller than our QPC base time");
      pts = 0;
    }
  }

  GST_BUFFER_PTS (buffer) = pts;
  GST_BUFFER_DTS (buffer) = GST_CLOCK_TIME_NONE;
  GST_BUFFER_DURATION (buffer) = GST_CLOCK_TIME_NONE;

  *buf = buffer;

  return GST_FLOW_OK;
}
