
/* GStreamer
 * Copyright (C) 2019 Thibault Saunier <tsaunier@igalia.com>
 *
 * gsttranscodebin.c:
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
#  include "config.h"
#endif

#include "gsttranscodeelements.h"
#include <glib/gi18n-lib.h>
#include <gst/pbutils/pbutils.h>

#include <gst/pbutils/missing-plugins.h>

GST_DEBUG_CATEGORY (gst_transcodebin_debug);
#define GST_CAT_DEFAULT gst_transcodebin_debug

void
transcodebin_element_init (GstPlugin * plugin)
{
  static gsize res = FALSE;

  if (g_once_init_enter (&res)) {
    gst_pb_utils_init ();
    GST_DEBUG_CATEGORY_INIT (gst_transcodebin_debug, "transcodebin", 0,
        "Transcodebin element");
    g_once_init_leave (&res, TRUE);
  }
}
