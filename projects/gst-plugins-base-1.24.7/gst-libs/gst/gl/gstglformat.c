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

/**
 * SECTION:gstglformat
 * @title: GstGLFormat
 * @short_description: utilities for dealing with OpenGL formats
 * @see_also: #GstGLBaseMemory, #GstGLMemory, #GstGLFramebuffer, #GstGLBuffer
 *
 * Some useful utilities for converting between various formats and OpenGL
 * formats.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstglformat.h"

#include "gstglcontext.h"
#include "gstglfuncs.h"

#define USING_OPENGL(context) (gst_gl_context_check_gl_version (context, GST_GL_API_OPENGL, 1, 0))
#define USING_OPENGL3(context) (gst_gl_context_check_gl_version (context, GST_GL_API_OPENGL3, 3, 1))
#define USING_GLES(context) (gst_gl_context_check_gl_version (context, GST_GL_API_GLES, 1, 0))
#define USING_GLES2(context) (gst_gl_context_check_gl_version (context, GST_GL_API_GLES2, 2, 0))
#define USING_GLES3(context) (gst_gl_context_check_gl_version (context, GST_GL_API_GLES2, 3, 0))

#ifndef GL_TEXTURE_RECTANGLE
#define GL_TEXTURE_RECTANGLE 0x84F5
#endif
#ifndef GL_TEXTURE_EXTERNAL_OES
#define GL_TEXTURE_EXTERNAL_OES 0x8D65
#endif
#ifndef GL_UNSIGNED_INT_2_10_10_10_REV
#define GL_UNSIGNED_INT_2_10_10_10_REV 0x8368
#endif

static inline guint
_gl_format_n_components (guint format)
{
  switch (format) {
    case GST_VIDEO_GL_TEXTURE_TYPE_RGBA:
    case GST_GL_RGBA:
    case GST_GL_RGBA8:
    case GST_GL_RGBA16:
    case GST_GL_RGB10_A2:
      return 4;
    case GST_VIDEO_GL_TEXTURE_TYPE_RGB:
    case GST_VIDEO_GL_TEXTURE_TYPE_RGB16:
    case GST_GL_RGB:
    case GST_GL_RGB8:
    case GST_GL_RGB16:
    case GST_GL_RGB565:
      return 3;
    case GST_VIDEO_GL_TEXTURE_TYPE_LUMINANCE_ALPHA:
    case GST_VIDEO_GL_TEXTURE_TYPE_RG:
    case GST_GL_LUMINANCE_ALPHA:
    case GST_GL_RG:
    case GST_GL_RG8:
    case GST_GL_RG16:
      return 2;
    case GST_VIDEO_GL_TEXTURE_TYPE_LUMINANCE:
    case GST_VIDEO_GL_TEXTURE_TYPE_R:
    case GST_GL_LUMINANCE:
    case GST_GL_ALPHA:
    case GST_GL_RED:
    case GST_GL_R8:
    case GST_GL_R16:
      return 1;
    default:
      return 0;
  }
}

static inline guint
_gl_type_n_components (guint type)
{
  switch (type) {
    case GL_UNSIGNED_BYTE:
    case GL_UNSIGNED_SHORT:
      return 1;
    case GL_UNSIGNED_SHORT_5_6_5:
      return 3;
    case GL_UNSIGNED_INT_2_10_10_10_REV:
      return 4;
    default:
      g_assert_not_reached ();
      return 0;
  }
}

static inline guint
_gl_type_n_bytes (guint type)
{
  switch (type) {
    case GL_UNSIGNED_BYTE:
      return 1;
    case GL_UNSIGNED_SHORT:
    case GL_UNSIGNED_SHORT_5_6_5:
      return 2;
    case GL_UNSIGNED_INT_2_10_10_10_REV:
      return 4;
    default:
      g_assert_not_reached ();
      return 0;
  }
}

/**
 * gst_gl_format_type_n_bytes:
 * @format: the OpenGL format, `GL_RGBA`, `GL_LUMINANCE`, etc
 * @type: the OpenGL type, `GL_UNSIGNED_BYTE`, `GL_FLOAT`, etc
 *
 * Returns: the number of bytes the specified @format, @type combination takes
 * per pixel
 */
