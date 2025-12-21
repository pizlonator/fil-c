/*
 * Copyright (C) 2004 Wim Taymans <wim@fluendo.com>
 * Copyright (C) 2006 Mindfruit Bv.
 *   Author: Sjoerd Simons <sjoerd@luon.net>
 *   Author: Alex Ugarte <alexugarte@gmail.com>
 * Copyright (C) 2009 Alex Ugarte <augarte@vicomtech.org>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "blend.h"
#include "compositororc.h"

#include <string.h>

#include <gst/video/video.h>

GST_DEBUG_CATEGORY_STATIC (gst_compositor_blend_debug);
#define GST_CAT_DEFAULT gst_compositor_blend_debug

/* Below are the implementations of everything */

/* A32 is for AYUV, VUYA, ARGB and BGRA */
#define BLEND_A32(name, method, LOOP)		\
static void \
method##_ ##name (GstVideoFrame * srcframe, gint xpos, gint ypos, \
    gdouble src_alpha, GstVideoFrame * destframe, gint dst_y_start, \
    gint dst_y_end, GstCompositorBlendMode mode) \
{ \
  guint s_alpha; \
  gint src_stride, dest_stride; \
  gint dest_width, dest_height; \
  guint8 *src, *dest; \
  gint src_width, src_height; \
  \
  src_width = GST_VIDEO_FRAME_WIDTH (srcframe); \
  src_height = GST_VIDEO_FRAME_HEIGHT (srcframe); \
  src = GST_VIDEO_FRAME_PLANE_DATA (srcframe, 0); \
  src_stride = GST_VIDEO_FRAME_COMP_STRIDE (srcframe, 0); \
  dest = GST_VIDEO_FRAME_PLANE_DATA (destframe, 0); \
  dest_stride = GST_VIDEO_FRAME_COMP_STRIDE (destframe, 0); \
  dest_width = GST_VIDEO_FRAME_COMP_WIDTH (destframe, 0); \
  dest_height = GST_VIDEO_FRAME_COMP_HEIGHT (destframe, 0); \
  \
  s_alpha = CLAMP ((gint) (src_alpha * 255), 0, 255); \
  \
  /* If it's completely transparent... we just return */ \
  if (G_UNLIKELY (s_alpha == 0)) \
    return; \
  \
  if (dst_y_end > dest_height) { \
    dst_y_end = dest_height; \
  } \
  /* adjust src pointers for negative sizes */ \
  if (xpos < 0) { \
    src += -xpos * 4; \
    src_width -= -xpos; \
    xpos = 0; \
  } \
  if (ypos < dst_y_start) { \
    src += (dst_y_start - ypos) * src_stride; \
    src_height -= dst_y_start - ypos; \
    ypos = dst_y_start; \
  } \
  /* adjust width/height if the src is bigger than dest */ \
  if (xpos + src_width > dest_width) { \
    src_width = dest_width - xpos; \
  } \
  if (ypos + src_height > dst_y_end) { \
    src_height = dst_y_end - ypos; \
  } \
  \
  if (src_height > 0 && src_width > 0) { \
    dest = dest + 4 * xpos + (ypos * dest_stride); \
  \
    LOOP (dest, src, src_height, src_width, src_stride, dest_stride, s_alpha, \
        mode); \
  } \
}

#define OVERLAY_A32_LOOP(name)			\
static inline void \
_overlay_loop_##name (guint8 * dest, const guint8 * src, gint src_height, \
    gint src_width, gint src_stride, gint dest_stride, guint s_alpha, \
    GstCompositorBlendMode mode) \
{ \
  s_alpha = MIN (255, s_alpha); \
  switch (mode) { \
    case COMPOSITOR_BLEND_MODE_SOURCE:\
      if (s_alpha == 255) { \
        guint y; \
        for (y = 0; y < src_height; y++) { \
          memcpy (dest, src, 4 * src_width); \
          dest += dest_stride; \
          src += src_stride; \
        } \
      } else { \
        compositor_orc_source_##name (dest, dest_stride, src, src_stride, \
          s_alpha, src_width, src_height); \
      } \
      break;\
    case COMPOSITOR_BLEND_MODE_OVER:\
      compositor_orc_overlay_##name (dest, dest_stride, src, src_stride, \
        s_alpha, src_width, src_height); \
      break;\
    case COMPOSITOR_BLEND_MODE_ADD:\
      compositor_orc_overlay_##name##_addition (dest, dest_stride, src, src_stride, \
        s_alpha, src_width, src_height); \
      break;\
  }\
}

#define BLEND_A32_LOOP(name) \
static inline void \
_blend_loop_##name (guint8 * dest, const guint8 * src, gint src_height, \
    gint src_width, gint src_stride, gint dest_stride, guint s_alpha, \
    GstCompositorBlendMode mode) \
{ \
  s_alpha = MIN (255, s_alpha); \
  switch (mode) { \
    case COMPOSITOR_BLEND_MODE_SOURCE:\
      if (s_alpha == 255) { \
        guint y; \
        for (y = 0; y < src_height; y++) { \
          memcpy (dest, src, 4 * src_width); \
          dest += dest_stride; \
          src += src_stride; \
        } \
      } else { \
        compositor_orc_source_##name (dest, dest_stride, src, src_stride, \
          s_alpha, src_width, src_height); \
      } \
      break;\
    case COMPOSITOR_BLEND_MODE_OVER:\
    case COMPOSITOR_BLEND_MODE_ADD:\
      /* both modes are the same for opaque background */ \
      compositor_orc_blend_##name (dest, dest_stride, src, src_stride, \
        s_alpha, src_width, src_height); \
      break;\
  }\
}

OVERLAY_A32_LOOP (argb);
OVERLAY_A32_LOOP (bgra);
BLEND_A32_LOOP (argb);
BLEND_A32_LOOP (bgra);

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
BLEND_A32 (argb, blend, _blend_loop_argb);
BLEND_A32 (bgra, blend, _blend_loop_bgra);
BLEND_A32 (argb, overlay, _overlay_loop_argb);
BLEND_A32 (bgra, overlay, _overlay_loop_bgra);
#else
BLEND_A32 (argb, blend, _blend_loop_bgra);
BLEND_A32 (bgra, blend, _blend_loop_argb);
BLEND_A32 (argb, overlay, _overlay_loop_bgra);
BLEND_A32 (bgra, overlay, _overlay_loop_argb);
#endif

#define A32_CHECKER_C(name, RGB, A, C1, C2, C3) \
static void \
fill_checker_##name##_c (GstVideoFrame * frame, guint y_start, guint y_end) \
{ \
  gint i, j; \
  gint val; \
  static const gint tab[] = { 80, 160, 80, 160 }; \
  gint width, stride; \
  guint8 *dest; \
  \
  dest = GST_VIDEO_FRAME_PLANE_DATA (frame, 0); \
  width = GST_VIDEO_FRAME_COMP_WIDTH (frame, 0); \
  stride = GST_VIDEO_FRAME_COMP_STRIDE (frame, 0); \
  \
  dest += y_start * stride; \
  if (!RGB) { \
    for (i = y_start; i < y_end; i++) { \
      for (j = 0; j < width; j++) { \
        dest[A] = 0xff; \
        dest[C1] = tab[((i & 0x8) >> 3) + ((j & 0x8) >> 3)]; \
        dest[C2] = 128; \
        dest[C3] = 128; \
        dest += 4; \
      } \
    } \
  } else { \
    for (i = y_start; i < y_end; i++) { \
      for (j = 0; j < width; j++) { \
        val = tab[((i & 0x8) >> 3) + ((j & 0x8) >> 3)]; \
        dest[A] = 0xFF; \
        dest[C1] = val; \
        dest[C2] = val; \
        dest[C3] = val; \
        dest += 4; \
      } \
    } \
  } \
}

A32_CHECKER_C (argb, TRUE, 0, 1, 2, 3);
A32_CHECKER_C (bgra, TRUE, 3, 2, 1, 0);
A32_CHECKER_C (ayuv, FALSE, 0, 1, 2, 3);
A32_CHECKER_C (vuya, FALSE, 3, 2, 1, 0);

#define A32_COLOR(name, A, C1, C2, C3) \
static void \
fill_color_##name (GstVideoFrame * frame, guint y_start, guint y_end, gint c1, gint c2, gint c3) \
{ \
  guint32 val; \
  gint stride; \
  guint8 *dest; \
  \
  dest = GST_VIDEO_FRAME_PLANE_DATA (frame, 0); \
  stride = GST_VIDEO_FRAME_COMP_STRIDE (frame, 0); \
  \
  dest += y_start * stride; \
  val = GUINT32_FROM_BE ((0xff << A) | (c1 << C1) | (c2 << C2) | (c3 << C3)); \
  \
  compositor_orc_splat_u32 ((guint32 *) dest, val, (y_end - y_start) * (stride / 4)); \
}

A32_COLOR (argb, 24, 16, 8, 0);
A32_COLOR (bgra, 0, 8, 16, 24);
A32_COLOR (abgr, 24, 0, 8, 16);
A32_COLOR (rgba, 0, 24, 16, 8);
A32_COLOR (ayuv, 24, 16, 8, 0);
A32_COLOR (vuya, 0, 8, 16, 24);

