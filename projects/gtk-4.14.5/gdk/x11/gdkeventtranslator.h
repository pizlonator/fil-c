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

#include "gdktypes.h"
#include "gdkdisplay.h"

#include "gdk/gdkdeviceprivate.h"

#include <X11/Xlib.h>

G_BEGIN_DECLS

#define GDK_TYPE_EVENT_TRANSLATOR         (_gdk_x11_event_translator_get_type ())
#define GDK_EVENT_TRANSLATOR(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GDK_TYPE_EVENT_TRANSLATOR, GdkEventTranslator))
#define GDK_IS_EVENT_TRANSLATOR(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GDK_TYPE_EVENT_TRANSLATOR))
#define GDK_EVENT_TRANSLATOR_GET_IFACE(o) (G_TYPE_INSTANCE_GET_INTERFACE  ((o), GDK_TYPE_EVENT_TRANSLATOR, GdkEventTranslatorIface))

typedef struct _GdkEventTranslatorIface GdkEventTranslatorIface;
typedef struct _GdkEventTranslator GdkEventTranslator; /* Dummy typedef */

struct _GdkEventTranslatorIface
{
  GTypeInterface iface;

  /* VMethods */
  GdkEvent * (* translate_event) (GdkEventTranslator *translator,
                                  GdkDisplay         *display,
                                  const XEvent       *xevent);

  GdkEventMask (* get_handled_events)    (GdkEventTranslator *translator);
  void         (* select_surface_events) (GdkEventTranslator *translator,
                                          Window              window,
                                          GdkEventMask        event_mask);
  GdkSurface *  (* get_surface)          (GdkEventTranslator *translator,
                                          const XEvent       *xevent);
};

GType      _gdk_x11_event_translator_get_type (void) G_GNUC_CONST;

GdkEvent * _gdk_x11_event_translator_translate (GdkEventTranslator *translator,
                                                GdkDisplay         *display,
                                                const XEvent       *xevent);
GdkEventMask _gdk_x11_event_translator_get_handled_events    (GdkEventTranslator *translator);
void         _gdk_x11_event_translator_select_surface_events (GdkEventTranslator *translator,
                                                              Window              window,
                                                              GdkEventMask        event_mask);
GdkSurface *  _gdk_x11_event_translator_get_surface          (GdkEventTranslator *translator,
                                                              GdkDisplay         *display,
                                                              const XEvent       *xevent);

G_END_DECLS

