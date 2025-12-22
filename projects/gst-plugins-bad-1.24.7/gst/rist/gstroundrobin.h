/* GStreamer RIST plugin
 * Copyright (C) 2019 Net Insight AB
 *     Author: Nicolas Dufresne <nicolas.dufresne@collabora.com>
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

#include <gst/gst.h>

#ifndef __GST_ROUND_ROBIN_H__
#define __GST_ROUND_ROBIN_H__

#define GST_TYPE_ROUND_ROBIN    (gst_round_robin_get_type())
#define GST_ROUND_ROBIN(obj)    (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_ROUND_ROBIN,GstRoundRobin))
typedef struct _GstRoundRobin GstRoundRobin; 
typedef struct {
  GstElementClass parent;
} GstRoundRobinClass;
GType gst_round_robin_get_type (void);
GST_ELEMENT_REGISTER_DECLARE (roundrobin);

#endif
