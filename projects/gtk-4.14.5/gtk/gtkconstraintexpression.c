/* gtkconstraintexpression.c: Constraint expressions and variables
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

#include "config.h"

#include "gtkconstraintexpressionprivate.h"
#include "gtkconstraintsolverprivate.h"

/* {{{ Variables */

typedef enum {
  GTK_CONSTRAINT_SYMBOL_DUMMY         = 'd',
  GTK_CONSTRAINT_SYMBOL_OBJECTIVE     = 'o',
  GTK_CONSTRAINT_SYMBOL_SLACK         = 'S',
  GTK_CONSTRAINT_SYMBOL_REGULAR       = 'v'
} GtkConstraintSymbolType;

struct _GtkConstraintVariable
{
  guint64 _id;

  GtkConstraintSymbolType _type;

  /* Interned strings */
  const char *name;
  const char *prefix;

  double value;

  guint is_external : 1;
  guint is_pivotable : 1;
  guint is_restricted : 1;
};

/* Variables are sorted by a monotonic id */
static guint64 gtk_constraint_variable_next_id;

static void
gtk_constraint_variable_init (GtkConstraintVariable *variable,
                              const char *prefix,
                              const char *name)
{
  variable->_id = gtk_constraint_variable_next_id++;

  variable->prefix = g_intern_string (prefix);
  variable->name = g_intern_string (name);
  variable->prefix = NULL;
  variable->value = 0.0;
}

/*< private >
 * gtk_constraint_variable_new_dummy:
 * @name: the name of the variable
 *
 * Allocates and initializes a new `GtkConstraintVariable` for a "dummy"
 * symbol. Dummy symbols are typically used as markers inside a solver,
 * and will not be factored in the solution when pivoting the tableau
 * of the constraint equations.
 *
 * Only `GtkConstraintSolver` should use this function.
 *
 * Returns: a newly allocated `GtkConstraintVariable`
 */
GtkConstraintVariable *
gtk_constraint_variable_new_dummy (const char *name)
{
  GtkConstraintVariable *res = g_rc_box_new (GtkConstraintVariable);

  gtk_constraint_variable_init (res, NULL, name);

  res->_type = GTK_CONSTRAINT_SYMBOL_DUMMY;
  res->is_external = FALSE;
  res->is_pivotable = FALSE;
  res->is_restricted = TRUE;

  return res;
}

/*< private >
 * gtk_constraint_variable_new_objective:
 * @name: the name of the variable
 *
 * Allocates and initializes a new `GtkConstraintVariable` for an objective
 * symbol. This is the constant value we wish to find as the result of the
 * simplex optimization.
 *
 * Only `GtkConstraintSolver` should use this function.
 *
 * Returns: a newly allocated `GtkConstraintVariable`
 */
GtkConstraintVariable *
gtk_constraint_variable_new_objective (const char *name)
{
  GtkConstraintVariable *res = g_rc_box_new (GtkConstraintVariable);

  gtk_constraint_variable_init (res, NULL, name);

  res->_type = GTK_CONSTRAINT_SYMBOL_OBJECTIVE;
  res->is_external = FALSE;
  res->is_pivotable = FALSE;
  res->is_restricted = FALSE;

  return res;
}

/*< private >
 * gtk_constraint_variable_new_slack:
 * @name: the name of the variable
 *
 * Allocates and initializes a new `GtkConstraintVariable` for a "slack"
 * symbol. Slack variables are introduced inside the tableau to turn
 * inequalities, like:
 *
 * |[
 *   expr ≥ 0
 * ]|
 *
 * Into equalities, like:
 *
 * |[
 *   expr - slack = 0
 * ]|
 *
 * Only `GtkConstraintSolver` should use this function.
 *
 * Returns: a newly allocated `GtkConstraintVariable`
 */
GtkConstraintVariable *
gtk_constraint_variable_new_slack (const char *name)
{
  GtkConstraintVariable *res = g_rc_box_new (GtkConstraintVariable);

  gtk_constraint_variable_init (res, NULL, name);

  res->_type = GTK_CONSTRAINT_SYMBOL_SLACK;
  res->is_external = FALSE;
  res->is_pivotable = TRUE;
  res->is_restricted = TRUE;

  return res;
}

/*< private >
 * gtk_constraint_variable_new:
 * @prefix: (nullable): an optional prefix string for @name
 * @name: (nullable): an optional name for the variable
 *
 * Allocates and initializes a new `GtkConstraintVariable` for a regular
 * symbol. All variables introduced by constraints are regular variables.
 *
 * Only `GtkConstraintSolver` should use this function; a constraint layout
 * manager should ask the `GtkConstraintSolver` to create a variable, using
 * gtk_constraint_solver_create_variable(), which will insert the variable
 * in the solver's tableau.
 *
 * Returns: a newly allocated `GtkConstraintVariable`
 */
GtkConstraintVariable *
gtk_constraint_variable_new (const char *prefix,
                             const char *name)
{
  GtkConstraintVariable *res = g_rc_box_new (GtkConstraintVariable);

  gtk_constraint_variable_init (res, prefix, name);

  res->_type = GTK_CONSTRAINT_SYMBOL_REGULAR;
  res->is_external = TRUE;
  res->is_pivotable = FALSE;
  res->is_restricted = FALSE;

  return res;
}

/*< private >
 * gtk_constraint_variable_ref:
 * @variable: a `GtkConstraintVariable`
 *
 * Acquires a reference to @variable.
 *
 * Returns: (transfer full): the given `GtkConstraintVariable`, with its reference
 *   count increased
 */
GtkConstraintVariable *
gtk_constraint_variable_ref (GtkConstraintVariable *variable)
{
  g_return_val_if_fail (variable != NULL, NULL);

  return g_rc_box_acquire (variable);
}

/*< private >
 * gtk_constraint_variable_unref:
 * @variable: (transfer full): a `GtkConstraintVariable`
 *
 * Releases a reference to @variable.
 */
void
gtk_constraint_variable_unref (GtkConstraintVariable *variable)
{
  g_return_if_fail (variable != NULL);

  g_rc_box_release (variable);
}

/*< private >
 * gtk_constraint_variable_set_value:
 * @variable: a `GtkConstraintVariable`
 *
 * Sets the current value of a `GtkConstraintVariable`.
 */
void
gtk_constraint_variable_set_value (GtkConstraintVariable *variable,
                                   double value)
{
  variable->value = value;
}

