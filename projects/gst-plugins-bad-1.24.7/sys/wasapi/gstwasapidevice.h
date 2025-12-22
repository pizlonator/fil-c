/* GStreamer
 * Copyright (C) 2018 Nirbheek Chauhan <nirbheek@centricular.com>
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

#ifndef __GST_WASAPI_DEVICE_H__
#define __GST_WASAPI_DEVICE_H__

#include "gstwasapiutil.h"

G_BEGIN_DECLS

typedef struct _GstWasapiDeviceProvider GstWasapiDeviceProvider;
typedef struct _GstWasapiDeviceProviderClass GstWasapiDeviceProviderClass;

#define GST_TYPE_WASAPI_DEVICE_PROVIDER                 (gst_wasapi_device_provider_get_type())
#define GST_IS_WASAPI_DEVICE_PROVIDER(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_WASAPI_DEVICE_PROVIDER))
#define GST_IS_WASAPI_DEVICE_PROVIDER_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_WASAPI_DEVICE_PROVIDER))
#define GST_WASAPI_DEVICE_PROVIDER_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_WASAPI_DEVICE_PROVIDER, GstWasapiDeviceProviderClass))
#define GST_WASAPI_DEVICE_PROVIDER(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_WASAPI_DEVICE_PROVIDER, GstWasapiDeviceProvider))
#define GST_WASAPI_DEVICE_PROVIDER_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_DEVICE_PROVIDER, GstWasapiDeviceProviderClass))
#define GST_WASAPI_DEVICE_PROVIDER_CAST(obj)            ((GstWasapiDeviceProvider *)(obj))

struct _GstWasapiDeviceProvider
{
  GstDeviceProvider parent;

  GstMMDeviceEnumerator *enumerator;
};

struct _GstWasapiDeviceProviderClass
{
  GstDeviceProviderClass parent_class;
};

GType gst_wasapi_device_provider_get_type (void);


typedef struct _GstWasapiDevice GstWasapiDevice;
typedef struct _GstWasapiDeviceClass GstWasapiDeviceClass;

#define GST_TYPE_WASAPI_DEVICE                 (gst_wasapi_device_get_type())
#define GST_IS_WASAPI_DEVICE(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_WASAPI_DEVICE))
#define GST_IS_WASAPI_DEVICE_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_WASAPI_DEVICE))
#define GST_WASAPI_DEVICE_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_WASAPI_DEVICE, GstWasapiDeviceClass))
#define GST_WASAPI_DEVICE(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_WASAPI_DEVICE, GstWasapiDevice))
#define GST_WASAPI_DEVICE_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_DEVICE, GstWasapiDeviceClass))
#define GST_WASAPI_DEVICE_CAST(obj)            ((GstWasapiDevice *)(obj))

struct _GstWasapiDevice
{
  GstDevice parent;

  gchar *strid;
  const gchar *element;
};

struct _GstWasapiDeviceClass
{
  GstDeviceClass parent_class;
};

GType gst_wasapi_device_get_type (void);

G_END_DECLS

#endif /* __GST_WASAPI_DEVICE_H__ */
