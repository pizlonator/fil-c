/*
 * Copyright 2019 Red Hat, Inc.
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
 * Author: Matthias Clasen
 */

#pragma once

#include "gtkconstraintlayout.h"
#include "gtkconstraintsolverprivate.h"

G_BEGIN_DECLS

GtkConstraintSolver *
gtk_constraint_layout_get_solver (GtkConstraintLayout *layout);

GtkConstraintVariable *
gtk_constraint_layout_get_attribute (GtkConstraintLayout    *layout,
                                     GtkConstraintAttribute  attr,
                                     const char             *prefix,
                                     GtkWidget              *widget,
                                     GHashTable             *bound_attributes);

G_END_DECLS
