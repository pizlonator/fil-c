/* GTK - The GIMP Toolkit
 *
 * Copyright (C) 2012 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "deprecated/gtkcolorchooserprivate.h"
#include "deprecated/gtkcolorchooserwidget.h"
#include "gtkcolorchooserwidgetprivate.h"
#include "gtkcoloreditorprivate.h"
#include "gtkcolorswatchprivate.h"
#include "gtkgrid.h"
#include "gtklabel.h"
#include "gtkorientable.h"
#include "gtkprivate.h"
#include <glib/gi18n-lib.h>
#include "gtksizegroup.h"
#include "gtkboxlayout.h"
#include "gtkwidgetprivate.h"
#include "gdkrgbaprivate.h"

#include <math.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

/**
 * GtkColorChooserWidget:
 *
 * The `GtkColorChooserWidget` widget lets the user select a color.
 *
 * By default, the chooser presents a predefined palette of colors,
 * plus a small number of settable custom colors. It is also possible
 * to select a different color with the single-color editor.
 *
 * To enter the single-color editing mode, use the context menu of any
 * color of the palette, or use the '+' button to add a new custom color.
 *
 * The chooser automatically remembers the last selection, as well
 * as custom colors.
 *
 * To create a `GtkColorChooserWidget`, use [ctor@Gtk.ColorChooserWidget.new].
 *
 * To change the initially selected color, use
 * [method@Gtk.ColorChooser.set_rgba]. To get the selected color use
 * [method@Gtk.ColorChooser.get_rgba].
 *
 * The `GtkColorChooserWidget` is used in the [class@Gtk.ColorChooserDialog]
 * to provide a dialog for selecting colors.
 *
 * # CSS names
 *
 * `GtkColorChooserWidget` has a single CSS node with name colorchooser.
 *
 * Deprecated: 4.10: Direct use of `GtkColorChooserWidget` is deprecated.
 */

typedef struct _GtkColorChooserWidgetClass   GtkColorChooserWidgetClass;

struct _GtkColorChooserWidget
{
  GtkWidget parent_instance;

  GtkWidget *palette;
  GtkWidget *editor;
  GtkSizeGroup *size_group;

  GtkWidget *custom_label;
  GtkWidget *custom;

  GtkWidget *button;
  GtkColorSwatch *current;

  gboolean use_alpha;
  gboolean has_default_palette;

  GSettings *settings;

  int max_custom;
};

struct _GtkColorChooserWidgetClass
{
  GtkWidgetClass parent_class;
};

enum
{
  PROP_ZERO,
  PROP_RGBA,
  PROP_USE_ALPHA,
  PROP_SHOW_EDITOR
};

static void gtk_color_chooser_widget_iface_init (GtkColorChooserInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkColorChooserWidget, gtk_color_chooser_widget, GTK_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_COLOR_CHOOSER,
                                                gtk_color_chooser_widget_iface_init))

static void
select_swatch (GtkColorChooserWidget *cc,
               GtkColorSwatch        *swatch)
{
  GdkRGBA color;
  double red, green, blue, alpha;

  if (cc->current == swatch)
    return;

  if (cc->current != NULL)
    gtk_widget_unset_state_flags (GTK_WIDGET (cc->current), GTK_STATE_FLAG_SELECTED);

  gtk_widget_set_state_flags (GTK_WIDGET (swatch), GTK_STATE_FLAG_SELECTED, FALSE);
  cc->current = swatch;

  gtk_color_swatch_get_rgba (swatch, &color);

  red = color.red;
  green = color.green;
  blue = color.blue;
  alpha = color.alpha;
  g_settings_set (cc->settings, "selected-color", "(bdddd)",
                  TRUE, red, green, blue, alpha);

  if (gtk_widget_get_visible (GTK_WIDGET (cc->editor)))
    gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (cc->editor), &color);
  else
    g_object_notify (G_OBJECT (cc), "rgba");
}

static void
swatch_selected (GtkColorSwatch        *swatch,
                 GtkStateFlags          previous,
                 GtkColorChooserWidget *cc)
{
  GtkStateFlags flags;

  flags = gtk_widget_get_state_flags (GTK_WIDGET (swatch));
  if ((flags & GTK_STATE_FLAG_SELECTED) != (previous & GTK_STATE_FLAG_SELECTED) &&
      (flags & GTK_STATE_FLAG_SELECTED) != 0)
    select_swatch (cc, swatch);
}

