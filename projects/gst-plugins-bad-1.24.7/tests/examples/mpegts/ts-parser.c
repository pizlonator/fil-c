/* GStreamer
 *
 * ts-parser.c: sample application to display mpeg-ts info from any pipeline
 * Copyright (C) 2013
 *           Edward Hervey <bilboed@gmail.com>
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

#define DUMP_DESCRIPTORS 0

#include <glib.h>
#include <glib-object.h>
#include <glib/gprintf.h>
#include <gst/gst.h>
#include <gst/mpegts/mpegts.h>

#define MPEGTIME_TO_GSTTIME(t) ((t) * (guint64)100000 / 9)

static void
gst_info_dump_mem_line (gchar * linebuf, gsize linebuf_size,
    const guint8 * mem, gsize mem_offset, gsize mem_size)
{
  gchar hexstr[50], ascstr[18], digitstr[4];

  if (mem_size > 16)
    mem_size = 16;

  hexstr[0] = '\0';
  ascstr[0] = '\0';

  if (mem != NULL) {
    guint i = 0;

    mem += mem_offset;
    while (i < mem_size) {
      ascstr[i] = (g_ascii_isprint (mem[i])) ? mem[i] : '.';
      g_snprintf (digitstr, sizeof (digitstr), "%02x ", mem[i]);
      g_strlcat (hexstr, digitstr, sizeof (hexstr));
      ++i;
    }
    ascstr[i] = '\0';
  }

  g_snprintf (linebuf, linebuf_size, "%08x: %-48.48s %-16.16s",
      (guint) mem_offset, hexstr, ascstr);
}

static void
dump_memory_bytes (guint8 * data, guint len, guint spacing)
{
  gsize off = 0;

  while (off < len) {
    gchar buf[128];

    /* gst_info_dump_mem_line will process 16 bytes at most */
    gst_info_dump_mem_line (buf, sizeof (buf), data, off, len - off);
    g_printf ("%*s   %s\n", spacing, "", buf);
    off += 16;
  }
}

#define dump_memory_content(desc, spacing) dump_memory_bytes((desc)->data + 2, (desc)->length, spacing)

static const gchar *
descriptor_name (GstMpegtsDescriptor * desc)
{
  GEnumValue *en = NULL;
  gint val = desc->tag;

  /* Treat extended descriptors */
  if (val == 0x7f) {
    en = g_enum_get_value (G_ENUM_CLASS (g_type_class_peek
            (GST_TYPE_MPEGTS_DVB_EXTENDED_DESCRIPTOR_TYPE)),
        desc->tag_extension);
  }
  if (en == NULL)
    en = g_enum_get_value (G_ENUM_CLASS (g_type_class_peek
            (GST_TYPE_MPEGTS_DESCRIPTOR_TYPE)), val);
  if (en == NULL)
    /* Else try with DVB enum types */
    en = g_enum_get_value (G_ENUM_CLASS (g_type_class_peek
            (GST_TYPE_MPEGTS_DVB_DESCRIPTOR_TYPE)), val);
  if (en == NULL)
    /* Else try with ATSC enum types */
    en = g_enum_get_value (G_ENUM_CLASS (g_type_class_peek
            (GST_TYPE_MPEGTS_ATSC_DESCRIPTOR_TYPE)), val);
  if (en == NULL)
    /* Else try with ISB enum types */
    en = g_enum_get_value (G_ENUM_CLASS (g_type_class_peek
            (GST_TYPE_MPEGTS_ISDB_DESCRIPTOR_TYPE)), val);
  if (en == NULL)
    /* Else try with SCTE enum types */
    en = g_enum_get_value (G_ENUM_CLASS (g_type_class_peek
            (GST_TYPE_MPEGTS_SCTE_DESCRIPTOR_TYPE)), val);
  if (en == NULL)
    /* Else try with misc enum types */
    en = g_enum_get_value (G_ENUM_CLASS (g_type_class_peek
            (GST_TYPE_MPEGTS_MISC_DESCRIPTOR_TYPE)), val);
  if (en == NULL)
    return "UNKNOWN/PRIVATE";
  return en->value_nick;
}

static const gchar *
table_id_name (gint val)
{
  GEnumValue *en;

  en = g_enum_get_value (G_ENUM_CLASS (g_type_class_peek
          (GST_TYPE_MPEGTS_SECTION_TABLE_ID)), val);
  if (en == NULL)
    /* Else try with DVB enum types */
    en = g_enum_get_value (G_ENUM_CLASS (g_type_class_peek
            (GST_TYPE_MPEGTS_SECTION_DVB_TABLE_ID)), val);
  if (en == NULL)
    /* Else try with ATSC enum types */
    en = g_enum_get_value (G_ENUM_CLASS (g_type_class_peek
            (GST_TYPE_MPEGTS_SECTION_ATSC_TABLE_ID)), val);
  if (en == NULL)
    /* Else try with SCTE enum types */
    en = g_enum_get_value (G_ENUM_CLASS (g_type_class_peek
            (GST_TYPE_MPEGTS_SECTION_SCTE_TABLE_ID)), val);
  if (en == NULL)
    return "UNKNOWN/PRIVATE";
  return en->value_nick;
}

static const gchar *
stream_type_name (gint val)
{
  GEnumValue *en;

  en = g_enum_get_value (G_ENUM_CLASS (g_type_class_peek
          (GST_TYPE_MPEGTS_STREAM_TYPE)), val);
  if (en == NULL)
    /* Else try with HDMV enum types */
    en = g_enum_get_value (G_ENUM_CLASS (g_type_class_peek
            (GST_TYPE_MPEGTS_HDMV_STREAM_TYPE)), val);
  if (en == NULL)
    /* Else try with SCTE enum types */
    en = g_enum_get_value (G_ENUM_CLASS (g_type_class_peek
            (GST_TYPE_MPEGTS_SCTE_STREAM_TYPE)), val);
  if (en == NULL)
    return "UNKNOWN/PRIVATE";
  return en->value_nick;
}

static const gchar *
enum_name (GType instance_type, gint val)
{
  GEnumValue *en;

  en = g_enum_get_value (G_ENUM_CLASS (g_type_class_peek (instance_type)), val);

  if (!en)
    return "UNKNOWN/PRIVATE";
  return en->value_nick;
}

static void
dump_cable_delivery_descriptor (GstMpegtsDescriptor * desc, guint spacing)
{
  GstMpegtsCableDeliverySystemDescriptor res;

  if (gst_mpegts_descriptor_parse_cable_delivery_system (desc, &res)) {
    g_printf ("%*s Cable Delivery Descriptor\n", spacing, "");
    g_printf ("%*s   Frequency   : %d Hz\n", spacing, "", res.frequency);
    g_printf ("%*s   Outer FEC   : %d (%s)\n", spacing, "", res.outer_fec,
        enum_name (GST_TYPE_MPEGTS_CABLE_OUTER_FEC_SCHEME, res.outer_fec));
    g_printf ("%*s   modulation  : %d (%s)\n", spacing, "", res.modulation,
        enum_name (GST_TYPE_MPEGTS_MODULATION_TYPE, res.modulation));
    g_printf ("%*s   Symbol rate : %d sym/s\n", spacing, "", res.symbol_rate);
    g_printf ("%*s   Inner FEC   : %d (%s)\n", spacing, "", res.fec_inner,
        enum_name (GST_TYPE_MPEGTS_DVB_CODE_RATE, res.fec_inner));
  }
}

