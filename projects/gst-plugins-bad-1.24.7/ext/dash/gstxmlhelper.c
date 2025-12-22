/* GStreamer
 *
 * Copyright (C) 2019 Collabora Ltd.
 *   Author: Stéphane Cerveau <scerveau@collabora.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 *
 */

#include "gstxmlhelper.h"

#define XML_HELPER_MINUTE_TO_SEC       60
#define XML_HELPER_HOUR_TO_SEC         (60 * XML_HELPER_MINUTE_TO_SEC)
#define XML_HELPER_DAY_TO_SEC          (24 * XML_HELPER_HOUR_TO_SEC)
#define XML_HELPER_MONTH_TO_SEC        (30 * XML_HELPER_DAY_TO_SEC)
#define XML_HELPER_YEAR_TO_SEC         (365 * XML_HELPER_DAY_TO_SEC)
#define XML_HELPER_MS_TO_SEC(time)     ((time) / 1000)
/* static methods */
/* this function computes decimals * 10 ^ (3 - pos) */
static guint
_mpd_helper_convert_to_millisecs (guint decimals, gint pos)
{
  guint num = 1, den = 1;
  gint i = 3 - pos;

  while (i < 0) {
    den *= 10;
    i++;
  }
  while (i > 0) {
    num *= 10;
    i--;
  }
  /* if i == 0 we have exactly 3 decimals and nothing to do */
  return decimals * num / den;
}

static gboolean
_mpd_helper_accumulate (guint64 * v, guint64 mul, guint64 add)
{
  guint64 tmp;

  if (*v > G_MAXUINT64 / mul)
    return FALSE;
  tmp = *v * mul;
  if (tmp > G_MAXUINT64 - add)
    return FALSE;
  *v = tmp + add;
  return TRUE;
}

/*
  Duration Data Type

  The duration data type is used to specify a time interval.

  The time interval is specified in the following form "-PnYnMnDTnHnMnS" where:

    * - indicates the negative sign (optional)
    * P indicates the period (required)
    * nY indicates the number of years
    * nM indicates the number of months
    * nD indicates the number of days
    * T indicates the start of a time section (required if you are going to specify hours, minutes, or seconds)
    * nH indicates the number of hours
    * nM indicates the number of minutes
    * nS indicates the number of seconds
*/
static gboolean
_mpd_helper_parse_duration (const char *str, guint64 * value)
{
  gint ret, len, pos, posT;
  gint years = -1, months = -1, days = -1, hours = -1, minutes = -1, seconds =
      -1, decimals = -1, read;
  gboolean have_ms = FALSE;
  guint64 tmp_value;

  len = strlen (str);
  GST_TRACE ("duration: %s, len %d", str, len);
  if (strspn (str, "PT0123456789., \tHMDSY") < len) {
    GST_WARNING ("Invalid character found: '%s'", str);
    goto error;
  }
  /* skip leading/trailing whitespace */
  while (g_ascii_isspace (str[0])) {
    str++;
    len--;
  }
  while (len > 0 && g_ascii_isspace (str[len - 1]))
    --len;

  /* read "P" for period */
  if (str[0] != 'P') {
    GST_WARNING ("P not found at the beginning of the string!");
    goto error;
  }
  str++;
  len--;

  /* read "T" for time (if present) */
  posT = strcspn (str, "T");
  len -= posT;
  if (posT > 0) {
    /* there is some room between P and T, so there must be a period section */
    /* read years, months, days */
    do {
      GST_TRACE ("parsing substring %s", str);
      pos = strcspn (str, "YMD");
      ret = sscanf (str, "%u", &read);
      if (ret != 1) {
        GST_WARNING ("can not read integer value from string %s!", str);
        goto error;
      }
      switch (str[pos]) {
        case 'Y':
          if (years != -1 || months != -1 || days != -1) {
            GST_WARNING ("year, month or day was already set");
            goto error;
          }
          years = read;
          break;
        case 'M':
          if (months != -1 || days != -1) {
            GST_WARNING ("month or day was already set");
            goto error;
          }
          months = read;
          if (months >= 12) {
            GST_WARNING ("Month out of range");
            goto error;
          }
          break;
        case 'D':
          if (days != -1) {
            GST_WARNING ("day was already set");
            goto error;
          }
          days = read;
          if (days >= 31) {
            GST_WARNING ("Day out of range");
            goto error;
          }
          break;
        default:
          GST_WARNING ("unexpected char %c!", str[pos]);
          goto error;
          break;
      }
      GST_TRACE ("read number %u type %c", read, str[pos]);
      str += (pos + 1);
      posT -= (pos + 1);
    } while (posT > 0);
  }

  if (years == -1)
    years = 0;
  if (months == -1)
    months = 0;
  if (days == -1)
    days = 0;

  GST_TRACE ("Y:M:D=%d:%d:%d", years, months, days);

  /* read "T" for time (if present) */
  /* here T is at pos == 0 */
  str++;
  len--;
  pos = 0;
  if (pos < len) {
    /* T found, there is a time section */
    /* read hours, minutes, seconds, hundredths of second */
    do {
      GST_TRACE ("parsing substring %s", str);
      pos = strcspn (str, "HMS,.");
      ret = sscanf (str, "%u", &read);
      if (ret != 1) {
        GST_WARNING ("can not read integer value from string %s!", str);
        goto error;
      }
      switch (str[pos]) {
        case 'H':
          if (hours != -1 || minutes != -1 || seconds != -1) {
            GST_WARNING ("hour, minute or second was already set");
            goto error;
          }
          hours = read;
          if (hours >= 24) {
            GST_WARNING ("Hour out of range");
            goto error;
          }
          break;
        case 'M':
          if (minutes != -1 || seconds != -1) {
            GST_WARNING ("minute or second was already set");
            goto error;
          }
          minutes = read;
          if (minutes >= 60) {
            GST_WARNING ("Minute out of range");
            goto error;
          }
          break;
        case 'S':
          if (have_ms) {
            /* we have read the decimal part of the seconds */
            decimals = _mpd_helper_convert_to_millisecs (read, pos);
            GST_TRACE ("decimal number %u (%d digits) -> %d ms", read, pos,
                decimals);
          } else {
            if (seconds != -1) {
              GST_WARNING ("second was already set");
              goto error;
            }
            /* no decimals */
            seconds = read;
          }
          break;
        case '.':
        case ',':
          /* we have read the integer part of a decimal number in seconds */
          if (seconds != -1) {
            GST_WARNING ("second was already set");
            goto error;
          }
          seconds = read;
          have_ms = TRUE;
          break;
        default:
          GST_WARNING ("unexpected char %c!", str[pos]);
          goto error;
          break;
      }
      GST_TRACE ("read number %u type %c", read, str[pos]);
      str += pos + 1;
      len -= (pos + 1);
    } while (len > 0);
  }

  if (hours == -1)
    hours = 0;
  if (minutes == -1)
    minutes = 0;
  if (seconds == -1)
    seconds = 0;
  if (decimals == -1)
    decimals = 0;
  GST_TRACE ("H:M:S.MS=%d:%d:%d.%03d", hours, minutes, seconds, decimals);

  tmp_value = 0;
  if (!_mpd_helper_accumulate (&tmp_value, 1, years)
      || !_mpd_helper_accumulate (&tmp_value, 365, months * 30)
      || !_mpd_helper_accumulate (&tmp_value, 1, days)
      || !_mpd_helper_accumulate (&tmp_value, 24, hours)
      || !_mpd_helper_accumulate (&tmp_value, 60, minutes)
      || !_mpd_helper_accumulate (&tmp_value, 60, seconds)
      || !_mpd_helper_accumulate (&tmp_value, 1000, decimals))
    goto error;

  /* ensure it can be converted from milliseconds to nanoseconds */
  if (tmp_value > G_MAXUINT64 / 1000000)
    goto error;

  *value = tmp_value;
  return TRUE;

error:
  return FALSE;
}

