/*
 * gstfluiddec - fluiddec plugin for gstreamer
 *
 * Copyright 2013 Wim Taymans <wim.taymans@gmail.com>
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
 * SECTION:element-fluiddec
 * @title: fluiddec
 * @see_also: timidity, wildmidi
 *
 * This element renders midi-events as audio streams using
 * [Fluidsynth](http://fluidsynth.sourceforge.net/).
 * It offers better sound quality compared to the timidity or wildmidi element.
 *
 * ## Example pipeline
 * |[
 * gst-launch-1.0 filesrc location=song.mid ! midiparse ! fluiddec ! pulsesink
 * ]| This example pipeline will parse the midi and render to raw audio which is
 * played via pulseaudio.
 *
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#define FLUID_DEC_RATE 44100
#define FLUID_DEC_BPS  (4 * 2)

#include <gst/gst.h>
#include <string.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <gst/audio/audio.h>

#include "gstfluiddec.h"

#define GST_HAVE_FLUIDSYNTH_VERSION(major,minor,micro) \
  (FLUIDSYNTH_VERSION_MAJOR > (major) || \
  (FLUIDSYNTH_VERSION_MAJOR == (major) && FLUIDSYNTH_VERSION_MINOR > (minor)) || \
  (FLUIDSYNTH_VERSION_MAJOR == (major) && FLUIDSYNTH_VERSION_MINOR == (minor) && FLUIDSYNTH_VERSION_MICRO >= (micro)))

GST_DEBUG_CATEGORY_STATIC (gst_fluid_dec_debug);
#define GST_CAT_DEFAULT gst_fluid_dec_debug

#if !GST_HAVE_FLUIDSYNTH_VERSION(2, 2, 0)
#define fluid_synth_chorus_on(synth,fx_group,on) fluid_synth_set_chorus_on(synth,on)
#define fluid_synth_reverb_on(synth,fx_group,on) fluid_synth_set_reverb_on(synth,on)
#endif

enum
{
  /* FILL ME */
  LAST_SIGNAL
};

#define DEFAULT_SOUNDFONT       NULL
#define DEFAULT_SYNTH_CHORUS    TRUE
#define DEFAULT_SYNTH_REVERB    TRUE
#define DEFAULT_SYNTH_GAIN      0.2
#define DEFAULT_SYNTH_POLYPHONY 256

enum
{
  PROP_0,
  PROP_SOUNDFONT,
  PROP_SYNTH_CHORUS,
  PROP_SYNTH_REVERB,
  PROP_SYNTH_GAIN,
  PROP_SYNTH_POLYPHONY
};

static void gst_fluid_dec_finalize (GObject * object);

static gboolean gst_fluid_dec_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event);

static GstStateChangeReturn gst_fluid_dec_change_state (GstElement * element,
    GstStateChange transition);

static GstFlowReturn gst_fluid_dec_chain (GstPad * sinkpad, GstObject * parent,
    GstBuffer * buffer);

static void gst_fluid_dec_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_fluid_dec_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static gboolean fluiddec_element_init (GstPlugin * plugin);

static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-midi-event")
    );

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw, "
        "format = (string) " GST_AUDIO_NE (F32) ", "
        "rate = (int) " G_STRINGIFY (FLUID_DEC_RATE) ", "
        "channels = (int) 2, " "layout = (string) interleaved"));

#define parent_class gst_fluid_dec_parent_class
G_DEFINE_TYPE (GstFluidDec, gst_fluid_dec, GST_TYPE_ELEMENT);
GST_ELEMENT_REGISTER_DEFINE_CUSTOM (fluiddec, fluiddec_element_init);