static void
dump_terrestrial_delivery (GstMpegtsDescriptor * desc, guint spacing)
{
  GstMpegtsTerrestrialDeliverySystemDescriptor res;

  if (gst_mpegts_descriptor_parse_terrestrial_delivery_system (desc, &res)) {
    g_printf ("%*s Terrestrial Delivery Descriptor\n", spacing, "");
    g_printf ("%*s   Frequency         : %d Hz\n", spacing, "", res.frequency);
    g_printf ("%*s   Bandwidth         : %d Hz\n", spacing, "", res.bandwidth);
    g_printf ("%*s   Priority          : %s\n", spacing, "",
        res.priority ? "TRUE" : "FALSE");
    g_printf ("%*s   Time slicing      : %s\n", spacing, "",
        res.time_slicing ? "TRUE" : "FALSE");
    g_printf ("%*s   MPE FEC           : %s\n", spacing, "",
        res.mpe_fec ? "TRUE" : "FALSE");
    g_printf ("%*s   Constellation     : %d (%s)\n", spacing, "",
        res.constellation, enum_name (GST_TYPE_MPEGTS_MODULATION_TYPE,
            res.constellation));
    g_printf ("%*s   Hierarchy         : %d (%s)\n", spacing, "",
        res.hierarchy, enum_name (GST_TYPE_MPEGTS_TERRESTRIAL_HIERARCHY,
            res.hierarchy));
    g_printf ("%*s   Code Rate HP      : %d (%s)\n", spacing, "",
        res.code_rate_hp, enum_name (GST_TYPE_MPEGTS_DVB_CODE_RATE,
            res.code_rate_hp));
    g_printf ("%*s   Code Rate LP      : %d (%s)\n", spacing, "",
        res.code_rate_lp, enum_name (GST_TYPE_MPEGTS_DVB_CODE_RATE,
            res.code_rate_lp));
    g_printf ("%*s   Guard Interval    : %d (%s)\n", spacing, "",
        res.guard_interval,
        enum_name (GST_TYPE_MPEGTS_TERRESTRIAL_GUARD_INTERVAL,
            res.guard_interval));
    g_printf ("%*s   Transmission Mode : %d (%s)\n", spacing, "",
        res.transmission_mode,
        enum_name (GST_TYPE_MPEGTS_TERRESTRIAL_TRANSMISSION_MODE,
            res.transmission_mode));
    g_printf ("%*s   Other Frequency   : %s\n", spacing, "",
        res.other_frequency ? "TRUE" : "FALSE");
  }
}

static void
dump_dvb_service_list (GstMpegtsDescriptor * desc, guint spacing)
{
  GPtrArray *res;

  if (gst_mpegts_descriptor_parse_dvb_service_list (desc, &res)) {
    guint i;
    g_printf ("%*s DVB Service List Descriptor\n", spacing, "");
    for (i = 0; i < res->len; i++) {
      GstMpegtsDVBServiceListItem *item = g_ptr_array_index (res, i);
      g_printf ("%*s   Service #%d, id:0x%04x, type:0x%x (%s)\n",
          spacing, "", i, item->service_id, item->type,
          enum_name (GST_TYPE_MPEGTS_DVB_SERVICE_TYPE, item->type));
    }
    g_ptr_array_unref (res);
  }
}

static void
dump_logical_channel_descriptor (GstMpegtsDescriptor * desc, guint spacing)
{
  GstMpegtsLogicalChannelDescriptor res;
  guint i;

  if (gst_mpegts_descriptor_parse_logical_channel (desc, &res)) {
    g_printf ("%*s Logical Channel Descriptor (%d channels)\n", spacing, "",
        res.nb_channels);
    for (i = 0; i < res.nb_channels; i++) {
      GstMpegtsLogicalChannel *chann = &res.channels[i];
      g_printf ("%*s   service_id: 0x%04x, logical channel number:%4d\n",
          spacing, "", chann->service_id, chann->logical_channel_number);
    }
  }
}

static void
dump_multiligual_network_name (GstMpegtsDescriptor * desc, guint spacing)
{
  GPtrArray *items;
  if (gst_mpegts_descriptor_parse_dvb_multilingual_network_name (desc, &items)) {
    guint i;
    for (i = 0; i < items->len; i++) {
      GstMpegtsDvbMultilingualNetworkNameItem *item =
          g_ptr_array_index (items, i);
      g_printf ("%*s item : %u\n", spacing, "", i);
      g_printf ("%*s   language_code : %s\n", spacing, "", item->language_code);
      g_printf ("%*s   network_name  : %s\n", spacing, "", item->network_name);
    }
    g_ptr_array_unref (items);
  }
}

static void
dump_multiligual_bouquet_name (GstMpegtsDescriptor * desc, guint spacing)
{
  GPtrArray *items;
  if (gst_mpegts_descriptor_parse_dvb_multilingual_bouquet_name (desc, &items)) {
    guint i;
    for (i = 0; i < items->len; i++) {
      GstMpegtsDvbMultilingualBouquetNameItem *item =
          g_ptr_array_index (items, i);
      g_printf ("%*s item : %u\n", spacing, "", i);
      g_printf ("%*s   language_code : %s\n", spacing, "", item->language_code);
      g_printf ("%*s   bouguet_name  : %s\n", spacing, "", item->bouquet_name);
    }
    g_ptr_array_unref (items);
  }
}

static void
dump_multiligual_service_name (GstMpegtsDescriptor * desc, guint spacing)
{
  GPtrArray *items;
  if (gst_mpegts_descriptor_parse_dvb_multilingual_service_name (desc, &items)) {
    guint i;
    for (i = 0; i < items->len; i++) {
      GstMpegtsDvbMultilingualServiceNameItem *item =
          g_ptr_array_index (items, i);
      g_printf ("%*s item : %u\n", spacing, "", i);
      g_printf ("%*s   language_code : %s\n", spacing, "", item->language_code);
      g_printf ("%*s   service_name  : %s\n", spacing, "", item->service_name);
      g_printf ("%*s   provider_name : %s\n", spacing, "", item->provider_name);
    }
    g_ptr_array_unref (items);
  }
}

static void
dump_multiligual_component (GstMpegtsDescriptor * desc, guint spacing)
{
  GPtrArray *items;
  guint8 tag;
  if (gst_mpegts_descriptor_parse_dvb_multilingual_component (desc, &tag,
          &items)) {
    guint8 i;
    g_printf ("%*s component_tag : 0x%02x\n", spacing, "", tag);
    for (i = 0; i < items->len; i++) {
      GstMpegtsDvbMultilingualComponentItem *item =
          g_ptr_array_index (items, i);
      g_printf ("%*s   item : %u\n", spacing, "", i);
      g_printf ("%*s     language_code : %s\n", spacing, "",
          item->language_code);
      g_printf ("%*s     description   : %s\n", spacing, "", item->description);
    }
    g_ptr_array_unref (items);
  }
}

