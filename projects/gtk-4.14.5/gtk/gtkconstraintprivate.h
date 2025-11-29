/* gtkconstraintprivate.h: Constraint between two widgets
 * Copyright 2019  GNOME Foundation
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

#include "gtkconstraint.h"
#include "gtkconstrainttypesprivate.h"

G_BEGIN_DECLS

struct _GtkConstraint
{
  GObject parent_instance;

  GtkConstraintAttribute target_attribute;
  GtkConstraintAttribute source_attribute;

  GtkConstraintTarget *target;
  GtkConstraintTarget *source;

  GtkConstraintRelation relation;

  double multiplier;
  double constant;

  int strength;

  /* A reference to the real constraint inside the
   * GtkConstraintSolver, so we can remove it when
   * finalizing the GtkConstraint instance
   */
  GtkConstraintRef *constraint_ref;

  GtkConstraintSolver *solver;

  guint active : 1;
};

void    gtk_constraint_attach           (GtkConstraint       *constraint,
                                         GtkConstraintSolver *solver,
                                         GtkConstraintRef    *ref);
void    gtk_constraint_detach           (GtkConstraint       *constraint);

G_END_DECLS
