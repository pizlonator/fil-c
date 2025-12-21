/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 * Library       <2002> Ronald Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2007 David A. Schleef <ds@schleef.org>
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

#include <string.h>
#include <stdio.h>

#include "video-format.h"
#include "video-orc.h"

#ifndef restrict
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
/* restrict should be available */
#elif defined(__GNUC__) && __GNUC__ >= 4
#define restrict __restrict__
#elif defined(_MSC_VER) &&  _MSC_VER >= 1500
#define restrict __restrict
#else
#define restrict                /* no op */
#endif
#endif

/* Line conversion to AYUV */

#define GET_PLANE_STRIDE(plane) (stride(plane))
#define GET_PLANE_LINE(plane, line) \
  (gpointer)(((guint8*)(data[plane])) + stride[plane] * (line))

#define GET_COMP_STRIDE(comp) \
  GST_VIDEO_FORMAT_INFO_STRIDE (info, stride, comp)
#define GET_COMP_DATA(comp) \
  GST_VIDEO_FORMAT_INFO_DATA (info, data, comp)

#define GET_COMP_LINE(comp, line) \
  (gpointer)(((guint8*)GET_COMP_DATA (comp)) + \
      GET_COMP_STRIDE(comp) * (line))

#define GET_LINE(line)               GET_PLANE_LINE (0, line)

#define GET_Y_LINE(line)             GET_COMP_LINE(GST_VIDEO_COMP_Y, line)
#define GET_U_LINE(line)             GET_COMP_LINE(GST_VIDEO_COMP_U, line)
#define GET_V_LINE(line)             GET_COMP_LINE(GST_VIDEO_COMP_V, line)

#define GET_R_LINE(line)             GET_COMP_LINE(GST_VIDEO_COMP_R, line)
#define GET_G_LINE(line)             GET_COMP_LINE(GST_VIDEO_COMP_G, line)
#define GET_B_LINE(line)             GET_COMP_LINE(GST_VIDEO_COMP_B, line)

#define GET_A_LINE(line)             GET_COMP_LINE(GST_VIDEO_COMP_A, line)

#define GET_UV_420(line, flags)                 \
  (flags & GST_VIDEO_PACK_FLAG_INTERLACED ?     \
   ((line & ~3) >> 1) + (line & 1) :            \
   line >> 1)
#define GET_UV_410(line, flags)                 \
  (flags & GST_VIDEO_PACK_FLAG_INTERLACED ?     \
   ((line & ~7) >> 2) + (line & 1) :            \
   line >> 2)

#define IS_CHROMA_LINE_420(line, flags)         \
  (flags & GST_VIDEO_PACK_FLAG_INTERLACED ?     \
   !(line & 2) : !(line & 1))
#define IS_CHROMA_LINE_410(line, flags)         \
  (flags & GST_VIDEO_PACK_FLAG_INTERLACED ?     \
   !(line & 6) : !(line & 3))

#define IS_ALIGNED(x,n) ((((guintptr)(x)&((n)-1))) == 0)

#define PACK_420 GST_VIDEO_FORMAT_AYUV, unpack_planar_420, 1, pack_planar_420
static void
unpack_planar_420 (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  gint uv = GET_UV_420 (y, flags);
  const guint8 *restrict sy = GET_Y_LINE (y);
  const guint8 *restrict su = GET_U_LINE (uv);
  const guint8 *restrict sv = GET_V_LINE (uv);
  guint8 *restrict d = dest;

  sy += x;
  su += x >> 1;
  sv += x >> 1;

  if (x & 1) {
    d[0] = 0xff;
    d[1] = *sy++;
    d[2] = *su++;
    d[3] = *sv++;
    width--;
    d += 4;
  }
  video_orc_unpack_I420 (d, sy, su, sv, width);
}

static void
pack_planar_420 (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  gint uv = GET_UV_420 (y, flags);
  guint8 *dy = GET_Y_LINE (y);
  guint8 *du = GET_U_LINE (uv);
  guint8 *dv = GET_V_LINE (uv);
  const guint8 *s = src;

  if (IS_CHROMA_LINE_420 (y, flags)) {
    if (IS_ALIGNED (s, 8))
      video_orc_pack_I420 (dy, du, dv, s, width / 2);
    else {
      gint i;

      for (i = 0; i < width / 2; i++) {
        dy[i * 2 + 0] = s[i * 8 + 1];
        dy[i * 2 + 1] = s[i * 8 + 5];
        du[i] = s[i * 8 + 2];
        dv[i] = s[i * 8 + 3];
      }
    }
    if (width & 1) {
      gint i = width - 1;

      dy[i] = s[i * 4 + 1];
      du[i >> 1] = s[i * 4 + 2];
      dv[i >> 1] = s[i * 4 + 3];
    }
  } else
    video_orc_pack_Y (dy, s, width);
}

#define PACK_YUY2 GST_VIDEO_FORMAT_AYUV, unpack_YUY2, 1, pack_YUY2
static void
unpack_YUY2 (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  const guint8 *restrict s = GET_LINE (y);
  guint8 *restrict d = dest;

  s += (x & ~1) << 1;
  if (x & 1) {
    d[0] = 0xff;
    d[1] = s[2];
    d[2] = s[1];
    d[3] = s[3];
    s += 4;
    d += 4;
    width--;
  }

  if (IS_ALIGNED (d, 8))
    video_orc_unpack_YUY2 (d, s, width / 2);
  else {
    gint i;

    for (i = 0; i < width / 2; i++) {
      d[i * 8 + 0] = 0xff;
      d[i * 8 + 1] = s[i * 4 + 0];
      d[i * 8 + 2] = s[i * 4 + 1];
      d[i * 8 + 3] = s[i * 4 + 3];
      d[i * 8 + 4] = 0xff;
      d[i * 8 + 5] = s[i * 4 + 2];
      d[i * 8 + 6] = s[i * 4 + 1];
      d[i * 8 + 7] = s[i * 4 + 3];
    }
  }

  if (width & 1) {
    gint i = width - 1;

    d[i * 4 + 0] = 0xff;
    d[i * 4 + 1] = s[i * 2 + 0];
    d[i * 4 + 2] = s[i * 2 + 1];
    d[i * 4 + 3] = s[i * 2 + 3];
  }
}

static void
pack_YUY2 (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  guint8 *restrict d = GET_LINE (y);
  const guint8 *restrict s = src;

  if (IS_ALIGNED (s, 8))
    video_orc_pack_YUY2 (d, s, width / 2);
  else {
    gint i;
    for (i = 0; i < width / 2; i++) {
      d[i * 4 + 0] = s[i * 8 + 1];
      d[i * 4 + 1] = s[i * 8 + 2];
      d[i * 4 + 2] = s[i * 8 + 5];
      d[i * 4 + 3] = s[i * 8 + 3];
    }
  }

  if (width & 1) {
    gint i = width - 1;

    d[i * 2 + 0] = s[i * 4 + 1];
    d[i * 2 + 1] = s[i * 4 + 2];
    d[i * 2 + 3] = s[i * 4 + 3];
  }
}

#define PACK_UYVY GST_VIDEO_FORMAT_AYUV, unpack_UYVY, 1, pack_UYVY
static void
unpack_UYVY (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  const guint8 *s = GET_LINE (y);
  guint8 *d = dest;

  s += (x & ~1) << 1;
  if (x & 1) {
    d[0] = 0xff;
    d[1] = s[3];
    d[2] = s[0];
    d[3] = s[2];
    s += 4;
    d += 4;
    width--;
  }

  if (IS_ALIGNED (d, 8))
    video_orc_unpack_UYVY (d, s, width / 2);
  else {
    gint i;

    for (i = 0; i < width / 2; i++) {
      d[i * 8 + 0] = 0xff;
      d[i * 8 + 1] = s[i * 4 + 1];
      d[i * 8 + 2] = s[i * 4 + 0];
      d[i * 8 + 3] = s[i * 4 + 2];
      d[i * 8 + 4] = 0xff;
      d[i * 8 + 5] = s[i * 4 + 3];
      d[i * 8 + 6] = s[i * 4 + 0];
      d[i * 8 + 7] = s[i * 4 + 2];
    }
  }

  if (width & 1) {
    gint i = width - 1;

    d[i * 4 + 0] = 0xff;
    d[i * 4 + 1] = s[i * 2 + 1];
    d[i * 4 + 2] = s[i * 2 + 0];
    d[i * 4 + 3] = s[i * 2 + 2];
  }
}

static void
pack_UYVY (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  guint8 *restrict d = GET_LINE (y);
  const guint8 *restrict s = src;

  if (IS_ALIGNED (s, 8))
    video_orc_pack_UYVY (d, s, width / 2);
  else {
    gint i;
    for (i = 0; i < width / 2; i++) {
      d[i * 4 + 0] = s[i * 8 + 2];
      d[i * 4 + 1] = s[i * 8 + 1];
      d[i * 4 + 2] = s[i * 8 + 3];
      d[i * 4 + 3] = s[i * 8 + 5];
    }
  }
  if (width & 1) {
    gint i = width - 1;

    d[i * 2 + 0] = s[i * 4 + 2];
    d[i * 2 + 1] = s[i * 4 + 1];
    d[i * 2 + 2] = s[i * 4 + 3];
  }
}

#define PACK_VYUY GST_VIDEO_FORMAT_AYUV, unpack_VYUY, 1, pack_VYUY
static void
unpack_VYUY (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  const guint8 *s = GET_LINE (y);
  guint8 *d = dest;

  s += (x & ~1) << 1;
  if (x & 1) {
    d[0] = 0xff;
    d[1] = s[3];
    d[2] = s[0];
    d[3] = s[2];
    s += 4;
    d += 4;
    width--;
  }

  if (IS_ALIGNED (d, 8))
    video_orc_unpack_VYUY (d, s, width / 2);
  else {
    gint i;

    for (i = 0; i < width / 2; i++) {
      d[i * 8 + 0] = 0xff;
      d[i * 8 + 1] = s[i * 4 + 1];
      d[i * 8 + 2] = s[i * 4 + 0];
      d[i * 8 + 3] = s[i * 4 + 2];
      d[i * 8 + 4] = 0xff;
      d[i * 8 + 5] = s[i * 4 + 3];
      d[i * 8 + 6] = s[i * 4 + 0];
      d[i * 8 + 7] = s[i * 4 + 2];
    }
  }

  if (width & 1) {
    gint i = width - 1;

    d[i * 4 + 0] = 0xff;
    d[i * 4 + 1] = s[i * 2 + 1];
    d[i * 4 + 2] = s[i * 2 + 0];
    d[i * 4 + 3] = s[i * 2 + 2];
  }
}

static void
pack_VYUY (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  guint8 *restrict d = GET_LINE (y);
  const guint8 *restrict s = src;

  if (IS_ALIGNED (s, 8))
    video_orc_pack_VYUY (d, s, width / 2);
  else {
    gint i;
    for (i = 0; i < width / 2; i++) {
      d[i * 4 + 0] = s[i * 8 + 2];
      d[i * 4 + 1] = s[i * 8 + 1];
      d[i * 4 + 2] = s[i * 8 + 3];
      d[i * 4 + 3] = s[i * 8 + 5];
    }
  }
  if (width & 1) {
    gint i = width - 1;

    d[i * 2 + 0] = s[i * 4 + 2];
    d[i * 2 + 1] = s[i * 4 + 1];
    d[i * 2 + 2] = s[i * 4 + 3];
  }
}

#define PACK_YVYU GST_VIDEO_FORMAT_AYUV, unpack_YVYU, 1, pack_YVYU
static void
unpack_YVYU (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  const guint8 *restrict s = GET_LINE (y);
  guint8 *restrict d = dest;

  s += (x & ~1) << 1;
  if (x & 1) {
    d[0] = 0xff;
    d[1] = s[2];
    d[2] = s[3];
    d[3] = s[1];
    s += 4;
    d += 4;
    width--;
  }

  if (IS_ALIGNED (d, 8))
    video_orc_unpack_YVYU (d, s, width / 2);
  else {
    gint i;

    for (i = 0; i < width / 2; i++) {
      d[i * 8 + 0] = 0xff;
      d[i * 8 + 1] = s[i * 4 + 0];
      d[i * 8 + 2] = s[i * 4 + 3];
      d[i * 8 + 3] = s[i * 4 + 1];
      d[i * 8 + 4] = 0xff;
      d[i * 8 + 5] = s[i * 4 + 2];
      d[i * 8 + 6] = s[i * 4 + 3];
      d[i * 8 + 7] = s[i * 4 + 1];
    }
  }

  if (width & 1) {
    gint i = width - 1;

    d[i * 4 + 0] = 0xff;
    d[i * 4 + 1] = s[i * 2 + 0];
    d[i * 4 + 2] = s[i * 2 + 3];
    d[i * 4 + 3] = s[i * 2 + 1];
  }
}

static void
pack_YVYU (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  guint8 *restrict d = GET_LINE (y);
  const guint8 *restrict s = src;

  if (IS_ALIGNED (s, 8))
    video_orc_pack_YVYU (d, s, width / 2);
  else {
    gint i;
    for (i = 0; i < width / 2; i++) {
      d[i * 4 + 0] = s[i * 8 + 1];
      d[i * 4 + 1] = s[i * 8 + 3];
      d[i * 4 + 2] = s[i * 8 + 5];
      d[i * 4 + 3] = s[i * 8 + 2];
    }
  }

  if (width & 1) {
    gint i = width - 1;

    d[i * 2 + 0] = s[i * 4 + 1];
    d[i * 2 + 1] = s[i * 4 + 3];
    d[i * 2 + 3] = s[i * 4 + 2];
  }
}

#define PACK_v308 GST_VIDEO_FORMAT_AYUV, unpack_v308, 1, pack_v308
static void
unpack_v308 (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  int i;
  const guint8 *restrict s = GET_LINE (y);
  guint8 *restrict d = dest;

  s += x * 3;

  for (i = 0; i < width; i++) {
    d[i * 4 + 0] = 0xff;
    d[i * 4 + 1] = s[i * 3 + 0];
    d[i * 4 + 2] = s[i * 3 + 1];
    d[i * 4 + 3] = s[i * 3 + 2];
  }
}

static void
pack_v308 (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  int i;
  guint8 *restrict d = GET_LINE (y);
  const guint8 *restrict s = src;

  for (i = 0; i < width; i++) {
    d[i * 3 + 0] = s[i * 4 + 1];
    d[i * 3 + 1] = s[i * 4 + 2];
    d[i * 3 + 2] = s[i * 4 + 3];
  }
}

#define PACK_IYU2 GST_VIDEO_FORMAT_AYUV, unpack_IYU2, 1, pack_IYU2
static void
unpack_IYU2 (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  int i;
  const guint8 *restrict s = GET_LINE (y);
  guint8 *restrict d = dest;

  s += x * 3;

  for (i = 0; i < width; i++) {
    d[i * 4 + 0] = 0xff;
    d[i * 4 + 1] = s[i * 3 + 1];
    d[i * 4 + 2] = s[i * 3 + 0];
    d[i * 4 + 3] = s[i * 3 + 2];
  }
}

static void
pack_IYU2 (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  int i;
  guint8 *restrict d = GET_LINE (y);
  const guint8 *restrict s = src;

  for (i = 0; i < width; i++) {
    d[i * 3 + 0] = s[i * 4 + 2];
    d[i * 3 + 1] = s[i * 4 + 1];
    d[i * 3 + 2] = s[i * 4 + 3];
  }
}

#define PACK_AYUV GST_VIDEO_FORMAT_AYUV, unpack_copy4, 1, pack_copy4
#define PACK_ARGB GST_VIDEO_FORMAT_ARGB, unpack_copy4, 1, pack_copy4
static void
unpack_copy4 (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  const guint8 *restrict s = GET_LINE (y);

  s += x * 4;

  memcpy (dest, s, width * 4);
}

static void
pack_copy4 (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  guint8 *restrict d = GET_LINE (y);

  memcpy (d, src, width * 4);
}

#define PACK_v210 GST_VIDEO_FORMAT_AYUV64, unpack_v210, 1, pack_v210
static void
unpack_v210 (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  int i;
  const guint8 *restrict s = GET_LINE (y);
  guint16 *restrict d = dest;
  guint32 a0, a1, a2, a3;
  guint16 y0, y1, y2, y3, y4, y5;
  guint16 u0, u2, u4;
  guint16 v0, v2, v4;

  if (x != 0)
    GST_FIXME ("Horizontal offsets are not supported for v210");

  for (i = 0; i < width; i += 6) {
    a0 = GST_READ_UINT32_LE (s + (i / 6) * 16 + 0);
    a1 = GST_READ_UINT32_LE (s + (i / 6) * 16 + 4);
    a2 = GST_READ_UINT32_LE (s + (i / 6) * 16 + 8);
    a3 = GST_READ_UINT32_LE (s + (i / 6) * 16 + 12);

    u0 = ((a0 >> 0) & 0x3ff) << 6;
    y0 = ((a0 >> 10) & 0x3ff) << 6;
    v0 = ((a0 >> 20) & 0x3ff) << 6;
    y1 = ((a1 >> 0) & 0x3ff) << 6;

    u2 = ((a1 >> 10) & 0x3ff) << 6;
    y2 = ((a1 >> 20) & 0x3ff) << 6;
    v2 = ((a2 >> 0) & 0x3ff) << 6;
    y3 = ((a2 >> 10) & 0x3ff) << 6;

    u4 = ((a2 >> 20) & 0x3ff) << 6;
    y4 = ((a3 >> 0) & 0x3ff) << 6;
    v4 = ((a3 >> 10) & 0x3ff) << 6;
    y5 = ((a3 >> 20) & 0x3ff) << 6;

    if (!(flags & GST_VIDEO_PACK_FLAG_TRUNCATE_RANGE)) {
      y0 |= (y0 >> 10);
      y1 |= (y1 >> 10);
      u0 |= (u0 >> 10);
      v0 |= (v0 >> 10);

      y2 |= (y2 >> 10);
      y3 |= (y3 >> 10);
      u2 |= (u2 >> 10);
      v2 |= (v2 >> 10);

      y4 |= (y4 >> 10);
      y5 |= (y5 >> 10);
      u4 |= (u4 >> 10);
      v4 |= (v4 >> 10);
    }

    d[4 * (i + 0) + 0] = 0xffff;
    d[4 * (i + 0) + 1] = y0;
    d[4 * (i + 0) + 2] = u0;
    d[4 * (i + 0) + 3] = v0;

    if (i < width - 1) {
      d[4 * (i + 1) + 0] = 0xffff;
      d[4 * (i + 1) + 1] = y1;
      d[4 * (i + 1) + 2] = u0;
      d[4 * (i + 1) + 3] = v0;
    }
    if (i < width - 2) {
      d[4 * (i + 2) + 0] = 0xffff;
      d[4 * (i + 2) + 1] = y2;
      d[4 * (i + 2) + 2] = u2;
      d[4 * (i + 2) + 3] = v2;
    }
    if (i < width - 3) {
      d[4 * (i + 3) + 0] = 0xffff;
      d[4 * (i + 3) + 1] = y3;
      d[4 * (i + 3) + 2] = u2;
      d[4 * (i + 3) + 3] = v2;
    }
    if (i < width - 4) {
      d[4 * (i + 4) + 0] = 0xffff;
      d[4 * (i + 4) + 1] = y4;
      d[4 * (i + 4) + 2] = u4;
      d[4 * (i + 4) + 3] = v4;
    }
    if (i < width - 5) {
      d[4 * (i + 5) + 0] = 0xffff;
      d[4 * (i + 5) + 1] = y5;
      d[4 * (i + 5) + 2] = u4;
      d[4 * (i + 5) + 3] = v4;
    }
  }
}

static void
pack_v210 (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  int i;
  guint8 *restrict d = GET_LINE (y);
  const guint16 *restrict s = src;
  guint32 a0, a1, a2, a3;
  guint16 y0, y1, y2, y3, y4, y5;
  guint16 u0, u1, u2;
  guint16 v0, v1, v2;

  for (i = 0; i < width; i += 6) {
    y1 = y2 = y3 = y4 = y5 = 0;
    u1 = u2 = v1 = v2 = 0;

    y0 = s[4 * (i + 0) + 1] >> 6;
    u0 = s[4 * (i + 0) + 2] >> 6;
    v0 = s[4 * (i + 0) + 3] >> 6;

    if (i < width - 1) {
      y1 = s[4 * (i + 1) + 1] >> 6;
    }
    if (i < width - 2) {
      y2 = s[4 * (i + 2) + 1] >> 6;
      u1 = s[4 * (i + 2) + 2] >> 6;
      v1 = s[4 * (i + 2) + 3] >> 6;
    }
    if (i < width - 3) {
      y3 = s[4 * (i + 3) + 1] >> 6;
    }
    if (i < width - 4) {
      y4 = s[4 * (i + 4) + 1] >> 6;
      u2 = s[4 * (i + 4) + 2] >> 6;
      v2 = s[4 * (i + 4) + 3] >> 6;
    }
    if (i < width - 5) {
      y5 = s[4 * (i + 5) + 1] >> 6;
    }

    a0 = u0 | (y0 << 10) | (v0 << 20);
    a1 = y1 | (u1 << 10) | (y2 << 20);
    a2 = v1 | (y3 << 10) | (u2 << 20);
    a3 = y4 | (v2 << 10) | (y5 << 20);

    GST_WRITE_UINT32_LE (d + (i / 6) * 16 + 0, a0);
    GST_WRITE_UINT32_LE (d + (i / 6) * 16 + 4, a1);
    GST_WRITE_UINT32_LE (d + (i / 6) * 16 + 8, a2);
    GST_WRITE_UINT32_LE (d + (i / 6) * 16 + 12, a3);
  }
}

#define PACK_v216 GST_VIDEO_FORMAT_AYUV64, unpack_v216, 1, pack_v216
static void
unpack_v216 (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  int i;
  const guint8 *restrict s = GET_LINE (y);
  guint16 *restrict d = dest;

  s += (x & ~1) << 2;
  if (x & 1) {
    d[0] = 0xffff;
    d[1] = GST_READ_UINT16_LE (s + 6);
    d[2] = GST_READ_UINT16_LE (s + 0);
    d[3] = GST_READ_UINT16_LE (s + 4);
    s += 8;
    d += 4;
    width--;
  }

  for (i = 0; i < width; i++) {
    d[i * 4 + 0] = 0xffff;
    d[i * 4 + 1] = GST_READ_UINT16_LE (s + i * 4 + 2);
    d[i * 4 + 2] = GST_READ_UINT16_LE (s + (i >> 1) * 8 + 0);
    d[i * 4 + 3] = GST_READ_UINT16_LE (s + (i >> 1) * 8 + 4);
  }
}

static void
pack_v216 (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  int i;
  guint8 *restrict d = GET_LINE (y);
  const guint16 *restrict s = src;

  for (i = 0; i < width - 1; i += 2) {
    GST_WRITE_UINT16_LE (d + i * 4 + 0, s[(i + 0) * 4 + 2]);
    GST_WRITE_UINT16_LE (d + i * 4 + 2, s[(i + 0) * 4 + 1]);
    GST_WRITE_UINT16_LE (d + i * 4 + 4, s[(i + 0) * 4 + 3]);
    GST_WRITE_UINT16_LE (d + i * 4 + 6, s[(i + 1) * 4 + 1]);
  }
  if (i == width - 1) {
    GST_WRITE_UINT16_LE (d + i * 4 + 0, s[i * 4 + 2]);
    GST_WRITE_UINT16_LE (d + i * 4 + 2, s[i * 4 + 1]);
    GST_WRITE_UINT16_LE (d + i * 4 + 4, s[i * 4 + 3]);
    GST_WRITE_UINT16_LE (d + i * 4 + 6, s[i * 4 + 1]);
  }
}

#define PACK_Y210 GST_VIDEO_FORMAT_AYUV64, unpack_Y210, 1, pack_Y210
static void
unpack_Y210 (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  int i;
  const guint8 *restrict s = GET_LINE (y);
  guint16 *restrict d = dest;
  guint Y0, Y1, U, V;

  s += GST_ROUND_DOWN_2 (x) * 4;

  if (x & 1) {
    Y1 = GST_READ_UINT16_LE (s + 4);
    U = GST_READ_UINT16_LE (s + 2);
    V = GST_READ_UINT16_LE (s + 6);

    if (!(flags & GST_VIDEO_PACK_FLAG_TRUNCATE_RANGE)) {
      Y1 |= (Y1 >> 10);
      U |= (U >> 10);
      V |= (V >> 10);
    }

    d[0] = 0xffff;
    d[1] = Y1;
    d[2] = U;
    d[3] = V;
    s += 8;
    d += 4;
    width--;
  }

  for (i = 0; i < width / 2; i++) {
    Y0 = GST_READ_UINT16_LE (s + i * 8 + 0);
    U = GST_READ_UINT16_LE (s + i * 8 + 2);
    V = GST_READ_UINT16_LE (s + i * 8 + 6);
    Y1 = GST_READ_UINT16_LE (s + i * 8 + 4);

    if (!(flags & GST_VIDEO_PACK_FLAG_TRUNCATE_RANGE)) {
      Y0 |= (Y0 >> 10);
      U |= (U >> 10);
      V |= (V >> 10);
    }

    d[i * 8 + 0] = 0xffff;
    d[i * 8 + 1] = Y0;
    d[i * 8 + 2] = U;
    d[i * 8 + 3] = V;

    d[i * 8 + 4] = 0xffff;
    d[i * 8 + 5] = Y1;
    d[i * 8 + 6] = U;
    d[i * 8 + 7] = V;
  }

  if (width & 1) {
    i = width - 1;

    Y0 = GST_READ_UINT16_LE (s + i * 4 + 0);
    U = GST_READ_UINT16_LE (s + i * 4 + 2);
    V = GST_READ_UINT16_LE (s + i * 4 + 6);

    if (!(flags & GST_VIDEO_PACK_FLAG_TRUNCATE_RANGE)) {
      Y0 |= (Y0 >> 10);
      U |= (U >> 10);
      V |= (V >> 10);
    }

    d[i * 4 + 0] = 0xffff;
    d[i * 4 + 1] = Y0;
    d[i * 4 + 2] = U;
    d[i * 4 + 3] = V;
  }
}

static void
pack_Y210 (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  int i;
  guint16 Y0, Y1, U, V;
  guint8 *restrict d = GET_LINE (y);
  const guint16 *restrict s = src;

  for (i = 0; i < width; i += 2) {
    Y0 = s[i * 4 + 1] & 0xffc0;
    U = s[i * 4 + 2] & 0xffc0;
    V = s[i * 4 + 3] & 0xffc0;
    if (i == width - 1)
      Y1 = s[i * 4 + 1] & 0xffc0;
    else
      Y1 = s[(i + 1) * 4 + 1] & 0xffc0;

    GST_WRITE_UINT16_LE (d + i * 4 + 0, Y0);
    GST_WRITE_UINT16_LE (d + i * 4 + 2, U);
    GST_WRITE_UINT16_LE (d + i * 4 + 4, Y1);
    GST_WRITE_UINT16_LE (d + i * 4 + 6, V);
  }
}

#define PACK_Y410 GST_VIDEO_FORMAT_AYUV64, unpack_Y410, 1, pack_Y410
static void
unpack_Y410 (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  int i;
  const guint8 *restrict s = GET_LINE (y);
  guint16 *restrict d = dest;
  guint32 AVYU;
  guint16 A, Y, U, V;

  s += x * 4;

  for (i = 0; i < width; i++) {
    AVYU = GST_READ_UINT32_LE (s + 4 * i);

    U = ((AVYU >> 0) & 0x3ff) << 6;
    Y = ((AVYU >> 10) & 0x3ff) << 6;
    V = ((AVYU >> 20) & 0x3ff) << 6;
    A = ((AVYU >> 30) & 0x03) << 14;

    if (!(flags & GST_VIDEO_PACK_FLAG_TRUNCATE_RANGE)) {
      U |= (U >> 10);
      Y |= (Y >> 10);
      V |= (V >> 10);
      A |= (A >> 10);
    }

    d[4 * i + 0] = A;
    d[4 * i + 1] = Y;
    d[4 * i + 2] = U;
    d[4 * i + 3] = V;
  }
}

static void
pack_Y410 (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  int i;
  guint32 *restrict d = GET_LINE (y);
  const guint16 *restrict s = src;
  guint32 AVYU;
  guint16 A, Y, U, V;

  for (i = 0; i < width; i++) {
    A = s[4 * i] & 0xc000;
    Y = s[4 * i + 1] & 0xffc0;
    U = s[4 * i + 2] & 0xffc0;
    V = s[4 * i + 3] & 0xffc0;

    AVYU = (U >> 6) | (Y << 4) | (V << 14) | (A << 16);

    GST_WRITE_UINT32_LE (d + i, AVYU);
  }
}

#define PACK_Y41B GST_VIDEO_FORMAT_AYUV, unpack_Y41B, 1, pack_Y41B
static void
unpack_Y41B (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  const guint8 *restrict sy = GET_Y_LINE (y);
  const guint8 *restrict su = GET_U_LINE (y);
  const guint8 *restrict sv = GET_V_LINE (y);
  guint8 *restrict d = dest;

  sy += x;
  su += x >> 2;
  sv += x >> 2;

  if (x & 3) {
    for (; x & 3; x++) {
      d[0] = 0xff;
      d[1] = *sy++;
      d[2] = *su;
      d[3] = *sv;
      width--;
      d += 4;
    }
    su++;
    sy++;
  }

  if (IS_ALIGNED (d, 8))
    video_orc_unpack_YUV9 (d, sy, su, sv, width / 2);
  else {
    gint i;
    for (i = 0; i < width / 2; i++) {
      d[i * 8 + 0] = 0xff;
      d[i * 8 + 1] = sy[i * 2 + 0];
      d[i * 8 + 2] = su[i >> 1];
      d[i * 8 + 3] = sv[i >> 1];
      d[i * 8 + 4] = 0xff;
      d[i * 8 + 5] = sy[i * 2 + 1];
      d[i * 8 + 6] = su[i >> 1];
      d[i * 8 + 7] = sv[i >> 1];
    }
  }

  if (width & 1) {
    gint i = width - 1;

    d[i * 4 + 0] = 0xff;
    d[i * 4 + 1] = sy[i];
    d[i * 4 + 2] = su[i >> 2];
    d[i * 4 + 3] = sv[i >> 2];
  }
}

static void
pack_Y41B (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  int i;
  guint8 *restrict dy = GET_Y_LINE (y);
  guint8 *restrict du = GET_U_LINE (y);
  guint8 *restrict dv = GET_V_LINE (y);
  const guint8 *restrict s = src;

  for (i = 0; i < width - 3; i += 4) {
    dy[i] = s[i * 4 + 1];
    dy[i + 1] = s[i * 4 + 5];
    dy[i + 2] = s[i * 4 + 9];
    dy[i + 3] = s[i * 4 + 13];

    du[i >> 2] = s[i * 4 + 2];
    dv[i >> 2] = s[i * 4 + 3];
  }
  if (i < width) {
    dy[i] = s[i * 4 + 1];
    du[i >> 2] = s[i * 4 + 2];
    dv[i >> 2] = s[i * 4 + 3];
    if (i < width - 1)
      dy[i + 1] = s[i * 4 + 5];
    if (i < width - 2)
      dy[i + 2] = s[i * 4 + 9];
  }
}

#define PACK_Y42B GST_VIDEO_FORMAT_AYUV, unpack_Y42B, 1, pack_Y42B
static void
unpack_Y42B (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  const guint8 *restrict sy = GET_Y_LINE (y);
  const guint8 *restrict su = GET_U_LINE (y);
  const guint8 *restrict sv = GET_V_LINE (y);
  guint8 *restrict d = dest;

  sy += x;
  su += x >> 1;
  sv += x >> 1;

  if (x & 1) {
    d[0] = 0xff;
    d[1] = *sy++;
    d[2] = *su++;
    d[3] = *sv++;
    width--;
    d += 4;
  }

  if (IS_ALIGNED (d, 8))
    video_orc_unpack_Y42B (d, sy, su, sv, width / 2);
  else {
    gint i;
    for (i = 0; i < width / 2; i++) {
      d[i * 8 + 0] = 0xff;
      d[i * 8 + 1] = sy[i * 2 + 0];
      d[i * 8 + 2] = su[i];
      d[i * 8 + 3] = sv[i];
      d[i * 8 + 4] = 0xff;
      d[i * 8 + 5] = sy[i * 2 + 1];
      d[i * 8 + 6] = su[i];
      d[i * 8 + 7] = sv[i];
    }
  }

  if (width & 1) {
    gint i = width - 1;

    d[i * 4 + 0] = 0xff;
    d[i * 4 + 1] = sy[i];
    d[i * 4 + 2] = su[i >> 1];
    d[i * 4 + 3] = sv[i >> 1];
  }
}

static void
pack_Y42B (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  guint8 *restrict dy = GET_Y_LINE (y);
  guint8 *restrict du = GET_U_LINE (y);
  guint8 *restrict dv = GET_V_LINE (y);
  const guint8 *restrict s = src;

  if (IS_ALIGNED (s, 8))
    video_orc_pack_Y42B (dy, du, dv, s, width / 2);
  else {
    gint i;
    for (i = 0; i < width / 2; i++) {
      dy[i * 2 + 0] = s[i * 8 + 1];
      dy[i * 2 + 1] = s[i * 8 + 5];
      du[i] = s[i * 8 + 2];
      dv[i] = s[i * 8 + 3];
    }
  }

  if (width & 1) {
    gint i = width - 1;

    dy[i] = s[i * 4 + 1];
    du[i >> 1] = s[i * 4 + 2];
    dv[i >> 1] = s[i * 4 + 3];
  }
}

#define PACK_Y444 GST_VIDEO_FORMAT_AYUV, unpack_Y444, 1, pack_Y444
static void
unpack_Y444 (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  const guint8 *restrict sy = GET_Y_LINE (y);
  const guint8 *restrict su = GET_U_LINE (y);
  const guint8 *restrict sv = GET_V_LINE (y);

  sy += x;
  su += x;
  sv += x;

  video_orc_unpack_Y444 (dest, sy, su, sv, width);
}

static void
pack_Y444 (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  guint8 *restrict dy = GET_Y_LINE (y);
  guint8 *restrict du = GET_U_LINE (y);
  guint8 *restrict dv = GET_V_LINE (y);

  video_orc_pack_Y444 (dy, du, dv, src, width);
}

#define PACK_GBR GST_VIDEO_FORMAT_ARGB, unpack_GBR, 1, pack_GBR
static void
unpack_GBR (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  const guint8 *restrict sr = GET_R_LINE (y);
  const guint8 *restrict sg = GET_G_LINE (y);
  const guint8 *restrict sb = GET_B_LINE (y);

  sr += x;
  sg += x;
  sb += x;

  video_orc_unpack_Y444 (dest, sr, sg, sb, width);
}

static void
pack_GBR (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  guint8 *restrict dr = GET_R_LINE (y);
  guint8 *restrict dg = GET_G_LINE (y);
  guint8 *restrict db = GET_B_LINE (y);

  video_orc_pack_Y444 (dr, dg, db, src, width);
}

#define PACK_GBRA GST_VIDEO_FORMAT_ARGB, unpack_GBRA, 1, pack_GBRA
static void
unpack_GBRA (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  int i;
  const guint8 *sg = GET_G_LINE (y);
  const guint8 *sb = GET_B_LINE (y);
  const guint8 *sr = GET_R_LINE (y);
  const guint8 *sa = GET_A_LINE (y);
  guint8 *d = dest, G, B, R, A;

  sg += x;
  sb += x;
  sr += x;
  sa += x;

  for (i = 0; i < width; i++) {
    G = GST_READ_UINT8 (sg + i);
    B = GST_READ_UINT8 (sb + i);
    R = GST_READ_UINT8 (sr + i);
    A = GST_READ_UINT8 (sa + i);

    d[i * 4 + 0] = A;
    d[i * 4 + 1] = R;
    d[i * 4 + 2] = G;
    d[i * 4 + 3] = B;
  }
}

static void
pack_GBRA (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  int i;
  guint8 *restrict dg = GET_G_LINE (y);
  guint8 *restrict db = GET_B_LINE (y);
  guint8 *restrict dr = GET_R_LINE (y);
  guint8 *restrict da = GET_A_LINE (y);
  guint8 G, B, R, A;
  const guint8 *restrict s = src;

  for (i = 0; i < width; i++) {
    G = (s[i * 4 + 2]);
    B = (s[i * 4 + 3]);
    R = (s[i * 4 + 1]);
    A = (s[i * 4 + 0]);

    GST_WRITE_UINT8 (dg + i, G);
    GST_WRITE_UINT8 (db + i, B);
    GST_WRITE_UINT8 (dr + i, R);
    GST_WRITE_UINT8 (da + i, A);
  }
}

#define PACK_GRAY8 GST_VIDEO_FORMAT_AYUV, unpack_GRAY8, 1, pack_GRAY8
static void
unpack_GRAY8 (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  const guint8 *restrict s = GET_LINE (y);

  s += x;

  video_orc_unpack_GRAY8 (dest, s, width);
}

static void
pack_GRAY8 (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  guint8 *restrict d = GET_LINE (y);

  video_orc_pack_GRAY8 (d, src, width);
}

#define PACK_GRAY16_BE GST_VIDEO_FORMAT_AYUV64, unpack_GRAY16_BE, 1, pack_GRAY16_BE
static void
unpack_GRAY16_BE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  int i;
  const guint16 *restrict s = GET_LINE (y);
  guint16 *restrict d = dest;

  s += x;

  for (i = 0; i < width; i++) {
    d[i * 4 + 0] = 0xffff;
    d[i * 4 + 1] = GST_READ_UINT16_BE (s + i);
    d[i * 4 + 2] = 0x8000;
    d[i * 4 + 3] = 0x8000;
  }
}

static void
pack_GRAY16_BE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  int i;
  guint16 *restrict d = GET_LINE (y);
  const guint16 *restrict s = src;

  for (i = 0; i < width; i++) {
    GST_WRITE_UINT16_BE (d + i, s[i * 4 + 1]);
  }
}

#define PACK_GRAY16_LE GST_VIDEO_FORMAT_AYUV64, unpack_GRAY16_LE, 1, pack_GRAY16_LE
static void
unpack_GRAY16_LE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  int i;
  const guint16 *restrict s = GET_LINE (y);
  guint16 *restrict d = dest;

  s += x;

  for (i = 0; i < width; i++) {
    d[i * 4 + 0] = 0xffff;
    d[i * 4 + 1] = GST_READ_UINT16_LE (s + i);
    d[i * 4 + 2] = 0x8000;
    d[i * 4 + 3] = 0x8000;
  }
}

