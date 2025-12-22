/* GStreamer
 * Copyright (C) 2019 Stéphane Cerveau <scerveau@collabora.com>
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
 * SECTION:element-zxing
 * @title: zxing
 *
 * Detect bar codes in the video streams and send them as element messages to
 * the #GstBus if .#GstZXing:message property is %TRUE.
 * If the .#GstZXing:attach-frame property is %TRUE, the posted barcode message
 * includes a sample of the frame where the barcode was detected (Since 1.18).
 *
 * The element generate messages named `barcode`. The structure contains these fields:
 *
 * * #GstClockTime `timestamp`: the timestamp of the buffer that triggered the message.
 * * gchar * `type`: the symbol type.
 * * gchar * `symbol`: the detected bar code data.
 * * #GstClockTime `stream-time`: timestamp converted to stream-time.
 * * #GstClockTime `running-time`: timestamp converted to running-time.
 * * #GstSample `frame`: the frame in which the barcode message was detected, if
 *   the .#GstZXing:attach-frame property was set to %TRUE (Since 1.18)
 *
 *   This element is based on the c++ implementation of zxing which can found
 *   at https://github.com/nu-book/zxing-cpp.
 *
 * ## Example launch lines
 * |[
 * gst-launch-1.0 -m v4l2src ! videoconvert ! zxing ! videoconvert ! xvimagesink
 * ]| This pipeline will detect barcodes and send them as messages.
 * |[
 * gst-launch-1.0 -m v4l2src ! tee name=t ! queue ! videoconvert ! zxing ! fakesink t. ! queue ! xvimagesink
 * ]| Same as above, but running the filter on a branch to keep the display in color
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstzxing.h"

#include <string.h>
#include <math.h>

#include <gst/video/video.h>

#include "ReadBarcode.h"
#include "TextUtfEncoding.h"
#include "ZXVersion.h"

using namespace ZXing;

GST_DEBUG_CATEGORY_STATIC (zxing_debug);
#define GST_CAT_DEFAULT zxing_debug

#define DEFAULT_MESSAGE  TRUE
#define DEFAULT_ATTACH_FRAME FALSE
#define DEFAULT_TRY_ROTATE FALSE
#define DEFAULT_TRY_FASTER FALSE

enum
{
  PROP_0,
  PROP_MESSAGE,
  PROP_ATTACH_FRAME,
  PROP_TRY_ROTATE,
  PROP_TRY_FASTER,
  PROP_FORMAT,
};

enum
{
  BARCODE_FORMAT_ALL,
  BARCODE_FORMAT_AZTEC,
  BARCODE_FORMAT_CODABAR,
  BARCODE_FORMAT_CODE_39,
  BARCODE_FORMAT_CODE_93,
  BARCODE_FORMAT_CODE_128,
  BARCODE_FORMAT_DATA_MATRIX,
  BARCODE_FORMAT_EAN_8,
  BARCODE_FORMAT_EAN_13,
  BARCODE_FORMAT_ITF,
  BARCODE_FORMAT_MAXICODE,
  BARCODE_FORMAT_PDF_417,
  BARCODE_FORMAT_QR_CODE,
  BARCODE_FORMAT_RSS_14,
  BARCODE_FORMAT_RSS_EXPANDED,
  BARCODE_FORMAT_UPC_A,
  BARCODE_FORMAT_UPC_E,
  BARCODE_FORMAT_UPC_EAN_EXTENSION
};

static const GEnumValue barcode_formats[] = {
  {BARCODE_FORMAT_ALL, "ALL", "all"},
  {BARCODE_FORMAT_AZTEC, "AZTEC", "aztec"},
  {BARCODE_FORMAT_CODABAR, "CODABAR", "codabar"},
  {BARCODE_FORMAT_CODE_39, "CODE_39", "code_39"},
  {BARCODE_FORMAT_CODE_93, "CODE_93", "code_93"},
  {BARCODE_FORMAT_CODE_128, "CODE_128", "code_128"},
  {BARCODE_FORMAT_DATA_MATRIX, "PNG", "png"},
  {BARCODE_FORMAT_EAN_8, "EAN_8", "ean_8"},
  {BARCODE_FORMAT_EAN_13, "EAN_13", "ean_13"},
  {BARCODE_FORMAT_ITF, "ITF", "itf"},
  {BARCODE_FORMAT_MAXICODE, "MAXICODE", "maxicode"},
  {BARCODE_FORMAT_PDF_417, "PDF_417", "pdf_417"},
  {BARCODE_FORMAT_QR_CODE, "QR_CODE", "qr_code"},
  {BARCODE_FORMAT_RSS_14, "RSS_14", "rss_14"},
  {BARCODE_FORMAT_RSS_EXPANDED, "RSS_EXPANDED", "rss_expanded"},
  {BARCODE_FORMAT_UPC_A, "UPC_A", "upc_a"},
  {BARCODE_FORMAT_UPC_E, "UPC_E", "upc_e"},
  {BARCODE_FORMAT_UPC_EAN_EXTENSION, "UPC_EAN_EXTENSION", "upc_ean_expansion"},
  {0, NULL, NULL},
};

#define GST_TYPE_BARCODE_FORMAT (gst_barcode_format_get_type())
static GType
gst_barcode_format_get_type (void)
{
  static GType barcode_format_type = 0;

  if (!barcode_format_type) {
    barcode_format_type =
        g_enum_register_static ("GstBarCodeFormat", barcode_formats);
  }
  return barcode_format_type;
}

#define ZXING_YUV_CAPS \
    "{ARGB, xRGB, Y444, Y42B, I420, Y41B, YUV9, YV12}"


static GstStaticPadTemplate gst_zxing_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE (ZXING_YUV_CAPS))
    );

static GstStaticPadTemplate gst_zxing_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE (ZXING_YUV_CAPS))
    );

/**
 * GstZXing:
 *
 * Opaque data structure.
 */