static void
connect_swatch_signals (GtkWidget *p,
                        gpointer   data)
{
  g_signal_connect (p, "state-flags-changed", G_CALLBACK (swatch_selected), data);
}

static void
connect_button_signals (GtkWidget *p,
                        gpointer   data)
{
//  g_signal_connect (p, "activate", G_CALLBACK (button_activate), data);
}

static void
save_custom_colors (GtkColorChooserWidget *cc)
{
  GVariantBuilder builder;
  GVariant *variant;
  GdkRGBA color;
  GtkWidget *child;
  gboolean first;

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(dddd)"));

  for (child = gtk_widget_get_first_child (cc->custom), first = TRUE;
       child != NULL;
       child = gtk_widget_get_next_sibling (child), first = FALSE)
    {
      if (first)
        continue;

      if (gtk_color_swatch_get_rgba (GTK_COLOR_SWATCH (child), &color))
        {
          double red, green, blue, alpha;

          red = color.red;
          green = color.green;
          blue = color.blue;
          alpha = color.alpha;
          g_variant_builder_add (&builder, "(dddd)", red, green, blue, alpha);
        }
    }

  variant = g_variant_builder_end (&builder);
  g_settings_set_value (cc->settings, "custom-colors", variant);
}

static void
connect_custom_signals (GtkWidget *p,
                        gpointer   data)
{
  connect_swatch_signals (p, data);
  g_signal_connect_swapped (p, "notify::rgba",
                            G_CALLBACK (save_custom_colors), data);
}

static void
gtk_color_chooser_widget_set_use_alpha (GtkColorChooserWidget *cc,
                                        gboolean               use_alpha)
{
  GtkWidget *child;
  GtkWidget *grid;

  if (cc->use_alpha == use_alpha)
    return;

  cc->use_alpha = use_alpha;
  gtk_color_chooser_set_use_alpha (GTK_COLOR_CHOOSER (cc->editor), use_alpha);

  for (grid = gtk_widget_get_first_child (cc->palette);
       grid != NULL;
       grid = gtk_widget_get_next_sibling (grid))
    {
      for (child = gtk_widget_get_first_child (grid);
           child != NULL;
           child = gtk_widget_get_next_sibling (child))
        {
          if (GTK_IS_COLOR_SWATCH (child))
            gtk_color_swatch_set_use_alpha (GTK_COLOR_SWATCH (child), use_alpha);
        }
    }

  gtk_widget_queue_draw (GTK_WIDGET (cc));
  g_object_notify (G_OBJECT (cc), "use-alpha");
}

static void
gtk_color_chooser_widget_set_show_editor (GtkColorChooserWidget *cc,
                                          gboolean               show_editor)
{
  if (show_editor)
    {
      GdkRGBA color = { 0.75, 0.25, 0.25, 1.0 };

      if (cc->current)
        gtk_color_swatch_get_rgba (cc->current, &color);
      gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (cc->editor), &color);
    }

  gtk_widget_set_visible (cc->editor, show_editor);
  gtk_widget_set_visible (cc->palette, !show_editor);
}

static void
update_from_editor (GtkColorEditor        *editor,
                    GParamSpec            *pspec,
                    GtkColorChooserWidget *widget)
{
  if (gtk_widget_get_visible (GTK_WIDGET (editor)))
    g_object_notify (G_OBJECT (widget), "rgba");
}

/* UI construction {{{1 */

static void
remove_palette (GtkColorChooserWidget *cc)
{
  GList *children, *l;
  GtkWidget *widget;

  if (cc->current != NULL &&
      gtk_widget_get_parent (GTK_WIDGET (cc->current)) != cc->custom)
    cc->current = NULL;

  children = NULL;
  for (widget = gtk_widget_get_first_child (cc->palette);
       widget != NULL;
       widget = gtk_widget_get_next_sibling (widget))
    children = g_list_prepend (children, widget);

  for (l = children; l; l = l->next)
    {
      widget = l->data;
      if (widget == cc->custom_label || widget == cc->custom)
        continue;
      gtk_box_remove (GTK_BOX (cc->palette), widget);
    }
  g_list_free (children);
}