static void
dump_linkage (GstMpegtsDescriptor * desc, guint spacing)
{
  GstMpegtsDVBLinkageDescriptor *res;

  if (gst_mpegts_descriptor_parse_dvb_linkage (desc, &res)) {
    g_printf ("%*s Linkage Descriptor : 0x%02x (%s)\n", spacing, "",
        res->linkage_type, enum_name (GST_TYPE_MPEGTS_DVB_LINKAGE_TYPE,
            res->linkage_type));

    g_printf ("%*s   Transport Stream ID : 0x%04x\n", spacing, "",
        res->transport_stream_id);
    g_printf ("%*s   Original Network ID : 0x%04x\n", spacing, "",
        res->original_network_id);
    g_printf ("%*s   Service ID          : 0x%04x\n", spacing, "",
        res->service_id);

    switch (res->linkage_type) {
      case GST_MPEGTS_DVB_LINKAGE_MOBILE_HAND_OVER:
      {
        const GstMpegtsDVBLinkageMobileHandOver *linkage =
            gst_mpegts_dvb_linkage_descriptor_get_mobile_hand_over (res);
        g_printf ("%*s   hand_over_type    : 0x%02x (%s)\n", spacing,
            "", linkage->hand_over_type,
            enum_name (GST_TYPE_MPEGTS_DVB_LINKAGE_HAND_OVER_TYPE,
                linkage->hand_over_type));
        g_printf ("%*s   origin_type       : %s\n", spacing, "",
            linkage->origin_type ? "SDT" : "NIT");
        g_printf ("%*s   network_id        : 0x%04x\n", spacing, "",
            linkage->network_id);
        g_printf ("%*s   initial_service_id: 0x%04x\n", spacing, "",
            linkage->initial_service_id);
        break;
      }
      case GST_MPEGTS_DVB_LINKAGE_EVENT:
      {
        const GstMpegtsDVBLinkageEvent *linkage =
            gst_mpegts_dvb_linkage_descriptor_get_event (res);
        g_printf ("%*s   target_event_id   : 0x%04x\n", spacing, "",
            linkage->target_event_id);
        g_printf ("%*s   target_listed     : %s\n", spacing, "",
            linkage->target_listed ? "TRUE" : "FALSE");
        g_printf ("%*s   event_simulcast   : %s\n", spacing, "",
            linkage->event_simulcast ? "TRUE" : "FALSE");
        break;
      }
      case GST_MPEGTS_DVB_LINKAGE_EXTENDED_EVENT:
      {
        guint i;
        const GPtrArray *items =
            gst_mpegts_dvb_linkage_descriptor_get_extended_event (res);

        for (i = 0; i < items->len; i++) {
          GstMpegtsDVBLinkageExtendedEvent *linkage =
              g_ptr_array_index (items, i);
          g_printf ("%*s   target_event_id   : 0x%04x\n", spacing, "",
              linkage->target_event_id);
          g_printf ("%*s   target_listed     : %s\n", spacing, "",
              linkage->target_listed ? "TRUE" : "FALSE");
          g_printf ("%*s   event_simulcast   : %s\n", spacing, "",
              linkage->event_simulcast ? "TRUE" : "FALSE");
          g_printf ("%*s   link_type         : 0x%01x\n", spacing, "",
              linkage->link_type);
          g_printf ("%*s   target_id_type    : 0x%01x\n", spacing, "",
              linkage->target_id_type);
          g_printf ("%*s   original_network_id_flag : %s\n", spacing, "",
              linkage->original_network_id_flag ? "TRUE" : "FALSE");
          g_printf ("%*s   service_id_flag   : %s\n", spacing, "",
              linkage->service_id_flag ? "TRUE" : "FALSE");
          if (linkage->target_id_type == 3) {
            g_printf ("%*s   user_defined_id   : 0x%02x\n", spacing, "",
                linkage->user_defined_id);
          } else {
            if (linkage->target_id_type == 1)
              g_printf ("%*s   target_transport_stream_id : 0x%04x\n",
                  spacing, "", linkage->target_transport_stream_id);
            if (linkage->original_network_id_flag)
              g_printf ("%*s   target_original_network_id : 0x%04x\n",
                  spacing, "", linkage->target_original_network_id);
            if (linkage->service_id_flag)
              g_printf ("%*s   target_service_id          : 0x%04x\n",
                  spacing, "", linkage->target_service_id);
          }
        }
        break;
      }
      default:
        break;
    }
    if (res->private_data_length > 0) {
      dump_memory_bytes (res->private_data_bytes, res->private_data_length,
          spacing + 2);
    }
    gst_mpegts_dvb_linkage_descriptor_free (res);
  }
}

static void
dump_component (GstMpegtsDescriptor * desc, guint spacing)
{
  GstMpegtsComponentDescriptor *res;

  if (gst_mpegts_descriptor_parse_dvb_component (desc, &res)) {
    g_printf ("%*s stream_content : 0x%02x (%s)\n", spacing, "",
        res->stream_content,
        enum_name (GST_TYPE_MPEGTS_COMPONENT_STREAM_CONTENT,
            res->stream_content));
    g_printf ("%*s component_type : 0x%02x\n", spacing, "",
        res->component_type);
    g_printf ("%*s component_tag  : 0x%02x\n", spacing, "", res->component_tag);
    g_printf ("%*s language_code  : %s\n", spacing, "", res->language_code);
    g_printf ("%*s text           : %s\n", spacing, "",
        res->text ? res->text : "NULL");
    gst_mpegts_dvb_component_descriptor_free (res);
  }
}

static void
dump_content (GstMpegtsDescriptor * desc, guint spacing)
{
  GPtrArray *contents;
  guint i;

  if (gst_mpegts_descriptor_parse_dvb_content (desc, &contents)) {
    for (i = 0; i < contents->len; i++) {
      GstMpegtsContent *item = g_ptr_array_index (contents, i);
      g_printf ("%*s content nibble 1 : 0x%01x (%s)\n", spacing, "",
          item->content_nibble_1,
          enum_name (GST_TYPE_MPEGTS_CONTENT_NIBBLE_HI,
              item->content_nibble_1));
      g_printf ("%*s content nibble 2 : 0x%01x\n", spacing, "",
          item->content_nibble_2);
      g_printf ("%*s user_byte        : 0x%02x\n", spacing, "",
          item->user_byte);
    }
    g_ptr_array_unref (contents);
  }
}

static void
dump_iso_639_language (GstMpegtsDescriptor * desc, guint spacing)
{
  guint i;
  GstMpegtsISO639LanguageDescriptor *res;

  if (gst_mpegts_descriptor_parse_iso_639_language (desc, &res)) {
    for (i = 0; i < res->nb_language; i++) {
      g_print
          ("%*s ISO 639 Language Descriptor %s , audio_type:0x%x (%s)\n",
          spacing, "", res->language[i], res->audio_type[i],
          enum_name (GST_TYPE_MPEGTS_ISO639_AUDIO_TYPE, res->audio_type[i]));
    }
    gst_mpegts_iso_639_language_descriptor_free (res);
  }
}

static void
dump_dvb_extended_event (GstMpegtsDescriptor * desc, guint spacing)
{
  GstMpegtsExtendedEventDescriptor *res;

  if (gst_mpegts_descriptor_parse_dvb_extended_event (desc, &res)) {
    guint i;
    g_printf ("%*s DVB Extended Event\n", spacing, "");
    g_printf ("%*s   descriptor_number:%d, last_descriptor_number:%d\n",
        spacing, "", res->descriptor_number, res->last_descriptor_number);
    g_printf ("%*s   language_code:%s\n", spacing, "", res->language_code);
    g_printf ("%*s   text : %s\n", spacing, "", res->text);
    for (i = 0; i < res->items->len; i++) {
      GstMpegtsExtendedEventItem *item = g_ptr_array_index (res->items, i);
      g_printf ("%*s     #%d [description:item]  %s : %s\n",
          spacing, "", i, item->item_description, item->item);
    }
    gst_mpegts_extended_event_descriptor_free (res);
  }
}

#define SAFE_CHAR(a) (g_ascii_isprint(a) ? a : '.')

/* Single descriptor dump
 * Descriptors that can only appear in specific tables should be handled before */
