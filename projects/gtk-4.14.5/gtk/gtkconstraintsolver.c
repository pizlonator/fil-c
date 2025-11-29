/* gtkconstraintsolver.c: Constraint solver based on the Cassowary method
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

/*< private >
 *  GtkConstraintSolver
 *
 * GtkConstraintSolver is an object that encodes constraints into a tableau
 * of linear equations and solves them, using an incremental optimization
 * algorithm known as the "Cassowary Linear Arithmetic Constraint Solving
 * Algorithm" (Badros, Borning & Stuckey 2001).
 *
 * Each constraint is expressed as a linear equation, whose terms are variables
 * containing widget attributes like the width, height, or position; the simplex
 * solver takes all the constraints and incrementally optimizes the tableau to
 * replace known terms; additionally, the algorithm will try to assign a value
 * to all remaining variables in order to satisfy the various constraints.
 *
 * Each constraint is given a "strength", which determines whether satisfying
 * the constraint is required in order to solve the tableau or not.
 *
 * A typical example of GtkConstraintSolver use is solving the following
 * system of constraints:
 *
 *  - [required] right = left + 10
 *  - [required] right ≤ 100
 *  - [required] middle = left + right / 2
 *  - [required] left ≥ 0
 *
 * Once we create a GtkConstraintSolver instance, we need to create the
 * various variables and expressions that represent the values we want to
 * compute and the constraints we wish to impose on the solutions:
 *
 * |[
 *   GtkConstraintSolver *solver = gtk_constraint_solver_new ();
 *
 *   // Our variables
 *   GtkConstraintVariable *left =
 *     gtk_constraint_solver_create_variable (solver, NULL, "left", 0.0);
 *   GtkConstraintVariable *middle =
 *     gtk_constraint_solver_create_variable (solver, NULL, "middle", 0.0);
 *   GtkConstraintVariable *right  =
 *     gtk_constraint_solver_create_variable (solver, NULL, "right", 0.0);
 *
 *   // Our constraints
 *   GtkConstraintExpressionBuilder builder;
 *   GtkConstraintExpression *e;
 *
 *   // right = left + 10
 *   gtk_constraint_expression_builder_init (&builder, solver);
 *   gtk_constraint_expression_builder_term (&builder, left);
 *   gtk_constraint_expression_builder_plus (&builder);
 *   gtk_constraint_expression_builder_constant (&builder, 10.0);
 *   e = gtk_constraint_expression_builder_finish (&builder);
 *   gtk_constraint_solver_add_constraint (solver,
 *                                         right, GTK_CONSTRAINT_RELATION_EQ, e,
 *                                         GTK_CONSTRAINT_STRENGTH_REQUIRED);
 *
 *   // right ≤ 100
 *   gtk_constraint_expression_builder_constant (&builder, 100.0);
 *   e = gtk_constraint_expression_builder_finish (&builder);
 *   gtk_constraint_solver_add_constraint (solver,
 *                                         right, GTK_CONSTRAINT_RELATION_LE, e,
 *                                         GTK_CONSTRAINT_STRENGTH_REQUIRED);
 *
 *   // middle = (left + right) / 2
 *   gtk_constraint_expression_builder_term (&builder, left);
 *   gtk_constraint_expression_builder_plus (&builder)
 *   gtk_constraint_expression_builder_term (&builder, right);
 *   gtk_constraint_expression_builder_divide_by (&builder);
 *   gtk_constraint_expression_builder_constant (&builder, 2.0);
 *   e = gtk_constraint_expression_builder_finish (&builder);
 *   gtk_constraint_solver_add_constraint (solver
 *                                         middle, GTK_CONSTRAINT_RELATION_EQ, e,
 *                                         GTK_CONSTRAINT_STRENGTH_REQUIRED);
 *
 *   // left ≥ 0
 *   gtk_constraint_expression_builder_constant (&builder, 0.0);
 *   e = gtk_constraint_expression_builder_finish (&builder);
 *   gtk_constraint_solver_add_constraint (solver,
 *                                         left, GTK_CONSTRAINT_RELATION_GE, e,
 *                                         GTK_CONSTRAINT_STRENGTH_REQUIRED);
 * ]|
 *
 * Now that we have all our constraints in place, suppose we wish to find
 * the values of `left` and `right` if we specify the value of `middle`. In
 * order to do that, we need to add an additional "stay" constraint, i.e.
 * a constraint that tells the solver to optimize for the solution that keeps
 * the variable in place:
 *
 * |[
 *   // Set the value first
 *   gtk_constraint_variable_set_value (middle, 45.0);
 *   // and then add the stay constraint, with a weak strength
 *   gtk_constraint_solver_add_stay_variable (solver, middle, GTK_CONSTRAINT_STRENGTH_WEAK);
 * ]|
 *
 * GtkConstraintSolver incrementally solves the system every time a constraint
 * is added or removed, so it's possible to query the values of the variables
 * immediately afterward:
 *
 * |[
 *   double left_val = gtk_constraint_variable_get_value (left);
 *   double right_val = gtk_constraint_variable_get_value (right);
 *   double middle_val = gtk_constraint_variable_get_value (middle);
 *
 *   // These are the values computed by the solver:
 *   g_assert_cmpfloat_with_epsilon (left_val, 40.0, 0.001);
 *   g_assert_cmpfloat_with_epsilon (middle_val, 45.0, 0.001);
 *   g_assert_cmpfloat_with_epsilon (right_val, 50.0, 0.001);
 * ]|
 *
 * As you can see:
 *
 *  - the middle value hasn't changed
 *  - the left value is ≥ 0
 *  - the right value is ≤ 100
 *  - the right value is left + 10
 *  - the middle value is (left + right) / 2.0
 *
 * For more information about the Cassowary constraint solving algorithm and
 * toolkit, see the following papers:
 *
 *  - Badros G & Borning A, 1998, 'Cassowary Linear Arithmetic Constraint
 *    Solving Algorithm: Interface and Implementation', Technical Report
 *    UW-CSE-98-06-04, June 1998 (revised July 1999)
 *    https://constraints.cs.washington.edu/cassowary/cassowary-tr.pdf
 *  - Badros G, Borning A & Stuckey P, 2001, 'Cassowary Linear Arithmetic
 *    Constraint Solving Algorithm', ACM Transactions on Computer-Human
 *    Interaction, vol. 8 no. 4, December 2001, pages 267-306
 *    https://constraints.cs.washington.edu/solvers/cassowary-tochi.pdf
 *
 * The following implementation is based on these projects:
 *
 *  - the original [C++ implementation](https://sourceforge.net/projects/cassowary/)
 *  - the JavaScript port [Cassowary.js](https://github.com/slightlyoff/cassowary.js)
 *  - the Python port [Cassowary](https://github.com/pybee/cassowary)
 */

#include "config.h"

#include "gtkconstraintsolverprivate.h"
#include "gtkconstraintexpressionprivate.h"

#include "gtkdebug.h"
#include "gtkprivate.h"

#include <glib.h>
#include <string.h>
#include <math.h>
#include <float.h>

struct _GtkConstraintRef
{
  /* The constraint's normal form inside the solver:
   *
   *   x - (y × coefficient + constant) = 0
   *
   * We only use equalities, and replace inequalities with slack
   * variables.
   */
  GtkConstraintExpression *expression;

  /* A constraint variable, only used by stay and edit constraints */
  GtkConstraintVariable *variable;

  /* The original relation used when creating the constraint */
  GtkConstraintRelation relation;

  /* The strength of the constraint; this value is used to strengthen
   * or weaken a constraint weight in the tableau when coming to a
   * solution
   */
  int strength;

  GtkConstraintSolver *solver;

  guint is_edit : 1;
  guint is_stay : 1;
};

typedef struct {
  GtkConstraintRef *constraint;

  GtkConstraintVariable *eplus;
  GtkConstraintVariable *eminus;

  double prev_constant;
} EditInfo;

typedef struct {
  GtkConstraintRef *constraint;
} StayInfo;

struct _GtkConstraintSolver
{
  GObject parent_instance;

  /* HashTable<Variable, VariableSet>; owns keys and values */
  GHashTable *columns;
  /* HashTable<Variable, Expression>; owns keys and values */
  GHashTable *rows;

  /* Set<Variable>; does not own keys */
  GHashTable *external_rows;
  /* Set<Variable>; does not own keys */
  GHashTable *external_parametric_vars;

  /* Vec<Variable> */
  GPtrArray *infeasible_rows;
  /* Vec<VariablePair>; owns the pair */
  GPtrArray *stay_error_vars;

  /* HashTable<Constraint, VariableSet>; owns the set */
  GHashTable *error_vars;
  /* HashTable<Constraint, Variable> */
  GHashTable *marker_vars;

  /* HashTable<Variable, EditInfo>; does not own keys, but owns values */
  GHashTable *edit_var_map;
  /* HashTable<Variable, StayInfo>; does not own keys, but owns values */
  GHashTable *stay_var_map;

