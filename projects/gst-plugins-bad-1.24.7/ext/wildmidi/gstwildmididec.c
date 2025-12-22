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
 * SECTION:element-wildmididec
 * @see_also: #GstWildmidiDec
 *
 * wildmididec decodes MIDI files.
 *
 * It uses [WildMidi](https://www.mindwerks.net/projects/wildmidi/) for this
 * purpose. It can be autoplugged and therefore works with decodebin.
 *
 * ## Example launch line
 *
 * |[
 * gst-launch-1.0 filesrc location=media/example.mid ! wildmididec ! audioconvert ! audioresample ! autoaudiosink
 * ]|
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#include <gst/gst.h>
#include <glib/gstdio.h>

#ifdef G_OS_WIN32

#ifndef R_OK
#define R_OK 4                  /* Test for read permission */
#endif

#else
#include <unistd.h>
#endif

#include "gstwildmididec.h"


GST_DEBUG_CATEGORY_STATIC (wildmididec_debug);
#define GST_CAT_DEFAULT wildmididec_debug


/* This is hardcoded because the sample rate is set once,
 * globally, in WildMidi_Init() */
#define WILDMIDI_SAMPLE_RATE 44100
/* WildMidi always outputs stereo data */
#define WILDMIDI_NUM_CHANNELS 2

#ifndef WILDMIDI_CFG
#define WILDMIDI_CFG "/etc/timidity.cfg"
#endif

#define DEFAULT_LOG_VOLUME_SCALE     TRUE
#define DEFAULT_ENHANCED_RESAMPLING  TRUE
#define DEFAULT_REVERB               FALSE
#define DEFAULT_OUTPUT_BUFFER_SIZE   1024


enum
{
  PROP_0,
  PROP_LOG_VOLUME_SCALE,
  PROP_ENHANCED_RESAMPLING,
  PROP_REVERB,
  PROP_OUTPUT_BUFFER_SIZE
};



static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/midi; audio/riff-midi")
    );

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw, "
        "format = (string) " GST_AUDIO_NE (S16) ", "
        "layout = (string) interleaved, "
        "rate = (int) " G_STRINGIFY (WILDMIDI_SAMPLE_RATE) ", "
        "channels = (int) " G_STRINGIFY (WILDMIDI_NUM_CHANNELS)
    )
    );



G_DEFINE_TYPE (GstWildmidiDec, gst_wildmidi_dec,
    GST_TYPE_NONSTREAM_AUDIO_DECODER);
GST_ELEMENT_REGISTER_DEFINE (wildmididec, "wildmididec", GST_RANK_MARGINAL,
    gst_wildmidi_dec_get_type ());


static void gst_wildmidi_dec_finalize (GObject * object);

static void gst_wildmidi_dec_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_wildmidi_dec_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_wildmidi_dec_seek (GstNonstreamAudioDecoder * dec,
    GstClockTime * new_position);
static GstClockTime gst_wildmidi_dec_tell (GstNonstreamAudioDecoder * dec);

static gboolean gst_wildmidi_dec_load_from_buffer (GstNonstreamAudioDecoder *
    dec, GstBuffer * source_data, guint initial_subsong,
    GstNonstreamAudioSubsongMode initial_subsong_mode,
    GstClockTime * initial_position,
    GstNonstreamAudioOutputMode * initial_output_mode,
    gint * initial_num_loops);

static guint gst_wildmidi_dec_get_current_subsong (GstNonstreamAudioDecoder *
    dec);

static guint gst_wildmidi_dec_get_num_subsongs (GstNonstreamAudioDecoder * dec);
static GstClockTime
gst_wildmidi_dec_get_subsong_duration (GstNonstreamAudioDecoder * dec,
    guint subsong);

static guint
gst_wildmidi_dec_get_supported_output_modes (GstNonstreamAudioDecoder * dec);
static gboolean gst_wildmidi_dec_decode (GstNonstreamAudioDecoder * dec,
    GstBuffer ** buffer, guint * num_samples);

