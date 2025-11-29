/* Benchmark/Fishbowl
 *
 * This demo models the fishbowl demos seen on the web in a GTK way.
 * It's also a neat little tool to see how fast your computer (or
 * your GTK version) is.
 */

#include <gtk/gtk.h>

#include "gtkfishbowl.h"
#include "gtkgears.h"
#include "gskshaderpaintable.h"

#include "nodewidget.h"
#include "graphwidget.h"

const char *const css =
".blurred-button {"
"  box-shadow: 0px 0px 5px 10px rgba(0, 0, 0, 0.5);"
"}"
"";

char **icon_names = NULL;
gsize n_icon_names = 0;

static void
init_icon_names (GtkIconTheme *theme)
{
  if (icon_names)
    return;

  icon_names = gtk_icon_theme_get_icon_names (theme);
  n_icon_names = g_strv_length (icon_names);
}

static const char *
get_random_icon_name (GtkIconTheme *theme)
{
  init_icon_names (theme);

  return icon_names[g_random_int_range(0, n_icon_names)];
}

/* Can't be static because it's also used in iconscroll.c */
GtkWidget *
create_icon (void)
{
  GtkWidget *image;

  image = gtk_image_new ();
  gtk_image_set_icon_size (GTK_IMAGE (image), GTK_ICON_SIZE_LARGE);
  gtk_image_set_from_icon_name (GTK_IMAGE (image),
                                get_random_icon_name (gtk_icon_theme_get_for_display (gtk_widget_get_display (image))));

  return image;
}

static GtkWidget *
create_button (void)
{
  return gtk_button_new_with_label ("Button");
}

static GtkWidget *
create_blurred_button (void)
{
  GtkWidget *w = gtk_button_new ();

  gtk_widget_add_css_class (w, "blurred-button");

  return w;
}

static GtkWidget *
create_font_button (void)
{
  return gtk_font_dialog_button_new (gtk_font_dialog_new ());
}

static GtkWidget *
create_level_bar (void)
{
  GtkWidget *w = gtk_level_bar_new_for_interval (0, 100);

  gtk_level_bar_set_value (GTK_LEVEL_BAR (w), 50);

  /* Force them to be a bit larger */
  gtk_widget_set_size_request (w, 200, -1);

  return w;
}

static GtkWidget *
create_spinner (void)
{
  GtkWidget *w = gtk_spinner_new ();

  gtk_spinner_start (GTK_SPINNER (w));

  return w;
}

static GtkWidget *
create_spinbutton (void)
{
  GtkWidget *w = gtk_spin_button_new_with_range (0, 10, 1);

  return w;
}

static GtkWidget *
create_label (void)
{
  GtkWidget *w = gtk_label_new ("pLorem ipsum dolor sit amet, consetetur sadipscing elitr, sed diam nonumy eirmod tempor invidunt ut labore et dolore magna aliquyam erat, sed diam voluptua.");

  gtk_label_set_wrap (GTK_LABEL (w), TRUE);
  gtk_label_set_max_width_chars (GTK_LABEL (w), 100);

  return w;
}

static GtkWidget *
create_video (void)
{
  GtkWidget *w = gtk_video_new ();

  gtk_widget_set_size_request (w, 64, 64);
  gtk_video_set_loop (GTK_VIDEO (w), TRUE);
  gtk_video_set_autoplay (GTK_VIDEO (w), TRUE);
  gtk_video_set_resource (GTK_VIDEO (w), "/images/gtk-logo.webm");

  return w;
}

static GtkWidget *
create_gears (void)
{
  GtkWidget *w = gtk_gears_new ();

  gtk_widget_set_size_request (w, 100, 100);

  return w;
}

static GtkWidget *
create_switch (void)
{
  GtkWidget *w = gtk_switch_new ();

  gtk_switch_set_state (GTK_SWITCH (w), TRUE);

  return w;
}

static gboolean
update_paintable (GtkWidget     *widget,
                  GdkFrameClock *frame_clock,
                  gpointer       user_data)
{
  GskShaderPaintable *paintable;
  gint64 frame_time;

  paintable = GSK_SHADER_PAINTABLE (gtk_picture_get_paintable (GTK_PICTURE (widget)));
  frame_time = gdk_frame_clock_get_frame_time (frame_clock);
  gsk_shader_paintable_update_time (paintable, 0, frame_time);

  return G_SOURCE_CONTINUE;
}

static GtkWidget *
create_cogs (void)
{
  GtkWidget *picture;
  static GskGLShader *cog_shader = NULL;
  GdkPaintable *paintable;

 if (cog_shader == NULL)
    cog_shader = gsk_gl_shader_new_from_resource ("/gltransition/cogs2.glsl");
  paintable = gsk_shader_paintable_new (g_object_ref (cog_shader), NULL);
  picture = gtk_picture_new_for_paintable (paintable);
  gtk_widget_set_size_request (picture, 150, 75);
  gtk_widget_add_tick_callback (picture, update_paintable, NULL, NULL);

  return picture;
}

static gboolean
check_cogs (GtkFishbowl *fb)
{
  return GSK_IS_GL_RENDERER (gtk_native_get_renderer (gtk_widget_get_native (GTK_WIDGET (fb))));
}

static void
mapped (GtkWidget *w)
{
  gtk_menu_button_popup (GTK_MENU_BUTTON (w));
}