  GtkConstraintVariable *objective;

  /* Set<Constraint>; owns the key */
  GHashTable *constraints;

  /* Counters */
  int var_counter;
  int slack_counter;
  int artificial_counter;
  int dummy_counter;
  int optimize_count;
  int freeze_count;

  /* Bitfields; keep at the end */
  guint auto_solve : 1;
  guint needs_solving : 1;
  guint in_edit_phase : 1;
};

static void gtk_constraint_ref_free (GtkConstraintRef *ref);
static void edit_info_free (gpointer data);

G_DEFINE_TYPE (GtkConstraintSolver, gtk_constraint_solver, G_TYPE_OBJECT)

static void
gtk_constraint_solver_finalize (GObject *gobject)
{
  GtkConstraintSolver *self = GTK_CONSTRAINT_SOLVER (gobject);

  g_hash_table_remove_all (self->constraints);
  g_clear_pointer (&self->constraints, g_hash_table_unref);

  g_clear_pointer (&self->stay_error_vars, g_ptr_array_unref);
  g_clear_pointer (&self->infeasible_rows, g_ptr_array_unref);

  g_clear_pointer (&self->external_rows, g_hash_table_unref);
  g_clear_pointer (&self->external_parametric_vars, g_hash_table_unref);
  g_clear_pointer (&self->error_vars, g_hash_table_unref);
  g_clear_pointer (&self->marker_vars, g_hash_table_unref);
  g_clear_pointer (&self->edit_var_map, g_hash_table_unref);
  g_clear_pointer (&self->stay_var_map, g_hash_table_unref);

  g_clear_pointer (&self->rows, g_hash_table_unref);
  g_clear_pointer (&self->columns, g_hash_table_unref);

  G_OBJECT_CLASS (gtk_constraint_solver_parent_class)->finalize (gobject);
}

static void
gtk_constraint_solver_class_init (GtkConstraintSolverClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = gtk_constraint_solver_finalize;
}

static void
gtk_constraint_solver_init (GtkConstraintSolver *self)
{
  self->columns =
    g_hash_table_new_full (NULL, NULL,
                           (GDestroyNotify) gtk_constraint_variable_unref,
                           (GDestroyNotify) gtk_constraint_variable_set_free);

  self->rows =
    g_hash_table_new_full (NULL, NULL,
                           (GDestroyNotify) gtk_constraint_variable_unref,
                           (GDestroyNotify) gtk_constraint_expression_unref);

  self->external_rows = g_hash_table_new (NULL, NULL);

  self->external_parametric_vars = g_hash_table_new (NULL, NULL);

  self->infeasible_rows = g_ptr_array_new ();

  self->stay_error_vars =
    g_ptr_array_new_with_free_func ((GDestroyNotify) gtk_constraint_variable_pair_free);

  self->error_vars =
    g_hash_table_new_full (NULL, NULL,
                           NULL,
                           (GDestroyNotify) gtk_constraint_variable_set_free);

  self->marker_vars = g_hash_table_new (NULL, NULL);

  self->edit_var_map = g_hash_table_new_full (NULL, NULL,
                                              NULL,
                                              edit_info_free);

  self->stay_var_map = g_hash_table_new_full (NULL, NULL,
                                              NULL,
                                              g_free);

  /* The rows table owns the objective variable */
  self->objective = gtk_constraint_variable_new_objective ("Z");
  g_hash_table_insert (self->rows,
                       self->objective,
                       gtk_constraint_expression_new (0.0));

  self->constraints =
    g_hash_table_new_full (NULL, NULL,
                           (GDestroyNotify) gtk_constraint_ref_free,
                           NULL);

  self->slack_counter = 0;
  self->dummy_counter = 0;
  self->artificial_counter = 0;
  self->freeze_count = 0;

  self->needs_solving = FALSE;
  self->auto_solve = TRUE;
}

static void
gtk_constraint_ref_free (GtkConstraintRef *self)
{
  gtk_constraint_solver_remove_constraint (self->solver, self);

  gtk_constraint_expression_unref (self->expression);

  if (self->is_edit || self->is_stay)
    {
      g_assert (self->variable != NULL);
      gtk_constraint_variable_unref (self->variable);
    }

  g_free (self);
}

static gboolean
gtk_constraint_ref_is_inequality (const GtkConstraintRef *self)
{
  return self->relation != GTK_CONSTRAINT_RELATION_EQ;
}

static gboolean
gtk_constraint_ref_is_required (const GtkConstraintRef *self)
{
  return self->strength == GTK_CONSTRAINT_STRENGTH_REQUIRED;
}

static const char *relations[] = {
  "<=",
  "==",
  ">=",
};

static const char *
relation_to_string (GtkConstraintRelation r)
{
  return relations[r + 1];
}

static const char *
strength_to_string (int s)
{
  if (s >= GTK_CONSTRAINT_STRENGTH_STRONG)
    return "strong";

  if (s >= GTK_CONSTRAINT_STRENGTH_MEDIUM)
    return "medium";

  return "weak";
}

static char *
gtk_constraint_ref_to_string (const GtkConstraintRef *self)
{
  GString *buf = g_string_new (NULL);
  char *str;

  if (self->is_stay)
    g_string_append (buf, "[stay]");
  else if (self->is_edit)
    g_string_append (buf, "[edit]");

  str = gtk_constraint_expression_to_string (self->expression);
  g_string_append (buf, str);
  g_free (str);

  g_string_append_c (buf, ' ');
  g_string_append (buf, relation_to_string (self->relation));
  g_string_append (buf, " 0.0");

  if (gtk_constraint_ref_is_required (self))
    g_string_append (buf, " [strength:required]");
  else
    g_string_append_printf (buf, " [strength:%d (%s)]",
                            self->strength,
                            strength_to_string (self->strength));

  return g_string_free (buf, FALSE);
}

static GtkConstraintVariableSet *
gtk_constraint_solver_get_column_set (GtkConstraintSolver *self,
                                      GtkConstraintVariable *param_var)
{
  return g_hash_table_lookup (self->columns, param_var);
}

static gboolean
gtk_constraint_solver_column_has_key (GtkConstraintSolver *self,
                                      GtkConstraintVariable *subject)
{
  return g_hash_table_contains (self->columns, subject);
}

static void
gtk_constraint_solver_insert_column_variable (GtkConstraintSolver *self,
                                              GtkConstraintVariable *param_var,
                                              GtkConstraintVariable *row_var)
{
  GtkConstraintVariableSet *cset = g_hash_table_lookup (self->columns, param_var);

  if (cset == NULL)
    {
      cset = gtk_constraint_variable_set_new ();
      g_hash_table_insert (self->columns, gtk_constraint_variable_ref (param_var), cset);
    }

  if (row_var != NULL)
    gtk_constraint_variable_set_add (cset, row_var);
}

static void
gtk_constraint_solver_insert_error_variable (GtkConstraintSolver *self,
                                             GtkConstraintRef *constraint,
                                             GtkConstraintVariable *variable)
{
  GtkConstraintVariableSet *cset = g_hash_table_lookup (self->error_vars, constraint);

  if (cset == NULL)
    {
      cset = gtk_constraint_variable_set_new ();
      g_hash_table_insert (self->error_vars, constraint, cset);
    }

  gtk_constraint_variable_set_add (cset, variable);
}

static void
gtk_constraint_solver_reset_stay_constants (GtkConstraintSolver *self)
{
  int i;

  for (i = 0; i < self->stay_error_vars->len; i++)
    {
      GtkConstraintVariablePair *pair = g_ptr_array_index (self->stay_error_vars, i);
      GtkConstraintExpression *expression;

      expression = g_hash_table_lookup (self->rows, pair->first);

      if (expression == NULL)
        expression = g_hash_table_lookup (self->rows, pair->second);

      if (expression != NULL)
        gtk_constraint_expression_set_constant (expression, 0.0);
    }
}

static void
gtk_constraint_solver_set_external_variables (GtkConstraintSolver *self)
{
  GHashTableIter iter;
  gpointer key_p;

  g_hash_table_iter_init (&iter, self->external_parametric_vars);
  while (g_hash_table_iter_next (&iter, &key_p, NULL))
    {
      GtkConstraintVariable *variable = key_p;

      if (g_hash_table_contains (self->rows, variable))
        continue;

      gtk_constraint_variable_set_value (variable, 0.0);
    }

  g_hash_table_iter_init (&iter, self->external_rows);
  while (g_hash_table_iter_next (&iter, &key_p, NULL))
    {
      GtkConstraintVariable *variable = key_p;
      GtkConstraintExpression *expression;
      double constant;

      expression = g_hash_table_lookup (self->rows, variable);
      constant = gtk_constraint_expression_get_constant (expression);

      gtk_constraint_variable_set_value (variable, constant);
    }

  self->needs_solving = FALSE;
}

