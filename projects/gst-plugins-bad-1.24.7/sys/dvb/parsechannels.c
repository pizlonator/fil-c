/*
 * parsechannels.c -
 * Copyright (C) 2008 Zaheer Abbas Merali
 *
 * Authors:
 *   Zaheer Abbas Merali <zaheerabbas at merali dot org>
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
#include <glib-object.h>
#include <stdlib.h>
#include <string.h>
#include <gst/gst.h>

#include <glib/gi18n-lib.h>

#include "parsechannels.h"

#include <linux/dvb/frontend.h>

GST_DEBUG_CATEGORY_EXTERN (dvb_base_bin_debug);
#define GST_CAT_DEFAULT dvb_base_bin_debug

typedef enum
{
  CHANNEL_CONF_FORMAT_NONE,
  CHANNEL_CONF_FORMAT_DVBV5,
  CHANNEL_CONF_FORMAT_ZAP
} GstDvbChannelConfFormat;

typedef gboolean (*GstDvbV5ChannelsConfPropSetFunction) (GstElement *
    dvbbasebin, const gchar * property, GKeyFile * kf,
    const gchar * channel_name, const gchar * key);

typedef struct
{
  const gchar *conf_property;
  const gchar *elem_property;
  GstDvbV5ChannelsConfPropSetFunction set_func;
} GstDvbV5ChannelsConfToPropertyMap;

static gboolean parse_and_configure_from_v5_conf_file (GstElement * dvbbasebin,
    const gchar * filename, const gchar * channel_name, GError ** error);
static gboolean parse_and_configure_from_zap_conf_file (GstElement * dvbbasebin,
    const gchar * filename, const gchar * channel_name, GError ** error);
static GstDvbChannelConfFormat detect_file_format (const gchar * filename);

static gboolean gst_dvb_base_bin_conf_set_string (GstElement * dvbbasebin,
    const gchar * property, GKeyFile * kf, const gchar * channel_name,
    const gchar * key);
static gboolean gst_dvb_base_bin_conf_set_uint (GstElement * dvbbasebin,
    const gchar * property, GKeyFile * kf, const gchar * channel_name,
    const gchar * key);
static gboolean gst_dvb_base_bin_conf_set_int (GstElement * dvbbasebin,
    const gchar * property, GKeyFile * kf, const gchar * channel_name,
    const gchar * key);
static gboolean gst_dvb_base_bin_conf_set_inversion (GstElement * dvbbasebin,
    const gchar * property, GKeyFile * kf, const gchar * channel_name,
    const gchar * key);
static gboolean gst_dvb_base_bin_conf_set_guard (GstElement * dvbbasebin,
    const gchar * property, GKeyFile * kf, const gchar * channel_name,
    const gchar * key);
static gboolean gst_dvb_base_bin_conf_set_trans_mode (GstElement * dvbbasebin,
    const gchar * property, GKeyFile * kf, const gchar * channel_name,
    const gchar * key);
static gboolean gst_dvb_base_bin_conf_set_code_rate (GstElement * dvbbasebin,
    const gchar * property, GKeyFile * kf, const gchar * channel_name,
    const gchar * key);
static gboolean gst_dvb_base_bin_conf_set_delsys (GstElement * dvbbasebin,
    const gchar * property, GKeyFile * kf, const gchar * channel_name,
    const gchar * key);
static gboolean gst_dvb_base_bin_conf_set_hierarchy (GstElement * dvbbasebin,
    const gchar * property, GKeyFile * kf, const gchar * channel_name,
    const gchar * key);
static gboolean gst_dvb_base_bin_conf_set_modulation (GstElement * dvbbasebin,
    const gchar * property, GKeyFile * kf, const gchar * channel_name,
    const gchar * key);
static GHashTable *parse_channels_conf_from_zap_file (GstElement * dvbbasebin,
    const gchar * filename, GError ** error);
static gboolean remove_channel_from_hash (gpointer key, gpointer value,
    gpointer user_data);
static void destroy_channels_hash (GHashTable * channels);

GstDvbV5ChannelsConfToPropertyMap dvbv5_prop_map[] = {
  {"SERVICE_ID", "program-numbers", gst_dvb_base_bin_conf_set_string},
  {"FREQUENCY", "frequency", gst_dvb_base_bin_conf_set_uint},
  {"BANDWIDTH_HZ", "bandwidth-hz", gst_dvb_base_bin_conf_set_uint},
  {"INVERSION", "inversion", gst_dvb_base_bin_conf_set_inversion},
  {"GUARD_INTERVAL", "guard", gst_dvb_base_bin_conf_set_guard},
  {"TRANSMISSION_MODE", "trans-mode", gst_dvb_base_bin_conf_set_trans_mode},
  {"HIERARCHY", "hierarchy", gst_dvb_base_bin_conf_set_hierarchy},
  {"MODULATION", "modulation", gst_dvb_base_bin_conf_set_modulation},
  {"CODE_RATE_HP", "code-rate-hp", gst_dvb_base_bin_conf_set_code_rate},
  {"CODE_RATE_LP", "code-rate-lp", gst_dvb_base_bin_conf_set_code_rate},
  {"ISDBT_LAYER_ENABLED", "isdbt-layer-enabled",
      gst_dvb_base_bin_conf_set_uint},
  {"ISDBT_PARTIAL_RECEPTION", "isdbt-partial-reception",
      gst_dvb_base_bin_conf_set_int},
  {"ISDBT_SOUND_BROADCASTING", "isdbt-sound-broadcasting",
      gst_dvb_base_bin_conf_set_int},
  {"ISDBT_SB_SUBCHANNEL_ID", "isdbt-sb-subchannel-id",
      gst_dvb_base_bin_conf_set_int},
  {"ISDBT_SB_SEGMENT_IDX", "isdbt-sb-segment-idx",
      gst_dvb_base_bin_conf_set_int},
  {"ISDBT_SB_SEGMENT_COUNT", "isdbt-sb-segment-count", gst_dvb_base_bin_conf_set_int},  /* Range in files start from 0, property starts from 1 */
  {"ISDBT_LAYERA_FEC", "isdbt-layera-fec", gst_dvb_base_bin_conf_set_code_rate},
  {"ISDBT_LAYERA_MODULATION", "isdbt-layera-modulation",
      gst_dvb_base_bin_conf_set_modulation},
  {"ISDBT_LAYERA_SEGMENT_COUNT", "isdbt-layera-segment-count",
      gst_dvb_base_bin_conf_set_int},
  {"ISDBT_LAYERA_TIME_INTERLEAVING", "isdbt-layera-time-interleaving",
      gst_dvb_base_bin_conf_set_int},
  {"ISDBT_LAYERB_FEC", "isdbt-layerb-fec", gst_dvb_base_bin_conf_set_code_rate},
  {"ISDBT_LAYERB_MODULATION", "isdbt-layerb-modulation",
      gst_dvb_base_bin_conf_set_modulation},
  {"ISDBT_LAYERB_SEGMENT_COUNT", "isdbt-layerb-segment-count",
      gst_dvb_base_bin_conf_set_int},
  {"ISDBT_LAYERB_TIME_INTERLEAVING", "isdbt-layerb-time-interleaving",
      gst_dvb_base_bin_conf_set_int},
  {"ISDBT_LAYERC_FEC", "isdbt-layerc-fec", gst_dvb_base_bin_conf_set_code_rate},
  {"ISDBT_LAYERC_MODULATION", "isdbt-layerc-modulation",
      gst_dvb_base_bin_conf_set_modulation},
  {"ISDBT_LAYERC_SEGMENT_COUNT", "isdbt-layerc-segment-count",
      gst_dvb_base_bin_conf_set_int},
  {"ISDBT_LAYERC_TIME_INTERLEAVING", "isdbt-layerc-time-interleaving",
      gst_dvb_base_bin_conf_set_int},
  {"DELIVERY_SYSTEM", "delsys", gst_dvb_base_bin_conf_set_delsys},
  {NULL,}
};

