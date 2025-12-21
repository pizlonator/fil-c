/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 * Copyright (C) <2008> Sebastian Dröge <sebastian.droege@collabora.co.uk>
 * Copyright (C) <2011-2012> Vincent Penquerc'h <vincent.penquerch@collabora.co.uk>
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

#ifndef __GST_OPUS_HEADER_H__
#define __GST_OPUS_HEADER_H__

#include <gst/gst.h>

G_BEGIN_DECLS

extern gboolean gst_opus_header_is_header (GstBuffer * buf,
    const char *magic, guint magic_size);
extern gboolean gst_opus_header_is_id_header (GstBuffer * buf);
extern gboolean gst_opus_header_is_comment_header (GstBuffer * buf);


G_END_DECLS

#endif /* __GST_OPUS_HEADER_H__ */
