/*
 * GStreamer
 *
 * Copyright (C) 2012 Cisco Systems, Inc.
 *   Author: Youness Alaoui <youness.alaoui@collabora.co.uk>
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
#  include <config.h>
#endif

#include "uvc_h264.h"

GType
uvc_h264_slicemode_get_type (void)
{
  static GType type = 0;

  static const GEnumValue types[] = {
    {UVC_H264_SLICEMODE_IGNORED, "Ignored", "ignored"},
    {UVC_H264_SLICEMODE_BITSPERSLICE, "Bits per slice", "bits/slice"},
    {UVC_H264_SLICEMODE_MBSPERSLICE, "MBs per Slice", "MBs/slice"},
    {UVC_H264_SLICEMODE_SLICEPERFRAME, "Slice Per Frame", "slice/frame"},
    {0, NULL, NULL}
  };

  if (!type) {
    type = g_enum_register_static ("UvcH264SliceMode", types);
  }
  return type;
}

GType
uvc_h264_usagetype_get_type (void)
{
  static GType type = 0;

  static const GEnumValue types[] = {
    {UVC_H264_USAGETYPE_REALTIME, "Realtime (video conferencing)", "realtime"},
    {UVC_H264_USAGETYPE_BROADCAST, "Broadcast", "broadcast"},
    {UVC_H264_USAGETYPE_STORAGE, "Storage", "storage"},
    {UVC_H264_USAGETYPE_UCCONFIG_0, "UCConfig 0", "ucconfig0"},
    {UVC_H264_USAGETYPE_UCCONFIG_1, "UCConfig 1", "ucconfig1"},
    {UVC_H264_USAGETYPE_UCCONFIG_2Q, "UCConfig 2Q", "ucconfig2q"},
    {UVC_H264_USAGETYPE_UCCONFIG_2S, "UCConfig 2S", "ucconfig2s"},
    {UVC_H264_USAGETYPE_UCCONFIG_3, "UCConfig 3", "ucconfig3"},
    {0, NULL, NULL}
  };

  if (!type) {
    type = g_enum_register_static ("UvcH264UsageType", types);
  }
  return type;
}

GType
uvc_h264_ratecontrol_get_type (void)
{
  static GType type = 0;

  static const GEnumValue types[] = {
    {UVC_H264_RATECONTROL_CBR, "Constant bit rate", "cbr"},
    {UVC_H264_RATECONTROL_VBR, "Variable bit rate", "vbr"},
    {UVC_H264_RATECONTROL_CONST_QP, "Constant QP", "qp"},
    {0, NULL, NULL}
  };

  if (!type) {
    type = g_enum_register_static ("UvcH264RateControl", types);
  }
  return type;
}

GType
uvc_h264_streamformat_get_type (void)
{
  static GType type = 0;

  static const GEnumValue types[] = {
    {UVC_H264_STREAMFORMAT_ANNEXB, "Byte stream format (Annex B)", "byte"},
    {UVC_H264_STREAMFORMAT_NAL, "NAL stream format", "nal"},
    {0, NULL, NULL}
  };

  if (!type) {
    type = g_enum_register_static ("UvcH264StreamFormat", types);
  }
  return type;
}

GType
uvc_h264_entropy_get_type (void)
{
  static GType type = 0;

  static const GEnumValue types[] = {
    {UVC_H264_ENTROPY_CAVLC, "CAVLC", "cavlc"},
    {UVC_H264_ENTROPY_CABAC, "CABAC", "cabac"},
    {0, NULL, NULL}
  };

  if (!type) {
    type = g_enum_register_static ("UvcH264Entropy", types);
  }
  return type;
}

guint8
xu_get_id (GstObject * self, const gchar * devicename,
    libusb_context ** usb_ctx)
{
  static const __u8 guid[16] = GUID_UVCX_H264_XU;
  GUdevClient *client;
  GUdevDevice *udevice;
  GUdevDevice *parent;
  guint64 busnum;
  guint64 devnum;
  libusb_device **device_list = NULL;
  libusb_device *device = NULL;
  ssize_t cnt;
  int i, j, k;


  if (*usb_ctx == NULL)
    libusb_init (usb_ctx);

  client = g_udev_client_new (NULL);
  if (client) {
    udevice = g_udev_client_query_by_device_file (client, devicename);
    if (udevice) {
      parent = g_udev_device_get_parent_with_subsystem (udevice, "usb",
          "usb_device");
      if (parent) {
        busnum = g_udev_device_get_sysfs_attr_as_uint64 (parent, "busnum");
        devnum = g_udev_device_get_sysfs_attr_as_uint64 (parent, "devnum");

        cnt = libusb_get_device_list (*usb_ctx, &device_list);
        for (i = 0; i < cnt; i++) {
          if (busnum == libusb_get_bus_number (device_list[i]) &&
              devnum == libusb_get_device_address (device_list[i])) {
            device = libusb_ref_device (device_list[i]);
            break;
          }
        }
        libusb_free_device_list (device_list, 1);
        g_object_unref (parent);
      }
      g_object_unref (udevice);
    }
    g_object_unref (client);
  }

  if (device) {
    struct libusb_device_descriptor desc;

    if (libusb_get_device_descriptor (device, &desc) == 0) {
      for (i = 0; i < desc.bNumConfigurations; ++i) {
        struct libusb_config_descriptor *config = NULL;

        if (libusb_get_config_descriptor (device, i, &config) == 0) {
          for (j = 0; j < config->bNumInterfaces; j++) {
            for (k = 0; k < config->interface[j].num_altsetting; k++) {
              const struct libusb_interface_descriptor *interface;
              const guint8 *ptr = NULL;

              interface = &config->interface[j].altsetting[k];
              if (interface->bInterfaceClass != LIBUSB_CLASS_VIDEO ||
                  interface->bInterfaceSubClass != USB_VIDEO_CONTROL)
                continue;
              ptr = interface->extra;
              while (ptr - interface->extra +
                  sizeof (xu_descriptor) < interface->extra_length) {
                xu_descriptor *desc = (xu_descriptor *) ptr;

                GST_DEBUG_OBJECT (self, "Found VideoControl interface with "
                    "unit id %d : %" GUID_FORMAT, desc->bUnitID,
                    GUID_ARGS (desc->guidExtensionCode));
                if (desc->bDescriptorType == USB_VIDEO_CONTROL_INTERFACE &&
                    desc->bDescriptorSubType == USB_VIDEO_CONTROL_XU_TYPE &&
                    memcmp (desc->guidExtensionCode, guid, 16) == 0) {
                  guint8 unit_id = desc->bUnitID;

                  GST_DEBUG_OBJECT (self, "Found H264 XU unit : %d", unit_id);

                  libusb_free_config_descriptor (config);
                  libusb_unref_device (device);
                  return unit_id;
                }
                ptr += desc->bLength;
              }
            }
          }
          libusb_free_config_descriptor (config);
        }
      }
    }
    libusb_unref_device (device);
  }

  return 0;
}
