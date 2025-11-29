#include <gtk/gtk.h>

static void
update_offset (GObject    *object,
               GParamSpec *pspec,
               GtkWidget  *widget)
{
  if (gtk_check_button_get_active (GTK_CHECK_BUTTON (object)))
    gtk_popover_set_offset (GTK_POPOVER (widget), 12, 12);
  else
    gtk_popover_set_offset (GTK_POPOVER (widget), 0, 0);
}

static void
update_shadow (GObject    *object,
               GParamSpec *pspec,
               GtkWidget  *widget)
{
  const char *classes[] = {
    "no-shadow",
    "shadow1",
    "shadow2",
    "shadow3",
    "shadow4",
  };
  guint selected;

  selected = gtk_drop_down_get_selected (GTK_DROP_DOWN (object));
  g_assert (selected < G_N_ELEMENTS (classes));

  for (int i = 0; i < G_N_ELEMENTS (classes); i++)
    gtk_widget_remove_css_class (widget, classes[i]);

  gtk_widget_add_css_class (widget, classes[selected]);
}

static const char css[] =
 "popover.no-shadow > contents { box-shadow: none; }\n"
 "popover.shadow1 > contents { box-shadow: 6px 6px rgba(128,0,255,0.5); }\n"
 "popover.shadow2 > contents { box-shadow: -6px -6px rgba(255,0,0,0.5), 6px 6px rgba(128,0,255,0.5); }\n"
 "popover.shadow3 > contents { box-shadow: -6px -6px rgba(255,0,0,0.5), 18px 18px rgba(128,0,255,0.5); }\n"
 "popover.shadow4 > contents { box-shadow: -18px -18px rgba(255,0,0,0.5), 18px 18px rgba(128,0,255,0.5); }\n";

int
main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *box;
  GtkWidget *button;
  GtkWidget *popover;
  GtkWidget *box2;
  GtkWidget *box3;
  GtkWidget *checkbox;
  GtkWidget *checkbox2;
  GtkWidget *dropdown;
  GtkWidget *dropdown2;
  GtkCssProvider *provider;

  gtk_init ();

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_string (provider, css);

  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (provider),
                                              800);

  window = gtk_window_new ();
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  g_object_set (box,
                "margin-top", 20,
                "margin-bottom", 20,
                "margin-start", 20,
                "margin-end", 20,
                NULL);

  button = gtk_menu_button_new ();

  gtk_widget_set_halign (button, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (button, GTK_ALIGN_CENTER);

  gtk_box_append (GTK_BOX (box), button);

  popover = gtk_popover_new ();
  box2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
  gtk_popover_set_child (GTK_POPOVER (popover), box2);

  gtk_box_append (GTK_BOX (box2), gtk_label_new ("First item"));
  gtk_box_append (GTK_BOX (box2), gtk_label_new ("Second item"));
  gtk_box_append (GTK_BOX (box2), gtk_label_new ("Third item"));

  gtk_menu_button_set_popover (GTK_MENU_BUTTON (button), popover);

  box3 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
  dropdown = gtk_drop_down_new_from_strings ((const char*[]){ "Left", "Right", "Top", "Bottom", NULL });
  gtk_drop_down_set_selected (GTK_DROP_DOWN (dropdown), 3);

  checkbox = gtk_check_button_new_with_label ("Arrow");

  checkbox2 = gtk_check_button_new_with_label ("Offset");

  dropdown2 = gtk_drop_down_new_from_strings ((const char*[]){ "No Shadow", "Shadow 1", "Shadow 2", "Shadow 3", "Shadow 4", NULL });

  gtk_box_append (GTK_BOX (box3), checkbox);
  gtk_box_append (GTK_BOX (box3), checkbox2);
  gtk_box_append (GTK_BOX (box3), dropdown);
  gtk_box_append (GTK_BOX (box3), dropdown2);

  gtk_box_append (GTK_BOX (box), box3);

  g_object_bind_property (checkbox, "active",
                          popover, "has-arrow",
                          G_BINDING_SYNC_CREATE);
  g_signal_connect (checkbox2, "notify::active",
                    G_CALLBACK (update_offset), popover);
  g_object_bind_property (dropdown, "selected",
                          popover, "position",
                          G_BINDING_SYNC_CREATE);
  g_signal_connect (dropdown2, "notify::selected",
                    G_CALLBACK (update_shadow), popover);
  update_shadow (G_OBJECT (dropdown2), NULL, popover);

  gtk_window_set_child (GTK_WINDOW (window), box);

  gtk_window_present (GTK_WINDOW (window));

  while (g_list_model_get_n_items (gtk_window_get_toplevels ()) > 0)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
