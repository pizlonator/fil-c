/*
 * Copyright (c) 2020 Alexander Mikhaylenko <alexm@gnome.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"

#include "gtkwindowcontrols.h"

#include "gtkaccessible.h"
#include "gtkactionable.h"
#include "gtkboxlayout.h"
#include "gtkbutton.h"
#include "gtkenums.h"
#include "gtkicontheme.h"
#include "gtkimage.h"
#include <glib/gi18n-lib.h>
#include "gtkprivate.h"
#include "gtktypebuiltins.h"
#include "gtkwindowprivate.h"

/**
 * GtkWindowControls:
 *
 * `GtkWindowControls` shows window frame controls.
 *
 * Typical window frame controls are minimize, maximize and close buttons,
 * and the window icon.
 *
 * ![An example GtkWindowControls](windowcontrols.png)
 *
 * `GtkWindowControls` only displays start or end side of the controls (see
 * [property@Gtk.WindowControls:side]), so it's intended to be always used
 * in pair with another `GtkWindowControls` for the opposite side, for example:
 *
 * ```xml
 * <object class="GtkBox">
 *   <child>
 *     <object class="GtkWindowControls">
 *       <property name="side">start</property>
 *     </object>
 *   </child>
 *
 *   ...
 *
 *   <child>
 *     <object class="GtkWindowControls">
 *       <property name="side">end</property>
 *     </object>
 *   </child>
 * </object>
 * ```
 *
 * # CSS nodes
 *
 * ```
 * windowcontrols
 * ├── [image.icon]
 * ├── [button.minimize]
 * ├── [button.maximize]
 * ╰── [button.close]
 * ```
 *
 * A `GtkWindowControls`' CSS node is called windowcontrols. It contains
 * subnodes corresponding to each title button. Which of the title buttons
 * exist and where they are placed exactly depends on the desktop environment
 * and [property@Gtk.WindowControls:decoration-layout] value.
 *
 * When [property@Gtk.WindowControls:empty] is %TRUE, it gets the .empty
 * style class.
 *
 * # Accessibility
 *
 * `GtkWindowControls` uses the %GTK_ACCESSIBLE_ROLE_GROUP role.
 */

struct _GtkWindowControls {
  GtkWidget parent_instance;

  GtkPackType side;
  char *decoration_layout;

  gboolean empty;
};

enum {
  PROP_0,
  PROP_SIDE,
  PROP_DECORATION_LAYOUT,
  PROP_EMPTY,
  LAST_PROP
};

static GParamSpec *props[LAST_PROP] = { NULL, };

#define WINDOW_ICON_SIZE 16

G_DEFINE_TYPE (GtkWindowControls, gtk_window_controls, GTK_TYPE_WIDGET)

static char *
get_layout (GtkWindowControls *self)
{
  GtkWidget *widget = GTK_WIDGET (self);
  GtkRoot *root;
  char *layout_desc, *layout_half;
  char **tokens;

  root = gtk_widget_get_root (widget);
  if (!root || !GTK_IS_WINDOW (root))
    return NULL;

  if (self->decoration_layout)
    layout_desc = g_strdup (self->decoration_layout);
  else
    g_object_get (gtk_widget_get_settings (widget),
                  "gtk-decoration-layout", &layout_desc,
                  NULL);

  tokens = g_strsplit (layout_desc, ":", 2);

  switch (self->side)
    {
    case GTK_PACK_START:
      layout_half = g_strdup (tokens[0]);
      break;

    case GTK_PACK_END:
      layout_half = g_strdup (tokens[1]);
      break;

    default:
      g_assert_not_reached ();
    }

  g_free (layout_desc);
  g_strfreev (tokens);

  return layout_half;
}

static GdkPaintable *
get_default_icon (GtkWidget *widget)
{
  GdkDisplay *display = gtk_widget_get_display (widget);
  GtkIconPaintable *info;
  int scale = gtk_widget_get_scale_factor (widget);

  info = gtk_icon_theme_lookup_icon (gtk_icon_theme_get_for_display (display),
                                     gtk_window_get_default_icon_name (),
                                     NULL,
                                     WINDOW_ICON_SIZE,
                                     scale,
                                     gtk_widget_get_direction (widget),
                                     0);

  return GDK_PAINTABLE (info);
}

