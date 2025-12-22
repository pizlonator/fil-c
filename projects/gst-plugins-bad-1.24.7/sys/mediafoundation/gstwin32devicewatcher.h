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

#ifndef __GST_WIN32_DEVICE_WATCHER_H__
#define __GST_WIN32_DEVICE_WATCHER_H__

#include <gst/gst.h>
#include <windows.h>

G_BEGIN_DECLS

#define GST_TYPE_WIN32_DEVICE_WATCHER (gst_win32_device_watcher_get_type())
G_DECLARE_FINAL_TYPE (GstWin32DeviceWatcher,
    gst_win32_device_watcher, GST, WIN32_DEVICE_WATCHER, GstObject);

typedef struct _GstWin32DeviceWatcherCallbacks
{
  void  (*device_changed)   (GstWin32DeviceWatcher * watcher,
                             WPARAM wparam,
                             LPARAM lparam,
                             gpointer user_data);

} GstWin32DeviceWatcherCallbacks;

GstWin32DeviceWatcher * gst_win32_device_watcher_new    (DWORD device_type,
                                                         const GUID * class_guid,
                                                         const GstWin32DeviceWatcherCallbacks * callbacks,
                                                         gpointer user_data);

gboolean                gst_win32_device_watcher_start  (GstWin32DeviceWatcher * watcher);

void                    gst_win32_device_watcher_stop   (GstWin32DeviceWatcher * watcher);

G_END_DECLS

#endif /* __GST_WIN32_DEVICE_WATCHER_H__ */
