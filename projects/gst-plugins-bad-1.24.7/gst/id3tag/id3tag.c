/* GStreamer ID3v2 tag writer
 *
 * Copyright (C) 2006 Christophe Fergeau <teuf@gnome.org>
 * Copyright (C) 2006-2009 Tim-Philipp Müller <tim centricular net>
 * Copyright (C) 2009 Pioneers of the Inevitable <songbird@songbirdnest.com>
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

#include "id3tag.h"
#include <string.h>

#include <gst/tag/tag.h>

GST_DEBUG_CATEGORY_EXTERN (gst_id3_mux_debug);
#define GST_CAT_DEFAULT gst_id3_mux_debug

#define ID3V2_APIC_PICTURE_OTHER 0
#define ID3V2_APIC_PICTURE_FILE_ICON 1

/* ======================================================================== */

typedef GString GstByteWriter;

static inline GstByteWriter *
gst_byte_writer_new (guint size)
{
  return (GstByteWriter *) g_string_sized_new (size);
}

static inline guint
gst_byte_writer_get_length (GstByteWriter * w)
{
  return ((GString *) w)->len;
}

static inline void
gst_byte_writer_write_bytes (GstByteWriter * w, const guint8 * data, guint len)
{
  g_string_append_len ((GString *) w, (const gchar *) data, len);
}

static inline void
gst_byte_writer_write_uint8 (GstByteWriter * w, guint8 val)
{
  guint8 data[1];

  GST_WRITE_UINT8 (data, val);
  gst_byte_writer_write_bytes (w, data, 1);
}

static inline void
gst_byte_writer_write_uint16 (GstByteWriter * w, guint16 val)
{
  guint8 data[2];

  GST_WRITE_UINT16_BE (data, val);
  gst_byte_writer_write_bytes (w, data, 2);
}

static inline void
gst_byte_writer_write_uint32 (GstByteWriter * w, guint32 val)
{
  guint8 data[4];

  GST_WRITE_UINT32_BE (data, val);
  gst_byte_writer_write_bytes (w, data, 4);
}

static inline void
gst_byte_writer_write_uint32_syncsafe (GstByteWriter * w, guint32 val)
{
  guint8 data[4];

  data[0] = (guint8) ((val >> 21) & 0x7f);
  data[1] = (guint8) ((val >> 14) & 0x7f);
  data[2] = (guint8) ((val >> 7) & 0x7f);
  data[3] = (guint8) ((val >> 0) & 0x7f);
  gst_byte_writer_write_bytes (w, data, 4);
}

static void
gst_byte_writer_copy_bytes (GstByteWriter * w, guint8 * dest, guint offset,
    gint size)
{
  guint length;

  length = gst_byte_writer_get_length (w);

  if (size == -1)
    size = length - offset;

  g_warn_if_fail (length >= (offset + size));

  memcpy (dest, w->str + offset, MIN (size, length - offset));
}

static inline void
gst_byte_writer_free (GstByteWriter * w)
{
  g_string_free (w, TRUE);
}

/* ======================================================================== */

/*
typedef enum {
  GST_ID3V2_FRAME_FLAG_NONE = 0,
  GST_ID3V2_FRAME_FLAG_
} GstID3v2FrameMsgFlags;
*/

typedef struct
{
  gchar id[5];
  guint32 len;                  /* Length encoded in the header; this is the
                                   total length - header size */
  guint16 flags;
  GstByteWriter *writer;
  gboolean dirty;               /* TRUE if frame header needs updating */
} GstId3v2Frame;

typedef struct
{
  GArray *frames;
  guint major_version;          /* The 3 in v2.3.0 */
} GstId3v2Tag;

typedef void (*GstId3v2AddTagFunc) (GstId3v2Tag * tag, const GstTagList * list,
    const gchar * gst_tag, guint num_tags, const gchar * data);

#define ID3V2_ENCODING_ISO_8859_1    0x00
#define ID3V2_ENCODING_UTF16_BOM     0x01
#define ID3V2_ENCODING_UTF8          0x03

static gboolean id3v2_tag_init (GstId3v2Tag * tag, guint major_version);
static void id3v2_tag_unset (GstId3v2Tag * tag);

static void id3v2_frame_init (GstId3v2Frame * frame,
    const gchar * frame_id, guint16 flags);
static void id3v2_frame_unset (GstId3v2Frame * frame);
static void id3v2_frame_finish (GstId3v2Tag * tag, GstId3v2Frame * frame);
static guint id3v2_frame_get_size (GstId3v2Tag * tag, GstId3v2Frame * frame);

static void id3v2_tag_add_text_frame (GstId3v2Tag * tag,
    const gchar * frame_id, const gchar ** strings, int num_strings);
static void id3v2_tag_add_simple_text_frame (GstId3v2Tag * tag,
    const gchar * frame_id, const gchar * string);

static gboolean
id3v2_tag_init (GstId3v2Tag * tag, guint major_version)
{
  if (major_version != 3 && major_version != 4)
    return FALSE;

  tag->major_version = major_version;
  tag->frames = g_array_new (TRUE, TRUE, sizeof (GstId3v2Frame));

  return TRUE;
}

static void
id3v2_tag_unset (GstId3v2Tag * tag)
{
  guint i;

  for (i = 0; i < tag->frames->len; ++i)
    id3v2_frame_unset (&g_array_index (tag->frames, GstId3v2Frame, i));

  g_array_free (tag->frames, TRUE);
  memset (tag, 0, sizeof (GstId3v2Tag));
}

#ifndef GST_ROUND_UP_1024
#define GST_ROUND_UP_1024(num) (((num)+1023)&~1023)
#endif

