/*
 * gstmpegtsdescriptor.c -
 * Copyright (C) 2013 Edward Hervey
 *
 * Authors:
 *   Edward Hervey <edward@collabora.com>
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

#include <stdlib.h>
#include <string.h>

#include "mpegts.h"
#include "gstmpegts-private.h"


/**
 * SECTION:gst-dvb-descriptor
 * @title: DVB variants of MPEG-TS descriptors
 * @short_description: Descriptors for the various DVB specifications
 * @include: gst/mpegts/mpegts.h
 *
 */

/*
 * TODO
 *
 * * Add common validation code for data presence and minimum/maximum expected
 *   size.
 * * Add parsing methods for the following descriptors that were previously
 *   handled in mpegtsbase:
 *   * GST_MTS_DESC_DVB_DATA_BROADCAST_ID
 *   * GST_MTS_DESC_DVB_CAROUSEL_IDENTIFIER
 */

#define BCD_UN(a) ((a) & 0x0f)
#define BCD_DEC(a) (((a) >> 4) & 0x0f)
#define BCD(a) (BCD_UN(a) + 10 * BCD_DEC(a))
#define BCD_16(a) (BCD(a[1]) + 100 * BCD(a[0]))
#define BCD_28(a) (BCD_DEC(a[3]) + 10 * BCD(a[2]) + 1000 * BCD(a[1]) + 100000 * BCD(a[0]))
#define BCD_32(a) (BCD(a[3]) + 100 * BCD(a[2]) + 10000 * BCD(a[1]) + 1000000 * BCD(a[0]))

#define DEFINE_STATIC_COPY_FUNCTION(type, name) \
static type * _##name##_copy (type * source) \
{ \
  return g_memdup2 (source, sizeof (type)); \
}

#define DEFINE_STATIC_FREE_FUNCTION(type, name) \
static void _##name##_free (type * source) \
{ \
  g_free (source); \
}

/* GST_MTS_DESC_DVB_NETWORK_NAME (0x40) */
/**
 * gst_mpegts_descriptor_parse_dvb_network_name:
 * @descriptor: a %GST_MTS_DESC_DVB_NETWORK_NAME #GstMpegtsDescriptor
 * @name: (out) (transfer full): the extracted name
 *
 * Parses out the dvb network name from the @descriptor:
 *
 * Returns: %TRUE if the parsing happened correctly, else %FALSE.
 */
gboolean
gst_mpegts_descriptor_parse_dvb_network_name (const GstMpegtsDescriptor *
    descriptor, gchar ** name)
{
  g_return_val_if_fail (descriptor != NULL && name != NULL, FALSE);
  /* We need at least one byte of data for the string */
  __common_desc_checks (descriptor, GST_MTS_DESC_DVB_NETWORK_NAME, 1, FALSE);

  *name = get_encoding_and_convert ((gchar *) descriptor->data + 2,
      descriptor->data[1]);
  return TRUE;
}

/**
 * gst_mpegts_descriptor_from_dvb_network_name:
 * @name: the network name to set
 *
 * Creates a #GstMpegtsDescriptor to be a %GST_MTS_DESC_DVB_NETWORK_NAME,
 * with the network name @name. The data field of the #GstMpegtsDescriptor
 * will be allocated, and transferred to the caller.
 *
 * Returns: (transfer full): the #GstMpegtsDescriptor or %NULL on fail
 */
GstMpegtsDescriptor *
gst_mpegts_descriptor_from_dvb_network_name (const gchar * name)
{
  GstMpegtsDescriptor *descriptor;
  guint8 *converted_name;
  gsize size;

  g_return_val_if_fail (name != NULL, NULL);

  converted_name = dvb_text_from_utf8 (name, &size);

  if (size >= 256) {
    g_free (converted_name);
    return NULL;
  }

  if (!converted_name) {
    GST_WARNING ("Could not find proper encoding for string `%s`", name);
    return NULL;
  }

  descriptor = _new_descriptor (GST_MTS_DESC_DVB_NETWORK_NAME, size);
  memcpy (descriptor->data + 2, converted_name, size);
  g_free (converted_name);

  return descriptor;
}

/* GST_MTS_DESC_DVB_SERVICE_LIST (0x41) */

DEFINE_STATIC_COPY_FUNCTION (GstMpegtsDVBServiceListItem,
    gst_mpegts_dvb_service_list_item);

DEFINE_STATIC_FREE_FUNCTION (GstMpegtsDVBServiceListItem,
    gst_mpegts_dvb_service_list_item);

G_DEFINE_BOXED_TYPE (GstMpegtsDVBServiceListItem,
    gst_mpegts_dvb_service_list_item,
    (GBoxedCopyFunc) _gst_mpegts_dvb_service_list_item_copy,
    (GFreeFunc) _gst_mpegts_dvb_service_list_item_free);

/**
 * gst_mpegts_descriptor_parse_dvb_service_list:
 * @descriptor: a %GST_MTS_DESC_DVB_SERVICE_LIST #GstMpegtsDescriptor
 * @list: (out) (transfer full) (element-type GstMpegtsDVBServiceListItem):
 * the list of services
 *
 * Parses out a list of services from the @descriptor:
 *
 * Returns: %TRUE if the parsing happened correctly, else %FALSE.
 */
gboolean
gst_mpegts_descriptor_parse_dvb_service_list (const GstMpegtsDescriptor *
    descriptor, GPtrArray ** list)
{
  guint8 *data, i;

  g_return_val_if_fail (descriptor != NULL && list != NULL, FALSE);
  /* a entry has 3 bytes, 2 for service id, 1 for service type */
  __common_desc_checks (descriptor, GST_MTS_DESC_DVB_SERVICE_LIST, 3, FALSE);

  data = (guint8 *) descriptor->data + 2;

  *list = g_ptr_array_new_with_free_func ((GDestroyNotify)
      _gst_mpegts_dvb_service_list_item_free);

  for (i = 0; i < descriptor->length - 2; i += 3) {
    GstMpegtsDVBServiceListItem *item = g_new0 (GstMpegtsDVBServiceListItem, 1);

    g_ptr_array_add (*list, item);
    item->service_id = GST_READ_UINT16_BE (data);
    data += 2;

    item->type = *data;
    data += 1;
  }

  return TRUE;
}

/* GST_MTS_DESC_DVB_STUFFING (0x42) */
/**
 * gst_mpegts_descriptor_parse_dvb_stuffing:
 * @descriptor: a %GST_MTS_DESC_DVB_STUFFING #GstMpegtsDescriptor
 * @stuffing_bytes: (out) (transfer full): the stuffing bytes
 *
 * Parses out the stuffing bytes from the @descriptor.
 *
 * Returns: %TRUE if the parsing happened correctly, else %FALSE.
 */
gboolean
gst_mpegts_descriptor_parse_dvb_stuffing (const GstMpegtsDescriptor *
    descriptor, guint8 ** stuffing_bytes)
{
  guint8 *data;

  g_return_val_if_fail (descriptor != NULL && stuffing_bytes != NULL, FALSE);
  __common_desc_check_base (descriptor, GST_MTS_DESC_DVB_STUFFING, FALSE);

  data = (guint8 *) descriptor->data + 2;

  *stuffing_bytes = g_memdup2 (data, descriptor->length);

  return TRUE;
}

/* GST_MTS_DESC_DVB_SATELLITE_DELIVERY_SYSTEM (0x43) */

DEFINE_STATIC_COPY_FUNCTION (GstMpegtsSatelliteDeliverySystemDescriptor,
    gst_mpegts_satellite_delivery_system_descriptor);

DEFINE_STATIC_FREE_FUNCTION (GstMpegtsSatelliteDeliverySystemDescriptor,
    gst_mpegts_satellite_delivery_system_descriptor);

G_DEFINE_BOXED_TYPE (GstMpegtsSatelliteDeliverySystemDescriptor,
    gst_mpegts_satellite_delivery_system_descriptor,
    (GBoxedCopyFunc) _gst_mpegts_satellite_delivery_system_descriptor_copy,
    (GFreeFunc) _gst_mpegts_satellite_delivery_system_descriptor_free);

/**
 * gst_mpegts_descriptor_parse_satellite_delivery_system:
 * @descriptor: a %GST_MTS_DESC_DVB_SATELLITE_DELIVERY_SYSTEM #GstMpegtsDescriptor
 * @res: (out) (transfer none): the #GstMpegtsSatelliteDeliverySystemDescriptor to fill
 *
 * Extracts the satellite delivery system information from @descriptor.
 *
 * Returns: %TRUE if parsing succeeded, else %FALSE.
 */
gboolean
gst_mpegts_descriptor_parse_satellite_delivery_system (const GstMpegtsDescriptor
    * descriptor, GstMpegtsSatelliteDeliverySystemDescriptor * res)
{
  guint8 *data;
  guint8 tmp;

  g_return_val_if_fail (descriptor != NULL && res != NULL, FALSE);
  /* This descriptor is always 11 bytes long */
  __common_desc_checks_exact (descriptor,
      GST_MTS_DESC_DVB_SATELLITE_DELIVERY_SYSTEM, 11, FALSE);

  data = (guint8 *) descriptor->data + 2;

  /* BCD coded frequency in GHz (decimal point occurs after the 3rd character)
   * So direct BCD gives us units of (GHz / 100 000) = 10 kHz*/
  res->frequency = BCD_32 (data) * 10;
  data += 4;
  /* BCD codec position in degrees (float pointer after the 3rd character) */
  res->orbital_position = (BCD_16 (data)) / 10.0;
  data += 2;

  tmp = *data;
  res->west_east = (tmp & 0x80) == 0x80;
  res->polarization = (tmp >> 7) & 0x03;
  res->modulation_system = (tmp & 0x04) == 0x04;
  if (res->modulation_system)
    res->roll_off = (tmp >> 3 & 0x03);
  else
    res->roll_off = GST_MPEGTS_ROLLOFF_AUTO;
  switch (tmp & 0x03) {
    case 0x00:
      res->modulation_type = GST_MPEGTS_MODULATION_QAM_AUTO;
      break;
    case 0x01:
      res->modulation_type = GST_MPEGTS_MODULATION_QPSK;
      break;
    case 0x02:
      res->modulation_type = GST_MPEGTS_MODULATION_PSK_8;
      break;
    case 0x03:
      res->modulation_type = GST_MPEGTS_MODULATION_QAM_16;
      break;
    default:
      res->modulation_type = GST_MPEGTS_MODULATION_QAM_AUTO;
      break;
  }
  data += 1;
  /* symbol_rate is in Msymbols/ (decimal point occurs after 3rd character) */
  /* So direct BCD gives us units of (Msymbol / 10 000) = 100 sym/s */
  res->symbol_rate = BCD_28 (data) * 100;
  data += 3;
  /* fec_inner */
  switch (*data >> 4) {
    case 0x01:
      res->fec_inner = GST_MPEGTS_FEC_1_2;
      break;
    case 0x02:
      res->fec_inner = GST_MPEGTS_FEC_2_3;
      break;
    case 0x03:
      res->fec_inner = GST_MPEGTS_FEC_3_4;
      break;
    case 0x04:
      res->fec_inner = GST_MPEGTS_FEC_5_6;
      break;
    case 0x05:
      res->fec_inner = GST_MPEGTS_FEC_7_8;
      break;
    case 0x06:
      res->fec_inner = GST_MPEGTS_FEC_8_9;
      break;
    case 0x07:
      res->fec_inner = GST_MPEGTS_FEC_3_5;
      break;
    case 0x08:
      res->fec_inner = GST_MPEGTS_FEC_4_5;
      break;
    case 0x09:
      res->fec_inner = GST_MPEGTS_FEC_9_10;
      break;
    case 0x0f:
      res->fec_inner = GST_MPEGTS_FEC_NONE;
      break;
    default:
      res->fec_inner = GST_MPEGTS_FEC_AUTO;
      break;
  }

  return TRUE;
}

