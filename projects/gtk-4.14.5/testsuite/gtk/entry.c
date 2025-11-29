/* GTK - The GIMP Toolkit
 * Copyright (C) 2011 Red Hat, Inc.
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

static int serial = 0;

typedef struct {
  int serial;
  int count;
  int start;
  int end;
  char *text;
  char *new_text;
  int position;
  int length;
} EntryData;

static void
notify (GtkEditable *editable, GParamSpec *pspec, EntryData *data)
{
  data->serial = serial++;
  data->count++;
  data->text = gtk_editable_get_chars (editable, 0, -1);
  gtk_editable_get_selection_bounds (editable, &data->start, &data->end);

#if 0
  g_print ("notify::%s\n", pspec->name);
  g_print ("\ttext: %s\n", data->text);
  g_print ("\tstart: %d\n", data->start);
  g_print ("\tend: %d\n", data->end);
#endif
}

static void
insert_text (GtkEditable *editable,
             const char *new_text,
             int          new_text_length,
             int         *position,
             EntryData   *data)
{
  data->serial = serial++;
  data->count++;
  data->text = gtk_editable_get_chars (editable, 0, -1);
  gtk_editable_get_selection_bounds (editable, &data->start, &data->end);
  data->new_text = g_strdup (new_text);
  data->position = *position;
  data->length = new_text_length;

#if 0
  g_print ("insert-text \"%s\", %d\n", new_text, *position);
  g_print ("\ttext: %s\n", data->text);
  g_print ("\tstart: %d\n", data->start);
  g_print ("\tend: %d\n", data->end);
#endif
}

static void
delete_text (GtkEditable *editable,
             int          start_pos,
             int          end_pos,
             EntryData   *data)
{
  data->serial = serial++;
  data->count++;
  data->text = gtk_editable_get_chars (editable, 0, -1);
  gtk_editable_get_selection_bounds (editable, &data->start, &data->end);
  data->position = start_pos;
  data->length = end_pos - start_pos;

#if 0
  g_print ("delete-text %d %d\n", start_pos, end_pos);
  g_print ("\ttext: %s\n", data->text);
  g_print ("\tstart: %d\n", data->start);
  g_print ("\tend: %d\n", data->end);
#endif
}

static void
changed (GtkEditable *editable,
         EntryData   *data)
{
  data->serial = serial++;
  data->count++;
  data->text = gtk_editable_get_chars (editable, 0, -1);
  gtk_editable_get_selection_bounds (editable, &data->start, &data->end);

#if 0
  g_print ("changed\n");
  g_print ("\ttext: %s\n", data->text);
  g_print ("\tstart: %d\n", data->start);
  g_print ("\tend: %d\n", data->end);
#endif
}

static void
test_insert (void)
{
  GtkWidget *entry;
  int pos;
  EntryData data1;
  EntryData data2;
  EntryData data3;
  EntryData data4;
  EntryData data5;
  EntryData data6;

  entry = gtk_entry_new ();
  g_object_ref_sink (entry);

  gtk_editable_set_text (GTK_EDITABLE (entry), "bar");
  gtk_editable_set_position (GTK_EDITABLE (entry), -1);
  pos = gtk_editable_get_position (GTK_EDITABLE (entry));
  g_assert_cmpint (pos, ==, 3);

  data1.count = 0;
  data2.count = 0;
  data3.count = 0;
  data4.count = 0;
  data5.count = 0;
  data6.count = 0;
  g_signal_connect (entry, "notify::cursor-position",
                    G_CALLBACK (notify), &data1);
  g_signal_connect (entry, "notify::selection-bound",
                    G_CALLBACK (notify), &data2);
  g_signal_connect (entry, "notify::text",
                    G_CALLBACK (notify), &data3);
  g_signal_connect (entry, "insert-text",
                    G_CALLBACK (insert_text), &data4);
  g_signal_connect (entry, "delete-text",
                    G_CALLBACK (delete_text), &data5);
  g_signal_connect (entry, "changed",
                    G_CALLBACK (changed), &data6);

  pos = 0;
  gtk_editable_insert_text (GTK_EDITABLE (entry), "foo", -1, &pos);
  g_assert_cmpint (pos, ==, 3);

  pos = gtk_editable_get_position (GTK_EDITABLE (entry));
  g_assert_cmpint (pos, ==, 6);

  /* Check that notification for ::text, ::cursor-position and
   * ::selection-bound happens in a consistent state after the
   * change.
   */
  g_assert_cmpint (data1.count, ==, 1);
  g_assert_cmpint (data1.start, ==, 6);
  g_assert_cmpint (data1.end, ==, 6);
  g_assert_cmpstr (data1.text, ==, "foobar");
  g_free (data1.text);

  g_assert_cmpint (data2.count, ==, 1);
  g_assert_cmpint (data2.start, ==, 6);
  g_assert_cmpint (data2.end, ==, 6);
  g_assert_cmpstr (data2.text, ==, "foobar");
  g_free (data2.text);

  g_assert_cmpint (data3.count, ==, 1);
  g_assert_cmpint (data3.start, ==, 6);
  g_assert_cmpint (data3.end, ==, 6);
  g_assert_cmpstr (data3.text, ==, "foobar");
  g_free (data3.text);

  /* Check that ::insert-text sees the state _before_ the insertion */
  g_assert_cmpint (data4.count, ==, 1);
  g_assert_cmpint (data4.start, ==, 3);
  g_assert_cmpint (data4.end, ==, 3);
  g_assert_cmpstr (data4.text, ==, "bar");
  g_assert_cmpint (data4.position, ==, 0);
  g_assert_cmpint (data4.length, ==, 3);
  g_assert_cmpstr (data4.new_text, ==, "foo");
  g_free (data4.text);
  g_free (data4.new_text);

  /* no deletion here */
  g_assert_cmpint (data5.count, ==, 0);

  /* Check that ::changed sees the post-change state */
  g_assert_cmpint (data6.count, ==, 1);
  g_assert_cmpint (data6.start, ==, 6);
  g_assert_cmpint (data6.end, ==, 6);
  g_assert_cmpstr (data6.text, ==, "foobar");
  g_free (data6.text);

  /* Now check ordering: ::insert-text comes before ::notify */
  g_assert_cmpint (data4.serial, <, data1.serial);
  g_assert_cmpint (data4.serial, <, data2.serial);
  g_assert_cmpint (data4.serial, <, data3.serial);

  /* ... and ::changed comes after ::notify */
  g_assert_cmpint (data6.serial, >, data1.serial);
  g_assert_cmpint (data6.serial, >, data2.serial);
  g_assert_cmpint (data6.serial, >, data3.serial);

  g_object_unref (entry);
}