static GstBuffer *
id3v2_tag_to_buffer (GstId3v2Tag * tag)
{
  GstByteWriter *w;
  GstMapInfo info;
  GstBuffer *buf;
  guint8 *dest;
  guint i, size, offset, size_frames = 0;

  GST_DEBUG ("Creating buffer for ID3v2 tag containing %d frames",
      tag->frames->len);

  for (i = 0; i < tag->frames->len; ++i) {
    GstId3v2Frame *frame = &g_array_index (tag->frames, GstId3v2Frame, i);

    id3v2_frame_finish (tag, frame);
    size_frames += id3v2_frame_get_size (tag, frame);
  }

  size = GST_ROUND_UP_1024 (10 + size_frames);

  w = gst_byte_writer_new (10);
  gst_byte_writer_write_uint8 (w, 'I');
  gst_byte_writer_write_uint8 (w, 'D');
  gst_byte_writer_write_uint8 (w, '3');
  gst_byte_writer_write_uint8 (w, tag->major_version);
  gst_byte_writer_write_uint8 (w, 0);   /* micro version */
  gst_byte_writer_write_uint8 (w, 0);   /* flags */
  gst_byte_writer_write_uint32_syncsafe (w, size - 10);

  buf = gst_buffer_new_allocate (NULL, size, NULL);
  gst_buffer_map (buf, &info, GST_MAP_WRITE);
  dest = info.data;
  gst_byte_writer_copy_bytes (w, dest, 0, 10);
  offset = 10;

  for (i = 0; i < tag->frames->len; ++i) {
    GstId3v2Frame *frame = &g_array_index (tag->frames, GstId3v2Frame, i);

    gst_byte_writer_copy_bytes (frame->writer, dest + offset, 0, -1);
    offset += id3v2_frame_get_size (tag, frame);
  }

  /* Zero out any additional space in our buffer as padding. */
  memset (dest + offset, 0, size - offset);

  gst_byte_writer_free (w);
  gst_buffer_unmap (buf, &info);

  return buf;
}

static inline void
id3v2_frame_write_bytes (GstId3v2Frame * frame, const guint8 * data, guint len)
{
  gst_byte_writer_write_bytes (frame->writer, data, len);
  frame->dirty = TRUE;
}

static inline void
id3v2_frame_write_uint8 (GstId3v2Frame * frame, guint8 val)
{
  gst_byte_writer_write_uint8 (frame->writer, val);
  frame->dirty = TRUE;
}

static inline void
id3v2_frame_write_uint16 (GstId3v2Frame * frame, guint16 val)
{
  gst_byte_writer_write_uint16 (frame->writer, val);
  frame->dirty = TRUE;
}

static inline void
id3v2_frame_write_uint32 (GstId3v2Frame * frame, guint32 val)
{
  gst_byte_writer_write_uint32 (frame->writer, val);
  frame->dirty = TRUE;
}

static void
id3v2_frame_init (GstId3v2Frame * frame, const gchar * frame_id, guint16 flags)
{
  g_assert (strlen (frame_id) == 4);    /* we only handle 2.3.0/2.4.0 */
  memcpy (frame->id, frame_id, 4 + 1);
  frame->flags = flags;
  frame->len = 0;
  frame->writer = gst_byte_writer_new (64);
  id3v2_frame_write_bytes (frame, (const guint8 *) frame->id, 4);
  id3v2_frame_write_uint32 (frame, 0);  /* size, set later */
  id3v2_frame_write_uint16 (frame, frame->flags);
}

static void
id3v2_frame_finish (GstId3v2Tag * tag, GstId3v2Frame * frame)
{
  if (frame->dirty) {
    frame->len = frame->writer->len - 10;
    GST_LOG ("[%s] %u bytes", frame->id, frame->len);
    if (tag->major_version == 3) {
      GST_WRITE_UINT32_BE (frame->writer->str + 4, frame->len);
    } else {
      /* Version 4 uses a syncsafe int here */
      GST_WRITE_UINT8 (frame->writer->str + 4, (frame->len >> 21) & 0x7f);
      GST_WRITE_UINT8 (frame->writer->str + 5, (frame->len >> 14) & 0x7f);
      GST_WRITE_UINT8 (frame->writer->str + 6, (frame->len >> 7) & 0x7f);
      GST_WRITE_UINT8 (frame->writer->str + 7, (frame->len >> 0) & 0x7f);
    }
    frame->dirty = FALSE;
  }
}

static guint
id3v2_frame_get_size (GstId3v2Tag * tag, GstId3v2Frame * frame)
{
  id3v2_frame_finish (tag, frame);
  return gst_byte_writer_get_length (frame->writer);
}

static void
id3v2_frame_unset (GstId3v2Frame * frame)
{
  gst_byte_writer_free (frame->writer);
  memset (frame, 0, sizeof (GstId3v2Frame));
}

static gboolean
id3v2_string_is_ascii (const gchar * string)
{
  while (*string) {
    if (!g_ascii_isprint (*string++))
      return FALSE;
  }

  return TRUE;
}

static int
id3v2_tag_string_encoding (GstId3v2Tag * tag, const gchar * string)
{
  int encoding;
  if (tag->major_version == 4) {
    /* ID3v2.4 supports UTF8, use it unconditionally as it's really the only
       sensible encoding. */
    encoding = ID3V2_ENCODING_UTF8;
  } else {
    /* If we're not writing v2.4, then check to see if it's ASCII.
       If it is, write ISO-8859-1 (compatible with ASCII).
       Otherwise, write UTF-16-LE with a byte order marker.
       Note that we don't write arbitrary ISO-8859-1 as ISO-8859-1, because much
       software misuses this - and non-ASCII might confuse it. */
    if (id3v2_string_is_ascii (string))
      encoding = ID3V2_ENCODING_ISO_8859_1;
    else
      encoding = ID3V2_ENCODING_UTF16_BOM;
  }

  return encoding;
}

static void
id3v2_frame_write_string (GstId3v2Frame * frame, int encoding,
    const gchar * string, gboolean null_terminate)
{
  int terminator_length;
  if (encoding == ID3V2_ENCODING_UTF16_BOM) {
    gsize utf16len;
    const guint8 bom[] = { 0xFF, 0xFE };
    /* This converts to little-endian UTF-16 */
    gchar *utf16 = g_convert (string, -1, "UTF-16LE", "UTF-8",
        NULL, &utf16len, NULL);
    if (!utf16) {
      GST_WARNING ("Failed to convert UTF-8 to UTF-16LE");
      return;
    }

    /* Write the BOM */
    id3v2_frame_write_bytes (frame, (const guint8 *) bom, 2);
    id3v2_frame_write_bytes (frame, (const guint8 *) utf16, utf16len);
    if (null_terminate) {
      /* NUL terminator is 2 bytes, if present. */
      id3v2_frame_write_uint16 (frame, 0);
    }

    g_free (utf16);
  } else {
    /* write NUL terminator as well if requested */
    terminator_length = null_terminate ? 1 : 0;
    id3v2_frame_write_bytes (frame, (const guint8 *) string,
        strlen (string) + terminator_length);
  }
}