/* Y444, Y42B, I420, YV12, Y41B */
#define PLANAR_YUV_BLEND(format_name,x_round,y_round,MEMCPY,BLENDLOOP,n_bits) \
inline static void \
_blend_##format_name (const guint8 * src, guint8 * dest, \
    gint src_stride, gint dest_stride, gint pstride, gint src_width, gint src_height, \
    gdouble src_alpha, GstCompositorBlendMode mode) \
{ \
  gint i; \
  gint b_alpha; \
  gint range; \
  \
  /* in source mode we just have to copy over things */ \
  if (mode == COMPOSITOR_BLEND_MODE_SOURCE) { \
    src_alpha = 1.0; \
  } \
  \
  /* If it's completely transparent... we just return */ \
  if (G_UNLIKELY (src_alpha == 0.0)) { \
    GST_LOG ("Fast copy (alpha == 0.0)"); \
    return; \
  } \
  \
  /* If it's completely opaque, we do a fast copy */ \
  if (G_UNLIKELY (src_alpha == 1.0)) { \
    gint width_in_bytes = src_width * pstride; \
    GST_LOG ("Fast copy (alpha == 1.0)"); \
    for (i = 0; i < src_height; i++) { \
      MEMCPY (dest, src, width_in_bytes); \
      src += src_stride; \
      dest += dest_stride; \
    } \
    return; \
  } \
  \
  range = (1 << n_bits) - 1; \
  b_alpha = CLAMP ((gint) (src_alpha * range), 0, range); \
  \
  BLENDLOOP(dest, dest_stride, src, src_stride, b_alpha, src_width, src_height);\
} \
\
static void \
blend_##format_name (GstVideoFrame * srcframe, gint xpos, gint ypos, \
    gdouble src_alpha, GstVideoFrame * destframe, gint dst_y_start, \
    gint dst_y_end, GstCompositorBlendMode mode) \
{ \
  const guint8 *b_src; \
  guint8 *b_dest; \
  gint b_src_width; \
  gint b_src_height; \
  gint xoffset = 0; \
  gint yoffset = 0; \
  gint src_comp_rowstride, dest_comp_rowstride; \
  gint src_comp_height; \
  gint src_comp_width; \
  gint comp_ypos, comp_xpos; \
  gint comp_yoffset, comp_xoffset; \
  gint dest_width, dest_height; \
  const GstVideoFormatInfo *info; \
  gint src_width, src_height; \
  gint pstride; \
  \
  src_width = GST_VIDEO_FRAME_WIDTH (srcframe); \
  src_height = GST_VIDEO_FRAME_HEIGHT (srcframe); \
  \
  info = srcframe->info.finfo; \
  dest_width = GST_VIDEO_FRAME_WIDTH (destframe); \
  dest_height = GST_VIDEO_FRAME_HEIGHT (destframe); \
  \
  if (dst_y_end > dest_height) { \
    dst_y_end = dest_height; \
  } \
  xpos = x_round (xpos); \
  ypos = y_round (ypos); \
  \
  b_src_width = src_width; \
  b_src_height = src_height; \
  \
  /* adjust src pointers for negative sizes */ \
  if (xpos < 0) { \
    xoffset = -xpos; \
    b_src_width -= -xpos; \
    xpos = 0; \
  } \
  if (ypos < dst_y_start) { \
    yoffset = dst_y_start - ypos; \
    b_src_height -= dst_y_start - ypos; \
    ypos = dst_y_start; \
  } \
  /* If x or y offset are larger then the source it's outside of the picture */ \
  if (xoffset >= src_width || yoffset >= src_height) { \
    return; \
  } \
  \
  /* adjust width/height if the src is bigger than dest */ \
  if (xpos + b_src_width > dest_width) { \
    b_src_width = dest_width - xpos; \
  } \
  if (ypos + b_src_height > dst_y_end) { \
    b_src_height = dst_y_end - ypos; \
  } \
  if (b_src_width <= 0 || b_src_height <= 0) { \
    return; \
  } \
  \
  /* First mix Y, then U, then V */ \
  b_src = GST_VIDEO_FRAME_COMP_DATA (srcframe, 0); \
  b_dest = GST_VIDEO_FRAME_COMP_DATA (destframe, 0); \
  src_comp_rowstride = GST_VIDEO_FRAME_COMP_STRIDE (srcframe, 0); \
  dest_comp_rowstride = GST_VIDEO_FRAME_COMP_STRIDE (destframe, 0); \
  src_comp_width = GST_VIDEO_FORMAT_INFO_SCALE_WIDTH(info, 0, b_src_width); \
  src_comp_height = GST_VIDEO_FORMAT_INFO_SCALE_HEIGHT(info, 0, b_src_height); \
  pstride = GST_VIDEO_FORMAT_INFO_PSTRIDE (info, 0); \
  comp_xpos = (xpos == 0) ? 0 : GST_VIDEO_FORMAT_INFO_SCALE_WIDTH (info, 0, xpos); \
  comp_ypos = (ypos == 0) ? 0 : GST_VIDEO_FORMAT_INFO_SCALE_HEIGHT (info, 0, ypos); \
  comp_xoffset = (xoffset == 0) ? 0 : GST_VIDEO_FORMAT_INFO_SCALE_WIDTH (info, 0, xoffset); \
  comp_yoffset = (yoffset == 0) ? 0 : GST_VIDEO_FORMAT_INFO_SCALE_HEIGHT (info, 0, yoffset); \
  _blend_##format_name (b_src + comp_xoffset * pstride + comp_yoffset * src_comp_rowstride, \
      b_dest + comp_xpos * pstride + comp_ypos * dest_comp_rowstride, \
      src_comp_rowstride, \
      dest_comp_rowstride, pstride, src_comp_width, src_comp_height, \
      src_alpha, mode); \
  \
  b_src = GST_VIDEO_FRAME_COMP_DATA (srcframe, 1); \
  b_dest = GST_VIDEO_FRAME_COMP_DATA (destframe, 1); \
  src_comp_rowstride = GST_VIDEO_FRAME_COMP_STRIDE (srcframe, 1); \
  dest_comp_rowstride = GST_VIDEO_FRAME_COMP_STRIDE (destframe, 1); \
  src_comp_width = GST_VIDEO_FORMAT_INFO_SCALE_WIDTH(info, 1, b_src_width); \
  src_comp_height = GST_VIDEO_FORMAT_INFO_SCALE_HEIGHT(info, 1, b_src_height); \
  pstride = GST_VIDEO_FORMAT_INFO_PSTRIDE (info, 1); \
  comp_xpos = (xpos == 0) ? 0 : GST_VIDEO_FORMAT_INFO_SCALE_WIDTH (info, 1, xpos); \
  comp_ypos = (ypos == 0) ? 0 : ypos >> info->h_sub[1]; \
  comp_xoffset = (xoffset == 0) ? 0 : GST_VIDEO_FORMAT_INFO_SCALE_WIDTH (info, 1, xoffset); \
  comp_yoffset = (yoffset == 0) ? 0 : yoffset >> info->h_sub[1]; \
  _blend_##format_name (b_src + comp_xoffset * pstride + comp_yoffset * src_comp_rowstride, \
      b_dest + comp_xpos * pstride + comp_ypos * dest_comp_rowstride, \
      src_comp_rowstride, \
      dest_comp_rowstride, pstride, src_comp_width, src_comp_height, \
      src_alpha, mode); \
  \
  b_src = GST_VIDEO_FRAME_COMP_DATA (srcframe, 2); \
  b_dest = GST_VIDEO_FRAME_COMP_DATA (destframe, 2); \
  src_comp_rowstride = GST_VIDEO_FRAME_COMP_STRIDE (srcframe, 2); \
  dest_comp_rowstride = GST_VIDEO_FRAME_COMP_STRIDE (destframe, 2); \
  src_comp_width = GST_VIDEO_FORMAT_INFO_SCALE_WIDTH(info, 2, b_src_width); \
  src_comp_height = GST_VIDEO_FORMAT_INFO_SCALE_HEIGHT(info, 2, b_src_height); \
  pstride = GST_VIDEO_FORMAT_INFO_PSTRIDE (info, 2); \
  comp_xpos = (xpos == 0) ? 0 : GST_VIDEO_FORMAT_INFO_SCALE_WIDTH (info, 2, xpos); \
  comp_ypos = (ypos == 0) ? 0 : ypos >> info->h_sub[2]; \
  comp_xoffset = (xoffset == 0) ? 0 : GST_VIDEO_FORMAT_INFO_SCALE_WIDTH (info, 2, xoffset); \
  comp_yoffset = (yoffset == 0) ? 0 : yoffset >> info->h_sub[2]; \
  _blend_##format_name (b_src + comp_xoffset * pstride + comp_yoffset * src_comp_rowstride, \
      b_dest + comp_xpos * pstride + comp_ypos * dest_comp_rowstride, \
      src_comp_rowstride, \
      dest_comp_rowstride, pstride, src_comp_width, src_comp_height, \
      src_alpha, mode); \
}

#define PLANAR_YUV_FILL_CHECKER(format_name, format_enum, MEMSET) \
static void \
fill_checker_##format_name (GstVideoFrame * frame, guint y_start, guint y_end) \
{ \
  gint i, j; \
  static const int tab[] = { 80, 160, 80, 160 }; \
  guint8 *p; \
  gint comp_width, comp_height; \
  gint rowstride, comp_yoffset; \
  const GstVideoFormatInfo *info; \
  \
  info = frame->info.finfo; \
  p = GST_VIDEO_FRAME_COMP_DATA (frame, 0); \
  comp_width = GST_VIDEO_FRAME_COMP_WIDTH (frame, 0); \
  comp_height = GST_VIDEO_FORMAT_INFO_SCALE_HEIGHT(info, 0, y_end - y_start); \
  rowstride = GST_VIDEO_FRAME_COMP_STRIDE (frame, 0); \
  comp_yoffset = (y_start == 0) ? 0 : GST_VIDEO_FORMAT_INFO_SCALE_HEIGHT (info, 0, y_start); \
  p += comp_yoffset * rowstride; \
  \
  for (i = 0; i < comp_height; i++) { \
    for (j = 0; j < comp_width; j++) { \
      *p++ = tab[(((i + y_start) & 0x8) >> 3) + ((j & 0x8) >> 3)]; \
    } \
    p += rowstride - comp_width; \
  } \
  \
  p = GST_VIDEO_FRAME_COMP_DATA (frame, 1); \
  comp_width = GST_VIDEO_FRAME_COMP_WIDTH (frame, 1); \
  comp_height = GST_VIDEO_FORMAT_INFO_SCALE_HEIGHT(info, 1, y_end - y_start); \
  rowstride = GST_VIDEO_FRAME_COMP_STRIDE (frame, 1); \
  comp_yoffset = (y_start == 0) ? 0 : y_start >> info->h_sub[1]; \
  p += comp_yoffset * rowstride; \
  \
  for (i = 0; i < comp_height; i++) { \
    MEMSET (p, 0x80, comp_width); \
    p += rowstride; \
  } \
  \
  p = GST_VIDEO_FRAME_COMP_DATA (frame, 2); \
  comp_width = GST_VIDEO_FRAME_COMP_WIDTH (frame, 2); \
  comp_height = GST_VIDEO_FORMAT_INFO_SCALE_HEIGHT(info, 2, y_end - y_start); \
  rowstride = GST_VIDEO_FRAME_COMP_STRIDE (frame, 2); \
  comp_yoffset = (y_start == 0) ? 0 : y_start >> info->h_sub[2]; \
  p += comp_yoffset * rowstride; \
  \
  for (i = 0; i < comp_height; i++) { \
    MEMSET (p, 0x80, comp_width); \
    p += rowstride; \
  } \
}

#define PLANAR_YUV_FILL_COLOR(format_name,format_enum,MEMSET) \
static void \
fill_color_##format_name (GstVideoFrame * frame, \
    guint y_start, guint y_end, gint colY, gint colU, gint colV) \
{ \
  guint8 *p; \
  gint comp_width, comp_height; \
  gint rowstride, comp_yoffset; \
  gint i; \
  const GstVideoFormatInfo *info; \
  \
  info = frame->info.finfo; \
  p = GST_VIDEO_FRAME_COMP_DATA (frame, 0); \
  comp_width = GST_VIDEO_FRAME_COMP_WIDTH (frame, 0); \
  comp_height = GST_VIDEO_FORMAT_INFO_SCALE_HEIGHT(info, 0, y_end - y_start); \
  rowstride = GST_VIDEO_FRAME_COMP_STRIDE (frame, 0); \
  comp_yoffset = (y_start == 0) ? 0 : GST_VIDEO_FORMAT_INFO_SCALE_HEIGHT (info, 0, y_start); \
  p += comp_yoffset * rowstride; \
  \
  for (i = 0; i < comp_height; i++) { \
    MEMSET (p, colY, comp_width); \
    p += rowstride; \
  } \
  \
  p = GST_VIDEO_FRAME_COMP_DATA (frame, 1); \
  comp_width = GST_VIDEO_FRAME_COMP_WIDTH (frame, 1); \
  comp_height = GST_VIDEO_FORMAT_INFO_SCALE_HEIGHT(info, 1, y_end - y_start); \
  rowstride = GST_VIDEO_FRAME_COMP_STRIDE (frame, 1); \
  comp_yoffset = (y_start == 0) ? 0 : y_start >> info->h_sub[1]; \
  p += comp_yoffset * rowstride; \
  \
  for (i = 0; i < comp_height; i++) { \
    MEMSET (p, colU, comp_width); \
    p += rowstride; \
  } \
  \
  p = GST_VIDEO_FRAME_COMP_DATA (frame, 2); \
  comp_width = GST_VIDEO_FRAME_COMP_WIDTH (frame, 2); \
  comp_height = GST_VIDEO_FORMAT_INFO_SCALE_HEIGHT(info, 2, y_end - y_start); \
  rowstride = GST_VIDEO_FRAME_COMP_STRIDE (frame, 2); \
  comp_yoffset = (y_start == 0) ? 0 : y_start >> info->h_sub[2]; \
  p += comp_yoffset * rowstride; \
  \
  for (i = 0; i < comp_height; i++) { \
    MEMSET (p, colV, comp_width); \
    p += rowstride; \
  } \
}

#define PLANAR_YUV_HIGH_FILL_CHECKER(format_name, nbits, endian, MEMSET) \
static void \
fill_checker_##format_name (GstVideoFrame * frame, guint y_start, guint y_end) \
{ \
  gint i, j; \
  static const int tab[] = { 80 << (nbits - 8), 160 << (nbits - 8), 80 << (nbits - 8), 160 << (nbits - 8),}; \
  guint8 *p; \
  gint comp_width, comp_height; \
  gint rowstride, comp_yoffset; \
  gint pstride; \
  gint uv; \
  const GstVideoFormatInfo *info; \
  \
  info = frame->info.finfo; \
  p = GST_VIDEO_FRAME_COMP_DATA (frame, 0); \
  comp_width = GST_VIDEO_FRAME_COMP_WIDTH (frame, 0); \
  comp_height = GST_VIDEO_FORMAT_INFO_SCALE_HEIGHT(info, 0, y_end - y_start); \
  rowstride = GST_VIDEO_FRAME_COMP_STRIDE (frame, 0); \
  pstride = GST_VIDEO_FRAME_COMP_PSTRIDE (frame, 0); \
  comp_yoffset = (y_start == 0) ? 0 : GST_VIDEO_FORMAT_INFO_SCALE_HEIGHT (info, 0, y_start); \
  p += comp_yoffset * rowstride; \
  \
  for (i = 0; i < comp_height; i++) { \
    for (j = 0; j < comp_width; j++) { \
      GST_WRITE_UINT16_##endian (p, tab[(((i + y_start) & 0x8) >> 3) + ((j & 0x8) >> 3)]); \
      p += pstride; \
    } \
    p += rowstride - comp_width * pstride; \
  } \
  \
  uv = GUINT16_TO_##endian (1 << (nbits - 1)); \
  p = GST_VIDEO_FRAME_COMP_DATA (frame, 1); \
  comp_width = GST_VIDEO_FRAME_COMP_WIDTH (frame, 1); \
  comp_height = GST_VIDEO_FORMAT_INFO_SCALE_HEIGHT(info, 1, y_end - y_start); \
  rowstride = GST_VIDEO_FRAME_COMP_STRIDE (frame, 1); \
  pstride = GST_VIDEO_FRAME_COMP_PSTRIDE (frame, 1); \
  comp_yoffset = (y_start == 0) ? 0 : y_start >> info->h_sub[1]; \
  p += comp_yoffset * rowstride; \
  MEMSET (p, rowstride, uv, comp_width, comp_height); \
  \
  p = GST_VIDEO_FRAME_COMP_DATA (frame, 2); \
  comp_width = GST_VIDEO_FRAME_COMP_WIDTH (frame, 2); \
  comp_height = GST_VIDEO_FORMAT_INFO_SCALE_HEIGHT(info, 2, y_end - y_start); \
  rowstride = GST_VIDEO_FRAME_COMP_STRIDE (frame, 2); \
  pstride = GST_VIDEO_FRAME_COMP_PSTRIDE (frame, 1); \
  comp_yoffset = (y_start == 0) ? 0 : y_start >> info->h_sub[2]; \
  p += comp_yoffset * rowstride; \
  MEMSET (p, rowstride, uv, comp_width, comp_height); \
}

