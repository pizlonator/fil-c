/* GStreamer
 * Copyright (C) 2016 Carlos Rafael Giani <dv@pseudoterminal.org>
 *
 * unalignedaudio.h:
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

#ifndef __GST_UNALIGNED_AUDIO_H__
#define __GST_UNALIGNED_AUDIO_H__

#include <gst/gst.h>
#include <gst/audio/audio.h>

#define GST_UNALIGNED_RAW_AUDIO_CAPS \
  "audio/x-unaligned-raw" \
  ", format = (string) " GST_AUDIO_FORMATS_ALL \
  ", rate = (int) [ 1, MAX ]" \
  ", channels = (int) [ 1, MAX ]" \
  ", layout = (string) { interleaved, non-interleaved }"

#endif /* __GST_UNALIGNED_AUDIO_H__ */
