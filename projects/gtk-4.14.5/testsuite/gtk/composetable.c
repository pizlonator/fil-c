#include <gtk/gtk.h>
#include <locale.h>

#include <glib/gstdio.h>

#include "../gtk/gtkcomposetable.h"
#include "../gtk/gtkimcontextsimpleseqs.h"
#include "testsuite/testutils.h"

static void
append_escaped (GString    *str,
                const char *s)
{
  for (const char *p = s; *p; p = g_utf8_next_char (p))
    {
      gunichar ch = g_utf8_get_char (p);
      if (ch == '"')
        g_string_append (str, "\\\"");
      else if (ch == '\\')
        g_string_append (str, "\\\\");
      else if (g_unichar_isprint (ch))
        g_string_append_unichar (str, ch);
      else
        {
          guint n[8] = { 0, };
          int i = 0;
          while (ch != 0)
            {
              n[i++] = ch & 7;
              ch = ch >> 3;
            }
          for (; i >= 0; i--)
            g_string_append_printf (str, "\\%o", n[i]);
        }
    }
}

static void
print_sequence (gunichar   *sequence,
                int         len,
                const char *value,
                gpointer    data)
{
  GString *str = data;

  for (int j = 0; j < len; j++)
    g_string_append_printf (str, "<U%x> ", sequence[j]);

  g_string_append (str, ": \"");
  append_escaped (str, value);
  g_string_append (str, "\"");

  if (g_utf8_strlen (value, -1) == 1)
    {
      gunichar ch = g_utf8_get_char (value);
      g_string_append_printf (str, " # U%x", ch);
    }

  g_string_append_c (str, '\n');
}

static char *
gtk_compose_table_print (GtkComposeTable *table)
{
  GString *str;

  str = g_string_new ("");

  g_string_append_printf (str, "# n_sequences: %d\n# max_seq_len: %d\n# n_index_size: %d\n# data_size: %d\n# n_chars: %d\n",
                          table->n_sequences,
                          table->max_seq_len,
                          table->n_index_size,
                          table->data_size,
                          table->n_chars);

  gtk_compose_table_foreach (table, print_sequence, str);

  return g_string_free (str, FALSE);
}

static void
generate_output (const char *file)
{
  GtkComposeTable *table;
  char *output;

  table = gtk_compose_table_parse (file, NULL);
  output = gtk_compose_table_print (table);

  g_print ("%s", output);
}

static void
compose_table_compare (gconstpointer data)
{
  const char *basename = data;
  GtkComposeTable *table;
  char *file;
  char *expected;
  char *output;
  char *diff;
  GError *error = NULL;

  file = g_test_build_filename (G_TEST_DIST, "compose", basename, NULL);
  expected = g_strconcat (file, ".expected", NULL);

  table = gtk_compose_table_parse (file, NULL);
  output = gtk_compose_table_print (table);

  diff = diff_with_file (expected, output, -1, &error);
  g_assert_no_error (error);

  if (diff && diff[0])
    {
      g_print ("Resulting output doesn't match reference:\n%s", diff);
      g_test_fail ();
    }

  g_free (output);
  g_free (file);
  g_free (expected);
}

static void
compose_table_cycle (void)
{
  if (g_test_subprocess ())
    {
      char *file;
      GtkComposeTable *table;

      file = g_test_build_filename (G_TEST_DIST, "compose", "cycle", NULL);

      table = gtk_compose_table_parse (file, NULL);
      g_assert_nonnull (table);
      g_free (file);

      return;
    }

  g_test_trap_subprocess (NULL, 0, 0);
  g_test_trap_assert_stderr ("*include cycle detected*");
  g_test_trap_assert_failed ();
}

