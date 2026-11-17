/*
 * Copyright © 2002 Havoc Pennington
 * Copyright © 2002 Mathias Hasselmann
 * Copyright © 2008, 2011, 2017 Christian Persch
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>

#include <string.h>
#include <math.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gio/gio.h>

#include "terminal-app.hh"
#include "terminal-enums.hh"
#include "profile-editor.hh"
#include "terminal-prefs.hh"
#include "terminal-schemas.hh"
#include "terminal-type-builtins.hh"
#include "terminal-util.hh"
#include "terminal-profiles-list.hh"
#include "terminal-libgsystem.hh"


/* Wrapper around g_signal_connect that maintains a list of the
 * handlers installed, and can disconnect them all. */
typedef struct {
  gpointer instance;
  gulong handler_id;
} ProfilePrefsSignal;

static void
profile_prefs_register_signal_handler (gpointer instance,
                                       gulong handler_id)
{
  ProfilePrefsSignal sig;
  sig.instance = instance;
  sig.handler_id = handler_id;
  g_array_append_val (the_pref_data->profile_signals, sig);
}

static gulong
profile_prefs_signal_connect (gpointer instance,
                              const gchar *detailed_signal,
                              GCallback c_handler,
                              gpointer data)
{
  gulong handler_id = g_signal_connect(instance, detailed_signal, c_handler, data);
  profile_prefs_register_signal_handler (instance, handler_id);
  return handler_id;
}

static void
profile_prefs_signal_handlers_disconnect_all (void)
{
  for (guint i = 0; i < the_pref_data->profile_signals->len; i++) {
    ProfilePrefsSignal *sig = &g_array_index (the_pref_data->profile_signals, ProfilePrefsSignal, i);
    g_signal_handler_disconnect (sig->instance, sig->handler_id);
  }
  g_array_set_size (the_pref_data->profile_signals, 0);
}


/* Wrappers around g_settings_bind and friends that maintain a list of the
 * bindings installed, and can unbind them all. */
typedef struct {
  gpointer object;
  char *property;
} ProfilePrefsBinding;

static void
profile_prefs_register_settings_binding (gpointer object,
                                         const char *property)
{
  ProfilePrefsBinding bind;
  bind.object = object;
  bind.property = g_strdup (property);
  g_array_append_val (the_pref_data->profile_bindings, bind);
}

static void
profile_prefs_settings_bind (GSettings *settings,
                             const gchar *key,
                             gpointer object,
                             const gchar *property,
                             GSettingsBindFlags flags)
{
  profile_prefs_register_settings_binding (object, property);
  g_settings_bind (settings, key, object, property, flags);
}

static void
profile_prefs_settings_bind_with_mapping (GSettings *settings,
                                          const gchar *key,
                                          gpointer object,
                                          const gchar *property,
                                          GSettingsBindFlags flags,
                                          GSettingsBindGetMapping get_mapping,
                                          GSettingsBindSetMapping set_mapping,
                                          GType (*user_data)(void),
                                          GDestroyNotify destroy)
{
  profile_prefs_register_settings_binding (object, property);
  g_settings_bind_with_mapping (settings, key, object, property, flags, get_mapping, set_mapping, (void*)user_data, destroy);
}

static void
profile_prefs_settings_bind_writable (GSettings *settings,
                                      const gchar *key,
                                      gpointer object,
                                      const gchar *property,
                                      gboolean inverted)
{
  profile_prefs_register_settings_binding (object, property);
  g_settings_bind_writable (settings, key, object, property, inverted);
}

static void
profile_prefs_settings_unbind_all (void)
{
  for (guint i = 0; i < the_pref_data->profile_bindings->len; i++) {
    ProfilePrefsBinding *bind = &g_array_index (the_pref_data->profile_bindings, ProfilePrefsBinding, i);
    g_settings_unbind (bind->object, bind->property);
    g_free (bind->property);
  }
  g_array_set_size (the_pref_data->profile_bindings, 0);
}


typedef struct _TerminalColorScheme TerminalColorScheme;

struct _TerminalColorScheme
{
  const char *name;
  const GdkRGBA foreground;
  const GdkRGBA background;
};

#define COLOR(r, g, b) { .red = (r) / 255.0, .green = (g) / 255.0, .blue = (b) / 255.0, .alpha = 1.0 }

static const TerminalColorScheme color_schemes[] = {
  { N_("Black on light yellow"),
    COLOR (0x00, 0x00, 0x00),
    COLOR (0xff, 0xff, 0xdd)
  },
  { N_("Black on white"),
    COLOR (0x00, 0x00, 0x00),
    COLOR (0xff, 0xff, 0xff)
  },
  { N_("Gray on black"),
    COLOR (0xaa, 0xaa, 0xaa),
    COLOR (0x00, 0x00, 0x00)
  },
  { N_("Green on black"),
    COLOR (0x00, 0xff, 0x00),
    COLOR (0x00, 0x00, 0x00)
  },
  { N_("White on black"),
    COLOR (0xff, 0xff, 0xff),
    COLOR (0x00, 0x00, 0x00)
  },
  /* Translators: "GNOME" is the name of a colour scheme, "light" can be translated */
  { N_("GNOME light"),
    COLOR (0x17, 0x14, 0x21), /* Palette entry 0 */
    COLOR (0xff, 0xff, 0xff)  /* Palette entry 15 */
  },
  /* Translators: "GNOME" is the name of a colour scheme, "dark" can be translated */
  { N_("GNOME dark"),
    COLOR (0xd0, 0xcf, 0xcc), /* Palette entry 7 */
    COLOR (0x17, 0x14, 0x21)  /* Palette entry 0 */
  },
  /* Translators: "Tango" is the name of a colour scheme, "light" can be translated */
  { N_("Tango light"),
    COLOR (0x2e, 0x34, 0x36),
    COLOR (0xee, 0xee, 0xec)
  },
  /* Translators: "Tango" is the name of a colour scheme, "dark" can be translated */
  { N_("Tango dark"),
    COLOR (0xd3, 0xd7, 0xcf),
    COLOR (0x2e, 0x34, 0x36)
  },
  /* Translators: "Solarized" is the name of a colour scheme, "light" can be translated */
  { N_("Solarized light"),
    COLOR (0x65, 0x7b, 0x83),  /* 11: base00 */
    COLOR (0xfd, 0xf6, 0xe3)   /* 15: base3  */
  },
  /* Translators: "Solarized" is the name of a colour scheme, "dark" can be translated */
  { N_("Solarized dark"),
    COLOR (0x83, 0x94, 0x96),  /* 12: base0  */
    COLOR (0x00, 0x2b, 0x36)   /*  8: base03 */
  },
};

#define TERMINAL_PALETTE_SIZE (16)

enum
{
  TERMINAL_PALETTE_GNOME     = 0,
  TERMINAL_PALETTE_TANGO     = 1,
  TERMINAL_PALETTE_LINUX     = 2,
  TERMINAL_PALETTE_XTERM     = 3,
  TERMINAL_PALETTE_RXVT      = 4,
  TERMINAL_PALETTE_SOLARIZED = 5,
  TERMINAL_PALETTE_N_BUILTINS
};

