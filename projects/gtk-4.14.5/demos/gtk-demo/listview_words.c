/* Lists/Words
 * #Keywords: GtkListView, GtkFilterListModel, GtkInscription
 *
 * This demo shows filtering a long list - of words.
 *
 * You should have the file `/usr/share/dict/words` installed for
 * this demo to work.
 */

#include <gtk/gtk.h>

static GtkWidget *window = NULL;
static GtkWidget *progress;

const char *factory_text =
"<?xml version='1.0' encoding='UTF-8'?>\n"
"<interface>\n"
"  <template class='GtkListItem'>\n"
"    <property name='child'>\n"
"      <object class='GtkInscription'>\n"
"        <property name='xalign'>0</property>\n"
"        <binding name='text'>\n"
"          <lookup name='string' type='GtkStringObject'>\n"
"            <lookup name='item'>GtkListItem</lookup>\n"
"          </lookup>\n"
"        </binding>\n"
"      </object>\n"
"    </property>\n"
"  </template>\n"
"</interface>\n";

static void
update_title_cb (GtkFilterListModel *model)
{
  guint total;
  char *title;
  guint pending;

  total = g_list_model_get_n_items (gtk_filter_list_model_get_model (model));
  pending = gtk_filter_list_model_get_pending (model);

  title = g_strdup_printf ("%u lines", g_list_model_get_n_items (G_LIST_MODEL (model)));

  gtk_widget_set_visible (progress, pending != 0);
  gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (progress), total > 0 ? (total - pending) / (double) total : 0.);
  gtk_window_set_title (GTK_WINDOW (window), title);
  g_free (title);
}

static void
read_lines_cb (GObject      *object,
               GAsyncResult *result,
               gpointer      data)
{
  GBufferedInputStream *stream = G_BUFFERED_INPUT_STREAM (object);
  GtkStringList *stringlist = data;
  GError *error = NULL;
  gsize size;
  GPtrArray *lines;
  gssize n_filled;
  const char *buffer, *newline;

  n_filled = g_buffered_input_stream_fill_finish (stream, result, &error);
  if (n_filled < 0)
    {
      g_print ("Could not read data: %s\n", error->message);
      g_clear_error (&error);
      g_object_unref (stringlist);
      return;
    }

  buffer = g_buffered_input_stream_peek_buffer (stream, &size);

  if (n_filled == 0)
    {
      if (size)
        gtk_string_list_take (stringlist, g_utf8_make_valid (buffer, size));
      g_object_unref (stringlist);
      return;
    }

  lines = NULL;
  while ((newline = memchr (buffer, '\n', size)))
    {
      if (newline > buffer)
        {
          if (lines == NULL)
            lines = g_ptr_array_new_with_free_func (g_free);
          g_ptr_array_add (lines, g_utf8_make_valid (buffer, newline - buffer));
        }
      if (g_input_stream_skip (G_INPUT_STREAM (stream), newline - buffer + 1, NULL, &error) < 0)
        {
          g_clear_error (&error);
          break;
        }
      buffer = g_buffered_input_stream_peek_buffer (stream, &size);
    }
  if (lines == NULL)
    {
      g_buffered_input_stream_set_buffer_size (stream, g_buffered_input_stream_get_buffer_size (stream) + 4096);
    }
  else
    {
      g_ptr_array_add (lines, NULL);
      gtk_string_list_splice (stringlist, g_list_model_get_n_items (G_LIST_MODEL (stringlist)), 0, (const char **) lines->pdata);
      g_ptr_array_free (lines, TRUE);
    }

  g_buffered_input_stream_fill_async (stream, -1, G_PRIORITY_HIGH_IDLE, NULL, read_lines_cb, data);
}
               
static void
file_is_open_cb (GObject      *file,
                 GAsyncResult *result,
                 gpointer      data)
{
  GError *error = NULL;
  GFileInputStream *file_stream;
  GBufferedInputStream *stream;

  file_stream = g_file_read_finish (G_FILE (file), result, &error);
  if (file_stream == NULL)
    {
      g_print ("Could not open file: %s\n", error->message);
      g_error_free (error);
      g_object_unref (data);
      return;
    }

  stream = G_BUFFERED_INPUT_STREAM (g_buffered_input_stream_new (G_INPUT_STREAM (file_stream)));
  g_buffered_input_stream_fill_async (stream, -1, G_PRIORITY_HIGH_IDLE, NULL, read_lines_cb, data);
  g_object_unref (stream);
}

