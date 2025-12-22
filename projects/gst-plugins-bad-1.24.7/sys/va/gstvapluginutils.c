/* GStreamer
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

#include "gstvapluginutils.h"

GstVaDisplay *
gst_va_display_platform_new (const gchar * path)
{
#ifdef G_OS_WIN32
  return gst_va_display_win32_new (path);
#else
  return gst_va_display_drm_new_from_path (path);
#endif
}

void
gst_va_create_feature_name (GstVaDevice * device,
    const gchar * type_name_default, const gchar * type_name_templ,
    gchar ** type_name, const gchar * feature_name_default,
    const gchar * feature_name_templ, gchar ** feature_name,
    gchar ** desc, guint * rank)
{
  gchar *basename;

  /* The first element to be registered should use a constant name,
   * like vah264dec, for any additional elements, we create unique
   * names, using inserting the render device name. */
  if (device->index == 0) {
    *type_name = g_strdup (type_name_default);
    *feature_name = g_strdup (feature_name_default);
    g_object_get (device->display, "description", desc, NULL);
    return;
  }
#ifdef G_OS_WIN32
  basename = g_strdup_printf ("device%d", device->index);
#else
  basename = g_path_get_basename (device->render_device_path);
#endif

  *type_name = g_strdup_printf (type_name_templ, basename);
  *feature_name = g_strdup_printf (feature_name_templ, basename);

  g_object_get (device->display, "description", desc, NULL);
#ifndef G_OS_WIN32
  {
    gchar *newdesc = g_strdup_printf ("%s in %s", *desc, basename);
    g_free (*desc);
    *desc = newdesc;
  }
#endif
  g_free (basename);

  if (*rank > 0)
    *rank -= 1;
}

GstBufferPool *
gst_va_create_other_pool (GstAllocator * allocator,
    GstAllocationParams * params, GstCaps * caps, guint size)
{
  GstBufferPool *pool = NULL;
  GstStructure *config;

  if (size == 0) {
    GstVideoInfo info;

    if (!gst_video_info_from_caps (&info, caps))
      return NULL;
    size = GST_VIDEO_INFO_SIZE (&info);
  }

  pool = gst_video_buffer_pool_new ();
  config = gst_buffer_pool_get_config (pool);

  gst_buffer_pool_config_set_params (config, caps, size, 0, 0);
  gst_buffer_pool_config_set_allocator (config, allocator, params);
  if (!gst_buffer_pool_set_config (pool, config))
    gst_clear_object (&pool);

  return pool;
}