static void
pack_GRAY16_LE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  int i;
  guint16 *restrict d = GET_LINE (y);
  const guint16 *restrict s = src;

  for (i = 0; i < width; i++) {
    GST_WRITE_UINT16_LE (d + i, s[i * 4 + 1]);
  }
}

#define PACK_RGB16 GST_VIDEO_FORMAT_ARGB, unpack_RGB16, 1, pack_RGB16
static void
unpack_RGB16 (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  const guint16 *restrict s = GET_LINE (y);

  if (flags & GST_VIDEO_PACK_FLAG_TRUNCATE_RANGE)
    video_orc_unpack_RGB16_trunc (dest, s + x, width);
  else
    video_orc_unpack_RGB16 (dest, s + x, width);
}

static void
pack_RGB16 (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  guint16 *restrict d = GET_LINE (y);

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
  video_orc_pack_RGB16_le (d, src, width);
#else
  video_orc_pack_RGB16_be (d, src, width);
#endif
}

#define PACK_BGR16 GST_VIDEO_FORMAT_ARGB, unpack_BGR16, 1, pack_BGR16
static void
unpack_BGR16 (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  const guint16 *restrict s = GET_LINE (y);

  if (flags & GST_VIDEO_PACK_FLAG_TRUNCATE_RANGE)
    video_orc_unpack_BGR16_trunc (dest, s + x, width);
  else
    video_orc_unpack_BGR16 (dest, s + x, width);
}

static void
pack_BGR16 (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  guint16 *restrict d = GET_LINE (y);

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
  video_orc_pack_BGR16_le (d, src, width);
#else
  video_orc_pack_BGR16_be (d, src, width);
#endif
}

#define PACK_RGB15 GST_VIDEO_FORMAT_ARGB, unpack_RGB15, 1, pack_RGB15
static void
unpack_RGB15 (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  const guint16 *restrict s = GET_LINE (y);

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
  if (flags & GST_VIDEO_PACK_FLAG_TRUNCATE_RANGE)
    video_orc_unpack_RGB15_le_trunc (dest, s + x, width);
  else
    video_orc_unpack_RGB15_le (dest, s + x, width);
#else
  if (flags & GST_VIDEO_PACK_FLAG_TRUNCATE_RANGE)
    video_orc_unpack_RGB15_be_trunc (dest, s + x, width);
  else
    video_orc_unpack_RGB15_be (dest, s + x, width);
#endif
}

static void
pack_RGB15 (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  guint16 *restrict d = GET_LINE (y);

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
  video_orc_pack_RGB15_le (d, src, width);
#else
  video_orc_pack_RGB15_be (d, src, width);
#endif
}

#define PACK_BGR15 GST_VIDEO_FORMAT_ARGB, unpack_BGR15, 1, pack_BGR15
static void
unpack_BGR15 (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  const guint16 *restrict s = GET_LINE (y);

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
  if (flags & GST_VIDEO_PACK_FLAG_TRUNCATE_RANGE)
    video_orc_unpack_BGR15_le_trunc (dest, s + x, width);
  else
    video_orc_unpack_BGR15_le (dest, s + x, width);
#else
  if (flags & GST_VIDEO_PACK_FLAG_TRUNCATE_RANGE)
    video_orc_unpack_BGR15_be_trunc (dest, s + x, width);
  else
    video_orc_unpack_BGR15_be (dest, s + x, width);
#endif
}

static void
pack_BGR15 (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  guint16 *restrict d = GET_LINE (y);

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
  video_orc_pack_BGR15_le (d, src, width);
#else
  video_orc_pack_BGR15_be (d, src, width);
#endif
}

#define PACK_BGRA GST_VIDEO_FORMAT_ARGB, unpack_BGRA, 1, pack_BGRA
static void
unpack_BGRA (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  const guint8 *restrict s = GET_LINE (y);

  s += x * 4;

  video_orc_unpack_BGRA (dest, s, width);
}

static void
pack_BGRA (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  guint8 *restrict d = GET_LINE (y);

  video_orc_pack_BGRA (d, src, width);
}

#define PACK_ABGR GST_VIDEO_FORMAT_ARGB, unpack_ABGR, 1, pack_ABGR
static void
unpack_ABGR (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  const guint8 *restrict s = GET_LINE (y);

  s += x * 4;

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
  video_orc_unpack_ABGR_le (dest, s, width);
#else
  video_orc_unpack_ABGR_be (dest, s, width);
#endif
}

static void
pack_ABGR (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  guint8 *restrict d = GET_LINE (y);

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
  video_orc_pack_ABGR_le (d, src, width);
#else
  video_orc_pack_ABGR_be (d, src, width);
#endif
}

#define PACK_RGBA GST_VIDEO_FORMAT_ARGB, unpack_RGBA, 1, pack_RGBA
static void
unpack_RGBA (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  const guint8 *restrict s = GET_LINE (y);

  s += x * 4;

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
  video_orc_unpack_RGBA_le (dest, s, width);
#else
  video_orc_unpack_RGBA_be (dest, s, width);
#endif
}

static void
pack_RGBA (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  guint8 *restrict d = GET_LINE (y);

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
  video_orc_pack_RGBA_le (d, src, width);
#else
  video_orc_pack_RGBA_be (d, src, width);
#endif
}

#define PACK_RGB GST_VIDEO_FORMAT_ARGB, unpack_RGB, 1, pack_RGB
static void
unpack_RGB (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  int i;
  const guint8 *restrict s = GET_LINE (y);
  guint8 *restrict d = dest;

  s += x * 3;

  for (i = 0; i < width; i++) {
    d[i * 4 + 0] = 0xff;
    d[i * 4 + 1] = s[i * 3 + 0];
    d[i * 4 + 2] = s[i * 3 + 1];
    d[i * 4 + 3] = s[i * 3 + 2];
  }
}

static void
pack_RGB (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  int i;
  guint8 *restrict d = GET_LINE (y);
  const guint8 *restrict s = src;

  for (i = 0; i < width; i++) {
    d[i * 3 + 0] = s[i * 4 + 1];
    d[i * 3 + 1] = s[i * 4 + 2];
    d[i * 3 + 2] = s[i * 4 + 3];
  }
}

#define PACK_BGR GST_VIDEO_FORMAT_ARGB, unpack_BGR, 1, pack_BGR
static void
unpack_BGR (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  int i;
  const guint8 *restrict s = GET_LINE (y);
  guint8 *restrict d = dest;

  s += x * 3;

  for (i = 0; i < width; i++) {
    d[i * 4 + 0] = 0xff;
    d[i * 4 + 1] = s[i * 3 + 2];
    d[i * 4 + 2] = s[i * 3 + 1];
    d[i * 4 + 3] = s[i * 3 + 0];
  }
}

static void
pack_BGR (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  int i;
  guint8 *restrict d = GET_LINE (y);
  const guint8 *restrict s = src;

  for (i = 0; i < width; i++) {
    d[i * 3 + 0] = s[i * 4 + 3];
    d[i * 3 + 1] = s[i * 4 + 2];
    d[i * 3 + 2] = s[i * 4 + 1];
  }
}

#define PACK_NV12 GST_VIDEO_FORMAT_AYUV, unpack_NV12, 1, pack_NV12
static void
unpack_NV12 (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  gint uv = GET_UV_420 (y, flags);
  const guint8 *restrict sy = GET_PLANE_LINE (0, y);
  const guint8 *restrict suv = GET_PLANE_LINE (1, uv);
  guint8 *restrict d = dest;

  sy += x;
  suv += (x & ~1);

  if (x & 1) {
    d[0] = 0xff;
    d[1] = *sy++;
    d[2] = suv[0];
    d[3] = suv[1];
    width--;
    d += 4;
    suv += 2;
  }

  if (IS_ALIGNED (d, 8))
    video_orc_unpack_NV12 (d, sy, suv, width / 2);
  else {
    gint i;
    for (i = 0; i < width / 2; i++) {
      d[i * 8 + 0] = 0xff;
      d[i * 8 + 1] = sy[i * 2 + 0];
      d[i * 8 + 2] = suv[i * 2 + 0];
      d[i * 8 + 3] = suv[i * 2 + 1];
      d[i * 8 + 4] = 0xff;
      d[i * 8 + 5] = sy[i * 2 + 1];
      d[i * 8 + 6] = suv[i * 2 + 0];
      d[i * 8 + 7] = suv[i * 2 + 1];
    }
  }

  if (width & 1) {
    gint i = width - 1;

    d[i * 4 + 0] = 0xff;
    d[i * 4 + 1] = sy[i];
    d[i * 4 + 2] = suv[i + 0];
    d[i * 4 + 3] = suv[i + 1];
  }
}

static void
pack_NV12 (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  gint uv = GET_UV_420 (y, flags);
  guint8 *restrict dy = GET_PLANE_LINE (0, y);
  guint8 *restrict duv = GET_PLANE_LINE (1, uv);
  const guint8 *restrict s = src;

  if (IS_CHROMA_LINE_420 (y, flags)) {
    if (IS_ALIGNED (s, 8))
      video_orc_pack_NV12 (dy, duv, s, width / 2);
    else {
      gint i;
      for (i = 0; i < width / 2; i++) {
        dy[i * 2 + 0] = s[i * 8 + 1];
        dy[i * 2 + 1] = s[i * 8 + 5];
        duv[i * 2 + 0] = s[i * 8 + 2];
        duv[i * 2 + 1] = s[i * 8 + 3];
      }
    }
    if (width & 1) {
      gint i = width - 1;

      dy[i] = s[i * 4 + 1];
      duv[i + 0] = s[i * 4 + 2];
      duv[i + 1] = s[i * 4 + 3];
    }
  } else
    video_orc_pack_Y (dy, s, width);
}

#define PACK_NV21 GST_VIDEO_FORMAT_AYUV, unpack_NV21, 1, pack_NV21
static void
unpack_NV21 (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  gint uv = GET_UV_420 (y, flags);
  const guint8 *restrict sy = GET_PLANE_LINE (0, y);
  const guint8 *restrict suv = GET_PLANE_LINE (1, uv);
  guint8 *restrict d = dest;

  sy += x;
  suv += (x & ~1);

  if (x & 1) {
    d[0] = 0xff;
    d[1] = *sy++;
    d[2] = suv[1];
    d[3] = suv[0];
    width--;
    d += 4;
    suv += 2;
  }

  if (IS_ALIGNED (d, 8))
    video_orc_unpack_NV21 (d, sy, suv, width / 2);
  else {
    gint i;
    for (i = 0; i < width / 2; i++) {
      d[i * 8 + 0] = 0xff;
      d[i * 8 + 1] = sy[i * 2 + 0];
      d[i * 8 + 2] = suv[i * 2 + 1];
      d[i * 8 + 3] = suv[i * 2 + 0];
      d[i * 8 + 4] = 0xff;
      d[i * 8 + 5] = sy[i * 2 + 1];
      d[i * 8 + 6] = suv[i * 2 + 1];
      d[i * 8 + 7] = suv[i * 2 + 0];
    }
  }

  if (width & 1) {
    gint i = width - 1;

    d[i * 4 + 0] = 0xff;
    d[i * 4 + 1] = sy[i];
    d[i * 4 + 2] = suv[i + 1];
    d[i * 4 + 3] = suv[i + 0];
  }
}

#define PACK_AV12 GST_VIDEO_FORMAT_AYUV, unpack_AV12, 1, pack_AV12
static void
unpack_AV12 (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  gint uv = GET_UV_420 (y, flags);
  const guint8 *restrict sy = GET_PLANE_LINE (0, y);
  const guint8 *restrict suv = GET_PLANE_LINE (1, uv);
  const guint8 *restrict sa = GET_PLANE_LINE (2, y);    /* a is for 'alpha' */
  guint8 *restrict d = dest;

  sy += x;
  sa += x;
  suv += (x & ~1);

  if (x & 1) {
    d[0] = *sa++;
    d[1] = *sy++;
    d[2] = suv[0];
    d[3] = suv[1];
    width--;
    d += 4;
    suv += 2;
  }

  if (IS_ALIGNED (d, 8)) {
    video_orc_unpack_AV12 (d, sy, suv, sa, width / 2);
  } else {
    gint i;
    for (i = 0; i < width / 2; i++) {
      d[i * 8 + 0] = sa[i * 2 + 0];
      d[i * 8 + 1] = sy[i * 2 + 0];
      d[i * 8 + 2] = suv[i * 2 + 0];
      d[i * 8 + 3] = suv[i * 2 + 1];
      d[i * 8 + 4] = sa[i * 2 + 1];
      d[i * 8 + 5] = sy[i * 2 + 1];
      d[i * 8 + 6] = suv[i * 2 + 0];
      d[i * 8 + 7] = suv[i * 2 + 1];
    }
  }

  if (width & 1) {
    gint i = width - 1;

    d[i * 4 + 0] = sa[i];
    d[i * 4 + 1] = sy[i];
    d[i * 4 + 2] = suv[i + 0];
    d[i * 4 + 3] = suv[i + 1];
  }
}

static void
pack_AV12 (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  gint uv = GET_UV_420 (y, flags);
  guint8 *restrict dy = GET_PLANE_LINE (0, y);
  guint8 *restrict duv = GET_PLANE_LINE (1, uv);
  guint8 *restrict da = GET_PLANE_LINE (2, y);  /* a is for 'alpha' */
  const guint8 *restrict s = src;

  if (IS_CHROMA_LINE_420 (y, flags)) {
    if (IS_ALIGNED (s, 8)) {
      video_orc_pack_AV12 (dy, duv, da, s, width / 2);
    } else {
      gint i;
      for (i = 0; i < width / 2; i++) {
        /* AYUV_AYUV: alpha is on bytes 0 and 4 */
        da[i * 2 + 0] = s[i * 8 + 0];
        da[i * 2 + 1] = s[i * 8 + 4];
        dy[i * 2 + 0] = s[i * 8 + 1];
        dy[i * 2 + 1] = s[i * 8 + 5];
        duv[i * 2 + 0] = s[i * 8 + 2];
        duv[i * 2 + 1] = s[i * 8 + 3];
      }
    }
    if (width & 1) {
      gint i = width - 1;

      da[i] = s[i * 4 + 0];     /* AYUV: alpha is byte 0 */
      dy[i] = s[i * 4 + 1];
      duv[i + 0] = s[i * 4 + 2];
      duv[i + 1] = s[i * 4 + 3];
    }
  } else {
    video_orc_pack_YA (dy, da, s, width);
  }
}

static void
pack_NV21 (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  gint uv = GET_UV_420 (y, flags);
  guint8 *restrict dy = GET_PLANE_LINE (0, y);
  guint8 *restrict duv = GET_PLANE_LINE (1, uv);
  const guint8 *restrict s = src;

  if (IS_CHROMA_LINE_420 (y, flags)) {
    if (IS_ALIGNED (s, 8))
      video_orc_pack_NV21 (dy, duv, s, width / 2);
    else {
      gint i;
      for (i = 0; i < width / 2; i++) {
        dy[i * 2 + 0] = s[i * 8 + 1];
        dy[i * 2 + 1] = s[i * 8 + 5];
        duv[i * 2 + 0] = s[i * 8 + 3];
        duv[i * 2 + 1] = s[i * 8 + 2];
      }
    }
    if (width & 1) {
      gint i = width - 1;

      dy[i] = s[i * 4 + 1];
      duv[i + 0] = s[i * 4 + 3];
      duv[i + 1] = s[i * 4 + 2];
    }
  } else
    video_orc_pack_Y (dy, s, width);
}

#define PACK_NV16 GST_VIDEO_FORMAT_AYUV, unpack_NV16, 1, pack_NV16
static void
unpack_NV16 (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  const guint8 *restrict sy = GET_PLANE_LINE (0, y);
  const guint8 *restrict suv = GET_PLANE_LINE (1, y);
  guint8 *restrict d = dest;

  sy += x;
  suv += (x & ~1);

  if (x & 1) {
    d[0] = 0xff;
    d[1] = *sy++;
    d[2] = suv[0];
    d[3] = suv[1];
    width--;
    d += 4;
    suv += 2;
  }

  if (IS_ALIGNED (d, 8))
    video_orc_unpack_NV12 (d, sy, suv, width / 2);
  else {
    gint i;
    for (i = 0; i < width / 2; i++) {
      d[i * 8 + 0] = 0xff;
      d[i * 8 + 1] = sy[i * 2 + 0];
      d[i * 8 + 2] = suv[i * 2 + 0];
      d[i * 8 + 3] = suv[i * 2 + 1];
      d[i * 8 + 4] = 0xff;
      d[i * 8 + 5] = sy[i * 2 + 1];
      d[i * 8 + 6] = suv[i * 2 + 0];
      d[i * 8 + 7] = suv[i * 2 + 1];
    }
  }

  if (width & 1) {
    gint i = width - 1;

    d[i * 4 + 0] = 0xff;
    d[i * 4 + 1] = sy[i];
    d[i * 4 + 2] = suv[i + 0];
    d[i * 4 + 3] = suv[i + 1];
  }
}

static void
pack_NV16 (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  guint8 *restrict dy = GET_PLANE_LINE (0, y);
  guint8 *restrict duv = GET_PLANE_LINE (1, y);
  const guint8 *restrict s = src;

  if (IS_ALIGNED (s, 8))
    video_orc_pack_NV12 (dy, duv, s, width / 2);
  else {
    gint i;
    for (i = 0; i < width / 2; i++) {
      dy[i * 2 + 0] = s[i * 8 + 1];
      dy[i * 2 + 1] = s[i * 8 + 5];
      duv[i * 2 + 0] = s[i * 8 + 2];
      duv[i * 2 + 1] = s[i * 8 + 3];
    }
  }

  if (width & 1) {
    gint i = width - 1;

    dy[i] = s[i * 4 + 1];
    duv[i + 0] = s[i * 4 + 2];
    duv[i + 1] = s[i * 4 + 3];
  }
}

#define PACK_NV61 GST_VIDEO_FORMAT_AYUV, unpack_NV61, 1, pack_NV61
static void
unpack_NV61 (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  const guint8 *restrict sy = GET_PLANE_LINE (0, y);
  const guint8 *restrict svu = GET_PLANE_LINE (1, y);
  guint8 *restrict d = dest;

  sy += x;
  svu += (x & ~1);

  if (x & 1) {
    d[0] = 0xff;
    d[1] = *sy++;
    d[2] = svu[1];
    d[3] = svu[0];
    width--;
    d += 4;
    svu += 2;
  }

  if (IS_ALIGNED (d, 8)) {
    video_orc_unpack_NV21 (d, sy, svu, width / 2);
  } else {
    gint i;

    for (i = 0; i < width / 2; i++) {
      d[i * 8 + 0] = 0xff;
      d[i * 8 + 1] = sy[i * 2 + 0];
      d[i * 8 + 2] = svu[i * 2 + 1];
      d[i * 8 + 3] = svu[i * 2 + 0];
      d[i * 8 + 4] = 0xff;
      d[i * 8 + 5] = sy[i * 2 + 1];
      d[i * 8 + 6] = svu[i * 2 + 1];
      d[i * 8 + 7] = svu[i * 2 + 0];
    }
  }

  if (width & 1) {
    gint i = width - 1;

    d[i * 4 + 0] = 0xff;
    d[i * 4 + 1] = sy[i];
    d[i * 4 + 2] = svu[i + 1];
    d[i * 4 + 3] = svu[i + 0];
  }
}

static void
pack_NV61 (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  const guint8 *restrict s = src;
  guint8 *restrict dy = GET_PLANE_LINE (0, y);
  guint8 *restrict dvu = GET_PLANE_LINE (1, y);

  if (IS_ALIGNED (s, 8)) {
    video_orc_pack_NV21 (dy, dvu, s, width / 2);
  } else {
    gint i;

    for (i = 0; i < width / 2; i++) {
      dy[i * 2 + 0] = s[i * 8 + 1];
      dy[i * 2 + 1] = s[i * 8 + 5];
      dvu[i * 2 + 0] = s[i * 8 + 3];
      dvu[i * 2 + 1] = s[i * 8 + 2];
    }
  }

  if (width & 1) {
    gint i = width - 1;

    dy[i] = s[i * 4 + 1];
    dvu[i + 0] = s[i * 4 + 2];
    dvu[i + 1] = s[i * 4 + 3];
  }
}

#define PACK_NV24 GST_VIDEO_FORMAT_AYUV, unpack_NV24, 1, pack_NV24
static void
unpack_NV24 (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  const guint8 *restrict sy = GET_PLANE_LINE (0, y);
  const guint8 *restrict suv = GET_PLANE_LINE (1, y);

  sy += x;
  suv += x << 1;

  video_orc_unpack_NV24 (dest, sy, suv, width);
}

static void
pack_NV24 (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  guint8 *restrict dy = GET_PLANE_LINE (0, y);
  guint8 *restrict duv = GET_PLANE_LINE (1, y);

  video_orc_pack_NV24 (dy, duv, src, width);
}

#define PACK_UYVP GST_VIDEO_FORMAT_AYUV64, unpack_UYVP, 1, pack_UYVP
static void
unpack_UYVP (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  int i;
  const guint8 *restrict s = GET_LINE (y);
  guint16 *restrict d = dest;

  /* FIXME */
  s += x << 1;

  for (i = 0; i < width; i += 2) {
    guint16 y0, y1;
    guint16 u0;
    guint16 v0;

    u0 = ((s[(i / 2) * 5 + 0] << 2) | (s[(i / 2) * 5 + 1] >> 6)) << 6;
    y0 = (((s[(i / 2) * 5 + 1] & 0x3f) << 4) | (s[(i / 2) * 5 + 2] >> 4)) << 6;
    v0 = (((s[(i / 2) * 5 + 2] & 0x0f) << 6) | (s[(i / 2) * 5 + 3] >> 2)) << 6;
    y1 = (((s[(i / 2) * 5 + 3] & 0x03) << 8) | s[(i / 2) * 5 + 4]) << 6;

    if (!(flags & GST_VIDEO_PACK_FLAG_TRUNCATE_RANGE)) {
      y0 |= (y0 >> 10);
      y1 |= (y1 >> 10);
      u0 |= (u0 >> 10);
      v0 |= (v0 >> 10);
    }

    d[i * 4 + 0] = 0xffff;
    d[i * 4 + 1] = y0;
    d[i * 4 + 2] = u0;
    d[i * 4 + 3] = v0;

    if (i < width - 1) {
      d[i * 4 + 4] = 0xffff;
      d[i * 4 + 5] = y1;
      d[i * 4 + 6] = u0;
      d[i * 4 + 7] = v0;
    }
  }
}

static void
pack_UYVP (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  int i;
  guint8 *restrict d = GET_LINE (y);
  const guint16 *restrict s = src;

  for (i = 0; i < width; i += 2) {
    guint16 y0, y1;
    guint16 u0;
    guint16 v0;

    y0 = s[4 * (i + 0) + 1];
    if (i < width - 1)
      y1 = s[4 * (i + 1) + 1];
    else
      y1 = y0;

    u0 = s[4 * (i + 0) + 2];
    v0 = s[4 * (i + 0) + 3];

    d[(i / 2) * 5 + 0] = u0 >> 8;
    d[(i / 2) * 5 + 1] = (u0 & 0xc0) | y0 >> 10;
    d[(i / 2) * 5 + 2] = ((y0 & 0x3c0) >> 2) | (v0 >> 12);
    d[(i / 2) * 5 + 3] = ((v0 & 0xfc0) >> 4) | (y1 >> 14);
    d[(i / 2) * 5 + 4] = (y1 >> 6);
  }
}

#define PACK_A420 GST_VIDEO_FORMAT_AYUV, unpack_A420, 1, pack_A420
static void
unpack_A420 (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  gint uv = GET_UV_420 (y, flags);
  const guint8 *restrict sy = GET_Y_LINE (y);
  const guint8 *restrict su = GET_U_LINE (uv);
  const guint8 *restrict sv = GET_V_LINE (uv);
  const guint8 *restrict sa = GET_A_LINE (y);
  guint8 *restrict d = dest;

  sy += x;
  su += x >> 1;
  sv += x >> 1;
  sa += x;

  if (x & 1) {
    d[0] = *sa++;
    d[1] = *sy++;
    d[2] = *su++;
    d[3] = *sv++;
    width--;
    d += 4;
  }
  video_orc_unpack_A420 (d, sy, su, sv, sa, width);
}

static void
pack_A420 (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  gint uv = GET_UV_420 (y, flags);
  guint8 *restrict dy = GET_Y_LINE (y);
  guint8 *restrict du = GET_U_LINE (uv);
  guint8 *restrict dv = GET_V_LINE (uv);
  guint8 *restrict da = GET_A_LINE (y);
  const guint8 *restrict s = src;

  if (IS_CHROMA_LINE_420 (y, flags)) {
    if (IS_ALIGNED (s, 8))
      video_orc_pack_A420 (dy, du, dv, da, s, width / 2);
    else {
      gint i;
      for (i = 0; i < width / 2; i++) {
        da[i * 2 + 0] = s[i * 8 + 0];
        dy[i * 2 + 0] = s[i * 8 + 1];
        da[i * 2 + 1] = s[i * 8 + 4];
        dy[i * 2 + 1] = s[i * 8 + 5];
        du[i] = s[i * 8 + 2];
        dv[i] = s[i * 8 + 3];
      }
    }

    if (width & 1) {
      gint i = width - 1;

      da[i] = s[i * 4 + 0];
      dy[i] = s[i * 4 + 1];
      du[i >> 1] = s[i * 4 + 2];
      dv[i >> 1] = s[i * 4 + 3];
    }
  } else
    video_orc_pack_AY (dy, da, s, width);
}

#define PACK_RGB8P GST_VIDEO_FORMAT_ARGB, unpack_RGB8P, 1, pack_RGB8P
static void
unpack_RGB8P (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  int i;
  const guint8 *restrict s = GET_LINE (y);
  const guint32 *restrict p = data[1];
  guint8 *restrict d = dest;

  s += x;

  for (i = 0; i < width; i++) {
    guint32 v = p[s[i]];
    d[i * 4 + 0] = (v >> 24) & 0xff;
    d[i * 4 + 1] = (v >> 16) & 0xff;
    d[i * 4 + 2] = (v >> 8) & 0xff;
    d[i * 4 + 3] = (v) & 0xff;
  }
}

static const guint32 std_palette_RGB8P[] = {
  0xff000000, 0xff000033, 0xff000066, 0xff000099, 0xff0000cc, 0xff0000ff,
  0xff003300, 0xff003333, 0xff003366, 0xff003399, 0xff0033cc, 0xff0033ff,
  0xff006600, 0xff006633, 0xff006666, 0xff006699, 0xff0066cc, 0xff0066ff,
  0xff009900, 0xff009933, 0xff009966, 0xff009999, 0xff0099cc, 0xff0099ff,
  0xff00cc00, 0xff00cc33, 0xff00cc66, 0xff00cc99, 0xff00cccc, 0xff00ccff,
  0xff00ff00, 0xff00ff33, 0xff00ff66, 0xff00ff99, 0xff00ffcc, 0xff00ffff,
  0xff330000, 0xff330033, 0xff330066, 0xff330099, 0xff3300cc, 0xff3300ff,
  0xff333300, 0xff333333, 0xff333366, 0xff333399, 0xff3333cc, 0xff3333ff,
  0xff336600, 0xff336633, 0xff336666, 0xff336699, 0xff3366cc, 0xff3366ff,
  0xff339900, 0xff339933, 0xff339966, 0xff339999, 0xff3399cc, 0xff3399ff,
  0xff33cc00, 0xff33cc33, 0xff33cc66, 0xff33cc99, 0xff33cccc, 0xff33ccff,
  0xff33ff00, 0xff33ff33, 0xff33ff66, 0xff33ff99, 0xff33ffcc, 0xff33ffff,
  0xff660000, 0xff660033, 0xff660066, 0xff660099, 0xff6600cc, 0xff6600ff,
  0xff663300, 0xff663333, 0xff663366, 0xff663399, 0xff6633cc, 0xff6633ff,
  0xff666600, 0xff666633, 0xff666666, 0xff666699, 0xff6666cc, 0xff6666ff,
  0xff669900, 0xff669933, 0xff669966, 0xff669999, 0xff6699cc, 0xff6699ff,
  0xff66cc00, 0xff66cc33, 0xff66cc66, 0xff66cc99, 0xff66cccc, 0xff66ccff,
  0xff66ff00, 0xff66ff33, 0xff66ff66, 0xff66ff99, 0xff66ffcc, 0xff66ffff,
  0xff990000, 0xff990033, 0xff990066, 0xff990099, 0xff9900cc, 0xff9900ff,
  0xff993300, 0xff993333, 0xff993366, 0xff993399, 0xff9933cc, 0xff9933ff,
  0xff996600, 0xff996633, 0xff996666, 0xff996699, 0xff9966cc, 0xff9966ff,
  0xff999900, 0xff999933, 0xff999966, 0xff999999, 0xff9999cc, 0xff9999ff,
  0xff99cc00, 0xff99cc33, 0xff99cc66, 0xff99cc99, 0xff99cccc, 0xff99ccff,
  0xff99ff00, 0xff99ff33, 0xff99ff66, 0xff99ff99, 0xff99ffcc, 0xff99ffff,
  0xffcc0000, 0xffcc0033, 0xffcc0066, 0xffcc0099, 0xffcc00cc, 0xffcc00ff,
  0xffcc3300, 0xffcc3333, 0xffcc3366, 0xffcc3399, 0xffcc33cc, 0xffcc33ff,
  0xffcc6600, 0xffcc6633, 0xffcc6666, 0xffcc6699, 0xffcc66cc, 0xffcc66ff,
  0xffcc9900, 0xffcc9933, 0xffcc9966, 0xffcc9999, 0xffcc99cc, 0xffcc99ff,
  0xffcccc00, 0xffcccc33, 0xffcccc66, 0xffcccc99, 0xffcccccc, 0xffccccff,
  0xffccff00, 0xffccff33, 0xffccff66, 0xffccff99, 0xffccffcc, 0xffccffff,
  0xffff0000, 0xffff0033, 0xffff0066, 0xffff0099, 0xffff00cc, 0xffff00ff,
  0xffff3300, 0xffff3333, 0xffff3366, 0xffff3399, 0xffff33cc, 0xffff33ff,
  0xffff6600, 0xffff6633, 0xffff6666, 0xffff6699, 0xffff66cc, 0xffff66ff,
  0xffff9900, 0xffff9933, 0xffff9966, 0xffff9999, 0xffff99cc, 0xffff99ff,
  0xffffcc00, 0xffffcc33, 0xffffcc66, 0xffffcc99, 0xffffcccc, 0xffffccff,
  0xffffff00, 0xffffff33, 0xffffff66, 0xffffff99, 0xffffffcc, 0xffffffff,
  0x00000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000,
  0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000,
  0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000,
  0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000,
  0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000,
  0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000,
  0xff000000, 0xff000000, 0xff000000, 0xff000000
};

static void
pack_RGB8P (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  int i;
  guint8 *restrict d = GET_LINE (y);
  const guint8 *restrict s = src;

  /* Use our poor man's palette, taken from ffmpegcolorspace too */
  for (i = 0; i < width; i++) {
    /* crude approximation for alpha ! */
    if (s[i * 4 + 0] < 0x80)
      d[i] = 6 * 6 * 6;
    else
      d[i] =
          ((((s[i * 4 + 1]) / 47) % 6) * 6 * 6 + (((s[i * 4 +
                          2]) / 47) % 6) * 6 + (((s[i * 4 + 3]) / 47) % 6));
  }
}

#define PACK_410 GST_VIDEO_FORMAT_AYUV, unpack_410, 1, pack_410
static void
unpack_410 (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  gint uv = GET_UV_410 (y, flags);
  const guint8 *restrict sy = GET_Y_LINE (y);
  const guint8 *restrict su = GET_U_LINE (uv);
  const guint8 *restrict sv = GET_V_LINE (uv);
  guint8 *restrict d = dest;

  sy += x;
  su += x >> 2;
  sv += x >> 2;

  if (x & 3) {
    for (; x & 3; x++) {
      d[0] = 0xff;
      d[1] = *sy++;
      d[2] = *su;
      d[3] = *sv;
      width--;
      d += 4;
    }
    su++;
    sy++;
  }

  if (IS_ALIGNED (d, 8))
    video_orc_unpack_YUV9 (d, sy, su, sv, width / 2);
  else {
    gint i;
    for (i = 0; i < width / 2; i++) {
      d[i * 8 + 0] = 0xff;
      d[i * 8 + 1] = sy[i * 2 + 0];
      d[i * 8 + 2] = su[i >> 1];
      d[i * 8 + 3] = sv[i >> 1];
      d[i * 8 + 4] = 0xff;
      d[i * 8 + 5] = sy[i * 2 + 1];
      d[i * 8 + 6] = su[i >> 1];
      d[i * 8 + 7] = sv[i >> 1];
    }
  }

  if (width & 1) {
    gint i = width - 1;

    d[i * 4 + 0] = 0xff;
    d[i * 4 + 1] = sy[i];
    d[i * 4 + 2] = su[i >> 2];
    d[i * 4 + 3] = sv[i >> 2];
  }
}

static void
pack_410 (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  int i;
  gint uv = GET_UV_410 (y, flags);
  guint8 *restrict dy = GET_Y_LINE (y);
  guint8 *restrict du = GET_U_LINE (uv);
  guint8 *restrict dv = GET_V_LINE (uv);
  const guint8 *restrict s = src;

  for (i = 0; i < width - 3; i += 4) {
    dy[i] = s[i * 4 + 1];
    dy[i + 1] = s[i * 4 + 5];
    dy[i + 2] = s[i * 4 + 9];
    dy[i + 3] = s[i * 4 + 13];
    if (IS_CHROMA_LINE_410 (y, flags)) {
      du[i >> 2] = s[i * 4 + 2];
      dv[i >> 2] = s[i * 4 + 3];
    }
  }
  if (i < width) {
    dy[i] = s[i * 4 + 1];
    if (IS_CHROMA_LINE_410 (y, flags)) {
      du[i >> 2] = s[i * 4 + 2];
      dv[i >> 2] = s[i * 4 + 3];
    }
    if (i < width - 1)
      dy[i + 1] = s[i * 4 + 5];
    if (i < width - 2)
      dy[i + 2] = s[i * 4 + 9];
  }
}

#define PACK_IYU1 GST_VIDEO_FORMAT_AYUV, unpack_IYU1, 1, pack_IYU1
static void
unpack_IYU1 (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  int i;
  const guint8 *restrict s = GET_LINE (y);
  guint8 *restrict d = dest;
  guint8 y0, y1, y2, y3;
  guint8 u0;
  guint8 v0;

  /* FIXME */
  s += x * 4;

  for (i = 0; i < width - 3; i += 4) {
    y0 = s[(i >> 2) * 6 + 1];
    y1 = s[(i >> 2) * 6 + 2];
    y2 = s[(i >> 2) * 6 + 4];
    y3 = s[(i >> 2) * 6 + 5];

    u0 = s[(i >> 2) * 6 + 0];
    v0 = s[(i >> 2) * 6 + 3];

    d[i * 4 + 0] = 0xff;
    d[i * 4 + 1] = y0;
    d[i * 4 + 2] = u0;
    d[i * 4 + 3] = v0;

    d[i * 4 + 4] = 0xff;
    d[i * 4 + 5] = y1;
    d[i * 4 + 6] = u0;
    d[i * 4 + 7] = v0;

    d[i * 4 + 8] = 0xff;
    d[i * 4 + 9] = y2;
    d[i * 4 + 10] = u0;
    d[i * 4 + 11] = v0;

    d[i * 4 + 12] = 0xff;
    d[i * 4 + 13] = y3;
    d[i * 4 + 14] = u0;
    d[i * 4 + 15] = v0;
  }
  if (i < width) {
    u0 = s[(i >> 2) * 6 + 0];
    v0 = s[(i >> 2) * 6 + 3];

    d[i * 4 + 0] = 0xff;
    d[i * 4 + 1] = s[(i >> 2) * 6 + 1];
    d[i * 4 + 2] = u0;
    d[i * 4 + 3] = v0;

    if (i < width - 1) {
      d[i * 4 + 4] = 0xff;
      d[i * 4 + 5] = s[(i >> 2) * 6 + 2];
      d[i * 4 + 6] = u0;
      d[i * 4 + 7] = v0;
    }
    if (i < width - 2) {
      d[i * 4 + 8] = 0xff;
      d[i * 4 + 9] = s[(i >> 2) * 6 + 4];
      d[i * 4 + 10] = u0;
      d[i * 4 + 11] = v0;
    }
  }
}

static void
pack_IYU1 (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  int i;
  guint8 *restrict d = GET_LINE (y);
  const guint8 *restrict s = src;

  for (i = 0; i < width - 3; i += 4) {
    d[(i >> 2) * 6 + 0] = s[i * 4 + 2];
    d[(i >> 2) * 6 + 1] = s[i * 4 + 1];
    d[(i >> 2) * 6 + 2] = s[i * 4 + 5];
    d[(i >> 2) * 6 + 3] = s[i * 4 + 3];
    d[(i >> 2) * 6 + 4] = s[i * 4 + 9];
    d[(i >> 2) * 6 + 5] = s[i * 4 + 13];
  }
  if (i < width) {
    d[(i >> 2) * 6 + 1] = s[i * 4 + 1];
    d[(i >> 2) * 6 + 0] = s[i * 4 + 2];
    d[(i >> 2) * 6 + 3] = s[i * 4 + 3];
    if (i < width - 1)
      d[(i >> 2) * 6 + 2] = s[i * 4 + 5];
    if (i < width - 2)
      d[(i >> 2) * 6 + 4] = s[i * 4 + 9];
  }
}

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
#define PACK_ARGB64_LE GST_VIDEO_FORMAT_ARGB64, unpack_copy8, 1, pack_copy8
#define PACK_ARGB64_BE GST_VIDEO_FORMAT_ARGB64, unpack_ARGB64_swap, 1, pack_ARGB64_swap
#else
#define PACK_ARGB64_LE GST_VIDEO_FORMAT_ARGB64, unpack_ARGB64_swap, 1, pack_ARGB64_swap
#define PACK_ARGB64_BE GST_VIDEO_FORMAT_ARGB64, unpack_copy8, 1, pack_copy8
#endif

static void
unpack_ARGB64_swap (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  int i;
  const guint16 *restrict s = GET_LINE (y);
  guint16 *restrict d = dest, R, G, B, A;

  s += x * 8;

  for (i = 0; i < width; i++) {
    A = GUINT16_SWAP_LE_BE (s[i * 4 + 0]);
    R = GUINT16_SWAP_LE_BE (s[i * 4 + 1]);
    G = GUINT16_SWAP_LE_BE (s[i * 4 + 2]);
    B = GUINT16_SWAP_LE_BE (s[i * 4 + 3]);

    d[i * 4 + 0] = A;
    d[i * 4 + 1] = R;
    d[i * 4 + 2] = G;
    d[i * 4 + 3] = B;
  }
}