static const GdkRGBA terminal_palettes[TERMINAL_PALETTE_N_BUILTINS][TERMINAL_PALETTE_SIZE] =
{
  /* Based on GNOME 3.32 palette: https://developer.gnome.org/hig/stable/icon-design.html.en#palette */
  {
    COLOR (0x17, 0x14, 0x21),  /* Blend of Dark 4 and Black */
    COLOR (0xc0, 0x1c, 0x28),  /* Red 4 */
    COLOR (0x26, 0xa2, 0x69),  /* Green 5 */
    COLOR (0xa2, 0x73, 0x4c),  /* Blend of Brown 2 and Brown 3 */
    COLOR (0x12, 0x48, 0x8b),  /* Blend of Blue 5 and Dark 4 */
    COLOR (0xa3, 0x47, 0xba),  /* Purple 3 */
    COLOR (0x2a, 0xa1, 0xb3),  /* Linear addition Blue 5 + Green 5, darkened slightly */
    COLOR (0xd0, 0xcf, 0xcc),  /* Blend of Light 3 and Light 4 */
    COLOR (0x5e, 0x5c, 0x64),  /* Dark 2 */
    COLOR (0xf6, 0x61, 0x51),  /* Red 1 */
    COLOR (0x33, 0xd1, 0x7a),  /* Green 3 */
    COLOR (0xe9, 0xad, 0x0c),  /* Blend of Yellow 4 and Yellow 5 */
    COLOR (0x2a, 0x7b, 0xde),  /* Blend of Blue 3 and Blue 4 */
    COLOR (0xc0, 0x61, 0xcb),  /* Purple 2 */
    COLOR (0x33, 0xc7, 0xde),  /* Linear addition Blue 4 + Green 4, darkened slightly */
    COLOR (0xff, 0xff, 0xff)   /* Light 1 */
  },

  /* Tango palette */
  {
    COLOR (0x2e, 0x34, 0x36),
    COLOR (0xcc, 0x00, 0x00),
    COLOR (0x4e, 0x9a, 0x06),
    COLOR (0xc4, 0xa0, 0x00),
    COLOR (0x34, 0x65, 0xa4),
    COLOR (0x75, 0x50, 0x7b),
    COLOR (0x06, 0x98, 0x9a),
    COLOR (0xd3, 0xd7, 0xcf),
    COLOR (0x55, 0x57, 0x53),
    COLOR (0xef, 0x29, 0x29),
    COLOR (0x8a, 0xe2, 0x34),
    COLOR (0xfc, 0xe9, 0x4f),
    COLOR (0x72, 0x9f, 0xcf),
    COLOR (0xad, 0x7f, 0xa8),
    COLOR (0x34, 0xe2, 0xe2),
    COLOR (0xee, 0xee, 0xec)
  },

  /* Linux palette */
  {
    COLOR (0x00, 0x00, 0x00),
    COLOR (0xaa, 0x00, 0x00),
    COLOR (0x00, 0xaa, 0x00),
    COLOR (0xaa, 0x55, 0x00),
    COLOR (0x00, 0x00, 0xaa),
    COLOR (0xaa, 0x00, 0xaa),
    COLOR (0x00, 0xaa, 0xaa),
    COLOR (0xaa, 0xaa, 0xaa),
    COLOR (0x55, 0x55, 0x55),
    COLOR (0xff, 0x55, 0x55),
    COLOR (0x55, 0xff, 0x55),
    COLOR (0xff, 0xff, 0x55),
    COLOR (0x55, 0x55, 0xff),
    COLOR (0xff, 0x55, 0xff),
    COLOR (0x55, 0xff, 0xff),
    COLOR (0xff, 0xff, 0xff)
  },

  /* XTerm palette */
  {
    COLOR (0x00, 0x00, 0x00),
    COLOR (0xcd, 0x00, 0x00),
    COLOR (0x00, 0xcd, 0x00),
    COLOR (0xcd, 0xcd, 0x00),
    COLOR (0x00, 0x00, 0xee),
    COLOR (0xcd, 0x00, 0xcd),
    COLOR (0x00, 0xcd, 0xcd),
    COLOR (0xe5, 0xe5, 0xe5),
    COLOR (0x7f, 0x7f, 0x7f),
    COLOR (0xff, 0x00, 0x00),
    COLOR (0x00, 0xff, 0x00),
    COLOR (0xff, 0xff, 0x00),
    COLOR (0x5c, 0x5c, 0xff),
    COLOR (0xff, 0x00, 0xff),
    COLOR (0x00, 0xff, 0xff),
    COLOR (0xff, 0xff, 0xff)
  },

  /* RXVT palette */
  {
    COLOR (0x00, 0x00, 0x00),
    COLOR (0xcd, 0x00, 0x00),
    COLOR (0x00, 0xcd, 0x00),
    COLOR (0xcd, 0xcd, 0x00),
    COLOR (0x00, 0x00, 0xcd),
    COLOR (0xcd, 0x00, 0xcd),
    COLOR (0x00, 0xcd, 0xcd),
    COLOR (0xfa, 0xeb, 0xd7),
    COLOR (0x40, 0x40, 0x40),
    COLOR (0xff, 0x00, 0x00),
    COLOR (0x00, 0xff, 0x00),
    COLOR (0xff, 0xff, 0x00),
    COLOR (0x00, 0x00, 0xff),
    COLOR (0xff, 0x00, 0xff),
    COLOR (0x00, 0xff, 0xff),
    COLOR (0xff, 0xff, 0xff)
  },

  /* Solarized palette (1.0.0beta2): http://ethanschoonover.com/solarized */
  {
    COLOR (0x07, 0x36, 0x42),  /*  0: base02  */
    COLOR (0xdc, 0x32, 0x2f),  /*  1: red     */
    COLOR (0x85, 0x99, 0x00),  /*  2: green   */
    COLOR (0xb5, 0x89, 0x00),  /*  3: yellow  */
    COLOR (0x26, 0x8b, 0xd2),  /*  4: blue    */
    COLOR (0xd3, 0x36, 0x82),  /*  5: magenta */
    COLOR (0x2a, 0xa1, 0x98),  /*  6: cyan    */
    COLOR (0xee, 0xe8, 0xd5),  /*  7: base2   */
    COLOR (0x00, 0x2b, 0x36),  /*  8: base03  */
    COLOR (0xcb, 0x4b, 0x16),  /*  9: orange  */
    COLOR (0x58, 0x6e, 0x75),  /* 10: base01  */
    COLOR (0x65, 0x7b, 0x83),  /* 11: base00  */
    COLOR (0x83, 0x94, 0x96),  /* 12: base0   */
    COLOR (0x6c, 0x71, 0xc4),  /* 13: violet  */
    COLOR (0x93, 0xa1, 0xa1),  /* 14: base1   */
    COLOR (0xfd, 0xf6, 0xe3)   /* 15: base3   */
  },
};

#undef COLOR

static void profile_colors_notify_scheme_combo_cb (GSettings *profile,
                                                   const char *key,
                                                   GtkComboBox *combo);

static void profile_palette_notify_scheme_combo_cb (GSettings *profile,
                                                    const char *key,
                                                    GtkComboBox *combo);

static void profile_palette_notify_colorpickers_cb (GSettings *profile,
                                                    const char *key,
                                                    gpointer user_data);

static void profile_notify_encoding_combo_cb (GSettings *profile,
                                              const char *key,
                                              GtkComboBox *combo);

enum {
        ENCODINGS_COL_ID,
        ENCODINGS_COL_TEXT
};

/* gdk_rgba_equal is too strict! */
static gboolean
rgba_equal (const GdkRGBA *a,
            const GdkRGBA *b)
{
  gdouble dr, dg, db;

  dr = a->red - b->red;
  dg = a->green - b->green;
  db = a->blue - b->blue;

  return (dr * dr + dg * dg + db * db) < 1e-4;
}

static gboolean
palette_cmp (const GdkRGBA *ca,
             const GdkRGBA *cb)
{
  guint i;

  for (i = 0; i < TERMINAL_PALETTE_SIZE; ++i)
    if (!rgba_equal (&ca[i], &cb[i]))
      return FALSE;

  return TRUE;
}

static gboolean
palette_is_builtin (const GdkRGBA *colors,
                    gsize n_colors,
                    guint *n)
{
  guint i;

  if (n_colors != TERMINAL_PALETTE_SIZE)
    return FALSE;

  for (i = 0; i < TERMINAL_PALETTE_N_BUILTINS; ++i)
    {
      if (palette_cmp (colors, terminal_palettes[i]))
        {
          *n = i;
          return TRUE;
        }
    }

  return FALSE;
}

static void
modify_palette_entry (GSettings       *profile,
                      guint            i,
                      const GdkRGBA   *color)
{
  gs_free GdkRGBA *colors;
  gsize n_colors;

  /* FIXMEchpe: this can be optimised, don't really need to parse the colours! */

  colors = terminal_g_settings_get_rgba_palette (profile, TERMINAL_PROFILE_PALETTE_KEY, &n_colors);

  if (i < n_colors)
    {
      colors[i] = *color;
      terminal_g_settings_set_rgba_palette (profile, TERMINAL_PROFILE_PALETTE_KEY,
                                            colors, n_colors);
    }
}