guint
gst_gl_format_type_n_bytes (guint format, guint type)
{
  return _gl_format_n_components (format) / _gl_type_n_components (type) *
      _gl_type_n_bytes (type);
}

/**
 * gst_gl_format_from_video_info:
 * @context: a #GstGLContext
 * @vinfo: a #GstVideoInfo
 * @plane: the plane number in @vinfo
 *
 * Returns: the #GstGLFormat necessary for holding the data in @plane of @vinfo
 */
GstGLFormat
gst_gl_format_from_video_info (GstGLContext * context,
    const GstVideoInfo * vinfo, guint plane)
{
  gboolean texture_rg =
      gst_gl_context_check_feature (context, "GL_EXT_texture_rg")
      || gst_gl_context_check_gl_version (context, GST_GL_API_GLES2, 3, 0)
      || gst_gl_context_check_feature (context, "GL_ARB_texture_rg")
      || gst_gl_context_check_gl_version (context, GST_GL_API_OPENGL3, 3, 0);
  GstVideoFormat v_format = GST_VIDEO_INFO_FORMAT (vinfo);
  guint n_plane_components;

  switch (v_format) {
    case GST_VIDEO_FORMAT_RGBx:
    case GST_VIDEO_FORMAT_BGRx:
    case GST_VIDEO_FORMAT_xRGB:
    case GST_VIDEO_FORMAT_xBGR:
    case GST_VIDEO_FORMAT_RGBA:
    case GST_VIDEO_FORMAT_BGRA:
    case GST_VIDEO_FORMAT_ARGB:
    case GST_VIDEO_FORMAT_ABGR:
    case GST_VIDEO_FORMAT_AYUV:
    case GST_VIDEO_FORMAT_VUYA:
    case GST_VIDEO_FORMAT_RBGA:
      n_plane_components = 4;
      break;
    case GST_VIDEO_FORMAT_RGBA64_LE:
    case GST_VIDEO_FORMAT_RGBA64_BE:
    case GST_VIDEO_FORMAT_ARGB64:
      return GST_GL_RGBA16;
    case GST_VIDEO_FORMAT_RGB:
    case GST_VIDEO_FORMAT_BGR:
      n_plane_components = 3;
      break;
    case GST_VIDEO_FORMAT_RGB16:
    case GST_VIDEO_FORMAT_BGR16:
      return GST_GL_RGB565;
      break;
    case GST_VIDEO_FORMAT_GRAY16_BE:
    case GST_VIDEO_FORMAT_GRAY16_LE:
    case GST_VIDEO_FORMAT_YUY2:
    case GST_VIDEO_FORMAT_UYVY:
      n_plane_components = 2;
      break;
    case GST_VIDEO_FORMAT_NV12:
    case GST_VIDEO_FORMAT_NV21:
    case GST_VIDEO_FORMAT_NV16:
    case GST_VIDEO_FORMAT_NV61:
    case GST_VIDEO_FORMAT_NV12_16L32S:
    case GST_VIDEO_FORMAT_NV12_4L4:
      n_plane_components = plane == 0 ? 1 : 2;
      break;
    case GST_VIDEO_FORMAT_AV12:
      n_plane_components = (plane == 1) ? 2 : 1;
      break;
    case GST_VIDEO_FORMAT_GRAY8:
    case GST_VIDEO_FORMAT_Y444:
    case GST_VIDEO_FORMAT_Y42B:
    case GST_VIDEO_FORMAT_Y41B:
    case GST_VIDEO_FORMAT_I420:
    case GST_VIDEO_FORMAT_YV12:
    case GST_VIDEO_FORMAT_A420:
    case GST_VIDEO_FORMAT_A422:
    case GST_VIDEO_FORMAT_A444:
      n_plane_components = 1;
      break;
    case GST_VIDEO_FORMAT_BGR10A2_LE:
    case GST_VIDEO_FORMAT_RGB10A2_LE:
    case GST_VIDEO_FORMAT_Y410:
      return GST_GL_RGB10_A2;
    case GST_VIDEO_FORMAT_P010_10LE:
    case GST_VIDEO_FORMAT_P010_10BE:
    case GST_VIDEO_FORMAT_P012_LE:
    case GST_VIDEO_FORMAT_P012_BE:
    case GST_VIDEO_FORMAT_P016_LE:
    case GST_VIDEO_FORMAT_P016_BE:
      return plane == 0 ? GST_GL_R16 : GST_GL_RG16;
    case GST_VIDEO_FORMAT_Y210:
    case GST_VIDEO_FORMAT_Y212_LE:
    case GST_VIDEO_FORMAT_Y212_BE:
      return GST_GL_RG16;
    case GST_VIDEO_FORMAT_Y412_LE:
    case GST_VIDEO_FORMAT_Y412_BE:
      return GST_GL_RGBA16;
    case GST_VIDEO_FORMAT_GBR:
    case GST_VIDEO_FORMAT_RGBP:
    case GST_VIDEO_FORMAT_BGRP:
    case GST_VIDEO_FORMAT_GBRA:
      return GST_GL_R8;
    case GST_VIDEO_FORMAT_I420_10LE:
    case GST_VIDEO_FORMAT_I420_10BE:
    case GST_VIDEO_FORMAT_I420_12LE:
    case GST_VIDEO_FORMAT_I420_12BE:
    case GST_VIDEO_FORMAT_A420_10LE:
    case GST_VIDEO_FORMAT_A420_10BE:
    case GST_VIDEO_FORMAT_A420_12LE:
    case GST_VIDEO_FORMAT_A420_12BE:
    case GST_VIDEO_FORMAT_A420_16LE:
    case GST_VIDEO_FORMAT_A420_16BE:
    case GST_VIDEO_FORMAT_A422_10LE:
    case GST_VIDEO_FORMAT_A422_10BE:
    case GST_VIDEO_FORMAT_A422_12LE:
    case GST_VIDEO_FORMAT_A422_12BE:
    case GST_VIDEO_FORMAT_A422_16LE:
    case GST_VIDEO_FORMAT_A422_16BE:
    case GST_VIDEO_FORMAT_A444_10LE:
    case GST_VIDEO_FORMAT_A444_10BE:
    case GST_VIDEO_FORMAT_A444_12LE:
    case GST_VIDEO_FORMAT_A444_12BE:
    case GST_VIDEO_FORMAT_A444_16LE:
    case GST_VIDEO_FORMAT_A444_16BE:
      return GST_GL_R16;
    default:
      n_plane_components = 4;
      g_assert_not_reached ();
      break;
  }

  switch (n_plane_components) {
    case 4:
      return GST_GL_RGBA;
    case 3:
      return GST_GL_RGB;
    case 2:
      return texture_rg ? GST_GL_RG : GST_GL_LUMINANCE_ALPHA;
    case 1:
      return texture_rg ? GST_GL_RED : GST_GL_LUMINANCE;
    default:
      break;
  }

  g_critical ("Unknown video format 0x%x provided", v_format);
  return 0;
}

