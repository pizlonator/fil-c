#include <gtk/gtk.h>

#include "gtk/gtkaccessibletextprivate.h"

static void
inscription_text_interface (void)
{
  GtkWidget *inscription = gtk_inscription_new ("");
  GBytes *bytes;
  gsize len;
  gboolean res;
  gsize n_ranges;
  GtkAccessibleTextRange *ranges = NULL;
  char **attr_names, **attr_values;
  const char *string;
  unsigned int start, end;

  g_object_ref_sink (inscription);

  gtk_inscription_set_markup (GTK_INSCRIPTION (inscription), "<markup>a<span overline='single'>b</span>c</markup> def");

  bytes = gtk_accessible_text_get_contents (GTK_ACCESSIBLE_TEXT (inscription), 0, G_MAXINT);
  string = g_bytes_get_data (bytes, &len);
  g_assert_cmpint (len, ==, 8);
  g_assert_cmpstr (string, ==, "abc def");
  g_bytes_unref (bytes);

  bytes = gtk_accessible_text_get_contents_at (GTK_ACCESSIBLE_TEXT (inscription), 1, GTK_ACCESSIBLE_TEXT_GRANULARITY_WORD, &start, &end);
  string = g_bytes_get_data (bytes, &len);
  g_assert_cmpint (len, ==, 5);
  g_assert_cmpint (start, ==, 0);
  g_assert_cmpint (end, ==, 4);
  g_assert_cmpstr (string, ==, "abc ");
  g_bytes_unref (bytes);

  g_assert_cmpint (gtk_accessible_text_get_caret_position (GTK_ACCESSIBLE_TEXT (inscription)), ==, 0);

  res = gtk_accessible_text_get_selection (GTK_ACCESSIBLE_TEXT (inscription), &n_ranges, &ranges);
  g_assert_false (res);

  res = gtk_accessible_text_get_attributes (GTK_ACCESSIBLE_TEXT (inscription), 1, &n_ranges, &ranges, &attr_names, &attr_values);
  g_assert_true (res);
  g_assert_cmpint (n_ranges, ==, 1);
  g_assert_cmpuint (ranges[0].start, ==, 1);
  g_assert_cmpuint (ranges[0].length, ==, 1);
  g_assert_cmpstr (attr_names[0], ==, GTK_ACCESSIBLE_ATTRIBUTE_OVERLINE);
  g_assert_cmpstr (attr_values[0], ==, GTK_ACCESSIBLE_ATTRIBUTE_OVERLINE_SINGLE);
  g_free (ranges);
  g_strfreev (attr_names);
  g_strfreev (attr_values);

  g_object_unref (inscription);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/a11y/inscription/text-interface", inscription_text_interface);

  return g_test_run ();
}
