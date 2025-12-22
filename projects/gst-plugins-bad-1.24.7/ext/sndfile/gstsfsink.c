/* GStreamer libsndfile plugin
 * Copyright (C) 2007 Andy Wingo <wingo at pobox dot com>
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


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/audio/audio.h>

#include <glib/gi18n-lib.h>

#include "gstsfsink.h"

enum
{
  PROP_0,
  PROP_LOCATION,
  PROP_MAJOR_TYPE,
  PROP_MINOR_TYPE,
  PROP_BUFFER_FRAMES
};

#define DEFAULT_BUFFER_FRAMES (256)

static GstStaticPadTemplate sf_sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw-float, "
        "rate = (int) [ 1, MAX ], "
        "channels = (int) [ 1, MAX ], "
        "endianness = (int) BYTE_ORDER, "
        "width = (int) 32; "
        "audio/x-raw-int, "
        "rate = (int) [ 1, MAX ], "
        "channels = (int) [ 1, MAX ], "
        "endianness = (int) BYTE_ORDER, "
        "width = (int) {16, 32}, "
        "depth = (int) {16, 32}, " "signed = (boolean) true")
    );

GST_BOILERPLATE (GstSFSink, gst_sf_sink, GstBaseSink, GST_TYPE_BASE_SINK);

static void gst_sf_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_sf_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_sf_sink_start (GstBaseSink * bsink);
static gboolean gst_sf_sink_stop (GstBaseSink * bsink);
static void gst_sf_sink_fixate (GstBaseSink * bsink, GstCaps * caps);
static gboolean gst_sf_sink_set_caps (GstBaseSink * bsink, GstCaps * caps);
static gboolean gst_sf_sink_activate_pull (GstBaseSink * bsink,
    gboolean active);
static GstFlowReturn gst_sf_sink_render (GstBaseSink * bsink,
    GstBuffer * buffer);
static gboolean gst_sf_sink_event (GstBaseSink * bsink, GstEvent * event);

static gboolean gst_sf_sink_open_file (GstSFSink * this);
static void gst_sf_sink_close_file (GstSFSink * this);

GST_DEBUG_CATEGORY_STATIC (gst_sf_debug);
#define GST_CAT_DEFAULT gst_sf_debug

static void
gst_sf_sink_base_init (gpointer g_class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);

  GST_DEBUG_CATEGORY_INIT (gst_sf_debug, "sfsink", 0, "sfsink element");
  gst_element_class_add_static_pad_template (element_class, &sf_sink_factory);
  gst_element_class_set_static_metadata (element_class, "Sndfile sink",
      "Sink/Audio",
      "Write audio streams to disk using libsndfile",
      "Andy Wingo <wingo at pobox dot com>");
}

static void
gst_sf_sink_class_init (GstSFSinkClass * klass)
{
  GObjectClass *gobject_class;
  GstBaseSinkClass *basesink_class;
  GParamSpec *pspec;

  gobject_class = (GObjectClass *) klass;
  basesink_class = (GstBaseSinkClass *) klass;

  gobject_class->set_property = gst_sf_sink_set_property;
  gobject_class->get_property = gst_sf_sink_get_property;

  g_object_class_install_property (gobject_class, PROP_LOCATION,
      g_param_spec_string ("location", "File Location",
          "Location of the file to write", NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  pspec = g_param_spec_enum
      ("major-type", "Major type", "Major output type", GST_TYPE_SF_MAJOR_TYPES,
      SF_FORMAT_WAV,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (gobject_class, PROP_MAJOR_TYPE, pspec);
  pspec = g_param_spec_enum
      ("minor-type", "Minor type", "Minor output type", GST_TYPE_SF_MINOR_TYPES,
      SF_FORMAT_FLOAT,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (gobject_class, PROP_MINOR_TYPE, pspec);
  pspec = g_param_spec_int
      ("buffer-frames", "Buffer frames",
      "Number of frames per buffer, in pull mode", 1, G_MAXINT,
      DEFAULT_BUFFER_FRAMES,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (gobject_class, PROP_BUFFER_FRAMES, pspec);

  basesink_class->get_times = NULL;
  basesink_class->start = GST_DEBUG_FUNCPTR (gst_sf_sink_start);
  basesink_class->stop = GST_DEBUG_FUNCPTR (gst_sf_sink_stop);
  basesink_class->fixate = GST_DEBUG_FUNCPTR (gst_sf_sink_fixate);
  basesink_class->set_caps = GST_DEBUG_FUNCPTR (gst_sf_sink_set_caps);
  basesink_class->activate_pull = GST_DEBUG_FUNCPTR (gst_sf_sink_activate_pull);
  basesink_class->render = GST_DEBUG_FUNCPTR (gst_sf_sink_render);
  basesink_class->event = GST_DEBUG_FUNCPTR (gst_sf_sink_event);
}

static void
gst_sf_sink_init (GstSFSink * this, GstSFSinkClass * klass)
{
  GST_BASE_SINK (this)->can_activate_pull = TRUE;
}

static void
gst_sf_sink_set_location (GstSFSink * this, const gchar * location)
{
  if (this->file)
    goto was_open;

  g_free (this->location);

  this->location = location ? g_strdup (location) : NULL;

  return;

was_open:
  {
    g_warning ("Changing the `location' property on sfsink when "
        "a file is open not supported.");
    return;
  }
}


static void
gst_sf_sink_set_property (GObject * object, guint prop_id, const GValue * value,
    GParamSpec * pspec)
{
  GstSFSink *this = GST_SF_SINK (object);

  switch (prop_id) {
    case PROP_LOCATION:
      gst_sf_sink_set_location (this, g_value_get_string (value));
      break;

    case PROP_MAJOR_TYPE:
      this->format_major = g_value_get_enum (value);
      break;

    case PROP_MINOR_TYPE:
      this->format_subtype = g_value_get_enum (value);
      break;

    case PROP_BUFFER_FRAMES:
      this->buffer_frames = g_value_get_int (value);
      break;

    default:
      break;
  }
}

static void
gst_sf_sink_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstSFSink *this = GST_SF_SINK (object);

  switch (prop_id) {
    case PROP_LOCATION:
      g_value_set_string (value, this->location);
      break;

    case PROP_MAJOR_TYPE:
      g_value_set_enum (value, this->format_major);
      break;

    case PROP_MINOR_TYPE:
      g_value_set_enum (value, this->format_subtype);
      break;

    case PROP_BUFFER_FRAMES:
      g_value_set_int (value, this->buffer_frames);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_sf_sink_start (GstBaseSink * bsink)
{
  /* pass */
  return TRUE;
}

