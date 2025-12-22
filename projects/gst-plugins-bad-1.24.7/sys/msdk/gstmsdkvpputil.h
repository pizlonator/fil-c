/*
 *  gstmsdkvpputil.h - MediaSDK video post processing utilities
 *
 *  Copyright (C) 2016 Intel Corporation
 *    Author: Gwenole Beauchesne <gwenole.beauchesne@intel.com>
 *    Author: Victor Jaquez <victorx.jaquez@intel.com>
 *  Copyright (C) 2018 Intel Corporation
 *    Author: Sreerenj Balachandran <sreerenj.balachandran@intel.com>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License
 *  as published by the Free Software Foundation; either version 2.1
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this program; if not, write to the Free
 *  Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301 USA
 */

#ifndef GST_MSDKVPPUTIL_H
#define GST_MSDKVPPUTIL_H

#include "gstmsdkvpp.h"

G_BEGIN_DECLS

GstCaps *gst_msdkvpp_fixate_srccaps (GstMsdkVPP * msdkvpp,
    GstCaps * sinkcaps, GstCaps * srccaps);

gboolean
gst_msdkvpp_is_deinterlace_enabled (GstMsdkVPP * msdkvpp,
    GstVideoInfo * vip);

G_END_DECLS

#endif /* GST_MSDKVPPUTIL_H */
