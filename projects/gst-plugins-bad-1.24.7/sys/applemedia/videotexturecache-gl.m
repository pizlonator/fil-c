/*
 * Copyright (C) 2010 Ole André Vadla Ravnås <oleavr@soundrop.com>
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
#  include "config.h"
#endif

#include <gst/gst.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

#if !HAVE_IOS
#import <AppKit/AppKit.h>
#include "iosurfaceglmemory.h"
#endif
#include "iosglmemory.h"
#include "videotexturecache-gl.h"
#include "coremediabuffer.h"
#include "corevideobuffer.h"
#include "vtutil.h"

G_DEFINE_TYPE (GstVideoTextureCacheGL, gst_video_texture_cache_gl, GST_TYPE_VIDEO_TEXTURE_CACHE);

typedef struct _ContextThreadData
{
  GstVideoTextureCacheGL *cache;
  GstAppleCoreVideoPixelBuffer *gpixbuf;
  guint plane;
  gsize size;
  GstMemory *memory;
} ContextThreadData;

typedef struct _TextureWrapper
{
#if HAVE_IOS
  CVOpenGLESTextureCacheRef cache;
  CVOpenGLESTextureRef texture;
#else
  CVOpenGLTextureCacheRef cache;
  CVOpenGLTextureRef texture;
#endif
} TextureWrapper;

enum
{
  PROP_0,
  PROP_CONTEXT,
};

static GstMemory * gst_video_texture_cache_gl_create_memory (GstVideoTextureCache * cache,
      GstAppleCoreVideoPixelBuffer *gpixbuf, guint plane, gsize size);

GstVideoTextureCache *
gst_video_texture_cache_gl_new (GstGLContext * ctx)
{
  g_return_val_if_fail (GST_IS_GL_CONTEXT (ctx), NULL);

  return g_object_new (GST_TYPE_VIDEO_TEXTURE_CACHE_GL,
      "context", ctx, NULL);
}

static void
gst_video_texture_cache_gl_finalize (GObject * object)
{
  GstVideoTextureCacheGL *cache_gl = GST_VIDEO_TEXTURE_CACHE_GL (object);

#if HAVE_IOS
  CFRelease (cache_gl->cache); /* iOS has no "CVOpenGLESTextureCacheRelease" */
#else
#if 0
  gst_buffer_pool_set_active (cache->pool, FALSE);
  gst_object_unref (cache->pool);
#endif
#endif
  gst_object_unref (cache_gl->ctx);

  G_OBJECT_CLASS (gst_video_texture_cache_gl_parent_class)->finalize (object);
}

