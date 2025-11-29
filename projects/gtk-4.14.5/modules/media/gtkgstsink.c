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

#include "config.h"

#include "gtkgstsinkprivate.h"

#include "gtkgstpaintableprivate.h"

#if GST_GL_HAVE_WINDOW_X11 && (GST_GL_HAVE_PLATFORM_GLX || GST_GL_HAVE_PLATFORM_EGL) && defined (GDK_WINDOWING_X11)
#define HAVE_GST_X11_SUPPORT
#include <gdk/x11/gdkx.h>
#if GST_GL_HAVE_PLATFORM_GLX
#include <gst/gl/x11/gstgldisplay_x11.h>
#endif
#endif

#if GST_GL_HAVE_WINDOW_WAYLAND && GST_GL_HAVE_PLATFORM_EGL && defined (GDK_WINDOWING_WAYLAND)
#define HAVE_GST_WAYLAND_SUPPORT
#include <gdk/wayland/gdkwayland.h>
#include <gst/gl/wayland/gstgldisplay_wayland.h>
#endif

#if GST_GL_HAVE_WINDOW_WIN32 && (GST_GL_HAVE_PLATFORM_WGL || GST_GL_HAVE_PLATFORM_EGL) && defined (GDK_WINDOWING_WIN32)
#include <gdk/win32/gdkwin32.h>
#endif

#if GST_GL_HAVE_PLATFORM_EGL && (GST_GL_HAVE_WINDOW_WIN32 || GST_GL_HAVE_WINDOW_X11)
#include <gst/gl/egl/gstgldisplay_egl.h>
#endif

#ifdef GDK_WINDOWING_MACOS
#include <gdk/macos/gdkmacos.h>
#endif

#include <gst/gl/gstglfuncs.h>

#ifdef HAVE_GSTREAMER_DRM
#include <gdk/gdkdmabuffourccprivate.h>
#include <gst/allocators/gstdmabuf.h>
#endif

enum {
  PROP_0,
  PROP_PAINTABLE,
  PROP_GL_CONTEXT,

  N_PROPS,
};

GST_DEBUG_CATEGORY (gtk_debug_gst_sink);
#define GST_CAT_DEFAULT gtk_debug_gst_sink

#define FORMATS "{ BGRA, ARGB, RGBA, ABGR, RGB, BGR }"

#define NOGL_CAPS GST_VIDEO_CAPS_MAKE (FORMATS)

#ifdef HAVE_GSTREAMER_DRM
# define GST_VIDEO_DMA_DRM_CAPS_MAKE_STR GST_VIDEO_DMA_DRM_CAPS_MAKE "; "
#else
# define GST_VIDEO_DMA_DRM_CAPS_MAKE_STR
#endif

static GstStaticPadTemplate gtk_gst_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_DMA_DRM_CAPS_MAKE_STR
                     "video/x-raw(" GST_CAPS_FEATURE_MEMORY_GL_MEMORY "), "
                     "format = (string) RGBA, "
                     "width = " GST_VIDEO_SIZE_RANGE ", "
                     "height = " GST_VIDEO_SIZE_RANGE ", "
                     "framerate = " GST_VIDEO_FPS_RANGE ", "
                     "texture-target = (string) 2D"
                     "; " NOGL_CAPS)
    );

#undef GST_VIDEO_DMA_DRM_CAPS_MAKE_STR

G_DEFINE_TYPE_WITH_CODE (GtkGstSink, gtk_gst_sink,
    GST_TYPE_VIDEO_SINK,
    GST_DEBUG_CATEGORY_INIT (gtk_debug_gst_sink,
        "gtkgstsink", 0, "GtkGstMediaFile Video Sink"));

static GParamSpec *properties[N_PROPS] = { NULL, };


