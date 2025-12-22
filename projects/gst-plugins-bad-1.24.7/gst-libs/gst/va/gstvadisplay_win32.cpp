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

/**
 * SECTION:gstvadisplaywin32
 * @title: GstVaDisplayWin32
 * @short_description: VADisplay from a Win32 Direct3D12 backend
 * @sources:
 * - gstvadisplay_win32.h
 *
 * This is a #GstVaDisplay subclass to instantiate for Win32 Direct3D12 backend.
 *
 * Since: 1.24
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef INITGUID
#include <initguid.h>
#endif

#include "gstvadisplay_win32.h"
#include <wrl.h>
#include <dxgi.h>
#include <va/va_win32.h>
#include <string>
#include <vector>

/* *INDENT-OFF* */
using namespace Microsoft::WRL;

extern "C" {
GST_DEBUG_CATEGORY_EXTERN (gst_va_display_debug);
#define GST_CAT_DEFAULT gst_va_display_debug
}
/* *INDENT-ON* */

/**
 * GstVaDisplayWin32:
 *
 * Since: 1.24
 */
struct _GstVaDisplayWin32
{
  GstVaDisplay parent;

  gchar *adapter_luid_str;
  gint64 adapter_luid;
  guint device_id;
  guint vendor_id;
  gchar *desc;
};

/**
 * GstVaDisplayWin32Class:
 *
 * Since: 1.24
 */
struct _GstVaDisplayWin32Class
{
  GstVaDisplayClass parent_class;
};

enum
{
  PROP_0,
  PROP_PATH,
  PROP_ADAPTER_LUID,
  PROP_DEVICE_ID,
  PROP_VENDOR_ID,
  PROP_DESC,
};

static void gst_va_display_win32_finalize (GObject * object);
static void gst_va_display_win32_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_va_display_win32_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static gpointer gst_va_display_win32_create_va_display (GstVaDisplay * display);

#define gst_va_display_win32_parent_class parent_class
G_DEFINE_TYPE (GstVaDisplayWin32, gst_va_display_win32, GST_TYPE_VA_DISPLAY);

static void
gst_va_display_win32_class_init (GstVaDisplayWin32Class * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GstVaDisplayClass *display_class = GST_VA_DISPLAY_CLASS (klass);
  GParamFlags construct_only_flags = (GParamFlags)
      (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT_ONLY);

  object_class->get_property = gst_va_display_win32_get_property;
  object_class->set_property = gst_va_display_win32_set_property;
  object_class->finalize = gst_va_display_win32_finalize;

  g_object_class_install_property (object_class, PROP_PATH,
      g_param_spec_string ("path", "Path",
          "String representation of DXGI Adapter LUID",
          nullptr, construct_only_flags));
  g_object_class_install_property (object_class, PROP_ADAPTER_LUID,
      g_param_spec_int64 ("adapter-luid", "Adapter LUID",
          "DXGI Adapter LUID",
          G_MININT64, G_MAXINT64, 0, construct_only_flags));
  g_object_class_install_property (object_class, PROP_DEVICE_ID,
      g_param_spec_uint ("device-id", "Device Id",
          "DXGI Device ID", 0, G_MAXUINT32, 0, construct_only_flags));
  g_object_class_install_property (object_class, PROP_VENDOR_ID,
      g_param_spec_uint ("vendor-id", "Vendor Id",
          "DXGI Vendor ID", 0, G_MAXUINT32, 0, construct_only_flags));
  g_object_class_override_property (object_class, PROP_DESC, "description");

  display_class->create_va_display =
      GST_DEBUG_FUNCPTR (gst_va_display_win32_create_va_display);
}

static void
gst_va_display_win32_init (GstVaDisplayWin32 * self)
{
}