static void
compose_table_nofile (void)
{
  if (g_test_subprocess ())
    {
      char *file;
      GtkComposeTable *table;

      file = g_build_filename (g_test_get_dir (G_TEST_DIST), "compose", "nofile", NULL);

      table = gtk_compose_table_parse (file, NULL);
      g_assert_nonnull (table);
      g_free (file);

      return;
    }

  g_test_trap_subprocess (NULL, 0, 0);
  g_test_trap_assert_stderr ("*No such file or directory*");
  g_test_trap_assert_failed ();
}

/* Check matching against a small table */
static void
compose_table_match (void)
{
  GtkComposeTable *table;
  char *file;
  guint buffer[8] = { 0, };
  gboolean finish, match, ret;
  GString *output;

  output = g_string_new ("");

  file = g_build_filename (g_test_get_dir (G_TEST_DIST), "compose", "match", NULL);

  table = gtk_compose_table_parse (file, NULL);

  buffer[0] = GDK_KEY_Multi_key;
  buffer[1] = 0;
  ret = gtk_compose_table_check (table, buffer, 1, &finish, &match, output);
  g_assert_true (ret);
  g_assert_false (finish);
  g_assert_false (match);
  g_assert_true (output->len == 0);

  buffer[0] = GDK_KEY_a;
  buffer[1] = 0;
  ret = gtk_compose_table_check (table, buffer, 1, &finish, &match, output);
  g_assert_false (ret);
  g_assert_false (finish);
  g_assert_false (match);
  g_assert_true (output->len == 0);

  buffer[0] = GDK_KEY_Multi_key;
  buffer[1] = GDK_KEY_s;
  buffer[2] = GDK_KEY_e;
  ret = gtk_compose_table_check (table, buffer, 3, &finish, &match, output);
  g_assert_true (ret);
  g_assert_false (finish);
  g_assert_false (match);
  g_assert_true (output->len == 0);

  buffer[0] = GDK_KEY_Multi_key;
  buffer[1] = GDK_KEY_s;
  buffer[2] = GDK_KEY_e;
  buffer[3] = GDK_KEY_q;
  ret = gtk_compose_table_check (table, buffer, 4, &finish, &match, output);
  g_assert_true (ret);
  g_assert_false (finish);
  g_assert_true (match);
  g_assert_cmpstr (output->str, ==, "!");

  g_string_set_size (output, 0);

  buffer[0] = GDK_KEY_Multi_key;
  buffer[1] = GDK_KEY_s;
  buffer[2] = GDK_KEY_e;
  buffer[3] = GDK_KEY_q;
  buffer[4] = GDK_KEY_u;
  ret = gtk_compose_table_check (table, buffer, 5, &finish, &match, output);
  g_assert_true (ret);
  g_assert_true (finish);
  g_assert_true (match);
  g_assert_cmpstr (output->str, ==, "?");

  g_string_set_size (output, 0);

  buffer[0] = GDK_KEY_Multi_key;
  buffer[1] = GDK_KEY_l;
  buffer[2] = GDK_KEY_o;
  buffer[3] = GDK_KEY_n;
  buffer[4] = GDK_KEY_g;
  ret = gtk_compose_table_check (table, buffer, 5, &finish, &match, output);
  g_assert_true (ret);
  g_assert_true (finish);
  g_assert_true (match);
  g_assert_cmpstr (output->str, ==, "this is a long replacement string");

  g_string_set_size (output, 0);

  buffer[0] = GDK_KEY_q;
  buffer[1] = 0;
  ret = gtk_compose_table_check (table, buffer, 1, &finish, &match, output);
  g_assert_true (ret);
  g_assert_false (finish);
  g_assert_true (match);
  g_assert_cmpstr (output->str, ==, "qq");

  g_string_free (output, TRUE);
  g_free (file);
}

extern const GtkComposeTable builtin_compose_table;

