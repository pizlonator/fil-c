/* GStreamer
 *
 * Copyright (C) 2023 Collabora
 * Copyright (C) 2016 Igalia
 *
 * Authors:
 *  Víctor Manuel Jáquez Leal <vjaquez@igalia.com>
 *  Javier Martin <javiermartin@by.com.es>
 *  Colin Kinloch <colin.kinloch@collabora.com>
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

#include "gstdrmdumb.h"
#include "gstdmabuf.h"

/**
 * SECTION:gstdrmdumb
 * @title: GstDRMDumbAllocator
 * @short_description: Memory wrapper for Linux DRM Dumb memory
 * @see_also: #GstMemory
 *
 * Since: 1.24
 */

#ifdef HAVE_LIBDRM
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <drm.h>
#include <drm_fourcc.h>
#include <xf86drm.h>
#endif

#define GST_CAT_DEFAULT drmdumballocator_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

#define GST_DRM_DUMB_MEMORY_TYPE "DRMDumbMemory"

typedef struct _GstDRMDumbMemory GstDRMDumbMemory;
struct _GstDRMDumbMemory
{
  GstMemory parent;

  gpointer ptr;
  gsize size;
  guint32 handle;
  guint refs;
};

struct _GstDRMDumbAllocator
{
  GstAllocator parent;

  gint drm_fd;
  gchar *drm_device_path;

  /* protected by GstDRMDumbAllocator object lock */
  GstAllocator *dmabuf_alloc;
};

#define parent_class gst_drm_dumb_allocator_parent_class
G_DEFINE_TYPE_WITH_CODE (GstDRMDumbAllocator, gst_drm_dumb_allocator,
    GST_TYPE_ALLOCATOR,
    GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "drmdumballocator", 0,
        "DRM dumb buffer allocator"));
enum
{
  PROP_DRM_FD = 1,
  PROP_DRM_DEVICE_PATH,
  PROP_N,
};

static GParamSpec *g_props[PROP_N] = { NULL, };

/**
 * gst_is_drm_dumb_memory:
 * @mem: the memory to be checked
 *
 * Returns: %TRUE if @mem is DRM Dumb memory, otherwise %FALSE
 *
 * Since: 1.24
 */
gboolean
gst_is_drm_dumb_memory (GstMemory * mem)
{
  return gst_memory_is_type (mem, GST_DRM_DUMB_MEMORY_TYPE);
}

/**
 * gst_drm_dumb_memory_get_handle:
 * @mem: the memory to get the handle from
 *
 * Return the DRM buffer object handle associated with @mem.
 *
 * Returns: the DRM buffer object handle associated with the memory, or 0.
 *     The handle is still owned by the GstMemory and cannot be used
 *     beyond the lifetime of this GstMemory unless it is being passed
 *     to DRM driver, which does handle a refcount internally.
 *
 * Since: 1.24
 */
guint32
gst_drm_dumb_memory_get_handle (GstMemory * mem)
{
  if (!gst_is_drm_dumb_memory (mem))
    return 0;

  return ((GstDRMDumbMemory *) mem)->handle;
}

/**
 * gst_drm_dumb_memory_export_dmabuf:
 * @mem: the memory to export from
 *
 * Exports a DMABuf from the DRM Bumb buffer object. One can check if this
 * feature is supported using gst_drm_dumb_allocator_has_prime_export();
 *
 * Returns: a #GstMemory from #GstDmaBufAllocator wrapping the exported dma-buf
 *    file descriptor.
 *
 * Since: 1.24
 */
GstMemory *
gst_drm_dumb_memory_export_dmabuf (GstMemory * mem)
{
#ifdef HAVE_LIBDRM
  GstDRMDumbMemory *drmmem = (GstDRMDumbMemory *) mem;
  GstDRMDumbAllocator *alloc = GST_DRM_DUMB_ALLOCATOR (mem->allocator);
  GstMemory *dmamem;
  gint ret;
  gint prime_fd;

  ret = drmPrimeHandleToFD (alloc->drm_fd, drmmem->handle,
      DRM_CLOEXEC | DRM_RDWR, &prime_fd);
  if (ret)
    goto export_fd_failed;

  dmamem = gst_dmabuf_allocator_alloc (alloc->dmabuf_alloc, prime_fd,
      gst_memory_get_sizes (mem, NULL, NULL));

  GST_DEBUG_OBJECT (alloc, "Exported bo handle %d as %d", drmmem->handle,
      prime_fd);

  return dmamem;

  /* ERRORS */
export_fd_failed:
  {
    GST_ERROR_OBJECT (alloc, "Failed to export bo handle %d: %s (%d)",
        drmmem->handle, g_strerror (errno), ret);
    return NULL;
  }

#else
  return NULL;
#endif
}