static guint
scale_round (double value,
             double scale)
{
  value = floor (value * scale + 0.5);
  value = MAX (value, 0);
  value = MIN (value, scale);
  return (guint)value;
}

char *
accessible_color_name (const GdkRGBA *color)
{
  if (color->alpha < 1.0)
    return g_strdup_printf (_("Red %d%%, Green %d%%, Blue %d%%, Alpha %d%%"),
                            scale_round (color->red, 100),
                            scale_round (color->green, 100),
                            scale_round (color->blue, 100),
                            scale_round (color->alpha, 100));
  else
    return g_strdup_printf (_("Red %d%%, Green %d%%, Blue %d%%"),
                            scale_round (color->red, 100),
                            scale_round (color->green, 100),
                            scale_round (color->blue, 100));
}

static void
add_palette (GtkColorChooserWidget  *cc,
             GtkOrientation          orientation,
             int                     colors_per_line,
             int                     n_colors,
             GdkRGBA                *colors,
             const char            **names)
{
  GtkWidget *grid;
  GtkWidget *p;
  int line, pos;
  int i;
  int left, right;

  if (colors == NULL)
    {
      remove_palette (cc);
      return;
    }

  grid = gtk_grid_new ();
  gtk_widget_set_margin_bottom (grid, 12);
  gtk_grid_set_row_spacing (GTK_GRID (grid), 2);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 4);
  gtk_box_append (GTK_BOX (cc->palette), grid);

  left = 0;
  right = colors_per_line - 1;
  if (gtk_widget_get_direction (GTK_WIDGET (cc)) == GTK_TEXT_DIR_RTL)
    {
      i = left;
      left = right;
      right = i;
    }

  for (i = 0; i < n_colors; i++)
    {
      p = gtk_color_swatch_new ();
      if (names)
        {
          gtk_accessible_update_property (GTK_ACCESSIBLE (p),
                                          GTK_ACCESSIBLE_PROPERTY_LABEL,
                                          g_dpgettext2 (GETTEXT_PACKAGE, "Color name", names[i]),
                                          -1);
        }
      else
        {
          char *name;
          char *text;

          name = accessible_color_name (&colors[i]);
          text = g_strdup_printf (_("Color: %s"), name);
          gtk_accessible_update_property (GTK_ACCESSIBLE (p),
                                          GTK_ACCESSIBLE_PROPERTY_LABEL, text,
                                          -1);
          g_free (name);
          g_free (text);
        }
      gtk_color_swatch_set_rgba (GTK_COLOR_SWATCH (p), &colors[i]);
      connect_swatch_signals (p, cc);

      line = i / colors_per_line;
      pos = i % colors_per_line;

      if (orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          if (pos == left)
            gtk_widget_add_css_class (p, "left");
          else if (pos == right)
            gtk_widget_add_css_class (p, "right");

          gtk_grid_attach (GTK_GRID (grid), p, pos, line, 1, 1);
        }
      else
        {
          if (pos == 0)
            gtk_widget_add_css_class (p, "top");
          else if (pos == colors_per_line - 1)
            gtk_widget_add_css_class (p, "bottom");

          gtk_grid_attach (GTK_GRID (grid), p, line, pos, 1, 1);
       }
    }

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    cc->max_custom = MAX (cc->max_custom, colors_per_line);
  else
    cc->max_custom = MAX (cc->max_custom, n_colors / colors_per_line);
}

static void
remove_default_palette (GtkColorChooserWidget *cc)
{
  if (!cc->has_default_palette)
    return;

  remove_palette (cc);
  cc->has_default_palette = FALSE;
  cc->max_custom = 0;
}

