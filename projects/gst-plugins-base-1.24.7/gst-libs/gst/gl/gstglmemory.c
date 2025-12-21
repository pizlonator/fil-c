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

#include "gstglmemory.h"

#include "gl.h"
#include "gstglcontext_private.h"
#include "gstglfuncs.h"

/**
 * SECTION:gstglmemory
 * @title: GstGLMemory
 * @short_description: memory subclass for GL textures
 * @see_also: #GstMemory, #GstAllocator, #GstGLBufferPool
 *
 * GstGLMemory is a #GstGLBaseMemory subclass providing support for the mapping of
 * OpenGL textures.
 *
 * #GstGLMemory is created or wrapped through gst_gl_base_memory_alloc()
 * with #GstGLVideoAllocationParams.
 *
 * Data is uploaded or downloaded from the GPU as is necessary.
 *
 * The #GstCaps that is used for #GstGLMemory based buffers should contain
 * the %GST_CAPS_FEATURE_MEMORY_GL_MEMORY as a #GstCapsFeatures and should
 * contain a 'texture-target' field with one of the #GstGLTextureTarget values
 * as a string, i.e. some combination of 'texture-target=(string){2D,
 * rectangle, external-oes}'.
 */

#define USING_OPENGL(context) (gst_gl_context_check_gl_version (context, GST_GL_API_OPENGL, 1, 0))
#define USING_OPENGL3(context) (gst_gl_context_check_gl_version (context, GST_GL_API_OPENGL3, 3, 1))
#define USING_GLES(context) (gst_gl_context_check_gl_version (context, GST_GL_API_GLES, 1, 0))
#define USING_GLES2(context) (gst_gl_context_check_gl_version (context, GST_GL_API_GLES2, 2, 0))
#define USING_GLES3(context) (gst_gl_context_check_gl_version (context, GST_GL_API_GLES2, 3, 0))

#define GL_MEM_WIDTH(gl_mem) _get_plane_width (&gl_mem->info, gl_mem->plane)
#define GL_MEM_HEIGHT(gl_mem) _get_plane_height (&gl_mem->info, gl_mem->plane)
#define GL_MEM_STRIDE(gl_mem) _get_mem_stride (gl_mem)

static GstAllocator *_gl_memory_allocator;

GST_DEBUG_CATEGORY_STATIC (GST_CAT_GL_MEMORY);
#define GST_CAT_DEFAULT GST_CAT_GL_MEMORY

/* compatibility definitions... */
#ifndef GL_UNPACK_ROW_LENGTH
#define GL_UNPACK_ROW_LENGTH 0x0CF2
#endif

#ifndef GL_TEXTURE_RECTANGLE
#define GL_TEXTURE_RECTANGLE 0x84F5
#endif
#ifndef GL_TEXTURE_EXTERNAL_OES
#define GL_TEXTURE_EXTERNAL_OES 0x8D65
#endif

#ifndef GL_READ_FRAMEBUFFER
#define GL_READ_FRAMEBUFFER 0x8CA8
#endif
#ifndef GL_DRAW_FRAMEBUFFER
#define GL_DRAW_FRAMEBUFFER 0x8CA9
#endif

G_DEFINE_TYPE (GstGLMemoryAllocator, gst_gl_memory_allocator,
    GST_TYPE_GL_BASE_MEMORY_ALLOCATOR);

#ifndef GST_REMOVE_DEPRECATED
GST_DEFINE_MINI_OBJECT_TYPE (GstGLMemory, gst_gl_memory);
#endif

typedef struct
{
  /* in */
  GstGLMemory *src;
  GstGLFormat out_format;
  guint out_width, out_height;
  GstGLTextureTarget tex_target;
  GstGLFormat tex_format;
  /* inout */
  guint tex_id;
  /* out */
  gboolean result;
} GstGLMemoryCopyParams;

static inline guint
_get_plane_width (const GstVideoInfo * info, guint plane)
{
  if (GST_VIDEO_INFO_IS_YUV (info)) {
    gint comp[GST_VIDEO_MAX_COMPONENTS];
    gst_video_format_info_component (info->finfo, plane, comp);
    return GST_VIDEO_INFO_COMP_WIDTH (info, comp[0]);
  } else {
    /* RGB, GRAY */
    return GST_VIDEO_INFO_WIDTH (info);
  }
}

static inline guint
_get_plane_height (const GstVideoInfo * info, guint plane)
{
  if (GST_VIDEO_FORMAT_INFO_IS_TILED (info->finfo)) {
    guint tile_height;
    gsize stride;

    tile_height = GST_VIDEO_FORMAT_INFO_TILE_HEIGHT (info->finfo, plane);
    stride = GST_VIDEO_INFO_PLANE_STRIDE (info, plane);

    return GST_VIDEO_TILE_Y_TILES (stride) * tile_height;
  }

  if (GST_VIDEO_INFO_IS_YUV (info)) {
    gint comp[GST_VIDEO_MAX_COMPONENTS];
    gst_video_format_info_component (info->finfo, plane, comp);
    return GST_VIDEO_INFO_COMP_HEIGHT (info, comp[0]);
  }

  /* RGB, GRAY */
  return GST_VIDEO_INFO_HEIGHT (info);
}

static guint
_get_mem_stride (GstGLMemory * gl_mem)
{
  const GstVideoFormatInfo *finfo = gl_mem->info.finfo;
  guint stride = GST_VIDEO_INFO_PLANE_STRIDE (&gl_mem->info, gl_mem->plane);

  if (!GST_VIDEO_FORMAT_INFO_IS_TILED (finfo))
    return stride;

  return GST_VIDEO_TILE_X_TILES (stride) *
      GST_VIDEO_FORMAT_INFO_TILE_STRIDE (finfo, gl_mem->plane);
}

