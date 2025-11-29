/* GTK - The GIMP Toolkit
 * Copyright (C) 2021 Red Hat, Inc.
 *
 * Authors:
 * - Matthias Clasen <mclasen@redhat.com>
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

#include "gtkpopovercontentprivate.h"

#include "gtkcssstylechangeprivate.h"
#include "gtkwidgetprivate.h"
#include "gtkprivate.h"

/* A private class used as the child of GtkPopover. The only thing
 * special here is that we need to queue a resize on the popover when
 * our shadow changes.
 */

G_DEFINE_TYPE (GtkPopoverContent, gtk_popover_content, GTK_TYPE_WIDGET);

static void
gtk_popover_content_css_changed (GtkWidget         *widget,
                                 GtkCssStyleChange *change)
{
  GTK_WIDGET_CLASS (gtk_popover_content_parent_class)->css_changed (widget, change);

  if (change == NULL ||
      gtk_css_style_change_changes_property (change, GTK_CSS_PROPERTY_BOX_SHADOW))
    gtk_widget_queue_resize (gtk_widget_get_parent (widget));
}

static void
gtk_popover_content_finalize (GObject *object)
{
  GtkPopoverContent *self = GTK_POPOVER_CONTENT (object);
  GtkWidget *widget;

  widget = _gtk_widget_get_first_child (GTK_WIDGET (self));
  while (widget != NULL)
    {
      GtkWidget *next = _gtk_widget_get_next_sibling (widget);

      gtk_widget_unparent (widget);

      widget = next;
    }

  G_OBJECT_CLASS (gtk_popover_content_parent_class)->finalize (object);
}

static void
gtk_popover_content_class_init (GtkPopoverContentClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gtk_popover_content_finalize;

  widget_class->focus = gtk_widget_focus_child;
  widget_class->grab_focus = gtk_widget_grab_focus_child;
  widget_class->css_changed = gtk_popover_content_css_changed;

  gtk_widget_class_set_css_name (widget_class, I_("contents"));
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_GENERIC);
}

static void
gtk_popover_content_init (GtkPopoverContent *self)
{
}

GtkWidget *
gtk_popover_content_new (void)
{
  return g_object_new (GTK_TYPE_POPOVER_CONTENT, NULL);
}
