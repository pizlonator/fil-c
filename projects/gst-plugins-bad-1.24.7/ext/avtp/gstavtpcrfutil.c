/*
 * GStreamer AVTP Plugin
 * Copyright (C) 2019 Intel Corporation
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
#include <avtp.h>
#include <avtp_aaf.h>
#include <avtp_cvf.h>
#include <glib.h>

#include "gstavtpcrfutil.h"

#define AVTP_CVF_H264_HEADER_SIZE (sizeof(struct avtp_stream_pdu) + sizeof(guint32))

gboolean
buffer_size_valid (GstMapInfo * info)
{
  struct avtp_stream_pdu *pdu;
  guint64 subtype;
  guint32 type;
  int res;

  if (info->size < sizeof (struct avtp_stream_pdu))
    return FALSE;

  pdu = (struct avtp_stream_pdu *) info->data;

  res =
      avtp_pdu_get ((struct avtp_common_pdu *) pdu, AVTP_FIELD_SUBTYPE, &type);
  g_assert (res == 0);
  res = avtp_cvf_pdu_get (pdu, AVTP_CVF_FIELD_FORMAT_SUBTYPE, &subtype);
  g_assert (res == 0);

  if (type == AVTP_SUBTYPE_CVF && subtype == AVTP_CVF_FORMAT_SUBTYPE_H264
      && info->size < AVTP_CVF_H264_HEADER_SIZE)
    return FALSE;

  return TRUE;
}

GstClockTime
get_avtp_tstamp (GstAvtpCrfBase * avtpcrfbase, struct avtp_stream_pdu *pdu)
{
  guint64 tstamp = GST_CLOCK_TIME_NONE, tstamp_valid;
  guint32 type;
  int res;

  res =
      avtp_pdu_get ((struct avtp_common_pdu *) pdu, AVTP_FIELD_SUBTYPE, &type);
  g_assert (res == 0);

  switch (type) {
    case AVTP_SUBTYPE_AAF:
      res = avtp_aaf_pdu_get (pdu, AVTP_AAF_FIELD_TV, &tstamp_valid);
      g_assert (res == 0);
      if (!tstamp_valid)
        break;

      res = avtp_aaf_pdu_get (pdu, AVTP_AAF_FIELD_TIMESTAMP, &tstamp);
      g_assert (res == 0);
      break;
    case AVTP_SUBTYPE_CVF:
      res = avtp_cvf_pdu_get (pdu, AVTP_CVF_FIELD_TV, &tstamp_valid);
      g_assert (res == 0);
      if (!tstamp_valid)
        break;

      res = avtp_cvf_pdu_get (pdu, AVTP_CVF_FIELD_TIMESTAMP, &tstamp);
      g_assert (res == 0);
      break;
    default:
      GST_INFO_OBJECT (avtpcrfbase, "type 0x%x not supported.\n", type);
      break;
  }

  return (GstClockTime) tstamp;
}

gboolean
h264_tstamp_valid (struct avtp_stream_pdu *pdu)
{
  guint64 subtype, h264_time_valid;
  guint32 type;
  int res;

  /*
   * Validate H264 timestamp for H264 format. For more details about the
   * timestamp look at IEEE 1722-2016 Section 8.5.3.1
   */
  res =
      avtp_pdu_get ((struct avtp_common_pdu *) pdu, AVTP_FIELD_SUBTYPE, &type);
  g_assert (res == 0);
  if (type == AVTP_SUBTYPE_CVF) {
    res = avtp_cvf_pdu_get (pdu, AVTP_CVF_FIELD_FORMAT_SUBTYPE, &subtype);
    g_assert (res == 0);
    res = avtp_cvf_pdu_get (pdu, AVTP_CVF_FIELD_H264_PTV, &h264_time_valid);
    g_assert (res == 0);

    if (subtype == AVTP_CVF_FORMAT_SUBTYPE_H264 && h264_time_valid)
      return TRUE;
  }
  return FALSE;
}
