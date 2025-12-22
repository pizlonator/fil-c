/* GStreamer bz2 decoder
 * Copyright (C) 2006 Lutz Müller <lutz topfrose de>

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
#include "gstbz2dec.h"

#include <gst/base/gsttypefindhelper.h>

#include <bzlib.h>
#include <string.h>

GST_DEBUG_CATEGORY_STATIC (bz2dec_debug);
#define GST_CAT_DEFAULT bz2dec_debug

static GstStaticPadTemplate sink_template =
GST_STATIC_PAD_TEMPLATE ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-bzip"));
static GstStaticPadTemplate src_template =
GST_STATIC_PAD_TEMPLATE ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

struct _GstBz2dec
{
  GstElement parent;

  GstPad *sink;
  GstPad *src;

  /* Properties */
  guint first_buffer_size;
  guint buffer_size;

  gboolean ready;
  bz_stream stream;
  guint64 offset;
};

struct _GstBz2decClass
{
  GstElementClass parent_class;
};

#define gst_bz2dec_parent_class parent_class
G_DEFINE_TYPE (GstBz2dec, gst_bz2dec, GST_TYPE_ELEMENT);
GST_ELEMENT_REGISTER_DEFINE (bz2dec, "bz2dec", GST_RANK_NONE, GST_TYPE_BZ2DEC);

#define DEFAULT_FIRST_BUFFER_SIZE 1024
#define DEFAULT_BUFFER_SIZE 1024

enum
{
  PROP_0,
  PROP_FIRST_BUFFER_SIZE,
  PROP_BUFFER_SIZE
};

static void
gst_bz2dec_decompress_end (GstBz2dec * b)
{
  g_return_if_fail (GST_IS_BZ2DEC (b));

  if (b->ready) {
    BZ2_bzDecompressEnd (&b->stream);
    memset (&b->stream, 0, sizeof (b->stream));
    b->ready = FALSE;
  }
}

static void
gst_bz2dec_decompress_init (GstBz2dec * b)
{
  g_return_if_fail (GST_IS_BZ2DEC (b));

  gst_bz2dec_decompress_end (b);
  b->offset = 0;
  switch (BZ2_bzDecompressInit (&b->stream, 0, 0)) {
    case BZ_OK:
      b->ready = TRUE;
      return;
    default:
      b->ready = FALSE;
      GST_ELEMENT_ERROR (b, CORE, FAILED, (NULL),
          ("Failed to start decompression."));
      return;
  }
}

static GstFlowReturn
gst_bz2dec_chain (GstPad * pad, GstObject * parent, GstBuffer * in)
{
  GstFlowReturn flow = GST_FLOW_OK;
  GstBuffer *out;
  GstBz2dec *b;
  int r = BZ_OK;
  GstMapInfo map = GST_MAP_INFO_INIT, omap;

  b = GST_BZ2DEC (parent);

  if (!b->ready)
    goto not_ready;

  gst_buffer_map (in, &map, GST_MAP_READ);
  b->stream.next_in = (char *) map.data;
  b->stream.avail_in = map.size;

  do {
    guint n;

    /* Create the output buffer */
    out =
        gst_buffer_new_and_alloc (b->offset ? b->buffer_size : b->
        first_buffer_size);

    /* Decode */
    gst_buffer_map (out, &omap, GST_MAP_WRITE);
    b->stream.next_out = (char *) omap.data;
    b->stream.avail_out = omap.size;
    r = BZ2_bzDecompress (&b->stream);
    gst_buffer_unmap (out, &omap);
    if ((r != BZ_OK) && (r != BZ_STREAM_END))
      goto decode_failed;

    if (b->stream.avail_out >= gst_buffer_get_size (out)) {
      gst_buffer_unref (out);
      break;
    }
    gst_buffer_resize (out, 0, gst_buffer_get_size (out) - b->stream.avail_out);
    GST_BUFFER_OFFSET (out) =
        b->stream.total_out_lo32 - gst_buffer_get_size (out);

    /* Configure source pad (if necessary) */
    if (!b->offset) {
      GstCaps *caps = NULL;

      caps = gst_type_find_helper_for_buffer (GST_OBJECT (b), out, NULL);
      if (caps) {
        gst_pad_set_caps (b->src, caps);
        gst_pad_use_fixed_caps (b->src);
        gst_caps_unref (caps);
      } else {
        /* FIXME: shouldn't we queue output buffers until we have a type? */
      }
    }

    /* Push data */
    n = gst_buffer_get_size (out);
    flow = gst_pad_push (b->src, out);
    if (flow != GST_FLOW_OK)
      break;
    b->offset += n;
  } while (r != BZ_STREAM_END);

done:

  gst_buffer_unmap (in, &map);
  gst_buffer_unref (in);
  return flow;

/* ERRORS */
decode_failed:
  {
    GST_ELEMENT_ERROR (b, STREAM, DECODE, (NULL),
        ("Failed to decompress data (error code %i).", r));
    gst_bz2dec_decompress_init (b);
    gst_buffer_unref (out);
    flow = GST_FLOW_ERROR;
    goto done;
  }
not_ready:
  {
    GST_ELEMENT_ERROR (b, LIBRARY, FAILED, (NULL), ("Decompressor not ready."));
    flow = GST_FLOW_FLUSHING;
    goto done;
  }
}

