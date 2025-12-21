/* GStreamer unit test for video
 *
 * Copyright (C) <2003> David A. Schleef <ds@schleef.org>
 * Copyright (C) <2006> Jan Schmidt <thaytan@mad.scientist.com>
 * Copyright (C) <2008,2011> Tim-Philipp Müller <tim centricular net>
 * Copyright (C) <2012> Collabora Ltd. <tim.muller@collabora.co.uk>
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

#ifdef HAVE_VALGRIND
# include <valgrind/valgrind.h>
#endif

#include <gst/check/gstcheck.h>

#include <gst/video/video.h>
#include <gst/video/gstvideometa.h>
#include <gst/video/video-overlay-composition.h>
#include <string.h>

/* These are from the current/old videotestsrc; we check our new public API
 * in libgstvideo against the old one to make sure the sizes and offsets
 * end up the same */

typedef struct paintinfo_struct paintinfo;
struct paintinfo_struct
{
  unsigned char *dest;          /* pointer to first byte of video data */
  unsigned char *yp, *up, *vp;  /* pointers to first byte of each component
                                 * for both packed/planar YUV and RGB */
  unsigned char *ap;            /* pointer to first byte of alpha component */
  unsigned char *endptr;        /* pointer to byte beyond last video data */
  int ystride;
  int ustride;
  int vstride;
  int width;
  int height;
};

struct fourcc_list_struct
{
  const char *fourcc;
  const char *name;
  int bitspp;
  void (*paint_setup) (paintinfo * p, unsigned char *dest);
};

static void paint_setup_I420 (paintinfo * p, unsigned char *dest);
static void paint_setup_YV12 (paintinfo * p, unsigned char *dest);
static void paint_setup_YUY2 (paintinfo * p, unsigned char *dest);
static void paint_setup_UYVY (paintinfo * p, unsigned char *dest);
static void paint_setup_YVYU (paintinfo * p, unsigned char *dest);
static void paint_setup_IYU2 (paintinfo * p, unsigned char *dest);
static void paint_setup_Y41B (paintinfo * p, unsigned char *dest);
static void paint_setup_Y42B (paintinfo * p, unsigned char *dest);
static void paint_setup_GRAY8 (paintinfo * p, unsigned char *dest);
static void paint_setup_AYUV (paintinfo * p, unsigned char *dest);

#if 0
static void paint_setup_IMC1 (paintinfo * p, unsigned char *dest);
static void paint_setup_IMC2 (paintinfo * p, unsigned char *dest);
static void paint_setup_IMC3 (paintinfo * p, unsigned char *dest);
static void paint_setup_IMC4 (paintinfo * p, unsigned char *dest);
#endif
static void paint_setup_YUV9 (paintinfo * p, unsigned char *dest);
static void paint_setup_YVU9 (paintinfo * p, unsigned char *dest);

int fourcc_get_size (struct fourcc_list_struct *fourcc, int w, int h);

struct fourcc_list_struct fourcc_list[] = {
/* packed */
  {"YUY2", "YUY2", 16, paint_setup_YUY2},
  {"UYVY", "UYVY", 16, paint_setup_UYVY},
  {"Y422", "Y422", 16, paint_setup_UYVY},
  {"UYNV", "UYNV", 16, paint_setup_UYVY},       /* FIXME: UYNV? */
  {"YVYU", "YVYU", 16, paint_setup_YVYU},
  {"AYUV", "AYUV", 32, paint_setup_AYUV},

  /* interlaced */
  /*{   "IUYV", "IUY2", 16, paint_setup_YVYU }, */

  /* inverted */
  /*{   "cyuv", "cyuv", 16, paint_setup_YVYU }, */

  /*{   "Y41P", "Y41P", 12, paint_setup_YVYU }, */

  /* interlaced */
  /*{   "IY41", "IY41", 12, paint_setup_YVYU }, */

  /*{   "Y211", "Y211", 8, paint_setup_YVYU }, */

  /*{   "Y41T", "Y41T", 12, paint_setup_YVYU }, */
  /*{   "Y42P", "Y42P", 16, paint_setup_YVYU }, */
  /*{   "CLJR", "CLJR", 8, paint_setup_YVYU }, */
  /*{   "IYU1", "IYU1", 12, paint_setup_YVYU }, */
  {"IYU2", "IYU2", 24, paint_setup_IYU2},

/* planar */
  /* YVU9 */
  {"YVU9", "YVU9", 9, paint_setup_YVU9},
  /* YUV9 */
  {"YUV9", "YUV9", 9, paint_setup_YUV9},
  /* IF09 */
  /* YV12 */
  {"YV12", "YV12", 12, paint_setup_YV12},
  /* I420 */
  {"I420", "I420", 12, paint_setup_I420},
  /* NV12 */
  /* NV21 */
#if 0
  /* IMC1 */
  {"IMC1", "IMC1", 16, paint_setup_IMC1},
  /* IMC2 */
  {"IMC2", "IMC2", 12, paint_setup_IMC2},
  /* IMC3 */
  {"IMC3", "IMC3", 16, paint_setup_IMC3},
  /* IMC4 */
  {"IMC4", "IMC4", 12, paint_setup_IMC4},
#endif
  /* CLPL */
  /* Y41B */
  {"Y41B", "Y41B", 12, paint_setup_Y41B},
  /* Y42B */
  {"Y42B", "Y42B", 16, paint_setup_Y42B},
  /* GRAY8 grayscale */
  {"GRAY8", "GRAY8", 8, paint_setup_GRAY8}
};

/* returns the size in bytes for one video frame of the given dimensions
 * given the fourcc */
int
fourcc_get_size (struct fourcc_list_struct *fourcc, int w, int h)
{
  paintinfo pi = { NULL, };
  paintinfo *p = &pi;

  p->width = w;
  p->height = h;

  fourcc->paint_setup (p, NULL);

  return GPOINTER_TO_INT (p->endptr);
}

static void
paint_setup_I420 (paintinfo * p, unsigned char *dest)
{
  p->yp = dest;
  p->ystride = GST_ROUND_UP_4 (p->width);
  p->up = p->yp + p->ystride * GST_ROUND_UP_2 (p->height);
  p->ustride = GST_ROUND_UP_8 (p->width) / 2;
  p->vp = p->up + p->ustride * GST_ROUND_UP_2 (p->height) / 2;
  p->vstride = GST_ROUND_UP_8 (p->ystride) / 2;
  p->endptr = p->vp + p->vstride * GST_ROUND_UP_2 (p->height) / 2;
}

static void
paint_setup_YV12 (paintinfo * p, unsigned char *dest)
{
  p->yp = dest;
  p->ystride = GST_ROUND_UP_4 (p->width);
  p->vp = p->yp + p->ystride * GST_ROUND_UP_2 (p->height);
  p->vstride = GST_ROUND_UP_8 (p->ystride) / 2;
  p->up = p->vp + p->vstride * GST_ROUND_UP_2 (p->height) / 2;
  p->ustride = GST_ROUND_UP_8 (p->ystride) / 2;
  p->endptr = p->up + p->ustride * GST_ROUND_UP_2 (p->height) / 2;
}

static void
paint_setup_AYUV (paintinfo * p, unsigned char *dest)
{
  p->ap = dest;
  p->yp = dest + 1;
  p->up = dest + 2;
  p->vp = dest + 3;
  p->ystride = p->width * 4;
  p->endptr = dest + p->ystride * p->height;
}

static void
paint_setup_YUY2 (paintinfo * p, unsigned char *dest)
{
  p->yp = dest;
  p->up = dest + 1;
  p->vp = dest + 3;
  p->ystride = GST_ROUND_UP_2 (p->width) * 2;
  p->endptr = dest + p->ystride * p->height;
}

static void
paint_setup_UYVY (paintinfo * p, unsigned char *dest)
{
  p->yp = dest + 1;
  p->up = dest;
  p->vp = dest + 2;
  p->ystride = GST_ROUND_UP_2 (p->width) * 2;
  p->endptr = dest + p->ystride * p->height;
}

static void
paint_setup_YVYU (paintinfo * p, unsigned char *dest)
{
  p->yp = dest;
  p->up = dest + 3;
  p->vp = dest + 1;
  p->ystride = GST_ROUND_UP_2 (p->width) * 2;
  p->endptr = dest + p->ystride * p->height;
}

static void
paint_setup_IYU2 (paintinfo * p, unsigned char *dest)
{
  /* untested */
  p->yp = dest + 1;
  p->up = dest + 0;
  p->vp = dest + 2;
  p->ystride = GST_ROUND_UP_4 (p->width * 3);
  p->endptr = dest + p->ystride * p->height;
}

static void
paint_setup_Y41B (paintinfo * p, unsigned char *dest)
{
  p->yp = dest;
  p->ystride = GST_ROUND_UP_4 (p->width);
  p->up = p->yp + p->ystride * p->height;
  p->ustride = GST_ROUND_UP_16 (p->width) / 4;
  p->vp = p->up + p->ustride * p->height;
  p->vstride = GST_ROUND_UP_16 (p->width) / 4;
  p->endptr = p->vp + p->vstride * p->height;
}

static void
paint_setup_Y42B (paintinfo * p, unsigned char *dest)
{
  p->yp = dest;
  p->ystride = GST_ROUND_UP_4 (p->width);
  p->up = p->yp + p->ystride * p->height;
  p->ustride = GST_ROUND_UP_8 (p->width) / 2;
  p->vp = p->up + p->ustride * p->height;
  p->vstride = GST_ROUND_UP_8 (p->width) / 2;
  p->endptr = p->vp + p->vstride * p->height;
}

static void
paint_setup_GRAY8 (paintinfo * p, unsigned char *dest)
{
  /* untested */
  p->yp = dest;
  p->ystride = GST_ROUND_UP_4 (p->width);
  p->endptr = dest + p->ystride * p->height;
}

#if 0
static void
paint_setup_IMC1 (paintinfo * p, unsigned char *dest)
{
  p->yp = dest;
  p->up = dest + p->width * p->height;
  p->vp = dest + p->width * p->height + p->width * p->height / 2;
}

static void
paint_setup_IMC2 (paintinfo * p, unsigned char *dest)
{
  p->yp = dest;
  p->vp = dest + p->width * p->height;
  p->up = dest + p->width * p->height + p->width / 2;
}

static void
paint_setup_IMC3 (paintinfo * p, unsigned char *dest)
{
  p->yp = dest;
  p->up = dest + p->width * p->height + p->width * p->height / 2;
  p->vp = dest + p->width * p->height;
}

static void
paint_setup_IMC4 (paintinfo * p, unsigned char *dest)
{
  p->yp = dest;
  p->vp = dest + p->width * p->height + p->width / 2;
  p->up = dest + p->width * p->height;
}
#endif

static void
paint_setup_YVU9 (paintinfo * p, unsigned char *dest)
{
  p->yp = dest;
  p->ystride = GST_ROUND_UP_4 (p->width);
  p->vp = p->yp + p->ystride * p->height;
  p->vstride = GST_ROUND_UP_4 (p->ystride / 4);
  p->up = p->vp + p->vstride * (GST_ROUND_UP_4 (p->height) / 4);
  p->ustride = GST_ROUND_UP_4 (p->ystride / 4);
  p->endptr = p->up + p->ustride * (GST_ROUND_UP_4 (p->height) / 4);
}

static void
paint_setup_YUV9 (paintinfo * p, unsigned char *dest)
{
  p->yp = dest;
  p->ystride = GST_ROUND_UP_4 (p->width);
  p->up = p->yp + p->ystride * p->height;
  p->ustride = GST_ROUND_UP_4 (p->ystride / 4);
  p->vp = p->up + p->ustride * (GST_ROUND_UP_4 (p->height) / 4);
  p->vstride = GST_ROUND_UP_4 (p->ystride / 4);
  p->endptr = p->vp + p->vstride * (GST_ROUND_UP_4 (p->height) / 4);
}

#define gst_video_format_is_packed video_format_is_packed
static gboolean
video_format_is_packed (GstVideoFormat fmt)
{
  switch (fmt) {
    case GST_VIDEO_FORMAT_I420:
    case GST_VIDEO_FORMAT_YV12:
    case GST_VIDEO_FORMAT_Y41B:
    case GST_VIDEO_FORMAT_Y42B:
    case GST_VIDEO_FORMAT_GRAY8:
    case GST_VIDEO_FORMAT_YUV9:
    case GST_VIDEO_FORMAT_YVU9:
      return FALSE;
    case GST_VIDEO_FORMAT_IYU1:
    case GST_VIDEO_FORMAT_IYU2:
    case GST_VIDEO_FORMAT_YUY2:
    case GST_VIDEO_FORMAT_YVYU:
    case GST_VIDEO_FORMAT_UYVY:
    case GST_VIDEO_FORMAT_VYUY:
    case GST_VIDEO_FORMAT_AYUV:
    case GST_VIDEO_FORMAT_RGBx:
    case GST_VIDEO_FORMAT_BGRx:
    case GST_VIDEO_FORMAT_xRGB:
    case GST_VIDEO_FORMAT_xBGR:
    case GST_VIDEO_FORMAT_RGBA:
    case GST_VIDEO_FORMAT_BGRA:
    case GST_VIDEO_FORMAT_ARGB:
    case GST_VIDEO_FORMAT_ABGR:
    case GST_VIDEO_FORMAT_RGB:
    case GST_VIDEO_FORMAT_BGR:
    case GST_VIDEO_FORMAT_RGB8P:
      return TRUE;
    default:
      g_return_val_if_reached (FALSE);
  }
  return FALSE;
}

static gint
get_num_formats (void)
{
  gint num_formats = 200;
  fail_unless (gst_video_format_to_string (num_formats) == NULL);
  while (gst_video_format_to_string (num_formats) == NULL)
    --num_formats;
  GST_INFO ("number of known video formats: %d", num_formats);
  return num_formats + 1;
}

GST_START_TEST (test_video_formats_all)
{
  GstStructure *s;
  const GValue *val, *list_val;
  GstCaps *caps;
  guint num, n, num_formats;

  num_formats = get_num_formats ();

  caps = gst_caps_from_string ("video/x-raw, format=" GST_VIDEO_FORMATS_ALL);
  s = gst_caps_get_structure (caps, 0);
  val = gst_structure_get_value (s, "format");
  fail_unless (val != NULL);
  fail_unless (GST_VALUE_HOLDS_LIST (val));
  num = gst_value_list_get_size (val);
  fail_unless (num > 0);
  for (n = 0; n < num; ++n) {
    const gchar *fmt_str;

    list_val = gst_value_list_get_value (val, n);
    fail_unless (G_VALUE_HOLDS_STRING (list_val));
    fmt_str = g_value_get_string (list_val);
    GST_INFO ("format: %s", fmt_str);
    fail_if (gst_video_format_from_string (fmt_str) ==
        GST_VIDEO_FORMAT_UNKNOWN);
  }
  /* Take into account GST_VIDEO_FORMAT_ENCODED, UNKNOWN and DMA_DRM. */
  fail_unless_equals_int (num, num_formats - 3);

  gst_caps_unref (caps);
}

GST_END_TEST;

#define WIDTH 77
#define HEIGHT 20
GST_START_TEST (test_video_formats_pack_unpack)
{
  guint n, num_formats;

  num_formats = get_num_formats ();

  for (n = GST_VIDEO_FORMAT_ENCODED + 1; n < num_formats; ++n) {
    const GstVideoFormatInfo *vfinfo, *unpackinfo;
    GstVideoFormat fmt = n;
    GstVideoInfo vinfo;
    gpointer data[GST_VIDEO_MAX_PLANES];
    gint stride[GST_VIDEO_MAX_PLANES];
    guint8 *vdata, *unpack_data;
    gsize vsize, unpack_size;
    guint p;

    if (n == GST_VIDEO_FORMAT_DMA_DRM)
      continue;

    GST_INFO ("testing %s", gst_video_format_to_string (fmt));

    vfinfo = gst_video_format_get_info (fmt);
    fail_unless (vfinfo != NULL);

    unpackinfo = gst_video_format_get_info (vfinfo->unpack_format);
    fail_unless (unpackinfo != NULL);

    gst_video_info_init (&vinfo);
    fail_unless (gst_video_info_set_format (&vinfo, fmt, WIDTH, HEIGHT));
    vsize = GST_VIDEO_INFO_SIZE (&vinfo);
    vdata = g_malloc (vsize);
    memset (vdata, 0x99, vsize);

    g_assert (vfinfo->pack_lines == 1);

    unpack_size =
        GST_VIDEO_FORMAT_INFO_BITS (unpackinfo) *
        GST_VIDEO_FORMAT_INFO_N_COMPONENTS (unpackinfo) *
        GST_ROUND_UP_16 (WIDTH);
    unpack_data = g_malloc (unpack_size);

    for (p = 0; p < GST_VIDEO_INFO_N_PLANES (&vinfo); ++p) {
      data[p] = vdata + GST_VIDEO_INFO_PLANE_OFFSET (&vinfo, p);
      stride[p] = GST_VIDEO_INFO_PLANE_STRIDE (&vinfo, p);
    }

    /* now unpack */
    vfinfo->unpack_func (vfinfo, GST_VIDEO_PACK_FLAG_NONE, unpack_data, data,
        stride, 0, 0, WIDTH);

    /* and pack */
    vfinfo->pack_func (vfinfo, GST_VIDEO_PACK_FLAG_NONE, unpack_data,
        unpack_size, data, stride, GST_VIDEO_CHROMA_SITE_UNKNOWN, 0, WIDTH);

    /* now unpack */
    vfinfo->unpack_func (vfinfo, GST_VIDEO_PACK_FLAG_NONE, unpack_data, data,
        stride, 0, HEIGHT - 1, WIDTH);

    /* and pack */
    vfinfo->pack_func (vfinfo, GST_VIDEO_PACK_FLAG_NONE, unpack_data,
        unpack_size, data, stride, GST_VIDEO_CHROMA_SITE_UNKNOWN, HEIGHT - 1,
        WIDTH);

    g_free (unpack_data);
    g_free (vdata);
  }
}

GST_END_TEST;
#undef WIDTH
#undef HEIGHT

GST_START_TEST (test_video_formats)
{
  guint i;

  for (i = 0; i < G_N_ELEMENTS (fourcc_list); ++i) {
    const GstVideoFormatInfo *vf_info;
    GstVideoFormat fmt;
    const gchar *s;
    guint32 fourcc;
    guint w, h;

    s = fourcc_list[i].fourcc;
    fourcc = GST_MAKE_FOURCC (s[0], s[1], s[2], s[3]);
    fmt = gst_video_format_from_fourcc (fourcc);

    if (fmt == GST_VIDEO_FORMAT_UNKNOWN) {
      GST_DEBUG ("Unknown format %s, skipping tests", fourcc_list[i].fourcc);
      continue;
    }

    vf_info = gst_video_format_get_info (fmt);
    fail_unless (vf_info != NULL);

    fail_unless_equals_int (GST_VIDEO_FORMAT_INFO_FORMAT (vf_info), fmt);

    GST_INFO ("Fourcc %s, packed=%d", fourcc_list[i].fourcc,
        gst_video_format_is_packed (fmt));

    fail_unless (GST_VIDEO_FORMAT_INFO_IS_YUV (vf_info));

    /* use any non-NULL pointer so we can compare against NULL */
    {
      paintinfo paintinfo = { 0, };
      fourcc_list[i].paint_setup (&paintinfo, (unsigned char *) s);
      if (paintinfo.ap != NULL) {
        fail_unless (GST_VIDEO_FORMAT_INFO_HAS_ALPHA (vf_info));
      } else {
        fail_if (GST_VIDEO_FORMAT_INFO_HAS_ALPHA (vf_info));
      }
    }

    for (w = 1; w <= 65; ++w) {
      for (h = 1; h <= 65; ++h) {
        GstVideoInfo vinfo;
        paintinfo paintinfo = { 0, };
        guint off0, off1, off2, off3;
        guint cs0, cs1, cs2, cs3;
        guint size;

        GST_LOG ("%s, %dx%d", fourcc_list[i].fourcc, w, h);

        gst_video_info_init (&vinfo);
        fail_unless (gst_video_info_set_format (&vinfo, fmt, w, h));

        paintinfo.width = w;
        paintinfo.height = h;
        fourcc_list[i].paint_setup (&paintinfo, NULL);
        fail_unless_equals_int (GST_VIDEO_INFO_COMP_STRIDE (&vinfo, 0),
            paintinfo.ystride);
        if (!gst_video_format_is_packed (fmt)
            && GST_VIDEO_INFO_N_PLANES (&vinfo) <= 2) {
          /* planar */
          fail_unless_equals_int (GST_VIDEO_INFO_COMP_STRIDE (&vinfo, 1),
              paintinfo.ustride);
          fail_unless_equals_int (GST_VIDEO_INFO_COMP_STRIDE (&vinfo, 2),
              paintinfo.vstride);
          /* check component_width * height against offsets/size somehow? */
        }

        size = GST_VIDEO_INFO_SIZE (&vinfo);
        off0 = GST_VIDEO_INFO_COMP_OFFSET (&vinfo, 0);
        off1 = GST_VIDEO_INFO_COMP_OFFSET (&vinfo, 1);
        off2 = GST_VIDEO_INFO_COMP_OFFSET (&vinfo, 2);

        GST_TRACE ("size %d <> %d", size, GPOINTER_TO_INT (paintinfo.endptr));
        GST_TRACE ("off0 %d <> %d", off0, GPOINTER_TO_INT (paintinfo.yp));
        GST_TRACE ("off1 %d <> %d", off1, GPOINTER_TO_INT (paintinfo.up));
        GST_TRACE ("off2 %d <> %d", off2, GPOINTER_TO_INT (paintinfo.vp));

        fail_unless_equals_int (size, GPOINTER_TO_INT (paintinfo.endptr));
        fail_unless_equals_int (off0, GPOINTER_TO_INT (paintinfo.yp));
        fail_unless_equals_int (off1, GPOINTER_TO_INT (paintinfo.up));
        fail_unless_equals_int (off2, GPOINTER_TO_INT (paintinfo.vp));

        /* should be 0 if there's no alpha component */
        off3 = GST_VIDEO_INFO_COMP_OFFSET (&vinfo, 3);
        fail_unless_equals_int (off3, GPOINTER_TO_INT (paintinfo.ap));

        cs0 = GST_VIDEO_INFO_COMP_WIDTH (&vinfo, 0) *
            GST_VIDEO_INFO_COMP_HEIGHT (&vinfo, 0);
        cs1 = GST_VIDEO_INFO_COMP_WIDTH (&vinfo, 1) *
            GST_VIDEO_INFO_COMP_HEIGHT (&vinfo, 1);
        cs2 = GST_VIDEO_INFO_COMP_WIDTH (&vinfo, 2) *
            GST_VIDEO_INFO_COMP_HEIGHT (&vinfo, 2);

        /* GST_LOG ("cs0=%d,cs1=%d,cs2=%d,off0=%d,off1=%d,off2=%d,size=%d",
           cs0, cs1, cs2, off0, off1, off2, size); */

        if (!gst_video_format_is_packed (fmt))
          fail_unless (cs0 <= off1);

        if (GST_VIDEO_FORMAT_INFO_HAS_ALPHA (vinfo.finfo)) {
          cs3 = GST_VIDEO_INFO_COMP_WIDTH (&vinfo, 3) *
              GST_VIDEO_INFO_COMP_HEIGHT (&vinfo, 2);
          fail_unless (cs3 < size);
          /* U/V/alpha shouldn't take up more space than the Y component */
          fail_if (cs1 > cs0, "cs1 (%d) should be <= cs0 (%d)", cs1, cs0);
          fail_if (cs2 > cs0, "cs2 (%d) should be <= cs0 (%d)", cs2, cs0);
          fail_if (cs3 > cs0, "cs3 (%d) should be <= cs0 (%d)", cs3, cs0);

          /* all components together shouldn't take up more space than size */
          fail_unless (cs0 + cs1 + cs2 + cs3 <= size);
        } else {
          /* U/V shouldn't take up more space than the Y component */
          fail_if (cs1 > cs0, "cs1 (%d) should be <= cs0 (%d)", cs1, cs0);
          fail_if (cs2 > cs0, "cs2 (%d) should be <= cs0 (%d)", cs2, cs0);

          /* all components together shouldn't take up more space than size */
          fail_unless (cs0 + cs1 + cs2 <= size,
              "cs0 (%d) + cs1 (%d) + cs2 (%d) should be <= size (%d)",
              cs0, cs1, cs2, size);
        }
      }
    }
  }
}