static void gst_wildmidi_dec_update_options (GstWildmidiDec * wildmidi_dec);



static GMutex load_mutex;
static unsigned long init_refcount = 0;
static gint wildmidi_initialized = 0;


static gchar *
gst_wildmidi_get_config_path (void)
{
  /* This code is adapted from the original wildmidi
   * gst-plugins-bad decoder element */

  gchar *path = g_strdup (g_getenv ("WILDMIDI_CFG"));

  GST_DEBUG
      ("trying configuration path \"%s\" from WILDMIDI_CFG environment variable",
      GST_STR_NULL (path));
  if (path && (g_access (path, R_OK) == -1)) {
    g_free (path);
    path = NULL;
  }

  if (path == NULL) {
    path =
        g_build_path (G_DIR_SEPARATOR_S, g_get_home_dir (), ".wildmidirc",
        NULL);
    GST_DEBUG ("trying configuration path \"%s\"", path);
    if (path && (g_access (path, R_OK) == -1)) {
      g_free (path);
      path = NULL;
    }
  }

  if (path == NULL) {
    path =
        g_build_path (G_DIR_SEPARATOR_S, G_DIR_SEPARATOR_S "etc",
        "wildmidi.cfg", NULL);
    GST_DEBUG ("trying configuration path \"%s\"", path);
    if (path && (g_access (path, R_OK) == -1)) {
      g_free (path);
      path = NULL;
    }
  }

  if (path == NULL) {
    path =
        g_build_path (G_DIR_SEPARATOR_S, G_DIR_SEPARATOR_S "etc", "wildmidi",
        "wildmidi.cfg", NULL);
    GST_DEBUG ("trying configuration path \"%s\"", path);
    if (path && (g_access (path, R_OK) == -1)) {
      g_free (path);
      path = NULL;
    }
  }

  if (path == NULL) {
    path = g_strdup (WILDMIDI_CFG);
    GST_DEBUG ("trying default configuration path \"%s\"", path);
    if (path && (g_access (path, R_OK) == -1)) {
      g_free (path);
      path = NULL;
    }
  }

  if (path == NULL) {
    path =
        g_build_path (G_DIR_SEPARATOR_S, G_DIR_SEPARATOR_S "etc",
        "timidity.cfg", NULL);
    GST_DEBUG ("trying TiMidity configuration path \"%s\"", path);
    if (path && (g_access (path, R_OK) == -1)) {
      g_free (path);
      path = NULL;
    }
  }

  if (path == NULL) {
    path =
        g_build_path (G_DIR_SEPARATOR_S, G_DIR_SEPARATOR_S "etc", "timidity",
        "timidity.cfg", NULL);
    GST_DEBUG ("trying TiMidity configuration path \"%s\"", path);
    if (path && (g_access (path, R_OK) == -1)) {
      g_free (path);
      path = NULL;
    }
  }

  return path;
}


static void
gst_wildmidi_init_library (void)
{
  GST_DEBUG ("WildMidi init instance counter: %lu", init_refcount);

  g_mutex_lock (&load_mutex);

  if (init_refcount != 0) {
    ++init_refcount;
  } else {
    gchar *config_path = gst_wildmidi_get_config_path ();
    if (config_path != NULL) {
      int ret = WildMidi_Init (config_path, WILDMIDI_SAMPLE_RATE, 0);
      g_free (config_path);

      if (ret == 0) {
        GST_DEBUG ("WildMidi initialized, version string: %s",
            WildMidi_GetString (WM_GS_VERSION));
        ++init_refcount;
        g_atomic_int_set (&wildmidi_initialized, 1);
      } else {
        GST_ERROR ("initializing WildMidi failed");
        g_atomic_int_set (&wildmidi_initialized, 0);
      }
    } else {
      GST_ERROR ("no config file, can't initialise");
      g_atomic_int_set (&wildmidi_initialized, 0);
    }
  }

  g_mutex_unlock (&load_mutex);
}


