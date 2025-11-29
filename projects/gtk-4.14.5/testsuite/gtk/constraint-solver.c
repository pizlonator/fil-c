#include <locale.h>

#include <gtk/gtk.h>
#include "../../gtk/gtkconstrainttypesprivate.h"
#include "../../gtk/gtkconstraintsolverprivate.h"
#include "../../gtk/gtkconstraintexpressionprivate.h"

static void
constraint_solver_simple (void)
{
  GtkConstraintSolver *solver = gtk_constraint_solver_new ();

  GtkConstraintVariable *x = gtk_constraint_solver_create_variable (solver, NULL, "x", 167.0);
  GtkConstraintVariable *y = gtk_constraint_solver_create_variable (solver, NULL, "y", 2.0);

  GtkConstraintExpression *e = gtk_constraint_expression_new_from_variable (y);

  gtk_constraint_solver_add_constraint (solver,
                                        x, GTK_CONSTRAINT_RELATION_EQ, e,
                                        GTK_CONSTRAINT_STRENGTH_REQUIRED);

  double x_value = gtk_constraint_variable_get_value (x);
  double y_value = gtk_constraint_variable_get_value (y);

  g_assert_cmpfloat_with_epsilon (x_value, y_value, 0.001);
  g_assert_cmpfloat_with_epsilon (x_value, 0.0, 0.001);
  g_assert_cmpfloat_with_epsilon (y_value, 0.0, 0.001);

  gtk_constraint_variable_unref (y);
  gtk_constraint_variable_unref (x);

  g_object_unref (solver);
}

static void
constraint_solver_stay (void)
{
  GtkConstraintSolver *solver = gtk_constraint_solver_new ();

  GtkConstraintVariable *x = gtk_constraint_solver_create_variable (solver, NULL, "x", 5.0);
  GtkConstraintVariable *y = gtk_constraint_solver_create_variable (solver, NULL, "y", 10.0);

  gtk_constraint_solver_add_stay_variable (solver, x, GTK_CONSTRAINT_STRENGTH_WEAK);
  gtk_constraint_solver_add_stay_variable (solver, y, GTK_CONSTRAINT_STRENGTH_WEAK);

  double x_value = gtk_constraint_variable_get_value (x);
  double y_value = gtk_constraint_variable_get_value (y);

  g_assert_cmpfloat_with_epsilon (x_value, 5.0, 0.001);
  g_assert_cmpfloat_with_epsilon (y_value, 10.0, 0.001);

  gtk_constraint_variable_unref (x);
  gtk_constraint_variable_unref (y);

  g_object_unref (solver);
}

static void
constraint_solver_variable_geq_constant (void)
{
  GtkConstraintSolver *solver = gtk_constraint_solver_new ();

  GtkConstraintVariable *x = gtk_constraint_solver_create_variable (solver, NULL, "x", 10.0);
  GtkConstraintExpression *e = gtk_constraint_expression_new (100.0);

  gtk_constraint_solver_add_constraint (solver,
                                        x, GTK_CONSTRAINT_RELATION_GE, e,
                                        GTK_CONSTRAINT_STRENGTH_REQUIRED);

  double x_value = gtk_constraint_variable_get_value (x);

  g_assert_cmpfloat (x_value, >=, 100.0);

  gtk_constraint_variable_unref (x);

  g_object_unref (solver);
}

static void
constraint_solver_variable_leq_constant (void)
{
  GtkConstraintSolver *solver = gtk_constraint_solver_new ();

  GtkConstraintVariable *x = gtk_constraint_solver_create_variable (solver, NULL, "x", 100.0);
  GtkConstraintExpression *e = gtk_constraint_expression_new (10.0);

  gtk_constraint_solver_add_constraint (solver,
                                        x, GTK_CONSTRAINT_RELATION_LE, e,
                                        GTK_CONSTRAINT_STRENGTH_REQUIRED);

  double x_value = gtk_constraint_variable_get_value (x);

  g_assert_cmpfloat (x_value, <=, 10.0);

  gtk_constraint_variable_unref (x);

  g_object_unref (solver);
}

static void
constraint_solver_variable_eq_constant (void)
{
  GtkConstraintSolver *solver = gtk_constraint_solver_new ();

  GtkConstraintVariable *x = gtk_constraint_solver_create_variable (solver, NULL, "x", 10.0);
  GtkConstraintExpression *e = gtk_constraint_expression_new (100.0);

  gtk_constraint_solver_add_constraint (solver,
                                        x, GTK_CONSTRAINT_RELATION_EQ, e,
                                        GTK_CONSTRAINT_STRENGTH_REQUIRED);

  double x_value = gtk_constraint_variable_get_value (x);

  g_assert_cmpfloat_with_epsilon (x_value, 100.0, 0.001);

  gtk_constraint_variable_unref (x);

  g_object_unref (solver);
}

