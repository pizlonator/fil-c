/* Copyright (C) 2019 Red Hat, Inc.
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
#include <gtk/gtk.h>

#define GTK_TYPE_GIZMO                 (gtk_gizmo_get_type ())
#define GTK_GIZMO(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_GIZMO, GtkGizmo))
#define GTK_GIZMO_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_GIZMO, GtkGizmoClass))
#define GTK_IS_GIZMO(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_GIZMO))
#define GTK_IS_GIZMO_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_GIZMO))
#define GTK_GIZMO_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_GIZMO, GtkGizmoClass))

typedef struct _GtkGizmo GtkGizmo;

struct _GtkGizmo {
  GtkWidget parent;

  const char *name;
  int min_width;
  int min_height;
  int nat_width;
  int nat_height;
  int width;
  int height;
};

typedef GtkWidgetClass GtkGizmoClass;

G_DEFINE_TYPE (GtkGizmo, gtk_gizmo, GTK_TYPE_WIDGET);

static void
gtk_gizmo_measure (GtkWidget      *widget,
                   GtkOrientation  orientation,
                   int             for_size,
                   int            *minimum,
                   int            *natural,
                   int            *minimum_baseline,
                   int            *natural_baseline)
{
  GtkGizmo *self = GTK_GIZMO (widget);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      *minimum = self->min_width;
      *natural = self->nat_width;
    }
  else
    {
      *minimum = self->min_height;
      *natural = self->nat_height;
    }
}

static void
gtk_gizmo_size_allocate (GtkWidget *widget,
                         int        width,
                         int        height,
                         int        baseline)
{
  GtkGizmo *self = GTK_GIZMO (widget);

  self->width = width;
  self->height = height;
}

static void
gtk_gizmo_class_init (GtkGizmoClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->measure = gtk_gizmo_measure;
  widget_class->size_allocate = gtk_gizmo_size_allocate;
}

static void
gtk_gizmo_init (GtkGizmo *self)
{
}

/* Create a grid with three children in row
 *
 * +--------+--------+--------+
 * | child1 | child2 | child3 |
 * +--------+--------+--------+
 *
 * Verify that
 * - the layout has the expected min and nat sizes
 * - the children get their nat width when the layout does
 * - they all get the same height
 */
static void
test_simple_row (void)
{
  GtkWidget *window;
  GtkWidget *parent;
  GtkLayoutManager *layout;
  GtkGizmo *child1;
  GtkGizmo *child2;
  GtkGizmo *child3;
  GtkLayoutChild *lc;
  int minimum, natural;

  window = gtk_window_new ();
  parent = g_object_new (GTK_TYPE_GIZMO, NULL);
  gtk_window_set_child (GTK_WINDOW (window), parent);

  layout = gtk_grid_layout_new ();
  gtk_widget_set_layout_manager (parent, layout);

  child1 = g_object_new (GTK_TYPE_GIZMO, NULL);
  child2 = g_object_new (GTK_TYPE_GIZMO, NULL);
  child3 = g_object_new (GTK_TYPE_GIZMO, NULL);

  child1->name = "child1";
  child1->min_width = 10;
  child1->min_height = 10;
  child1->nat_width = 20;
  child1->nat_height = 20;
  child2->name = "child2";
  child2->min_width = 20;
  child2->min_height = 20;
  child2->nat_width = 30;
  child2->nat_height = 30;
  child3->name = "child3";
  child3->min_width = 30;
  child3->min_height = 30;
  child3->nat_width = 40;
  child3->nat_height = 40;

  gtk_widget_set_parent (GTK_WIDGET (child1), parent);
  gtk_widget_set_parent (GTK_WIDGET (child2), parent);
  gtk_widget_set_parent (GTK_WIDGET (child3), parent);

  lc = gtk_layout_manager_get_layout_child (layout, GTK_WIDGET (child1));
  gtk_grid_layout_child_set_column (GTK_GRID_LAYOUT_CHILD (lc), 0);
  lc = gtk_layout_manager_get_layout_child (layout, GTK_WIDGET (child2));
  gtk_grid_layout_child_set_column (GTK_GRID_LAYOUT_CHILD (lc), 1);
  lc = gtk_layout_manager_get_layout_child (layout, GTK_WIDGET (child3));
  gtk_grid_layout_child_set_column (GTK_GRID_LAYOUT_CHILD (lc), 2);

  gtk_layout_manager_measure (layout,
                              parent,
                              GTK_ORIENTATION_HORIZONTAL,
                              -1,
                              &minimum,
                              &natural,
                              NULL,
                              NULL);

  g_assert_cmpint (minimum, ==, 10 + 20 + 30);
  g_assert_cmpint (natural, ==, 20 + 30 + 40);

  gtk_layout_manager_measure (layout,
                              parent,
                              GTK_ORIENTATION_VERTICAL,
                              -1,
                              &minimum,
                              &natural,
                              NULL,
                              NULL);

  g_assert_cmpint (minimum, ==, 30);
  g_assert_cmpint (natural, ==, 40);

  gtk_layout_manager_allocate (layout, parent, 90, 40, 0);

  g_assert_cmpint (child1->width, ==, 20);
  g_assert_cmpint (child2->width, ==, 30);
  g_assert_cmpint (child3->width, ==, 40);

  g_assert_cmpint (child1->height, ==, 40);
  g_assert_cmpint (child2->height, ==, 40);
  g_assert_cmpint (child3->height, ==, 40);

  gtk_widget_unparent (GTK_WIDGET (child1));
  gtk_widget_unparent (GTK_WIDGET (child2));
  gtk_widget_unparent (GTK_WIDGET (child3));

  gtk_window_destroy (GTK_WINDOW (window));
}