static void
gst_wildmidi_shutdown_library (void)
{
  GST_DEBUG ("WildMidi init instance counter: %lu", init_refcount);

  g_mutex_lock (&load_mutex);

  if (init_refcount != 0) {
    --init_refcount;
    if (init_refcount == 0) {
      WildMidi_Shutdown ();
      GST_DEBUG ("WildMidi shut down");
      g_atomic_int_set (&wildmidi_initialized, 0);
    }
  }

  g_mutex_unlock (&load_mutex);
}



void
gst_wildmidi_dec_class_init (GstWildmidiDecClass * klass)
{
  GObjectClass *object_class;
  GstElementClass *element_class;
  GstNonstreamAudioDecoderClass *dec_class;

  GST_DEBUG_CATEGORY_INIT (wildmididec_debug, "wildmididec", 0,
      "WildMidi-based MIDI music decoder");

  object_class = G_OBJECT_CLASS (klass);
  element_class = GST_ELEMENT_CLASS (klass);
  dec_class = GST_NONSTREAM_AUDIO_DECODER_CLASS (klass);

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&sink_template));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&src_template));

  object_class->finalize = GST_DEBUG_FUNCPTR (gst_wildmidi_dec_finalize);
  object_class->set_property =
      GST_DEBUG_FUNCPTR (gst_wildmidi_dec_set_property);
  object_class->get_property =
      GST_DEBUG_FUNCPTR (gst_wildmidi_dec_get_property);

  dec_class->tell = GST_DEBUG_FUNCPTR (gst_wildmidi_dec_tell);
  dec_class->seek = GST_DEBUG_FUNCPTR (gst_wildmidi_dec_seek);
  dec_class->load_from_buffer =
      GST_DEBUG_FUNCPTR (gst_wildmidi_dec_load_from_buffer);
  dec_class->get_current_subsong =
      GST_DEBUG_FUNCPTR (gst_wildmidi_dec_get_current_subsong);
  dec_class->get_num_subsongs =
      GST_DEBUG_FUNCPTR (gst_wildmidi_dec_get_num_subsongs);
  dec_class->get_subsong_duration =
      GST_DEBUG_FUNCPTR (gst_wildmidi_dec_get_subsong_duration);
  dec_class->get_supported_output_modes =
      GST_DEBUG_FUNCPTR (gst_wildmidi_dec_get_supported_output_modes);
  dec_class->decode = GST_DEBUG_FUNCPTR (gst_wildmidi_dec_decode);

  gst_element_class_set_static_metadata (element_class,
      "WildMidi-based MIDI music decoder",
      "Codec/Decoder/Audio",
      "Decodes MIDI music using WildMidi",
      "Carlos Rafael Giani <dv@pseudoterminal.org>");

  g_object_class_install_property (object_class,
      PROP_LOG_VOLUME_SCALE,
      g_param_spec_boolean ("log-volume-scale",
          "Logarithmic volume scale",
          "Use a logarithmic volume scale if set to TRUE, or a linear scale if set to FALSE",
          DEFAULT_LOG_VOLUME_SCALE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)
      );
  g_object_class_install_property (object_class,
      PROP_ENHANCED_RESAMPLING,
      g_param_spec_boolean ("enhanced-resampling",
          "Enhanced resampling",
          "Use enhanced resampling if set to TRUE, or linear interpolation if set to FALSE",
          DEFAULT_ENHANCED_RESAMPLING,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)
      );
  g_object_class_install_property (object_class,
      PROP_REVERB,
      g_param_spec_boolean ("reverb",
          "Reverb",
          "Whether or not to enable the WildMidi 8 reflection reverb engine to add more depth to the sound",
          DEFAULT_REVERB, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)
      );
  /* 2*2 => stereo output with S16 samples; the division ensures that no overflow can happen */
  g_object_class_install_property (object_class,
      PROP_OUTPUT_BUFFER_SIZE,
      g_param_spec_uint ("output-buffer-size",
          "Output buffer size",
          "Size of each output buffer, in samples (actual size can be smaller than this during flush or EOS)",
          1, G_MAXUINT / (2 * 2),
          DEFAULT_OUTPUT_BUFFER_SIZE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)
      );
}


