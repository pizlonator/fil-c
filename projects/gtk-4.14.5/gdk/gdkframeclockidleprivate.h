/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2010.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

/* Uninstalled header, internal to GDK */

#pragma once

#include "gdkframeclockprivate.h"

G_BEGIN_DECLS

#define GDK_TYPE_FRAME_CLOCK_IDLE            (gdk_frame_clock_idle_get_type ())
#define GDK_FRAME_CLOCK_IDLE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GDK_TYPE_FRAME_CLOCK_IDLE, GdkFrameClockIdle))
#define GDK_FRAME_CLOCK_IDLE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_FRAME_CLOCK_IDLE, GdkFrameClockIdleClass))
#define GDK_IS_FRAME_CLOCK_IDLE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GDK_TYPE_FRAME_CLOCK_IDLE))
#define GDK_IS_FRAME_CLOCK_IDLE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_FRAME_CLOCK_IDLE))
#define GDK_FRAME_CLOCK_IDLE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_FRAME_CLOCK_IDLE, GdkFrameClockIdleClass))


typedef struct _GdkFrameClockIdle              GdkFrameClockIdle;
typedef struct _GdkFrameClockIdlePrivate       GdkFrameClockIdlePrivate;
typedef struct _GdkFrameClockIdleClass         GdkFrameClockIdleClass;

struct _GdkFrameClockIdle
{
  GdkFrameClock parent_instance;

  /*< private >*/
  GdkFrameClockIdlePrivate *priv;
};

struct _GdkFrameClockIdleClass
{
  GdkFrameClockClass parent_class;
};

GType           gdk_frame_clock_idle_get_type       (void) G_GNUC_CONST;

GdkFrameClock *_gdk_frame_clock_idle_new            (void);

G_END_DECLS

