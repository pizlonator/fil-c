/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 * Copyright (C) 2020 Huawei Technologies Co., Ltd.
 *   @Author: Stéphane Cerveau <scerveau@collabora.com>
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

GST_DEBUG_CATEGORY (tcp_debug);
#define GST_CAT_DEFAULT tcp_debug

void
tcp_element_init (GstPlugin * plugin)
{
  static gsize res = FALSE;
  if (g_once_init_enter (&res)) {
    GST_DEBUG_CATEGORY_INIT (tcp_debug, "tcp", 0, "TCP calls");
    g_once_init_leave (&res, TRUE);
  }
}

GList *
tcp_get_addresses (GstElement * obj, const char *host,
    GCancellable * cancellable, GError ** err)
{
  GList *addrs = NULL;
  GInetAddress *addr;

  g_return_val_if_fail (GST_IS_ELEMENT (obj), NULL);
  g_return_val_if_fail (host != NULL, NULL);
  g_return_val_if_fail (err == NULL || *err == NULL, NULL);

  /* look up name if we need to */
  addr = g_inet_address_new_from_string (host);
  if (addr) {
    addrs = g_list_append (addrs, addr);
  } else {
    GResolver *resolver = g_resolver_get_default ();

    GST_DEBUG_OBJECT (obj, "Looking up IP address(es) for host '%s'", host);
    addrs = g_resolver_lookup_by_name (resolver, host, cancellable, err);
    g_object_unref (resolver);
  }

  return addrs;
}

/*
 * Loops over available addresses until successfully creates a socket
 * 
 * iter: updated to contain current list position or NULL if finished
 * saddr: contains current address is successful
 */
GSocket *
tcp_create_socket (GstElement * obj, GList ** iter, guint16 port,
    GSocketAddress ** saddr, GError ** err)
{
  GSocket *sock = NULL;

  g_return_val_if_fail (GST_IS_ELEMENT (obj), NULL);
  g_return_val_if_fail (iter != NULL, NULL);
  g_return_val_if_fail (saddr != NULL, NULL);
  g_return_val_if_fail (err == NULL || *err == NULL, NULL);

  *saddr = NULL;
  while (*iter) {
    GInetAddress *addr = G_INET_ADDRESS ((*iter)->data);

#ifndef GST_DISABLE_GST_DEBUG
    {
      gchar *ip = g_inet_address_to_string (addr);
      GST_DEBUG_OBJECT (obj, "Trying IP address %s", ip);
      g_free (ip);
    }
#endif
    /* clean up from possible previous iterations */
    g_clear_error (err);
    /* update iter in case we get called again */
    *iter = (*iter)->next;

    *saddr = g_inet_socket_address_new (addr, port);
    sock =
        g_socket_new (g_socket_address_get_family (*saddr),
        G_SOCKET_TYPE_STREAM, G_SOCKET_PROTOCOL_TCP, err);
    if (sock)
      break;

    /* release and try next... */
    g_clear_object (saddr);
  }

  return sock;
}