#define PLANAR_YUV_HIGH_FILL_COLOR(format_name,endian,MEMSET) \
static void \
fill_color_##format_name (GstVideoFrame * frame, \
    guint y_start, guint y_end, gint colY, gint colU, gint colV) \
{ \
  guint8 *p; \
  gint comp_width, comp_height; \
  gint rowstride, comp_yoffset; \
  const GstVideoFormatInfo *info; \
  \
  info = frame->info.finfo; \
  p = GST_VIDEO_FRAME_COMP_DATA (frame, 0); \
  comp_width = GST_VIDEO_FRAME_COMP_WIDTH (frame, 0); \
  comp_height = GST_VIDEO_FORMAT_INFO_SCALE_HEIGHT(info, 0, y_end - y_start); \
  rowstride = GST_VIDEO_FRAME_COMP_STRIDE (frame, 0); \
  comp_yoffset = (y_start == 0) ? 0 : GST_VIDEO_FORMAT_INFO_SCALE_HEIGHT (info, 0, y_start); \
  p += comp_yoffset * rowstride; \
  MEMSET (p, rowstride, GUINT16_TO_##endian (colY), comp_width, comp_height); \
  \
  p = GST_VIDEO_FRAME_COMP_DATA (frame, 1); \
  comp_width = GST_VIDEO_FRAME_COMP_WIDTH (frame, 1); \
  comp_height = GST_VIDEO_FORMAT_INFO_SCALE_HEIGHT(info, 1, y_end - y_start); \
  rowstride = GST_VIDEO_FRAME_COMP_STRIDE (frame, 1); \
  comp_yoffset = (y_start == 0) ? 0 : y_start >> info->h_sub[1]; \
  p += comp_yoffset * rowstride; \
  MEMSET (p, rowstride, GUINT16_TO_##endian (colU), comp_width, comp_height); \
  \
  p = GST_VIDEO_FRAME_COMP_DATA (frame, 2); \
  comp_width = GST_VIDEO_FRAME_COMP_WIDTH (frame, 2); \
  comp_height = GST_VIDEO_FORMAT_INFO_SCALE_HEIGHT(info, 2, y_end - y_start); \
  rowstride = GST_VIDEO_FRAME_COMP_STRIDE (frame, 2); \
  comp_yoffset = (y_start == 0) ? 0 : y_start >> info->h_sub[2]; \
  p += comp_yoffset * rowstride; \
  MEMSET (p, rowstride, GUINT16_TO_##endian (colV), comp_width, comp_height); \
}

#define GST_ROUND_UP_1(x) (x)

PLANAR_YUV_BLEND (i420, GST_ROUND_UP_2,
    GST_ROUND_UP_2, memcpy, compositor_orc_blend_u8, 8);
PLANAR_YUV_FILL_CHECKER (i420, GST_VIDEO_FORMAT_I420, memset);
PLANAR_YUV_FILL_COLOR (i420, GST_VIDEO_FORMAT_I420, memset);
PLANAR_YUV_FILL_COLOR (yv12, GST_VIDEO_FORMAT_YV12, memset);
PLANAR_YUV_BLEND (y444, GST_ROUND_UP_1,
    GST_ROUND_UP_1, memcpy, compositor_orc_blend_u8, 8);
PLANAR_YUV_FILL_CHECKER (y444, GST_VIDEO_FORMAT_Y444, memset);
PLANAR_YUV_FILL_COLOR (y444, GST_VIDEO_FORMAT_Y444, memset);
PLANAR_YUV_BLEND (y42b, GST_ROUND_UP_2,
    GST_ROUND_UP_1, memcpy, compositor_orc_blend_u8, 8);
PLANAR_YUV_FILL_CHECKER (y42b, GST_VIDEO_FORMAT_Y42B, memset);
PLANAR_YUV_FILL_COLOR (y42b, GST_VIDEO_FORMAT_Y42B, memset);
PLANAR_YUV_BLEND (y41b, GST_ROUND_UP_4,
    GST_ROUND_UP_1, memcpy, compositor_orc_blend_u8, 8);
PLANAR_YUV_FILL_CHECKER (y41b, GST_VIDEO_FORMAT_Y41B, memset);
PLANAR_YUV_FILL_COLOR (y41b, GST_VIDEO_FORMAT_Y41B, memset);

#define BLEND_HIGH(format_name) \
compositor_orc_blend_##format_name

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
PLANAR_YUV_BLEND (i420_10le, GST_ROUND_UP_2, GST_ROUND_UP_2, memcpy,
    BLEND_HIGH (u10), 10);
PLANAR_YUV_BLEND (i420_10be, GST_ROUND_UP_2, GST_ROUND_UP_2, memcpy,
    BLEND_HIGH (u10_swap), 10);

PLANAR_YUV_BLEND (i420_12le, GST_ROUND_UP_2, GST_ROUND_UP_2, memcpy,
    BLEND_HIGH (u12), 12);
PLANAR_YUV_BLEND (i420_12be, GST_ROUND_UP_2, GST_ROUND_UP_2, memcpy,
    BLEND_HIGH (u12_swap), 12);

PLANAR_YUV_BLEND (i422_10le, GST_ROUND_UP_2, GST_ROUND_UP_1, memcpy,
    BLEND_HIGH (u10), 10);
PLANAR_YUV_BLEND (i422_10be, GST_ROUND_UP_2, GST_ROUND_UP_1, memcpy,
    BLEND_HIGH (u10_swap), 10);

PLANAR_YUV_BLEND (i422_12le, GST_ROUND_UP_2, GST_ROUND_UP_1, memcpy,
    BLEND_HIGH (u12), 12);
PLANAR_YUV_BLEND (i422_12be, GST_ROUND_UP_2, GST_ROUND_UP_1, memcpy,
    BLEND_HIGH (u12_swap), 12);

PLANAR_YUV_BLEND (y444_10le, GST_ROUND_UP_1, GST_ROUND_UP_1, memcpy,
    BLEND_HIGH (u10), 10);
PLANAR_YUV_BLEND (y444_10be, GST_ROUND_UP_1, GST_ROUND_UP_1, memcpy,
    BLEND_HIGH (u10_swap), 10);

PLANAR_YUV_BLEND (y444_12le, GST_ROUND_UP_1, GST_ROUND_UP_1, memcpy,
    BLEND_HIGH (u12), 12);
PLANAR_YUV_BLEND (y444_12be, GST_ROUND_UP_1, GST_ROUND_UP_1, memcpy,
    BLEND_HIGH (u12_swap), 12);

PLANAR_YUV_BLEND (y444_16le, GST_ROUND_UP_1, GST_ROUND_UP_1, memcpy,
    BLEND_HIGH (u16), 16);
PLANAR_YUV_BLEND (y444_16be, GST_ROUND_UP_1, GST_ROUND_UP_1, memcpy,
    BLEND_HIGH (u16_swap), 16);
#else /* G_BYTE_ORDER == G_LITTLE_ENDIAN */
PLANAR_YUV_BLEND (i420_10le, GST_ROUND_UP_2, GST_ROUND_UP_2, memcpy,
    BLEND_HIGH (u10_swap), 10);
PLANAR_YUV_BLEND (i420_10be, GST_ROUND_UP_2, GST_ROUND_UP_2, memcpy,
    BLEND_HIGH (u10), 10);

PLANAR_YUV_BLEND (i420_12le, GST_ROUND_UP_2, GST_ROUND_UP_2, memcpy,
    BLEND_HIGH (u12_swap), 12);
PLANAR_YUV_BLEND (i420_12be, GST_ROUND_UP_2, GST_ROUND_UP_2, memcpy,
    BLEND_HIGH (u12), 12);

PLANAR_YUV_BLEND (i422_10le, GST_ROUND_UP_2, GST_ROUND_UP_1, memcpy,
    BLEND_HIGH (u10_swap), 10);
PLANAR_YUV_BLEND (i422_10be, GST_ROUND_UP_2, GST_ROUND_UP_1, memcpy,
    BLEND_HIGH (u10), 10);

PLANAR_YUV_BLEND (i422_12le, GST_ROUND_UP_2, GST_ROUND_UP_1, memcpy,
    BLEND_HIGH (u12_swap), 12);
PLANAR_YUV_BLEND (i422_12be, GST_ROUND_UP_2, GST_ROUND_UP_1, memcpy,
    BLEND_HIGH (u12), 12);

PLANAR_YUV_BLEND (y444_10le, GST_ROUND_UP_1, GST_ROUND_UP_1, memcpy,
    BLEND_HIGH (u10_swap), 10);
PLANAR_YUV_BLEND (y444_10be, GST_ROUND_UP_1, GST_ROUND_UP_1, memcpy,
    BLEND_HIGH (u10), 10);

PLANAR_YUV_BLEND (y444_12le, GST_ROUND_UP_1, GST_ROUND_UP_1, memcpy,
    BLEND_HIGH (u12_swap), 12);
PLANAR_YUV_BLEND (y444_12be, GST_ROUND_UP_1, GST_ROUND_UP_1, memcpy,
    BLEND_HIGH (u12), 12);

PLANAR_YUV_BLEND (y444_16le, GST_ROUND_UP_1, GST_ROUND_UP_1, memcpy,
    BLEND_HIGH (u16_swap), 16);
PLANAR_YUV_BLEND (y444_16be, GST_ROUND_UP_1, GST_ROUND_UP_1, memcpy,
    BLEND_HIGH (u16), 16);
#endif /* G_BYTE_ORDER == G_LITTLE_ENDIAN */

PLANAR_YUV_HIGH_FILL_CHECKER (i420_10le, 10, LE, compositor_orc_memset_u16_2d);
PLANAR_YUV_HIGH_FILL_COLOR (i420_10le, LE, compositor_orc_memset_u16_2d);
PLANAR_YUV_HIGH_FILL_CHECKER (i420_10be, 10, BE, compositor_orc_memset_u16_2d);
PLANAR_YUV_HIGH_FILL_COLOR (i420_10be, BE, compositor_orc_memset_u16_2d);

PLANAR_YUV_HIGH_FILL_CHECKER (i420_12le, 12, LE, compositor_orc_memset_u16_2d);
PLANAR_YUV_HIGH_FILL_COLOR (i420_12le, LE, compositor_orc_memset_u16_2d);
PLANAR_YUV_HIGH_FILL_CHECKER (i420_12be, 12, BE, compositor_orc_memset_u16_2d);
PLANAR_YUV_HIGH_FILL_COLOR (i420_12be, BE, compositor_orc_memset_u16_2d);

PLANAR_YUV_HIGH_FILL_CHECKER (y444_16le, 16, LE, compositor_orc_memset_u16_2d);
PLANAR_YUV_HIGH_FILL_COLOR (y444_16le, LE, compositor_orc_memset_u16_2d);
PLANAR_YUV_HIGH_FILL_CHECKER (y444_16be, 16, BE, compositor_orc_memset_u16_2d);
PLANAR_YUV_HIGH_FILL_COLOR (y444_16be, BE, compositor_orc_memset_u16_2d);

/* TODO: port to ORC */
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
static void
compositor_blend_argb64 (guint8 * ORC_RESTRICT d1, int d1_stride,
    const guint8 * ORC_RESTRICT s1, int s1_stride, int p1, int n, int m)
{
  gint i, j, k;
  const guint64 comp_mask_0 = 0xffff000000000000;
  const guint64 comp_mask_1 = 0x0000ffff00000000;
  const guint64 comp_mask_2 = 0x00000000ffff0000;
  const guint64 comp_mask_alpha = 0x000000000000ffff;

  for (j = 0; j < m; j++) {
    guint64 *dst;
    guint64 *src;

    dst = (guint64 *) (d1 + (d1_stride * j));
    src = (guint64 *) (s1 + (s1_stride * j));

    for (i = 0; i < n; i++) {
      guint64 src_val = src[i];
      guint64 dst_val = dst[i];
      guint64 src_comp[3];
      guint64 dst_comp[3];
      guint64 src_alpha;
      guint64 src_alpha_inv;

      src_comp[0] = (src_val & comp_mask_0) >> 48;
      src_comp[1] = (src_val & comp_mask_1) >> 32;
      src_comp[2] = (src_val & comp_mask_2) >> 16;

      dst_comp[0] = (dst_val & comp_mask_0) >> 48;
      dst_comp[1] = (dst_val & comp_mask_1) >> 32;
      dst_comp[2] = (dst_val & comp_mask_2) >> 16;

      src_alpha = src_val & comp_mask_alpha;
      src_alpha *= p1;
      src_alpha /= G_MAXUINT16;
      src_alpha = CLAMP (src_alpha, 0, G_MAXUINT16);
      src_alpha_inv = G_MAXUINT16 - src_alpha;

      for (k = 0; k < G_N_ELEMENTS (src_comp); k++) {
        src_comp[k] *= src_alpha;
        dst_comp[k] *= src_alpha_inv;
        dst_comp[k] += src_comp[k];
        dst_comp[k] /= G_MAXUINT16;

        dst_comp[k] = CLAMP (dst_comp[k], 0, G_MAXUINT16);
      }

      dst_val = (dst_comp[0] << 48) | (dst_comp[1] << 32) | (dst_comp[2] << 16)
          | comp_mask_alpha;
      dst[i] = dst_val;
    }
  }
}

static void
compositor_source_argb64 (guint8 * ORC_RESTRICT d1, int d1_stride,
    const guint8 * ORC_RESTRICT s1, int s1_stride, int p1, int n, int m)
{
  gint i, j;
  const guint64 comp_mask_non_alpha = 0xffffffffffff0000;
  const guint64 comp_mask_alpha = 0x000000000000ffff;

  for (j = 0; j < m; j++) {
    guint64 *dst;
    guint64 *src;

    dst = (guint64 *) (d1 + (d1_stride * j));
    src = (guint64 *) (s1 + (s1_stride * j));

    for (i = 0; i < n; i++) {
      guint64 src_val = src[i];
      guint64 dst_val;
      guint64 src_alpha;

      src_alpha = src_val & comp_mask_alpha;
      src_alpha *= p1;
      src_alpha /= G_MAXUINT16;
      src_alpha = CLAMP (src_alpha, 0, G_MAXUINT16);

      dst_val = (src_val & comp_mask_non_alpha) | src_alpha;
      dst[i] = dst_val;
    }
  }
}

static void
compositor_overlay_argb64 (guint8 * ORC_RESTRICT d1, int d1_stride,
    const guint8 * ORC_RESTRICT s1, int s1_stride, int p1, int n, int m)
{
  gint i, j, k;
  const guint64 comp_mask_0 = 0xffff000000000000;
  const guint64 comp_mask_1 = 0x0000ffff00000000;
  const guint64 comp_mask_2 = 0x00000000ffff0000;
  const guint64 comp_mask_alpha = 0x000000000000ffff;

  for (j = 0; j < m; j++) {
    guint64 *dst;
    guint64 *src;

    dst = (guint64 *) (d1 + (d1_stride * j));
    src = (guint64 *) (s1 + (s1_stride * j));

    for (i = 0; i < n; i++) {
      guint64 src_val = src[i];
      guint64 dst_val = dst[i];
      guint64 src_comp[3];
      guint64 dst_comp[3];
      guint64 src_alpha;
      guint64 src_alpha_inv;
      guint64 dst_alpha;

      src_comp[0] = (src_val & comp_mask_0) >> 48;
      src_comp[1] = (src_val & comp_mask_1) >> 32;
      src_comp[2] = (src_val & comp_mask_2) >> 16;

      dst_comp[0] = (dst_val & comp_mask_0) >> 48;
      dst_comp[1] = (dst_val & comp_mask_1) >> 32;
      dst_comp[2] = (dst_val & comp_mask_2) >> 16;

      /* calc source alpha as alpha_s = alpha_s * alpha / 255 */
      src_alpha = src_val & comp_mask_alpha;
      src_alpha *= p1;
      src_alpha /= G_MAXUINT16;
      src_alpha = CLAMP (src_alpha, 0, G_MAXUINT16);
      src_alpha_inv = G_MAXUINT16 - src_alpha;

      for (k = 0; k < G_N_ELEMENTS (src_comp); k++)
        src_comp[k] *= src_alpha;

      /* calc destination alpha as alpha_d = (1.0 - alpha_s) * alpha_d / 1.0 */
      dst_alpha = dst_val & comp_mask_alpha;
      dst_alpha *= src_alpha_inv;
      dst_alpha /= G_MAXUINT16;
      for (k = 0; k < G_N_ELEMENTS (dst_comp); k++)
        dst_comp[k] *= dst_alpha;

      /* calc final pixel as pix_d = pix_s*alpha_s + pix_d*alpha_d*(255-alpha_s)/255 */
      for (k = 0; k < G_N_ELEMENTS (dst_comp); k++)
        dst_comp[k] += src_comp[k];

      /* calc the final destination alpha_d = alpha_s + alpha_d * (255-alpha_s)/255 */
      dst_alpha += src_alpha;
      dst_alpha = CLAMP (dst_alpha, 0, G_MAXUINT16);

      /* now normalize the pix_d by the final alpha to make it associative */
      for (k = 0; k < G_N_ELEMENTS (dst_comp); k++) {
        if (dst_alpha > 0)
          dst_comp[k] /= dst_alpha;
        dst_comp[k] = CLAMP (dst_comp[k], 0, G_MAXUINT16);
      }

      dst_val = (dst_comp[0] << 48) | (dst_comp[1] << 32) | (dst_comp[2] << 16)
          | dst_alpha;
      dst[i] = dst_val;
    }
  }
}

static void
compositor_overlay_argb64_addition (guint8 * ORC_RESTRICT d1, int d1_stride,
    const guint8 * ORC_RESTRICT s1, int s1_stride, int p1, int n, int m)
{
  gint i, j, k;
  const guint64 comp_mask_0 = 0xffff000000000000;
  const guint64 comp_mask_1 = 0x0000ffff00000000;
  const guint64 comp_mask_2 = 0x00000000ffff0000;
  const guint64 comp_mask_alpha = 0x000000000000ffff;

  for (j = 0; j < m; j++) {
    guint64 *dst;
    guint64 *src;

    dst = (guint64 *) (d1 + (d1_stride * j));
    src = (guint64 *) (s1 + (s1_stride * j));

    for (i = 0; i < n; i++) {
      guint64 src_val = src[i];
      guint64 dst_val = dst[i];
      guint64 src_comp[3];
      guint64 dst_comp[3];
      guint64 src_alpha;
      guint64 src_alpha_inv;
      guint64 alpha_factor;
      guint64 dst_alpha;

      src_comp[0] = (src_val & comp_mask_0) >> 48;
      src_comp[1] = (src_val & comp_mask_1) >> 32;
      src_comp[2] = (src_val & comp_mask_2) >> 16;

      dst_comp[0] = (dst_val & comp_mask_0) >> 48;
      dst_comp[1] = (dst_val & comp_mask_1) >> 32;
      dst_comp[2] = (dst_val & comp_mask_2) >> 16;

      /* calc source alpha as alpha_s = alpha_s * alpha / 255 */
      src_alpha = src_val & comp_mask_alpha;
      src_alpha *= p1;
      src_alpha /= G_MAXUINT16;
      src_alpha = CLAMP (src_alpha, 0, G_MAXUINT16);
      src_alpha_inv = G_MAXUINT16 - src_alpha;

      for (k = 0; k < G_N_ELEMENTS (src_comp); k++)
        src_comp[k] *= src_alpha;

      /* calc destination alpha as alpha_factor = (255-alpha_s) * alpha_factor / factor */
      alpha_factor = dst_val & comp_mask_alpha;
      alpha_factor *= src_alpha_inv;
      alpha_factor /= G_MAXUINT16;
      for (k = 0; k < G_N_ELEMENTS (dst_comp); k++)
        dst_comp[k] *= alpha_factor;

      /* calc final pixel as pix_d = pix_s*alpha_s + pix_d*alpha_factor*(255-alpha_s)/255 */
      for (k = 0; k < G_N_ELEMENTS (dst_comp); k++)
        dst_comp[k] += src_comp[k];

      /* calc the alpha factor alpha_factor = alpha_s + alpha_factor * (255-alpha_s)/255 */
      alpha_factor += src_alpha;
      alpha_factor = CLAMP (alpha_factor, 0, G_MAXUINT16);

      /* now normalize the pix_d by the final alpha to make it associative */
      for (k = 0; k < G_N_ELEMENTS (dst_comp); k++) {
        if (alpha_factor > 0)
          dst_comp[k] /= alpha_factor;
        dst_comp[k] = CLAMP (dst_comp[k], 0, G_MAXUINT16);
      }

      /* calc the final global alpha_d = alpha_d + (alpha_s * (alpha / 255)) */
      dst_alpha = dst_val & comp_mask_alpha;
      dst_alpha += src_alpha;
      dst_alpha = CLAMP (dst_alpha, 0, G_MAXUINT16);

      dst_val = (dst_comp[0] << 48) | (dst_comp[1] << 32) | (dst_comp[2] << 16)
          | dst_alpha;
      dst[i] = dst_val;
    }
  }
}
#else /* if G_BYTE_ORDER == G_LITTLE_ENDIAN */
static void
compositor_blend_bgra64 (guint8 * ORC_RESTRICT d1, int d1_stride,
    const guint8 * ORC_RESTRICT s1, int s1_stride, int p1, int n, int m)
{
  gint i, j, k;
  const guint64 comp_mask_0 = 0x000000000000ffff;
  const guint64 comp_mask_1 = 0x00000000ffff0000;
  const guint64 comp_mask_2 = 0x0000ffff00000000;
  const guint64 comp_mask_alpha = 0xffff000000000000;

  for (j = 0; j < m; j++) {
    guint64 *dst;
    guint64 *src;

    dst = (guint64 *) (d1 + (d1_stride * j));
    src = (guint64 *) (s1 + (s1_stride * j));

    for (i = 0; i < n; i++) {
      guint64 src_val = src[i];
      guint64 dst_val = dst[i];
      guint64 src_comp[3];
      guint64 dst_comp[3];
      guint64 src_alpha;
      guint64 src_alpha_inv;

      src_comp[0] = src_val & comp_mask_0;
      src_comp[1] = (src_val & comp_mask_1) >> 16;
      src_comp[2] = (src_val & comp_mask_2) >> 32;

      dst_comp[0] = dst_val & comp_mask_0;
      dst_comp[1] = (dst_val & comp_mask_1) >> 16;
      dst_comp[2] = (dst_val & comp_mask_2) >> 32;

      src_alpha = (src_val & comp_mask_alpha) >> 48;
      src_alpha *= p1;
      src_alpha /= G_MAXUINT16;
      src_alpha = CLAMP (src_alpha, 0, G_MAXUINT16);
      src_alpha_inv = G_MAXUINT16 - src_alpha;

      for (k = 0; k < G_N_ELEMENTS (src_comp); k++) {
        src_comp[k] *= src_alpha;
        dst_comp[k] *= src_alpha_inv;
        dst_comp[k] += src_comp[k];
        dst_comp[k] /= G_MAXUINT16;

        dst_comp[k] = CLAMP (dst_comp[k], 0, G_MAXUINT16);
      }

      dst_val = (dst_comp[0]) | (dst_comp[1] << 16) | (dst_comp[2] << 32)
          | comp_mask_alpha;
      dst[i] = dst_val;
    }
  }
}

static void
compositor_source_bgra64 (guint8 * ORC_RESTRICT d1, int d1_stride,
    const guint8 * ORC_RESTRICT s1, int s1_stride, int p1, int n, int m)
{
  gint i, j;
  const guint64 comp_mask_non_alpha = 0x0000ffffffffffff;
  const guint64 comp_mask_alpha = 0xffff000000000000;

  for (j = 0; j < m; j++) {
    guint64 *dst;
    guint64 *src;

    dst = (guint64 *) (d1 + (d1_stride * j));
    src = (guint64 *) (s1 + (s1_stride * j));

    for (i = 0; i < n; i++) {
      guint64 src_val = src[i];
      guint64 dst_val;
      guint64 src_alpha;

      src_alpha = (src_val & comp_mask_alpha) >> 48;
      src_alpha *= p1;
      src_alpha /= G_MAXUINT16;
      src_alpha = CLAMP (src_alpha, 0, G_MAXUINT16);
      src_alpha <<= 48;

      dst_val = (src_val & comp_mask_non_alpha) | src_alpha;
      dst[i] = dst_val;
    }
  }
}

static void
compositor_overlay_bgra64 (guint8 * ORC_RESTRICT d1, int d1_stride,
    const guint8 * ORC_RESTRICT s1, int s1_stride, int p1, int n, int m)
{
  gint i, j, k;
  const guint64 comp_mask_0 = 0x000000000000ffff;
  const guint64 comp_mask_1 = 0x00000000ffff0000;
  const guint64 comp_mask_2 = 0x0000ffff00000000;
  const guint64 comp_mask_alpha = 0xffff000000000000;

  for (j = 0; j < m; j++) {
    guint64 *dst;
    guint64 *src;

    dst = (guint64 *) (d1 + (d1_stride * j));
    src = (guint64 *) (s1 + (s1_stride * j));

    for (i = 0; i < n; i++) {
      guint64 src_val = src[i];
      guint64 dst_val = dst[i];
      guint64 src_comp[3];
      guint64 dst_comp[3];
      guint64 src_alpha;
      guint64 src_alpha_inv;
      guint64 dst_alpha;

      src_comp[0] = src_val & comp_mask_0;
      src_comp[1] = (src_val & comp_mask_1) >> 16;
      src_comp[2] = (src_val & comp_mask_2) >> 32;

      dst_comp[0] = dst_val & comp_mask_0;
      dst_comp[1] = (dst_val & comp_mask_1) >> 16;
      dst_comp[2] = (dst_val & comp_mask_2) >> 32;

      /* calc source alpha as alpha_s = alpha_s * alpha / 255 */
      src_alpha = (src_val & comp_mask_alpha) >> 48;
      src_alpha *= p1;
      src_alpha /= G_MAXUINT16;
      src_alpha = CLAMP (src_alpha, 0, G_MAXUINT16);
      src_alpha_inv = G_MAXUINT16 - src_alpha;

      for (k = 0; k < G_N_ELEMENTS (src_comp); k++)
        src_comp[k] *= src_alpha;
      /* calc destination alpha as alpha_d = (1.0 - alpha_s) * alpha_d / 1.0 */
      dst_alpha = (dst_val & comp_mask_alpha) >> 48;
      dst_alpha *= src_alpha_inv;
      dst_alpha /= G_MAXUINT16;
      for (k = 0; k < G_N_ELEMENTS (dst_comp); k++)
        dst_comp[k] *= dst_alpha;

      /* calc final pixel as pix_d = pix_s*alpha_s + pix_d*alpha_d*(255-alpha_s)/255 */
      for (k = 0; k < G_N_ELEMENTS (dst_comp); k++)
        dst_comp[k] += src_comp[k];

      /* calc the final destination alpha_d = alpha_s + alpha_d * (255-alpha_s)/255 */
      dst_alpha += src_alpha;
      dst_alpha = CLAMP (dst_alpha, 0, G_MAXUINT16);

      /* now normalize the pix_d by the final alpha to make it associative */
      for (k = 0; k < G_N_ELEMENTS (dst_comp); k++) {
        if (dst_alpha > 0)
          dst_comp[k] /= dst_alpha;
        dst_comp[k] = CLAMP (dst_comp[k], 0, G_MAXUINT16);
      }

      dst_val = (dst_comp[0]) | (dst_comp[1] << 16) | (dst_comp[2] << 32)
          | (dst_alpha << 48);
      dst[i] = dst_val;
    }
  }
}

static void
compositor_overlay_bgra64_addition (guint8 * ORC_RESTRICT d1, int d1_stride,
    const guint8 * ORC_RESTRICT s1, int s1_stride, int p1, int n, int m)
{
  gint i, j, k;
  const guint64 comp_mask_0 = 0x000000000000ffff;
  const guint64 comp_mask_1 = 0x00000000ffff0000;
  const guint64 comp_mask_2 = 0x0000ffff00000000;
  const guint64 comp_mask_alpha = 0xffff000000000000;

  for (j = 0; j < m; j++) {
    guint64 *dst;
    guint64 *src;

    dst = (guint64 *) (d1 + (d1_stride * j));
    src = (guint64 *) (s1 + (s1_stride * j));

    for (i = 0; i < n; i++) {
      guint64 src_val = src[i];
      guint64 dst_val = dst[i];
      guint64 src_comp[3];
      guint64 dst_comp[3];
      guint64 src_alpha;
      guint64 src_alpha_inv;
      guint64 alpha_factor;
      guint64 dst_alpha;

      src_comp[0] = src_val & comp_mask_0;
      src_comp[1] = (src_val & comp_mask_1) >> 16;
      src_comp[2] = (src_val & comp_mask_2) >> 32;

      dst_comp[0] = dst_val & comp_mask_0;
      dst_comp[1] = (dst_val & comp_mask_1) >> 16;
      dst_comp[2] = (dst_val & comp_mask_2) >> 32;

      /* calc source alpha as alpha_s = alpha_s * alpha / 255 */
      src_alpha = (src_val & comp_mask_alpha) >> 48;
      src_alpha *= p1;
      src_alpha /= G_MAXUINT16;
      src_alpha = CLAMP (src_alpha, 0, G_MAXUINT16);
      src_alpha_inv = G_MAXUINT16 - src_alpha;

      for (k = 0; k < G_N_ELEMENTS (src_comp); k++)
        src_comp[k] *= src_alpha;

      /* calc destination alpha as alpha_factor = (255-alpha_s) * alpha_factor / factor */
      alpha_factor = (dst_val & comp_mask_alpha) >> 48;
      alpha_factor *= src_alpha_inv;
      alpha_factor /= G_MAXUINT16;
      for (k = 0; k < G_N_ELEMENTS (dst_comp); k++)
        dst_comp[k] *= alpha_factor;

      /* calc final pixel as pix_d = pix_s*alpha_s + pix_d*alpha_factor*(255-alpha_s)/255 */
      for (k = 0; k < G_N_ELEMENTS (dst_comp); k++)
        dst_comp[k] += src_comp[k];

      /* calc the alpha factor alpha_factor = alpha_s + alpha_factor * (255-alpha_s)/255 */
      alpha_factor += src_alpha;
      alpha_factor = CLAMP (alpha_factor, 0, G_MAXUINT16);

      /* now normalize the pix_d by the final alpha to make it associative */
      for (k = 0; k < G_N_ELEMENTS (dst_comp); k++) {
        if (alpha_factor > 0)
          dst_comp[k] /= alpha_factor;
        dst_comp[k] = CLAMP (dst_comp[k], 0, G_MAXUINT16);
      }

      /* calc the final global alpha_d = alpha_d + (alpha_s * (alpha / 255)) */
      dst_alpha = (dst_val & comp_mask_alpha) >> 48;
      dst_alpha += src_alpha;
      dst_alpha = CLAMP (dst_alpha, 0, G_MAXUINT16);

      dst_val = (dst_comp[0]) | (dst_comp[1] << 16) | (dst_comp[2] << 32)
          | (dst_alpha << 48);
      dst[i] = dst_val;
    }
  }
}
#endif /* if G_BYTE_ORDER == G_LITTLE_ENDIAN */

/* for AYUV64, ARGB64 */
#define BLEND_A64(name, method, LOOP) \
static void \
method##_ ##name (GstVideoFrame * srcframe, gint xpos, gint ypos, \
    gdouble src_alpha, GstVideoFrame * destframe, gint dst_y_start, \
    gint dst_y_end, GstCompositorBlendMode mode) \
{ \
  guint s_alpha; \
  gint src_stride, dest_stride; \
  gint dest_width, dest_height; \
  guint8 *src, *dest; \
  gint src_width, src_height; \
  \
  src_width = GST_VIDEO_FRAME_WIDTH (srcframe); \
  src_height = GST_VIDEO_FRAME_HEIGHT (srcframe); \
  src = GST_VIDEO_FRAME_PLANE_DATA (srcframe, 0); \
  src_stride = GST_VIDEO_FRAME_COMP_STRIDE (srcframe, 0); \
  dest = GST_VIDEO_FRAME_PLANE_DATA (destframe, 0); \
  dest_stride = GST_VIDEO_FRAME_COMP_STRIDE (destframe, 0); \
  dest_width = GST_VIDEO_FRAME_COMP_WIDTH (destframe, 0); \
  dest_height = GST_VIDEO_FRAME_COMP_HEIGHT (destframe, 0); \
  \
  s_alpha = CLAMP ((gint) (src_alpha * G_MAXUINT16), 0, G_MAXUINT16); \
  \
  /* If it's completely transparent... we just return */ \
  if (G_UNLIKELY (s_alpha == 0)) \
    return; \
  \
  if (dst_y_end > dest_height) { \
    dst_y_end = dest_height; \
  } \
  /* adjust src pointers for negative sizes */ \
  if (xpos < 0) { \
    src += -xpos * 8; \
    src_width -= -xpos; \
    xpos = 0; \
  } \
  if (ypos < dst_y_start) { \
    src += (dst_y_start - ypos) * src_stride; \
    src_height -= dst_y_start - ypos; \
    ypos = dst_y_start; \
  } \
  /* adjust width/height if the src is bigger than dest */ \
  if (xpos + src_width > dest_width) { \
    src_width = dest_width - xpos; \
  } \
  if (ypos + src_height > dst_y_end) { \
    src_height = dst_y_end - ypos; \
  } \
  \
  if (src_height > 0 && src_width > 0) { \
    dest = dest + 8 * xpos + (ypos * dest_stride); \
  \
    LOOP (dest, src, src_height, src_width, src_stride, dest_stride, s_alpha, \
        mode); \
  } \
}

#define OVERLAY_A64_LOOP(name)  \
static inline void \
_overlay_loop_##name (guint8 * dest, const guint8 * src, gint src_height, \
    gint src_width, gint src_stride, gint dest_stride, guint s_alpha, \
    GstCompositorBlendMode mode) \
{ \
  s_alpha = MIN (G_MAXUINT16, s_alpha); \
  switch (mode) { \
    case COMPOSITOR_BLEND_MODE_SOURCE:\
      if (s_alpha == G_MAXUINT16) { \
        guint y; \
        for (y = 0; y < src_height; y++) { \
          memcpy (dest, src, 8 * src_width); \
          dest += dest_stride; \
          src += src_stride; \
        } \
      } else { \
        compositor_source_##name (dest, dest_stride, src, src_stride, \
          s_alpha, src_width, src_height); \
      } \
      break;\
    case COMPOSITOR_BLEND_MODE_OVER:\
      compositor_overlay_##name (dest, dest_stride, src, src_stride, \
        s_alpha, src_width, src_height); \
      break;\
    case COMPOSITOR_BLEND_MODE_ADD:\
      compositor_overlay_##name##_addition (dest, dest_stride, src, src_stride, \
        s_alpha, src_width, src_height); \
      break;\
  }\
}

