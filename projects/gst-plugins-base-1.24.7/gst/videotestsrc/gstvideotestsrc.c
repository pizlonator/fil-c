/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 * Copyright (C) <2002> David A. Schleef <ds@schleef.org>
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

/**
 * SECTION:element-videotestsrc
 * @title: videotestsrc
 *
 * The videotestsrc element is used to produce test video data in a wide variety
 * of formats. The video test data produced can be controlled with the "pattern"
 * property.
 *
 * By default the videotestsrc will generate data indefinitely, but if the
 * #GstBaseSrc:num-buffers property is non-zero it will instead generate a
 * fixed number of video frames and then send EOS.
 *
 * ## Example launch line
 * |[
 * gst-launch-1.0 -v videotestsrc pattern=snow ! video/x-raw,width=1280,height=720 ! autovideosink
 * ]|
 *  Shows random noise in a video window.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "gstvideotestsrc.h"
#include "gstvideotestsrcorc.h"
#include "videotestsrc.h"

#include <string.h>
#include <stdlib.h>

GST_DEBUG_CATEGORY_STATIC (video_test_src_debug);
#define GST_CAT_DEFAULT video_test_src_debug

#define DEFAULT_PATTERN            GST_VIDEO_TEST_SRC_SMPTE
#define DEFAULT_ANIMATION_MODE     GST_VIDEO_TEST_SRC_FRAMES
#define DEFAULT_MOTION_TYPE        GST_VIDEO_TEST_SRC_WAVY
#define DEFAULT_FLIP               FALSE
#define DEFAULT_TIMESTAMP_OFFSET   0
#define DEFAULT_IS_LIVE            FALSE
#define DEFAULT_COLOR_SPEC         GST_VIDEO_TEST_SRC_BT601
#define DEFAULT_FOREGROUND_COLOR   0xffffffff
#define DEFAULT_BACKGROUND_COLOR   0xff000000
#define DEFAULT_HORIZONTAL_SPEED   0

enum
{
  PROP_0,
  PROP_PATTERN,
  PROP_TIMESTAMP_OFFSET,
  PROP_IS_LIVE,
  PROP_K0,
  PROP_KX,
  PROP_KY,
  PROP_KT,
  PROP_KXT,
  PROP_KYT,
  PROP_KXY,
  PROP_KX2,
  PROP_KY2,
  PROP_KT2,
  PROP_XOFFSET,
  PROP_YOFFSET,
  PROP_FOREGROUND_COLOR,
  PROP_BACKGROUND_COLOR,
  PROP_HORIZONTAL_SPEED,
  PROP_ANIMATION_MODE,
  PROP_MOTION_TYPE,
  PROP_FLIP,
  PROP_LAST
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

#define VTS_VIDEO_CAPS GST_VIDEO_CAPS_MAKE (GST_VIDEO_FORMATS_ALL) ","	\
  "multiview-mode = { mono, left, right }"                              \
  ";" \
  "video/x-bayer, format=(string) {" BAYER_CAPS_ALL " },"\
  "width = " GST_VIDEO_SIZE_RANGE ", "                                 \
  "height = " GST_VIDEO_SIZE_RANGE ", "                                \
  "framerate = " GST_VIDEO_FPS_RANGE ", "                              \
  "multiview-mode = { mono, left, right }"


static GstStaticPadTemplate gst_video_test_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (VTS_VIDEO_CAPS)
    );

#define gst_video_test_src_parent_class parent_class
G_DEFINE_TYPE (GstVideoTestSrc, gst_video_test_src, GST_TYPE_PUSH_SRC);
GST_ELEMENT_REGISTER_DEFINE (videotestsrc, "videotestsrc",
    GST_RANK_NONE, GST_TYPE_VIDEO_TEST_SRC);

static void gst_video_test_src_finalize (GObject * object);

static void gst_video_test_src_set_pattern (GstVideoTestSrc * videotestsrc,
    int pattern_type);
static void gst_video_test_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_video_test_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_video_test_src_setcaps (GstBaseSrc * bsrc, GstCaps * caps);
static GstCaps *gst_video_test_src_src_fixate (GstBaseSrc * bsrc,
    GstCaps * caps);

static gboolean gst_video_test_src_is_seekable (GstBaseSrc * psrc);
static gboolean gst_video_test_src_do_seek (GstBaseSrc * bsrc,
    GstSegment * segment);
static gboolean gst_video_test_src_query (GstBaseSrc * bsrc, GstQuery * query);

static void gst_video_test_src_get_times (GstBaseSrc * basesrc,
    GstBuffer * buffer, GstClockTime * start, GstClockTime * end);
static gboolean gst_video_test_src_decide_allocation (GstBaseSrc * bsrc,
    GstQuery * query);
static GstFlowReturn gst_video_test_src_fill (GstPushSrc * psrc,
    GstBuffer * buffer);
static gboolean gst_video_test_src_start (GstBaseSrc * basesrc);
static gboolean gst_video_test_src_stop (GstBaseSrc * basesrc);

#define GST_TYPE_VIDEO_TEST_SRC_PATTERN (gst_video_test_src_pattern_get_type ())
static GType
gst_video_test_src_pattern_get_type (void)
{
  static GType video_test_src_pattern_type = 0;
  static const GEnumValue pattern_types[] = {
    {GST_VIDEO_TEST_SRC_SMPTE, "SMPTE 100% color bars", "smpte"},
    {GST_VIDEO_TEST_SRC_SNOW, "Random (television snow)", "snow"},
    {GST_VIDEO_TEST_SRC_BLACK, "100% Black", "black"},
    {GST_VIDEO_TEST_SRC_WHITE, "100% White", "white"},
    {GST_VIDEO_TEST_SRC_RED, "Red", "red"},
    {GST_VIDEO_TEST_SRC_GREEN, "Green", "green"},
    {GST_VIDEO_TEST_SRC_BLUE, "Blue", "blue"},
    {GST_VIDEO_TEST_SRC_CHECKERS1, "Checkers 1px", "checkers-1"},
    {GST_VIDEO_TEST_SRC_CHECKERS2, "Checkers 2px", "checkers-2"},
    {GST_VIDEO_TEST_SRC_CHECKERS4, "Checkers 4px", "checkers-4"},
    {GST_VIDEO_TEST_SRC_CHECKERS8, "Checkers 8px", "checkers-8"},
    {GST_VIDEO_TEST_SRC_CIRCULAR, "Circular", "circular"},
    {GST_VIDEO_TEST_SRC_BLINK, "Blink", "blink"},
    {GST_VIDEO_TEST_SRC_SMPTE75, "SMPTE 75% color bars", "smpte75"},
    {GST_VIDEO_TEST_SRC_ZONE_PLATE, "Zone plate", "zone-plate"},
    {GST_VIDEO_TEST_SRC_GAMUT, "Gamut checkers", "gamut"},
    {GST_VIDEO_TEST_SRC_CHROMA_ZONE_PLATE, "Chroma zone plate",
        "chroma-zone-plate"},
    {GST_VIDEO_TEST_SRC_SOLID, "Solid color", "solid-color"},
    {GST_VIDEO_TEST_SRC_BALL, "Moving ball", "ball"},
    {GST_VIDEO_TEST_SRC_SMPTE100, "SMPTE 100% color bars", "smpte100"},
    {GST_VIDEO_TEST_SRC_BAR, "Bar", "bar"},
    {GST_VIDEO_TEST_SRC_PINWHEEL, "Pinwheel", "pinwheel"},
    {GST_VIDEO_TEST_SRC_SPOKES, "Spokes", "spokes"},
    {GST_VIDEO_TEST_SRC_GRADIENT, "Gradient", "gradient"},
    {GST_VIDEO_TEST_SRC_COLORS, "Colors", "colors"},
    {GST_VIDEO_TEST_SRC_SMPTE_RP_219, "SMPTE test pattern, RP 219 conformant",
        "smpte-rp-219"},
    {0, NULL, NULL}
  };

  if (!video_test_src_pattern_type) {
    video_test_src_pattern_type =
        g_enum_register_static ("GstVideoTestSrcPattern", pattern_types);
  }
  return video_test_src_pattern_type;
}


/*"animation-mode", which can
 * take the following values: frames (current behaviour that should stay the
 * default), running time, wall clock time.
 */