static gboolean
gst_sf_sink_stop (GstBaseSink * bsink)
{
  GstSFSink *this = GST_SF_SINK (bsink);

  if (this->file)
    gst_sf_sink_close_file (this);

  return TRUE;
}

static gboolean
gst_sf_sink_open_file (GstSFSink * this)
{
  int mode;
  SF_INFO info;

  g_return_val_if_fail (this->file == NULL, FALSE);
  g_return_val_if_fail (this->rate > 0, FALSE);
  g_return_val_if_fail (this->channels > 0, FALSE);

  if (!this->location)
    goto no_filename;

  mode = SFM_WRITE;
  this->format = this->format_major | this->format_subtype;
  info.samplerate = this->rate;
  info.channels = this->channels;
  info.format = this->format;

  GST_INFO_OBJECT (this, "Opening %s with rate %d, %d channels, format 0x%x",
      this->location, info.samplerate, info.channels, info.format);

  if (!sf_format_check (&info))
    goto bad_format;

  this->file = sf_open (this->location, mode, &info);

  if (!this->file)
    goto open_failed;

  return TRUE;

no_filename:
  {
    GST_ELEMENT_ERROR (this, RESOURCE, NOT_FOUND,
        (_("No file name specified for writing.")), (NULL));
    return FALSE;
  }
bad_format:
  {
    GST_ELEMENT_ERROR (this, STREAM, ENCODE, (NULL),
        ("Input parameters (rate:%d, channels:%d, format:0x%x) invalid",
            info.samplerate, info.channels, info.format));
    return FALSE;
  }
open_failed:
  {
    GST_ELEMENT_ERROR (this, RESOURCE, OPEN_WRITE,
        (_("Could not open file \"%s\" for writing."), this->location),
        ("soundfile error: %s", sf_strerror (NULL)));
    return FALSE;
  }
}

static void
gst_sf_sink_close_file (GstSFSink * this)
{
  int err = 0;

  g_return_if_fail (this->file != NULL);

  GST_INFO_OBJECT (this, "Closing file %s", this->location);

  if ((err = sf_close (this->file)))
    goto close_failed;

  this->file = NULL;

  return;

close_failed:
  {
    GST_ELEMENT_ERROR (this, RESOURCE, CLOSE,
        ("Could not close file file \"%s\".", this->location),
        ("soundfile error: %s", sf_error_number (err)));
    return;
  }
}

static void
gst_sf_sink_fixate (GstBaseSink * bsink, GstCaps * caps)
{
  GstStructure *s;
  gint width, depth;

  s = gst_caps_get_structure (caps, 0);

  /* fields for all formats */
  gst_structure_fixate_field_nearest_int (s, "rate", 44100);
  gst_structure_fixate_field_nearest_int (s, "channels", 2);
  gst_structure_fixate_field_nearest_int (s, "width", 16);

  /* fields for int */
  if (gst_structure_has_field (s, "depth")) {
    gst_structure_get_int (s, "width", &width);
    /* round width to nearest multiple of 8 for the depth */
    depth = GST_ROUND_UP_8 (width);
    gst_structure_fixate_field_nearest_int (s, "depth", depth);
  }
  if (gst_structure_has_field (s, "signed"))
    gst_structure_fixate_field_boolean (s, "signed", TRUE);
  if (gst_structure_has_field (s, "endianness"))
    gst_structure_fixate_field_nearest_int (s, "endianness", G_BYTE_ORDER);
}

