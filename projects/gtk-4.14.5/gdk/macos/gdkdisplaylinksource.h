/* gdkdisplaylinksource.h
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *   Christian Hergert <christian@hergert.me>
 */

#pragma once

#include <glib.h>

#include <QuartzCore/QuartzCore.h>

G_BEGIN_DECLS

typedef struct
{
  GSource           source;

  CGDirectDisplayID display_id;
  CVDisplayLinkRef  display_link;
  gint64            refresh_interval;
  guint             refresh_rate;
  guint             paused : 1;

  volatile gint64   presentation_time;
  volatile guint    needs_dispatch;
} GdkDisplayLinkSource;

GSource *gdk_display_link_source_new     (CGDirectDisplayID     display_id,
                                          CGDisplayModeRef      mode);
void     gdk_display_link_source_pause   (GdkDisplayLinkSource *source);
void     gdk_display_link_source_unpause (GdkDisplayLinkSource *source);

G_END_DECLS