/*< private >
 * gtk_constraint_variable_get_value:
 * @variable: a `GtkConstraintVariable`
 *
 * Retrieves the current value of a `GtkConstraintVariable`
 *
 * Returns: the value of the variable
 */
double
gtk_constraint_variable_get_value (const GtkConstraintVariable *variable)
{
  return variable->value;
}

/*< private >
 * gtk_constraint_variable_to_string:
 * @variable: a `GtkConstraintVariable`
 *
 * Turns @variable into a string, for debugging purposes.
 *
 * Returns: (transfer full): a string with the contents of @variable
 */
char *
gtk_constraint_variable_to_string (const GtkConstraintVariable *variable)
{
  GString *buf = g_string_new (NULL);

  if (variable == NULL)
    g_string_append (buf, "<null>");
  else
    {
      switch (variable->_type)
        {
        case GTK_CONSTRAINT_SYMBOL_DUMMY:
          g_string_append (buf, "(d)");
          break;
        case GTK_CONSTRAINT_SYMBOL_OBJECTIVE:
          g_string_append (buf, "(O)");
          break;
        case GTK_CONSTRAINT_SYMBOL_SLACK:
          g_string_append (buf, "(S)");
          break;
        case GTK_CONSTRAINT_SYMBOL_REGULAR:
          break;

        default:
          g_assert_not_reached ();
        }

      g_string_append_c (buf, '[');

      if (variable->prefix != NULL)
        {
          g_string_append (buf, variable->prefix);
          g_string_append_c (buf, '.');
        }

      if (variable->name != NULL)
        g_string_append (buf, variable->name);

      if (variable->_type == GTK_CONSTRAINT_SYMBOL_REGULAR)
        {
          char dbl_buf[G_ASCII_DTOSTR_BUF_SIZE];

          g_ascii_dtostr (dbl_buf, G_ASCII_DTOSTR_BUF_SIZE, variable->value);

          g_string_append_c (buf, ':');
          g_string_append (buf, dbl_buf);
        }

      g_string_append_c (buf, ']');
    }

  return g_string_free (buf, FALSE);
}

/*< private >
 * gtk_constraint_variable_is_external:
 * @variable: a `GtkConstraintVariable`
 *
 * Checks whether the @variable was introduced from outside the solver.
 *
 * Returns: %TRUE if the variable is external
 */
gboolean
gtk_constraint_variable_is_external (const GtkConstraintVariable *variable)
{
  return variable->is_external;
}

/*< private >
 * gtk_constraint_variable_is_pivotable:
 * @variable: a `GtkConstraintVariable`
 *
 * Checks whether the @variable can be used as a pivot.
 *
 * Returns: %TRUE if the variable is pivotable
 */
gboolean
gtk_constraint_variable_is_pivotable (const GtkConstraintVariable *variable)
{
  return variable->is_pivotable;
}

/*< private >
 * gtk_constraint_variable_is_restricted:
 * @variable: a `GtkConstraintVariable`
 *
 * Checks whether the @variable's use is restricted.
 *
 * Returns: %TRUE if the variable is restricted
 */
gboolean
gtk_constraint_variable_is_restricted (const GtkConstraintVariable *variable)
{
  return variable->is_restricted;
}

/*< private >
 * gtk_constraint_variable_is_dummy:
 * @variable: a `GtkConstraintVariable`
 *
 * Checks whether the @variable is a dummy symbol.
 *
 * Returns: %TRUE if the variable is a dummy symbol
 */
gboolean
gtk_constraint_variable_is_dummy (const GtkConstraintVariable *variable)
{
  return variable->_type == GTK_CONSTRAINT_SYMBOL_DUMMY;
}

/*< private >
 * GtkConstraintVariableSet:
 *
 * A set of variables.
 */
struct _GtkConstraintVariableSet {
  /* List<Variable>, owns a reference */
  GSequence *set;

  /* Age of the set, to guard against mutations while iterating */
  gint64 age;
};

/*< private >
 * gtk_constraint_variable_set_free:
 * @set: a `GtkConstraintVariable`Set
 *
 * Frees the resources associated to a `GtkConstraintVariable`Set/
 */
void
gtk_constraint_variable_set_free (GtkConstraintVariableSet *set)
{
  g_return_if_fail (set != NULL);

  g_sequence_free (set->set);

  g_free (set);
}

/*< private >
 * gtk_constraint_variable_set_new:
 *
 * Creates a new `GtkConstraintVariable`Set.
 *
 * Returns: the newly created variable set
 */
GtkConstraintVariableSet *
gtk_constraint_variable_set_new (void)
{
  GtkConstraintVariableSet *res = g_new (GtkConstraintVariableSet, 1);

  res->set = g_sequence_new ((GDestroyNotify) gtk_constraint_variable_unref);

  res->age = 0;

  return res;
}

static int
sort_by_variable_id (gconstpointer a,
                     gconstpointer b,
                     gpointer      data)
{
  const GtkConstraintVariable *va = a, *vb = b;

  if (va == vb)
    return 0;

  return va->_id - vb->_id;
}

/*< private >
 * gtk_constraint_variable_set_add:
 * @set: a `GtkConstraintVariable`Set
 * @variable: a `GtkConstraintVariable`
 *
 * Adds @variable to the given @set, if the @variable is not already
 * in it.
 *
 * The @set will acquire a reference on the @variable, and will release
 * it after calling gtk_constraint_variable_set_remove(), or when the @set
 * is freed.
 *
 * Returns: %TRUE if the variable was added to the set, and %FALSE otherwise
 */
gboolean
gtk_constraint_variable_set_add (GtkConstraintVariableSet *set,
                                 GtkConstraintVariable *variable)
{
  GSequenceIter *iter;

  iter = g_sequence_search (set->set, variable, sort_by_variable_id, NULL);
  if (!g_sequence_iter_is_end (iter))
    {
      GtkConstraintVariable *v = g_sequence_get (iter);
      if (v->_id == variable->_id)
        return FALSE;
    }

  g_sequence_insert_before (iter, gtk_constraint_variable_ref (variable));

  set->age += 1;

  return TRUE;
}

/*< private >
 * gtk_constraint_variable_set_remove:
 * @set: a `GtkConstraintVariable`Set
 * @variable: a `GtkConstraintVariable`
 *
 * Removes @variable from the @set.
 *
 * This function will release the reference on @variable held by the @set.
 *
 * Returns: %TRUE if the variable was removed from the set, and %FALSE
 *   otherwise
 */
