/* GStreamer
 * Copyright (C) 2018 Joshua M. Doe <oss@nvl.army.mil>
 *
 * dshowdeviceprovider.cpp: DirectShow device probing
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstdshow.h"
#include "gstdshowvideosrc.h"
#include "dshowdeviceprovider.h"

#include <gst/gst.h>

#include <windows.h>

GST_DEBUG_CATEGORY_EXTERN (dshowsrcwrapper_debug);
#define GST_CAT_DEFAULT dshowsrcwrapper_debug


static GstDevice *gst_dshow_device_new (guint id,
    const gchar * device_name, GstCaps * caps, const gchar * device_path,
    GstDshowDeviceType type);

G_DEFINE_TYPE (GstDshowDeviceProvider, gst_dshow_device_provider,
    GST_TYPE_DEVICE_PROVIDER);

static void gst_dshow_device_provider_dispose (GObject * gobject);

static GList *gst_dshow_device_provider_probe (GstDeviceProvider * provider);
static gboolean gst_dshow_device_provider_start (GstDeviceProvider * provider);
static void gst_dshow_device_provider_stop (GstDeviceProvider * provider);

static void
gst_dshow_device_provider_class_init (GstDshowDeviceProviderClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstDeviceProviderClass *dm_class = GST_DEVICE_PROVIDER_CLASS (klass);

  gobject_class->dispose = gst_dshow_device_provider_dispose;

  dm_class->probe = gst_dshow_device_provider_probe;
  dm_class->start = gst_dshow_device_provider_start;
  dm_class->stop = gst_dshow_device_provider_stop;

  gst_device_provider_class_set_static_metadata (dm_class,
      "DirectShow Device Provider", "Source/Audio/Video",
      "List and provide DirectShow source devices",
      "Руслан Ижбулатов <lrn1986@gmail.com>");
}

static void
gst_dshow_device_provider_init (GstDshowDeviceProvider * self)
{
  CoInitializeEx (NULL, COINIT_MULTITHREADED);
}

static void
gst_dshow_device_provider_dispose (GObject * gobject)
{
  CoUninitialize ();
}

static GstDevice *
new_video_source (const DshowDeviceEntry * info)
{
  g_assert (info && info->device != NULL);

  return gst_dshow_device_new (info->device_index, info->device_name,
      info->caps, info->device, GST_DSHOW_DEVICE_TYPE_VIDEO_SOURCE);
}

static GList *
gst_dshow_device_provider_probe (GstDeviceProvider * provider)
{
  /*GstDshowDeviceProvider *self = GST_DSHOW_DEVICE_PROVIDER (provider); */
  GList *devices, *cur;
  GList *result;

  result = NULL;

  devices = gst_dshow_enumerate_devices (&CLSID_VideoInputDeviceCategory, TRUE);
  if (devices == NULL)
    return result;

  /* TODO: try and sort camera first like ksvideosrc? */

  for (cur = devices; cur != NULL; cur = cur->next) {
    GstDevice *source;
    DshowDeviceEntry *entry = (DshowDeviceEntry *) cur->data;

    source = new_video_source (entry);
    if (source)
      result = g_list_prepend (result, gst_object_ref_sink (source));
  }

  result = g_list_reverse (result);

  gst_dshow_device_list_free (devices);

  return result;
}

static gboolean
gst_dshow_device_provider_start (GstDeviceProvider * provider)
{
  GList *devs;
  GList *dev;
  GstDshowDeviceProvider *self = GST_DSHOW_DEVICE_PROVIDER (provider);

  devs = gst_dshow_device_provider_probe (provider);
  for (dev = devs; dev; dev = dev->next) {
    if (dev->data)
      gst_device_provider_device_add (provider, (GstDevice *) dev->data);
  }
  g_list_free_full (devs, gst_object_unref);

  return TRUE;
}

static void
gst_dshow_device_provider_stop (GstDeviceProvider * provider)
{

}

enum
{
  PROP_0,
  PROP_DEVICE,
  PROP_DEVICE_NAME,
  PROP_DEVICE_INDEX
};

G_DEFINE_TYPE (GstDshowDevice, gst_dshow_device, GST_TYPE_DEVICE);