/* GST_MTS_DESC_DVB_CABLE_DELIVERY_SYSTEM (0x44) */

DEFINE_STATIC_COPY_FUNCTION (GstMpegtsCableDeliverySystemDescriptor,
    gst_mpegts_dvb_cable_delivery_system_descriptor);

void gst_mpegts_dvb_cable_delivery_system_descriptor_free
    (GstMpegtsCableDeliverySystemDescriptor * source)
{
  g_free (source);
}

G_DEFINE_BOXED_TYPE (GstMpegtsCableDeliverySystemDescriptor,
    gst_mpegts_dvb_cable_delivery_system_descriptor,
    (GBoxedCopyFunc) _gst_mpegts_dvb_cable_delivery_system_descriptor_copy,
    (GFreeFunc) gst_mpegts_dvb_cable_delivery_system_descriptor_free);

/**
 * gst_mpegts_descriptor_parse_cable_delivery_system:
 * @descriptor: a %GST_MTS_DESC_DVB_CABLE_DELIVERY_SYSTEM #GstMpegtsDescriptor
 * @res: (out) (transfer none): the #GstMpegtsCableDeliverySystemDescriptor to fill
 *
 * Extracts the cable delivery system information from @descriptor.
 *
 * Returns: %TRUE if parsing succeeded, else %FALSE.
 */
gboolean
gst_mpegts_descriptor_parse_cable_delivery_system (const GstMpegtsDescriptor *
    descriptor, GstMpegtsCableDeliverySystemDescriptor * res)
{
  guint8 *data;

  g_return_val_if_fail (descriptor != NULL && res != NULL, FALSE);
  /* This descriptor is always 11 bytes long */
  __common_desc_checks_exact (descriptor,
      GST_MTS_DESC_DVB_CABLE_DELIVERY_SYSTEM, 11, FALSE);

  data = (guint8 *) descriptor->data + 2;
  /* BCD in MHz, decimal place after the fourth character */
  res->frequency = BCD_32 (data) * 100;
  data += 5;
  /* fec_out (4bits) */
  res->outer_fec = *data++ & 0x0f;
  switch (*data) {
    case 0x00:
      res->modulation = GST_MPEGTS_MODULATION_NONE;
      break;
    case 0x01:
      res->modulation = GST_MPEGTS_MODULATION_QAM_16;
      break;
    case 0x02:
      res->modulation = GST_MPEGTS_MODULATION_QAM_32;
      break;
    case 0x03:
      res->modulation = GST_MPEGTS_MODULATION_QAM_64;
      break;
    case 0x04:
      res->modulation = GST_MPEGTS_MODULATION_QAM_128;
      break;
    case 0x05:
      res->modulation = GST_MPEGTS_MODULATION_QAM_256;
      break;
    default:
      GST_WARNING ("Unsupported cable modulation type: 0x%02x", *data);
      res->modulation = GST_MPEGTS_MODULATION_NONE;
      break;
  }

  data += 1;
  /* symbol_rate is in Msymbols/ (decimal point occurs after 3rd character) */
  /* So direct BCD gives us units of (Msymbol / 10 000) = 100 sym/s */
  res->symbol_rate = BCD_28 (data) * 100;
  data += 3;
  /* fec_inner */
  switch (*data & 0xf) {
    case 0x00:
      res->fec_inner = GST_MPEGTS_FEC_AUTO;
      break;
    case 0x01:
      res->fec_inner = GST_MPEGTS_FEC_1_2;
      break;
    case 0x02:
      res->fec_inner = GST_MPEGTS_FEC_2_3;
      break;
    case 0x03:
      res->fec_inner = GST_MPEGTS_FEC_3_4;
      break;
    case 0x04:
      res->fec_inner = GST_MPEGTS_FEC_5_6;
      break;
    case 0x05:
      res->fec_inner = GST_MPEGTS_FEC_7_8;
      break;
    case 0x06:
      res->fec_inner = GST_MPEGTS_FEC_8_9;
      break;
    case 0x07:
      res->fec_inner = GST_MPEGTS_FEC_3_5;
      break;
    case 0x08:
      res->fec_inner = GST_MPEGTS_FEC_4_5;
      break;
    case 0x09:
      res->fec_inner = GST_MPEGTS_FEC_9_10;
      break;
    case 0x0f:
      res->fec_inner = GST_MPEGTS_FEC_NONE;
      break;
    default:
      res->fec_inner = GST_MPEGTS_FEC_AUTO;
      break;
  }

  return TRUE;
}

/* GST_MTS_DESC_DVB_BOUQUET_NAME (0x47) */
/**
 * gst_mpegts_descriptor_parse_dvb_bouquet_name:
 * @bouquet_name: (out) (transfer full) (allow-none): the bouquet name
 *
 * Extracts the bouquet name from @descriptor.
 *
 * Returns: %TRUE if parsing succeeded, else %FALSE.
 */
gboolean
gst_mpegts_descriptor_parse_dvb_bouquet_name (const GstMpegtsDescriptor *
    descriptor, gchar ** bouquet_name)
{
  guint8 *data;

  g_return_val_if_fail (descriptor != NULL && bouquet_name != NULL, FALSE);
  __common_desc_checks (descriptor, GST_MTS_DESC_DVB_BOUQUET_NAME, 1, FALSE);

  data = (guint8 *) descriptor->data + 2;

  *bouquet_name =
      get_encoding_and_convert ((const gchar *) data, descriptor->length);

  return TRUE;
}

/* GST_MTS_DESC_DVB_SERVICE (0x48) */
/**
 * gst_mpegts_descriptor_parse_dvb_service:
 * @descriptor: a %GST_MTS_DESC_DVB_SERVICE #GstMpegtsDescriptor
 * @service_type: (out) (allow-none): the service type
 * @service_name: (out) (transfer full) (allow-none): the service name
 * @provider_name: (out) (transfer full) (allow-none): the provider name
 *
 * Extracts the dvb service information from @descriptor.
 *
 * Returns: %TRUE if parsing succeeded, else %FALSE.
 */
gboolean
gst_mpegts_descriptor_parse_dvb_service (const GstMpegtsDescriptor *
    descriptor, GstMpegtsDVBServiceType * service_type, gchar ** service_name,
    gchar ** provider_name)
{
  guint8 *data;

  g_return_val_if_fail (descriptor != NULL, FALSE);
  /* Need at least 3 bytes (type and 2 bytes for the string length) */
  __common_desc_checks (descriptor, GST_MTS_DESC_DVB_SERVICE, 3, FALSE);

  data = (guint8 *) descriptor->data + 2;

  if (service_type)
    *service_type = *data;
  data += 1;
  if (provider_name)
    *provider_name = get_encoding_and_convert ((const gchar *) data + 1, *data);
  data += *data + 1;
  if (service_name)
    *service_name = get_encoding_and_convert ((const gchar *) data + 1, *data);

  return TRUE;
}

/**
 * gst_mpegts_descriptor_from_dvb_service:
 * @service_type: Service type defined as a #GstMpegtsDVBServiceType
 * @service_name: (allow-none): Name of the service
 * @service_provider: (allow-none): Name of the service provider
 *
 * Fills a #GstMpegtsDescriptor to be a %GST_MTS_DESC_DVB_SERVICE.
 * The data field of the #GstMpegtsDescriptor will be allocated,
 * and transferred to the caller.
 *
 * Returns: (transfer full): the #GstMpegtsDescriptor or %NULL on fail
 */
GstMpegtsDescriptor *
gst_mpegts_descriptor_from_dvb_service (GstMpegtsDVBServiceType service_type,
    const gchar * service_name, const gchar * service_provider)
{
  GstMpegtsDescriptor *descriptor = NULL;
  guint8 *conv_provider_name = NULL, *conv_service_name = NULL;
  gsize provider_size = 0, service_size = 0;
  guint8 *data;

  if (service_provider) {
    conv_provider_name = dvb_text_from_utf8 (service_provider, &provider_size);

    if (!conv_provider_name) {
      GST_WARNING ("Could not find proper encoding for string `%s`",
          service_provider);
      goto beach;
    }
  }

  if (provider_size >= 256) {
    GST_WARNING ("Service provider string too big (%" G_GSIZE_FORMAT " > 256)",
        provider_size);
    goto beach;
  }

  if (service_name) {
    conv_service_name = dvb_text_from_utf8 (service_name, &service_size);

    if (!conv_service_name) {
      GST_WARNING ("Could not find proper encoding for string `%s`",
          service_name);
      goto beach;
    }
  }

  if (service_size >= 256) {
    GST_WARNING ("Service name string too big (%" G_GSIZE_FORMAT " > 256)",
        service_size);
    goto beach;
  }

  descriptor =
      _new_descriptor (GST_MTS_DESC_DVB_SERVICE,
      3 + provider_size + service_size);

  data = descriptor->data + 2;
  *data++ = service_type;
  *data++ = provider_size;
  if (conv_provider_name)
    memcpy (data, conv_provider_name, provider_size);

  data += provider_size;
  *data++ = service_size;
  if (conv_service_name)
    memcpy (data, conv_service_name, service_size);

beach:
  g_free (conv_service_name);
  g_free (conv_provider_name);

  return descriptor;
}

/* GST_MTS_DESC_DVB_LINKAGE (0x4A) */
static GstMpegtsDVBLinkageDescriptor *
_gst_mpegts_dvb_linkage_descriptor_copy (GstMpegtsDVBLinkageDescriptor * source)
{
  GstMpegtsDVBLinkageDescriptor *copy;

  copy = g_memdup2 (source, sizeof (GstMpegtsDVBLinkageDescriptor));

  switch (source->linkage_type) {
    case GST_MPEGTS_DVB_LINKAGE_MOBILE_HAND_OVER:
      copy->linkage_data = g_memdup2 (source->linkage_data,
          sizeof (GstMpegtsDVBLinkageMobileHandOver));
      break;
    case GST_MPEGTS_DVB_LINKAGE_EVENT:
      copy->linkage_data = g_memdup2 (source->linkage_data,
          sizeof (GstMpegtsDVBLinkageEvent));
      break;
    case GST_MPEGTS_DVB_LINKAGE_EXTENDED_EVENT:
      copy->linkage_data = g_ptr_array_ref (source->linkage_data);
      break;
    default:
      break;
  }

  copy->private_data_bytes = g_memdup2 (source->private_data_bytes,
      source->private_data_length);

  return copy;
}

void
gst_mpegts_dvb_linkage_descriptor_free (GstMpegtsDVBLinkageDescriptor * source)
{
  if (source->linkage_data)
    switch (source->linkage_type) {
      case GST_MPEGTS_DVB_LINKAGE_MOBILE_HAND_OVER:
        g_free (source->linkage_data);
        break;
      case GST_MPEGTS_DVB_LINKAGE_EVENT:
        g_free (source->linkage_data);
        break;
      case GST_MPEGTS_DVB_LINKAGE_EXTENDED_EVENT:
        g_ptr_array_unref (source->linkage_data);
        break;
      default:
        break;
    }

  g_free (source->private_data_bytes);
  g_free (source);
}

G_DEFINE_BOXED_TYPE (GstMpegtsDVBLinkageDescriptor,
    gst_mpegts_dvb_linkage_descriptor,
    (GBoxedCopyFunc) _gst_mpegts_dvb_linkage_descriptor_copy,
    (GFreeFunc) gst_mpegts_dvb_linkage_descriptor_free);

DEFINE_STATIC_COPY_FUNCTION (GstMpegtsDVBLinkageMobileHandOver,
    gst_mpegts_dvb_linkage_mobile_hand_over);

DEFINE_STATIC_FREE_FUNCTION (GstMpegtsDVBLinkageMobileHandOver,
    gst_mpegts_dvb_linkage_mobile_hand_over);

