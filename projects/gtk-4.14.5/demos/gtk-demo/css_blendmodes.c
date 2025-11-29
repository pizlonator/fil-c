/* Theming/CSS Blend Modes
 *
 * You can blend multiple backgrounds using the CSS blend modes available.
 */

#include <gtk/gtk.h>

#define WID(x) ((GtkWidget*) gtk_builder_get_object (builder, x))

/*
 * These are the available blend modes.
 */
struct {
  const char *name;
  const char *id;
} blend_modes[] =
{
  { "Color", "color" },
  { "Color (burn)", "color-burn" },
  { "Color (dodge)", "color-dodge" },
  { "Darken", "darken" },
  { "Difference", "difference" },
  { "Exclusion", "exclusion" },
  { "Hard Light", "hard-light" },
  { "Hue", "hue" },
  { "Lighten", "lighten" },
  { "Luminosity", "luminosity" },
  { "Multiply", "multiply" },
  { "Normal", "normal" },
  { "Overlay", "overlay" },
  { "Saturate", "saturation" },
  { "Screen", "screen" },
  { "Soft Light", "soft-light" },
  { NULL }
};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
static void
update_css_for_blend_mode (GtkCssProvider *provider,
                           const char     *blend_mode)
{
  GBytes *bytes;
  char *css;

  bytes = g_resources_lookup_data ("/css_blendmodes/css_blendmodes.css", 0, NULL);

  css = g_strdup_printf ((char *) g_bytes_get_data (bytes, NULL),
                         blend_mode,
                         blend_mode,
                         blend_mode);

  gtk_css_provider_load_from_string (provider, css);

  g_bytes_unref (bytes);
  g_free (css);
}
#pragma GCC diagnostic pop

static void
row_activated (GtkListBox     *listbox,
               GtkListBoxRow  *row,
               GtkCssProvider *provider)
{
  const char *blend_mode;

  blend_mode = blend_modes[gtk_list_box_row_get_index (row)].id;

  update_css_for_blend_mode (provider, blend_mode);
}

static void
setup_listbox (GtkBuilder       *builder,
               GtkStyleProvider *provider)
{
  GtkWidget *normal_row;
  GtkWidget *listbox;
  int i;

  normal_row = NULL;
  listbox = gtk_list_box_new ();
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (WID ("scrolledwindow")), listbox);

  g_signal_connect (listbox, "row-activated", G_CALLBACK (row_activated), provider);

  /* Add a row for each blend mode available */
  for (i = 0; blend_modes[i].name != NULL; i++)
    {
      GtkWidget *label;
      GtkWidget *row;

      row = gtk_list_box_row_new ();
      label = g_object_new (GTK_TYPE_LABEL,
                            "label", blend_modes[i].name,
                            "xalign", 0.0,
                            NULL);

      gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), label);
      gtk_list_box_insert (GTK_LIST_BOX (listbox), row, -1);

      /* The first selected row is "normal" */
      if (g_strcmp0 (blend_modes[i].id, "normal") == 0)
        normal_row = row;
    }

  /* Select the "normal" row */
  gtk_list_box_select_row (GTK_LIST_BOX (listbox), GTK_LIST_BOX_ROW (normal_row));
  g_signal_emit_by_name (G_OBJECT (normal_row), "activate");

  gtk_widget_grab_focus (normal_row);
}

GtkWidget *
do_css_blendmodes (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkStyleProvider *provider;
      GtkBuilder *builder;

      builder = gtk_builder_new_from_resource ("/css_blendmodes/blendmodes.ui");

      window = WID ("window");
      gtk_window_set_transient_for (GTK_WINDOW (window), GTK_WINDOW (do_widget));
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      /* Setup the CSS provider for window */
      provider = GTK_STYLE_PROVIDER (gtk_css_provider_new ());

      gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                                  provider,
                                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

      setup_listbox (builder, provider);

      g_object_unref (builder);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_set_visible (window, TRUE);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