static void gst_dshow_device_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_dshow_device_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_dshow_device_finalize (GObject * object);
static GstElement *gst_dshow_device_create_element (GstDevice * device,
    const gchar * name);

static void
gst_dshow_device_class_init (GstDshowDeviceClass * klass)
{
  GstDeviceClass *dev_class = GST_DEVICE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  dev_class->create_element = gst_dshow_device_create_element;

  object_class->get_property = gst_dshow_device_get_property;
  object_class->set_property = gst_dshow_device_set_property;
  object_class->finalize = gst_dshow_device_finalize;

  g_object_class_install_property (object_class, PROP_DEVICE,
      g_param_spec_string ("device", "Device",
          "DirectShow device path (@..classID/name)", "",
          (GParamFlags) (G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE |
              G_PARAM_CONSTRUCT_ONLY)));

  g_object_class_install_property (object_class, PROP_DEVICE_NAME,
      g_param_spec_string ("device-name", "Device name",
          "Human-readable name of the audio/video device", "",
          (GParamFlags) (G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE |
              G_PARAM_CONSTRUCT_ONLY)));

  g_object_class_install_property (object_class, PROP_DEVICE_INDEX,
      g_param_spec_int ("device-index", "Device index",
          "Index of the enumerated audio/video device", 0, G_MAXINT, 0,
          (GParamFlags) (G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE |
              G_PARAM_CONSTRUCT_ONLY)));
}

static void
gst_dshow_device_init (GstDshowDevice * device)
{
}

static void
gst_dshow_device_finalize (GObject * object)
{
  GstDshowDevice *device = GST_DSHOW_DEVICE (object);

  g_free (device->device);
  g_free (device->device_name);

  G_OBJECT_CLASS (gst_dshow_device_parent_class)->finalize (object);
}

static GstElement *
gst_dshow_device_create_element (GstDevice * device, const gchar * name)
{
  GstDshowDevice *dev = GST_DSHOW_DEVICE (device);
  GstElement *elem;

  elem = gst_element_factory_make (dev->element, name);
  g_object_set (elem, "device", dev->device, NULL);
  g_object_set (elem, "device-name", dev->device_name, NULL);

  return elem;
}


static GstDevice *
gst_dshow_device_new (guint device_index, const gchar * device_name,
    GstCaps * caps, const gchar * device_path, GstDshowDeviceType type)
{
  GstDshowDevice *gstdev;
  const gchar *element = NULL;
  const gchar *klass = NULL;

  g_return_val_if_fail (device_name, NULL);
  g_return_val_if_fail (device_path, NULL);
  g_return_val_if_fail (caps, NULL);

  switch (type) {
    case GST_DSHOW_DEVICE_TYPE_VIDEO_SOURCE:
      element = "dshowvideosrc";
      klass = "Video/Source";
      break;
    case GST_DSHOW_DEVICE_TYPE_AUDIO_SOURCE:
      element = "dshowaudiosrc";
      klass = "Audio/Source";
      break;
    default:
      g_assert_not_reached ();
      break;
  }

  /* set props of parent device class */
  gstdev = (GstDshowDevice *) g_object_new (GST_TYPE_DSHOW_DEVICE,
      "display-name", device_name, "caps", caps, "device-class", klass, NULL);

  gstdev->type = type;
  gstdev->device = g_strdup (device_path);
  gstdev->device_name = g_strdup (device_name);
  gstdev->device_index = device_index;
  gstdev->element = element;

  return GST_DEVICE (gstdev);
}


static void
gst_dshow_device_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstDshowDevice *device;

  device = GST_DSHOW_DEVICE_CAST (object);

  switch (prop_id) {
    case PROP_DEVICE:
      g_value_set_string (value, device->device);
      break;
    case PROP_DEVICE_NAME:
      g_value_set_string (value, device->device_name);
      break;
    case PROP_DEVICE_INDEX:
      g_value_set_int (value, device->device_index);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}


static void
gst_dshow_device_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstDshowDevice *device;

  device = GST_DSHOW_DEVICE_CAST (object);

  switch (prop_id) {
    case PROP_DEVICE:
      device->device = g_value_dup_string (value);
      break;
    case PROP_DEVICE_NAME:
      device->device_name = g_value_dup_string (value);
      break;
    case PROP_DEVICE_INDEX:
      device->device_index = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}
