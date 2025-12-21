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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gstgiobasesrc.h"
#include "gstgioelements.h"

GST_DEBUG_CATEGORY_STATIC (gst_gio_base_src_debug);
#define GST_CAT_DEFAULT gst_gio_base_src_debug

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

#define gst_gio_base_src_parent_class parent_class
G_DEFINE_TYPE (GstGioBaseSrc, gst_gio_base_src, GST_TYPE_BASE_SRC);

static void gst_gio_base_src_finalize (GObject * object);

static gboolean gst_gio_base_src_start (GstBaseSrc * base_src);
static gboolean gst_gio_base_src_stop (GstBaseSrc * base_src);
static gboolean gst_gio_base_src_get_size (GstBaseSrc * base_src,
    guint64 * size);
static gboolean gst_gio_base_src_is_seekable (GstBaseSrc * base_src);
static gboolean gst_gio_base_src_unlock (GstBaseSrc * base_src);
static gboolean gst_gio_base_src_unlock_stop (GstBaseSrc * base_src);
static GstFlowReturn gst_gio_base_src_create (GstBaseSrc * base_src,
    guint64 offset, guint size, GstBuffer ** buf);
static gboolean gst_gio_base_src_query (GstBaseSrc * base_src,
    GstQuery * query);

static void
gst_gio_base_src_class_init (GstGioBaseSrcClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *gstelement_class = (GstElementClass *) klass;
  GstBaseSrcClass *gstbasesrc_class = (GstBaseSrcClass *) klass;

  GST_DEBUG_CATEGORY_INIT (gst_gio_base_src_debug, "gio_base_src", 0,
      "GIO base source");

  gobject_class->finalize = gst_gio_base_src_finalize;

  gst_element_class_add_static_pad_template (gstelement_class, &src_factory);

  gstbasesrc_class->start = GST_DEBUG_FUNCPTR (gst_gio_base_src_start);
  gstbasesrc_class->stop = GST_DEBUG_FUNCPTR (gst_gio_base_src_stop);
  gstbasesrc_class->get_size = GST_DEBUG_FUNCPTR (gst_gio_base_src_get_size);
  gstbasesrc_class->is_seekable =
      GST_DEBUG_FUNCPTR (gst_gio_base_src_is_seekable);
  gstbasesrc_class->unlock = GST_DEBUG_FUNCPTR (gst_gio_base_src_unlock);
  gstbasesrc_class->unlock_stop =
      GST_DEBUG_FUNCPTR (gst_gio_base_src_unlock_stop);
  gstbasesrc_class->create = GST_DEBUG_FUNCPTR (gst_gio_base_src_create);
  gstbasesrc_class->query = GST_DEBUG_FUNCPTR (gst_gio_base_src_query);

  gst_type_mark_as_plugin_api (GST_TYPE_GIO_BASE_SRC, 0);
}

static void
gst_gio_base_src_init (GstGioBaseSrc * src)
{
  src->cancel = g_cancellable_new ();
}

static void
gst_gio_base_src_finalize (GObject * object)
{
  GstGioBaseSrc *src = GST_GIO_BASE_SRC (object);

  if (src->cancel) {
    g_object_unref (src->cancel);
    src->cancel = NULL;
  }

  if (src->stream) {
    g_object_unref (src->stream);
    src->stream = NULL;
  }

  if (src->cache) {
    gst_buffer_unref (src->cache);
    src->cache = NULL;
  }

  GST_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static gboolean
gst_gio_base_src_start (GstBaseSrc * base_src)
{
  GstGioBaseSrc *src = GST_GIO_BASE_SRC (base_src);
  GstGioBaseSrcClass *gbsrc_class = GST_GIO_BASE_SRC_GET_CLASS (src);

  src->position = 0;

  /* FIXME: This will likely block */
  src->stream = gbsrc_class->get_stream (src);
  if (G_UNLIKELY (!G_IS_INPUT_STREAM (src->stream))) {
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ, (NULL),
        ("No input stream provided by subclass"));
    return FALSE;
  } else if (G_UNLIKELY (g_input_stream_is_closed (src->stream))) {
    GST_ELEMENT_ERROR (src, LIBRARY, FAILED, (NULL),
        ("Input stream is already closed"));
    return FALSE;
  }

  if (G_IS_SEEKABLE (src->stream))
    src->position = g_seekable_tell (G_SEEKABLE (src->stream));

  GST_DEBUG_OBJECT (src, "started source");

  return TRUE;
}