static void
pack_ARGB64_swap (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  int i;
  guint16 *restrict d = GET_LINE (y);
  guint16 A, R, G, B;
  const guint16 *restrict s = src;

  for (i = 0; i < width; i++) {
    A = s[i * 4 + 0];
    R = s[i * 4 + 1];
    G = s[i * 4 + 2];
    B = s[i * 4 + 3];

    d[i * 4 + 0] = GUINT16_SWAP_LE_BE (A);
    d[i * 4 + 1] = GUINT16_SWAP_LE_BE (R);
    d[i * 4 + 2] = GUINT16_SWAP_LE_BE (G);
    d[i * 4 + 3] = GUINT16_SWAP_LE_BE (B);
  }
}

#define PACK_RGBA64_LE GST_VIDEO_FORMAT_ARGB64, unpack_RGBA64_LE, 1, pack_RGBA64_LE
static void
unpack_RGBA64_LE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  int i;
  const guint16 *restrict s = GET_LINE (y);
  guint16 *restrict d = dest, R, G, B, A;

  s += x * 8;

  for (i = 0; i < width; i++) {
    R = GST_READ_UINT16_LE (s + i * 4 + 0);
    G = GST_READ_UINT16_LE (s + i * 4 + 1);
    B = GST_READ_UINT16_LE (s + i * 4 + 2);
    A = GST_READ_UINT16_LE (s + i * 4 + 3);

    d[i * 4 + 0] = A;
    d[i * 4 + 1] = R;
    d[i * 4 + 2] = G;
    d[i * 4 + 3] = B;
  }
}

static void
pack_RGBA64_LE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  int i;
  guint16 *restrict d = GET_LINE (y);
  guint16 A, R, G, B;
  const guint16 *restrict s = src;

  for (i = 0; i < width; i++) {
    A = s[i * 4 + 0];
    R = s[i * 4 + 1];
    G = s[i * 4 + 2];
    B = s[i * 4 + 3];

    GST_WRITE_UINT16_LE (d + i * 4 + 0, R);
    GST_WRITE_UINT16_LE (d + i * 4 + 1, G);
    GST_WRITE_UINT16_LE (d + i * 4 + 2, B);
    GST_WRITE_UINT16_LE (d + i * 4 + 3, A);
  }
}

#define PACK_RGBA64_BE GST_VIDEO_FORMAT_ARGB64, unpack_RGBA64_BE, 1, pack_RGBA64_BE
static void
unpack_RGBA64_BE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  int i;
  const guint16 *restrict s = GET_LINE (y);
  guint16 *restrict d = dest, R, G, B, A;

  s += x * 8;

  for (i = 0; i < width; i++) {
    R = GST_READ_UINT16_BE (s + i * 4 + 0);
    G = GST_READ_UINT16_BE (s + i * 4 + 1);
    B = GST_READ_UINT16_BE (s + i * 4 + 2);
    A = GST_READ_UINT16_BE (s + i * 4 + 3);

    d[i * 4 + 0] = A;
    d[i * 4 + 1] = R;
    d[i * 4 + 2] = G;
    d[i * 4 + 3] = B;
  }
}

static void
pack_RGBA64_BE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  int i;
  guint16 *restrict d = GET_LINE (y);
  guint16 A, R, G, B;
  const guint16 *restrict s = src;

  for (i = 0; i < width; i++) {
    A = s[i * 4 + 0];
    R = s[i * 4 + 1];
    G = s[i * 4 + 2];
    B = s[i * 4 + 3];

    GST_WRITE_UINT16_BE (d + i * 4 + 0, R);
    GST_WRITE_UINT16_BE (d + i * 4 + 1, G);
    GST_WRITE_UINT16_BE (d + i * 4 + 2, B);
    GST_WRITE_UINT16_BE (d + i * 4 + 3, A);
  }
}

#define PACK_BGRA64_LE GST_VIDEO_FORMAT_ARGB64, unpack_BGRA64_LE, 1, pack_BGRA64_LE
static void
unpack_BGRA64_LE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  int i;
  const guint16 *restrict s = GET_LINE (y);
  guint16 *restrict d = dest, R, G, B, A;

  s += x * 8;

  for (i = 0; i < width; i++) {
    B = GST_READ_UINT16_LE (s + i * 4 + 0);
    G = GST_READ_UINT16_LE (s + i * 4 + 1);
    R = GST_READ_UINT16_LE (s + i * 4 + 2);
    A = GST_READ_UINT16_LE (s + i * 4 + 3);

    d[i * 4 + 0] = A;
    d[i * 4 + 1] = R;
    d[i * 4 + 2] = G;
    d[i * 4 + 3] = B;
  }
}

static void
pack_BGRA64_LE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  int i;
  guint16 *restrict d = GET_LINE (y);
  guint16 A, R, G, B;
  const guint16 *restrict s = src;

  for (i = 0; i < width; i++) {
    A = s[i * 4 + 0];
    R = s[i * 4 + 1];
    G = s[i * 4 + 2];
    B = s[i * 4 + 3];

    GST_WRITE_UINT16_LE (d + i * 4 + 0, B);
    GST_WRITE_UINT16_LE (d + i * 4 + 1, G);
    GST_WRITE_UINT16_LE (d + i * 4 + 2, R);
    GST_WRITE_UINT16_LE (d + i * 4 + 3, A);
  }
}

#define PACK_BGRA64_BE GST_VIDEO_FORMAT_ARGB64, unpack_BGRA64_BE, 1, pack_BGRA64_BE
static void
unpack_BGRA64_BE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  int i;
  const guint16 *restrict s = GET_LINE (y);
  guint16 *restrict d = dest, R, G, B, A;

  s += x * 8;

  for (i = 0; i < width; i++) {
    B = GST_READ_UINT16_BE (s + i * 4 + 0);
    G = GST_READ_UINT16_BE (s + i * 4 + 1);
    R = GST_READ_UINT16_BE (s + i * 4 + 2);
    A = GST_READ_UINT16_BE (s + i * 4 + 3);

    d[i * 4 + 0] = A;
    d[i * 4 + 1] = R;
    d[i * 4 + 2] = G;
    d[i * 4 + 3] = B;
  }
}

static void
pack_BGRA64_BE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  int i;
  guint16 *restrict d = GET_LINE (y);
  guint16 A, R, G, B;
  const guint16 *restrict s = src;

  for (i = 0; i < width; i++) {
    A = s[i * 4 + 0];
    R = s[i * 4 + 1];
    G = s[i * 4 + 2];
    B = s[i * 4 + 3];

    GST_WRITE_UINT16_BE (d + i * 4 + 0, B);
    GST_WRITE_UINT16_BE (d + i * 4 + 1, G);
    GST_WRITE_UINT16_BE (d + i * 4 + 2, R);
    GST_WRITE_UINT16_BE (d + i * 4 + 3, A);
  }
}


#define PACK_ABGR64_LE GST_VIDEO_FORMAT_ARGB64, unpack_ABGR64_LE, 1, pack_ABGR64_LE
static void
unpack_ABGR64_LE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  int i;
  const guint16 *restrict s = GET_LINE (y);
  guint16 *restrict d = dest, R, G, B, A;

  s += x * 8;

  for (i = 0; i < width; i++) {
    A = GST_READ_UINT16_LE (s + i * 4 + 0);
    B = GST_READ_UINT16_LE (s + i * 4 + 1);
    G = GST_READ_UINT16_LE (s + i * 4 + 2);
    R = GST_READ_UINT16_LE (s + i * 4 + 3);

    d[i * 4 + 0] = A;
    d[i * 4 + 1] = R;
    d[i * 4 + 2] = G;
    d[i * 4 + 3] = B;
  }
}

static void
pack_ABGR64_LE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  int i;
  guint16 *restrict d = GET_LINE (y);
  guint16 A, R, G, B;
  const guint16 *restrict s = src;

  for (i = 0; i < width; i++) {
    A = s[i * 4 + 0];
    R = s[i * 4 + 1];
    G = s[i * 4 + 2];
    B = s[i * 4 + 3];

    GST_WRITE_UINT16_LE (d + i * 4 + 0, A);
    GST_WRITE_UINT16_LE (d + i * 4 + 1, B);
    GST_WRITE_UINT16_LE (d + i * 4 + 2, G);
    GST_WRITE_UINT16_LE (d + i * 4 + 3, R);
  }
}

#define PACK_ABGR64_BE GST_VIDEO_FORMAT_ARGB64, unpack_ABGR64_BE, 1, pack_ABGR64_BE
static void
unpack_ABGR64_BE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  int i;
  const guint16 *restrict s = GET_LINE (y);
  guint16 *restrict d = dest, R, G, B, A;

  s += x * 8;

  for (i = 0; i < width; i++) {
    A = GST_READ_UINT16_BE (s + i * 4 + 0);
    B = GST_READ_UINT16_BE (s + i * 4 + 1);
    G = GST_READ_UINT16_BE (s + i * 4 + 2);
    R = GST_READ_UINT16_BE (s + i * 4 + 3);

    d[i * 4 + 0] = A;
    d[i * 4 + 1] = R;
    d[i * 4 + 2] = G;
    d[i * 4 + 3] = B;
  }
}

static void
pack_ABGR64_BE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  int i;
  guint16 *restrict d = GET_LINE (y);
  guint16 A, R, G, B;
  const guint16 *restrict s = src;

  for (i = 0; i < width; i++) {
    A = s[i * 4 + 0];
    R = s[i * 4 + 1];
    G = s[i * 4 + 2];
    B = s[i * 4 + 3];

    GST_WRITE_UINT16_BE (d + i * 4 + 0, A);
    GST_WRITE_UINT16_BE (d + i * 4 + 1, B);
    GST_WRITE_UINT16_BE (d + i * 4 + 2, G);
    GST_WRITE_UINT16_BE (d + i * 4 + 3, R);
  }
}


#define PACK_ARGB64 GST_VIDEO_FORMAT_ARGB64, unpack_copy8, 1, pack_copy8
#define PACK_AYUV64 GST_VIDEO_FORMAT_AYUV64, unpack_copy8, 1, pack_copy8
static void
unpack_copy8 (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  const guint8 *s = GET_LINE (y);

  s += x * 8;

  memcpy (dest, s, width * 8);
}

static void
pack_copy8 (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  guint8 *restrict d = GET_LINE (y);

  memcpy (d, src, width * 8);
}

#define PACK_r210 GST_VIDEO_FORMAT_ARGB64, unpack_r210, 1, pack_r210
static void
unpack_r210 (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  int i;
  const guint8 *restrict s = GET_LINE (y);
  guint16 *restrict d = dest, R, G, B;

  s += x * 4;

  for (i = 0; i < width; i++) {
    guint32 x = GST_READ_UINT32_BE (s + i * 4);

    R = ((x >> 14) & 0xffc0);
    G = ((x >> 4) & 0xffc0);
    B = ((x << 6) & 0xffc0);

    if (!(flags & GST_VIDEO_PACK_FLAG_TRUNCATE_RANGE)) {
      R |= (R >> 10);
      G |= (G >> 10);
      B |= (B >> 10);
    }

    d[i * 4 + 0] = 0xffff;
    d[i * 4 + 1] = R;
    d[i * 4 + 2] = G;
    d[i * 4 + 3] = B;
  }
}

static void
pack_r210 (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  int i;
  guint8 *restrict d = GET_LINE (y);
  const guint16 *restrict s = src;

  for (i = 0; i < width; i++) {
    guint32 x = 0;
    x |= (s[i * 4 + 1] & 0xffc0) << 14;
    x |= (s[i * 4 + 2] & 0xffc0) << 4;
    x |= (s[i * 4 + 3] & 0xffc0) >> 6;
    GST_WRITE_UINT32_BE (d + i * 4, x);
  }
}

#define PACK_GBR_10LE GST_VIDEO_FORMAT_ARGB64, unpack_GBR_10LE, 1, pack_GBR_10LE
static void
unpack_GBR_10LE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  int i;
  const guint16 *sg = GET_G_LINE (y);
  const guint16 *sb = GET_B_LINE (y);
  const guint16 *sr = GET_R_LINE (y);
  guint16 *d = dest, G, B, R;

  sg += x;
  sb += x;
  sr += x;

  for (i = 0; i < width; i++) {
    G = GST_READ_UINT16_LE (sg + i) << 6;
    B = GST_READ_UINT16_LE (sb + i) << 6;
    R = GST_READ_UINT16_LE (sr + i) << 6;

    if (!(flags & GST_VIDEO_PACK_FLAG_TRUNCATE_RANGE)) {
      R |= (R >> 10);
      G |= (G >> 10);
      B |= (B >> 10);
    }

    d[i * 4 + 0] = 0xffff;
    d[i * 4 + 1] = R;
    d[i * 4 + 2] = G;
    d[i * 4 + 3] = B;
  }
}

static void
pack_GBR_10LE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  int i;
  guint16 *restrict dg = GET_G_LINE (y);
  guint16 *restrict db = GET_B_LINE (y);
  guint16 *restrict dr = GET_R_LINE (y);
  guint16 G, B, R;
  const guint16 *restrict s = src;

  for (i = 0; i < width; i++) {
    G = (s[i * 4 + 2]) >> 6;
    B = (s[i * 4 + 3]) >> 6;
    R = (s[i * 4 + 1]) >> 6;

    GST_WRITE_UINT16_LE (dg + i, G);
    GST_WRITE_UINT16_LE (db + i, B);
    GST_WRITE_UINT16_LE (dr + i, R);
  }
}

#define PACK_GBR_10BE GST_VIDEO_FORMAT_ARGB64, unpack_GBR_10BE, 1, pack_GBR_10BE
static void
unpack_GBR_10BE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  int i;
  const guint16 *restrict sg = GET_G_LINE (y);
  const guint16 *restrict sb = GET_B_LINE (y);
  const guint16 *restrict sr = GET_R_LINE (y);
  guint16 *restrict d = dest, G, B, R;

  sg += x;
  sb += x;
  sr += x;

  for (i = 0; i < width; i++) {
    G = GST_READ_UINT16_BE (sg + i) << 6;
    B = GST_READ_UINT16_BE (sb + i) << 6;
    R = GST_READ_UINT16_BE (sr + i) << 6;

    if (!(flags & GST_VIDEO_PACK_FLAG_TRUNCATE_RANGE)) {
      R |= (R >> 10);
      G |= (G >> 10);
      B |= (B >> 10);
    }

    d[i * 4 + 0] = 0xffff;
    d[i * 4 + 1] = R;
    d[i * 4 + 2] = G;
    d[i * 4 + 3] = B;
  }
}

static void
pack_GBR_10BE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  int i;
  guint16 *restrict dg = GET_G_LINE (y);
  guint16 *restrict db = GET_B_LINE (y);
  guint16 *restrict dr = GET_R_LINE (y);
  guint16 G, B, R;
  const guint16 *restrict s = src;

  for (i = 0; i < width; i++) {
    G = s[i * 4 + 2] >> 6;
    B = s[i * 4 + 3] >> 6;
    R = s[i * 4 + 1] >> 6;

    GST_WRITE_UINT16_BE (dg + i, G);
    GST_WRITE_UINT16_BE (db + i, B);
    GST_WRITE_UINT16_BE (dr + i, R);
  }
}

#define PACK_GBRA_10LE GST_VIDEO_FORMAT_ARGB64, unpack_GBRA_10LE, 1, pack_GBRA_10LE
static void
unpack_GBRA_10LE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  int i;
  const guint16 *sg = GET_G_LINE (y);
  const guint16 *sb = GET_B_LINE (y);
  const guint16 *sr = GET_R_LINE (y);
  const guint16 *sa = GET_A_LINE (y);
  guint16 *d = dest, G, B, R, A;

  sg += x;
  sb += x;
  sr += x;
  sa += x;

  for (i = 0; i < width; i++) {
    G = GST_READ_UINT16_LE (sg + i) << 6;
    B = GST_READ_UINT16_LE (sb + i) << 6;
    R = GST_READ_UINT16_LE (sr + i) << 6;
    A = GST_READ_UINT16_LE (sa + i) << 6;

    if (!(flags & GST_VIDEO_PACK_FLAG_TRUNCATE_RANGE)) {
      R |= (R >> 10);
      G |= (G >> 10);
      B |= (B >> 10);
      A |= (A >> 10);
    }

    d[i * 4 + 0] = A;
    d[i * 4 + 1] = R;
    d[i * 4 + 2] = G;
    d[i * 4 + 3] = B;
  }
}

static void
pack_GBRA_10LE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  int i;
  guint16 *restrict dg = GET_G_LINE (y);
  guint16 *restrict db = GET_B_LINE (y);
  guint16 *restrict dr = GET_R_LINE (y);
  guint16 *restrict da = GET_A_LINE (y);
  guint16 G, B, R, A;
  const guint16 *restrict s = src;

  for (i = 0; i < width; i++) {
    G = (s[i * 4 + 2]) >> 6;
    B = (s[i * 4 + 3]) >> 6;
    R = (s[i * 4 + 1]) >> 6;
    A = (s[i * 4 + 0]) >> 6;

    GST_WRITE_UINT16_LE (dg + i, G);
    GST_WRITE_UINT16_LE (db + i, B);
    GST_WRITE_UINT16_LE (dr + i, R);
    GST_WRITE_UINT16_LE (da + i, A);
  }
}

#define PACK_GBRA_10BE GST_VIDEO_FORMAT_ARGB64, unpack_GBRA_10BE, 1, pack_GBRA_10BE
static void
unpack_GBRA_10BE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  int i;
  const guint16 *restrict sg = GET_G_LINE (y);
  const guint16 *restrict sb = GET_B_LINE (y);
  const guint16 *restrict sr = GET_R_LINE (y);
  const guint16 *restrict sa = GET_A_LINE (y);
  guint16 *restrict d = dest, G, B, R, A;

  sg += x;
  sb += x;
  sr += x;
  sa += x;

  for (i = 0; i < width; i++) {
    G = GST_READ_UINT16_BE (sg + i) << 6;
    B = GST_READ_UINT16_BE (sb + i) << 6;
    R = GST_READ_UINT16_BE (sr + i) << 6;
    A = GST_READ_UINT16_BE (sa + i) << 6;

    if (!(flags & GST_VIDEO_PACK_FLAG_TRUNCATE_RANGE)) {
      R |= (R >> 10);
      G |= (G >> 10);
      B |= (B >> 10);
      A |= (A >> 10);
    }

    d[i * 4 + 0] = A;
    d[i * 4 + 1] = R;
    d[i * 4 + 2] = G;
    d[i * 4 + 3] = B;
  }
}

static void
pack_GBRA_10BE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  int i;
  guint16 *restrict dg = GET_G_LINE (y);
  guint16 *restrict db = GET_B_LINE (y);
  guint16 *restrict dr = GET_R_LINE (y);
  guint16 *restrict da = GET_A_LINE (y);
  guint16 G, B, R, A;
  const guint16 *restrict s = src;

  for (i = 0; i < width; i++) {
    G = s[i * 4 + 2] >> 6;
    B = s[i * 4 + 3] >> 6;
    R = s[i * 4 + 1] >> 6;
    A = s[i * 4 + 0] >> 6;

    GST_WRITE_UINT16_BE (dg + i, G);
    GST_WRITE_UINT16_BE (db + i, B);
    GST_WRITE_UINT16_BE (dr + i, R);
    GST_WRITE_UINT16_BE (da + i, A);
  }
}

#define PACK_GBR_12LE GST_VIDEO_FORMAT_ARGB64, unpack_GBR_12LE, 1, pack_GBR_12LE
static void
unpack_GBR_12LE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  int i;
  const guint16 *sg = GET_G_LINE (y);
  const guint16 *sb = GET_B_LINE (y);
  const guint16 *sr = GET_R_LINE (y);
  guint16 *d = dest, G, B, R;

  sg += x;
  sb += x;
  sr += x;

  for (i = 0; i < width; i++) {
    G = GST_READ_UINT16_LE (sg + i) << 4;
    B = GST_READ_UINT16_LE (sb + i) << 4;
    R = GST_READ_UINT16_LE (sr + i) << 4;

    if (!(flags & GST_VIDEO_PACK_FLAG_TRUNCATE_RANGE)) {
      R |= (R >> 12);
      G |= (G >> 12);
      B |= (B >> 12);
    }

    d[i * 4 + 0] = 0xffff;
    d[i * 4 + 1] = R;
    d[i * 4 + 2] = G;
    d[i * 4 + 3] = B;
  }
}

static void
pack_GBR_12LE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  int i;
  guint16 *restrict dg = GET_G_LINE (y);
  guint16 *restrict db = GET_B_LINE (y);
  guint16 *restrict dr = GET_R_LINE (y);
  guint16 G, B, R;
  const guint16 *restrict s = src;

  for (i = 0; i < width; i++) {
    G = (s[i * 4 + 2]) >> 4;
    B = (s[i * 4 + 3]) >> 4;
    R = (s[i * 4 + 1]) >> 4;

    GST_WRITE_UINT16_LE (dg + i, G);
    GST_WRITE_UINT16_LE (db + i, B);
    GST_WRITE_UINT16_LE (dr + i, R);
  }
}

#define PACK_GBR_12BE GST_VIDEO_FORMAT_ARGB64, unpack_GBR_12BE, 1, pack_GBR_12BE
static void
unpack_GBR_12BE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  int i;
  const guint16 *restrict sg = GET_G_LINE (y);
  const guint16 *restrict sb = GET_B_LINE (y);
  const guint16 *restrict sr = GET_R_LINE (y);
  guint16 *restrict d = dest, G, B, R;

  sg += x;
  sb += x;
  sr += x;

  for (i = 0; i < width; i++) {
    G = GST_READ_UINT16_BE (sg + i) << 4;
    B = GST_READ_UINT16_BE (sb + i) << 4;
    R = GST_READ_UINT16_BE (sr + i) << 4;

    if (!(flags & GST_VIDEO_PACK_FLAG_TRUNCATE_RANGE)) {
      R |= (R >> 12);
      G |= (G >> 12);
      B |= (B >> 12);
    }

    d[i * 4 + 0] = 0xffff;
    d[i * 4 + 1] = R;
    d[i * 4 + 2] = G;
    d[i * 4 + 3] = B;
  }
}

static void
pack_GBR_12BE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  int i;
  guint16 *restrict dg = GET_G_LINE (y);
  guint16 *restrict db = GET_B_LINE (y);
  guint16 *restrict dr = GET_R_LINE (y);
  guint16 G, B, R;
  const guint16 *restrict s = src;

  for (i = 0; i < width; i++) {
    G = s[i * 4 + 2] >> 4;
    B = s[i * 4 + 3] >> 4;
    R = s[i * 4 + 1] >> 4;

    GST_WRITE_UINT16_BE (dg + i, G);
    GST_WRITE_UINT16_BE (db + i, B);
    GST_WRITE_UINT16_BE (dr + i, R);
  }
}

#define PACK_GBR_16LE GST_VIDEO_FORMAT_ARGB64, unpack_GBR_16LE, 1, pack_GBR_16LE
static void
unpack_GBR_16LE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  int i;
  const guint16 *sg = GET_G_LINE (y);
  const guint16 *sb = GET_B_LINE (y);
  const guint16 *sr = GET_R_LINE (y);
  guint16 *d = dest, G, B, R;

  sg += x;
  sb += x;
  sr += x;

  for (i = 0; i < width; i++) {
    G = GST_READ_UINT16_LE (sg + i);
    B = GST_READ_UINT16_LE (sb + i);
    R = GST_READ_UINT16_LE (sr + i);

    d[i * 4 + 0] = 0xffff;
    d[i * 4 + 1] = R;
    d[i * 4 + 2] = G;
    d[i * 4 + 3] = B;
  }
}

static void
pack_GBR_16LE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  int i;
  guint16 *restrict dg = GET_G_LINE (y);
  guint16 *restrict db = GET_B_LINE (y);
  guint16 *restrict dr = GET_R_LINE (y);
  guint16 G, B, R;
  const guint16 *restrict s = src;

  for (i = 0; i < width; i++) {
    G = (s[i * 4 + 2]);
    B = (s[i * 4 + 3]);
    R = (s[i * 4 + 1]);

    GST_WRITE_UINT16_LE (dg + i, G);
    GST_WRITE_UINT16_LE (db + i, B);
    GST_WRITE_UINT16_LE (dr + i, R);
  }
}

#define PACK_GBR_16BE GST_VIDEO_FORMAT_ARGB64, unpack_GBR_16BE, 1, pack_GBR_16BE
static void
unpack_GBR_16BE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  int i;
  const guint16 *restrict sg = GET_G_LINE (y);
  const guint16 *restrict sb = GET_B_LINE (y);
  const guint16 *restrict sr = GET_R_LINE (y);
  guint16 *restrict d = dest, G, B, R;

  sg += x;
  sb += x;
  sr += x;

  for (i = 0; i < width; i++) {
    G = GST_READ_UINT16_BE (sg + i);
    B = GST_READ_UINT16_BE (sb + i);
    R = GST_READ_UINT16_BE (sr + i);

    d[i * 4 + 0] = 0xffff;
    d[i * 4 + 1] = R;
    d[i * 4 + 2] = G;
    d[i * 4 + 3] = B;
  }
}

static void
pack_GBR_16BE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  int i;
  guint16 *restrict dg = GET_G_LINE (y);
  guint16 *restrict db = GET_B_LINE (y);
  guint16 *restrict dr = GET_R_LINE (y);
  guint16 G, B, R;
  const guint16 *restrict s = src;

  for (i = 0; i < width; i++) {
    G = s[i * 4 + 2];
    B = s[i * 4 + 3];
    R = s[i * 4 + 1];

    GST_WRITE_UINT16_BE (dg + i, G);
    GST_WRITE_UINT16_BE (db + i, B);
    GST_WRITE_UINT16_BE (dr + i, R);
  }
}

#define PACK_GBRA_12LE GST_VIDEO_FORMAT_ARGB64, unpack_GBRA_12LE, 1, pack_GBRA_12LE
static void
unpack_GBRA_12LE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  int i;
  const guint16 *sg = GET_G_LINE (y);
  const guint16 *sb = GET_B_LINE (y);
  const guint16 *sr = GET_R_LINE (y);
  const guint16 *sa = GET_A_LINE (y);
  guint16 *d = dest, G, B, R, A;

  sg += x;
  sb += x;
  sr += x;
  sa += x;

  for (i = 0; i < width; i++) {
    G = GST_READ_UINT16_LE (sg + i) << 4;
    B = GST_READ_UINT16_LE (sb + i) << 4;
    R = GST_READ_UINT16_LE (sr + i) << 4;
    A = GST_READ_UINT16_LE (sa + i) << 4;

    if (!(flags & GST_VIDEO_PACK_FLAG_TRUNCATE_RANGE)) {
      A |= (A >> 12);
      R |= (R >> 12);
      G |= (G >> 12);
      B |= (B >> 12);
    }

    d[i * 4 + 0] = A;
    d[i * 4 + 1] = R;
    d[i * 4 + 2] = G;
    d[i * 4 + 3] = B;
  }
}

static void
pack_GBRA_12LE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  int i;
  guint16 *restrict dg = GET_G_LINE (y);
  guint16 *restrict db = GET_B_LINE (y);
  guint16 *restrict dr = GET_R_LINE (y);
  guint16 *restrict da = GET_A_LINE (y);
  guint16 G, B, R, A;
  const guint16 *restrict s = src;

  for (i = 0; i < width; i++) {
    G = (s[i * 4 + 2]) >> 4;
    B = (s[i * 4 + 3]) >> 4;
    R = (s[i * 4 + 1]) >> 4;
    A = (s[i * 4 + 0]) >> 4;

    GST_WRITE_UINT16_LE (dg + i, G);
    GST_WRITE_UINT16_LE (db + i, B);
    GST_WRITE_UINT16_LE (dr + i, R);
    GST_WRITE_UINT16_LE (da + i, A);
  }
}

#define PACK_GBRA_12BE GST_VIDEO_FORMAT_ARGB64, unpack_GBRA_12BE, 1, pack_GBRA_12BE
static void
unpack_GBRA_12BE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  int i;
  const guint16 *restrict sg = GET_G_LINE (y);
  const guint16 *restrict sb = GET_B_LINE (y);
  const guint16 *restrict sr = GET_R_LINE (y);
  const guint16 *restrict sa = GET_A_LINE (y);
  guint16 *restrict d = dest, G, B, R, A;

  sg += x;
  sb += x;
  sr += x;
  sa += x;

  for (i = 0; i < width; i++) {
    G = GST_READ_UINT16_BE (sg + i) << 4;
    B = GST_READ_UINT16_BE (sb + i) << 4;
    R = GST_READ_UINT16_BE (sr + i) << 4;
    A = GST_READ_UINT16_BE (sa + i) << 4;

    if (!(flags & GST_VIDEO_PACK_FLAG_TRUNCATE_RANGE)) {
      R |= (R >> 12);
      G |= (G >> 12);
      B |= (B >> 12);
      A |= (A >> 12);
    }

    d[i * 4 + 0] = A;
    d[i * 4 + 1] = R;
    d[i * 4 + 2] = G;
    d[i * 4 + 3] = B;
  }
}

static void
pack_GBRA_12BE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  int i;
  guint16 *restrict dg = GET_G_LINE (y);
  guint16 *restrict db = GET_B_LINE (y);
  guint16 *restrict dr = GET_R_LINE (y);
  guint16 *restrict da = GET_A_LINE (y);
  guint16 G, B, R, A;
  const guint16 *restrict s = src;

  for (i = 0; i < width; i++) {
    G = s[i * 4 + 2] >> 4;
    B = s[i * 4 + 3] >> 4;
    R = s[i * 4 + 1] >> 4;
    A = s[i * 4 + 0] >> 4;

    GST_WRITE_UINT16_BE (dg + i, G);
    GST_WRITE_UINT16_BE (db + i, B);
    GST_WRITE_UINT16_BE (dr + i, R);
    GST_WRITE_UINT16_BE (da + i, A);
  }
}

#define PACK_Y444_10LE GST_VIDEO_FORMAT_AYUV64, unpack_Y444_10LE, 1, pack_Y444_10LE
static void
unpack_Y444_10LE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  int i;
  guint16 *restrict sy = GET_Y_LINE (y);
  guint16 *restrict su = GET_U_LINE (y);
  guint16 *restrict sv = GET_V_LINE (y);
  guint16 *restrict d = dest, Y, U, V;

  sy += x;
  su += x;
  sv += x;

  for (i = 0; i < width; i++) {
    Y = GST_READ_UINT16_LE (sy + i) << 6;
    U = GST_READ_UINT16_LE (su + i) << 6;
    V = GST_READ_UINT16_LE (sv + i) << 6;

    if (!(flags & GST_VIDEO_PACK_FLAG_TRUNCATE_RANGE)) {
      Y |= (Y >> 10);
      U |= (U >> 10);
      V |= (V >> 10);
    }

    d[i * 4 + 0] = 0xffff;
    d[i * 4 + 1] = Y;
    d[i * 4 + 2] = U;
    d[i * 4 + 3] = V;
  }
}

static void
pack_Y444_10LE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  int i;
  guint16 *restrict dy = GET_Y_LINE (y);
  guint16 *restrict du = GET_U_LINE (y);
  guint16 *restrict dv = GET_V_LINE (y);
  guint16 Y, U, V;
  const guint16 *restrict s = src;

  for (i = 0; i < width; i++) {
    Y = (s[i * 4 + 1]) >> 6;
    U = (s[i * 4 + 2]) >> 6;
    V = (s[i * 4 + 3]) >> 6;

    GST_WRITE_UINT16_LE (dy + i, Y);
    GST_WRITE_UINT16_LE (du + i, U);
    GST_WRITE_UINT16_LE (dv + i, V);
  }
}

#define PACK_Y444_10BE GST_VIDEO_FORMAT_AYUV64, unpack_Y444_10BE, 1, pack_Y444_10BE
static void
unpack_Y444_10BE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  int i;
  const guint16 *restrict sy = GET_Y_LINE (y);
  const guint16 *restrict su = GET_U_LINE (y);
  const guint16 *restrict sv = GET_V_LINE (y);
  guint16 *restrict d = dest, Y, U, V;

  sy += x;
  su += x;
  sv += x;

  for (i = 0; i < width; i++) {
    Y = GST_READ_UINT16_BE (sy + i) << 6;
    U = GST_READ_UINT16_BE (su + i) << 6;
    V = GST_READ_UINT16_BE (sv + i) << 6;

    if (!(flags & GST_VIDEO_PACK_FLAG_TRUNCATE_RANGE)) {
      Y |= (Y >> 10);
      U |= (U >> 10);
      V |= (V >> 10);
    }

    d[i * 4 + 0] = 0xffff;
    d[i * 4 + 1] = Y;
    d[i * 4 + 2] = U;
    d[i * 4 + 3] = V;
  }
}

static void
pack_Y444_10BE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  int i;
  guint16 *restrict dy = GET_Y_LINE (y);
  guint16 *restrict du = GET_U_LINE (y);
  guint16 *restrict dv = GET_V_LINE (y);
  guint16 Y, U, V;
  const guint16 *restrict s = src;

  for (i = 0; i < width; i++) {
    Y = s[i * 4 + 1] >> 6;
    U = s[i * 4 + 2] >> 6;
    V = s[i * 4 + 3] >> 6;

    GST_WRITE_UINT16_BE (dy + i, Y);
    GST_WRITE_UINT16_BE (du + i, U);
    GST_WRITE_UINT16_BE (dv + i, V);
  }
}

#define PACK_I420_10LE GST_VIDEO_FORMAT_AYUV64, unpack_I420_10LE, 1, pack_I420_10LE
static void
unpack_I420_10LE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  int i;
  gint uv = GET_UV_420 (y, flags);
  const guint16 *restrict sy = GET_Y_LINE (y);
  const guint16 *restrict su = GET_U_LINE (uv);
  const guint16 *restrict sv = GET_V_LINE (uv);
  guint16 *restrict d = dest, Y, U, V;

  sy += x;
  su += x >> 1;
  sv += x >> 1;

  for (i = 0; i < width; i++) {
    Y = GST_READ_UINT16_LE (sy + i) << 6;
    U = GST_READ_UINT16_LE (su + (i >> 1)) << 6;
    V = GST_READ_UINT16_LE (sv + (i >> 1)) << 6;

    if (!(flags & GST_VIDEO_PACK_FLAG_TRUNCATE_RANGE)) {
      Y |= (Y >> 10);
      U |= (U >> 10);
      V |= (V >> 10);
    }

    d[i * 4 + 0] = 0xffff;
    d[i * 4 + 1] = Y;
    d[i * 4 + 2] = U;
    d[i * 4 + 3] = V;

    if (x & 1) {
      x = 0;
      su++;
      sv++;
    }
  }
}

static void
pack_I420_10LE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  int i;
  gint uv = GET_UV_420 (y, flags);
  guint16 *restrict dy = GET_Y_LINE (y);
  guint16 *restrict du = GET_U_LINE (uv);
  guint16 *restrict dv = GET_V_LINE (uv);
  guint16 Y0, Y1, U, V;
  const guint16 *restrict s = src;

  if (IS_CHROMA_LINE_420 (y, flags)) {
    for (i = 0; i < width - 1; i += 2) {
      Y0 = s[i * 4 + 1] >> 6;
      Y1 = s[i * 4 + 5] >> 6;
      U = s[i * 4 + 2] >> 6;
      V = s[i * 4 + 3] >> 6;

      GST_WRITE_UINT16_LE (dy + i + 0, Y0);
      GST_WRITE_UINT16_LE (dy + i + 1, Y1);
      GST_WRITE_UINT16_LE (du + (i >> 1), U);
      GST_WRITE_UINT16_LE (dv + (i >> 1), V);
    }
    if (i == width - 1) {
      Y0 = s[i * 4 + 1] >> 6;
      U = s[i * 4 + 2] >> 6;
      V = s[i * 4 + 3] >> 6;

      GST_WRITE_UINT16_LE (dy + i, Y0);
      GST_WRITE_UINT16_LE (du + (i >> 1), U);
      GST_WRITE_UINT16_LE (dv + (i >> 1), V);
    }
  } else {
    for (i = 0; i < width; i++) {
      Y0 = s[i * 4 + 1] >> 6;
      GST_WRITE_UINT16_LE (dy + i, Y0);
    }
  }
}

#define PACK_I420_10BE GST_VIDEO_FORMAT_AYUV64, unpack_I420_10BE, 1, pack_I420_10BE
static void
unpack_I420_10BE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  int i;
  gint uv = GET_UV_420 (y, flags);
  const guint16 *restrict sy = GET_Y_LINE (y);
  const guint16 *restrict su = GET_U_LINE (uv);
  const guint16 *restrict sv = GET_V_LINE (uv);
  guint16 *restrict d = dest, Y, U, V;

  sy += x;
  su += x >> 1;
  sv += x >> 1;

  for (i = 0; i < width; i++) {
    Y = GST_READ_UINT16_BE (sy + i) << 6;
    U = GST_READ_UINT16_BE (su + (i >> 1)) << 6;
    V = GST_READ_UINT16_BE (sv + (i >> 1)) << 6;

    if (!(flags & GST_VIDEO_PACK_FLAG_TRUNCATE_RANGE)) {
      Y |= (Y >> 10);
      U |= (U >> 10);
      V |= (V >> 10);
    }

    d[i * 4 + 0] = 0xffff;
    d[i * 4 + 1] = Y;
    d[i * 4 + 2] = U;
    d[i * 4 + 3] = V;

    if (x & 1) {
      x = 0;
      su++;
      sv++;
    }
  }
}

static void
pack_I420_10BE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  int i;
  gint uv = GET_UV_420 (y, flags);
  guint16 *restrict dy = GET_Y_LINE (y);
  guint16 *restrict du = GET_U_LINE (uv);
  guint16 *restrict dv = GET_V_LINE (uv);
  guint16 Y0, Y1, U, V;
  const guint16 *restrict s = src;

  if (IS_CHROMA_LINE_420 (y, flags)) {
    for (i = 0; i < width - 1; i += 2) {
      Y0 = s[i * 4 + 1] >> 6;
      Y1 = s[i * 4 + 5] >> 6;
      U = s[i * 4 + 2] >> 6;
      V = s[i * 4 + 3] >> 6;

      GST_WRITE_UINT16_BE (dy + i + 0, Y0);
      GST_WRITE_UINT16_BE (dy + i + 1, Y1);
      GST_WRITE_UINT16_BE (du + (i >> 1), U);
      GST_WRITE_UINT16_BE (dv + (i >> 1), V);
    }
    if (i == width - 1) {
      Y0 = s[i * 4 + 1] >> 6;
      U = s[i * 4 + 2] >> 6;
      V = s[i * 4 + 3] >> 6;

      GST_WRITE_UINT16_BE (dy + i, Y0);
      GST_WRITE_UINT16_BE (du + (i >> 1), U);
      GST_WRITE_UINT16_BE (dv + (i >> 1), V);
    }
  } else {
    for (i = 0; i < width; i++) {
      Y0 = s[i * 4 + 1] >> 6;
      GST_WRITE_UINT16_BE (dy + i, Y0);
    }
  }
}

