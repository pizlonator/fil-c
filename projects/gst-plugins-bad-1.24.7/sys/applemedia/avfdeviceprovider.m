/* GStreamer
 * Copyright (C) 2019 Josh Matthews <josh@joshmatthews.net>
 *
 * avfdeviceprovider.c: AVF device probing and monitoring
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

#import <AVFoundation/AVFoundation.h>
#include "avfvideosrc.h"
#include "avfdeviceprovider.h"

#include <string.h>

#include <gst/gst.h>

static GstDevice *gst_avf_device_new (const gchar * device_name, int device_index,
                                      GstCaps * caps, GstAvfDeviceType type,
                                      GstStructure *props);
G_DEFINE_TYPE (GstAVFDeviceProvider, gst_avf_device_provider,
               GST_TYPE_DEVICE_PROVIDER);

static GList *gst_avf_device_provider_probe (GstDeviceProvider * provider);

static void
gst_avf_device_provider_class_init (GstAVFDeviceProviderClass * klass)
{
  GstDeviceProviderClass *dm_class = GST_DEVICE_PROVIDER_CLASS (klass);

  // TODO: Add start/stop callbacks to receive device notifications.
  // https://gitlab.freedesktop.org/gstreamer/gst-plugins-bad/issues/886
  dm_class->probe = gst_avf_device_provider_probe;

  gst_avf_video_src_debug_init ();

  gst_device_provider_class_set_static_metadata (dm_class,
                                                 "AVF Device Provider", "Source/Video",
                                                 "List and provide AVF source devices",
                                                 "Josh Matthews <josh@joshmatthews.net>");
}

static void
gst_avf_device_provider_init (GstAVFDeviceProvider * self)
{
}

static GstStructure *
gst_av_capture_device_get_props (AVCaptureDevice *device)
{
  char *unique_id, *model_id;
  GstStructure *props = gst_structure_new_empty ("avf-proplist");

  unique_id = g_strdup ([[device uniqueID] UTF8String]);
  model_id = g_strdup ([[device modelID] UTF8String]);

  gst_structure_set (props,
    "device.api", G_TYPE_STRING, "avf",
    "avf.unique_id", G_TYPE_STRING, unique_id,
    "avf.model_id", G_TYPE_STRING, model_id,
    "avf.has_flash", G_TYPE_BOOLEAN, [device hasFlash],
    "avf.has_torch", G_TYPE_BOOLEAN, [device hasTorch],
  NULL);

  g_free (unique_id);
  g_free (model_id);

#if !HAVE_IOS
  char *manufacturer = g_strdup ([[device manufacturer] UTF8String]);
  gst_structure_set (props,
    "avf.manufacturer", G_TYPE_STRING, manufacturer,
  NULL);

  g_free (manufacturer);
#endif

  return props;
}

static GList *
gst_avf_device_provider_probe (GstDeviceProvider * provider)
{
  GList *result;

  result = NULL;
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  NSArray *devices = [AVCaptureDevice devicesWithMediaType:AVMediaTypeVideo];
G_GNUC_END_IGNORE_DEPRECATIONS
  AVCaptureVideoDataOutput *output = [[AVCaptureVideoDataOutput alloc] init];
  for (int i = 0; i < [devices count]; i++) {
    AVCaptureDevice *device = [devices objectAtIndex:i];
    g_assert (device != nil);

    GstCaps *caps = gst_av_capture_device_get_caps (device, output, GST_AVF_VIDEO_SOURCE_ORIENTATION_DEFAULT);
    GstStructure *props = gst_av_capture_device_get_props (device);
    const gchar *deviceName = [[device localizedName] UTF8String];
    GstDevice *gst_device = gst_avf_device_new (deviceName, i, caps, GST_AVF_DEVICE_TYPE_VIDEO_SOURCE, props);

    result = g_list_prepend (result, gst_object_ref_sink (gst_device));

    gst_structure_free (props);
  }

  result = g_list_reverse (result);

  return result;
}

enum
{
  PROP_DEVICE_INDEX = 1
};

G_DEFINE_TYPE (GstAvfDevice, gst_avf_device, GST_TYPE_DEVICE);

static GstElement *gst_avf_device_create_element (GstDevice * device,
                                                 const gchar * name);
static gboolean gst_avf_device_reconfigure_element (GstDevice * device,
                                                   GstElement * element);

static void gst_avf_device_get_property (GObject * object, guint prop_id,
                                         GValue * value, GParamSpec * pspec);
static void gst_avf_device_set_property (GObject * object, guint prop_id,
                                         const GValue * value, GParamSpec * pspec);

static void
gst_avf_device_class_init (GstAvfDeviceClass * klass)
{
  GstDeviceClass *dev_class = GST_DEVICE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  dev_class->create_element = gst_avf_device_create_element;
  dev_class->reconfigure_element = gst_avf_device_reconfigure_element;

  object_class->get_property = gst_avf_device_get_property;
  object_class->set_property = gst_avf_device_set_property;

  g_object_class_install_property (object_class, PROP_DEVICE_INDEX,
      g_param_spec_int ("device-index", "Device Index",
          "The zero-based device index", -1, G_MAXINT, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT_ONLY));
}

static void
gst_avf_device_init (GstAvfDevice * device)
{
}

static GstElement *
gst_avf_device_create_element (GstDevice * device, const gchar * name)
{
  GstAvfDevice *avf_dev = GST_AVF_DEVICE (device);
  GstElement *elem;

  elem = gst_element_factory_make (avf_dev->element, name);
  g_object_set (elem, "device-index", avf_dev->device_index, NULL);

  return elem;
}

static gboolean
gst_avf_device_reconfigure_element (GstDevice * device, GstElement * element)
{
  GstAvfDevice *avf_dev = GST_AVF_DEVICE (device);

  if (!strcmp (avf_dev->element, "avfvideosrc") && GST_IS_AVF_VIDEO_SRC (element)) {
    g_object_set (element, "device-index", avf_dev->device_index, NULL);
    return TRUE;
  }

  return FALSE;
}

static void
gst_avf_device_get_property (GObject * object, guint prop_id,
                             GValue * value, GParamSpec * pspec)
{
  GstAvfDevice *device;

  device = GST_AVF_DEVICE_CAST (object);

  switch (prop_id) {
    case PROP_DEVICE_INDEX:
      g_value_set_int (value, device->device_index);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_avf_device_set_property (GObject * object, guint prop_id,
                             const GValue * value, GParamSpec * pspec)
{
  GstAvfDevice *device;

  device = GST_AVF_DEVICE_CAST (object);

  switch (prop_id) {
    case PROP_DEVICE_INDEX:
      device->device_index = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstDevice *
gst_avf_device_new (const gchar * device_name, int device_index, GstCaps * caps, GstAvfDeviceType type, GstStructure *props)
{
  GstAvfDevice *gstdev;
  const gchar *element = NULL;
  const gchar *klass = NULL;

  g_return_val_if_fail (device_name, NULL);
  g_return_val_if_fail (caps, NULL);


  switch (type) {
    case GST_AVF_DEVICE_TYPE_VIDEO_SOURCE:
      element = "avfvideosrc";
      klass = "Video/Source";
      break;
    default:
      g_assert_not_reached ();
      break;
  }


  gstdev = g_object_new (GST_TYPE_AVF_DEVICE,
                         "display-name", device_name, "caps", caps, "device-class", klass,
                         "device-index", device_index, "properties", props, NULL);

  gstdev->type = type;
  gstdev->element = element;

  return GST_DEVICE (gstdev);
}
