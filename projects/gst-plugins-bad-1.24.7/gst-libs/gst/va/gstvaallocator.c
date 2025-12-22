/* GStreamer
 * Copyright (C) 2020 Igalia, S.L.
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

/**
 * SECTION:gstvaallocator
 * @title: VA allocators
 * @short_description: VA allocators
 * @sources:
 * - gstvaallocator.h
 *
 * There are two types of VA allocators:
 *
 * * #GstVaAllocator
 * * #GstVaDmabufAllocator
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstvaallocator.h"

#ifndef G_OS_WIN32
#include <sys/types.h>
#include <unistd.h>
#endif

#include "gstvasurfacecopy.h"
#include "gstvavideoformat.h"
#include "vasurfaceimage.h"

#ifndef DRM_FORMAT_INVALID
#define DRM_FORMAT_INVALID    0
#endif

#define GST_CAT_DEFAULT gst_va_memory_debug
GST_DEBUG_CATEGORY (gst_va_memory_debug);

static void
_init_debug_category (void)
{
#ifndef GST_DISABLE_GST_DEBUG
  static gsize _init = 0;

  if (g_once_init_enter (&_init)) {
    GST_DEBUG_CATEGORY_INIT (gst_va_memory_debug, "vamemory", 0, "VA memory");
    g_once_init_leave (&_init, 1);
  }
#endif
}

/*=========================== Quarks for GstMemory ===========================*/

static GQuark
gst_va_buffer_surface_quark (void)
{
  static gsize surface_quark = 0;

  if (g_once_init_enter (&surface_quark)) {
    GQuark quark = g_quark_from_string ("GstVaBufferSurface");
    g_once_init_leave (&surface_quark, quark);
  }

  return surface_quark;
}

static GQuark
gst_va_buffer_aux_surface_quark (void)
{
  static gsize surface_quark = 0;

  if (g_once_init_enter (&surface_quark)) {
    GQuark quark = g_quark_from_string ("GstVaBufferAuxSurface");
    g_once_init_leave (&surface_quark, quark);
  }

  return surface_quark;
}

/*========================= GstVaBufferSurface ===============================*/

typedef struct _GstVaBufferSurface GstVaBufferSurface;
struct _GstVaBufferSurface
{
  GstVaDisplay *display;
  VASurfaceID surface;
  guint n_mems;
  GstMemory *mems[GST_VIDEO_MAX_PLANES];
  gint ref_count;
  gint ref_mems_count;
};

static void
gst_va_buffer_surface_unref (gpointer data)
{
  GstVaBufferSurface *buf = data;

  g_return_if_fail (buf && GST_IS_VA_DISPLAY (buf->display));

  if (g_atomic_int_dec_and_test (&buf->ref_count)) {
    GST_LOG_OBJECT (buf->display, "Destroying surface %#x", buf->surface);
    va_destroy_surfaces (buf->display, &buf->surface, 1);
    gst_clear_object (&buf->display);
    g_free (buf);
  }
}

static GstVaBufferSurface *
gst_va_buffer_surface_new (VASurfaceID surface)
{
  GstVaBufferSurface *buf = g_new (GstVaBufferSurface, 1);

  g_atomic_int_set (&buf->ref_count, 0);
  g_atomic_int_set (&buf->ref_mems_count, 0);
  buf->surface = surface;
  buf->display = NULL;
  buf->n_mems = 0;

  return buf;
}

/*=========================== GstVaMemoryPool ================================*/

/* queue for disposed surfaces */
typedef struct _GstVaMemoryPool GstVaMemoryPool;
struct _GstVaMemoryPool
{
  GstAtomicQueue *queue;
  gint surface_count;

  GMutex lock;
};

#define GST_VA_MEMORY_POOL_CAST(obj) ((GstVaMemoryPool *)obj)
#define GST_VA_MEMORY_POOL_LOCK(obj) g_mutex_lock (&GST_VA_MEMORY_POOL_CAST(obj)->lock)
#define GST_VA_MEMORY_POOL_UNLOCK(obj) g_mutex_unlock (&GST_VA_MEMORY_POOL_CAST(obj)->lock)

static void
gst_va_memory_pool_init (GstVaMemoryPool * self)
{
  self->queue = gst_atomic_queue_new (2);

  g_mutex_init (&self->lock);

  self->surface_count = 0;
}

static void
gst_va_memory_pool_finalize (GstVaMemoryPool * self)
{
  g_mutex_clear (&self->lock);

  gst_atomic_queue_unref (self->queue);
}

static void
gst_va_memory_pool_flush_unlocked (GstVaMemoryPool * self,
    GstVaDisplay * display)
{
  GstMemory *mem;
  GstVaBufferSurface *buf;

  while ((mem = gst_atomic_queue_pop (self->queue))) {
    /* destroy the surface */
    buf = gst_mini_object_get_qdata (GST_MINI_OBJECT (mem),
        gst_va_buffer_surface_quark ());
    if (buf) {
      if (g_atomic_int_dec_and_test (&buf->ref_count)) {
        GST_LOG ("Destroying surface %#x", buf->surface);
        va_destroy_surfaces (display, &buf->surface, 1);
        self->surface_count -= 1;       /* GstVaDmabufAllocator */
        g_free (buf);
      }
    } else {
      self->surface_count -= 1; /* GstVaAllocator */
    }

    GST_MINI_OBJECT_CAST (mem)->dispose = NULL;
    /* when mem are pushed available queue its allocator is unref,
     * then now it is required to ref the allocator here because
     * memory's finalize will unref it again */
    gst_object_ref (mem->allocator);
    gst_memory_unref (mem);
  }
}

static void
gst_va_memory_pool_flush (GstVaMemoryPool * self, GstVaDisplay * display)
{
  GST_VA_MEMORY_POOL_LOCK (self);
  gst_va_memory_pool_flush_unlocked (self, display);
  GST_VA_MEMORY_POOL_UNLOCK (self);
}

static inline void
gst_va_memory_pool_push (GstVaMemoryPool * self, GstMemory * mem)
{
  gst_atomic_queue_push (self->queue, gst_memory_ref (mem));
}

static inline GstMemory *
gst_va_memory_pool_pop (GstVaMemoryPool * self)
{
  return gst_atomic_queue_pop (self->queue);
}

static inline GstMemory *
gst_va_memory_pool_peek (GstVaMemoryPool * self)
{
  return gst_atomic_queue_peek (self->queue);
}

static inline guint
gst_va_memory_pool_surface_count (GstVaMemoryPool * self)
{
  return g_atomic_int_get (&self->surface_count);
}

static inline void
gst_va_memory_pool_surface_inc (GstVaMemoryPool * self)
{
  g_atomic_int_inc (&self->surface_count);
}

/*=========================== GstVaDmabufAllocator ===========================*/

/**
 * GstVaDmabufAllocator:
 *
 * A pooled memory allocator backed by the DMABufs exported from a
 * VASurfaceID. Also it is possible to import DMAbufs into a
 * VASurfaceID.
 *
 * Since: 1.22
 */
typedef struct _GstVaDmabufAllocator GstVaDmabufAllocator;
typedef struct _GstVaDmabufAllocatorClass GstVaDmabufAllocatorClass;

struct _GstVaDmabufAllocator
{
  GstDmaBufAllocator parent;

  GstVaDisplay *display;

  GstMemoryMapFunction parent_map;
  GstMemoryCopyFunction parent_copy;

  GstVideoInfoDmaDrm info;
  guint usage_hint;

  GstVaSurfaceCopy *copy;

  GstVaMemoryPool pool;
};

struct _GstVaDmabufAllocatorClass
{
  GstDmaBufAllocatorClass parent_class;
};

#define gst_va_dmabuf_allocator_parent_class dmabuf_parent_class
G_DEFINE_TYPE_WITH_CODE (GstVaDmabufAllocator, gst_va_dmabuf_allocator,
    GST_TYPE_DMABUF_ALLOCATOR, _init_debug_category ());

static GstVaSurfaceCopy *
_ensure_surface_copy (GstVaSurfaceCopy ** old, GstVaDisplay * display,
    GstVideoInfo * info)
{
  GstVaSurfaceCopy *surface_copy;

  surface_copy = g_atomic_pointer_get (old);
  if (!surface_copy) {
    surface_copy = gst_va_surface_copy_new (display, info);

    /* others create a new one and set it before us */
    if (surface_copy &&
        !g_atomic_pointer_compare_and_exchange (old, NULL, surface_copy)) {
      gst_va_surface_copy_free (surface_copy);
      surface_copy = g_atomic_pointer_get (old);
    }
  }

  return surface_copy;
}

/* If a buffer contains multiple memories (dmabuf objects) its very
 * difficult to provide a realiable way to fast-copy single memories:
 * While VA API sees surfaces with dependant dmabufs, GStreamer only
 * copies dmabufs in isolation; trying to solve it while keeping a
 * reference of the copied buffer and dmabuf index is very fragile. */
