/*
 * Copyright (C) 2019 Mathieu Duponchelle <mathieu@centricular.com>
 * Copyright (C) 2019 Sebastian Dröge <sebastian@centricular.com>
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
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef _GST_DECKLINK_DEVICE_PROVIDER_H_
#define _GST_DECKLINK_DEVICE_PROVIDER_H_

#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_TYPE_DECKLINK_DEVICE_PROVIDER gst_decklink_device_provider_get_type()
#define GST_DECKLINK_DEVICE_PROVIDER(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_DECKLINK_DEVICE_PROVIDER,GstDecklinkDeviceProvider))

typedef struct _GstDecklinkDeviceProvider GstDecklinkDeviceProvider;
typedef struct _GstDecklinkDeviceProviderClass GstDecklinkDeviceProviderClass;

struct _GstDecklinkDeviceProviderClass
{
  GstDeviceProviderClass parent_class;
};

struct _GstDecklinkDeviceProvider
{
  GstDeviceProvider parent;
};

GType gst_decklink_device_provider_get_type (void);
GST_DEVICE_PROVIDER_REGISTER_DECLARE (decklinkdeviceprovider);

G_END_DECLS

#endif /* _GST_DECKLINK_DEVICE_PROVIDER_H_ */
