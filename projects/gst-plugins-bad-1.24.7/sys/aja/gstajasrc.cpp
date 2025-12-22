/* GStreamer
 * Copyright (C) 2021 Sebastian Dröge <sebastian@centricular.com>
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
 * Free Software Foundation, Inc., 51 Franklin Street, Suite 500,
 * Boston, MA 02110-1335, USA.
 */

/**
 * SECTION:element-ajasrc
 *
 * Source element for [AJA](https://www.aja.com) capture cards.
 *
 * ## Example usage
 *
 * Capture 1080p30 audio/video and display it locally
 *
 * ```sh
 * gst-launch-1.0 ajasrc video-format=1080p-3000 ! ajasrcdemux name=d \
 *     d.video ! queue max-size-bytes=0 max-size-buffers=0 max-size-time=1000000000 ! videoconvert ! autovideosink \
 *     d.audio ! queue max-size-bytes=0 max-size-buffers=0 max-size-time=1000000000 ! audioconvert ! audioresample ! autoaudiosink
 * ```
 *
 * Capture 1080p30 audio/video and directly output it again on the same card
 *
 * ```sh
 * gst-launch-1.0 ajasrc video-format=1080p-3000 channel=1 input-source=sdi-1 audio-system=2 ! ajasrcdemux name=d \
 *     d.video ! queue max-size-bytes=0 max-size-buffers=0 max-size-time=1000000000 ! c.video \
 *     d.audio ! queue max-size-bytes=0 max-size-buffers=0 max-size-time=1000000000 ! c.audio \
 *     ajasinkcombiner name=c ! ajasink channel=0 reference-source=input-1
 * ```
 *
 * Since: 1.24
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ajaanc/includes/ancillarydata_cea608_vanc.h>
#include <ajaanc/includes/ancillarydata_cea708.h>
#include <ajaanc/includes/ancillarylist.h>
#include <ajantv2/includes/ntv2rp188.h>
#include <ajantv2/includes/ntv2vpid.h>

#include "gstajacommon.h"
#include "gstajasrc.h"

GST_DEBUG_CATEGORY_STATIC(gst_aja_src_debug);
#define GST_CAT_DEFAULT gst_aja_src_debug

#define DEFAULT_DEVICE_IDENTIFIER ("0")
#define DEFAULT_CHANNEL (::NTV2_CHANNEL1)
#define DEFAULT_VIDEO_FORMAT (GST_AJA_VIDEO_FORMAT_AUTO)
#define DEFAULT_AUDIO_SYSTEM (GST_AJA_AUDIO_SYSTEM_AUTO)
#define DEFAULT_INPUT_SOURCE (GST_AJA_INPUT_SOURCE_AUTO)
#define DEFAULT_SDI_MODE (GST_AJA_SDI_MODE_SINGLE_LINK)
#define DEFAULT_AUDIO_SOURCE (GST_AJA_AUDIO_SOURCE_EMBEDDED)
#define DEFAULT_EMBEDDED_AUDIO_INPUT (GST_AJA_EMBEDDED_AUDIO_INPUT_AUTO)
#define DEFAULT_TIMECODE_INDEX (GST_AJA_TIMECODE_INDEX_VITC)
#define DEFAULT_RP188 (TRUE)
#define DEFAULT_REFERENCE_SOURCE (GST_AJA_REFERENCE_SOURCE_FREERUN)
#define DEFAULT_CLOSED_CAPTION_CAPTURE_MODE \
  (GST_AJA_CLOSED_CAPTION_CAPTURE_MODE_CEA708_AND_CEA608)
#define DEFAULT_QUEUE_SIZE (16)
#define DEFAULT_START_FRAME (8)
#define DEFAULT_END_FRAME (8)
#define DEFAULT_CAPTURE_CPU_CORE (G_MAXUINT)
#define DEFAULT_ATTACH_ANCILLARY_META (FALSE)

enum {
  PROP_0,
  PROP_DEVICE_IDENTIFIER,
  PROP_CHANNEL,
  PROP_VIDEO_FORMAT,
  PROP_AUDIO_SYSTEM,
  PROP_INPUT_SOURCE,
  PROP_SDI_MODE,
  PROP_AUDIO_SOURCE,
  PROP_EMBEDDED_AUDIO_INPUT,
  PROP_TIMECODE_INDEX,
  PROP_RP188,
  PROP_REFERENCE_SOURCE,
  PROP_CLOSED_CAPTION_CAPTURE_MODE,
  PROP_START_FRAME,
  PROP_END_FRAME,
  PROP_QUEUE_SIZE,
  PROP_CAPTURE_CPU_CORE,
  PROP_SIGNAL,
  PROP_ATTACH_ANCILLARY_META,
};

// Make these plain C structs for usage in GstQueueArray
G_BEGIN_DECLS

typedef enum {
  QUEUE_ITEM_TYPE_DUMMY,
  QUEUE_ITEM_TYPE_FRAME,
  QUEUE_ITEM_TYPE_SIGNAL_CHANGE,
  QUEUE_ITEM_TYPE_ERROR,
  QUEUE_ITEM_TYPE_FRAMES_DROPPED,
} QueueItemType;

typedef struct {
  QueueItemType type;

  union {
    // For DUMMY
    struct {
      gchar dummy;
    } dummy;
    // For FRAME
    struct {
      GstClockTime capture_time;
      GstBuffer *video_buffer;
      GstBuffer *audio_buffer;
      GstBuffer *anc_buffer, *anc_buffer2;
      NTV2_RP188 tc;

      NTV2VideoFormat detected_format;
      guint32 vpid;
    } frame;
    // For SIGNAL_CHANGE
    struct {
      gboolean have_signal;
      NTV2VideoFormat detected_format;
      guint32 vpid;
    } signal_change;
    // For ERROR
    struct {
      GstMessage *msg;
    } error;
    // For FRAMES_DROPPED
    struct {
      gboolean driver_side;
      GstClockTime timestamp_start, timestamp_end;
    } frames_dropped;
  };
} QueueItem;

G_END_DECLS

static void queue_item_clear(QueueItem *item) {
  switch (item->type) {
    case QUEUE_ITEM_TYPE_DUMMY:
      break;
    case QUEUE_ITEM_TYPE_FRAME:
      gst_clear_buffer(&item->frame.video_buffer);
      gst_clear_buffer(&item->frame.audio_buffer);
      gst_clear_buffer(&item->frame.anc_buffer);
      gst_clear_buffer(&item->frame.anc_buffer2);
      item->frame.tc.~NTV2_RP188();
      break;
    case QUEUE_ITEM_TYPE_SIGNAL_CHANGE:
      break;
    case QUEUE_ITEM_TYPE_ERROR:
      gst_clear_message(&item->error.msg);
      break;
    case QUEUE_ITEM_TYPE_FRAMES_DROPPED:
      break;
  }

  item->type = QUEUE_ITEM_TYPE_DUMMY;
}

static void gst_aja_src_set_property(GObject *object, guint property_id,
                                     const GValue *value, GParamSpec *pspec);
static void gst_aja_src_get_property(GObject *object, guint property_id,
                                     GValue *value, GParamSpec *pspec);
static void gst_aja_src_finalize(GObject *object);

static GstCaps *gst_aja_src_get_caps(GstBaseSrc *bsrc, GstCaps *filter);
static gboolean gst_aja_src_query(GstBaseSrc *bsrc, GstQuery *query);
static gboolean gst_aja_src_unlock(GstBaseSrc *bsrc);
static gboolean gst_aja_src_unlock_stop(GstBaseSrc *bsrc);

static GstFlowReturn gst_aja_src_create(GstPushSrc *psrc, GstBuffer **buffer);

static gboolean gst_aja_src_open(GstAjaSrc *src);
static gboolean gst_aja_src_close(GstAjaSrc *src);
static gboolean gst_aja_src_stop(GstAjaSrc *src);

static GstStateChangeReturn gst_aja_src_change_state(GstElement *element,
                                                     GstStateChange transition);

static void capture_thread_func(AJAThread *thread, void *data);

#define parent_class gst_aja_src_parent_class
G_DEFINE_TYPE(GstAjaSrc, gst_aja_src, GST_TYPE_PUSH_SRC);

static void gst_aja_src_class_init(GstAjaSrcClass *klass) {
  GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS(klass);
  GstBaseSrcClass *basesrc_class = GST_BASE_SRC_CLASS(klass);
  GstPushSrcClass *pushsrc_class = GST_PUSH_SRC_CLASS(klass);
  GstCaps *templ_caps;

  gobject_class->set_property = gst_aja_src_set_property;
  gobject_class->get_property = gst_aja_src_get_property;
  gobject_class->finalize = gst_aja_src_finalize;

  g_object_class_install_property(
      gobject_class, PROP_DEVICE_IDENTIFIER,
      g_param_spec_string(
          "device-identifier", "Device identifier",
          "Input device instance to use", DEFAULT_DEVICE_IDENTIFIER,
          (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
                        G_PARAM_CONSTRUCT)));

  g_object_class_install_property(
      gobject_class, PROP_CHANNEL,
      g_param_spec_uint(
          "channel", "Channel", "Channel to use", 0, NTV2_MAX_NUM_CHANNELS - 1,
          DEFAULT_CHANNEL,
          (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
                        G_PARAM_CONSTRUCT)));

  g_object_class_install_property(
      gobject_class, PROP_VIDEO_FORMAT,
      g_param_spec_enum(
          "video-format", "Video Format", "Video format to use",
          GST_TYPE_AJA_VIDEO_FORMAT, DEFAULT_VIDEO_FORMAT,
          (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
                        G_PARAM_CONSTRUCT)));

  g_object_class_install_property(
      gobject_class, PROP_QUEUE_SIZE,
      g_param_spec_uint(
          "queue-size", "Queue Size",
          "Size of internal queue in number of video frames. "
          "Half of this is allocated as device buffers and equal to the "
          "latency.",
          1, G_MAXINT, DEFAULT_QUEUE_SIZE,
          (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property(
      gobject_class, PROP_START_FRAME,
      g_param_spec_uint(
          "start-frame", "Start Frame",
          "Start frame buffer to be used for capturing (automatically assign "
          "that many frames if same number as end-frame).",
          0, G_MAXINT, DEFAULT_START_FRAME,
          (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property(
      gobject_class, PROP_END_FRAME,
      g_param_spec_uint(
          "end-frame", "End Frame",
          "End frame buffer to be used for capturing (automatically assign "
          "that many frames if same number as start-frame).",
          0, G_MAXINT, DEFAULT_END_FRAME,
          (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property(
      gobject_class, PROP_AUDIO_SYSTEM,
      g_param_spec_enum(
          "audio-system", "Audio System", "Audio system to use",
          GST_TYPE_AJA_AUDIO_SYSTEM, DEFAULT_AUDIO_SYSTEM,
          (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
                        G_PARAM_CONSTRUCT)));

  g_object_class_install_property(
      gobject_class, PROP_INPUT_SOURCE,
      g_param_spec_enum(
          "input-source", "Input Source", "Input source to use",
          GST_TYPE_AJA_INPUT_SOURCE, DEFAULT_INPUT_SOURCE,
          (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
                        G_PARAM_CONSTRUCT)));

  g_object_class_install_property(
      gobject_class, PROP_SDI_MODE,
      g_param_spec_enum(
          "sdi-input-mode", "SDI Input Mode", "SDI input mode to use",
          GST_TYPE_AJA_SDI_MODE, DEFAULT_SDI_MODE,
          (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
                        G_PARAM_CONSTRUCT)));

  g_object_class_install_property(
      gobject_class, PROP_AUDIO_SOURCE,
      g_param_spec_enum(
          "audio-source", "Audio Source", "Audio source to use",
          GST_TYPE_AJA_AUDIO_SOURCE, DEFAULT_AUDIO_SOURCE,
          (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
                        G_PARAM_CONSTRUCT)));

  g_object_class_install_property(
      gobject_class, PROP_EMBEDDED_AUDIO_INPUT,
      g_param_spec_enum(
          "embedded-audio-input", "Embedded Audio Input",
          "Embedded Audio Input to use", GST_TYPE_AJA_EMBEDDED_AUDIO_INPUT,
          DEFAULT_EMBEDDED_AUDIO_INPUT,
          (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
                        G_PARAM_CONSTRUCT)));

  g_object_class_install_property(
      gobject_class, PROP_TIMECODE_INDEX,
      g_param_spec_enum(
          "timecode-index", "Timecode Index", "Timecode index to use",
          GST_TYPE_AJA_TIMECODE_INDEX, DEFAULT_TIMECODE_INDEX,
          (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
                        G_PARAM_CONSTRUCT)));

  g_object_class_install_property(
      gobject_class, PROP_RP188,
      g_param_spec_boolean(
          "rp188", "RP188", "Enable RP188 timecode retrieval", DEFAULT_RP188,
          (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
                        G_PARAM_CONSTRUCT)));

  g_object_class_install_property(
      gobject_class, PROP_REFERENCE_SOURCE,
      g_param_spec_enum(
          "reference-source", "Reference Source", "Reference source to use",
          GST_TYPE_AJA_REFERENCE_SOURCE, DEFAULT_REFERENCE_SOURCE,
          (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
                        G_PARAM_CONSTRUCT)));

  g_object_class_install_property(
      gobject_class, PROP_CLOSED_CAPTION_CAPTURE_MODE,
      g_param_spec_enum(
          "closed-caption-capture-mode", "Closed Caption Capture Mode",
          "Closed Caption Capture Mode",
          GST_TYPE_AJA_CLOSED_CAPTION_CAPTURE_MODE,
          DEFAULT_CLOSED_CAPTION_CAPTURE_MODE,
          (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
                        G_PARAM_CONSTRUCT)));

  g_object_class_install_property(
      gobject_class, PROP_CAPTURE_CPU_CORE,
      g_param_spec_uint(
          "capture-cpu-core", "Capture CPU Core",
          "Sets the affinity of the capture thread to this CPU core "
          "(-1=disabled)",
          0, G_MAXUINT, DEFAULT_CAPTURE_CPU_CORE,
          (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
                        G_PARAM_CONSTRUCT)));

  g_object_class_install_property(
      gobject_class, PROP_SIGNAL,
      g_param_spec_boolean(
          "signal", "Input signal available",
          "True if there is a valid input signal available", FALSE,
          (GParamFlags)(G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)));

  /**
   * GstAjaSrc:attach-ancillary-meta:
   *
   * If set to %TRUE attach any ancillary data as #GstAncillaryMeta on buffers
   *
   * Since: 1.24
   */
  g_object_class_install_property(
      gobject_class, PROP_ATTACH_ANCILLARY_META,
      g_param_spec_boolean(
          "attach-ancillary-meta", "Attach Ancillary Meta",
          "Attach ancillary meta to video frames",
          DEFAULT_ATTACH_ANCILLARY_META,
          (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
                        G_PARAM_CONSTRUCT)));

  element_class->change_state = GST_DEBUG_FUNCPTR(gst_aja_src_change_state);

  basesrc_class->get_caps = GST_DEBUG_FUNCPTR(gst_aja_src_get_caps);
  basesrc_class->negotiate = NULL;
  basesrc_class->query = GST_DEBUG_FUNCPTR(gst_aja_src_query);
  basesrc_class->unlock = GST_DEBUG_FUNCPTR(gst_aja_src_unlock);
  basesrc_class->unlock_stop = GST_DEBUG_FUNCPTR(gst_aja_src_unlock_stop);

  pushsrc_class->create = GST_DEBUG_FUNCPTR(gst_aja_src_create);

  templ_caps = gst_ntv2_supported_caps(DEVICE_ID_INVALID);
  gst_element_class_add_pad_template(
      element_class,
      gst_pad_template_new("src", GST_PAD_SRC, GST_PAD_ALWAYS, templ_caps));
  gst_caps_unref(templ_caps);

  gst_element_class_set_static_metadata(
      element_class, "AJA audio/video src", "Audio/Video/Source",
      "Captures audio/video frames with AJA devices",
      "Sebastian Dröge <sebastian@centricular.com>");

  GST_DEBUG_CATEGORY_INIT(gst_aja_src_debug, "ajasrc", 0, "AJA src");
}

