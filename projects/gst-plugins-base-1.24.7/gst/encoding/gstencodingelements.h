/* GStreamer encoding bin
 * Copyright (C) 2016 Jan Schmidt <jan@centricular.com>
 *           (C) 2020 Thibault Saunier <tsaunier@igalia.com>
 *           (C) 2009 Edward Hervey <edward.hervey@collabora.co.uk>
 *           (C) 2009 Nokia Corporation
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
#ifndef __ENCODING_ELEMENTS_H__
#define __ENCODING_ELEMENTS_H__

#include <gst/gst.h>

G_BEGIN_DECLS

G_GNUC_INTERNAL void encoding_element_init (GstPlugin * plugin);

GST_ELEMENT_REGISTER_DECLARE (encodebin);
GST_ELEMENT_REGISTER_DECLARE (encodebin2);

G_END_DECLS

#endif //__ENCODING_ELEMENTS_H__
