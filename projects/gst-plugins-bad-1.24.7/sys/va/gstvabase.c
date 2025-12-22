/* GStreamer
 * Copyright (C) 2023 Igalia, S.L.
 *     Author: Víctor Jáquez <vjaquez@igalia.com>
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

#include "gstvabase.h"

#include <gst/video/video.h>

#include <gst/va/gstvavideoformat.h>
#include <gst/va/vasurfaceimage.h>

#define GST_CAT_DEFAULT (importer->debug_category)

/* big bad mutex to exclusive access to shared stream buffers, such as
 * DMABuf after a tee */
static GRecMutex GST_VA_SHARED_LOCK = { 0, };

static gboolean
_try_import_dmabuf_unlocked (GstVaBufferImporter * importer, GstBuffer * inbuf)
{
  GstVideoMeta *meta;
  GstVideoInfoDmaDrm drm_info = *importer->in_drm_info;
  GstMemory *mems[GST_VIDEO_MAX_PLANES];
  guint i, n_planes, usage_hint;
  gsize offset[GST_VIDEO_MAX_PLANES];
  uintptr_t fd[GST_VIDEO_MAX_PLANES];

  /* This will eliminate most non-dmabuf out there */
  if (!gst_is_dmabuf_memory (gst_buffer_peek_memory (inbuf, 0)))
    return FALSE;

  n_planes = GST_VIDEO_INFO_N_PLANES (&drm_info.vinfo);

  meta = gst_buffer_get_video_meta (inbuf);

  /* Update video info importerd on video meta */
  if (meta) {
    GST_VIDEO_INFO_WIDTH (&drm_info.vinfo) = meta->width;
    GST_VIDEO_INFO_HEIGHT (&drm_info.vinfo) = meta->height;

    g_assert (n_planes == meta->n_planes);

    for (i = 0; i < n_planes; i++) {
      GST_VIDEO_INFO_PLANE_OFFSET (&drm_info.vinfo, i) = meta->offset[i];
      GST_VIDEO_INFO_PLANE_STRIDE (&drm_info.vinfo, i) = meta->stride[i];
    }
  }

  /* Find and validate all memories */
  for (i = 0; i < n_planes; i++) {
    guint length;
    guint mem_idx;
    gsize mem_skip;

    if (!gst_buffer_find_memory (inbuf,
            GST_VIDEO_INFO_PLANE_OFFSET (&drm_info.vinfo, i), 1, &mem_idx,
            &length, &mem_skip))
      return FALSE;

    /* We can't have more then one dmabuf per plane */
    if (length != 1)
      return FALSE;

    mems[i] = gst_buffer_peek_memory (inbuf, mem_idx);

    /* And all memory found must be dmabuf */
    if (!gst_is_dmabuf_memory (mems[i]))
      return FALSE;

    offset[i] = mems[i]->offset + mem_skip;
    fd[i] = gst_dmabuf_memory_get_fd (mems[i]);
  }

  usage_hint = va_get_surface_usage_hint (importer->display,
      importer->entrypoint, GST_PAD_SINK, TRUE);

  /* Now create a VASurfaceID for the buffer */
  return gst_va_dmabuf_memories_setup (importer->display, &drm_info, mems, fd,
      offset, usage_hint);
}

static gboolean
_try_import_buffer (GstVaBufferImporter * importer, GstBuffer * inbuf)
{
  VASurfaceID surface;
  gboolean ret;

  surface = gst_va_buffer_get_surface (inbuf);
  if (surface != VA_INVALID_ID &&
      (gst_va_buffer_peek_display (inbuf) == importer->display))
    return TRUE;

  g_rec_mutex_lock (&GST_VA_SHARED_LOCK);
  ret = _try_import_dmabuf_unlocked (importer, inbuf);
  g_rec_mutex_unlock (&GST_VA_SHARED_LOCK);

  return ret;
}

GstFlowReturn
gst_va_buffer_importer_import (GstVaBufferImporter * importer,
    GstBuffer * inbuf, GstBuffer ** outbuf)
{
  GstBuffer *buffer = NULL;
  GstBufferPool *pool;
  GstFlowReturn ret;
  GstVideoFrame in_frame, out_frame;
  gboolean imported, copied;

  imported = _try_import_buffer (importer, inbuf);
  if (imported) {
    *outbuf = gst_buffer_ref (inbuf);
    return GST_FLOW_OK;
  }

  /* input buffer doesn't come from a vapool, thus it is required to
   * have a pool, grab from it a new buffer and copy the input
   * buffer to the new one */
  if (!(pool = importer->get_sinkpad_pool (importer->element,
              importer->pool_data)))
    return GST_FLOW_ERROR;

  ret = gst_buffer_pool_acquire_buffer (pool, &buffer, NULL);
  if (ret != GST_FLOW_OK)
    return ret;

  GST_LOG_OBJECT (importer->element, "copying input frame");

  if (!gst_video_frame_map (&in_frame, importer->in_info, inbuf, GST_MAP_READ))
    goto invalid_buffer;

  if (!gst_video_frame_map (&out_frame, importer->sinkpad_info, buffer,
          GST_MAP_WRITE)) {
    gst_video_frame_unmap (&in_frame);
    goto invalid_buffer;
  }

  copied = gst_video_frame_copy (&out_frame, &in_frame);

  gst_video_frame_unmap (&out_frame);
  gst_video_frame_unmap (&in_frame);

  if (!copied)
    goto invalid_buffer;

  if (!gst_buffer_copy_into (buffer, inbuf, GST_BUFFER_COPY_FLAGS |
          GST_BUFFER_COPY_TIMESTAMPS, 0, -1)) {
    GST_WARNING_OBJECT (importer->element,
        "Couldn't import buffer flags and timestamps");
  }

  *outbuf = buffer;

  return GST_FLOW_OK;

invalid_buffer:
  {
    GST_ELEMENT_WARNING (importer->element, STREAM, FORMAT, (NULL),
        ("invalid video buffer received"));
    if (buffer)
      gst_buffer_unref (buffer);
    return GST_FLOW_ERROR;
  }
}


gboolean
gst_va_base_convert_caps_to_va (GstCaps * caps)
{
  g_return_val_if_fail (gst_caps_is_fixed (caps), FALSE);

  /* For DMA buffer, we can only import linear buffers. Replace the drm-format
   * into format field. */
  if (gst_video_is_dma_drm_caps (caps)) {
    GstVideoInfoDmaDrm dma_info;
    GstVideoInfo info;

    if (!gst_video_info_dma_drm_from_caps (&dma_info, caps))
      return FALSE;

    if (dma_info.drm_modifier != DRM_FORMAT_MOD_LINEAR)
      return FALSE;

    if (!gst_va_dma_drm_info_to_video_info (&dma_info, &info))
      return FALSE;

    gst_caps_set_simple (caps, "format", G_TYPE_STRING,
        gst_video_format_to_string (GST_VIDEO_INFO_FORMAT (&info)), NULL);
    gst_structure_remove_field (gst_caps_get_structure (caps, 0), "drm-format");
  }

  gst_caps_set_features_simple (caps,
      gst_caps_features_from_string (GST_CAPS_FEATURE_MEMORY_VA));

  return TRUE;
}
