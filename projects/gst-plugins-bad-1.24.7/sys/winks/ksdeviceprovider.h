/* GStreamer
 * Copyright (C) 2015 Руслан Ижбулатов <lrn1986@gmail.com>
 *
 * ksdeviceprovider.h: Kernel Streaming device probing and monitoring
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


#ifndef __GST_KS_DEVICE_PROVIDER_H__
#define __GST_KS_DEVICE_PROVIDER_H__

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <windows.h>

G_BEGIN_DECLS

typedef struct _GstKsDeviceProvider GstKsDeviceProvider;
typedef struct _GstKsDeviceProviderPrivate GstKsDeviceProviderPrivate;
typedef struct _GstKsDeviceProviderClass GstKsDeviceProviderClass;

#define GST_TYPE_KS_DEVICE_PROVIDER                 (gst_ks_device_provider_get_type())
#define GST_IS_KS_DEVICE_PROVIDER(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_KS_DEVICE_PROVIDER))
#define GST_IS_KS_DEVICE_PROVIDER_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_KS_DEVICE_PROVIDER))
#define GST_KS_DEVICE_PROVIDER_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_KS_DEVICE_PROVIDER, GstKsDeviceProviderClass))
#define GST_KS_DEVICE_PROVIDER(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_KS_DEVICE_PROVIDER, GstKsDeviceProvider))
#define GST_KS_DEVICE_PROVIDER_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_DEVICE_PROVIDER, GstKsDeviceProviderClass))
#define GST_KS_DEVICE_PROVIDER_CAST(obj)            ((GstKsDeviceProvider *)(obj))

struct _GstKsDeviceProvider {
  GstDeviceProvider parent;

  HANDLE            message_window;
  ATOM              message_window_class;
  HDEVNOTIFY        device_notify_handle;
  HANDLE            wakeup_event;
  GThread          *message_thread;
};

typedef enum {
  GST_KS_DEVICE_TYPE_INVALID = 0,
  GST_KS_DEVICE_TYPE_VIDEO_SOURCE,
  GST_KS_DEVICE_TYPE_VIDEO_SINK,
  GST_KS_DEVICE_TYPE_AUDIO_SOURCE,
  GST_KS_DEVICE_TYPE_AUDIO_SINK
} GstKsDeviceType;

struct _GstKsDeviceProviderClass {
  GstDeviceProviderClass    parent_class;
};

GType gst_ks_device_provider_get_type (void);


typedef struct _GstKsDevice GstKsDevice;
typedef struct _GstKsDevicePrivate GstKsDevicePrivate;
typedef struct _GstKsDeviceClass GstKsDeviceClass;

#define GST_TYPE_KS_DEVICE                 (gst_ks_device_get_type())
#define GST_IS_KS_DEVICE(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_KS_DEVICE))
#define GST_IS_KS_DEVICE_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_KS_DEVICE))
#define GST_KS_DEVICE_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_KS_DEVICE, GstKsDeviceClass))
#define GST_KS_DEVICE(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_KS_DEVICE, GstKsDevice))
#define GST_KS_DEVICE_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_DEVICE, GstKsDeviceClass))
#define GST_KS_DEVICE_CAST(obj)            ((GstKsDevice *)(obj))

struct _GstKsDevice {
  GstDevice         parent;

  GstKsDeviceType   type;
  guint             device_index;
  gchar            *path;
  const gchar      *element;
};

struct _GstKsDeviceClass {
  GstDeviceClass    parent_class;
};

GType gst_ks_device_get_type (void);

G_END_DECLS

#endif /* __GST_KS_DEVICE_PROVIDER_H__ */
