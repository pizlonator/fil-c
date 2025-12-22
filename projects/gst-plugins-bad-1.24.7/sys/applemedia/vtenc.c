/*
 * Copyright (C) 2010, 2013 Ole André Vadla Ravnås <oleavr@soundrop.com>
 * Copyright (C) 2013 Intel Corporation
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
 * SECTION:element-vtenc_h264
 * @title: vtenc_h264
 *
 * Apple VideoToolbox H264 encoder, which can either use HW or a SW
 * implementation depending on the device.
 *
 * ## Example pipeline
 * |[
 * gst-launch-1.0 -v videotestsrc ! vtenc_h264 ! qtmux ! filesink location=out.mov
 * ]| Encode a test video pattern and save it as an MOV file
 *
 */

/**
 * SECTION:element-vtenc_h264_hw
 * @title: vtenc_h264_hw
 *
 * Apple VideoToolbox H264 HW-only encoder (only available on macOS at
 * present).
 *
 * ## Example pipeline
 * |[
 * gst-launch-1.0 -v videotestsrc ! vtenc_h264_hw ! qtmux ! filesink location=out.mov
 * ]| Encode a test video pattern and save it as an MOV file
 *
 */

/**
 * SECTION:element-vtenc_prores
 * @title: vtenc_prores
 *
 * Apple VideoToolbox ProRes encoder
 *
 * ## Example pipeline
 * |[
 * gst-launch-1.0 -v videotestsrc ! vtenc_prores ! qtmux ! filesink location=out.mov
 * ]| Encode a test video pattern and save it as an MOV file
 *
 * Since: 1.20
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "vtenc.h"

#include "coremediabuffer.h"
#include "corevideobuffer.h"
#include "vtutil.h"
#include "helpers.h"
#include <gst/pbutils/codec-utils.h>
#include <sys/sysctl.h>

#define VTENC_DEFAULT_BITRATE     0
#define VTENC_DEFAULT_FRAME_REORDERING TRUE
#define VTENC_DEFAULT_REALTIME FALSE
#define VTENC_DEFAULT_QUALITY 0.5
#define VTENC_DEFAULT_MAX_KEYFRAME_INTERVAL 0
#define VTENC_DEFAULT_MAX_KEYFRAME_INTERVAL_DURATION 0
#define VTENC_DEFAULT_PRESERVE_ALPHA TRUE
#define VTENC_OUTPUT_QUEUE_SIZE 3

GST_DEBUG_CATEGORY (gst_vtenc_debug);
#define GST_CAT_DEFAULT (gst_vtenc_debug)

#define GST_VTENC_CODEC_DETAILS_QDATA \
    g_quark_from_static_string ("vtenc-codec-details")

#define CMTIME_TO_GST_CLOCK_TIME(time) time.value / (time.timescale / GST_SECOND)

/* define EnableHardwareAcceleratedVideoEncoder in < 10.9 */
#if defined(MAC_OS_X_VERSION_MAX_ALLOWED) && MAC_OS_X_VERSION_MAX_ALLOWED < 1090
const CFStringRef
    kVTVideoEncoderSpecification_EnableHardwareAcceleratedVideoEncoder =
CFSTR ("EnableHardwareAcceleratedVideoEncoder");
const CFStringRef
    kVTVideoEncoderSpecification_RequireHardwareAcceleratedVideoEncoder =
CFSTR ("RequireHardwareAcceleratedVideoEncoder");
const CFStringRef kVTCompressionPropertyKey_ProfileLevel =
CFSTR ("ProfileLevel");
const CFStringRef kVTProfileLevel_H264_Baseline_AutoLevel =
CFSTR ("H264_Baseline_AutoLevel");
#endif

#if defined(MAC_OS_X_VERSION_MAX_ALLOWED) && MAC_OS_X_VERSION_MAX_ALLOWED < 1080
const CFStringRef kVTCompressionPropertyKey_Quality = CFSTR ("Quality");
#endif

#ifdef HAVE_VIDEOTOOLBOX_10_9_6
extern OSStatus
VTCompressionSessionPrepareToEncodeFrames (VTCompressionSessionRef session)
    __attribute__((weak_import));
#endif

/* This property key is currently completely undocumented. The only way you can
 * know about its existence is if Apple tells you. It allows you to tell the
 * encoder to not preserve alpha even when outputting alpha formats. */
const CFStringRef gstVTCodecPropertyKey_PreserveAlphaChannel =
CFSTR ("kVTCodecPropertyKey_PreserveAlphaChannel");

enum
{
  PROP_0,
  PROP_USAGE,
  PROP_BITRATE,
  PROP_ALLOW_FRAME_REORDERING,
  PROP_REALTIME,
  PROP_QUALITY,
  PROP_MAX_KEYFRAME_INTERVAL,
  PROP_MAX_KEYFRAME_INTERVAL_DURATION,
  PROP_PRESERVE_ALPHA,
};

typedef struct _GstVTEncFrame GstVTEncFrame;

struct _GstVTEncFrame
{
  GstBuffer *buf;
  GstVideoFrame videoframe;
};

static GstElementClass *parent_class = NULL;

static void gst_vtenc_get_property (GObject * obj, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_vtenc_set_property (GObject * obj, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_vtenc_finalize (GObject * obj);

static gboolean gst_vtenc_start (GstVideoEncoder * enc);
static gboolean gst_vtenc_stop (GstVideoEncoder * enc);
static void gst_vtenc_loop (GstVTEnc * self);
static gboolean gst_vtenc_set_format (GstVideoEncoder * enc,
    GstVideoCodecState * input_state);
static GstFlowReturn gst_vtenc_handle_frame (GstVideoEncoder * enc,
    GstVideoCodecFrame * frame);
static GstStateChangeReturn gst_vtenc_change_state (GstElement * element,
    GstStateChange transition);
static GstFlowReturn gst_vtenc_finish (GstVideoEncoder * enc);
static gboolean gst_vtenc_flush (GstVideoEncoder * enc);
static gboolean gst_vtenc_sink_event (GstVideoEncoder * enc, GstEvent * event);
static VTCompressionSessionRef gst_vtenc_create_session (GstVTEnc * self);
static void gst_vtenc_destroy_session (GstVTEnc * self,
    VTCompressionSessionRef * session);
static void gst_vtenc_session_dump_properties (GstVTEnc * self,
    VTCompressionSessionRef session);
static void gst_vtenc_session_configure_expected_framerate (GstVTEnc * self,
    VTCompressionSessionRef session, gdouble framerate);
static void gst_vtenc_session_configure_max_keyframe_interval (GstVTEnc * self,
    VTCompressionSessionRef session, gint interval);
static void gst_vtenc_session_configure_max_keyframe_interval_duration
    (GstVTEnc * self, VTCompressionSessionRef session, gdouble duration);
static void gst_vtenc_session_configure_bitrate (GstVTEnc * self,
    VTCompressionSessionRef session, guint bitrate);
static OSStatus gst_vtenc_session_configure_property_int (GstVTEnc * self,
    VTCompressionSessionRef session, CFStringRef name, gint value);
static OSStatus gst_vtenc_session_configure_property_double (GstVTEnc * self,
    VTCompressionSessionRef session, CFStringRef name, gdouble value);
static void gst_vtenc_session_configure_allow_frame_reordering (GstVTEnc * self,
    VTCompressionSessionRef session, gboolean allow_frame_reordering);
static void gst_vtenc_session_configure_realtime (GstVTEnc * self,
    VTCompressionSessionRef session, gboolean realtime);

static GstFlowReturn gst_vtenc_encode_frame (GstVTEnc * self,
    GstVideoCodecFrame * frame);
static void gst_vtenc_enqueue_buffer (void *outputCallbackRefCon,
    void *sourceFrameRefCon, OSStatus status, VTEncodeInfoFlags infoFlags,
    CMSampleBufferRef sampleBuffer);
static gboolean gst_vtenc_buffer_is_keyframe (GstVTEnc * self,
    CMSampleBufferRef sbuf);


#ifndef HAVE_IOS
static GstVTEncFrame *gst_vtenc_frame_new (GstBuffer * buf,
    GstVideoInfo * videoinfo);
static void gst_vtenc_frame_free (GstVTEncFrame * frame);

static void gst_pixel_buffer_release_cb (void *releaseRefCon,
    const void *dataPtr, size_t dataSize, size_t numberOfPlanes,
    const void *planeAddresses[]);
#endif

#ifdef HAVE_IOS
static GstStaticCaps sink_caps =
GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("{ NV12, I420 }"));
#else
static GstStaticCaps sink_caps =
GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("{ AYUV64, UYVY, NV12, I420 }"));
#endif


