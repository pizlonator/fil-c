/* 
 * GStreamer
 * Copyright (C) 2007 David Schleef <ds@schleef.org>
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
 *
 * March 2008
 * Logic enhanced by William Brack <wbrack@mmm.com.hk>
 */

/**
 * SECTION:element-bayer2rgb
 * @title: bayer2rgb
 *
 * Decodes raw camera bayer (fourcc BA81) to RGB.
 */

/*
 * In order to guard against my advancing maturity, some extra detailed
 * information about the logic of the decode is included here.  Much of
 * this was inspired by a technical paper from siliconimaging.com, which
 * in turn was based upon an article from IEEE,
 * T. Sakamoto, C. Nakanishi and T. Hase,
 * “Software pixel interpolation for digital still cameras suitable for
 *  a 32-bit MCU,”
 * IEEE Trans. Consumer Electronics, vol. 44, no. 4, November 1998.
 *
 * The code assumes a Bayer matrix of the type produced by the fourcc
 * BA81 (v4l2 format SBGGR8) of width w and height h which looks like:
 *       0 1 2 3  w-2 w-1
 *
 *   0   B G B G ....B G
 *   1   G R G R ....G R
 *   2   B G B G ....B G
 *       ...............
 * h-2   B G B G ....B G
 * h-1   G R G R ....G R
 *
 * We expand this matrix, producing a separate {r, g, b} triple for each
 * of the individual elements.  The algorithm for doing this expansion is
 * as follows.
 *
 * We are designing for speed of transformation, at a slight expense of code.
 * First, we calculate the appropriate triples for the four corners, the
 * remainder of the top and bottom rows, and the left and right columns.
 * The reason for this is that those elements are transformed slightly
 * differently than all of the remainder of the matrix. Finally, we transform
 * all of the remainder.
 *
 * The transformation into the "appropriate triples" is based upon the
 * "nearest neighbor" principal, with some additional complexity for the
 * calculation of the "green" element, where an "adaptive" pairing is used.
 *
 * For purposes of documentation and identification, each element of the
 * original array can be put into one of four classes:
 *   R   A red element
 *   B   A blue element
 *   GR  A green element which is followed by a red one
 *   GB  A green element which is followed by a blue one
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include <gst/video/video.h>
#include <string.h>
#include <stdlib.h>

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#include "gstbayerelements.h"
#include "gstbayerorc.h"

#define DIV_ROUND_UP(s,v) (((s) + ((v)-1)) / (v))

#define GST_CAT_DEFAULT gst_bayer2rgb_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

enum
{
  GST_BAYER_2_RGB_FORMAT_BGGR = 0,
  GST_BAYER_2_RGB_FORMAT_GBRG,
  GST_BAYER_2_RGB_FORMAT_GRBG,
  GST_BAYER_2_RGB_FORMAT_RGGB
};


#define GST_TYPE_BAYER2RGB            (gst_bayer2rgb_get_type())
#define GST_BAYER2RGB(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_BAYER2RGB,GstBayer2RGB))
#define GST_IS_BAYER2RGB(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_BAYER2RGB))
#define GST_BAYER2RGB_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass) ,GST_TYPE_BAYER2RGB,GstBayer2RGBClass))
#define GST_IS_BAYER2RGB_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass) ,GST_TYPE_BAYER2RGB))
#define GST_BAYER2RGB_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj) ,GST_TYPE_BAYER2RGB,GstBayer2RGBClass))
typedef struct _GstBayer2RGB GstBayer2RGB;
typedef struct _GstBayer2RGBClass GstBayer2RGBClass;

typedef void (*GstBayer2RGBProcessFunc) (GstBayer2RGB *, guint8 *, guint);

struct _GstBayer2RGB
{
  GstBaseTransform basetransform;

  /* < private > */
  GstVideoInfo info;
  int width;
  int height;
  int r_off;                    /* offset for red */
  int g_off;                    /* offset for green */
  int b_off;                    /* offset for blue */
  int format;
  int bpp;                      /* bits per pixel, 8/10/12/14/16 */
  int bigendian;
};

struct _GstBayer2RGBClass
{
  GstBaseTransformClass parent;
};

#define BAYER_CAPS_GEN(mask, bits, endian)	\
	" "#mask#bits#endian

