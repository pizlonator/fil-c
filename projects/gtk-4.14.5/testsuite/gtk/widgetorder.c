#include <gtk/gtk.h>

static void
simple (void)
{
  GtkWidget *box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  GtkWidget *l = gtk_label_new ("");

  gtk_box_append (GTK_BOX (box), l);

  g_assert_true (gtk_widget_get_parent (l) == box);
  g_assert_null (gtk_widget_get_prev_sibling (l));
  g_assert_null (gtk_widget_get_next_sibling (l));
  g_assert_null (gtk_widget_get_first_child (l));
  g_assert_null (gtk_widget_get_last_child (l));

  g_assert_true (gtk_widget_get_first_child (box) == l);
  g_assert_true (gtk_widget_get_last_child (box) == l);
}

static void
two (void)
{
  GtkWidget *box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  GtkWidget *l1 = gtk_label_new ("");
  GtkWidget *l2 = gtk_label_new ("");

  gtk_box_append (GTK_BOX (box), l1);
  gtk_box_append (GTK_BOX (box), l2);

  g_assert_true (gtk_widget_get_parent (l1) == box);
  g_assert_null (gtk_widget_get_prev_sibling (l1));
  g_assert_true (gtk_widget_get_next_sibling (l1) == l2);

  g_assert_true (gtk_widget_get_parent (l2) == box);
  g_assert_true (gtk_widget_get_prev_sibling (l2) == l1);
  g_assert_null (gtk_widget_get_next_sibling (l2));

  g_assert_true (gtk_widget_get_first_child (box) == l1);
  g_assert_true (gtk_widget_get_last_child (box) == l2);
}

static void
prepend (void)
{
  GtkWidget *p = gtk_label_new ("");
  GtkWidget *l1 = gtk_label_new ("");
  GtkWidget *l2 = gtk_label_new ("");

  gtk_widget_set_parent (l1, p);
  gtk_widget_insert_after (l2, p, NULL);

  /* l2 should now be *before* l1 */

  g_assert_true (gtk_widget_get_parent (l1) == p);
  g_assert_true (gtk_widget_get_prev_sibling (l1) == l2);
  g_assert_null (gtk_widget_get_next_sibling (l1));

  g_assert_true (gtk_widget_get_parent (l2) == p);
  g_assert_null (gtk_widget_get_prev_sibling (l2));
  g_assert_true (gtk_widget_get_next_sibling (l2) == l1);

  g_assert_true (gtk_widget_get_first_child (p) == l2);
  g_assert_true (gtk_widget_get_last_child (p) == l1);
}

static void
append (void)
{
  GtkWidget *p = gtk_label_new ("");
  GtkWidget *l1 = gtk_label_new ("");
  GtkWidget *l2 = gtk_label_new ("");

  gtk_widget_set_parent (l1, p);
  gtk_widget_insert_before (l2, p, NULL);

  g_assert_true (gtk_widget_get_parent (l1) == p);
  g_assert_null (gtk_widget_get_prev_sibling (l1));
  g_assert_true (gtk_widget_get_next_sibling (l1) == l2);

  g_assert_true (gtk_widget_get_parent (l2) == p);
  g_assert_true (gtk_widget_get_prev_sibling (l2) == l1);
  g_assert_null (gtk_widget_get_next_sibling (l2));

  g_assert_true (gtk_widget_get_first_child (p) == l1);
  g_assert_true (gtk_widget_get_last_child (p) == l2);
}

static void
insert_after (void)
{
  GtkWidget *p = gtk_label_new ("");
  GtkWidget *l1 = gtk_label_new ("");
  GtkWidget *l2 = gtk_label_new ("");
  GtkWidget *l3 = gtk_label_new ("");

  gtk_widget_set_parent (l1, p);
  gtk_widget_set_parent (l3, p);

  g_assert_true (gtk_widget_get_parent (l1) == p);
  g_assert_null (gtk_widget_get_prev_sibling (l1));
  g_assert_true (gtk_widget_get_next_sibling (l1) == l3);

  g_assert_true (gtk_widget_get_parent (l3) == p);
  g_assert_true (gtk_widget_get_prev_sibling (l3) == l1);
  g_assert_null (gtk_widget_get_next_sibling (l3));

  g_assert_true (gtk_widget_get_first_child (p) == l1);
  g_assert_true (gtk_widget_get_last_child (p) == l3);

  /* Now add l2 between l1 and l3 */
  gtk_widget_insert_after (l2, p, l1);

  g_assert_true (gtk_widget_get_next_sibling (l1) == l2);

  g_assert_true (gtk_widget_get_parent (l2) == p);
  g_assert_true (gtk_widget_get_prev_sibling (l2) == l1);
  g_assert_true (gtk_widget_get_next_sibling (l2) == l3);

  g_assert_true (gtk_widget_get_prev_sibling (l3) == l2);

  g_assert_true (gtk_widget_get_first_child (p) == l1);
  g_assert_true (gtk_widget_get_last_child (p) == l3);
}

