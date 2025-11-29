#include <gtk/gtk.h>
#include "gtk/gtkmodelbuttonprivate.h"

static void
on_action_beep (GSimpleAction *action,
                GVariant      *parameter,
                void          *user_data)
{
  GdkDisplay *display = gdk_display_get_default ();
  gdk_display_beep (display);
}

static void
on_application_activate (GApplication *gapplication,
                         void         *user_data)
{
  GtkApplication *application = GTK_APPLICATION (gapplication);
  GtkCssProvider *css_provider = gtk_css_provider_new ();
  GdkDisplay *display = gdk_display_get_default ();

  GSimpleAction *action;
  GtkWidget *box;
  GIcon *gicon;
  GtkWidget *model_button;
  GtkWidget *widget;

  gtk_css_provider_load_from_data (css_provider,
    "window > box { padding: 0.5em; }"
    "window > box > * { margin: 0.5em; }"
    /* :iconic == FALSE */
    "modelbutton > check { background: red; }"
    "modelbutton > radio { background: green; }"
    "modelbutton > arrow { background: blue; }"
    /* :iconic == TRUE */
    "button.model { background: yellow; }"
    , -1);
  g_assert (GDK_IS_DISPLAY (display));
  gtk_style_context_add_provider_for_display (display,
                                              GTK_STYLE_PROVIDER (css_provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  action = g_simple_action_new ("beep", NULL);
  g_signal_connect (action, "activate", G_CALLBACK (on_action_beep), NULL);
  g_action_map_add_action (G_ACTION_MAP (application), G_ACTION (action));

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

  gicon = g_themed_icon_new ("face-smile");

  model_button = g_object_new (g_type_from_name ("GtkModelButton"),
                               "action-name", "app.beep",
                               "text", "It’s-a-me! ModelButton",
                               "icon", gicon,
                               NULL);
  gtk_box_append (GTK_BOX (box), model_button);

  g_object_unref (gicon);

  widget = gtk_combo_box_text_new ();
  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (widget),
                             NULL, "GTK_BUTTON_ROLE_NORMAL");
  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (widget),
                             NULL, "GTK_BUTTON_ROLE_CHECK");
  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (widget),
                             NULL, "GTK_BUTTON_ROLE_RADIO");
  gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);
  g_object_bind_property (widget, "active",
                          model_button, "role",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  gtk_box_append (GTK_BOX (box), widget);

  widget = gtk_toggle_button_new_with_label (":iconic");
  g_object_bind_property (widget, "active",
                          model_button, "iconic",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  gtk_box_append (GTK_BOX (box), widget);

  widget = gtk_window_new ();
  gtk_box_append (GTK_BOX (widget), box);
  gtk_window_present (GTK_WINDOW (widget))
  gtk_application_add_window (GTK_APPLICATION (application), GTK_WINDOW (widget));
}

int
main (int   argc,
      char *argv[])
{
  GtkApplication *application = gtk_application_new ("org.gtk.test.modelbutton",
                                                     G_APPLICATION_FLAGS_NONE);
  int result;

  g_signal_connect (application, "activate",
                    G_CALLBACK (on_application_activate), NULL);

  result = g_application_run (G_APPLICATION (application), argc, argv);
  g_object_unref (application);
  return result;
}