#define BAYER_CAPS_ORD(bits, endian)		\
	BAYER_CAPS_GEN(bggr, bits, endian)","	\
	BAYER_CAPS_GEN(rggb, bits, endian)","	\
	BAYER_CAPS_GEN(grbg, bits, endian)","	\
	BAYER_CAPS_GEN(gbrg, bits, endian)

#define BAYER_CAPS_BITS(bits)			\
	BAYER_CAPS_ORD(bits, le)","		\
	BAYER_CAPS_ORD(bits, be)

#define BAYER_CAPS_ALL				\
	BAYER_CAPS_ORD(,)"," 			\
	BAYER_CAPS_BITS(10)","			\
	BAYER_CAPS_BITS(12)","			\
	BAYER_CAPS_BITS(14)","			\
	BAYER_CAPS_BITS(16)

#define	SRC_CAPS                                 \
  GST_VIDEO_CAPS_MAKE ("{ RGBx, xRGB, BGRx, xBGR, RGBA, ARGB, BGRA, ABGR, " \
  "RGBA64_LE, ARGB64_LE, BGRA64_LE, ABGR64_LE, " \
  "RGBA64_BE, ARGB64_BE, BGRA64_BE, ABGR64_BE }")

#define SINK_CAPS "video/x-bayer,format=(string){" BAYER_CAPS_ALL " }, "\
  "width=(int)[1,MAX],height=(int)[1,MAX],framerate=(fraction)[0/1,MAX]"

enum
{
  PROP_0
};

GType gst_bayer2rgb_get_type (void);

#define gst_bayer2rgb_parent_class parent_class
G_DEFINE_TYPE (GstBayer2RGB, gst_bayer2rgb, GST_TYPE_BASE_TRANSFORM);
GST_ELEMENT_REGISTER_DEFINE (bayer2rgb, "bayer2rgb", GST_RANK_NONE,
    gst_bayer2rgb_get_type ());

static void gst_bayer2rgb_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_bayer2rgb_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_bayer2rgb_set_caps (GstBaseTransform * filter,
    GstCaps * incaps, GstCaps * outcaps);
static GstFlowReturn gst_bayer2rgb_transform (GstBaseTransform * base,
    GstBuffer * inbuf, GstBuffer * outbuf);
static void gst_bayer2rgb_reset (GstBayer2RGB * filter);
static GstCaps *gst_bayer2rgb_transform_caps (GstBaseTransform * base,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter);
static gboolean gst_bayer2rgb_get_unit_size (GstBaseTransform * base,
    GstCaps * caps, gsize * size);


static void
gst_bayer2rgb_class_init (GstBayer2RGBClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->set_property = gst_bayer2rgb_set_property;
  gobject_class->get_property = gst_bayer2rgb_get_property;

  gst_element_class_set_static_metadata (gstelement_class,
      "Bayer to RGB decoder for cameras", "Filter/Converter/Video",
      "Converts video/x-bayer to video/x-raw",
      "William Brack <wbrack@mmm.com.hk>");

  gst_element_class_add_pad_template (gstelement_class,
      gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
          gst_caps_from_string (SRC_CAPS)));
  gst_element_class_add_pad_template (gstelement_class,
      gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
          gst_caps_from_string (SINK_CAPS)));

  GST_BASE_TRANSFORM_CLASS (klass)->transform_caps =
      GST_DEBUG_FUNCPTR (gst_bayer2rgb_transform_caps);
  GST_BASE_TRANSFORM_CLASS (klass)->get_unit_size =
      GST_DEBUG_FUNCPTR (gst_bayer2rgb_get_unit_size);
  GST_BASE_TRANSFORM_CLASS (klass)->set_caps =
      GST_DEBUG_FUNCPTR (gst_bayer2rgb_set_caps);
  GST_BASE_TRANSFORM_CLASS (klass)->transform =
      GST_DEBUG_FUNCPTR (gst_bayer2rgb_transform);

  GST_DEBUG_CATEGORY_INIT (gst_bayer2rgb_debug, "bayer2rgb", 0,
      "bayer2rgb element");
}

static void
gst_bayer2rgb_init (GstBayer2RGB * filter)
{
  gst_bayer2rgb_reset (filter);
  gst_base_transform_set_in_place (GST_BASE_TRANSFORM (filter), FALSE);
}