#define GST_TYPE_VIDEO_TEST_SRC_ANIMATION_MODE (gst_video_test_src_animation_mode_get_type ())
static GType
gst_video_test_src_animation_mode_get_type (void)
{
  static GType video_test_src_animation_mode = 0;
  static const GEnumValue animation_modes[] = {

    {GST_VIDEO_TEST_SRC_FRAMES, "frame count", "frames"},
    {GST_VIDEO_TEST_SRC_WALL_TIME, "wall clock time", "wall-time"},
    {GST_VIDEO_TEST_SRC_RUNNING_TIME, "running time", "running-time"},
    {0, NULL, NULL}
  };

  if (!video_test_src_animation_mode) {
    video_test_src_animation_mode =
        g_enum_register_static ("GstVideoTestSrcAnimationMode",
        animation_modes);
  }
  return video_test_src_animation_mode;
}


#define GST_TYPE_VIDEO_TEST_SRC_MOTION_TYPE (gst_video_test_src_motion_type_get_type ())
static GType
gst_video_test_src_motion_type_get_type (void)
{
  static GType video_test_src_motion_type = 0;
  static const GEnumValue motion_types[] = {
    {GST_VIDEO_TEST_SRC_WAVY, "Ball waves back and forth, up and down",
        "wavy"},
    {GST_VIDEO_TEST_SRC_SWEEP, "1 revolution per second", "sweep"},
    {GST_VIDEO_TEST_SRC_HSWEEP, "1/2 revolution per second, then reset to top",
        "hsweep"},
    {0, NULL, NULL}
  };

  if (!video_test_src_motion_type) {
    video_test_src_motion_type =
        g_enum_register_static ("GstVideoTestSrcMotionType", motion_types);
  }
  return video_test_src_motion_type;
}


