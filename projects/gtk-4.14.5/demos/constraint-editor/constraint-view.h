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

#define CONSTRAINT_VIEW_TYPE (constraint_view_get_type ())

G_MODULE_EXPORT
G_DECLARE_FINAL_TYPE (ConstraintView, constraint_view, CONSTRAINT, VIEW, GtkWidget)

ConstraintView * constraint_view_new (void);

void             constraint_view_add_child (ConstraintView *view,
                                            const char     *name);
void             constraint_view_remove_child (ConstraintView *view,
                                               GtkWidget      *child);
void             constraint_view_add_guide (ConstraintView     *view,
                                            GtkConstraintGuide *guide);
void             constraint_view_remove_guide (ConstraintView     *view,
                                               GtkConstraintGuide *guide);
void             constraint_view_guide_changed (ConstraintView     *view,
                                                GtkConstraintGuide *guide);
void             constraint_view_add_constraint (ConstraintView *view,
                                                 GtkConstraint  *constraint);
void             constraint_view_remove_constraint (ConstraintView *view,
                                                    GtkConstraint  *constraint);
GListModel *     constraint_view_get_model (ConstraintView *view);
