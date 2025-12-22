/* GStreamer TTML subtitle parser
 * Copyright (C) <2015> British Broadcasting Corporation
 *   Authors:
 *     Chris Bass <dash@rd.bbc.co.uk>
 *     Peter Taylour <dash@rd.bbc.co.uk>
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

#ifndef _TTML_PARSE_H_
#define _TTML_PARSE_H_

#include <gst/gst.h>

G_BEGIN_DECLS

guint ttml_parse (const gchar * file, GstClockTime begin,
    GstClockTime duration, GList **parsed);

G_END_DECLS
#endif /* _TTML_PARSE_H_ */