static gboolean
_mpd_helper_validate_no_whitespace (const char *s)
{
  return !strpbrk (s, "\r\n\t ");
}

/* API */

GstXMLRange *
gst_xml_helper_clone_range (GstXMLRange * range)
{
  GstXMLRange *clone = NULL;

  if (range) {
    clone = g_slice_new0 (GstXMLRange);
    clone->first_byte_pos = range->first_byte_pos;
    clone->last_byte_pos = range->last_byte_pos;
  }

  return clone;
}

GstXMLRatio *
gst_xml_helper_clone_ratio (GstXMLRatio * ratio)
{
  GstXMLRatio *clone = NULL;

  if (ratio) {
    clone = g_slice_new0 (GstXMLRatio);
    clone->num = ratio->num;
    clone->den = ratio->den;
  }

  return clone;
}

GstXMLFrameRate *
gst_xml_helper_clone_frame_rate (GstXMLFrameRate * frameRate)
{
  GstXMLFrameRate *clone = NULL;

  if (frameRate) {
    clone = g_slice_new0 (GstXMLFrameRate);
    clone->num = frameRate->num;
    clone->den = frameRate->den;
  }

  return clone;
}

/* XML property get method */
gboolean
gst_xml_helper_get_prop_validated_string (xmlNode * a_node,
    const gchar * property_name, gchar ** property_value,
    gboolean (*validate) (const char *))
{
  xmlChar *prop_string;
  gboolean exists = FALSE;

  prop_string = xmlGetProp (a_node, (const xmlChar *) property_name);
  if (prop_string) {
    if (validate && !(*validate) ((const char *) prop_string)) {
      GST_WARNING ("Validation failure: %s", prop_string);
      xmlFree (prop_string);
      return FALSE;
    }
    *property_value = (gchar *) prop_string;
    exists = TRUE;
    GST_LOG (" - %s: %s", property_name, prop_string);
  }

  return exists;
}

