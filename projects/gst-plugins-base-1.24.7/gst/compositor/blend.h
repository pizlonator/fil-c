/*
 * Copyright (C) 2009 Sebastian Dröge <sebastian.droege@collabora.co.uk>
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

#ifndef __BLEND_H__
#define __BLEND_H__

#include <gst/gst.h>
#include <gst/video/video.h>

/**
 * GstCompositorBlendMode:
 * @COMPOSITOR_BLEND_MODE_SOURCE: Copy source
 * @COMPOSITOR_BLEND_MODE_OVER: Normal blending
 * @COMPOSITOR_BLEND_MODE_ADD: Alphas are simply added,
 *
 * The different modes compositor can use for blending.
 */
typedef enum
{
  COMPOSITOR_BLEND_MODE_SOURCE,
  COMPOSITOR_BLEND_MODE_OVER,
  COMPOSITOR_BLEND_MODE_ADD,
} GstCompositorBlendMode;

/*
 * @srcframe: source #GstVideoFrame
 * @xpos: horizontal start position of @srcframe, leftmost pixel line.
 * @ypos: vertical start position of @srcframe, topmost pixel line.
 * @gdouble: src_alpha, alpha factor applied to @srcframe
 * @destframe: destination #GstVideoFrame
 * @dst_y_start: start position of where to write into @destframe. Used for splitting work across multiple sequences.
 * @dst_y_end: end position of where to write into @destframe. Used for splitting work across multiple sequences.
 */
typedef void (*BlendFunction) (GstVideoFrame *srcframe, gint xpos, gint ypos, gdouble src_alpha, GstVideoFrame * destframe,
    gint dst_y_start, gint dst_y_end, GstCompositorBlendMode mode);
typedef void (*FillCheckerFunction) (GstVideoFrame * frame, guint y_start, guint y_end);
typedef void (*FillColorFunction) (GstVideoFrame * frame, guint y_start, guint y_end, gint c1, gint c2, gint c3);

extern BlendFunction gst_compositor_blend_argb;
extern BlendFunction gst_compositor_blend_bgra;
#define gst_compositor_blend_ayuv gst_compositor_blend_argb
#define gst_compositor_blend_vuya gst_compositor_blend_bgra
#define gst_compositor_blend_abgr gst_compositor_blend_argb
#define gst_compositor_blend_rgba gst_compositor_blend_bgra

extern BlendFunction gst_compositor_overlay_argb;
extern BlendFunction gst_compositor_overlay_bgra;
#define gst_compositor_overlay_ayuv gst_compositor_overlay_argb
#define gst_compositor_overlay_vuya gst_compositor_overlay_bgra
#define gst_compositor_overlay_abgr gst_compositor_overlay_argb
#define gst_compositor_overlay_rgba gst_compositor_overlay_bgra
extern BlendFunction gst_compositor_overlay_argb64;
#define gst_compositor_overlay_ayuv64 gst_compositor_overlay_argb64;

extern BlendFunction gst_compositor_blend_i420;
#define gst_compositor_blend_yv12 gst_compositor_blend_i420
extern BlendFunction gst_compositor_blend_nv12;
extern BlendFunction gst_compositor_blend_nv21;
extern BlendFunction gst_compositor_blend_y41b;
extern BlendFunction gst_compositor_blend_y42b;
extern BlendFunction gst_compositor_blend_y444;
extern BlendFunction gst_compositor_blend_rgb;
#define gst_compositor_blend_bgr gst_compositor_blend_rgb
extern BlendFunction gst_compositor_blend_rgbx;
#define gst_compositor_blend_bgrx gst_compositor_blend_rgbx
#define gst_compositor_blend_xrgb gst_compositor_blend_rgbx
#define gst_compositor_blend_xbgr gst_compositor_blend_rgbx
extern BlendFunction gst_compositor_blend_yuy2;
#define gst_compositor_blend_uyvy gst_compositor_blend_yuy2;
#define gst_compositor_blend_yvyu gst_compositor_blend_yuy2;
extern BlendFunction gst_compositor_blend_i420_10le;
extern BlendFunction gst_compositor_blend_i420_10be;
extern BlendFunction gst_compositor_blend_i420_12le;
extern BlendFunction gst_compositor_blend_i420_12be;
extern BlendFunction gst_compositor_blend_i422_10le;
extern BlendFunction gst_compositor_blend_i422_10be;
extern BlendFunction gst_compositor_blend_i422_12le;
extern BlendFunction gst_compositor_blend_i422_12be;
extern BlendFunction gst_compositor_blend_y444_10le;
extern BlendFunction gst_compositor_blend_y444_10be;
extern BlendFunction gst_compositor_blend_y444_12le;
extern BlendFunction gst_compositor_blend_y444_12be;
extern BlendFunction gst_compositor_blend_y444_16le;
extern BlendFunction gst_compositor_blend_y444_16be;
extern BlendFunction gst_compositor_blend_argb64;
#define gst_compositor_blend_ayuv64 gst_compositor_blend_argb64;