static void
constraint_solver_eq_with_stay (void)
{
  GtkConstraintSolver *solver = gtk_constraint_solver_new ();

  GtkConstraintVariable *x = gtk_constraint_solver_create_variable (solver, NULL, "x", 10.0);
  GtkConstraintVariable *width = gtk_constraint_solver_create_variable (solver, NULL, "width", 10.0);
  GtkConstraintVariable *right_min = gtk_constraint_solver_create_variable (solver, NULL, "rightMin", 100.0);

  GtkConstraintExpressionBuilder builder;
  gtk_constraint_expression_builder_init (&builder, solver);
  gtk_constraint_expression_builder_term (&builder, x);
  gtk_constraint_expression_builder_plus (&builder);
  gtk_constraint_expression_builder_term (&builder, width);
  GtkConstraintExpression *right = gtk_constraint_expression_builder_finish (&builder);

  gtk_constraint_solver_add_stay_variable (solver, width, GTK_CONSTRAINT_STRENGTH_WEAK);
  gtk_constraint_solver_add_stay_variable (solver, right_min, GTK_CONSTRAINT_STRENGTH_WEAK);
  gtk_constraint_solver_add_constraint (solver,
                                        right_min, GTK_CONSTRAINT_RELATION_EQ, right,
                                        GTK_CONSTRAINT_STRENGTH_REQUIRED);

  double x_value = gtk_constraint_variable_get_value (x);
  double width_value = gtk_constraint_variable_get_value (width);

  g_assert_cmpfloat_with_epsilon (x_value, 90.0, 0.001);
  g_assert_cmpfloat_with_epsilon (width_value, 10.0, 0.001);

  gtk_constraint_variable_unref (right_min);
  gtk_constraint_variable_unref (width);
  gtk_constraint_variable_unref (x);

  g_object_unref (solver);
}

static void
constraint_solver_cassowary (void)
{
  GtkConstraintSolver *solver = gtk_constraint_solver_new ();

  GtkConstraintVariable *x = gtk_constraint_solver_create_variable (solver, NULL, "x", 0.0);
  GtkConstraintVariable *y = gtk_constraint_solver_create_variable (solver, NULL, "y", 0.0);

  GtkConstraintExpression *e;

  e = gtk_constraint_expression_new_from_variable (y);
  gtk_constraint_solver_add_constraint (solver,
                                        x, GTK_CONSTRAINT_RELATION_LE, e,
                                        GTK_CONSTRAINT_STRENGTH_REQUIRED);

  e = gtk_constraint_expression_plus_constant (gtk_constraint_expression_new_from_variable (x), 3.0);
  gtk_constraint_solver_add_constraint (solver,
                                        y, GTK_CONSTRAINT_RELATION_EQ, e,
                                        GTK_CONSTRAINT_STRENGTH_REQUIRED);

  e = gtk_constraint_expression_new (10.0);
  gtk_constraint_solver_add_constraint (solver,
                                        x, GTK_CONSTRAINT_RELATION_EQ, e,
                                        GTK_CONSTRAINT_STRENGTH_WEAK);

  e = gtk_constraint_expression_new (10.0);
  gtk_constraint_solver_add_constraint (solver,
                                        y, GTK_CONSTRAINT_RELATION_EQ, e,
                                        GTK_CONSTRAINT_STRENGTH_WEAK);

  double x_val = gtk_constraint_variable_get_value (x);
  double y_val = gtk_constraint_variable_get_value (y);

  g_test_message ("x = %g, y = %g", x_val, y_val);

  /* The system is unstable and has two possible solutions we need to test */
  g_assert_true ((G_APPROX_VALUE (x_val, 10.0, 1e-8) &&
                  G_APPROX_VALUE (y_val, 13.0, 1e-8)) ||
                 (G_APPROX_VALUE (x_val,  7.0, 1e-8) &&
                  G_APPROX_VALUE (y_val, 10.0, 1e-8)));

  gtk_constraint_variable_unref (x);
  gtk_constraint_variable_unref (y);

  g_object_unref (solver);
}