gboolean
gtk_constraint_variable_set_remove (GtkConstraintVariableSet *set,
                                    GtkConstraintVariable *variable)
{
  GSequenceIter *iter;

  iter = g_sequence_lookup (set->set, variable, sort_by_variable_id, NULL);
  if (iter != NULL)
    {
      g_sequence_remove (iter);
      set->age += 1;

      return TRUE;
    }

  return FALSE;
}

/*< private >
 * gtk_constraint_variable_set_size:
 * @set: a `GtkConstraintVariable`Set
 *
 * Retrieves the size of the @set.
 *
 * Returns: the number of variables in the set
 */
int
gtk_constraint_variable_set_size (GtkConstraintVariableSet *set)
{
  return g_sequence_get_length (set->set);
}

gboolean
gtk_constraint_variable_set_is_empty (GtkConstraintVariableSet *set)
{
  return g_sequence_is_empty (set->set);
}

gboolean
gtk_constraint_variable_set_is_singleton (GtkConstraintVariableSet *set)
{
  return g_sequence_iter_next (g_sequence_get_begin_iter (set->set)) == g_sequence_get_end_iter (set->set);
}

/*< private >
 * GtkConstraintVariableSetIter:
 *
 * An iterator type for `GtkConstraintVariable`Set.
 */
/* Keep in sync with GtkConstraintVariableSetIter */
typedef struct {
  GtkConstraintVariableSet *set;
  GSequenceIter *iter;
  gint64 age;
} RealVariableSetIter;

#define REAL_VARIABLE_SET_ITER(i)       ((RealVariableSetIter *) (i))

/*< private >
 * gtk_constraint_variable_set_iter_init:
 * @iter: a `GtkConstraintVariable`SetIter
 * @set: the `GtkConstraintVariable`Set to iterate
 *
 * Initializes @iter for iterating over @set.
 */
void
gtk_constraint_variable_set_iter_init (GtkConstraintVariableSetIter *iter,
                                       GtkConstraintVariableSet *set)
{
  RealVariableSetIter *riter = REAL_VARIABLE_SET_ITER (iter);

  g_return_if_fail (iter != NULL);
  g_return_if_fail (set != NULL);

  riter->set = set;
  riter->iter = g_sequence_get_begin_iter (set->set);
  riter->age = set->age;
}

/*< private >
 * gtk_constraint_variable_set_iter_next:
 * @iter: a `GtkConstraintVariable`SetIter
 * @variable_p: (out): the next variable in the set
 *
 * Advances the @iter to the next variable in the `GtkConstraintVariable`Set.
 *
 * Returns: %TRUE if the iterator was advanced, and %FALSE otherwise
 */
gboolean
gtk_constraint_variable_set_iter_next (GtkConstraintVariableSetIter *iter,
                                       GtkConstraintVariable **variable_p)
{
  RealVariableSetIter *riter = REAL_VARIABLE_SET_ITER (iter);

  g_return_val_if_fail (iter != NULL, FALSE);
  g_return_val_if_fail (variable_p != NULL, FALSE);

  g_assert (riter->age == riter->set->age);

  if (g_sequence_iter_is_end (riter->iter))
    return FALSE;

  *variable_p = g_sequence_get (riter->iter);
  riter->iter = g_sequence_iter_next (riter->iter);

  return TRUE;
}

/*< private >
 * gtk_constraint_variable_pair_new:
 * @first: a `GtkConstraintVariable`
 * @second: a `GtkConstraintVariable`
 *
 * Creates a new `GtkConstraintVariable`Pair, containing @first and @second.
 *
 * The `GtkConstraintVariable`Pair acquires a reference over the two
 * given `GtkConstraintVariable`s.
 *
 * Returns: a new `GtkConstraintVariable`Pair
 */
GtkConstraintVariablePair *
gtk_constraint_variable_pair_new (GtkConstraintVariable *first,
                                  GtkConstraintVariable *second)
{
  GtkConstraintVariablePair *res = g_new (GtkConstraintVariablePair, 1);

  res->first = gtk_constraint_variable_ref (first);
  res->second = gtk_constraint_variable_ref (second);

  return res;
}

/*< private >
 * gtk_constraint_variable_pair_free:
 * @pair: a `GtkConstraintVariable`Pair
 *
 * Frees the resources associated by @pair.
 */
void
gtk_constraint_variable_pair_free (GtkConstraintVariablePair *pair)
{
  g_clear_pointer (&pair->first, gtk_constraint_variable_unref);
  g_clear_pointer (&pair->second, gtk_constraint_variable_unref);

  g_free (pair);
}

/* }}} */

/* {{{ Expressions */

/*< private >
 * Term:
 * @variable: a `GtkConstraintVariable`
 * @coefficient: the coefficient applied to the @variable
 * @next: the next term in the expression
 * @prev: the previous term in the expression;
 *
 * A tuple of (@variable, @coefficient) in an equation.
 *
 * The term acquires a reference on the variable.
 */
typedef struct _Term Term;

struct _Term {
  GtkConstraintVariable *variable;
  double coefficient;

  Term *next;
  Term *prev;
};

static Term *
term_new (GtkConstraintVariable *variable,
          double coefficient)
{
  Term *res = g_new (Term, 1);

  res->variable = gtk_constraint_variable_ref (variable);
  res->coefficient = coefficient;
  res->next = res->prev = NULL;

  return res;
}

static void
term_free (gpointer data)
{
  Term *term = data;

  if (term == NULL)
    return;

  gtk_constraint_variable_unref (term->variable);

  g_free (term);
}

struct _GtkConstraintExpression
{
  double constant;

  /* HashTable<Variable, Term>; the key is the term's variable,
   * and the value is owned by the hash table
   */
  GHashTable *terms;

  /* List of terms, in insertion order */
  Term *first_term;
  Term *last_term;

  /* Used by GtkConstraintExpressionIter to guard against changes
   * in the expression while iterating
   */
  gint64 age;
};

/*< private >
 * gtk_constraint_expression_add_term:
 * @self: a `GtkConstraintExpression`
 * @variable: a `GtkConstraintVariable`
 * @coefficient: a coefficient for @variable
 *
 * Adds a new term formed by (@variable, @coefficient) into a
 * `GtkConstraintExpression`.
 *
 * The @expression acquires a reference on @variable.
 */
