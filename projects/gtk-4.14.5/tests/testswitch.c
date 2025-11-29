#include <stdlib.h>
#include <gtk/gtk.h>

static gboolean
boolean_to_text (GBinding *binding,
                 const GValue *source,
                 GValue *target,
                 gpointer dummy G_GNUC_UNUSED)
{
  if (g_value_get_boolean (source))
    g_value_set_string (target, "Enabled");
  else
    g_value_set_string (target, "Disabled");

  return TRUE;
}

static GtkWidget *
make_switch (gboolean is_on,
             gboolean is_sensitive)
{
  GtkWidget *hbox;
  GtkWidget *sw, *label;

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);

  sw = gtk_switch_new ();
  gtk_switch_set_active (GTK_SWITCH (sw), is_on);
  gtk_box_append (GTK_BOX (hbox), sw);
  gtk_widget_set_sensitive (sw, is_sensitive);

  label = gtk_label_new (is_on ? "Enabled" : "Disabled");
  gtk_widget_set_hexpand (label, TRUE);
  gtk_box_append (GTK_BOX (hbox), label);

  g_object_bind_property_full (sw, "active",
                               label, "label",
                               G_BINDING_DEFAULT,
                               boolean_to_text,
                               NULL,
                               NULL, NULL);

  return hbox;
}

typedef struct {
  GtkSwitch *sw;
  gboolean state;
} SetStateData;

static gboolean
set_state_delayed (gpointer data)
{
  SetStateData *d = data;

  gtk_switch_set_state (d->sw, d->state);

  g_object_set_data (G_OBJECT (d->sw), "timeout", NULL);

  return G_SOURCE_REMOVE; 
}

static gboolean
set_state (GtkSwitch *sw, gboolean state, gpointer data)
{
  SetStateData *d;
  guint id;

  id = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (sw), "timeout"));

  if (id != 0)
    g_source_remove (id);

  d = g_new (SetStateData, 1);
  d->sw = sw;
  d->state = state;

  id = g_timeout_add_full (G_PRIORITY_DEFAULT, 2000, set_state_delayed, d, g_free);
  g_object_set_data (G_OBJECT (sw), "timeout", GUINT_TO_POINTER (id));

  return TRUE;
}

static void
sw_delay_notify (GObject *obj, GParamSpec *pspec, gpointer data)
{
  GtkWidget *spinner = data;
  gboolean active;
  gboolean state;

  g_object_get (obj,
                "active", &active,
                "state", &state,
                NULL);

  if (active != state)
    {
      gtk_spinner_start (GTK_SPINNER (spinner));
      gtk_widget_set_opacity (spinner, 1.0);
    }
  else
    {
      gtk_widget_set_opacity (spinner, 0.0);
      gtk_spinner_stop (GTK_SPINNER (spinner));
    }
}

static GtkWidget *
make_delayed_switch (gboolean is_on,
                     gboolean is_sensitive)
{
  GtkWidget *hbox;
  GtkWidget *sw, *label, *spinner, *check;

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);

  sw = gtk_switch_new ();
  gtk_switch_set_active (GTK_SWITCH (sw), is_on);
  gtk_box_append (GTK_BOX (hbox), sw);
  gtk_widget_set_sensitive (sw, is_sensitive);

  g_signal_connect (sw, "state-set", G_CALLBACK (set_state), NULL);

  spinner = gtk_spinner_new ();
  gtk_box_append (GTK_BOX (hbox), spinner);
  gtk_widget_set_opacity (spinner, 0.0);

  label = gtk_label_new (is_on ? "Enabled" : "Disabled");
  gtk_widget_set_hexpand (label, TRUE);
  gtk_box_append (GTK_BOX (hbox), label);

  g_object_bind_property_full (sw, "active",
                               label, "label",
                               G_BINDING_DEFAULT,
                               boolean_to_text,
                               NULL,
                               NULL, NULL);

  check = gtk_check_button_new ();
  gtk_box_append (GTK_BOX (hbox), check);
  g_object_bind_property (sw, "state",
                          check, "active",
                          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);

  g_signal_connect (sw, "notify", G_CALLBACK (sw_delay_notify), spinner);

  return hbox;
}

static void
quit_cb (GtkWidget *widget,
         gpointer   data)
{
  gboolean *done = data;

  *done = TRUE;

  g_main_context_wakeup (NULL);
}

int main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *vbox, *hbox;
  gboolean done = FALSE;

  gtk_init ();

  window = gtk_window_new ();
  gtk_window_set_title (GTK_WINDOW (window), "GtkSwitch");
  gtk_window_set_default_size (GTK_WINDOW (window), 400, -1);
  g_signal_connect (window, "destroy", G_CALLBACK (quit_cb), &done);
  gtk_window_present (GTK_WINDOW (window));

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
  gtk_window_set_child (GTK_WINDOW (window), vbox);

  hbox = make_switch (FALSE, TRUE);
  gtk_box_append (GTK_BOX (vbox), hbox);

  hbox = make_switch (TRUE, TRUE);
  gtk_box_append (GTK_BOX (vbox), hbox);

  hbox = make_switch (FALSE, FALSE);
  gtk_box_append (GTK_BOX (vbox), hbox);

  hbox = make_switch (TRUE, FALSE);
  gtk_box_append (GTK_BOX (vbox), hbox);

  hbox = make_delayed_switch (FALSE, TRUE);
  gtk_box_append (GTK_BOX (vbox), hbox);

  while (!done)
    g_main_context_iteration (NULL, TRUE);

  return EXIT_SUCCESS;
}