GST_END_TEST;

GST_START_TEST (test_video_formats_overflow)
{
  GstVideoInfo vinfo;

  gst_video_info_init (&vinfo);

  fail_unless (gst_video_info_set_format (&vinfo, GST_VIDEO_FORMAT_ARGB, 32768,
          32767));
  /* fails due to simplification: we forbid some things that would in theory be fine.
   * We assume a 128 byte alignment for the width currently
   * fail_unless (gst_video_info_set_format (&vinfo, GST_VIDEO_FORMAT_ARGB, 32767, 32768));
   */
  fail_if (gst_video_info_set_format (&vinfo, GST_VIDEO_FORMAT_ARGB, 32768,
          32768));

  fail_if (gst_video_info_set_format (&vinfo, GST_VIDEO_FORMAT_ARGB,
          G_MAXINT / 2, G_MAXINT));
  fail_if (gst_video_info_set_format (&vinfo, GST_VIDEO_FORMAT_ARGB, G_MAXINT,
          G_MAXINT / 2));
  fail_if (gst_video_info_set_format (&vinfo, GST_VIDEO_FORMAT_ARGB,
          G_MAXINT / 2, G_MAXINT / 2));
  fail_if (gst_video_info_set_format (&vinfo, GST_VIDEO_FORMAT_ARGB, G_MAXINT,
          G_MAXINT));
  fail_if (gst_video_info_set_format (&vinfo, GST_VIDEO_FORMAT_ARGB,
          G_MAXUINT / 2, G_MAXUINT));
  fail_if (gst_video_info_set_format (&vinfo, GST_VIDEO_FORMAT_ARGB, G_MAXUINT,
          G_MAXUINT / 2));
  fail_if (gst_video_info_set_format (&vinfo, GST_VIDEO_FORMAT_ARGB,
          G_MAXUINT / 2, G_MAXUINT / 2));
  fail_if (gst_video_info_set_format (&vinfo, GST_VIDEO_FORMAT_ARGB, G_MAXUINT,
          G_MAXUINT));

  fail_unless (gst_video_info_set_format (&vinfo, GST_VIDEO_FORMAT_ARGB,
          1073741824 - 128, 1));
  fail_if (gst_video_info_set_format (&vinfo, GST_VIDEO_FORMAT_ARGB, 1073741824,
          1));

}

GST_END_TEST;

GST_START_TEST (test_video_formats_rgb)
{
  GstVideoInfo vinfo;
  gint width, height, framerate_n, framerate_d, par_n, par_d;
  GstCaps *caps;
  GstStructure *structure;

  gst_video_info_init (&vinfo);
  fail_unless (gst_video_info_set_format (&vinfo, GST_VIDEO_FORMAT_RGB, 800,
          600));
  vinfo.par_n = 1;
  vinfo.par_d = 1;
  vinfo.fps_n = 0;
  vinfo.fps_d = 1;
  caps = gst_video_info_to_caps (&vinfo);
  structure = gst_caps_get_structure (caps, 0);

  fail_unless (gst_structure_get_int (structure, "width", &width));
  fail_unless (gst_structure_get_int (structure, "height", &height));
  fail_unless (gst_structure_get_fraction (structure, "framerate", &framerate_n,
          &framerate_d));
  fail_unless (gst_structure_get_fraction (structure, "pixel-aspect-ratio",
          &par_n, &par_d));

  fail_unless (width == 800);
  fail_unless (height == 600);
  fail_unless (framerate_n == 0);
  fail_unless (framerate_d == 1);
  fail_unless (par_n == 1);
  fail_unless (par_d == 1);

  gst_caps_unref (caps);
}

GST_END_TEST;


GST_START_TEST (test_video_formats_rgba_large_dimension)
{
  GstVideoInfo vinfo;
  gint width, height, framerate_n, framerate_d, par_n, par_d;
  GstCaps *caps;
  GstStructure *structure;

  gst_video_info_init (&vinfo);
  fail_unless (gst_video_info_set_format (&vinfo, GST_VIDEO_FORMAT_RGBA, 29700,
          21000));
  vinfo.par_n = 1;
  vinfo.par_d = 1;
  vinfo.fps_n = 0;
  vinfo.fps_d = 1;
  caps = gst_video_info_to_caps (&vinfo);
  structure = gst_caps_get_structure (caps, 0);

  fail_unless (gst_structure_get_int (structure, "width", &width));
  fail_unless (gst_structure_get_int (structure, "height", &height));
  fail_unless (gst_structure_get_fraction (structure, "framerate", &framerate_n,
          &framerate_d));
  fail_unless (gst_structure_get_fraction (structure, "pixel-aspect-ratio",
          &par_n, &par_d));

  fail_unless (width == 29700);
  fail_unless (height == 21000);
  fail_unless (framerate_n == 0);
  fail_unless (framerate_d == 1);
  fail_unless (par_n == 1);
  fail_unless (par_d == 1);
  fail_unless (vinfo.size == (gsize) 29700 * 21000 * 4);

  gst_caps_unref (caps);
}

GST_END_TEST;

GST_START_TEST (test_guess_framerate)
{
  /* Check some obvious exact framerates */
  gint fps_n, fps_d;
  fail_unless (gst_video_guess_framerate (GST_SECOND / 24, &fps_n, &fps_d));
  fail_unless (fps_n == 24 && fps_d == 1);

  fail_unless (gst_video_guess_framerate (GST_SECOND / 30, &fps_n, &fps_d));
  fail_unless (fps_n == 30 && fps_d == 1);

  fail_unless (gst_video_guess_framerate (GST_SECOND / 25, &fps_n, &fps_d));
  fail_unless (fps_n == 25 && fps_d == 1);

  /* Some NTSC rates: */
  fail_unless (gst_video_guess_framerate (GST_SECOND * 1001 / 30000, &fps_n,
          &fps_d));
  fail_unless (fps_n == 30000 && fps_d == 1001);

  fail_unless (gst_video_guess_framerate (GST_SECOND * 1001 / 24000, &fps_n,
          &fps_d));
  fail_unless (fps_n == 24000 && fps_d == 1001);

  fail_unless (gst_video_guess_framerate (GST_SECOND * 1001 / 60000, &fps_n,
          &fps_d));
  fail_unless (fps_n == 60000 && fps_d == 1001);

  /* Check some high FPS, low durations */
  fail_unless (gst_video_guess_framerate (GST_SECOND / 9000, &fps_n, &fps_d));
  fail_unless (fps_n == 9000 && fps_d == 1);
  fail_unless (gst_video_guess_framerate (GST_SECOND / 10000, &fps_n, &fps_d));
  fail_unless (fps_n == 10000 && fps_d == 1);
  fail_unless (gst_video_guess_framerate (GST_SECOND / 11000, &fps_n, &fps_d));
  fail_unless (fps_n == 11000 && fps_d == 1);
  fail_unless (gst_video_guess_framerate (GST_SECOND / 20000, &fps_n, &fps_d));
  fail_unless (fps_n == 20000 && fps_d == 1);
  fail_unless (gst_video_guess_framerate (GST_SECOND / 100000, &fps_n, &fps_d));
  fail_unless (fps_n == 100000 && fps_d == 1);
}

GST_END_TEST;

GST_START_TEST (test_dar_calc)
{
  guint display_ratio_n, display_ratio_d;

  /* Ensure that various Display Ratio calculations are correctly done */
  /* video 768x576, par 16/15, display par 16/15 = 4/3 */
  fail_unless (gst_video_calculate_display_ratio (&display_ratio_n,
          &display_ratio_d, 768, 576, 16, 15, 16, 15));
  fail_unless (display_ratio_n == 4 && display_ratio_d == 3);

  /* video 720x480, par 32/27, display par 1/1 = 16/9 */
  fail_unless (gst_video_calculate_display_ratio (&display_ratio_n,
          &display_ratio_d, 720, 480, 32, 27, 1, 1));
  fail_unless (display_ratio_n == 16 && display_ratio_d == 9);

  /* video 360x288, par 533333/500000, display par 16/15 =
   * dar 1599999/1600000 */
  fail_unless (gst_video_calculate_display_ratio (&display_ratio_n,
          &display_ratio_d, 360, 288, 533333, 500000, 16, 15));
  fail_unless (display_ratio_n == 1599999 && display_ratio_d == 1280000);
}

GST_END_TEST;

GST_START_TEST (test_parse_caps_rgb)
{
  /* *INDENT-OFF* */
  struct
  {
    const gchar *tmpl_caps_string;
    GstVideoFormat fmt;
  } formats[] = {
    /* 24 bit */
    {
    GST_VIDEO_CAPS_MAKE ("RGB"), GST_VIDEO_FORMAT_RGB}, {
    GST_VIDEO_CAPS_MAKE ("BGR"), GST_VIDEO_FORMAT_BGR},
        /* 32 bit (no alpha) */
    {
    GST_VIDEO_CAPS_MAKE ("RGBx"), GST_VIDEO_FORMAT_RGBx}, {
    GST_VIDEO_CAPS_MAKE ("xRGB"), GST_VIDEO_FORMAT_xRGB}, {
    GST_VIDEO_CAPS_MAKE ("BGRx"), GST_VIDEO_FORMAT_BGRx}, {
    GST_VIDEO_CAPS_MAKE ("xBGR"), GST_VIDEO_FORMAT_xBGR},
        /* 32 bit (with alpha) */
    {
    GST_VIDEO_CAPS_MAKE ("RGBA"), GST_VIDEO_FORMAT_RGBA}, {
    GST_VIDEO_CAPS_MAKE ("ARGB"), GST_VIDEO_FORMAT_ARGB}, {
    GST_VIDEO_CAPS_MAKE ("BGRA"), GST_VIDEO_FORMAT_BGRA}, {
    GST_VIDEO_CAPS_MAKE ("ABGR"), GST_VIDEO_FORMAT_ABGR},
        /* 16 bit */
    {
    GST_VIDEO_CAPS_MAKE ("RGB16"), GST_VIDEO_FORMAT_RGB16}, {
    GST_VIDEO_CAPS_MAKE ("BGR16"), GST_VIDEO_FORMAT_BGR16}, {
    GST_VIDEO_CAPS_MAKE ("RGB15"), GST_VIDEO_FORMAT_RGB15}, {
    GST_VIDEO_CAPS_MAKE ("BGR15"), GST_VIDEO_FORMAT_BGR15}
  };
  /* *INDENT-ON* */
  gint i;

  for (i = 0; i < G_N_ELEMENTS (formats); ++i) {
    GstVideoInfo vinfo;
    GstCaps *caps, *caps2;

    caps = gst_caps_from_string (formats[i].tmpl_caps_string);
    fail_unless (caps != NULL);
    gst_caps_set_simple (caps, "width", G_TYPE_INT, 2 * (i + 1), "height",
        G_TYPE_INT, i + 1, "framerate", GST_TYPE_FRACTION, 15, 1,
        "pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1,
        "interlace-mode", G_TYPE_STRING, "progressive",
        "colorimetry", G_TYPE_STRING, "1:1:0:0",
        "multiview-mode", G_TYPE_STRING, "mono",
        "multiview-flags", GST_TYPE_VIDEO_MULTIVIEW_FLAGSET, 0,
        GST_FLAG_SET_MASK_EXACT, NULL);
    g_assert (gst_caps_is_fixed (caps));

    GST_DEBUG ("testing caps: %" GST_PTR_FORMAT, caps);

    gst_video_info_init (&vinfo);
    fail_unless (gst_video_info_from_caps (&vinfo, caps));
    fail_unless_equals_int (GST_VIDEO_INFO_FORMAT (&vinfo), formats[i].fmt);
    fail_unless_equals_int (GST_VIDEO_INFO_WIDTH (&vinfo), 2 * (i + 1));
    fail_unless_equals_int (GST_VIDEO_INFO_HEIGHT (&vinfo), i + 1);

    /* make sure they're serialised back correctly */
    caps2 = gst_video_info_to_caps (&vinfo);
    fail_unless (caps2 != NULL);
    if (!gst_caps_is_equal (caps, caps2)) {
      gchar *caps1s = gst_caps_to_string (caps);
      gchar *caps2s = gst_caps_to_string (caps2);
      fail ("caps [%s] not equal to caps2 [%s]", caps1s, caps2s);
      g_free (caps1s);
      g_free (caps2s);
    }

    gst_caps_unref (caps);
    gst_caps_unref (caps2);
  }
}

GST_END_TEST;

GST_START_TEST (test_parse_caps_multiview)
{
  gint i, j;
  GstVideoMultiviewMode modes[] = {
    GST_VIDEO_MULTIVIEW_MODE_MONO,
    GST_VIDEO_MULTIVIEW_MODE_LEFT,
    GST_VIDEO_MULTIVIEW_MODE_RIGHT,
    GST_VIDEO_MULTIVIEW_MODE_SIDE_BY_SIDE,
    GST_VIDEO_MULTIVIEW_MODE_SIDE_BY_SIDE_QUINCUNX,
    GST_VIDEO_MULTIVIEW_MODE_COLUMN_INTERLEAVED,
    GST_VIDEO_MULTIVIEW_MODE_ROW_INTERLEAVED,
    GST_VIDEO_MULTIVIEW_MODE_TOP_BOTTOM,
    GST_VIDEO_MULTIVIEW_MODE_CHECKERBOARD,
    GST_VIDEO_MULTIVIEW_MODE_FRAME_BY_FRAME,
    GST_VIDEO_MULTIVIEW_MODE_MULTIVIEW_FRAME_BY_FRAME,
    GST_VIDEO_MULTIVIEW_MODE_SEPARATED,
  };
  GstVideoMultiviewFlags flags[] = {
    GST_VIDEO_MULTIVIEW_FLAGS_NONE,
    GST_VIDEO_MULTIVIEW_FLAGS_RIGHT_VIEW_FIRST,
    GST_VIDEO_MULTIVIEW_FLAGS_LEFT_FLIPPED,
    GST_VIDEO_MULTIVIEW_FLAGS_LEFT_FLOPPED,
    GST_VIDEO_MULTIVIEW_FLAGS_RIGHT_FLIPPED,
    GST_VIDEO_MULTIVIEW_FLAGS_RIGHT_FLOPPED,
    GST_VIDEO_MULTIVIEW_FLAGS_MIXED_MONO,
    GST_VIDEO_MULTIVIEW_FLAGS_MIXED_MONO |
        GST_VIDEO_MULTIVIEW_FLAGS_RIGHT_VIEW_FIRST,
    GST_VIDEO_MULTIVIEW_FLAGS_MIXED_MONO |
        GST_VIDEO_MULTIVIEW_FLAGS_LEFT_FLIPPED
  };

  for (i = 0; i < G_N_ELEMENTS (modes); i++) {
    for (j = 0; j < G_N_ELEMENTS (flags); j++) {
      GstVideoInfo vinfo;
      GstCaps *caps;

      gst_video_info_init (&vinfo);
      fail_unless (gst_video_info_set_format (&vinfo, GST_VIDEO_FORMAT_I420,
              320, 240));

      GST_VIDEO_INFO_MULTIVIEW_MODE (&vinfo) = modes[i];
      GST_VIDEO_INFO_MULTIVIEW_FLAGS (&vinfo) = flags[j];

      caps = gst_video_info_to_caps (&vinfo);
      fail_if (caps == NULL);
      GST_LOG ("mview mode %d flags %x -> caps %" GST_PTR_FORMAT,
          modes[i], flags[j], caps);

      fail_unless (gst_video_info_from_caps (&vinfo, caps));

      GST_LOG ("mview mode %d flags %x -> info mode %d flags %x",
          modes[i], flags[j], GST_VIDEO_INFO_MULTIVIEW_MODE (&vinfo),
          GST_VIDEO_INFO_MULTIVIEW_FLAGS (&vinfo));

      fail_unless (GST_VIDEO_INFO_MULTIVIEW_MODE (&vinfo) == modes[i],
          "Expected multiview mode %d got mode %d", modes[i],
          GST_VIDEO_INFO_MULTIVIEW_MODE (&vinfo));
      fail_unless (GST_VIDEO_INFO_MULTIVIEW_FLAGS (&vinfo) == flags[j],
          "Expected multiview flags 0x%x got 0x%x", flags[j],
          GST_VIDEO_INFO_MULTIVIEW_FLAGS (&vinfo));

      gst_caps_unref (caps);
    }
  }
}

GST_END_TEST;

typedef struct
{
  const gchar *string_from;
  const gchar *string_to;
  const gchar *name;
  GstVideoColorimetry color;
} ColorimetryTest;