static GstMemory *
gst_va_dmabuf_mem_copy (GstMemory * gmem, gssize offset, gssize size)
{
  GstVaDmabufAllocator *self = GST_VA_DMABUF_ALLOCATOR (gmem->allocator);
  GstVaBufferSurface *buf;
  gsize mem_size;

  buf = gst_mini_object_get_qdata (GST_MINI_OBJECT (gmem),
      gst_va_buffer_surface_quark ());

  if (buf->n_mems > 1 && self->info.drm_modifier != DRM_FORMAT_MOD_LINEAR) {
    GST_ERROR_OBJECT (self, "Failed to copy multi-dmabuf because non-linear "
        "modifier: %#" G_GINT64_MODIFIER "x.", self->info.drm_modifier);
    return NULL;
  }

  /* check if it's full memory copy */
  mem_size = gst_memory_get_sizes (gmem, NULL, NULL);

  if (size == -1)
    size = mem_size > offset ? mem_size - offset : 0;

  /* @XXX: if one-memory buffer it's possible to copy */
  if (offset == 0 && size == mem_size && buf->n_mems == 1) {
    GstVaBufferSurface *buf_copy = NULL;
    GstMemory *copy;
    GstVaSurfaceCopy *copy_func;

    GST_VA_MEMORY_POOL_LOCK (&self->pool);
    copy = gst_va_memory_pool_pop (&self->pool);
    GST_VA_MEMORY_POOL_UNLOCK (&self->pool);

    if (copy) {
      gst_object_ref (copy->allocator);

      buf_copy = gst_mini_object_get_qdata (GST_MINI_OBJECT (copy),
          gst_va_buffer_surface_quark ());

      g_assert (g_atomic_int_get (&buf_copy->ref_mems_count) == 0);

      g_atomic_int_add (&buf_copy->ref_mems_count, 1);
    } else {
      GstBuffer *buffer = gst_buffer_new ();

      if (!gst_va_dmabuf_allocator_setup_buffer (gmem->allocator, buffer)) {
        GST_WARNING_OBJECT (self, "Failed to create a new dmabuf memory");
        return NULL;
      }

      copy = gst_buffer_get_memory (buffer, 0);
      gst_buffer_unref (buffer);

      buf_copy = gst_mini_object_get_qdata (GST_MINI_OBJECT (copy),
          gst_va_buffer_surface_quark ());
    }

    g_assert (buf_copy->n_mems == 1);

    copy_func =
        _ensure_surface_copy (&self->copy, self->display, &self->info.vinfo);
    if (copy_func
        && gst_va_surface_copy (copy_func, buf_copy->surface, buf->surface))
      return copy;

    gst_memory_unref (copy);

    /* try system memory */
  }

  if (self->info.drm_modifier != DRM_FORMAT_MOD_LINEAR) {
    GST_ERROR_OBJECT (self, "Failed to copy dmabuf because non-linear "
        "modifier: %#" G_GINT64_MODIFIER "x.", self->info.drm_modifier);
    return NULL;
  }

  /* fallback to system memory */
  return self->parent_copy (gmem, offset, size);

}

static gpointer
gst_va_dmabuf_mem_map (GstMemory * gmem, gsize maxsize, GstMapFlags flags)
{
  GstVaDmabufAllocator *self = GST_VA_DMABUF_ALLOCATOR (gmem->allocator);
  VASurfaceID surface = gst_va_memory_get_surface (gmem);

  if (flags & GST_MAP_READWRITE) {
    if (self->info.drm_modifier != DRM_FORMAT_MOD_LINEAR) {
      GST_ERROR_OBJECT (self, "Failed to map the dmabuf because the modifier "
          "is: %#" G_GINT64_MODIFIER "x, which is not linear.",
          self->info.drm_modifier);
      return NULL;
    }

    if (!va_sync_surface (self->display, surface))
      return NULL;
  }

  return self->parent_map (gmem, maxsize, flags);
}

static void
gst_va_dmabuf_allocator_finalize (GObject * object)
{
  GstVaDmabufAllocator *self = GST_VA_DMABUF_ALLOCATOR (object);

  g_clear_pointer (&self->copy, gst_va_surface_copy_free);
  gst_va_memory_pool_finalize (&self->pool);
  gst_clear_object (&self->display);

  G_OBJECT_CLASS (dmabuf_parent_class)->finalize (object);
}

static void
gst_va_dmabuf_allocator_dispose (GObject * object)
{
  GstVaDmabufAllocator *self = GST_VA_DMABUF_ALLOCATOR (object);

  gst_va_memory_pool_flush_unlocked (&self->pool, self->display);
  if (gst_va_memory_pool_surface_count (&self->pool) != 0) {
    GST_WARNING_OBJECT (self, "Surfaces leaked: %d",
        gst_va_memory_pool_surface_count (&self->pool));
  }

  G_OBJECT_CLASS (dmabuf_parent_class)->dispose (object);
}

static void
gst_va_dmabuf_allocator_class_init (GstVaDmabufAllocatorClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gst_va_dmabuf_allocator_dispose;
  object_class->finalize = gst_va_dmabuf_allocator_finalize;
}

static void
gst_va_dmabuf_allocator_init (GstVaDmabufAllocator * self)
{
  GstAllocator *allocator = GST_ALLOCATOR (self);

  self->parent_map = allocator->mem_map;
  allocator->mem_map = gst_va_dmabuf_mem_map;
  self->parent_copy = allocator->mem_copy;
  allocator->mem_copy = gst_va_dmabuf_mem_copy;

  gst_video_info_dma_drm_init (&self->info);

  gst_va_memory_pool_init (&self->pool);
}

/**
 * gst_va_dmabuf_allocator_new:
 * @display: a #GstVaDisplay
 *
 * Instanciate a new pooled allocator backed with both DMABuf and
 * VASurfaceID.
 *
 * Returns: a new allocated #GstAllocator
 *
 * Since: 1.22
 */
GstAllocator *
gst_va_dmabuf_allocator_new (GstVaDisplay * display)
{
  GstVaDmabufAllocator *self;

  g_return_val_if_fail (GST_IS_VA_DISPLAY (display), NULL);

  self = g_object_new (GST_TYPE_VA_DMABUF_ALLOCATOR, NULL);
  self->display = gst_object_ref (display);
  gst_object_ref_sink (self);

  return GST_ALLOCATOR (self);
}

static inline goffset
_get_fd_size (gint fd)
{
#ifndef G_OS_WIN32
  return lseek (fd, 0, SEEK_END);
#else
  return 0;
#endif
}

static gboolean
gst_va_dmabuf_memory_release (GstMiniObject * mini_object)
{
  GstMemory *mem = GST_MEMORY_CAST (mini_object);
  GstVaBufferSurface *buf;
  GstVaDmabufAllocator *self = GST_VA_DMABUF_ALLOCATOR (mem->allocator);
  guint i;

  buf = gst_mini_object_get_qdata (GST_MINI_OBJECT (mem),
      gst_va_buffer_surface_quark ());
  if (!buf)
    return TRUE;                /* free this unknown buffer */

  /* if this is the last reference to the GstVaBufferSurface, iterates
   * its array of memories to push them into the queue with thread
   * safetly. */
  GST_VA_MEMORY_POOL_LOCK (&self->pool);
  if (g_atomic_int_dec_and_test (&buf->ref_mems_count)) {
    for (i = 0; i < buf->n_mems; i++) {
      GST_LOG_OBJECT (self, "releasing %p: dmabuf %d, va surface %#x",
          buf->mems[i], gst_dmabuf_memory_get_fd (buf->mems[i]), buf->surface);
      gst_va_memory_pool_push (&self->pool, buf->mems[i]);
    }
  }
  GST_VA_MEMORY_POOL_UNLOCK (&self->pool);

  /* note: if ref_mem_count doesn't reach zero, that memory will
   * "float" until it's pushed back into the pool by the last va
   * buffer surface ref */

  /* Keep last in case we are holding on the last allocator ref */
  gst_object_unref (mem->allocator);

  /* don't call mini_object's free */
  return FALSE;
}

static gboolean
_modifier_found (guint64 modifier, guint64 * modifiers, guint num_modifiers)
{
  guint i;

  /* user doesn't care the returned modifier */
  if (num_modifiers == 0)
    return TRUE;

  for (i = 0; i < num_modifiers; i++)
    if (modifier == modifiers[i])
      return TRUE;
  return FALSE;
}

static void
_close_fds (VADRMPRIMESurfaceDescriptor * desc)
{
#ifndef G_OS_WIN32
  for (guint32 i = 0; i < desc->num_objects; i++) {
    gint fd = desc->objects[i].fd;
    close (fd);
  }
#endif
}

