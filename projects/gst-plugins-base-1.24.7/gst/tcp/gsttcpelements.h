/*
 * Copyright (C) 2020 Huawei Technologies Co., Ltd.
 *   @Author: Julian Bouzas <julian.bouzas@collabora.com>
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

#ifndef __GST_TCP_ELEMENTS_H__
#define __GST_TCP_ELEMENTS_H__

#include <gst/gst.h>
#include <gio/gio.h>

#define TCP_HIGHEST_PORT        65535
#define TCP_DEFAULT_HOST        "localhost"
#define TCP_DEFAULT_PORT        4953

G_BEGIN_DECLS

G_GNUC_INTERNAL void tcp_element_init (GstPlugin * plugin);

GST_ELEMENT_REGISTER_DECLARE (socketsrc);
GST_ELEMENT_REGISTER_DECLARE (tcpclientsink);
GST_ELEMENT_REGISTER_DECLARE (tcpclientsrc);
GST_ELEMENT_REGISTER_DECLARE (tcpserversink);
GST_ELEMENT_REGISTER_DECLARE (tcpserversrc);
GST_ELEMENT_REGISTER_DECLARE (multifdsink);
GST_ELEMENT_REGISTER_DECLARE (multisocketsink);

G_GNUC_INTERNAL GList *    tcp_get_addresses (GstElement * obj, const char *host,
                                              GCancellable * cancellable, GError ** err);
G_GNUC_INTERNAL GSocket *  tcp_create_socket (GstElement * obj, GList ** iter,
                                              guint16 port, GSocketAddress ** saddr, GError ** err);

G_END_DECLS

#endif /* __GST_TCP_ELEMENTS_H__ */
