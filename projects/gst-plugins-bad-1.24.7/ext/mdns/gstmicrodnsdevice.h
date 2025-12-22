/* GStreamer
 * Copyright (C) 2019 Mathieu Duponchelle <mathieu@centricular.com>
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GST_MICRODNS_DEVICE_H__
#define __GST_MICRODNS_DEVICE_H__

#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_TYPE_MDNS_DEVICE_PROVIDER gst_mdns_device_provider_get_type()

G_DECLARE_FINAL_TYPE (GstMDNSDeviceProvider, gst_mdns_device_provider, GST,
    MDNS_DEVICE_PROVIDER, GstDeviceProvider);

#define GST_TYPE_MDNS_DEVICE gst_mdns_device_get_type()

G_DECLARE_FINAL_TYPE (GstMDNSDevice, gst_mdns_device, GST, MDNS_DEVICE,
    GstDevice);

G_END_DECLS

#endif /* __GST_MICRODNS_DEVICE_H__ */
