
#include<gtk/gtk.h>

static const char *css =
".one {"
"  all: unset;"
"  min-width: 100px;"
"  min-height:100px;"
"   border-left:   50px solid #0f0;"
"   border-top:    10px solid red;"
"   border-bottom: 50px solid teal;"
"   border-right:  100px solid pink;"
"   border-radius: 100px;"
"}"
".two {"
"  all: unset;"
"  min-width: 100px;"
"  min-height:100px;"
"   border-left:   50px solid #0f0;"
"   border-top:    10px solid red;"
"   border-bottom: 50px solid teal;"
"   border-right:  100px solid pink;"
"   border-radius: 50%;"
"}"
".three {"
"  all: unset;"
"  min-width: 100px;"
"  min-height:100px;"
"   border-left:   50px solid #0f0;"
"   border-top:    10px solid red;"
"   border-bottom: 50px solid teal;"
"   border-right:  100px solid pink;"
"   border-radius: 0px;"
"}"
".four {"
"  all: unset;"
"  min-width: 100px;"
"  min-height:100px;"
"  border: 10px solid black;"
"  border-radius: 999px;"
"}"
".five {"
"  all: unset;"
"  min-width: 100px;"
"  min-height:100px;"
"  border: 30px solid black;"
"  border-radius: 0px;"
"}"
".b1 {"
"  all: unset;"
"  min-width: 100px;"
"  min-height:100px;"
"  border-top: 30px solid black;"
"  border-radius: 0px;"
"}"
".b2 {"
"  all: unset;"
"  min-width: 100px;"
"  min-height:100px;"
"  border-bottom: 30px solid black;"
"  border-radius: 0px;"
"}"
".b3 {"
"  all: unset;"
"  min-width: 100px;"
"  min-height:100px;"
"  border-right: 30px solid blue;"
"  border-radius: 40px;"
"}"
".b4 {"
"  all: unset;"
"  min-width: 100px;"
"  min-height:100px;"
"  border-bottom: 30px solid blue;"
"  border-radius: 40px;"
"}"
;

static void
quit_cb (GtkWidget *widget,
         gpointer   data)
{
  gboolean *done = data;

  *done = TRUE;

  g_main_context_wakeup (NULL);
}

int
main (int argc, char **argv)
{
  GtkWidget *window;
  GtkWidget *box;
  GtkWidget *top;
  GtkWidget *bottom;
  GtkWidget *w;
  GtkCssProvider *provider;
  gboolean done = FALSE;

  gtk_init ();

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_string (provider, css);
  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  window = gtk_window_new ();
  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 40);
  top = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 40);
  bottom = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 40);
  gtk_widget_set_margin_start (box, 40);
  gtk_widget_set_margin_end (box, 40);
  gtk_widget_set_margin_top (box, 40);
  gtk_widget_set_margin_bottom (box, 40);

  w = gtk_button_new ();
  gtk_widget_set_valign (w, GTK_ALIGN_CENTER);
  gtk_widget_add_css_class (w, "one");
  gtk_box_append (GTK_BOX (top), w);

  w = gtk_button_new ();
  gtk_widget_set_valign (w, GTK_ALIGN_CENTER);
  gtk_widget_add_css_class (w, "two");
  gtk_box_append (GTK_BOX (top), w);

  w = gtk_button_new ();
  gtk_widget_set_valign (w, GTK_ALIGN_CENTER);
  gtk_widget_add_css_class (w, "three");
  gtk_box_append (GTK_BOX (top), w);

  w = gtk_button_new ();
  gtk_widget_set_valign (w, GTK_ALIGN_CENTER);
  gtk_widget_add_css_class (w, "four");
  gtk_box_append (GTK_BOX (top), w);

  w = gtk_button_new ();
  gtk_widget_set_valign (w, GTK_ALIGN_CENTER);
  gtk_widget_add_css_class (w, "five");
  gtk_box_append (GTK_BOX (top), w);


  /* Bottom */
  w = gtk_button_new ();
  gtk_widget_set_valign (w, GTK_ALIGN_CENTER);
  gtk_widget_add_css_class (w, "b1");
  gtk_box_append (GTK_BOX (bottom), w);

  w = gtk_button_new ();
  gtk_widget_set_valign (w, GTK_ALIGN_CENTER);
  gtk_widget_add_css_class (w, "b2");
  gtk_box_append (GTK_BOX (bottom), w);

  w = gtk_button_new ();
  gtk_widget_set_valign (w, GTK_ALIGN_CENTER);
  gtk_widget_add_css_class (w, "b3");
  gtk_box_append (GTK_BOX (bottom), w);

  w = gtk_button_new ();
  gtk_widget_set_valign (w, GTK_ALIGN_CENTER);
  gtk_widget_add_css_class (w, "b4");
  gtk_box_append (GTK_BOX (bottom), w);

  gtk_box_append (GTK_BOX (box), top);
  gtk_box_append (GTK_BOX (box), bottom);
  gtk_window_set_child (GTK_WINDOW (window), box);
  g_signal_connect (window, "destroy", G_CALLBACK (quit_cb), &done);
  gtk_window_present (GTK_WINDOW (window));

  while (!done)
    g_main_context_iteration (NULL, TRUE);
}