static inline void
_calculate_unpack_length (GstGLMemory * gl_mem, GstGLContext * context)
{
  GstVideoInfo *info = &gl_mem->info;
  guint n_gl_bytes;
  GstGLFormat tex_format;
  guint tex_type;

  gl_mem->tex_scaling[0] = 1.0f;
  gl_mem->tex_scaling[1] = 1.0f;
  gl_mem->unpack_length = 1;

  gst_gl_format_type_from_sized_gl_format (gl_mem->tex_format, &tex_format,
      &tex_type);
  n_gl_bytes = gst_gl_format_type_n_bytes (tex_format, tex_type);
  if (n_gl_bytes == 0) {
    GST_ERROR ("Unsupported texture type %d", gl_mem->tex_format);
    return;
  }

  /* For tiles, we need GL not to clip on the display width, as the would make
   * some tile not fully accessible by GLSL. */
  if (GST_VIDEO_FORMAT_INFO_IS_TILED (info->finfo))
    gl_mem->tex_width = GL_MEM_STRIDE (gl_mem) / n_gl_bytes;
  else
    gl_mem->tex_width = GL_MEM_WIDTH (gl_mem);

  if (USING_OPENGL (context) || USING_GLES3 (context)
      || USING_OPENGL3 (context)) {
    gl_mem->unpack_length = GL_MEM_STRIDE (gl_mem) / n_gl_bytes;
  } else if (USING_GLES2 (context)) {
    guint j = 8;

    while (j >= n_gl_bytes) {
      /* GST_ROUND_UP_j(GL_MEM_WIDTH (gl_mem) * n_gl_bytes) */
      guint round_up_j =
          ((GL_MEM_WIDTH (gl_mem) * n_gl_bytes) + j - 1) & ~(j - 1);

      if (round_up_j == GL_MEM_STRIDE (gl_mem)) {
        GST_CAT_LOG (GST_CAT_GL_MEMORY, "Found alignment of %u based on "
            "width (with plane width:%u, plane stride:%u and pixel stride:%u. "
            "RU%u(%u*%u) = %u)", j, GL_MEM_WIDTH (gl_mem),
            GL_MEM_STRIDE (gl_mem), n_gl_bytes, j, GL_MEM_WIDTH (gl_mem),
            n_gl_bytes, round_up_j);

        gl_mem->unpack_length = j;
        break;
      }
      j >>= 1;
    }

    if (j < n_gl_bytes) {
      /* Failed to find a suitable alignment, try based on plane_stride and
       * scale in the shader.  Useful for alignments that are greater than 8.
       */
      j = 8;

      while (j >= n_gl_bytes) {
        /* GST_ROUND_UP_j((GL_MEM_STRIDE (gl_mem)) */
        guint round_up_j = ((GL_MEM_STRIDE (gl_mem)) + j - 1) & ~(j - 1);

        if (round_up_j == (GL_MEM_STRIDE (gl_mem))) {
          GST_CAT_LOG (GST_CAT_GL_MEMORY, "Found alignment of %u based "
              "on stride (with plane stride:%u and pixel stride:%u. "
              "RU%u(%u) = %u)", j, GL_MEM_STRIDE (gl_mem), n_gl_bytes, j,
              GL_MEM_STRIDE (gl_mem), round_up_j);

          gl_mem->unpack_length = j;
          gl_mem->tex_scaling[0] =
              (gfloat) (GL_MEM_WIDTH (gl_mem) * n_gl_bytes) /
              (gfloat) GL_MEM_STRIDE (gl_mem);
          gl_mem->tex_width = GL_MEM_STRIDE (gl_mem) / n_gl_bytes;
          break;
        }
        j >>= 1;
      }

      if (j < n_gl_bytes) {
        GST_CAT_ERROR (GST_CAT_GL_MEMORY, "Failed to find matching "
            "alignment. Image may look corrupted. plane width:%u, "
            "plane stride:%u and pixel stride:%u", GL_MEM_WIDTH (gl_mem),
            GL_MEM_STRIDE (gl_mem), n_gl_bytes);
      }
    }
  }

  if (gl_mem->tex_target == GST_GL_TEXTURE_TARGET_RECTANGLE) {
    guint w_sub =
        GST_VIDEO_FORMAT_INFO_W_SUB (gl_mem->info.finfo, gl_mem->plane);
    guint h_sub =
        GST_VIDEO_FORMAT_INFO_H_SUB (gl_mem->info.finfo, gl_mem->plane);

    if (w_sub)
      gl_mem->tex_scaling[0] /= (1 << w_sub);
    if (h_sub)
      gl_mem->tex_scaling[1] /= (1 << h_sub);
  }
}

