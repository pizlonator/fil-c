/* GStreamer
 * Copyright (C) 2020 Mathieu Duponchelle <mathieu@centricular.com>
 * Copyright (C) 2023 Seungha Yang <seungha@centricular.com>
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

#include "gstdwritetextoverlay.h"
#include <gst/base/base.h>
#include <caption.h>
#include <mutex>

GST_DEBUG_CATEGORY_STATIC (dwrite_text_overlay_debug);
#define GST_CAT_DEFAULT dwrite_text_overlay_debug

enum
{
  PROP_0,
  PROP_ENABLE_CC,
  PROP_CC_FIELD,
  PROP_CC_TIMEOUT,
  PROP_REMOVE_CC_META,
};

/* *INDENT-OFF* */
static std::vector<GParamSpec *> _pspec;
/* *INDENT-ON* */

#define DEFAULT_ENABLE_CC TRUE
#define DEFAULT_CC_FIELD -1
#define DEFAULT_CC_TIMEOUT GST_CLOCK_TIME_NONE
#define DEFAULT_REMOVE_CC_META FALSE

/* *INDENT-OFF* */
struct GstDWriteTextOverlayPrivate
{
  std::mutex lock;
  caption_frame_t frame;
  GstClockTime caption_running_time;
  GstClockTime running_time;
  guint8 selected_field;

  std::string closed_caption;
  std::string text;

  /* properties */
  gboolean enable_cc = DEFAULT_ENABLE_CC;
  gint field = DEFAULT_CC_FIELD;
  GstClockTime timeout = DEFAULT_CC_TIMEOUT;
  gboolean remove_caption_meta = DEFAULT_REMOVE_CC_META;
};
/* *INDENT-ON* */

struct _GstDWriteTextOverlay
{
  GstDWriteBaseOverlay parent;

  GstDWriteTextOverlayPrivate *priv;
};

static void gst_dwrite_text_overlay_finalize (GObject * object);
static void gst_dwrite_text_overlay_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_dwrite_text_overlay_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);
static gboolean gst_dwrite_text_overlay_start (GstBaseTransform * trans);
static gboolean gst_dwrite_text_overlay_sink_event (GstBaseTransform * trans,
    GstEvent * event);
static WString gst_dwrite_text_overlay_get_text (GstDWriteBaseOverlay * overlay,
    const WString & default_text, GstBuffer * buffer);
static void
gst_dwrite_text_overlay_after_transform (GstDWriteBaseOverlay * overlay,
    GstBuffer * buffer);

#define gst_dwrite_text_overlay_parent_class parent_class
G_DEFINE_TYPE (GstDWriteTextOverlay, gst_dwrite_text_overlay,
    GST_TYPE_DWRITE_BASE_OVERLAY);

static void
gst_dwrite_text_overlay_class_init (GstDWriteTextOverlayClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstBaseTransformClass *trans_class = GST_BASE_TRANSFORM_CLASS (klass);
  GstDWriteBaseOverlayClass *overlay_class =
      GST_DWRITE_BASE_OVERLAY_CLASS (klass);

  object_class->finalize = gst_dwrite_text_overlay_finalize;
  object_class->set_property = gst_dwrite_text_overlay_set_property;
  object_class->get_property = gst_dwrite_text_overlay_get_property;

  gst_dwrite_text_overlay_build_param_specs (_pspec);
  for (guint i = 0; i < _pspec.size (); i++)
    g_object_class_install_property (object_class, i + 1, _pspec[i]);

  gst_element_class_set_static_metadata (element_class,
      "DirectWrite Text Overlay", "Filter/Editor/Video",
      "Adds text strings on top of a video buffer",
      "Seungha Yang <seungha@centricular.com>");

  trans_class->start = GST_DEBUG_FUNCPTR (gst_dwrite_text_overlay_start);
  trans_class->sink_event =
      GST_DEBUG_FUNCPTR (gst_dwrite_text_overlay_sink_event);

  overlay_class->get_text =
      GST_DEBUG_FUNCPTR (gst_dwrite_text_overlay_get_text);
  overlay_class->after_transform =
      GST_DEBUG_FUNCPTR (gst_dwrite_text_overlay_after_transform);

  GST_DEBUG_CATEGORY_INIT (dwrite_text_overlay_debug,
      "dwritetextoverlay", 0, "dwritetextoverlay");
}

