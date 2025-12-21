/* GStreamer base utils library
 * Copyright (C) 2006 Tim-Philipp Müller <tim centricular net>
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
 * SECTION:gstpbutils
 * @title: Pbutils
 * @short_description: General Application and Plugin Utility Library
 *
 * libgstpbutils is a general utility library for plugins and applications.
 * It currently provides the
 * following:
 *
 * * human-readable description strings of codecs, elements, sources, decoders,
 * encoders, or sinks from decoder/encoder caps, element names, or protocol
 * names.
 *
 * * support for applications to initiate installation of missing plugins (if
 * this is supported by the distribution or operating system used)
 *
 * * API for GStreamer elements to create missing-plugin messages in order to
 * communicate to the application that a certain type of plugin is missing
 * (decoder, encoder, URI protocol source, URI protocol sink, named element)
 *
 * * API for applications to recognise and handle missing-plugin messages
 *
 * ## Linking to this library
 *
 * You should obtain the required CFLAGS and LIBS using pkg-config on the
 * gstreamer-plugins-base-1.0 module. You will then also need to add
 * '-lgstreamer-pbutils-1.0' manually to your LIBS line.
 *
 * ## Library initialisation
 *
 * Before using any of its functions, applications and plugins must call
 * gst_pb_utils_init() to initialise the library.
 *
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "pbutils.h"
#include "pbutils-private.h"

#include <glib/gi18n-lib.h>

#ifndef GST_DISABLE_GST_DEBUG
#define GST_CAT_DEFAULT gst_pb_utils_ensure_debug_category()

static GstDebugCategory *
gst_pb_utils_ensure_debug_category (void)
{
  static gsize cat_gonce = 0;

  if (g_once_init_enter (&cat_gonce)) {
    GstDebugCategory *cat = NULL;

    GST_DEBUG_CATEGORY_INIT (cat, "pbutils", 0, "GStreamer Plugins Base utils");

    g_once_init_leave (&cat_gonce, (gsize) cat);
  }

  return (GstDebugCategory *) cat_gonce;
}
#endif /* GST_DISABLE_GST_DEBUG */

static gpointer
_init_locale_text_domain (gpointer data)
{
#ifdef ENABLE_NLS
  GST_DEBUG ("binding text domain %s to locale dir %s", GETTEXT_PACKAGE,
      LOCALEDIR);
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
#endif

  return NULL;
}

void
gst_pb_utils_init_locale_text_domain (void)
{
  static GOnce locale_init_once = G_ONCE_INIT;

  g_once (&locale_init_once, _init_locale_text_domain, NULL);
}

/**
 * gst_pb_utils_init:
 *
 * Initialises the base utils support library. This function is not
 * thread-safe. Applications should call it after calling gst_init(),
 * plugins should call it from their plugin_init function.
 *
 * This function may be called multiple times. It will do nothing if the
 * library has already been initialised.
 */
void
gst_pb_utils_init (void)
{
  static gboolean inited;       /* FALSE */

  if (inited) {
    GST_LOG ("already initialised");
    return;
  }
  gst_pb_utils_init_locale_text_domain ();

  inited = TRUE;
}
