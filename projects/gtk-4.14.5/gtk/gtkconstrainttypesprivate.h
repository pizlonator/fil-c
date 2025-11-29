/* gtkconstrainttypesprivate.h: Private types for the constraint solver
 * Copyright 2019  GNOME Foundation
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
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
 * Author: Emmanuele Bassi
 */

#pragma once

#include <gtk/gtktypes.h>
#include <gtk/gtkenums.h>

G_BEGIN_DECLS

typedef struct _GtkConstraintVariable           GtkConstraintVariable;
typedef struct _GtkConstraintExpression         GtkConstraintExpression;
typedef struct _GtkConstraintExpressionBuilder  GtkConstraintExpressionBuilder;

/*< private >
 * GtkConstraintRef:
 *
 * A reference to a constraint stored inside the solver; while `GtkConstraint`
 * represent the public API, a `GtkConstraintRef` represents data stored inside
 * the solver. A `GtkConstraintRef` is completely opaque, and should only be
 * used to remove a constraint from the solver.
 */
typedef struct _GtkConstraintRef                GtkConstraintRef;

/*< private >
 * GtkConstraintSolver:
 *
 * A simplex solver using the Cassowary constraint solving algorithm.
 */
typedef struct _GtkConstraintSolver             GtkConstraintSolver;

G_END_DECLS