static gboolean
update_window_icon (GtkWindow *window,
                    GtkWidget *icon)
{
  GdkPaintable *paintable;

  if (window)
    paintable = gtk_window_get_icon_for_size (window, WINDOW_ICON_SIZE);
  else
    paintable = get_default_icon (icon);

  if (paintable)
    {
      gtk_image_set_from_paintable (GTK_IMAGE (icon), paintable);
      g_object_unref (paintable);
      gtk_widget_set_visible (icon, TRUE);

      return TRUE;
    }

  return FALSE;
}

static void
set_empty (GtkWindowControls *self,
           gboolean           empty)
{
  if (empty == self->empty)
    return;

  self->empty = empty;

  if (empty)
    gtk_widget_add_css_class (GTK_WIDGET (self), "empty");
  else
    gtk_widget_remove_css_class (GTK_WIDGET (self), "empty");

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_EMPTY]);
}

static void
clear_controls (GtkWindowControls *self)
{
  GtkWidget *child = gtk_widget_get_first_child (GTK_WIDGET (self));

  while (child)
    {
      GtkWidget *next = gtk_widget_get_next_sibling (child);

      gtk_widget_unparent (child);

      child = next;
    }
}

static void
update_window_buttons (GtkWindowControls *self)
{
  GtkWidget *widget = GTK_WIDGET (self);
  char *layout;
  char **tokens;
  int i;
  gboolean is_sovereign_window;
  gboolean maximized;
  gboolean resizable;
  gboolean deletable;
  gboolean empty = TRUE;
  GtkRoot *root;
  GtkWindow *window = NULL;

  root = gtk_widget_get_root (widget);
  if (!root || !GTK_IS_WINDOW (root))
    {
      set_empty (self, TRUE);

      return;
    }

  clear_controls (self);

  window = GTK_WINDOW (root);
  is_sovereign_window = !gtk_window_get_modal (window) &&
                         gtk_window_get_transient_for (window) == NULL;
  maximized = gtk_window_is_maximized (window);
  resizable = gtk_window_get_resizable (window);
  deletable = gtk_window_get_deletable (window);

  layout = get_layout (self);

  if (!layout)
    {
      set_empty (self, TRUE);

      return;
    }

  tokens = g_strsplit (layout, ",", -1);

  for (i = 0; tokens[i]; i++)
    {
      GtkWidget *button = NULL;
      GtkWidget *image = NULL;

      if (strcmp (tokens[i], "icon") == 0 &&
          is_sovereign_window)
        {
          /* The icon is not relevant for accessibility purposes */
          button = g_object_new (GTK_TYPE_IMAGE,
                                 "accessible-role", GTK_ACCESSIBLE_ROLE_PRESENTATION,
                                 NULL);
          gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
          gtk_widget_add_css_class (button, "icon");

          if (!update_window_icon (window, button))
            {
              g_object_ref_sink (button);
              g_object_unref (button);
              button = NULL;
            }
        }
      else if (strcmp (tokens[i], "minimize") == 0 &&
               is_sovereign_window)
        {
          button = gtk_button_new ();
          gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
          gtk_widget_add_css_class (button, "minimize");
          /* The icon is not relevant for accessibility purposes */
          image = g_object_new (GTK_TYPE_IMAGE,
                                "accessible-role", GTK_ACCESSIBLE_ROLE_PRESENTATION,
                                "icon-name", "window-minimize-symbolic",
                                NULL);
          g_object_set (image, "use-fallback", TRUE, NULL);
          gtk_button_set_child (GTK_BUTTON (button), image);
          gtk_widget_set_can_focus (button, FALSE);
          gtk_actionable_set_action_name (GTK_ACTIONABLE (button),
                                          "window.minimize");
          gtk_accessible_update_property (GTK_ACCESSIBLE (button),
                                          GTK_ACCESSIBLE_PROPERTY_LABEL, _("Minimize"),
                                          GTK_ACCESSIBLE_PROPERTY_DESCRIPTION,
                                            _("Minimize the window"),
                                          -1);
        }
      else if (strcmp (tokens[i], "maximize") == 0 &&
               resizable &&
               is_sovereign_window)
        {
          const char *icon_name;

          icon_name = maximized ? "window-restore-symbolic" : "window-maximize-symbolic";
          button = gtk_button_new ();
          gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
          gtk_widget_add_css_class (button, "maximize");
          /* The icon is not relevant for accessibility purposes */
          image = g_object_new (GTK_TYPE_IMAGE,
                                "accessible-role", GTK_ACCESSIBLE_ROLE_PRESENTATION,
                                "icon-name", icon_name,
                                NULL);
          g_object_set (image, "use-fallback", TRUE, NULL);
          gtk_button_set_child (GTK_BUTTON (button), image);
          gtk_widget_set_can_focus (button, FALSE);
          gtk_actionable_set_action_name (GTK_ACTIONABLE (button),
                                          "window.toggle-maximized");
          gtk_accessible_update_property (GTK_ACCESSIBLE (button),
                                          GTK_ACCESSIBLE_PROPERTY_LABEL, _("Maximize"),
                                          GTK_ACCESSIBLE_PROPERTY_DESCRIPTION,
                                            _("Maximize the window"),
                                          -1);
        }
      else if (strcmp (tokens[i], "close") == 0 &&
               deletable)
        {
          button = gtk_button_new ();
          gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
          /* The icon is not relevant for accessibility purposes */
          image = g_object_new (GTK_TYPE_IMAGE,
                                "accessible-role", GTK_ACCESSIBLE_ROLE_PRESENTATION,
                                "icon-name", "window-close-symbolic",
                                "use-fallback", TRUE,
                                NULL);
          gtk_widget_add_css_class (button, "close");
          gtk_button_set_child (GTK_BUTTON (button), image);
          gtk_widget_set_can_focus (button, FALSE);
          gtk_actionable_set_action_name (GTK_ACTIONABLE (button),
                                          "window.close");
          gtk_accessible_update_property (GTK_ACCESSIBLE (button),
                                          GTK_ACCESSIBLE_PROPERTY_LABEL, _("Close"),
                                          GTK_ACCESSIBLE_PROPERTY_DESCRIPTION,
                                            _("Close the window"),
                                          -1);
        }

      if (button)
        {
          gtk_widget_set_parent (button, widget);
          empty = FALSE;
        }
    }
  g_free (layout);
  g_strfreev (tokens);

  set_empty (self, empty);
}

