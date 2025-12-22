/*
 * WebRTC Audio Processing Elements
 *
 *  Copyright 2016 Collabora Ltd
 *    @author: Nicolas Dufresne <nicolas.dufresne@collabora.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 *
 */

/**
 * SECTION:element-webrtcdsp
 * @short_description: Audio Filter using WebRTC Audio Processing library
 *
 * A voice enhancement filter based on WebRTC Audio Processing library. This
 * library provides a whide variety of enhancement algorithms. This element
 * tries to enable as much as possible. The currently enabled enhancements are
 * High Pass Filter, Echo Canceller, Noise Suppression, Automatic Gain Control,
 * and some extended filters.
 *
 * While webrtcdsp element can be used alone, there is an exception for the
 * echo canceller. The audio canceller need to be aware of the far end streams
 * that are played to loud speakers. For this, you must place a webrtcechoprobe
 * element at that far end. Note that the sample rate must match between
 * webrtcdsp and the webrtechoprobe. Though, the number of channels can differ.
 * The probe is found by the DSP element using it's object name. By default,
 * webrtcdsp looks for webrtcechoprobe0, which means it just work if you have
 * a single probe and DSP.
 *
 * The probe can only be used within the same top level GstPipeline.
 * Additionally, to simplify the code, the probe element must be created
 * before the DSP sink pad is activated. It does not need to be in any
 * particular state and does not even need to be added to the pipeline yet.
 *
 * # Example launch line
 *
 * As a convenience, the echo canceller can be tested using an echo loop. In
 * this configuration, one would expect a single echo to be heard.
 *
 * |[
 * gst-launch-1.0 pulsesrc ! webrtcdsp ! webrtcechoprobe ! pulsesink
 * ]|
 *
 * In real environment, you'll place the probe before the playback, but only
 * process the far end streams. The DSP should be placed as close as possible
 * to the audio capture. The following pipeline is astracted and does not
 * represent a real pipeline.
 *
 * |[
 * gst-launch-1.0 far-end-src ! audio/x-raw,rate=48000 ! webrtcechoprobe ! pulsesink \
 *                pulsesrc ! audio/x-raw,rate=48000 ! webrtcdsp ! far-end-sink
 * ]|
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstwebrtcdsp.h"
#include "gstwebrtcechoprobe.h"

#include <modules/audio_processing/include/audio_processing.h>

GST_DEBUG_CATEGORY (webrtc_dsp_debug);
#define GST_CAT_DEFAULT (webrtc_dsp_debug)

#define DEFAULT_TARGET_LEVEL_DBFS 3
#define DEFAULT_COMPRESSION_GAIN_DB 9
#define DEFAULT_STARTUP_MIN_VOLUME 12
#define DEFAULT_LIMITER TRUE
#define DEFAULT_GAIN_CONTROL_MODE webrtc::AudioProcessing::Config::GainController1::Mode::kAdaptiveDigital
#define DEFAULT_VOICE_DETECTION FALSE
#define DEFAULT_VOICE_DETECTION_FRAME_SIZE_MS 10

static GstStaticPadTemplate gst_webrtc_dsp_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw, "
        "format = (string) " GST_AUDIO_NE (S16) ", "
        "layout = (string) interleaved, "
        "rate = (int) { 48000, 32000, 16000, 8000 }, "
        "channels = (int) [1, MAX];"
        "audio/x-raw, "
        "format = (string) " GST_AUDIO_NE (F32) ", "
        "layout = (string) non-interleaved, "
        "rate = (int) { 48000, 32000, 16000, 8000 }, "
        "channels = (int) [1, MAX]")
    );

static GstStaticPadTemplate gst_webrtc_dsp_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw, "
        "format = (string) " GST_AUDIO_NE (S16) ", "
        "layout = (string) interleaved, "
        "rate = (int) { 48000, 32000, 16000, 8000 }, "
        "channels = (int) [1, MAX];"
        "audio/x-raw, "
        "format = (string) " GST_AUDIO_NE (F32) ", "
        "layout = (string) non-interleaved, "
        "rate = (int) { 48000, 32000, 16000, 8000 }, "
        "channels = (int) [1, MAX]")
    );

typedef int GstWebrtcEchoSuppressionLevel;
#define GST_TYPE_WEBRTC_ECHO_SUPPRESSION_LEVEL \
    (gst_webrtc_echo_suppression_level_get_type ())
static GType
gst_webrtc_echo_suppression_level_get_type (void)
{
  static GType suppression_level_type = 0;
  static const GEnumValue level_types[] = {
    {1, "Low Suppression", "low"},
    {2, "Moderate Suppression", "moderate"},
    {3, "high Suppression", "high"},
    {0, NULL, NULL}
  };

  if (!suppression_level_type) {
    suppression_level_type =
        g_enum_register_static ("GstWebrtcEchoSuppressionLevel", level_types);
  }
  return suppression_level_type;
}

typedef webrtc::AudioProcessing::Config::NoiseSuppression::Level GstWebrtcNoiseSuppressionLevel;
#define GST_TYPE_WEBRTC_NOISE_SUPPRESSION_LEVEL \
    (gst_webrtc_noise_suppression_level_get_type ())
static GType
gst_webrtc_noise_suppression_level_get_type (void)
{
  static GType suppression_level_type = 0;
  static const GEnumValue level_types[] = {
    {webrtc::AudioProcessing::Config::NoiseSuppression::Level::kLow, "Low Suppression", "low"},
    {webrtc::AudioProcessing::Config::NoiseSuppression::Level::kModerate, "Moderate Suppression", "moderate"},
    {webrtc::AudioProcessing::Config::NoiseSuppression::Level::kHigh, "High Suppression", "high"},
    {webrtc::AudioProcessing::Config::NoiseSuppression::Level::kVeryHigh, "Very High Suppression",
      "very-high"},
    {0, NULL, NULL}
  };

  if (!suppression_level_type) {
    suppression_level_type =
        g_enum_register_static ("GstWebrtcNoiseSuppressionLevel", level_types);
  }
  return suppression_level_type;
}

typedef webrtc::AudioProcessing::Config::GainController1::Mode GstWebrtcGainControlMode;
#define GST_TYPE_WEBRTC_GAIN_CONTROL_MODE \
    (gst_webrtc_gain_control_mode_get_type ())
static GType
gst_webrtc_gain_control_mode_get_type (void)
{
  static GType gain_control_mode_type = 0;
  static const GEnumValue mode_types[] = {
    {webrtc::AudioProcessing::Config::GainController1::kAdaptiveDigital, "Adaptive Digital", "adaptive-digital"},
    {webrtc::AudioProcessing::Config::GainController1::kFixedDigital, "Fixed Digital", "fixed-digital"},
    {webrtc::AudioProcessing::Config::GainController1::kAdaptiveAnalog, "Adaptive Analog", "adaptive-analog"},
    {0, NULL, NULL}
  };

  if (!gain_control_mode_type) {
    gain_control_mode_type =
        g_enum_register_static ("GstWebrtcGainControlMode", mode_types);
  }
  return gain_control_mode_type;
}

typedef int GstWebrtcVoiceDetectionLikelihood;
#define GST_TYPE_WEBRTC_VOICE_DETECTION_LIKELIHOOD \
    (gst_webrtc_voice_detection_likelihood_get_type ())
static GType
gst_webrtc_voice_detection_likelihood_get_type (void)
{
  static GType likelihood_type = 0;
  static const GEnumValue likelihood_types[] = {
    {1, "Very Low Likelihood", "very-low"},
    {2, "Low Likelihood", "low"},
    {3, "Moderate Likelihood", "moderate"},
    {4, "High Likelihood", "high"},
    {0, NULL, NULL}
  };

  if (!likelihood_type) {
    likelihood_type =
        g_enum_register_static ("GstWebrtcVoiceDetectionLikelihood", likelihood_types);
  }
  return likelihood_type;
}

enum
{
  PROP_0,
  PROP_PROBE,
  PROP_HIGH_PASS_FILTER,
  PROP_ECHO_CANCEL,
  PROP_ECHO_SUPPRESSION_LEVEL,
  PROP_NOISE_SUPPRESSION,
  PROP_NOISE_SUPPRESSION_LEVEL,
  PROP_GAIN_CONTROL,
  PROP_EXPERIMENTAL_AGC,
  PROP_EXTENDED_FILTER,
  PROP_DELAY_AGNOSTIC,
  PROP_TARGET_LEVEL_DBFS,
  PROP_COMPRESSION_GAIN_DB,
  PROP_STARTUP_MIN_VOLUME,
  PROP_LIMITER,
  PROP_GAIN_CONTROL_MODE,
  PROP_VOICE_DETECTION,
  PROP_VOICE_DETECTION_FRAME_SIZE_MS,
  PROP_VOICE_DETECTION_LIKELIHOOD,
  PROP_EXTRA_DELAY_MS,
};

/**
 * GstWebrtcDSP:
 *
 * The adder object structure.
 */