#ifdef HAVE_LIBDRM
static guint32
gst_drm_height_from_drm (guint32 drmfmt, guint32 height)
{
  guint32 ret;

  switch (drmfmt) {
    case DRM_FORMAT_YUV420:
    case DRM_FORMAT_YVU420:
    case DRM_FORMAT_YUV422:
    case DRM_FORMAT_NV12:
    case DRM_FORMAT_NV21:
    case DRM_FORMAT_P010:
    case DRM_FORMAT_P016:
      ret = height * 3 / 2;
      break;
    case DRM_FORMAT_NV16:
    case DRM_FORMAT_NV61:
      ret = height * 2;
      break;
    case DRM_FORMAT_NV24:
      ret = height * 3;
      break;
    default:
      ret = height;
      break;
  }

  return ret;
}

static guint32
gst_drm_bpp_from_drm (guint32 drm_fourcc)
{
  guint32 bpp;

  switch (drm_fourcc) {
    case DRM_FORMAT_YUV420:
    case DRM_FORMAT_YVU420:
    case DRM_FORMAT_YUV422:
    case DRM_FORMAT_NV12:
    case DRM_FORMAT_NV21:
    case DRM_FORMAT_NV16:
    case DRM_FORMAT_NV61:
    case DRM_FORMAT_NV24:
      bpp = 8;
      break;
    case DRM_FORMAT_P010:
      bpp = 10;
      break;
    case DRM_FORMAT_UYVY:
    case DRM_FORMAT_YUYV:
    case DRM_FORMAT_YVYU:
    case DRM_FORMAT_P016:
    case DRM_FORMAT_RGB565:
    case DRM_FORMAT_BGR565:
      bpp = 16;
      break;
    case DRM_FORMAT_BGR888:
    case DRM_FORMAT_RGB888:
      bpp = 24;
      break;
    default:
      bpp = 32;
      break;
  }

  return bpp;
}

static gboolean
check_drm_fd (GstDRMDumbAllocator * alloc)
{
  return alloc->drm_fd > -1;
}
#endif

static void
gst_drm_dumb_allocator_open_device (GstDRMDumbAllocator * alloc,
    const gchar * path)
{
#ifdef HAVE_LIBDRM
  gint fd = -1;

  /* Ignore default constructor call */
  if (path == NULL)
    return;

  /* construct only */
  g_assert (alloc->drm_fd == -1);
  g_assert (alloc->drm_device_path == NULL);

  fd = open (path, O_RDWR | O_CLOEXEC);

  if (fd < 0) {
    GST_WARNING_OBJECT (alloc, "Failed to open DRM device at %s", path);
    return;
  }

  alloc->drm_device_path = g_strdup (path);
  alloc->drm_fd = fd;
#endif
}

static void
gst_drm_dumb_allocator_set_fd (GstDRMDumbAllocator * alloc, gint fd)
{
#ifdef HAVE_LIBDRM
  /* Ignore default constructor call */
  if (fd == -1)
    return;

  /* construct only */
  g_assert (alloc->drm_fd == -1);
  g_assert (alloc->drm_device_path == NULL);

  if (fd >= 0) {
    alloc->drm_device_path = drmGetDeviceNameFromFd2 (fd);
    if (!alloc->drm_device_path) {
      GST_WARNING_OBJECT (alloc, "Failed to verify DRM fd.");
      return;
    }

    GST_DEBUG_OBJECT (alloc, "Using external FD for %s",
        alloc->drm_device_path);

    alloc->drm_fd = dup (fd);
  }
#endif
}

static void
gst_drm_dumb_allocator_memory_reset (GstDRMDumbAllocator * alloc,
    GstDRMDumbMemory * mem)
{
#ifdef HAVE_LIBDRM
  gint err;
  struct drm_mode_destroy_dumb arg = { 0, };

  if (!mem->size)
    return;

  if (!check_drm_fd (alloc))
    return;

  if (mem->ptr != NULL) {
    GST_WARNING_OBJECT (alloc, "destroying mapped bo (refcount=%d)", mem->refs);
    munmap (mem->ptr, mem->size);
    mem->ptr = NULL;
  }

  arg.handle = mem->handle;

  err = drmIoctl (alloc->drm_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &arg);
  if (err)
    GST_WARNING_OBJECT (alloc,
        "Failed to destroy dumb buffer object: %s %d",
        g_strerror (errno), errno);

  mem->handle = -1;
  mem->size = 0;
#endif
}