static void
insert_before (void)
{
  GtkWidget *p = gtk_label_new ("");
  GtkWidget *l1 = gtk_label_new ("");
  GtkWidget *l2 = gtk_label_new ("");
  GtkWidget *l3 = gtk_label_new ("");

  gtk_widget_set_parent (l1, p);
  gtk_widget_set_parent (l3, p);

  g_assert_true (gtk_widget_get_parent (l1) == p);
  g_assert_null (gtk_widget_get_prev_sibling (l1));
  g_assert_true (gtk_widget_get_next_sibling (l1) == l3);

  g_assert_true (gtk_widget_get_parent (l3) == p);
  g_assert_true (gtk_widget_get_prev_sibling (l3) == l1);
  g_assert_null (gtk_widget_get_next_sibling (l3));

  g_assert_true (gtk_widget_get_first_child (p) == l1);
  g_assert_true (gtk_widget_get_last_child (p) == l3);

  /* Now add l2 between l1 and l3 */
  gtk_widget_insert_before (l2, p, l3);

  g_assert_true (gtk_widget_get_next_sibling (l1) == l2);

  g_assert_true (gtk_widget_get_parent (l2) == p);
  g_assert_true (gtk_widget_get_prev_sibling (l2) == l1);
  g_assert_true (gtk_widget_get_next_sibling (l2) == l3);

  g_assert_true (gtk_widget_get_prev_sibling (l3) == l2);

  g_assert_true (gtk_widget_get_first_child (p) == l1);
  g_assert_true (gtk_widget_get_last_child (p) == l3);
}

static void
insert_after_self (void)
{
  GtkWidget *p = gtk_label_new ("");
  GtkWidget *l = gtk_label_new ("");

  gtk_widget_insert_after (l, p, NULL);

  g_assert_true (gtk_widget_get_parent (l) == p);
  g_assert_null (gtk_widget_get_prev_sibling (l));
  g_assert_null (gtk_widget_get_next_sibling (l));
  g_assert_null (gtk_widget_get_first_child (l));
  g_assert_null (gtk_widget_get_last_child (l));

  g_assert_true (gtk_widget_get_first_child (p) == l);
  g_assert_true (gtk_widget_get_last_child (p) == l);

  /* Insert l after l */
  gtk_widget_insert_after (l, p, l);

  g_assert_true (gtk_widget_get_parent (l) == p);
  g_assert_null (gtk_widget_get_prev_sibling (l));
  g_assert_null (gtk_widget_get_next_sibling (l));
  g_assert_null (gtk_widget_get_first_child (l));
  g_assert_null (gtk_widget_get_last_child (l));

  g_assert_true (gtk_widget_get_first_child (p) == l);
  g_assert_true (gtk_widget_get_last_child (p) == l);
}

static void
insert_before_self (void)
{
  GtkWidget *p = gtk_label_new ("");
  GtkWidget *l = gtk_label_new ("");

  gtk_widget_insert_before (l, p, NULL);

  g_assert_true (gtk_widget_get_parent (l) == p);
  g_assert_null (gtk_widget_get_prev_sibling (l));
  g_assert_null (gtk_widget_get_next_sibling (l));
  g_assert_null (gtk_widget_get_first_child (l));
  g_assert_null (gtk_widget_get_last_child (l));

  g_assert_true (gtk_widget_get_first_child (p) == l);
  g_assert_true (gtk_widget_get_last_child (p) == l);

  /* Insert l before l */
  gtk_widget_insert_before (l, p, l);

  g_assert_true (gtk_widget_get_parent (l) == p);
  g_assert_null (gtk_widget_get_prev_sibling (l));
  g_assert_null (gtk_widget_get_next_sibling (l));
  g_assert_null (gtk_widget_get_first_child (l));
  g_assert_null (gtk_widget_get_last_child (l));

  g_assert_true (gtk_widget_get_first_child (p) == l);
  g_assert_true (gtk_widget_get_last_child (p) == l);
}

