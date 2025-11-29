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

#include "gtkeventcontroller.h"

/* GdkCrossingType:
 * @GTK_CROSSING_FOCUS: Focus moved from one widget to another
 * @GTK_CROSSING_ACTIVE: The active window changed (the crossing
 *    events in this case leave from the old active window's focus
 *    location to the new active window's one.
 * @GTK_CROSSING_POINTER: The pointer moved from one widget to another
 * @GTK_CROSSING_DROP: An active drag moved from one widget to another
 *
 * We emit various kinds of crossing events when the target widget
 * for keyboard or pointer events changes.
 */
typedef enum {
  GTK_CROSSING_FOCUS,
  GTK_CROSSING_ACTIVE,
  GTK_CROSSING_POINTER,
  GTK_CROSSING_DROP
} GtkCrossingType;

/*
 * GdkCrossingirection:
 * @GTK_CROSSING_IN: the event is on the downward slope, towards the new target
 * @GTK_CROSSING_OUT: the event is on the upward slope, away from the old target
 */
typedef enum {
  GTK_CROSSING_IN,
  GTK_CROSSING_OUT
} GtkCrossingDirection;

typedef struct _GtkCrossingData GtkCrossingData;

/**
 * GtkCrossingData:
 * @type: the type of crossing event
 * @direction: whether this is a focus-in or focus-out event
 * @mode: the crossing mode
 * @old_target: the old target
 * @old_descendent: the direct child of the receiving widget that
 *   is an ancestor of @old_target, or %NULL if @old_target is not
 *   a descendent of the receiving widget
 * @new_target: the new target
 * @new_descendent: the direct child of the receiving widget that
 *   is an ancestor of @new_target, or %NULL if @new_target is not
 *   a descendent of the receiving widget
 * @drop: the `GdkDrop` if this is info for a drop operation
 *
 * The struct that is passed to gtk_event_controller_handle_crossing().
 *
 * The @old_target and @new_target fields are set to the old or new
 * focus, drop or hover location.
 */
struct _GtkCrossingData {
  GtkCrossingType type;
  GtkCrossingDirection direction;
  GdkCrossingMode mode;
  GtkWidget *old_target;
  GtkWidget *old_descendent;
  GtkWidget *new_target;
  GtkWidget *new_descendent;
  GdkDrop *drop;
};

struct _GtkEventController
{
  GObject parent_instance;
};

struct _GtkEventControllerClass
{
  GObjectClass parent_class;

  void     (* set_widget)   (GtkEventController *controller,
                             GtkWidget          *widget);
  void     (* unset_widget) (GtkEventController *controller);
  gboolean (* handle_event) (GtkEventController *controller,
                             GdkEvent            *event,
                             double              x,
                             double              y);
  void     (* reset)        (GtkEventController *controller);

  void     (* handle_crossing) (GtkEventController    *controller,
                                const GtkCrossingData *crossing,
                                double                 x,
                                double                 y);

  /*<private>*/

  /* Tells whether the event is filtered out, %TRUE makes
   * the event unseen by the handle_event vfunc.
   */
  gboolean (* filter_event) (GtkEventController *controller,
                             GdkEvent           *event);

  gpointer padding[10];
};

GtkWidget * gtk_event_controller_get_target (GtkEventController *controller);


gboolean   gtk_event_controller_handle_event   (GtkEventController *controller,
                                                GdkEvent           *event,
                                                GtkWidget          *target,
                                                double              x,
                                                double              y);
void       gtk_event_controller_handle_crossing (GtkEventController    *controller,
                                                 const GtkCrossingData *crossing,
                                                 double                 x,
                                                 double                 y);