#define MAKE_COLORIMETRY_TEST(s1,s2,n,r,m,t,p) { s1, s2, n,         \
    { GST_VIDEO_COLOR_RANGE ##r, GST_VIDEO_COLOR_MATRIX_ ##m,       \
    GST_VIDEO_TRANSFER_ ##t, GST_VIDEO_COLOR_PRIMARIES_ ##p } }

GST_START_TEST (test_parse_colorimetry)
{
  ColorimetryTest tests[] = {
    MAKE_COLORIMETRY_TEST ("bt601", "bt601", "bt601",
        _16_235, BT601, BT601, SMPTE170M),
    MAKE_COLORIMETRY_TEST ("2:4:5:4", "2:4:5:4", NULL,
        _16_235, BT601, BT709, SMPTE170M),
    MAKE_COLORIMETRY_TEST ("bt709", "bt709", "bt709",
        _16_235, BT709, BT709, BT709),
    MAKE_COLORIMETRY_TEST ("smpte240m", "smpte240m", "smpte240m",
        _16_235, SMPTE240M, SMPTE240M, SMPTE240M),
    MAKE_COLORIMETRY_TEST ("sRGB", "sRGB", "sRGB",
        _0_255, RGB, SRGB, BT709),
    MAKE_COLORIMETRY_TEST ("bt2020", "bt2020", "bt2020",
        _16_235, BT2020, BT2020_12, BT2020),
    MAKE_COLORIMETRY_TEST ("1:4:0:0", "1:4:0:0", NULL,
        _0_255, BT601, UNKNOWN, UNKNOWN),
  };
  gint i;

  for (i = 0; i < G_N_ELEMENTS (tests); i++) {
    const ColorimetryTest *test = &tests[i];
    GstVideoColorimetry color;
    gchar *string;

    fail_unless (gst_video_colorimetry_from_string (&color, test->string_from));
    fail_unless_equals_int (color.range, test->color.range);
    fail_unless_equals_int (color.matrix, test->color.matrix);
    fail_unless_equals_int (color.transfer, test->color.transfer);
    fail_unless_equals_int (color.primaries, test->color.primaries);

    string = gst_video_colorimetry_to_string (&color);
    fail_unless_equals_string (string, test->string_to);
    g_free (string);

    fail_unless (gst_video_colorimetry_is_equal (&color, &test->color));

    if (test->name)
      fail_unless (gst_video_colorimetry_matches (&color, test->name));
  }
}

GST_END_TEST;

GST_START_TEST (test_events)
{
  GstEvent *e;
  gboolean in_still;

  e = gst_video_event_new_still_frame (TRUE);
  fail_if (e == NULL, "Failed to create still frame event");
  fail_unless (gst_video_event_parse_still_frame (e, &in_still),
      "Failed to parse still frame event");
  fail_unless (gst_video_event_parse_still_frame (e, NULL),
      "Failed to parse still frame event w/ in_still == NULL");
  fail_unless (in_still == TRUE);
  gst_event_unref (e);

  e = gst_video_event_new_still_frame (FALSE);
  fail_if (e == NULL, "Failed to create still frame event");
  fail_unless (gst_video_event_parse_still_frame (e, &in_still),
      "Failed to parse still frame event");
  fail_unless (gst_video_event_parse_still_frame (e, NULL),
      "Failed to parse still frame event w/ in_still == NULL");
  fail_unless (in_still == FALSE);
  gst_event_unref (e);
}

GST_END_TEST;

GST_START_TEST (test_convert_frame)
{
  GstVideoInfo vinfo;
  GstCaps *from_caps, *to_caps;
  GstBuffer *from_buffer;
  GstSample *from_sample, *to_sample;
  GError *error = NULL;
  gint i;
  GstMapInfo map;

  gst_debug_set_threshold_for_name ("default", GST_LEVEL_NONE);

  from_buffer = gst_buffer_new_and_alloc (640 * 480 * 4);

  gst_buffer_map (from_buffer, &map, GST_MAP_WRITE);
  for (i = 0; i < 640 * 480; i++) {
    map.data[4 * i + 0] = 0;    /* x */
    map.data[4 * i + 1] = 255;  /* R */
    map.data[4 * i + 2] = 0;    /* G */
    map.data[4 * i + 3] = 0;    /* B */
  }
  gst_buffer_unmap (from_buffer, &map);

  gst_video_info_init (&vinfo);
  fail_unless (gst_video_info_set_format (&vinfo, GST_VIDEO_FORMAT_xRGB, 640,
          480));
  vinfo.fps_n = 25;
  vinfo.fps_d = 1;
  vinfo.par_n = 1;
  vinfo.par_d = 1;
  from_caps = gst_video_info_to_caps (&vinfo);

  from_sample = gst_sample_new (from_buffer, from_caps, NULL, NULL);

  to_caps =
      gst_caps_from_string
      ("something/that, does=(string)not, exist=(boolean)FALSE");

  to_sample =
      gst_video_convert_sample (from_sample, to_caps,
      GST_CLOCK_TIME_NONE, &error);
  fail_if (to_sample != NULL);
  fail_unless (error != NULL);
  g_error_free (error);
  error = NULL;

  gst_caps_unref (to_caps);
  fail_unless (gst_video_info_set_format (&vinfo, GST_VIDEO_FORMAT_I420, 240,
          320));
  vinfo.fps_n = 25;
  vinfo.fps_d = 1;
  vinfo.par_n = 1;
  vinfo.par_d = 2;
  to_caps = gst_video_info_to_caps (&vinfo);

  to_sample =
      gst_video_convert_sample (from_sample, to_caps,
      GST_CLOCK_TIME_NONE, &error);
  fail_unless (to_sample != NULL);
  fail_unless (error == NULL);

  gst_buffer_unref (from_buffer);
  gst_caps_unref (from_caps);
  gst_sample_unref (from_sample);
  gst_sample_unref (to_sample);
  gst_caps_unref (to_caps);
}

GST_END_TEST;

typedef struct
{
  GMainLoop *loop;
  GstSample *sample;
  GError *error;
} ConvertFrameContext;

static void
convert_sample_async_callback (GstSample * sample, GError * err,
    ConvertFrameContext * cf_data)
{
  cf_data->sample = sample;
  cf_data->error = err;

  g_main_loop_quit (cf_data->loop);
}

GST_START_TEST (test_convert_frame_async)
{
  GstVideoInfo vinfo;
  GstCaps *from_caps, *to_caps;
  GstBuffer *from_buffer;
  GstSample *from_sample;
  gint i;
  GstMapInfo map;
  GMainLoop *loop;
  ConvertFrameContext cf_data = { NULL, NULL, NULL };

  gst_debug_set_threshold_for_name ("default", GST_LEVEL_NONE);

  from_buffer = gst_buffer_new_and_alloc (640 * 480 * 4);

  gst_buffer_map (from_buffer, &map, GST_MAP_WRITE);
  for (i = 0; i < 640 * 480; i++) {
    map.data[4 * i + 0] = 0;    /* x */
    map.data[4 * i + 1] = 255;  /* R */
    map.data[4 * i + 2] = 0;    /* G */
    map.data[4 * i + 3] = 0;    /* B */
  }
  gst_buffer_unmap (from_buffer, &map);

  loop = cf_data.loop = g_main_loop_new (NULL, FALSE);

  gst_video_info_init (&vinfo);
  fail_unless (gst_video_info_set_format (&vinfo, GST_VIDEO_FORMAT_xRGB, 640,
          470));
  vinfo.par_n = 1;
  vinfo.par_d = 1;
  vinfo.fps_n = 25;
  vinfo.fps_d = 1;
  from_caps = gst_video_info_to_caps (&vinfo);

  from_sample = gst_sample_new (from_buffer, from_caps, NULL, NULL);
  gst_buffer_unref (from_buffer);
  gst_caps_unref (from_caps);

  gst_video_info_init (&vinfo);
  fail_unless (gst_video_info_set_format (&vinfo, GST_VIDEO_FORMAT_I420, 240,
          320));
  vinfo.par_n = 1;
  vinfo.par_d = 2;
  vinfo.fps_n = 25;
  vinfo.fps_d = 1;
  to_caps = gst_video_info_to_caps (&vinfo);
  gst_video_convert_sample_async (from_sample, to_caps,
      GST_CLOCK_TIME_NONE,
      (GstVideoConvertSampleCallback) convert_sample_async_callback, &cf_data,
      NULL);
  g_main_loop_run (loop);
  fail_unless (cf_data.sample != NULL);
  fail_unless (cf_data.error == NULL);

  gst_sample_unref (cf_data.sample);
  gst_caps_unref (to_caps);
  gst_sample_unref (from_sample);

  g_main_loop_unref (loop);
}

GST_END_TEST;

GST_START_TEST (test_convert_frame_async_error)
{
  GstVideoInfo vinfo;
  GstCaps *from_caps, *to_caps;
  GstBuffer *from_buffer;
  GstSample *from_sample;
  gint i;
  GstMapInfo map;
  GMainLoop *loop;
  ConvertFrameContext cf_data = { NULL, NULL, NULL };

  gst_debug_set_threshold_for_name ("default", GST_LEVEL_NONE);

  from_buffer = gst_buffer_new_and_alloc (640 * 480 * 4);

  gst_buffer_map (from_buffer, &map, GST_MAP_WRITE);
  for (i = 0; i < 640 * 480; i++) {
    map.data[4 * i + 0] = 0;    /* x */
    map.data[4 * i + 1] = 255;  /* R */
    map.data[4 * i + 2] = 0;    /* G */
    map.data[4 * i + 3] = 0;    /* B */
  }
  gst_buffer_unmap (from_buffer, &map);

  gst_video_info_init (&vinfo);
  fail_unless (gst_video_info_set_format (&vinfo, GST_VIDEO_FORMAT_xRGB, 640,
          470));
  vinfo.par_n = 1;
  vinfo.par_d = 1;
  vinfo.fps_n = 25;
  vinfo.fps_d = 1;
  from_caps = gst_video_info_to_caps (&vinfo);

  to_caps =
      gst_caps_from_string
      ("something/that, does=(string)not, exist=(boolean)FALSE");

  loop = cf_data.loop = g_main_loop_new (NULL, FALSE);

  from_sample = gst_sample_new (from_buffer, from_caps, NULL, NULL);
  gst_buffer_unref (from_buffer);
  gst_caps_unref (from_caps);

  gst_video_convert_sample_async (from_sample, to_caps,
      GST_CLOCK_TIME_NONE,
      (GstVideoConvertSampleCallback) convert_sample_async_callback, &cf_data,
      NULL);

  g_main_loop_run (loop);

  fail_if (cf_data.sample != NULL);
  fail_unless (cf_data.error != NULL);
  g_error_free (cf_data.error);
  cf_data.error = NULL;

  gst_caps_unref (to_caps);
  gst_sample_unref (from_sample);

  g_main_loop_unref (loop);
}

GST_END_TEST;

GST_START_TEST (test_video_size_from_caps)
{
  GstVideoInfo vinfo;
  GstCaps *caps;

  caps = gst_caps_new_simple ("video/x-raw",
      "format", G_TYPE_STRING, "YV12",
      "width", G_TYPE_INT, 640,
      "height", G_TYPE_INT, 480, "framerate", GST_TYPE_FRACTION, 25, 1, NULL);

  gst_video_info_init (&vinfo);
  fail_unless (gst_video_info_from_caps (&vinfo, caps));
  fail_unless (GST_VIDEO_INFO_SIZE (&vinfo) == (640 * 480 * 12 / 8));

  gst_caps_unref (caps);
}

GST_END_TEST;

GST_START_TEST (test_interlace_mode)
{
  GstVideoInfo vinfo;
  GstCaps *caps;
  GstStructure *structure;
  GstCapsFeatures *features;
  const char *mode_str, *order_str;
  int mode;
  GstVideoFieldOrder order;

  gst_video_info_init (&vinfo);

  /* Progressive */
  fail_unless (gst_video_info_set_interlaced_format (&vinfo,
          GST_VIDEO_FORMAT_YV12, GST_VIDEO_INTERLACE_MODE_PROGRESSIVE, 320,
          240));
  fail_unless (GST_VIDEO_INFO_SIZE (&vinfo) == 115200);

  caps = gst_video_info_to_caps (&vinfo);
  fail_unless (caps != NULL);
  structure = gst_caps_get_structure (caps, 0);
  fail_unless (structure != NULL);
  mode_str = gst_structure_get_string (structure, "interlace-mode");
  mode = gst_video_interlace_mode_from_string (mode_str);
  fail_unless (mode == GST_VIDEO_INTERLACE_MODE_PROGRESSIVE);

  /* Converting back to video info */
  fail_unless (gst_video_info_from_caps (&vinfo, caps));
  fail_unless (GST_VIDEO_INFO_INTERLACE_MODE (&vinfo) ==
      GST_VIDEO_INTERLACE_MODE_PROGRESSIVE);

  gst_caps_unref (caps);

  /* Interlaced with alternate frame on buffers */
  fail_unless (gst_video_info_set_interlaced_format (&vinfo,
          GST_VIDEO_FORMAT_YV12, GST_VIDEO_INTERLACE_MODE_ALTERNATE, 320, 240));
  fail_unless (GST_VIDEO_INFO_SIZE (&vinfo) == 57600);
  GST_VIDEO_INFO_FIELD_ORDER (&vinfo) = GST_VIDEO_FIELD_ORDER_TOP_FIELD_FIRST;

  caps = gst_video_info_to_caps (&vinfo);
  fail_unless (caps != NULL);
  structure = gst_caps_get_structure (caps, 0);
  fail_unless (structure != NULL);
  mode_str = gst_structure_get_string (structure, "interlace-mode");
  mode = gst_video_interlace_mode_from_string (mode_str);
  fail_unless (mode == GST_VIDEO_INTERLACE_MODE_ALTERNATE);
  order_str = gst_structure_get_string (structure, "field-order");
  order = gst_video_field_order_from_string (order_str);
  fail_unless (order == GST_VIDEO_FIELD_ORDER_TOP_FIELD_FIRST);
  /* 'alternate' mode must always be accompanied by interlaced caps feature. */
  features = gst_caps_get_features (caps, 0);
  fail_unless (gst_caps_features_contains (features,
          GST_CAPS_FEATURE_FORMAT_INTERLACED));

  /* Converting back to video info */
  fail_unless (gst_video_info_from_caps (&vinfo, caps));
  fail_unless (GST_VIDEO_INFO_INTERLACE_MODE (&vinfo) ==
      GST_VIDEO_INTERLACE_MODE_ALTERNATE);
  fail_unless (GST_VIDEO_INFO_FIELD_ORDER (&vinfo) ==
      GST_VIDEO_FIELD_ORDER_TOP_FIELD_FIRST);

  gst_caps_unref (caps);

  /* gst_video_info_from_caps() fails if an alternate stream doesn't contain
   * the caps feature. */
  caps =
      gst_caps_from_string
      ("video/x-raw, format=NV12, width=320, height=240, interlace-mode=alternate");
  fail_unless (caps);

  fail_if (gst_video_info_from_caps (&vinfo, caps));
  gst_caps_unref (caps);

  /* ... but it's ok for encoded video */
  caps =
      gst_caps_from_string
      ("video/x-h265, width=320, height=240, interlace-mode=alternate");
  fail_unless (caps);

  fail_unless (gst_video_info_from_caps (&vinfo, caps));
  gst_caps_unref (caps);
}

GST_END_TEST;

GST_START_TEST (test_overlay_composition)
{
  GstVideoOverlayComposition *comp1, *comp2;
  GstVideoOverlayRectangle *rect1, *rect2;
  GstVideoOverlayCompositionMeta *ometa;
  GstBuffer *pix1, *pix2, *buf;
  GstVideoMeta *vmeta;
  guint seq1, seq2;
  guint w, h, stride;
  gint x, y;
  guint8 val;

  pix1 = gst_buffer_new_and_alloc (200 * sizeof (guint32) * 50);
  gst_buffer_memset (pix1, 0, 0, gst_buffer_get_size (pix1));

  gst_buffer_add_video_meta (pix1, GST_VIDEO_FRAME_FLAG_NONE,
      GST_VIDEO_OVERLAY_COMPOSITION_FORMAT_RGB, 200, 50);
  rect1 = gst_video_overlay_rectangle_new_raw (pix1,
      600, 50, 300, 50, GST_VIDEO_OVERLAY_FORMAT_FLAG_NONE);

  gst_buffer_unref (pix1);
  pix1 = NULL;

  comp1 = gst_video_overlay_composition_new (rect1);
  fail_unless (gst_video_overlay_composition_n_rectangles (comp1) == 1);
  fail_unless (gst_video_overlay_composition_get_rectangle (comp1, 0) == rect1);
  fail_unless (gst_video_overlay_composition_get_rectangle (comp1, 1) == NULL);

  /* rectangle was created first, sequence number should be smaller */
  seq1 = gst_video_overlay_rectangle_get_seqnum (rect1);
  seq2 = gst_video_overlay_composition_get_seqnum (comp1);
  fail_unless (seq1 < seq2);

  /* composition took own ref, so refcount is 2 now, so this should fail */
  ASSERT_CRITICAL (gst_video_overlay_rectangle_set_render_rectangle (rect1, 50,
          600, 300, 50));

  /* drop our ref, so refcount is 1 (we know it will continue to be valid) */
  gst_video_overlay_rectangle_unref (rect1);
  gst_video_overlay_rectangle_set_render_rectangle (rect1, 50, 600, 300, 50);

  comp2 = gst_video_overlay_composition_new (rect1);
  fail_unless (gst_video_overlay_composition_n_rectangles (comp2) == 1);
  fail_unless (gst_video_overlay_composition_get_rectangle (comp2, 0) == rect1);
  fail_unless (gst_video_overlay_composition_get_rectangle (comp2, 1) == NULL);

  fail_unless (seq1 < gst_video_overlay_composition_get_seqnum (comp2));
  fail_unless (seq2 < gst_video_overlay_composition_get_seqnum (comp2));

  /* now refcount is 2 again because comp2 has also taken a ref, so must fail */
  ASSERT_CRITICAL (gst_video_overlay_rectangle_set_render_rectangle (rect1, 0,
          0, 1, 1));

  /* this should make a copy of the rectangles so drop the original
   * second ref on rect1 */
  comp2 = gst_video_overlay_composition_make_writable (comp2);
  gst_video_overlay_rectangle_set_render_rectangle (rect1, 51, 601, 301, 51);

  rect2 = gst_video_overlay_composition_get_rectangle (comp2, 0);
  fail_unless (gst_video_overlay_composition_n_rectangles (comp2) == 1);
  fail_unless (gst_video_overlay_composition_get_rectangle (comp2, 0) == rect2);
  fail_unless (gst_video_overlay_composition_get_rectangle (comp2, 1) == NULL);
  fail_unless (rect1 != rect2);

  gst_video_overlay_composition_add_rectangle (comp1, rect2);
  gst_video_overlay_composition_ref (comp1);
  ASSERT_CRITICAL (gst_video_overlay_composition_add_rectangle (comp1, rect2));
  gst_video_overlay_composition_unref (comp1);

  /* make sure the copy really worked */
  gst_video_overlay_rectangle_get_render_rectangle (rect1, &x, &y, &w, &h);
  fail_unless_equals_int (x, 51);
  fail_unless_equals_int (y, 601);
  fail_unless_equals_int (w, 301);
  fail_unless_equals_int (h, 51);

  /* get scaled pixbuf and touch last byte */
  pix1 = gst_video_overlay_rectangle_get_pixels_raw (rect1,
      GST_VIDEO_OVERLAY_FORMAT_FLAG_NONE);
  stride = 4 * w;
  fail_unless (gst_buffer_get_size (pix1) > ((h - 1) * stride + (w * 4) - 1),
      "size %u vs. last pixel offset %u", gst_buffer_get_size (pix1),
      ((h - 1) * stride + (w * 4) - 1));
  gst_buffer_extract (pix1, ((h - 1) * stride + (w * 4) - 1), &val, 1);
  fail_unless_equals_int (val, 0);

  gst_video_overlay_rectangle_get_render_rectangle (rect2, &x, &y, &w, &h);
  fail_unless_equals_int (x, 50);
  fail_unless_equals_int (y, 600);
  fail_unless_equals_int (w, 300);
  fail_unless_equals_int (h, 50);

  /* get scaled pixbuf and touch last byte */
  pix2 = gst_video_overlay_rectangle_get_pixels_raw (rect2,
      GST_VIDEO_OVERLAY_FORMAT_FLAG_NONE);
  stride = 4 * w;
  fail_unless (gst_buffer_get_size (pix2) > ((h - 1) * stride + (w * 4) - 1),
      "size %u vs. last pixel offset %u", gst_buffer_get_size (pix1),
      ((h - 1) * stride + (w * 4) - 1));
  gst_buffer_extract (pix2, ((h - 1) * stride + (w * 4) - 1), &val, 1);
  fail_unless_equals_int (val, 0);

  /* get scaled pixbuf again, should be the same buffer as before (caching) */
  pix1 = gst_video_overlay_rectangle_get_pixels_raw (rect2,
      GST_VIDEO_OVERLAY_FORMAT_FLAG_NONE);
  fail_unless (pix1 == pix2);

  /* get in different format */
  pix1 = gst_video_overlay_rectangle_get_pixels_ayuv (rect2,
      GST_VIDEO_OVERLAY_FORMAT_FLAG_NONE);
  fail_unless (pix1 != pix2);
  /* get it again, should be same (caching) */
  pix2 = gst_video_overlay_rectangle_get_pixels_ayuv (rect2,
      GST_VIDEO_OVERLAY_FORMAT_FLAG_NONE);
  fail_unless (pix1 == pix2);
  /* get unscaled, should be different */
  pix2 = gst_video_overlay_rectangle_get_pixels_unscaled_ayuv (rect2,
      GST_VIDEO_OVERLAY_FORMAT_FLAG_NONE);
  fail_unless (pix1 != pix2);
  /* but should be cached */
  pix1 = gst_video_overlay_rectangle_get_pixels_unscaled_ayuv (rect2,
      GST_VIDEO_OVERLAY_FORMAT_FLAG_NONE);
  fail_unless (pix1 == pix2);

  vmeta = gst_buffer_get_video_meta (pix1);
  fail_unless (vmeta != NULL);
  w = vmeta->width;
  h = vmeta->height;
  fail_unless_equals_int (w, 200);
  fail_unless_equals_int (h, 50);
  fail_unless_equals_int (vmeta->format,
      GST_VIDEO_OVERLAY_COMPOSITION_FORMAT_YUV);
  fail_unless (gst_buffer_get_size (pix1) == w * h * 4);
  gst_buffer_extract (pix1, 0, &seq1, 4);
  fail_unless (seq1 != 0);

  /* now compare the original unscaled ones */
  pix1 = gst_video_overlay_rectangle_get_pixels_unscaled_raw (rect1,
      GST_VIDEO_OVERLAY_FORMAT_FLAG_NONE);
  pix2 = gst_video_overlay_rectangle_get_pixels_unscaled_raw (rect2,
      GST_VIDEO_OVERLAY_FORMAT_FLAG_NONE);

  vmeta = gst_buffer_get_video_meta (pix2);
  fail_unless (vmeta != NULL);
  w = vmeta->width;
  h = vmeta->height;

  /* the original pixel buffers should be identical */
  fail_unless (pix1 == pix2);
  fail_unless_equals_int (w, 200);
  fail_unless_equals_int (h, 50);
  stride = 4 * w;

  /* touch last byte */
  fail_unless (gst_buffer_get_size (pix1) > ((h - 1) * stride + (w * 4) - 1),
      "size %u vs. last pixel offset %u", gst_buffer_get_size (pix1),
      ((h - 1) * stride + (w * 4) - 1));
  gst_buffer_extract (pix1, ((h - 1) * stride + (w * 4) - 1), &val, 1);
  fail_unless_equals_int (val, 0);

  /* test attaching and retrieving of compositions to/from buffers */
  buf = gst_buffer_new ();
  fail_unless (gst_buffer_get_video_overlay_composition_meta (buf) == NULL);

  gst_buffer_ref (buf);
  /* buffer now has refcount of 2, so its metadata is not writable.
   * only check this if we are not running in valgrind, as it leaks */
#ifdef HAVE_VALGRIND
  if (!RUNNING_ON_VALGRIND) {
    ASSERT_CRITICAL (gst_buffer_add_video_overlay_composition_meta (buf,
            comp1));
  }
#endif
  gst_buffer_unref (buf);
  gst_buffer_add_video_overlay_composition_meta (buf, comp1);
  ometa = gst_buffer_get_video_overlay_composition_meta (buf);
  fail_unless (ometa != NULL);
  fail_unless (ometa->overlay == comp1);
  fail_unless (gst_buffer_remove_video_overlay_composition_meta (buf, ometa));
  gst_buffer_add_video_overlay_composition_meta (buf, comp2);
  ometa = gst_buffer_get_video_overlay_composition_meta (buf);
  fail_unless (ometa->overlay == comp2);
  fail_unless (gst_buffer_remove_video_overlay_composition_meta (buf, ometa));
  fail_unless (gst_buffer_get_video_overlay_composition_meta (buf) == NULL);

  /* make sure the buffer cleans up its composition ref when unreffed */
  gst_buffer_add_video_overlay_composition_meta (buf, comp2);
  gst_buffer_unref (buf);

  gst_video_overlay_composition_unref (comp2);
  gst_video_overlay_composition_unref (comp1);
}

GST_END_TEST;

GST_START_TEST (test_overlay_composition_premultiplied_alpha)
{
  GstVideoOverlayRectangle *rect1;
  GstVideoMeta *vmeta;
  GstBuffer *pix1, *pix2, *pix3, *pix4, *pix5;
  GstBuffer *pix6, *pix7, *pix8, *pix9, *pix10;
  guint8 *data5, *data7;
  guint w, h, w2, h2;
  GstMapInfo map;

  pix1 = gst_buffer_new_and_alloc (200 * sizeof (guint32) * 50);
  gst_buffer_memset (pix1, 0, 0x80, gst_buffer_get_size (pix1));

  gst_buffer_add_video_meta (pix1, GST_VIDEO_FRAME_FLAG_NONE,
      GST_VIDEO_OVERLAY_COMPOSITION_FORMAT_RGB, 200, 50);
  rect1 = gst_video_overlay_rectangle_new_raw (pix1,
      600, 50, 300, 50, GST_VIDEO_OVERLAY_FORMAT_FLAG_NONE);
  gst_buffer_unref (pix1);

  /* same flags, unscaled, should be the same buffer */
  pix2 = gst_video_overlay_rectangle_get_pixels_unscaled_raw (rect1,
      GST_VIDEO_OVERLAY_FORMAT_FLAG_NONE);
  fail_unless (pix1 == pix2);

  /* same flags, but scaled */
  pix3 = gst_video_overlay_rectangle_get_pixels_raw (rect1,
      GST_VIDEO_OVERLAY_FORMAT_FLAG_NONE);
  fail_if (pix3 == pix1 || pix3 == pix2);

  /* same again, should hopefully get the same (cached) buffer as before */
  pix4 = gst_video_overlay_rectangle_get_pixels_raw (rect1,
      GST_VIDEO_OVERLAY_FORMAT_FLAG_NONE);
  fail_unless (pix4 == pix3);

  /* just to update the vars */
  pix2 = gst_video_overlay_rectangle_get_pixels_unscaled_raw (rect1,
      GST_VIDEO_OVERLAY_FORMAT_FLAG_NONE);

  vmeta = gst_buffer_get_video_meta (pix2);
  fail_unless (vmeta != NULL);
  w = vmeta->width;
  h = vmeta->height;

  /* now, let's try to get premultiplied alpha from the unpremultiplied input */
  pix5 = gst_video_overlay_rectangle_get_pixels_unscaled_raw (rect1,
      GST_VIDEO_OVERLAY_FORMAT_FLAG_PREMULTIPLIED_ALPHA);
  fail_if (pix5 == pix1 || pix5 == pix2 || pix5 == pix3);
  vmeta = gst_buffer_get_video_meta (pix5);
  fail_unless (vmeta != NULL);
  w2 = vmeta->width;
  h2 = vmeta->height;
  fail_unless_equals_int (w, w2);
  fail_unless_equals_int (h, h2);
  fail_unless_equals_int (gst_buffer_get_size (pix2),
      gst_buffer_get_size (pix5));
  gst_buffer_map (pix5, &map, GST_MAP_READ);
  fail_if (gst_buffer_memcmp (pix2, 0, map.data, map.size) == 0);
  /* make sure it actually did what we expected it to do (input=0x80808080) */
  data5 = map.data;
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
  /* B - G - R - A */
  fail_unless_equals_int (data5[0], 0x40);
  fail_unless_equals_int (data5[1], 0x40);
  fail_unless_equals_int (data5[2], 0x40);
  fail_unless_equals_int (data5[3], 0x80);
#else
  /* A - R - G - B */
  fail_unless_equals_int (data5[0], 0x80);
  fail_unless_equals_int (data5[1], 0x40);
  fail_unless_equals_int (data5[2], 0x40);
  fail_unless_equals_int (data5[3], 0x40);
#endif
  gst_buffer_unmap (pix5, &map);

  /* same again, now we should be getting back the same buffer as before,
   * as it should have been cached */
  pix6 = gst_video_overlay_rectangle_get_pixels_unscaled_raw (rect1,
      GST_VIDEO_OVERLAY_FORMAT_FLAG_PREMULTIPLIED_ALPHA);
  fail_unless (pix6 == pix5);

  /* just to update the stride var */
  pix3 = gst_video_overlay_rectangle_get_pixels_raw (rect1,
      GST_VIDEO_OVERLAY_FORMAT_FLAG_NONE);
  fail_unless (pix3 == pix4);

  /* now try to get scaled premultiplied alpha from unpremultiplied input */
  pix7 = gst_video_overlay_rectangle_get_pixels_raw (rect1,
      GST_VIDEO_OVERLAY_FORMAT_FLAG_PREMULTIPLIED_ALPHA);
  fail_if (pix7 == pix1 || pix7 == pix2 || pix7 == pix3 || pix7 == pix5);

  gst_buffer_map (pix7, &map, GST_MAP_READ);
  data7 = map.data;
  /* make sure it actually did what we expected it to do (input=0x80808080)
   * hoping that the scaling didn't mess up our values */
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
  /* B - G - R - A */
  fail_unless_equals_int (data7[0], 0x40);
  fail_unless_equals_int (data7[1], 0x40);
  fail_unless_equals_int (data7[2], 0x40);
  fail_unless_equals_int (data7[3], 0x80);
#else
  /* A - R - G - B */
  fail_unless_equals_int (data7[0], 0x80);
  fail_unless_equals_int (data7[1], 0x40);
  fail_unless_equals_int (data7[2], 0x40);
  fail_unless_equals_int (data7[3], 0x40);
#endif
  gst_buffer_unmap (pix7, &map);

  /* and the same again, it should be cached now */
  pix8 = gst_video_overlay_rectangle_get_pixels_raw (rect1,
      GST_VIDEO_OVERLAY_FORMAT_FLAG_PREMULTIPLIED_ALPHA);
  fail_unless (pix8 == pix7);

  /* make sure other cached stuff is still there */
  pix9 = gst_video_overlay_rectangle_get_pixels_raw (rect1,
      GST_VIDEO_OVERLAY_FORMAT_FLAG_NONE);
  fail_unless (pix9 == pix3);
  pix10 = gst_video_overlay_rectangle_get_pixels_unscaled_raw (rect1,
      GST_VIDEO_OVERLAY_FORMAT_FLAG_PREMULTIPLIED_ALPHA);
  fail_unless (pix10 == pix5);

  gst_video_overlay_rectangle_unref (rect1);
}

GST_END_TEST;

GST_START_TEST (test_overlay_composition_global_alpha)
{
  GstVideoOverlayRectangle *rect1;
  GstBuffer *pix1, *pix2, *pix3, *pix4, *pix5;
  GstVideoMeta *vmeta;
  guint8 *data2, *data4, *data5;
  guint w, h, w4, h4;
  guint seq1, seq2;
  gfloat ga1, ga2;
  GstVideoOverlayFormatFlags flags1;
  GstMapInfo map;

  pix1 = gst_buffer_new_and_alloc (200 * sizeof (guint32) * 50);
  gst_buffer_memset (pix1, 0, 0x80, gst_buffer_get_size (pix1));

  gst_buffer_add_video_meta (pix1, GST_VIDEO_FRAME_FLAG_NONE,
      GST_VIDEO_OVERLAY_COMPOSITION_FORMAT_RGB, 200, 50);
  rect1 = gst_video_overlay_rectangle_new_raw (pix1,
      600, 50, 300, 50, GST_VIDEO_OVERLAY_FORMAT_FLAG_NONE);
  gst_buffer_unref (pix1);

  /* same flags, unscaled, should be the same buffer */
  pix2 = gst_video_overlay_rectangle_get_pixels_unscaled_raw (rect1,
      GST_VIDEO_OVERLAY_FORMAT_FLAG_NONE);
  fail_unless (pix1 == pix2);

  vmeta = gst_buffer_get_video_meta (pix2);
  fail_unless (vmeta != NULL);
  w = vmeta->width;
  h = vmeta->height;

  /* same flags, but scaled */
  pix3 = gst_video_overlay_rectangle_get_pixels_raw (rect1,
      GST_VIDEO_OVERLAY_FORMAT_FLAG_NONE);
  fail_if (pix3 == pix1 || pix3 == pix2);

  /* get unscaled premultiplied data, new cached rectangle should be created */
  pix4 = gst_video_overlay_rectangle_get_pixels_unscaled_raw (rect1,
      GST_VIDEO_OVERLAY_FORMAT_FLAG_PREMULTIPLIED_ALPHA);
  fail_if (pix4 == pix2 || pix4 == pix3);
  vmeta = gst_buffer_get_video_meta (pix4);
  fail_unless (vmeta != NULL);
  w4 = vmeta->width;
  h4 = vmeta->height;
  fail_unless_equals_int (w, w4);
  fail_unless_equals_int (h, h4);
  fail_unless_equals_int (gst_buffer_get_size (pix2),
      gst_buffer_get_size (pix4));
  gst_buffer_map (pix4, &map, GST_MAP_READ);
  fail_if (gst_buffer_memcmp (pix1, 0, map.data, map.size) == 0);
  /* make sure it actually did what we expected it to do (input=0x80808080) */
  data4 = map.data;
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
  /* B - G - R - A */
  fail_unless_equals_int (data4[0], 0x40);
  fail_unless_equals_int (data4[1], 0x40);
  fail_unless_equals_int (data4[2], 0x40);
  fail_unless_equals_int (data4[3], 0x80);
#else
  /* A - R - G - B */
  fail_unless_equals_int (data4[0], 0x80);
  fail_unless_equals_int (data4[1], 0x40);
  fail_unless_equals_int (data4[2], 0x40);
  fail_unless_equals_int (data4[3], 0x40);
#endif
  gst_buffer_unmap (pix4, &map);

  /* now premultiplied and scaled, again a new cached rectangle should be cached */
  pix5 = gst_video_overlay_rectangle_get_pixels_raw (rect1,
      GST_VIDEO_OVERLAY_FORMAT_FLAG_PREMULTIPLIED_ALPHA);
  fail_if (pix5 == pix2 || pix5 == pix3 || pix5 == pix4);
  /* stride and size should be equal to the first scaled rect */
  fail_unless_equals_int (gst_buffer_get_size (pix5),
      gst_buffer_get_size (pix3));
  /* data should be different (premutliplied) though */
  gst_buffer_map (pix5, &map, GST_MAP_READ);
  fail_if (gst_buffer_memcmp (pix3, 0, map.data, map.size) == 0);
  /* make sure it actually did what we expected it to do (input=0x80808080) */
  data5 = map.data;
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
  /* B - G - R - A */
  fail_unless_equals_int (data5[0], 0x40);
  fail_unless_equals_int (data5[1], 0x40);
  fail_unless_equals_int (data5[2], 0x40);
  fail_unless_equals_int (data5[3], 0x80);
#else
  /* A - R - G - B */
  fail_unless_equals_int (data5[0], 0x80);
  fail_unless_equals_int (data5[1], 0x40);
  fail_unless_equals_int (data5[2], 0x40);
  fail_unless_equals_int (data5[3], 0x40);
#endif
  gst_buffer_unmap (pix5, &map);

  /* global_alpha should initially be 1.0 */
  ga1 = gst_video_overlay_rectangle_get_global_alpha (rect1);
  fail_unless_equals_float (ga1, 1.0);

  /* now set global_alpha */
  seq1 = gst_video_overlay_rectangle_get_seqnum (rect1);
  gst_video_overlay_rectangle_set_global_alpha (rect1, 0.5);
  ga2 = gst_video_overlay_rectangle_get_global_alpha (rect1);
  fail_unless_equals_float (ga2, 0.5);

  /* seqnum should have changed */
  seq2 = gst_video_overlay_rectangle_get_seqnum (rect1);
  fail_unless (seq1 < seq2);

  /* internal flags should have been set */
  flags1 = gst_video_overlay_rectangle_get_flags (rect1);
  fail_unless_equals_int (flags1, GST_VIDEO_OVERLAY_FORMAT_FLAG_GLOBAL_ALPHA);

  /* request unscaled pixel-data, global-alpha not applied */
  pix2 = gst_video_overlay_rectangle_get_pixels_unscaled_raw (rect1,
      GST_VIDEO_OVERLAY_FORMAT_FLAG_GLOBAL_ALPHA);
  /* this should just return the same buffer */
  fail_unless (pix2 == pix1);
  /* make sure we got the initial data (input=0x80808080) */
  gst_buffer_map (pix2, &map, GST_MAP_READ);
  data2 = map.data;
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
  /* B - G - R - A */
  fail_unless_equals_int (data2[0], 0x80);
  fail_unless_equals_int (data2[1], 0x80);
  fail_unless_equals_int (data2[2], 0x80);
  fail_unless_equals_int (data2[3], 0x80);
#else
  /* A - R - G - B */
  fail_unless_equals_int (data2[0], 0x80);
  fail_unless_equals_int (data2[1], 0x80);
  fail_unless_equals_int (data2[2], 0x80);
  fail_unless_equals_int (data2[3], 0x80);
#endif
  gst_buffer_unmap (pix2, &map);

  /* unscaled pixel-data, global-alpha applied */
  pix2 = gst_video_overlay_rectangle_get_pixels_unscaled_raw (rect1,
      GST_VIDEO_OVERLAY_FORMAT_FLAG_NONE);
  /* this should be the same buffer with on-the-fly modified alpha-channel */
  fail_unless (pix2 == pix1);
  gst_buffer_map (pix2, &map, GST_MAP_READ);
  data2 = map.data;
  /* make sure we got the initial data with adjusted alpha-channel */
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
  /* B - G - R - A */
  fail_unless_equals_int (data2[0], 0x80);
  fail_unless_equals_int (data2[1], 0x80);
  fail_unless_equals_int (data2[2], 0x80);
  fail_unless_equals_int (data2[3], 0x40);
#else
  /* A - R - G - B */
  fail_unless_equals_int (data2[0], 0x40);
  fail_unless_equals_int (data2[1], 0x80);
  fail_unless_equals_int (data2[2], 0x80);
  fail_unless_equals_int (data2[3], 0x80);
#endif
  gst_buffer_unmap (pix2, &map);

  /* adjust global_alpha once more */
  gst_video_overlay_rectangle_set_global_alpha (rect1, 0.25);
  ga2 = gst_video_overlay_rectangle_get_global_alpha (rect1);
  fail_unless_equals_float (ga2, 0.25);
  /* and again request unscaled pixel-data, global-alpha applied */
  pix2 = gst_video_overlay_rectangle_get_pixels_unscaled_raw (rect1,
      GST_VIDEO_OVERLAY_FORMAT_FLAG_NONE);
  fail_unless (pix2 == pix1);
  /* make sure we got the initial data with adjusted alpha-channel */
  gst_buffer_map (pix2, &map, GST_MAP_READ);
  data2 = map.data;
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
  /* B - G - R - A */
  fail_unless_equals_int (data2[0], 0x80);
  fail_unless_equals_int (data2[1], 0x80);
  fail_unless_equals_int (data2[2], 0x80);
  fail_unless_equals_int (data2[3], 0x20);
#else
  /* A - R - G - B */
  fail_unless_equals_int (data2[0], 0x20);
  fail_unless_equals_int (data2[1], 0x80);
  fail_unless_equals_int (data2[2], 0x80);
  fail_unless_equals_int (data2[3], 0x80);
#endif
  gst_buffer_unmap (pix2, &map);

  /* again: unscaled pixel-data, global-alpha not applied,
   * this should revert alpha-channel to initial values */
  pix2 = gst_video_overlay_rectangle_get_pixels_unscaled_raw (rect1,
      GST_VIDEO_OVERLAY_FORMAT_FLAG_GLOBAL_ALPHA);
  fail_unless (pix2 == pix1);
  /* make sure we got the initial data (input=0x80808080) */
  gst_buffer_map (pix2, &map, GST_MAP_READ);
  data2 = map.data;
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
  /* B - G - R - A */
  fail_unless_equals_int (data2[0], 0x80);
  fail_unless_equals_int (data2[1], 0x80);
  fail_unless_equals_int (data2[2], 0x80);
  fail_unless_equals_int (data2[3], 0x80);
#else
  /* A - R - G - B */
  fail_unless_equals_int (data2[0], 0x80);
  fail_unless_equals_int (data2[1], 0x80);
  fail_unless_equals_int (data2[2], 0x80);
  fail_unless_equals_int (data2[3], 0x80);
#endif
  gst_buffer_unmap (pix2, &map);

  /* now scaled, global-alpha not applied */
  pix2 = gst_video_overlay_rectangle_get_pixels_raw (rect1,
      GST_VIDEO_OVERLAY_FORMAT_FLAG_GLOBAL_ALPHA);
  /* this should just return the rect/buffer, that was cached for these
   * scaling dimensions */
  fail_unless (pix2 == pix3);
  /* make sure we got the initial data (input=0x80808080) */
  gst_buffer_map (pix2, &map, GST_MAP_READ);
  data2 = map.data;
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
  /* B - G - R - A */
  fail_unless_equals_int (data2[0], 0x80);
  fail_unless_equals_int (data2[1], 0x80);
  fail_unless_equals_int (data2[2], 0x80);
  fail_unless_equals_int (data2[3], 0x80);
#else
  /* A - R - G - B */
  fail_unless_equals_int (data2[0], 0x80);
  fail_unless_equals_int (data2[1], 0x80);
  fail_unless_equals_int (data2[2], 0x80);
  fail_unless_equals_int (data2[3], 0x80);
#endif
  gst_buffer_unmap (pix2, &map);

  /* scaled, global-alpha (0.25) applied */
  pix2 = gst_video_overlay_rectangle_get_pixels_raw (rect1,
      GST_VIDEO_OVERLAY_FORMAT_FLAG_NONE);
  /* this should just return the rect/buffer, that was cached for these
   * scaling dimensions with modified alpha channel */
  fail_unless (pix2 == pix3);
  /* make sure we got the data we expect for global-alpha=0.25 */
  gst_buffer_map (pix2, &map, GST_MAP_READ);
  data2 = map.data;
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
  /* B - G - R - A */
  fail_unless_equals_int (data2[0], 0x80);
  fail_unless_equals_int (data2[1], 0x80);
  fail_unless_equals_int (data2[2], 0x80);
  fail_unless_equals_int (data2[3], 0x20);
#else
  /* A - R - G - B */
  fail_unless_equals_int (data2[0], 0x20);
  fail_unless_equals_int (data2[1], 0x80);
  fail_unless_equals_int (data2[2], 0x80);
  fail_unless_equals_int (data2[3], 0x80);
#endif
  gst_buffer_unmap (pix2, &map);

  /* now unscaled premultiplied data, global-alpha not applied,
   * is this really a valid use case?*/
  pix2 = gst_video_overlay_rectangle_get_pixels_unscaled_raw (rect1,
      GST_VIDEO_OVERLAY_FORMAT_FLAG_PREMULTIPLIED_ALPHA |
      GST_VIDEO_OVERLAY_FORMAT_FLAG_GLOBAL_ALPHA);
  /* this should just return the rect/buffer, that was cached for the
   * premultiplied data */
  fail_unless (pix2 == pix4);
  /* make sure we got what we expected */
  gst_buffer_map (pix2, &map, GST_MAP_READ);
  data2 = map.data;
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
  /* B - G - R - A */
  fail_unless_equals_int (data2[0], 0x40);
  fail_unless_equals_int (data2[1], 0x40);
  fail_unless_equals_int (data2[2], 0x40);
  fail_unless_equals_int (data2[3], 0x80);
#else
  /* A - R - G - B */
  fail_unless_equals_int (data2[0], 0x80);
  fail_unless_equals_int (data2[1], 0x40);
  fail_unless_equals_int (data2[2], 0x40);
  fail_unless_equals_int (data2[3], 0x40);
#endif
  gst_buffer_unmap (pix2, &map);

  /* unscaled premultiplied data, global-alpha (0.25) applied */
  pix2 = gst_video_overlay_rectangle_get_pixels_unscaled_raw (rect1,
      GST_VIDEO_OVERLAY_FORMAT_FLAG_PREMULTIPLIED_ALPHA);
  /* this should just return the rect/buffer, that was cached for the
   * premultiplied data */
  fail_unless (pix2 == pix4);
  /* make sure we got what we expected:
   * (0x40 / (0x80/0xFF) * (0x20/0xFF) = 0x10
   * NOTE: unless we are using round() for the premultiplied case
   * in gst_video_overlay_rectangle_apply_global_alpha() we get rounding
   * error, i.e. 0x0F here */
  gst_buffer_map (pix2, &map, GST_MAP_READ);
  data2 = map.data;
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
  /* B - G - R - A */
  fail_unless_equals_int (data2[0], 0x0F);
  fail_unless_equals_int (data2[1], 0x0F);
  fail_unless_equals_int (data2[2], 0x0F);
  fail_unless_equals_int (data2[3], 0x20);
#else
  /* A - R - G - B */
  fail_unless_equals_int (data2[0], 0x20);
  fail_unless_equals_int (data2[1], 0x0F);
  fail_unless_equals_int (data2[2], 0x0F);
  fail_unless_equals_int (data2[3], 0x0F);
#endif
  gst_buffer_unmap (pix2, &map);

  /* set global_alpha once more */
  gst_video_overlay_rectangle_set_global_alpha (rect1, 0.75);
  /* and verify that also premultiplied data is adjusted
   * correspondingly (though with increasing rounding errors) */
  pix2 = gst_video_overlay_rectangle_get_pixels_unscaled_raw (rect1,
      GST_VIDEO_OVERLAY_FORMAT_FLAG_PREMULTIPLIED_ALPHA);
  /* this should just return the rect/buffer, that was cached for the
   * premultiplied data */
  fail_unless (pix2 == pix4);
  /* make sure we got what we expected:
   * (0x0F / (0x20/0xFF) * (0x60/0xFF) = 0x2D
   * NOTE: using floats everywhere we would get 0x30
   * here we will actually end up with 0x2C */
  gst_buffer_map (pix2, &map, GST_MAP_READ);
  data2 = map.data;
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
  /* B - G - R - A */
  fail_unless_equals_int (data2[0], 0x2C);
  fail_unless_equals_int (data2[1], 0x2C);
  fail_unless_equals_int (data2[2], 0x2C);
  fail_unless_equals_int (data2[3], 0x60);
#else
  /* A - R - G - B */
  fail_unless_equals_int (data2[0], 0x60);
  fail_unless_equals_int (data2[1], 0x2C);
  fail_unless_equals_int (data2[2], 0x2C);
  fail_unless_equals_int (data2[3], 0x2C);
#endif
  gst_buffer_unmap (pix2, &map);

  /* now scaled and premultiplied data, global-alpha not applied,
   * is this really a valid use case?*/
  pix2 = gst_video_overlay_rectangle_get_pixels_raw (rect1,
      GST_VIDEO_OVERLAY_FORMAT_FLAG_PREMULTIPLIED_ALPHA |
      GST_VIDEO_OVERLAY_FORMAT_FLAG_GLOBAL_ALPHA);
  /* this should just return the rect/buffer, that was cached for the
   * first premultiplied+scaled rect*/
  fail_unless (pix2 == pix5);
  /* make sure we got what we expected */
  gst_buffer_map (pix2, &map, GST_MAP_READ);
  data2 = map.data;
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
  /* B - G - R - A */
  fail_unless_equals_int (data2[0], 0x40);
  fail_unless_equals_int (data2[1], 0x40);
  fail_unless_equals_int (data2[2], 0x40);
  fail_unless_equals_int (data2[3], 0x80);
#else
  /* A - R - G - B */
  fail_unless_equals_int (data2[0], 0x80);
  fail_unless_equals_int (data2[1], 0x40);
  fail_unless_equals_int (data2[2], 0x40);
  fail_unless_equals_int (data2[3], 0x40);
#endif
  gst_buffer_unmap (pix2, &map);

  /* scaled and premultiplied data, global-alpha applied */
  pix2 = gst_video_overlay_rectangle_get_pixels_raw (rect1,
      GST_VIDEO_OVERLAY_FORMAT_FLAG_PREMULTIPLIED_ALPHA);
  /* this should just return the rect/buffer, that was cached for the
   * first premultiplied+scaled rect*/
  fail_unless (pix2 == pix5);
  /* make sure we got what we expected; see above note about rounding errors! */
  gst_buffer_map (pix2, &map, GST_MAP_READ);
  data2 = map.data;
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
  /* B - G - R - A */
  fail_unless_equals_int (data2[0], 0x2F);
  fail_unless_equals_int (data2[1], 0x2F);
  fail_unless_equals_int (data2[2], 0x2F);
  fail_unless_equals_int (data2[3], 0x60);
#else
  /* A - R - G - B */
  fail_unless_equals_int (data2[0], 0x60);
  fail_unless_equals_int (data2[1], 0x2F);
  fail_unless_equals_int (data2[2], 0x2F);
  fail_unless_equals_int (data2[3], 0x2F);
#endif
  gst_buffer_unmap (pix2, &map);

  gst_video_overlay_rectangle_unref (rect1);
}

GST_END_TEST;

static guint8 *
make_pixels (gint depth, gint width, gint height)
{
  guint32 color = 0xff000000;
  gint i, j;

  if (depth == 8) {
    guint8 *pixels = g_malloc (width * height * 4);
    for (i = 0; i < height; i++) {
      for (j = 0; j < width; j++) {
        pixels[(i * width + j) * 4 + 0] = ((color >> 24) & 0xff);
        pixels[(i * width + j) * 4 + 1] = ((color >> 16) & 0xff);
        pixels[(i * width + j) * 4 + 2] = ((color >> 8) & 0xff);
        pixels[(i * width + j) * 4 + 3] = (color & 0xff);
        color++;
      }
    }
    return pixels;
  } else {
#define TO16(a) (((a)<<8)|(a))
    guint16 *pixels = g_malloc (width * height * 8);
    for (i = 0; i < height; i++) {
      for (j = 0; j < width; j++) {
        pixels[(i * width + j) * 4 + 0] = TO16 ((color >> 24) & 0xff);
        pixels[(i * width + j) * 4 + 1] = TO16 ((color >> 16) & 0xff);
        pixels[(i * width + j) * 4 + 2] = TO16 ((color >> 8) & 0xff);
        pixels[(i * width + j) * 4 + 3] = TO16 (color & 0xff);
        color++;
      }
    }
#undef TO16
    return (guint8 *) pixels;
  }
}

#define HS(x,o) ((x)&hs[o])
#define WS(x,o) ((x)&ws[o])
#define IN(i,j,o) (in[(HS(i, o)*width + WS(j,o))*4+(o)] & mask[o])
#define OUT(i,j,o) (out[((i)*width + (j))*4+o] & mask[o])
static gint
compare_frame (const GstVideoFormatInfo * finfo, gint depth, guint8 * outpixels,
    guint8 * pixels, gint width, gint height)
{
  gint diff, i, j, k;
  guint ws[4], hs[4], mask[4];

  for (k = 0; k < 4; k++) {
    hs[k] = G_MAXUINT << finfo->h_sub[(3 + k) % 4];
    ws[k] = G_MAXUINT << finfo->w_sub[(3 + k) % 4];
    mask[k] = G_MAXUINT << (depth - finfo->depth[(3 + k) % 4]);
  }
  diff = 0;
  if (depth == 8) {
    guint8 *in = pixels;
    guint8 *out = outpixels;

    for (i = 0; i < height; i++) {
      for (j = 0; j < width; j++) {
        for (k = 0; k < 4; k++) {
          diff += IN (i, j, k) != OUT (i, j, k);
        }
      }
    }
  } else {
    guint16 *in = (guint16 *) pixels;
    guint16 *out = (guint16 *) outpixels;

    for (i = 0; i < height; i++) {
      for (j = 0; j < width; j++) {
        for (k = 0; k < 4; k++) {
          diff += IN (i, j, k) != OUT (i, j, k);
        }
      }
    }
  }
  return diff;
}

#undef WS
#undef HS
#undef IN
#undef OUT

typedef struct
{
  GstVideoFormat infmt;
  GstVideoFormat outfmt;
  gint method;
  gdouble convert_sec;
} ConvertResult;

#define SIGN(a,b) ((a) < (b) ? -1 : (a) > (b) ? 1 : 0)

static gint
compare_result (gconstpointer a, gconstpointer b)
{
  const ConvertResult *ap = a;
  const ConvertResult *bp = b;

  return SIGN (ap->convert_sec, bp->convert_sec);
}

#define UNPACK_FRAME(frame,dest,line,x,width)            \
  (frame)->info.finfo->unpack_func ((frame)->info.finfo, \
      (GST_VIDEO_FRAME_IS_INTERLACED (frame) ?           \
        GST_VIDEO_PACK_FLAG_INTERLACED :                 \
        GST_VIDEO_PACK_FLAG_NONE),                       \
      dest, (frame)->data, (frame)->info.stride, x,      \
      line, width)
#define PACK_FRAME(frame,src,line,width)               \
  (frame)->info.finfo->pack_func ((frame)->info.finfo, \
      (GST_VIDEO_FRAME_IS_INTERLACED (frame) ?         \
        GST_VIDEO_PACK_FLAG_INTERLACED :               \
        GST_VIDEO_PACK_FLAG_NONE),                     \
      src, 0, (frame)->data, (frame)->info.stride,     \
      (frame)->info.chroma_site, line, width);

GST_START_TEST (test_video_pack_unpack2)
{
  GstVideoFormat format;
  GTimer *timer;
  gint num_formats, i;
  GArray *packarray, *unpackarray;

#define WIDTH 320
#define HEIGHT 240
/* set to something larger to do benchmarks */
#define TIME 0.01

  timer = g_timer_new ();
  packarray = g_array_new (FALSE, FALSE, sizeof (ConvertResult));
  unpackarray = g_array_new (FALSE, FALSE, sizeof (ConvertResult));

  num_formats = get_num_formats ();

  GST_DEBUG ("pack/sec\t unpack/sec \tpack GB/sec\tunpack GB/sec\tformat");

  for (format = GST_VIDEO_FORMAT_I420; format < num_formats; format++) {
    GstVideoInfo info;
    const GstVideoFormatInfo *finfo, *fuinfo;
    GstBuffer *buffer;
    GstVideoFrame frame;
    gint k, stride, count, diff, depth;
    guint8 *pixels, *outpixels;
    gdouble elapsed;
    gdouble unpack_sec, pack_sec;
    ConvertResult res;

    if (format == GST_VIDEO_FORMAT_DMA_DRM)
      continue;

    finfo = gst_video_format_get_info (format);
    fail_unless (finfo != NULL);

    if (GST_VIDEO_FORMAT_INFO_HAS_PALETTE (finfo))
      continue;

    fuinfo = gst_video_format_get_info (finfo->unpack_format);
    fail_unless (fuinfo != NULL);

    depth = GST_VIDEO_FORMAT_INFO_BITS (fuinfo);
    fail_unless (depth == 8 || depth == 16);

    pixels = make_pixels (depth, WIDTH, HEIGHT);
    stride = WIDTH * (depth >> 1);

    fail_unless (gst_video_info_set_format (&info, format, WIDTH, HEIGHT));
    buffer = gst_buffer_new_and_alloc (info.size);
    gst_video_frame_map (&frame, &info, buffer, GST_MAP_READWRITE);

    /* pack the frame into the target format */
    /* warmup */
    PACK_FRAME (&frame, pixels, 0, WIDTH);

    count = 0;
    g_timer_start (timer);
    while (TRUE) {
      for (k = 0; k < HEIGHT; k += finfo->pack_lines) {
        PACK_FRAME (&frame, pixels + k * stride, k, WIDTH);
      }
      count++;
      elapsed = g_timer_elapsed (timer, NULL);
      if (elapsed >= TIME)
        break;
    }
    unpack_sec = count / elapsed;

    res.infmt = format;
    res.outfmt = finfo->unpack_format;
    res.convert_sec = unpack_sec;
    g_array_append_val (unpackarray, res);

    outpixels = g_malloc0 (HEIGHT * stride);

    /* unpack the frame */
    /* warmup */
    UNPACK_FRAME (&frame, outpixels, 0, 0, WIDTH);

    count = 0;
    g_timer_start (timer);
    while (TRUE) {
      for (k = 0; k < HEIGHT; k += finfo->pack_lines) {
        UNPACK_FRAME (&frame, outpixels + k * stride, k, 0, WIDTH);
      }
      count++;
      elapsed = g_timer_elapsed (timer, NULL);
      if (elapsed >= TIME)
        break;
    }
    pack_sec = count / elapsed;

    res.outfmt = format;
    res.infmt = finfo->unpack_format;
    res.convert_sec = pack_sec;
    g_array_append_val (packarray, res);

    /* compare the frame */
    diff = compare_frame (finfo, depth, outpixels, pixels, WIDTH, HEIGHT);

    GST_DEBUG ("%f \t %f \t %f \t %f \t %s %d/%f", pack_sec, unpack_sec,
        info.size * pack_sec, info.size * unpack_sec, finfo->name, count,
        elapsed);

    if (diff != 0) {
      gst_util_dump_mem (outpixels, 128);
      gst_util_dump_mem (pixels, 128);
      fail_if (diff != 0);
    }
    gst_video_frame_unmap (&frame);
    gst_buffer_unref (buffer);
    g_free (pixels);
    g_free (outpixels);
  }

  g_array_sort (packarray, compare_result);
  for (i = 0; i < packarray->len; i++) {
    ConvertResult *res = &g_array_index (packarray, ConvertResult, i);

    GST_DEBUG ("%f pack/sec %s->%s", res->convert_sec,
        gst_video_format_to_string (res->infmt),
        gst_video_format_to_string (res->outfmt));
  }

  g_array_sort (unpackarray, compare_result);
  for (i = 0; i < unpackarray->len; i++) {
    ConvertResult *res = &g_array_index (unpackarray, ConvertResult, i);

    GST_DEBUG ("%f unpack/sec %s->%s", res->convert_sec,
        gst_video_format_to_string (res->infmt),
        gst_video_format_to_string (res->outfmt));
  }

  g_timer_destroy (timer);
  g_array_free (packarray, TRUE);
  g_array_free (unpackarray, TRUE);
}

GST_END_TEST;
#undef WIDTH
#undef HEIGHT
#undef TIME

#define WIDTH 320
#define HEIGHT 240
#define TIME 0.1
#define GET_LINE(l) (pixels + CLAMP (l, 0, HEIGHT-1) * WIDTH * 4)
GST_START_TEST (test_video_chroma)
{
  guint8 *pixels;
  guint n_lines;
  gint i, j, k, offset, count;
  gpointer lines[10];
  GTimer *timer;
  gdouble elapsed, subsample_sec;
  GstVideoChromaSite sites[] = {
    GST_VIDEO_CHROMA_SITE_NONE,
    GST_VIDEO_CHROMA_SITE_H_COSITED,
  };

  timer = g_timer_new ();
  pixels = make_pixels (8, WIDTH, HEIGHT);

  for (k = 0; k < G_N_ELEMENTS (sites); k++) {
    GstVideoChromaResample *resample;

    resample = gst_video_chroma_resample_new (GST_VIDEO_CHROMA_METHOD_LINEAR,
        sites[k], GST_VIDEO_CHROMA_FLAG_NONE, GST_VIDEO_FORMAT_AYUV, -1, -1);

    gst_video_chroma_resample_get_info (resample, &n_lines, &offset);
    fail_unless (n_lines < 10);

    /* warmup */
    for (j = 0; j < n_lines; j++)
      lines[j] = GET_LINE (offset + j);
    gst_video_chroma_resample (resample, lines, WIDTH);

    count = 0;
    g_timer_start (timer);
    while (TRUE) {
      for (i = 0; i < HEIGHT; i += n_lines) {
        for (j = 0; j < n_lines; j++)
          lines[j] = GET_LINE (i + offset + j);

        gst_video_chroma_resample (resample, lines, WIDTH);
      }
      count++;
      elapsed = g_timer_elapsed (timer, NULL);
      if (elapsed >= TIME)
        break;
    }
    subsample_sec = count / elapsed;
    GST_DEBUG ("%f downsamples/sec  %d/%f", subsample_sec, count, elapsed);
    gst_video_chroma_resample_free (resample);

    resample = gst_video_chroma_resample_new (GST_VIDEO_CHROMA_METHOD_LINEAR,
        sites[k], GST_VIDEO_CHROMA_FLAG_NONE, GST_VIDEO_FORMAT_AYUV, 1, 1);

    gst_video_chroma_resample_get_info (resample, &n_lines, &offset);
    fail_unless (n_lines < 10);

    /* warmup */
    for (j = 0; j < n_lines; j++)
      lines[j] = GET_LINE (offset + j);
    gst_video_chroma_resample (resample, lines, WIDTH);

    count = 0;
    g_timer_start (timer);
    while (TRUE) {
      for (i = 0; i < HEIGHT; i += n_lines) {
        for (j = 0; j < n_lines; j++)
          lines[j] = GET_LINE (i + offset + j);

        gst_video_chroma_resample (resample, lines, WIDTH);
      }
      count++;
      elapsed = g_timer_elapsed (timer, NULL);
      if (elapsed >= TIME)
        break;
    }
    subsample_sec = count / elapsed;
    GST_DEBUG ("%f upsamples/sec  %d/%f", subsample_sec, count, elapsed);
    gst_video_chroma_resample_free (resample);
  }

  g_free (pixels);
  g_timer_destroy (timer);
}

GST_END_TEST;
#undef WIDTH
#undef HEIGHT
#undef TIME

typedef struct
{
  const gchar *name;
  GstVideoChromaSite site;
} ChromaSiteElem;

GST_START_TEST (test_video_chroma_site)
{
  ChromaSiteElem valid_sites[] = {
    /* pre-defined flags */
    {"jpeg", GST_VIDEO_CHROMA_SITE_JPEG},
    {"mpeg2", GST_VIDEO_CHROMA_SITE_MPEG2},
    {"dv", GST_VIDEO_CHROMA_SITE_DV},
    {"alt-line", GST_VIDEO_CHROMA_SITE_ALT_LINE},
    {"cosited", GST_VIDEO_CHROMA_SITE_COSITED},
    /* new values */
    {"v-cosited", GST_VIDEO_CHROMA_SITE_V_COSITED},
    {"v-cosited+alt-line",
        GST_VIDEO_CHROMA_SITE_V_COSITED | GST_VIDEO_CHROMA_SITE_ALT_LINE},
  };
  ChromaSiteElem unknown_sites[] = {
    {NULL, GST_VIDEO_CHROMA_SITE_UNKNOWN},
    /* Any combination with GST_VIDEO_CHROMA_SITE_NONE doesn' make sense */
    {NULL, GST_VIDEO_CHROMA_SITE_NONE | GST_VIDEO_CHROMA_SITE_H_COSITED},
  };
  gint i;

  for (i = 0; i < G_N_ELEMENTS (valid_sites); i++) {
    gchar *site = gst_video_chroma_site_to_string (valid_sites[i].site);

    fail_unless (site != NULL);
    fail_unless (g_strcmp0 (site, valid_sites[i].name) == 0);
    fail_unless (gst_video_chroma_site_from_string (site) ==
        valid_sites[i].site);
    g_free (site);
  }

  for (i = 0; i < G_N_ELEMENTS (unknown_sites); i++) {
    gchar *site = gst_video_chroma_site_to_string (unknown_sites[i].site);
    fail_unless (site == NULL);
  }

  /* totally wrong string */
  fail_unless (gst_video_chroma_site_from_string ("foo/bar") ==
      GST_VIDEO_CHROMA_SITE_UNKNOWN);

  /* valid ones */
  fail_unless (gst_video_chroma_site_from_string ("jpeg") ==
      GST_VIDEO_CHROMA_SITE_NONE);
  fail_unless (gst_video_chroma_site_from_string ("none") ==
      GST_VIDEO_CHROMA_SITE_NONE);

  fail_unless (gst_video_chroma_site_from_string ("mpeg2") ==
      GST_VIDEO_CHROMA_SITE_H_COSITED);
  fail_unless (gst_video_chroma_site_from_string ("h-cosited") ==
      GST_VIDEO_CHROMA_SITE_H_COSITED);

  /* Equal to "cosited" */
  fail_unless (gst_video_chroma_site_from_string ("v-cosited+h-cosited") ==
      GST_VIDEO_CHROMA_SITE_COSITED);

  fail_unless (gst_video_chroma_site_from_string ("v-cosited") ==
      GST_VIDEO_CHROMA_SITE_V_COSITED);

  /* none + something doesn't make sense */
  fail_unless (gst_video_chroma_site_from_string ("none+v-cosited") ==
      GST_VIDEO_CHROMA_SITE_UNKNOWN);

  /* mix of valid and invalid strings */
  fail_unless (gst_video_chroma_site_from_string ("mpeg2+foo/bar") ==
      GST_VIDEO_CHROMA_SITE_UNKNOWN);
}

GST_END_TEST;

GST_START_TEST (test_video_scaler)
{
  GstVideoScaler *scale;

  scale = gst_video_scaler_new (GST_VIDEO_RESAMPLER_METHOD_LINEAR,
      GST_VIDEO_SCALER_FLAG_NONE, 2, 10, 5, NULL);
  gst_video_scaler_free (scale);

  scale = gst_video_scaler_new (GST_VIDEO_RESAMPLER_METHOD_LINEAR,
      GST_VIDEO_SCALER_FLAG_NONE, 2, 15, 5, NULL);
  gst_video_scaler_free (scale);
}

GST_END_TEST;

typedef enum
{
  RGB,
  YUV,
  OTHER
} ColorType;

#define WIDTH 320
#define HEIGHT 240

static gboolean
check_video_format_is_type (GstVideoFormat fmt, ColorType fmt_type)
{
  const GstVideoFormatInfo *info = gst_video_format_get_info (fmt);
  gboolean is_rgb = GST_VIDEO_FORMAT_INFO_IS_RGB (info);
  gboolean is_yuv = GST_VIDEO_FORMAT_INFO_IS_YUV (info);

  switch (fmt_type) {
    case RGB:
      return is_rgb;
    case YUV:
      return is_yuv;
    case OTHER:
      break;
  }
  return !is_rgb && !is_yuv;
}

static void
run_video_color_convert (ColorType in_type, ColorType out_type)
{
  GstVideoFormat infmt, outfmt;
  gint num_formats;

  num_formats = get_num_formats ();

  for (infmt = GST_VIDEO_FORMAT_I420; infmt < num_formats; infmt++) {
    GstVideoInfo ininfo;
    GstVideoFrame inframe;
    GstBuffer *inbuffer;

    if (infmt == GST_VIDEO_FORMAT_DMA_DRM)
      continue;

    if (!check_video_format_is_type (infmt, in_type))
      continue;

    fail_unless (gst_video_info_set_format (&ininfo, infmt, WIDTH, HEIGHT));
    inbuffer = gst_buffer_new_and_alloc (ininfo.size);
    gst_buffer_memset (inbuffer, 0, 0, -1);
    gst_video_frame_map (&inframe, &ininfo, inbuffer, GST_MAP_READ);

    for (outfmt = GST_VIDEO_FORMAT_I420; outfmt < num_formats; outfmt++) {
      GstVideoInfo outinfo;
      GstVideoFrame outframe;
      GstBuffer *outbuffer;
      GstVideoConverter *convert;

      if (outfmt == GST_VIDEO_FORMAT_DMA_DRM)
        continue;

      if (!check_video_format_is_type (outfmt, out_type))
        continue;

      GST_LOG ("%s -> %s @ %ux%u", gst_video_format_to_string (infmt),
          gst_video_format_to_string (outfmt), WIDTH, HEIGHT);

      fail_unless (gst_video_info_set_format (&outinfo, outfmt, WIDTH, HEIGHT));
      outbuffer = gst_buffer_new_and_alloc (outinfo.size);
      gst_video_frame_map (&outframe, &outinfo, outbuffer, GST_MAP_WRITE);

      convert = gst_video_converter_new (&ininfo, &outinfo, NULL);

      gst_video_converter_frame (convert, &inframe, &outframe);

      gst_video_converter_free (convert);

      gst_video_frame_unmap (&outframe);
      gst_buffer_unref (outbuffer);
    }
    gst_video_frame_unmap (&inframe);
    gst_buffer_unref (inbuffer);
  }
}

GST_START_TEST (test_video_color_convert_rgb_rgb)
{
  run_video_color_convert (RGB, RGB);
}

GST_END_TEST;

GST_START_TEST (test_video_color_convert_rgb_yuv)
{
  run_video_color_convert (RGB, YUV);
}

GST_END_TEST;

GST_START_TEST (test_video_color_convert_yuv_yuv)
{
  run_video_color_convert (YUV, YUV);
}

GST_END_TEST;

GST_START_TEST (test_video_color_convert_yuv_rgb)
{
  run_video_color_convert (YUV, RGB);
}

GST_END_TEST;

GST_START_TEST (test_video_color_convert_other)
{
  run_video_color_convert (OTHER, RGB);
  run_video_color_convert (RGB, OTHER);
  run_video_color_convert (OTHER, YUV);
  run_video_color_convert (YUV, OTHER);
  run_video_color_convert (OTHER, OTHER);
}

GST_END_TEST;
#undef WIDTH
#undef HEIGHT

#define WIDTH_IN 320
#define HEIGHT_IN 240
#define WIDTH_OUT 400
#define HEIGHT_OUT 300
#define TIME 0.01

GST_START_TEST (test_video_size_convert)
{
  GstVideoFormat infmt, outfmt;
  GTimer *timer;
  gint num_formats, i;
  GArray *array;

  array = g_array_new (FALSE, FALSE, sizeof (ConvertResult));

  timer = g_timer_new ();

  num_formats = get_num_formats ();

  for (infmt = GST_VIDEO_FORMAT_I420; infmt < num_formats; infmt++) {
    GstVideoInfo ininfo, outinfo;
    GstVideoFrame inframe, outframe;
    GstBuffer *inbuffer, *outbuffer;
    GstVideoConverter *convert;
    gdouble elapsed;
    gint count, method;
    ConvertResult res;

    if (infmt == GST_VIDEO_FORMAT_DMA_DRM)
      continue;

    fail_unless (gst_video_info_set_format (&ininfo, infmt, WIDTH_IN,
            HEIGHT_IN));
    inbuffer = gst_buffer_new_and_alloc (ininfo.size);
    gst_buffer_memset (inbuffer, 0, 0, -1);
    gst_video_frame_map (&inframe, &ininfo, inbuffer, GST_MAP_READ);

    outfmt = infmt;
    fail_unless (gst_video_info_set_format (&outinfo, outfmt, WIDTH_OUT,
            HEIGHT_OUT));
    outbuffer = gst_buffer_new_and_alloc (outinfo.size);
    gst_video_frame_map (&outframe, &outinfo, outbuffer, GST_MAP_WRITE);

    for (method = 0; method < 4; method++) {
      convert = gst_video_converter_new (&ininfo, &outinfo,
          gst_structure_new ("options",
              GST_VIDEO_CONVERTER_OPT_RESAMPLER_METHOD,
              GST_TYPE_VIDEO_RESAMPLER_METHOD, method, NULL));

      /* warmup */
      gst_video_converter_frame (convert, &inframe, &outframe);

      count = 0;
      g_timer_start (timer);
      while (TRUE) {
        gst_video_converter_frame (convert, &inframe, &outframe);

        count++;
        elapsed = g_timer_elapsed (timer, NULL);
        if (elapsed >= TIME)
          break;
      }

      res.infmt = infmt;
      res.outfmt = outfmt;
      res.method = method;
      res.convert_sec = count / elapsed;

      GST_DEBUG ("%f resize/sec %s->%s, %d, %d/%f", res.convert_sec,
          gst_video_format_to_string (infmt),
          gst_video_format_to_string (outfmt), method, count, elapsed);

      g_array_append_val (array, res);

      gst_video_converter_free (convert);
    }
    gst_video_frame_unmap (&outframe);
    gst_buffer_unref (outbuffer);
    gst_video_frame_unmap (&inframe);
    gst_buffer_unref (inbuffer);
  }

  g_array_sort (array, compare_result);

  for (i = 0; i < array->len; i++) {
    ConvertResult *res = &g_array_index (array, ConvertResult, i);

    GST_DEBUG ("%f method %d, resize/sec %s->%s", res->convert_sec, res->method,
        gst_video_format_to_string (res->infmt),
        gst_video_format_to_string (res->outfmt));
  }

  g_array_free (array, TRUE);

  g_timer_destroy (timer);
}

GST_END_TEST;
#undef WIDTH
#undef HEIGHT

GST_START_TEST (test_video_convert)
{
  GstVideoInfo ininfo, outinfo;
  GstVideoFrame inframe, outframe;
  GstBuffer *inbuffer, *outbuffer;
  GstVideoConverter *convert;

  fail_unless (gst_video_info_set_format (&ininfo, GST_VIDEO_FORMAT_ARGB, 320,
          240));
  inbuffer = gst_buffer_new_and_alloc (ininfo.size);
  gst_buffer_memset (inbuffer, 0, 0, -1);
  gst_video_frame_map (&inframe, &ininfo, inbuffer, GST_MAP_READ);

  fail_unless (gst_video_info_set_format (&outinfo, GST_VIDEO_FORMAT_BGRx, 400,
          300));
  outbuffer = gst_buffer_new_and_alloc (outinfo.size);
  gst_video_frame_map (&outframe, &outinfo, outbuffer, GST_MAP_WRITE);

  /* see that we don't reuse the source line directly because we need
   * to add borders to it */
  convert = gst_video_converter_new (&ininfo, &outinfo,
      gst_structure_new ("options",
          GST_VIDEO_CONVERTER_OPT_RESAMPLER_METHOD,
          GST_TYPE_VIDEO_RESAMPLER_METHOD, 3,
          GST_VIDEO_CONVERTER_OPT_SRC_X, G_TYPE_INT, 10,
          GST_VIDEO_CONVERTER_OPT_SRC_Y, G_TYPE_INT, 0,
          GST_VIDEO_CONVERTER_OPT_SRC_WIDTH, G_TYPE_INT, 300,
          GST_VIDEO_CONVERTER_OPT_SRC_HEIGHT, G_TYPE_INT, 220,
          GST_VIDEO_CONVERTER_OPT_DEST_X, G_TYPE_INT, 80,
          GST_VIDEO_CONVERTER_OPT_DEST_Y, G_TYPE_INT, 60,
          GST_VIDEO_CONVERTER_OPT_DEST_WIDTH, G_TYPE_INT, 300,
          GST_VIDEO_CONVERTER_OPT_DEST_HEIGHT, G_TYPE_INT, 220, NULL));

  g_assert (gst_video_info_is_equal (&ininfo,
          gst_video_converter_get_in_info (convert)));
  g_assert (gst_video_info_is_equal (&outinfo,
          gst_video_converter_get_out_info (convert)));

  gst_video_converter_frame (convert, &inframe, &outframe);
  gst_video_converter_free (convert);

  /* see that we reuse the source line directly because we need to scale
   * it first */
  convert = gst_video_converter_new (&ininfo, &outinfo,
      gst_structure_new ("options",
          GST_VIDEO_CONVERTER_OPT_RESAMPLER_METHOD,
          GST_TYPE_VIDEO_RESAMPLER_METHOD, 3,
          GST_VIDEO_CONVERTER_OPT_SRC_X, G_TYPE_INT, 10,
          GST_VIDEO_CONVERTER_OPT_SRC_Y, G_TYPE_INT, 0,
          GST_VIDEO_CONVERTER_OPT_SRC_WIDTH, G_TYPE_INT, 300,
          GST_VIDEO_CONVERTER_OPT_SRC_HEIGHT, G_TYPE_INT, 220,
          GST_VIDEO_CONVERTER_OPT_DEST_X, G_TYPE_INT, 80,
          GST_VIDEO_CONVERTER_OPT_DEST_Y, G_TYPE_INT, 60,
          GST_VIDEO_CONVERTER_OPT_DEST_WIDTH, G_TYPE_INT, 310,
          GST_VIDEO_CONVERTER_OPT_DEST_HEIGHT, G_TYPE_INT, 230, NULL));

  gst_video_converter_frame (convert, &inframe, &outframe);

  /* Check that video convert doesn't crash if we give it frames with different info
   * than we configured it with by swapping width/height */
  gst_video_frame_unmap (&inframe);
  fail_unless (gst_video_info_set_format (&ininfo, GST_VIDEO_FORMAT_ARGB, 240,
          320));
  gst_video_frame_map (&inframe, &ininfo, inbuffer, GST_MAP_READ);
  ASSERT_CRITICAL (gst_video_converter_frame (convert, &inframe, &outframe));
  gst_video_converter_free (convert);

  /* Make sure we can crop the entire frame away without dying */
  convert = gst_video_converter_new (&ininfo, &outinfo,
      gst_structure_new ("options",
          GST_VIDEO_CONVERTER_OPT_RESAMPLER_METHOD,
          GST_TYPE_VIDEO_RESAMPLER_METHOD, 3,
          GST_VIDEO_CONVERTER_OPT_SRC_X, G_TYPE_INT, -500,
          GST_VIDEO_CONVERTER_OPT_SRC_Y, G_TYPE_INT, -500,
          GST_VIDEO_CONVERTER_OPT_SRC_WIDTH, G_TYPE_INT, 300,
          GST_VIDEO_CONVERTER_OPT_SRC_HEIGHT, G_TYPE_INT, 220,
          GST_VIDEO_CONVERTER_OPT_DEST_X, G_TYPE_INT, 800,
          GST_VIDEO_CONVERTER_OPT_DEST_Y, G_TYPE_INT, 600,
          GST_VIDEO_CONVERTER_OPT_DEST_WIDTH, G_TYPE_INT, 310,
          GST_VIDEO_CONVERTER_OPT_DEST_HEIGHT, G_TYPE_INT, 230, NULL));

  gst_video_converter_frame (convert, &inframe, &outframe);
  gst_video_converter_free (convert);

  gst_video_frame_unmap (&outframe);
  gst_buffer_unref (outbuffer);
  gst_video_frame_unmap (&inframe);
  gst_buffer_unref (inbuffer);

}

GST_END_TEST;

GST_START_TEST (test_video_convert_multithreading)
{
  GstVideoInfo ininfo, outinfo;
  GstVideoFrame inframe, outframe, refframe;
  GstBuffer *inbuffer, *outbuffer, *refbuffer;
  GstVideoConverter *convert;
  GstMapInfo info;
  GstTaskPool *pool;

  /* Large enough input resolution for video-converter to actually use
   * 4 threads if required */
  fail_unless (gst_video_info_set_format (&ininfo, GST_VIDEO_FORMAT_ARGB, 1280,
          720));
  inbuffer = gst_buffer_new_and_alloc (ininfo.size);
  gst_buffer_memset (inbuffer, 0, 0, -1);
  gst_video_frame_map (&inframe, &ininfo, inbuffer, GST_MAP_READ);

  fail_unless (gst_video_info_set_format (&outinfo, GST_VIDEO_FORMAT_BGRx, 400,
          300));
  outbuffer = gst_buffer_new_and_alloc (outinfo.size);
  refbuffer = gst_buffer_new_and_alloc (outinfo.size);

  gst_video_frame_map (&outframe, &outinfo, outbuffer, GST_MAP_WRITE);
  gst_video_frame_map (&refframe, &outinfo, refbuffer, GST_MAP_WRITE);

  /* Single threaded-conversion */
  convert = gst_video_converter_new (&ininfo, &outinfo,
      gst_structure_new_empty ("options"));
  gst_video_converter_frame (convert, &inframe, &refframe);
  gst_video_converter_free (convert);

  /* Multithreaded conversion, converter creates pool */
  convert = gst_video_converter_new (&ininfo, &outinfo,
      gst_structure_new ("options",
          GST_VIDEO_CONVERTER_OPT_THREADS, G_TYPE_UINT, 4, NULL)
      );
  gst_video_converter_frame (convert, &inframe, &outframe);
  gst_video_converter_free (convert);

  gst_video_frame_unmap (&outframe);
  gst_video_frame_unmap (&refframe);

  gst_buffer_map (outbuffer, &info, GST_MAP_READ);
  fail_unless (gst_buffer_memcmp (refbuffer, 0, info.data, info.size) == 0);
  gst_buffer_unmap (outbuffer, &info);

  gst_video_frame_map (&outframe, &outinfo, outbuffer, GST_MAP_WRITE);
  gst_video_frame_map (&refframe, &outinfo, refbuffer, GST_MAP_WRITE);

  /* Multi-threaded conversion, user-provided pool */
  pool = gst_shared_task_pool_new ();
  gst_shared_task_pool_set_max_threads (GST_SHARED_TASK_POOL (pool), 4);
  gst_task_pool_prepare (pool, NULL);
  convert = gst_video_converter_new_with_pool (&ininfo, &outinfo,
      gst_structure_new ("options",
          GST_VIDEO_CONVERTER_OPT_THREADS, G_TYPE_UINT, 4, NULL), pool);
  gst_video_converter_frame (convert, &inframe, &outframe);
  gst_video_converter_free (convert);
  gst_task_pool_cleanup (pool);
  gst_object_unref (pool);

  gst_video_frame_unmap (&outframe);
  gst_video_frame_unmap (&refframe);

  gst_buffer_map (outbuffer, &info, GST_MAP_READ);
  fail_unless (gst_buffer_memcmp (refbuffer, 0, info.data, info.size) == 0);
  gst_buffer_unmap (outbuffer, &info);


  gst_buffer_unref (refbuffer);
  gst_buffer_unref (outbuffer);
  gst_video_frame_unmap (&inframe);
  gst_buffer_unref (inbuffer);

}

GST_END_TEST;

GST_START_TEST (test_video_transfer)
{
  gint i, j;

  for (j = GST_VIDEO_TRANSFER_GAMMA10; j <= GST_VIDEO_TRANSFER_ARIB_STD_B67;
      j++) {
    for (i = 0; i < 256; i++) {
      gdouble val1, val2;

      val1 = gst_video_transfer_function_encode (j, i / 255.0);
      fail_if (val1 < 0.0 || val1 > 1.0);

      val2 = gst_video_transfer_function_decode (j, val1);
      fail_if (val2 < 0.0 || val2 > 1.0);

      GST_DEBUG ("%d: %d %f->%f->%f %d", j, i, i / 255.0, val1, val2,
          (int) lrint (val2 * 255.0));
      if (val1 == 0.0)
        fail_if (val2 != 0.0);
      else
        fail_if (lrint (val2 * 255.0) != i);
    }
  }
}

GST_END_TEST;

GST_START_TEST (test_video_center_rect)
{
  GstVideoRectangle src, dest, result, expected;

#define NEW_RECT(x,y,w,h) ((GstVideoRectangle) {x,y,w,h})
#define CHECK_RECT(res, exp)			\
  fail_unless_equals_int(exp.x, res.x);\
  fail_unless_equals_int(exp.y, res.y);\
  fail_unless_equals_int(exp.w, res.w);\
  fail_unless_equals_int(exp.h, res.h);

  /* 1:1 Aspect Ratio */
  src = NEW_RECT (0, 0, 100, 100);
  dest = NEW_RECT (0, 0, 100, 100);
  expected = NEW_RECT (0, 0, 100, 100);
  gst_video_sink_center_rect (src, dest, &result, TRUE);
  CHECK_RECT (result, expected);

  src = NEW_RECT (0, 0, 100, 100);
  dest = NEW_RECT (0, 0, 50, 50);
  expected = NEW_RECT (0, 0, 50, 50);
  gst_video_sink_center_rect (src, dest, &result, TRUE);
  CHECK_RECT (result, expected);

  src = NEW_RECT (0, 0, 100, 100);
  dest = NEW_RECT (50, 50, 100, 100);
  expected = NEW_RECT (50, 50, 100, 100);
  gst_video_sink_center_rect (src, dest, &result, TRUE);
  CHECK_RECT (result, expected);

  /* Aspect ratio scaling (tall) */
  src = NEW_RECT (0, 0, 50, 100);
  dest = NEW_RECT (0, 0, 50, 50);
  expected = NEW_RECT (12, 0, 25, 50);
  gst_video_sink_center_rect (src, dest, &result, TRUE);
  CHECK_RECT (result, expected);

  src = NEW_RECT (0, 0, 50, 100);
  dest = NEW_RECT (50, 50, 50, 50);
  expected = NEW_RECT (62, 50, 25, 50);
  gst_video_sink_center_rect (src, dest, &result, TRUE);
  CHECK_RECT (result, expected);

  /* Aspect ratio scaling (wide) */
  src = NEW_RECT (0, 0, 100, 50);
  dest = NEW_RECT (0, 0, 50, 50);
  expected = NEW_RECT (0, 12, 50, 25);
  gst_video_sink_center_rect (src, dest, &result, TRUE);
  CHECK_RECT (result, expected);

  src = NEW_RECT (0, 0, 100, 50);
  dest = NEW_RECT (50, 50, 50, 50);
  expected = NEW_RECT (50, 62, 50, 25);
  gst_video_sink_center_rect (src, dest, &result, TRUE);
  CHECK_RECT (result, expected);
}

GST_END_TEST;

void test_overlay_blend_rect (gint x, gint y, gint width, gint height,
    GstVideoFrame * video_frame);
void test_overlay_blend_rect_verify (gint x, gint y, gint width,
    gint height, GstVideoFrame * video_frame);
#define VIDEO_WIDTH 320
#define VIDEO_HEIGHT 240

void
test_overlay_blend_rect_verify (gint x, gint y, gint width, gint height,
    GstVideoFrame * video_frame)
{
  guint8 *data;
  gint i = 0, prev_i = 0;
  gint size = 0;
  gint temp_width = 0, temp_height = 0;

  data = GST_VIDEO_FRAME_PLANE_DATA (video_frame, 0);
  size = GST_VIDEO_FRAME_SIZE (video_frame);

  if (x + width < 0 || y + height < 0 || x >= VIDEO_WIDTH || y >= VIDEO_HEIGHT)
    return;
  if (x <= 0)
    temp_width = width + x;
  else if (x > 0 && (x + width) <= VIDEO_WIDTH)
    temp_width = width;
  else
    temp_width = VIDEO_WIDTH - x;
  if (y <= 0)
    temp_height = height + y;
  else if (y > 0 && (y + height) <= VIDEO_HEIGHT)
    temp_height = height;
  else
    temp_height = VIDEO_HEIGHT - y;

  if (x <= 0 && y <= 0)
    i = 0;
  else
    i = (((x <= 0) ? 0 : x) + (((y <= 0) ? 0 : y) * VIDEO_WIDTH)) * 4;
  prev_i = i;

  for (; i < size - 4; i += 4) {
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
    /* B - G - R - A */
    fail_unless_equals_int (data[i], 0x80);
    fail_unless_equals_int (data[i + 1], 0x80);
    fail_unless_equals_int (data[i + 2], 0x80);
    fail_unless_equals_int (data[i + 3], 0x80);
#else
    /* A - R - G - B */
    fail_unless_equals_int (data[i], 0x80);
    fail_unless_equals_int (data[i + 1], 0x80);
    fail_unless_equals_int (data[i + 2], 0x80);
    fail_unless_equals_int (data[i + 3], 0x80);
#endif
    if ((i + 4) == (4 * (((((y > 0) ? (y + temp_height) : temp_height) -
                        1) * VIDEO_WIDTH) + ((x >
                        0) ? (x + temp_width) : temp_width))))
      break;
    if ((i + 4 - prev_i) == ((temp_width) * 4)) {
      i += ((VIDEO_WIDTH - (temp_width)) * 4);
      prev_i = i + 4;
    }

  }
}

void
test_overlay_blend_rect (gint x, gint y, gint width, gint height,
    GstVideoFrame * video_frame)
{
  GstVideoOverlayComposition *comp1;
  GstVideoOverlayRectangle *rect1;
  GstBuffer *pix, *pix1;
  GstVideoInfo vinfo;

  memset (video_frame, 0, sizeof (GstVideoFrame));
  pix =
      gst_buffer_new_and_alloc (VIDEO_WIDTH * VIDEO_HEIGHT * sizeof (guint32));
  gst_buffer_memset (pix, 0, 0, gst_buffer_get_size (pix));
  gst_video_info_init (&vinfo);
  fail_unless (gst_video_info_set_format (&vinfo,
          GST_VIDEO_OVERLAY_COMPOSITION_FORMAT_RGB, VIDEO_WIDTH, VIDEO_HEIGHT));
  gst_video_frame_map (video_frame, &vinfo, pix, GST_MAP_READWRITE);
  gst_buffer_unref (pix);
  pix = NULL;

  pix1 = gst_buffer_new_and_alloc (width * height * sizeof (guint32));
  gst_buffer_memset (pix1, 0, 0x80, gst_buffer_get_size (pix1));
  gst_buffer_add_video_meta (pix1, GST_VIDEO_FRAME_FLAG_NONE,
      GST_VIDEO_OVERLAY_COMPOSITION_FORMAT_RGB, width, height);
  rect1 = gst_video_overlay_rectangle_new_raw (pix1,
      x, y, width, height, GST_VIDEO_OVERLAY_FORMAT_FLAG_NONE);
  gst_buffer_unref (pix1);
  pix1 = NULL;

  comp1 = gst_video_overlay_composition_new (rect1);
  fail_unless (gst_video_overlay_composition_blend (comp1, video_frame));
  gst_video_overlay_composition_unref (comp1);
  gst_video_overlay_rectangle_unref (rect1);

  test_overlay_blend_rect_verify (x, y, width, height, video_frame);
  gst_video_frame_unmap (video_frame);
}

GST_START_TEST (test_overlay_blend)
{
  GstVideoFrame video_frame;

  /* Overlay width & height smaller than video width & height */
  /* Overlay rendered completely left of video surface
   * x + overlay_width <= 0 */
  test_overlay_blend_rect (-60, 50, 50, 50, &video_frame);
  /* Overlay rendered completely right of video surface
   * x >= video_width */
  test_overlay_blend_rect (330, 50, 50, 50, &video_frame);
  /* Overlay rendered completely top of video surface
   * y + overlay_height <= 0 */
  test_overlay_blend_rect (50, -60, 50, 50, &video_frame);
  /* Overlay rendered completely bottom of video surface
   * y >= video_height */
  test_overlay_blend_rect (50, 250, 50, 50, &video_frame);
  /* Overlay rendered partially left of video surface
   * x < 0 && -x < overlay_width */
  test_overlay_blend_rect (-40, 50, 50, 50, &video_frame);
  /* Overlay rendered partially right of video surface
   * x < video_width && (overlay_width + x) > video_width */
  test_overlay_blend_rect (300, 50, 50, 50, &video_frame);
  /* Overlay rendered partially top of video surface
   * y < 0 && -y < overlay_height */
  test_overlay_blend_rect (50, -40, 50, 50, &video_frame);
  /* Overlay rendered partially bottom of video surface
   * y < video_height && (overlay_height + y) > video_height */
  test_overlay_blend_rect (50, 220, 50, 50, &video_frame);

  /* Overlay width & height bigger than video width & height */
  /* Overlay rendered completely left of video surface
   * x + overlay_width <= 0 */
  test_overlay_blend_rect (-360, 50, 350, 250, &video_frame);
  /* Overlay rendered completely right of video surface
   * x >= video_width */
  test_overlay_blend_rect (330, 50, 350, 250, &video_frame);
  /* Overlay rendered completely top of video surface
   * y + overlay_height <= 0 */
  test_overlay_blend_rect (50, -260, 350, 250, &video_frame);
  /* Overlay rendered completely bottom of video surface
   * y >= video_height */
  test_overlay_blend_rect (50, 250, 350, 250, &video_frame);
  /* Overlay rendered partially left of video surface
   * x < 0 && -x < overlay_width */
  test_overlay_blend_rect (-40, 50, 350, 250, &video_frame);
  /* Overlay rendered partially right of video surface
   * x < video_width && (overlay_width + x) > video_width */
  test_overlay_blend_rect (300, 50, 350, 250, &video_frame);
  /* Overlay rendered partially top of video surface
   * y < 0 && -y < overlay_height */
  test_overlay_blend_rect (50, -40, 350, 250, &video_frame);
  /* Overlay rendered partially bottom of video surface
   * y < video_height && (overlay_height + y) > video_height */
  test_overlay_blend_rect (50, 220, 350, 250, &video_frame);
}

GST_END_TEST;

GST_START_TEST (test_overlay_composition_over_transparency)
{
  GstVideoOverlayComposition *comp1;
  GstVideoOverlayRectangle *rect1;
  GstBuffer *pix1, *pix2;
  GstVideoInfo vinfo;
  guint8 *data;

  GstVideoFrame video_frame;
  guint fwidth = 200, height = 50, swidth = 100;

  memset (&video_frame, 0, sizeof (GstVideoFrame));

  pix1 = gst_buffer_new_and_alloc (fwidth * sizeof (guint32) * height);
  gst_buffer_memset (pix1, 0, 0x00, gst_buffer_get_size (pix1));
  gst_video_info_init (&vinfo);
  fail_unless (gst_video_info_set_format (&vinfo,
          GST_VIDEO_OVERLAY_COMPOSITION_FORMAT_RGB, fwidth, height));
  gst_video_frame_map (&video_frame, &vinfo, pix1, GST_MAP_READWRITE);
  gst_buffer_unref (pix1);

  pix2 = gst_buffer_new_and_alloc (swidth * sizeof (guint32) * height);
  gst_buffer_memset (pix2, 0, 0xFF, gst_buffer_get_size (pix2));
  gst_buffer_add_video_meta (pix2, GST_VIDEO_FRAME_FLAG_NONE,
      GST_VIDEO_OVERLAY_COMPOSITION_FORMAT_RGB, swidth, height);
  rect1 = gst_video_overlay_rectangle_new_raw (pix2, swidth, 0,
      swidth, height, GST_VIDEO_OVERLAY_FORMAT_FLAG_NONE);

  comp1 = gst_video_overlay_composition_new (rect1);
  fail_unless (gst_video_overlay_composition_blend (comp1, &video_frame));
  gst_video_overlay_composition_unref (comp1);
  gst_video_overlay_rectangle_unref (rect1);
  gst_buffer_unref (pix2);

  data = GST_VIDEO_FRAME_PLANE_DATA (&video_frame, 0);

  fail_unless_equals_int (data[0], 0x00);
  fail_unless_equals_int (data[1], 0x00);
  fail_unless_equals_int (data[2], 0x00);
  fail_unless_equals_int (data[3], 0x00);

  data += swidth * sizeof (guint32);

  fail_unless_equals_int (data[0], 0xFF);
  fail_unless_equals_int (data[1], 0xFF);
  fail_unless_equals_int (data[2], 0xFF);
  fail_unless_equals_int (data[3], 0xFF);

  gst_video_frame_unmap (&video_frame);
}

GST_END_TEST;

GST_START_TEST (test_video_format_enum_stability)
{
  /* When adding new formats, adding a format in the middle of the enum will
   * break the API. This check picks the last known format and checks that
   * it's value isn't changing. This test should ideall be updated when a new
   * format is added, though will stay valid. */
  fail_unless_equals_int (GST_VIDEO_FORMAT_Y210, 82);
}

GST_END_TEST;

GST_START_TEST (test_video_formats_pstrides)
{
  GstVideoFormat fmt = GST_VIDEO_FORMAT_I420;


  while ((gst_video_format_to_string (fmt) != NULL)) {
    const GstVideoFormatInfo *vf_info = gst_video_format_get_info (fmt);
    guint n_comps = GST_VIDEO_FORMAT_INFO_N_COMPONENTS (vf_info);

    GST_LOG ("format: %s (%d), n_comps = %u", vf_info->name, fmt, n_comps);

    if (fmt == GST_VIDEO_FORMAT_v210
        || fmt == GST_VIDEO_FORMAT_UYVP
        || fmt == GST_VIDEO_FORMAT_IYU1
        || fmt == GST_VIDEO_FORMAT_GRAY10_LE32
        || fmt == GST_VIDEO_FORMAT_NV12_64Z32
        || fmt == GST_VIDEO_FORMAT_NV12_4L4
        || fmt == GST_VIDEO_FORMAT_NV12_32L32
        || fmt == GST_VIDEO_FORMAT_NV12_16L32S
        || fmt == GST_VIDEO_FORMAT_NV12_10LE32
        || fmt == GST_VIDEO_FORMAT_NV16_10LE32
        || fmt == GST_VIDEO_FORMAT_NV12_10LE40
        || fmt == GST_VIDEO_FORMAT_Y410
        || fmt == GST_VIDEO_FORMAT_NV12_8L128
        || fmt == GST_VIDEO_FORMAT_NV12_10BE_8L128
        || fmt == GST_VIDEO_FORMAT_NV12_10LE40_4L4
        || fmt == GST_VIDEO_FORMAT_DMA_DRM
        || fmt == GST_VIDEO_FORMAT_MT2110T || fmt == GST_VIDEO_FORMAT_MT2110R) {
      fmt++;
      continue;
    }

    switch (n_comps) {
      case 4:
        fail_unless (GST_VIDEO_FORMAT_INFO_PSTRIDE (vf_info, 3) > 0);
        /* fall through */
      case 3:
        fail_unless (GST_VIDEO_FORMAT_INFO_PSTRIDE (vf_info, 2) > 0);
        /* fall through */
      case 2:
        fail_unless (GST_VIDEO_FORMAT_INFO_PSTRIDE (vf_info, 1) > 0);
        /* fall through */
      case 1:
        fail_unless (GST_VIDEO_FORMAT_INFO_PSTRIDE (vf_info, 0) > 0);
        break;
    }

    fmt++;
  }
}

GST_END_TEST;

GST_START_TEST (test_hdr)
{
  GstCaps *caps;
  GstCaps *other_caps;
  GstVideoMasteringDisplayInfo minfo;
  GstVideoMasteringDisplayInfo other_minfo;
  GstVideoMasteringDisplayInfo minfo_from_caps;
  GstVideoContentLightLevel level;
  GstVideoContentLightLevel other_level;
  GstVideoContentLightLevel level_from_caps;
  GstStructure *s = NULL;
  gchar *minfo_str;
  gchar *level_str = NULL;
  gint i;
  guint val;


  gst_video_mastering_display_info_init (&minfo);
  gst_video_mastering_display_info_init (&other_minfo);

  /* Test GstVideoMasteringDisplayInfo, initialize with random values
   * just for comparison */
  val = 1;
  for (i = 0; i < G_N_ELEMENTS (minfo.display_primaries); i++) {
    minfo.display_primaries[i].x = val++;
    minfo.display_primaries[i].y = val++;
  }
  minfo.white_point.x = val++;
  minfo.white_point.y = val++;
  minfo.max_display_mastering_luminance = val++;
  minfo.min_display_mastering_luminance = val++;

  caps = gst_caps_new_empty_simple ("video/x-raw");
  minfo_str = gst_video_mastering_display_info_to_string (&minfo);
  fail_unless (minfo_str != NULL, "cannot convert info to string");
  GST_DEBUG ("converted mastering info string %s", minfo_str);

  gst_caps_set_simple (caps, "mastering-display-info",
      G_TYPE_STRING, minfo_str, NULL);
  g_free (minfo_str);
  minfo_str = NULL;

  /* manually parsing mastering info from string */
  s = gst_caps_get_structure (caps, 0);
  minfo_str = (gchar *) gst_structure_get_string (s, "mastering-display-info");
  fail_unless (minfo_str != NULL);
  fail_unless (gst_video_mastering_display_info_from_string
      (&other_minfo, minfo_str), "cannot get mastering info from string");
  GST_DEBUG ("extracted info string %s", minfo_str);

  fail_unless (gst_video_mastering_display_info_is_equal (&minfo,
          &other_minfo), "Extracted mastering info is not equal to original");

  /* simplified version for caps use case */
  fail_unless (gst_video_mastering_display_info_from_caps (&minfo_from_caps,
          caps), "cannot parse mastering info from caps");
  fail_unless (gst_video_mastering_display_info_is_equal (&minfo,
          &minfo_from_caps),
      "Extracted mastering info is not equal to original");

  /* check _add_to_caps () and manually created one */
  other_caps = gst_caps_new_empty_simple ("video/x-raw");
  fail_unless (gst_video_mastering_display_info_add_to_caps (&other_minfo,
          other_caps));
  fail_unless (gst_caps_is_equal (caps, other_caps));

  gst_caps_unref (caps);
  gst_caps_unref (other_caps);

  /* Test GstVideoContentLightLevel */
  gst_video_content_light_level_init (&level);
  gst_video_content_light_level_init (&other_level);

  level.max_content_light_level = 1000;
  level.max_frame_average_light_level = 300;

  caps = gst_caps_new_empty_simple ("video/x-raw");
  level_str = gst_video_content_light_level_to_string (&level);
  fail_unless (level_str != NULL);

  gst_caps_set_simple (caps, "content-light-level",
      G_TYPE_STRING, level_str, NULL);
  g_free (level_str);

  /* manually parsing CLL info from string */
  s = gst_caps_get_structure (caps, 0);
  fail_unless (gst_structure_get (s, "content-light-level",
          G_TYPE_STRING, &level_str, NULL), "Failed to get level from caps");
  fail_unless (gst_video_content_light_level_from_string (&other_level,
          level_str));
  g_free (level_str);

  fail_unless_equals_int (level.max_content_light_level,
      other_level.max_content_light_level);
  fail_unless_equals_int (level.max_frame_average_light_level,
      other_level.max_frame_average_light_level);

  /* simplified version for caps use case */
  fail_unless (gst_video_content_light_level_from_caps (&level_from_caps,
          caps));
  fail_unless_equals_int (level.max_content_light_level,
      level_from_caps.max_content_light_level);
  fail_unless_equals_int (level.max_frame_average_light_level,
      level_from_caps.max_frame_average_light_level);

  /* check _add_to_caps () and manually created one */
  other_caps = gst_caps_new_empty_simple ("video/x-raw");
  fail_unless (gst_video_content_light_level_add_to_caps (&other_level,
          other_caps));
  fail_unless (gst_caps_is_equal (caps, other_caps));

  gst_caps_unref (caps);
  gst_caps_unref (other_caps);
}

GST_END_TEST;

GST_START_TEST (test_video_color_from_to_iso)
{
  gint i;

#define ISO_IEC_UNSPECIFIED_COLOR_VALUE 2

  for (i = 0; i <= GST_VIDEO_COLOR_MATRIX_BT2020; i++) {
    guint matrix_val = gst_video_color_matrix_to_iso (i);
    fail_unless_equals_int (gst_video_color_matrix_from_iso (matrix_val), i);
  }

  for (i = 0; i <= GST_VIDEO_TRANSFER_ARIB_STD_B67; i++) {
    guint transfer_val = gst_video_transfer_function_to_iso (i);

    /* don't know how to map below values to spec. */
    if (i == GST_VIDEO_TRANSFER_GAMMA18 || i == GST_VIDEO_TRANSFER_GAMMA20
        || i == GST_VIDEO_TRANSFER_ADOBERGB) {
      fail_unless_equals_int (transfer_val, ISO_IEC_UNSPECIFIED_COLOR_VALUE);
      continue;
    }

    fail_unless_equals_int (gst_video_transfer_function_from_iso (transfer_val),
        i);
  }

  for (i = 0; i <= GST_VIDEO_COLOR_PRIMARIES_EBU3213; i++) {
    guint primaries_val = gst_video_color_primaries_to_iso (i);

    /* don't know how to map below value to spec. */
    if (i == GST_VIDEO_COLOR_PRIMARIES_ADOBERGB) {
      fail_unless_equals_int (primaries_val, ISO_IEC_UNSPECIFIED_COLOR_VALUE);
      continue;
    }

    fail_unless_equals_int (gst_video_color_primaries_from_iso (primaries_val),
        i);
  }
#undef ISO_IEC_UNSPECIFIED_COLOR_VALUE
}

GST_END_TEST;

GST_START_TEST (test_video_format_info_plane_to_components)
{
  const GstVideoFormatInfo *info;
  gint comps[GST_VIDEO_MAX_COMPONENTS];

  /* RGB: 1 plane, 3 components */
  info = gst_video_format_get_info (GST_VIDEO_FORMAT_RGB);

  gst_video_format_info_component (info, 0, comps);
  g_assert_cmpint (comps[0], ==, 0);
  g_assert_cmpint (comps[1], ==, 1);
  g_assert_cmpint (comps[2], ==, 2);
  g_assert_cmpint (comps[3], ==, -1);

  gst_video_format_info_component (info, 1, comps);
  g_assert_cmpint (comps[0], ==, -1);
  g_assert_cmpint (comps[1], ==, -1);
  g_assert_cmpint (comps[2], ==, -1);
  g_assert_cmpint (comps[3], ==, -1);

  gst_video_format_info_component (info, 2, comps);
  g_assert_cmpint (comps[0], ==, -1);
  g_assert_cmpint (comps[1], ==, -1);
  g_assert_cmpint (comps[2], ==, -1);
  g_assert_cmpint (comps[3], ==, -1);

  gst_video_format_info_component (info, 3, comps);
  g_assert_cmpint (comps[0], ==, -1);
  g_assert_cmpint (comps[1], ==, -1);
  g_assert_cmpint (comps[2], ==, -1);
  g_assert_cmpint (comps[3], ==, -1);

  /* I420: 3 planes, 3 components */
  info = gst_video_format_get_info (GST_VIDEO_FORMAT_I420);

  gst_video_format_info_component (info, 0, comps);
  g_assert_cmpint (comps[0], ==, 0);
  g_assert_cmpint (comps[1], ==, -1);
  g_assert_cmpint (comps[2], ==, -1);
  g_assert_cmpint (comps[3], ==, -1);

  gst_video_format_info_component (info, 1, comps);
  g_assert_cmpint (comps[0], ==, 1);
  g_assert_cmpint (comps[1], ==, -1);
  g_assert_cmpint (comps[2], ==, -1);
  g_assert_cmpint (comps[3], ==, -1);

  gst_video_format_info_component (info, 2, comps);
  g_assert_cmpint (comps[0], ==, 2);
  g_assert_cmpint (comps[1], ==, -1);
  g_assert_cmpint (comps[2], ==, -1);
  g_assert_cmpint (comps[3], ==, -1);

  gst_video_format_info_component (info, 3, comps);
  g_assert_cmpint (comps[0], ==, -1);
  g_assert_cmpint (comps[1], ==, -1);
  g_assert_cmpint (comps[2], ==, -1);
  g_assert_cmpint (comps[3], ==, -1);

  /* NV12: 2 planes, 3 components */
  info = gst_video_format_get_info (GST_VIDEO_FORMAT_NV12);

  gst_video_format_info_component (info, 0, comps);
  g_assert_cmpint (comps[0], ==, 0);
  g_assert_cmpint (comps[1], ==, -1);
  g_assert_cmpint (comps[2], ==, -1);
  g_assert_cmpint (comps[3], ==, -1);

  gst_video_format_info_component (info, 1, comps);
  g_assert_cmpint (comps[0], ==, 1);
  g_assert_cmpint (comps[1], ==, 2);
  g_assert_cmpint (comps[2], ==, -1);
  g_assert_cmpint (comps[3], ==, -1);

  gst_video_format_info_component (info, 2, comps);
  g_assert_cmpint (comps[0], ==, -1);
  g_assert_cmpint (comps[1], ==, -1);
  g_assert_cmpint (comps[2], ==, -1);
  g_assert_cmpint (comps[3], ==, -1);

  gst_video_format_info_component (info, 3, comps);
  g_assert_cmpint (comps[0], ==, -1);
  g_assert_cmpint (comps[1], ==, -1);
  g_assert_cmpint (comps[2], ==, -1);
  g_assert_cmpint (comps[3], ==, -1);
}

GST_END_TEST;

GST_START_TEST (test_video_info_align)
{
  GstVideoInfo info;
  GstVideoAlignment align;
  gsize plane_size[GST_VIDEO_MAX_PLANES];

  /* NV12 */
  gst_video_info_init (&info);
  gst_video_info_set_format (&info, GST_VIDEO_FORMAT_NV12, 1920, 1080);

  g_assert_cmpuint (GST_VIDEO_INFO_HEIGHT (&info), ==, 1080);
  g_assert_cmpuint (GST_VIDEO_INFO_FIELD_HEIGHT (&info), ==, 1080);
  g_assert_cmpuint (GST_VIDEO_INFO_SIZE (&info), ==, 1920 * 1080 * 1.5);

  gst_video_alignment_reset (&align);
  /* Align with no padding to retrieve the plane heights */
  g_assert (gst_video_info_align_full (&info, &align, plane_size));

  g_assert_cmpuint (plane_size[0], ==, 1920 * 1080);
  g_assert_cmpuint (plane_size[1], ==, 1920 * 1080 / 2);
  g_assert_cmpuint (plane_size[2], ==, 0);
  g_assert_cmpuint (plane_size[3], ==, 0);

  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_STRIDE (&info, 0), ==, 1920);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_STRIDE (&info, 1), ==, 1920);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_STRIDE (&info, 2), ==, 0);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_STRIDE (&info, 3), ==, 0);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_HEIGHT (&info, 0, plane_size), ==,
      1080);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_HEIGHT (&info, 1, plane_size), ==,
      540);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_HEIGHT (&info, 2, plane_size), ==, 0);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_HEIGHT (&info, 3, plane_size), ==, 0);

  gst_video_alignment_reset (&align);
  align.padding_bottom = 8;
  g_assert (gst_video_info_align_full (&info, &align, plane_size));

  g_assert_cmpuint (plane_size[0], ==, 1920 * 1088);
  g_assert_cmpuint (plane_size[1], ==, 1920 * 1088 / 2);
  g_assert_cmpuint (plane_size[2], ==, 0);
  g_assert_cmpuint (plane_size[3], ==, 0);

  g_assert_cmpuint (GST_VIDEO_INFO_HEIGHT (&info), ==, 1080);
  g_assert_cmpuint (GST_VIDEO_INFO_FIELD_HEIGHT (&info), ==, 1080);
  g_assert_cmpuint (GST_VIDEO_INFO_SIZE (&info), ==, 1920 * 1088 * 1.5);

  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_STRIDE (&info, 0), ==, 1920);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_STRIDE (&info, 1), ==, 1920);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_STRIDE (&info, 2), ==, 0);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_STRIDE (&info, 3), ==, 0);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_HEIGHT (&info, 0, plane_size), ==,
      1088);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_HEIGHT (&info, 1, plane_size), ==,
      544);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_HEIGHT (&info, 2, plane_size), ==, 0);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_HEIGHT (&info, 3, plane_size), ==, 0);

  /* NV16 */
  gst_video_info_init (&info);
  gst_video_info_set_format (&info, GST_VIDEO_FORMAT_NV16, 1920, 1080);

  g_assert_cmpuint (GST_VIDEO_INFO_HEIGHT (&info), ==, 1080);
  g_assert_cmpuint (GST_VIDEO_INFO_FIELD_HEIGHT (&info), ==, 1080);
  g_assert_cmpuint (GST_VIDEO_INFO_SIZE (&info), ==, 1920 * 1080 * 2);

  gst_video_alignment_reset (&align);
  /* Align with no padding to retrieve the plane heights */
  g_assert (gst_video_info_align_full (&info, &align, plane_size));

  g_assert_cmpuint (plane_size[0], ==, 1920 * 1080);
  g_assert_cmpuint (plane_size[1], ==, 1920 * 1080);
  g_assert_cmpuint (plane_size[2], ==, 0);
  g_assert_cmpuint (plane_size[3], ==, 0);

  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_STRIDE (&info, 0), ==, 1920);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_STRIDE (&info, 1), ==, 1920);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_STRIDE (&info, 2), ==, 0);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_STRIDE (&info, 3), ==, 0);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_HEIGHT (&info, 0, plane_size), ==,
      1080);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_HEIGHT (&info, 1, plane_size), ==,
      1080);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_HEIGHT (&info, 2, plane_size), ==, 0);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_HEIGHT (&info, 3, plane_size), ==, 0);

  gst_video_alignment_reset (&align);
  align.padding_bottom = 8;
  g_assert (gst_video_info_align_full (&info, &align, plane_size));

  g_assert_cmpuint (GST_VIDEO_INFO_HEIGHT (&info), ==, 1080);
  g_assert_cmpuint (GST_VIDEO_INFO_FIELD_HEIGHT (&info), ==, 1080);
  g_assert_cmpuint (GST_VIDEO_INFO_SIZE (&info), ==, 1920 * 1088 * 2);

  g_assert_cmpuint (plane_size[0], ==, 1920 * 1088);
  g_assert_cmpuint (plane_size[1], ==, 1920 * 1088);
  g_assert_cmpuint (plane_size[2], ==, 0);
  g_assert_cmpuint (plane_size[3], ==, 0);

  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_STRIDE (&info, 0), ==, 1920);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_STRIDE (&info, 1), ==, 1920);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_STRIDE (&info, 2), ==, 0);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_STRIDE (&info, 3), ==, 0);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_HEIGHT (&info, 0, plane_size), ==,
      1088);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_HEIGHT (&info, 1, plane_size), ==,
      1088);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_HEIGHT (&info, 2, plane_size), ==, 0);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_HEIGHT (&info, 3, plane_size), ==, 0);

  /* RGB */
  gst_video_info_init (&info);
  gst_video_info_set_format (&info, GST_VIDEO_FORMAT_RGB, 1920, 1080);

  g_assert_cmpuint (GST_VIDEO_INFO_HEIGHT (&info), ==, 1080);
  g_assert_cmpuint (GST_VIDEO_INFO_FIELD_HEIGHT (&info), ==, 1080);
  g_assert_cmpuint (GST_VIDEO_INFO_SIZE (&info), ==, 1920 * 1080 * 3);

  gst_video_alignment_reset (&align);
  /* Align with no padding to retrieve the plane heights */
  g_assert (gst_video_info_align_full (&info, &align, plane_size));

  g_assert_cmpuint (plane_size[0], ==, 1920 * 1080 * 3);
  g_assert_cmpuint (plane_size[1], ==, 0);
  g_assert_cmpuint (plane_size[2], ==, 0);
  g_assert_cmpuint (plane_size[3], ==, 0);

  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_STRIDE (&info, 0), ==, 5760);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_STRIDE (&info, 1), ==, 0);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_STRIDE (&info, 2), ==, 0);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_STRIDE (&info, 3), ==, 0);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_HEIGHT (&info, 0, plane_size), ==,
      1080);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_HEIGHT (&info, 1, plane_size), ==, 0);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_HEIGHT (&info, 2, plane_size), ==, 0);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_HEIGHT (&info, 3, plane_size), ==, 0);

  gst_video_alignment_reset (&align);
  align.padding_bottom = 8;
  g_assert (gst_video_info_align_full (&info, &align, plane_size));

  g_assert_cmpuint (GST_VIDEO_INFO_HEIGHT (&info), ==, 1080);
  g_assert_cmpuint (GST_VIDEO_INFO_FIELD_HEIGHT (&info), ==, 1080);
  g_assert_cmpuint (GST_VIDEO_INFO_SIZE (&info), ==, 1920 * 1088 * 3);

  g_assert_cmpuint (plane_size[0], ==, 1920 * 1088 * 3);
  g_assert_cmpuint (plane_size[1], ==, 0);
  g_assert_cmpuint (plane_size[2], ==, 0);
  g_assert_cmpuint (plane_size[3], ==, 0);

  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_STRIDE (&info, 0), ==, 5760);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_STRIDE (&info, 1), ==, 0);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_STRIDE (&info, 2), ==, 0);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_STRIDE (&info, 3), ==, 0);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_HEIGHT (&info, 0, plane_size), ==,
      1088);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_HEIGHT (&info, 1, plane_size), ==, 0);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_HEIGHT (&info, 2, plane_size), ==, 0);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_HEIGHT (&info, 3, plane_size), ==, 0);

  /* I420 */
  gst_video_info_init (&info);
  gst_video_info_set_format (&info, GST_VIDEO_FORMAT_I420, 1920, 1080);

  g_assert_cmpuint (GST_VIDEO_INFO_HEIGHT (&info), ==, 1080);
  g_assert_cmpuint (GST_VIDEO_INFO_FIELD_HEIGHT (&info), ==, 1080);
  g_assert_cmpuint (GST_VIDEO_INFO_SIZE (&info), ==, 1920 * 1080 * 1.5);

  gst_video_alignment_reset (&align);
  /* Align with no padding to retrieve the plane heights */
  g_assert (gst_video_info_align_full (&info, &align, plane_size));

  g_assert_cmpuint (plane_size[0], ==, 1920 * 1080);
  g_assert_cmpuint (plane_size[1], ==, 1920 * 1080 / 4);
  g_assert_cmpuint (plane_size[2], ==, 1920 * 1080 / 4);
  g_assert_cmpuint (plane_size[3], ==, 0);

  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_STRIDE (&info, 0), ==, 1920);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_STRIDE (&info, 1), ==, 960);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_STRIDE (&info, 2), ==, 960);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_STRIDE (&info, 3), ==, 0);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_HEIGHT (&info, 0, plane_size), ==,
      1080);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_HEIGHT (&info, 1, plane_size), ==,
      540);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_HEIGHT (&info, 2, plane_size), ==,
      540);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_HEIGHT (&info, 3, plane_size), ==, 0);

  gst_video_alignment_reset (&align);
  align.padding_bottom = 8;
  g_assert (gst_video_info_align_full (&info, &align, plane_size));

  g_assert_cmpuint (GST_VIDEO_INFO_HEIGHT (&info), ==, 1080);
  g_assert_cmpuint (GST_VIDEO_INFO_FIELD_HEIGHT (&info), ==, 1080);
  g_assert_cmpuint (GST_VIDEO_INFO_SIZE (&info), ==, 1920 * 1088 * 1.5);

  g_assert_cmpuint (plane_size[0], ==, 1920 * 1088);
  g_assert_cmpuint (plane_size[1], ==, 1920 * 1088 / 4);
  g_assert_cmpuint (plane_size[2], ==, 1920 * 1088 / 4);
  g_assert_cmpuint (plane_size[3], ==, 0);

  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_STRIDE (&info, 0), ==, 1920);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_STRIDE (&info, 1), ==, 960);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_STRIDE (&info, 2), ==, 960);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_STRIDE (&info, 3), ==, 0);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_HEIGHT (&info, 0, plane_size), ==,
      1088);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_HEIGHT (&info, 1, plane_size), ==,
      544);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_HEIGHT (&info, 2, plane_size), ==,
      544);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_HEIGHT (&info, 3, plane_size), ==, 0);

  /* NV16 alternate */
  gst_video_info_init (&info);
  gst_video_info_set_interlaced_format (&info, GST_VIDEO_FORMAT_NV16,
      GST_VIDEO_INTERLACE_MODE_ALTERNATE, 1920, 1080);

  g_assert_cmpuint (GST_VIDEO_INFO_HEIGHT (&info), ==, 1080);
  g_assert_cmpuint (GST_VIDEO_INFO_FIELD_HEIGHT (&info), ==, 540);
  g_assert_cmpuint (GST_VIDEO_INFO_SIZE (&info), ==, 1920 * 540 * 2);

  gst_video_alignment_reset (&align);
  /* Align with no padding to retrieve the plane heights */
  g_assert (gst_video_info_align_full (&info, &align, plane_size));

  g_assert_cmpuint (plane_size[0], ==, 1920 * 540);
  g_assert_cmpuint (plane_size[1], ==, 1920 * 540);
  g_assert_cmpuint (plane_size[2], ==, 0);
  g_assert_cmpuint (plane_size[3], ==, 0);

  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_STRIDE (&info, 0), ==, 1920);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_STRIDE (&info, 1), ==, 1920);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_STRIDE (&info, 2), ==, 0);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_STRIDE (&info, 3), ==, 0);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_HEIGHT (&info, 0, plane_size), ==,
      540);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_HEIGHT (&info, 1, plane_size), ==,
      540);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_HEIGHT (&info, 2, plane_size), ==, 0);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_HEIGHT (&info, 3, plane_size), ==, 0);

  gst_video_alignment_reset (&align);
  align.padding_bottom = 8;
  g_assert (gst_video_info_align_full (&info, &align, plane_size));

  g_assert_cmpuint (GST_VIDEO_INFO_HEIGHT (&info), ==, 1080);
  g_assert_cmpuint (GST_VIDEO_INFO_FIELD_HEIGHT (&info), ==, 540);
  g_assert_cmpuint (GST_VIDEO_INFO_SIZE (&info), ==, 1920 * 544 * 2);

  g_assert_cmpuint (plane_size[0], ==, 1920 * 544);
  g_assert_cmpuint (plane_size[1], ==, 1920 * 544);
  g_assert_cmpuint (plane_size[2], ==, 0);
  g_assert_cmpuint (plane_size[3], ==, 0);

  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_STRIDE (&info, 0), ==, 1920);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_STRIDE (&info, 1), ==, 1920);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_STRIDE (&info, 2), ==, 0);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_STRIDE (&info, 3), ==, 0);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_HEIGHT (&info, 0, plane_size), ==,
      544);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_HEIGHT (&info, 1, plane_size), ==,
      544);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_HEIGHT (&info, 2, plane_size), ==, 0);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_HEIGHT (&info, 3, plane_size), ==, 0);

  /* NV16 alternate with an odd height */
  gst_video_info_init (&info);
  gst_video_info_set_interlaced_format (&info, GST_VIDEO_FORMAT_NV16,
      GST_VIDEO_INTERLACE_MODE_ALTERNATE, 1920, 1081);

  g_assert_cmpuint (GST_VIDEO_INFO_HEIGHT (&info), ==, 1081);
  g_assert_cmpuint (GST_VIDEO_INFO_FIELD_HEIGHT (&info), ==, 541);
  g_assert_cmpuint (GST_VIDEO_INFO_SIZE (&info), ==, 1920 * 541 * 2);

  gst_video_alignment_reset (&align);
  /* Align with no padding to retrieve the plane heights */
  g_assert (gst_video_info_align_full (&info, &align, plane_size));

  g_assert_cmpuint (plane_size[0], ==, 1920 * 541);
  g_assert_cmpuint (plane_size[1], ==, 1920 * 541);
  g_assert_cmpuint (plane_size[2], ==, 0);
  g_assert_cmpuint (plane_size[3], ==, 0);

  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_STRIDE (&info, 0), ==, 1920);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_STRIDE (&info, 1), ==, 1920);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_STRIDE (&info, 2), ==, 0);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_STRIDE (&info, 3), ==, 0);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_HEIGHT (&info, 0, plane_size), ==,
      541);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_HEIGHT (&info, 1, plane_size), ==,
      541);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_HEIGHT (&info, 2, plane_size), ==, 0);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_HEIGHT (&info, 3, plane_size), ==, 0);

  gst_video_alignment_reset (&align);
  align.padding_bottom = 2;
  g_assert (gst_video_info_align_full (&info, &align, plane_size));

  g_assert_cmpuint (GST_VIDEO_INFO_HEIGHT (&info), ==, 1081);
  g_assert_cmpuint (GST_VIDEO_INFO_FIELD_HEIGHT (&info), ==, 541);
  g_assert_cmpuint (GST_VIDEO_INFO_SIZE (&info), ==, 1920 * 542 * 2);

  g_assert_cmpuint (plane_size[0], ==, 1920 * 542);
  g_assert_cmpuint (plane_size[1], ==, 1920 * 542);
  g_assert_cmpuint (plane_size[2], ==, 0);
  g_assert_cmpuint (plane_size[3], ==, 0);

  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_STRIDE (&info, 0), ==, 1920);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_STRIDE (&info, 1), ==, 1920);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_STRIDE (&info, 2), ==, 0);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_STRIDE (&info, 3), ==, 0);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_HEIGHT (&info, 0, plane_size), ==,
      542);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_HEIGHT (&info, 1, plane_size), ==,
      542);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_HEIGHT (&info, 2, plane_size), ==, 0);
  g_assert_cmpuint (GST_VIDEO_INFO_PLANE_HEIGHT (&info, 3, plane_size), ==, 0);
}