gboolean
gst_xml_helper_get_ns_prop_string (xmlNode * a_node,
    const gchar * ns_name, const gchar * property_name, gchar ** property_value)
{
  xmlChar *prop_string;
  gboolean exists = FALSE;

  prop_string =
      xmlGetNsProp (a_node, (const xmlChar *) property_name,
      (const xmlChar *) ns_name);
  if (prop_string) {
    *property_value = (gchar *) prop_string;
    exists = TRUE;
    GST_LOG (" - %s:%s: %s", ns_name, property_name, prop_string);
  }

  return exists;
}

gboolean
gst_xml_helper_get_prop_string (xmlNode * a_node,
    const gchar * property_name, gchar ** property_value)
{
  return gst_xml_helper_get_prop_validated_string (a_node, property_name,
      property_value, NULL);
}

gboolean
gst_xml_helper_get_prop_string_vector_type (xmlNode * a_node,
    const gchar * property_name, gchar *** property_value)
{
  xmlChar *prop_string;
  gchar **prop_string_vector = NULL;
  guint i = 0;
  gboolean exists = FALSE;

  prop_string = xmlGetProp (a_node, (const xmlChar *) property_name);
  if (prop_string) {
    prop_string_vector = g_strsplit ((gchar *) prop_string, " ", -1);
    if (prop_string_vector) {
      exists = TRUE;
      *property_value = prop_string_vector;
      GST_LOG (" - %s:", property_name);
      while (prop_string_vector[i]) {
        GST_LOG ("    %s", prop_string_vector[i]);
        i++;
      }
    } else {
      GST_WARNING ("Scan of string vector property failed!");
    }
    xmlFree (prop_string);
  }

  return exists;
}

gboolean
gst_xml_helper_get_prop_signed_integer (xmlNode * a_node,
    const gchar * property_name, gint default_val, gint * property_value)
{
  xmlChar *prop_string;
  gboolean exists = FALSE;

  *property_value = default_val;
  prop_string = xmlGetProp (a_node, (const xmlChar *) property_name);
  if (prop_string) {
    if (sscanf ((const gchar *) prop_string, "%d", property_value) == 1) {
      exists = TRUE;
      GST_LOG (" - %s: %d", property_name, *property_value);
    } else {
      GST_WARNING
          ("failed to parse signed integer property %s from xml string %s",
          property_name, prop_string);
    }
    xmlFree (prop_string);
  }

  return exists;
}

gboolean
gst_xml_helper_get_prop_unsigned_integer (xmlNode * a_node,
    const gchar * property_name, guint default_val, guint * property_value)
{
  xmlChar *prop_string;
  gboolean exists = FALSE;

  *property_value = default_val;
  prop_string = xmlGetProp (a_node, (const xmlChar *) property_name);
  if (prop_string) {
    if (sscanf ((gchar *) prop_string, "%u", property_value) == 1 &&
        strstr ((gchar *) prop_string, "-") == NULL) {
      exists = TRUE;
      GST_LOG (" - %s: %u", property_name, *property_value);
    } else {
      GST_WARNING
          ("failed to parse unsigned integer property %s from xml string %s",
          property_name, prop_string);
      /* sscanf might have written to *property_value. Restore to default */
      *property_value = default_val;
    }
    xmlFree (prop_string);
  }

  return exists;
}

gboolean
gst_xml_helper_get_prop_unsigned_integer_64 (xmlNode * a_node,
    const gchar * property_name, guint64 default_val, guint64 * property_value)
{
  xmlChar *prop_string;
  gboolean exists = FALSE;

  *property_value = default_val;
  prop_string = xmlGetProp (a_node, (const xmlChar *) property_name);
  if (prop_string) {
    if (g_ascii_string_to_unsigned ((gchar *) prop_string, 10, 0, G_MAXUINT64,
            property_value, NULL)) {
      exists = TRUE;
      GST_LOG (" - %s: %" G_GUINT64_FORMAT, property_name, *property_value);
    } else {
      GST_WARNING
          ("failed to parse unsigned integer property %s from xml string %s",
          property_name, prop_string);
    }
    xmlFree (prop_string);
  }

  return exists;
}