static void
gtk_constraint_solver_add_row (GtkConstraintSolver *self,
                               GtkConstraintVariable *variable,
                               GtkConstraintExpression *expression)
{
  GtkConstraintExpressionIter iter;
  GtkConstraintVariable *t_v;
  double t_c;

  g_hash_table_insert (self->rows,
                       gtk_constraint_variable_ref (variable),
                       gtk_constraint_expression_ref (expression));

  gtk_constraint_expression_iter_init (&iter, expression);
  while (gtk_constraint_expression_iter_next (&iter, &t_v, &t_c))
    {
      gtk_constraint_solver_insert_column_variable (self, t_v, variable);

      if (gtk_constraint_variable_is_external (t_v))
        g_hash_table_add (self->external_parametric_vars, t_v);
    }

  if (gtk_constraint_variable_is_external (variable))
    g_hash_table_add (self->external_rows, variable);
}

static void
gtk_constraint_solver_remove_column (GtkConstraintSolver *self,
                                     GtkConstraintVariable *variable)
{
  GtkConstraintVariable *v;
  GtkConstraintVariableSetIter iter;
  GtkConstraintVariableSet *cset;

  /* Take a reference on the variable, as we're going to remove it
   * from various maps and we want to guarantee the pointer is
   * valid until we leave this function
   */
  gtk_constraint_variable_ref (variable);

  cset = g_hash_table_lookup (self->columns, variable);
  if (cset == NULL)
    goto out;

  gtk_constraint_variable_set_iter_init (&iter, cset);
  while (gtk_constraint_variable_set_iter_next (&iter, &v))
    {
      GtkConstraintExpression *e = g_hash_table_lookup (self->rows, v);

      gtk_constraint_expression_remove_variable (e, variable);
    }

  g_hash_table_remove (self->columns, variable);

out:
  if (gtk_constraint_variable_is_external (variable))
    {
      g_hash_table_remove (self->external_rows, variable);
      g_hash_table_remove (self->external_parametric_vars, variable);
    }

  gtk_constraint_variable_unref (variable);
}

static GtkConstraintExpression *
gtk_constraint_solver_remove_row (GtkConstraintSolver *self,
                                  GtkConstraintVariable *variable,
                                  gboolean free_res)
{
  GtkConstraintExpression *e;
  GtkConstraintExpressionIter iter;
  GtkConstraintVariable *t_v;
  double t_c;

  e = g_hash_table_lookup (self->rows, variable);
  g_assert (e != NULL);

  gtk_constraint_expression_ref (e);

  gtk_constraint_expression_iter_init (&iter, e);
  while (gtk_constraint_expression_iter_next (&iter, &t_v, &t_c))
    {
      GtkConstraintVariableSet *cset = g_hash_table_lookup (self->columns, t_v);

      if (cset != NULL)
        gtk_constraint_variable_set_remove (cset, variable);
    }

  g_ptr_array_remove (self->infeasible_rows, variable);

  if (gtk_constraint_variable_is_external (variable))
    g_hash_table_remove (self->external_rows, variable);

  g_hash_table_remove (self->rows, variable);

  if (free_res)
    {
      gtk_constraint_expression_unref (e);
      return NULL;
    }

  return e;
}

/*< private >
 * gtk_constraint_solver_substitute_out:
 * @self: a `GtkConstraintSolver`
 * @old_variable: a `GtkConstraintVariable`
 * @expression: a `GtkConstraintExpression`
 *
 * Replaces @old_variable in every row of the tableau with @expression.
 */
static void
gtk_constraint_solver_substitute_out (GtkConstraintSolver *self,
                                      GtkConstraintVariable *old_variable,
                                      GtkConstraintExpression *expression)
{
  GtkConstraintVariableSet *cset = g_hash_table_lookup (self->columns, old_variable);
  if (cset != NULL)
    {
      GtkConstraintVariableSetIter iter;
      GtkConstraintVariable *v;

      gtk_constraint_variable_set_iter_init (&iter, cset);
      while (gtk_constraint_variable_set_iter_next (&iter, &v))
        {
          GtkConstraintExpression *row = g_hash_table_lookup (self->rows, v);

          gtk_constraint_expression_substitute_out (row, old_variable, expression, v, self);

          if (gtk_constraint_variable_is_restricted (v) &&
              gtk_constraint_expression_get_constant (row) < 0)
            g_ptr_array_add (self->infeasible_rows, v);
        }
    }

  if (gtk_constraint_variable_is_external (old_variable))
    {
      g_hash_table_add (self->external_rows, old_variable);
      g_hash_table_remove (self->external_parametric_vars, old_variable);
    }

  g_hash_table_remove (self->columns, old_variable);
}

/*< private >
 * gtk_constraint_solver_pivot:
 * @self: a `GtkConstraintSolver`
 * @entry_var: a `GtkConstraintVariable`
 * @exit_var: a `GtkConstraintVariable`
 *
 * Pivots the `GtkConstraintSolver`.
 *
 * This function will move @entry_var into the basis of the tableau,
 * making it a basic variable; and move @exit_var out of the basis of
 * the tableau, making it a parametric variable.
 */
static void
gtk_constraint_solver_pivot (GtkConstraintSolver *self,
                             GtkConstraintVariable *entry_var,
                             GtkConstraintVariable *exit_var)
{
  GtkConstraintExpression *expr;

  if (entry_var != NULL)
    gtk_constraint_variable_ref (entry_var);
  else
    g_critical ("INTERNAL: invalid entry variable during pivot");

  if (exit_var != NULL)
    gtk_constraint_variable_ref (exit_var);
  else
    g_critical ("INTERNAL: invalid exit variable during pivot");

  /* We keep a reference to the expression */
  expr = gtk_constraint_solver_remove_row (self, exit_var, FALSE);

  gtk_constraint_expression_change_subject (expr, exit_var, entry_var);
  gtk_constraint_solver_substitute_out (self, entry_var, expr);

  if (gtk_constraint_variable_is_external (entry_var))
    g_hash_table_remove (self->external_parametric_vars, entry_var);

  gtk_constraint_solver_add_row (self, entry_var, expr);

  gtk_constraint_variable_unref (entry_var);
  gtk_constraint_variable_unref (exit_var);
  gtk_constraint_expression_unref (expr);
}

static void
gtk_constraint_solver_optimize (GtkConstraintSolver *self,
                                GtkConstraintVariable *z)
{
  GtkConstraintVariable *entry = NULL, *exit = NULL;
  GtkConstraintExpression *z_row = g_hash_table_lookup (self->rows, z);
  gint64 start_time = g_get_monotonic_time ();

  g_assert (z_row != NULL);

  self->optimize_count += 1;

  if (GTK_DEBUG_CHECK (CONSTRAINTS))
    {
      char *str = gtk_constraint_variable_to_string (z);
      g_message ("optimize: %s", str);
      g_free (str);
    }

  while (TRUE)
    {
      GtkConstraintVariableSet *column_vars;
      GtkConstraintVariableSetIter viter;
      GtkConstraintExpressionIter eiter;
      GtkConstraintVariable *t_v, *v;
      double t_c;
      double objective_coefficient = 0.0;
      double min_ratio;

      gtk_constraint_expression_iter_init (&eiter, z_row);
      while (gtk_constraint_expression_iter_prev (&eiter, &t_v, &t_c))
        {
          if (gtk_constraint_variable_is_pivotable (t_v) && t_c < objective_coefficient)
            {
              entry = t_v;
              objective_coefficient = t_c;
              break;
            }
        }

      if (objective_coefficient >= -1e-8)
        break;

      min_ratio = DBL_MAX;

      column_vars = gtk_constraint_solver_get_column_set (self, entry);
      gtk_constraint_variable_set_iter_init (&viter, column_vars);
      while (gtk_constraint_variable_set_iter_next (&viter, &v))
        {
          if (gtk_constraint_variable_is_pivotable (v))
            {
              GtkConstraintExpression *expr = g_hash_table_lookup (self->rows, v);
              double coeff = gtk_constraint_expression_get_coefficient (expr, entry);

              if (coeff < 0.0)
                {
                  double constant = gtk_constraint_expression_get_constant (expr);

                  double r = -1.0 * constant / coeff;
                  if (r < min_ratio)
                    {
                      min_ratio = r;
                      exit = v;
                    }
                }
            }
        }

      if (min_ratio == DBL_MAX)
        {
          GTK_DEBUG (CONSTRAINTS, "Unbounded objective variable during optimization");
          break;
        }

      if (GTK_DEBUG_CHECK (CONSTRAINTS))
        {
          char *entry_s = gtk_constraint_variable_to_string (entry);
          char *exit_s = gtk_constraint_variable_to_string (exit);
          g_message ("pivot(entry: %s, exit: %s)", entry_s, exit_s);
          g_free (entry_s);
          g_free (exit_s);
        }

      gtk_constraint_solver_pivot (self, entry, exit);
    }

  GTK_DEBUG (CONSTRAINTS, "solver.optimize.time := %.3f ms (pass: %d)",
                          (float) (g_get_monotonic_time () - start_time) / 1000.f,
                          self->optimize_count);
}