G_DEFINE_BOXED_TYPE (GstMpegtsDVBLinkageMobileHandOver,
    gst_mpegts_dvb_linkage_mobile_hand_over,
    (GBoxedCopyFunc) _gst_mpegts_dvb_linkage_mobile_hand_over_copy,
    (GFreeFunc) _gst_mpegts_dvb_linkage_mobile_hand_over_free);

DEFINE_STATIC_COPY_FUNCTION (GstMpegtsDVBLinkageEvent,
    gst_mpegts_dvb_linkage_event);

DEFINE_STATIC_FREE_FUNCTION (GstMpegtsDVBLinkageEvent,
    gst_mpegts_dvb_linkage_event);

G_DEFINE_BOXED_TYPE (GstMpegtsDVBLinkageEvent,
    gst_mpegts_dvb_linkage_event,
    (GBoxedCopyFunc) _gst_mpegts_dvb_linkage_event_copy,
    (GFreeFunc) _gst_mpegts_dvb_linkage_event_free);

DEFINE_STATIC_COPY_FUNCTION (GstMpegtsDVBLinkageExtendedEvent,
    gst_mpegts_dvb_linkage_extended_event);

DEFINE_STATIC_FREE_FUNCTION (GstMpegtsDVBLinkageExtendedEvent,
    gst_mpegts_dvb_linkage_extended_event);

G_DEFINE_BOXED_TYPE (GstMpegtsDVBLinkageExtendedEvent,
    gst_mpegts_dvb_linkage_extended_event,
    (GBoxedCopyFunc) _gst_mpegts_dvb_linkage_extended_event_copy,
    (GFreeFunc) _gst_mpegts_dvb_linkage_extended_event_free);

/**
 * gst_mpegts_descriptor_parse_dvb_linkage:
 * @descriptor: a %GST_MTS_DESC_DVB_LINKAGE #GstMpegtsDescriptor
 * @res: (out) (transfer full): the #GstMpegtsDVBLinkageDescriptor to fill
 *
 * Extracts the DVB linkage information from @descriptor.
 *
 * Returns: %TRUE if parsing succeeded, else %FALSE.
 */
gboolean
gst_mpegts_descriptor_parse_dvb_linkage (const GstMpegtsDescriptor * descriptor,
    GstMpegtsDVBLinkageDescriptor ** desc)
{
  guint i;
  guint8 *data, *end;
  GstMpegtsDVBLinkageDescriptor *res;

  g_return_val_if_fail (descriptor != NULL && desc != NULL, FALSE);
  __common_desc_checks (descriptor, GST_MTS_DESC_DVB_LINKAGE, 7, FALSE);

  data = (guint8 *) descriptor->data + 2;
  end = data + descriptor->length;

  res = g_new0 (GstMpegtsDVBLinkageDescriptor, 1);

  res->transport_stream_id = GST_READ_UINT16_BE (data);
  data += 2;

  res->original_network_id = GST_READ_UINT16_BE (data);
  data += 2;

  res->service_id = GST_READ_UINT16_BE (data);
  data += 2;

  res->linkage_type = *data;
  data += 1;

  switch (res->linkage_type) {
    case GST_MPEGTS_DVB_LINKAGE_MOBILE_HAND_OVER:{
      GstMpegtsDVBLinkageMobileHandOver *hand_over;

      if (end - data < 1)
        goto fail;

      hand_over = g_new0 (GstMpegtsDVBLinkageMobileHandOver, 1);
      res->linkage_data = (gpointer) hand_over;

      hand_over->origin_type = (*data) & 0x01;
      hand_over->hand_over_type = (*data >> 4) & 0x0f;
      data += 1;

      if (hand_over->hand_over_type ==
          GST_MPEGTS_DVB_LINKAGE_HAND_OVER_IDENTICAL
          || hand_over->hand_over_type ==
          GST_MPEGTS_DVB_LINKAGE_HAND_OVER_LOCAL_VARIATION
          || hand_over->hand_over_type ==
          GST_MPEGTS_DVB_LINKAGE_HAND_OVER_ASSOCIATED) {
        if (end - data < 2)
          goto fail;

        hand_over->network_id = GST_READ_UINT16_BE (data);
        data += 2;
      }

      if (hand_over->origin_type == 0) {
        if (end - data < 2)
          goto fail;

        hand_over->initial_service_id = GST_READ_UINT16_BE (data);
        data += 2;
      }
      break;
    }
    case GST_MPEGTS_DVB_LINKAGE_EVENT:{
      GstMpegtsDVBLinkageEvent *event;

      if (end - data < 3)
        goto fail;

      event = g_new0 (GstMpegtsDVBLinkageEvent, 1);
      res->linkage_data = (gpointer) event;

      event->target_event_id = GST_READ_UINT16_BE (data);
      data += 2;
      event->target_listed = *data & 0x01;
      event->event_simulcast = (*data >> 1) & 0x01;
      data += 1;
      break;
    }
    case GST_MPEGTS_DVB_LINKAGE_EXTENDED_EVENT:{
      GPtrArray *ext_events;
      ext_events = g_ptr_array_new_with_free_func ((GDestroyNotify)
          _gst_mpegts_dvb_linkage_extended_event_free);

      res->linkage_data = (gpointer) ext_events;

      for (i = 0; i < *data++;) {
        GstMpegtsDVBLinkageExtendedEvent *ext_event;

        if (end - data < 3)
          goto fail;

        ext_event = g_new0 (GstMpegtsDVBLinkageExtendedEvent, 1);
        g_ptr_array_add (res->linkage_data, ext_event);

        ext_event->target_event_id = GST_READ_UINT16_BE (data);
        data += 2;
        i += 2;

        ext_event->target_listed = *data & 0x01;
        ext_event->event_simulcast = (*data >> 1) & 0x01;
        ext_event->link_type = (*data >> 3) & 0x03;
        ext_event->target_id_type = (*data >> 5) & 0x03;
        ext_event->original_network_id_flag = (*data >> 6) & 0x01;
        ext_event->service_id_flag = (*data >> 7) & 0x01;
        data += 1;
        i += 1;

        if (ext_event->target_id_type == 3) {
          if (end - data < 2)
            goto fail;

          ext_event->user_defined_id = GST_READ_UINT16_BE (data);
          data += 2;
          i += 2;
        } else {
          if (ext_event->target_id_type == 1) {
            if (end - data < 2)
              goto fail;

            ext_event->target_transport_stream_id = GST_READ_UINT16_BE (data);
            data += 2;
            i += 2;
          }
          if (ext_event->original_network_id_flag) {
            if (end - data < 2)
              goto fail;

            ext_event->target_original_network_id = GST_READ_UINT16_BE (data);
            data += 2;
            i += 2;
          }
          if (ext_event->service_id_flag) {
            if (end - data < 2)
              goto fail;

            ext_event->target_service_id = GST_READ_UINT16_BE (data);
            data += 2;
            i += 2;
          }
        }
      }
      break;
    }
    default:
      break;
  }

  res->private_data_length = end - data;
  res->private_data_bytes = g_memdup2 (data, res->private_data_length);

  *desc = res;

  return TRUE;

fail:
  gst_mpegts_dvb_linkage_descriptor_free (res);
  return FALSE;
}

/**
 * gst_mpegts_dvb_linkage_descriptor_get_mobile_hand_over:
 * @desc: the #GstMpegtsDVBLinkageDescriptor
 *
 * Returns: The #GstMpegtsDVBLinkageMobileHandOver or %NULL if an error happened
 */
const GstMpegtsDVBLinkageMobileHandOver *
gst_mpegts_dvb_linkage_descriptor_get_mobile_hand_over (const
    GstMpegtsDVBLinkageDescriptor * desc)
{
  g_return_val_if_fail (desc != NULL, NULL);
  g_return_val_if_fail (desc->linkage_type ==
      GST_MPEGTS_DVB_LINKAGE_MOBILE_HAND_OVER, NULL);

  return (const GstMpegtsDVBLinkageMobileHandOver *) desc->linkage_data;
}

/**
 * gst_mpegts_dvb_linkage_descriptor_get_event:
 * @desc: the #GstMpegtsDVBLinkageDescriptor
 *
 * Returns: The #GstMpegtsDVBLinkageEvent or %NULL if an error happened
 */
const GstMpegtsDVBLinkageEvent *
gst_mpegts_dvb_linkage_descriptor_get_event (const GstMpegtsDVBLinkageDescriptor
    * desc)
{
  g_return_val_if_fail (desc != NULL, NULL);
  g_return_val_if_fail (desc->linkage_type ==
      GST_MPEGTS_DVB_LINKAGE_EVENT, NULL);

  return (const GstMpegtsDVBLinkageEvent *) desc->linkage_data;
}

/**
 * gst_mpegts_dvb_linkage_descriptor_get_extended_event:
 * @desc: the #GstMpegtsDVBLinkageDescriptor
 *
 * Returns: (element-type GstMpegtsDVBLinkageExtendedEvent): an #GstMpegtsDVBLinkageExtendedEvent array or %NULL if an error happened
 */
const GPtrArray *
gst_mpegts_dvb_linkage_descriptor_get_extended_event (const
    GstMpegtsDVBLinkageDescriptor * desc)
{
  g_return_val_if_fail (desc != NULL, NULL);
  g_return_val_if_fail (desc->linkage_type ==
      GST_MPEGTS_DVB_LINKAGE_EXTENDED_EVENT, NULL);

  return (const GPtrArray *) desc->linkage_data;
}

/* GST_MTS_DESC_DVB_SHORT_EVENT (0x4D) */
/**
 * gst_mpegts_descriptor_parse_dvb_short_event:
 * @descriptor: a %GST_MTS_DESC_DVB_SHORT_EVENT #GstMpegtsDescriptor
 * @language_code: (out) (transfer full) (allow-none): the language code
 * @event_name: (out) (transfer full) (allow-none): the event name
 * @text: (out) (transfer full) (allow-none): the event text
 *
 * Extracts the DVB short event information from @descriptor.
 *
 * Returns: %TRUE if parsing succeeded, else %FALSE.
 */
gboolean
gst_mpegts_descriptor_parse_dvb_short_event (const GstMpegtsDescriptor *
    descriptor, gchar ** language_code, gchar ** event_name, gchar ** text)
{
  guint8 *data;

  g_return_val_if_fail (descriptor != NULL, FALSE);
  /* Need at least 5 bytes (3 for language code, 2 for each string length) */
  __common_desc_checks (descriptor, GST_MTS_DESC_DVB_SHORT_EVENT, 5, FALSE);

  data = (guint8 *) descriptor->data + 2;

  if (language_code)
    *language_code = convert_lang_code (data);

  data += 3;
  if (event_name)
    *event_name = get_encoding_and_convert ((const gchar *) data + 1, *data);
  data += *data + 1;
  if (text)
    *text = get_encoding_and_convert ((const gchar *) data + 1, *data);
  return TRUE;
}

/* GST_MTS_DESC_DVB_TELETEXT (0x56) */
/**
 * gst_mpegts_descriptor_parse_dvb_teletext_idx:
 * @descriptor: a %GST_MTS_DESC_DVB_TELETEXT #GstMpegtsDescriptor
 * @idx: The id of the teletext to get
 * @language_code: (out) (transfer full) (allow-none): a null-terminated string
 * @teletext_type: (out) (allow-none): #GstMpegtsDVBTeletextType
 * @magazine_number: (out) (allow-none):
 * @page_number: (out) (allow-none):
 *
 * Parses teletext number @idx in the @descriptor. The language is in ISO639 format.
 *
 * Returns: FALSE on out-of-bounds and errors
 */