static void
add_default_palette (GtkColorChooserWidget *cc)
{
  GdkRGBA colors[9*5] = {
    GDK_RGBA("99c1f1"), GDK_RGBA("62a0ea"), GDK_RGBA("3584e4"), GDK_RGBA("1c71d8"), GDK_RGBA("1a5fb4"), /* Blue */
    GDK_RGBA("8ff0a4"), GDK_RGBA("57e389"), GDK_RGBA("33d17a"), GDK_RGBA("2ec27e"), GDK_RGBA("26a269"), /* Green */
    GDK_RGBA("f9f06b"), GDK_RGBA("f8e45c"), GDK_RGBA("f6d32d"), GDK_RGBA("f5c211"), GDK_RGBA("e5a50a"), /* Yellow */
    GDK_RGBA("ffbe6f"), GDK_RGBA("ffa348"), GDK_RGBA("ff7800"), GDK_RGBA("e66100"), GDK_RGBA("c64600"), /* Orange */
    GDK_RGBA("f66151"), GDK_RGBA("ed333b"), GDK_RGBA("e01b24"), GDK_RGBA("c01c28"), GDK_RGBA("a51d2d"), /* Red */
    GDK_RGBA("dc8add"), GDK_RGBA("c061cb"), GDK_RGBA("9141ac"), GDK_RGBA("813d9c"), GDK_RGBA("613583"), /* Purple */
    GDK_RGBA("cdab8f"), GDK_RGBA("b5835a"), GDK_RGBA("986a44"), GDK_RGBA("865e3c"), GDK_RGBA("63452c"), /* Brown */
    GDK_RGBA("ffffff"), GDK_RGBA("f6f5f4"), GDK_RGBA("deddda"), GDK_RGBA("c0bfbc"), GDK_RGBA("9a9996"), /* Light */
    GDK_RGBA("77767b"), GDK_RGBA("5e5c64"), GDK_RGBA("3d3846"), GDK_RGBA("241f31"), GDK_RGBA("000000")  /* Dark */
  };
  const char *color_names[] = {
    NC_("Color name", "Very Light Blue"),
    NC_("Color name", "Light Blue"),
    NC_("Color name", "Blue"),
    NC_("Color name", "Dark Blue"),
    NC_("Color name", "Very Dark Blue"),
    NC_("Color name", "Very Light Green"),
    NC_("Color name", "Light Green"),
    NC_("Color name", "Green"),
    NC_("Color name", "Dark Green"),
    NC_("Color name", "Very Dark Green"),
    NC_("Color name", "Very Light Yellow"),
    NC_("Color name", "Light Yellow"),
    NC_("Color name", "Yellow"),
    NC_("Color name", "Dark Yellow"),
    NC_("Color name", "Very Dark Yellow"),
    NC_("Color name", "Very Light Orange"),
    NC_("Color name", "Light Orange"),
    NC_("Color name", "Orange"),
    NC_("Color name", "Dark Orange"),
    NC_("Color name", "Very Dark Orange"),
    NC_("Color name", "Very Light Red"),
    NC_("Color name", "Light Red"),
    NC_("Color name", "Red"),
    NC_("Color name", "Dark Red"),
    NC_("Color name", "Very Dark Red"),
    NC_("Color name", "Very Light Purple"),
    NC_("Color name", "Light Purple"),
    NC_("Color name", "Purple"),
    NC_("Color name", "Dark Purple"),
    NC_("Color name", "Very Dark Purple"),
    NC_("Color name", "Very Light Brown"),
    NC_("Color name", "Light Brown"),
    NC_("Color name", "Brown"),
    NC_("Color name", "Dark Brown"),
    NC_("Color name", "Very Dark Brown"),
    NC_("Color name", "White"),
    NC_("Color name", "Light Gray 1"),
    NC_("Color name", "Light Gray 2"),
    NC_("Color name", "Light Gray 3"),
    NC_("Color name", "Light Gray 4"),
    NC_("Color name", "Dark Gray 1"),
    NC_("Color name", "Dark Gray 2"),
    NC_("Color name", "Dark Gray 3"),
    NC_("Color name", "Dark Gray 4"),
    NC_("Color name", "Black"),
  };

  add_palette (cc, GTK_ORIENTATION_VERTICAL, 5, 9*5, colors, color_names);

  cc->has_default_palette = TRUE;
}