/**
 * gst_gl_sized_gl_format_from_gl_format_type:
 * @context: a #GstGLContext
 * @format: an OpenGL format, `GL_RGBA`, `GL_LUMINANCE`, etc
 * @type: an OpenGL type, `GL_UNSIGNED_BYTE`, `GL_FLOAT`, etc
 *
 * Returns: the sized internal format specified by @format and @type that can
 *          be used in @context
 */
guint
gst_gl_sized_gl_format_from_gl_format_type (GstGLContext * context,
    guint format, guint type)
{
  gboolean ext_texture_rg =
      gst_gl_context_check_feature (context, "GL_EXT_texture_rg");

  switch (format) {
    case GST_GL_RGBA:
      switch (type) {
        case GL_UNSIGNED_BYTE:
          return USING_GLES2 (context)
              && !USING_GLES3 (context) ? GST_GL_RGBA : GST_GL_RGBA8;
        case GL_UNSIGNED_SHORT:
          return GST_GL_RGBA16;
        case GL_UNSIGNED_INT_2_10_10_10_REV:
          return GST_GL_RGB10_A2;
      }
      break;
    case GST_GL_RGB:
      switch (type) {
        case GL_UNSIGNED_BYTE:
          return USING_GLES2 (context)
              && !USING_GLES3 (context) ? GST_GL_RGB : GST_GL_RGB8;
        case GL_UNSIGNED_SHORT_5_6_5:
          return GST_GL_RGB565;
        case GL_UNSIGNED_SHORT:
          return GST_GL_RGB16;
      }
      break;
    case GST_GL_RG:
      switch (type) {
        case GL_UNSIGNED_BYTE:
          if (!USING_GLES3 (context) && USING_GLES2 (context) && ext_texture_rg)
            return GST_GL_RG;
          return GST_GL_RG8;
        case GL_UNSIGNED_SHORT:
          return GST_GL_RG16;
      }
      break;
    case GST_GL_RED:
      switch (type) {
        case GL_UNSIGNED_BYTE:
          if (!USING_GLES3 (context) && USING_GLES2 (context) && ext_texture_rg)
            return GST_GL_RED;
          return GST_GL_R8;
        case GL_UNSIGNED_SHORT:
          return GST_GL_R16;
      }
      break;
    case GST_GL_RGBA8:
    case GST_GL_RGBA16:
    case GST_GL_RGB8:
    case GST_GL_RGB16:
    case GST_GL_RGB565:
    case GST_GL_RG8:
    case GST_GL_R8:
    case GST_GL_LUMINANCE:
    case GST_GL_LUMINANCE_ALPHA:
    case GST_GL_ALPHA:
    case GST_GL_DEPTH_COMPONENT16:
    case GST_GL_DEPTH24_STENCIL8:
    case GST_GL_RGB10_A2:
    case GST_GL_R16:
    case GST_GL_RG16:
      return format;
    default:
      g_critical ("Unknown GL format 0x%x type 0x%x provided", format, type);
      return format;
  }

  g_assert_not_reached ();
  return 0;
}

