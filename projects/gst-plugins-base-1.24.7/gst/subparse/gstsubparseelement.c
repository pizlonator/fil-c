/* GStreamer
 * Copyright (C) 2020 Huawei Technologies Co., Ltd.
 *   @Author: Stéphane Cerveau <scerveau@collabora.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_VALGRIND
# include <valgrind/valgrind.h>
#endif

#include <stdio.h>

#include "gstsubparseelements.h"

GST_DEBUG_CATEGORY (sub_parse_debug);

/* regex type enum */
typedef enum
{
  GST_SUB_PARSE_REGEX_UNKNOWN = 0,
  GST_SUB_PARSE_REGEX_MDVDSUB = 1,
  GST_SUB_PARSE_REGEX_SUBRIP = 2,
  GST_SUB_PARSE_REGEX_DKS = 3,
  GST_SUB_PARSE_REGEX_VTT = 4,
} GstSubParseRegex;

static gpointer
gst_sub_parse_data_format_autodetect_regex_once (GstSubParseRegex regtype)
{
  gpointer result = NULL;
  GError *gerr = NULL;
  GRegexCompileFlags jit_flags = G_REGEX_OPTIMIZE | G_REGEX_RAW;

#ifdef HAVE_VALGRIND
  if (RUNNING_ON_VALGRIND) {
    /* jitted regex confuse valgrind */
    jit_flags &= ~G_REGEX_OPTIMIZE;
  }
#endif

  switch (regtype) {
    case GST_SUB_PARSE_REGEX_MDVDSUB:
      result =
          (gpointer) g_regex_new ("^\\{[0-9]+\\}\\{[0-9]+\\}",
          jit_flags, 0, &gerr);
      if (result == NULL) {
        g_warning ("Compilation of mdvd regex failed: %s", gerr->message);
        g_clear_error (&gerr);
      }
      break;
    case GST_SUB_PARSE_REGEX_SUBRIP:
      result = (gpointer)
          g_regex_new ("^[\\s\\n]*[\\n]? {0,3}[ 0-9]{1,4}\\s*(\x0d)?\x0a"
          " ?[0-9]{1,2}: ?[0-9]{1,2}: ?[0-9]{1,2}[,.] {0,2}[0-9]{1,3}"
          " +--> +[0-9]{1,2}: ?[0-9]{1,2}: ?[0-9]{1,2}[,.] {0,2}[0-9]{1,2}",
          jit_flags, 0, &gerr);
      if (result == NULL) {
        g_warning ("Compilation of subrip regex failed: %s", gerr->message);
        g_clear_error (&gerr);
      }
      break;
    case GST_SUB_PARSE_REGEX_DKS:
      result = (gpointer) g_regex_new ("^\\[[0-9]+:[0-9]+:[0-9]+\\].*",
          jit_flags, 0, &gerr);
      if (result == NULL) {
        g_warning ("Compilation of dks regex failed: %s", gerr->message);
        g_clear_error (&gerr);
      }
      break;
    case GST_SUB_PARSE_REGEX_VTT:
      result = (gpointer)
          g_regex_new ("^(\\xef\\xbb\\xbf)?WEBVTT[\\xa\\xd\\x20\\x9]", 0, 0,
          &gerr);
      if (result == NULL) {
        g_warning ("Compilation of vtt regex failed: %s", gerr->message);
        g_error_free (gerr);
      }
      break;

    default:
      GST_WARNING ("Trying to allocate regex of unknown type %u", regtype);
  }
  return result;
}

/*
 * FIXME: maybe we should pass along a second argument, the preceding
 * text buffer, because that is how this originally worked, even though
 * I don't really see the use of that.
 */