static void
gtk_constraint_expression_add_term (GtkConstraintExpression *self,
                                    GtkConstraintVariable *variable,
                                    double coefficient)
{
  Term *term;

  if (self->terms == NULL)
    {
      g_assert (self->first_term == NULL && self->last_term == NULL);
      self->terms = g_hash_table_new_full (NULL, NULL,
                                           NULL,
                                           term_free);
    }

  term = term_new (variable, coefficient);

  g_hash_table_insert (self->terms, term->variable, term);

  if (self->first_term == NULL)
    self->first_term = term;

  term->prev = self->last_term;

  if (self->last_term != NULL)
    self->last_term->next = term;

  self->last_term = term;

  /* Increase the age of the expression, so that we can catch
   * mutations from within an iteration over the terms
   */
  self->age += 1;
}

static void
gtk_constraint_expression_remove_term (GtkConstraintExpression *self,
                                       GtkConstraintVariable *variable)
{
  Term *term, *iter;

  if (self->terms == NULL)
    return;

  term = g_hash_table_lookup (self->terms, variable);
  if (term == NULL)
    return;

  /* Keep the variable alive for the duration of the function */
  gtk_constraint_variable_ref (variable);

  iter = self->first_term;
  while (iter != NULL)
    {
      Term *next = iter->next;
      Term *prev = iter->prev;

      if (iter == term)
        {
          if (prev != NULL)
            prev->next = next;
          if (next != NULL)
            next->prev = prev;

          if (iter == self->first_term)
            self->first_term = next;
          if (iter == self->last_term)
            self->last_term = prev;

          iter->next = NULL;
          iter->prev = NULL;

          break;
        }

      iter = next;
    }

  g_hash_table_remove (self->terms, variable);

  gtk_constraint_variable_unref (variable);

  self->age += 1;
}

/*< private >
 * gtk_constraint_expression_new:
 * @constant: a constant for the expression
 *
 * Creates a new `GtkConstraintExpression` with the given @constant.
 *
 * Returns: (transfer full): the newly created expression
 */
GtkConstraintExpression *
gtk_constraint_expression_new (double constant)
{
  GtkConstraintExpression *res = g_rc_box_new (GtkConstraintExpression);

  res->age = 0;
  res->terms = NULL;
  res->first_term = NULL;
  res->last_term = NULL;
  res->constant = constant;

  return res;
}

/*< private >
 * gtk_constraint_expression_new_from_variable:
 * @variable: a `GtkConstraintVariable`
 *
 * Creates a new `GtkConstraintExpression` with the given @variable.
 *
 * Returns: (transfer full): the newly created expression
 */
GtkConstraintExpression *
gtk_constraint_expression_new_from_variable (GtkConstraintVariable *variable)
{
  GtkConstraintExpression *res = gtk_constraint_expression_new (0.0);

  gtk_constraint_expression_add_term (res, variable, 1.0);

  return res;
}

/*< private >
 * gtk_constraint_expression_ref:
 * @expression: a `GtkConstraintExpression`
 *
 * Acquires a reference on @expression.
 *
 * Returns: (transfer full): the @expression, with its reference
 *   count increased
 */
GtkConstraintExpression *
gtk_constraint_expression_ref (GtkConstraintExpression *expression)
{
  g_return_val_if_fail (expression != NULL, NULL);

  return g_rc_box_acquire (expression);
}

static void
gtk_constraint_expression_clear (gpointer data)
{
  GtkConstraintExpression *self = data;

  g_clear_pointer (&self->terms, g_hash_table_unref);

  self->age = 0;
  self->constant = 0.0;
  self->first_term = NULL;
  self->last_term = NULL;
}

/*< private >
 * gtk_constraint_expression_unref:
 * @expression: (transfer full): a `GtkConstraintExpression`
 *
 * Releases a reference on @expression.
 */
void
gtk_constraint_expression_unref (GtkConstraintExpression *expression)
{
  g_rc_box_release_full (expression, gtk_constraint_expression_clear);
}

/*< private >
 * gtk_constraint_expression_is_constant:
 * @expression: a `GtkConstraintExpression`
 *
 * Checks whether @expression is a constant value, with no variable terms.
 *
 * Returns: %TRUE if the @expression is a constant
 */
gboolean
gtk_constraint_expression_is_constant (const GtkConstraintExpression *expression)
{
  return expression->terms == NULL;
}

/*< private >
 * gtk_constraint_expression_set_constant:
 * @expression: a `GtkConstraintExpression`
 * @constant: the value of the constant
 *
 * Sets the value of the constant part of @expression.
 */
void
gtk_constraint_expression_set_constant (GtkConstraintExpression *expression,
                                        double constant)
{
  g_return_if_fail (expression != NULL);

  expression->constant = constant;
}

/*< private >
 * gtk_constraint_expression_get_constant:
 * @expression: a `GtkConstraintExpression`
 *
 * Retrieves the constant value of @expression.
 *
 * Returns: the constant of @expression
 */
double
gtk_constraint_expression_get_constant (const GtkConstraintExpression *expression)
{
  g_return_val_if_fail (expression != NULL, 0.0);

  return expression->constant;
}

GtkConstraintExpression *
gtk_constraint_expression_clone (GtkConstraintExpression *expression)
{
  GtkConstraintExpression *res;
  Term *iter;

  res = gtk_constraint_expression_new (expression->constant);

  iter = expression->first_term;
  while (iter != NULL)
    {
      gtk_constraint_expression_add_term (res, iter->variable, iter->coefficient);

      iter = iter->next;
    }

  return res;
}

/*< private >
 * gtk_constraint_expression_add_variable:
 * @expression: a `GtkConstraintExpression`
 * @variable: a `GtkConstraintVariable` to add to @expression
 * @coefficient: the coefficient of @variable
 * @subject: (nullable): a `GtkConstraintVariable`
 * @solver: (nullable): a `GtkConstraintSolver`
 *
 * Adds a `(@coefficient × @variable)` term to @expression.
 *
 * If @expression already contains a term for @variable, this function will
 * update its coefficient.
 *
 * If @coefficient is 0 and @expression already contains a term for @variable,
 * the term for @variable will be removed.
 *
 * This function will notify @solver if @variable is added or removed from
 * the @expression.
 */