static void
gtk_color_chooser_widget_activate_color_customize (GtkWidget  *widget,
                                                   const char *name,
                                                   GVariant   *parameter)
{
  GtkColorChooserWidget *cc = GTK_COLOR_CHOOSER_WIDGET (widget);
  double red, green, blue, alpha;
  GdkRGBA color;

  g_variant_get (parameter, "(dddd)", &red, &green, &blue, &alpha);
  color.red = red;
  color.green = green;
  color.blue = blue;
  color.alpha = alpha;

  gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (cc->editor), &color);

  gtk_widget_set_visible (cc->palette, FALSE);
  gtk_widget_set_visible (cc->editor, TRUE);
  g_object_notify (G_OBJECT (cc), "show-editor");
}

static void
gtk_color_chooser_widget_activate_color_select (GtkWidget  *widget,
                                                const char *name,
                                                GVariant   *parameter)
{
  GtkColorChooserWidget *cc = GTK_COLOR_CHOOSER_WIDGET (widget);
  GdkRGBA color;
  double red, green, blue, alpha;

  g_variant_get (parameter, "(dddd)", &red, &green, &blue, &alpha);
  color.red = red;
  color.green = green;
  color.blue = blue;
  color.alpha = alpha;

  _gtk_color_chooser_color_activated (GTK_COLOR_CHOOSER (cc), &color);
}

static void
gtk_color_chooser_widget_init (GtkColorChooserWidget *cc)
{
  GtkWidget *box;
  GtkWidget *p;
  GtkWidget *button;
  GtkWidget *label;
  int i;
  double color[4];
  GdkRGBA rgba;
  GVariant *variant;
  GVariantIter iter;
  gboolean selected;
  char *name;
  char *text;

  cc->use_alpha = TRUE;

  cc->palette = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_parent (cc->palette, GTK_WIDGET (cc));

  add_default_palette (cc);

  /* translators: label for the custom section in the color chooser */
  cc->custom_label = label = gtk_label_new (_("Custom"));
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_box_append (GTK_BOX (cc->palette), label);

  cc->custom = box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
  g_object_set (box, "margin-top", 12, NULL);
  gtk_box_append (GTK_BOX (cc->palette), box);

  cc->button = button = gtk_color_swatch_new ();
  gtk_widget_set_name (button, "add-color-button");
  connect_button_signals (button, cc);
  gtk_color_swatch_set_icon (GTK_COLOR_SWATCH (button), "list-add-symbolic");
  gtk_color_swatch_set_selectable (GTK_COLOR_SWATCH (button), FALSE);
  gtk_accessible_update_property (GTK_ACCESSIBLE (button),
                                  GTK_ACCESSIBLE_PROPERTY_LABEL, _("Add Color"),
                                  -1);
  gtk_box_append (GTK_BOX (box), button);

  cc->settings = g_settings_new ("org.gtk.gtk4.Settings.ColorChooser");
  variant = g_settings_get_value (cc->settings, I_("custom-colors"));
  g_variant_iter_init (&iter, variant);
  i = 0;
  p = NULL;
  while (g_variant_iter_loop (&iter, "(dddd)", &color[0], &color[1], &color[2], &color[3]))
    {
      i++;
      p = gtk_color_swatch_new ();

      rgba.red = color[0];
      rgba.green = color[1];
      rgba.blue = color[2];
      rgba.alpha = color[3];

      gtk_color_swatch_set_rgba (GTK_COLOR_SWATCH (p), &rgba);

      name = accessible_color_name (&rgba);
      text = g_strdup_printf (_("Custom color %d: %s"), i, name);
      gtk_accessible_update_property (GTK_ACCESSIBLE (p),
                                      GTK_ACCESSIBLE_PROPERTY_LABEL, text,
                                      -1);
      g_free (name);
      g_free (text);

      gtk_color_swatch_set_can_drop (GTK_COLOR_SWATCH (p), TRUE);
      connect_custom_signals (p, cc);
      gtk_box_append (GTK_BOX (box), p);

      if (i == 8)
        break;
    }
  g_variant_unref (variant);

  cc->editor = gtk_color_editor_new ();
  gtk_widget_set_halign (cc->editor, GTK_ALIGN_CENTER);
  gtk_widget_set_hexpand (cc->editor, TRUE);
  g_signal_connect (cc->editor, "notify::rgba",
                    G_CALLBACK (update_from_editor), cc);

  gtk_widget_set_parent (cc->editor, GTK_WIDGET (cc));

  g_settings_get (cc->settings, I_("selected-color"), "(bdddd)",
                  &selected,
                  &color[0], &color[1], &color[2], &color[3]);
  if (selected)
    {
      rgba.red = color[0];
      rgba.green = color[1];
      rgba.blue = color[2];
      rgba.alpha = color[3];
      gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (cc), &rgba);
    }

  gtk_widget_set_visible (GTK_WIDGET (cc->editor), FALSE);
}