static gboolean
_va_create_surface_and_export_to_dmabuf (GstVaDisplay * display,
    guint usage_hint, guint64 * modifiers, guint num_modifiers,
    GstVideoInfo * info, VASurfaceID * ret_surface,
    VADRMPRIMESurfaceDescriptor * ret_desc)
{
  VADRMPRIMESurfaceDescriptor desc = { 0, };
  guint32 i, fourcc, rt_format, export_flags;
  GstVideoFormat format;
  VASurfaceID surface;
  guint64 prev_modifier = DRM_FORMAT_MOD_INVALID;

  _init_debug_category ();

  format = GST_VIDEO_INFO_FORMAT (info);

  fourcc = gst_va_fourcc_from_video_format (format);
  rt_format = gst_va_chroma_from_video_format (format);
  if (fourcc == 0 || rt_format == 0)
    return FALSE;

  if (!va_create_surfaces (display, rt_format, fourcc,
          GST_VIDEO_INFO_WIDTH (info), GST_VIDEO_INFO_HEIGHT (info),
          usage_hint, modifiers, num_modifiers, NULL, &surface, 1))
    return FALSE;

  /* workaround for missing layered dmabuf formats in i965 */
  if (GST_VA_DISPLAY_IS_IMPLEMENTATION (display, INTEL_I965)
      && (fourcc == VA_FOURCC_YUY2 || fourcc == VA_FOURCC_UYVY)) {
    /* These are not representable as separate planes */
    export_flags = VA_EXPORT_SURFACE_COMPOSED_LAYERS;
  } else {
    /* Each layer will contain exactly one plane.  For example, an NV12
     * surface will be exported as two layers */
    export_flags = VA_EXPORT_SURFACE_SEPARATE_LAYERS;
  }

  export_flags |= VA_EXPORT_SURFACE_READ_WRITE;

  if (!va_export_surface_to_dmabuf (display, surface, export_flags, &desc))
    goto failed;

  if (GST_VIDEO_INFO_N_PLANES (info) != desc.num_layers)
    goto failed;

  /* YUY2 and YUYV are the same. radeonsi returns always YUYV.
   * There's no reason to fail if the different fourcc if there're dups.
   * https://fourcc.org/pixel-format/yuv-yuy2/ */
  if (fourcc != desc.fourcc) {
    GST_INFO ("Different fourcc: requested %" GST_FOURCC_FORMAT " - returned %"
        GST_FOURCC_FORMAT, GST_FOURCC_ARGS (fourcc),
        GST_FOURCC_ARGS (desc.fourcc));
  }

  if (desc.num_objects == 0) {
    GST_ERROR ("Failed to export surface to dmabuf");
    goto failed;
  }

  for (i = 0; i < desc.num_objects; i++) {
    guint64 modifier = desc.objects[i].drm_format_modifier;

    if (!_modifier_found (modifier, modifiers, num_modifiers)) {
      GST_ERROR ("driver set a modifier different from allowed list: "
          "0x%016" G_GINT64_MODIFIER "x", modifier);
      goto failed;
    }
    /* XXX: all dmabufs in buffer have to have the same modifier, otherwise the
     * drm-format field in caps is ill-designed */
    if (i > 0 && modifier != prev_modifier) {
      GST_ERROR ("Different objects have different modifier");
      goto failed;
    }

    prev_modifier = modifier;
  }

  *ret_surface = surface;
  if (ret_desc)
    *ret_desc = desc;

  return TRUE;

failed:
  {
    /* Free DMAbufs on failure */
    _close_fds (&desc);
    va_destroy_surfaces (display, &surface, 1);
    return FALSE;
  }
}

/**
 * gst_va_dmabuf_get_modifier_for_format:
 * @display: a #GstVaDisplay
 * @format: a #GstVideoFormat
 * @usage_hint: VA usage hint
 *
 * Get the underlying modifier for specified @format and @usage_hint.
 *
 * Returns: the underlying modifier.
 *
 * Since: 1.24
 */
guint64
gst_va_dmabuf_get_modifier_for_format (GstVaDisplay * display,
    GstVideoFormat format, guint usage_hint)
{
  VADRMPRIMESurfaceDescriptor desc = { 0, };
  VASurfaceID surface;
  GstVideoInfo info;

  gst_video_info_init (&info);
  gst_video_info_set_format (&info, format, 64, 64);

  if (!_va_create_surface_and_export_to_dmabuf (display, usage_hint,
          NULL, 0, &info, &surface, &desc))
    return DRM_FORMAT_MOD_INVALID;

  /* Close the fds we won't be using */
  _close_fds (&desc);
  va_destroy_surfaces (display, &surface, 1);

  return desc.objects[0].drm_format_modifier;
}

/* Creates an exported VASurfaceID and adds it as @buffer's memories
 * qdata
 *
 * If @info is not NULL, a dummy (non-pooled) buffer is created to
 * update offsets and strides, and it has to be unrefed immediately.
 */
static gboolean
gst_va_dmabuf_allocator_setup_buffer_full (GstAllocator * allocator,
    GstBuffer * buffer, GstVideoInfoDmaDrm * info)
{
  GstVaBufferSurface *buf;
  GstVaDmabufAllocator *self = GST_VA_DMABUF_ALLOCATOR (allocator);
  VADRMPRIMESurfaceDescriptor desc = { 0, };
  VASurfaceID surface;
  guint32 i;
  GDestroyNotify buffer_destroy = NULL;
  gsize object_offset[4];

  g_return_val_if_fail (GST_IS_VA_DMABUF_ALLOCATOR (allocator), FALSE);

  if (!_va_create_surface_and_export_to_dmabuf (self->display, self->usage_hint,
          &self->info.drm_modifier, 1, &self->info.vinfo, &surface, &desc))
    return FALSE;

  buf = gst_va_buffer_surface_new (surface);
  if (G_UNLIKELY (info))
    *info = self->info;

  buf->n_mems = desc.num_objects;

  for (i = 0; i < desc.num_objects; i++) {
    gint fd = desc.objects[i].fd;
    /* prime descriptor reports the total size of the object, including regions
     * which aren't part surface's space. Let's just grab the surface's size: */
    gsize size = _get_fd_size (fd);
    GstMemory *mem = gst_dmabuf_allocator_alloc (allocator, fd, size);

    if (desc.objects[i].size < size) {
      GST_WARNING_OBJECT (self, "driver bug: fd size (%" G_GSIZE_FORMAT
          ") is bigger than object descriptor size (%" G_GUINT32_FORMAT ")",
          size, desc.objects[i].size);
    }

    object_offset[i] = gst_buffer_get_size (buffer);
    gst_buffer_append_memory (buffer, mem);
    buf->mems[i] = mem;

    if (G_LIKELY (!info)) {
      GST_MINI_OBJECT (mem)->dispose = gst_va_dmabuf_memory_release;
      g_atomic_int_add (&buf->ref_mems_count, 1);
    } else {
      /* if no @info, surface will be destroyed as soon as buffer is
       * destroyed (e.g. gst_va_dmabuf_allocator_try()) */
      buf->display = gst_object_ref (self->display);
      buffer_destroy = gst_va_buffer_surface_unref;
    }

    g_atomic_int_add (&buf->ref_count, 1);
    gst_mini_object_set_qdata (GST_MINI_OBJECT (mem),
        gst_va_buffer_surface_quark (), buf, buffer_destroy);

    if (G_UNLIKELY (info)) {
      GST_VIDEO_INFO_PLANE_OFFSET (&info->vinfo, i) =
          GST_VIDEO_INFO_SIZE (&info->vinfo);
    }

    GST_LOG_OBJECT (self, "buffer %p: new dmabuf %d / surface %#x [%dx%d] "
        "size %" G_GSIZE_FORMAT " drm mod %#" G_GINT64_MODIFIER "x",
        buffer, fd, surface, GST_VIDEO_INFO_WIDTH (&self->info.vinfo),
        GST_VIDEO_INFO_HEIGHT (&self->info.vinfo), size,
        self->info.drm_modifier);
  }

  if (G_UNLIKELY (info)) {
    if (desc.num_objects > 0) {
      /* update drm modifier and format */
      info->drm_modifier = desc.objects[0].drm_format_modifier;
      info->drm_fourcc = gst_va_drm_fourcc_from_video_format
          (GST_VIDEO_INFO_FORMAT (&self->info.vinfo));
    }

    GST_VIDEO_INFO_SIZE (&info->vinfo) = gst_buffer_get_size (buffer);

    for (i = 0; i < desc.num_layers; i++) {
      g_assert (desc.layers[i].num_planes == 1);
      GST_VIDEO_INFO_PLANE_OFFSET (&info->vinfo, i) =
          object_offset[desc.layers[i].object_index[0]] +
          desc.layers[i].offset[0];
      GST_VIDEO_INFO_PLANE_STRIDE (&info->vinfo, i) = desc.layers[i].pitch[0];
    }
  } else {
    gst_va_memory_pool_surface_inc (&self->pool);
  }

  return TRUE;
}

/**
 * gst_va_dmabuf_allocator_setup_buffer:
 * @allocator: a #GstAllocator
 * @buffer: an empty #GstBuffer
 *
 * This function creates a new VASurfaceID and exposes its DMABufs,
 * later it populates the @buffer with those DMABufs.
 *
 * Return: %TRUE if @buffer is populated correctly; %FALSE otherwise.
 *
 * Since: 1.22
 */
gboolean
gst_va_dmabuf_allocator_setup_buffer (GstAllocator * allocator,
    GstBuffer * buffer)
{
  return gst_va_dmabuf_allocator_setup_buffer_full (allocator, buffer, NULL);
}

static VASurfaceID
gst_va_dmabuf_allocator_prepare_buffer_unlocked (GstVaDmabufAllocator * self,
    GstBuffer * buffer)
{
  GstMemory *mems[GST_VIDEO_MAX_PLANES] = { 0, };
  GstVaBufferSurface *buf;
  gint i, j, idx;

  mems[0] = gst_va_memory_pool_pop (&self->pool);
  if (!mems[0])
    return VA_INVALID_ID;

  buf = gst_mini_object_get_qdata (GST_MINI_OBJECT (mems[0]),
      gst_va_buffer_surface_quark ());
  if (!buf)
    return VA_INVALID_ID;

  if (buf->surface == VA_INVALID_ID)
    return VA_INVALID_ID;

  for (idx = 1; idx < buf->n_mems; idx++) {
    /* grab next memory from queue */
    {
      GstMemory *mem;
      GstVaBufferSurface *pbuf;

      mem = gst_va_memory_pool_peek (&self->pool);
      if (!mem)
        return VA_INVALID_ID;

      pbuf = gst_mini_object_get_qdata (GST_MINI_OBJECT (mem),
          gst_va_buffer_surface_quark ());
      if (!pbuf)
        return VA_INVALID_ID;

      if (pbuf->surface != buf->surface) {
        GST_WARNING_OBJECT (self,
            "expecting memory with surface %#x but got %#x: "
            "possible memory interweaving", buf->surface, pbuf->surface);
        return VA_INVALID_ID;
      }
    }

    mems[idx] = gst_va_memory_pool_pop (&self->pool);
  };

  /* append memories */
  for (i = 0; i < buf->n_mems; i++) {
    gboolean found = FALSE;

    /* find next memory to append */
    for (j = 0; j < idx; j++) {
      if (buf->mems[i] == mems[j]) {
        found = TRUE;
        break;
      }
    }

    /* if not found, free all the popped memories and bail */
    if (!found) {
      if (!buf->display)
        buf->display = gst_object_ref (self->display);
      for (j = 0; j < idx; j++) {
        gst_object_ref (buf->mems[j]->allocator);
        GST_MINI_OBJECT (mems[j])->dispose = NULL;
        gst_memory_unref (mems[j]);
      }
      return VA_INVALID_ID;
    }

    g_atomic_int_add (&buf->ref_mems_count, 1);
    gst_object_ref (buf->mems[i]->allocator);
    gst_buffer_append_memory (buffer, buf->mems[i]);

    GST_LOG ("bufer %p: memory %p - dmabuf %d / surface %#x", buffer,
        buf->mems[i], gst_dmabuf_memory_get_fd (buf->mems[i]),
        gst_va_memory_get_surface (buf->mems[i]));
  }

  return buf->surface;
}