gboolean
gst_xml_helper_get_prop_uint_vector_type (xmlNode * a_node,
    const gchar * property_name, guint ** property_value, guint * value_size)
{
  xmlChar *prop_string;
  gchar **str_vector;
  guint *prop_uint_vector = NULL, i;
  gboolean exists = FALSE;

  prop_string = xmlGetProp (a_node, (const xmlChar *) property_name);
  if (prop_string) {
    str_vector = g_strsplit ((gchar *) prop_string, " ", -1);
    if (str_vector) {
      *value_size = g_strv_length (str_vector);
      prop_uint_vector = g_malloc (*value_size * sizeof (guint));
      if (prop_uint_vector) {
        exists = TRUE;
        GST_LOG (" - %s:", property_name);
        for (i = 0; i < *value_size; i++) {
          if (sscanf ((gchar *) str_vector[i], "%u", &prop_uint_vector[i]) == 1
              && strstr (str_vector[i], "-") == NULL) {
            GST_LOG ("    %u", prop_uint_vector[i]);
          } else {
            GST_WARNING
                ("failed to parse uint vector type property %s from xml string %s",
                property_name, str_vector[i]);
            /* there is no special value to put in prop_uint_vector[i] to
             * signal it is invalid, so we just clean everything and return
             * FALSE
             */
            g_free (prop_uint_vector);
            prop_uint_vector = NULL;
            exists = FALSE;
            break;
          }
        }
        *property_value = prop_uint_vector;
      } else {
        GST_WARNING ("Array allocation failed!");
      }
    } else {
      GST_WARNING ("Scan of uint vector property failed!");
    }
    xmlFree (prop_string);
    g_strfreev (str_vector);
  }

  return exists;
}

gboolean
gst_xml_helper_get_prop_double (xmlNode * a_node,
    const gchar * property_name, gdouble * property_value)
{
  xmlChar *prop_string;
  gboolean exists = FALSE;

  prop_string = xmlGetProp (a_node, (const xmlChar *) property_name);
  if (prop_string) {
    if (sscanf ((gchar *) prop_string, "%lf", property_value) == 1) {
      exists = TRUE;
      GST_LOG (" - %s: %lf", property_name, *property_value);
    } else {
      GST_WARNING ("failed to parse double property %s from xml string %s",
          property_name, prop_string);
    }
    xmlFree (prop_string);
  }

  return exists;
}

gboolean
gst_xml_helper_get_prop_boolean (xmlNode * a_node,
    const gchar * property_name, gboolean default_val,
    gboolean * property_value)
{
  xmlChar *prop_string;
  gboolean exists = FALSE;

  *property_value = default_val;
  prop_string = xmlGetProp (a_node, (const xmlChar *) property_name);
  if (prop_string) {
    if (xmlStrcmp (prop_string, (xmlChar *) "false") == 0) {
      exists = TRUE;
      *property_value = FALSE;
      GST_LOG (" - %s: false", property_name);
    } else if (xmlStrcmp (prop_string, (xmlChar *) "true") == 0) {
      exists = TRUE;
      *property_value = TRUE;
      GST_LOG (" - %s: true", property_name);
    } else {
      GST_WARNING ("failed to parse boolean property %s from xml string %s",
          property_name, prop_string);
    }
    xmlFree (prop_string);
  }

  return exists;
}

gboolean
gst_xml_helper_get_prop_range (xmlNode * a_node,
    const gchar * property_name, GstXMLRange ** property_value)
{
  xmlChar *prop_string;
  guint64 first_byte_pos = 0, last_byte_pos = -1;
  guint len, pos;
  gchar *str;
  gboolean exists = FALSE;

  prop_string = xmlGetProp (a_node, (const xmlChar *) property_name);
  if (prop_string) {
    len = xmlStrlen (prop_string);
    str = (gchar *) prop_string;
    GST_TRACE ("range: %s, len %d", str, len);

    /* find "-" */
    pos = strcspn (str, "-");
    if (pos >= len) {
      GST_TRACE ("pos %d >= len %d", pos, len);
      goto error;
    }
    if (pos == 0) {
      GST_TRACE ("pos == 0, but first_byte_pos is not optional");
      goto error;
    }

    /* read first_byte_pos */

    /* replace str[pos] with '\0' since we only want to read the
     * first_byte_pos, and g_ascii_string_to_unsigned requires the entire
     * string to be a single number, which is exactly what we want */
    str[pos] = '\0';
    if (!g_ascii_string_to_unsigned (str, 10, 0, G_MAXUINT64, &first_byte_pos,
            NULL)) {
      /* restore the '-' sign */
      str[pos] = '-';
      goto error;
    }
    /* restore the '-' sign */
    str[pos] = '-';

    /* read last_byte_pos, which is optional */
    if (pos < (len - 1) && !g_ascii_string_to_unsigned (str + pos + 1, 10, 0,
            G_MAXUINT64, &last_byte_pos, NULL)) {
      goto error;
    }
    /* malloc return data structure */
    *property_value = g_slice_new0 (GstXMLRange);
    exists = TRUE;
    (*property_value)->first_byte_pos = first_byte_pos;
    (*property_value)->last_byte_pos = last_byte_pos;
    xmlFree (prop_string);
    GST_LOG (" - %s: %" G_GUINT64_FORMAT "-%" G_GUINT64_FORMAT,
        property_name, first_byte_pos, last_byte_pos);
  }

  return exists;

error:
  GST_WARNING ("failed to parse property %s from xml string %s", property_name,
      prop_string);
  xmlFree (prop_string);
  return FALSE;
}

