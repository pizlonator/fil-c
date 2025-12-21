/* Gstreamer video blending utility functions
 *
 * Copied/pasted from gst/videoconvert/videoconvert.c
 *    Copyright (C) 2010 David Schleef <ds@schleef.org>
 *    Copyright (C) 2010 Sebastian Dröge <sebastian.droege@collabora.co.uk>
 *
 * Copyright (C) <2011> Intel Corporation
 * Copyright (C) <2011> Collabora Ltd.
 * Copyright (C) <2011> Thibault Saunier <thibault.saunier@collabora.com>
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

#include "video-blend.h"
#include "video-orc.h"

#include <string.h>

#ifndef GST_DISABLE_GST_DEBUG

#define GST_CAT_DEFAULT ensure_debug_category()

static GstDebugCategory *
ensure_debug_category (void)
{
  static gsize cat_gonce = 0;

  if (g_once_init_enter (&cat_gonce)) {
    gsize cat_done;

    cat_done = (gsize) _gst_debug_category_new ("video-blending", 0,
        "video blending");

    g_once_init_leave (&cat_gonce, cat_done);
  }

  return (GstDebugCategory *) cat_gonce;
}

#else

#define ensure_debug_category() /* NOOP */

#endif /* GST_DISABLE_GST_DEBUG */

static void
matrix_identity (guint8 * tmpline, guint width)
{
}

static void
matrix_prea_rgb_to_yuv (guint8 * tmpline, guint width)
{
  int i;
  int a, r, g, b;
  int y, u, v;

  for (i = 0; i < width; i++) {
    a = tmpline[i * 4 + 0];
    r = tmpline[i * 4 + 1];
    g = tmpline[i * 4 + 2];
    b = tmpline[i * 4 + 3];
    if (a) {
      r = (r * 255 + a / 2) / a;
      g = (g * 255 + a / 2) / a;
      b = (b * 255 + a / 2) / a;
    }

    y = (47 * r + 157 * g + 16 * b + 4096) >> 8;
    u = (-26 * r - 87 * g + 112 * b + 32768) >> 8;
    v = (112 * r - 102 * g - 10 * b + 32768) >> 8;

    tmpline[i * 4 + 1] = CLAMP (y, 0, 255);
    tmpline[i * 4 + 2] = CLAMP (u, 0, 255);
    tmpline[i * 4 + 3] = CLAMP (v, 0, 255);
  }
}

static void
matrix_rgb_to_yuv (guint8 * tmpline, guint width)
{
  int i;
  int r, g, b;
  int y, u, v;

  for (i = 0; i < width; i++) {
    r = tmpline[i * 4 + 1];
    g = tmpline[i * 4 + 2];
    b = tmpline[i * 4 + 3];

    y = (47 * r + 157 * g + 16 * b + 4096) >> 8;
    u = (-26 * r - 87 * g + 112 * b + 32768) >> 8;
    v = (112 * r - 102 * g - 10 * b + 32768) >> 8;

    tmpline[i * 4 + 1] = CLAMP (y, 0, 255);
    tmpline[i * 4 + 2] = CLAMP (u, 0, 255);
    tmpline[i * 4 + 3] = CLAMP (v, 0, 255);
  }
}

static void
matrix_yuv_to_rgb (guint8 * tmpline, guint width)
{
  int i;
  int r, g, b;
  int y, u, v;

  for (i = 0; i < width; i++) {
    y = tmpline[i * 4 + 1];
    u = tmpline[i * 4 + 2];
    v = tmpline[i * 4 + 3];

    r = (298 * y + 459 * v - 63514) >> 8;
    g = (298 * y - 55 * u - 136 * v + 19681) >> 8;
    b = (298 * y + 541 * u - 73988) >> 8;

    tmpline[i * 4 + 1] = CLAMP (r, 0, 255);
    tmpline[i * 4 + 2] = CLAMP (g, 0, 255);
    tmpline[i * 4 + 3] = CLAMP (b, 0, 255);
  }
}