/**
 * gst_gl_format_type_from_sized_gl_format:
 * @format: the sized internal #GstGLFormat
 * @unsized_format: (out): location for the resulting unsized #GstGLFormat
 * @gl_type: (out): location for the resulting GL type
 *
 * Get the unsized format and type from @format for usage in glReadPixels,
 * glTex{Sub}Image*, glTexImage* and similar functions.
 *
 * Since: 1.16
 */
void
gst_gl_format_type_from_sized_gl_format (GstGLFormat format,
    GstGLFormat * unsized_format, guint * gl_type)
{
  g_return_if_fail (unsized_format != NULL);
  g_return_if_fail (gl_type != NULL);

  switch (format) {
    case GST_GL_RGBA8:
      *unsized_format = GST_GL_RGBA;
      *gl_type = GL_UNSIGNED_BYTE;
      break;
    case GST_GL_RGB8:
      *unsized_format = GST_GL_RGB;
      *gl_type = GL_UNSIGNED_BYTE;
      break;
    case GST_GL_RGBA16:
      *unsized_format = GST_GL_RGBA;
      *gl_type = GL_UNSIGNED_SHORT;
      break;
    case GST_GL_RGB16:
      *unsized_format = GST_GL_RGB;
      *gl_type = GL_UNSIGNED_SHORT;
      break;
    case GST_GL_RGB565:
      *unsized_format = GST_GL_RGB;
      *gl_type = GL_UNSIGNED_SHORT_5_6_5;
      break;
    case GST_GL_RG8:
      *unsized_format = GST_GL_RG;
      *gl_type = GL_UNSIGNED_BYTE;
      break;
    case GST_GL_R8:
      *unsized_format = GST_GL_RED;
      *gl_type = GL_UNSIGNED_BYTE;
      break;
    case GST_GL_RGBA:
    case GST_GL_RGB:
    case GST_GL_RG:
    case GST_GL_RED:
    case GST_GL_LUMINANCE:
    case GST_GL_LUMINANCE_ALPHA:
    case GST_GL_ALPHA:
      *unsized_format = format;
      *gl_type = GL_UNSIGNED_BYTE;
      break;
    case GST_GL_RGB10_A2:
      *unsized_format = GST_GL_RGBA;
      *gl_type = GL_UNSIGNED_INT_2_10_10_10_REV;
      break;
    case GST_GL_R16:
      *unsized_format = GST_GL_RED;
      *gl_type = GL_UNSIGNED_SHORT;
      break;
    case GST_GL_RG16:
      *unsized_format = GST_GL_RG;
      *gl_type = GL_UNSIGNED_SHORT;
      break;
    default:
      g_critical ("Unknown GL format 0x%x provided", format);
      *unsized_format = format;
      *gl_type = GL_UNSIGNED_BYTE;
      return;
  }
}