static void
color_scheme_combo_changed_cb (GtkWidget *combo,
                               GParamSpec *pspec,
                               GSettings *profile)
{
  guint i;

  i = gtk_combo_box_get_active (GTK_COMBO_BOX (combo));

  if (i < G_N_ELEMENTS (color_schemes))
    {
      g_signal_handlers_block_by_func (profile, (void*)profile_colors_notify_scheme_combo_cb, combo);
      terminal_g_settings_set_rgba (profile, TERMINAL_PROFILE_FOREGROUND_COLOR_KEY, &color_schemes[i].foreground);
      terminal_g_settings_set_rgba (profile, TERMINAL_PROFILE_BACKGROUND_COLOR_KEY, &color_schemes[i].background);
      g_signal_handlers_unblock_by_func (profile, (void*)profile_colors_notify_scheme_combo_cb, combo);
    }
  else
    {
      /* "custom" selected, no change */
    }
}

static void
profile_colors_notify_scheme_combo_cb (GSettings *profile,
                                       const char *key,
                                       GtkComboBox *combo)
{
  GdkRGBA fg, bg;
  guint i;

  terminal_g_settings_get_rgba (profile, TERMINAL_PROFILE_FOREGROUND_COLOR_KEY, &fg);
  terminal_g_settings_get_rgba (profile, TERMINAL_PROFILE_BACKGROUND_COLOR_KEY, &bg);

  for (i = 0; i < G_N_ELEMENTS (color_schemes); ++i)
    {
      if (rgba_equal (&fg, &color_schemes[i].foreground) &&
          rgba_equal (&bg, &color_schemes[i].background))
        break;
    }
  /* If we didn't find a match, then we get the last combo box item which is "custom" */

  g_signal_handlers_block_by_func (combo, (void*)color_scheme_combo_changed_cb, profile);
  gtk_combo_box_set_active (GTK_COMBO_BOX (combo), i);
  g_signal_handlers_unblock_by_func (combo, (void*)color_scheme_combo_changed_cb, profile);
}

static void
palette_scheme_combo_changed_cb (GtkComboBox *combo,
                                 GParamSpec *pspec,
                                 GSettings *profile)
{
  int i;

  i = gtk_combo_box_get_active (GTK_COMBO_BOX (combo));

  g_signal_handlers_block_by_func (profile, (void*)profile_colors_notify_scheme_combo_cb, combo);
  if (i < TERMINAL_PALETTE_N_BUILTINS)
    terminal_g_settings_set_rgba_palette (profile, TERMINAL_PROFILE_PALETTE_KEY,
                                          terminal_palettes[i], TERMINAL_PALETTE_SIZE);
  else
    {
      /* "custom" selected, no change */
    }
  g_signal_handlers_unblock_by_func (profile, (void*)profile_colors_notify_scheme_combo_cb, combo);
}

static void
profile_palette_notify_scheme_combo_cb (GSettings *profile,
                                        const char *key,
                                        GtkComboBox *combo)
{
  gs_free GdkRGBA *colors;
  gsize n_colors;
  guint i;

  colors = terminal_g_settings_get_rgba_palette (profile, TERMINAL_PROFILE_PALETTE_KEY, &n_colors);
  if (!palette_is_builtin (colors, n_colors, &i))
    /* If we didn't find a match, then we want the last combo
     * box item which is "custom"
     */
    i = TERMINAL_PALETTE_N_BUILTINS;

  g_signal_handlers_block_by_func (combo, (void*)palette_scheme_combo_changed_cb, profile);
  gtk_combo_box_set_active (combo, i);
  g_signal_handlers_unblock_by_func (combo, (void*)palette_scheme_combo_changed_cb, profile);
}

static void
palette_color_notify_cb (GtkColorButton *button,
                         GParamSpec *pspec,
                         GSettings *profile)
{
  GdkRGBA color;
  guint i;

  gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (button), &color);
  i = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (button), "palette-entry-index"));

  g_signal_handlers_block_by_func (profile, (void*)profile_palette_notify_colorpickers_cb, nullptr);
  modify_palette_entry (profile, i, &color);
  g_signal_handlers_unblock_by_func (profile, (void*)profile_palette_notify_colorpickers_cb, nullptr);
}

static void
profile_palette_notify_colorpickers_cb (GSettings *profile,
                                        const char *key,
                                        gpointer user_data)
{
  GtkWidget *w;
  GtkBuilder *builder = the_pref_data->builder;
  gs_free GdkRGBA *colors;
  gsize n_colors, i;

  g_assert (strcmp (key, TERMINAL_PROFILE_PALETTE_KEY) == 0);

  colors = terminal_g_settings_get_rgba_palette (profile, TERMINAL_PROFILE_PALETTE_KEY, &n_colors);

  n_colors = MIN (n_colors, TERMINAL_PALETTE_SIZE);
  for (i = 0; i < n_colors; i++)
    {
      char name[32];

      g_snprintf (name, sizeof (name), "palette-colorpicker-%" G_GSIZE_FORMAT, i);
      w = (GtkWidget *) gtk_builder_get_object (builder, name);

      g_signal_handlers_block_by_func (w, (void*)palette_color_notify_cb, profile);
      gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (w), &colors[i]);
      g_signal_handlers_unblock_by_func (w, (void*)palette_color_notify_cb, profile);
    }
}

static void
profile_scrollback_warning_update_cb (GSettings *profile,
                                      const char *key,
                                      GtkWidget *infobar)
{
  gboolean unlimited = g_settings_get_boolean (profile, TERMINAL_PROFILE_SCROLLBACK_UNLIMITED_KEY);
  gint lines = g_settings_get_int (profile, TERMINAL_PROFILE_SCROLLBACK_LINES_KEY);

  gtk_widget_set_visible (infobar, unlimited || lines >= 1000000);
}

static void
custom_command_entry_changed_cb (GtkEntry *entry)
{
  const char *command;
  gs_free_error GError *error = nullptr;

  command = gtk_entry_get_text (entry);

  if (command[0] == '\0' ||
      g_shell_parse_argv (command, nullptr, nullptr, &error))
    {
      gtk_entry_set_icon_from_icon_name (entry, GTK_ENTRY_ICON_SECONDARY, nullptr);
    }
  else
    {
      gs_free char *tooltip;

      gtk_entry_set_icon_from_icon_name (entry, GTK_ENTRY_ICON_SECONDARY, "dialog-warning");

      tooltip = g_strdup_printf (_("Error parsing command: %s"), error->message);
      gtk_entry_set_icon_tooltip_text (entry, GTK_ENTRY_ICON_SECONDARY, tooltip);
    }
}

static void
default_size_reset_cb (GtkWidget *button,
                       GSettings *profile)
{
  g_settings_reset (profile, TERMINAL_PROFILE_DEFAULT_SIZE_COLUMNS_KEY);
  g_settings_reset (profile, TERMINAL_PROFILE_DEFAULT_SIZE_ROWS_KEY);
}

static void
cell_scale_reset_cb (GtkWidget *button,
                     GSettings *profile)
{
  g_settings_reset (profile, TERMINAL_PROFILE_CELL_HEIGHT_SCALE_KEY);
  g_settings_reset (profile, TERMINAL_PROFILE_CELL_WIDTH_SCALE_KEY);
}

static void
reset_compat_defaults_cb (GtkWidget *button,
                          GSettings *profile)
{
  g_settings_reset (profile, TERMINAL_PROFILE_DELETE_BINDING_KEY);
  g_settings_reset (profile, TERMINAL_PROFILE_BACKSPACE_BINDING_KEY);
  g_settings_reset (profile, TERMINAL_PROFILE_ENCODING_KEY);
  g_settings_reset (profile, TERMINAL_PROFILE_CJK_UTF8_AMBIGUOUS_WIDTH_KEY);
  g_settings_reset (profile, TERMINAL_PROFILE_ENABLE_SIXEL_KEY);
}

static gboolean
tree_model_id_to_iter_recurse (GtkTreeModel *model,
                               int id_column,
                               const char *active_id,
                               GtkTreeIter *iter,
                               GtkTreeIter *result_iter)
{
  do {
    /* Descend the tree */
    GtkTreeIter child_iter;
    if (gtk_tree_model_iter_children(model, &child_iter, iter) &&
        tree_model_id_to_iter_recurse (model, id_column, active_id, &child_iter, result_iter))
      return TRUE;

    gs_free char *id = nullptr;
    gtk_tree_model_get (model, iter, id_column, &id, -1);
    if (g_strcmp0 (id, active_id) == 0) {
      *result_iter = *iter;
      return TRUE;
    }
  } while (gtk_tree_model_iter_next (model, iter));

  return FALSE;
}