gboolean
gst_mpegts_descriptor_parse_dvb_teletext_idx (const GstMpegtsDescriptor *
    descriptor, guint idx, gchar ** language_code,
    GstMpegtsDVBTeletextType * teletext_type, guint8 * magazine_number,
    guint8 * page_number)
{
  guint8 *data;

  g_return_val_if_fail (descriptor != NULL, FALSE);
  __common_desc_check_base (descriptor, GST_MTS_DESC_DVB_TELETEXT, FALSE);

  if (descriptor->length / 5 <= idx)
    return FALSE;

  data = (guint8 *) descriptor->data + 2 + idx * 5;

  if (language_code)
    *language_code = convert_lang_code (data);

  if (teletext_type)
    *teletext_type = data[3] >> 3;

  if (magazine_number)
    *magazine_number = data[3] & 0x07;

  if (page_number)
    *page_number = data[4];

  return TRUE;
}

/**
 * gst_mpegts_descriptor_parse_dvb_teletext_nb:
 * @descriptor: a %GST_MTS_DESC_DVB_TELETEXT #GstMpegtsDescriptor
 *
 * Find the number of teletext entries in @descriptor
 *
 * Returns: Number of teletext entries
 */
guint
gst_mpegts_descriptor_parse_dvb_teletext_nb (const GstMpegtsDescriptor *
    descriptor)
{
  g_return_val_if_fail (descriptor != NULL, 0);
  __common_desc_check_base (descriptor, GST_MTS_DESC_DVB_TELETEXT, FALSE);

  return descriptor->length / 5;
}

/* GST_MTS_DESC_DVB_SUBTITLING (0x59) */

/**
 * gst_mpegts_descriptor_parse_dvb_subtitling_idx:
 * @descriptor: a %GST_MTS_DESC_DVB_SUBTITLING #GstMpegtsDescriptor
 * @idx: Table id of the entry to parse
 * @lang: (out) (transfer full): the language code
 * @type: (out) (transfer none) (allow-none): the type of subtitling
 * @composition_page_id: (out) (transfer none) (allow-none): the composition page id
 * @ancillary_page_id: (out) (transfer none) (allow-none): the ancillary page id
 *
 * Extracts the DVB subtitling informatio from specific table id in @descriptor.
 *
 * Note: Use #gst_tag_get_language_code if you want to get the the
 * ISO 639-1 language code from the returned ISO 639-2 one.
 *
 * Returns: %TRUE if parsing succeeded, else %FALSE.
 */
gboolean
gst_mpegts_descriptor_parse_dvb_subtitling_idx (const GstMpegtsDescriptor *
    descriptor, guint idx, gchar ** lang, guint8 * type,
    guint16 * composition_page_id, guint16 * ancillary_page_id)
{
  guint8 *data;

  g_return_val_if_fail (descriptor != NULL && lang != NULL, FALSE);
  __common_desc_check_base (descriptor, GST_MTS_DESC_DVB_SUBTITLING, FALSE);

  /* If we went too far, return FALSE */
  if (descriptor->length / 8 <= idx)
    return FALSE;

  data = (guint8 *) descriptor->data + 2 + idx * 8;

  *lang = convert_lang_code (data);

  data += 3;

  if (type)
    *type = *data;
  data += 1;
  if (composition_page_id)
    *composition_page_id = GST_READ_UINT16_BE (data);
  data += 2;
  if (ancillary_page_id)
    *ancillary_page_id = GST_READ_UINT16_BE (data);

  return TRUE;
}

/**
 * gst_mpegts_descriptor_parse_dvb_subtitling_nb:
 * @descriptor: a %GST_MTS_DESC_DVB_SUBTITLING #GstMpegtsDescriptor
 *
 * Returns: The number of entries in @descriptor
 */
guint
gst_mpegts_descriptor_parse_dvb_subtitling_nb (const GstMpegtsDescriptor *
    descriptor)
{
  g_return_val_if_fail (descriptor != NULL, FALSE);
  __common_desc_check_base (descriptor, GST_MTS_DESC_DVB_SUBTITLING, FALSE);

  return descriptor->length / 8;
}

/**
 * gst_mpegts_descriptor_from_dvb_subtitling:
 * @lang: (transfer none): a string containing the ISO639 language
 * @type: subtitling type
 * @composition: composition page id
 * @ancillary: ancillary page id
 */
GstMpegtsDescriptor *
gst_mpegts_descriptor_from_dvb_subtitling (const gchar * lang,
    guint8 type, guint16 composition, guint16 ancillary)
{
  GstMpegtsDescriptor *descriptor;
  guint8 *data;

  g_return_val_if_fail (lang != NULL, NULL);

  descriptor = _new_descriptor (GST_MTS_DESC_DVB_SUBTITLING, 8);

  data = descriptor->data + 2;

  memcpy (data, lang, 3);
  data += 3;

  *data++ = type;

  GST_WRITE_UINT16_BE (data, composition);
  data += 2;

  GST_WRITE_UINT16_BE (data, ancillary);

  return descriptor;
}

/* GST_MTS_DESC_DVB_EXTENDED_EVENT (0x4E) */
static GstMpegtsExtendedEventDescriptor *
_gst_mpegts_extended_event_descriptor_copy (GstMpegtsExtendedEventDescriptor *
    source)
{
  GstMpegtsExtendedEventDescriptor *copy;

  copy = g_memdup2 (source, sizeof (GstMpegtsExtendedEventDescriptor));
  copy->items = g_ptr_array_ref (source->items);
  copy->text = g_strdup (source->text);

  return copy;
}

void
gst_mpegts_extended_event_descriptor_free (GstMpegtsExtendedEventDescriptor *
    source)
{
  g_free (source->text);
  g_free (source->language_code);
  g_ptr_array_unref (source->items);
  g_free (source);
}

G_DEFINE_BOXED_TYPE (GstMpegtsExtendedEventDescriptor,
    gst_mpegts_extended_event_descriptor,
    (GBoxedCopyFunc) _gst_mpegts_extended_event_descriptor_copy,
    (GFreeFunc) gst_mpegts_extended_event_descriptor_free);

static GstMpegtsExtendedEventItem *
_gst_mpegts_extended_event_item_copy (GstMpegtsExtendedEventItem * source)
{
  GstMpegtsExtendedEventItem *copy =
      g_memdup2 (source, sizeof (GstMpegtsExtendedEventItem));
  copy->item_description = g_strdup (source->item_description);
  copy->item = g_strdup (source->item);
  return copy;
}

static void
_gst_mpegts_extended_event_item_free (GstMpegtsExtendedEventItem * item)
{
  g_free (item->item);
  g_free (item->item_description);
  g_free (item);
}

G_DEFINE_BOXED_TYPE (GstMpegtsExtendedEventItem,
    gst_mpegts_extended_event_item,
    (GBoxedCopyFunc) _gst_mpegts_extended_event_item_copy,
    (GFreeFunc) _gst_mpegts_extended_event_item_free);

/**
 * gst_mpegts_descriptor_parse_dvb_extended_event:
 * @descriptor: a %GST_MTS_DESC_DVB_EXTENDED_EVENT #GstMpegtsDescriptor
 * @res: (out) (transfer full): the #GstMpegtsExtendedEventDescriptor to fill
 *
 * Extracts the DVB extended event information from @descriptor.
 *
 * Returns: %TRUE if parsing succeeded, else %FALSE.
 */
gboolean
gst_mpegts_descriptor_parse_dvb_extended_event (const GstMpegtsDescriptor
    * descriptor, GstMpegtsExtendedEventDescriptor ** desc)
{
  guint8 *data, *pdata;
  guint8 tmp, len_item;
  GstMpegtsExtendedEventItem *item;
  GstMpegtsExtendedEventDescriptor *res;

  g_return_val_if_fail (descriptor != NULL && desc != NULL, FALSE);
  /* Need at least 6 bytes (1 for desc number, 3 for language code, 2 for the loop length) */
  __common_desc_checks (descriptor, GST_MTS_DESC_DVB_EXTENDED_EVENT, 6, FALSE);

  res = g_new0 (GstMpegtsExtendedEventDescriptor, 1);

  data = (guint8 *) descriptor->data + 2;

  tmp = *data;
  res->descriptor_number = tmp >> 4;
  res->last_descriptor_number = tmp & 0x0f;

  data += 1;

  res->language_code = convert_lang_code (data);
  data += 3;

  len_item = *data;
  if (len_item > descriptor->length - 5) {
    gst_mpegts_extended_event_descriptor_free (res);
    return FALSE;
  }

  data += 1;

  res->items = g_ptr_array_new_with_free_func ((GDestroyNotify)
      _gst_mpegts_extended_event_item_free);

  pdata = data + len_item;
  while (data < pdata) {
    item = g_new0 (GstMpegtsExtendedEventItem, 1);
    item->item_description =
        get_encoding_and_convert ((const gchar *) data + 1, *data);

    data += *data + 1;

    item->item = get_encoding_and_convert ((const gchar *) data + 1, *data);

    data += *data + 1;

    g_ptr_array_add (res->items, item);
  }
  if (pdata != data) {
    gst_mpegts_extended_event_descriptor_free (res);
    return FALSE;
  }
  res->text = get_encoding_and_convert ((const gchar *) data + 1, *data);

  *desc = res;

  return TRUE;
}

/* GST_MTS_DESC_DVB_COMPONENT (0x50) */
static GstMpegtsComponentDescriptor *
_gst_mpegts_dvb_component_descriptor_copy (GstMpegtsComponentDescriptor *
    source)
{
  GstMpegtsComponentDescriptor *copy;

  copy = g_memdup2 (source, sizeof (GstMpegtsComponentDescriptor));
  copy->language_code = g_strdup (source->language_code);
  copy->text = g_strdup (source->text);

  return copy;
}

void
gst_mpegts_dvb_component_descriptor_free (GstMpegtsComponentDescriptor * source)
{
  g_free (source->language_code);
  g_free (source->text);
  g_free (source);
}

G_DEFINE_BOXED_TYPE (GstMpegtsComponentDescriptor,
    gst_mpegts_component_descriptor,
    (GBoxedCopyFunc) _gst_mpegts_dvb_component_descriptor_copy,
    (GFreeFunc) gst_mpegts_dvb_component_descriptor_free);

/**
 * gst_mpegts_descriptor_parse_dvb_component:
 * @descriptor: a %GST_MTS_DESC_DVB_COMPONENT #GstMpegtsDescriptor
 * @res: (out) (transfer full): the #GstMpegtsComponentDescriptor to fill
 *
 * Extracts the DVB component information from @descriptor.
 *
 * Returns: %TRUE if parsing succeeded, else %FALSE.
 */
gboolean
gst_mpegts_descriptor_parse_dvb_component (const GstMpegtsDescriptor
    * descriptor, GstMpegtsComponentDescriptor ** res)
{
  guint8 *data;
  guint8 len;
  GstMpegtsComponentDescriptor *desc;

  g_return_val_if_fail (descriptor != NULL && res != NULL, FALSE);
  /* Need 6 bytes at least (1 for content, 1 for type, 1 for tag, 3 for language code) */
  __common_desc_checks (descriptor, GST_MTS_DESC_DVB_COMPONENT, 6, FALSE);

  data = (guint8 *) descriptor->data + 2;

  desc = g_new0 (GstMpegtsComponentDescriptor, 1);

  desc->stream_content = *data & 0x0f;
  data += 1;

  desc->component_type = *data;
  data += 1;

  desc->component_tag = *data;
  data += 1;

  desc->language_code = convert_lang_code (data);
  data += 3;

  len = descriptor->length - 6;
  if (len)
    desc->text = get_encoding_and_convert ((const gchar *) data, len);

  *res = desc;

  return TRUE;
}

/* GST_MTS_DESC_DVB_STREAM_IDENTIFIER (0x52) */
/**
 * gst_mpegts_descriptor_parse_dvb_stream_identifier:
 * @descriptor: a %GST_MTS_DESC_DVB_CONTENT #GstMpegtsDescriptor
 * @component_tag: (out) (transfer none): the component tag
 *
 * Extracts the component tag from @descriptor.
 *
 * Returns: %TRUE if the parsing happened correctly, else %FALSE.
 */
