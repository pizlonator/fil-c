/*
 * Copyright © 2019 Red Hat, Inc
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Matthias Clasen
 */

#pragma once

#include <gtk/gtk.h>

#define CONSTRAINT_VIEW_CHILD_TYPE (constraint_view_get_type ())

G_DECLARE_TYPE (ConstraintViewChild, constraint_view_child, CONSTRAINT, VIEW_CHILD, GObject)

#define CONSTRAINT_VIEW_WIDGET_TYPE (constraint_view_widget_get_type ())

G_DECLARE_FINAL_TYPE (ConstraintViewWidget, constraint_view_widget, CONSTRAINT, VIEW_WIDGET, ConstraintViewChild)

ConstraintViewWidget * constraint_view_widget_new (void);

#define CONSTRAINT_VIEW_GUIDE_TYPE (constraint_view_guide_get_type ())

G_DECLARE_FINAL_TYPE (ConstraintViewGuide, constraint_view_guide, CONSTRAINT, VIEW_GUIDE, ConstraintViewChild)

ConstraintViewGuide * constraint_view_guide_new (void);

#define CONSTRAINT_VIEW_CONSTRAINT_TYPE (constraint_view_constraint_get_type ())

G_DECLARE_FINAL_TYPE (ConstraintViewConstraint, constraint_view_constraint, CONSTRAINT, VIEW_CONSTRAINT, ConstraintViewChild)

ConstraintViewGuide * constraint_view_constraint_new (void);