struct _GstWebrtcDsp
{
  GstAudioFilter element;

  /* Protected by the object lock */
  GstAudioInfo info;
  gboolean interleaved;
  guint period_size;
  guint period_samples;
  gboolean stream_has_voice;

  /* Protected by the stream lock */
  GstAdapter *adapter;
  GstPlanarAudioAdapter *padapter;
  webrtc::AudioProcessing *apm;

  /* Protected by the object lock */
  gchar *probe_name;
  GstWebrtcEchoProbe *probe;

  /* Properties */
  gboolean high_pass_filter;
  gboolean echo_cancel;
  gboolean noise_suppression;
  webrtc::AudioProcessing::Config::NoiseSuppression::Level noise_suppression_level;
  gboolean gain_control;
  gint target_level_dbfs;
  gint compression_gain_db;
  gint startup_min_volume;
  gboolean limiter;
  webrtc::AudioProcessing::Config::GainController1::Mode gain_control_mode;
  gboolean voice_detection;
};

G_DEFINE_TYPE_WITH_CODE (GstWebrtcDsp, gst_webrtc_dsp, GST_TYPE_AUDIO_FILTER,
    GST_DEBUG_CATEGORY_INIT (webrtc_dsp_debug, "webrtcdsp", 0,
        "libwebrtcdsp wrapping elements"););