gboolean
gst_mpegts_descriptor_parse_dvb_stream_identifier (const GstMpegtsDescriptor
    * descriptor, guint8 * component_tag)
{
  guint8 *data;

  g_return_val_if_fail (descriptor != NULL && component_tag != NULL, FALSE);
  __common_desc_checks_exact (descriptor, GST_MTS_DESC_DVB_STREAM_IDENTIFIER,
      1, FALSE);

  data = (guint8 *) descriptor->data + 2;

  *component_tag = *data;

  return TRUE;
}

/* GST_MTS_DESC_DVB_CA_IDENTIFIER (0x53) */
/**
 * gst_mpegts_descriptor_parse_dvb_ca_identifier:
 * @descriptor: a %GST_MTS_DESC_DVB_CA_IDENTIFIER #GstMpegtsDescriptor
 * @list: (out) (transfer full) (element-type guint16): a list of ca identifier.
 * Edge entry identifies the CA system. Allocations of the value of this field
 * are found in http://www.dvbservices.com
 *
 * Extracts ca id's from @descriptor.
 *
 * Returns: %TRUE if the parsing happened correctly, else %FALSE.
 */
gboolean
gst_mpegts_descriptor_parse_dvb_ca_identifier (const GstMpegtsDescriptor *
    descriptor, GArray ** list)
{
  guint8 *data;
  guint16 tmp;
  guint i;

  g_return_val_if_fail (descriptor != NULL && list != NULL, FALSE);
  /* 2 bytes = one entry */
  __common_desc_checks (descriptor, GST_MTS_DESC_DVB_CA_IDENTIFIER, 2, FALSE);

  data = (guint8 *) descriptor->data + 2;

  *list = g_array_new (FALSE, FALSE, sizeof (guint16));

  for (i = 0; i < descriptor->length - 1; i += 2) {
    tmp = GST_READ_UINT16_BE (data);
    g_array_append_val (*list, tmp);
    data += 2;
  }

  return TRUE;
}

/* GST_MTS_DESC_DVB_CONTENT (0x54) */

DEFINE_STATIC_COPY_FUNCTION (GstMpegtsContent, gst_mpegts_content);

DEFINE_STATIC_FREE_FUNCTION (GstMpegtsContent, gst_mpegts_content);

G_DEFINE_BOXED_TYPE (GstMpegtsContent,
    gst_mpegts_content,
    (GBoxedCopyFunc) _gst_mpegts_content_copy,
    (GFreeFunc) _gst_mpegts_content_free);

/**
 * gst_mpegts_descriptor_parse_dvb_content:
 * @descriptor: a %GST_MTS_DESC_DVB_CONTENT #GstMpegtsDescriptor
 * @content: (out) (transfer full) (element-type GstMpegtsContent): #GstMpegtsContent
 *
 * Extracts the DVB content information from @descriptor.
 *
 * Returns: %TRUE if the parsing happened correctly, else %FALSE.
 */
gboolean
gst_mpegts_descriptor_parse_dvb_content (const GstMpegtsDescriptor
    * descriptor, GPtrArray ** content)
{
  guint8 *data;
  guint8 len, tmp;
  guint8 i;

  g_return_val_if_fail (descriptor != NULL && content != NULL, FALSE);
  __common_desc_check_base (descriptor, GST_MTS_DESC_DVB_CONTENT, FALSE);

  data = (guint8 *) descriptor->data + 2;
  len = descriptor->length;

  *content = g_ptr_array_new_with_free_func ((GDestroyNotify)
      _gst_mpegts_content_free);
  for (i = 0; i < len;) {
    GstMpegtsContent *cont = g_new0 (GstMpegtsContent, 1);
    tmp = *data;
    cont->content_nibble_1 = (tmp & 0xf0) >> 4;
    cont->content_nibble_2 = tmp & 0x0f;
    data += 1;
    cont->user_byte = *data;
    data += 1;
    i += 2;
    g_ptr_array_add (*content, cont);
  }

  return TRUE;
}

/* GST_MTS_DESC_DVB_PARENTAL_RATING (0x55) */

static GstMpegtsDVBParentalRatingItem *
_gst_mpegts_dvb_parental_rating_item_copy (GstMpegtsDVBParentalRatingItem *
    source)
{
  GstMpegtsDVBParentalRatingItem *copy =
      g_memdup2 (source, sizeof (GstMpegtsDVBParentalRatingItem));
  copy->country_code = g_strdup (source->country_code);
  return copy;
}

static void
_gst_mpegts_dvb_parental_rating_item_free (GstMpegtsDVBParentalRatingItem *
    item)
{
  g_free (item->country_code);
  g_free (item);
}

G_DEFINE_BOXED_TYPE (GstMpegtsDVBParentalRatingItem,
    gst_mpegts_dvb_parental_rating_item,
    (GBoxedCopyFunc) _gst_mpegts_dvb_parental_rating_item_copy,
    (GFreeFunc) _gst_mpegts_dvb_parental_rating_item_free);

/**
 * gst_mpegts_descriptor_parse_dvb_parental_rating:
 * @descriptor: a %GST_MTS_DESC_DVB_PARENTAL_RATING #GstMpegtsDescriptor
 * @rating: (out) (transfer full) (element-type GstMpegtsDVBParentalRatingItem):
 * #GstMpegtsDVBParentalRatingItem
 *
 * Extracts the DVB parental rating information from @descriptor.
 *
 * Returns: %TRUE if the parsing happened correctly, else %FALSE.
 */
gboolean
gst_mpegts_descriptor_parse_dvb_parental_rating (const GstMpegtsDescriptor
    * descriptor, GPtrArray ** rating)
{
  guint8 *data;
  guint i;

  g_return_val_if_fail (descriptor != NULL && rating != NULL, FALSE);
  __common_desc_check_base (descriptor, GST_MTS_DESC_DVB_PARENTAL_RATING,
      FALSE);

  data = (guint8 *) descriptor->data + 2;

  *rating = g_ptr_array_new_with_free_func ((GDestroyNotify)
      _gst_mpegts_dvb_parental_rating_item_free);

  for (i = 0; i < descriptor->length - 3; i += 4) {
    GstMpegtsDVBParentalRatingItem *item =
        g_new0 (GstMpegtsDVBParentalRatingItem, 1);
    g_ptr_array_add (*rating, item);

    item->country_code = convert_lang_code (data);
    data += 3;

    if (g_strcmp0 (item->country_code, "BRA") == 0) {
      /* brasil */
      switch (*data & 0xf) {
        case 1:
          item->rating = 6;
          break;
        case 2:
          item->rating = 10;
          break;
        case 3:
          item->rating = 12;
          break;
        case 4:
          item->rating = 14;
          break;
        case 5:
          item->rating = 16;
          break;
        case 6:
          item->rating = 18;
          break;
        default:
          item->rating = 0;
          break;
      }
    } else {
      item->rating = (*data & 0xf) + 3;
    }

    data += 1;
  }

  return TRUE;
}

/* GST_MTS_DESC_DVB_TERRESTRIAL_DELIVERY_SYSTEM (0x5A) */

DEFINE_STATIC_COPY_FUNCTION (GstMpegtsTerrestrialDeliverySystemDescriptor,
    gst_mpegts_terrestrial_delivery_system_descriptor);

DEFINE_STATIC_FREE_FUNCTION (GstMpegtsTerrestrialDeliverySystemDescriptor,
    gst_mpegts_terrestrial_delivery_system_descriptor);

G_DEFINE_BOXED_TYPE (GstMpegtsTerrestrialDeliverySystemDescriptor,
    gst_mpegts_terrestrial_delivery_system_descriptor,
    (GBoxedCopyFunc) _gst_mpegts_terrestrial_delivery_system_descriptor_copy,
    (GFreeFunc) _gst_mpegts_terrestrial_delivery_system_descriptor_free);


/**
 * gst_mpegts_descriptor_parse_terrestrial_delivery_system:
 * @descriptor: a %GST_MTS_DESC_DVB_TERRESTRIAL_DELIVERY_SYSTEM #GstMpegtsDescriptor
 * @res: (out) (transfer none): #GstMpegtsTerrestrialDeliverySystemDescriptor
 *
 * Parses out the terrestrial delivery system from the @descriptor.
 *
 * Returns: %TRUE if the parsing happened correctly, else %FALSE.
 */
gboolean
gst_mpegts_descriptor_parse_terrestrial_delivery_system (const
    GstMpegtsDescriptor * descriptor,
    GstMpegtsTerrestrialDeliverySystemDescriptor * res)
{
  guint8 *data;
  guint8 tmp;

  g_return_val_if_fail (descriptor != NULL && res != NULL, FALSE);
  /* Descriptor is always 11 bytes long */
  __common_desc_checks_exact (descriptor,
      GST_MTS_DESC_DVB_TERRESTRIAL_DELIVERY_SYSTEM, 11, FALSE);

  data = (guint8 *) descriptor->data + 2;

  res->frequency = 0;
  res->frequency = GST_READ_UINT32_BE (data);
  res->frequency *= 10;

  data += 4;

  tmp = *data;
  switch ((tmp >> 5) & 0x07) {
    case 0:
      res->bandwidth = 8000000;
      break;
    case 1:
      res->bandwidth = 7000000;
      break;
    case 2:
      res->bandwidth = 6000000;
      break;
    case 3:
      res->bandwidth = 5000000;
      break;
    default:
      res->bandwidth = 0;
      break;
  }

  res->priority = (tmp >> 4) & 0x01;
  res->time_slicing = (tmp >> 3) & 0x01;
  res->mpe_fec = (tmp >> 2) & 0x01;
  data += 1;

  tmp = *data;
  switch ((tmp >> 6) & 0x03) {
    case 0:
      res->constellation = GST_MPEGTS_MODULATION_QPSK;
      break;
    case 1:
      res->constellation = GST_MPEGTS_MODULATION_QAM_16;
      break;
    case 2:
      res->constellation = GST_MPEGTS_MODULATION_QAM_64;
      break;
    default:
      break;
  }

  switch ((tmp >> 3) & 0x07) {
    case 0:
      res->hierarchy = GST_MPEGTS_HIERARCHY_NONE;
      break;
    case 1:
      res->hierarchy = GST_MPEGTS_HIERARCHY_1;
      break;
    case 2:
      res->hierarchy = GST_MPEGTS_HIERARCHY_2;
      break;
    case 3:
      res->hierarchy = GST_MPEGTS_HIERARCHY_4;
      break;
    case 4:
      res->hierarchy = GST_MPEGTS_HIERARCHY_NONE;
      break;
    case 5:
      res->hierarchy = GST_MPEGTS_HIERARCHY_1;
      break;
    case 6:
      res->hierarchy = GST_MPEGTS_HIERARCHY_2;
      break;
    case 7:
      res->hierarchy = GST_MPEGTS_HIERARCHY_4;
      break;
    default:
      break;
  }

  switch (tmp & 0x07) {
    case 0:
      res->code_rate_hp = GST_MPEGTS_FEC_1_2;
      break;
    case 1:
      res->code_rate_hp = GST_MPEGTS_FEC_2_3;
      break;
    case 2:
      res->code_rate_hp = GST_MPEGTS_FEC_3_4;
      break;
    case 3:
      res->code_rate_hp = GST_MPEGTS_FEC_5_6;
      break;
    case 4:
      res->code_rate_hp = GST_MPEGTS_FEC_7_8;
      break;
    default:
      break;
  }
  data += 1;

  tmp = *data;
  switch ((tmp >> 5) & 0x07) {
    case 0:
      res->code_rate_lp = GST_MPEGTS_FEC_1_2;
      break;
    case 1:
      res->code_rate_lp = GST_MPEGTS_FEC_2_3;
      break;
    case 2:
      res->code_rate_lp = GST_MPEGTS_FEC_3_4;
      break;
    case 3:
      res->code_rate_lp = GST_MPEGTS_FEC_5_6;
      break;
    case 4:
      res->code_rate_lp = GST_MPEGTS_FEC_7_8;
      break;
    default:
      break;
  }

  switch ((tmp >> 3) & 0x03) {
    case 0:
      res->guard_interval = GST_MPEGTS_GUARD_INTERVAL_1_32;
      break;
    case 1:
      res->guard_interval = GST_MPEGTS_GUARD_INTERVAL_1_16;
      break;
    case 2:
      res->guard_interval = GST_MPEGTS_GUARD_INTERVAL_1_8;
      break;
    case 3:
      res->guard_interval = GST_MPEGTS_GUARD_INTERVAL_1_4;
      break;
    default:
      break;
  }

  switch ((tmp >> 1) & 0x03) {
    case 0:
      res->transmission_mode = GST_MPEGTS_TRANSMISSION_MODE_2K;
      break;
    case 1:
      res->transmission_mode = GST_MPEGTS_TRANSMISSION_MODE_8K;
      break;
    case 2:
      res->transmission_mode = GST_MPEGTS_TRANSMISSION_MODE_4K;
      break;
    default:
      break;
  }
  res->other_frequency = tmp & 0x01;

  return TRUE;
}

