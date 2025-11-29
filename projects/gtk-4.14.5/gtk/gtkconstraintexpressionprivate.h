/* gtkconstraintequationprivate.h: Constraint expressions and variables
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

#include "gtkconstrainttypesprivate.h"

G_BEGIN_DECLS

GtkConstraintVariable *
gtk_constraint_variable_new (const char *prefix,
                             const char *name);

GtkConstraintVariable *
gtk_constraint_variable_new_dummy (const char *name);

GtkConstraintVariable *
gtk_constraint_variable_new_objective (const char *name);

GtkConstraintVariable *
gtk_constraint_variable_new_slack (const char *name);

GtkConstraintVariable *
gtk_constraint_variable_ref (GtkConstraintVariable *variable);

void
gtk_constraint_variable_unref (GtkConstraintVariable *variable);

void
gtk_constraint_variable_set_value (GtkConstraintVariable *variable,
                                   double value);

double
gtk_constraint_variable_get_value (const GtkConstraintVariable *variable);

char *
gtk_constraint_variable_to_string (const GtkConstraintVariable *variable);

gboolean
gtk_constraint_variable_is_external (const GtkConstraintVariable *variable);

gboolean
gtk_constraint_variable_is_pivotable (const GtkConstraintVariable *variable);

gboolean
gtk_constraint_variable_is_restricted (const GtkConstraintVariable *variable);

gboolean
gtk_constraint_variable_is_dummy (const GtkConstraintVariable *variable);

typedef struct {
  GtkConstraintVariable *first;
  GtkConstraintVariable *second;
} GtkConstraintVariablePair;

GtkConstraintVariablePair *
gtk_constraint_variable_pair_new (GtkConstraintVariable *first,
                                  GtkConstraintVariable *second);

void
gtk_constraint_variable_pair_free (GtkConstraintVariablePair *pair);

typedef struct _GtkConstraintVariableSet        GtkConstraintVariableSet;

GtkConstraintVariableSet *
gtk_constraint_variable_set_new (void);

void
gtk_constraint_variable_set_free (GtkConstraintVariableSet *set);

gboolean
gtk_constraint_variable_set_add (GtkConstraintVariableSet *set,
                                 GtkConstraintVariable *variable);

gboolean
gtk_constraint_variable_set_remove (GtkConstraintVariableSet *set,
                                    GtkConstraintVariable *variable);

gboolean
gtk_constraint_variable_set_is_empty (GtkConstraintVariableSet *set);

gboolean
gtk_constraint_variable_set_is_singleton (GtkConstraintVariableSet *set);

int
gtk_constraint_variable_set_size (GtkConstraintVariableSet *set);

typedef struct {
  /*< private >*/
  gpointer dummy1;
  gpointer dummy2;
  gint64 dummy3;
} GtkConstraintVariableSetIter;

void
gtk_constraint_variable_set_iter_init (GtkConstraintVariableSetIter *iter,
                                       GtkConstraintVariableSet *set);

gboolean
gtk_constraint_variable_set_iter_next (GtkConstraintVariableSetIter *iter,
                                       GtkConstraintVariable **variable_p);

GtkConstraintExpression *
gtk_constraint_expression_new (double constant);

GtkConstraintExpression *
gtk_constraint_expression_new_from_variable (GtkConstraintVariable *variable);

GtkConstraintExpression *
gtk_constraint_expression_ref (GtkConstraintExpression *expression);

void
gtk_constraint_expression_unref (GtkConstraintExpression *expression);

GtkConstraintExpression *
gtk_constraint_expression_clone (GtkConstraintExpression *expression);

void
gtk_constraint_expression_set_constant (GtkConstraintExpression *expression,
                                        double constant);
double
gtk_constraint_expression_get_constant (const GtkConstraintExpression *expression);

gboolean
gtk_constraint_expression_is_constant (const GtkConstraintExpression *expression);

void
gtk_constraint_expression_add_expression (GtkConstraintExpression *a_expr,
                                          GtkConstraintExpression *b_expr,
                                          double n,
                                          GtkConstraintVariable *subject,
                                          GtkConstraintSolver *solver);