GST_ELEMENT_REGISTER_DEFINE (webrtcdsp, "webrtcdsp", GST_RANK_NONE,
    GST_TYPE_WEBRTC_DSP);

static const gchar *
webrtc_error_to_string (gint err)
{
  const gchar *str = "unknown error";

  switch (err) {
    case webrtc::AudioProcessing::kNoError:
      str = "success";
      break;
    case webrtc::AudioProcessing::kUnspecifiedError:
      str = "unspecified error";
      break;
    case webrtc::AudioProcessing::kCreationFailedError:
      str = "creating failed";
      break;
    case webrtc::AudioProcessing::kUnsupportedComponentError:
      str = "unsupported component";
      break;
    case webrtc::AudioProcessing::kUnsupportedFunctionError:
      str = "unsupported function";
      break;
    case webrtc::AudioProcessing::kNullPointerError:
      str = "null pointer";
      break;
    case webrtc::AudioProcessing::kBadParameterError:
      str = "bad parameter";
      break;
    case webrtc::AudioProcessing::kBadSampleRateError:
      str = "bad sample rate";
      break;
    case webrtc::AudioProcessing::kBadDataLengthError:
      str = "bad data length";
      break;
    case webrtc::AudioProcessing::kBadNumberChannelsError:
      str = "bad number of channels";
      break;
    case webrtc::AudioProcessing::kFileError:
      str = "file IO error";
      break;
    case webrtc::AudioProcessing::kStreamParameterNotSetError:
      str = "stream parameter not set";
      break;
    case webrtc::AudioProcessing::kNotEnabledError:
      str = "not enabled";
      break;
    default:
      break;
  }

  return str;
}

static GstBuffer *
gst_webrtc_dsp_take_buffer (GstWebrtcDsp * self)
{
  GstBuffer *buffer;
  GstClockTime timestamp;
  guint64 distance;
  gboolean at_discont;

  if (self->interleaved) {
    timestamp = gst_adapter_prev_pts (self->adapter, &distance);
    distance /= self->info.bpf;
  } else {
    timestamp = gst_planar_audio_adapter_prev_pts (self->padapter, &distance);
  }

  timestamp += gst_util_uint64_scale_int (distance, GST_SECOND, self->info.rate);

  if (self->interleaved) {
    buffer = gst_adapter_take_buffer (self->adapter, self->period_size);
    at_discont = (gst_adapter_pts_at_discont (self->adapter) == timestamp);
  } else {
    buffer = gst_planar_audio_adapter_take_buffer (self->padapter,
        self->period_samples, GST_MAP_READWRITE);
    at_discont =
        (gst_planar_audio_adapter_pts_at_discont (self->padapter) == timestamp);
  }

  GST_BUFFER_PTS (buffer) = timestamp;
  GST_BUFFER_DURATION (buffer) = 10 * GST_MSECOND;

  if (at_discont && distance == 0) {
    GST_BUFFER_FLAG_SET (buffer, GST_BUFFER_FLAG_DISCONT);
  } else {
    GST_BUFFER_FLAG_UNSET (buffer, GST_BUFFER_FLAG_DISCONT);
  }

  return buffer;
}

static GstFlowReturn
gst_webrtc_dsp_analyze_reverse_stream (GstWebrtcDsp * self,
    GstClockTime rec_time)
{
  GstWebrtcEchoProbe *probe = NULL;
  webrtc::AudioProcessing *apm;
  GstBuffer *buf = NULL;
  GstAudioInfo info;
  gboolean interleaved = self->interleaved;
  GstAudioBuffer abuf;
  GstFlowReturn ret = GST_FLOW_OK;
  gint err, delay;

  GST_OBJECT_LOCK (self);
  if (self->echo_cancel)
    probe = GST_WEBRTC_ECHO_PROBE (g_object_ref (self->probe));
  info = self->info;
  GST_OBJECT_UNLOCK (self);

  /* If echo cancellation is disabled */
  if (!probe)
    return GST_FLOW_OK;

  delay =
    gst_webrtc_echo_probe_read (probe, rec_time, &buf, &info, &interleaved);

  apm = self->apm;
  apm->set_stream_delay_ms (delay);

  webrtc::StreamConfig config (info.rate, info.channels, false);

  g_return_val_if_fail (buf != NULL, GST_FLOW_ERROR);

  if (delay < 0)
    goto done;

  if (info.rate != self->info.rate) {
    GST_ELEMENT_ERROR (self, STREAM, FORMAT,
        ("Echo Probe has rate %i , while the DSP is running at rate %i,"
         " use a caps filter to ensure those are the same.",
         info.rate, self->info.rate), (NULL));
    ret = GST_FLOW_ERROR;
    goto done;
  }

  gst_audio_buffer_map (&abuf, &info, buf, GST_MAP_READWRITE);

  if (interleaved) {
    int16_t * const data = (int16_t * const) abuf.planes[0];

    if ((err = apm->ProcessReverseStream (data, config, config, data)) < 0)
      GST_WARNING_OBJECT (self, "Reverse stream analyses failed: %s.",
          webrtc_error_to_string (err));
  } else {
    float * const * data = (float * const *) abuf.planes;

    if ((err = apm->ProcessReverseStream (data, config, config, data)) < 0)
      GST_WARNING_OBJECT (self, "Reverse stream analyses failed: %s.",
          webrtc_error_to_string (err));
  }

  gst_audio_buffer_unmap (&abuf);

done:
  gst_object_unref (probe);
  gst_buffer_replace (&buf, NULL);

  return ret;
}