static void
reorder_after (void)
{
  GtkWidget *p = gtk_label_new ("");
  GtkWidget *l1 = gtk_label_new ("");
  GtkWidget *l2 = gtk_label_new ("");
  GtkWidget *l3 = gtk_label_new ("");

  gtk_widget_set_parent (l1, p);
  gtk_widget_set_parent (l3, p);

  g_assert_true (gtk_widget_get_parent (l1) == p);
  g_assert_null (gtk_widget_get_prev_sibling (l1));
  g_assert_true (gtk_widget_get_next_sibling (l1) == l3);

  g_assert_true (gtk_widget_get_parent (l3) == p);
  g_assert_true (gtk_widget_get_prev_sibling (l3) == l1);
  g_assert_null (gtk_widget_get_next_sibling (l3));

  g_assert_true (gtk_widget_get_first_child (p) == l1);
  g_assert_true (gtk_widget_get_last_child (p) == l3);

  /* Now add l2 between l1 and l3 */
  gtk_widget_insert_before (l2, p, l3);

  g_assert_true (gtk_widget_get_next_sibling (l1) == l2);

  g_assert_true (gtk_widget_get_parent (l2) == p);
  g_assert_true (gtk_widget_get_prev_sibling (l2) == l1);
  g_assert_true (gtk_widget_get_next_sibling (l2) == l3);

  g_assert_true (gtk_widget_get_prev_sibling (l3) == l2);

  g_assert_true (gtk_widget_get_first_child (p) == l1);
  g_assert_true (gtk_widget_get_last_child (p) == l3);

  /* The order is now l1, l2, l3. Now reorder l3 after l1 so
   * the correct order is l1, l3, l2 */

  gtk_widget_insert_after (l3, p, l1);

  g_assert_null (gtk_widget_get_prev_sibling (l1));
  g_assert_true (gtk_widget_get_next_sibling (l1) == l3);

  g_assert_true (gtk_widget_get_prev_sibling (l3) == l1);
  g_assert_true (gtk_widget_get_next_sibling (l3) == l2);

  g_assert_true (gtk_widget_get_prev_sibling (l2) == l3);
  g_assert_null (gtk_widget_get_next_sibling (l2));

  g_assert_true (gtk_widget_get_first_child (p) == l1);
  g_assert_true (gtk_widget_get_last_child (p) == l2);
}

static void
reorder_before (void)
{
  GtkWidget *p = gtk_label_new ("");
  GtkWidget *l1 = gtk_label_new ("");
  GtkWidget *l2 = gtk_label_new ("");
  GtkWidget *l3 = gtk_label_new ("");

  gtk_widget_set_parent (l1, p);
  gtk_widget_set_parent (l3, p);

  g_assert_true (gtk_widget_get_parent (l1) == p);
  g_assert_null (gtk_widget_get_prev_sibling (l1));
  g_assert_true (gtk_widget_get_next_sibling (l1) == l3);

  g_assert_true (gtk_widget_get_parent (l3) == p);
  g_assert_true (gtk_widget_get_prev_sibling (l3) == l1);
  g_assert_null (gtk_widget_get_next_sibling (l3));

  g_assert_true (gtk_widget_get_first_child (p) == l1);
  g_assert_true (gtk_widget_get_last_child (p) == l3);

  /* Now add l2 between l1 and l3 */
  gtk_widget_insert_before (l2, p, l3);

  g_assert_true (gtk_widget_get_next_sibling (l1) == l2);

  g_assert_true (gtk_widget_get_parent (l2) == p);
  g_assert_true (gtk_widget_get_prev_sibling (l2) == l1);
  g_assert_true (gtk_widget_get_next_sibling (l2) == l3);

  g_assert_true (gtk_widget_get_prev_sibling (l3) == l2);

  g_assert_true (gtk_widget_get_first_child (p) == l1);
  g_assert_true (gtk_widget_get_last_child (p) == l3);

  /* The order is now l1, l2, l3. Now reorder l3 before l2 so
   * the correct order is l1, l3, l2 */

  gtk_widget_insert_before (l3, p, l2);

  g_assert_null (gtk_widget_get_prev_sibling (l1));
  g_assert_true (gtk_widget_get_next_sibling (l1) == l3);

  g_assert_true (gtk_widget_get_prev_sibling (l3) == l1);
  g_assert_true (gtk_widget_get_next_sibling (l3) == l2);

  g_assert_true (gtk_widget_get_prev_sibling (l2) == l3);
  g_assert_null (gtk_widget_get_next_sibling (l2));

  g_assert_true (gtk_widget_get_first_child (p) == l1);
  g_assert_true (gtk_widget_get_last_child (p) == l2);
}