static void
window_notify_cb (GtkWindowControls *self,
                  GParamSpec        *pspec,
                  GtkWindow         *window)
{
  if (pspec->name == I_("deletable") ||
      pspec->name == I_("icon-name") ||
      pspec->name == I_("maximized") ||
      pspec->name == I_("modal") ||
      pspec->name == I_("resizable") ||
      pspec->name == I_("transient-for"))
    update_window_buttons (self);
}

static void
gtk_window_controls_root (GtkWidget *widget)
{
  GtkSettings *settings;
  GtkWidget *root;

  GTK_WIDGET_CLASS (gtk_window_controls_parent_class)->root (widget);

  settings = gtk_widget_get_settings (widget);
  g_signal_connect_swapped (settings, "notify::gtk-decoration-layout",
                            G_CALLBACK (update_window_buttons), widget);

  root = GTK_WIDGET (gtk_widget_get_root (widget));

  if (GTK_IS_WINDOW (root))
    g_signal_connect_swapped (root, "notify",
                              G_CALLBACK (window_notify_cb), widget);

  update_window_buttons (GTK_WINDOW_CONTROLS (widget));
}

static void
gtk_window_controls_unroot (GtkWidget *widget)
{
  GtkSettings *settings;

  settings = gtk_widget_get_settings (widget);

  g_signal_handlers_disconnect_by_func (settings, update_window_buttons, widget);
  g_signal_handlers_disconnect_by_func (gtk_widget_get_root (widget), window_notify_cb, widget);

  GTK_WIDGET_CLASS (gtk_window_controls_parent_class)->unroot (widget);
}