static void
gtk_gst_sink_get_times (GstBaseSink  *bsink,
                        GstBuffer    *buf,
                        GstClockTime *start,
                        GstClockTime *end)
{
  GtkGstSink *gtk_sink;

  gtk_sink = GTK_GST_SINK (bsink);

  if (GST_BUFFER_TIMESTAMP_IS_VALID (buf))
    {
      *start = GST_BUFFER_TIMESTAMP (buf);
      if (GST_BUFFER_DURATION_IS_VALID (buf))
        *end = *start + GST_BUFFER_DURATION (buf);
      else
        {
          if (GST_VIDEO_INFO_FPS_N (&gtk_sink->v_info) > 0)
            {
              *end = *start +
                  gst_util_uint64_scale_int (GST_SECOND,
                                             GST_VIDEO_INFO_FPS_D (&gtk_sink->v_info),
                                             GST_VIDEO_INFO_FPS_N (&gtk_sink->v_info));
            }
        }
    }
}

#ifdef HAVE_GSTREAMER_DRM
static void
add_drm_formats_and_modifiers (GstCaps          *caps,
                               GdkDmabufFormats *dmabuf_formats)
{
  GValue dmabuf_list = G_VALUE_INIT;
  size_t i;

  g_value_init (&dmabuf_list, GST_TYPE_LIST);

  for (i = 0; i < gdk_dmabuf_formats_get_n_formats (dmabuf_formats); i++)
    {
      GValue value = G_VALUE_INIT;
      gchar *drm_format_string;
      guint32 fmt;
      guint64 mod;

      gdk_dmabuf_formats_get_format (dmabuf_formats, i, &fmt, &mod);

      if (mod == DRM_FORMAT_MOD_INVALID)
        continue;

      drm_format_string = gst_video_dma_drm_fourcc_to_string (fmt, mod);
      if (!drm_format_string)
        continue;

      g_value_init (&value, G_TYPE_STRING);
      g_value_take_string (&value, drm_format_string);
      gst_value_list_append_and_take_value (&dmabuf_list, &value);
    }

    gst_structure_take_value (gst_caps_get_structure (caps, 0), "drm-format",
        &dmabuf_list);
}
#endif

static GstCaps *
gtk_gst_sink_get_caps (GstBaseSink *bsink,
                       GstCaps     *filter)
{
  GtkGstSink *self = GTK_GST_SINK (bsink);
  GstCaps *tmp;
  GstCaps *result;

  if (self->gst_context)
    {
      tmp = gst_pad_get_pad_template_caps (GST_BASE_SINK_PAD (bsink));
#ifdef HAVE_GSTREAMER_DRM
      {
        GdkDisplay *display = gdk_gl_context_get_display (self->gdk_context);
        GdkDmabufFormats *formats = gdk_display_get_dmabuf_formats (display);

        tmp = gst_caps_make_writable (tmp);
        add_drm_formats_and_modifiers (tmp, formats);
      }
#endif
    }
  else
    {
      tmp = gst_caps_from_string (NOGL_CAPS);
    }
  GST_DEBUG_OBJECT (self, "advertising own caps %" GST_PTR_FORMAT, tmp);

  if (filter)
    {
      GST_DEBUG_OBJECT (self, "intersecting with filter caps %" GST_PTR_FORMAT, filter);

      result = gst_caps_intersect_full (filter, tmp, GST_CAPS_INTERSECT_FIRST);
      gst_caps_unref (tmp);
    }
  else
    {
      result = tmp;
    }

  GST_DEBUG_OBJECT (self, "returning caps: %" GST_PTR_FORMAT, result);

  return result;
}

