/* GStreamer
 *
 * Copyright (C) 2020 Stephan Hesse <stephan@emliri.com>
 * Copyright (C) 2020 Philippe Normand <philn@igalia.com>
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

#ifndef __GST_PLAY_MESSAGE_PRIVATE_H__
#define __GST_PLAY_MESSAGE_PRIVATE_H__

#define GST_PLAY_MESSAGE_DATA "gst-play-message-data"
#define GST_PLAY_MESSAGE_DATA_TYPE "play-message-type"
#define GST_PLAY_MESSAGE_DATA_URI "uri"
#define GST_PLAY_MESSAGE_DATA_POSITION "position"
#define GST_PLAY_MESSAGE_DATA_DURATION "duration"
#define GST_PLAY_MESSAGE_DATA_PLAY_STATE "play-state"
#define GST_PLAY_MESSAGE_DATA_BUFFERING_PERCENT "bufferring-percent"
#define GST_PLAY_MESSAGE_DATA_ERROR "error"
#define GST_PLAY_MESSAGE_DATA_ERROR_DETAILS "error-details"
#define GST_PLAY_MESSAGE_DATA_WARNING "warning"
#define GST_PLAY_MESSAGE_DATA_WARNING_DETAILS "warning-details"
#define GST_PLAY_MESSAGE_DATA_VIDEO_WIDTH "video-width"
#define GST_PLAY_MESSAGE_DATA_VIDEO_HEIGHT "video-height"
#define GST_PLAY_MESSAGE_DATA_MEDIA_INFO "media-info"
#define GST_PLAY_MESSAGE_DATA_VOLUME "volume"
#define GST_PLAY_MESSAGE_DATA_IS_MUTED "is-muted"

#endif