GST_END_TEST;

GST_START_TEST (test_video_meta_align)
{
  GstBuffer *buf;
  GstVideoInfo info;
  GstVideoMeta *meta;
  gsize plane_size[GST_VIDEO_MAX_PLANES];
  guint plane_height[GST_VIDEO_MAX_PLANES];
  GstVideoAlignment alig;

  buf = gst_buffer_new ();

  /* NV12 no alignment */
  gst_video_info_init (&info);
  gst_video_info_set_format (&info, GST_VIDEO_FORMAT_NV12, 1920, 1080);

  meta = gst_buffer_add_video_meta_full (buf, GST_VIDEO_FRAME_FLAG_NONE,
      GST_VIDEO_INFO_FORMAT (&info), GST_VIDEO_INFO_WIDTH (&info),
      GST_VIDEO_INFO_HEIGHT (&info), GST_VIDEO_INFO_N_PLANES (&info),
      info.offset, info.stride);

  g_assert_cmpuint (meta->alignment.padding_top, ==, 0);
  g_assert_cmpuint (meta->alignment.padding_bottom, ==, 0);
  g_assert_cmpuint (meta->alignment.padding_left, ==, 0);
  g_assert_cmpuint (meta->alignment.padding_right, ==, 0);

  g_assert (gst_video_meta_get_plane_size (meta, plane_size));
  g_assert_cmpuint (plane_size[0], ==, 1920 * 1080);
  g_assert_cmpuint (plane_size[1], ==, 1920 * 1080 * 0.5);
  g_assert_cmpuint (plane_size[2], ==, 0);
  g_assert_cmpuint (plane_size[3], ==, 0);

  g_assert (gst_video_meta_get_plane_height (meta, plane_height));
  g_assert_cmpuint (plane_height[0], ==, 1080);
  g_assert_cmpuint (plane_height[1], ==, 540);
  g_assert_cmpuint (plane_height[2], ==, 0);
  g_assert_cmpuint (plane_height[3], ==, 0);

  /* horizontal alignment */
  gst_video_info_init (&info);
  gst_video_info_set_format (&info, GST_VIDEO_FORMAT_NV12, 1920, 1080);

  gst_video_alignment_reset (&alig);
  alig.padding_left = 2;
  alig.padding_right = 6;

  g_assert (gst_video_info_align (&info, &alig));

  meta = gst_buffer_add_video_meta_full (buf, GST_VIDEO_FRAME_FLAG_NONE,
      GST_VIDEO_INFO_FORMAT (&info), GST_VIDEO_INFO_WIDTH (&info),
      GST_VIDEO_INFO_HEIGHT (&info), GST_VIDEO_INFO_N_PLANES (&info),
      info.offset, info.stride);
  g_assert (gst_video_meta_set_alignment (meta, alig));

  g_assert_cmpuint (meta->alignment.padding_top, ==, 0);
  g_assert_cmpuint (meta->alignment.padding_bottom, ==, 0);
  g_assert_cmpuint (meta->alignment.padding_left, ==, 2);
  g_assert_cmpuint (meta->alignment.padding_right, ==, 6);

  g_assert (gst_video_meta_get_plane_size (meta, plane_size));
  g_assert_cmpuint (plane_size[0], ==, 1928 * 1080);
  g_assert_cmpuint (plane_size[1], ==, 1928 * 1080 * 0.5);
  g_assert_cmpuint (plane_size[2], ==, 0);
  g_assert_cmpuint (plane_size[3], ==, 0);

  g_assert (gst_video_meta_get_plane_height (meta, plane_height));
  g_assert_cmpuint (plane_height[0], ==, 1080);
  g_assert_cmpuint (plane_height[1], ==, 540);
  g_assert_cmpuint (plane_height[2], ==, 0);
  g_assert_cmpuint (plane_height[3], ==, 0);

  /* vertical alignment */
  gst_video_info_init (&info);
  gst_video_info_set_format (&info, GST_VIDEO_FORMAT_NV12, 1920, 1080);

  gst_video_alignment_reset (&alig);
  alig.padding_top = 2;
  alig.padding_bottom = 6;

  g_assert (gst_video_info_align (&info, &alig));

  meta = gst_buffer_add_video_meta_full (buf, GST_VIDEO_FRAME_FLAG_NONE,
      GST_VIDEO_INFO_FORMAT (&info), GST_VIDEO_INFO_WIDTH (&info),
      GST_VIDEO_INFO_HEIGHT (&info), GST_VIDEO_INFO_N_PLANES (&info),
      info.offset, info.stride);
  g_assert (gst_video_meta_set_alignment (meta, alig));

  g_assert_cmpuint (meta->alignment.padding_top, ==, 2);
  g_assert_cmpuint (meta->alignment.padding_bottom, ==, 6);
  g_assert_cmpuint (meta->alignment.padding_left, ==, 0);
  g_assert_cmpuint (meta->alignment.padding_right, ==, 0);

  g_assert (gst_video_meta_get_plane_size (meta, plane_size));
  g_assert_cmpuint (plane_size[0], ==, 1920 * 1088);
  g_assert_cmpuint (plane_size[1], ==, 1920 * 1088 * 0.5);
  g_assert_cmpuint (plane_size[2], ==, 0);
  g_assert_cmpuint (plane_size[3], ==, 0);

  g_assert (gst_video_meta_get_plane_height (meta, plane_height));
  g_assert_cmpuint (plane_height[0], ==, 1088);
  g_assert_cmpuint (plane_height[1], ==, 544);
  g_assert_cmpuint (plane_height[2], ==, 0);
  g_assert_cmpuint (plane_height[3], ==, 0);

  /* incompatible alignment */
  gst_video_info_init (&info);
  gst_video_info_set_format (&info, GST_VIDEO_FORMAT_NV12, 1920, 1080);

  gst_video_alignment_reset (&alig);
  alig.padding_right = 2;

  meta = gst_buffer_add_video_meta_full (buf, GST_VIDEO_FRAME_FLAG_NONE,
      GST_VIDEO_INFO_FORMAT (&info), GST_VIDEO_INFO_WIDTH (&info),
      GST_VIDEO_INFO_HEIGHT (&info), GST_VIDEO_INFO_N_PLANES (&info),
      info.offset, info.stride);
  g_assert (!gst_video_meta_set_alignment (meta, alig));

  gst_buffer_unref (buf);
}

