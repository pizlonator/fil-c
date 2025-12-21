/*
 * GStreamer
 * Copyright (C) 2015 Matthew Waters <matthew@centricular.com>
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

#include <string.h>

#include "gstglrenderbuffer.h"

#include "gstglcontext.h"
#include "gstglfuncs.h"
#include "gstglmemory.h"

/**
 * SECTION:gstglrenderbuffer
 * @title: GstGLRenderBuffer
 * @short_description: memory subclass for GL renderbuffer objects
 * @see_also: #GstMemory, #GstAllocator
 *
 * GstGLRenderbuffer is a #GstGLBaseMemory subclass providing support for
 * OpenGL renderbuffers.
 *
 * #GstGLRenderbuffer is created or wrapped through gst_gl_base_memory_alloc()
 * with #GstGLRenderbufferAllocationParams.
 *
 * Since: 1.10
 */

#define USING_OPENGL(context) (gst_gl_context_check_gl_version (context, GST_GL_API_OPENGL, 1, 0))
#define USING_OPENGL3(context) (gst_gl_context_check_gl_version (context, GST_GL_API_OPENGL3, 3, 1))
#define USING_GLES(context) (gst_gl_context_check_gl_version (context, GST_GL_API_GLES, 1, 0))
#define USING_GLES2(context) (gst_gl_context_check_gl_version (context, GST_GL_API_GLES2, 2, 0))
#define USING_GLES3(context) (gst_gl_context_check_gl_version (context, GST_GL_API_GLES2, 3, 0))

static GstAllocator *_gl_renderbuffer_allocator;

GST_DEBUG_CATEGORY_STATIC (GST_CAT_GL_RENDERBUFFER);
#define GST_CAT_DEFAULT GST_CAT_GL_RENDERBUFFER

G_DEFINE_TYPE (GstGLRenderbufferAllocator, gst_gl_renderbuffer_allocator,
    GST_TYPE_GL_BASE_MEMORY_ALLOCATOR);

#ifndef GST_REMOVE_DEPRECATED
GST_DEFINE_MINI_OBJECT_TYPE (GstGLRenderbuffer, gst_gl_renderbuffer);
#endif

static guint
_new_renderbuffer (GstGLContext * context, guint format, guint width,
    guint height)
{
  const GstGLFuncs *gl = context->gl_vtable;
  guint rbo_id;

  gl->GenRenderbuffers (1, &rbo_id);
  gl->BindRenderbuffer (GL_RENDERBUFFER, rbo_id);

  gl->RenderbufferStorage (GL_RENDERBUFFER, format, width, height);

  gl->BindRenderbuffer (GL_RENDERBUFFER, 0);

  return rbo_id;
}

static gboolean
_gl_rbo_create (GstGLRenderbuffer * gl_mem, GError ** error)
{
  if (!gl_mem->renderbuffer_wrapped) {
    GstGLContext *context = gl_mem->mem.context;
    GLenum internal_format;
    GLenum tex_format;
    GLenum renderbuffer_type;

    tex_format = gl_mem->renderbuffer_format;
    renderbuffer_type = GL_UNSIGNED_BYTE;
    if (gl_mem->renderbuffer_format == GST_GL_RGB565) {
      tex_format = GST_GL_RGB;
      renderbuffer_type = GL_UNSIGNED_SHORT_5_6_5;
    }

    internal_format =
        gst_gl_sized_gl_format_from_gl_format_type (context, tex_format,
        renderbuffer_type);

    gl_mem->renderbuffer_id =
        _new_renderbuffer (context, internal_format,
        gst_gl_renderbuffer_get_width (gl_mem),
        gst_gl_renderbuffer_get_height (gl_mem));

    GST_CAT_TRACE (GST_CAT_GL_RENDERBUFFER, "Generating renderbuffer id:%u "
        "format:%u dimensions:%ux%u", gl_mem->renderbuffer_id, internal_format,
        gst_gl_renderbuffer_get_width (gl_mem),
        gst_gl_renderbuffer_get_height (gl_mem));
  }

  return TRUE;
}