static void
load_file (GtkStringList *list,
           GFile         *file)
{
  gtk_string_list_splice (list, 0, g_list_model_get_n_items (G_LIST_MODEL (list)), NULL);
  g_file_read_async (file, G_PRIORITY_HIGH_IDLE, NULL, file_is_open_cb, g_object_ref (list));
}

static void
open_response_cb (GObject *source,
                  GAsyncResult *result,
                  void *user_data)
{
  GtkFileDialog *dialog = GTK_FILE_DIALOG (source);
  GtkStringList *stringlist = GTK_STRING_LIST (user_data);
  GFile *file;

  file = gtk_file_dialog_open_finish (dialog, result, NULL);
  if (file)
    {
      load_file (stringlist, file);
      g_object_unref (file);
    }
}

static void
file_open_cb (GtkWidget     *button,
              GtkStringList *stringlist)
{
  GtkFileDialog *dialog;

  dialog = gtk_file_dialog_new ();
  gtk_file_dialog_open (dialog,
                        GTK_WINDOW (gtk_widget_get_root (button)),
                        NULL,
                        open_response_cb, stringlist);
  g_object_unref (dialog);
}

GtkWidget *
do_listview_words (GtkWidget *do_widget)
{
  if (window == NULL)
    {
      GtkWidget *header, *listview, *sw, *vbox, *search_entry, *open_button, *overlay;
      GtkFilterListModel *filter_model;
      GtkStringList *stringlist;
      GtkFilter *filter;
      GFile *file;

      file = g_file_new_for_path ("/usr/share/dict/words");
      if (g_file_query_exists (file, NULL))
        {
          stringlist = gtk_string_list_new (NULL);
          load_file (stringlist, file);
        }
      else
        {
          char **words;
          words = g_strsplit ("lorem ipsum dolor sit amet consectetur adipisci elit sed eiusmod tempor incidunt labore et dolore magna aliqua ut enim ad minim veniam quis nostrud exercitation ullamco laboris nisi ut aliquid ex ea commodi consequat", " ", -1);
          stringlist = gtk_string_list_new ((const char **) words);
          g_strfreev (words);
        }
      g_object_unref (file);

      filter = GTK_FILTER (gtk_string_filter_new (gtk_property_expression_new (GTK_TYPE_STRING_OBJECT, NULL, "string")));
      filter_model = gtk_filter_list_model_new (G_LIST_MODEL (stringlist), filter);
      gtk_filter_list_model_set_incremental (filter_model, TRUE);

      window = gtk_window_new ();
      gtk_window_set_default_size (GTK_WINDOW (window), 400, 600);

      header = gtk_header_bar_new ();
      gtk_header_bar_set_show_title_buttons (GTK_HEADER_BAR (header), TRUE);
      open_button = gtk_button_new_with_mnemonic ("_Open");
      g_signal_connect (open_button, "clicked", G_CALLBACK (file_open_cb), stringlist);
      gtk_header_bar_pack_start (GTK_HEADER_BAR (header), open_button);
      gtk_window_set_titlebar (GTK_WINDOW (window), header);

      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer*)&window);

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_window_set_child (GTK_WINDOW (window), vbox);

      search_entry = gtk_search_entry_new ();
      g_object_bind_property (search_entry, "text", filter, "search", 0);
      gtk_box_append (GTK_BOX (vbox), search_entry);

      overlay = gtk_overlay_new ();
      gtk_box_append (GTK_BOX (vbox), overlay);

      progress = gtk_progress_bar_new ();
      gtk_widget_set_halign (progress, GTK_ALIGN_FILL);
      gtk_widget_set_valign (progress, GTK_ALIGN_START);
      gtk_widget_set_hexpand (progress, TRUE);
      gtk_overlay_add_overlay (GTK_OVERLAY (overlay), progress);

      sw = gtk_scrolled_window_new ();
      gtk_widget_set_vexpand (sw, TRUE);
      gtk_overlay_set_child (GTK_OVERLAY (overlay), sw);

      listview = gtk_list_view_new (
          GTK_SELECTION_MODEL (gtk_no_selection_new (G_LIST_MODEL (filter_model))),
          gtk_builder_list_item_factory_new_from_bytes (NULL,
              g_bytes_new_static (factory_text, strlen (factory_text))));
      gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), listview);

      g_signal_connect (filter_model, "items-changed", G_CALLBACK (update_title_cb), progress);
      g_signal_connect (filter_model, "notify::pending", G_CALLBACK (update_title_cb), progress);
      update_title_cb (filter_model);

    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_set_visible (window, TRUE);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