#define BLEND_A64_LOOP(name) \
static inline void \
_blend_loop_##name (guint8 * dest, const guint8 * src, gint src_height, \
    gint src_width, gint src_stride, gint dest_stride, guint s_alpha, \
    GstCompositorBlendMode mode) \
{ \
  s_alpha = MIN (G_MAXUINT16, s_alpha); \
  switch (mode) { \
    case COMPOSITOR_BLEND_MODE_SOURCE:\
      if (s_alpha == G_MAXUINT16) { \
        guint y; \
        for (y = 0; y < src_height; y++) { \
          memcpy (dest, src, 8 * src_width); \
          dest += dest_stride; \
          src += src_stride; \
        } \
      } else { \
        compositor_source_##name (dest, dest_stride, src, src_stride, \
          s_alpha, src_width, src_height); \
      } \
      break;\
    case COMPOSITOR_BLEND_MODE_OVER:\
    case COMPOSITOR_BLEND_MODE_ADD:\
      /* both modes are the same for opaque background */ \
      compositor_blend_##name (dest, dest_stride, src, src_stride, \
        s_alpha, src_width, src_height); \
      break;\
  }\
}

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
OVERLAY_A64_LOOP (argb64);
BLEND_A64_LOOP (argb64);
BLEND_A64 (argb64, blend, _blend_loop_argb64);
BLEND_A64 (argb64, overlay, _overlay_loop_argb64);
#else
OVERLAY_A64_LOOP (bgra64);
BLEND_A64_LOOP (bgra64);
BLEND_A64 (argb64, blend, _blend_loop_bgra64);
BLEND_A64 (argb64, overlay, _overlay_loop_bgra64);
#endif