static gboolean
gst_drm_dumb_allocator_memory_create (GstDRMDumbAllocator * alloc,
    GstDRMDumbMemory * drmmem,
    guint32 drm_fourcc, guint32 width, guint32 height, guint32 * out_pitch)
{
#ifdef HAVE_LIBDRM
  gint ret;
  struct drm_mode_create_dumb arg = { 0, };

  if (drmmem->size)
    return TRUE;

  if (!check_drm_fd (alloc))
    return FALSE;

  arg.bpp = gst_drm_bpp_from_drm (drm_fourcc);
  arg.width = width;
  arg.height = gst_drm_height_from_drm (drm_fourcc, height);

  ret = drmIoctl (alloc->drm_fd, DRM_IOCTL_MODE_CREATE_DUMB, &arg);
  if (ret)
    goto create_failed;

  if (!arg.pitch) {
    GST_DEBUG_OBJECT (alloc,
        "DRM dumb buffer pitch not set, no need to modify vinfo");
    goto done;
  }

  GST_DEBUG_OBJECT (alloc,
      "DRM dumb buffer pitch is set, vinfo modification required");
  *out_pitch = arg.pitch;

done:
  drmmem->handle = arg.handle;
  /* will be used used as maxsize of GstMemory */
  drmmem->size = arg.size;

  return TRUE;

  /* ERRORS */
create_failed:
  {
    GST_ERROR_OBJECT (alloc, "Failed to create buffer object: %s (%d)",
        g_strerror (errno), errno);
    return FALSE;
  }

#else
  return FALSE;
#endif
}

static void
gst_drm_dumb_allocator_free (GstAllocator * base_alloc, GstMemory * mem)
{
  GstDRMDumbAllocator *alloc = GST_DRM_DUMB_ALLOCATOR (base_alloc);
  GstDRMDumbMemory *drmmem;

  drmmem = (GstDRMDumbMemory *) mem;

  gst_drm_dumb_allocator_memory_reset (alloc, drmmem);
  g_free (drmmem);
}

