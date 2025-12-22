/*
 * camutils.c - 
 * Copyright (C) 2007 Alessandro Decina
 * 
 * Authors:
 *   Alessandro Decina <alessandro.d@gmail.com>
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

#include <gst/gst.h>
#include <string.h>

#include "cam.h"
#include "camutils.h"

#define GST_CAT_DEFAULT cam_debug_cat

/* From the spec: 
 * length_field() {
 *               size_indicator
 *               if (size_indicator == 0)
 *                        length_value
 *               else if (size_indicator == 1) {
 *                        length_field_size
 *                        for (i=0; i<length_field_size; i++) {
 *                                length_value_byte
 *                        }
 *               }
 * }
*/

guint8
cam_calc_length_field_size (guint length)
{
  guint field_len;

  if (length < G_MAXUINT8)
    field_len = 1;
  else if (length <= G_MAXUINT16)
    field_len = 3;
  else if (length <= (1 << 24) - 1)
    field_len = 4;
  else
    field_len = 5;

  return field_len;
}

/* write a length_field */
guint8
cam_write_length_field (guint8 * buff, guint length)
{
  guint8 field_len = cam_calc_length_field_size (length);

  if (buff) {
    switch (field_len) {
      case 1:
        buff[0] = length;
        break;
      case 2:
        g_return_val_if_reached (0);
        break;
      case 3:
        buff[0] = TPDU_HEADER_SIZE_INDICATOR | (field_len - 1);
        buff[1] = length >> 8;
        buff[2] = length & 0xFF;
        break;
      case 4:
        buff[0] = TPDU_HEADER_SIZE_INDICATOR | (field_len - 1);
        buff[1] = length >> 16;
        buff[2] = (length >> 8) & 0xFF;
        buff[3] = length & 0xFF;
        break;
      case 5:
        buff[0] = TPDU_HEADER_SIZE_INDICATOR | (field_len - 1);
        buff[1] = length >> 24;
        buff[2] = (length >> 16) & 0xFF;
        buff[3] = (length >> 8) & 0xFF;
        buff[4] = length & 0xFF;
        break;
      default:
        g_return_val_if_reached (0);
    }
  }

  return field_len;
}

/* read a length_field */
guint8
cam_read_length_field (guint8 * buff, guint * length)
{
  guint i;
  guint field_len;
  guint8 len;

  if (buff[0] <= G_MAXINT8) {
    field_len = 1;
    len = buff[0];
  } else {
    field_len = buff[0] & ~TPDU_HEADER_SIZE_INDICATOR;
    if (field_len > 4) {
      GST_ERROR ("length_field length exceeds 4 bytes: %d", field_len);
      field_len = 0;
      len = 0;
    } else {
      len = 0;
      for (i = 0; i < field_len; ++i)
        len = (len << 8) | *++buff;

      /* count the size indicator byte */
      field_len += 1;
    }
  }

  if (length)
    *length = len;

  return field_len;
}

/*
 * ca_pmt () {
 *    ca_pmt_tag                                              24        uimsbf
 *    length_field()
 *    ca_pmt_list_management                                   8        uimsbf
 *    program_number                                          16        uimsbf
 *    reserved                                                 2        bslbf
 *    version_number                                           5        uimsbf
 *    current_next_indicator                                   1        bslbf
 *    reserved                                                 4        bslbf
 *    program_info_length                                     12        uimsbf
 *    if (program_info_length != 0) {
 *      ca_pmt_cmd_id at program level                         8        uimsbf
 *      for (i=0; i<n; i++) {
 *        CA_descriptor() programme level 
 *      }
 *   }
 *   for (i=0; i<n; i++) {
 *     stream_type                                             8       uimsbf
 *     reserved                                                3       bslbf
 *     elementary_PID  elementary stream PID                  13        uimsbf
 *     reserved                                                4       bslbf
 *     ES_info_length                                         12        uimsbf
 *     if (ES_info_length != 0) {
 *       ca_pmt_cmd_id at ES level                             8       uimsbf
 *       for (i=0; i<n; i++) {
 *         CA_descriptor() elementary stream level 
 *       }
 *     }
 *   }
 * }
 */

static guint
get_ca_descriptors_length (GPtrArray * descriptors)
{
  guint i;
  guint nb_desc = descriptors->len;
  guint len = 0;

  for (i = 0; i < nb_desc; i++) {
    GstMpegtsDescriptor *desc = g_ptr_array_index (descriptors, i);
    if (desc->tag == 0x09)
      len += desc->length;
  }

  return len;
}

static guint8 *
write_ca_descriptors (guint8 * body, GPtrArray * descriptors)
{
  guint i, nb_desc;

  nb_desc = descriptors->len;
  for (i = 0; i < nb_desc; i++) {
    GstMpegtsDescriptor *desc = g_ptr_array_index (descriptors, i);
    if (desc->tag == 0x09) {
      memcpy (body, desc->data, desc->length);
      body += desc->length;
    }
  }

  return body;
}

guint8 *
cam_build_ca_pmt (GstMpegtsPMT * pmt, guint8 list_management, guint8 cmd_id,
    guint * size)
{
  GstMpegtsSection *section = (GstMpegtsSection *) pmt;
  guint body_size = 0;
  guint8 *buffer;
  guint8 *body;
  GList *lengths = NULL;
  guint len = 0;
  guint i;

  /* get the length of program level CA_descriptor()s */
  len = get_ca_descriptors_length (pmt->descriptors);
  if (len > 0)
    /* add one byte for the program level cmd_id */
    len += 1;

  lengths = g_list_append (lengths, GINT_TO_POINTER (len));
  body_size += 6 + len;

  for (i = 0; i < pmt->streams->len; i++) {
    GstMpegtsPMTStream *pmtstream = g_ptr_array_index (pmt->streams, i);

    len = get_ca_descriptors_length (pmtstream->descriptors);
    if (len > 0)
      /* one byte for the stream level cmd_id */
      len += 1;

    lengths = g_list_append (lengths, GINT_TO_POINTER (len));
    body_size += 5 + len;
  }

  GST_DEBUG ("Body Size %d", body_size);

  buffer = g_malloc0 (body_size);
  body = buffer;

  /* ca_pmt_list_management 8 uimsbf */
  *body++ = list_management;

  /* program_number 16 uimsbf */
  GST_WRITE_UINT16_BE (body, section->subtable_extension);
  body += 2;

  /* reserved 2
   * version_number 5
   * current_next_indicator 1
   */
  *body++ = (section->version_number << 1) | 0x01;

  /* Reserved 4
   * program_info_length 12
   */
  len = GPOINTER_TO_INT (lengths->data);
  lengths = g_list_delete_link (lengths, lengths);
  GST_WRITE_UINT16_BE (body, len);
  body += 2;

  if (len != 0) {
    *body++ = cmd_id;

    body = write_ca_descriptors (body, pmt->descriptors);
  }

  for (i = 0; i < pmt->streams->len; i++) {
    GstMpegtsPMTStream *pmtstream = g_ptr_array_index (pmt->streams, i);

    *body++ = pmtstream->stream_type;
    GST_WRITE_UINT16_BE (body, pmtstream->pid);
    body += 2;
    len = GPOINTER_TO_INT (lengths->data);
    lengths = g_list_delete_link (lengths, lengths);
    GST_WRITE_UINT16_BE (body, len);
    body += 2;

    if (len != 0) {
      *body++ = cmd_id;
      body = write_ca_descriptors (body, pmtstream->descriptors);
    }
  }

  *size = body_size;
  return buffer;
}
