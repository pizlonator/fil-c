/* GStreamer command line playback testing utility - keyboard handling helpers
 *
 * Copyright (C) 2013 Tim-Philipp Müller <tim centricular net>
 * Copyright (C) 2013 Centricular Ltd
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
#ifndef __GST_PLAY_KB_INCLUDED__
#define __GST_PLAY_KB_INCLUDED__

#include <glib.h>

#define GST_PLAY_KB_ARROW_UP    "\033[A"
#define GST_PLAY_KB_ARROW_DOWN  "\033[B"
#define GST_PLAY_KB_ARROW_RIGHT "\033[C"
#define GST_PLAY_KB_ARROW_LEFT  "\033[D"

typedef void (*GstPlayKbFunc) (const gchar * kb_input, gpointer user_data);

gboolean gst_play_kb_set_key_handler (GstPlayKbFunc kb_func, gpointer user_data);

#endif /* __GST_PLAY_KB_INCLUDED__ */