static gboolean
gtk_gst_sink_set_caps (GstBaseSink *bsink,
                       GstCaps     *caps)
{
  GtkGstSink *self = GTK_GST_SINK (bsink);

  GST_DEBUG_OBJECT (self, "set caps with %" GST_PTR_FORMAT, caps);

#ifdef HAVE_GSTREAMER_DRM
  if (gst_video_is_dma_drm_caps (caps)) {
    if (!gst_video_info_dma_drm_from_caps (&self->drm_info, caps))
      return FALSE;

    if (!gst_video_info_dma_drm_to_video_info (&self->drm_info, &self->v_info))
      return FALSE;

    GST_INFO_OBJECT (self, "using DMABuf, passthrough possible");
  } else {
    gst_video_info_dma_drm_init (&self->drm_info);
#endif

    if (!gst_video_info_from_caps (&self->v_info, caps))
      return FALSE;
#ifdef HAVE_GSTREAMER_DRM
  }
#endif

  return TRUE;
}

static gboolean
gtk_gst_sink_query (GstBaseSink *bsink,
                    GstQuery    *query)
{
  GtkGstSink *self = GTK_GST_SINK (bsink);

  if (GST_QUERY_TYPE (query) == GST_QUERY_CONTEXT &&
      self->gst_display != NULL &&
      gst_gl_handle_context_query (GST_ELEMENT (self), query, self->gst_display, self->gst_context, self->gst_gdk_context))
    return TRUE;

  return GST_BASE_SINK_CLASS (gtk_gst_sink_parent_class)->query (bsink, query);
}

static gboolean
gtk_gst_sink_propose_allocation (GstBaseSink *bsink,
                                 GstQuery    *query)
{
  GtkGstSink *self = GTK_GST_SINK (bsink);
  GstBufferPool *pool = NULL;
  GstStructure *config;
  GstCaps *caps;
  guint size;
  gboolean need_pool;
  GstVideoInfo info;

  if (!self->gst_context)
    return FALSE;

  gst_query_parse_allocation (query, &caps, &need_pool);

  if (caps == NULL)
    {
      GST_DEBUG_OBJECT (bsink, "no caps specified");
      return FALSE;
    }

#ifdef HAVE_GSTREAMER_DRM
  if (gst_caps_features_contains (gst_caps_get_features (caps, 0), GST_CAPS_FEATURE_MEMORY_DMABUF))
    {
      gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, 0);
      return TRUE;
    }
#endif

  if (!gst_caps_features_contains (gst_caps_get_features (caps, 0), GST_CAPS_FEATURE_MEMORY_GL_MEMORY))
    return FALSE;

  if (!gst_video_info_from_caps (&info, caps))
    {
      GST_DEBUG_OBJECT (self, "invalid caps specified");
      return FALSE;
    }

  /* the normal size of a frame */
  size = info.size;

  if (need_pool)
    {
      GST_DEBUG_OBJECT (self, "create new pool");
      pool = gst_gl_buffer_pool_new (self->gst_context);

      config = gst_buffer_pool_get_config (pool);
      gst_buffer_pool_config_set_params (config, caps, size, 0, 0);
      gst_buffer_pool_config_add_option (config, GST_BUFFER_POOL_OPTION_GL_SYNC_META);

      if (!gst_buffer_pool_set_config (pool, config))
        {
          GST_DEBUG_OBJECT (bsink, "failed setting config");
          gst_object_unref (pool);
          return FALSE;
        }
    }

  /* we need at least 2 buffer because we hold on to the last one */
  gst_query_add_allocation_pool (query, pool, size, 2, 0);
  if (pool)
    gst_object_unref (pool);

  /* we also support various metadata */
  gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, 0);

  if (self->gst_context->gl_vtable->FenceSync)
    gst_query_add_allocation_meta (query, GST_GL_SYNC_META_API_TYPE, 0);

  return TRUE;
}