GstSubParseFormat
gst_sub_parse_data_format_autodetect (gchar * match_str)
{
  guint n1, n2, n3;

  static GOnce mdvd_rx_once = G_ONCE_INIT;
  static GOnce subrip_rx_once = G_ONCE_INIT;
  static GOnce dks_rx_once = G_ONCE_INIT;
  static GOnce vtt_rx_once = G_ONCE_INIT;

  GRegex *mdvd_grx;
  GRegex *subrip_grx;
  GRegex *dks_grx;
  GRegex *vtt_grx;

  g_once (&mdvd_rx_once,
      (GThreadFunc) gst_sub_parse_data_format_autodetect_regex_once,
      (gpointer) GST_SUB_PARSE_REGEX_MDVDSUB);
  g_once (&subrip_rx_once,
      (GThreadFunc) gst_sub_parse_data_format_autodetect_regex_once,
      (gpointer) GST_SUB_PARSE_REGEX_SUBRIP);
  g_once (&dks_rx_once,
      (GThreadFunc) gst_sub_parse_data_format_autodetect_regex_once,
      (gpointer) GST_SUB_PARSE_REGEX_DKS);
  g_once (&vtt_rx_once,
      (GThreadFunc) gst_sub_parse_data_format_autodetect_regex_once,
      (gpointer) GST_SUB_PARSE_REGEX_VTT);

  mdvd_grx = (GRegex *) mdvd_rx_once.retval;
  subrip_grx = (GRegex *) subrip_rx_once.retval;
  dks_grx = (GRegex *) dks_rx_once.retval;
  vtt_grx = (GRegex *) vtt_rx_once.retval;

  if (g_regex_match (mdvd_grx, match_str, 0, NULL)) {
    GST_LOG ("MicroDVD (frame based) format detected");
    return GST_SUB_PARSE_FORMAT_MDVDSUB;
  }
  if (g_regex_match (subrip_grx, match_str, 0, NULL)) {
    GST_LOG ("SubRip (time based) format detected");
    return GST_SUB_PARSE_FORMAT_SUBRIP;
  }
  if (g_regex_match (dks_grx, match_str, 0, NULL)) {
    GST_LOG ("DKS (time based) format detected");
    return GST_SUB_PARSE_FORMAT_DKS;
  }
  if (g_regex_match (vtt_grx, match_str, 0, NULL) == TRUE) {
    GST_LOG ("WebVTT (time based) format detected");
    return GST_SUB_PARSE_FORMAT_VTT;
  }

  if (!strncmp (match_str, "FORMAT=TIME", 11)) {
    GST_LOG ("MPSub (time based) format detected");
    return GST_SUB_PARSE_FORMAT_MPSUB;
  }
  if (strstr (match_str, "<SAMI>") != NULL ||
      strstr (match_str, "<sami>") != NULL) {
    GST_LOG ("SAMI (time based) format detected");
    return GST_SUB_PARSE_FORMAT_SAMI;
  }
  /* we're boldly assuming the first subtitle appears within the first hour */
  if (sscanf (match_str, "0:%02u:%02u:", &n1, &n2) == 2 ||
      sscanf (match_str, "0:%02u:%02u=", &n1, &n2) == 2 ||
      sscanf (match_str, "00:%02u:%02u:", &n1, &n2) == 2 ||
      sscanf (match_str, "00:%02u:%02u=", &n1, &n2) == 2 ||
      sscanf (match_str, "00:%02u:%02u,%u=", &n1, &n2, &n3) == 3) {
    GST_LOG ("TMPlayer (time based) format detected");
    return GST_SUB_PARSE_FORMAT_TMPLAYER;
  }
  if (sscanf (match_str, "[%u][%u]", &n1, &n2) == 2) {
    GST_LOG ("MPL2 (time based) format detected");
    return GST_SUB_PARSE_FORMAT_MPL2;
  }
  if (strstr (match_str, "[INFORMATION]") != NULL) {
    GST_LOG ("SubViewer (time based) format detected");
    return GST_SUB_PARSE_FORMAT_SUBVIEWER;
  }
  if (strstr (match_str, "{QTtext}") != NULL) {
    GST_LOG ("QTtext (time based) format detected");
    return GST_SUB_PARSE_FORMAT_QTTEXT;
  }
  /* We assume the LRC file starts immediately */
  if (match_str[0] == '[') {
    gboolean all_lines_good = TRUE;
    gchar **split;
    gchar **ptr;

    ptr = split = g_strsplit (match_str, "\n", -1);
    while (*ptr && *(ptr + 1)) {
      gchar *str = *ptr;
      gint len = strlen (str);

      if (sscanf (str, "[%u:%02u.%02u]", &n1, &n2, &n3) == 3 ||
          sscanf (str, "[%u:%02u.%03u]", &n1, &n2, &n3) == 3) {
        all_lines_good = TRUE;
      } else if (len > 0 && str[len - 1] == ']' && strchr (str, ':') != NULL) {
        all_lines_good = TRUE;
      } else {
        all_lines_good = FALSE;
        break;
      }

      ptr++;
    }
    g_strfreev (split);

    if (all_lines_good)
      return GST_SUB_PARSE_FORMAT_LRC;
  }

  GST_DEBUG ("no subtitle format detected");
  return GST_SUB_PARSE_FORMAT_UNKNOWN;
}