static void
dump_generic_descriptor (GstMpegtsDescriptor * desc, guint spacing)
{
  switch (desc->tag) {
    case GST_MTS_DESC_REGISTRATION:
    {
      const guint8 *data = desc->data + 2;
      g_printf ("%*s   Registration : %c%c%c%c [%02x%02x%02x%02x]\n", spacing,
          "", SAFE_CHAR (data[0]), SAFE_CHAR (data[1]), SAFE_CHAR (data[2]),
          SAFE_CHAR (data[3]), data[0], data[1], data[2], data[3]);

      break;
    }
    case GST_MTS_DESC_CA:
    {
      guint16 ca_pid, ca_system_id;
      const guint8 *private_data;
      gsize private_data_size;
      if (gst_mpegts_descriptor_parse_ca (desc, &ca_system_id, &ca_pid,
              &private_data, &private_data_size)) {
        g_printf ("%*s   CA system id : 0x%04x\n", spacing, "", ca_system_id);
        g_printf ("%*s   CA PID       : 0x%04x\n", spacing, "", ca_pid);
        if (private_data_size) {
          g_printf ("%*s   Private Data :\n", spacing, "");
          dump_memory_bytes ((guint8 *) private_data, private_data_size,
              spacing + 2);
        }
      }
      break;
    }
    case GST_MTS_DESC_DVB_NETWORK_NAME:
    {
      gchar *network_name;
      if (gst_mpegts_descriptor_parse_dvb_network_name (desc, &network_name)) {
        g_printf ("%*s   Network Name : %s\n", spacing, "", network_name);
        g_free (network_name);
      }
      break;
    }
    case GST_MTS_DESC_DVB_SERVICE_LIST:
    {
      dump_dvb_service_list (desc, spacing + 2);
      break;
    }
    case GST_MTS_DESC_DVB_CABLE_DELIVERY_SYSTEM:
      dump_cable_delivery_descriptor (desc, spacing + 2);
      break;
    case GST_MTS_DESC_DVB_TERRESTRIAL_DELIVERY_SYSTEM:
      dump_terrestrial_delivery (desc, spacing + 2);
      break;
    case GST_MTS_DESC_DVB_BOUQUET_NAME:
    {
      gchar *bouquet_name;
      if (gst_mpegts_descriptor_parse_dvb_bouquet_name (desc, &bouquet_name)) {
        g_printf ("%*s   Bouquet Name Descriptor, bouquet_name:%s\n", spacing,
            "", bouquet_name);
        g_free (bouquet_name);
      }
      break;
    }
    case GST_MTS_DESC_DVB_SERVICE:
    {
      gchar *service_name, *provider_name;
      GstMpegtsDVBServiceType service_type;
      if (gst_mpegts_descriptor_parse_dvb_service (desc, &service_type,
              &service_name, &provider_name)) {
        g_printf ("%*s   Service Descriptor, type:0x%02x (%s)\n", spacing, "",
            service_type, enum_name (GST_TYPE_MPEGTS_DVB_SERVICE_TYPE,
                service_type));
        g_printf ("%*s      service_name  : %s\n", spacing, "", service_name);
        g_printf ("%*s      provider_name : %s\n", spacing, "", provider_name);
        g_free (service_name);
        g_free (provider_name);

      }
      break;
    }
    case GST_MTS_DESC_DVB_MULTILINGUAL_BOUQUET_NAME:
    {
      dump_multiligual_bouquet_name (desc, spacing + 2);
      break;
    }
    case GST_MTS_DESC_DVB_MULTILINGUAL_NETWORK_NAME:
    {
      dump_multiligual_network_name (desc, spacing + 2);
      break;
    }
    case GST_MTS_DESC_DVB_MULTILINGUAL_SERVICE_NAME:
    {
      dump_multiligual_service_name (desc, spacing + 2);
      break;
    }
    case GST_MTS_DESC_DVB_MULTILINGUAL_COMPONENT:
    {
      dump_multiligual_component (desc, spacing + 2);
      break;
    }
    case GST_MTS_DESC_DVB_PRIVATE_DATA_SPECIFIER:
    {
      guint32 specifier;
      guint8 len = 0, *data = NULL;

      if (gst_mpegts_descriptor_parse_dvb_private_data_specifier (desc,
              &specifier, &data, &len)) {
        g_printf ("%*s   private_data_specifier : 0x%08x\n", spacing, "",
            specifier);
        if (len > 0) {
          dump_memory_bytes (data, len, spacing + 2);
          g_free (data);
        }
      }
      break;
    }
    case GST_MTS_DESC_DVB_FREQUENCY_LIST:
    {
      gboolean offset;
      GArray *list;
      if (gst_mpegts_descriptor_parse_dvb_frequency_list (desc, &offset, &list)) {
        guint j;
        for (j = 0; j < list->len; j++) {
          guint32 freq = g_array_index (list, guint32, j);
          g_printf ("%*s   Frequency : %u %s\n", spacing, "", freq,
              offset ? "kHz" : "Hz");
        }
        g_array_unref (list);
      }
      break;
    }
    case GST_MTS_DESC_DVB_LINKAGE:
      dump_linkage (desc, spacing + 2);
      break;
    case GST_MTS_DESC_DVB_COMPONENT:
      dump_component (desc, spacing + 2);
      break;
    case GST_MTS_DESC_DVB_STREAM_IDENTIFIER:
    {
      guint8 tag;
      if (gst_mpegts_descriptor_parse_dvb_stream_identifier (desc, &tag)) {
        g_printf ("%*s   Component Tag : 0x%02x\n", spacing, "", tag);
      }
      break;
    }
    case GST_MTS_DESC_DVB_CA_IDENTIFIER:
    {
      GArray *list;
      guint j;
      guint16 ca_id;
      if (gst_mpegts_descriptor_parse_dvb_ca_identifier (desc, &list)) {
        for (j = 0; j < list->len; j++) {
          ca_id = g_array_index (list, guint16, j);
          g_printf ("%*s   CA Identifier : 0x%04x\n", spacing, "", ca_id);
        }
        g_array_unref (list);
      }
      break;
    }
    case GST_MTS_DESC_DVB_CONTENT:
      dump_content (desc, spacing + 2);
      break;
    case GST_MTS_DESC_DVB_PARENTAL_RATING:
    {
      GPtrArray *ratings;
      guint j;

      if (gst_mpegts_descriptor_parse_dvb_parental_rating (desc, &ratings)) {
        for (j = 0; j < ratings->len; j++) {
          GstMpegtsDVBParentalRatingItem *item = g_ptr_array_index (ratings, j);
          g_printf ("%*s   country_code : %s\n", spacing, "",
              item->country_code);
          g_printf ("%*s   rating age   : %d\n", spacing, "", item->rating);
        }
        g_ptr_array_unref (ratings);
      }
      break;
    }
    case GST_MTS_DESC_DVB_DATA_BROADCAST:
    {
      GstMpegtsDataBroadcastDescriptor *res;

      if (gst_mpegts_descriptor_parse_dvb_data_broadcast (desc, &res)) {
        g_printf ("%*s   data_broadcast_id : 0x%04x\n", spacing, "",
            res->data_broadcast_id);
        g_printf ("%*s   component_tag     : 0x%02x\n", spacing, "",
            res->component_tag);
        if (res->length > 0) {
          g_printf ("%*s   selector_bytes:\n", spacing, "");
          dump_memory_bytes (res->selector_bytes, res->length, spacing + 2);
        }
        g_printf ("%*s   text              : %s\n", spacing, "",
            res->text ? res->text : "NULL");
        gst_mpegts_dvb_data_broadcast_descriptor_free (res);
      }
      break;
    }
    case GST_MTS_DESC_ISO_639_LANGUAGE:
      dump_iso_639_language (desc, spacing + 2);
      break;
    case GST_MTS_DESC_DVB_SHORT_EVENT:
    {
      gchar *language_code, *event_name, *text;
      if (gst_mpegts_descriptor_parse_dvb_short_event (desc, &language_code,
              &event_name, &text)) {
        g_printf ("%*s   Short Event, language_code:%s\n", spacing, "",
            language_code);
        g_printf ("%*s     event_name : %s\n", spacing, "", event_name);
        g_printf ("%*s     text       : %s\n", spacing, "", text);
        g_free (language_code);
        g_free (event_name);
        g_free (text);
      }
    }
      break;
    case GST_MTS_DESC_DVB_EXTENDED_EVENT:
    {
      dump_dvb_extended_event (desc, spacing + 2);
      break;
    }
    case GST_MTS_DESC_DVB_SUBTITLING:
    {
      gchar *lang;
      guint8 type;
      guint16 composition;
      guint16 ancillary;
      guint j;

      for (j = 0;
          gst_mpegts_descriptor_parse_dvb_subtitling_idx (desc, j, &lang,
              &type, &composition, &ancillary); j++) {
        g_printf ("%*s   Subtitling, language_code:%s\n", spacing, "", lang);
        g_printf ("%*s      type                : %u\n", spacing, "", type);
        g_printf ("%*s      composition page id : %u\n", spacing, "",
            composition);
        g_printf ("%*s      ancillary page id   : %u\n", spacing, "",
            ancillary);
        g_free (lang);
      }
    }
      break;
    case GST_MTS_DESC_DVB_TELETEXT:
    {
      GstMpegtsDVBTeletextType type;
      gchar *lang;
      guint8 magazine, page_number;
      guint j;

      for (j = 0;
          gst_mpegts_descriptor_parse_dvb_teletext_idx (desc, j, &lang, &type,
              &magazine, &page_number); j++) {
        g_printf ("%*s   Teletext, type:0x%02x (%s)\n", spacing, "", type,
            enum_name (GST_TYPE_MPEGTS_DVB_TELETEXT_TYPE, type));
        g_printf ("%*s      language    : %s\n", spacing, "", lang);
        g_printf ("%*s      magazine    : %u\n", spacing, "", magazine);
        g_printf ("%*s      page number : %u\n", spacing, "", page_number);
        g_free (lang);
      }
    }
      break;
    case GST_MTS_DESC_METADATA:
    {
      GstMpegtsMetadataDescriptor *metadataDescriptor;
      if (gst_mpegts_descriptor_parse_metadata (desc, &metadataDescriptor)) {
        g_printf ("%*s   metadata application format : 0x%04x\n", spacing, "",
            metadataDescriptor->metadata_application_format);
        g_printf ("%*s   metadata format             : 0x%02x\n", spacing, "",
            metadataDescriptor->metadata_format);
        if (metadataDescriptor->metadata_format ==
            GST_MPEGTS_METADATA_FORMAT_IDENTIFIER_FIELD) {
          g_printf ("%*s   metadata format identifier  : 0x%08x\n", spacing, "",
              metadataDescriptor->metadata_format_identifier);
        }
        g_printf ("%*s   metadata service id         : 0x%02x\n", spacing, "",
            metadataDescriptor->metadata_service_id);
        g_printf ("%*s   decoder config flags        : 0x%x\n", spacing, "",
            metadataDescriptor->decoder_config_flags);
        g_printf ("%*s   DSM-CC flag                 : %s\n", spacing, "",
            metadataDescriptor->dsm_cc_flag ? "Set" : "Not set");
        g_free (metadataDescriptor);
      }
    }
      break;
    case GST_MTS_DESC_METADATA_STD:
    {
      guint32 metadata_input_leak_rate;
      guint32 metadata_buffer_size;
      guint32 metadata_output_leak_rate;

      if (gst_mpegts_descriptor_parse_metadata_std (desc,
              &metadata_input_leak_rate, &metadata_buffer_size,
              &metadata_output_leak_rate)) {
        g_printf ("%*s   metadata input leak rate  : %i\n", spacing, "",
            metadata_input_leak_rate);
        g_printf ("%*s   metadata buffer size      : %i\n", spacing, "",
            metadata_buffer_size);
        g_printf ("%*s   metadata output leak rate : %i\n", spacing, "",
            metadata_output_leak_rate);
      }
    }
      break;
    default:
      break;
  }
}