static void
gst_webrtc_vad_post_activity (GstWebrtcDsp *self, GstBuffer *buffer,
    gboolean stream_has_voice, guint8 level)
{
  GstClockTime timestamp = GST_BUFFER_PTS (buffer);
  GstBaseTransform *trans = GST_BASE_TRANSFORM_CAST (self);
  GstStructure *s;
  GstClockTime stream_time;
  GstAudioLevelMeta *meta;

  meta = gst_buffer_get_audio_level_meta (buffer);
  if (meta) {
    meta->voice_activity = stream_has_voice;
    meta->level = level;
  } else {
    gst_buffer_add_audio_level_meta (buffer, level, stream_has_voice);
  }

  stream_time = gst_segment_to_stream_time (&trans->segment, GST_FORMAT_TIME,
      timestamp);

  s = gst_structure_new ("voice-activity",
      "stream-time", G_TYPE_UINT64, stream_time,
      "stream-has-voice", G_TYPE_BOOLEAN, stream_has_voice, NULL);

  GST_LOG_OBJECT (self, "Posting voice activity message, stream %s voice",
      stream_has_voice ? "now has" : "no longer has");

  gst_element_post_message (GST_ELEMENT (self),
      gst_message_new_element (GST_OBJECT (self), s));
}

static GstFlowReturn
gst_webrtc_dsp_process_stream (GstWebrtcDsp * self,
    GstBuffer * buffer)
{
  GstAudioBuffer abuf;
  webrtc::AudioProcessing * apm = self->apm;
  webrtc::StreamConfig config (self->info.rate, self->info.channels, false);
  gint err;

  if (!gst_audio_buffer_map (&abuf, &self->info, buffer,
          (GstMapFlags) GST_MAP_READWRITE)) {
    gst_buffer_unref (buffer);
    return GST_FLOW_ERROR;
  }

  if (self->interleaved) {
    int16_t * const data = (int16_t * const) abuf.planes[0];
    err = apm->ProcessStream (data, config, config, data);
  } else {
    float * const * data = (float * const *) abuf.planes;
    err = apm->ProcessStream (data, config, config, data);
  }

  if (err < 0) {
    GST_WARNING_OBJECT (self, "Failed to filter the audio: %s.",
        webrtc_error_to_string (err));
  } else {
    if (self->voice_detection) {
      webrtc::AudioProcessingStats stats = apm->GetStatistics ();
      gboolean stream_has_voice = stats.voice_detected && *stats.voice_detected;
      // The meta takes the value as -dbov, so we negate
      guint8 level = stats.output_rms_dbfs ? (guint8) -(*stats.output_rms_dbfs) : 127;

      if (stream_has_voice != self->stream_has_voice)
        gst_webrtc_vad_post_activity (self, buffer, stream_has_voice, level);

      self->stream_has_voice = stream_has_voice;
    }
  }

  gst_audio_buffer_unmap (&abuf);

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_webrtc_dsp_submit_input_buffer (GstBaseTransform * btrans,
    gboolean is_discont, GstBuffer * buffer)
{
  GstWebrtcDsp *self = GST_WEBRTC_DSP (btrans);

  buffer = gst_buffer_make_writable (buffer);
  GST_BUFFER_PTS (buffer) = gst_segment_to_running_time (&btrans->segment,
      GST_FORMAT_TIME, GST_BUFFER_PTS (buffer));

  if (is_discont) {
    GST_DEBUG_OBJECT (self,
        "Received discont, clearing adapter.");
    if (self->interleaved)
      gst_adapter_clear (self->adapter);
    else
      gst_planar_audio_adapter_clear (self->padapter);
  }

  if (self->interleaved)
    gst_adapter_push (self->adapter, buffer);
  else
    gst_planar_audio_adapter_push (self->padapter, buffer);

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_webrtc_dsp_generate_output (GstBaseTransform * btrans, GstBuffer ** outbuf)
{
  GstWebrtcDsp *self = GST_WEBRTC_DSP (btrans);
  GstFlowReturn ret;
  gboolean not_enough;

  if (self->interleaved)
    not_enough = gst_adapter_available (self->adapter) < self->period_size;
  else
    not_enough = gst_planar_audio_adapter_available (self->padapter) <
        self->period_samples;

  if (not_enough) {
    *outbuf = NULL;
    return GST_FLOW_OK;
  }

  *outbuf = gst_webrtc_dsp_take_buffer (self);
  ret = gst_webrtc_dsp_analyze_reverse_stream (self, GST_BUFFER_PTS (*outbuf));

  if (ret == GST_FLOW_OK)
    ret = gst_webrtc_dsp_process_stream (self, *outbuf);

  return ret;
}

static gboolean
gst_webrtc_dsp_start (GstBaseTransform * btrans)
{
  GstWebrtcDsp *self = GST_WEBRTC_DSP (btrans);

  GST_OBJECT_LOCK (self);

  if (self->echo_cancel) {
    self->probe = gst_webrtc_acquire_echo_probe (self->probe_name);

    if (self->probe == NULL) {
      GST_OBJECT_UNLOCK (self);
      GST_ELEMENT_ERROR (self, RESOURCE, NOT_FOUND,
          ("No echo probe with name %s found.", self->probe_name), (NULL));
      return FALSE;
    }
  }

  GST_OBJECT_UNLOCK (self);

  return TRUE;
}

static gboolean
gst_webrtc_dsp_setup (GstAudioFilter * filter, const GstAudioInfo * info)
{
  GstWebrtcDsp *self = GST_WEBRTC_DSP (filter);
  webrtc::AudioProcessing::Config config;
  GstAudioInfo probe_info = *info;

  GST_LOG_OBJECT (self, "setting format to %s with %i Hz and %i channels",
      info->finfo->description, info->rate, info->channels);

  GST_OBJECT_LOCK (self);

  gst_adapter_clear (self->adapter);
  gst_planar_audio_adapter_clear (self->padapter);

  self->info = *info;
  self->interleaved = (info->layout == GST_AUDIO_LAYOUT_INTERLEAVED);
  self->apm = webrtc::AudioProcessingBuilder().Create();

  if (!self->interleaved)
    gst_planar_audio_adapter_configure (self->padapter, info);

  /* WebRTC library works with 10ms buffers, compute once this size */
  self->period_samples = info->rate / 100;
  self->period_size = self->period_samples * info->bpf;

  if (self->interleaved && (self->period_size > MAX_DATA_SIZE_SAMPLES * 2))
    goto period_too_big;

  if (self->probe) {
    GST_WEBRTC_ECHO_PROBE_LOCK (self->probe);

    if (self->probe->info.rate != 0) {
      if (self->probe->info.rate != info->rate)
        goto probe_has_wrong_rate;
      probe_info = self->probe->info;
    }

    GST_WEBRTC_ECHO_PROBE_UNLOCK (self->probe);
  }

  config.pipeline.multi_channel_render = (info->channels > 1);
  config.pipeline.multi_channel_capture = (probe_info.channels > 1);

  /* Setup Filters */
  // TODO: expose pre_amplifier

  if (self->high_pass_filter) {
    GST_DEBUG_OBJECT (self, "Enabling High Pass filter");
    config.high_pass_filter.enabled = true;
  }

  if (self->echo_cancel) {
    GST_DEBUG_OBJECT (self, "Enabling Echo Cancellation");
    config.echo_canceller.enabled = true;
  }

  if (self->noise_suppression) {
    GST_DEBUG_OBJECT (self, "Enabling Noise Suppression");
    config.noise_suppression.enabled = true;
    config.noise_suppression.level = self->noise_suppression_level;
  }

  // TODO: expose transient suppression

  if (self->voice_detection) {
    GST_DEBUG_OBJECT (self, "Enabling Voice Activity Detection");
    config.voice_detection.enabled = true;
    self->stream_has_voice = FALSE;
  }

  if (self->gain_control) {
    GEnumClass *mode_class = (GEnumClass *)
        g_type_class_ref (GST_TYPE_WEBRTC_GAIN_CONTROL_MODE);

    GST_DEBUG_OBJECT (self, "Enabling Digital Gain Control, target level "
        "dBFS %d, compression gain dB %d, limiter %senabled, mode: %s",
        self->target_level_dbfs, self->compression_gain_db,
        self->limiter ? "" : "NOT ",
        g_enum_get_value (mode_class, self->gain_control_mode)->value_name);

    g_type_class_unref (mode_class);

    config.gain_controller1.enabled = true;
    config.gain_controller1.target_level_dbfs = self->target_level_dbfs;
    config.gain_controller1.compression_gain_db = self->compression_gain_db;
    config.gain_controller1.enable_limiter = self->limiter;
    config.level_estimation.enabled = true;
  }

  // TODO: expose gain controller 2
  // TODO: expose residual echo detector

  self->apm->ApplyConfig (config);

  GST_OBJECT_UNLOCK (self);

  return TRUE;

period_too_big:
  GST_OBJECT_UNLOCK (self);
  GST_WARNING_OBJECT (self, "webrtcdsp format produce too big period "
      "(maximum is %d samples and we have %u samples), "
      "reduce the number of channels or the rate.",
      MAX_DATA_SIZE_SAMPLES, self->period_size / 2);
  return FALSE;

probe_has_wrong_rate:
  GST_WEBRTC_ECHO_PROBE_UNLOCK (self->probe);
  GST_OBJECT_UNLOCK (self);
  GST_ELEMENT_ERROR (self, STREAM, FORMAT,
      ("Echo Probe has rate %i , while the DSP is running at rate %i,"
          " use a caps filter to ensure those are the same.",
          probe_info.rate, info->rate), (NULL));
  return FALSE;
}

static gboolean
gst_webrtc_dsp_stop (GstBaseTransform * btrans)
{
  GstWebrtcDsp *self = GST_WEBRTC_DSP (btrans);

  GST_OBJECT_LOCK (self);

  gst_adapter_clear (self->adapter);
  gst_planar_audio_adapter_clear (self->padapter);

  if (self->probe) {
    gst_webrtc_release_echo_probe (self->probe);
    self->probe = NULL;
  }

  delete self->apm;
  self->apm = NULL;

  GST_OBJECT_UNLOCK (self);

  return TRUE;
}

static void
gst_webrtc_dsp_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GstWebrtcDsp *self = GST_WEBRTC_DSP (object);

  GST_OBJECT_LOCK (self);
  switch (prop_id) {
    case PROP_PROBE:
      g_free (self->probe_name);
      self->probe_name = g_value_dup_string (value);
      break;
    case PROP_HIGH_PASS_FILTER:
      self->high_pass_filter = g_value_get_boolean (value);
      break;
    case PROP_ECHO_CANCEL:
      self->echo_cancel = g_value_get_boolean (value);
      break;
    case PROP_ECHO_SUPPRESSION_LEVEL:
      break;
    case PROP_NOISE_SUPPRESSION:
      self->noise_suppression = g_value_get_boolean (value);
      break;
    case PROP_NOISE_SUPPRESSION_LEVEL:
      self->noise_suppression_level =
          (GstWebrtcNoiseSuppressionLevel) g_value_get_enum (value);
      break;
    case PROP_GAIN_CONTROL:
      self->gain_control = g_value_get_boolean (value);
      break;
    case PROP_EXPERIMENTAL_AGC:
      break;
    case PROP_EXTENDED_FILTER:
      break;
    case PROP_DELAY_AGNOSTIC:
      break;
    case PROP_TARGET_LEVEL_DBFS:
      self->target_level_dbfs = g_value_get_int (value);
      break;
    case PROP_COMPRESSION_GAIN_DB:
      self->compression_gain_db = g_value_get_int (value);
      break;
    case PROP_STARTUP_MIN_VOLUME:
      self->startup_min_volume = g_value_get_int (value);
      break;
    case PROP_LIMITER:
      self->limiter = g_value_get_boolean (value);
      break;
    case PROP_GAIN_CONTROL_MODE:
      self->gain_control_mode =
          (GstWebrtcGainControlMode) g_value_get_enum (value);
      break;
    case PROP_VOICE_DETECTION:
      self->voice_detection = g_value_get_boolean (value);
      break;
    case PROP_VOICE_DETECTION_FRAME_SIZE_MS:
      break;
    case PROP_VOICE_DETECTION_LIKELIHOOD:
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (self);
}

static void
gst_webrtc_dsp_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstWebrtcDsp *self = GST_WEBRTC_DSP (object);

  GST_OBJECT_LOCK (self);
  switch (prop_id) {
    case PROP_PROBE:
      g_value_set_string (value, self->probe_name);
      break;
    case PROP_HIGH_PASS_FILTER:
      g_value_set_boolean (value, self->high_pass_filter);
      break;
    case PROP_ECHO_CANCEL:
      g_value_set_boolean (value, self->echo_cancel);
      break;
    case PROP_ECHO_SUPPRESSION_LEVEL:
      g_value_set_enum (value, (GstWebrtcEchoSuppressionLevel) 2);
      break;
    case PROP_NOISE_SUPPRESSION:
      g_value_set_boolean (value, self->noise_suppression);
      break;
    case PROP_NOISE_SUPPRESSION_LEVEL:
      g_value_set_enum (value, self->noise_suppression_level);
      break;
    case PROP_GAIN_CONTROL:
      g_value_set_boolean (value, self->gain_control);
      break;
    case PROP_EXPERIMENTAL_AGC:
      g_value_set_boolean (value, false);
      break;
    case PROP_EXTENDED_FILTER:
      g_value_set_boolean (value, false);
      break;
    case PROP_DELAY_AGNOSTIC:
      g_value_set_boolean (value, false);
      break;
    case PROP_TARGET_LEVEL_DBFS:
      g_value_set_int (value, self->target_level_dbfs);
      break;
    case PROP_COMPRESSION_GAIN_DB:
      g_value_set_int (value, self->compression_gain_db);
      break;
    case PROP_STARTUP_MIN_VOLUME:
      g_value_set_int (value, self->startup_min_volume);
      break;
    case PROP_LIMITER:
      g_value_set_boolean (value, self->limiter);
      break;
    case PROP_GAIN_CONTROL_MODE:
      g_value_set_enum (value, self->gain_control_mode);
      break;
    case PROP_VOICE_DETECTION:
      g_value_set_boolean (value, self->voice_detection);
      break;
    case PROP_VOICE_DETECTION_FRAME_SIZE_MS:
      g_value_set_int (value, 0);
      break;
    case PROP_VOICE_DETECTION_LIKELIHOOD:
      g_value_set_enum (value, 2);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (self);
}


static void
gst_webrtc_dsp_finalize (GObject * object)
{
  GstWebrtcDsp *self = GST_WEBRTC_DSP (object);

  gst_object_unref (self->adapter);
  gst_object_unref (self->padapter);
  g_free (self->probe_name);

  G_OBJECT_CLASS (gst_webrtc_dsp_parent_class)->finalize (object);
}

static void
gst_webrtc_dsp_init (GstWebrtcDsp * self)
{
  self->adapter = gst_adapter_new ();
  self->padapter = gst_planar_audio_adapter_new ();
  gst_audio_info_init (&self->info);
}

static void
gst_webrtc_dsp_class_init (GstWebrtcDspClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstBaseTransformClass *btrans_class = GST_BASE_TRANSFORM_CLASS (klass);
  GstAudioFilterClass *audiofilter_class = GST_AUDIO_FILTER_CLASS (klass);

  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_webrtc_dsp_finalize);
  gobject_class->set_property = GST_DEBUG_FUNCPTR (gst_webrtc_dsp_set_property);
  gobject_class->get_property = GST_DEBUG_FUNCPTR (gst_webrtc_dsp_get_property);

  btrans_class->passthrough_on_same_caps = FALSE;
  btrans_class->start = GST_DEBUG_FUNCPTR (gst_webrtc_dsp_start);
  btrans_class->stop = GST_DEBUG_FUNCPTR (gst_webrtc_dsp_stop);
  btrans_class->submit_input_buffer =
      GST_DEBUG_FUNCPTR (gst_webrtc_dsp_submit_input_buffer);
  btrans_class->generate_output =
      GST_DEBUG_FUNCPTR (gst_webrtc_dsp_generate_output);

  audiofilter_class->setup = GST_DEBUG_FUNCPTR (gst_webrtc_dsp_setup);

  gst_element_class_add_static_pad_template (element_class,
      &gst_webrtc_dsp_src_template);
  gst_element_class_add_static_pad_template (element_class,
      &gst_webrtc_dsp_sink_template);
  gst_element_class_set_static_metadata (element_class,
      "Voice Processor (AGC, AEC, filters, etc.)",
      "Generic/Audio",
      "Pre-processes voice with WebRTC Audio Processing Library",
      "Nicolas Dufresne <nicolas.dufresne@collabora.com>");

  g_object_class_install_property (gobject_class,
      PROP_PROBE,
      g_param_spec_string ("probe", "Echo Probe",
          "The name of the webrtcechoprobe element that record the audio being "
          "played through loud speakers. Must be set before PAUSED state.",
          "webrtcechoprobe0",
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              G_PARAM_CONSTRUCT)));

  g_object_class_install_property (gobject_class,
      PROP_HIGH_PASS_FILTER,
      g_param_spec_boolean ("high-pass-filter", "High Pass Filter",
          "Enable or disable high pass filtering", TRUE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              G_PARAM_CONSTRUCT)));

  g_object_class_install_property (gobject_class,
      PROP_ECHO_CANCEL,
      g_param_spec_boolean ("echo-cancel", "Echo Cancel",
          "Enable or disable echo canceller", TRUE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              G_PARAM_CONSTRUCT)));

  g_object_class_install_property (gobject_class,
      PROP_ECHO_SUPPRESSION_LEVEL,
      g_param_spec_enum ("echo-suppression-level",
          "Echo Suppression Level (does nothing)",
          "Controls the aggressiveness of the suppressor. A higher level "
          "trades off double-talk performance for increased echo suppression.",
          GST_TYPE_WEBRTC_ECHO_SUPPRESSION_LEVEL, 2,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              G_PARAM_CONSTRUCT | G_PARAM_DEPRECATED)));

  g_object_class_install_property (gobject_class,
      PROP_NOISE_SUPPRESSION,
      g_param_spec_boolean ("noise-suppression", "Noise Suppression",
          "Enable or disable noise suppression", TRUE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              G_PARAM_CONSTRUCT)));

  g_object_class_install_property (gobject_class,
      PROP_NOISE_SUPPRESSION_LEVEL,
      g_param_spec_enum ("noise-suppression-level", "Noise Suppression Level",
          "Controls the aggressiveness of the suppression. Increasing the "
          "level will reduce the noise level at the expense of a higher "
          "speech distortion.", GST_TYPE_WEBRTC_NOISE_SUPPRESSION_LEVEL,
          webrtc::AudioProcessing::Config::NoiseSuppression::Level::kModerate,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              G_PARAM_CONSTRUCT)));

  g_object_class_install_property (gobject_class,
      PROP_GAIN_CONTROL,
      g_param_spec_boolean ("gain-control", "Gain Control",
          "Enable or disable automatic digital gain control",
          TRUE, (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              G_PARAM_CONSTRUCT)));

  g_object_class_install_property (gobject_class,
      PROP_EXPERIMENTAL_AGC,
      g_param_spec_boolean ("experimental-agc",
          "Experimental AGC (does nothing)",
          "Enable or disable experimental automatic gain control.",
          FALSE, (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              G_PARAM_CONSTRUCT | G_PARAM_DEPRECATED)));

  g_object_class_install_property (gobject_class,
      PROP_EXTENDED_FILTER,
      g_param_spec_boolean ("extended-filter", "Extended Filter",
          "Enable or disable the extended filter.",
          TRUE, (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              G_PARAM_CONSTRUCT | G_PARAM_DEPRECATED)));

  g_object_class_install_property (gobject_class,
      PROP_DELAY_AGNOSTIC,
      g_param_spec_boolean ("delay-agnostic",
          "Delay agnostic mode (does nothing)",
          "Enable or disable the delay agnostic mode.",
          FALSE, (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              G_PARAM_CONSTRUCT | G_PARAM_DEPRECATED)));

  g_object_class_install_property (gobject_class,
      PROP_TARGET_LEVEL_DBFS,
      g_param_spec_int ("target-level-dbfs", "Target Level dBFS",
          "Sets the target peak |level| (or envelope) of the gain control in "
          "dBFS (decibels from digital full-scale).",
          0, 31, DEFAULT_TARGET_LEVEL_DBFS, (GParamFlags) (G_PARAM_READWRITE |
              G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT)));

  g_object_class_install_property (gobject_class,
      PROP_COMPRESSION_GAIN_DB,
      g_param_spec_int ("compression-gain-db", "Compression Gain dB",
          "Sets the maximum |gain| the digital compression stage may apply, "
					"in dB.",
          0, 90, DEFAULT_COMPRESSION_GAIN_DB, (GParamFlags) (G_PARAM_READWRITE |
              G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT)));

  g_object_class_install_property (gobject_class,
      PROP_STARTUP_MIN_VOLUME,
      g_param_spec_int ("startup-min-volume", "Startup Minimum Volume",
          "At startup the experimental AGC moves the microphone volume up to "
          "|startup_min_volume| if the current microphone volume is set too "
          "low. No effect if experimental-agc isn't enabled.",
          12, 255, DEFAULT_STARTUP_MIN_VOLUME, (GParamFlags) (G_PARAM_READWRITE |
              G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT)));

  g_object_class_install_property (gobject_class,
      PROP_LIMITER,
      g_param_spec_boolean ("limiter", "Limiter",
          "When enabled, the compression stage will hard limit the signal to "
          "the target level. Otherwise, the signal will be compressed but not "
          "limited above the target level.",
          DEFAULT_LIMITER, (GParamFlags) (G_PARAM_READWRITE |
              G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT)));

  g_object_class_install_property (gobject_class,
      PROP_GAIN_CONTROL_MODE,
      g_param_spec_enum ("gain-control-mode", "Gain Control Mode",
          "Controls the mode of the compression stage",
          GST_TYPE_WEBRTC_GAIN_CONTROL_MODE,
          DEFAULT_GAIN_CONTROL_MODE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              G_PARAM_CONSTRUCT)));

  g_object_class_install_property (gobject_class,
      PROP_VOICE_DETECTION,
      g_param_spec_boolean ("voice-detection", "Voice Detection",
          "Enable or disable the voice activity detector",
          DEFAULT_VOICE_DETECTION, (GParamFlags) (G_PARAM_READWRITE |
              G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT)));

  g_object_class_install_property (gobject_class,
      PROP_VOICE_DETECTION_FRAME_SIZE_MS,
      g_param_spec_int ("voice-detection-frame-size-ms",
          "Voice detection frame size in milliseconds (does nothing)",
          "Sets the |size| of the frames in ms on which the VAD will operate. "
          "Larger frames will improve detection accuracy, but reduce the "
          "frequency of updates",
          10, 30, DEFAULT_VOICE_DETECTION_FRAME_SIZE_MS,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              G_PARAM_CONSTRUCT | G_PARAM_DEPRECATED)));

  g_object_class_install_property (gobject_class,
      PROP_VOICE_DETECTION_LIKELIHOOD,
      g_param_spec_enum ("voice-detection-likelihood",
          "Voice detection likelihood (does nothing)",
          "Specifies the likelihood that a frame will be declared to contain "
          "voice.",
          GST_TYPE_WEBRTC_VOICE_DETECTION_LIKELIHOOD, 2,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              G_PARAM_CONSTRUCT | G_PARAM_DEPRECATED)));

  gst_type_mark_as_plugin_api (GST_TYPE_WEBRTC_GAIN_CONTROL_MODE, (GstPluginAPIFlags) 0);
  gst_type_mark_as_plugin_api (GST_TYPE_WEBRTC_NOISE_SUPPRESSION_LEVEL, (GstPluginAPIFlags) 0);
  gst_type_mark_as_plugin_api (GST_TYPE_WEBRTC_ECHO_SUPPRESSION_LEVEL, (GstPluginAPIFlags) 0);
  gst_type_mark_as_plugin_api (GST_TYPE_WEBRTC_VOICE_DETECTION_LIKELIHOOD, (GstPluginAPIFlags) 0);
}