/**
 * gst_video_blend_scale_linear_RGBA:
 * @src: the #GstVideoInfo describing the video data in @src_buffer
 * @src_buffer: the source buffer containing video pixels to scale
 * @dest_height: the height in pixels to scale the video data in @src_buffer to
 * @dest_width: the width in pixels to scale the video data in @src_buffer to
 * @dest: (out): pointer to a #GstVideoInfo structure that will be filled in
 *     with the details for @dest_buffer
 * @dest_buffer: (out): a pointer to a #GstBuffer variable, which will be
 *     set to a newly-allocated buffer containing the scaled pixels.
 *
 * Scales a buffer containing RGBA (or AYUV) video. This is an internal
 * helper function which is used to scale subtitle overlays, and may be
 * deprecated in the near future. Use #GstVideoScaler to scale video buffers
 * instead.
 */
/* returns newly-allocated buffer, which caller must unref */
void
gst_video_blend_scale_linear_RGBA (GstVideoInfo * src, GstBuffer * src_buffer,
    gint dest_height, gint dest_width, GstVideoInfo * dest,
    GstBuffer ** dest_buffer)
{
  const guint8 *src_pixels;
  int acc;
  int y_increment;
  int x_increment;
  int y1;
  int i;
  int j;
  int x;
  int dest_size;
  guint dest_stride;
  guint src_stride;
  guint8 *dest_pixels;
  guint8 *tmpbuf;
  GstVideoFrame src_frame, dest_frame;

  g_return_if_fail (dest_buffer != NULL);

  gst_video_info_init (dest);
  if (!gst_video_info_set_format (dest, GST_VIDEO_INFO_FORMAT (src),
          dest_width, dest_height)) {
    g_warn_if_reached ();
    return;
  }

  tmpbuf = g_malloc (dest_width * 8 * 4);

  *dest_buffer = gst_buffer_new_and_alloc (GST_VIDEO_INFO_SIZE (dest));

  gst_video_frame_map (&src_frame, src, src_buffer, GST_MAP_READ);
  gst_video_frame_map (&dest_frame, dest, *dest_buffer, GST_MAP_WRITE);

  if (dest_height == 1 || src->height == 1)
    y_increment = 0;
  else
    y_increment = ((src->height - 1) << 16) / (dest_height - 1) - 1;

  if (dest_width == 1 || src->width == 1)
    x_increment = 0;
  else
    x_increment = ((src->width - 1) << 16) / (dest_width - 1) - 1;

  dest_size = dest_stride = dest_width * 4;
  src_stride = GST_VIDEO_FRAME_PLANE_STRIDE (&src_frame, 0);

#define LINE(x) ((tmpbuf) + (dest_size)*((x)&1))

  dest_pixels = GST_VIDEO_FRAME_PLANE_DATA (&dest_frame, 0);
  src_pixels = GST_VIDEO_FRAME_PLANE_DATA (&src_frame, 0);

  acc = 0;
  video_orc_resample_bilinear_u32 (LINE (0), src_pixels, 0, x_increment,
      dest_width);
  y1 = 0;
  for (i = 0; i < dest_height; i++) {
    j = acc >> 16;
    x = acc & 0xffff;

    if (x == 0) {
      memcpy (dest_pixels + i * dest_stride, LINE (j), dest_size);
    } else {
      if (j > y1) {
        video_orc_resample_bilinear_u32 (LINE (j),
            src_pixels + j * src_stride, 0, x_increment, dest_width);
        y1++;
      }
      if (j >= y1) {
        video_orc_resample_bilinear_u32 (LINE (j + 1),
            src_pixels + (j + 1) * src_stride, 0, x_increment, dest_width);
        y1++;
      }
      video_orc_merge_linear_u8 (dest_pixels + i * dest_stride,
          LINE (j), LINE (j + 1), (x >> 8), dest_width * 4);
    }

    acc += y_increment;
  }

  gst_video_frame_unmap (&src_frame);
  gst_video_frame_unmap (&dest_frame);

  g_free (tmpbuf);
}