/* TODO:
 * Store the channels hash table around instead of constantly parsing it
 * Detect when the file changed on disk
 */

static gint
gst_dvb_base_bin_find_string_in_array (const gchar ** array, const gchar * str)
{
  gint i = 0;
  const gchar *cur;
  while ((cur = array[i])) {
    if (strcmp (cur, str) == 0)
      return i;

    i++;
  }

  return -1;
}

static gboolean
gst_dvb_base_bin_conf_set_property_from_string_array (GstElement * dvbbasebin,
    const gchar * property, GKeyFile * kf, const gchar * channel_name,
    const gchar * key, const gchar ** strings, gint default_value)
{
  gchar *str;
  gint v;

  str = g_key_file_get_string (kf, channel_name, key, NULL);
  v = gst_dvb_base_bin_find_string_in_array (strings, str);
  if (v == -1) {
    GST_WARNING_OBJECT (dvbbasebin, "Unexpected value '%s' for property "
        "'%s', using default: '%d'", str, property, default_value);
    v = default_value;
  }

  g_free (str);
  g_object_set (dvbbasebin, property, v, NULL);
  return TRUE;
}

static gboolean
gst_dvb_base_bin_conf_set_string (GstElement * dvbbasebin,
    const gchar * property, GKeyFile * kf, const gchar * channel_name,
    const gchar * key)
{
  gchar *str;

  str = g_key_file_get_string (kf, channel_name, key, NULL);
  if (!str) {
    GST_WARNING_OBJECT (dvbbasebin,
        "Could not get value for '%s' on channel '%s'", key, channel_name);
    return FALSE;
  }

  g_object_set (dvbbasebin, property, str, NULL);
  g_free (str);
  return TRUE;
}

