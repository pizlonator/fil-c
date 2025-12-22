/* GStreamer
 * Copyright (C) 2015 Thibault Saunier <tsaunier@gnome.org>
 *
 * gsttranscodebin.c:
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

#ifndef __GST_TRANSCODING_H__
#define __GST_TRANSCODING_H__


#include <gst/gst.h>

GType gst_transcode_bin_get_type (void);
GType gst_uri_transcode_bin_get_type (void);

#endif /* __GST_TRANSCODING_H__ */
