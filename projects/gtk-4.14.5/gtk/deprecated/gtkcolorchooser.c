/* GTK - The GIMP Toolkit
 *
 * Copyright (C) 2012, Red Hat, Inc.
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

#include "gtkcolorchooser.h"
#include "gtkcolorchooserprivate.h"
#include "gtktypebuiltins.h"
#include "gtkprivate.h"
#include "gtksnapshot.h"
#include "gdk/gdkrgbaprivate.h"

/**
 * GtkColorChooser:
 *
 * `GtkColorChooser` is an interface that is implemented by widgets
 * for choosing colors.
 *
 * Depending on the situation, colors may be allowed to have alpha (translucency).
 *
 * In GTK, the main widgets that implement this interface are
 * [class@Gtk.ColorChooserWidget], [class@Gtk.ColorChooserDialog] and
 * [class@Gtk.ColorButton].
 *
 * Deprecated: 4.10: Use [class@Gtk.ColorDialog] and [class@Gtk.ColorDialogButton]
 *   instead of widgets implementing `GtkColorChooser`
 */

enum
{
  COLOR_ACTIVATED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_INTERFACE (GtkColorChooser, gtk_color_chooser, G_TYPE_OBJECT);

static void
gtk_color_chooser_default_init (GtkColorChooserInterface *iface)
{
  /**
   * GtkColorChooser:rgba: (attributes org.gtk.Property.get=gtk_color_chooser_get_rgba org.gtk.Property.set=gtk_color_chooser_set_rgba)
   *
   * The currently selected color, as a `GdkRGBA` struct.
   *
   * The property can be set to change the current selection
   * programmatically.
   *
   * Deprecated: 4.10: Use [class@Gtk.ColorDialog] and [class@Gtk.ColorDialogButton]
   *   instead of widgets implementing `GtkColorChooser`
   */
  g_object_interface_install_property (iface,
      g_param_spec_boxed ("rgba", NULL, NULL,
                          GDK_TYPE_RGBA,
                          GTK_PARAM_READWRITE));

  /**
   * GtkColorChooser:use-alpha: (attributes org.gtk.Property.get=gtk_color_chooser_get_use_alpha org.gtk.Property.set=gtk_color_chooser_set_use_alpha)
   *
   * Whether colors may have alpha (translucency).
   *
   * When ::use-alpha is %FALSE, the `GdkRGBA` struct obtained
   * via the [property@Gtk.ColorChooser:rgba] property will be
   * forced to have alpha == 1.
   *
   * Implementations are expected to show alpha by rendering the color
   * over a non-uniform background (like a checkerboard pattern).
   *
   * Deprecated: 4.10: Use [class@Gtk.ColorDialog] and [class@Gtk.ColorDialogButton]
   *   instead of widgets implementing `GtkColorChooser`
   */
  g_object_interface_install_property (iface,
      g_param_spec_boolean ("use-alpha", NULL, NULL,
                            TRUE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkColorChooser::color-activated:
   * @chooser: the object which received the signal
   * @color: the color
   *
   * Emitted when a color is activated from the color chooser.
   *
   * This usually happens when the user clicks a color swatch,
   * or a color is selected and the user presses one of the keys
   * Space, Shift+Space, Return or Enter.
   *
   * Deprecated: 4.10: Use [class@Gtk.ColorDialog] and [class@Gtk.ColorDialogButton]
   *   instead of widgets implementing `GtkColorChooser`
   */
  signals[COLOR_ACTIVATED] =
    g_signal_new (I_("color-activated"),
                  GTK_TYPE_COLOR_CHOOSER,
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkColorChooserInterface, color_activated),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE,
                  1, GDK_TYPE_RGBA);
}

void
_gtk_color_chooser_color_activated (GtkColorChooser *chooser,
                                    const GdkRGBA   *color)
{
  g_signal_emit (chooser, signals[COLOR_ACTIVATED], 0, color);
}

/**
 * gtk_color_chooser_get_rgba: (attributes org.gtk.Method.get_property=rgba)
 * @chooser: a `GtkColorChooser`
 * @color: (out): a `GdkRGBA` to fill in with the current color
 *
 * Gets the currently-selected color.
 *
 * Deprecated: 4.10: Use [class@Gtk.ColorDialog] instead
 */
void
gtk_color_chooser_get_rgba (GtkColorChooser *chooser,
                            GdkRGBA         *color)
{
  g_return_if_fail (GTK_IS_COLOR_CHOOSER (chooser));

  GTK_COLOR_CHOOSER_GET_IFACE (chooser)->get_rgba (chooser, color);
}

/**
 * gtk_color_chooser_set_rgba: (attributes org.gtk.Method.set_property=rgba)
 * @chooser: a `GtkColorChooser`
 * @color: the new color
 *
 * Sets the color.
 *
 * Deprecated: 4.10: Use [class@Gtk.ColorDialog] instead
 */
void
gtk_color_chooser_set_rgba (GtkColorChooser *chooser,
                            const GdkRGBA   *color)
{
  g_return_if_fail (GTK_IS_COLOR_CHOOSER (chooser));
  g_return_if_fail (color != NULL);

  GTK_COLOR_CHOOSER_GET_IFACE (chooser)->set_rgba (chooser, color);
}

/**
 * gtk_color_chooser_get_use_alpha: (attributes org.gtk.Method.get_property=use-alpha)
 * @chooser: a `GtkColorChooser`
 *
 * Returns whether the color chooser shows the alpha channel.
 *
 * Returns: %TRUE if the color chooser uses the alpha channel,
 *   %FALSE if not
 *
 * Deprecated: 4.10: Use [class@Gtk.ColorDialog] instead
 */
gboolean
gtk_color_chooser_get_use_alpha (GtkColorChooser *chooser)
{
  gboolean use_alpha;

  g_return_val_if_fail (GTK_IS_COLOR_CHOOSER (chooser), TRUE);

  g_object_get (chooser, "use-alpha", &use_alpha, NULL);

  return use_alpha;
}

/**
 * gtk_color_chooser_set_use_alpha: (attributes org.gtk.Method.set_property=use-alpha)
 * @chooser: a `GtkColorChooser`
 * @use_alpha: %TRUE if color chooser should use alpha channel, %FALSE if not
 *
 * Sets whether or not the color chooser should use the alpha channel.
 *
 * Deprecated: 4.10: Use [class@Gtk.ColorDialog] instead
 */
void
gtk_color_chooser_set_use_alpha (GtkColorChooser *chooser,
                                 gboolean         use_alpha)
{

  g_return_if_fail (GTK_IS_COLOR_CHOOSER (chooser));

  g_object_set (chooser, "use-alpha", use_alpha, NULL);
}

/**
 * gtk_color_chooser_add_palette:
 * @chooser: a `GtkColorChooser`
 * @orientation: %GTK_ORIENTATION_HORIZONTAL if the palette should
 *   be displayed in rows, %GTK_ORIENTATION_VERTICAL for columns
 * @colors_per_line: the number of colors to show in each row/column
 * @n_colors: the total number of elements in @colors
 * @colors: (nullable) (array length=n_colors): the colors of the palette
 *
 * Adds a palette to the color chooser.
 *
 * If @orientation is horizontal, the colors are grouped in rows,
 * with @colors_per_line colors in each row. If @horizontal is %FALSE,
 * the colors are grouped in columns instead.
 *
 * The default color palette of [class@Gtk.ColorChooserWidget] has
 * 45 colors, organized in columns of 5 colors (this includes some
 * grays).
 *
 * The layout of the color chooser widget works best when the
 * palettes have 9-10 columns.
 *
 * Calling this function for the first time has the side effect
 * of removing the default color palette from the color chooser.
 *
 * If @colors is %NULL, removes all previously added palettes.
 *
 * Deprecated: 4.10: Use [class@Gtk.ColorDialog] instead
 */
void
gtk_color_chooser_add_palette (GtkColorChooser *chooser,
                               GtkOrientation   orientation,
                               int              colors_per_line,
                               int              n_colors,
                               GdkRGBA         *colors)
{
  g_return_if_fail (GTK_IS_COLOR_CHOOSER (chooser));

  if (GTK_COLOR_CHOOSER_GET_IFACE (chooser)->add_palette)
    GTK_COLOR_CHOOSER_GET_IFACE (chooser)->add_palette (chooser, orientation, colors_per_line, n_colors, colors);
}

void
_gtk_color_chooser_snapshot_checkered_pattern (GtkSnapshot *snapshot,
                                               int          width,
                                               int          height)
{
  const GdkRGBA color1 = GDK_RGBA("A8A8A8");
  const GdkRGBA color2 = GDK_RGBA("545454");

  gtk_snapshot_push_repeat (snapshot, &GRAPHENE_RECT_INIT (0, 0, width, height), NULL);
  gtk_snapshot_append_color (snapshot, &color1, &GRAPHENE_RECT_INIT (0,  0,  10, 10));
  gtk_snapshot_append_color (snapshot, &color2, &GRAPHENE_RECT_INIT (10, 0,  10, 10));
  gtk_snapshot_append_color (snapshot, &color2, &GRAPHENE_RECT_INIT (0,  10, 10, 10));
  gtk_snapshot_append_color (snapshot, &color1, &GRAPHENE_RECT_INIT (10, 10, 10, 10));
  gtk_snapshot_pop (snapshot);
}