/* same as the previous test, with a column
 */
static void
test_simple_column (void)
{
  GtkWidget *window;
  GtkWidget *parent;
  GtkLayoutManager *layout;
  GtkGizmo *child1;
  GtkGizmo *child2;
  GtkGizmo *child3;
  GtkLayoutChild *lc;
  int minimum, natural;

  window = gtk_window_new ();
  parent = g_object_new (GTK_TYPE_GIZMO, NULL);
  gtk_window_set_child (GTK_WINDOW (window), parent);

  layout = gtk_grid_layout_new ();
  gtk_widget_set_layout_manager (parent, layout);

  child1 = g_object_new (GTK_TYPE_GIZMO, NULL);
  child2 = g_object_new (GTK_TYPE_GIZMO, NULL);
  child3 = g_object_new (GTK_TYPE_GIZMO, NULL);

  child1->name = "child1";
  child1->min_width = 10;
  child1->min_height = 10;
  child1->nat_width = 20;
  child1->nat_height = 20;
  child2->name = "child2";
  child2->min_width = 20;
  child2->min_height = 20;
  child2->nat_width = 30;
  child2->nat_height = 30;
  child3->name = "child3";
  child3->min_width = 30;
  child3->min_height = 30;
  child3->nat_width = 40;
  child3->nat_height = 40;

  gtk_widget_set_parent (GTK_WIDGET (child1), parent);
  gtk_widget_set_parent (GTK_WIDGET (child2), parent);
  gtk_widget_set_parent (GTK_WIDGET (child3), parent);

  lc = gtk_layout_manager_get_layout_child (layout, GTK_WIDGET (child1));
  gtk_grid_layout_child_set_row (GTK_GRID_LAYOUT_CHILD (lc), 0);
  lc = gtk_layout_manager_get_layout_child (layout, GTK_WIDGET (child2));
  gtk_grid_layout_child_set_row (GTK_GRID_LAYOUT_CHILD (lc), 1);
  lc = gtk_layout_manager_get_layout_child (layout, GTK_WIDGET (child3));
  gtk_grid_layout_child_set_row (GTK_GRID_LAYOUT_CHILD (lc), 2);

  gtk_layout_manager_measure (layout,
                              parent,
                              GTK_ORIENTATION_HORIZONTAL,
                              -1,
                              &minimum,
                              &natural,
                              NULL,
                              NULL);

  g_assert_cmpint (minimum, ==, 30);
  g_assert_cmpint (natural, ==, 40);

  gtk_layout_manager_measure (layout,
                              parent,
                              GTK_ORIENTATION_VERTICAL,
                              -1,
                              &minimum,
                              &natural,
                              NULL,
                              NULL);

  g_assert_cmpint (minimum, ==, 10 + 20 + 30);
  g_assert_cmpint (natural, ==, 20 + 30 + 40);

  gtk_layout_manager_allocate (layout, parent, 40, 90, 0);

  g_assert_cmpint (child1->width, ==, 40);
  g_assert_cmpint (child2->width, ==, 40);
  g_assert_cmpint (child3->width, ==, 40);

  g_assert_cmpint (child1->height, ==, 20);
  g_assert_cmpint (child2->height, ==, 30);
  g_assert_cmpint (child3->height, ==, 40);

  gtk_widget_unparent (GTK_WIDGET (child1));
  gtk_widget_unparent (GTK_WIDGET (child2));
  gtk_widget_unparent (GTK_WIDGET (child3));

  gtk_window_destroy (GTK_WINDOW (window));
}

