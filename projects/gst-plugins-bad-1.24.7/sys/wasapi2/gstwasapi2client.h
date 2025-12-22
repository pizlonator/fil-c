/*
 * Copyright (C) 2020 Seungha Yang <seungha@centricular.com>
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

#ifndef __GST_WASAPI2_CLIENT_H__
#define __GST_WASAPI2_CLIENT_H__

#include <gst/gst.h>
#include <gst/audio/audio.h>
#include "gstwasapi2util.h"

G_BEGIN_DECLS

typedef enum
{
  GST_WASAPI2_CLIENT_DEVICE_CLASS_CAPTURE = 0,
  GST_WASAPI2_CLIENT_DEVICE_CLASS_RENDER,
  GST_WASAPI2_CLIENT_DEVICE_CLASS_LOOPBACK_CAPTURE,
  GST_WASAPI2_CLIENT_DEVICE_CLASS_INCLUDE_PROCESS_LOOPBACK_CAPTURE,
  GST_WASAPI2_CLIENT_DEVICE_CLASS_EXCLUDE_PROCESS_LOOPBACK_CAPTURE,
} GstWasapi2ClientDeviceClass;

typedef enum
{
  GST_WASAPI2_OK,
  GST_WASAPI2_DEVICE_NOT_FOUND,
  GST_WASAPI2_ACTIVATION_FAILED,
} GstWasapi2Result;

static inline gboolean
gst_wasapi2_device_class_is_loopback (GstWasapi2ClientDeviceClass device_class)
{
  switch (device_class) {
    case GST_WASAPI2_CLIENT_DEVICE_CLASS_LOOPBACK_CAPTURE:
      return TRUE;
    default:
      break;
  }

  return FALSE;
}

static inline gboolean
gst_wasapi2_device_class_is_process_loopback (GstWasapi2ClientDeviceClass device_class)
{
  switch (device_class) {
    case GST_WASAPI2_CLIENT_DEVICE_CLASS_INCLUDE_PROCESS_LOOPBACK_CAPTURE:
    case GST_WASAPI2_CLIENT_DEVICE_CLASS_EXCLUDE_PROCESS_LOOPBACK_CAPTURE:
      return TRUE;
    default:
      break;
  }

  return FALSE;
}

#define GST_TYPE_WASAPI2_CLIENT_DEVICE_CLASS (gst_wasapi2_client_device_class_get_type())
GType gst_wasapi2_client_device_class_get_type (void);

#define GST_TYPE_WASAPI2_CLIENT (gst_wasapi2_client_get_type())
G_DECLARE_FINAL_TYPE (GstWasapi2Client,
    gst_wasapi2_client, GST, WASAPI2_CLIENT, GstObject);

GstWasapi2Client * gst_wasapi2_client_new (GstWasapi2ClientDeviceClass device_class,
                                           gint device_index,
                                           const gchar * device_id,
                                           guint target_pid,
                                           gpointer dispatcher);

gboolean           gst_wasapi2_client_ensure_activation (GstWasapi2Client * client);

IAudioClient *     gst_wasapi2_client_get_handle (GstWasapi2Client * client);

gboolean           gst_wasapi2_client_is_endpoint_muted (GstWasapi2Client * client);

GstCaps *          gst_wasapi2_client_get_caps (GstWasapi2Client * client);

GstWasapi2Result   gst_wasapi2_client_enumerate (GstWasapi2ClientDeviceClass device_class,
                                                 gint device_index,
                                                 GstWasapi2Client ** client);

G_END_DECLS

#endif /* __GST_WASAPI2_CLIENT_H__ */
