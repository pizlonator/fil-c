/*
 * GStreamer
 * Copyright (C) 2018 Edward Hervey <edward@centricular.com>
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
 * SECTION:element-line21decoder
 * @title: line21decoder
 *
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>
#include <gst/video/video.h>
#include <string.h>

#include "gstline21dec.h"

GST_DEBUG_CATEGORY_STATIC (gst_line_21_decoder_debug);
#define GST_CAT_DEFAULT gst_line_21_decoder_debug

/**
 * GstLine21DecoderMode:
 * @GST_LINE_21_DECODER_MODE_ADD: add new CC meta on top of other CC meta, if any
 * @GST_LINE_21_DECODER_MODE_DROP: ignore CC if a CC meta was already present
 * @GST_LINE_21_DECODER_MODE_REPLACE: replace existing CC meta
 *
 * Since: 1.20
 */

enum
{
  PROP_0,
  PROP_NTSC_ONLY,
  PROP_MODE,
};

#define DEFAULT_NTSC_ONLY FALSE
#define DEFAULT_MODE GST_LINE_21_DECODER_MODE_ADD

#define CAPS "video/x-raw, format={ I420, YUY2, YVYU, UYVY, VYUY, v210 }"

static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (CAPS));

static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (CAPS));

#define parent_class gst_line_21_decoder_parent_class
G_DEFINE_TYPE (GstLine21Decoder, gst_line_21_decoder, GST_TYPE_VIDEO_FILTER);
GST_ELEMENT_REGISTER_DEFINE (line21decoder, "line21decoder",
    GST_RANK_NONE, GST_TYPE_LINE21DECODER);

#define GST_TYPE_LINE_21_DECODER_MODE (gst_line_21_decoder_mode_get_type())
static GType
gst_line_21_decoder_mode_get_type (void)
{
  static const GEnumValue values[] = {
    {GST_LINE_21_DECODER_MODE_ADD,
        "add new CC meta on top of other CC meta, if any", "add"},
    {GST_LINE_21_DECODER_MODE_DROP,
          "ignore CC if a CC meta was already present",
        "drop"},
    {GST_LINE_21_DECODER_MODE_REPLACE,
        "replace existing CC meta", "replace"},
    {0, NULL, NULL}
  };
  static volatile GType id = 0;

  if (g_once_init_enter ((gsize *) & id)) {
    GType _id;

    _id = g_enum_register_static ("GstLine21DecoderMode", values);

    g_once_init_leave ((gsize *) & id, _id);
  }

  return id;
}

static void gst_line_21_decoder_finalize (GObject * self);
static gboolean gst_line_21_decoder_stop (GstBaseTransform * btrans);
static gboolean gst_line_21_decoder_set_info (GstVideoFilter * filter,
    GstCaps * incaps, GstVideoInfo * in_info,
    GstCaps * outcaps, GstVideoInfo * out_info);
static GstFlowReturn gst_line_21_decoder_transform_ip (GstVideoFilter * filter,
    GstVideoFrame * frame);
static GstFlowReturn gst_line_21_decoder_prepare_output_buffer (GstBaseTransform
    * trans, GstBuffer * in, GstBuffer ** out);