/**
 * gst_gl_format_n_components:
 * @gl_format: the #GstGLFormat
 *
 * Returns: the number of components in a #GstGLFormat
 *
 * Since: 1.24
 */
guint
gst_gl_format_n_components (GstGLFormat gl_format)
{
  switch (gl_format) {
    case GST_GL_LUMINANCE:
    case GST_GL_ALPHA:
    case GST_GL_RED:
    case GST_GL_R8:
    case GST_GL_DEPTH_COMPONENT16:
    case GST_GL_R16:
      return 1;
    case GST_GL_LUMINANCE_ALPHA:
    case GST_GL_RG:
    case GST_GL_RG8:
    case GST_GL_DEPTH24_STENCIL8:
    case GST_GL_RG16:
      return 2;
    case GST_GL_RGB:
    case GST_GL_RGB8:
    case GST_GL_RGB565:
    case GST_GL_RGB16:
      return 3;
    case GST_GL_RGBA:
    case GST_GL_RGBA8:
    case GST_GL_RGBA16:
    case GST_GL_RGB10_A2:
      return 4;
    default:
      g_warn_if_reached ();
      return 0;
  }
}

static void
get_single_planar_format_gl_swizzle_order (GstVideoFormat format,
    gint swizzle[GST_VIDEO_MAX_COMPONENTS])
{
  const GstVideoFormatInfo *finfo = gst_video_format_get_info (format);
  int c_i = 0, i;

  g_return_if_fail (finfo->flags & GST_VIDEO_FORMAT_FLAG_RGB
      || format == GST_VIDEO_FORMAT_AYUV || format == GST_VIDEO_FORMAT_VUYA);

  if (format == GST_VIDEO_FORMAT_BGR10A2_LE) {
    swizzle[0] = 2;
    swizzle[1] = 1;
    swizzle[2] = 0;
    swizzle[3] = 3;
    return;
  }
  if (format == GST_VIDEO_FORMAT_RGB10A2_LE) {
    swizzle[0] = 0;
    swizzle[1] = 1;
    swizzle[2] = 2;
    swizzle[3] = 3;
    return;
  }

  for (i = 0; i < finfo->n_components; i++) {
    swizzle[c_i++] = finfo->poffset[i] / (GST_ROUND_UP_8 (finfo->bits) / 8);
  }

  /* special case spaced RGB formats as the space does not contain a poffset
   * value and we need all four components to be valid in order to swizzle
   * correctly */
  if (format == GST_VIDEO_FORMAT_xRGB || format == GST_VIDEO_FORMAT_xBGR) {
    swizzle[c_i++] = 0;
  } else if (format == GST_VIDEO_FORMAT_RGBx || format == GST_VIDEO_FORMAT_BGRx
      || format == GST_VIDEO_FORMAT_RGB || format == GST_VIDEO_FORMAT_BGR) {
    swizzle[c_i++] = 3;
  } else {
    for (i = finfo->n_components; i < GST_VIDEO_MAX_COMPONENTS; i++) {
      swizzle[c_i++] = -1;
    }
  }
}

/**
 * gst_gl_video_format_swizzle:
 * @video_format: the #GstVideoFormat in use
 * @swizzle: (out) (array fixed-size=4): the returned swizzle indices
 *
 * Calculates the swizzle indices for @video_format and @gl_format in order to
 * access a texture such that accessing a texel from a texture through the swizzle
 * index produces values in the order (R, G, B, A) or (Y, U, V, A).
 *
 * For multi-planer formats, the swizzle index uses the same component order (RGBA/YUVA)
 * and should be applied after combining multiple planes into a single rgba/yuva value.
 * e.g. sampling from a NV12 format would have Y from one texture and UV from
 * another texture into a (Y, U, V) value.  Add an Aplha component and then
 * perform swizzling.  Sampling from NV21 would produce (Y, V, U) which is then
 * swizzled to (Y, U, V).
 *
 * Returns: whether valid swizzle indices could be found
 *
 * Since: 1.24
 */
