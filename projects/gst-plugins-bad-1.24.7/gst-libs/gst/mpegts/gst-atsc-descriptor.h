/*
 * gstmpegtsdescriptor.h -
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

#ifndef GST_ATSC_DESCRIPTOR_H
#define GST_ATSC_DESCRIPTOR_H

#include <gst/gst.h>
#include <gst/mpegts/mpegts-prelude.h>

G_BEGIN_DECLS

/**
 * SECTION:gst-atsc-descriptor
 * @title: ATSC variants of MPEG-TS descriptors
 * @short_description: Descriptors for the various ATSC specifications
 * @include: gst/mpegts/mpegts.h
 *
 * This contains the various descriptors defined by the ATSC specifications
 */

/**
 * GstMpegtsATSCDescriptorType:
 *
 * These values correspond to the registered descriptor type from
 * the various ATSC specifications.
 *
 * Consult the relevant specifications for more details.
 */
typedef enum {
  /* ATSC A/65 2009 */
  GST_MTS_DESC_ATSC_STUFFING                    = 0x80,
  GST_MTS_DESC_ATSC_AC3                         = 0x81,
  GST_MTS_DESC_ATSC_CAPTION_SERVICE             = 0x86,
  GST_MTS_DESC_ATSC_CONTENT_ADVISORY            = 0x87,
  GST_MTS_DESC_ATSC_EXTENDED_CHANNEL_NAME       = 0xA0,
  GST_MTS_DESC_ATSC_SERVICE_LOCATION            = 0xA1,
  GST_MTS_DESC_ATSC_TIME_SHIFTED_SERVICE        = 0xA2,
  GST_MTS_DESC_ATSC_COMPONENT_NAME              = 0xA3,
  GST_MTS_DESC_ATSC_DCC_DEPARTING_REQUEST       = 0xA8,
  GST_MTS_DESC_ATSC_DCC_ARRIVING_REQUEST        = 0xA9,
  GST_MTS_DESC_ATSC_REDISTRIBUTION_CONTROL      = 0xAA,
  GST_MTS_DESC_ATSC_GENRE                       = 0xAB,
  GST_MTS_DESC_ATSC_PRIVATE_INFORMATION         = 0xAD,
  GST_MTS_DESC_ATSC_EAC3                        = 0xCC,

  /* ATSC A/53:3 2009 */
  GST_MTS_DESC_ATSC_ENHANCED_SIGNALING          = 0xB2,

  /* ATSC A/90 */
  GST_MTS_DESC_ATSC_DATA_SERVICE                = 0xA4,
  GST_MTS_DESC_ATSC_PID_COUNT                   = 0xA5,
  GST_MTS_DESC_ATSC_DOWNLOAD_DESCRIPTOR         = 0xA6,
  GST_MTS_DESC_ATSC_MULTIPROTOCOL_ENCAPSULATION = 0xA7,
  GST_MTS_DESC_ATSC_MODULE_LINK                 = 0xB4,
  GST_MTS_DESC_ATSC_CRC32                       = 0xB5,
  GST_MTS_DESC_ATSC_GROUP_LINK                  = 0xB8,
} GstMpegtsATSCDescriptorType;

/* For backwards compatibility */
/** 
 * GST_MTS_DESC_AC3_AUDIO_STREAM: (skip) (attributes doc.skip=true)
 */
#define GST_MTS_DESC_AC3_AUDIO_STREAM GST_MTS_DESC_ATSC_AC3

G_END_DECLS

#endif
