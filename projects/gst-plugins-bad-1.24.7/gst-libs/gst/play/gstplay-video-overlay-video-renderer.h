/* GStreamer
 *
 * Copyright (C) 2014-2015 Sebastian Dröge <sebastian@centricular.com>
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

#ifndef __GST_PLAY_VIDEO_OVERLAY_VIDEO_RENDERER_H__
#define __GST_PLAY_VIDEO_OVERLAY_VIDEO_RENDERER_H__

#include <gst/play/gstplay-types.h>
#include <gst/play/gstplay-video-renderer.h>

G_BEGIN_DECLS

/**
 * GstPlayVideoOverlayVideoRenderer:
 * Since: 1.20
 */
typedef struct _GstPlayVideoOverlayVideoRenderer
    GstPlayVideoOverlayVideoRenderer;
typedef struct _GstPlayVideoOverlayVideoRendererClass
    GstPlayVideoOverlayVideoRendererClass;

#define GST_TYPE_PLAY_VIDEO_OVERLAY_VIDEO_RENDERER             (gst_play_video_overlay_video_renderer_get_type ())
#define GST_IS_PLAY_VIDEO_OVERLAY_VIDEO_RENDERER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_PLAY_VIDEO_OVERLAY_VIDEO_RENDERER))
#define GST_IS_PLAY_VIDEO_OVERLAY_VIDEO_RENDERER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_PLAY_VIDEO_OVERLAY_VIDEO_RENDERER))
#define GST_PLAY_VIDEO_OVERLAY_VIDEO_RENDERER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_PLAY_VIDEO_OVERLAY_VIDEO_RENDERER, GstPlayVideoOverlayVideoRendererClass))
#define GST_PLAY_VIDEO_OVERLAY_VIDEO_RENDERER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_PLAY_VIDEO_OVERLAY_VIDEO_RENDERER, GstPlayVideoOverlayVideoRenderer))
#define GST_PLAY_VIDEO_OVERLAY_VIDEO_RENDERER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_PLAY_VIDEO_OVERLAY_VIDEO_RENDERER, GstPlayVideoOverlayVideoRendererClass))

/**
 * GST_PLAY_VIDEO_OVERLAY_VIDEO_RENDERER_CAST:
 * Since: 1.20
 */
#define GST_PLAY_VIDEO_OVERLAY_VIDEO_RENDERER_CAST(obj)        ((GstPlayVideoOverlayVideoRenderer*)(obj))

#ifdef G_DEFINE_AUTOPTR_CLEANUP_FUNC
G_DEFINE_AUTOPTR_CLEANUP_FUNC(GstPlayVideoOverlayVideoRenderer, g_object_unref)
#endif

GST_PLAY_API
GType gst_play_video_overlay_video_renderer_get_type (void);

GST_PLAY_API
GstPlayVideoRenderer * gst_play_video_overlay_video_renderer_new (gpointer window_handle);

GST_PLAY_API
GstPlayVideoRenderer * gst_play_video_overlay_video_renderer_new_with_sink (gpointer window_handle, GstElement * video_sink);

GST_PLAY_API
void gst_play_video_overlay_video_renderer_set_window_handle (GstPlayVideoOverlayVideoRenderer * self, gpointer window_handle);

GST_PLAY_API
gpointer gst_play_video_overlay_video_renderer_get_window_handle (GstPlayVideoOverlayVideoRenderer * self);

GST_PLAY_API
void gst_play_video_overlay_video_renderer_expose (GstPlayVideoOverlayVideoRenderer * self);

GST_PLAY_API
void gst_play_video_overlay_video_renderer_set_render_rectangle (GstPlayVideoOverlayVideoRenderer * self, gint x, gint y, gint width, gint height);

GST_PLAY_API
void gst_play_video_overlay_video_renderer_get_render_rectangle (GstPlayVideoOverlayVideoRenderer * self, gint *x, gint *y, gint *width, gint *height);

G_END_DECLS

#endif /* __GST_PLAY_VIDEO_OVERLAY_VIDEO_RENDERER_H__ */