static gboolean
gst_dvb_base_bin_conf_set_uint (GstElement * dvbbasebin, const gchar * property,
    GKeyFile * kf, const gchar * channel_name, const gchar * key)
{
  guint64 v;
  GError *err = NULL;

  v = g_key_file_get_uint64 (kf, channel_name, key, &err);
  if (err != NULL) {
    GST_WARNING_OBJECT (dvbbasebin,
        "Could not get value for '%s' on channel '%s' error: '%s'", key,
        channel_name, err->message);
    g_error_free (err);
    return FALSE;
  }

  g_object_set (dvbbasebin, property, (guint) v, NULL);
  return TRUE;
}

static gboolean
gst_dvb_base_bin_conf_set_int (GstElement * dvbbasebin, const gchar * property,
    GKeyFile * kf, const gchar * channel_name, const gchar * key)
{
  gint v;
  GError *err = NULL;

  v = g_key_file_get_integer (kf, channel_name, key, &err);
  if (err != NULL) {
    GST_WARNING_OBJECT (dvbbasebin,
        "Could not get value for '%s' on channel '%s' error: '%s'", key,
        channel_name, err->message);
    g_error_free (err);
    return FALSE;
  }

  g_object_set (dvbbasebin, property, v, NULL);
  return TRUE;
}

static gboolean
gst_dvb_base_bin_conf_set_inversion (GstElement * dvbbasebin,
    const gchar * property, GKeyFile * kf, const gchar * channel_name,
    const gchar * key)
{
  gchar *str;
  gint v;

  str = g_key_file_get_string (kf, channel_name, key, NULL);
  if (!str) {
    GST_WARNING_OBJECT (dvbbasebin,
        "Could not get value for '%s' on channel '%s'", key, channel_name);
    return FALSE;
  }

  if (strcmp (str, "AUTO") == 0)
    v = 2;
  else if (strcmp (str, "ON") == 0)
    v = 1;
  else
    v = 0;                      /* OFF */

  g_free (str);
  g_object_set (dvbbasebin, property, v, NULL);
  return TRUE;
}

static gboolean
gst_dvb_base_bin_conf_set_guard (GstElement * dvbbasebin,
    const gchar * property, GKeyFile * kf, const gchar * channel_name,
    const gchar * key)
{
  const gchar *guards[] = {
    "1/32", "1/16", "1/8", "1/4", "auto",
    "1/128", "19/128", "19/256",
    "PN420", "PN595", "PN945", NULL
  };
  return gst_dvb_base_bin_conf_set_property_from_string_array (dvbbasebin,
      property, kf, channel_name, key, guards, 4);
}

static gboolean
gst_dvb_base_bin_conf_set_trans_mode (GstElement * dvbbasebin,
    const gchar * property, GKeyFile * kf, const gchar * channel_name,
    const gchar * key)
{
  const gchar *trans_modes[] = {
    "2K", "8K", "AUTO", "4K", "1K",
    "16K", "32K", "C1", "C3780", NULL
  };
  return gst_dvb_base_bin_conf_set_property_from_string_array (dvbbasebin,
      property, kf, channel_name, key, trans_modes, 2);
}

static gboolean
gst_dvb_base_bin_conf_set_code_rate (GstElement * dvbbasebin,
    const gchar * property, GKeyFile * kf, const gchar * channel_name,
    const gchar * key)
{
  const gchar *code_rates[] = {
    "NONE", "1/2", "2/3", "3/4", "4/5",
    "5/6", "6/7", "7/8", "8/9", "AUTO",
    "3/5", "9/10", "2/5", NULL
  };
  return gst_dvb_base_bin_conf_set_property_from_string_array (dvbbasebin,
      property, kf, channel_name, key, code_rates, 9);
}

static gboolean
gst_dvb_base_bin_conf_set_delsys (GstElement * dvbbasebin,
    const gchar * property, GKeyFile * kf, const gchar * channel_name,
    const gchar * key)
{
  const gchar *delsys[] = {
    "UNDEFINED", "DVBCA", "DVBCB", "DVBT", "DSS",
    "DVBS", "DVBS2", "DVBH", "ISDBT", "ISDBS",
    "ISDBC", "ATSC", "ATSCMH", "DTMB", "CMMB",
    "DAB", "DVBT2", "TURBO", "DVBCC", NULL
  };
  return gst_dvb_base_bin_conf_set_property_from_string_array (dvbbasebin,
      property, kf, channel_name, key, delsys, 0);
}

static gboolean
gst_dvb_base_bin_conf_set_hierarchy (GstElement * dvbbasebin,
    const gchar * property, GKeyFile * kf, const gchar * channel_name,
    const gchar * key)
{
  const gchar *hierarchies[] = {
    "NONE", "1", "2", "4", "AUTO", NULL
  };
  return gst_dvb_base_bin_conf_set_property_from_string_array (dvbbasebin,
      property, kf, channel_name, key, hierarchies, 4);
}

