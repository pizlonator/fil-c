/* GStreamer
 *
 * Copyright (C) 2007 Rene Stadler <mail@renestadler.de>
 * Copyright (C) 2007-2009 Sebastian Dröge <sebastian.droege@collabora.co.uk>
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
 * SECTION:element-giosink
 * @title: giosink
 * @see_also: #GstFileSink, #GstGnomeVFSSink, #GstGioSrc
 *
 * This plugin writes incoming data to a local or remote location specified
 * by an URI. This location can be specified using any protocol supported by
 * the GIO library or it's VFS backends. Common protocols are 'file', 'ftp',
 * or 'smb'.
 *
 * If the URI or #GFile already exists giosink will post a message of
 * type %GST_MESSAGE_ELEMENT with name "file-exists" on the bus. The message
 * also contains the #GFile and the corresponding URI.
 * Applications can use the "file-exists" message to notify the user about
 * the problem and to set a different target location or to remove the
 * existing file. Note that right after the "file-exists" message a normal
 * error message is posted on the bus which should be ignored if "file-exists"
 * is handled by the application, for example by calling
 * gst_bus_set_flushing(bus, TRUE) after the "file-exists" message was
 * received and gst_bus_set_flushing(bus, FALSE) after the problem is
 * resolved.
 *
 * Similar to the "file-exist" message a "not-mounted" message is posted
 * on the bus if the target location is not mounted yet and needs to be
 * mounted. This message can be used by application to mount the location
 * and retry after the location was mounted successfully.
 *
 * ## Example pipelines
 * |[
 * gst-launch-1.0 -v filesrc location=input.xyz ! giosink location=file:///home/joe/out.xyz
 * ]|
 *  The above pipeline will simply copy a local file. Instead of giosink,
 * we could just as well have used the filesink element here.
 * |[
 * gst-launch-1.0 -v uridecodebin uri=file:///path/to/audio.file ! audioconvert ! flacenc ! giosink location=smb://othercomputer/foo.flac
 * ]|
 *  The above pipeline will re-encode an audio file into FLAC format and store
 * it on a remote host using the Samba protocol.
 * |[
 * gst-launch-1.0 -v audiotestsrc num-buffers=100 ! vorbisenc ! oggmux ! giosink location=file:///home/foo/bar.ogg
 * ]|
 *  The above pipeline will encode a 440Hz sine wave to Ogg Vorbis and stores
 * it in the home directory of user foo.
 *
 */

/* FIXME: We would like to mount the enclosing volume of an URL
 *        if it isn't mounted yet but this is possible async-only.
 *        Unfortunately this requires a running main loop from the
 *        default context and we can't guarantee this!
 *
 *        We would also like to do authentication while mounting.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gstgiosink.h"
#include "gstgioelements.h"

GST_DEBUG_CATEGORY_STATIC (gst_gio_sink_debug);
#define GST_CAT_DEFAULT gst_gio_sink_debug

/* Filter signals and args */
enum
{
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_LOCATION,
  PROP_FILE
};

#define gst_gio_sink_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstGioSink, gst_gio_sink, GST_TYPE_GIO_BASE_SINK,
    gst_gio_uri_handler_do_init (g_define_type_id));
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (giosink, "giosink",
    GST_RANK_SECONDARY, GST_TYPE_GIO_SINK, gio_element_init (plugin));


static void gst_gio_sink_finalize (GObject * object);
static void gst_gio_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_gio_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static GOutputStream *gst_gio_sink_get_stream (GstGioBaseSink * base_sink);

