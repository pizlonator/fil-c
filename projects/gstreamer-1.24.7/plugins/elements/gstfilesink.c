/* GStreamer
 * Copyright (C) 1999,2000 Erik Walthinsen <omega@cse.ogi.edu>
 *                    2000 Wim Taymans <wtay@chello.be>
 *                    2006 Wim Taymans <wim@fluendo.com>
 *
 * gstfilesink.c:
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
 * SECTION:element-filesink
 * @title: filesink
 * @see_also: #GstFileSrc
 *
 * Write incoming data to a file in the local file system.
 *
 * ## Example launch line
 * |[
 * gst-launch-1.0 v4l2src num-buffers=1 ! jpegenc ! filesink location=capture1.jpeg
 * ]| Capture one frame from a v4l2 camera and save as jpeg image.
 *
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib/gi18n-lib.h>

#include <gst/gst.h>
#include <glib/gstdio.h>
#include <stdio.h>              /* for fseeko() */
#ifdef HAVE_STDIO_EXT_H
#include <stdio_ext.h>          /* for __fbufsize, for debugging */
#endif
#include <errno.h>
#include "gstfilesink.h"
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>

#ifdef G_OS_WIN32
#include <io.h>                 /* lseek, open, close, read */
#undef lseek
#define lseek _lseeki64
#undef off_t
#define off_t guint64
#undef ftruncate
#define ftruncate _chsize
#undef fsync
#define fsync _commit
#ifdef _MSC_VER                 /* Check if we are using MSVC, fileno is deprecated in favour */
#define fileno _fileno          /* of _fileno */
#endif
#endif

#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "gstelements_private.h"
#include "gstfilesink.h"
#include "gstcoreelementselements.h"

static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

#define GST_TYPE_FILE_SINK_FILE_MODE (gst_file_sink_file_mode_get_type())
static GType
gst_file_sink_file_mode_get_type (void)
{
  static GType file_mode_type = 0;

  if (g_once_init_enter_pointer (&file_mode_type)) {
    static const GEnumValue file_mode[] = {
      {GST_FILE_SINK_FILE_MODE_TRUNC, "Truncate file (mode wb)", "truncate"},
      {GST_FILE_SINK_FILE_MODE_APPEND, "Append file (mode ab)", "output"},
      {GST_FILE_SINK_FILE_MODE_OVERWRITE,
          "Overwrite file without truncating (mode rb+)", "overwrite"},
      {0, NULL, NULL}
    };

    GType new_file_mode_type =
        g_enum_register_static ("GstFileSinkFileMode", file_mode);

    g_once_init_leave_pointer (&file_mode_type, new_file_mode_type);
  }
  return file_mode_type;
}

#define GST_TYPE_FILE_SINK_BUFFER_MODE (gst_file_sink_buffer_mode_get_type ())
static GType
gst_file_sink_buffer_mode_get_type (void)
{
  static GType buffer_mode_type = 0;
  static const GEnumValue buffer_mode[] = {
    {GST_FILE_SINK_BUFFER_MODE_DEFAULT, "Default buffering", "default"},
    {GST_FILE_SINK_BUFFER_MODE_FULL, "Fully buffered", "full"},
    {GST_FILE_SINK_BUFFER_MODE_LINE, "Line buffered (deprecated, like full)",
        "line"},
    {GST_FILE_SINK_BUFFER_MODE_UNBUFFERED, "Unbuffered", "unbuffered"},
    {0, NULL, NULL},
  };

  if (!buffer_mode_type) {
    buffer_mode_type =
        g_enum_register_static ("GstFileSinkBufferMode", buffer_mode);
  }
  return buffer_mode_type;
}

GST_DEBUG_CATEGORY_STATIC (gst_file_sink_debug);
#define GST_CAT_DEFAULT gst_file_sink_debug

#define DEFAULT_LOCATION 	NULL
#define DEFAULT_BUFFER_MODE 	GST_FILE_SINK_BUFFER_MODE_DEFAULT
#define DEFAULT_BUFFER_SIZE 	64 * 1024
#define DEFAULT_APPEND		FALSE
#define DEFAULT_O_SYNC		FALSE
#define DEFAULT_MAX_TRANSIENT_ERROR_TIMEOUT	0
#define DEFAULT_FILE_MODE      GST_FILE_SINK_FILE_MODE_TRUNC

enum
{
  PROP_0,
  PROP_LOCATION,
  PROP_BUFFER_MODE,
  PROP_BUFFER_SIZE,
  PROP_APPEND,
  PROP_O_SYNC,
  PROP_MAX_TRANSIENT_ERROR_TIMEOUT,
  PROP_FILE_MODE,
  PROP_LAST
};

/* Copy of glib's g_fopen due to win32 libc/cross-DLL brokenness: we can't
 * use the 'file pointer' opened in glib (and returned from this function)
 * in this library, as they may have unrelated C runtimes. */
