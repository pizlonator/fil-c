/* GStreamer
 * Copyright (C) 2017, Collabora Ltd.
 *   Author:Justin Kim <justin.kim@collabora.com>
 * Copyright (C) <2020> The GStreamer Contributors.
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

#include "gstsrtelements.h"
#include <srt/srt.h>


GST_DEBUG_CATEGORY_STATIC (gst_debug_srtlib);
GST_DEBUG_CATEGORY (gst_debug_srtobject);
#define GST_CAT_DEFAULT gst_debug_srtobject

#ifndef GST_DISABLE_GST_DEBUG
static void
gst_srt_log_handler (void *opaque, int level, const char *file, int line,
    const char *area, const char *message)
{
  GstDebugLevel gst_level;

  switch (level) {
    case LOG_CRIT:
      gst_level = GST_LEVEL_ERROR;
      break;

    case LOG_ERR:
      gst_level = GST_LEVEL_WARNING;
      break;

    case LOG_WARNING:
      gst_level = GST_LEVEL_INFO;
      break;

    case LOG_NOTICE:
      gst_level = GST_LEVEL_DEBUG;
      break;

    case LOG_DEBUG:
      gst_level = GST_LEVEL_LOG;
      break;

    default:
      gst_level = GST_LEVEL_FIXME;
      break;
  }

  if (G_UNLIKELY (gst_level <= _gst_debug_min)) {
    gst_debug_log (gst_debug_srtlib, gst_level, file, area, line, NULL, "%s",
        message);
  }
}
#endif

void
srt_element_init (GstPlugin * plugin)
{
  static gsize res = FALSE;

  if (g_once_init_enter (&res)) {
    GST_DEBUG_CATEGORY_INIT (gst_debug_srtobject, "srtobject", 0, "SRT Object");
    GST_DEBUG_CATEGORY_INIT (gst_debug_srtlib, "srtlib", 0, "SRT Library");
#ifndef GST_DISABLE_GST_DEBUG
    srt_setloghandler (NULL, gst_srt_log_handler);
    srt_setlogflags (SRT_LOGF_DISABLE_TIME | SRT_LOGF_DISABLE_THREADNAME |
        SRT_LOGF_DISABLE_SEVERITY | SRT_LOGF_DISABLE_EOL);
    srt_setloglevel (LOG_DEBUG);
#endif
    g_once_init_leave (&res, TRUE);
  }
}