static gboolean
gst_dvb_base_bin_conf_set_modulation (GstElement * dvbbasebin,
    const gchar * property, GKeyFile * kf, const gchar * channel_name,
    const gchar * key)
{
  const gchar *modulations[] = {
    "QPSK", "QAM/16", "QAM/32", "QAM/64",
    "QAM/128", "QAM/256", "QAM/AUTO", "VSB/8",
    "VSB/16", "PSK/8", "APSK/16", "APSK/32",
    "DQPSK", "QAM/4_NR", NULL
  };
  return gst_dvb_base_bin_conf_set_property_from_string_array (dvbbasebin,
      property, kf, channel_name, key, modulations, 6);
}

/* FIXME: is channel_name guaranteed to be ASCII or UTF-8? */
static gboolean
parse_and_configure_from_v5_conf_file (GstElement * dvbbasebin,
    const gchar * filename, const gchar * channel_name, GError ** error)
{
  GKeyFile *keyfile;
  gchar **keys, **keys_p;
  GError *err = NULL;

  keyfile = g_key_file_new ();
  if (!g_key_file_load_from_file (keyfile, filename, G_KEY_FILE_NONE, &err))
    goto load_error;

  if (!g_key_file_has_group (keyfile, channel_name))
    goto unknown_channel;

  keys = g_key_file_get_keys (keyfile, channel_name, NULL, &err);
  if (!keys)
    goto no_properties;

  keys_p = keys;
  while (*keys_p) {
    const gchar *k = *keys_p;
    const GstDvbV5ChannelsConfToPropertyMap *map_entry = dvbv5_prop_map;
    gboolean property_found = FALSE;

    GST_LOG_OBJECT (dvbbasebin, "Setting property %s", k);

    while (map_entry->conf_property) {
      if (strcmp (map_entry->conf_property, k) == 0) {
        if (!map_entry->set_func (dvbbasebin, map_entry->elem_property, keyfile,
                channel_name, k))
          goto property_error;
        property_found = TRUE;
        break;
      }
      map_entry++;
    }

    if (!property_found)
      GST_WARNING_OBJECT (dvbbasebin, "Failed to map property '%s'", k);

    keys_p++;
  }

  GST_DEBUG_OBJECT (dvbbasebin, "Successfully parsed channel configuration "
      "file '%s'", filename);
  g_strfreev (keys);
  g_key_file_unref (keyfile);
  return TRUE;

load_error:
  if ((err->domain == G_FILE_ERROR && err->code == G_FILE_ERROR_NOENT) ||
      (err->domain == G_KEY_FILE_ERROR
          && err->code == G_KEY_FILE_ERROR_NOT_FOUND)) {
    g_set_error (error, GST_RESOURCE_ERROR, GST_RESOURCE_ERROR_NOT_FOUND,
        _("Couldn't find channel configuration file"));
  } else {
    g_set_error (error, GST_RESOURCE_ERROR, GST_RESOURCE_ERROR_READ,
        _("Couldn't load channel configuration file: '%s'"), err->message);
  }
  g_clear_error (&err);
  return FALSE;

unknown_channel:
  {
    g_set_error (error, GST_RESOURCE_ERROR, GST_RESOURCE_ERROR_NOT_FOUND,
        _("Couldn't find details for channel '%s'"), channel_name);
    g_key_file_unref (keyfile);
    g_clear_error (&err);
    return FALSE;
  }

no_properties:
  {
    g_set_error (error, GST_RESOURCE_ERROR, GST_RESOURCE_ERROR_NOT_FOUND,
        _("No properties for channel '%s'"), channel_name);
    g_key_file_unref (keyfile);
    g_clear_error (&err);
    return FALSE;
  }

property_error:
  {
    g_set_error (error, GST_RESOURCE_ERROR, GST_RESOURCE_ERROR_FAILED,
        _("Failed to set properties for channel '%s'"), channel_name);
    g_key_file_unref (keyfile);
    g_clear_error (&err);
    return FALSE;
  }
}

