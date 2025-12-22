/* GStreamer
 * Copyright (C) 2019 Josh Matthews <josh@joshmatthews.net>
 *
 * avfdeviceprovider.h: AVF device probing and monitoring
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


#ifndef __GST_AVF_DEVICE_PROVIDER_H__
#define __GST_AVF_DEVICE_PROVIDER_H__

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include "avfvideosrc.h"

G_BEGIN_DECLS

typedef struct _GstAVFDeviceProvider GstAVFDeviceProvider;
typedef struct _GstAVFDeviceProviderPrivate GstAVFDeviceProviderPrivate;
typedef struct _GstAVFDeviceProviderClass GstAVFDeviceProviderClass;

#define GST_TYPE_AVF_DEVICE_PROVIDER                 (gst_avf_device_provider_get_type())
#define GST_IS_AVF_DEVICE_PROVIDER(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_AVF_DEVICE_PROVIDER))
#define GST_IS_AVF_DEVICE_PROVIDER_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_AVF_DEVICE_PROVIDER))
#define GST_AVF_DEVICE_PROVIDER_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_AVF_DEVICE_PROVIDER, GstAVFDeviceProviderClass))
#define GST_AVF_DEVICE_PROVIDER(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_AVF_DEVICE_PROVIDER, GstAVFDeviceProvider))
#define GST_AVF_DEVICE_PROVIDER_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_DEVICE_PROVIDER, GstAVFDeviceProviderClass))
#define GST_AVF_DEVICE_PROVIDER_CAST(obj)            ((GstAvfDeviceProvider *)(obj))

struct _GstAVFDeviceProvider {
  GstDeviceProvider parent;
};

typedef enum {
  GST_AVF_DEVICE_TYPE_INVALID = 0,
  GST_AVF_DEVICE_TYPE_VIDEO_SOURCE,
} GstAvfDeviceType;

struct _GstAVFDeviceProviderClass {
  GstDeviceProviderClass    parent_class;
};

GType gst_avf_device_provider_get_type (void);


typedef struct _GstAvfDevice GstAvfDevice;
typedef struct _GstAvfDevicePrivate GstAvfDevicePrivate;
typedef struct _GstAvfDeviceClass GstAvfDeviceClass;

#define GST_TYPE_AVF_DEVICE                 (gst_avf_device_get_type())
#define GST_IS_AVF_DEVICE(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_AVF_DEVICE))
#define GST_IS_AVF_DEVICE_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_AVF_DEVICE))
#define GST_AVF_DEVICE_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_AVF_DEVICE, GstAvfDeviceClass))
#define GST_AVF_DEVICE(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_AVF_DEVICE, GstAvfDevice))
#define GST_AVF_DEVICE_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_DEVICE, GstAvfDeviceClass))
#define GST_AVF_DEVICE_CAST(obj)            ((GstAvfDevice *)(obj))

struct _GstAvfDevice {
  GstDevice         parent;

  GstAvfDeviceType  type;
  int               device_index;
  const gchar      *element;
};

struct _GstAvfDeviceClass {
  GstDeviceClass    parent_class;
};

GType gst_avf_device_get_type (void);

G_END_DECLS

#endif /* __GST_AVF_DEVICE_PROVIDER_H__ */
