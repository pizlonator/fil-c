#include <gtk/gtk.h>
#include "gtk/gtkcomposetable.h"
#include <locale.h>
#include <stdlib.h>

/* This program reads a Compose file and generates files with sequences,
 * character data, and definitions for the builtin compose table of GTK.
 * Run it like this:
 *
 *   cpp -DXCOMM='#' Compose.pre | sed -e 's/^ *#/#/' > Compose
 *   compose-parse Compose sequences-little-endian sequences-big-endian chars gtkcomposedata.h
 *
 * The GTK build expects the output files to be in the source tree, in
 * the gtk/compose directory.
 */
int
main (int argc, char *argv[])
{
  const guint16 *sequences_le;
  const guint16 *sequences_be;
  guint16 *other_sequences;
  GtkComposeTable *table;
  GError *error = NULL;
  GString *str;
  gsize i;

  setlocale (LC_ALL, "");

  if (argc < 5)
    {
      g_print ("Usage: compose-parse INPUT SEQUENCES-LE SEQUENCES-BE CHARS HEADER\n");
      exit (1);
    }

  table = gtk_compose_table_parse (argv[1], NULL);
  if (!table)
    g_error ("Failed to parse %s", argv[1]);

#if G_BYTE_ORDER == G_BIG_ENDIAN
  sequences_le = other_sequences = g_new0 (guint16, table->data_size);
  sequences_be = (const guint16 *) table->data;
#else
  sequences_le = (const guint16 *) table->data;
  sequences_be = other_sequences = g_new0 (guint16, table->data_size);
#endif

  for (i = 0; i < table->data_size; i++)
    other_sequences[i] = GUINT16_SWAP_LE_BE (table->data[i]);

  /* data_size is the size in guint16 */
  if (!g_file_set_contents (argv[2], (char *) sequences_le, 2 * table->data_size, &error))
    g_error ("%s", error->message);

  /* data_size is the size in guint16 */
  if (!g_file_set_contents (argv[3], (char *) sequences_be, 2 * table->data_size, &error))
    g_error ("%s", error->message);

  if (!g_file_set_contents (argv[4], table->char_data, table->n_chars + 1, &error))
    g_error ("%s", error->message);

  str = g_string_new ("");
  g_string_append (str,
                   "#ifndef __GTK_COMPOSE_DATA__\n"
                   "#define __GTK_COMPOSE_DATA__\n"
                   "\n");
  g_string_append_printf (str,
                   "#define MAX_SEQ_LEN %d\n", table->max_seq_len);
  g_string_append_printf (str,
                   "#define N_INDEX_SIZE %d\n", table->n_index_size);
  g_string_append_printf (str,
                   "#define DATA_SIZE %d\n", table->data_size);
  g_string_append_printf (str,
                   "#define N_CHARS %d\n", table->n_chars);
  g_string_append (str,
                   "\n"
                   "#endif\n");

  if (!g_file_set_contents (argv[5], str->str, str->len, &error))
    g_error ("%s", error->message);

  g_string_free (str, TRUE);
  g_free (other_sequences);

  return 0;
}
