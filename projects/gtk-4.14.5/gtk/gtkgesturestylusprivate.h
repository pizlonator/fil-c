/* GTK - The GIMP Toolkit
 * Copyright (C) 2017-2018, Red Hat, Inc.
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
 * Author(s): Carlos Garnacho <carlosg@gnome.org>
 */
#pragma once

#include "gtkgesturesingleprivate.h"
#include "gtkgesturestylus.h"

struct _GtkGestureStylus
{
  GtkGestureSingle parent_instance;
};

struct _GtkGestureStylusClass
{
  GtkGestureSingleClass parent_class;

  void (*proximity) (GtkGestureStylus *gesture,
                     double            x,
                     double            y);
  void (*down)      (GtkGestureStylus *gesture,
                     double            x,
                     double            y);
  void (*motion)    (GtkGestureStylus *gesture,
                     double            x,
                     double            y);
  void (*up)        (GtkGestureStylus *gesture,
                     double            x,
                     double            y);

  /*< private >*/
  gpointer padding[10];
};