/* No properties are implemented, so only a warning is produced */
static void
gst_bayer2rgb_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{

  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_bayer2rgb_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{

  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_bayer2rgb_set_caps (GstBaseTransform * base, GstCaps * incaps,
    GstCaps * outcaps)
{
  GstBayer2RGB *bayer2rgb = GST_BAYER2RGB (base);
  GstStructure *structure;
  const char *format;
  GstVideoInfo info;

  GST_DEBUG ("in caps %" GST_PTR_FORMAT " out caps %" GST_PTR_FORMAT, incaps,
      outcaps);

  structure = gst_caps_get_structure (incaps, 0);

  gst_structure_get_int (structure, "width", &bayer2rgb->width);
  gst_structure_get_int (structure, "height", &bayer2rgb->height);

  format = gst_structure_get_string (structure, "format");
  if (g_str_has_prefix (format, "bggr")) {
    bayer2rgb->format = GST_BAYER_2_RGB_FORMAT_BGGR;
  } else if (g_str_has_prefix (format, "gbrg")) {
    bayer2rgb->format = GST_BAYER_2_RGB_FORMAT_GBRG;
  } else if (g_str_has_prefix (format, "grbg")) {
    bayer2rgb->format = GST_BAYER_2_RGB_FORMAT_GRBG;
  } else if (g_str_has_prefix (format, "rggb")) {
    bayer2rgb->format = GST_BAYER_2_RGB_FORMAT_RGGB;
  } else {
    return FALSE;
  }

  if (strlen (format) == 4) {   /* 8bit bayer */
    bayer2rgb->bpp = 8;
  } else if (strlen (format) == 8) {    /* 10/12/14/16 le/be bayer */
    bayer2rgb->bpp = (gint) g_ascii_strtoull (format + 4, NULL, 10);
    if (bayer2rgb->bpp & 1)     /* odd bayer2rgb->bpp bayer formats not supported */
      return FALSE;
    if (bayer2rgb->bpp < 10 || bayer2rgb->bpp > 16)     /* bayer 10,12,14,16 only */
      return FALSE;

    if (g_str_has_suffix (format, "le"))
      bayer2rgb->bigendian = 0;
    else if (g_str_has_suffix (format, "be"))
      bayer2rgb->bigendian = 1;
    else
      return FALSE;
  } else
    return FALSE;

  /* To cater for different RGB formats, we need to set params for later */
  gst_video_info_from_caps (&info, outcaps);
  bayer2rgb->r_off =
      GST_VIDEO_INFO_COMP_OFFSET (&info,
      0) / DIV_ROUND_UP (GST_VIDEO_INFO_COMP_DEPTH (&info, 0), 8);
  bayer2rgb->g_off =
      GST_VIDEO_INFO_COMP_OFFSET (&info,
      1) / DIV_ROUND_UP (GST_VIDEO_INFO_COMP_DEPTH (&info, 1), 8);
  bayer2rgb->b_off =
      GST_VIDEO_INFO_COMP_OFFSET (&info,
      2) / DIV_ROUND_UP (GST_VIDEO_INFO_COMP_DEPTH (&info, 2), 8);

  bayer2rgb->info = info;

  return TRUE;
}

static void
gst_bayer2rgb_reset (GstBayer2RGB * filter)
{
  filter->width = 0;
  filter->height = 0;
  filter->r_off = 0;
  filter->g_off = 0;
  filter->b_off = 0;
  filter->bpp = 8;
  filter->bigendian = 0;
  gst_video_info_init (&filter->info);
}

static GstCaps *
gst_bayer2rgb_transform_caps (GstBaseTransform * base,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter)
{
  GstBayer2RGB *bayer2rgb;
  GstCaps *res_caps, *tmp_caps;
  GstStructure *structure;
  guint i, caps_size;

  bayer2rgb = GST_BAYER2RGB (base);

  res_caps = gst_caps_copy (caps);
  caps_size = gst_caps_get_size (res_caps);
  for (i = 0; i < caps_size; i++) {
    structure = gst_caps_get_structure (res_caps, i);
    if (direction == GST_PAD_SINK) {
      gst_structure_set_name (structure, "video/x-raw");
      gst_structure_remove_field (structure, "format");
    } else {
      gst_structure_set_name (structure, "video/x-bayer");
      gst_structure_remove_fields (structure, "format", "colorimetry",
          "chroma-site", NULL);
    }
  }
  if (filter) {
    tmp_caps = res_caps;
    res_caps =
        gst_caps_intersect_full (filter, tmp_caps, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (tmp_caps);
  }
  GST_DEBUG_OBJECT (bayer2rgb, "transformed %" GST_PTR_FORMAT " into %"
      GST_PTR_FORMAT, caps, res_caps);
  return res_caps;
}

static gboolean
gst_bayer2rgb_get_unit_size (GstBaseTransform * base, GstCaps * caps,
    gsize * size)
{
  GstStructure *structure;
  GstBayer2RGB *bayer2rgb;
  int width;
  int height;
  const char *name;

  structure = gst_caps_get_structure (caps, 0);
  bayer2rgb = GST_BAYER2RGB (base);

  if (gst_structure_get_int (structure, "width", &width) &&
      gst_structure_get_int (structure, "height", &height)) {
    name = gst_structure_get_name (structure);
    /* Our name must be either video/x-bayer video/x-raw */
    if (strcmp (name, "video/x-raw")) {
      *size =
          GST_ROUND_UP_4 (width) * height * DIV_ROUND_UP (bayer2rgb->bpp, 8);
      return TRUE;
    } else {
      /* For output, calculate according to format */
      *size = width * height * DIV_ROUND_UP (bayer2rgb->bpp, 8);
      return TRUE;
    }

  }
  GST_ELEMENT_ERROR (base, CORE, NEGOTIATION, (NULL),
      ("Incomplete caps, some required field missing"));
  return FALSE;
}

static void
gst_bayer2rgb8_split_and_upsample_horiz (guint8 * dest0, guint8 * dest1,
    const guint8 * src, GstBayer2RGB * bayer2rgb)
{
  int n = bayer2rgb->width;
  int i;

  /*
   * Pre-process line of source data in 'src' into two neighboring
   * lines of temporary data 'dest0' and 'dest1' as follows. Note
   * that first two pixels of both 'dest0' and 'dest1' are special,
   * and so are the last two pixels of both 'dest0' and 'dest1' .
   *
   * The gist of this transformation is this:
   * - Assume the source data contain this BG bayer pattern on input
   *   data line 0 (this is the same as src[0, 1, 2, 3, 4, 5 ...]):
   *   src 0 : B0     G0          B1          G1          B2          G2
   * - This gets transformed into two lines:
   *   line 0: B0 avg(B0, B1)     B1      avg(B1, B2)     B2      avg(B2, B3)...
   *   line 1: G0     G0      avg(G0, G1)     G1      avg(G1, G2)     G2...
   * - Notice  ^^     ^^      ^^^^^^^^^^^     ^^      ^^^^^^^^^^^
   *   These are interpolated BG pairs, one for each input bayer pixel.
   *
   * The immediatelly following call to this function operates on the next
   * input data line 1 as follows:
   * - Assume the source data contain this GR bayer pattern on line 1:
   *   src 0 : G0     R0          G1          R1          G2          R2
   * - This gets transformed into two lines:
   *   line 2: G0 avg(G0, G1)     G1      avg(G1, G2)     G2      avg(G2, G3)...
   *   line 3: R0     R0      avg(R0, R1)     R1      avg(R1, R2)     R2...
   * - Notice  ^^     ^^      ^^^^^^^^^^^     ^^      ^^^^^^^^^^^
   *   These are interpolated GR pairs, one for each input bayer pixel.
   *
   * First two pixels are special, two more pixels as an example of the rest:
   *       ||   0    |       1        ||       2        |       3        ::
   * ------||--------+----------------++----------------+----------------+:
   * DEST0 || src[0] | avg(src[0, 2]) ||     src[2]     | avg(src[2, 4]) ::
   * ------||--------+----------------++----------------+----------------+:
   * DEST1 || src[1] |     src[1]     || avg(src[1, 3]) |     src[3]     ::
   * ------||--------+----------------++----------------+----------------+:
   *
   * Inner block of pixels:
   * :       |         n          |        n+1       :
   * :-------+--------------------+------------------:
   * : DEST0 |       src[n]       | avg(src[n, n+2]) :
   * :-------+--------------------+------------------:
   * : DEST1 | avg(src[n-1, n+1]) |     src[n+1]     :
   * :-------+--------------------+------------------:
   *
   * Last two pixels:
   *           w is EVEN                          w is ODD
   * :       |    w-2   |    w-1   ||   :       |    w-2   |    w-1   ||
   * :-------+----------+----------||   :-------+----------+----------||
   * : DEST0 | src[w-2] | src[w-2] ||   : DEST0 | src[w-3] | src[w-1] ||
   * :-------+----------+----------||   :-------+----------+----------||
   * : DEST1 | src[w-3] | src[w-1] ||   : DEST1 | src[w-2] | src[w-2] ||
   * :-------+----------+----------||   :-------+----------+----------||
   */

  dest0[0] = src[0];
  dest1[0] = src[1];
  dest0[1] = (src[0] + src[2] + 1) >> 1;
  dest1[1] = src[1];

#if defined(__i386__) || defined(__amd64__)
  bayer_orc_horiz_upsample_unaligned (dest0 + 2, dest1 + 2, src + 1,
      (n - 4) >> 1);
#else
  bayer_orc_horiz_upsample (dest0 + 2, dest1 + 2, src + 2, (n - 4) >> 1);
#endif

  for (i = n - 2; i < n; i++) {
    if ((i & 1) == 0) {
      dest0[i] = src[i];
      dest1[i] = src[i - 1];
    } else {
      dest0[i] = src[i - 1];
      dest1[i] = src[i];
    }
  }
}

static guint16
gswab16 (guint16 val, guint8 swap)
{
  if (swap) {
    return GUINT16_FROM_BE (val);
  } else {
    return val;
  }
}

static void
gst_bayer2rgb16_split_and_upsample_horiz (guint16 * dest0, guint16 * dest1,
    const guint16 * src, GstBayer2RGB * bayer2rgb)
{
  int swap = bayer2rgb->bigendian;
  int n = bayer2rgb->width;
  int i;

  dest0[0] = gswab16 (src[0], swap);
  dest1[0] = gswab16 (src[1], swap);
  dest0[1] = (gswab16 (src[0], swap) + gswab16 (src[2], swap) + 1) >> 1;
  dest1[1] = gswab16 (src[1], swap);

  if (swap) {
    bayer16_orc_horiz_upsample_be (dest0 + 2, dest1 + 2, src + 1, (n - 4) >> 1);
  } else {
    bayer16_orc_horiz_upsample_le (dest0 + 2, dest1 + 2, src + 1, (n - 4) >> 1);
  }

  for (i = n - 2; i < n; i++) {
    if ((i & 1) == 0) {
      dest0[i] = gswab16 (src[i], swap);
      dest1[i] = gswab16 (src[i - 1], swap);
    } else {
      dest0[i] = gswab16 (src[i - 1], swap);
      dest1[i] = gswab16 (src[i], swap);
    }
  }
}

static void
gst_bayer2rgb_split_and_upsample_horiz (guint8 * dest0, guint8 * dest1,
    const guint8 * src, GstBayer2RGB * bayer2rgb)
{
  if (bayer2rgb->bpp == 8) {
    gst_bayer2rgb8_split_and_upsample_horiz (dest0, dest1, src, bayer2rgb);
  } else {
    gst_bayer2rgb16_split_and_upsample_horiz ((guint16 *) dest0,
        (guint16 *) dest1, (const guint16 *) src, bayer2rgb);
  }
}

typedef void (*process_func) (guint8 * d0, const guint8 * s0, const guint8 * s1,
    const guint8 * s2, const guint8 * s3, const guint8 * s4, const guint8 * s5,
    int n);

typedef void (*process_func16) (guint16 * d0, guint16 * d1, const guint8 * s0,
    const guint8 * s1, const guint8 * s2, const guint8 * s3, const guint8 * s4,
    const guint8 * s5, int n);

#define LINE(t, x, b) ((t) + (((x) & 7) * ((b)->width * DIV_ROUND_UP((b)->bpp, 8))))

static void
gst_bayer2rgb_process (GstBayer2RGB * bayer2rgb, uint8_t * dest,
    int dest_stride, uint8_t * src)
{
  const int src_stride =
      GST_ROUND_UP_4 (bayer2rgb->width) * DIV_ROUND_UP (bayer2rgb->bpp, 8);
  const int bayersrc16 = bayer2rgb->bpp > 8;
  int j;
  guint8 *tmp;
  guint32 *dtmp;
  process_func merge[2] = { NULL, NULL };
  process_func16 merge16[2] = { NULL, NULL };
  int r_off, g_off, b_off;

  /*
   * Handle emission of either RGBA64 or RGBA (32bpp) . The default is
   * emission of RGBA64 in case the input bayer data are >8 bit, since
   * there is no loss of precision that way.
   *
   * The emission of RGBA (32bpp) as done here is done by shifting the
   * debayered data by the bpp-8 bits right, to fit into the 8 bits per
   * channel output buffer. This retains precision during calculation,
   * and the calculation is a bit more expensive in terms of CPU cycles
   * and memory. An alternative approach would be to downgrade the input
   * bayer data in gst_bayer2rgb16_split_and_upsample_horiz() already,
   * and then perform this second part of debayering as if those input
   * data were 8bpp bayer data. This would increase speed, but decrease
   * precision.
   */
  const int bayerdst16 = (dest_stride / bayer2rgb->width / 4) == 2;

  /* We exploit some symmetry in the functions here.  The base functions
   * are all named for the BGGR arrangement.  For RGGB, we swap the
   * red offset and blue offset in the output.  For GRBG, we swap the
   * order of the merge functions.  For GBRG, do both. */
  r_off = bayer2rgb->r_off;
  g_off = bayer2rgb->g_off;
  b_off = bayer2rgb->b_off;
  if (bayer2rgb->format == GST_BAYER_2_RGB_FORMAT_RGGB ||
      bayer2rgb->format == GST_BAYER_2_RGB_FORMAT_GBRG) {
    r_off = bayer2rgb->b_off;
    b_off = bayer2rgb->r_off;
  }

  if (r_off == 2 && g_off == 1 && b_off == 0) {
    merge[0] = bayer_orc_merge_bg_bgra;
    merge[1] = bayer_orc_merge_gr_bgra;
    merge16[0] = bayer16_orc_merge_bg_bgra;
    merge16[1] = bayer16_orc_merge_gr_bgra;
  } else if (r_off == 3 && g_off == 2 && b_off == 1) {
    merge[0] = bayer_orc_merge_bg_abgr;
    merge[1] = bayer_orc_merge_gr_abgr;
    merge16[0] = bayer16_orc_merge_bg_abgr;
    merge16[1] = bayer16_orc_merge_gr_abgr;
  } else if (r_off == 1 && g_off == 2 && b_off == 3) {
    merge[0] = bayer_orc_merge_bg_argb;
    merge[1] = bayer_orc_merge_gr_argb;
    merge16[0] = bayer16_orc_merge_bg_argb;
    merge16[1] = bayer16_orc_merge_gr_argb;
  } else if (r_off == 0 && g_off == 1 && b_off == 2) {
    merge[0] = bayer_orc_merge_bg_rgba;
    merge[1] = bayer_orc_merge_gr_rgba;
    merge16[0] = bayer16_orc_merge_bg_rgba;
    merge16[1] = bayer16_orc_merge_gr_rgba;
  }
  if (bayer2rgb->format == GST_BAYER_2_RGB_FORMAT_GRBG ||
      bayer2rgb->format == GST_BAYER_2_RGB_FORMAT_GBRG) {
    process_func tmp = merge[0];
    merge[0] = merge[1];
    merge[1] = tmp;
    process_func16 tmp16 = merge16[0];
    merge16[0] = merge16[1];
    merge16[1] = tmp16;
  }

  tmp = g_malloc (DIV_ROUND_UP (bayer2rgb->bpp, 8) * 2 * 4 * bayer2rgb->width);

  if (bayersrc16 || bayerdst16)
    dtmp = g_malloc (sizeof (*dtmp) * 2 * bayer2rgb->width);

  /* Pre-process source line 1 into bottom two lines 6 and 7 as PREVIOUS line */
  gst_bayer2rgb_split_and_upsample_horiz (      /* src line 1 */
      LINE (tmp, 3 * 2 + 0, bayer2rgb), /* tmp buffer line 6 */
      LINE (tmp, 3 * 2 + 1, bayer2rgb), /* tmp buffer line 7 */
      src + 1 * src_stride, bayer2rgb);

  /* Pre-process source line 0 into top two lines 0 and 1 as CURRENT line */
  gst_bayer2rgb_split_and_upsample_horiz (      /* src line 0 */
      LINE (tmp, 0 * 2 + 0, bayer2rgb), /* tmp buffer line 0 */
      LINE (tmp, 0 * 2 + 1, bayer2rgb), /* tmp buffer line 1 */
      src + 0 * src_stride, bayer2rgb);

  for (j = 0; j < bayer2rgb->height; j++) {
    if (j < bayer2rgb->height - 1) {
      /*
       * Pre-process NEXT source line (j + 1) into two consecutive lines
       * (2 * (j + 1) + 0) % 8
       * and
       * (2 * (j + 1) + 1) % 8
       *
       * This cycle here starts with j=0, and therefore
       * - reads source line 1
       * - writes tmp buffer lines 2,3
       * The cycle continues with source line 2 and tmp buffer lines 4,5 etc.
       */
      gst_bayer2rgb_split_and_upsample_horiz (  /* src line (j + 1) */
          LINE (tmp, (j + 1) * 2 + 0, bayer2rgb),       /* tmp buffer line 2/4/6/0 */
          LINE (tmp, (j + 1) * 2 + 1, bayer2rgb),       /* tmp buffer line 3/5/7/1 */
          src + (j + 1) * src_stride, bayer2rgb);
    }

    /*
     * Use the pre-processed tmp buffer lines and construct resulting frame.
     * Assume j=0, the tmp buffer content looks as follows:
     *   line 0: B0 avg(B0, B1)     B1      avg(B1, B2)     B2      avg(B2, B3)...
     *   line 1: G0     G0      avg(G0, G1)     G1      avg(G1, G2)     G2...
     *   line 2: G0 avg(G0, G1)     G1      avg(G1, G2)     G2      avg(G2, G3)...
     *   line 3: R0     R0      avg(R0, R1)     R1      avg(R1, R2)     R2...
     *   line 4: empty
     *   line 5: empty
     *   line 6: G0 avg(G0, G1)     G1      avg(G1, G2)     G2      avg(G2, G3)...
     *   line 7: R0     R0      avg(R0, R1)     R1      avg(R1, R2)     R2...
     * Line 0,1 and 6,7 were populated in pre-process step outside of this loop.
     * Line 2,3 was populated just above this comment.
     * The code is currently processing source line 0 (not tmp buffer line 0)
     * and uses the following tmp buffer lines in the process as inputs to the
     * merge function:
     * - tmp buffer line -2 => tmp buffer line 6 => orc g0 values
     * - tmp buffer line -1 => tmp buffer line 7 => orc r0 values
     * - tmp buffer line +0 => tmp buffer line 0 => orc b1 values
     * - tmp buffer line +1 => tmp buffer line 1 => orc g1 values
     * - tmp buffer line +2 => tmp buffer line 2 => orc g2 values
     * - tmp buffer line +3 => tmp buffer line 3 => orc r2 values
     * With j=0, the merge function used is one of bayer_orc_merge_bg_*
     *
     * A good material regarding the ORC functions below is
     * https://www.siliconimaging.com/RGB%20Bayer.htm
     * chapter
     * "Interpolating the green component"
     *
     * The bayer_orc_merge_bg_* performs BG interpolation.
     *   # average R from PREVIOUS line and NEXT line
     *   r = [ avg(r0[0], r2[0]) , avg(r0[1], r2[1]) ]
     *   # average G from PREVIOUS line and NEXT line
     *   g = [ avg(g0[0], g2[0]) , avg(g0[1], g2[1]) ]
     *   # copy CURRENT line G into variable t
     *   t = [ g1[0]             , g1[1]             ]
     *   # average G from PREVIOUS, CURRENT, NEXT line
     *   g = [ avg(g[0], g1[0])  , avg(g[1], g1[1])  ]
     *   # reorder the content of g and t variables using bit operations
     *   # (the "g first, t second" order is here because the B pixel we
     *   #  are now processing does not have its own G value, it only has
     *   #  G value extrapolated from surrouding G pixels. The following
     *   #  G pixel has its own G value, so we use it as-is, without any
     *   #  extrapolation)
     *   g = [ g, 0 ]
     *   t = [ 0, t ]
     *   g = [ g, t ]
     *   # generate resulting (e.g. BGRx) data
     *   bg = [ b1[0] , g   , b1[1] , t   ]
     *   ra = [ r[0]  , 255 , r[1]  , 255 ]
     *   d  = [ b1[0] , g   , r[0]  , 255 , b1[1] , t , r[1] , 255  ]
     *
     * In the next cycle, j=1, line 4,5 would be populated with BG values
     * calculated from source line 2, merge function would use tmp buffer
     * inputs from lines 0,1,2,3,4,5 i.e. b0,g0,g1,r1,b2,g2 and the merge
     * function would be bayer_orc_merge_gr_* .
     */
    if (bayersrc16) {
      merge16[j & 1] ((guint16 *) dtmp, /* temporary buffer BG */
          (guint16 *) (dtmp + bayer2rgb->width),        /* temporary buffer GR */
          LINE (tmp, j * 2 - 2, bayer2rgb),     /* PREVIOUS: even: BG g0 , odd: GR b0 */
          LINE (tmp, j * 2 - 1, bayer2rgb),     /* PREVIOUS: even: BG r0 , odd: GR g0 */
          LINE (tmp, j * 2 + 0, bayer2rgb),     /* CURRENT: even: BG b1 , odd: GR g1 */
          LINE (tmp, j * 2 + 1, bayer2rgb),     /* CURRENT: even: BG g1 , odd: GR r1 */
          LINE (tmp, j * 2 + 2, bayer2rgb),     /* NEXT: even: BG g2 , odd: GR b2 */
          LINE (tmp, j * 2 + 3, bayer2rgb),     /* NEXT: even: BG r2 , odd: GR g2 */
          bayer2rgb->width >> 1);

      if (bayerdst16)
        bayer16to16_orc_reorder (dest + j * dest_stride,
            dtmp, dtmp + bayer2rgb->width, bayer2rgb->bpp, bayer2rgb->width);
      else
        bayer16to8_orc_reorder (dest + j * dest_stride,
            dtmp, dtmp + bayer2rgb->width, bayer2rgb->bpp - 8,
            bayer2rgb->width);
    } else {
      merge[j & 1] (bayerdst16 ? (guint8 *) dtmp : (dest + j * dest_stride),    /* output line j */
          LINE (tmp, j * 2 - 2, bayer2rgb),     /* PREVIOUS: even: BG g0 , odd: GR b0 */
          LINE (tmp, j * 2 - 1, bayer2rgb),     /* PREVIOUS: even: BG r0 , odd: GR g0 */
          LINE (tmp, j * 2 + 0, bayer2rgb),     /* CURRENT: even: BG b1 , odd: GR g1 */
          LINE (tmp, j * 2 + 1, bayer2rgb),     /* CURRENT: even: BG g1 , odd: GR r1 */
          LINE (tmp, j * 2 + 2, bayer2rgb),     /* NEXT: even: BG g2 , odd: GR b2 */
          LINE (tmp, j * 2 + 3, bayer2rgb),     /* NEXT: even: BG r2 , odd: GR g2 */
          bayer2rgb->width >> 1);
      if (bayerdst16)
        bayer8to16_orc_reorder (dest + j * dest_stride, dtmp, bayer2rgb->width);
    }
  }

  if (bayersrc16)
    g_free (dtmp);
  g_free (tmp);
}




static GstFlowReturn
gst_bayer2rgb_transform (GstBaseTransform * base, GstBuffer * inbuf,
    GstBuffer * outbuf)
{
  GstBayer2RGB *filter = GST_BAYER2RGB (base);
  GstMapInfo map;
  uint8_t *output;
  GstVideoFrame frame;

  GST_DEBUG ("transforming buffer");

  if (!gst_buffer_map (inbuf, &map, GST_MAP_READ))
    goto map_failed;

  if (!gst_video_frame_map (&frame, &filter->info, outbuf, GST_MAP_WRITE)) {
    gst_buffer_unmap (inbuf, &map);
    goto map_failed;
  }

  output = GST_VIDEO_FRAME_PLANE_DATA (&frame, 0);
  gst_bayer2rgb_process (filter, output, frame.info.stride[0], map.data);

  gst_video_frame_unmap (&frame);
  gst_buffer_unmap (inbuf, &map);

  return GST_FLOW_OK;

map_failed:
  GST_WARNING_OBJECT (base, "Could not map buffer, skipping");
  return GST_FLOW_OK;
}