static void
dump_descriptors (GPtrArray * descriptors, guint spacing)
{
  guint i;

  for (i = 0; i < descriptors->len; i++) {
    GstMpegtsDescriptor *desc = g_ptr_array_index (descriptors, i);
    g_printf ("%*s [descriptor 0x%02x (%s) length:%d]\n", spacing, "",
        desc->tag, descriptor_name (desc), desc->length);
    if (DUMP_DESCRIPTORS)
      dump_memory_content (desc, spacing + 2);
    dump_generic_descriptor (desc, spacing + 2);
  }
}

static void
dump_nit_descriptors (GPtrArray * descriptors, guint spacing)
{
  /* Descriptors that can only appear in NIT */
  guint i;

  for (i = 0; i < descriptors->len; i++) {
    GstMpegtsDescriptor *desc = g_ptr_array_index (descriptors, i);
    g_printf ("%*s [descriptor 0x%02x (%s) length:%d]\n", spacing, "",
        desc->tag, descriptor_name (desc), desc->length);
    if (DUMP_DESCRIPTORS)
      dump_memory_content (desc, spacing + 2);
    switch (desc->tag) {
      case GST_MTS_DESC_DTG_LOGICAL_CHANNEL:
        dump_logical_channel_descriptor (desc, spacing + 2);
        break;
      default:
        dump_generic_descriptor (desc, spacing + 2);
        break;
    }
  }
}


static void
dump_pat (GstMpegtsSection * section)
{
  GPtrArray *pat = gst_mpegts_section_get_pat (section);
  guint i, len;

  len = pat->len;
  g_printf ("   %d program(s):\n", len);

  for (i = 0; i < len; i++) {
    GstMpegtsPatProgram *patp = g_ptr_array_index (pat, i);

    g_print
        ("     program_number:%6d (0x%04x), network_or_program_map_PID:0x%04x\n",
        patp->program_number, patp->program_number,
        patp->network_or_program_map_PID);
  }

  g_ptr_array_unref (pat);
}

static void
dump_pmt (GstMpegtsSection * section)
{
  const GstMpegtsPMT *pmt = gst_mpegts_section_get_pmt (section);
  guint i, len;

  g_printf ("     program_number : 0x%04x\n", section->subtable_extension);
  g_printf ("     pcr_pid        : 0x%04x\n", pmt->pcr_pid);
  dump_descriptors (pmt->descriptors, 7);
  len = pmt->streams->len;
  g_printf ("     %d Streams:\n", len);
  for (i = 0; i < len; i++) {
    GstMpegtsPMTStream *stream = g_ptr_array_index (pmt->streams, i);
    g_printf ("       pid:0x%04x , stream_type:0x%02x (%s)\n", stream->pid,
        stream->stream_type, stream_type_name (stream->stream_type));
    dump_descriptors (stream->descriptors, 9);
  }
}

static void
dump_eit (GstMpegtsSection * section)
{
  const GstMpegtsEIT *eit = gst_mpegts_section_get_eit (section);
  guint i, len;

  g_assert (eit);

  g_printf ("     service_id          : 0x%04x\n", section->subtable_extension);
  g_printf ("     transport_stream_id : 0x%04x\n", eit->transport_stream_id);
  g_printf ("     original_network_id : 0x%04x\n", eit->original_network_id);
  g_printf ("     segment_last_section_number:0x%02x, last_table_id:0x%02x\n",
      eit->segment_last_section_number, eit->last_table_id);
  g_printf ("     actual_stream : %s, present_following : %s\n",
      eit->actual_stream ? "TRUE" : "FALSE",
      eit->present_following ? "TRUE" : "FALSE");

  len = eit->events->len;
  g_printf ("     %d Event(s):\n", len);
  for (i = 0; i < len; i++) {
    gchar *tmp = (gchar *) "<NO TIME>";
    GstMpegtsEITEvent *event = g_ptr_array_index (eit->events, i);

    if (event->start_time)
      tmp = gst_date_time_to_iso8601_string (event->start_time);
    g_printf ("       event_id:0x%04x, start_time:%s, duration:%"
        GST_TIME_FORMAT "\n", event->event_id, tmp,
        GST_TIME_ARGS (event->duration * GST_SECOND));
    g_printf ("       running_status:0x%02x (%s), free_CA_mode:%d (%s)\n",
        event->running_status, enum_name (GST_TYPE_MPEGTS_RUNNING_STATUS,
            event->running_status), event->free_CA_mode,
        event->free_CA_mode ? "MAYBE SCRAMBLED" : "NOT SCRAMBLED");
    if (event->start_time)
      g_free (tmp);
    dump_descriptors (event->descriptors, 9);
  }
}