#define PACK_I422_10LE GST_VIDEO_FORMAT_AYUV64, unpack_I422_10LE, 1, pack_I422_10LE
static void
unpack_I422_10LE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  int i;
  const guint16 *restrict sy = GET_Y_LINE (y);
  const guint16 *restrict su = GET_U_LINE (y);
  const guint16 *restrict sv = GET_V_LINE (y);
  guint16 *restrict d = dest, Y, U, V;

  sy += x;
  su += x >> 1;
  sv += x >> 1;

  for (i = 0; i < width; i++) {
    Y = GST_READ_UINT16_LE (sy + i) << 6;
    U = GST_READ_UINT16_LE (su + (i >> 1)) << 6;
    V = GST_READ_UINT16_LE (sv + (i >> 1)) << 6;

    if (!(flags & GST_VIDEO_PACK_FLAG_TRUNCATE_RANGE)) {
      Y |= (Y >> 10);
      U |= (U >> 10);
      V |= (V >> 10);
    }

    d[i * 4 + 0] = 0xffff;
    d[i * 4 + 1] = Y;
    d[i * 4 + 2] = U;
    d[i * 4 + 3] = V;

    if (x & 1) {
      x = 0;
      su++;
      sv++;
    }
  }
}

static void
pack_I422_10LE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  int i;
  guint16 *restrict dy = GET_Y_LINE (y);
  guint16 *restrict du = GET_U_LINE (y);
  guint16 *restrict dv = GET_V_LINE (y);
  guint16 Y0, Y1, U, V;
  const guint16 *restrict s = src;

  for (i = 0; i < width - 1; i += 2) {
    Y0 = s[i * 4 + 1] >> 6;
    Y1 = s[i * 4 + 5] >> 6;
    U = s[i * 4 + 2] >> 6;
    V = s[i * 4 + 3] >> 6;

    GST_WRITE_UINT16_LE (dy + i + 0, Y0);
    GST_WRITE_UINT16_LE (dy + i + 1, Y1);
    GST_WRITE_UINT16_LE (du + (i >> 1), U);
    GST_WRITE_UINT16_LE (dv + (i >> 1), V);
  }
  if (i == width - 1) {
    Y0 = s[i * 4 + 1] >> 6;
    U = s[i * 4 + 2] >> 6;
    V = s[i * 4 + 3] >> 6;

    GST_WRITE_UINT16_LE (dy + i, Y0);
    GST_WRITE_UINT16_LE (du + (i >> 1), U);
    GST_WRITE_UINT16_LE (dv + (i >> 1), V);
  }
}

#define PACK_I422_10BE GST_VIDEO_FORMAT_AYUV64, unpack_I422_10BE, 1, pack_I422_10BE
static void
unpack_I422_10BE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  int i;
  const guint16 *restrict sy = GET_Y_LINE (y);
  const guint16 *restrict su = GET_U_LINE (y);
  const guint16 *restrict sv = GET_V_LINE (y);
  guint16 *restrict d = dest, Y, U, V;

  sy += x;
  su += x >> 1;
  sv += x >> 1;

  for (i = 0; i < width; i++) {
    Y = GST_READ_UINT16_BE (sy + i) << 6;
    U = GST_READ_UINT16_BE (su + (i >> 1)) << 6;
    V = GST_READ_UINT16_BE (sv + (i >> 1)) << 6;

    if (!(flags & GST_VIDEO_PACK_FLAG_TRUNCATE_RANGE)) {
      Y |= (Y >> 10);
      U |= (U >> 10);
      V |= (V >> 10);
    }

    d[i * 4 + 0] = 0xffff;
    d[i * 4 + 1] = Y;
    d[i * 4 + 2] = U;
    d[i * 4 + 3] = V;

    if (x & 1) {
      x = 0;
      su++;
      sv++;
    }
  }
}

static void
pack_I422_10BE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  int i;
  guint16 *restrict dy = GET_Y_LINE (y);
  guint16 *restrict du = GET_U_LINE (y);
  guint16 *restrict dv = GET_V_LINE (y);
  guint16 Y0, Y1, U, V;
  const guint16 *restrict s = src;

  for (i = 0; i < width - 1; i += 2) {
    Y0 = s[i * 4 + 1] >> 6;
    Y1 = s[i * 4 + 5] >> 6;
    U = s[i * 4 + 2] >> 6;
    V = s[i * 4 + 3] >> 6;

    GST_WRITE_UINT16_BE (dy + i + 0, Y0);
    GST_WRITE_UINT16_BE (dy + i + 1, Y1);
    GST_WRITE_UINT16_BE (du + (i >> 1), U);
    GST_WRITE_UINT16_BE (dv + (i >> 1), V);
  }
  if (i == width - 1) {
    Y0 = s[i * 4 + 1] >> 6;
    U = s[i * 4 + 2] >> 6;
    V = s[i * 4 + 3] >> 6;

    GST_WRITE_UINT16_BE (dy + i, Y0);
    GST_WRITE_UINT16_BE (du + (i >> 1), U);
    GST_WRITE_UINT16_BE (dv + (i >> 1), V);
  }
}

#define PACK_Y444_12LE GST_VIDEO_FORMAT_AYUV64, unpack_Y444_12LE, 1, pack_Y444_12LE
static void
unpack_Y444_12LE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  int i;
  guint16 *restrict sy = GET_Y_LINE (y);
  guint16 *restrict su = GET_U_LINE (y);
  guint16 *restrict sv = GET_V_LINE (y);
  guint16 *restrict d = dest, Y, U, V;

  sy += x;
  su += x;
  sv += x;

  for (i = 0; i < width; i++) {
    Y = GST_READ_UINT16_LE (sy + i) << 4;
    U = GST_READ_UINT16_LE (su + i) << 4;
    V = GST_READ_UINT16_LE (sv + i) << 4;

    if (!(flags & GST_VIDEO_PACK_FLAG_TRUNCATE_RANGE)) {
      Y |= (Y >> 12);
      U |= (U >> 12);
      V |= (V >> 12);
    }

    d[i * 4 + 0] = 0xffff;
    d[i * 4 + 1] = Y;
    d[i * 4 + 2] = U;
    d[i * 4 + 3] = V;
  }
}

static void
pack_Y444_12LE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  int i;
  guint16 *restrict dy = GET_Y_LINE (y);
  guint16 *restrict du = GET_U_LINE (y);
  guint16 *restrict dv = GET_V_LINE (y);
  guint16 Y, U, V;
  const guint16 *restrict s = src;

  for (i = 0; i < width; i++) {
    Y = (s[i * 4 + 1]) >> 4;
    U = (s[i * 4 + 2]) >> 4;
    V = (s[i * 4 + 3]) >> 4;

    GST_WRITE_UINT16_LE (dy + i, Y);
    GST_WRITE_UINT16_LE (du + i, U);
    GST_WRITE_UINT16_LE (dv + i, V);
  }
}

#define PACK_Y444_12BE GST_VIDEO_FORMAT_AYUV64, unpack_Y444_12BE, 1, pack_Y444_12BE
static void
unpack_Y444_12BE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  int i;
  const guint16 *restrict sy = GET_Y_LINE (y);
  const guint16 *restrict su = GET_U_LINE (y);
  const guint16 *restrict sv = GET_V_LINE (y);
  guint16 *restrict d = dest, Y, U, V;

  sy += x;
  su += x;
  sv += x;

  for (i = 0; i < width; i++) {
    Y = GST_READ_UINT16_BE (sy + i) << 4;
    U = GST_READ_UINT16_BE (su + i) << 4;
    V = GST_READ_UINT16_BE (sv + i) << 4;

    if (!(flags & GST_VIDEO_PACK_FLAG_TRUNCATE_RANGE)) {
      Y |= (Y >> 12);
      U |= (U >> 12);
      V |= (V >> 12);
    }

    d[i * 4 + 0] = 0xffff;
    d[i * 4 + 1] = Y;
    d[i * 4 + 2] = U;
    d[i * 4 + 3] = V;
  }
}

static void
pack_Y444_12BE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  int i;
  guint16 *restrict dy = GET_Y_LINE (y);
  guint16 *restrict du = GET_U_LINE (y);
  guint16 *restrict dv = GET_V_LINE (y);
  guint16 Y, U, V;
  const guint16 *restrict s = src;

  for (i = 0; i < width; i++) {
    Y = s[i * 4 + 1] >> 4;
    U = s[i * 4 + 2] >> 4;
    V = s[i * 4 + 3] >> 4;

    GST_WRITE_UINT16_BE (dy + i, Y);
    GST_WRITE_UINT16_BE (du + i, U);
    GST_WRITE_UINT16_BE (dv + i, V);
  }
}

#define PACK_I420_12LE GST_VIDEO_FORMAT_AYUV64, unpack_I420_12LE, 1, pack_I420_12LE
static void
unpack_I420_12LE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  int i;
  gint uv = GET_UV_420 (y, flags);
  const guint16 *restrict sy = GET_Y_LINE (y);
  const guint16 *restrict su = GET_U_LINE (uv);
  const guint16 *restrict sv = GET_V_LINE (uv);
  guint16 *restrict d = dest, Y, U, V;

  sy += x;
  su += x >> 1;
  sv += x >> 1;

  for (i = 0; i < width; i++) {
    Y = GST_READ_UINT16_LE (sy + i) << 4;
    U = GST_READ_UINT16_LE (su + (i >> 1)) << 4;
    V = GST_READ_UINT16_LE (sv + (i >> 1)) << 4;

    if (!(flags & GST_VIDEO_PACK_FLAG_TRUNCATE_RANGE)) {
      Y |= (Y >> 12);
      U |= (U >> 12);
      V |= (V >> 12);
    }

    d[i * 4 + 0] = 0xffff;
    d[i * 4 + 1] = Y;
    d[i * 4 + 2] = U;
    d[i * 4 + 3] = V;

    if (x & 1) {
      x = 0;
      su++;
      sv++;
    }
  }
}

static void
pack_I420_12LE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  int i;
  gint uv = GET_UV_420 (y, flags);
  guint16 *restrict dy = GET_Y_LINE (y);
  guint16 *restrict du = GET_U_LINE (uv);
  guint16 *restrict dv = GET_V_LINE (uv);
  guint16 Y0, Y1, U, V;
  const guint16 *restrict s = src;

  if (IS_CHROMA_LINE_420 (y, flags)) {
    for (i = 0; i < width - 1; i += 2) {
      Y0 = s[i * 4 + 1] >> 4;
      Y1 = s[i * 4 + 5] >> 4;
      U = s[i * 4 + 2] >> 4;
      V = s[i * 4 + 3] >> 4;

      GST_WRITE_UINT16_LE (dy + i + 0, Y0);
      GST_WRITE_UINT16_LE (dy + i + 1, Y1);
      GST_WRITE_UINT16_LE (du + (i >> 1), U);
      GST_WRITE_UINT16_LE (dv + (i >> 1), V);
    }
    if (i == width - 1) {
      Y0 = s[i * 4 + 1] >> 4;
      U = s[i * 4 + 2] >> 4;
      V = s[i * 4 + 3] >> 4;

      GST_WRITE_UINT16_LE (dy + i, Y0);
      GST_WRITE_UINT16_LE (du + (i >> 1), U);
      GST_WRITE_UINT16_LE (dv + (i >> 1), V);
    }
  } else {
    for (i = 0; i < width; i++) {
      Y0 = s[i * 4 + 1] >> 4;
      GST_WRITE_UINT16_LE (dy + i, Y0);
    }
  }
}

#define PACK_I420_12BE GST_VIDEO_FORMAT_AYUV64, unpack_I420_12BE, 1, pack_I420_12BE
static void
unpack_I420_12BE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  int i;
  gint uv = GET_UV_420 (y, flags);
  const guint16 *restrict sy = GET_Y_LINE (y);
  const guint16 *restrict su = GET_U_LINE (uv);
  const guint16 *restrict sv = GET_V_LINE (uv);
  guint16 *restrict d = dest, Y, U, V;

  sy += x;
  su += x >> 1;
  sv += x >> 1;

  for (i = 0; i < width; i++) {
    Y = GST_READ_UINT16_BE (sy + i) << 4;
    U = GST_READ_UINT16_BE (su + (i >> 1)) << 4;
    V = GST_READ_UINT16_BE (sv + (i >> 1)) << 4;

    if (!(flags & GST_VIDEO_PACK_FLAG_TRUNCATE_RANGE)) {
      Y |= (Y >> 12);
      U |= (U >> 12);
      V |= (V >> 12);
    }

    d[i * 4 + 0] = 0xffff;
    d[i * 4 + 1] = Y;
    d[i * 4 + 2] = U;
    d[i * 4 + 3] = V;

    if (x & 1) {
      x = 0;
      su++;
      sv++;
    }
  }
}

static void
pack_I420_12BE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  int i;
  gint uv = GET_UV_420 (y, flags);
  guint16 *restrict dy = GET_Y_LINE (y);
  guint16 *restrict du = GET_U_LINE (uv);
  guint16 *restrict dv = GET_V_LINE (uv);
  guint16 Y0, Y1, U, V;
  const guint16 *restrict s = src;

  if (IS_CHROMA_LINE_420 (y, flags)) {
    for (i = 0; i < width - 1; i += 2) {
      Y0 = s[i * 4 + 1] >> 4;
      Y1 = s[i * 4 + 5] >> 4;
      U = s[i * 4 + 2] >> 4;
      V = s[i * 4 + 3] >> 4;

      GST_WRITE_UINT16_BE (dy + i + 0, Y0);
      GST_WRITE_UINT16_BE (dy + i + 1, Y1);
      GST_WRITE_UINT16_BE (du + (i >> 1), U);
      GST_WRITE_UINT16_BE (dv + (i >> 1), V);
    }
    if (i == width - 1) {
      Y0 = s[i * 4 + 1] >> 4;
      U = s[i * 4 + 2] >> 4;
      V = s[i * 4 + 3] >> 4;

      GST_WRITE_UINT16_BE (dy + i, Y0);
      GST_WRITE_UINT16_BE (du + (i >> 1), U);
      GST_WRITE_UINT16_BE (dv + (i >> 1), V);
    }
  } else {
    for (i = 0; i < width; i++) {
      Y0 = s[i * 4 + 1] >> 4;
      GST_WRITE_UINT16_BE (dy + i, Y0);
    }
  }
}

#define PACK_I422_12LE GST_VIDEO_FORMAT_AYUV64, unpack_I422_12LE, 1, pack_I422_12LE
static void
unpack_I422_12LE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  int i;
  const guint16 *restrict sy = GET_Y_LINE (y);
  const guint16 *restrict su = GET_U_LINE (y);
  const guint16 *restrict sv = GET_V_LINE (y);
  guint16 *restrict d = dest, Y, U, V;

  sy += x;
  su += x >> 1;
  sv += x >> 1;

  for (i = 0; i < width; i++) {
    Y = GST_READ_UINT16_LE (sy + i) << 4;
    U = GST_READ_UINT16_LE (su + (i >> 1)) << 4;
    V = GST_READ_UINT16_LE (sv + (i >> 1)) << 4;

    if (!(flags & GST_VIDEO_PACK_FLAG_TRUNCATE_RANGE)) {
      Y |= (Y >> 12);
      U |= (U >> 12);
      V |= (V >> 12);
    }

    d[i * 4 + 0] = 0xffff;
    d[i * 4 + 1] = Y;
    d[i * 4 + 2] = U;
    d[i * 4 + 3] = V;

    if (x & 1) {
      x = 0;
      su++;
      sv++;
    }
  }
}

static void
pack_I422_12LE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  int i;
  guint16 *restrict dy = GET_Y_LINE (y);
  guint16 *restrict du = GET_U_LINE (y);
  guint16 *restrict dv = GET_V_LINE (y);
  guint16 Y0, Y1, U, V;
  const guint16 *restrict s = src;

  for (i = 0; i < width - 1; i += 2) {
    Y0 = s[i * 4 + 1] >> 4;
    Y1 = s[i * 4 + 5] >> 4;
    U = s[i * 4 + 2] >> 4;
    V = s[i * 4 + 3] >> 4;

    GST_WRITE_UINT16_LE (dy + i + 0, Y0);
    GST_WRITE_UINT16_LE (dy + i + 1, Y1);
    GST_WRITE_UINT16_LE (du + (i >> 1), U);
    GST_WRITE_UINT16_LE (dv + (i >> 1), V);
  }
  if (i == width - 1) {
    Y0 = s[i * 4 + 1] >> 4;
    U = s[i * 4 + 2] >> 4;
    V = s[i * 4 + 3] >> 4;

    GST_WRITE_UINT16_LE (dy + i, Y0);
    GST_WRITE_UINT16_LE (du + (i >> 1), U);
    GST_WRITE_UINT16_LE (dv + (i >> 1), V);
  }
}

#define PACK_I422_12BE GST_VIDEO_FORMAT_AYUV64, unpack_I422_12BE, 1, pack_I422_12BE
static void
unpack_I422_12BE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  int i;
  const guint16 *restrict sy = GET_Y_LINE (y);
  const guint16 *restrict su = GET_U_LINE (y);
  const guint16 *restrict sv = GET_V_LINE (y);
  guint16 *restrict d = dest, Y, U, V;

  sy += x;
  su += x >> 1;
  sv += x >> 1;

  for (i = 0; i < width; i++) {
    Y = GST_READ_UINT16_BE (sy + i) << 4;
    U = GST_READ_UINT16_BE (su + (i >> 1)) << 4;
    V = GST_READ_UINT16_BE (sv + (i >> 1)) << 4;

    if (!(flags & GST_VIDEO_PACK_FLAG_TRUNCATE_RANGE)) {
      Y |= (Y >> 12);
      U |= (U >> 12);
      V |= (V >> 12);
    }

    d[i * 4 + 0] = 0xffff;
    d[i * 4 + 1] = Y;
    d[i * 4 + 2] = U;
    d[i * 4 + 3] = V;

    if (x & 1) {
      x = 0;
      su++;
      sv++;
    }
  }
}

static void
pack_I422_12BE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  int i;
  guint16 *restrict dy = GET_Y_LINE (y);
  guint16 *restrict du = GET_U_LINE (y);
  guint16 *restrict dv = GET_V_LINE (y);
  guint16 Y0, Y1, U, V;
  const guint16 *restrict s = src;

  for (i = 0; i < width - 1; i += 2) {
    Y0 = s[i * 4 + 1] >> 4;
    Y1 = s[i * 4 + 5] >> 4;
    U = s[i * 4 + 2] >> 4;
    V = s[i * 4 + 3] >> 4;

    GST_WRITE_UINT16_BE (dy + i + 0, Y0);
    GST_WRITE_UINT16_BE (dy + i + 1, Y1);
    GST_WRITE_UINT16_BE (du + (i >> 1), U);
    GST_WRITE_UINT16_BE (dv + (i >> 1), V);
  }
  if (i == width - 1) {
    Y0 = s[i * 4 + 1] >> 4;
    U = s[i * 4 + 2] >> 4;
    V = s[i * 4 + 3] >> 4;

    GST_WRITE_UINT16_BE (dy + i, Y0);
    GST_WRITE_UINT16_BE (du + (i >> 1), U);
    GST_WRITE_UINT16_BE (dv + (i >> 1), V);
  }
}

#define PACK_A444 GST_VIDEO_FORMAT_AYUV, unpack_A444, 1, pack_A444
static void
unpack_A444 (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  int i;
  guint8 *restrict sa = GET_A_LINE (y);
  guint8 *restrict sy = GET_Y_LINE (y);
  guint8 *restrict su = GET_U_LINE (y);
  guint8 *restrict sv = GET_V_LINE (y);
  guint8 *restrict d = dest, A, Y, U, V;

  sa += x;
  sy += x;
  su += x;
  sv += x;

  for (i = 0; i < width; i++) {
    A = sa[i];
    Y = sy[i];
    U = su[i];
    V = sv[i];

    d[i * 4 + 0] = A;
    d[i * 4 + 1] = Y;
    d[i * 4 + 2] = U;
    d[i * 4 + 3] = V;
  }
}

static void
pack_A444 (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  int i;
  guint8 *restrict da = GET_A_LINE (y);
  guint8 *restrict dy = GET_Y_LINE (y);
  guint8 *restrict du = GET_U_LINE (y);
  guint8 *restrict dv = GET_V_LINE (y);
  guint8 A, Y, U, V;
  const guint8 *restrict s = src;

  for (i = 0; i < width; i++) {
    A = (s[i * 4 + 0]);
    Y = (s[i * 4 + 1]);
    U = (s[i * 4 + 2]);
    V = (s[i * 4 + 3]);

    da[i] = A;
    dy[i] = Y;
    du[i] = U;
    dv[i] = V;
  }
}

static void
unpack_A444_16 (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  int i;
  guint16 *restrict sa = GET_A_LINE (y);
  guint16 *restrict sy = GET_Y_LINE (y);
  guint16 *restrict su = GET_U_LINE (y);
  guint16 *restrict sv = GET_V_LINE (y);
  guint16 *restrict d = dest, A, Y, U, V;

  sa += x;
  sy += x;
  su += x;
  sv += x;

  for (i = 0; i < width; i++) {
    if (info->flags & GST_VIDEO_FORMAT_FLAG_LE) {
      A = GST_READ_UINT16_LE (sa + i) << (16 - info->bits);
      Y = GST_READ_UINT16_LE (sy + i) << (16 - info->bits);
      U = GST_READ_UINT16_LE (su + i) << (16 - info->bits);
      V = GST_READ_UINT16_LE (sv + i) << (16 - info->bits);
    } else {
      A = GST_READ_UINT16_BE (sa + i) << (16 - info->bits);
      Y = GST_READ_UINT16_BE (sy + i) << (16 - info->bits);
      U = GST_READ_UINT16_BE (su + i) << (16 - info->bits);
      V = GST_READ_UINT16_BE (sv + i) << (16 - info->bits);
    }

    if (!(flags & GST_VIDEO_PACK_FLAG_TRUNCATE_RANGE)) {
      A |= (A >> info->bits);
      Y |= (Y >> info->bits);
      U |= (U >> info->bits);
      V |= (V >> info->bits);
    }

    d[i * 4 + 0] = A;
    d[i * 4 + 1] = Y;
    d[i * 4 + 2] = U;
    d[i * 4 + 3] = V;
  }
}

static void
pack_A444_16 (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  int i;
  guint16 *restrict da = GET_A_LINE (y);
  guint16 *restrict dy = GET_Y_LINE (y);
  guint16 *restrict du = GET_U_LINE (y);
  guint16 *restrict dv = GET_V_LINE (y);
  guint16 A, Y, U, V;
  const guint16 *restrict s = src;

  for (i = 0; i < width; i++) {
    A = (s[i * 4 + 0]) >> (16 - info->bits);
    Y = (s[i * 4 + 1]) >> (16 - info->bits);
    U = (s[i * 4 + 2]) >> (16 - info->bits);
    V = (s[i * 4 + 3]) >> (16 - info->bits);

    if (info->flags & GST_VIDEO_FORMAT_FLAG_LE) {
      GST_WRITE_UINT16_LE (da + i, A);
      GST_WRITE_UINT16_LE (dy + i, Y);
      GST_WRITE_UINT16_LE (du + i, U);
      GST_WRITE_UINT16_LE (dv + i, V);
    } else {
      GST_WRITE_UINT16_BE (da + i, A);
      GST_WRITE_UINT16_BE (dy + i, Y);
      GST_WRITE_UINT16_BE (du + i, U);
      GST_WRITE_UINT16_BE (dv + i, V);
    }
  }
}

#define PACK_A444_10BE GST_VIDEO_FORMAT_AYUV64, unpack_A444_16, 1, pack_A444_16
#define PACK_A444_10LE GST_VIDEO_FORMAT_AYUV64, unpack_A444_16, 1, pack_A444_16
#define PACK_A444_12BE GST_VIDEO_FORMAT_AYUV64, unpack_A444_16, 1, pack_A444_16
#define PACK_A444_12LE GST_VIDEO_FORMAT_AYUV64, unpack_A444_16, 1, pack_A444_16
#define PACK_A444_16BE GST_VIDEO_FORMAT_AYUV64, unpack_A444_16, 1, pack_A444_16
#define PACK_A444_16LE GST_VIDEO_FORMAT_AYUV64, unpack_A444_16, 1, pack_A444_16

static void
unpack_A420_16 (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  int i;
  gint uv = GET_UV_420 (y, flags);
  const guint16 *restrict sa = GET_A_LINE (y);
  const guint16 *restrict sy = GET_Y_LINE (y);
  const guint16 *restrict su = GET_U_LINE (uv);
  const guint16 *restrict sv = GET_V_LINE (uv);
  guint16 *restrict d = dest, A, Y, U, V;

  sa += x;
  sy += x;
  su += x >> 1;
  sv += x >> 1;

  for (i = 0; i < width; i++) {
    if (info->flags & GST_VIDEO_FORMAT_FLAG_LE) {
      A = GST_READ_UINT16_LE (sa + i) << (16 - info->bits);
      Y = GST_READ_UINT16_LE (sy + i) << (16 - info->bits);
      U = GST_READ_UINT16_LE (su + (i >> 1)) << (16 - info->bits);
      V = GST_READ_UINT16_LE (sv + (i >> 1)) << (16 - info->bits);
    } else {
      A = GST_READ_UINT16_BE (sa + i) << (16 - info->bits);
      Y = GST_READ_UINT16_BE (sy + i) << (16 - info->bits);
      U = GST_READ_UINT16_BE (su + (i >> 1)) << (16 - info->bits);
      V = GST_READ_UINT16_BE (sv + (i >> 1)) << (16 - info->bits);
    }
    if (!(flags & GST_VIDEO_PACK_FLAG_TRUNCATE_RANGE)) {
      A |= (A >> info->bits);
      Y |= (Y >> info->bits);
      U |= (U >> info->bits);
      V |= (V >> info->bits);
    }

    d[i * 4 + 0] = A;
    d[i * 4 + 1] = Y;
    d[i * 4 + 2] = U;
    d[i * 4 + 3] = V;
    if (x & 1) {
      x = 0;
      su++;
      sv++;
    }
  }
}

static void
pack_A420_16 (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  int i;
  gint uv = GET_UV_420 (y, flags);
  guint16 *restrict da = GET_A_LINE (y);
  guint16 *restrict dy = GET_Y_LINE (y);
  guint16 *restrict du = GET_U_LINE (uv);
  guint16 *restrict dv = GET_V_LINE (uv);
  guint16 A0, Y0, A1, Y1, U, V;
  const guint16 *restrict s = src;

  if (IS_CHROMA_LINE_420 (y, flags)) {
    for (i = 0; i < width - 1; i += 2) {
      A0 = s[i * 4 + 0] >> (16 - info->bits);
      Y0 = s[i * 4 + 1] >> (16 - info->bits);
      A1 = s[i * 4 + 4] >> (16 - info->bits);
      Y1 = s[i * 4 + 5] >> (16 - info->bits);
      U = s[i * 4 + 2] >> (16 - info->bits);
      V = s[i * 4 + 3] >> (16 - info->bits);

      if (info->flags & GST_VIDEO_FORMAT_FLAG_LE) {
        GST_WRITE_UINT16_LE (da + i + 0, A0);
        GST_WRITE_UINT16_LE (dy + i + 0, Y0);
        GST_WRITE_UINT16_LE (da + i + 1, A1);
        GST_WRITE_UINT16_LE (dy + i + 1, Y1);
        GST_WRITE_UINT16_LE (du + (i >> 1), U);
        GST_WRITE_UINT16_LE (dv + (i >> 1), V);
      } else {
        GST_WRITE_UINT16_BE (da + i + 0, A0);
        GST_WRITE_UINT16_BE (dy + i + 0, Y0);
        GST_WRITE_UINT16_BE (da + i + 1, A1);
        GST_WRITE_UINT16_BE (dy + i + 1, Y1);
        GST_WRITE_UINT16_BE (du + (i >> 1), U);
        GST_WRITE_UINT16_BE (dv + (i >> 1), V);
      }
    }
    if (i == width - 1) {
      A0 = s[i * 4 + 0] >> (16 - info->bits);
      Y0 = s[i * 4 + 1] >> (16 - info->bits);
      U = s[i * 4 + 2] >> (16 - info->bits);
      V = s[i * 4 + 3] >> (16 - info->bits);
      if (info->flags & GST_VIDEO_FORMAT_FLAG_LE) {
        GST_WRITE_UINT16_LE (da + i, A0);
        GST_WRITE_UINT16_LE (dy + i, Y0);
        GST_WRITE_UINT16_LE (du + (i >> 1), U);
        GST_WRITE_UINT16_LE (dv + (i >> 1), V);
      } else {
        GST_WRITE_UINT16_BE (da + i, A0);
        GST_WRITE_UINT16_BE (dy + i, Y0);
        GST_WRITE_UINT16_BE (du + (i >> 1), U);
        GST_WRITE_UINT16_BE (dv + (i >> 1), V);
      }
    }
  } else {
    for (i = 0; i < width; i++) {
      A0 = s[i * 4 + 0] >> (16 - info->bits);
      Y0 = s[i * 4 + 1] >> (16 - info->bits);
      if (info->flags & GST_VIDEO_FORMAT_FLAG_LE) {
        GST_WRITE_UINT16_LE (da + i, A0);
        GST_WRITE_UINT16_LE (dy + i, Y0);
      } else {
        GST_WRITE_UINT16_BE (da + i, A0);
        GST_WRITE_UINT16_BE (dy + i, Y0);
      }
    }
  }
}

#define PACK_A420_10LE GST_VIDEO_FORMAT_AYUV64, unpack_A420_16, 1, pack_A420_16
#define PACK_A420_10BE GST_VIDEO_FORMAT_AYUV64, unpack_A420_16, 1, pack_A420_16
#define PACK_A420_12LE GST_VIDEO_FORMAT_AYUV64, unpack_A420_16, 1, pack_A420_16
#define PACK_A420_12BE GST_VIDEO_FORMAT_AYUV64, unpack_A420_16, 1, pack_A420_16
#define PACK_A420_16LE GST_VIDEO_FORMAT_AYUV64, unpack_A420_16, 1, pack_A420_16
#define PACK_A420_16BE GST_VIDEO_FORMAT_AYUV64, unpack_A420_16, 1, pack_A420_16

#define PACK_A422 GST_VIDEO_FORMAT_AYUV, unpack_A422, 1, pack_A422
static void
unpack_A422 (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  int i;
  const guint8 *restrict sa = GET_A_LINE (y);
  const guint8 *restrict sy = GET_Y_LINE (y);
  const guint8 *restrict su = GET_U_LINE (y);
  const guint8 *restrict sv = GET_V_LINE (y);
  guint8 *restrict d = dest, A, Y, U, V;

  sa += x;
  sy += x;
  su += x >> 1;
  sv += x >> 1;

  for (i = 0; i < width; i++) {
    A = sa[i];
    Y = sy[i];
    U = su[(i >> 1)];
    V = sv[(i >> 1)];

    d[i * 4 + 0] = A;
    d[i * 4 + 1] = Y;
    d[i * 4 + 2] = U;
    d[i * 4 + 3] = V;

    if (x & 1) {
      x = 0;
      su++;
      sv++;
    }
  }
}

static void
pack_A422 (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  int i;
  guint8 *restrict da = GET_A_LINE (y);
  guint8 *restrict dy = GET_Y_LINE (y);
  guint8 *restrict du = GET_U_LINE (y);
  guint8 *restrict dv = GET_V_LINE (y);
  guint8 A0, Y0, A1, Y1, U, V;
  const guint8 *restrict s = src;

  for (i = 0; i < width - 1; i += 2) {
    A0 = s[i * 4 + 0];
    Y0 = s[i * 4 + 1];
    A1 = s[i * 4 + 4];
    Y1 = s[i * 4 + 5];
    U = s[i * 4 + 2];
    V = s[i * 4 + 3];

    da[i + 0] = A0;
    dy[i + 0] = Y0;
    da[i + 1] = A1;
    dy[i + 1] = Y1;
    du[i >> 1] = U;
    dv[i >> 1] = V;
  }
  if (i == width - 1) {
    A0 = s[i * 4 + 0];
    Y0 = s[i * 4 + 1];
    U = s[i * 4 + 2];
    V = s[i * 4 + 3];

    da[i + 0] = A0;
    dy[i + 0] = Y0;
    du[i >> 1] = U;
    dv[i >> 1] = V;
  }
}

static void
unpack_A422_16 (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  int i;
  const guint16 *restrict sa = GET_A_LINE (y);
  const guint16 *restrict sy = GET_Y_LINE (y);
  const guint16 *restrict su = GET_U_LINE (y);
  const guint16 *restrict sv = GET_V_LINE (y);
  guint16 *restrict d = dest, A, Y, U, V;

  sa += x;
  sy += x;
  su += x >> 1;
  sv += x >> 1;

  for (i = 0; i < width; i++) {
    if (info->flags & GST_VIDEO_FORMAT_FLAG_LE) {
      A = GST_READ_UINT16_LE (sa + i) << (16 - info->bits);
      Y = GST_READ_UINT16_LE (sy + i) << (16 - info->bits);
      U = GST_READ_UINT16_LE (su + (i >> 1)) << (16 - info->bits);
      V = GST_READ_UINT16_LE (sv + (i >> 1)) << (16 - info->bits);
    } else {
      A = GST_READ_UINT16_BE (sa + i) << (16 - info->bits);
      Y = GST_READ_UINT16_BE (sy + i) << (16 - info->bits);
      U = GST_READ_UINT16_BE (su + (i >> 1)) << (16 - info->bits);
      V = GST_READ_UINT16_BE (sv + (i >> 1)) << (16 - info->bits);
    }

    if (!(flags & GST_VIDEO_PACK_FLAG_TRUNCATE_RANGE)) {
      A |= (A >> info->bits);
      Y |= (Y >> info->bits);
      U |= (U >> info->bits);
      V |= (V >> info->bits);
    }

    d[i * 4 + 0] = A;
    d[i * 4 + 1] = Y;
    d[i * 4 + 2] = U;
    d[i * 4 + 3] = V;

    if (x & 1) {
      x = 0;
      su++;
      sv++;
    }
  }
}

static void
pack_A422_16 (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  int i;
  guint16 *restrict da = GET_A_LINE (y);
  guint16 *restrict dy = GET_Y_LINE (y);
  guint16 *restrict du = GET_U_LINE (y);
  guint16 *restrict dv = GET_V_LINE (y);
  guint16 A0, Y0, A1, Y1, U, V;
  const guint16 *restrict s = src;

  for (i = 0; i < width - 1; i += 2) {
    A0 = s[i * 4 + 0] >> (16 - info->bits);
    Y0 = s[i * 4 + 1] >> (16 - info->bits);
    A1 = s[i * 4 + 4] >> (16 - info->bits);
    Y1 = s[i * 4 + 5] >> (16 - info->bits);
    U = s[i * 4 + 2] >> (16 - info->bits);
    V = s[i * 4 + 3] >> (16 - info->bits);

    if (info->flags & GST_VIDEO_FORMAT_FLAG_LE) {
      GST_WRITE_UINT16_LE (da + i + 0, A0);
      GST_WRITE_UINT16_LE (dy + i + 0, Y0);
      GST_WRITE_UINT16_LE (da + i + 1, A1);
      GST_WRITE_UINT16_LE (dy + i + 1, Y1);
      GST_WRITE_UINT16_LE (du + (i >> 1), U);
      GST_WRITE_UINT16_LE (dv + (i >> 1), V);
    } else {
      GST_WRITE_UINT16_BE (da + i + 0, A0);
      GST_WRITE_UINT16_BE (dy + i + 0, Y0);
      GST_WRITE_UINT16_BE (da + i + 1, A1);
      GST_WRITE_UINT16_BE (dy + i + 1, Y1);
      GST_WRITE_UINT16_BE (du + (i >> 1), U);
      GST_WRITE_UINT16_BE (dv + (i >> 1), V);
    }
  }

  if (i == width - 1) {
    A0 = s[i * 4 + 0] >> (16 - info->bits);
    Y0 = s[i * 4 + 1] >> (16 - info->bits);
    U = s[i * 4 + 2] >> (16 - info->bits);
    V = s[i * 4 + 3] >> (16 - info->bits);

    if (info->flags & GST_VIDEO_FORMAT_FLAG_LE) {
      GST_WRITE_UINT16_LE (da + i, A0);
      GST_WRITE_UINT16_LE (dy + i, Y0);
      GST_WRITE_UINT16_LE (du + (i >> 1), U);
      GST_WRITE_UINT16_LE (dv + (i >> 1), V);
    } else {
      GST_WRITE_UINT16_BE (da + i, A0);
      GST_WRITE_UINT16_BE (dy + i, Y0);
      GST_WRITE_UINT16_BE (du + (i >> 1), U);
      GST_WRITE_UINT16_BE (dv + (i >> 1), V);
    }
  }
}

#define PACK_A422_10LE GST_VIDEO_FORMAT_AYUV64, unpack_A422_16, 1, pack_A422_16
#define PACK_A422_10BE GST_VIDEO_FORMAT_AYUV64, unpack_A422_16, 1, pack_A422_16
#define PACK_A422_12LE GST_VIDEO_FORMAT_AYUV64, unpack_A422_16, 1, pack_A422_16
#define PACK_A422_12BE GST_VIDEO_FORMAT_AYUV64, unpack_A422_16, 1, pack_A422_16
#define PACK_A422_16LE GST_VIDEO_FORMAT_AYUV64, unpack_A422_16, 1, pack_A422_16
#define PACK_A422_16BE GST_VIDEO_FORMAT_AYUV64, unpack_A422_16, 1, pack_A422_16

