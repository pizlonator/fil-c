/* GStreamer
 * Copyright (C) <2016> Carlos Rafael Giani <dv at pseudoterminal dot org>
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
 * SECTION:element-rawvideoparse
 * @title: rawvideoparse
 *
 * This element parses incoming data as raw video frames and timestamps these.
 * It also handles seek queries in said raw video data, and ensures that output
 * buffers contain exactly one frame, even if the input buffers contain only
 * partial frames or multiple frames. In the former case, it will continue to
 * receive buffers until there is enough input data to output one frame. In the
 * latter case, it will extract the first frame in the buffer and output it, then
 * the second one etc. until the remaining unparsed bytes aren't enough to form
 * a complete frame, and it will then continue as described in the earlier case.
 *
 * If the properties configuration is used, plane strides and offsets will be
 * computed by using gst_video_info_set_format(). This can be overridden by passing
 * GstValueArrays to the plane-offsets and plane-strides properties. When this is
 * done, these custom offsets and strides are used later even if new width,
 * height, format etc. property values might be set. To switch back to computed
 * plane strides & offsets, pass NULL to one or both of the plane-offset and
 * plane-array properties.
 *
 * The frame size property is useful in cases where there is extra data between
 * the frames (for example, trailing metadata, or headers). The parser calculates
 * the actual frame size out of the other properties and compares it with this
 * frame-size value. If the frame size is larger than the calculated size,
 * then the extra bytes after the end of the frame are skipped. For example, with
 * 8-bit grayscale frames and a actual frame size of 100x10 pixels and a frame-size of
 * 1500 bytes, there are 500 excess bytes at the end of the actual frame which
 * are then skipped. It is safe to set the frame size to a value that is smaller
 * than the actual frame size (in fact, its default value is 0); if it is smaller,
 * then no trailing data will be skipped.
 *
 * If a framerate of 0 Hz is set (for example, 0/1), then output buffers will have
 * no duration set. The first output buffer will have a PTS 0, all subsequent ones
 * an unset PTS.
 *
 * ## Example pipelines
 * |[
 * gst-launch-1.0 filesrc location=video.raw ! rawvideoparse use-sink-caps=false \
 *         width=500 height=400 format=y444 ! autovideosink
 * ]|
 *  Read raw data from a local file and parse it as video data with 500x400 pixels
 * and Y444 video format.
 * |[
 * gst-launch-1.0 filesrc location=video.raw ! queue ! "video/x-raw, width=320, \
 *         height=240, format=I420, framerate=1/1" ! rawvideoparse \
 *         use-sink-caps=true ! autovideosink
 * ]|
 *  Read raw data from a local file and parse it as video data with 320x240 pixels
 * and I420 video format. The queue element here is to force push based scheduling.
 *
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <string.h>
#include "gstrawparseelements.h"
#include "gstrawvideoparse.h"
#include "unalignedvideo.h"

GST_DEBUG_CATEGORY_STATIC (raw_video_parse_debug);
#define GST_CAT_DEFAULT raw_video_parse_debug

enum
{
  PROP_0,
  PROP_WIDTH,
  PROP_HEIGHT,
  PROP_FORMAT,
  PROP_PIXEL_ASPECT_RATIO,
  PROP_FRAMERATE,
  PROP_INTERLACED,
  PROP_TOP_FIELD_FIRST,
  PROP_PLANE_STRIDES,
  PROP_PLANE_OFFSETS,
  PROP_FRAME_SIZE,
  PROP_COLORIMETRY,
};

#define DEFAULT_WIDTH                 320
#define DEFAULT_HEIGHT                240
#define DEFAULT_FORMAT                GST_VIDEO_FORMAT_I420
#define DEFAULT_PIXEL_ASPECT_RATIO_N  1
#define DEFAULT_PIXEL_ASPECT_RATIO_D  1
#define DEFAULT_FRAMERATE_N           25
#define DEFAULT_FRAMERATE_D           1
#define DEFAULT_INTERLACED            FALSE
#define DEFAULT_TOP_FIELD_FIRST       FALSE
#define DEFAULT_FRAME_STRIDE          0

#define GST_RAW_VIDEO_PARSE_CAPS \
        GST_VIDEO_CAPS_MAKE(GST_VIDEO_FORMATS_ALL) "; "

static GstStaticPadTemplate static_sink_template =
    GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_UNALIGNED_RAW_VIDEO_CAPS "; " GST_RAW_VIDEO_PARSE_CAPS)
    );

static GstStaticPadTemplate static_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_RAW_VIDEO_PARSE_CAPS)
    );

#define gst_raw_video_parse_parent_class parent_class
G_DEFINE_TYPE (GstRawVideoParse, gst_raw_video_parse, GST_TYPE_RAW_BASE_PARSE);
GST_ELEMENT_REGISTER_DEFINE (rawvideoparse, "rawvideoparse",
    GST_RANK_NONE, GST_TYPE_RAW_VIDEO_PARSE);

static void gst_raw_video_parse_set_property (GObject * object, guint prop_id,
    GValue const *value, GParamSpec * pspec);
static void gst_raw_video_parse_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_raw_video_parse_stop (GstBaseParse * parse);

static gboolean gst_raw_video_parse_set_current_config (GstRawBaseParse *
    raw_base_parse, GstRawBaseParseConfig config);
static GstRawBaseParseConfig
gst_raw_video_parse_get_current_config (GstRawBaseParse * raw_base_parse);
static gboolean gst_raw_video_parse_set_config_from_caps (GstRawBaseParse *
    raw_base_parse, GstRawBaseParseConfig config, GstCaps * caps);
static gboolean gst_raw_video_parse_get_caps_from_config (GstRawBaseParse *
    raw_base_parse, GstRawBaseParseConfig config, GstCaps ** caps);
static gsize gst_raw_video_parse_get_config_frame_size (GstRawBaseParse *
    raw_base_parse, GstRawBaseParseConfig config);
static guint gst_raw_video_parse_get_max_frames_per_buffer (GstRawBaseParse *
    raw_base_parse, GstRawBaseParseConfig config);
static gboolean gst_raw_video_parse_is_config_ready (GstRawBaseParse *
    raw_base_parse, GstRawBaseParseConfig config);
static gboolean gst_raw_video_parse_process (GstRawBaseParse * raw_base_parse,
    GstRawBaseParseConfig config, GstBuffer * in_data, gsize total_num_in_bytes,
    gsize num_valid_in_bytes, GstBuffer ** processed_data);
static gboolean gst_raw_video_parse_is_unit_format_supported (GstRawBaseParse *
    raw_base_parse, GstFormat format);
static void gst_raw_video_parse_get_units_per_second (GstRawBaseParse *
    raw_base_parse, GstFormat format, GstRawBaseParseConfig config,
    gsize * units_per_sec_n, gsize * units_per_sec_d);

static gint gst_raw_video_parse_get_overhead_size (GstRawBaseParse *
    raw_base_parse, GstRawBaseParseConfig config);

static gboolean gst_raw_video_parse_is_using_sink_caps (GstRawVideoParse *
    raw_video_parse);
static GstRawVideoParseConfig
    * gst_raw_video_parse_get_config_ptr (GstRawVideoParse * raw_video_parse,
    GstRawBaseParseConfig config);

static void gst_raw_video_parse_init_config (GstRawVideoParseConfig * config);
static void gst_raw_video_parse_update_info (GstRawVideoParseConfig * config);

static void
gst_raw_video_parse_class_init (GstRawVideoParseClass * klass)
{
  GObjectClass *object_class;
  GstElementClass *element_class;
  GstBaseParseClass *baseparse_class;
  GstRawBaseParseClass *rawbaseparse_class;

  GST_DEBUG_CATEGORY_INIT (raw_video_parse_debug, "rawvideoparse", 0,
      "rawvideoparse element");

  object_class = G_OBJECT_CLASS (klass);
  element_class = GST_ELEMENT_CLASS (klass);
  baseparse_class = GST_BASE_PARSE_CLASS (klass);
  rawbaseparse_class = GST_RAW_BASE_PARSE_CLASS (klass);

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&static_sink_template));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&static_src_template));

  object_class->set_property =
      GST_DEBUG_FUNCPTR (gst_raw_video_parse_set_property);
  object_class->get_property =
      GST_DEBUG_FUNCPTR (gst_raw_video_parse_get_property);

  baseparse_class->stop = GST_DEBUG_FUNCPTR (gst_raw_video_parse_stop);

  rawbaseparse_class->set_current_config =
      GST_DEBUG_FUNCPTR (gst_raw_video_parse_set_current_config);
  rawbaseparse_class->get_current_config =
      GST_DEBUG_FUNCPTR (gst_raw_video_parse_get_current_config);
  rawbaseparse_class->set_config_from_caps =
      GST_DEBUG_FUNCPTR (gst_raw_video_parse_set_config_from_caps);
  rawbaseparse_class->get_caps_from_config =
      GST_DEBUG_FUNCPTR (gst_raw_video_parse_get_caps_from_config);
  rawbaseparse_class->get_config_frame_size =
      GST_DEBUG_FUNCPTR (gst_raw_video_parse_get_config_frame_size);
  rawbaseparse_class->get_max_frames_per_buffer =
      GST_DEBUG_FUNCPTR (gst_raw_video_parse_get_max_frames_per_buffer);
  rawbaseparse_class->is_config_ready =
      GST_DEBUG_FUNCPTR (gst_raw_video_parse_is_config_ready);
  rawbaseparse_class->process = GST_DEBUG_FUNCPTR (gst_raw_video_parse_process);
  rawbaseparse_class->is_unit_format_supported =
      GST_DEBUG_FUNCPTR (gst_raw_video_parse_is_unit_format_supported);
  rawbaseparse_class->get_units_per_second =
      GST_DEBUG_FUNCPTR (gst_raw_video_parse_get_units_per_second);
  rawbaseparse_class->get_overhead_size =
      GST_DEBUG_FUNCPTR (gst_raw_video_parse_get_overhead_size);

  g_object_class_install_property (object_class,
      PROP_WIDTH,
      g_param_spec_int ("width",
          "Width",
          "Width of frames in raw stream",
          0, G_MAXINT, DEFAULT_WIDTH,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)
      );
  g_object_class_install_property (object_class,
      PROP_HEIGHT,
      g_param_spec_int ("height",
          "Height",
          "Height of frames in raw stream",
          0, G_MAXINT,
          DEFAULT_HEIGHT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)
      );
  g_object_class_install_property (object_class,
      PROP_FORMAT,
      g_param_spec_enum ("format",
          "Format",
          "Format of frames in raw stream",
          GST_TYPE_VIDEO_FORMAT,
          DEFAULT_FORMAT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)
      );
  g_object_class_install_property (object_class,
      PROP_FRAMERATE,
      gst_param_spec_fraction ("framerate",
          "Frame rate",
          "Rate of frames in raw stream",
          0, 1, G_MAXINT, 1,
          DEFAULT_FRAMERATE_N, DEFAULT_FRAMERATE_D,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)
      );
  g_object_class_install_property (object_class,
      PROP_PIXEL_ASPECT_RATIO,
      gst_param_spec_fraction ("pixel-aspect-ratio",
          "Pixel aspect ratio",
          "Pixel aspect ratio of frames in raw stream",
          1, 100, 100, 1,
          DEFAULT_PIXEL_ASPECT_RATIO_N, DEFAULT_PIXEL_ASPECT_RATIO_D,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)
      );
  g_object_class_install_property (object_class,
      PROP_INTERLACED,
      g_param_spec_boolean ("interlaced",
          "Interlaced flag",
          "True if frames in raw stream are interlaced",
          DEFAULT_INTERLACED, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)
      );
  g_object_class_install_property (object_class,
      PROP_TOP_FIELD_FIRST,
      g_param_spec_boolean ("top-field-first",
          "Top field first",
          "True if top field in frames in raw stream come first (not used if frames aren't interlaced)",
          DEFAULT_INTERLACED, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)
      );
  g_object_class_install_property (object_class,
      PROP_PLANE_STRIDES,
      gst_param_spec_array ("plane-strides",
          "Plane strides",
          "Strides of the planes in bytes (e.g. plane-strides=\"<320,320>\")",
          g_param_spec_int ("plane-stride",
              "Plane stride",
              "Stride of the n-th plane in bytes (0 = stride equals width*bytes-per-pixel)",
              0, G_MAXINT,
              0,
              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS),
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)
      );
  g_object_class_install_property (object_class,
      PROP_PLANE_OFFSETS,
      gst_param_spec_array ("plane-offsets",
          "Plane offsets",
          "Offsets of the planes in bytes (e.g. plane-offsets=\"<0,76800>\")",
          g_param_spec_int ("plane-offset",
              "Plane offset",
              "Offset of the n-th plane in bytes",
              0, G_MAXINT,
              0,
              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS),
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)
      );
  g_object_class_install_property (object_class,
      PROP_FRAME_SIZE,
      g_param_spec_uint ("frame-size",
          "Frame size",
          "Size of a frame (0 = frames are tightly packed together)",
          0, G_MAXUINT,
          DEFAULT_FRAME_STRIDE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)
      );
  g_object_class_install_property (object_class,
      PROP_COLORIMETRY,
      g_param_spec_string ("colorimetry",
          "Colorimetry",
          "The video source colorimetry",
          NULL, G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS)
      );

  gst_element_class_set_static_metadata (element_class,
      "rawvideoparse",
      "Codec/Parser/Video",
      "Converts unformatted data streams into timestamped raw video frames",
      "Carlos Rafael Giani <dv@pseudoterminal.org>");
}

static void
gst_raw_video_parse_init (GstRawVideoParse * raw_video_parse)
{
  gst_raw_video_parse_init_config (&(raw_video_parse->properties_config));
  gst_raw_video_parse_init_config (&(raw_video_parse->sink_caps_config));

  /* As required by GstRawBaseParse, ensure that the current configuration
   * is initially set to be the properties config */
  raw_video_parse->current_config = &(raw_video_parse->properties_config);

  /* Properties config must be valid from the start, so set its ready value
   * to TRUE, and make sure its bpf value is valid. */
  raw_video_parse->properties_config.ready = TRUE;
  raw_video_parse->properties_config.top_field_first = DEFAULT_TOP_FIELD_FIRST;
  raw_video_parse->properties_config.frame_size = DEFAULT_FRAME_STRIDE;
}