static GdkMemoryFormat
gtk_gst_memory_format_from_video_info (GstVideoInfo *info)
{
#define IS_PREMULTIPLIED(_info) (GST_VIDEO_INFO_FLAGS (_info) & GST_VIDEO_FLAG_PREMULTIPLIED_ALPHA)
  switch ((guint) GST_VIDEO_INFO_FORMAT (info))
  {
    case GST_VIDEO_FORMAT_BGRA:
      return IS_PREMULTIPLIED (info) ? GDK_MEMORY_B8G8R8A8_PREMULTIPLIED : GDK_MEMORY_B8G8R8A8;
    case GST_VIDEO_FORMAT_ARGB:
      return IS_PREMULTIPLIED (info) ? GDK_MEMORY_A8R8G8B8_PREMULTIPLIED : GDK_MEMORY_A8R8G8B8;
    case GST_VIDEO_FORMAT_RGBA:
      return IS_PREMULTIPLIED (info) ? GDK_MEMORY_R8G8B8A8_PREMULTIPLIED : GDK_MEMORY_R8G8B8A8;
    case GST_VIDEO_FORMAT_ABGR:
      return IS_PREMULTIPLIED (info) ? GDK_MEMORY_A8B8G8R8_PREMULTIPLIED : GDK_MEMORY_A8B8G8R8;
    case GST_VIDEO_FORMAT_RGB:
      return GDK_MEMORY_R8G8B8;
    case GST_VIDEO_FORMAT_BGR:
      return GDK_MEMORY_B8G8R8;
    default:
      if (GST_VIDEO_INFO_HAS_ALPHA (info))
        return IS_PREMULTIPLIED (info) ? GDK_MEMORY_R8G8B8A8_PREMULTIPLIED : GDK_MEMORY_R8G8B8A8;
      else
        return GDK_MEMORY_R8G8B8;
  }
#undef IS_PREMULTIPLIED
}

static void
video_frame_free (GstVideoFrame *frame)
{
  gst_video_frame_unmap (frame);
  g_free (frame);
}