static void
dump_atsc_mult_string (GPtrArray * mstrings, guint spacing)
{
  guint i;

  for (i = 0; i < mstrings->len; i++) {
    GstMpegtsAtscMultString *mstring = g_ptr_array_index (mstrings, i);
    gint j, n;

    n = mstring->segments->len;

    g_printf ("%*s [multstring entry (%d) iso_639 langcode: %s]\n", spacing, "",
        i, mstring->iso_639_langcode);
    g_printf ("%*s   segments:%d\n", spacing, "", n);
    for (j = 0; j < n; j++) {
      GstMpegtsAtscStringSegment *segment =
          g_ptr_array_index (mstring->segments, j);

      g_printf ("%*s    Compression:0x%x\n", spacing, "",
          segment->compression_type);
      g_printf ("%*s    Mode:0x%x\n", spacing, "", segment->mode);
      g_printf ("%*s    Len:%u\n", spacing, "", segment->compressed_data_size);
      g_printf ("%*s    %s\n", spacing, "",
          gst_mpegts_atsc_string_segment_get_string (segment));
    }
  }
}

static void
dump_atsc_eit (GstMpegtsSection * section)
{
  const GstMpegtsAtscEIT *eit = gst_mpegts_section_get_atsc_eit (section);
  guint i, len;

  g_assert (eit);

  g_printf ("     event_id            : 0x%04x\n", eit->source_id);
  g_printf ("     protocol_version    : %u\n", eit->protocol_version);

  len = eit->events->len;
  g_printf ("     %d Event(s):\n", len);
  for (i = 0; i < len; i++) {
    GstMpegtsAtscEITEvent *event = g_ptr_array_index (eit->events, i);

    g_printf ("     %d)\n", i);
    g_printf ("       event_id: 0x%04x\n", event->event_id);
    g_printf ("       start_time: %u\n", event->start_time);
    g_printf ("       etm_location: 0x%x\n", event->etm_location);
    g_printf ("       length_in_seconds: %u\n", event->length_in_seconds);
    g_printf ("       Title(s):\n");
    dump_atsc_mult_string (event->titles, 9);
    dump_descriptors (event->descriptors, 9);
  }
}

static void
dump_ett (GstMpegtsSection * section)
{
  const GstMpegtsAtscETT *ett = gst_mpegts_section_get_atsc_ett (section);
  guint len;

  g_assert (ett);

  g_printf ("     ett_table_id_ext    : 0x%04x\n", ett->ett_table_id_extension);
  g_printf ("     protocol_version    : 0x%04x\n", ett->protocol_version);
  g_printf ("     etm_id              : 0x%04x\n", ett->etm_id);

  len = ett->messages->len;
  g_printf ("     %d Messages(s):\n", len);
  dump_atsc_mult_string (ett->messages, 9);
}

static void
dump_stt (GstMpegtsSection * section)
{
  const GstMpegtsAtscSTT *stt = gst_mpegts_section_get_atsc_stt (section);
  GstDateTime *dt;
  gchar *dt_str = NULL;

  g_assert (stt);

  dt = gst_mpegts_atsc_stt_get_datetime_utc ((GstMpegtsAtscSTT *) stt);
  if (dt)
    dt_str = gst_date_time_to_iso8601_string (dt);

  g_printf ("     protocol_version    : 0x%04x\n", stt->protocol_version);
  g_printf ("     system_time         : 0x%08x\n", stt->system_time);
  g_printf ("     gps_utc_offset      : %d\n", stt->gps_utc_offset);
  g_printf ("     daylight saving     : %d day:%d hour:%d\n", stt->ds_status,
      stt->ds_dayofmonth, stt->ds_hour);
  g_printf ("     utc datetime        : %s", dt_str);

  g_free (dt_str);
  gst_date_time_unref (dt);
}

static void
dump_nit (GstMpegtsSection * section)
{
  const GstMpegtsNIT *nit = gst_mpegts_section_get_nit (section);
  guint i, len;

  g_assert (nit);

  g_printf ("     network_id     : 0x%04x\n", section->subtable_extension);
  g_printf ("     actual_network : %s\n",
      nit->actual_network ? "TRUE" : "FALSE");
  dump_descriptors (nit->descriptors, 7);
  len = nit->streams->len;
  g_printf ("     %d Streams:\n", len);
  for (i = 0; i < len; i++) {
    GstMpegtsNITStream *stream = g_ptr_array_index (nit->streams, i);
    g_printf
        ("       transport_stream_id:0x%04x , original_network_id:0x%02x\n",
        stream->transport_stream_id, stream->original_network_id);
    dump_nit_descriptors (stream->descriptors, 9);
  }
}

static void
dump_bat (GstMpegtsSection * section)
{
  const GstMpegtsBAT *bat = gst_mpegts_section_get_bat (section);
  guint i, len;

  g_assert (bat);

  g_printf ("     bouquet_id     : 0x%04x\n", section->subtable_extension);
  dump_descriptors (bat->descriptors, 7);
  len = bat->streams->len;
  g_printf ("     %d Streams:\n", len);
  for (i = 0; i < len; i++) {
    GstMpegtsBATStream *stream = g_ptr_array_index (bat->streams, i);
    g_printf
        ("       transport_stream_id:0x%04x , original_network_id:0x%02x\n",
        stream->transport_stream_id, stream->original_network_id);
    dump_descriptors (stream->descriptors, 9);
  }
}

static void
dump_sdt (GstMpegtsSection * section)
{
  const GstMpegtsSDT *sdt = gst_mpegts_section_get_sdt (section);
  guint i, len;

  g_assert (sdt);

  g_printf ("     original_network_id : 0x%04x\n", sdt->original_network_id);
  g_printf ("     actual_ts           : %s\n",
      sdt->actual_ts ? "TRUE" : "FALSE");
  len = sdt->services->len;
  g_printf ("     %d Services:\n", len);
  for (i = 0; i < len; i++) {
    GstMpegtsSDTService *service = g_ptr_array_index (sdt->services, i);
    g_print
        ("       service_id:0x%04x, EIT_schedule_flag:%d, EIT_present_following_flag:%d\n",
        service->service_id, service->EIT_schedule_flag,
        service->EIT_present_following_flag);
    g_print
        ("       running_status:0x%02x (%s), free_CA_mode:%d (%s)\n",
        service->running_status,
        enum_name (GST_TYPE_MPEGTS_RUNNING_STATUS, service->running_status),
        service->free_CA_mode,
        service->free_CA_mode ? "MAYBE SCRAMBLED" : "NOT SCRAMBLED");
    dump_descriptors (service->descriptors, 9);
  }
}


