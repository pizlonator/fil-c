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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstasioutils.h"
#include <windows.h>
#include <string.h>
#include <atlconv.h>

static gboolean
gst_asio_enum_check_class_root (GstAsioDeviceInfo * info, LPCWSTR clsid)
{
  LSTATUS status;
  HKEY root_key = nullptr;
  HKEY device_key = nullptr;
  HKEY proc_server_key = nullptr;
  DWORD type = REG_SZ;
  CHAR data[256];
  DWORD size = sizeof (data);
  gboolean ret = FALSE;

  status = RegOpenKeyExW (HKEY_CLASSES_ROOT, L"clsid", 0, KEY_READ, &root_key);
  if (status != ERROR_SUCCESS)
    return FALSE;

  /* Read registry HKEY_CLASS_ROOT/CLSID/{device-clsid} */
  status = RegOpenKeyExW (root_key, clsid, 0, KEY_READ, &device_key);
  if (status != ERROR_SUCCESS)
    goto done;

  /* ThreadingModel describes COM apartment */
  status = RegOpenKeyExW (device_key,
      L"InprocServer32", 0, KEY_READ, &proc_server_key);
  if (status != ERROR_SUCCESS)
    goto done;

  status = RegQueryValueExA (proc_server_key,
      "ThreadingModel", nullptr, &type, (LPBYTE) data, &size);
  if (status != ERROR_SUCCESS)
    goto done;

  if (g_ascii_strcasecmp (data, "Both") == 0 ||
      g_ascii_strcasecmp (data, "Free") == 0) {
    info->sta_model = FALSE;
  } else {
    info->sta_model = TRUE;
  }

  ret = TRUE;

done:
  if (proc_server_key)
    RegCloseKey (proc_server_key);

  if (device_key)
    RegCloseKey (device_key);

  if (root_key)
    RegCloseKey (root_key);

  return ret;
}

static GstAsioDeviceInfo *
gst_asio_enum_new_device_info_from_reg (HKEY reg_key, LPWSTR key_name)
{
  LSTATUS status;
  HKEY sub_key = nullptr;
  WCHAR clsid_data[256];
  WCHAR desc_data[256];
  DWORD type = REG_SZ;
  DWORD size = sizeof (clsid_data);
  GstAsioDeviceInfo *ret = nullptr;
  CLSID id;
  HRESULT hr;

  USES_CONVERSION;

  status = RegOpenKeyExW (reg_key, key_name, 0, KEY_READ, &sub_key);
  if (status != ERROR_SUCCESS)
    return nullptr;

  /* find CLSID value, used for CoCreateInstance */
  status = RegQueryValueExW (sub_key,
      L"clsid", 0, &type, (LPBYTE) clsid_data, &size);
  if (status != ERROR_SUCCESS)
    goto done;

  hr = CLSIDFromString (W2COLE (clsid_data), &id);
  if (FAILED (hr))
    goto done;

  ret = g_new0 (GstAsioDeviceInfo, 1);
  ret->clsid = id;
  ret->driver_name = g_utf16_to_utf8 ((gunichar2 *) key_name, -1,
      nullptr, nullptr, nullptr);

  /* human readable device description */
  status = RegQueryValueExW (sub_key,
      L"description", 0, &type, (LPBYTE) desc_data, &size);
  if (status != ERROR_SUCCESS) {
    GST_WARNING ("no description");
    ret->driver_desc = g_strdup (ret->driver_name);
  } else {
    ret->driver_desc = g_utf16_to_utf8 ((gunichar2 *) desc_data, -1,
        nullptr, nullptr, nullptr);
  }

  /* Check COM threading model */
  if (!gst_asio_enum_check_class_root (ret, clsid_data)) {
    gst_asio_device_info_free (ret);
    ret = nullptr;
  }

done:
  if (sub_key)
    RegCloseKey (sub_key);

  return ret;
}