/* initialize the plugin's class */
static void
gst_fluid_dec_class_init (GstFluidDecClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->finalize = gst_fluid_dec_finalize;
  gobject_class->set_property = gst_fluid_dec_set_property;
  gobject_class->get_property = gst_fluid_dec_get_property;

  g_object_class_install_property (gobject_class, PROP_SOUNDFONT,
      g_param_spec_string ("soundfont",
          "Soundfont", "the filename of a soundfont (NULL for default)",
          DEFAULT_SOUNDFONT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_SYNTH_CHORUS,
      g_param_spec_boolean ("synth-chorus",
          "Synth Chorus", "Turn the chorus on or off",
          DEFAULT_SYNTH_CHORUS, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_SYNTH_REVERB,
      g_param_spec_boolean ("synth-reverb",
          "Synth Reverb", "Turn the reverb on or off",
          DEFAULT_SYNTH_REVERB, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_SYNTH_GAIN,
      g_param_spec_double ("synth-gain",
          "Synth Gain", "Set the master gain", 0.0, 10.0,
          DEFAULT_SYNTH_GAIN, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_SYNTH_POLYPHONY,
      g_param_spec_int ("synth-polyphony",
          "Synth Polyphony", "The number of simultaneous voices", 1, 65535,
          DEFAULT_SYNTH_POLYPHONY, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_add_static_pad_template (gstelement_class, &src_factory);
  gst_element_class_add_static_pad_template (gstelement_class, &sink_factory);

  gst_element_class_set_static_metadata (gstelement_class, "Fluidsynth",
      "Codec/Decoder/Audio",
      "Midi Synthesizer Element", "Wim Taymans <wim.taymans@gmail.com>");

  gstelement_class->change_state = gst_fluid_dec_change_state;
}

/* initialize the new element
 * instantiate pads and add them to element
 * set functions
 * initialize structure
 */
static void
gst_fluid_dec_init (GstFluidDec * filter)
{
  filter->sinkpad = gst_pad_new_from_static_template (&sink_factory, "sink");
  gst_pad_set_event_function (filter->sinkpad, gst_fluid_dec_sink_event);
  gst_pad_set_chain_function (filter->sinkpad, gst_fluid_dec_chain);
  gst_element_add_pad (GST_ELEMENT (filter), filter->sinkpad);

  filter->srcpad = gst_pad_new_from_static_template (&src_factory, "src");
  gst_pad_use_fixed_caps (filter->srcpad);
  gst_element_add_pad (GST_ELEMENT (filter), filter->srcpad);

  filter->soundfont = g_strdup (DEFAULT_SOUNDFONT);
  filter->synth_chorus = DEFAULT_SYNTH_CHORUS;
  filter->synth_reverb = DEFAULT_SYNTH_REVERB;
  filter->synth_gain = DEFAULT_SYNTH_GAIN;
  filter->synth_polyphony = DEFAULT_SYNTH_POLYPHONY;

  filter->settings = new_fluid_settings ();

  /* http://www.fluidsynth.org/api/fluidsettings.xml */
  fluid_settings_setnum (filter->settings, "synth.sample-rate", FLUID_DEC_RATE);

  /* FIXME: Initialize after caps negotiation so we can support more rates */
  filter->synth = new_fluid_synth (filter->settings);
  filter->sf = -1;

  fluid_synth_chorus_on (filter->synth, -1, filter->synth_chorus);
  fluid_synth_reverb_on (filter->synth, -1, filter->synth_reverb);
  fluid_synth_set_gain (filter->synth, filter->synth_gain);
  fluid_synth_set_polyphony (filter->synth, filter->synth_polyphony);
}

static void
gst_fluid_dec_finalize (GObject * object)
{
  GstFluidDec *fluiddec = GST_FLUID_DEC (object);

  delete_fluid_synth (fluiddec->synth);
  delete_fluid_settings (fluiddec->settings);
  g_free (fluiddec->soundfont);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

#if 0
static GstBuffer *
gst_fluid_dec_clip_buffer (GstFluidDec * fluiddec, GstBuffer * buffer)
{
  guint64 start, stop;
  guint64 new_start, new_stop;
  gint64 offset, length;

  /* clipping disabled for now */
  return buffer;

  start = GST_BUFFER_OFFSET (buffer);
  stop = GST_BUFFER_OFFSET_END (buffer);

  if (!gst_segment_clip (&fluiddec->segment, GST_FORMAT_DEFAULT,
          start, stop, &new_start, &new_stop)) {
    gst_buffer_unref (buffer);
    return NULL;
  }

  if (start == new_start && stop == new_stop)
    return buffer;

  offset = new_start - start;
  length = new_stop - new_start;

  buffer = gst_buffer_make_writable (buffer);
  gst_buffer_resize (buffer, offset, length);

  GST_BUFFER_OFFSET (buffer) = new_start;
  GST_BUFFER_OFFSET_END (buffer) = new_stop;
  GST_BUFFER_TIMESTAMP (buffer) =
      gst_util_uint64_scale_int (new_start, GST_SECOND, FLUID_DEC_RATE);
  GST_BUFFER_DURATION (buffer) =
      gst_util_uint64_scale_int (new_stop, GST_SECOND, FLUID_DEC_RATE) -
      GST_BUFFER_TIMESTAMP (buffer);

  return buffer;
}
#endif

static void
gst_fluid_dec_reset (GstFluidDec * fluiddec)
{
  fluid_synth_system_reset (fluiddec->synth);
  fluiddec->last_pts = GST_CLOCK_TIME_NONE;
}

static gboolean
gst_fluid_dec_sink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  gboolean res;
  GstFluidDec *fluiddec = GST_FLUID_DEC (parent);

  GST_DEBUG_OBJECT (pad, "%s event received", GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:
    {
      GstCaps *caps;

      caps = gst_caps_new_simple ("audio/x-raw",
          "format", G_TYPE_STRING, GST_AUDIO_NE (F32),
          "rate", G_TYPE_INT, FLUID_DEC_RATE,
          "channels", G_TYPE_INT, 2,
          "layout", G_TYPE_STRING, "interleaved", NULL);

      res = gst_pad_push_event (fluiddec->srcpad, gst_event_new_caps (caps));
      gst_caps_unref (caps);
      gst_event_unref (event);
      break;
    }
    case GST_EVENT_SEGMENT:
      gst_event_copy_segment (event, &fluiddec->segment);
      GST_DEBUG_OBJECT (fluiddec, "configured segment %" GST_SEGMENT_FORMAT,
          &fluiddec->segment);
      res = gst_pad_event_default (pad, parent, event);
      break;
    case GST_EVENT_FLUSH_STOP:
      gst_fluid_dec_reset (fluiddec);
      res = gst_pad_event_default (pad, parent, event);
      break;
    case GST_EVENT_EOS:
      /* FIXME, push last samples */
      res = gst_pad_event_default (pad, parent, event);
      break;
    default:
      res = gst_pad_event_default (pad, parent, event);
      break;
  }
  return res;
}

static GstFlowReturn
produce_samples (GstFluidDec * fluiddec, GstClockTime pts, guint64 sample)
{
  GstClockTime duration, timestamp;
  guint64 samples, offset;
  GstMapInfo info;
  GstBuffer *outbuf;

  samples = sample - fluiddec->last_sample;
  duration = pts - fluiddec->last_pts;
  offset = fluiddec->last_sample;
  timestamp = fluiddec->last_pts;

  fluiddec->last_pts = pts;
  fluiddec->last_sample = sample;

  if (samples == 0)
    return GST_FLOW_OK;

  GST_DEBUG_OBJECT (fluiddec, "duration %" GST_TIME_FORMAT
      ", samples %" G_GUINT64_FORMAT, GST_TIME_ARGS (duration), samples);

  outbuf = gst_buffer_new_allocate (NULL, samples * FLUID_DEC_BPS, NULL);

  gst_buffer_map (outbuf, &info, GST_MAP_WRITE);
  fluid_synth_write_float (fluiddec->synth, samples, info.data, 0, 2,
      info.data, 1, 2);
  gst_buffer_unmap (outbuf, &info);

  GST_BUFFER_DTS (outbuf) = timestamp;
  GST_BUFFER_PTS (outbuf) = timestamp;
  GST_BUFFER_DURATION (outbuf) = duration;
  GST_BUFFER_OFFSET (outbuf) = offset;
  GST_BUFFER_OFFSET_END (outbuf) = offset + samples;

  if (fluiddec->discont) {
    GST_BUFFER_FLAG_SET (outbuf, GST_BUFFER_FLAG_DISCONT);
    fluiddec->discont = FALSE;
  }

  return gst_pad_push (fluiddec->srcpad, outbuf);
}

static void
handle_buffer (GstFluidDec * fluiddec, GstBuffer * buffer)
{
  GstMapInfo info;
  guint8 event;

  gst_buffer_map (buffer, &info, GST_MAP_READ);

  if (info.size == 0)
    goto done;

  event = info.data[0];

  switch (event & 0xf0) {
    case 0xf0:
      switch (event) {
        case 0xff:
          GST_DEBUG_OBJECT (fluiddec, "system reset");
          fluid_synth_system_reset (fluiddec->synth);
          break;
        case 0xf0:
        case 0xf7:
          GST_DEBUG_OBJECT (fluiddec, "sysex 0x%02x", event);
          GST_MEMDUMP_OBJECT (fluiddec, "bytes ", info.data + 1, info.size - 1);
          fluid_synth_sysex (fluiddec->synth, (char *) info.data + 1,
              info.size - 1, NULL, NULL, NULL, 0);

          break;
        case 0xf9:
          GST_LOG_OBJECT (fluiddec, "midi tick");
          break;
        default:
          GST_WARNING_OBJECT (fluiddec, "unhandled event 0x%02x", event);
          break;
      }
      break;
    default:
    {
      guint8 channel, p1, p2;

      channel = event & 0x0f;

      p1 = info.size > 1 ? info.data[1] & 0x7f : 0;
      p2 = info.size > 2 ? info.data[2] & 0x7f : 0;

      GST_DEBUG_OBJECT (fluiddec, "event 0x%02x channel %d, 0x%02x 0x%02x",
          event, channel, p1, p2);

      switch (event & 0xf0) {
        case 0x80:
          fluid_synth_noteoff (fluiddec->synth, channel, p1);
          break;
        case 0x90:
          fluid_synth_noteon (fluiddec->synth, channel, p1, p2);
          break;
        case 0xA0:
          /* aftertouch */
          break;
        case 0xB0:
          fluid_synth_cc (fluiddec->synth, channel, p1, p2);
          break;
        case 0xC0:
          fluid_synth_program_change (fluiddec->synth, channel, p1);
          break;
        case 0xD0:
          fluid_synth_channel_pressure (fluiddec->synth, channel, p1);
          break;
        case 0xE0:
          fluid_synth_pitch_bend (fluiddec->synth, channel, (p2 << 7) | p1);
          break;
        default:
          break;
      }
      break;
    }
  }

done:

  gst_buffer_unmap (buffer, &info);
}

static GstFlowReturn
gst_fluid_dec_chain (GstPad * sinkpad, GstObject * parent, GstBuffer * buffer)
{
  GstFlowReturn res = GST_FLOW_OK;
  GstFluidDec *fluiddec;
  GstClockTime pts;

  fluiddec = GST_FLUID_DEC (parent);

  if (GST_BUFFER_IS_DISCONT (buffer)) {
    fluiddec->discont = TRUE;
  }

  pts = GST_BUFFER_PTS (buffer);

  if (pts != GST_CLOCK_TIME_NONE) {
    guint64 sample =
        gst_util_uint64_scale_int (pts, FLUID_DEC_RATE, GST_SECOND);

    if (fluiddec->last_pts == GST_CLOCK_TIME_NONE) {
      fluiddec->last_pts = pts;
      fluiddec->last_sample = sample;
    } else if (fluiddec->last_pts < pts) {
      /* generate samples for the elapsed time */
      res = produce_samples (fluiddec, pts, sample);
    }
  }

  if (res == GST_FLOW_OK) {
    handle_buffer (fluiddec, buffer);
  }
  gst_buffer_unref (buffer);

  return res;
}

static gboolean
gst_fluid_dec_open (GstFluidDec * fluiddec)
{
  GDir *dir;
  GError *error = NULL;
  const gchar *const *sharedirs;

  if (fluiddec->sf != -1)
    return TRUE;

  if (fluiddec->soundfont) {
    GST_DEBUG_OBJECT (fluiddec, "loading soundfont file %s",
        fluiddec->soundfont);

    fluiddec->sf = fluid_synth_sfload (fluiddec->synth, fluiddec->soundfont, 1);
    if (fluiddec->sf == -1)
      goto load_failed;

    GST_DEBUG_OBJECT (fluiddec, "loaded soundfont file %s",
        fluiddec->soundfont);
  } else {
    gint i, j;
    /* ubuntu/debian in sounds/sf[23], fedora in soundfonts */
    static const gchar *paths[] =
        { "sounds/sf3/", "sounds/sf2/", "soundfonts/", NULL };

    sharedirs = g_get_system_data_dirs ();

    for (i = 0; sharedirs[i]; i++) {
      for (j = 0; paths[j]; j++) {
        gchar *soundfont_path = g_build_path ("/", sharedirs[i], paths[j],
            NULL);
        GST_DEBUG_OBJECT (fluiddec, "Trying to list contents of a %s directory",
            soundfont_path);
        error = NULL;
        dir = g_dir_open (soundfont_path, 0, &error);
        if (dir == NULL) {
          GST_DEBUG_OBJECT (fluiddec,
              "Can't open a potential soundfont directory %s: %s",
              soundfont_path, error->message);
          g_free (soundfont_path);
          g_error_free (error);
          continue;
        }

        while (TRUE) {
          const gchar *name;
          gchar *filename;

          if ((name = g_dir_read_name (dir)) == NULL)
            break;

          filename = g_build_filename (soundfont_path, name, NULL);

          GST_DEBUG_OBJECT (fluiddec, "loading soundfont file %s", filename);
          fluiddec->sf = fluid_synth_sfload (fluiddec->synth, filename, 1);
          if (fluiddec->sf != -1) {
            GST_DEBUG_OBJECT (fluiddec, "loaded soundfont file %s", filename);
            g_free (filename);
            g_dir_close (dir);
            g_free (soundfont_path);
            goto done;
          }
          GST_DEBUG_OBJECT (fluiddec, "could not load soundfont file %s",
              filename);
          g_free (filename);
        }
        g_dir_close (dir);
        g_free (soundfont_path);
      }
    }
    if (fluiddec->sf == -1) {
      goto no_soundfont;
    }
  }
done:
  return TRUE;

  /* ERRORS */
load_failed:
  {
    GST_ELEMENT_ERROR (fluiddec, RESOURCE, OPEN_READ,
        ("Can't open soundfont %s", fluiddec->soundfont),
        ("failed to open soundfont file %s for reading", fluiddec->soundfont));
    return FALSE;
  }
no_soundfont:
  {
    GST_ELEMENT_ERROR (fluiddec, RESOURCE, OPEN_READ,
        ("Can't find a soundfont file in subdirectories of XDG_DATA_DIRS paths"),
        ("no usable soundfont files found in subdirectories of XDG_DATA_DIRS"));
    return FALSE;
  }
}

static gboolean
gst_fluid_dec_close (GstFluidDec * fluiddec)
{
  if (fluiddec->sf) {
    fluid_synth_sfunload (fluiddec->synth, fluiddec->sf, 1);
    fluiddec->sf = -1;
  }
  return TRUE;
}

static GstStateChangeReturn
gst_fluid_dec_change_state (GstElement * element, GstStateChange transition)
{
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  GstFluidDec *fluiddec = GST_FLUID_DEC (element);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      if (!gst_fluid_dec_open (fluiddec))
        goto open_failed;
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      gst_fluid_dec_reset (fluiddec);
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      gst_fluid_dec_close (fluiddec);
      break;
    default:
      break;
  }

  return ret;

  /* ERRORS */
open_failed:
  {
    GST_ERROR_OBJECT (fluiddec, "could not open");
    return GST_STATE_CHANGE_FAILURE;
  }
}

static void
gst_fluid_dec_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstFluidDec *fluiddec = GST_FLUID_DEC (object);

  switch (prop_id) {
    case PROP_SOUNDFONT:
      g_free (fluiddec->soundfont);
      fluiddec->soundfont = g_value_dup_string (value);
      break;
    case PROP_SYNTH_CHORUS:
      fluiddec->synth_chorus = g_value_get_boolean (value);
      fluid_synth_chorus_on (fluiddec->synth, -1, fluiddec->synth_chorus);
      break;
    case PROP_SYNTH_REVERB:
      fluiddec->synth_reverb = g_value_get_boolean (value);
      fluid_synth_reverb_on (fluiddec->synth, -1, fluiddec->synth_reverb);
      break;
    case PROP_SYNTH_GAIN:
      fluiddec->synth_gain = g_value_get_double (value);
      fluid_synth_set_gain (fluiddec->synth, fluiddec->synth_gain);
      break;
    case PROP_SYNTH_POLYPHONY:
      fluiddec->synth_polyphony = g_value_get_int (value);
      fluid_synth_set_polyphony (fluiddec->synth, fluiddec->synth_polyphony);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_fluid_dec_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstFluidDec *fluiddec = GST_FLUID_DEC (object);

  switch (prop_id) {
    case PROP_SOUNDFONT:
      g_value_set_string (value, fluiddec->soundfont);
      break;
    case PROP_SYNTH_CHORUS:
      g_value_set_boolean (value, fluiddec->synth_chorus);
      break;
    case PROP_SYNTH_REVERB:
      g_value_set_boolean (value, fluiddec->synth_reverb);
      break;
    case PROP_SYNTH_GAIN:
      g_value_set_double (value, fluiddec->synth_gain);
      break;
    case PROP_SYNTH_POLYPHONY:
      g_value_set_int (value, fluiddec->synth_polyphony);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* fluid_synth log handler */
static void
gst_fluid_synth_error_log_function (int level, const char *message, void *data)
{
  GST_ERROR ("%s", message);
}

static void
gst_fluid_synth_warning_log_function (int level, const char *message,
    void *data)
{
  GST_WARNING ("%s", message);
}

static void
gst_fluid_synth_info_log_function (int level, const char *message, void *data)
{
  GST_INFO ("%s", message);
}

static void
gst_fluid_synth_debug_log_function (int level, const char *message, void *data)
{
  GST_DEBUG ("%s", message);
}

static gboolean
fluiddec_element_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (gst_fluid_dec_debug, "fluiddec",
      0, "Fluidsynth MIDI decoder plugin");

  /* We modify FluidSynth's global state here; let's hope nobody tries to use
   * it natively alongside this plugin. */

#ifndef GST_DISABLE_GST_DEBUG
  fluid_set_log_function (FLUID_PANIC,
      (fluid_log_function_t) gst_fluid_synth_error_log_function, NULL);
  fluid_set_log_function (FLUID_ERR,
      (fluid_log_function_t) gst_fluid_synth_warning_log_function, NULL);
  fluid_set_log_function (FLUID_WARN,
      (fluid_log_function_t) gst_fluid_synth_warning_log_function, NULL);
  fluid_set_log_function (FLUID_INFO,
      (fluid_log_function_t) gst_fluid_synth_info_log_function, NULL);
  fluid_set_log_function (FLUID_DBG,
      (fluid_log_function_t) gst_fluid_synth_debug_log_function, NULL);
#else
  fluid_set_log_function (FLUID_PANIC, NULL, NULL);
  fluid_set_log_function (FLUID_ERR, NULL, NULL);
  fluid_set_log_function (FLUID_WARN, NULL, NULL);
  fluid_set_log_function (FLUID_INFO, NULL, NULL);
  fluid_set_log_function (FLUID_DBG, NULL, NULL);
#endif

  return gst_element_register (plugin, "fluiddec",
      GST_RANK_SECONDARY, GST_TYPE_FLUID_DEC);
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  return GST_ELEMENT_REGISTER (fluiddec, plugin);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    fluidsynthmidi,
    "Fluidsynth MIDI Plugin",
    plugin_init, VERSION, "LGPL", GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