static void
constraint_solver_edit_var_required (void)
{
  GtkConstraintSolver *solver = gtk_constraint_solver_new ();

  GtkConstraintVariable *a = gtk_constraint_solver_create_variable (solver, NULL, "a", 0.0);
  gtk_constraint_solver_add_stay_variable (solver, a, GTK_CONSTRAINT_STRENGTH_STRONG);

  g_assert_cmpfloat_with_epsilon (gtk_constraint_variable_get_value (a), 0.0, 0.001);

  gtk_constraint_solver_add_edit_variable (solver, a, GTK_CONSTRAINT_STRENGTH_REQUIRED);
  gtk_constraint_solver_begin_edit (solver);
  gtk_constraint_solver_suggest_value (solver, a, 2.0);
  gtk_constraint_solver_resolve (solver);

  g_assert_cmpfloat_with_epsilon (gtk_constraint_variable_get_value (a), 2.0, 0.001);

  gtk_constraint_solver_suggest_value (solver, a, 10.0);
  gtk_constraint_solver_resolve (solver);

  g_assert_cmpfloat_with_epsilon (gtk_constraint_variable_get_value (a), 10.0, 0.001);

  gtk_constraint_solver_end_edit (solver);

  gtk_constraint_variable_unref (a);

  g_object_unref (solver);
}

static void
constraint_solver_edit_var_suggest (void)
{
  GtkConstraintSolver *solver = gtk_constraint_solver_new ();

  GtkConstraintVariable *a = gtk_constraint_solver_create_variable (solver, NULL, "a", 0.0);
  GtkConstraintVariable *b = gtk_constraint_solver_create_variable (solver, NULL, "b", 0.0);

  gtk_constraint_solver_add_stay_variable (solver, a, GTK_CONSTRAINT_STRENGTH_STRONG);

  GtkConstraintExpression *e = gtk_constraint_expression_new_from_variable (b);
  gtk_constraint_solver_add_constraint (solver,
                                        a, GTK_CONSTRAINT_RELATION_EQ, e,
                                        GTK_CONSTRAINT_STRENGTH_REQUIRED);

  gtk_constraint_solver_resolve (solver);

  g_assert_cmpfloat_with_epsilon (gtk_constraint_variable_get_value (a), 0.0, 0.001);
  g_assert_cmpfloat_with_epsilon (gtk_constraint_variable_get_value (b), 0.0, 0.001);

  gtk_constraint_solver_add_edit_variable (solver, a, GTK_CONSTRAINT_STRENGTH_REQUIRED);
  gtk_constraint_solver_begin_edit (solver);

  gtk_constraint_solver_suggest_value (solver, a, 2.0);
  gtk_constraint_solver_resolve (solver);

  g_test_message ("Check values after first edit");

  g_assert_cmpfloat_with_epsilon (gtk_constraint_variable_get_value (a), 2.0, 0.001);
  g_assert_cmpfloat_with_epsilon (gtk_constraint_variable_get_value (b), 2.0, 0.001);

  gtk_constraint_solver_suggest_value (solver, a, 10.0);
  gtk_constraint_solver_resolve (solver);

  g_test_message ("Check values after second edit");

  g_assert_cmpfloat_with_epsilon (gtk_constraint_variable_get_value (a), 10.0, 0.001);
  g_assert_cmpfloat_with_epsilon (gtk_constraint_variable_get_value (b), 10.0, 0.001);

  gtk_constraint_solver_suggest_value (solver, a, 12.0);
  gtk_constraint_solver_resolve (solver);

  g_test_message ("Check values after third edit");

  g_assert_cmpfloat_with_epsilon (gtk_constraint_variable_get_value (a), 12.0, 0.001);
  g_assert_cmpfloat_with_epsilon (gtk_constraint_variable_get_value (b), 12.0, 0.001);

  gtk_constraint_variable_unref (a);
  gtk_constraint_variable_unref (b);

  g_object_unref (solver);
}