/* GST_MTS_DESC_DVB_MULTILINGUAL_NETWORK_NAME (0x5B) */

static GstMpegtsDvbMultilingualNetworkNameItem
    * _gst_mpegts_dvb_multilingual_network_name_item_copy
    (GstMpegtsDvbMultilingualNetworkNameItem * source)
{
  GstMpegtsDvbMultilingualNetworkNameItem *copy =
      g_memdup2 (source, sizeof (GstMpegtsDvbMultilingualNetworkNameItem));
  copy->language_code = g_strdup (source->language_code);
  copy->network_name = g_strdup (source->network_name);
  return copy;
}

static void
    _gst_mpegts_dvb_multilingual_network_name_item_free
    (GstMpegtsDvbMultilingualNetworkNameItem * item)
{
  g_free (item->network_name);
  g_free (item->language_code);
  g_free (item);
}

G_DEFINE_BOXED_TYPE (GstMpegtsDvbMultilingualNetworkNameItem,
    gst_mpegts_dvb_multilingual_network_name_item,
    (GBoxedCopyFunc) _gst_mpegts_dvb_multilingual_network_name_item_copy,
    (GFreeFunc) _gst_mpegts_dvb_multilingual_network_name_item_free);

/**
 * gst_mpegts_descriptor_parse_dvb_multilingual_network_name:
 * @descriptor: a %GST_MTS_DESC_DVB_MULTILINGUAL_NETWORK_NAME
 * #GstMpegtsDescriptor
 * @network_name_items: (out) (transfer full) (element-type GstMpegtsDvbMultilingualNetworkNameItem):
 * a #GstMpegtsDvbMultilingualNetworkNameItem
 *
 * Parses out the multilingual network name from the @descriptor.
 *
 * Returns: %TRUE if the parsing happened correctly, else %FALSE.
 */
gboolean
gst_mpegts_descriptor_parse_dvb_multilingual_network_name (const
    GstMpegtsDescriptor * descriptor, GPtrArray ** network_name_items)
{
  guint8 *data, i, len;
  GstMpegtsDvbMultilingualNetworkNameItem *item;

  g_return_val_if_fail (descriptor != NULL && network_name_items != NULL,
      FALSE);
  __common_desc_checks (descriptor, GST_MTS_DESC_DVB_MULTILINGUAL_NETWORK_NAME,
      5, FALSE);

  data = (guint8 *) descriptor->data + 2;

  *network_name_items = g_ptr_array_new_with_free_func ((GDestroyNotify)
      _gst_mpegts_dvb_multilingual_network_name_item_free);

  for (i = 0; i < descriptor->length - 3;) {
    item = g_new0 (GstMpegtsDvbMultilingualNetworkNameItem, 1);
    g_ptr_array_add (*network_name_items, item);
    item->language_code = convert_lang_code (data);
    data += 3;
    i += 3;

    len = *data;
    item->network_name =
        get_encoding_and_convert ((const gchar *) data + 1, len);
    data += len + 1;
    i += len + 1;
  }

  return TRUE;
}

/* GST_MTS_DESC_DVB_MULTILINGUAL_BOUQUET_NAME (0x5C) */

static GstMpegtsDvbMultilingualBouquetNameItem
    * _gst_mpegts_dvb_multilingual_bouquet_name_item_copy
    (GstMpegtsDvbMultilingualBouquetNameItem * source)
{
  GstMpegtsDvbMultilingualBouquetNameItem *copy =
      g_memdup2 (source, sizeof (GstMpegtsDvbMultilingualBouquetNameItem));
  copy->bouquet_name = g_strdup (source->bouquet_name);
  copy->language_code = g_strdup (source->language_code);
  return copy;
}

static void
    _gst_mpegts_dvb_multilingual_bouquet_name_item_free
    (GstMpegtsDvbMultilingualBouquetNameItem * item)
{
  g_free (item->language_code);
  g_free (item->bouquet_name);
  g_free (item);
}

G_DEFINE_BOXED_TYPE (GstMpegtsDvbMultilingualBouquetNameItem,
    gst_mpegts_dvb_multilingual_bouquet_name_item,
    (GBoxedCopyFunc) _gst_mpegts_dvb_multilingual_bouquet_name_item_copy,
    (GFreeFunc) _gst_mpegts_dvb_multilingual_bouquet_name_item_free);

/**
 * gst_mpegts_descriptor_parse_dvb_multilingual_bouquet_name:
 * @descriptor: a %GST_MTS_DESC_DVB_MULTILINGUAL_BOUQUET_NAME
 * #GstMpegtsDescriptor
 * @bouquet_name_items: (out) (transfer full) (element-type GstMpegtsDvbMultilingualBouquetNameItem):
 * a #GstMpegtsDvbMultilingualBouquetNameItem
 *
 * Parses out the multilingual bouquet name from the @descriptor.
 *
 * Returns: %TRUE if the parsing happened correctly, else %FALSE.
 */
gboolean
gst_mpegts_descriptor_parse_dvb_multilingual_bouquet_name (const
    GstMpegtsDescriptor * descriptor, GPtrArray ** bouquet_name_items)
{
  guint8 *data, i, len;
  GstMpegtsDvbMultilingualBouquetNameItem *item;

  g_return_val_if_fail (descriptor != NULL && bouquet_name_items != NULL,
      FALSE);
  __common_desc_checks (descriptor, GST_MTS_DESC_DVB_MULTILINGUAL_BOUQUET_NAME,
      5, FALSE);

  data = (guint8 *) descriptor->data + 2;

  *bouquet_name_items = g_ptr_array_new_with_free_func ((GDestroyNotify)
      _gst_mpegts_dvb_multilingual_bouquet_name_item_free);

  for (i = 0; i < descriptor->length - 3;) {
    item = g_new0 (GstMpegtsDvbMultilingualBouquetNameItem, 1);
    g_ptr_array_add (*bouquet_name_items, item);
    item->language_code = convert_lang_code (data);
    data += 3;
    i += 3;

    len = *data;
    item->bouquet_name =
        get_encoding_and_convert ((const gchar *) data + 1, len);
    data += len + 1;
    i += len + 1;
  }

  return TRUE;
}

/* GST_MTS_DESC_DVB_MULTILINGUAL_SERVICE_NAME (0x5D) */

static GstMpegtsDvbMultilingualServiceNameItem
    * _gst_mpegts_dvb_multilingual_service_name_item_copy
    (GstMpegtsDvbMultilingualServiceNameItem * source)
{
  GstMpegtsDvbMultilingualServiceNameItem *copy =
      g_memdup2 (source, sizeof (GstMpegtsDvbMultilingualServiceNameItem));
  copy->language_code = g_strdup (source->language_code);
  copy->service_name = g_strdup (source->service_name);
  copy->provider_name = g_strdup (source->provider_name);
  return copy;
}

static void
    _gst_mpegts_dvb_multilingual_service_name_item_free
    (GstMpegtsDvbMultilingualServiceNameItem * item)
{
  g_free (item->provider_name);
  g_free (item->service_name);
  g_free (item->language_code);
  g_free (item);
}

G_DEFINE_BOXED_TYPE (GstMpegtsDvbMultilingualServiceNameItem,
    gst_mpegts_dvb_multilingual_service_name_item,
    (GBoxedCopyFunc) _gst_mpegts_dvb_multilingual_service_name_item_copy,
    (GFreeFunc) _gst_mpegts_dvb_multilingual_service_name_item_free);

/**
 * gst_mpegts_descriptor_parse_dvb_multilingual_service_name:
 * @descriptor: a %GST_MTS_DESC_DVB_MULTILINGUAL_SERVICE_NAME
 * #GstMpegtsDescriptor
 * @service_name_items: (out) (transfer full) (element-type GstMpegtsDvbMultilingualServiceNameItem):
 * a #GstMpegtsDvbMultilingualServiceNameItem
 *
 * Parses out the multilingual service name from the @descriptor.
 *
 * Returns: %TRUE if the parsing happened correctly, else %FALSE.
 */
gboolean
gst_mpegts_descriptor_parse_dvb_multilingual_service_name (const
    GstMpegtsDescriptor * descriptor, GPtrArray ** service_name_items)
{
  guint8 *data, i, len;
  GstMpegtsDvbMultilingualServiceNameItem *item;

  g_return_val_if_fail (descriptor != NULL && service_name_items != NULL,
      FALSE);
  __common_desc_checks (descriptor, GST_MTS_DESC_DVB_MULTILINGUAL_SERVICE_NAME,
      7, FALSE);

  data = (guint8 *) descriptor->data + 2;

  *service_name_items = g_ptr_array_new_with_free_func ((GDestroyNotify)
      _gst_mpegts_dvb_multilingual_service_name_item_free);

  for (i = 0; i < descriptor->length - 3;) {
    item = g_new0 (GstMpegtsDvbMultilingualServiceNameItem, 1);
    g_ptr_array_add (*service_name_items, item);
    item->language_code = convert_lang_code (data);
    data += 3;
    i += 3;

    len = *data;
    item->provider_name =
        get_encoding_and_convert ((const gchar *) data + 1, len);
    data += len + 1;
    i += len + 1;

    len = *data;
    item->service_name =
        get_encoding_and_convert ((const gchar *) data + 1, len);
    data += len + 1;
    i += len + 1;
  }

  return TRUE;
}

/* GST_MTS_DESC_DVB_MULTILINGUAL_COMPONENT (0x5E) */

static GstMpegtsDvbMultilingualComponentItem
    * _gst_mpegts_dvb_multilingual_component_item_copy
    (GstMpegtsDvbMultilingualComponentItem * source)
{
  GstMpegtsDvbMultilingualComponentItem *copy =
      g_memdup2 (source, sizeof (GstMpegtsDvbMultilingualComponentItem));
  copy->description = g_strdup (source->description);
  copy->language_code = g_strdup (source->language_code);
  return copy;
}

static void
    _gst_mpegts_dvb_multilingual_component_item_free
    (GstMpegtsDvbMultilingualComponentItem * item)
{
  g_free (item->language_code);
  g_free (item->description);
  g_free (item);
}

G_DEFINE_BOXED_TYPE (GstMpegtsDvbMultilingualComponentItem,
    gst_mpegts_dvb_multilingual_component_item,
    (GBoxedCopyFunc) _gst_mpegts_dvb_multilingual_component_item_copy,
    (GFreeFunc) _gst_mpegts_dvb_multilingual_component_item_free);

