/* GStreamer libsndfile plugin
 * Copyright (C) 1999,2000 Erik Walthinsen <omega@cse.ogi.edu>
 *               2000,2005 Wim Taymans <wim@fluendo.com>
 *               2003,2007 Andy Wingo <wingo@pobox.com>
 *
 * gstsfsrc.c:
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
#  include "config.h"
#endif

#include <glib/gi18n-lib.h>

#include "gstsfsrc.h"

enum
{
  PROP_0,
  PROP_LOCATION
};

static GstStaticPadTemplate sf_src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
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


GST_DEBUG_CATEGORY_STATIC (gst_sf_src_debug);
#define GST_CAT_DEFAULT gst_sf_src_debug


#define DEFAULT_BUFFER_FRAMES	(256)


static void gst_sf_src_finalize (GObject * object);
static void gst_sf_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_sf_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_sf_src_start (GstBaseSrc * bsrc);
static gboolean gst_sf_src_stop (GstBaseSrc * bsrc);
static gboolean gst_sf_src_is_seekable (GstBaseSrc * bsrc);
static gboolean gst_sf_src_get_size (GstBaseSrc * bsrc, guint64 * size);
static GstFlowReturn gst_sf_src_create (GstBaseSrc * bsrc, guint64 offset,
    guint length, GstBuffer ** buffer);
static GstCaps *gst_sf_src_get_caps (GstBaseSrc * bsrc);
static gboolean gst_sf_src_set_caps (GstBaseSrc * bsrc, GstCaps * caps);
static gboolean gst_sf_src_check_get_range (GstBaseSrc * bsrc);
static void gst_sf_src_fixate (GstBaseSrc * bsrc, GstCaps * caps);

GST_BOILERPLATE (GstSFSrc, gst_sf_src, GstBaseSrc, GST_TYPE_BASE_SRC);

static void
gst_sf_src_base_init (gpointer g_class)
{
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_add_static_pad_template (gstelement_class, &sf_src_factory);

  gst_element_class_set_static_metadata (gstelement_class, "Sndfile source",
      "Source/Audio",
      "Read audio streams from disk using libsndfile",
      "Andy Wingo <wingo at pobox dot com>");
  GST_DEBUG_CATEGORY_INIT (gst_sf_src_debug, "sfsrc", 0, "sfsrc element");
}

static void
gst_sf_src_class_init (GstSFSrcClass * klass)
{
  GObjectClass *gobject_class;
  GstBaseSrcClass *gstbasesrc_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gstbasesrc_class = GST_BASE_SRC_CLASS (klass);

  gobject_class->set_property = gst_sf_src_set_property;
  gobject_class->get_property = gst_sf_src_get_property;

  g_object_class_install_property (gobject_class, PROP_LOCATION,
      g_param_spec_string ("location", "File Location",
          "Location of the file to read", NULL, G_PARAM_READWRITE));

  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_sf_src_finalize);

  gstbasesrc_class->start = GST_DEBUG_FUNCPTR (gst_sf_src_start);
  gstbasesrc_class->stop = GST_DEBUG_FUNCPTR (gst_sf_src_stop);
  gstbasesrc_class->is_seekable = GST_DEBUG_FUNCPTR (gst_sf_src_is_seekable);
  gstbasesrc_class->get_size = GST_DEBUG_FUNCPTR (gst_sf_src_get_size);
  gstbasesrc_class->create = GST_DEBUG_FUNCPTR (gst_sf_src_create);
  gstbasesrc_class->get_caps = GST_DEBUG_FUNCPTR (gst_sf_src_get_caps);
  gstbasesrc_class->set_caps = GST_DEBUG_FUNCPTR (gst_sf_src_set_caps);
  gstbasesrc_class->check_get_range =
      GST_DEBUG_FUNCPTR (gst_sf_src_check_get_range);
  gstbasesrc_class->fixate = GST_DEBUG_FUNCPTR (gst_sf_src_fixate);
}

static void
gst_sf_src_init (GstSFSrc * src, GstSFSrcClass * g_class)
{
}

static void
gst_sf_src_finalize (GObject * object)
{
  GstSFSrc *src;

  src = GST_SF_SRC (object);

  g_free (src->location);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_sf_src_set_location (GstSFSrc * this, const gchar * location)
{
  if (this->file)
    goto was_open;

  g_free (this->location);

  this->location = location ? g_strdup (location) : NULL;

  return;

was_open:
  {
    g_warning ("Changing the `location' property on sfsrc when "
        "a file is open not supported.");
    return;
  }
}

static void
gst_sf_src_set_property (GObject * object, guint prop_id, const GValue * value,
    GParamSpec * pspec)
{
  GstSFSrc *this = GST_SF_SRC (object);

  switch (prop_id) {
    case PROP_LOCATION:
      gst_sf_src_set_location (this, g_value_get_string (value));
      break;

    default:
      break;
  }
}

static void
gst_sf_src_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstSFSrc *this = GST_SF_SRC (object);

  switch (prop_id) {
    case PROP_LOCATION:
      g_value_set_string (value, this->location);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstFlowReturn
gst_sf_src_create (GstBaseSrc * bsrc, guint64 offset, guint length,
    GstBuffer ** buffer)
{
  GstSFSrc *this;
  GstBuffer *buf;
/* FIXME discont is set but not used */
#if 0
  gboolean discont = FALSE;
