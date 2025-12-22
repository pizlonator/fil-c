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

#include "gstvadisplay_priv.h"

#include <gst/va/gstvavideoformat.h>

#include "gstvaprofile.h"

GArray *
gst_va_display_get_profiles (GstVaDisplay * self, guint32 codec,
    VAEntrypoint entrypoint)
{
  GArray *ret = NULL;
  VADisplay dpy;
  VAEntrypoint *entrypoints;
  VAProfile *profiles;
  VAStatus status;
  gint i, j, num_entrypoints = 0, num_profiles = 0;

  g_return_val_if_fail (GST_IS_VA_DISPLAY (self), NULL);

  dpy = gst_va_display_get_va_dpy (self);

  num_profiles = vaMaxNumProfiles (dpy);
  num_entrypoints = vaMaxNumEntrypoints (dpy);

  profiles = g_new (VAProfile, num_profiles);
  entrypoints = g_new (VAEntrypoint, num_entrypoints);

  status = vaQueryConfigProfiles (dpy, profiles, &num_profiles);
  if (status != VA_STATUS_SUCCESS) {
    GST_ERROR ("vaQueryConfigProfile: %s", vaErrorStr (status));
    goto bail;
  }

  for (i = 0; i < num_profiles; i++) {
    if (codec != gst_va_profile_codec (profiles[i]))
      continue;

    status = vaQueryConfigEntrypoints (dpy, profiles[i], entrypoints,
        &num_entrypoints);
    if (status != VA_STATUS_SUCCESS) {
      GST_ERROR ("vaQueryConfigEntrypoints: %s", vaErrorStr (status));
      goto bail;
    }

    for (j = 0; j < num_entrypoints; j++) {
      if (entrypoints[j] == entrypoint) {
        if (!ret)
          ret = g_array_new (FALSE, FALSE, sizeof (VAProfile));
        g_array_append_val (ret, profiles[i]);
        break;
      }
    }
  }

bail:
  g_free (entrypoints);
  g_free (profiles);
  return ret;
}

GArray *
gst_va_display_get_image_formats (GstVaDisplay * self)
{
  GArray *ret = NULL;
  GstVideoFormat format;
  VADisplay dpy;
  VAImageFormat *va_formats;
  VAStatus status;
  int i, max, num = 0;

  g_return_val_if_fail (GST_IS_VA_DISPLAY (self), NULL);

  dpy = gst_va_display_get_va_dpy (self);

  max = vaMaxNumImageFormats (dpy);
  if (max == 0)
    return NULL;

  va_formats = g_new (VAImageFormat, max);

  status = vaQueryImageFormats (dpy, va_formats, &num);

  gst_va_video_format_fix_map (va_formats, num);

  if (status != VA_STATUS_SUCCESS) {
    GST_ERROR ("vaQueryImageFormats: %s", vaErrorStr (status));
    goto bail;
  }

  ret = g_array_sized_new (FALSE, FALSE, sizeof (GstVideoFormat), num);
  for (i = 0; i < num; i++) {
    format = gst_va_video_format_from_va_image_format (&va_formats[i]);
    if (format != GST_VIDEO_FORMAT_UNKNOWN)
      g_array_append_val (ret, format);
  }

  if (ret->len == 0) {
    g_array_unref (ret);
    ret = NULL;
  }

bail:
  g_free (va_formats);
  return ret;
}

gboolean
gst_va_display_has_vpp (GstVaDisplay * self)
{
  VADisplay dpy;
  VAEntrypoint *entrypoints;
  VAStatus status;
  int i, max, num;
  gboolean found = FALSE;
  g_return_val_if_fail (GST_IS_VA_DISPLAY (self), FALSE);

  dpy = gst_va_display_get_va_dpy (self);

  max = vaMaxNumEntrypoints (dpy);

  entrypoints = g_new (VAEntrypoint, max);

  status = vaQueryConfigEntrypoints (dpy, VAProfileNone, entrypoints, &num);
  if (status != VA_STATUS_SUCCESS) {
    GST_ERROR ("vaQueryConfigEntrypoints: %s", vaErrorStr (status));
    goto bail;
  }

  for (i = 0; i < num; i++) {
    if (entrypoints[i] == VAEntrypointVideoProc) {
      found = TRUE;
      break;
    }
  }

bail:
  g_free (entrypoints);
  return found;
}
