/* GStreamer
 * Copyright (C) 2019 Thibault Saunier <tsaunier@igalia.com>
 *
 * gstuvc_h264deviceprovider.c: UvcH264 device probing and monitoring
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

/**
 * SECTION:provider-uvch264deviceprovider
 *
 * Device provider for uvch264 devices, it basically contains
 * the same information as the v4l2 device provider but on top
 * set the following properties:
 *
 * ```
 *   device.api=uvch264
 *   device.is-camerasrc=TRUE
 * ```
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "uvc_h264.h"
#include <gst/gst.h>
#include "gstuvch264deviceprovider.h"

enum
{
  PROP_DEVICE_PATH = 1,
};

/* *INDENT-OFF* */

struct _GstUvcH264Device
{
  GstDevice parent;
  gchar *device_path;
};

G_DEFINE_TYPE (GstUvcH264Device, gst_uvc_h264_device, GST_TYPE_DEVICE);
/* *INDENT-ON* */
GST_DEVICE_PROVIDER_REGISTER_DEFINE (uvch264deviceprovider,
    "uvch264deviceprovider", GST_RANK_PRIMARY,
    gst_uvc_h264_device_provider_get_type ());

static void
gst_uvc_h264_device_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstUvcH264Device *self = (GstUvcH264Device *) object;

  switch (prop_id) {
    case PROP_DEVICE_PATH:
      g_value_set_string (value, self->device_path);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstElement *
gst_uvc_h264_device_create_element (GstDevice * device, const gchar * name)
{
  GstUvcH264Device *self = (GstUvcH264Device *) device;
  GstElement *elem;

  elem = gst_element_factory_make ("uvch264src", name);
  g_object_set (elem, "device", self->device_path, NULL);

  return elem;
}

static void
gst_uvc_h264_device_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstUvcH264Device *self = (GstUvcH264Device *) object;

  switch (prop_id) {
    case PROP_DEVICE_PATH:
      self->device_path = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_uvc_h264_device_finalize (GObject * object)
{
  GstUvcH264Device *self = (GstUvcH264Device *) object;

  g_free (self->device_path);

  G_OBJECT_CLASS (gst_uvc_h264_device_parent_class)->finalize (object);
}

static void
gst_uvc_h264_device_class_init (GstUvcH264DeviceClass * klass)
{
  GstDeviceClass *dev_class = GST_DEVICE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  dev_class->create_element = gst_uvc_h264_device_create_element;

  object_class->get_property = gst_uvc_h264_device_get_property;
  object_class->set_property = gst_uvc_h264_device_set_property;
  object_class->finalize = gst_uvc_h264_device_finalize;

  g_object_class_install_property (object_class, PROP_DEVICE_PATH,
      g_param_spec_string ("device-path", "Device Path",
          "The Path of the device node", "",
          G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

static void
gst_uvc_h264_device_init (GstUvcH264Device * device)
{
}

/* *INDENT-OFF* */
struct _GstUvcH264DeviceProvider
{
  GstDeviceProvider parent;
  GstDeviceProvider *v4l2;

  guint bus_watch_id;
  libusb_context *usb_ctx;
};

G_DEFINE_TYPE (GstUvcH264DeviceProvider, gst_uvc_h264_device_provider,
    GST_TYPE_DEVICE_PROVIDER);
/* *INDENT-ON* */

static GstDevice *
create_device (GstUvcH264DeviceProvider * self, GstDevice * v4l2dev)
{
  GstDevice *dev = NULL;
  GstStructure *props = gst_device_get_properties (v4l2dev);
  const gchar *devname = gst_structure_get_string (props, "device.path");
  gchar *tmp, *device_name = NULL;
  GstCaps *caps;

  if (!xu_get_id (GST_OBJECT (self), devname, &self->usb_ctx)) {
    GST_INFO_OBJECT (self, "%s is not a uvch264 device", devname);
    goto done;
  }

  gst_structure_set (props, "device.api", G_TYPE_STRING, "uvch264",
      "device.is-camerasrc", G_TYPE_BOOLEAN, TRUE, NULL);

  caps = gst_device_get_caps (v4l2dev);
  tmp = gst_device_get_display_name (v4l2dev);
  device_name = g_strdup_printf ("UvcH264 %s", tmp);
  g_free (tmp);
  dev = g_object_new (gst_uvc_h264_device_get_type (), "device-path", devname,
      "display-name", device_name, "caps", caps, "device-class",
      "Video/CameraSource", "properties", props, NULL);
  if (caps)
    gst_caps_unref (caps);

done:
  g_free (device_name);
  gst_structure_free (props);

  return dev;
}

static GList *
gst_uvc_h264_device_provider_probe (GstDeviceProvider * provider)
{
  GList *tmp, *v4l2devs, *devs = NULL;
  GstUvcH264DeviceProvider *self = (GstUvcH264DeviceProvider *) provider;

  if (!self->v4l2)
    self->v4l2 = gst_device_provider_factory_get_by_name ("v4l2deviceprovider");

  if (!self->v4l2)
    return NULL;

  v4l2devs = gst_device_provider_get_devices (self->v4l2);
  for (tmp = v4l2devs; tmp; tmp = tmp->next) {
    GstDevice *dev = create_device (self, tmp->data);

    if (dev)
      devs = g_list_prepend (devs, dev);
  }
  g_list_free_full (v4l2devs, gst_object_unref);

  return devs;
}

static void
_bus_message_cb (GstBus * bus, GstMessage * msg,
    GstUvcH264DeviceProvider * self)
{
  GstDevice *v4l2dev;
  GstDeviceProvider *provider = (GstDeviceProvider *) self;

  if (GST_MESSAGE_SRC (msg) != GST_OBJECT (self->v4l2))
    return;

  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_DEVICE_ADDED:
    {
      GstDevice *dev;
      gst_message_parse_device_added (msg, &v4l2dev);

      if ((dev = create_device (self, v4l2dev)))
        gst_device_provider_device_add (provider, dev);

      break;
    }
    case GST_MESSAGE_DEVICE_REMOVED:
    {
      GList *item;
      gchar *v4l2path;
      GstDevice *dev = NULL;

      gst_message_parse_device_removed (msg, &v4l2dev);

      g_object_get (v4l2dev, "device-path", &v4l2path, NULL);

      GST_OBJECT_LOCK (self);
      for (item = provider->devices; item; item = item->next) {
        if (!g_strcmp0 (((GstUvcH264Device *) item->data)->device_path,
                v4l2path)) {
          dev = item->data;
          break;
        }
      }
      GST_OBJECT_UNLOCK (self);

      if (dev)
        gst_device_provider_device_remove (provider, dev);
      break;
    }
    default:
      break;
  }

}

static gboolean
gst_uvc_h264_device_provider_start (GstDeviceProvider * provider)
{
  GstBus *bus;
  GstUvcH264DeviceProvider *self = (GstUvcH264DeviceProvider *) provider;
  GList *tmp, *devs = gst_uvc_h264_device_provider_probe (provider);
  if (!self->v4l2)
    return TRUE;

  bus = gst_device_provider_get_bus (self->v4l2);
  gst_bus_enable_sync_message_emission (bus);
  self->bus_watch_id = g_signal_connect (bus, "sync-message",
      G_CALLBACK (_bus_message_cb), self);
  gst_object_unref (bus);

  for (tmp = devs; tmp; tmp = tmp->next)
    gst_device_provider_device_add (provider, tmp->data);
  g_list_free (devs);

  return TRUE;
}

static void
gst_uvc_h264_device_provider_stop (GstDeviceProvider * provider)
{
  GstBus *bus;
  GstUvcH264DeviceProvider *self = (GstUvcH264DeviceProvider *) provider;

  if (!self->v4l2)
    return;

  if (self->usb_ctx)
    libusb_exit (self->usb_ctx);
  self->usb_ctx = NULL;

  bus = gst_device_provider_get_bus (self->v4l2);
  g_signal_handler_disconnect (bus, self->bus_watch_id);
  self->bus_watch_id = 0;
  gst_clear_object (&self->v4l2);
  gst_clear_object (&bus);

  return;
}

static void
gst_uvc_h264_device_provider_class_init (GstUvcH264DeviceProviderClass * klass)
{
  GstDeviceProviderClass *dm_class = GST_DEVICE_PROVIDER_CLASS (klass);

  dm_class->probe = gst_uvc_h264_device_provider_probe;
  dm_class->start = gst_uvc_h264_device_provider_start;
  dm_class->stop = gst_uvc_h264_device_provider_stop;

  gst_device_provider_class_set_static_metadata (dm_class,
      "UVC H.264 Device Provider", "Video/CameraSource",
      "List and provides UVC H.264 source devices",
      "Thibault Saunier <tsaunier@igalia.com>");
}

static void
gst_uvc_h264_device_provider_init (GstUvcH264DeviceProvider * self)
{
}
