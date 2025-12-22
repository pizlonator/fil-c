/* GStreamer libsndfile plugin
 * Copyright (C) 2003 Andy Wingo <wingo at pobox dot com>
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


#ifndef __GST_SFELEMENTS_H__
#define __GST_SFELEMENTS_H__


#include <gst/gst.h>
#include <sndfile.h>

G_BEGIN_DECLS

GstCaps *gst_sf_create_audio_template_caps (void);
void sf_element_init(GstPlugin * plugin);

GST_ELEMENT_REGISTER_DECLARE (sfdec);

G_END_DECLS


#endif /* __GST_SFELEMENTS_H__ */
