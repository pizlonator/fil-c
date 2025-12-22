/* GStreamer
 * Copyright (C) 2020 Igalia, S.L.
 *     Author: Víctor Jáquez <vjaquez@igalia.com>
 * Copyright (C) 2023 Seungha Yang <seungha@centricular.com>
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

#include <wrl.h>
#include <dxgi.h>
#include <string>

/* *INDENT-OFF* */
using namespace Microsoft::WRL;

extern "C" {
GST_DEBUG_CATEGORY_EXTERN (gstva_debug);
#define GST_CAT_DEFAULT gstva_debug
}
/* *INDENT-ON* */

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

GList *
gst_va_device_find_devices (void)
{
  HRESULT hr;
  ComPtr < IDXGIFactory1 > factory;
  GList *ret = nullptr;
  guint idx = 0;

  hr = CreateDXGIFactory1 (IID_PPV_ARGS (&factory));
  if (FAILED (hr))
    return nullptr;

  for (guint i = 0;; i++) {
    ComPtr < IDXGIAdapter > adapter;
    LARGE_INTEGER val;
    DXGI_ADAPTER_DESC desc;
    std::string luid_str;
    GstVaDisplay *dpy;
    GstVaDevice *dev;

    hr = factory->EnumAdapters (i, &adapter);
    if (FAILED (hr))
      break;

    hr = adapter->GetDesc (&desc);
    if (FAILED (hr))
      continue;

    val.LowPart = desc.AdapterLuid.LowPart;
    val.HighPart = desc.AdapterLuid.HighPart;

    luid_str = std::to_string (val.QuadPart);
    dpy = gst_va_display_win32_new (luid_str.c_str ());
    if (!dpy)
      continue;

    dev = gst_va_device_new (dpy, luid_str.c_str (), idx);
    ret = g_list_append (ret, dev);
    idx++;
  }

  return ret;
}

void
gst_va_device_list_free (GList * devices)
{
  g_list_free_full (devices, (GDestroyNotify) gst_mini_object_unref);
}