static void
constraint_solver_paper (void)
{
  GtkConstraintSolver *solver = gtk_constraint_solver_new ();

  GtkConstraintVariable *left = gtk_constraint_solver_create_variable (solver, NULL, "left", 0.0);
  GtkConstraintVariable *middle = gtk_constraint_solver_create_variable (solver, NULL, "middle", 0.0);
  GtkConstraintVariable *right = gtk_constraint_solver_create_variable (solver, NULL, "right", 0.0);

  GtkConstraintExpressionBuilder builder;
  GtkConstraintExpression *expr;

  gtk_constraint_expression_builder_init (&builder, solver);
  gtk_constraint_expression_builder_term (&builder, left);
  gtk_constraint_expression_builder_plus (&builder);
  gtk_constraint_expression_builder_term (&builder, right);
  gtk_constraint_expression_builder_divide_by (&builder);
  gtk_constraint_expression_builder_constant (&builder, 2.0);
  expr = gtk_constraint_expression_builder_finish (&builder);
  gtk_constraint_solver_add_constraint (solver,
                                        middle, GTK_CONSTRAINT_RELATION_EQ, expr,
                                        GTK_CONSTRAINT_STRENGTH_REQUIRED);

  gtk_constraint_expression_builder_init (&builder, solver);
  gtk_constraint_expression_builder_term (&builder, left);
  gtk_constraint_expression_builder_plus (&builder);
  gtk_constraint_expression_builder_constant (&builder, 10.0);
  expr = gtk_constraint_expression_builder_finish (&builder);
  gtk_constraint_solver_add_constraint (solver,
                                        right, GTK_CONSTRAINT_RELATION_EQ, expr,
                                        GTK_CONSTRAINT_STRENGTH_REQUIRED);

  expr = gtk_constraint_expression_new (100.0);
  gtk_constraint_solver_add_constraint (solver,
                                        right, GTK_CONSTRAINT_RELATION_LE, expr,
                                        GTK_CONSTRAINT_STRENGTH_REQUIRED);

  expr = gtk_constraint_expression_new (0.0);
  gtk_constraint_solver_add_constraint (solver,
                                        left, GTK_CONSTRAINT_RELATION_GE, expr,
                                        GTK_CONSTRAINT_STRENGTH_REQUIRED);

  g_test_message ("Check constraints hold");

  g_assert_cmpfloat_with_epsilon (gtk_constraint_variable_get_value (middle),
                                  (gtk_constraint_variable_get_value (left) + gtk_constraint_variable_get_value (right)) / 2.0,
                                  0.001);
  g_assert_cmpfloat_with_epsilon (gtk_constraint_variable_get_value (right),
                                  gtk_constraint_variable_get_value (left) + 10.0,
                                  0.001);
  g_assert_cmpfloat (gtk_constraint_variable_get_value (right), <=, 100.0);
  g_assert_cmpfloat (gtk_constraint_variable_get_value (left), >=, 0.0);

  gtk_constraint_variable_set_value (middle, 45.0);
  gtk_constraint_solver_add_stay_variable (solver, middle, GTK_CONSTRAINT_STRENGTH_WEAK);

  g_test_message ("Check constraints hold after setting middle");

  g_assert_cmpfloat_with_epsilon (gtk_constraint_variable_get_value (middle),
                                  (gtk_constraint_variable_get_value (left) + gtk_constraint_variable_get_value (right)) / 2.0,
                                  0.001);
  g_assert_cmpfloat_with_epsilon (gtk_constraint_variable_get_value (right),
                                  gtk_constraint_variable_get_value (left) + 10.0,
                                  0.001);
  g_assert_cmpfloat (gtk_constraint_variable_get_value (right), <=, 100.0);
  g_assert_cmpfloat (gtk_constraint_variable_get_value (left), >=, 0.0);

  g_assert_cmpfloat_with_epsilon (gtk_constraint_variable_get_value (left), 40.0, 0.001);
  g_assert_cmpfloat_with_epsilon (gtk_constraint_variable_get_value (middle), 45.0, 0.001);
  g_assert_cmpfloat_with_epsilon (gtk_constraint_variable_get_value (right), 50.0, 0.001);

  gtk_constraint_variable_unref (left);
  gtk_constraint_variable_unref (middle);
  gtk_constraint_variable_unref (right);

  g_object_unref (solver);
}

int
main (int argc, char *argv[])
{
  (g_test_init) (&argc, &argv, NULL);
  setlocale (LC_ALL, "C");

  g_test_add_func ("/constraint-solver/paper", constraint_solver_paper);
  g_test_add_func ("/constraint-solver/simple", constraint_solver_simple);
  g_test_add_func ("/constraint-solver/constant/eq", constraint_solver_variable_eq_constant);
  g_test_add_func ("/constraint-solver/constant/ge", constraint_solver_variable_geq_constant);
  g_test_add_func ("/constraint-solver/constant/le", constraint_solver_variable_leq_constant);
  g_test_add_func ("/constraint-solver/stay/simple", constraint_solver_stay);
  g_test_add_func ("/constraint-solver/stay/eq", constraint_solver_eq_with_stay);
  g_test_add_func ("/constraint-solver/cassowary", constraint_solver_cassowary);
  g_test_add_func ("/constraint-solver/edit/required", constraint_solver_edit_var_required);
  g_test_add_func ("/constraint-solver/edit/suggest", constraint_solver_edit_var_suggest);

  return g_test_run ();
}