static void
gst_gl_renderbuffer_init (GstGLRenderbuffer * mem, GstAllocator * allocator,
    GstMemory * parent, GstGLContext * context,
    GstGLFormat renderbuffer_format, const GstAllocationParams * params,
    guint width, guint height, gpointer user_data, GDestroyNotify notify)
{
  gsize size;
  guint tex_type;

  tex_type = GL_UNSIGNED_BYTE;
  if (renderbuffer_format == GST_GL_RGB565)
    tex_type = GL_UNSIGNED_SHORT_5_6_5;
  size =
      gst_gl_format_type_n_bytes (renderbuffer_format,
      tex_type) * width * height;

  mem->renderbuffer_format = renderbuffer_format;
  mem->width = width;
  mem->height = height;

  gst_gl_base_memory_init ((GstGLBaseMemory *) mem, allocator, parent, context,
      params, size, user_data, notify);

  GST_CAT_DEBUG (GST_CAT_GL_RENDERBUFFER, "new GL renderbuffer context:%"
      GST_PTR_FORMAT " memory:%p format:%u dimensions:%ux%u ", context, mem,
      mem->renderbuffer_format, gst_gl_renderbuffer_get_width (mem),
      gst_gl_renderbuffer_get_height (mem));
}

static gpointer
_gl_rbo_map (GstGLRenderbuffer * gl_mem, GstMapInfo * info, gsize maxsize)
{
  GST_CAT_WARNING (GST_CAT_GL_RENDERBUFFER, "Renderbuffer's cannot be mapped");

  return NULL;
}

static void
_gl_rbo_unmap (GstGLRenderbuffer * gl_mem, GstMapInfo * info)
{
}

static GstMemory *
_gl_rbo_copy (GstGLRenderbuffer * src, gssize offset, gssize size)
{
  GST_CAT_WARNING (GST_CAT_GL_RENDERBUFFER, "Renderbuffer's cannot be copied");

  return NULL;
}

static GstMemory *
_gl_rbo_alloc (GstAllocator * allocator, gsize size,
    GstAllocationParams * params)
{
  g_warning ("Use gst_gl_base_memory_alloc to allocate from this allocator");

  return NULL;
}

static void
_gl_rbo_destroy (GstGLRenderbuffer * gl_mem)
{
  const GstGLFuncs *gl = gl_mem->mem.context->gl_vtable;

  if (gl_mem->renderbuffer_id && !gl_mem->renderbuffer_wrapped)
    gl->DeleteRenderbuffers (1, &gl_mem->renderbuffer_id);
}

static GstGLRenderbuffer *
_default_gl_rbo_alloc (GstGLRenderbufferAllocator * allocator,
    GstGLRenderbufferAllocationParams * params)
{
  guint alloc_flags = params->parent.alloc_flags;
  GstGLRenderbuffer *mem;

  g_return_val_if_fail ((alloc_flags &
          GST_GL_ALLOCATION_PARAMS_ALLOC_FLAG_WRAP_SYSMEM) == 0, NULL);

  mem = g_new0 (GstGLRenderbuffer, 1);

  if (alloc_flags & GST_GL_ALLOCATION_PARAMS_ALLOC_FLAG_WRAP_GPU_HANDLE) {
    mem->renderbuffer_id = GPOINTER_TO_UINT (params->parent.gl_handle);
    mem->renderbuffer_wrapped = TRUE;
  }

  gst_gl_renderbuffer_init (mem, GST_ALLOCATOR_CAST (allocator), NULL,
      params->parent.context, params->renderbuffer_format,
      params->parent.alloc_params, params->width, params->height,
      params->parent.user_data, params->parent.notify);

  return mem;
}

static void
gst_gl_renderbuffer_allocator_class_init (GstGLRenderbufferAllocatorClass *
    klass)
{
  GstGLBaseMemoryAllocatorClass *gl_base;
  GstAllocatorClass *allocator_class;

  gl_base = (GstGLBaseMemoryAllocatorClass *) klass;
  allocator_class = (GstAllocatorClass *) klass;

  gl_base->alloc =
      (GstGLBaseMemoryAllocatorAllocFunction) _default_gl_rbo_alloc;
  gl_base->create = (GstGLBaseMemoryAllocatorCreateFunction) _gl_rbo_create;
  gl_base->destroy = (GstGLBaseMemoryAllocatorDestroyFunction) _gl_rbo_destroy;

  allocator_class->alloc = _gl_rbo_alloc;
}

static void
gst_gl_renderbuffer_allocator_init (GstGLRenderbufferAllocator * allocator)
{
  GstAllocator *alloc = GST_ALLOCATOR_CAST (allocator);

  alloc->mem_type = GST_GL_RENDERBUFFER_ALLOCATOR_NAME;

  alloc->mem_map_full = (GstMemoryMapFullFunction) _gl_rbo_map;
  alloc->mem_unmap_full = (GstMemoryUnmapFullFunction) _gl_rbo_unmap;
  alloc->mem_copy = (GstMemoryCopyFunction) _gl_rbo_copy;

  GST_OBJECT_FLAG_SET (allocator, GST_ALLOCATOR_FLAG_CUSTOM_ALLOC);
}