static void
reorder_start (void)
{
  GtkWidget *p = gtk_label_new ("");
  GtkWidget *l1 = gtk_label_new ("");
  GtkWidget *l2 = gtk_label_new ("");
  GtkWidget *l3 = gtk_label_new ("");

  gtk_widget_set_parent (l1, p);
  gtk_widget_set_parent (l3, p);

  g_assert_true (gtk_widget_get_parent (l1) == p);
  g_assert_null (gtk_widget_get_prev_sibling (l1));
  g_assert_true (gtk_widget_get_next_sibling (l1) == l3);

  g_assert_true (gtk_widget_get_parent (l3) == p);
  g_assert_true (gtk_widget_get_prev_sibling (l3) == l1);
  g_assert_null (gtk_widget_get_next_sibling (l3));

  g_assert_true (gtk_widget_get_first_child (p) == l1);
  g_assert_true (gtk_widget_get_last_child (p) == l3);

  /* Now add l2 between l1 and l3 */
  gtk_widget_insert_before (l2, p, l3);

  g_assert_true (gtk_widget_get_next_sibling (l1) == l2);

  g_assert_true (gtk_widget_get_parent (l2) == p);
  g_assert_true (gtk_widget_get_prev_sibling (l2) == l1);
  g_assert_true (gtk_widget_get_next_sibling (l2) == l3);

  g_assert_true (gtk_widget_get_prev_sibling (l3) == l2);

  g_assert_true (gtk_widget_get_first_child (p) == l1);
  g_assert_true (gtk_widget_get_last_child (p) == l3);

  /* The order is now l1, l2, l3. Now reorder l3 to the start so
   * the correct order is l3, l1, l2 */

  gtk_widget_insert_after (l3, p, NULL);

  g_assert_true (gtk_widget_get_prev_sibling (l1) == l3);
  g_assert_true (gtk_widget_get_next_sibling (l1) == l2);

  g_assert_null (gtk_widget_get_prev_sibling (l3));
  g_assert_true (gtk_widget_get_next_sibling (l3) == l1);

  g_assert_true (gtk_widget_get_prev_sibling (l2) == l1);
  g_assert_null (gtk_widget_get_next_sibling (l2));

  g_assert_true (gtk_widget_get_first_child (p) == l3);
  g_assert_true (gtk_widget_get_last_child (p) == l2);
}

static void
reorder_end (void)
{
  GtkWidget *p = gtk_label_new ("");
  GtkWidget *l1 = gtk_label_new ("");
  GtkWidget *l2 = gtk_label_new ("");
  GtkWidget *l3 = gtk_label_new ("");

  gtk_widget_set_parent (l1, p);
  gtk_widget_set_parent (l3, p);

  g_assert_true (gtk_widget_get_parent (l1) == p);
  g_assert_null (gtk_widget_get_prev_sibling (l1));
  g_assert_true (gtk_widget_get_next_sibling (l1) == l3);

  g_assert_true (gtk_widget_get_parent (l3) == p);
  g_assert_true (gtk_widget_get_prev_sibling (l3) == l1);
  g_assert_null (gtk_widget_get_next_sibling (l3));

  g_assert_true (gtk_widget_get_first_child (p) == l1);
  g_assert_true (gtk_widget_get_last_child (p) == l3);

  /* Now add l2 between l1 and l3 */
  gtk_widget_insert_before (l2, p, l3);

  g_assert_true (gtk_widget_get_next_sibling (l1) == l2);

  g_assert_true (gtk_widget_get_parent (l2) == p);
  g_assert_true (gtk_widget_get_prev_sibling (l2) == l1);
  g_assert_true (gtk_widget_get_next_sibling (l2) == l3);

  g_assert_true (gtk_widget_get_prev_sibling (l3) == l2);

  g_assert_true (gtk_widget_get_first_child (p) == l1);
  g_assert_true (gtk_widget_get_last_child (p) == l3);

  /* The order is now l1, l2, l3. Now reorder l1 to the end so
   * the correct order is l2, l3, l1 */

  gtk_widget_insert_before (l1, p, NULL);

  g_assert_true (gtk_widget_get_prev_sibling (l1) == l3);
  g_assert_null (gtk_widget_get_next_sibling (l1));

  g_assert_true (gtk_widget_get_prev_sibling (l3) == l2);
  g_assert_true (gtk_widget_get_next_sibling (l3) == l1);

  g_assert_null (gtk_widget_get_prev_sibling (l2));
  g_assert_true (gtk_widget_get_next_sibling (l2) == l3);

  g_assert_true (gtk_widget_get_first_child (p) == l2);
  g_assert_true (gtk_widget_get_last_child (p) == l1);
}