static void
gst_gio_sink_class_init (GstGioSinkClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *gstelement_class = (GstElementClass *) klass;
  GstGioBaseSinkClass *gstgiobasesink_class = (GstGioBaseSinkClass *) klass;

  GST_DEBUG_CATEGORY_INIT (gst_gio_sink_debug, "gio_sink", 0, "GIO sink");

  gobject_class->finalize = gst_gio_sink_finalize;
  gobject_class->set_property = gst_gio_sink_set_property;
  gobject_class->get_property = gst_gio_sink_get_property;

  g_object_class_install_property (gobject_class, PROP_LOCATION,
      g_param_spec_string ("location", "Location", "URI location to write to",
          NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstGioSink:file:
   *
   * %GFile to write to.
   */
  g_object_class_install_property (gobject_class, PROP_FILE,
      g_param_spec_object ("file", "File", "GFile to write to",
          G_TYPE_FILE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_set_static_metadata (gstelement_class, "GIO sink",
      "Sink/File",
      "Write to any GIO-supported location",
      "Ren\xc3\xa9 Stadler <mail@renestadler.de>, "
      "Sebastian Dröge <sebastian.droege@collabora.co.uk>");

  gstgiobasesink_class->get_stream =
      GST_DEBUG_FUNCPTR (gst_gio_sink_get_stream);
}

static void
gst_gio_sink_init (GstGioSink * sink)
{
  GstGioBaseSink *bsink = GST_GIO_BASE_SINK (sink);

  // TRUE is the historical default for this element
  bsink->close_on_stop = TRUE;
}

static void
gst_gio_sink_finalize (GObject * object)
{
  GstGioSink *sink = GST_GIO_SINK (object);

  if (sink->file) {
    g_object_unref (sink->file);
    sink->file = NULL;
  }

  GST_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static void
gst_gio_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstGioSink *sink = GST_GIO_SINK (object);

  switch (prop_id) {
    case PROP_LOCATION:{
      const gchar *uri = NULL;

      if (GST_STATE (sink) == GST_STATE_PLAYING ||
          GST_STATE (sink) == GST_STATE_PAUSED) {
        GST_WARNING
            ("Setting a new location or GFile not supported in PLAYING or PAUSED state");
        break;
      }

      GST_OBJECT_LOCK (GST_OBJECT (sink));
      if (sink->file)
        g_object_unref (sink->file);

      uri = g_value_get_string (value);

      if (uri) {
        sink->file = g_file_new_for_uri (uri);

        if (!sink->file) {
          GST_ERROR ("Could not create GFile for URI '%s'", uri);
        }
      } else {
        sink->file = NULL;
      }
      GST_OBJECT_UNLOCK (GST_OBJECT (sink));
      break;
    }
    case PROP_FILE:
      if (GST_STATE (sink) == GST_STATE_PLAYING ||
          GST_STATE (sink) == GST_STATE_PAUSED) {
        GST_WARNING
            ("Setting a new location or GFile not supported in PLAYING or PAUSED state");
        break;
      }

      GST_OBJECT_LOCK (GST_OBJECT (sink));
      if (sink->file)
        g_object_unref (sink->file);

      sink->file = g_value_dup_object (value);

      GST_OBJECT_UNLOCK (GST_OBJECT (sink));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_gio_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstGioSink *sink = GST_GIO_SINK (object);

  switch (prop_id) {
    case PROP_LOCATION:{
      gchar *uri;

      GST_OBJECT_LOCK (GST_OBJECT (sink));
      if (sink->file) {
        uri = g_file_get_uri (sink->file);
        g_value_set_string (value, uri);
        g_free (uri);
      } else {
        g_value_set_string (value, NULL);
      }
      GST_OBJECT_UNLOCK (GST_OBJECT (sink));
      break;
    }
    case PROP_FILE:
      GST_OBJECT_LOCK (GST_OBJECT (sink));
      g_value_set_object (value, sink->file);
      GST_OBJECT_UNLOCK (GST_OBJECT (sink));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GOutputStream *
gst_gio_sink_get_stream (GstGioBaseSink * bsink)
{
  GstGioSink *sink = GST_GIO_SINK (bsink);
  GOutputStream *stream;
  GCancellable *cancel = GST_GIO_BASE_SINK (sink)->cancel;
  GError *err = NULL;
  gchar *uri;

  if (sink->file == NULL) {
    GST_ELEMENT_ERROR (sink, RESOURCE, OPEN_WRITE, (NULL),
        ("No location or GFile given"));
    return NULL;
  }

  uri = g_file_get_uri (sink->file);
  if (!uri)
    uri = g_strdup ("(null)");

  stream =
      G_OUTPUT_STREAM (g_file_create (sink->file, G_FILE_CREATE_NONE, cancel,
          &err));

  if (!stream) {
    if (!gst_gio_error (sink, "g_file_create", &err, NULL)) {
      /*if (GST_GIO_ERROR_MATCHES (err, EXISTS)) */
      /* FIXME: Retry with replace if overwrite == TRUE! */

      if (GST_GIO_ERROR_MATCHES (err, NOT_FOUND)) {
        GST_ELEMENT_ERROR (sink, RESOURCE, NOT_FOUND, (NULL),
            ("Could not open location %s for writing: %s", uri, err->message));
      } else if (GST_GIO_ERROR_MATCHES (err, EXISTS)) {
        gst_element_post_message (GST_ELEMENT_CAST (sink),
            gst_message_new_element (GST_OBJECT_CAST (sink),
                gst_structure_new ("file-exists", "file", G_TYPE_FILE,
                    sink->file, "uri", G_TYPE_STRING, uri, NULL)));

        GST_ELEMENT_ERROR (sink, RESOURCE, OPEN_WRITE, (NULL),
            ("Location %s already exists: %s", uri, err->message));
      } else if (GST_GIO_ERROR_MATCHES (err, NOT_MOUNTED)) {
        gst_element_post_message (GST_ELEMENT_CAST (sink),
            gst_message_new_element (GST_OBJECT_CAST (sink),
                gst_structure_new ("not-mounted", "file", G_TYPE_FILE,
                    sink->file, "uri", G_TYPE_STRING, uri, NULL)));

        GST_ELEMENT_ERROR (sink, RESOURCE, OPEN_WRITE, (NULL),
            ("Location %s not mounted: %s", uri, err->message));
      } else {
        GST_ELEMENT_ERROR (sink, RESOURCE, OPEN_WRITE, (NULL),
            ("Could not open location %s for writing: %s", uri, err->message));
      }

      g_clear_error (&err);
    }
    g_free (uri);
    return NULL;
  }

  GST_DEBUG_OBJECT (sink, "opened location %s", uri);

  g_free (uri);

  return stream;
}