/**
 * gst_va_dmabuf_allocator_prepare_buffer:
 * @allocator: a #GstAllocator
 * @buffer: an empty #GstBuffer
 *
 * This method will populate @buffer with pooled VASurfaceID/DMABuf
 * memories. It doesn't allocate new VASurfacesID.
 *
 * Returns: %TRUE if @buffer was populated correctly; %FALSE
 * otherwise.
 *
 * Since: 1.22
 */
gboolean
gst_va_dmabuf_allocator_prepare_buffer (GstAllocator * allocator,
    GstBuffer * buffer)
{
  GstVaDmabufAllocator *self;
  VASurfaceID surface;

  g_return_val_if_fail (GST_IS_VA_DMABUF_ALLOCATOR (allocator), FALSE);

  self = GST_VA_DMABUF_ALLOCATOR (allocator);

  GST_VA_MEMORY_POOL_LOCK (&self->pool);
  surface = gst_va_dmabuf_allocator_prepare_buffer_unlocked (self, buffer);
  GST_VA_MEMORY_POOL_UNLOCK (&self->pool);

  return (surface != VA_INVALID_ID);
}

/**
 * gst_va_dmabuf_allocator_flush:
 * @allocator: a #GstAllocator
 *
 * Removes all the memories in @allocator's pool.
 *
 * Since: 1.22
 */
void
gst_va_dmabuf_allocator_flush (GstAllocator * allocator)
{
  GstVaDmabufAllocator *self;

  g_return_if_fail (GST_IS_VA_DMABUF_ALLOCATOR (allocator));

  self = GST_VA_DMABUF_ALLOCATOR (allocator);

  gst_va_memory_pool_flush (&self->pool, self->display);
}

/**
 * gst_va_dmabuf_allocator_try:
 * @allocator: a #GstAllocator
 *
 * Try to allocate a test buffer in order to verify that the
 * allocator's configuration is valid.
 *
 * Returns: %TRUE if the configuration is valid; %FALSE otherwise.
 *
 * Since: 1.22
 */
static gboolean
gst_va_dmabuf_allocator_try (GstAllocator * allocator)
{
  GstBuffer *buffer;
  GstVaDmabufAllocator *self;
  GstVideoInfoDmaDrm info;
  gboolean ret;

  g_return_val_if_fail (GST_IS_VA_DMABUF_ALLOCATOR (allocator), FALSE);

  self = GST_VA_DMABUF_ALLOCATOR (allocator);
  info = self->info;

  buffer = gst_buffer_new ();
  ret = gst_va_dmabuf_allocator_setup_buffer_full (allocator, buffer, &info);
  gst_buffer_unref (buffer);

  if (ret)
    self->info = info;

  return ret;
}

/**
 * gst_va_dmabuf_allocator_set_format:
 * @allocator: a #GstAllocator
 * @info: (in) (out caller-allocates) (not nullable): a #GstVideoInfoDmaDrm
 * @usage_hint: VA usage hint
 *
 * Sets the configuration defined by @info and @usage_hint for
 * @allocator, and it tries the configuration, if @allocator has not
 * allocated memories yet.
 *
 * If @allocator has memory allocated already, and frame size, format
 * and modifier in @info are the same as currently configured in
 * @allocator, the rest of @info parameters are updated internally.
 *
 * Returns: %TRUE if the configuration is valid or updated; %FALSE if
 * configuration is not valid or not updated.
 *
 * Since: 1.22
 */
gboolean
gst_va_dmabuf_allocator_set_format (GstAllocator * allocator,
    GstVideoInfoDmaDrm * info, guint usage_hint)
{
  GstVaDmabufAllocator *self;
  gboolean ret;

  g_return_val_if_fail (GST_IS_VA_DMABUF_ALLOCATOR (allocator), FALSE);
  g_return_val_if_fail (info, FALSE);

  self = GST_VA_DMABUF_ALLOCATOR (allocator);

  if (gst_va_memory_pool_surface_count (&self->pool) != 0) {
    if (info->drm_modifier == self->info.drm_modifier
        && GST_VIDEO_INFO_FORMAT (&info->vinfo)
        == GST_VIDEO_INFO_FORMAT (&self->info.vinfo)
        && GST_VIDEO_INFO_WIDTH (&info->vinfo)
        == GST_VIDEO_INFO_WIDTH (&self->info.vinfo)
        && GST_VIDEO_INFO_HEIGHT (&info->vinfo)
        == GST_VIDEO_INFO_HEIGHT (&self->info.vinfo)
        && usage_hint == self->usage_hint) {
      *info = self->info;       /* update callee info (offset & stride) */
      return TRUE;
    }
    return FALSE;
  }

  self->usage_hint = usage_hint;
  self->info = *info;

  g_clear_pointer (&self->copy, gst_va_surface_copy_free);

  ret = gst_va_dmabuf_allocator_try (allocator);

  if (ret)
    *info = self->info;

  return ret;
}

/**
 * gst_va_dmabuf_allocator_get_format:
 * @allocator: a #GstAllocator
 * @info: (out) (optional): a #GstVideoInfoDmaDrm
 * @usage_hint: (out) (optional): VA usage hint
 *
 * Gets current internal configuration of @allocator.
 *
 * Returns: %TRUE if @allocator is already configured; %FALSE
 * otherwise.
 *
 * Since: 1.22
 */
gboolean
gst_va_dmabuf_allocator_get_format (GstAllocator * allocator,
    GstVideoInfoDmaDrm * info, guint * usage_hint)
{
  GstVaDmabufAllocator *self = GST_VA_DMABUF_ALLOCATOR (allocator);

  if (GST_VIDEO_INFO_FORMAT (&self->info.vinfo) == GST_VIDEO_FORMAT_UNKNOWN)
    return FALSE;

  if (info)
    *info = self->info;
  if (usage_hint)
    *usage_hint = self->usage_hint;

  return TRUE;
}

static gboolean
_is_fd_repeated (uintptr_t fds[GST_VIDEO_MAX_PLANES], guint cur, guint * prev)
{
  guint i;

  g_assert (cur <= GST_VIDEO_MAX_PLANES);

  if (cur == 0)
    return FALSE;

  for (i = 0; i < cur; i++) {
    if (fds[i] == fds[cur]) {
      if (prev)
        *prev = i;
      return TRUE;
    }
  }
  return FALSE;
}

/**
 * gst_va_dmabuf_memories_setup:
 * @display: a #GstVaDisplay
 * @drm_info: a #GstVideoInfoDmaDrm
 * @mem: (array fixed-size=4) (element-type GstMemory): Memories. One
 *     per plane.
 * @fds: (array fixed-size=4) (element-type uintptr_t): array of
 *     DMABuf file descriptors.
 * @offset: (array fixed-size=4) (element-type gsize): array of memory
 *     offsets.
 * @usage_hint: VA usage hint.
 *
 * It imports the array of @mem, representing a single frame, into a
 * VASurfaceID and it's attached into every @mem.
 *
 * Returns: %TRUE if frame is imported correctly into a VASurfaceID;
 * %FALSE otherwise.
 *
 * Since: 1.22
 */
