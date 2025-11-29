/* GTK - The GIMP Toolkit
 * Copyright (C) 2012, One Laptop Per Child.
 * Copyright (C) 2014, Red Hat, Inc.
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
#include "gtkgestureswipe.h"

struct _GtkGestureSwipe
{
  GtkGestureSingle parent_instance;
};

struct _GtkGestureSwipeClass
{
  GtkGestureSingleClass parent_class;

  void (* swipe) (GtkGestureSwipe *gesture,
                  double           velocity_x,
                  double           velocity_y);

  /*< private >*/
  gpointer padding[10];
};