#define A64_CHECKER_C(name, RGB, A, C1, C2, C3) \
static void \
fill_checker_##name##_c (GstVideoFrame * frame, guint y_start, guint y_end) \
{ \
  gint i, j; \
  gint val; \
  static const gint tab[] = { 20480, 40960, 20480, 40960 }; \
  static const gint uv = 1 << 15; \
  gint width, stride; \
  guint8 *dest; \
  \
  dest = GST_VIDEO_FRAME_PLANE_DATA (frame, 0); \
  width = GST_VIDEO_FRAME_COMP_WIDTH (frame, 0); \
  stride = GST_VIDEO_FRAME_COMP_STRIDE (frame, 0); \
  \
  if (!RGB) { \
    for (i = y_start; i < y_end; i++) { \
      guint16 *data = (guint16 *) (dest + i * stride); \
      for (j = 0; j < width; j++) { \
        data[A] = 0xffff; \
        data[C1] = tab[((i & 0x8) >> 3) + ((j & 0x8) >> 3)]; \
        data[C2] = uv; \
        data[C3] = uv; \
        data += 4; \
      } \
    } \
  } else { \
    for (i = y_start; i < y_end; i++) { \
      guint16 *data = (guint16 *) (dest + i * stride); \
      for (j = 0; j < width; j++) { \
        val = tab[((i & 0x8) >> 3) + ((j & 0x8) >> 3)]; \
        data[A] = 0xffff; \
        data[C1] = val; \
        data[C2] = val; \
        data[C3] = val; \
        data += 4; \
      } \
    } \
  } \
}

A64_CHECKER_C (argb64, TRUE, 0, 1, 2, 3);
A64_CHECKER_C (ayuv64, FALSE, 0, 1, 2, 3);

#define A64_COLOR(name, A, C1, C2, C3) \
static void \
fill_color_##name (GstVideoFrame * frame, guint y_start, guint y_end, gint c1, gint c2, gint c3) \
{ \
  gint i, j; \
  gint stride; \
  guint8 *dest; \
  guint width; \
  guint height; \
  \
  height = y_end - y_start; \
  if (height <= 0) \
    return; \
  \
  dest = GST_VIDEO_FRAME_PLANE_DATA (frame, 0); \
  stride = GST_VIDEO_FRAME_COMP_STRIDE (frame, 0); \
  width = GST_VIDEO_FRAME_WIDTH (frame); \
  \
  for (i = y_start; i < y_end; i++) { \
    guint16 *data = (guint16 *) (dest + i * stride); \
    for (j = 0; j < width; j++) { \
      data[A] = 0xffff; \
      data[C1] = c1; \
      data[C2] = c2; \
      data[C3] = c3; \
      data += 4; \
    } \
  } \
}

