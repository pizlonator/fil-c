#include <gtk/gtk.h>

static GtkWidget *win;
static GtkWidget *inhibit_entry;
static GtkWidget *inhibit_logout;
static GtkWidget *inhibit_switch;
static GtkWidget *inhibit_suspend;
static GtkWidget *inhibit_idle;
static GtkWidget *inhibit_label;

static void
inhibitor_toggled (GtkToggleButton *button, GtkApplication *app)
{
  gboolean active;
  const char *reason;
  GtkApplicationInhibitFlags flags;
  GtkWidget *toplevel;
  guint cookie;

  active = gtk_toggle_button_get_active (button);
  reason = gtk_editable_get_text (GTK_EDITABLE (inhibit_entry));

  flags = 0;
  if (gtk_check_button_get_active (GTK_CHECK_BUTTON (inhibit_logout)))
    flags |= GTK_APPLICATION_INHIBIT_LOGOUT;
  if (gtk_check_button_get_active (GTK_CHECK_BUTTON (inhibit_switch)))
    flags |= GTK_APPLICATION_INHIBIT_SWITCH;
  if (gtk_check_button_get_active (GTK_CHECK_BUTTON (inhibit_suspend)))
    flags |= GTK_APPLICATION_INHIBIT_SUSPEND;
  if (gtk_check_button_get_active (GTK_CHECK_BUTTON (inhibit_idle)))
    flags |= GTK_APPLICATION_INHIBIT_IDLE;

  toplevel = GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (button)));

  if (active)
    {
      char *text;

      g_print ("Calling gtk_application_inhibit: %d, '%s'\n", flags, reason);

      cookie = gtk_application_inhibit (app, GTK_WINDOW (toplevel), flags, reason);
      g_object_set_data (G_OBJECT (button), "cookie", GUINT_TO_POINTER (cookie));
      if (cookie == 0)
        {
          g_signal_handlers_block_by_func (button, inhibitor_toggled, app);
          gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), FALSE);
          g_signal_handlers_unblock_by_func (button, inhibitor_toggled, app);
          active = FALSE;
        }
      else
        {
          text = g_strdup_printf ("%#x", cookie);
          gtk_label_set_label (GTK_LABEL (inhibit_label), text);
          g_free (text);
        }
    }
  else
    {
      cookie = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (button), "cookie"));
      g_print ("Calling gtk_application_uninhibit: %#x\n", cookie);
      gtk_application_uninhibit (app, cookie);
      gtk_label_set_label (GTK_LABEL (inhibit_label), "");
    }

  gtk_widget_set_sensitive (inhibit_entry, !active);
  gtk_widget_set_sensitive (inhibit_logout, !active);
  gtk_widget_set_sensitive (inhibit_switch, !active);
  gtk_widget_set_sensitive (inhibit_suspend, !active);
  gtk_widget_set_sensitive (inhibit_idle, !active);
}

static void
activate (GtkApplication *app,
          gpointer        data)
{
  GtkWidget *box;
  GtkWidget *separator;
  GtkWidget *grid;
  GtkWidget *button;
  GtkWidget *label;

  win = gtk_window_new ();

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
  gtk_widget_set_margin_start (box, 12);
  gtk_widget_set_margin_end (box, 12);
  gtk_widget_set_margin_top (box, 12);
  gtk_widget_set_margin_bottom (box, 12);
  gtk_window_set_child (GTK_WINDOW (win), box);

  grid = gtk_grid_new ();
  gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 6);

  gtk_box_append (GTK_BOX (box), grid);

  label = gtk_label_new ("Inhibitor");
  gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);

  inhibit_label = gtk_label_new ("");
  gtk_grid_attach (GTK_GRID (grid), inhibit_label, 1, 0, 1, 1);

  inhibit_logout = gtk_check_button_new_with_label ("Logout");
  gtk_grid_attach (GTK_GRID (grid), inhibit_logout, 1, 1, 1, 1);

  inhibit_switch = gtk_check_button_new_with_label ("User switching");
  gtk_grid_attach (GTK_GRID (grid), inhibit_switch, 1, 2, 1, 1);

  inhibit_suspend = gtk_check_button_new_with_label ("Suspend");
  gtk_grid_attach (GTK_GRID (grid), inhibit_suspend, 1, 4, 1, 1);

  inhibit_idle = gtk_check_button_new_with_label ("Idle");
  gtk_grid_attach (GTK_GRID (grid), inhibit_idle, 1, 5, 1, 1);

  inhibit_entry = gtk_entry_new ();
  gtk_grid_attach (GTK_GRID (grid), inhibit_entry, 1, 6, 1, 1);

  button = gtk_toggle_button_new_with_label ("Inhibit");
  g_signal_connect (button, "toggled",
                    G_CALLBACK (inhibitor_toggled), app);
  gtk_grid_attach (GTK_GRID (grid), button, 2, 6, 1, 1);

  separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_box_append (GTK_BOX (box), separator);

  grid = gtk_grid_new ();
  gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 6);

  gtk_window_present (GTK_WINDOW (win));

  gtk_application_add_window (app, GTK_WINDOW (win));
}

int
main (int argc, char *argv[])
{
  GtkApplication *app;

  app = gtk_application_new ("org.gtk.Test.session", 0);
  g_object_set (app, "register-session", TRUE, NULL);

  g_signal_connect (app, "activate",
                    G_CALLBACK (activate), NULL);

  g_application_run (G_APPLICATION (app), argc, argv);

  return 0;
}