/*< private >
 * gtk_constraint_solver_new_expression:
 * @self: a `GtkConstraintSolver`
 * @constraint: a `GtkConstraintRef`
 * @eplus_p: (out) (optional): the positive error variable
 * @eminus_p: (out) (optional): the negative error variable
 * @prev_constant_p: the constant part of the @constraint's expression
 *
 * Creates a new expression for the @constraint, replacing
 * any basic variable with their expressions, and normalizing
 * the terms to avoid a negative constant.
 *
 * If the @constraint is not required, this function will add
 * error variables with the appropriate weight to the tableau.
 *
 * Returns: (transfer full): the new expression for the constraint
 */
static GtkConstraintExpression *
gtk_constraint_solver_new_expression (GtkConstraintSolver *self,
                                      GtkConstraintRef *constraint,
                                      GtkConstraintVariable **eplus_p,
                                      GtkConstraintVariable **eminus_p,
                                      double *prev_constant_p)
{
  GtkConstraintExpression *cn_expr = constraint->expression;
  GtkConstraintExpression *expr;
  GtkConstraintExpressionIter eiter;
  GtkConstraintVariable *t_v;
  double t_c;

  if (eplus_p != NULL)
    *eplus_p = NULL;
  if (eminus_p != NULL)
    *eminus_p = NULL;
  if (prev_constant_p != NULL)
    *prev_constant_p = 0.0;

  expr = gtk_constraint_expression_new (gtk_constraint_expression_get_constant (cn_expr));

  gtk_constraint_expression_iter_init (&eiter, cn_expr);
  while (gtk_constraint_expression_iter_next (&eiter, &t_v, &t_c))
    {
      GtkConstraintExpression *e = g_hash_table_lookup (self->rows, t_v);

      if (e == NULL)
        gtk_constraint_expression_add_variable (expr, t_v, t_c, NULL, self);
      else
        gtk_constraint_expression_add_expression (expr, e, t_c, NULL, self);
    }

  if (gtk_constraint_ref_is_inequality (constraint))
    {
      GtkConstraintVariable *slack_var;

      /* If the constraint is an inequality, we add a slack variable to
       * turn it into an equality, e.g. from
       *
       *   expr ≥ 0
       *
       * to
       *
       *   expr - slack = 0
       *
       * Additionally, if the constraint is not required we add an
       * error variable with the weight of the constraint:
       *
       *   expr - slack + error = 0
       */
      self->slack_counter += 1;

      slack_var = gtk_constraint_variable_new_slack ("s");
      gtk_constraint_expression_set_variable (expr, slack_var, -1.0);
      gtk_constraint_variable_unref (slack_var);

      g_hash_table_insert (self->marker_vars, constraint, slack_var);

      if (!gtk_constraint_ref_is_required (constraint))
        {
          GtkConstraintExpression *z_row;
          GtkConstraintVariable *eminus;

          self->slack_counter += 1;

          eminus = gtk_constraint_variable_new_slack ("em");
          gtk_constraint_expression_set_variable (expr, eminus, 1.0);
          gtk_constraint_variable_unref (eminus);

          z_row = g_hash_table_lookup (self->rows, self->objective);
          gtk_constraint_expression_set_variable (z_row, eminus, constraint->strength);

          gtk_constraint_solver_insert_error_variable (self, constraint, eminus);
          gtk_constraint_solver_note_added_variable (self, eminus, self->objective);
          gtk_constraint_variable_unref (eminus);
        }
    }
  else
    {
      GtkConstraintVariable *dummy_var;

      if (gtk_constraint_ref_is_required (constraint))
        {
          /* If the constraint is required, we use a dummy marker variable;
           * the dummy won't be allowed to enter the basis of the tableau
           * when pivoting.
           */
          self->dummy_counter += 1;

          dummy_var = gtk_constraint_variable_new_dummy ("dummy");

          if (eplus_p != NULL)
            *eplus_p = dummy_var;
          if (eminus_p != NULL)
            *eminus_p = dummy_var;
          if (prev_constant_p != NULL)
            *prev_constant_p = gtk_constraint_expression_get_constant (cn_expr);

          gtk_constraint_expression_set_variable (expr, dummy_var, 1.0);
          g_hash_table_insert (self->marker_vars, constraint, dummy_var);

          gtk_constraint_variable_unref (dummy_var);
        }
      else
        {
          GtkConstraintVariable *eplus, *eminus;
          GtkConstraintExpression *z_row;

          /* Since the constraint is a non-required equality, we need to
           * add error variables around it, i.e. turn it from:
           *
           *   expr = 0
           *
           * to:
           *
           *   expr - eplus + eminus = 0
           */
          self->slack_counter += 1;

          eplus = gtk_constraint_variable_new_slack ("ep");
          eminus = gtk_constraint_variable_new_slack ("em");

          gtk_constraint_expression_set_variable (expr, eplus, -1.0);
          gtk_constraint_expression_set_variable (expr, eminus, 1.0);

          g_hash_table_insert (self->marker_vars, constraint, eplus);

          z_row = g_hash_table_lookup (self->rows, self->objective);

          gtk_constraint_expression_set_variable (z_row, eplus, constraint->strength);
          gtk_constraint_expression_set_variable (z_row, eminus, constraint->strength);
          gtk_constraint_solver_note_added_variable (self, eplus, self->objective);
          gtk_constraint_solver_note_added_variable (self, eminus, self->objective);

          gtk_constraint_solver_insert_error_variable (self, constraint, eplus);
          gtk_constraint_solver_insert_error_variable (self, constraint, eminus);

          if (constraint->is_stay)
            {
              g_ptr_array_add (self->stay_error_vars, gtk_constraint_variable_pair_new (eplus, eminus));
            }
          else if (constraint->is_edit)
            {
              if (eplus_p != NULL)
                *eplus_p = eplus;
              if (eminus_p != NULL)
                *eminus_p = eminus;
              if (prev_constant_p != NULL)
                *prev_constant_p = gtk_constraint_expression_get_constant (cn_expr);
            }

          gtk_constraint_variable_unref (eplus);
          gtk_constraint_variable_unref (eminus);
        }
    }

  if (gtk_constraint_expression_get_constant (expr) < 0.0)
    gtk_constraint_expression_multiply_by (expr, -1.0);

  return expr;
}

static void
gtk_constraint_solver_dual_optimize (GtkConstraintSolver *self)
{
  GtkConstraintExpression *z_row = g_hash_table_lookup (self->rows, self->objective);
  gint64 start_time = g_get_monotonic_time ();

  /* We iterate until we don't have any more infeasible rows; the pivot()
   * at the end of the loop iteration may add or remove infeasible rows
   * as well
   */
  while (self->infeasible_rows->len != 0)
    {
      GtkConstraintVariable *entry_var, *exit_var, *t_v;
      GtkConstraintExpressionIter eiter;
      GtkConstraintExpression *expr;
      double ratio, t_c;

      /* Pop the last element of the array */
      exit_var = g_ptr_array_index (self->infeasible_rows, self->infeasible_rows->len - 1);
      g_ptr_array_set_size (self->infeasible_rows, self->infeasible_rows->len - 1);

      expr = g_hash_table_lookup (self->rows, exit_var);
      if (expr == NULL)
        continue;

      if (gtk_constraint_expression_get_constant (expr) >= 0.0)
        continue;

      ratio = DBL_MAX;
      entry_var = NULL;

      gtk_constraint_expression_iter_init (&eiter, expr);
      while (gtk_constraint_expression_iter_next (&eiter, &t_v, &t_c))
        {
          if (t_c > 0.0 && gtk_constraint_variable_is_pivotable (t_v))
            {
              double zc = gtk_constraint_expression_get_coefficient (z_row, t_v);
              double r = zc / t_c;

              if (r < ratio)
                {
                  entry_var = t_v;
                  ratio = r;
                }
            }
        }

      if (ratio == DBL_MAX)
        g_critical ("INTERNAL: ratio == DBL_MAX in dual_optimize");

      gtk_constraint_solver_pivot (self, entry_var, exit_var);
    }

  GTK_DEBUG (CONSTRAINTS, "dual_optimize.time := %.3f ms",
                          (float) (g_get_monotonic_time () - start_time) / 1000.f);
}