static GdkTexture *
gtk_gst_sink_texture_from_buffer (GtkGstSink      *self,
                                  GstBuffer       *buffer,
                                  double          *pixel_aspect_ratio,
                                  graphene_rect_t *viewport)
{
  GstVideoFrame *frame = g_new (GstVideoFrame, 1);
  GdkTexture *texture;

  viewport->origin.x = 0;
  viewport->origin.y = 0;
  viewport->size.width = GST_VIDEO_INFO_WIDTH (&self->v_info);
  viewport->size.height = GST_VIDEO_INFO_HEIGHT (&self->v_info);

#ifdef HAVE_GSTREAMER_DRM
  if (gst_is_dmabuf_memory (gst_buffer_peek_memory (buffer, 0)))
    {
      g_autoptr (GdkDmabufTextureBuilder) builder = NULL;
      const GstVideoMeta *vmeta = gst_buffer_get_video_meta (buffer);
      GError *error = NULL;
      int i;

      /* We don't map dmabufs */
      g_clear_pointer (&frame, g_free);

      g_return_val_if_fail (vmeta, NULL);
      g_return_val_if_fail (self->gdk_context, NULL);
      g_return_val_if_fail (self->drm_info.drm_fourcc != DRM_FORMAT_INVALID, NULL);

      builder = gdk_dmabuf_texture_builder_new ();
      gdk_dmabuf_texture_builder_set_display (builder, gdk_gl_context_get_display (self->gdk_context));
      gdk_dmabuf_texture_builder_set_fourcc (builder, self->drm_info.drm_fourcc);
      gdk_dmabuf_texture_builder_set_modifier (builder, self->drm_info.drm_modifier);
      gdk_dmabuf_texture_builder_set_width (builder, vmeta->width);
      gdk_dmabuf_texture_builder_set_height (builder, vmeta->height);
      gdk_dmabuf_texture_builder_set_n_planes (builder, vmeta->n_planes);

      for (i = 0; i < vmeta->n_planes; i++)
        {
          GstMemory *mem;
          guint mem_idx, length;
          gsize skip;

          if (!gst_buffer_find_memory (buffer,
                                       vmeta->offset[i],
                                       1,
                                       &mem_idx,
                                       &length,
                                       &skip))
            {
              GST_ERROR_OBJECT (self, "Buffer data is bogus");
              return NULL;
            }

          mem = gst_buffer_peek_memory (buffer, mem_idx);

          gdk_dmabuf_texture_builder_set_fd (builder, i, gst_dmabuf_memory_get_fd (mem));
          gdk_dmabuf_texture_builder_set_offset (builder, i, mem->offset + skip);
          gdk_dmabuf_texture_builder_set_stride (builder, i, vmeta->stride[i]);
        }

      texture = gdk_dmabuf_texture_builder_build (builder,
                                                  (GDestroyNotify) gst_buffer_unref,
                                                  gst_buffer_ref (buffer),
                                                  &error);
      if (!texture)
        GST_ERROR_OBJECT (self, "Failed to create dmabuf texture: %s", error->message);

      *pixel_aspect_ratio = ((double) GST_VIDEO_INFO_PAR_N (&self->v_info) /
                             (double) GST_VIDEO_INFO_PAR_D (&self->v_info));
    }
  else
#endif
  if (self->gdk_context &&
      gst_video_frame_map (frame, &self->v_info, buffer, GST_MAP_READ | GST_MAP_GL))
    {
      GstGLSyncMeta *sync_meta;
      GdkGLTextureBuilder *builder;

      sync_meta = gst_buffer_get_gl_sync_meta (buffer);
      if (sync_meta)
        gst_gl_sync_meta_set_sync_point (sync_meta, self->gst_context);

      /* Note: using the gdk_context here is a (harmless) lie,
       * since the texture really originates in the gst_context.
       * But that is not a GdkGLContext. It is harmless, because
       * we are never using the texture in the gdk_context, so we
       * never make the (erroneous) decision to ignore the sync.
       */
      builder = gdk_gl_texture_builder_new ();
      gdk_gl_texture_builder_set_context (builder, self->gdk_context);
      gdk_gl_texture_builder_set_format (builder, gtk_gst_memory_format_from_video_info (&frame->info));
      gdk_gl_texture_builder_set_id (builder, *(guint *) frame->data[0]);
      gdk_gl_texture_builder_set_width (builder, frame->info.width);
      gdk_gl_texture_builder_set_height (builder, frame->info.height);
      gdk_gl_texture_builder_set_sync (builder, sync_meta ? sync_meta->data : NULL);

      texture = gdk_gl_texture_builder_build (builder,
                                              (GDestroyNotify) video_frame_free,
                                              frame);

      g_object_unref (builder);

      *pixel_aspect_ratio = ((double) frame->info.par_n) / ((double) frame->info.par_d);
    }
  else if (gst_video_frame_map (frame, &self->v_info, buffer, GST_MAP_READ))
    {
      GBytes *bytes;

      bytes = g_bytes_new_with_free_func (frame->data[0],
                                          frame->info.height * frame->info.stride[0],
                                          (GDestroyNotify) video_frame_free,
                                          frame);
      texture = gdk_memory_texture_new (frame->info.width,
                                        frame->info.height,
                                        gtk_gst_memory_format_from_video_info (&frame->info),
                                        bytes,
                                        frame->info.stride[0]);
      g_bytes_unref (bytes);

      *pixel_aspect_ratio = ((double) frame->info.par_n) / ((double) frame->info.par_d);
    }
  else
    {
      GST_ERROR_OBJECT (self, "Could not convert buffer to texture.");
      texture = NULL;
      g_free (frame);
    }

  return texture;
}