gboolean
gst_xml_helper_get_prop_ratio (xmlNode * a_node,
    const gchar * property_name, GstXMLRatio ** property_value)
{
  xmlChar *prop_string;
  guint num = 0, den = 1;
  guint len, pos;
  gchar *str;
  gboolean exists = FALSE;

  prop_string = xmlGetProp (a_node, (const xmlChar *) property_name);
  if (prop_string) {
    len = xmlStrlen (prop_string);
    str = (gchar *) prop_string;
    GST_TRACE ("ratio: %s, len %d", str, len);

    /* read ":" */
    pos = strcspn (str, ":");
    if (pos >= len) {
      GST_TRACE ("pos %d >= len %d", pos, len);
      goto error;
    }
    /* search for negative sign */
    if (strstr (str, "-") != NULL) {
      goto error;
    }
    /* read num */
    if (pos != 0) {
      if (sscanf (str, "%u", &num) != 1) {
        goto error;
      }
    }
    /* read den */
    if (pos < (len - 1)) {
      if (sscanf (str + pos + 1, "%u", &den) != 1) {
        goto error;
      }
    }
    /* malloc return data structure */
    *property_value = g_slice_new0 (GstXMLRatio);
    exists = TRUE;
    (*property_value)->num = num;
    (*property_value)->den = den;
    xmlFree (prop_string);
    GST_LOG (" - %s: %u:%u", property_name, num, den);
  }

  return exists;

error:
  GST_WARNING ("failed to parse property %s from xml string %s", property_name,
      prop_string);
  xmlFree (prop_string);
  return FALSE;
}

gboolean
gst_xml_helper_get_prop_framerate (xmlNode * a_node,
    const gchar * property_name, GstXMLFrameRate ** property_value)
{
  xmlChar *prop_string;
  guint num = 0, den = 1;
  guint len, pos;
  gchar *str;
  gboolean exists = FALSE;

  prop_string = xmlGetProp (a_node, (const xmlChar *) property_name);
  if (prop_string) {
    len = xmlStrlen (prop_string);
    str = (gchar *) prop_string;
    GST_TRACE ("framerate: %s, len %d", str, len);

    /* search for negative sign */
    if (strstr (str, "-") != NULL) {
      goto error;
    }

    /* read "/" if available */
    pos = strcspn (str, "/");
    /* read num */
    if (pos != 0) {
      if (sscanf (str, "%u", &num) != 1) {
        goto error;
      }
    }
    /* read den (if available) */
    if (pos < (len - 1)) {
      if (sscanf (str + pos + 1, "%u", &den) != 1) {
        goto error;
      }
    }
    /* alloc return data structure */
    *property_value = g_slice_new0 (GstXMLFrameRate);
    exists = TRUE;
    (*property_value)->num = num;
    (*property_value)->den = den;
    xmlFree (prop_string);
    if (den == 1)
      GST_LOG (" - %s: %u", property_name, num);
    else
      GST_LOG (" - %s: %u/%u", property_name, num, den);
  }

  return exists;

error:
  GST_WARNING ("failed to parse property %s from xml string %s", property_name,
      prop_string);
  xmlFree (prop_string);
  return FALSE;
}

gboolean
gst_xml_helper_get_prop_cond_uint (xmlNode * a_node,
    const gchar * property_name, GstXMLConditionalUintType ** property_value)
{
  xmlChar *prop_string;
  gchar *str;
  gboolean flag;
  guint val;
  gboolean exists = FALSE;

  prop_string = xmlGetProp (a_node, (const xmlChar *) property_name);
  if (prop_string) {
    str = (gchar *) prop_string;
    GST_TRACE ("conditional uint: %s", str);

    if (strcmp (str, "false") == 0) {
      flag = FALSE;
      val = 0;
    } else if (strcmp (str, "true") == 0) {
      flag = TRUE;
      val = 0;
    } else {
      flag = TRUE;
      if (sscanf (str, "%u", &val) != 1 || strstr (str, "-") != NULL)
        goto error;
    }

    /* alloc return data structure */
    *property_value = g_slice_new0 (GstXMLConditionalUintType);
    exists = TRUE;
    (*property_value)->flag = flag;
    (*property_value)->value = val;
    xmlFree (prop_string);
    GST_LOG (" - %s: flag=%s val=%u", property_name, flag ? "true" : "false",
        val);
  }

  return exists;

error:
  GST_WARNING ("failed to parse property %s from xml string %s", property_name,
      prop_string);
  xmlFree (prop_string);
  return FALSE;
}

