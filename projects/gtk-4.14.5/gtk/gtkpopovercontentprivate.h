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

#pragma once

#include "gtkwidget.h"
#include "gtkenums.h"

#define GTK_TYPE_POPOVER_CONTENT                 (gtk_popover_content_get_type ())
#define GTK_POPOVER_CONTENT(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_POPOVER_CONTENT, GtkPopoverContent))
#define GTK_POPOVER_CONTENT_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_POPOVER_CONTENT, GtkPopoverContentClass))
#define GTK_IS_POPOVER_CONTENT(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_POPOVER_CONTENT))
#define GTK_IS_POPOVER_CONTENT_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_POPOVER_CONTENT))
#define GTK_POPOVER_CONTENT_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_POPOVER_CONTENT, GtkPopoverContentClass))

typedef struct _GtkPopoverContent             GtkPopoverContent;
typedef struct _GtkPopoverContentClass        GtkPopoverContentClass;

struct _GtkPopoverContent
{
  GtkWidget parent_instance;
};

struct _GtkPopoverContentClass
{
  GtkWidgetClass parent_class;
};

GType      gtk_popover_content_get_type (void) G_GNUC_CONST;

GtkWidget *gtk_popover_content_new (void);

