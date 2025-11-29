#include <gtk/gtk.h>

#include "exampleapp.h"
#include "exampleappwin.h"
#include "exampleappprefs.h"

struct _ExampleAppPrefs
{
  GtkDialog parent;

  GSettings *settings;
  GtkWidget *font;
  GtkWidget *transition;
};

G_DEFINE_TYPE (ExampleAppPrefs, example_app_prefs, GTK_TYPE_DIALOG)

static gboolean
string_to_font_desc (GValue   *value,
                     GVariant *variant,
                     gpointer  user_data)
{
  const char *s = g_variant_get_string (variant, NULL);
  PangoFontDescription *desc;

  desc = pango_font_description_from_string (s);
  g_value_take_boxed (value, desc);

  return TRUE;
}

static GVariant *
font_desc_to_string (const GValue       *value,
                     const GVariantType *expected_type,
                     gpointer            user_data)
{
  PangoFontDescription *desc = g_value_get_boxed (value);
  char *s = pango_font_description_to_string (desc);
  return g_variant_new_take_string (s);
}

static gboolean
transition_to_pos (GValue   *value,
                   GVariant *variant,
                   gpointer  user_data)
{
  const char *s = g_variant_get_string (variant, NULL);
  if (strcmp (s, "none") == 0)
    g_value_set_uint (value, 0);
  else if (strcmp (s, "crossfade") == 0)
    g_value_set_uint (value, 1);
  else
    g_value_set_uint (value, 2);

  return TRUE;
}

static GVariant *
pos_to_transition (const GValue       *value,
                   const GVariantType *expected_type,
                   gpointer            user_data)
{
  switch (g_value_get_uint (value))
    {
    case 0: return g_variant_new_string ("none");
    case 1: return g_variant_new_string ("crossfade");
    case 2: return g_variant_new_string ("slide-left-right");
    default: g_assert_not_reached ();
    }
}

static void
example_app_prefs_init (ExampleAppPrefs *prefs)
{
  gtk_widget_init_template (GTK_WIDGET (prefs));
  prefs->settings = g_settings_new ("org.gtk.exampleapp");

  g_settings_bind_with_mapping (prefs->settings, "font",
                                prefs->font, "font-desc",
                                G_SETTINGS_BIND_DEFAULT,
                                string_to_font_desc,
                                font_desc_to_string,
                                NULL, NULL);
  g_settings_bind_with_mapping (prefs->settings, "transition",
                                prefs->transition, "selected",
                                G_SETTINGS_BIND_DEFAULT,
                                transition_to_pos,
                                pos_to_transition,
                                NULL, NULL);
}

static void
example_app_prefs_dispose (GObject *object)
{
  ExampleAppPrefs *prefs;

  prefs = EXAMPLE_APP_PREFS (object);

  g_clear_object (&prefs->settings);

  G_OBJECT_CLASS (example_app_prefs_parent_class)->dispose (object);
}

static void
example_app_prefs_class_init (ExampleAppPrefsClass *class)
{
  G_OBJECT_CLASS (class)->dispose = example_app_prefs_dispose;

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (class),
                                               "/org/gtk/exampleapp/prefs.ui");
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), ExampleAppPrefs, font);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), ExampleAppPrefs, transition);
}

ExampleAppPrefs *
example_app_prefs_new (ExampleAppWindow *win)
{
  return g_object_new (EXAMPLE_APP_PREFS_TYPE, "transient-for", win, "use-header-bar", TRUE, NULL);
}