void
gtk_constraint_expression_add_variable (GtkConstraintExpression *expression,
                                        GtkConstraintVariable *variable,
                                        double coefficient,
                                        GtkConstraintVariable *subject,
                                        GtkConstraintSolver *solver)
{
  /* If the expression already contains the variable, update the coefficient */
  if (expression->terms != NULL)
    {
      Term *t = g_hash_table_lookup (expression->terms, variable);

      if (t != NULL)
        {
          double new_coefficient = t->coefficient + coefficient;

          /* Setting the coefficient to 0 will remove the variable */
          if (G_APPROX_VALUE (new_coefficient, 0.0, 0.001))
            {
              /* Update the tableau if needed */
              if (solver != NULL)
                gtk_constraint_solver_note_removed_variable (solver, variable, subject);

              gtk_constraint_expression_remove_term (expression, variable);
            }
          else
            {
              t->coefficient = new_coefficient;
            }

          return;
        }
    }

  /* Otherwise, add the variable if the coefficient is non-zero */
  if (!G_APPROX_VALUE (coefficient, 0.0, 0.001))
    {
      gtk_constraint_expression_add_term (expression, variable, coefficient);

      if (solver != NULL)
        gtk_constraint_solver_note_added_variable (solver, variable, subject);
    }
}

/*< private >
 * gtk_constraint_expression_remove_variable:
 * @expression: a `GtkConstraintExpression`
 * @variable: a `GtkConstraintVariable`
 *
 * Removes @variable from @expression.
 */
void
gtk_constraint_expression_remove_variable (GtkConstraintExpression *expression,
                                           GtkConstraintVariable *variable)
{
  g_return_if_fail (expression != NULL);
  g_return_if_fail (variable != NULL);

  gtk_constraint_expression_remove_term (expression, variable);
}

/*< private >
 * gtk_constraint_expression_set_variable:
 * @expression: a `GtkConstraintExpression`
 * @variable: a `GtkConstraintVariable`
 * @coefficient: a coefficient for @variable
 *
 * Sets the @coefficient for @variable inside an @expression.
 *
 * If the @expression does not contain a term for @variable, a new
 * one will be added.
 */
void
gtk_constraint_expression_set_variable (GtkConstraintExpression *expression,
                                        GtkConstraintVariable *variable,
                                        double coefficient)
{
  if (expression->terms != NULL)
    {
      Term *t = g_hash_table_lookup (expression->terms, variable);

      if (t != NULL)
        {
          t->coefficient = coefficient;
          return;
        }
    }

  gtk_constraint_expression_add_term (expression, variable, coefficient);
}

/*< private >
 * gtk_constraint_expression_add_expression:
 * @a_expr: first operand
 * @b_expr: second operand
 * @n: the multiplication factor for @b_expr
 * @subject: (nullable): a `GtkConstraintVariable`
 * @solver: (nullable): a `GtkConstraintSolver`
 *
 * Adds `(@n × @b_expr)` to @a_expr.
 *
 * Typically, this function is used to turn two expressions in the
 * form:
 *
 * |[
 *   a.x + a.width = b.x + b.width
 * ]|
 *
 * into a single expression:
 *
 * |[
 *   a.x + a.width - b.x - b.width = 0
 * ]|
 *
 * If @solver is not %NULL, this function will notify a `GtkConstraintSolver`
 * of every variable that was added or removed from @a_expr.
 */
void
gtk_constraint_expression_add_expression (GtkConstraintExpression *a_expr,
                                          GtkConstraintExpression *b_expr,
                                          double n,
                                          GtkConstraintVariable *subject,
                                          GtkConstraintSolver *solver)
{
  Term *iter;

  a_expr->constant += (n * b_expr->constant);

  iter = b_expr->last_term;
  while (iter != NULL)
    {
      Term *next = iter->prev;

      gtk_constraint_expression_add_variable (a_expr,
                                              iter->variable, n * iter->coefficient,
                                              subject,
                                              solver);

      iter = next;
    }
}

/*< private >
 * gtk_constraint_expression_plus_constant:
 * @expression: a `GtkConstraintExpression`
 * @constant: a constant value
 *
 * Adds a @constant value to the @expression.
 *
 * This is the equivalent of creating a new `GtkConstraintExpression` for
 * the @constant and calling gtk_constraint_expression_add_expression().
 *
 * Returns: the @expression
 */
GtkConstraintExpression *
gtk_constraint_expression_plus_constant (GtkConstraintExpression *expression,
                                         double constant)
{
  GtkConstraintExpression *e;

  e = gtk_constraint_expression_new (constant);
  gtk_constraint_expression_add_expression (expression, e, 1.0, NULL, NULL);
  gtk_constraint_expression_unref (e);

  return expression;
}

/*< private >
 * gtk_constraint_expression_minus_constant:
 * @expression: a `GtkConstraintExpression`
 * @constant: a constant value
 *
 * Removes a @constant value from the @expression.
 *
 * This is the equivalent of creating a new `GtkConstraintExpression` for
 * the inverse of @constant and calling gtk_constraint_expression_add_expression().
 *
 * Returns: the @expression
 */
GtkConstraintExpression *
gtk_constraint_expression_minus_constant (GtkConstraintExpression *expression,
                                          double constant)
{
  return gtk_constraint_expression_plus_constant (expression, constant * -1.0);
}

/*< private >
 * gtk_constraint_expression_plus_variable:
 * @expression: a `GtkConstraintExpression`
 * @variable: a `GtkConstraintVariable`
 *
 * Adds a @variable to the @expression.
 *
 * Returns: the @expression
 */
GtkConstraintExpression *
gtk_constraint_expression_plus_variable (GtkConstraintExpression *expression,
                                         GtkConstraintVariable *variable)
{
  GtkConstraintExpression *e;

  e = gtk_constraint_expression_new_from_variable (variable);
  gtk_constraint_expression_add_expression (expression, e, 1.0, NULL, NULL);
  gtk_constraint_expression_unref (e);

  return expression;
}

/*< private >
 * gtk_constraint_expression_minus_variable:
 * @expression: a `GtkConstraintExpression`
 * @variable: a `GtkConstraintVariable`
 *
 * Subtracts a @variable from the @expression.
 *
 * Returns: the @expression
 */
GtkConstraintExpression *
gtk_constraint_expression_minus_variable (GtkConstraintExpression *expression,
                                          GtkConstraintVariable *variable)
{
  GtkConstraintExpression *e;

  e = gtk_constraint_expression_new_from_variable (variable);
  gtk_constraint_expression_add_expression (expression, e, -1.0, NULL, NULL);
  gtk_constraint_expression_unref (e);

  return expression;
}

/*< private >
 * gtk_constraint_expression_multiply_by:
 * @expression: a `GtkConstraintExpression`
 * @factor: the multiplication factor
 *
 * Multiplies the constant part and the coefficient of all terms
 * in @expression with the given @factor.
 *
 * Returns: the @expression
 */
