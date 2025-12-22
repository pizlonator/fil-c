/* GStreamer Wayland Library
 *
 * Copyright (C) 2016 STMicroelectronics SA
 * Copyright (C) 2016 Fabien Dessenne <fabien.dessenne@st.com>
 * Copyright (C) 2022 Collabora Ltd.
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
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gstwllinuxdmabuf.h"

#include "linux-dmabuf-unstable-v1-client-protocol.h"

GST_DEBUG_CATEGORY (gst_wl_dmabuf_debug);
#define GST_CAT_DEFAULT gst_wl_dmabuf_debug

void
gst_wl_linux_dmabuf_init_once (void)
{
  static gsize _init = 0;

  if (g_once_init_enter (&_init)) {
    GST_DEBUG_CATEGORY_INIT (gst_wl_dmabuf_debug, "wl_dmabuf", 0,
        "wl_dmabuf library");

    g_once_init_leave (&_init, 1);
  }
}

typedef struct
{
  GMutex lock;
  GCond cond;
  struct wl_buffer *wbuf;
} ConstructBufferData;

static void
create_succeeded (void *data, struct zwp_linux_buffer_params_v1 *params,
    struct wl_buffer *new_buffer)
{
  ConstructBufferData *d = data;

  g_mutex_lock (&d->lock);
  d->wbuf = new_buffer;
  zwp_linux_buffer_params_v1_destroy (params);
  g_cond_signal (&d->cond);
  g_mutex_unlock (&d->lock);
}

static void
create_failed (void *data, struct zwp_linux_buffer_params_v1 *params)
{
  ConstructBufferData *d = data;

  g_mutex_lock (&d->lock);
  d->wbuf = NULL;
  zwp_linux_buffer_params_v1_destroy (params);
  g_cond_signal (&d->cond);
  g_mutex_unlock (&d->lock);
}

static const struct zwp_linux_buffer_params_v1_listener params_listener = {
  create_succeeded,
  create_failed
};

struct wl_buffer *
gst_wl_linux_dmabuf_construct_wl_buffer (GstBuffer * buf,
    GstWlDisplay * display, const GstVideoInfoDmaDrm * drm_info)
{
  GstMemory *mem;
  guint fourcc;
  guint64 modifier;
  guint i, width = 0, height = 0;
  GstVideoInfo info;
  const gsize *offsets = NULL;
  const gint *strides = NULL;
  GstVideoMeta *vmeta;
  guint nplanes = 0, flags = 0;
  struct zwp_linux_buffer_params_v1 *params;
  gint64 timeout;
  ConstructBufferData data;

  g_return_val_if_fail (gst_wl_display_check_format_for_dmabuf (display,
          drm_info), NULL);

  mem = gst_buffer_peek_memory (buf, 0);
  fourcc = drm_info->drm_fourcc;
  modifier = drm_info->drm_modifier;

  g_cond_init (&data.cond);
  g_mutex_init (&data.lock);
  g_mutex_lock (&data.lock);

  vmeta = gst_buffer_get_video_meta (buf);
  if (vmeta) {
    width = vmeta->width;
    height = vmeta->height;
    nplanes = vmeta->n_planes;
    offsets = vmeta->offset;
    strides = vmeta->stride;
  } else if (gst_video_info_dma_drm_to_video_info (drm_info, &info)) {
    nplanes = GST_VIDEO_INFO_N_PLANES (&info);
    width = info.width;
    height = info.height;
    offsets = info.offset;
    strides = info.stride;
  } else {
    GST_ERROR_OBJECT (display, "GstVideoMeta is needed to carry DMABuf using "
        "'memory:DMABuf' caps feature.");
    goto out;
  }

  GST_DEBUG_OBJECT (display,
      "Creating wl_buffer from DMABuf of size %" G_GSSIZE_FORMAT
      " (%d x %d), DRM fourcc %" GST_FOURCC_FORMAT, gst_buffer_get_size (buf),
      width, height, GST_FOURCC_ARGS (fourcc));

  /* Creation and configuration of planes  */
  params = zwp_linux_dmabuf_v1_create_params (gst_wl_display_get_dmabuf_v1
      (display));

  for (i = 0; i < nplanes; i++) {
    guint offset, stride, mem_idx, length;
    gsize skip;

    offset = offsets[i];
    stride = strides[i];
    if (gst_buffer_find_memory (buf, offset, 1, &mem_idx, &length, &skip)) {
      GstMemory *m = gst_buffer_peek_memory (buf, mem_idx);
      gint fd = gst_dmabuf_memory_get_fd (m);
      zwp_linux_buffer_params_v1_add (params, fd, i, m->offset + skip,
          stride, modifier >> 32, modifier & G_GUINT64_CONSTANT (0x0ffffffff));
    } else {
      GST_ERROR_OBJECT (mem->allocator, "memory does not seem to contain "
          "enough data for the specified format");
      zwp_linux_buffer_params_v1_destroy (params);
      data.wbuf = NULL;
      goto out;
    }
  }

  if (GST_BUFFER_FLAG_IS_SET (buf, GST_VIDEO_BUFFER_FLAG_INTERLACED)) {
    GST_DEBUG_OBJECT (mem->allocator, "interlaced buffer");
    flags = ZWP_LINUX_BUFFER_PARAMS_V1_FLAGS_INTERLACED;

    if (!GST_BUFFER_FLAG_IS_SET (buf, GST_VIDEO_BUFFER_FLAG_TFF)) {
      GST_DEBUG_OBJECT (mem->allocator, "with bottom field first");
      flags |= ZWP_LINUX_BUFFER_PARAMS_V1_FLAGS_BOTTOM_FIRST;
    }
  }

  /* Request buffer creation */
  zwp_linux_buffer_params_v1_add_listener (params, &params_listener, &data);
  zwp_linux_buffer_params_v1_create (params, width, height, fourcc, flags);

  /* Wait for the request answer */
  wl_display_flush (gst_wl_display_get_display (display));
  data.wbuf = (gpointer) 0x1;
  timeout = g_get_monotonic_time () + G_TIME_SPAN_SECOND;
  while (data.wbuf == (gpointer) 0x1) {
    if (!g_cond_wait_until (&data.cond, &data.lock, timeout)) {
      GST_ERROR_OBJECT (mem->allocator, "zwp_linux_buffer_params_v1 time out");
      zwp_linux_buffer_params_v1_destroy (params);
      data.wbuf = NULL;
    }
  }

out:
  if (!data.wbuf) {
    GST_ERROR_OBJECT (mem->allocator, "can't create linux-dmabuf buffer");
  } else {
    GST_DEBUG_OBJECT (mem->allocator, "created linux_dmabuf wl_buffer (%p):"
        "%dx%d, fmt=%" GST_FOURCC_FORMAT ", %d planes",
        data.wbuf, width, height, GST_FOURCC_ARGS (fourcc), nplanes);
  }

  g_mutex_unlock (&data.lock);
  g_mutex_clear (&data.lock);
  g_cond_clear (&data.cond);

  return data.wbuf;
}