void
gtk_constraint_expression_add_variable (GtkConstraintExpression *expression,
                                        GtkConstraintVariable *variable,
                                        double coefficient,
                                        GtkConstraintVariable *subject,
                                        GtkConstraintSolver *solver);

void
gtk_constraint_expression_remove_variable (GtkConstraintExpression *expression,
                                           GtkConstraintVariable *variable);

void
gtk_constraint_expression_set_variable (GtkConstraintExpression *expression,
                                        GtkConstraintVariable *variable,
                                        double coefficient);

double
gtk_constraint_expression_get_coefficient (GtkConstraintExpression *expression,
                                           GtkConstraintVariable *variable);

char *
gtk_constraint_expression_to_string (const GtkConstraintExpression *expression);

double
gtk_constraint_expression_new_subject (GtkConstraintExpression *expression,
                                       GtkConstraintVariable *subject);

void
gtk_constraint_expression_change_subject (GtkConstraintExpression *expression,
                                          GtkConstraintVariable *old_subject,
                                          GtkConstraintVariable *new_subject);

void
gtk_constraint_expression_substitute_out (GtkConstraintExpression *expression,
                                          GtkConstraintVariable *out_var,
                                          GtkConstraintExpression *expr,
                                          GtkConstraintVariable *subject,
                                          GtkConstraintSolver *solver);

GtkConstraintVariable *
gtk_constraint_expression_get_pivotable_variable (GtkConstraintExpression *expression);

GtkConstraintExpression *
gtk_constraint_expression_plus_constant (GtkConstraintExpression *expression,
                                         double constant);

GtkConstraintExpression *
gtk_constraint_expression_minus_constant (GtkConstraintExpression *expression,
                                          double constant);

GtkConstraintExpression *
gtk_constraint_expression_plus_variable (GtkConstraintExpression *expression,
                                         GtkConstraintVariable *variable);

GtkConstraintExpression *
gtk_constraint_expression_minus_variable (GtkConstraintExpression *expression,
                                          GtkConstraintVariable *variable);

GtkConstraintExpression *
gtk_constraint_expression_multiply_by (GtkConstraintExpression *expression,
                                       double factor);

GtkConstraintExpression *
gtk_constraint_expression_divide_by (GtkConstraintExpression *expression,
                                     double factor);

struct _GtkConstraintExpressionBuilder
{
  /*< private >*/
  gpointer dummy1;
  gpointer dummy2;
  int dummy3;
};

void
gtk_constraint_expression_builder_init (GtkConstraintExpressionBuilder *builder,
                                        GtkConstraintSolver *solver);

void
gtk_constraint_expression_builder_term (GtkConstraintExpressionBuilder *builder,
                                        GtkConstraintVariable *term);

void
gtk_constraint_expression_builder_plus (GtkConstraintExpressionBuilder *builder);

void
gtk_constraint_expression_builder_minus (GtkConstraintExpressionBuilder *builder);

void
gtk_constraint_expression_builder_divide_by (GtkConstraintExpressionBuilder *builder);

void
gtk_constraint_expression_builder_multiply_by (GtkConstraintExpressionBuilder *builder);

void
gtk_constraint_expression_builder_constant (GtkConstraintExpressionBuilder *builder,
                                            double value);

GtkConstraintExpression *
gtk_constraint_expression_builder_finish (GtkConstraintExpressionBuilder *builder) G_GNUC_WARN_UNUSED_RESULT;

/*< private >
 * GtkConstraintExpressionIter:
 *
 * An iterator object for terms inside a `GtkConstraintExpression`.
 */
typedef struct {
  /*< private >*/
  gpointer dummy1;
  gpointer dummy2;
  gint64 dummy3;
} GtkConstraintExpressionIter;

void
gtk_constraint_expression_iter_init (GtkConstraintExpressionIter *iter,
                                     GtkConstraintExpression *equation);

gboolean
gtk_constraint_expression_iter_next (GtkConstraintExpressionIter *iter,
                                     GtkConstraintVariable **variable,
                                     double *coefficient);

gboolean
gtk_constraint_expression_iter_prev (GtkConstraintExpressionIter *iter,
                                     GtkConstraintVariable **variable,
                                     double *coefficient);

G_END_DECLS
