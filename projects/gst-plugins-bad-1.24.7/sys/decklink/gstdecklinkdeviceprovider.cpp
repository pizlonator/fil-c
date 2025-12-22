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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstdecklinkdeviceprovider.h"
#include "gstdecklink.h"

#define DEFAULT_PERSISTENT_ID (-1)

G_DEFINE_TYPE (GstDecklinkDeviceProvider, gst_decklink_device_provider,
    GST_TYPE_DEVICE_PROVIDER);
GST_DEVICE_PROVIDER_REGISTER_DEFINE (decklinkdeviceprovider, "decklinkdeviceprovider",
    GST_RANK_PRIMARY, GST_TYPE_DECKLINK_DEVICE_PROVIDER);

static void
gst_decklink_device_provider_init (GstDecklinkDeviceProvider * self)
{
}

static GList *
gst_decklink_device_provider_probe (GstDeviceProvider * provider)
{
  return gst_decklink_get_devices ();
}

static void
gst_decklink_device_provider_class_init (GstDecklinkDeviceProviderClass * klass)
{
  GstDeviceProviderClass *dm_class = GST_DEVICE_PROVIDER_CLASS (klass);

  dm_class->probe = GST_DEBUG_FUNCPTR (gst_decklink_device_provider_probe);

  gst_device_provider_class_set_static_metadata (dm_class,
      "Decklink Device Provider", "Hardware/Source/Sink/Audio/Video",
      "Lists and provides Decklink devices",
      "Sebastian Dröge <sebastian@centricular.com>");
}

G_DEFINE_TYPE (GstDecklinkDevice, gst_decklink_device, GST_TYPE_DEVICE);

static void
gst_decklink_device_init (GstDecklinkDevice * self)
{
}

static GstElement *
gst_decklink_device_create_element (GstDevice * device, const gchar * name)
{
  GstDecklinkDevice *self = GST_DECKLINK_DEVICE (device);
  GstElement *ret = NULL;

  if (self->video && self->capture) {
    ret = gst_element_factory_make ("decklinkvideosrc", name);
  } else if (!self->video && self->capture) {
    ret = gst_element_factory_make ("decklinkaudiosrc", name);
  } else if (self->video && !self->capture) {
    ret = gst_element_factory_make ("decklinkvideosink", name);
  } else {
    ret = gst_element_factory_make ("decklinkaudiosink", name);
  }

  if (ret) {
    g_object_set (ret, "persistent-id", self->persistent_id, NULL);
  }

  return ret;
}

static void
gst_decklink_device_class_init (GstDecklinkDeviceClass * klass)
{
  GstDeviceClass *gst_device_class = GST_DEVICE_CLASS (klass);

  gst_device_class->create_element =
      GST_DEBUG_FUNCPTR (gst_decklink_device_create_element);
}