GST_END_TEST;

GST_START_TEST (test_video_flags)
{
  GstBuffer *buf;
  GstVideoInfo info;
  GstVideoFrame frame;

  gst_video_info_init (&info);
  fail_unless (gst_video_info_set_interlaced_format (&info,
          GST_VIDEO_FORMAT_RGB, GST_VIDEO_INTERLACE_MODE_ALTERNATE, 4, 4));

  buf = gst_buffer_new_and_alloc (GST_VIDEO_INFO_SIZE (&info));
  fail_unless (!GST_VIDEO_BUFFER_IS_TOP_FIELD (buf));
  fail_unless (!GST_VIDEO_BUFFER_IS_BOTTOM_FIELD (buf));
  fail_unless (gst_video_frame_map (&frame, &info, buf, GST_MAP_READ));
  fail_unless (!GST_VIDEO_FRAME_IS_TOP_FIELD (&frame));
  fail_unless (!GST_VIDEO_FRAME_IS_BOTTOM_FIELD (&frame));
  gst_video_frame_unmap (&frame);
  gst_buffer_unref (buf);

  buf = gst_buffer_new_and_alloc (GST_VIDEO_INFO_SIZE (&info));
  GST_BUFFER_FLAG_SET (buf, GST_VIDEO_BUFFER_FLAG_TOP_FIELD);
  fail_unless (GST_VIDEO_BUFFER_IS_TOP_FIELD (buf));
  fail_unless (!GST_VIDEO_BUFFER_IS_BOTTOM_FIELD (buf));
  fail_unless (gst_video_frame_map (&frame, &info, buf, GST_MAP_READ));
  fail_unless (GST_VIDEO_FRAME_IS_TOP_FIELD (&frame));
  fail_unless (!GST_VIDEO_FRAME_IS_BOTTOM_FIELD (&frame));
  gst_video_frame_unmap (&frame);
  gst_buffer_unref (buf);

  buf = gst_buffer_new_and_alloc (GST_VIDEO_INFO_SIZE (&info));
  GST_BUFFER_FLAG_SET (buf, GST_VIDEO_BUFFER_FLAG_BOTTOM_FIELD);
  fail_unless (!GST_VIDEO_BUFFER_IS_TOP_FIELD (buf));
  fail_unless (GST_VIDEO_BUFFER_IS_BOTTOM_FIELD (buf));
  fail_unless (gst_video_frame_map (&frame, &info, buf, GST_MAP_READ));
  fail_unless (!GST_VIDEO_FRAME_IS_TOP_FIELD (&frame));
  fail_unless (GST_VIDEO_FRAME_IS_BOTTOM_FIELD (&frame));
  gst_video_frame_unmap (&frame);
  gst_buffer_unref (buf);
}