/**
 * gst_mpegts_descriptor_parse_dvb_multilingual_component:
 * @descriptor: a %GST_MTS_DESC_DVB_MULTILINGUAL_COMPONENT
 * #GstMpegtsDescriptor
 * @component_tag: (out): the component tag
 * @component_description_items: (out) (transfer full) (element-type GstMpegtsDvbMultilingualComponentItem):
 * a #GstMpegtsDvbMultilingualComponentItem
 *
 * Parses out the multilingual component from the @descriptor.
 *
 * Returns: %TRUE if the parsing happened correctly, else %FALSE.
 */
gboolean
gst_mpegts_descriptor_parse_dvb_multilingual_component (const
    GstMpegtsDescriptor * descriptor, guint8 * component_tag,
    GPtrArray ** component_description_items)
{
  guint8 *data, i, len;
  GstMpegtsDvbMultilingualComponentItem *item;

  g_return_val_if_fail (descriptor != NULL
      && component_description_items != NULL && component_tag != NULL, FALSE);
  __common_desc_checks (descriptor, GST_MTS_DESC_DVB_MULTILINGUAL_COMPONENT, 6,
      FALSE);

  data = (guint8 *) descriptor->data + 2;

  *component_tag = *data;
  data += 1;

  *component_description_items =
      g_ptr_array_new_with_free_func ((GDestroyNotify)
      _gst_mpegts_dvb_multilingual_component_item_free);

  for (i = 0; i < descriptor->length - 3;) {
    item = g_new0 (GstMpegtsDvbMultilingualComponentItem, 1);
    g_ptr_array_add (*component_description_items, item);
    item->language_code = convert_lang_code (data);
    data += 3;
    i += 3;

    len = *data;
    item->description =
        get_encoding_and_convert ((const gchar *) data + 1, len);
    data += len + 1;
    i += len + 1;
  }

  return TRUE;
}

/* GST_MTS_DESC_DVB_PRIVATE_DATA_SPECIFIER (0x5F) */
/**
 * gst_mpegts_descriptor_parse_dvb_private_data_specifier:
 * @descriptor: a %GST_MTS_DESC_DVB_PRIVATE_DATA_SPECIFIER #GstMpegtsDescriptor
 * @private_data_specifier: (out): the private data specifier id
 * registered by http://www.dvbservices.com/
 * @private_data: (out) (transfer full) (allow-none) (array length=length): additional data or NULL
 * @length: (out) (allow-none): length of @private_data
 *
 * Parses out the private data specifier from the @descriptor.
 *
 * Returns: %TRUE if the parsing happened correctly, else %FALSE.
 */
gboolean
gst_mpegts_descriptor_parse_dvb_private_data_specifier (const
    GstMpegtsDescriptor * descriptor, guint32 * private_data_specifier,
    guint8 ** private_data, guint8 * length)
{
  guint8 *data;

  g_return_val_if_fail (descriptor != NULL
      && private_data_specifier != NULL, FALSE);
  __common_desc_checks (descriptor,
      GST_MTS_DESC_DVB_PRIVATE_DATA_SPECIFIER, 4, FALSE);

  data = (guint8 *) descriptor->data + 2;

  *private_data_specifier = GST_READ_UINT32_BE (data);

  if (length && private_data) {
    *length = descriptor->length - 4;

    *private_data = g_memdup2 (data + 4, *length);
  }
  return TRUE;
}

/* GST_MTS_DESC_DVB_FREQUENCY_LIST (0x62) */
/**
 * gst_mpegts_descriptor_parse_dvb_frequency_list:
 * @descriptor: a %GST_MTS_DESC_DVB_FREQUENCY_LIST #GstMpegtsDescriptor
 * @offset: (out): %FALSE in Hz, %TRUE in kHz
 * @list: (out) (transfer full) (element-type guint32): a list of all frequencies in Hz/kHz
 * depending on @offset
 *
 * Parses out a list of frequencies from the @descriptor.
 *
 * Returns: %TRUE if the parsing happened correctly, else %FALSE.
 */
gboolean
gst_mpegts_descriptor_parse_dvb_frequency_list (const GstMpegtsDescriptor
    * descriptor, gboolean * offset, GArray ** list)
{
  guint8 *data, type, len;
  guint32 freq;
  guint i;

  g_return_val_if_fail (descriptor != NULL && offset != NULL &&
      list != NULL, FALSE);
  /* 1 byte coding system, 4 bytes each frequency entry */
  __common_desc_checks (descriptor, GST_MTS_DESC_DVB_FREQUENCY_LIST, 5, FALSE);

  data = (guint8 *) descriptor->data + 2;

  type = *data & 0x03;
  data += 1;

  if (type == 1) {
    /* satellite */
    *offset = TRUE;
  } else {
    /* cable, terrestrial */
    *offset = FALSE;
  }

  *list = g_array_new (FALSE, FALSE, sizeof (guint32));

  len = descriptor->length - 1;

  for (i = 0; i < len - 3; i += 4) {
    switch (type) {
      case 1:
        freq = BCD_32 (data) * 10;
        break;
      case 2:
        freq = BCD_32 (data) * 100;
        break;
      case 3:
        freq = GST_READ_UINT32_BE (data) * 10;
        break;
      default:
        break;
    }

    g_array_append_val (*list, freq);
    data += 4;
  }

  return TRUE;
}

/* GST_MTS_DESC_DVB_DATA_BROADCAST (0x64) */
static GstMpegtsDataBroadcastDescriptor *
_gst_mpegts_dvb_data_broadcast_descriptor_copy (GstMpegtsDataBroadcastDescriptor
    * source)
{
  GstMpegtsDataBroadcastDescriptor *copy;

  copy = g_memdup2 (source, sizeof (GstMpegtsDataBroadcastDescriptor));

  copy->selector_bytes = g_memdup2 (source->selector_bytes, source->length);
  copy->language_code = g_strdup (source->language_code);
  copy->text = g_strdup (source->text);

  return copy;
}

void
gst_mpegts_dvb_data_broadcast_descriptor_free (GstMpegtsDataBroadcastDescriptor
    * source)
{
  g_free (source->selector_bytes);
  g_free (source->language_code);
  g_free (source->text);
  g_free (source);
}

G_DEFINE_BOXED_TYPE (GstMpegtsDataBroadcastDescriptor,
    gst_mpegts_dvb_data_broadcast_descriptor,
    (GBoxedCopyFunc) _gst_mpegts_dvb_data_broadcast_descriptor_copy,
    (GFreeFunc) gst_mpegts_dvb_data_broadcast_descriptor_free);

/**
 * gst_mpegts_descriptor_parse_dvb_data_broadcast:
 * @descriptor: a %GST_MTS_DESC_DVB_DATA_BROADCAST #GstMpegtsDescriptor
 * @res: (out) (transfer full): #GstMpegtsDataBroadcastDescriptor
 *
 * Parses out the data broadcast from the @descriptor.
 *
 * Returns: %TRUE if the parsing happened correctly, else %FALSE.
 */
gboolean
gst_mpegts_descriptor_parse_dvb_data_broadcast (const GstMpegtsDescriptor
    * descriptor, GstMpegtsDataBroadcastDescriptor ** desc)
{
  guint8 *data;
  GstMpegtsDataBroadcastDescriptor *res;

  g_return_val_if_fail (descriptor != NULL && desc != NULL, FALSE);
  __common_desc_checks (descriptor, GST_MTS_DESC_DVB_DATA_BROADCAST, 8, FALSE);

  data = (guint8 *) descriptor->data + 2;

  res = g_new0 (GstMpegtsDataBroadcastDescriptor, 1);

  res->data_broadcast_id = GST_READ_UINT16_BE (data);
  data += 2;

  res->component_tag = *data;
  data += 1;

  res->length = *data;
  data += 1;

  res->selector_bytes = g_memdup2 (data, res->length);
  data += res->length;

  res->language_code = convert_lang_code (data);
  data += 3;

  res->text = get_encoding_and_convert ((const gchar *) data + 1, *data);

  *desc = res;

  return TRUE;
}

/* GST_MTS_DESC_DVB_SCRAMBLING (0x65) */
/**
 * gst_mpegts_descriptor_parse_dvb_scrambling:
 * @descriptor: a %GST_MTS_DESC_DVB_SCRAMBLING #GstMpegtsDescriptor
 * @scrambling_mode: (out): This 8-bit field identifies the selected
 * mode of the scrambling algorithm (#GstMpegtsDVBScramblingModeType).
 * The technical details of the scrambling algorithm are available only
 * to bona-fide users upon signature of a Non Disclosure Agreement (NDA)
 * administered by the DVB Common Scrambling Algorithm Custodian.
 *
 * Parses out the scrambling mode from the @descriptor.
 *
 * Returns: %TRUE if the parsing happened correctly, else %FALSE.
 */
gboolean
gst_mpegts_descriptor_parse_dvb_scrambling (const GstMpegtsDescriptor *
    descriptor, GstMpegtsDVBScramblingModeType * scrambling_mode)
{
  guint8 *data;

  g_return_val_if_fail (descriptor != NULL && scrambling_mode != NULL, FALSE);
  __common_desc_checks_exact (descriptor, GST_MTS_DESC_DVB_SCRAMBLING, 1,
      FALSE);

  data = (guint8 *) descriptor->data + 2;

  *scrambling_mode = *data;

  return TRUE;
}

/* GST_MTS_DESC_DVB_DATA_BROADCAST_ID (0x66) */
/**
 * gst_mpegts_descriptor_parse_dvb_data_broadcast_id:
 * @descriptor: a %GST_MTS_DESC_DVB_DATA_BROADCAST_ID #GstMpegtsDescriptor
 * @data_broadcast_id: (out): the data broadcast id
 * @id_selector_bytes: (out) (transfer full) (array length=len): the selector bytes, if present
 * @len: (out): the length of @id_selector_bytes
 *
 * Parses out the data broadcast id from the @descriptor.
 *
 * Returns: %TRUE if the parsing happened correctly, else %FALSE.
 */
gboolean
gst_mpegts_descriptor_parse_dvb_data_broadcast_id (const GstMpegtsDescriptor
    * descriptor, guint16 * data_broadcast_id, guint8 ** id_selector_bytes,
    guint8 * len)
{
  guint8 *data;

  g_return_val_if_fail (descriptor != NULL && data_broadcast_id != NULL &&
      id_selector_bytes != NULL, FALSE);
  __common_desc_checks (descriptor, GST_MTS_DESC_DVB_DATA_BROADCAST_ID, 2,
      FALSE);

  data = (guint8 *) descriptor->data + 2;

  *data_broadcast_id = GST_READ_UINT16_BE (data);
  data += 2;

  *len = descriptor->length - 2;

  *id_selector_bytes = g_memdup2 (data, *len);

  return TRUE;
}

/* GST_MTS_DESC_EXT_DVB_T2_DELIVERY_SYSTEM (0x7F && 0x04) */
static GstMpegtsT2DeliverySystemDescriptor
    * _gst_mpegts_t2_delivery_system_descriptor_copy
    (GstMpegtsT2DeliverySystemDescriptor * source)
{
  GstMpegtsT2DeliverySystemDescriptor *copy;

  copy = g_memdup2 (source, sizeof (GstMpegtsT2DeliverySystemDescriptor));
  copy->cells = g_ptr_array_ref (source->cells);

  return copy;
}

void gst_mpegts_t2_delivery_system_descriptor_free
    (GstMpegtsT2DeliverySystemDescriptor * source)
{
  g_ptr_array_unref (source->cells);
  g_free (source);
}

G_DEFINE_BOXED_TYPE (GstMpegtsT2DeliverySystemDescriptor,
    gst_mpegts_t2_delivery_system_descriptor,
    (GBoxedCopyFunc) _gst_mpegts_t2_delivery_system_descriptor_copy,
    (GFreeFunc) gst_mpegts_t2_delivery_system_descriptor_free);

