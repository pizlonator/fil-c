/* GStreamer
 * Copyright (C) 2013 Wim Taymans <wim.taymans@gmail.com>
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

#include "video-orc.h"
#include "video-format.h"
#include <gst/video/video-enumtypes.h>

/**
 * SECTION:gstvideochroma
 * @title: GstVideoChromaResample
 * @short_description: Functions and utility object for operating on chroma video planes
 *
 * The functions gst_video_chroma_from_string() and gst_video_chroma_to_string() convert
 * between #GstVideoChromaSite and string descriptions.
 *
 * #GstVideoChromaResample is a utility object for resampling chroma planes
 * and converting between different chroma sampling sitings.
 *
 */

#ifndef GST_DISABLE_GST_DEBUG
#define GST_CAT_DEFAULT ensure_debug_category()
static GstDebugCategory *
ensure_debug_category (void)
{
  static gsize cat_gonce = 0;

  if (g_once_init_enter (&cat_gonce)) {
    gsize cat_done;

    cat_done = (gsize) _gst_debug_category_new ("video-chroma", 0,
        "video-chroma object");

    g_once_init_leave (&cat_gonce, cat_done);
  }

  return (GstDebugCategory *) cat_gonce;
}
#else
#define ensure_debug_category() /* NOOP */
#endif /* GST_DISABLE_GST_DEBUG */

typedef struct
{
  const gchar *name;
  GstVideoChromaSite site;
} ChromaSiteInfo;

static const ChromaSiteInfo chromasite[] = {
  {"jpeg", GST_VIDEO_CHROMA_SITE_JPEG},
  {"mpeg2", GST_VIDEO_CHROMA_SITE_MPEG2},
  {"dv", GST_VIDEO_CHROMA_SITE_DV},
  {"alt-line", GST_VIDEO_CHROMA_SITE_ALT_LINE},
  {"cosited", GST_VIDEO_CHROMA_SITE_COSITED},
};

/**
 * gst_video_chroma_from_string:
 * @s: a chromasite string
 *
 * Convert @s to a #GstVideoChromaSite
 *
 * Deprecated: 1.20: Use gst_video_chroma_site_from_string() instead.
 *
 * Returns: a #GstVideoChromaSite or %GST_VIDEO_CHROMA_SITE_UNKNOWN when @s does
 * not contain a valid chroma description.
 */
GstVideoChromaSite
gst_video_chroma_from_string (const gchar * s)
{
  return gst_video_chroma_site_from_string (s);
}

/**
 * gst_video_chroma_site_from_string:
 * @s: a chromasite string
 *
 * Convert @s to a #GstVideoChromaSite
 *
 * Returns: a #GstVideoChromaSite or %GST_VIDEO_CHROMA_SITE_UNKNOWN when @s does
 * not contain a valid chroma-site description.
 *
 * Since: 1.20
 */
GstVideoChromaSite
gst_video_chroma_site_from_string (const gchar * s)
{
  gint i;
  gchar **split;
  gchar **iter;
  GstVideoChromaSite ret = GST_VIDEO_CHROMA_SITE_UNKNOWN;
  GFlagsClass *klass;

  for (i = 0; i < G_N_ELEMENTS (chromasite); i++) {
    if (g_str_equal (chromasite[i].name, s))
      return chromasite[i].site;
  }

  klass = (GFlagsClass *) g_type_class_ref (GST_TYPE_VIDEO_CHROMA_SITE);
  split = g_strsplit (s, "+", 0);
  for (iter = split; *iter; iter++) {
    GFlagsValue *value;

    value = g_flags_get_value_by_nick (klass, *iter);
    if (!value) {
      ret = GST_VIDEO_CHROMA_SITE_UNKNOWN;
      goto out;
    }

    ret |= value->value;
  }

out:
  g_type_class_unref (klass);
  g_strfreev (split);

  /* Doesn't make sense */
  if ((ret & GST_VIDEO_CHROMA_SITE_NONE) != 0 &&
      ret != GST_VIDEO_CHROMA_SITE_NONE)
    return GST_VIDEO_CHROMA_SITE_UNKNOWN;

  return ret;
}

/**
 * gst_video_chroma_to_string:
 * @site: a #GstVideoChromaSite
 *
 * Converts @site to its string representation.
 *
 * Deprecated: 1.20: Use gst_video_chroma_site_to_string() instead.
 *
 * Returns: a string describing @site.
 */
const gchar *
gst_video_chroma_to_string (GstVideoChromaSite site)
{
  gint i;
  for (i = 0; i < G_N_ELEMENTS (chromasite); i++) {
    if (chromasite[i].site == site)
      return chromasite[i].name;
  }
  return NULL;
}

/**
 * gst_video_chroma_site_to_string:
 * @site: a #GstVideoChromaSite
 *
 * Converts @site to its string representation.
 *
 * Returns: (transfer full) (nullable): a string representation of @site
 *          or %NULL if @site contains undefined value or
 *          is equal to %GST_VIDEO_CHROMA_SITE_UNKNOWN
 *
 * Since: 1.20
 */
gchar *
gst_video_chroma_site_to_string (GstVideoChromaSite site)
{
  gint i;
  GString *str;
  GFlagsValue *value;
  GFlagsClass *klass;

  /* return null string for GST_VIDEO_CHROMA_SITE_UNKNOWN */
  if (site == 0)
    return NULL;

  for (i = 0; i < G_N_ELEMENTS (chromasite); i++) {
    if (chromasite[i].site == site)
      return g_strdup (chromasite[i].name);
  }

  /* Doesn't make sense */
  if ((site & GST_VIDEO_CHROMA_SITE_NONE) != 0 &&
      site != GST_VIDEO_CHROMA_SITE_NONE)
    return NULL;

  /* Construct new string */
  klass = (GFlagsClass *) g_type_class_ref (GST_TYPE_VIDEO_CHROMA_SITE);
  str = g_string_new (NULL);
  while (site != GST_VIDEO_CHROMA_SITE_UNKNOWN &&
      (value = g_flags_get_first_value (klass, site))) {
    if (str->len > 0)
      g_string_append (str, "+");

    g_string_append (str, value->value_nick);
    site &= ~value->value;
  }
  g_type_class_unref (klass);

  /* This means given chroma-site has unknown value */
  if (site != 0)
    return g_string_free (str, TRUE);

  return g_string_free (str, FALSE);
}

