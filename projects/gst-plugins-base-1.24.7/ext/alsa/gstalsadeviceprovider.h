/* GStreamer
 * Copyright (C) 2018 Thibault Saunier <tsaunier@igalia.com>
 *
 * alsadeviceprovider.c: alsa device probing and monitoring
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


#ifndef __GST_ALSA_DEVICE_PROVIDER_H__
#define __GST_ALSA_DEVICE_PROVIDER_H__

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstalsa.h"
#include <gst/gst.h>

G_BEGIN_DECLS

typedef struct _GstAlsaDeviceProvider GstAlsaDeviceProvider;
typedef struct _GstAlsaDeviceProviderClass GstAlsaDeviceProviderClass;

#define GST_TYPE_ALSA_DEVICE_PROVIDER                 (gst_alsa_device_provider_get_type())
#define GST_IS_ALSA_DEVICE_PROVIDER(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_ALSA_DEVICE_PROVIDER))
#define GST_IS_ALSA_DEVICE_PROVIDER_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_ALSA_DEVICE_PROVIDER))
#define GST_ALSA_DEVICE_PROVIDER_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_ALSA_DEVICE_PROVIDER, GstAlsaDeviceProviderClass))
#define GST_ALSA_DEVICE_PROVIDER(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_ALSA_DEVICE_PROVIDER, GstAlsaDeviceProvider))
#define GST_ALSA_DEVICE_PROVIDER_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_DEVICE_PROVIDER, GstAlsaDeviceProviderClass))
#define GST_ALSA_DEVICE_PROVIDER_CAST(obj)            ((GstAlsaDeviceProvider *)(obj))

struct _GstAlsaDeviceProvider {
  GstDeviceProvider         parent;
};

struct _GstAlsaDeviceProviderClass {
  GstDeviceProviderClass    parent_class;
};

GType        gst_alsa_device_provider_get_type (void);


typedef struct _GstAlsaDevice GstAlsaDevice;
typedef struct _GstAlsaDeviceClass GstAlsaDeviceClass;

#define GST_TYPE_ALSA_DEVICE                 (gst_alsa_device_get_type())
#define GST_IS_ALSA_DEVICE(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_ALSA_DEVICE))
#define GST_IS_ALSA_DEVICE_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_ALSA_DEVICE))
#define GST_ALSA_DEVICE_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_ALSA_DEVICE, GstAlsaDeviceClass))
#define GST_ALSA_DEVICE(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_ALSA_DEVICE, GstAlsaDevice))
#define GST_ALSA_DEVICE_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_DEVICE, GstAlsaDeviceClass))
#define GST_ALSA_DEVICE_CAST(obj)            ((GstAlsaDevice *)(obj))

struct _GstAlsaDevice {
  GstDevice         parent;

  snd_pcm_stream_t  stream;
  gchar            *internal_name;
  const gchar      *element;
};

struct _GstAlsaDeviceClass {
  GstDeviceClass    parent_class;
};

GType        gst_alsa_device_get_type (void);

G_END_DECLS

#endif /* __GST_ALSA_DEVICE_PROVIDER_H__ */

