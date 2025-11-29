/* Entry/Undo and Redo
 *
 * GtkEntry can provide basic Undo/Redo support using standard keyboard
 * accelerators such as Control+z to undo and Control+Shift+z to redo.
 * Additionally, Control+y can be used to redo.
 *
 * Use gtk_entry_set_enable_undo() to enable undo/redo support.
 */

#include <glib/gi18n.h>
#include <gtk/gtk.h>

GtkWidget *
do_entry_undo (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *vbox;
  GtkWidget *label;
  GtkWidget *entry;

  if (!window)
    {
      window = gtk_window_new ();
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      gtk_window_set_title (GTK_WINDOW (window), "Undo and Redo");
      gtk_window_set_resizable (GTK_WINDOW (window), FALSE);
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
      gtk_widget_set_margin_start (vbox, 18);
      gtk_widget_set_margin_end (vbox, 18);
      gtk_widget_set_margin_top (vbox, 18);
      gtk_widget_set_margin_bottom (vbox, 18);
      gtk_window_set_child (GTK_WINDOW (window), vbox);

      label = gtk_label_new (NULL);
      gtk_label_set_markup (GTK_LABEL (label),
                            "Use Control+z or Control+Shift+z to undo or redo changes");
      gtk_box_append (GTK_BOX (vbox), label);

      /* Create our entry */
      entry = gtk_entry_new ();
      gtk_editable_set_enable_undo (GTK_EDITABLE (entry), TRUE);
      gtk_box_append (GTK_BOX (vbox), entry);

      gtk_accessible_update_relation (GTK_ACCESSIBLE (entry),
                                      GTK_ACCESSIBLE_RELATION_LABELLED_BY, label, NULL,
                                      -1);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_set_visible (window, TRUE);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