void
gst_wildmidi_dec_init (GstWildmidiDec * wildmidi_dec)
{
  wildmidi_dec->song = NULL;

  wildmidi_dec->log_volume_scale = DEFAULT_LOG_VOLUME_SCALE;
  wildmidi_dec->enhanced_resampling = DEFAULT_ENHANCED_RESAMPLING;
  wildmidi_dec->reverb = DEFAULT_REVERB;
  wildmidi_dec->output_buffer_size = DEFAULT_OUTPUT_BUFFER_SIZE;

  gst_wildmidi_init_library ();
}


static void
gst_wildmidi_dec_finalize (GObject * object)
{
  GstWildmidiDec *wildmidi_dec = GST_WILDMIDI_DEC (object);

  if (wildmidi_dec->song != NULL)
    WildMidi_Close (wildmidi_dec->song);

  gst_wildmidi_shutdown_library ();

  G_OBJECT_CLASS (gst_wildmidi_dec_parent_class)->finalize (object);
}


static void
gst_wildmidi_dec_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstWildmidiDec *wildmidi_dec;

  wildmidi_dec = GST_WILDMIDI_DEC (object);

  switch (prop_id) {
    case PROP_LOG_VOLUME_SCALE:
      GST_NONSTREAM_AUDIO_DECODER_LOCK_MUTEX (object);
      wildmidi_dec->log_volume_scale = g_value_get_boolean (value);
      gst_wildmidi_dec_update_options (wildmidi_dec);
      GST_NONSTREAM_AUDIO_DECODER_UNLOCK_MUTEX (object);
      break;

    case PROP_ENHANCED_RESAMPLING:
      GST_NONSTREAM_AUDIO_DECODER_LOCK_MUTEX (object);
      wildmidi_dec->enhanced_resampling = g_value_get_boolean (value);
      gst_wildmidi_dec_update_options (wildmidi_dec);
      GST_NONSTREAM_AUDIO_DECODER_UNLOCK_MUTEX (object);
      break;

    case PROP_REVERB:
      GST_NONSTREAM_AUDIO_DECODER_LOCK_MUTEX (object);
      wildmidi_dec->reverb = g_value_get_boolean (value);
      gst_wildmidi_dec_update_options (wildmidi_dec);
      GST_NONSTREAM_AUDIO_DECODER_UNLOCK_MUTEX (object);
      break;

    case PROP_OUTPUT_BUFFER_SIZE:
      GST_NONSTREAM_AUDIO_DECODER_LOCK_MUTEX (object);
      wildmidi_dec->output_buffer_size = g_value_get_uint (value);
      GST_NONSTREAM_AUDIO_DECODER_UNLOCK_MUTEX (object);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}


static void
gst_wildmidi_dec_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstWildmidiDec *wildmidi_dec = GST_WILDMIDI_DEC (object);

  switch (prop_id) {
    case PROP_LOG_VOLUME_SCALE:
      GST_NONSTREAM_AUDIO_DECODER_LOCK_MUTEX (object);
      g_value_set_boolean (value, wildmidi_dec->log_volume_scale);
      GST_NONSTREAM_AUDIO_DECODER_UNLOCK_MUTEX (object);
      break;

    case PROP_ENHANCED_RESAMPLING:
      GST_NONSTREAM_AUDIO_DECODER_LOCK_MUTEX (object);
      g_value_set_boolean (value, wildmidi_dec->enhanced_resampling);
      GST_NONSTREAM_AUDIO_DECODER_UNLOCK_MUTEX (object);
      break;

    case PROP_REVERB:
      GST_NONSTREAM_AUDIO_DECODER_LOCK_MUTEX (object);
      g_value_set_boolean (value, wildmidi_dec->reverb);
      GST_NONSTREAM_AUDIO_DECODER_UNLOCK_MUTEX (object);
      break;

    case PROP_OUTPUT_BUFFER_SIZE:
      GST_NONSTREAM_AUDIO_DECODER_LOCK_MUTEX (object);
      g_value_set_uint (value, wildmidi_dec->output_buffer_size);
      GST_NONSTREAM_AUDIO_DECODER_UNLOCK_MUTEX (object);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}


