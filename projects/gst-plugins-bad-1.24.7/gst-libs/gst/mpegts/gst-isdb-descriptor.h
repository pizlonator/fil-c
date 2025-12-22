/*
 * gst-isdb-descriptor.h -
 * Copyright (C) 2020 Edward Hervey
 *
 * Authors:
 *   Edward Hervey <edward@centricular.com>
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

#ifndef GST_ISDB_DESCRIPTOR_H
#define GST_ISDB_DESCRIPTOR_H

#include <gst/gst.h>
#include <gst/mpegts/mpegts-prelude.h>

G_BEGIN_DECLS

/**
 * SECTION:gst-isdb-descriptor
 * @title: ISDB variants of MPEG-TS descriptors
 * @short_description: Descriptors for the various ISDB specifications
 * @include: gst/mpegts/mpegts.h
 *
 * This contains the various descriptors defined by the ISDB specifications
 */

/**
 * GstMpegtsISDBDescriptorType:
 *
 * These values correspond to the registered descriptor type from
 * the various ISDB specifications.
 *
 * Consult the relevant specifications for more details.
 */
typedef enum {
  /* ISDB ARIB B10 v4.6 */
  GST_MTS_DESC_ISDB_HIERARCHICAL_TRANSMISSION   = 0xC0,
  GST_MTS_DESC_ISDB_DIGITAL_COPY_CONTROL        = 0xC1,
  GST_MTS_DESC_ISDB_NETWORK_IDENTIFICATION      = 0xC2,
  GST_MTS_DESC_ISDB_PARTIAL_TS_TIME             = 0xc3,
  GST_MTS_DESC_ISDB_AUDIO_COMPONENT             = 0xc4,
  GST_MTS_DESC_ISDB_HYPERLINK                   = 0xc5,
  GST_MTS_DESC_ISDB_TARGET_REGION               = 0xc6,
  GST_MTS_DESC_ISDB_DATA_CONTENT                = 0xc7,
  GST_MTS_DESC_ISDB_VIDEO_DECODE_CONTROL        = 0xc8,
  GST_MTS_DESC_ISDB_DOWNLOAD_CONTENT            = 0xc9,
  GST_MTS_DESC_ISDB_CA_EMM_TS                   = 0xca,
  GST_MTS_DESC_ISDB_CA_CONTRACT_INFORMATION     = 0xcb,
  GST_MTS_DESC_ISDB_CA_SERVICE                  = 0xcc,
  GST_MTS_DESC_ISDB_TS_INFORMATION              = 0xcd,
  GST_MTS_DESC_ISDB_EXTENDED_BROADCASTER        = 0xce,
  GST_MTS_DESC_ISDB_LOGO_TRANSMISSION           = 0xcf,
  GST_MTS_DESC_ISDB_BASIC_LOCAL_EVENT           = 0xd0,
  GST_MTS_DESC_ISDB_REFERENCE                   = 0xd1,
  GST_MTS_DESC_ISDB_NODE_RELATION               = 0xd2,
  GST_MTS_DESC_ISDB_SHORT_NODE_INFORMATION      = 0xd3,
  GST_MTS_DESC_ISDB_STC_REFERENCE               = 0xd4,
  GST_MTS_DESC_ISDB_SERIES                      = 0xd5,
  GST_MTS_DESC_ISDB_EVENT_GROUP                 = 0xd6,
  GST_MTS_DESC_ISDB_SI_PARAMETER                = 0xd7,
  GST_MTS_DESC_ISDB_BROADCASTER_NAME            = 0xd8,
  GST_MTS_DESC_ISDB_COMPONENT_GROUP             = 0xd9,
  GST_MTS_DESC_ISDB_SI_PRIME_TS                 = 0xda,
  GST_MTS_DESC_ISDB_BOARD_INFORMATION           = 0xdb,
  GST_MTS_DESC_ISDB_LDT_LINKAGE                 = 0xdc,
  GST_MTS_DESC_ISDB_CONNECTED_TRANSMISSION      = 0xdd,
  GST_MTS_DESC_ISDB_CONTENT_AVAILABILITY        = 0xde,
  /* ... */
  GST_MTS_DESC_ISDB_SERVICE_GROUP               = 0xe0

} GstMpegtsISDBDescriptorType;

G_END_DECLS

#endif