static void
gtk_constraint_solver_delta_edit_constant (GtkConstraintSolver *self,
                                           double delta,
                                           GtkConstraintVariable *plus_error_var,
                                           GtkConstraintVariable *minus_error_var)
{
  GtkConstraintExpression *plus_expr, *minus_expr;
  GtkConstraintVariable *basic_var;
  GtkConstraintVariableSet *column_set;
  GtkConstraintVariableSetIter iter;

  plus_expr = g_hash_table_lookup (self->rows, plus_error_var);
  if (plus_expr != NULL)
    {
      double new_constant = gtk_constraint_expression_get_constant (plus_expr) + delta;

      gtk_constraint_expression_set_constant (plus_expr, new_constant);

      if (new_constant < 0.0)
        g_ptr_array_add (self->infeasible_rows, plus_error_var);

      return;
    }

  minus_expr = g_hash_table_lookup (self->rows, minus_error_var);
  if (minus_expr != NULL)
    {
      double new_constant = gtk_constraint_expression_get_constant (minus_expr) - delta;

      gtk_constraint_expression_set_constant (minus_expr, new_constant);

      if (new_constant < 0.0)
        g_ptr_array_add (self->infeasible_rows, minus_error_var);

      return;
    }

  column_set = g_hash_table_lookup (self->columns, minus_error_var);
  if (column_set == NULL)
    {
      g_critical ("INTERNAL: Columns are unset during delta edit");
      return;
    }

  gtk_constraint_variable_set_iter_init (&iter, column_set);
  while (gtk_constraint_variable_set_iter_next (&iter, &basic_var))
    {
      GtkConstraintExpression *expr;
      double c, new_constant;

      expr = g_hash_table_lookup (self->rows, basic_var);
      c = gtk_constraint_expression_get_coefficient (expr, minus_error_var);

      new_constant = gtk_constraint_expression_get_constant (expr) + (c * delta);
      gtk_constraint_expression_set_constant (expr, new_constant);

      if (gtk_constraint_variable_is_restricted (basic_var) && new_constant < 0.0)
        g_ptr_array_add (self->infeasible_rows, basic_var);
    }
}

static GtkConstraintVariable *
gtk_constraint_solver_choose_subject (GtkConstraintSolver *self,
                                      GtkConstraintExpression *expression)
{
  GtkConstraintExpressionIter eiter;
  GtkConstraintVariable *subject = NULL;
  GtkConstraintVariable *retval = NULL;
  GtkConstraintVariable *t_v;
  gboolean found_unrestricted = FALSE;
  gboolean found_new_restricted = FALSE;
  gboolean retval_found = FALSE;
  double coeff = 0.0;
  double t_c;

  gtk_constraint_expression_iter_init (&eiter, expression);
  while (gtk_constraint_expression_iter_prev (&eiter, &t_v, &t_c))
    {
      if (found_unrestricted)
        {
          if (!gtk_constraint_variable_is_restricted (t_v))
            {
              if (!g_hash_table_contains (self->columns, t_v))
                {
                  retval_found = TRUE;
                  retval = t_v;
                  break;
                }
            }
        }
      else
        {
          if (gtk_constraint_variable_is_restricted (t_v))
            {
              if (!found_new_restricted &&
                  !gtk_constraint_variable_is_dummy (t_v) &&
                  t_c < 0.0)
                {
                  GtkConstraintVariableSet *cset = g_hash_table_lookup (self->columns, t_v);

                  if (cset == NULL ||
                      (gtk_constraint_variable_set_is_singleton (cset) &&
                       g_hash_table_contains (self->columns, self->objective)))
                    {
                      subject = t_v;
                      found_new_restricted = TRUE;
                    }
                }
            }
          else
            {
              subject = t_v;
              found_unrestricted = TRUE;
            }
        }
    }

  if (retval_found)
    return retval;

  if (subject != NULL)
    return subject;

  gtk_constraint_expression_iter_init (&eiter, expression);
  while (gtk_constraint_expression_iter_prev (&eiter, &t_v, &t_c))
    {
      if (!gtk_constraint_variable_is_dummy (t_v))
        return NULL;

      if (!g_hash_table_contains (self->columns, t_v))
        {
          subject = t_v;
          coeff = t_c;
        }
    }

  if (!G_APPROX_VALUE (gtk_constraint_expression_get_constant (expression), 0.0, 0.001))
    {
      GTK_DEBUG (CONSTRAINTS,
                 "Unable to satisfy required constraint (choose_subject)");
      return NULL;
    }

  if (coeff > 0)
    gtk_constraint_expression_multiply_by (expression, -1.0);

  return subject;
}

static gboolean
gtk_constraint_solver_try_adding_directly (GtkConstraintSolver *self,
                                           GtkConstraintExpression *expression)
{
  GtkConstraintVariable *subject;

  subject = gtk_constraint_solver_choose_subject (self, expression);
  if (subject == NULL)
    return FALSE;

  gtk_constraint_variable_ref (subject);

  gtk_constraint_expression_new_subject (expression, subject);
  if (gtk_constraint_solver_column_has_key (self, subject))
    gtk_constraint_solver_substitute_out (self, subject, expression);

  gtk_constraint_solver_add_row (self, subject, expression);

  gtk_constraint_variable_unref (subject);

  return TRUE;
}

static void
gtk_constraint_solver_add_with_artificial_variable (GtkConstraintSolver *self,
                                                    GtkConstraintExpression *expression)
{
  GtkConstraintVariable *av, *az;
  GtkConstraintExpression *az_row;
  GtkConstraintExpression *az_tableau_row;
  GtkConstraintExpression *e;

  av = gtk_constraint_variable_new_slack ("a");
  self->artificial_counter += 1;

  az = gtk_constraint_variable_new_objective ("az");

  az_row = gtk_constraint_expression_clone (expression);

  gtk_constraint_solver_add_row (self, az, az_row);
  gtk_constraint_solver_add_row (self, av, expression);

  gtk_constraint_expression_unref (az_row);
  gtk_constraint_variable_unref (av);
  gtk_constraint_variable_unref (az);

  gtk_constraint_solver_optimize (self, az);

  az_tableau_row = g_hash_table_lookup (self->rows, az);
  if (!G_APPROX_VALUE (gtk_constraint_expression_get_constant (az_tableau_row), 0.0, 0.001))
    {
      gtk_constraint_solver_remove_column (self, av);
      gtk_constraint_solver_remove_row (self, az, TRUE);

      if (GTK_DEBUG_CHECK (CONSTRAINTS))
        {
          char *str = gtk_constraint_expression_to_string (expression);
          g_message ("Unable to satisfy a required constraint (add): %s", str);
          g_free (str);
        }

      return;
    }

  e = g_hash_table_lookup (self->rows, av);
  if (e != NULL)
    {
      GtkConstraintVariable *entry_var;

      if (gtk_constraint_expression_is_constant (e))
        {
          gtk_constraint_solver_remove_row (self, av, TRUE);
          gtk_constraint_solver_remove_row (self, az, TRUE);
          return;
        }

      entry_var = gtk_constraint_expression_get_pivotable_variable (e);
      if (entry_var == NULL)
        return;

      gtk_constraint_solver_pivot (self, entry_var, av);
    }

  g_assert (!g_hash_table_contains (self->rows, av));

  gtk_constraint_solver_remove_column (self, av);
  gtk_constraint_solver_remove_row (self, az, TRUE);
}

static void
gtk_constraint_solver_add_constraint_internal (GtkConstraintSolver *self,
                                               GtkConstraintRef *constraint)
{
  GtkConstraintExpression *expr;
  GtkConstraintVariable *eplus;
  GtkConstraintVariable *eminus;
  double prev_constant;

  expr = gtk_constraint_solver_new_expression (self, constraint,
                                               &eplus,
                                               &eminus,
                                               &prev_constant);

  if (GTK_DEBUG_CHECK (CONSTRAINTS))
    {
      char *expr_s = gtk_constraint_expression_to_string (expr);
      char *ref_s = gtk_constraint_ref_to_string (constraint);
      g_message ("Adding constraint '%s' (normalized expression: '%s')", ref_s, expr_s);
      g_free (ref_s);
      g_free (expr_s);
    }

  if (constraint->is_stay)
    {
      StayInfo *si = g_new (StayInfo, 1);

      si->constraint = constraint;

      g_hash_table_insert (self->stay_var_map, constraint->variable, si);
    }
  else if (constraint->is_edit)
    {
      EditInfo *ei = g_new (EditInfo, 1);

      ei->constraint = constraint;
      ei->eplus = eplus;
      ei->eminus = eminus;
      ei->prev_constant = prev_constant;

      g_hash_table_insert (self->edit_var_map, constraint->variable, ei);
    }

  if (!gtk_constraint_solver_try_adding_directly (self, expr))
    gtk_constraint_solver_add_with_artificial_variable (self, expr);

  gtk_constraint_expression_unref (expr);

  self->needs_solving = TRUE;

  if (self->auto_solve)
    {
      gtk_constraint_solver_optimize (self, self->objective);
      gtk_constraint_solver_set_external_variables (self);
    }

  constraint->solver = self;

  g_hash_table_add (self->constraints, constraint);
}

/*< private >
 * gtk_constraint_solver_new:
 *
 * Creates a new `GtkConstraintSolver` instance.
 *
 * Returns: the newly created `GtkConstraintSolver`
 */
GtkConstraintSolver *
gtk_constraint_solver_new (void)
{
  return g_object_new (GTK_TYPE_CONSTRAINT_SOLVER, NULL);
}