static gboolean
gst_gio_base_src_stop (GstBaseSrc * base_src)
{
  GstGioBaseSrc *src = GST_GIO_BASE_SRC (base_src);
  GstGioBaseSrcClass *klass = GST_GIO_BASE_SRC_GET_CLASS (src);
  gboolean success;
  GError *err = NULL;

  if (klass->close_on_stop && G_IS_INPUT_STREAM (src->stream)) {
    GST_DEBUG_OBJECT (src, "closing stream");

    /* FIXME: can block but unfortunately we can't use async operations
     * here because they require a running main loop */
    success = g_input_stream_close (src->stream, src->cancel, &err);

    if (!success && !gst_gio_error (src, "g_input_stream_close", &err, NULL)) {
      GST_ELEMENT_WARNING (src, RESOURCE, CLOSE, (NULL),
          ("g_input_stream_close failed: %s", err->message));
      g_clear_error (&err);
    } else if (!success) {
      GST_ELEMENT_WARNING (src, RESOURCE, CLOSE, (NULL),
          ("g_input_stream_close failed"));
    } else {
      GST_DEBUG_OBJECT (src, "g_input_stream_close succeeded");
    }

    g_object_unref (src->stream);
    src->stream = NULL;
  } else {
    g_object_unref (src->stream);
    src->stream = NULL;
  }

  return TRUE;
}

static gboolean
gst_gio_base_src_get_size (GstBaseSrc * base_src, guint64 * size)
{
  GstGioBaseSrc *src = GST_GIO_BASE_SRC (base_src);

  if (G_IS_FILE_INPUT_STREAM (src->stream)) {
    GFileInfo *info;
    GError *err = NULL;

    info = g_file_input_stream_query_info (G_FILE_INPUT_STREAM (src->stream),
        G_FILE_ATTRIBUTE_STANDARD_SIZE, src->cancel, &err);

    if (info != NULL) {
      *size = g_file_info_get_size (info);
      g_object_unref (info);
      GST_DEBUG_OBJECT (src, "found size: %" G_GUINT64_FORMAT, *size);
      return TRUE;
    }

    if (!gst_gio_error (src, "g_file_input_stream_query_info", &err, NULL)) {

      if (GST_GIO_ERROR_MATCHES (err, NOT_SUPPORTED))
        GST_DEBUG_OBJECT (src, "size information not available");
      else
        GST_WARNING_OBJECT (src, "size information retrieval failed: %s",
            err->message);

      g_clear_error (&err);
    }
  }

  if (GST_GIO_STREAM_IS_SEEKABLE (src->stream)) {
    goffset old;
    goffset stream_size;
    gboolean ret;
    GSeekable *seekable = G_SEEKABLE (src->stream);
    GError *err = NULL;

    old = g_seekable_tell (seekable);

    ret = g_seekable_seek (seekable, 0, G_SEEK_END, src->cancel, &err);
    if (!ret) {
      if (!gst_gio_error (src, "g_seekable_seek", &err, NULL)) {
        if (GST_GIO_ERROR_MATCHES (err, NOT_SUPPORTED))
          GST_DEBUG_OBJECT (src,
              "Seeking to the end of stream is not supported");
        else
          GST_WARNING_OBJECT (src, "Seeking to end of stream failed: %s",
              err->message);
        g_clear_error (&err);
      } else {
        GST_WARNING_OBJECT (src, "Seeking to end of stream failed");
      }
      return FALSE;
    }

    stream_size = g_seekable_tell (seekable);

    ret = g_seekable_seek (seekable, old, G_SEEK_SET, src->cancel, &err);
    if (!ret) {
      if (!gst_gio_error (src, "g_seekable_seek", &err, NULL)) {
        if (GST_GIO_ERROR_MATCHES (err, NOT_SUPPORTED))
          GST_ERROR_OBJECT (src, "Seeking to the old position not supported");
        else
          GST_ERROR_OBJECT (src, "Seeking to the old position failed: %s",
              err->message);
        g_clear_error (&err);
      } else {
        GST_ERROR_OBJECT (src, "Seeking to the old position failed");
      }
      return FALSE;
    }

    if (stream_size >= 0) {
      *size = stream_size;
      return TRUE;
    }
  }

  return FALSE;
}