struct _GstVideoChromaResample
{
  GstVideoChromaMethod method;
  GstVideoChromaSite site;
  GstVideoChromaFlags flags;
  GstVideoFormat format;
  gint h_factor, v_factor;
  guint n_lines;
  gint offset;
  void (*h_resample) (GstVideoChromaResample * resample, gpointer pixels,
      gint width);
  void (*v_resample) (GstVideoChromaResample * resample, gpointer lines[],
      gint width);
};

#define PR(i)          (p[2 + 4 * (i)])
#define PB(i)          (p[3 + 4 * (i)])

#define PR0(i)         (l0[2 + 4 * (i)])
#define PR1(i)         (l1[2 + 4 * (i)])
#define PR2(i)         (l2[2 + 4 * (i)])
#define PR3(i)         (l3[2 + 4 * (i)])
#define PB0(i)         (l0[3 + 4 * (i)])
#define PB1(i)         (l1[3 + 4 * (i)])
#define PB2(i)         (l2[3 + 4 * (i)])
#define PB3(i)         (l3[3 + 4 * (i)])

#define FILT_1_1(a,b)          ((a) + (b) + 1) >> 1
#define FILT_1_3_3_1(a,b,c,d)  ((a) + 3*((b)+(c)) + (d) + 4) >> 3

#define FILT_3_1(a,b)          (3*(a) + (b) + 2) >> 2
#define FILT_1_3(a,b)          ((a) + 3*(b) + 2) >> 2
#define FILT_1_2_1(a,b,c)      ((a) + 2*(b) + (c) + 2) >> 2

#define FILT_7_1(a,b)          (7*(a) + 1*(b) + 4) >> 3
#define FILT_1_7(a,b)          (1*(a) + 7*(b) + 4) >> 3

#define FILT_5_3(a,b)          (5*(a) + 3*(b) + 4) >> 3
#define FILT_3_5(a,b)          (3*(a) + 5*(b) + 4) >> 3

#define FILT_10_3_2_1(a,b,c,d)      (10*(a) + 3*(b) + 2*(c) + (d) + 8) >> 4
#define FILT_1_2_3_10(a,b,c,d)      ((a) + 2*(b) + 3*(c) + 10*(d) + 8) >> 4
#define FILT_1_2_3_4_3_2_1(a,b,c,d,e,f,g) ((a) + 2*((b)+(f)) + 3*((c)+(e)) + 4*(d) + (g) + 8) >> 4

/* 2x horizontal upsampling without cositing
 *
 * +----------    a
 * | +------ (3*a +   b + 2) >> 2
 * | | +---- (  a + 3*b + 2) >> 2
 * v v v
 * O-O-O-O-
 *  x   x
 *  a   b
 */
#define MAKE_UPSAMPLE_H2(name,type)                                     \
static void                                                             \
video_chroma_up_h2_##name (GstVideoChromaResample *resample,            \
    gpointer pixels, gint width)                                        \
{                                                                       \
  type *p = pixels;                                                     \
  gint i;                                                               \
  type tr0, tr1;                                                        \
  type tb0, tb1;                                                        \
                                                                        \
  tr1 = PR(0);                                                          \
  tb1 = PB(0);                                                          \
  for (i = 1; i < width - 1; i += 2) {                                  \
    tr0 = tr1, tr1 = PR(i+1);                                           \
    tb0 = tb1, tb1 = PB(i+1);                                           \
                                                                        \
    PR(i) = FILT_3_1 (tr0, tr1);                                        \
    PB(i) = FILT_3_1 (tb0, tb1);                                        \
    PR(i+1) = FILT_1_3 (tr0, tr1);                                      \
    PB(i+1) = FILT_1_3 (tb0, tb1);                                      \
  }                                                                     \
}

/* 2x vertical upsampling without cositing
 *
 *   O--O--O-  <---- a
 * a x  x  x
 *   O--O--O-  <---- (3*a +   b + 2) >> 2
 *   O--O--O-  <-----(  a + 3*b + 2) >> 2
 * b x  x  x
 *   O--O--O-  <---- b
 */
#define MAKE_UPSAMPLE_V2(name,type)                                     \
static void                                                             \
video_chroma_up_v2_##name (GstVideoChromaResample *resample,            \
    gpointer lines[], gint width)                                       \
{                                                                       \
  type *l0 = lines[0];                                                  \
  type *l1 = lines[1];                                                  \
                                                                        \
  if (resample->h_resample) {                                           \
    resample->h_resample (resample, l0, width);                         \
    if (l0 != l1)                                                       \
      resample->h_resample (resample, l1, width);                       \
  }                                                                     \
  if (l0 != l1) {                                                       \
    type *d0 = l0;							\
    type *d1 = l1;							\
    video_orc_chroma_up_v2_##name (d0, d1, l0, l1, width);              \
  }                                                                     \
}
/* 2x vertical upsampling interlaced without cositing
 *
 *   even           odd
 *
 *   O--O--O--------------- <---  a
 * a x  x  x
 *   --------------O--O--O- <---  c
 *   O--O--O--------------- <--- (5*a + 3*b + 4) >> 3
 * c               x  x  x
 *   --------------O--O--O- <--- (7*c +   d + 4) >> 3
 *   O--O--O--------------- <--- (  a + 7*b + 4) >> 3
 * b x  x  x
 *   --------------O--O--O- <--- (3*c + 5*d + 4) >> 3
 *   O--O--O---------------
 * d               x  x  x
 *   --------------O--O--O-
 */