static void
test_delete (void)
{
  GtkWidget *entry;
  int pos;
  EntryData data1;
  EntryData data2;
  EntryData data3;
  EntryData data4;
  EntryData data5;
  EntryData data6;

  entry = gtk_entry_new ();
  g_object_ref_sink (entry);

  gtk_editable_set_text (GTK_EDITABLE (entry), "foobar");
  gtk_editable_set_position (GTK_EDITABLE (entry), -1);
  pos = gtk_editable_get_position (GTK_EDITABLE (entry));
  g_assert_cmpint (pos, ==, 6);

  data1.count = 0;
  data2.count = 0;
  data3.count = 0;
  data4.count = 0;
  data5.count = 0;
  data6.count = 0;
  g_signal_connect (entry, "notify::cursor-position",
                    G_CALLBACK (notify), &data1);
  g_signal_connect (entry, "notify::selection-bound",
                    G_CALLBACK (notify), &data2);
  g_signal_connect (entry, "notify::text",
                    G_CALLBACK (notify), &data3);
  g_signal_connect (entry, "insert-text",
                    G_CALLBACK (insert_text), &data4);
  g_signal_connect (entry, "delete-text",
                    G_CALLBACK (delete_text), &data5);
  g_signal_connect (entry, "changed",
                    G_CALLBACK (changed), &data6);

  gtk_editable_delete_text (GTK_EDITABLE (entry), 0, 3);

  pos = gtk_editable_get_position (GTK_EDITABLE (entry));
  g_assert_cmpint (pos, ==, 3);

  /* Check that notification for ::text, ::cursor-position and
   * ::selection-bound happens in a consistent state after the
   * change.
   */
  g_assert_cmpint (data1.count, ==, 1);
  g_assert_cmpint (data1.start, ==, 3);
  g_assert_cmpint (data1.end, ==, 3);
  g_assert_cmpstr (data1.text, ==, "bar");
  g_free (data1.text);

  g_assert_cmpint (data2.count, ==, 1);
  g_assert_cmpint (data2.start, ==, 3);
  g_assert_cmpint (data2.end, ==, 3);
  g_assert_cmpstr (data2.text, ==, "bar");
  g_free (data2.text);

  g_assert_cmpint (data3.count, ==, 1);
  g_assert_cmpint (data3.start, ==, 3);
  g_assert_cmpint (data3.end, ==, 3);
  g_assert_cmpstr (data3.text, ==, "bar");
  g_free (data3.text);

  /* no insertion here */
  g_assert_cmpint (data4.count, ==, 0);

  /* Check that ::delete-text sees the state _before_ the insertion */
  g_assert_cmpint (data5.count, ==, 1);
  g_assert_cmpint (data5.start, ==, 6);
  g_assert_cmpint (data5.end, ==, 6);
  g_assert_cmpstr (data5.text, ==, "foobar");
  g_assert_cmpint (data5.position, ==, 0);
  g_assert_cmpint (data5.length, ==, 3);
  g_free (data5.text);

  /* Check that ::changed sees the post-change state */
  g_assert_cmpint (data6.count, ==, 1);
  g_assert_cmpint (data6.start, ==, 3);
  g_assert_cmpint (data6.end, ==, 3);
  g_assert_cmpstr (data6.text, ==, "bar");
  g_free (data6.text);

  /* Now check ordering: ::delete-text comes before ::notify */
  g_assert_cmpint (data5.serial, <, data1.serial);
  g_assert_cmpint (data5.serial, <, data2.serial);
  g_assert_cmpint (data5.serial, <, data3.serial);

  /* ... and ::changed comes after ::notify */
  g_assert_cmpint (data6.serial, >, data1.serial);
  g_assert_cmpint (data6.serial, >, data2.serial);
  g_assert_cmpint (data6.serial, >, data3.serial);
  g_object_unref (entry);
}