static GHashTable *
parse_channels_conf_from_zap_file (GstElement * dvbbasebin,
    const gchar * filename, GError ** error)
{
  gchar *contents;
  gchar **lines;
  gchar *line;
  gchar **fields;
  int i, parsedchannels = 0;
  GHashTable *res;
  GError *err = NULL;
  const gchar *terrestrial[] = { "inversion", "bandwidth",
    "code-rate-hp", "code-rate-lp", "modulation", "transmission-mode",
    "guard", "hierarchy"
  };
  const gchar *satellite[] = { "polarity", "diseqc-source",
    "symbol-rate"
  };
  const gchar *cable[] = { "inversion", "symbol-rate", "code-rate-hp",
    "modulation"
  };

  GST_INFO_OBJECT (dvbbasebin, "parsing '%s'", filename);

  if (!g_file_get_contents (filename, &contents, NULL, &err))
    goto open_fail;

  lines = g_strsplit (contents, "\n", 0);
  res = g_hash_table_new (g_str_hash, g_str_equal);

  i = 0;
  line = lines[0];
  while (line != NULL) {
    GHashTable *params;
    int j, numfields;

    if (line[0] == '#')
      goto next_line;

    params = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
    fields = g_strsplit (line, ":", 0);
    numfields = g_strv_length (fields);

    switch (numfields) {
      case 13:                 /* terrestrial */
        g_hash_table_insert (params, g_strdup ("type"),
            g_strdup ("terrestrial"));
        for (j = 2; j <= 9; j++) {
          g_hash_table_insert (params, g_strdup (terrestrial[j - 2]),
              g_strdup (fields[j]));
        }
        g_hash_table_insert (params, g_strdup ("frequency"),
            g_strdup (fields[1]));
        break;
      case 9:                  /* cable */
        g_hash_table_insert (params, g_strdup ("type"), g_strdup ("cable"));
        for (j = 2; j <= 5; j++) {
          g_hash_table_insert (params, g_strdup (cable[j - 2]),
              g_strdup (fields[j]));
        }
        g_hash_table_insert (params, g_strdup ("frequency"),
            g_strdup (fields[1]));
        break;
      case 8:                  /* satellite */
        g_hash_table_insert (params, g_strdup ("type"), g_strdup ("satellite"));
        for (j = 2; j <= 4; j++) {
          g_hash_table_insert (params, g_strdup (satellite[j - 2]),
              g_strdup (fields[j]));
        }
        /* Some ZAP format variations store freqs in MHz
         * but we internally use kHz for DVB-S/S2. */
        if (strlen (fields[1]) < 6) {
          g_hash_table_insert (params, g_strdup ("frequency"),
              g_strdup_printf ("%d", atoi (fields[1]) * 1000));
        } else {
          g_hash_table_insert (params, g_strdup ("frequency"),
              g_strdup_printf ("%d", atoi (fields[1])));
        }
        break;
      case 6:                  /* atsc (vsb/qam) */
        g_hash_table_insert (params, g_strdup ("type"), g_strdup ("atsc"));
        g_hash_table_insert (params, g_strdup ("modulation"),
            g_strdup (fields[2]));

        g_hash_table_insert (params, g_strdup ("frequency"),
            g_strdup (fields[1]));
        break;
      default:
        goto not_parsed;
    }

    /* parsed */
    g_hash_table_insert (params, g_strdup ("sid"),
        g_strdup (fields[numfields - 1]));
    g_hash_table_insert (res, g_strdup (fields[0]), params);
    parsedchannels++;

  not_parsed:
    g_strfreev (fields);
  next_line:
    line = lines[++i];
  }

  g_strfreev (lines);
  g_free (contents);

  if (parsedchannels == 0)
    goto no_channels;

  return res;

open_fail:
  if (err->code == G_FILE_ERROR_NOENT) {
    g_set_error (error, GST_RESOURCE_ERROR, GST_RESOURCE_ERROR_NOT_FOUND,
        _("Couldn't find channel configuration file: '%s'"), err->message);
  } else {
    g_set_error (error, GST_RESOURCE_ERROR, GST_RESOURCE_ERROR_READ,
        _("Couldn't load channel configuration file: '%s'"), err->message);
  }
  g_clear_error (&err);
  return NULL;

no_channels:
  g_set_error (error, GST_RESOURCE_ERROR, GST_RESOURCE_ERROR_FAILED,
      _("Channel configuration file doesn't contain any channels"));
  g_hash_table_unref (res);
  return NULL;
}

static gboolean
remove_channel_from_hash (gpointer key, gpointer value, gpointer user_data)
{
  g_free (key);
  if (value)
    g_hash_table_destroy ((GHashTable *) value);
  return TRUE;
}

static void
destroy_channels_hash (GHashTable * channels)
{
  g_hash_table_foreach_remove (channels, remove_channel_from_hash, NULL);
}