static void
dump_sit (GstMpegtsSection * section)
{
  const GstMpegtsSIT *sit = gst_mpegts_section_get_sit (section);
  guint i, len;

  g_assert (sit);

  dump_descriptors (sit->descriptors, 7);
  len = sit->services->len;
  g_printf ("     %d Services:\n", len);
  for (i = 0; i < len; i++) {
    GstMpegtsSITService *service = g_ptr_array_index (sit->services, i);
    g_print
        ("       service_id:0x%04x, running_status:0x%02x (%s)\n",
        service->service_id, service->running_status,
        enum_name (GST_TYPE_MPEGTS_RUNNING_STATUS, service->running_status));
    dump_descriptors (service->descriptors, 9);
  }
}


static void
dump_tdt (GstMpegtsSection * section)
{
  GstDateTime *date = gst_mpegts_section_get_tdt (section);

  if (date) {
    gchar *str = gst_date_time_to_iso8601_string (date);
    g_printf ("     utc_time : %s\n", str);
    g_free (str);
    gst_date_time_unref (date);
  } else {
    g_printf ("     No utc_time present\n");
  }
}

static void
dump_tot (GstMpegtsSection * section)
{
  const GstMpegtsTOT *tot = gst_mpegts_section_get_tot (section);
  gchar *str = gst_date_time_to_iso8601_string (tot->utc_time);

  g_printf ("     utc_time : %s\n", str);
  dump_descriptors (tot->descriptors, 7);
  g_free (str);
}

static void
dump_mgt (GstMpegtsSection * section)
{
  const GstMpegtsAtscMGT *mgt = gst_mpegts_section_get_atsc_mgt (section);
  gint i;

  g_printf ("     protocol_version    : %u\n", mgt->protocol_version);
  g_printf ("     tables number       : %d\n", mgt->tables->len);
  for (i = 0; i < mgt->tables->len; i++) {
    GstMpegtsAtscMGTTable *table = g_ptr_array_index (mgt->tables, i);
    g_printf ("     table %d)\n", i);
    g_printf ("       table_type    : %u\n", table->table_type);
    g_printf ("       pid           : 0x%x\n", table->pid);
    g_printf ("       version_number: %u\n", table->version_number);
    g_printf ("       number_bytes  : %u\n", table->number_bytes);
    dump_descriptors (table->descriptors, 9);
  }
  dump_descriptors (mgt->descriptors, 7);
}

static void
dump_vct (GstMpegtsSection * section)
{
  const GstMpegtsAtscVCT *vct;
  gint i;

  if (GST_MPEGTS_SECTION_TYPE (section) == GST_MPEGTS_SECTION_ATSC_CVCT) {
    vct = gst_mpegts_section_get_atsc_cvct (section);
  } else {
    /* GST_MPEGTS_SECTION_ATSC_TVCT */
    vct = gst_mpegts_section_get_atsc_tvct (section);
  }

  g_assert (vct);

  g_printf ("     transport_stream_id : 0x%04x\n", vct->transport_stream_id);
  g_printf ("     protocol_version    : %u\n", vct->protocol_version);
  g_printf ("     %d Sources:\n", vct->sources->len);
  for (i = 0; i < vct->sources->len; i++) {
    GstMpegtsAtscVCTSource *source = g_ptr_array_index (vct->sources, i);
    g_print ("       short_name: %s\n", source->short_name);
    g_print ("       major_channel_number: %u, minor_channel_number: %u\n",
        source->major_channel_number, source->minor_channel_number);
    g_print ("       modulation_mode: %u\n", source->modulation_mode);
    g_print ("       carrier_frequency: %u\n", source->carrier_frequency);
    g_print ("       channel_tsid: %u\n", source->channel_TSID);
    g_print ("       program_number: %u\n", source->program_number);
    g_print ("       ETM_location: %u\n", source->ETM_location);
    g_print ("       access_controlled: %u\n", source->access_controlled);
    g_print ("       hidden: %u\n", source->hidden);
    if (section->table_id == GST_MPEGTS_SECTION_ATSC_CVCT) {
      g_print ("       path_select: %u\n", source->path_select);
      g_print ("       out_of_band: %u\n", source->out_of_band);
    }
    g_print ("       hide_guide: %u\n", source->hide_guide);
    g_print ("       service_type: %u\n", source->service_type);
    g_print ("       source_id: %u\n", source->source_id);

    dump_descriptors (source->descriptors, 9);
  }
  dump_descriptors (vct->descriptors, 7);
}

static void
dump_cat (GstMpegtsSection * section)
{
  GPtrArray *descriptors;

  descriptors = gst_mpegts_section_get_cat (section);
  g_assert (descriptors);
  dump_descriptors (descriptors, 7);
  g_ptr_array_unref (descriptors);
}

static const gchar *
scte_descriptor_name (guint8 tag)
{
  switch (tag) {
    case 0x00:
      return "avail";
    case 0x01:
      return "DTMF";
    case 0x02:
      return "segmentation";
    case 0x03:
      return "time";
    case 0x04:
      return "audio";
    default:
      return "UNKNOWN";
  }
}

static void
dump_scte_descriptors (GPtrArray * descriptors, guint spacing)
{
  guint i;

  for (i = 0; i < descriptors->len; i++) {
    GstMpegtsDescriptor *desc = g_ptr_array_index (descriptors, i);
    g_printf ("%*s [scte descriptor 0x%02x (%s) length:%d]\n", spacing, "",
        desc->tag, scte_descriptor_name (desc->tag), desc->length);
    if (DUMP_DESCRIPTORS)
      dump_memory_content (desc, spacing + 2);
    /* FIXME : Add parsing of SCTE descriptors */
  }
}


static void
dump_scte_sit (GstMpegtsSection * section)
{
  const GstMpegtsSCTESIT *sit = gst_mpegts_section_get_scte_sit (section);
  guint i, len;

  g_assert (sit);

  g_printf ("     encrypted_packet    : %d\n", sit->encrypted_packet);
  if (sit->encrypted_packet) {
    g_printf ("     encryption_algorithm: %d\n", sit->encryption_algorithm);
    g_printf ("     cw_index            : %d\n", sit->cw_index);
    g_printf ("     tier                : %d\n", sit->tier);
  }
  g_printf ("     pts_adjustment      : %" G_GUINT64_FORMAT " (%"
      GST_TIME_FORMAT ")\n", sit->pts_adjustment,
      GST_TIME_ARGS (MPEGTIME_TO_GSTTIME (sit->pts_adjustment)));
  g_printf ("     command_type        : %d\n", sit->splice_command_type);

  if ((len = sit->splices->len)) {
    g_printf ("     %d splice(s):\n", len);
    for (i = 0; i < len; i++) {
      GstMpegtsSCTESpliceEvent *event = g_ptr_array_index (sit->splices, i);
      g_printf ("     event_id:%d event_cancel_indicator:%d\n",
          event->splice_event_id, event->splice_event_cancel_indicator);
      if (!event->splice_event_cancel_indicator) {
        g_printf ("       out_of_network_indicator:%d\n",
            event->out_of_network_indicator);
        if (event->program_splice_flag) {
          if (event->program_splice_time_specified)
            g_printf ("       program_splice_time:%" G_GUINT64_FORMAT " (%"
                GST_TIME_FORMAT ")\n", event->program_splice_time,
                GST_TIME_ARGS (MPEGTIME_TO_GSTTIME
                    (event->program_splice_time)));
          else
            g_printf ("       program_splice_time not specified\n");
        }
        if (event->duration_flag) {
          g_printf ("       break_duration_auto_return:%d\n",
              event->break_duration_auto_return);
          g_printf ("       break_duration:%" G_GUINT64_FORMAT " (%"
              GST_TIME_FORMAT ")\n", event->break_duration,
              GST_TIME_ARGS (MPEGTIME_TO_GSTTIME (event->break_duration)));

        }
        g_printf ("       unique_program_id  : %d\n", event->unique_program_id);
        g_printf ("       avail num/expected : %d/%d\n",
            event->avail_num, event->avails_expected);
      }
    }
  }

  dump_scte_descriptors (sit->descriptors, 4);
}

