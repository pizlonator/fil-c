/*
 * GStreamer
 *
 * Copyright (C) 2012 Cisco Systems, Inc.
 *   Author: Youness Alaoui <youness.alaoui@collabora.co.uk>
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
 * SECTION:element-uvch264src
 * @title: uvch264src
 *
 * A camera bin src element that wraps v4l2src and implements UVC H264
 * Extension Units (XU) to control the H264 encoder in the camera
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gstuvch264_src.h"

enum
{
  PROP_0,
  /* uvch264_src properties */
  PROP_COLORSPACE_NAME,
  PROP_JPEG_DECODER_NAME,
  PROP_NUM_CLOCK_SAMPLES,
  /* v4l2src properties */
  PROP_NUM_BUFFERS,
  PROP_DEVICE,
  PROP_DEVICE_NAME,
  /* Static controls */
  PROP_INITIAL_BITRATE,
  PROP_SLICE_UNITS,
  PROP_SLICE_MODE,
  PROP_IFRAME_PERIOD,
  PROP_USAGE_TYPE,
  PROP_ENTROPY,
  PROP_ENABLE_SEI,
  PROP_NUM_REORDER_FRAMES,
  PROP_PREVIEW_FLIPPED,
  PROP_LEAKY_BUCKET_SIZE,
  /* Dynamic controls */
  PROP_RATE_CONTROL,
  PROP_FIXED_FRAMERATE,
  PROP_MAX_MBPS,                /* read-only */
  PROP_LEVEL_IDC,
  PROP_PEAK_BITRATE,
  PROP_AVERAGE_BITRATE,
  PROP_MIN_IFRAME_QP,
  PROP_MAX_IFRAME_QP,
  PROP_MIN_PFRAME_QP,
  PROP_MAX_PFRAME_QP,
  PROP_MIN_BFRAME_QP,
  PROP_MAX_BFRAME_QP,
  PROP_LTR_BUFFER_SIZE,
  PROP_LTR_ENCODER_CONTROL,
};
/* In caps : frame interval (fps), width, height, profile, mux */
/* Ignored: temporal, spatial, SNR, MVC views, version, reset */
/* Events: LTR, generate IDR */

enum
{
  /* action signals */
  SIGNAL_GET_ENUM_SETTING,
  SIGNAL_GET_BOOLEAN_SETTING,
  SIGNAL_GET_INT_SETTING,
  LAST_SIGNAL
};

static guint _signals[LAST_SIGNAL];

/* Default values */
#define DEFAULT_COLORSPACE_NAME "videoconvert"
#define DEFAULT_JPEG_DECODER_NAME "jpegdec"
#define DEFAULT_NUM_CLOCK_SAMPLES 0
#define DEFAULT_NUM_BUFFERS -1
#define DEFAULT_DEVICE "/dev/video0"
#define DEFAULT_DEVICE_NAME NULL
#define DEFAULT_INITIAL_BITRATE 3000000
#define DEFAULT_SLICE_UNITS 4
#define DEFAULT_SLICE_MODE UVC_H264_SLICEMODE_SLICEPERFRAME
#define DEFAULT_IFRAME_PERIOD 10000
#define DEFAULT_USAGE_TYPE UVC_H264_USAGETYPE_REALTIME
#define DEFAULT_ENTROPY UVC_H264_ENTROPY_CAVLC
#define DEFAULT_ENABLE_SEI FALSE
#define DEFAULT_NUM_REORDER_FRAMES 0
#define DEFAULT_PREVIEW_FLIPPED FALSE
#define DEFAULT_LEAKY_BUCKET_SIZE 1000
#define DEFAULT_RATE_CONTROL UVC_H264_RATECONTROL_CBR
#define DEFAULT_FIXED_FRAMERATE FALSE
#define DEFAULT_LEVEL_IDC 40
#define DEFAULT_PEAK_BITRATE DEFAULT_INITIAL_BITRATE
#define DEFAULT_AVERAGE_BITRATE DEFAULT_INITIAL_BITRATE
#define DEFAULT_MIN_QP 10
#define DEFAULT_MAX_QP 46
#define DEFAULT_LTR_BUFFER_SIZE 0
#define DEFAULT_LTR_ENCODER_CONTROL 0

#define NSEC_PER_SEC (G_USEC_PER_SEC * 1000)


GST_DEBUG_CATEGORY (uvc_h264_src_debug);
#define GST_CAT_DEFAULT uvc_h264_src_debug

#define gst_uvc_h264_src_parent_class parent_class
G_DEFINE_TYPE (GstUvcH264Src, gst_uvc_h264_src, GST_TYPE_BASE_CAMERA_SRC);
GST_ELEMENT_REGISTER_DEFINE (uvch264src, "uvch264src", GST_RANK_NONE,
    GST_TYPE_UVC_H264_SRC);

#define GST_UVC_H264_SRC_VF_CAPS_STR \
  GST_VIDEO_CAPS_MAKE (GST_VIDEO_FORMATS_ALL) ";" \
  "image/jpeg,"                                   \
  "width = " GST_VIDEO_SIZE_RANGE ","             \
  "height = " GST_VIDEO_SIZE_RANGE ","            \
  "framerate = " GST_VIDEO_FPS_RANGE

#define GST_UVC_H264_SRC_VID_CAPS_STR                                   \
  GST_UVC_H264_SRC_VF_CAPS_STR ";"                                      \
  "video/x-h264, "                                                      \
  "width = " GST_VIDEO_SIZE_RANGE ", "                                  \
  "height = " GST_VIDEO_SIZE_RANGE ", "                                 \
  "framerate = " GST_VIDEO_FPS_RANGE ", "                               \
  "stream-format = (string) { byte-stream, avc }, "                     \
  "alignment = (string) au, "                                           \
  "profile = (string) { high, main, baseline, constrained-baseline }"

/**
 * GstUvcH264Src!vfsrc:
 *
 * The video src pad template
 */
static GstStaticPadTemplate vfsrc_template =
GST_STATIC_PAD_TEMPLATE (GST_BASE_CAMERA_SRC_VIEWFINDER_PAD_NAME,
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_UVC_H264_SRC_VF_CAPS_STR));

static GstStaticPadTemplate imgsrc_template =
GST_STATIC_PAD_TEMPLATE (GST_BASE_CAMERA_SRC_IMAGE_PAD_NAME,
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_NONE);

static GstStaticPadTemplate vidsrc_template =
GST_STATIC_PAD_TEMPLATE (GST_BASE_CAMERA_SRC_VIDEO_PAD_NAME,
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_UVC_H264_SRC_VID_CAPS_STR));


static void gst_uvc_h264_src_dispose (GObject * object);
static void gst_uvc_h264_src_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_uvc_h264_src_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);
static gboolean gst_uvc_h264_src_event (GstPad * pad, GstObject * parent,
    GstEvent * event);
static gboolean gst_uvc_h264_src_send_event (GstElement * element,
    GstEvent * event);
static gboolean gst_uvc_h264_src_construct_pipeline (GstBaseCameraSrc *
    bcamsrc);
static gboolean gst_uvc_h264_src_set_mode (GstBaseCameraSrc * bcamsrc,
    GstCameraBinMode mode);
static gboolean gst_uvc_h264_src_start_capture (GstBaseCameraSrc * camerasrc);
static void gst_uvc_h264_src_stop_capture (GstBaseCameraSrc * camerasrc);
static GstStateChangeReturn gst_uvc_h264_src_change_state (GstElement * element,
    GstStateChange trans);
static GstPadProbeReturn gst_uvc_h264_src_buffer_probe (GstPad * pad,
    GstPadProbeInfo * info, gpointer user_data);
static GstPadProbeReturn gst_uvc_h264_src_event_probe (GstPad * pad,
    GstPadProbeInfo * info, gpointer user_data);
static void gst_uvc_h264_src_pad_linking_cb (GstPad * pad,
    GstPad * peer, gpointer user_data);
static gboolean gst_uvc_h264_src_query (GstPad * pad, GstObject * parent,
    GstQuery * query);


static void v4l2src_prepare_format (GstElement * v4l2src, gint fd,
    GstCaps * caps, gpointer user_data);
static void fill_probe_commit (GstUvcH264Src * self,
    uvcx_video_config_probe_commit_t * probe, guint32 frame_interval,
    guint32 width, guint32 height, guint32 profile,
    UvcH264StreamFormat stream_format);
static gboolean xu_query (GstUvcH264Src * self, guint selector, guint query,
    guchar * data);

static void set_rate_control (GstUvcH264Src * self);
static void set_level_idc (GstUvcH264Src * self);
static void set_bitrate (GstUvcH264Src * self);
static void set_qp (GstUvcH264Src * self, gint type);
static void set_ltr (GstUvcH264Src * self);
static void update_rate_control (GstUvcH264Src * self);
static guint32 update_level_idc_and_get_max_mbps (GstUvcH264Src * self);
static void update_bitrate (GstUvcH264Src * self);
static gboolean update_qp (GstUvcH264Src * self, gint type);
static void update_ltr (GstUvcH264Src * self);

static gboolean gst_uvc_h264_src_get_enum_setting (GstUvcH264Src * self,
    gchar * property, gint * mask, gint * default_value);
static gboolean gst_uvc_h264_src_get_boolean_setting (GstUvcH264Src * self,
    gchar * property, gboolean * changeable, gboolean * def);
static gboolean gst_uvc_h264_src_get_int_setting (GstUvcH264Src * self,
    gchar * property, gint * min, gint * def, gint * max);