gboolean
gst_xml_helper_get_prop_dateTime (xmlNode * a_node,
    const gchar * property_name, GstDateTime ** property_value)
{
  xmlChar *prop_string;
  gchar *str;
  gint ret, pos;
  gint year, month, day, hour, minute;
  gdouble second;
  gboolean exists = FALSE;
  gfloat tzoffset = 0.0;
  gint gmt_offset_hour = -99, gmt_offset_min = -99;

  prop_string = xmlGetProp (a_node, (const xmlChar *) property_name);
  if (prop_string) {
    str = (gchar *) prop_string;
    GST_TRACE ("dateTime: %s, len %d", str, xmlStrlen (prop_string));
    /* parse year */
    ret = sscanf (str, "%d", &year);
    if (ret != 1 || year <= 0)
      goto error;
    pos = strcspn (str, "-");
    str += (pos + 1);
    GST_TRACE (" - year %d", year);
    /* parse month */
    ret = sscanf (str, "%d", &month);
    if (ret != 1 || month <= 0)
      goto error;
    pos = strcspn (str, "-");
    str += (pos + 1);
    GST_TRACE (" - month %d", month);
    /* parse day */
    ret = sscanf (str, "%d", &day);
    if (ret != 1 || day <= 0)
      goto error;
    pos = strcspn (str, "T");
    str += (pos + 1);
    GST_TRACE (" - day %d", day);
    /* parse hour */
    ret = sscanf (str, "%d", &hour);
    if (ret != 1 || hour < 0)
      goto error;
    pos = strcspn (str, ":");
    str += (pos + 1);
    GST_TRACE (" - hour %d", hour);
    /* parse minute */
    ret = sscanf (str, "%d", &minute);
    if (ret != 1 || minute < 0)
      goto error;
    pos = strcspn (str, ":");
    str += (pos + 1);
    GST_TRACE (" - minute %d", minute);
    /* parse second */
    ret = sscanf (str, "%lf", &second);
    if (ret != 1 || second < 0)
      goto error;
    GST_TRACE (" - second %lf", second);

    GST_LOG (" - %s: %4d/%02d/%02d %02d:%02d:%09.6lf", property_name,
        year, month, day, hour, minute, second);

    if (strrchr (str, '+') || strrchr (str, '-')) {
      /* reuse some code from gst-plugins-base/gst-libs/gst/tag/gstxmptag.c */
      gint gmt_offset = -1;
      gchar *plus_pos = NULL;
      gchar *neg_pos = NULL;
      gchar *pos = NULL;

      GST_LOG ("Checking for timezone information");

      /* check if there is timezone info */
      plus_pos = strrchr (str, '+');
      neg_pos = strrchr (str, '-');
      if (plus_pos)
        pos = plus_pos + 1;
      else if (neg_pos)
        pos = neg_pos + 1;

      if (pos && strlen (pos) >= 3) {
        gint ret_tz;
        if (pos[2] == ':')
          ret_tz = sscanf (pos, "%d:%d", &gmt_offset_hour, &gmt_offset_min);
        else
          ret_tz = sscanf (pos, "%02d%02d", &gmt_offset_hour, &gmt_offset_min);

        GST_DEBUG ("Parsing timezone: %s", pos);

        if (ret_tz == 2) {
          if (neg_pos != NULL && neg_pos + 1 == pos) {
            gmt_offset_hour *= -1;
            gmt_offset_min *= -1;
          }
          gmt_offset = gmt_offset_hour * 60 + gmt_offset_min;

          tzoffset = gmt_offset / 60.0;

          GST_LOG ("Timezone offset: %f (%d minutes)", tzoffset, gmt_offset);
        } else
          GST_WARNING ("Failed to parse timezone information");
      }
    }

    exists = TRUE;
    *property_value =
        gst_date_time_new (tzoffset, year, month, day, hour, minute, second);
    xmlFree (prop_string);
  }

  return exists;

error:
  GST_WARNING ("failed to parse property %s from xml string %s", property_name,
      prop_string);
  xmlFree (prop_string);
  return FALSE;
}

gboolean
gst_xml_helper_get_prop_duration (xmlNode * a_node,
    const gchar * property_name, guint64 default_value,
    guint64 * property_value)
{
  xmlChar *prop_string;
  gchar *str;
  gboolean exists = FALSE;

  *property_value = default_value;
  prop_string = xmlGetProp (a_node, (const xmlChar *) property_name);
  if (prop_string) {
    str = (gchar *) prop_string;
    if (!_mpd_helper_parse_duration (str, property_value))
      goto error;
    GST_LOG (" - %s: %" G_GUINT64_FORMAT, property_name, *property_value);
    xmlFree (prop_string);
    exists = TRUE;
  }
  return exists;

error:
  xmlFree (prop_string);
  return FALSE;
}

gboolean
gst_xml_helper_get_node_content (xmlNode * a_node, gchar ** content)
{
  xmlChar *node_content = NULL;
  gboolean exists = FALSE;

  node_content = xmlNodeGetContent (a_node);
  if (node_content) {
    exists = TRUE;
    *content = (gchar *) node_content;
    GST_LOG (" - %s: %s", a_node->name, *content);
  }

  return exists;
}