static void
gst_video_texture_cache_gl_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstVideoTextureCacheGL *cache_gl = GST_VIDEO_TEXTURE_CACHE_GL (object);

  switch (prop_id) {
    case PROP_CONTEXT:
      cache_gl->ctx = g_value_dup_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_video_texture_cache_gl_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstVideoTextureCacheGL *cache_gl = GST_VIDEO_TEXTURE_CACHE_GL (object);

  switch (prop_id) {
    case PROP_CONTEXT:
      g_value_set_object (value, cache_gl->ctx);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_video_texture_cache_gl_constructed (GObject * object)
{
  GstVideoTextureCacheGL *cache_gl = GST_VIDEO_TEXTURE_CACHE_GL (object);

  g_return_if_fail (GST_IS_GL_CONTEXT (cache_gl->ctx));

#if HAVE_IOS
  CFMutableDictionaryRef cache_attrs =
      CFDictionaryCreateMutable (NULL, 0, &kCFTypeDictionaryKeyCallBacks,
      &kCFTypeDictionaryValueCallBacks);
  CVOpenGLESTextureCacheCreate (kCFAllocatorDefault, (CFDictionaryRef) cache_attrs,
      (__bridge CVEAGLContext) (gpointer) gst_gl_context_get_gl_context (cache_gl->ctx), NULL, &cache_gl->cache);
#else
  gst_ios_surface_gl_memory_init ();
#if 0
  cache->pool = GST_BUFFER_POOL (gst_gl_buffer_pool_new (ctx));
#endif
#endif
}

static void
gst_video_texture_cache_gl_init (GstVideoTextureCacheGL * cache_gl)
{
}

static void
gst_video_texture_cache_gl_class_init (GstVideoTextureCacheGLClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstVideoTextureCacheClass *cache_class = (GstVideoTextureCacheClass *) klass;

  gobject_class->set_property = gst_video_texture_cache_gl_set_property;
  gobject_class->get_property = gst_video_texture_cache_gl_get_property;
  gobject_class->constructed = gst_video_texture_cache_gl_constructed;
  gobject_class->finalize = gst_video_texture_cache_gl_finalize;

  g_object_class_install_property (gobject_class, PROP_CONTEXT,
      g_param_spec_object ("context", "Context",
          "Associated OpenGL context", GST_TYPE_GL_CONTEXT,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  cache_class->create_memory = gst_video_texture_cache_gl_create_memory;
}

#if HAVE_IOS
static void
gst_video_texture_cache_gl_release_texture (TextureWrapper *data)
{
  CFRelease(data->texture);
  CFRelease(data->cache);
  g_free(data);
}

static void
_do_create_memory (GstGLContext * context, ContextThreadData * data)
{
  CVOpenGLESTextureRef texture = NULL;
  GstVideoTextureCache *cache = GST_VIDEO_TEXTURE_CACHE (data->cache);
  GstVideoTextureCacheGL *cache_gl = data->cache;
  GstAppleCoreVideoPixelBuffer *gpixbuf = data->gpixbuf;
  CVPixelBufferRef pixel_buf = gpixbuf->buf;
  guint plane = data->plane;
  gssize size = data->size;
  GstGLTextureTarget gl_target;
  GstAppleCoreVideoMemory *memory;
  GstIOSGLMemory *gl_memory;
  GstGLFormat texformat;

  switch (GST_VIDEO_INFO_FORMAT (&cache->input_info)) {
      case GST_VIDEO_FORMAT_BGRA:
        if (CVOpenGLESTextureCacheCreateTextureFromImage (kCFAllocatorDefault,
              cache_gl->cache, pixel_buf, NULL, GL_TEXTURE_2D, GL_RGBA,
              GST_VIDEO_INFO_WIDTH (&cache->input_info),
              GST_VIDEO_INFO_HEIGHT (&cache->input_info),
              GL_RGBA, GL_UNSIGNED_BYTE, 0, &texture) != kCVReturnSuccess)
          goto error;

        texformat = GST_GL_RGBA;
        plane = 0;
        goto success;
      case GST_VIDEO_FORMAT_NV12: {
        GstGLFormat texfmt;

        if (plane == 0)
          texformat = GST_GL_LUMINANCE;
        else
          texformat = GST_GL_LUMINANCE_ALPHA;
        texfmt = gst_gl_sized_gl_format_from_gl_format_type (cache_gl->ctx, texformat, GL_UNSIGNED_BYTE);

        if (CVOpenGLESTextureCacheCreateTextureFromImage (kCFAllocatorDefault,
              cache_gl->cache, pixel_buf, NULL, GL_TEXTURE_2D, texformat,
              GST_VIDEO_INFO_COMP_WIDTH (&cache->input_info, plane),
              GST_VIDEO_INFO_COMP_HEIGHT (&cache->input_info, plane),
              texfmt, GL_UNSIGNED_BYTE, plane, &texture) != kCVReturnSuccess)
          goto error;

        goto success;
      }
    default:
      g_warn_if_reached ();
      goto error;
  }

success: {
  TextureWrapper *texture_data = g_new0 (TextureWrapper, 1);
  CFRetain(cache_gl->cache);
  texture_data->cache = cache_gl->cache;
  texture_data->texture = texture;
  gl_target = gst_gl_texture_target_from_gl (CVOpenGLESTextureGetTarget (texture));
  memory = gst_apple_core_video_memory_new_wrapped (gpixbuf, plane, size);
  gl_memory = gst_ios_gl_memory_new_wrapped (context, memory,
      gl_target, texformat, CVOpenGLESTextureGetName (texture), &cache->input_info,
     plane, NULL, texture_data, (GDestroyNotify) gst_video_texture_cache_gl_release_texture);

  data->memory = GST_MEMORY_CAST (gl_memory);

  return;
}

error:
  data->memory = NULL;
}
#endif

static GstMemory *
gst_video_texture_cache_gl_create_memory (GstVideoTextureCache * cache,
      GstAppleCoreVideoPixelBuffer *gpixbuf, guint plane, gsize size)
{
  GstVideoTextureCacheGL *cache_gl = GST_VIDEO_TEXTURE_CACHE_GL (cache);
  ContextThreadData data = {cache_gl, gpixbuf, plane, size, NULL};

#if HAVE_IOS
  gst_gl_context_thread_add (cache_gl->ctx,
      (GstGLContextThreadFunc) _do_create_memory, &data);
#endif

  return data.memory;
}

G_GNUC_END_IGNORE_DEPRECATIONS