A64_COLOR (argb64, 0, 1, 2, 3);

/* NV12, NV21 */
#define NV_YUV_BLEND(format_name,MEMCPY,BLENDLOOP) \
inline static void \
_blend_##format_name (const guint8 * src, guint8 * dest, \
    gint src_stride, gint dest_stride, gint src_width, gint src_height, \
    gdouble src_alpha, GstCompositorBlendMode mode) \
{ \
  gint i; \
  gint b_alpha; \
  \
  /* in source mode we just have to copy over things */ \
  if (mode == COMPOSITOR_BLEND_MODE_SOURCE) { \
    src_alpha = 1.0; \
  } \
  \
  /* If it's completely transparent... we just return */ \
  if (G_UNLIKELY (src_alpha == 0.0)) { \
    GST_LOG ("Fast copy (alpha == 0.0)"); \
    return; \
  } \
  \
  /* If it's completely opaque, we do a fast copy */ \
  if (G_UNLIKELY (src_alpha == 1.0)) { \
    GST_LOG ("Fast copy (alpha == 1.0)"); \
    for (i = 0; i < src_height; i++) { \
      MEMCPY (dest, src, src_width); \
      src += src_stride; \
      dest += dest_stride; \
    } \
    return; \
  } \
  \
  b_alpha = CLAMP ((gint) (src_alpha * 255), 0, 255); \
  \
  BLENDLOOP(dest, dest_stride, src, src_stride, b_alpha, src_width, src_height); \
} \
\
static void \
blend_##format_name (GstVideoFrame * srcframe, gint xpos, gint ypos, \
    gdouble src_alpha, GstVideoFrame * destframe, gint dst_y_start, \
    gint dst_y_end, GstCompositorBlendMode mode)                    \
{ \
  const guint8 *b_src; \
  guint8 *b_dest; \
  gint b_src_width; \
  gint b_src_height; \
  gint xoffset = 0; \
  gint yoffset = 0; \
  gint src_comp_rowstride, dest_comp_rowstride; \
  gint src_comp_height; \
  gint src_comp_width; \
  gint comp_ypos, comp_xpos; \
  gint comp_yoffset, comp_xoffset; \
  gint dest_width, dest_height; \
  const GstVideoFormatInfo *info; \
  gint src_width, src_height; \
  \
  src_width = GST_VIDEO_FRAME_WIDTH (srcframe); \
  src_height = GST_VIDEO_FRAME_HEIGHT (srcframe); \
  \
  info = srcframe->info.finfo; \
  dest_width = GST_VIDEO_FRAME_WIDTH (destframe); \
  dest_height = GST_VIDEO_FRAME_HEIGHT (destframe); \
  \
  if (dst_y_end > dest_height) { \
    dst_y_end = dest_height; \
  } \
  xpos = GST_ROUND_UP_2 (xpos); \
  ypos = GST_ROUND_UP_2 (ypos); \
  \
  b_src_width = src_width; \
  b_src_height = src_height; \
  \
  /* adjust src pointers for negative sizes */ \
  if (xpos < 0) { \
    xoffset = -xpos; \
    b_src_width -= -xpos; \
    xpos = 0; \
  } \
  if (ypos < dst_y_start) { \
    yoffset += dst_y_start - ypos; \
    b_src_height -= dst_y_start - ypos; \
    ypos = dst_y_start; \
  } \
  /* If x or y offset are larger then the source it's outside of the picture */ \
  if (xoffset > src_width || yoffset > src_height) { \
    return; \
  } \
  \
  /* adjust width/height if the src is bigger than dest */ \
  if (xpos + b_src_width > dest_width) { \
    b_src_width = dest_width - xpos; \
  } \
  if (ypos + b_src_height > dst_y_end) { \
    b_src_height = dst_y_end - ypos; \
  } \
  if (b_src_width < 0 || b_src_height < 0) { \
    return; \
  } \
  \
  /* First mix Y, then UV */ \
  b_src = GST_VIDEO_FRAME_COMP_DATA (srcframe, 0); \
  b_dest = GST_VIDEO_FRAME_COMP_DATA (destframe, 0); \
  src_comp_rowstride = GST_VIDEO_FRAME_COMP_STRIDE (srcframe, 0); \
  dest_comp_rowstride = GST_VIDEO_FRAME_COMP_STRIDE (destframe, 0); \
  src_comp_width = GST_VIDEO_FORMAT_INFO_SCALE_WIDTH(info, 0, b_src_width); \
  src_comp_height = GST_VIDEO_FORMAT_INFO_SCALE_HEIGHT(info, 0, b_src_height); \
  comp_xpos = (xpos == 0) ? 0 : GST_VIDEO_FORMAT_INFO_SCALE_WIDTH (info, 0, xpos); \
  comp_ypos = (ypos == 0) ? 0 : GST_VIDEO_FORMAT_INFO_SCALE_HEIGHT (info, 0, ypos); \
  comp_xoffset = (xoffset == 0) ? 0 : GST_VIDEO_FORMAT_INFO_SCALE_WIDTH (info, 0, xoffset); \
  comp_yoffset = (yoffset == 0) ? 0 : GST_VIDEO_FORMAT_INFO_SCALE_HEIGHT (info, 0, yoffset); \
  _blend_##format_name (b_src + comp_xoffset + comp_yoffset * src_comp_rowstride, \
      b_dest + comp_xpos + comp_ypos * dest_comp_rowstride, \
      src_comp_rowstride, \
      dest_comp_rowstride, src_comp_width, src_comp_height, \
      src_alpha, mode); \
  \
  b_src = GST_VIDEO_FRAME_PLANE_DATA (srcframe, 1); \
  b_dest = GST_VIDEO_FRAME_PLANE_DATA (destframe, 1); \
  src_comp_rowstride = GST_VIDEO_FRAME_COMP_STRIDE (srcframe, 1); \
  dest_comp_rowstride = GST_VIDEO_FRAME_COMP_STRIDE (destframe, 1); \
  src_comp_width = GST_VIDEO_FORMAT_INFO_SCALE_WIDTH(info, 1, b_src_width); \
  src_comp_height = GST_VIDEO_FORMAT_INFO_SCALE_HEIGHT(info, 1, b_src_height); \
  comp_xpos = (xpos == 0) ? 0 : GST_VIDEO_FORMAT_INFO_SCALE_WIDTH (info, 1, xpos); \
  comp_ypos = (ypos == 0) ? 0 : ypos >> info->h_sub[1]; \
  comp_xoffset = (xoffset == 0) ? 0 : GST_VIDEO_FORMAT_INFO_SCALE_WIDTH (info, 1, xoffset); \
  comp_yoffset = (yoffset == 0) ? 0 : yoffset >> info->h_sub[1]; \
  _blend_##format_name (b_src + comp_xoffset * 2 + comp_yoffset * src_comp_rowstride, \
      b_dest + comp_xpos * 2 + comp_ypos * dest_comp_rowstride, \
      src_comp_rowstride, \
      dest_comp_rowstride, 2 * src_comp_width, src_comp_height, \
      src_alpha, mode); \
}

#define NV_YUV_FILL_CHECKER(format_name, MEMSET)        \
static void \
fill_checker_##format_name (GstVideoFrame * frame, guint y_start, guint y_end) \
{ \
  gint i, j; \
  static const int tab[] = { 80, 160, 80, 160 }; \
  guint8 *p; \
  gint comp_width, comp_height; \
  gint rowstride, comp_yoffset; \
  const GstVideoFormatInfo *info; \
  \
  info = frame->info.finfo; \
  p = GST_VIDEO_FRAME_COMP_DATA (frame, 0); \
  comp_width = GST_VIDEO_FRAME_COMP_WIDTH (frame, 0); \
  comp_height = GST_VIDEO_FORMAT_INFO_SCALE_HEIGHT(info, 0, y_end - y_start); \
  rowstride = GST_VIDEO_FRAME_COMP_STRIDE (frame, 0); \
  comp_yoffset = (y_start == 0) ? 0 : GST_VIDEO_FORMAT_INFO_SCALE_HEIGHT (info, 0, y_start); \
  p += comp_yoffset * rowstride; \
  \
  for (i = 0; i < comp_height; i++) { \
    for (j = 0; j < comp_width; j++) { \
      *p++ = tab[(((i + y_start) & 0x8) >> 3) + ((j & 0x8) >> 3)]; \
    } \
    p += rowstride - comp_width; \
  } \
  \
  p = GST_VIDEO_FRAME_PLANE_DATA (frame, 1); \
  comp_width = GST_VIDEO_FRAME_COMP_WIDTH (frame, 1); \
  comp_height = GST_VIDEO_FORMAT_INFO_SCALE_HEIGHT(info, 1, y_end - y_start); \
  rowstride = GST_VIDEO_FRAME_COMP_STRIDE (frame, 1); \
  comp_yoffset = (y_start == 0) ? 0 : y_start >> info->h_sub[1]; \
  p += comp_yoffset * rowstride; \
  \
  for (i = 0; i < comp_height; i++) { \
    MEMSET (p, 0x80, comp_width * 2); \
    p += rowstride; \
  } \
}

#define NV_YUV_FILL_COLOR(format_name,MEMSET) \
static void \
fill_color_##format_name (GstVideoFrame * frame, \
    guint y_start, guint y_end, gint colY, gint colU, gint colV) \
{ \
  guint8 *y, *u, *v; \
  gint comp_width, comp_height; \
  gint rowstride, comp_yoffset; \
  gint i, j; \
  const GstVideoFormatInfo *info; \
  \
  info = frame->info.finfo; \
  y = GST_VIDEO_FRAME_COMP_DATA (frame, 0); \
  comp_width = GST_VIDEO_FRAME_COMP_WIDTH (frame, 0); \
  comp_height = GST_VIDEO_FORMAT_INFO_SCALE_HEIGHT(info, 0, y_end - y_start); \
  rowstride = GST_VIDEO_FRAME_COMP_STRIDE (frame, 0); \
  comp_yoffset = (y_start == 0) ? 0 : GST_VIDEO_FORMAT_INFO_SCALE_HEIGHT (info, 0, y_start); \
  \
  y += comp_yoffset * rowstride; \
  for (i = 0; i < comp_height; i++) { \
    MEMSET (y, colY, comp_width); \
    y += rowstride; \
  } \
  \
  u = GST_VIDEO_FRAME_COMP_DATA (frame, 1); \
  v = GST_VIDEO_FRAME_COMP_DATA (frame, 2); \
  comp_width = GST_VIDEO_FRAME_COMP_WIDTH (frame, 1); \
  comp_height = GST_VIDEO_FORMAT_INFO_SCALE_HEIGHT(info, 1, y_end - y_start); \
  rowstride = GST_VIDEO_FRAME_COMP_STRIDE (frame, 1); \
  comp_yoffset = (y_start == 0) ? 0 : y_start >> info->h_sub[1]; \
  \
  u += comp_yoffset * rowstride; \
  v += comp_yoffset * rowstride; \
  for (i = 0; i < comp_height; i++) { \
    for (j = 0; j < comp_width; j++) { \
      u[j*2] = colU; \
      v[j*2] = colV; \
    } \
    u += rowstride; \
    v += rowstride; \
  } \
}

NV_YUV_BLEND (nv12, memcpy, compositor_orc_blend_u8);
NV_YUV_FILL_CHECKER (nv12, memset);
NV_YUV_FILL_COLOR (nv12, memset);
NV_YUV_BLEND (nv21, memcpy, compositor_orc_blend_u8);
NV_YUV_FILL_CHECKER (nv21, memset);

/* RGB, BGR, xRGB, xBGR, RGBx, BGRx */

#define RGB_BLEND(name, bpp, MEMCPY, BLENDLOOP) \
static void \
blend_##name (GstVideoFrame * srcframe, gint xpos, gint ypos, \
    gdouble src_alpha, GstVideoFrame * destframe, gint dst_y_start, \
    gint dst_y_end, GstCompositorBlendMode mode) \
{ \
  gint b_alpha; \
  gint i; \
  gint src_stride, dest_stride; \
  gint dest_width, dest_height; \
  guint8 *dest, *src; \
  gint src_width, src_height; \
  \
  src_width = GST_VIDEO_FRAME_WIDTH (srcframe); \
  src_height = GST_VIDEO_FRAME_HEIGHT (srcframe); \
  \
  src = GST_VIDEO_FRAME_PLANE_DATA (srcframe, 0); \
  dest = GST_VIDEO_FRAME_PLANE_DATA (destframe, 0); \
  \
  dest_width = GST_VIDEO_FRAME_WIDTH (destframe); \
  dest_height = GST_VIDEO_FRAME_HEIGHT (destframe); \
  \
  src_stride = GST_VIDEO_FRAME_COMP_STRIDE (srcframe, 0); \
  dest_stride = GST_VIDEO_FRAME_COMP_STRIDE (destframe, 0); \
  \
  b_alpha = CLAMP ((gint) (src_alpha * 255), 0, 255); \
  \
  if (dst_y_end > dest_height) { \
    dst_y_end = dest_height; \
  } \
  /* adjust src pointers for negative sizes */ \
  if (xpos < 0) { \
    src += -xpos * bpp; \
    src_width -= -xpos; \
    xpos = 0; \
  } \
  if (ypos < dst_y_start) { \
    src += (dst_y_start - ypos) * src_stride; \
    src_height -= dst_y_start - ypos; \
    ypos = dst_y_start; \
  } \
  /* adjust width/height if the src is bigger than dest */ \
  if (xpos + src_width > dest_width) { \
    src_width = dest_width - xpos; \
  } \
  if (ypos + src_height > dst_y_end) { \
    src_height = dst_y_end - ypos; \
  } \
  \
  dest = dest + bpp * xpos + (ypos * dest_stride); \
  \
  /* in source mode we just have to copy over things */ \
  if (mode == COMPOSITOR_BLEND_MODE_SOURCE) { \
    src_alpha = 1.0; \
  } \
  \
  /* If it's completely transparent... we just return */ \
  if (G_UNLIKELY (src_alpha == 0.0)) { \
    GST_LOG ("Fast copy (alpha == 0.0)"); \
    return; \
  } \
  \
  /* If it's completely opaque, we do a fast copy */ \
  if (G_UNLIKELY (src_alpha == 1.0)) { \
    GST_LOG ("Fast copy (alpha == 1.0)"); \
    for (i = 0; i < src_height; i++) { \
      MEMCPY (dest, src, bpp * src_width); \
      src += src_stride; \
      dest += dest_stride; \
    } \
    return; \
  } \
  \
  BLENDLOOP(dest, dest_stride, src, src_stride, b_alpha, src_width * bpp, src_height); \
}