static void
gtk_window_controls_dispose (GObject *object)
{
  GtkWindowControls *self = GTK_WINDOW_CONTROLS (object);

  clear_controls (self);

  G_OBJECT_CLASS (gtk_window_controls_parent_class)->dispose (object);
}

static void
gtk_window_controls_finalize (GObject *object)
{
  GtkWindowControls *self = GTK_WINDOW_CONTROLS (object);

  g_free (self->decoration_layout);

  G_OBJECT_CLASS (gtk_window_controls_parent_class)->finalize (object);
}

static void
gtk_window_controls_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GtkWindowControls *self = GTK_WINDOW_CONTROLS (object);

  switch (prop_id)
    {
    case PROP_SIDE:
      g_value_set_enum (value, gtk_window_controls_get_side (self));
      break;

    case PROP_DECORATION_LAYOUT:
      g_value_set_string (value, gtk_window_controls_get_decoration_layout (self));
      break;

    case PROP_EMPTY:
      g_value_set_boolean (value, gtk_window_controls_get_empty (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_window_controls_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GtkWindowControls *self = GTK_WINDOW_CONTROLS (object);

  switch (prop_id)
    {
    case PROP_SIDE:
      gtk_window_controls_set_side (self, g_value_get_enum (value));
      break;

    case PROP_DECORATION_LAYOUT:
      gtk_window_controls_set_decoration_layout (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_window_controls_class_init (GtkWindowControlsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gtk_window_controls_dispose;
  object_class->finalize = gtk_window_controls_finalize;
  object_class->get_property = gtk_window_controls_get_property;
  object_class->set_property = gtk_window_controls_set_property;

  widget_class->root = gtk_window_controls_root;
  widget_class->unroot = gtk_window_controls_unroot;

  /**
   * GtkWindowControls:side: (attributes org.gtk.Property.get=gtk_window_controls_get_side org.gtk.Property.set=gtk_window_controls_set_side)
   *
   * Whether the widget shows start or end side of the decoration layout.
   *
   * See [property@Gtk.WindowControls:decoration_layout].
   */
  props[PROP_SIDE] =
      g_param_spec_enum ("side", NULL, NULL,
                         GTK_TYPE_PACK_TYPE,
                         GTK_PACK_START,
                         GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkWindowControls:decoration-layout: (attributes org.gtk.Property.get=gtk_window_controls_get_decoration_layout org.gtk.Property.set=gtk_window_controls_set_decoration_layout)
   *
   * The decoration layout for window buttons.
   *
   * If this property is not set, the
   * [property@Gtk.Settings:gtk-decoration-layout] setting is used.
   */
  props[PROP_DECORATION_LAYOUT] =
      g_param_spec_string ("decoration-layout", NULL, NULL,
                           NULL,
                           GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkWindowControls:empty: (attributes org.gtk.Property.get=gtk_window_controls_get_empty)
   *
   * Whether the widget has any window buttons.
   */
  props[PROP_EMPTY] =
    g_param_spec_boolean ("empty", NULL, NULL,
                          TRUE,
                          GTK_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BOX_LAYOUT);
  gtk_widget_class_set_css_name (widget_class, I_("windowcontrols"));
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_GROUP);
}

static void
gtk_window_controls_init (GtkWindowControls *self)
{
  self->decoration_layout = NULL;
  self->side = GTK_PACK_START;
  self->empty = TRUE;

  gtk_widget_add_css_class (GTK_WIDGET (self), "empty");
  gtk_widget_add_css_class (GTK_WIDGET (self), "start");

  gtk_widget_set_can_focus (GTK_WIDGET (self), FALSE);
}

/**
 * gtk_window_controls_new:
 * @side: the side
 *
 * Creates a new `GtkWindowControls`.
 *
 * Returns: a new `GtkWindowControls`.
 **/
GtkWidget *
gtk_window_controls_new (GtkPackType side)
{
  return g_object_new (GTK_TYPE_WINDOW_CONTROLS,
                       "side", side,
                       NULL);
}

/**
 * gtk_window_controls_get_side: (attributes org.gtk.Method.get_property=side)
 * @self: a `GtkWindowControls`
 *
 * Gets the side to which this `GtkWindowControls` instance belongs.
 *
 * Returns: the side
 */
GtkPackType
gtk_window_controls_get_side (GtkWindowControls *self)
{
  g_return_val_if_fail (GTK_IS_WINDOW_CONTROLS (self), GTK_PACK_START);

  return self->side;
}

/**
 * gtk_window_controls_set_side: (attributes org.gtk.Method.set_property=side)
 * @self: a `GtkWindowControls`
 * @side: a side
 *
 * Determines which part of decoration layout the `GtkWindowControls` uses.
 *
 * See [property@Gtk.WindowControls:decoration-layout].
 */
void
gtk_window_controls_set_side (GtkWindowControls *self,
                              GtkPackType        side)
{
  g_return_if_fail (GTK_IS_WINDOW_CONTROLS (self));

  if (self->side == side)
    return;

  self->side = side;

  switch (side)
    {
    case GTK_PACK_START:
      gtk_widget_add_css_class (GTK_WIDGET (self), "start");
      gtk_widget_remove_css_class (GTK_WIDGET (self), "end");
      break;

    case GTK_PACK_END:
      gtk_widget_add_css_class (GTK_WIDGET (self), "end");
      gtk_widget_remove_css_class (GTK_WIDGET (self), "start");
      break;

    default:
      g_warning ("Unexpected side: %d", side);
      break;
    }

  update_window_buttons (self);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_SIDE]);
}

/**
 * gtk_window_controls_get_decoration_layout: (attributes org.gtk.Method.get_property=decoration-layout)
 * @self: a `GtkWindowControls`
 *
 * Gets the decoration layout of this `GtkWindowControls`.
 *
 * Returns: (nullable): the decoration layout or %NULL if it is unset
 */
const char *
gtk_window_controls_get_decoration_layout (GtkWindowControls *self)
{
  g_return_val_if_fail (GTK_IS_WINDOW_CONTROLS (self), NULL);

  return self->decoration_layout;
}

/**
 * gtk_window_controls_set_decoration_layout: (attributes org.gtk.Method.set_property=decoration-layout)
 * @self: a `GtkWindowControls`
 * @layout: (nullable): a decoration layout, or %NULL to unset the layout
 *
 * Sets the decoration layout for the title buttons.
 *
 * This overrides the [property@Gtk.Settings:gtk-decoration-layout]
 * setting.
 *
 * The format of the string is button names, separated by commas.
 * A colon separates the buttons that should appear on the left
 * from those on the right. Recognized button names are minimize,
 * maximize, close and icon (the window icon).
 *
 * For example, “icon:minimize,maximize,close” specifies a icon
 * on the left, and minimize, maximize and close buttons on the right.
 *
 * If [property@Gtk.WindowControls:side] value is @GTK_PACK_START, @self
 * will display the part before the colon, otherwise after that.
 */
void
gtk_window_controls_set_decoration_layout (GtkWindowControls *self,
                                           const char        *layout)
{
  g_return_if_fail (GTK_IS_WINDOW_CONTROLS (self));

  g_free (self->decoration_layout);
  self->decoration_layout = g_strdup (layout);

  update_window_buttons (self);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_DECORATION_LAYOUT]);
}

/**
 * gtk_window_controls_get_empty: (attributes org.gtk.Method.get_property=empty)
 * @self: a `GtkWindowControls`
 *
 * Gets whether the widget has any window buttons.
 *
 * Returns: %TRUE if the widget has window buttons, otherwise %FALSE
 */
gboolean
gtk_window_controls_get_empty (GtkWindowControls *self)
{
  g_return_val_if_fail (GTK_IS_WINDOW_CONTROLS (self), FALSE);

  return self->empty;
}
