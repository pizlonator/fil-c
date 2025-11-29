#include <gtk/gtk.h>

#include "gtk/gtkaccessibletextprivate.h"

static void
label_role (void)
{
  GtkWidget *label = gtk_label_new ("a");

  g_object_ref_sink (label);

  gtk_test_accessible_assert_role (GTK_ACCESSIBLE (label), GTK_ACCESSIBLE_ROLE_LABEL);

  g_object_unref (label);
}

static void
label_relations (void)
{
  GtkWidget *label = gtk_label_new ("a");
  GtkWidget *label2 = gtk_label_new ("b");
  GtkWidget *entry = gtk_entry_new ();

  g_object_ref_sink (label);
  g_object_ref_sink (label2);
  g_object_ref_sink (entry);

  gtk_test_accessible_assert_relation (GTK_ACCESSIBLE (entry), GTK_ACCESSIBLE_RELATION_LABELLED_BY, NULL);

  gtk_widget_add_mnemonic_label (entry, label);

  gtk_test_accessible_assert_relation (GTK_ACCESSIBLE (entry), GTK_ACCESSIBLE_RELATION_LABELLED_BY, label, NULL);

  gtk_widget_add_mnemonic_label (entry, label2);

  gtk_test_accessible_assert_relation (GTK_ACCESSIBLE (entry), GTK_ACCESSIBLE_RELATION_LABELLED_BY, label, label2, NULL);

  g_object_unref (entry);
  g_object_unref (label);
  g_object_unref (label2);
}

static void
label_properties (void)
{
  GtkWidget *label = gtk_label_new ("a");

  g_object_ref_sink (label);

  gtk_label_set_selectable (GTK_LABEL (label), TRUE);

  gtk_test_accessible_assert_property (GTK_ACCESSIBLE (label), GTK_ACCESSIBLE_PROPERTY_HAS_POPUP, TRUE);

  gtk_label_set_selectable (GTK_LABEL (label), FALSE);

  g_assert_false (gtk_test_accessible_has_property (GTK_ACCESSIBLE (label), GTK_ACCESSIBLE_PROPERTY_HAS_POPUP));

  g_object_unref (label);
}

static void
label_text_interface (void)
{
  GtkWidget *label = gtk_label_new ("");
  GBytes *bytes;
  gsize len;
  gboolean res;
  gsize n_ranges;
  GtkAccessibleTextRange *ranges = NULL;
  char **attr_names, **attr_values;
  const char *string;
  unsigned int start, end;

  g_object_ref_sink (label);

  gtk_label_set_markup (GTK_LABEL (label), "<markup>a<span underline='single'>b</span>c def</markup>");
  gtk_label_set_selectable (GTK_LABEL (label), TRUE);
  gtk_label_select_region (GTK_LABEL (label), 1, 2);

  bytes = gtk_accessible_text_get_contents (GTK_ACCESSIBLE_TEXT (label), 0, G_MAXINT);
  string = g_bytes_get_data (bytes, &len);
  g_assert_cmpint (len, ==, 8);
  g_assert_cmpstr (string, ==, "abc def");
  g_bytes_unref (bytes);

  bytes = gtk_accessible_text_get_contents_at (GTK_ACCESSIBLE_TEXT (label), 1, GTK_ACCESSIBLE_TEXT_GRANULARITY_WORD, &start, &end);
  string = g_bytes_get_data (bytes, &len);
  g_assert_cmpint (len, ==, 5);
  g_assert_cmpint (start, ==, 0);
  g_assert_cmpint (end, ==, 4);
  g_assert_cmpstr (string, ==, "abc ");
  g_bytes_unref (bytes);

  g_assert_cmpint (gtk_accessible_text_get_caret_position (GTK_ACCESSIBLE_TEXT (label)), ==, 2);

  res = gtk_accessible_text_get_selection (GTK_ACCESSIBLE_TEXT (label), &n_ranges, &ranges);
  g_assert_true (res);
  g_assert_cmpint (n_ranges, ==, 1);
  g_assert_cmpuint (ranges[0].start, ==, 1);
  g_assert_cmpuint (ranges[0].length, ==, 1);
  g_free (ranges);

  // Waiting for the attribute api to be fixed
  res = gtk_accessible_text_get_attributes (GTK_ACCESSIBLE_TEXT (label), 1, &n_ranges, &ranges, &attr_names, &attr_values);
  for (int i = 0; i < n_ranges; i++)
    g_print ("%s = %s\n", attr_names[i], attr_values[i]);
  g_assert_true (res);
  g_assert_cmpint (n_ranges, ==, 1);
  g_assert_cmpuint (ranges[0].start, ==, 1);
  g_assert_cmpuint (ranges[0].length, ==, 1);
  g_assert_cmpstr (attr_names[0], ==, GTK_ACCESSIBLE_ATTRIBUTE_UNDERLINE);
  g_assert_cmpstr (attr_values[0], ==, GTK_ACCESSIBLE_ATTRIBUTE_UNDERLINE_SINGLE);
  g_free (ranges);
  g_strfreev (attr_names);
  g_strfreev (attr_values);

  g_object_unref (label);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/a11y/label/role", label_role);
  g_test_add_func ("/a11y/label/relations", label_relations);
  g_test_add_func ("/a11y/label/properties", label_properties);
  g_test_add_func ("/a11y/label/text-interface", label_text_interface);

  return g_test_run ();
}