static void
gst_line_21_decoder_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstLine21Decoder *enc = GST_LINE21DECODER (object);

  switch (prop_id) {
    case PROP_MODE:
      enc->mode = g_value_get_enum (value);
      break;
    case PROP_NTSC_ONLY:
      enc->ntsc_only = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_line_21_decoder_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstLine21Decoder *enc = GST_LINE21DECODER (object);

  switch (prop_id) {
    case PROP_MODE:
      g_value_set_enum (value, enc->mode);
      break;
    case PROP_NTSC_ONLY:
      g_value_set_boolean (value, enc->ntsc_only);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_line_21_decoder_class_init (GstLine21DecoderClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseTransformClass *transform_class;
  GstVideoFilterClass *filter_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  transform_class = (GstBaseTransformClass *) klass;
  filter_class = (GstVideoFilterClass *) klass;

  gobject_class->finalize = gst_line_21_decoder_finalize;
  gobject_class->set_property = gst_line_21_decoder_set_property;
  gobject_class->get_property = gst_line_21_decoder_get_property;

  /**
   * line21decoder:ntsc-only
   *
   * Whether line 21 decoding should only be attempted when the
   * input resolution matches NTSC (720 x 525) or NTSC usable
   * lines (720 x 486)
   *
   * Since: 1.20
   */
  g_object_class_install_property (G_OBJECT_CLASS (klass),
      PROP_NTSC_ONLY, g_param_spec_boolean ("ntsc-only",
          "NTSC only",
          "Whether line 21 decoding should only be attempted when the "
          "input resolution matches NTSC", DEFAULT_NTSC_ONLY,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstLine21Decoder:mode
   *
   * Control whether and how detected CC meta should be inserted
   * in the list of existing CC meta on a frame (if any).
   *
   * Since: 1.20
   */
  g_object_class_install_property (G_OBJECT_CLASS (klass),
      PROP_MODE, g_param_spec_enum ("mode",
          "Mode",
          "Control whether and how detected CC meta should be inserted "
          "in the list of existing CC meta on a frame (if any).",
          GST_TYPE_LINE_21_DECODER_MODE, DEFAULT_MODE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_set_static_metadata (gstelement_class,
      "Line 21 CC Decoder",
      "Filter/Video/ClosedCaption",
      "Extract line21 CC from SD video streams",
      "Edward Hervey <edward@centricular.com>");

  gst_element_class_add_static_pad_template (gstelement_class, &sinktemplate);
  gst_element_class_add_static_pad_template (gstelement_class, &srctemplate);

  transform_class->stop = gst_line_21_decoder_stop;
  transform_class->prepare_output_buffer =
      gst_line_21_decoder_prepare_output_buffer;

  filter_class->set_info = gst_line_21_decoder_set_info;
  filter_class->transform_frame_ip = gst_line_21_decoder_transform_ip;

  GST_DEBUG_CATEGORY_INIT (gst_line_21_decoder_debug, "line21decoder",
      0, "Line 21 CC Decoder");
  vbi_initialize_gst_debug ();

  gst_type_mark_as_plugin_api (GST_TYPE_LINE_21_DECODER_MODE, 0);
}

static void
gst_line_21_decoder_init (GstLine21Decoder * filter)
{
  GstLine21Decoder *self = (GstLine21Decoder *) filter;

  self->info = NULL;
  self->line21_offset = -1;
  self->max_line_probes = 40;
  self->ntsc_only = DEFAULT_NTSC_ONLY;
  self->mode = DEFAULT_MODE;
}

static vbi_pixfmt
vbi_pixfmt_from_gst_video_format (GstVideoFormat format,
    gboolean * convert_v210)
{
  *convert_v210 = FALSE;

  switch (format) {
    case GST_VIDEO_FORMAT_I420:
      return VBI_PIXFMT_YUV420;
    case GST_VIDEO_FORMAT_YUY2:
      return VBI_PIXFMT_YUYV;
    case GST_VIDEO_FORMAT_YVYU:
      return VBI_PIXFMT_YVYU;
    case GST_VIDEO_FORMAT_UYVY:
      return VBI_PIXFMT_UYVY;
    case GST_VIDEO_FORMAT_VYUY:
      return VBI_PIXFMT_VYUY;
      /* for v210 we'll convert it to I420 luma */
    case GST_VIDEO_FORMAT_v210:
      *convert_v210 = TRUE;
      return VBI_PIXFMT_YUV420;
      /* All the other formats are not really bullet-proof. Force conversion */
    default:
      g_assert_not_reached ();
      return (vbi_pixfmt) 0;
  }
#undef NATIVE_VBI_FMT
}

static gboolean
gst_line_21_decoder_set_info (GstVideoFilter * filter,
    GstCaps * incaps, GstVideoInfo * in_info,
    GstCaps * outcaps, GstVideoInfo * out_info)
{
  GstLine21Decoder *self = (GstLine21Decoder *) filter;
  vbi_pixfmt fmt =
      vbi_pixfmt_from_gst_video_format (GST_VIDEO_INFO_FORMAT (in_info),
      &self->convert_v210);

  GST_DEBUG_OBJECT (filter, "caps %" GST_PTR_FORMAT, incaps);
  GST_DEBUG_OBJECT (filter, "plane_stride:%u , comp_stride:%u , pstride:%u",
      GST_VIDEO_INFO_PLANE_STRIDE (in_info, 0),
      GST_VIDEO_INFO_COMP_STRIDE (in_info, 0),
      GST_VIDEO_INFO_COMP_PSTRIDE (in_info, 0));
  GST_DEBUG_OBJECT (filter, "#planes : %d #components : %d",
      GST_VIDEO_INFO_N_PLANES (in_info), GST_VIDEO_INFO_N_COMPONENTS (in_info));

  if (self->info) {
    gst_video_info_free (self->info);
    self->info = NULL;
  }

  g_free (self->converted_lines);
  self->converted_lines = NULL;

  /* Scan the next frame from the first line */
  self->line21_offset = -1;

  if (!GST_VIDEO_INFO_IS_INTERLACED (in_info)) {
    GST_DEBUG_OBJECT (filter, "Only interlaced formats are supported");
    self->compatible_format = FALSE;
    return TRUE;
  }

  if (GST_VIDEO_INFO_WIDTH (in_info) != 720) {
    GST_DEBUG_OBJECT (filter, "Only 720 pixel wide formats are supported");
    self->compatible_format = FALSE;
    return TRUE;
  }

  if (self->ntsc_only &&
      GST_VIDEO_INFO_HEIGHT (in_info) != 525 &&
      GST_VIDEO_INFO_HEIGHT (in_info) != 486) {
    GST_DEBUG_OBJECT (filter,
        "NTSC-only, only 525 or 486 pixel high formats are supported");
    self->compatible_format = FALSE;
    return TRUE;
  }

  if (fmt == 0) {
    if (GST_VIDEO_INFO_FORMAT (in_info) == GST_VIDEO_FORMAT_v210) {
      GST_DEBUG_OBJECT (filter,
          "Format not supported natively, Adding conversion to YUY2");
      self->compatible_format = TRUE;
      self->convert_v210 = TRUE;
    } else {
      GST_DEBUG_OBJECT (filter, "Unsupported format");
      self->compatible_format = FALSE;
    }
    return TRUE;
  }

  if (GST_VIDEO_INFO_WIDTH (in_info) == 720
      && GST_VIDEO_INFO_HEIGHT (in_info) >= 200) {
    GST_DEBUG_OBJECT (filter, "Compatible size!");
    GST_DEBUG_OBJECT (filter,
        "Compatible format plane_stride:%u , comp_stride:%u , pstride:%u",
        GST_VIDEO_INFO_PLANE_STRIDE (in_info, 0),
        GST_VIDEO_INFO_COMP_STRIDE (in_info, 0),
        GST_VIDEO_INFO_COMP_PSTRIDE (in_info, 0));
    self->compatible_format = TRUE;
    if (self->convert_v210) {
      self->info = gst_video_info_new ();
      gst_video_info_set_format (self->info, GST_VIDEO_FORMAT_I420,
          GST_VIDEO_INFO_WIDTH (in_info), GST_VIDEO_INFO_HEIGHT (in_info));
      /* Allocate space for two *I420* Y lines (with stride) */
      self->converted_lines =
          g_malloc0 (2 * GST_VIDEO_INFO_COMP_STRIDE (self->info, 0));
    } else
      self->info = gst_video_info_copy (in_info);

    /* initialize the decoder */
    if (self->zvbi_decoder.pattern != NULL)
      vbi_raw_decoder_reset (&self->zvbi_decoder);
    else
      vbi_raw_decoder_init (&self->zvbi_decoder);
    /*
     * Set up blank / black / white levels fit for NTSC, no actual relation
     * with the height of the video
     */
    self->zvbi_decoder.scanning = 525;
    /* The pixel format. Quite a few formats are handled by zvbi, but
     * some are not and require conversion (or cheating) */
    self->zvbi_decoder.sampling_format = fmt;
    /* Sampling rate. For BT.601 it's 13.5MHz */
    self->zvbi_decoder.sampling_rate = 13.5e6;  /* Hz (i.e. BT.601) */
    /* Stride */
    self->zvbi_decoder.bytes_per_line =
        GST_VIDEO_INFO_COMP_STRIDE (self->info, 0);
    /* Sampling starts 9.7 µs from the front edge of the
       hor. sync pulse. You may have to adjust this.
       NOTE : This is actually ignored in the code ... 
     */
    self->zvbi_decoder.offset = 9.7e-6 * 13.5e6;

    /* The following values indicate what we are feeding to zvbi.
     * By setting start[0] = 21, we are telling zvbi that the very
     * beginning of the data we are feeding to it corresponds to
     * line 21 (which is where CC1/CC3 is located).
     *
     * Then by specifying count[0] = 1, we are telling it to only
     * scan 1 line from the beginning of the data.
     *
     * It is more efficient and flexible to do it this way, since
     * we can then control what we are feeding it (i.e. *we* will
     * figure out the offset to line 21, which might or might not
     * be the beginning of the buffer data, and feed data from
     * there). This would also allows us to have a "scanning" mode
     * where we repeatedly provide it with pairs of lines until it
     * finds something. */
    self->zvbi_decoder.start[0] = 21;
    self->zvbi_decoder.count[0] = 1;

    /* Second field. */
    self->zvbi_decoder.start[1] = 284;
    self->zvbi_decoder.count[1] = 1;

    /* FIXME : Adjust according to the info.interlace_mode ! */
    self->zvbi_decoder.interlaced = TRUE;

    /* synchronous is essentially top-field-first.
     * WARNING : zvbi doesn't support bottom-field-first. */
    self->zvbi_decoder.synchronous = TRUE;

    /* Specify the services you want. Adjust based on whether we
     * have PAL or NTSC */
    vbi_raw_decoder_add_services (&self->zvbi_decoder,
        VBI_SLICED_CAPTION_525, /* strict */ 0);

  } else
    self->compatible_format = FALSE;

  return TRUE;
}

static GstFlowReturn
gst_line_21_decoder_prepare_output_buffer (GstBaseTransform * trans,
    GstBuffer * in, GstBuffer ** out)
{
  GstLine21Decoder *self = (GstLine21Decoder *) trans;

  GST_DEBUG_OBJECT (trans, "compatible_format:%d", self->compatible_format);
  if (self->compatible_format) {
    /* Make the output buffer writable */
    *out = gst_buffer_make_writable (in);
    return GST_FLOW_OK;
  }

  return
      GST_BASE_TRANSFORM_CLASS
      (gst_line_21_decoder_parent_class)->prepare_output_buffer (trans, in,
      out);
}

static void
convert_line_v210_luma (const guint8 * orig, guint8 * dest, guint width)
{
  guint i;
  guint32 a, b, c, d;
  guint8 *y = dest;

  for (i = 0; i < width - 5; i += 6) {
    a = GST_READ_UINT32_LE (orig + (i / 6) * 16 + 0);
    b = GST_READ_UINT32_LE (orig + (i / 6) * 16 + 4);
    c = GST_READ_UINT32_LE (orig + (i / 6) * 16 + 8);
    d = GST_READ_UINT32_LE (orig + (i / 6) * 16 + 12);

    *y++ = (a >> 12) & 0xff;
    *y++ = (b >> 2) & 0xff;

    *y++ = (b >> 22) & 0xff;
    *y++ = (c >> 12) & 0xff;

    *y++ = (d >> 2) & 0xff;
    *y++ = (d >> 22) & 0xff;
  }
}

static guint8 *
get_video_data (GstLine21Decoder * self, GstVideoFrame * frame, gint line)
{
  guint8 *data = self->converted_lines;
  guint8 *v210;

  if (!self->convert_v210)
    return (guint8 *) GST_VIDEO_FRAME_PLANE_DATA (frame,
        0) + line * GST_VIDEO_INFO_COMP_STRIDE (self->info, 0);

  v210 = (guint8 *)
      GST_VIDEO_FRAME_PLANE_DATA (frame,
      0) + line * GST_VIDEO_FRAME_COMP_STRIDE (frame, 0);

  /* Convert v210 to I420 */
  convert_line_v210_luma (v210, data, GST_VIDEO_FRAME_WIDTH (frame));
  v210 += GST_VIDEO_FRAME_COMP_STRIDE (frame, 0);
  convert_line_v210_luma (v210, data + GST_VIDEO_INFO_COMP_STRIDE (self->info,
          0), GST_VIDEO_FRAME_WIDTH (frame));
  GST_MEMDUMP ("converted", self->converted_lines, 64);
  return self->converted_lines;
}

static gboolean
drop_cc_meta (GstBuffer * buffer, GstMeta ** meta, gpointer unused)
{
  if ((*meta)->info->api == GST_VIDEO_CAPTION_META_API_TYPE)
    *meta = NULL;

  return TRUE;
}

/* Call this to scan for CC
 * Returns TRUE if it was found and set, else FALSE */
static gboolean
gst_line_21_decoder_scan (GstLine21Decoder * self, GstVideoFrame * frame)
{
  gint i;
  vbi_sliced sliced[52];
  gboolean found = FALSE;
  guint8 *data;

  if (self->mode == GST_LINE_21_DECODER_MODE_DROP &&
      gst_buffer_get_n_meta (frame->buffer,
          GST_VIDEO_CAPTION_META_API_TYPE) > 0) {
    GST_DEBUG_OBJECT (self, "Mode drop and buffer had CC meta, ignoring");
    return FALSE;
  }

  GST_DEBUG_OBJECT (self, "Starting probing. max_line_probes:%d",
      self->max_line_probes);

  i = self->line21_offset;
  if (i == -1) {
    GST_DEBUG_OBJECT (self, "Scanning from the beginning");
    i = 0;
  }

  for (; i < self->max_line_probes && i < GST_VIDEO_FRAME_HEIGHT (frame); i++) {
    gint n_lines;
    data = get_video_data (self, frame, i);
    /* Scan until we get n_lines == 2 */
    n_lines = vbi_raw_decode (&self->zvbi_decoder, data, sliced);
    GST_DEBUG_OBJECT (self, "i:%d n_lines:%d", i, n_lines);
    if (n_lines == 2) {
      GST_DEBUG_OBJECT (self, "Found 2 CC lines at offset %d", i);
      self->line21_offset = i;
      found = TRUE;
      break;
    } else if (i == self->line21_offset) {
      /* Otherwise if this was the previously probed line offset,
       * reset and start searching again from the beginning */
      i = -1;
      self->line21_offset = -1;
    }
  }

  if (!found) {
    self->line21_offset = -1;
  } else {
    guint base_line1 = 0, base_line2 = 0;
    guint8 ccdata[6] = { 0x80, 0x80, 0x80, 0x00, 0x80, 0x80 };  /* Initialize the ccdata */

    if (GST_VIDEO_FRAME_HEIGHT (frame) == 525) {
      base_line1 = 9;
      base_line2 = 272;
    } else if (GST_VIDEO_FRAME_HEIGHT (frame) == 625) {
      base_line1 = 5;
      base_line2 = 318;
    }

    if (self->mode == GST_LINE_21_DECODER_MODE_REPLACE) {
      GST_DEBUG_OBJECT (self,
          "Mode replace and new CC meta, removing existing CC meta");
      gst_buffer_foreach_meta (frame->buffer, drop_cc_meta, NULL);
    }

    ccdata[0] |= (base_line1 < i ? i - base_line1 : 0) & 0x1f;
    ccdata[1] = sliced[0].data[0];
    ccdata[2] = sliced[0].data[1];
    ccdata[3] |= (base_line2 < i ? i - base_line2 : 0) & 0x1f;
    ccdata[4] = sliced[1].data[0];
    ccdata[5] = sliced[1].data[1];
    gst_buffer_add_video_caption_meta (frame->buffer,
        GST_VIDEO_CAPTION_TYPE_CEA608_S334_1A, ccdata, 6);
    GST_TRACE_OBJECT (self,
        "Got CC 0x%02x 0x%02x / 0x%02x 0x%02x '%c%c / %c%c'", ccdata[1],
        ccdata[2], ccdata[4], ccdata[5],
        g_ascii_isprint (ccdata[1] & 0x7f) ? ccdata[1] & 0x7f : '.',
        g_ascii_isprint (ccdata[2] & 0x7f) ? ccdata[2] & 0x7f : '.',
        g_ascii_isprint (ccdata[4] & 0x7f) ? ccdata[4] & 0x7f : '.',
        g_ascii_isprint (ccdata[5] & 0x7f) ? ccdata[5] & 0x7f : '.');

  }

  return found;
}

static GstFlowReturn
gst_line_21_decoder_transform_ip (GstVideoFilter * filter,
    GstVideoFrame * frame)
{
  GstLine21Decoder *self = (GstLine21Decoder *) filter;

  if (!self->compatible_format)
    return GST_FLOW_OK;

  gst_line_21_decoder_scan (self, frame);
  return GST_FLOW_OK;
}

static gboolean
gst_line_21_decoder_stop (GstBaseTransform * btrans)
{
  GstLine21Decoder *self = (GstLine21Decoder *) btrans;

  vbi_raw_decoder_destroy (&self->zvbi_decoder);
  if (self->info) {
    gst_video_info_free (self->info);
    self->info = NULL;
  }

  return TRUE;
}

static void
gst_line_21_decoder_finalize (GObject * object)
{
  GstLine21Decoder *self = (GstLine21Decoder *) object;

  if (self->info) {
    gst_video_info_free (self->info);
    self->info = NULL;
  }
  g_free (self->converted_lines);
  self->converted_lines = NULL;

  G_OBJECT_CLASS (parent_class)->finalize (object);
}