GST_END_TEST;

GST_START_TEST (test_video_make_raw_caps)
{
  GstCaps *caps, *expected;
  GstVideoFormat f1[] = { GST_VIDEO_FORMAT_NV12 };
  GstVideoFormat f2[] = { GST_VIDEO_FORMAT_NV12, GST_VIDEO_FORMAT_NV16 };

  caps = gst_video_make_raw_caps (f1, G_N_ELEMENTS (f1));
  expected = gst_caps_from_string (GST_VIDEO_CAPS_MAKE ("NV12"));
  fail_unless (gst_caps_is_equal (caps, expected));
  gst_caps_unref (caps);
  gst_caps_unref (expected);

  caps = gst_video_make_raw_caps (f2, G_N_ELEMENTS (f2));
  expected = gst_caps_from_string (GST_VIDEO_CAPS_MAKE ("{ NV12, NV16 }"));
  fail_unless (gst_caps_is_equal (caps, expected));
  gst_caps_unref (caps);
  gst_caps_unref (expected);

  caps = gst_video_make_raw_caps (NULL, 0);
  expected = gst_caps_from_string (GST_VIDEO_CAPS_MAKE (GST_VIDEO_FORMATS_ALL));
  fail_unless (gst_caps_is_equal (caps, expected));
  gst_caps_unref (caps);
  gst_caps_unref (expected);

  caps =
      gst_video_make_raw_caps_with_features (NULL, 0,
      gst_caps_features_new_any ());
  expected =
      gst_caps_from_string (GST_VIDEO_CAPS_MAKE_WITH_FEATURES ("ANY",
          GST_VIDEO_FORMATS_ALL));
  fail_unless (gst_caps_is_equal (caps, expected));
  gst_caps_unref (caps);
  gst_caps_unref (expected);

  guint len;
  const GstVideoFormat *formats = gst_video_formats_any (&len);
  caps =
      gst_video_make_raw_caps_with_features (formats, len,
      gst_caps_features_new_any ());
  expected =
      gst_caps_from_string (GST_VIDEO_CAPS_MAKE_WITH_FEATURES ("ANY",
          GST_VIDEO_FORMATS_ANY));
  fail_unless (gst_caps_is_equal (caps, expected));
  gst_caps_unref (caps);
  gst_caps_unref (expected);
}