/**
 * gst_gl_renderbuffer_get_width:
 * @gl_mem: a #GstGLRenderbuffer
 *
 * Returns: the configured width of @gl_mem
 *
 * Since: 1.10
 */
gint
gst_gl_renderbuffer_get_width (GstGLRenderbuffer * gl_mem)
{
  g_return_val_if_fail (gst_is_gl_renderbuffer ((GstMemory *) gl_mem), 0);

  return gl_mem->width;
}

/**
 * gst_gl_renderbuffer_get_height:
 * @gl_mem: a #GstGLRenderbuffer
 *
 * Returns: the configured height of @gl_mem
 *
 * Since: 1.10
 */
gint
gst_gl_renderbuffer_get_height (GstGLRenderbuffer * gl_mem)
{
  g_return_val_if_fail (gst_is_gl_renderbuffer ((GstMemory *) gl_mem), 0);

  return gl_mem->height;
}

/**
 * gst_gl_renderbuffer_get_format:
 * @gl_mem: a #GstGLRenderbuffer
 *
 * Returns: the #GstGLFormat of @gl_mem
 *
 * Since: 1.12
 */
GstGLFormat
gst_gl_renderbuffer_get_format (GstGLRenderbuffer * gl_mem)
{
  g_return_val_if_fail (gst_is_gl_renderbuffer ((GstMemory *) gl_mem), 0);

  return gl_mem->renderbuffer_format;
}

/**
 * gst_gl_renderbuffer_get_id:
 * @gl_mem: a #GstGLRenderbuffer
 *
 * Returns: the OpenGL renderbuffer handle of @gl_mem
 *
 * Since: 1.10
 */
guint
gst_gl_renderbuffer_get_id (GstGLRenderbuffer * gl_mem)
{
  g_return_val_if_fail (gst_is_gl_renderbuffer ((GstMemory *) gl_mem), 0);

  return gl_mem->renderbuffer_id;
}

/**
 * gst_gl_renderbuffer_init_once:
 *
 * Initializes the GL Base Texture allocator. It is safe to call this function
 * multiple times.  This must be called before any other GstGLRenderbuffer operation.
 *
 * Since: 1.10
 */
void
gst_gl_renderbuffer_init_once (void)
{
  static gsize _init = 0;

  if (g_once_init_enter (&_init)) {
    gst_gl_base_memory_init_once ();

    GST_DEBUG_CATEGORY_INIT (GST_CAT_GL_RENDERBUFFER, "glrenderbuffermemory", 0,
        "OpenGL Renderbuffer memory");

    _gl_renderbuffer_allocator =
        g_object_new (GST_TYPE_GL_RENDERBUFFER_ALLOCATOR, NULL);
    gst_object_ref_sink (_gl_renderbuffer_allocator);
    GST_OBJECT_FLAG_SET (_gl_renderbuffer_allocator,
        GST_OBJECT_FLAG_MAY_BE_LEAKED);

    gst_allocator_register (GST_GL_RENDERBUFFER_ALLOCATOR_NAME,
        _gl_renderbuffer_allocator);

    g_once_init_leave (&_init, 1);
  }
}

/**
 * gst_is_gl_renderbuffer:
 * @mem:a #GstMemory
 *
 * Returns: whether the memory at @mem is a #GstGLRenderbuffer
 *
 * Since: 1.10
 */
gboolean
gst_is_gl_renderbuffer (GstMemory * mem)
{
  return mem != NULL && mem->allocator != NULL
      && g_type_is_a (G_OBJECT_TYPE (mem->allocator),
      GST_TYPE_GL_RENDERBUFFER_ALLOCATOR);
}

G_DEFINE_BOXED_TYPE (GstGLRenderbufferAllocationParams,
    gst_gl_renderbuffer_allocation_params,
    (GBoxedCopyFunc) gst_gl_allocation_params_copy,
    (GBoxedFreeFunc) gst_gl_allocation_params_free);

static void
_gst_gl_rb_alloc_params_free_data (GstGLRenderbufferAllocationParams * params)
{
  gst_gl_allocation_params_free_data (&params->parent);
}

static void
_gst_gl_rb_alloc_params_copy_data (GstGLRenderbufferAllocationParams * src_vid,
    GstGLRenderbufferAllocationParams * dest_vid)
{
  GstGLAllocationParams *src = (GstGLAllocationParams *) src_vid;
  GstGLAllocationParams *dest = (GstGLAllocationParams *) dest_vid;

  gst_gl_allocation_params_copy_data (src, dest);

  dest_vid->renderbuffer_format = src_vid->renderbuffer_format;
  dest_vid->width = src_vid->width;
  dest_vid->height = src_vid->height;
}

