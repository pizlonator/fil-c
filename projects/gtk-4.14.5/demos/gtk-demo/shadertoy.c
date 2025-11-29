/* OpenGL/Shadertoy
 * #Keywords: GtkGLArea
 *
 * Generate pixels using a custom fragment shader.
 *
 * The names of the uniforms are compatible with the shaders on shadertoy.com, so
 * many of the shaders there work here too.
 */
#include <math.h>
#include <gtk/gtk.h>
#include <epoxy/gl.h>
#include "gtkshadertoy.h"

static GtkWidget *demo_window = NULL;
static GtkWidget *shadertoy = NULL;
static GtkTextBuffer *textbuffer = NULL;

static void
run (void)
{
  GtkTextIter start, end;
  char *text;

  gtk_text_buffer_get_bounds (textbuffer, &start, &end);
  text = gtk_text_buffer_get_text (textbuffer, &start, &end, FALSE);

  gtk_shadertoy_set_image_shader (GTK_SHADERTOY (shadertoy), text);
  g_free (text);
}

static void
run_clicked_cb (GtkWidget *button,
                gpointer   user_data)
{
  run ();
}

static void
load_clicked_cb (GtkWidget *button,
                 gpointer   user_data)
{
  const char *path = user_data;
  GBytes *initial_shader;

  initial_shader = g_resources_lookup_data (path, 0, NULL);
  gtk_text_buffer_set_text (textbuffer, g_bytes_get_data (initial_shader, NULL), -1);
  g_bytes_unref (initial_shader);

  run ();
}

static void
clear_clicked_cb (GtkWidget *button,
                  gpointer   user_data)
{
  gtk_text_buffer_set_text (textbuffer, "", 0);
}

static void
close_window (GtkWidget *widget)
{
  /* Reset the state */
  demo_window = NULL;
  shadertoy = NULL;
  textbuffer = NULL;
}

static GtkWidget *
new_shadertoy (const char *path)
{
  GBytes *shader;
  GtkWidget *toy;

  toy = gtk_shadertoy_new ();
  shader = g_resources_lookup_data (path, 0, NULL);
  gtk_shadertoy_set_image_shader (GTK_SHADERTOY (toy),
                                  g_bytes_get_data (shader, NULL));
  g_bytes_unref (shader);

  return toy;
}

static GtkWidget *
new_button (const char *path)
{
  GtkWidget *button, *toy;

  button = gtk_button_new ();
  g_signal_connect (button, "clicked", G_CALLBACK (load_clicked_cb), (char *)path);

  toy = new_shadertoy (path);
  gtk_widget_set_size_request (toy, 64, 36);
  gtk_button_set_child (GTK_BUTTON (button),  toy);

  return button;
}

static GtkWidget *
create_shadertoy_window (GtkWidget *do_widget)
{
  GtkWidget *window, *box, *hbox, *button, *textview, *sw, *aspect, *centerbox;

  window = gtk_window_new ();
  gtk_window_set_display (GTK_WINDOW (window),  gtk_widget_get_display (do_widget));
  gtk_window_set_title (GTK_WINDOW (window), "Shadertoy");
  gtk_window_set_default_size (GTK_WINDOW (window), 690, 740);
  g_signal_connect (window, "destroy", G_CALLBACK (close_window), NULL);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, FALSE);
  gtk_widget_set_margin_start (box, 12);
  gtk_widget_set_margin_end (box, 12);
  gtk_widget_set_margin_top (box, 12);
  gtk_widget_set_margin_bottom (box, 12);
  gtk_box_set_spacing (GTK_BOX (box), 6);
  gtk_window_set_child (GTK_WINDOW (window), box);

  aspect = gtk_aspect_frame_new (0.5, 0.5, 1.77777, FALSE);
  gtk_widget_set_hexpand (aspect, TRUE);
  gtk_widget_set_vexpand (aspect, TRUE);
  gtk_box_append (GTK_BOX (box), aspect);

  shadertoy = new_shadertoy ("/shadertoy/alienplanet.glsl");
  gtk_aspect_frame_set_child (GTK_ASPECT_FRAME (aspect), shadertoy);

  sw = gtk_scrolled_window_new ();
  gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (sw), 250);
  gtk_scrolled_window_set_has_frame (GTK_SCROLLED_WINDOW (sw), TRUE);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);
  gtk_widget_set_hexpand (sw, TRUE);
  gtk_box_append (GTK_BOX (box), sw);

  textview = gtk_text_view_new ();
  gtk_text_view_set_monospace (GTK_TEXT_VIEW (textview), TRUE);
  g_object_set (textview,
                "left-margin", 20,
                "right-margin", 20,
                "top-margin", 20,
                "bottom-margin", 20,
                NULL);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), textview);

  textbuffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (textview));
  gtk_text_buffer_set_text (textbuffer,
                            gtk_shadertoy_get_image_shader (GTK_SHADERTOY (shadertoy)),
                            -1);

  centerbox = gtk_center_box_new ();
  gtk_box_append (GTK_BOX (box), centerbox);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, FALSE);
  gtk_box_set_spacing (GTK_BOX (hbox), 6);
  gtk_center_box_set_start_widget (GTK_CENTER_BOX (centerbox), hbox);

  button = gtk_button_new_from_icon_name ("view-refresh-symbolic");
  gtk_widget_set_tooltip_text (button, "Restart the demo");
  gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
  g_signal_connect (button, "clicked", G_CALLBACK (run_clicked_cb), NULL);
  gtk_box_append (GTK_BOX (hbox), button);

  button = gtk_button_new_from_icon_name ("edit-clear-all-symbolic");
  gtk_widget_set_tooltip_text (button, "Clear the text view");
  gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
  g_signal_connect (button, "clicked", G_CALLBACK (clear_clicked_cb), NULL);
  gtk_box_append (GTK_BOX (hbox), button);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, FALSE);
  gtk_box_set_spacing (GTK_BOX (hbox), 6);
  gtk_center_box_set_end_widget (GTK_CENTER_BOX (centerbox), hbox);

  button = new_button ("/shadertoy/alienplanet.glsl");
  gtk_box_append (GTK_BOX (hbox), button);

  button = new_button ("/shadertoy/mandelbrot.glsl");
  gtk_box_append (GTK_BOX (hbox), button);

  button = new_button ("/shadertoy/neon.glsl");
  gtk_box_append (GTK_BOX (hbox), button);

  button = new_button ("/shadertoy/cogs.glsl");
  gtk_box_append (GTK_BOX (hbox), button);

  button = new_button ("/shadertoy/glowingstars.glsl");
  gtk_box_append (GTK_BOX (hbox), button);

  return window;
}

GtkWidget *
do_shadertoy (GtkWidget *do_widget)
{
  if (!demo_window)
    demo_window = create_shadertoy_window (do_widget);

  if (!gtk_widget_get_visible (demo_window))
    gtk_widget_set_visible (demo_window, TRUE);
  else
    gtk_window_destroy (GTK_WINDOW (demo_window));

  return demo_window;
}