static gboolean
gst_wildmidi_dec_seek (GstNonstreamAudioDecoder * dec,
    GstClockTime * new_position)
{
  GstWildmidiDec *wildmidi_dec = GST_WILDMIDI_DEC (dec);
  unsigned long int sample_pos =
      gst_util_uint64_scale_int (*new_position, WILDMIDI_SAMPLE_RATE,
      GST_SECOND);

  if (G_UNLIKELY (wildmidi_dec->song == NULL))
    return FALSE;

  WildMidi_FastSeek (wildmidi_dec->song, &sample_pos);

  *new_position =
      gst_util_uint64_scale_int (sample_pos, GST_SECOND, WILDMIDI_SAMPLE_RATE);
  return TRUE;
}


static GstClockTime
gst_wildmidi_dec_tell (GstNonstreamAudioDecoder * dec)
{
  GstWildmidiDec *wildmidi_dec = GST_WILDMIDI_DEC (dec);
  struct _WM_Info *info;

  if (G_UNLIKELY (wildmidi_dec->song == NULL))
    return GST_CLOCK_TIME_NONE;

  info = WildMidi_GetInfo (wildmidi_dec->song);
  return gst_util_uint64_scale_int (info->current_sample, GST_SECOND,
      WILDMIDI_SAMPLE_RATE);
}


static gboolean
gst_wildmidi_dec_load_from_buffer (GstNonstreamAudioDecoder * dec,
    GstBuffer * source_data, G_GNUC_UNUSED guint initial_subsong,
    G_GNUC_UNUSED GstNonstreamAudioSubsongMode initial_subsong_mode,
    GstClockTime * initial_position,
    GstNonstreamAudioOutputMode * initial_output_mode,
    G_GNUC_UNUSED gint * initial_num_loops)
{
  GstWildmidiDec *wildmidi_dec = GST_WILDMIDI_DEC (dec);
  GstMapInfo buffer_map;


  if (g_atomic_int_get (&wildmidi_initialized) == 0) {
    GST_ERROR_OBJECT (wildmidi_dec,
        "Could not start loading: WildMidi is not initialized");
    return FALSE;
  }


  /* Set output format */
  if (!gst_nonstream_audio_decoder_set_output_format_simple (dec,
          WILDMIDI_SAMPLE_RATE, GST_AUDIO_FORMAT_S16, WILDMIDI_NUM_CHANNELS))
    return FALSE;


  /* Load MIDI */
  gst_buffer_map (source_data, &buffer_map, GST_MAP_READ);
  wildmidi_dec->song = WildMidi_OpenBuffer (buffer_map.data, buffer_map.size);
  gst_buffer_unmap (source_data, &buffer_map);

  if (wildmidi_dec->song == NULL) {
    GST_ERROR_OBJECT (wildmidi_dec, "Could not load MIDI tune");
    return FALSE;
  }

  gst_wildmidi_dec_update_options (wildmidi_dec);


  /* Seek to initial position */
  if (*initial_position != 0) {
    unsigned long int sample_pos =
        gst_util_uint64_scale_int (*initial_position, WILDMIDI_SAMPLE_RATE,
        GST_SECOND);
    WildMidi_FastSeek (wildmidi_dec->song, &sample_pos);
    *initial_position =
        gst_util_uint64_scale_int (sample_pos, GST_SECOND,
        WILDMIDI_SAMPLE_RATE);
  }


  /* LOOPING output mode is not supported */
  *initial_output_mode = GST_NONSTREAM_AUDIO_OUTPUT_MODE_STEADY;


  return TRUE;
}