static void
id3v2_tag_add_text_frame (GstId3v2Tag * tag, const gchar * frame_id,
    const gchar ** strings_utf8, int num_strings)
{
  GstId3v2Frame frame;
  guint len, i;
  int encoding;

  if (num_strings < 1 || strings_utf8 == NULL || strings_utf8[0] == NULL) {
    GST_LOG ("Not adding text frame, no strings");
    return;
  }

  id3v2_frame_init (&frame, frame_id, 0);

  encoding = id3v2_tag_string_encoding (tag, strings_utf8[0]);
  id3v2_frame_write_uint8 (&frame, encoding);

  GST_LOG ("Adding text frame %s with %d strings", frame_id, num_strings);

  for (i = 0; i < num_strings; ++i) {
    len = strlen (strings_utf8[i]);
    g_return_if_fail (g_utf8_validate (strings_utf8[i], len, NULL));

    id3v2_frame_write_string (&frame, encoding, strings_utf8[i],
        i != num_strings - 1);

    /* only v2.4.0 supports multiple strings per frame (according to the
     * earlier specs tag readers should just ignore everything after the first
     * string, but we probably shouldn't write anything there, just in case
     * tag readers that only support the old version are not expecting
     * more data after the first string) */
    if (tag->major_version < 4)
      break;
  }

  if (i < num_strings - 1) {
    GST_WARNING ("Only wrote one of multiple string values for text frame %s "
        "- ID3v2 supports multiple string values only since v2.4.0, but writing"
        "v2.%u.0 tag", frame_id, tag->major_version);
  }

  g_array_append_val (tag->frames, frame);
}

static void
id3v2_tag_add_simple_text_frame (GstId3v2Tag * tag, const gchar * frame_id,
    const gchar * string)
{
  id3v2_tag_add_text_frame (tag, frame_id, (const gchar **) &string, 1);
}

/* ====================================================================== */

static void
add_text_tag (GstId3v2Tag * id3v2tag, const GstTagList * list,
    const gchar * tag, guint num_tags, const gchar * frame_id)
{
  const gchar **strings;
  guint n, i;

  GST_LOG ("Adding '%s' frame", frame_id);

  strings = g_new0 (const gchar *, num_tags + 1);
  for (n = 0, i = 0; n < num_tags; ++n) {
    if (gst_tag_list_peek_string_index (list, tag, n, &strings[i]) &&
        strings[i] != NULL) {
      GST_LOG ("%s: %s[%u] = '%s'", frame_id, tag, i, strings[i]);
      ++i;
    }
  }

  if (strings[0] != NULL) {
    id3v2_tag_add_text_frame (id3v2tag, frame_id, strings, i);
  } else {
    GST_WARNING ("Empty list for tag %s, skipping", tag);
  }

  g_free ((gchar **) strings);
}

static void
add_private_data_tag (GstId3v2Tag * id3v2tag, const GstTagList * list,
    const gchar * tag, guint num_tags, const gchar * frame_id)
{
  gint n;

  for (n = 0; n < num_tags; ++n) {
    GstId3v2Frame frame;
    GstSample *sample = NULL;
    const GstStructure *structure = NULL;
    GstBuffer *binary = NULL;
    GstBuffer *priv_frame = NULL;
    const gchar *owner_str = NULL;
    guint owner_len = 0;
    GstMapInfo mapinfo;

    if (!gst_tag_list_get_sample_index (list, tag, n, &sample))
      continue;

    structure = gst_sample_get_info (sample);
    if (structure != NULL
        && !strcmp (gst_structure_get_name (structure), "ID3PrivateFrame")) {
      owner_str = gst_structure_get_string (structure, "owner");

      if (owner_str != NULL) {
        owner_len = strlen (owner_str) + 1;
        priv_frame = gst_buffer_new_and_alloc (owner_len);
        gst_buffer_fill (priv_frame, 0, owner_str, owner_len);

        binary = gst_buffer_ref (gst_sample_get_buffer (sample));
        priv_frame = gst_buffer_append (priv_frame, binary);

        id3v2_frame_init (&frame, frame_id, 0);

        if (gst_buffer_map (priv_frame, &mapinfo, GST_MAP_READ)) {
          id3v2_frame_write_bytes (&frame, mapinfo.data, mapinfo.size);
          g_array_append_val (id3v2tag->frames, frame);
          gst_buffer_unmap (priv_frame, &mapinfo);
        } else {
          GST_WARNING ("Couldn't map priv frame tag buffer");
          id3v2_frame_unset (&frame);
        }

        gst_buffer_unref (priv_frame);
        gst_sample_unref (sample);
      }
    } else {
      GST_WARNING ("Couldn't find ID3PrivateFrame structure");
    }
  }
}

static void
add_id3v2frame_tag (GstId3v2Tag * id3v2tag, const GstTagList * list,
    const gchar * tag, guint num_tags, const gchar * unused)
{
  guint i;

  for (i = 0; i < num_tags; ++i) {
    GstSample *sample;
    GstBuffer *buf;
    GstCaps *caps;

    if (!gst_tag_list_get_sample_index (list, tag, i, &sample))
      continue;

    buf = gst_sample_get_buffer (sample);

    /* FIXME: should use auxiliary sample struct instead of caps for this */
    caps = gst_sample_get_caps (sample);

    if (buf && caps) {
      GstStructure *s;
      gint version = 0;

      s = gst_caps_get_structure (caps, 0);
      /* We can only add it if this private buffer is for the same ID3 version,
         because we don't understand the contents at all. */
      if (s && gst_structure_get_int (s, "version", &version) &&
          version == id3v2tag->major_version) {
        GstId3v2Frame frame;
        GstMapInfo mapinfo;
        gchar frame_id[5];
        guint16 flags;
        guint8 *data;
        gint size;

        if (!gst_buffer_map (buf, &mapinfo, GST_MAP_READ)) {
          gst_sample_unref (sample);
          continue;
        }

        size = mapinfo.size;
        data = mapinfo.data;

        if (size >= 10) {       /* header size */
          /* We only get here if the frame version matches the muxer. Since the
           * muxer only does v2.3 or v2.4, the frame must be one of those - and
           * so the frame header is the same format */
          memcpy (frame_id, data, 4);
          frame_id[4] = 0;
          flags = GST_READ_UINT16_BE (data + 8);

          id3v2_frame_init (&frame, frame_id, flags);
          id3v2_frame_write_bytes (&frame, data + 10, size - 10);

          g_array_append_val (id3v2tag->frames, frame);
          GST_DEBUG ("Added unparsed tag with %d bytes", size);
          gst_buffer_unmap (buf, &mapinfo);
        } else {
          GST_WARNING ("Short ID3v2 frame");
        }
      } else {
        GST_WARNING ("Discarding unrecognised ID3 tag for different ID3 "
            "version");
      }
    }
    gst_sample_unref (sample);
  }
}

