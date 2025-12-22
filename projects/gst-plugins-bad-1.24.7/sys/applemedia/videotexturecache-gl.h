/*
 * Copyright (C) 2015 Alessandro Decina <twi@centricular.com>
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

#ifndef __GST_CORE_VIDEO_TEXTURE_CACHE_GL_H__
#define __GST_CORE_VIDEO_TEXTURE_CACHE_GL_H__

#include <gst/video/gstvideometa.h>
#include <gst/gl/gl.h>
#include "corevideomemory.h"
#include "videotexturecache.h"

G_BEGIN_DECLS

#define GST_TYPE_VIDEO_TEXTURE_CACHE_GL         (gst_video_texture_cache_gl_get_type())
#define GST_VIDEO_TEXTURE_CACHE_GL(o)           (G_TYPE_CHECK_INSTANCE_CAST((o), GST_TYPE_VIDEO_TEXTURE_CACHE_GL, GstVideoTextureCacheGL))
#define GST_VIDEO_TEXTURE_CACHE_GL_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GST_TYPE_VIDEO_TEXTURE_CACHE_GL, GstVideoTextureCacheGLClass))
#define GST_IS_VIDEO_TEXTURE_CACHE_GL(o)        (G_TYPE_CHECK_INSTANCE_TYPE((o), GST_TYPE_VIDEO_TEXTURE_CACHE_GL))
#define GST_IS_VIDEO_TEXTURE_CACHE_GL_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE((k), GST_TYPE_VIDEO_TEXTURE_CACHE_GL))
#define GST_VIDEO_TEXTURE_CACHE_GL_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o), GST_TYPE_VIDEO_TEXTURE_CACHE_GL, GstVideoTextureCacheGLClass))
GType gst_video_texture_cache_gl_get_type     (void);

typedef struct _GstVideoTextureCacheGL
{
  GstVideoTextureCache parent;

  GstGLContext *ctx;
#if HAVE_IOS
  CVOpenGLESTextureCacheRef cache;
#else
  GstBufferPool *pool;
#endif
} GstVideoTextureCacheGL;

typedef struct _GstVideoTextureCacheGLClass
{
  GstVideoTextureCacheClass parent_class;
} GstVideoTextureCacheGLClass;

GstVideoTextureCache *  gst_video_texture_cache_gl_new             (GstGLContext * ctx);

G_END_DECLS

#endif /* __GST_CORE_VIDEO_TEXTURE_CACHE_H__ */
