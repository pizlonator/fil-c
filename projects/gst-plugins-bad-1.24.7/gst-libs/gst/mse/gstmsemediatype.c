/* GStreamer
 *
 * SPDX-License-Identifier: LGPL-2.1
 *
 * Copyright (C) 2019 Igalia S.L.
 * Copyright (C) 2022, 2023 Collabora Ltd.
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

#include <glib.h>
#include <gst/gst.h>

#include "gstmsemediatype-private.h"

static const gchar tspecials[] = {
  '(', ')', '<', '>', '@', ',', ';', ':',
  '\\', '"', '/', '[', ']', '?', '=',
  0,
};

static gboolean
is_tspecial (const gchar c)
{
  for (const gchar * tspecial = tspecials; *tspecial != 0; tspecial++) {
    if (*tspecial == c) {
      return TRUE;
    }
  }
  return FALSE;
}

static gboolean
is_token_character (const gchar c)
{
  if (g_ascii_iscntrl (c) || is_tspecial (c)) {
    return FALSE;
  }
  return g_ascii_isgraph (c);
}

static gboolean
is_ascii (const gchar c)
{
  return c > 0 && c <= G_MAXINT8;
}

static gboolean
is_eos (const gchar c)
{
  return c == '\0';
}

static void
ignore_whitespace (const gchar ** input)
{
  g_return_if_fail (input != NULL);

  const gchar *output;
  for (output = *input; g_ascii_isspace (*output); output++) {
  }
  *input = output;
}

static gboolean
any_of (const gchar ** input, const gchar ** choices, gchar ** value)
{
  g_return_val_if_fail (input != NULL, FALSE);
  g_return_val_if_fail (choices != NULL, FALSE);
  g_return_val_if_fail (value != NULL, FALSE);

  for (const gchar ** choice = choices; *choice != NULL; choice++) {
    gulong prefix_length = strlen (*choice);
    if (g_str_has_prefix (*input, *choice)) {
      *value = g_strndup (*input, prefix_length);
      *input += prefix_length;
      return TRUE;
    }
  }
  return FALSE;
}

static gboolean
literal (const gchar ** input, const gchar * value)
{
  g_return_val_if_fail (input != NULL, FALSE);
  g_return_val_if_fail (value != NULL, FALSE);

  if (!g_str_has_prefix (*input, value)) {
    return FALSE;
  }
  *input += strlen (value);
  return TRUE;
}

static gboolean
token (const gchar ** input, gchar ** value)
{
  g_return_val_if_fail (input != NULL, FALSE);
  g_return_val_if_fail (value != NULL, FALSE);

  const gchar *unparsed = *input;
  for (; is_token_character (unparsed[0]); unparsed++) {
  }
  gsize length = unparsed - *input;
  if (length < 1) {
    return FALSE;
  }
  *value = g_strndup (*input, length);
  *input = unparsed;
  return TRUE;
}

static gboolean
quoted_string_char (const gchar ** input, gchar * value)
{
  g_return_val_if_fail (input != NULL, FALSE);
  g_return_val_if_fail (value != NULL, FALSE);

  const gchar *unparsed = *input;
  char c = unparsed[0];
  if (!is_ascii (c)) {
    return FALSE;
  }

  if (c == '"' || c == '\\' || c == '\r') {
    return FALSE;
  }

  *value = c;
  (*input)++;
  return TRUE;
}

static gboolean
escaped_ascii_char (const gchar ** input, gchar * value)
{
  g_return_val_if_fail (input != NULL, FALSE);
  g_return_val_if_fail (value != NULL, FALSE);

  if (!literal (input, "\\")) {
    return FALSE;
  }

  gchar c = (*input)[0];
  if (!is_ascii (c)) {
    return FALSE;
  }

  *value = c;
  (*input)++;
  return TRUE;
}

static gboolean
quoted_string (const gchar ** input, gchar ** value)
{
  g_return_val_if_fail (input != NULL, FALSE);
  g_return_val_if_fail (value != NULL, FALSE);

  const gchar *unparsed = *input;
  if (!literal (&unparsed, "\"")) {
    return FALSE;
  }

  GString *string_value = g_string_new (NULL);
  for (;;) {
    if (literal (&unparsed, "\"")) {
      *input = unparsed;
      *value = g_string_free (string_value, FALSE);
      return TRUE;
    }

    if (is_eos (unparsed[0])) {
      goto error;
    }

    gchar c;

    if (quoted_string_char (&unparsed, &c)) {
      g_string_append_c (string_value, c);
      continue;
    }

    if (escaped_ascii_char (&unparsed, &c)) {
      g_string_append_c (string_value, c);
      continue;
    }

    goto error;
  }
error:
  g_string_free (string_value, TRUE);
  return FALSE;
}

static const gchar *discrete_media_types[] = {
  "text",
  "image",
  "audio",
  "video",
  "application",
  NULL,
};

static gboolean
discrete_media_type (const gchar ** input, gchar ** value)
{
  return any_of (input, discrete_media_types, value);
}

static gboolean
composite_media_type (const gchar ** input, gchar ** value)
{
  return token (input, value);
}

static gboolean
media_type_parameter (const gchar ** input, gchar ** param_name,
    gchar ** param_value)
{
  g_return_val_if_fail (input != NULL, FALSE);
  g_return_val_if_fail (param_name != NULL, FALSE);
  g_return_val_if_fail (param_value != NULL, FALSE);

  ignore_whitespace (input);
  if (!token (input, param_name)) {
    return FALSE;
  }
  if (!literal (input, "=")) {
    return FALSE;
  }
  if (!token (input, param_value)) {
    if (!quoted_string (input, param_value)) {
      return FALSE;
    }
  }
  ignore_whitespace (input);
  return TRUE;
}

static gboolean
media_type (const gchar ** input, gchar ** value)
{
  g_return_val_if_fail (input != NULL, FALSE);
  g_return_val_if_fail (value != NULL, FALSE);

  gchar *discrete = NULL;
  gchar *composite = NULL;

  if (!discrete_media_type (input, &discrete)) {
    goto error;
  }
  if (!literal (input, "/")) {
    goto error;
  }
  if (!composite_media_type (input, &composite)) {
    goto error;
  }

  *value = g_strdup_printf ("%s/%s", discrete, composite);
  g_clear_pointer (&discrete, g_free);
  g_clear_pointer (&composite, g_free);
  return TRUE;

error:
  g_clear_pointer (&discrete, g_free);
  g_clear_pointer (&composite, g_free);
  return FALSE;
}

static gboolean
media_type_codecs (const gchar ** input, gchar *** codecs)
{
  g_return_val_if_fail (input != NULL, FALSE);
  g_return_val_if_fail (codecs != NULL, FALSE);

  gchar *param_name = NULL;
  gchar *codecs_value = NULL;
  while (!is_eos (*input[0])
      && media_type_parameter (input, &param_name, &codecs_value)) {
    gboolean is_codecs = g_strcmp0 (param_name, "codecs") == 0;
    g_clear_pointer (&param_name, g_free);
    if (is_codecs) {
      break;
    }
    g_clear_pointer (&codecs_value, g_free);
  }
  if (codecs_value == NULL) {
    return TRUE;
  }

  gchar **codec_names = g_strsplit (codecs_value, ",", 0);
  guint n_codecs = g_strv_length (codec_names);
  GPtrArray *codecs_array = g_ptr_array_new_full (n_codecs, NULL);
  for (guint i = 0; i < n_codecs; i++) {
    gchar *codec = g_strstrip (g_strdup (codec_names[i]));
    if (g_strcmp0 (codec, "") != 0) {
      g_ptr_array_add (codecs_array, g_strdup (codec));
    }
    g_clear_pointer (&codec, g_free);
  }
  g_ptr_array_add (codecs_array, NULL);
  *codecs = (gchar **) g_ptr_array_free (codecs_array, FALSE);

  g_clear_pointer (&codecs_value, g_free);
  g_strfreev (codec_names);

  return TRUE;
}

gboolean
gst_media_source_media_type_parse (GstMediaSourceMediaType * self,
    const gchar * type)
{
  g_return_val_if_fail (self != NULL, FALSE);
  g_return_val_if_fail (type != NULL, FALSE);

  gchar *lowercase = g_ascii_strdown (type, -1);
  const gchar *input = lowercase;

  ignore_whitespace (&input);
  if (!media_type (&input, &self->mime_type)) {
    g_free (lowercase);
    return FALSE;
  }
  self->codecs = NULL;
  ignore_whitespace (&input);
  if (is_eos (input[0])) {
    g_free (lowercase);
    return TRUE;
  }
  if (!literal (&input, ";")) {
    g_free (lowercase);
    return TRUE;
  }
  ignore_whitespace (&input);
  media_type_codecs (&input, &self->codecs);
  g_free (lowercase);
  return TRUE;
}

static inline gboolean
has_any_prefix (const gchar * str, const gchar * prefix, ...)
{
  if (g_str_has_prefix (str, prefix)) {
    return TRUE;
  }

  va_list varargs;

  va_start (varargs, prefix);
  for (const gchar * arg = va_arg (varargs, const gchar *); arg != NULL;
      arg = va_arg (varargs, const gchar *))
  {
    if (g_str_has_prefix (str, arg)) {
      va_end (varargs);
      return TRUE;
    }
  }
  va_end (varargs);
  return FALSE;
}

static const gchar *
patch_media_type (const gchar * media_type)
{
  if (g_strcmp0 (media_type, "video/mp4") == 0) {
    return "video/quicktime";
  }
  if (g_strcmp0 (media_type, "audio/mp4") == 0 ||
      g_strcmp0 (media_type, "audio/aac") == 0) {
    return "audio/x-m4a";
  }
  if (!has_any_prefix (media_type, "audio/", "video/", NULL)) {
    return NULL;
  }
  return media_type;
}

static GstStaticCaps h264_caps = GST_STATIC_CAPS ("video/x-h264");
static GstStaticCaps h265_caps = GST_STATIC_CAPS ("video/x-h265");
static GstStaticCaps av1_caps = GST_STATIC_CAPS ("video/x-av1");
static GstStaticCaps vp8_caps = GST_STATIC_CAPS ("video/x-vp8");
static GstStaticCaps vp9_caps = GST_STATIC_CAPS ("video/x-vp9");

static GstStaticCaps vorbis_caps = GST_STATIC_CAPS ("audio/x-vorbis");
static GstStaticCaps opus_caps = GST_STATIC_CAPS ("audio/x-opus");
static GstStaticCaps flac_caps = GST_STATIC_CAPS ("audio/x-flac");
static GstStaticCaps mp3_caps =
GST_STATIC_CAPS ("audio/mpeg, mpegversion=(int)1, layer=(int)3");
static GstStaticCaps aac_caps =
GST_STATIC_CAPS ("audio/mpeg, mpegversion=(int)4");

static inline GstStaticCaps *
mse_codec_id_to_static_caps (const gchar * codec_id)
{
  if (has_any_prefix (codec_id, "avc", "x-h264", "mp4v", NULL)) {
    return &h264_caps;
  }
  if (has_any_prefix (codec_id, "hvc1", "hev1", "x-h265", NULL)) {
    return &h265_caps;
  }
  if (has_any_prefix (codec_id, "av01", "av1", "x-av1", NULL)) {
    return &av1_caps;
  }
  if (has_any_prefix (codec_id, "vp8", "x-vp8", NULL)) {
    return &vp8_caps;
  }
  if (has_any_prefix (codec_id, "vp9", "vp09", "x-vp9", NULL)) {
    return &vp9_caps;
  }

  if (has_any_prefix (codec_id, "mpeg", "mp4a", NULL)) {
    return &aac_caps;
  }
  if (has_any_prefix (codec_id, "vorbis", "x-vorbis", NULL)) {
    return &vorbis_caps;
  }
  if (has_any_prefix (codec_id, "opus", "x-opus", NULL)) {
    return &opus_caps;
  }
  if (has_any_prefix (codec_id, "flac", "x-flac", NULL)) {
    return &flac_caps;
  }
  if (has_any_prefix (codec_id, "mp3", "audio/mp3", NULL)) {
    return &mp3_caps;
  }

  return NULL;
}

static gboolean
is_supported (const GstCaps * caps, GList * elements)
{
  GList *supporting_elements = gst_element_factory_list_filter (elements, caps,
      GST_PAD_SINK, FALSE);
  gboolean supported = supporting_elements != NULL;
  gst_plugin_feature_list_free (supporting_elements);
  return supported;
}

static gboolean
is_codec_supported (const gchar * codec_id, GList * elements)
{
  GstStaticCaps *static_caps = mse_codec_id_to_static_caps (codec_id);
  if (static_caps == NULL) {
    return FALSE;
  }
  GstCaps *codec_caps = gst_static_caps_get (static_caps);
  gboolean supported = is_supported (codec_caps, elements);
  gst_clear_caps (&codec_caps);
  return supported;
}

static inline GList *
get_srcpad_elements (void)
{
  return gst_element_factory_list_get_elements (GST_ELEMENT_FACTORY_TYPE_DEMUXER
      | GST_ELEMENT_FACTORY_TYPE_DECODER | GST_ELEMENT_FACTORY_TYPE_DECRYPTOR |
      GST_ELEMENT_FACTORY_TYPE_DEPAYLOADER | GST_ELEMENT_FACTORY_TYPE_SINK |
      GST_ELEMENT_FACTORY_TYPE_PARSER, GST_RANK_MARGINAL);
}

gboolean
gst_media_source_media_type_is_caps_supported (const GstCaps * caps)
{
  GList *elements = get_srcpad_elements ();
  gboolean supported = is_supported (caps, elements);
  gst_plugin_feature_list_free (elements);
  return supported;
}

gboolean
gst_media_source_media_type_is_supported (GstMediaSourceMediaType * self)
{
  g_return_val_if_fail (self != NULL, FALSE);
  g_return_val_if_fail (self->mime_type != NULL, FALSE);

  const gchar *mime_type = patch_media_type (self->mime_type);
  if (mime_type == NULL) {
    return FALSE;
  }

  GList *elements = get_srcpad_elements ();
  GstCaps *caps = gst_caps_from_string (mime_type);

  gboolean supported = is_supported (caps, elements);
  for (gchar ** codec_id = self->codecs;
      supported && codec_id && *codec_id; codec_id++) {
    supported &= is_codec_supported (*codec_id, elements);
  }

  gst_plugin_feature_list_free (elements);
  gst_clear_caps (&caps);

  return supported;
}

static const gchar *generate_timestamps_formats[] = {
  "audio/mpeg",
  "audio/aac",
  NULL,
};

gboolean
gst_media_source_media_type_generates_timestamp (GstMediaSourceMediaType * self)
{
  g_return_val_if_fail (self != NULL, FALSE);
  g_return_val_if_fail (self->mime_type != NULL, FALSE);
  return g_strv_contains (generate_timestamps_formats, self->mime_type);
}

void
gst_media_source_media_type_reset (GstMediaSourceMediaType * self)
{
  g_return_if_fail (self != NULL);
  g_clear_pointer (&self->mime_type, g_free);
  g_clear_pointer (&self->codecs, g_strfreev);
}

void
gst_media_source_media_type_free (GstMediaSourceMediaType * self)
{
  gst_media_source_media_type_reset (self);
  g_free (self);
}