GtkConstraintExpression *
gtk_constraint_expression_multiply_by (GtkConstraintExpression *expression,
                                       double factor)
{
  GHashTableIter iter;
  gpointer value_p;

  expression->constant *= factor;

  if (expression->terms == NULL)
    return expression;

  g_hash_table_iter_init (&iter, expression->terms);
  while (g_hash_table_iter_next (&iter, NULL, &value_p))
    {
      Term *t = value_p;

      t->coefficient *= factor;
    }

  return expression;
}

/*< private >
 * gtk_constraint_expression_divide_by:
 * @expression: a `GtkConstraintExpression`
 * @factor: the division factor
 *
 * Divides the constant part and the coefficient of all terms
 * in @expression by the given @factor.
 *
 * Returns: the @expression
 */
GtkConstraintExpression *
gtk_constraint_expression_divide_by (GtkConstraintExpression *expression,
                                     double factor)
{
  if (G_APPROX_VALUE (factor, 0.0, 0.001))
    return expression;

  return gtk_constraint_expression_multiply_by (expression, 1.0 / factor);
}

/*< private >
 * gtk_constraint_expression_new_subject:
 * @expression: a `GtkConstraintExpression`
 * @subject: a `GtkConstraintVariable` part of @expression
 *
 * Modifies @expression to have a new @subject.
 *
 * A `GtkConstraintExpression` is a linear expression in the form of
 * `@expression = 0`. If @expression contains @subject, for instance:
 *
 * |[
 *   c + (a × @subject) + (a1 × v1) + … + (an × vn) = 0
 * ]|
 *
 * this function will make @subject the new subject of the expression:
 *
 * |[
 *   subject = - (c / a) - ((a1 / a) × v1) - … - ((an / a) × vn) = 0
 * ]|
 *
 * The term @subject is removed from the @expression.
 *
 * Returns: the reciprocal of the coefficient of @subject, so we
 *   can use this function in gtk_constraint_expression_change_subject()
 */
double
gtk_constraint_expression_new_subject (GtkConstraintExpression *expression,
                                       GtkConstraintVariable *subject)
{
  double reciprocal = 1.0;
  Term *term;

  g_assert (!gtk_constraint_expression_is_constant (expression));

  term = g_hash_table_lookup (expression->terms, subject);
  g_assert (term != NULL);
  g_assert (!G_APPROX_VALUE (term->coefficient, 0.0, 0.001));

  reciprocal = 1.0 / term->coefficient;

  gtk_constraint_expression_remove_term (expression, subject);
  gtk_constraint_expression_multiply_by (expression, -reciprocal);

  return reciprocal;
}

/*< private >
 * gtk_constraint_expression_change_subject:
 * @expression: a `GtkConstraintExpression`
 * @old_subject: the old subject `GtkConstraintVariable` of @expression
 * @new_subject: the new subject `GtkConstraintVariable` of @expression
 *
 * Turns an @expression in the form of:
 *
 * |[
 *   old_subject = c + (a × new_subject) + (a1 × v1) + … + (an × vn)
 * ]|
 *
 * into the form of:
 *
 * |[
 *   new_subject = -c / a + old_subject / a - ((a1 / a) × v1) - … - ((an / a) × vn)
 * ]|
 *
 * Which means resolving @expression for @new_subject.
 */
void
gtk_constraint_expression_change_subject (GtkConstraintExpression *expression,
                                          GtkConstraintVariable *old_subject,
                                          GtkConstraintVariable *new_subject)
{
  double reciprocal;

  g_return_if_fail (expression != NULL);
  g_return_if_fail (old_subject != NULL);
  g_return_if_fail (new_subject != NULL);

  reciprocal = gtk_constraint_expression_new_subject (expression, new_subject);
  gtk_constraint_expression_set_variable (expression, old_subject, reciprocal);
}

/*< private >
 * gtk_constraint_expression_get_coefficient:
 * @expression: a `GtkConstraintExpression`
 * @variable: a `GtkConstraintVariable`
 *
 * Retrieves the coefficient of the term for @variable inside @expression.
 *
 * Returns: the coefficient of @variable
 */
double
gtk_constraint_expression_get_coefficient (GtkConstraintExpression *expression,
                                           GtkConstraintVariable *variable)
{
  const Term *term;

  g_return_val_if_fail (expression != NULL, 0.0);
  g_return_val_if_fail (variable != NULL, 0.0);

  if (expression->terms == NULL)
    return 0.0;

  term = g_hash_table_lookup (expression->terms, variable);
  if (term == NULL)
    return 0.0;

  return term->coefficient;
}

/*< private >
 * gtk_constraint_expression_substitute_out:
 * @expression: a `GtkConstraintExpression`
 * @out_var: the variable to replace
 * @expr: the expression used to replace @out_var
 * @subject: (nullable): a `GtkConstraintVariable`
 * @solver: (nullable): a `GtkConstraintSolver`
 *
 * Replaces every term containing @out_var inside @expression with @expr.
 *
 * If @solver is not %NULL, this function will notify the `GtkConstraintSolver`
 * for every variable added to or removed from @expression.
 */
void
gtk_constraint_expression_substitute_out (GtkConstraintExpression *expression,
                                          GtkConstraintVariable *out_var,
                                          GtkConstraintExpression *expr,
                                          GtkConstraintVariable *subject,
                                          GtkConstraintSolver *solver)
{
  double multiplier;
  Term *iter;

  if (expression->terms == NULL)
    return;

  multiplier = gtk_constraint_expression_get_coefficient (expression, out_var);
  gtk_constraint_expression_remove_term (expression, out_var);

  expression->constant = expression->constant + multiplier * expr->constant;

  iter = expr->first_term;
  while (iter != NULL)
    {
      GtkConstraintVariable *clv = iter->variable;
      double coeff = iter->coefficient;
      Term *next = iter->next;

      if (expression->terms != NULL &&
          g_hash_table_contains (expression->terms, clv))
        {
          double old_coefficient = gtk_constraint_expression_get_coefficient (expression, clv);
          double new_coefficient = old_coefficient + multiplier * coeff;

          if (G_APPROX_VALUE (new_coefficient, 0.0, 0.001))
            {
              if (solver != NULL)
                gtk_constraint_solver_note_removed_variable (solver, clv, subject);

              gtk_constraint_expression_remove_term (expression, clv);
            }
          else
            gtk_constraint_expression_set_variable (expression, clv, new_coefficient);
        }
      else
        {
          gtk_constraint_expression_set_variable (expression, clv, multiplier * coeff);

          if (solver != NULL)
            gtk_constraint_solver_note_added_variable (solver, clv, subject);
        }

      iter = next;
    }
}