static void
add_text_tag_v4 (GstId3v2Tag * id3v2tag, const GstTagList * list,
    const gchar * tag, guint num_tags, const gchar * frame_id)
{
  if (id3v2tag->major_version == 4)
    add_text_tag (id3v2tag, list, tag, num_tags, frame_id);
  else {
    GST_WARNING ("Cannot serialise tag '%s' in ID3v2.%d", frame_id,
        id3v2tag->major_version);
  }
}

static void
add_count_or_num_tag (GstId3v2Tag * id3v2tag, const GstTagList * list,
    const gchar * tag, guint num_tags, const gchar * frame_id)
{
  static const struct
  {
    const gchar *gst_tag;
    const gchar *corr_count;    /* corresponding COUNT tag (if number) */
    const gchar *corr_num;      /* corresponding NUMBER tag (if count) */
  } corr[] = {
    {
        GST_TAG_TRACK_NUMBER, GST_TAG_TRACK_COUNT, NULL}, {
        GST_TAG_TRACK_COUNT, NULL, GST_TAG_TRACK_NUMBER}, {
        GST_TAG_ALBUM_VOLUME_NUMBER, GST_TAG_ALBUM_VOLUME_COUNT, NULL}, {
        GST_TAG_ALBUM_VOLUME_COUNT, NULL, GST_TAG_ALBUM_VOLUME_NUMBER}
  };
  guint idx;

  for (idx = 0; idx < G_N_ELEMENTS (corr); ++idx) {
    if (strcmp (corr[idx].gst_tag, tag) == 0)
      break;
  }

  g_assert (idx < G_N_ELEMENTS (corr));
  g_assert (frame_id && strlen (frame_id) == 4);

  if (corr[idx].corr_num == NULL) {
    guint number;

    /* number tag */
    if (gst_tag_list_get_uint_index (list, tag, 0, &number)) {
      gchar *tag_str;
      guint count;

      if (gst_tag_list_get_uint_index (list, corr[idx].corr_count, 0, &count))
        tag_str = g_strdup_printf ("%u/%u", number, count);
      else
        tag_str = g_strdup_printf ("%u", number);

      GST_DEBUG ("Setting %s to %s (frame_id = %s)", tag, tag_str, frame_id);

      id3v2_tag_add_simple_text_frame (id3v2tag, frame_id, tag_str);
      g_free (tag_str);
    }
  } else if (corr[idx].corr_count == NULL) {
    guint count;

    /* count tag */
    if (gst_tag_list_get_uint_index (list, corr[idx].corr_num, 0, &count)) {
      GST_DEBUG ("%s handled with %s, skipping", tag, corr[idx].corr_num);
    } else if (gst_tag_list_get_uint_index (list, tag, 0, &count)) {
      gchar *tag_str = g_strdup_printf ("0/%u", count);
      GST_DEBUG ("Setting %s to %s (frame_id = %s)", tag, tag_str, frame_id);

      id3v2_tag_add_simple_text_frame (id3v2tag, frame_id, tag_str);
      g_free (tag_str);
    }
  }

  if (num_tags > 1) {
    GST_WARNING ("more than one %s, can only handle one", tag);
  }
}

static void
add_bpm_tag (GstId3v2Tag * id3v2tag, const GstTagList * list,
    const gchar * tag, guint num_tags, const gchar * unused)
{
  gdouble bpm;

  GST_LOG ("Adding BPM frame");

  if (gst_tag_list_get_double (list, tag, &bpm)) {
    gchar *tag_str;

    /* bpm is stored as an integer in id3 tags, but is a double in
     * tag lists.
     */
    tag_str = g_strdup_printf ("%u", (guint) bpm);
    GST_DEBUG ("Setting %s to %s", tag, tag_str);
    id3v2_tag_add_simple_text_frame (id3v2tag, "TBPM", tag_str);
    g_free (tag_str);
  }

  if (num_tags > 1) {
    GST_WARNING ("more than one %s, can only handle one", tag);
  }
}

static void
add_comment_tag (GstId3v2Tag * id3v2tag, const GstTagList * list,
    const gchar * tag, guint num_tags, const gchar * unused)
{
  guint n;

  GST_LOG ("Adding comment frames");
  for (n = 0; n < num_tags; ++n) {
    const gchar *s = NULL;

    if (gst_tag_list_peek_string_index (list, tag, n, &s) && s != NULL) {
      gchar *desc = NULL, *val = NULL, *lang = NULL;
      int desclen, vallen, encoding1, encoding2, encoding;
      GstId3v2Frame frame;

      id3v2_frame_init (&frame, "COMM", 0);

      if (strcmp (tag, GST_TAG_COMMENT) == 0 ||
          !gst_tag_parse_extended_comment (s, &desc, &lang, &val, TRUE)) {
        /* create dummy description fields */
        desc = g_strdup ("Comment");
        val = g_strdup (s);
      }

      /* If we don't have a valid language, match what taglib does for
         unknown languages */
      if (!lang || strlen (lang) < 3)
        lang = g_strdup ("XXX");

      desclen = strlen (desc);
      g_return_if_fail (g_utf8_validate (desc, desclen, NULL));
      vallen = strlen (val);
      g_return_if_fail (g_utf8_validate (val, vallen, NULL));

      GST_LOG ("%s[%u] = '%s' (%s|%s|%s)", tag, n, s, GST_STR_NULL (desc),
          GST_STR_NULL (lang), GST_STR_NULL (val));

      encoding1 = id3v2_tag_string_encoding (id3v2tag, desc);
      encoding2 = id3v2_tag_string_encoding (id3v2tag, val);
      encoding = MAX (encoding1, encoding2);

      id3v2_frame_write_uint8 (&frame, encoding);
      id3v2_frame_write_bytes (&frame, (const guint8 *) lang, 3);
      /* write description and value */
      id3v2_frame_write_string (&frame, encoding, desc, TRUE);
      id3v2_frame_write_string (&frame, encoding, val, FALSE);

      g_free (lang);
      g_free (desc);
      g_free (val);

      g_array_append_val (id3v2tag->frames, frame);
    }
  }
}