static void
gst_drm_dumb_allocator_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstDRMDumbAllocator *alloc = GST_DRM_DUMB_ALLOCATOR (object);

  switch (prop_id) {
    case PROP_DRM_FD:
      gst_drm_dumb_allocator_set_fd (alloc, g_value_get_int (value));
      break;
    case PROP_DRM_DEVICE_PATH:
      gst_drm_dumb_allocator_open_device (alloc, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_drm_dumb_allocator_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstDRMDumbAllocator *alloc = GST_DRM_DUMB_ALLOCATOR (object);

  switch (prop_id) {
    case PROP_DRM_FD:
      g_value_set_int (value, alloc->drm_fd);
      break;
    case PROP_DRM_DEVICE_PATH:
      g_value_set_string (value, alloc->drm_device_path);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_drm_dumb_allocator_finalize (GObject * obj)
{
  GstDRMDumbAllocator *alloc = GST_DRM_DUMB_ALLOCATOR (obj);

  gst_object_unref (alloc->dmabuf_alloc);

  g_free (alloc->drm_device_path);
  alloc->drm_device_path = NULL;

#ifdef HAVE_LIBDRM
  if (alloc->drm_fd >= 0) {
    close (alloc->drm_fd);
    alloc->drm_fd = -1;
  }
#endif

  G_OBJECT_CLASS (parent_class)->finalize (obj);
}

static void
gst_drm_dumb_allocator_class_init (GstDRMDumbAllocatorClass * klass)
{
  GObjectClass *gobject_class;
  GstAllocatorClass *allocator_class;

  allocator_class = GST_ALLOCATOR_CLASS (klass);
  gobject_class = G_OBJECT_CLASS (klass);

  allocator_class->free = gst_drm_dumb_allocator_free;

  gobject_class->set_property = gst_drm_dumb_allocator_set_property;
  gobject_class->get_property = gst_drm_dumb_allocator_get_property;
  gobject_class->finalize = gst_drm_dumb_allocator_finalize;

  /**
   * GstDRMDumbAllocator:drm-fd:
   *
   * Since: 1.24
   */
  g_props[PROP_DRM_FD] = g_param_spec_int ("drm-fd", "DRM fd",
      "DRM file descriptor", -1, G_MAXINT, -1,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

  /**
   * GstDRMDumbAllocator:drm-device-path: (type filename):
   *
   * Since: 1.24
   */
  g_props[PROP_DRM_DEVICE_PATH] = g_param_spec_string ("drm-device-path",
      "DRM device path", "DRM device path", NULL,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (gobject_class, PROP_N, g_props);
}

static gpointer
gst_drm_dumb_memory_map (GstMemory * mem, gsize maxsize, GstMapFlags flags)
{
#ifdef HAVE_LIBDRM
  GstDRMDumbAllocator *alloc = GST_DRM_DUMB_ALLOCATOR (mem->allocator);
  GstDRMDumbMemory *drmmem;
  gint err;
  gpointer out;
  struct drm_mode_map_dumb arg = { 0, };


  if (!check_drm_fd (alloc))
    return NULL;

  drmmem = (GstDRMDumbMemory *) mem;
  if (!drmmem->size)
    return NULL;

  /* Reuse existing buffer object mapping if possible */
  if (drmmem->ptr != NULL) {
    goto out;
  }

  arg.handle = drmmem->handle;

  err = drmIoctl (alloc->drm_fd, DRM_IOCTL_MODE_MAP_DUMB, &arg);
  if (err) {
    GST_ERROR_OBJECT (alloc, "Failed to get offset of buffer object: %s %d",
        g_strerror (errno), errno);
    return NULL;
  }

  out = mmap (0, drmmem->size,
      PROT_READ | PROT_WRITE, MAP_SHARED, alloc->drm_fd, arg.offset);
  if (out == MAP_FAILED) {
    GST_ERROR_OBJECT (alloc, "Failed to map dumb buffer object: %s %d",
        g_strerror (errno), errno);
    return NULL;
  }
  drmmem->ptr = out;

out:
  g_atomic_int_inc (&drmmem->refs);
  return drmmem->ptr;

#else
  return NULL;
#endif
}

static void
gst_drm_dumb_memory_unmap (GstMemory * mem)
{
#ifdef HAVE_LIBDRM
  GstDRMDumbMemory *drmmem;

  if (!check_drm_fd ((GstDRMDumbAllocator *) mem->allocator))
    return;

  drmmem = (GstDRMDumbMemory *) mem;
  if (!drmmem->size)
    return;

  if (g_atomic_int_dec_and_test (&drmmem->refs)) {
    munmap (drmmem->ptr, drmmem->size);
    drmmem->ptr = NULL;
  }
#endif
}

static void
gst_drm_dumb_allocator_init (GstDRMDumbAllocator * alloc)
{
  GstAllocator *base_alloc = GST_ALLOCATOR_CAST (alloc);

  alloc->drm_fd = -1;
  alloc->drm_device_path = NULL;
  alloc->dmabuf_alloc = gst_dmabuf_allocator_new ();

  base_alloc->mem_type = GST_DRM_DUMB_MEMORY_TYPE;
  base_alloc->mem_map = gst_drm_dumb_memory_map;
  base_alloc->mem_unmap = gst_drm_dumb_memory_unmap;
  /* Use the default, fallback copy function */

  GST_OBJECT_FLAG_SET (alloc, GST_ALLOCATOR_FLAG_CUSTOM_ALLOC);
}

#ifdef HAVE_LIBDRM
static gboolean
check_cap (GstDRMDumbAllocator * alloc)
{
  gint ret;
  guint64 has_dumb = 0;

  if (!alloc)
    return FALSE;

  if (!check_drm_fd (alloc))
    return FALSE;

  ret = drmGetCap (alloc->drm_fd, DRM_CAP_DUMB_BUFFER, &has_dumb);
  if (ret)
    GST_WARNING_OBJECT (alloc, "could not get dumb buffer capability");

  return !!(has_dumb);
}
#endif

/**
 * gst_drm_dumb_allocator_new_with_fd:
 * @drm_fd: file descriptor of the DRM device
 *
 * Creates a new #GstDRMDumbAllocator for the specific file desciptor. This
 * function can fail if the file descriptor is not a DRM device or if
 * the DRM device does not support DUMB allocation.
 *
 * Returns: (transfer full) (nullable): a new DRM Dumb allocator. Use gst_object_unref()
 *   to release the allocator after usage.
 *
 * Since: 1.24

 */
GstAllocator *
gst_drm_dumb_allocator_new_with_fd (gint drm_fd)
{
#ifdef HAVE_LIBDRM
  GstDRMDumbAllocator *alloc;

  alloc = g_object_new (GST_TYPE_DRM_DUMB_ALLOCATOR, "drm-fd", drm_fd, NULL);
  gst_object_ref_sink (alloc);

  if (!check_drm_fd (alloc))
    g_clear_object (&alloc);

  if (!check_cap (alloc))
    g_clear_object (&alloc);

  return alloc ? GST_ALLOCATOR (alloc) : NULL;

#else
  return NULL;
#endif
}

/**
 * gst_drm_dumb_allocator_new_with_device_path:
 * @drm_device_path: (type filename): path to the DRM device to open
 *
 * Creates a new #GstDRMDumbAllocator for the specific device path. This
 * function can fail if the path does not exist, is not a DRM device or if
 * the DRM device doesnot support DUMB allocation.
 *
 * Returns: (transfer full) (nullable): a new DRM Dumb allocator. Use gst_object_unref()
 *   to release the allocator after usage.
 *
 * Since: 1.24
 */
GstAllocator *
gst_drm_dumb_allocator_new_with_device_path (const gchar * drm_device_path)
{
#ifdef HAVE_LIBDRM
  GstDRMDumbAllocator *alloc;

  alloc = g_object_new (GST_TYPE_DRM_DUMB_ALLOCATOR,
      "drm-device-path", drm_device_path, NULL);
  gst_object_ref_sink (alloc);

  if (!check_drm_fd (alloc))
    g_clear_object (&alloc);

  if (!check_cap (alloc))
    g_clear_object (&alloc);

  return alloc ? GST_ALLOCATOR (alloc) : NULL;

#else
  return NULL;
#endif
}

/**
 * gst_drm_dumb_allocator_alloc:
 * @allocator: (type GstAllocators.DRMDumbAllocator): the allocator instance
 * @drm_fourcc: the DRM format to allocate for
 * @width: padded width for this allocation
 * @height: padded height for this allocation
 * @out_pitch: (out): the pitch as returned by the driver
 *
 * Allocated a DRM buffer object for the specific @drm_fourcc, @width and
 * @height. Note that the DRM Dumb allocation interface is agnostic to the
 * pixel format. This @drm_fourcc is converted into a bpp (bit-per-pixel)
 * number and the height is scaled according to the sub-sampling.
 *
 * Returns: (transfer full): a new DRM Dumb #GstMemory. Use gst_memory_unref()
 *   to release the memory after usage.
 *
 * Since: 1.24
 */
GstMemory *
gst_drm_dumb_allocator_alloc (GstAllocator * base_alloc,
    guint32 drm_fourcc, guint32 width, guint32 height, guint32 * out_pitch)
{
  GstDRMDumbAllocator *alloc = GST_DRM_DUMB_ALLOCATOR (base_alloc);
  GstDRMDumbMemory *drmmem;
  GstMemory *mem;

  drmmem = g_new0 (GstDRMDumbMemory, 1);
  mem = GST_MEMORY_CAST (drmmem);

  if (!gst_drm_dumb_allocator_memory_create (alloc, drmmem,
          drm_fourcc, width, height, out_pitch)) {
    g_free (drmmem);
    return NULL;
  }

  gst_memory_init (mem, 0, base_alloc, NULL, drmmem->size, 0, 0, drmmem->size);
  return mem;
}

/**
 * gst_drm_dumb_allocator_has_prime_export:
 * @allocator: (type GstAllocators.DRMDumbAllocator): the #GstAllocator
 *
 * This function allow verifying if the driver support dma-buf exportation.
 *
 * Returns: %TRUE if the allocator support exporting dma-buf.
 *
 * Since: 1.24
 */
gboolean
gst_drm_dumb_allocator_has_prime_export (GstAllocator * base_alloc)
{
#ifdef HAVE_LIBDRM
  GstDRMDumbAllocator *alloc = GST_DRM_DUMB_ALLOCATOR (base_alloc);
  gint ret;
  guint64 has_prime = 0;

  if (!check_drm_fd (alloc))
    return FALSE;

  ret = drmGetCap (alloc->drm_fd, DRM_CAP_PRIME, &has_prime);
  if (ret)
    GST_WARNING_OBJECT (alloc, "could not get prime capability");

  return !!(has_prime & DRM_PRIME_CAP_EXPORT);

#else
  return FALSE;
#endif
}