static FILE *
gst_fopen (const gchar * filename, const gchar * mode, gboolean o_sync)
{
  FILE *retval;
#ifdef G_OS_WIN32
  retval = g_fopen (filename, mode);
  return retval;
#else
  int fd;
  int flags = O_CREAT | O_WRONLY;

  /* NOTE: below code is for handing spurious EACCES return on write
   * See https://gitlab.freedesktop.org/gstreamer/gstreamer/-/merge_requests/143
   */
  if (strcmp (mode, "wb") == 0)
    flags |= O_TRUNC;
  else if (strcmp (mode, "ab") == 0)
    flags |= O_APPEND;
  else if (strcmp (mode, "rb+") == 0)
    flags |= O_RDWR;
  else
    g_assert_not_reached ();

  if (o_sync)
    flags |= O_SYNC;

  fd = open (filename, flags, 0666);

  if (fd < 0)
    return NULL;

  retval = fdopen (fd, mode);
  return retval;
#endif
}

static void gst_file_sink_dispose (GObject * object);

static void gst_file_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_file_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_file_sink_open_file (GstFileSink * sink);
static void gst_file_sink_close_file (GstFileSink * sink);

static gboolean gst_file_sink_start (GstBaseSink * sink);
static gboolean gst_file_sink_stop (GstBaseSink * sink);
static gboolean gst_file_sink_event (GstBaseSink * sink, GstEvent * event);
static GstFlowReturn gst_file_sink_render (GstBaseSink * sink,
    GstBuffer * buffer);
static GstFlowReturn gst_file_sink_render_list (GstBaseSink * sink,
    GstBufferList * list);
static gboolean gst_file_sink_unlock (GstBaseSink * sink);
static gboolean gst_file_sink_unlock_stop (GstBaseSink * sink);

static gboolean gst_file_sink_do_seek (GstFileSink * filesink,
    guint64 new_offset);
static gboolean gst_file_sink_get_current_offset (GstFileSink * filesink,
    guint64 * p_pos);

static gboolean gst_file_sink_query (GstBaseSink * bsink, GstQuery * query);

static void gst_file_sink_uri_handler_init (gpointer g_iface,
    gpointer iface_data);

static GstFlowReturn gst_file_sink_flush_buffer (GstFileSink * filesink);

#define _do_init \
  G_IMPLEMENT_INTERFACE (GST_TYPE_URI_HANDLER, gst_file_sink_uri_handler_init); \
  GST_DEBUG_CATEGORY_INIT (gst_file_sink_debug, "filesink", 0, "filesink element");
#define gst_file_sink_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstFileSink, gst_file_sink, GST_TYPE_BASE_SINK,
    _do_init);
GST_ELEMENT_REGISTER_DEFINE (filesink, "filesink", GST_RANK_PRIMARY,
    GST_TYPE_FILE_SINK);

