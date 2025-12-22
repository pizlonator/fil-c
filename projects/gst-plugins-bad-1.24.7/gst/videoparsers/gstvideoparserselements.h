/* GStreamer
 * Copyright (C) 2020 Huawei Technologies Co., Ltd.
 *   @Author: Stéphane Cerveau <stephane.cerveau@collabora.com>
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


#ifndef __GST_VIDEOPARSERS_ELEMENTS_H__
#define __GST_VIDEOPARSERS_ELEMENTS_H__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gst/gst.h>


void videoparsers_element_init (GstPlugin * plugin);

GST_ELEMENT_REGISTER_DECLARE (av1parse);
GST_ELEMENT_REGISTER_DECLARE (diracparse);
GST_ELEMENT_REGISTER_DECLARE (h263parse);
GST_ELEMENT_REGISTER_DECLARE (h264parse);
GST_ELEMENT_REGISTER_DECLARE (h265parse);
GST_ELEMENT_REGISTER_DECLARE (jpeg2000parse);
GST_ELEMENT_REGISTER_DECLARE (mpeg4videoparse);
GST_ELEMENT_REGISTER_DECLARE (mpegvideoparse);
GST_ELEMENT_REGISTER_DECLARE (pngparse);
GST_ELEMENT_REGISTER_DECLARE (vc1parse);
GST_ELEMENT_REGISTER_DECLARE (vp9parse);

#endif /* __GST_VIDEOPARSERS_ELEMENTS_H__ */