#define MAKE_UPSAMPLE_VI2(name,type)                                    \
static void                                                             \
video_chroma_up_vi2_##name (GstVideoChromaResample *resample,           \
    gpointer lines[], gint width)                                       \
{                                                                       \
  gint i;                                                               \
  type *l0 = lines[0];                                                  \
  type *l1 = lines[1];                                                  \
  type *l2 = lines[2];                                                  \
  type *l3 = lines[3];                                                  \
  type tr0, tr1, tr2, tr3;                                              \
  type tb0, tb1, tb2, tb3;                                              \
                                                                        \
  if (resample->h_resample) {                                           \
    if (l0 != l1) {                                                     \
      resample->h_resample (resample, l0, width);                       \
      resample->h_resample (resample, l1, width);                       \
    }                                                                   \
    if (l2 != l3) {                                                     \
      resample->h_resample (resample, l2, width);                       \
      resample->h_resample (resample, l3, width);                       \
    }                                                                   \
  }                                                                     \
  if (l0 != l1 && l2 != l3) {                                           \
    for (i = 0; i < width; i++) {                                       \
      tr0 = PR0(i), tr2 = PR2(i);                                       \
      tb0 = PB0(i), tb2 = PB2(i);                                       \
      tr1 = PR1(i), tr3 = PR3(i);                                       \
      tb1 = PB1(i), tb3 = PB3(i);                                       \
                                                                        \
      PR0(i) = FILT_5_3 (tr0, tr2);                                     \
      PB0(i) = FILT_5_3 (tb0, tb2);                                     \
      PR1(i) = FILT_7_1 (tr1, tr3);                                     \
      PB1(i) = FILT_7_1 (tb1, tb3);                                     \
      PR2(i) = FILT_1_7 (tr0, tr2);                                     \
      PB2(i) = FILT_1_7 (tb0, tb2);                                     \
      PR3(i) = FILT_3_5 (tr1, tr3);                                     \
      PB3(i) = FILT_3_5 (tb1, tb3);                                     \
    }                                                                   \
  }                                                                     \
}

/* 2x horizontal downsampling without cositing
 *
 *  +------ (a + b+ 1) >> 1
 *  |
 *  v
 * -O---O--
 * x x x x
 * a b c d
 */
#define MAKE_DOWNSAMPLE_H2_ORC(name,type)                               \
static void                                                             \
video_chroma_down_h2_##name (GstVideoChromaResample *resample,          \
    gpointer pixels, gint width)                                        \
{                                                                       \
  type *p = pixels;                                                     \
  type *d = p;								\
                                                                        \
  video_orc_chroma_down_h2_##name (d, p, width / 2);                    \
}

#define MAKE_DOWNSAMPLE_H2(name,type)                                   \
static void                                                             \
video_chroma_down_h2_##name (GstVideoChromaResample *resample,          \
    gpointer pixels, gint width)                                        \
{                                                                       \
  type *p = pixels;                                                     \
  gint i;                                                               \
                                                                        \
  for (i = 0; i < width - 1; i += 2) {                                  \
    type tr0 = PR(i), tr1 = PR(i+1);                                    \
    type tb0 = PB(i), tb1 = PB(i+1);                                    \
                                                                        \
    PR(i) = FILT_1_1 (tr0, tr1);                                        \
    PB(i) = FILT_1_1 (tb0, tb1);                                        \
  }                                                                     \
}
/* 2x vertical downsampling without cositing
 *
 * a x--x--x-
 *   O  O  O <---- (a + b + 1) >> 1
 * b x--x--x-
 * c x--x--x-
 *   O  O  O
 * d x--x--x-
 */
#define MAKE_DOWNSAMPLE_V2(name,type)                                   \
static void                                                             \
video_chroma_down_v2_##name (GstVideoChromaResample *resample,          \
    gpointer lines[], gint width)                                       \
{                                                                       \
  type *l0 = lines[0];                                                  \
  type *l1 = lines[1];                                                  \
                                                                        \
  if (l0 != l1) {							\
    type *d0 = l0;							\
    video_orc_chroma_down_v2_##name (d0, l0, l1, width);                \
  }									\
                                                                        \
  if (resample->h_resample)                                             \
    resample->h_resample (resample, l0, width);                         \
}
/* 2x vertical downsampling interlaced without cositing
 *
 *   even           odd
 *
 * a x--x--x---------------
 *   O  O  O                <---
 * b --------------x--x--x-
 * c x--x--x---------------
 *                 O  O  O  <---
 * d --------------x--x--x-
 */
#define MAKE_DOWNSAMPLE_VI2(name,type)                                  \
static void                                                             \
video_chroma_down_vi2_##name (GstVideoChromaResample *resample,         \
    gpointer lines[], gint width)                                       \
{                                                                       \
  /* FIXME */                                                           \
  if (resample->h_resample)                                             \
    resample->h_resample (resample, lines[0], width);                   \
}

MAKE_UPSAMPLE_H2 (u16, guint16);
MAKE_UPSAMPLE_H2 (u8, guint8);
MAKE_UPSAMPLE_V2 (u16, guint16);
MAKE_UPSAMPLE_V2 (u8, guint8);
MAKE_UPSAMPLE_VI2 (u16, guint16);
MAKE_UPSAMPLE_VI2 (u8, guint8);
MAKE_DOWNSAMPLE_H2 (u16, guint16);
MAKE_DOWNSAMPLE_H2_ORC (u8, guint8);
MAKE_DOWNSAMPLE_V2 (u16, guint16);
MAKE_DOWNSAMPLE_V2 (u8, guint8);
MAKE_DOWNSAMPLE_VI2 (u16, guint16);
MAKE_DOWNSAMPLE_VI2 (u8, guint8);