static gboolean
gst_gio_base_src_is_seekable (GstBaseSrc * base_src)
{
  GstGioBaseSrc *src = GST_GIO_BASE_SRC (base_src);
  gboolean seekable;

  seekable = GST_GIO_STREAM_IS_SEEKABLE (src->stream);

  GST_DEBUG_OBJECT (src, "can seek: %d", seekable);

  return seekable;
}

static gboolean
gst_gio_base_src_unlock (GstBaseSrc * base_src)
{
  GstGioBaseSrc *src = GST_GIO_BASE_SRC (base_src);

  GST_LOG_OBJECT (src, "triggering cancellation");

  g_cancellable_cancel (src->cancel);

  return TRUE;
}

static gboolean
gst_gio_base_src_unlock_stop (GstBaseSrc * base_src)
{
  GstGioBaseSrc *src = GST_GIO_BASE_SRC (base_src);

  GST_LOG_OBJECT (src, "resetting cancellable");

  g_object_unref (src->cancel);
  src->cancel = g_cancellable_new ();

  return TRUE;
}

static GstFlowReturn
gst_gio_base_src_create (GstBaseSrc * base_src, guint64 offset, guint size,
    GstBuffer ** buf_return)
{
  GstGioBaseSrc *src = GST_GIO_BASE_SRC (base_src);
  GstBuffer *buf;
  GstFlowReturn ret = GST_FLOW_OK;

  g_return_val_if_fail (G_IS_INPUT_STREAM (src->stream), GST_FLOW_ERROR);

  /* If we have the requested part in our cache take a subbuffer of that,
   * otherwise fill the cache again with at least 4096 bytes from the
   * requested offset and return a subbuffer of that.
   *
   * We need caching because every read/seek operation will need to go
   * over DBus if our backend is GVfs and this is painfully slow. */
  if (src->cache && offset >= GST_BUFFER_OFFSET (src->cache) &&
      offset + size <= GST_BUFFER_OFFSET_END (src->cache)) {
    GST_DEBUG_OBJECT (src, "Creating subbuffer from cached buffer: offset %"
        G_GUINT64_FORMAT " length %u", offset, size);

    buf = gst_buffer_copy_region (src->cache, GST_BUFFER_COPY_ALL,
        offset - GST_BUFFER_OFFSET (src->cache), size);

    GST_BUFFER_OFFSET (buf) = offset;
    GST_BUFFER_OFFSET_END (buf) = offset + size;
  } else {
    guint cachesize = MAX (4096, size);
    GstMapInfo map;
    gssize read, streamread, res;
    guint64 readoffset;
    gboolean success, eos;
    GError *err = NULL;
    GstBuffer *newbuffer;
    GstMemory *mem;
    gboolean waited_for_data = FALSE;
    GstGioBaseSrcClass *klass = GST_GIO_BASE_SRC_GET_CLASS (src);

    newbuffer = gst_buffer_new ();

    /* copy any overlapping data from the cached buffer */
    if (src->cache && offset >= GST_BUFFER_OFFSET (src->cache) &&
        offset <= GST_BUFFER_OFFSET_END (src->cache)) {
      read = GST_BUFFER_OFFSET_END (src->cache) - offset;
      GST_LOG_OBJECT (src,
          "Copying %" G_GSSIZE_FORMAT " bytes from cached buffer at %"
          G_GUINT64_FORMAT, read, offset - GST_BUFFER_OFFSET (src->cache));
      gst_buffer_copy_into (newbuffer, src->cache, GST_BUFFER_COPY_MEMORY,
          offset - GST_BUFFER_OFFSET (src->cache), read);
    } else {
      read = 0;
    }

    if (src->cache)
      gst_buffer_unref (src->cache);
    src->cache = newbuffer;

    readoffset = offset + read;
    GST_LOG_OBJECT (src,
        "Reading %u bytes from offset %" G_GUINT64_FORMAT, cachesize,
        readoffset);

    if (G_UNLIKELY (readoffset != src->position)) {
      if (!GST_GIO_STREAM_IS_SEEKABLE (src->stream))
        return GST_FLOW_NOT_SUPPORTED;

      GST_DEBUG_OBJECT (src, "Seeking to position %" G_GUINT64_FORMAT,
          readoffset);
      ret =
          gst_gio_seek (src, G_SEEKABLE (src->stream), readoffset, src->cancel);

      if (ret == GST_FLOW_OK)
        src->position = readoffset;
      else
        return ret;
    }

    /* GIO sometimes gives less bytes than requested although
     * it's not at the end of file. SMB for example only
     * supports reads up to 64k. So we loop here until we get at
     * at least the requested amount of bytes or a read returns
     * nothing. */
    mem = gst_allocator_alloc (NULL, cachesize, NULL);
    if (mem == NULL) {
      GST_ERROR_OBJECT (src, "Failed to allocate %u bytes", cachesize);
      return GST_FLOW_ERROR;
    }

    gst_memory_map (mem, &map, GST_MAP_WRITE);
    streamread = 0;
    while (size - read > 0 && (res =
            g_input_stream_read (G_INPUT_STREAM (src->stream),
                map.data + streamread, cachesize - streamread, src->cancel,
                &err)) >= 0) {

      read += res;
      streamread += res;
      src->position += res;

      if (res != 0)
        continue;

      if (!klass->wait_for_data || !klass->wait_for_data (src))
        break;

      waited_for_data = TRUE;
    }

    if (waited_for_data && klass->waited_for_data)
      klass->waited_for_data (src);

    gst_memory_unmap (mem, &map);
    gst_buffer_append_memory (src->cache, mem);

    success = (read >= 0);
    eos = (cachesize > 0 && read == 0);

    if (!success && !gst_gio_error (src, "g_input_stream_read", &err, &ret)) {
      GST_ELEMENT_ERROR (src, RESOURCE, READ, (NULL),
          ("Could not read from stream: %s", err->message));
      g_clear_error (&err);
    }

    if (success && !eos) {
      GST_BUFFER_OFFSET (src->cache) = offset;
      GST_BUFFER_OFFSET_END (src->cache) = offset + read;

      GST_DEBUG_OBJECT (src, "Read successful");
      GST_DEBUG_OBJECT (src, "Creating subbuffer from new "
          "cached buffer: offset %" G_GUINT64_FORMAT " length %u", offset,
          size);

      buf =
          gst_buffer_copy_region (src->cache, GST_BUFFER_COPY_ALL, 0, MIN (size,
              read));

      GST_BUFFER_OFFSET (buf) = offset;
      GST_BUFFER_OFFSET_END (buf) = offset + MIN (size, read);
    } else {
      GST_DEBUG_OBJECT (src, "Read not successful");
      gst_buffer_unref (src->cache);
      src->cache = NULL;
      buf = NULL;
    }

    if (eos)
      ret = GST_FLOW_EOS;
  }

  *buf_return = buf;

  return ret;
}

static gboolean
gst_gio_base_src_query (GstBaseSrc * base_src, GstQuery * query)
{
  gboolean ret = FALSE;
  GstGioBaseSrc *src = GST_GIO_BASE_SRC (base_src);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_URI:
      if (GST_IS_URI_HANDLER (src)) {
        gchar *uri = gst_uri_handler_get_uri (GST_URI_HANDLER (src));
        gst_query_set_uri (query, uri);
        g_free (uri);
        ret = TRUE;
      }
      break;
    default:
      ret = FALSE;
      break;
  }

  if (!ret)
    ret = GST_BASE_SRC_CLASS (parent_class)->query (base_src, query);

  return ret;
}