/* XXX: use a surface pool to control the created surfaces */
gboolean
gst_va_dmabuf_memories_setup (GstVaDisplay * display,
    GstVideoInfoDmaDrm * drm_info, GstMemory * mem[GST_VIDEO_MAX_PLANES],
    uintptr_t fds[GST_VIDEO_MAX_PLANES], gsize offset[GST_VIDEO_MAX_PLANES],
    guint usage_hint)
{
  GstVideoFormat format;
  GstVaBufferSurface *buf;
  GstVideoInfo *info = &drm_info->vinfo;
  /* *INDENT-OFF* */
  VADRMPRIMESurfaceDescriptor desc = {
    .width = GST_VIDEO_INFO_WIDTH (info),
    .height = GST_VIDEO_INFO_HEIGHT (info),
    /* GStreamer can only describe one PRIME layer */
    .num_layers = 1,
  };
  /* *INDENT-ON* */
  VASurfaceID surface;
  guint32 fourcc, rt_format, drm_fourcc;
  guint64 drm_modifier;
  guint i, j, prev, n_planes;
  gboolean ret;

  g_return_val_if_fail (GST_IS_VA_DISPLAY (display), FALSE);

  n_planes = GST_VIDEO_INFO_N_PLANES (info);

  format = (drm_info->drm_fourcc == DRM_FORMAT_INVALID) ?
      GST_VIDEO_INFO_FORMAT (info) :
      gst_va_video_format_from_drm_fourcc (drm_info->drm_fourcc);
  if (format == GST_VIDEO_FORMAT_UNKNOWN)
    return FALSE;

  drm_fourcc = (drm_info->drm_fourcc == DRM_FORMAT_INVALID) ?
      gst_va_drm_fourcc_from_video_format (format) : drm_info->drm_fourcc;
  if (drm_fourcc == 0)
    return FALSE;

  rt_format = gst_va_chroma_from_video_format (format);
  if (rt_format == 0)
    return FALSE;

  fourcc = gst_va_fourcc_from_video_format (format);
  if (fourcc == 0)
    return FALSE;

  drm_modifier = (drm_info->drm_modifier == DRM_FORMAT_MOD_INVALID) ?
      DRM_FORMAT_MOD_LINEAR : drm_info->drm_modifier;

  desc.fourcc = fourcc;
  desc.layers[0].num_planes = n_planes;
  desc.layers[0].drm_format = drm_fourcc;

  for (i = j = 0; i < n_planes; i++) {
    desc.layers[0].pitch[i] = GST_VIDEO_INFO_PLANE_STRIDE (info, i);
    desc.layers[0].offset[i] = offset[i];

    if (_is_fd_repeated (fds, i, &prev)) {
      desc.layers[0].object_index[i] = prev;
      continue;
    }

    desc.objects[j].fd = fds[i];
    desc.objects[j].size = _get_fd_size (fds[i]);
    desc.objects[j].drm_format_modifier = drm_modifier;

    desc.layers[0].object_index[i] = j;
    j++;
  }

  desc.num_objects = j;

  ret = va_create_surfaces (display, rt_format, fourcc, desc.width, desc.height,
      usage_hint, NULL, 0, &desc, &surface, 1);
  if (!ret)
    return FALSE;

  GST_LOG_OBJECT (display, "Created surface %#x [%dx%d]", surface, desc.width,
      desc.height);

  buf = gst_va_buffer_surface_new (surface);
  buf->display = gst_object_ref (display);
  buf->n_mems = n_planes;
  memcpy (buf->mems, mem, sizeof (buf->mems));

  for (i = 0; i < n_planes; i++) {
    g_atomic_int_add (&buf->ref_count, 1);
    gst_mini_object_set_qdata (GST_MINI_OBJECT (mem[i]),
        gst_va_buffer_surface_quark (), buf, gst_va_buffer_surface_unref);
    GST_INFO_OBJECT (display, "setting surface %#x to dmabuf fd %d",
        buf->surface, gst_dmabuf_memory_get_fd (mem[i]));
  }

  return TRUE;
}

/*===================== GstVaAllocator / GstVaMemory =========================*/

/**
 * GstVaAllocator:
 *
 * A pooled memory allocator backed by VASurfaceID.
 *
 * Since: 1.22
 */
typedef struct _GstVaAllocator GstVaAllocator;
typedef struct _GstVaAllocatorClass GstVaAllocatorClass;

struct _GstVaAllocator
{
  GstAllocator parent;

  GstVaDisplay *display;

  gboolean use_derived;
  GArray *surface_formats;

  GstVideoFormat surface_format;
  GstVideoFormat img_format;
  guint32 fourcc;
  guint32 rt_format;

  GstVideoInfo info;
  guint usage_hint;

  guint32 hacks;

  GstVaSurfaceCopy *copy;

  GstVaMemoryPool pool;
};

struct _GstVaAllocatorClass
{
  GstAllocatorClass parent_class;
};

typedef struct _GstVaMemory GstVaMemory;
struct _GstVaMemory
{
  GstMemory mem;

  VASurfaceID surface;
  GstVideoFormat surface_format;
  VAImage image;
  gpointer mapped_data;

  GstMapFlags prev_mapflags;
  gint map_count;

  gboolean is_derived;
  gboolean is_dirty;
  GMutex lock;
};

G_DEFINE_TYPE_WITH_CODE (GstVaAllocator, gst_va_allocator, GST_TYPE_ALLOCATOR,
    _init_debug_category ());

static gboolean _va_unmap (GstVaMemory * mem);

static void
gst_va_allocator_finalize (GObject * object)
{
  GstVaAllocator *self = GST_VA_ALLOCATOR (object);

  g_clear_pointer (&self->copy, gst_va_surface_copy_free);
  gst_va_memory_pool_finalize (&self->pool);
  g_clear_pointer (&self->surface_formats, g_array_unref);
  gst_clear_object (&self->display);

  G_OBJECT_CLASS (gst_va_allocator_parent_class)->finalize (object);
}

static void
gst_va_allocator_dispose (GObject * object)
{
  GstVaAllocator *self = GST_VA_ALLOCATOR (object);

  gst_va_memory_pool_flush_unlocked (&self->pool, self->display);
  if (gst_va_memory_pool_surface_count (&self->pool) != 0) {
    GST_WARNING_OBJECT (self, "Surfaces leaked: %d",
        gst_va_memory_pool_surface_count (&self->pool));
  }

  G_OBJECT_CLASS (gst_va_allocator_parent_class)->dispose (object);
}

static void
_va_free (GstAllocator * allocator, GstMemory * mem)
{
  GstVaAllocator *self = GST_VA_ALLOCATOR (allocator);
  GstVaMemory *va_mem = (GstVaMemory *) mem;

  if (va_mem->mapped_data) {
    g_warning (G_STRLOC ":%s: Freeing memory %p still mapped", G_STRFUNC,
        va_mem);
    _va_unmap (va_mem);
  }

  if (va_mem->surface != VA_INVALID_ID && mem->parent == NULL) {
    GST_LOG_OBJECT (self, "Destroying surface %#x", va_mem->surface);
    va_destroy_surfaces (self->display, &va_mem->surface, 1);
  }

  g_mutex_clear (&va_mem->lock);

  g_free (va_mem);
}

static void
gst_va_allocator_class_init (GstVaAllocatorClass * klass)
{
  GstAllocatorClass *allocator_class = GST_ALLOCATOR_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gst_va_allocator_dispose;
  object_class->finalize = gst_va_allocator_finalize;
  allocator_class->free = _va_free;
}

static inline void
_clean_mem (GstVaMemory * mem)
{
  memset (&mem->image, 0, sizeof (mem->image));
  mem->image.image_id = VA_INVALID_ID;
  mem->image.buf = VA_INVALID_ID;

  mem->is_derived = FALSE;
  mem->is_dirty = FALSE;
  mem->prev_mapflags = 0;
  mem->mapped_data = NULL;
}

static void
_reset_mem (GstVaMemory * mem, GstAllocator * allocator, gsize size)
{
  _clean_mem (mem);
  g_atomic_int_set (&mem->map_count, 0);
  g_mutex_init (&mem->lock);

  gst_memory_init (GST_MEMORY_CAST (mem), 0, allocator, NULL, size,
      0 /* align */ , 0 /* offset */ , size);
}

#ifndef G_OS_WIN32
static inline gboolean
_is_old_mesa (GstVaAllocator * va_allocator)
{
  return GST_VA_DISPLAY_IS_IMPLEMENTATION (va_allocator->display, MESA_GALLIUM)
      && !gst_va_display_check_version (va_allocator->display, 23, 3);
}
#endif /* G_OS_WIN32 */

static inline void
_update_info (GstVideoInfo * info, const VAImage * image)
{
  guint i;

  for (i = 0; i < image->num_planes; i++) {
    GST_VIDEO_INFO_PLANE_OFFSET (info, i) = image->offsets[i];
    GST_VIDEO_INFO_PLANE_STRIDE (info, i) = image->pitches[i];
  }

  /* Don't update image size for one planed images since drivers might add extra
   * bits which will drop wrong raw images with filesink, for example. Multiple
   * plane images require video meta */
  if (image->num_planes > 1)
    GST_VIDEO_INFO_SIZE (info) = image->data_size;
}