static void
dump_section (GstMpegtsSection * section)
{
  switch (GST_MPEGTS_SECTION_TYPE (section)) {
    case GST_MPEGTS_SECTION_PAT:
      dump_pat (section);
      break;
    case GST_MPEGTS_SECTION_PMT:
      dump_pmt (section);
      break;
    case GST_MPEGTS_SECTION_CAT:
      dump_cat (section);
      break;
    case GST_MPEGTS_SECTION_TDT:
      dump_tdt (section);
      break;
    case GST_MPEGTS_SECTION_TOT:
      dump_tot (section);
      break;
    case GST_MPEGTS_SECTION_SDT:
      dump_sdt (section);
      break;
    case GST_MPEGTS_SECTION_NIT:
      dump_nit (section);
      break;
    case GST_MPEGTS_SECTION_BAT:
      dump_bat (section);
      break;
    case GST_MPEGTS_SECTION_EIT:
      dump_eit (section);
      break;
    case GST_MPEGTS_SECTION_SIT:
      dump_sit (section);
      break;
    case GST_MPEGTS_SECTION_ATSC_MGT:
      dump_mgt (section);
      break;
    case GST_MPEGTS_SECTION_ATSC_CVCT:
    case GST_MPEGTS_SECTION_ATSC_TVCT:
      dump_vct (section);
      break;
    case GST_MPEGTS_SECTION_ATSC_EIT:
      dump_atsc_eit (section);
      break;
    case GST_MPEGTS_SECTION_ATSC_ETT:
      dump_ett (section);
      break;
    case GST_MPEGTS_SECTION_ATSC_STT:
      dump_stt (section);
      break;
    case GST_MPEGTS_SECTION_SCTE_SIT:
      dump_scte_sit (section);
      break;
    default:
      g_printf ("     Unknown section type\n");
      break;
  }
}

static void
_on_bus_message (GstBus * bus, GstMessage * message, GMainLoop * mainloop)
{
  /* g_printf ("Got message %s\n", GST_MESSAGE_TYPE_NAME (message)); */

  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_ERROR:
    case GST_MESSAGE_EOS:
      g_main_loop_quit (mainloop);
      break;
    case GST_MESSAGE_ELEMENT:
    {
      GstMpegtsSection *section;
      if ((section = gst_message_parse_mpegts_section (message))) {
        const gchar *table_name;

        table_name = table_id_name (section->table_id);
        g_print
            ("Got section: PID:0x%04x type:%s (table_id 0x%02x (%s)) at offset %"
            G_GUINT64_FORMAT "\n", section->pid,
            enum_name (GST_TYPE_MPEGTS_SECTION_TYPE, section->section_type),
            section->table_id, table_name, section->offset);
        if (!section->short_section) {
          g_print
              ("   subtable_extension:0x%04x, version_number:0x%02x\n",
              section->subtable_extension, section->version_number);
          g_print
              ("   section_number:0x%02x last_section_number:0x%02x crc:0x%08x\n",
              section->section_number, section->last_section_number,
              section->crc);
        }
        dump_section (section);
        g_printf ("\n\n");
        gst_mpegts_section_unref (section);
      }
      break;
    }
    default:
      break;
  }
}

int
main (int argc, gchar ** argv)
{
  GstElement *pipeline = NULL;
  GError *error = NULL;
  GstBus *bus;
  GMainLoop *mainloop;

  gst_init (&argc, &argv);

  gst_mpegts_initialize ();

  pipeline = gst_parse_launchv ((const gchar **) &argv[1], &error);
  if (error) {
    g_printf ("pipeline could not be constructed: %s\n", error->message);
    g_error_free (error);
    return 1;
  }

  /* Hack: ensure all enum type classes are loaded */
  g_type_class_ref (GST_TYPE_MPEGTS_SECTION_TYPE);
  g_type_class_ref (GST_TYPE_MPEGTS_SECTION_TABLE_ID);
  g_type_class_ref (GST_TYPE_MPEGTS_RUNNING_STATUS);
  g_type_class_ref (GST_TYPE_MPEGTS_DESCRIPTOR_TYPE);
  g_type_class_ref (GST_TYPE_MPEGTS_DVB_DESCRIPTOR_TYPE);
  g_type_class_ref (GST_TYPE_MPEGTS_DVB_EXTENDED_DESCRIPTOR_TYPE);
  g_type_class_ref (GST_TYPE_MPEGTS_ATSC_DESCRIPTOR_TYPE);
  g_type_class_ref (GST_TYPE_MPEGTS_ISDB_DESCRIPTOR_TYPE);
  g_type_class_ref (GST_TYPE_MPEGTS_SCTE_DESCRIPTOR_TYPE);
  g_type_class_ref (GST_TYPE_MPEGTS_MISC_DESCRIPTOR_TYPE);
  g_type_class_ref (GST_TYPE_MPEGTS_ISO639_AUDIO_TYPE);
  g_type_class_ref (GST_TYPE_MPEGTS_DVB_SERVICE_TYPE);
  g_type_class_ref (GST_TYPE_MPEGTS_DVB_TELETEXT_TYPE);
  g_type_class_ref (GST_TYPE_MPEGTS_STREAM_TYPE);
  g_type_class_ref (GST_TYPE_MPEGTS_SECTION_DVB_TABLE_ID);
  g_type_class_ref (GST_TYPE_MPEGTS_SECTION_ATSC_TABLE_ID);
  g_type_class_ref (GST_TYPE_MPEGTS_SECTION_SCTE_TABLE_ID);
  g_type_class_ref (GST_TYPE_MPEGTS_MODULATION_TYPE);
  g_type_class_ref (GST_TYPE_MPEGTS_DVB_CODE_RATE);
  g_type_class_ref (GST_TYPE_MPEGTS_CABLE_OUTER_FEC_SCHEME);
  g_type_class_ref (GST_TYPE_MPEGTS_TERRESTRIAL_TRANSMISSION_MODE);
  g_type_class_ref (GST_TYPE_MPEGTS_TERRESTRIAL_GUARD_INTERVAL);
  g_type_class_ref (GST_TYPE_MPEGTS_TERRESTRIAL_HIERARCHY);
  g_type_class_ref (GST_TYPE_MPEGTS_DVB_LINKAGE_TYPE);
  g_type_class_ref (GST_TYPE_MPEGTS_DVB_LINKAGE_HAND_OVER_TYPE);
  g_type_class_ref (GST_TYPE_MPEGTS_COMPONENT_STREAM_CONTENT);
  g_type_class_ref (GST_TYPE_MPEGTS_CONTENT_NIBBLE_HI);
  g_type_class_ref (GST_TYPE_MPEGTS_SCTE_STREAM_TYPE);
  g_type_class_ref (GST_TYPE_MPEGTS_HDMV_STREAM_TYPE);
  g_type_class_ref (GST_TYPE_MPEGTS_SECTION_SCTE_TABLE_ID);

  mainloop = g_main_loop_new (NULL, FALSE);

  /* Put a bus handler */
  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  gst_bus_add_signal_watch (bus);
  g_signal_connect (bus, "message", (GCallback) _on_bus_message, mainloop);

  /* Start pipeline */
  gst_element_set_state (pipeline, GST_STATE_PLAYING);
  g_main_loop_run (mainloop);

  gst_element_set_state (pipeline, GST_STATE_NULL);

  gst_object_unref (pipeline);
  gst_object_unref (bus);

  return 0;
}