/* just check some random sequences */
static void
compose_table_match_builtin (void)
{
  const GtkComposeTable *table = &builtin_compose_table;
  guint buffer[8] = { 0, };
  gboolean finish, match, ret;
  GString *s;

  buffer[0] = GDK_KEY_Multi_key;
  buffer[1] = 0;

  s = g_string_new ("");

  ret = gtk_compose_table_check (table, buffer, 1, &finish, &match, s);
  g_assert_true (ret);
  g_assert_false (finish);
  g_assert_false (match);
  g_assert_true (s->len == 0);

  buffer[0] = GDK_KEY_a;
  buffer[1] = GDK_KEY_b;
  buffer[2] = GDK_KEY_c;
  buffer[3] = 0;

  ret = gtk_compose_table_check (table, buffer, 3, &finish, &match, s);
  g_assert_false (ret);
  g_assert_false (finish);
  g_assert_false (match);
  g_assert_true (s->len == 0);

  buffer[0] = GDK_KEY_Multi_key;
  buffer[1] = GDK_KEY_parenleft;
  buffer[2] = GDK_KEY_j;
  buffer[3] = GDK_KEY_parenright;
  buffer[4] = 0;

  ret = gtk_compose_table_check (table, buffer, 4, &finish, &match, s);
  g_assert_true (ret);
  g_assert_true (finish);
  g_assert_true (match);
  g_assert_cmpstr (s->str, ==, "ⓙ"); /* CIRCLED LATIN SMALL LETTER J */

  buffer[0] = GDK_KEY_dead_acute;
  buffer[1] = GDK_KEY_space;
  buffer[2] = 0;

  g_string_set_size (s, 0);

  ret = gtk_compose_table_check (table, buffer, 2, &finish, &match, s);
  g_assert_true (ret);
  g_assert_true (finish);
  g_assert_true (match);
  g_assert_cmpstr (s->str, ==, "'");

  buffer[0] = GDK_KEY_dead_acute;
  buffer[1] = GDK_KEY_dead_acute;
  buffer[2] = 0;

  g_string_set_size (s, 0);

  ret = gtk_compose_table_check (table, buffer, 2, &finish, &match, s);
  g_assert_true (ret);
  g_assert_true (finish);
  g_assert_true (match);
  g_assert_cmpstr (s->str, ==, "´");

  g_string_free (s, TRUE);
}

static void
match_algorithmic (void)
{
  guint buffer[8] = { 0, };
  gboolean ret;
  GString *output;

  output = g_string_new ("");

  buffer[0] = GDK_KEY_a;
  buffer[1] = GDK_KEY_b;

  ret = gtk_check_algorithmically (buffer, 2, output);
  g_assert_false (ret);
  g_assert_cmpstr (output->str, ==, "");

  buffer[0] = GDK_KEY_dead_abovering;
  buffer[1] = GDK_KEY_A;

  ret = gtk_check_algorithmically (buffer, 2, output);
  g_assert_true (ret);
  g_assert_cmpstr (output->str, ==, "Å");

  buffer[0] = GDK_KEY_A;
  buffer[1] = GDK_KEY_dead_abovering;

  ret = gtk_check_algorithmically (buffer, 2, output);
  g_assert_false (ret);
  g_assert_cmpstr (output->str, ==, "");

  buffer[0] = GDK_KEY_dead_dasia;
  buffer[1] = GDK_KEY_dead_perispomeni;
  buffer[2] = GDK_KEY_Greek_alpha;

  ret = gtk_check_algorithmically (buffer, 3, output);
  g_assert_true (ret);
  g_assert_cmpstr (output->str, ==, "ᾶ\xcc\x94");

  buffer[0] = GDK_KEY_dead_perispomeni;
  buffer[1] = GDK_KEY_dead_dasia;
  buffer[2] = GDK_KEY_Greek_alpha;

  ret = gtk_check_algorithmically (buffer, 3, output);
  g_assert_true (ret);
  g_assert_cmpstr (output->str, ==, "ἇ");

  buffer[0] = GDK_KEY_dead_acute;
  buffer[1] = GDK_KEY_dead_cedilla;
  buffer[2] = GDK_KEY_c;

  ret = gtk_check_algorithmically (buffer, 2, output);
  g_assert_true (ret);
  g_assert_cmpstr (output->str, ==, "");

  ret = gtk_check_algorithmically (buffer, 3, output);
  g_assert_true (ret);
  g_assert_cmpstr (output->str, ==, "ḉ");

  buffer[0] = GDK_KEY_dead_cedilla;
  buffer[1] = GDK_KEY_dead_acute;
  buffer[2] = GDK_KEY_c;

  ret = gtk_check_algorithmically (buffer, 3, output);
  g_assert_true (ret);
  g_assert_cmpstr (output->str, ==, "ḉ");

  ret = gtk_check_algorithmically (buffer, 2, output);
  g_assert_true (ret);

  buffer[0] = GDK_KEY_dead_acute;
  buffer[1] = GDK_KEY_dead_cedilla;
  buffer[2] = GDK_KEY_dead_grave;

  ret = gtk_check_algorithmically (buffer, 3, output);
  g_assert_true (ret);
  g_assert_cmpstr (output->str, ==, "");

  buffer[0] = GDK_KEY_dead_diaeresis;
  buffer[1] = GDK_KEY_a;

  ret = gtk_check_algorithmically (buffer, 2, output);
  g_assert_true (ret);
  g_assert_cmpstr (output->str, ==, "ä");

  g_string_free (output, TRUE);
}