static guint
gst_wildmidi_dec_get_current_subsong (G_GNUC_UNUSED GstNonstreamAudioDecoder *
    dec)
{
  return 0;
}


static guint
gst_wildmidi_dec_get_num_subsongs (G_GNUC_UNUSED GstNonstreamAudioDecoder * dec)
{
  return 1;
}


static GstClockTime
gst_wildmidi_dec_get_subsong_duration (GstNonstreamAudioDecoder * dec,
    G_GNUC_UNUSED guint subsong)
{
  GstWildmidiDec *wildmidi_dec = GST_WILDMIDI_DEC (dec);
  struct _WM_Info *info;

  if (G_UNLIKELY (wildmidi_dec->song == NULL))
    return GST_CLOCK_TIME_NONE;

  info = WildMidi_GetInfo (wildmidi_dec->song);
  return gst_util_uint64_scale_int (info->approx_total_samples, GST_SECOND,
      WILDMIDI_SAMPLE_RATE);
}


static guint
gst_wildmidi_dec_get_supported_output_modes (G_GNUC_UNUSED
    GstNonstreamAudioDecoder * dec)
{
  return 1u << GST_NONSTREAM_AUDIO_OUTPUT_MODE_STEADY;
}


static gboolean
gst_wildmidi_dec_decode (GstNonstreamAudioDecoder * dec, GstBuffer ** buffer,
    guint * num_samples)
{
  GstWildmidiDec *wildmidi_dec = GST_WILDMIDI_DEC (dec);
  GstMapInfo info;
  GstBuffer *outbuf;
  gsize outbuf_size;
  int decoded_size_in_bytes;

  if (G_UNLIKELY (wildmidi_dec->song == NULL))
    return FALSE;

  /* Allocate output buffer
   * Multiply by 2 to accommodate for the sample size (16 bit = 2 byte) */
  outbuf_size = wildmidi_dec->output_buffer_size * 2 * WILDMIDI_NUM_CHANNELS;
  outbuf =
      gst_nonstream_audio_decoder_allocate_output_buffer (dec, outbuf_size);
  if (G_UNLIKELY (outbuf == NULL))
    return FALSE;

  /* The actual decoding */
  gst_buffer_map (outbuf, &info, GST_MAP_WRITE);
  decoded_size_in_bytes =
      WildMidi_GetOutput (wildmidi_dec->song, (int8_t *) (info.data),
      info.size);
  gst_buffer_unmap (outbuf, &info);

  if (decoded_size_in_bytes == 0) {
    gst_buffer_unref (outbuf);
    return FALSE;
  }

  *buffer = outbuf;
  *num_samples = decoded_size_in_bytes / 2 / WILDMIDI_NUM_CHANNELS;

  return TRUE;
}


static void
gst_wildmidi_dec_update_options (GstWildmidiDec * wildmidi_dec)
{
  unsigned short int options = 0;

  if (wildmidi_dec->song == NULL)
    return;

  if (wildmidi_dec->log_volume_scale)
    options |= WM_MO_LOG_VOLUME;
  if (wildmidi_dec->enhanced_resampling)
    options |= WM_MO_ENHANCED_RESAMPLING;
  if (wildmidi_dec->reverb)
    options |= WM_MO_REVERB;

  WildMidi_SetOption (wildmidi_dec->song,
      WM_MO_LOG_VOLUME | WM_MO_ENHANCED_RESAMPLING | WM_MO_REVERB, options);
}


static gboolean
plugin_init (GstPlugin * plugin)
{
  return GST_ELEMENT_REGISTER (wildmididec, plugin);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    wildmidi,
    "WildMidi-based MIDI playback plugin",
    plugin_init, VERSION, "LGPL", GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