#define RGB_FILL_CHECKER_C(name, bpp, r, g, b) \
static void \
fill_checker_##name##_c (GstVideoFrame * frame, guint y_start, guint y_end) \
{ \
  gint i, j; \
  static const int tab[] = { 80, 160, 80, 160 }; \
  gint stride, dest_add, width, height; \
  guint8 *dest; \
  \
  width = GST_VIDEO_FRAME_WIDTH (frame); \
  height = y_end - y_start; \
  dest = GST_VIDEO_FRAME_PLANE_DATA (frame, 0); \
  stride = GST_VIDEO_FRAME_COMP_STRIDE (frame, 0); \
  dest_add = stride - width * bpp; \
  \
  dest += y_start * stride; \
  for (i = 0; i < height; i++) { \
    for (j = 0; j < width; j++) { \
      dest[r] = tab[(((i + y_start) & 0x8) >> 3) + ((j & 0x8) >> 3)];       /* red */ \
      dest[g] = tab[(((i + y_start) & 0x8) >> 3) + ((j & 0x8) >> 3)];       /* green */ \
      dest[b] = tab[(((i + y_start) & 0x8) >> 3) + ((j & 0x8) >> 3)];       /* blue */ \
      dest += bpp; \
    } \
    dest += dest_add; \
  } \
}

#define RGB_FILL_COLOR(name, bpp, MEMSET_RGB) \
static void \
fill_color_##name (GstVideoFrame * frame, \
    guint y_start, guint y_end, gint colR, gint colG, gint colB) \
{ \
  gint i; \
  gint dest_stride; \
  gint width, height; \
  guint8 *dest; \
  \
  width = GST_VIDEO_FRAME_WIDTH (frame); \
  height = y_end - y_start; \
  dest = GST_VIDEO_FRAME_PLANE_DATA (frame, 0); \
  dest_stride = GST_VIDEO_FRAME_COMP_STRIDE (frame, 0); \
  \
  dest += y_start * dest_stride; \
  for (i = 0; i < height; i++) { \
    MEMSET_RGB (dest, colR, colG, colB, width); \
    dest += dest_stride; \
  } \
}

#define MEMSET_RGB_C(name, r, g, b) \
static inline void \
_memset_##name##_c (guint8* dest, gint red, gint green, gint blue, gint width) { \
  gint j; \
  \
  for (j = 0; j < width; j++) { \
    dest[r] = red; \
    dest[g] = green; \
    dest[b] = blue; \
    dest += 3; \
  } \
}

#define MEMSET_XRGB(name, r, g, b) \
static inline void \
_memset_##name (guint8* dest, gint red, gint green, gint blue, gint width) { \
  guint32 val; \
  \
  val = GUINT32_FROM_BE ((red << r) | (green << g) | (blue << b)); \
  compositor_orc_splat_u32 ((guint32 *) dest, val, width); \
}

#define _orc_memcpy_u32(dest,src,len) compositor_orc_memcpy_u32((guint32 *) dest, (const guint32 *) src, len/4)

RGB_BLEND (rgb, 3, memcpy, compositor_orc_blend_u8);
RGB_FILL_CHECKER_C (rgb, 3, 0, 1, 2);
MEMSET_RGB_C (rgb, 0, 1, 2);
RGB_FILL_COLOR (rgb_c, 3, _memset_rgb_c);

MEMSET_RGB_C (bgr, 2, 1, 0);
RGB_FILL_COLOR (bgr_c, 3, _memset_bgr_c);

RGB_BLEND (xrgb, 4, _orc_memcpy_u32, compositor_orc_blend_u8);
RGB_FILL_CHECKER_C (xrgb, 4, 1, 2, 3);
MEMSET_XRGB (xrgb, 24, 16, 0);
RGB_FILL_COLOR (xrgb, 4, _memset_xrgb);

MEMSET_XRGB (xbgr, 0, 16, 24);
RGB_FILL_COLOR (xbgr, 4, _memset_xbgr);

RGB_FILL_CHECKER_C (rgbx, 4, 0, 1, 2);
MEMSET_XRGB (rgbx, 24, 16, 8);
RGB_FILL_COLOR (rgbx, 4, _memset_rgbx);

MEMSET_XRGB (bgrx, 8, 16, 24);
RGB_FILL_COLOR (bgrx, 4, _memset_bgrx);

/* YUY2, YVYU, UYVY */

#define PACKED_422_BLEND(name, MEMCPY, BLENDLOOP) \
static void \
blend_##name (GstVideoFrame * srcframe, gint xpos, gint ypos, \
    gdouble src_alpha, GstVideoFrame * destframe, gint dst_y_start, \
    gint dst_y_end, GstCompositorBlendMode mode) \
{ \
  gint b_alpha; \
  gint i; \
  gint src_stride, dest_stride; \
  gint dest_width, dest_height; \
  guint8 *src, *dest; \
  gint src_width, src_height; \
  \
  src_width = GST_VIDEO_FRAME_WIDTH (srcframe); \
  src_height = GST_VIDEO_FRAME_HEIGHT (srcframe); \
  \
  dest_width = GST_VIDEO_FRAME_WIDTH (destframe); \
  dest_height = GST_VIDEO_FRAME_HEIGHT (destframe); \
  \
  src = GST_VIDEO_FRAME_PLANE_DATA (srcframe, 0); \
  dest = GST_VIDEO_FRAME_PLANE_DATA (destframe, 0); \
  \
  src_stride = GST_VIDEO_FRAME_COMP_STRIDE (srcframe, 0); \
  dest_stride = GST_VIDEO_FRAME_COMP_STRIDE (destframe, 0); \
  \
  b_alpha = CLAMP ((gint) (src_alpha * 255), 0, 255); \
  \
  xpos = GST_ROUND_UP_2 (xpos); \
  \
  if (dst_y_end > dest_height) { \
    dst_y_end = dest_height; \
  } \
  /* adjust src pointers for negative sizes */ \
  if (xpos < 0) { \
    src += -xpos * 2; \
    src_width -= -xpos; \
    xpos = 0; \
  } \
  if (ypos < dst_y_start) { \
    src += (dst_y_start - ypos) * src_stride; \
    src_height -= dst_y_start - ypos; \
    ypos = dst_y_start; \
  } \
  \
  /* adjust width/height if the src is bigger than dest */ \
  if (xpos + src_width > dest_width) { \
    src_width = dest_width - xpos; \
  } \
  if (ypos + src_height > dst_y_end) { \
    src_height = dst_y_end - ypos; \
  } \
  \
  dest = dest + 2 * xpos + (ypos * dest_stride); \
  \
  /* in source mode we just have to copy over things */ \
  if (mode == COMPOSITOR_BLEND_MODE_SOURCE) { \
    src_alpha = 1.0; \
  } \
  \
  /* If it's completely transparent... we just return */ \
  if (G_UNLIKELY (src_alpha == 0.0)) { \
    GST_LOG ("Fast copy (alpha == 0.0)"); \
    return; \
  } \
  \
  /* If it's completely opaque, we do a fast copy */ \
  if (G_UNLIKELY (src_alpha == 1.0)) { \
    GST_LOG ("Fast copy (alpha == 1.0)"); \
    for (i = 0; i < src_height; i++) { \
      MEMCPY (dest, src, 2 * src_width); \
      src += src_stride; \
      dest += dest_stride; \
    } \
    return; \
  } \
  \
  BLENDLOOP(dest, dest_stride, src, src_stride, b_alpha, 2 * src_width, src_height); \
}

#define PACKED_422_FILL_CHECKER_C(name, Y1, U, Y2, V) \
static void \
fill_checker_##name##_c (GstVideoFrame * frame, guint y_start, guint y_end) \
{ \
  gint i, j; \
  static const int tab[] = { 80, 160, 80, 160 }; \
  gint dest_add; \
  gint width, height; \
  guint8 *dest; \
  \
  width = GST_VIDEO_FRAME_WIDTH (frame); \
  width = GST_ROUND_UP_2 (width); \
  height = y_end - y_start; \
  dest = GST_VIDEO_FRAME_PLANE_DATA (frame, 0); \
  dest_add = GST_VIDEO_FRAME_COMP_STRIDE (frame, 0) - width * 2; \
  width /= 2; \
  \
  dest += GST_VIDEO_FRAME_COMP_STRIDE (frame, 0) * y_start; \
  for (i = 0; i < height; i++) { \
    for (j = 0; j < width; j++) { \
      dest[Y1] = tab[(((i + y_start) & 0x8) >> 3) + (((2 * j + 0) & 0x8) >> 3)]; \
      dest[Y2] = tab[(((i + y_start) & 0x8) >> 3) + (((2 * j + 1) & 0x8) >> 3)]; \
      dest[U] = 128; \
      dest[V] = 128; \
      dest += 4; \
    } \
    dest += dest_add; \
  } \
}

#define PACKED_422_FILL_COLOR(name, Y1, U, Y2, V) \
static void \
fill_color_##name (GstVideoFrame * frame, \
    guint y_start, guint y_end, gint colY, gint colU, gint colV) \
{ \
  gint i; \
  gint dest_stride; \
  guint32 val; \
  gint width, height; \
  guint8 *dest; \
  \
  width = GST_VIDEO_FRAME_WIDTH (frame); \
  width = GST_ROUND_UP_2 (width); \
  height = y_end - y_start; \
  dest = GST_VIDEO_FRAME_PLANE_DATA (frame, 0); \
  dest_stride = GST_VIDEO_FRAME_COMP_STRIDE (frame, 0); \
  width /= 2; \
  \
  val = GUINT32_FROM_BE ((colY << Y1) | (colY << Y2) | (colU << U) | (colV << V)); \
  \
  dest += dest_stride * y_start; \
  for (i = 0; i < height; i++) { \
    compositor_orc_splat_u32 ((guint32 *) dest, val, width); \
    dest += dest_stride; \
  } \
}

PACKED_422_BLEND (yuy2, memcpy, compositor_orc_blend_u8);
PACKED_422_FILL_CHECKER_C (yuy2, 0, 1, 2, 3);
PACKED_422_FILL_CHECKER_C (uyvy, 1, 0, 3, 2);
PACKED_422_FILL_COLOR (yuy2, 24, 16, 8, 0);
PACKED_422_FILL_COLOR (yvyu, 24, 0, 8, 16);
PACKED_422_FILL_COLOR (uyvy, 16, 24, 0, 8);

/* Init function */
BlendFunction gst_compositor_blend_argb;
BlendFunction gst_compositor_blend_bgra;
BlendFunction gst_compositor_overlay_argb;
BlendFunction gst_compositor_overlay_bgra;
/* AYUV/ABGR is equal to ARGB, RGBA is equal to BGRA */
BlendFunction gst_compositor_blend_y444;
BlendFunction gst_compositor_blend_y42b;
BlendFunction gst_compositor_blend_i420;
/* I420 is equal to YV12 */
BlendFunction gst_compositor_blend_nv12;
BlendFunction gst_compositor_blend_nv21;
BlendFunction gst_compositor_blend_y41b;
BlendFunction gst_compositor_blend_rgb;
/* BGR is equal to RGB */
BlendFunction gst_compositor_blend_rgbx;
/* BGRx, xRGB, xBGR are equal to RGBx */
BlendFunction gst_compositor_blend_yuy2;
/* YVYU and UYVY are equal to YUY2 */
BlendFunction gst_compositor_blend_i420_10le;
BlendFunction gst_compositor_blend_i420_10be;
BlendFunction gst_compositor_blend_i420_12le;
BlendFunction gst_compositor_blend_i420_12be;
BlendFunction gst_compositor_blend_i422_10le;
BlendFunction gst_compositor_blend_i422_10be;
BlendFunction gst_compositor_blend_i422_12le;
BlendFunction gst_compositor_blend_i422_12be;
BlendFunction gst_compositor_blend_y444_10le;
BlendFunction gst_compositor_blend_y444_10be;
BlendFunction gst_compositor_blend_y444_12le;
BlendFunction gst_compositor_blend_y444_12be;
BlendFunction gst_compositor_blend_y444_16le;
BlendFunction gst_compositor_blend_y444_16be;
BlendFunction gst_compositor_blend_argb64;
BlendFunction gst_compositor_overlay_argb64;
/* AYUV64 is equal to ARGB64 */