gboolean
gst_xml_helper_get_node_as_string (xmlNode * a_node, gchar ** content)
{
  gboolean exists = FALSE;
  const char *txt_encoding;
  xmlOutputBufferPtr out_buf;
  xmlNode *ncopy = NULL;

  txt_encoding = (const char *) a_node->doc->encoding;
  out_buf = xmlAllocOutputBuffer (NULL);
  g_assert (out_buf != NULL);

  /* Need to make a copy of XML element so that it includes namespaces
     in the output, so that the resulting string can be parsed by an XML parser
     that is namespace aware.
     Use extended=1 for recursive copy (properties, namespaces and children) */
  ncopy = xmlDocCopyNode (a_node, a_node->doc, 1);

  if (!ncopy) {
    GST_WARNING ("Failed to clone XML node");
    goto done;
  }
  xmlNodeDumpOutput (out_buf, ncopy->doc, ncopy, 0, 0, txt_encoding);

  (void) xmlOutputBufferFlush (out_buf);
#ifdef LIBXML2_NEW_BUFFER
  if (xmlOutputBufferGetSize (out_buf) > 0) {
    *content =
        (gchar *) xmlStrndup (xmlOutputBufferGetContent (out_buf),
        xmlOutputBufferGetSize (out_buf));
    exists = TRUE;
  }
#else
  if (out_buf->conv && out_buf->conv->use > 0) {
    *content =
        (gchar *) xmlStrndup (out_buf->conv->content, out_buf->conv->use);
    exists = TRUE;
  } else if (out_buf->buffer && out_buf->buffer->use > 0) {
    *content =
        (gchar *) xmlStrndup (out_buf->buffer->content, out_buf->buffer->use);
    exists = TRUE;
  }
#endif // LIBXML2_NEW_BUFFER
  xmlFreeNode (ncopy);
done:
  (void) xmlOutputBufferClose (out_buf);

  if (exists) {
    GST_LOG (" - %s: %s", a_node->name, *content);
  }
  return exists;
}

gchar *
gst_xml_helper_get_node_namespace (xmlNode * a_node, const gchar * prefix)
{
  xmlNs *curr_ns;
  gchar *namespace = NULL;

  if (prefix == NULL) {
    /* return the default namespace */
    if (a_node->ns) {
      namespace = xmlMemStrdup ((const gchar *) a_node->ns->href);
      if (namespace) {
        GST_LOG (" - default namespace: %s", namespace);
      }
    }
  } else {
    /* look for the specified prefix in the namespace list */
    for (curr_ns = a_node->ns; curr_ns; curr_ns = curr_ns->next) {
      if (xmlStrcmp (curr_ns->prefix, (xmlChar *) prefix) == 0) {
        namespace = xmlMemStrdup ((const gchar *) curr_ns->href);
        if (namespace) {
          GST_LOG (" - %s namespace: %s", curr_ns->prefix, curr_ns->href);
        }
      }
    }
  }

  return namespace;
}

gboolean
gst_xml_helper_get_prop_string_stripped (xmlNode * a_node,
    const gchar * property_name, gchar ** property_value)
{
  gboolean ret;
  ret = gst_xml_helper_get_prop_string (a_node, property_name, property_value);
  if (ret)
    *property_value = g_strstrip (*property_value);
  return ret;
}

gboolean
gst_xml_helper_get_prop_string_no_whitespace (xmlNode * a_node,
    const gchar * property_name, gchar ** property_value)
{
  return gst_xml_helper_get_prop_validated_string (a_node, property_name,
      property_value, _mpd_helper_validate_no_whitespace);
}


/* XML property set method */

void
gst_xml_helper_set_prop_string (xmlNodePtr node, const gchar * name,
    gchar * value)
{
  if (value)
    xmlSetProp (node, (xmlChar *) name, (xmlChar *) value);
}

void
gst_xml_helper_set_prop_boolean (xmlNodePtr node, const gchar * name,
    gboolean value)
{
  if (value)
    xmlSetProp (node, (xmlChar *) name, (xmlChar *) "true");
  else
    xmlSetProp (node, (xmlChar *) name, (xmlChar *) "false");
}

void
gst_xml_helper_set_prop_int (xmlNodePtr node, const gchar * name, gint value)
{
  gchar *text;
  text = g_strdup_printf ("%d", value);
  xmlSetProp (node, (xmlChar *) name, (xmlChar *) text);
  g_free (text);
}

void
gst_xml_helper_set_prop_uint (xmlNodePtr node, const gchar * name, guint value)
{
  gchar *text;
  text = g_strdup_printf ("%d", value);
  xmlSetProp (node, (xmlChar *) name, (xmlChar *) text);
  g_free (text);
}

void
gst_xml_helper_set_prop_int64 (xmlNodePtr node, const gchar * name,
    gint64 value)
{
  gchar *text;
  text = g_strdup_printf ("%" G_GINT64_FORMAT, value);
  xmlSetProp (node, (xmlChar *) name, (xmlChar *) text);
  g_free (text);
}