/* GObject implementation {{{1 */

static void
gtk_color_chooser_widget_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  GtkColorChooserWidget *cc = GTK_COLOR_CHOOSER_WIDGET (object);

  switch (prop_id)
    {
    case PROP_RGBA:
      {
        GdkRGBA color;

        gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (cc), &color);
        g_value_set_boxed (value, &color);
      }
      break;
    case PROP_USE_ALPHA:
      g_value_set_boolean (value, cc->use_alpha);
      break;
    case PROP_SHOW_EDITOR:
      g_value_set_boolean (value, gtk_widget_get_visible (cc->editor));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_color_chooser_widget_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  GtkColorChooserWidget *cc = GTK_COLOR_CHOOSER_WIDGET (object);

  switch (prop_id)
    {
    case PROP_RGBA:
      gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (cc),
                                  g_value_get_boxed (value));
      break;
    case PROP_USE_ALPHA:
      gtk_color_chooser_widget_set_use_alpha (cc,
                                              g_value_get_boolean (value));
      break;
    case PROP_SHOW_EDITOR:
      gtk_color_chooser_widget_set_show_editor (cc,
                                                g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_color_chooser_widget_finalize (GObject *object)
{
  GtkColorChooserWidget *cc = GTK_COLOR_CHOOSER_WIDGET (object);

  g_object_unref (cc->settings);

  gtk_widget_unparent (cc->editor);
  gtk_widget_unparent (cc->palette);

  G_OBJECT_CLASS (gtk_color_chooser_widget_parent_class)->finalize (object);
}

static void
gtk_color_chooser_widget_class_init (GtkColorChooserWidgetClass *class)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->get_property = gtk_color_chooser_widget_get_property;
  object_class->set_property = gtk_color_chooser_widget_set_property;
  object_class->finalize = gtk_color_chooser_widget_finalize;

  widget_class->grab_focus = gtk_widget_grab_focus_child;
  widget_class->focus = gtk_widget_focus_child;

  g_object_class_override_property (object_class, PROP_RGBA, "rgba");
  g_object_class_override_property (object_class, PROP_USE_ALPHA, "use-alpha");

  /**
   * GtkColorChooserWidget:show-editor:
   *
   * %TRUE when the color chooser is showing the single-color editor.
   *
   * It can be set to switch the color chooser into single-color editing mode.
   */
  g_object_class_install_property (object_class, PROP_SHOW_EDITOR,
      g_param_spec_boolean ("show-editor", NULL, NULL,
                            FALSE, GTK_PARAM_READWRITE));

  gtk_widget_class_set_css_name (widget_class, I_("colorchooser"));
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BOX_LAYOUT);

  /**
   * GtkColorChooserWidget|color.select:
   * @red: the red value, between 0 and 1
   * @green: the green value, between 0 and 1
   * @blue: the blue value, between 0 and 1
   * @alpha: the alpha value, between 0 and 1
   *
   * Emits the [signal@Gtk.ColorChooser::color-activated] signal for
   * the given color.
   */
  gtk_widget_class_install_action (widget_class, "color.select", "(dddd)",
                                   gtk_color_chooser_widget_activate_color_select);

  /**
   * GtkColorChooserWidget|color.customize:
   * @red: the red value, between 0 and 1
   * @green: the green value, between 0 and 1
   * @blue: the blue value, between 0 and 1
   * @alpha: the alpha value, between 0 and 1
   *
   * Activates the color editor for the given color.
   */
  gtk_widget_class_install_action (widget_class, "color.customize", "(dddd)",
                                   gtk_color_chooser_widget_activate_color_customize);
}

/* GtkColorChooser implementation {{{1 */