GST_END_TEST;

GST_START_TEST (test_video_extrapolate_stride)
{
  guint num_formats = get_num_formats ();
  GstVideoFormat format;

  for (format = 2; format < num_formats; format++) {
    GstVideoInfo info;
    guint p;

    /*
     * Use an easy resolution, since GStreamer uses arbitrary padding which
     * cannot be extrapolated.
     */
    gst_video_info_set_format (&info, format, 320, 240);

    /* Skip over tiled formats, since stride meaning is different */
    if (GST_VIDEO_FORMAT_INFO_IS_TILED (info.finfo))
      continue;

    for (p = 0; p < GST_VIDEO_INFO_N_PLANES (&info); p++) {
      guint stride;

      /* Skip over palette planes */
      if (GST_VIDEO_FORMAT_INFO_HAS_PALETTE (info.finfo) &&
          p >= GST_VIDEO_COMP_PALETTE)
        break;

      stride = gst_video_format_info_extrapolate_stride (info.finfo, p,
          info.stride[0]);
      fail_unless (stride == info.stride[p]);
    }
  }
}

GST_END_TEST;

GST_START_TEST (test_auto_video_frame_unmap)
{
#ifdef g_auto
  g_autoptr (GstBuffer) buf = NULL;
  GstVideoInfo info;

  fail_unless (gst_video_info_set_format (&info, GST_VIDEO_FORMAT_ENCODED, 10,
          10));
  buf = gst_buffer_new_and_alloc (info.size);

  {
    // unmap should be no-op
    g_auto (GstVideoFrame) frame = GST_VIDEO_FRAME_INIT;
    fail_unless (frame.buffer == NULL);
  }

  {
    g_auto (GstVideoFrame) frame = GST_VIDEO_FRAME_INIT;
    gst_video_frame_map (&frame, &info, buf, GST_MAP_READ);
    fail_unless_equals_int (GST_MINI_OBJECT_REFCOUNT (buf), 2);
  }

  fail_unless_equals_int (GST_MINI_OBJECT_REFCOUNT (buf), 1);

#endif
}