static void
test_editable (void)
{
  GtkWidget *entry;
  int start, end;
  gboolean res;
  char *text;

  entry = gtk_entry_new ();
  g_object_ref_sink (entry);

  res = gtk_editable_get_selection_bounds (GTK_EDITABLE (entry), &start, &end);
  g_assert_false (res);
  g_assert_cmpint (start, ==, end);

  text = gtk_editable_get_chars (GTK_EDITABLE (entry), start, end);
  g_assert_nonnull (text);
  g_assert_cmpstr (text, ==, "");
  g_free (text);

  gtk_editable_set_text (GTK_EDITABLE (entry), "ABC");
  gtk_editable_select_region (GTK_EDITABLE (entry), 1, 2);

  res = gtk_editable_get_selection_bounds (GTK_EDITABLE (entry), &start, &end);
  g_assert_true (res);
  g_assert_cmpint (start, ==, 1);
  g_assert_cmpint (end, ==, 2);

  text = gtk_editable_get_chars (GTK_EDITABLE (entry), start, end);
  g_assert_nonnull (text);
  g_assert_cmpstr (text, ==, "B");
  g_free (text);

  g_object_unref (entry);
}

int
main (int   argc,
      char *argv[])
{
  gtk_test_init (&argc, &argv);

  g_test_add_func ("/entry/delete", test_delete);
  g_test_add_func ("/entry/insert", test_insert);
  g_test_add_func ("/entry/editable", test_editable);

  return g_test_run();
}