/* 4x horizontal upsampling without cositing
 *
 *     +---------- (7*a +   b + 4) >> 3
 *     | +-------- (5*a + 3*b + 4) >> 3
 * a a | | +------ (3*a + 5*b + 4) >> 3
 * | | | | | +---- (  a + 7*b + 4) >> 3
 * v v v v v v
 * O-O-O-O-O-O-O-O-
 *    x       x
 *    a       b
 */
#define MAKE_UPSAMPLE_H4(name,type)                                          \
static void                                                             \
video_chroma_up_h4_##name (GstVideoChromaResample *resample,           \
    gpointer pixels, gint width)                        \
{                                                                       \
  type *p = pixels;                                                     \
  gint i;                                                               \
  type tr0, tr1;                                                        \
  type tb0, tb1;                                                        \
                                                                        \
  tr1 = PR(0);                                                          \
  tb1 = PB(0);                                                          \
  for (i = 2; i < width - 3; i += 4) {                                  \
    tr0 = tr1, tr1 = PR(i+2);                                           \
    tb0 = tb1, tb1 = PB(i+2);                                           \
                                                                        \
    PR(i) = FILT_7_1 (tr0, tr1);                                        \
    PB(i) = FILT_7_1 (tb0, tb1);                                        \
    PR(i+1) = FILT_5_3 (tr0, tr1);                                      \
    PB(i+1) = FILT_5_3 (tb0, tb1);                                      \
    PR(i+2) = FILT_3_5 (tr0, tr1);                                      \
    PB(i+2) = FILT_3_5 (tb0, tb1);                                      \
    PR(i+3) = FILT_1_7 (tr0, tr1);                                      \
    PB(i+3) = FILT_1_7 (tb0, tb1);                                      \
  }                                                                     \
}

/* 4x vertical upsampling without cositing
 *
 *   O--O--O-  <---- a
 *   O--O--O-  <---- a
 * a x  x  x
 *   O--O--O-  <---- (7*a +   b + 4) >> 3
 *   O--O--O-  <---- (5*a + 3*b + 4) >> 3
 *   O--O--O-  <---- (3*a + 5*b + 4) >> 3
 *   O--O--O-  <-----(  a + 7*b + 4) >> 3
 * b x  x  x
 *   O--O--O-
 *   O--O--O-
 */
#define MAKE_UPSAMPLE_V4(name,type)                                          \
static void                                                             \
video_chroma_up_v4_##name (GstVideoChromaResample *resample,            \
    gpointer lines[], gint width)                                       \
{                                                                       \
  gint i;                                                               \
  type *l0 = lines[0];                                                  \
  type *l1 = lines[1];                                                  \
  type *l2 = lines[2];                                                  \
  type *l3 = lines[3];                                                  \
  type tr0, tr1;                                                        \
  type tb0, tb1;                                                        \
                                                                        \
  if (resample->h_resample) {                                           \
    if (l0 != l1) {                                                     \
      resample->h_resample (resample, l0, width);                       \
      resample->h_resample (resample, l1, width);                       \
    }                                                                   \
    if (l2 != l3) {                                                     \
      resample->h_resample (resample, l2, width);                       \
      resample->h_resample (resample, l3, width);                       \
    }                                                                   \
  }                                                                     \
  if (l0 != l1 && l2 != l3) {                                           \
    for (i = 0; i < width; i++) {                                       \
      tr0 = PR0(i), tr1 = PR2(i);                                       \
      tb0 = PB0(i), tb1 = PB2(i);                                       \
                                                                        \
      PR0(i) = FILT_7_1 (tr0, tr1);                                     \
      PB0(i) = FILT_7_1 (tb0, tb1);                                     \
      PR1(i) = FILT_5_3 (tr0, tr1);                                     \
      PB1(i) = FILT_5_3 (tb0, tb1);                                     \
      PR2(i) = FILT_3_5 (tr0, tr1);                                     \
      PB2(i) = FILT_3_5 (tb0, tb1);                                     \
      PR3(i) = FILT_1_7 (tr0, tr1);                                     \
      PB3(i) = FILT_1_7 (tb0, tb1);                                     \
    }                                                                   \
  }                                                                     \
}
/* 4x vertical upsampling interlaced without cositing
 *
 */
#define MAKE_UPSAMPLE_VI4(name,type)                                         \
static void                                                             \
video_chroma_up_vi4_##name (GstVideoChromaResample *resample,           \
    gpointer lines[], gint width)                                       \
{                                                                       \
  /* FIXME */                                                           \
  if (resample->h_resample) {                                           \
    resample->h_resample (resample, lines[0], width);                   \
  }                                                                     \
}

/* 4x horizontal downsampling without cositing
 *
 *    +------ (a + 3*b + 3*c + d + 4) >> 3
 *    |
 *    v
 * ---O-------O---
 * x x x x x x x x
 * a b c d e f g h
 */
#define MAKE_DOWNSAMPLE_H4(name,type)                                        \
static void                                                             \
video_chroma_down_h4_##name (GstVideoChromaResample *resample,         \
    gpointer pixels, gint width)                        \
{                                                                       \
  type *p = pixels;                                                     \
  gint i;                                                               \
                                                                        \
  for (i = 0; i < width - 4; i += 4) {                                  \
    type tr0 = PR(i), tr1 = PR(i+1), tr2 = PR(i+2), tr3 = PR(i+3);      \
    type tb0 = PB(i), tb1 = PB(i+1), tb2 = PB(i+2), tb3 = PB(i+3);      \
                                                                        \
    PR(i) = FILT_1_3_3_1 (tr0, tr1, tr2, tr3);                          \
    PB(i) = FILT_1_3_3_1 (tb0, tb1, tb2, tb3);                          \
  }                                                                     \
}