GST_END_TEST;

static gboolean
is_equal_primaries_coord (const GstVideoColorPrimariesInfo * a,
    const GstVideoColorPrimariesInfo * b)
{
  return (a->Wx == b->Wx && a->Wy == b->Wy && a->Rx == b->Rx && a->Ry == a->Ry
      && a->Gx == b->Gx && a->Gy == b->Gy && a->Bx == b->Bx && a->By == b->By);
}

GST_START_TEST (test_video_color_primaries_equivalent)
{
  guint i, j;

  for (i = 0; i <= GST_VIDEO_COLOR_PRIMARIES_EBU3213; i++) {
    for (j = 0; j <= GST_VIDEO_COLOR_PRIMARIES_EBU3213; j++) {
      GstVideoColorPrimaries primaries = (GstVideoColorPrimaries) i;
      GstVideoColorPrimaries other = (GstVideoColorPrimaries) j;
      const GstVideoColorPrimariesInfo *primaries_info =
          gst_video_color_primaries_get_info (primaries);
      const GstVideoColorPrimariesInfo *other_info =
          gst_video_color_primaries_get_info (other);
      gboolean equal =
          gst_video_color_primaries_is_equivalent (primaries, other);
      gboolean same_coord = is_equal_primaries_coord (primaries_info,
          other_info);

      if (equal)
        fail_unless (same_coord);
      else
        fail_if (same_coord);
    }
  }
}

GST_END_TEST;

GST_START_TEST (test_info_dma_drm)
{
  const char *nondma_str = "video/x-raw, format=NV12, width=16, height=16";
  const char *dma_str = "video/x-raw(memory:DMABuf), format=NV12, width=16, "
      "height=16";
  const char *drm_str = "video/x-raw(memory:DMABuf), format=DMA_DRM, "
      "width=16, height=16, interlace-mode=(string)progressive, "
      "pixel-aspect-ratio=(fraction)1/1, framerate=(fraction)0/1, "
      "drm-format=NV12:0x0100000000000002";
  const char *invaliddrm_str = "video/x-raw(memory:DMABuf), width=16, "
      "height=16, format=DMA_DRM, drm-format=ZZZZ:0xRGCSEz9ew80";
  GstCaps *caps, *ncaps;
  GstVideoInfo info;
  GstVideoInfoDmaDrm drm_info;
  GstVideoInfo vinfo;

  caps = gst_caps_from_string (nondma_str);
  fail_if (gst_video_is_dma_drm_caps (caps));
  gst_caps_unref (caps);

  caps = gst_caps_from_string (dma_str);
  fail_if (gst_video_info_dma_drm_from_caps (&drm_info, caps));
  gst_caps_unref (caps);

  caps = gst_caps_from_string (drm_str);
  fail_unless (gst_video_info_from_caps (&info, caps));
  fail_unless (GST_VIDEO_INFO_FORMAT (&info) == GST_VIDEO_FORMAT_DMA_DRM);
  fail_unless (gst_video_info_dma_drm_from_caps (&drm_info, caps));
  fail_unless (drm_info.drm_fourcc == 0x3231564e
      && drm_info.drm_modifier == 0x100000000000002);

  fail_unless (gst_video_info_dma_drm_to_video_info (&drm_info, &info));
  fail_unless (GST_VIDEO_INFO_FORMAT (&info) == GST_VIDEO_FORMAT_NV12);

  ncaps = gst_video_info_dma_drm_to_caps (&drm_info);
  fail_unless (ncaps);
  fail_unless (gst_caps_is_equal (caps, ncaps));
  gst_caps_unref (caps);
  gst_caps_unref (ncaps);

  caps = gst_caps_from_string (invaliddrm_str);
  fail_if (gst_video_info_dma_drm_from_caps (&drm_info, caps));
  gst_caps_unref (caps);

  fail_unless (gst_video_info_set_format (&vinfo,
          GST_VIDEO_FORMAT_NV12, 16, 16));
  drm_info.vinfo = vinfo;
  drm_info.drm_fourcc = 0x3231564e;
  drm_info.drm_modifier = 0x100000000000002;
  ncaps = gst_video_info_dma_drm_to_caps (&drm_info);
  fail_unless (ncaps);
  /* remove some fields unrelated to this test. */
  gst_structure_remove_fields (gst_caps_get_structure (ncaps, 0),
      "chroma-site", "colorimetry", NULL);

  caps = gst_caps_from_string (drm_str);
  fail_unless (gst_caps_is_equal (caps, ncaps));
  gst_caps_unref (caps);
  gst_caps_unref (ncaps);

  fail_unless (gst_video_info_dma_drm_from_video_info (&drm_info, &vinfo, 0));
  fail_unless (GST_VIDEO_INFO_FORMAT (&drm_info.vinfo) ==
      GST_VIDEO_FORMAT_NV12);

  fail_unless (gst_video_info_dma_drm_from_video_info (&drm_info, &vinfo,
          0x100000000000002));
  fail_unless (GST_VIDEO_INFO_FORMAT (&drm_info.vinfo) ==
      GST_VIDEO_FORMAT_DMA_DRM);
}

GST_END_TEST;

GST_START_TEST (test_video_meta_serialize)
{
  GstBuffer *buf;
  GstVideoInfo info;
  GstVideoMeta *meta;
  GstVideoAlignment alig;

  gst_video_alignment_reset (&alig);
  alig.padding_left = 2;
  alig.padding_right = 6;
  alig.padding_top = 2;
  alig.padding_bottom = 6;

  gst_video_info_init (&info);
  gst_video_info_set_format (&info, GST_VIDEO_FORMAT_NV12, 1920, 1080);
  g_assert (gst_video_info_align (&info, &alig));

  /* Add video meta */
  buf = gst_buffer_new ();
  meta = gst_buffer_add_video_meta_full (buf, GST_VIDEO_FRAME_FLAG_NONE,
      GST_VIDEO_INFO_FORMAT (&info), GST_VIDEO_INFO_WIDTH (&info),
      GST_VIDEO_INFO_HEIGHT (&info), GST_VIDEO_INFO_N_PLANES (&info),
      info.offset, info.stride);
  g_assert (gst_video_meta_set_alignment (meta, alig));

  /* Serialize */
  GByteArray *data = g_byte_array_new ();
  fail_unless (gst_meta_serialize_simple ((GstMeta *) meta, data));
  gst_buffer_unref (buf);

  /* Create a new buffer */
  buf = gst_buffer_new ();
  guint32 consumed;
  meta = (GstVideoMeta *) gst_meta_deserialize (buf, data->data, data->len,
      &consumed);
  fail_unless (meta);
  fail_unless (consumed == data->len);
  g_byte_array_unref (data);

  /* Check meta's content */
  g_assert_cmpuint (meta->flags, ==, GST_VIDEO_FRAME_FLAG_NONE);
  g_assert_cmpuint (meta->format, ==, GST_VIDEO_FORMAT_NV12);
  g_assert_cmpuint (meta->id, ==, 0);
  g_assert_cmpuint (meta->width, ==, 1920);
  g_assert_cmpuint (meta->height, ==, 1080);
  g_assert_cmpuint (meta->alignment.padding_top, ==, 2);
  g_assert_cmpuint (meta->alignment.padding_bottom, ==, 6);
  g_assert_cmpuint (meta->alignment.padding_left, ==, 2);
  g_assert_cmpuint (meta->alignment.padding_right, ==, 6);

  g_assert_cmpuint (meta->n_planes, ==, GST_VIDEO_INFO_N_PLANES (&info));
  for (int i = 0; i < meta->n_planes; i++) {
    g_assert_cmpuint (meta->stride[i], ==, GST_VIDEO_INFO_PLANE_STRIDE (&info,
            i));
    g_assert_cmpuint (meta->offset[i], ==, GST_VIDEO_INFO_PLANE_OFFSET (&info,
            i));
    g_assert_cmpuint (meta->alignment.stride_align[i], ==, 0);
  }

  gst_buffer_unref (buf);
}

GST_END_TEST;

static Suite *
video_suite (void)
{
  Suite *s = suite_create ("video support library");
  TCase *tc_chain = tcase_create ("general");

  tcase_set_timeout (tc_chain, 60 * 60);

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_video_formats);
  tcase_add_test (tc_chain, test_video_formats_overflow);
  tcase_add_test (tc_chain, test_video_formats_rgb);
  tcase_add_test (tc_chain, test_video_formats_rgba_large_dimension);
  tcase_add_test (tc_chain, test_video_formats_all);
  tcase_add_test (tc_chain, test_video_formats_pack_unpack);
  tcase_add_test (tc_chain, test_guess_framerate);
  tcase_add_test (tc_chain, test_dar_calc);
  tcase_add_test (tc_chain, test_parse_caps_rgb);
  tcase_add_test (tc_chain, test_parse_caps_multiview);
  tcase_add_test (tc_chain, test_parse_colorimetry);
  tcase_add_test (tc_chain, test_events);
  tcase_add_test (tc_chain, test_convert_frame);
  tcase_add_test (tc_chain, test_convert_frame_async);
  tcase_add_test (tc_chain, test_convert_frame_async_error);
  tcase_add_test (tc_chain, test_video_size_from_caps);
  tcase_add_test (tc_chain, test_interlace_mode);
  tcase_add_test (tc_chain, test_overlay_composition);
  tcase_add_test (tc_chain, test_overlay_composition_premultiplied_alpha);
  tcase_add_test (tc_chain, test_overlay_composition_global_alpha);
  tcase_add_test (tc_chain, test_video_pack_unpack2);
  tcase_add_test (tc_chain, test_video_chroma);
  tcase_add_test (tc_chain, test_video_chroma_site);
  tcase_add_test (tc_chain, test_video_scaler);
  tcase_add_test (tc_chain, test_video_color_convert_rgb_rgb);
  tcase_add_test (tc_chain, test_video_color_convert_rgb_yuv);
  tcase_add_test (tc_chain, test_video_color_convert_yuv_yuv);
  tcase_add_test (tc_chain, test_video_color_convert_yuv_rgb);
  tcase_add_test (tc_chain, test_video_color_convert_other);
  tcase_add_test (tc_chain, test_video_size_convert);
  tcase_add_test (tc_chain, test_video_convert);
  tcase_add_test (tc_chain, test_video_convert_multithreading);
  tcase_add_test (tc_chain, test_video_transfer);
  tcase_add_test (tc_chain, test_overlay_blend);
  tcase_add_test (tc_chain, test_video_center_rect);
  tcase_add_test (tc_chain, test_overlay_composition_over_transparency);
  tcase_add_test (tc_chain, test_video_format_enum_stability);
  tcase_add_test (tc_chain, test_video_formats_pstrides);
  tcase_add_test (tc_chain, test_hdr);
  tcase_add_test (tc_chain, test_video_color_from_to_iso);
  tcase_add_test (tc_chain, test_video_format_info_plane_to_components);
  tcase_add_test (tc_chain, test_video_info_align);
  tcase_add_test (tc_chain, test_video_meta_align);
  tcase_add_test (tc_chain, test_video_flags);
  tcase_add_test (tc_chain, test_video_make_raw_caps);
  tcase_add_test (tc_chain, test_video_extrapolate_stride);
  tcase_add_test (tc_chain, test_auto_video_frame_unmap);
  tcase_add_test (tc_chain, test_video_color_primaries_equivalent);
  tcase_add_test (tc_chain, test_info_dma_drm);
  tcase_add_test (tc_chain, test_video_meta_serialize);

  return s;
}

GST_CHECK_MAIN (video);
