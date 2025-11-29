/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2009 Carlos Garnacho <carlosg@gnome.org>
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
 */

#pragma once

#include "gdkeventtranslator.h"
#include "gdkx11display.h"

G_BEGIN_DECLS

typedef struct _GdkEventSource GdkEventSource;

G_GNUC_INTERNAL
GSource * gdk_x11_event_source_new            (GdkDisplay *display);

G_GNUC_INTERNAL
void      gdk_x11_event_source_add_translator (GdkEventSource  *source,
                                               GdkEventTranslator *translator);

G_GNUC_INTERNAL
void      gdk_x11_event_source_select_events  (GdkEventSource *source,
                                               Window          window,
                                               GdkEventMask    event_mask,
                                               unsigned int    extra_x_mask);

gboolean  gdk_event_source_xevent             (GdkX11Display  *x11_display,
                                               const XEvent   *xevent);


G_END_DECLS