static void
gst_va_display_win32_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstVaDisplayWin32 *self = GST_VA_DISPLAY_WIN32 (object);

  switch (prop_id) {
    case PROP_PATH:
      g_value_set_string (value, self->adapter_luid_str);
      break;
    case PROP_ADAPTER_LUID:
      g_value_set_int64 (value, self->adapter_luid);
      break;
    case PROP_DEVICE_ID:
      g_value_set_uint (value, self->device_id);
      break;
    case PROP_VENDOR_ID:
      g_value_set_uint (value, self->vendor_id);
      break;
    case PROP_DESC:
      g_value_set_string (value, self->desc);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_va_display_win32_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstVaDisplayWin32 *self = GST_VA_DISPLAY_WIN32 (object);

  switch (prop_id) {
    case PROP_PATH:
      self->adapter_luid_str = g_value_dup_string (value);
      break;
    case PROP_ADAPTER_LUID:
      self->adapter_luid = g_value_get_int64 (value);
      break;
    case PROP_DEVICE_ID:
      self->device_id = g_value_get_uint (value);
      break;
    case PROP_VENDOR_ID:
      self->vendor_id = g_value_get_uint (value);
      break;
    case PROP_DESC:
      self->desc = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_va_display_win32_finalize (GObject * object)
{
  GstVaDisplayWin32 *self = GST_VA_DISPLAY_WIN32 (object);

  g_free (self->adapter_luid_str);
  g_free (self->desc);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gpointer
gst_va_display_win32_create_va_display (GstVaDisplay * display)
{
  GstVaDisplayWin32 *self = GST_VA_DISPLAY_WIN32 (display);
  LARGE_INTEGER val;
  LUID luid;

  val.QuadPart = self->adapter_luid;
  luid.LowPart = val.LowPart;
  luid.HighPart = val.HighPart;

  return vaGetDisplayWin32 (&luid);
}

/**
 * gst_va_display_win32_new:
 * @adapter_luid: DXGI adapter luid
 *
 * Creates a new #GstVaDisplay from Win32 Direct3D backend
 *
 * Returns: (transfer full): a newly allocated #GstVaDisplay if the
 *     specified Win32 backend could be opened and initialized;
 *     otherwise %NULL is returned.
 *
 * Since: 1.24
 */
GstVaDisplay *
gst_va_display_win32_new (const gchar * adapter_luid)
{
  GstVaDisplayWin32 *self;
  HRESULT hr;
  ComPtr < IDXGIFactory1 > factory;
  DXGI_ADAPTER_DESC desc;
  gint64 adapter_luid_i64;
  gchar *desc_str;
  gint max_profiles, max_entry_points;
  gint num_profiles;
  VAStatus status;
  VADisplay dpy;
  std::vector < VAEntrypoint > entry_points;
  std::vector < VAProfile > profiles;

  g_return_val_if_fail (adapter_luid != nullptr, nullptr);

  /* *INDENT-OFF* */
  try {
    adapter_luid_i64 = std::stoll (adapter_luid);
  } catch (...) {
    return nullptr;
  }
  /* *INDENT-ON* */

  hr = CreateDXGIFactory1 (IID_PPV_ARGS (&factory));
  if (FAILED (hr))
    return nullptr;

  for (guint i = 0;; i++) {
    ComPtr < IDXGIAdapter > adapter;
    LARGE_INTEGER val;

    hr = factory->EnumAdapters (i, &adapter);
    if (FAILED (hr))
      return nullptr;

    hr = adapter->GetDesc (&desc);
    if (FAILED (hr))
      continue;

    val.LowPart = desc.AdapterLuid.LowPart;
    val.HighPart = desc.AdapterLuid.HighPart;

    if (val.QuadPart == adapter_luid_i64)
      break;
  }

  desc_str = g_utf16_to_utf8 ((gunichar2 *) desc.Description,
      -1, nullptr, nullptr, nullptr);

  self = (GstVaDisplayWin32 *) g_object_new (gst_va_display_win32_get_type (),
      "path", adapter_luid, "adapter-luid", adapter_luid_i64, "device-id",
      desc.DeviceId, "vendor-id", desc.VendorId, nullptr);
  self->desc = desc_str;
  if (!gst_va_display_initialize (GST_VA_DISPLAY (self)))
    goto error;

  /* Validate device */
  dpy = gst_va_display_get_va_dpy (GST_VA_DISPLAY (self));

  max_profiles = vaMaxNumProfiles (dpy);
  if (max_profiles <= 0)
    goto error;

  max_entry_points = vaMaxNumEntrypoints (dpy);
  if (max_entry_points <= 0)
    goto error;

  profiles.resize (max_profiles);

  status = vaQueryConfigProfiles (dpy, &profiles[0], &num_profiles);
  if (status != VA_STATUS_SUCCESS || num_profiles <= 0)
    goto error;

  entry_points.resize (max_entry_points);
  for (gint i = 0; i < num_profiles; i++) {
    gint num_entry_poinits;
    status = vaQueryConfigEntrypoints (dpy, profiles[i], &entry_points[0],
        &num_entry_poinits);
    if (status != VA_STATUS_SUCCESS)
      goto error;
  }

  gst_object_ref_sink (self);

  return GST_VA_DISPLAY (self);

error:
  gst_object_unref (self);
  return nullptr;
}
