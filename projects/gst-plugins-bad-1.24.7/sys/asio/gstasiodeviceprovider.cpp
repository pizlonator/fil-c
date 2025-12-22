/* GStreamer
 * Copyright (C) 2021 Seungha Yang <seungha@centricular.com>
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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstasiodeviceprovider.h"
#include "gstasioutils.h"
#include "gstasioobject.h"
#include <atlconv.h>

enum
{
  PROP_0,
  PROP_DEVICE_CLSID,
};

struct _GstAsioDevice
{
  GstDevice parent;

  gchar *device_clsid;
  const gchar *factory_name;
};

G_DEFINE_TYPE (GstAsioDevice, gst_asio_device, GST_TYPE_DEVICE);

static void gst_asio_device_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);
static void gst_asio_device_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_asio_device_finalize (GObject * object);
static GstElement *gst_asio_device_create_element (GstDevice * device,
    const gchar * name);

static void
gst_asio_device_class_init (GstAsioDeviceClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstDeviceClass *dev_class = GST_DEVICE_CLASS (klass);

  dev_class->create_element = gst_asio_device_create_element;

  gobject_class->get_property = gst_asio_device_get_property;
  gobject_class->set_property = gst_asio_device_set_property;
  gobject_class->finalize = gst_asio_device_finalize;

  g_object_class_install_property (gobject_class, PROP_DEVICE_CLSID,
      g_param_spec_string ("device-clsid", "Device CLSID",
          "ASIO device CLSID as string including curly brackets", NULL,
          (GParamFlags) (G_PARAM_READWRITE |
              G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS)));
}

static void
gst_asio_device_init (GstAsioDevice * self)
{
}

static void
gst_asio_device_finalize (GObject * object)
{
  GstAsioDevice *self = GST_ASIO_DEVICE (object);

  g_free (self->device_clsid);

  G_OBJECT_CLASS (gst_asio_device_parent_class)->finalize (object);
}

static GstElement *
gst_asio_device_create_element (GstDevice * device, const gchar * name)
{
  GstAsioDevice *self = GST_ASIO_DEVICE (device);
  GstElement *elem;

  elem = gst_element_factory_make (self->factory_name, name);

  g_object_set (elem, "device-clsid", self->device_clsid, NULL);

  return elem;
}

static void
gst_asio_device_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstAsioDevice *self = GST_ASIO_DEVICE (object);

  switch (prop_id) {
    case PROP_DEVICE_CLSID:
      g_value_set_string (value, self->device_clsid);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_asio_device_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstAsioDevice *self = GST_ASIO_DEVICE (object);

  switch (prop_id) {
    case PROP_DEVICE_CLSID:
      g_free (self->device_clsid);
      self->device_clsid = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

struct _GstAsioDeviceProvider
{
  GstDeviceProvider parent;
};

G_DEFINE_TYPE (GstAsioDeviceProvider, gst_asio_device_provider,
    GST_TYPE_DEVICE_PROVIDER);

static GList *gst_asio_device_provider_probe (GstDeviceProvider * provider);

static void
gst_asio_device_provider_class_init (GstAsioDeviceProviderClass * klass)
{
  GstDeviceProviderClass *provider_class = GST_DEVICE_PROVIDER_CLASS (klass);

  provider_class->probe = GST_DEBUG_FUNCPTR (gst_asio_device_provider_probe);

  gst_device_provider_class_set_static_metadata (provider_class,
      "ASIO Device Provider",
      "Source/Sink/Audio", "List ASIO source and sink devices",
      "Seungha Yang <seungha@centricular.com>");
}

static void
gst_asio_device_provider_init (GstAsioDeviceProvider * provider)
{
}

static void
gst_asio_device_provider_probe_internal (GstAsioDeviceProvider * self,
    gboolean is_src, GList * asio_device_list, GList ** devices)
{
  const gchar *device_class, *factory_name;
  GList *iter;

  USES_CONVERSION;

  if (is_src) {
    device_class = "Audio/Source";
    factory_name = "asiosrc";
  } else {
    device_class = "Audio/Sink";
    factory_name = "asiosink";
  }

  for (iter = asio_device_list; iter; iter = g_list_next (iter)) {
    GstDevice *device;
    GstAsioDeviceInfo *info = (GstAsioDeviceInfo *) iter->data;
    GstAsioObject *obj;
    GstCaps *caps = nullptr;
    GstStructure *props = nullptr;
    long max_in_ch = 0;
    long max_out_ch = 0;
    HRESULT hr;
    LPOLESTR clsid_str = nullptr;
    glong min_buf_size = 0;
    glong max_buf_size = 0;
    glong preferred_buf_size = 0;
    glong buf_size_granularity = 0;

    obj = gst_asio_object_new (info, FALSE);
    if (!obj)
      continue;

    if (!gst_asio_object_get_max_num_channels (obj, &max_in_ch, &max_out_ch))
      goto done;

    if (is_src && max_in_ch <= 0)
      goto done;
    else if (!is_src && max_out_ch <= 0)
      goto done;

    if (is_src) {
      caps = gst_asio_object_get_caps (obj,
          GST_ASIO_DEVICE_CLASS_CAPTURE, 1, max_in_ch);
    } else {
      caps = gst_asio_object_get_caps (obj,
          GST_ASIO_DEVICE_CLASS_RENDER, 1, max_out_ch);
    }
    if (!caps)
      goto done;

    hr = StringFromIID (info->clsid, &clsid_str);
    if (FAILED (hr))
      goto done;

    if (!gst_asio_object_get_buffer_size (obj, &min_buf_size, &max_buf_size,
            &preferred_buf_size, &buf_size_granularity))
      goto done;

    props = gst_structure_new ("asio-proplist",
        "device.api", G_TYPE_STRING, "asio",
        "device.clsid", G_TYPE_STRING, OLE2A (clsid_str),
        "asio.device.description", G_TYPE_STRING, info->driver_desc,
        "asio.device.min-buf-size", G_TYPE_LONG, min_buf_size,
        "asio.device.max-buf-size", G_TYPE_LONG, max_buf_size,
        "asio.device.preferred-buf-size", G_TYPE_LONG, preferred_buf_size,
        "asio.device.buf-size-granularity", G_TYPE_LONG, buf_size_granularity,
        nullptr);

    device = (GstDevice *) g_object_new (GST_TYPE_ASIO_DEVICE,
        "device-clsid", OLE2A (clsid_str),
        "display-name", info->driver_desc, "caps", caps,
        "device-class", device_class, "properties", props, nullptr);
    GST_ASIO_DEVICE (device)->factory_name = factory_name;

    *devices = g_list_append (*devices, device);

  done:
    gst_clear_caps (&caps);
    gst_clear_object (&obj);
    if (props)
      gst_structure_free (props);
  }

  return;
}

static GList *
gst_asio_device_provider_probe (GstDeviceProvider * provider)
{
  GstAsioDeviceProvider *self = GST_ASIO_DEVICE_PROVIDER (provider);
  GList *devices = nullptr;
  guint num_device;
  GList *asio_device_list = nullptr;

  num_device = gst_asio_enum (&asio_device_list);

  if (num_device == 0)
    return nullptr;

  gst_asio_device_provider_probe_internal (self,
      TRUE, asio_device_list, &devices);
  gst_asio_device_provider_probe_internal (self,
      FALSE, asio_device_list, &devices);

  g_list_free_full (asio_device_list,
      (GDestroyNotify) gst_asio_device_info_free);

  return devices;
}