void
gst_xml_helper_set_prop_uint64 (xmlNodePtr node, const gchar * name,
    guint64 value)
{
  gchar *text;
  text = g_strdup_printf ("%" G_GUINT64_FORMAT, value);
  xmlSetProp (node, (xmlChar *) name, (xmlChar *) text);
  g_free (text);
}

void
gst_xml_helper_set_prop_double (xmlNodePtr node, const gchar * name,
    gdouble value)
{
  gchar *text;
  text = g_strdup_printf ("%lf", value);
  xmlSetProp (node, (xmlChar *) name, (xmlChar *) text);
  g_free (text);
}

void
gst_xml_helper_set_prop_uint_vector_type (xmlNode * node, const gchar * name,
    guint * value, guint value_size)
{
  int i;
  gchar *text = NULL;
  gchar *prev;
  gchar *temp;

  for (i = 0; i < value_size; i++) {
    temp = g_strdup_printf ("%d", value[i]);
    prev = text;
    text = g_strjoin (" ", text, prev, NULL);
    g_free (prev);
    g_free (temp);
  }

  if (text) {
    xmlSetProp (node, (xmlChar *) name, (xmlChar *) text);
    g_free (text);
  }
}

void
gst_xml_helper_set_prop_date_time (xmlNodePtr node, const gchar * name,
    GstDateTime * value)
{
  gchar *text;
  if (value) {
    text = gst_date_time_to_iso8601_string (value);
    xmlSetProp (node, (xmlChar *) name, (xmlChar *) text);
    g_free (text);
  }
}

void
gst_xml_helper_set_prop_duration (xmlNode * node, const gchar * name,
    guint64 value)
{
  gchar *text;
  gint years, months, days, hours, minutes, seconds, milliseconds;
  if (value) {
    years = (gint) (XML_HELPER_MS_TO_SEC (value) / (XML_HELPER_YEAR_TO_SEC));
    months =
        (gint) ((XML_HELPER_MS_TO_SEC (value) % XML_HELPER_YEAR_TO_SEC) /
        XML_HELPER_MONTH_TO_SEC);
    days =
        (gint) ((XML_HELPER_MS_TO_SEC (value) % XML_HELPER_MONTH_TO_SEC) /
        XML_HELPER_DAY_TO_SEC);
    hours =
        (gint) ((XML_HELPER_MS_TO_SEC (value) % XML_HELPER_DAY_TO_SEC) /
        XML_HELPER_HOUR_TO_SEC);
    minutes =
        (gint) ((XML_HELPER_MS_TO_SEC (value) % XML_HELPER_HOUR_TO_SEC) /
        XML_HELPER_MINUTE_TO_SEC);
    seconds = (gint) (XML_HELPER_MS_TO_SEC (value) % XML_HELPER_MINUTE_TO_SEC);
    milliseconds = value % 1000;

    text =
        g_strdup_printf ("P%dY%dM%dDT%dH%dM%d.%dS", years, months, days, hours,
        minutes, seconds, milliseconds);
    GST_LOG ("duration %" G_GUINT64_FORMAT " -> %s", value, text);
    xmlSetProp (node, (xmlChar *) name, (xmlChar *) text);
    g_free (text);
  }
}

void
gst_xml_helper_set_prop_ratio (xmlNodePtr node, const gchar * name,
    GstXMLRatio * value)
{
  gchar *text;
  if (value) {
    text = g_strdup_printf ("%d:%d", value->num, value->den);
    xmlSetProp (node, (xmlChar *) name, (xmlChar *) text);
    g_free (text);
  }
}


void
gst_xml_helper_set_prop_framerate (xmlNodePtr node, const gchar * name,
    GstXMLFrameRate * value)
{
  gchar *text;
  if (value) {
    text = g_strdup_printf ("%d/%d", value->num, value->den);
    xmlSetProp (node, (xmlChar *) name, (xmlChar *) text);
    g_free (text);
  }
}

void
gst_xml_helper_set_prop_range (xmlNodePtr node, const gchar * name,
    GstXMLRange * value)
{
  gchar *text;
  if (value) {
    text =
        g_strdup_printf ("%" G_GUINT64_FORMAT "-%" G_GUINT64_FORMAT,
        value->first_byte_pos, value->last_byte_pos);
    xmlSetProp (node, (xmlChar *) name, (xmlChar *) text);
    g_free (text);
  }
}

void
gst_xml_helper_set_prop_cond_uint (xmlNodePtr node, const gchar * name,
    GstXMLConditionalUintType * cond)
{
  gchar *text;
  if (cond) {
    if (cond->flag)
      if (cond->value)
        text = g_strdup_printf ("%d", cond->value);
      else
        text = g_strdup_printf ("%s", "true");
    else
      text = g_strdup_printf ("%s", "false");

    xmlSetProp (node, (xmlChar *) name, (xmlChar *) text);
    g_free (text);
  }
}

void
gst_xml_helper_set_content (xmlNodePtr node, gchar * content)
{
  if (content)
    xmlNodeSetContent (node, (xmlChar *) content);
}