/* 4x vertical downsampling without cositing
 *
 * a x--x--x-
 * b x--x--x-
 *   O  O  O   <---- (a + 3*b + 3*c + d + 4) >> 4
 * c x--x--x-
 * d x--x--x-
 * e x--x--x-
 * f x--x--x-
 *   O  O  O
 * g x--x--x-
 * h x--x--x-
 */
#define MAKE_DOWNSAMPLE_V4(name,type)                               \
static void                                                             \
video_chroma_down_v4_##name (GstVideoChromaResample *resample,          \
    gpointer lines[], gint width)                                       \
{                                                                       \
  type *l0 = lines[0];                                                  \
  type *l1 = lines[1];                                                  \
  type *l2 = lines[2];                                                  \
  type *l3 = lines[3];                                                  \
  type *d = l0;								\
									\
  video_orc_chroma_down_v4_##name(d, l0, l1, l2, l3, width);		\
                                                                        \
  if (resample->h_resample)                                             \
    resample->h_resample (resample, l0, width);                         \
}
/* 4x vertical downsampling interlaced without cositing
 *
 */
#define MAKE_DOWNSAMPLE_VI4(name,type)                                       \
static void                                                             \
video_chroma_down_vi4_##name (GstVideoChromaResample *resample,         \
    gpointer lines[], gint width)                                       \
{                                                                       \
  /* FIXME */                                                           \
  if (resample->h_resample) {                                           \
    resample->h_resample (resample, lines[0], width);                   \
  }                                                                     \
}

MAKE_UPSAMPLE_H4 (u16, guint16);
MAKE_UPSAMPLE_H4 (u8, guint8);
MAKE_UPSAMPLE_V4 (u16, guint16);
MAKE_UPSAMPLE_V4 (u8, guint8);
MAKE_UPSAMPLE_VI4 (u16, guint16);
MAKE_UPSAMPLE_VI4 (u8, guint8);
MAKE_DOWNSAMPLE_H4 (u16, guint16);
MAKE_DOWNSAMPLE_H4 (u8, guint8);
MAKE_DOWNSAMPLE_V4 (u16, guint16);
MAKE_DOWNSAMPLE_V4 (u8, guint8);
MAKE_DOWNSAMPLE_VI4 (u16, guint16);
MAKE_DOWNSAMPLE_VI4 (u8, guint8);

/* 2x horizontal upsampling with cositing
 *
 * a +------ (a + b + 1) >> 1
 * | |
 * v v
 * O-O-O-O
 * x   x
 * a   b
 */
#define MAKE_UPSAMPLE_H2_CS_ORC(name,type)                              \
static void                                                             \
video_chroma_up_h2_cs_##name (GstVideoChromaResample *resample,         \
    gpointer pixels, gint width)                                        \
{                                                                       \
  type *p = pixels;                                                     \
  /* ORC version is slower */                                           \
  video_orc_chroma_up_h2_cs_##name (p, p, p, width-1);                  \
}

#define MAKE_UPSAMPLE_H2_CS(name,type)                                  \
static void                                                             \
video_chroma_up_h2_cs_##name (GstVideoChromaResample *resample,         \
    gpointer pixels, gint width)                                        \
{                                                                       \
  type *p = pixels;                                                     \
  gint i;                                                               \
                                                                        \
  for (i = 1; i < width - 1; i += 2) {                                  \
    PR(i) = FILT_1_1 (PR(i-1), PR(i+1));                                \
    PB(i) = FILT_1_1 (PB(i-1), PB(i+1));                                \
  }                                                                     \
}
/* 2x vertical upsampling with cositing
 *
 * a x O--O--O-  <---- a
 *     O--O--O-  <---- (a +  b + 1) >> 1
 * b x O--O--O-
 *     O--O--O-
 */
#define MAKE_UPSAMPLE_V2_CS(name,type)                                       \
static void                                                             \
video_chroma_up_v2_cs_##name (GstVideoChromaResample *resample,         \
    gpointer lines[], gint width)                                       \
{                                                                       \
  /* FIXME */                                                           \
  if (resample->h_resample) {                                           \
    resample->h_resample (resample, lines[0], width);                   \
  }                                                                     \
}
/* 2x vertical upsampling interlaced with cositing
 *
 */
#define MAKE_UPSAMPLE_VI2_CS(name,type)                                      \
static void                                                             \
video_chroma_up_vi2_cs_##name (GstVideoChromaResample *resample,        \
    gpointer lines[], gint width)                                       \
{                                                                       \
  /* FIXME */                                                           \
  if (resample->h_resample) {                                           \
    resample->h_resample (resample, lines[0], width);                   \
  }                                                                     \
}

/* 2x horizontal downsampling with cositing
 *
 * a
 * |   +------ (b + 2*c + d + 2) >> 2
 * v   v
 * O---O---O---
 * x x x x x x
 * a b c d e f
 */
#define MAKE_DOWNSAMPLE_H2_CS(name,type)                                     \
static void                                                             \
video_chroma_down_h2_cs_##name (GstVideoChromaResample *resample,       \
    gpointer pixels, gint width)                                        \
{                                                                       \
  type *p = pixels;                                                     \
  gint i;                                                               \
                                                                        \
  if (width < 2)                                                        \
    return;                                                             \
                                                                        \
  PR(0) = FILT_3_1 (PR(0), PR(1));                                      \
  PB(0) = FILT_3_1 (PB(0), PB(1));                                      \
                                                                        \
  for (i = 2; i < width - 2; i += 2) {                                  \
    PR(i) = FILT_1_2_1 (PR(i-1), PR(i), PR(i+1));                       \
    PB(i) = FILT_1_2_1 (PB(i-1), PB(i), PB(i+1));                       \
  }                                                                     \
  if (i < width) {                                                      \
    PR(i) = FILT_1_3 (PR(i-1), PR(i));                                  \
    PB(i) = FILT_1_3 (PB(i-1), PB(i));                                  \
  }                                                                     \
}
/* 2x vertical downsampling with cositing
 *
 * a x O--O--O-  <---- a
 * b x --------
 * c x O--O--O-  <---- (b + 2*c + d + 2) >> 2
 * d x --------
 * e x O--O--O-
 * f x --------
 */