/* Helpers for the most common tile format */
static void
get_tile_NV12 (const GstVideoFormatInfo * info, gint tx, gint ty,
    const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES],
    gpointer tile_data[GST_VIDEO_MAX_PLANES],
    gint tile_stride[GST_VIDEO_MAX_PLANES])
{
  GstVideoTileMode mode = GST_VIDEO_FORMAT_INFO_TILE_MODE (info);
  gsize index, offset;

  /* index of Y tile */
  index = gst_video_tile_get_index (mode, tx, ty,
      GST_VIDEO_TILE_X_TILES (stride[0]), GST_VIDEO_TILE_Y_TILES (stride[0]));
  offset = index * GST_VIDEO_FORMAT_INFO_TILE_SIZE (info, 0);
  tile_data[0] = ((guint8 *) data[0]) + offset;

  index = gst_video_tile_get_index (mode, tx,
      GST_VIDEO_FORMAT_INFO_HAS_SUBTILES (info) ? ty : ty / 2,
      GST_VIDEO_TILE_X_TILES (stride[1]), GST_VIDEO_TILE_Y_TILES (stride[1]));
  offset = index * GST_VIDEO_FORMAT_INFO_TILE_SIZE (info, 1);

  if (!GST_VIDEO_FORMAT_INFO_HAS_SUBTILES (info) && (ty & 1))
    offset += GST_VIDEO_FORMAT_INFO_TILE_SIZE (info, 1) / 2;

  tile_data[1] = ((guint8 *) data[1]) + offset;

  tile_stride[0] = GST_VIDEO_FORMAT_INFO_TILE_STRIDE (info, 0);
  tile_stride[1] = GST_VIDEO_FORMAT_INFO_TILE_STRIDE (info, 1);
}

static inline void
unpack_TILED (GstVideoFormat inner_format,
    const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  const GstVideoFormatInfo *unpack_info, *tile_finfo;
  guint8 *line = dest;
  gint tile_width, tile_height;
  gint ntx, tx, ty;
  gint unpack_pstride;

  tile_width = GST_VIDEO_FORMAT_INFO_TILE_WIDTH (info, 0);
  tile_height = GST_VIDEO_FORMAT_INFO_TILE_HEIGHT (info, 0);

  /* we reuse these unpack functions */
  tile_finfo = gst_video_format_get_info (inner_format);

  /* get pstride of unpacked format */
  unpack_info = gst_video_format_get_info (info->unpack_format);
  unpack_pstride = GST_VIDEO_FORMAT_INFO_PSTRIDE (unpack_info, 0);

  /* first x tile to convert */
  tx = x / tile_width;
  /* Last tile to convert */
  ntx = ((x + width - 1) / tile_width) + 1;
  /* The row we are going to convert */
  ty = y / tile_height;

  /* y position in a tile */
  y = y % tile_height;
  /* x position in a tile */
  x = x % tile_width;

  for (; tx < ntx; tx++) {
    gpointer tdata[GST_VIDEO_MAX_PLANES];
    gint tstride[GST_VIDEO_MAX_PLANES];
    gint unpack_width;

    get_tile_NV12 (info, tx, ty, data, stride, tdata, tstride);

    /* the number of pixels left to unpack */
    unpack_width = MIN (width - x, tile_width - x);

    tile_finfo->unpack_func (tile_finfo, flags, line, tdata, tstride, x, y,
        unpack_width);

    x = 0;
    width -= unpack_width;
    line += unpack_width * unpack_pstride;
  }
}

static inline void
pack_TILED (GstVideoFormat inner_format,
    const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  const GstVideoFormatInfo *pack_info, *tile_finfo;
  guint8 *line = src;
  gint tile_width, tile_height;
  gint ntx, tx, ty;
  gint pack_pstride;

  tile_width = GST_VIDEO_FORMAT_INFO_TILE_WIDTH (info, 0);
  tile_height = GST_VIDEO_FORMAT_INFO_TILE_HEIGHT (info, 0);

  /* we reuse these pack functions */
  tile_finfo = gst_video_format_get_info (inner_format);

  /* get pstride of packed format */
  pack_info = gst_video_format_get_info (info->unpack_format);
  pack_pstride = GST_VIDEO_FORMAT_INFO_PSTRIDE (pack_info, 0);

  /* Last tile to convert */
  ntx = ((width - 1) / tile_width) + 1;
  /* The row we are going to convert */
  ty = y / tile_height;

  /* y position in a tile */
  y = y % tile_height;

  for (tx = 0; tx < ntx; tx++) {
    gpointer tdata[GST_VIDEO_MAX_PLANES];
    gint tstride[GST_VIDEO_MAX_PLANES];
    gint pack_width;

    get_tile_NV12 (info, tx, ty, data, stride, tdata, tstride);

    /* the number of pixels left to pack */
    pack_width = MIN (width, tile_width);

    tile_finfo->pack_func (tile_finfo, flags, line, sstride, tdata, tstride,
        chroma_site, y, pack_width);

    width -= pack_width;
    line += pack_width * pack_pstride;
  }
}

#define PACK_NV12_TILED GST_VIDEO_FORMAT_AYUV, unpack_NV12_TILED, 1, pack_NV12_TILED
static void
unpack_NV12_TILED (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  unpack_TILED (GST_VIDEO_FORMAT_NV12, info, flags, dest, data, stride,
      x, y, width);
}

static void
pack_NV12_TILED (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  pack_TILED (GST_VIDEO_FORMAT_NV12, info, flags, src, sstride, data, stride,
      chroma_site, y, width);
}


#define PACK_P010_10BE GST_VIDEO_FORMAT_AYUV64, unpack_P010_10BE, 1, pack_P010_10BE
static void
unpack_P010_10BE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  int i;
  gint uv = GET_UV_420 (y, flags);
  const guint16 *restrict sy = GET_PLANE_LINE (0, y);
  const guint16 *restrict suv = GET_PLANE_LINE (1, uv);
  guint16 *restrict d = dest, Y0, Y1, U, V;

  sy += x;
  suv += (x & ~1);

  if (x & 1) {
    Y0 = GST_READ_UINT16_BE (sy);
    U = GST_READ_UINT16_BE (suv);
    V = GST_READ_UINT16_BE (suv + 1);

    if (!(flags & GST_VIDEO_PACK_FLAG_TRUNCATE_RANGE)) {
      Y0 |= (Y0 >> 10);
      U |= (U >> 10);
      V |= (V >> 10);
    }

    d[0] = 0xffff;
    d[1] = Y0;
    d[2] = U;
    d[3] = V;
    width--;
    d += 4;
    sy += 1;
    suv += 2;
  }

  for (i = 0; i < width / 2; i++) {
    Y0 = GST_READ_UINT16_BE (sy + 2 * i);
    Y1 = GST_READ_UINT16_BE (sy + 2 * i + 1);
    U = GST_READ_UINT16_BE (suv + 2 * i);
    V = GST_READ_UINT16_BE (suv + 2 * i + 1);

    if (!(flags & GST_VIDEO_PACK_FLAG_TRUNCATE_RANGE)) {
      Y0 |= (Y0 >> 10);
      Y1 |= (Y1 >> 10);
      U |= (U >> 10);
      V |= (V >> 10);
    }

    d[i * 8 + 0] = 0xffff;
    d[i * 8 + 1] = Y0;
    d[i * 8 + 2] = U;
    d[i * 8 + 3] = V;
    d[i * 8 + 4] = 0xffff;
    d[i * 8 + 5] = Y1;
    d[i * 8 + 6] = U;
    d[i * 8 + 7] = V;
  }

  if (width & 1) {
    gint i = width - 1;

    Y0 = GST_READ_UINT16_BE (sy + i);
    U = GST_READ_UINT16_BE (suv + i);
    V = GST_READ_UINT16_BE (suv + i + 1);

    if (!(flags & GST_VIDEO_PACK_FLAG_TRUNCATE_RANGE)) {
      Y0 |= (Y0 >> 10);
      U |= (U >> 10);
      V |= (V >> 10);
    }

    d[i * 4 + 0] = 0xffff;
    d[i * 4 + 1] = Y0;
    d[i * 4 + 2] = U;
    d[i * 4 + 3] = V;
  }
}

static void
pack_P010_10BE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  int i;
  gint uv = GET_UV_420 (y, flags);
  guint16 *restrict dy = GET_PLANE_LINE (0, y);
  guint16 *restrict duv = GET_PLANE_LINE (1, uv);
  guint16 Y0, Y1, U, V;
  const guint16 *restrict s = src;

  if (IS_CHROMA_LINE_420 (y, flags)) {
    for (i = 0; i < width / 2; i++) {
      Y0 = s[i * 8 + 1] & 0xffc0;
      Y1 = s[i * 8 + 5] & 0xffc0;
      U = s[i * 8 + 2] & 0xffc0;
      V = s[i * 8 + 3] & 0xffc0;

      GST_WRITE_UINT16_BE (dy + i * 2 + 0, Y0);
      GST_WRITE_UINT16_BE (dy + i * 2 + 1, Y1);
      GST_WRITE_UINT16_BE (duv + i * 2 + 0, U);
      GST_WRITE_UINT16_BE (duv + i * 2 + 1, V);
    }
    if (width & 1) {
      gint i = width - 1;

      Y0 = s[i * 4 + 1] & 0xffc0;
      U = s[i * 4 + 2] & 0xffc0;
      V = s[i * 4 + 3] & 0xffc0;

      GST_WRITE_UINT16_BE (dy + i, Y0);
      GST_WRITE_UINT16_BE (duv + i + 0, U);
      GST_WRITE_UINT16_BE (duv + i + 1, V);
    }
  } else {
    for (i = 0; i < width; i++) {
      Y0 = s[i * 4 + 1] & 0xffc0;
      GST_WRITE_UINT16_BE (dy + i, Y0);
    }
  }
}

#define PACK_P010_10LE GST_VIDEO_FORMAT_AYUV64, unpack_P010_10LE, 1, pack_P010_10LE
static void
unpack_P010_10LE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  int i;
  gint uv = GET_UV_420 (y, flags);
  const guint16 *restrict sy = GET_PLANE_LINE (0, y);
  const guint16 *restrict suv = GET_PLANE_LINE (1, uv);
  guint16 *restrict d = dest, Y0, Y1, U, V;

  sy += x;
  suv += (x & ~1);

  if (x & 1) {
    Y0 = GST_READ_UINT16_LE (sy);
    U = GST_READ_UINT16_LE (suv);
    V = GST_READ_UINT16_LE (suv + 1);

    if (!(flags & GST_VIDEO_PACK_FLAG_TRUNCATE_RANGE)) {
      Y0 |= (Y0 >> 10);
      U |= (U >> 10);
      V |= (V >> 10);
    }

    d[0] = 0xffff;
    d[1] = Y0;
    d[2] = U;
    d[3] = V;
    width--;
    d += 4;
    sy += 1;
    suv += 2;
  }

  for (i = 0; i < width / 2; i++) {
    Y0 = GST_READ_UINT16_LE (sy + 2 * i);
    Y1 = GST_READ_UINT16_LE (sy + 2 * i + 1);
    U = GST_READ_UINT16_LE (suv + 2 * i);
    V = GST_READ_UINT16_LE (suv + 2 * i + 1);

    if (!(flags & GST_VIDEO_PACK_FLAG_TRUNCATE_RANGE)) {
      Y0 |= (Y0 >> 10);
      Y1 |= (Y1 >> 10);
      U |= (U >> 10);
      V |= (V >> 10);
    }

    d[i * 8 + 0] = 0xffff;
    d[i * 8 + 1] = Y0;
    d[i * 8 + 2] = U;
    d[i * 8 + 3] = V;
    d[i * 8 + 4] = 0xffff;
    d[i * 8 + 5] = Y1;
    d[i * 8 + 6] = U;
    d[i * 8 + 7] = V;
  }

  if (width & 1) {
    gint i = width - 1;

    Y0 = GST_READ_UINT16_LE (sy + i);
    U = GST_READ_UINT16_LE (suv + i);
    V = GST_READ_UINT16_LE (suv + i + 1);

    if (!(flags & GST_VIDEO_PACK_FLAG_TRUNCATE_RANGE)) {
      Y0 |= (Y0 >> 10);
      U |= (U >> 10);
      V |= (V >> 10);
    }

    d[i * 4 + 0] = 0xffff;
    d[i * 4 + 1] = Y0;
    d[i * 4 + 2] = U;
    d[i * 4 + 3] = V;
  }
}

static void
pack_P010_10LE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  int i;
  gint uv = GET_UV_420 (y, flags);
  guint16 *restrict dy = GET_PLANE_LINE (0, y);
  guint16 *restrict duv = GET_PLANE_LINE (1, uv);
  guint16 Y0, Y1, U, V;
  const guint16 *restrict s = src;

  if (IS_CHROMA_LINE_420 (y, flags)) {
    for (i = 0; i < width / 2; i++) {
      Y0 = s[i * 8 + 1] & 0xffc0;
      Y1 = s[i * 8 + 5] & 0xffc0;
      U = s[i * 8 + 2] & 0xffc0;
      V = s[i * 8 + 3] & 0xffc0;

      GST_WRITE_UINT16_LE (dy + i * 2 + 0, Y0);
      GST_WRITE_UINT16_LE (dy + i * 2 + 1, Y1);
      GST_WRITE_UINT16_LE (duv + i * 2 + 0, U);
      GST_WRITE_UINT16_LE (duv + i * 2 + 1, V);
    }
    if (width & 1) {
      gint i = width - 1;

      Y0 = s[i * 4 + 1] & 0xffc0;
      U = s[i * 4 + 2] & 0xffc0;
      V = s[i * 4 + 3] & 0xffc0;

      GST_WRITE_UINT16_LE (dy + i, Y0);
      GST_WRITE_UINT16_LE (duv + i + 0, U);
      GST_WRITE_UINT16_LE (duv + i + 1, V);
    }
  } else {
    for (i = 0; i < width; i++) {
      Y0 = s[i * 4 + 1] & 0xffc0;
      GST_WRITE_UINT16_LE (dy + i, Y0);
    }
  }
}

#define PACK_GRAY10_LE32 GST_VIDEO_FORMAT_AYUV64, unpack_GRAY10_LE32, 1, pack_GRAY10_LE32
static void
unpack_GRAY10_LE32 (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  gint i;
  const guint32 *restrict sy = GET_PLANE_LINE (0, y);
  guint16 *restrict d = dest;
  gint num_words = (width + 2) / 3;

  /* Y data is packed into little endian 32bit words, with the 2 MSB being
   * padding. There is only 1 pattern.
   * -> padding | Y1 | Y2 | Y3
   */

  for (i = 0; i < num_words; i++) {
    gint num_comps = MIN (3, width - i * 3);
    guint pix = i * 3;
    gsize doff = pix * 4;
    gint c;
    guint32 Y;

    Y = GST_READ_UINT32_LE (sy + i);

    for (c = 0; c < num_comps; c++) {
      guint16 Yn;

      /* For Y, we simply read 10 bit and shift it out */
      Yn = (Y & 0x03ff) << 6;
      Y >>= 10;

      if (G_UNLIKELY (pix + c < x))
        continue;

      if (!(flags & GST_VIDEO_PACK_FLAG_TRUNCATE_RANGE))
        Yn |= Yn >> 10;

      d[doff + 0] = 0xffff;
      d[doff + 1] = Yn;
      d[doff + 2] = 0x8000;
      d[doff + 3] = 0x8000;

      doff += 4;
    }
  }
}

static void
pack_GRAY10_LE32 (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  gint i;
  guint32 *restrict dy = GET_PLANE_LINE (0, y);
  const guint16 *restrict s = src;
  gint num_words = (width + 2) / 3;

  for (i = 0; i < num_words; i++) {
    gint num_comps = MIN (3, width - i * 3);
    guint pix = i * 3;
    gsize soff = pix * 4;
    gint c;
    guint32 Y = 0;

    for (c = 0; c < num_comps; c++) {
      Y |= s[soff + 1] >> 6 << (10 * c);
      soff += 4;
    }

    GST_WRITE_UINT32_LE (dy + i, Y);
  }
}

#define PACK_NV12_10LE32 GST_VIDEO_FORMAT_AYUV64, unpack_NV12_10LE32, 1, pack_NV12_10LE32
static void
unpack_NV12_10LE32 (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  gint i;
  gint uv = GET_UV_420 (y, flags);
  const guint32 *restrict sy = GET_PLANE_LINE (0, y);
  const guint32 *restrict suv = GET_PLANE_LINE (1, uv);
  guint16 *restrict d = dest;
  gint num_words = (width + 2) / 3;
  guint32 UV = 0;
  guint16 Un = 0, Vn = 0;

  /* Y data is packed into little endian 32bit words, with the 2 MSB being
   * padding. There is only 1 pattern.
   * -> padding | Y1 | Y2 | Y3
   *
   * UV is packed the same way, though we end up with 2 patterns:
   * -> U | V | U | padding
   * -> V | U | V | padding
   */

  /* FIXME unroll the 6 states ? */

  for (i = 0; i < num_words; i++) {
    gint num_comps = MIN (3, width - i * 3);
    guint pix = i * 3;
    gsize doff = pix * 4;
    gint c;
    guint32 Y;

    Y = GST_READ_UINT32_LE (sy + i);

    for (c = 0; c < num_comps; c++) {
      guint16 Yn;

      /* For Y, we simply read 10 bit and shift it out */
      Yn = (Y & 0x03ff) << 6;
      Y >>= 10;

      /* Unpacking UV has been reduced to a cycle of 6 states. The following
       * code is a reduce version of:
       * 0: - Read first UV word (UVU)
       *      Unpack U and V
       * 1: - Reused U/V from 1 (sub-sampling)
       * 2: - Unpack remaining U value
       *    - Read following UV word (VUV)
       *    - Unpack V value
       * 3: - Reuse U/V from 2 (sub-sampling)
       * 4: - Unpack remaining U
       *    - Unpack remaining V
       * 5: - Reuse UV/V from 4 (sub-sampling)
       */
      switch ((pix + c) % 6) {
        case 0:
          UV = GST_READ_UINT32_LE (suv + i);
          /* fallthrough */
        case 4:
          Un = (UV & 0x03ff) << 6;
          UV >>= 10;
          Vn = (UV & 0x03ff) << 6;
          UV >>= 10;
          break;
        case 2:
          Un = (UV & 0x03ff) << 6;
          UV = GST_READ_UINT32_LE (suv + i + 1);
          Vn = (UV & 0x03ff) << 6;
          UV >>= 10;
          break;
        default:
          /* keep value */
          break;
      }

      if (G_UNLIKELY (pix + c < x))
        continue;

      if (!(flags & GST_VIDEO_PACK_FLAG_TRUNCATE_RANGE)) {
        Yn |= Yn >> 10;
        Un |= Un >> 10;
        Vn |= Vn >> 10;
      }

      d[doff + 0] = 0xffff;
      d[doff + 1] = Yn;
      d[doff + 2] = Un;
      d[doff + 3] = Vn;

      doff += 4;
    }
  }
}

static void
pack_NV12_10LE32 (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  gint i;
  gint uv = GET_UV_420 (y, flags);
  guint32 *restrict dy = GET_PLANE_LINE (0, y);
  guint32 *restrict duv = GET_PLANE_LINE (1, uv);
  const guint16 *restrict s = src;
  gint num_words = (width + 2) / 3;
  guint32 UV = 0;

  /* FIXME unroll the 6 states ? */

  for (i = 0; i < num_words; i++) {
    gint num_comps = MIN (3, width - i * 3);
    guint pix = i * 3;
    gsize soff = pix * 4;
    gint c;
    guint32 Y = 0;

    for (c = 0; c < num_comps; c++) {
      Y |= s[soff + 1] >> 6 << (10 * c);

      if (IS_CHROMA_LINE_420 (y, flags)) {
        switch ((pix + c) % 6) {
          case 0:
            UV = s[soff + 2] >> 6;
            UV |= s[soff + 3] >> 6 << 10;
            break;
          case 2:
            UV |= s[soff + 2] >> 6 << 20;
            GST_WRITE_UINT32_LE (duv + i, UV);
            UV = s[soff + 3] >> 6;
            break;
          case 4:
            UV |= s[soff + 2] >> 6 << 10;
            UV |= s[soff + 3] >> 6 << 20;
            GST_WRITE_UINT32_LE (duv + i, UV);
            break;
          default:
            /* keep value */
            break;
        }
      }

      soff += 4;
    }

    GST_WRITE_UINT32_LE (dy + i, Y);

    if (IS_CHROMA_LINE_420 (y, flags) && num_comps < 3)
      GST_WRITE_UINT32_LE (duv + i, UV);

  }
}

#define PACK_NV16_10LE32 GST_VIDEO_FORMAT_AYUV64, unpack_NV16_10LE32, 1, pack_NV16_10LE32
static void
unpack_NV16_10LE32 (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  gint i;
  const guint32 *restrict sy = GET_PLANE_LINE (0, y);
  const guint32 *restrict suv = GET_PLANE_LINE (1, y);
  guint16 *restrict d = dest;
  gint num_words = (width + 2) / 3;
  guint32 UV = 0;
  guint16 Un = 0, Vn = 0;

  /* Y data is packed into little endian 32bit words, with the 2 MSB being
   * padding. There is only 1 pattern.
   * -> padding | Y1 | Y2 | Y3
   *
   * UV is packed the same way, though we end up with 2 patterns:
   * -> U | V | U | padding
   * -> V | U | V | padding
   */

  /* FIXME unroll the 6 states ? */

  for (i = 0; i < num_words; i++) {
    gint num_comps = MIN (3, width - i * 3);
    guint pix = i * 3;
    gsize doff = pix * 4;
    gint c;
    guint32 Y;

    Y = GST_READ_UINT32_LE (sy + i);

    for (c = 0; c < num_comps; c++) {
      guint16 Yn;

      /* For Y, we simply read 10 bit and shift it out */
      Yn = (Y & 0x03ff) << 6;
      Y >>= 10;

      /* Unpacking UV has been reduced to a cycle of 6 states. The following
       * code is a reduce version of:
       * 0: - Read first UV word (UVU)
       *      Unpack U and V
       * 1: - Reused U/V from 1 (sub-sampling)
       * 2: - Unpack remaining U value
       *    - Read following UV word (VUV)
       *    - Unpack V value
       * 3: - Reuse U/V from 2 (sub-sampling)
       * 4: - Unpack remaining U
       *    - Unpack remaining V
       * 5: - Reuse UV/V from 4 (sub-sampling)
       */
      switch ((pix + c) % 6) {
        case 0:
          UV = GST_READ_UINT32_LE (suv + i);
          /* fallthrough */
        case 4:
          Un = (UV & 0x03ff) << 6;
          UV >>= 10;
          Vn = (UV & 0x03ff) << 6;
          UV >>= 10;
          break;
        case 2:
          Un = (UV & 0x03ff) << 6;
          UV = GST_READ_UINT32_LE (suv + i + 1);
          Vn = (UV & 0x03ff) << 6;
          UV >>= 10;
          break;
        default:
          /* keep value */
          break;
      }

      if (G_UNLIKELY (pix + c < x))
        continue;

      if (!(flags & GST_VIDEO_PACK_FLAG_TRUNCATE_RANGE)) {
        Yn |= Yn >> 10;
        Un |= Un >> 10;
        Vn |= Vn >> 10;
      }

      d[doff + 0] = 0xffff;
      d[doff + 1] = Yn;
      d[doff + 2] = Un;
      d[doff + 3] = Vn;

      doff += 4;
    }
  }
}

static void
pack_NV16_10LE32 (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  gint i;
  guint32 *restrict dy = GET_PLANE_LINE (0, y);
  guint32 *restrict duv = GET_PLANE_LINE (1, y);
  const guint16 *restrict s = src;
  gint num_words = (width + 2) / 3;
  guint32 UV = 0;

  /* FIXME unroll the 6 states ? */

  for (i = 0; i < num_words; i++) {
    gint num_comps = MIN (3, width - i * 3);
    guint pix = i * 3;
    gsize soff = pix * 4;
    gint c;
    guint32 Y = 0;

    for (c = 0; c < num_comps; c++) {
      Y |= s[soff + 1] >> 6 << (10 * c);

      switch ((pix + c) % 6) {
        case 0:
          UV = s[soff + 2] >> 6;
          UV |= s[soff + 3] >> 6 << 10;
          break;
        case 2:
          UV |= s[soff + 2] >> 6 << 20;
          GST_WRITE_UINT32_LE (duv + i, UV);
          UV = s[soff + 3] >> 6;
          break;
        case 4:
          UV |= s[soff + 2] >> 6 << 10;
          UV |= s[soff + 3] >> 6 << 20;
          GST_WRITE_UINT32_LE (duv + i, UV);
          break;
        default:
          /* keep value */
          break;
      }

      soff += 4;
    }

    GST_WRITE_UINT32_LE (dy + i, Y);

    if (num_comps < 3)
      GST_WRITE_UINT32_LE (duv + i, UV);
  }
}

#define PACK_NV12_10LE40 GST_VIDEO_FORMAT_AYUV64, unpack_NV12_10LE40, 1, pack_NV12_10LE40
static void
unpack_NV12_10LE40 (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  gint i;
  gint uv = GET_UV_420 (y, flags);
  guint16 *restrict d = dest;
  const guint8 *restrict sy = GET_PLANE_LINE (0, y);
  const guint8 *restrict suv = GET_PLANE_LINE (1, uv);
  guint16 Y0 = 0, Y1 = 0, Yn = 0, Un = 0, Vn = 0;
  guint32 UV = 0;

  for (i = 0; i < width; i++) {
    gboolean update_c = FALSE;

    switch (i & 3) {
      case 0:
        Y0 = GST_READ_UINT16_LE (sy);
        Yn = Y0 & 0x3ff;
        sy += 2;

        UV = GST_READ_UINT32_LE (suv);
        Un = UV & 0x3ff;
        Vn = (UV >> 10) & 0x3ff;
        suv += 4;

        Yn <<= 6;
        Un <<= 6;
        Vn <<= 6;
        update_c = TRUE;
        break;
      case 1:
        Y1 = GST_READ_UINT16_LE (sy);
        Yn = (Y0 >> 10) | ((Y1 & 0xf) << 6);
        sy += 2;

        Yn <<= 6;
        break;
      case 2:
        Yn = (Y1 >> 4) & 0x3ff;

        Un = (UV >> 20) & 0x3ff;
        Vn = (UV >> 30);
        UV = GST_READ_UINT8 (suv);
        Vn |= (UV << 2);
        suv++;

        Yn <<= 6;
        Un <<= 6;
        Vn <<= 6;
        update_c = TRUE;
        break;
      case 3:
        Y0 = GST_READ_UINT8 (sy);
        Yn = (Y1 >> 14) | (Y0 << 2);
        sy++;

        Yn <<= 6;
        break;
    }

    if (!(flags & GST_VIDEO_PACK_FLAG_TRUNCATE_RANGE)) {
      Yn |= Yn >> 10;
      if (update_c) {
        Un |= Un >> 10;
        Vn |= Vn >> 10;
      }
    }

    d[i * 4 + 0] = 0xffff;
    d[i * 4 + 1] = Yn;
    d[i * 4 + 2] = Un;
    d[i * 4 + 3] = Vn;
  }
}

static void
pack_NV12_10LE40 (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  gint i;
  gint uv = GET_UV_420 (y, flags);
  guint8 *restrict dy = GET_PLANE_LINE (0, y);
  guint8 *restrict duv = GET_PLANE_LINE (1, uv);
  guint16 Y0 = 0, Y1 = 0, Y2 = 0, Y3 = 0, U0, V0 = 0, U1 = 0, V1 = 0;
  const guint16 *restrict s = src;

  for (i = 0; i < width; i++) {
    switch (i & 3) {
      case 0:
        Y0 = s[i * 4 + 1] >> 6;
        GST_WRITE_UINT8 (dy, Y0 & 0xff);
        dy++;

        if (IS_CHROMA_LINE_420 (y, flags)) {
          U0 = s[i * 4 + 2] >> 6;
          V0 = s[i * 4 + 3] >> 6;

          GST_WRITE_UINT8 (duv, U0 & 0xff);
          duv++;

          GST_WRITE_UINT8 (duv, (U0 >> 8) | ((V0 & 0x3f) << 2));
          duv++;
        }
        break;
      case 1:
        Y1 = s[i * 4 + 1] >> 6;
        GST_WRITE_UINT8 (dy, (Y0 >> 8) | ((Y1 & 0x3f) << 2));
        dy++;
        break;
      case 2:
        Y2 = s[i * 4 + 1] >> 6;
        GST_WRITE_UINT8 (dy, (Y1 >> 6) | ((Y2 & 0xf) << 4));
        dy++;

        if (IS_CHROMA_LINE_420 (y, flags)) {
          U1 = s[i * 4 + 2] >> 6;
          V1 = s[i * 4 + 3] >> 6;

          GST_WRITE_UINT8 (duv, (V0 >> 6) | ((U1 & 0xf) << 4));
          duv++;

          GST_WRITE_UINT8 (duv, (U1 >> 4) | ((V1 & 0x3) << 6));
          duv++;

          GST_WRITE_UINT8 (duv, V1 >> 2);
          duv++;
        }
        break;
      case 3:
        Y3 = s[i * 4 + 1] >> 6;
        GST_WRITE_UINT8 (dy, (Y2 >> 4) | ((Y3 & 0x3) << 6));
        dy++;
        GST_WRITE_UINT8 (dy, (Y3 >> 2));
        dy++;
        break;
    }
  }

  switch (width & 3) {
    case 0:
      break;
    case 1:
      GST_WRITE_UINT8 (dy, Y0 >> 8);
      if (IS_CHROMA_LINE_420 (y, flags))
        GST_WRITE_UINT8 (duv, V0 >> 6);
      break;
    case 2:
      GST_WRITE_UINT8 (dy, Y1 >> 6);
      if (IS_CHROMA_LINE_420 (y, flags))
        GST_WRITE_UINT8 (duv, V0 >> 6);
      break;
    case 3:
      GST_WRITE_UINT8 (dy, Y2 >> 4);
      break;
  }
}

#define PACK_VUYA GST_VIDEO_FORMAT_AYUV, unpack_VUYA, 1, pack_VUYA
static void
unpack_VUYA (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  const guint8 *restrict s = GET_LINE (y);
  guint8 *restrict d = dest;

  s += x * 4;

  video_orc_unpack_VUYA (d, s, width);
}

static void
pack_VUYA (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  const guint8 *restrict s = src;
  guint8 *restrict d = GET_LINE (y);

  video_orc_pack_VUYA (d, s, width);
}

#define PACK_BGR10A2_LE GST_VIDEO_FORMAT_ARGB64, unpack_bgr10a2_le, 1, pack_bgr10a2_le
static void
unpack_bgr10a2_le (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  int i;
  const guint8 *restrict s = GET_LINE (y);
  guint16 *restrict d = dest;
  guint32 ARGB;
  guint16 A, R, G, B;

  s += x * 4;

  for (i = 0; i < width; i++) {
    ARGB = GST_READ_UINT32_LE (s + 4 * i);

    B = ((ARGB >> 0) & 0x3ff) << 6;
    G = ((ARGB >> 10) & 0x3ff) << 6;
    R = ((ARGB >> 20) & 0x3ff) << 6;
    A = ((ARGB >> 30) & 0x03) << 14;

    if (!(flags & GST_VIDEO_PACK_FLAG_TRUNCATE_RANGE)) {
      B |= (B >> 10);
      G |= (G >> 10);
      R |= (R >> 10);
      A |= (A >> 10);
    }

    d[4 * i + 0] = A;
    d[4 * i + 1] = R;
    d[4 * i + 2] = G;
    d[4 * i + 3] = B;
  }
}

static void
pack_bgr10a2_le (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  int i;
  guint32 *restrict d = GET_LINE (y);
  const guint16 *restrict s = src;
  guint32 ARGB;
  guint16 A, R, G, B;

  for (i = 0; i < width; i++) {
    A = s[4 * i] & 0xc000;
    R = s[4 * i + 1] & 0xffc0;
    G = s[4 * i + 2] & 0xffc0;
    B = s[4 * i + 3] & 0xffc0;

    ARGB = (B >> 6) | (G << 4) | (R << 14) | (A << 16);

    GST_WRITE_UINT32_LE (d + i, ARGB);
  }
}

#define PACK_RGB10A2_LE GST_VIDEO_FORMAT_ARGB64, unpack_rgb10a2_le, 1, pack_rgb10a2_le
static void
unpack_rgb10a2_le (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  int i;
  const guint8 *restrict s = GET_LINE (y);
  guint16 *restrict d = dest;
  guint32 ARGB;
  guint16 A, R, G, B;

  s += x * 4;

  for (i = 0; i < width; i++) {
    ARGB = GST_READ_UINT32_LE (s + 4 * i);

    R = ((ARGB >> 0) & 0x3ff) << 6;
    G = ((ARGB >> 10) & 0x3ff) << 6;
    B = ((ARGB >> 20) & 0x3ff) << 6;
    A = ((ARGB >> 30) & 0x03) << 14;

    if (!(flags & GST_VIDEO_PACK_FLAG_TRUNCATE_RANGE)) {
      R |= (R >> 10);
      G |= (G >> 10);
      B |= (B >> 10);
      A |= (A >> 10);
    }

    d[4 * i + 0] = A;
    d[4 * i + 1] = R;
    d[4 * i + 2] = G;
    d[4 * i + 3] = B;
  }
}

static void
pack_rgb10a2_le (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  int i;
  guint32 *restrict d = GET_LINE (y);
  const guint16 *restrict s = src;
  guint32 ARGB;
  guint16 A, R, G, B;

  for (i = 0; i < width; i++) {
    A = s[4 * i] & 0xc000;
    R = s[4 * i + 1] & 0xffc0;
    G = s[4 * i + 2] & 0xffc0;
    B = s[4 * i + 3] & 0xffc0;

    ARGB = (R >> 6) | (G << 4) | (B << 14) | (A << 16);

    GST_WRITE_UINT32_LE (d + i, ARGB);
  }
}

#define PACK_Y444_16BE GST_VIDEO_FORMAT_AYUV64, unpack_Y444_16BE, 1, pack_Y444_16BE
static void
unpack_Y444_16BE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  int i;
  guint16 *restrict sy = GET_Y_LINE (y);
  guint16 *restrict su = GET_U_LINE (y);
  guint16 *restrict sv = GET_V_LINE (y);
  guint16 *restrict d = dest, Y, U, V;

  sy += x;
  su += x;
  sv += x;

  for (i = 0; i < width; i++) {
    Y = GST_READ_UINT16_BE (sy + i);
    U = GST_READ_UINT16_BE (su + i);
    V = GST_READ_UINT16_BE (sv + i);

    d[i * 4 + 0] = 0xffff;
    d[i * 4 + 1] = Y;
    d[i * 4 + 2] = U;
    d[i * 4 + 3] = V;
  }
}

static void
pack_Y444_16BE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  int i;
  guint16 *restrict dy = GET_Y_LINE (y);
  guint16 *restrict du = GET_U_LINE (y);
  guint16 *restrict dv = GET_V_LINE (y);
  guint16 Y, U, V;
  const guint16 *restrict s = src;

  for (i = 0; i < width; i++) {
    Y = s[i * 4 + 1];
    U = s[i * 4 + 2];
    V = s[i * 4 + 3];

    GST_WRITE_UINT16_BE (dy + i, Y);
    GST_WRITE_UINT16_BE (du + i, U);
    GST_WRITE_UINT16_BE (dv + i, V);
  }
}

#define PACK_Y444_16LE GST_VIDEO_FORMAT_AYUV64, unpack_Y444_16LE, 1, pack_Y444_16LE
static void
unpack_Y444_16LE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  int i;
  guint16 *restrict sy = GET_Y_LINE (y);
  guint16 *restrict su = GET_U_LINE (y);
  guint16 *restrict sv = GET_V_LINE (y);
  guint16 *restrict d = dest, Y, U, V;

  sy += x;
  su += x;
  sv += x;

  for (i = 0; i < width; i++) {
    Y = GST_READ_UINT16_LE (sy + i);
    U = GST_READ_UINT16_LE (su + i);
    V = GST_READ_UINT16_LE (sv + i);

    d[i * 4 + 0] = 0xffff;
    d[i * 4 + 1] = Y;
    d[i * 4 + 2] = U;
    d[i * 4 + 3] = V;
  }
}

static void
pack_Y444_16LE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  int i;
  guint16 *restrict dy = GET_Y_LINE (y);
  guint16 *restrict du = GET_U_LINE (y);
  guint16 *restrict dv = GET_V_LINE (y);
  guint16 Y, U, V;
  const guint16 *restrict s = src;

  for (i = 0; i < width; i++) {
    Y = s[i * 4 + 1];
    U = s[i * 4 + 2];
    V = s[i * 4 + 3];

    GST_WRITE_UINT16_LE (dy + i, Y);
    GST_WRITE_UINT16_LE (du + i, U);
    GST_WRITE_UINT16_LE (dv + i, V);
  }
}

#define PACK_P016_BE GST_VIDEO_FORMAT_AYUV64, unpack_P016_BE, 1, pack_P016_BE
static void
unpack_P016_BE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  int i;
  gint uv = GET_UV_420 (y, flags);
  const guint16 *restrict sy = GET_PLANE_LINE (0, y);
  const guint16 *restrict suv = GET_PLANE_LINE (1, uv);
  guint16 *restrict d = dest, Y0, Y1, U, V;

  sy += x;
  suv += (x & ~1);

  if (x & 1) {
    Y0 = GST_READ_UINT16_BE (sy);
    U = GST_READ_UINT16_BE (suv);
    V = GST_READ_UINT16_BE (suv + 1);

    d[0] = 0xffff;
    d[1] = Y0;
    d[2] = U;
    d[3] = V;
    width--;
    d += 4;
    sy += 1;
    suv += 2;
  }

  for (i = 0; i < width / 2; i++) {
    Y0 = GST_READ_UINT16_BE (sy + 2 * i);
    Y1 = GST_READ_UINT16_BE (sy + 2 * i + 1);
    U = GST_READ_UINT16_BE (suv + 2 * i);
    V = GST_READ_UINT16_BE (suv + 2 * i + 1);

    d[i * 8 + 0] = 0xffff;
    d[i * 8 + 1] = Y0;
    d[i * 8 + 2] = U;
    d[i * 8 + 3] = V;
    d[i * 8 + 4] = 0xffff;
    d[i * 8 + 5] = Y1;
    d[i * 8 + 6] = U;
    d[i * 8 + 7] = V;
  }

  if (width & 1) {
    gint i = width - 1;

    Y0 = GST_READ_UINT16_BE (sy + i);
    U = GST_READ_UINT16_BE (suv + i);
    V = GST_READ_UINT16_BE (suv + i + 1);

    d[i * 4 + 0] = 0xffff;
    d[i * 4 + 1] = Y0;
    d[i * 4 + 2] = U;
    d[i * 4 + 3] = V;
  }
}

