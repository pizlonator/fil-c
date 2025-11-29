/* testfontchooserdialog.c
 * Copyright (C) 2011 Alberto Ruiz <aruiz@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <string.h>
#ifdef HAVE_PANGOFT
#include <pango/pangofc-fontmap.h>
#endif
#include <gtk/gtk.h>


G_GNUC_BEGIN_IGNORE_DEPRECATIONS

static gboolean
monospace_filter (const PangoFontFamily *family,
                  const PangoFontFace   *face,
                  gpointer               data)
{
  return pango_font_family_is_monospace ((PangoFontFamily *) family);
}

static void
notify_font_cb (GtkFontChooser *fontchooser, GParamSpec *pspec, gpointer data)
{
  PangoFontFamily *family;
  PangoFontFace *face;

  g_debug ("Changed font name %s", gtk_font_chooser_get_font (fontchooser));

  family = gtk_font_chooser_get_font_family (fontchooser);
  face = gtk_font_chooser_get_font_face (fontchooser);
  if (family)
    {
       g_debug ("  Family: %s is-monospace:%s",
                pango_font_family_get_name (family),
                pango_font_family_is_monospace (family) ? "true" : "false");
    }
  else
    g_debug ("  No font family!");

  if (face)
    g_debug ("  Face description: %s", pango_font_face_get_face_name (face));
  else
    g_debug ("  No font face!");
}

static void
notify_preview_text_cb (GObject *fontchooser, GParamSpec *pspec, gpointer data)
{
  g_debug ("Changed preview text %s", gtk_font_chooser_get_preview_text (GTK_FONT_CHOOSER (fontchooser)));
}

static void
font_activated_cb (GtkFontChooser *chooser, const char *font_name, gpointer data)
{
  g_debug ("font-activated: %s", font_name);
}

static void
quit_cb (GtkWidget *widget,
         gpointer   data)
{
  gboolean *done = data;

  *done = TRUE;

  g_main_context_wakeup (NULL);
}

static void
level_changed (GtkCheckButton *button,
               GParamSpec     *pspec,
               gpointer        data)
{
  GtkFontChooser *chooser = data;
  GtkFontChooserLevel flags;
  GtkFontChooserLevel flag;

  flags = gtk_font_chooser_get_level (chooser);
  flag = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (button), "flag"));

  if (gtk_check_button_get_active (button))
    flags |= flag;
  else
    flags &= ~flag;

  gtk_font_chooser_set_level (chooser, flags);
}

int
main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *font_button;
  GtkWidget *box;
  GtkWidget *toggle;
  gboolean done = FALSE;

  gtk_init ();

  font_button = gtk_font_button_new ();

#ifdef HAVE_PANGOFT
  if (argc > 0)
    {
      FcConfig *config;
      PangoFontMap *fontmap;
      int i;

      /* Create a custom font configuration by adding font files specified
       * on the commandline to the default config.
       */
      config = FcInitLoadConfigAndFonts ();
      for (i = 0; i < argc; i++)
        FcConfigAppFontAddFile (config, (const FcChar8 *)argv[i]);

      fontmap = pango_cairo_font_map_new_for_font_type (CAIRO_FONT_TYPE_FT);
      pango_fc_font_map_set_config (PANGO_FC_FONT_MAP (fontmap), config);
      gtk_font_chooser_set_font_map (GTK_FONT_CHOOSER (font_button), fontmap);
    }
#endif

  gtk_font_button_set_use_font (GTK_FONT_BUTTON (font_button), TRUE);

  window = gtk_window_new ();
  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);

  gtk_window_set_child (GTK_WINDOW (window), box);

  gtk_box_append (GTK_BOX (box), font_button);

  toggle = gtk_check_button_new_with_label ("Style");
  gtk_check_button_set_active (GTK_CHECK_BUTTON (toggle), TRUE);
  g_object_set_data (G_OBJECT (toggle), "flag", GUINT_TO_POINTER (GTK_FONT_CHOOSER_LEVEL_STYLE));
  g_signal_connect (toggle, "notify::active", G_CALLBACK (level_changed), font_button);
  gtk_box_append (GTK_BOX (box), toggle);
  toggle = gtk_check_button_new_with_label ("Size");
  gtk_check_button_set_active (GTK_CHECK_BUTTON (toggle), TRUE);
  g_object_set_data (G_OBJECT (toggle), "flag", GUINT_TO_POINTER (GTK_FONT_CHOOSER_LEVEL_SIZE));
  g_signal_connect (toggle, "notify::active", G_CALLBACK (level_changed), font_button);
  gtk_box_append (GTK_BOX (box), toggle);
  toggle = gtk_check_button_new_with_label ("Variations");
  g_object_set_data (G_OBJECT (toggle), "flag", GUINT_TO_POINTER (GTK_FONT_CHOOSER_LEVEL_VARIATIONS));
  g_signal_connect (toggle, "notify::active", G_CALLBACK (level_changed), font_button);
  gtk_box_append (GTK_BOX (box), toggle);
  toggle = gtk_check_button_new_with_label ("Features");
  g_object_set_data (G_OBJECT (toggle), "flag", GUINT_TO_POINTER (GTK_FONT_CHOOSER_LEVEL_FEATURES));
  g_signal_connect (toggle, "notify::active", G_CALLBACK (level_changed), font_button);
  gtk_box_append (GTK_BOX (box), toggle);

  gtk_window_present (GTK_WINDOW (window));

  g_signal_connect (font_button, "notify::font",
                    G_CALLBACK (notify_font_cb), NULL);
  g_signal_connect (font_button, "notify::preview-text",
                    G_CALLBACK (notify_preview_text_cb), NULL);
  g_signal_connect (font_button, "font-activated",
                    G_CALLBACK (font_activated_cb), NULL);

  if (argc >= 2 && strcmp (argv[1], "--monospace") == 0)
    {
      gtk_font_chooser_set_filter_func (GTK_FONT_CHOOSER (font_button),
                                        monospace_filter, NULL, NULL);
    }

  g_signal_connect (window, "destroy", G_CALLBACK (quit_cb), &done);

  while (!done)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
