/*
 * GStreamer
 *
 * Copyright (C) 2005 Wim Taymans <wim@fluendo.com>
 * Copyright (C) 2006 Tim-Philipp Müller <tim centricular net>
 * Copyright (C) 2009-2010 Chris Robinson <chris.kcat@gmail.com>
 * Copyright (C) 2013 Juan Manuel Borges Caño <juanmabcmail@gmail.com>
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
 * SECTION:element-openalsink
 * @title: openalsink
 * @see_also: openalsrc
 * @short_description: capture raw audio samples through OpenAL
 *
 * This element plays raw audio samples through OpenAL.
 *
 * Unfortunately the capture API doesn't have a format enumeration/check. all you can do is try opening it and see if it works.
 *
 * ## Example pipelines
 * |[
 * gst-launch-1.0 audiotestsrc ! audioconvert ! volume volume=0.5 ! openalsink
 * ]| will play a sine wave (continuous beep sound) through OpenAL.
 * |[
 * gst-launch-1.0 filesrc location=stream.wav ! decodebin ! audioconvert ! openalsink
 * ]| will play a wav audio file through OpenAL.
 * |[
 * gst-launch-1.0 openalsrc ! "audio/x-raw,format=S16LE,rate=44100" ! audioconvert ! volume volume=0.25 ! openalsink
 * ]| will capture and play audio through OpenAL.
 *
 */

/*
 * DEV:
 * To get better timing/delay information you may also be interested in this:
 *  http://kcat.strangesoft.net/openal-extensions/SOFT_source_latency.txt
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/gsterror.h>

GST_DEBUG_CATEGORY_EXTERN (openal_debug);
#define GST_CAT_DEFAULT openal_debug

#include "gstopenalelements.h"
#include "gstopenalsink.h"

static void gst_openal_sink_dispose (GObject * object);
static void gst_openal_sink_finalize (GObject * object);

static void gst_openal_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_openal_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static GstCaps *gst_openal_sink_getcaps (GstBaseSink * basesink,
    GstCaps * filter);
static gboolean gst_openal_sink_open (GstAudioSink * audiosink);
static gboolean gst_openal_sink_close (GstAudioSink * audiosink);
static gboolean gst_openal_sink_prepare (GstAudioSink * audiosink,
    GstAudioRingBufferSpec * spec);
static gboolean gst_openal_sink_unprepare (GstAudioSink * audiosink);
static gint gst_openal_sink_write (GstAudioSink * audiosink, gpointer data,
    guint length);
static guint gst_openal_sink_delay (GstAudioSink * audiosink);
static void gst_openal_sink_reset (GstAudioSink * audiosink);

#define OPENAL_DEFAULT_DEVICE NULL

#define OPENAL_MIN_RATE 8000
#define OPENAL_MAX_RATE 192000

enum
{
  PROP_0,

  PROP_DEVICE,
  PROP_DEVICE_NAME,

  PROP_USER_DEVICE,
  PROP_USER_CONTEXT,
  PROP_USER_SOURCE
};

static GstStaticPadTemplate openalsink_factory =
    GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw, " "format = (string) " GST_AUDIO_NE (F64)
        ", " "rate = (int) [ 1, MAX ], " "channels = (int) [ 1, 2 ]; "
        "audio/x-raw, " "format = (string) " GST_AUDIO_NE (F32) ", "
        "rate = (int) [ 1, MAX ], " "channels = (int) [ 1, MAX ]; "
        "audio/x-raw, " "format = (string) " GST_AUDIO_NE (S16) ", "
        "rate = (int) [ 1, MAX ], " "channels = (int) [ 1, MAX ]; "
        "audio/x-raw, " "format = (string) " G_STRINGIFY (U8) ", "
        "rate = (int) [ 1, MAX ], " "channels = (int) [ 1, MAX ]; "
        /* These caps do not work on my card */
        // "audio/x-adpcm, " "layout = (string) ima, "
        // "rate = (int) [ 1, MAX ], " "channels = (int) [ 1, 2 ]; "
        // "audio/x-alaw, " "rate = (int) [ 1, MAX ], "
        // "channels = (int) [ 1, 2 ]; "
        // "audio/x-mulaw, " "rate = (int) [ 1, MAX ], "
        // "channels = (int) [ 1, MAX ]"
    )
    );

static PFNALCSETTHREADCONTEXTPROC palcSetThreadContext;
static PFNALCGETTHREADCONTEXTPROC palcGetThreadContext;

static inline ALCcontext *
pushContext (ALCcontext * context)
{
  ALCcontext *old;
  if (!palcGetThreadContext || !palcSetThreadContext)
    return NULL;

  old = palcGetThreadContext ();
  if (old != context)
    palcSetThreadContext (context);
  return old;
}

static inline void
popContext (ALCcontext * old, ALCcontext * context)
{
  if (!palcGetThreadContext || !palcSetThreadContext)
    return;

  if (old != context)
    palcSetThreadContext (old);
}