static void
add_image_tag (GstId3v2Tag * id3v2tag, const GstTagList * list,
    const gchar * tag, guint num_tags, const gchar * unused)
{
  guint n;

  for (n = 0; n < num_tags; ++n) {
    GstSample *sample;
    GstBuffer *image;
    GstCaps *caps;

    GST_DEBUG ("image %u/%u", n + 1, num_tags);

    if (!gst_tag_list_get_sample_index (list, tag, n, &sample))
      continue;

    image = gst_sample_get_buffer (sample);
    caps = gst_sample_get_caps (sample);

    if (image != NULL && gst_buffer_get_size (image) > 0 &&
        caps != NULL && !gst_caps_is_empty (caps)) {
      const gchar *mime_type;
      GstStructure *s;

      s = gst_caps_get_structure (caps, 0);
      mime_type = gst_structure_get_name (s);
      if (mime_type != NULL) {
        const gchar *desc = NULL;
        GstId3v2Frame frame;
        GstMapInfo mapinfo;
        int encoding;
        const GstStructure *info_struct;

        info_struct = gst_sample_get_info (sample);
        if (!info_struct
            || !gst_structure_has_name (info_struct, "GstTagImageInfo"))
          info_struct = NULL;

        /* APIC frame specifies "-->" if we're providing a URL to the image
           rather than directly embedding it */
        if (strcmp (mime_type, "text/uri-list") == 0)
          mime_type = "-->";

        GST_DEBUG ("Attaching picture of %" G_GSIZE_FORMAT " bytes and "
            "mime type %s", gst_buffer_get_size (image), mime_type);

        id3v2_frame_init (&frame, "APIC", 0);

        if (info_struct)
          desc = gst_structure_get_string (info_struct, "image-description");
        if (!desc)
          desc = "";
        encoding = id3v2_tag_string_encoding (id3v2tag, desc);
        id3v2_frame_write_uint8 (&frame, encoding);

        id3v2_frame_write_string (&frame, encoding, mime_type, TRUE);

        if (strcmp (tag, GST_TAG_PREVIEW_IMAGE) == 0) {
          id3v2_frame_write_uint8 (&frame, ID3V2_APIC_PICTURE_FILE_ICON);
        } else {
          int image_type = ID3V2_APIC_PICTURE_OTHER;

          if (info_struct) {
            if (gst_structure_get (info_struct, "image-type",
                    GST_TYPE_TAG_IMAGE_TYPE, &image_type, NULL)) {
              if (image_type > 0 && image_type <= 18) {
                image_type += 2;
              } else {
                image_type = ID3V2_APIC_PICTURE_OTHER;
              }
            } else {
              image_type = ID3V2_APIC_PICTURE_OTHER;
            }
          }
          id3v2_frame_write_uint8 (&frame, image_type);
        }

        id3v2_frame_write_string (&frame, encoding, desc, TRUE);

        if (gst_buffer_map (image, &mapinfo, GST_MAP_READ)) {
          id3v2_frame_write_bytes (&frame, mapinfo.data, mapinfo.size);
          g_array_append_val (id3v2tag->frames, frame);
          gst_buffer_unmap (image, &mapinfo);
        } else {
          GST_WARNING ("Couldn't map image tag buffer");
          id3v2_frame_unset (&frame);
        }
      }
    } else {
      GST_WARNING ("no image or caps: %p, caps=%" GST_PTR_FORMAT, image, caps);
    }
    gst_sample_unref (sample);
  }
}

static void
add_musicbrainz_tag (GstId3v2Tag * id3v2tag, const GstTagList * list,
    const gchar * tag, guint num_tags, const gchar * data)
{
  static const struct
  {
    const gchar gst_tag[32];
    const gchar spec_id[32];
    const gchar realworld_id[32];
  } mb_ids[] = {
    {
          GST_TAG_MUSICBRAINZ_ARTISTID, "MusicBrainz Artist Id",
        "musicbrainz_artistid"}, {
          GST_TAG_MUSICBRAINZ_ALBUMID, "MusicBrainz Album Id",
        "musicbrainz_albumid"}, {
          GST_TAG_MUSICBRAINZ_ALBUMARTISTID, "MusicBrainz Album Artist Id",
        "musicbrainz_albumartistid"}, {
          GST_TAG_MUSICBRAINZ_RELEASEGROUPID, "MusicBrainz Release Group Id",
        "musicbrainz_releasegroupid"}, {
        GST_TAG_MUSICBRAINZ_TRMID, "MusicBrainz TRM Id", "musicbrainz_trmid"}, {
          GST_TAG_CDDA_MUSICBRAINZ_DISCID, "MusicBrainz DiscID",
        "musicbrainz_discid"}, {
          /* the following one is more or less made up, there seems to be little
           * evidence that any popular application is actually putting this info
           * into TXXX frames; the first one comes from a musicbrainz wiki 'proposed
           * tags' page, the second one is analogue to the vorbis/ape/flac tag. */
        GST_TAG_CDDA_CDDB_DISCID, "CDDB DiscID", "discid"}, {
          GST_TAG_MUSICBRAINZ_RELEASETRACKID, "MusicBrainz Track Id",
        "musicbrainz_trackid"}
  };
  guint i, idx;

  idx = (guint8) data[0];
  g_assert (idx < G_N_ELEMENTS (mb_ids));

  for (i = 0; i < num_tags; ++i) {
    const gchar *id_str;

    if (gst_tag_list_peek_string_index (list, tag, 0, &id_str) && id_str) {
      /* add two frames, one with the ID the musicbrainz.org spec mentions
       * and one with the ID that applications use in the real world */
      GstId3v2Frame frame1, frame2;
      int encoding;

      GST_DEBUG ("Setting '%s' to '%s'", mb_ids[idx].spec_id, id_str);
      encoding = id3v2_tag_string_encoding (id3v2tag, id_str);

      id3v2_frame_init (&frame1, "TXXX", 0);
      id3v2_frame_write_uint8 (&frame1, encoding);
      id3v2_frame_write_string (&frame1, encoding, mb_ids[idx].spec_id, TRUE);
      id3v2_frame_write_string (&frame1, encoding, id_str, FALSE);
      g_array_append_val (id3v2tag->frames, frame1);

      id3v2_frame_init (&frame2, "TXXX", 0);
      id3v2_frame_write_uint8 (&frame2, encoding);
      id3v2_frame_write_string (&frame2, encoding,
          mb_ids[idx].realworld_id, TRUE);
      id3v2_frame_write_string (&frame2, encoding, id_str, FALSE);
      g_array_append_val (id3v2tag->frames, frame2);
    }
  }
}

static void
add_unique_file_id_tag (GstId3v2Tag * id3v2tag, const GstTagList * list,
    const gchar * tag, guint num_tags, const gchar * unused)
{
  const gchar *origin = "http://musicbrainz.org";
  const gchar *id_str = NULL;

  if (gst_tag_list_peek_string_index (list, tag, 0, &id_str) && id_str) {
    GstId3v2Frame frame;

    GST_LOG ("Adding %s (%s): %s", tag, origin, id_str);

    id3v2_frame_init (&frame, "UFID", 0);
    id3v2_frame_write_bytes (&frame, (const guint8 *) origin,
        strlen (origin) + 1);
    id3v2_frame_write_bytes (&frame, (const guint8 *) id_str,
        strlen (id_str) + 1);
    g_array_append_val (id3v2tag->frames, frame);
  }
}