static void
same_after (void)
{
  GtkWidget *p = gtk_label_new ("");
  GtkWidget *l1 = gtk_label_new ("");
  GtkWidget *l2 = gtk_label_new ("");

  gtk_widget_set_parent (l1, p);
  gtk_widget_set_parent (l2, p);

  g_assert_true (gtk_widget_get_parent (l1) == p);
  g_assert_null (gtk_widget_get_prev_sibling (l1));
  g_assert_true (gtk_widget_get_next_sibling (l1) == l2);

  g_assert_true (gtk_widget_get_parent (l2) == p);
  g_assert_true (gtk_widget_get_prev_sibling (l2) == l1);
  g_assert_null (gtk_widget_get_next_sibling (l2));

  g_assert_true (gtk_widget_get_first_child (p) == l1);
  g_assert_true (gtk_widget_get_last_child (p) == l2);

  /* l2 is already after l1, so this shouldn't change anything! */
  gtk_widget_insert_after (l2, p, l1);

  g_assert_true (gtk_widget_get_prev_sibling (l2) == l1);
  g_assert_null (gtk_widget_get_next_sibling (l2));

  g_assert_null (gtk_widget_get_prev_sibling (l1));
  g_assert_true (gtk_widget_get_next_sibling (l1) == l2);

  g_assert_true (gtk_widget_get_first_child (p) == l1);
  g_assert_true (gtk_widget_get_last_child (p) == l2);
}

static void
same_before (void)
{
  GtkWidget *p = gtk_label_new ("");
  GtkWidget *l1 = gtk_label_new ("");
  GtkWidget *l2 = gtk_label_new ("");

  gtk_widget_set_parent (l1, p);
  gtk_widget_set_parent (l2, p);

  g_assert_true (gtk_widget_get_parent (l1) == p);
  g_assert_null (gtk_widget_get_prev_sibling (l1));
  g_assert_true (gtk_widget_get_next_sibling (l1) == l2);

  g_assert_true (gtk_widget_get_parent (l2) == p);
  g_assert_true (gtk_widget_get_prev_sibling (l2) == l1);
  g_assert_null (gtk_widget_get_next_sibling (l2));

  g_assert_true (gtk_widget_get_first_child (p) == l1);
  g_assert_true (gtk_widget_get_last_child (p) == l2);

  /* l1 is already before l2, so this shouldn't change anything! */
  gtk_widget_insert_before (l1, p, l2);

  g_assert_true (gtk_widget_get_prev_sibling (l2) == l1);
  g_assert_null (gtk_widget_get_next_sibling (l2));

  g_assert_null (gtk_widget_get_prev_sibling (l1));
  g_assert_true (gtk_widget_get_next_sibling (l1) == l2);

  g_assert_true (gtk_widget_get_first_child (p) == l1);
  g_assert_true (gtk_widget_get_last_child (p) == l2);
}