FillCheckerFunction gst_compositor_fill_checker_argb;
FillCheckerFunction gst_compositor_fill_checker_bgra;
/* ABGR is equal to ARGB, RGBA is equal to BGRA */
FillCheckerFunction gst_compositor_fill_checker_ayuv;
FillCheckerFunction gst_compositor_fill_checker_vuya;
FillCheckerFunction gst_compositor_fill_checker_y444;
FillCheckerFunction gst_compositor_fill_checker_y42b;
FillCheckerFunction gst_compositor_fill_checker_i420;
/* I420 is equal to YV12 */
FillCheckerFunction gst_compositor_fill_checker_nv12;
FillCheckerFunction gst_compositor_fill_checker_nv21;
FillCheckerFunction gst_compositor_fill_checker_y41b;
FillCheckerFunction gst_compositor_fill_checker_rgb;
/* BGR is equal to RGB */
FillCheckerFunction gst_compositor_fill_checker_xrgb;
FillCheckerFunction gst_compositor_fill_checker_rgbx;
/* BGRx, xRGB, xBGR are equal to RGBx */
FillCheckerFunction gst_compositor_fill_checker_yuy2;
/* YVYU is equal to YUY2 */
FillCheckerFunction gst_compositor_fill_checker_uyvy;
FillCheckerFunction gst_compositor_fill_checker_i420_10le;
FillCheckerFunction gst_compositor_fill_checker_i420_10be;
FillCheckerFunction gst_compositor_fill_checker_i420_12le;
FillCheckerFunction gst_compositor_fill_checker_i420_12be;
FillCheckerFunction gst_compositor_fill_checker_y444_16le;
FillCheckerFunction gst_compositor_fill_checker_y444_16be;
FillCheckerFunction gst_compositor_fill_checker_argb64;
FillCheckerFunction gst_compositor_fill_checker_ayuv64;

FillColorFunction gst_compositor_fill_color_argb;
FillColorFunction gst_compositor_fill_color_bgra;
FillColorFunction gst_compositor_fill_color_abgr;
FillColorFunction gst_compositor_fill_color_rgba;
FillColorFunction gst_compositor_fill_color_ayuv;
FillColorFunction gst_compositor_fill_color_vuya;
FillColorFunction gst_compositor_fill_color_y444;
FillColorFunction gst_compositor_fill_color_y42b;
FillColorFunction gst_compositor_fill_color_i420;
FillColorFunction gst_compositor_fill_color_yv12;
FillColorFunction gst_compositor_fill_color_nv12;
/* NV21 is equal to NV12 */
FillColorFunction gst_compositor_fill_color_y41b;
FillColorFunction gst_compositor_fill_color_rgb;
FillColorFunction gst_compositor_fill_color_bgr;
FillColorFunction gst_compositor_fill_color_xrgb;
FillColorFunction gst_compositor_fill_color_xbgr;
FillColorFunction gst_compositor_fill_color_rgbx;
FillColorFunction gst_compositor_fill_color_bgrx;
FillColorFunction gst_compositor_fill_color_yuy2;
FillColorFunction gst_compositor_fill_color_yvyu;
FillColorFunction gst_compositor_fill_color_uyvy;
FillColorFunction gst_compositor_fill_color_i420_10le;
FillColorFunction gst_compositor_fill_color_i420_10be;
FillColorFunction gst_compositor_fill_color_i420_12le;
FillColorFunction gst_compositor_fill_color_i420_12be;
FillColorFunction gst_compositor_fill_color_y444_16le;
FillColorFunction gst_compositor_fill_color_y444_16be;
FillColorFunction gst_compositor_fill_color_argb64;

void
gst_compositor_init_blend (void)
{
  GST_DEBUG_CATEGORY_INIT (gst_compositor_blend_debug, "compositor_blend", 0,
      "video compositor blending functions");

  gst_compositor_blend_argb = GST_DEBUG_FUNCPTR (blend_argb);
  gst_compositor_blend_bgra = GST_DEBUG_FUNCPTR (blend_bgra);
  gst_compositor_overlay_argb = GST_DEBUG_FUNCPTR (overlay_argb);
  gst_compositor_overlay_bgra = GST_DEBUG_FUNCPTR (overlay_bgra);
  gst_compositor_blend_i420 = GST_DEBUG_FUNCPTR (blend_i420);
  gst_compositor_blend_nv12 = GST_DEBUG_FUNCPTR (blend_nv12);
  gst_compositor_blend_nv21 = GST_DEBUG_FUNCPTR (blend_nv21);
  gst_compositor_blend_y444 = GST_DEBUG_FUNCPTR (blend_y444);
  gst_compositor_blend_y42b = GST_DEBUG_FUNCPTR (blend_y42b);
  gst_compositor_blend_y41b = GST_DEBUG_FUNCPTR (blend_y41b);
  gst_compositor_blend_rgb = GST_DEBUG_FUNCPTR (blend_rgb);
  gst_compositor_blend_xrgb = GST_DEBUG_FUNCPTR (blend_xrgb);
  gst_compositor_blend_yuy2 = GST_DEBUG_FUNCPTR (blend_yuy2);
  gst_compositor_blend_i420_10le = GST_DEBUG_FUNCPTR (blend_i420_10le);
  gst_compositor_blend_i420_10be = GST_DEBUG_FUNCPTR (blend_i420_10be);
  gst_compositor_blend_i420_12le = GST_DEBUG_FUNCPTR (blend_i420_12le);
  gst_compositor_blend_i420_12be = GST_DEBUG_FUNCPTR (blend_i420_12be);
  gst_compositor_blend_i422_10le = GST_DEBUG_FUNCPTR (blend_i422_10le);
  gst_compositor_blend_i422_10be = GST_DEBUG_FUNCPTR (blend_i422_10be);
  gst_compositor_blend_i422_12le = GST_DEBUG_FUNCPTR (blend_i422_12le);
  gst_compositor_blend_i422_12be = GST_DEBUG_FUNCPTR (blend_i422_12be);
  gst_compositor_blend_y444_10le = GST_DEBUG_FUNCPTR (blend_y444_10le);
  gst_compositor_blend_y444_10be = GST_DEBUG_FUNCPTR (blend_y444_10be);
  gst_compositor_blend_y444_12le = GST_DEBUG_FUNCPTR (blend_y444_12le);
  gst_compositor_blend_y444_12be = GST_DEBUG_FUNCPTR (blend_y444_12be);
  gst_compositor_blend_y444_16le = GST_DEBUG_FUNCPTR (blend_y444_16le);
  gst_compositor_blend_y444_16be = GST_DEBUG_FUNCPTR (blend_y444_16be);
  gst_compositor_blend_argb64 = GST_DEBUG_FUNCPTR (blend_argb64);
  gst_compositor_overlay_argb64 = GST_DEBUG_FUNCPTR (overlay_argb64);

  gst_compositor_fill_checker_argb = GST_DEBUG_FUNCPTR (fill_checker_argb_c);
  gst_compositor_fill_checker_bgra = GST_DEBUG_FUNCPTR (fill_checker_bgra_c);
  gst_compositor_fill_checker_ayuv = GST_DEBUG_FUNCPTR (fill_checker_ayuv_c);
  gst_compositor_fill_checker_vuya = GST_DEBUG_FUNCPTR (fill_checker_vuya_c);
  gst_compositor_fill_checker_i420 = GST_DEBUG_FUNCPTR (fill_checker_i420);
  gst_compositor_fill_checker_nv12 = GST_DEBUG_FUNCPTR (fill_checker_nv12);
  gst_compositor_fill_checker_nv21 = GST_DEBUG_FUNCPTR (fill_checker_nv21);
  gst_compositor_fill_checker_y444 = GST_DEBUG_FUNCPTR (fill_checker_y444);
  gst_compositor_fill_checker_y42b = GST_DEBUG_FUNCPTR (fill_checker_y42b);
  gst_compositor_fill_checker_y41b = GST_DEBUG_FUNCPTR (fill_checker_y41b);
  gst_compositor_fill_checker_rgb = GST_DEBUG_FUNCPTR (fill_checker_rgb_c);
  gst_compositor_fill_checker_xrgb = GST_DEBUG_FUNCPTR (fill_checker_xrgb_c);
  gst_compositor_fill_checker_rgbx = GST_DEBUG_FUNCPTR (fill_checker_rgbx_c);
  gst_compositor_fill_checker_yuy2 = GST_DEBUG_FUNCPTR (fill_checker_yuy2_c);
  gst_compositor_fill_checker_uyvy = GST_DEBUG_FUNCPTR (fill_checker_uyvy_c);
  gst_compositor_fill_checker_i420_10le =
      GST_DEBUG_FUNCPTR (fill_checker_i420_10le);
  gst_compositor_fill_checker_i420_10be =
      GST_DEBUG_FUNCPTR (fill_checker_i420_10be);
  gst_compositor_fill_checker_i420_12le =
      GST_DEBUG_FUNCPTR (fill_checker_i420_12le);
  gst_compositor_fill_checker_i420_12be =
      GST_DEBUG_FUNCPTR (fill_checker_i420_12be);
  gst_compositor_fill_checker_y444_16le =
      GST_DEBUG_FUNCPTR (fill_checker_y444_16le);
  gst_compositor_fill_checker_y444_16be =
      GST_DEBUG_FUNCPTR (fill_checker_y444_16be);
  gst_compositor_fill_checker_argb64 =
      GST_DEBUG_FUNCPTR (fill_checker_argb64_c);
  gst_compositor_fill_checker_ayuv64 =
      GST_DEBUG_FUNCPTR (fill_checker_ayuv64_c);

  gst_compositor_fill_color_argb = GST_DEBUG_FUNCPTR (fill_color_argb);
  gst_compositor_fill_color_bgra = GST_DEBUG_FUNCPTR (fill_color_bgra);
  gst_compositor_fill_color_abgr = GST_DEBUG_FUNCPTR (fill_color_abgr);
  gst_compositor_fill_color_rgba = GST_DEBUG_FUNCPTR (fill_color_rgba);
  gst_compositor_fill_color_ayuv = GST_DEBUG_FUNCPTR (fill_color_ayuv);
  gst_compositor_fill_color_vuya = GST_DEBUG_FUNCPTR (fill_color_vuya);
  gst_compositor_fill_color_i420 = GST_DEBUG_FUNCPTR (fill_color_i420);
  gst_compositor_fill_color_yv12 = GST_DEBUG_FUNCPTR (fill_color_yv12);
  gst_compositor_fill_color_nv12 = GST_DEBUG_FUNCPTR (fill_color_nv12);
  gst_compositor_fill_color_y444 = GST_DEBUG_FUNCPTR (fill_color_y444);
  gst_compositor_fill_color_y42b = GST_DEBUG_FUNCPTR (fill_color_y42b);
  gst_compositor_fill_color_y41b = GST_DEBUG_FUNCPTR (fill_color_y41b);
  gst_compositor_fill_color_rgb = GST_DEBUG_FUNCPTR (fill_color_rgb_c);
  gst_compositor_fill_color_bgr = GST_DEBUG_FUNCPTR (fill_color_bgr_c);
  gst_compositor_fill_color_xrgb = GST_DEBUG_FUNCPTR (fill_color_xrgb);
  gst_compositor_fill_color_xbgr = GST_DEBUG_FUNCPTR (fill_color_xbgr);
  gst_compositor_fill_color_rgbx = GST_DEBUG_FUNCPTR (fill_color_rgbx);
  gst_compositor_fill_color_bgrx = GST_DEBUG_FUNCPTR (fill_color_bgrx);
  gst_compositor_fill_color_yuy2 = GST_DEBUG_FUNCPTR (fill_color_yuy2);
  gst_compositor_fill_color_yvyu = GST_DEBUG_FUNCPTR (fill_color_yvyu);
  gst_compositor_fill_color_uyvy = GST_DEBUG_FUNCPTR (fill_color_uyvy);
  gst_compositor_fill_color_i420_10le =
      GST_DEBUG_FUNCPTR (fill_color_i420_10le);
  gst_compositor_fill_color_i420_10be =
      GST_DEBUG_FUNCPTR (fill_color_i420_10be);
  gst_compositor_fill_color_i420_12le =
      GST_DEBUG_FUNCPTR (fill_color_i420_12le);
  gst_compositor_fill_color_i420_12be =
      GST_DEBUG_FUNCPTR (fill_color_i420_12be);
  gst_compositor_fill_color_y444_16le =
      GST_DEBUG_FUNCPTR (fill_color_y444_16le);
  gst_compositor_fill_color_y444_16be =
      GST_DEBUG_FUNCPTR (fill_color_y444_16be);
  gst_compositor_fill_color_argb64 = GST_DEBUG_FUNCPTR (fill_color_argb64);
}