static void
gst_bz2dec_init (GstBz2dec * b)
{
  b->first_buffer_size = DEFAULT_FIRST_BUFFER_SIZE;
  b->buffer_size = DEFAULT_BUFFER_SIZE;

  b->sink = gst_pad_new_from_static_template (&sink_template, "sink");
  gst_pad_set_chain_function (b->sink, GST_DEBUG_FUNCPTR (gst_bz2dec_chain));
  gst_element_add_pad (GST_ELEMENT (b), b->sink);

  b->src = gst_pad_new_from_static_template (&src_template, "src");
  gst_element_add_pad (GST_ELEMENT (b), b->src);
  gst_pad_use_fixed_caps (b->src);

  gst_bz2dec_decompress_init (b);
}

static void
gst_bz2dec_finalize (GObject * object)
{
  GstBz2dec *b = GST_BZ2DEC (object);

  gst_bz2dec_decompress_end (b);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_bz2dec_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstBz2dec *b = GST_BZ2DEC (object);

  switch (prop_id) {
    case PROP_BUFFER_SIZE:
      g_value_set_uint (value, b->buffer_size);
      break;
    case PROP_FIRST_BUFFER_SIZE:
      g_value_set_uint (value, b->first_buffer_size);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
gst_bz2dec_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstBz2dec *b = GST_BZ2DEC (object);

  switch (prop_id) {
    case PROP_BUFFER_SIZE:
      b->buffer_size = g_value_get_uint (value);
      break;
    case PROP_FIRST_BUFFER_SIZE:
      b->first_buffer_size = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static GstStateChangeReturn
gst_bz2dec_change_state (GstElement * element, GstStateChange transition)
{
  GstBz2dec *b = GST_BZ2DEC (element);
  GstStateChangeReturn ret;

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret != GST_STATE_CHANGE_SUCCESS)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      gst_bz2dec_decompress_init (b);
      break;
    default:
      break;
  }
  return ret;
}

static void
gst_bz2dec_class_init (GstBz2decClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);

  gstelement_class->change_state = GST_DEBUG_FUNCPTR (gst_bz2dec_change_state);

  gobject_class->finalize = gst_bz2dec_finalize;
  gobject_class->get_property = gst_bz2dec_get_property;
  gobject_class->set_property = gst_bz2dec_set_property;

  g_object_class_install_property (G_OBJECT_CLASS (klass),
      PROP_FIRST_BUFFER_SIZE, g_param_spec_uint ("first-buffer-size",
          "Size of first buffer", "Size of first buffer (used to determine the "
          "mime type of the uncompressed data)", 1, G_MAXUINT,
          DEFAULT_FIRST_BUFFER_SIZE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_BUFFER_SIZE,
      g_param_spec_uint ("buffer-size", "Buffer size", "Buffer size",
          1, G_MAXUINT, DEFAULT_BUFFER_SIZE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_add_static_pad_template (gstelement_class, &sink_template);
  gst_element_class_add_static_pad_template (gstelement_class, &src_template);
  gst_element_class_set_static_metadata (gstelement_class, "BZ2 decoder",
      "Codec/Decoder", "Decodes compressed streams",
      "Lutz Mueller <lutz@users.sourceforge.net>");

  GST_DEBUG_CATEGORY_INIT (bz2dec_debug, "bz2dec", 0, "BZ2 decompressor");
}