/* FIXME: is channel_name guaranteed to be ASCII or UTF-8? */
static gboolean
parse_and_configure_from_zap_conf_file (GstElement * dvbbasebin,
    const gchar * filename, const gchar * channel_name, GError ** error)
{
  gboolean ret = FALSE;
  GHashTable *channels, *params;
  gchar *type;

  /* Assumptions are made here about a format that is loosely
   * defined. Particularly, we assume a given delivery system
   * out of counting the number of fields per line. dvbsrc has
   * smarter code to auto-detect a delivery system based on
   * known-correct combinations of parameters so if you ever
   * encounter cases where the delivery system is being
   * wrongly set here, just remove the offending
   * g_object_set line and let dvbsrc work his magic out. */

  channels = parse_channels_conf_from_zap_file (dvbbasebin, filename, error);

  if (!channels)
    goto beach;

  params = g_hash_table_lookup (channels, channel_name);

  if (!params)
    goto unknown_channel;

  g_object_set (dvbbasebin, "program-numbers",
      g_hash_table_lookup (params, "sid"), NULL);
  /* check if it is terrestrial or satellite */
  g_object_set (dvbbasebin, "frequency",
      atoi (g_hash_table_lookup (params, "frequency")), NULL);
  type = g_hash_table_lookup (params, "type");
  if (strcmp (type, "terrestrial") == 0) {
    gchar *val;

    val = g_hash_table_lookup (params, "inversion");
    if (strcmp (val, "INVERSION_OFF") == 0)
      g_object_set (dvbbasebin, "inversion", INVERSION_OFF, NULL);
    else if (strcmp (val, "INVERSION_ON") == 0)
      g_object_set (dvbbasebin, "inversion", INVERSION_ON, NULL);
    else
      g_object_set (dvbbasebin, "inversion", INVERSION_AUTO, NULL);

    val = g_hash_table_lookup (params, "bandwidth");
    if (strcmp (val, "BANDWIDTH_8_MHZ") == 0)
      g_object_set (dvbbasebin, "bandwidth", 0, NULL);
    else if (strcmp (val, "BANDWIDTH_7_MHZ") == 0)
      g_object_set (dvbbasebin, "bandwidth", 1, NULL);
    else if (strcmp (val, "BANDWIDTH_6_MHZ") == 0)
      g_object_set (dvbbasebin, "bandwidth", 2, NULL);
    else if (strcmp (val, "BANDWIDTH_5_MHZ") == 0)
      g_object_set (dvbbasebin, "bandwidth", 4, NULL);
    else if (strcmp (val, "BANDWIDTH_10_MHZ") == 0)
      g_object_set (dvbbasebin, "bandwidth", 5, NULL);
    else if (strcmp (val, "BANDWIDTH_1_712_MHZ") == 0)
      g_object_set (dvbbasebin, "bandwidth", 6, NULL);
    else
      g_object_set (dvbbasebin, "bandwidth", 3, NULL);

    val = g_hash_table_lookup (params, "code-rate-hp");
    if (strcmp (val, "FEC_NONE") == 0)
      g_object_set (dvbbasebin, "code-rate-hp", 0, NULL);
    else if (strcmp (val, "FEC_1_2") == 0)
      g_object_set (dvbbasebin, "code-rate-hp", 1, NULL);
    else if (strcmp (val, "FEC_2_3") == 0)
      g_object_set (dvbbasebin, "code-rate-hp", 2, NULL);
    else if (strcmp (val, "FEC_3_4") == 0)
      g_object_set (dvbbasebin, "code-rate-hp", 3, NULL);
    else if (strcmp (val, "FEC_4_5") == 0)
      g_object_set (dvbbasebin, "code-rate-hp", 4, NULL);
    else if (strcmp (val, "FEC_5_6") == 0)
      g_object_set (dvbbasebin, "code-rate-hp", 5, NULL);
    else if (strcmp (val, "FEC_6_7") == 0)
      g_object_set (dvbbasebin, "code-rate-hp", 6, NULL);
    else if (strcmp (val, "FEC_7_8") == 0)
      g_object_set (dvbbasebin, "code-rate-hp", 7, NULL);
    else if (strcmp (val, "FEC_8_9") == 0)
      g_object_set (dvbbasebin, "code-rate-hp", 8, NULL);
    else
      g_object_set (dvbbasebin, "code-rate-hp", 9, NULL);

    val = g_hash_table_lookup (params, "code-rate-lp");
    if (strcmp (val, "FEC_NONE") == 0)
      g_object_set (dvbbasebin, "code-rate-lp", 0, NULL);
    else if (strcmp (val, "FEC_1_2") == 0)
      g_object_set (dvbbasebin, "code-rate-lp", 1, NULL);
    else if (strcmp (val, "FEC_2_3") == 0)
      g_object_set (dvbbasebin, "code-rate-lp", 2, NULL);
    else if (strcmp (val, "FEC_3_4") == 0)
      g_object_set (dvbbasebin, "code-rate-lp", 3, NULL);
    else if (strcmp (val, "FEC_4_5") == 0)
      g_object_set (dvbbasebin, "code-rate-lp", 4, NULL);
    else if (strcmp (val, "FEC_5_6") == 0)
      g_object_set (dvbbasebin, "code-rate-lp", 5, NULL);
    else if (strcmp (val, "FEC_6_7") == 0)
      g_object_set (dvbbasebin, "code-rate-lp", 6, NULL);
    else if (strcmp (val, "FEC_7_8") == 0)
      g_object_set (dvbbasebin, "code-rate-lp", 7, NULL);
    else if (strcmp (val, "FEC_8_9") == 0)
      g_object_set (dvbbasebin, "code-rate-lp", 8, NULL);
    else
      g_object_set (dvbbasebin, "code-rate-lp", 9, NULL);

    val = g_hash_table_lookup (params, "modulation");
    if (strcmp (val, "QPSK") == 0)
      g_object_set (dvbbasebin, "modulation", 0, NULL);
    else if (strcmp (val, "QAM_16") == 0)
      g_object_set (dvbbasebin, "modulation", 1, NULL);
    else if (strcmp (val, "QAM_32") == 0)
      g_object_set (dvbbasebin, "modulation", 2, NULL);
    else if (strcmp (val, "QAM_64") == 0)
      g_object_set (dvbbasebin, "modulation", 3, NULL);
    else if (strcmp (val, "QAM_128") == 0)
      g_object_set (dvbbasebin, "modulation", 4, NULL);
    else if (strcmp (val, "QAM_256") == 0)
      g_object_set (dvbbasebin, "modulation", 5, NULL);
    else
      g_object_set (dvbbasebin, "modulation", 6, NULL);

    val = g_hash_table_lookup (params, "transmission-mode");
    if (strcmp (val, "TRANSMISSION_MODE_2K") == 0)
      g_object_set (dvbbasebin, "trans-mode", 0, NULL);
    else if (strcmp (val, "TRANSMISSION_MODE_8K") == 0)
      g_object_set (dvbbasebin, "trans-mode", 1, NULL);
    else
      g_object_set (dvbbasebin, "trans-mode", 2, NULL);

    val = g_hash_table_lookup (params, "guard");
    if (strcmp (val, "GUARD_INTERVAL_1_32") == 0)
      g_object_set (dvbbasebin, "guard", 0, NULL);
    else if (strcmp (val, "GUARD_INTERVAL_1_16") == 0)
      g_object_set (dvbbasebin, "guard", 1, NULL);
    else if (strcmp (val, "GUARD_INTERVAL_1_8") == 0)
      g_object_set (dvbbasebin, "guard", 2, NULL);
    else if (strcmp (val, "GUARD_INTERVAL_1_4") == 0)
      g_object_set (dvbbasebin, "guard", 3, NULL);
    else
      g_object_set (dvbbasebin, "guard", 4, NULL);

    val = g_hash_table_lookup (params, "hierarchy");
    if (strcmp (val, "HIERARCHY_NONE") == 0)
      g_object_set (dvbbasebin, "hierarchy", 0, NULL);
    else if (strcmp (val, "HIERARCHY_1") == 0)
      g_object_set (dvbbasebin, "hierarchy", 1, NULL);
    else if (strcmp (val, "HIERARCHY_2") == 0)
      g_object_set (dvbbasebin, "hierarchy", 2, NULL);
    else if (strcmp (val, "HIERARCHY_4") == 0)
      g_object_set (dvbbasebin, "hierarchy", 3, NULL);
    else
      g_object_set (dvbbasebin, "hierarchy", 4, NULL);

    ret = TRUE;
  } else if (strcmp (type, "satellite") == 0) {
    gchar *val;

    ret = TRUE;

    g_object_set (dvbbasebin, "delsys", SYS_DVBS, NULL);

    val = g_hash_table_lookup (params, "polarity");
    if (val)
      g_object_set (dvbbasebin, "polarity", val, NULL);
    else
      ret = FALSE;

    val = g_hash_table_lookup (params, "diseqc-source");
    if (val)
      g_object_set (dvbbasebin, "diseqc-source", atoi (val), NULL);

    val = g_hash_table_lookup (params, "symbol-rate");
    if (val)
      g_object_set (dvbbasebin, "symbol-rate", atoi (val), NULL);
    else
      ret = FALSE;
  } else if (strcmp (type, "cable") == 0) {
    gchar *val;

    g_object_set (dvbbasebin, "delsys", SYS_DVBC_ANNEX_A, NULL);

    ret = TRUE;
    val = g_hash_table_lookup (params, "symbol-rate");
    if (val)
      g_object_set (dvbbasebin, "symbol-rate", atoi (val) / 1000, NULL);
    val = g_hash_table_lookup (params, "modulation");
    if (strcmp (val, "QPSK") == 0)
      g_object_set (dvbbasebin, "modulation", 0, NULL);
    else if (strcmp (val, "QAM_16") == 0)
      g_object_set (dvbbasebin, "modulation", 1, NULL);
    else if (strcmp (val, "QAM_32") == 0)
      g_object_set (dvbbasebin, "modulation", 2, NULL);
    else if (strcmp (val, "QAM_64") == 0)
      g_object_set (dvbbasebin, "modulation", 3, NULL);
    else if (strcmp (val, "QAM_128") == 0)
      g_object_set (dvbbasebin, "modulation", 4, NULL);
    else if (strcmp (val, "QAM_256") == 0)
      g_object_set (dvbbasebin, "modulation", 5, NULL);
    else
      g_object_set (dvbbasebin, "modulation", 6, NULL);
    val = g_hash_table_lookup (params, "code-rate-hp");
    if (strcmp (val, "FEC_NONE") == 0)
      g_object_set (dvbbasebin, "code-rate-hp", 0, NULL);
    else if (strcmp (val, "FEC_1_2") == 0)
      g_object_set (dvbbasebin, "code-rate-hp", 1, NULL);
    else if (strcmp (val, "FEC_2_3") == 0)
      g_object_set (dvbbasebin, "code-rate-hp", 2, NULL);
    else if (strcmp (val, "FEC_3_4") == 0)
      g_object_set (dvbbasebin, "code-rate-hp", 3, NULL);
    else if (strcmp (val, "FEC_4_5") == 0)
      g_object_set (dvbbasebin, "code-rate-hp", 4, NULL);
    else if (strcmp (val, "FEC_5_6") == 0)
      g_object_set (dvbbasebin, "code-rate-hp", 5, NULL);
    else if (strcmp (val, "FEC_6_7") == 0)
      g_object_set (dvbbasebin, "code-rate-hp", 6, NULL);
    else if (strcmp (val, "FEC_7_8") == 0)
      g_object_set (dvbbasebin, "code-rate-hp", 7, NULL);
    else if (strcmp (val, "FEC_8_9") == 0)
      g_object_set (dvbbasebin, "code-rate-hp", 8, NULL);
    else
      g_object_set (dvbbasebin, "code-rate-hp", 9, NULL);
    val = g_hash_table_lookup (params, "inversion");
    if (strcmp (val, "INVERSION_OFF") == 0)
      g_object_set (dvbbasebin, "inversion", 0, NULL);
    else if (strcmp (val, "INVERSION_ON") == 0)
      g_object_set (dvbbasebin, "inversion", 1, NULL);
    else
      g_object_set (dvbbasebin, "inversion", 2, NULL);
  } else if (strcmp (type, "atsc") == 0) {
    gchar *val;

    ret = TRUE;

    g_object_set (dvbbasebin, "delsys", SYS_ATSC, NULL);

    val = g_hash_table_lookup (params, "modulation");
    if (strcmp (val, "QAM_64") == 0)
      g_object_set (dvbbasebin, "modulation", 3, NULL);
    else if (strcmp (val, "QAM_256") == 0)
      g_object_set (dvbbasebin, "modulation", 5, NULL);
    else if (strcmp (val, "8VSB") == 0)
      g_object_set (dvbbasebin, "modulation", 7, NULL);
    else if (strcmp (val, "16VSB") == 0)
      g_object_set (dvbbasebin, "modulation", 8, NULL);
    else
      ret = FALSE;
  }

  destroy_channels_hash (channels);

beach:
  return ret;

unknown_channel:
  {
    g_set_error (error, GST_RESOURCE_ERROR, GST_RESOURCE_ERROR_NOT_FOUND,
        _("Couldn't find details for channel '%s'"), channel_name);
    destroy_channels_hash (channels);
    return FALSE;
  }
}