static gboolean
tree_model_id_to_iter (GtkTreeModel *model,
                       int id_column,
                       const char *active_id,
                       GtkTreeIter *iter)
{
  GtkTreeIter first_iter;

  return gtk_tree_model_get_iter_first(model, &first_iter) &&
    tree_model_id_to_iter_recurse(model, id_column, active_id, &first_iter, iter);
}

static void
profile_encoding_combo_changed_cb (GtkComboBox *combo,
                                   GSettings *profile)
{
  GtkTreeIter iter;

  if (!gtk_combo_box_get_active_iter(combo, &iter))
    return;

  gs_free char *encoding = nullptr;
  gtk_tree_model_get(gtk_combo_box_get_model(combo),
                     &iter,
                     ENCODINGS_COL_ID, &encoding,
                     -1);
  if (encoding == nullptr)
    return;

  g_signal_handlers_block_by_func (profile, (void*)profile_notify_encoding_combo_cb, combo);
  g_settings_set_string(profile, TERMINAL_PROFILE_ENCODING_KEY, encoding);
  g_signal_handlers_unblock_by_func (profile, (void*)profile_notify_encoding_combo_cb, combo);
}

static void
profile_notify_encoding_combo_cb (GSettings *profile,
                                  const char *key,
                                  GtkComboBox *combo)
{
  gs_free char *encoding = nullptr;
  g_settings_get(profile, key, "s", &encoding);

  g_signal_handlers_block_by_func (combo, (void*)profile_encoding_combo_changed_cb, profile);

  GtkTreeIter iter;
  if (tree_model_id_to_iter(gtk_combo_box_get_model(combo),
                            ENCODINGS_COL_ID,
                            encoding,
                            &iter)) {
    gtk_combo_box_set_active_iter(combo, &iter);
  } else {
    gtk_combo_box_set_active(combo, -1);
  }

  g_signal_handlers_unblock_by_func (combo, (void*)profile_encoding_combo_changed_cb, profile);
}

/*
 * initialize widgets
 */

static void
init_color_scheme_menu (GtkWidget *widget)
{
  GtkCellRenderer *renderer;
  GtkTreeIter iter;
  gs_unref_object GtkListStore *store;
  guint i;

  store = gtk_list_store_new (1, G_TYPE_STRING);
  for (i = 0; i < G_N_ELEMENTS (color_schemes); ++i)
    gtk_list_store_insert_with_values (store, &iter, -1,
                                       0, _(color_schemes[i].name),
                                       -1);
  gtk_list_store_insert_with_values (store, &iter, -1,
                                      0, _("Custom"),
                                      -1);

  gtk_combo_box_set_model (GTK_COMBO_BOX (widget), GTK_TREE_MODEL (store));

  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (widget), renderer, TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (widget), renderer, "text", 0, nullptr);
}

typedef enum {
        GROUP_UTF8,
        GROUP_CJKV,
        GROUP_OBSOLETE,
        LAST_GROUP
} EncodingGroup;

typedef struct {
  const char *charset;
  const char *name;
  EncodingGroup group;
} EncodingEntry;

/* These MUST be sorted by charset so that bsearch can work! */
static const EncodingEntry encodings[] = {
  { "BIG5",           N_("Chinese Traditional"), GROUP_CJKV },
  { "BIG5-HKSCS",     N_("Chinese Traditional"), GROUP_CJKV },
  { "CP866",          N_("Cyrillic/Russian"),    GROUP_OBSOLETE },
  { "EUC-JP",         N_("Japanese"),            GROUP_CJKV },
  { "EUC-KR",         N_("Korean"),              GROUP_CJKV },
  { "EUC-TW",         N_("Chinese Traditional"), GROUP_CJKV },
  { "GB18030",        N_("Chinese Simplified"),  GROUP_CJKV },
  { "GB2312",         N_("Chinese Simplified"),  GROUP_CJKV },
  { "GBK",            N_("Chinese Simplified"),  GROUP_CJKV },
  { "IBM850",         N_("Western"),             GROUP_OBSOLETE },
  { "IBM852",         N_("Central European"),    GROUP_OBSOLETE },
  { "IBM855",         N_("Cyrillic"),            GROUP_OBSOLETE },
  { "IBM857",         N_("Turkish"),             GROUP_OBSOLETE },
  { "IBM862",         N_("Hebrew"),              GROUP_OBSOLETE },
  { "IBM864",         N_("Arabic"),              GROUP_OBSOLETE },
  { "ISO-8859-1",     N_("Western"),             GROUP_OBSOLETE },
  { "ISO-8859-10",    N_("Nordic"),              GROUP_OBSOLETE },
  { "ISO-8859-13",    N_("Baltic"),              GROUP_OBSOLETE },
  { "ISO-8859-14",    N_("Celtic"),              GROUP_OBSOLETE },
  { "ISO-8859-15",    N_("Western"),             GROUP_OBSOLETE },
  { "ISO-8859-16",    N_("Romanian"),            GROUP_OBSOLETE },
  { "ISO-8859-2",     N_("Central European"),    GROUP_OBSOLETE },
  { "ISO-8859-3",     N_("South European"),      GROUP_OBSOLETE },
  { "ISO-8859-4",     N_("Baltic"),              GROUP_OBSOLETE },
  { "ISO-8859-5",     N_("Cyrillic"),            GROUP_OBSOLETE },
  { "ISO-8859-6",     N_("Arabic"),              GROUP_OBSOLETE },
  { "ISO-8859-7",     N_("Greek"),               GROUP_OBSOLETE },
  { "ISO-8859-8",     N_("Hebrew Visual"),       GROUP_OBSOLETE },
  { "ISO-8859-8-I",   N_("Hebrew"),              GROUP_OBSOLETE },
  { "ISO-8859-9",     N_("Turkish"),             GROUP_OBSOLETE },
  { "KOI8-R",         N_("Cyrillic"),            GROUP_OBSOLETE },
  { "KOI8-U",         N_("Cyrillic/Ukrainian"),  GROUP_OBSOLETE },
  { "MAC-CYRILLIC",   N_("Cyrillic"),            GROUP_OBSOLETE },
  { "MAC_ARABIC",     N_("Arabic"),              GROUP_OBSOLETE },
  { "MAC_CE",         N_("Central European"),    GROUP_OBSOLETE },
  { "MAC_CROATIAN",   N_("Croatian"),            GROUP_OBSOLETE },
  { "MAC_GREEK",      N_("Greek"),               GROUP_OBSOLETE },
  { "MAC_HEBREW",     N_("Hebrew"),              GROUP_OBSOLETE },
  { "MAC_ROMAN",      N_("Western"),             GROUP_OBSOLETE },
  { "MAC_ROMANIAN",   N_("Romanian"),            GROUP_OBSOLETE },
  { "MAC_TURKISH",    N_("Turkish"),             GROUP_OBSOLETE },
  { "MAC_UKRAINIAN",  N_("Cyrillic/Ukrainian"),  GROUP_OBSOLETE },
  { "SHIFT_JIS",      N_("Japanese"),            GROUP_CJKV },
  { "TIS-620",        N_("Thai"),                GROUP_OBSOLETE },
  { "UHC",            N_("Korean"),              GROUP_CJKV },
  { "UTF-8",          N_("Unicode"),             GROUP_UTF8 },
  { "WINDOWS-1250",   N_("Central European"),    GROUP_OBSOLETE },
  { "WINDOWS-1251",   N_("Cyrillic"),            GROUP_OBSOLETE },
  { "WINDOWS-1252",   N_("Western"),             GROUP_OBSOLETE },
  { "WINDOWS-1253",   N_("Greek"),               GROUP_OBSOLETE },
  { "WINDOWS-1254",   N_("Turkish"),             GROUP_OBSOLETE },
  { "WINDOWS-1255",   N_("Hebrew"),              GROUP_OBSOLETE},
  { "WINDOWS-1256",   N_("Arabic"),              GROUP_OBSOLETE },
  { "WINDOWS-1257",   N_("Baltic"),              GROUP_OBSOLETE },
  { "WINDOWS-1258",   N_("Vietnamese"),          GROUP_OBSOLETE },
};

static const struct {
  EncodingGroup group;
  const char *name;
} encodings_group_names[] = {
  { GROUP_UTF8,     N_("Unicode") },
  { GROUP_CJKV,     N_("Legacy CJK Encodings") },
  { GROUP_OBSOLETE, N_("Obsolete Encodings") },
};