/*< private >
 * gtk_constraint_expression_get_pivotable_variable:
 * @expression: a `GtkConstraintExpression`
 *
 * Retrieves the first `GtkConstraintVariable` in @expression that
 * is marked as pivotable.
 *
 * Returns: (transfer none) (nullable): a `GtkConstraintVariable`
 */
GtkConstraintVariable *
gtk_constraint_expression_get_pivotable_variable (GtkConstraintExpression *expression)
{
  Term *iter;

  if (expression->terms == NULL)
    {
      g_critical ("Expression %p is a constant", expression);
      return NULL;
    }

  iter = expression->first_term;
  while (iter != NULL)
    {
      Term *next = iter->next;

      if (gtk_constraint_variable_is_pivotable (iter->variable))
        return iter->variable;

      iter = next;
    }

  return NULL;
}

/*< private >
 * gtk_constraint_expression_to_string:
 * @expression: a `GtkConstraintExpression`
 *
 * Creates a string containing @expression.
 *
 * This function is only useful for debugging.
 *
 * Returns: (transfer full): a string containing the given expression
 */
char *
gtk_constraint_expression_to_string (const GtkConstraintExpression *expression)
{
  gboolean needs_plus = FALSE;
  GString *buf;
  Term *iter;

  if (expression == NULL)
    return g_strdup ("<null>");

  buf = g_string_new (NULL);

  if (!G_APPROX_VALUE (expression->constant, 0.0, 0.001))
    {
      g_string_append_printf (buf, "%g", expression->constant);

      if (expression->terms != NULL)
        needs_plus = TRUE;
    }

  if (expression->terms == NULL)
    return g_string_free (buf, FALSE);

  iter = expression->first_term;
  while (iter != NULL)
    {
      char *str = gtk_constraint_variable_to_string (iter->variable);
      Term *next = iter->next;

      if (needs_plus)
        g_string_append (buf, " + ");

      if (G_APPROX_VALUE (iter->coefficient, 1.0, 0.001))
        g_string_append_printf (buf, "%s", str);
      else
        g_string_append_printf (buf, "(%g * %s)", iter->coefficient, str);

      g_free (str);

      if (!needs_plus)
        needs_plus = TRUE;

      iter = next;
    }

  return g_string_free (buf, FALSE);
}

/* Keep in sync with GtkConstraintExpressionIter */
typedef struct {
  GtkConstraintExpression *expression;
  Term *current;
  gint64 age;
} RealExpressionIter;

#define REAL_EXPRESSION_ITER(i) ((RealExpressionIter *) (i))

/*< private >
 * gtk_constraint_expression_iter_init:
 * @iter: a `GtkConstraintExpression`Iter
 * @expression: a `GtkConstraintExpression`
 *
 * Initializes an iterator over @expression.
 */
void
gtk_constraint_expression_iter_init (GtkConstraintExpressionIter *iter,
                                     GtkConstraintExpression *expression)
{
  RealExpressionIter *riter = REAL_EXPRESSION_ITER (iter);

  riter->expression = expression;
  riter->current = NULL;
  riter->age = expression->age;
}

/*< private >
 * gtk_constraint_expression_iter_next:
 * @iter: a valid `GtkConstraintExpression`Iter
 * @variable: (out): the variable of the next term
 * @coefficient: (out): the coefficient of the next term
 *
 * Moves the given `GtkConstraintExpression`Iter forwards to the next
 * term in the expression, starting from the first term.
 *
 * Returns: %TRUE if the iterator was moved, and %FALSE if the iterator
 *   has reached the end of the terms of the expression
 */
gboolean
gtk_constraint_expression_iter_next (GtkConstraintExpressionIter *iter,
                                     GtkConstraintVariable **variable,
                                     double *coefficient)
{
  RealExpressionIter *riter = REAL_EXPRESSION_ITER (iter);

  g_assert (riter->age == riter->expression->age);

  if (riter->current == NULL)
    riter->current = riter->expression->first_term;
  else
    riter->current = riter->current->next;

  if (riter->current != NULL)
    {
      *coefficient = riter->current->coefficient;
      *variable = riter->current->variable;
    }

  return riter->current != NULL;
}

/*< private >
 * gtk_constraint_expression_iter_prev:
 * @iter: a valid `GtkConstraintExpression`Iter
 * @variable: (out): the variable of the previous term
 * @coefficient: (out): the coefficient of the previous term
 *
 * Moves the given `GtkConstraintExpression`Iter backwards to the previous
 * term in the expression, starting from the last term.
 *
 * Returns: %TRUE if the iterator was moved, and %FALSE if the iterator
 *   has reached the beginning of the terms of the expression
 */
gboolean
gtk_constraint_expression_iter_prev (GtkConstraintExpressionIter *iter,
                                     GtkConstraintVariable **variable,
                                     double *coefficient)
{
  RealExpressionIter *riter = REAL_EXPRESSION_ITER (iter);

  g_assert (riter->age == riter->expression->age);

  if (riter->current == NULL)
    riter->current = riter->expression->last_term;
  else
    riter->current = riter->current->prev;

  if (riter->current != NULL)
    {
      *coefficient = riter->current->coefficient;
      *variable = riter->current->variable;
    }

  return riter->current != NULL;
}

typedef enum {
  BUILDER_OP_NONE,
  BUILDER_OP_PLUS,
  BUILDER_OP_MINUS,
  BUILDER_OP_MULTIPLY,
  BUILDER_OP_DIVIDE
} BuilderOpType;

typedef struct
{
  GtkConstraintExpression *expression;
  GtkConstraintSolver *solver;
  int op;
} RealExpressionBuilder;

#define REAL_EXPRESSION_BUILDER(b) ((RealExpressionBuilder *) (b))