gboolean
gst_gl_video_format_swizzle (GstVideoFormat video_format, int *swizzle)
{
  const GstVideoFormatInfo *finfo = gst_video_format_get_info (video_format);

  if (finfo->n_planes == 1 &&
      (finfo->flags & GST_VIDEO_FORMAT_FLAG_RGB ||
          video_format == GST_VIDEO_FORMAT_AYUV ||
          video_format == GST_VIDEO_FORMAT_VUYA)) {
    get_single_planar_format_gl_swizzle_order (video_format, swizzle);
    return TRUE;
  }

  switch (video_format) {
    case GST_VIDEO_FORMAT_BGRP:
      get_single_planar_format_gl_swizzle_order (GST_VIDEO_FORMAT_BGR, swizzle);
      return TRUE;
    case GST_VIDEO_FORMAT_RGBP:
      get_single_planar_format_gl_swizzle_order (GST_VIDEO_FORMAT_RGB, swizzle);
      return TRUE;
    case GST_VIDEO_FORMAT_AV12:
    case GST_VIDEO_FORMAT_NV12:
    case GST_VIDEO_FORMAT_NV16:
    case GST_VIDEO_FORMAT_P010_10LE:
    case GST_VIDEO_FORMAT_P010_10BE:
    case GST_VIDEO_FORMAT_P012_LE:
    case GST_VIDEO_FORMAT_P012_BE:
    case GST_VIDEO_FORMAT_P016_LE:
    case GST_VIDEO_FORMAT_P016_BE:
    case GST_VIDEO_FORMAT_NV12_16L32S:
    case GST_VIDEO_FORMAT_NV12_4L4:
    case GST_VIDEO_FORMAT_I420:
    case GST_VIDEO_FORMAT_I420_10LE:
    case GST_VIDEO_FORMAT_I420_10BE:
    case GST_VIDEO_FORMAT_I420_12LE:
    case GST_VIDEO_FORMAT_I420_12BE:
    case GST_VIDEO_FORMAT_Y444:
    case GST_VIDEO_FORMAT_Y42B:
    case GST_VIDEO_FORMAT_Y41B:
    case GST_VIDEO_FORMAT_A420:
    case GST_VIDEO_FORMAT_A420_10LE:
    case GST_VIDEO_FORMAT_A420_10BE:
    case GST_VIDEO_FORMAT_A420_12LE:
    case GST_VIDEO_FORMAT_A420_12BE:
    case GST_VIDEO_FORMAT_A420_16LE:
    case GST_VIDEO_FORMAT_A420_16BE:
    case GST_VIDEO_FORMAT_A422:
    case GST_VIDEO_FORMAT_A422_10LE:
    case GST_VIDEO_FORMAT_A422_10BE:
    case GST_VIDEO_FORMAT_A422_12LE:
    case GST_VIDEO_FORMAT_A422_12BE:
    case GST_VIDEO_FORMAT_A422_16LE:
    case GST_VIDEO_FORMAT_A422_16BE:
    case GST_VIDEO_FORMAT_A444:
    case GST_VIDEO_FORMAT_A444_10LE:
    case GST_VIDEO_FORMAT_A444_10BE:
    case GST_VIDEO_FORMAT_A444_12LE:
    case GST_VIDEO_FORMAT_A444_12BE:
    case GST_VIDEO_FORMAT_A444_16LE:
    case GST_VIDEO_FORMAT_A444_16BE:
      swizzle[0] = 0;
      swizzle[1] = 1;
      swizzle[2] = 2;
      swizzle[3] = 3;
      return TRUE;
    case GST_VIDEO_FORMAT_NV21:
    case GST_VIDEO_FORMAT_NV61:
    case GST_VIDEO_FORMAT_YV12:
      swizzle[0] = 0;
      swizzle[1] = 2;
      swizzle[2] = 1;
      swizzle[3] = 3;
      return TRUE;
    case GST_VIDEO_FORMAT_Y410:
    case GST_VIDEO_FORMAT_Y412_LE:
    case GST_VIDEO_FORMAT_Y412_BE:
      swizzle[0] = 1;
      swizzle[1] = 0;
      swizzle[2] = 2;
      swizzle[3] = 3;
      return TRUE;
      /* TODO: deal with YUY2 variants */
    default:
      return FALSE;
  }
}

/**
 * gst_gl_swizzle_invert:
 * @swizzle: (array fixed-size=4): input swizzle
 * @inversion: (out) (array fixed-size=4): resulting inversion
 *
 * Given @swizzle, produce @inversion such that:
 *
 * @swizzle[@inversion[i]] == identity[i] where:
 * - identity = {0, 1, 2,...}
 * - unset fields are marked by -1
 *
 * Since: 1.24
 */