/*
 * A OVER B alpha compositing operation, with:
 *  max: maximum value a color can have
 *  alphaG: global alpha to apply on the source color
 *     -> only needed for premultiplied source
 *  alphaA: source pixel alpha
 *  colorA: source pixel color
 *  alphaB: destination pixel alpha
 *  colorB: destination pixel color
 *  alphaD: blended pixel alpha
 *     -> only needed for premultiplied destination
 */

/* Source non-premultiplied, Destination non-premultiplied */
#define OVER00_8BIT(max, alphaG, alphaA, colorA, alphaB, colorB, alphaD) \
  ((colorA * alphaA + colorB * alphaB * (max - alphaA) / max) / alphaD)

#define OVER00_16BIT(max, alphaG, alphaA, colorA, alphaB, colorB, alphaD) \
  ((colorA * alphaA + (guint64) colorB * alphaB * (max - alphaA) / max) / alphaD)

/* Source premultiplied, Destination non-premultiplied */
#define OVER10_8BIT(max, alphaG, alphaA, colorA, alphaB, colorB, alphaD) \
  ((colorA * alphaG + colorB * alphaB * (max - alphaA) / max) / alphaD)

#define OVER10_16BIT(max, alphaG, alphaA, colorA, alphaB, colorB, alphaD) \
  ((colorA * alphaG + (guint64) colorB * alphaB * (max - alphaA) / max) / alphaD)

/* Source non-premultiplied, Destination premultiplied */
#define OVER01(max, alphaG, alphaA, colorA, alphaB, colorB, alphaD) \
  ((colorA * alphaA + colorB * (max - alphaA)) / max)

/* Source premultiplied, Destination premultiplied */
#define OVER11(max, alphaG, alphaA, colorA, alphaB, colorB, alphaD) \
  ((colorA * alphaG + colorB * (max - alphaA)) / max)

#define BLENDC(op, max, global_alpha, aa, ca, ab, cb, dest_alpha) \
G_STMT_START { \
  int c = op((max), (global_alpha), (aa), (ca), (ab), (cb), (dest_alpha)); \
  cb = MIN(c, (max)); \
} G_STMT_END


/**
 * gst_video_blend:
 * @dest: The #GstVideoFrame where to blend @src in
 * @src: the #GstVideoFrame that we want to blend into
 * @x: The x offset in pixel where the @src image should be blended
 * @y: the y offset in pixel where the @src image should be blended
 * @global_alpha: the global_alpha each per-pixel alpha value is multiplied
 *                with
 *
 * Lets you blend the @src image into the @dest image
 */