static inline gboolean
_update_image_info (GstVaAllocator * va_allocator,
    GstVaFeature feat_use_derived)
{
  VASurfaceID surface;
  VAImage image = {.image_id = VA_INVALID_ID, };

  /* Create a test surface first */
  if (!va_create_surfaces (va_allocator->display, va_allocator->rt_format,
          va_allocator->fourcc, GST_VIDEO_INFO_WIDTH (&va_allocator->info),
          GST_VIDEO_INFO_HEIGHT (&va_allocator->info), va_allocator->usage_hint,
          NULL, 0, NULL, &surface, 1)) {
    GST_ERROR_OBJECT (va_allocator, "Failed to create a test surface");
    return FALSE;
  }

  GST_DEBUG_OBJECT (va_allocator, "Created surface %#x [%dx%d]", surface,
      GST_VIDEO_INFO_WIDTH (&va_allocator->info),
      GST_VIDEO_INFO_HEIGHT (&va_allocator->info));

#ifdef G_OS_WIN32
  /* XXX: Derived image is problematic for D3D backend */
  if (feat_use_derived != GST_VA_FEATURE_DISABLED) {
    GST_INFO_OBJECT (va_allocator, "Disable image derive on Windows.");
    feat_use_derived = GST_VA_FEATURE_DISABLED;
  }
  va_allocator->use_derived = FALSE;
#else
  /* XXX: Derived in radeonsi Mesa <23.3 can't use derived images for several
   * cases
   * https://gitlab.freedesktop.org/mesa/mesa/-/merge_requests/24381
   * https://gitlab.freedesktop.org/mesa/mesa/-/merge_requests/26174
   */
  if (_is_old_mesa (va_allocator)) {
    if (feat_use_derived != GST_VA_FEATURE_DISABLED) {
      GST_INFO_OBJECT (va_allocator,
          "Disable image derive on old Mesa (< 23.3).");
      feat_use_derived = GST_VA_FEATURE_DISABLED;
    }
    va_allocator->use_derived = FALSE;
  }
#endif
  /* Try derived first, but different formats can never derive */
  if (feat_use_derived != GST_VA_FEATURE_DISABLED
      && va_allocator->surface_format == va_allocator->img_format) {
    va_allocator->use_derived =
        va_get_derive_image (va_allocator->display, surface, &image);
    if (va_allocator->use_derived)
      goto done;
    image.image_id = VA_INVALID_ID;     /* reset it */
  }

  if (feat_use_derived == GST_VA_FEATURE_ENABLED && !va_allocator->use_derived)
    GST_WARNING_OBJECT (va_allocator, "Derived images are disabled.");

  /* Then we try to create a image. */
  if (!va_create_image (va_allocator->display, va_allocator->img_format,
          GST_VIDEO_INFO_WIDTH (&va_allocator->info),
          GST_VIDEO_INFO_HEIGHT (&va_allocator->info), &image)) {
    va_destroy_surfaces (va_allocator->display, &surface, 1);
    return FALSE;
  }

done:
  _update_info (&va_allocator->info, &image);
  if (GST_VIDEO_INFO_SIZE (&va_allocator->info) > image.data_size) {
    GST_WARNING_OBJECT (va_allocator,
        "image size is lesser than the minimum required");
  }

  va_destroy_image (va_allocator->display, image.image_id);
  va_destroy_surfaces (va_allocator->display, &surface, 1);

  return TRUE;
}

static gpointer
_va_map_unlocked (GstVaMemory * mem, GstMapFlags flags)
{
  GstAllocator *allocator = GST_MEMORY_CAST (mem)->allocator;
  GstVaAllocator *va_allocator;
  GstVaDisplay *display;

  g_return_val_if_fail (mem->surface != VA_INVALID_ID, NULL);
  g_return_val_if_fail (GST_IS_VA_ALLOCATOR (allocator), NULL);

  if (g_atomic_int_get (&mem->map_count) > 0) {
    if (!(mem->prev_mapflags & flags) || !mem->mapped_data)
      return NULL;
    else
      goto success;
  }

  va_allocator = GST_VA_ALLOCATOR (allocator);
  display = va_allocator->display;

  if (flags & GST_MAP_WRITE) {
    mem->is_dirty = TRUE;
  } else {                      /* GST_MAP_READ only */
    mem->is_dirty = FALSE;
  }

  if (flags & GST_MAP_VA) {
    mem->mapped_data = &mem->surface;
    goto success;
  }

  mem->is_derived = va_allocator->use_derived;

  if (!va_ensure_image (display, mem->surface, &va_allocator->info, &mem->image,
          va_allocator->use_derived))
    return NULL;

  if (!mem->is_derived) {
    if (!va_get_image (display, mem->surface, &mem->image))
      goto fail;
  }

  if (!va_map_buffer (display, mem->image.buf, flags, &mem->mapped_data))
    goto fail;

success:
  {
    mem->prev_mapflags = flags;
    g_atomic_int_add (&mem->map_count, 1);
    return mem->mapped_data;
  }

fail:
  {
    va_destroy_image (display, mem->image.image_id);
    _clean_mem (mem);
    return NULL;
  }
}

static gpointer
_va_map (GstVaMemory * mem, gsize maxsize, GstMapFlags flags)
{
  gpointer data;

  g_mutex_lock (&mem->lock);
  data = _va_map_unlocked (mem, flags);
  g_mutex_unlock (&mem->lock);

  return data;
}

static gboolean
_va_unmap_unlocked (GstVaMemory * mem)
{
  GstAllocator *allocator = GST_MEMORY_CAST (mem)->allocator;
  GstVaDisplay *display;
  gboolean ret = TRUE;

  if (!g_atomic_int_dec_and_test (&mem->map_count))
    return TRUE;

  if (mem->prev_mapflags & GST_MAP_VA)
    goto bail;

  display = GST_VA_ALLOCATOR (allocator)->display;

  if (mem->image.image_id != VA_INVALID_ID) {
    if (mem->is_dirty && !mem->is_derived) {
      ret = va_put_image (display, mem->surface, &mem->image);
      mem->is_dirty = FALSE;
    }
    /* XXX(victor): if is derived and is dirty, create another surface
     * an replace it in mem */
  }

  ret &= va_unmap_buffer (display, mem->image.buf);
  ret &= va_destroy_image (display, mem->image.image_id);

bail:
  _clean_mem (mem);

  return ret;
}

static gboolean
_va_unmap (GstVaMemory * mem)
{
  gboolean ret;

  g_mutex_lock (&mem->lock);
  ret = _va_unmap_unlocked (mem);
  g_mutex_unlock (&mem->lock);

  return ret;
}

static GstMemory *
_va_share (GstMemory * mem, gssize offset, gssize size)
{
  GstVaMemory *vamem = (GstVaMemory *) mem;
  GstVaMemory *sub;
  GstMemory *parent;

  GST_DEBUG ("%p: share %" G_GSSIZE_FORMAT ", %" G_GSIZE_FORMAT, mem, offset,
      size);

  /* find real parent */
  if ((parent = vamem->mem.parent) == NULL)
    parent = (GstMemory *) vamem;

  if (size == -1)
    size = mem->maxsize - offset;

  sub = g_new (GstVaMemory, 1);

  /* the shared memory is alwyas readonly */
  gst_memory_init (GST_MEMORY_CAST (sub), GST_MINI_OBJECT_FLAGS (parent) |
      GST_MINI_OBJECT_FLAG_LOCK_READONLY, vamem->mem.allocator, parent,
      vamem->mem.maxsize, vamem->mem.align, vamem->mem.offset + offset, size);

  sub->surface = vamem->surface;
  sub->surface_format = vamem->surface_format;

  _clean_mem (sub);

  g_atomic_int_set (&sub->map_count, 0);
  g_mutex_init (&sub->lock);

  return GST_MEMORY_CAST (sub);
}

/* XXX(victor): deep copy implementation. */
static GstMemory *
_va_copy (GstMemory * mem, gssize offset, gssize size)
{
  GstMemory *copy;
  GstMapInfo sinfo, dinfo;
  GstVaAllocator *va_allocator = GST_VA_ALLOCATOR (mem->allocator);
  GstVaMemory *va_copy, *va_mem = (GstVaMemory *) mem;
  gsize mem_size;

  GST_DEBUG ("%p: copy %" G_GSSIZE_FORMAT ", %" G_GSIZE_FORMAT, mem, offset,
      size);

  {
    GST_VA_MEMORY_POOL_LOCK (&va_allocator->pool);
    copy = gst_va_memory_pool_pop (&va_allocator->pool);
    GST_VA_MEMORY_POOL_UNLOCK (&va_allocator->pool);

    if (!copy) {
      copy = gst_va_allocator_alloc (mem->allocator);
      if (!copy) {
        GST_WARNING ("failed to allocate new memory");
        return NULL;
      }
    } else {
      gst_object_ref (mem->allocator);
    }
  }

  va_copy = (GstVaMemory *) copy;
  mem_size = gst_memory_get_sizes (mem, NULL, NULL);

  if (size == -1)
    size = mem_size > offset ? mem_size - offset : 0;

  if (offset == 0 && size == mem_size) {
    GstVaSurfaceCopy *copy_func;

    copy_func = _ensure_surface_copy (&va_allocator->copy,
        va_allocator->display, &va_allocator->info);
    if (copy_func
        && gst_va_surface_copy (copy_func, va_copy->surface, va_mem->surface))
      return copy;
  }

  if (!gst_memory_map (mem, &sinfo, GST_MAP_READ)) {
    GST_WARNING ("failed to map memory to copy");
    return NULL;
  }

  if (!gst_memory_map (copy, &dinfo, GST_MAP_WRITE)) {
    GST_WARNING ("could not write map memory %p", copy);
    gst_allocator_free (mem->allocator, copy);
    gst_memory_unmap (mem, &sinfo);
    return NULL;
  }

  memcpy (dinfo.data, sinfo.data + offset, size);
  gst_memory_unmap (copy, &dinfo);
  gst_memory_unmap (mem, &sinfo);

  return copy;
}

static void
gst_va_allocator_init (GstVaAllocator * self)
{
  GstAllocator *allocator = GST_ALLOCATOR (self);

  allocator->mem_type = GST_ALLOCATOR_VASURFACE;
  allocator->mem_map = (GstMemoryMapFunction) _va_map;
  allocator->mem_unmap = (GstMemoryUnmapFunction) _va_unmap;
  allocator->mem_share = _va_share;
  allocator->mem_copy = _va_copy;

  gst_va_memory_pool_init (&self->pool);

  GST_OBJECT_FLAG_SET (self, GST_ALLOCATOR_FLAG_CUSTOM_ALLOC);
}

static gboolean
gst_va_memory_release (GstMiniObject * mini_object)
{
  GstMemory *mem = GST_MEMORY_CAST (mini_object);
  GstVaAllocator *self = GST_VA_ALLOCATOR (mem->allocator);

  GST_LOG ("releasing %p: surface %#x", mem, gst_va_memory_get_surface (mem));

  gst_va_memory_pool_push (&self->pool, mem);

  /* Keep last in case we are holding on the last allocator ref */
  gst_object_unref (mem->allocator);

  /* don't call mini_object's free */
  return FALSE;
}

