/* GTK - The GIMP Toolkit
 * Copyright (C) 2017 - Red Hat Inc.
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

#include "config.h"

#include "gtkpointerfocusprivate.h"
#include "gtkprivate.h"

static void
target_destroyed (gpointer  data,
                  GObject  *object_location)
{
  GtkPointerFocus *focus = data;

  focus->target = NULL;
  gtk_pointer_focus_repick_target (focus);
}

GtkPointerFocus *
gtk_pointer_focus_new (GtkWindow        *toplevel,
                       GtkWidget        *widget,
                       GdkDevice        *device,
                       GdkEventSequence *sequence,
                       double            x,
                       double            y)
{
  GtkPointerFocus *focus;

  focus = g_new0 (GtkPointerFocus, 1);
  focus->ref_count = 1;
  focus->toplevel = toplevel;
  focus->device = device;
  focus->sequence = sequence;
  gtk_pointer_focus_set_target (focus, widget);
  gtk_pointer_focus_set_coordinates (focus, x, y);

  return focus;
}

GtkPointerFocus *
gtk_pointer_focus_ref (GtkPointerFocus *focus)
{
  focus->ref_count++;
  return focus;
}

void
gtk_pointer_focus_unref (GtkPointerFocus *focus)
{
  focus->ref_count--;

  if (focus->ref_count == 0)
    {
      gtk_pointer_focus_set_target (focus, NULL);
      gtk_pointer_focus_set_implicit_grab (focus, NULL);
      g_free (focus);
    }
}

void
gtk_pointer_focus_set_target (GtkPointerFocus *focus,
                              GtkWidget       *target)
{
  if (focus->target == target)
    return;

  if (focus->target)
    g_object_weak_unref (G_OBJECT (focus->target), target_destroyed, focus);

  focus->target = target;

  if (focus->target)
    g_object_weak_ref (G_OBJECT (focus->target), target_destroyed, focus);
}

GtkWidget *
gtk_pointer_focus_get_target (GtkPointerFocus *focus)
{
  return focus->target;
}

void
gtk_pointer_focus_set_implicit_grab (GtkPointerFocus *focus,
                                     GtkWidget       *grab_widget)
{
  focus->grab_widget = grab_widget;
}

GtkWidget *
gtk_pointer_focus_get_implicit_grab (GtkPointerFocus *focus)
{
  return focus->grab_widget;
}

void
gtk_pointer_focus_set_coordinates (GtkPointerFocus *focus,
                                   double           x,
                                   double           y)
{
  focus->x = x;
  focus->y = y;
}

GtkWidget *
gtk_pointer_focus_get_effective_target (GtkPointerFocus *focus)
{
  GtkWidget *target;

  target = focus->target;

  if (focus->grab_widget &&
      focus->grab_widget != target &&
      !gtk_widget_is_ancestor (target, focus->grab_widget))
    target = focus->grab_widget;

  return target;
}

void
gtk_pointer_focus_repick_target (GtkPointerFocus *focus)
{
  GtkWidget *target;

  target = gtk_widget_pick (GTK_WIDGET (focus->toplevel), focus->x, focus->y, GTK_PICK_DEFAULT);
  if (target == NULL)
    target = GTK_WIDGET (focus->toplevel);
  gtk_pointer_focus_set_target (focus, target);
}