gchar *
gst_sub_parse_gst_convert_to_utf8 (const gchar * str, gsize len,
    const gchar * encoding, gsize * consumed, GError ** err)
{
  gchar *ret = NULL;

  *consumed = 0;
  /* The char cast is necessary in glib < 2.24 */
  ret =
      g_convert_with_fallback (str, len, "UTF-8", encoding, (char *) "*",
      consumed, NULL, err);
  if (ret == NULL)
    return ret;

  /* + 3 to skip UTF-8 BOM if it was added */
  len = strlen (ret);
  if (len >= 3 && (guint8) ret[0] == 0xEF && (guint8) ret[1] == 0xBB
      && (guint8) ret[2] == 0xBF)
    memmove (ret, ret + 3, len + 1 - 3);

  return ret;
}

gchar *
gst_sub_parse_detect_encoding (const gchar * str, gsize len)
{
  if (len >= 3 && (guint8) str[0] == 0xEF && (guint8) str[1] == 0xBB
      && (guint8) str[2] == 0xBF)
    return g_strdup ("UTF-8");

  if (len >= 2 && (guint8) str[0] == 0xFE && (guint8) str[1] == 0xFF)
    return g_strdup ("UTF-16BE");

  if (len >= 2 && (guint8) str[0] == 0xFF && (guint8) str[1] == 0xFE)
    return g_strdup ("UTF-16LE");

  if (len >= 4 && (guint8) str[0] == 0x00 && (guint8) str[1] == 0x00
      && (guint8) str[2] == 0xFE && (guint8) str[3] == 0xFF)
    return g_strdup ("UTF-32BE");

  if (len >= 4 && (guint8) str[0] == 0xFF && (guint8) str[1] == 0xFE
      && (guint8) str[2] == 0x00 && (guint8) str[3] == 0x00)
    return g_strdup ("UTF-32LE");

  return NULL;
}

/*
 * Typefind support.
 */

/* FIXME 0.11: these caps are ugly, use app/x-subtitle + type field or so;
 * also, give different  subtitle formats really different types */
static GstStaticCaps mpl2_caps =
GST_STATIC_CAPS ("application/x-subtitle-mpl2");
#define SUB_CAPS (gst_static_caps_get (&sub_caps))

static GstStaticCaps tmp_caps =
GST_STATIC_CAPS ("application/x-subtitle-tmplayer");
#define TMP_CAPS (gst_static_caps_get (&tmp_caps))

static GstStaticCaps sub_caps = GST_STATIC_CAPS ("application/x-subtitle");
#define MPL2_CAPS (gst_static_caps_get (&mpl2_caps))

static GstStaticCaps smi_caps = GST_STATIC_CAPS ("application/x-subtitle-sami");
#define SAMI_CAPS (gst_static_caps_get (&smi_caps))

static GstStaticCaps dks_caps = GST_STATIC_CAPS ("application/x-subtitle-dks");
#define DKS_CAPS (gst_static_caps_get (&dks_caps))

static GstStaticCaps vtt_caps = GST_STATIC_CAPS ("application/x-subtitle-vtt");
#define VTT_CAPS (gst_static_caps_get (&vtt_caps))

static GstStaticCaps qttext_caps =
GST_STATIC_CAPS ("application/x-subtitle-qttext");
#define QTTEXT_CAPS (gst_static_caps_get (&qttext_caps))

static GstStaticCaps lrc_caps = GST_STATIC_CAPS ("application/x-subtitle-lrc");
#define LRC_CAPS (gst_static_caps_get (&lrc_caps))

