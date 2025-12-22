/* GStreamer
 * Copyright (C) 2020 Igalia, S.L.
 *     Author: Víctor Jáquez <vjaquez@igalia.com>
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

#include "gstvadevice.h"

#ifdef HAVE_GUDEV
#include <gudev/gudev.h>
#endif

#define GST_CAT_DEFAULT gstva_debug
GST_DEBUG_CATEGORY_EXTERN (gstva_debug);

GST_DEFINE_MINI_OBJECT_TYPE (GstVaDevice, gst_va_device);

static void
gst_va_device_free (GstVaDevice * device)
{
  gst_clear_object (&device->display);
  g_free (device->render_device_path);
  g_free (device);
}

static GstVaDevice *
gst_va_device_new (GstVaDisplay * display, const gchar * render_device_path,
    gint index)
{
  GstVaDevice *device = g_new0 (GstVaDevice, 1);

  gst_mini_object_init (GST_MINI_OBJECT_CAST (device), 0, GST_TYPE_VA_DEVICE,
      NULL, NULL, (GstMiniObjectFreeFunction) gst_va_device_free);

  /* take ownership */
  device->display = display;
  device->render_device_path = g_strdup (render_device_path);
  device->index = index;

  return device;
}

#ifdef HAVE_GUDEV
static gint
compare_udev_path (gconstpointer a, gconstpointer b)
{
  GUdevDevice *pa = (GUdevDevice *) a;
  GUdevDevice *pb = (GUdevDevice *) b;

  return g_strcmp0 (g_udev_device_get_device_file (pa),
      g_udev_device_get_device_file (pb));
}

GList *
gst_va_device_find_devices (void)
{
  GUdevClient *client;
  GList *udev_devices, *dev;
  GQueue devices = G_QUEUE_INIT;
  gint i = 0;

  client = g_udev_client_new (NULL);
  udev_devices = g_udev_client_query_by_subsystem (client, "drm");

  if (!udev_devices) {
    g_object_unref (client);
    return NULL;
  }

  udev_devices = g_list_sort (udev_devices, compare_udev_path);

  for (dev = udev_devices; dev; dev = g_list_next (dev)) {
    GstVaDisplay *dpy;
    GUdevDevice *udev = (GUdevDevice *) dev->data;
    const gchar *path = g_udev_device_get_device_file (udev);
    const gchar *name = g_udev_device_get_name (udev);

    if (!path || !g_str_has_prefix (name, "renderD")) {
      GST_LOG ("Ignoring %s in %s", name, path);
      continue;
    }

    if (!(dpy = gst_va_display_drm_new_from_path (path)))
      continue;

    GST_INFO ("Found VA-API device: %s", path);
    g_queue_push_tail (&devices, gst_va_device_new (dpy, path, i++));
  }

  g_list_free_full (udev_devices, g_object_unref);
  g_object_unref (client);

  return devices.head;
}
#else
GList *
gst_va_device_find_devices (void)
{
  GstVaDisplay *dpy;
  GQueue devices = G_QUEUE_INIT;
  gchar path[64];
  guint i, j = 0;

  for (i = 0; i < 8; i++) {
    g_snprintf (path, sizeof (path), "/dev/dri/renderD%d", 128 + i);
    if (!g_file_test (path, G_FILE_TEST_EXISTS))
      continue;

    if (!(dpy = gst_va_display_drm_new_from_path (path)))
      continue;

    GST_INFO ("Found VA-API device: %s", path);
    g_queue_push_tail (&devices, gst_va_device_new (dpy, path, j++));
  }

  return devices.head;
}
#endif

void
gst_va_device_list_free (GList * devices)
{
  g_list_free_full (devices, (GDestroyNotify) gst_mini_object_unref);
}