#define MAKE_DOWNSAMPLE_V2_CS(name,type)                                     \
static void                                                             \
video_chroma_down_v2_cs_##name (GstVideoChromaResample *resample,       \
    gpointer lines[], gint width)                                       \
{                                                                       \
  /* FIXME */                                                           \
  if (resample->h_resample) {                                           \
    resample->h_resample (resample, lines[0], width);                   \
  }                                                                     \
}
/* 2x vertical downsampling interlaced with cositing
 *
 */
#define MAKE_DOWNSAMPLE_VI2_CS(name,type)                                    \
static void                                                             \
video_chroma_down_vi2_cs_##name (GstVideoChromaResample *resample,      \
    gpointer lines[], gint width)                                       \
{                                                                       \
  /* FIXME */                                                           \
  if (resample->h_resample) {                                           \
    resample->h_resample (resample, lines[0], width);                   \
  }                                                                     \
}

MAKE_UPSAMPLE_H2_CS (u16, guint16);
MAKE_UPSAMPLE_H2_CS (u8, guint8);
MAKE_UPSAMPLE_V2_CS (u16, guint16);
MAKE_UPSAMPLE_V2_CS (u8, guint8);
MAKE_UPSAMPLE_VI2_CS (u16, guint16);
MAKE_UPSAMPLE_VI2_CS (u8, guint8);
MAKE_DOWNSAMPLE_H2_CS (u16, guint16);
MAKE_DOWNSAMPLE_H2_CS (u8, guint8);
MAKE_DOWNSAMPLE_V2_CS (u16, guint16);
MAKE_DOWNSAMPLE_V2_CS (u8, guint8);
MAKE_DOWNSAMPLE_VI2_CS (u16, guint16);
MAKE_DOWNSAMPLE_VI2_CS (u8, guint8);

/* 4x horizontal upsampling with cositing
 *
 *   +---------- (3*a +   b + 2) >> 2
 * a | +-------- (  a +   b + 1) >> 1
 * | | | +------ (  a + 3*b + 2) >> 2
 * v v v v
 * O-O-O-O-O-O-O-O
 * x       x
 * a       b
 */
#define MAKE_UPSAMPLE_H4_CS(name,type)                                       \
static void                                                             \
video_chroma_up_h4_cs_##name (GstVideoChromaResample *resample,        \
    gpointer pixels, gint width)                        \
{                                                                       \
  type *p = pixels;                                                        \
  gint i;                                                               \
                                                                        \
  for (i = 0; i < width - 4; i += 4) {                                  \
    type tr0 = PR(i), tr1 = PR(i+4);                                    \
    type tb0 = PB(i), tb1 = PB(i+4);                                    \
                                                                        \
    PR(i+1) = FILT_3_1 (tr0, tr1);                                      \
    PB(i+1) = FILT_3_1 (tb0, tb1);                                      \
    PR(i+2) = FILT_1_1 (tr0, tr1);                                      \
    PB(i+2) = FILT_1_1 (tb0, tb1);                                      \
    PR(i+3) = FILT_1_3 (tr0, tr1);                                      \
    PB(i+3) = FILT_1_3 (tb0, tb1);                                      \
  }                                                                     \
}
/* 4x vertical upsampling with cositing
 *
 * a x O--O--O-  <---- a
 *     O--O--O-  <---- (3*a +   b + 2) >> 2
 *     O--O--O-  <---- (  a +   b + 1) >> 1
 *     O--O--O-  <---- (  a + 3*b + 2) >> 2
 * b x O--O--O-
 *     O--O--O-
 */
#define MAKE_UPSAMPLE_V4_CS(name,type)                                       \
static void                                                             \
video_chroma_up_v4_cs_##name (GstVideoChromaResample *resample,         \
    gpointer lines[], gint width)                                       \
{                                                                       \
  /* FIXME */                                                           \
  if (resample->h_resample) {                                           \
    resample->h_resample (resample, lines[0], width);                   \
  }                                                                     \
}
/* 4x vertical upsampling interlaced with cositing
 *
 */
#define MAKE_UPSAMPLE_VI4_CS(name,type)                                      \
static void                                                             \
video_chroma_up_vi4_cs_##name (GstVideoChromaResample *resample,        \
    gpointer lines[], gint width)                                       \
{                                                                       \
  /* FIXME */                                                           \
  if (resample->h_resample) {                                           \
    resample->h_resample (resample, lines[0], width);                   \
  }                                                                     \
}
/* 4x horizontal downsampling with cositing
 *
 * a
 * |       +------ (b + 2*c + 3*d + 4*e + 3*f + 2*g + h + 8) >> 16
 * v       v
 * O-------O-------
 * x x x x x x x x
 * a b c d e f g h
 */