static void gst_aja_src_init(GstAjaSrc *self) {
  g_mutex_init(&self->queue_lock);
  g_cond_init(&self->queue_cond);

  self->device_identifier = g_strdup(DEFAULT_DEVICE_IDENTIFIER);
  self->channel = DEFAULT_CHANNEL;
  self->queue_size = DEFAULT_QUEUE_SIZE;
  self->start_frame = DEFAULT_START_FRAME;
  self->end_frame = DEFAULT_END_FRAME;
  self->video_format_setting = DEFAULT_VIDEO_FORMAT;
  self->audio_system_setting = DEFAULT_AUDIO_SYSTEM;
  self->input_source = DEFAULT_INPUT_SOURCE;
  self->audio_source = DEFAULT_AUDIO_SOURCE;
  self->embedded_audio_input = DEFAULT_EMBEDDED_AUDIO_INPUT;
  self->timecode_index = DEFAULT_TIMECODE_INDEX;
  self->reference_source = DEFAULT_REFERENCE_SOURCE;
  self->closed_caption_capture_mode = DEFAULT_CLOSED_CAPTION_CAPTURE_MODE;
  self->capture_cpu_core = DEFAULT_CAPTURE_CPU_CORE;
  self->attach_ancillary_meta = DEFAULT_ATTACH_ANCILLARY_META;

  self->queue =
      gst_queue_array_new_for_struct(sizeof(QueueItem), self->queue_size);
  gst_base_src_set_live(GST_BASE_SRC_CAST(self), TRUE);
  gst_base_src_set_format(GST_BASE_SRC_CAST(self), GST_FORMAT_TIME);

  self->video_format = NTV2_FORMAT_UNKNOWN;
}

