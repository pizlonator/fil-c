/* Copyright (C) <2018> Philippe Normand <philn@igalia.com>
 * Copyright (C) <2018> Žan Doberšek <zdobersek@igalia.com>
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
#include <config.h>
#endif

#include "gstwpevideosrc.h"
#include "gstwpesrcbin.h"
#include "gstwpe.h"

static gchar *extension_path = NULL;

GST_DEBUG_CATEGORY (wpe_video_src_debug);
GST_DEBUG_CATEGORY (wpe_view_debug);
GST_DEBUG_CATEGORY (wpe_src_debug);

const gchar *gst_wpe_get_devenv_extension_path (void)
{
  return extension_path;
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  gboolean result;
  gchar *dirname;
  const gchar *filename = gst_plugin_get_filename (plugin);

  GST_DEBUG_CATEGORY_INIT (wpe_video_src_debug, "wpevideosrc", 0, "WPE Video Source");
  GST_DEBUG_CATEGORY_INIT (wpe_view_debug, "wpeview", 0, "WPE Threaded View");
  GST_DEBUG_CATEGORY_INIT (wpe_src_debug, "wpesrc", 0, "WPE Source");

  if (filename != NULL) {
      dirname = g_path_get_dirname (filename);
      extension_path = g_build_filename (dirname, "wpe-extension", NULL);
      g_free (dirname);
  }
  result = gst_element_register (plugin, "wpevideosrc", GST_RANK_NONE,
      GST_TYPE_WPE_VIDEO_SRC);
  result &= gst_element_register(plugin, "wpesrc", GST_RANK_NONE, GST_TYPE_WPE_SRC);
  return result;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR, GST_VERSION_MINOR,
    wpe, "WPE src plugin", plugin_init, VERSION, GST_LICENSE, PACKAGE,
    GST_PACKAGE_ORIGIN)