/**
 * gst_va_allocator_alloc:
 * @allocator: a #GstAllocator
 *
 * Allocate a new VASurfaceID backed #GstMemory.
 *
 * Returns: a #GstMemory backed with a VASurfaceID; %NULL, otherwise.
 *
 * Since: 1.22
 */
GstMemory *
gst_va_allocator_alloc (GstAllocator * allocator)
{
  GstVaAllocator *self;
  GstVaMemory *mem;
  VASurfaceID surface;

  g_return_val_if_fail (GST_IS_VA_ALLOCATOR (allocator), NULL);

  self = GST_VA_ALLOCATOR (allocator);

  if (self->rt_format == 0) {
    GST_ERROR_OBJECT (self, "Unknown fourcc or chroma format");
    return NULL;
  }

  if (!va_create_surfaces (self->display, self->rt_format, self->fourcc,
          GST_VIDEO_INFO_WIDTH (&self->info),
          GST_VIDEO_INFO_HEIGHT (&self->info), self->usage_hint, NULL, 0, NULL,
          &surface, 1))
    return NULL;

  mem = g_new (GstVaMemory, 1);

  mem->surface = surface;
  mem->surface_format = self->surface_format;

  _reset_mem (mem, allocator, GST_VIDEO_INFO_SIZE (&self->info));

  GST_MINI_OBJECT (mem)->dispose = gst_va_memory_release;
  gst_va_memory_pool_surface_inc (&self->pool);

  GST_LOG_OBJECT (self, "Created surface %#x [%dx%d]", mem->surface,
      GST_VIDEO_INFO_WIDTH (&self->info), GST_VIDEO_INFO_HEIGHT (&self->info));

  return GST_MEMORY_CAST (mem);
}

/**
 * gst_va_allocator_new:
 * @display: a #GstVaDisplay
 * @surface_formats: (element-type guint) (transfer full): a #GArray
 *     of valid #GstVideoFormat for surfaces in current VA context.
 *
 * Instanciate a new pooled #GstAllocator backed by VASurfaceID.
 *
 * Returns: a #GstVaDisplay
 *
 * Since: 1.22
 */
GstAllocator *
gst_va_allocator_new (GstVaDisplay * display, GArray * surface_formats)
{
  GstVaAllocator *self;

  g_return_val_if_fail (GST_IS_VA_DISPLAY (display), NULL);

  self = g_object_new (GST_TYPE_VA_ALLOCATOR, NULL);
  self->display = gst_object_ref (display);
  self->surface_formats = surface_formats;
  gst_object_ref_sink (self);

  return GST_ALLOCATOR (self);
}

/**
 * gst_va_allocator_setup_buffer:
 * @allocator: a #GstAllocator
 * @buffer: a #GstBuffer
 *
 * Populates an empty @buffer with a VASuface backed #GstMemory.
 *
 * Returns: %TRUE if @buffer is populated; %FALSE otherwise.
 *
 * Since: 1.22
 */
gboolean
gst_va_allocator_setup_buffer (GstAllocator * allocator, GstBuffer * buffer)
{
  GstMemory *mem = gst_va_allocator_alloc (allocator);
  if (!mem)
    return FALSE;

  gst_buffer_append_memory (buffer, mem);
  return TRUE;
}

static VASurfaceID
gst_va_allocator_prepare_buffer_unlocked (GstVaAllocator * self,
    GstBuffer * buffer)
{
  GstMemory *mem;
  VASurfaceID surface;

  mem = gst_va_memory_pool_pop (&self->pool);
  if (!mem)
    return VA_INVALID_ID;

  gst_object_ref (mem->allocator);
  surface = gst_va_memory_get_surface (mem);
  gst_buffer_append_memory (buffer, mem);

  GST_LOG ("buffer %p: memory %p - surface %#x", buffer, mem, surface);

  return surface;
}

/**
 * gst_va_allocator_prepare_buffer:
 * @allocator: a #GstAllocator
 * @buffer: an empty #GstBuffer
 *
 * This method will populate @buffer with pooled VASurfaceID
 * memories. It doesn't allocate new VASurfacesID.
 *
 * Returns: %TRUE if @buffer was populated correctly; %FALSE
 * otherwise.
 *
 * Since: 1.22
 */
gboolean
gst_va_allocator_prepare_buffer (GstAllocator * allocator, GstBuffer * buffer)
{
  GstVaAllocator *self;
  VASurfaceID surface;

  g_return_val_if_fail (GST_IS_VA_ALLOCATOR (allocator), FALSE);

  self = GST_VA_ALLOCATOR (allocator);

  GST_VA_MEMORY_POOL_LOCK (&self->pool);
  surface = gst_va_allocator_prepare_buffer_unlocked (self, buffer);
  GST_VA_MEMORY_POOL_UNLOCK (&self->pool);

  return (surface != VA_INVALID_ID);
}

/**
 * gst_va_allocator_flush:
 * @allocator: a #GstAllocator
 *
 * Removes all the memories in @allocator's pool.
 *
 * Since: 1.22
 */
void
gst_va_allocator_flush (GstAllocator * allocator)
{
  GstVaAllocator *self;

  g_return_if_fail (GST_IS_VA_ALLOCATOR (allocator));

  self = GST_VA_ALLOCATOR (allocator);

  gst_va_memory_pool_flush (&self->pool, self->display);
}

/**
 * gst_va_allocator_try:
 * @allocator: a #GstAllocator
 *
 * Try to allocate a test buffer in order to verify that the
 * allocator's configuration is valid.
 *
 * Returns: %TRUE if the configuration is valid; %FALSE otherwise.
 *
 * Since: 1.22
 */
static gboolean
gst_va_allocator_try (GstAllocator * allocator, GstVaFeature feat_use_derived)
{
  GstVaAllocator *self;

  g_return_val_if_fail (GST_IS_VA_ALLOCATOR (allocator), FALSE);

  self = GST_VA_ALLOCATOR (allocator);

  self->fourcc = 0;
  self->rt_format = 0;
  self->use_derived = FALSE;
  self->img_format = GST_VIDEO_INFO_FORMAT (&self->info);

  self->surface_format =
      gst_va_video_surface_format_from_image_format (self->img_format,
      self->surface_formats);
  if (self->surface_format == GST_VIDEO_FORMAT_UNKNOWN) {
    /* try a surface without fourcc but rt_format only */
    self->fourcc = 0;
    self->rt_format = gst_va_chroma_from_video_format (self->img_format);
  } else {
    if (G_LIKELY (!(self->hacks & GST_VA_HACK_SURFACE_NO_FOURCC)))
      self->fourcc = gst_va_fourcc_from_video_format (self->surface_format);
    self->rt_format = gst_va_chroma_from_video_format (self->surface_format);
  }

  if (self->rt_format == 0) {
    GST_ERROR_OBJECT (allocator, "Unsupported image format: %s",
        gst_video_format_to_string (self->img_format));
    return FALSE;
  }

  if (!_update_image_info (self, feat_use_derived)) {
    GST_ERROR_OBJECT (allocator, "Failed to update allocator info");
    return FALSE;
  }

  GST_INFO_OBJECT (self,
      "va allocator info, surface format: %s, image format: %s, "
      "use derived: %s, rt format: 0x%x, fourcc: %" GST_FOURCC_FORMAT,
      (self->surface_format == GST_VIDEO_FORMAT_UNKNOWN) ? "unknown"
      : gst_video_format_to_string (self->surface_format),
      gst_video_format_to_string (self->img_format),
      self->use_derived ? "true" : "false", self->rt_format,
      GST_FOURCC_ARGS (self->fourcc));
  return TRUE;
}

/**
 * gst_va_allocator_set_format:
 * @allocator: a #GstAllocator
 * @info: (inout): a #GstVideoInfo
 * @usage_hint: VA usage hint
 * @feat_use_derived: a #GstVaFeature
 *
 * Sets the configuration defined by @info, @usage_hint and
 * @use_derived for @allocator, and it tries the configuration, if
 * @allocator has not allocated memories yet.
 *
 * If @allocator has memory allocated already, and frame size and
 * format in @info are the same as currently configured in @allocator,
 * the rest of @info parameters are updated internally.
 *
 * Returns: %TRUE if the configuration is valid or updated; %FALSE if
 * configuration is not valid or not updated.
 *
 * Since: 1.22
 */
gboolean
gst_va_allocator_set_format (GstAllocator * allocator, GstVideoInfo * info,
    guint usage_hint, GstVaFeature feat_use_derived)
{
  GstVaAllocator *self;
  gboolean use_derived;
  gboolean ret;

  g_return_val_if_fail (GST_IS_VA_ALLOCATOR (allocator), FALSE);
  g_return_val_if_fail (info, FALSE);

  self = GST_VA_ALLOCATOR (allocator);

  use_derived = feat_use_derived == GST_VA_FEATURE_AUTO ? self->use_derived
      : feat_use_derived == GST_VA_FEATURE_DISABLED ? FALSE : TRUE;

  if (gst_va_memory_pool_surface_count (&self->pool) != 0) {
    if (GST_VIDEO_INFO_FORMAT (info) == GST_VIDEO_INFO_FORMAT (&self->info)
        && GST_VIDEO_INFO_WIDTH (info) == GST_VIDEO_INFO_WIDTH (&self->info)
        && GST_VIDEO_INFO_HEIGHT (info) == GST_VIDEO_INFO_HEIGHT (&self->info)
        && usage_hint == self->usage_hint && use_derived == self->use_derived) {
      *info = self->info;       /* update callee info (offset & stride) */
      return TRUE;
    }
    return FALSE;
  }

  self->usage_hint = usage_hint;
  self->info = *info;

  g_clear_pointer (&self->copy, gst_va_surface_copy_free);

  ret = gst_va_allocator_try (allocator, feat_use_derived);
  if (ret)
    *info = self->info;

  return ret;
}