extern FillCheckerFunction gst_compositor_fill_checker_argb;
#define gst_compositor_fill_checker_abgr gst_compositor_fill_checker_argb
extern FillCheckerFunction gst_compositor_fill_checker_bgra;
#define gst_compositor_fill_checker_rgba gst_compositor_fill_checker_bgra
extern FillCheckerFunction gst_compositor_fill_checker_ayuv;
extern FillCheckerFunction gst_compositor_fill_checker_vuya;
extern FillCheckerFunction gst_compositor_fill_checker_i420;
#define gst_compositor_fill_checker_yv12 gst_compositor_fill_checker_i420
extern FillCheckerFunction gst_compositor_fill_checker_nv12;
extern FillCheckerFunction gst_compositor_fill_checker_nv21;
extern FillCheckerFunction gst_compositor_fill_checker_y41b;
extern FillCheckerFunction gst_compositor_fill_checker_y42b;
extern FillCheckerFunction gst_compositor_fill_checker_y444;
extern FillCheckerFunction gst_compositor_fill_checker_rgb;
#define gst_compositor_fill_checker_bgr gst_compositor_fill_checker_rgb
extern FillCheckerFunction gst_compositor_fill_checker_rgbx;
#define gst_compositor_fill_checker_bgrx gst_compositor_fill_checker_rgbx
extern FillCheckerFunction gst_compositor_fill_checker_xrgb;
#define gst_compositor_fill_checker_xbgr gst_compositor_fill_checker_xrgb
extern FillCheckerFunction gst_compositor_fill_checker_yuy2;
#define gst_compositor_fill_checker_yvyu gst_compositor_fill_checker_yuy2;
extern FillCheckerFunction gst_compositor_fill_checker_uyvy;
extern FillCheckerFunction gst_compositor_fill_checker_i420_10le;
#define gst_compositor_fill_checker_i422_10le gst_compositor_fill_checker_i420_10le
#define gst_compositor_fill_checker_y444_10le gst_compositor_fill_checker_i420_10le
extern FillCheckerFunction gst_compositor_fill_checker_i420_10be;
#define gst_compositor_fill_checker_i422_10be gst_compositor_fill_checker_i420_10be
#define gst_compositor_fill_checker_y444_10be gst_compositor_fill_checker_i420_10be
extern FillCheckerFunction gst_compositor_fill_checker_i420_12le;
#define gst_compositor_fill_checker_i422_12le gst_compositor_fill_checker_i420_12le
#define gst_compositor_fill_checker_y444_12le gst_compositor_fill_checker_i420_12le
extern FillCheckerFunction gst_compositor_fill_checker_i420_12be;
#define gst_compositor_fill_checker_i422_12be gst_compositor_fill_checker_i420_12be
#define gst_compositor_fill_checker_y444_12be gst_compositor_fill_checker_i420_12be
extern FillCheckerFunction gst_compositor_fill_checker_y444_16le;
extern FillCheckerFunction gst_compositor_fill_checker_y444_16be;
extern FillCheckerFunction gst_compositor_fill_checker_argb64;
extern FillCheckerFunction gst_compositor_fill_checker_ayuv64;

extern FillColorFunction gst_compositor_fill_color_argb;
extern FillColorFunction gst_compositor_fill_color_abgr;
extern FillColorFunction gst_compositor_fill_color_bgra;
extern FillColorFunction gst_compositor_fill_color_rgba;
extern FillColorFunction gst_compositor_fill_color_ayuv;
extern FillColorFunction gst_compositor_fill_color_vuya;
extern FillColorFunction gst_compositor_fill_color_i420;
extern FillColorFunction gst_compositor_fill_color_yv12;
extern FillColorFunction gst_compositor_fill_color_nv12;
#define gst_compositor_fill_color_nv21 gst_compositor_fill_color_nv12;
extern FillColorFunction gst_compositor_fill_color_y41b;
extern FillColorFunction gst_compositor_fill_color_y42b;
extern FillColorFunction gst_compositor_fill_color_y444;
extern FillColorFunction gst_compositor_fill_color_rgb;
extern FillColorFunction gst_compositor_fill_color_bgr;
extern FillColorFunction gst_compositor_fill_color_xrgb;
extern FillColorFunction gst_compositor_fill_color_xbgr;
extern FillColorFunction gst_compositor_fill_color_rgbx;
extern FillColorFunction gst_compositor_fill_color_bgrx;
extern FillColorFunction gst_compositor_fill_color_yuy2;
extern FillColorFunction gst_compositor_fill_color_yvyu;
extern FillColorFunction gst_compositor_fill_color_uyvy;
extern FillColorFunction gst_compositor_fill_color_i420_10le;
#define gst_compositor_fill_color_i422_10le gst_compositor_fill_color_i420_10le
#define gst_compositor_fill_color_y444_10le gst_compositor_fill_color_i420_10le
extern FillColorFunction gst_compositor_fill_color_i420_10be;
#define gst_compositor_fill_color_i422_10be gst_compositor_fill_color_i420_10be
#define gst_compositor_fill_color_y444_10be gst_compositor_fill_color_i420_10be
extern FillColorFunction gst_compositor_fill_color_i420_12le;
#define gst_compositor_fill_color_i422_12le gst_compositor_fill_color_i420_12le
#define gst_compositor_fill_color_y444_12le gst_compositor_fill_color_i420_12le
extern FillColorFunction gst_compositor_fill_color_i420_12be;
#define gst_compositor_fill_color_i422_12be gst_compositor_fill_color_i420_12be
#define gst_compositor_fill_color_y444_12be gst_compositor_fill_color_i420_12be
extern FillColorFunction gst_compositor_fill_color_y444_16le;
extern FillColorFunction gst_compositor_fill_color_y444_16be;
extern FillColorFunction gst_compositor_fill_color_argb64;
#define gst_compositor_fill_color_ayuv64 gst_compositor_fill_color_argb64;

void gst_compositor_init_blend (void);

#endif /* __BLEND_H__ */
