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
#ifndef __GST_AVTP_CRF_UTILS_H__
#define __GST_AVTP_CRF_UTILS_H__

#include <avtp.h>

#include "gstavtpcrfbase.h"

gboolean buffer_size_valid (GstMapInfo * info);
GstClockTime get_avtp_tstamp (GstAvtpCrfBase * avtpcrfbase,
    struct avtp_stream_pdu * pdu);
gboolean h264_tstamp_valid (struct avtp_stream_pdu * pdu);

#endif /* __GST_AVTP_CRF_UTILS_H__ */
