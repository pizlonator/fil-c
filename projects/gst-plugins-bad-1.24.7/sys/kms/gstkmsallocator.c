/* GStreamer
 *
 * Copyright (C) 2016 Igalia
 *
 * Authors:
 *  Víctor Manuel Jáquez Leal <vjaquez@igalia.com>
 *  Javier Martin <javiermartin@by.com.es>
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
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <fcntl.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

/* it needs to be below because is internal to libdrm */
#include <drm.h>

#include <gst/allocators/gstdmabuf.h>
#include <gst/allocators/gstdrmdumb.h>

#include "gstkmsallocator.h"
#include "gstkmsutils.h"

#ifndef DRM_RDWR
#define DRM_RDWR O_RDWR
#endif

#define GST_CAT_DEFAULT kmsallocator_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

#define GST_KMS_MEMORY_TYPE "KMSMemory"

struct _GstKMSAllocatorPrivate
{
  int fd;
  /* protected by GstKMSAllocator object lock */
  GList *mem_cache;
  GstAllocator *dumb_alloc;
};

#define parent_class gst_kms_allocator_parent_class
G_DEFINE_TYPE_WITH_CODE (GstKMSAllocator, gst_kms_allocator, GST_TYPE_ALLOCATOR,
    G_ADD_PRIVATE (GstKMSAllocator);
    GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "kmsallocator", 0,
        "KMS allocator"));

enum
{
  PROP_DRM_FD = 1,
  PROP_N,
};

static GParamSpec *g_props[PROP_N] = { NULL, };

gboolean
gst_is_kms_memory (GstMemory * mem)
{
  return gst_memory_is_type (mem, GST_KMS_MEMORY_TYPE);
}

guint32
gst_kms_memory_get_fb_id (GstMemory * mem)
{
  if (!gst_is_kms_memory (mem))
    return 0;
  return ((GstKMSMemory *) mem)->fb_id;
}

static gboolean
check_fd (GstKMSAllocator * alloc)
{
  return alloc->priv->fd > -1;
}

static gboolean
gst_kms_allocator_memory_create (GstKMSAllocator * allocator,
    GstKMSMemory * kmsmem, GstVideoInfo * vinfo)
{
  gint i, h;
  gint num_planes = GST_VIDEO_INFO_N_PLANES (vinfo);
  gsize offs = 0;
  guint32 pitch;

  if (kmsmem->bo)
    return TRUE;

  kmsmem->bo = gst_drm_dumb_allocator_alloc (allocator->priv->dumb_alloc,
      gst_drm_format_from_video (GST_VIDEO_INFO_FORMAT (vinfo)),
      GST_VIDEO_INFO_WIDTH (vinfo), GST_VIDEO_INFO_HEIGHT (vinfo), &pitch);
  if (!kmsmem->bo)
    goto create_failed;

  if (!pitch)
    goto done;

  h = GST_VIDEO_INFO_HEIGHT (vinfo);
  for (i = 0; i < num_planes; i++) {
    guint32 stride;

    /* Overwrite the video info's stride and offset using the pitch calculcated
     * by the kms driver. */
    stride = gst_video_format_info_extrapolate_stride (vinfo->finfo, i, pitch);
    GST_VIDEO_INFO_PLANE_STRIDE (vinfo, i) = pitch;
    GST_VIDEO_INFO_PLANE_OFFSET (vinfo, i) = offs;

    /* Note that we cannot negotiate special padding betweem each planes,
     * hence using the display height here. */
    offs += stride * GST_VIDEO_FORMAT_INFO_SCALE_HEIGHT (vinfo->finfo, i, h);

    GST_DEBUG_OBJECT (allocator, "Created BO plane %i with stride %i and "
        "offset %" G_GSIZE_FORMAT, i,
        GST_VIDEO_INFO_PLANE_STRIDE (vinfo, i),
        GST_VIDEO_INFO_PLANE_OFFSET (vinfo, i));
  }

  /* Update with the size use for display, excluding any padding at the end */
  GST_VIDEO_INFO_SIZE (vinfo) = offs;

done:
  /* Validate the size to prevent overflow */
  if (kmsmem->bo->size < GST_VIDEO_INFO_SIZE (vinfo)) {
    GST_ERROR_OBJECT (allocator,
        "DUMB buffer has a size of %" G_GSIZE_FORMAT
        " but we require at least %" G_GSIZE_FORMAT " to hold a frame",
        kmsmem->bo->size, GST_VIDEO_INFO_SIZE (vinfo));
    return FALSE;
  }

  return TRUE;

  /* ERRORS */
create_failed:
  {
    GST_ERROR_OBJECT (allocator, "Failed to create buffer object: %s (%d)",
        g_strerror (errno), errno);
    return FALSE;
  }
}