static guint
_new_texture (GstGLContext * context, guint target, guint internal_format,
    guint format, guint type, guint width, guint height)
{
  const GstGLFuncs *gl = context->gl_vtable;
  guint tex_id;

  gl->GenTextures (1, &tex_id);
  gl->BindTexture (target, tex_id);
  if (target == GL_TEXTURE_2D || target == GL_TEXTURE_RECTANGLE)
    gl->TexImage2D (target, 0, internal_format, width, height, 0, format, type,
        NULL);

  gl->TexParameteri (target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  gl->TexParameteri (target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  gl->TexParameteri (target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  gl->TexParameteri (target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  gl->BindTexture (target, 0);

  return tex_id;
}

static gboolean
_gl_tex_create (GstGLMemory * gl_mem, GError ** error)
{
  GstGLContext *context = gl_mem->mem.context;
  GstGLFormat internal_format;
  GstGLFormat tex_format;
  GLenum tex_type;

  internal_format = gl_mem->tex_format;
  gst_gl_format_type_from_sized_gl_format (internal_format, &tex_format,
      &tex_type);
  internal_format =
      gst_gl_sized_gl_format_from_gl_format_type (context, tex_format,
      tex_type);

  if (!gl_mem->texture_wrapped) {
    gl_mem->tex_id =
        _new_texture (context, gst_gl_texture_target_to_gl (gl_mem->tex_target),
        internal_format, tex_format, tex_type, gl_mem->tex_width,
        GL_MEM_HEIGHT (gl_mem));

    GST_TRACE ("Generating texture id:%u format:%u type:%u dimensions:%ux%u",
        gl_mem->tex_id, tex_format, tex_type, gl_mem->tex_width,
        GL_MEM_HEIGHT (gl_mem));
  }

  return TRUE;
}

static void
_gst_gl_memory_start_log (GstGLMemory * gl_mem, const gchar * func_name)
{
  /* debugging is disabled */
  if (!GST_GL_BASE_MEMORY_CAST (gl_mem)->query)
    return;

  gst_gl_query_start_log (GST_GL_BASE_MEMORY_CAST (gl_mem)->query,
      GST_CAT_GL_MEMORY, GST_LEVEL_LOG, NULL, "%s took", func_name);
}

static void
_gst_gl_memory_end_log (GstGLMemory * gl_mem)
{
  /* debugging is disabled */
  if (!GST_GL_BASE_MEMORY_CAST (gl_mem)->query)
    return;

  gst_gl_query_end (GST_GL_BASE_MEMORY_CAST (gl_mem)->query);
}

/**
 * gst_gl_memory_init:
 * @mem: the #GstGLBaseMemory to initialize
 * @allocator: the #GstAllocator to initialize with
 * @parent: (allow-none): the parent #GstMemory to initialize with
 * @context: the #GstGLContext to initialize with
 * @target: the #GstGLTextureTarget for this #GstGLMemory
 * @tex_format: the #GstGLFormat for this #GstGLMemory
 * @params: (allow-none): the @GstAllocationParams to initialize with
 * @info: the #GstVideoInfo for this #GstGLMemory
 * @plane: the plane number (starting from 0) for this #GstGLMemory
 * @valign: (allow-none): optional #GstVideoAlignment parameters
 * @notify: (allow-none): a #GDestroyNotify
 * @user_data: (allow-none): user data to call @notify with
 *
 * Initializes @mem with the required parameters.  @info is assumed to have
 * already have been modified with gst_video_info_align().
 *
 * Since: 1.8
 */
void
gst_gl_memory_init (GstGLMemory * mem, GstAllocator * allocator,
    GstMemory * parent, GstGLContext * context, GstGLTextureTarget target,
    GstGLFormat tex_format, const GstAllocationParams * params,
    const GstVideoInfo * info, guint plane, const GstVideoAlignment * valign,
    gpointer user_data, GDestroyNotify notify)
{
  const gchar *target_str;
  gsize size;

  g_return_if_fail (plane < GST_VIDEO_INFO_N_PLANES (info));

  mem->info = *info;
  if (valign)
    mem->valign = *valign;
  else
    gst_video_alignment_reset (&mem->valign);

  /* double-check alignment requirements (caller should've taken care of this) */
  if (params) {
    guint max_align, n;

    max_align = gst_memory_alignment;
    max_align |= params->align;
    for (n = 0; n < GST_VIDEO_MAX_PLANES; ++n)
      max_align |= mem->valign.stride_align[n];

    if (params->align < max_align && max_align > gst_memory_alignment) {
      GST_WARNING ("allocation params alignment %" G_GSIZE_FORMAT " is smaller "
          "than the max required video alignment %u", params->align, max_align);
    }
  }

  size = gst_gl_get_plane_data_size (info, valign, plane);

  mem->tex_target = target;
  mem->tex_format = tex_format;
  mem->plane = plane;

  _calculate_unpack_length (mem, context);

  gst_gl_base_memory_init ((GstGLBaseMemory *) mem, allocator, parent, context,
      params, size, user_data, notify);

  target_str = gst_gl_texture_target_to_string (target);
  GST_CAT_DEBUG (GST_CAT_GL_MEMORY, "new GL texture context:%"
      GST_PTR_FORMAT " memory:%p target:%s format:%u dimensions:%ux%u "
      "stride:%u size:%" G_GSIZE_FORMAT, context, mem, target_str,
      mem->tex_format, mem->tex_width, GL_MEM_HEIGHT (mem), GL_MEM_STRIDE (mem),
      mem->mem.mem.size);
}

/**
 * gst_gl_memory_read_pixels:
 * @gl_mem: a #GstGLMemory
 * @write_pointer: the data pointer to pass to glReadPixels
 *
 * Reads the texture in #GstGLMemory into @write_pointer if no buffer is bound
 * to `GL_PIXEL_PACK_BUFFER`.  Otherwise @write_pointer is the byte offset into
 * the currently bound `GL_PIXEL_PACK_BUFFER` buffer to store the result of
 * glReadPixels.  See the OpenGL specification for glReadPixels for more
 * details.
 *
 * Returns: whether theread operation succeeded
 *
 * Since: 1.8
 */
gboolean
gst_gl_memory_read_pixels (GstGLMemory * gl_mem, gpointer write_pointer)
{
  GstGLContext *context = gl_mem->mem.context;
  const GstGLFuncs *gl = context->gl_vtable;
  GstGLFormat format;
  guint type;
  guint fbo;

  gst_gl_format_type_from_sized_gl_format (gl_mem->tex_format, &format, &type);

  /* FIXME: avoid creating a framebuffer every download/copy */
  gl->GenFramebuffers (1, &fbo);
  gl->BindFramebuffer (GL_FRAMEBUFFER, fbo);

  gl->FramebufferTexture2D (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
      gst_gl_texture_target_to_gl (gl_mem->tex_target), gl_mem->tex_id, 0);

  if (!gst_gl_context_check_framebuffer_status (context, GL_FRAMEBUFFER)) {
    GST_CAT_WARNING (GST_CAT_GL_MEMORY,
        "Could not create framebuffer to read pixels for memory %p", gl_mem);
    gl->DeleteFramebuffers (1, &fbo);
    return FALSE;
  }

  if (USING_GLES2 (context) || USING_GLES3 (context)) {
    if (format == GL_RGBA && type == GL_UNSIGNED_BYTE) {
      /* explicitly supported */
    } else {
      gint supported_format, supported_type;

      gl->GetIntegerv (GL_IMPLEMENTATION_COLOR_READ_FORMAT, &supported_format);
      gl->GetIntegerv (GL_IMPLEMENTATION_COLOR_READ_TYPE, &supported_type);

      if (supported_format != format || supported_type != type) {
        GST_CAT_ERROR (GST_CAT_GL_MEMORY, "cannot read pixels with "
            "unsupported format and type.  Supported format 0x%x type 0x%x",
            supported_format, supported_type);
        gl->BindFramebuffer (GL_FRAMEBUFFER, 0);
        gl->DeleteFramebuffers (1, &fbo);
        return FALSE;
      }
    }
  }

  _gst_gl_memory_start_log (gl_mem, "glReadPixels");
  gl->ReadPixels (0, 0, gl_mem->tex_width, GL_MEM_HEIGHT (gl_mem), format,
      type, write_pointer);
  _gst_gl_memory_end_log (gl_mem);

  gl->BindFramebuffer (GL_FRAMEBUFFER, 0);
  gl->DeleteFramebuffers (1, &fbo);

  return TRUE;
}

static gpointer
_gl_tex_download_get_tex_image (GstGLMemory * gl_mem, GstMapInfo * info,
    gsize size)
{
  GstGLContext *context = gl_mem->mem.context;
  const GstGLFuncs *gl = context->gl_vtable;

  if (size != -1 && size != ((GstMemory *) gl_mem)->maxsize)
    return NULL;

  if (USING_GLES2 (context) || USING_GLES3 (context))
    return NULL;

  /* taken care of by read pixels */
  if (gl_mem->tex_format != GST_GL_LUMINANCE
      && gl_mem->tex_format != GST_GL_LUMINANCE_ALPHA)
    return NULL;

  if (info->flags & GST_MAP_READ
      && GST_MEMORY_FLAG_IS_SET (gl_mem,
          GST_GL_BASE_MEMORY_TRANSFER_NEED_DOWNLOAD)) {
    GstGLFormat format;
    guint type;
    guint target;

    GST_CAT_TRACE (GST_CAT_GL_MEMORY, "attempting download of texture %u "
        "using glGetTexImage", gl_mem->tex_id);

    gst_gl_format_type_from_sized_gl_format (gl_mem->tex_format, &format,
        &type);

    target = gst_gl_texture_target_to_gl (gl_mem->tex_target);
    gl->BindTexture (target, gl_mem->tex_id);
    _gst_gl_memory_start_log (gl_mem, "glGetTexImage");
    gl->GetTexImage (target, 0, format, type, gl_mem->mem.data);
    _gst_gl_memory_end_log (gl_mem);
    gl->BindTexture (target, 0);
  }

  return gl_mem->mem.data;
}

static gpointer
_gl_tex_download_read_pixels (GstGLMemory * gl_mem, GstMapInfo * info,
    gsize size)
{
  if (size != -1 && size != ((GstMemory *) gl_mem)->maxsize)
    return NULL;

  if (info->flags & GST_MAP_READ
      && GST_MEMORY_FLAG_IS_SET (gl_mem,
          GST_GL_BASE_MEMORY_TRANSFER_NEED_DOWNLOAD)) {
    GST_CAT_TRACE (GST_CAT_GL_MEMORY,
        "attempting download of texture %u " "using glReadPixels",
        gl_mem->tex_id);
    if (!gst_gl_memory_read_pixels (gl_mem, gl_mem->mem.data))
      return NULL;
  }

  return gl_mem->mem.data;
}

static gpointer
_gl_tex_map_cpu_access (GstGLMemory * gl_mem, GstMapInfo * info, gsize size)
{
  gpointer data = NULL;

  if (!gst_gl_base_memory_alloc_data (GST_GL_BASE_MEMORY_CAST (gl_mem)))
    return NULL;

  if (!data)
    data = _gl_tex_download_get_tex_image (gl_mem, info, size);

  if (!data)
    data = _gl_tex_download_read_pixels (gl_mem, info, size);

  return data;
}

static void
_upload_cpu_write (GstGLMemory * gl_mem, GstMapInfo * info, gsize maxsize)
{
  gst_gl_memory_texsubimage (gl_mem, gl_mem->mem.data);
}

/**
 * gst_gl_memory_texsubimage:
 * @gl_mem: a #GstGLMemory
 * @read_pointer: the data pointer to pass to glTexSubImage
 *
 * Reads the texture in @read_pointer into @gl_mem.
 *
 * See gst_gl_memory_read_pixels() for what @read_pointer signifies.
 *
 * Since: 1.8
 */
void
gst_gl_memory_texsubimage (GstGLMemory * gl_mem, gpointer read_pointer)
{
  GstGLContext *context = gl_mem->mem.context;
  const GstGLFuncs *gl;
  GstGLFormat gl_format;
  GLenum gl_type, gl_target;
  gpointer data;
  gsize plane_start;

  if (!GST_MEMORY_FLAG_IS_SET (gl_mem, GST_GL_BASE_MEMORY_TRANSFER_NEED_UPLOAD))
    return;

  gl = context->gl_vtable;

  gst_gl_format_type_from_sized_gl_format (gl_mem->tex_format, &gl_format,
      &gl_type);

  gl_target = gst_gl_texture_target_to_gl (gl_mem->tex_target);

  if (USING_OPENGL (context) || USING_GLES3 (context)
      || USING_OPENGL3 (context)) {
    gl->PixelStorei (GL_UNPACK_ROW_LENGTH, gl_mem->unpack_length);
  } else if (USING_GLES2 (context)) {
    gl->PixelStorei (GL_UNPACK_ALIGNMENT, gl_mem->unpack_length);
  }

  GST_CAT_LOG (GST_CAT_GL_MEMORY, "upload for texture id:%u, %ux%u",
      gl_mem->tex_id, gl_mem->tex_width, GL_MEM_HEIGHT (gl_mem));

  /* find the start of the plane data including padding */
  plane_start =
      gst_gl_get_plane_start (&gl_mem->info, &gl_mem->valign,
      gl_mem->plane) + gl_mem->mem.mem.offset;

  data = (gpointer) ((gintptr) plane_start + (gintptr) read_pointer);

  gl->BindTexture (gl_target, gl_mem->tex_id);
  _gst_gl_memory_start_log (gl_mem, "glTexSubImage");
  gl->TexSubImage2D (gl_target, 0, 0, 0, gl_mem->tex_width,
      GL_MEM_HEIGHT (gl_mem), gl_format, gl_type, data);
  _gst_gl_memory_end_log (gl_mem);

  /* Reset to default values */
  if (USING_OPENGL (context) || USING_GLES3 (context)
      || USING_OPENGL3 (context)) {
    gl->PixelStorei (GL_UNPACK_ROW_LENGTH, 0);
  } else if (USING_GLES2 (context)) {
    gl->PixelStorei (GL_UNPACK_ALIGNMENT, 4);
  }

  gl->BindTexture (gl_target, 0);
}

static gpointer
_default_gl_tex_map (GstGLMemory * gl_mem, GstMapInfo * info, gsize size)
{
  if ((info->flags & GST_MAP_GL) == GST_MAP_GL) {
    _upload_cpu_write (gl_mem, info, size);
    return &gl_mem->tex_id;
  } else {
    return _gl_tex_map_cpu_access (gl_mem, info, size);
  }
}

static gpointer
_gl_tex_map (GstGLMemory * gl_mem, GstMapInfo * info, gsize maxsize)
{
  GstGLMemoryAllocatorClass *alloc_class;
  gpointer data;

  alloc_class = GST_GL_MEMORY_ALLOCATOR_GET_CLASS (gl_mem->mem.mem.allocator);

  if ((info->flags & GST_MAP_GL) == GST_MAP_GL) {
    if (gl_mem->tex_target == GST_GL_TEXTURE_TARGET_EXTERNAL_OES)
      return &gl_mem->tex_id;
  } else {                      /* not GL */
    if (gl_mem->tex_target == GST_GL_TEXTURE_TARGET_EXTERNAL_OES) {
      GST_CAT_ERROR (GST_CAT_GL_MEMORY, "Cannot map External OES textures");
      return NULL;
    }
  }

  g_return_val_if_fail (alloc_class->map != NULL, NULL);
  data = alloc_class->map (GST_GL_BASE_MEMORY_CAST (gl_mem), info, maxsize);

  return data;
}

static void
_default_gl_tex_unmap (GstGLMemory * gl_mem, GstMapInfo * info)
{
}

static void
_gl_tex_unmap (GstGLMemory * gl_mem, GstMapInfo * info)
{
  GstGLMemoryAllocatorClass *alloc_class;

  alloc_class = GST_GL_MEMORY_ALLOCATOR_GET_CLASS (gl_mem->mem.mem.allocator);
  g_return_if_fail (alloc_class->unmap != NULL);

  alloc_class->unmap (GST_GL_BASE_MEMORY_CAST (gl_mem), info);
}

/**
 * gst_gl_memory_copy_teximage:
 * @src: the source #GstGLMemory
 * @tex_id: the destination texture id
 * @out_target: the destination #GstGLTextureTarget
 * @out_tex_format: the destination #GstGLFormat
 * @out_width: the destination width
 * @out_height: the destination height
 *
 * Copies the texture in #GstGLMemory into the texture specified by @tex_id,
 * @out_target, @out_tex_format, @out_width and @out_height.
 *
 * Returns: whether the copy succeeded.
 *
 * Since: 1.8
 */
gboolean
gst_gl_memory_copy_teximage (GstGLMemory * src, guint tex_id,
    GstGLTextureTarget out_target, GstGLFormat out_tex_format,
    gint out_width, gint out_height)
{
  const GstGLFuncs *gl;
  guint out_tex_target;
  GstMapInfo sinfo;
  guint src_tex_id;
  guint fbo[2];
  guint n_fbos;

  gl = src->mem.context->gl_vtable;
  out_tex_target = gst_gl_texture_target_to_gl (out_target);

  if (!gl->GenFramebuffers) {
    GST_CAT_ERROR (GST_CAT_GL_MEMORY, "Framebuffer objects not supported");
    goto error;
  }

  if (USING_GLES2 (src->mem.context)
      && (src->tex_format == GST_GL_LUMINANCE
          || src->tex_format == GST_GL_LUMINANCE_ALPHA)) {
    GST_CAT_FIXME (GST_CAT_GL_MEMORY,
        "Cannot copy Luminance/Luminance Alpha textures in GLES");
    goto error;
  }

  if (!gst_memory_map (GST_MEMORY_CAST (src), &sinfo,
          GST_MAP_READ | GST_MAP_GL)) {
    GST_CAT_ERROR (GST_CAT_GL_MEMORY,
        "Failed to map source memory for copying");
    goto error;
  }

  src_tex_id = *(guint *) sinfo.data;

  GST_CAT_LOG (GST_CAT_GL_MEMORY, "copying memory %p, tex %u into "
      "texture %i", src, src_tex_id, tex_id);

  /* FIXME: try and avoid creating and destroying fbo's every copy... */
  if (!gl->BlitFramebuffer || (!gl->DrawBuffer && !gl->DrawBuffers)
      || !gl->ReadBuffer) {
    /* create a framebuffer object */
    n_fbos = 1;
    gl->GenFramebuffers (n_fbos, &fbo[0]);
    gl->BindFramebuffer (GL_FRAMEBUFFER, fbo[0]);

    gl->FramebufferTexture2D (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
        gst_gl_texture_target_to_gl (src->tex_target), src_tex_id, 0);

    if (!gst_gl_context_check_framebuffer_status (src->mem.context,
            GL_FRAMEBUFFER))
      goto fbo_error;

    gl->BindTexture (out_tex_target, tex_id);
    _gst_gl_memory_start_log (src, "CopyTexImage2D");
    gl->CopyTexImage2D (out_tex_target, 0, out_tex_format, 0, 0, out_width,
        out_height, 0);
    _gst_gl_memory_end_log (src);

    gl->BindTexture (out_tex_target, 0);
    gl->BindFramebuffer (GL_FRAMEBUFFER, 0);

    gl->DeleteFramebuffers (n_fbos, &fbo[0]);
  } else {
    GLenum multipleRT[] = {
      GL_COLOR_ATTACHMENT0,
      GL_COLOR_ATTACHMENT1,
      GL_COLOR_ATTACHMENT2
    };

    /* create a framebuffer object */
    n_fbos = 2;
    gl->GenFramebuffers (n_fbos, &fbo[0]);

    gl->BindFramebuffer (GL_READ_FRAMEBUFFER, fbo[0]);
    gl->FramebufferTexture2D (GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
        gst_gl_texture_target_to_gl (src->tex_target), src_tex_id, 0);

    if (!gst_gl_context_check_framebuffer_status (src->mem.context,
            GL_READ_FRAMEBUFFER))
      goto fbo_error;

    gl->BindFramebuffer (GL_DRAW_FRAMEBUFFER, fbo[1]);

    gl->FramebufferTexture2D (GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
        gst_gl_texture_target_to_gl (src->tex_target), tex_id, 0);

    if (!gst_gl_context_check_framebuffer_status (src->mem.context,
            GL_DRAW_FRAMEBUFFER))
      goto fbo_error;

    gl->BindTexture (out_tex_target, tex_id);
    _gst_gl_memory_start_log (src, "BlitFramebuffer");
    gl->ReadBuffer (GL_COLOR_ATTACHMENT0);
    if (gl->DrawBuffers)
      gl->DrawBuffers (1, multipleRT);
    else
      gl->DrawBuffer (GL_COLOR_ATTACHMENT0);
    gl->BlitFramebuffer (0, 0, out_width, out_height,
        0, 0, out_width, out_height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    _gst_gl_memory_end_log (src);

    gl->BindTexture (out_tex_target, 0);
    gl->BindFramebuffer (GL_DRAW_FRAMEBUFFER, 0);
    gl->BindFramebuffer (GL_READ_FRAMEBUFFER, 0);

    gl->DeleteFramebuffers (n_fbos, &fbo[0]);

    if (gl->DrawBuffer)
      gl->DrawBuffer (GL_BACK);
  }

  gst_memory_unmap (GST_MEMORY_CAST (src), &sinfo);

  return TRUE;

fbo_error:
  {
    gl->BindTexture (out_tex_target, 0);
    if (!gl->BlitFramebuffer) {
      gl->BindFramebuffer (GL_FRAMEBUFFER, 0);
    } else {
      gl->BindFramebuffer (GL_DRAW_FRAMEBUFFER, 0);
      gl->BindFramebuffer (GL_READ_FRAMEBUFFER, 0);
    }

    gl->DeleteFramebuffers (n_fbos, &fbo[0]);

    gst_memory_unmap (GST_MEMORY_CAST (src), &sinfo);
  }
error:
  return FALSE;
}

static void
_gl_tex_copy_thread (GstGLContext * context, gpointer data)
{
  GstGLMemoryCopyParams *copy_params;

  copy_params = (GstGLMemoryCopyParams *) data;

  if (!copy_params->tex_id) {
    GstGLFormat internal_format, out_gl_format;
    guint out_gl_type, out_tex_target;

    out_tex_target = gst_gl_texture_target_to_gl (copy_params->tex_target);
    internal_format = copy_params->src->tex_format;
    gst_gl_format_type_from_sized_gl_format (internal_format, &out_gl_format,
        &out_gl_type);
    internal_format =
        gst_gl_sized_gl_format_from_gl_format_type (context, out_gl_format,
        out_gl_type);

    copy_params->tex_id =
        _new_texture (context, out_tex_target,
        internal_format, out_gl_format, out_gl_type, copy_params->out_width,
        copy_params->out_height);
  }

  copy_params->result = gst_gl_memory_copy_teximage (copy_params->src,
      copy_params->tex_id, copy_params->tex_target, copy_params->tex_format,
      copy_params->out_width, copy_params->out_height);
}

static GstMemory *
_default_gl_tex_copy (GstGLMemory * src, gssize offset, gssize size)
{
  GstAllocationParams params = { 0, GST_MEMORY_CAST (src)->align, 0, 0 };
  GstGLBaseMemoryAllocator *base_mem_allocator;
  GstAllocator *allocator;
  GstGLMemory *dest = NULL;

  allocator = GST_MEMORY_CAST (src)->allocator;
  base_mem_allocator = (GstGLBaseMemoryAllocator *) allocator;

  if (src->tex_target == GST_GL_TEXTURE_TARGET_EXTERNAL_OES) {
    GST_CAT_ERROR (GST_CAT_GL_MEMORY, "Cannot copy External OES textures");
    return NULL;
  }

  /* If not doing a full copy, then copy to sysmem, the 2D represention of the
   * texture would become wrong */
  if (offset > 0 || size < GST_MEMORY_CAST (src)->size) {
    return base_mem_allocator->fallback_mem_copy (GST_MEMORY_CAST (src), offset,
        size);
  }

  dest = g_new0 (GstGLMemory, 1);

  gst_gl_memory_init (dest, allocator, NULL, src->mem.context, src->tex_target,
      src->tex_format, &params, &src->info, src->plane, &src->valign, NULL,
      NULL);

  if (!GST_MEMORY_FLAG_IS_SET (src, GST_GL_BASE_MEMORY_TRANSFER_NEED_UPLOAD)) {
    GstMapInfo dinfo;

    if (!gst_memory_map (GST_MEMORY_CAST (dest), &dinfo,
            GST_MAP_WRITE | GST_MAP_GL)) {
      GST_CAT_WARNING (GST_CAT_GL_MEMORY,
          "Failed not map destination for writing");
      gst_memory_unref (GST_MEMORY_CAST (dest));
      return NULL;
    }

    if (!gst_gl_memory_copy_into ((GstGLMemory *) src,
            ((GstGLMemory *) dest)->tex_id, src->tex_target,
            src->tex_format, src->tex_width, GL_MEM_HEIGHT (src))) {
      GST_CAT_WARNING (GST_CAT_GL_MEMORY, "Could not copy GL Memory");
      gst_memory_unmap (GST_MEMORY_CAST (dest), &dinfo);
      goto memcpy;
    }

    gst_memory_unmap (GST_MEMORY_CAST (dest), &dinfo);
  } else {
  memcpy:
    if (!gst_gl_base_memory_memcpy ((GstGLBaseMemory *) src,
            (GstGLBaseMemory *) dest, offset, size)) {
      GST_CAT_WARNING (GST_CAT_GL_MEMORY, "Could not copy GL Memory");
      gst_memory_unref (GST_MEMORY_CAST (dest));
      return NULL;
    }
  }

  return (GstMemory *) dest;
}

static GstMemory *
_gl_tex_copy (GstGLMemory * src, gssize offset, gssize size)
{
  GstGLMemoryAllocatorClass *alloc_class;

  alloc_class = GST_GL_MEMORY_ALLOCATOR_GET_CLASS (src->mem.mem.allocator);

  if (src->tex_target == GST_GL_TEXTURE_TARGET_EXTERNAL_OES) {
    GST_CAT_ERROR (GST_CAT_GL_MEMORY, "Cannot copy External OES textures");
    return NULL;
  }

  g_return_val_if_fail (alloc_class->copy, NULL);
  return (GstMemory *) alloc_class->copy (GST_GL_BASE_MEMORY_CAST (src), offset,
      size);
}

static GstMemory *
_gl_tex_alloc (GstAllocator * allocator, gsize size,
    GstAllocationParams * params)
{
  g_warning ("Use gst_gl_base_memory_alloc to allocate from this allocator");

  return NULL;
}

static void
_gl_tex_destroy (GstGLMemory * gl_mem)
{
  const GstGLFuncs *gl = gl_mem->mem.context->gl_vtable;

  if (gl_mem->tex_id && !gl_mem->texture_wrapped)
    gl->DeleteTextures (1, &gl_mem->tex_id);
}

static GstGLMemory *
_default_gl_tex_alloc (GstGLMemoryAllocator * allocator,
    GstGLVideoAllocationParams * params)
{
  guint alloc_flags = params->parent.alloc_flags;
  GstGLMemory *mem;

  g_return_val_if_fail (alloc_flags & GST_GL_ALLOCATION_PARAMS_ALLOC_FLAG_VIDEO,
      NULL);

  mem = g_new0 (GstGLMemory, 1);

  if (alloc_flags & GST_GL_ALLOCATION_PARAMS_ALLOC_FLAG_WRAP_GPU_HANDLE) {
    mem->tex_id = GPOINTER_TO_UINT (params->parent.gl_handle);
    mem->texture_wrapped = TRUE;
  }

  gst_gl_memory_init (mem, GST_ALLOCATOR_CAST (allocator), NULL,
      params->parent.context, params->target, params->tex_format,
      params->parent.alloc_params, params->v_info, params->plane,
      params->valign, params->parent.user_data, params->parent.notify);

  if (alloc_flags & GST_GL_ALLOCATION_PARAMS_ALLOC_FLAG_WRAP_GPU_HANDLE) {
    GST_MINI_OBJECT_FLAG_SET (mem, GST_GL_BASE_MEMORY_TRANSFER_NEED_DOWNLOAD);
  }
  if (alloc_flags & GST_GL_ALLOCATION_PARAMS_ALLOC_FLAG_WRAP_SYSMEM) {
    mem->mem.data = params->parent.wrapped_data;
    GST_MINI_OBJECT_FLAG_SET (mem, GST_GL_BASE_MEMORY_TRANSFER_NEED_UPLOAD);
  }

  return mem;
}

static void
gst_gl_memory_allocator_class_init (GstGLMemoryAllocatorClass * klass)
{
  GstGLBaseMemoryAllocatorClass *gl_base;
  GstAllocatorClass *allocator_class;

  gl_base = (GstGLBaseMemoryAllocatorClass *) klass;
  allocator_class = (GstAllocatorClass *) klass;

  klass->map = (GstGLBaseMemoryAllocatorMapFunction) _default_gl_tex_map;
  klass->unmap = (GstGLBaseMemoryAllocatorUnmapFunction) _default_gl_tex_unmap;
  klass->copy = (GstGLBaseMemoryAllocatorCopyFunction) _default_gl_tex_copy;

  gl_base->alloc =
      (GstGLBaseMemoryAllocatorAllocFunction) _default_gl_tex_alloc;
  gl_base->create = (GstGLBaseMemoryAllocatorCreateFunction) _gl_tex_create;
  gl_base->map = (GstGLBaseMemoryAllocatorMapFunction) _gl_tex_map;
  gl_base->unmap = (GstGLBaseMemoryAllocatorUnmapFunction) _gl_tex_unmap;
  gl_base->copy = (GstGLBaseMemoryAllocatorCopyFunction) _gl_tex_copy;
  gl_base->destroy = (GstGLBaseMemoryAllocatorDestroyFunction) _gl_tex_destroy;

  allocator_class->alloc = _gl_tex_alloc;
}

static void
gst_gl_memory_allocator_init (GstGLMemoryAllocator * allocator)
{
  GstAllocator *alloc = GST_ALLOCATOR_CAST (allocator);

  alloc->mem_type = GST_GL_MEMORY_ALLOCATOR_NAME;

  GST_OBJECT_FLAG_SET (allocator, GST_ALLOCATOR_FLAG_CUSTOM_ALLOC);
}

/**
 * gst_gl_memory_copy_into:
 * @gl_mem:a #GstGLMemory
 * @tex_id:OpenGL texture id
 * @target: the #GstGLTextureTarget
 * @tex_format: the #GstGLFormat
 * @width: width of @tex_id
 * @height: height of @tex_id
 *
 * Copies @gl_mem into the texture specified by @tex_id.  The format of @tex_id
 * is specified by @tex_format, @width and @height.
 *
 * Returns: Whether the copy succeeded
 *
 * Since: 1.8
 */
gboolean
gst_gl_memory_copy_into (GstGLMemory * gl_mem, guint tex_id,
    GstGLTextureTarget target, GstGLFormat tex_format, gint width, gint height)
{
  GstGLMemoryCopyParams copy_params;

  copy_params.src = gl_mem;
  copy_params.tex_id = tex_id;
  copy_params.tex_target = target;
  copy_params.tex_format = tex_format;
  copy_params.out_width = width;
  copy_params.out_height = height;

  gst_gl_context_thread_add (gl_mem->mem.context, _gl_tex_copy_thread,
      &copy_params);

  return copy_params.result;
}

/**
 * gst_gl_memory_get_texture_width:
 * @gl_mem: a #GstGLMemory
 *
 * Returns: the texture width of @gl_mem
 *
 * Since: 1.8
 */
gint
gst_gl_memory_get_texture_width (GstGLMemory * gl_mem)
{
  g_return_val_if_fail (gst_is_gl_memory ((GstMemory *) gl_mem), 0);

  return gl_mem->tex_width;
}

/**
 * gst_gl_memory_get_texture_height:
 * @gl_mem: a #GstGLMemory
 *
 * Returns: the texture height of @gl_mem
 *
 * Since: 1.8
 */
gint
gst_gl_memory_get_texture_height (GstGLMemory * gl_mem)
{
  g_return_val_if_fail (gst_is_gl_memory ((GstMemory *) gl_mem), 0);

  return _get_plane_height (&gl_mem->info, gl_mem->plane);
}

/**
 * gst_gl_memory_get_texture_format:
 * @gl_mem: a #GstGLMemory
 *
 * Returns: the #GstGLFormat of @gl_mem
 *
 * Since: 1.12
 */
GstGLFormat
gst_gl_memory_get_texture_format (GstGLMemory * gl_mem)
{
  g_return_val_if_fail (gst_is_gl_memory ((GstMemory *) gl_mem), 0);

  return gl_mem->tex_format;
}

/**
 * gst_gl_memory_get_texture_target:
 * @gl_mem: a #GstGLMemory
 *
 * Returns: the #GstGLTextureTarget of @gl_mem
 *
 * Since: 1.8
 */
GstGLTextureTarget
gst_gl_memory_get_texture_target (GstGLMemory * gl_mem)
{
  g_return_val_if_fail (gst_is_gl_memory ((GstMemory *) gl_mem), 0);

  return gl_mem->tex_target;
}

/**
 * gst_gl_memory_get_texture_id:
 * @gl_mem: a #GstGLMemory
 *
 * Returns: the OpenGL texture handle of @gl_mem
 *
 * Since: 1.8
 */
guint
gst_gl_memory_get_texture_id (GstGLMemory * gl_mem)
{
  g_return_val_if_fail (gst_is_gl_memory ((GstMemory *) gl_mem), 0);

  return gl_mem->tex_id;
}

/**
 * gst_gl_memory_init_once:
 *
 * Initializes the GL Base Texture allocator. It is safe to call this function
 * multiple times.  This must be called before any other GstGLMemory operation.
 *
 * Since: 1.4
 */
void
gst_gl_memory_init_once (void)
{
  static gsize _init = 0;

  if (g_once_init_enter (&_init)) {
    gst_gl_base_memory_init_once ();

    GST_DEBUG_CATEGORY_INIT (GST_CAT_GL_MEMORY, "glbasetexture", 0,
        "OpenGL Base Texture Memory");

    _gl_memory_allocator = g_object_new (GST_TYPE_GL_MEMORY_ALLOCATOR, NULL);
    gst_object_ref_sink (_gl_memory_allocator);

    gst_allocator_register (GST_GL_MEMORY_ALLOCATOR_NAME, _gl_memory_allocator);

    g_once_init_leave (&_init, 1);
  }
}

/**
 * gst_is_gl_memory:
 * @mem:a #GstMemory
 *
 * Returns: whether the memory at @mem is a #GstGLMemory
 *
 * Since: 1.4
 */
gboolean
gst_is_gl_memory (GstMemory * mem)
{
  return mem != NULL && mem->allocator != NULL
      && g_type_is_a (G_OBJECT_TYPE (mem->allocator),
      GST_TYPE_GL_MEMORY_ALLOCATOR);
}

G_DEFINE_BOXED_TYPE (GstGLVideoAllocationParams, gst_gl_video_allocation_params,
    (GBoxedCopyFunc) gst_gl_allocation_params_copy,
    (GBoxedFreeFunc) gst_gl_allocation_params_free);

static void
_gst_gl_video_allocation_params_set_video_alignment (GstGLVideoAllocationParams
    * params, const GstVideoAlignment * valign)
{
  g_return_if_fail (params != NULL);

  if (!params->valign)
    params->valign = g_new0 (GstVideoAlignment, 1);

  if (valign) {
    *params->valign = *valign;
  } else {
    gst_video_alignment_reset (params->valign);
  }
}

/**
 * gst_gl_video_allocation_params_init_full: (skip)
 * @params: a #GstGLVideoAllocationParams to initialize
 * @struct_size: the size of the struct in @params
 * @alloc_flags: some allocation flags
 * @copy: a copy function
 * @free: a free function
 * @context: a #GstGLContext
 * @alloc_params: (allow-none): the #GstAllocationParams for @wrapped_data
 * @v_info: the #GstVideoInfo for @wrapped_data
 * @plane: the video plane @wrapped_data represents
 * @valign: (allow-none): any #GstVideoAlignment applied to symem mappings of @wrapped_data
 * @target: the #GstGLTextureTarget
 * @tex_format: the #GstGLFormat
 * @wrapped_data: (allow-none): the optional data pointer to wrap
 * @gl_handle: the optional OpenGL handle to wrap or 0
 * @user_data: (allow-none): user data to call @notify with
 * @notify: (allow-none): a #GDestroyNotify
 *
 * Intended for subclass usage
 *
 * Returns: initializes @params with the parameters specified
 *
 * Since: 1.8
 */
gboolean
gst_gl_video_allocation_params_init_full (GstGLVideoAllocationParams * params,
    gsize struct_size, guint alloc_flags, GstGLAllocationParamsCopyFunc copy,
    GstGLAllocationParamsFreeFunc free, GstGLContext * context,
    const GstAllocationParams * alloc_params, const GstVideoInfo * v_info,
    guint plane, const GstVideoAlignment * valign, GstGLTextureTarget target,
    GstGLFormat tex_format, gpointer wrapped_data, gpointer gl_handle,
    gpointer user_data, GDestroyNotify notify)
{
  guint i;

  g_return_val_if_fail (params != NULL, FALSE);
  g_return_val_if_fail (copy != NULL, FALSE);
  g_return_val_if_fail (free != NULL, FALSE);
  g_return_val_if_fail (GST_IS_GL_CONTEXT (context), FALSE);
  g_return_val_if_fail (v_info != NULL, FALSE);

  memset (params, 0, sizeof (*params));

  if (!gst_gl_allocation_params_init ((GstGLAllocationParams *) params,
          struct_size, alloc_flags, copy, free, context, 0, alloc_params,
          wrapped_data, gl_handle, user_data, notify))
    return FALSE;

  params->v_info = g_new0 (GstVideoInfo, 1);
  *params->v_info = *v_info;
  for (i = 0; i < GST_VIDEO_MAX_PLANES; i++) {
    params->v_info->offset[i] = v_info->offset[i];
    params->v_info->stride[i] = v_info->stride[i];
  }
  _gst_gl_video_allocation_params_set_video_alignment (params, valign);
  params->target = target;
  params->tex_format = tex_format;
  params->plane = plane;

  return TRUE;
}

/**
 * gst_gl_video_allocation_params_new:
 * @context: a #GstGLContext
 * @alloc_params: (allow-none): the #GstAllocationParams for sysmem mappings of the texture
 * @v_info: the #GstVideoInfo for the texture
 * @plane: the video plane of @v_info to allocate
 * @valign: (allow-none): any #GstVideoAlignment applied to symem mappings of the texture
 * @target: the #GstGLTextureTarget for the created textures
 * @tex_format: the #GstGLFormat for the created textures
 *
 * Returns: a new #GstGLVideoAllocationParams for allocating #GstGLMemory's
 *
 * Since: 1.8
 */
GstGLVideoAllocationParams *
gst_gl_video_allocation_params_new (GstGLContext * context,
    const GstAllocationParams * alloc_params, const GstVideoInfo * v_info,
    guint plane, const GstVideoAlignment * valign, GstGLTextureTarget target,
    GstGLFormat tex_format)
{
  GstGLVideoAllocationParams *params = g_new0 (GstGLVideoAllocationParams, 1);

  if (!gst_gl_video_allocation_params_init_full (params,
          sizeof (GstGLVideoAllocationParams),
          GST_GL_ALLOCATION_PARAMS_ALLOC_FLAG_ALLOC |
          GST_GL_ALLOCATION_PARAMS_ALLOC_FLAG_VIDEO,
          (GstGLAllocationParamsCopyFunc)
          gst_gl_video_allocation_params_copy_data,
          (GstGLAllocationParamsFreeFunc)
          gst_gl_video_allocation_params_free_data, context, alloc_params,
          v_info, plane, valign, target, tex_format, NULL, 0, NULL, NULL)) {
    g_free (params);
    return NULL;
  }

  return params;
}

/**
 * gst_gl_video_allocation_params_new_wrapped_data:
 * @context: a #GstGLContext
 * @alloc_params: (allow-none): the #GstAllocationParams for @wrapped_data
 * @v_info: the #GstVideoInfo for @wrapped_data
 * @plane: the video plane @wrapped_data represents
 * @valign: (allow-none): any #GstVideoAlignment applied to symem mappings of @wrapped_data
 * @target: the #GstGLTextureTarget for @wrapped_data
 * @tex_format: the #GstGLFormat for @wrapped_data
 * @wrapped_data: the data pointer to wrap
 * @user_data: (allow-none): user data to call @notify with
 * @notify: (allow-none): a #GDestroyNotify
 *
 * Returns: a new #GstGLVideoAllocationParams for wrapping @wrapped_data
 *
 * Since: 1.8
 */
GstGLVideoAllocationParams *
gst_gl_video_allocation_params_new_wrapped_data (GstGLContext * context,
    const GstAllocationParams * alloc_params, const GstVideoInfo * v_info,
    guint plane, const GstVideoAlignment * valign, GstGLTextureTarget target,
    GstGLFormat tex_format, gpointer wrapped_data, gpointer user_data,
    GDestroyNotify notify)
{
  GstGLVideoAllocationParams *params = g_new0 (GstGLVideoAllocationParams, 1);

  if (!gst_gl_video_allocation_params_init_full (params,
          sizeof (GstGLVideoAllocationParams),
          GST_GL_ALLOCATION_PARAMS_ALLOC_FLAG_WRAP_SYSMEM |
          GST_GL_ALLOCATION_PARAMS_ALLOC_FLAG_VIDEO,
          (GstGLAllocationParamsCopyFunc)
          gst_gl_video_allocation_params_copy_data,
          (GstGLAllocationParamsFreeFunc)
          gst_gl_video_allocation_params_free_data, context, alloc_params,
          v_info, plane, valign, target, tex_format, wrapped_data, 0, user_data,
          notify)) {
    g_free (params);
    return NULL;
  }

  return params;
}

/**
 * gst_gl_video_allocation_params_new_wrapped_gl_handle:
 * @context: a #GstGLContext
 * @alloc_params: (allow-none): the #GstAllocationParams for @tex_id
 * @v_info: the #GstVideoInfo for @tex_id
 * @plane: the video plane @tex_id represents
 * @valign: (allow-none): any #GstVideoAlignment applied to symem mappings of @tex_id
 * @target: the #GstGLTextureTarget for @tex_id
 * @tex_format: the #GstGLFormat for @tex_id
 * @gl_handle: the GL handle to wrap
 * @user_data: (allow-none): user data to call @notify with
 * @notify: (allow-none): a #GDestroyNotify
 *
 * @gl_handle is defined by the specific OpenGL handle being wrapped
 * For #GstGLMemory and #GstGLMemoryPBO it is an OpenGL texture id.
 * Other memory types may define it to require a different type of parameter.
 *
 * Returns: a new #GstGLVideoAllocationParams for wrapping @gl_handle
 *
 * Since: 1.8
 */
GstGLVideoAllocationParams *
gst_gl_video_allocation_params_new_wrapped_gl_handle (GstGLContext * context,
    const GstAllocationParams * alloc_params, const GstVideoInfo * v_info,
    guint plane, const GstVideoAlignment * valign, GstGLTextureTarget target,
    GstGLFormat tex_format, gpointer gl_handle, gpointer user_data,
    GDestroyNotify notify)
{
  GstGLVideoAllocationParams *params = g_new0 (GstGLVideoAllocationParams, 1);

  if (!gst_gl_video_allocation_params_init_full (params,
          sizeof (GstGLVideoAllocationParams),
          GST_GL_ALLOCATION_PARAMS_ALLOC_FLAG_WRAP_GPU_HANDLE |
          GST_GL_ALLOCATION_PARAMS_ALLOC_FLAG_VIDEO,
          (GstGLAllocationParamsCopyFunc)
          gst_gl_video_allocation_params_copy_data,
          (GstGLAllocationParamsFreeFunc)
          gst_gl_video_allocation_params_free_data, context, alloc_params,
          v_info, plane, valign, target, tex_format, NULL, gl_handle, user_data,
          notify)) {
    g_free (params);
    return NULL;
  }

  return params;
}

/**
 * gst_gl_video_allocation_params_new_wrapped_texture:
 * @context: a #GstGLContext
 * @alloc_params: (allow-none): the #GstAllocationParams for @tex_id
 * @v_info: the #GstVideoInfo for @tex_id
 * @plane: the video plane @tex_id represents
 * @valign: (allow-none): any #GstVideoAlignment applied to symem mappings of @tex_id
 * @target: the #GstGLTextureTarget for @tex_id
 * @tex_format: the #GstGLFormat for @tex_id
 * @tex_id: the GL texture to wrap
 * @user_data: (allow-none): user data to call @notify with
 * @notify: (allow-none): a #GDestroyNotify
 *
 * Returns: a new #GstGLVideoAllocationParams for wrapping @tex_id
 *
 * Since: 1.8
 */
GstGLVideoAllocationParams *
gst_gl_video_allocation_params_new_wrapped_texture (GstGLContext * context,
    const GstAllocationParams * alloc_params, const GstVideoInfo * v_info,
    guint plane, const GstVideoAlignment * valign, GstGLTextureTarget target,
    GstGLFormat tex_format, guint tex_id, gpointer user_data,
    GDestroyNotify notify)
{
  return gst_gl_video_allocation_params_new_wrapped_gl_handle (context,
      alloc_params, v_info, plane, valign, target, tex_format,
      GUINT_TO_POINTER (tex_id), user_data, notify);
}

/**
 * gst_gl_video_allocation_params_free_data:
 * @params: a #GstGLVideoAllocationParams
 *
 * Unset and free any dynamically allocated resources.  Intended for subclass
 * usage only to chain up at the end of a subclass free function.
 *
 * Since: 1.8
 */
void
gst_gl_video_allocation_params_free_data (GstGLVideoAllocationParams * params)
{
  g_free (params->v_info);
  g_free (params->valign);

  gst_gl_allocation_params_free_data (&params->parent);
}

/**
 * gst_gl_video_allocation_params_copy_data:
 * @src_vid: source #GstGLVideoAllocationParams to copy from
 * @dest_vid: destination #GstGLVideoAllocationParams to copy into
 *
 * Copy and set any dynamically allocated resources in @dest_vid.  Intended
 * for subclass usage only to chain up at the end of a subclass copy function.
 *
 * Since: 1.8
 */
void
gst_gl_video_allocation_params_copy_data (GstGLVideoAllocationParams * src_vid,
    GstGLVideoAllocationParams * dest_vid)
{
  GstGLAllocationParams *src = (GstGLAllocationParams *) src_vid;
  GstGLAllocationParams *dest = (GstGLAllocationParams *) dest_vid;
  guint i;

  gst_gl_allocation_params_copy_data (src, dest);

  dest_vid->v_info = g_new0 (GstVideoInfo, 1);
  *dest_vid->v_info = *src_vid->v_info;
  for (i = 0; i < GST_VIDEO_MAX_PLANES; i++) {
    dest_vid->v_info->offset[i] = src_vid->v_info->offset[i];
    dest_vid->v_info->stride[i] = src_vid->v_info->stride[i];
  }
  _gst_gl_video_allocation_params_set_video_alignment (dest_vid,
      src_vid->valign);
  dest_vid->target = src_vid->target;
  dest_vid->tex_format = src_vid->tex_format;
  dest_vid->plane = src_vid->plane;
}

/**
 * gst_gl_memory_setup_buffer: (skip)
 * @allocator: the @GstGLMemoryAllocator to allocate from
 * @buffer: a #GstBuffer to setup
 * @params: the #GstGLVideoAllocationParams to allocate with
 * @tex_formats: (allow-none) (array length=n_wrapped_pointers):
 *     a list of #GstGLFormat's to allocate with.
 * @wrapped_data: (array length=n_wrapped_pointers) (element-type gpointer):
 *     a list of wrapped data pointers
 * @n_wrapped_pointers: the number of elements in @tex_formats and @wrapped_data
 *
 * Returns: whether the buffer was correctly setup
 *
 * Since: 1.8
 */
gboolean
gst_gl_memory_setup_buffer (GstGLMemoryAllocator * allocator,
    GstBuffer * buffer, GstGLVideoAllocationParams * params,
    GstGLFormat * tex_formats, gpointer * wrapped_data,
    gsize n_wrapped_pointers)
{
  GstGLBaseMemoryAllocator *base_allocator;
  guint n_mem, i, v, views;
  guint alloc_flags = params->parent.alloc_flags;

  g_return_val_if_fail (params != NULL, FALSE);
  g_return_val_if_fail (alloc_flags & GST_GL_ALLOCATION_PARAMS_ALLOC_FLAG_VIDEO,
      FALSE);

  base_allocator = GST_GL_BASE_MEMORY_ALLOCATOR (allocator);
  n_mem = GST_VIDEO_INFO_N_PLANES (params->v_info);

  if (GST_VIDEO_INFO_MULTIVIEW_MODE (params->v_info) ==
      GST_VIDEO_MULTIVIEW_MODE_SEPARATED)
    views = params->v_info->views;
  else
    views = 1;

  if (n_wrapped_pointers == views)
    n_mem = 1;

  /* Sanity check for the code below; there should be as many pointers as the
   * number of memory we are going to create */
  g_return_val_if_fail (!wrapped_data
      || n_mem * views == n_wrapped_pointers, FALSE);

  for (v = 0; v < views; v++) {
    GstVideoMeta *meta;

    for (i = 0; i < n_mem; i++) {
      GstGLMemory *gl_mem;

      if (tex_formats) {
        params->tex_format = tex_formats[i];
      } else {
        params->tex_format =
            gst_gl_format_from_video_info (params->parent.context,
            params->v_info, i);
      }

      params->plane = i;
      if (alloc_flags & GST_GL_ALLOCATION_PARAMS_ALLOC_FLAG_WRAP_SYSMEM) {
        g_return_val_if_fail (wrapped_data != NULL, FALSE);
        params->parent.wrapped_data = wrapped_data[i];
      } else if (alloc_flags &
          GST_GL_ALLOCATION_PARAMS_ALLOC_FLAG_WRAP_GPU_HANDLE) {
        g_return_val_if_fail (wrapped_data != NULL, FALSE);
        params->parent.gl_handle = wrapped_data[i];
      }

      if (!(gl_mem = (GstGLMemory *) gst_gl_base_memory_alloc (base_allocator,
                  (GstGLAllocationParams *) params)))
        return FALSE;

      gst_buffer_append_memory (buffer, (GstMemory *) gl_mem);
    }

    meta = gst_buffer_add_video_meta_full (buffer, v,
        GST_VIDEO_INFO_FORMAT (params->v_info),
        GST_VIDEO_INFO_WIDTH (params->v_info),
        GST_VIDEO_INFO_HEIGHT (params->v_info), n_mem, params->v_info->offset,
        params->v_info->stride);

    if (params->valign)
      gst_video_meta_set_alignment (meta, *params->valign);
  }

  return TRUE;
}

/**
 * gst_gl_memory_allocator_get_default:
 * @context: a #GstGLContext
 *
 * Returns: (transfer full): the default #GstGLMemoryAllocator supported by
 *          @context
 *
 * Since: 1.8
 */
GstGLMemoryAllocator *
gst_gl_memory_allocator_get_default (GstGLContext * context)
{
  GstGLMemoryAllocator *allocator = NULL;

  g_return_val_if_fail (GST_IS_GL_CONTEXT (context), NULL);

  /* we can only use the pbo allocator with GL > 3.0 contexts */
  if (gst_gl_context_check_gl_version (context,
          GST_GL_API_OPENGL | GST_GL_API_OPENGL3 | GST_GL_API_GLES2, 3, 0)) {
    allocator = (GstGLMemoryAllocator *)
        gst_allocator_find (GST_GL_MEMORY_PBO_ALLOCATOR_NAME);
  } else {
    allocator = (GstGLMemoryAllocator *)
        gst_allocator_find (GST_GL_MEMORY_ALLOCATOR_NAME);
  }

  return allocator;
}