guint
gst_asio_enum (GList ** infos)
{
  GList *info_list = nullptr;
  DWORD index = 0;
  guint num_device = 0;
  LSTATUS status;
  HKEY reg_key = nullptr;
  WCHAR key_name[512];

  g_return_val_if_fail (infos != nullptr, 0);

  status = RegOpenKeyExW (HKEY_LOCAL_MACHINE, L"software\\asio", 0,
      KEY_READ, &reg_key);
  while (status == ERROR_SUCCESS) {
    GstAsioDeviceInfo *info;

    status = RegEnumKeyW (reg_key, index, key_name, 512);
    if (status != ERROR_SUCCESS)
      break;

    index++;
    info = gst_asio_enum_new_device_info_from_reg (reg_key, key_name);
    if (!info)
      continue;

    info_list = g_list_append (info_list, info);
    num_device++;
  }

  if (reg_key)
    RegCloseKey (reg_key);

  *infos = info_list;

  return num_device;
}

GstAsioDeviceInfo *
gst_asio_device_info_copy (const GstAsioDeviceInfo * info)
{
  GstAsioDeviceInfo *new_info;

  if (!info)
    return nullptr;

  new_info = g_new0 (GstAsioDeviceInfo, 1);

  new_info->clsid = info->clsid;
  new_info->sta_model = info->sta_model;
  new_info->driver_name = g_strdup (info->driver_name);
  new_info->driver_desc = g_strdup (info->driver_desc);

  return new_info;
}

void
gst_asio_device_info_free (GstAsioDeviceInfo * info)
{
  if (!info)
    return;

  g_free (info->driver_name);
  g_free (info->driver_desc);

  g_free (info);
}

GstAudioFormat
gst_asio_sample_type_to_gst (ASIOSampleType type)
{
  GstAudioFormat fmt;

  switch (type) {
      /*~~ MSB means big endian ~~ */
    case ASIOSTInt16MSB:
      fmt = GST_AUDIO_FORMAT_S16BE;
      break;
      /* FIXME: also used for 20 bits packed in 24 bits, how do we detect that? */
    case ASIOSTInt24MSB:
      fmt = GST_AUDIO_FORMAT_S24BE;
      break;
    case ASIOSTInt32MSB:
      fmt = GST_AUDIO_FORMAT_S32BE;
      break;
    case ASIOSTFloat32MSB:
      fmt = GST_AUDIO_FORMAT_F32BE;
      break;
    case ASIOSTFloat64MSB:
      fmt = GST_AUDIO_FORMAT_F64BE;
      break;
      /* All these are aligned to a different boundary than the packing, not sure
       * how to handle it, let's try the normal S32BE format */
    case ASIOSTInt32MSB16:
    case ASIOSTInt32MSB18:
    case ASIOSTInt32MSB20:
    case ASIOSTInt32MSB24:
      fmt = GST_AUDIO_FORMAT_S32BE;
      break;

      /*~~ LSB means little endian ~~ */
    case ASIOSTInt16LSB:
      fmt = GST_AUDIO_FORMAT_S16LE;
      break;
      /* FIXME: also used for 20 bits packed in 24 bits, how do we detect that? */
    case ASIOSTInt24LSB:
      fmt = GST_AUDIO_FORMAT_S24LE;
      break;
    case ASIOSTInt32LSB:
      fmt = GST_AUDIO_FORMAT_S32LE;
      break;
    case ASIOSTFloat32LSB:
      fmt = GST_AUDIO_FORMAT_F32LE;
      break;
    case ASIOSTFloat64LSB:
      fmt = GST_AUDIO_FORMAT_F64LE;
      break;
      /* All these are aligned to a different boundary than the packing, not sure
       * how to handle it, let's try the normal S32LE format */
    case ASIOSTInt32LSB16:
    case ASIOSTInt32LSB18:
    case ASIOSTInt32LSB20:
    case ASIOSTInt32LSB24:
      GST_WARNING ("weird alignment %ld, trying S32LE", type);
      fmt = GST_AUDIO_FORMAT_S32LE;
      break;

      /*~~ ASIO DSD formats are don't have gstreamer mappings ~~ */
    case ASIOSTDSDInt8LSB1:
    case ASIOSTDSDInt8MSB1:
    case ASIOSTDSDInt8NER8:
      GST_ERROR ("ASIO DSD formats are not supported");
      fmt = GST_AUDIO_FORMAT_UNKNOWN;
      break;
    default:
      GST_ERROR ("Unknown asio sample type %ld", type);
      fmt = GST_AUDIO_FORMAT_UNKNOWN;
      break;
  }

  return fmt;
}