/* Create a grid with spanning children
 *
 * +--------+-----------------+
 * | child1 |      child2     |
 * +--------+--------+--------+
 * |      child3     | child4 |
 * +-----------------+--------+
 *
 * Verify that
 * - the layout has the expected min and nat sizes
 * - the children get their nat width when the layout does
 */
static void
test_spans (void)
{
  GtkWidget *window;
  GtkWidget *parent;
  GtkLayoutManager *layout;
  GtkGizmo *child1;
  GtkGizmo *child2;
  GtkGizmo *child3;
  GtkGizmo *child4;
  GtkLayoutChild *lc;
  int minimum, natural;

  window = gtk_window_new ();
  parent = g_object_new (GTK_TYPE_GIZMO, NULL);
  gtk_window_set_child (GTK_WINDOW (window), parent);

  layout = gtk_grid_layout_new ();
  gtk_widget_set_layout_manager (parent, layout);

  child1 = g_object_new (GTK_TYPE_GIZMO, NULL);
  child2 = g_object_new (GTK_TYPE_GIZMO, NULL);
  child3 = g_object_new (GTK_TYPE_GIZMO, NULL);
  child4 = g_object_new (GTK_TYPE_GIZMO, NULL);

  child1->name = "child1";
  child1->min_width = 10;
  child1->min_height = 10;
  child1->nat_width = 20;
  child1->nat_height = 20;
  child2->name = "child2";
  child2->min_width = 20;
  child2->min_height = 20;
  child2->nat_width = 30;
  child2->nat_height = 30;
  child3->name = "child3";
  child3->min_width = 30;
  child3->min_height = 30;
  child3->nat_width = 40;
  child3->nat_height = 40;
  child4->name = "child4";
  child4->min_width = 30;
  child4->min_height = 30;
  child4->nat_width = 40;
  child4->nat_height = 40;

  gtk_widget_set_parent (GTK_WIDGET (child1), parent);
  gtk_widget_set_parent (GTK_WIDGET (child2), parent);
  gtk_widget_set_parent (GTK_WIDGET (child3), parent);
  gtk_widget_set_parent (GTK_WIDGET (child4), parent);

  lc = gtk_layout_manager_get_layout_child (layout, GTK_WIDGET (child1));
  gtk_grid_layout_child_set_row (GTK_GRID_LAYOUT_CHILD (lc), 0);
  gtk_grid_layout_child_set_column (GTK_GRID_LAYOUT_CHILD (lc), 0);

  lc = gtk_layout_manager_get_layout_child (layout, GTK_WIDGET (child2));
  gtk_grid_layout_child_set_row (GTK_GRID_LAYOUT_CHILD (lc), 0);
  gtk_grid_layout_child_set_column (GTK_GRID_LAYOUT_CHILD (lc), 1);
  gtk_grid_layout_child_set_column_span (GTK_GRID_LAYOUT_CHILD (lc), 2);

  lc = gtk_layout_manager_get_layout_child (layout, GTK_WIDGET (child3));
  gtk_grid_layout_child_set_row (GTK_GRID_LAYOUT_CHILD (lc), 1);
  gtk_grid_layout_child_set_column (GTK_GRID_LAYOUT_CHILD (lc), 0);
  gtk_grid_layout_child_set_column_span (GTK_GRID_LAYOUT_CHILD (lc), 2);

  lc = gtk_layout_manager_get_layout_child (layout, GTK_WIDGET (child4));
  gtk_grid_layout_child_set_row (GTK_GRID_LAYOUT_CHILD (lc), 1);
  gtk_grid_layout_child_set_column (GTK_GRID_LAYOUT_CHILD (lc), 2);

  gtk_layout_manager_measure (layout,
                              parent,
                              GTK_ORIENTATION_HORIZONTAL,
                              -1,
                              &minimum,
                              &natural,
                              NULL,
                              NULL);

  g_assert_cmpint (minimum, ==, 60);
  g_assert_cmpint (natural, ==, 80);

  gtk_layout_manager_measure (layout,
                              parent,
                              GTK_ORIENTATION_VERTICAL,
                              -1,
                              &minimum,
                              &natural,
                              NULL,
                              NULL);

  g_assert_cmpint (minimum, ==, 50);
  g_assert_cmpint (natural, ==, 70);

  gtk_layout_manager_allocate (layout, parent, 80, 70, 0);

  g_assert_cmpint (child1->width, ==, 30);
  g_assert_cmpint (child2->width, ==, 50);
  g_assert_cmpint (child3->width, ==, 40);
  g_assert_cmpint (child4->width, ==, 40);

  g_assert_cmpint (child1->height, ==, 30);
  g_assert_cmpint (child2->height, ==, 30);
  g_assert_cmpint (child3->height, ==, 40);
  g_assert_cmpint (child4->height, ==, 40);

  gtk_widget_unparent (GTK_WIDGET (child1));
  gtk_widget_unparent (GTK_WIDGET (child2));
  gtk_widget_unparent (GTK_WIDGET (child3));
  gtk_widget_unparent (GTK_WIDGET (child4));

  gtk_window_destroy (GTK_WINDOW (window));
}