void
gst_gl_swizzle_invert (gint * swizzle, gint * inversion)
{
  int i;

  for (i = 0; i < GST_VIDEO_MAX_COMPONENTS; i++) {
    inversion[i] = -1;
  }

  for (i = 0; i < GST_VIDEO_MAX_COMPONENTS; i++) {
    if (swizzle[i] >= 0 && swizzle[i] < 4 && inversion[swizzle[i]] == -1) {
      inversion[swizzle[i]] = i;
    }
  }
}

/**
 * gst_gl_format_is_supported:
 * @context: a #GstGLContext
 * @format: the #GstGLFormat to check is supported by @context
 *
 * Returns: Whether @format is supported by @context based on the OpenGL API,
 *          version, or available OpenGL extension/s.
 *
 * Since: 1.16
 */
gboolean
gst_gl_format_is_supported (GstGLContext * context, GstGLFormat format)
{
  g_return_val_if_fail (GST_IS_GL_CONTEXT (context), FALSE);

  switch (format) {
    case GST_GL_RGBA:
    case GST_GL_RGB:
      return TRUE;
    case GST_GL_LUMINANCE:
    case GST_GL_ALPHA:
    case GST_GL_LUMINANCE_ALPHA:
      /* deprecated/removed in core GL3 contexts */
      return USING_OPENGL (context) || USING_GLES2 (context);
    case GST_GL_RG:
    case GST_GL_RED:
      return gst_gl_context_check_gl_version (context, GST_GL_API_GLES2, 3, 0)
          || gst_gl_context_check_gl_version (context, GST_GL_API_OPENGL3, 3, 0)
          || gst_gl_context_check_feature (context, "GL_EXT_texture_rg")
          || gst_gl_context_check_feature (context, "GL_ARB_texture_rg");
    case GST_GL_R8:
    case GST_GL_RG8:
      return USING_GLES3 (context)
          || gst_gl_context_check_gl_version (context, GST_GL_API_OPENGL3, 3, 0)
          || gst_gl_context_check_feature (context, "GL_ARB_texture_rg");
    case GST_GL_RGB8:
    case GST_GL_RGBA8:
      return (USING_GLES3 (context) && !USING_GLES2 (context))
          || USING_OPENGL (context) || USING_OPENGL3 (context);
    case GST_GL_RGB16:
    case GST_GL_RGBA16:
      return USING_OPENGL (context) || USING_OPENGL3 (context)
          || USING_GLES3 (context);
    case GST_GL_RGB565:
      return USING_GLES2 (context) || (USING_OPENGL3 (context)
          && gst_gl_context_check_feature (context,
              "GL_ARB_ES2_compatibility"));
    case GST_GL_DEPTH_COMPONENT16:
      return gst_gl_context_check_gl_version (context, GST_GL_API_OPENGL, 1, 4)
          || USING_GLES2 (context)
          || gst_gl_context_check_feature (context, "GL_ARB_depth_texture")
          || gst_gl_context_check_feature (context, "GL_OES_depth_texture");
    case GST_GL_DEPTH24_STENCIL8:
      return gst_gl_context_check_gl_version (context, GST_GL_API_OPENGL, 3, 0)
          || USING_GLES3 (context)
          || gst_gl_context_check_feature (context,
          "GL_OES_packed_depth_stencil")
          || gst_gl_context_check_feature (context,
          "GL_EXT_packed_depth_stencil");
    case GST_GL_RGB10_A2:
      return USING_OPENGL (context) || USING_OPENGL3 (context)
          || USING_GLES3 (context)
          || gst_gl_context_check_feature (context,
          "GL_OES_required_internalformat");
    case GST_GL_R16:
    case GST_GL_RG16:
      return gst_gl_context_check_gl_version (context,
          GST_GL_API_OPENGL | GST_GL_API_OPENGL3, 3, 0)
          || (gst_gl_context_check_gl_version (context, GST_GL_API_GLES2, 3, 1)
          && gst_gl_context_check_feature (context, "GL_EXT_texture_norm16"));
    default:
      g_assert_not_reached ();
      return FALSE;
  }
}