static void
gtk_color_chooser_widget_get_rgba (GtkColorChooser *chooser,
                                   GdkRGBA         *color)
{
  GtkColorChooserWidget *cc = GTK_COLOR_CHOOSER_WIDGET (chooser);

  if (gtk_widget_get_visible (cc->editor))
    gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (cc->editor), color);
  else if (cc->current)
    gtk_color_swatch_get_rgba (cc->current, color);
  else
    {
      color->red = 1.0;
      color->green = 1.0;
      color->blue = 1.0;
      color->alpha = 1.0;
    }

  if (!cc->use_alpha)
    color->alpha = 1.0;
}

static void
add_custom_color (GtkColorChooserWidget *cc,
                  const GdkRGBA         *color)
{
  GtkWidget *widget;
  GtkWidget *p;
  int n;

  n = 0;
  for (widget = gtk_widget_get_first_child (cc->custom);
       widget != NULL;
       widget = gtk_widget_get_next_sibling (widget))
    n++;

  while (n >= cc->max_custom)
    {
      GtkWidget *last = gtk_widget_get_last_child (cc->custom);

      if (last == (GtkWidget *)cc->current)
        cc->current = NULL;

      gtk_box_remove (GTK_BOX (cc->custom), last);
      n--;
    }

  p = gtk_color_swatch_new ();
  gtk_color_swatch_set_rgba (GTK_COLOR_SWATCH (p), color);
  gtk_color_swatch_set_can_drop (GTK_COLOR_SWATCH (p), TRUE);
  connect_custom_signals (p, cc);

  gtk_box_insert_child_after (GTK_BOX (cc->custom), p, gtk_widget_get_first_child (cc->custom));

  select_swatch (cc, GTK_COLOR_SWATCH (p));
  save_custom_colors (cc);
}

static void
gtk_color_chooser_widget_set_rgba (GtkColorChooser *chooser,
                                   const GdkRGBA   *color)
{
  GtkColorChooserWidget *cc = GTK_COLOR_CHOOSER_WIDGET (chooser);
  GtkWidget *swatch;
  GtkWidget *w;

  GdkRGBA c;

  for (w = gtk_widget_get_first_child (cc->palette);
       w != NULL;
       w = gtk_widget_get_next_sibling (w))
    {
      if (!GTK_IS_GRID (w) && !GTK_IS_BOX (w))
        continue;

      for (swatch = gtk_widget_get_first_child (w);
           swatch != NULL;
           swatch = gtk_widget_get_next_sibling (swatch))
        {
          gtk_color_swatch_get_rgba (GTK_COLOR_SWATCH (swatch), &c);
          if (!cc->use_alpha)
            c.alpha = color->alpha;
          if (gdk_rgba_equal (color, &c))
            {
              select_swatch (cc, GTK_COLOR_SWATCH (swatch));
              return;
            }
        }
    }

  add_custom_color (cc, color);
}

static void
gtk_color_chooser_widget_add_palette (GtkColorChooser *chooser,
                                      GtkOrientation   orientation,
                                      int              colors_per_line,
                                      int              n_colors,
                                      GdkRGBA         *colors)
{
  GtkColorChooserWidget *cc = GTK_COLOR_CHOOSER_WIDGET (chooser);

  remove_default_palette (cc);
  add_palette (cc, orientation, colors_per_line, n_colors, colors, NULL);

  gtk_box_reorder_child_after (GTK_BOX (cc->palette), cc->custom_label, gtk_widget_get_last_child (cc->palette));
  gtk_box_reorder_child_after (GTK_BOX (cc->palette), cc->custom, cc->custom_label);
}

static void
gtk_color_chooser_widget_iface_init (GtkColorChooserInterface *iface)
{
  iface->get_rgba = gtk_color_chooser_widget_get_rgba;
  iface->set_rgba = gtk_color_chooser_widget_set_rgba;
  iface->add_palette = gtk_color_chooser_widget_add_palette;
}
 
 /* Public API {{{1 */

/**
 * gtk_color_chooser_widget_new:
 *
 * Creates a new `GtkColorChooserWidget`.
 *
 * Returns: a new `GtkColorChooserWidget`
 */
GtkWidget *
gtk_color_chooser_widget_new (void)
{
  return g_object_new (GTK_TYPE_COLOR_CHOOSER_WIDGET, NULL);
}

/* vim:set foldmethod=marker: */
