/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
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

#include "gsttcpelements.h"

static gboolean
plugin_init (GstPlugin * plugin)
{
  gboolean ret = FALSE;

  ret |= GST_ELEMENT_REGISTER (socketsrc, plugin);
  ret |= GST_ELEMENT_REGISTER (tcpclientsink, plugin);
  ret |= GST_ELEMENT_REGISTER (tcpclientsrc, plugin);
  ret |= GST_ELEMENT_REGISTER (tcpserversink, plugin);
  ret |= GST_ELEMENT_REGISTER (tcpserversrc, plugin);

#ifdef HAVE_SYS_SOCKET_H
  ret |= GST_ELEMENT_REGISTER (multifdsink, plugin);

#endif
  ret |= GST_ELEMENT_REGISTER (multisocketsink, plugin);

  return ret;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    tcp,
    "transfer data over the network via TCP",
    plugin_init, VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
