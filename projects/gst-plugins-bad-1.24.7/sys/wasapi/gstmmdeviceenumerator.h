/*
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

#ifndef __GST_MM_DEVICE_ENUMERATOR_H__
#define __GST_MM_DEVICE_ENUMERATOR_H__

#include <gst/gst.h>
#include <mmdeviceapi.h>

G_BEGIN_DECLS

#define GST_TYPE_MM_DEVICE_ENUMERATOR (gst_mm_device_enumerator_get_type ())
G_DECLARE_FINAL_TYPE (GstMMDeviceEnumerator, gst_mm_device_enumerator,
    GST, MM_DEVICE_ENUMERATOR, GstObject);

typedef struct
{
  HRESULT (*device_state_changed)    (GstMMDeviceEnumerator * enumerator,
                                      LPCWSTR device_id,
                                      DWORD new_state,
                                      gpointer user_data);

  HRESULT (*device_added)            (GstMMDeviceEnumerator * enumerator,
                                      LPCWSTR device_id,
                                      gpointer user_data);

  HRESULT (*device_removed)          (GstMMDeviceEnumerator * provider,
                                      LPCWSTR device_id,
                                      gpointer user_data);

  HRESULT (*default_device_changed)  (GstMMDeviceEnumerator * provider,
                                      EDataFlow flow,
                                      ERole role,
                                      LPCWSTR default_device_id,
                                      gpointer user_data);

  HRESULT (*property_value_changed)  (GstMMDeviceEnumerator * provider,
                                      LPCWSTR device_id,
                                      const PROPERTYKEY key,
                                      gpointer user_data);
} GstMMNotificationClientCallbacks;

GstMMDeviceEnumerator * gst_mm_device_enumerator_new (void);

IMMDeviceEnumerator * gst_mm_device_enumerator_get_handle (GstMMDeviceEnumerator * enumerator);

gboolean gst_mm_device_enumerator_set_notification_callback (GstMMDeviceEnumerator * enumerator,
                                                             GstMMNotificationClientCallbacks * callbacks,
                                                             gpointer user_data);

G_END_DECLS

#endif /* __GST_MM_DEVICE_ENUMERATOR_H__ */