#define MAKE_DOWNSAMPLE_H4_CS(name,type)                                     \
static void                                                             \
video_chroma_down_h4_cs_##name (GstVideoChromaResample *resample,      \
    gpointer pixels, gint width)                        \
{                                                                       \
  type *p = pixels;                                                     \
  gint i;                                                               \
                                                                        \
  if (width < 4)                                                        \
    return;                                                             \
                                                                        \
  PR(0) = FILT_10_3_2_1 (PR(0), PR(1), PR(2), PR(3));                   \
  PB(0) = FILT_10_3_2_1 (PB(0), PB(1), PB(2), PB(3));                   \
                                                                        \
  for (i = 4; i < width - 4; i += 4) {                                  \
    PR(i) = FILT_1_2_3_4_3_2_1 (PR(i-3), PR(i-2), PR(i-1), PR(i), PR(i+1), PR(i+2), PR(i+3));   \
    PB(i) = FILT_1_2_3_4_3_2_1 (PB(i-3), PB(i-2), PB(i-1), PB(i), PB(i+1), PB(i+2), PB(i+3));   \
  }                                                                     \
  if (i < width) {                                                      \
    PR(i) = FILT_1_2_3_10 (PR(i-3), PR(i-2), PR(i-1), PR(i));           \
    PB(i) = FILT_1_2_3_10 (PB(i-3), PB(i-2), PB(i-1), PB(i));           \
  }                                                                     \
}
/* 4x vertical downsampling with cositing
 *
 * a x O--O--O-  <---- a
 * b x --------
 * c x --------
 * d x --------
 * e x O--O--O-  <---- (b + 2*c + 3*d + 4*e + 3*f + 2*g + h + 8) >> 16
 * f x --------
 * g x --------
 * h x --------
 * i x O--O--O-
 * j x --------
 */
#define MAKE_DOWNSAMPLE_V4_CS(name,type)                                     \
static void                                                             \
video_chroma_down_v4_cs_##name (GstVideoChromaResample *resample,       \
    gpointer lines[], gint width)                                       \
{                                                                       \
  /* FIXME */                                                           \
  if (resample->h_resample) {                                           \
    resample->h_resample (resample, lines[0], width);                   \
  }                                                                     \
}
/* 4x vertical downsampling interlaced with cositing
 *
 */
#define MAKE_DOWNSAMPLE_VI4_CS(name,type)                                    \
static void                                                             \
video_chroma_down_vi4_cs_##name (GstVideoChromaResample *resample,      \
    gpointer lines[], gint width)                                       \
{                                                                       \
  /* FIXME */                                                           \
  if (resample->h_resample) {                                           \
    resample->h_resample (resample, lines[0], width);                   \
  }                                                                     \
}

MAKE_UPSAMPLE_H4_CS (u16, guint16);
MAKE_UPSAMPLE_H4_CS (u8, guint8);
MAKE_UPSAMPLE_V4_CS (u16, guint16);
MAKE_UPSAMPLE_V4_CS (u8, guint8);
MAKE_UPSAMPLE_VI4_CS (u16, guint16);
MAKE_UPSAMPLE_VI4_CS (u8, guint8);
MAKE_DOWNSAMPLE_H4_CS (u16, guint16);
MAKE_DOWNSAMPLE_H4_CS (u8, guint8);
MAKE_DOWNSAMPLE_V4_CS (u16, guint16);
MAKE_DOWNSAMPLE_V4_CS (u8, guint8);
MAKE_DOWNSAMPLE_VI4_CS (u16, guint16);
MAKE_DOWNSAMPLE_VI4_CS (u8, guint8);

typedef struct
{
  void (*resample) (GstVideoChromaResample * resample, gpointer pixels,
      gint width);
} HorizResampler;

static const HorizResampler h_resamplers[] = {
  {NULL},
  {video_chroma_up_h2_u8},
  {video_chroma_down_h2_u8},
  {video_chroma_up_h2_u16},
  {video_chroma_down_h2_u16},
  {video_chroma_up_h2_cs_u8},
  {video_chroma_down_h2_cs_u8},
  {video_chroma_up_h2_cs_u16},
  {video_chroma_down_h2_cs_u16},
  {video_chroma_up_h4_u8},
  {video_chroma_down_h4_u8},
  {video_chroma_up_h4_u16},
  {video_chroma_down_h4_u16},
  {video_chroma_up_h4_cs_u8},
  {video_chroma_down_h4_cs_u8},
  {video_chroma_up_h4_cs_u16},
  {video_chroma_down_h4_cs_u16}
};

typedef struct
{
  void (*resample) (GstVideoChromaResample * resample, gpointer lines[],
      gint width);
  guint n_lines;
  gint offset;
} VertResampler;

static void
video_chroma_none (GstVideoChromaResample * resample,
    gpointer lines[], gint width)
{
  if (resample->h_resample)
    resample->h_resample (resample, lines[0], width);
}