#endif
  sf_count_t bytes_read;

  this = GST_SF_SRC (bsrc);

  if (G_UNLIKELY (offset % this->bytes_per_frame))
    goto bad_offset;
  if (G_UNLIKELY (length % this->bytes_per_frame))
    goto bad_length;

  offset /= this->bytes_per_frame;

  if (G_UNLIKELY (this->offset != offset)) {
    sf_count_t pos;

    pos = sf_seek (this->file, offset, SEEK_SET);

    if (G_UNLIKELY (pos < 0 || pos != offset))
      goto seek_failed;

    this->offset = offset;
#if 0
    discont = TRUE;
#endif
  }

  buf = gst_buffer_new_and_alloc (length);

  /* now make length in frames */
  length /= this->bytes_per_frame;

  bytes_read = this->reader (this->file, GST_BUFFER_DATA (buf), length);
  if (G_UNLIKELY (bytes_read < 0))
    goto could_not_read;

  if (G_UNLIKELY (bytes_read == 0 && length > 0))
    goto eos;

  GST_BUFFER_SIZE (buf) = bytes_read * this->bytes_per_frame;

  GST_BUFFER_OFFSET (buf) = offset;
  GST_BUFFER_OFFSET_END (buf) = offset + length;
  GST_BUFFER_TIMESTAMP (buf) = gst_util_uint64_scale_int (offset,
      GST_SECOND, this->rate);
  GST_BUFFER_DURATION (buf) = gst_util_uint64_scale_int (offset + length,
      GST_SECOND, this->rate) - GST_BUFFER_TIMESTAMP (buf);

  gst_buffer_set_caps (buf, GST_PAD_CAPS (GST_BASE_SRC_PAD (bsrc)));

  *buffer = buf;

  this->offset += length;

  return GST_FLOW_OK;

  /* ERROR */
bad_offset:
  {
    GST_ELEMENT_ERROR (this, RESOURCE, SEEK,
        (NULL), ("offset %" G_GUINT64_FORMAT " not divisible by %d bytes per "
            "frame", offset, this->bytes_per_frame));
    return GST_FLOW_ERROR;
  }
bad_length:
  {
    GST_ELEMENT_ERROR (this, RESOURCE, SEEK, (NULL),
        ("length %u not divisible by %d bytes per frame", length,
            this->bytes_per_frame));
    return GST_FLOW_ERROR;
  }
seek_failed:
  {
    GST_ELEMENT_ERROR (this, RESOURCE, READ, (NULL), GST_ERROR_SYSTEM);
    return GST_FLOW_ERROR;
  }
could_not_read:
  {
    GST_ELEMENT_ERROR (this, RESOURCE, READ, (NULL), GST_ERROR_SYSTEM);
    gst_buffer_unref (buf);
    return GST_FLOW_ERROR;
  }
eos:
  {
    GST_DEBUG ("EOS, baby");
    gst_buffer_unref (buf);
    return GST_FLOW_UNEXPECTED;
  }
}

static gboolean
gst_sf_src_is_seekable (GstBaseSrc * basesrc)
{
  return TRUE;
}