static void
pack_P016_BE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  int i;
  gint uv = GET_UV_420 (y, flags);
  guint16 *restrict dy = GET_PLANE_LINE (0, y);
  guint16 *restrict duv = GET_PLANE_LINE (1, uv);
  guint16 Y0, Y1, U, V;
  const guint16 *restrict s = src;

  if (IS_CHROMA_LINE_420 (y, flags)) {
    for (i = 0; i < width / 2; i++) {
      Y0 = s[i * 8 + 1];
      Y1 = s[i * 8 + 5];
      U = s[i * 8 + 2];
      V = s[i * 8 + 3];

      GST_WRITE_UINT16_BE (dy + i * 2 + 0, Y0);
      GST_WRITE_UINT16_BE (dy + i * 2 + 1, Y1);
      GST_WRITE_UINT16_BE (duv + i * 2 + 0, U);
      GST_WRITE_UINT16_BE (duv + i * 2 + 1, V);
    }
    if (width & 1) {
      gint i = width - 1;

      Y0 = s[i * 4 + 1];
      U = s[i * 4 + 2];
      V = s[i * 4 + 3];

      GST_WRITE_UINT16_BE (dy + i, Y0);
      GST_WRITE_UINT16_BE (duv + i + 0, U);
      GST_WRITE_UINT16_BE (duv + i + 1, V);
    }
  } else {
    for (i = 0; i < width; i++) {
      Y0 = s[i * 4 + 1];
      GST_WRITE_UINT16_BE (dy + i, Y0);
    }
  }
}

#define PACK_P016_LE GST_VIDEO_FORMAT_AYUV64, unpack_P016_LE, 1, pack_P016_LE
static void
unpack_P016_LE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  int i;
  gint uv = GET_UV_420 (y, flags);
  const guint16 *restrict sy = GET_PLANE_LINE (0, y);
  const guint16 *restrict suv = GET_PLANE_LINE (1, uv);
  guint16 *restrict d = dest, Y0, Y1, U, V;

  sy += x;
  suv += (x & ~1);

  if (x & 1) {
    Y0 = GST_READ_UINT16_LE (sy);
    U = GST_READ_UINT16_LE (suv);
    V = GST_READ_UINT16_LE (suv + 1);

    d[0] = 0xffff;
    d[1] = Y0;
    d[2] = U;
    d[3] = V;
    width--;
    d += 4;
    sy += 1;
    suv += 2;
  }

  for (i = 0; i < width / 2; i++) {
    Y0 = GST_READ_UINT16_LE (sy + 2 * i);
    Y1 = GST_READ_UINT16_LE (sy + 2 * i + 1);
    U = GST_READ_UINT16_LE (suv + 2 * i);
    V = GST_READ_UINT16_LE (suv + 2 * i + 1);

    d[i * 8 + 0] = 0xffff;
    d[i * 8 + 1] = Y0;
    d[i * 8 + 2] = U;
    d[i * 8 + 3] = V;
    d[i * 8 + 4] = 0xffff;
    d[i * 8 + 5] = Y1;
    d[i * 8 + 6] = U;
    d[i * 8 + 7] = V;
  }

  if (width & 1) {
    gint i = width - 1;

    Y0 = GST_READ_UINT16_LE (sy + i);
    U = GST_READ_UINT16_LE (suv + i);
    V = GST_READ_UINT16_LE (suv + i + 1);

    d[i * 4 + 0] = 0xffff;
    d[i * 4 + 1] = Y0;
    d[i * 4 + 2] = U;
    d[i * 4 + 3] = V;
  }
}

static void
pack_P016_LE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  int i;
  gint uv = GET_UV_420 (y, flags);
  guint16 *restrict dy = GET_PLANE_LINE (0, y);
  guint16 *restrict duv = GET_PLANE_LINE (1, uv);
  guint16 Y0, Y1, U, V;
  const guint16 *restrict s = src;

  if (IS_CHROMA_LINE_420 (y, flags)) {
    for (i = 0; i < width / 2; i++) {
      Y0 = s[i * 8 + 1];
      Y1 = s[i * 8 + 5];
      U = s[i * 8 + 2];
      V = s[i * 8 + 3];

      GST_WRITE_UINT16_LE (dy + i * 2 + 0, Y0);
      GST_WRITE_UINT16_LE (dy + i * 2 + 1, Y1);
      GST_WRITE_UINT16_LE (duv + i * 2 + 0, U);
      GST_WRITE_UINT16_LE (duv + i * 2 + 1, V);
    }
    if (width & 1) {
      gint i = width - 1;

      Y0 = s[i * 4 + 1];
      U = s[i * 4 + 2];
      V = s[i * 4 + 3];

      GST_WRITE_UINT16_LE (dy + i, Y0);
      GST_WRITE_UINT16_LE (duv + i + 0, U);
      GST_WRITE_UINT16_LE (duv + i + 1, V);
    }
  } else {
    for (i = 0; i < width; i++) {
      Y0 = s[i * 4 + 1];
      GST_WRITE_UINT16_LE (dy + i, Y0);
    }
  }
}

#define PACK_P012_BE GST_VIDEO_FORMAT_AYUV64, unpack_P012_BE, 1, pack_P012_BE
static void
unpack_P012_BE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  int i;
  gint uv = GET_UV_420 (y, flags);
  const guint16 *restrict sy = GET_PLANE_LINE (0, y);
  const guint16 *restrict suv = GET_PLANE_LINE (1, uv);
  guint16 *restrict d = dest, Y0, Y1, U, V;

  sy += x;
  suv += (x & ~1);

  if (x & 1) {
    Y0 = GST_READ_UINT16_BE (sy);
    U = GST_READ_UINT16_BE (suv);
    V = GST_READ_UINT16_BE (suv + 1);

    if (!(flags & GST_VIDEO_PACK_FLAG_TRUNCATE_RANGE)) {
      Y0 |= (Y0 >> 12);
      U |= (U >> 12);
      V |= (V >> 12);
    }

    d[0] = 0xffff;
    d[1] = Y0;
    d[2] = U;
    d[3] = V;
    width--;
    d += 4;
    sy += 1;
    suv += 2;
  }

  for (i = 0; i < width / 2; i++) {
    Y0 = GST_READ_UINT16_BE (sy + 2 * i);
    Y1 = GST_READ_UINT16_BE (sy + 2 * i + 1);
    U = GST_READ_UINT16_BE (suv + 2 * i);
    V = GST_READ_UINT16_BE (suv + 2 * i + 1);

    if (!(flags & GST_VIDEO_PACK_FLAG_TRUNCATE_RANGE)) {
      Y0 |= (Y0 >> 12);
      Y1 |= (Y1 >> 12);
      U |= (U >> 12);
      V |= (V >> 12);
    }

    d[i * 8 + 0] = 0xffff;
    d[i * 8 + 1] = Y0;
    d[i * 8 + 2] = U;
    d[i * 8 + 3] = V;
    d[i * 8 + 4] = 0xffff;
    d[i * 8 + 5] = Y1;
    d[i * 8 + 6] = U;
    d[i * 8 + 7] = V;
  }

  if (width & 1) {
    gint i = width - 1;

    Y0 = GST_READ_UINT16_BE (sy + i);
    U = GST_READ_UINT16_BE (suv + i);
    V = GST_READ_UINT16_BE (suv + i + 1);

    if (!(flags & GST_VIDEO_PACK_FLAG_TRUNCATE_RANGE)) {
      Y0 |= (Y0 >> 12);
      U |= (U >> 12);
      V |= (V >> 12);
    }

    d[i * 4 + 0] = 0xffff;
    d[i * 4 + 1] = Y0;
    d[i * 4 + 2] = U;
    d[i * 4 + 3] = V;
  }
}

static void
pack_P012_BE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  int i;
  gint uv = GET_UV_420 (y, flags);
  guint16 *restrict dy = GET_PLANE_LINE (0, y);
  guint16 *restrict duv = GET_PLANE_LINE (1, uv);
  guint16 Y0, Y1, U, V;
  const guint16 *restrict s = src;

  if (IS_CHROMA_LINE_420 (y, flags)) {
    for (i = 0; i < width / 2; i++) {
      Y0 = s[i * 8 + 1] & 0xfff0;
      Y1 = s[i * 8 + 5] & 0xfff0;
      U = s[i * 8 + 2] & 0xfff0;
      V = s[i * 8 + 3] & 0xfff0;

      GST_WRITE_UINT16_BE (dy + i * 2 + 0, Y0);
      GST_WRITE_UINT16_BE (dy + i * 2 + 1, Y1);
      GST_WRITE_UINT16_BE (duv + i * 2 + 0, U);
      GST_WRITE_UINT16_BE (duv + i * 2 + 1, V);
    }
    if (width & 1) {
      gint i = width - 1;

      Y0 = s[i * 4 + 1] & 0xfff0;
      U = s[i * 4 + 2] & 0xfff0;
      V = s[i * 4 + 3] & 0xfff0;

      GST_WRITE_UINT16_BE (dy + i, Y0);
      GST_WRITE_UINT16_BE (duv + i + 0, U);
      GST_WRITE_UINT16_BE (duv + i + 1, V);
    }
  } else {
    for (i = 0; i < width; i++) {
      Y0 = s[i * 4 + 1] & 0xfff0;
      GST_WRITE_UINT16_BE (dy + i, Y0);
    }
  }
}

#define PACK_P012_LE GST_VIDEO_FORMAT_AYUV64, unpack_P012_LE, 1, pack_P012_LE
static void
unpack_P012_LE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  int i;
  gint uv = GET_UV_420 (y, flags);
  const guint16 *restrict sy = GET_PLANE_LINE (0, y);
  const guint16 *restrict suv = GET_PLANE_LINE (1, uv);
  guint16 *restrict d = dest, Y0, Y1, U, V;

  sy += x;
  suv += (x & ~1);

  if (x & 1) {
    Y0 = GST_READ_UINT16_LE (sy);
    U = GST_READ_UINT16_LE (suv);
    V = GST_READ_UINT16_LE (suv + 1);

    if (!(flags & GST_VIDEO_PACK_FLAG_TRUNCATE_RANGE)) {
      Y0 |= (Y0 >> 12);
      U |= (U >> 12);
      V |= (V >> 12);
    }

    d[0] = 0xffff;
    d[1] = Y0;
    d[2] = U;
    d[3] = V;
    width--;
    d += 4;
    sy += 1;
    suv += 2;
  }

  for (i = 0; i < width / 2; i++) {
    Y0 = GST_READ_UINT16_LE (sy + 2 * i);
    Y1 = GST_READ_UINT16_LE (sy + 2 * i + 1);
    U = GST_READ_UINT16_LE (suv + 2 * i);
    V = GST_READ_UINT16_LE (suv + 2 * i + 1);

    if (!(flags & GST_VIDEO_PACK_FLAG_TRUNCATE_RANGE)) {
      Y0 |= (Y0 >> 12);
      Y1 |= (Y1 >> 12);
      U |= (U >> 12);
      V |= (V >> 12);
    }

    d[i * 8 + 0] = 0xffff;
    d[i * 8 + 1] = Y0;
    d[i * 8 + 2] = U;
    d[i * 8 + 3] = V;
    d[i * 8 + 4] = 0xffff;
    d[i * 8 + 5] = Y1;
    d[i * 8 + 6] = U;
    d[i * 8 + 7] = V;
  }

  if (width & 1) {
    gint i = width - 1;

    Y0 = GST_READ_UINT16_LE (sy + i);
    U = GST_READ_UINT16_LE (suv + i);
    V = GST_READ_UINT16_LE (suv + i + 1);

    if (!(flags & GST_VIDEO_PACK_FLAG_TRUNCATE_RANGE)) {
      Y0 |= (Y0 >> 12);
      U |= (U >> 12);
      V |= (V >> 12);
    }

    d[i * 4 + 0] = 0xffff;
    d[i * 4 + 1] = Y0;
    d[i * 4 + 2] = U;
    d[i * 4 + 3] = V;
  }
}

static void
pack_P012_LE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  int i;
  gint uv = GET_UV_420 (y, flags);
  guint16 *restrict dy = GET_PLANE_LINE (0, y);
  guint16 *restrict duv = GET_PLANE_LINE (1, uv);
  guint16 Y0, Y1, U, V;
  const guint16 *restrict s = src;

  if (IS_CHROMA_LINE_420 (y, flags)) {
    for (i = 0; i < width / 2; i++) {
      Y0 = s[i * 8 + 1] & 0xfff0;
      Y1 = s[i * 8 + 5] & 0xfff0;
      U = s[i * 8 + 2] & 0xfff0;
      V = s[i * 8 + 3] & 0xfff0;

      GST_WRITE_UINT16_LE (dy + i * 2 + 0, Y0);
      GST_WRITE_UINT16_LE (dy + i * 2 + 1, Y1);
      GST_WRITE_UINT16_LE (duv + i * 2 + 0, U);
      GST_WRITE_UINT16_LE (duv + i * 2 + 1, V);
    }
    if (width & 1) {
      gint i = width - 1;

      Y0 = s[i * 4 + 1] & 0xfff0;
      U = s[i * 4 + 2] & 0xfff0;
      V = s[i * 4 + 3] & 0xfff0;

      GST_WRITE_UINT16_LE (dy + i, Y0);
      GST_WRITE_UINT16_LE (duv + i + 0, U);
      GST_WRITE_UINT16_LE (duv + i + 1, V);
    }
  } else {
    for (i = 0; i < width; i++) {
      Y0 = s[i * 4 + 1] & 0xfff0;
      GST_WRITE_UINT16_LE (dy + i, Y0);
    }
  }
}

#define PACK_Y212_BE GST_VIDEO_FORMAT_AYUV64, unpack_Y212_BE, 1, pack_Y212_BE
static void
unpack_Y212_BE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  int i;
  const guint8 *restrict s = GET_LINE (y);
  guint16 *restrict d = dest;
  guint Y0, Y1, U, V;

  s += GST_ROUND_DOWN_2 (x) * 4;

  if (x & 1) {
    Y1 = GST_READ_UINT16_BE (s + 4);
    U = GST_READ_UINT16_BE (s + 2);
    V = GST_READ_UINT16_BE (s + 6);

    if (!(flags & GST_VIDEO_PACK_FLAG_TRUNCATE_RANGE)) {
      Y1 |= (Y1 >> 12);
      U |= (U >> 12);
      V |= (V >> 12);
    }

    d[0] = 0xffff;
    d[1] = Y1;
    d[2] = U;
    d[3] = V;
    s += 8;
    d += 4;
    width--;
  }

  for (i = 0; i < width / 2; i++) {
    Y0 = GST_READ_UINT16_BE (s + i * 8 + 0);
    U = GST_READ_UINT16_BE (s + i * 8 + 2);
    V = GST_READ_UINT16_BE (s + i * 8 + 6);
    Y1 = GST_READ_UINT16_BE (s + i * 8 + 4);

    if (!(flags & GST_VIDEO_PACK_FLAG_TRUNCATE_RANGE)) {
      Y0 |= (Y0 >> 12);
      U |= (U >> 12);
      V |= (V >> 12);
    }

    d[i * 8 + 0] = 0xffff;
    d[i * 8 + 1] = Y0;
    d[i * 8 + 2] = U;
    d[i * 8 + 3] = V;

    d[i * 8 + 4] = 0xffff;
    d[i * 8 + 5] = Y1;
    d[i * 8 + 6] = U;
    d[i * 8 + 7] = V;
  }

  if (width & 1) {
    i = width - 1;

    Y0 = GST_READ_UINT16_BE (s + i * 4 + 0);
    U = GST_READ_UINT16_BE (s + i * 4 + 2);
    V = GST_READ_UINT16_BE (s + i * 4 + 6);

    if (!(flags & GST_VIDEO_PACK_FLAG_TRUNCATE_RANGE)) {
      Y0 |= (Y0 >> 12);
      U |= (U >> 12);
      V |= (V >> 12);
    }

    d[i * 4 + 0] = 0xffff;
    d[i * 4 + 1] = Y0;
    d[i * 4 + 2] = U;
    d[i * 4 + 3] = V;
  }
}

static void
pack_Y212_BE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  int i;
  guint16 Y0, Y1, U, V;
  guint8 *restrict d = GET_LINE (y);
  const guint16 *restrict s = src;

  for (i = 0; i < width; i += 2) {
    Y0 = s[i * 4 + 1] & 0xfff0;
    U = s[i * 4 + 2] & 0xfff0;
    V = s[i * 4 + 3] & 0xfff0;
    if (i == width - 1)
      Y1 = s[i * 4 + 1] & 0xfff0;
    else
      Y1 = s[(i + 1) * 4 + 1] & 0xfff0;

    GST_WRITE_UINT16_BE (d + i * 4 + 0, Y0);
    GST_WRITE_UINT16_BE (d + i * 4 + 2, U);
    GST_WRITE_UINT16_BE (d + i * 4 + 4, Y1);
    GST_WRITE_UINT16_BE (d + i * 4 + 6, V);
  }
}

#define PACK_Y212_LE GST_VIDEO_FORMAT_AYUV64, unpack_Y212_LE, 1, pack_Y212_LE
static void
unpack_Y212_LE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  int i;
  const guint8 *restrict s = GET_LINE (y);
  guint16 *restrict d = dest;
  guint Y0, Y1, U, V;

  s += GST_ROUND_DOWN_2 (x) * 4;

  if (x & 1) {
    Y1 = GST_READ_UINT16_LE (s + 4);
    U = GST_READ_UINT16_LE (s + 2);
    V = GST_READ_UINT16_LE (s + 6);

    if (!(flags & GST_VIDEO_PACK_FLAG_TRUNCATE_RANGE)) {
      Y1 |= (Y1 >> 12);
      U |= (U >> 12);
      V |= (V >> 12);
    }

    d[0] = 0xffff;
    d[1] = Y1;
    d[2] = U;
    d[3] = V;
    s += 8;
    d += 4;
    width--;
  }

  for (i = 0; i < width / 2; i++) {
    Y0 = GST_READ_UINT16_LE (s + i * 8 + 0);
    U = GST_READ_UINT16_LE (s + i * 8 + 2);
    V = GST_READ_UINT16_LE (s + i * 8 + 6);
    Y1 = GST_READ_UINT16_LE (s + i * 8 + 4);

    if (!(flags & GST_VIDEO_PACK_FLAG_TRUNCATE_RANGE)) {
      Y0 |= (Y0 >> 12);
      U |= (U >> 12);
      V |= (V >> 12);
    }

    d[i * 8 + 0] = 0xffff;
    d[i * 8 + 1] = Y0;
    d[i * 8 + 2] = U;
    d[i * 8 + 3] = V;

    d[i * 8 + 4] = 0xffff;
    d[i * 8 + 5] = Y1;
    d[i * 8 + 6] = U;
    d[i * 8 + 7] = V;
  }

  if (width & 1) {
    i = width - 1;

    Y0 = GST_READ_UINT16_LE (s + i * 4 + 0);
    U = GST_READ_UINT16_LE (s + i * 4 + 2);
    V = GST_READ_UINT16_LE (s + i * 4 + 6);

    if (!(flags & GST_VIDEO_PACK_FLAG_TRUNCATE_RANGE)) {
      Y0 |= (Y0 >> 12);
      U |= (U >> 12);
      V |= (V >> 12);
    }

    d[i * 4 + 0] = 0xffff;
    d[i * 4 + 1] = Y0;
    d[i * 4 + 2] = U;
    d[i * 4 + 3] = V;
  }
}

static void
pack_Y212_LE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  int i;
  guint16 Y0, Y1, U, V;
  guint8 *restrict d = GET_LINE (y);
  const guint16 *restrict s = src;

  for (i = 0; i < width; i += 2) {
    Y0 = s[i * 4 + 1] & 0xfff0;
    U = s[i * 4 + 2] & 0xfff0;
    V = s[i * 4 + 3] & 0xfff0;
    if (i == width - 1)
      Y1 = s[i * 4 + 1] & 0xfff0;
    else
      Y1 = s[(i + 1) * 4 + 1] & 0xfff0;

    GST_WRITE_UINT16_LE (d + i * 4 + 0, Y0);
    GST_WRITE_UINT16_LE (d + i * 4 + 2, U);
    GST_WRITE_UINT16_LE (d + i * 4 + 4, Y1);
    GST_WRITE_UINT16_LE (d + i * 4 + 6, V);
  }
}

#define PACK_Y412_BE GST_VIDEO_FORMAT_AYUV64, unpack_Y412_BE, 1, pack_Y412_BE
static void
unpack_Y412_BE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  int i;
  const guint16 *restrict s = GET_LINE (y);
  guint16 *restrict d = dest;
  guint16 A, Y, U, V;

  s += x * 4;

  for (i = 0; i < width; i++) {
    U = GST_READ_UINT16_BE (s + 4 * i + 0) & 0xfff0;
    Y = GST_READ_UINT16_BE (s + 4 * i + 1) & 0xfff0;
    V = GST_READ_UINT16_BE (s + 4 * i + 2) & 0xfff0;
    A = GST_READ_UINT16_BE (s + 4 * i + 3) & 0xfff0;

    if (!(flags & GST_VIDEO_PACK_FLAG_TRUNCATE_RANGE)) {
      U |= (U >> 12);
      Y |= (Y >> 12);
      V |= (V >> 12);
      A |= (A >> 12);
    }

    d[4 * i + 0] = A;
    d[4 * i + 1] = Y;
    d[4 * i + 2] = U;
    d[4 * i + 3] = V;
  }
}

static void
pack_Y412_BE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  int i;
  guint16 *restrict d = GET_LINE (y);
  const guint16 *restrict s = src;
  guint16 A, Y, U, V;

  for (i = 0; i < width; i++) {
    A = s[4 * i + 0] & 0xfff0;
    Y = s[4 * i + 1] & 0xfff0;
    U = s[4 * i + 2] & 0xfff0;
    V = s[4 * i + 3] & 0xfff0;

    GST_WRITE_UINT16_BE (d + 4 * i + 0, U);
    GST_WRITE_UINT16_BE (d + 4 * i + 1, Y);
    GST_WRITE_UINT16_BE (d + 4 * i + 2, V);
    GST_WRITE_UINT16_BE (d + 4 * i + 3, A);
  }
}

#define PACK_Y412_LE GST_VIDEO_FORMAT_AYUV64, unpack_Y412_LE, 1, pack_Y412_LE
static void
unpack_Y412_LE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  int i;
  const guint16 *restrict s = GET_LINE (y);
  guint16 *restrict d = dest;
  guint16 A, Y, U, V;

  s += x * 4;

  for (i = 0; i < width; i++) {
    U = GST_READ_UINT16_LE (s + 4 * i + 0) & 0xfff0;
    Y = GST_READ_UINT16_LE (s + 4 * i + 1) & 0xfff0;
    V = GST_READ_UINT16_LE (s + 4 * i + 2) & 0xfff0;
    A = GST_READ_UINT16_LE (s + 4 * i + 3) & 0xfff0;

    if (!(flags & GST_VIDEO_PACK_FLAG_TRUNCATE_RANGE)) {
      U |= (U >> 12);
      Y |= (Y >> 12);
      V |= (V >> 12);
      A |= (A >> 12);
    }

    d[4 * i + 0] = A;
    d[4 * i + 1] = Y;
    d[4 * i + 2] = U;
    d[4 * i + 3] = V;
  }
}

static void
pack_Y412_LE (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  int i;
  guint16 *restrict d = GET_LINE (y);
  const guint16 *restrict s = src;
  guint16 A, Y, U, V;

  for (i = 0; i < width; i++) {
    A = s[4 * i + 0] & 0xfff0;
    Y = s[4 * i + 1] & 0xfff0;
    U = s[4 * i + 2] & 0xfff0;
    V = s[4 * i + 3] & 0xfff0;

    GST_WRITE_UINT16_LE (d + 4 * i + 0, U);
    GST_WRITE_UINT16_LE (d + 4 * i + 1, Y);
    GST_WRITE_UINT16_LE (d + 4 * i + 2, V);
    GST_WRITE_UINT16_LE (d + 4 * i + 3, A);
  }
}

#define PACK_RGBP GST_VIDEO_FORMAT_ARGB, unpack_RGBP, 1, pack_RGBP
static void
unpack_RGBP (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  const guint8 *restrict sr = GET_R_LINE (y);
  const guint8 *restrict sg = GET_G_LINE (y);
  const guint8 *restrict sb = GET_B_LINE (y);

  sr += x;
  sg += x;
  sb += x;

  video_orc_unpack_Y444 (dest, sr, sg, sb, width);
}

static void
pack_RGBP (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  guint8 *restrict dr = GET_R_LINE (y);
  guint8 *restrict dg = GET_G_LINE (y);
  guint8 *restrict db = GET_B_LINE (y);

  video_orc_pack_Y444 (dr, dg, db, src, width);
}

#define PACK_BGRP GST_VIDEO_FORMAT_ARGB, unpack_BGRP, 1, pack_BGRP
static void
unpack_BGRP (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  const guint8 *restrict sr = GET_R_LINE (y);
  const guint8 *restrict sg = GET_G_LINE (y);
  const guint8 *restrict sb = GET_B_LINE (y);

  sr += x;
  sg += x;
  sb += x;

  video_orc_unpack_Y444 (dest, sr, sg, sb, width);
}

static void
pack_BGRP (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  guint8 *restrict dr = GET_R_LINE (y);
  guint8 *restrict dg = GET_G_LINE (y);
  guint8 *restrict db = GET_B_LINE (y);

  video_orc_pack_Y444 (dr, dg, db, src, width);
}

#define PACK_NV12_10BE_8L128  GST_VIDEO_FORMAT_AYUV64, unpack_NV12_10BE_8L128, 1, pack_NV12_10BE_8L128
#define GST_SET_U8_BITS(__pdata, __val, __mask, __shift)  \
  *(guint8 *)(__pdata) = (guint8)(((*(guint8 *)(__pdata)) & (~((__mask) << (__shift)))) | (((__val) & (__mask)) << (__shift)))
#define GST_GET_U8_BITS(__pdata, __mask, __shift)   (((*(guint8 *)(__pdata)) >> (__shift)) & (__mask))

static void
get_tile_NV12_10BE (const gint bit_depth,
    gint ts, gint ws, gint hs, gint ty, gint y, gint x,
    const gpointer data, const gint stride,
    void *restrict pdata[2], gint bits[2], guint16 mask[2])
{
  gint bit_index = bit_depth * x;
  gint line_size = GST_VIDEO_TILE_X_TILES (stride) << ts;
  gint tile_size = 1 << ts;
  gint tile_width = 1 << ws;
  gint pos[2];
  gsize offset;
  gint i;

  pos[0] = bit_index / 8;
  bits[0] = 8 - (bit_index % 8);
  if (bits[0] > bit_depth)
    bits[0] = bit_depth;
  bits[1] = bit_depth > bits[0] ? (bit_depth - bits[0]) : 0;
  pos[1] = bits[1] ? (pos[0] + 1) : 0;
  mask[0] = (1 << bits[0]) - 1;
  mask[1] = (1 << bits[1]) - 1;

  for (i = 0; i < 2; i++) {
    offset =
        line_size * ty + tile_size * (pos[i] >> ws) + tile_width * y +
        (pos[i] % tile_width);
    pdata[i] = ((guint8 *) data) + offset;
  }
}

static void
unpack_NV12_10BE_8L128 (const GstVideoFormatInfo * info,
    GstVideoPackFlags flags, gpointer dest,
    const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  const gint bit_depth = 10;
  guint16 *restrict d = dest;
  gint ws, hs, ts;
  gint ty, uv_ty, uv_y;
  guint16 Y = 0, U = 0, V = 0;
  void *restrict pdata[2];
  gint bits[2];
  guint16 mask[2];
  int i;
  int j;

  ws = 3;
  hs = 7;
  ts = ws + hs;
  ty = y >> hs;
  y = y & ((1 << hs) - 1);
  uv_ty = ty >> 1;
  uv_y = (((ty % 2) << hs) + y) / 2;

  for (i = 0; i < width; i++) {
    j = x + i;

    get_tile_NV12_10BE (bit_depth, ts, ws, hs, ty, y, j,
        data[0], stride[0], pdata, bits, mask);
    Y = (GST_GET_U8_BITS (pdata[0], mask[0], 0) << bits[1]) |
        GST_GET_U8_BITS (pdata[1], mask[1], 8 - bits[1]);

    if ((i == 0) && (j % 2))
      j--;
    if (j % 2 == 0) {
      get_tile_NV12_10BE (bit_depth, ts, ws, hs, uv_ty, uv_y, j,
          data[1], stride[1], pdata, bits, mask);
      U = (GST_GET_U8_BITS (pdata[0], mask[0], 0) << bits[1]) |
          GST_GET_U8_BITS (pdata[1], mask[1], 8 - bits[1]);

      get_tile_NV12_10BE (bit_depth, ts, ws, hs, uv_ty, uv_y, j + 1,
          data[1], stride[1], pdata, bits, mask);
      V = (GST_GET_U8_BITS (pdata[0], mask[0], 0) << bits[1]) |
          GST_GET_U8_BITS (pdata[1], mask[1], 8 - bits[1]);
    }

    d[0] = 0xffff;
    d[1] = Y << 6;
    d[2] = U << 6;
    d[3] = V << 6;
    d += 4;
  }
}

static void
pack_NV12_10BE_8L128 (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  const guint16 *restrict s = src;
  const gint bit_depth = 10;
  gint ws, hs, ts;
  gint ty, uv_ty, uv_y;
  guint16 Y, U, V;
  void *restrict pdata[2];
  gint bits[2];
  guint16 mask[2];
  int i;

  ws = 3;
  hs = 7;
  ts = ws + hs;
  ty = y >> hs;
  y = y & ((1 << hs) - 1);
  uv_ty = ty >> 1;
  uv_y = (((ty % 2) << hs) + y) / 2;

  for (i = 0; i < width; i++) {
    Y = s[i * 4 + 1] >> 6;
    U = s[i * 4 + 2] >> 6;
    V = s[i * 4 + 3] >> 6;

    get_tile_NV12_10BE (bit_depth, ts, ws, hs, ty, y, i,
        data[0], stride[0], pdata, bits, mask);
    GST_SET_U8_BITS (pdata[0], Y >> bits[1], mask[0], 0);
    GST_SET_U8_BITS (pdata[1], Y & mask[1], mask[1], 8 - bits[1]);

    if ((y % 2 == 0) && (i % 2 == 0)) {
      get_tile_NV12_10BE (bit_depth, ts, ws, hs, uv_ty, uv_y, i,
          data[1], stride[1], pdata, bits, mask);
      GST_SET_U8_BITS (pdata[0], U >> bits[1], mask[0], 0);
      GST_SET_U8_BITS (pdata[1], U & mask[1], mask[1], 8 - bits[1]);

      get_tile_NV12_10BE (bit_depth, ts, ws, hs, uv_ty, uv_y, i + 1,
          data[1], stride[1], pdata, bits, mask);
      GST_SET_U8_BITS (pdata[0], V >> bits[1], mask[0], 0);
      GST_SET_U8_BITS (pdata[1], V & mask[1], mask[1], 8 - bits[1]);
    }
  }
}

#define PACK_NV12_10LE40_TILED  GST_VIDEO_FORMAT_AYUV64, unpack_NV12_10LE40_TILED, 1, pack_NV12_10LE40_TILED
static void
unpack_NV12_10LE40_TILED (const GstVideoFormatInfo * info,
    GstVideoPackFlags flags, gpointer dest,
    const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  unpack_TILED (GST_VIDEO_FORMAT_NV12_10LE40, info, flags, dest, data, stride,
      x, y, width);
}

static void
pack_NV12_10LE40_TILED (const GstVideoFormatInfo * info,
    GstVideoPackFlags flags, const gpointer src, gint sstride,
    gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  pack_TILED (GST_VIDEO_FORMAT_NV12_10LE40, info, flags, src, sstride, data,
      stride, chroma_site, y, width);
}

#define PACK_MT2110T_TILED  GST_VIDEO_FORMAT_AYUV64, unpack_MT2110T, 1, pack_MT2110T

static void
unpack_MT2110T (const GstVideoFormatInfo * info,
    GstVideoPackFlags flags, gpointer dest,
    const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  const gsize TILE_SIZE = 640;
  guint16 *restrict d = dest;

  /* TODO not supported yet */
  g_assert (x == 0);

  /* Position in the partition grid */
  guint ty = y / 32;
  guint tx;

  /* Vertical position within the tile */
  gint tile_y = y % 32;

  /* Tiles are split into partitions of 16 bytes of lower 2bit and 64 bytes
   * of higher 8 bits. It contains 64 pixels, so 4 lines per parititons. The
   * chroma tiles have half the number of parititons. */
  gint partition_luma = tile_y / 4;
  gint partition_chroma = partition_luma / 2;
  gint partition_line_luma = tile_y % 4;
  gint partition_line_chroma = (tile_y / 2) % 4;
  gint partition_skip_luma = partition_luma * 80;
  gint partition_skip_chroma = partition_chroma * 80;

  for (tx = 0; tx < (width / 16); tx++) {
    const guint8 *y_tile = data[0];
    const guint8 *uv_tile = data[1];
    gsize offset;
    gint tile_x;
    gint num_pixels = MIN (16, width - (tx * 16));

    offset = gst_video_tile_get_index (info->tile_mode, tx, ty,
        GST_VIDEO_TILE_X_TILES (stride[0]),
        GST_VIDEO_TILE_Y_TILES (stride[0])) * TILE_SIZE;
    y_tile = y_tile + offset + partition_skip_luma;

    offset = gst_video_tile_get_index (info->tile_mode, tx, ty,
        GST_VIDEO_TILE_X_TILES (stride[1]),
        GST_VIDEO_TILE_Y_TILES (stride[1])) * (TILE_SIZE / 2);
    uv_tile = uv_tile + offset + partition_skip_chroma;

    for (tile_x = 0; tile_x < num_pixels; tile_x++) {
      guint high_i = partition_line_luma * 16 + tile_x;
      guint low_i = tile_x;
      guint low_shift = partition_line_luma * 2;
      guint low_mask = 0x03 << low_shift;
      guint low_y = (y_tile[low_i] & low_mask) >> low_shift;
      guint16 Y = (y_tile[16 + high_i] << 2) | low_y;

      high_i = partition_line_chroma * 16 + GST_ROUND_DOWN_2 (tile_x);
      low_i = GST_ROUND_DOWN_2 (tile_x);
      low_shift = partition_line_chroma * 2;
      low_mask = 0x03 << low_shift;
      guint low_u = (uv_tile[low_i] & low_mask) >> low_shift;
      guint low_v = (uv_tile[low_i + 1] & low_mask) >> low_shift;
      guint16 U = (uv_tile[16 + high_i] << 2) | low_u;
      guint16 V = (uv_tile[16 + high_i + 1] << 2) | low_v;

      d[0] = 0xffff;
      d[1] = Y << 6;
      d[2] = U << 6;
      d[3] = V << 6;
      d += 4;
    }
  }
}

static void
pack_MT2110T (const GstVideoFormatInfo * info,
    GstVideoPackFlags flags, const gpointer src, gint sstride,
    gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  const gsize TILE_SIZE = 640;
  const guint16 *restrict s = src;

  /* Position in the partition grid */
  guint ty = y / 32;
  guint tx;

  /* Vertical position within the tile */
  gint tile_y = y % 32;

  /* Tiles are split into paritions of 16 bytes of lower 2bit and 64 bytes
   * of higher 8 bits. It contains 64 pixels, so 4 lines per parititons. The
   * chroma tiles have half the number of parititons. */
  gint partition_luma = tile_y / 4;
  gint partition_chroma = partition_luma / 2;
  gint partition_line_luma = tile_y % 4;
  gint partition_line_chroma = (tile_y / 2) % 4;
  gint partition_skip_luma = partition_luma * 80;
  gint partition_skip_chroma = partition_chroma * 80;

  for (tx = 0; tx < (width / 16); tx++) {
    guint8 *y_tile = data[0];
    guint8 *uv_tile = data[1];
    gsize offset;
    gint tile_x;
    gint num_pixels = MIN (16, width - (tx * 16));

    offset = gst_video_tile_get_index (info->tile_mode, tx, ty,
        GST_VIDEO_TILE_X_TILES (stride[0]),
        GST_VIDEO_TILE_Y_TILES (stride[0])) * TILE_SIZE;
    y_tile = y_tile + offset + partition_skip_luma;

    offset = gst_video_tile_get_index (info->tile_mode, tx, ty,
        GST_VIDEO_TILE_X_TILES (stride[1]),
        GST_VIDEO_TILE_Y_TILES (stride[1])) * (TILE_SIZE / 2);
    uv_tile = uv_tile + offset + partition_skip_chroma;

    for (tile_x = 0; tile_x < num_pixels; tile_x++) {
      const guint16 Y = s[1] >> 6;
      const guint16 U = s[2] >> 6;
      const guint16 V = s[3] >> 6;

      guint high_i = partition_line_luma * 16 + tile_x;
      guint low_i = tile_x;
      guint low_shift = partition_line_luma * 2;
      guint low_mask = 0x03 << low_shift;

      y_tile[low_i] = (y_tile[low_i] & ~low_mask) | ((Y & 0x03) << low_shift);
      y_tile[16 + high_i] = Y >> 2;

      if (IS_CHROMA_LINE_420 (y, flags) && (tile_x & 1) == 0) {
        high_i = partition_line_chroma * 16 + GST_ROUND_DOWN_2 (tile_x);
        low_i = GST_ROUND_DOWN_2 (tile_x);
        low_shift = partition_line_chroma * 2;
        low_mask = 0x03 << low_shift;

        uv_tile[low_i] =
            (uv_tile[low_i] & ~low_mask) | ((U & 0x03) << low_shift);
        uv_tile[low_i + 1] =
            (uv_tile[low_i + 1] & ~low_mask) | ((V & 0x03) << low_shift);
        uv_tile[16 + high_i] = U >> 2;
        uv_tile[16 + high_i + 1] = V >> 2;
      }

      s += 4;
    }
  }
}

#define PACK_MT2110R_TILED  GST_VIDEO_FORMAT_AYUV64, unpack_MT2110R, 1, pack_MT2110R