static void
gst_sub_parse_type_find (GstTypeFind * tf, gpointer private)
{
  GstSubParseFormat format;
  const guint8 *data;
  GstCaps *caps;
  gchar *str;
  gchar *encoding = NULL;
  const gchar *end;

  if (!(data = gst_type_find_peek (tf, 0, 129)))
    return;

  /* make sure string passed to _autodetect() is NUL-terminated */
  str = g_malloc0 (129);
  memcpy (str, data, 128);

  if ((encoding = gst_sub_parse_detect_encoding (str, 128)) != NULL) {
    gchar *converted_str;
    GError *err = NULL;
    gsize tmp;

    converted_str =
        gst_sub_parse_gst_convert_to_utf8 (str, 128, encoding, &tmp, &err);
    if (converted_str == NULL) {
      GST_DEBUG ("Encoding '%s' detected but conversion failed: %s", encoding,
          err->message);
      g_clear_error (&err);
    } else {
      g_free (str);
      str = converted_str;
    }
    g_free (encoding);
  }

  /* Check if at least the first 120 chars are valid UTF8,
   * otherwise convert as always */
  if (!g_utf8_validate (str, 128, &end) && (end - str) < 120) {
    gchar *converted_str;
    gsize tmp;
    const gchar *enc;

    enc = g_getenv ("GST_SUBTITLE_ENCODING");
    if (enc == NULL || *enc == '\0') {
      /* if local encoding is UTF-8 and no encoding specified
       * via the environment variable, assume ISO-8859-15 */
      if (g_get_charset (&enc)) {
        enc = "ISO-8859-15";
      }
    }
    converted_str =
        gst_sub_parse_gst_convert_to_utf8 (str, 128, enc, &tmp, NULL);
    if (converted_str != NULL) {
      g_free (str);
      str = converted_str;
    }
  }

  format = gst_sub_parse_data_format_autodetect (str);
  g_free (str);

  switch (format) {
    case GST_SUB_PARSE_FORMAT_MDVDSUB:
      GST_DEBUG ("MicroDVD format detected");
      caps = SUB_CAPS;
      break;
    case GST_SUB_PARSE_FORMAT_SUBRIP:
      GST_DEBUG ("SubRip format detected");
      caps = SUB_CAPS;
      break;
    case GST_SUB_PARSE_FORMAT_MPSUB:
      GST_DEBUG ("MPSub format detected");
      caps = SUB_CAPS;
      break;
    case GST_SUB_PARSE_FORMAT_SAMI:
      GST_DEBUG ("SAMI (time-based) format detected");
      caps = SAMI_CAPS;
      break;
    case GST_SUB_PARSE_FORMAT_TMPLAYER:
      GST_DEBUG ("TMPlayer (time based) format detected");
      caps = TMP_CAPS;
      break;
      /* FIXME: our MPL2 typefinding is not really good enough to warrant
       * returning a high probability (however, since we registered our
       * typefinder here with a rank of MARGINAL we should pretty much only
       * be called if most other typefinders have already run */
    case GST_SUB_PARSE_FORMAT_MPL2:
      GST_DEBUG ("MPL2 (time based) format detected");
      caps = MPL2_CAPS;
      break;
    case GST_SUB_PARSE_FORMAT_SUBVIEWER:
      GST_DEBUG ("SubViewer format detected");
      caps = SUB_CAPS;
      break;
    case GST_SUB_PARSE_FORMAT_DKS:
      GST_DEBUG ("DKS format detected");
      caps = DKS_CAPS;
      break;
    case GST_SUB_PARSE_FORMAT_QTTEXT:
      GST_DEBUG ("QTtext format detected");
      caps = QTTEXT_CAPS;
      break;
    case GST_SUB_PARSE_FORMAT_LRC:
      GST_DEBUG ("LRC format detected");
      caps = LRC_CAPS;
      break;
    case GST_SUB_PARSE_FORMAT_VTT:
      GST_DEBUG ("WebVTT format detected");
      caps = VTT_CAPS;
      break;
    default:
    case GST_SUB_PARSE_FORMAT_UNKNOWN:
      GST_DEBUG ("no subtitle format detected");
      return;
  }

  /* if we're here, it's ok */
  gst_type_find_suggest (tf, GST_TYPE_FIND_MAXIMUM, caps);
}

GST_TYPE_FIND_REGISTER_DEFINE (subparse, "subparse_typefind", GST_RANK_MARGINAL,
    gst_sub_parse_type_find, "srt,sub,mpsub,mdvd,smi,txt,dks,vtt", SUB_CAPS,
    NULL, NULL)
    gboolean
sub_parse_element_init (GstPlugin * plugin)
{
  static gsize res = FALSE;
  gboolean ret = TRUE;
  if (g_once_init_enter (&res)) {
    GST_DEBUG_CATEGORY_INIT (sub_parse_debug, "subparse", 0, ".sub parser");

    ret |= GST_TYPE_FIND_REGISTER (subparse, plugin);

    g_once_init_leave (&res, TRUE);
  }
  return ret;
}
