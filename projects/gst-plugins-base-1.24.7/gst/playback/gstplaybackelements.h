/* GStreamer
 * Copyright (C) <2007> Wim Taymans <wim.taymans@gmail.com>
 * Copyright (C) 2020 Huawei Technologies Co., Ltd.
 *   @Author: Stéphane Cerveau <scerveau@collabora.com>
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

#ifndef __GST_PLAY_BACK_ELEMENTS_H__
#define __GST_PLAY_BACK_ELEMENTS_H__

#include <gst/gst.h>

GST_ELEMENT_REGISTER_DECLARE (playbin);
GST_ELEMENT_REGISTER_DECLARE (playbin3);
GST_ELEMENT_REGISTER_DECLARE (playsink);
GST_ELEMENT_REGISTER_DECLARE (subtitleoverlay);
GST_ELEMENT_REGISTER_DECLARE (streamsynchronizer);
GST_ELEMENT_REGISTER_DECLARE (decodebin);
GST_ELEMENT_REGISTER_DECLARE (decodebin3);
GST_ELEMENT_REGISTER_DECLARE (uridecodebin);
GST_ELEMENT_REGISTER_DECLARE (uridecodebin3);
GST_ELEMENT_REGISTER_DECLARE (urisourcebin);
GST_ELEMENT_REGISTER_DECLARE (parsebin);

gboolean gst_play_bin_custom_element_init (GstPlugin * plugin);
gboolean gst_play_bin3_custom_element_init (GstPlugin * plugin);

G_GNUC_INTERNAL void playback_element_init (GstPlugin * plugin);

#endif /* __GST_PLAY_BACK_ELEMENTS_H__ */