static void
gst_vtenc_base_init (GstVTEncClass * klass)
{
  const GstVTEncoderDetails *codec_details =
      GST_VTENC_CLASS_GET_CODEC_DETAILS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  const int min_width = 1, max_width = G_MAXINT;
  const int min_height = 1, max_height = G_MAXINT;
  const int min_fps_n = 0, max_fps_n = G_MAXINT;
  const int min_fps_d = 1, max_fps_d = 1;
  GstCaps *src_caps;
  gchar *longname, *description;

  longname = g_strdup_printf ("%s encoder", codec_details->name);
  description = g_strdup_printf ("%s encoder", codec_details->name);

  gst_element_class_set_metadata (element_class, longname,
      "Codec/Encoder/Video/Hardware", description,
      "Ole André Vadla Ravnås <oleavr@soundrop.com>, Dominik Röttsches <dominik.rottsches@intel.com>");

  g_free (longname);
  g_free (description);

  {
    GstCaps *caps = gst_static_caps_get (&sink_caps);
#ifndef HAVE_IOS
    gboolean enable_argb = TRUE;
    int retval;
    char cpu_name[30];
    size_t cpu_len = 30;

    if (__builtin_available (macOS 13.0, *)) {
      /* Can't negate a __builtin_available check */
    } else {
      /* Disable ARGB64/RGBA64 if we're on M1 Pro/Max and macOS < 13.0
       * due to a bug within VideoToolbox which causes encoding to fail. */
      retval = sysctlbyname ("machdep.cpu.brand_string", &cpu_name, &cpu_len,
          NULL, 0);

      if (retval == 0 &&
          (strstr (cpu_name, "M1 Pro") != NULL ||
              strstr (cpu_name, "M1 Max") != NULL)) {
        GST_WARNING
            ("Disabling ARGB64/RGBA64 caps due to a bug in VideoToolbox "
            "on M1 Pro/Max running macOS < 13.0.");
        enable_argb = FALSE;
      }
    }

    if (enable_argb) {
      caps = gst_vtutil_caps_append_video_format (caps, "ARGB64_BE");
      /* RGBA64_LE is kCVPixelFormatType_64RGBALE, only available on macOS 11.3+ */
      if (GST_APPLEMEDIA_HAVE_64RGBALE)
        caps = gst_vtutil_caps_append_video_format (caps, "RGBA64_LE");
    }
#endif
    gst_element_class_add_pad_template (element_class,
        gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS, caps));
  }

  src_caps = gst_caps_new_simple (codec_details->mimetype,
      "width", GST_TYPE_INT_RANGE, min_width, max_width,
      "height", GST_TYPE_INT_RANGE, min_height, max_height,
      "framerate", GST_TYPE_FRACTION_RANGE,
      min_fps_n, min_fps_d, max_fps_n, max_fps_d, NULL);

  /* Signal our limited interlace support */
  {
    G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
    GValueArray *arr = g_value_array_new (2);
    GValue val = G_VALUE_INIT;

    g_value_init (&val, G_TYPE_STRING);
    g_value_set_string (&val, "progressive");
    arr = g_value_array_append (arr, &val);
    g_value_set_string (&val, "interleaved");
    arr = g_value_array_append (arr, &val);
    G_GNUC_END_IGNORE_DEPRECATIONS;
    gst_structure_set_list (gst_caps_get_structure (src_caps, 0),
        "interlace-mode", arr);
  }

  switch (codec_details->format_id) {
    case kCMVideoCodecType_H264:
      gst_structure_set (gst_caps_get_structure (src_caps, 0),
          "stream-format", G_TYPE_STRING, "avc",
          "alignment", G_TYPE_STRING, "au", NULL);
      break;
    case kCMVideoCodecType_HEVC:
      gst_structure_set (gst_caps_get_structure (src_caps, 0),
          "stream-format", G_TYPE_STRING, "hvc1",
          "alignment", G_TYPE_STRING, "au", NULL);
      break;
    case GST_kCMVideoCodecType_Some_AppleProRes:
      if (g_strcmp0 (codec_details->mimetype, "video/x-prores") == 0) {
        G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
        GValueArray *arr = g_value_array_new (6);
        GValue val = G_VALUE_INIT;

        g_value_init (&val, G_TYPE_STRING);
        g_value_set_string (&val, "standard");
        arr = g_value_array_append (arr, &val);
        g_value_set_string (&val, "4444xq");
        arr = g_value_array_append (arr, &val);
        g_value_set_string (&val, "4444");
        arr = g_value_array_append (arr, &val);
        g_value_set_string (&val, "hq");
        arr = g_value_array_append (arr, &val);
        g_value_set_string (&val, "lt");
        arr = g_value_array_append (arr, &val);
        g_value_set_string (&val, "proxy");
        arr = g_value_array_append (arr, &val);
        gst_structure_set_list (gst_caps_get_structure (src_caps, 0),
            "variant", arr);
        g_value_array_free (arr);
        g_value_unset (&val);
        G_GNUC_END_IGNORE_DEPRECATIONS;
        break;
      }
      /* fall through */
    default:
      g_assert_not_reached ();
  }

  gst_element_class_add_pad_template (element_class,
      gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS, src_caps));
  gst_caps_unref (src_caps);
}

