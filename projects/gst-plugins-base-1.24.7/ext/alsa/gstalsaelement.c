/*
 * Copyright (C) 2001 CodeFactory AB
 * Copyright (C) 2001 Thomas Nyberg <thomas@codefactory.se>
 * Copyright (C) 2001-2002 Andy Wingo <apwingo@eos.ncsu.edu>
 * Copyright (C) 2003 Benjamin Otte <in7y118@public.uni-hamburg.de>
 * Copyright (C) 2020 Huawei Technologies Co., Ltd.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstalsaelements.h"
#include "gstalsadeviceprovider.h"

#include <glib/gi18n-lib.h>

GST_DEBUG_CATEGORY (alsa_debug);

/* ALSA debugging wrapper */
/* *INDENT-OFF* */
G_GNUC_PRINTF (5, 6)
/* *INDENT-ON* */
static void
gst_alsa_error_wrapper (const char *file, int line, const char *function,
    int err, const char *fmt, ...)
{
#ifndef GST_DISABLE_GST_DEBUG
  va_list args;
  gchar *str;

  va_start (args, fmt);
  str = g_strdup_vprintf (fmt, args);
  va_end (args);
  /* FIXME: use GST_LEVEL_ERROR here? Currently warning is used because we're
   * able to catch enough of the errors that would be printed otherwise
   */
  gst_debug_log (alsa_debug, GST_LEVEL_WARNING, file, function, line, NULL,
      "alsalib error: %s%s%s", str, err ? ": " : "",
      err ? snd_strerror (err) : "");
  g_free (str);
#endif
}

GST_DEVICE_PROVIDER_REGISTER_DEFINE (alsadeviceprovider, "alsadeviceprovider",
    GST_RANK_SECONDARY, GST_TYPE_ALSA_DEVICE_PROVIDER);

void
alsa_element_init (GstPlugin * plugin)
{
  static gsize res = FALSE;

  if (g_once_init_enter (&res)) {
    GST_DEBUG_CATEGORY_INIT (alsa_debug, "alsa", 0, "alsa plugins");
#ifdef ENABLE_NLS
    GST_DEBUG ("binding text domain %s to locale dir %s", GETTEXT_PACKAGE,
        LOCALEDIR);
    bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
#endif
    if (snd_lib_error_set_handler (gst_alsa_error_wrapper) != 0)
      GST_WARNING ("failed to set alsa error handler");
    g_once_init_leave (&res, TRUE);
  }
}