/*< private >
 * gtk_constraint_solver_freeze:
 * @solver: a `GtkConstraintSolver`
 *
 * Freezes the solver; any constraint addition or removal will not
 * be automatically solved until gtk_constraint_solver_thaw() is
 * called.
 */
void
gtk_constraint_solver_freeze (GtkConstraintSolver *solver)
{
  g_return_if_fail (GTK_IS_CONSTRAINT_SOLVER (solver));

  solver->freeze_count += 1;

  if (solver->freeze_count > 0)
    solver->auto_solve = FALSE;
}

/*< private >
 * gtk_constraint_solver_thaw:
 * @solver: a `GtkConstraintSolver`
 *
 * Thaws a frozen `GtkConstraintSolver`.
 */
void
gtk_constraint_solver_thaw (GtkConstraintSolver *solver)
{
  g_return_if_fail (GTK_IS_CONSTRAINT_SOLVER (solver));
  g_return_if_fail (solver->freeze_count > 0);

  solver->freeze_count -= 1;

  if (solver->freeze_count == 0)
    {
      solver->auto_solve = TRUE;
      gtk_constraint_solver_resolve (solver);
    }
}

/*< private >
 * gtk_constraint_solver_note_added_variable:
 * @self: a `GtkConstraintSolver`
 * @variable: a `GtkConstraintVariable`
 * @subject: a `GtkConstraintVariable`
 *
 * Adds a new @variable into the tableau of a `GtkConstraintSolver`.
 *
 * This function is typically called by `GtkConstraintExpression`, and
 * should never be directly called.
 */
void
gtk_constraint_solver_note_added_variable (GtkConstraintSolver *self,
                                           GtkConstraintVariable *variable,
                                           GtkConstraintVariable *subject)
{
  if (subject != NULL)
    gtk_constraint_solver_insert_column_variable (self, variable, subject);
}

/*< private >
 * gtk_constraint_solver_note_removed_variable:
 * @self: a `GtkConstraintSolver`
 * @variable: a `GtkConstraintVariable`
 * @subject: a `GtkConstraintVariable`
 *
 * Removes a @variable from the tableau of a `GtkConstraintSolver`.
 *
 * This function is typically called by `GtkConstraintExpression`, and
 * should never be directly called.
 */
void
gtk_constraint_solver_note_removed_variable (GtkConstraintSolver *self,
                                             GtkConstraintVariable *variable,
                                             GtkConstraintVariable *subject)
{
  GtkConstraintVariableSet *set;

  set = g_hash_table_lookup (self->columns, variable);
  if (set != NULL && subject != NULL)
    gtk_constraint_variable_set_remove (set, subject);
}

/*< private >
 * gtk_constraint_solver_create_variable:
 * @self: a `GtkConstraintSolver`
 * @prefix: (nullable): the prefix of the variable
 * @name: (nullable): the name of the variable
 * @value: the initial value of the variable
 *
 * Creates a new variable inside the @solver.
 *
 * Returns: (transfer full): the newly created variable
 */
GtkConstraintVariable *
gtk_constraint_solver_create_variable (GtkConstraintSolver *self,
                                       const char *prefix,
                                       const char *name,
                                       double value)
{
  GtkConstraintVariable *res;

  res = gtk_constraint_variable_new (prefix, name);
  gtk_constraint_variable_set_value (res, value);

  self->var_counter++;

  return res;
}

/*< private >
 * gtk_constraint_solver_resolve:
 * @solver: a `GtkConstraintSolver`
 *
 * Resolves the constraints currently stored in @solver.
 */
void
gtk_constraint_solver_resolve (GtkConstraintSolver *solver)
{
  gint64 start_time = g_get_monotonic_time ();

  g_return_if_fail (GTK_IS_CONSTRAINT_SOLVER (solver));

  gtk_constraint_solver_dual_optimize (solver);
  gtk_constraint_solver_set_external_variables (solver);

  g_ptr_array_set_size (solver->infeasible_rows, 0);

  gtk_constraint_solver_reset_stay_constants (solver);

  GTK_DEBUG (CONSTRAINTS, "resolve.time := %.3f ms",
                          (float) (g_get_monotonic_time () - start_time) / 1000.f);

  solver->needs_solving = FALSE;
}

/*< private >
 * gtk_constraint_solver_add_constraint:
 * @self: a `GtkConstraintSolver`
 * @variable: the subject of the constraint
 * @relation: the relation of the constraint
 * @expression: the expression of the constraint
 * @strength: the strength of the constraint
 *
 * Adds a new constraint in the form of:
 *
 * |[
 *   variable relation expression (strength)
 * |]
 *
 * into the `GtkConstraintSolver`.
 *
 * Returns: (transfer none): a reference to the newly created
 *   constraint; you can use the reference to remove the
 *   constraint from the solver
 */
GtkConstraintRef *
gtk_constraint_solver_add_constraint (GtkConstraintSolver *self,
                                      GtkConstraintVariable *variable,
                                      GtkConstraintRelation relation,
                                      GtkConstraintExpression *expression,
                                      int strength)
{
  GtkConstraintRef *res = g_new0 (GtkConstraintRef, 1);

  res->solver = self;
  res->strength = strength;
  res->is_edit = FALSE;
  res->is_stay = FALSE;
  res->relation = relation;

  if (expression == NULL)
    res->expression = gtk_constraint_expression_new_from_variable (variable);
  else
    {
      res->expression = expression;

      if (variable != NULL)
        {
          switch (res->relation)
            {
            case GTK_CONSTRAINT_RELATION_EQ:
              gtk_constraint_expression_add_variable (res->expression,
                                                      variable, -1.0,
                                                      NULL,
                                                      self);
              break;

            case GTK_CONSTRAINT_RELATION_LE:
              gtk_constraint_expression_add_variable (res->expression,
                                                      variable, -1.0,
                                                      NULL,
                                                      self);
              break;

            case GTK_CONSTRAINT_RELATION_GE:
              gtk_constraint_expression_multiply_by (res->expression, -1.0);
              gtk_constraint_expression_add_variable (res->expression,
                                                      variable, 1.0,
                                                      NULL,
                                                      self);
              break;

            default:
              g_assert_not_reached ();
            }
        }
    }

  gtk_constraint_solver_add_constraint_internal (self, res);

  return res;
}

/*< private >
 * gtk_constraint_solver_add_stay_variable:
 * @self: a `GtkConstraintSolver`
 * @variable: a stay `GtkConstraintVariable`
 * @strength: the strength of the constraint
 *
 * Adds a constraint on a stay @variable with the given @strength.
 *
 * A stay variable is an "anchor" in the system: a variable that is
 * supposed to stay at the same value.
 *
 * Returns: (transfer none): a reference to the newly created
 *   constraint; you can use the reference to remove the
 *   constraint from the solver
 */
GtkConstraintRef *
gtk_constraint_solver_add_stay_variable (GtkConstraintSolver *self,
                                         GtkConstraintVariable *variable,
                                         int strength)
{
  GtkConstraintRef *res = g_new0 (GtkConstraintRef, 1);

  res->solver = self;
  res->variable = gtk_constraint_variable_ref (variable);
  res->relation = GTK_CONSTRAINT_RELATION_EQ;
  res->strength = strength;
  res->is_stay = TRUE;
  res->is_edit = FALSE;

  res->expression = gtk_constraint_expression_new (gtk_constraint_variable_get_value (res->variable));
  gtk_constraint_expression_add_variable (res->expression,
                                          res->variable, -1.0,
                                          NULL,
                                          self);

  if (GTK_DEBUG_CHECK (CONSTRAINTS))
    {
      char *str = gtk_constraint_expression_to_string (res->expression);
      g_message ("Adding stay variable: %s", str);
      g_free (str);
    }

  gtk_constraint_solver_add_constraint_internal (self, res);

  return res;
}

/*< private >
 * gtk_constraint_solver_remove_stay_variable:
 * @self: a `GtkConstraintSolver`
 * @variable: a stay variable
 *
 * Removes the stay constraint associated to @variable.
 *
 * This is a convenience function for gtk_constraint_solver_remove_constraint().
 */
void
gtk_constraint_solver_remove_stay_variable (GtkConstraintSolver *self,
                                            GtkConstraintVariable *variable)
{
  StayInfo *si = g_hash_table_lookup (self->stay_var_map, variable);

  if (si == NULL)
    {
      char *str = gtk_constraint_variable_to_string (variable);

      g_critical ("Unknown stay variable '%s'", str);

      g_free (str);

      return;
    }

  gtk_constraint_solver_remove_constraint (self, si->constraint);
}

/*< private >
 * gtk_constraint_solver_add_edit_variable:
 * @self: a `GtkConstraintSolver`
 * @variable: an edit variable
 * @strength: the strength of the constraint
 *
 * Adds an editable constraint to the @solver.
 *
 * Editable constraints can be used to suggest values to a
 * `GtkConstraintSolver` inside an edit phase, for instance: if
 * you want to change the value of a variable without necessarily
 * insert a new constraint every time.
 *
 * See also: gtk_constraint_solver_suggest_value()
 *
 * Returns: (transfer none): a reference to the newly added constraint
 */