static GtkWidget *
create_menu_button (void)
{
  GtkWidget *w = gtk_menu_button_new ();
  GtkWidget *popover = gtk_popover_new ();

  gtk_popover_set_child (GTK_POPOVER (popover), gtk_button_new_with_label ("Hey!"));
  gtk_popover_set_autohide (GTK_POPOVER (popover), FALSE);
  gtk_menu_button_set_popover (GTK_MENU_BUTTON (w), popover);
  g_signal_connect (w, "map", G_CALLBACK (mapped), NULL);

  return w;
}

static GtkWidget *
create_tiger (void)
{
  return node_widget_new ("/fishbowl/tiger.node");
}

static GtkWidget *
create_graph (void)
{
  return graph_widget_new ();
}

static const struct {
  const char *name;
  GtkWidget * (* create_func) (void);
  gboolean    (* check) (GtkFishbowl *fb);
} widget_types[] = {
  { "Icon",       create_icon,           NULL },
  { "Button",     create_button,         NULL },
  { "Blurbutton", create_blurred_button, NULL },
  { "Fontbutton", create_font_button,    NULL },
  { "Levelbar",   create_level_bar,      NULL },
  { "Label",      create_label,          NULL },
  { "Spinner",    create_spinner,        NULL },
  { "Spinbutton", create_spinbutton,     NULL },
  { "Video",      create_video,          NULL },
  { "Gears",      create_gears,          NULL },
  { "Switch",     create_switch,         NULL },
  { "Menubutton", create_menu_button,    NULL },
  { "Shader",     create_cogs,           check_cogs },
  { "Tiger",      create_tiger,          NULL },
  { "Graph",      create_graph,          NULL },
};

static int selected_widget_type = -1;
static const int N_WIDGET_TYPES = G_N_ELEMENTS (widget_types);

static gboolean
set_widget_type (GtkFishbowl *fishbowl,
                 int          widget_type_index)
{
  GtkWidget *window;

  if (widget_type_index == selected_widget_type)
    return TRUE;

  if (widget_types[widget_type_index].check != NULL &&
      !widget_types[widget_type_index].check (fishbowl))
    return FALSE;

  selected_widget_type = widget_type_index;

  gtk_fishbowl_set_creation_func (fishbowl,
                                  widget_types[selected_widget_type].create_func);

  window = GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (fishbowl)));
  gtk_window_set_title (GTK_WINDOW (window),
                        widget_types[selected_widget_type].name);

  return TRUE;
}

G_MODULE_EXPORT void
fishbowl_next_button_clicked_cb (GtkButton *source,
                                 gpointer   user_data)
{
  GtkFishbowl *fishbowl = user_data;
  int new_index = selected_widget_type;

  do
    {
      if (new_index + 1 >= N_WIDGET_TYPES)
        new_index = 0;
      else
        new_index = new_index + 1;

    }
  while (!set_widget_type (fishbowl, new_index));
}

G_MODULE_EXPORT void
fishbowl_prev_button_clicked_cb (GtkButton *source,
                                 gpointer   user_data)
{
  GtkFishbowl *fishbowl = user_data;
  int new_index = selected_widget_type;

  do
    {
      if (new_index - 1 < 0)
        new_index = N_WIDGET_TYPES - 1;
      else
        new_index = new_index - 1;

    }

  while (!set_widget_type (fishbowl, new_index));
}

G_MODULE_EXPORT void
fishbowl_changes_toggled_cb (GtkToggleButton *button,
                             gpointer         user_data)
{
  if (gtk_toggle_button_get_active (button))
    gtk_button_set_icon_name (GTK_BUTTON (button), "changes-prevent");
  else
    gtk_button_set_icon_name (GTK_BUTTON (button), "changes-allow");
}

G_MODULE_EXPORT char *
format_header_cb (GObject *object,
                  guint    count,
                  double   fps)
{
  return g_strdup_printf ("%u Icons, %.2f fps", count, fps);
}

GtkWidget *
do_fishbowl (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;
  static GtkCssProvider *provider = NULL;

  if (provider == NULL)
    {
      provider = gtk_css_provider_new ();
      gtk_css_provider_load_from_string (provider, css);
      gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                                  GTK_STYLE_PROVIDER (provider),
                                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }

  if (!window)
    {
      GtkBuilder *builder;
      GtkBuilderScope *scope;
      GtkWidget *bowl;

      g_type_ensure (GTK_TYPE_FISHBOWL);

      scope = gtk_builder_cscope_new ();
      gtk_builder_cscope_add_callback (GTK_BUILDER_CSCOPE (scope), fishbowl_prev_button_clicked_cb);
      gtk_builder_cscope_add_callback (GTK_BUILDER_CSCOPE (scope), fishbowl_next_button_clicked_cb);
      gtk_builder_cscope_add_callback (GTK_BUILDER_CSCOPE (scope), fishbowl_changes_toggled_cb);
      gtk_builder_cscope_add_callback (GTK_BUILDER_CSCOPE (scope), format_header_cb);

      builder = gtk_builder_new ();
      gtk_builder_set_scope (builder, scope);
      gtk_builder_add_from_resource (builder, "/fishbowl/fishbowl.ui", NULL);
      window = GTK_WIDGET (gtk_builder_get_object (builder, "window"));
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      bowl = GTK_WIDGET (gtk_builder_get_object (builder, "bowl"));
      selected_widget_type = -1;
      set_widget_type (GTK_FISHBOWL (bowl), 0);
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));

      gtk_widget_realize (window);
      g_object_unref (builder);
      g_object_unref (scope);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_set_visible (window, TRUE);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