#define EM_DASH "—"

static void
append_encodings_for_group (GtkTreeStore *store,
                            EncodingGroup group,
                            gboolean submenu)
{
  GtkTreeIter parent_iter;

  if (submenu) {
    gtk_tree_store_insert_with_values (store,
                                       &parent_iter,
                                       nullptr,
                                       -1,
                                       ENCODINGS_COL_ID, nullptr,
                                       ENCODINGS_COL_TEXT, _(encodings_group_names[group].name),
                                       -1);
  }

  for (guint i = 0; i < G_N_ELEMENTS (encodings); i++) {
    if (encodings[i].group != group)
      continue;

    /* Skip encodings not supported by ICU */
    if (terminal_util_translate_encoding (encodings[i].charset) == nullptr)
      continue;

    gs_free char *name = g_strdup_printf ("%s " EM_DASH " %s",
                                          _(encodings[i].name), encodings[i].charset);

    GtkTreeIter iter;
    gtk_tree_store_insert_with_values (store,
                                       &iter,
                                       submenu ? &parent_iter : nullptr,
                                       -1,
                                       ENCODINGS_COL_ID, encodings[i].charset,
                                       ENCODINGS_COL_TEXT, name,
                                       -1);
  }
}

static GtkTreeStore *
encodings_tree_store_new (void)
{
  GtkTreeStore *store = gtk_tree_store_new (2, G_TYPE_STRING, G_TYPE_STRING);

  append_encodings_for_group(store, GROUP_UTF8, FALSE); /* UTF-8 in main menu */
  append_encodings_for_group(store, GROUP_CJKV, TRUE);
  append_encodings_for_group(store, GROUP_OBSOLETE, TRUE);

  return store;
}

static void
init_encodings_combo (GtkWidget *widget)
{
  gs_unref_object GtkTreeStore *store = encodings_tree_store_new ();
  gtk_combo_box_set_model (GTK_COMBO_BOX (widget), GTK_TREE_MODEL (store));
}

static gboolean
s_to_rgba (GValue *value,
           GVariant *variant,
           gpointer user_data)
{
  const char *s;
  GdkRGBA color;

  g_variant_get (variant, "&s", &s);
  if (!gdk_rgba_parse (&color, s))
    return FALSE;

  color.alpha = 1.0;
  g_value_set_boxed (value, &color);
  return TRUE;
}

static GVariant *
rgba_to_s (const GValue *value,
           const GVariantType *expected_type,
           gpointer user_data)
{
  GdkRGBA *color;
  gs_free char *s = nullptr;

  color = reinterpret_cast<GdkRGBA*>(g_value_get_boxed (value));
  if (color == nullptr)
    return nullptr;

  s = gdk_rgba_to_string (color);
  return g_variant_new_string (s);
}

static gboolean
string_to_enum (GValue *value,
                GVariant *variant,
                gpointer user_data)
{
  GType (* get_type) (void) = (GType (*)(void))user_data;
  GEnumClass *klass;
  GEnumValue *eval = nullptr;
  const char *s;
  guint i;

  g_variant_get (variant, "&s", &s);

  klass = reinterpret_cast<GEnumClass*>(g_type_class_ref (get_type ()));
  for (i = 0; i < klass->n_values; ++i) {
    if (strcmp (klass->values[i].value_nick, s) != 0)
      continue;

    eval = &klass->values[i];
    break;
  }

  if (eval)
    g_value_set_int (value, eval->value);

  g_type_class_unref (klass);

  return eval != nullptr;
}

static GVariant *
enum_to_string (const GValue *value,
                const GVariantType *expected_type,
                gpointer user_data)
{
  GType (* get_type) (void) = (GType (*)(void))user_data;
  GEnumClass *klass;
  GEnumValue *eval = nullptr;
  int val;
  guint i;
  GVariant *variant = nullptr;

  val = g_value_get_int (value);

  klass = reinterpret_cast<GEnumClass*>(g_type_class_ref (get_type ()));
  for (i = 0; i < klass->n_values; ++i) {
    if (klass->values[i].value != val)
      continue;

    eval = &klass->values[i];
    break;
  }

  if (eval)
    variant = g_variant_new_string (eval->value_nick);

  g_type_class_unref (klass);

  return variant;
}

static gboolean
scrollbar_policy_to_bool (GValue *value,
                          GVariant *variant,
                          gpointer user_data)
{
  const char *str;

  g_variant_get (variant, "&s", &str);
  g_value_set_boolean (value, g_str_equal (str, "always"));

  return TRUE;
}

static GVariant *
bool_to_scrollbar_policy (const GValue *value,
                          const GVariantType *expected_type,
                          gpointer user_data)
{
  return g_variant_new_string (g_value_get_boolean (value) ? "always" : "never");
}

static gboolean
monospace_filter (const PangoFontFamily *family,
                  const PangoFontFace   *face,
                  gpointer data)
{
  return pango_font_family_is_monospace ((PangoFontFamily *) family);
}

/* Called once per Preferences window, to initialize stuff that doesn't depend on the profile being edited */
void
profile_prefs_init (void)
{
  GtkWidget *w;
  GtkBuilder *builder = the_pref_data->builder;
  char *text;

  the_pref_data->profile_signals = g_array_new (FALSE, FALSE, sizeof (ProfilePrefsSignal));
  the_pref_data->profile_bindings = g_array_new (FALSE, FALSE, sizeof (ProfilePrefsBinding));

  w = (GtkWidget *) gtk_builder_get_object (builder, "color-scheme-combobox");
  init_color_scheme_menu (w);

  w = (GtkWidget *) gtk_builder_get_object (builder, "encoding-combobox");
  init_encodings_combo (w);

  /* Translators: Appears as: [numeric entry] × width */
  text = g_strdup_printf ("× %s", _("width"));
  gtk_label_set_text ((GtkLabel *) gtk_builder_get_object (builder, "cell-width-scale-label"),
                      text);
  g_free (text);
  /* Translators: Appears as: [numeric entry] × height */
  text = g_strdup_printf ("× %s", _("height"));
  gtk_label_set_text ((GtkLabel *) gtk_builder_get_object (builder, "cell-height-scale-label"),
                      text);
  g_free (text);
}

/* Called each time the user switches away from a profile, so it's no longer being edited */
void
profile_prefs_unload (void)
{
  profile_prefs_signal_handlers_disconnect_all ();
  profile_prefs_settings_unbind_all ();
}