static void
unpack_MT2110R (const GstVideoFormatInfo * info,
    GstVideoPackFlags flags, gpointer dest,
    const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  const gsize TILE_SIZE = 640;
  guint16 *restrict d = dest;

  /* TODO not supported yet */
  g_assert (x == 0);

  /* Position in the partition grid */
  guint ty = y / 32;
  guint tx;

  /* Vertical position within the tile */
  gint tile_y = y % 32;

  /* Tiles are split into partitions of 16 bytes of lower 2bit and 64 bytes
   * of higher 8 bits. It contains 64 pixels, so 4 lines per parititons. The
   * chroma tiles have half the number of parititons. */
  gint partition_luma = tile_y / 4;
  gint partition_chroma = partition_luma / 2;
  gint partition_line_luma = tile_y % 4;
  gint partition_line_chroma = (tile_y / 2) % 4;
  gint partition_skip_luma = partition_luma * 80;
  gint partition_skip_chroma = partition_chroma * 80;

  for (tx = 0; tx < (width / 16); tx++) {
    const guint8 *y_tile = data[0];
    const guint8 *uv_tile = data[1];
    gsize offset;
    gint tile_x;
    gint num_pixels = MIN (16, width - (tx * 16));

    offset = gst_video_tile_get_index (info->tile_mode, tx, ty,
        GST_VIDEO_TILE_X_TILES (stride[0]),
        GST_VIDEO_TILE_Y_TILES (stride[0])) * TILE_SIZE;
    y_tile = y_tile + offset + partition_skip_luma;

    offset = gst_video_tile_get_index (info->tile_mode, tx, ty,
        GST_VIDEO_TILE_X_TILES (stride[1]),
        GST_VIDEO_TILE_Y_TILES (stride[1])) * (TILE_SIZE / 2);
    uv_tile = uv_tile + offset + partition_skip_chroma;

    for (tile_x = 0; tile_x < num_pixels; tile_x++) {
      guint high_i = partition_line_luma * 16 + tile_x;
      guint low_i = partition_line_luma * 4 + tile_x / 4;
      guint low_shift = tile_x % 4 * 2;
      guint low_mask = 0x03 << low_shift;
      guint low_y = (y_tile[low_i] & low_mask) >> low_shift;
      guint16 Y = (y_tile[16 + high_i] << 2) | low_y;

      high_i = partition_line_chroma * 16 + GST_ROUND_DOWN_2 (tile_x);
      low_i = partition_line_chroma * 4 + tile_x / 4;
      low_shift = GST_ROUND_DOWN_2 (tile_x % 4) * 2;
      low_mask = 0x03 << low_shift;
      guint low_u = (uv_tile[low_i] & low_mask) >> low_shift;
      guint low_v = (uv_tile[low_i] & (low_mask << 2)) >> (low_shift + 2);
      guint16 U = (uv_tile[16 + high_i] << 2) | low_u;
      guint16 V = (uv_tile[16 + high_i + 1] << 2) | low_v;

      d[0] = 0xffff;
      d[1] = Y << 6;
      d[2] = U << 6;
      d[3] = V << 6;
      d += 4;
    }
  }
}

static void
pack_MT2110R (const GstVideoFormatInfo * info,
    GstVideoPackFlags flags, const gpointer src, gint sstride,
    gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  const gsize TILE_SIZE = 640;
  const guint16 *restrict s = src;

  /* Position in the partition grid */
  guint ty = y / 32;
  guint tx;

  /* Vertical position within the tile */
  gint tile_y = y % 32;

  /* Tiles are split into paritions of 16 bytes of lower 2bit and 64 bytes
   * of higher 8 bits. It contains 64 pixels, so 4 lines per parititons. The
   * chroma tiles have half the number of parititons. */
  gint partition_luma = tile_y / 4;
  gint partition_chroma = partition_luma / 2;
  gint partition_line_luma = tile_y % 4;
  gint partition_line_chroma = (tile_y / 2) % 4;
  gint partition_skip_luma = partition_luma * 80;
  gint partition_skip_chroma = partition_chroma * 80;

  for (tx = 0; tx < (width / 16); tx++) {
    guint8 *y_tile = data[0];
    guint8 *uv_tile = data[1];
    gsize offset;
    gint tile_x;
    gint num_pixels = MIN (16, width - (tx * 16));

    offset = gst_video_tile_get_index (info->tile_mode, tx, ty,
        GST_VIDEO_TILE_X_TILES (stride[0]),
        GST_VIDEO_TILE_Y_TILES (stride[0])) * TILE_SIZE;
    y_tile = y_tile + offset + partition_skip_luma;

    offset = gst_video_tile_get_index (info->tile_mode, tx, ty,
        GST_VIDEO_TILE_X_TILES (stride[1]),
        GST_VIDEO_TILE_Y_TILES (stride[1])) * (TILE_SIZE / 2);
    uv_tile = uv_tile + offset + partition_skip_chroma;

    for (tile_x = 0; tile_x < num_pixels; tile_x++) {
      const guint16 Y = s[1] >> 6;
      const guint16 U = s[2] >> 6;
      const guint16 V = s[3] >> 6;

      guint high_i = partition_line_luma * 16 + tile_x;
      guint low_i = partition_line_luma * 4 + tile_x / 4;
      guint low_shift = tile_x % 4 * 2;
      guint low_mask = 0x03 << low_shift;

      y_tile[low_i] = (y_tile[low_i] & ~low_mask) | ((Y & 0x03) << low_shift);
      y_tile[16 + high_i] = Y >> 2;

      if (IS_CHROMA_LINE_420 (y, flags) && (tile_x & 1) == 0) {
        high_i = partition_line_chroma * 16 + GST_ROUND_DOWN_2 (tile_x);
        low_i = partition_line_chroma * 4 + tile_x / 4;
        low_shift = GST_ROUND_DOWN_2 (tile_x % 4) * 2;
        low_mask = 0x0f << low_shift;

        uv_tile[low_i] = (uv_tile[low_i] & ~(low_mask)) |
            ((U & 0x03) << low_shift) | ((V & 0x03) << (low_shift + 2));
        uv_tile[16 + high_i] = U >> 2;
        uv_tile[16 + high_i + 1] = V >> 2;
      }

      s += 4;
    }
  }
}

#define PACK_RBGA GST_VIDEO_FORMAT_ARGB, unpack_RBGA, 1, pack_RBGA
static void
unpack_RBGA (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    gpointer dest, const gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], gint x, gint y, gint width)
{
  const guint8 *restrict s = GET_LINE (y);

  s += x * 4;

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
  video_orc_unpack_RBGA_le (dest, s, width);
#else
  video_orc_unpack_RBGA_be (dest, s, width);
#endif
}

static void
pack_RBGA (const GstVideoFormatInfo * info, GstVideoPackFlags flags,
    const gpointer src, gint sstride, gpointer data[GST_VIDEO_MAX_PLANES],
    const gint stride[GST_VIDEO_MAX_PLANES], GstVideoChromaSite chroma_site,
    gint y, gint width)
{
  guint8 *restrict d = GET_LINE (y);

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
  video_orc_pack_RBGA_le (d, src, width);
#else
  video_orc_pack_RBGA_be (d, src, width);
#endif
}

typedef struct
{
  guint32 fourcc;
  GstVideoFormatInfo info;
} VideoFormat;

/* depths: bits, n_components, shift, depth */
#define DPTH0            0, 0, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }
#define DPTH8            8, 1, { 0, 0, 0, 0 }, { 8, 0, 0, 0 }
#define DPTH8_32         8, 2, { 0, 0, 0, 0 }, { 8, 32, 0, 0 }
#define DPTH888          8, 3, { 0, 0, 0, 0 }, { 8, 8, 8, 0 }
#define DPTH8888         8, 4, { 0, 0, 0, 0 }, { 8, 8, 8, 8 }
#define DPTH8880         8, 4, { 0, 0, 0, 0 }, { 8, 8, 8, 0 }
#define DPTH10           10, 1, { 0, 0, 0, 0 }, { 10, 0, 0, 0 }
#define DPTH10_10_10     10, 3, { 0, 0, 0, 0 }, { 10, 10, 10, 0 }
#define DPTH10_10_10_10  10, 4, { 0, 0, 0, 0 }, { 10, 10, 10, 10 }
#define DPTH10_10_10_HI  16, 3, { 6, 6, 6, 0 }, { 10, 10, 10, 0 }
#define DPTH10_10_10_2   10, 4, { 0, 0, 0, 0 }, { 10, 10, 10, 2}
#define DPTH12_12_12     12, 3, { 0, 0, 0, 0 }, { 12, 12, 12, 0 }
#define DPTH12_12_12_HI  16, 3, { 4, 4, 4, 0 }, { 12, 12, 12, 0 }
#define DPTH12_12_12_12  12, 4, { 0, 0, 0, 0 }, { 12, 12, 12, 12 }
#define DPTH12_12_12_12_HI   16, 4, { 4, 4, 4, 4 }, { 12, 12, 12, 12 }
#define DPTH16           16, 1, { 0, 0, 0, 0 }, { 16, 0, 0, 0 }
#define DPTH16_16_16     16, 3, { 0, 0, 0, 0 }, { 16, 16, 16, 0 }
#define DPTH16_16_16_16  16, 4, { 0, 0, 0, 0 }, { 16, 16, 16, 16 }
#define DPTH555          5, 3, { 10, 5, 0, 0 }, { 5, 5, 5, 0 }
#define DPTH565          6, 3, { 11, 5, 0, 0 }, { 5, 6, 5, 0 }

/* pixel strides */
#define PSTR0             { 0, 0, 0, 0 }
#define PSTR1             { 1, 0, 0, 0 }
#define PSTR14            { 1, 4, 0, 0 }
#define PSTR111           { 1, 1, 1, 0 }
#define PSTR1111          { 1, 1, 1, 1 }
#define PSTR122           { 1, 2, 2, 0 }
#define PSTR1221          { 1, 2, 2, 1 }
#define PSTR2             { 2, 0, 0, 0 }
#define PSTR222           { 2, 2, 2, 0 }
#define PSTR2222          { 2, 2, 2, 2 }
#define PSTR244           { 2, 4, 4, 0 }
#define PSTR444           { 4, 4, 4, 0 }
#define PSTR4444          { 4, 4, 4, 4 }
#define PSTR333           { 3, 3, 3, 0 }
#define PSTR488           { 4, 8, 8, 0 }
#define PSTR8888          { 8, 8, 8, 8 }

/* planes, in what plane do we find component N */
#define PLANE_NA          0, { 0, 0, 0, 0 }
#define PLANE0            1, { 0, 0, 0, 0 }
#define PLANE01           2, { 0, 1, 0, 0 }
#define PLANE011          2, { 0, 1, 1, 0 }
#define PLANE0112         3, { 0, 1, 1, 2 }
#define PLANE012          3, { 0, 1, 2, 0 }
#define PLANE0123         4, { 0, 1, 2, 3 }
#define PLANE021          3, { 0, 2, 1, 0 }
#define PLANE201          3, { 2, 0, 1, 0 }
#define PLANE2013         4, { 2, 0, 1, 3 }
#define PLANE210          3, { 2, 1, 0, 0 }

/* offsets */
#define OFFS0             { 0, 0, 0, 0 }
#define OFFS013           { 0, 1, 3, 0 }
#define OFFS102           { 1, 0, 2, 0 }
#define OFFS1230          { 1, 2, 3, 0 }
#define OFFS012           { 0, 1, 2, 0 }
#define OFFS210           { 2, 1, 0, 0 }
#define OFFS123           { 1, 2, 3, 0 }
#define OFFS321           { 3, 2, 1, 0 }
#define OFFS0123          { 0, 1, 2, 3 }
#define OFFS0213          { 0, 2, 1, 3 }
#define OFFS2103          { 2, 1, 0, 3 }
#define OFFS3210          { 3, 2, 1, 0 }
#define OFFS031           { 0, 3, 1, 0 }
#define OFFS204           { 2, 0, 4, 0 }
#define OFFS001           { 0, 0, 1, 0 }
#define OFFS010           { 0, 1, 0, 0 }
#define OFFS104           { 1, 0, 4, 0 }
#define OFFS0246          { 0, 2, 4, 6 }
#define OFFS2460          { 2, 4, 6, 0 }
#define OFFS4206          { 4, 2, 0, 6 }
#define OFFS6420          { 6, 4, 2, 0 }

/* subsampling, w_sub, h_sub */
#define SUB410            { 0, 2, 2, 0 }, { 0, 2, 2, 0 }
#define SUB411            { 0, 2, 2, 0 }, { 0, 0, 0, 0 }
#define SUB420            { 0, 1, 1, 0 }, { 0, 1, 1, 0 }
#define SUB422            { 0, 1, 1, 0 }, { 0, 0, 0, 0 }
#define SUB4              { 0, 0, 0, 0 }, { 0, 0, 0, 0 }
#define SUB44             { 0, 0, 0, 0 }, { 0, 0, 0, 0 }
#define SUB444            { 0, 0, 0, 0 }, { 0, 0, 0, 0 }
#define SUB4444           { 0, 0, 0, 0 }, { 0, 0, 0, 0 }
#define SUB4204           { 0, 1, 1, 0 }, { 0, 1, 1, 0 }
#define SUB4224           { 0, 1, 1, 0 }, { 0, 0, 0, 0 }

/* tile_mode, tile_ws (width shift), tile_hs (height shift), tile_info[] */
#define TILE_4x4(mode) GST_VIDEO_TILE_MODE_ ##mode, 2, 2, { {4, 4, 4, 16}, {2, 4, 4, 16}, }
#define TILE_16x32s(mode) GST_VIDEO_TILE_MODE_ ##mode, 4, 5, { {16, 32, 16, 512}, {8, 16, 16, 256}, }
#define TILE_32x32(mode) GST_VIDEO_TILE_MODE_ ##mode, 5, 5, { {32, 32, 32, 1024}, {16, 32, 32, 1024}, }
#define TILE_64x32(mode) GST_VIDEO_TILE_MODE_ ##mode, 6, 5, { {64, 32, 64, 2048}, {32, 32, 64, 2048}, }
#define TILE_8x128(mode) GST_VIDEO_TILE_MODE_ ##mode, 3, 7, { {8, 128, 8, 1024}, {4, 128, 8, 1024}, }
#define TILE_10bit_16x32s(mode) GST_VIDEO_TILE_MODE_ ##mode, 4, 5, { {16, 32, 20, 640}, {8, 16, 20, 320}, }
#define TILE_10bit_8x128(mode) GST_VIDEO_TILE_MODE_ ##mode, 3, 7, { {0, 128, 8, 1024}, {0, 128, 8, 1024}, }
#define TILE_10bit_4x4(mode) GST_VIDEO_TILE_MODE_ ##mode, 2, 2, { {4, 4, 5, 20}, {2, 4, 5, 20}, }

