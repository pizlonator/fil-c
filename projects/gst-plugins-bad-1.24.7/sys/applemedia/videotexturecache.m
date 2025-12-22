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

#if !HAVE_IOS
#import <AppKit/AppKit.h>
#endif
#include "videotexturecache.h"
#include "coremediabuffer.h"
#include "corevideobuffer.h"
#include "vtutil.h"

G_DEFINE_ABSTRACT_TYPE (GstVideoTextureCache, gst_video_texture_cache, G_TYPE_OBJECT);

static void gst_video_texture_cache_default_set_format (GstVideoTextureCache * cache,
    GstVideoFormat in_format, GstCaps * out_caps);

static void
gst_video_texture_cache_finalize (GObject * object)
{
  GstVideoTextureCache *cache = GST_VIDEO_TEXTURE_CACHE (object);

  if (cache->in_caps)
    gst_caps_unref (cache->in_caps);
  if (cache->out_caps)
    gst_caps_unref (cache->out_caps);

  G_OBJECT_CLASS (gst_video_texture_cache_parent_class)->finalize (object);
}

static void
gst_video_texture_cache_init (GstVideoTextureCache * cache)
{
  gst_video_info_init (&cache->input_info);
}

static void
gst_video_texture_cache_class_init (GstVideoTextureCacheClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = gst_video_texture_cache_finalize;

  klass->set_format = gst_video_texture_cache_default_set_format;
}

static void
gst_video_texture_cache_default_set_format (GstVideoTextureCache * cache,
    GstVideoFormat in_format, GstCaps * out_caps)
{
  GstCaps *in_caps;

  g_return_if_fail (gst_caps_is_fixed (out_caps));

  out_caps = gst_caps_copy (out_caps);
  gst_video_info_from_caps (&cache->output_info, out_caps);

  in_caps = gst_caps_copy (out_caps);
  gst_caps_set_simple (in_caps, "format",
          G_TYPE_STRING, gst_video_format_to_string (in_format), NULL);
  gst_video_info_from_caps (&cache->input_info, in_caps);

  gst_caps_take (&cache->in_caps, in_caps);
  gst_caps_take (&cache->out_caps, out_caps);
}

void
gst_video_texture_cache_set_format (GstVideoTextureCache * cache,
    GstVideoFormat in_format, GstCaps * out_caps)
{
  GstVideoTextureCacheClass *cache_class = GST_VIDEO_TEXTURE_CACHE_GET_CLASS (cache);

  g_return_if_fail (cache_class->set_format);
  cache_class->set_format (cache, in_format, out_caps);
}

GstMemory *
gst_video_texture_cache_create_memory (GstVideoTextureCache * cache,
      GstAppleCoreVideoPixelBuffer *gpixbuf, guint plane, gsize size)
{
  GstVideoTextureCacheClass *cache_class = GST_VIDEO_TEXTURE_CACHE_GET_CLASS (cache);

  g_return_val_if_fail (cache_class->create_memory, NULL);
  return cache_class->create_memory (cache, gpixbuf, plane, size);
}