/* Create a 2x2 homogeneous grid and verify
 * all children get the same size.
 */
static void
test_homogeneous (void)
{
  GtkWidget *window;
  GtkWidget *parent;
  GtkLayoutManager *layout;
  GtkGizmo *child1;
  GtkGizmo *child2;
  GtkGizmo *child3;
  GtkGizmo *child4;
  GtkLayoutChild *lc;
  int minimum, natural;

  window = gtk_window_new ();
  parent = g_object_new (GTK_TYPE_GIZMO, NULL);
  gtk_window_set_child (GTK_WINDOW (window), parent);

  layout = gtk_grid_layout_new ();
  gtk_grid_layout_set_row_homogeneous (GTK_GRID_LAYOUT (layout), TRUE);
  gtk_grid_layout_set_column_homogeneous (GTK_GRID_LAYOUT (layout), TRUE);
  gtk_widget_set_layout_manager (parent, layout);

  child1 = g_object_new (GTK_TYPE_GIZMO, NULL);
  child2 = g_object_new (GTK_TYPE_GIZMO, NULL);
  child3 = g_object_new (GTK_TYPE_GIZMO, NULL);
  child4 = g_object_new (GTK_TYPE_GIZMO, NULL);

  child1->name = "child1";
  child1->min_width = 10;
  child1->min_height = 10;
  child1->nat_width = 20;
  child1->nat_height = 20;
  child2->name = "child2";
  child2->min_width = 20;
  child2->min_height = 20;
  child2->nat_width = 30;
  child2->nat_height = 30;
  child3->name = "child3";
  child3->min_width = 30;
  child3->min_height = 30;
  child3->nat_width = 40;
  child3->nat_height = 40;
  child4->name = "child4";
  child4->min_width = 30;
  child4->min_height = 30;
  child4->nat_width = 40;
  child4->nat_height = 40;

  gtk_widget_set_parent (GTK_WIDGET (child1), parent);
  gtk_widget_set_parent (GTK_WIDGET (child2), parent);
  gtk_widget_set_parent (GTK_WIDGET (child3), parent);
  gtk_widget_set_parent (GTK_WIDGET (child4), parent);

  lc = gtk_layout_manager_get_layout_child (layout, GTK_WIDGET (child1));
  gtk_grid_layout_child_set_row (GTK_GRID_LAYOUT_CHILD (lc), 0);
  gtk_grid_layout_child_set_column (GTK_GRID_LAYOUT_CHILD (lc), 0);

  lc = gtk_layout_manager_get_layout_child (layout, GTK_WIDGET (child2));
  gtk_grid_layout_child_set_row (GTK_GRID_LAYOUT_CHILD (lc), 0);
  gtk_grid_layout_child_set_column (GTK_GRID_LAYOUT_CHILD (lc), 1);

  lc = gtk_layout_manager_get_layout_child (layout, GTK_WIDGET (child3));
  gtk_grid_layout_child_set_row (GTK_GRID_LAYOUT_CHILD (lc), 1);
  gtk_grid_layout_child_set_column (GTK_GRID_LAYOUT_CHILD (lc), 0);

  lc = gtk_layout_manager_get_layout_child (layout, GTK_WIDGET (child4));
  gtk_grid_layout_child_set_row (GTK_GRID_LAYOUT_CHILD (lc), 1);
  gtk_grid_layout_child_set_column (GTK_GRID_LAYOUT_CHILD (lc), 1);

  gtk_layout_manager_measure (layout,
                              parent,
                              GTK_ORIENTATION_HORIZONTAL,
                              -1,
                              &minimum,
                              &natural,
                              NULL,
                              NULL);

  g_assert_cmpint (minimum, ==, 60);
  g_assert_cmpint (natural, ==, 80);

  gtk_layout_manager_measure (layout,
                              parent,
                              GTK_ORIENTATION_VERTICAL,
                              -1,
                              &minimum,
                              &natural,
                              NULL,
                              NULL);

  g_assert_cmpint (minimum, ==, 60);
  g_assert_cmpint (natural, ==, 80);

  gtk_layout_manager_allocate (layout, parent, 80, 80, 0);

  g_assert_cmpint (child1->width, ==, 40);
  g_assert_cmpint (child2->width, ==, 40);
  g_assert_cmpint (child3->width, ==, 40);
  g_assert_cmpint (child4->width, ==, 40);

  g_assert_cmpint (child1->height, ==, 40);
  g_assert_cmpint (child2->height, ==, 40);
  g_assert_cmpint (child3->height, ==, 40);
  g_assert_cmpint (child4->height, ==, 40);

  gtk_widget_unparent (GTK_WIDGET (child1));
  gtk_widget_unparent (GTK_WIDGET (child2));
  gtk_widget_unparent (GTK_WIDGET (child3));
  gtk_widget_unparent (GTK_WIDGET (child4));

  gtk_window_destroy (GTK_WINDOW (window));
}