#define MAKE_YUV_FORMAT(name, desc, fourcc, depth, pstride, plane, offs, sub, pack ) \
 { fourcc, {GST_VIDEO_FORMAT_ ##name, G_STRINGIFY(name), desc, GST_VIDEO_FORMAT_FLAG_YUV, depth, pstride, plane, offs, sub, pack } }
#define MAKE_YUV_LE_FORMAT(name, desc, fourcc, depth, pstride, plane, offs, sub, pack ) \
 { fourcc, {GST_VIDEO_FORMAT_ ##name, G_STRINGIFY(name), desc, GST_VIDEO_FORMAT_FLAG_YUV | GST_VIDEO_FORMAT_FLAG_LE, depth, pstride, plane, offs, sub, pack } }
#define MAKE_YUVA_FORMAT(name, desc, fourcc, depth, pstride, plane, offs, sub, pack) \
 { fourcc, {GST_VIDEO_FORMAT_ ##name, G_STRINGIFY(name), desc, GST_VIDEO_FORMAT_FLAG_YUV | GST_VIDEO_FORMAT_FLAG_ALPHA, depth, pstride, plane, offs, sub, pack } }
#define MAKE_YUVA_LE_FORMAT(name, desc, fourcc, depth, pstride, plane, offs, sub, pack ) \
 { fourcc, {GST_VIDEO_FORMAT_ ##name, G_STRINGIFY(name), desc, GST_VIDEO_FORMAT_FLAG_YUV | GST_VIDEO_FORMAT_FLAG_ALPHA | GST_VIDEO_FORMAT_FLAG_LE, depth, pstride, plane, offs, sub, pack } }
#define MAKE_YUVA_PACK_FORMAT(name, desc, fourcc, depth, pstride, plane, offs, sub, pack) \
 { fourcc, {GST_VIDEO_FORMAT_ ##name, G_STRINGIFY(name), desc, GST_VIDEO_FORMAT_FLAG_YUV | GST_VIDEO_FORMAT_FLAG_ALPHA | GST_VIDEO_FORMAT_FLAG_UNPACK, depth, pstride, plane, offs, sub, pack } }
#define MAKE_YUVA_LE_PACK_FORMAT(name, desc, fourcc, depth, pstride, plane, offs, sub, pack) \
 { fourcc, {GST_VIDEO_FORMAT_ ##name, G_STRINGIFY(name), desc, GST_VIDEO_FORMAT_FLAG_YUV | GST_VIDEO_FORMAT_FLAG_ALPHA | GST_VIDEO_FORMAT_FLAG_UNPACK | GST_VIDEO_FORMAT_FLAG_LE, depth, pstride, plane, offs, sub, pack } }
#define MAKE_YUV_C_FORMAT(name, desc, fourcc, depth, pstride, plane, offs, sub, pack) \
 { fourcc, {GST_VIDEO_FORMAT_ ##name, G_STRINGIFY(name), desc, GST_VIDEO_FORMAT_FLAG_YUV | GST_VIDEO_FORMAT_FLAG_COMPLEX, depth, pstride, plane, offs, sub, pack } }
#define MAKE_YUV_C_LE_FORMAT(name, desc, fourcc, depth, pstride, plane, offs, sub, pack) \
 { fourcc, {GST_VIDEO_FORMAT_ ##name, G_STRINGIFY(name), desc, GST_VIDEO_FORMAT_FLAG_YUV | GST_VIDEO_FORMAT_FLAG_COMPLEX | GST_VIDEO_FORMAT_FLAG_LE, depth, pstride, plane, offs, sub, pack } }
#define MAKE_YUV_T_FORMAT(name, desc, fourcc, depth, pstride, plane, offs, sub, pack, tile) \
 { fourcc, {GST_VIDEO_FORMAT_ ##name, G_STRINGIFY(name), desc, GST_VIDEO_FORMAT_FLAG_YUV | GST_VIDEO_FORMAT_FLAG_COMPLEX | GST_VIDEO_FORMAT_FLAG_TILED, depth, pstride, plane, offs, sub, pack, tile } }
#define MAKE_YUV_ST_FORMAT(name, desc, fourcc, depth, pstride, plane, offs, sub, pack, tile) \
 { fourcc, {GST_VIDEO_FORMAT_ ##name, G_STRINGIFY(name), desc, GST_VIDEO_FORMAT_FLAG_YUV | GST_VIDEO_FORMAT_FLAG_COMPLEX | GST_VIDEO_FORMAT_FLAG_TILED | GST_VIDEO_FORMAT_FLAG_SUBTILES, depth, pstride, plane, offs, sub, pack, tile } }

#define MAKE_RGB_FORMAT(name, desc, depth, pstride, plane, offs, sub, pack) \
 { 0x00000000, {GST_VIDEO_FORMAT_ ##name, G_STRINGIFY(name), desc, GST_VIDEO_FORMAT_FLAG_RGB, depth, pstride, plane, offs, sub, pack } }
#define MAKE_RGB_LE_FORMAT(name, desc, depth, pstride, plane, offs, sub, pack) \
 { 0x00000000, {GST_VIDEO_FORMAT_ ##name, G_STRINGIFY(name), desc, GST_VIDEO_FORMAT_FLAG_RGB | GST_VIDEO_FORMAT_FLAG_LE, depth, pstride, plane, offs, sub, pack } }
#define MAKE_RGBA_FORMAT(name, desc, depth, pstride, plane, offs, sub, pack) \
 { 0x00000000, {GST_VIDEO_FORMAT_ ##name, G_STRINGIFY(name), desc, GST_VIDEO_FORMAT_FLAG_RGB | GST_VIDEO_FORMAT_FLAG_ALPHA, depth, pstride, plane, offs, sub, pack } }
#define MAKE_RGBA_LE_FORMAT(name, desc, depth, pstride, plane, offs, sub, pack) \
 { 0x00000000, {GST_VIDEO_FORMAT_ ##name, G_STRINGIFY(name), desc, GST_VIDEO_FORMAT_FLAG_RGB | GST_VIDEO_FORMAT_FLAG_ALPHA | GST_VIDEO_FORMAT_FLAG_LE, depth, pstride, plane, offs, sub, pack } }
#define MAKE_RGBAP_FORMAT(name, desc, depth, pstride, plane, offs, sub, pack) \
 { 0x00000000, {GST_VIDEO_FORMAT_ ##name, G_STRINGIFY(name), desc, GST_VIDEO_FORMAT_FLAG_RGB | GST_VIDEO_FORMAT_FLAG_ALPHA | GST_VIDEO_FORMAT_FLAG_PALETTE, depth, pstride, plane, offs, sub, pack } }
#define MAKE_RGBA_PACK_FORMAT(name, desc, depth, pstride, plane, offs, sub, pack) \
 { 0x00000000, {GST_VIDEO_FORMAT_ ##name, G_STRINGIFY(name), desc, GST_VIDEO_FORMAT_FLAG_RGB | GST_VIDEO_FORMAT_FLAG_ALPHA | GST_VIDEO_FORMAT_FLAG_UNPACK, depth, pstride, plane, offs, sub, pack } }
#define MAKE_RGBA_LE_PACK_FORMAT(name, desc, depth, pstride, plane, offs, sub, pack) \
 { 0x00000000, {GST_VIDEO_FORMAT_ ##name, G_STRINGIFY(name), desc, GST_VIDEO_FORMAT_FLAG_RGB | GST_VIDEO_FORMAT_FLAG_ALPHA | GST_VIDEO_FORMAT_FLAG_UNPACK | GST_VIDEO_FORMAT_FLAG_LE, depth, pstride, plane, offs, sub, pack } }

#define MAKE_GRAY_FORMAT(name, desc, depth, pstride, plane, offs, sub, pack) \
 { 0x00000000, {GST_VIDEO_FORMAT_ ##name, G_STRINGIFY(name), desc, GST_VIDEO_FORMAT_FLAG_GRAY, depth, pstride, plane, offs, sub, pack } }
#define MAKE_GRAY_LE_FORMAT(name, desc, depth, pstride, plane, offs, sub, pack) \
 { 0x00000000, {GST_VIDEO_FORMAT_ ##name, G_STRINGIFY(name), desc, GST_VIDEO_FORMAT_FLAG_GRAY | GST_VIDEO_FORMAT_FLAG_LE, depth, pstride, plane, offs, sub, pack } }
#define MAKE_GRAY_C_LE_FORMAT(name, desc, depth, pstride, plane, offs, sub, pack) \
 { 0x00000000, {GST_VIDEO_FORMAT_ ##name, G_STRINGIFY(name), desc, GST_VIDEO_FORMAT_FLAG_GRAY | GST_VIDEO_FORMAT_FLAG_COMPLEX | GST_VIDEO_FORMAT_FLAG_LE, depth, pstride, plane, offs, sub, pack } }

G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
static const VideoFormat formats[] = {
  {0x00000000, {GST_VIDEO_FORMAT_UNKNOWN, "UNKNOWN", "unknown video", 0, DPTH0,
          PSTR0, PLANE_NA, OFFS0}},
  {0x00000000, {GST_VIDEO_FORMAT_ENCODED, "ENCODED", "encoded video",
          GST_VIDEO_FORMAT_FLAG_COMPLEX, DPTH0, PSTR0, PLANE_NA, OFFS0}},

  MAKE_YUV_FORMAT (I420, "raw video", GST_MAKE_FOURCC ('I', '4', '2', '0'),
      DPTH888, PSTR111, PLANE012, OFFS0, SUB420, PACK_420),
  MAKE_YUV_FORMAT (YV12, "raw video", GST_MAKE_FOURCC ('Y', 'V', '1', '2'),
      DPTH888, PSTR111, PLANE021, OFFS0, SUB420, PACK_420),
  MAKE_YUV_FORMAT (YUY2, "raw video", GST_MAKE_FOURCC ('Y', 'U', 'Y', '2'),
      DPTH888, PSTR244, PLANE0, OFFS013, SUB422, PACK_YUY2),
  MAKE_YUV_FORMAT (UYVY, "raw video", GST_MAKE_FOURCC ('U', 'Y', 'V', 'Y'),
      DPTH888, PSTR244, PLANE0, OFFS102, SUB422, PACK_UYVY),
  MAKE_YUVA_PACK_FORMAT (AYUV, "raw video", GST_MAKE_FOURCC ('A', 'Y', 'U',
          'V'), DPTH8888, PSTR4444, PLANE0, OFFS1230, SUB4444, PACK_AYUV),
  MAKE_RGB_FORMAT (RGBx, "raw video", DPTH888, PSTR444, PLANE0, OFFS012,
      SUB444, PACK_RGBA),
  MAKE_RGB_FORMAT (BGRx, "raw video", DPTH888, PSTR444, PLANE0, OFFS210,
      SUB444, PACK_BGRA),
  MAKE_RGB_FORMAT (xRGB, "raw video", DPTH888, PSTR444, PLANE0, OFFS123,
      SUB444, PACK_ARGB),
  MAKE_RGB_FORMAT (xBGR, "raw video", DPTH888, PSTR444, PLANE0, OFFS321,
      SUB444, PACK_ABGR),
  MAKE_RGBA_FORMAT (RGBA, "raw video", DPTH8888, PSTR4444, PLANE0, OFFS0123,
      SUB4444, PACK_RGBA),
  MAKE_RGBA_FORMAT (BGRA, "raw video", DPTH8888, PSTR4444, PLANE0, OFFS2103,
      SUB4444, PACK_BGRA),
  MAKE_RGBA_PACK_FORMAT (ARGB, "raw video", DPTH8888, PSTR4444, PLANE0,
      OFFS1230, SUB4444, PACK_ARGB),
  MAKE_RGBA_FORMAT (ABGR, "raw video", DPTH8888, PSTR4444, PLANE0, OFFS3210,
      SUB4444, PACK_ABGR),
  MAKE_RGB_FORMAT (RGB, "raw video", DPTH888, PSTR333, PLANE0, OFFS012, SUB444,
      PACK_RGB),
  MAKE_RGB_FORMAT (BGR, "raw video", DPTH888, PSTR333, PLANE0, OFFS210, SUB444,
      PACK_BGR),

  MAKE_YUV_FORMAT (Y41B, "raw video", GST_MAKE_FOURCC ('Y', '4', '1', 'B'),
      DPTH888, PSTR111, PLANE012, OFFS0, SUB411, PACK_Y41B),
  MAKE_YUV_FORMAT (Y42B, "raw video", GST_MAKE_FOURCC ('Y', '4', '2', 'B'),
      DPTH888, PSTR111, PLANE012, OFFS0, SUB422, PACK_Y42B),
  MAKE_YUV_FORMAT (YVYU, "raw video", GST_MAKE_FOURCC ('Y', 'V', 'Y', 'U'),
      DPTH888, PSTR244, PLANE0, OFFS031, SUB422, PACK_YVYU),
  MAKE_YUV_FORMAT (Y444, "raw video", GST_MAKE_FOURCC ('Y', '4', '4', '4'),
      DPTH888, PSTR111, PLANE012, OFFS0, SUB444, PACK_Y444),
  MAKE_YUV_C_FORMAT (v210, "raw video", GST_MAKE_FOURCC ('v', '2', '1', '0'),
      DPTH10_10_10, PSTR0, PLANE0, OFFS0, SUB422, PACK_v210),
  MAKE_YUV_FORMAT (v216, "raw video", GST_MAKE_FOURCC ('v', '2', '1', '6'),
      DPTH16_16_16, PSTR488, PLANE0, OFFS204, SUB422, PACK_v216),
  MAKE_YUV_FORMAT (NV12, "raw video", GST_MAKE_FOURCC ('N', 'V', '1', '2'),
      DPTH888, PSTR122, PLANE011, OFFS001, SUB420, PACK_NV12),
  MAKE_YUV_FORMAT (NV21, "raw video", GST_MAKE_FOURCC ('N', 'V', '2', '1'),
      DPTH888, PSTR122, PLANE011, OFFS010, SUB420, PACK_NV21),

  MAKE_GRAY_FORMAT (GRAY8, "raw video", DPTH8, PSTR1, PLANE0, OFFS0, SUB4,
      PACK_GRAY8),
  MAKE_GRAY_FORMAT (GRAY16_BE, "raw video", DPTH16, PSTR2, PLANE0, OFFS0, SUB4,
      PACK_GRAY16_BE),
  MAKE_GRAY_LE_FORMAT (GRAY16_LE, "raw video", DPTH16, PSTR2, PLANE0, OFFS0,
      SUB4, PACK_GRAY16_LE),

  MAKE_YUV_FORMAT (v308, "raw video", GST_MAKE_FOURCC ('v', '3', '0', '8'),
      DPTH888, PSTR333, PLANE0, OFFS012, SUB444, PACK_v308),

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
  MAKE_RGB_LE_FORMAT (RGB16, "raw video", DPTH565, PSTR222, PLANE0, OFFS0,
      SUB444, PACK_RGB16),
  MAKE_RGB_LE_FORMAT (BGR16, "raw video", DPTH565, PSTR222, PLANE0, OFFS0,
      SUB444, PACK_BGR16),
  MAKE_RGB_LE_FORMAT (RGB15, "raw video", DPTH555, PSTR222, PLANE0, OFFS0,
      SUB444, PACK_RGB15),
  MAKE_RGB_LE_FORMAT (BGR15, "raw video", DPTH555, PSTR222, PLANE0, OFFS0,
      SUB444, PACK_BGR15),
#else
  MAKE_RGB_FORMAT (RGB16, "raw video", DPTH565, PSTR222, PLANE0, OFFS0, SUB444,
      PACK_RGB16),
  MAKE_RGB_FORMAT (BGR16, "raw video", DPTH565, PSTR222, PLANE0, OFFS0, SUB444,
      PACK_BGR16),
  MAKE_RGB_FORMAT (RGB15, "raw video", DPTH555, PSTR222, PLANE0, OFFS0, SUB444,
      PACK_RGB15),
  MAKE_RGB_FORMAT (BGR15, "raw video", DPTH555, PSTR222, PLANE0, OFFS0, SUB444,
      PACK_BGR15),
#endif

  MAKE_YUV_C_FORMAT (UYVP, "raw video", GST_MAKE_FOURCC ('U', 'Y', 'V', 'P'),
      DPTH10_10_10, PSTR0, PLANE0, OFFS0, SUB422, PACK_UYVP),
  MAKE_YUVA_FORMAT (A420, "raw video", GST_MAKE_FOURCC ('A', '4', '2', '0'),
      DPTH8888, PSTR1111, PLANE0123, OFFS0, SUB4204, PACK_A420),
  MAKE_RGBAP_FORMAT (RGB8P, "raw video", DPTH8_32, PSTR14, PLANE01,
      OFFS0, SUB44, PACK_RGB8P),
  MAKE_YUV_FORMAT (YUV9, "raw video", GST_MAKE_FOURCC ('Y', 'U', 'V', '9'),
      DPTH888, PSTR111, PLANE012, OFFS0, SUB410, PACK_410),
  MAKE_YUV_FORMAT (YVU9, "raw video", GST_MAKE_FOURCC ('Y', 'V', 'U', '9'),
      DPTH888, PSTR111, PLANE021, OFFS0, SUB410, PACK_410),
  MAKE_YUV_FORMAT (IYU1, "raw video", GST_MAKE_FOURCC ('I', 'Y', 'U', '1'),
      DPTH888, PSTR0, PLANE0, OFFS104, SUB411, PACK_IYU1),
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
  MAKE_RGBA_LE_PACK_FORMAT (ARGB64, "raw video", DPTH16_16_16_16, PSTR8888,
      PLANE0,
      OFFS2460, SUB444, PACK_ARGB64),
  MAKE_YUVA_LE_PACK_FORMAT (AYUV64, "raw video", 0x00000000, DPTH16_16_16_16,
      PSTR8888, PLANE0, OFFS2460, SUB444, PACK_AYUV64),
#else
  MAKE_RGBA_PACK_FORMAT (ARGB64, "raw video", DPTH16_16_16_16, PSTR8888, PLANE0,
      OFFS2460, SUB444, PACK_ARGB64),
  MAKE_YUVA_PACK_FORMAT (AYUV64, "raw video", 0x00000000, DPTH16_16_16_16,
      PSTR8888, PLANE0, OFFS2460, SUB444, PACK_AYUV64),
#endif
  MAKE_RGB_FORMAT (r210, "raw video", DPTH10_10_10, PSTR444, PLANE0, OFFS0,
      SUB444, PACK_r210),
  MAKE_YUV_FORMAT (I420_10BE, "raw video", 0x00000000, DPTH10_10_10,
      PSTR222, PLANE012, OFFS0, SUB420, PACK_I420_10BE),
  MAKE_YUV_LE_FORMAT (I420_10LE, "raw video", 0x00000000, DPTH10_10_10,
      PSTR222, PLANE012, OFFS0, SUB420, PACK_I420_10LE),
  MAKE_YUV_FORMAT (I422_10BE, "raw video", 0x00000000, DPTH10_10_10,
      PSTR222, PLANE012, OFFS0, SUB422, PACK_I422_10BE),
  MAKE_YUV_LE_FORMAT (I422_10LE, "raw video", 0x00000000, DPTH10_10_10,
      PSTR222, PLANE012, OFFS0, SUB422, PACK_I422_10LE),
  MAKE_YUV_FORMAT (Y444_10BE, "raw video", 0x00000000, DPTH10_10_10,
      PSTR222, PLANE012, OFFS0, SUB444, PACK_Y444_10BE),
  MAKE_YUV_LE_FORMAT (Y444_10LE, "raw video", 0x00000000, DPTH10_10_10,
      PSTR222, PLANE012, OFFS0, SUB444, PACK_Y444_10LE),
  MAKE_RGB_FORMAT (GBR, "raw video", DPTH888, PSTR111, PLANE201, OFFS0, SUB444,
      PACK_GBR),
  MAKE_RGB_FORMAT (GBR_10BE, "raw video", DPTH10_10_10, PSTR222, PLANE201,
      OFFS0, SUB444, PACK_GBR_10BE),
  MAKE_RGB_LE_FORMAT (GBR_10LE, "raw video", DPTH10_10_10, PSTR222, PLANE201,
      OFFS0, SUB444, PACK_GBR_10LE),
  MAKE_YUV_FORMAT (NV16, "raw video", GST_MAKE_FOURCC ('N', 'V', '1', '6'),
      DPTH888, PSTR122, PLANE011, OFFS001, SUB422, PACK_NV16),
  MAKE_YUV_FORMAT (NV24, "raw video", GST_MAKE_FOURCC ('N', 'V', '2', '4'),
      DPTH888, PSTR122, PLANE011, OFFS001, SUB444, PACK_NV24),
  MAKE_YUV_T_FORMAT (NV12_64Z32, "raw video",
      GST_MAKE_FOURCC ('T', 'M', '1', '2'), DPTH888, PSTR122, PLANE011,
      OFFS001, SUB420, PACK_NV12_TILED, TILE_64x32 (ZFLIPZ_2X2)),
  MAKE_YUVA_FORMAT (A420_10BE, "raw video", 0x00000000, DPTH10_10_10_10,
      PSTR2222, PLANE0123, OFFS0, SUB4204, PACK_A420_10BE),
  MAKE_YUVA_LE_FORMAT (A420_10LE, "raw video", 0x00000000, DPTH10_10_10_10,
      PSTR2222, PLANE0123, OFFS0, SUB4204, PACK_A420_10LE),
  MAKE_YUVA_FORMAT (A422_10BE, "raw video", 0x00000000, DPTH10_10_10_10,
      PSTR2222, PLANE0123, OFFS0, SUB4224, PACK_A422_10BE),
  MAKE_YUVA_LE_FORMAT (A422_10LE, "raw video", 0x00000000, DPTH10_10_10_10,
      PSTR2222, PLANE0123, OFFS0, SUB4224, PACK_A422_10LE),
  MAKE_YUVA_FORMAT (A444_10BE, "raw video", 0x00000000, DPTH10_10_10_10,
      PSTR2222, PLANE0123, OFFS0, SUB4444, PACK_A444_10BE),
  MAKE_YUVA_LE_FORMAT (A444_10LE, "raw video", 0x00000000, DPTH10_10_10_10,
      PSTR2222, PLANE0123, OFFS0, SUB4444, PACK_A444_10LE),
  MAKE_YUV_FORMAT (NV61, "raw video", GST_MAKE_FOURCC ('N', 'V', '6', '1'),
      DPTH888, PSTR122, PLANE011, OFFS010, SUB422, PACK_NV61),
  MAKE_YUV_FORMAT (P010_10BE, "raw video", 0x00000000, DPTH10_10_10_HI,
      PSTR244, PLANE011, OFFS001, SUB420, PACK_P010_10BE),
  MAKE_YUV_LE_FORMAT (P010_10LE, "raw video", 0x00000000, DPTH10_10_10_HI,
      PSTR244, PLANE011, OFFS001, SUB420, PACK_P010_10LE),
  MAKE_YUV_FORMAT (IYU2, "raw video", GST_MAKE_FOURCC ('I', 'Y', 'U', '2'),
      DPTH888, PSTR333, PLANE0, OFFS102, SUB444, PACK_IYU2),
  MAKE_YUV_FORMAT (VYUY, "raw video", GST_MAKE_FOURCC ('V', 'Y', 'U', 'Y'),
      DPTH888, PSTR244, PLANE0, OFFS102, SUB422, PACK_VYUY),
  MAKE_RGBA_FORMAT (GBRA, "raw video", DPTH8888, PSTR1111, PLANE2013,
      OFFS0, SUB4444, PACK_GBRA),
  MAKE_RGBA_FORMAT (GBRA_10BE, "raw video", DPTH10_10_10_10, PSTR2222,
      PLANE2013, OFFS0, SUB4444, PACK_GBRA_10BE),
  MAKE_RGBA_LE_FORMAT (GBRA_10LE, "raw video", DPTH10_10_10_10, PSTR2222,
      PLANE2013, OFFS0, SUB4444, PACK_GBRA_10LE),
  MAKE_RGB_FORMAT (GBR_12BE, "raw video", DPTH12_12_12, PSTR222, PLANE201,
      OFFS0, SUB444, PACK_GBR_12BE),
  MAKE_RGB_LE_FORMAT (GBR_12LE, "raw video", DPTH12_12_12, PSTR222, PLANE201,
      OFFS0, SUB444, PACK_GBR_12LE),
  MAKE_RGBA_FORMAT (GBRA_12BE, "raw video", DPTH12_12_12_12, PSTR2222,
      PLANE2013, OFFS0, SUB4444, PACK_GBRA_12BE),
  MAKE_RGBA_LE_PACK_FORMAT (GBRA_12LE, "raw video", DPTH12_12_12_12, PSTR2222,
      PLANE2013, OFFS0, SUB4444, PACK_GBRA_12LE),
  MAKE_YUV_FORMAT (I420_12BE, "raw video", 0x00000000, DPTH12_12_12,
      PSTR222, PLANE012, OFFS0, SUB420, PACK_I420_12BE),
  MAKE_YUV_LE_FORMAT (I420_12LE, "raw video", 0x00000000, DPTH12_12_12,
      PSTR222, PLANE012, OFFS0, SUB420, PACK_I420_12LE),
  MAKE_YUV_FORMAT (I422_12BE, "raw video", 0x00000000, DPTH12_12_12,
      PSTR222, PLANE012, OFFS0, SUB422, PACK_I422_12BE),
  MAKE_YUV_LE_FORMAT (I422_12LE, "raw video", 0x00000000, DPTH12_12_12,
      PSTR222, PLANE012, OFFS0, SUB422, PACK_I422_12LE),
  MAKE_YUV_FORMAT (Y444_12BE, "raw video", 0x00000000, DPTH12_12_12,
      PSTR222, PLANE012, OFFS0, SUB444, PACK_Y444_12BE),
  MAKE_YUV_LE_FORMAT (Y444_12LE, "raw video", 0x00000000, DPTH12_12_12,
      PSTR222, PLANE012, OFFS0, SUB444, PACK_Y444_12LE),
  MAKE_GRAY_C_LE_FORMAT (GRAY10_LE32, "raw video", DPTH10, PSTR0, PLANE0, OFFS0,
      SUB4, PACK_GRAY10_LE32),
  MAKE_YUV_C_LE_FORMAT (NV12_10LE32, "raw video",
      GST_MAKE_FOURCC ('X', 'V', '1', '5'), DPTH10_10_10, PSTR0, PLANE011,
      OFFS001, SUB420, PACK_NV12_10LE32),
  MAKE_YUV_C_LE_FORMAT (NV16_10LE32, "raw video",
      GST_MAKE_FOURCC ('X', 'V', '2', '0'), DPTH10_10_10, PSTR0, PLANE011,
      OFFS001, SUB422, PACK_NV16_10LE32),
  MAKE_YUV_C_LE_FORMAT (NV12_10LE40, "raw video",
      GST_MAKE_FOURCC ('R', 'K', '2', '0'), DPTH10_10_10, PSTR0, PLANE011,
      OFFS0, SUB420, PACK_NV12_10LE40),
  MAKE_YUV_FORMAT (Y210, "raw video", GST_MAKE_FOURCC ('Y', '2', '1', '0'),
      DPTH10_10_10, PSTR488, PLANE0, OFFS0, SUB422, PACK_Y210),
  MAKE_YUV_FORMAT (Y410, "raw video", GST_MAKE_FOURCC ('Y', '4', '1', '0'),
      DPTH10_10_10_2, PSTR4444, PLANE0, OFFS0, SUB4444, PACK_Y410),
  MAKE_YUVA_PACK_FORMAT (VUYA, "raw video", GST_MAKE_FOURCC ('V', 'U', 'Y',
          'A'), DPTH8888, PSTR4444, PLANE0, OFFS2103, SUB4444, PACK_VUYA),
  MAKE_RGBA_LE_PACK_FORMAT (BGR10A2_LE, "raw video", DPTH10_10_10_2, PSTR4444,
      PLANE0,
      OFFS0, SUB4444, PACK_BGR10A2_LE),
  MAKE_RGBA_LE_PACK_FORMAT (RGB10A2_LE, "raw video", DPTH10_10_10_2, PSTR4444,
      PLANE0, OFFS0, SUB4444, PACK_RGB10A2_LE),
  MAKE_YUV_FORMAT (Y444_16BE, "raw video", 0x00000000, DPTH16_16_16,
      PSTR222, PLANE012, OFFS0, SUB444, PACK_Y444_16BE),
  MAKE_YUV_LE_FORMAT (Y444_16LE, "raw video", 0x00000000, DPTH16_16_16,
      PSTR222, PLANE012, OFFS0, SUB444, PACK_Y444_16LE),
  MAKE_YUV_FORMAT (P016_BE, "raw video", 0x00000000, DPTH16_16_16,
      PSTR244, PLANE011, OFFS001, SUB420, PACK_P016_BE),
  MAKE_YUV_LE_FORMAT (P016_LE, "raw video", 0x00000000, DPTH16_16_16,
      PSTR244, PLANE011, OFFS001, SUB420, PACK_P016_LE),
  MAKE_YUV_FORMAT (P012_BE, "raw video", 0x00000000, DPTH12_12_12_HI,
      PSTR244, PLANE011, OFFS001, SUB420, PACK_P012_BE),
  MAKE_YUV_LE_FORMAT (P012_LE, "raw video", 0x00000000, DPTH12_12_12_HI,
      PSTR244, PLANE011, OFFS001, SUB420, PACK_P012_LE),
  MAKE_YUV_FORMAT (Y212_BE, "raw video", 0x00000000, DPTH12_12_12_HI,
      PSTR488, PLANE0, OFFS0, SUB422, PACK_Y212_BE),
  MAKE_YUV_LE_FORMAT (Y212_LE, "raw video", 0x00000000, DPTH12_12_12_HI,
      PSTR488, PLANE0, OFFS0, SUB422, PACK_Y212_LE),
  MAKE_YUV_FORMAT (Y412_BE, "raw video", 0x00000000, DPTH12_12_12_12_HI,
      PSTR8888, PLANE0, OFFS0, SUB4444, PACK_Y412_BE),
  MAKE_YUV_LE_FORMAT (Y412_LE, "raw video", 0x00000000, DPTH12_12_12_12_HI,
      PSTR8888, PLANE0, OFFS0, SUB4444, PACK_Y412_LE),
  MAKE_YUV_T_FORMAT (NV12_4L4, "raw video",
      GST_MAKE_FOURCC ('V', 'T', '1', '2'), DPTH888, PSTR122, PLANE011,
      OFFS001, SUB420, PACK_NV12_TILED, TILE_4x4 (LINEAR)),
  MAKE_YUV_T_FORMAT (NV12_32L32, "raw video",
      GST_MAKE_FOURCC ('S', 'T', '1', '2'), DPTH888, PSTR122, PLANE011,
      OFFS001, SUB420, PACK_NV12_TILED, TILE_32x32 (LINEAR)),
  MAKE_RGB_FORMAT (RGBP, "raw video", DPTH888, PSTR111, PLANE012, OFFS0, SUB444,
      PACK_RGBP),
  MAKE_RGB_FORMAT (BGRP, "raw video", DPTH888, PSTR111, PLANE210, OFFS0, SUB444,
      PACK_BGRP),
  MAKE_YUV_FORMAT (AV12, "raw video", GST_MAKE_FOURCC ('A', 'V', '1', '2'),
      DPTH8888, PSTR1221, PLANE0112, OFFS001, SUB4204, PACK_AV12),
  MAKE_RGBA_LE_FORMAT (ARGB64_LE, "raw video", DPTH16_16_16_16, PSTR8888,
      PLANE0, OFFS2460, SUB4444, PACK_ARGB64_LE),
  MAKE_RGBA_FORMAT (ARGB64_BE, "raw video", DPTH16_16_16_16, PSTR8888, PLANE0,
      OFFS2460, SUB4444, PACK_ARGB64_BE),
  MAKE_RGBA_LE_FORMAT (RGBA64_LE, "raw video", DPTH16_16_16_16, PSTR8888,
      PLANE0, OFFS0246, SUB4444, PACK_RGBA64_LE),
  MAKE_RGBA_FORMAT (RGBA64_BE, "raw video", DPTH16_16_16_16, PSTR8888, PLANE0,
      OFFS0246, SUB4444, PACK_RGBA64_BE),
  MAKE_RGBA_LE_FORMAT (BGRA64_LE, "raw video", DPTH16_16_16_16, PSTR8888,
      PLANE0, OFFS4206, SUB4444, PACK_BGRA64_LE),
  MAKE_RGBA_FORMAT (BGRA64_BE, "raw video", DPTH16_16_16_16, PSTR8888, PLANE0,
      OFFS4206, SUB4444, PACK_BGRA64_BE),
  MAKE_RGBA_LE_FORMAT (ABGR64_LE, "raw video", DPTH16_16_16_16, PSTR8888,
      PLANE0, OFFS6420, SUB444, PACK_ABGR64_LE),
  MAKE_RGBA_FORMAT (ABGR64_BE, "raw video", DPTH16_16_16_16, PSTR8888, PLANE0,
      OFFS6420, SUB4444, PACK_ABGR64_BE),
  MAKE_YUV_ST_FORMAT (NV12_16L32S, "raw video",
      GST_MAKE_FOURCC ('M', 'M', '2', '1'), DPTH888, PSTR122, PLANE011,
      OFFS001, SUB420, PACK_NV12_TILED, TILE_16x32s (LINEAR)),
  MAKE_YUV_T_FORMAT (NV12_8L128, "raw video",
      GST_MAKE_FOURCC ('N', 'A', '1', '2'), DPTH888, PSTR122, PLANE011,
      OFFS001, SUB420, PACK_NV12_TILED, TILE_8x128 (LINEAR)),
  MAKE_YUV_T_FORMAT (NV12_10BE_8L128, "raw video",
      GST_MAKE_FOURCC ('N', 'T', '1', '2'), DPTH10_10_10, PSTR0, PLANE011,
      OFFS001, SUB420, PACK_NV12_10BE_8L128, TILE_10bit_8x128 (LINEAR)),
  MAKE_YUV_T_FORMAT (NV12_10LE40_4L4, "raw video",
      GST_MAKE_FOURCC ('M', 'H', '2', '1'), DPTH10_10_10, PSTR0, PLANE011,
      OFFS001, SUB420, PACK_NV12_10LE40_TILED, TILE_10bit_4x4 (LINEAR)),
  {0x00000000, {GST_VIDEO_FORMAT_DMA_DRM, "DMA_DRM", "DMA DRM video",
          GST_VIDEO_FORMAT_FLAG_COMPLEX, DPTH0, PSTR0, PLANE_NA, OFFS0}},
  MAKE_YUV_ST_FORMAT (MT2110T, "raw video",
      GST_MAKE_FOURCC ('M', 'T', '2', 'T'), DPTH10_10_10, PSTR0, PLANE011,
      OFFS001, SUB420, PACK_MT2110T_TILED, TILE_10bit_16x32s (LINEAR)),
  MAKE_YUV_ST_FORMAT (MT2110R, "raw video",
      GST_MAKE_FOURCC ('M', 'T', '2', 'R'), DPTH10_10_10, PSTR0, PLANE011,
      OFFS001, SUB420, PACK_MT2110R_TILED, TILE_10bit_16x32s (LINEAR)),
  MAKE_YUVA_FORMAT (A422, "raw video", GST_MAKE_FOURCC ('A', '4', '2', '2'),
      DPTH8888, PSTR1111, PLANE0123, OFFS0, SUB4224, PACK_A422),
  MAKE_YUVA_FORMAT (A444, "raw video", GST_MAKE_FOURCC ('A', '4', '4', '4'),
      DPTH8888, PSTR1111, PLANE0123, OFFS0, SUB4444, PACK_A444),
  MAKE_YUVA_LE_FORMAT (A444_12LE, "raw video", 0x00000000, DPTH12_12_12_12,
      PSTR2222, PLANE0123, OFFS0, SUB4444, PACK_A444_12LE),
  MAKE_YUVA_FORMAT (A444_12BE, "raw video", 0x00000000, DPTH12_12_12_12,
      PSTR2222, PLANE0123, OFFS0, SUB4444, PACK_A444_12BE),
  MAKE_YUVA_LE_FORMAT (A422_12LE, "raw video", 0x00000000, DPTH12_12_12_12,
      PSTR2222, PLANE0123, OFFS0, SUB4224, PACK_A422_12LE),
  MAKE_YUVA_FORMAT (A422_12BE, "raw video", 0x00000000, DPTH12_12_12_12,
      PSTR2222, PLANE0123, OFFS0, SUB4224, PACK_A422_12BE),
  MAKE_YUVA_LE_FORMAT (A420_12LE, "raw video", 0x00000000, DPTH12_12_12_12,
      PSTR2222, PLANE0123, OFFS0, SUB4204, PACK_A420_12LE),
  MAKE_YUVA_FORMAT (A420_12BE, "raw video", 0x00000000, DPTH12_12_12_12,
      PSTR2222, PLANE0123, OFFS0, SUB4204, PACK_A420_12BE),
  MAKE_YUVA_LE_FORMAT (A444_16LE, "raw video", 0x00000000, DPTH16_16_16_16,
      PSTR2222, PLANE0123, OFFS0, SUB4444, PACK_A444_16LE),
  MAKE_YUVA_FORMAT (A444_16BE, "raw video", 0x00000000, DPTH16_16_16_16,
      PSTR2222, PLANE0123, OFFS0, SUB4444, PACK_A444_16BE),
  MAKE_YUVA_LE_FORMAT (A422_16LE, "raw video", 0x00000000, DPTH16_16_16_16,
      PSTR2222, PLANE0123, OFFS0, SUB4224, PACK_A422_16LE),
  MAKE_YUVA_FORMAT (A422_16BE, "raw video", 0x00000000, DPTH16_16_16_16,
      PSTR2222, PLANE0123, OFFS0, SUB4224, PACK_A422_16BE),
  MAKE_YUVA_LE_FORMAT (A420_16LE, "raw video", 0x00000000, DPTH16_16_16_16,
      PSTR2222, PLANE0123, OFFS0, SUB4204, PACK_A420_16LE),
  MAKE_YUVA_FORMAT (A420_16BE, "raw video", 0x00000000, DPTH16_16_16_16,
      PSTR2222, PLANE0123, OFFS0, SUB4204, PACK_A420_16BE),
  MAKE_RGB_LE_FORMAT (GBR_16LE, "raw video", DPTH16_16_16, PSTR222, PLANE201,
      OFFS0, SUB444, PACK_GBR_16LE),
  MAKE_RGB_FORMAT (GBR_16BE, "raw video", DPTH16_16_16, PSTR222, PLANE201,
      OFFS0, SUB444, PACK_GBR_16BE),
  MAKE_RGBA_FORMAT (RBGA, "raw video", DPTH8888, PSTR4444, PLANE0, OFFS0213,
      SUB4444, PACK_RBGA),
};

G_GNUC_END_IGNORE_DEPRECATIONS;

static GstVideoFormat
gst_video_format_from_rgb32_masks (int red_mask, int green_mask, int blue_mask)
{
  if (red_mask == 0xff000000 && green_mask == 0x00ff0000 &&
      blue_mask == 0x0000ff00) {
    return GST_VIDEO_FORMAT_RGBx;
  }
  if (red_mask == 0x0000ff00 && green_mask == 0x00ff0000 &&
      blue_mask == 0xff000000) {
    return GST_VIDEO_FORMAT_BGRx;
  }
  if (red_mask == 0x00ff0000 && green_mask == 0x0000ff00 &&
      blue_mask == 0x000000ff) {
    return GST_VIDEO_FORMAT_xRGB;
  }
  if (red_mask == 0x000000ff && green_mask == 0x0000ff00 &&
      blue_mask == 0x00ff0000) {
    return GST_VIDEO_FORMAT_xBGR;
  }

  return GST_VIDEO_FORMAT_UNKNOWN;
}

static GstVideoFormat
gst_video_format_from_rgba32_masks (int red_mask, int green_mask,
    int blue_mask, int alpha_mask)
{
  if (red_mask == 0xff000000 && green_mask == 0x00ff0000 &&
      blue_mask == 0x0000ff00 && alpha_mask == 0x000000ff) {
    return GST_VIDEO_FORMAT_RGBA;
  }
  if (red_mask == 0x0000ff00 && green_mask == 0x00ff0000 &&
      blue_mask == 0xff000000 && alpha_mask == 0x000000ff) {
    return GST_VIDEO_FORMAT_BGRA;
  }
  if (red_mask == 0x00ff0000 && green_mask == 0x0000ff00 &&
      blue_mask == 0x000000ff && alpha_mask == 0xff000000) {
    return GST_VIDEO_FORMAT_ARGB;
  }
  if (red_mask == 0x000000ff && green_mask == 0x0000ff00 &&
      blue_mask == 0x00ff0000 && alpha_mask == 0xff000000) {
    return GST_VIDEO_FORMAT_ABGR;
  }
  return GST_VIDEO_FORMAT_UNKNOWN;
}

static GstVideoFormat
gst_video_format_from_rgb24_masks (int red_mask, int green_mask, int blue_mask)
{
  if (red_mask == 0xff0000 && green_mask == 0x00ff00 && blue_mask == 0x0000ff) {
    return GST_VIDEO_FORMAT_RGB;
  }
  if (red_mask == 0x0000ff && green_mask == 0x00ff00 && blue_mask == 0xff0000) {
    return GST_VIDEO_FORMAT_BGR;
  }

  return GST_VIDEO_FORMAT_UNKNOWN;
}

#define GST_VIDEO_COMP1_MASK_16_INT 0xf800
#define GST_VIDEO_COMP2_MASK_16_INT 0x07e0
#define GST_VIDEO_COMP3_MASK_16_INT 0x001f

#define GST_VIDEO_COMP1_MASK_15_INT 0x7c00
#define GST_VIDEO_COMP2_MASK_15_INT 0x03e0
#define GST_VIDEO_COMP3_MASK_15_INT 0x001f

static GstVideoFormat
gst_video_format_from_rgb16_masks (int red_mask, int green_mask, int blue_mask)
{
  if (red_mask == GST_VIDEO_COMP1_MASK_16_INT
      && green_mask == GST_VIDEO_COMP2_MASK_16_INT
      && blue_mask == GST_VIDEO_COMP3_MASK_16_INT) {
    return GST_VIDEO_FORMAT_RGB16;
  }
  if (red_mask == GST_VIDEO_COMP3_MASK_16_INT
      && green_mask == GST_VIDEO_COMP2_MASK_16_INT
      && blue_mask == GST_VIDEO_COMP1_MASK_16_INT) {
    return GST_VIDEO_FORMAT_BGR16;
  }
  if (red_mask == GST_VIDEO_COMP1_MASK_15_INT
      && green_mask == GST_VIDEO_COMP2_MASK_15_INT
      && blue_mask == GST_VIDEO_COMP3_MASK_15_INT) {
    return GST_VIDEO_FORMAT_RGB15;
  }
  if (red_mask == GST_VIDEO_COMP3_MASK_15_INT
      && green_mask == GST_VIDEO_COMP2_MASK_15_INT
      && blue_mask == GST_VIDEO_COMP1_MASK_15_INT) {
    return GST_VIDEO_FORMAT_BGR15;
  }
  return GST_VIDEO_FORMAT_UNKNOWN;
}

/**
 * gst_video_format_from_masks:
 * @depth: the amount of bits used for a pixel
 * @bpp: the amount of bits used to store a pixel. This value is bigger than
 *   @depth
 * @endianness: the endianness of the masks, #G_LITTLE_ENDIAN or #G_BIG_ENDIAN
 * @red_mask: the red mask
 * @green_mask: the green mask
 * @blue_mask: the blue mask
 * @alpha_mask: the alpha mask, or 0 if no alpha mask
 *
 * Find the #GstVideoFormat for the given parameters.
 *
 * Returns: a #GstVideoFormat or GST_VIDEO_FORMAT_UNKNOWN when the parameters to
 * not specify a known format.
 */
GstVideoFormat
gst_video_format_from_masks (gint depth, gint bpp, gint endianness,
    guint red_mask, guint green_mask, guint blue_mask, guint alpha_mask)
{
  GstVideoFormat format;

  /* our caps system handles 24/32bpp RGB as big-endian. */
  if ((bpp == 24 || bpp == 32) && endianness == G_LITTLE_ENDIAN &&
      alpha_mask != 0xc0000000) {
    red_mask = GUINT32_TO_BE (red_mask);
    green_mask = GUINT32_TO_BE (green_mask);
    blue_mask = GUINT32_TO_BE (blue_mask);
    alpha_mask = GUINT32_TO_BE (alpha_mask);
    endianness = G_BIG_ENDIAN;
    if (bpp == 24) {
      red_mask >>= 8;
      green_mask >>= 8;
      blue_mask >>= 8;
    }
  }

  if (depth == 32 && bpp == 32 && alpha_mask == 0xc0000000 &&
      endianness == G_LITTLE_ENDIAN) {
    if (red_mask == 0x3ff00000)
      format = GST_VIDEO_FORMAT_RGB10A2_LE;
    else
      format = GST_VIDEO_FORMAT_BGR10A2_LE;
  } else if (depth == 30 && bpp == 32) {
    format = GST_VIDEO_FORMAT_r210;
  } else if (depth == 24 && bpp == 32) {
    format = gst_video_format_from_rgb32_masks (red_mask, green_mask,
        blue_mask);
  } else if (depth == 32 && bpp == 32 && !alpha_mask) {
    format = gst_video_format_from_rgb32_masks (red_mask, green_mask,
        blue_mask);
  } else if (depth == 32 && bpp == 32 && alpha_mask) {
    format = gst_video_format_from_rgba32_masks (red_mask, green_mask,
        blue_mask, alpha_mask);
  } else if (depth == 24 && bpp == 24) {
    format = gst_video_format_from_rgb24_masks (red_mask, green_mask,
        blue_mask);
  } else if ((depth == 15 || depth == 16) && bpp == 16 &&
      endianness == G_BYTE_ORDER) {
    format = gst_video_format_from_rgb16_masks (red_mask, green_mask,
        blue_mask);
  } else if (depth == 8 && bpp == 8) {
    format = GST_VIDEO_FORMAT_RGB8P;
  } else if (depth == 64 && bpp == 64) {
    format = gst_video_format_from_rgba32_masks (red_mask, green_mask,
        blue_mask, alpha_mask);
    if (format == GST_VIDEO_FORMAT_ARGB) {
      format = GST_VIDEO_FORMAT_ARGB64;
    } else {
      format = GST_VIDEO_FORMAT_UNKNOWN;
    }
  } else {
    format = GST_VIDEO_FORMAT_UNKNOWN;
  }
  return format;
}

/**
 * gst_video_format_from_fourcc:
 * @fourcc: a FOURCC value representing raw YUV video
 *
 * Converts a FOURCC value into the corresponding #GstVideoFormat.
 * If the FOURCC cannot be represented by #GstVideoFormat,
 * #GST_VIDEO_FORMAT_UNKNOWN is returned.
 *
 * Returns: the #GstVideoFormat describing the FOURCC value
 */
GstVideoFormat
gst_video_format_from_fourcc (guint32 fourcc)
{
  switch (fourcc) {
    case GST_MAKE_FOURCC ('I', '4', '2', '0'):
      return GST_VIDEO_FORMAT_I420;
    case GST_MAKE_FOURCC ('Y', 'V', '1', '2'):
      return GST_VIDEO_FORMAT_YV12;
    case GST_MAKE_FOURCC ('Y', 'U', 'Y', '2'):
      return GST_VIDEO_FORMAT_YUY2;
    case GST_MAKE_FOURCC ('Y', 'V', 'Y', 'U'):
      return GST_VIDEO_FORMAT_YVYU;
    case GST_MAKE_FOURCC ('U', 'Y', 'V', 'Y'):
      return GST_VIDEO_FORMAT_UYVY;
    case GST_MAKE_FOURCC ('V', 'Y', 'U', 'Y'):
      return GST_VIDEO_FORMAT_VYUY;
    case GST_MAKE_FOURCC ('A', 'Y', 'U', 'V'):
      return GST_VIDEO_FORMAT_AYUV;
    case GST_MAKE_FOURCC ('Y', '4', '1', 'B'):
      return GST_VIDEO_FORMAT_Y41B;
    case GST_MAKE_FOURCC ('Y', '4', '2', 'B'):
      return GST_VIDEO_FORMAT_Y42B;
    case GST_MAKE_FOURCC ('Y', '4', '4', '4'):
      return GST_VIDEO_FORMAT_Y444;
    case GST_MAKE_FOURCC ('v', '2', '1', '0'):
      return GST_VIDEO_FORMAT_v210;
    case GST_MAKE_FOURCC ('v', '2', '1', '6'):
      return GST_VIDEO_FORMAT_v216;
    case GST_MAKE_FOURCC ('Y', '2', '1', '0'):
      return GST_VIDEO_FORMAT_Y210;
    case GST_MAKE_FOURCC ('N', 'V', '1', '2'):
      return GST_VIDEO_FORMAT_NV12;
    case GST_MAKE_FOURCC ('N', 'V', '2', '1'):
      return GST_VIDEO_FORMAT_NV21;
    case GST_MAKE_FOURCC ('N', 'V', '1', '6'):
      return GST_VIDEO_FORMAT_NV16;
    case GST_MAKE_FOURCC ('N', 'V', '6', '1'):
      return GST_VIDEO_FORMAT_NV61;
    case GST_MAKE_FOURCC ('N', 'V', '2', '4'):
      return GST_VIDEO_FORMAT_NV24;
    case GST_MAKE_FOURCC ('v', '3', '0', '8'):
      return GST_VIDEO_FORMAT_v308;
    case GST_MAKE_FOURCC ('I', 'Y', 'U', '2'):
      return GST_VIDEO_FORMAT_IYU2;
    case GST_MAKE_FOURCC ('Y', '8', '0', '0'):
    case GST_MAKE_FOURCC ('Y', '8', ' ', ' '):
    case GST_MAKE_FOURCC ('G', 'R', 'E', 'Y'):
      return GST_VIDEO_FORMAT_GRAY8;
    case GST_MAKE_FOURCC ('Y', '1', '6', ' '):
      return GST_VIDEO_FORMAT_GRAY16_LE;
    case GST_MAKE_FOURCC ('U', 'Y', 'V', 'P'):
      return GST_VIDEO_FORMAT_UYVP;
    case GST_MAKE_FOURCC ('A', '4', '2', '0'):
      return GST_VIDEO_FORMAT_A420;
    case GST_MAKE_FOURCC ('Y', 'U', 'V', '9'):
      return GST_VIDEO_FORMAT_YUV9;
    case GST_MAKE_FOURCC ('Y', 'V', 'U', '9'):
      return GST_VIDEO_FORMAT_YVU9;
    case GST_MAKE_FOURCC ('I', 'Y', 'U', '1'):
      return GST_VIDEO_FORMAT_IYU1;
    case GST_MAKE_FOURCC ('A', 'Y', '6', '4'):
      return GST_VIDEO_FORMAT_AYUV64;
    case GST_MAKE_FOURCC ('X', 'V', '1', '0'):
      return GST_VIDEO_FORMAT_GRAY10_LE32;
    case GST_MAKE_FOURCC ('X', 'V', '1', '5'):
      return GST_VIDEO_FORMAT_NV12_10LE32;
    case GST_MAKE_FOURCC ('X', 'V', '2', '0'):
      return GST_VIDEO_FORMAT_NV16_10LE32;
    case GST_MAKE_FOURCC ('R', 'K', '2', '0'):
      return GST_VIDEO_FORMAT_NV12_10LE40;
    case GST_MAKE_FOURCC ('Y', '4', '1', '0'):
      return GST_VIDEO_FORMAT_Y410;
    case GST_MAKE_FOURCC ('V', 'U', 'Y', 'A'):
      return GST_VIDEO_FORMAT_VUYA;
    case GST_MAKE_FOURCC ('A', 'R', '3', '0'):
      return GST_VIDEO_FORMAT_BGR10A2_LE;

    default:
      return GST_VIDEO_FORMAT_UNKNOWN;
  }
}

/**
 * gst_video_format_from_string:
 * @format: a format string
 *
 * Convert the @format string to its #GstVideoFormat.
 *
 * Returns: the #GstVideoFormat for @format or GST_VIDEO_FORMAT_UNKNOWN when the
 * string is not a known format.
 */
GstVideoFormat
gst_video_format_from_string (const gchar * format)
{
  guint i;

  g_return_val_if_fail (format != NULL, GST_VIDEO_FORMAT_UNKNOWN);

  for (i = 0; i < G_N_ELEMENTS (formats); i++) {
    if (strcmp (GST_VIDEO_FORMAT_INFO_NAME (&formats[i].info), format) == 0)
      return GST_VIDEO_FORMAT_INFO_FORMAT (&formats[i].info);
  }

  return GST_VIDEO_FORMAT_UNKNOWN;
}


/**
 * gst_video_format_to_fourcc:
 * @format: a #GstVideoFormat video format
 *
 * Converts a #GstVideoFormat value into the corresponding FOURCC.  Only
 * a few YUV formats have corresponding FOURCC values.  If @format has
 * no corresponding FOURCC value, 0 is returned.
 *
 * Returns: the FOURCC corresponding to @format
 */
guint32
gst_video_format_to_fourcc (GstVideoFormat format)
{
  g_return_val_if_fail (format != GST_VIDEO_FORMAT_UNKNOWN, 0);

  if ((gint) format >= G_N_ELEMENTS (formats))
    return 0;

  return formats[format].fourcc;
}

/**
 * gst_video_format_to_string:
 * @format: a #GstVideoFormat video format
 *
 * Returns a string containing a descriptive name for
 * the #GstVideoFormat if there is one, or NULL otherwise.
 *
 * Returns: the name corresponding to @format
 */
const gchar *
gst_video_format_to_string (GstVideoFormat format)
{
  g_return_val_if_fail (format != GST_VIDEO_FORMAT_UNKNOWN, NULL);

  if ((gint) format >= G_N_ELEMENTS (formats))
    return NULL;

  return GST_VIDEO_FORMAT_INFO_NAME (&formats[format].info);
}

/**
 * gst_video_format_get_info:
 * @format: a #GstVideoFormat
 *
 * Get the #GstVideoFormatInfo for @format
 *
 * Returns: The #GstVideoFormatInfo for @format.
 */
const GstVideoFormatInfo *
gst_video_format_get_info (GstVideoFormat format)
{
  g_return_val_if_fail ((gint) format < G_N_ELEMENTS (formats), NULL);

  return &formats[format].info;
}

/**
 * gst_video_format_get_palette:
 * @format: a #GstVideoFormat
 * @size: (out): size of the palette in bytes
 *
 * Get the default palette of @format. This the palette used in the pack
 * function for paletted formats.
 *
 * Returns: (transfer none) (nullable): the default palette of @format or %NULL when
 * @format does not have a palette.
 *
 * Since: 1.2
 */
gconstpointer
gst_video_format_get_palette (GstVideoFormat format, gsize * size)
{
  g_return_val_if_fail ((gint) format < G_N_ELEMENTS (formats), NULL);
  g_return_val_if_fail (size != NULL, NULL);

  switch (format) {
    case GST_VIDEO_FORMAT_RGB8P:
      *size = sizeof (std_palette_RGB8P);
      return std_palette_RGB8P;
    default:
      return NULL;
  }
}

/**
 * gst_video_format_info_component:
 * @info: #GstVideoFormatInfo
 * @plane: a plane number
 * @components: (out): array used to store component numbers
 *
 * Fill @components with the number of all the components packed in plane @p
 * for the format @info. A value of -1 in @components indicates that no more
 * components are packed in the plane.
 *
 * Since: 1.18
 */
void
gst_video_format_info_component (const GstVideoFormatInfo * info, guint plane,
    gint components[GST_VIDEO_MAX_COMPONENTS])
{
  guint c, i = 0;

  /* Reverse mapping of info->plane */
  for (c = 0; c < GST_VIDEO_FORMAT_INFO_N_COMPONENTS (info); c++) {
    if (GST_VIDEO_FORMAT_INFO_PLANE (info, c) == plane) {
      components[i] = c;
      i++;
    }
  }

  for (c = i; c < GST_VIDEO_MAX_COMPONENTS; c++)
    components[c] = -1;
}

/**
 * gst_video_format_info_extrapolate_stride:
 * @finfo: #GstVideoFormatInfo
 * @plane: a plane number
 * @stride: The fist plane stride
 *
 * Extrapolate @plane stride from the first stride of an image. This helper is
 * useful to support legacy API were only one stride is supported.
 *
 * Returns: The extrapolated stride for @plane
 *
 * Since: 1.22
 */
gint
gst_video_format_info_extrapolate_stride (const GstVideoFormatInfo * finfo,
    gint plane, gint stride)
{
  gint estride;
  gint comp[GST_VIDEO_MAX_COMPONENTS];
  gint i;

  /* there is nothing to extrapolate on first plane */
  if (plane == 0)
    return stride;

  gst_video_format_info_component (finfo, plane, comp);

  /* For now, all planar formats have a single component on first plane, but
   * if there was a planar format with more, we'd have to make a ratio of the
   * number of component on the first plane against the number of component on
   * the current plane. */
  estride = 0;
  for (i = 0; i < GST_VIDEO_MAX_COMPONENTS && comp[i] >= 0; i++)
    estride += GST_VIDEO_FORMAT_INFO_SCALE_WIDTH (finfo, comp[i], stride);

  return estride;
}

struct RawVideoFormats
{
  GstVideoFormat *formats;
  guint n;
};

static struct RawVideoFormats *
generate_video_formats (const gchar * formats)
{
  GValue list = G_VALUE_INIT;
  struct RawVideoFormats *all = g_new (struct RawVideoFormats, 1);
  gchar *tmp;
  guint i;
  gboolean res G_GNUC_UNUSED;

  g_value_init (&list, GST_TYPE_LIST);
  /* Workaround a bug in our parser that would lead to segfaults
   * when deserializing container types using static strings,
   * see https://gitlab.freedesktop.org/gstreamer/gstreamer/-/issues/446 */
  tmp = g_strdup (formats);
  res = gst_value_deserialize (&list, tmp);
  g_assert (res);
  g_free (tmp);

  all->n = gst_value_list_get_size (&list);
  all->formats = g_new (GstVideoFormat, all->n);

  for (i = 0; i < all->n; i++) {
    const GValue *v = gst_value_list_get_value (&list, i);

    all->formats[i] = gst_video_format_from_string (g_value_get_string (v));
    g_assert (all->formats[i] != GST_VIDEO_FORMAT_UNKNOWN
        && all->formats[i] != GST_VIDEO_FORMAT_ENCODED);
  }

  g_value_unset (&list);

  return all;
}

static gpointer
generate_raw_video_formats (gpointer data)
{
  return generate_video_formats (GST_VIDEO_FORMATS_ALL);
}

static gpointer
generate_any_video_formats (gpointer data)
{
  return generate_video_formats (GST_VIDEO_FORMATS_ANY);
}

/**
 * gst_video_formats_raw:
 * @len: (out): the number of elements in the returned array
 *
 * Return all the raw video formats supported by GStreamer.
 *
 * Returns: (transfer none) (array length=len): an array of #GstVideoFormat
 * Since: 1.18
 */
const GstVideoFormat *
gst_video_formats_raw (guint * len)
{
  static GOnce raw_video_formats_once = G_ONCE_INIT;
  struct RawVideoFormats *all;

  g_return_val_if_fail (len, NULL);

  g_once (&raw_video_formats_once, generate_raw_video_formats, NULL);

  all = raw_video_formats_once.retval;
  *len = all->n;
  return all->formats;
}

/**
 * gst_video_formats_any:
 * @len: (out): the number of elements in the returned array
 *
 * Return all the raw video formats supported by GStreamer including
 * special opaque formats such as %GST_VIDEO_FORMAT_DMA_DRM for which
 * no software conversion exists. This should be use for passthrough
 * template cpas.
 *
 * Returns: (transfer none) (array length=len): an array of #GstVideoFormat
 * Since: 1.24
 */
const GstVideoFormat *
gst_video_formats_any (guint * len)
{
  static GOnce any_video_formats_once = G_ONCE_INIT;
  struct RawVideoFormats *all;

  g_return_val_if_fail (len, NULL);

  g_once (&any_video_formats_once, generate_any_video_formats, NULL);

  all = any_video_formats_once.retval;
  *len = all->n;
  return all->formats;
}

/**
 * gst_video_make_raw_caps:
 * @formats: (array length=len) (nullable): an array of raw #GstVideoFormat, or %NULL
 * @len: the size of @formats
 *
 * Return a generic raw video caps for formats defined in @formats.
 * If @formats is %NULL returns a caps for all the supported raw video formats,
 * see gst_video_formats_raw().
 *
 * Returns: (transfer full): a video @GstCaps
 * Since: 1.18
 */
GstCaps *
gst_video_make_raw_caps (const GstVideoFormat formats[], guint len)
{
  return gst_video_make_raw_caps_with_features (formats, len, NULL);
}

/**
 * gst_video_make_raw_caps_with_features:
 * @formats: (array length=len) (nullable): an array of raw #GstVideoFormat, or %NULL
 * @len: the size of @formats
 * @features: (transfer full) (allow-none): the #GstCapsFeatures to set on the caps
 *
 * Return a generic raw video caps for formats defined in @formats with features
 * @features.
 * If @formats is %NULL returns a caps for all the supported video formats,
 * see gst_video_formats_raw().
 *
 * Returns: (transfer full): a video @GstCaps
 * Since: 1.18
 */
GstCaps *
gst_video_make_raw_caps_with_features (const GstVideoFormat formats[],
    guint len, GstCapsFeatures * features)
{
  GstStructure *s;
  GValue format = G_VALUE_INIT;
  GstCaps *caps;

  g_return_val_if_fail ((formats && len > 0) || (!formats && len == 0), NULL);

  if (!formats) {
    formats = gst_video_formats_raw (&len);
  }

  if (len > 1) {
    guint i;

    g_value_init (&format, GST_TYPE_LIST);

    for (i = 0; i < len; i++) {
      GValue v = G_VALUE_INIT;

      g_return_val_if_fail (formats[i] != GST_VIDEO_FORMAT_UNKNOWN
          && formats[i] != GST_VIDEO_FORMAT_ENCODED, NULL);

      g_value_init (&v, G_TYPE_STRING);
      g_value_set_static_string (&v, gst_video_format_to_string (formats[i]));
      gst_value_list_append_and_take_value (&format, &v);
    }
  } else {
    g_value_init (&format, G_TYPE_STRING);

    g_value_set_static_string (&format,
        gst_video_format_to_string (formats[0]));
  }

  s = gst_structure_new ("video/x-raw",
      "width", GST_TYPE_INT_RANGE, 1, G_MAXINT,
      "height", GST_TYPE_INT_RANGE, 1, G_MAXINT,
      "framerate", GST_TYPE_FRACTION_RANGE, 0, 1, G_MAXINT, 1, NULL);

  gst_structure_take_value (s, "format", &format);

  caps = gst_caps_new_full (s, NULL);

  if (features)
    gst_caps_set_features (caps, 0, features);

  return caps;
}