GtkConstraintRef *
gtk_constraint_solver_add_edit_variable (GtkConstraintSolver *self,
                                         GtkConstraintVariable *variable,
                                         int strength)
{
  GtkConstraintRef *res = g_new0 (GtkConstraintRef, 1);

  res->solver = self;
  res->variable = gtk_constraint_variable_ref (variable);
  res->relation = GTK_CONSTRAINT_RELATION_EQ;
  res->strength = strength;
  res->is_stay = FALSE;
  res->is_edit = TRUE;

  res->expression = gtk_constraint_expression_new (gtk_constraint_variable_get_value (variable));
  gtk_constraint_expression_add_variable (res->expression,
                                          variable, -1.0,
                                          NULL,
                                          self);

  gtk_constraint_solver_add_constraint_internal (self, res);

  return res;
}

/*< private >
 * gtk_constraint_solver_remove_edit_variable:
 * @self: a `GtkConstraintSolver`
 * @variable: an edit variable
 *
 * Removes the edit constraint associated to @variable.
 *
 * This is a convenience function around gtk_constraint_solver_remove_constraint().
 */
void
gtk_constraint_solver_remove_edit_variable (GtkConstraintSolver *self,
                                            GtkConstraintVariable *variable)
{
  EditInfo *ei = g_hash_table_lookup (self->edit_var_map, variable);

  if (ei == NULL)
    {
      char *str = gtk_constraint_variable_to_string (variable);

      g_critical ("Unknown edit variable '%s'", str);

      g_free (str);

      return;
    }

  gtk_constraint_solver_remove_constraint (self, ei->constraint);
}

/*< private >
 * gtk_constraint_solver_remove_constraint:
 * @self: a `GtkConstraintSolver`
 * @constraint: a constraint reference
 *
 * Removes a @constraint from the @solver.
 */
void
gtk_constraint_solver_remove_constraint (GtkConstraintSolver *self,
                                         GtkConstraintRef *constraint)
{
  GtkConstraintExpression *z_row;
  GtkConstraintVariable *marker;
  GtkConstraintVariableSet *error_vars;
  GtkConstraintVariableSetIter iter;

  if (!g_hash_table_contains (self->constraints, constraint))
    return;

  self->needs_solving = TRUE;

  gtk_constraint_solver_reset_stay_constants (self);

  z_row = g_hash_table_lookup (self->rows, self->objective);
  error_vars = g_hash_table_lookup (self->error_vars, constraint);

  if (error_vars != NULL)
    {
      GtkConstraintVariable *v;

      gtk_constraint_variable_set_iter_init (&iter, error_vars);
      while (gtk_constraint_variable_set_iter_next (&iter, &v))
        {
          GtkConstraintExpression *e = g_hash_table_lookup (self->rows, v);

          if (e == NULL)
            {
              gtk_constraint_expression_add_variable (z_row,
                                                      v,
                                                      constraint->strength,
                                                      self->objective,
                                                      self);
            }
          else
            {
              gtk_constraint_expression_add_expression (z_row,
                                                        e,
                                                        constraint->strength,
                                                        self->objective,
                                                        self);
            }
        }
    }

  marker = g_hash_table_lookup (self->marker_vars, constraint);
  if (marker == NULL)
    {
      g_critical ("Constraint %p not found", constraint);
      return;
    }

  g_hash_table_remove (self->marker_vars, constraint);

  if (g_hash_table_lookup (self->rows, marker) == NULL)
    {
      GtkConstraintVariableSet *set = g_hash_table_lookup (self->columns, marker);
      GtkConstraintVariable *exit_var = NULL;
      GtkConstraintVariable *v;
      double min_ratio = 0;

      if (set == NULL)
        goto no_columns;

      gtk_constraint_variable_set_iter_init (&iter, set);
      while (gtk_constraint_variable_set_iter_next (&iter, &v))
        {
          if (gtk_constraint_variable_is_restricted (v))
            {
              GtkConstraintExpression *e = g_hash_table_lookup (self->rows, v);
              double coeff = gtk_constraint_expression_get_coefficient (e, marker);

              if (coeff < 0.0)
                {
                  double r = -gtk_constraint_expression_get_constant (e) / coeff;

                  if (exit_var == NULL ||
                      r < min_ratio ||
                      G_APPROX_VALUE (r, min_ratio, 0.0001))
                    {
                      min_ratio = r;
                      exit_var = v;
                    }
                }
            }
        }

      if (exit_var == NULL)
        {
          gtk_constraint_variable_set_iter_init (&iter, set);
          while (gtk_constraint_variable_set_iter_next (&iter, &v))
            {
              if (gtk_constraint_variable_is_restricted (v))
                {
                  GtkConstraintExpression *e = g_hash_table_lookup (self->rows, v);
                  double coeff = gtk_constraint_expression_get_coefficient (e, marker);
                  double r = 0.0;

                  if (!G_APPROX_VALUE (coeff, 0.0, 0.0001))
                    r = gtk_constraint_expression_get_constant (e) / coeff;

                  if (exit_var == NULL || r < min_ratio)
                    {
                      min_ratio = r;
                      exit_var = v;
                    }
                }
            }
        }

      if (exit_var == NULL)
        {
          if (gtk_constraint_variable_set_is_empty (set))
            gtk_constraint_solver_remove_column (self, marker);
          else
            {
              gtk_constraint_variable_set_iter_init (&iter, set);
              while (gtk_constraint_variable_set_iter_next (&iter, &v))
                {
                  if (v != self->objective)
                    {
                      exit_var = v;
                      break;
                    }
                }
            }
        }

      if (exit_var != NULL)
        gtk_constraint_solver_pivot (self, marker, exit_var);
    }

no_columns:
  if (g_hash_table_lookup (self->rows, marker) != NULL)
    gtk_constraint_solver_remove_row (self, marker, TRUE);
  else
    gtk_constraint_variable_unref (marker);

  if (error_vars != NULL)
    {
      GtkConstraintVariable *v;

      gtk_constraint_variable_set_iter_init (&iter, error_vars);
      while (gtk_constraint_variable_set_iter_next (&iter, &v))
        {
          if (v != marker)
            gtk_constraint_solver_remove_column (self, v);
        }
    }

  if (constraint->is_stay)
    {
      if (error_vars != NULL)
        {
          GPtrArray *remaining =
            g_ptr_array_new_with_free_func ((GDestroyNotify) gtk_constraint_variable_pair_free);

          int i = 0;

          for (i = 0; i < self->stay_error_vars->len; i++)
            {
              GtkConstraintVariablePair *pair = g_ptr_array_index (self->stay_error_vars, i);
              gboolean found = FALSE;

              if (gtk_constraint_variable_set_remove (error_vars, pair->first))
                found = TRUE;

              if (gtk_constraint_variable_set_remove (error_vars, pair->second))
                found = FALSE;

              if (!found)
                g_ptr_array_add (remaining, gtk_constraint_variable_pair_new (pair->first, pair->second));
            }

          g_clear_pointer (&self->stay_error_vars, g_ptr_array_unref);
          self->stay_error_vars = remaining;
        }

      g_hash_table_remove (self->stay_var_map, constraint->variable);
    }
  else if (constraint->is_edit)
    {
      EditInfo *ei = g_hash_table_lookup (self->edit_var_map, constraint->variable);

      gtk_constraint_solver_remove_column (self, ei->eminus);

      g_hash_table_remove (self->edit_var_map, constraint->variable);
    }

  if (error_vars != NULL)
    g_hash_table_remove (self->error_vars, constraint);

  if (self->auto_solve)
    {
      gtk_constraint_solver_optimize (self, self->objective);
      gtk_constraint_solver_set_external_variables (self);
    }

  g_hash_table_remove (self->constraints, constraint);
}

/*< private >
 * gtk_constraint_solver_suggest_value:
 * @self: a `GtkConstraintSolver`
 * @variable: a `GtkConstraintVariable`
 * @value: the suggested value for @variable
 *
 * Suggests a new @value for an edit @variable.
 *
 * The @variable must be an edit variable, and the solver must be
 * in an edit phase.
 */
void
gtk_constraint_solver_suggest_value (GtkConstraintSolver *self,
                                     GtkConstraintVariable *variable,
                                     double value)
{
  EditInfo *ei = g_hash_table_lookup (self->edit_var_map, variable);
  double delta;
  if (ei == NULL)
    {
      g_critical ("Suggesting value '%g' but variable %p is not editable",
                  value, variable);
      return;
    }

  if (!self->in_edit_phase)
    {
      g_critical ("Suggesting value '%g' for variable '%p' but solver is "
                  "not in an edit phase",
                  value, variable);
      return;
    }

  delta = value - ei->prev_constant;
  ei->prev_constant = value;

  gtk_constraint_solver_delta_edit_constant (self, delta, ei->eplus, ei->eminus);
}