/* Create a layout with three children
 *
 * +--------+--------+
 * | child1 | child2 |
 * +--------+--------+
 * |      child3     |
 * +-----------------+
 *
 * This is a layout that we also reproduce with
 * constraints, for comparison. Among the constraints:
 * - child1.width == child2.width
 * - child1.height == child2.height == child3.height
 */
static void
test_simple_layout (void)
{
  GtkWidget *window;
  GtkWidget *parent;
  GtkLayoutManager *layout;
  GtkLayoutChild *lc;
  GtkGizmo *child1;
  GtkGizmo *child2;
  GtkGizmo *child3;
  int minimum, natural;

  window = gtk_window_new ();
  parent = g_object_new (GTK_TYPE_GIZMO, NULL);
  gtk_window_set_child (GTK_WINDOW (window), parent);

  layout = gtk_grid_layout_new ();
  gtk_grid_layout_set_row_homogeneous (GTK_GRID_LAYOUT (layout), TRUE);
  gtk_grid_layout_set_column_homogeneous (GTK_GRID_LAYOUT (layout), TRUE);
  gtk_widget_set_layout_manager (parent, layout);

  child1 = g_object_new (GTK_TYPE_GIZMO, NULL);
  child2 = g_object_new (GTK_TYPE_GIZMO, NULL);
  child3 = g_object_new (GTK_TYPE_GIZMO, NULL);

  child1->name = "child1";
  child1->min_width = 10;
  child1->min_height = 10;
  child1->nat_width = 50;
  child1->nat_height = 50;
  child2->name = "child2";
  child2->min_width = 20;
  child2->min_height = 20;
  child2->nat_width = 50;
  child2->nat_height = 50;
  child3->name = "child3";
  child3->min_width = 50;
  child3->min_height = 10;
  child3->nat_width = 50;
  child3->nat_height = 50;

  gtk_widget_set_parent (GTK_WIDGET (child1), parent);
  gtk_widget_set_parent (GTK_WIDGET (child2), parent);
  gtk_widget_set_parent (GTK_WIDGET (child3), parent);

  lc = gtk_layout_manager_get_layout_child (layout, GTK_WIDGET (child1));
  gtk_grid_layout_child_set_row (GTK_GRID_LAYOUT_CHILD (lc), 0);
  gtk_grid_layout_child_set_column (GTK_GRID_LAYOUT_CHILD (lc), 0);

  lc = gtk_layout_manager_get_layout_child (layout, GTK_WIDGET (child2));
  gtk_grid_layout_child_set_row (GTK_GRID_LAYOUT_CHILD (lc), 0);
  gtk_grid_layout_child_set_column (GTK_GRID_LAYOUT_CHILD (lc), 1);

  lc = gtk_layout_manager_get_layout_child (layout, GTK_WIDGET (child3));
  gtk_grid_layout_child_set_row (GTK_GRID_LAYOUT_CHILD (lc), 1);
  gtk_grid_layout_child_set_column (GTK_GRID_LAYOUT_CHILD (lc), 0);
  gtk_grid_layout_child_set_column_span (GTK_GRID_LAYOUT_CHILD (lc), 2);

  gtk_layout_manager_measure (layout,
                              parent,
                              GTK_ORIENTATION_HORIZONTAL,
                              -1,
                              &minimum,
                              &natural,
                              NULL,
                              NULL);

  g_assert_cmpint (minimum, ==, 50);
  g_assert_cmpint (natural, ==, 100);

  gtk_layout_manager_measure (layout,
                              parent,
                              GTK_ORIENTATION_VERTICAL,
                              -1,
                              &minimum,
                              &natural,
                              NULL,
                              NULL);

  g_assert_cmpint (minimum, ==, 40);
  g_assert_cmpint (natural, ==, 100);

  gtk_layout_manager_allocate (layout, parent, 100, 100, 0);

  g_assert_cmpint (child1->width, ==, 50);
  g_assert_cmpint (child2->width, ==, 50);
  g_assert_cmpint (child3->width, ==, 100);

  g_assert_cmpint (child1->height, ==, 50);
  g_assert_cmpint (child2->height, ==, 50);
  g_assert_cmpint (child3->height, ==, 50);

  gtk_widget_unparent (GTK_WIDGET (child1));
  gtk_widget_unparent (GTK_WIDGET (child2));
  gtk_widget_unparent (GTK_WIDGET (child3));

  gtk_window_destroy (GTK_WINDOW (window));
}

int
main (int   argc,
      char *argv[])
{
  gtk_test_init (&argc, &argv);

  g_test_add_func ("/grid-layout/row", test_simple_row);
  g_test_add_func ("/grid-layout/column", test_simple_column);
  g_test_add_func ("/grid-layout/span", test_spans);
  g_test_add_func ("/grid-layout/homogeneous", test_homogeneous);
  g_test_add_func ("/grid-layout/simple", test_simple_layout);

  return g_test_run();
}
