#include <gtk/gtk.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

typedef enum {
  RESPONSE_UNREVEAL,
} Response;

static void
on_info_bar_response (GtkInfoBar *info_bar,
                      int         response_id,
                      void       *user_data)
{
  switch (response_id)
  {
  case GTK_RESPONSE_CLOSE:
    gtk_widget_set_visible (GTK_WIDGET (info_bar), FALSE);
    break;

  case RESPONSE_UNREVEAL:
    gtk_info_bar_set_revealed (info_bar, FALSE);
    break;

  default:
    g_assert_not_reached ();
  }
}

static void
on_activate (GApplication *application,
             void         *user_data)
{
  GtkWidget *box;
  GtkWidget *info_bar;
  GtkWidget *widget;

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);

  info_bar = gtk_info_bar_new ();
  gtk_info_bar_add_child (GTK_INFO_BAR (info_bar),
                          gtk_label_new ("Hello!\nI am a GtkInfoBar"));

  widget = gtk_toggle_button_new_with_label ("Toggle :visible");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
  g_object_bind_property (widget, "active",
                          info_bar, "visible",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  gtk_box_append (GTK_BOX (box), widget);

  widget = gtk_toggle_button_new_with_label ("Toggle :revealed");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
  g_object_bind_property (widget, "active",
                          info_bar, "revealed",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  gtk_box_append (GTK_BOX (box), widget);

  widget = gtk_toggle_button_new_with_label ("Toggle :show-close-button");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
  g_object_bind_property (widget, "active",
                          info_bar, "show-close-button",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  gtk_box_append (GTK_BOX (box), widget);

  widget = gtk_combo_box_text_new ();
  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (widget),
                             NULL, "GTK_MESSAGE_INFO");
  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (widget),
                             NULL, "GTK_MESSAGE_WARNING");
  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (widget),
                             NULL, "GTK_MESSAGE_QUESTION");
  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (widget),
                             NULL, "GTK_MESSAGE_ERROR");
  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (widget),
                             NULL, "GTK_MESSAGE_OTHER");
  gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);
  g_object_bind_property (widget, "active",
                          info_bar, "message-type",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  gtk_box_append (GTK_BOX (box), widget);

  gtk_box_append (GTK_BOX (box), info_bar);

  widget = gtk_button_new_with_label ("Un-reveal");
  gtk_info_bar_add_action_widget (GTK_INFO_BAR (info_bar), widget,
                                  RESPONSE_UNREVEAL);

  g_signal_connect (info_bar, "response",
                    G_CALLBACK (on_info_bar_response), widget);

  widget = gtk_window_new ();
  gtk_window_set_child (GTK_WINDOW (widget), box);
  gtk_widget_show (widget);
  gtk_application_add_window (GTK_APPLICATION (application),
                              GTK_WINDOW (widget));
}

int
main (int   argc,
      char *argv[])
{
  GtkApplication *application;
  int result;

  application = gtk_application_new ("org.gtk.test.infobar",
                                     G_APPLICATION_FLAGS_NONE);
  g_signal_connect (application, "activate", G_CALLBACK (on_activate), NULL);

  result = g_application_run (G_APPLICATION (application), argc, argv);
  g_object_unref (application);
  return result;
}