static void
gst_video_test_src_class_init (GstVideoTestSrcClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseSrcClass *gstbasesrc_class;
  GstPushSrcClass *gstpushsrc_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstbasesrc_class = (GstBaseSrcClass *) klass;
  gstpushsrc_class = (GstPushSrcClass *) klass;

  gobject_class->set_property = gst_video_test_src_set_property;
  gobject_class->get_property = gst_video_test_src_get_property;
  gobject_class->finalize = gst_video_test_src_finalize;

  g_object_class_install_property (gobject_class, PROP_PATTERN,
      g_param_spec_enum ("pattern", "Pattern",
          "Type of test pattern to generate", GST_TYPE_VIDEO_TEST_SRC_PATTERN,
          DEFAULT_PATTERN, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_ANIMATION_MODE,
      g_param_spec_enum ("animation-mode", "Animation mode",
          "For pattern=ball, which counter defines the position of the ball.",
          GST_TYPE_VIDEO_TEST_SRC_ANIMATION_MODE, DEFAULT_ANIMATION_MODE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_MOTION_TYPE,
      g_param_spec_enum ("motion", "Motion",
          "For pattern=ball, what motion the ball does",
          GST_TYPE_VIDEO_TEST_SRC_MOTION_TYPE, DEFAULT_MOTION_TYPE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_FLIP,
      g_param_spec_boolean ("flip", "Flip",
          "For pattern=ball, invert colors every second.",
          DEFAULT_FLIP, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_TIMESTAMP_OFFSET,
      g_param_spec_int64 ("timestamp-offset", "Timestamp offset",
          "An offset added to timestamps set on buffers (in ns)", 0,
          G_MAXINT64, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_IS_LIVE,
      g_param_spec_boolean ("is-live", "Is Live",
          "Whether to act as a live source", DEFAULT_IS_LIVE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_K0,
      g_param_spec_int ("k0", "Zoneplate zero order phase",
          "Zoneplate zero order phase, for generating plain fields or phase offsets",
          G_MININT32, G_MAXINT32, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_KX,
      g_param_spec_int ("kx", "Zoneplate 1st order x phase",
          "Zoneplate 1st order x phase, for generating constant horizontal frequencies",
          G_MININT32, G_MAXINT32, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_KY,
      g_param_spec_int ("ky", "Zoneplate 1st order y phase",
          "Zoneplate 1st order y phase, for generating content vertical frequencies",
          G_MININT32, G_MAXINT32, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_KT,
      g_param_spec_int ("kt", "Zoneplate 1st order t phase",
          "Zoneplate 1st order t phase, for generating phase rotation as a function of time",
          G_MININT32, G_MAXINT32, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_KXT,
      g_param_spec_int ("kxt", "Zoneplate x*t product phase",
          "Zoneplate x*t product phase, normalised to kxy/256 cycles per vertical pixel at width/2 from origin",
          G_MININT32, G_MAXINT32, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_KYT,
      g_param_spec_int ("kyt", "Zoneplate y*t product phase",
          "Zoneplate y*t product phase", G_MININT32, G_MAXINT32, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_KXY,
      g_param_spec_int ("kxy", "Zoneplate x*y product phase",
          "Zoneplate x*y product phase", G_MININT32, G_MAXINT32, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_KX2,
      g_param_spec_int ("kx2", "Zoneplate 2nd order x phase",
          "Zoneplate 2nd order x phase, normalised to kx2/256 cycles per horizontal pixel at width/2 from origin",
          G_MININT32, G_MAXINT32, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_KY2,
      g_param_spec_int ("ky2", "Zoneplate 2nd order y phase",
          "Zoneplate 2nd order y phase, normailsed to ky2/256 cycles per vertical pixel at height/2 from origin",
          G_MININT32, G_MAXINT32, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_KT2,
      g_param_spec_int ("kt2", "Zoneplate 2nd order t phase",
          "Zoneplate 2nd order t phase, t*t/256 cycles per picture", G_MININT32,
          G_MAXINT32, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_XOFFSET,
      g_param_spec_int ("xoffset", "Zoneplate 2nd order products x offset",
          "Zoneplate 2nd order products x offset", G_MININT32, G_MAXINT32, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_YOFFSET,
      g_param_spec_int ("yoffset", "Zoneplate 2nd order products y offset",
          "Zoneplate 2nd order products y offset", G_MININT32, G_MAXINT32, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  /**
   * GstVideoTestSrc:foreground-color
   *
   * Color to use for solid-color pattern and foreground color of other
   * patterns.  Default is white (0xffffffff).
   */
  g_object_class_install_property (gobject_class, PROP_FOREGROUND_COLOR,
      g_param_spec_uint ("foreground-color", "Foreground Color",
          "Foreground color to use (big-endian ARGB)", 0, G_MAXUINT32,
          DEFAULT_FOREGROUND_COLOR,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  /**
   * GstVideoTestSrc:background-color
   *
   * Color to use for background color of some patterns.  Default is
   * black (0xff000000).
   */
  g_object_class_install_property (gobject_class, PROP_BACKGROUND_COLOR,
      g_param_spec_uint ("background-color", "Background Color",
          "Background color to use (big-endian ARGB)", 0, G_MAXUINT32,
          DEFAULT_BACKGROUND_COLOR,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_HORIZONTAL_SPEED,
      g_param_spec_int ("horizontal-speed", "Horizontal Speed",
          "Scroll image number of pixels per frame (positive is scroll to the left)",
          G_MININT32, G_MAXINT32, DEFAULT_HORIZONTAL_SPEED,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_set_static_metadata (gstelement_class,
      "Video test source", "Source/Video",
      "Creates a test video stream", "David A. Schleef <ds@schleef.org>");

  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_video_test_src_template);

  gstbasesrc_class->set_caps = gst_video_test_src_setcaps;
  gstbasesrc_class->fixate = gst_video_test_src_src_fixate;
  gstbasesrc_class->is_seekable = gst_video_test_src_is_seekable;
  gstbasesrc_class->do_seek = gst_video_test_src_do_seek;
  gstbasesrc_class->query = gst_video_test_src_query;
  gstbasesrc_class->get_times = gst_video_test_src_get_times;
  gstbasesrc_class->start = gst_video_test_src_start;
  gstbasesrc_class->stop = gst_video_test_src_stop;
  gstbasesrc_class->decide_allocation = gst_video_test_src_decide_allocation;

  gstpushsrc_class->fill = gst_video_test_src_fill;

  gst_type_mark_as_plugin_api (GST_TYPE_VIDEO_TEST_SRC_ANIMATION_MODE, 0);
  gst_type_mark_as_plugin_api (GST_TYPE_VIDEO_TEST_SRC_MOTION_TYPE, 0);
  gst_type_mark_as_plugin_api (GST_TYPE_VIDEO_TEST_SRC_PATTERN, 0);
}

static void
gst_video_test_src_init (GstVideoTestSrc * src)
{
  gst_video_test_src_set_pattern (src, DEFAULT_PATTERN);

  src->timestamp_offset = DEFAULT_TIMESTAMP_OFFSET;
  src->foreground_color = DEFAULT_FOREGROUND_COLOR;
  src->background_color = DEFAULT_BACKGROUND_COLOR;
  src->horizontal_speed = DEFAULT_HORIZONTAL_SPEED;
  src->random_state = 0;

  /* we operate in time */
  gst_base_src_set_format (GST_BASE_SRC (src), GST_FORMAT_TIME);
  gst_base_src_set_live (GST_BASE_SRC (src), DEFAULT_IS_LIVE);

  src->animation_mode = DEFAULT_ANIMATION_MODE;
  src->motion_type = DEFAULT_MOTION_TYPE;
  src->flip = DEFAULT_FLIP;

  g_mutex_init (&src->cache_lock);
}

static void
gst_video_test_src_finalize (GObject * object)
{
  GstVideoTestSrc *src = GST_VIDEO_TEST_SRC (object);
  g_mutex_clear (&src->cache_lock);
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static GstCaps *
gst_video_test_src_src_fixate (GstBaseSrc * bsrc, GstCaps * caps)
{
  GstVideoTestSrc *src = GST_VIDEO_TEST_SRC (bsrc);
  GstStructure *structure;

  /* Check if foreground color has alpha, if it is the case,
   * force color format with an alpha channel downstream */
  if (src->foreground_color >> 24 != 255) {
    guint i;
    GstCaps *alpha_only_caps = gst_caps_new_empty ();

    for (i = 0; i < gst_caps_get_size (caps); i++) {
      const GstVideoFormatInfo *info;
      const GValue *formats =
          gst_structure_get_value (gst_caps_get_structure (caps, i),
          "format");

      if (GST_VALUE_HOLDS_LIST (formats)) {
        GValue possible_formats = { 0, };
        guint list_size = gst_value_list_get_size (formats);
        guint index;

        g_value_init (&possible_formats, GST_TYPE_LIST);
        for (index = 0; index < list_size; index++) {
          const GValue *list_item = gst_value_list_get_value (formats, index);
          info =
              gst_video_format_get_info (gst_video_format_from_string
              (g_value_get_string (list_item)));
          if (GST_VIDEO_FORMAT_INFO_HAS_ALPHA (info)) {
            GValue tmp = { 0, };

            gst_value_init_and_copy (&tmp, list_item);
            gst_value_list_append_value (&possible_formats, &tmp);
          }
        }

        if (gst_value_list_get_size (&possible_formats)) {
          GstStructure *astruct =
              gst_structure_copy (gst_caps_get_structure (caps, i));

          gst_structure_set_value (astruct, "format", &possible_formats);
          gst_caps_append_structure (alpha_only_caps, astruct);
        }

      } else if (G_VALUE_HOLDS_STRING (formats)) {
        info =
            gst_video_format_get_info (gst_video_format_from_string
            (g_value_get_string (formats)));

        if (GST_VIDEO_FORMAT_INFO_HAS_ALPHA (info)) {
          gst_caps_append_structure (alpha_only_caps,
              gst_structure_copy (gst_caps_get_structure (caps, i)));
        }
      } else {
        g_assert_not_reached ();
        GST_WARNING ("Unexpected type for video 'format' field: %s",
            G_VALUE_TYPE_NAME (formats));
      }
    }

    if (gst_caps_is_empty (alpha_only_caps)) {
      GST_WARNING_OBJECT (src,
          "Foreground color contains alpha, but downstream can't support alpha.");
    } else {
      gst_caps_replace (&caps, alpha_only_caps);
    }
    gst_caps_unref (alpha_only_caps);
  }

  caps = gst_caps_make_writable (caps);
  structure = gst_caps_get_structure (caps, 0);

  gst_structure_fixate_field_nearest_int (structure, "width", 320);
  gst_structure_fixate_field_nearest_int (structure, "height", 240);

  if (gst_structure_has_field (structure, "framerate"))
    gst_structure_fixate_field_nearest_fraction (structure, "framerate", 30, 1);
  else
    gst_structure_set (structure, "framerate", GST_TYPE_FRACTION, 30, 1, NULL);

  if (gst_structure_has_field (structure, "pixel-aspect-ratio"))
    gst_structure_fixate_field_nearest_fraction (structure,
        "pixel-aspect-ratio", 1, 1);
  else
    gst_structure_set (structure, "pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1,
        NULL);

  if (gst_structure_has_field (structure, "colorimetry"))
    gst_structure_fixate_field_string (structure, "colorimetry", "bt601");
  if (gst_structure_has_field (structure, "chroma-site"))
    gst_structure_fixate_field_string (structure, "chroma-site", "mpeg2");

  if (gst_structure_has_field (structure, "interlace-mode"))
    gst_structure_fixate_field_string (structure, "interlace-mode",
        "progressive");
  else
    gst_structure_set (structure, "interlace-mode", G_TYPE_STRING,
        "progressive", NULL);

  if (gst_structure_has_field (structure, "multiview-mode"))
    gst_structure_fixate_field_string (structure, "multiview-mode",
        gst_video_multiview_mode_to_caps_string
        (GST_VIDEO_MULTIVIEW_MODE_MONO));
  else
    gst_structure_set (structure, "multiview-mode", G_TYPE_STRING,
        gst_video_multiview_mode_to_caps_string (GST_VIDEO_MULTIVIEW_MODE_MONO),
        NULL);

  caps = GST_BASE_SRC_CLASS (parent_class)->fixate (bsrc, caps);

  return caps;
}

static gboolean
gst_video_test_src_is_static_pattern (GstVideoTestSrc * videotestsrc)
{
  switch (videotestsrc->pattern_type) {
    case GST_VIDEO_TEST_SRC_SMPTE:
    case GST_VIDEO_TEST_SRC_SNOW:
    case GST_VIDEO_TEST_SRC_BLINK:
    case GST_VIDEO_TEST_SRC_BALL:
      return FALSE;

    case GST_VIDEO_TEST_SRC_ZONE_PLATE:
    case GST_VIDEO_TEST_SRC_CHROMA_ZONE_PLATE:
      if (videotestsrc->kxt != 0 || videotestsrc->kyt != 0 ||
          videotestsrc->kt != 0 || videotestsrc->kt2 != 0) {
        return FALSE;
      }
      break;
    default:
      break;
  }

  switch (videotestsrc->pattern_type) {
      /* Any pattern that's not a solid color is non-static
       * with horizontal motion */
    case GST_VIDEO_TEST_SRC_SMPTE:
    case GST_VIDEO_TEST_SRC_SNOW:
    case GST_VIDEO_TEST_SRC_CHECKERS1:
    case GST_VIDEO_TEST_SRC_CHECKERS2:
    case GST_VIDEO_TEST_SRC_CHECKERS4:
    case GST_VIDEO_TEST_SRC_CHECKERS8:
    case GST_VIDEO_TEST_SRC_CIRCULAR:
    case GST_VIDEO_TEST_SRC_BLINK:
    case GST_VIDEO_TEST_SRC_SMPTE75:
    case GST_VIDEO_TEST_SRC_ZONE_PLATE:
    case GST_VIDEO_TEST_SRC_GAMUT:
    case GST_VIDEO_TEST_SRC_CHROMA_ZONE_PLATE:
    case GST_VIDEO_TEST_SRC_BALL:
    case GST_VIDEO_TEST_SRC_SMPTE100:
    case GST_VIDEO_TEST_SRC_BAR:
    case GST_VIDEO_TEST_SRC_PINWHEEL:
    case GST_VIDEO_TEST_SRC_SPOKES:
    case GST_VIDEO_TEST_SRC_GRADIENT:
    case GST_VIDEO_TEST_SRC_COLORS:
      if (videotestsrc->horizontal_speed)
        return FALSE;
      break;
    default:
      break;
  }

  return TRUE;
}

static void
gst_video_test_src_set_pattern (GstVideoTestSrc * videotestsrc,
    int pattern_type)
{
  videotestsrc->pattern_type = pattern_type;

  GST_DEBUG_OBJECT (videotestsrc, "setting pattern to %d", pattern_type);

  switch (pattern_type) {
    case GST_VIDEO_TEST_SRC_SMPTE:
      videotestsrc->make_image = gst_video_test_src_smpte;
      break;
    case GST_VIDEO_TEST_SRC_SNOW:
      videotestsrc->make_image = gst_video_test_src_snow;
      break;
    case GST_VIDEO_TEST_SRC_BLACK:
      videotestsrc->make_image = gst_video_test_src_black;
      break;
    case GST_VIDEO_TEST_SRC_WHITE:
      videotestsrc->make_image = gst_video_test_src_white;
      break;
    case GST_VIDEO_TEST_SRC_RED:
      videotestsrc->make_image = gst_video_test_src_red;
      break;
    case GST_VIDEO_TEST_SRC_GREEN:
      videotestsrc->make_image = gst_video_test_src_green;
      break;
    case GST_VIDEO_TEST_SRC_BLUE:
      videotestsrc->make_image = gst_video_test_src_blue;
      break;
    case GST_VIDEO_TEST_SRC_CHECKERS1:
      videotestsrc->make_image = gst_video_test_src_checkers1;
      break;
    case GST_VIDEO_TEST_SRC_CHECKERS2:
      videotestsrc->make_image = gst_video_test_src_checkers2;
      break;
    case GST_VIDEO_TEST_SRC_CHECKERS4:
      videotestsrc->make_image = gst_video_test_src_checkers4;
      break;
    case GST_VIDEO_TEST_SRC_CHECKERS8:
      videotestsrc->make_image = gst_video_test_src_checkers8;
      break;
    case GST_VIDEO_TEST_SRC_CIRCULAR:
      videotestsrc->make_image = gst_video_test_src_circular;
      break;
    case GST_VIDEO_TEST_SRC_BLINK:
      videotestsrc->make_image = gst_video_test_src_blink;
      break;
    case GST_VIDEO_TEST_SRC_SMPTE75:
      videotestsrc->make_image = gst_video_test_src_smpte75;
      break;
    case GST_VIDEO_TEST_SRC_ZONE_PLATE:
      videotestsrc->make_image = gst_video_test_src_zoneplate;
      break;
    case GST_VIDEO_TEST_SRC_GAMUT:
      videotestsrc->make_image = gst_video_test_src_gamut;
      break;
    case GST_VIDEO_TEST_SRC_CHROMA_ZONE_PLATE:
      videotestsrc->make_image = gst_video_test_src_chromazoneplate;
      break;
    case GST_VIDEO_TEST_SRC_SOLID:
      videotestsrc->make_image = gst_video_test_src_solid;
      break;
    case GST_VIDEO_TEST_SRC_BALL:
      videotestsrc->make_image = gst_video_test_src_ball;
      break;
    case GST_VIDEO_TEST_SRC_SMPTE100:
      videotestsrc->make_image = gst_video_test_src_smpte100;
      break;
    case GST_VIDEO_TEST_SRC_BAR:
      videotestsrc->make_image = gst_video_test_src_bar;
      break;
    case GST_VIDEO_TEST_SRC_PINWHEEL:
      videotestsrc->make_image = gst_video_test_src_pinwheel;
      break;
    case GST_VIDEO_TEST_SRC_SPOKES:
      videotestsrc->make_image = gst_video_test_src_spokes;
      break;
    case GST_VIDEO_TEST_SRC_GRADIENT:
      videotestsrc->make_image = gst_video_test_src_gradient;
      break;
    case GST_VIDEO_TEST_SRC_COLORS:
      videotestsrc->make_image = gst_video_test_src_colors;
      break;
    case GST_VIDEO_TEST_SRC_SMPTE_RP_219:
      videotestsrc->make_image = gst_video_test_src_smpte_rp_219;
      break;
    default:
      g_assert_not_reached ();
  }
}

static void
gst_video_test_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstVideoTestSrc *src = GST_VIDEO_TEST_SRC (object);
  gboolean invalidate = TRUE;

  switch (prop_id) {
    case PROP_PATTERN:
      gst_video_test_src_set_pattern (src, g_value_get_enum (value));
      break;
    case PROP_TIMESTAMP_OFFSET:
      src->timestamp_offset = g_value_get_int64 (value);
      invalidate = FALSE;
      break;
    case PROP_IS_LIVE:
      gst_base_src_set_live (GST_BASE_SRC (src), g_value_get_boolean (value));
      invalidate = FALSE;
      break;
    case PROP_K0:
      src->k0 = g_value_get_int (value);
      break;
    case PROP_KX:
      src->kx = g_value_get_int (value);
      break;
    case PROP_KY:
      src->ky = g_value_get_int (value);
      break;
    case PROP_KT:
      src->kt = g_value_get_int (value);
      break;
    case PROP_KXT:
      src->kxt = g_value_get_int (value);
      break;
    case PROP_KYT:
      src->kyt = g_value_get_int (value);
      break;
    case PROP_KXY:
      src->kxy = g_value_get_int (value);
      break;
    case PROP_KX2:
      src->kx2 = g_value_get_int (value);
      break;
    case PROP_KY2:
      src->ky2 = g_value_get_int (value);
      break;
    case PROP_KT2:
      src->kt2 = g_value_get_int (value);
      break;
    case PROP_XOFFSET:
      src->xoffset = g_value_get_int (value);
      break;
    case PROP_YOFFSET:
      src->yoffset = g_value_get_int (value);
      break;
    case PROP_FOREGROUND_COLOR:
      src->foreground_color = g_value_get_uint (value);
      break;
    case PROP_BACKGROUND_COLOR:
      src->background_color = g_value_get_uint (value);
      break;
    case PROP_HORIZONTAL_SPEED:
      src->horizontal_speed = g_value_get_int (value);
      break;
    case PROP_ANIMATION_MODE:
      src->animation_mode = g_value_get_enum (value);
      break;
    case PROP_MOTION_TYPE:
      src->motion_type = g_value_get_enum (value);
      break;
    case PROP_FLIP:
      src->flip = g_value_get_boolean (value);
      break;
    default:
      break;
  }

  if (invalidate) {
    g_mutex_lock (&src->cache_lock);
    /* Property change invalidated the current pattern - check if it's static now or not */
    src->have_static_pattern = gst_video_test_src_is_static_pattern (src);
    gst_clear_buffer (&src->cached);
    g_mutex_unlock (&src->cache_lock);
  }
}

static void
gst_video_test_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstVideoTestSrc *src = GST_VIDEO_TEST_SRC (object);

  switch (prop_id) {
    case PROP_PATTERN:
      g_value_set_enum (value, src->pattern_type);
      break;
    case PROP_TIMESTAMP_OFFSET:
      g_value_set_int64 (value, src->timestamp_offset);
      break;
    case PROP_IS_LIVE:
      g_value_set_boolean (value, gst_base_src_is_live (GST_BASE_SRC (src)));
      break;
    case PROP_K0:
      g_value_set_int (value, src->k0);
      break;
    case PROP_KX:
      g_value_set_int (value, src->kx);
      break;
    case PROP_KY:
      g_value_set_int (value, src->ky);
      break;
    case PROP_KT:
      g_value_set_int (value, src->kt);
      break;
    case PROP_KXT:
      g_value_set_int (value, src->kxt);
      break;
    case PROP_KYT:
      g_value_set_int (value, src->kyt);
      break;
    case PROP_KXY:
      g_value_set_int (value, src->kxy);
      break;
    case PROP_KX2:
      g_value_set_int (value, src->kx2);
      break;
    case PROP_KY2:
      g_value_set_int (value, src->ky2);
      break;
    case PROP_KT2:
      g_value_set_int (value, src->kt2);
      break;
    case PROP_XOFFSET:
      g_value_set_int (value, src->xoffset);
      break;
    case PROP_YOFFSET:
      g_value_set_int (value, src->yoffset);
      break;
    case PROP_FOREGROUND_COLOR:
      g_value_set_uint (value, src->foreground_color);
      break;
    case PROP_BACKGROUND_COLOR:
      g_value_set_uint (value, src->background_color);
      break;
    case PROP_HORIZONTAL_SPEED:
      g_value_set_int (value, src->horizontal_speed);
      break;
    case PROP_ANIMATION_MODE:
      g_value_set_enum (value, src->animation_mode);
      break;
    case PROP_MOTION_TYPE:
      g_value_set_enum (value, src->motion_type);
      break;
    case PROP_FLIP:
      g_value_set_boolean (value, src->flip);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

#define DIV_ROUND_UP(s,v) (((s) + ((v)-1)) / (v))

static gboolean
gst_video_test_src_parse_caps (const GstCaps * caps, GstVideoInfo * info,
    GstVideoTestSrc * videotestsrc)
{
  const GstStructure *structure;
  gboolean ret;
  const GValue *framerate;
  const gchar *str;
  gint x_inv = 0, y_inv = 0, bpp = 0, bigendian = 0;

  GST_DEBUG ("parsing caps");

  gst_video_info_init (info);

  structure = gst_caps_get_structure (caps, 0);

  ret = gst_structure_get_int (structure, "width", &info->width);
  ret &= gst_structure_get_int (structure, "height", &info->height);
  framerate = gst_structure_get_value (structure, "framerate");

  if (framerate) {
    info->fps_n = gst_value_get_fraction_numerator (framerate);
    info->fps_d = gst_value_get_fraction_denominator (framerate);
  } else
    goto no_framerate;

  if ((str = gst_structure_get_string (structure, "colorimetry")))
    gst_video_colorimetry_from_string (&info->colorimetry, str);

  if ((str = gst_structure_get_string (structure, "format"))) {
    if (g_str_has_prefix (str, "bggr")) {
      x_inv = y_inv = 0;
    } else if (g_str_has_prefix (str, "rggb")) {
      x_inv = y_inv = 1;
    } else if (g_str_has_prefix (str, "grbg")) {
      x_inv = 0;
      y_inv = 1;
    } else if (g_str_has_prefix (str, "gbrg")) {
      x_inv = 1;
      y_inv = 0;
    } else
      goto invalid_format;

    if (strlen (str) == 4) {    /* 8bit bayer */
      bpp = 8;
    } else if (strlen (str) == 8) {     /* 10/12/14/16 le/be bayer */
      bpp = (gint) g_ascii_strtoull (str + 4, NULL, 10);
      if (bpp & 1)              /* odd bpp bayer formats not supported */
        goto invalid_format;
      if (bpp < 10 || bpp > 16) /* bayer 10,12,14,16 only */
        goto invalid_format;

      if (g_str_has_suffix (str, "le"))
        bigendian = 0;
      else if (g_str_has_suffix (str, "be"))
        bigendian = 1;
      else
        goto invalid_format;
    } else
      goto invalid_format;

    if (bpp == 8)
      info->finfo = gst_video_format_get_info (GST_VIDEO_FORMAT_GRAY8);
    else if (bigendian)
      info->finfo = gst_video_format_get_info (GST_VIDEO_FORMAT_GRAY16_BE);
    else
      info->finfo = gst_video_format_get_info (GST_VIDEO_FORMAT_GRAY16_LE);
  }

  videotestsrc->bayer = TRUE;
  videotestsrc->bpp = bpp;
  videotestsrc->x_invert = x_inv;
  videotestsrc->y_invert = y_inv;

  info->stride[0] = GST_ROUND_UP_4 (info->width) * DIV_ROUND_UP (bpp, 8);
  info->size = info->stride[0] * info->height;

  return ret;

  /* ERRORS */
no_framerate:
  {
    GST_DEBUG ("videotestsrc no framerate given");
    return FALSE;
  }
invalid_format:
  {
    GST_DEBUG ("videotestsrc invalid bayer format given");
    return FALSE;
  }
}

static gboolean
gst_video_test_src_decide_allocation (GstBaseSrc * bsrc, GstQuery * query)
{
  GstVideoTestSrc *videotestsrc;
  GstBufferPool *pool;
  gboolean update;
  guint size, min, max;
  GstStructure *config;
  GstCaps *caps = NULL;

  videotestsrc = GST_VIDEO_TEST_SRC (bsrc);

  if (gst_query_get_n_allocation_pools (query) > 0) {
    gst_query_parse_nth_allocation_pool (query, 0, &pool, &size, &min, &max);

    /* adjust size */
    size = MAX (size, videotestsrc->info.size);
    update = TRUE;
  } else {
    pool = NULL;
    size = videotestsrc->info.size;
    min = max = 0;
    update = FALSE;
  }

  /* no downstream pool, make our own */
  if (pool == NULL) {
    if (videotestsrc->bayer)
      pool = gst_buffer_pool_new ();
    else
      pool = gst_video_buffer_pool_new ();
  }

  config = gst_buffer_pool_get_config (pool);

  gst_query_parse_allocation (query, &caps, NULL);
  if (caps)
    gst_buffer_pool_config_set_params (config, caps, size, min, max);

  if (gst_query_find_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL)) {
    gst_buffer_pool_config_add_option (config,
        GST_BUFFER_POOL_OPTION_VIDEO_META);
  }
  gst_buffer_pool_set_config (pool, config);

  if (update)
    gst_query_set_nth_allocation_pool (query, 0, pool, size, min, max);
  else
    gst_query_add_allocation_pool (query, pool, size, min, max);

  if (pool)
    gst_object_unref (pool);

  return GST_BASE_SRC_CLASS (parent_class)->decide_allocation (bsrc, query);
}

static gboolean
gst_video_test_src_setcaps (GstBaseSrc * bsrc, GstCaps * caps)
{
  const GstStructure *structure;
  GstVideoTestSrc *videotestsrc;
  GstVideoInfo info;
  guint i;
  guint n_lines;
  gint offset;

  videotestsrc = GST_VIDEO_TEST_SRC (bsrc);

  structure = gst_caps_get_structure (caps, 0);

  GST_OBJECT_LOCK (videotestsrc);

  if (gst_structure_has_name (structure, "video/x-raw")) {
    /* we can use the parsing code */
    if (!gst_video_info_from_caps (&info, caps))
      goto parse_failed;

  } else if (gst_structure_has_name (structure, "video/x-bayer")) {
    if (!gst_video_test_src_parse_caps (caps, &info, videotestsrc))
      goto parse_failed;
  } else {
    goto unsupported_caps;
  }

  /* create chroma subsampler */
  if (videotestsrc->subsample)
    gst_video_chroma_resample_free (videotestsrc->subsample);
  videotestsrc->subsample = gst_video_chroma_resample_new (0,
      info.chroma_site, 0, info.finfo->unpack_format, -info.finfo->w_sub[2],
      -info.finfo->h_sub[2]);

  for (i = 0; i < videotestsrc->n_lines; i++)
    g_free (videotestsrc->lines[i]);
  g_free (videotestsrc->lines);

  if (videotestsrc->subsample != NULL) {
    gst_video_chroma_resample_get_info (videotestsrc->subsample,
        &n_lines, &offset);
  } else {
    n_lines = 1;
    offset = 0;
  }

  videotestsrc->lines = g_malloc (sizeof (gpointer) * n_lines);
  for (i = 0; i < n_lines; i++)
    videotestsrc->lines[i] = g_malloc ((info.width + 16) * 8);
  videotestsrc->n_lines = n_lines;
  videotestsrc->offset = offset;

  /* looks ok here */
  videotestsrc->info = info;

  GST_DEBUG_OBJECT (videotestsrc, "size %dx%d, %d/%d fps",
      info.width, info.height, info.fps_n, info.fps_d);

  g_free (videotestsrc->tmpline);
  g_free (videotestsrc->tmpline2);
  g_free (videotestsrc->tmpline_u8);
  g_free (videotestsrc->tmpline_u16);
  videotestsrc->tmpline_u8 = g_malloc (info.width + 8);
  videotestsrc->tmpline = g_malloc ((info.width + 8) * 4);
  videotestsrc->tmpline2 = g_malloc ((info.width + 8) * 4);
  videotestsrc->tmpline_u16 = g_malloc ((info.width + 16) * 8);

  videotestsrc->accum_rtime += videotestsrc->running_time;
  videotestsrc->accum_frames += videotestsrc->n_frames;

  videotestsrc->running_time = 0;
  videotestsrc->n_frames = 0;

  g_mutex_lock (&videotestsrc->cache_lock);
  gst_clear_buffer (&videotestsrc->cached);
  g_mutex_unlock (&videotestsrc->cache_lock);

  GST_OBJECT_UNLOCK (videotestsrc);

  return TRUE;

  /* ERRORS */
parse_failed:
  {
    GST_OBJECT_UNLOCK (videotestsrc);
    GST_DEBUG_OBJECT (bsrc, "failed to parse caps");
    return FALSE;
  }
unsupported_caps:
  {
    GST_OBJECT_UNLOCK (videotestsrc);
    GST_DEBUG_OBJECT (bsrc, "unsupported caps: %" GST_PTR_FORMAT, caps);
    return FALSE;
  }
}

static gboolean
gst_video_test_src_query (GstBaseSrc * bsrc, GstQuery * query)
{
  gboolean res = FALSE;
  GstVideoTestSrc *src;

  src = GST_VIDEO_TEST_SRC (bsrc);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CONVERT:
    {
      GstFormat src_fmt, dest_fmt;
      gint64 src_val, dest_val;

      GST_OBJECT_LOCK (src);
      gst_query_parse_convert (query, &src_fmt, &src_val, &dest_fmt, &dest_val);
      res =
          gst_video_info_convert (&src->info, src_fmt, src_val, dest_fmt,
          &dest_val);
      gst_query_set_convert (query, src_fmt, src_val, dest_fmt, dest_val);
      GST_OBJECT_UNLOCK (src);
      break;
    }
    case GST_QUERY_LATENCY:
    {
      GST_OBJECT_LOCK (src);
      if (src->info.fps_n > 0) {
        GstClockTime latency;

        latency =
            gst_util_uint64_scale (GST_SECOND, src->info.fps_d,
            src->info.fps_n);
        GST_OBJECT_UNLOCK (src);
        gst_query_set_latency (query,
            gst_base_src_is_live (GST_BASE_SRC_CAST (src)), latency,
            GST_CLOCK_TIME_NONE);
        GST_DEBUG_OBJECT (src, "Reporting latency of %" GST_TIME_FORMAT,
            GST_TIME_ARGS (latency));
        res = TRUE;
      } else {
        GST_OBJECT_UNLOCK (src);
      }
      break;
    }
    case GST_QUERY_DURATION:{
      if (bsrc->num_buffers != -1) {
        GstFormat format;

        gst_query_parse_duration (query, &format, NULL);
        switch (format) {
          case GST_FORMAT_TIME:{
            gint64 dur;

            GST_OBJECT_LOCK (src);
            if (src->info.fps_n) {
              dur = gst_util_uint64_scale_int_round (bsrc->num_buffers
                  * GST_SECOND, src->info.fps_d, src->info.fps_n);
              res = TRUE;
              gst_query_set_duration (query, GST_FORMAT_TIME, dur);
            }
            GST_OBJECT_UNLOCK (src);
            goto done;
          }
          case GST_FORMAT_BYTES:
            GST_OBJECT_LOCK (src);
            res = TRUE;
            gst_query_set_duration (query, GST_FORMAT_BYTES,
                bsrc->num_buffers * src->info.size);
            GST_OBJECT_UNLOCK (src);
            goto done;
          default:
            break;
        }
      }
      /* fall through */
    }
    default:
      res = GST_BASE_SRC_CLASS (parent_class)->query (bsrc, query);
      break;
  }
done:
  return res;
}

static void
gst_video_test_src_get_times (GstBaseSrc * basesrc, GstBuffer * buffer,
    GstClockTime * start, GstClockTime * end)
{
  /* for live sources, sync on the timestamp of the buffer */
  if (gst_base_src_is_live (basesrc)) {
    GstClockTime timestamp = GST_BUFFER_PTS (buffer);

    if (GST_CLOCK_TIME_IS_VALID (timestamp)) {
      /* get duration to calculate end time */
      GstClockTime duration = GST_BUFFER_DURATION (buffer);

      if (GST_CLOCK_TIME_IS_VALID (duration)) {
        *end = timestamp + duration;
      }
      *start = timestamp;
    }
  } else {
    *start = -1;
    *end = -1;
  }
}

static gboolean
gst_video_test_src_do_seek (GstBaseSrc * bsrc, GstSegment * segment)
{
  GstClockTime position;
  GstVideoTestSrc *src;

  src = GST_VIDEO_TEST_SRC (bsrc);

  segment->time = segment->start;
  position = segment->position;
  src->reverse = segment->rate < 0;

  /* now move to the position indicated */
  if (src->info.fps_n) {
    src->n_frames = gst_util_uint64_scale (position,
        src->info.fps_n, src->info.fps_d * GST_SECOND);
  } else {
    src->n_frames = 0;
  }
  src->accum_frames = 0;
  src->accum_rtime = 0;
  if (src->info.fps_n) {
    src->running_time = gst_util_uint64_scale (src->n_frames,
        src->info.fps_d * GST_SECOND, src->info.fps_n);
  } else {
    /* FIXME : Not sure what to set here */
    src->running_time = 0;
  }

  g_assert (src->running_time <= position);

  return TRUE;
}

static gboolean
gst_video_test_src_is_seekable (GstBaseSrc * psrc)
{
  /* we're seekable... */
  return TRUE;
}

static GstFlowReturn
fill_image (GstPushSrc * psrc, GstBuffer * buffer)
{
  GstVideoTestSrc *src;
  GstVideoFrame frame;
  gconstpointer pal;
  gsize palsize;

  src = GST_VIDEO_TEST_SRC (psrc);

  if (G_UNLIKELY (GST_VIDEO_INFO_FORMAT (&src->info) ==
          GST_VIDEO_FORMAT_UNKNOWN))
    goto not_negotiated;

  /* 0 framerate and we are at the second frame, eos */
  if (G_UNLIKELY (src->info.fps_n == 0 && src->n_frames == 1))
    goto eos;

  if (G_UNLIKELY (src->n_frames == -1)) {
    /* EOS for reverse playback */
    goto eos;
  }

  if (!gst_video_frame_map (&frame, &src->info, buffer, GST_MAP_WRITE))
    goto invalid_frame;

  src->make_image (src, GST_BUFFER_PTS (buffer), &frame);

  if ((pal = gst_video_format_get_palette (GST_VIDEO_FRAME_FORMAT (&frame),
              &palsize))) {
    memcpy (GST_VIDEO_FRAME_PLANE_DATA (&frame, 1), pal, palsize);
  }

  gst_video_frame_unmap (&frame);
  return GST_FLOW_OK;

not_negotiated:
  {
    return GST_FLOW_NOT_NEGOTIATED;
  }
eos:
  {
    GST_DEBUG_OBJECT (src, "eos: 0 framerate, frame %d", (gint) src->n_frames);
    return GST_FLOW_EOS;
  }
invalid_frame:
  {
    GST_DEBUG_OBJECT (src, "invalid frame");
    return GST_FLOW_OK;
  }
}

static GstFlowReturn
gst_video_test_src_fill (GstPushSrc * psrc, GstBuffer * buffer)
{
  GstVideoTestSrc *src = GST_VIDEO_TEST_SRC (psrc);
  GstClockTime next_time;
  GstFlowReturn ret;

  GstClockTime pts =
      src->accum_rtime + src->timestamp_offset + src->running_time;

  gst_object_sync_values (GST_OBJECT (src), pts);

  if (src->have_static_pattern) {
    GstVideoFrame sframe, dframe;

    g_mutex_lock (&src->cache_lock);
    if (src->cached == NULL) {
      src->cached = gst_buffer_new_allocate (NULL, src->info.size, NULL);

      ret = fill_image (GST_PUSH_SRC (src), src->cached);
      if (G_UNLIKELY (ret != GST_FLOW_OK)) {
        g_mutex_unlock (&src->cache_lock);
        goto fill_failed;
      }
    } else {
      GST_LOG_OBJECT (src, "Reusing cached pattern buffer");
    }

    /* Do a memory copy instead of just passing a reference to this buffer to
     * be consistent with other sources. This should make things clear for
     * cases where downstream cannot queue the same buffer twice (such as v4l2)
     */
    gst_video_frame_map (&sframe, &src->info, src->cached, GST_MAP_READ);
    gst_video_frame_map (&dframe, &src->info, buffer, GST_MAP_WRITE);

    if (!gst_video_frame_copy (&dframe, &sframe)) {
      g_mutex_unlock (&src->cache_lock);
      goto copy_failed;
    }

    gst_video_frame_unmap (&sframe);
    gst_video_frame_unmap (&dframe);
    g_mutex_unlock (&src->cache_lock);
  } else {
    ret = fill_image (GST_PUSH_SRC (src), buffer);
    if (G_UNLIKELY (ret != GST_FLOW_OK))
      goto fill_failed;
  }

  GST_BUFFER_PTS (buffer) = pts;
  GST_BUFFER_DTS (buffer) = GST_CLOCK_TIME_NONE;

  GST_DEBUG_OBJECT (src, "Timestamp: %" GST_TIME_FORMAT " = accumulated %"
      GST_TIME_FORMAT " + offset: %"
      GST_TIME_FORMAT " + running time: %" GST_TIME_FORMAT,
      GST_TIME_ARGS (GST_BUFFER_PTS (buffer)), GST_TIME_ARGS (src->accum_rtime),
      GST_TIME_ARGS (src->timestamp_offset), GST_TIME_ARGS (src->running_time));

  GST_BUFFER_OFFSET (buffer) = src->accum_frames + src->n_frames;
  if (src->reverse) {
    src->n_frames--;
  } else {
    src->n_frames++;
  }
  GST_BUFFER_OFFSET_END (buffer) = GST_BUFFER_OFFSET (buffer) + 1;
  if (src->info.fps_n) {
    next_time = gst_util_uint64_scale (src->n_frames,
        src->info.fps_d * GST_SECOND, src->info.fps_n);
    if (src->reverse) {
      /* We already decremented to next frame */
      GstClockTime prev_pts = gst_util_uint64_scale (src->n_frames + 2,
          src->info.fps_d * GST_SECOND, src->info.fps_n);

      GST_BUFFER_DURATION (buffer) = prev_pts - GST_BUFFER_PTS (buffer);
    } else {
      GST_BUFFER_DURATION (buffer) = next_time - src->running_time;
    }
  } else {
    next_time = src->timestamp_offset;
    /* NONE means forever */
    GST_BUFFER_DURATION (buffer) = GST_CLOCK_TIME_NONE;
  }

  src->running_time = next_time;

  return GST_FLOW_OK;

fill_failed:
  {
    GST_DEBUG_OBJECT (src, "fill returned %d (%s)", ret,
        gst_flow_get_name (ret));
    return ret;
  }
copy_failed:
  {
    GST_DEBUG_OBJECT (src, "Failed to copy cached buffer");
    return GST_FLOW_ERROR;
  }
}


static gboolean
gst_video_test_src_start (GstBaseSrc * basesrc)
{
  GstVideoTestSrc *src = GST_VIDEO_TEST_SRC (basesrc);

  GST_OBJECT_LOCK (src);
  src->running_time = 0;
  src->n_frames = 0;
  src->accum_frames = 0;
  src->accum_rtime = 0;

  gst_video_info_init (&src->info);
  GST_OBJECT_UNLOCK (src);

  return TRUE;
}

static gboolean
gst_video_test_src_stop (GstBaseSrc * basesrc)
{
  GstVideoTestSrc *src = GST_VIDEO_TEST_SRC (basesrc);
  guint i;

  g_free (src->tmpline);
  src->tmpline = NULL;
  g_free (src->tmpline2);
  src->tmpline2 = NULL;
  g_free (src->tmpline_u8);
  src->tmpline_u8 = NULL;
  g_free (src->tmpline_u16);
  src->tmpline_u16 = NULL;
  if (src->subsample)
    gst_video_chroma_resample_free (src->subsample);
  src->subsample = NULL;

  for (i = 0; i < src->n_lines; i++)
    g_free (src->lines[i]);
  g_free (src->lines);
  src->n_lines = 0;
  src->lines = NULL;

  g_mutex_lock (&src->cache_lock);
  gst_clear_buffer (&src->cached);
  g_mutex_unlock (&src->cache_lock);

  return TRUE;
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (video_test_src_debug, "videotestsrc", 0,
      "Video Test Source");

  return GST_ELEMENT_REGISTER (videotestsrc, plugin);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    videotestsrc,
    "Creates a test video stream",
    plugin_init, VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
