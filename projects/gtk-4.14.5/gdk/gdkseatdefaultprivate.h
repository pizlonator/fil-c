/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2015 Red Hat
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
 * Author: Carlos Garnacho <carlosg@gnome.org>
 */

#pragma once

#include "gdkseat.h"
#include "gdkseatprivate.h"

G_BEGIN_DECLS

#define GDK_TYPE_SEAT_DEFAULT         (gdk_seat_default_get_type ())
#define GDK_SEAT_DEFAULT(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GDK_TYPE_SEAT_DEFAULT, GdkSeatDefault))
#define GDK_IS_SEAT_DEFAULT(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GDK_TYPE_SEAT_DEFAULT))
#define GDK_SEAT_DEFAULT_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), GDK_TYPE_SEAT_DEFAULT, GdkSeatDefaultClass))
#define GDK_IS_SEAT_DEFAULT_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), GDK_TYPE_SEAT_DEFAULT))
#define GDK_SEAT_DEFAULT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GDK_TYPE_SEAT_DEFAULT, GdkSeatDefaultClass))

typedef struct _GdkSeatDefault GdkSeatDefault;
typedef struct _GdkSeatDefaultClass GdkSeatDefaultClass;

struct _GdkSeatDefault
{
  GdkSeat parent_instance;
};

struct _GdkSeatDefaultClass
{
  GdkSeatClass parent_class;
};

GType     gdk_seat_default_get_type     (void) G_GNUC_CONST;

GdkSeat * gdk_seat_default_new_for_logical_pair         (GdkDevice      *pointer,
                                                         GdkDevice      *keyboard);

void      gdk_seat_default_add_physical_device          (GdkSeatDefault *seat,
                                                         GdkDevice      *device);
void      gdk_seat_default_remove_physical_device       (GdkSeatDefault *seat,
                                                         GdkDevice      *device);
void      gdk_seat_default_add_tool                     (GdkSeatDefault *seat,
                                                         GdkDeviceTool  *tool);
void      gdk_seat_default_remove_tool                  (GdkSeatDefault *seat,
                                                         GdkDeviceTool  *tool);

G_END_DECLS