static void
compose_table_large (void)
{
  if (g_test_subprocess ())
    {
      char *file;
      GtkComposeTable *table;

      file = g_test_build_filename (G_TEST_DIST, "compose", "large", NULL);

      table = gtk_compose_table_parse (file, NULL);
      g_assert_nonnull (table);
      g_free (file);

      return;
    }

  g_test_trap_subprocess (NULL, 0, 0);
  g_test_trap_assert_stderr ("*can't handle compose tables this large*");
  g_test_trap_assert_failed ();
}

int
main (int argc, char *argv[])
{
  if (argc == 3 && strcmp (argv[1], "--generate") == 0)
    {
      gtk_disable_setlocale();
      setlocale (LC_ALL, "en_US.UTF-8");

      gtk_init ();

      /* Ensure that the builtin table is initialized */
      GtkIMContext *ctx = gtk_im_context_simple_new ();
      g_object_unref (ctx);

      generate_output (argv[2]);
      return 0;
    }

  gtk_test_init (&argc, &argv, NULL);

  /* Ensure that the builtin table is initialized */
  GtkIMContext *ctx = gtk_im_context_simple_new ();
  g_object_unref (ctx);

  g_test_add_data_func ("/compose-table/basic", "basic", compose_table_compare);
  g_test_add_data_func ("/compose-table/long", "long", compose_table_compare);
  g_test_add_data_func ("/compose-table/octal", "octal", compose_table_compare);
  g_test_add_data_func ("/compose-table/hex", "hex", compose_table_compare);
  g_test_add_data_func ("/compose-table/codepoint", "codepoint", compose_table_compare);
  g_test_add_data_func ("/compose-table/multi", "multi", compose_table_compare);
  g_test_add_data_func ("/compose-table/strings", "strings", compose_table_compare);
  g_test_add_data_func ("/compose-table/single", "single", compose_table_compare);
  g_test_add_data_func ("/compose-table/include", "include", compose_table_compare);
  g_test_add_data_func ("/compose-table/system", "system", compose_table_compare);
  g_test_add_func ("/compose-table/include-cycle", compose_table_cycle);
  g_test_add_func ("/compose-table/include-nofile", compose_table_nofile);
  g_test_add_func ("/compose-table/match", compose_table_match);
  g_test_add_func ("/compose-table/match-builtin", compose_table_match_builtin);
  g_test_add_func ("/compose-table/match-algorithmic", match_algorithmic);
  g_test_add_func ("/compose-table/large", compose_table_large);

  return g_test_run ();
}
