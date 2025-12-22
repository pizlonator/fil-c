/* GStreamer
 * Copyright (C) 1999,2000 Erik Walthinsen <omega@cse.ogi.edu>
 *                    2000 Wim Taymans <wtay@chello.be>
 *                    2002 Kristian Rietveld <kris@gtk.org>
 *                    2002,2003 Colin Walters <walters@gnu.org>
 *                    2001,2010 Bastien Nocera <hadess@hadess.net>
 *                    2010 Sebastian Dröge <sebastian.droege@collabora.co.uk>
 *                    2010 Jan Schmidt <thaytan@noraisin.net>
 *
 * rtmpsrc.c:
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

#include <gst/gst.h>

#include "gstrtmpelements.h"
#include "gstrtmpsrc.h"
#include "gstrtmpsink.h"

#ifndef GST_DISABLE_GST_DEBUG
GST_DEBUG_CATEGORY_STATIC (rtmp_debug);

static void
gst_rtmp_log_callback (int level, const gchar * fmt, va_list vl)
{
  GstDebugLevel gst_level;

  switch (level) {
    case RTMP_LOGCRIT:
    case RTMP_LOGERROR:
      gst_level = GST_LEVEL_ERROR;
      break;
    case RTMP_LOGWARNING:
      gst_level = GST_LEVEL_WARNING;
      break;
    case RTMP_LOGINFO:
      gst_level = GST_LEVEL_INFO;
      break;
    case RTMP_LOGDEBUG:
      gst_level = GST_LEVEL_DEBUG;
      break;
    case RTMP_LOGDEBUG2:
      gst_level = GST_LEVEL_LOG;
      break;
    default:
      gst_level = GST_LEVEL_TRACE;
      break;
  }

  gst_debug_log_valist (rtmp_debug, gst_level, "", "", 0, NULL, fmt, vl);
}

static void
_set_debug_level (void)
{
  GstDebugLevel gst_level;

  RTMP_LogSetCallback (gst_rtmp_log_callback);
  gst_level = gst_debug_category_get_threshold (rtmp_debug);

  switch (gst_level) {
    case GST_LEVEL_ERROR:
      RTMP_LogSetLevel (RTMP_LOGERROR);
      break;
    case GST_LEVEL_WARNING:
    case GST_LEVEL_FIXME:
      RTMP_LogSetLevel (RTMP_LOGWARNING);
      break;
    case GST_LEVEL_INFO:
      RTMP_LogSetLevel (RTMP_LOGINFO);
      break;
    case GST_LEVEL_DEBUG:
      RTMP_LogSetLevel (RTMP_LOGDEBUG);
      break;
    case GST_LEVEL_LOG:
      RTMP_LogSetLevel (RTMP_LOGDEBUG2);
      break;
    default:                   /* _TRACE and beyond */
      RTMP_LogSetLevel (RTMP_LOGALL);
  }
}
#endif

void
rtmp_element_init (GstPlugin * plugin)
{
  static gsize res = FALSE;
  if (g_once_init_enter (&res)) {
#ifndef GST_DISABLE_GST_DEBUG
    GST_DEBUG_CATEGORY_INIT (rtmp_debug, "rtmp", 0, "libRTMP logging");
    _set_debug_level ();
#endif
    g_once_init_leave (&res, TRUE);
  }
}