/* Called each time the user selects a new profile to edit */
void
profile_prefs_load (const char *uuid, GSettings *profile)
{
  GtkWidget *w;
  GtkBuilder *builder = the_pref_data->builder;
  guint i;

  profile_prefs_unload ();

  gtk_label_set_text (GTK_LABEL (gtk_builder_get_object (builder, "profile-uuid")),
                      uuid);

  profile_prefs_signal_connect (gtk_builder_get_object (builder, "default-size-reset-button"),
                                "clicked",
                                G_CALLBACK (default_size_reset_cb),
                                profile);
  profile_prefs_signal_connect (gtk_builder_get_object (builder, "cell-scale-reset-button"),
                                "clicked",
                                G_CALLBACK (cell_scale_reset_cb),
                                profile);

  /* Hook up the palette colorpickers and combo box */

  for (i = 0; i < TERMINAL_PALETTE_SIZE; ++i)
    {
      char name[32];
      char *text;

      g_snprintf (name, sizeof (name), "palette-colorpicker-%u", i);
      w = (GtkWidget *) gtk_builder_get_object (builder, name);

      g_object_set_data (G_OBJECT (w), "palette-entry-index", GUINT_TO_POINTER (i));

      text = g_strdup_printf (_("Choose Palette Color %u"), i);
      gtk_color_button_set_title (GTK_COLOR_BUTTON (w), text);
      g_free (text);

      text = g_strdup_printf (_("Palette entry %u"), i);
      gtk_widget_set_tooltip_text (w, text);
      g_free (text);

      profile_prefs_signal_connect (w, "notify::rgba",
                                    G_CALLBACK (palette_color_notify_cb),
                                    profile);
    }

  profile_palette_notify_colorpickers_cb (profile, TERMINAL_PROFILE_PALETTE_KEY, nullptr);
  profile_prefs_signal_connect (profile, "changed::" TERMINAL_PROFILE_PALETTE_KEY,
                                G_CALLBACK (profile_palette_notify_colorpickers_cb),
                                nullptr);

  w = (GtkWidget *) gtk_builder_get_object (builder, "palette-combobox");
  profile_prefs_signal_connect (w, "notify::active",
                                G_CALLBACK (palette_scheme_combo_changed_cb),
                                profile);

  profile_palette_notify_scheme_combo_cb (profile, TERMINAL_PROFILE_PALETTE_KEY, GTK_COMBO_BOX (w));
  profile_prefs_signal_connect (profile, "changed::" TERMINAL_PROFILE_PALETTE_KEY,
                                G_CALLBACK (profile_palette_notify_scheme_combo_cb),
                                w);

  /* Hook up the color scheme pickers and combo box */
  w = (GtkWidget *) gtk_builder_get_object (builder, "color-scheme-combobox");
  profile_prefs_signal_connect (w, "notify::active",
                                G_CALLBACK (color_scheme_combo_changed_cb),
                                profile);

  profile_colors_notify_scheme_combo_cb (profile, nullptr, GTK_COMBO_BOX (w));
  profile_prefs_signal_connect (profile, "changed::" TERMINAL_PROFILE_FOREGROUND_COLOR_KEY,
                                G_CALLBACK (profile_colors_notify_scheme_combo_cb),
                                w);
  profile_prefs_signal_connect (profile, "changed::" TERMINAL_PROFILE_BACKGROUND_COLOR_KEY,
                                G_CALLBACK (profile_colors_notify_scheme_combo_cb),
                                w);

  w = GTK_WIDGET (gtk_builder_get_object (builder, "custom-command-entry"));
  custom_command_entry_changed_cb (GTK_ENTRY (w));
  profile_prefs_signal_connect (w, "changed",
                                G_CALLBACK (custom_command_entry_changed_cb), nullptr);

  profile_prefs_signal_connect (gtk_builder_get_object (builder, "reset-compat-defaults-button"),
                                "clicked",
                                G_CALLBACK (reset_compat_defaults_cb),
                                profile);

  profile_prefs_settings_bind_with_mapping (profile,
                                            TERMINAL_PROFILE_BACKGROUND_COLOR_KEY,
                                            gtk_builder_get_object (builder,
                                                                    "background-colorpicker"),
                                            "rgba",
                                            GSettingsBindFlags(G_SETTINGS_BIND_GET |
							       G_SETTINGS_BIND_SET),
                                            (GSettingsBindGetMapping) s_to_rgba,
                                            (GSettingsBindSetMapping) rgba_to_s,
                                            nullptr, nullptr);

  profile_prefs_settings_bind_with_mapping (profile,
                                            TERMINAL_PROFILE_BACKSPACE_BINDING_KEY,
                                            gtk_builder_get_object (builder,
                                                                    "backspace-binding-combobox"),
                                            "active",
                                            GSettingsBindFlags(G_SETTINGS_BIND_GET |
							       G_SETTINGS_BIND_SET),
                                            (GSettingsBindGetMapping) string_to_enum,
                                            (GSettingsBindSetMapping) enum_to_string,
                                            vte_erase_binding_get_type, nullptr);
  profile_prefs_settings_bind (profile,
                               TERMINAL_PROFILE_BOLD_IS_BRIGHT_KEY,
                               gtk_builder_get_object (builder, "bold-is-bright-checkbutton"),
                               "active",
			       GSettingsBindFlags(G_SETTINGS_BIND_GET |
						  G_SETTINGS_BIND_SET));
  profile_prefs_settings_bind (profile, TERMINAL_PROFILE_BOLD_COLOR_SAME_AS_FG_KEY,
                               gtk_builder_get_object (builder,
                                                       "bold-color-checkbutton"),
                               "active",
                               GSettingsBindFlags(G_SETTINGS_BIND_GET |
						  G_SETTINGS_BIND_INVERT_BOOLEAN |
						  G_SETTINGS_BIND_SET));

  w = GTK_WIDGET (gtk_builder_get_object (builder, "bold-colorpicker"));
  profile_prefs_settings_bind (profile, TERMINAL_PROFILE_BOLD_COLOR_SAME_AS_FG_KEY,
                               w,
                               "sensitive",
                               GSettingsBindFlags(G_SETTINGS_BIND_GET |
						  G_SETTINGS_BIND_INVERT_BOOLEAN |
						  G_SETTINGS_BIND_NO_SENSITIVITY));
  profile_prefs_settings_bind_with_mapping (profile, TERMINAL_PROFILE_BOLD_COLOR_KEY,
                                            w,
                                            "rgba",
                                            GSettingsBindFlags(G_SETTINGS_BIND_GET |
							       G_SETTINGS_BIND_SET |
							       G_SETTINGS_BIND_NO_SENSITIVITY),
                                            (GSettingsBindGetMapping) s_to_rgba,
                                            (GSettingsBindSetMapping) rgba_to_s,
                                            nullptr, nullptr);

  w = GTK_WIDGET (gtk_builder_get_object (builder, "cell-height-scale-spinbutton"));
  profile_prefs_settings_bind (profile, TERMINAL_PROFILE_CELL_HEIGHT_SCALE_KEY,
                               gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (w)),
                               "value",
			       GSettingsBindFlags(G_SETTINGS_BIND_GET |
						  G_SETTINGS_BIND_SET));

  w = GTK_WIDGET (gtk_builder_get_object (builder, "cell-width-scale-spinbutton"));
  profile_prefs_settings_bind (profile, TERMINAL_PROFILE_CELL_WIDTH_SCALE_KEY,
                               gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (w)),
                               "value",
			       GSettingsBindFlags(G_SETTINGS_BIND_GET |
						  G_SETTINGS_BIND_SET));

  profile_prefs_settings_bind (profile, TERMINAL_PROFILE_CURSOR_COLORS_SET_KEY,
                               gtk_builder_get_object (builder,
                                                       "cursor-colors-checkbutton"),
                               "active",
			       GSettingsBindFlags(G_SETTINGS_BIND_GET |
						  G_SETTINGS_BIND_SET));

  w = GTK_WIDGET (gtk_builder_get_object (builder, "cursor-foreground-colorpicker"));
  profile_prefs_settings_bind (profile, TERMINAL_PROFILE_CURSOR_COLORS_SET_KEY,
                               w,
                               "sensitive",
                               GSettingsBindFlags(G_SETTINGS_BIND_GET |
						  G_SETTINGS_BIND_NO_SENSITIVITY));
  profile_prefs_settings_bind_with_mapping (profile, TERMINAL_PROFILE_CURSOR_FOREGROUND_COLOR_KEY,
                                            w,
                                            "rgba",
                                            GSettingsBindFlags(G_SETTINGS_BIND_GET |
							       G_SETTINGS_BIND_SET |
							       G_SETTINGS_BIND_NO_SENSITIVITY),
                                            (GSettingsBindGetMapping) s_to_rgba,
                                            (GSettingsBindSetMapping) rgba_to_s,
                                            nullptr, nullptr);

  w = GTK_WIDGET (gtk_builder_get_object (builder, "cursor-background-colorpicker"));
  profile_prefs_settings_bind (profile, TERMINAL_PROFILE_CURSOR_COLORS_SET_KEY,
                               w,
                               "sensitive",
                               GSettingsBindFlags(G_SETTINGS_BIND_GET |
						  G_SETTINGS_BIND_NO_SENSITIVITY));
  profile_prefs_settings_bind_with_mapping (profile, TERMINAL_PROFILE_CURSOR_BACKGROUND_COLOR_KEY,
                                            w,
                                            "rgba",
                                            GSettingsBindFlags(G_SETTINGS_BIND_GET |
							       G_SETTINGS_BIND_SET |
							       G_SETTINGS_BIND_NO_SENSITIVITY),
                                            (GSettingsBindGetMapping) s_to_rgba,
                                            (GSettingsBindSetMapping) rgba_to_s,
                                            nullptr, nullptr);

  profile_prefs_settings_bind (profile, TERMINAL_PROFILE_HIGHLIGHT_COLORS_SET_KEY,
                               gtk_builder_get_object (builder,
                                                       "highlight-colors-checkbutton"),
                               "active",
			       GSettingsBindFlags(G_SETTINGS_BIND_GET |
						  G_SETTINGS_BIND_SET));

  w = GTK_WIDGET (gtk_builder_get_object (builder, "highlight-foreground-colorpicker"));
  profile_prefs_settings_bind (profile, TERMINAL_PROFILE_HIGHLIGHT_COLORS_SET_KEY,
                               w,
                               "sensitive",
                               GSettingsBindFlags(G_SETTINGS_BIND_GET |
						  G_SETTINGS_BIND_NO_SENSITIVITY));
  profile_prefs_settings_bind_with_mapping (profile, TERMINAL_PROFILE_HIGHLIGHT_FOREGROUND_COLOR_KEY,
                                            w,
                                            "rgba",
                                            GSettingsBindFlags(G_SETTINGS_BIND_GET |
							       G_SETTINGS_BIND_SET |
							       G_SETTINGS_BIND_NO_SENSITIVITY),
                                            (GSettingsBindGetMapping) s_to_rgba,
                                            (GSettingsBindSetMapping) rgba_to_s,
                                            nullptr, nullptr);

  w = GTK_WIDGET (gtk_builder_get_object (builder, "highlight-background-colorpicker"));
  profile_prefs_settings_bind (profile, TERMINAL_PROFILE_HIGHLIGHT_COLORS_SET_KEY,
                               w,
                               "sensitive",
                               GSettingsBindFlags(G_SETTINGS_BIND_GET |
						  G_SETTINGS_BIND_NO_SENSITIVITY));
  profile_prefs_settings_bind_with_mapping (profile, TERMINAL_PROFILE_HIGHLIGHT_BACKGROUND_COLOR_KEY,
                                            w,
                                            "rgba",
                                            GSettingsBindFlags(G_SETTINGS_BIND_GET |
							       G_SETTINGS_BIND_SET |
							       G_SETTINGS_BIND_NO_SENSITIVITY),
                                            (GSettingsBindGetMapping) s_to_rgba,
                                            (GSettingsBindSetMapping) rgba_to_s,
                                            nullptr, nullptr);

  profile_prefs_settings_bind_with_mapping (profile, TERMINAL_PROFILE_CURSOR_SHAPE_KEY,
                                            gtk_builder_get_object (builder,
                                                                    "cursor-shape-combobox"),
                                            "active",
                                            GSettingsBindFlags(G_SETTINGS_BIND_GET |
							       G_SETTINGS_BIND_SET),
                                            (GSettingsBindGetMapping) string_to_enum,
                                            (GSettingsBindSetMapping) enum_to_string,
                                            vte_cursor_shape_get_type, nullptr);
  profile_prefs_settings_bind_with_mapping (profile, TERMINAL_PROFILE_CURSOR_BLINK_MODE_KEY,
                                            gtk_builder_get_object (builder,
                                                                    "cursor-blink-mode-combobox"),
                                            "active",
                                            GSettingsBindFlags(G_SETTINGS_BIND_GET |
							       G_SETTINGS_BIND_SET),
                                            (GSettingsBindGetMapping) string_to_enum,
                                            (GSettingsBindSetMapping) enum_to_string,
                                            vte_cursor_blink_mode_get_type, nullptr);
  profile_prefs_settings_bind_with_mapping (profile, TERMINAL_PROFILE_TEXT_BLINK_MODE_KEY,
                                            gtk_builder_get_object (builder,
                                                                    "text-blink-mode-combobox"),
                                            "active",
                                            GSettingsBindFlags(G_SETTINGS_BIND_GET |
							       G_SETTINGS_BIND_SET),
                                            (GSettingsBindGetMapping) string_to_enum,
                                            (GSettingsBindSetMapping) enum_to_string,
                                            vte_text_blink_mode_get_type, nullptr);

  profile_prefs_settings_bind (profile, TERMINAL_PROFILE_CUSTOM_COMMAND_KEY,
                               gtk_builder_get_object (builder,
                                                       "custom-command-entry"),
                               "text",
			       GSettingsBindFlags(G_SETTINGS_BIND_GET |
						  G_SETTINGS_BIND_SET));

  w = GTK_WIDGET (gtk_builder_get_object (builder, "default-size-columns-spinbutton"));
  profile_prefs_settings_bind (profile, TERMINAL_PROFILE_DEFAULT_SIZE_COLUMNS_KEY,
                               gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (w)),
                               "value",
			       GSettingsBindFlags(G_SETTINGS_BIND_GET |
						  G_SETTINGS_BIND_SET));

  w = GTK_WIDGET (gtk_builder_get_object (builder, "default-size-rows-spinbutton"));
  profile_prefs_settings_bind (profile, TERMINAL_PROFILE_DEFAULT_SIZE_ROWS_KEY,
                               gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (w)),
                               "value",
			       GSettingsBindFlags(G_SETTINGS_BIND_GET |
						  G_SETTINGS_BIND_SET));

  profile_prefs_settings_bind_with_mapping (profile, TERMINAL_PROFILE_DELETE_BINDING_KEY,
                                            gtk_builder_get_object (builder,
                                                                    "delete-binding-combobox"),
                                            "active",
                                            GSettingsBindFlags(G_SETTINGS_BIND_GET |
							       G_SETTINGS_BIND_SET),
                                            (GSettingsBindGetMapping) string_to_enum,
                                            (GSettingsBindSetMapping) enum_to_string,
                                            vte_erase_binding_get_type, nullptr);
  profile_prefs_settings_bind_with_mapping (profile, TERMINAL_PROFILE_EXIT_ACTION_KEY,
                                            gtk_builder_get_object (builder,
                                                                    "exit-action-combobox"),
                                            "active",
                                            GSettingsBindFlags(G_SETTINGS_BIND_GET |
							       G_SETTINGS_BIND_SET),
                                            (GSettingsBindGetMapping) string_to_enum,
                                            (GSettingsBindSetMapping) enum_to_string,
                                            terminal_exit_action_get_type, nullptr);
  w = (GtkWidget*) gtk_builder_get_object (builder, "font-selector");
  gtk_font_chooser_set_filter_func (GTK_FONT_CHOOSER (w), monospace_filter, nullptr, nullptr);