static void
no_loop (void)
{
  GtkWidget *p = gtk_label_new ("");
  GtkWidget *l1 = gtk_label_new ("");
  GtkWidget *l2 = gtk_label_new ("");
  GtkWidget *l3 = gtk_label_new ("");

  gtk_widget_set_parent (l1, p);
  gtk_widget_set_parent (l2, p);
  gtk_widget_set_parent (l3, p);

  /* l1 -> l2 -> l3 */

  gtk_widget_insert_after (l1, p, l3);
  /* Now: l2 -> l3 -> l1 */
  g_assert_null (gtk_widget_get_prev_sibling (l2));
  g_assert_true (gtk_widget_get_next_sibling (l2) == l3);
  g_assert_true (gtk_widget_get_next_sibling (l3) == l1);
  g_assert_null (gtk_widget_get_next_sibling (l1));
  g_assert_true (gtk_widget_get_prev_sibling (l1) == l3);

  gtk_widget_insert_after (l2, p, l1);
  /* Now: l3 -> l1 -> l2 */
  g_assert_null (gtk_widget_get_prev_sibling (l3));
  g_assert_true (gtk_widget_get_next_sibling (l3) == l1);
  g_assert_true (gtk_widget_get_next_sibling (l1) == l2);
  g_assert_true (gtk_widget_get_prev_sibling (l1) == l3);
  g_assert_true (gtk_widget_get_prev_sibling (l2) == l1);
  g_assert_null (gtk_widget_get_next_sibling (l2));

  gtk_widget_insert_after (l1, p, NULL);
  /* Now: l1 -> l3 -> l2 */
  g_assert_null (gtk_widget_get_prev_sibling (l1));
  g_assert_true (gtk_widget_get_next_sibling (l1) == l3);
  g_assert_true (gtk_widget_get_next_sibling (l3) == l2);
  g_assert_true (gtk_widget_get_prev_sibling (l3) == l1);
  g_assert_true (gtk_widget_get_prev_sibling (l2) == l3);
  g_assert_null (gtk_widget_get_next_sibling (l2));
}

static void
reorder_refcount (void)
{
  GtkWidget *p = gtk_label_new ("");
  GtkWidget *l1 = gtk_label_new ("");
  GtkWidget *l2 = gtk_label_new ("");

  g_assert_true (g_object_is_floating (l1));
  g_assert_cmpint (G_OBJECT (l1)->ref_count, ==, 1);

  gtk_widget_set_parent (l1, p);

  g_assert_true (!g_object_is_floating (l1));
  g_assert_cmpint (G_OBJECT (l1)->ref_count, ==, 1);

  g_assert_true (g_object_is_floating (l2));
  g_assert_cmpint (G_OBJECT (l2)->ref_count, ==, 1);

  gtk_widget_set_parent (l2, p);

  g_assert_true (!g_object_is_floating (l2));
  g_assert_cmpint (G_OBJECT (l2)->ref_count, ==, 1);

  g_assert_true (gtk_widget_get_next_sibling (l1) == l2);

  gtk_widget_insert_before (l2, p, l1);

  g_assert_true (gtk_widget_get_prev_sibling (l1) == l2);

  g_assert_cmpint (G_OBJECT (l1)->ref_count, ==, 1);
  g_assert_cmpint (G_OBJECT (l2)->ref_count, ==, 1);

  gtk_widget_unparent (l1);
  gtk_widget_unparent (l2);
  g_object_unref (g_object_ref_sink (p));
}

int
main (int argc, char **argv)
{
  gtk_init ();
  (g_test_init) (&argc, &argv, NULL);

  g_test_add_func ("/widgetorder/simple", simple);
  g_test_add_func ("/widgetorder/two", two);
  g_test_add_func ("/widgetorder/prepend", prepend);
  g_test_add_func ("/widgetorder/append", append);
  g_test_add_func ("/widgetorder/insert-after", insert_after);
  g_test_add_func ("/widgetorder/insert-before", insert_before);
  g_test_add_func ("/widgetorder/insert-after-self", insert_after_self);
  g_test_add_func ("/widgetorder/insert-before-self", insert_before_self);
  g_test_add_func ("/widgetorder/reorder-after", reorder_after);
  g_test_add_func ("/widgetorder/reorder-before", reorder_before);
  g_test_add_func ("/widgetorder/reorder-start", reorder_start);
  g_test_add_func ("/widgetorder/reorder-end", reorder_end);
  g_test_add_func ("/widgetorder/same-after", same_after);
  g_test_add_func ("/widgetorder/same-before", same_before);
  g_test_add_func ("/widgetorder/no-loop", no_loop);
  g_test_add_func ("/widgetorder/reorder-refcount", reorder_refcount);

  return g_test_run ();
}
