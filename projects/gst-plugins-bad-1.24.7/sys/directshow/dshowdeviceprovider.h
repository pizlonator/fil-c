/* GStreamer
 * Copyright (C) 2018 Joshua M. Doe <oss@nvl.army.mil>
 *
 * dshowdeviceprovider.h: DirectShow device probing
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


#ifndef __GST_DSHOW_DEVICE_PROVIDER_H__
#define __GST_DSHOW_DEVICE_PROVIDER_H__

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>

G_BEGIN_DECLS

typedef struct _GstDshowDeviceProvider GstDshowDeviceProvider;
typedef struct _GstDshowDeviceProviderPrivate GstDshowDeviceProviderPrivate;
typedef struct _GstDshowDeviceProviderClass GstDshowDeviceProviderClass;

#define GST_TYPE_DSHOW_DEVICE_PROVIDER                 (gst_dshow_device_provider_get_type())
#define GST_IS_DSHOW_DEVICE_PROVIDER(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_DSHOW_DEVICE_PROVIDER))
#define GST_IS_DSHOW_DEVICE_PROVIDER_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_DSHOW_DEVICE_PROVIDER))
#define GST_DSHOW_DEVICE_PROVIDER_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_DSHOW_DEVICE_PROVIDER, GstDshowDeviceProviderClass))
#define GST_DSHOW_DEVICE_PROVIDER(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_DSHOW_DEVICE_PROVIDER, GstDshowDeviceProvider))
#define GST_DSHOW_DEVICE_PROVIDER_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_DEVICE_PROVIDER, GstDshowDeviceProviderClass))
#define GST_DSHOW_DEVICE_PROVIDER_CAST(obj)            ((GstDshowDeviceProvider *)(obj))

struct _GstDshowDeviceProvider {
  GstDeviceProvider parent;
};

typedef enum {
  GST_DSHOW_DEVICE_TYPE_INVALID = 0,
  GST_DSHOW_DEVICE_TYPE_VIDEO_SOURCE,
  GST_DSHOW_DEVICE_TYPE_AUDIO_SOURCE,
} GstDshowDeviceType;

struct _GstDshowDeviceProviderClass {
  GstDeviceProviderClass    parent_class;
};

GType gst_dshow_device_provider_get_type (void);


typedef struct _GstDshowDevice GstDshowDevice;
typedef struct _GstDshowDevicePrivate GstDshowDevicePrivate;
typedef struct _GstDshowDeviceClass GstDshowDeviceClass;

#define GST_TYPE_DSHOW_DEVICE                 (gst_dshow_device_get_type())
#define GST_IS_DSHOW_DEVICE(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_DSHOW_DEVICE))
#define GST_IS_DSHOW_DEVICE_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_DSHOW_DEVICE))
#define GST_DSHOW_DEVICE_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_DSHOW_DEVICE, GstDshowDeviceClass))
#define GST_DSHOW_DEVICE(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_DSHOW_DEVICE, GstDshowDevice))
#define GST_DSHOW_DEVICE_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_DEVICE, GstDshowDeviceClass))
#define GST_DSHOW_DEVICE_CAST(obj)            ((GstDshowDevice *)(obj))

struct _GstDshowDevice {
  GstDevice         parent;

  GstDshowDeviceType   type;
  guint             device_index;
  gchar            *device;
  gchar            *device_name;
  const gchar      *element;
};

struct _GstDshowDeviceClass {
  GstDeviceClass    parent_class;
};

GType gst_dshow_device_get_type (void);

G_END_DECLS

#endif /* __GST_DSHOW_DEVICE_PROVIDER_H__ */