static void
gst_dwrite_text_overlay_init (GstDWriteTextOverlay * self)
{
  self->priv = new GstDWriteTextOverlayPrivate ();
  self->priv->closed_caption.reserve (CAPTION_FRAME_TEXT_BYTES);

  g_object_set (self, "text-alignment", DWRITE_TEXT_ALIGNMENT_CENTER,
      "paragraph-alignment", DWRITE_PARAGRAPH_ALIGNMENT_FAR,
      "font-size", 20.0, nullptr);
}

static void
gst_dwrite_text_overlay_finalize (GObject * object)
{
  GstDWriteTextOverlay *self = GST_DWRITE_TEXT_OVERLAY (object);

  delete self->priv;

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_dwrite_text_overlay_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstDWriteTextOverlay *self = GST_DWRITE_TEXT_OVERLAY (object);
  GstDWriteTextOverlayPrivate *priv = self->priv;
  std::lock_guard < std::mutex > lk (priv->lock);

  switch (prop_id) {
    case PROP_ENABLE_CC:
      priv->enable_cc = g_value_get_boolean (value);
      break;
    case PROP_CC_FIELD:
      priv->field = g_value_get_int (value);
      if (priv->field == -1) {
        priv->selected_field = 0xff;
      } else {
        priv->selected_field = (guint) priv->field;
      }
      break;
    case PROP_CC_TIMEOUT:
      priv->timeout = g_value_get_uint64 (value);
      break;
    case PROP_REMOVE_CC_META:
      priv->remove_caption_meta = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_dwrite_text_overlay_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstDWriteTextOverlay *self = GST_DWRITE_TEXT_OVERLAY (object);
  GstDWriteTextOverlayPrivate *priv = self->priv;
  std::lock_guard < std::mutex > lk (priv->lock);

  switch (prop_id) {
    case PROP_ENABLE_CC:
      g_value_set_boolean (value, priv->enable_cc);
      break;
    case PROP_CC_FIELD:
      g_value_set_int (value, priv->field);
      break;
    case PROP_CC_TIMEOUT:
      g_value_set_uint64 (value, priv->timeout);
      break;
    case PROP_REMOVE_CC_META:
      g_value_set_boolean (value, priv->remove_caption_meta);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_dwrite_text_overlay_start (GstBaseTransform * trans)
{
  GstDWriteTextOverlay *self = GST_DWRITE_TEXT_OVERLAY (trans);
  GstDWriteTextOverlayPrivate *priv = self->priv;

  caption_frame_init (&priv->frame);
  priv->running_time = GST_CLOCK_TIME_NONE;
  priv->caption_running_time = GST_CLOCK_TIME_NONE;
  priv->selected_field = 0xff;
  priv->closed_caption.clear ();

  return GST_BASE_TRANSFORM_CLASS (parent_class)->start (trans);
}

static gboolean
gst_dwrite_text_overlay_sink_event (GstBaseTransform * trans, GstEvent * event)
{
  GstDWriteTextOverlay *self = GST_DWRITE_TEXT_OVERLAY (trans);
  GstDWriteTextOverlayPrivate *priv = self->priv;

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_SEGMENT:
      priv->caption_running_time = GST_CLOCK_TIME_NONE;
      priv->running_time = GST_CLOCK_TIME_NONE;
      break;
    default:
      break;
  }

  return GST_BASE_TRANSFORM_CLASS (parent_class)->sink_event (trans, event);
}

static guint
gst_dwrite_text_overlay_extract_cdp (GstDWriteTextOverlay * self,
    const guint8 * cdp, guint cdp_len, guint * pos)
{
  GstByteReader br;
  guint16 u16;
  guint8 u8;
  guint8 flags;
  guint len = 0;

  GST_TRACE_OBJECT (self, "Extracting CDP");

  /* Header + footer length */
  if (cdp_len < 11) {
    GST_WARNING_OBJECT (self, "cdp packet too short (%u). expected at "
        "least %u", cdp_len, 11);
    return 0;
  }

  gst_byte_reader_init (&br, cdp, cdp_len);
  u16 = gst_byte_reader_get_uint16_be_unchecked (&br);
  if (u16 != 0x9669) {
    GST_WARNING_OBJECT (self, "cdp packet does not have initial magic bytes "
        "of 0x9669");
    return 0;
  }

  u8 = gst_byte_reader_get_uint8_unchecked (&br);
  if (u8 != cdp_len) {
    GST_WARNING_OBJECT (self, "cdp packet length (%u) does not match passed "
        "in value (%u)", u8, cdp_len);
    return 0;
  }

  /* skip framerate value */
  gst_byte_reader_skip_unchecked (&br, 1);

  flags = gst_byte_reader_get_uint8_unchecked (&br);
  /* No cc_data? */
  if ((flags & 0x40) == 0) {
    GST_DEBUG_OBJECT (self, "cdp packet does have any cc_data");
    return 0;
  }

  /* cdp_hdr_sequence_cntr */
  gst_byte_reader_skip_unchecked (&br, 2);

  /* skip timecode */
  if (flags & 0x80) {
    if (gst_byte_reader_get_remaining (&br) < 5) {
      GST_WARNING_OBJECT (self, "cdp packet does not have enough data to "
          "contain a timecode (%u). Need at least 5 bytes",
          gst_byte_reader_get_remaining (&br));
      return 0;
    }

    gst_byte_reader_skip_unchecked (&br, 5);
  }

  /* ccdata_present */
  if (flags & 0x40) {
    guint8 cc_count;

    if (gst_byte_reader_get_remaining (&br) < 2) {
      GST_WARNING_OBJECT (self, "not enough data to contain valid cc_data");
      return 0;
    }

    u8 = gst_byte_reader_get_uint8_unchecked (&br);
    if (u8 != 0x72) {
      GST_WARNING_OBJECT (self, "missing cc_data start code of 0x72, "
          "found 0x%02x", u8);
      return 0;
    }

    cc_count = gst_byte_reader_get_uint8_unchecked (&br);
    if ((cc_count & 0xe0) != 0xe0) {
      GST_WARNING_OBJECT (self, "reserved bits are not 0xe0, found 0x%02x", u8);
      return 0;
    }
    cc_count &= 0x1f;

    len = 3 * cc_count;
    if (gst_byte_reader_get_remaining (&br) < len) {
      GST_WARNING_OBJECT (self, "not enough bytes (%u) left for the "
          "number of byte triples (%u)", gst_byte_reader_get_remaining (&br),
          cc_count);
      return 0;
    }

    *pos = gst_byte_reader_get_pos (&br);
  }

  /* skip everything else we don't care about */
  return len;
}

static void
gst_dwrite_text_overlay_decode_cc_data (GstDWriteTextOverlay * self,
    const guint8 * data, guint len, GstClockTime running_time)
{
  GstDWriteTextOverlayPrivate *priv = self->priv;
  GstByteReader br;

  GST_TRACE_OBJECT (self, "Decoding CC data");

  gst_byte_reader_init (&br, data, len);
  while (gst_byte_reader_get_remaining (&br) >= 3) {
    guint8 cc_type;
    guint16 cc_data;

    cc_type = gst_byte_reader_get_uint8_unchecked (&br);
    cc_data = gst_byte_reader_get_uint16_be_unchecked (&br);

    if ((cc_type & 0x04) != 0x04)
      continue;

    cc_type = cc_type & 0x03;
    if (cc_type != 0x00 && cc_type != 0x01)
      continue;

    if (priv->selected_field == 0xff) {
      GST_INFO_OBJECT (self, "Selected field %d", cc_type);
      priv->selected_field = cc_type;
    }

    if (cc_type != priv->selected_field)
      continue;

    auto status = caption_frame_decode (&priv->frame, cc_data, 0.0);
    switch (status) {
      case LIBCAPTION_READY:
      {
        auto len = caption_frame_to_text (&priv->frame,
            &priv->closed_caption[0], FALSE);
        priv->closed_caption.resize (len);
        break;
      }
      case LIBCAPTION_CLEAR:
        priv->closed_caption.clear ();
        break;
      default:
        break;
    }

    priv->caption_running_time = running_time;
  }
}

static void
gst_dwrite_text_overlay_decode_s334_1a (GstDWriteTextOverlay * self,
    const guint8 * data, guint len, GstClockTime running_time)
{
  GstDWriteTextOverlayPrivate *priv = self->priv;
  GstByteReader br;

  GST_TRACE_OBJECT (self, "Decoding S334-1A");

  gst_byte_reader_init (&br, data, len);
  while (gst_byte_reader_get_remaining (&br) >= 3) {
    guint8 cc_type;
    guint16 cc_data;

    cc_type = gst_byte_reader_get_uint8_unchecked (&br);
    cc_data = gst_byte_reader_get_uint16_be_unchecked (&br);

    cc_type = cc_type & 0x01;
    if (priv->selected_field == 0xff) {
      GST_INFO_OBJECT (self, "Selected field %d", cc_type);
      priv->selected_field = cc_type;
    }

    if (cc_type != priv->selected_field)
      continue;

    auto status = caption_frame_decode (&priv->frame, cc_data, 0.0);
    switch (status) {
      case LIBCAPTION_READY:
      {
        auto len = caption_frame_to_text (&priv->frame,
            &priv->closed_caption[0], FALSE);
        priv->closed_caption.resize (len);
        break;
      }
      case LIBCAPTION_CLEAR:
        priv->closed_caption.clear ();
        break;
      default:
        break;
    }

    priv->caption_running_time = running_time;
  }
}

static void
gst_dwrite_text_overlay_decode_raw (GstDWriteTextOverlay * self,
    const guint8 * data, guint len, GstClockTime running_time)
{
  GstDWriteTextOverlayPrivate *priv = self->priv;
  GstByteReader br;

  GST_TRACE_OBJECT (self, "Decoding CEA608 RAW");

  gst_byte_reader_init (&br, data, len);
  while (gst_byte_reader_get_remaining (&br) >= 2) {
    guint16 cc_data;

    cc_data = gst_byte_reader_get_uint16_be_unchecked (&br);

    auto status = caption_frame_decode (&priv->frame, cc_data, 0.0);
    switch (status) {
      case LIBCAPTION_READY:
      {
        auto len = caption_frame_to_text (&priv->frame,
            &priv->closed_caption[0], FALSE);
        priv->closed_caption.resize (len);
        break;
      }
      case LIBCAPTION_CLEAR:
        priv->closed_caption.clear ();
        break;
      default:
        break;
    }

    priv->caption_running_time = running_time;
  }
}

static void
xml_text (GMarkupParseContext * context, const gchar * text, gsize text_len,
    gpointer user_data, GError ** error)
{
  gchar **accum = (gchar **) user_data;
  gchar *concat;

  if (*accum) {
    concat = g_strconcat (*accum, text, NULL);
    g_free (*accum);
    *accum = concat;
  } else {
    *accum = g_strdup (text);
  }
}

static gchar *
gst_dwrite_text_overlay_strip_markup (GstDWriteTextOverlay * self,
    const gchar * markup)
{
  GMarkupParser parser = { 0, };
  GMarkupParseContext *context;
  gchar *accum = nullptr;

  parser.text = xml_text;
  context = g_markup_parse_context_new (&parser,
      (GMarkupParseFlags) 0, &accum, nullptr);

  if (!g_markup_parse_context_parse (context, "<root>", 6, nullptr))
    goto error;

  if (!g_markup_parse_context_parse (context, markup, strlen (markup), nullptr))
    goto error;

  if (!g_markup_parse_context_parse (context, "</root>", 7, nullptr))
    goto error;

  if (!g_markup_parse_context_end_parse (context, nullptr))
    goto error;

done:
  g_markup_parse_context_free (context);
  return accum;

error:
  g_free (accum);
  accum = nullptr;
  goto done;
}

static void
gst_dwrite_text_overlay_extract_meta (GstDWriteTextOverlay * self,
    GstDWriteSubtitleMeta * meta)
{
  GstDWriteTextOverlayPrivate *priv = self->priv;
  GstCaps *caps = nullptr;
  GstStructure *s;
  const gchar *format;
  std::string str;
  GstMapInfo info;

  if (!meta || !meta->subtitle || !meta->stream)
    return;

  caps = gst_stream_get_caps (meta->stream);
  if (!caps)
    return;

  if (gst_buffer_get_size (meta->subtitle) == 0)
    goto out;

  if (!gst_buffer_map (meta->subtitle, &info, GST_MAP_READ))
    goto out;

  s = gst_caps_get_structure (caps, 0);
  format = gst_structure_get_string (s, "format");
  /* TODO: parse pango attributs and make layout based on that */
  if (g_strcmp0 (format, "pango-markup") == 0) {
    gchar *stripped = gst_dwrite_text_overlay_strip_markup (self,
        (gchar *) info.data);
    gst_buffer_unmap (meta->subtitle, &info);

    if (!stripped)
      goto out;

    if (priv->text.empty ()) {
      priv->text = stripped;
    } else {
      priv->text += "\n";
      priv->text += stripped;
    }
  } else {
    std::string ret;
    ret.resize (info.size);
    memcpy (&ret[0], info.data, info.size);
    gst_buffer_unmap (meta->subtitle, &info);
    auto len = strlen (ret.c_str ());
    ret.resize (len);

    if (priv->text.empty ())
      priv->text = ret;
    else
      priv->text += " " + ret;
  }

out:
  gst_clear_caps (&caps);
}

static gboolean
gst_dwrite_text_overlay_foreach_meta (GstBuffer * buffer, GstMeta ** meta,
    GstDWriteTextOverlay * self)
{
  GstDWriteTextOverlayPrivate *priv = self->priv;
  GstVideoCaptionMeta *cc_meta;

  if (priv->enable_cc && (*meta)->info->api == GST_VIDEO_CAPTION_META_API_TYPE) {
    cc_meta = (GstVideoCaptionMeta *) (*meta);
    switch (cc_meta->caption_type) {
      case GST_VIDEO_CAPTION_TYPE_CEA608_RAW:
        gst_dwrite_text_overlay_decode_raw (self, cc_meta->data, cc_meta->size,
            priv->running_time);
        break;
      case GST_VIDEO_CAPTION_TYPE_CEA608_S334_1A:
        gst_dwrite_text_overlay_decode_s334_1a (self, cc_meta->data,
            cc_meta->size, priv->running_time);
        break;
      case GST_VIDEO_CAPTION_TYPE_CEA708_RAW:
        gst_dwrite_text_overlay_decode_cc_data (self, cc_meta->data,
            cc_meta->size, priv->running_time);
        break;
      case GST_VIDEO_CAPTION_TYPE_CEA708_CDP:
      {
        guint len, pos = 0;
        len = gst_dwrite_text_overlay_extract_cdp (self, cc_meta->data,
            cc_meta->size, &pos);
        if (len > 0) {
          gst_dwrite_text_overlay_decode_cc_data (self, cc_meta->data + pos,
              len, priv->running_time);
        }
        break;
      }
      default:
        break;
    }
  } else if ((*meta)->info->api == GST_DWRITE_SUBTITLE_META_API_TYPE) {
    GstDWriteSubtitleMeta *smeta = (GstDWriteSubtitleMeta *) (*meta);
    gst_dwrite_text_overlay_extract_meta (self, smeta);
  }

  return TRUE;
}

static WString
gst_dwrite_text_overlay_get_text (GstDWriteBaseOverlay * overlay,
    const WString & default_text, GstBuffer * buffer)
{
  GstBaseTransform *trans = GST_BASE_TRANSFORM (overlay);
  GstDWriteTextOverlay *self = GST_DWRITE_TEXT_OVERLAY (overlay);
  GstDWriteTextOverlayPrivate *priv = self->priv;
  std::lock_guard < std::mutex > lk (priv->lock);
  WString text_wide;

  priv->text.clear ();

  priv->running_time = gst_segment_to_running_time (&trans->segment,
      GST_FORMAT_TIME, GST_BUFFER_PTS (buffer));

  gst_buffer_foreach_meta (buffer,
      (GstBufferForeachMetaFunc) gst_dwrite_text_overlay_foreach_meta, self);

  if (priv->enable_cc) {
    if (GST_CLOCK_TIME_IS_VALID (priv->timeout) &&
        GST_CLOCK_TIME_IS_VALID (priv->running_time) &&
        GST_CLOCK_TIME_IS_VALID (priv->caption_running_time) &&
        priv->running_time >= priv->caption_running_time) {
      GstClockTime diff = priv->running_time - priv->caption_running_time;

      if (diff > priv->timeout) {
        GST_INFO_OBJECT (self, "Reached timeout, clearing text");
        priv->closed_caption.clear ();
      }
    }
  } else {
    priv->closed_caption.clear ();
  }

  if (priv->closed_caption.empty () && priv->text.empty ())
    return default_text;

  if (!priv->text.empty ())
    text_wide = gst_dwrite_string_to_wstring (priv->text);

  if (!priv->closed_caption.empty ()) {
    if (!text_wide.empty ())
      text_wide += L"\n";

    text_wide += gst_dwrite_string_to_wstring (priv->closed_caption);
  }

  if (default_text.empty ())
    return text_wide;

  return default_text + WString (L" ") + text_wide;
}

static gboolean
gst_dwrite_text_overlay_remove_meta (GstBuffer * buffer, GstMeta ** meta,
    GstDWriteTextOverlay * self)
{
  GstDWriteTextOverlayPrivate *priv = self->priv;

  if ((*meta)->info->api == GST_VIDEO_CAPTION_META_API_TYPE &&
      priv->enable_cc && priv->remove_caption_meta) {
    GST_TRACE_OBJECT (self, "Removing caption meta");
    *meta = nullptr;
  } else if ((*meta)->info->api == GST_DWRITE_SUBTITLE_META_API_TYPE) {
    *meta = nullptr;
  }

  return TRUE;
}

static void
gst_dwrite_text_overlay_after_transform (GstDWriteBaseOverlay * overlay,
    GstBuffer * buffer)
{
  GstDWriteTextOverlay *self = GST_DWRITE_TEXT_OVERLAY (overlay);
  GstDWriteTextOverlayPrivate *priv = self->priv;
  std::lock_guard < std::mutex > lk (priv->lock);

  gst_buffer_foreach_meta (buffer,
      (GstBufferForeachMetaFunc) gst_dwrite_text_overlay_remove_meta, self);
}

void
gst_dwrite_text_overlay_build_param_specs (std::vector < GParamSpec * >&pspec)
{
  pspec.push_back (g_param_spec_boolean ("enable-cc", "Enable CC",
          "Enable closed caption rendering",
          DEFAULT_ENABLE_CC,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  pspec.push_back (g_param_spec_int ("cc-field", "CC Field",
          "The closed caption field to render when available, (-1 = automatic)",
          -1, 1, DEFAULT_CC_FIELD,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  pspec.push_back (g_param_spec_uint64 ("cc-timeout", "CC Timeout",
          "Duration after which to erase overlay when no cc data has arrived "
          "for the selected field, in nanoseconds unit", 16 * GST_SECOND,
          GST_CLOCK_TIME_NONE, DEFAULT_CC_TIMEOUT,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  pspec.push_back (g_param_spec_boolean ("remove-cc-meta", "Remove CC Meta",
          "Remove caption meta from output buffers "
          "when closed caption rendering is enabled",
          DEFAULT_REMOVE_CC_META,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
}