static GstFlowReturn
gtk_gst_sink_show_frame (GstVideoSink *vsink,
                         GstBuffer    *buf)
{
  GtkGstSink *self;
  GdkTexture *texture;
  double pixel_aspect_ratio;
  graphene_rect_t viewport;

  GST_TRACE ("rendering buffer:%p", buf);

  self = GTK_GST_SINK (vsink);

  GST_OBJECT_LOCK (self);

  texture = gtk_gst_sink_texture_from_buffer (self, buf, &pixel_aspect_ratio, &viewport);
  if (texture)
    {
      gtk_gst_paintable_queue_set_texture (self->paintable, texture, pixel_aspect_ratio, &viewport);
      g_object_unref (texture);
    }

  GST_OBJECT_UNLOCK (self);

  return GST_FLOW_OK;
}

static gboolean
gtk_gst_sink_initialize_gl (GtkGstSink *self)
{
  GdkDisplay *display;
  GError *error = NULL;
  GstGLPlatform platform = GST_GL_PLATFORM_NONE;
  GstGLAPI gl_api = GST_GL_API_NONE;
  guintptr gl_handle = 0;
  gboolean succeeded = FALSE;

  display = gdk_gl_context_get_display (self->gdk_context);

  gdk_gl_context_make_current (self->gdk_context);

#ifdef HAVE_GST_X11_SUPPORT
  if (GDK_IS_X11_DISPLAY (display))
    {
      gpointer display_ptr;

#if GST_GL_HAVE_PLATFORM_EGL
      display_ptr = gdk_x11_display_get_egl_display (display);
      if (display_ptr)
        {
          GST_DEBUG_OBJECT (self, "got EGL on X11!");
          platform = GST_GL_PLATFORM_EGL;
          self->gst_display = GST_GL_DISPLAY (gst_gl_display_egl_new_with_egl_display (display_ptr));
        }
#endif
#if GST_GL_HAVE_PLATFORM_GLX
      if (!self->gst_display)
        {
          GST_DEBUG_OBJECT (self, "got GLX on X11!");
          platform = GST_GL_PLATFORM_GLX;
          display_ptr = gdk_x11_display_get_xdisplay (display);
          self->gst_display = GST_GL_DISPLAY (gst_gl_display_x11_new_with_display (display_ptr));
        }
#endif

      gl_api = gst_gl_context_get_current_gl_api (platform, NULL, NULL);
      gl_handle = gst_gl_context_get_current_gl_context (platform);

      if (gl_handle)
        {
          self->gst_gdk_context = gst_gl_context_new_wrapped (self->gst_display, gl_handle, platform, gl_api);
        }
      else
        {
          GST_ERROR_OBJECT (self, "Failed to get handle from GdkGLContext");
          return FALSE;
        }
    }
  else
#endif
#ifdef HAVE_GST_WAYLAND_SUPPORT
  if (GDK_IS_WAYLAND_DISPLAY (display))
    {
      platform = GST_GL_PLATFORM_EGL;

      GST_DEBUG_OBJECT (self, "got EGL on Wayland!");

      gl_api = gst_gl_context_get_current_gl_api (platform, NULL, NULL);
      gl_handle = gst_gl_context_get_current_gl_context (platform);

      if (gl_handle)
        {
          struct wl_display *wayland_display;

          wayland_display = gdk_wayland_display_get_wl_display (display);
          self->gst_display = GST_GL_DISPLAY (gst_gl_display_wayland_new_with_display (wayland_display));
          self->gst_gdk_context = gst_gl_context_new_wrapped (self->gst_display, gl_handle, platform, gl_api);
        }
      else
        {
          GST_ERROR_OBJECT (self, "Failed to get handle from GdkGLContext, not using Wayland EGL");
          return FALSE;
        }
    }
  else
#endif
#if defined(GST_GL_HAVE_PLATFORM_CGL) && defined(GDK_WINDOWING_MACOS)
  if (GDK_IS_MACOS_DISPLAY (display))
    {
      platform = GST_GL_PLATFORM_CGL;

      GST_DEBUG_OBJECT (self, "got CGL on macOS!");

      gl_api = gst_gl_context_get_current_gl_api (platform, NULL, NULL);
      gl_handle = gst_gl_context_get_current_gl_context (platform);

      if (gl_handle)
        {
          self->gst_display = gst_gl_display_new ();
          self->gst_gdk_context = gst_gl_context_new_wrapped (self->gst_display, gl_handle, platform, gl_api);
        }
      else
        {
          GST_ERROR_OBJECT (self, "Failed to get handle from GdkGLContext, not using macOS CGL");
          return FALSE;
        }
    }
  else
#endif
#if GST_GL_HAVE_WINDOW_WIN32 && (GST_GL_HAVE_PLATFORM_WGL || GST_GL_HAVE_PLATFORM_EGL) && defined (GDK_WINDOWING_WIN32)
  if (GDK_IS_WIN32_DISPLAY (display))
    {
      gboolean is_gles = gdk_gl_context_get_use_es (self->gdk_context);
      const char *gl_type = is_gles ? "EGL" : "WGL";

      platform = is_gles ? GST_GL_PLATFORM_EGL : GST_GL_PLATFORM_WGL;
      gl_api = gst_gl_context_get_current_gl_api (platform, NULL, NULL);

      GST_DEBUG_OBJECT (self, "got %s on Win32!", gl_type);

      gl_handle = gst_gl_context_get_current_gl_context (platform);

      if (gl_handle)
        {
          /*
           * We must force a win32 GstGL display type and if using desktop GL, the GL_Platform to be WGL
           * and an appropriate GstGL API depending on the gl_api we receive.  We also ensure that we use
           * an EGL GstGL API if we are using EGL in GDK.  Envvars are required, unless
           * gst_gl_display_new_with_type() is available, unfortunately, so that gst_gl_display_new() does
           * things correctly if we have GstGL built with both EGL and WGL support for the WGL case,
           * otherwise gst_gl_display_new() will assume an EGL display, which won't work for us
           */

          if (gl_api & (GST_GL_API_OPENGL3 | GST_GL_API_OPENGL))
            {
#ifdef HAVE_GST_GL_DISPLAY_NEW_WITH_TYPE
              self->gst_display = gst_gl_display_new_with_type (GST_GL_DISPLAY_TYPE_WIN32);
#else
#if GST_GL_HAVE_PLATFORM_EGL
              g_message ("If media fails to play, set the envvar `GST_DEBUG=1`, and if GstGL context creation fails");
              g_message ("due to \"Couldn't create GL context: Cannot share context with non-EGL context\",");
              g_message ("set in the environment `GST_GL_PLATFORM=wgl` and `GST_GL_WINDOW=win32`,");
              g_message ("and restart the GTK application");
#endif

              self->gst_display = gst_gl_display_new ();
#endif
            }

#if GST_GL_HAVE_PLATFORM_EGL
          else
            {
              gpointer display_ptr = gdk_win32_display_get_egl_display (display);
              self->gst_display = GST_GL_DISPLAY (gst_gl_display_egl_new_with_egl_display (display_ptr));
            }
#endif

          gst_gl_display_filter_gl_api (self->gst_display, gl_api);
          self->gst_gdk_context = gst_gl_context_new_wrapped (self->gst_display, gl_handle, platform, gl_api);
        }
      else
        {
          GST_ERROR_OBJECT (self, "Failed to get handle from GdkGLContext, not using %s", gl_type);
	      return FALSE;
        }
    }
  else
#endif
    {
      GST_INFO_OBJECT (self, "Unsupported GDK display %s for GL", G_OBJECT_TYPE_NAME (display));
      return FALSE;
    }

  g_assert (self->gst_gdk_context != NULL);

  gst_gl_context_activate (self->gst_gdk_context, TRUE);

  if (!gst_gl_context_fill_info (self->gst_gdk_context, &error))
    {
      GST_ERROR_OBJECT (self, "failed to retrieve GDK context info: %s", error->message);
      g_clear_error (&error);
      g_clear_object (&self->gst_gdk_context);
      g_clear_object (&self->gst_display);

      return FALSE;
    }
  else
    {
      gdk_gl_context_clear_current ();
      gst_gl_context_activate (self->gst_gdk_context, FALSE);
    }

  succeeded = gst_gl_display_create_context (self->gst_display, self->gst_gdk_context, &self->gst_context, &error);

  if (!succeeded)
    {
      GST_ERROR_OBJECT (self, "Couldn't create GL context: %s", error->message);
      g_error_free (error);
      g_clear_object (&self->gst_gdk_context);
      g_clear_object (&self->gst_display);
    }

  return succeeded;
}