DEFINE_STATIC_COPY_FUNCTION (GstMpegtsT2DeliverySystemCellExtension,
    gst_mpegts_t2_delivery_system_cell_extension);

DEFINE_STATIC_FREE_FUNCTION (GstMpegtsT2DeliverySystemCellExtension,
    gst_mpegts_t2_delivery_system_cell_extension);

G_DEFINE_BOXED_TYPE (GstMpegtsT2DeliverySystemCellExtension,
    gst_mpegts_t2_delivery_system_cell_extension,
    (GBoxedCopyFunc) _gst_mpegts_t2_delivery_system_cell_extension_copy,
    (GFreeFunc) _gst_mpegts_t2_delivery_system_cell_extension_free);

static GstMpegtsT2DeliverySystemCell *
_gst_mpegts_t2_delivery_system_cell_copy (GstMpegtsT2DeliverySystemCell
    * source)
{
  GstMpegtsT2DeliverySystemCell *copy =
      g_memdup2 (source, sizeof (GstMpegtsT2DeliverySystemCell));
  copy->centre_frequencies = g_array_ref (source->centre_frequencies);
  copy->sub_cells = g_ptr_array_ref (source->sub_cells);
  return copy;
}

static void
_gst_mpegts_t2_delivery_system_cell_free (GstMpegtsT2DeliverySystemCell * cell)
{
  g_ptr_array_unref (cell->sub_cells);
  g_array_unref (cell->centre_frequencies);
  g_free (cell);
}

G_DEFINE_BOXED_TYPE (GstMpegtsT2DeliverySystemCell,
    gst_mpegts_t2_delivery_system_cell,
    (GBoxedCopyFunc) _gst_mpegts_t2_delivery_system_cell_copy,
    (GFreeFunc) _gst_mpegts_t2_delivery_system_cell_free);

/**
 * gst_mpegts_descriptor_parse_dvb_t2_delivery_system:
 * @descriptor: a %GST_MTS_DESC_EXT_DVB_T2_DELIVERY_SYSTEM #GstMpegtsDescriptor
 * @res: (out) (transfer full): #GstMpegtsT2DeliverySystemDescriptor
 *
 * Parses out the DVB-T2 delivery system from the @descriptor.
 *
 * Returns: %TRUE if the parsing happened correctly, else %FALSE.
 */
gboolean
gst_mpegts_descriptor_parse_dvb_t2_delivery_system (const GstMpegtsDescriptor
    * descriptor, GstMpegtsT2DeliverySystemDescriptor ** desc)
{
  guint8 *data;
  guint8 len, freq_len, sub_cell_len;
  guint32 tmp_freq;
  guint8 i;
  GstMpegtsT2DeliverySystemDescriptor *res;

  g_return_val_if_fail (descriptor != NULL && desc != NULL, FALSE);
  __common_desc_ext_checks (descriptor, GST_MTS_DESC_EXT_DVB_T2_DELIVERY_SYSTEM,
      4, FALSE);

  data = (guint8 *) descriptor->data + 3;

  res = g_new0 (GstMpegtsT2DeliverySystemDescriptor, 1);

  res->plp_id = *data;
  data += 1;

  res->t2_system_id = GST_READ_UINT16_BE (data);
  data += 2;

  if (descriptor->length > 4) {
    // FIXME: siso / miso
    res->siso_miso = (*data >> 6) & 0x03;
    switch ((*data >> 2) & 0x0f) {
      case 0:
        res->bandwidth = 8000000;
        break;
      case 1:
        res->bandwidth = 7000000;
        break;
      case 2:
        res->bandwidth = 6000000;
        break;
      case 3:
        res->bandwidth = 5000000;
        break;
      case 4:
        res->bandwidth = 10000000;
        break;
      case 5:
        res->bandwidth = 1712000;
        break;
      default:
        res->bandwidth = 0;
        break;
    }
    data += 1;

    switch ((*data >> 5) & 0x07) {
      case 0:
        res->guard_interval = GST_MPEGTS_GUARD_INTERVAL_1_32;
        break;
      case 1:
        res->guard_interval = GST_MPEGTS_GUARD_INTERVAL_1_16;
        break;
      case 2:
        res->guard_interval = GST_MPEGTS_GUARD_INTERVAL_1_8;
        break;
      case 3:
        res->guard_interval = GST_MPEGTS_GUARD_INTERVAL_1_4;
        break;
      case 4:
        res->guard_interval = GST_MPEGTS_GUARD_INTERVAL_1_128;
        break;
      case 5:
        res->guard_interval = GST_MPEGTS_GUARD_INTERVAL_19_128;
        break;
      case 6:
        res->guard_interval = GST_MPEGTS_GUARD_INTERVAL_19_256;
        break;
      default:
        break;
    }

    switch ((*data >> 2) & 0x07) {
      case 0:
        res->transmission_mode = GST_MPEGTS_TRANSMISSION_MODE_2K;
        break;
      case 1:
        res->transmission_mode = GST_MPEGTS_TRANSMISSION_MODE_8K;
        break;
      case 2:
        res->transmission_mode = GST_MPEGTS_TRANSMISSION_MODE_4K;
        break;
      case 3:
        res->transmission_mode = GST_MPEGTS_TRANSMISSION_MODE_1K;
        break;
      case 4:
        res->transmission_mode = GST_MPEGTS_TRANSMISSION_MODE_16K;
        break;
      case 5:
        res->transmission_mode = GST_MPEGTS_TRANSMISSION_MODE_32K;
        break;
      default:
        break;
    }
    res->other_frequency = (*data >> 1) & 0x01;
    res->tfs = (*data) & 0x01;
    data += 1;

    len = descriptor->length - 6;

    res->cells = g_ptr_array_new_with_free_func ((GDestroyNotify)
        _gst_mpegts_t2_delivery_system_cell_free);

    for (i = 0; i < len;) {
      GstMpegtsT2DeliverySystemCell *cell;
      guint8 j, k;

      cell = g_new0 (GstMpegtsT2DeliverySystemCell, 1);
      g_ptr_array_add (res->cells, cell);

      cell->cell_id = GST_READ_UINT16_BE (data);
      data += 2;
      i += 2;

      cell->centre_frequencies = g_array_new (FALSE, FALSE, sizeof (guint32));

      if (res->tfs == TRUE) {
        freq_len = *data;
        data += 1;
        i += 1;

        for (j = 0; j < freq_len;) {
          tmp_freq = GST_READ_UINT32_BE (data) * 10;
          g_array_append_val (cell->centre_frequencies, tmp_freq);
          data += 4;
          j += 4;
          i += 4;
        }
      } else {
        tmp_freq = GST_READ_UINT32_BE (data) * 10;
        g_array_append_val (cell->centre_frequencies, tmp_freq);
        data += 4;
        i += 4;
      }
      sub_cell_len = (*data);
      data += 1;
      i += 1;

      cell->sub_cells = g_ptr_array_new_with_free_func ((GDestroyNotify)
          _gst_mpegts_t2_delivery_system_cell_extension_free);

      for (k = 0; k < sub_cell_len;) {
        GstMpegtsT2DeliverySystemCellExtension *cell_ext;
        cell_ext = g_new0 (GstMpegtsT2DeliverySystemCellExtension, 1);

        g_ptr_array_add (cell->sub_cells, cell_ext);
        cell_ext->cell_id_extension = *data;
        data += 1;

        cell_ext->transposer_frequency = GST_READ_UINT32_BE (data) * 10;
        data += 4;
        i += 5;
        k += 5;
      }
    }
  }

  *desc = res;
  return TRUE;
}

/**
 * gst_mpegts_descriptor_parse_audio_preselection_list:
 * @descriptor: a %GST_MTS_DESC_EXT_DVB_AUDIO_PRESELECTION #GstMpegtsDescriptor
 * @list: (out) (transfer full) (element-type GstMpegtsAudioPreselectionDescriptor):
 * the list of audio preselection
 *
 * Parses out a list of audio preselection from the @descriptor.
 *
 * Returns: %TRUE if the parsing happened correctly, else %FALSE.
 *
 * Since: 1.20
 */
gboolean
gst_mpegts_descriptor_parse_audio_preselection_list (const GstMpegtsDescriptor
    * descriptor, GPtrArray ** list)
{
  guint8 *data;
  guint8 i, num_preselections, num_aux_components, future_extension_length;
  GstMpegtsAudioPreselectionDescriptor *item;

  g_return_val_if_fail (descriptor != NULL && list != NULL, FALSE);
  __common_desc_ext_check_base (descriptor,
      GST_MTS_DESC_EXT_DVB_AUDIO_PRESELECTION, FALSE);

  *list = g_ptr_array_new_with_free_func ((GDestroyNotify)
      gst_mpegts_descriptor_parse_audio_preselection_free);

  data = (guint8 *) descriptor->data + 3;
  num_preselections = (guint8) ((*data & 0xF8) >> 3);
  data += 1;

  for (i = 0; i < num_preselections; i++) {
    item = g_new0 (GstMpegtsAudioPreselectionDescriptor, 1);
    g_ptr_array_add (*list, item);

    item->preselection_id = (*data & 0xF8) >> 3;
    item->audio_rendering_indication = *data & 0x7;
    data += 1;

    item->audio_description = (*data & 0x80) >> 7;
    item->spoken_subtitles = (*data & 0x40) >> 6;
    item->dialogue_enhancement = (*data & 0x20) >> 5;
    item->interactivity_enabled = (*data & 0x10) >> 4;
    item->language_code_present = (*data & 0x08) >> 3;
    item->text_label_present = (*data & 0x04) >> 2;
    item->multi_stream_info_present = (*data & 0x02) >> 1;
    item->future_extension = (*data) & 0x01;
    data += 1;

    if (item->language_code_present == 1) {
      item->language_code = convert_lang_code (data);
      data += 3;
    }

    if (item->text_label_present == 1) {
      item->message_id = *data;
      data += 1;
    }

    if (item->multi_stream_info_present == 1) {
      num_aux_components = (*data & 0xE0) >> 5;
      data += 1;
      for (i = 0; i < num_aux_components; i++) {
        data += 1;
      }
    }

    if (item->future_extension == 1) {
      future_extension_length = *data & 0x1F;
      data += 1;
      for (i = 0; i < future_extension_length; i++) {
        data += 1;
      }
    }
  }

  return TRUE;
}

void gst_mpegts_descriptor_parse_audio_preselection_free
    (GstMpegtsAudioPreselectionDescriptor * source)
{
  if (source->language_code_present == 1) {
    g_free (source->language_code);
  }
  g_free (source);
}

void gst_mpegts_descriptor_parse_audio_preselection_dump
    (GstMpegtsAudioPreselectionDescriptor * source)
{
  GST_DEBUG ("[Audio Preselection Descriptor]");
  GST_DEBUG ("           preselection_id: 0x%02x", source->preselection_id);
  GST_DEBUG ("audio_rendering_indication: 0x%02x",
      source->audio_rendering_indication);
  GST_DEBUG ("         audio_description: %d", source->audio_description);
  GST_DEBUG ("          spoken_subtitles: %d", source->spoken_subtitles);
  GST_DEBUG ("      dialogue_enhancement: %d", source->dialogue_enhancement);
  GST_DEBUG ("     interactivity_enabled: %d", source->interactivity_enabled);
  GST_DEBUG ("     language_code_present: %d", source->language_code_present);
  GST_DEBUG ("        text_label_present: %d", source->text_label_present);
  GST_DEBUG (" multi_stream_info_present: %d",
      source->multi_stream_info_present);
  GST_DEBUG ("          future_extension: %d", source->future_extension);

  if (source->language_code_present == 1) {
    GST_DEBUG ("             language_code: %s", source->language_code);
  }
  if (source->text_label_present == 1) {
    GST_DEBUG ("                message_id: 0x%02x", source->message_id);
  }
  GST_DEBUG ("-------------------------------");
}