/**
 * gst_gl_texture_target_to_string:
 * @target: a #GstGLTextureTarget
 *
 * Returns: (nullable): the stringified version of @target or %NULL
 */
const gchar *
gst_gl_texture_target_to_string (GstGLTextureTarget target)
{
  switch (target) {
    case GST_GL_TEXTURE_TARGET_2D:
      return GST_GL_TEXTURE_TARGET_2D_STR;
    case GST_GL_TEXTURE_TARGET_RECTANGLE:
      return GST_GL_TEXTURE_TARGET_RECTANGLE_STR;
    case GST_GL_TEXTURE_TARGET_EXTERNAL_OES:
      return GST_GL_TEXTURE_TARGET_EXTERNAL_OES_STR;
    default:
      return NULL;
  }
}

/**
 * gst_gl_texture_target_from_string:
 * @str: a string equivalent to one of the GST_GL_TEXTURE_TARGET_*_STR values
 *
 * Returns: the #GstGLTextureTarget represented by @str or
 *          %GST_GL_TEXTURE_TARGET_NONE
 */
GstGLTextureTarget
gst_gl_texture_target_from_string (const gchar * str)
{
  if (!str)
    return GST_GL_TEXTURE_TARGET_NONE;

  if (g_strcmp0 (str, GST_GL_TEXTURE_TARGET_2D_STR) == 0)
    return GST_GL_TEXTURE_TARGET_2D;
  if (g_strcmp0 (str, GST_GL_TEXTURE_TARGET_RECTANGLE_STR) == 0)
    return GST_GL_TEXTURE_TARGET_RECTANGLE;
  if (g_strcmp0 (str, GST_GL_TEXTURE_TARGET_EXTERNAL_OES_STR) == 0)
    return GST_GL_TEXTURE_TARGET_EXTERNAL_OES;

  return GST_GL_TEXTURE_TARGET_NONE;
}

/**
 * gst_gl_texture_target_to_gl:
 * @target: a #GstGLTextureTarget
 *
 * Returns: the OpenGL value for binding the @target with glBindTexture() and
 *          similar functions or 0
 */
guint
gst_gl_texture_target_to_gl (GstGLTextureTarget target)
{
  switch (target) {
    case GST_GL_TEXTURE_TARGET_2D:
      return GL_TEXTURE_2D;
    case GST_GL_TEXTURE_TARGET_RECTANGLE:
      return GL_TEXTURE_RECTANGLE;
    case GST_GL_TEXTURE_TARGET_EXTERNAL_OES:
      return GL_TEXTURE_EXTERNAL_OES;
    default:
      return 0;
  }
}

/**
 * gst_gl_texture_target_from_gl:
 * @target: an OpenGL texture binding target
 *
 * Returns: the #GstGLTextureTarget that's equiavalant to @target or
 *          %GST_GL_TEXTURE_TARGET_NONE
 */
GstGLTextureTarget
gst_gl_texture_target_from_gl (guint target)
{
  switch (target) {
    case GL_TEXTURE_2D:
      return GST_GL_TEXTURE_TARGET_2D;
    case GL_TEXTURE_RECTANGLE:
      return GST_GL_TEXTURE_TARGET_RECTANGLE;
    case GL_TEXTURE_EXTERNAL_OES:
      return GST_GL_TEXTURE_TARGET_EXTERNAL_OES;
    default:
      return GST_GL_TEXTURE_TARGET_NONE;
  }
}

/**
 * gst_gl_texture_target_to_buffer_pool_option:
 * @target: a #GstGLTextureTarget
 *
 * Returns: (nullable): a string representing the @GstBufferPoolOption specified by @target
 */
const gchar *
gst_gl_texture_target_to_buffer_pool_option (GstGLTextureTarget target)
{
  switch (target) {
    case GST_GL_TEXTURE_TARGET_2D:
      return GST_BUFFER_POOL_OPTION_GL_TEXTURE_TARGET_2D;
    case GST_GL_TEXTURE_TARGET_RECTANGLE:
      return GST_BUFFER_POOL_OPTION_GL_TEXTURE_TARGET_RECTANGLE;
    case GST_GL_TEXTURE_TARGET_EXTERNAL_OES:
      return GST_BUFFER_POOL_OPTION_GL_TEXTURE_TARGET_EXTERNAL_OES;
    default:
      return NULL;
  }
}