static GstDvbChannelConfFormat
detect_file_format (const gchar * filename)
{
  gchar *contents;
  gchar **lines;
  gchar **line;
  GstDvbChannelConfFormat ret = CHANNEL_CONF_FORMAT_NONE;

  if (!g_file_get_contents (filename, &contents, NULL, NULL))
    return ret;

  lines = g_strsplit (contents, "\n", 0);
  line = lines;

  while (*line) {
    if (g_str_has_prefix (*line, "[") && g_str_has_suffix (*line, "]")) {
      ret = CHANNEL_CONF_FORMAT_DVBV5;
      break;
    } else if (g_strrstr (*line, ":")) {
      ret = CHANNEL_CONF_FORMAT_ZAP;
      break;
    }
    line++;
  }

  g_strfreev (lines);
  g_free (contents);
  return ret;
}

gboolean
set_properties_for_channel (GstElement * dvbbasebin,
    const gchar * channel_name, GError ** error)
{
  gboolean ret = FALSE;
  gchar *filename;

  filename = g_strdup (g_getenv ("GST_DVB_CHANNELS_CONF"));
  if (filename == NULL) {
    filename = g_build_filename (g_get_user_config_dir (),
        "gstreamer-" GST_API_VERSION, "dvb-channels.conf", NULL);
  }

  switch (detect_file_format (filename)) {
    case CHANNEL_CONF_FORMAT_DVBV5:
      if (!parse_and_configure_from_v5_conf_file (dvbbasebin, filename,
              channel_name, error)) {
        GST_WARNING_OBJECT (dvbbasebin, "Problem finding information for "
            "channel '%s' in configuration file '%s'", channel_name, filename);
      } else {
        GST_INFO_OBJECT (dvbbasebin, "Parsed libdvbv5 channel configuration "
            "file");
        ret = TRUE;
      }
      break;
    case CHANNEL_CONF_FORMAT_ZAP:
      if (!parse_and_configure_from_zap_conf_file (dvbbasebin, filename,
              channel_name, error)) {
        GST_WARNING_OBJECT (dvbbasebin, "Problem finding information for "
            "channel '%s' in configuration file '%s'", channel_name, filename);
      } else {
        GST_INFO_OBJECT (dvbbasebin, "Parsed ZAP channel configuration file");
        ret = TRUE;
      }
      break;
    default:
      GST_WARNING_OBJECT (dvbbasebin, "Unknown configuration file format. "
          "Can not get parameters for channel");
  }

  g_free (filename);
  return ret;
}
