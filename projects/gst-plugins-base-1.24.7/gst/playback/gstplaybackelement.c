/* GStreamer
 * Copyright (C) <2007> Wim Taymans <wim.taymans@gmail.com>
 * Copyright (C) 2020 Huawei Technologies Co., Ltd.
 *
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
 * SECTION:plugin-playback:
 * @short_description: Set of elements to create dynamic pipelines (or part of it) to play
 * media files.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>

#include <glib/gi18n-lib.h>
#include <gst/pbutils/pbutils.h>

#include "gstplaybackelements.h"


void
playback_element_init (GstPlugin * plugin)
{
  static gsize res = FALSE;

  if (g_once_init_enter (&res)) {
    gst_pb_utils_init ();

#ifdef ENABLE_NLS
    GST_DEBUG ("binding text domain %s to locale dir %s", GETTEXT_PACKAGE,
        LOCALEDIR);
    bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
#endif /* ENABLE_NLS */
    g_once_init_leave (&res, TRUE);
  }
}
