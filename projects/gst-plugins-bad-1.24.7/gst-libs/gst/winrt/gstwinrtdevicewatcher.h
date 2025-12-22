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

#ifndef __GST_WINRT_DEVICE_WATCHER_H__
#define __GST_WINRT_DEVICE_WATCHER_H__

#include <gst/gst.h>
#include <gst/winrt/winrt-prelude.h>
#include <windows.devices.enumeration.h>

G_BEGIN_DECLS

#define GST_TYPE_WINRT_DEVICE_WATCHER            (gst_winrt_device_watcher_get_type())
#define GST_WINRT_DEVICE_WATCHER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_WINRT_DEVICE_WATCHER, GstWinRTDeviceWatcher))
#define GST_WINRT_DEVICE_WATCHER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_WINRT_DEVICE_WATCHER, GstWinRTDeviceWatcherClass))
#define GST_IS_WINRT_DEVICE_WATCHER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_WINRT_DEVICE_WATCHER))
#define GST_IS_WINRT_DEVICE_WATCHER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_WINRT_DEVICE_WATCHER))
#define GST_WINRT_DEVICE_WATCHER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GST_TYPE_WINRT_DEVICE_WATCHER, GstWinRTDeviceWatcherClass))
#define GST_WINRT_DEVICE_WATCHER_CAST(obj)       ((GstWinRTDeviceWatcher *)obj)

typedef struct _GstWinRTDeviceWatcher GstWinRTDeviceWatcher;
typedef struct _GstWinRTDeviceWatcherClass GstWinRTDeviceWatcherClass;
typedef struct _GstWinRTDeviceWatcherPrivate GstWinRTDeviceWatcherPrivate;

/* ABI::Windows::Devices::Enumeration::DeviceClass */
#define GST_TYPE_WINRT_DEVICE_CLASS (gst_winrt_device_class_get_type ())
typedef enum
{
  GST_WINRT_DEVICE_CLASS_ALL = 0,
  GST_WINRT_DEVICE_CLASS_AUDIO_CAPTURE = 1,
  GST_WINRT_DEVICE_CLASS_AUDIO_RENDER = 2,
  GST_WINRT_DEVICE_CLASS_PORTABLE_STORAGE_DEVICE = 3,
  GST_WINRT_DEVICE_CLASS_VIDEO_CAPTURE = 4,
} GstWinRTDeviceClass;

typedef struct
{
  /**
   * GstWinRTDeviceWatcherCallbacks::added:
   * @watcher: a #GstWinRTDeviceWatcher
   * @info: (transfer none): a IDeviceInformation interface handle
   * @user_data: a user_data
   *
   * Called when a device is added to the collection enumerated by the DeviceWatcher
   */
  void (*added)                   (GstWinRTDeviceWatcher * watcher,
                                   __x_ABI_CWindows_CDevices_CEnumeration_CIDeviceInformation * info,
                                   gpointer user_data);

  /**
   * GstWinRTDeviceWatcherCallbacks::updated:
   * @watcher: a #GstWinRTDeviceWatcher
   * @info_update: (transfer none): a IDeviceInformationUpdate interface handle
   * @user_data: a user_data
   *
   * Called when a device is updated in the collection of enumerated devices
   */
  void (*updated)                 (GstWinRTDeviceWatcher * watcher,
                                   __x_ABI_CWindows_CDevices_CEnumeration_CIDeviceInformationUpdate * info_update,
                                   gpointer user_data);

  /**
   * GstWinRTDeviceWatcherCallbacks::removed:
   * @watcher: a #GstWinRTDeviceWatcher
   * @info_update: (transfer none): a IDeviceInformationUpdate interface handle
   * @user_data: a user_data
   *
   * Called when a device is removed from the collection of enumerated devices
   */
  void (*removed)                 (GstWinRTDeviceWatcher * watcher,
                                   __x_ABI_CWindows_CDevices_CEnumeration_CIDeviceInformationUpdate * info_update,
                                   gpointer user_data);

  /**
   * GstWinRTDeviceWatcherCallbacks::removed:
   * @watcher: a #GstWinRTDeviceWatcher
   * @user_data: a user_data
   *
   * Called when the enumeration of devices completes
   */
  void (*enumeration_completed)   (GstWinRTDeviceWatcher * watcher,
                                   gpointer user_data);
} GstWinRTDeviceWatcherCallbacks;

struct _GstWinRTDeviceWatcher
{
  GstObject parent;

  GstWinRTDeviceWatcherPrivate *priv;

  gpointer _gst_reserved[GST_PADDING];
};

struct _GstWinRTDeviceWatcherClass
{
  GstObjectClass parent_class;

  gpointer _gst_reserved[GST_PADDING_LARGE];
};

GST_WINRT_API
GType                   gst_winrt_device_class_get_type   (void);

GST_WINRT_API
GType                   gst_winrt_device_watcher_get_type (void);

GST_WINRT_API
GstWinRTDeviceWatcher * gst_winrt_device_watcher_new      (GstWinRTDeviceClass device_class,
                                                           const GstWinRTDeviceWatcherCallbacks * callbacks,
                                                           gpointer user_data);

GST_WINRT_API
gboolean                gst_winrt_device_watcher_start    (GstWinRTDeviceWatcher * watcher);

GST_WINRT_API
void                    gst_winrt_device_watcher_stop     (GstWinRTDeviceWatcher * watcher);

G_END_DECLS

#endif /* __GST_WINRT_DEVICE_WATCHER_H__ */