#if GTK_CHECK_VERSION (3, 24, 0)
  gtk_font_chooser_set_level (GTK_FONT_CHOOSER (w),
			      GtkFontChooserLevel(GTK_FONT_CHOOSER_LEVEL_FAMILY |
						  GTK_FONT_CHOOSER_LEVEL_SIZE));
#endif

  profile_prefs_settings_bind (profile, TERMINAL_PROFILE_FONT_KEY,
                               w,
                               "font-name",
			       GSettingsBindFlags(G_SETTINGS_BIND_GET |
						  G_SETTINGS_BIND_SET));

  profile_prefs_settings_bind_with_mapping (profile,
                                            TERMINAL_PROFILE_FOREGROUND_COLOR_KEY,
                                            gtk_builder_get_object (builder,
                                                                    "foreground-colorpicker"),
                                            "rgba",
                                            GSettingsBindFlags(G_SETTINGS_BIND_GET |
							       G_SETTINGS_BIND_SET),
                                            (GSettingsBindGetMapping) s_to_rgba,
                                            (GSettingsBindSetMapping) rgba_to_s,
                                            nullptr, nullptr);

  profile_prefs_settings_bind (profile, TERMINAL_PROFILE_LOGIN_SHELL_KEY,
                               gtk_builder_get_object (builder,
                                                       "login-shell-checkbutton"),
                               "active",
			       GSettingsBindFlags(G_SETTINGS_BIND_GET |
						  G_SETTINGS_BIND_SET));

  w = GTK_WIDGET (gtk_builder_get_object (builder, "scrollback-lines-spinbutton"));
  profile_prefs_settings_bind (profile, TERMINAL_PROFILE_SCROLLBACK_LINES_KEY,
                               gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (w)),
                               "value",
			       GSettingsBindFlags(G_SETTINGS_BIND_GET |
						  G_SETTINGS_BIND_SET));

  profile_prefs_settings_bind (profile, TERMINAL_PROFILE_SCROLLBACK_UNLIMITED_KEY,
                               gtk_builder_get_object (builder,
                                                       "scrollback-limited-checkbutton"),
                               "active",
                               GSettingsBindFlags(G_SETTINGS_BIND_GET |
						  G_SETTINGS_BIND_SET |
						  G_SETTINGS_BIND_INVERT_BOOLEAN));
  profile_prefs_settings_bind (profile, TERMINAL_PROFILE_SCROLLBACK_UNLIMITED_KEY,
                               gtk_builder_get_object (builder,
                                                       "scrollback-box"),
                               "sensitive",
                               GSettingsBindFlags(G_SETTINGS_BIND_GET |
						  G_SETTINGS_BIND_INVERT_BOOLEAN |
						  G_SETTINGS_BIND_NO_SENSITIVITY));
  profile_prefs_settings_bind_with_mapping (profile,
                                            TERMINAL_PROFILE_SCROLLBAR_POLICY_KEY,
                                            gtk_builder_get_object (builder,
                                                                    "scrollbar-checkbutton"),
                                            "active",
                                            GSettingsBindFlags(G_SETTINGS_BIND_GET |
							       G_SETTINGS_BIND_SET),
                                            (GSettingsBindGetMapping) scrollbar_policy_to_bool,
                                            (GSettingsBindSetMapping) bool_to_scrollbar_policy,
                                            nullptr, nullptr);
  profile_prefs_settings_bind (profile, TERMINAL_PROFILE_SCROLL_ON_KEYSTROKE_KEY,
                               gtk_builder_get_object (builder,
                                                       "scroll-on-keystroke-checkbutton"),
                               "active",
			       GSettingsBindFlags(G_SETTINGS_BIND_GET |
						  G_SETTINGS_BIND_SET));
  profile_prefs_settings_bind (profile, TERMINAL_PROFILE_SCROLL_ON_OUTPUT_KEY,
                               gtk_builder_get_object (builder,
                                                       "scroll-on-output-checkbutton"),
                               "active",
			       GSettingsBindFlags(G_SETTINGS_BIND_GET |
						  G_SETTINGS_BIND_SET));
  profile_prefs_settings_bind (profile, TERMINAL_PROFILE_SCROLL_ON_INSERT_KEY,
                               gtk_builder_get_object (builder,
                                                       "scroll-on-insert-checkbutton"),
                               "active",
			       GSettingsBindFlags(G_SETTINGS_BIND_GET |
						  G_SETTINGS_BIND_SET));
  profile_prefs_settings_bind (profile, TERMINAL_PROFILE_USE_SYSTEM_FONT_KEY,
                               gtk_builder_get_object (builder,
                                                       "custom-font-checkbutton"),
                               "active",
                               GSettingsBindFlags(G_SETTINGS_BIND_GET |
						  G_SETTINGS_BIND_SET |
						  G_SETTINGS_BIND_INVERT_BOOLEAN));

  w = (GtkWidget *) gtk_builder_get_object (builder, "preserve-working-directory-combobox");
  profile_prefs_settings_bind_with_mapping (profile, TERMINAL_PROFILE_PRESERVE_WORKING_DIRECTORY_KEY, w,
                                            "active",
                                            GSettingsBindFlags(G_SETTINGS_BIND_GET |
							       G_SETTINGS_BIND_SET),
                                            (GSettingsBindGetMapping) string_to_enum,
                                            (GSettingsBindSetMapping) enum_to_string,
                                            terminal_preserve_working_directory_get_type, nullptr);

  profile_prefs_settings_bind (profile, TERMINAL_PROFILE_USE_CUSTOM_COMMAND_KEY,
                               gtk_builder_get_object (builder,
                                                       "use-custom-command-checkbutton"),
                               "active",
			       GSettingsBindFlags(G_SETTINGS_BIND_GET |
						  G_SETTINGS_BIND_SET));

  profile_prefs_settings_bind (profile, TERMINAL_PROFILE_USE_THEME_COLORS_KEY,
                               gtk_builder_get_object (builder,
                                                       "use-theme-colors-checkbutton"),
                               "active",
			       GSettingsBindFlags(G_SETTINGS_BIND_GET |
						  G_SETTINGS_BIND_SET));
  profile_prefs_settings_bind (profile, TERMINAL_PROFILE_AUDIBLE_BELL_KEY,
                               gtk_builder_get_object (builder, "bell-checkbutton"),
                               "active",
                               GSettingsBindFlags(G_SETTINGS_BIND_GET |
						  G_SETTINGS_BIND_SET));

  profile_prefs_settings_bind (profile,
                               TERMINAL_PROFILE_USE_CUSTOM_COMMAND_KEY,
                               gtk_builder_get_object (builder, "custom-command-entry-label"),
                               "sensitive",
                               GSettingsBindFlags(G_SETTINGS_BIND_GET |
						  G_SETTINGS_BIND_NO_SENSITIVITY));
  profile_prefs_settings_bind (profile,
                               TERMINAL_PROFILE_USE_CUSTOM_COMMAND_KEY,
                               gtk_builder_get_object (builder, "custom-command-entry"),
                               "sensitive",
                               GSettingsBindFlags(G_SETTINGS_BIND_GET |
						  G_SETTINGS_BIND_NO_SENSITIVITY));
  profile_prefs_settings_bind (profile,
                               TERMINAL_PROFILE_USE_SYSTEM_FONT_KEY,
                               gtk_builder_get_object (builder, "font-selector"),
                               "sensitive",
                               GSettingsBindFlags(G_SETTINGS_BIND_GET |
						  G_SETTINGS_BIND_INVERT_BOOLEAN |
						  G_SETTINGS_BIND_NO_SENSITIVITY));
  profile_prefs_settings_bind (profile,
                               TERMINAL_PROFILE_USE_THEME_COLORS_KEY,
                               gtk_builder_get_object (builder, "colors-box"),
                               "sensitive",
                               GSettingsBindFlags(G_SETTINGS_BIND_GET |
						  G_SETTINGS_BIND_INVERT_BOOLEAN |
						  G_SETTINGS_BIND_NO_SENSITIVITY));
  profile_prefs_settings_bind_writable (profile,
                                        TERMINAL_PROFILE_PALETTE_KEY,
                                        gtk_builder_get_object (builder, "palette-box"),
                                        "sensitive",
                                        FALSE);

  /* Scrolling options */
  w = (GtkWidget *) gtk_builder_get_object (builder, "scrollback-warning");
  profile_scrollback_warning_update_cb (profile, nullptr, w);
  profile_prefs_signal_connect (profile, "changed::" TERMINAL_PROFILE_SCROLLBACK_UNLIMITED_KEY,
                                G_CALLBACK (profile_scrollback_warning_update_cb),
                                w);
  profile_prefs_signal_connect (profile, "changed::" TERMINAL_PROFILE_SCROLLBACK_LINES_KEY,
                                G_CALLBACK (profile_scrollback_warning_update_cb),
                                w);

  /* Compatibility options */
  w = (GtkWidget *) gtk_builder_get_object (builder, "encoding-combobox");
  profile_prefs_signal_connect (w, "changed",
                                G_CALLBACK (profile_encoding_combo_changed_cb),
                                profile);

  profile_notify_encoding_combo_cb (profile, TERMINAL_PROFILE_ENCODING_KEY, GTK_COMBO_BOX (w));
  profile_prefs_signal_connect (profile, "changed::" TERMINAL_PROFILE_ENCODING_KEY,
                                G_CALLBACK (profile_notify_encoding_combo_cb),
                                w);

  w = (GtkWidget *) gtk_builder_get_object (builder, "cjk-ambiguous-width-combobox");
  profile_prefs_settings_bind (profile, TERMINAL_PROFILE_CJK_UTF8_AMBIGUOUS_WIDTH_KEY,
                               w,
                               "active-id",
                               GSettingsBindFlags(G_SETTINGS_BIND_GET |
						  G_SETTINGS_BIND_SET));

  w = (GtkWidget *) gtk_builder_get_object (builder, "enable-sixel-checkbutton");
  profile_prefs_settings_bind (profile, TERMINAL_PROFILE_ENABLE_SIXEL_KEY, w,
                               "active",
			       GSettingsBindFlags(G_SETTINGS_BIND_GET |
						  G_SETTINGS_BIND_SET));
  gtk_widget_set_visible (w, (vte_get_feature_flags() & VTE_FEATURE_FLAG_SIXEL) != 0);
}

/* Called once per Preferences window, to destroy stuff that doesn't depend on the profile being edited */
void
profile_prefs_destroy (void)
{
  profile_prefs_unload ();

  g_array_free (the_pref_data->profile_signals, TRUE);
  g_array_free (the_pref_data->profile_bindings, TRUE);
}