static inline ALenum
checkALError (const char *fname, unsigned int fline)
{
  ALenum err = alGetError ();
  if (err != AL_NO_ERROR)
    g_warning ("%s:%u: context error: %s", fname, fline, alGetString (err));
  return err;
}

#define checkALError() checkALError(__FILE__, __LINE__)

G_DEFINE_TYPE (GstOpenALSink, gst_openal_sink, GST_TYPE_AUDIO_SINK);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (openalsink, "openalsink",
    GST_RANK_SECONDARY, GST_TYPE_OPENAL_SINK, openal_element_init (plugin));

static void
gst_openal_sink_dispose (GObject * object)
{
  GstOpenALSink *sink = GST_OPENAL_SINK (object);

  if (sink->probed_caps)
    gst_caps_unref (sink->probed_caps);
  sink->probed_caps = NULL;

  G_OBJECT_CLASS (gst_openal_sink_parent_class)->dispose (object);
}

static void
gst_openal_sink_class_init (GstOpenALSinkClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *gstelement_class = (GstElementClass *) klass;
  GstBaseSinkClass *gstbasesink_class = (GstBaseSinkClass *) klass;
  GstAudioSinkClass *gstaudiosink_class = (GstAudioSinkClass *) klass;

  if (alcIsExtensionPresent (NULL, "ALC_EXT_thread_local_context")) {
    palcSetThreadContext = alcGetProcAddress (NULL, "alcSetThreadContext");
    palcGetThreadContext = alcGetProcAddress (NULL, "alcGetThreadContext");
  }

  gobject_class->dispose = GST_DEBUG_FUNCPTR (gst_openal_sink_dispose);
  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_openal_sink_finalize);
  gobject_class->set_property =
      GST_DEBUG_FUNCPTR (gst_openal_sink_set_property);
  gobject_class->get_property =
      GST_DEBUG_FUNCPTR (gst_openal_sink_get_property);

  gst_openal_sink_parent_class = g_type_class_peek_parent (klass);

  gstbasesink_class->get_caps = GST_DEBUG_FUNCPTR (gst_openal_sink_getcaps);

  gstaudiosink_class->open = GST_DEBUG_FUNCPTR (gst_openal_sink_open);
  gstaudiosink_class->close = GST_DEBUG_FUNCPTR (gst_openal_sink_close);
  gstaudiosink_class->prepare = GST_DEBUG_FUNCPTR (gst_openal_sink_prepare);
  gstaudiosink_class->unprepare = GST_DEBUG_FUNCPTR (gst_openal_sink_unprepare);
  gstaudiosink_class->write = GST_DEBUG_FUNCPTR (gst_openal_sink_write);
  gstaudiosink_class->delay = GST_DEBUG_FUNCPTR (gst_openal_sink_delay);
  gstaudiosink_class->reset = GST_DEBUG_FUNCPTR (gst_openal_sink_reset);

  g_object_class_install_property (gobject_class, PROP_DEVICE_NAME,
      g_param_spec_string ("device-name", "Device name",
          "Human-readable name of the opened device", "", G_PARAM_READABLE));

  g_object_class_install_property (gobject_class, PROP_DEVICE,
      g_param_spec_string ("device", "Device",
          "Human-readable name of the device", OPENAL_DEFAULT_DEVICE,
          G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_USER_DEVICE,
      g_param_spec_pointer ("user-device", "ALCdevice", "User device",
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_USER_CONTEXT,
      g_param_spec_pointer ("user-context", "ALCcontext", "User context",
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_USER_SOURCE,
      g_param_spec_uint ("user-source", "ALsource", "User source", 0, UINT_MAX,
          0, G_PARAM_READWRITE));

  gst_element_class_set_static_metadata (gstelement_class, "OpenAL Audio Sink",
      "Sink/Audio", "Output audio through OpenAL",
      "Juan Manuel Borges Caño <juanmabcmail@gmail.com>");

  gst_element_class_add_static_pad_template (gstelement_class,
      &openalsink_factory);

}

static void
gst_openal_sink_init (GstOpenALSink * sink)
{
  GST_DEBUG_OBJECT (sink, "initializing");

  sink->device_name = g_strdup (OPENAL_DEFAULT_DEVICE);

  sink->user_device = NULL;
  sink->user_context = NULL;
  sink->user_source = 0;

  sink->default_device = NULL;
  sink->default_context = NULL;
  sink->default_source = 0;

  sink->buffer_idx = 0;
  sink->buffer_count = 0;
  sink->buffers = NULL;
  sink->buffer_length = 0;

  sink->write_reset = AL_FALSE;
  sink->probed_caps = NULL;

  g_mutex_init (&sink->openal_lock);
}

static void
gst_openal_sink_finalize (GObject * object)
{
  GstOpenALSink *sink = GST_OPENAL_SINK (object);

  g_free (sink->device_name);
  sink->device_name = NULL;
  g_mutex_clear (&sink->openal_lock);

  G_OBJECT_CLASS (gst_openal_sink_parent_class)->finalize (object);
}

static void
gst_openal_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstOpenALSink *sink = GST_OPENAL_SINK (object);

  switch (prop_id) {
    case PROP_DEVICE:
      g_free (sink->device_name);
      sink->device_name = g_value_dup_string (value);
      if (sink->probed_caps)
        gst_caps_unref (sink->probed_caps);
      sink->probed_caps = NULL;
      break;
    case PROP_USER_DEVICE:
      if (!sink->default_device)
        sink->user_device = g_value_get_pointer (value);
      break;
    case PROP_USER_CONTEXT:
      if (!sink->default_device)
        sink->user_context = g_value_get_pointer (value);
      break;
    case PROP_USER_SOURCE:
      if (!sink->default_device)
        sink->user_source = g_value_get_uint (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_openal_sink_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstOpenALSink *sink = GST_OPENAL_SINK (object);
  const ALCchar *device_name = sink->device_name;
  ALCdevice *device = sink->default_device;
  ALCcontext *context = sink->default_context;
  ALuint source = sink->default_source;

  switch (prop_id) {
    case PROP_DEVICE_NAME:
      device_name = "";
      if (device)
        device_name = alcGetString (device, ALC_DEVICE_SPECIFIER);
      /* fall-through */
    case PROP_DEVICE:
      g_value_set_string (value, device_name);
      break;
    case PROP_USER_DEVICE:
      if (!device)
        device = sink->user_device;
      g_value_set_pointer (value, device);
      break;
    case PROP_USER_CONTEXT:
      if (!context)
        context = sink->user_context;
      g_value_set_pointer (value, context);
      break;
    case PROP_USER_SOURCE:
      if (!source)
        source = sink->user_source;
      g_value_set_uint (value, source);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstCaps *
gst_openal_helper_probe_caps (ALCcontext * context)
{
  static const struct
  {
    gint count;
    GstAudioChannelPosition positions[8];
  } chans[] = {
    {
          1, {
              GST_AUDIO_CHANNEL_POSITION_MONO}
        }, {
          2, {
                GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT,
              GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT}
        }, {
          4, {
                GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT,
                GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT,
                GST_AUDIO_CHANNEL_POSITION_REAR_LEFT,
              GST_AUDIO_CHANNEL_POSITION_REAR_RIGHT}
        }, {
          6, {
                GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT,
                GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT,
                GST_AUDIO_CHANNEL_POSITION_FRONT_CENTER,
                GST_AUDIO_CHANNEL_POSITION_LFE1,
                GST_AUDIO_CHANNEL_POSITION_REAR_LEFT,
              GST_AUDIO_CHANNEL_POSITION_REAR_RIGHT}
        }, {
          7, {
                GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT,
                GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT,
                GST_AUDIO_CHANNEL_POSITION_FRONT_CENTER,
                GST_AUDIO_CHANNEL_POSITION_LFE1,
                GST_AUDIO_CHANNEL_POSITION_REAR_CENTER,
                GST_AUDIO_CHANNEL_POSITION_SIDE_LEFT,
              GST_AUDIO_CHANNEL_POSITION_SIDE_RIGHT}
        }, {
          8, {
                GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT,
                GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT,
                GST_AUDIO_CHANNEL_POSITION_FRONT_CENTER,
                GST_AUDIO_CHANNEL_POSITION_LFE1,
                GST_AUDIO_CHANNEL_POSITION_REAR_LEFT,
                GST_AUDIO_CHANNEL_POSITION_REAR_RIGHT,
                GST_AUDIO_CHANNEL_POSITION_SIDE_LEFT,
              GST_AUDIO_CHANNEL_POSITION_SIDE_RIGHT}
        },
  };
  GstStructure *structure;
  guint64 channel_mask;
  GstCaps *caps;
  ALCcontext *old;

  old = pushContext (context);

  caps = gst_caps_new_empty ();

  if (alIsExtensionPresent ("AL_EXT_MCFORMATS")) {
    const char *fmt32[] = {
      "AL_FORMAT_MONO_FLOAT32",
      "AL_FORMAT_STEREO_FLOAT32",
      "AL_FORMAT_QUAD32",
      "AL_FORMAT_51CHN32",
      "AL_FORMAT_61CHN32",
      "AL_FORMAT_71CHN32",
      NULL
    }, *fmt16[] = {
      "AL_FORMAT_MONO16",
      "AL_FORMAT_STEREO16",
      "AL_FORMAT_QUAD16",
      "AL_FORMAT_51CHN16",
      "AL_FORMAT_61CHN16", "AL_FORMAT_71CHN16", NULL
    }, *fmt8[] = {
      "AL_FORMAT_MONO8",
      "AL_FORMAT_STEREO8",
      "AL_FORMAT_QUAD8",
      "AL_FORMAT_51CHN8", "AL_FORMAT_61CHN8", "AL_FORMAT_71CHN8", NULL
    };
    int i;

    if (alIsExtensionPresent ("AL_EXT_FLOAT32")) {
      for (i = 0; fmt32[i]; i++) {
        ALenum value = alGetEnumValue (fmt32[i]);
        if (checkALError () != AL_NO_ERROR || value == 0 || value == -1)
          continue;

        structure =
            gst_structure_new ("audio/x-raw", "format", G_TYPE_STRING,
            GST_AUDIO_NE (F32), "rate", GST_TYPE_INT_RANGE, OPENAL_MIN_RATE,
            OPENAL_MAX_RATE, "channels", G_TYPE_INT, chans[i].count, NULL);
        if (chans[i].count > 2) {
          gst_audio_channel_positions_to_mask (chans[i].positions,
              chans[i].count, FALSE, &channel_mask);
          gst_structure_set (structure, "channel-mask", GST_TYPE_BITMASK,
              channel_mask, NULL);
        }
        gst_caps_append_structure (caps, structure);
      }
    }

    for (i = 0; fmt16[i]; i++) {
      ALenum value = alGetEnumValue (fmt16[i]);
      if (checkALError () != AL_NO_ERROR || value == 0 || value == -1)
        continue;

      structure =
          gst_structure_new ("audio/x-raw", "format", G_TYPE_STRING,
          GST_AUDIO_NE (S16), "rate", GST_TYPE_INT_RANGE, OPENAL_MIN_RATE,
          OPENAL_MAX_RATE, "channels", G_TYPE_INT, chans[i].count, NULL);
      if (chans[i].count > 2) {
        gst_audio_channel_positions_to_mask (chans[i].positions, chans[i].count,
            FALSE, &channel_mask);
        gst_structure_set (structure, "channel-mask", GST_TYPE_BITMASK,
            channel_mask, NULL);
      }
      gst_caps_append_structure (caps, structure);
    }
    for (i = 0; fmt8[i]; i++) {
      ALenum value = alGetEnumValue (fmt8[i]);
      if (checkALError () != AL_NO_ERROR || value == 0 || value == -1)
        continue;

      structure =
          gst_structure_new ("audio/x-raw", "format", G_TYPE_STRING,
          G_STRINGIFY (U8), "rate", GST_TYPE_INT_RANGE, OPENAL_MIN_RATE,
          OPENAL_MAX_RATE, "channels", G_TYPE_INT, chans[i].count, NULL);
      if (chans[i].count > 2) {
        gst_audio_channel_positions_to_mask (chans[i].positions, chans[i].count,
            FALSE, &channel_mask);
        gst_structure_set (structure, "channel-mask", GST_TYPE_BITMASK,
            channel_mask, NULL);
      }
      gst_caps_append_structure (caps, structure);
    }
  } else {
    if (alIsExtensionPresent ("AL_EXT_FLOAT32")) {
      structure =
          gst_structure_new ("audio/x-raw", "format", G_TYPE_STRING,
          GST_AUDIO_NE (F32), "rate", GST_TYPE_INT_RANGE, OPENAL_MIN_RATE,
          OPENAL_MAX_RATE, "channels", GST_TYPE_INT_RANGE, 1, 2, NULL);
      gst_caps_append_structure (caps, structure);
    }

    structure =
        gst_structure_new ("audio/x-raw", "format", G_TYPE_STRING,
        GST_AUDIO_NE (S16), "rate", GST_TYPE_INT_RANGE, OPENAL_MIN_RATE,
        OPENAL_MAX_RATE, "channels", GST_TYPE_INT_RANGE, 1, 2, NULL);
    gst_caps_append_structure (caps, structure);

    structure =
        gst_structure_new ("audio/x-raw", "format", G_TYPE_STRING,
        G_STRINGIFY (U8), "rate", GST_TYPE_INT_RANGE, OPENAL_MIN_RATE,
        OPENAL_MAX_RATE, "channels", GST_TYPE_INT_RANGE, 1, 2, NULL);
    gst_caps_append_structure (caps, structure);
  }

  if (alIsExtensionPresent ("AL_EXT_double")) {
    structure =
        gst_structure_new ("audio/x-raw", "format", G_TYPE_STRING,
        GST_AUDIO_NE (F64), "rate", GST_TYPE_INT_RANGE, OPENAL_MIN_RATE,
        OPENAL_MAX_RATE, "channels", GST_TYPE_INT_RANGE, 1, 2, NULL);
    gst_caps_append_structure (caps, structure);
  }

  if (alIsExtensionPresent ("AL_EXT_IMA4")) {
    structure =
        gst_structure_new ("audio/x-adpcm", "layout", G_TYPE_STRING, "ima",
        "rate", GST_TYPE_INT_RANGE, OPENAL_MIN_RATE, OPENAL_MAX_RATE,
        "channels", GST_TYPE_INT_RANGE, 1, 2, NULL);
    gst_caps_append_structure (caps, structure);
  }

  if (alIsExtensionPresent ("AL_EXT_ALAW")) {
    structure =
        gst_structure_new ("audio/x-alaw", "rate", GST_TYPE_INT_RANGE,
        OPENAL_MIN_RATE, OPENAL_MAX_RATE, "channels", GST_TYPE_INT_RANGE, 1, 2,
        NULL);
    gst_caps_append_structure (caps, structure);
  }

  if (alIsExtensionPresent ("AL_EXT_MULAW_MCFORMATS")) {
    const char *fmtmulaw[] = {
      "AL_FORMAT_MONO_MULAW",
      "AL_FORMAT_STEREO_MULAW",
      "AL_FORMAT_QUAD_MULAW",
      "AL_FORMAT_51CHN_MULAW",
      "AL_FORMAT_61CHN_MULAW",
      "AL_FORMAT_71CHN_MULAW",
      NULL
    };
    int i;

    for (i = 0; fmtmulaw[i]; i++) {
      ALenum value = alGetEnumValue (fmtmulaw[i]);
      if (checkALError () != AL_NO_ERROR || value == 0 || value == -1)
        continue;

      structure =
          gst_structure_new ("audio/x-mulaw", "rate", GST_TYPE_INT_RANGE,
          OPENAL_MIN_RATE, OPENAL_MAX_RATE, "channels", G_TYPE_INT,
          chans[i].count, NULL);
      if (chans[i].count > 2) {
        gst_audio_channel_positions_to_mask (chans[i].positions, chans[i].count,
            FALSE, &channel_mask);
        gst_structure_set (structure, "channel-mask", GST_TYPE_BITMASK,
            channel_mask, NULL);
      }
      gst_caps_append_structure (caps, structure);
    }
  } else if (alIsExtensionPresent ("AL_EXT_MULAW")) {
    structure =
        gst_structure_new ("audio/x-mulaw", "rate", GST_TYPE_INT_RANGE,
        OPENAL_MIN_RATE, OPENAL_MAX_RATE, "channels", GST_TYPE_INT_RANGE, 1, 2,
        NULL);
    gst_caps_append_structure (caps, structure);
  }

  popContext (old, context);

  return caps;
}

static GstCaps *
gst_openal_sink_getcaps (GstBaseSink * basesink, GstCaps * filter)
{
  GstOpenALSink *sink = GST_OPENAL_SINK (basesink);
  GstCaps *caps;

  if (sink->default_device == NULL) {
    GstPad *pad = GST_BASE_SINK_PAD (basesink);
    GstCaps *tcaps = gst_pad_get_pad_template_caps (pad);
    caps = gst_caps_copy (tcaps);
    gst_caps_unref (tcaps);
  } else if (sink->probed_caps)
    caps = gst_caps_copy (sink->probed_caps);
  else {
    if (sink->default_context)
      caps = gst_openal_helper_probe_caps (sink->default_context);
    else if (sink->user_context)
      caps = gst_openal_helper_probe_caps (sink->user_context);
    else {
      ALCcontext *context = alcCreateContext (sink->default_device, NULL);
      if (context) {
        caps = gst_openal_helper_probe_caps (context);
        alcDestroyContext (context);
      } else {
        GST_ELEMENT_WARNING (sink, RESOURCE, FAILED,
            ("Could not create temporary context."),
            GST_ALC_ERROR (sink->default_device));
        caps = NULL;
      }
    }

    if (caps && !gst_caps_is_empty (caps))
      sink->probed_caps = gst_caps_copy (caps);
  }

  if (filter) {
    GstCaps *intersection;

    intersection =
        gst_caps_intersect_full (filter, caps, GST_CAPS_INTERSECT_FIRST);
    return intersection;
  } else {
    return caps;
  }
}

static gboolean
gst_openal_sink_open (GstAudioSink * audiosink)
{
  GstOpenALSink *sink = GST_OPENAL_SINK (audiosink);

  if (sink->user_device) {
    ALCint value = -1;
    alcGetIntegerv (sink->user_device, ALC_ATTRIBUTES_SIZE, 1, &value);
    if (value > 0) {
      if (!sink->user_context
          || alcGetContextsDevice (sink->user_context) == sink->user_device)
        sink->default_device = sink->user_device;
    }
  } else if (sink->user_context)
    sink->default_device = alcGetContextsDevice (sink->user_context);
  else
    sink->default_device = alcOpenDevice (sink->device_name);
  if (!sink->default_device) {
    GST_ELEMENT_ERROR (sink, RESOURCE, OPEN_WRITE,
        ("Could not open device."), GST_ALC_ERROR (sink->default_device));
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_openal_sink_close (GstAudioSink * audiosink)
{
  GstOpenALSink *sink = GST_OPENAL_SINK (audiosink);

  if (!sink->user_device && !sink->user_context) {
    if (alcCloseDevice (sink->default_device) == ALC_FALSE) {
      GST_ELEMENT_ERROR (sink, RESOURCE, CLOSE,
          ("Could not close device."), GST_ALC_ERROR (sink->default_device));
      return FALSE;
    }
  }
  sink->default_device = NULL;

  if (sink->probed_caps)
    gst_caps_unref (sink->probed_caps);
  sink->probed_caps = NULL;

  return TRUE;
}

static void
gst_openal_sink_parse_spec (GstOpenALSink * sink,
    const GstAudioRingBufferSpec * spec)
{
  ALuint format = AL_NONE;

  GST_DEBUG_OBJECT (sink,
      "looking up format for type %d, gst-format %d, and %d channels",
      spec->type, GST_AUDIO_INFO_FORMAT (&spec->info),
      GST_AUDIO_INFO_CHANNELS (&spec->info));

  /* Don't need to verify supported formats, since the probed caps will only
   * report what was detected and we shouldn't get anything different */
  switch (spec->type) {
    case GST_AUDIO_RING_BUFFER_FORMAT_TYPE_RAW:
      switch (GST_AUDIO_INFO_FORMAT (&spec->info)) {
        case GST_AUDIO_FORMAT_U8:
          switch (GST_AUDIO_INFO_CHANNELS (&spec->info)) {
            case 1:
              format = AL_FORMAT_MONO8;
              break;
            case 2:
              format = AL_FORMAT_STEREO8;
              break;
            case 4:
              format = AL_FORMAT_QUAD8;
              break;
            case 6:
              format = AL_FORMAT_51CHN8;
              break;
            case 7:
              format = AL_FORMAT_61CHN8;
              break;
            case 8:
              format = AL_FORMAT_71CHN8;
              break;
            default:
              break;
          }
          break;

        case GST_AUDIO_FORMAT_S16:
          switch (GST_AUDIO_INFO_CHANNELS (&spec->info)) {
            case 1:
              format = AL_FORMAT_MONO16;
              break;
            case 2:
              format = AL_FORMAT_STEREO16;
              break;
            case 4:
              format = AL_FORMAT_QUAD16;
              break;
            case 6:
              format = AL_FORMAT_51CHN16;
              break;
            case 7:
              format = AL_FORMAT_61CHN16;
              break;
            case 8:
              format = AL_FORMAT_71CHN16;
              break;
            default:
              break;
          }
          break;

        case GST_AUDIO_FORMAT_F32:
          switch (GST_AUDIO_INFO_CHANNELS (&spec->info)) {
            case 1:
              format = AL_FORMAT_MONO_FLOAT32;
              break;
            case 2:
              format = AL_FORMAT_STEREO_FLOAT32;
              break;
            case 4:
              format = AL_FORMAT_QUAD32;
              break;
            case 6:
              format = AL_FORMAT_51CHN32;
              break;
            case 7:
              format = AL_FORMAT_61CHN32;
              break;
            case 8:
              format = AL_FORMAT_71CHN32;
              break;
            default:
              break;
          }
          break;

        case GST_AUDIO_FORMAT_F64:
          switch (GST_AUDIO_INFO_CHANNELS (&spec->info)) {
            case 1:
              format = AL_FORMAT_MONO_DOUBLE_EXT;
              break;
            case 2:
              format = AL_FORMAT_STEREO_DOUBLE_EXT;
              break;
            default:
              break;
          }
          break;
        default:
          break;
      }
      break;

    case GST_AUDIO_RING_BUFFER_FORMAT_TYPE_IMA_ADPCM:
      switch (GST_AUDIO_INFO_CHANNELS (&spec->info)) {
        case 1:
          format = AL_FORMAT_MONO_IMA4;
          break;
        case 2:
          format = AL_FORMAT_STEREO_IMA4;
          break;
        default:
          break;
      }
      break;

    case GST_AUDIO_RING_BUFFER_FORMAT_TYPE_A_LAW:
      switch (GST_AUDIO_INFO_CHANNELS (&spec->info)) {
        case 1:
          format = AL_FORMAT_MONO_ALAW_EXT;
          break;
        case 2:
          format = AL_FORMAT_STEREO_ALAW_EXT;
          break;
        default:
          break;
      }
      break;

    case GST_AUDIO_RING_BUFFER_FORMAT_TYPE_MU_LAW:
      switch (GST_AUDIO_INFO_CHANNELS (&spec->info)) {
        case 1:
          format = AL_FORMAT_MONO_MULAW;
          break;
        case 2:
          format = AL_FORMAT_STEREO_MULAW;
          break;
        case 4:
          format = AL_FORMAT_QUAD_MULAW;
          break;
        case 6:
          format = AL_FORMAT_51CHN_MULAW;
          break;
        case 7:
          format = AL_FORMAT_61CHN_MULAW;
          break;
        case 8:
          format = AL_FORMAT_71CHN_MULAW;
          break;
        default:
          break;
      }
      break;

    default:
      break;
  }

  sink->bytes_per_sample = GST_AUDIO_INFO_BPS (&spec->info);
  sink->rate = GST_AUDIO_INFO_RATE (&spec->info);
  sink->channels = GST_AUDIO_INFO_CHANNELS (&spec->info);
  sink->format = format;
  sink->buffer_count = spec->segtotal;
  sink->buffer_length = spec->segsize;
}

static gboolean
gst_openal_sink_prepare (GstAudioSink * audiosink,
    GstAudioRingBufferSpec * spec)
{
  GstOpenALSink *sink = GST_OPENAL_SINK (audiosink);
  ALCcontext *context, *old;

  if (sink->default_context && !gst_openal_sink_unprepare (audiosink))
    return FALSE;

  if (sink->user_context)
    context = sink->user_context;
  else {
    ALCint attribs[3] = { 0, 0, 0 };

    /* Don't try to change the playback frequency of an app's device */
    if (!sink->user_device) {
      attribs[0] = ALC_FREQUENCY;
      attribs[1] = GST_AUDIO_INFO_RATE (&spec->info);
      attribs[2] = 0;
    }

    context = alcCreateContext (sink->default_device, attribs);
    if (!context) {
      GST_ELEMENT_ERROR (sink, RESOURCE, FAILED,
          ("Unable to prepare device."), GST_ALC_ERROR (sink->default_device));
      return FALSE;
    }
  }

  old = pushContext (context);

  if (sink->user_source) {
    if (!sink->user_context || !alIsSource (sink->user_source)) {
      GST_ELEMENT_ERROR (sink, RESOURCE, NOT_FOUND, (NULL),
          ("Invalid source specified for context"));
      goto fail;
    }
    sink->default_source = sink->user_source;
  } else {
    ALuint source;

    alGenSources (1, &source);
    if (checkALError () != AL_NO_ERROR) {
      GST_ELEMENT_ERROR (sink, RESOURCE, NO_SPACE_LEFT, (NULL),
          ("Unable to generate source"));
      goto fail;
    }
    sink->default_source = source;
  }

  gst_openal_sink_parse_spec (sink, spec);
  if (sink->format == AL_NONE) {
    GST_ELEMENT_ERROR (sink, RESOURCE, SETTINGS, (NULL),
        ("Unable to get type %d, format %d, and %d channels", spec->type,
            GST_AUDIO_INFO_FORMAT (&spec->info),
            GST_AUDIO_INFO_CHANNELS (&spec->info)));
    goto fail;
  }

  sink->buffers = g_malloc (sink->buffer_count * sizeof (*sink->buffers));
  if (!sink->buffers) {
    GST_ELEMENT_ERROR (sink, RESOURCE, FAILED, ("Out of memory."),
        ("Unable to allocate buffers"));
    goto fail;
  }

  alGenBuffers (sink->buffer_count, sink->buffers);
  if (checkALError () != AL_NO_ERROR) {
    GST_ELEMENT_ERROR (sink, RESOURCE, NO_SPACE_LEFT, (NULL),
        ("Unable to generate %d buffers", sink->buffer_count));
    goto fail;
  }
  sink->buffer_idx = 0;

  popContext (old, context);
  sink->default_context = context;
  return TRUE;

fail:
  if (!sink->user_source && sink->default_source)
    alDeleteSources (1, &sink->default_source);
  sink->default_source = 0;

  g_free (sink->buffers);
  sink->buffers = NULL;
  sink->buffer_count = 0;
  sink->buffer_length = 0;

  popContext (old, context);
  if (!sink->user_context)
    alcDestroyContext (context);
  return FALSE;
}

static gboolean
gst_openal_sink_unprepare (GstAudioSink * audiosink)
{
  GstOpenALSink *sink = GST_OPENAL_SINK (audiosink);
  ALCcontext *old;

  if (!sink->default_context)
    return TRUE;

  old = pushContext (sink->default_context);

  alSourceStop (sink->default_source);
  alSourcei (sink->default_source, AL_BUFFER, 0);

  if (!sink->user_source)
    alDeleteSources (1, &sink->default_source);
  sink->default_source = 0;

  alDeleteBuffers (sink->buffer_count, sink->buffers);
  g_free (sink->buffers);
  sink->buffers = NULL;
  sink->buffer_idx = 0;
  sink->buffer_count = 0;
  sink->buffer_length = 0;

  checkALError ();
  popContext (old, sink->default_context);
  if (!sink->user_context)
    alcDestroyContext (sink->default_context);
  sink->default_context = NULL;

  return TRUE;
}

static gint
gst_openal_sink_write (GstAudioSink * audiosink, gpointer data, guint length)
{
  GstOpenALSink *sink = GST_OPENAL_SINK (audiosink);
  ALint processed, queued, state;
  ALCcontext *old;
  gulong rest_us;

  g_assert (length == sink->buffer_length);

  old = pushContext (sink->default_context);

  rest_us =
      (guint64) (sink->buffer_length / sink->bytes_per_sample) *
      G_USEC_PER_SEC / sink->rate / sink->channels;
  do {
    alGetSourcei (sink->default_source, AL_SOURCE_STATE, &state);
    alGetSourcei (sink->default_source, AL_BUFFERS_QUEUED, &queued);
    alGetSourcei (sink->default_source, AL_BUFFERS_PROCESSED, &processed);
    if (checkALError () != AL_NO_ERROR) {
      GST_ELEMENT_ERROR (sink, RESOURCE, WRITE, (NULL),
          ("Source state error detected"));
      length = 0;
      goto out_nolock;
    }

    if (processed > 0 || queued < sink->buffer_count)
      break;
    if (state != AL_PLAYING)
      alSourcePlay (sink->default_source);
    g_usleep (rest_us);
  }
  while (1);

  GST_OPENAL_SINK_LOCK (sink);
  if (sink->write_reset != AL_FALSE) {
    sink->write_reset = AL_FALSE;
    length = 0;
    goto out;
  }

  queued -= processed;
  while (processed-- > 0) {
    ALuint bid;
    alSourceUnqueueBuffers (sink->default_source, 1, &bid);
  }
  if (state == AL_STOPPED) {
    /* "Restore" from underruns (not actually needed, but it keeps delay
     * calculations correct while rebuffering) */
    alSourceRewind (sink->default_source);
  }

  alBufferData (sink->buffers[sink->buffer_idx], sink->format,
      data, sink->buffer_length, sink->rate);
  alSourceQueueBuffers (sink->default_source, 1,
      &sink->buffers[sink->buffer_idx]);
  sink->buffer_idx = (sink->buffer_idx + 1) % sink->buffer_count;
  queued++;

  if (state != AL_PLAYING && queued == sink->buffer_count)
    alSourcePlay (sink->default_source);

  if (checkALError () != AL_NO_ERROR) {
    GST_ELEMENT_ERROR (sink, RESOURCE, WRITE, (NULL),
        ("Source queue error detected"));
    goto out;
  }

out:
  GST_OPENAL_SINK_UNLOCK (sink);
out_nolock:
  popContext (old, sink->default_context);
  return length;
}

static guint
gst_openal_sink_delay (GstAudioSink * audiosink)
{
  GstOpenALSink *sink = GST_OPENAL_SINK (audiosink);
  ALint queued, state, offset, delay;
  ALCcontext *old;

  if (!sink->default_context)
    return 0;

  GST_OPENAL_SINK_LOCK (sink);
  old = pushContext (sink->default_context);

  delay = 0;
  alGetSourcei (sink->default_source, AL_BUFFERS_QUEUED, &queued);
  /* Order here is important. If the offset is queried after the state and an
   * underrun occurs in between the two calls, it can end up with a 0 offset
   * in a playing state, incorrectly reporting a len*queued/bps delay. */
  alGetSourcei (sink->default_source, AL_BYTE_OFFSET, &offset);
  alGetSourcei (sink->default_source, AL_SOURCE_STATE, &state);

  /* Note: state=stopped is an underrun, meaning all buffers are processed
   * and there's no delay when writing the next buffer. Pre-buffering is
   * state=initial, which will introduce a delay while writing. */
  if (checkALError () == AL_NO_ERROR && state != AL_STOPPED)
    delay =
        ((queued * sink->buffer_length) -
        offset) / sink->bytes_per_sample / sink->channels / GST_MSECOND;

  popContext (old, sink->default_context);
  GST_OPENAL_SINK_UNLOCK (sink);

  if (G_UNLIKELY (delay < 0)) {
    /* make sure we never return a negative delay */
    GST_WARNING_OBJECT (openal_debug, "negative delay");
    delay = 0;
  }

  return delay;
}

static void
gst_openal_sink_reset (GstAudioSink * audiosink)
{
  GstOpenALSink *sink = GST_OPENAL_SINK (audiosink);
  ALCcontext *old;

  GST_OPENAL_SINK_LOCK (sink);
  old = pushContext (sink->default_context);

  sink->write_reset = AL_TRUE;
  alSourceStop (sink->default_source);
  alSourceRewind (sink->default_source);
  alSourcei (sink->default_source, AL_BUFFER, 0);
  checkALError ();

  popContext (old, sink->default_context);
  GST_OPENAL_SINK_UNLOCK (sink);
}