static const VertResampler v_resamplers[] = {
  {video_chroma_none, 1, 0},
  {video_chroma_up_v2_u8, 2, -1},
  {video_chroma_down_v2_u8, 2, 0},
  /* 16 bits */
  {video_chroma_up_v2_u16, 2, -1},
  {video_chroma_down_v2_u16, 2, 0},
  /* cosited */
  {video_chroma_up_v2_cs_u8, 1, 0},     /* IMPLEMENT ME */
  {video_chroma_down_v2_cs_u8, 1, 0},   /* IMPLEMENT ME */
  {video_chroma_up_v2_cs_u16, 1, 0},    /* IMPLEMENT ME */
  {video_chroma_down_v2_cs_u16, 1, 0},  /* IMPLEMENT ME */
  /* 4x */
  {video_chroma_up_v4_u8, 4, -2},
  {video_chroma_down_v4_u8, 4, 0},
  {video_chroma_up_v4_u16, 4, -2},
  {video_chroma_down_v4_u16, 4, 0},
  {video_chroma_up_v4_cs_u8, 1, 0},     /* IMPLEMENT ME */
  {video_chroma_down_v4_cs_u8, 1, 0},   /* IMPLEMENT ME */
  {video_chroma_up_v4_cs_u16, 1, 0},    /* IMPLEMENT ME */
  {video_chroma_down_v4_cs_u16, 1, 0},  /* IMPLEMENT ME */
  /* interlaced */
  {video_chroma_up_vi2_u8, 4, -2},
  {video_chroma_down_vi2_u8, 1, 0},     /* IMPLEMENT ME */
  {video_chroma_up_vi2_u16, 4, -2},
  {video_chroma_down_vi2_u16, 1, 0},    /* IMPLEMENT ME */
  {video_chroma_up_vi2_cs_u8, 1, 0},    /* IMPLEMENT ME */
  {video_chroma_down_vi2_cs_u8, 1, 0},  /* IMPLEMENT ME */
  {video_chroma_up_vi2_cs_u16, 1, 0},   /* IMPLEMENT ME */
  {video_chroma_down_vi2_cs_u16, 1, 0}, /* IMPLEMENT ME */
  {video_chroma_up_vi4_u8, 1, 0},       /* IMPLEMENT ME */
  {video_chroma_down_vi4_u8, 1, 0},     /* IMPLEMENT ME */
  {video_chroma_up_vi4_u16, 1, 0},      /* IMPLEMENT ME */
  {video_chroma_down_vi4_u16, 1, 0},    /* IMPLEMENT ME */
  {video_chroma_up_vi4_cs_u8, 1, 0},    /* IMPLEMENT ME */
  {video_chroma_down_vi4_cs_u8, 1, 0},  /* IMPLEMENT ME */
  {video_chroma_up_vi4_cs_u16, 1, 0},   /* IMPLEMENT ME */
  {video_chroma_down_vi4_cs_u16, 1, 0}, /* IMPLEMENT ME */
};

/**
 * gst_video_chroma_resample_new: (skip)
 * @method: a #GstVideoChromaMethod
 * @site: a #GstVideoChromaSite
 * @flags: #GstVideoChromaFlags
 * @format: the #GstVideoFormat
 * @h_factor: horizontal resampling factor
 * @v_factor: vertical resampling factor
 *
 * Create a new resampler object for the given parameters. When @h_factor or
 * @v_factor is > 0, upsampling will be used, otherwise subsampling is
 * performed.
 *
 * Returns: a new #GstVideoChromaResample that should be freed with
 *     gst_video_chroma_resample_free() after usage.
 */
GstVideoChromaResample *
gst_video_chroma_resample_new (GstVideoChromaMethod method,
    GstVideoChromaSite site, GstVideoChromaFlags flags,
    GstVideoFormat format, gint h_factor, gint v_factor)
{
  GstVideoChromaResample *result;
  guint cosite, h_index, v_index, bits;

  /* no resampling */
  if (h_factor == 0 && v_factor == 0)
    return NULL;

  if (format == GST_VIDEO_FORMAT_AYUV)
    bits = 8;
  else if (format == GST_VIDEO_FORMAT_AYUV64)
    bits = 16;
  else
    return NULL;

  cosite = (site & GST_VIDEO_CHROMA_SITE_H_COSITED ? 1 : 0);
  if (h_factor == 0)
    h_index = 0;
  else
    h_index =
        ((ABS (h_factor) - 1) * 8) + (cosite ? 4 : 0) + (bits ==
        16 ? 2 : 0) + (h_factor < 0 ? 1 : 0) + 1;

  GST_DEBUG ("h_resample %d, factor %d, cosite %d", h_index, h_factor, cosite);

  cosite = (site & GST_VIDEO_CHROMA_SITE_V_COSITED ? 1 : 0);
  if (v_factor == 0)
    v_index = 0;
  else
    v_index =
        ((ABS (v_factor) - 1) * 8) + (cosite ? 4 : 0) + (bits ==
        16 ? 2 : 0) + (v_factor < 0 ? 1 : 0) + 1;

  if (flags & GST_VIDEO_CHROMA_FLAG_INTERLACED)
    v_index += 16;

  GST_DEBUG ("v_resample %d, factor %d, cosite %d", v_index, v_factor, cosite);

  result = g_new (GstVideoChromaResample, 1);
  result->method = method;
  result->site = site;
  result->flags = flags;
  result->format = format;
  result->h_factor = h_factor;
  result->v_factor = v_factor;
  result->h_resample = h_resamplers[h_index].resample;
  result->v_resample = v_resamplers[v_index].resample;
  result->n_lines = v_resamplers[v_index].n_lines;
  result->offset = v_resamplers[v_index].offset;

  GST_DEBUG ("resample %p, bits %d, n_lines %u, offset %d", result, bits,
      result->n_lines, result->offset);

  return result;
}

/**
 * gst_video_chroma_resample_get_info:
 * @resample: a #GstVideoChromaResample
 * @n_lines: (out) (optional): the number of input lines
 * @offset: (out) (optional): the first line
 *
 * The resampler must be fed @n_lines at a time. The first line should be
 * at @offset.
 */
void
gst_video_chroma_resample_get_info (GstVideoChromaResample * resample,
    guint * n_lines, gint * offset)
{
  g_return_if_fail (resample != NULL);

  if (n_lines)
    *n_lines = resample->n_lines;
  if (offset)
    *offset = resample->offset;
}

/**
 * gst_video_chroma_resample_free:
 * @resample: a #GstVideoChromaResample
 *
 * Free @resample
 */
void
gst_video_chroma_resample_free (GstVideoChromaResample * resample)
{
  g_return_if_fail (resample != NULL);

  g_free (resample);
}

/**
 * gst_video_chroma_resample:
 * @resample: a #GstVideoChromaResample
 * @lines: pixel lines
 * @width: the number of pixels on one line
 *
 * Perform resampling of @width chroma pixels in @lines.
 */
void
gst_video_chroma_resample (GstVideoChromaResample * resample,
    gpointer lines[], gint width)
{
  g_return_if_fail (resample != NULL);

  resample->v_resample (resample, lines, width);
}