static void
gst_vtenc_class_init (GstVTEncClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class;
  GstVideoEncoderClass *gstvideoencoder_class;

  gobject_class = (GObjectClass *) klass;
  element_class = (GstElementClass *) klass;
  gstvideoencoder_class = (GstVideoEncoderClass *) klass;

  parent_class = g_type_class_peek_parent (klass);

  gobject_class->get_property = gst_vtenc_get_property;
  gobject_class->set_property = gst_vtenc_set_property;
  gobject_class->finalize = gst_vtenc_finalize;

  element_class->change_state = GST_DEBUG_FUNCPTR (gst_vtenc_change_state);

  gstvideoencoder_class->start = gst_vtenc_start;
  gstvideoencoder_class->stop = gst_vtenc_stop;
  gstvideoencoder_class->set_format = gst_vtenc_set_format;
  gstvideoencoder_class->handle_frame = gst_vtenc_handle_frame;
  gstvideoencoder_class->finish = gst_vtenc_finish;
  gstvideoencoder_class->flush = gst_vtenc_flush;
  gstvideoencoder_class->sink_event = gst_vtenc_sink_event;

  g_object_class_install_property (gobject_class, PROP_BITRATE,
      g_param_spec_uint ("bitrate", "Bitrate",
          "Target video bitrate in kbps (0 = auto)",
          0, G_MAXUINT, VTENC_DEFAULT_BITRATE,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_ALLOW_FRAME_REORDERING,
      g_param_spec_boolean ("allow-frame-reordering", "Allow frame reordering",
          "Whether to allow frame reordering or not",
          VTENC_DEFAULT_FRAME_REORDERING,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_REALTIME,
      g_param_spec_boolean ("realtime", "Realtime",
          "Configure the encoder for realtime output",
          VTENC_DEFAULT_REALTIME,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_QUALITY,
      g_param_spec_double ("quality", "Quality",
          "The desired compression quality",
          0.0, 1.0, VTENC_DEFAULT_QUALITY,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_MAX_KEYFRAME_INTERVAL,
      g_param_spec_int ("max-keyframe-interval", "Max Keyframe Interval",
          "Maximum number of frames between keyframes (0 = auto)",
          0, G_MAXINT, VTENC_DEFAULT_MAX_KEYFRAME_INTERVAL,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_MAX_KEYFRAME_INTERVAL_DURATION,
      g_param_spec_uint64 ("max-keyframe-interval-duration",
          "Max Keyframe Interval Duration",
          "Maximum number of nanoseconds between keyframes (0 = no limit)", 0,
          G_MAXUINT64, VTENC_DEFAULT_MAX_KEYFRAME_INTERVAL_DURATION,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  /*
   * H264 doesn't support alpha components, so only add the property for prores
   */
  if (g_strcmp0 (G_OBJECT_CLASS_NAME (klass), "vtenc_prores") == 0) {
    /**
     * vtenc_prores:preserve-alpha
     *
     * Preserve non-opaque video alpha values from the input video when
     * compressing, else treat all alpha component as opaque.
     *
     * Since: 1.20
     */
    g_object_class_install_property (gobject_class, PROP_PRESERVE_ALPHA,
        g_param_spec_boolean ("preserve-alpha", "Preserve Video Alpha Values",
            "Video alpha values (non opaque) need to be preserved",
            VTENC_DEFAULT_PRESERVE_ALPHA,
            G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));
  }
}

static void
gst_vtenc_init (GstVTEnc * self)
{
  GstVTEncClass *klass = (GstVTEncClass *) G_OBJECT_GET_CLASS (self);
  CFStringRef keyframe_props_keys[] = { kVTEncodeFrameOptionKey_ForceKeyFrame };
  CFBooleanRef keyframe_props_values[] = { kCFBooleanTrue };

  self->details = GST_VTENC_CLASS_GET_CODEC_DETAILS (klass);

  /* These could be controlled by properties later */
  self->dump_properties = FALSE;
  self->dump_attributes = FALSE;
  self->latency_frames = -1;
  self->session = NULL;
  self->profile_level = NULL;
  self->have_field_order = TRUE;

  self->keyframe_props =
      CFDictionaryCreate (NULL, (const void **) keyframe_props_keys,
      (const void **) keyframe_props_values, G_N_ELEMENTS (keyframe_props_keys),
      &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

  g_mutex_init (&self->queue_mutex);
  g_cond_init (&self->queue_cond);
}

static void
gst_vtenc_finalize (GObject * obj)
{
  GstVTEnc *self = GST_VTENC_CAST (obj);

  CFRelease (self->keyframe_props);
  g_mutex_clear (&self->queue_mutex);
  g_cond_clear (&self->queue_cond);

  G_OBJECT_CLASS (parent_class)->finalize (obj);
}

static guint
gst_vtenc_get_bitrate (GstVTEnc * self)
{
  guint result;

  GST_OBJECT_LOCK (self);
  result = self->bitrate;
  GST_OBJECT_UNLOCK (self);

  return result;
}

static void
gst_vtenc_set_bitrate (GstVTEnc * self, guint bitrate)
{
  GST_OBJECT_LOCK (self);

  self->bitrate = bitrate;

  if (self->session != NULL)
    gst_vtenc_session_configure_bitrate (self, self->session, bitrate);

  GST_OBJECT_UNLOCK (self);
}

static gboolean
gst_vtenc_get_allow_frame_reordering (GstVTEnc * self)
{
  gboolean result;

  GST_OBJECT_LOCK (self);
  result = self->allow_frame_reordering;
  GST_OBJECT_UNLOCK (self);

  return result;
}

static void
gst_vtenc_set_allow_frame_reordering (GstVTEnc * self,
    gboolean allow_frame_reordering)
{
  GST_OBJECT_LOCK (self);
  self->allow_frame_reordering = allow_frame_reordering;
  if (self->session != NULL) {
    gst_vtenc_session_configure_allow_frame_reordering (self,
        self->session, allow_frame_reordering);
  }
  GST_OBJECT_UNLOCK (self);
}

static gboolean
gst_vtenc_get_realtime (GstVTEnc * self)
{
  gboolean result;

  GST_OBJECT_LOCK (self);
  result = self->realtime;
  GST_OBJECT_UNLOCK (self);

  return result;
}

static void
gst_vtenc_set_realtime (GstVTEnc * self, gboolean realtime)
{
  GST_OBJECT_LOCK (self);
  self->realtime = realtime;
  if (self->session != NULL)
    gst_vtenc_session_configure_realtime (self, self->session, realtime);
  GST_OBJECT_UNLOCK (self);
}

static gdouble
gst_vtenc_get_quality (GstVTEnc * self)
{
  gdouble result;

  GST_OBJECT_LOCK (self);
  result = self->quality;
  GST_OBJECT_UNLOCK (self);

  return result;
}

static void
gst_vtenc_set_quality (GstVTEnc * self, gdouble quality)
{
  GST_OBJECT_LOCK (self);
  self->quality = quality;
  GST_INFO_OBJECT (self, "setting quality %f", quality);
  if (self->session != NULL) {
    gst_vtenc_session_configure_property_double (self, self->session,
        kVTCompressionPropertyKey_Quality, quality);
  }
  GST_OBJECT_UNLOCK (self);
}

static gint
gst_vtenc_get_max_keyframe_interval (GstVTEnc * self)
{
  gint result;

  GST_OBJECT_LOCK (self);
  result = self->max_keyframe_interval;
  GST_OBJECT_UNLOCK (self);

  return result;
}

static void
gst_vtenc_set_max_keyframe_interval (GstVTEnc * self, gint interval)
{
  GST_OBJECT_LOCK (self);
  self->max_keyframe_interval = interval;
  if (self->session != NULL) {
    gst_vtenc_session_configure_max_keyframe_interval (self, self->session,
        interval);
  }
  GST_OBJECT_UNLOCK (self);
}

static GstClockTime
gst_vtenc_get_max_keyframe_interval_duration (GstVTEnc * self)
{
  GstClockTime result;

  GST_OBJECT_LOCK (self);
  result = self->max_keyframe_interval_duration;
  GST_OBJECT_UNLOCK (self);

  return result;
}

static void
gst_vtenc_set_max_keyframe_interval_duration (GstVTEnc * self,
    GstClockTime interval)
{
  GST_OBJECT_LOCK (self);
  self->max_keyframe_interval_duration = interval;
  if (self->session != NULL) {
    gst_vtenc_session_configure_max_keyframe_interval_duration (self,
        self->session, interval / ((gdouble) GST_SECOND));
  }
  GST_OBJECT_UNLOCK (self);
}

static void
gst_vtenc_get_property (GObject * obj, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstVTEnc *self = GST_VTENC_CAST (obj);

  switch (prop_id) {
    case PROP_BITRATE:
      g_value_set_uint (value, gst_vtenc_get_bitrate (self) / 1000);
      break;
    case PROP_ALLOW_FRAME_REORDERING:
      g_value_set_boolean (value, gst_vtenc_get_allow_frame_reordering (self));
      break;
    case PROP_REALTIME:
      g_value_set_boolean (value, gst_vtenc_get_realtime (self));
      break;
    case PROP_QUALITY:
      g_value_set_double (value, gst_vtenc_get_quality (self));
      break;
    case PROP_MAX_KEYFRAME_INTERVAL:
      g_value_set_int (value, gst_vtenc_get_max_keyframe_interval (self));
      break;
    case PROP_MAX_KEYFRAME_INTERVAL_DURATION:
      g_value_set_uint64 (value,
          gst_vtenc_get_max_keyframe_interval_duration (self));
      break;
    case PROP_PRESERVE_ALPHA:
      g_value_set_boolean (value, self->preserve_alpha);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
      break;
  }
}

static void
gst_vtenc_set_property (GObject * obj, guint prop_id, const GValue * value,
    GParamSpec * pspec)
{
  GstVTEnc *self = GST_VTENC_CAST (obj);

  switch (prop_id) {
    case PROP_BITRATE:
      gst_vtenc_set_bitrate (self, g_value_get_uint (value) * 1000);
      break;
    case PROP_ALLOW_FRAME_REORDERING:
      gst_vtenc_set_allow_frame_reordering (self, g_value_get_boolean (value));
      break;
    case PROP_REALTIME:
      gst_vtenc_set_realtime (self, g_value_get_boolean (value));
      break;
    case PROP_QUALITY:
      gst_vtenc_set_quality (self, g_value_get_double (value));
      break;
    case PROP_MAX_KEYFRAME_INTERVAL:
      gst_vtenc_set_max_keyframe_interval (self, g_value_get_int (value));
      break;
    case PROP_MAX_KEYFRAME_INTERVAL_DURATION:
      gst_vtenc_set_max_keyframe_interval_duration (self,
          g_value_get_uint64 (value));
      break;
    case PROP_PRESERVE_ALPHA:
      self->preserve_alpha = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
      break;
  }
}

static gboolean
gst_vtenc_ensure_output_loop (GstVTEnc * self)
{
  GstPad *pad = GST_VIDEO_ENCODER_SRC_PAD (self);
  GstTask *task = GST_PAD_TASK (pad);

  return gst_task_resume (task);
}

static void
gst_vtenc_pause_output_loop (GstVTEnc * self)
{
  g_mutex_lock (&self->queue_mutex);
  self->pause_task = TRUE;
  g_cond_signal (&self->queue_cond);
  g_mutex_unlock (&self->queue_mutex);

  gst_pad_pause_task (GST_VIDEO_ENCODER_SRC_PAD (self));
  GST_DEBUG_OBJECT (self, "paused output thread");

  g_mutex_lock (&self->queue_mutex);
  self->pause_task = FALSE;
  g_mutex_unlock (&self->queue_mutex);
}

static void
gst_vtenc_set_flushing_flag (GstVTEnc * self)
{
  g_mutex_lock (&self->queue_mutex);
  self->is_flushing = TRUE;
  g_cond_signal (&self->queue_cond);
  g_mutex_unlock (&self->queue_mutex);
}

static GstFlowReturn
gst_vtenc_finish_encoding (GstVTEnc * self, gboolean is_flushing)
{
  GST_DEBUG_OBJECT (self,
      "complete encoding and clean buffer queue, is flushing %d", is_flushing);
  OSStatus vt_status;

  /* In case of EOS before the first buffer/caps */
  if (self->session == NULL)
    return GST_FLOW_OK;

  /* If output loop failed to push things downstream */
  if (self->downstream_ret != GST_FLOW_OK
      && self->downstream_ret != GST_FLOW_FLUSHING) {
    /* Tells enqueue_buffer() to instantly discard any new encoded frames */
    gst_vtenc_set_flushing_flag (self);
    GST_WARNING_OBJECT (self, "Output loop stopped with error (%s), leaving",
        gst_flow_get_name (self->downstream_ret));
    return self->downstream_ret;
  }

  if (is_flushing)
    gst_vtenc_set_flushing_flag (self);

  if (!gst_vtenc_ensure_output_loop (self)) {
    GST_ERROR_OBJECT (self, "Output loop failed to resume");
    return GST_FLOW_ERROR;
  }

  /* We need to unlock the stream lock here because
   * it can wait for gst_vtenc_enqueue_buffer() to
   * handle a buffer... which will take the stream
   * lock from another thread and then deadlock */
  GST_VIDEO_ENCODER_STREAM_UNLOCK (self);
  GST_DEBUG_OBJECT (self, "starting VTCompressionSessionCompleteFrames");
  vt_status =
      VTCompressionSessionCompleteFrames (self->session,
      kCMTimePositiveInfinity);
  GST_DEBUG_OBJECT (self, "VTCompressionSessionCompleteFrames ended");
  if (vt_status != noErr) {
    GST_WARNING_OBJECT (self, "VTCompressionSessionCompleteFrames returned %d",
        (int) vt_status);
  }

  gst_vtenc_pause_output_loop (self);
  GST_VIDEO_ENCODER_STREAM_LOCK (self);

  if (self->downstream_ret == GST_FLOW_OK)
    GST_DEBUG_OBJECT (self, "buffer queue cleaned");
  else
    GST_DEBUG_OBJECT (self,
        "buffer queue not cleaned, output thread returned %s",
        gst_flow_get_name (self->downstream_ret));

  return self->downstream_ret;
}

static gboolean
gst_vtenc_start (GstVideoEncoder * enc)
{
  GstVTEnc *self = GST_VTENC_CAST (enc);

  /* DTS can be negative if b-frames are enabled */
  gst_video_encoder_set_min_pts (enc, GST_SECOND * 60 * 60 * 1000);

  self->is_flushing = FALSE;
  self->downstream_ret = GST_FLOW_OK;

  self->output_queue = gst_queue_array_new (VTENC_OUTPUT_QUEUE_SIZE);
  /* Set clear_func to unref all remaining frames in gst_queue_array_free() */
  gst_queue_array_set_clear_func (self->output_queue,
      (GDestroyNotify) gst_video_codec_frame_unref);

  /* Create the output task, but pause it immediately */
  self->pause_task = TRUE;
  if (!gst_pad_start_task (GST_VIDEO_ENCODER_SRC_PAD (enc),
          (GstTaskFunction) gst_vtenc_loop, self, NULL)) {
    GST_ERROR_OBJECT (self, "failed to start output thread");
    return FALSE;
  }
  /* This blocks until the loop actually pauses */
  gst_pad_pause_task (GST_VIDEO_ENCODER_SRC_PAD (enc));
  self->pause_task = FALSE;

  return TRUE;
}

static gboolean
gst_vtenc_stop (GstVideoEncoder * enc)
{
  GstVTEnc *self = GST_VTENC_CAST (enc);

  GST_VIDEO_ENCODER_STREAM_LOCK (self);
  gst_vtenc_flush (enc);
  self->downstream_ret = GST_FLOW_FLUSHING;
  GST_VIDEO_ENCODER_STREAM_UNLOCK (self);

  gst_pad_stop_task (GST_VIDEO_ENCODER_SRC_PAD (enc));

  GST_OBJECT_LOCK (self);
  gst_vtenc_destroy_session (self, &self->session);
  GST_OBJECT_UNLOCK (self);

  self->negotiate_downstream = TRUE;
  self->is_flushing = TRUE;

  if (self->profile_level)
    CFRelease (self->profile_level);
  self->profile_level = NULL;

  if (self->input_state)
    gst_video_codec_state_unref (self->input_state);
  self->input_state = NULL;

  self->video_info.width = self->video_info.height = 0;
  self->video_info.fps_n = self->video_info.fps_d = 0;

  gst_queue_array_free (self->output_queue);
  self->output_queue = NULL;

  return TRUE;
}

static gboolean
gst_vtenc_h264_parse_profile_level_key (GstVTEnc * self, const gchar * profile,
    const gchar * level_arg)
{
  char level[64];
  gchar *key = NULL;

  if (profile == NULL)
    profile = "main";
  if (level_arg == NULL)
    level_arg = "AutoLevel";
  strncpy (level, level_arg, sizeof (level));

  if (!strcmp (profile, "constrained-baseline") ||
      !strcmp (profile, "baseline")) {
    profile = "Baseline";
    self->h264_profile = GST_H264_PROFILE_BASELINE;
  } else if (g_str_has_prefix (profile, "high")) {
    profile = "High";
    self->h264_profile = GST_H264_PROFILE_HIGH;
  } else if (!strcmp (profile, "main")) {
    profile = "Main";
    self->h264_profile = GST_H264_PROFILE_MAIN;
  } else {
    GST_ERROR_OBJECT (self, "invalid profile: %s", profile);
    return FALSE;
  }

  if (strlen (level) == 1) {
    level[1] = '_';
    level[2] = '0';
  } else if (strlen (level) == 3) {
    level[1] = '_';
  }

  key = g_strdup_printf ("H264_%s_%s", profile, level);
  self->profile_level =
      CFStringCreateWithBytes (NULL, (const guint8 *) key, strlen (key),
      kCFStringEncodingASCII, 0);
  GST_INFO_OBJECT (self, "negotiated profile and level %s", key);

  g_free (key);

  return TRUE;
}

static gboolean
gst_vtenc_hevc_parse_profile_level_key (GstVTEnc * self, const gchar * profile,
    const gchar * level_arg)
{
  gchar *key = NULL;

  if (profile == NULL || !strcmp (profile, "main"))
    profile = "Main";
  else if (!strcmp (profile, "main-10"))
    profile = "Main10";
  else if (!strcmp (profile, "main-422-10"))
    /* TODO: this should probably be guarded with a version check (macOS 12.3+ / iOS 15.4+)
     * https://developer.apple.com/documentation/videotoolbox/kvtprofilelevel_hevc_main10_autolevel */
    profile = "Main42210";
  else {
    GST_ERROR_OBJECT (self, "invalid profile: %s", profile);
    return FALSE;
  }

  /* VT does not support specific levels for HEVC */
  key = g_strdup_printf ("HEVC_%s_AutoLevel", profile);
  self->profile_level =
      CFStringCreateWithBytes (NULL, (const guint8 *) key, strlen (key),
      kCFStringEncodingASCII, 0);
  GST_INFO_OBJECT (self, "negotiated profile and level %s", key);

  g_free (key);
  return TRUE;
}

static gboolean
gst_vtenc_negotiate_profile_and_level (GstVTEnc * self, GstStructure * s)
{
  const gchar *profile = gst_structure_get_string (s, "profile");
  const gchar *level = gst_structure_get_string (s, "level");

  if (self->profile_level)
    CFRelease (self->profile_level);

  if (self->specific_format_id == kCMVideoCodecType_HEVC) {
    return gst_vtenc_hevc_parse_profile_level_key (self, profile, level);
  } else {
    return gst_vtenc_h264_parse_profile_level_key (self, profile, level);
  }
}

static gboolean
gst_vtenc_negotiate_prores_variant (GstVTEnc * self, GstStructure * s)
{
  const char *variant = gst_structure_get_string (s, "variant");
  CMVideoCodecType codec_type =
      gst_vtutil_codec_type_from_prores_variant (variant);

  if (codec_type == GST_kCMVideoCodecType_Some_AppleProRes) {
    GST_ERROR_OBJECT (self, "unsupported prores variant: %s", variant);
    return FALSE;
  }

  self->specific_format_id = codec_type;
  return TRUE;
}

static gboolean
gst_vtenc_negotiate_specific_format_details (GstVideoEncoder * enc)
{
  GstVTEnc *self = GST_VTENC_CAST (enc);
  GstCaps *allowed_caps = NULL;
  gboolean ret = TRUE;

  allowed_caps = gst_pad_get_allowed_caps (GST_VIDEO_ENCODER_SRC_PAD (enc));
  if (allowed_caps) {
    GstStructure *s;

    if (gst_caps_is_empty (allowed_caps)) {
      GST_ERROR_OBJECT (self, "no allowed downstream caps");
      goto fail;
    }

    allowed_caps = gst_caps_make_writable (allowed_caps);
    allowed_caps = gst_caps_fixate (allowed_caps);
    s = gst_caps_get_structure (allowed_caps, 0);
    switch (self->details->format_id) {
      case kCMVideoCodecType_H264:
        self->specific_format_id = kCMVideoCodecType_H264;
        if (!gst_vtenc_negotiate_profile_and_level (self, s))
          goto fail;
        break;
      case kCMVideoCodecType_HEVC:
        self->specific_format_id = kCMVideoCodecType_HEVC;
        if (!gst_vtenc_negotiate_profile_and_level (self, s))
          goto fail;
        break;
      case GST_kCMVideoCodecType_Some_AppleProRes:
        if (g_strcmp0 (self->details->mimetype, "video/x-prores") != 0) {
          GST_ERROR_OBJECT (self, "format_id == %i mimetype must be Apple "
              "ProRes", GST_kCMVideoCodecType_Some_AppleProRes);
          goto fail;
        }
        if (!gst_vtenc_negotiate_prores_variant (self, s))
          goto fail;
        break;
      default:
        g_assert_not_reached ();
    }
  }

out:
  if (allowed_caps)
    gst_caps_unref (allowed_caps);

  return ret;

fail:
  ret = FALSE;
  goto out;
}

static gboolean
gst_vtenc_set_format (GstVideoEncoder * enc, GstVideoCodecState * state)
{
  GstVTEnc *self = GST_VTENC_CAST (enc);
  VTCompressionSessionRef session;

  if (self->input_state) {
    gst_vtenc_finish_encoding (self, FALSE);
    gst_video_codec_state_unref (self->input_state);
  }

  GST_OBJECT_LOCK (self);
  gst_vtenc_destroy_session (self, &self->session);
  GST_OBJECT_UNLOCK (self);

  self->input_state = gst_video_codec_state_ref (state);
  self->video_info = state->info;

  if (!gst_vtenc_negotiate_specific_format_details (enc))
    return FALSE;

  self->negotiate_downstream = TRUE;

  session = gst_vtenc_create_session (self);
  GST_OBJECT_LOCK (self);
  self->session = session;
  GST_OBJECT_UNLOCK (self);

  return session != NULL;
}

static gboolean
gst_vtenc_is_negotiated (GstVTEnc * self)
{
  return self->session && self->video_info.width != 0;
}

/*
 * When the image is opaque but the output ProRes format has an alpha
 * component (4 component, 32 bits per pixel), Apple requires that we signal
 * that it should be ignored by setting the depth to 24 bits per pixel. Not
 * doing so causes the encoded files to fail validation.
 *
 * So we set that in the caps and qtmux sets the depth value in the container,
 * which will be read by demuxers so that decoders can skip those bytes
 * entirely. qtdemux does this, but vtdec does not use this information at
 * present.
 */
static gboolean
gst_vtenc_signal_ignored_alpha_component (GstVTEnc * self)
{
  if (self->preserve_alpha)
    return FALSE;
  if (self->specific_format_id == kCMVideoCodecType_AppleProRes4444XQ ||
      self->specific_format_id == kCMVideoCodecType_AppleProRes4444)
    return TRUE;
  return FALSE;
}

static gboolean
gst_vtenc_negotiate_downstream (GstVTEnc * self, CMSampleBufferRef sbuf)
{
  gboolean result;
  GstCaps *caps;
  GstStructure *s;
  GstVideoCodecState *state;

  caps = gst_pad_get_pad_template_caps (GST_VIDEO_ENCODER_SRC_PAD (self));
  caps = gst_caps_make_writable (caps);
  s = gst_caps_get_structure (caps, 0);
  gst_structure_set (s,
      "width", G_TYPE_INT, self->video_info.width,
      "height", G_TYPE_INT, self->video_info.height,
      "framerate", GST_TYPE_FRACTION,
      self->video_info.fps_n, self->video_info.fps_d, NULL);

  switch (self->details->format_id) {
    case kCMVideoCodecType_H264:
    case kCMVideoCodecType_HEVC:
    {
      CMFormatDescriptionRef fmt;
      CFDictionaryRef atoms;
      CFStringRef boxKey;
      CFDataRef box;
      guint8 *codec_data;
      gsize codec_data_size;
      GstBuffer *codec_data_buf;
      guint8 sps[12];

      fmt = CMSampleBufferGetFormatDescription (sbuf);
      atoms = CMFormatDescriptionGetExtension (fmt,
          kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms);

      if (self->details->format_id == kCMVideoCodecType_HEVC)
        boxKey =
            CFStringCreateWithCString (NULL, "hvcC", kCFStringEncodingUTF8);
      else
        boxKey =
            CFStringCreateWithCString (NULL, "avcC", kCFStringEncodingUTF8);

      box = CFDictionaryGetValue (atoms, boxKey);
      CFRelease (boxKey);
      codec_data_size = CFDataGetLength (box);
      codec_data = g_malloc (codec_data_size);
      CFDataGetBytes (box, CFRangeMake (0, codec_data_size), codec_data);
      codec_data_buf = gst_buffer_new_wrapped (codec_data, codec_data_size);

      gst_structure_set (s, "codec_data", GST_TYPE_BUFFER, codec_data_buf,
          NULL);

      if (self->details->format_id == kCMVideoCodecType_HEVC) {
        sps[0] = codec_data[1];
        sps[11] = codec_data[12];
        gst_codec_utils_h265_caps_set_level_tier_and_profile (caps, sps, 12);
      } else {
        sps[0] = codec_data[1];
        sps[1] = codec_data[2] & ~0xDF;
        sps[2] = codec_data[3];
        gst_codec_utils_h264_caps_set_level_and_profile (caps, sps, 3);
      }

      gst_buffer_unref (codec_data_buf);
    }
      break;
    case GST_kCMVideoCodecType_Some_AppleProRes:
      gst_structure_set (s, "variant", G_TYPE_STRING,
          gst_vtutil_codec_type_to_prores_variant (self->specific_format_id),
          NULL);
      if (gst_vtenc_signal_ignored_alpha_component (self))
        gst_structure_set (s, "depth", G_TYPE_INT, 24, NULL);
      break;
    default:
      g_assert_not_reached ();
  }

  state =
      gst_video_encoder_set_output_state (GST_VIDEO_ENCODER_CAST (self), caps,
      self->input_state);
  gst_video_codec_state_unref (state);
  result = gst_video_encoder_negotiate (GST_VIDEO_ENCODER_CAST (self));

  return result;
}

static GstFlowReturn
gst_vtenc_handle_frame (GstVideoEncoder * enc, GstVideoCodecFrame * frame)
{
  GstVTEnc *self = GST_VTENC_CAST (enc);

  if (!gst_vtenc_is_negotiated (self))
    goto not_negotiated;

  return gst_vtenc_encode_frame (self, frame);

not_negotiated:
  gst_video_codec_frame_unref (frame);
  return GST_FLOW_NOT_NEGOTIATED;
}

static gboolean
gst_vtenc_sink_event (GstVideoEncoder * enc, GstEvent * event)
{
  GstVTEnc *self = GST_VTENC_CAST (enc);
  GstEventType type = GST_EVENT_TYPE (event);
  gboolean ret;

  switch (type) {
    case GST_EVENT_FLUSH_START:
      GST_DEBUG_OBJECT (self, "flush start received, setting flushing flag");

      g_mutex_lock (&self->queue_mutex);
      self->is_flushing = TRUE;
      g_cond_signal (&self->queue_cond);
      g_mutex_unlock (&self->queue_mutex);
      break;
    default:
      break;
  }

  ret = GST_VIDEO_ENCODER_CLASS (parent_class)->sink_event (enc, event);

  switch (type) {
    case GST_EVENT_FLUSH_STOP:
      /* The base class handles this event and calls _flush().
       * We can then safely reset the flushing flag. */
      GST_DEBUG_OBJECT (self, "flush stop received, removing flushing flag");

      g_mutex_lock (&self->queue_mutex);
      self->is_flushing = FALSE;
      g_mutex_unlock (&self->queue_mutex);
      break;
    default:
      break;
  }

  return ret;
}

static GstStateChangeReturn
gst_vtenc_change_state (GstElement * element, GstStateChange transition)
{
  GstVTEnc *self = GST_VTENC_CAST (element);

  if (transition == GST_STATE_CHANGE_PAUSED_TO_READY) {
    GST_DEBUG_OBJECT (self, "pausing output loop on PAUSED->READY");
    gst_vtenc_pause_output_loop (self);
  }

  return GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
}

static GstFlowReturn
gst_vtenc_finish (GstVideoEncoder * enc)
{
  GstVTEnc *self = GST_VTENC_CAST (enc);
  return gst_vtenc_finish_encoding (self, FALSE);
}

static gboolean
gst_vtenc_flush (GstVideoEncoder * enc)
{
  GstVTEnc *self = GST_VTENC_CAST (enc);
  GstFlowReturn ret;

  ret = gst_vtenc_finish_encoding (self, TRUE);

  return (ret == GST_FLOW_OK);
}

static void
gst_vtenc_set_colorimetry (GstVTEnc * self, VTCompressionSessionRef session)
{
  OSStatus status;
  CFStringRef primaries = NULL, transfer = NULL, matrix = NULL;
  GstVideoColorimetry cm = GST_VIDEO_INFO_COLORIMETRY (&self->video_info);

  /*
   * https://developer.apple.com/documentation/corevideo/cvimagebuffer/image_buffer_ycbcr_matrix_constants
   */
  switch (cm.matrix) {
    case GST_VIDEO_COLOR_MATRIX_BT709:
      matrix = kCVImageBufferYCbCrMatrix_ITU_R_709_2;
      break;
    case GST_VIDEO_COLOR_MATRIX_BT601:
      matrix = kCVImageBufferYCbCrMatrix_ITU_R_601_4;
      break;
    case GST_VIDEO_COLOR_MATRIX_SMPTE240M:
      matrix = kCVImageBufferYCbCrMatrix_SMPTE_240M_1995;
      break;
    case GST_VIDEO_COLOR_MATRIX_BT2020:
      matrix = kCVImageBufferYCbCrMatrix_ITU_R_2020;
      break;
    default:
      GST_WARNING_OBJECT (self, "Unsupported color matrix %u", cm.matrix);
  }

  /*
   * https://developer.apple.com/documentation/corevideo/cvimagebuffer/image_buffer_transfer_function_constants
   */
  switch (cm.transfer) {
    case GST_VIDEO_TRANSFER_BT709:
    case GST_VIDEO_TRANSFER_BT601:
    case GST_VIDEO_TRANSFER_UNKNOWN:
      transfer = kCVImageBufferTransferFunction_ITU_R_709_2;
      break;
    case GST_VIDEO_TRANSFER_SMPTE240M:
      transfer = kCVImageBufferTransferFunction_SMPTE_240M_1995;
      break;
    case GST_VIDEO_TRANSFER_BT2020_12:
      transfer = kCVImageBufferTransferFunction_ITU_R_2020;
      break;
    case GST_VIDEO_TRANSFER_SRGB:
      if (__builtin_available (macOS 10.13, *))
        transfer = kCVImageBufferTransferFunction_sRGB;
      else
        GST_WARNING_OBJECT (self, "macOS version is too old, the sRGB transfer "
            "function is not available");
      break;
    case GST_VIDEO_TRANSFER_SMPTE2084:
      if (__builtin_available (macOS 10.13, *))
        transfer = kCVImageBufferTransferFunction_SMPTE_ST_2084_PQ;
      else
        GST_WARNING_OBJECT (self, "macOS version is too old, the SMPTE2084 "
            "transfer function is not available");
      break;
    default:
      GST_WARNING_OBJECT (self, "Unsupported color transfer %u", cm.transfer);
  }

  /*
   * https://developer.apple.com/documentation/corevideo/cvimagebuffer/image_buffer_color_primaries_constants
   */
  switch (cm.primaries) {
    case GST_VIDEO_COLOR_PRIMARIES_BT709:
      primaries = kCVImageBufferColorPrimaries_ITU_R_709_2;
      break;
    case GST_VIDEO_COLOR_PRIMARIES_SMPTE170M:
    case GST_VIDEO_COLOR_PRIMARIES_SMPTE240M:
      primaries = kCVImageBufferColorPrimaries_SMPTE_C;
      break;
    case GST_VIDEO_COLOR_PRIMARIES_BT2020:
      primaries = kCVImageBufferColorPrimaries_ITU_R_2020;
      break;
    case GST_VIDEO_COLOR_PRIMARIES_SMPTERP431:
      primaries = kCVImageBufferColorPrimaries_DCI_P3;
      break;
    case GST_VIDEO_COLOR_PRIMARIES_SMPTEEG432:
      primaries = kCVImageBufferColorPrimaries_P3_D65;
      break;
    case GST_VIDEO_COLOR_PRIMARIES_EBU3213:
      primaries = kCVImageBufferColorPrimaries_EBU_3213;
      break;
    default:
      GST_WARNING_OBJECT (self, "Unsupported color primaries %u", cm.primaries);
  }

  if (primaries) {
    status = VTSessionSetProperty (session,
        kVTCompressionPropertyKey_ColorPrimaries, primaries);
    GST_DEBUG_OBJECT (self, "kVTCompressionPropertyKey_ColorPrimaries =>"
        "%d", status);
  }

  if (transfer) {
    status = VTSessionSetProperty (session,
        kVTCompressionPropertyKey_TransferFunction, transfer);
    GST_DEBUG_OBJECT (self, "kVTCompressionPropertyKey_TransferFunction =>"
        "%d", status);
  }

  if (matrix) {
    status = VTSessionSetProperty (session,
        kVTCompressionPropertyKey_YCbCrMatrix, matrix);
    GST_DEBUG_OBJECT (self, "kVTCompressionPropertyKey_YCbCrMatrix => %d",
        status);
  }
}

static gboolean
gst_vtenc_compute_dts_offset (GstVTEnc * self, gint fps_n, gint fps_d)
{
  gint num_offset_frames;

  // kVTCompressionPropertyKey_AllowFrameReordering enables B-Frames
  if (!self->allow_frame_reordering ||
      (self->specific_format_id == kCMVideoCodecType_H264
          && self->h264_profile == GST_H264_PROFILE_BASELINE)) {
    num_offset_frames = 0;
  } else {
    if (self->specific_format_id == kCMVideoCodecType_H264) {
      // H264 encoder always sets 2 max_num_ref_frames
      num_offset_frames = 1;
    } else {
      // HEVC encoder uses B-pyramid
      num_offset_frames = 2;
    }
  }

  if (fps_d == 0 && num_offset_frames != 0) {
    GST_ERROR_OBJECT (self,
        "Variable framerate is not supported with B-Frames");
    return FALSE;
  }

  self->dts_offset =
      gst_util_uint64_scale (num_offset_frames * GST_SECOND,
      self->video_info.fps_d, self->video_info.fps_n);

  GST_DEBUG_OBJECT (self, "DTS Offset:%" GST_TIME_FORMAT,
      GST_TIME_ARGS (self->dts_offset));

  return TRUE;
}

static VTCompressionSessionRef
gst_vtenc_create_session (GstVTEnc * self)
{
  VTCompressionSessionRef session = NULL;
  CFMutableDictionaryRef encoder_spec = NULL, pb_attrs = NULL;
  OSStatus status;

#if !HAVE_IOS
  const GstVTEncoderDetails *codec_details =
      GST_VTENC_CLASS_GET_CODEC_DETAILS (G_OBJECT_GET_CLASS (self));

  /* Apple's M1 hardware encoding fails when provided with an interlaced ProRes source.
   * It's most likely a bug in VideoToolbox, as no such limitation has been officially mentioned anywhere.
   * For now let's disable HW encoding entirely when such case occurs. */
  gboolean enable_hw = !(GST_VIDEO_INFO_IS_INTERLACED (&self->video_info)
      && codec_details->format_id == GST_kCMVideoCodecType_Some_AppleProRes);

  if (!enable_hw)
    GST_WARNING_OBJECT (self,
        "Interlaced content detected, disabling HW-accelerated encoding due to https://gitlab.freedesktop.org/gstreamer/gstreamer/-/issues/1429");

  encoder_spec =
      CFDictionaryCreateMutable (NULL, 0, &kCFTypeDictionaryKeyCallBacks,
      &kCFTypeDictionaryValueCallBacks);
  gst_vtutil_dict_set_boolean (encoder_spec,
      kVTVideoEncoderSpecification_EnableHardwareAcceleratedVideoEncoder,
      enable_hw);
  if (codec_details->require_hardware)
    gst_vtutil_dict_set_boolean (encoder_spec,
        kVTVideoEncoderSpecification_RequireHardwareAcceleratedVideoEncoder,
        TRUE);
#endif

  if (self->profile_level) {
    pb_attrs = CFDictionaryCreateMutable (NULL, 0,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    gst_vtutil_dict_set_i32 (pb_attrs, kCVPixelBufferWidthKey,
        self->video_info.width);
    gst_vtutil_dict_set_i32 (pb_attrs, kCVPixelBufferHeightKey,
        self->video_info.height);
  }

  /* This was set in gst_vtenc_negotiate_specific_format_details() */
  g_assert_cmpint (self->specific_format_id, !=, 0);

  if (self->profile_level) {
    if (!gst_vtenc_compute_dts_offset (self, self->video_info.fps_d,
            self->video_info.fps_n)) {
      goto beach;
    }
  }

  status = VTCompressionSessionCreate (NULL,
      self->video_info.width, self->video_info.height,
      self->specific_format_id, encoder_spec, pb_attrs, NULL,
      gst_vtenc_enqueue_buffer, self, &session);
  GST_INFO_OBJECT (self, "VTCompressionSessionCreate for %d x %d => %d",
      self->video_info.width, self->video_info.height, (int) status);
  if (status != noErr) {
    GST_ERROR_OBJECT (self, "VTCompressionSessionCreate() returned: %d",
        (int) status);
    goto beach;
  }

  if (self->profile_level) {
    gst_vtenc_session_configure_expected_framerate (self, session,
        (gdouble) self->video_info.fps_n / (gdouble) self->video_info.fps_d);

    /*
     * https://developer.apple.com/documentation/videotoolbox/kvtcompressionpropertykey_profilelevel
     */
    status = VTSessionSetProperty (session,
        kVTCompressionPropertyKey_ProfileLevel, self->profile_level);
    GST_DEBUG_OBJECT (self, "kVTCompressionPropertyKey_ProfileLevel => %d",
        (int) status);

    status = VTSessionSetProperty (session,
        kVTCompressionPropertyKey_AllowTemporalCompression, kCFBooleanTrue);
    GST_DEBUG_OBJECT (self,
        "kVTCompressionPropertyKey_AllowTemporalCompression => %d",
        (int) status);

    gst_vtenc_session_configure_max_keyframe_interval (self, session,
        self->max_keyframe_interval);
    gst_vtenc_session_configure_max_keyframe_interval_duration (self, session,
        self->max_keyframe_interval_duration / ((gdouble) GST_SECOND));

    gst_vtenc_session_configure_bitrate (self, session,
        gst_vtenc_get_bitrate (self));
  }

  /* Force encoder to not preserve alpha with 4444(XQ) ProRes formats if
   * requested */
  if (!self->preserve_alpha &&
      (self->specific_format_id == kCMVideoCodecType_AppleProRes4444XQ ||
          self->specific_format_id == kCMVideoCodecType_AppleProRes4444)) {
    status = VTSessionSetProperty (session,
        gstVTCodecPropertyKey_PreserveAlphaChannel, CFSTR ("NO"));
    GST_DEBUG_OBJECT (self, "kVTCodecPropertyKey_PreserveAlphaChannel => %d",
        (int) status);
  }

  gst_vtenc_set_colorimetry (self, session);

  /* Interlacing */
  switch (GST_VIDEO_INFO_INTERLACE_MODE (&self->video_info)) {
    case GST_VIDEO_INTERLACE_MODE_PROGRESSIVE:
      gst_vtenc_session_configure_property_int (self, session,
          kVTCompressionPropertyKey_FieldCount, 1);
      break;
    case GST_VIDEO_INTERLACE_MODE_INTERLEAVED:
      gst_vtenc_session_configure_property_int (self, session,
          kVTCompressionPropertyKey_FieldCount, 2);
      switch (GST_VIDEO_INFO_FIELD_ORDER (&self->video_info)) {
        case GST_VIDEO_FIELD_ORDER_TOP_FIELD_FIRST:
          status = VTSessionSetProperty (session,
              kVTCompressionPropertyKey_FieldDetail,
              kCMFormatDescriptionFieldDetail_TemporalTopFirst);
          GST_DEBUG_OBJECT (self, "kVTCompressionPropertyKey_FieldDetail "
              "TemporalTopFirst => %d", (int) status);
          break;
        case GST_VIDEO_FIELD_ORDER_BOTTOM_FIELD_FIRST:
          status = VTSessionSetProperty (session,
              kVTCompressionPropertyKey_FieldDetail,
              kCMFormatDescriptionFieldDetail_TemporalBottomFirst);
          GST_DEBUG_OBJECT (self, "kVTCompressionPropertyKey_FieldDetail "
              "TemporalBottomFirst => %d", (int) status);
          break;
        case GST_VIDEO_FIELD_ORDER_UNKNOWN:
          GST_INFO_OBJECT (self, "Unknown field order for interleaved content, "
              "will check first buffer");
          self->have_field_order = FALSE;
      }
      break;
    default:
      /* Caps negotiation should prevent this */
      g_assert_not_reached ();
  }

  gst_vtenc_session_configure_realtime (self, session,
      gst_vtenc_get_realtime (self));
  gst_vtenc_session_configure_allow_frame_reordering (self, session,
      gst_vtenc_get_allow_frame_reordering (self));
  gst_vtenc_set_quality (self, self->quality);

  if (self->dump_properties) {
    gst_vtenc_session_dump_properties (self, session);
    self->dump_properties = FALSE;
  }
#ifdef HAVE_VIDEOTOOLBOX_10_9_6
  if (VTCompressionSessionPrepareToEncodeFrames) {
    status = VTCompressionSessionPrepareToEncodeFrames (session);
    if (status != noErr) {
      GST_ERROR_OBJECT (self,
          "VTCompressionSessionPrepareToEncodeFrames() returned: %d",
          (int) status);
    }
  }
#endif

beach:
  if (encoder_spec)
    CFRelease (encoder_spec);
  if (pb_attrs)
    CFRelease (pb_attrs);

  return session;
}

static void
gst_vtenc_destroy_session (GstVTEnc * self, VTCompressionSessionRef * session)
{
  VTCompressionSessionInvalidate (*session);
  if (*session != NULL) {
    CFRelease (*session);
    *session = NULL;
  }
}

typedef struct
{
  GstVTEnc *self;
  VTCompressionSessionRef session;
} GstVTDumpPropCtx;

static void
gst_vtenc_session_dump_property (CFStringRef prop_name,
    CFDictionaryRef prop_attrs, GstVTDumpPropCtx * dpc)
{
  gchar *name_str;
  CFTypeRef prop_value;
  OSStatus status;

  name_str = gst_vtutil_string_to_utf8 (prop_name);
  if (dpc->self->dump_attributes) {
    gchar *attrs_str;

    attrs_str = gst_vtutil_object_to_string (prop_attrs);
    GST_DEBUG_OBJECT (dpc->self, "%s = %s", name_str, attrs_str);
    g_free (attrs_str);
  }

  status = VTSessionCopyProperty (dpc->session, prop_name, NULL, &prop_value);
  if (status == noErr) {
    gchar *value_str;

    value_str = gst_vtutil_object_to_string (prop_value);
    GST_DEBUG_OBJECT (dpc->self, "%s = %s", name_str, value_str);
    g_free (value_str);

    if (prop_value != NULL)
      CFRelease (prop_value);
  } else {
    GST_DEBUG_OBJECT (dpc->self, "%s = <failed to query: %d>",
        name_str, (int) status);
  }

  g_free (name_str);
}

static void
gst_vtenc_session_dump_properties (GstVTEnc * self,
    VTCompressionSessionRef session)
{
  GstVTDumpPropCtx dpc = { self, session };
  CFDictionaryRef dict;
  OSStatus status;

  status = VTSessionCopySupportedPropertyDictionary (session, &dict);
  if (status != noErr)
    goto error;
  CFDictionaryApplyFunction (dict,
      (CFDictionaryApplierFunction) gst_vtenc_session_dump_property, &dpc);
  CFRelease (dict);

  return;

error:
  GST_WARNING_OBJECT (self, "failed to dump properties");
}

static void
gst_vtenc_session_configure_expected_framerate (GstVTEnc * self,
    VTCompressionSessionRef session, gdouble framerate)
{
  gst_vtenc_session_configure_property_double (self, session,
      kVTCompressionPropertyKey_ExpectedFrameRate, framerate);
}

static void
gst_vtenc_session_configure_max_keyframe_interval (GstVTEnc * self,
    VTCompressionSessionRef session, gint interval)
{
  gst_vtenc_session_configure_property_int (self, session,
      kVTCompressionPropertyKey_MaxKeyFrameInterval, interval);
}

static void
gst_vtenc_session_configure_max_keyframe_interval_duration (GstVTEnc * self,
    VTCompressionSessionRef session, gdouble duration)
{
  gst_vtenc_session_configure_property_double (self, session,
      kVTCompressionPropertyKey_MaxKeyFrameIntervalDuration, duration);
}

static void
gst_vtenc_session_configure_bitrate (GstVTEnc * self,
    VTCompressionSessionRef session, guint bitrate)
{
  gst_vtenc_session_configure_property_int (self, session,
      kVTCompressionPropertyKey_AverageBitRate, bitrate);
}

static void
gst_vtenc_session_configure_allow_frame_reordering (GstVTEnc * self,
    VTCompressionSessionRef session, gboolean allow_frame_reordering)
{
  VTSessionSetProperty (session, kVTCompressionPropertyKey_AllowFrameReordering,
      allow_frame_reordering ? kCFBooleanTrue : kCFBooleanFalse);
}

static void
gst_vtenc_session_configure_realtime (GstVTEnc * self,
    VTCompressionSessionRef session, gboolean realtime)
{
  VTSessionSetProperty (session, kVTCompressionPropertyKey_RealTime,
      realtime ? kCFBooleanTrue : kCFBooleanFalse);
}

static OSStatus
gst_vtenc_session_configure_property_int (GstVTEnc * self,
    VTCompressionSessionRef session, CFStringRef name, gint value)
{
  CFNumberRef num;
  OSStatus status;
  gchar name_str[128];

  num = CFNumberCreate (NULL, kCFNumberIntType, &value);
  status = VTSessionSetProperty (session, name, num);
  CFRelease (num);

  CFStringGetCString (name, name_str, sizeof (name_str), kCFStringEncodingUTF8);
  GST_DEBUG_OBJECT (self, "%s(%d) => %d", name_str, value, (int) status);

  return status;
}

static OSStatus
gst_vtenc_session_configure_property_double (GstVTEnc * self,
    VTCompressionSessionRef session, CFStringRef name, gdouble value)
{
  CFNumberRef num;
  OSStatus status;
  gchar name_str[128];

  num = CFNumberCreate (NULL, kCFNumberDoubleType, &value);
  status = VTSessionSetProperty (session, name, num);
  CFRelease (num);

  CFStringGetCString (name, name_str, sizeof (name_str), kCFStringEncodingUTF8);
  GST_DEBUG_OBJECT (self, "%s(%f) => %d", name_str, value, (int) status);

  return status;
}

static void
gst_vtenc_update_latency (GstVTEnc * self)
{
  OSStatus status;
  CFNumberRef value;
  int frames = 0;
  GstClockTime frame_duration;
  GstClockTime latency;

  if (self->video_info.fps_d == 0) {
    GST_INFO_OBJECT (self, "framerate not known, can't set latency");
    return;
  }

  status = VTSessionCopyProperty (self->session,
      kVTCompressionPropertyKey_NumberOfPendingFrames, NULL, &value);
  if (status != noErr || !value) {
    GST_INFO_OBJECT (self, "failed to get NumberOfPendingFrames: %d", status);
    return;
  }

  CFNumberGetValue (value, kCFNumberSInt32Type, &frames);
  if (MAX (self->latency_frames, frames) != self->latency_frames) {
    self->latency_frames = frames;
    if (self->video_info.fps_d == 0 || self->video_info.fps_n == 0) {
      /* FIXME: Assume 25fps. This is better than reporting no latency at
       * all and then later failing in live pipelines
       */
      frame_duration = gst_util_uint64_scale (GST_SECOND, 1, 25);
    } else {
      frame_duration = gst_util_uint64_scale (GST_SECOND,
          self->video_info.fps_d, self->video_info.fps_n);
    }
    latency = frame_duration * frames;
    GST_INFO_OBJECT (self,
        "latency status %d frames %d fps %d/%d time %" GST_TIME_FORMAT, status,
        frames, self->video_info.fps_n, self->video_info.fps_d,
        GST_TIME_ARGS (latency));
    gst_video_encoder_set_latency (GST_VIDEO_ENCODER (self), latency, latency);
  }
  CFRelease (value);
}

static void
gst_vtenc_update_timestamps (GstVTEnc * self, GstVideoCodecFrame * frame,
    CMSampleBufferRef sample_buf)
{
  CMTime pts = CMSampleBufferGetOutputPresentationTimeStamp (sample_buf);
  frame->pts = CMTIME_TO_GST_CLOCK_TIME (pts);
  CMTime dts = CMSampleBufferGetOutputDecodeTimeStamp (sample_buf);
  if (CMTIME_IS_VALID (dts)) {
    frame->dts = CMTIME_TO_GST_CLOCK_TIME (dts) - self->dts_offset;
  }
}

static GstFlowReturn
gst_vtenc_encode_frame (GstVTEnc * self, GstVideoCodecFrame * frame)
{
  CMTime ts, duration;
  GstCoreMediaMeta *meta;
  CVPixelBufferRef pbuf = NULL;
  OSStatus vt_status;
  GstFlowReturn ret = GST_FLOW_OK;
  CFDictionaryRef frame_props = NULL;
  GstTaskState task_state;
  gboolean is_flushing;

  /* If this condition changes later while we're still in this function,
   * it'll just fail on next frame encode or in _finish() */
  task_state = gst_pad_get_task_state (GST_VIDEO_ENCODER_SRC_PAD (self));
  if (task_state == GST_TASK_STOPPED || task_state == GST_TASK_PAUSED) {
    /* Abort if our loop failed to push frames downstream... */
    if (self->downstream_ret != GST_FLOW_OK) {
      if (self->downstream_ret == GST_FLOW_FLUSHING)
        GST_DEBUG_OBJECT (self,
            "Output loop stopped because of flushing, ignoring frame");
      else
        GST_WARNING_OBJECT (self,
            "Output loop stopped with error (%s), leaving",
            gst_flow_get_name (self->downstream_ret));

      ret = self->downstream_ret;
      goto drop;
    }

    /* ...or if it stopped because of the flushing flag while the queue
     * was empty, in which case we didn't get GST_FLOW_FLUSHING... */
    g_mutex_lock (&self->queue_mutex);
    is_flushing = self->is_flushing;
    g_mutex_unlock (&self->queue_mutex);
    if (is_flushing) {
      GST_DEBUG_OBJECT (self, "Flushing flag set, ignoring frame");
      ret = GST_FLOW_FLUSHING;
      goto drop;
    }

    /* .. or if it refuses to resume - e.g. it was stopped instead of paused */
    if (!gst_vtenc_ensure_output_loop (self)) {
      GST_ERROR_OBJECT (self, "Output loop failed to resume");
      ret = GST_FLOW_ERROR;
      goto drop;
    }
  }

  if (GST_VIDEO_CODEC_FRAME_IS_FORCE_KEYFRAME (frame)) {
    GST_INFO_OBJECT (self, "received force-keyframe-event, will force intra");
    frame_props = self->keyframe_props;
  }

  ts = CMTimeMake (frame->pts, GST_SECOND);
  if (frame->duration != GST_CLOCK_TIME_NONE)
    duration = CMTimeMake (frame->duration, GST_SECOND);
  else
    duration = kCMTimeInvalid;

  /* If we don't have field order, we need to pick it up from the first buffer
   * that has that information. The encoder session also cannot be reconfigured
   * with a new field detail after it has been set, so we encode mixed streams
   * with whatever the first buffer's field order is. */
  if (!self->have_field_order) {
    CFStringRef field_detail = NULL;

    if (GST_VIDEO_BUFFER_IS_TOP_FIELD (frame->input_buffer))
      field_detail = kCMFormatDescriptionFieldDetail_TemporalTopFirst;
    else if (GST_VIDEO_BUFFER_IS_BOTTOM_FIELD (frame->input_buffer))
      field_detail = kCMFormatDescriptionFieldDetail_TemporalBottomFirst;

    if (field_detail) {
      vt_status = VTSessionSetProperty (self->session,
          kVTCompressionPropertyKey_FieldDetail, field_detail);
      GST_DEBUG_OBJECT (self, "kVTCompressionPropertyKey_FieldDetail => %d",
          (int) vt_status);
    } else {
      GST_WARNING_OBJECT (self, "have interlaced content, but don't know field "
          "order yet, skipping buffer");
      gst_video_codec_frame_unref (frame);
      return GST_FLOW_OK;
    }

    self->have_field_order = TRUE;
  }

  meta = gst_buffer_get_core_media_meta (frame->input_buffer);
  if (meta != NULL) {
    pbuf = gst_core_media_buffer_get_pixel_buffer (frame->input_buffer);
  }
#ifdef HAVE_IOS
  if (pbuf == NULL) {
    GstVideoFrame inframe, outframe;
    GstBuffer *outbuf;
    CVReturn cv_ret;
    OSType pixel_format_type =
        gst_video_format_to_cvpixelformat (GST_VIDEO_INFO_FORMAT
        (&self->video_info));

    /* FIXME: iOS has special stride requirements that we don't know yet.
     * Copy into a newly allocated pixelbuffer for now. Probably makes
     * sense to create a buffer pool around these at some point.
     */
    if (!gst_video_frame_map (&inframe, &self->video_info, frame->input_buffer,
            GST_MAP_READ)) {
      GST_ERROR_OBJECT (self, "failed to map input buffer");
      goto cv_error;
    }

    cv_ret =
        CVPixelBufferCreate (NULL, self->video_info.width,
        self->video_info.height, pixel_format_type, NULL, &pbuf);

    if (cv_ret != kCVReturnSuccess) {
      GST_ERROR_OBJECT (self, "CVPixelBufferCreate failed: %i", cv_ret);
      gst_video_frame_unmap (&inframe);
      goto cv_error;
    }

    outbuf =
        gst_core_video_buffer_new ((CVBufferRef) pbuf, &self->video_info, NULL);
    if (!gst_video_frame_map (&outframe, &self->video_info, outbuf,
            GST_MAP_WRITE)) {
      GST_ERROR_OBJECT (self, "Failed to map output buffer");
      gst_video_frame_unmap (&inframe);
      gst_buffer_unref (outbuf);
      CVPixelBufferRelease (pbuf);
      goto cv_error;
    }

    if (!gst_video_frame_copy (&outframe, &inframe)) {
      GST_ERROR_OBJECT (self, "Failed to copy output frame");
      gst_video_frame_unmap (&inframe);
      gst_buffer_unref (outbuf);
      CVPixelBufferRelease (pbuf);
      goto cv_error;
    }

    gst_buffer_unref (outbuf);
    gst_video_frame_unmap (&inframe);
    gst_video_frame_unmap (&outframe);
  }
#else
  if (pbuf == NULL) {
    GstVTEncFrame *vframe;
    CVReturn cv_ret;

    vframe = gst_vtenc_frame_new (frame->input_buffer, &self->video_info);
    if (!vframe) {
      GST_ERROR_OBJECT (self, "Failed to create a new input frame");
      goto cv_error;
    }

    {
      OSType pixel_format_type =
          gst_video_format_to_cvpixelformat (GST_VIDEO_INFO_FORMAT
          (&self->video_info));
      const size_t num_planes = GST_VIDEO_FRAME_N_PLANES (&vframe->videoframe);
      void *plane_base_addresses[GST_VIDEO_MAX_PLANES];
      size_t plane_widths[GST_VIDEO_MAX_PLANES];
      size_t plane_heights[GST_VIDEO_MAX_PLANES];
      size_t plane_bytes_per_row[GST_VIDEO_MAX_PLANES];
      size_t i;

      for (i = 0; i < num_planes; i++) {
        plane_base_addresses[i] =
            GST_VIDEO_FRAME_PLANE_DATA (&vframe->videoframe, i);
        plane_widths[i] = GST_VIDEO_FRAME_COMP_WIDTH (&vframe->videoframe, i);
        plane_heights[i] = GST_VIDEO_FRAME_COMP_HEIGHT (&vframe->videoframe, i);
        plane_bytes_per_row[i] =
            GST_VIDEO_FRAME_COMP_STRIDE (&vframe->videoframe, i);
        plane_bytes_per_row[i] =
            GST_VIDEO_FRAME_COMP_STRIDE (&vframe->videoframe, i);
      }

      cv_ret = CVPixelBufferCreateWithPlanarBytes (NULL,
          self->video_info.width, self->video_info.height,
          pixel_format_type,
          frame,
          GST_VIDEO_FRAME_SIZE (&vframe->videoframe),
          num_planes,
          plane_base_addresses,
          plane_widths,
          plane_heights,
          plane_bytes_per_row, gst_pixel_buffer_release_cb, vframe, NULL,
          &pbuf);
      if (cv_ret != kCVReturnSuccess) {
        GST_ERROR_OBJECT (self, "CVPixelBufferCreateWithPlanarBytes failed: %i",
            cv_ret);
        gst_vtenc_frame_free (vframe);
        goto cv_error;
      }
    }
  }
#endif

  /* We need to unlock the stream lock here because
   * it can wait for gst_vtenc_enqueue_buffer() to
   * handle a buffer... which will take the stream
   * lock from another thread and then deadlock */
  GST_VIDEO_ENCODER_STREAM_UNLOCK (self);
  vt_status = VTCompressionSessionEncodeFrame (self->session,
      pbuf, ts, duration, frame_props,
      GINT_TO_POINTER (frame->system_frame_number), NULL);
  GST_VIDEO_ENCODER_STREAM_LOCK (self);

  if (vt_status != noErr) {
    GST_WARNING_OBJECT (self, "VTCompressionSessionEncodeFrame returned %d",
        (int) vt_status);
  }

  gst_video_codec_frame_unref (frame);
  CVPixelBufferRelease (pbuf);

  return ret;

drop:
  {
    gst_video_codec_frame_unref (frame);
    return ret;
  }

cv_error:
  {
    gst_video_codec_frame_unref (frame);
    return GST_FLOW_ERROR;
  }
}

static void
gst_vtenc_enqueue_buffer (void *outputCallbackRefCon,
    void *sourceFrameRefCon,
    OSStatus status,
    VTEncodeInfoFlags infoFlags, CMSampleBufferRef sampleBuffer)
{
  GstVTEnc *self = outputCallbackRefCon;
  GstVideoCodecFrame *frame;
  gboolean is_flushing;

  frame =
      gst_video_encoder_get_frame (GST_VIDEO_ENCODER_CAST (self),
      GPOINTER_TO_INT (sourceFrameRefCon));

  if (status != noErr) {
    if (frame) {
      GST_ELEMENT_ERROR (self, LIBRARY, ENCODE, (NULL),
          ("Failed to encode frame %d: %d", frame->system_frame_number,
              (int) status));
    } else {
      GST_ELEMENT_ERROR (self, LIBRARY, ENCODE, (NULL),
          ("Failed to encode (frame unknown): %d", (int) status));
    }
    goto beach;
  }

  if (!frame) {
    GST_WARNING_OBJECT (self, "No corresponding frame found!");
    goto beach;
  }

  g_mutex_lock (&self->queue_mutex);
  is_flushing = self->is_flushing;
  g_mutex_unlock (&self->queue_mutex);
  if (is_flushing) {
    GST_DEBUG_OBJECT (self, "Ignoring frame %d because we're flushing",
        frame->system_frame_number);
    goto beach;
  }

  /* This may happen if we don't have enough bitrate */
  if (sampleBuffer == NULL)
    goto beach;

  if (gst_vtenc_buffer_is_keyframe (self, sampleBuffer))
    GST_VIDEO_CODEC_FRAME_SET_SYNC_POINT (frame);

  /* We are dealing with block buffers here, so we don't need
   * to enable the use of the video meta API on the core media buffer */
  frame->output_buffer = gst_core_media_buffer_new (sampleBuffer, FALSE, NULL);

  gst_vtenc_update_timestamps (self, frame, sampleBuffer);

  /* Limit the amount of frames in our output queue
   * to avoid processing too many frames ahead */
  g_mutex_lock (&self->queue_mutex);
  while (gst_queue_array_get_length (self->output_queue) >
      VTENC_OUTPUT_QUEUE_SIZE) {
    g_cond_wait (&self->queue_cond, &self->queue_mutex);
  }
  g_mutex_unlock (&self->queue_mutex);

beach:
  if (!frame)
    return;

  g_mutex_lock (&self->queue_mutex);
  if (self->is_flushing) {
    /* We can discard the frame here, no need to have the output loop do that */
    gst_video_codec_frame_unref (frame);
    g_mutex_unlock (&self->queue_mutex);
    return;
  }

  /* Buffer-less frames will be discarded in the output loop */
  gst_queue_array_push_tail (self->output_queue, frame);
  g_cond_signal (&self->queue_cond);
  g_mutex_unlock (&self->queue_mutex);
}

static void
gst_vtenc_loop (GstVTEnc * self)
{
  GstVideoCodecFrame *outframe;
  GstCoreMediaMeta *meta;
  GstFlowReturn ret = GST_FLOW_OK;
  gboolean should_pause;

  g_mutex_lock (&self->queue_mutex);
  while (gst_queue_array_is_empty (self->output_queue) && !self->pause_task
      && !self->is_flushing) {
    g_cond_wait (&self->queue_cond, &self->queue_mutex);
  }

  if (self->pause_task) {
    g_mutex_unlock (&self->queue_mutex);
    gst_pad_pause_task (GST_VIDEO_ENCODER_CAST (self)->srcpad);
    return;
  }

  while ((outframe = gst_queue_array_pop_head (self->output_queue))) {
    g_cond_signal (&self->queue_cond);
    g_mutex_unlock (&self->queue_mutex);

    /* Keep the stream lock -> queue lock order */
    GST_VIDEO_ENCODER_STREAM_LOCK (self);

    g_mutex_lock (&self->queue_mutex);
    if (self->is_flushing) {
      GST_LOG_OBJECT (self, "flushing frame %d", outframe->system_frame_number);
      gst_video_codec_frame_unref (outframe);
      GST_VIDEO_ENCODER_STREAM_UNLOCK (self);
      continue;
    }
    g_mutex_unlock (&self->queue_mutex);

    if (self->negotiate_downstream &&
        (meta = gst_buffer_get_core_media_meta (outframe->output_buffer))) {
      if (!gst_vtenc_negotiate_downstream (self, meta->sample_buf)) {
        ret = GST_FLOW_NOT_NEGOTIATED;
        gst_video_codec_frame_unref (outframe);
        g_mutex_lock (&self->queue_mutex);
        /* the rest of the frames will be pop'd and unref'd later */
        break;
      }

      self->negotiate_downstream = FALSE;
    }

    gst_vtenc_update_latency (self);

    GST_LOG_OBJECT (self, "finishing frame %d", outframe->system_frame_number);
    GST_VIDEO_ENCODER_STREAM_UNLOCK (self);
    /* releases frame, even if it has no output buffer (i.e. failed to encode) */
    ret =
        gst_video_encoder_finish_frame (GST_VIDEO_ENCODER_CAST (self),
        outframe);
    g_mutex_lock (&self->queue_mutex);

    if (ret != GST_FLOW_OK)
      break;
  }

  g_mutex_unlock (&self->queue_mutex);
  GST_VIDEO_ENCODER_STREAM_LOCK (self);
  self->downstream_ret = ret;

  /* We need to empty the queue immediately so that enqueue_buffer() 
   * can push out the current buffer, otherwise it can block other
   * encoder callbacks completely */
  if (ret != GST_FLOW_OK) {
    g_mutex_lock (&self->queue_mutex);

    while ((outframe = gst_queue_array_pop_head (self->output_queue))) {
      GST_LOG_OBJECT (self, "flushing frame %d", outframe->system_frame_number);
      gst_video_codec_frame_unref (outframe);
    }

    g_cond_signal (&self->queue_cond);
    g_mutex_unlock (&self->queue_mutex);
  }

  GST_VIDEO_ENCODER_STREAM_UNLOCK (self);

  /* Check is_flushing here in case we had an empty queue.
   * In that scenario we also want to pause, as the encoder callback
   * will discard any frames that are output while flushing */
  g_mutex_lock (&self->queue_mutex);
  should_pause = ret != GST_FLOW_OK || self->is_flushing;
  g_mutex_unlock (&self->queue_mutex);
  if (should_pause) {
    GST_DEBUG_OBJECT (self, "pausing output task: %s",
        ret != GST_FLOW_OK ? gst_flow_get_name (ret) : "flushing");
    gst_pad_pause_task (GST_VIDEO_ENCODER_CAST (self)->srcpad);
  }
}

static gboolean
gst_vtenc_buffer_is_keyframe (GstVTEnc * self, CMSampleBufferRef sbuf)
{
  gboolean result = FALSE;
  CFArrayRef attachments_for_sample;

  attachments_for_sample = CMSampleBufferGetSampleAttachmentsArray (sbuf, 0);
  if (attachments_for_sample != NULL) {
    CFDictionaryRef attachments;
    CFBooleanRef depends_on_others;

    attachments = CFArrayGetValueAtIndex (attachments_for_sample, 0);
    depends_on_others = CFDictionaryGetValue (attachments,
        kCMSampleAttachmentKey_DependsOnOthers);
    result = (depends_on_others == kCFBooleanFalse);
  }

  return result;
}

#ifndef HAVE_IOS
static GstVTEncFrame *
gst_vtenc_frame_new (GstBuffer * buf, GstVideoInfo * video_info)
{
  GstVTEncFrame *frame;

  frame = g_slice_new (GstVTEncFrame);
  frame->buf = gst_buffer_ref (buf);
  if (!gst_video_frame_map (&frame->videoframe, video_info, buf, GST_MAP_READ)) {
    gst_buffer_unref (frame->buf);
    g_slice_free (GstVTEncFrame, frame);
    return NULL;
  }

  return frame;
}

static void
gst_vtenc_frame_free (GstVTEncFrame * frame)
{
  gst_video_frame_unmap (&frame->videoframe);
  gst_buffer_unref (frame->buf);
  g_slice_free (GstVTEncFrame, frame);
}

static void
gst_pixel_buffer_release_cb (void *releaseRefCon, const void *dataPtr,
    size_t dataSize, size_t numberOfPlanes, const void *planeAddresses[])
{
  GstVTEncFrame *frame = (GstVTEncFrame *) releaseRefCon;
  gst_vtenc_frame_free (frame);
}
#endif

static void
gst_vtenc_register (GstPlugin * plugin,
    const GstVTEncoderDetails * codec_details)
{
  GTypeInfo type_info = {
    sizeof (GstVTEncClass),
    (GBaseInitFunc) gst_vtenc_base_init,
    NULL,
    (GClassInitFunc) gst_vtenc_class_init,
    NULL,
    NULL,
    sizeof (GstVTEnc),
    0,
    (GInstanceInitFunc) gst_vtenc_init,
  };
  gchar *type_name;
  GType type;
  gboolean result;

  type_name = g_strdup_printf ("vtenc_%s", codec_details->element_name);

  type =
      g_type_register_static (GST_TYPE_VIDEO_ENCODER, type_name, &type_info, 0);

  g_type_set_qdata (type, GST_VTENC_CODEC_DETAILS_QDATA,
      (gpointer) codec_details);

  result = gst_element_register (plugin, type_name, GST_RANK_PRIMARY, type);
  if (!result) {
    GST_ERROR_OBJECT (plugin, "failed to register element %s", type_name);
  }

  g_free (type_name);
}

static const GstVTEncoderDetails gst_vtenc_codecs[] = {
  {"H.264", "h264", "video/x-h264", kCMVideoCodecType_H264, FALSE},
  {"H.265/HEVC", "h265", "video/x-h265", kCMVideoCodecType_HEVC, FALSE},
#ifndef HAVE_IOS
  {"H.264 (HW only)", "h264_hw", "video/x-h264", kCMVideoCodecType_H264, TRUE},
  {"H.265/HEVC (HW only)", "h265_hw", "video/x-h265", kCMVideoCodecType_HEVC,
      TRUE},
#endif
  {"Apple ProRes", "prores", "video/x-prores",
      GST_kCMVideoCodecType_Some_AppleProRes, FALSE},
};

void
gst_vtenc_register_elements (GstPlugin * plugin)
{
  guint i;

  GST_DEBUG_CATEGORY_INIT (gst_vtenc_debug, "vtenc",
      0, "Apple VideoToolbox Encoder Wrapper");

  for (i = 0; i != G_N_ELEMENTS (gst_vtenc_codecs); i++)
    gst_vtenc_register (plugin, &gst_vtenc_codecs[i]);
}