static gboolean
gst_sf_src_get_size (GstBaseSrc * basesrc, guint64 * size)
{
  GstSFSrc *this;
  sf_count_t end;

  this = GST_SF_SRC (basesrc);

  end = sf_seek (this->file, 0, SEEK_END);

  sf_seek (this->file, this->offset, SEEK_SET);

  *size = end * this->bytes_per_frame;

  return TRUE;
}

static gboolean
gst_sf_src_open_file (GstSFSrc * this)
{
  int mode;
  SF_INFO info;

  g_return_val_if_fail (this->file == NULL, FALSE);

  if (!this->location)
    goto no_filename;

  mode = SFM_READ;
  info.format = 0;

  this->file = sf_open (this->location, mode, &info);

  if (!this->file)
    goto open_failed;

  this->channels = info.channels;
  this->rate = info.samplerate;
  /* do something with info.seekable? */

  return TRUE;

no_filename:
  {
    GST_ELEMENT_ERROR (this, RESOURCE, NOT_FOUND,
        (_("No file name specified for writing.")), (NULL));
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
gst_sf_src_close_file (GstSFSrc * this)
{
  int err = 0;

  g_return_if_fail (this->file != NULL);

  GST_INFO_OBJECT (this, "Closing file %s", this->location);

  if ((err = sf_close (this->file)))
    goto close_failed;

  this->file = NULL;
  this->offset = 0;
  this->channels = 0;
  this->rate = 0;

  return;

close_failed:
  {
    GST_ELEMENT_ERROR (this, RESOURCE, CLOSE,
        ("Could not close file file \"%s\".", this->location),
        ("soundfile error: %s", sf_error_number (err)));
    return;
  }
}

static gboolean
gst_sf_src_start (GstBaseSrc * basesrc)
{
  GstSFSrc *this = GST_SF_SRC (basesrc);

  return gst_sf_src_open_file (this);
}

/* unmap and close the file */
static gboolean
gst_sf_src_stop (GstBaseSrc * basesrc)
{
  GstSFSrc *this = GST_SF_SRC (basesrc);

  gst_sf_src_close_file (this);

  return TRUE;
}

static GstCaps *
gst_sf_src_get_caps (GstBaseSrc * bsrc)
{
  GstSFSrc *this;
  GstCaps *ret;

  this = GST_SF_SRC (bsrc);

  ret = gst_caps_copy (gst_pad_get_pad_template_caps (bsrc->srcpad));

  if (this->file) {
    GstStructure *s;
    gint i;

    for (i = 0; i < gst_caps_get_size (ret); i++) {
      s = gst_caps_get_structure (ret, i);
      gst_structure_set (s, "channels", G_TYPE_INT, this->channels,
          "rate", G_TYPE_INT, this->rate, NULL);
    }
  }

  return ret;
}

static gboolean
gst_sf_src_set_caps (GstBaseSrc * bsrc, GstCaps * caps)
{
  GstSFSrc *this = (GstSFSrc *) bsrc;
  GstStructure *structure;
  gint width;

  structure = gst_caps_get_structure (caps, 0);

  if (!this->file)
    goto file_not_open;

  if (!gst_structure_get_int (structure, "width", &width))
    goto impossible;

  if (gst_structure_has_name (structure, "audio/x-raw-int")) {
    switch (width) {
      case 16:
        this->reader = (GstSFReader) sf_readf_short;
        break;
      case 32:
        this->reader = (GstSFReader) sf_readf_int;
        break;
      default:
        goto impossible;
    }
  } else {
    switch (width) {
      case 32:
        this->reader = (GstSFReader) sf_readf_float;
        break;
      default:
        goto impossible;
    }
  }

  this->bytes_per_frame = width * this->channels / 8;

  return TRUE;

impossible:
  {
    g_warning ("something impossible happened");
    return FALSE;
  }
file_not_open:
  {
    GST_WARNING_OBJECT (this, "file has to be open in order to set caps");
    return FALSE;
  }
}

static gboolean
gst_sf_src_check_get_range (GstBaseSrc * bsrc)
{
  return TRUE;
}

static void
gst_sf_src_fixate (GstBaseSrc * bsrc, GstCaps * caps)
{
  GstStructure *s;
  gint width, depth;

  s = gst_caps_get_structure (caps, 0);

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