struct _GstZXing
{
  /*< private > */
  GstVideoFilter videofilter;

  /* properties */
  gboolean message;
  gboolean attach_frame;
  gboolean rotate;
  gboolean faster;
  ImageFormat image_format;
  guint barcode_format;
};

static void gst_zxing_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_zxing_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static gboolean gst_zxing_set_info (GstVideoFilter * vfilter, GstCaps * in,
    GstVideoInfo * in_info, GstCaps * out, GstVideoInfo * out_info);
static GstFlowReturn gst_zxing_transform_frame_ip (GstVideoFilter * vfilter,
    GstVideoFrame * frame);

#define gst_zxing_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstZXing, gst_zxing,
    GST_TYPE_VIDEO_FILTER,
    GST_DEBUG_CATEGORY_INIT (zxing_debug, "zxing", 0,
        "debug category for zxing element"));
GST_ELEMENT_REGISTER_DEFINE (zxing, "zxing", GST_RANK_MARGINAL, GST_TYPE_ZXING);

static void
gst_zxing_class_init (GstZXingClass * g_class)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstVideoFilterClass *vfilter_class;

  gobject_class = G_OBJECT_CLASS (g_class);
  gstelement_class = GST_ELEMENT_CLASS (g_class);
  vfilter_class = GST_VIDEO_FILTER_CLASS (g_class);

  gobject_class->set_property = gst_zxing_set_property;
  gobject_class->get_property = gst_zxing_get_property;

  g_object_class_install_property (gobject_class, PROP_MESSAGE,
      g_param_spec_boolean ("message",
          "message", "Post a barcode message for each detected code",
          DEFAULT_MESSAGE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_ATTACH_FRAME,
      g_param_spec_boolean ("attach-frame", "Attach frame",
          "Attach a frame dump to each barcode message",
          DEFAULT_ATTACH_FRAME,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_TRY_ROTATE,
      g_param_spec_boolean ("try-rotate", "Try rotate",
          "Try to rotate the frame to detect barcode (slower)",
          DEFAULT_TRY_ROTATE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_TRY_FASTER,
      g_param_spec_boolean ("try-faster", "Try faster",
          "Try faster to analyze the frame", DEFAULT_TRY_FASTER,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_FORMAT,
      g_param_spec_enum ("format", "barcode format", "Barcode image format",
          GST_TYPE_BARCODE_FORMAT, BARCODE_FORMAT_ALL,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  gst_element_class_set_static_metadata (gstelement_class, "Barcode detector",
      "Filter/Analyzer/Video",
      "Detect bar codes in the video streams with zxing library",
      "Stéphane Cerveau <scerveau@collabora.com>");

  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_zxing_sink_template);
  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_zxing_src_template);

  vfilter_class->transform_frame_ip =
      GST_DEBUG_FUNCPTR (gst_zxing_transform_frame_ip);
  vfilter_class->set_info =
      GST_DEBUG_FUNCPTR (gst_zxing_set_info);
}

static void
gst_zxing_init (GstZXing * zxing)
{
  zxing->message = DEFAULT_MESSAGE;
  zxing->attach_frame = DEFAULT_ATTACH_FRAME;
  zxing->rotate = DEFAULT_TRY_ROTATE;
  zxing->faster = DEFAULT_TRY_FASTER;
  zxing->image_format = ImageFormat::None;
  zxing->barcode_format = BARCODE_FORMAT_ALL;
}

static void
gst_zxing_set_property (GObject * object, guint prop_id, const GValue * value,
    GParamSpec * pspec)
{
  GstZXing *zxing;

  g_return_if_fail (GST_IS_ZXING (object));
  zxing = GST_ZXING (object);

  switch (prop_id) {
    case PROP_MESSAGE:
      zxing->message = g_value_get_boolean (value);
      break;
    case PROP_ATTACH_FRAME:
      zxing->attach_frame = g_value_get_boolean (value);
      break;
    case PROP_TRY_ROTATE:
      zxing->rotate = g_value_get_boolean (value);
      break;
    case PROP_TRY_FASTER:
      zxing->faster = g_value_get_boolean (value);
      break;
    case PROP_FORMAT:
      zxing->barcode_format = g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_zxing_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstZXing *zxing;

  g_return_if_fail (GST_IS_ZXING (object));
  zxing = GST_ZXING (object);

  switch (prop_id) {
    case PROP_MESSAGE:
      g_value_set_boolean (value, zxing->message);
      break;
    case PROP_ATTACH_FRAME:
      g_value_set_boolean (value, zxing->attach_frame);
      break;
    case PROP_TRY_ROTATE:
      g_value_set_boolean (value, zxing->rotate);
      break;
    case PROP_TRY_FASTER:
      g_value_set_boolean (value, zxing->faster);
      break;
    case PROP_FORMAT:
      g_value_set_enum (value, zxing->barcode_format);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_zxing_set_info (GstVideoFilter * vfilter, GstCaps * in,
    GstVideoInfo * in_info, GstCaps * out, GstVideoInfo * out_info)
{
  GstZXing *zxing = GST_ZXING (vfilter);
  switch (in_info->finfo->format) {
    case GST_VIDEO_FORMAT_ARGB:
    case GST_VIDEO_FORMAT_xRGB:
      zxing->image_format = ImageFormat::XRGB;
      break;
    case GST_VIDEO_FORMAT_Y444:
    case GST_VIDEO_FORMAT_Y42B:
    case GST_VIDEO_FORMAT_I420:
    case GST_VIDEO_FORMAT_Y41B:
    case GST_VIDEO_FORMAT_YUV9:
    case GST_VIDEO_FORMAT_YV12:
      zxing->image_format = ImageFormat::Lum;
      break;
    default:
      zxing->image_format = ImageFormat::None;
      GST_WARNING_OBJECT (zxing, "This format is not supported %s", gst_video_format_to_string(in_info->finfo->format));
  }
  return TRUE;
}

static GstFlowReturn
gst_zxing_transform_frame_ip (GstVideoFilter * vfilter, GstVideoFrame * frame)
{
  GstZXing *zxing = GST_ZXING (vfilter);
  gpointer data;
  gint height, width;
  DecodeHints hints;

  hints.setTryRotate(zxing->rotate);
  hints.setTryHarder(!zxing->faster);
  hints.setFormats(BarcodeFormatFromString (barcode_formats[zxing->barcode_format].value_name));

  /* all formats we support start with an 8-bit Y plane. zxing doesn't need
   * to know about the chroma plane(s) */
  data = GST_VIDEO_FRAME_COMP_DATA (frame, 0);
  width = GST_VIDEO_FRAME_WIDTH (frame);
  height = GST_VIDEO_FRAME_HEIGHT (frame);

  auto result = ReadBarcode ({(unsigned char *)data, width, height, zxing->image_format}, hints);
  if (result.isValid ()) {
    GST_DEBUG_OBJECT (zxing, "Symbol found. Text: %s Format: %s",
        result.text ().c_str (),
#if ZXING_VERSION_MAJOR >= 2
        ToString (result.format ()).c_str ());
#else
        ToString (result.format ()));
#endif
  } else {
    goto out;
  }

  /* extract results */
  if (zxing->message) {
    GstMessage *m;
    GstStructure *s;
    GstSample *sample;
    GstCaps *sample_caps;
    GstClockTime timestamp, running_time, stream_time;

    timestamp = GST_BUFFER_TIMESTAMP (frame->buffer);
    running_time =
        gst_segment_to_running_time (&GST_BASE_TRANSFORM (zxing)->segment,
        GST_FORMAT_TIME, timestamp);
    stream_time =
        gst_segment_to_stream_time (&GST_BASE_TRANSFORM (zxing)->segment,
        GST_FORMAT_TIME, timestamp);

    s = gst_structure_new ("barcode",
        "timestamp", G_TYPE_UINT64, timestamp,
        "stream-time", G_TYPE_UINT64, stream_time,
        "running-time", G_TYPE_UINT64, running_time,
#if ZXING_VERSION_MAJOR >= 2
        "type", G_TYPE_STRING, ToString (result.format ()).c_str (),
#else
        "type", G_TYPE_STRING, ToString (result.format ()),
#endif
        "symbol", G_TYPE_STRING,
        result.text ().c_str (), NULL);

    if (zxing->attach_frame) {
      /* create a sample from image */
      sample_caps = gst_video_info_to_caps (&frame->info);
      sample = gst_sample_new (frame->buffer, sample_caps, NULL, NULL);
      gst_caps_unref (sample_caps);
      gst_structure_set (s, "frame", GST_TYPE_SAMPLE, sample, NULL);
      gst_sample_unref (sample);
    }
    m = gst_message_new_element (GST_OBJECT (zxing), s);
    gst_element_post_message (GST_ELEMENT (zxing), m);

  } else if (zxing->attach_frame)
    GST_WARNING_OBJECT (zxing,
        "attach-frame=true has no effect if message=false");

out:
  return GST_FLOW_OK;
}