void gst_aja_src_set_property(GObject *object, guint property_id,
                              const GValue *value, GParamSpec *pspec) {
  GstAjaSrc *self = GST_AJA_SRC(object);

  switch (property_id) {
    case PROP_DEVICE_IDENTIFIER:
      g_free(self->device_identifier);
      self->device_identifier = g_value_dup_string(value);
      break;
    case PROP_CHANNEL:
      self->channel = (NTV2Channel)g_value_get_uint(value);
      break;
    case PROP_QUEUE_SIZE:
      self->queue_size = g_value_get_uint(value);
      break;
    case PROP_START_FRAME:
      self->start_frame = g_value_get_uint(value);
      break;
    case PROP_END_FRAME:
      self->end_frame = g_value_get_uint(value);
      break;
    case PROP_VIDEO_FORMAT:
      self->video_format_setting = (GstAjaVideoFormat)g_value_get_enum(value);
      break;
    case PROP_AUDIO_SYSTEM:
      self->audio_system_setting = (GstAjaAudioSystem)g_value_get_enum(value);
      break;
    case PROP_INPUT_SOURCE:
      self->input_source = (GstAjaInputSource)g_value_get_enum(value);
      break;
    case PROP_SDI_MODE:
      self->sdi_mode = (GstAjaSdiMode)g_value_get_enum(value);
      break;
    case PROP_AUDIO_SOURCE:
      self->audio_source = (GstAjaAudioSource)g_value_get_enum(value);
      break;
    case PROP_EMBEDDED_AUDIO_INPUT:
      self->embedded_audio_input =
          (GstAjaEmbeddedAudioInput)g_value_get_enum(value);
      break;
    case PROP_TIMECODE_INDEX:
      self->timecode_index = (GstAjaTimecodeIndex)g_value_get_enum(value);
      break;
    case PROP_RP188:
      self->rp188 = g_value_get_boolean(value);
      break;
    case PROP_REFERENCE_SOURCE:
      self->reference_source = (GstAjaReferenceSource)g_value_get_enum(value);
      break;
    case PROP_CLOSED_CAPTION_CAPTURE_MODE:
      self->closed_caption_capture_mode =
          (GstAjaClosedCaptionCaptureMode)g_value_get_enum(value);
      break;
    case PROP_CAPTURE_CPU_CORE:
      self->capture_cpu_core = g_value_get_uint(value);
      break;
    case PROP_ATTACH_ANCILLARY_META:
      self->attach_ancillary_meta = g_value_get_boolean(value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
      break;
  }
}

void gst_aja_src_get_property(GObject *object, guint property_id, GValue *value,
                              GParamSpec *pspec) {
  GstAjaSrc *self = GST_AJA_SRC(object);

  switch (property_id) {
    case PROP_DEVICE_IDENTIFIER:
      g_value_set_string(value, self->device_identifier);
      break;
    case PROP_CHANNEL:
      g_value_set_uint(value, self->channel);
      break;
    case PROP_QUEUE_SIZE:
      g_value_set_uint(value, self->queue_size);
      break;
    case PROP_START_FRAME:
      g_value_set_uint(value, self->start_frame);
      break;
    case PROP_END_FRAME:
      g_value_set_uint(value, self->end_frame);
      break;
    case PROP_VIDEO_FORMAT:
      g_value_set_enum(value, self->video_format_setting);
      break;
    case PROP_AUDIO_SYSTEM:
      g_value_set_enum(value, self->audio_system_setting);
      break;
    case PROP_INPUT_SOURCE:
      g_value_set_enum(value, self->input_source);
      break;
    case PROP_SDI_MODE:
      g_value_set_enum(value, self->sdi_mode);
      break;
    case PROP_AUDIO_SOURCE:
      g_value_set_enum(value, self->audio_source);
      break;
    case PROP_EMBEDDED_AUDIO_INPUT:
      g_value_set_enum(value, self->embedded_audio_input);
      break;
    case PROP_TIMECODE_INDEX:
      g_value_set_enum(value, self->timecode_index);
      break;
    case PROP_RP188:
      g_value_set_boolean(value, self->rp188);
      break;
    case PROP_REFERENCE_SOURCE:
      g_value_set_enum(value, self->reference_source);
      break;
    case PROP_CLOSED_CAPTION_CAPTURE_MODE:
      g_value_set_enum(value, self->closed_caption_capture_mode);
      break;
    case PROP_CAPTURE_CPU_CORE:
      g_value_set_uint(value, self->capture_cpu_core);
      break;
    case PROP_SIGNAL:
      g_value_set_boolean(value, self->signal);
      break;
    case PROP_ATTACH_ANCILLARY_META:
      g_value_set_boolean(value, self->attach_ancillary_meta);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
      break;
  }
}

void gst_aja_src_finalize(GObject *object) {
  GstAjaSrc *self = GST_AJA_SRC(object);

  g_assert(self->device == NULL);
  g_assert(gst_queue_array_get_length(self->queue) == 0);
  g_clear_pointer(&self->queue, gst_queue_array_free);

  g_mutex_clear(&self->queue_lock);
  g_cond_clear(&self->queue_cond);

  G_OBJECT_CLASS(parent_class)->finalize(object);
}

static gboolean gst_aja_src_open(GstAjaSrc *self) {
  GST_DEBUG_OBJECT(self, "Opening device");

  g_assert(self->device == NULL);

  self->device = gst_aja_ntv2_device_obtain(self->device_identifier);
  if (!self->device) {
    GST_ERROR_OBJECT(self, "Failed to open device");
    return FALSE;
  }

  if (!self->device->device->IsDeviceReady(false)) {
    g_clear_pointer(&self->device, gst_aja_ntv2_device_unref);
    return FALSE;
  }

  self->device->device->SetEveryFrameServices(::NTV2_OEM_TASKS);
  self->device_id = self->device->device->GetDeviceID();

  std::string serial_number;
  if (!self->device->device->GetSerialNumberString(serial_number))
    serial_number = "none";

  GST_DEBUG_OBJECT(self,
                   "Opened device with ID %d at index %d (%s, version %s, "
                   "serial number %s, can do VANC %d)",
                   self->device_id, self->device->device->GetIndexNumber(),
                   self->device->device->GetDisplayName().c_str(),
                   self->device->device->GetDeviceVersionString().c_str(),
                   serial_number.c_str(),
                   ::NTV2DeviceCanDoCustomAnc(self->device_id));

  GST_DEBUG_OBJECT(self,
                   "Using SDK version %d.%d.%d.%d (%s) and driver version %s",
                   AJA_NTV2_SDK_VERSION_MAJOR, AJA_NTV2_SDK_VERSION_MINOR,
                   AJA_NTV2_SDK_VERSION_POINT, AJA_NTV2_SDK_BUILD_NUMBER,
                   AJA_NTV2_SDK_BUILD_DATETIME,
                   self->device->device->GetDriverVersionString().c_str());

  self->device->device->SetMultiFormatMode(true);

  self->allocator = gst_aja_allocator_new(self->device);

  GST_DEBUG_OBJECT(self, "Opened device");

  return TRUE;
}

static gboolean gst_aja_src_close(GstAjaSrc *self) {
  gst_clear_object(&self->allocator);
  g_clear_pointer(&self->device, gst_aja_ntv2_device_unref);
  self->device_id = DEVICE_ID_INVALID;

  GST_DEBUG_OBJECT(self, "Closed device");

  return TRUE;
}

// Must be called with GstAjaNtv2DeviceLocker
static gboolean gst_aja_src_configure(GstAjaSrc *self) {
  GST_DEBUG_OBJECT(self, "Starting");

#define NEEDS_QUAD_MODE(self)                           \
  (self->sdi_mode == GST_AJA_SDI_MODE_QUAD_LINK_SQD ||  \
   self->sdi_mode == GST_AJA_SDI_MODE_QUAD_LINK_TSI ||  \
   (self->input_source >= GST_AJA_INPUT_SOURCE_HDMI1 && \
    self->input_source <= GST_AJA_INPUT_SOURCE_HDMI4))

  self->quad_mode = NEEDS_QUAD_MODE(self);

#undef NEEDS_QUAD_MODE

  if (self->quad_mode) {
    if (self->input_source != GST_AJA_INPUT_SOURCE_AUTO &&
        !(self->input_source >= GST_AJA_INPUT_SOURCE_HDMI1 &&
          self->input_source <= GST_AJA_INPUT_SOURCE_HDMI4)) {
      GST_ERROR_OBJECT(
          self,
          "Quad modes require usage of the channel's default input source");
      return FALSE;
    }

    if (self->channel != ::NTV2_CHANNEL1 && self->channel != ::NTV2_CHANNEL5) {
      GST_ERROR_OBJECT(self, "Quad modes require channels 1 or 5");
      return FALSE;
    }
  }

  bool had_quad_enabled = false, had_quad_quad_enabled = false;

  // HDMI can also be internally quad mode but it runs on a single channel.
  if (!(self->input_source >= GST_AJA_INPUT_SOURCE_HDMI1 &&
        self->input_source <= GST_AJA_INPUT_SOURCE_HDMI4)) {
    if (self->channel < ::NTV2_CHANNEL5) {
      self->device->device->GetQuadFrameEnable(had_quad_enabled,
                                               ::NTV2_CHANNEL1);

      // 12G UHD is also internally considered quad modes but they run on a
      // single channel.
      if (had_quad_enabled && ::NTV2DeviceCanDo12gRouting(self->device_id)) {
        NTV2VideoFormat fmt =
            self->device->device->GetInputVideoFormat(::NTV2_INPUTSOURCE_SDI1);
        if (fmt >= NTV2_FORMAT_FIRST_UHD_TSI_DEF_FORMAT &&
            fmt < NTV2_FORMAT_END_4K_TSI_DEF_FORMATS)
          had_quad_enabled = false;
      }

      self->device->device->GetQuadQuadFrameEnable(had_quad_quad_enabled,
                                                   ::NTV2_CHANNEL1);
    } else {
      self->device->device->GetQuadFrameEnable(had_quad_enabled,
                                               ::NTV2_CHANNEL5);

      // 12G UHD is also internally considered quad modes but they run on a
      // single channel.
      if (had_quad_enabled && ::NTV2DeviceCanDo12gRouting(self->device_id)) {
        NTV2VideoFormat fmt =
            self->device->device->GetInputVideoFormat(::NTV2_INPUTSOURCE_SDI5);
        if (fmt >= NTV2_FORMAT_FIRST_UHD_TSI_DEF_FORMAT &&
            fmt < NTV2_FORMAT_END_4K_TSI_DEF_FORMATS)
          had_quad_enabled = false;
      }

      self->device->device->GetQuadQuadFrameEnable(had_quad_quad_enabled,
                                                   ::NTV2_CHANNEL5);
    }
  }

  // Stop any previously running quad mode, or other configurations on the
  // quad channels
  self->device->device->AutoCirculateStop(self->channel);
  if (self->quad_mode || had_quad_enabled || had_quad_enabled) {
    NTV2Channel quad_channel;

    if (self->channel < ::NTV2_CHANNEL5)
      quad_channel = ::NTV2_CHANNEL1;
    else
      quad_channel = ::NTV2_CHANNEL5;

    for (int i = 0; i < 4; i++) {
      self->device->device->AutoCirculateStop((NTV2Channel)(quad_channel + i));
    }
  }

  if (self->buffer_pool) {
    gst_buffer_pool_set_active(self->buffer_pool, FALSE);
    gst_clear_object(&self->buffer_pool);
  }

  if (self->audio_buffer_pool) {
    gst_buffer_pool_set_active(self->audio_buffer_pool, FALSE);
    gst_clear_object(&self->audio_buffer_pool);
  }

  if (self->anc_buffer_pool) {
    gst_buffer_pool_set_active(self->anc_buffer_pool, FALSE);
    gst_clear_object(&self->anc_buffer_pool);
  }

  NTV2VANCMode vanc_mode;
  NTV2InputSource input_source;
  NTV2OutputCrosspointID input_source_id;
  switch (self->input_source) {
    case GST_AJA_INPUT_SOURCE_AUTO:
      input_source = ::NTV2ChannelToInputSource(self->channel);
      input_source_id = ::GetSDIInputOutputXptFromChannel(self->channel, false);
      vanc_mode = ::NTV2DeviceCanDoCustomAnc(self->device_id)
                      ? ::NTV2_VANCMODE_OFF
                      : ::NTV2_VANCMODE_TALL;
      break;
    case GST_AJA_INPUT_SOURCE_ANALOG1:
      input_source = ::NTV2_INPUTSOURCE_ANALOG1;
      input_source_id = ::NTV2_XptAnalogIn;
      vanc_mode = ::NTV2_VANCMODE_TALL;
      break;
    case GST_AJA_INPUT_SOURCE_HDMI1:
      input_source = ::NTV2_INPUTSOURCE_HDMI1;
      input_source_id = ::NTV2_XptHDMIIn1;
      vanc_mode = ::NTV2_VANCMODE_OFF;
      break;
    case GST_AJA_INPUT_SOURCE_HDMI2:
      input_source = ::NTV2_INPUTSOURCE_HDMI2;
      input_source_id = ::NTV2_XptHDMIIn2;
      vanc_mode = ::NTV2_VANCMODE_OFF;
      break;
    case GST_AJA_INPUT_SOURCE_HDMI3:
      input_source = ::NTV2_INPUTSOURCE_HDMI3;
      input_source_id = ::NTV2_XptHDMIIn3;
      vanc_mode = ::NTV2_VANCMODE_OFF;
      break;
    case GST_AJA_INPUT_SOURCE_HDMI4:
      input_source = ::NTV2_INPUTSOURCE_HDMI4;
      input_source_id = ::NTV2_XptHDMIIn4;
      vanc_mode = ::NTV2_VANCMODE_OFF;
      break;
    case GST_AJA_INPUT_SOURCE_SDI1:
      input_source = ::NTV2_INPUTSOURCE_SDI1;
      input_source_id = ::NTV2_XptSDIIn1;
      vanc_mode = ::NTV2_VANCMODE_TALL;
      break;
    case GST_AJA_INPUT_SOURCE_SDI2:
      input_source = ::NTV2_INPUTSOURCE_SDI2;
      input_source_id = ::NTV2_XptSDIIn2;
      vanc_mode = ::NTV2_VANCMODE_TALL;
      break;
    case GST_AJA_INPUT_SOURCE_SDI3:
      input_source = ::NTV2_INPUTSOURCE_SDI3;
      input_source_id = ::NTV2_XptSDIIn3;
      vanc_mode = ::NTV2_VANCMODE_TALL;
      break;
    case GST_AJA_INPUT_SOURCE_SDI4:
      input_source = ::NTV2_INPUTSOURCE_SDI4;
      input_source_id = ::NTV2_XptSDIIn4;
      vanc_mode = ::NTV2_VANCMODE_TALL;
      break;
    case GST_AJA_INPUT_SOURCE_SDI5:
      input_source = ::NTV2_INPUTSOURCE_SDI5;
      input_source_id = ::NTV2_XptSDIIn5;
      vanc_mode = ::NTV2_VANCMODE_TALL;
      break;
    case GST_AJA_INPUT_SOURCE_SDI6:
      input_source = ::NTV2_INPUTSOURCE_SDI6;
      input_source_id = ::NTV2_XptSDIIn6;
      vanc_mode = ::NTV2_VANCMODE_TALL;
      break;
    case GST_AJA_INPUT_SOURCE_SDI7:
      input_source = ::NTV2_INPUTSOURCE_SDI7;
      input_source_id = ::NTV2_XptSDIIn7;
      vanc_mode = ::NTV2_VANCMODE_TALL;
      break;
    case GST_AJA_INPUT_SOURCE_SDI8:
      input_source = ::NTV2_INPUTSOURCE_SDI8;
      input_source_id = ::NTV2_XptSDIIn8;
      vanc_mode = ::NTV2_VANCMODE_TALL;
      break;
    default:
      g_assert_not_reached();
      break;
  }

  self->configured_input_source = input_source;

  self->vanc_mode = vanc_mode;

  if (!self->device->device->EnableChannel(self->channel)) {
    GST_ERROR_OBJECT(self, "Failed to enable channel");
    return FALSE;
  }

  if (self->quad_mode) {
    for (int i = 1; i < 4; i++) {
      if (!self->device->device->EnableChannel(
              (NTV2Channel)(self->channel + i))) {
        GST_ERROR_OBJECT(self, "Failed to enable channel");
        return FALSE;
      }
    }
  }

  self->device->device->EnableInputInterrupt(self->channel);
  self->device->device->SubscribeInputVerticalEvent(self->channel);

  if (self->video_format_setting == GST_AJA_VIDEO_FORMAT_AUTO) {
    self->device->device->WaitForInputVerticalInterrupt(self->channel, 10);
    self->video_format = self->device->device->GetInputVideoFormat(
        self->configured_input_source);
    if (self->video_format == NTV2_FORMAT_UNKNOWN) {
      GST_ERROR_OBJECT(self, "Input video format not detected");
      return TRUE;
    }
    std::string configured_string = NTV2VideoFormatToString(self->video_format);
    GST_DEBUG_OBJECT(self, "Detected input video format %s (%d)",
                     configured_string.c_str(), (int)self->video_format);
  } else {
    self->video_format = gst_ntv2_video_format_from_aja_format(
        self->video_format_setting, self->quad_mode);
  }

  if (self->video_format == NTV2_FORMAT_UNKNOWN) {
    GST_ERROR_OBJECT(self, "Unsupported mode");
    return FALSE;
  }

  if (!::NTV2DeviceCanDoVideoFormat(self->device_id, self->video_format)) {
    GST_ERROR_OBJECT(self, "Device does not support mode %d",
                     (int)self->video_format);
    return FALSE;
  }

  gst_video_info_from_ntv2_video_format(&self->configured_info,
                                        self->video_format);

  if (self->quad_mode) {
    if (self->input_source >= GST_AJA_INPUT_SOURCE_HDMI1 &&
        self->input_source <= GST_AJA_INPUT_SOURCE_HDMI4) {
      self->device->device->SetQuadQuadFrameEnable(false, self->channel);
      self->device->device->SetQuadQuadSquaresEnable(false, self->channel);
      self->device->device->Set4kSquaresEnable(true, self->channel);
      self->device->device->SetTsiFrameEnable(true, self->channel);
    } else {
      switch (self->sdi_mode) {
        case GST_AJA_SDI_MODE_SINGLE_LINK:
          g_assert_not_reached();
          break;
        case GST_AJA_SDI_MODE_QUAD_LINK_SQD:
          if (self->configured_info.height > 2160) {
            self->device->device->Set4kSquaresEnable(false, self->channel);
            self->device->device->SetTsiFrameEnable(false, self->channel);
            self->device->device->SetQuadQuadFrameEnable(true, self->channel);
            self->device->device->SetQuadQuadSquaresEnable(true, self->channel);
          } else {
            self->device->device->SetQuadQuadFrameEnable(false, self->channel);
            self->device->device->SetQuadQuadSquaresEnable(false,
                                                           self->channel);
            self->device->device->Set4kSquaresEnable(true, self->channel);
            self->device->device->SetTsiFrameEnable(false, self->channel);
          }
          break;
        case GST_AJA_SDI_MODE_QUAD_LINK_TSI:
          if (self->configured_info.height > 2160) {
            self->device->device->Set4kSquaresEnable(false, self->channel);
            self->device->device->SetTsiFrameEnable(false, self->channel);
            self->device->device->SetQuadQuadFrameEnable(true, self->channel);
            self->device->device->SetQuadQuadSquaresEnable(false,
                                                           self->channel);
          } else {
            self->device->device->SetQuadQuadFrameEnable(false, self->channel);
            self->device->device->SetQuadQuadSquaresEnable(false,
                                                           self->channel);
            self->device->device->Set4kSquaresEnable(false, self->channel);
            self->device->device->SetTsiFrameEnable(true, self->channel);
          }
          break;
      }
    }
  } else if (had_quad_enabled || had_quad_quad_enabled) {
    NTV2Channel quad_channel;

    if (self->channel < ::NTV2_CHANNEL5)
      quad_channel = ::NTV2_CHANNEL1;
    else
      quad_channel = ::NTV2_CHANNEL5;

    self->device->device->Set4kSquaresEnable(false, quad_channel);
    self->device->device->SetTsiFrameEnable(false, quad_channel);
    self->device->device->SetQuadQuadFrameEnable(false, quad_channel);
    self->device->device->SetQuadQuadSquaresEnable(false, quad_channel);
  }

  self->device->device->SetMode(self->channel, NTV2_MODE_CAPTURE, false);
  if (self->quad_mode) {
    for (int i = 1; i < 4; i++)
      self->device->device->SetMode((NTV2Channel)(self->channel + i),
                                    NTV2_MODE_CAPTURE, false);
  }

  std::string configured_string = NTV2VideoFormatToString(self->video_format);
  GST_DEBUG_OBJECT(self, "Configuring video format %s (%d) on channel %d",
                   configured_string.c_str(), (int)self->video_format,
                   (int)self->channel);
  if (!self->device->device->SetVideoFormat(self->video_format, false, false,
                                            self->channel)) {
    GST_DEBUG_OBJECT(
        self, "Failed configuring video format %s (%d) on channel %d",
        configured_string.c_str(), (int)self->video_format, (int)self->channel);
    return FALSE;
  }

  if (!::NTV2DeviceCanDoFrameBufferFormat(self->device_id,
                                          ::NTV2_FBF_10BIT_YCBCR)) {
    GST_ERROR_OBJECT(self, "Device does not support frame buffer format %d",
                     (int)::NTV2_FBF_10BIT_YCBCR);
    return FALSE;
  }

  if (!self->device->device->SetFrameBufferFormat(self->channel,
                                                  ::NTV2_FBF_10BIT_YCBCR)) {
    GST_ERROR_OBJECT(self, "Failed configuring frame buffer format %d",
                     (int)::NTV2_FBF_10BIT_YCBCR);
    return FALSE;
  }

  // FIXME: Workaround for sometimes setting the video format not actually
  // changing the register values. Let's just try again.
  {
    NTV2VideoFormat fmt;
    self->device->device->GetVideoFormat(fmt, self->channel);

    if (fmt != self->video_format) {
      std::string actual_string = NTV2VideoFormatToString(fmt);

      GST_ERROR_OBJECT(self,
                       "Configured video format %s (%d) on channel %d but %s "
                       "(%d) is configured instead, trying again",
                       configured_string.c_str(), (int)self->video_format,
                       (int)self->channel, actual_string.c_str(), (int)fmt);
      self->video_format = ::NTV2_FORMAT_UNKNOWN;
      return TRUE;
    }
  }

  if (self->quad_mode) {
    for (int i = 1; i < 4; i++)
      self->device->device->SetFrameBufferFormat(
          (NTV2Channel)(self->channel + i), ::NTV2_FBF_10BIT_YCBCR);
  }

  self->device->device->DMABufferAutoLock(false, true, 0);

  if (::NTV2DeviceHasBiDirectionalSDI(self->device_id)) {
    self->device->device->SetSDITransmitEnable(self->channel, false);
    if (self->quad_mode) {
      for (int i = 1; i < 4; i++)
        self->device->device->SetSDITransmitEnable(
            (NTV2Channel)(self->channel + i), false);
    }
  }

  // Always use the framebuffer associated with the channel
  NTV2InputCrosspointID framebuffer_id =
      ::GetFrameBufferInputXptFromChannel(self->channel, false);

  const NTV2Standard standard(
      ::GetNTV2StandardFromVideoFormat(self->video_format));
  self->device->device->SetStandard(standard, self->channel);
  if (self->quad_mode) {
    for (int i = 1; i < 4; i++)
      self->device->device->SetStandard(standard,
                                        (NTV2Channel)(self->channel + i));
  }

  const NTV2FrameGeometry geometry =
      ::GetNTV2FrameGeometryFromVideoFormat(self->video_format);

  self->vanc_mode =
      ::HasVANCGeometries(geometry) ? vanc_mode : ::NTV2_VANCMODE_OFF;
  if (self->vanc_mode == ::NTV2_VANCMODE_OFF) {
    self->device->device->SetFrameGeometry(geometry, false, self->channel);

    if (self->quad_mode) {
      for (int i = 1; i < 4; i++) {
        self->device->device->SetFrameGeometry(
            geometry, false, (NTV2Channel)(self->channel + i));
      }
    }
  } else {
    const NTV2FrameGeometry vanc_geometry =
        ::GetVANCFrameGeometry(geometry, self->vanc_mode);

    self->device->device->SetFrameGeometry(vanc_geometry, false, self->channel);

    if (self->quad_mode) {
      for (int i = 1; i < 4; i++) {
        self->device->device->SetFrameGeometry(
            vanc_geometry, false, (NTV2Channel)(self->channel + i));
      }
    }
  }

  CNTV2SignalRouter router;

  // If any channels are currently running, initialize the router with the
  // existing routing setup. Otherwise overwrite the whole routing table.
  {
    bool have_channels_running = false;

    for (NTV2Channel c = ::NTV2_CHANNEL1; c < NTV2_MAX_NUM_CHANNELS;
         c = (NTV2Channel)(c + 1)) {
      AUTOCIRCULATE_STATUS ac_status;

      if (c == self->channel) continue;

      if (self->device->device->AutoCirculateGetStatus(c, ac_status) &&
          !ac_status.IsStopped()) {
        have_channels_running = true;
        break;
      }
    }

    if (have_channels_running) self->device->device->GetRouting(router);
  }

  // Need to remove old routes for the output and framebuffer we're going to
  // use
  NTV2ActualConnections connections = router.GetConnections();

  if (self->quad_mode) {
    if (self->input_source >= GST_AJA_INPUT_SOURCE_HDMI1 &&
        self->input_source <= GST_AJA_INPUT_SOURCE_HDMI4) {
      // Need to disconnect the 4 inputs corresponding to this channel from
      // their framebuffers/muxers, and muxers from their framebuffers
      for (auto iter = connections.begin(); iter != connections.end(); iter++) {
        if (iter->first == NTV2_XptFrameBuffer1Input ||
            iter->first == NTV2_XptFrameBuffer1BInput ||
            iter->first == NTV2_XptFrameBuffer2Input ||
            iter->first == NTV2_XptFrameBuffer2BInput ||
            iter->second == NTV2_Xpt425Mux1AYUV ||
            iter->second == NTV2_Xpt425Mux1BYUV ||
            iter->second == NTV2_Xpt425Mux2AYUV ||
            iter->second == NTV2_Xpt425Mux2BYUV ||
            iter->first == NTV2_Xpt425Mux1AInput ||
            iter->first == NTV2_Xpt425Mux1BInput ||
            iter->first == NTV2_Xpt425Mux2AInput ||
            iter->first == NTV2_Xpt425Mux2BInput ||
            iter->second == NTV2_XptHDMIIn1 ||
            iter->second == NTV2_XptHDMIIn1Q2 ||
            iter->second == NTV2_XptHDMIIn1Q3 ||
            iter->second == NTV2_XptHDMIIn1Q4)
          router.RemoveConnection(iter->first, iter->second);
      }
    } else if (self->channel == NTV2_CHANNEL1) {
      for (auto iter = connections.begin(); iter != connections.end(); iter++) {
        if (iter->first == NTV2_XptFrameBuffer1Input ||
            iter->first == NTV2_XptFrameBuffer1BInput ||
            iter->first == NTV2_XptFrameBuffer1DS2Input ||
            iter->first == NTV2_XptFrameBuffer2Input ||
            iter->first == NTV2_XptFrameBuffer2BInput ||
            iter->first == NTV2_XptFrameBuffer2DS2Input ||
            iter->second == NTV2_Xpt425Mux1AYUV ||
            iter->second == NTV2_Xpt425Mux1BYUV ||
            iter->second == NTV2_Xpt425Mux2AYUV ||
            iter->second == NTV2_Xpt425Mux2BYUV ||
            iter->first == NTV2_Xpt425Mux1AInput ||
            iter->first == NTV2_Xpt425Mux1BInput ||
            iter->first == NTV2_Xpt425Mux2AInput ||
            iter->first == NTV2_Xpt425Mux2BInput ||
            iter->second == NTV2_XptSDIIn1 || iter->second == NTV2_XptSDIIn2 ||
            iter->second == NTV2_XptSDIIn3 || iter->second == NTV2_XptSDIIn4 ||
            iter->second == NTV2_XptSDIIn1DS2 ||
            iter->second == NTV2_XptSDIIn2DS2 ||
            iter->first == NTV2_XptFrameBuffer1Input ||
            iter->first == NTV2_XptFrameBuffer2Input ||
            iter->first == NTV2_XptFrameBuffer3Input ||
            iter->first == NTV2_XptFrameBuffer4Input)
          router.RemoveConnection(iter->first, iter->second);
      }
    } else if (self->channel == NTV2_CHANNEL5) {
      for (auto iter = connections.begin(); iter != connections.end(); iter++) {
        if (iter->first == NTV2_XptFrameBuffer5Input ||
            iter->first == NTV2_XptFrameBuffer5BInput ||
            iter->first == NTV2_XptFrameBuffer5DS2Input ||
            iter->first == NTV2_XptFrameBuffer6Input ||
            iter->first == NTV2_XptFrameBuffer6BInput ||
            iter->first == NTV2_XptFrameBuffer6DS2Input ||
            iter->second == NTV2_Xpt425Mux3AYUV ||
            iter->second == NTV2_Xpt425Mux3BYUV ||
            iter->second == NTV2_Xpt425Mux4AYUV ||
            iter->second == NTV2_Xpt425Mux4BYUV ||
            iter->first == NTV2_Xpt425Mux3AInput ||
            iter->first == NTV2_Xpt425Mux3BInput ||
            iter->first == NTV2_Xpt425Mux4AInput ||
            iter->first == NTV2_Xpt425Mux4BInput ||
            iter->second == NTV2_XptSDIIn5 || iter->second == NTV2_XptSDIIn6 ||
            iter->second == NTV2_XptSDIIn7 || iter->second == NTV2_XptSDIIn8 ||
            iter->second == NTV2_XptSDIIn5DS2 ||
            iter->second == NTV2_XptSDIIn6DS2 ||
            iter->first == NTV2_XptFrameBuffer5Input ||
            iter->first == NTV2_XptFrameBuffer6Input ||
            iter->first == NTV2_XptFrameBuffer7Input ||
            iter->first == NTV2_XptFrameBuffer8Input)
          router.RemoveConnection(iter->first, iter->second);
      }
    } else {
      g_assert_not_reached();
    }
  } else {
    // This also removes all connections for any previous quad mode on the
    // corresponding channels.

    NTV2OutputCrosspointID quad_input_source_ids[10];

    if (input_source_id == NTV2_XptSDIIn1 ||
        input_source_id == NTV2_XptSDIIn2 ||
        input_source_id == NTV2_XptSDIIn3 ||
        input_source_id == NTV2_XptSDIIn4) {
      if (had_quad_enabled || had_quad_quad_enabled) {
        quad_input_source_ids[0] = NTV2_XptSDIIn1;
        quad_input_source_ids[1] = NTV2_XptSDIIn2;
        quad_input_source_ids[2] = NTV2_XptSDIIn3;
        quad_input_source_ids[3] = NTV2_XptSDIIn4;
        quad_input_source_ids[4] = NTV2_XptSDIIn1DS2;
        quad_input_source_ids[5] = NTV2_XptSDIIn2DS2;
        quad_input_source_ids[6] = NTV2_Xpt425Mux1AYUV;
        quad_input_source_ids[7] = NTV2_Xpt425Mux1BYUV;
        quad_input_source_ids[8] = NTV2_Xpt425Mux2AYUV;
        quad_input_source_ids[9] = NTV2_Xpt425Mux2BYUV;
      }
    } else if (input_source_id == NTV2_XptSDIIn5 ||
               input_source_id == NTV2_XptSDIIn6 ||
               input_source_id == NTV2_XptSDIIn7 ||
               input_source_id == NTV2_XptSDIIn8) {
      if (had_quad_enabled || had_quad_quad_enabled) {
        quad_input_source_ids[0] = NTV2_XptSDIIn5;
        quad_input_source_ids[1] = NTV2_XptSDIIn6;
        quad_input_source_ids[2] = NTV2_XptSDIIn7;
        quad_input_source_ids[3] = NTV2_XptSDIIn8;
        quad_input_source_ids[4] = NTV2_XptSDIIn5DS2;
        quad_input_source_ids[5] = NTV2_XptSDIIn6DS2;
        quad_input_source_ids[6] = NTV2_Xpt425Mux3AYUV;
        quad_input_source_ids[7] = NTV2_Xpt425Mux3BYUV;
        quad_input_source_ids[8] = NTV2_Xpt425Mux4AYUV;
        quad_input_source_ids[9] = NTV2_Xpt425Mux4BYUV;
      }
    } else {
      g_assert_not_reached();
    }

    for (auto iter = connections.begin(); iter != connections.end(); iter++) {
      if (had_quad_enabled || had_quad_quad_enabled) {
        for (auto quad_input_source_id : quad_input_source_ids) {
          if (iter->second == quad_input_source_id)
            router.RemoveConnection(iter->first, iter->second);
        }
      } else {
        if (iter->first == framebuffer_id || iter->second == input_source_id)
          router.RemoveConnection(iter->first, iter->second);
      }
    }
  }

  if (self->quad_mode) {
    if (self->input_source >= GST_AJA_INPUT_SOURCE_HDMI1 &&
        self->input_source <= GST_AJA_INPUT_SOURCE_HDMI4) {
      input_source_id = NTV2_Xpt425Mux1AYUV;
    } else if (self->sdi_mode == GST_AJA_SDI_MODE_QUAD_LINK_TSI &&
               !NTV2_IS_QUAD_QUAD_HFR_VIDEO_FORMAT(self->video_format) &&
               !NTV2_IS_QUAD_QUAD_FORMAT(self->video_format)) {
      if (self->channel == NTV2_CHANNEL1)
        input_source_id = NTV2_Xpt425Mux1AYUV;
      else if (self->channel == NTV2_CHANNEL5)
        input_source_id = NTV2_Xpt425Mux3AYUV;
      else
        g_assert_not_reached();
    }
  }

  GST_DEBUG_OBJECT(self, "Creating connection %d - %d", framebuffer_id,
                   input_source_id);
  router.AddConnection(framebuffer_id, input_source_id);

  if (self->quad_mode) {
    if (self->input_source >= GST_AJA_INPUT_SOURCE_HDMI1 &&
        self->input_source <= GST_AJA_INPUT_SOURCE_HDMI4) {
      router.AddConnection(NTV2_XptFrameBuffer1BInput, NTV2_Xpt425Mux1BYUV);
      router.AddConnection(NTV2_XptFrameBuffer2Input, NTV2_Xpt425Mux2AYUV);
      router.AddConnection(NTV2_XptFrameBuffer2BInput, NTV2_Xpt425Mux2BYUV);

      router.AddConnection(NTV2_Xpt425Mux1AInput, NTV2_XptHDMIIn1);
      router.AddConnection(NTV2_Xpt425Mux1BInput, NTV2_XptHDMIIn1Q2);
      router.AddConnection(NTV2_Xpt425Mux2AInput, NTV2_XptHDMIIn1Q3);
      router.AddConnection(NTV2_Xpt425Mux2BInput, NTV2_XptHDMIIn1Q4);
    } else {
      if (self->sdi_mode == GST_AJA_SDI_MODE_QUAD_LINK_TSI) {
        if (NTV2_IS_QUAD_QUAD_HFR_VIDEO_FORMAT(self->video_format)) {
          if (self->channel == NTV2_CHANNEL1) {
            router.AddConnection(NTV2_XptFrameBuffer1DS2Input, NTV2_XptSDIIn2);
            router.AddConnection(NTV2_XptFrameBuffer2Input, NTV2_XptSDIIn3);
            router.AddConnection(NTV2_XptFrameBuffer2DS2Input, NTV2_XptSDIIn4);
          } else if (self->channel == NTV2_CHANNEL5) {
            router.AddConnection(NTV2_XptFrameBuffer5DS2Input, NTV2_XptSDIIn6);
            router.AddConnection(NTV2_XptFrameBuffer5Input, NTV2_XptSDIIn7);
            router.AddConnection(NTV2_XptFrameBuffer6DS2Input, NTV2_XptSDIIn8);
          } else {
            g_assert_not_reached();
          }
        } else if (NTV2_IS_QUAD_QUAD_FORMAT(self->video_format)) {
          if (self->channel == NTV2_CHANNEL1) {
            router.AddConnection(NTV2_XptFrameBuffer1DS2Input,
                                 NTV2_XptSDIIn1DS2);
            router.AddConnection(NTV2_XptFrameBuffer2Input, NTV2_XptSDIIn2);
            router.AddConnection(NTV2_XptFrameBuffer2DS2Input,
                                 NTV2_XptSDIIn2DS2);
          } else if (self->channel == NTV2_CHANNEL5) {
            router.AddConnection(NTV2_XptFrameBuffer5DS2Input,
                                 NTV2_XptSDIIn5DS2);
            router.AddConnection(NTV2_XptFrameBuffer5Input, NTV2_XptSDIIn6);
            router.AddConnection(NTV2_XptFrameBuffer6DS2Input,
                                 NTV2_XptSDIIn6DS2);
          } else {
            g_assert_not_reached();
          }
          // FIXME: Need special handling of NTV2_IS_4K_HFR_VIDEO_FORMAT for
          // TSI?
        } else {
          if (self->channel == NTV2_CHANNEL1) {
            router.AddConnection(NTV2_XptFrameBuffer1BInput,
                                 NTV2_Xpt425Mux1BYUV);
            router.AddConnection(NTV2_XptFrameBuffer2Input,
                                 NTV2_Xpt425Mux2AYUV);
            router.AddConnection(NTV2_XptFrameBuffer2BInput,
                                 NTV2_Xpt425Mux2BYUV);

            router.AddConnection(NTV2_Xpt425Mux1AInput, NTV2_XptSDIIn1);
            router.AddConnection(NTV2_Xpt425Mux1BInput, NTV2_XptSDIIn2);
            router.AddConnection(NTV2_Xpt425Mux2AInput, NTV2_XptSDIIn3);
            router.AddConnection(NTV2_Xpt425Mux2BInput, NTV2_XptSDIIn4);
          } else if (self->channel == NTV2_CHANNEL5) {
            router.AddConnection(NTV2_XptFrameBuffer5BInput,
                                 NTV2_Xpt425Mux3BYUV);
            router.AddConnection(NTV2_XptFrameBuffer6Input,
                                 NTV2_Xpt425Mux4AYUV);
            router.AddConnection(NTV2_XptFrameBuffer6BInput,
                                 NTV2_Xpt425Mux4BYUV);

            router.AddConnection(NTV2_Xpt425Mux3AInput, NTV2_XptSDIIn5);
            router.AddConnection(NTV2_Xpt425Mux3BInput, NTV2_XptSDIIn6);
            router.AddConnection(NTV2_Xpt425Mux4AInput, NTV2_XptSDIIn7);
            router.AddConnection(NTV2_Xpt425Mux4BInput, NTV2_XptSDIIn8);
          } else {
            g_assert_not_reached();
          }
        }
      } else {
        if (self->channel == NTV2_CHANNEL1) {
          router.AddConnection(NTV2_XptFrameBuffer2Input, NTV2_XptSDIIn2);
          router.AddConnection(NTV2_XptFrameBuffer3Input, NTV2_XptSDIIn3);
          router.AddConnection(NTV2_XptFrameBuffer4Input, NTV2_XptSDIIn4);
        } else if (self->channel == NTV2_CHANNEL5) {
          router.AddConnection(NTV2_XptFrameBuffer6Input, NTV2_XptSDIIn6);
          router.AddConnection(NTV2_XptFrameBuffer7Input, NTV2_XptSDIIn7);
          router.AddConnection(NTV2_XptFrameBuffer8Input, NTV2_XptSDIIn8);
        } else {
          g_assert_not_reached();
        }
      }
    }
  }

  {
    std::stringstream os;
    CNTV2SignalRouter oldRouter;
    self->device->device->GetRouting(oldRouter);
    oldRouter.Print(os);
    GST_DEBUG_OBJECT(self, "Previous routing:\n%s", os.str().c_str());
  }
  self->device->device->ApplySignalRoute(router, true);
  {
    std::stringstream os;
    CNTV2SignalRouter currentRouter;
    self->device->device->GetRouting(currentRouter);
    currentRouter.Print(os);
    GST_DEBUG_OBJECT(self, "New routing:\n%s", os.str().c_str());
  }

  switch (self->audio_system_setting) {
    case GST_AJA_AUDIO_SYSTEM_1:
      self->audio_system = ::NTV2_AUDIOSYSTEM_1;
      break;
    case GST_AJA_AUDIO_SYSTEM_2:
      self->audio_system = ::NTV2_AUDIOSYSTEM_2;
      break;
    case GST_AJA_AUDIO_SYSTEM_3:
      self->audio_system = ::NTV2_AUDIOSYSTEM_3;
      break;
    case GST_AJA_AUDIO_SYSTEM_4:
      self->audio_system = ::NTV2_AUDIOSYSTEM_4;
      break;
    case GST_AJA_AUDIO_SYSTEM_5:
      self->audio_system = ::NTV2_AUDIOSYSTEM_5;
      break;
    case GST_AJA_AUDIO_SYSTEM_6:
      self->audio_system = ::NTV2_AUDIOSYSTEM_6;
      break;
    case GST_AJA_AUDIO_SYSTEM_7:
      self->audio_system = ::NTV2_AUDIOSYSTEM_7;
      break;
    case GST_AJA_AUDIO_SYSTEM_8:
      self->audio_system = ::NTV2_AUDIOSYSTEM_8;
      break;
    case GST_AJA_AUDIO_SYSTEM_AUTO:
      self->audio_system = ::NTV2_AUDIOSYSTEM_1;
      if (::NTV2DeviceGetNumAudioSystems(self->device_id) > 1)
        self->audio_system = ::NTV2ChannelToAudioSystem(self->channel);
      if (!::NTV2DeviceCanDoFrameStore1Display(self->device_id))
        self->audio_system = ::NTV2_AUDIOSYSTEM_1;
      break;
    default:
      g_assert_not_reached();
      break;
  }

  GST_DEBUG_OBJECT(self, "Using audio system %d", self->audio_system);

  NTV2AudioSource audio_source;
  switch (self->audio_source) {
    case GST_AJA_AUDIO_SOURCE_EMBEDDED:
      audio_source = ::NTV2_AUDIO_EMBEDDED;
      break;
    case GST_AJA_AUDIO_SOURCE_AES:
      audio_source = ::NTV2_AUDIO_AES;
      break;
    case GST_AJA_AUDIO_SOURCE_ANALOG:
      audio_source = ::NTV2_AUDIO_ANALOG;
      break;
    case GST_AJA_AUDIO_SOURCE_HDMI:
      audio_source = ::NTV2_AUDIO_HDMI;
      break;
    case GST_AJA_AUDIO_SOURCE_MIC:
      audio_source = ::NTV2_AUDIO_MIC;
      break;
    default:
      g_assert_not_reached();
      break;
  }

  NTV2EmbeddedAudioInput embedded_audio_input;
  switch (self->embedded_audio_input) {
    case GST_AJA_EMBEDDED_AUDIO_INPUT_AUTO:
      embedded_audio_input =
          ::NTV2InputSourceToEmbeddedAudioInput(input_source);
      break;
    case GST_AJA_EMBEDDED_AUDIO_INPUT_VIDEO1:
      embedded_audio_input = ::NTV2_EMBEDDED_AUDIO_INPUT_VIDEO_1;
      break;
    case GST_AJA_EMBEDDED_AUDIO_INPUT_VIDEO2:
      embedded_audio_input = ::NTV2_EMBEDDED_AUDIO_INPUT_VIDEO_2;
      break;
    case GST_AJA_EMBEDDED_AUDIO_INPUT_VIDEO3:
      embedded_audio_input = ::NTV2_EMBEDDED_AUDIO_INPUT_VIDEO_3;
      break;
    case GST_AJA_EMBEDDED_AUDIO_INPUT_VIDEO4:
      embedded_audio_input = ::NTV2_EMBEDDED_AUDIO_INPUT_VIDEO_4;
      break;
    case GST_AJA_EMBEDDED_AUDIO_INPUT_VIDEO5:
      embedded_audio_input = ::NTV2_EMBEDDED_AUDIO_INPUT_VIDEO_5;
      break;
    case GST_AJA_EMBEDDED_AUDIO_INPUT_VIDEO6:
      embedded_audio_input = ::NTV2_EMBEDDED_AUDIO_INPUT_VIDEO_6;
      break;
    case GST_AJA_EMBEDDED_AUDIO_INPUT_VIDEO7:
      embedded_audio_input = ::NTV2_EMBEDDED_AUDIO_INPUT_VIDEO_7;
      break;
    case GST_AJA_EMBEDDED_AUDIO_INPUT_VIDEO8:
      embedded_audio_input = ::NTV2_EMBEDDED_AUDIO_INPUT_VIDEO_8;
      break;
    default:
      g_assert_not_reached();
      break;
  }

  self->device->device->SetAudioSystemInputSource(
      self->audio_system, audio_source, embedded_audio_input);
  self->configured_audio_channels =
      ::NTV2DeviceGetMaxAudioChannels(self->device_id);
  self->device->device->SetNumberAudioChannels(self->configured_audio_channels,
                                               self->audio_system);
  self->device->device->SetAudioRate(::NTV2_AUDIO_48K, self->audio_system);
  self->device->device->SetAudioBufferSize(::NTV2_AUDIO_BUFFER_BIG,
                                           self->audio_system);
  self->device->device->SetAudioLoopBack(::NTV2_AUDIO_LOOPBACK_OFF,
                                         self->audio_system);
  self->device->device->SetEmbeddedAudioClock(
      ::NTV2_EMBEDDED_AUDIO_CLOCK_VIDEO_INPUT, self->audio_system);

  NTV2ReferenceSource reference_source;
  switch (self->reference_source) {
    case GST_AJA_REFERENCE_SOURCE_AUTO:
      reference_source = ::NTV2InputSourceToReferenceSource(input_source);
      break;
    case GST_AJA_REFERENCE_SOURCE_EXTERNAL:
      reference_source = ::NTV2_REFERENCE_EXTERNAL;
      break;
    case GST_AJA_REFERENCE_SOURCE_FREERUN:
      reference_source = ::NTV2_REFERENCE_FREERUN;
      break;
    case GST_AJA_REFERENCE_SOURCE_INPUT_1:
      reference_source = ::NTV2_REFERENCE_INPUT1;
      break;
    case GST_AJA_REFERENCE_SOURCE_INPUT_2:
      reference_source = ::NTV2_REFERENCE_INPUT2;
      break;
    case GST_AJA_REFERENCE_SOURCE_INPUT_3:
      reference_source = ::NTV2_REFERENCE_INPUT3;
      break;
    case GST_AJA_REFERENCE_SOURCE_INPUT_4:
      reference_source = ::NTV2_REFERENCE_INPUT4;
      break;
    case GST_AJA_REFERENCE_SOURCE_INPUT_5:
      reference_source = ::NTV2_REFERENCE_INPUT5;
      break;
    case GST_AJA_REFERENCE_SOURCE_INPUT_6:
      reference_source = ::NTV2_REFERENCE_INPUT6;
      break;
    case GST_AJA_REFERENCE_SOURCE_INPUT_7:
      reference_source = ::NTV2_REFERENCE_INPUT7;
      break;
    case GST_AJA_REFERENCE_SOURCE_INPUT_8:
      reference_source = ::NTV2_REFERENCE_INPUT8;
      break;
    default:
      g_assert_not_reached();
      break;
  }
  GST_DEBUG_OBJECT(self, "Configuring reference source %d",
                   (int)reference_source);

  self->device->device->SetReference(reference_source);
  self->device->device->SetLTCInputEnable(true);
  self->device->device->SetRP188SourceFilter(self->channel, 0xff);

  guint video_buffer_size = ::GetVideoActiveSize(
      self->video_format, ::NTV2_FBF_10BIT_YCBCR, self->vanc_mode);

  self->buffer_pool = gst_buffer_pool_new();
  GstStructure *config = gst_buffer_pool_get_config(self->buffer_pool);
  gst_buffer_pool_config_set_params(config, NULL, video_buffer_size,
                                    2 * self->queue_size, 0);
  gst_buffer_pool_config_set_allocator(config, self->allocator, NULL);
  gst_buffer_pool_set_config(self->buffer_pool, config);
  gst_buffer_pool_set_active(self->buffer_pool, TRUE);

  guint audio_buffer_size = 401 * 1024;

  self->audio_buffer_pool = gst_buffer_pool_new();
  config = gst_buffer_pool_get_config(self->audio_buffer_pool);
  gst_buffer_pool_config_set_params(config, NULL, audio_buffer_size,
                                    2 * self->queue_size, 0);
  gst_buffer_pool_config_set_allocator(config, self->allocator, NULL);
  gst_buffer_pool_set_config(self->audio_buffer_pool, config);
  gst_buffer_pool_set_active(self->audio_buffer_pool, TRUE);

  guint anc_buffer_size = 8 * 1024;

  if (self->vanc_mode == ::NTV2_VANCMODE_OFF &&
      ::NTV2DeviceCanDoCustomAnc(self->device_id)) {
    self->anc_buffer_pool = gst_buffer_pool_new();
    config = gst_buffer_pool_get_config(self->anc_buffer_pool);
    gst_buffer_pool_config_set_params(
        config, NULL, anc_buffer_size,
        (self->configured_info.interlace_mode ==
                 GST_VIDEO_INTERLACE_MODE_PROGRESSIVE
             ? 1
             : 2) *
            self->queue_size,
        0);
    gst_buffer_pool_config_set_allocator(config, self->allocator, NULL);
    gst_buffer_pool_set_config(self->anc_buffer_pool, config);
    gst_buffer_pool_set_active(self->anc_buffer_pool, TRUE);
  }

  gst_element_post_message(GST_ELEMENT_CAST(self),
                           gst_message_new_latency(GST_OBJECT_CAST(self)));

  return TRUE;
}

static gboolean gst_aja_src_start(GstAjaSrc *self) {
  GST_DEBUG_OBJECT(self, "Starting");

  self->video_format = NTV2_FORMAT_UNKNOWN;
  self->signal = FALSE;

  self->capture_thread = new AJAThread();
  self->capture_thread->Attach(capture_thread_func, self);
  self->capture_thread->SetPriority(AJA_ThreadPriority_High);
  self->capture_thread->Start();
  g_mutex_lock(&self->queue_lock);
  self->shutdown = FALSE;
  self->playing = FALSE;
  self->flushing = FALSE;
  g_cond_signal(&self->queue_cond);
  g_mutex_unlock(&self->queue_lock);

  return TRUE;
}

static gboolean gst_aja_src_stop(GstAjaSrc *self) {
  QueueItem *item;

  GST_DEBUG_OBJECT(self, "Stopping");

  g_mutex_lock(&self->queue_lock);
  self->shutdown = TRUE;
  self->flushing = TRUE;
  self->playing = FALSE;
  g_cond_signal(&self->queue_cond);
  g_mutex_unlock(&self->queue_lock);

  if (self->capture_thread) {
    self->capture_thread->Stop();
    delete self->capture_thread;
    self->capture_thread = NULL;
  }

  GST_OBJECT_LOCK(self);
  memset(&self->current_info, 0, sizeof(self->current_info));
  memset(&self->configured_info, 0, sizeof(self->configured_info));
  self->configured_audio_channels = 0;
  GST_OBJECT_UNLOCK(self);

  while ((item = (QueueItem *)gst_queue_array_pop_head_struct(self->queue))) {
    queue_item_clear(item);
  }
  self->queue_num_frames = 0;

  if (self->buffer_pool) {
    gst_buffer_pool_set_active(self->buffer_pool, FALSE);
    gst_clear_object(&self->buffer_pool);
  }

  if (self->audio_buffer_pool) {
    gst_buffer_pool_set_active(self->audio_buffer_pool, FALSE);
    gst_clear_object(&self->audio_buffer_pool);
  }

  if (self->anc_buffer_pool) {
    gst_buffer_pool_set_active(self->anc_buffer_pool, FALSE);
    gst_clear_object(&self->anc_buffer_pool);
  }

  self->video_format = NTV2_FORMAT_UNKNOWN;

  if (self->signal) {
    self->signal = FALSE;
    g_object_notify(G_OBJECT(self), "signal");
  }

  GST_DEBUG_OBJECT(self, "Stopped");

  return TRUE;
}

static GstStateChangeReturn gst_aja_src_change_state(
    GstElement *element, GstStateChange transition) {
  GstAjaSrc *self = GST_AJA_SRC(element);
  GstStateChangeReturn ret;

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      if (!gst_aja_src_open(self)) return GST_STATE_CHANGE_FAILURE;
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      if (!gst_aja_src_start(self)) return GST_STATE_CHANGE_FAILURE;
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS(parent_class)->change_state(element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE) return ret;

  switch (transition) {
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      g_mutex_lock(&self->queue_lock);
      self->playing = FALSE;
      g_cond_signal(&self->queue_cond);
      g_mutex_unlock(&self->queue_lock);
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      g_mutex_lock(&self->queue_lock);
      self->playing = TRUE;
      g_cond_signal(&self->queue_cond);
      g_mutex_unlock(&self->queue_lock);
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      if (!gst_aja_src_stop(self)) return GST_STATE_CHANGE_FAILURE;
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      if (!gst_aja_src_close(self)) return GST_STATE_CHANGE_FAILURE;
      break;
    default:
      break;
  }

  return ret;
}

static GstCaps *gst_aja_src_get_caps(GstBaseSrc *bsrc, GstCaps *filter) {
  GstAjaSrc *self = GST_AJA_SRC(bsrc);
  GstCaps *caps;

  if (self->device) {
    caps = gst_ntv2_supported_caps(self->device_id);
  } else {
    caps = gst_pad_get_pad_template_caps(GST_BASE_SRC_PAD(self));
  }

  // Intersect with the configured video format if any to constrain the caps
  // further.
  if (self->video_format_setting != GST_AJA_VIDEO_FORMAT_AUTO) {
    GstCaps *configured_caps =
        gst_aja_video_format_to_caps(self->video_format_setting);

    if (configured_caps) {
      GstCaps *tmp;

      // Remove pixel-aspect-ratio from the configured caps to allow for both
      // widescreen and non-widescreen PAL/NTSC. It's added back by the
      // template caps above when intersecting.
      guint n = gst_caps_get_size(configured_caps);
      for (guint i = 0; i < n; i++) {
        GstStructure *s = gst_caps_get_structure(configured_caps, i);

        gst_structure_remove_fields(s, "pixel-aspect-ratio", NULL);
      }

      tmp = gst_caps_intersect(caps, configured_caps);
      gst_caps_unref(caps);
      gst_caps_unref(configured_caps);
      caps = tmp;
    }
  }

  if (filter) {
    GstCaps *tmp =
        gst_caps_intersect_full(filter, caps, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref(caps);
    caps = tmp;
  }

  return caps;
}

static gboolean gst_aja_src_query(GstBaseSrc *bsrc, GstQuery *query) {
  GstAjaSrc *self = GST_AJA_SRC(bsrc);
  gboolean ret = TRUE;

  switch (GST_QUERY_TYPE(query)) {
    case GST_QUERY_LATENCY: {
      if (self->current_info.finfo &&
          self->current_info.finfo->format != GST_VIDEO_FORMAT_UNKNOWN) {
        GstClockTime min, max;

        min = gst_util_uint64_scale_ceil(
            GST_SECOND, 3 * self->current_info.fps_d, self->current_info.fps_n);
        max = self->queue_size * min;

        gst_query_set_latency(query, TRUE, min, max);
        ret = TRUE;
      } else {
        ret = FALSE;
      }

      return ret;
    }

    default:
      return GST_BASE_SRC_CLASS(parent_class)->query(bsrc, query);
      break;
  }
}

static gboolean gst_aja_src_unlock(GstBaseSrc *bsrc) {
  GstAjaSrc *self = GST_AJA_SRC(bsrc);

  g_mutex_lock(&self->queue_lock);
  self->flushing = TRUE;
  g_cond_signal(&self->queue_cond);
  g_mutex_unlock(&self->queue_lock);

  return TRUE;
}

static gboolean gst_aja_src_unlock_stop(GstBaseSrc *bsrc) {
  GstAjaSrc *self = GST_AJA_SRC(bsrc);

  g_mutex_lock(&self->queue_lock);
  self->flushing = FALSE;
  g_mutex_unlock(&self->queue_lock);

  return TRUE;
}

static GstFlowReturn gst_aja_src_create(GstPushSrc *psrc, GstBuffer **buffer) {
  GstAjaSrc *self = GST_AJA_SRC(psrc);
  GstFlowReturn flow_ret = GST_FLOW_OK;
  QueueItem item = {
      .type = QUEUE_ITEM_TYPE_DUMMY,
  };

next_item:
  item.type = QUEUE_ITEM_TYPE_DUMMY;

  g_mutex_lock(&self->queue_lock);
  while (gst_queue_array_is_empty(self->queue) && !self->flushing) {
    g_cond_wait(&self->queue_cond, &self->queue_lock);
  }

  if (self->flushing) {
    g_mutex_unlock(&self->queue_lock);
    GST_DEBUG_OBJECT(self, "Flushing");
    return GST_FLOW_FLUSHING;
  }

  item = *(QueueItem *)gst_queue_array_pop_head_struct(self->queue);
  if (item.type == QUEUE_ITEM_TYPE_FRAME) {
    self->queue_num_frames -= 1;
  }
  g_mutex_unlock(&self->queue_lock);

  switch (item.type) {
    case QUEUE_ITEM_TYPE_DUMMY:
      queue_item_clear(&item);
      goto next_item;
    case QUEUE_ITEM_TYPE_SIGNAL_CHANGE:
      // These are already only produced when signal status is changing
      if (item.signal_change.have_signal) {
        GST_ELEMENT_INFO(GST_ELEMENT(self), RESOURCE, READ,
                         ("Signal recovered"), ("Input source detected"));
        self->signal = TRUE;
        g_object_notify(G_OBJECT(self), "signal");
      } else if (!item.signal_change.have_signal) {
        if (item.signal_change.detected_format != ::NTV2_FORMAT_UNKNOWN) {
          std::string format_string =
              NTV2VideoFormatToString(item.signal_change.detected_format);

          GST_ELEMENT_WARNING_WITH_DETAILS(
              GST_ELEMENT(self), RESOURCE, READ, ("Signal lost"),
              ("Input source with different mode %s was detected",
               format_string.c_str()),
              ("detected-format", G_TYPE_STRING, format_string.c_str(), "vpid",
               G_TYPE_UINT, item.signal_change.vpid, NULL));
        } else {
          GST_ELEMENT_WARNING(GST_ELEMENT(self), RESOURCE, READ,
                              ("Signal lost"),
                              ("No input source was detected"));
        }
        self->signal = FALSE;
        g_object_notify(G_OBJECT(self), "signal");
      }
      queue_item_clear(&item);
      goto next_item;
    case QUEUE_ITEM_TYPE_ERROR:
      GST_ERROR_OBJECT(self, "Stopping because of error on capture thread");
      gst_element_post_message(GST_ELEMENT(self),
                               (GstMessage *)g_steal_pointer(&item.error.msg));
      queue_item_clear(&item);
      return GST_FLOW_ERROR;
    case QUEUE_ITEM_TYPE_FRAMES_DROPPED:
      GST_WARNING_OBJECT(
          self, "Dropped frames from %" GST_TIME_FORMAT " to %" GST_TIME_FORMAT,
          GST_TIME_ARGS(item.frames_dropped.timestamp_start),
          GST_TIME_ARGS(item.frames_dropped.timestamp_end));
      gst_element_post_message(
          GST_ELEMENT(self),
          gst_message_new_qos(GST_OBJECT_CAST(self), TRUE, GST_CLOCK_TIME_NONE,
                              GST_CLOCK_TIME_NONE,
                              item.frames_dropped.timestamp_start,
                              item.frames_dropped.timestamp_end -
                                  item.frames_dropped.timestamp_start));
      queue_item_clear(&item);
      goto next_item;
    case QUEUE_ITEM_TYPE_FRAME:
      // fall through below
      break;
  }

  g_assert(item.type == QUEUE_ITEM_TYPE_FRAME);

  if (!self->signal) {
    self->signal = TRUE;
    g_object_notify(G_OBJECT(self), "signal");
  }

  *buffer = (GstBuffer *)g_steal_pointer(&item.frame.video_buffer);
  gst_buffer_add_aja_audio_meta(*buffer, item.frame.audio_buffer);
  gst_clear_buffer(&item.frame.audio_buffer);

  if (item.frame.tc.IsValid()) {
    TimecodeFormat tc_format = ::kTCFormatUnknown;
    GstVideoTimeCodeFlags flags = GST_VIDEO_TIME_CODE_FLAGS_NONE;

    if (self->configured_info.fps_n == 24 && self->configured_info.fps_d == 1) {
      tc_format = kTCFormat24fps;
    } else if (self->configured_info.fps_n == 25 &&
               self->configured_info.fps_d == 1) {
      tc_format = kTCFormat25fps;
    } else if (self->configured_info.fps_n == 30 &&
               self->configured_info.fps_d == 1) {
      tc_format = kTCFormat30fps;
    } else if (self->configured_info.fps_n == 30000 &&
               self->configured_info.fps_d == 1001) {
      tc_format = kTCFormat30fpsDF;
      flags =
          (GstVideoTimeCodeFlags)(flags | GST_VIDEO_TIME_CODE_FLAGS_DROP_FRAME);
    } else if (self->configured_info.fps_n == 48 &&
               self->configured_info.fps_d == 1) {
      tc_format = kTCFormat48fps;
    } else if (self->configured_info.fps_n == 50 &&
               self->configured_info.fps_d == 1) {
      tc_format = kTCFormat50fps;
    } else if (self->configured_info.fps_n == 60 &&
               self->configured_info.fps_d == 1) {
      tc_format = kTCFormat60fps;
    } else if (self->configured_info.fps_n == 60000 &&
               self->configured_info.fps_d == 1001) {
      tc_format = kTCFormat60fpsDF;
      flags =
          (GstVideoTimeCodeFlags)(flags | GST_VIDEO_TIME_CODE_FLAGS_DROP_FRAME);
    }

    if (self->configured_info.interlace_mode !=
        GST_VIDEO_INTERLACE_MODE_PROGRESSIVE)
      flags =
          (GstVideoTimeCodeFlags)(flags | GST_VIDEO_TIME_CODE_FLAGS_INTERLACED);

    CRP188 rp188(item.frame.tc, tc_format);

    {
      std::stringstream os;
      os << rp188;
      GST_TRACE_OBJECT(self, "Adding timecode %s", os.str().c_str());
    }

    guint hours, minutes, seconds, frames;
    rp188.GetRP188Hrs(hours);
    rp188.GetRP188Mins(minutes);
    rp188.GetRP188Secs(seconds);
    rp188.GetRP188Frms(frames);

    GstVideoTimeCode tc;
    gst_video_time_code_init(&tc, self->configured_info.fps_n,
                             self->configured_info.fps_d, NULL, flags, hours,
                             minutes, seconds, frames, 0);
    gst_buffer_add_video_time_code_meta(*buffer, &tc);
  }

  AJAAncillaryList anc_packets;

  if (item.frame.anc_buffer) {
    GstMapInfo map = GST_MAP_INFO_INIT;
    GstMapInfo map2 = GST_MAP_INFO_INIT;

    gst_buffer_map(item.frame.anc_buffer, &map, GST_MAP_READ);
    if (item.frame.anc_buffer2)
      gst_buffer_map(item.frame.anc_buffer2, &map2, GST_MAP_READ);

    NTV2_POINTER ptr1(map.data, map.size);
    NTV2_POINTER ptr2(map2.data, map2.size);

    AJAAncillaryList::SetFromDeviceAncBuffers(ptr1, ptr2, anc_packets);

    if (item.frame.anc_buffer2) gst_buffer_unmap(item.frame.anc_buffer2, &map2);
    gst_buffer_unmap(item.frame.anc_buffer, &map);
  } else if (self->vanc_mode != ::NTV2_VANCMODE_OFF) {
    GstMapInfo map;

    NTV2FormatDescriptor format_desc(self->video_format, ::NTV2_FBF_10BIT_YCBCR,
                                     self->vanc_mode);

    gst_buffer_map(*buffer, &map, GST_MAP_READ);
    NTV2_POINTER ptr(map.data, map.size);
    AJAAncillaryList::SetFromVANCData(ptr, format_desc, anc_packets);
    gst_buffer_unmap(*buffer, &map);

    guint offset =
        format_desc.RasterLineToByteOffset(format_desc.GetFirstActiveLine());
    guint size = format_desc.GetVisibleRasterBytes();

    gst_buffer_resize(*buffer, offset, size);
  }

  gst_clear_buffer(&item.frame.anc_buffer);
  gst_clear_buffer(&item.frame.anc_buffer2);

  // Not using CountAncillaryDataWithType(AJAAncillaryDataType_Cea708) etc
  // here because for SD it doesn't recognize the packets. It assumes they
  // would only be received on AJAAncillaryDataChannel_Y but for SD it is
  // actually AJAAncillaryDataChannel_Both.
  //
  // See AJA SDK support ticket #4844.
  guint32 n_vanc_packets = anc_packets.CountAncillaryData();

  // Check if we have either CEA608 or CEA708 packets, or both.
  bool have_cea608 = false;
  bool have_cea708 = false;
  for (guint32 i = 0; i < n_vanc_packets; i++) {
    AJAAncillaryData *packet = anc_packets.GetAncillaryDataAtIndex(i);

    if (packet->GetDID() == AJAAncillaryData_Cea608_Vanc_DID &&
        packet->GetSID() == AJAAncillaryData_Cea608_Vanc_SID &&
        packet->GetPayloadData() && packet->GetPayloadByteCount() &&
        AJA_SUCCESS(packet->ParsePayloadData())) {
      GST_TRACE_OBJECT(
          self, "Found CEA608 VANC of %" G_GSIZE_FORMAT " bytes at line %u",
          packet->GetPayloadByteCount(), packet->GetLocationLineNumber());
      have_cea608 = true;
    } else if (packet->GetDID() == AJAAncillaryData_CEA708_DID &&
               packet->GetSID() == AJAAncillaryData_CEA708_SID &&
               packet->GetPayloadData() && packet->GetPayloadByteCount() &&
               AJA_SUCCESS(packet->ParsePayloadData())) {
      GST_TRACE_OBJECT(
          self, "Found CEA708 CDP VANC of %" G_GSIZE_FORMAT " bytes at line %u",
          packet->GetPayloadByteCount(), packet->GetLocationLineNumber());
      have_cea708 = true;
    }
  }

  // Decide based on the closed-caption-capture-mode property and closed
  // caption availability which ones to add as metadata to the output buffer.
  bool want_cea608 =
      have_cea608 &&
      (self->closed_caption_capture_mode ==
           GST_AJA_CLOSED_CAPTION_CAPTURE_MODE_CEA708_AND_CEA608 ||
       self->closed_caption_capture_mode ==
           GST_AJA_CLOSED_CAPTION_CAPTURE_MODE_CEA608_OR_CEA708 ||
       self->closed_caption_capture_mode ==
           GST_AJA_CLOSED_CAPTION_CAPTURE_MODE_CEA608_ONLY ||
       (!have_cea708 &&
        self->closed_caption_capture_mode ==
            GST_AJA_CLOSED_CAPTION_CAPTURE_MODE_CEA708_OR_CEA608));

  bool want_cea708 =
      have_cea708 &&
      (self->closed_caption_capture_mode ==
           GST_AJA_CLOSED_CAPTION_CAPTURE_MODE_CEA708_AND_CEA608 ||
       self->closed_caption_capture_mode ==
           GST_AJA_CLOSED_CAPTION_CAPTURE_MODE_CEA708_OR_CEA608 ||
       self->closed_caption_capture_mode ==
           GST_AJA_CLOSED_CAPTION_CAPTURE_MODE_CEA708_ONLY ||
       (!have_cea608 &&
        self->closed_caption_capture_mode ==
            GST_AJA_CLOSED_CAPTION_CAPTURE_MODE_CEA608_OR_CEA708));

  bool aspect_ratio_flag = false;
  bool have_afd_bar = false;
  for (guint32 i = 0; i < n_vanc_packets; i++) {
    AJAAncillaryData *packet = anc_packets.GetAncillaryDataAtIndex(i);

    if (want_cea608 && packet->GetDID() == AJAAncillaryData_Cea608_Vanc_DID &&
        packet->GetSID() == AJAAncillaryData_Cea608_Vanc_SID &&
        packet->GetPayloadData() && packet->GetPayloadByteCount() &&
        AJA_SUCCESS(packet->ParsePayloadData())) {
      GST_TRACE_OBJECT(
          self, "Adding CEA608 VANC of %" G_GSIZE_FORMAT " bytes at line %u",
          packet->GetPayloadByteCount(), packet->GetLocationLineNumber());
      gst_buffer_add_video_caption_meta(
          *buffer, GST_VIDEO_CAPTION_TYPE_CEA608_S334_1A,
          packet->GetPayloadData(), packet->GetPayloadByteCount());
    } else if (want_cea708 && packet->GetDID() == AJAAncillaryData_CEA708_DID &&
               packet->GetSID() == AJAAncillaryData_CEA708_SID &&
               packet->GetPayloadData() && packet->GetPayloadByteCount() &&
               AJA_SUCCESS(packet->ParsePayloadData())) {
      GST_TRACE_OBJECT(
          self,
          "Adding CEA708 CDP VANC of %" G_GSIZE_FORMAT " bytes at line %u",
          packet->GetPayloadByteCount(), packet->GetLocationLineNumber());
      gst_buffer_add_video_caption_meta(
          *buffer, GST_VIDEO_CAPTION_TYPE_CEA708_CDP, packet->GetPayloadData(),
          packet->GetPayloadByteCount());
    } else if (packet->GetDID() == 0x41 && packet->GetSID() == 0x05 &&
               packet->GetPayloadData() && packet->GetPayloadByteCount() == 8) {
      const guint8 *data = packet->GetPayloadData();

      have_afd_bar = true;
      aspect_ratio_flag = (data[0] >> 2) & 0x1;

      GstVideoAFDValue afd = (GstVideoAFDValue)((data[0] >> 3) & 0xf);
      gboolean is_letterbox = ((data[3] >> 4) & 0x3) == 0;
      guint16 bar1 = GST_READ_UINT16_BE(&data[4]);
      guint16 bar2 = GST_READ_UINT16_BE(&data[6]);

      GST_TRACE_OBJECT(self,
                       "Found AFD/Bar VANC at line %u: AR %u, AFD %u, "
                       "letterbox %u, bar1 %u, bar2 %u",
                       packet->GetLocationLineNumber(), aspect_ratio_flag, afd,
                       is_letterbox, bar1, bar2);

      const NTV2Standard standard(
          ::GetNTV2StandardFromVideoFormat(item.frame.detected_format));
      const NTV2SmpteLineNumber smpte_line_num_info =
          ::GetSmpteLineNumber(standard);
      bool field2 =
          packet->GetLocationLineNumber() >
          smpte_line_num_info.GetLastLine(
              smpte_line_num_info.firstFieldTop ? NTV2_FIELD0 : NTV2_FIELD1);

      gst_buffer_add_video_afd_meta(*buffer, field2 ? 1 : 0,
                                    GST_VIDEO_AFD_SPEC_SMPTE_ST2016_1, afd);
      gst_buffer_add_video_bar_meta(*buffer, field2 ? 1 : 0, is_letterbox, bar1,
                                    bar2);
    }

    // Don't attach other ANC as ancillary meta if not requested to do so.
    if (!self->attach_ancillary_meta) continue;

    // Skip non-SMPTE 291M ancillary data
    if (packet->GetDataCoding() != AJAAncillaryDataCoding_Digital) continue;

    const guint8 *in_data = packet->GetPayloadData();
    guint data_count = packet->GetDC();

    guint16 line_number = packet->GetLocationLineNumber();
    guint16 horiz_offset = packet->GetLocationHorizOffset();

    if (!in_data || data_count == 0) {
      GST_TRACE_OBJECT(self, "ANC %s (%04x) at (%u,%u) has no payload data",
                       packet->IDAsString().c_str(), packet->GetDIDSID(),
                       line_number, horiz_offset);
      continue;
    }

    GST_TRACE_OBJECT(self,
                     "Adding ANC meta for %s (%04x) at (%u,%u) of size %u",
                     packet->IDAsString().c_str(), packet->GetDIDSID(),
                     line_number, horiz_offset, data_count);
    GstAncillaryMeta *anc_meta = gst_buffer_add_ancillary_meta(*buffer);

    anc_meta->c_not_y_channel = packet->IsChromaChannel();
    anc_meta->line = line_number;
    anc_meta->offset = horiz_offset;

    packet->GeneratePayloadData();
    anc_meta->DID = AJAAncillaryData::AddEvenParity(packet->GetDID());
    anc_meta->SDID_block_number =
        AJAAncillaryData::AddEvenParity(packet->GetSID());

    anc_meta->data_count = AJAAncillaryData::AddEvenParity(data_count);

    guint16 *data = g_new(guint16, packet->GetDC());
    for (guint i = 0; i < data_count; i++) {
      data[i] = AJAAncillaryData::AddEvenParity(in_data[i]);
    }
    anc_meta->data = data;

    anc_meta->checksum = packet->Calculate9BitChecksum();
  }

  bool caps_changed = false;

  CNTV2VPID vpid(item.frame.vpid);
  if (vpid.IsValid()) {
    GstVideoInfo info;

    {
      std::stringstream os;
      vpid.Print(os);
      GST_TRACE_OBJECT(self, "Got valid VPID %s", os.str().c_str());
    }

    if (gst_video_info_from_ntv2_video_format(&info,
                                              item.frame.detected_format)) {
      switch (vpid.GetTransferCharacteristics()) {
        default:
        case NTV2_VPID_TC_SDR_TV:
          if (info.height < 720) {
            info.colorimetry.transfer = GST_VIDEO_TRANSFER_BT601;
          } else {
            info.colorimetry.transfer = GST_VIDEO_TRANSFER_BT709;
          }
          break;
        case NTV2_VPID_TC_HLG:
          info.colorimetry.transfer = GST_VIDEO_TRANSFER_ARIB_STD_B67;
          break;
        case NTV2_VPID_TC_PQ:
          info.colorimetry.transfer = GST_VIDEO_TRANSFER_SMPTE2084;
          break;
      }

      switch (vpid.GetColorimetry()) {
        case NTV2_VPID_Color_Rec709:
          info.colorimetry.matrix = GST_VIDEO_COLOR_MATRIX_BT709;
          info.colorimetry.primaries = GST_VIDEO_COLOR_PRIMARIES_BT709;
          break;
        case NTV2_VPID_Color_UHDTV:
          info.colorimetry.matrix = GST_VIDEO_COLOR_MATRIX_BT2020;
          info.colorimetry.primaries = GST_VIDEO_COLOR_PRIMARIES_BT2020;
          break;
        default:
          // Default handling
          break;
      }

      switch (vpid.GetRGBRange()) {
        case NTV2_VPID_Range_Full:
          info.colorimetry.range = GST_VIDEO_COLOR_RANGE_0_255;
          break;
        case NTV2_VPID_Range_Narrow:
          info.colorimetry.range = GST_VIDEO_COLOR_RANGE_16_235;
          break;
      }

      if (!have_afd_bar && vpid.GetImageAspect16x9()) aspect_ratio_flag = true;

      // Widescreen PAL/NTSC
      if (aspect_ratio_flag && info.height == 486) {
        info.par_n = 40;
        info.par_d = 33;
      } else if (aspect_ratio_flag && info.height == 576) {
        info.par_n = 16;
        info.par_d = 11;
      }

      if (!gst_pad_has_current_caps(GST_BASE_SRC_PAD(self)) ||
          !gst_video_info_is_equal(&info, &self->current_info)) {
        self->current_info = info;
        caps_changed = true;
      }
    }
  } else {
    GstVideoInfo info;

    if (gst_video_info_from_ntv2_video_format(&info,
                                              item.frame.detected_format)) {
      // Widescreen PAL/NTSC
      if (aspect_ratio_flag && info.height == 486) {
        info.par_n = 40;
        info.par_d = 33;
      } else if (aspect_ratio_flag && info.height == 576) {
        info.par_n = 16;
        info.par_d = 11;
      }

      if (!gst_pad_has_current_caps(GST_BASE_SRC_PAD(self)) ||
          !gst_video_info_is_equal(&info, &self->current_info)) {
        self->current_info = info;
        caps_changed = true;
      }
    } else if (!gst_pad_has_current_caps(GST_BASE_SRC_PAD(self))) {
      self->current_info = self->configured_info;

      // Widescreen PAL/NTSC
      if (aspect_ratio_flag && self->current_info.height == 486) {
        self->current_info.par_n = 40;
        self->current_info.par_d = 33;
      } else if (aspect_ratio_flag && self->current_info.height == 576) {
        self->current_info.par_n = 16;
        self->current_info.par_d = 11;
      }

      caps_changed = true;
    }
  }

  if (caps_changed) {
    GstCaps *caps = gst_video_info_to_caps(&self->current_info);
    gst_caps_set_simple(caps, "audio-channels", G_TYPE_INT,
                        self->configured_audio_channels, NULL);
    GST_DEBUG_OBJECT(self, "Configuring caps %" GST_PTR_FORMAT, caps);
    gst_base_src_set_caps(GST_BASE_SRC_CAST(self), caps);
    gst_caps_unref(caps);
  }

  if (self->configured_info.interlace_mode !=
      GST_VIDEO_INTERLACE_MODE_PROGRESSIVE) {
    GST_BUFFER_FLAG_SET(*buffer, GST_VIDEO_BUFFER_FLAG_INTERLACED);
    switch (GST_VIDEO_INFO_FIELD_ORDER(&self->configured_info)) {
      case GST_VIDEO_FIELD_ORDER_TOP_FIELD_FIRST:
        GST_BUFFER_FLAG_SET(*buffer, GST_VIDEO_BUFFER_FLAG_TFF);
      default:
        break;
    }
  }

  queue_item_clear(&item);

  GST_TRACE_OBJECT(self, "Outputting buffer %" GST_PTR_FORMAT, *buffer);

  return flow_ret;
}

#define AJA_SRC_ERROR(el, domain, code, text, debug)                        \
  G_STMT_START {                                                            \
    gchar *__txt = _gst_element_error_printf text;                          \
    gchar *__dbg = _gst_element_error_printf debug;                         \
    GstMessage *__msg;                                                      \
    GError *__err;                                                          \
    gchar *__name, *__fmt_dbg;                                              \
    if (__txt) GST_WARNING_OBJECT(el, "error: %s", __txt);                  \
    if (__dbg) GST_WARNING_OBJECT(el, "error: %s", __dbg);                  \
    if (!__txt)                                                             \
      __txt = gst_error_get_message(GST_##domain##_ERROR,                   \
                                    GST_##domain##_ERROR_##code);           \
    __err = g_error_new_literal(GST_##domain##_ERROR,                       \
                                GST_##domain##_ERROR_##code, __txt);        \
    __name = gst_object_get_path_string(GST_OBJECT_CAST(el));               \
    if (__dbg)                                                              \
      __fmt_dbg = g_strdup_printf("%s(%d): %s (): %s:\n%s", __FILE__,       \
                                  __LINE__, GST_FUNCTION, __name, __dbg);   \
    else                                                                    \
      __fmt_dbg = g_strdup_printf("%s(%d): %s (): %s", __FILE__, __LINE__,  \
                                  GST_FUNCTION, __name);                    \
    g_free(__name);                                                         \
    g_free(__dbg);                                                          \
    __msg = gst_message_new_error(GST_OBJECT(el), __err, __fmt_dbg);        \
    QueueItem item = {.type = QUEUE_ITEM_TYPE_ERROR, .error{.msg = __msg}}; \
    gst_queue_array_push_tail_struct(el->queue, &item);                     \
    g_cond_signal(&el->queue_cond);                                         \
  }                                                                         \
  G_STMT_END;

static void capture_thread_func(AJAThread *thread, void *data) {
  GstAjaSrc *self = GST_AJA_SRC(data);
  GstClock *clock = NULL;
  AUTOCIRCULATE_TRANSFER transfer;
  guint64 frames_dropped_last = G_MAXUINT64;
  gboolean have_signal = TRUE, discont = TRUE;
  guint iterations_without_frame = 0;
  NTV2VideoFormat last_detected_video_format = ::NTV2_FORMAT_UNKNOWN;

  if (self->capture_cpu_core != G_MAXUINT) {
    cpu_set_t mask;
    pthread_t current_thread = pthread_self();

    CPU_ZERO(&mask);
    CPU_SET(self->capture_cpu_core, &mask);

    if (pthread_setaffinity_np(current_thread, sizeof(mask), &mask) != 0) {
      GST_ERROR_OBJECT(self,
                       "Failed to set affinity for current thread to core %u",
                       self->capture_cpu_core);
    }
  }

  g_mutex_lock(&self->queue_lock);
restart:
  GST_DEBUG_OBJECT(self, "Waiting for playing or shutdown");
  while (!self->playing && !self->shutdown)
    g_cond_wait(&self->queue_cond, &self->queue_lock);
  if (self->shutdown) {
    GST_DEBUG_OBJECT(self, "Shutting down");
    g_mutex_unlock(&self->queue_lock);
    return;
  }

  GST_DEBUG_OBJECT(self, "Starting capture");
  g_mutex_unlock(&self->queue_lock);

  gst_clear_object(&clock);
  clock = gst_element_get_clock(GST_ELEMENT_CAST(self));

  frames_dropped_last = G_MAXUINT64;
  have_signal = TRUE;

  g_mutex_lock(&self->queue_lock);
  while (self->playing && !self->shutdown) {
    // If we don't have a video format configured, configure the device now
    // and potentially auto-detect the video format
    if (self->video_format == NTV2_FORMAT_UNKNOWN) {
      // Don't keep queue locked while configuring as this might take a while
      g_mutex_unlock(&self->queue_lock);

      // Make sure to globally lock here as the routing settings and others are
      // global shared state
      GstAjaNtv2DeviceLocker locker(self->device);

      if (!gst_aja_src_configure(self)) {
        g_mutex_lock(&self->queue_lock);
        AJA_SRC_ERROR(self, STREAM, FAILED, (NULL),
                      ("Failed to configure device"));
        goto out;
      }
      g_mutex_lock(&self->queue_lock);

      if (!self->playing || self->shutdown) goto restart;

      if (self->video_format == ::NTV2_FORMAT_UNKNOWN) {
        GST_DEBUG_OBJECT(self, "No signal, waiting");
        frames_dropped_last = G_MAXUINT64;
        if (have_signal) {
          QueueItem item = {
              .type = QUEUE_ITEM_TYPE_SIGNAL_CHANGE,
              .signal_change = {.have_signal = FALSE,
                                .detected_format = ::NTV2_FORMAT_UNKNOWN,
                                .vpid = 0}};
          gst_queue_array_push_tail_struct(self->queue, &item);
          g_cond_signal(&self->queue_cond);
          have_signal = FALSE;
          discont = TRUE;
        }
        self->device->device->WaitForInputVerticalInterrupt(self->channel);
        continue;
      }

      guint16 start_frame = self->start_frame;
      guint16 end_frame = self->end_frame;

      // If both are set to the same value, try to find that many unallocated
      // frames and use those.
      if (start_frame == end_frame) {
        gint assigned_start_frame = gst_aja_ntv2_device_find_unallocated_frames(
            self->device, self->channel, self->start_frame);

        if (assigned_start_frame == -1) {
          AJA_SRC_ERROR(self, STREAM, FAILED, (NULL),
                        ("Failed to allocate %u frames", start_frame));
          goto out;
        }

        start_frame = assigned_start_frame;
        end_frame = start_frame + self->start_frame - 1;
      }

      GST_DEBUG_OBJECT(
          self, "Configuring channel %u with start frame %u and end frame %u",
          self->channel, start_frame, end_frame);

      if (!self->device->device->AutoCirculateInitForInput(
              self->channel, 0, self->audio_system,
              (self->rp188 ? AUTOCIRCULATE_WITH_RP188 : 0) |
                  (self->vanc_mode == ::NTV2_VANCMODE_OFF
                       ? AUTOCIRCULATE_WITH_ANC
                       : 0),
              1, start_frame, end_frame)) {
        AJA_SRC_ERROR(self, STREAM, FAILED, (NULL),
                      ("Failed to initialize autocirculate"));
        goto out;
      }

      self->device->device->AutoCirculateStart(self->channel);
    }

    // Check for valid signal first
    NTV2VideoFormat current_video_format =
        self->device->device->GetInputVideoFormat(
            self->configured_input_source);

    bool all_quads_equal = true;
    if (self->quad_mode) {
      for (int i = 1; i < 4; i++) {
        NTV2VideoFormat other_video_format =
            self->device->device->GetInputVideoFormat(
                (NTV2InputSource)(self->configured_input_source + i));
        if (other_video_format != current_video_format) {
          std::string current_string =
              NTV2VideoFormatToString(current_video_format);
          std::string other_string =
              NTV2VideoFormatToString(other_video_format);
          GST_DEBUG_OBJECT(
              self,
              "Not all quadrants had the same format in "
              "quad-link-mode: %s (%d) on input 1 vs. %s (%d) on input %d",
              current_string.c_str(), current_video_format,
              other_string.c_str(), other_video_format, i + 1);
          all_quads_equal = false;
          break;
        }
      }
    }

    ULWord vpid_a = 0;
    ULWord vpid_b = 0;
    self->device->device->ReadSDIInVPID(self->channel, vpid_a, vpid_b);

    {
      std::string current_string =
          NTV2VideoFormatToString(current_video_format);
      GST_TRACE_OBJECT(
          self, "Detected input video format %s (%d) with VPID %08x / %08x",
          current_string.c_str(), (int)current_video_format, vpid_a, vpid_b);
    }

    NTV2VideoFormat effective_video_format = self->video_format;
    // Can't call this unconditionally as it also maps e.g. 3840x2160p to 1080p
    if (self->quad_mode) {
      effective_video_format =
          ::GetQuarterSizedVideoFormat(effective_video_format);
    }
    switch (self->video_format) {
      case NTV2_FORMAT_1080psf_2500_2:
        if (current_video_format == NTV2_FORMAT_1080i_5000)
          current_video_format = NTV2_FORMAT_1080psf_2500_2;
        break;
      case NTV2_FORMAT_1080psf_2997_2:
        if (current_video_format == NTV2_FORMAT_1080i_5994)
          current_video_format = NTV2_FORMAT_1080psf_2997_2;
        break;
      case NTV2_FORMAT_1080psf_3000_2:
        if (current_video_format == NTV2_FORMAT_1080i_6000)
          current_video_format = NTV2_FORMAT_1080psf_3000_2;
        break;
      default:
        break;
    }

    if (current_video_format == ::NTV2_FORMAT_UNKNOWN || !all_quads_equal) {
      if (self->video_format_setting == GST_AJA_VIDEO_FORMAT_AUTO)
        self->video_format = NTV2_FORMAT_UNKNOWN;

      GST_DEBUG_OBJECT(self, "No signal, waiting");
      g_mutex_unlock(&self->queue_lock);
      frames_dropped_last = G_MAXUINT64;
      if (have_signal) {
        QueueItem item = {
            .type = QUEUE_ITEM_TYPE_SIGNAL_CHANGE,
            .signal_change = {.have_signal = FALSE,
                              .detected_format = ::NTV2_FORMAT_UNKNOWN,
                              .vpid = 0}};
        last_detected_video_format = ::NTV2_FORMAT_UNKNOWN;
        gst_queue_array_push_tail_struct(self->queue, &item);
        g_cond_signal(&self->queue_cond);
        have_signal = FALSE;
        discont = TRUE;
      }
      self->device->device->WaitForInputVerticalInterrupt(self->channel);
      g_mutex_lock(&self->queue_lock);
      continue;
    } else if (current_video_format != effective_video_format &&
               current_video_format != self->video_format) {
      // Try reconfiguring with the newly detected video format
      if (self->video_format_setting == GST_AJA_VIDEO_FORMAT_AUTO) {
        self->video_format = NTV2_FORMAT_UNKNOWN;
        continue;
      }

      std::string current_string =
          NTV2VideoFormatToString(current_video_format);
      std::string configured_string =
          NTV2VideoFormatToString(self->video_format);
      std::string effective_string =
          NTV2VideoFormatToString(effective_video_format);

      GST_DEBUG_OBJECT(self,
                       "Different input format %s than configured %s "
                       "(effective %s), waiting",
                       current_string.c_str(), configured_string.c_str(),
                       effective_string.c_str());
      g_mutex_unlock(&self->queue_lock);
      frames_dropped_last = G_MAXUINT64;
      if (have_signal || current_video_format != last_detected_video_format) {
        QueueItem item = {
            .type = QUEUE_ITEM_TYPE_SIGNAL_CHANGE,
            .signal_change = {.have_signal = FALSE,
                              .detected_format = current_video_format,
                              .vpid = vpid_a}};
        last_detected_video_format = current_video_format;
        gst_queue_array_push_tail_struct(self->queue, &item);
        g_cond_signal(&self->queue_cond);
        have_signal = FALSE;
        discont = TRUE;
      }
      self->device->device->WaitForInputVerticalInterrupt(self->channel);
      g_mutex_lock(&self->queue_lock);
      continue;
    }

    AUTOCIRCULATE_STATUS status;

    self->device->device->AutoCirculateGetStatus(self->channel, status);

    GST_TRACE_OBJECT(self,
                     "Start frame %d "
                     "end frame %d "
                     "active frame %d "
                     "start time %" G_GUINT64_FORMAT
                     " "
                     "current time %" G_GUINT64_FORMAT
                     " "
                     "frames processed %u "
                     "frames dropped %u "
                     "buffer level %u",
                     status.acStartFrame, status.acEndFrame,
                     status.acActiveFrame, status.acRDTSCStartTime,
                     status.acRDTSCCurrentTime, status.acFramesProcessed,
                     status.acFramesDropped, status.acBufferLevel);

    if (frames_dropped_last == G_MAXUINT64) {
      frames_dropped_last = status.acFramesDropped;
    } else if (frames_dropped_last < status.acFramesDropped) {
      GST_WARNING_OBJECT(self, "Dropped %" G_GUINT64_FORMAT " frames",
                         status.acFramesDropped - frames_dropped_last);

      GstClockTime timestamp =
          gst_util_uint64_scale(status.acFramesProcessed + frames_dropped_last,
                                self->configured_info.fps_n,
                                self->configured_info.fps_d * GST_SECOND);
      GstClockTime timestamp_end = gst_util_uint64_scale(
          status.acFramesProcessed + status.acFramesDropped,
          self->configured_info.fps_n,
          self->configured_info.fps_d * GST_SECOND);

      QueueItem item = {.type = QUEUE_ITEM_TYPE_FRAMES_DROPPED,
                        .frames_dropped = {.driver_side = TRUE,
                                           .timestamp_start = timestamp,
                                           .timestamp_end = timestamp_end}};
      gst_queue_array_push_tail_struct(self->queue, &item);
      g_cond_signal(&self->queue_cond);

      frames_dropped_last = status.acFramesDropped;
      discont = TRUE;
    }

    if (status.IsRunning() && status.acBufferLevel > 1) {
      GstBuffer *video_buffer = NULL;
      GstBuffer *audio_buffer = NULL;
      GstBuffer *anc_buffer = NULL, *anc_buffer2 = NULL;
      GstMapInfo video_map = GST_MAP_INFO_INIT;
      GstMapInfo audio_map = GST_MAP_INFO_INIT;
      GstMapInfo anc_map = GST_MAP_INFO_INIT;
      GstMapInfo anc_map2 = GST_MAP_INFO_INIT;
      AUTOCIRCULATE_TRANSFER transfer;

      if (!have_signal) {
        QueueItem item = {
            .type = QUEUE_ITEM_TYPE_SIGNAL_CHANGE,
            .signal_change = {.have_signal = TRUE,
                              .detected_format = current_video_format,
                              .vpid = vpid_a}};
        gst_queue_array_push_tail_struct(self->queue, &item);
        g_cond_signal(&self->queue_cond);
        have_signal = TRUE;
      }

      iterations_without_frame = 0;

      if (gst_buffer_pool_acquire_buffer(self->buffer_pool, &video_buffer,
                                         NULL) != GST_FLOW_OK) {
        AJA_SRC_ERROR(self, STREAM, FAILED, (NULL),
                      ("Failed to acquire video buffer"));
        break;
      }

      if (gst_buffer_pool_acquire_buffer(self->audio_buffer_pool, &audio_buffer,
                                         NULL) != GST_FLOW_OK) {
        gst_buffer_unref(video_buffer);
        AJA_SRC_ERROR(self, STREAM, FAILED, (NULL),
                      ("Failed to acquire audio buffer"));
        break;
      }

      if (self->vanc_mode == ::NTV2_VANCMODE_OFF &&
          ::NTV2DeviceCanDoCustomAnc(self->device_id)) {
        if (gst_buffer_pool_acquire_buffer(self->anc_buffer_pool, &anc_buffer,
                                           NULL) != GST_FLOW_OK) {
          gst_buffer_unref(audio_buffer);
          gst_buffer_unref(video_buffer);
          AJA_SRC_ERROR(self, STREAM, FAILED, (NULL),
                        ("Failed to acquire anc buffer"));
          break;
        }

        if (self->configured_info.interlace_mode !=
            GST_VIDEO_INTERLACE_MODE_PROGRESSIVE) {
          if (gst_buffer_pool_acquire_buffer(
                  self->anc_buffer_pool, &anc_buffer2, NULL) != GST_FLOW_OK) {
            gst_buffer_unref(anc_buffer);
            gst_buffer_unref(audio_buffer);
            gst_buffer_unref(video_buffer);
            AJA_SRC_ERROR(self, STREAM, FAILED, (NULL),
                          ("Failed to acquire anc buffer"));
            break;
          }
        }
      }

      gst_buffer_map(video_buffer, &video_map, GST_MAP_READWRITE);
      gst_buffer_map(audio_buffer, &audio_map, GST_MAP_READWRITE);
      if (anc_buffer) gst_buffer_map(anc_buffer, &anc_map, GST_MAP_READWRITE);
      if (anc_buffer2)
        gst_buffer_map(anc_buffer2, &anc_map2, GST_MAP_READWRITE);

      transfer.acFrameBufferFormat = ::NTV2_FBF_10BIT_YCBCR;

      transfer.SetVideoBuffer((ULWord *)video_map.data, video_map.size);
      transfer.SetAudioBuffer((ULWord *)audio_map.data, audio_map.size);
      transfer.SetAncBuffers((ULWord *)anc_map.data, anc_map.size,
                             (ULWord *)anc_map2.data, anc_map2.size);

      g_mutex_unlock(&self->queue_lock);

      bool transfered = true;
      if (!self->device->device->AutoCirculateTransfer(self->channel,
                                                       transfer)) {
        GST_WARNING_OBJECT(self, "Failed to transfer frame");
        transfered = false;
      }

      if (anc_buffer2) gst_buffer_unmap(anc_buffer2, &anc_map2);
      if (anc_buffer) gst_buffer_unmap(anc_buffer, &anc_map);
      gst_buffer_unmap(audio_buffer, &audio_map);
      gst_buffer_unmap(video_buffer, &video_map);

      g_mutex_lock(&self->queue_lock);

      if (!transfered) {
        gst_clear_buffer(&anc_buffer2);
        gst_clear_buffer(&anc_buffer);
        gst_clear_buffer(&audio_buffer);
        gst_clear_buffer(&video_buffer);
        continue;
      }

      gst_buffer_set_size(audio_buffer, transfer.GetCapturedAudioByteCount());
      if (anc_buffer)
        gst_buffer_set_size(anc_buffer,
                            transfer.GetCapturedAncByteCount(false));
      if (anc_buffer2)
        gst_buffer_set_size(anc_buffer2,
                            transfer.GetCapturedAncByteCount(true));

      NTV2TCIndex tc_index;
      switch (self->timecode_index) {
        case GST_AJA_TIMECODE_INDEX_VITC:
          tc_index = ::NTV2InputSourceToTimecodeIndex(
              self->configured_input_source, true);
          break;
        case GST_AJA_TIMECODE_INDEX_ATC_LTC:
          tc_index = ::NTV2InputSourceToTimecodeIndex(
              self->configured_input_source, false);
          break;
        case GST_AJA_TIMECODE_INDEX_LTC1:
          tc_index = ::NTV2_TCINDEX_LTC1;
          break;
        case GST_AJA_TIMECODE_INDEX_LTC2:
          tc_index = ::NTV2_TCINDEX_LTC2;
          break;
        default:
          g_assert_not_reached();
          break;
      }

      NTV2_RP188 time_code;
      transfer.acTransferStatus.acFrameStamp.GetInputTimeCode(time_code,
                                                              tc_index);

      gint64 frame_time = transfer.acTransferStatus.acFrameStamp.acFrameTime;
      gint64 now_sys = g_get_real_time();
      GstClockTime now_gst = gst_clock_get_time(clock);
      if (now_sys * 10 > frame_time) {
        GstClockTime diff = now_sys * 1000 - frame_time * 100;
        if (now_gst > diff)
          now_gst -= diff;
        else
          now_gst = 0;
      }

      GstClockTime base_time =
          gst_element_get_base_time(GST_ELEMENT_CAST(self));
      if (now_gst > base_time)
        now_gst -= base_time;
      else
        now_gst = 0;

      // TODO: Drift detection and compensation
      GST_BUFFER_PTS(video_buffer) = now_gst;
      GST_BUFFER_DURATION(video_buffer) = gst_util_uint64_scale(
          GST_SECOND, self->configured_info.fps_d, self->configured_info.fps_n);
      GST_BUFFER_PTS(audio_buffer) = now_gst;
      GST_BUFFER_DURATION(audio_buffer) = gst_util_uint64_scale(
          GST_SECOND, self->configured_info.fps_d, self->configured_info.fps_n);

      while (self->queue_num_frames >= self->queue_size) {
        guint n = gst_queue_array_get_length(self->queue);

        for (guint i = 0; i < n; i++) {
          QueueItem *tmp =
              (QueueItem *)gst_queue_array_peek_nth_struct(self->queue, i);
          if (tmp->type == QUEUE_ITEM_TYPE_FRAME) {
            GST_WARNING_OBJECT(self,
                               "Element queue overrun, dropping old frame");

            QueueItem item = {
                .type = QUEUE_ITEM_TYPE_FRAMES_DROPPED,
                .frames_dropped = {
                    .driver_side = FALSE,
                    .timestamp_start = tmp->frame.capture_time,
                    .timestamp_end =
                        tmp->frame.capture_time +
                        gst_util_uint64_scale(GST_SECOND,
                                              self->configured_info.fps_d,
                                              self->configured_info.fps_n)}};
            queue_item_clear(tmp);
            gst_queue_array_drop_struct(self->queue, i, NULL);
            gst_queue_array_push_tail_struct(self->queue, &item);
            self->queue_num_frames -= 1;
            discont = TRUE;
            g_cond_signal(&self->queue_cond);
            break;
          }
        }
      }

      if (discont) {
        GST_BUFFER_FLAG_SET(video_buffer, GST_BUFFER_FLAG_DISCONT);
        GST_BUFFER_FLAG_SET(audio_buffer, GST_BUFFER_FLAG_DISCONT);
        discont = FALSE;
      }

      QueueItem item = {
          .type = QUEUE_ITEM_TYPE_FRAME,
          .frame = {.capture_time = now_gst,
                    .video_buffer = video_buffer,
                    .audio_buffer = audio_buffer,
                    .anc_buffer = anc_buffer,
                    .anc_buffer2 = anc_buffer2,
                    .tc = time_code,
                    .detected_format =
                        (self->quad_mode
                             ? ::GetQuadSizedVideoFormat(current_video_format)
                             : current_video_format),
                    .vpid = vpid_a}};

      GST_TRACE_OBJECT(self, "Queuing frame %" GST_TIME_FORMAT,
                       GST_TIME_ARGS(now_gst));
      gst_queue_array_push_tail_struct(self->queue, &item);
      self->queue_num_frames += 1;
      GST_TRACE_OBJECT(self, "%u frames queued", self->queue_num_frames);
      g_cond_signal(&self->queue_cond);
    } else {
      g_mutex_unlock(&self->queue_lock);

      // If we don't have a frame for 32 iterations (512ms) then consider
      // this as signal loss too even if the driver still reports the
      // expected mode above
      if (have_signal && iterations_without_frame < 32) {
        iterations_without_frame++;
      } else {
        frames_dropped_last = G_MAXUINT64;
        if (have_signal || last_detected_video_format != current_video_format) {
          QueueItem item = {
              .type = QUEUE_ITEM_TYPE_SIGNAL_CHANGE,
              .signal_change = {.have_signal = TRUE,
                                .detected_format = current_video_format,
                                .vpid = vpid_a}};
          last_detected_video_format = current_video_format;
          gst_queue_array_push_tail_struct(self->queue, &item);
          g_cond_signal(&self->queue_cond);
          have_signal = FALSE;
          discont = TRUE;
        }
      }

      self->device->device->WaitForInputVerticalInterrupt(self->channel);

      g_mutex_lock(&self->queue_lock);
    }
  }

out: {
  // Make sure to globally lock here as the routing settings and others are
  // global shared state
  GstAjaNtv2DeviceLocker locker(self->device);

  self->device->device->AutoCirculateStop(self->channel);
  self->device->device->UnsubscribeInputVerticalEvent(self->channel);
  self->device->device->DisableInputInterrupt(self->channel);

  self->device->device->DisableChannel(self->channel);
  if (self->quad_mode) {
    for (int i = 1; i < 4; i++) {
      self->device->device->DisableChannel((NTV2Channel)(self->channel + i));
    }
  }
}

  if (!self->playing && !self->shutdown) goto restart;
  g_mutex_unlock(&self->queue_lock);

  gst_clear_object(&clock);

  GST_DEBUG_OBJECT(self, "Stopped");
}