static void
gst_raw_video_parse_set_property (GObject * object, guint prop_id,
    GValue const *value, GParamSpec * pspec)
{
  GstBaseParse *base_parse = GST_BASE_PARSE (object);
  GstRawBaseParse *raw_base_parse = GST_RAW_BASE_PARSE (object);
  GstRawVideoParse *raw_video_parse = GST_RAW_VIDEO_PARSE (object);
  GstRawVideoParseConfig *props_cfg = &(raw_video_parse->properties_config);

  /* All properties are handled similarly:
   * - if the new value is the same as the current value, nothing is done
   * - the parser lock is held while the new value is set
   * - if the properties config is the current config, the source caps are
   *   invalidated to ensure that the code in handle_frame pushes a new CAPS
   *   event out
   * - properties that affect the video frame size call the function to update
   *   the info and also call gst_base_parse_set_min_frame_size() to ensure
   *   that the minimum frame size can hold 1 frame (= one sample for each
   *   channel); to ensure that the min frame size includes any extra padding,
   *   it is set to the result of gst_raw_video_parse_get_config_frame_size()
   * - property configuration values that require video info updates aren't
   *   written directory into the video info structure, but in the extra
   *   fields instead (gst_raw_video_parse_update_info() then copies the values
   *   from these fields into the video info); see the documentation inside
   *   gst_raw_video_parse_update_info() for the reason why
   */

  switch (prop_id) {
    case PROP_WIDTH:
    {
      gint new_width = g_value_get_int (value);

      GST_RAW_BASE_PARSE_CONFIG_MUTEX_LOCK (object);

      if (new_width != props_cfg->width) {
        props_cfg->width = new_width;
        gst_raw_video_parse_update_info (props_cfg);

        if (!gst_raw_video_parse_is_using_sink_caps (raw_video_parse)) {
          gst_raw_base_parse_invalidate_src_caps (raw_base_parse);
          gst_base_parse_set_min_frame_size (base_parse,
              gst_raw_video_parse_get_config_frame_size (raw_base_parse,
                  GST_RAW_BASE_PARSE_CONFIG_PROPERTIES));
        }
      }

      GST_RAW_BASE_PARSE_CONFIG_MUTEX_UNLOCK (object);
      break;
    }

    case PROP_HEIGHT:
    {
      gint new_height = g_value_get_int (value);

      GST_RAW_BASE_PARSE_CONFIG_MUTEX_LOCK (object);

      if (new_height != props_cfg->height) {
        props_cfg->height = new_height;
        gst_raw_video_parse_update_info (props_cfg);

        if (!gst_raw_video_parse_is_using_sink_caps (raw_video_parse)) {
          gst_raw_base_parse_invalidate_src_caps (raw_base_parse);
          gst_base_parse_set_min_frame_size (base_parse,
              gst_raw_video_parse_get_config_frame_size (raw_base_parse,
                  GST_RAW_BASE_PARSE_CONFIG_PROPERTIES));
        }
      }

      GST_RAW_BASE_PARSE_CONFIG_MUTEX_UNLOCK (object);
      break;
    }

    case PROP_FORMAT:
    {
      GstVideoFormat new_format = g_value_get_enum (value);

      GST_RAW_BASE_PARSE_CONFIG_MUTEX_LOCK (object);

      if (new_format != props_cfg->format) {
        props_cfg->format = new_format;
        gst_raw_video_parse_update_info (props_cfg);

        if (!gst_raw_video_parse_is_using_sink_caps (raw_video_parse)) {
          gst_raw_base_parse_invalidate_src_caps (raw_base_parse);
          gst_base_parse_set_min_frame_size (base_parse,
              gst_raw_video_parse_get_config_frame_size (raw_base_parse,
                  GST_RAW_BASE_PARSE_CONFIG_PROPERTIES));
        }
      }

      GST_RAW_BASE_PARSE_CONFIG_MUTEX_UNLOCK (object);
      break;
    }

    case PROP_PIXEL_ASPECT_RATIO:
    {
      GST_RAW_BASE_PARSE_CONFIG_MUTEX_LOCK (object);

      /* The pixel aspect ratio does not affect the video frame size,
       * so it is just set directly without any updates */
      props_cfg->pixel_aspect_ratio_n =
          GST_VIDEO_INFO_PAR_N (&(props_cfg->info)) =
          gst_value_get_fraction_numerator (value);
      props_cfg->pixel_aspect_ratio_d =
          GST_VIDEO_INFO_PAR_D (&(props_cfg->info)) =
          gst_value_get_fraction_denominator (value);
      GST_DEBUG_OBJECT (raw_video_parse, "setting pixel aspect ratio to %u/%u",
          props_cfg->pixel_aspect_ratio_n, props_cfg->pixel_aspect_ratio_d);

      GST_RAW_BASE_PARSE_CONFIG_MUTEX_UNLOCK (object);
      break;
    }

    case PROP_FRAMERATE:
    {
      GST_RAW_BASE_PARSE_CONFIG_MUTEX_LOCK (object);

      /* The framerate does not affect the video frame size,
       * so it is just set directly without any updates */
      props_cfg->framerate_n = GST_VIDEO_INFO_FPS_N (&(props_cfg->info)) =
          gst_value_get_fraction_numerator (value);
      props_cfg->framerate_d = GST_VIDEO_INFO_FPS_D (&(props_cfg->info)) =
          gst_value_get_fraction_denominator (value);
      GST_DEBUG_OBJECT (raw_video_parse, "setting framerate to %u/%u",
          props_cfg->framerate_n, props_cfg->framerate_d);

      GST_RAW_BASE_PARSE_CONFIG_MUTEX_UNLOCK (object);
      break;
    }

    case PROP_INTERLACED:
    {
      GST_RAW_BASE_PARSE_CONFIG_MUTEX_LOCK (object);

      /* Interlacing does not affect the video frame size,
       * so it is just set directly without any updates */
      props_cfg->interlaced = g_value_get_boolean (value);
      GST_VIDEO_INFO_INTERLACE_MODE (&(props_cfg->info)) =
          props_cfg->interlaced ? GST_VIDEO_INTERLACE_MODE_INTERLEAVED :
          GST_VIDEO_INTERLACE_MODE_PROGRESSIVE;

      GST_RAW_BASE_PARSE_CONFIG_MUTEX_UNLOCK (object);

      break;
    }

    case PROP_TOP_FIELD_FIRST:
    {
      /* The top-field-first flag is a detail related to
       * interlacing, so no video info update is needed */

      GST_RAW_BASE_PARSE_CONFIG_MUTEX_LOCK (object);
      props_cfg->top_field_first = g_value_get_boolean (value);
      GST_RAW_BASE_PARSE_CONFIG_MUTEX_UNLOCK (object);
      break;
    }

    case PROP_PLANE_STRIDES:
    {
      guint n_planes;
      guint i;

      /* If no array is given, then disable custom
       * plane strides & offsets and stick to the
       * standard computed ones */
      if (gst_value_array_get_size (value) == 0) {
        GST_DEBUG_OBJECT (raw_video_parse,
            "custom plane strides & offsets disabled");
        props_cfg->custom_plane_strides = FALSE;
        gst_raw_video_parse_update_info (props_cfg);
        break;
      }

      GST_RAW_BASE_PARSE_CONFIG_MUTEX_LOCK (object);

      n_planes = GST_VIDEO_INFO_N_PLANES (&(props_cfg->info));

      /* Check that the array holds the right number of values */
      if (gst_value_array_get_size (value) < n_planes) {
        GST_ELEMENT_ERROR (raw_video_parse, LIBRARY, SETTINGS,
            ("incorrect number of elements in plane strides property"),
            ("expected: %u, got: %u", n_planes,
                gst_value_array_get_size (value)));
        GST_RAW_BASE_PARSE_CONFIG_MUTEX_UNLOCK (object);
        break;
      }

      /* Copy the values to the stride array */
      for (i = 0; i < n_planes; ++i) {
        const GValue *val = gst_value_array_get_value (value, i);
        props_cfg->plane_strides[i] = g_value_get_int (val);
        GST_DEBUG_OBJECT (raw_video_parse, "plane #%u stride: %d", i,
            props_cfg->plane_strides[i]);
      }

      props_cfg->custom_plane_strides = TRUE;

      gst_raw_video_parse_update_info (props_cfg);

      if (!gst_raw_video_parse_is_using_sink_caps (raw_video_parse))
        gst_base_parse_set_min_frame_size (base_parse,
            gst_raw_video_parse_get_config_frame_size (raw_base_parse,
                GST_RAW_BASE_PARSE_CONFIG_PROPERTIES));

      GST_RAW_BASE_PARSE_CONFIG_MUTEX_UNLOCK (object);
      break;
    }

    case PROP_PLANE_OFFSETS:
    {
      guint n_planes;
      guint i;

      /* If no array is given, then disable custom
       * plane strides & offsets and stick to the
       * standard computed ones */
      if (gst_value_array_get_size (value) == 0) {
        GST_DEBUG_OBJECT (raw_video_parse,
            "custom plane strides & offsets disabled");
        props_cfg->custom_plane_strides = FALSE;
        gst_raw_video_parse_update_info (props_cfg);
        break;
      }

      GST_RAW_BASE_PARSE_CONFIG_MUTEX_LOCK (object);

      n_planes = GST_VIDEO_INFO_N_PLANES (&(props_cfg->info));

      /* Check that the alarray holds the right number of values */
      if (gst_value_array_get_size (value) < n_planes) {
        GST_ELEMENT_ERROR (raw_video_parse, LIBRARY, SETTINGS,
            ("incorrect number of elements in plane offsets property"),
            ("expected: %u, got: %u", n_planes,
                gst_value_array_get_size (value)));
        GST_RAW_BASE_PARSE_CONFIG_MUTEX_UNLOCK (object);
        break;
      }

      /* Copy the values to the offset array */
      for (i = 0; i < n_planes; ++i) {
        const GValue *val = gst_value_array_get_value (value, i);
        props_cfg->plane_offsets[i] = g_value_get_int (val);
        GST_DEBUG_OBJECT (raw_video_parse, "plane #%u offset: %" G_GSIZE_FORMAT,
            i, props_cfg->plane_offsets[i]);
      }

      props_cfg->custom_plane_strides = TRUE;

      gst_raw_video_parse_update_info (props_cfg);

      if (!gst_raw_video_parse_is_using_sink_caps (raw_video_parse))
        gst_base_parse_set_min_frame_size (base_parse,
            gst_raw_video_parse_get_config_frame_size (raw_base_parse,
                GST_RAW_BASE_PARSE_CONFIG_PROPERTIES));

      GST_RAW_BASE_PARSE_CONFIG_MUTEX_UNLOCK (object);
      break;
    }

    case PROP_FRAME_SIZE:
    {
      /* The frame size is used to accumulate extra padding that may exist at
       * the end of a frame. It does not affect GstVideoInfo::size, hence
       * it is just set directly without any updates */

      GST_RAW_BASE_PARSE_CONFIG_MUTEX_LOCK (object);
      props_cfg->frame_size = g_value_get_uint (value);
      gst_raw_video_parse_update_info (props_cfg);
      if (!gst_raw_video_parse_is_using_sink_caps (raw_video_parse))
        gst_base_parse_set_min_frame_size (base_parse,
            gst_raw_video_parse_get_config_frame_size (raw_base_parse,
                GST_RAW_BASE_PARSE_CONFIG_PROPERTIES));
      GST_RAW_BASE_PARSE_CONFIG_MUTEX_UNLOCK (object);

      break;
    }
    case PROP_COLORIMETRY:
    {
      GstVideoColorimetry new_cinfo;

      if (!gst_video_colorimetry_from_string (&new_cinfo,
              g_value_get_string (value)))
        break;

      GST_RAW_BASE_PARSE_CONFIG_MUTEX_LOCK (object);
      if (!gst_video_colorimetry_is_equal (&new_cinfo,
              &(props_cfg->info.colorimetry))) {

        props_cfg->colorimetry = new_cinfo;
        gst_raw_video_parse_update_info (props_cfg);

        if (!gst_raw_video_parse_is_using_sink_caps (raw_video_parse))
          gst_raw_base_parse_invalidate_src_caps (raw_base_parse);
      }
      GST_RAW_BASE_PARSE_CONFIG_MUTEX_UNLOCK (object);

      break;
    }

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_raw_video_parse_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstRawVideoParse *raw_video_parse = GST_RAW_VIDEO_PARSE (object);
  GstRawVideoParseConfig *props_cfg = &(raw_video_parse->properties_config);

  switch (prop_id) {
    case PROP_WIDTH:
      GST_RAW_BASE_PARSE_CONFIG_MUTEX_LOCK (object);
      g_value_set_int (value, props_cfg->width);
      GST_RAW_BASE_PARSE_CONFIG_MUTEX_UNLOCK (object);
      break;

    case PROP_HEIGHT:
      GST_RAW_BASE_PARSE_CONFIG_MUTEX_LOCK (object);
      g_value_set_int (value, props_cfg->height);
      GST_RAW_BASE_PARSE_CONFIG_MUTEX_UNLOCK (object);
      break;

    case PROP_FORMAT:
      GST_RAW_BASE_PARSE_CONFIG_MUTEX_LOCK (object);
      g_value_set_enum (value, props_cfg->format);
      GST_RAW_BASE_PARSE_CONFIG_MUTEX_UNLOCK (object);
      break;

    case PROP_PIXEL_ASPECT_RATIO:
      GST_RAW_BASE_PARSE_CONFIG_MUTEX_LOCK (object);
      gst_value_set_fraction (value, props_cfg->pixel_aspect_ratio_n,
          props_cfg->pixel_aspect_ratio_d);
      GST_RAW_BASE_PARSE_CONFIG_MUTEX_UNLOCK (object);

      break;

    case PROP_FRAMERATE:
      GST_RAW_BASE_PARSE_CONFIG_MUTEX_LOCK (object);
      gst_value_set_fraction (value, props_cfg->framerate_n,
          props_cfg->framerate_d);
      GST_RAW_BASE_PARSE_CONFIG_MUTEX_UNLOCK (object);
      break;

    case PROP_INTERLACED:
      GST_RAW_BASE_PARSE_CONFIG_MUTEX_LOCK (object);
      g_value_set_boolean (value, props_cfg->interlaced);
      GST_RAW_BASE_PARSE_CONFIG_MUTEX_UNLOCK (object);
      break;

    case PROP_TOP_FIELD_FIRST:
      GST_RAW_BASE_PARSE_CONFIG_MUTEX_LOCK (object);
      g_value_set_boolean (value, props_cfg->top_field_first);
      GST_RAW_BASE_PARSE_CONFIG_MUTEX_UNLOCK (object);
      break;

    case PROP_PLANE_STRIDES:
    {
      guint i, n_planes;
      GValue val = G_VALUE_INIT;

      g_value_reset (value);

      GST_RAW_BASE_PARSE_CONFIG_MUTEX_LOCK (object);

      n_planes = GST_VIDEO_INFO_N_PLANES (&(props_cfg->info));
      g_value_init (&val, G_TYPE_INT);

      for (i = 0; i < n_planes; ++i) {
        g_value_set_int (&val, props_cfg->plane_strides[i]);
        gst_value_array_append_value (value, &val);
      }

      g_value_unset (&val);

      GST_RAW_BASE_PARSE_CONFIG_MUTEX_UNLOCK (object);
      break;
    }

    case PROP_PLANE_OFFSETS:
    {
      guint i, n_planes;
      GValue val = G_VALUE_INIT;

      g_value_reset (value);

      GST_RAW_BASE_PARSE_CONFIG_MUTEX_LOCK (object);

      n_planes = GST_VIDEO_INFO_N_PLANES (&(props_cfg->info));
      g_value_init (&val, G_TYPE_INT);

      for (i = 0; i < n_planes; ++i) {
        g_value_set_int (&val, props_cfg->plane_offsets[i]);
        gst_value_array_append_value (value, &val);
      }

      g_value_unset (&val);

      GST_RAW_BASE_PARSE_CONFIG_MUTEX_UNLOCK (object);
      break;
    }

    case PROP_FRAME_SIZE:
      GST_RAW_BASE_PARSE_CONFIG_MUTEX_LOCK (object);
      g_value_set_uint (value, raw_video_parse->properties_config.frame_size);
      GST_RAW_BASE_PARSE_CONFIG_MUTEX_UNLOCK (object);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_raw_video_parse_stop (GstBaseParse * parse)
{
  GstRawVideoParse *raw_video_parse = GST_RAW_VIDEO_PARSE (parse);

  /* Sink caps config is not ready until caps come in.
   * We are stopping processing, the element is being reset,
   * so the config has to be un-readied.
   * (Since the properties config is not depending on caps,
   * its ready status is always TRUE.) */
  raw_video_parse->sink_caps_config.ready = FALSE;

  return GST_BASE_PARSE_CLASS (parent_class)->stop (parse);
}

static gboolean
gst_raw_video_parse_set_current_config (GstRawBaseParse * raw_base_parse,
    GstRawBaseParseConfig config)
{
  GstRawVideoParse *raw_video_parse = GST_RAW_VIDEO_PARSE (raw_base_parse);

  switch (config) {
    case GST_RAW_BASE_PARSE_CONFIG_PROPERTIES:
      raw_video_parse->current_config = &(raw_video_parse->properties_config);
      break;

    case GST_RAW_BASE_PARSE_CONFIG_SINKCAPS:
      raw_video_parse->current_config = &(raw_video_parse->sink_caps_config);
      break;

    default:
      g_assert_not_reached ();
  }

  return TRUE;
}

static GstRawBaseParseConfig
gst_raw_video_parse_get_current_config (GstRawBaseParse * raw_base_parse)
{
  GstRawVideoParse *raw_video_parse = GST_RAW_VIDEO_PARSE (raw_base_parse);
  return gst_raw_video_parse_is_using_sink_caps (raw_video_parse) ?
      GST_RAW_BASE_PARSE_CONFIG_SINKCAPS : GST_RAW_BASE_PARSE_CONFIG_PROPERTIES;
}

static gboolean
gst_raw_video_parse_set_config_from_caps (GstRawBaseParse * raw_base_parse,
    GstRawBaseParseConfig config, GstCaps * caps)
{
  int i;
  GstStructure *structure;
  GstRawVideoParse *raw_video_parse = GST_RAW_VIDEO_PARSE (raw_base_parse);
  GstRawVideoParseConfig *config_ptr =
      gst_raw_video_parse_get_config_ptr (raw_video_parse, config);

  g_assert (caps != NULL);

  /* Caps might get copied, and the copy needs to be unref'd.
   * Also, the caller retains ownership over the original caps.
   * So, to make this mechanism also work with cases where the
   * caps are *not* copied, ref the original caps here first. */
  gst_caps_ref (caps);

  structure = gst_caps_get_structure (caps, 0);

  /* For unaligned raw data, the output caps stay the same,
   * except that video/x-unaligned-raw becomes video/x-raw,
   * since the parser aligns the frame data */
  if (gst_structure_has_name (structure, "video/x-unaligned-raw")) {
    /* Copy the caps to be able to modify them */
    GstCaps *new_caps = gst_caps_copy (caps);
    gst_caps_unref (caps);
    caps = new_caps;

    /* Change the media type to video/x-raw , otherwise
     * gst_video_info_from_caps() won't work */
    structure = gst_caps_get_structure (caps, 0);
    gst_structure_set_name (structure, "video/x-raw");
  }

  config_ptr->ready = gst_video_info_from_caps (&(config_ptr->info), caps);

  if (config_ptr->ready) {
    config_ptr->width = GST_VIDEO_INFO_WIDTH (&(config_ptr->info));
    config_ptr->height = GST_VIDEO_INFO_HEIGHT (&(config_ptr->info));
    config_ptr->format = GST_VIDEO_INFO_FORMAT (&(config_ptr->info));
    config_ptr->pixel_aspect_ratio_n =
        GST_VIDEO_INFO_PAR_N (&(config_ptr->info));
    config_ptr->pixel_aspect_ratio_d =
        GST_VIDEO_INFO_PAR_D (&(config_ptr->info));
    config_ptr->framerate_n = GST_VIDEO_INFO_FPS_N (&(config_ptr->info));
    config_ptr->framerate_d = GST_VIDEO_INFO_FPS_D (&(config_ptr->info));
    config_ptr->interlaced = GST_VIDEO_INFO_IS_INTERLACED (&(config_ptr->info));
    config_ptr->top_field_first = 0;
    config_ptr->frame_size = 0;

    for (i = 0; i < GST_VIDEO_MAX_PLANES; ++i) {
      config_ptr->plane_offsets[i] =
          GST_VIDEO_INFO_PLANE_OFFSET (&(config_ptr->info), i);
      config_ptr->plane_strides[i] =
          GST_VIDEO_INFO_PLANE_STRIDE (&(config_ptr->info), i);
    }
  }

  gst_caps_unref (caps);

  return config_ptr->ready;
}

static gboolean
gst_raw_video_parse_get_caps_from_config (GstRawBaseParse * raw_base_parse,
    GstRawBaseParseConfig config, GstCaps ** caps)
{
  GstRawVideoParse *raw_video_parse = GST_RAW_VIDEO_PARSE (raw_base_parse);
  GstRawVideoParseConfig *config_ptr =
      gst_raw_video_parse_get_config_ptr (raw_video_parse, config);

  g_assert (caps != NULL);

  *caps = gst_video_info_to_caps (&(config_ptr->info));

  return *caps != NULL;
}

static gsize
gst_raw_video_parse_get_config_frame_size (GstRawBaseParse * raw_base_parse,
    GstRawBaseParseConfig config)
{
  GstRawVideoParse *raw_video_parse = GST_RAW_VIDEO_PARSE (raw_base_parse);
  GstRawVideoParseConfig *config_ptr =
      gst_raw_video_parse_get_config_ptr (raw_video_parse, config);
  return MAX (GST_VIDEO_INFO_SIZE (&(config_ptr->info)),
      (gsize) (config_ptr->frame_size));
}

static guint
gst_raw_video_parse_get_max_frames_per_buffer (G_GNUC_UNUSED GstRawBaseParse *
    raw_base_parse, G_GNUC_UNUSED GstRawBaseParseConfig config)
{
  /* We want exactly one frame per buffer */
  return 1;
}

static gboolean
gst_raw_video_parse_is_config_ready (GstRawBaseParse * raw_base_parse,
    GstRawBaseParseConfig config)
{
  GstRawVideoParse *raw_video_parse = GST_RAW_VIDEO_PARSE (raw_base_parse);
  return gst_raw_video_parse_get_config_ptr (raw_video_parse, config)->ready;
}

static gboolean
gst_raw_video_parse_process (GstRawBaseParse * raw_base_parse,
    GstRawBaseParseConfig config, GstBuffer * in_data,
    G_GNUC_UNUSED gsize total_num_in_bytes,
    G_GNUC_UNUSED gsize num_valid_in_bytes, GstBuffer ** processed_data)
{
  GstAllocationParams alloc_params = { 0, 31, 0, 0 };
  GstMapInfo map_info;
  GstRawVideoParse *raw_video_parse = GST_RAW_VIDEO_PARSE (raw_base_parse);
  GstRawVideoParseConfig *config_ptr =
      gst_raw_video_parse_get_config_ptr (raw_video_parse, config);
  guint frame_flags = 0;
  GstVideoInfo *video_info = &(config_ptr->info);
  GstBuffer *out_data;

  if (!gst_buffer_map (in_data, &map_info, GST_MAP_READ)) {
    GST_WARNING_OBJECT (raw_video_parse, "Failed to map input data");
    return FALSE;
  }

  /* Allocate the output memory our required alignment */
  *processed_data = out_data = gst_buffer_new_allocate (NULL,
      GST_VIDEO_INFO_SIZE (video_info), &alloc_params);
  gst_buffer_fill (*processed_data, 0, map_info.data,
      GST_VIDEO_INFO_SIZE (video_info));
  gst_buffer_unmap (in_data, &map_info);

  /* And copy the metadata */
  gst_buffer_copy_into (*processed_data, in_data,
      GST_BUFFER_COPY_FLAGS | GST_BUFFER_COPY_TIMESTAMPS, 0,
      GST_VIDEO_INFO_SIZE (video_info));

  if (config_ptr->interlaced) {
    GST_BUFFER_FLAG_SET (out_data, GST_VIDEO_BUFFER_FLAG_INTERLACED);
    frame_flags |= GST_VIDEO_FRAME_FLAG_INTERLACED;

    if (config_ptr->top_field_first) {
      GST_BUFFER_FLAG_SET (out_data, GST_VIDEO_BUFFER_FLAG_TFF);
      frame_flags |= GST_VIDEO_FRAME_FLAG_TFF;
    } else
      GST_BUFFER_FLAG_UNSET (out_data, GST_VIDEO_BUFFER_FLAG_TFF);
  }

  gst_buffer_add_video_meta_full (out_data,
      frame_flags,
      config_ptr->format,
      config_ptr->width,
      config_ptr->height,
      GST_VIDEO_INFO_N_PLANES (video_info),
      config_ptr->plane_offsets, config_ptr->plane_strides);


  return TRUE;
}

static gboolean
gst_raw_video_parse_is_unit_format_supported (G_GNUC_UNUSED GstRawBaseParse *
    raw_base_parse, GstFormat format)
{
  switch (format) {
    case GST_FORMAT_BYTES:
    case GST_FORMAT_DEFAULT:
      return TRUE;
    default:
      return FALSE;
  }
}

static void
gst_raw_video_parse_get_units_per_second (GstRawBaseParse * raw_base_parse,
    GstFormat format, GstRawBaseParseConfig config, gsize * units_per_sec_n,
    gsize * units_per_sec_d)
{
  GstRawVideoParse *raw_video_parse = GST_RAW_VIDEO_PARSE (raw_base_parse);
  GstRawVideoParseConfig *config_ptr =
      gst_raw_video_parse_get_config_ptr (raw_video_parse, config);

  switch (format) {
    case GST_FORMAT_BYTES:
    {
      gsize framesize = GST_VIDEO_INFO_SIZE (&(config_ptr->info));
      gint64 n = framesize * config_ptr->framerate_n;
      gint64 d = config_ptr->framerate_d;
      gint64 common_div = gst_util_greatest_common_divisor_int64 (n, d);
      GST_DEBUG_OBJECT (raw_video_parse,
          "n: %" G_GINT64_FORMAT " d: %" G_GINT64_FORMAT " common divisor: %"
          G_GINT64_FORMAT, n, d, common_div);

      /* Divide numerator and denominator by greatest common divisor.
       * This minimizes the risk of integer overflows in the baseparse class. */
      *units_per_sec_n = n / common_div;
      *units_per_sec_d = d / common_div;

      break;
    }

    case GST_FORMAT_DEFAULT:
    {
      *units_per_sec_n = config_ptr->framerate_n;
      *units_per_sec_d = config_ptr->framerate_d;
      break;
    }

    default:
      g_assert_not_reached ();
  }
}

static gint
gst_raw_video_parse_get_overhead_size (GstRawBaseParse * raw_base_parse,
    GstRawBaseParseConfig config)
{
  GstRawVideoParse *raw_video_parse = GST_RAW_VIDEO_PARSE (raw_base_parse);
  GstRawVideoParseConfig *config_ptr =
      gst_raw_video_parse_get_config_ptr (raw_video_parse, config);
  gint64 info_size = GST_VIDEO_INFO_SIZE (&(config_ptr->info));
  gint64 frame_size = config_ptr->frame_size;

  /* In the video parser, the overhead is defined by the difference between
   * the configured frame size and the GstVideoInfo size. If the former is
   * larger, then the additional bytes are considered padding bytes and get
   * ignored by the base class. */

  GST_LOG_OBJECT (raw_video_parse,
      "info size: %" G_GINT64_FORMAT "  frame size: %" G_GINT64_FORMAT,
      info_size, frame_size);

  return (info_size < frame_size) ? (gint) (frame_size - info_size) : 0;
}

static gboolean
gst_raw_video_parse_is_using_sink_caps (GstRawVideoParse * raw_video_parse)
{
  return raw_video_parse->current_config ==
      &(raw_video_parse->sink_caps_config);
}

static GstRawVideoParseConfig *
gst_raw_video_parse_get_config_ptr (GstRawVideoParse * raw_video_parse,
    GstRawBaseParseConfig config)
{
  g_assert (raw_video_parse->current_config != NULL);

  switch (config) {
    case GST_RAW_BASE_PARSE_CONFIG_PROPERTIES:
      return &(raw_video_parse->properties_config);

    case GST_RAW_BASE_PARSE_CONFIG_SINKCAPS:
      return &(raw_video_parse->sink_caps_config);

    default:
      g_assert (raw_video_parse->current_config != NULL);
      return raw_video_parse->current_config;
  }
}

static void
gst_raw_video_parse_init_config (GstRawVideoParseConfig * config)
{
  int i;

  config->ready = FALSE;
  config->width = DEFAULT_WIDTH;
  config->height = DEFAULT_HEIGHT;
  config->format = DEFAULT_FORMAT;
  config->pixel_aspect_ratio_n = DEFAULT_PIXEL_ASPECT_RATIO_N;
  config->pixel_aspect_ratio_d = DEFAULT_PIXEL_ASPECT_RATIO_D;
  config->framerate_n = DEFAULT_FRAMERATE_N;
  config->framerate_d = DEFAULT_FRAMERATE_D;
  config->interlaced = DEFAULT_INTERLACED;

  config->top_field_first = DEFAULT_TOP_FIELD_FIRST;
  config->frame_size = DEFAULT_FRAME_STRIDE;

  gst_video_info_set_format (&(config->info), DEFAULT_FORMAT, DEFAULT_WIDTH,
      DEFAULT_HEIGHT);
  for (i = 0; i < GST_VIDEO_MAX_PLANES; ++i) {
    config->plane_offsets[i] = GST_VIDEO_INFO_PLANE_OFFSET (&(config->info), i);
    config->plane_strides[i] = GST_VIDEO_INFO_PLANE_STRIDE (&(config->info), i);
  }
}

static void
gst_raw_video_parse_update_info (GstRawVideoParseConfig * config)
{
  guint i;
  guint n_planes;
  guint last_plane;
  gsize last_plane_offset, last_plane_size;
  GstVideoInfo *info = &(config->info);

  GST_DEBUG ("updating info with width %u height %u format %s "
      " custom plane strides&offsets %d", config->width, config->height,
      gst_video_format_to_string (config->format),
      config->custom_plane_strides);

  gst_video_info_set_format (info, config->format, config->width,
      config->height);

  info->colorimetry = config->colorimetry;

  GST_VIDEO_INFO_PAR_N (info) = config->pixel_aspect_ratio_n;
  GST_VIDEO_INFO_PAR_D (info) = config->pixel_aspect_ratio_d;
  GST_VIDEO_INFO_FPS_N (info) = config->framerate_n;
  GST_VIDEO_INFO_FPS_D (info) = config->framerate_d;
  GST_VIDEO_INFO_INTERLACE_MODE (info) =
      config->interlaced ? GST_VIDEO_INTERLACE_MODE_INTERLEAVED :
      GST_VIDEO_INTERLACE_MODE_PROGRESSIVE;

  /* Check if there are custom plane strides & offsets that need to be preserved */
  if (config->custom_plane_strides) {
    /* In case there are, overwrite the offsets&strides computed by
     * gst_video_info_set_format with the custom ones */
    for (i = 0; i < GST_VIDEO_MAX_PLANES; ++i) {
      GST_VIDEO_INFO_PLANE_OFFSET (info, i) = config->plane_offsets[i];
      GST_VIDEO_INFO_PLANE_STRIDE (info, i) = config->plane_strides[i];
    }
  } else {
    /* No custom planes&offsets; copy the computed ones into
     * the plane_offsets & plane_strides arrays to ensure they
     * are equal to the ones in the videoinfo */
    for (i = 0; i < GST_VIDEO_MAX_PLANES; ++i) {
      config->plane_offsets[i] = GST_VIDEO_INFO_PLANE_OFFSET (info, i);
      config->plane_strides[i] = GST_VIDEO_INFO_PLANE_STRIDE (info, i);
    }
  }

  n_planes = GST_VIDEO_INFO_N_PLANES (info);
  if (n_planes < 1)
    n_planes = 1;

  /* Figure out what plane is the physically last one. Typically
   * this is the last plane in the list (= at index n_planes-1).
   * However, this is not guaranteed, so we have to scan the offsets
   * to find the last plane. */
  last_plane_offset = 0;
  last_plane = 0;
  for (i = 0; i < n_planes; ++i) {
    gsize plane_offset = GST_VIDEO_INFO_PLANE_OFFSET (info, i);
    if (plane_offset >= last_plane_offset) {
      last_plane = i;
      last_plane_offset = plane_offset;
    }
  }

  if (GST_VIDEO_FORMAT_INFO_IS_TILED (info->finfo)) {
    gint stride = GST_VIDEO_INFO_PLANE_STRIDE (info, last_plane);
    gint x_tiles = GST_VIDEO_TILE_X_TILES (stride);
    gint y_tiles = GST_VIDEO_TILE_Y_TILES (stride);
    guint tile_size = GST_VIDEO_FORMAT_INFO_TILE_SIZE (info->finfo, last_plane);

    last_plane_size = x_tiles * y_tiles * tile_size;
  } else {
    gint comp[GST_VIDEO_MAX_COMPONENTS];
    gst_video_format_info_component (info->finfo, last_plane, comp);
    last_plane_size =
        GST_VIDEO_INFO_PLANE_STRIDE (info,
        last_plane) * GST_VIDEO_FORMAT_INFO_SCALE_HEIGHT (info->finfo,
        comp[0], config->height);
  }

  GST_VIDEO_INFO_SIZE (info) = last_plane_offset + last_plane_size;

  GST_DEBUG ("last plane #%u:  offset: %" G_GSIZE_FORMAT " size: %"
      G_GSIZE_FORMAT " => frame size minus extra padding: %" G_GSIZE_FORMAT,
      last_plane, last_plane_offset, last_plane_size,
      GST_VIDEO_INFO_SIZE (info));
}