static void
gst_file_sink_class_init (GstFileSinkClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GstBaseSinkClass *gstbasesink_class = GST_BASE_SINK_CLASS (klass);

  gobject_class->dispose = gst_file_sink_dispose;

  gobject_class->set_property = gst_file_sink_set_property;
  gobject_class->get_property = gst_file_sink_get_property;

  g_object_class_install_property (gobject_class, PROP_LOCATION,
      g_param_spec_string ("location", "File Location",
          "Location of the file to write", NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_BUFFER_MODE,
      g_param_spec_enum ("buffer-mode", "Buffering mode",
          "The buffering mode to use", GST_TYPE_FILE_SINK_BUFFER_MODE,
          DEFAULT_BUFFER_MODE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_BUFFER_SIZE,
      g_param_spec_uint ("buffer-size", "Buffering size",
          "Size of buffer in number of bytes for line or full buffer-mode", 0,
          G_MAXUINT, DEFAULT_BUFFER_SIZE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstFileSink:file-mode
   *
   * Ability to specify file mode.
   *
   * Since: 1.24
   */
  g_object_class_install_property (gobject_class, PROP_FILE_MODE,
      g_param_spec_enum ("file-mode", "File Mode",
          "Specify file mode used to open file", GST_TYPE_FILE_SINK_FILE_MODE,
          DEFAULT_FILE_MODE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstFileSink:append
   *
   * Append to an already existing file.
   */
  g_object_class_install_property (gobject_class, PROP_APPEND,
      g_param_spec_boolean ("append", "Append",
          "Append to an already existing file", DEFAULT_APPEND,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_DEPRECATED));

  g_object_class_install_property (gobject_class, PROP_O_SYNC,
      g_param_spec_boolean ("o-sync", "Synchronous IO",
          "Open the file with O_SYNC for enabling synchronous IO",
          DEFAULT_O_SYNC, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_MAX_TRANSIENT_ERROR_TIMEOUT,
      g_param_spec_int ("max-transient-error-timeout",
          "Max Transient Error Timeout",
          "Retry up to this many ms on transient errors (currently EACCES)", 0,
          G_MAXINT, DEFAULT_MAX_TRANSIENT_ERROR_TIMEOUT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_set_static_metadata (gstelement_class,
      "File Sink",
      "Sink/File", "Write stream to a file",
      "Thomas Vander Stichele <thomas at apestaart dot org>");
  gst_element_class_add_static_pad_template (gstelement_class, &sinktemplate);

  gstbasesink_class->start = GST_DEBUG_FUNCPTR (gst_file_sink_start);
  gstbasesink_class->stop = GST_DEBUG_FUNCPTR (gst_file_sink_stop);
  gstbasesink_class->query = GST_DEBUG_FUNCPTR (gst_file_sink_query);
  gstbasesink_class->render = GST_DEBUG_FUNCPTR (gst_file_sink_render);
  gstbasesink_class->render_list =
      GST_DEBUG_FUNCPTR (gst_file_sink_render_list);
  gstbasesink_class->event = GST_DEBUG_FUNCPTR (gst_file_sink_event);
  gstbasesink_class->unlock = GST_DEBUG_FUNCPTR (gst_file_sink_unlock);
  gstbasesink_class->unlock_stop =
      GST_DEBUG_FUNCPTR (gst_file_sink_unlock_stop);

  if (sizeof (off_t) < 8) {
    GST_LOG ("No large file support, sizeof (off_t) = %" G_GSIZE_FORMAT "!",
        sizeof (off_t));
  }

  gst_type_mark_as_plugin_api (GST_TYPE_FILE_SINK_BUFFER_MODE, 0);
  gst_type_mark_as_plugin_api (GST_TYPE_FILE_SINK_FILE_MODE, 0);
}

static void
gst_file_sink_init (GstFileSink * filesink)
{
  filesink->filename = NULL;
  filesink->file = NULL;
  filesink->current_pos = 0;
  filesink->buffer_mode = DEFAULT_BUFFER_MODE;
  filesink->buffer_size = DEFAULT_BUFFER_SIZE;
  filesink->append = FALSE;
  filesink->file_mode = DEFAULT_FILE_MODE;

  gst_base_sink_set_sync (GST_BASE_SINK (filesink), FALSE);
}

static void
gst_file_sink_dispose (GObject * object)
{
  GstFileSink *sink = GST_FILE_SINK (object);

  G_OBJECT_CLASS (parent_class)->dispose (object);

  g_free (sink->uri);
  sink->uri = NULL;
  g_free (sink->filename);
  sink->filename = NULL;
}

static gboolean
gst_file_sink_set_location (GstFileSink * sink, const gchar * location,
    GError ** error)
{
  if (sink->file)
    goto was_open;

  g_free (sink->filename);
  g_free (sink->uri);
  if (location != NULL) {
    /* we store the filename as we received it from the application. On Windows
     * this should be in UTF8 */
    sink->filename = g_strdup (location);
    sink->uri = gst_filename_to_uri (location, NULL);
    GST_INFO_OBJECT (sink, "filename : %s", sink->filename);
    GST_INFO_OBJECT (sink, "uri      : %s", sink->uri);
  } else {
    sink->filename = NULL;
    sink->uri = NULL;
  }

  return TRUE;

  /* ERRORS */
was_open:
  {
    g_warning ("Changing the `location' property on filesink when a file is "
        "open is not supported.");
    g_set_error (error, GST_URI_ERROR, GST_URI_ERROR_BAD_STATE,
        "Changing the 'location' property on filesink when a file is "
        "open is not supported");
    return FALSE;
  }
}

static void
gst_file_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstFileSink *sink = GST_FILE_SINK (object);

  switch (prop_id) {
    case PROP_LOCATION:
      gst_file_sink_set_location (sink, g_value_get_string (value), NULL);
      break;
    case PROP_BUFFER_MODE:
      sink->buffer_mode = g_value_get_enum (value);
      break;
    case PROP_BUFFER_SIZE:
      sink->buffer_size = g_value_get_uint (value);
      break;
    case PROP_APPEND:
      sink->append = g_value_get_boolean (value);
      break;
    case PROP_FILE_MODE:
      sink->file_mode = g_value_get_enum (value);
      break;
    case PROP_O_SYNC:
      sink->o_sync = g_value_get_boolean (value);
      break;
    case PROP_MAX_TRANSIENT_ERROR_TIMEOUT:
      sink->max_transient_error_timeout = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_file_sink_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstFileSink *sink = GST_FILE_SINK (object);

  switch (prop_id) {
    case PROP_LOCATION:
      g_value_set_string (value, sink->filename);
      break;
    case PROP_BUFFER_MODE:
      g_value_set_enum (value, sink->buffer_mode);
      break;
    case PROP_BUFFER_SIZE:
      g_value_set_uint (value, sink->buffer_size);
      break;
    case PROP_APPEND:
      g_value_set_boolean (value, sink->append);
      break;
    case PROP_FILE_MODE:
      g_value_set_enum (value, sink->file_mode);
      break;
    case PROP_O_SYNC:
      g_value_set_boolean (value, sink->o_sync);
      break;
    case PROP_MAX_TRANSIENT_ERROR_TIMEOUT:
      g_value_set_int (value, sink->max_transient_error_timeout);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_file_sink_open_file (GstFileSink * sink)
{
  /* open the file */
  if (sink->filename == NULL || sink->filename[0] == '\0')
    goto no_filename;

  if (sink->append || sink->file_mode == GST_FILE_SINK_FILE_MODE_APPEND)
    sink->file = gst_fopen (sink->filename, "ab", sink->o_sync);
  else if (sink->file_mode == GST_FILE_SINK_FILE_MODE_OVERWRITE)
    sink->file = gst_fopen (sink->filename, "rb+", sink->o_sync);
  else
    sink->file = gst_fopen (sink->filename, "wb", sink->o_sync);
  if (sink->file == NULL)
    goto open_failed;

  sink->current_pos = 0;
  /* try to seek in the file to figure out if it is seekable */
  sink->seekable = gst_file_sink_do_seek (sink, 0);

  if (sink->buffer)
    g_free (sink->buffer);
  sink->buffer = NULL;
  if (sink->buffer_list)
    gst_buffer_list_unref (sink->buffer_list);
  sink->buffer_list = NULL;

  if (sink->buffer_mode != GST_FILE_SINK_BUFFER_MODE_UNBUFFERED) {
    if (sink->buffer_size == 0) {
      sink->buffer_size = DEFAULT_BUFFER_SIZE;
      g_object_notify (G_OBJECT (sink), "buffer-size");
    }

    if (sink->buffer_mode == GST_FILE_SINK_BUFFER_MODE_FULL) {
      sink->buffer = g_malloc (sink->buffer_size);
      sink->allocated_buffer_size = sink->buffer_size;
    } else {
      sink->buffer_list = gst_buffer_list_new ();
    }
    sink->current_buffer_size = 0;
  }

  GST_DEBUG_OBJECT (sink, "opened file %s, seekable %d",
      sink->filename, sink->seekable);

  return TRUE;

  /* ERRORS */
no_filename:
  {
    GST_ELEMENT_ERROR (sink, RESOURCE, NOT_FOUND,
        (_("No file name specified for writing.")), (NULL));
    return FALSE;
  }
open_failed:
  {
    GST_ELEMENT_ERROR (sink, RESOURCE, OPEN_WRITE,
        (_("Could not open file \"%s\" for writing."), sink->filename),
        GST_ERROR_SYSTEM);
    return FALSE;
  }
}

static void
gst_file_sink_close_file (GstFileSink * sink)
{
  if (sink->file) {
    if (gst_file_sink_flush_buffer (sink) != GST_FLOW_OK)
      GST_ELEMENT_ERROR (sink, RESOURCE, CLOSE,
          (_("Error closing file \"%s\"."), sink->filename), NULL);

    if (fclose (sink->file) != 0)
      GST_ELEMENT_ERROR (sink, RESOURCE, CLOSE,
          (_("Error closing file \"%s\"."), sink->filename), GST_ERROR_SYSTEM);

    GST_DEBUG_OBJECT (sink, "closed file");
    sink->file = NULL;
  }

  if (sink->buffer) {
    g_free (sink->buffer);
    sink->buffer = NULL;
  }
  sink->allocated_buffer_size = 0;

  if (sink->buffer_list) {
    gst_buffer_list_unref (sink->buffer_list);
    sink->buffer_list = NULL;
  }
  sink->current_buffer_size = 0;
}

static gboolean
gst_file_sink_query (GstBaseSink * bsink, GstQuery * query)
{
  gboolean res;
  GstFileSink *self;
  GstFormat format;

  self = GST_FILE_SINK (bsink);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_POSITION:
      gst_query_parse_position (query, &format, NULL);

      switch (format) {
        case GST_FORMAT_DEFAULT:
        case GST_FORMAT_BYTES:
          gst_query_set_position (query, GST_FORMAT_BYTES,
              self->current_pos + self->current_buffer_size);
          res = TRUE;
          break;
        default:
          res = FALSE;
          break;
      }
      break;

    case GST_QUERY_FORMATS:
      gst_query_set_formats (query, 2, GST_FORMAT_DEFAULT, GST_FORMAT_BYTES);
      res = TRUE;
      break;

    case GST_QUERY_URI:
      gst_query_set_uri (query, self->uri);
      res = TRUE;
      break;

    case GST_QUERY_SEEKING:
      gst_query_parse_seeking (query, &format, NULL, NULL, NULL);
      if (format == GST_FORMAT_BYTES || format == GST_FORMAT_DEFAULT) {
        gst_query_set_seeking (query, GST_FORMAT_BYTES, self->seekable, 0, -1);
      } else {
        gst_query_set_seeking (query, format, FALSE, 0, -1);
      }
      res = TRUE;
      break;

    default:
      res = GST_BASE_SINK_CLASS (parent_class)->query (bsink, query);
      break;
  }
  return res;
}

#ifdef HAVE_FSEEKO
# define __GST_STDIO_SEEK_FUNCTION "fseeko"
#elif defined (G_OS_UNIX) || defined (G_OS_WIN32)
# define __GST_STDIO_SEEK_FUNCTION "lseek"
#else
# define __GST_STDIO_SEEK_FUNCTION "fseek"
#endif

static gboolean
gst_file_sink_do_seek (GstFileSink * filesink, guint64 new_offset)
{
  GST_DEBUG_OBJECT (filesink, "Seeking to offset %" G_GUINT64_FORMAT
      " using " __GST_STDIO_SEEK_FUNCTION, new_offset);

  if (gst_file_sink_flush_buffer (filesink) != GST_FLOW_OK)
    goto flush_buffer_failed;

#ifdef HAVE_FSEEKO
  if (fseeko (filesink->file, (off_t) new_offset, SEEK_SET) != 0)
    goto seek_failed;
#elif defined (G_OS_UNIX) || defined (G_OS_WIN32)
  if (lseek (fileno (filesink->file), (off_t) new_offset,
          SEEK_SET) == (off_t) - 1)
    goto seek_failed;
#else
  if (fseek (filesink->file, (long) new_offset, SEEK_SET) != 0)
    goto seek_failed;
#endif

  /* adjust position reporting after seek;
   * presumably this should basically yield new_offset */
  gst_file_sink_get_current_offset (filesink, &filesink->current_pos);

  return TRUE;

  /* ERRORS */
flush_buffer_failed:
  {
    GST_DEBUG_OBJECT (filesink, "Flushing buffer failed");
    return FALSE;
  }
seek_failed:
  {
    GST_DEBUG_OBJECT (filesink, "Seeking failed: %s", g_strerror (errno));
    return FALSE;
  }
}

/* handle events (search) */
static gboolean
gst_file_sink_event (GstBaseSink * sink, GstEvent * event)
{
  GstEventType type;
  GstFileSink *filesink;

  filesink = GST_FILE_SINK (sink);

  type = GST_EVENT_TYPE (event);

  switch (type) {
    case GST_EVENT_SEGMENT:
    {
      const GstSegment *segment;

      gst_event_parse_segment (event, &segment);

      if (segment->format == GST_FORMAT_BYTES) {
        /* only try to seek and fail when we are going to a different
         * position */
        if (filesink->current_pos + filesink->current_buffer_size !=
            segment->start) {
          /* FIXME, the seek should be performed on the pos field, start/stop are
           * just boundaries for valid bytes offsets. We should also fill the file
           * with zeroes if the new position extends the current EOF (sparse streams
           * and segment accumulation). */
          if (!gst_file_sink_do_seek (filesink, (guint64) segment->start))
            goto seek_failed;
        } else {
          GST_DEBUG_OBJECT (filesink, "Ignored SEGMENT, no seek needed");
        }
      } else {
        GST_DEBUG_OBJECT (filesink,
            "Ignored SEGMENT event of format %u (%s)", (guint) segment->format,
            gst_format_get_name (segment->format));
      }
      break;
    }
    case GST_EVENT_FLUSH_STOP:
      if (filesink->current_pos != 0 && filesink->seekable) {
        gst_file_sink_do_seek (filesink, 0);
        if (ftruncate (fileno (filesink->file), 0))
          goto truncate_failed;
      }
      if (filesink->buffer_list) {
        gst_buffer_list_unref (filesink->buffer_list);
        filesink->buffer_list = gst_buffer_list_new ();
      }
      filesink->current_buffer_size = 0;
      break;
    case GST_EVENT_EOS:
      if (gst_file_sink_flush_buffer (filesink) != GST_FLOW_OK)
        goto flush_buffer_failed;
      break;
    default:
      break;
  }

  return GST_BASE_SINK_CLASS (parent_class)->event (sink, event);

  /* ERRORS */
seek_failed:
  {
    GST_ELEMENT_ERROR (filesink, RESOURCE, SEEK,
        (_("Error while seeking in file \"%s\"."), filesink->filename),
        GST_ERROR_SYSTEM);
    gst_event_unref (event);
    return FALSE;
  }
flush_buffer_failed:
  {
    GST_ELEMENT_ERROR (filesink, RESOURCE, WRITE,
        (_("Error while writing to file \"%s\"."), filesink->filename), NULL);
    gst_event_unref (event);
    return FALSE;
  }
truncate_failed:
  {
    GST_ELEMENT_ERROR (filesink, RESOURCE, WRITE,
        (_("Error while writing to file \"%s\"."), filesink->filename),
        GST_ERROR_SYSTEM);
    gst_event_unref (event);
    return FALSE;
  }
}

static gboolean
gst_file_sink_get_current_offset (GstFileSink * filesink, guint64 * p_pos)
{
  off_t ret = -1;

  /* no need to flush internal buffer here as this is only called right
   * after a seek. If this changes then the buffer should be flushed here
   * too
   */

#ifdef HAVE_FTELLO
  ret = ftello (filesink->file);
#elif defined (G_OS_UNIX) || defined (G_OS_WIN32)
  ret = lseek (fileno (filesink->file), 0, SEEK_CUR);
#else
  ret = (off_t) ftell (filesink->file);
#endif

  if (ret != (off_t) - 1)
    *p_pos = (guint64) ret;

  return (ret != (off_t) - 1);
}

static GstFlowReturn
gst_file_sink_render_list_internal (GstFileSink * sink,
    GstBufferList * buffer_list)
{
  GstFlowReturn flow;
  guint num_buffers;
  guint64 skip = 0;

  num_buffers = gst_buffer_list_length (buffer_list);
  if (num_buffers == 0)
    goto no_data;

  GST_DEBUG_OBJECT (sink,
      "writing %u buffers at position %" G_GUINT64_FORMAT, num_buffers,
      sink->current_pos);

  for (;;) {
    guint64 bytes_written = 0;

    flow =
        gst_writev_buffer_list (GST_OBJECT_CAST (sink), fileno (sink->file),
        NULL, buffer_list, &bytes_written, skip,
        sink->max_transient_error_timeout, sink->current_pos, &sink->flushing);

    sink->current_pos += bytes_written;
    skip += bytes_written;

    if (flow != GST_FLOW_FLUSHING)
      break;

    flow = gst_base_sink_wait_preroll (GST_BASE_SINK (sink));

    if (flow != GST_FLOW_OK)
      return flow;
  }

  return flow;

no_data:
  {
    GST_LOG_OBJECT (sink, "empty buffer list");
    return GST_FLOW_OK;
  }
}

static GstFlowReturn
gst_file_sink_flush_buffer (GstFileSink * filesink)
{
  GstFlowReturn flow_ret = GST_FLOW_OK;

  GST_DEBUG_OBJECT (filesink, "Flushing out buffer of size %" G_GSIZE_FORMAT,
      filesink->current_buffer_size);

  if (filesink->buffer && filesink->current_buffer_size) {
    guint64 skip = 0;

    for (;;) {
      guint64 bytes_written = 0;

      flow_ret =
          gst_writev_mem (GST_OBJECT_CAST (filesink), fileno (filesink->file),
          NULL, filesink->buffer, filesink->current_buffer_size, &bytes_written,
          skip, filesink->max_transient_error_timeout, filesink->current_pos,
          &filesink->flushing);

      filesink->current_pos += bytes_written;
      skip += bytes_written;

      if (flow_ret != GST_FLOW_FLUSHING)
        break;

      flow_ret = gst_base_sink_wait_preroll (GST_BASE_SINK (filesink));
      if (flow_ret != GST_FLOW_OK)
        break;
    }
  } else if (filesink->buffer_list && filesink->current_buffer_size) {
    guint length;

    length = gst_buffer_list_length (filesink->buffer_list);

    if (length > 0) {
      flow_ret =
          gst_file_sink_render_list_internal (filesink, filesink->buffer_list);
      /* Remove all buffers from the list but keep the list. This ensures that
       * we don't re-allocate the array storing the buffers all the time */
      gst_buffer_list_remove (filesink->buffer_list, 0, length);
    }
  }

  filesink->current_buffer_size = 0;

  return flow_ret;
}

static gboolean
has_sync_after_buffer (GstBuffer ** buffer, guint idx, gpointer user_data)
{
  if (GST_BUFFER_FLAG_IS_SET (*buffer, GST_BUFFER_FLAG_SYNC_AFTER)) {
    gboolean *sync_after = user_data;

    *sync_after = TRUE;
    return FALSE;
  }

  return TRUE;
}

static gboolean
accumulate_size (GstBuffer ** buffer, guint idx, gpointer user_data)
{
  guint *size = user_data;

  *size += gst_buffer_get_size (*buffer);

  return TRUE;
}

static GstFlowReturn
render_buffer (GstFileSink * filesink, GstBuffer * buffer)
{
  GstFlowReturn flow;
  guint64 bytes_written = 0;
  guint64 skip = 0;

  for (;;) {
    flow =
        gst_writev_buffer (GST_OBJECT_CAST (filesink),
        fileno (filesink->file), NULL, buffer, &bytes_written, skip,
        filesink->max_transient_error_timeout, filesink->current_pos,
        &filesink->flushing);

    filesink->current_pos += bytes_written;
    skip += bytes_written;

    if (flow != GST_FLOW_FLUSHING)
      break;

    flow = gst_base_sink_wait_preroll (GST_BASE_SINK (filesink));

    if (flow != GST_FLOW_OK)
      break;
  }

  return flow;
}

static GstFlowReturn
gst_file_sink_render_list (GstBaseSink * bsink, GstBufferList * buffer_list)
{
  GstFlowReturn flow;
  GstFileSink *sink;
  guint i, num_buffers;
  gboolean sync_after = FALSE;
  gint fsync_ret;

  sink = GST_FILE_SINK_CAST (bsink);

  num_buffers = gst_buffer_list_length (buffer_list);
  if (num_buffers == 0)
    goto no_data;

  gst_buffer_list_foreach (buffer_list, has_sync_after_buffer, &sync_after);

  if (sync_after || (!sink->buffer && !sink->buffer_list)) {
    flow = gst_file_sink_flush_buffer (sink);
    if (flow == GST_FLOW_OK)
      flow = gst_file_sink_render_list_internal (sink, buffer_list);
  } else {
    guint size = 0;
    gst_buffer_list_foreach (buffer_list, accumulate_size, &size);

    GST_DEBUG_OBJECT (sink,
        "Queueing buffer list of %u bytes (%u buffers) at offset %"
        G_GUINT64_FORMAT, size, num_buffers,
        sink->current_pos + sink->current_buffer_size);

    if (sink->buffer) {
      flow = GST_FLOW_OK;
      for (i = 0; i < num_buffers && flow == GST_FLOW_OK; i++) {
        GstBuffer *buffer = gst_buffer_list_get (buffer_list, i);
        gsize buffer_size = gst_buffer_get_size (buffer);

        if (sink->current_buffer_size + buffer_size >
            sink->allocated_buffer_size) {
          flow = gst_file_sink_flush_buffer (sink);
          if (flow != GST_FLOW_OK)
            return flow;
        }

        if (buffer_size > sink->allocated_buffer_size) {
          GST_DEBUG_OBJECT (sink,
              "writing buffer ( %" G_GSIZE_FORMAT
              " bytes) at position %" G_GUINT64_FORMAT,
              buffer_size, sink->current_pos);

          flow = render_buffer (sink, buffer);
        } else {
          sink->current_buffer_size +=
              gst_buffer_extract (buffer, 0,
              sink->buffer + sink->current_buffer_size, buffer_size);
          flow = GST_FLOW_OK;
        }
      }
    } else {
      for (i = 0; i < num_buffers; ++i)
        gst_buffer_list_add (sink->buffer_list,
            gst_buffer_ref (gst_buffer_list_get (buffer_list, i)));
      sink->current_buffer_size += size;

      if (sink->current_buffer_size > sink->buffer_size)
        flow = gst_file_sink_flush_buffer (sink);
      else
        flow = GST_FLOW_OK;
    }
  }

  if (flow == GST_FLOW_OK && sync_after) {
    do {
      fsync_ret = fsync (fileno (sink->file));
    } while (fsync_ret < 0 && errno == EINTR);
    if (fsync_ret) {
      GST_ELEMENT_ERROR (sink, RESOURCE, WRITE,
          (_("Error while writing to file \"%s\"."), sink->filename),
          ("%s", g_strerror (errno)));
      flow = GST_FLOW_ERROR;
    }
  }

  return flow;

no_data:
  {
    GST_LOG_OBJECT (sink, "empty buffer list");
    return GST_FLOW_OK;
  }
}

static GstFlowReturn
gst_file_sink_render (GstBaseSink * sink, GstBuffer * buffer)
{
  GstFileSink *filesink;
  GstFlowReturn flow;
  guint8 n_mem;
  gboolean sync_after;
  gint fsync_ret;

  filesink = GST_FILE_SINK_CAST (sink);

  sync_after = GST_BUFFER_FLAG_IS_SET (buffer, GST_BUFFER_FLAG_SYNC_AFTER);

  n_mem = gst_buffer_n_memory (buffer);

  if (n_mem > 0 && (sync_after || (!filesink->buffer
              && !filesink->buffer_list))) {
    flow = gst_file_sink_flush_buffer (filesink);
    if (flow == GST_FLOW_OK) {
      flow = render_buffer (filesink, buffer);
    }
  } else if (n_mem > 0) {
    gsize size = gst_buffer_get_size (buffer);

    GST_DEBUG_OBJECT (filesink,
        "Queueing buffer of %" G_GSIZE_FORMAT " bytes at offset %"
        G_GUINT64_FORMAT, size,
        filesink->current_pos + filesink->current_buffer_size);

    if (filesink->buffer) {
      if (filesink->current_buffer_size + size >
          filesink->allocated_buffer_size) {
        flow = gst_file_sink_flush_buffer (filesink);
        if (flow != GST_FLOW_OK)
          return flow;
      }

      if (size > filesink->allocated_buffer_size) {
        GST_DEBUG_OBJECT (sink,
            "writing buffer ( %" G_GSIZE_FORMAT
            " bytes) at position %" G_GUINT64_FORMAT,
            size, filesink->current_pos);

        flow = render_buffer (filesink, buffer);
      } else {
        filesink->current_buffer_size +=
            gst_buffer_extract (buffer, 0,
            filesink->buffer + filesink->current_buffer_size, size);
        flow = GST_FLOW_OK;
      }
    } else {
      filesink->current_buffer_size += gst_buffer_get_size (buffer);
      gst_buffer_list_add (filesink->buffer_list, gst_buffer_ref (buffer));

      if (filesink->current_buffer_size > filesink->buffer_size)
        flow = gst_file_sink_flush_buffer (filesink);
      else
        flow = GST_FLOW_OK;
    }
  } else {
    flow = GST_FLOW_OK;
  }

  if (flow == GST_FLOW_OK && sync_after) {
    do {
      fsync_ret = fsync (fileno (filesink->file));
    } while (fsync_ret < 0 && errno == EINTR);
    if (fsync_ret) {
      GST_ELEMENT_ERROR (filesink, RESOURCE, WRITE,
          (_("Error while writing to file \"%s\"."), filesink->filename),
          ("%s", g_strerror (errno)));
      flow = GST_FLOW_ERROR;
    }
  }

  return flow;
}

static gboolean
gst_file_sink_start (GstBaseSink * basesink)
{
  GstFileSink *filesink;

  filesink = GST_FILE_SINK_CAST (basesink);

  g_atomic_int_set (&filesink->flushing, FALSE);
  return gst_file_sink_open_file (filesink);
}

static gboolean
gst_file_sink_stop (GstBaseSink * basesink)
{
  GstFileSink *filesink;

  filesink = GST_FILE_SINK_CAST (basesink);

  gst_file_sink_close_file (filesink);
  return TRUE;
}

static gboolean
gst_file_sink_unlock (GstBaseSink * basesink)
{
  GstFileSink *filesink;

  filesink = GST_FILE_SINK_CAST (basesink);
  g_atomic_int_set (&filesink->flushing, TRUE);

  return TRUE;
}

static gboolean
gst_file_sink_unlock_stop (GstBaseSink * basesink)
{
  GstFileSink *filesink;

  filesink = GST_FILE_SINK_CAST (basesink);
  g_atomic_int_set (&filesink->flushing, FALSE);

  return TRUE;
}

/*** GSTURIHANDLER INTERFACE *************************************************/

static GstURIType
gst_file_sink_uri_get_type (GType type)
{
  return GST_URI_SINK;
}

static const gchar *const *
gst_file_sink_uri_get_protocols (GType type)
{
  static const gchar *protocols[] = { "file", NULL };

  return protocols;
}

static gchar *
gst_file_sink_uri_get_uri (GstURIHandler * handler)
{
  GstFileSink *sink = GST_FILE_SINK (handler);

  /* FIXME: make thread-safe */
  return g_strdup (sink->uri);
}

static gboolean
gst_file_sink_uri_set_uri (GstURIHandler * handler, const gchar * uri,
    GError ** error)
{
  gchar *location;
  gboolean ret;
  GstFileSink *sink = GST_FILE_SINK (handler);

  /* allow file://localhost/foo/bar by stripping localhost but fail
   * for every other hostname */
  if (g_str_has_prefix (uri, "file://localhost/")) {
    char *tmp;

    /* 16 == strlen ("file://localhost") */
    tmp = g_strconcat ("file://", uri + 16, NULL);
    /* we use gst_uri_get_location() although we already have the
     * "location" with uri + 16 because it provides unescaping */
    location = gst_uri_get_location (tmp);
    g_free (tmp);
  } else if (strcmp (uri, "file://") == 0) {
    /* Special case for "file://" as this is used by some applications
     *  to test with gst_element_make_from_uri if there's an element
     *  that supports the URI protocol. */
    gst_file_sink_set_location (sink, NULL, NULL);
    return TRUE;
  } else {
    location = gst_uri_get_location (uri);
  }

  if (!location) {
    g_set_error_literal (error, GST_URI_ERROR, GST_URI_ERROR_BAD_URI,
        "File URI without location");
    return FALSE;
  }

  if (!g_path_is_absolute (location)) {
    g_set_error_literal (error, GST_URI_ERROR, GST_URI_ERROR_BAD_URI,
        "File URI location must be an absolute path");
    g_free (location);
    return FALSE;
  }

  ret = gst_file_sink_set_location (sink, location, error);
  g_free (location);

  return ret;
}

static void
gst_file_sink_uri_handler_init (gpointer g_iface, gpointer iface_data)
{
  GstURIHandlerInterface *iface = (GstURIHandlerInterface *) g_iface;

  iface->get_type = gst_file_sink_uri_get_type;
  iface->get_protocols = gst_file_sink_uri_get_protocols;
  iface->get_uri = gst_file_sink_uri_get_uri;
  iface->set_uri = gst_file_sink_uri_set_uri;
}