/*< private >
 * gtk_constraint_expression_builder_init:
 * @builder: a `GtkConstraintExpression`Builder
 * @solver: a `GtkConstraintSolver`
 *
 * Initializes the given `GtkConstraintExpression`Builder for the
 * given `GtkConstraintSolver`.
 *
 * You can use the @builder to construct expressions to be added to the
 * @solver, in the form of constraints.
 *
 * A typical use is:
 *
 * |[<!-- language="C" -->
 *   GtkConstraintExpressionBuilder builder;
 *
 *   // "solver" is set in another part of the code
 *   gtk_constraint_expression_builder_init (&builder, solver);
 *
 *   // "width" is set in another part of the code
 *   gtk_constraint_expression_builder_term (&builder, width);
 *   gtk_constraint_expression_builder_divide_by (&builder);
 *   gtk_constraint_expression_builder_constant (&builder, 2.0);
 *
 *   // "left" is set in another part of the code
 *   gtk_constraint_expression_builder_plus (&builder);
 *   gtk_constraint_expression_builder_term (&builder, left);
 *
 *   // "expr" now contains the following expression:
 *   //     width / 2.0 + left
 *   GtkConstraintExpression *expr =
 *     gtk_constraint_expression_builder_finish (&builder);
 *
 *   // The builder is inert, and can be re-used by calling
 *   // gtk_constraint_expression_builder_init() again.
 * ]|
 */
void
gtk_constraint_expression_builder_init (GtkConstraintExpressionBuilder *builder,
                                        GtkConstraintSolver *solver)
{
  RealExpressionBuilder *rbuilder = REAL_EXPRESSION_BUILDER (builder);

  rbuilder->solver = solver;
  rbuilder->expression = gtk_constraint_expression_new (0);
  rbuilder->op = BUILDER_OP_NONE;
}

/*< private >
 * gtk_constraint_expression_builder_term:
 * @builder: a `GtkConstraintExpression`Builder
 * @term: a `GtkConstraintVariable`
 *
 * Adds a variable @term to the @builder.
 */
void
gtk_constraint_expression_builder_term (GtkConstraintExpressionBuilder *builder,
                                        GtkConstraintVariable *term)
{
  RealExpressionBuilder *rbuilder = REAL_EXPRESSION_BUILDER (builder);
  GtkConstraintExpression *expr;

  expr = gtk_constraint_expression_new_from_variable (term);

  switch (rbuilder->op)
    {
    case BUILDER_OP_NONE:
      g_clear_pointer (&rbuilder->expression, gtk_constraint_expression_unref);
      rbuilder->expression = g_steal_pointer (&expr);
      break;

    case BUILDER_OP_PLUS:
      gtk_constraint_expression_add_expression (rbuilder->expression,
                                                expr, 1.0,
                                                NULL,
                                                NULL);
      gtk_constraint_expression_unref (expr);
      break;

    case BUILDER_OP_MINUS:
      gtk_constraint_expression_add_expression (rbuilder->expression,
                                                expr, -1.0,
                                                NULL,
                                                NULL);
      gtk_constraint_expression_unref (expr);
      break;

    default:
      break;
    }

  rbuilder->op = BUILDER_OP_NONE;
}

/*< private >
 * gtk_constraint_expression_builder_plus:
 * @builder: a `GtkConstraintExpression`Builder
 *
 * Adds a plus operator to the @builder.
 */
void
gtk_constraint_expression_builder_plus (GtkConstraintExpressionBuilder *builder)
{
  RealExpressionBuilder *rbuilder = REAL_EXPRESSION_BUILDER (builder);

  rbuilder->op = BUILDER_OP_PLUS;
}

/*< private >
 * gtk_constraint_expression_builder_minus:
 * @builder: a `GtkConstraintExpression`Builder
 *
 * Adds a minus operator to the @builder.
 */
void
gtk_constraint_expression_builder_minus (GtkConstraintExpressionBuilder *builder)
{
  RealExpressionBuilder *rbuilder = REAL_EXPRESSION_BUILDER (builder);

  rbuilder->op = BUILDER_OP_MINUS;
}

/*< private >
 * gtk_constraint_expression_builder_divide_by:
 * @builder: a `GtkConstraintExpression`Builder
 *
 * Adds a division operator to the @builder.
 */
void
gtk_constraint_expression_builder_divide_by (GtkConstraintExpressionBuilder *builder)
{
  RealExpressionBuilder *rbuilder = REAL_EXPRESSION_BUILDER (builder);

  rbuilder->op = BUILDER_OP_DIVIDE;
}

/*< private >
 * gtk_constraint_expression_builder_multiply_by:
 * @builder: a `GtkConstraintExpression`Builder
 *
 * Adds a multiplication operator to the @builder.
 */
void
gtk_constraint_expression_builder_multiply_by (GtkConstraintExpressionBuilder *builder)

{
  RealExpressionBuilder *rbuilder = REAL_EXPRESSION_BUILDER (builder);

  rbuilder->op = BUILDER_OP_MULTIPLY;
}

/*< private >
 * gtk_constraint_expression_builder_constant:
 * @builder: a `GtkConstraintExpression`Builder
 * @value: a constant value
 *
 * Adds a constant value to the @builder.
 */
void
gtk_constraint_expression_builder_constant (GtkConstraintExpressionBuilder *builder,
                                            double value)
{
  RealExpressionBuilder *rbuilder = REAL_EXPRESSION_BUILDER (builder);

  switch (rbuilder->op)
    {
    case BUILDER_OP_NONE:
      gtk_constraint_expression_set_constant (rbuilder->expression, value);
      break;

    case BUILDER_OP_PLUS:
      gtk_constraint_expression_plus_constant (rbuilder->expression, value);
      break;

    case BUILDER_OP_MINUS:
      gtk_constraint_expression_minus_constant (rbuilder->expression, value);
      break;

    case BUILDER_OP_MULTIPLY:
      gtk_constraint_expression_multiply_by (rbuilder->expression, value);
      break;

    case BUILDER_OP_DIVIDE:
      gtk_constraint_expression_divide_by (rbuilder->expression, value);
      break;

    default:
      break;
    }

  rbuilder->op = BUILDER_OP_NONE;
}

/*< private >
 * gtk_constraint_expression_builder_finish:
 * @builder: a `GtkConstraintExpression`Builder
 *
 * Closes the given expression builder, and returns the expression.
 *
 * You can only call this function once.
 *
 * Returns: (transfer full): the built expression
 */
GtkConstraintExpression *
gtk_constraint_expression_builder_finish (GtkConstraintExpressionBuilder *builder)
{
  RealExpressionBuilder *rbuilder = REAL_EXPRESSION_BUILDER (builder);

  rbuilder->solver = NULL;
  rbuilder->op = BUILDER_OP_NONE;

  return g_steal_pointer (&rbuilder->expression);
}

/* }}} */
