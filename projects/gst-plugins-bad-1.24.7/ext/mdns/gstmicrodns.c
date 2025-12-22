/* GStreamer
 * Copyright (C) 2019 Mathieu Duponchelle <mathieu@centricular.com>
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstmicrodnsdevice.h"

GST_DEVICE_PROVIDER_REGISTER_DECLARE (microdnsdeviceprovider);
GST_DEVICE_PROVIDER_REGISTER_DEFINE (microdnsdeviceprovider,
    "microdnsdeviceprovider", GST_RANK_PRIMARY, GST_TYPE_MDNS_DEVICE_PROVIDER);

static gboolean
plugin_init (GstPlugin * plugin)
{
  return GST_DEVICE_PROVIDER_REGISTER (microdnsdeviceprovider, plugin);;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    microdns,
    "libmicrodns plugin library",
    plugin_init, VERSION, "LGPL", GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
