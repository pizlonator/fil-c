/* GStreamer
 * Copyright (C) 2020 Julien Isorce <jisorce@oblong.com>
 *
 * gstgssrc.c:
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

#ifndef __GST_GS_SRC_H__
#define __GST_GS_SRC_H__

#include <gst/base/gstbasesrc.h>
#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_TYPE_GS_SRC (gst_gs_src_get_type())
G_DECLARE_FINAL_TYPE(GstGsSrc, gst_gs_src, GST, GS_SRC, GstBaseSrc)
GST_ELEMENT_REGISTER_DECLARE(gssrc);

G_END_DECLS
#endif  // __GST_GS_SRC_H__
