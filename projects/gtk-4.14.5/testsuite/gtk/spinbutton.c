#include <gtk/gtk.h>

static int value_changed_count;

static void
value_changed_cb (GtkSpinButton *spin)
{
  value_changed_count++;
}

static void
test_value_changed (void)
{
  GtkWidget *spin;
  GtkAdjustment *adj;

  spin = gtk_spin_button_new_with_range (0.0, 10.0, 1.0);

  g_signal_connect (spin, "value-changed", G_CALLBACK (value_changed_cb), NULL);

  value_changed_count = 0;
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin), 1.0);
  g_assert_cmpint (value_changed_count, ==, 1);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin), 2.0);
  g_assert_cmpint (value_changed_count, ==, 2);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin), 2.0);
  g_assert_cmpint (value_changed_count, ==, 2);

  gtk_spin_button_spin (GTK_SPIN_BUTTON (spin), GTK_SPIN_STEP_FORWARD, 0.5);
  g_assert_cmpint (value_changed_count, ==, 3);

  gtk_spin_button_configure (GTK_SPIN_BUTTON (spin), NULL, 1.0, 0);
  g_assert_cmpint (value_changed_count, ==, 4);

  adj = gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (spin));
  gtk_adjustment_set_value (adj, 0.0);
  g_assert_cmpint (value_changed_count, ==, 5);
}

static int adjustment_changed_count;

static void
adjustment_changed_cb (GObject *object, GParamSpec *pspec)
{
  adjustment_changed_count++;
}

static void
test_adjustment_changed (void)
{
  GtkWidget *spin;
  GtkAdjustment *adj;

  spin = gtk_spin_button_new_with_range (0.0, 10.0, 1.0);

  g_signal_connect (spin, "notify::adjustment", G_CALLBACK (adjustment_changed_cb), NULL);

  adjustment_changed_count = 0;
  adj = gtk_adjustment_new (50.0, 0.0, 100.0, 1.0, 1.0, 0.0);
  gtk_spin_button_configure (GTK_SPIN_BUTTON (spin), adj, 1.0, 0);
  g_assert_cmpint (adjustment_changed_count, ==, 1);

  adj = gtk_adjustment_new (51.0, 1.0, 101.0, 1.0, 1.0, 0.0);
  gtk_spin_button_set_adjustment (GTK_SPIN_BUTTON (spin), adj);
  g_assert_cmpint (adjustment_changed_count, ==, 2);

  gtk_spin_button_set_range (GTK_SPIN_BUTTON (spin), 2.0, 102.0);
  g_assert_cmpint (adjustment_changed_count, ==, 2);
}

static void
test_adjustment_null (void)
{
  GtkWidget *spin;
  GtkAdjustment *adj;

  spin = gtk_spin_button_new_with_range (0.0, 10.0, 1.0);

  adj = gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (spin));
  gtk_spin_button_configure (GTK_SPIN_BUTTON (spin), NULL, 1.0, 0);
  g_assert_true (adj == gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (spin)));

  gtk_spin_button_set_adjustment (GTK_SPIN_BUTTON (spin), NULL);
  adj = gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (spin));
  g_assert_cmpfloat (gtk_adjustment_get_lower (adj), ==, 0.0);
  g_assert_cmpfloat (gtk_adjustment_get_value (adj), ==, 0.0);
  g_assert_cmpfloat (gtk_adjustment_get_upper (adj), ==, 0.0);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv);

  g_test_add_func ("/spinbutton/value-changed", test_value_changed);
  g_test_add_func ("/spinbutton/adjustment-changed", test_adjustment_changed);
  g_test_add_func ("/spinbutton/adjustment-null", test_adjustment_null);

  return g_test_run ();
}