static void
add_date_tag (GstId3v2Tag * id3v2tag, const GstTagList * list,
    const gchar * tag, guint num_tags, const gchar * unused)
{
  guint n;
  guint i = 0;
  const gchar *frame_id;
  gchar **strings;

  if (id3v2tag->major_version == 3)
    frame_id = "TYER";
  else
    frame_id = "TDRC";

  GST_LOG ("Adding date time frame");

  strings = g_new0 (gchar *, num_tags + 1);
  for (n = 0; n < num_tags; ++n) {
    GstDateTime *dt = NULL;
    guint year;
    gchar *s;

    if (!gst_tag_list_get_date_time_index (list, tag, n, &dt) || dt == NULL)
      continue;

    year = gst_date_time_get_year (dt);
    if (year > 500 && year < 2100) {
      s = g_strdup_printf ("%u", year);
      GST_LOG ("%s[%u] = '%s'", tag, n, s);
      strings[i] = s;
      i++;
    } else {
      GST_WARNING ("invalid year %u, skipping", year);
    }

    if (gst_date_time_has_month (dt)) {
      if (id3v2tag->major_version == 3)
        GST_FIXME ("write TDAT and possibly also TIME frame");
    }
    gst_date_time_unref (dt);
  }

  if (strings[0] != NULL) {
    id3v2_tag_add_text_frame (id3v2tag, frame_id, (const gchar **) strings, i);
  } else {
    GST_WARNING ("Empty list for tag %s, skipping", tag);
  }

  g_strfreev (strings);
}

static void
add_encoder_tag (GstId3v2Tag * id3v2tag, const GstTagList * list,
    const gchar * tag, guint num_tags, const gchar * unused)
{
  guint n;
  gchar **strings;
  int i = 0;

  /* ENCODER_VERSION is either handled with the ENCODER tag or not at all */
  if (strcmp (tag, GST_TAG_ENCODER_VERSION) == 0)
    return;

  strings = g_new0 (gchar *, num_tags + 1);
  for (n = 0; n < num_tags; ++n) {
    const gchar *encoder = NULL;

    if (gst_tag_list_peek_string_index (list, tag, n, &encoder) && encoder) {
      guint encoder_version;
      gchar *s;

      if (gst_tag_list_get_uint_index (list, GST_TAG_ENCODER_VERSION, n,
              &encoder_version) && encoder_version > 0) {
        s = g_strdup_printf ("%s %u", encoder, encoder_version);
      } else {
        s = g_strdup (encoder);
      }

      GST_LOG ("encoder[%u] = '%s'", n, s);
      strings[i] = s;
      i++;
    }
  }

  if (strings[0] != NULL) {
    id3v2_tag_add_text_frame (id3v2tag, "TSSE", (const gchar **) strings, i);
  } else {
    GST_WARNING ("Empty list for tag %s, skipping", tag);
  }

  g_strfreev (strings);
}

static void
add_uri_tag (GstId3v2Tag * id3v2tag, const GstTagList * list,
    const gchar * tag, guint num_tags, const gchar * frame_id)
{
  const gchar *url = NULL;

  g_assert (frame_id != NULL);

  /* URI tags are limited to one of each per taglist */
  if (gst_tag_list_peek_string_index (list, tag, 0, &url) && url != NULL) {
    guint url_len;

    url_len = strlen (url);
    if (url_len > 0 && gst_uri_is_valid (url)) {
      GstId3v2Frame frame;

      id3v2_frame_init (&frame, frame_id, 0);
      id3v2_frame_write_bytes (&frame, (const guint8 *) url, strlen (url) + 1);
      g_array_append_val (id3v2tag->frames, frame);
    } else {
      GST_WARNING ("Tag %s does not contain a valid URI (%s)", tag, url);
    }
  }
}

static void
add_relative_volume_tag (GstId3v2Tag * id3v2tag, const GstTagList * list,
    const gchar * tag, guint num_tags, const gchar * unused)
{
  const char *gain_tag_name;
  const char *peak_tag_name;
  gdouble peak_val;
  gdouble gain_val;
  const char *identification;
  guint16 peak_int;
  gint16 gain_int;
  guint8 peak_bits;
  GstId3v2Frame frame;
  const gchar *frame_id;

  /* figure out tag names and the identification string to use */
  if (strcmp (tag, GST_TAG_TRACK_PEAK) == 0 ||
      strcmp (tag, GST_TAG_TRACK_GAIN) == 0) {
    gain_tag_name = GST_TAG_TRACK_GAIN;
    peak_tag_name = GST_TAG_TRACK_PEAK;
    identification = "track";
    GST_DEBUG ("adding track relative-volume frame");
  } else {
    gain_tag_name = GST_TAG_ALBUM_GAIN;
    peak_tag_name = GST_TAG_ALBUM_PEAK;
    identification = "album";

    if (id3v2tag->major_version == 3) {
      GST_WARNING ("Cannot store replaygain album gain data in ID3v2.3");
      return;
    }
    GST_DEBUG ("adding album relative-volume frame");
  }

  /* find the value for the paired tag (gain, if this is peak, and
   * vice versa).  if both tags exist, only write the frame when
   * we're processing the peak tag.
   */
  if (strcmp (tag, GST_TAG_TRACK_PEAK) == 0 ||
      strcmp (tag, GST_TAG_ALBUM_PEAK) == 0) {

    gst_tag_list_get_double (list, tag, &peak_val);

    if (gst_tag_list_get_tag_size (list, gain_tag_name) > 0) {
      gst_tag_list_get_double (list, gain_tag_name, &gain_val);
      GST_DEBUG ("setting volume adjustment %g", gain_val);
      gain_int = (gint16) (gain_val * 512.0);
    } else
      gain_int = 0;

    /* copying mutagen: always write as 16 bits for sanity. */
    peak_int = (short) (peak_val * G_MAXSHORT);
    peak_bits = 16;
  } else {
    gst_tag_list_get_double (list, tag, &gain_val);
    GST_DEBUG ("setting volume adjustment %g", gain_val);

    gain_int = (gint16) (gain_val * 512.0);
    peak_bits = 0;
    peak_int = 0;

    if (gst_tag_list_get_tag_size (list, peak_tag_name) != 0) {
      GST_DEBUG
          ("both gain and peak tags exist, not adding frame this time around");
      return;
    }
  }

  if (id3v2tag->major_version == 4) {
    /* 2.4: Use RVA2 tag */
    frame_id = "RVA2";
  } else {
    /* 2.3: Use XRVA tag - this is experimental, but useful in the real world.
       This version only officially supports the 'RVAD' tag, but that appears
       to not be widely implemented in reality. */
    frame_id = "XRVA";
  }

  id3v2_frame_init (&frame, frame_id, 0);
  id3v2_frame_write_bytes (&frame, (const guint8 *) identification,
      strlen (identification) + 1);
  id3v2_frame_write_uint8 (&frame, 0x01);       /* Master volume */
  id3v2_frame_write_uint16 (&frame, gain_int);
  id3v2_frame_write_uint8 (&frame, peak_bits);
  if (peak_bits)
    id3v2_frame_write_uint16 (&frame, peak_int);

  g_array_append_val (id3v2tag->frames, frame);
}

