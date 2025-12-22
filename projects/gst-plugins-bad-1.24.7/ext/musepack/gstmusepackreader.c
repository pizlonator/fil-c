/* GStreamer Musepack decoder plugin
 * Copyright (C) 2004 Ronald Bultje <rbultje@ronald.bitfreak.net>
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

#include "gstmusepackreader.h"
#include <gst/gst.h>
#include <string.h>


GST_DEBUG_CATEGORY_EXTERN (musepackdec_debug);
#define GST_CAT_DEFAULT musepackdec_debug

static mpc_int32_t gst_musepack_reader_peek (mpc_reader * this, void *ptr,
    mpc_int32_t size);
static mpc_int32_t gst_musepack_reader_read (mpc_reader * this, void *ptr,
    mpc_int32_t size);
static mpc_bool_t gst_musepack_reader_seek (mpc_reader * this,
    mpc_int32_t offset);
static mpc_int32_t gst_musepack_reader_tell (mpc_reader * this);
static mpc_int32_t gst_musepack_reader_get_size (mpc_reader * this);
static mpc_bool_t gst_musepack_reader_canseek (mpc_reader * this);

static mpc_int32_t
gst_musepack_reader_peek (mpc_reader * this, void *ptr, mpc_int32_t size)
{
  GstMusepackDec *musepackdec = GST_MUSEPACK_DEC (this->data);
  GstFlowReturn flow_ret;
  GstBuffer *buf = NULL;
  guint read;

  g_return_val_if_fail (size > 0, 0);

  /* GST_LOG_OBJECT (musepackdec, "size=%d", size); */

  flow_ret = gst_pad_pull_range (musepackdec->sinkpad, musepackdec->offset,
      size, &buf);

  if (flow_ret != GST_FLOW_OK) {
    GST_DEBUG_OBJECT (musepackdec, "Flow: %s", gst_flow_get_name (flow_ret));
    return 0;
  }

  read = MIN (gst_buffer_get_size (buf), size);

  if (read < size) {
    GST_WARNING_OBJECT (musepackdec, "Short read: got only %u bytes of %u "
        "bytes requested at offset %" G_GINT64_FORMAT, read, size,
        musepackdec->offset);
    /* GST_ELEMENT_ERROR (musepackdec, RESOURCE, READ, (NULL), (NULL)); */
  }

  gst_buffer_extract (buf, 0, ptr, read);
  gst_buffer_unref (buf);
  return read;
}

static mpc_int32_t
gst_musepack_reader_read (mpc_reader * this, void *ptr, mpc_int32_t size)
{
  GstMusepackDec *musepackdec = GST_MUSEPACK_DEC (this->data);
  gint read;

  /* read = peek + flush */
  if ((read = gst_musepack_reader_peek (this, ptr, size)) > 0) {
    musepackdec->offset += read;
  }

  return read;
}

static mpc_bool_t
gst_musepack_reader_seek (mpc_reader * this, mpc_int32_t offset)
{
  GstMusepackDec *musepackdec = GST_MUSEPACK_DEC (this->data);
  mpc_int32_t length;

  length = gst_musepack_reader_get_size (this);
  if (length > 0 && offset >= 0 && offset < length) {
    musepackdec->offset = offset;
    GST_LOG_OBJECT (musepackdec, "Seek'ed to byte offset %d", (gint) offset);
    return TRUE;
  } else {
    GST_DEBUG_OBJECT (musepackdec, "Cannot seek to offset %d", (gint) offset);
    return FALSE;
  }
}

static mpc_int32_t
gst_musepack_reader_tell (mpc_reader * this)
{
  GstMusepackDec *musepackdec = GST_MUSEPACK_DEC (this->data);
  return musepackdec->offset;
}

static mpc_int32_t
gst_musepack_reader_get_size (mpc_reader * this)
{
  GstMusepackDec *dec = GST_MUSEPACK_DEC (this->data);
  GstQuery *query;
  GstFormat format;
  gint64 length;

  query = gst_query_new_duration (GST_FORMAT_BYTES);
  if (gst_pad_peer_query (dec->sinkpad, query))
    gst_query_parse_duration (query, &format, &length);
  else
    length = -1;

  return (mpc_int32_t) length;
}

static mpc_bool_t
gst_musepack_reader_canseek (mpc_reader * this)
{
  return TRUE;
}

void
gst_musepack_init_reader (mpc_reader * r, GstMusepackDec * musepackdec)
{
  r->data = musepackdec;

  r->read = gst_musepack_reader_read;
  r->seek = gst_musepack_reader_seek;
  r->tell = gst_musepack_reader_tell;
  r->get_size = gst_musepack_reader_get_size;
  r->canseek = gst_musepack_reader_canseek;
}