static void
gst_uvc_h264_src_class_init (GstUvcH264SrcClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseCameraSrcClass *gstbasecamerasrc_class;

  parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gstelement_class = GST_ELEMENT_CLASS (klass);
  gstbasecamerasrc_class = GST_BASE_CAMERA_SRC_CLASS (klass);

  gobject_class->dispose = gst_uvc_h264_src_dispose;
  gobject_class->set_property = gst_uvc_h264_src_set_property;
  gobject_class->get_property = gst_uvc_h264_src_get_property;

  gstelement_class->change_state = gst_uvc_h264_src_change_state;
  gstelement_class->send_event = gst_uvc_h264_src_send_event;

  gstbasecamerasrc_class->construct_pipeline =
      gst_uvc_h264_src_construct_pipeline;
  gstbasecamerasrc_class->set_mode = gst_uvc_h264_src_set_mode;
  gstbasecamerasrc_class->start_capture = gst_uvc_h264_src_start_capture;
  gstbasecamerasrc_class->stop_capture = gst_uvc_h264_src_stop_capture;

  GST_DEBUG_CATEGORY_INIT (uvc_h264_src_debug, "uvch264src",
      0, "UVC H264 Compliant camera bin source");

  gst_element_class_set_static_metadata (gstelement_class,
      "UVC H264 Source",
      "Source/Video",
      "UVC H264 Encoding camera source",
      "Youness Alaoui <youness.alaoui@collabora.co.uk>");

  gst_element_class_add_static_pad_template (gstelement_class,
      &vidsrc_template);
  gst_element_class_add_static_pad_template (gstelement_class,
      &imgsrc_template);
  gst_element_class_add_static_pad_template (gstelement_class, &vfsrc_template);

  /* Properties */
  g_object_class_install_property (gobject_class, PROP_COLORSPACE_NAME,
      g_param_spec_string ("colorspace-name", "colorspace element name",
          "The name of the colorspace element",
          DEFAULT_COLORSPACE_NAME, G_PARAM_CONSTRUCT | G_PARAM_READWRITE |
          GST_PARAM_MUTABLE_READY | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_JPEG_DECODER_NAME,
      g_param_spec_string ("jpeg-decoder-name", "jpeg decoder element name",
          "The name of the jpeg decoder element",
          DEFAULT_JPEG_DECODER_NAME, G_PARAM_CONSTRUCT | G_PARAM_READWRITE |
          GST_PARAM_MUTABLE_READY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_NUM_CLOCK_SAMPLES,
      g_param_spec_int ("num-clock-samples", "num-clock-samples",
          "Number of clock samples to gather for the PTS synchronization"
          " (-1 = unlimited)",
          0, G_MAXINT, DEFAULT_NUM_CLOCK_SAMPLES,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | GST_PARAM_MUTABLE_PLAYING |
          G_PARAM_STATIC_STRINGS));

  /* v4l2src proxied properties */
  g_object_class_install_property (gobject_class, PROP_NUM_BUFFERS,
      g_param_spec_int ("num-buffers", "num-buffers",
          "Number of buffers to output before sending EOS (-1 = unlimited)",
          -1, G_MAXINT, DEFAULT_NUM_BUFFERS,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_DEVICE,
      g_param_spec_string ("device", "device",
          "Device location",
          DEFAULT_DEVICE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_DEVICE_NAME,
      g_param_spec_string ("device-name", "Device name",
          "Name of the device", DEFAULT_DEVICE_NAME,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /* Static controls */
  g_object_class_install_property (gobject_class, PROP_INITIAL_BITRATE,
      g_param_spec_uint ("initial-bitrate", "Initial bitrate",
          "Initial bitrate in bits/second (static control)",
          0, G_MAXUINT, DEFAULT_INITIAL_BITRATE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));
  g_object_class_install_property (gobject_class, PROP_SLICE_UNITS,
      g_param_spec_uint ("slice-units", "Slice units",
          "Slice units (static control)",
          0, G_MAXUINT16, DEFAULT_SLICE_UNITS,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));
  g_object_class_install_property (gobject_class, PROP_SLICE_MODE,
      g_param_spec_enum ("slice-mode", "Slice mode",
          "Defines the unit of the slice-units property (static control)",
          UVC_H264_SLICEMODE_TYPE,
          DEFAULT_SLICE_MODE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));
  g_object_class_install_property (gobject_class, PROP_IFRAME_PERIOD,
      g_param_spec_uint ("iframe-period", "I Frame Period",
          "Time between IDR frames in milliseconds (static control)",
          0, G_MAXUINT16, DEFAULT_IFRAME_PERIOD,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));
  g_object_class_install_property (gobject_class, PROP_USAGE_TYPE,
      g_param_spec_enum ("usage-type", "Usage type",
          "The usage type (static control)",
          UVC_H264_USAGETYPE_TYPE, DEFAULT_USAGE_TYPE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));
  g_object_class_install_property (gobject_class, PROP_ENTROPY,
      g_param_spec_enum ("entropy", "Entropy",
          "Entropy (static control)",
          UVC_H264_ENTROPY_TYPE, DEFAULT_ENTROPY,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));
  g_object_class_install_property (gobject_class, PROP_ENABLE_SEI,
      g_param_spec_boolean ("enable-sei", "Enable SEI",
          "Enable SEI picture timing (static control)",
          DEFAULT_ENABLE_SEI, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));
  g_object_class_install_property (gobject_class, PROP_NUM_REORDER_FRAMES,
      g_param_spec_uint ("num-reorder-frames", "Number of Reorder frames",
          "Number of B frames between the references frames (static control)",
          0, G_MAXUINT8, DEFAULT_NUM_REORDER_FRAMES,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));
  g_object_class_install_property (gobject_class, PROP_PREVIEW_FLIPPED,
      g_param_spec_boolean ("preview-flipped", "Flip preview",
          "Horizontal flipped image for non H.264 streams (static control)",
          DEFAULT_PREVIEW_FLIPPED, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));
  g_object_class_install_property (gobject_class, PROP_LEAKY_BUCKET_SIZE,
      g_param_spec_uint ("leaky-bucket-size", "Size of the leaky bucket size",
          "Size of the leaky bucket size in milliseconds (static control)",
          0, G_MAXUINT16, DEFAULT_LEAKY_BUCKET_SIZE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  /* Dynamic controls */
  g_object_class_install_property (gobject_class, PROP_RATE_CONTROL,
      g_param_spec_enum ("rate-control", "Rate control",
          "Rate control mode (static & dynamic control)",
          UVC_H264_RATECONTROL_TYPE, DEFAULT_RATE_CONTROL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_PLAYING));
  g_object_class_install_property (gobject_class, PROP_FIXED_FRAMERATE,
      g_param_spec_boolean ("fixed-framerate", "Fixed framerate",
          "Fixed framerate (static & dynamic control)",
          DEFAULT_FIXED_FRAMERATE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_PLAYING));
  g_object_class_install_property (gobject_class, PROP_MAX_MBPS,
      g_param_spec_uint ("max-mbps", "Max macroblocks/second",
          "The number of macroblocks per second for the maximum processing rate",
          0, G_MAXUINT, 0, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_LEVEL_IDC,
      g_param_spec_uint ("level-idc", "Level IDC",
          "Level IDC (dynamic control)",
          0, G_MAXUINT8, DEFAULT_LEVEL_IDC,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_PLAYING));
  g_object_class_install_property (gobject_class, PROP_PEAK_BITRATE,
      g_param_spec_uint ("peak-bitrate", "Peak bitrate",
          "The peak bitrate in bits/second (dynamic control)",
          0, G_MAXUINT, DEFAULT_PEAK_BITRATE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_PLAYING));
  g_object_class_install_property (gobject_class, PROP_AVERAGE_BITRATE,
      g_param_spec_uint ("average-bitrate", "Average bitrate",
          "The average bitrate in bits/second (dynamic control)",
          0, G_MAXUINT, DEFAULT_AVERAGE_BITRATE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_PLAYING));
  g_object_class_install_property (gobject_class, PROP_MIN_IFRAME_QP,
      g_param_spec_int ("min-iframe-qp", "Minimum I frame QP",
          "The minimum Quantization step size for I frames (dynamic control)",
          -G_MAXINT8, G_MAXINT8, DEFAULT_MIN_QP,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_PLAYING));
  g_object_class_install_property (gobject_class, PROP_MAX_IFRAME_QP,
      g_param_spec_int ("max-iframe-qp", "Minimum I frame QP",
          "The minimum Quantization step size for I frames (dynamic control)",
          -G_MAXINT8, G_MAXINT8, DEFAULT_MAX_QP,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_PLAYING));
  g_object_class_install_property (gobject_class, PROP_MIN_PFRAME_QP,
      g_param_spec_int ("min-pframe-qp", "Minimum P frame QP",
          "The minimum Quantization step size for P frames (dynamic control)",
          -G_MAXINT8, G_MAXINT8, DEFAULT_MIN_QP,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_PLAYING));
  g_object_class_install_property (gobject_class, PROP_MAX_PFRAME_QP,
      g_param_spec_int ("max-pframe-qp", "Minimum P frame QP",
          "The minimum Quantization step size for P frames (dynamic control)",
          -G_MAXINT8, G_MAXINT8, DEFAULT_MAX_QP,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_PLAYING));
  g_object_class_install_property (gobject_class, PROP_MIN_BFRAME_QP,
      g_param_spec_int ("min-bframe-qp", "Minimum B frame QP",
          "The minimum Quantization step size for B frames (dynamic control)",
          -G_MAXINT8, G_MAXINT8, DEFAULT_MIN_QP,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_PLAYING));
  g_object_class_install_property (gobject_class, PROP_MAX_BFRAME_QP,
      g_param_spec_int ("max-bframe-qp", "Minimum B frame QP",
          "The minimum Quantization step size for B frames (dynamic control)",
          -G_MAXINT8, G_MAXINT8, DEFAULT_MAX_QP,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_PLAYING));
  g_object_class_install_property (gobject_class, PROP_LTR_BUFFER_SIZE,
      g_param_spec_int ("ltr-buffer-size", "LTR Buffer size",
          "Total number of Long-Term Reference frames (dynamic control)",
          0, G_MAXUINT8, DEFAULT_LTR_BUFFER_SIZE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_PLAYING));
  g_object_class_install_property (gobject_class, PROP_LTR_ENCODER_CONTROL,
      g_param_spec_int ("ltr-encoder-control",
          "LTR frames controlled by device",
          "Number of LTR frames the device can control (dynamic control)", 0,
          G_MAXUINT8, DEFAULT_LTR_ENCODER_CONTROL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_PLAYING));

  _signals[SIGNAL_GET_ENUM_SETTING] =
      g_signal_new_class_handler ("get-enum-setting",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
      G_CALLBACK (gst_uvc_h264_src_get_enum_setting),
      NULL, NULL, NULL,
      G_TYPE_BOOLEAN, 3, G_TYPE_STRING, G_TYPE_POINTER, G_TYPE_POINTER, 0);
  _signals[SIGNAL_GET_BOOLEAN_SETTING] =
      g_signal_new_class_handler ("get-boolean-setting",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
      G_CALLBACK (gst_uvc_h264_src_get_boolean_setting), NULL, NULL, NULL,
      G_TYPE_BOOLEAN, 3, G_TYPE_STRING, G_TYPE_POINTER, G_TYPE_POINTER, 0);
  _signals[SIGNAL_GET_INT_SETTING] =
      g_signal_new_class_handler ("get-int-setting",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
      G_CALLBACK (gst_uvc_h264_src_get_int_setting), NULL, NULL, NULL,
      G_TYPE_BOOLEAN, 4, G_TYPE_STRING, G_TYPE_POINTER, G_TYPE_POINTER,
      G_TYPE_POINTER, 0);

  gst_type_mark_as_plugin_api (UVC_H264_ENTROPY_TYPE, 0);
  gst_type_mark_as_plugin_api (UVC_H264_RATECONTROL_TYPE, 0);
  gst_type_mark_as_plugin_api (UVC_H264_SLICEMODE_TYPE, 0);
  gst_type_mark_as_plugin_api (UVC_H264_USAGETYPE_TYPE, 0);
}

static void
gst_uvc_h264_src_init (GstUvcH264Src * self)
{
  self->vfsrc =
      gst_ghost_pad_new_no_target (GST_BASE_CAMERA_SRC_VIEWFINDER_PAD_NAME,
      GST_PAD_SRC);
  gst_pad_set_query_function (self->vfsrc,
      GST_DEBUG_FUNCPTR (gst_uvc_h264_src_query));
  gst_element_add_pad (GST_ELEMENT (self), self->vfsrc);

  self->imgsrc =
      gst_ghost_pad_new_no_target (GST_BASE_CAMERA_SRC_IMAGE_PAD_NAME,
      GST_PAD_SRC);
  gst_element_add_pad (GST_ELEMENT (self), self->imgsrc);

  self->vidsrc =
      gst_ghost_pad_new_no_target (GST_BASE_CAMERA_SRC_VIDEO_PAD_NAME,
      GST_PAD_SRC);
  gst_pad_set_query_function (self->vidsrc,
      GST_DEBUG_FUNCPTR (gst_uvc_h264_src_query));
  gst_element_add_pad (GST_ELEMENT (self), self->vidsrc);
  gst_pad_add_probe (self->vidsrc, GST_PAD_PROBE_TYPE_BUFFER,
      gst_uvc_h264_src_buffer_probe, self, NULL);
  gst_pad_add_probe (self->vfsrc, GST_PAD_PROBE_TYPE_EVENT_UPSTREAM,
      gst_uvc_h264_src_event_probe, self, NULL);
  gst_pad_add_probe (self->vidsrc,
      GST_PAD_PROBE_TYPE_EVENT_UPSTREAM | GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM,
      gst_uvc_h264_src_event_probe, self, NULL);

  self->srcpad_event_func = GST_PAD_EVENTFUNC (self->vfsrc);

  gst_pad_set_event_function (self->imgsrc, gst_uvc_h264_src_event);
  gst_pad_set_event_function (self->vidsrc, gst_uvc_h264_src_event);
  gst_pad_set_event_function (self->vfsrc, gst_uvc_h264_src_event);

  g_signal_connect (self->vidsrc, "linked",
      (GCallback) gst_uvc_h264_src_pad_linking_cb, self);
  g_signal_connect (self->vidsrc, "unlinked",
      (GCallback) gst_uvc_h264_src_pad_linking_cb, self);
  g_signal_connect (self->vfsrc, "linked",
      (GCallback) gst_uvc_h264_src_pad_linking_cb, self);
  g_signal_connect (self->vfsrc, "unlinked",
      (GCallback) gst_uvc_h264_src_pad_linking_cb, self);

  self->v4l2_fd = -1;
  gst_base_camera_src_set_mode (GST_BASE_CAMERA_SRC (self), MODE_VIDEO);

  self->main_format = UVC_H264_SRC_FORMAT_NONE;
  self->main_width = 0;
  self->main_height = 0;
  self->main_frame_interval = 0;
  self->main_stream_format = UVC_H264_STREAMFORMAT_ANNEXB;
  self->main_profile = UVC_H264_PROFILE_CONSTRAINED_BASELINE;
  self->secondary_format = UVC_H264_SRC_FORMAT_NONE;
  self->secondary_width = 0;
  self->secondary_height = 0;
  self->secondary_frame_interval = 0;

  /* v4l2src properties */
  self->num_buffers = DEFAULT_NUM_BUFFERS;
  self->device = g_strdup (DEFAULT_DEVICE);

  /* Static controls */
  self->initial_bitrate = DEFAULT_INITIAL_BITRATE;
  self->slice_units = DEFAULT_SLICE_UNITS;
  self->slice_mode = DEFAULT_SLICE_MODE;
  self->iframe_period = DEFAULT_IFRAME_PERIOD;
  self->usage_type = DEFAULT_USAGE_TYPE;
  self->entropy = DEFAULT_ENTROPY;
  self->enable_sei = DEFAULT_ENABLE_SEI;
  self->num_reorder_frames = DEFAULT_NUM_REORDER_FRAMES;
  self->preview_flipped = DEFAULT_PREVIEW_FLIPPED;
  self->leaky_bucket_size = DEFAULT_LEAKY_BUCKET_SIZE;

  /* Dynamic controls */
  self->rate_control = DEFAULT_RATE_CONTROL;
  self->fixed_framerate = DEFAULT_FIXED_FRAMERATE;
  self->level_idc = DEFAULT_LEVEL_IDC;
  self->peak_bitrate = DEFAULT_PEAK_BITRATE;
  self->average_bitrate = DEFAULT_AVERAGE_BITRATE;
  self->min_qp[QP_I_FRAME] = DEFAULT_MIN_QP;
  self->max_qp[QP_I_FRAME] = DEFAULT_MAX_QP;
  self->min_qp[QP_P_FRAME] = DEFAULT_MIN_QP;
  self->max_qp[QP_P_FRAME] = DEFAULT_MAX_QP;
  self->min_qp[QP_B_FRAME] = DEFAULT_MIN_QP;
  self->max_qp[QP_B_FRAME] = DEFAULT_MAX_QP;
  self->ltr_buffer_size = DEFAULT_LTR_BUFFER_SIZE;
  self->ltr_encoder_control = DEFAULT_LTR_ENCODER_CONTROL;
}

static void
gst_uvc_h264_src_dispose (GObject * object)
{
  GstUvcH264Src *self = GST_UVC_H264_SRC (object);

  if (self->usb_ctx)
    libusb_exit (self->usb_ctx);
  self->usb_ctx = NULL;
  g_free (self->jpeg_decoder_name);
  self->jpeg_decoder_name = NULL;
  g_free (self->colorspace_name);
  self->colorspace_name = NULL;
  g_free (self->device);
  self->device = NULL;

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_uvc_h264_src_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GstUvcH264Src *self = GST_UVC_H264_SRC (object);

  switch (prop_id) {
    case PROP_COLORSPACE_NAME:
      g_free (self->colorspace_name);
      self->colorspace_name = g_value_dup_string (value);
      break;
    case PROP_JPEG_DECODER_NAME:
      g_free (self->jpeg_decoder_name);
      self->jpeg_decoder_name = g_value_dup_string (value);
      break;
    case PROP_NUM_CLOCK_SAMPLES:
      self->num_clock_samples = g_value_get_int (value);
      if (self->mjpg_demux)
        g_object_set (self->mjpg_demux,
            "num-clock-samples", self->num_clock_samples, NULL);
      break;
      /* v4l2 properties */
    case PROP_NUM_BUFFERS:
      self->num_buffers = g_value_get_int (value);
      if (self->v4l2_src)
        g_object_set_property (G_OBJECT (self->v4l2_src), "num-buffers", value);
      break;
    case PROP_DEVICE:
      g_free (self->device);
      self->device = g_value_dup_string (value);
      if (self->v4l2_src)
        g_object_set_property (G_OBJECT (self->v4l2_src), "device", value);
      break;
      /* Static controls */
    case PROP_INITIAL_BITRATE:
      self->initial_bitrate = g_value_get_uint (value);
      break;
    case PROP_SLICE_UNITS:
      self->slice_units = g_value_get_uint (value);
      break;
    case PROP_SLICE_MODE:
      self->slice_mode = g_value_get_enum (value);
      break;
    case PROP_IFRAME_PERIOD:
      self->iframe_period = g_value_get_uint (value);
      break;
    case PROP_USAGE_TYPE:
      self->usage_type = g_value_get_enum (value);
      break;
    case PROP_ENTROPY:
      self->entropy = g_value_get_enum (value);
      break;
    case PROP_ENABLE_SEI:
      self->enable_sei = g_value_get_boolean (value);
      break;
    case PROP_NUM_REORDER_FRAMES:
      self->num_reorder_frames = g_value_get_uint (value);
      break;
    case PROP_PREVIEW_FLIPPED:
      self->preview_flipped = g_value_get_boolean (value);
      break;
    case PROP_LEAKY_BUCKET_SIZE:
      self->leaky_bucket_size = g_value_get_uint (value);
      break;


      /* Dynamic controls */
    case PROP_RATE_CONTROL:
      self->rate_control = g_value_get_enum (value);
      set_rate_control (self);
      update_rate_control (self);
      break;
    case PROP_FIXED_FRAMERATE:
      self->fixed_framerate = g_value_get_boolean (value);
      set_rate_control (self);
      update_rate_control (self);
      break;
    case PROP_LEVEL_IDC:
      self->level_idc = g_value_get_uint (value);
      set_level_idc (self);
      update_level_idc_and_get_max_mbps (self);
      break;
    case PROP_PEAK_BITRATE:
      self->peak_bitrate = g_value_get_uint (value);
      set_bitrate (self);
      update_bitrate (self);
      break;
    case PROP_AVERAGE_BITRATE:
      self->average_bitrate = g_value_get_uint (value);
      set_bitrate (self);
      update_bitrate (self);
      break;
    case PROP_MIN_IFRAME_QP:
      self->min_qp[QP_I_FRAME] = g_value_get_int (value);
      set_qp (self, QP_I_FRAME);
      update_qp (self, QP_I_FRAME);
      break;
    case PROP_MAX_IFRAME_QP:
      self->max_qp[QP_I_FRAME] = g_value_get_int (value);
      set_qp (self, QP_I_FRAME);
      update_qp (self, QP_I_FRAME);
      break;
    case PROP_MIN_PFRAME_QP:
      self->min_qp[QP_P_FRAME] = g_value_get_int (value);
      set_qp (self, QP_P_FRAME);
      update_qp (self, QP_P_FRAME);
      break;
    case PROP_MAX_PFRAME_QP:
      self->max_qp[QP_P_FRAME] = g_value_get_int (value);
      set_qp (self, QP_P_FRAME);
      update_qp (self, QP_P_FRAME);
      break;
    case PROP_MIN_BFRAME_QP:
      self->min_qp[QP_B_FRAME] = g_value_get_int (value);
      set_qp (self, QP_B_FRAME);
      update_qp (self, QP_B_FRAME);
      break;
    case PROP_MAX_BFRAME_QP:
      self->max_qp[QP_B_FRAME] = g_value_get_int (value);
      set_qp (self, QP_B_FRAME);
      update_qp (self, QP_B_FRAME);
      break;
    case PROP_LTR_BUFFER_SIZE:
      self->ltr_buffer_size = g_value_get_int (value);
      set_ltr (self);
      update_ltr (self);
      break;
    case PROP_LTR_ENCODER_CONTROL:
      self->ltr_encoder_control = g_value_get_int (value);
      set_ltr (self);
      update_ltr (self);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (self, prop_id, pspec);
      break;
  }
}

static void
gst_uvc_h264_src_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstUvcH264Src *self = GST_UVC_H264_SRC (object);
  uvcx_video_config_probe_commit_t probe;

  switch (prop_id) {
    case PROP_INITIAL_BITRATE:
    case PROP_SLICE_UNITS:
    case PROP_SLICE_MODE:
    case PROP_IFRAME_PERIOD:
    case PROP_USAGE_TYPE:
    case PROP_ENTROPY:
    case PROP_ENABLE_SEI:
    case PROP_NUM_REORDER_FRAMES:
    case PROP_PREVIEW_FLIPPED:
    case PROP_LEAKY_BUCKET_SIZE:
      fill_probe_commit (self, &probe, 0, 0, 0, 0, 0);
      if (GST_STATE (self) >= GST_STATE_PAUSED) {
        if (!xu_query (self, UVCX_VIDEO_CONFIG_PROBE, UVC_GET_CUR,
                (guchar *) & probe))
          GST_WARNING_OBJECT (self, "probe_setting GET_CUR error");
      }
      break;
    default:
      break;
  }

  switch (prop_id) {
    case PROP_COLORSPACE_NAME:
      g_value_set_string (value, self->colorspace_name);
      break;
    case PROP_JPEG_DECODER_NAME:
      g_value_set_string (value, self->jpeg_decoder_name);
      break;
    case PROP_NUM_CLOCK_SAMPLES:
      g_value_set_int (value, self->num_clock_samples);
      break;
      /* v4l2src properties */
    case PROP_NUM_BUFFERS:
      g_value_set_int (value, self->num_buffers);
      break;
    case PROP_DEVICE:
      g_value_set_string (value, self->device);
      break;
    case PROP_DEVICE_NAME:
      if (self->v4l2_src)
        g_object_get_property (G_OBJECT (self->v4l2_src), "device-name", value);
      else
        g_value_set_static_string (value, "");
      break;
      /* Static controls */
    case PROP_INITIAL_BITRATE:
      g_value_set_uint (value, probe.dwBitRate);
      break;
    case PROP_SLICE_UNITS:
      g_value_set_uint (value, probe.wSliceUnits);
      break;
    case PROP_SLICE_MODE:
      g_value_set_enum (value, probe.wSliceMode);
      break;
    case PROP_IFRAME_PERIOD:
      g_value_set_uint (value, probe.wIFramePeriod);
      break;
    case PROP_USAGE_TYPE:
      g_value_set_enum (value, probe.bUsageType);
      break;
    case PROP_ENTROPY:
      g_value_set_enum (value, probe.bEntropyCABAC);
      break;
    case PROP_ENABLE_SEI:
      g_value_set_boolean (value,
          (probe.bTimestamp == UVC_H264_TIMESTAMP_SEI_ENABLE));
      break;
    case PROP_NUM_REORDER_FRAMES:
      g_value_set_uint (value, probe.bNumOfReorderFrames);
      break;
    case PROP_PREVIEW_FLIPPED:
      g_value_set_boolean (value,
          (probe.bPreviewFlipped == UVC_H264_PREFLIPPED_HORIZONTAL));
      break;
    case PROP_LEAKY_BUCKET_SIZE:
      g_value_set_uint (value, probe.wLeakyBucketSize);
      break;

      /* Dynamic controls */
    case PROP_RATE_CONTROL:
      update_rate_control (self);
      g_value_set_enum (value, self->rate_control);
      break;
    case PROP_FIXED_FRAMERATE:
      update_rate_control (self);
      g_value_set_boolean (value, self->fixed_framerate);
      break;
    case PROP_MAX_MBPS:
      g_value_set_uint (value, update_level_idc_and_get_max_mbps (self));
      break;
    case PROP_LEVEL_IDC:
      update_level_idc_and_get_max_mbps (self);
      g_value_set_uint (value, self->level_idc);
      break;
    case PROP_PEAK_BITRATE:
      update_bitrate (self);
      g_value_set_uint (value, self->peak_bitrate);
      break;
    case PROP_AVERAGE_BITRATE:
      update_bitrate (self);
      g_value_set_uint (value, self->average_bitrate);
      break;
    case PROP_MIN_IFRAME_QP:
      update_qp (self, QP_I_FRAME);
      g_value_set_int (value, self->min_qp[QP_I_FRAME]);
      break;
    case PROP_MAX_IFRAME_QP:
      update_qp (self, QP_I_FRAME);
      g_value_set_int (value, self->max_qp[QP_I_FRAME]);
      break;
    case PROP_MIN_PFRAME_QP:
      update_qp (self, QP_P_FRAME);
      g_value_set_int (value, self->min_qp[QP_P_FRAME]);
      break;
    case PROP_MAX_PFRAME_QP:
      update_qp (self, QP_P_FRAME);
      g_value_set_int (value, self->max_qp[QP_P_FRAME]);
      break;
    case PROP_MIN_BFRAME_QP:
      update_qp (self, QP_B_FRAME);
      g_value_set_int (value, self->min_qp[QP_B_FRAME]);
      break;
    case PROP_MAX_BFRAME_QP:
      update_qp (self, QP_B_FRAME);
      g_value_set_int (value, self->max_qp[QP_B_FRAME]);
      break;
    case PROP_LTR_BUFFER_SIZE:
      update_ltr (self);
      g_value_set_int (value, self->ltr_buffer_size);
      break;
    case PROP_LTR_ENCODER_CONTROL:
      update_ltr (self);
      g_value_set_int (value, self->ltr_encoder_control);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (self, prop_id, pspec);
      break;
  }
}

/* Set dynamic controls */
static void
set_rate_control (GstUvcH264Src * self)
{
  uvcx_rate_control_mode_t req;

  if (!xu_query (self, UVCX_RATE_CONTROL_MODE, UVC_GET_CUR, (guchar *) & req)) {
    GST_WARNING_OBJECT (self, " RATE_CONTROL GET_CUR error");
    return;
  }

  req.bRateControlMode = self->rate_control;
  if (self->fixed_framerate)
    req.bRateControlMode |= UVC_H264_RATECONTROL_FIXED_FRM_FLG;

  if (!xu_query (self, UVCX_RATE_CONTROL_MODE, UVC_SET_CUR, (guchar *) & req)) {
    GST_WARNING_OBJECT (self, " RATE_CONTROL SET_CUR error");
    return;
  }
}

static void
set_level_idc (GstUvcH264Src * self)
{
  uvcx_video_advance_config_t req;

  if (!xu_query (self, UVCX_VIDEO_ADVANCE_CONFIG, UVC_GET_CUR,
          (guchar *) & req)) {
    GST_WARNING_OBJECT (self, " VIDEO_ADVANCE_CONFIG GET_CUR error");
    return;
  }

  req.blevel_idc = self->level_idc;
  if (!xu_query (self, UVCX_VIDEO_ADVANCE_CONFIG, UVC_SET_CUR,
          (guchar *) & req)) {
    GST_WARNING_OBJECT (self, " VIDEO_ADVANCE_CONFIG SET_CUR error");
    return;
  }
}

static void
set_bitrate (GstUvcH264Src * self)
{
  uvcx_bitrate_layers_t req;

  if (!xu_query (self, UVCX_BITRATE_LAYERS, UVC_GET_CUR, (guchar *) & req)) {
    GST_WARNING_OBJECT (self, " BITRATE_LAYERS GET_CUR error");
    return;
  }

  req.dwPeakBitrate = self->peak_bitrate;
  req.dwAverageBitrate = self->average_bitrate;
  if (!xu_query (self, UVCX_BITRATE_LAYERS, UVC_SET_CUR, (guchar *) & req)) {
    GST_WARNING_OBJECT (self, " BITRATE_LAYERS SET_CUR error");
    return;
  }
}

static void
set_qp (GstUvcH264Src * self, gint type)
{
  uvcx_qp_steps_layers_t req;

  req.wLayerID = 0;
  switch (type) {
    case QP_I_FRAME:
      req.bFrameType = UVC_H264_QP_STEPS_I_FRAME_TYPE;
      break;
    case QP_P_FRAME:
      req.bFrameType = UVC_H264_QP_STEPS_P_FRAME_TYPE;
      break;
    case QP_B_FRAME:
      req.bFrameType = UVC_H264_QP_STEPS_B_FRAME_TYPE;
      break;
    default:
      return;
  }
  req.bMinQp = 0;
  req.bMaxQp = 0;
  if (!xu_query (self, UVCX_QP_STEPS_LAYERS, UVC_SET_CUR, (guchar *) & req)) {
    GST_WARNING_OBJECT (self, " QP_STEPS_LAYERS SET_CUR error");
    return;
  }

  if (!xu_query (self, UVCX_QP_STEPS_LAYERS, UVC_GET_CUR, (guchar *) & req)) {
    GST_WARNING_OBJECT (self, " QP_STEPS_LAYERS GET_CUR error");
    return;
  }

  req.bMinQp = self->min_qp[type];
  req.bMaxQp = self->max_qp[type];
  if (!xu_query (self, UVCX_QP_STEPS_LAYERS, UVC_SET_CUR, (guchar *) & req)) {
    GST_WARNING_OBJECT (self, " QP_STEPS_LAYERS SET_CUR error");
    return;
  }
}

static void
set_ltr (GstUvcH264Src * self)
{
  uvcx_ltr_buffer_size_control_t req;

  if (!xu_query (self, UVCX_LTR_BUFFER_SIZE_CONTROL, UVC_GET_CUR,
          (guchar *) & req)) {
    GST_WARNING_OBJECT (self, " LTR_BUFFER_SIZE GET_CUR error");
    return;
  }

  req.bLTRBufferSize = self->ltr_buffer_size;
  req.bLTREncoderControl = self->ltr_encoder_control;
  if (!xu_query (self, UVCX_LTR_BUFFER_SIZE_CONTROL, UVC_SET_CUR,
          (guchar *) & req)) {
    GST_WARNING_OBJECT (self, "LTR_BUFFER_SIZE  SET_CUR error");
    return;
  }
}

/* Get Dynamic controls */

static void
update_rate_control (GstUvcH264Src * self)
{
  uvcx_rate_control_mode_t req;

  if (!xu_query (self, UVCX_RATE_CONTROL_MODE, UVC_GET_CUR, (guchar *) & req)) {
    GST_WARNING_OBJECT (self, " RATE_CONTROL GET_CUR error");
    return;
  }

  if (self->rate_control != (req.bRateControlMode &
          ~UVC_H264_RATECONTROL_FIXED_FRM_FLG)) {
    self->rate_control = (req.bRateControlMode &
        ~UVC_H264_RATECONTROL_FIXED_FRM_FLG);
    g_object_notify (G_OBJECT (self), "rate-control");
  }
  if (self->fixed_framerate != ((req.bRateControlMode &
              UVC_H264_RATECONTROL_FIXED_FRM_FLG) != 0)) {
    self->fixed_framerate = ((req.bRateControlMode &
            UVC_H264_RATECONTROL_FIXED_FRM_FLG) != 0);
    g_object_notify (G_OBJECT (self), "fixed-framerate");
  }
}


static guint32
update_level_idc_and_get_max_mbps (GstUvcH264Src * self)
{
  uvcx_video_advance_config_t req;

  if (!xu_query (self, UVCX_VIDEO_ADVANCE_CONFIG, UVC_GET_CUR,
          (guchar *) & req)) {
    GST_WARNING_OBJECT (self, " VIDEO_ADVANCE_CONFIG GET_CUR error");
    return 0;
  }

  if (self->level_idc != req.blevel_idc) {
    self->level_idc = req.blevel_idc;
    g_object_notify (G_OBJECT (self), "level-idc");
  }
  return req.dwMb_max;
}

static void
update_bitrate (GstUvcH264Src * self)
{
  uvcx_bitrate_layers_t req;

  if (!xu_query (self, UVCX_BITRATE_LAYERS, UVC_GET_CUR, (guchar *) & req)) {
    GST_WARNING_OBJECT (self, " BITRATE_LAYERS GET_CUR error");
    return;
  }
  if (self->peak_bitrate != req.dwPeakBitrate) {
    self->peak_bitrate = req.dwPeakBitrate;
    g_object_notify (G_OBJECT (self), "peak-bitrate");
  }
  if (self->average_bitrate != req.dwAverageBitrate) {
    self->average_bitrate = req.dwAverageBitrate;
    g_object_notify (G_OBJECT (self), "average-bitrate");
  }
}

static gboolean
update_qp (GstUvcH264Src * self, gint type)
{
  uvcx_qp_steps_layers_t req;
  guint8 frame_type;

  req.wLayerID = 0;
  switch (type) {
    case QP_I_FRAME:
      frame_type = UVC_H264_QP_STEPS_I_FRAME_TYPE;
      break;
    case QP_P_FRAME:
      frame_type = UVC_H264_QP_STEPS_P_FRAME_TYPE;
      break;
    case QP_B_FRAME:
      frame_type = UVC_H264_QP_STEPS_B_FRAME_TYPE;
      break;
    default:
      return FALSE;
  }
  req.bFrameType = frame_type;
  req.bMinQp = 0;
  req.bMaxQp = 0;
  if (!xu_query (self, UVCX_QP_STEPS_LAYERS, UVC_SET_CUR, (guchar *) & req)) {
    GST_WARNING_OBJECT (self, " QP_STEPS_LAYERS SET_CUR error");
    return FALSE;
  }

  if (!xu_query (self, UVCX_QP_STEPS_LAYERS, UVC_GET_CUR, (guchar *) & req)) {
    GST_WARNING_OBJECT (self, " QP_STEPS_LAYERS GET_CUR error");
    return FALSE;
  }

  if (req.bFrameType == frame_type) {
    if (self->min_qp[type] != req.bMinQp) {
      self->min_qp[type] = req.bMinQp;
      switch (type) {
        case QP_I_FRAME:
          g_object_notify (G_OBJECT (self), "min-iframe-qp");
          break;
        case QP_P_FRAME:
          g_object_notify (G_OBJECT (self), "min-pframe-qp");
          break;
        case QP_B_FRAME:
          g_object_notify (G_OBJECT (self), "min-bframe-qp");
          break;
        default:
          break;
      }
    }
    if (self->max_qp[type] != req.bMaxQp) {
      self->max_qp[type] = req.bMaxQp;
      switch (type) {
        case QP_I_FRAME:
          g_object_notify (G_OBJECT (self), "max-iframe-qp");
          break;
        case QP_P_FRAME:
          g_object_notify (G_OBJECT (self), "max-pframe-qp");
          break;
        case QP_B_FRAME:
          g_object_notify (G_OBJECT (self), "max-bframe-qp");
          break;
        default:
          break;
      }
    }
    return TRUE;
  } else {
    self->min_qp[type] = 0xFF;
    self->max_qp[type] = 0xFF;
    return FALSE;
  }
}

static void
update_ltr (GstUvcH264Src * self)
{
  uvcx_ltr_buffer_size_control_t req;

  if (!xu_query (self, UVCX_LTR_BUFFER_SIZE_CONTROL, UVC_GET_CUR,
          (guchar *) & req)) {
    GST_WARNING_OBJECT (self, " LTR_BUFFER_SIZE GET_CUR error");
    return;
  }

  if (self->ltr_buffer_size != req.bLTRBufferSize) {
    self->ltr_buffer_size = req.bLTRBufferSize;
    g_object_notify (G_OBJECT (self), "ltr-buffer-size");
  }
  if (self->ltr_encoder_control != req.bLTREncoderControl) {
    self->ltr_encoder_control = req.bLTREncoderControl;
    g_object_notify (G_OBJECT (self), "ltr-encoder-control");
  }
}

#define STORE_MIN_DEF_MAX(type)                         \
  *(type *)min = *((type *) (min_p + offset));          \
  *(type *)def = *((type *) (def_p + offset));          \
  *(type *)max = *((type *) (max_p + offset));

static gboolean
probe_setting (GstUvcH264Src * self, uvcx_control_selector_t selector,
    guint offset, gint size, gpointer min, gpointer def, gpointer max)
{
  guchar *min_p, *def_p, *max_p;
  gboolean ret = FALSE;
  __u16 len;

  if (!xu_query (self, selector, UVC_GET_LEN, (guchar *) & len)) {
    GST_WARNING_OBJECT (self, "probe_setting GET_LEN error");
    return FALSE;
  }
  min_p = g_malloc0 (len);
  def_p = g_malloc0 (len);
  max_p = g_malloc0 (len);

  if (!xu_query (self, selector, UVC_GET_MIN, min_p)) {
    GST_WARNING_OBJECT (self, "probe_setting GET_MIN error");
    goto end;
  }
  if (!xu_query (self, selector, UVC_GET_DEF, def_p)) {
    GST_WARNING_OBJECT (self, "probe_setting GET_DEF error");
    goto end;
  }
  if (!xu_query (self, selector, UVC_GET_MAX, max_p)) {
    GST_WARNING_OBJECT (self, "probe_setting GET_MAX error");
    goto end;
  }

  switch (size) {
    case -1:
      STORE_MIN_DEF_MAX (gint8);
      ret = TRUE;
      break;
    case 1:
      STORE_MIN_DEF_MAX (guint8);
      ret = TRUE;
      break;
    case -2:
      STORE_MIN_DEF_MAX (gint16);
      ret = TRUE;
      break;
    case 2:
      STORE_MIN_DEF_MAX (guint16);
      ret = TRUE;
      break;
    case -4:
      STORE_MIN_DEF_MAX (gint32);
      ret = TRUE;
      break;
    case 4:
      STORE_MIN_DEF_MAX (guint32);
      ret = TRUE;
      break;
    default:
      break;
  }

end:
  g_free (min_p);
  g_free (def_p);
  g_free (max_p);

  return ret;
}

static gboolean
test_enum_setting (GstUvcH264Src * self, guint offset, guint size,
    guint16 value)
{
  uvcx_video_config_probe_commit_t cur;
  uvcx_video_config_probe_commit_t req;
  guchar *req_p = (guchar *) & req;

  if (!xu_query (self, UVCX_VIDEO_CONFIG_PROBE, UVC_GET_CUR, (guchar *) & cur)) {
    GST_WARNING_OBJECT (self, " GET_CUR error");
    return FALSE;
  }

  req = cur;

  if (size == 1)
    *((guint8 *) (req_p + offset)) = (guint8) value;
  else
    *((guint16 *) (req_p + offset)) = value;

  if (!xu_query (self, UVCX_VIDEO_CONFIG_PROBE, UVC_SET_CUR, req_p)) {
    GST_WARNING_OBJECT (self, " SET_CUR error");
    return FALSE;
  }

  if (!xu_query (self, UVCX_VIDEO_CONFIG_PROBE, UVC_GET_CUR, req_p)) {
    GST_WARNING_OBJECT (self, " GET_CUR error");
    return FALSE;
  }

  if (!xu_query (self, UVCX_VIDEO_CONFIG_PROBE, UVC_SET_CUR, (guchar *) & cur)) {
    GST_WARNING_OBJECT (self, " SET_CUR error");
    return FALSE;
  }

  if (size == 1)
    return *((guint8 *) (req_p + offset)) == (guint8) value;
  else
    return *((guint16 *) (req_p + offset)) == value;
}

static gboolean
gst_uvc_h264_src_get_enum_setting (GstUvcH264Src * self, gchar * property,
    gint * mask, gint * default_value)
{
  guint8 min, def, max;
  guint8 en;
  gboolean ret = FALSE;

  if (g_strcmp0 (property, "slice-mode") == 0) {
    guint16 min16, def16, max16;
    guint16 en16;

    ret = probe_setting (self, UVCX_VIDEO_CONFIG_PROBE,
        offsetof (uvcx_video_config_probe_commit_t, wSliceMode), 2,
        &min16, &def16, &max16);
    if (ret) {
      *default_value = def16;
      *mask = 0;
      for (en16 = min16; en16 <= max16; en16++) {
        if (test_enum_setting (self, offsetof (uvcx_video_config_probe_commit_t,
                    wSliceMode), 2, en16))
          *mask |= (1 << en16);
      }
    }
  } else if (g_strcmp0 (property, "usage-type") == 0) {
    ret = probe_setting (self, UVCX_VIDEO_CONFIG_PROBE,
        offsetof (uvcx_video_config_probe_commit_t, bUsageType), 1,
        &min, &def, &max);
    if (ret) {
      *default_value = def;
      *mask = 0;
      for (en = min; en <= max; en++) {
        if (test_enum_setting (self, offsetof (uvcx_video_config_probe_commit_t,
                    bUsageType), 1, en))
          *mask |= (1 << en);
      }
    }
  } else if (g_strcmp0 (property, "entropy") == 0) {
    ret = probe_setting (self, UVCX_VIDEO_CONFIG_PROBE,
        offsetof (uvcx_video_config_probe_commit_t, bEntropyCABAC), 1,
        &min, &def, &max);
    if (ret) {
      *mask = (1 << min) | (1 << max);
      *default_value = def;
    }
  } else if (g_strcmp0 (property, "rate-control") == 0) {
    ret = probe_setting (self, UVCX_VIDEO_CONFIG_PROBE,
        offsetof (uvcx_video_config_probe_commit_t, bRateControlMode), 1,
        &min, &def, &max);
    if (ret) {
      uvcx_rate_control_mode_t cur;

      *default_value = def;
      *mask = 0;

      if (!xu_query (self, UVCX_RATE_CONTROL_MODE, UVC_GET_CUR,
              (guchar *) & cur)) {
        GST_WARNING_OBJECT (self, " CONTROL_MODE GET_CUR error");
        return FALSE;
      }

      for (en = min; en <= max; en++) {
        uvcx_rate_control_mode_t req = { 0, en };

        if (xu_query (self, UVCX_RATE_CONTROL_MODE, UVC_SET_CUR,
                (guchar *) & req) &&
            xu_query (self, UVCX_RATE_CONTROL_MODE, UVC_GET_CUR,
                (guchar *) & req) && req.bRateControlMode == en)
          *mask |= (1 << en);
      }
      if (!xu_query (self, UVCX_RATE_CONTROL_MODE, UVC_SET_CUR,
              (guchar *) & cur)) {
        GST_WARNING_OBJECT (self, " CONTROL_MODE SET_CUR error");
        return FALSE;
      }
    }
  }

  return ret;
}

static gboolean
gst_uvc_h264_src_get_boolean_setting (GstUvcH264Src * self, gchar * property,
    gboolean * changeable, gboolean * default_value)
{
  guint8 min, def, max;
  gboolean ret = FALSE;

  if (g_strcmp0 (property, "enable-sei") == 0) {
    if ((ret = probe_setting (self, UVCX_VIDEO_CONFIG_PROBE,
                offsetof (uvcx_video_config_probe_commit_t, bTimestamp), 1,
                &min, &def, &max))) {
      *changeable = (min != max);
      *default_value = (def != 0);
    }
  } else if (g_strcmp0 (property, "preview-flipped") == 0) {
    if ((ret = probe_setting (self, UVCX_VIDEO_CONFIG_PROBE,
                offsetof (uvcx_video_config_probe_commit_t, bPreviewFlipped), 1,
                &min, &def, &max))) {
      *changeable = (min != max);
      *default_value = (def != 0);
    }
  } else if (g_strcmp0 (property, "fixed-framerate") == 0) {
    if ((ret = probe_setting (self, UVCX_VIDEO_CONFIG_PROBE,
                offsetof (uvcx_video_config_probe_commit_t, bRateControlMode),
                1, &min, &def, &max))) {
      *changeable = ((max & UVC_H264_RATECONTROL_FIXED_FRM_FLG) != 0);
      *default_value = ((def & UVC_H264_RATECONTROL_FIXED_FRM_FLG) != 0);
    }
  }

  return ret;
}

static gboolean
gst_uvc_h264_src_get_int_setting (GstUvcH264Src * self, gchar * property,
    gint * min, gint * def, gint * max)
{
  guint32 min32, def32, max32;
  guint16 min16, def16, max16;
  guint8 min8, def8, max8;
  gint8 smin8, sdef8, smax8;
  gboolean ret = FALSE;

  GST_DEBUG_OBJECT (self, "Probing int property %s", property);
  if (g_strcmp0 (property, "initial-bitrate") == 0) {
    ret = probe_setting (self, UVCX_VIDEO_CONFIG_PROBE,
        offsetof (uvcx_video_config_probe_commit_t, dwBitRate), 4,
        &min32, &def32, &max32);
    if (ret) {
      *min = min32;
      *def = def32;
      *max = max32;
    }
  } else if (g_strcmp0 (property, "slice-units") == 0) {
    ret = probe_setting (self, UVCX_VIDEO_CONFIG_PROBE,
        offsetof (uvcx_video_config_probe_commit_t, wSliceUnits), 2,
        &min16, &def16, &max16);
    if (ret) {
      *min = min16;
      *def = def16;
      *max = max16;
    }
  } else if (g_strcmp0 (property, "iframe-period") == 0) {
    ret = probe_setting (self, UVCX_VIDEO_CONFIG_PROBE,
        offsetof (uvcx_video_config_probe_commit_t, wIFramePeriod), 2,
        &min16, &def16, &max16);
    if (ret) {
      *min = min16;
      *def = def16;
      *max = max16;
    }
  } else if (g_strcmp0 (property, "num-reorder-frames") == 0) {
    ret = probe_setting (self, UVCX_VIDEO_CONFIG_PROBE,
        offsetof (uvcx_video_config_probe_commit_t, bNumOfReorderFrames), 1,
        &min8, &def8, &max8);
    if (ret) {
      *min = min8;
      *def = def8;
      *max = max8;
    }
  } else if (g_strcmp0 (property, "leaky-bucket-size") == 0) {
    ret = probe_setting (self, UVCX_VIDEO_CONFIG_PROBE,
        offsetof (uvcx_video_config_probe_commit_t, wLeakyBucketSize), 2,
        &min16, &def16, &max16);
    if (ret) {
      *min = min16;
      *def = def16;
      *max = max16;
    }
  } else if (g_strcmp0 (property, "level-idc") == 0) {
    ret = probe_setting (self, UVCX_VIDEO_ADVANCE_CONFIG,
        offsetof (uvcx_video_advance_config_t, blevel_idc), 1,
        &min8, &def8, &max8);
    if (ret) {
      *min = min8;
      *def = def8;
      *max = max8;
    }
  } else if (g_strcmp0 (property, "max-mbps") == 0) {
    ret = probe_setting (self, UVCX_VIDEO_ADVANCE_CONFIG,
        offsetof (uvcx_video_advance_config_t, dwMb_max), 4,
        &min32, &def32, &max32);
    if (ret) {
      *min = min32;
      *def = def32;
      *max = max32;
    }
  } else if (g_strcmp0 (property, "peak-bitrate") == 0) {
    ret = probe_setting (self, UVCX_BITRATE_LAYERS,
        offsetof (uvcx_bitrate_layers_t, dwPeakBitrate), 4,
        &min32, &def32, &max32);
    if (ret) {
      *min = min32;
      *def = def32;
      *max = max32;
    }
  } else if (g_strcmp0 (property, "average-bitrate") == 0) {
    ret = probe_setting (self, UVCX_BITRATE_LAYERS,
        offsetof (uvcx_bitrate_layers_t, dwAverageBitrate), 4,
        &min32, &def32, &max32);
    if (ret) {
      *min = min32;
      *def = def32;
      *max = max32;
    }
  } else if (g_strcmp0 (property, "min-iframe-qp") == 0) {
    if (update_qp (self, QP_I_FRAME))
      ret = probe_setting (self, UVCX_QP_STEPS_LAYERS,
          offsetof (uvcx_qp_steps_layers_t, bMinQp), 1, &smin8, &sdef8, &smax8);
    if (ret) {
      *min = smin8;
      *def = sdef8;
      *max = smax8;
    }
  } else if (g_strcmp0 (property, "max-iframe-qp") == 0) {
    if (update_qp (self, QP_I_FRAME))
      ret = probe_setting (self, UVCX_QP_STEPS_LAYERS,
          offsetof (uvcx_qp_steps_layers_t, bMaxQp), 1, &smin8, &sdef8, &smax8);
    if (ret) {
      *min = smin8;
      *def = sdef8;
      *max = smax8;
    }
  } else if (g_strcmp0 (property, "min-pframe-qp") == 0) {
    if (update_qp (self, QP_P_FRAME))
      ret = probe_setting (self, UVCX_QP_STEPS_LAYERS,
          offsetof (uvcx_qp_steps_layers_t, bMinQp), 1, &smin8, &sdef8, &smax8);
    if (ret) {
      *min = smin8;
      *def = sdef8;
      *max = smax8;
    }
  } else if (g_strcmp0 (property, "max-pframe-qp") == 0) {
    if (update_qp (self, QP_P_FRAME))
      ret = probe_setting (self, UVCX_QP_STEPS_LAYERS,
          offsetof (uvcx_qp_steps_layers_t, bMaxQp), 1, &smin8, &sdef8, &smax8);
    if (ret) {
      *min = smin8;
      *def = sdef8;
      *max = smax8;
    }
  } else if (g_strcmp0 (property, "min-bframe-qp") == 0) {
    if (update_qp (self, QP_B_FRAME))
      ret = probe_setting (self, UVCX_QP_STEPS_LAYERS,
          offsetof (uvcx_qp_steps_layers_t, bMinQp), 1, &smin8, &sdef8, &smax8);
    if (ret) {
      *min = smin8;
      *def = sdef8;
      *max = smax8;
    }
  } else if (g_strcmp0 (property, "max-bframe-qp") == 0) {
    if (update_qp (self, QP_B_FRAME))
      ret = probe_setting (self, UVCX_QP_STEPS_LAYERS,
          offsetof (uvcx_qp_steps_layers_t, bMaxQp), 1, &smin8, &sdef8, &smax8);
    if (ret) {
      *min = smin8;
      *def = sdef8;
      *max = smax8;
    }
  } else if (g_strcmp0 (property, "ltr-buffer-size") == 0) {
    ret = probe_setting (self, UVCX_LTR_BUFFER_SIZE_CONTROL,
        offsetof (uvcx_ltr_buffer_size_control_t, bLTRBufferSize), 1,
        &min8, &def8, &max8);
    if (ret) {
      *min = min8;
      *def = def8;
      *max = max8;
    }
  } else if (g_strcmp0 (property, "ltr-encoder-control") == 0) {
    ret = probe_setting (self, UVCX_LTR_BUFFER_SIZE_CONTROL,
        offsetof (uvcx_ltr_buffer_size_control_t, bLTREncoderControl), 1,
        &min8, &def8, &max8);
    if (ret) {
      *min = min8;
      *def = def8;
      *max = max8;
    }
  } else {
    g_return_val_if_reached (FALSE);
  }

  return ret;
}

static GstPadProbeReturn
gst_uvc_h264_src_event_probe (GstPad * pad, GstPadProbeInfo * info,
    gpointer user_data)
{
  GstUvcH264Src *self = GST_UVC_H264_SRC (user_data);
  GstPadProbeReturn ret = GST_PAD_PROBE_OK;
  GstEvent *event = info->data;

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_SEGMENT:
      if (pad == self->vidsrc) {
        const GstSegment *s;

        gst_event_parse_segment (event, &s);
        gst_segment_copy_into (s, &self->segment);
      }
      break;
    case GST_EVENT_EOS:
      ret = self->reconfiguring ? GST_PAD_PROBE_DROP : GST_PAD_PROBE_OK;
      break;
    default:
      break;
  }

  return ret;
}

static GstPadProbeReturn
gst_uvc_h264_src_buffer_probe (GstPad * pad, GstPadProbeInfo * info,
    gpointer user_data)
{
  GstUvcH264Src *self = GST_UVC_H264_SRC (user_data);
  GstBuffer *buffer = info->data;

  /* TODO: Check the NALU type and make sure it is a keyframe */
  if (self->key_unit_event) {
    GstClockTime ts, running_time, stream_time;
    gboolean all_headers;
    guint count;
    GstEvent *downstream;

    if (gst_video_event_parse_upstream_force_key_unit (self->key_unit_event,
            &ts, &all_headers, &count)) {
      if (!GST_CLOCK_TIME_IS_VALID (ts)) {
        ts = GST_BUFFER_TIMESTAMP (buffer);
      }
      running_time = gst_segment_to_running_time (&self->segment,
          GST_FORMAT_TIME, ts);

      stream_time = gst_segment_to_stream_time (&self->segment,
          GST_FORMAT_TIME, ts);

      GST_DEBUG_OBJECT (self, "Sending downstream force-key-unit : %d - %d ts=%"
          GST_TIME_FORMAT " running time =%" GST_TIME_FORMAT " stream=%"
          GST_TIME_FORMAT, all_headers, count, GST_TIME_ARGS (ts),
          GST_TIME_ARGS (running_time), GST_TIME_ARGS (stream_time));
      downstream = gst_video_event_new_downstream_force_key_unit (ts,
          stream_time, running_time, all_headers, count);
      gst_pad_push_event (self->vidsrc, downstream);
      gst_event_replace (&self->key_unit_event, NULL);
    }
  }
  return TRUE;
}

static gboolean
gst_uvc_h264_src_parse_event (GstUvcH264Src * self, GstPad * pad,
    GstEvent * event)
{
  const GstStructure *s = gst_event_get_structure (event);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CUSTOM_UPSTREAM:
      if (pad == self->vidsrc && self->main_format == UVC_H264_SRC_FORMAT_H264) {
        if (gst_video_event_is_force_key_unit (event)) {
          uvcx_picture_type_control_t req = { 0, 0 };
          GstClockTime ts;
          gboolean all_headers;

          if (gst_video_event_parse_upstream_force_key_unit (event,
                  &ts, &all_headers, NULL)) {
            GST_INFO_OBJECT (self, "Received upstream force-key-unit : %d %"
                GST_TIME_FORMAT, all_headers, GST_TIME_ARGS (ts));
            /* TODO: wait until 'ts' time is reached */
            if (all_headers)
              req.wPicType = UVC_H264_PICTYPE_IDR_WITH_PPS_SPS;
            else
              req.wPicType = UVC_H264_PICTYPE_IDR;

            if (!xu_query (self, UVCX_PICTURE_TYPE_CONTROL, UVC_SET_CUR,
                    (guchar *) & req)) {
              GST_WARNING_OBJECT (self, " PICTURE_TYPE_CONTROL SET_CUR error");
            } else {
              gst_event_replace (&self->key_unit_event, event);
              gst_event_unref (event);

              return TRUE;
            }
          }
        } else if (s &&
            gst_structure_has_name (s, "uvc-h264-ltr-picture-control")) {
          guint put_at, encode_using;

          if (gst_structure_get_uint (s, "put-at", &put_at) &&
              gst_structure_get_uint (s, "encode-using", &encode_using)) {
            uvcx_ltr_picture_control req = { 0, put_at, encode_using };

            if (!xu_query (self, UVCX_LTR_PICTURE_CONTROL, UVC_SET_CUR,
                    (guchar *) & req)) {
              GST_WARNING_OBJECT (self, " LTR PICTURE_CONTROL SET_CUR error");
            } else {
              gst_event_unref (event);

              return TRUE;
            }
          }
          return TRUE;
        } else if (s && gst_structure_has_name (s, "uvc-h264-bitrate-control")) {
          guint average, peak;

          if (gst_structure_get_uint (s, "average-bitrate", &average) &&
              gst_structure_get_uint (s, "peak-bitrate", &peak)) {
            self->average_bitrate = average;
            self->peak_bitrate = peak;
            set_bitrate (self);
            update_bitrate (self);

            gst_event_unref (event);

            return TRUE;
          }
        } else if (s && gst_structure_has_name (s, "uvc-h264-qp-control")) {
          gint min_qp, max_qp;
          gboolean valid_event = FALSE;

          if (gst_structure_get_int (s, "min-iframe-qp", &min_qp) &&
              gst_structure_get_int (s, "max-iframe-qp", &max_qp)) {
            self->min_qp[QP_I_FRAME] = min_qp;
            self->max_qp[QP_I_FRAME] = max_qp;
            set_qp (self, QP_I_FRAME);
            update_qp (self, QP_I_FRAME);
            valid_event = TRUE;
          }
          if (gst_structure_get_int (s, "min-pframe-qp", &min_qp) &&
              gst_structure_get_int (s, "max-pframe-qp", &max_qp)) {
            self->min_qp[QP_P_FRAME] = min_qp;
            self->max_qp[QP_P_FRAME] = max_qp;
            set_qp (self, QP_P_FRAME);
            update_qp (self, QP_P_FRAME);
            valid_event = TRUE;
          }
          if (gst_structure_get_int (s, "min-bframe-qp", &min_qp) &&
              gst_structure_get_int (s, "max-bframe-qp", &max_qp)) {
            self->min_qp[QP_B_FRAME] = min_qp;
            self->max_qp[QP_B_FRAME] = max_qp;
            set_qp (self, QP_B_FRAME);
            update_qp (self, QP_B_FRAME);
            valid_event = TRUE;
          }

          if (valid_event) {
            gst_event_unref (event);

            return TRUE;
          }
        } else if (s && gst_structure_has_name (s, "uvc-h264-rate-control")) {
          UvcH264RateControl rate;
          gboolean fixed_framerate;

          if (gst_structure_get_enum (s, "rate-control",
                  UVC_H264_RATECONTROL_TYPE, (gint *) & rate) &&
              gst_structure_get_boolean (s, "fixed-framerate",
                  &fixed_framerate)) {
            self->rate_control = rate;
            self->fixed_framerate = fixed_framerate;
            set_rate_control (self);
            update_rate_control (self);

            gst_event_unref (event);

            return TRUE;
          }
        } else if (s && gst_structure_has_name (s, "uvc-h264-level-idc")) {
          guint level_idc;

          if (gst_structure_get_uint (s, "level-idc", &level_idc)) {
            self->level_idc = level_idc;
            set_level_idc (self);
            update_level_idc_and_get_max_mbps (self);

            gst_event_unref (event);
          }
        }
      }
      break;
    default:
      break;
  }

  return FALSE;
}

static gboolean
gst_uvc_h264_src_send_event (GstElement * element, GstEvent * event)
{
  GstUvcH264Src *self = GST_UVC_H264_SRC (element);

  if (gst_uvc_h264_src_parse_event (self, self->vidsrc, event))
    return TRUE;

  return GST_ELEMENT_CLASS (parent_class)->send_event (element, event);
}

static gboolean
gst_uvc_h264_src_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstUvcH264Src *self = GST_UVC_H264_SRC (GST_PAD_PARENT (pad));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_SEGMENT:
      if (pad == self->vidsrc) {
        const GstSegment *s;

        gst_event_parse_segment (event, &s);
        gst_segment_copy_into (s, &self->segment);
      }
      break;
    case GST_EVENT_FLUSH_STOP:
      if (pad == self->vidsrc)
        gst_segment_init (&self->segment, GST_FORMAT_UNDEFINED);
      break;
    default:
      if (gst_uvc_h264_src_parse_event (self, pad, event))
        return TRUE;
      break;
  }
  return self->srcpad_event_func (pad, parent, event);
}

static gboolean
xu_query (GstUvcH264Src * self, guint selector, guint query, guchar * data)
{
  struct uvc_xu_control_query xu;
  __u16 len;

  if (self->v4l2_fd == -1) {
    GST_WARNING_OBJECT (self, "Can't query XU with fd = -1");
    return FALSE;
  }

  xu.unit = self->h264_unit_id;
  xu.selector = selector;

  xu.query = UVC_GET_LEN;
  xu.size = sizeof (len);
  xu.data = (unsigned char *) &len;
  if (-1 == ioctl (self->v4l2_fd, UVCIOC_CTRL_QUERY, &xu)) {
    GST_WARNING_OBJECT (self, "PROBE GET_LEN error");
    return FALSE;
  }

  if (query == UVC_GET_LEN) {
    *((__u16 *) data) = len;
  } else {
    xu.query = query;
    xu.size = len;
    xu.data = data;
    if (-1 == ioctl (self->v4l2_fd, UVCIOC_CTRL_QUERY, &xu)) {
      return FALSE;
    }
  }

  return TRUE;
}

static void
fill_probe_commit (GstUvcH264Src * self,
    uvcx_video_config_probe_commit_t * probe, guint32 frame_interval,
    guint32 width, guint32 height, guint32 profile,
    UvcH264StreamFormat stream_format)
{
  probe->dwFrameInterval = frame_interval;
  probe->dwBitRate = self->initial_bitrate;
  probe->wWidth = width;
  probe->wHeight = height;
  probe->wSliceUnits = self->slice_units;
  probe->wSliceMode = self->slice_mode;
  probe->wProfile = profile;
  probe->wIFramePeriod = self->iframe_period;
  probe->bUsageType = self->usage_type;
  probe->bRateControlMode = self->rate_control;
  if (self->fixed_framerate)
    probe->bRateControlMode |= UVC_H264_RATECONTROL_FIXED_FRM_FLG;
  probe->bStreamFormat = stream_format;
  probe->bEntropyCABAC = self->entropy;
  probe->bTimestamp = self->enable_sei ?
      UVC_H264_TIMESTAMP_SEI_ENABLE : UVC_H264_TIMESTAMP_SEI_DISABLE;
  probe->bNumOfReorderFrames = self->num_reorder_frames;
  probe->bPreviewFlipped = self->preview_flipped ?
      UVC_H264_PREFLIPPED_HORIZONTAL : UVC_H264_PREFLIPPED_DISABLE;
  probe->wLeakyBucketSize = self->leaky_bucket_size;
}

static void
print_probe_commit (GstUvcH264Src * self,
    uvcx_video_config_probe_commit_t * probe)
{
  GST_DEBUG_OBJECT (self, "  Frame interval : %d *100ns",
      probe->dwFrameInterval);
  GST_DEBUG_OBJECT (self, "  Bit rate : %d", probe->dwBitRate);
  GST_DEBUG_OBJECT (self, "  Hints : %X", probe->bmHints);
  GST_DEBUG_OBJECT (self, "  Configuration index : %d",
      probe->wConfigurationIndex);
  GST_DEBUG_OBJECT (self, "  Width : %d", probe->wWidth);
  GST_DEBUG_OBJECT (self, "  Height : %d", probe->wHeight);
  GST_DEBUG_OBJECT (self, "  Slice units : %d", probe->wSliceUnits);
  GST_DEBUG_OBJECT (self, "  Slice mode : %X", probe->wSliceMode);
  GST_DEBUG_OBJECT (self, "  Profile : %X", probe->wProfile);
  GST_DEBUG_OBJECT (self, "  IFrame Period : %d ms", probe->wIFramePeriod);
  GST_DEBUG_OBJECT (self, "  Estimated video delay : %d ms",
      probe->wEstimatedVideoDelay);
  GST_DEBUG_OBJECT (self, "  Estimated max config delay : %d ms",
      probe->wEstimatedMaxConfigDelay);
  GST_DEBUG_OBJECT (self, "  Usage type : %X", probe->bUsageType);
  GST_DEBUG_OBJECT (self, "  Rate control mode : %X", probe->bRateControlMode);
  GST_DEBUG_OBJECT (self, "  Temporal scale mode : %X",
      probe->bTemporalScaleMode);
  GST_DEBUG_OBJECT (self, "  Spatial scale mode : %X",
      probe->bSpatialScaleMode);
  GST_DEBUG_OBJECT (self, "  SNR scale mode : %X", probe->bSNRScaleMode);
  GST_DEBUG_OBJECT (self, "  Stream mux option : %X", probe->bStreamMuxOption);
  GST_DEBUG_OBJECT (self, "  Stream Format : %X", probe->bStreamFormat);
  GST_DEBUG_OBJECT (self, "  Entropy CABAC : %X", probe->bEntropyCABAC);
  GST_DEBUG_OBJECT (self, "  Timestamp : %X", probe->bTimestamp);
  GST_DEBUG_OBJECT (self, "  Num of reorder frames : %d",
      probe->bNumOfReorderFrames);
  GST_DEBUG_OBJECT (self, "  Preview flipped : %X", probe->bPreviewFlipped);
  GST_DEBUG_OBJECT (self, "  View : %d", probe->bView);
  GST_DEBUG_OBJECT (self, "  Stream ID : %X", probe->bStreamID);
  GST_DEBUG_OBJECT (self, "  Spatial layer ratio : %f",
      ((probe->bSpatialLayerRatio & 0xF0) >> 4) +
      ((float) (probe->bSpatialLayerRatio & 0x0F)) / 16);
  GST_DEBUG_OBJECT (self, "  Leaky bucket size : %d ms",
      probe->wLeakyBucketSize);
}

static void
configure_h264 (GstUvcH264Src * self, gint fd)
{
  uvcx_video_config_probe_commit_t probe;

  /* Set the secondary format first, so the last SET_CUR will be for the
   * H264 format. This way, we can still get the static control values with
   * a GET_CUR. Otherwise all static properties will return 0 because that's
   * what the GET_CUR of the raw format returns.
   */
  if (self->secondary_format == UVC_H264_SRC_FORMAT_RAW) {
    memset (&probe, 0, sizeof (probe));
    probe.dwFrameInterval = self->secondary_frame_interval;
    probe.wWidth = self->secondary_width;
    probe.wHeight = self->secondary_height;
    probe.bStreamMuxOption = 5;

    GST_DEBUG_OBJECT (self, "RAW PROBE SET_CUR : ");
    print_probe_commit (self, &probe);

    if (!xu_query (self, UVCX_VIDEO_CONFIG_PROBE, UVC_SET_CUR,
            (guchar *) & probe)) {
      GST_WARNING_OBJECT (self, "PROBE SET_CUR error");
      return;
    }

    if (!xu_query (self, UVCX_VIDEO_CONFIG_PROBE, UVC_GET_CUR,
            (guchar *) & probe)) {
      GST_WARNING_OBJECT (self, "PROBE GET_CUR error");
      return;
    }
    GST_DEBUG_OBJECT (self, "RAW PROBE GET_CUR : ");
    print_probe_commit (self, &probe);

    if (!xu_query (self, UVCX_VIDEO_CONFIG_COMMIT, UVC_SET_CUR,
            (guchar *) & probe)) {
      GST_WARNING_OBJECT (self, "COMMIT SET_CUR error");
      return;
    }
  }
  /* Print MIN/MAX/DEF probe values for debugging purposes */
  if (!xu_query (self, UVCX_VIDEO_CONFIG_PROBE, UVC_GET_MIN,
          (guchar *) & probe)) {
    GST_WARNING_OBJECT (self, "PROBE GET_CUR error");
    return;
  }
  GST_DEBUG_OBJECT (self, "PROBE GET_MIN : ");
  print_probe_commit (self, &probe);

  if (!xu_query (self, UVCX_VIDEO_CONFIG_PROBE, UVC_GET_MAX,
          (guchar *) & probe)) {
    GST_WARNING_OBJECT (self, "PROBE GET_CUR error");
    return;
  }
  GST_DEBUG_OBJECT (self, "PROBE GET_MAX : ");
  print_probe_commit (self, &probe);

  if (!xu_query (self, UVCX_VIDEO_CONFIG_PROBE, UVC_GET_DEF,
          (guchar *) & probe)) {
    GST_WARNING_OBJECT (self, "PROBE GET_CUR error");
    return;
  }
  GST_DEBUG_OBJECT (self, "PROBE GET_DEF : ");
  print_probe_commit (self, &probe);

  fill_probe_commit (self, &probe, self->main_frame_interval,
      self->main_width, self->main_height, self->main_profile,
      self->main_stream_format);
  if (self->secondary_format != UVC_H264_SRC_FORMAT_NONE)
    probe.bStreamMuxOption = 3;
  else
    probe.bStreamMuxOption = 0;
  probe.bmHints = UVC_H264_BMHINTS_RESOLUTION | UVC_H264_BMHINTS_PROFILE |
      UVC_H264_BMHINTS_FRAME_INTERVAL;

  GST_DEBUG_OBJECT (self, "PROBE SET_CUR : ");
  print_probe_commit (self, &probe);

  if (!xu_query (self, UVCX_VIDEO_CONFIG_PROBE, UVC_SET_CUR,
          (guchar *) & probe)) {
    GST_WARNING_OBJECT (self, "PROBE SET_CUR error");
    return;
  }

  if (!xu_query (self, UVCX_VIDEO_CONFIG_PROBE, UVC_GET_CUR,
          (guchar *) & probe)) {
    GST_WARNING_OBJECT (self, "PROBE GET_CUR error");
    return;
  }
  GST_DEBUG_OBJECT (self, "PROBE GET_CUR : ");
  print_probe_commit (self, &probe);

  /* Must validate the settings accepted by the encoder */
  if (!xu_query (self, UVCX_VIDEO_CONFIG_COMMIT, UVC_SET_CUR,
          (guchar *) & probe)) {
    GST_WARNING_OBJECT (self, "COMMIT SET_CUR error");
    return;
  }
}

static void
v4l2src_prepare_format (GstElement * v4l2src, gint fd, GstCaps * caps,
    gpointer user_data)
{
  GstUvcH264Src *self = GST_UVC_H264_SRC (user_data);

  if (self->main_format == UVC_H264_SRC_FORMAT_H264) {
    /* TODO: update static controls and g_object_notify those that changed */
    configure_h264 (self, fd);

    /* TODO: update dynamic controls on READY state */
    /* Configure dynamic controls */
    set_rate_control (self);
    update_rate_control (self);
    set_level_idc (self);
    update_level_idc_and_get_max_mbps (self);
    set_bitrate (self);
    update_bitrate (self);
    set_qp (self, QP_I_FRAME);
    update_qp (self, QP_I_FRAME);
    set_qp (self, QP_P_FRAME);
    update_qp (self, QP_P_FRAME);
    set_qp (self, QP_B_FRAME);
    update_qp (self, QP_B_FRAME);
    set_ltr (self);
    update_ltr (self);
  }
}

static gboolean
_extract_caps_info (GstStructure * structure, guint16 * width, guint16 * height,
    guint32 * frame_interval)
{
  gint w, h, fps_n, fps_d;
  gboolean ret = TRUE;

  ret &= gst_structure_get_int (structure, "width", &w);
  ret &= gst_structure_get_int (structure, "height", &h);
  ret &= gst_structure_get_fraction (structure, "framerate", &fps_n, &fps_d);

  if (ret) {
    *width = w;
    *height = h;
    /* Interval is in 100ns */
    *frame_interval = GST_TIME_AS_NSECONDS ((fps_d * GST_SECOND) / fps_n) / 100;
  }

  return ret;
}

static guint16
_extract_profile (GstStructure * structure)
{
  const gchar *profile_str;
  guint16 profile;

  profile = UVC_H264_PROFILE_HIGH;
  profile_str = gst_structure_get_string (structure, "profile");
  if (profile_str) {
    if (!strcmp (profile_str, "constrained-baseline")) {
      profile = UVC_H264_PROFILE_CONSTRAINED_BASELINE;
    } else if (!strcmp (profile_str, "baseline")) {
      profile = UVC_H264_PROFILE_BASELINE;
    } else if (!strcmp (profile_str, "main")) {
      profile = UVC_H264_PROFILE_MAIN;
    } else if (!strcmp (profile_str, "high")) {
      profile = UVC_H264_PROFILE_HIGH;
    }
  }
  return profile;
}

static UvcH264StreamFormat
_extract_stream_format (GstStructure * structure)
{
  const gchar *stream_format;

  stream_format = gst_structure_get_string (structure, "stream-format");
  if (stream_format) {
    if (!strcmp (stream_format, "avc"))
      return UVC_H264_STREAMFORMAT_NAL;
    else if (!strcmp (stream_format, "byte-stream"))
      return UVC_H264_STREAMFORMAT_ANNEXB;
  }
  return UVC_H264_STREAMFORMAT_ANNEXB;
}

static GstCaps *
_transform_caps (GstUvcH264Src * self, GstCaps * caps, const gchar * name)
{
  GstElement *el = gst_element_factory_make (name, NULL);
  GstElement *cf = gst_element_factory_make ("capsfilter", NULL);
  GstElement *fs = gst_element_factory_make ("fakesink", NULL);
  GstPad *sink;
  GstCaps *out_caps = NULL;

  if (!el || !cf || !fs) {
    if (el)
      gst_object_unref (el);
    if (cf)
      gst_object_unref (cf);
    if (fs)
      gst_object_unref (fs);
    goto done;
  }

  gst_element_set_locked_state (el, TRUE);
  gst_element_set_locked_state (cf, TRUE);
  gst_element_set_locked_state (fs, TRUE);

  if (!gst_bin_add (GST_BIN (self), el)) {
    gst_object_unref (el);
    gst_object_unref (cf);
    gst_object_unref (fs);
    goto done;
  }
  if (!gst_bin_add (GST_BIN (self), cf)) {
    gst_object_unref (cf);
    gst_object_unref (fs);
    gst_bin_remove (GST_BIN (self), el);
    goto done;
  }
  if (!gst_bin_add (GST_BIN (self), fs)) {
    gst_object_unref (fs);
    gst_bin_remove (GST_BIN (self), el);
    gst_bin_remove (GST_BIN (self), cf);
    goto done;
  }

  g_object_set (cf, "caps", caps, NULL);

  if (!gst_element_link (cf, fs))
    goto error_remove;
  if (!gst_element_link (el, cf))
    goto error_remove;

  sink = gst_element_get_static_pad (el, "sink");
  if (!sink)
    goto error_remove;
  GST_DEBUG_OBJECT (self, "Transforming: %" GST_PTR_FORMAT, caps);

  caps = gst_pad_query_caps (sink, NULL);
  gst_object_unref (sink);

  GST_DEBUG_OBJECT (self, "Result: %" GST_PTR_FORMAT, out_caps);

error_remove:
  gst_bin_remove (GST_BIN (self), cf);
  gst_bin_remove (GST_BIN (self), el);
  gst_bin_remove (GST_BIN (self), fs);

done:
  if (out_caps == NULL)
    out_caps = gst_caps_copy (caps);

  return out_caps;
}

static GstCaps *
gst_uvc_h264_src_transform_caps (GstUvcH264Src * self, GstCaps * caps)
{
  GstCaps *h264 = gst_caps_new_empty_simple ("video/x-h264");
  GstCaps *jpg = gst_caps_new_empty_simple ("image/jpeg");
  GstCaps *h264_caps = gst_caps_intersect (h264, caps);
  GstCaps *jpg_caps = gst_caps_intersect (jpg, caps);

  /* TODO: Keep caps order after transformation */
  caps = _transform_caps (self, caps, self->colorspace_name);
  caps = gst_caps_make_writable (caps);

  if (!gst_caps_is_empty (h264_caps)) {
    gst_caps_append (caps, h264_caps);
  } else {
    gst_caps_unref (h264_caps);
  }

  if (!gst_caps_is_empty (jpg_caps)) {
    gst_caps_append (caps, jpg_caps);
  } else {
    gst_caps_unref (jpg_caps);
  }

  gst_caps_unref (h264);
  gst_caps_unref (jpg);

  return caps;
}

static GstCaps *
gst_uvc_h264_src_fixate_caps (GstUvcH264Src * self, GstPad * v4l_pad,
    GstCaps * v4l_caps, GstCaps * peer_caps, gboolean primary)
{
  GstCaps *caps = NULL;
  GstCaps *icaps = NULL;
  GstCaps *tcaps = NULL;
  int i;

  if (v4l_caps == NULL || gst_caps_is_any (v4l_caps)) {
    GST_DEBUG_OBJECT (self, "v4l caps are invalid. not fixating");
    return NULL;
  }

  tcaps = gst_caps_intersect_full (peer_caps, v4l_caps,
      GST_CAPS_INTERSECT_FIRST);
  GST_DEBUG_OBJECT (self, "intersect: %" GST_PTR_FORMAT, tcaps);
  icaps = gst_caps_normalize (tcaps);

  /* Prefer the first caps we are compatible with that the peer proposed */
  for (i = 0; i < gst_caps_get_size (icaps); i++) {
    /* get intersection */
    GstCaps *ipcaps = gst_caps_copy_nth (icaps, i);
    GstStructure *s = gst_caps_get_structure (ipcaps, 0);

    GST_DEBUG_OBJECT (self, "Testing %s: %" GST_PTR_FORMAT,
        primary ? "primary" : "secondary", ipcaps);
    if (primary && gst_structure_has_name (s, "video/x-h264")) {
      uvcx_video_config_probe_commit_t probe;
      guint16 width;
      guint16 height;
      guint32 interval;
      guint16 profile;
      UvcH264StreamFormat stream_format;

      if (_extract_caps_info (s, &width, &height, &interval)) {
        profile = _extract_profile (s);
        stream_format = _extract_stream_format (s);
        fill_probe_commit (self, &probe, interval, width, height,
            profile, stream_format);
        probe.bmHints = UVC_H264_BMHINTS_RESOLUTION |
            UVC_H264_BMHINTS_PROFILE | UVC_H264_BMHINTS_FRAME_INTERVAL;

        if (!xu_query (self, UVCX_VIDEO_CONFIG_PROBE, UVC_SET_CUR,
                (guchar *) & probe)) {
          GST_WARNING_OBJECT (self, "PROBE SET_CUR error");
          return NULL;
        }

        if (!xu_query (self, UVCX_VIDEO_CONFIG_PROBE, UVC_GET_CUR,
                (guchar *) & probe)) {
          GST_WARNING_OBJECT (self, "PROBE GET_CUR error");
          return NULL;
        }
        GST_DEBUG_OBJECT (self, "Probe gives us %d==%d, %d==%d, %d==%d",
            probe.wWidth, width, probe.wHeight, height,
            probe.bStreamFormat, stream_format);
        if (probe.wWidth == width && probe.wHeight == height &&
            probe.bStreamFormat == stream_format) {
          caps = ipcaps;
          break;
        }
      }
    } else if (!primary && self->main_format == UVC_H264_SRC_FORMAT_H264) {
      uvcx_video_config_probe_commit_t probe;
      guint16 width;
      guint16 height;
      guint32 interval;

      if (_extract_caps_info (s, &width, &height, &interval)) {
        if (gst_structure_has_name (s, "video/x-raw")) {
          guint8 mux = 0;
          const gchar *format = gst_structure_get_string (s, "format");

          if ((format = gst_structure_get_string (s, "format"))) {
            if (g_strcmp0 (format, "YUY2") == 0)
              mux = 4;
            else if (g_strcmp0 (format, "NV12") == 0)
              mux = 8;
          }
          if (mux != 0) {
            memset (&probe, 0, sizeof (probe));
            probe.dwFrameInterval = interval;
            probe.wWidth = width;
            probe.wHeight = height;
            probe.bStreamMuxOption = mux | 1;
            probe.bmHints = UVC_H264_BMHINTS_RESOLUTION |
                UVC_H264_BMHINTS_PROFILE | UVC_H264_BMHINTS_FRAME_INTERVAL;

            if (!xu_query (self, UVCX_VIDEO_CONFIG_PROBE, UVC_SET_CUR,
                    (guchar *) & probe)) {
              GST_WARNING_OBJECT (self, "PROBE SET_CUR error");
              return NULL;
            }

            if (!xu_query (self, UVCX_VIDEO_CONFIG_PROBE, UVC_GET_CUR,
                    (guchar *) & probe)) {
              GST_WARNING_OBJECT (self, "PROBE GET_CUR error");
              return NULL;
            }
            GST_DEBUG_OBJECT (self, "Probe gives us %d==%d, %d==%d, %d~=%d",
                probe.wWidth, width, probe.wHeight, height,
                probe.bStreamMuxOption, mux);
            if (probe.wWidth == width && probe.wHeight == height &&
                (probe.bStreamMuxOption & mux) != 0) {
              caps = ipcaps;
              break;
            }
          }
        } else if (gst_structure_has_name (s, "image/jpeg")) {
          /* HACK ALERT:  No way of figuring this one out but it seems the
           * camera doesn't allow for h264 muxing and jpeg resolution higher
           * than 640x480 so we shouldn't allow it */
          if (width <= 640 && height <= 480) {
            caps = ipcaps;
            break;
          }
        }
      }
    } else {
      caps = ipcaps;
      break;
    }
    gst_caps_unref (ipcaps);
  }

  if (caps) {
    caps = gst_caps_make_writable (caps);

    /* now fixate */
    if (!gst_caps_is_empty (caps)) {
      caps = gst_caps_fixate (caps);
      GST_DEBUG_OBJECT (self, "fixated to: %" GST_PTR_FORMAT, caps);
    }

    if (gst_caps_is_empty (caps) || gst_caps_is_any (caps)) {
      gst_caps_unref (caps);
      caps = NULL;
    }
  }

  return caps;
}

static void
gst_uvc_h264_src_destroy_pipeline (GstUvcH264Src * self, gboolean v4l2src)
{
  GstIterator *iter = NULL;
  gboolean done;

  if (v4l2src && self->v4l2_src) {
    gst_bin_remove (GST_BIN (self), self->v4l2_src);
    gst_element_set_state (self->v4l2_src, GST_STATE_NULL);
    gst_object_unref (self->v4l2_src);
    self->v4l2_src = NULL;
    self->v4l2_fd = -1;
    self->h264_unit_id = 0;
  }
  if (self->mjpg_demux) {
    gst_bin_remove (GST_BIN (self), self->mjpg_demux);
    gst_element_set_state (self->mjpg_demux, GST_STATE_NULL);
    gst_object_unref (self->mjpg_demux);
    self->mjpg_demux = NULL;
  }
  if (self->jpeg_dec) {
    gst_bin_remove (GST_BIN (self), self->jpeg_dec);
    gst_element_set_state (self->jpeg_dec, GST_STATE_NULL);
    gst_object_unref (self->jpeg_dec);
    self->jpeg_dec = NULL;
  }
  if (self->vid_colorspace) {
    gst_bin_remove (GST_BIN (self), self->vid_colorspace);
    gst_element_set_state (self->vid_colorspace, GST_STATE_NULL);
    gst_object_unref (self->vid_colorspace);
    self->vid_colorspace = NULL;
  }
  if (self->vf_colorspace) {
    gst_bin_remove (GST_BIN (self), self->vf_colorspace);
    gst_element_set_state (self->vf_colorspace, GST_STATE_NULL);
    gst_object_unref (self->vf_colorspace);
    self->vf_colorspace = NULL;
  }
  iter = gst_bin_iterate_elements (GST_BIN (self));
  done = FALSE;
  while (!done) {
    GValue data = { 0, };

    switch (gst_iterator_next (iter, &data)) {
      case GST_ITERATOR_OK:
      {
        GstElement *child = g_value_get_object (&data);
        if (child != self->v4l2_src) {
          gst_bin_remove (GST_BIN (self), child);
          gst_element_set_state (child, GST_STATE_NULL);
        }
        g_value_reset (&data);
        break;
      }
      case GST_ITERATOR_RESYNC:
        gst_iterator_resync (iter);
        break;
      case GST_ITERATOR_ERROR:
        done = TRUE;
        break;
      case GST_ITERATOR_DONE:
        done = TRUE;
        break;
    }
  }
  gst_iterator_free (iter);
}

static gboolean
ensure_v4l2src (GstUvcH264Src * self)
{
  gchar *device = NULL;
  GstClock *v4l2_clock = NULL;

  if (self->v4l2_src == NULL) {
    /* Create v4l2 source and set it up */
    self->v4l2_src = gst_element_factory_make ("v4l2src", NULL);
    if (!self->v4l2_src || !gst_bin_add (GST_BIN (self), self->v4l2_src))
      goto error;
    gst_object_ref (self->v4l2_src);
    g_signal_connect (self->v4l2_src, "prepare-format",
        (GCallback) v4l2src_prepare_format, self);
  }

  g_object_get (self->v4l2_src, "device", &device, NULL);
  g_object_set (self->v4l2_src,
      "device", self->device, "num-buffers", self->num_buffers, NULL);

  v4l2_clock = gst_element_get_clock (self->v4l2_src);

  /* Set to NULL if the device changed */
  if (g_strcmp0 (device, self->device))
    gst_element_set_state (self->v4l2_src, GST_STATE_NULL);
  g_free (device);

  if (gst_element_set_state (self->v4l2_src, GST_STATE_READY) !=
      GST_STATE_CHANGE_SUCCESS) {
    GST_DEBUG_OBJECT (self, "Unable to set v4l2src to READY state");
    goto error_remove;
  }

  /* Set/Update the fd and unit id after we go to READY */
  g_object_get (self->v4l2_src, "device-fd", &self->v4l2_fd, NULL);
  self->h264_unit_id =
      xu_get_id (GST_OBJECT (self), self->device, &self->usb_ctx);

  if (self->h264_unit_id == 0) {
    GST_ELEMENT_ERROR (self, RESOURCE, SETTINGS,
        ("Device is not a valid UVC H264 camera"), (NULL));
    goto error_remove;
  }

  /* going to state READY makes v4l2src lose its reference to the clock */
  if (v4l2_clock) {
    gst_element_set_clock (self->v4l2_src, v4l2_clock);
    gst_element_set_base_time (self->v4l2_src,
        gst_element_get_base_time (GST_ELEMENT (self)));
    gst_object_unref (v4l2_clock);
  }

  return TRUE;

error_remove:
  gst_element_set_state (self->v4l2_src, GST_STATE_NULL);
  gst_bin_remove (GST_BIN (self), self->v4l2_src);

error:
  if (v4l2_clock)
    gst_object_unref (v4l2_clock);
  if (self->v4l2_src)
    gst_object_unref (self->v4l2_src);
  self->v4l2_src = NULL;
  self->v4l2_fd = -1;
  self->h264_unit_id = 0;

  return FALSE;
}

static gboolean
gst_uvc_h264_src_construct_pipeline (GstBaseCameraSrc * bcamsrc)
{
  GstUvcH264Src *self = GST_UVC_H264_SRC (bcamsrc);
  GstIterator *iter = NULL;
  gboolean iter_done = FALSE;
  GstPad *vf_pad = NULL;
  GstCaps *vf_caps = NULL;
  GstStructure *vf_struct = NULL;
  GstPad *vid_pad = NULL;
  GstCaps *vid_caps = NULL;
  GstStructure *vid_struct = NULL;
  GstCaps *src_caps = NULL;
  GstPad *v4l_pad = NULL;
  GstCaps *v4l_caps = NULL;
  gboolean jpg2raw = FALSE;

  enum
  {
    RAW_NONE, ENCODED_NONE, NONE_RAW, NONE_ENCODED,
    H264_JPG, H264_RAW, H264_JPG2RAW, NONE_NONE,
    RAW_RAW, ENCODED_ENCODED,
  } type;

  GST_DEBUG_OBJECT (self, "Construct pipeline");
  self->reconfiguring = TRUE;

  if (self->v4l2_src) {
    uvcx_encoder_reset req = { 0 };

    if (!xu_query (self, UVCX_ENCODER_RESET, UVC_SET_CUR, (guchar *) & req))
      GST_WARNING_OBJECT (self, " UVCX_ENCODER_RESET SET_CUR error");
  }

  if (!ensure_v4l2src (self))
    goto error;

  gst_uvc_h264_src_destroy_pipeline (self, FALSE);

  /* Potentially unlink v4l2src to the ghost pads */
  gst_ghost_pad_set_target (GST_GHOST_PAD (self->vidsrc), NULL);
  gst_ghost_pad_set_target (GST_GHOST_PAD (self->vfsrc), NULL);

  if (gst_pad_is_linked (self->vfsrc))
    vf_caps = gst_pad_peer_query_caps (self->vfsrc, NULL);
  if (gst_pad_is_linked (self->vidsrc))
    vid_caps = gst_pad_peer_query_caps (self->vidsrc, NULL);

  GST_DEBUG_OBJECT (self, "vfsrc caps : %" GST_PTR_FORMAT, vf_caps);
  GST_DEBUG_OBJECT (self, "vidsrc caps : %" GST_PTR_FORMAT, vid_caps);
  if (!self->started) {
    GST_DEBUG_OBJECT (self, "video not started. Ignoring vidsrc caps");
    if (vid_caps)
      gst_caps_unref (vid_caps);
    vid_caps = NULL;
  }

  v4l_pad = gst_element_get_static_pad (self->v4l2_src, "src");
  v4l_caps = gst_pad_query_caps (v4l_pad, NULL);
  GST_DEBUG_OBJECT (self, "v4l2src caps : %" GST_PTR_FORMAT, v4l_caps);
  if (vid_caps) {
    GstCaps *trans_caps = gst_uvc_h264_src_transform_caps (self, vid_caps);

    gst_caps_unref (vid_caps);
    vid_caps = gst_uvc_h264_src_fixate_caps (self, v4l_pad, v4l_caps,
        trans_caps, TRUE);
    gst_caps_unref (trans_caps);

    if (vid_caps) {
      vid_struct = gst_caps_get_structure (vid_caps, 0);
    } else {
      GST_WARNING_OBJECT (self, "Could not negotiate vidsrc caps format");
      gst_object_unref (v4l_pad);
      gst_caps_unref (v4l_caps);
      goto error_remove;
    }
  }
  GST_DEBUG_OBJECT (self, "Fixated vidsrc caps : %" GST_PTR_FORMAT, vid_caps);

  if (vid_caps && gst_structure_has_name (vid_struct, "video/x-h264")) {
    self->main_format = UVC_H264_SRC_FORMAT_H264;
    if (!_extract_caps_info (vid_struct, &self->main_width,
            &self->main_height, &self->main_frame_interval)) {
      gst_object_unref (v4l_pad);
      gst_caps_unref (v4l_caps);
      goto error_remove;
    }

    self->main_stream_format = _extract_stream_format (vid_struct);
    self->main_profile = _extract_profile (vid_struct);
  } else {
    self->main_format = UVC_H264_SRC_FORMAT_NONE;
  }

  if (vf_caps) {
    GstCaps *trans_caps = gst_uvc_h264_src_transform_caps (self, vf_caps);

    gst_caps_unref (vf_caps);
    vf_caps = gst_uvc_h264_src_fixate_caps (self, v4l_pad, v4l_caps,
        trans_caps, FALSE);

    /* If we couldn't find a suitable vf cap, try the jpeg2raw pipeline */
    if (!vf_caps && self->main_format == UVC_H264_SRC_FORMAT_H264) {
      GstCaps *jpg_caps;

      jpg2raw = TRUE;
      jpg_caps = _transform_caps (self, trans_caps, self->jpeg_decoder_name);

      vf_caps = gst_uvc_h264_src_fixate_caps (self, v4l_pad, v4l_caps,
          jpg_caps, FALSE);
      gst_caps_unref (jpg_caps);
    }
    gst_caps_unref (trans_caps);
    if (vf_caps) {
      vf_struct = gst_caps_get_structure (vf_caps, 0);
    } else {
      GST_WARNING_OBJECT (self, "Could not negotiate vfsrc caps format");
      gst_object_unref (v4l_pad);
      gst_caps_unref (v4l_caps);
      goto error_remove;
    }
  }
  GST_DEBUG_OBJECT (self, "Fixated vfsrc caps : %" GST_PTR_FORMAT, vf_caps);
  gst_object_unref (v4l_pad);
  gst_caps_unref (v4l_caps);

  if (vf_caps && vid_caps &&
      !gst_structure_has_name (vid_struct, "video/x-h264")) {
    /* Allow for vfsrc+vidsrc to both be raw or jpeg */
    if (gst_structure_has_name (vid_struct, "image/jpeg") &&
        gst_structure_has_name (vf_struct, "image/jpeg")) {
      self->main_format = UVC_H264_SRC_FORMAT_JPG;
      self->secondary_format = UVC_H264_SRC_FORMAT_JPG;
      type = ENCODED_ENCODED;
    } else if (!gst_structure_has_name (vid_struct, "image/jpeg") &&
        !gst_structure_has_name (vf_struct, "image/jpeg")) {
      self->main_format = UVC_H264_SRC_FORMAT_RAW;
      self->secondary_format = UVC_H264_SRC_FORMAT_RAW;
      type = RAW_RAW;
    } else {
      goto error_remove;
    }
  } else if (vf_caps && vid_caps) {
    guint32 smallest_frame_interval;

    if (!_extract_caps_info (vf_struct, &self->secondary_width,
            &self->secondary_height, &self->secondary_frame_interval))
      goto error_remove;

    if (jpg2raw == FALSE && gst_structure_has_name (vf_struct, "image/jpeg")) {
      type = H264_JPG;
      self->secondary_format = UVC_H264_SRC_FORMAT_JPG;
    } else {
      if (jpg2raw) {
        type = H264_JPG2RAW;
        self->secondary_format = UVC_H264_SRC_FORMAT_JPG;
      } else {
        type = H264_RAW;
        self->secondary_format = UVC_H264_SRC_FORMAT_RAW;
      }
    }
    smallest_frame_interval = MIN (self->main_frame_interval,
        self->secondary_frame_interval);
    /* Just to avoid a potential division by zero, set interval to 30 fps */
    if (smallest_frame_interval == 0)
      smallest_frame_interval = 333333;

    /* Frame interval is in 100ns units */
    src_caps = gst_caps_new_simple ("image/jpeg",
        "width", G_TYPE_INT, self->secondary_width,
        "height", G_TYPE_INT, self->secondary_height,
        "framerate", GST_TYPE_FRACTION,
        NSEC_PER_SEC / smallest_frame_interval, 100, NULL);
  } else if (vf_caps || vid_caps) {
    self->secondary_format = UVC_H264_SRC_FORMAT_NONE;
    if (vid_struct && gst_structure_has_name (vid_struct, "video/x-h264")) {
      type = ENCODED_NONE;
    } else if (vid_struct && gst_structure_has_name (vid_struct, "image/jpeg")) {
      type = ENCODED_NONE;
      self->main_format = UVC_H264_SRC_FORMAT_JPG;
    } else if (vf_struct && gst_structure_has_name (vf_struct, "image/jpeg")) {
      type = NONE_ENCODED;
      self->secondary_format = UVC_H264_SRC_FORMAT_JPG;
    } else if (vid_struct) {
      type = RAW_NONE;
      self->main_format = UVC_H264_SRC_FORMAT_RAW;
    } else if (vf_struct) {
      type = NONE_RAW;
      self->secondary_format = UVC_H264_SRC_FORMAT_RAW;
    } else {
      g_assert_not_reached ();
      type = NONE_NONE;
      self->main_format = UVC_H264_SRC_FORMAT_NONE;
    }
  } else {
    type = NONE_NONE;
    self->main_format = UVC_H264_SRC_FORMAT_NONE;
    self->secondary_format = UVC_H264_SRC_FORMAT_NONE;
  }

  switch (type) {
    case NONE_NONE:
      GST_DEBUG_OBJECT (self, "None+None");
      vf_pad = gst_element_get_static_pad (self->v4l2_src, "src");
      break;
    case RAW_NONE:
      GST_DEBUG_OBJECT (self, "Raw+None");
      self->vid_colorspace = gst_element_factory_make (self->colorspace_name,
          NULL);
      if (!self->vid_colorspace ||
          !gst_bin_add (GST_BIN (self), self->vid_colorspace))
        goto error_remove;
      gst_object_ref (self->vid_colorspace);
      if (!gst_element_link (self->v4l2_src, self->vid_colorspace))
        goto error_remove_all;
      vid_pad = gst_element_get_static_pad (self->vid_colorspace, "src");
      break;
    case NONE_RAW:
      GST_DEBUG_OBJECT (self, "None+Raw");
      self->vf_colorspace = gst_element_factory_make (self->colorspace_name,
          NULL);
      if (!self->vf_colorspace ||
          !gst_bin_add (GST_BIN (self), self->vf_colorspace))
        goto error_remove;
      gst_object_ref (self->vf_colorspace);
      if (!gst_element_link (self->v4l2_src, self->vf_colorspace))
        goto error_remove_all;
      vf_pad = gst_element_get_static_pad (self->vf_colorspace, "src");
      break;
    case ENCODED_NONE:
      GST_DEBUG_OBJECT (self, "Encoded+None");
      vid_pad = gst_element_get_static_pad (self->v4l2_src, "src");
      break;
    case NONE_ENCODED:
      GST_DEBUG_OBJECT (self, "None+Encoded");
      vf_pad = gst_element_get_static_pad (self->v4l2_src, "src");
      break;
    case H264_JPG:
      GST_DEBUG_OBJECT (self, "H264+JPG");
      self->mjpg_demux = gst_element_factory_make ("uvch264mjpgdemux", NULL);
      if (!self->mjpg_demux || !gst_bin_add (GST_BIN (self), self->mjpg_demux))
        goto error_remove;
      gst_object_ref (self->mjpg_demux);
      g_object_set (self->mjpg_demux, "device-fd", self->v4l2_fd,
          "num-clock-samples", self->num_clock_samples, NULL);
      if (!gst_element_link_filtered (self->v4l2_src, self->mjpg_demux,
              src_caps))
        goto error_remove_all;
      vid_pad = gst_element_get_static_pad (self->mjpg_demux, "h264");
      vf_pad = gst_element_get_static_pad (self->mjpg_demux, "jpeg");
      break;
    case H264_RAW:
      GST_DEBUG_OBJECT (self, "H264+Raw");
      self->mjpg_demux = gst_element_factory_make ("uvch264mjpgdemux", NULL);
      self->vf_colorspace = gst_element_factory_make (self->colorspace_name,
          NULL);
      if (!self->mjpg_demux || !self->vf_colorspace)
        goto error_remove;
      if (!gst_bin_add (GST_BIN (self), self->mjpg_demux))
        goto error_remove;
      gst_object_ref (self->mjpg_demux);
      g_object_set (self->mjpg_demux, "device-fd", self->v4l2_fd,
          "num-clock-samples", self->num_clock_samples, NULL);
      if (!gst_bin_add (GST_BIN (self), self->vf_colorspace)) {
        gst_object_unref (self->vf_colorspace);
        self->vf_colorspace = NULL;
        goto error_remove_all;
      }
      gst_object_ref (self->vf_colorspace);
      if (!gst_element_link_filtered (self->v4l2_src, self->mjpg_demux,
              src_caps))
        goto error_remove_all;
      if (!gst_element_link_pads (self->mjpg_demux, "yuy2",
              self->vf_colorspace, "sink"))
        goto error_remove_all;
      vid_pad = gst_element_get_static_pad (self->mjpg_demux, "h264");
      vf_pad = gst_element_get_static_pad (self->vf_colorspace, "src");
      break;
    case H264_JPG2RAW:
      GST_DEBUG_OBJECT (self, "H264+Raw(jpegdec)");
      self->mjpg_demux = gst_element_factory_make ("uvch264mjpgdemux", NULL);
      self->jpeg_dec = gst_element_factory_make (self->jpeg_decoder_name, NULL);
      self->vf_colorspace = gst_element_factory_make (self->colorspace_name,
          NULL);
      if (!self->mjpg_demux || !self->jpeg_dec || !self->vf_colorspace)
        goto error_remove;
      if (!gst_bin_add (GST_BIN (self), self->mjpg_demux))
        goto error_remove;
      gst_object_ref (self->mjpg_demux);
      g_object_set (self->mjpg_demux, "device-fd", self->v4l2_fd,
          "num-clock-samples", self->num_clock_samples, NULL);
      if (!gst_bin_add (GST_BIN (self), self->jpeg_dec)) {
        gst_object_unref (self->jpeg_dec);
        self->jpeg_dec = NULL;
        gst_object_unref (self->vf_colorspace);
        self->vf_colorspace = NULL;
        goto error_remove_all;
      }
      gst_object_ref (self->jpeg_dec);
      if (!gst_bin_add (GST_BIN (self), self->vf_colorspace)) {
        gst_object_unref (self->vf_colorspace);
        self->vf_colorspace = NULL;
        goto error_remove_all;
      }
      gst_object_ref (self->vf_colorspace);
      if (!gst_element_link_filtered (self->v4l2_src, self->mjpg_demux,
              src_caps))
        goto error_remove_all;
      if (!gst_element_link_pads (self->mjpg_demux, "jpeg", self->jpeg_dec,
              "sink"))
        goto error_remove_all;
      if (!gst_element_link (self->jpeg_dec, self->vf_colorspace))
        goto error_remove_all;
      vid_pad = gst_element_get_static_pad (self->mjpg_demux, "h264");
      vf_pad = gst_element_get_static_pad (self->vf_colorspace, "src");
      break;
    case RAW_RAW:
    {
      GstElement *tee = NULL;

      GST_DEBUG_OBJECT (self, "Raw+Raw");
      tee = gst_element_factory_make ("tee", NULL);
      if (!tee || !gst_bin_add (GST_BIN (self), tee)) {
        if (tee)
          gst_object_unref (tee);
        goto error_remove;
      }
      self->vf_colorspace = gst_element_factory_make (self->colorspace_name,
          NULL);
      self->vid_colorspace = gst_element_factory_make (self->colorspace_name,
          NULL);
      if (!self->vf_colorspace || !self->vid_colorspace)
        goto error_remove;
      if (!gst_bin_add (GST_BIN (self), self->vf_colorspace))
        goto error_remove;
      gst_object_ref (self->vf_colorspace);
      if (!gst_bin_add (GST_BIN (self), self->vid_colorspace)) {
        gst_object_unref (self->vid_colorspace);
        self->vid_colorspace = NULL;
        goto error_remove_all;
      }
      gst_object_ref (self->vid_colorspace);
      if (!gst_element_link (self->v4l2_src, tee))
        goto error_remove_all;
      if (!gst_element_link (tee, self->vf_colorspace))
        goto error_remove_all;
      if (!gst_element_link (tee, self->vid_colorspace))
        goto error_remove_all;
      vf_pad = gst_element_get_static_pad (self->vf_colorspace, "src");
      vid_pad = gst_element_get_static_pad (self->vid_colorspace, "src");
    }
      break;
    case ENCODED_ENCODED:
    {
      GstElement *tee = NULL;

      GST_DEBUG_OBJECT (self, "Encoded+Encoded");
      tee = gst_element_factory_make ("tee", NULL);
      if (!tee || !gst_bin_add (GST_BIN (self), tee)) {
        if (tee)
          gst_object_unref (tee);
        goto error_remove;
      }
      if (!gst_element_link (self->v4l2_src, tee))
        goto error_remove_all;
      vf_pad = gst_element_request_pad_simple (tee, "src_%u");
      vid_pad = gst_element_request_pad_simple (tee, "src_%u");
    }
      break;
  }

  if (!gst_ghost_pad_set_target (GST_GHOST_PAD (self->vidsrc), vid_pad) ||
      !gst_ghost_pad_set_target (GST_GHOST_PAD (self->vfsrc), vf_pad))
    goto error_remove_all;
  if (vid_pad)
    gst_object_unref (vid_pad);
  if (vf_pad)
    gst_object_unref (vf_pad);
  vid_pad = vf_pad = NULL;

  if (vf_caps)
    gst_caps_unref (vf_caps);
  if (vid_caps)
    gst_caps_unref (vid_caps);
  if (src_caps)
    gst_caps_unref (src_caps);
  vf_caps = vid_caps = src_caps = NULL;

  /* Sync children states, in sink to source order */
  if (self->vid_colorspace &&
      !gst_element_sync_state_with_parent (self->vid_colorspace))
    goto error_remove_all;
  if (self->vf_colorspace &&
      !gst_element_sync_state_with_parent (self->vf_colorspace))
    goto error_remove_all;
  if (self->jpeg_dec && !gst_element_sync_state_with_parent (self->jpeg_dec))
    goto error_remove_all;
  if (self->mjpg_demux &&
      !gst_element_sync_state_with_parent (self->mjpg_demux))
    goto error_remove_all;
  if (self->v4l2_src && !gst_element_sync_state_with_parent (self->v4l2_src))
    goto error_remove_all;

  /* Sync any remaining children states with bin's state */
  iter = gst_bin_iterate_elements (GST_BIN (self));
  iter_done = FALSE;
  while (!iter_done) {
    GstElement *child = NULL;
    GValue data = { 0, };

    switch (gst_iterator_next (iter, &data)) {
      case GST_ITERATOR_OK:
        child = g_value_get_object (&data);
        if (!gst_element_sync_state_with_parent (child)) {
          g_value_reset (&data);
          gst_iterator_free (iter);
          goto error_remove_all;
        }
        g_value_reset (&data);
        break;
      case GST_ITERATOR_RESYNC:
        gst_iterator_resync (iter);
        break;
      case GST_ITERATOR_ERROR:
        iter_done = TRUE;
        break;
      case GST_ITERATOR_DONE:
        iter_done = TRUE;
        break;
    }
  }
  gst_iterator_free (iter);

  self->reconfiguring = FALSE;
  return TRUE;

error_remove_all:
  gst_uvc_h264_src_destroy_pipeline (self, FALSE);
error_remove:
  gst_element_set_state (self->v4l2_src, GST_STATE_NULL);
  gst_bin_remove (GST_BIN (self), self->v4l2_src);

error:
  if (self->v4l2_src)
    gst_object_unref (self->v4l2_src);
  self->v4l2_src = NULL;
  self->v4l2_fd = -1;
  self->h264_unit_id = 0;

  if (self->mjpg_demux)
    gst_object_unref (self->mjpg_demux);
  self->mjpg_demux = NULL;
  if (self->jpeg_dec)
    gst_object_unref (self->jpeg_dec);
  self->jpeg_dec = NULL;
  if (self->vid_colorspace)
    gst_object_unref (self->vid_colorspace);
  self->vid_colorspace = NULL;
  if (self->vf_colorspace)
    gst_object_unref (self->vf_colorspace);
  self->vf_colorspace = NULL;

  if (src_caps)
    gst_caps_unref (src_caps);

  if (vf_caps)
    gst_caps_unref (vf_caps);
  if (vid_caps)
    gst_caps_unref (vid_caps);

  if (vid_pad)
    gst_object_unref (vid_pad);
  if (vf_pad)
    gst_object_unref (vf_pad);

  self->reconfiguring = FALSE;
  return FALSE;
}

static GstCaps *
gst_uvc_h264_src_getcaps (GstPad * pad, GstObject * parent, GstQuery * query)
{
  GstUvcH264Src *self = GST_UVC_H264_SRC (parent);
  GstCaps *template = NULL;
  GstCaps *result = NULL;

  if (pad == self->vfsrc)
    template = gst_static_pad_template_get_caps (&vfsrc_template);
  else if (pad == self->vidsrc)
    template = gst_static_pad_template_get_caps (&vidsrc_template);
  else
    template = gst_caps_new_empty ();

  if (self->v4l2_src) {
    GstCaps *filter;
    GstPad *v4l_pad = gst_element_get_static_pad (self->v4l2_src, "src");
    GstCaps *v4l_caps = NULL;
    GstCaps *new_caps = NULL;

    gst_query_parse_caps (query, &filter);
    v4l_caps = gst_pad_query_caps (v4l_pad, filter);
    new_caps = gst_uvc_h264_src_transform_caps (self, v4l_caps);

    result = gst_caps_intersect (new_caps, template);
    gst_object_unref (v4l_pad);
    gst_caps_unref (v4l_caps);
    gst_caps_unref (new_caps);
    gst_caps_unref (template);
  } else {
    result = template;
  }

  return result;
}

static gboolean
gst_uvc_h264_src_query (GstPad * pad, GstObject * parent, GstQuery * query)
{
  gboolean ret;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CAPS:{
      GstCaps *caps;

      caps = gst_uvc_h264_src_getcaps (pad, parent, query);
      gst_query_set_caps_result (query, caps);
      gst_caps_unref (caps);
      ret = TRUE;
      break;
    }
    default:
      ret = gst_pad_query_default (pad, parent, query);
      break;
  }
  return ret;
}


static gboolean
gst_uvc_h264_src_set_mode (GstBaseCameraSrc * bcamsrc, GstCameraBinMode mode)
{
  GstUvcH264Src *self = GST_UVC_H264_SRC (bcamsrc);

  GST_DEBUG_OBJECT (self, "set mode to %d", mode);

  return (mode == MODE_VIDEO);
}

static gboolean
gst_uvc_h264_src_start_capture (GstBaseCameraSrc * camerasrc)
{
  GstUvcH264Src *self = GST_UVC_H264_SRC (camerasrc);
  gboolean ret = TRUE;

  GST_DEBUG_OBJECT (self, "start capture");

  if (!self->started) {
    self->started = TRUE;
    if (GST_STATE (self) >= GST_STATE_READY) {
      ret = gst_uvc_h264_src_construct_pipeline (GST_BASE_CAMERA_SRC (self));
      if (!ret) {
        GST_DEBUG_OBJECT (self, "Could not start capture");
        self->started = FALSE;
        gst_uvc_h264_src_construct_pipeline (GST_BASE_CAMERA_SRC (self));
      }
    }
  }

  return ret;
}

static void
gst_uvc_h264_src_stop_capture (GstBaseCameraSrc * camerasrc)
{
  GstUvcH264Src *self = GST_UVC_H264_SRC (camerasrc);

  GST_DEBUG_OBJECT (self, "stop capture");

  if (self->started) {
    self->started = FALSE;
    if (GST_STATE (self) >= GST_STATE_READY)
      gst_uvc_h264_src_construct_pipeline (GST_BASE_CAMERA_SRC (self));
    gst_base_camera_src_finish_capture (camerasrc);
  }
}

static void
gst_uvc_h264_src_pad_linking_cb (GstPad * pad,
    GstPad * peer, gpointer user_data)
{
  GstUvcH264Src *self = GST_UVC_H264_SRC (user_data);
  gchar *pad_name = gst_pad_get_name (pad);

  GST_DEBUG_OBJECT (self, "Pad %s was (un)linked. Renegotiating", pad_name);
  g_free (pad_name);
  if (GST_STATE (self) >= GST_STATE_READY)
    gst_uvc_h264_src_construct_pipeline (GST_BASE_CAMERA_SRC (self));
}


static GstStateChangeReturn
gst_uvc_h264_src_change_state (GstElement * element, GstStateChange trans)
{
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  GstUvcH264Src *self = GST_UVC_H264_SRC (element);

  switch (trans) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      if (!ensure_v4l2src (self)) {
        ret = GST_STATE_CHANGE_FAILURE;
        goto end;
      }
      gst_segment_init (&self->segment, GST_FORMAT_UNDEFINED);
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      if (!self->v4l2_src)
        gst_uvc_h264_src_construct_pipeline (GST_BASE_CAMERA_SRC (self));
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, trans);

  if (ret == GST_STATE_CHANGE_FAILURE)
    goto end;

  switch (trans) {
    case GST_STATE_CHANGE_READY_TO_NULL:
      gst_uvc_h264_src_destroy_pipeline (self, TRUE);
      break;
    default:
      break;
  }


end:
  return ret;
}