/* id3demux produces these for frames it cannot parse */
#define GST_ID3_DEMUX_TAG_ID3V2_FRAME "private-id3v2-frame"

static const struct
{
  const gchar *gst_tag;
  const GstId3v2AddTagFunc func;
  const gchar *data;
} add_funcs[] = {
  {
        /* Simple text tags */
      GST_TAG_ARTIST, add_text_tag, "TPE1"}, {
      GST_TAG_ALBUM_ARTIST, add_text_tag, "TPE2"}, {
      GST_TAG_TITLE, add_text_tag, "TIT2"}, {
      GST_TAG_ALBUM, add_text_tag, "TALB"}, {
      GST_TAG_COPYRIGHT, add_text_tag, "TCOP"}, {
      GST_TAG_COMPOSER, add_text_tag, "TCOM"}, {
      GST_TAG_GENRE, add_text_tag, "TCON"}, {
      GST_TAG_ENCODED_BY, add_text_tag, "TENC"}, {
      GST_TAG_PUBLISHER, add_text_tag, "TPUB"}, {
      GST_TAG_INTERPRETED_BY, add_text_tag, "TPE4"}, {
      GST_TAG_MUSICAL_KEY, add_text_tag, "TKEY"}, {

        /* Private frames */
      GST_TAG_PRIVATE_DATA, add_private_data_tag, "PRIV"}, {
      GST_ID3_DEMUX_TAG_ID3V2_FRAME, add_id3v2frame_tag, NULL}, {

        /* Track and album numbers */
      GST_TAG_TRACK_NUMBER, add_count_or_num_tag, "TRCK"}, {
      GST_TAG_TRACK_COUNT, add_count_or_num_tag, "TRCK"}, {
      GST_TAG_ALBUM_VOLUME_NUMBER, add_count_or_num_tag, "TPOS"}, {
      GST_TAG_ALBUM_VOLUME_COUNT, add_count_or_num_tag, "TPOS"}, {

        /* Comment tags */
      GST_TAG_COMMENT, add_comment_tag, NULL}, {
      GST_TAG_EXTENDED_COMMENT, add_comment_tag, NULL}, {

        /* BPM tag */
      GST_TAG_BEATS_PER_MINUTE, add_bpm_tag, NULL}, {

        /* Images */
      GST_TAG_IMAGE, add_image_tag, NULL}, {
      GST_TAG_PREVIEW_IMAGE, add_image_tag, NULL}, {

        /* Misc user-defined text tags for IDs (and UFID frame) */
      GST_TAG_MUSICBRAINZ_ARTISTID, add_musicbrainz_tag, "\000"}, {
      GST_TAG_MUSICBRAINZ_ALBUMID, add_musicbrainz_tag, "\001"}, {
      GST_TAG_MUSICBRAINZ_ALBUMARTISTID, add_musicbrainz_tag, "\002"}, {
      GST_TAG_MUSICBRAINZ_RELEASEGROUPID, add_musicbrainz_tag, "\003"}, {
      GST_TAG_MUSICBRAINZ_TRMID, add_musicbrainz_tag, "\004"}, {
      GST_TAG_CDDA_MUSICBRAINZ_DISCID, add_musicbrainz_tag, "\005"}, {
      GST_TAG_CDDA_CDDB_DISCID, add_musicbrainz_tag, "\006"}, {
      GST_TAG_MUSICBRAINZ_RELEASETRACKID, add_musicbrainz_tag, "\007"}, {
      GST_TAG_MUSICBRAINZ_TRACKID, add_unique_file_id_tag, NULL}, {

        /* Info about encoder */
      GST_TAG_ENCODER, add_encoder_tag, NULL}, {
      GST_TAG_ENCODER_VERSION, add_encoder_tag, NULL}, {

        /* URIs */
      GST_TAG_COPYRIGHT_URI, add_uri_tag, "WCOP"}, {
      GST_TAG_LICENSE_URI, add_uri_tag, "WCOP"}, {

        /* Up to here, all the frame ids and contents have been the same between
           versions 2.3 and 2.4. The rest of them differ... */
        /* Date (in ID3v2.3, this is a TYER tag. In v2.4, it's a TDRC tag */
      GST_TAG_DATE_TIME, add_date_tag, NULL}, {

        /* Replaygain data (not really supported in 2.3, we use an experimental
           tag there) */
      GST_TAG_TRACK_PEAK, add_relative_volume_tag, NULL}, {
      GST_TAG_TRACK_GAIN, add_relative_volume_tag, NULL}, {
      GST_TAG_ALBUM_PEAK, add_relative_volume_tag, NULL}, {
      GST_TAG_ALBUM_GAIN, add_relative_volume_tag, NULL}, {

        /* Sortable version of various tags. These are all v2.4 ONLY */
      GST_TAG_ARTIST_SORTNAME, add_text_tag_v4, "TSOP"}, {
      GST_TAG_ALBUM_SORTNAME, add_text_tag_v4, "TSOA"}, {
      GST_TAG_TITLE_SORTNAME, add_text_tag_v4, "TSOT"}
};

static void
foreach_add_tag (const GstTagList * list, const gchar * tag, gpointer userdata)
{
  GstId3v2Tag *id3v2tag = (GstId3v2Tag *) userdata;
  guint num_tags, i;

  num_tags = gst_tag_list_get_tag_size (list, tag);

  GST_LOG ("Processing tag %s (num=%u)", tag, num_tags);

  if (num_tags > 1 && gst_tag_is_fixed (tag)) {
    GST_WARNING ("Multiple occurrences of fixed tag '%s', ignoring some", tag);
    num_tags = 1;
  }

  for (i = 0; i < G_N_ELEMENTS (add_funcs); ++i) {
    if (strcmp (add_funcs[i].gst_tag, tag) == 0) {
      add_funcs[i].func (id3v2tag, list, tag, num_tags, add_funcs[i].data);
      break;
    }
  }

  if (i == G_N_ELEMENTS (add_funcs)) {
    GST_WARNING ("Unsupported tag '%s' - not written", tag);
  }
}