gboolean
gst_video_blend (GstVideoFrame * dest,
    GstVideoFrame * src, gint x, gint y, gfloat global_alpha)
{
  gint i, j, global_alpha_val, src_width, src_height, dest_width, dest_height;
  gint src_xoff = 0, src_yoff = 0;
  guint8 *tmpdestline = NULL, *tmpsrcline = NULL;
  gboolean src_premultiplied_alpha, dest_premultiplied_alpha;
  gint bpp;
  void (*matrix) (guint8 * tmpline, guint width);
  const GstVideoFormatInfo *sinfo, *dinfo, *dunpackinfo, *sunpackinfo;

  g_assert (dest != NULL);
  g_assert (src != NULL);

  dest_premultiplied_alpha =
      GST_VIDEO_INFO_FLAGS (&dest->info) & GST_VIDEO_FLAG_PREMULTIPLIED_ALPHA;
  src_premultiplied_alpha =
      GST_VIDEO_INFO_FLAGS (&src->info) & GST_VIDEO_FLAG_PREMULTIPLIED_ALPHA;

  src_width = GST_VIDEO_FRAME_WIDTH (src);
  src_height = GST_VIDEO_FRAME_HEIGHT (src);

  dest_width = GST_VIDEO_FRAME_WIDTH (dest);
  dest_height = GST_VIDEO_FRAME_HEIGHT (dest);

  ensure_debug_category ();

  GST_LOG ("blend src %dx%d onto dest %dx%d @ %d,%d", src_width, src_height,
      dest_width, dest_height, x, y);

  /* In case overlay is completely outside the video, don't render */
  if (x + src_width <= 0 || y + src_height <= 0
      || x >= dest_width || y >= dest_height) {
    goto nothing_to_do;
  }

  dinfo = gst_video_format_get_info (GST_VIDEO_FRAME_FORMAT (dest));
  sinfo = gst_video_format_get_info (GST_VIDEO_FRAME_FORMAT (src));

  if (!sinfo || !dinfo)
    goto failed;

  dunpackinfo = gst_video_format_get_info (dinfo->unpack_format);
  sunpackinfo = gst_video_format_get_info (sinfo->unpack_format);

  if (dunpackinfo == NULL || sunpackinfo == NULL)
    goto failed;

  g_assert (GST_VIDEO_FORMAT_INFO_BITS (sunpackinfo) == 8);

  if (GST_VIDEO_FORMAT_INFO_BITS (dunpackinfo) != 8
      && GST_VIDEO_FORMAT_INFO_BITS (dunpackinfo) != 16)
    goto unpack_format_not_supported;

  /* Source is always 8 bit but destination might be 8 or 16 bit */
  bpp = 4 * (GST_VIDEO_FORMAT_INFO_BITS (dunpackinfo) / 8);

  global_alpha_val = (bpp == 4) ? 255.0 * global_alpha : 65535.0 * global_alpha;

  matrix = matrix_identity;
  if (GST_VIDEO_INFO_IS_RGB (&src->info) != GST_VIDEO_INFO_IS_RGB (&dest->info)) {
    if (GST_VIDEO_INFO_IS_RGB (&src->info)) {
      if (src_premultiplied_alpha) {
        matrix = matrix_prea_rgb_to_yuv;
        src_premultiplied_alpha = FALSE;
      } else {
        matrix = matrix_rgb_to_yuv;
      }
    } else {
      matrix = matrix_yuv_to_rgb;
    }
  }

  /* If we're here we know that the overlay image fully or
   * partially overlaps with the video frame */

  /* adjust src image for negative offsets */
  if (x < 0) {
    src_xoff = -x;
    src_width -= src_xoff;
    x = 0;
  }

  if (y < 0) {
    src_yoff = -y;
    src_height -= src_yoff;
    y = 0;
  }

  /* adjust width/height to render (i.e. clip source image) if the source
   * image extends beyond the right or bottom border of the video surface */
  if (x + src_width > dest_width)
    src_width = dest_width - x;

  if (y + src_height > dest_height)
    src_height = dest_height - y;

  tmpsrcline = g_malloc (sizeof (guint8) * (src_width + 8) * 4);
  tmpdestline = g_malloc (sizeof (guint8) * (dest_width + 8) * bpp);

  /* Mainloop doing the needed conversions, and blending */
  for (i = y; i < y + src_height; i++, src_yoff++) {

    dinfo->unpack_func (dinfo, 0, tmpdestline, dest->data, dest->info.stride,
        0, i, dest_width);
    sinfo->unpack_func (sinfo, 0, tmpsrcline, src->data, src->info.stride,
        src_xoff, src_yoff, src_width);

    /* FIXME: use the x parameter of the unpack func once implemented */
    tmpdestline += bpp * x;

    matrix (tmpsrcline, src_width);

#define BLENDLOOP(op, dest_type, max, shift, alpha_val)                                       \
  G_STMT_START {                                                                              \
    for (j = 0; j < src_width * 4; j += 4) {                                                  \
      guint asrc, adst;                                                                       \
      guint final_alpha;                                                                      \
      dest_type * dest = (dest_type *) tmpdestline;                                           \
                                                                                              \
      asrc = ((guint) tmpsrcline[j]) * alpha_val / max;                                       \
      asrc = asrc << shift;                                                                   \
      if (asrc == 0)                                                                          \
        continue;                                                                             \
                                                                                              \
      adst = dest[j];                                                                         \
      final_alpha = asrc + adst * (max - asrc) / max;                                         \
      dest[j] = final_alpha;                                                                  \
      if (final_alpha == 0)                                                                   \
        final_alpha = 1;                                                                      \
                                                                                              \
      BLENDC (op, max, alpha_val, asrc, tmpsrcline[j + 1] << shift, adst, dest[j + 1], final_alpha); \
      BLENDC (op, max, alpha_val, asrc, tmpsrcline[j + 2] << shift, adst, dest[j + 2], final_alpha); \
      BLENDC (op, max, alpha_val, asrc, tmpsrcline[j + 3] << shift, adst, dest[j + 3], final_alpha); \
    }                                                                                         \
  } G_STMT_END

    if (bpp == 4) {
      if (G_LIKELY (global_alpha_val == 255)) {
        if (src_premultiplied_alpha && dest_premultiplied_alpha) {
          BLENDLOOP (OVER11, guint8, 255, 0, 255);
        } else if (!src_premultiplied_alpha && dest_premultiplied_alpha) {
          BLENDLOOP (OVER01, guint8, 255, 0, 255);
        } else if (src_premultiplied_alpha && !dest_premultiplied_alpha) {
          BLENDLOOP (OVER10_8BIT, guint8, 255, 0, 255);
        } else {
          BLENDLOOP (OVER00_8BIT, guint8, 255, 0, 255);
        }
      } else {
        if (src_premultiplied_alpha && dest_premultiplied_alpha) {
          BLENDLOOP (OVER11, guint8, 255, 0, global_alpha_val);
        } else if (!src_premultiplied_alpha && dest_premultiplied_alpha) {
          BLENDLOOP (OVER01, guint8, 255, 0, global_alpha_val);
        } else if (src_premultiplied_alpha && !dest_premultiplied_alpha) {
          BLENDLOOP (OVER10_8BIT, guint8, 255, 0, global_alpha_val);
        } else {
          BLENDLOOP (OVER00_8BIT, guint8, 255, 0, global_alpha_val);
        }
      }
    } else {
      g_assert (bpp == 8);

      if (G_LIKELY (global_alpha_val == 65535)) {
        if (src_premultiplied_alpha && dest_premultiplied_alpha) {
          BLENDLOOP (OVER11, guint16, 65535, 8, 65535);
        } else if (!src_premultiplied_alpha && dest_premultiplied_alpha) {
          BLENDLOOP (OVER01, guint16, 65535, 8, 65535);
        } else if (src_premultiplied_alpha && !dest_premultiplied_alpha) {
          BLENDLOOP (OVER10_16BIT, guint16, 65535, 8, 65535);
        } else {
          BLENDLOOP (OVER00_16BIT, guint16, 65535, 8, 65535);
        }
      } else {
        if (src_premultiplied_alpha && dest_premultiplied_alpha) {
          BLENDLOOP (OVER11, guint16, 65535, 8, global_alpha_val);
        } else if (!src_premultiplied_alpha && dest_premultiplied_alpha) {
          BLENDLOOP (OVER01, guint16, 65535, 8, global_alpha_val);
        } else if (src_premultiplied_alpha && !dest_premultiplied_alpha) {
          BLENDLOOP (OVER10_16BIT, guint16, 65535, 8, global_alpha_val);
        } else {
          BLENDLOOP (OVER00_16BIT, guint16, 65535, 8, global_alpha_val);
        }
      }
    }

#undef BLENDLOOP

    /* undo previous pointer adjustments to pass right pointer to g_free */
    tmpdestline -= bpp * x;

    /* FIXME
     * #if G_BYTE_ORDER == LITTLE_ENDIAN
     * video_orc_blend_little (tmpdestline, tmpsrcline, dest->width);
     * #else
     * video_orc_blend_big (tmpdestline, tmpsrcline, src->width);
     * #endif
     */

    dinfo->pack_func (dinfo, 0, tmpdestline, dest_width,
        dest->data, dest->info.stride, dest->info.chroma_site, i, dest_width);
  }

  g_free (tmpdestline);
  g_free (tmpsrcline);

  return TRUE;

failed:
  {
    GST_WARNING ("Could not do the blending");
    return FALSE;
  }
unpack_format_not_supported:
  {
    GST_FIXME ("video format %s not supported yet for blending",
        gst_video_format_to_string (dinfo->unpack_format));
    return FALSE;
  }
nothing_to_do:
  {
    GST_LOG
        ("Overlay completely outside the video surface, hence not rendering");
    return TRUE;
  }
}
