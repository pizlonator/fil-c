/*
 * Copyright (C) 2018 Centricular Ltd.
 *     Author: Nirbheek Chauhan <nirbheek@centricular.com>
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

#ifndef __GST_PROXY_PRIV_H__
#define __GST_PROXY_PRIV_H__

#include "gstproxysrc.h"
#include "gstproxysink.h"

G_BEGIN_DECLS

G_GNUC_INTERNAL
void gst_proxy_sink_set_proxysrc (GstProxySink *sink, GstProxySrc *src);

G_GNUC_INTERNAL
GstPad* gst_proxy_sink_get_internal_sinkpad (GstProxySink *sink);

G_GNUC_INTERNAL
GstPad* gst_proxy_src_get_internal_srcpad (GstProxySrc *src);

G_END_DECLS

#endif /* __GST_PROXY_PRIV_H__ */