/**
 * gst_va_allocator_get_format:
 * @allocator: a #GstAllocator
 * @info: (out) (optional): a #GstVideoInfo
 * @usage_hint: (out) (optional): VA usage hint
 * @use_derived: (out) (optional): whether derived images are used for buffer
 *     mapping.
 *
 * Gets current internal configuration of @allocator.
 *
 * Returns: %TRUE if @allocator is already configured; %FALSE
 * otherwise.
 *
 * Since: 1.22
 */
gboolean
gst_va_allocator_get_format (GstAllocator * allocator, GstVideoInfo * info,
    guint * usage_hint, gboolean * use_derived)
{
  GstVaAllocator *self;

  g_return_val_if_fail (GST_IS_VA_ALLOCATOR (allocator), FALSE);
  self = GST_VA_ALLOCATOR (allocator);

  if (GST_VIDEO_INFO_FORMAT (&self->info) == GST_VIDEO_FORMAT_UNKNOWN)
    return FALSE;

  if (info)
    *info = self->info;
  if (usage_hint)
    *usage_hint = self->usage_hint;
  if (use_derived)
    *use_derived = self->use_derived;

  return TRUE;
}

/**
 * gst_va_allocator_set_hacks: (skip)
 * @allocator: a #GstAllocator
 * @hacks: hacks id to set
 *
 * Internal method to set allocator specific logic changes.
 *
 * Since: 1.22
 */
void
gst_va_allocator_set_hacks (GstAllocator * allocator, guint32 hacks)
{
  GstVaAllocator *self;

  g_return_if_fail (GST_IS_VA_ALLOCATOR (allocator));
  self = GST_VA_ALLOCATOR (allocator);

  self->hacks = hacks;
}

/**
 * gst_va_allocator_peek_display:
 * @allocator: a #GstAllocator
 *
 * Returns: (transfer none): the display which this
 *     @allocator belongs to. The reference of the display is unchanged.
 *
 * Since: 1.22
 */
GstVaDisplay *
gst_va_allocator_peek_display (GstAllocator * allocator)
{
  if (!allocator)
    return NULL;

  if (GST_IS_VA_DMABUF_ALLOCATOR (allocator)) {
    return GST_VA_DMABUF_ALLOCATOR (allocator)->display;
  } else if (GST_IS_VA_ALLOCATOR (allocator)) {
    return GST_VA_ALLOCATOR (allocator)->display;
  }

  return NULL;
}

/*============ Utilities =====================================================*/

/**
 * gst_va_memory_get_surface: (skip)
 * @mem: a #GstMemory
 *
 * Returns: (type guint): the VASurfaceID in @mem.
 *
 * Since: 1.22
 */
VASurfaceID
gst_va_memory_get_surface (GstMemory * mem)
{
  VASurfaceID surface = VA_INVALID_ID;

  if (!mem->allocator)
    return VA_INVALID_ID;

  if (GST_IS_DMABUF_ALLOCATOR (mem->allocator)) {
    GstVaBufferSurface *buf;

    buf = gst_mini_object_get_qdata (GST_MINI_OBJECT (mem),
        gst_va_buffer_surface_quark ());
    if (buf)
      surface = buf->surface;
  } else if (GST_IS_VA_ALLOCATOR (mem->allocator)) {
    GstVaMemory *va_mem = (GstVaMemory *) mem;
    surface = va_mem->surface;
  }

  return surface;
}

/**
 * gst_va_memory_peek_display:
 * @mem: a #GstMemory
 *
 * Returns: (transfer none): the display which
 *     this @mem belongs to. The reference of the display is unchanged.
 *
 * Since: 1.22
 */
GstVaDisplay *
gst_va_memory_peek_display (GstMemory * mem)
{
  GstAllocator *allocator;

  if (!mem)
    return NULL;

  allocator = GST_MEMORY_CAST (mem)->allocator;
  /* no allocator, not VA kind memory. */
  if (!allocator)
    return NULL;

  return gst_va_allocator_peek_display (allocator);
}

/**
 * gst_va_buffer_get_surface: (skip)
 * @buffer: a #GstBuffer
 *
 * Returns: (type guint): the VASurfaceID in @buffer.
 *
 * Since: 1.22
 */
VASurfaceID
gst_va_buffer_get_surface (GstBuffer * buffer)
{
  GstMemory *mem;

  mem = gst_buffer_peek_memory (buffer, 0);
  if (!mem)
    return VA_INVALID_ID;

  return gst_va_memory_get_surface (mem);
}

/**
 * gst_va_buffer_create_aux_surface:
 * @buffer: a #GstBuffer
 *
 * Creates a new VASurfaceID with @buffer's allocator and attached it
 * to it.
 *
 * *This method is used only by plugin's internal VA decoder.*
 *
 * Returns: %TRUE if the new VASurfaceID is attached to @buffer
 *     correctly; %FALSE, otherwise.
 *
 * Since: 1.22
 */
gboolean
gst_va_buffer_create_aux_surface (GstBuffer * buffer)
{
  GstMemory *mem;
  VASurfaceID surface = VA_INVALID_ID;
  GstVaDisplay *display = NULL;
  GstVideoFormat format;
  GstVaBufferSurface *surface_buffer;

  mem = gst_buffer_peek_memory (buffer, 0);
  if (!mem)
    return FALSE;

  /* Already created. */
  surface_buffer = gst_mini_object_get_qdata (GST_MINI_OBJECT (mem),
      gst_va_buffer_aux_surface_quark ());
  if (surface_buffer)
    return TRUE;

  if (!mem->allocator)
    return FALSE;

  if (GST_IS_VA_DMABUF_ALLOCATOR (mem->allocator)) {
    GstVaDmabufAllocator *self = GST_VA_DMABUF_ALLOCATOR (mem->allocator);
    guint32 fourcc, rt_format;

    format = GST_VIDEO_INFO_FORMAT (&self->info.vinfo);
    fourcc = gst_va_fourcc_from_video_format (format);
    rt_format = gst_va_chroma_from_video_format (format);
    if (fourcc == 0 || rt_format == 0) {
      GST_ERROR_OBJECT (self, "Unsupported format: %s",
          gst_video_format_to_string (format));
      return FALSE;
    }

    display = self->display;
    if (!va_create_surfaces (self->display, rt_format, fourcc,
            GST_VIDEO_INFO_WIDTH (&self->info.vinfo),
            GST_VIDEO_INFO_HEIGHT (&self->info.vinfo), self->usage_hint, NULL,
            0, NULL, &surface, 1))
      return FALSE;
  } else if (GST_IS_VA_ALLOCATOR (mem->allocator)) {
    GstVaAllocator *self = GST_VA_ALLOCATOR (mem->allocator);

    if (self->rt_format == 0) {
      GST_ERROR_OBJECT (self, "Unknown fourcc or chroma format");
      return FALSE;
    }

    display = self->display;
    format = GST_VIDEO_INFO_FORMAT (&self->info);
    if (!va_create_surfaces (self->display, self->rt_format, self->fourcc,
            GST_VIDEO_INFO_WIDTH (&self->info),
            GST_VIDEO_INFO_HEIGHT (&self->info), self->usage_hint, NULL, 0,
            NULL, &surface, 1))
      return FALSE;
  } else {
    g_assert_not_reached ();
  }

  if (!display || surface == VA_INVALID_ID)
    return FALSE;

  surface_buffer = gst_va_buffer_surface_new (surface);
  surface_buffer->display = gst_object_ref (display);
  g_atomic_int_add (&surface_buffer->ref_count, 1);

  gst_mini_object_set_qdata (GST_MINI_OBJECT (mem),
      gst_va_buffer_aux_surface_quark (), surface_buffer,
      gst_va_buffer_surface_unref);

  return TRUE;
}

/**
 * gst_va_buffer_get_aux_surface: (skip)
 * @buffer: a #GstBuffer
 *
 * Returns: (type guint): the VASurfaceID attached to
 *     @buffer.
 *
 * Since: 1.22
 */
VASurfaceID
gst_va_buffer_get_aux_surface (GstBuffer * buffer)
{
  GstVaBufferSurface *surface_buffer;
  GstMemory *mem;

  mem = gst_buffer_peek_memory (buffer, 0);
  if (!mem)
    return VA_INVALID_ID;

  surface_buffer = gst_mini_object_get_qdata (GST_MINI_OBJECT (mem),
      gst_va_buffer_aux_surface_quark ());
  if (!surface_buffer)
    return VA_INVALID_ID;

  /* No one increments it, and its lifetime is the same with the
     gstmemory itself */
  g_assert (g_atomic_int_get (&surface_buffer->ref_count) == 1);

  return surface_buffer->surface;
}

/**
 * gst_va_buffer_peek_display:
 * @buffer: a #GstBuffer
 *
 * Returns: (transfer none): the display which this
 *     @buffer belongs to. The reference of the display is unchanged.
 *
 * Since: 1.22
 */
GstVaDisplay *
gst_va_buffer_peek_display (GstBuffer * buffer)
{
  GstMemory *mem;

  if (!buffer)
    return NULL;

  mem = gst_buffer_peek_memory (buffer, 0);
  /* Buffer without mem, not VA kind memory. */
  if (!mem)
    return NULL;

  return gst_va_memory_peek_display (mem);
}
