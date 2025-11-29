/* Dialogs
 * #Keywords: GtkMessageDialog
 *
 * Dialogs are used to pop up transient windows for information
 * and user feedback.
 */

#include <glib/gi18n.h>
#include <gtk/gtk.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

static GtkWidget *window = NULL;
static GtkWidget *entry1 = NULL;
static GtkWidget *entry2 = NULL;

static void
message_dialog_clicked (GtkButton *button,
                        gpointer   user_data)
{
  GtkWidget *dialog;
  static int i = 1;

  dialog = gtk_message_dialog_new (GTK_WINDOW (window),
                                   GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_INFO,
                                   GTK_BUTTONS_OK_CANCEL,
                                   "Test message");
  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                            ngettext ("Has been shown once", "Has been shown %d times", i), i);
  g_signal_connect (dialog, "response", G_CALLBACK (gtk_window_destroy), NULL);
  gtk_window_present (GTK_WINDOW (dialog));
  i++;
}

typedef struct {
  GtkWidget *local_entry1;
  GtkWidget *local_entry2;
  GtkWidget *global_entry1;
  GtkWidget *global_entry2;
} ResponseData;

static void
on_dialog_response (GtkDialog *dialog,
                    int        response,
                    gpointer   user_data)
{
  ResponseData *data = user_data;

  if (response == GTK_RESPONSE_OK)
    {
      gtk_editable_set_text (GTK_EDITABLE (data->global_entry1),
                             gtk_editable_get_text (GTK_EDITABLE (data->local_entry1)));
      gtk_editable_set_text (GTK_EDITABLE (data->global_entry2),
                             gtk_editable_get_text (GTK_EDITABLE (data->local_entry2)));
    }

  gtk_window_destroy (GTK_WINDOW (dialog));
}

static void
interactive_dialog_clicked (GtkButton *button,
                            gpointer   user_data)
{
  GtkWidget *content_area;
  GtkWidget *dialog;
  GtkWidget *table;
  GtkWidget *local_entry1;
  GtkWidget *local_entry2;
  GtkWidget *label;
  ResponseData *data;

  dialog = gtk_dialog_new_with_buttons ("Interactive Dialog",
                                        GTK_WINDOW (window),
                                        GTK_DIALOG_MODAL| GTK_DIALOG_DESTROY_WITH_PARENT|GTK_DIALOG_USE_HEADER_BAR,
                                        _("_OK"), GTK_RESPONSE_OK,
                                        _("_Cancel"), GTK_RESPONSE_CANCEL,
                                        NULL);

  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

  content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

  table = gtk_grid_new ();
  gtk_widget_set_hexpand (table, TRUE);
  gtk_widget_set_vexpand (table, TRUE);
  gtk_widget_set_halign (table, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (table, GTK_ALIGN_CENTER);
  gtk_box_append (GTK_BOX (content_area), table);
  gtk_grid_set_row_spacing (GTK_GRID (table), 6);
  gtk_grid_set_column_spacing (GTK_GRID (table), 6);

  label = gtk_label_new_with_mnemonic ("_Entry 1");
  gtk_grid_attach (GTK_GRID (table), label, 0, 0, 1, 1);
  local_entry1 = gtk_entry_new ();
  gtk_editable_set_text (GTK_EDITABLE (local_entry1), gtk_editable_get_text (GTK_EDITABLE (entry1)));
  gtk_grid_attach (GTK_GRID (table), local_entry1, 1, 0, 1, 1);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), local_entry1);

  label = gtk_label_new_with_mnemonic ("E_ntry 2");
  gtk_grid_attach (GTK_GRID (table), label, 0, 1, 1, 1);

  local_entry2 = gtk_entry_new ();
  gtk_editable_set_text (GTK_EDITABLE (local_entry2), gtk_editable_get_text (GTK_EDITABLE (entry2)));
  gtk_grid_attach (GTK_GRID (table), local_entry2, 1, 1, 1, 1);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), local_entry2);

  data = g_new (ResponseData, 1);
  data->local_entry1 = local_entry1;
  data->local_entry2 = local_entry2;
  data->global_entry1 = entry1;
  data->global_entry2 = entry2;

  g_signal_connect_data (dialog, "response",
                         G_CALLBACK (on_dialog_response),
                         data, (GClosureNotify) g_free,
                         0);

  gtk_window_present (GTK_WINDOW (dialog));
}

GtkWidget *
do_dialog (GtkWidget *do_widget)
{
  GtkWidget *vbox;
  GtkWidget *vbox2;
  GtkWidget *hbox;
  GtkWidget *button;
  GtkWidget *table;
  GtkWidget *label;

  if (!window)
    {
      window = gtk_window_new ();
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      gtk_window_set_title (GTK_WINDOW (window), "Dialogs");
      gtk_window_set_resizable (GTK_WINDOW (window), FALSE);
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
      gtk_widget_set_margin_start (vbox, 8);
      gtk_widget_set_margin_end (vbox, 8);
      gtk_widget_set_margin_top (vbox, 8);
      gtk_widget_set_margin_bottom (vbox, 8);
      gtk_window_set_child (GTK_WINDOW (window), vbox);

      /* Standard message dialog */
      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
      gtk_box_append (GTK_BOX (vbox), hbox);
      button = gtk_button_new_with_mnemonic ("_Message Dialog");
      g_signal_connect (button, "clicked",
                        G_CALLBACK (message_dialog_clicked), NULL);
      gtk_box_append (GTK_BOX (hbox), button);

      gtk_box_append (GTK_BOX (vbox), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));

      /* Interactive dialog*/
      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
      gtk_box_append (GTK_BOX (vbox), hbox);
      vbox2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

      button = gtk_button_new_with_mnemonic ("_Interactive Dialog");
      g_signal_connect (button, "clicked",
                        G_CALLBACK (interactive_dialog_clicked), NULL);
      gtk_box_append (GTK_BOX (hbox), vbox2);
      gtk_box_append (GTK_BOX (vbox2), button);

      table = gtk_grid_new ();
      gtk_grid_set_row_spacing (GTK_GRID (table), 4);
      gtk_grid_set_column_spacing (GTK_GRID (table), 4);
      gtk_box_append (GTK_BOX (hbox), table);

      label = gtk_label_new_with_mnemonic ("_Entry 1");
      gtk_grid_attach (GTK_GRID (table), label, 0, 0, 1, 1);

      entry1 = gtk_entry_new ();
      gtk_grid_attach (GTK_GRID (table), entry1, 1, 0, 1, 1);
      gtk_label_set_mnemonic_widget (GTK_LABEL (label), entry1);

      label = gtk_label_new_with_mnemonic ("E_ntry 2");
      gtk_grid_attach (GTK_GRID (table), label, 0, 1, 1, 1);

      entry2 = gtk_entry_new ();
      gtk_grid_attach (GTK_GRID (table), entry2, 1, 1, 1, 1);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_set_visible (window, TRUE);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