static void
gst_kms_allocator_free (GstAllocator * allocator, GstMemory * mem)
{
  GstKMSAllocator *alloc;
  GstKMSMemory *kmsmem;

  alloc = GST_KMS_ALLOCATOR (allocator);
  kmsmem = (GstKMSMemory *) mem;

  if (check_fd (alloc) && kmsmem->fb_id) {
    GST_DEBUG_OBJECT (allocator, "removing fb id %d", kmsmem->fb_id);
    drmModeRmFB (alloc->priv->fd, kmsmem->fb_id);
    kmsmem->fb_id = 0;
  }

  if (kmsmem->bo) {
    gst_memory_unref (kmsmem->bo);
    kmsmem->bo = NULL;
  }

  g_free (kmsmem);
}

static void
gst_kms_allocator_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstKMSAllocator *alloc;

  alloc = GST_KMS_ALLOCATOR (object);

  switch (prop_id) {
    case PROP_DRM_FD:{
      int fd = g_value_get_int (value);
      if (fd > -1)
        alloc->priv->fd = dup (fd);
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_kms_allocator_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstKMSAllocator *alloc;

  alloc = GST_KMS_ALLOCATOR (object);

  switch (prop_id) {
    case PROP_DRM_FD:
      g_value_set_int (value, alloc->priv->fd);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_kms_allocator_constructed (GObject * obj)
{
  GstKMSAllocator *alloc;

  alloc = GST_KMS_ALLOCATOR (obj);

  /* Should be called after the properties are set */
  g_assert (check_fd (alloc));
  alloc->priv->dumb_alloc =
      gst_drm_dumb_allocator_new_with_fd (alloc->priv->fd);

  /* Its already opened and we already checked for dumb allocation support */
  g_assert (alloc->priv->dumb_alloc);
}

static void
gst_kms_allocator_finalize (GObject * obj)
{
  GstKMSAllocator *alloc;

  alloc = GST_KMS_ALLOCATOR (obj);

  gst_kms_allocator_clear_cache (GST_ALLOCATOR (alloc));

  if (alloc->priv->dumb_alloc)
    gst_object_unref (alloc->priv->dumb_alloc);

  if (check_fd (alloc))
    close (alloc->priv->fd);

  G_OBJECT_CLASS (parent_class)->finalize (obj);
}

static void
gst_kms_allocator_class_init (GstKMSAllocatorClass * klass)
{
  GObjectClass *gobject_class;
  GstAllocatorClass *allocator_class;

  allocator_class = GST_ALLOCATOR_CLASS (klass);
  gobject_class = G_OBJECT_CLASS (klass);

  allocator_class->free = gst_kms_allocator_free;

  gobject_class->set_property = gst_kms_allocator_set_property;
  gobject_class->get_property = gst_kms_allocator_get_property;
  gobject_class->constructed = gst_kms_allocator_constructed;
  gobject_class->finalize = gst_kms_allocator_finalize;

  g_props[PROP_DRM_FD] = g_param_spec_int ("drm-fd", "DRM fd",
      "DRM file descriptor", -1, G_MAXINT, -1,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (gobject_class, PROP_N, g_props);
}

static gpointer
gst_kms_memory_map (GstMemory * mem, gsize maxsize, GstMapFlags flags)
{
  GstKMSMemory *kmsmem;

  kmsmem = (GstKMSMemory *) mem;
  if (!kmsmem->bo)
    return NULL;

  if (kmsmem->bo_map.data)
    goto out;

  if (!gst_memory_map (kmsmem->bo, &kmsmem->bo_map, flags))
    return NULL;

out:
  g_atomic_int_inc (&kmsmem->bo_map_refs);
  return kmsmem->bo_map.data;
}

static void
gst_kms_memory_unmap (GstMemory * mem)
{
  GstKMSMemory *kmsmem;

  kmsmem = (GstKMSMemory *) mem;
  if (!kmsmem->bo)
    return;

  if (g_atomic_int_dec_and_test (&kmsmem->bo_map_refs)) {
    gst_memory_unmap (kmsmem->bo, &kmsmem->bo_map);
    kmsmem->bo_map.data = NULL;
  }
}

static void
gst_kms_allocator_init (GstKMSAllocator * allocator)
{
  GstAllocator *alloc;

  alloc = GST_ALLOCATOR_CAST (allocator);

  allocator->priv = gst_kms_allocator_get_instance_private (allocator);
  allocator->priv->fd = -1;

  alloc->mem_type = GST_KMS_MEMORY_TYPE;
  alloc->mem_map = gst_kms_memory_map;
  alloc->mem_unmap = gst_kms_memory_unmap;
  /* Use the default, fallback copy function */

  GST_OBJECT_FLAG_SET (allocator, GST_ALLOCATOR_FLAG_CUSTOM_ALLOC);
}

GstAllocator *
gst_kms_allocator_new (int fd)
{
  GstAllocator *alloc;

  alloc = g_object_new (GST_TYPE_KMS_ALLOCATOR, "name",
      "KMSMemory::allocator", "drm-fd", fd, NULL);
  gst_object_ref_sink (alloc);

  return alloc;
}

/* The mem_offsets are relative to the GstMemory start, unlike the vinfo->offset
 * which are relative to the GstBuffer start. */
static gboolean
gst_kms_allocator_add_fb (GstKMSAllocator * alloc, GstKMSMemory * kmsmem,
    gsize in_offsets[GST_VIDEO_MAX_PLANES], GstVideoInfo * vinfo,
    guint32 bo_handles[4])
{
  gint i, ret;
  gint num_planes = GST_VIDEO_INFO_N_PLANES (vinfo);
  guint32 w, h, fmt;
  guint32 pitches[4] = { 0, };
  guint32 offsets[4] = { 0, };

  if (kmsmem->fb_id)
    return TRUE;

  w = GST_VIDEO_INFO_WIDTH (vinfo);
  h = GST_VIDEO_INFO_HEIGHT (vinfo);
  fmt = gst_drm_format_from_video (GST_VIDEO_INFO_FORMAT (vinfo));

  for (i = 0; i < num_planes; i++) {
    pitches[i] = GST_VIDEO_INFO_PLANE_STRIDE (vinfo, i);
    offsets[i] = in_offsets[i];
  }

  GST_DEBUG_OBJECT (alloc, "bo handles: %d, %d, %d, %d", bo_handles[0],
      bo_handles[1], bo_handles[2], bo_handles[3]);

  ret = drmModeAddFB2 (alloc->priv->fd, w, h, fmt, bo_handles, pitches,
      offsets, &kmsmem->fb_id, 0);
  if (ret) {
    GST_ERROR_OBJECT (alloc, "Failed to bind to framebuffer: %s (%d)",
        g_strerror (errno), errno);
    return FALSE;
  }

  return TRUE;
}

GstMemory *
gst_kms_allocator_bo_alloc (GstAllocator * allocator, GstVideoInfo * vinfo)
{
  GstKMSAllocator *alloc;
  GstKMSMemory *kmsmem;
  GstMemory *mem;
  guint32 bo_handle[4] = { 0, };
  gint i;

  kmsmem = g_new0 (GstKMSMemory, 1);

  alloc = GST_KMS_ALLOCATOR (allocator);

  mem = GST_MEMORY_CAST (kmsmem);

  if (!gst_kms_allocator_memory_create (alloc, kmsmem, vinfo)) {
    g_free (kmsmem);
    return NULL;
  }

  gst_memory_init (mem, GST_MEMORY_FLAG_NO_SHARE, allocator, NULL,
      kmsmem->bo->maxsize, 0, 0, GST_VIDEO_INFO_SIZE (vinfo));

  for (i = 0; i < GST_VIDEO_INFO_N_PLANES (vinfo); i++)
    bo_handle[i] = gst_drm_dumb_memory_get_handle (kmsmem->bo);

  if (!gst_kms_allocator_add_fb (alloc, kmsmem, vinfo->offset, vinfo,
          bo_handle))
    goto fail;

  return mem;

  /* ERRORS */
fail:
  gst_memory_unref (mem);
  return NULL;
}

GstKMSMemory *
gst_kms_allocator_dmabuf_import (GstAllocator * allocator, gint * prime_fds,
    gint n_planes, gsize offsets[GST_VIDEO_MAX_PLANES], GstVideoInfo * vinfo)
{
  GstKMSAllocator *alloc;
  GstKMSMemory *kmsmem;
  GstMemory *mem;
  gint i, j, ret;
  guint32 gem_handle[4] = { 0, };

  g_return_val_if_fail (n_planes <= GST_VIDEO_MAX_PLANES, FALSE);

  kmsmem = g_new0 (GstKMSMemory, 1);

  mem = GST_MEMORY_CAST (kmsmem);
  gst_memory_init (mem, GST_MEMORY_FLAG_NO_SHARE, allocator, NULL,
      GST_VIDEO_INFO_SIZE (vinfo), 0, 0, GST_VIDEO_INFO_SIZE (vinfo));

  alloc = GST_KMS_ALLOCATOR (allocator);
  for (i = 0; i < n_planes; i++) {
    ret = drmPrimeFDToHandle (alloc->priv->fd, prime_fds[i], &gem_handle[i]);
    if (ret)
      goto import_fd_failed;
  }

  if (!gst_kms_allocator_add_fb (alloc, kmsmem, offsets, vinfo, gem_handle))
    goto failed;

done:
  for (i = 0; i < n_planes; i++) {
    struct drm_gem_close arg = { gem_handle[i], };
    gint err;

    if (!gem_handle[i])
      continue;

    /* Do not close the same handle twice. */
    for (j = 0; j < i; j++) {
      if (gem_handle[j] == gem_handle[i])
        break;
    }
    if (j < i)
      continue;

    err = drmIoctl (alloc->priv->fd, DRM_IOCTL_GEM_CLOSE, &arg);
    if (err)
      GST_WARNING_OBJECT (allocator,
          "Failed to close GEM handle: %s %d", g_strerror (errno), errno);
  }

  return kmsmem;

  /* ERRORS */
import_fd_failed:
  GST_ERROR_OBJECT (alloc, "Failed to import prime fd %d: %s (%d)",
      prime_fds[i], g_strerror (errno), errno);
  /* fallthrough */

failed:
  gst_memory_unref (mem);
  mem = NULL;
  kmsmem = NULL;
  goto done;
}

GstMemory *
gst_kms_allocator_dmabuf_export (GstAllocator * allocator, GstMemory * _kmsmem)
{
  GstKMSMemory *kmsmem = (GstKMSMemory *) _kmsmem;
  GstKMSAllocator *alloc = GST_KMS_ALLOCATOR (allocator);
  GstMemory *mem;

  /* We can only export DUMB buffers */
  g_return_val_if_fail (kmsmem->bo, NULL);

  mem = gst_drm_dumb_memory_export_dmabuf (kmsmem->bo);
  if (!mem)
    goto export_fd_failed;

  /* Populate the cache so KMSSink can find the kmsmem back when it receives
   * one of these DMABuf. This call takes ownership of the kmsmem. */
  gst_kms_allocator_cache (allocator, mem, _kmsmem);

  GST_DEBUG_OBJECT (alloc, "Exported bo handle %d as %d",
      gst_drm_dumb_memory_get_handle (kmsmem->bo),
      gst_dmabuf_memory_get_fd (mem));

  return mem;

  /* ERRORS */
export_fd_failed:
  {
    GST_ERROR_OBJECT (alloc, "Failed to export bo handle %d: %s (%d)",
        gst_drm_dumb_memory_get_handle (kmsmem->bo), g_strerror (errno), errno);
    return NULL;
  }
}

/* FIXME, using gdata for caching on upstream memory is not tee safe */
GstMemory *
gst_kms_allocator_get_cached (GstMemory * mem)
{
  return gst_mini_object_get_qdata (GST_MINI_OBJECT (mem),
      g_quark_from_static_string ("kmsmem"));
}

static void
cached_kmsmem_disposed_cb (GstKMSAllocator * alloc, GstMiniObject * obj)
{
  GST_OBJECT_LOCK (alloc);
  alloc->priv->mem_cache = g_list_remove (alloc->priv->mem_cache, obj);
  GST_OBJECT_UNLOCK (alloc);
}

void
gst_kms_allocator_clear_cache (GstAllocator * allocator)
{
  GstKMSAllocator *alloc = GST_KMS_ALLOCATOR (allocator);
  GList *iter;

  GST_OBJECT_LOCK (alloc);

  iter = alloc->priv->mem_cache;
  while (iter) {
    GstMiniObject *obj = iter->data;
    gst_mini_object_weak_unref (obj,
        (GstMiniObjectNotify) cached_kmsmem_disposed_cb, alloc);
    gst_mini_object_set_qdata (obj,
        g_quark_from_static_string ("kmsmem"), NULL, NULL);
    iter = iter->next;
  }

  g_list_free (alloc->priv->mem_cache);
  alloc->priv->mem_cache = NULL;

  GST_OBJECT_UNLOCK (alloc);
}

/* @kmsmem is transfer-full */
void
gst_kms_allocator_cache (GstAllocator * allocator, GstMemory * mem,
    GstMemory * kmsmem)
{
  GstKMSAllocator *alloc = GST_KMS_ALLOCATOR (allocator);

  GST_OBJECT_LOCK (alloc);
  gst_mini_object_weak_ref (GST_MINI_OBJECT (mem),
      (GstMiniObjectNotify) cached_kmsmem_disposed_cb, alloc);
  alloc->priv->mem_cache = g_list_prepend (alloc->priv->mem_cache, mem);
  GST_OBJECT_UNLOCK (alloc);

  gst_mini_object_set_qdata (GST_MINI_OBJECT (mem),
      g_quark_from_static_string ("kmsmem"), kmsmem,
      (GDestroyNotify) gst_memory_unref);
}
