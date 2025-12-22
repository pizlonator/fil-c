/* GStreamer
 * Copyright (C) 2020 Julien Isorce <jisorce@oblong.com>
 *
 * gstgssrc.c:
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
 * plugin-gs:
 *
 * The gs plugin contains elements to interact with with Google Cloud Storage.
 * In particular with the gs:// protocol or by specifying the storage bucket.
 *
 * Since: 1.20
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstgssink.h"
#include "gstgssrc.h"

static gboolean plugin_init(GstPlugin* plugin) {
  gboolean ret = FALSE;

  ret |= GST_ELEMENT_REGISTER(gssrc, plugin);
  ret |= GST_ELEMENT_REGISTER(gssink, plugin);

  return ret;
}

GST_PLUGIN_DEFINE(GST_VERSION_MAJOR,
                  GST_VERSION_MINOR,
                  gs,
                  "Read and write from and to a Google Cloud Storage",
                  plugin_init,
                  PACKAGE_VERSION,
                  GST_LICENSE,
                  GST_PACKAGE_NAME,
                  GST_PACKAGE_ORIGIN)
