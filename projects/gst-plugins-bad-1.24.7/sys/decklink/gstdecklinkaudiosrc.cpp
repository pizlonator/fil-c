/* GStreamer
 * Copyright (C) 2011 David Schleef <ds@entropywave.com>
 * Copyright (C) 2014 Sebastian Dröge <sebastian@centricular.com>
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
 * SECTION:element-decklinkaudiosrc
 * @short_description: Inputs Audio from a BlackMagic DeckLink Device
 * @see_also: decklinkvideosrc
 *
 * Capture Video and Audio from a BlackMagic DeckLink Device. Can only be used
 * in conjunction with decklinkvideosink.
 *
 * ## Sample pipeline
 * |[
 * gst-launch-1.0 \
 *   decklinkvideosrc device-number=0 mode=1080p25 ! autovideosink \
 *   decklinkaudiosrc device-number=0 ! autoaudiosink
 * ]|
 * Capturing 1080p25 video and audio from the SDI-In of Card 0. Devices are numbered
 * starting with 0.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstdecklinkaudiosrc.h"
#include "gstdecklinkvideosrc.h"
#include <string.h>

GST_DEBUG_CATEGORY_STATIC (gst_decklink_audio_src_debug);
#define GST_CAT_DEFAULT gst_decklink_audio_src_debug

#define DEFAULT_CONNECTION            (GST_DECKLINK_AUDIO_CONNECTION_AUTO)
#define DEFAULT_BUFFER_SIZE           (5)

#define DEFAULT_ALIGNMENT_THRESHOLD   (40 * GST_MSECOND)
#define DEFAULT_DISCONT_WAIT          (1 * GST_SECOND)
#define DEFAULT_CHANNELS              (GST_DECKLINK_AUDIO_CHANNELS_2)

#ifndef ABSDIFF
#define ABSDIFF(x, y) ( (x) > (y) ? ((x) - (y)) : ((y) - (x)) )
#endif

#define DEFAULT_PERSISTENT_ID (-1)

enum
{
  PROP_0,
  PROP_CONNECTION,
  PROP_DEVICE_NUMBER,
  PROP_ALIGNMENT_THRESHOLD,
  PROP_DISCONT_WAIT,
  PROP_BUFFER_SIZE,
  PROP_CHANNELS,
  PROP_HW_SERIAL_NUMBER,
  PROP_PERSISTENT_ID
};

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS
    ("audio/x-raw, format={S16LE,S32LE}, channels=2, rate=48000, "
        "layout=interleaved;"
        "audio/x-raw, format={S16LE,S32LE}, channels={8,16}, channel-mask=(bitmask)0, rate=48000, "
        "layout=interleaved")
    );

typedef struct
{
  IDeckLinkAudioInputPacket *packet;
  GstClockTime timestamp;
  GstClockTime stream_timestamp;
  GstClockTime stream_duration;
  GstClockTime hardware_timestamp;
  GstClockTime hardware_duration;
  gboolean no_signal;
} CapturePacket;

static void
capture_packet_clear (CapturePacket * packet)
{
  packet->packet->Release ();
  memset (packet, 0, sizeof (*packet));
}

typedef struct
{
  IDeckLinkAudioInputPacket *packet;
  IDeckLinkInput *input;
} AudioPacket;

static void
audio_packet_free (void *data)
{
  AudioPacket *packet = (AudioPacket *) data;

  packet->packet->Release ();
  packet->input->Release ();
  g_free (packet);
}

static void gst_decklink_audio_src_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_decklink_audio_src_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_decklink_audio_src_finalize (GObject * object);

static GstStateChangeReturn
gst_decklink_audio_src_change_state (GstElement * element,
    GstStateChange transition);

static gboolean gst_decklink_audio_src_unlock (GstBaseSrc * bsrc);
static gboolean gst_decklink_audio_src_unlock_stop (GstBaseSrc * bsrc);
static GstCaps *gst_decklink_audio_src_get_caps (GstBaseSrc * bsrc,
    GstCaps * filter);
static gboolean gst_decklink_audio_src_query (GstBaseSrc * bsrc,
    GstQuery * query);

static GstFlowReturn gst_decklink_audio_src_create (GstPushSrc * psrc,
    GstBuffer ** buffer);

static gboolean gst_decklink_audio_src_open (GstDecklinkAudioSrc * self);
static gboolean gst_decklink_audio_src_close (GstDecklinkAudioSrc * self);

static gboolean gst_decklink_audio_src_stop (GstDecklinkAudioSrc * self);

#define parent_class gst_decklink_audio_src_parent_class
G_DEFINE_TYPE (GstDecklinkAudioSrc, gst_decklink_audio_src, GST_TYPE_PUSH_SRC);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (decklinkaudiosrc, "decklinkaudiosrc", GST_RANK_NONE,
    GST_TYPE_DECKLINK_AUDIO_SRC, decklink_element_init (plugin));

static void
gst_decklink_audio_src_class_init (GstDecklinkAudioSrcClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstBaseSrcClass *basesrc_class = GST_BASE_SRC_CLASS (klass);
  GstPushSrcClass *pushsrc_class = GST_PUSH_SRC_CLASS (klass);

  gobject_class->set_property = gst_decklink_audio_src_set_property;
  gobject_class->get_property = gst_decklink_audio_src_get_property;
  gobject_class->finalize = gst_decklink_audio_src_finalize;

  element_class->change_state =
      GST_DEBUG_FUNCPTR (gst_decklink_audio_src_change_state);

  basesrc_class->query = GST_DEBUG_FUNCPTR (gst_decklink_audio_src_query);
  basesrc_class->negotiate = NULL;
  basesrc_class->get_caps = GST_DEBUG_FUNCPTR (gst_decklink_audio_src_get_caps);
  basesrc_class->unlock = GST_DEBUG_FUNCPTR (gst_decklink_audio_src_unlock);
  basesrc_class->unlock_stop =
      GST_DEBUG_FUNCPTR (gst_decklink_audio_src_unlock_stop);

  pushsrc_class->create = GST_DEBUG_FUNCPTR (gst_decklink_audio_src_create);

  g_object_class_install_property (gobject_class, PROP_CONNECTION,
      g_param_spec_enum ("connection", "Connection",
          "Audio input connection to use",
          GST_TYPE_DECKLINK_AUDIO_CONNECTION, DEFAULT_CONNECTION,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              G_PARAM_CONSTRUCT)));

  g_object_class_install_property (gobject_class, PROP_DEVICE_NUMBER,
      g_param_spec_int ("device-number", "Device number",
          "Output device instance to use", 0, G_MAXINT, 0,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              G_PARAM_CONSTRUCT)));

    /**
   * GstDecklinkAudioSrc:persistent-id
   *
   * Decklink device to use. Higher priority than "device-number".
   * BMDDeckLinkPersistentID is a device speciﬁc, 32-bit unique identiﬁer.
   * It is stable even when the device is plugged in a diﬀerent connector,
   * across reboots, and when plugged into diﬀerent computers.
   *
   * Since: 1.22
   */
  g_object_class_install_property (gobject_class, PROP_PERSISTENT_ID,
      g_param_spec_int64 ("persistent-id", "Persistent id",
          "Output device instance to use. Higher priority than \"device-number\".",
          DEFAULT_PERSISTENT_ID, G_MAXINT64, DEFAULT_PERSISTENT_ID,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              G_PARAM_CONSTRUCT)));

  g_object_class_install_property (gobject_class, PROP_ALIGNMENT_THRESHOLD,
      g_param_spec_uint64 ("alignment-threshold", "Alignment Threshold",
          "Timestamp alignment threshold in nanoseconds", 0,
          G_MAXUINT64 - 1, DEFAULT_ALIGNMENT_THRESHOLD,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_DISCONT_WAIT,
      g_param_spec_uint64 ("discont-wait", "Discont Wait",
          "Window of time in nanoseconds to wait before "
          "creating a discontinuity", 0,
          G_MAXUINT64 - 1, DEFAULT_DISCONT_WAIT,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_BUFFER_SIZE,
      g_param_spec_uint ("buffer-size", "Buffer Size",
          "Size of internal buffer in number of video frames", 1,
          G_MAXINT, DEFAULT_BUFFER_SIZE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_CHANNELS,
      g_param_spec_enum ("channels", "Channels",
          "Audio channels",
          GST_TYPE_DECKLINK_AUDIO_CHANNELS, DEFAULT_CHANNELS,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              G_PARAM_CONSTRUCT)));

  g_object_class_install_property (gobject_class, PROP_HW_SERIAL_NUMBER,
      g_param_spec_string ("hw-serial-number", "Hardware serial number",
          "The serial number (hardware ID) of the Decklink card",
          NULL, (GParamFlags) (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)));

  gst_element_class_add_static_pad_template (element_class, &sink_template);

  gst_element_class_set_static_metadata (element_class, "Decklink Audio Source",
      "Audio/Source/Hardware", "Decklink Source",
      "David Schleef <ds@entropywave.com>, "
      "Sebastian Dröge <sebastian@centricular.com>");

  GST_DEBUG_CATEGORY_INIT (gst_decklink_audio_src_debug, "decklinkaudiosrc",
      0, "debug category for decklinkaudiosrc element");
}

static void
gst_decklink_audio_src_init (GstDecklinkAudioSrc * self)
{
  self->device_number = 0;
  self->alignment_threshold = DEFAULT_ALIGNMENT_THRESHOLD;
  self->discont_wait = DEFAULT_DISCONT_WAIT;
  self->buffer_size = DEFAULT_BUFFER_SIZE;
  self->channels = DEFAULT_CHANNELS;

  gst_base_src_set_live (GST_BASE_SRC (self), TRUE);
  gst_base_src_set_format (GST_BASE_SRC (self), GST_FORMAT_TIME);

  gst_pad_use_fixed_caps (GST_BASE_SRC_PAD (self));

  g_mutex_init (&self->lock);
  g_cond_init (&self->cond);

  self->current_packets =
      gst_queue_array_new_for_struct (sizeof (CapturePacket),
      DEFAULT_BUFFER_SIZE);

  self->skipped_last = 0;
  self->skip_from_timestamp = GST_CLOCK_TIME_NONE;
  self->skip_to_timestamp = GST_CLOCK_TIME_NONE;

  self->persistent_id = DEFAULT_PERSISTENT_ID;
}

void
gst_decklink_audio_src_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstDecklinkAudioSrc *self = GST_DECKLINK_AUDIO_SRC_CAST (object);

  switch (property_id) {
    case PROP_CONNECTION:
      self->connection =
          (GstDecklinkAudioConnectionEnum) g_value_get_enum (value);
      break;
    case PROP_DEVICE_NUMBER:
      self->device_number = g_value_get_int (value);
      break;
    case PROP_ALIGNMENT_THRESHOLD:
      self->alignment_threshold = g_value_get_uint64 (value);
      break;
    case PROP_DISCONT_WAIT:
      self->discont_wait = g_value_get_uint64 (value);
      break;
    case PROP_BUFFER_SIZE:
      self->buffer_size = g_value_get_uint (value);
      break;
    case PROP_CHANNELS:
      self->channels = (GstDecklinkAudioChannelsEnum) g_value_get_enum (value);
      break;
    case PROP_PERSISTENT_ID:
      self->persistent_id = g_value_get_int64 (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_decklink_audio_src_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstDecklinkAudioSrc *self = GST_DECKLINK_AUDIO_SRC_CAST (object);

  switch (property_id) {
    case PROP_CONNECTION:
      g_value_set_enum (value, self->connection);
      break;
    case PROP_DEVICE_NUMBER:
      g_value_set_int (value, self->device_number);
      break;
    case PROP_ALIGNMENT_THRESHOLD:
      g_value_set_uint64 (value, self->alignment_threshold);
      break;
    case PROP_DISCONT_WAIT:
      g_value_set_uint64 (value, self->discont_wait);
      break;
    case PROP_BUFFER_SIZE:
      g_value_set_uint (value, self->buffer_size);
      break;
    case PROP_CHANNELS:
      g_value_set_enum (value, self->channels);
      break;
    case PROP_HW_SERIAL_NUMBER:
      if (self->input)
        g_value_set_string (value, self->input->hw_serial_number);
      else
        g_value_set_string (value, NULL);
      break;
    case PROP_PERSISTENT_ID:
      g_value_set_int64 (value, self->persistent_id);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_decklink_audio_src_finalize (GObject * object)
{
  GstDecklinkAudioSrc *self = GST_DECKLINK_AUDIO_SRC_CAST (object);

  g_mutex_clear (&self->lock);
  g_cond_clear (&self->cond);
  if (self->current_packets) {
    while (gst_queue_array_get_length (self->current_packets) > 0) {
      CapturePacket *tmp = (CapturePacket *)
          gst_queue_array_pop_head_struct (self->current_packets);
      capture_packet_clear (tmp);
    }
    gst_queue_array_free (self->current_packets);
    self->current_packets = NULL;
  }

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean
gst_decklink_audio_src_start (GstDecklinkAudioSrc * self)
{
  BMDAudioSampleType sample_depth;
  HRESULT ret;
  BMDAudioConnection conn = (BMDAudioConnection) - 1;
  GstCaps *allowed_caps, *caps;

  g_mutex_lock (&self->input->lock);
  if (self->input->audio_enabled) {
    g_mutex_unlock (&self->input->lock);
    return TRUE;
  }
  g_mutex_unlock (&self->input->lock);

  /* Negotiate the format / sample depth with downstream */
  allowed_caps = gst_pad_get_allowed_caps (GST_BASE_SRC_PAD (self));
  if (!allowed_caps)
    allowed_caps = gst_pad_get_pad_template_caps (GST_BASE_SRC_PAD (self));

  sample_depth = bmdAudioSampleType32bitInteger;
  if (!gst_caps_is_empty (allowed_caps)) {
    GstStructure *s;

    allowed_caps = gst_caps_simplify (allowed_caps);

    s = gst_caps_get_structure (allowed_caps, 0);

    /* If it's not a string then both formats are supported */
    if (gst_structure_has_field_typed (s, "format", G_TYPE_STRING)) {
      const gchar *format = gst_structure_get_string (s, "format");
      if (g_str_equal (format, "S16LE")) {
        sample_depth = bmdAudioSampleType16bitInteger;
      }
    }
  }
  gst_caps_unref (allowed_caps);

  switch (self->connection) {
    case GST_DECKLINK_AUDIO_CONNECTION_AUTO:{
      GstElement *videosrc = NULL;
      GstDecklinkConnectionEnum vconn;

      // Try to get the connection from the videosrc and try
      // to select a sensible audio connection based on that
      g_mutex_lock (&self->input->lock);
      if (self->input->videosrc)
        videosrc = GST_ELEMENT_CAST (gst_object_ref (self->input->videosrc));
      g_mutex_unlock (&self->input->lock);

      if (videosrc) {
        g_object_get (videosrc, "connection", &vconn, NULL);
        gst_object_unref (videosrc);

        switch (vconn) {
          case GST_DECKLINK_CONNECTION_SDI:
            conn = bmdAudioConnectionEmbedded;
            break;
          case GST_DECKLINK_CONNECTION_HDMI:
            conn = bmdAudioConnectionEmbedded;
            break;
          case GST_DECKLINK_CONNECTION_OPTICAL_SDI:
            conn = bmdAudioConnectionEmbedded;
            break;
          case GST_DECKLINK_CONNECTION_COMPONENT:
            conn = bmdAudioConnectionAnalog;
            break;
          case GST_DECKLINK_CONNECTION_COMPOSITE:
            conn = bmdAudioConnectionAnalog;
            break;
          case GST_DECKLINK_CONNECTION_SVIDEO:
            conn = bmdAudioConnectionAnalog;
            break;
          default:
            // Use default
            break;
        }
      }

      break;
    }
    case GST_DECKLINK_AUDIO_CONNECTION_EMBEDDED:
      conn = bmdAudioConnectionEmbedded;
      break;
    case GST_DECKLINK_AUDIO_CONNECTION_AES_EBU:
      conn = bmdAudioConnectionAESEBU;
      break;
    case GST_DECKLINK_AUDIO_CONNECTION_ANALOG:
      conn = bmdAudioConnectionAnalog;
      break;
    case GST_DECKLINK_AUDIO_CONNECTION_ANALOG_XLR:
      conn = bmdAudioConnectionAnalogXLR;
      break;
    case GST_DECKLINK_AUDIO_CONNECTION_ANALOG_RCA:
      conn = bmdAudioConnectionAnalogRCA;
      break;
    default:
      g_assert_not_reached ();
      break;
  }

  if (conn != (BMDAudioConnection) - 1) {
    ret =
        self->input->config->SetInt (bmdDeckLinkConfigAudioInputConnection,
        conn);
    if (ret != S_OK) {
      GST_ERROR ("set configuration (audio input connection): 0x%08lx",
          (unsigned long) ret);
      return FALSE;
    }
  }

  ret = self->input->input->EnableAudioInput (bmdAudioSampleRate48kHz,
      sample_depth, self->channels_found);
  if (ret != S_OK) {
    GST_WARNING_OBJECT (self, "Failed to enable audio input: 0x%08lx",
        (unsigned long) ret);
    return FALSE;
  }
  gst_audio_info_set_format (&self->info,
      sample_depth ==
      bmdAudioSampleType16bitInteger ? GST_AUDIO_FORMAT_S16LE :
      GST_AUDIO_FORMAT_S32LE, 48000, self->channels_found, NULL);

  g_mutex_lock (&self->input->lock);
  self->input->audio_enabled = TRUE;
  if (self->input->start_streams && self->input->videosrc)
    self->input->start_streams (self->input->videosrc);
  g_mutex_unlock (&self->input->lock);

  caps = gst_audio_info_to_caps (&self->info);
  if (!gst_base_src_set_caps (GST_BASE_SRC (self), caps)) {
    gst_caps_unref (caps);
    GST_WARNING_OBJECT (self, "Failed to set caps");
    return FALSE;
  }
  gst_caps_unref (caps);

  self->skipped_last = 0;
  self->skip_from_timestamp = GST_CLOCK_TIME_NONE;
  self->skip_to_timestamp = GST_CLOCK_TIME_NONE;

  return TRUE;
}

static void
gst_decklink_audio_src_got_packet (GstElement * element,
    IDeckLinkAudioInputPacket * packet, GstClockTime capture_time,
    GstClockTime stream_time, GstClockTime stream_duration,
    GstClockTime hardware_time, GstClockTime hardware_duration,
    gboolean no_signal)
{
  GstDecklinkAudioSrc *self = GST_DECKLINK_AUDIO_SRC_CAST (element);
  GstClockTime timestamp = GST_CLOCK_TIME_NONE;

  GST_LOG_OBJECT (self,
      "Got audio packet at %" GST_TIME_FORMAT " / %" GST_TIME_FORMAT
      ", no signal %d", GST_TIME_ARGS (capture_time),
      GST_TIME_ARGS (stream_time), no_signal);

  g_mutex_lock (&self->input->lock);
  if (self->input->videosrc) {
    GstDecklinkVideoSrc *videosrc =
        GST_DECKLINK_VIDEO_SRC_CAST (gst_object_ref (self->input->videosrc));

    if (videosrc->drop_no_signal_frames && no_signal) {
      g_mutex_unlock (&self->input->lock);
      return;
    }

    if (videosrc->first_time == GST_CLOCK_TIME_NONE)
      videosrc->first_time = stream_time;

    if (GST_CLOCK_TIME_IS_VALID (videosrc->first_time) &&
        GST_CLOCK_TIME_IS_VALID (stream_time) &&
        videosrc->skip_first_time > 0 &&
        stream_time - videosrc->first_time < videosrc->skip_first_time) {
      GST_DEBUG_OBJECT (self,
          "Skipping frame as requested: %" GST_TIME_FORMAT " < %"
          GST_TIME_FORMAT, GST_TIME_ARGS (stream_time),
          GST_TIME_ARGS (videosrc->skip_first_time + videosrc->first_time));
      g_mutex_unlock (&self->input->lock);
      return;
    }

    if (videosrc->output_stream_time)
      timestamp = stream_time;
    else if (GST_CLOCK_TIME_IS_VALID (stream_time))
      timestamp = gst_clock_adjust_with_calibration (NULL, stream_time,
          videosrc->current_time_mapping.xbase,
          videosrc->current_time_mapping.b, videosrc->current_time_mapping.num,
          videosrc->current_time_mapping.den);
  } else {
    timestamp = capture_time;
  }
  g_mutex_unlock (&self->input->lock);

  GST_LOG_OBJECT (self, "Converted times to %" GST_TIME_FORMAT,
      GST_TIME_ARGS (timestamp));

  g_mutex_lock (&self->lock);
  if (!self->flushing) {
    CapturePacket p;
    guint skipped_packets = 0;

    while (gst_queue_array_get_length (self->current_packets) >=
        self->buffer_size) {
      CapturePacket *tmp = (CapturePacket *)
          gst_queue_array_pop_head_struct (self->current_packets);
      if (skipped_packets == 0 && self->skipped_last == 0)
        self->skip_from_timestamp = tmp->timestamp;
      skipped_packets++;
      self->skip_to_timestamp = tmp->timestamp;
      capture_packet_clear (tmp);
    }

    if (self->skipped_last == 0 && skipped_packets > 0) {
      GST_WARNING_OBJECT (self, "Starting to drop audio packets");
    }

    if (skipped_packets == 0 && self->skipped_last > 0) {
      GST_ELEMENT_WARNING_WITH_DETAILS (self,
          STREAM, FAILED,
          ("Dropped %u old packets from %" GST_TIME_FORMAT " to %"
              GST_TIME_FORMAT, self->skipped_last,
              GST_TIME_ARGS (self->skip_from_timestamp),
              GST_TIME_ARGS (self->skip_to_timestamp)),
          (NULL),
          ("dropped", G_TYPE_UINT, self->skipped_last,
              "from", G_TYPE_UINT64, self->skip_from_timestamp,
              "to", G_TYPE_UINT64, self->skip_to_timestamp, NULL));
      self->skipped_last = 0;
    }
    self->skipped_last += skipped_packets;

    memset (&p, 0, sizeof (p));
    p.packet = packet;
    p.timestamp = timestamp;
    p.stream_timestamp = stream_time;
    p.stream_duration = stream_duration;
    p.hardware_timestamp = hardware_time;
    p.hardware_duration = hardware_duration;
    p.no_signal = no_signal;
    packet->AddRef ();
    gst_queue_array_push_tail_struct (self->current_packets, &p);
    g_cond_signal (&self->cond);
  }
  g_mutex_unlock (&self->lock);
}

static GstFlowReturn
gst_decklink_audio_src_create (GstPushSrc * bsrc, GstBuffer ** buffer)
{
  GstDecklinkAudioSrc *self = GST_DECKLINK_AUDIO_SRC_CAST (bsrc);
  GstFlowReturn flow_ret = GST_FLOW_OK;
  const guint8 *data;
  glong sample_count;
  gsize data_size;
  CapturePacket p;
  AudioPacket *ap;
  GstClockTime timestamp, duration;
  GstClockTime start_time, end_time;
  guint64 start_offset, end_offset;
  gboolean discont = FALSE;
  static GstStaticCaps stream_reference =
      GST_STATIC_CAPS ("timestamp/x-decklink-stream");
  static GstStaticCaps hardware_reference =
      GST_STATIC_CAPS ("timestamp/x-decklink-hardware");

  if (!gst_decklink_audio_src_start (self)) {
    return GST_FLOW_NOT_NEGOTIATED;
  }

retry:
  g_mutex_lock (&self->lock);
  while (gst_queue_array_is_empty (self->current_packets) && !self->flushing) {
    g_cond_wait (&self->cond, &self->lock);
  }

  if (self->flushing) {
    GST_DEBUG_OBJECT (self, "Flushing");
    g_mutex_unlock (&self->lock);
    return GST_FLOW_FLUSHING;
  }

  p = *(CapturePacket *)
      gst_queue_array_pop_head_struct (self->current_packets);
  g_mutex_unlock (&self->lock);

  p.packet->GetBytes ((gpointer *) & data);
  sample_count = p.packet->GetSampleFrameCount ();
  data_size = self->info.bpf * sample_count;

  timestamp = p.timestamp;

  if (!GST_CLOCK_TIME_IS_VALID (timestamp)) {
    if (self->next_offset == (guint64) - 1) {
      GST_DEBUG_OBJECT (self,
          "Got packet without timestamp before initial "
          "timestamp after discont - dropping");
      capture_packet_clear (&p);
      goto retry;
    } else {
      GST_INFO_OBJECT (self, "Unknown timestamp value");

      /* Likely the case where IDeckLinkInputCallback::VideoInputFrameArrived()
       * didn't provide video frame, so no reference video stream timestamp
       * is available. It can happen as per SDK documentation
       * under the following circumstances:
       * - On Intensity Pro with progressive NTSC only, every video frame will
       *   have two audio packets.
       * - With 3:2 pulldown there are five audio packets for each four
       *   video frames.
       * - If video processing is not fast enough, audio will still be delivered
       *
       * Assume there was no packet drop from previous valid packet, and use
       * previously calculated expected timestamp here */
      timestamp = gst_util_uint64_scale (self->next_offset,
          GST_SECOND, self->info.rate);
    }
  }

  ap = (AudioPacket *) g_malloc0 (sizeof (AudioPacket));

  *buffer =
      gst_buffer_new_wrapped_full ((GstMemoryFlags) GST_MEMORY_FLAG_READONLY,
      (gpointer) data, data_size, 0, data_size, ap,
      (GDestroyNotify) audio_packet_free);

  ap->packet = p.packet;
  p.packet->AddRef ();
  ap->input = self->input->input;
  ap->input->AddRef ();

  // Jitter and discontinuity handling, based on audiobasesrc
  start_time = timestamp;

  // Convert to the sample numbers
  start_offset =
      gst_util_uint64_scale (start_time, self->info.rate, GST_SECOND);
  // Convert back to round down to a sample multiple and get rid of rounding errors
  start_time = gst_util_uint64_scale (start_offset, GST_SECOND, self->info.rate);

  end_offset = start_offset + sample_count;
  end_time = gst_util_uint64_scale_int (end_offset, GST_SECOND,
      self->info.rate);

  duration = end_time - start_time;

  if (self->next_offset == (guint64) - 1) {
    discont = TRUE;
  } else {
    guint64 diff, max_sample_diff;

    // Check discont
    if (start_offset <= self->next_offset)
      diff = self->next_offset - start_offset;
    else
      diff = start_offset - self->next_offset;

    max_sample_diff =
        gst_util_uint64_scale_int (self->alignment_threshold, self->info.rate,
        GST_SECOND);

    // Discont!
    if (self->alignment_threshold > 0
        && self->alignment_threshold != GST_CLOCK_TIME_NONE
        && G_UNLIKELY (diff >= max_sample_diff)) {
      if (self->discont_wait > 0) {
        if (self->discont_time == GST_CLOCK_TIME_NONE) {
          self->discont_time = start_time;
        } else if (start_time - self->discont_time >= self->discont_wait) {
          discont = TRUE;
          self->discont_time = GST_CLOCK_TIME_NONE;
        }
      } else {
        discont = TRUE;
      }
    } else if (G_UNLIKELY (self->discont_time != GST_CLOCK_TIME_NONE)) {
      // we have had a discont, but are now back on track!
      self->discont_time = GST_CLOCK_TIME_NONE;
    }
  }

  if (discont) {
    // Have discont, need resync and use the capture timestamps
    if (self->next_offset != (guint64) - 1)
      GST_INFO_OBJECT (self, "Have discont. Expected %"
          G_GUINT64_FORMAT ", got %" G_GUINT64_FORMAT,
          self->next_offset, start_offset);
    GST_BUFFER_FLAG_SET (*buffer, GST_BUFFER_FLAG_DISCONT);
    self->next_offset = end_offset;
    // Got a discont and adjusted, reset the discont_time marker.
    self->discont_time = GST_CLOCK_TIME_NONE;
  } else if (self->alignment_threshold == 0) {
    // Don't align, just pass through timestamps
  } else {
    // No discont, just keep counting
    timestamp =
        gst_util_uint64_scale (self->next_offset, GST_SECOND, self->info.rate);
    self->next_offset += sample_count;
    duration =
        gst_util_uint64_scale (self->next_offset, GST_SECOND,
        self->info.rate) - timestamp;
  }

  // Detect gaps in stream time
  self->processed += sample_count;
  if (self->expected_stream_time != GST_CLOCK_TIME_NONE
      && p.stream_timestamp == GST_CLOCK_TIME_NONE) {
    /* We missed a frame. Extrapolate the timestamps */
    p.stream_timestamp = self->expected_stream_time;
    p.stream_duration =
        gst_util_uint64_scale_int (sample_count, GST_SECOND, self->info.rate);
  }
  if (self->last_hardware_time != GST_CLOCK_TIME_NONE
      && p.hardware_timestamp == GST_CLOCK_TIME_NONE) {
    /* This should always happen when the previous one also does, but let's
     * have two separate checks just in case */
    GstClockTime start_hw_offset, end_hw_offset;
    start_hw_offset =
        gst_util_uint64_scale (self->last_hardware_time, self->info.rate,
        GST_SECOND);
    end_hw_offset = start_hw_offset + sample_count;
    p.hardware_timestamp =
        gst_util_uint64_scale_int (end_hw_offset, GST_SECOND, self->info.rate);
    /* Will be the same as the stream duration - reuse it */
    p.hardware_duration = p.stream_duration;
  }

  if (p.stream_timestamp != GST_CLOCK_TIME_NONE) {
    GstClockTime start_stream_time, end_stream_time;

    start_stream_time = p.stream_timestamp;

    start_offset =
        gst_util_uint64_scale (start_stream_time, self->info.rate, GST_SECOND);

    end_offset = start_offset + sample_count;
    end_stream_time = gst_util_uint64_scale_int (end_offset, GST_SECOND,
        self->info.rate);

    /* With drop-frame we have gaps of 1 sample every now and then (rounding
     * errors because of the samples-per-frame pattern which is not 100%
     * accurate), and due to rounding errors in the calculations these can be
     * 2>x>1 */
    if (self->expected_stream_time != GST_CLOCK_TIME_NONE &&
        ABSDIFF (self->expected_stream_time, p.stream_timestamp) >
        gst_util_uint64_scale (2, GST_SECOND, self->info.rate)) {
      GstMessage *msg;
      GstClockTime running_time;

      self->dropped +=
          gst_util_uint64_scale (ABSDIFF (self->expected_stream_time,
              p.stream_timestamp), self->info.rate, GST_SECOND);
      running_time =
          gst_segment_to_running_time (&GST_BASE_SRC (self)->segment,
          GST_FORMAT_TIME, timestamp);

      msg =
          gst_message_new_qos (GST_OBJECT (self), TRUE, running_time,
          p.stream_timestamp, timestamp, duration);
      gst_message_set_qos_stats (msg, GST_FORMAT_DEFAULT, self->processed,
          self->dropped);
      gst_element_post_message (GST_ELEMENT (self), msg);
    }
    self->expected_stream_time = end_stream_time;
  }
  self->last_hardware_time = p.hardware_timestamp;

  if (p.no_signal)
    GST_BUFFER_FLAG_SET (*buffer, GST_BUFFER_FLAG_GAP);
  GST_BUFFER_TIMESTAMP (*buffer) = timestamp;
  GST_BUFFER_DURATION (*buffer) = duration;

  gst_buffer_add_reference_timestamp_meta (*buffer,
      gst_static_caps_get (&stream_reference), p.stream_timestamp,
      p.stream_duration);
  gst_buffer_add_reference_timestamp_meta (*buffer,
      gst_static_caps_get (&hardware_reference), p.hardware_timestamp,
      p.hardware_duration);

  GST_DEBUG_OBJECT (self,
      "Outputting buffer %p with timestamp %" GST_TIME_FORMAT " and duration %"
      GST_TIME_FORMAT, *buffer, GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (*buffer)),
      GST_TIME_ARGS (GST_BUFFER_DURATION (*buffer)));

  capture_packet_clear (&p);

  return flow_ret;
}

static GstCaps *
gst_decklink_audio_src_get_caps (GstBaseSrc * bsrc, GstCaps * filter)
{
  GstDecklinkAudioSrc *self = GST_DECKLINK_AUDIO_SRC_CAST (bsrc);
  GstCaps *caps, *template_caps;
  const GstStructure *s;
  gint channels;

  channels = self->channels;
  if (channels == 0)
    channels = self->channels_found;

  template_caps = gst_pad_get_pad_template_caps (GST_BASE_SRC_PAD (bsrc));
  if (channels == 0) {
    caps = template_caps;
  } else {
    if (channels > 2)
      s = gst_caps_get_structure (template_caps, 1);
    else
      s = gst_caps_get_structure (template_caps, 0);

    caps = gst_caps_new_full (gst_structure_copy (s), NULL);
    gst_caps_set_simple (caps, "channels", G_TYPE_INT, channels, NULL);
    gst_caps_unref (template_caps);
  }

  if (filter) {
    GstCaps *tmp =
        gst_caps_intersect_full (filter, caps, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (caps);
    caps = tmp;
  }

  return caps;
}

static gboolean
gst_decklink_audio_src_query (GstBaseSrc * bsrc, GstQuery * query)
{
  GstDecklinkAudioSrc *self = GST_DECKLINK_AUDIO_SRC_CAST (bsrc);
  gboolean ret = TRUE;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_LATENCY:{
      if (self->input) {
        g_mutex_lock (&self->input->lock);
        if (self->input->mode) {
          GstClockTime min, max;

          min =
              gst_util_uint64_scale_ceil (GST_SECOND, self->input->mode->fps_d,
              self->input->mode->fps_n);
          max = self->buffer_size * min;

          gst_query_set_latency (query, TRUE, min, max);
          ret = TRUE;
        } else {
          ret = FALSE;
        }
        g_mutex_unlock (&self->input->lock);
      } else {
        ret = FALSE;
      }

      break;
    }
    default:
      ret = GST_BASE_SRC_CLASS (parent_class)->query (bsrc, query);
      break;
  }

  return ret;
}

static gboolean
gst_decklink_audio_src_unlock (GstBaseSrc * bsrc)
{
  GstDecklinkAudioSrc *self = GST_DECKLINK_AUDIO_SRC_CAST (bsrc);

  g_mutex_lock (&self->lock);
  self->flushing = TRUE;
  g_cond_signal (&self->cond);
  g_mutex_unlock (&self->lock);

  return TRUE;
}

static gboolean
gst_decklink_audio_src_unlock_stop (GstBaseSrc * bsrc)
{
  GstDecklinkAudioSrc *self = GST_DECKLINK_AUDIO_SRC_CAST (bsrc);

  g_mutex_lock (&self->lock);
  self->flushing = FALSE;
  while (gst_queue_array_get_length (self->current_packets) > 0) {
    CapturePacket *tmp = (CapturePacket *)
        gst_queue_array_pop_head_struct (self->current_packets);
    capture_packet_clear (tmp);
  }
  g_mutex_unlock (&self->lock);

  return TRUE;
}

static gboolean
gst_decklink_audio_src_open (GstDecklinkAudioSrc * self)
{
  GST_DEBUG_OBJECT (self, "Opening");

  self->input =
      gst_decklink_acquire_nth_input (self->device_number, self->persistent_id,
      GST_ELEMENT_CAST (self), TRUE);
  if (!self->input) {
    GST_ERROR_OBJECT (self, "Failed to acquire input");
    return FALSE;
  }

  g_object_notify (G_OBJECT (self), "hw-serial-number");

  g_mutex_lock (&self->input->lock);
  if (self->channels > 0) {
    self->channels_found = self->channels;
  } else {
    if (self->input->attributes) {
      int64_t channels_found;

      HRESULT ret = self->input->attributes->GetInt
          (BMDDeckLinkMaximumAudioChannels, &channels_found);
      self->channels_found = channels_found;

      /* Sometimes the card may report an invalid number of channels. In
       * that case, we should (empirically) use 8. */
      if (ret != S_OK ||
          self->channels_found == 0 || g_enum_get_value ((GEnumClass *)
              g_type_class_peek (GST_TYPE_DECKLINK_AUDIO_CHANNELS),
              self->channels_found)
          == NULL) {
        self->channels_found = GST_DECKLINK_AUDIO_CHANNELS_8;
      }
    }
  }
  self->input->got_audio_packet = gst_decklink_audio_src_got_packet;
  g_mutex_unlock (&self->input->lock);

  return TRUE;
}

static gboolean
gst_decklink_audio_src_close (GstDecklinkAudioSrc * self)
{
  GST_DEBUG_OBJECT (self, "Closing");

  if (self->input) {
    g_mutex_lock (&self->input->lock);
    self->input->got_audio_packet = NULL;
    g_mutex_unlock (&self->input->lock);

    gst_decklink_release_nth_input (self->device_number, self->persistent_id,
        GST_ELEMENT_CAST (self), TRUE);
    self->input = NULL;
  }

  return TRUE;
}

static gboolean
gst_decklink_audio_src_stop (GstDecklinkAudioSrc * self)
{
  GST_DEBUG_OBJECT (self, "Stopping");

  while (gst_queue_array_get_length (self->current_packets) > 0) {
    CapturePacket *tmp = (CapturePacket *)
        gst_queue_array_pop_head_struct (self->current_packets);
    capture_packet_clear (tmp);
  }

  if (self->input && self->input->audio_enabled) {
    g_mutex_lock (&self->input->lock);
    self->input->audio_enabled = FALSE;
    g_mutex_unlock (&self->input->lock);

    self->input->input->DisableAudioInput ();
  }

  return TRUE;
}

#if 0
static gboolean
in_same_pipeline (GstElement * a, GstElement * b)
{
  GstObject *root = NULL, *tmp;
  gboolean ret = FALSE;

  tmp = gst_object_get_parent (GST_OBJECT_CAST (a));
  while (tmp != NULL) {
    if (root)
      gst_object_unref (root);
    root = tmp;
    tmp = gst_object_get_parent (root);
  }

  ret = root && gst_object_has_ancestor (GST_OBJECT_CAST (b), root);

  if (root)
    gst_object_unref (root);

  return ret;
}
#endif

static GstStateChangeReturn
gst_decklink_audio_src_change_state (GstElement * element,
    GstStateChange transition)
{
  GstDecklinkAudioSrc *self = GST_DECKLINK_AUDIO_SRC_CAST (element);
  GstStateChangeReturn ret;

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      self->processed = 0;
      self->dropped = 0;
      self->expected_stream_time = GST_CLOCK_TIME_NONE;
      if (!gst_decklink_audio_src_open (self)) {
        ret = GST_STATE_CHANGE_FAILURE;
        goto out;
      }
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:{
      GstElement *videosrc = NULL;

      // Check if there is a video src for this input too and if it
      // is actually in the same pipeline
      g_mutex_lock (&self->input->lock);
      if (self->input->videosrc)
        videosrc = GST_ELEMENT_CAST (gst_object_ref (self->input->videosrc));
      g_mutex_unlock (&self->input->lock);

      if (!videosrc) {
        GST_ELEMENT_ERROR (self, STREAM, FAILED,
            (NULL), ("Audio src needs a video src for its operation"));
        ret = GST_STATE_CHANGE_FAILURE;
        goto out;
      }
      // FIXME: This causes deadlocks sometimes
#if 0
      else if (!in_same_pipeline (GST_ELEMENT_CAST (self), videosrc)) {
        GST_ELEMENT_ERROR (self, STREAM, FAILED,
            (NULL),
            ("Audio src and video src need to be in the same pipeline"));
        ret = GST_STATE_CHANGE_FAILURE;
        gst_object_unref (videosrc);
        goto out;
      }
#endif

      if (videosrc)
        gst_object_unref (videosrc);

      self->flushing = FALSE;
      self->next_offset = -1;
      break;
    }
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      gst_decklink_audio_src_stop (self);
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      gst_decklink_audio_src_close (self);
      break;
    default:
      break;
  }
out:

  return ret;
}