/*< private >
 * gtk_constraint_solver_has_stay_variable:
 * @solver: a `GtkConstraintSolver`
 * @variable: a `GtkConstraintVariable`
 *
 * Checks whether @variable is a stay variable.
 *
 * Returns: %TRUE if the variable is a stay variable
 */
gboolean
gtk_constraint_solver_has_stay_variable (GtkConstraintSolver   *solver,
                                         GtkConstraintVariable *variable)
{
  g_return_val_if_fail (GTK_IS_CONSTRAINT_SOLVER (solver), FALSE);
  g_return_val_if_fail (variable != NULL, FALSE);

  return g_hash_table_contains (solver->stay_var_map, variable);
}

/*< private >
 * gtk_constraint_solver_has_edit_variable:
 * @solver: a `GtkConstraintSolver`
 * @variable: a `GtkConstraintVariable`
 *
 * Checks whether @variable is an edit variable.
 *
 * Returns: %TRUE if the variable is an edit variable
 */
gboolean
gtk_constraint_solver_has_edit_variable (GtkConstraintSolver   *solver,
                                         GtkConstraintVariable *variable)
{
  g_return_val_if_fail (GTK_IS_CONSTRAINT_SOLVER (solver), FALSE);
  g_return_val_if_fail (variable != NULL, FALSE);

  return g_hash_table_contains (solver->edit_var_map, variable);
}

/*< private >
 * gtk_constraint_solver_begin_edit:
 * @solver: a `GtkConstraintSolver`
 *
 * Begins the edit phase for a constraint system.
 *
 * Typically, you need to add new edit constraints for a variable to the
 * system, using gtk_constraint_solver_add_edit_variable(); then you
 * call this function and suggest values for the edit variables, using
 * gtk_constraint_solver_suggest_value(). After you suggested a value
 * for all the variables you need to edit, you will need to call
 * gtk_constraint_solver_resolve() to solve the system, and get the value
 * of the various variables that you're interested in.
 *
 * Once you completed the edit phase, call gtk_constraint_solver_end_edit()
 * to remove all the edit variables.
 */
void
gtk_constraint_solver_begin_edit (GtkConstraintSolver *solver)
{
  g_return_if_fail (GTK_IS_CONSTRAINT_SOLVER (solver));

  if (g_hash_table_size (solver->edit_var_map) == 0)
    {
      g_critical ("Solver %p does not have editable variables.", solver);
      return;
    }

  g_ptr_array_set_size (solver->infeasible_rows, 0);
  gtk_constraint_solver_reset_stay_constants (solver);

  solver->in_edit_phase = TRUE;
}

static void
edit_info_free (gpointer data)
{
  g_free (data);
}

/*< private >
 * gtk_constraint_solver_end_edit:
 * @solver: a `GtkConstraintSolver`
 *
 * Ends the edit phase for a constraint system, and clears
 * all the edit variables introduced.
 */
void
gtk_constraint_solver_end_edit (GtkConstraintSolver *solver)
{
  solver->in_edit_phase = FALSE;

  gtk_constraint_solver_resolve (solver);

  g_hash_table_remove_all (solver->edit_var_map);
}

void
gtk_constraint_solver_clear (GtkConstraintSolver *solver)
{
  g_return_if_fail (GTK_IS_CONSTRAINT_SOLVER (solver));

  g_hash_table_remove_all (solver->constraints);
  g_hash_table_remove_all (solver->external_rows);
  g_hash_table_remove_all (solver->external_parametric_vars);
  g_hash_table_remove_all (solver->error_vars);
  g_hash_table_remove_all (solver->marker_vars);
  g_hash_table_remove_all (solver->edit_var_map);
  g_hash_table_remove_all (solver->stay_var_map);

  g_ptr_array_set_size (solver->infeasible_rows, 0);
  g_ptr_array_set_size (solver->stay_error_vars, 0);

  g_hash_table_remove_all (solver->rows);
  g_hash_table_remove_all (solver->columns);

  /* The rows table owns the objective variable */
  solver->objective = gtk_constraint_variable_new_objective ("Z");
  g_hash_table_insert (solver->rows,
                       solver->objective,
                       gtk_constraint_expression_new (0.0));

  solver->slack_counter = 0;
  solver->dummy_counter = 0;
  solver->artificial_counter = 0;
  solver->freeze_count = 0;

  solver->needs_solving = FALSE;
  solver->auto_solve = TRUE;
}

char *
gtk_constraint_solver_to_string (GtkConstraintSolver *solver)
{
  GString *buf = g_string_new (NULL);

  g_string_append (buf, "Tableau info:\n");
  g_string_append_printf (buf, "Rows: %d (= %d constraints)\n",
                          g_hash_table_size (solver->rows),
                          g_hash_table_size (solver->rows) - 1);
  g_string_append_printf (buf, "Columns: %d\n",
                          g_hash_table_size (solver->columns));
  g_string_append_printf (buf, "Infeasible rows: %d\n",
                          solver->infeasible_rows->len);
  g_string_append_printf (buf, "External basic variables: %d\n",
                          g_hash_table_size (solver->external_rows));
  g_string_append_printf (buf, "External parametric variables: %d\n",
                          g_hash_table_size (solver->external_parametric_vars));

  g_string_append (buf, "Constraints:");
  if (g_hash_table_size (solver->constraints) == 0)
    g_string_append (buf, " <empty>\n");
  else
    {
      GHashTableIter iter;
      gpointer key_p;

      g_string_append (buf, "\n");

      g_hash_table_iter_init (&iter, solver->constraints);
      while (g_hash_table_iter_next (&iter, &key_p, NULL))
        {
          char *ref = gtk_constraint_ref_to_string (key_p);

          g_string_append_printf (buf, "  %s\n", ref);

          g_free (ref);
        }
    }

  g_string_append (buf, "Stay error vars:");
  if (solver->stay_error_vars->len == 0)
    g_string_append (buf, " <empty>\n");
  else
    {
      g_string_append (buf, "\n");

      for (int i = 0; i < solver->stay_error_vars->len; i++)
        {
          const GtkConstraintVariablePair *pair = g_ptr_array_index (solver->stay_error_vars, i);
          char *first_s = gtk_constraint_variable_to_string (pair->first);
          char *second_s = gtk_constraint_variable_to_string (pair->second);

          g_string_append_printf (buf, "  (%s, %s)\n", first_s, second_s);

          g_free (first_s);
          g_free (second_s);
        }
    }

  g_string_append (buf, "Edit var map:");
  if (g_hash_table_size (solver->edit_var_map) == 0)
    g_string_append (buf, " <empty>\n");
  else
    {
      GHashTableIter iter;
      gpointer key_p, value_p;

      g_string_append (buf, "\n");

      g_hash_table_iter_init (&iter, solver->edit_var_map);
      while (g_hash_table_iter_next (&iter, &key_p, &value_p))
        {
          char *var = gtk_constraint_variable_to_string (key_p);
          const EditInfo *ei = value_p;
          char *c = gtk_constraint_ref_to_string (ei->constraint);

          g_string_append_printf (buf, "  %s => %s\n", var, c);

          g_free (var);
          g_free (c);
        }
    }

  return g_string_free (buf, FALSE);
}

char *
gtk_constraint_solver_statistics (GtkConstraintSolver *solver)
{
  GString *buf = g_string_new (NULL);

  g_string_append_printf (buf, "Variables: %d\n", solver->var_counter);
  g_string_append_printf (buf, "Slack vars: %d\n", solver->slack_counter);
  g_string_append_printf (buf, "Artificial vars: %d\n", solver->artificial_counter);
  g_string_append_printf (buf, "Dummy vars: %d\n", solver->dummy_counter);
  g_string_append_printf (buf, "Stay vars: %d\n", g_hash_table_size (solver->stay_var_map));
  g_string_append_printf (buf, "Optimize count: %d\n", solver->optimize_count);
  g_string_append_printf (buf, "Rows: %d\n", g_hash_table_size (solver->rows));
  g_string_append_printf (buf, "Columns: %d\n", g_hash_table_size (solver->columns));

  if (g_hash_table_size (solver->columns) > 0)
    {
      GHashTableIter iter;
      gpointer val;
      double sum = 0;

      g_hash_table_iter_init (&iter, solver->columns);
      while (g_hash_table_iter_next (&iter, NULL, &val))
        {
          GtkConstraintVariableSet *set = val;
          sum += gtk_constraint_variable_set_size (set);
        }
      g_string_append_printf (buf, "Avg column size: %g\n", sum / g_hash_table_size (solver->columns));
    }

  g_string_append_printf (buf, "Infeasible rows: %d\n", solver->infeasible_rows->len);
  g_string_append_printf (buf, "External basic variables: %d\n", g_hash_table_size (solver->external_rows));
  g_string_append_printf (buf, "External parametric variables: %d\n", g_hash_table_size (solver->external_parametric_vars));

  return g_string_free (buf, FALSE);
}