static void
gtk_gst_sink_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)

{
  GtkGstSink *self = GTK_GST_SINK (object);

  switch (prop_id)
    {
    case PROP_PAINTABLE:
      self->paintable = g_value_dup_object (value);
      if (self->paintable == NULL)
        self->paintable = GTK_GST_PAINTABLE (gtk_gst_paintable_new ());
      break;

    case PROP_GL_CONTEXT:
      self->gdk_context = g_value_dup_object (value);
      if (self->gdk_context != NULL && !gtk_gst_sink_initialize_gl (self))
        g_clear_object (&self->gdk_context);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_gst_sink_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  GtkGstSink *self = GTK_GST_SINK (object);

  switch (prop_id)
    {
    case PROP_PAINTABLE:
      g_value_set_object (value, self->paintable);
      break;
    case PROP_GL_CONTEXT:
      g_value_set_object (value, self->gdk_context);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_gst_sink_dispose (GObject *object)
{
  GtkGstSink *self = GTK_GST_SINK (object);

  g_clear_object (&self->paintable);
  g_clear_object (&self->gst_gdk_context);
  g_clear_object (&self->gst_display);
  g_clear_object (&self->gdk_context);

  G_OBJECT_CLASS (gtk_gst_sink_parent_class)->dispose (object);
}

static void
gtk_gst_sink_class_init (GtkGstSinkClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GstBaseSinkClass *gstbasesink_class = GST_BASE_SINK_CLASS (klass);
  GstVideoSinkClass *gstvideosink_class = GST_VIDEO_SINK_CLASS (klass);

  gobject_class->set_property = gtk_gst_sink_set_property;
  gobject_class->get_property = gtk_gst_sink_get_property;
  gobject_class->dispose = gtk_gst_sink_dispose;

  gstbasesink_class->set_caps = gtk_gst_sink_set_caps;
  gstbasesink_class->get_times = gtk_gst_sink_get_times;
  gstbasesink_class->query = gtk_gst_sink_query;
  gstbasesink_class->propose_allocation = gtk_gst_sink_propose_allocation;
  gstbasesink_class->get_caps = gtk_gst_sink_get_caps;

  gstvideosink_class->show_frame = gtk_gst_sink_show_frame;

  /**
   * GtkGstSink:paintable:
   *
   * The paintable that provides the picture for this sink.
   */
  properties[PROP_PAINTABLE] =
    g_param_spec_object ("paintable", NULL, NULL,
                         GTK_TYPE_GST_PAINTABLE,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkGstSink:gl-context:
   *
   * The #GdkGLContext to use for GL rendering.
   */
  properties[PROP_GL_CONTEXT] =
    g_param_spec_object ("gl-context", NULL, NULL,
                         GDK_TYPE_GL_CONTEXT,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPS, properties);

  gst_element_class_set_metadata (gstelement_class,
      "GtkMediaStream Video Sink",
      "Sink/Video", "The video sink used by GtkMediaStream",
      "Matthew Waters <matthew@centricular.com>, "
      "Benjamin Otte <otte@gnome.org>");

  gst_element_class_add_static_pad_template (gstelement_class,
      &gtk_gst_sink_template);
}

static void
gtk_gst_sink_init (GtkGstSink * gtk_sink)
{
}