GstBuffer *
id3_mux_render_v2_tag (GstTagMux * mux, const GstTagList * taglist, int version)
{
  GstId3v2Tag tag;
  GstBuffer *buf;

  if (!id3v2_tag_init (&tag, version)) {
    GST_WARNING_OBJECT (mux, "Unsupported version %d", version);
    return NULL;
  }

  /* Render the tag */
  gst_tag_list_foreach (taglist, foreach_add_tag, &tag);

#if 0
  /* Do we want to add our own signature to the tag somewhere? */
  {
    gchar *tag_producer_str;

    tag_producer_str = g_strdup_printf ("(GStreamer id3v2mux %s, using "
        "taglib %u.%u)", VERSION, TAGLIB_MAJOR_VERSION, TAGLIB_MINOR_VERSION);
    add_one_txxx_tag (id3v2tag, "tag_encoder", tag_producer_str);
    g_free (tag_producer_str);
  }
#endif

  /* Create buffer with tag */
  buf = id3v2_tag_to_buffer (&tag);
  GST_LOG_OBJECT (mux, "tag size = %d bytes", (int) gst_buffer_get_size (buf));

  id3v2_tag_unset (&tag);

  return buf;
}

#define ID3_V1_TAG_SIZE 128

typedef void (*GstId3v1WriteFunc) (const GstTagList * list,
    const gchar * gst_tag, guint8 * dst, int len, gboolean * wrote_tag);

static void
latin1_convert (const GstTagList * list, const gchar * tag,
    guint8 * dst, int maxlen, gboolean * wrote_tag)
{
  gchar *str;
  gsize len;
  gchar *latin1;

  if (!gst_tag_list_get_string (list, tag, &str) || str == NULL)
    return;

  /* Convert to Latin-1 (ISO-8859-1), replacing unrepresentable characters
     with '?' */
  latin1 =
      g_convert_with_fallback (str, -1, "ISO-8859-1", "UTF-8", (char *) "?",
      NULL, &len, NULL);

  if (latin1 != NULL && *latin1 != '\0') {
    len = MIN (len, maxlen);
    memcpy (dst, latin1, len);
    *wrote_tag = TRUE;
    g_free (latin1);
  }

  g_free (str);
}

static void
date_v1_convert (const GstTagList * list, const gchar * tag,
    guint8 * dst, int maxlen, gboolean * wrote_tag)
{
  GstDateTime *dt;

  /* Only one date supported */
  if (gst_tag_list_get_date_time_index (list, tag, 0, &dt)) {
    guint year = gst_date_time_get_year (dt);
    /* Check for plausible year */
    if (year > 500 && year < 2100) {
      gchar str[5];
      g_snprintf (str, 5, "%.4u", year);
      *wrote_tag = TRUE;
      memcpy (dst, str, 4);
    } else {
      GST_WARNING ("invalid year %u, skipping", year);
    }

    gst_date_time_unref (dt);
  }
}

static void
genre_v1_convert (const GstTagList * list, const gchar * tag,
    guint8 * dst, int maxlen, gboolean * wrote_tag)
{
  const gchar *str;
  int genreidx = -1;
  guint i, max;

  /* We only support one genre */
  if (!gst_tag_list_peek_string_index (list, tag, 0, &str) || str == NULL)
    return;

  max = gst_tag_id3_genre_count ();

  for (i = 0; i < max; i++) {
    const gchar *genre = gst_tag_id3_genre_get (i);
    if (g_str_equal (str, genre)) {
      genreidx = i;
      break;
    }
  }

  if (genreidx >= 0 && genreidx <= 127) {
    *dst = (guint8) genreidx;
    *wrote_tag = TRUE;
  }
}

static void
track_number_convert (const GstTagList * list, const gchar * tag,
    guint8 * dst, int maxlen, gboolean * wrote_tag)
{
  guint tracknum;

  /* We only support one track number */
  if (!gst_tag_list_get_uint_index (list, tag, 0, &tracknum))
    return;

  if (tracknum <= 127) {
    *dst = (guint8) tracknum;
    *wrote_tag = TRUE;
  }
}

/* FIXME: get rid of silly table */
static const struct
{
  const gchar *gst_tag;
  const gint offset;
  const gint length;
  const GstId3v1WriteFunc func;
} v1_funcs[] = {
  {
      GST_TAG_TITLE, 3, 30, latin1_convert}, {
      GST_TAG_ARTIST, 33, 30, latin1_convert}, {
      GST_TAG_ALBUM, 63, 30, latin1_convert}, {
      GST_TAG_DATE_TIME, 93, 4, date_v1_convert}, {
      GST_TAG_COMMENT, 97, 28, latin1_convert}, {
        /* Note: one-byte gap here */
      GST_TAG_TRACK_NUMBER, 126, 1, track_number_convert}, {
      GST_TAG_GENRE, 127, 1, genre_v1_convert}
};

GstBuffer *
id3_mux_render_v1_tag (GstTagMux * mux, const GstTagList * taglist)
{
  GstMapInfo info;
  GstBuffer *buf;
  guint8 *data;
  gboolean wrote_tag = FALSE;
  int i;

  buf = gst_buffer_new_allocate (NULL, ID3_V1_TAG_SIZE, NULL);
  gst_buffer_map (buf, &info, GST_MAP_WRITE);
  data = info.data;
  memset (data, 0, ID3_V1_TAG_SIZE);

  data[0] = 'T';
  data[1] = 'A';
  data[2] = 'G';

  /* Genre #0 stands for 'Blues', so init genre field to an invalid number */
  data[127] = 255;

  for (i = 0; i < G_N_ELEMENTS (v1_funcs); i++) {
    v1_funcs[i].func (taglist, v1_funcs[i].gst_tag, data + v1_funcs[i].offset,
        v1_funcs[i].length, &wrote_tag);
  }

  gst_buffer_unmap (buf, &info);

  if (!wrote_tag) {
    GST_WARNING_OBJECT (mux, "no ID3v1 tag written (no suitable tags found)");
    gst_buffer_unref (buf);
    return NULL;
  }

  return buf;
}
