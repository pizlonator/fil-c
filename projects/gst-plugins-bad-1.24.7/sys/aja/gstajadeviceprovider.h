/*
 * Copyright (C) 2019 Mathieu Duponchelle <mathieu@centricular.com>
 * Copyright (C) 2019,2021 Sebastian Dröge <sebastian@centricular.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#ifndef _GST_AJA_DEVICE_PROVIDER_H_
#define _GST_AJA_DEVICE_PROVIDER_H_

#include <ajantv2/includes/ntv2devicescanner.h>
#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_TYPE_AJA_DEVICE_PROVIDER gst_aja_device_provider_get_type()
#define GST_AJA_DEVICE_PROVIDER(obj)                               \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_AJA_DEVICE_PROVIDER, \
                              GstAjaDeviceProvider))

typedef struct _GstAjaDeviceProvider GstAjaDeviceProvider;
typedef struct _GstAjaDeviceProviderClass GstAjaDeviceProviderClass;

struct _GstAjaDeviceProviderClass {
  GstDeviceProviderClass parent_class;
};

struct _GstAjaDeviceProvider {
  GstDeviceProvider parent;
};

G_GNUC_INTERNAL
GType gst_aja_device_provider_get_type(void);

#define GST_TYPE_AJA_DEVICE gst_aja_device_get_type()
#define GST_AJA_DEVICE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_AJA_DEVICE, GstAjaDevice))

typedef struct _GstAjaDevice GstAjaDevice;
typedef struct _GstAjaDeviceClass GstAjaDeviceClass;

struct _GstAjaDeviceClass {
  GstDeviceClass parent_class;
};

struct _GstAjaDevice {
  GstDevice parent;
  gboolean is_capture;
  guint device_index;
};

G_GNUC_INTERNAL
GType gst_aja_device_get_type(void);

G_END_DECLS

#endif /* _GST_AJA_DEVICE_PROVIDER_H_ */
