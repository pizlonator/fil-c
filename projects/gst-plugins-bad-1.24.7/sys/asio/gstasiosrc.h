/* GStreamer
 * Copyright (C) 2021 Seungha Yang <seungha@centricular.com>
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

#ifndef __GST_ASIO_SRC_H__
#define __GST_ASIO_SRC_H__

#include <gst/gst.h>
#include <gst/audio/audio.h>

G_BEGIN_DECLS

#define GST_TYPE_ASIO_SRC (gst_asio_src_get_type ())
G_DECLARE_FINAL_TYPE (GstAsioSrc,
    gst_asio_src, GST, ASIO_SRC, GstAudioBaseSrc);

G_END_DECLS

#endif /* __GST_ASIO_SRC_H__ */