static gboolean
    _gst_gl_renderbuffer_allocation_params_init_full
    (GstGLRenderbufferAllocationParams * params, gsize struct_size,
    guint alloc_flags, GstGLAllocationParamsCopyFunc copy,
    GstGLAllocationParamsFreeFunc free, GstGLContext * context,
    const GstAllocationParams * alloc_params, guint width, guint height,
    GstGLFormat renderbuffer_format, gpointer wrapped_data,
    gpointer gl_handle, gpointer user_data, GDestroyNotify notify)
{
  g_return_val_if_fail (params != NULL, FALSE);
  g_return_val_if_fail (copy != NULL, FALSE);
  g_return_val_if_fail (free != NULL, FALSE);
  g_return_val_if_fail (GST_IS_GL_CONTEXT (context), FALSE);

  memset (params, 0, sizeof (*params));

  if (!gst_gl_allocation_params_init ((GstGLAllocationParams *) params,
          struct_size, alloc_flags, copy, free, context, 0, alloc_params,
          wrapped_data, gl_handle, user_data, notify))
    return FALSE;

  params->renderbuffer_format = renderbuffer_format;
  params->width = width;
  params->height = height;

  return TRUE;
}

/**
 * gst_gl_renderbuffer_allocation_params_new:
 * @context: a #GstGLContext
 * @alloc_params: (allow-none): the #GstAllocationParams for sysmem mappings of the texture
 * @width: the width of the renderbuffer
 * @height: the height of the renderbuffer
 * @renderbuffer_format: the #GstGLFormat for the created textures
 *
 * Returns: a new #GstGLRenderbufferAllocationParams for allocating #GstGLRenderbuffer's
 *
 * Since: 1.10
 */
GstGLRenderbufferAllocationParams *
gst_gl_renderbuffer_allocation_params_new (GstGLContext * context,
    const GstAllocationParams * alloc_params, GstGLFormat renderbuffer_format,
    guint width, guint height)
{
  GstGLRenderbufferAllocationParams *params =
      g_new0 (GstGLRenderbufferAllocationParams, 1);

  if (!_gst_gl_renderbuffer_allocation_params_init_full (params,
          sizeof (GstGLRenderbufferAllocationParams),
          GST_GL_ALLOCATION_PARAMS_ALLOC_FLAG_ALLOC |
          GST_GL_ALLOCATION_PARAMS_ALLOC_FLAG_VIDEO,
          (GstGLAllocationParamsCopyFunc) _gst_gl_rb_alloc_params_copy_data,
          (GstGLAllocationParamsFreeFunc) _gst_gl_rb_alloc_params_free_data,
          context, alloc_params, width, height, renderbuffer_format, NULL, 0,
          NULL, NULL)) {
    g_free (params);
    return NULL;
  }

  return params;
}

/**
 * gst_gl_renderbuffer_allocation_params_new_wrapped:
 * @context: a #GstGLContext
 * @alloc_params: (allow-none): the #GstAllocationParams for @tex_id
 * @width: the width of the renderbuffer
 * @height: the height of the renderbuffer
 * @renderbuffer_format: the #GstGLFormat for @tex_id
 * @gl_handle: the GL handle to wrap
 * @user_data: (allow-none): user data to call @notify with
 * @notify: (allow-none): a #GDestroyNotify
 *
 * Returns: a new #GstGLRenderbufferAllocationParams for wrapping @gl_handle as a
 *          renderbuffer
 *
 * Since: 1.10
 */
GstGLRenderbufferAllocationParams *
gst_gl_renderbuffer_allocation_params_new_wrapped (GstGLContext * context,
    const GstAllocationParams * alloc_params, GstGLFormat renderbuffer_format,
    guint width, guint height, gpointer gl_handle, gpointer user_data,
    GDestroyNotify notify)
{
  GstGLRenderbufferAllocationParams *params =
      g_new0 (GstGLRenderbufferAllocationParams, 1);

  if (!_gst_gl_renderbuffer_allocation_params_init_full (params,
          sizeof (GstGLRenderbufferAllocationParams),
          GST_GL_ALLOCATION_PARAMS_ALLOC_FLAG_WRAP_GPU_HANDLE |
          GST_GL_ALLOCATION_PARAMS_ALLOC_FLAG_VIDEO,
          (GstGLAllocationParamsCopyFunc) _gst_gl_rb_alloc_params_copy_data,
          (GstGLAllocationParamsFreeFunc) _gst_gl_rb_alloc_params_free_data,
          context, alloc_params, width, height, renderbuffer_format, NULL,
          gl_handle, user_data, notify)) {
    g_free (params);
    return NULL;
  }

  return params;
}