static gboolean
gst_sf_sink_set_caps (GstBaseSink * bsink, GstCaps * caps)
{
  GstSFSink *this = (GstSFSink *) bsink;
  GstStructure *structure;
  gint width, channels, rate;

  structure = gst_caps_get_structure (caps, 0);

  if (!gst_structure_get_int (structure, "width", &width)
      || !gst_structure_get_int (structure, "channels", &channels)
      || !gst_structure_get_int (structure, "rate", &rate))
    goto impossible;

  if (gst_structure_has_name (structure, "audio/x-raw-int")) {
    switch (width) {
      case 16:
        this->writer = (GstSFWriter) sf_writef_short;
        break;
      case 32:
        this->writer = (GstSFWriter) sf_writef_int;
        break;
      default:
        goto impossible;
    }
  } else {
    switch (width) {
      case 32:
        this->writer = (GstSFWriter) sf_writef_float;
        break;
      default:
        goto impossible;
    }
  }

  this->bytes_per_frame = width * channels / 8;
  this->rate = rate;
  this->channels = channels;

  return gst_sf_sink_open_file (this);

impossible:
  {
    g_warning ("something impossible happened");
    return FALSE;
  }
}

/* with STREAM_LOCK
 */
static void
gst_sf_sink_loop (GstPad * pad)
{
  GstSFSink *this;
  GstBaseSink *basesink;
  GstBuffer *buf = NULL;
  GstFlowReturn result;

  this = GST_SF_SINK (gst_pad_get_parent (pad));
  basesink = GST_BASE_SINK (this);

  result = gst_pad_pull_range (pad, basesink->offset,
      this->buffer_frames * this->bytes_per_frame, &buf);
  if (G_UNLIKELY (result != GST_FLOW_OK))
    goto paused;

  if (G_UNLIKELY (buf == NULL))
    goto no_buffer;

  basesink->offset += GST_BUFFER_SIZE (buf);

  GST_BASE_SINK_PREROLL_LOCK (basesink);
  result = gst_sf_sink_render (basesink, buf);
  GST_BASE_SINK_PREROLL_UNLOCK (basesink);
  if (G_UNLIKELY (result != GST_FLOW_OK))
    goto paused;

  gst_object_unref (this);

  return;

  /* ERRORS */
paused:
  {
    GST_INFO_OBJECT (basesink, "pausing task, reason %s",
        gst_flow_get_name (result));
    gst_pad_pause_task (pad);
    /* fatal errors and NOT_LINKED cause EOS */
    if (result == GST_FLOW_UNEXPECTED) {
      gst_pad_send_event (pad, gst_event_new_eos ());
    } else if (result < GST_FLOW_UNEXPECTED || result == GST_FLOW_NOT_LINKED) {
      GST_ELEMENT_FLOW_ERROR (basesink, result);
      gst_pad_send_event (pad, gst_event_new_eos ());
    }
    gst_object_unref (this);
    return;
  }
no_buffer:
  {
    GST_INFO_OBJECT (this, "no buffer, pausing");
    result = GST_FLOW_ERROR;
    goto paused;
  }
}

static gboolean
gst_sf_sink_activate_pull (GstBaseSink * basesink, gboolean active)
{
  gboolean result;

  if (active) {
    /* start task */
    result = gst_pad_start_task (basesink->sinkpad,
        (GstTaskFunction) gst_sf_sink_loop, basesink->sinkpad, NULL);
  } else {
    /* step 2, make sure streaming finishes */
    result = gst_pad_stop_task (basesink->sinkpad);
  }

  return result;
}

static GstFlowReturn
gst_sf_sink_render (GstBaseSink * bsink, GstBuffer * buffer)
{
  GstSFSink *this;
  sf_count_t written, num_to_write;

  this = (GstSFSink *) bsink;

  if (GST_BUFFER_SIZE (buffer) % this->bytes_per_frame)
    goto bad_length;

  num_to_write = GST_BUFFER_SIZE (buffer) / this->bytes_per_frame;

  written = this->writer (this->file, GST_BUFFER_DATA (buffer), num_to_write);
  if (written != num_to_write)
    goto short_write;

  return GST_FLOW_OK;

bad_length:
  {
    GST_ELEMENT_ERROR (this, RESOURCE, WRITE,
        (_("Could not write to file \"%s\"."), this->location),
        ("bad buffer size: %u %% %d != 0", GST_BUFFER_SIZE (buffer),
            this->bytes_per_frame));
    return GST_FLOW_ERROR;
  }
short_write:
  {
    GST_ELEMENT_ERROR (this, RESOURCE, WRITE,
        (_("Could not write to file \"%s\"."), this->location),
        ("soundfile error: %s", sf_strerror (this->file)));
    return GST_FLOW_ERROR;
  }
}

static gboolean
gst_sf_sink_event (GstBaseSink * bsink, GstEvent * event)
{
  GstSFSink *this;
  GstEventType type;

  this = (GstSFSink *) bsink;

  type = GST_EVENT_TYPE (event);

  switch (type) {
    case GST_EVENT_EOS:
      if (this->file)
        sf_write_sync (this->file);
      break;
    default:
      break;
  }

  return TRUE;
}
