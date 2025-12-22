/*
 * Copyright (C) 2010 Ole André Vadla Ravnås <oleavr@soundrop.com>
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

#include "vtutil.h"

gchar *
gst_vtutil_object_to_string (CFTypeRef obj)
{
  gchar *result;
  CFStringRef s;

  if (obj == NULL)
    return g_strdup ("(null)");

  s = CFCopyDescription (obj);
  result = gst_vtutil_string_to_utf8 (s);
  CFRelease (s);

  return result;
}

gchar *
gst_vtutil_string_to_utf8 (CFStringRef s)
{
  gchar *result;
  CFIndex size;

  size = CFStringGetMaximumSizeForEncoding (CFStringGetLength (s),
      kCFStringEncodingUTF8);
  result = g_malloc (size + 1);
  CFStringGetCString (s, result, size + 1, kCFStringEncodingUTF8);

  return result;
}

void
gst_vtutil_dict_set_i32 (CFMutableDictionaryRef dict, CFStringRef key,
    gint32 value)
{
  CFNumberRef number;

  number = CFNumberCreate (NULL, kCFNumberSInt32Type, &value);
  CFDictionarySetValue (dict, key, number);
  CFRelease (number);
}

void
gst_vtutil_dict_set_string (CFMutableDictionaryRef dict, CFStringRef key,
    const gchar * value)
{
  CFStringRef string;

  string = CFStringCreateWithCString (NULL, value, kCFStringEncodingASCII);
  CFDictionarySetValue (dict, key, string);
  CFRelease (string);
}

void
gst_vtutil_dict_set_boolean (CFMutableDictionaryRef dict, CFStringRef key,
    gboolean value)
{
  CFDictionarySetValue (dict, key, value ? kCFBooleanTrue : kCFBooleanFalse);
}

void
gst_vtutil_dict_set_data (CFMutableDictionaryRef dict, CFStringRef key,
    guint8 * value, guint64 length)
{
  CFDataRef data;

  data = CFDataCreate (NULL, value, length);
  CFDictionarySetValue (dict, key, data);
  CFRelease (data);
}

void
gst_vtutil_dict_set_object (CFMutableDictionaryRef dict, CFStringRef key,
    CFTypeRef * value)
{
  CFDictionarySetValue (dict, key, value);
  CFRelease (value);
}

CMVideoCodecType
gst_vtutil_codec_type_from_prores_variant (const char *variant)
{
  if (g_strcmp0 (variant, "standard") == 0)
    return kCMVideoCodecType_AppleProRes422;
  else if (g_strcmp0 (variant, "4444xq") == 0)
    return kCMVideoCodecType_AppleProRes4444XQ;
  else if (g_strcmp0 (variant, "4444") == 0)
    return kCMVideoCodecType_AppleProRes4444;
  else if (g_strcmp0 (variant, "hq") == 0)
    return kCMVideoCodecType_AppleProRes422HQ;
  else if (g_strcmp0 (variant, "lt") == 0)
    return kCMVideoCodecType_AppleProRes422LT;
  else if (g_strcmp0 (variant, "proxy") == 0)
    return kCMVideoCodecType_AppleProRes422Proxy;
  return GST_kCMVideoCodecType_Some_AppleProRes;
}

const char *
gst_vtutil_codec_type_to_prores_variant (CMVideoCodecType codec_type)
{
  switch (codec_type) {
    case kCMVideoCodecType_AppleProRes422:
      return "standard";
    case kCMVideoCodecType_AppleProRes4444XQ:
      return "4444xq";
    case kCMVideoCodecType_AppleProRes4444:
      return "4444";
    case kCMVideoCodecType_AppleProRes422HQ:
      return "hq";
    case kCMVideoCodecType_AppleProRes422LT:
      return "lt";
    case kCMVideoCodecType_AppleProRes422Proxy:
      return "proxy";
    default:
      g_assert_not_reached ();
  }
}

GstCaps *
gst_vtutil_caps_append_video_format (GstCaps * caps, const char *vfmt)
{
  GstStructure *s;
  GValueArray *arr;
  GValue val = G_VALUE_INIT;

  caps = gst_caps_make_writable (caps);
  s = gst_caps_get_structure (caps, 0);
  gst_structure_get_list (s, "format", &arr);

  g_value_init (&val, G_TYPE_STRING);

  g_value_set_string (&val, vfmt);
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  arr = g_value_array_append (arr, &val);
  G_GNUC_END_IGNORE_DEPRECATIONS;

  g_value_unset (&val);

  gst_structure_set_list (s, "format", arr);
  return caps;
}
