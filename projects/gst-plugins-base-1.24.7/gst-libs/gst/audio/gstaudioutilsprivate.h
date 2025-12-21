/* GStreamer
 * Copyright (C) 2011 Mark Nauwelaerts <mark.nauwelaerts@collabora.co.uk>.
 * Copyright (C) 2011 Nokia Corporation. All rights reserved.
 *   Contact: Stefan Kost <stefan.kost@nokia.com>
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

#ifndef __GST_AUDIO_H__
#include <gst/audio/audio.h>
#endif

#ifndef _GST_AUDIO_UTILS_PRIVATE_H_
#define _GST_AUDIO_UTILS_PRIVATE_H_

#include <gst/gst.h>

G_BEGIN_DECLS

/* Element utility functions */
G_GNUC_INTERNAL
GstCaps *__gst_audio_element_proxy_getcaps (GstElement * element, GstPad * sinkpad,
                                            GstPad * srcpad, GstCaps * initial_caps,
                                            GstCaps * filter);

G_GNUC_INTERNAL
gboolean __gst_audio_encoded_audio_convert (GstAudioInfo * fmt, gint64 bytes,
                                            gint64 samples, GstFormat src_format,
                                            gint64 src_value, GstFormat * dest_format,
                                            gint64 * dest_value);

G_GNUC_INTERNAL
gboolean __gst_audio_set_thread_priority   (gpointer * handle);

G_GNUC_INTERNAL
gboolean __gst_audio_restore_thread_priority (gpointer handle);

G_END_DECLS

#endif
