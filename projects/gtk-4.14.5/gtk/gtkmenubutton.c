/* GTK - The GIMP Toolkit
 *
 * Copyright (C) 2003 Ricardo Fernandez Pascual
 * Copyright (C) 2004 Paolo Borelli
 * Copyright (C) 2012 Bastien Nocera
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

/**
 * GtkMenuButton:
 *
 * The `GtkMenuButton` widget is used to display a popup when clicked.
 *
 * ![An example GtkMenuButton](menu-button.png)
 *
 * This popup can be provided either as a `GtkPopover` or as an abstract
 * `GMenuModel`.
 *
 * The `GtkMenuButton` widget can show either an icon (set with the
 * [property@Gtk.MenuButton:icon-name] property) or a label (set with the
 * [property@Gtk.MenuButton:label] property). If neither is explicitly set,
 * a [class@Gtk.Image] is automatically created, using an arrow image oriented
 * according to [property@Gtk.MenuButton:direction] or the generic
 * “open-menu-symbolic” icon if the direction is not set.
 *
 * The positioning of the popup is determined by the
 * [property@Gtk.MenuButton:direction] property of the menu button.
 *
 * For menus, the [property@Gtk.Widget:halign] and [property@Gtk.Widget:valign]
 * properties of the menu are also taken into account. For example, when the
 * direction is %GTK_ARROW_DOWN and the horizontal alignment is %GTK_ALIGN_START,
 * the menu will be positioned below the button, with the starting edge
 * (depending on the text direction) of the menu aligned with the starting
 * edge of the button. If there is not enough space below the button, the
 * menu is popped up above the button instead. If the alignment would move
 * part of the menu offscreen, it is “pushed in”.
 *
 * |           | start                | center                | end                |
 * | -         | ---                  | ---                   | ---                |
 * | **down**  | ![](down-start.png)  | ![](down-center.png)  | ![](down-end.png)  |
 * | **up**    | ![](up-start.png)    | ![](up-center.png)    | ![](up-end.png)    |
 * | **left**  | ![](left-start.png)  | ![](left-center.png)  | ![](left-end.png)  |
 * | **right** | ![](right-start.png) | ![](right-center.png) | ![](right-end.png) |
 *
 * # CSS nodes
 *
 * ```
 * menubutton
 * ╰── button.toggle
 *     ╰── <content>
 *          ╰── [arrow]
 * ```
 *
 * `GtkMenuButton` has a single CSS node with name `menubutton`
 * which contains a `button` node with a `.toggle` style class.
 *
 * If the button contains an icon, it will have the `.image-button` style class,
 * if it contains text, it will have `.text-button` style class. If an arrow is
 * visible in addition to an icon, text or a custom child, it will also have
 * `.arrow-button` style class.
 *
 * Inside the toggle button content, there is an `arrow` node for
 * the indicator, which will carry one of the `.none`, `.up`, `.down`,
 * `.left` or `.right` style classes to indicate the direction that
 * the menu will appear in. The CSS is expected to provide a suitable
 * image for each of these cases using the `-gtk-icon-source` property.
 *
 * Optionally, the `menubutton` node can carry the `.circular` style class
 * to request a round appearance.
 *
 * # Accessibility
 *
 * `GtkMenuButton` uses the %GTK_ACCESSIBLE_ROLE_BUTTON role.
 */

#include "config.h"

#include "gtkactionable.h"
#include "gtkbinlayout.h"
#include "gtkbuildable.h"
#include "gtkbuiltiniconprivate.h"
#include "gtkgizmoprivate.h"
#include "gtkimage.h"
#include "gtkmain.h"
#include "gtkmenubutton.h"
#include "gtkmenubuttonprivate.h"
#include "gtkpopover.h"
#include "gtkpopovermenu.h"
#include "gtkprivate.h"
#include "gtktypebuiltins.h"
#include "gtklabel.h"
#include "gtkbox.h"
#include "gtkwidgetprivate.h"
#include "gtkbuttonprivate.h"
#include "gtknative.h"
#include "gtkwindow.h"

typedef struct _GtkMenuButtonClass   GtkMenuButtonClass;
typedef struct _GtkMenuButtonPrivate GtkMenuButtonPrivate;

struct _GtkMenuButton
{
  GtkWidget parent_instance;

  GtkWidget *button;
  GtkWidget *popover; /* Only one at a time can be set */
  GMenuModel *model;

  GtkMenuButtonCreatePopupFunc create_popup_func;
  gpointer create_popup_user_data;
  GDestroyNotify create_popup_destroy_notify;

  GtkWidget *label_widget;
  GtkWidget *image_widget;
  GtkWidget *arrow_widget;
  GtkWidget *child;
  GtkArrowType arrow_type;
  gboolean always_show_arrow;

  gboolean primary;
  gboolean can_shrink;
};

struct _GtkMenuButtonClass
{
  GtkWidgetClass parent_class;

  void (* activate) (GtkMenuButton *self);
};

enum
{
  PROP_0,
  PROP_MENU_MODEL,
  PROP_DIRECTION,
  PROP_POPOVER,
  PROP_ICON_NAME,
  PROP_ALWAYS_SHOW_ARROW,
  PROP_LABEL,
  PROP_USE_UNDERLINE,
  PROP_HAS_FRAME,
  PROP_PRIMARY,
  PROP_CHILD,
  PROP_ACTIVE,
  PROP_CAN_SHRINK,
  LAST_PROP
};

enum {
  ACTIVATE,
  LAST_SIGNAL
};

static GParamSpec *menu_button_props[LAST_PROP];
static guint signals[LAST_SIGNAL] = { 0 };

static void gtk_menu_button_buildable_iface_init (GtkBuildableIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkMenuButton, gtk_menu_button, GTK_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE, gtk_menu_button_buildable_iface_init))

static void gtk_menu_button_dispose (GObject *object);

static void
gtk_menu_button_set_property (GObject      *object,
                              guint         property_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GtkMenuButton *self = GTK_MENU_BUTTON (object);

  switch (property_id)
    {
      case PROP_MENU_MODEL:
        gtk_menu_button_set_menu_model (self, g_value_get_object (value));
        break;
      case PROP_DIRECTION:
        gtk_menu_button_set_direction (self, g_value_get_enum (value));
        break;
      case PROP_POPOVER:
        gtk_menu_button_set_popover (self, g_value_get_object (value));
        break;
      case PROP_ICON_NAME:
        gtk_menu_button_set_icon_name (self, g_value_get_string (value));
        break;
      case PROP_ALWAYS_SHOW_ARROW:
        gtk_menu_button_set_always_show_arrow (self, g_value_get_boolean (value));
        break;
      case PROP_LABEL:
        gtk_menu_button_set_label (self, g_value_get_string (value));
        break;
      case PROP_USE_UNDERLINE:
        gtk_menu_button_set_use_underline (self, g_value_get_boolean (value));
        break;
      case PROP_HAS_FRAME:
        gtk_menu_button_set_has_frame (self, g_value_get_boolean (value));
        break;
      case PROP_PRIMARY:
        gtk_menu_button_set_primary (self, g_value_get_boolean (value));
        break;
      case PROP_CHILD:
        gtk_menu_button_set_child (self, g_value_get_object (value));
        break;
      case PROP_ACTIVE:
        gtk_menu_button_set_active (self, g_value_get_boolean (value));
        break;
      case PROP_CAN_SHRINK:
        gtk_menu_button_set_can_shrink (self, g_value_get_boolean (value));
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
gtk_menu_button_get_property (GObject    *object,
                              guint       property_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GtkMenuButton *self = GTK_MENU_BUTTON (object);

  switch (property_id)
    {
      case PROP_MENU_MODEL:
        g_value_set_object (value, self->model);
        break;
      case PROP_DIRECTION:
        g_value_set_enum (value, self->arrow_type);
        break;
      case PROP_POPOVER:
        g_value_set_object (value, self->popover);
        break;
      case PROP_ICON_NAME:
        g_value_set_string (value, gtk_menu_button_get_icon_name (GTK_MENU_BUTTON (object)));
        break;
      case PROP_ALWAYS_SHOW_ARROW:
        g_value_set_boolean (value, gtk_menu_button_get_always_show_arrow (self));
        break;
      case PROP_LABEL:
        g_value_set_string (value, gtk_menu_button_get_label (GTK_MENU_BUTTON (object)));
        break;
      case PROP_USE_UNDERLINE:
        g_value_set_boolean (value, gtk_menu_button_get_use_underline (GTK_MENU_BUTTON (object)));
        break;
      case PROP_HAS_FRAME:
        g_value_set_boolean (value, gtk_menu_button_get_has_frame (GTK_MENU_BUTTON (object)));
        break;
      case PROP_PRIMARY:
        g_value_set_boolean (value, gtk_menu_button_get_primary (GTK_MENU_BUTTON (object)));
        break;
      case PROP_CHILD:
        g_value_set_object (value, gtk_menu_button_get_child (self));
        break;
      case PROP_ACTIVE:
        g_value_set_boolean (value, gtk_menu_button_get_active (self));
        break;
      case PROP_CAN_SHRINK:
        g_value_set_boolean (value, gtk_menu_button_get_can_shrink (self));
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
gtk_menu_button_notify (GObject    *object,
                        GParamSpec *pspec)
{
  if (strcmp (pspec->name, "focus-on-click") == 0)
    {
      GtkMenuButton *self = GTK_MENU_BUTTON (object);

      gtk_widget_set_focus_on_click (self->button,
                                     gtk_widget_get_focus_on_click (GTK_WIDGET (self)));
    }

  if (G_OBJECT_CLASS (gtk_menu_button_parent_class)->notify)
    G_OBJECT_CLASS (gtk_menu_button_parent_class)->notify (object, pspec);
}

static void
gtk_menu_button_state_flags_changed (GtkWidget    *widget,
                                     GtkStateFlags previous_state_flags)
{
  GtkMenuButton *self = GTK_MENU_BUTTON (widget);

  if (!gtk_widget_is_sensitive (widget))
    {
      if (self->popover)
        gtk_widget_set_visible (self->popover, FALSE);
    }
}

static void
gtk_menu_button_toggled (GtkMenuButton *self)
{
  const gboolean active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->button));

  /* Might set a new menu/popover */
  if (active && self->create_popup_func)
    {
      self->create_popup_func (self, self->create_popup_user_data);
    }

  if (self->popover)
    {
      if (active)
        {
          gtk_popover_popup (GTK_POPOVER (self->popover));
          gtk_accessible_update_state (GTK_ACCESSIBLE (self),
                                       GTK_ACCESSIBLE_STATE_EXPANDED, TRUE,
                                       -1);
        }
      else
        {
          gtk_popover_popdown (GTK_POPOVER (self->popover));
          gtk_accessible_reset_state (GTK_ACCESSIBLE (self),
                                      GTK_ACCESSIBLE_STATE_EXPANDED);
        }
    }

  g_object_notify_by_pspec (G_OBJECT (self), menu_button_props[PROP_ACTIVE]);
}

static void
gtk_menu_button_measure (GtkWidget      *widget,
                         GtkOrientation  orientation,
                         int             for_size,
                         int            *minimum,
                         int            *natural,
                         int            *minimum_baseline,
                         int            *natural_baseline)
{
  GtkMenuButton *self = GTK_MENU_BUTTON (widget);

  gtk_widget_measure (self->button,
                      orientation,
                      for_size,
                      minimum, natural,
                      minimum_baseline, natural_baseline);

}

static void
gtk_menu_button_size_allocate (GtkWidget *widget,
                               int        width,
                               int        height,
                               int        baseline)
{
  GtkMenuButton *self= GTK_MENU_BUTTON (widget);

  gtk_widget_size_allocate (self->button,
                            &(GtkAllocation) { 0, 0, width, height },
                            baseline);
  if (self->popover)
    gtk_popover_present (GTK_POPOVER (self->popover));
}

static gboolean
gtk_menu_button_focus (GtkWidget        *widget,
                       GtkDirectionType  direction)
{
  GtkMenuButton *self = GTK_MENU_BUTTON (widget);

  if (self->popover && gtk_widget_get_visible (self->popover))
    return gtk_widget_child_focus (self->popover, direction);
  else
    return gtk_widget_child_focus (self->button, direction);
}

static gboolean
gtk_menu_button_grab_focus (GtkWidget *widget)
{
  GtkMenuButton *self = GTK_MENU_BUTTON (widget);

  return gtk_widget_grab_focus (self->button);
}

static void
gtk_menu_button_activate (GtkMenuButton *self)
{
  gtk_widget_activate (self->button);
}

static void gtk_menu_button_root (GtkWidget *widget);
static void gtk_menu_button_unroot (GtkWidget *widget);

static void
gtk_menu_button_class_init (GtkMenuButtonClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gobject_class->set_property = gtk_menu_button_set_property;
  gobject_class->get_property = gtk_menu_button_get_property;
  gobject_class->notify = gtk_menu_button_notify;
  gobject_class->dispose = gtk_menu_button_dispose;

  widget_class->root = gtk_menu_button_root;
  widget_class->unroot = gtk_menu_button_unroot;
  widget_class->measure = gtk_menu_button_measure;
  widget_class->size_allocate = gtk_menu_button_size_allocate;
  widget_class->state_flags_changed = gtk_menu_button_state_flags_changed;
  widget_class->focus = gtk_menu_button_focus;
  widget_class->grab_focus = gtk_menu_button_grab_focus;

  klass->activate = gtk_menu_button_activate;

  /**
   * GtkMenuButton:menu-model: (attributes org.gtk.Property.get=gtk_menu_button_get_menu_model org.gtk.Property.set=gtk_menu_button_set_menu_model)
   *
   * The `GMenuModel` from which the popup will be created.
   *
   * See [method@Gtk.MenuButton.set_menu_model] for the interaction
   * with the [property@Gtk.MenuButton:popover] property.
   */
  menu_button_props[PROP_MENU_MODEL] =
      g_param_spec_object ("menu-model", NULL, NULL,
                           G_TYPE_MENU_MODEL,
                           GTK_PARAM_READWRITE);

  /**
   * GtkMenuButton:direction: (attributes org.gtk.Property.get=gtk_menu_button_get_direction org.gtk.Property.set=gtk_menu_button_set_direction)
   *
   * The `GtkArrowType` representing the direction in which the
   * menu or popover will be popped out.
   */
  menu_button_props[PROP_DIRECTION] =
      g_param_spec_enum ("direction", NULL, NULL,
                         GTK_TYPE_ARROW_TYPE,
                         GTK_ARROW_DOWN,
                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkMenuButton:popover: (attributes org.gtk.Property.get=gtk_menu_button_get_popover org.gtk.Property.set=gtk_menu_button_set_popover)
   *
   * The `GtkPopover` that will be popped up when the button is clicked.
   */
  menu_button_props[PROP_POPOVER] =
      g_param_spec_object ("popover", NULL, NULL,
                           GTK_TYPE_POPOVER,
                           G_PARAM_READWRITE);

  /**
   * GtkMenuButton:icon-name: (attributes org.gtk.Property.get=gtk_menu_button_get_icon_name org.gtk.Property.set=gtk_menu_button_set_icon_name)
   *
   * The name of the icon used to automatically populate the button.
   */
  menu_button_props[PROP_ICON_NAME] =
      g_param_spec_string ("icon-name", NULL, NULL,
                           NULL,
                           GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkMenuButton:always-show-arrow: (attributes org.gtk.Property.get=gtk_menu_button_get_always_show_arrow org.gtk.Property.set=gtk_menu_button_set_always_show_arrow)
   *
   * Whether to show a dropdown arrow even when using an icon or a custom child.
   *
   * Since: 4.4
   */
  menu_button_props[PROP_ALWAYS_SHOW_ARROW] =
      g_param_spec_boolean ("always-show-arrow", NULL, NULL,
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkMenuButton:label: (attributes org.gtk.Property.get=gtk_menu_button_get_label org.gtk.Property.set=gtk_menu_button_set_label)
   *
   * The label for the button.
   */
  menu_button_props[PROP_LABEL] =
      g_param_spec_string ("label", NULL, NULL,
                           NULL,
                           GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkMenuButton:use-underline: (attributes org.gtk.Property.get=gtk_menu_button_get_use_underline org.gtk.Property.set=gtk_menu_button_set_use_underline)
   *
   * If set an underscore in the text indicates a mnemonic.
   */
  menu_button_props[PROP_USE_UNDERLINE] =
      g_param_spec_boolean ("use-underline", NULL, NULL,
                           FALSE,
                           GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkMenuButton:has-frame: (attributes org.gtk.Property.get=gtk_menu_button_get_has_frame org.gtk.Property.set=gtk_menu_button_set_has_frame)
   *
   * Whether the button has a frame.
   */
  menu_button_props[PROP_HAS_FRAME] =
    g_param_spec_boolean ("has-frame", NULL, NULL,
                          TRUE,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkMenuButton:primary: (attributes org.gtk.Property.get=gtk_menu_button_get_primary org.gtk.Property.set=gtk_menu_button_set_primary)
   *
   * Whether the menu button acts as a primary menu.
   *
   * Primary menus can be opened using the <kbd>F10</kbd> key
   *
   * Since: 4.4
   */
  menu_button_props[PROP_PRIMARY] =
    g_param_spec_boolean ("primary", NULL, NULL,
                          FALSE,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkMenuButton:child: (attributes org.gtk.Property.get=gtk_menu_button_get_child org.gtk.Property.set=gtk_menu_button_set_child)
   *
   * The child widget.
   *
   * Since: 4.6
   */
  menu_button_props[PROP_CHILD] =
    g_param_spec_object ("child", NULL, NULL,
                         GTK_TYPE_WIDGET,
                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkMenuButton:active: (attributes org.gtk.Property.get=gtk_menu_button_get_active org.gtk.Property.set=gtk_menu_button_set_active)
   *
   * Whether the menu button is active.
   *
   * Since: 4.10
   */
  menu_button_props[PROP_ACTIVE] =
    g_param_spec_boolean ("active", NULL, NULL,
                          FALSE,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkMenuButton:can-shrink: (attributes org.gtk.Property.get=gtk_menu_button_get_can_shrink org.gtk.Property.set=gtk_menu_button_set_can_shrink)
   *
   * Whether the size of the button can be made smaller than the natural
   * size of its contents.
   *
   * Since: 4.12
   */
  menu_button_props[PROP_CAN_SHRINK] =
    g_param_spec_boolean ("can-shrink", NULL, NULL,
                          FALSE,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, LAST_PROP, menu_button_props);

  /**
   * GtkMenuButton::activate:
   * @widget: the object which received the signal.
   *
   * Emitted to when the menu button is activated.
   *
   * The `::activate` signal on `GtkMenuButton` is an action signal and
   * emitting it causes the button to pop up its menu.
   *
   * Since: 4.4
   */
  signals[ACTIVATE] =
      g_signal_new (I_ ("activate"),
                    G_OBJECT_CLASS_TYPE (gobject_class),
                    G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                    G_STRUCT_OFFSET (GtkMenuButtonClass, activate),
                    NULL, NULL,
                    NULL,
                    G_TYPE_NONE, 0);

  gtk_widget_class_set_activate_signal (widget_class, signals[ACTIVATE]);

  gtk_widget_class_set_css_name (widget_class, I_("menubutton"));
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_BUTTON);
}

static void
set_arrow_type (GtkWidget    *arrow,
                GtkArrowType  arrow_type,
                gboolean      visible)
{
  gtk_widget_remove_css_class (arrow, "none");
  gtk_widget_remove_css_class (arrow, "down");
  gtk_widget_remove_css_class (arrow, "up");
  gtk_widget_remove_css_class (arrow, "left");
  gtk_widget_remove_css_class (arrow, "right");
  switch (arrow_type)
    {
    case GTK_ARROW_NONE:
      gtk_widget_add_css_class (arrow, "none");
      break;
    case GTK_ARROW_DOWN:
      gtk_widget_add_css_class (arrow, "down");
      break;
    case GTK_ARROW_UP:
      gtk_widget_add_css_class (arrow, "up");
      break;
    case GTK_ARROW_LEFT:
      gtk_widget_add_css_class (arrow, "left");
      break;
    case GTK_ARROW_RIGHT:
      gtk_widget_add_css_class (arrow, "right");
      break;
    default:
      break;
    }

  gtk_widget_set_visible (arrow, visible);
}

static void
update_style_classes (GtkMenuButton *menu_button)
{
  gboolean has_icon = menu_button->image_widget != NULL;
  gboolean has_label = menu_button->label_widget != NULL;
  gboolean has_only_arrow = menu_button->arrow_widget == gtk_button_get_child (GTK_BUTTON (menu_button->button));
  gboolean has_arrow = gtk_widget_get_visible (menu_button->arrow_widget);

  if (has_only_arrow || has_icon)
    gtk_widget_add_css_class (menu_button->button, "image-button");
  else
    gtk_widget_remove_css_class (menu_button->button, "image-button");

  if (has_label)
    gtk_widget_add_css_class (menu_button->button, "text-button");
  else
    gtk_widget_remove_css_class (menu_button->button, "text-button");

  if (has_arrow && !has_only_arrow)
    gtk_widget_add_css_class (menu_button->button, "arrow-button");
  else
    gtk_widget_remove_css_class (menu_button->button, "arrow-button");
}

static void
update_arrow (GtkMenuButton *menu_button)
{
  gboolean has_only_arrow, is_text_button;

  if (menu_button->arrow_widget == NULL)
    return;

  has_only_arrow = menu_button->arrow_widget == gtk_button_get_child (GTK_BUTTON (menu_button->button));
  is_text_button = menu_button->label_widget != NULL;

  set_arrow_type (menu_button->arrow_widget,
                  menu_button->arrow_type,
                  has_only_arrow ||
                  ((is_text_button || menu_button->always_show_arrow) &&
                   (menu_button->arrow_type != GTK_ARROW_NONE)));

  update_style_classes (menu_button);
}

static void
add_arrow (GtkMenuButton *self)
{
  GtkWidget *arrow;

  arrow = gtk_builtin_icon_new ("arrow");
  gtk_widget_set_halign (arrow, GTK_ALIGN_CENTER);
  set_arrow_type (arrow, self->arrow_type, TRUE);
  gtk_button_set_child (GTK_BUTTON (self->button), arrow);
  self->arrow_widget = arrow;
}

static void
gtk_menu_button_init (GtkMenuButton *self)
{
  self->arrow_type = GTK_ARROW_DOWN;

  self->button = gtk_toggle_button_new ();
  gtk_widget_set_parent (self->button, GTK_WIDGET (self));
  g_signal_connect_swapped (self->button, "toggled", G_CALLBACK (gtk_menu_button_toggled), self);
  add_arrow (self);
  update_style_classes (self);

  gtk_widget_set_sensitive (self->button, FALSE);

  gtk_widget_add_css_class (GTK_WIDGET (self), "popup");
}

static GtkBuildableIface *parent_buildable_iface;

static void
gtk_menu_button_buildable_add_child (GtkBuildable *buildable,
                                     GtkBuilder   *builder,
                                     GObject      *child,
                                     const char   *type)
{
  if (GTK_IS_WIDGET (child))
    gtk_menu_button_set_child (GTK_MENU_BUTTON (buildable), GTK_WIDGET (child));
  else
    parent_buildable_iface->add_child (buildable, builder, child, type);
}

static void
gtk_menu_button_buildable_iface_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);

  iface->add_child = gtk_menu_button_buildable_add_child;
}

/**
 * gtk_menu_button_new:
 *
 * Creates a new `GtkMenuButton` widget with downwards-pointing
 * arrow as the only child.
 *
 * You can replace the child widget with another `GtkWidget`
 * should you wish to.
 *
 * Returns: The newly created `GtkMenuButton`
 */
GtkWidget *
gtk_menu_button_new (void)
{
  return g_object_new (GTK_TYPE_MENU_BUTTON, NULL);
}

static void
update_sensitivity (GtkMenuButton *self)
{
  gboolean has_popup;

  has_popup = self->popover != NULL || self->create_popup_func != NULL;

  gtk_widget_set_sensitive (self->button, has_popup);

  gtk_accessible_update_property (GTK_ACCESSIBLE (self),
                                  GTK_ACCESSIBLE_PROPERTY_HAS_POPUP, has_popup,
                                  -1);
  if (self->popover != NULL)
    gtk_accessible_update_relation (GTK_ACCESSIBLE (self),
                                    GTK_ACCESSIBLE_RELATION_CONTROLS, self->popover, NULL,
                                    -1);
  else
    gtk_accessible_reset_relation (GTK_ACCESSIBLE (self),
                                   GTK_ACCESSIBLE_RELATION_CONTROLS);
}

static gboolean
menu_deactivate_cb (GtkMenuButton *self)
{
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->button), FALSE);

  return TRUE;
}

/**
 * gtk_menu_button_set_menu_model: (attributes org.gtk.Method.set_property=menu-model)
 * @menu_button: a `GtkMenuButton`
 * @menu_model: (nullable): a `GMenuModel`, or %NULL to unset and disable the
 *   button
 *
 * Sets the `GMenuModel` from which the popup will be constructed.
 *
 * If @menu_model is %NULL, the button is disabled.
 *
 * A [class@Gtk.Popover] will be created from the menu model with
 * [ctor@Gtk.PopoverMenu.new_from_model]. Actions will be connected
 * as documented for this function.
 *
 * If [property@Gtk.MenuButton:popover] is already set, it will be
 * dissociated from the @menu_button, and the property is set to %NULL.
 */
void
gtk_menu_button_set_menu_model (GtkMenuButton *menu_button,
                                GMenuModel    *menu_model)
{
  g_return_if_fail (GTK_IS_MENU_BUTTON (menu_button));
  g_return_if_fail (G_IS_MENU_MODEL (menu_model) || menu_model == NULL);

  g_object_freeze_notify (G_OBJECT (menu_button));

  if (menu_model)
    g_object_ref (menu_model);

  if (menu_model)
    {
      GtkWidget *popover;

      popover = gtk_popover_menu_new_from_model (menu_model);

      gtk_accessible_update_relation (GTK_ACCESSIBLE (popover),
                                      GTK_ACCESSIBLE_RELATION_LABELLED_BY, menu_button, NULL,
                                      -1);

      gtk_menu_button_set_popover (menu_button, popover);
    }
  else
    {
      gtk_menu_button_set_popover (menu_button, NULL);
    }

  menu_button->model = menu_model;
  g_object_notify_by_pspec (G_OBJECT (menu_button), menu_button_props[PROP_MENU_MODEL]);

  g_object_thaw_notify (G_OBJECT (menu_button));
}

/**
 * gtk_menu_button_get_menu_model: (attributes org.gtk.Method.get_property=menu-model)
 * @menu_button: a `GtkMenuButton`
 *
 * Returns the `GMenuModel` used to generate the popup.
 *
 * Returns: (nullable) (transfer none): a `GMenuModel`
 */
GMenuModel *
gtk_menu_button_get_menu_model (GtkMenuButton *menu_button)
{
  g_return_val_if_fail (GTK_IS_MENU_BUTTON (menu_button), NULL);

  return menu_button->model;
}

static void
update_popover_direction (GtkMenuButton *self)
{
  if (!self->popover)
    return;

  switch (self->arrow_type)
    {
    case GTK_ARROW_UP:
      gtk_popover_set_position (GTK_POPOVER (self->popover), GTK_POS_TOP);
      break;
    case GTK_ARROW_DOWN:
    case GTK_ARROW_NONE:
      gtk_popover_set_position (GTK_POPOVER (self->popover), GTK_POS_BOTTOM);
      break;
    case GTK_ARROW_LEFT:
      gtk_popover_set_position (GTK_POPOVER (self->popover), GTK_POS_LEFT);
      break;
    case GTK_ARROW_RIGHT:
      gtk_popover_set_position (GTK_POPOVER (self->popover), GTK_POS_RIGHT);
      break;
    default:
      break;
    }
}

static void
popover_destroy_cb (GtkMenuButton *menu_button)
{
  gtk_menu_button_set_popover (menu_button, NULL);
}

/**
 * gtk_menu_button_set_direction: (attributes org.gtk.Method.set_property=direction)
 * @menu_button: a `GtkMenuButton`
 * @direction: a `GtkArrowType`
 *
 * Sets the direction in which the popup will be popped up.
 *
 * If the button is automatically populated with an arrow icon,
 * its direction will be changed to match.
 *
 * If the does not fit in the available space in the given direction,
 * GTK will its best to keep it inside the screen and fully visible.
 *
 * If you pass %GTK_ARROW_NONE for a @direction, the popup will behave
 * as if you passed %GTK_ARROW_DOWN (although you won’t see any arrows).
 */
void
gtk_menu_button_set_direction (GtkMenuButton *menu_button,
                               GtkArrowType   direction)
{
  g_return_if_fail (GTK_IS_MENU_BUTTON (menu_button));

  if (menu_button->arrow_type == direction)
    return;

  menu_button->arrow_type = direction;
  g_object_notify_by_pspec (G_OBJECT (menu_button), menu_button_props[PROP_DIRECTION]);

  update_arrow (menu_button);
  update_popover_direction (menu_button);
}

/**
 * gtk_menu_button_get_direction: (attributes org.gtk.Method.get_property=direction)
 * @menu_button: a `GtkMenuButton`
 *
 * Returns the direction the popup will be pointing at when popped up.
 *
 * Returns: a `GtkArrowType` value
 */
GtkArrowType
gtk_menu_button_get_direction (GtkMenuButton *menu_button)
{
  g_return_val_if_fail (GTK_IS_MENU_BUTTON (menu_button), GTK_ARROW_DOWN);

  return menu_button->arrow_type;
}

static void
gtk_menu_button_dispose (GObject *object)
{
  GtkMenuButton *self = GTK_MENU_BUTTON (object);

  if (self->popover)
    {
      g_signal_handlers_disconnect_by_func (self->popover,
                                            menu_deactivate_cb,
                                            object);
      g_signal_handlers_disconnect_by_func (self->popover,
                                            popover_destroy_cb,
                                            object);
      gtk_widget_unparent (self->popover);
      self->popover = NULL;
    }

  g_clear_object (&self->model);
  g_clear_pointer (&self->button, gtk_widget_unparent);

  if (self->create_popup_destroy_notify)
    self->create_popup_destroy_notify (self->create_popup_user_data);

  G_OBJECT_CLASS (gtk_menu_button_parent_class)->dispose (object);
}

/**
 * gtk_menu_button_set_popover: (attributes org.gtk.Method.set_property=popover)
 * @menu_button: a `GtkMenuButton`
 * @popover: (nullable): a `GtkPopover`, or %NULL to unset and disable the button
 *
 * Sets the `GtkPopover` that will be popped up when the @menu_button is clicked.
 *
 * If @popover is %NULL, the button is disabled.
 *
 * If [property@Gtk.MenuButton:menu-model] is set, the menu model is dissociated
 * from the @menu_button, and the property is set to %NULL.
 */
void
gtk_menu_button_set_popover (GtkMenuButton *menu_button,
                             GtkWidget     *popover)
{
  g_return_if_fail (GTK_IS_MENU_BUTTON (menu_button));
  g_return_if_fail (GTK_IS_POPOVER (popover) || popover == NULL);

  g_object_freeze_notify (G_OBJECT (menu_button));

  g_clear_object (&menu_button->model);

  if (menu_button->popover)
    {
      gtk_widget_set_visible (menu_button->popover, FALSE);

      g_signal_handlers_disconnect_by_func (menu_button->popover,
                                            menu_deactivate_cb,
                                            menu_button);
      g_signal_handlers_disconnect_by_func (menu_button->popover,
                                            popover_destroy_cb,
                                            menu_button);

      gtk_widget_unparent (menu_button->popover);
    }

  menu_button->popover = popover;

  if (popover)
    {
      gtk_widget_set_parent (menu_button->popover, GTK_WIDGET (menu_button));
      g_signal_connect_swapped (menu_button->popover, "closed",
                                G_CALLBACK (menu_deactivate_cb), menu_button);
      g_signal_connect_swapped (menu_button->popover, "destroy",
                                G_CALLBACK (popover_destroy_cb), menu_button);
      update_popover_direction (menu_button);
    }

  update_sensitivity (menu_button);

  g_object_notify_by_pspec (G_OBJECT (menu_button), menu_button_props[PROP_POPOVER]);
  g_object_notify_by_pspec (G_OBJECT (menu_button), menu_button_props[PROP_MENU_MODEL]);
  g_object_thaw_notify (G_OBJECT (menu_button));
}

/**
 * gtk_menu_button_get_popover: (attributes org.gtk.Method.get_property=popover)
 * @menu_button: a `GtkMenuButton`
 *
 * Returns the `GtkPopover` that pops out of the button.
 *
 * If the button is not using a `GtkPopover`, this function
 * returns %NULL.
 *
 * Returns: (nullable) (transfer none): a `GtkPopover` or %NULL
 */
GtkPopover *
gtk_menu_button_get_popover (GtkMenuButton *menu_button)
{
  g_return_val_if_fail (GTK_IS_MENU_BUTTON (menu_button), NULL);

  return GTK_POPOVER (menu_button->popover);
}

/**
 * gtk_menu_button_set_icon_name: (attributes org.gtk.Method.set_property=icon-name)
 * @menu_button: a `GtkMenuButton`
 * @icon_name: the icon name
 *
 * Sets the name of an icon to show inside the menu button.
 *
 * Setting icon name resets [property@Gtk.MenuButton:label] and
 * [property@Gtk.MenuButton:child].
 *
 * If [property@Gtk.MenuButton:always-show-arrow] is set to `TRUE` and
 * [property@Gtk.MenuButton:direction] is not `GTK_ARROW_NONE`, a dropdown arrow
 * will be shown next to the icon.
 */
void
gtk_menu_button_set_icon_name (GtkMenuButton *menu_button,
                               const char    *icon_name)
{
  GtkWidget *box, *image_widget, *arrow;

  g_return_if_fail (GTK_IS_MENU_BUTTON (menu_button));

  g_object_freeze_notify (G_OBJECT (menu_button));

  if (gtk_menu_button_get_label (menu_button))
    g_object_notify_by_pspec (G_OBJECT (menu_button), menu_button_props[PROP_LABEL]);

  if (gtk_menu_button_get_child (menu_button))
    g_object_notify_by_pspec (G_OBJECT (menu_button), menu_button_props[PROP_CHILD]);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_halign (box, GTK_ALIGN_CENTER);

  image_widget = g_object_new (GTK_TYPE_IMAGE,
                               "accessible-role", GTK_ACCESSIBLE_ROLE_PRESENTATION,
                               "icon-name", icon_name,
                               NULL);
  menu_button->image_widget = image_widget;

  arrow = gtk_builtin_icon_new ("arrow");
  menu_button->arrow_widget = arrow;

  gtk_box_append (GTK_BOX (box), image_widget);
  gtk_box_append (GTK_BOX (box), arrow);
  gtk_button_set_child (GTK_BUTTON (menu_button->button), box);

  menu_button->label_widget = NULL;
  menu_button->child = NULL;

  update_arrow (menu_button);

  g_object_notify_by_pspec (G_OBJECT (menu_button), menu_button_props[PROP_ICON_NAME]);

  g_object_thaw_notify (G_OBJECT (menu_button));
}

/**
 * gtk_menu_button_get_icon_name: (attributes org.gtk.Method.get_property=icon-name)
 * @menu_button: a `GtkMenuButton`
 *
 * Gets the name of the icon shown in the button.
 *
 * Returns: (nullable): the name of the icon shown in the button
 */
const char *
gtk_menu_button_get_icon_name (GtkMenuButton *menu_button)
{
  g_return_val_if_fail (GTK_IS_MENU_BUTTON (menu_button), NULL);

  if (menu_button->image_widget)
    return gtk_image_get_icon_name (GTK_IMAGE (menu_button->image_widget));

  return NULL;
}

/**
 * gtk_menu_button_set_always_show_arrow: (attributes org.gtk.Method.set_property=always-show-arrow)
 * @menu_button: a `GtkMenuButton`
 * @always_show_arrow: whether to show a dropdown arrow even when using an icon
 * or a custom child
 *
 * Sets whether to show a dropdown arrow even when using an icon or a custom
 * child.
 *
 * Since: 4.4
 */
void
gtk_menu_button_set_always_show_arrow (GtkMenuButton *menu_button,
                                       gboolean       always_show_arrow)
{
  g_return_if_fail (GTK_IS_MENU_BUTTON (menu_button));

  always_show_arrow = !!always_show_arrow;

  if (always_show_arrow == menu_button->always_show_arrow)
    return;

  menu_button->always_show_arrow = always_show_arrow;

  update_arrow (menu_button);

  g_object_notify_by_pspec (G_OBJECT (menu_button), menu_button_props[PROP_ALWAYS_SHOW_ARROW]);
}

/**
 * gtk_menu_button_get_always_show_arrow: (attributes org.gtk.Method.get_property=always-show-arrow)
 * @menu_button: a `GtkMenuButton`
 *
 * Gets whether to show a dropdown arrow even when using an icon or a custom
 * child.
 *
 * Returns: whether to show a dropdown arrow even when using an icon or a custom
 * child.
 *
 * Since: 4.4
 */
gboolean
gtk_menu_button_get_always_show_arrow (GtkMenuButton *menu_button)
{
  g_return_val_if_fail (GTK_IS_MENU_BUTTON (menu_button), FALSE);

  return menu_button->always_show_arrow;
}

/**
 * gtk_menu_button_set_label: (attributes org.gtk.Method.set_property=label)
 * @menu_button: a `GtkMenuButton`
 * @label: the label
 *
 * Sets the label to show inside the menu button.
 *
 * Setting a label resets [property@Gtk.MenuButton:icon-name] and
 * [property@Gtk.MenuButton:child].
 *
 * If [property@Gtk.MenuButton:direction] is not `GTK_ARROW_NONE`, a dropdown
 * arrow will be shown next to the label.
 */
void
gtk_menu_button_set_label (GtkMenuButton *menu_button,
                           const char    *label)
{
  GtkWidget *box;
  GtkWidget *label_widget;
  GtkWidget *arrow;

  g_return_if_fail (GTK_IS_MENU_BUTTON (menu_button));

  g_object_freeze_notify (G_OBJECT (menu_button));

  if (gtk_menu_button_get_icon_name (menu_button))
    g_object_notify_by_pspec (G_OBJECT (menu_button), menu_button_props[PROP_ICON_NAME]);
  if (gtk_menu_button_get_child (menu_button))
    g_object_notify_by_pspec (G_OBJECT (menu_button), menu_button_props[PROP_CHILD]);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_hexpand (box, FALSE);
  label_widget = gtk_label_new (label);
  gtk_label_set_use_underline (GTK_LABEL (label_widget),
                               gtk_button_get_use_underline (GTK_BUTTON (menu_button->button)));
  gtk_label_set_ellipsize (GTK_LABEL (label_widget),
                           menu_button->can_shrink ? PANGO_ELLIPSIZE_END
                                                   : PANGO_ELLIPSIZE_NONE);
  gtk_widget_set_hexpand (label_widget, TRUE);
  arrow = gtk_builtin_icon_new ("arrow");
  menu_button->arrow_widget = arrow;
  gtk_box_append (GTK_BOX (box), label_widget);
  gtk_box_append (GTK_BOX (box), arrow);
  gtk_button_set_child (GTK_BUTTON (menu_button->button), box);
  menu_button->label_widget = label_widget;

  menu_button->image_widget = NULL;
  menu_button->child = NULL;

  update_arrow (menu_button);

  g_object_notify_by_pspec (G_OBJECT (menu_button), menu_button_props[PROP_LABEL]);

  g_object_thaw_notify (G_OBJECT (menu_button));
}

/**
 * gtk_menu_button_get_label: (attributes org.gtk.Method.get_property=label)
 * @menu_button: a `GtkMenuButton`
 *
 * Gets the label shown in the button
 *
 * Returns: (nullable): the label shown in the button
 */
const char *
gtk_menu_button_get_label (GtkMenuButton *menu_button)
{
  g_return_val_if_fail (GTK_IS_MENU_BUTTON (menu_button), NULL);

  if (menu_button->label_widget)
    return gtk_label_get_label (GTK_LABEL (menu_button->label_widget));

  return NULL;
}

/**
 * gtk_menu_button_set_has_frame: (attributes org.gtk.Method.set_property=has-frame)
 * @menu_button: a `GtkMenuButton`
 * @has_frame: whether the button should have a visible frame
 *
 * Sets the style of the button.
 */
void
gtk_menu_button_set_has_frame (GtkMenuButton *menu_button,
                               gboolean       has_frame)
{
  g_return_if_fail (GTK_IS_MENU_BUTTON (menu_button));

  if (gtk_button_get_has_frame (GTK_BUTTON (menu_button->button)) == has_frame)
    return;

  gtk_button_set_has_frame (GTK_BUTTON (menu_button->button), has_frame);
  g_object_notify_by_pspec (G_OBJECT (menu_button), menu_button_props[PROP_HAS_FRAME]);
}

/**
 * gtk_menu_button_get_has_frame: (attributes org.gtk.Method.get_property=has-frame)
 * @menu_button: a `GtkMenuButton`
 *
 * Returns whether the button has a frame.
 *
 * Returns: %TRUE if the button has a frame
 */
gboolean
gtk_menu_button_get_has_frame (GtkMenuButton *menu_button)
{
  g_return_val_if_fail (GTK_IS_MENU_BUTTON (menu_button), TRUE);

  return gtk_button_get_has_frame (GTK_BUTTON (menu_button->button));
}

/**
 * gtk_menu_button_popup:
 * @menu_button: a `GtkMenuButton`
 *
 * Pop up the menu.
 */
void
gtk_menu_button_popup (GtkMenuButton *menu_button)
{
  g_return_if_fail (GTK_IS_MENU_BUTTON (menu_button));

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (menu_button->button), TRUE);
}

/**
 * gtk_menu_button_popdown:
 * @menu_button: a `GtkMenuButton`
 *
 * Dismiss the menu.
 */
void
gtk_menu_button_popdown (GtkMenuButton *menu_button)
{
  g_return_if_fail (GTK_IS_MENU_BUTTON (menu_button));

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (menu_button->button), FALSE);
}

/**
 * gtk_menu_button_set_create_popup_func:
 * @menu_button: a `GtkMenuButton`
 * @func: (nullable): function to call when a popup is about to
 *   be shown, but none has been provided via other means, or %NULL
 *   to reset to default behavior.
 * @user_data: (closure): user data to pass to @func.
 * @destroy_notify: (nullable): destroy notify for @user_data
 *
 * Sets @func to be called when a popup is about to be shown.
 *
 * @func should use one of
 *
 *  - [method@Gtk.MenuButton.set_popover]
 *  - [method@Gtk.MenuButton.set_menu_model]
 *
 * to set a popup for @menu_button.
 * If @func is non-%NULL, @menu_button will always be sensitive.
 *
 * Using this function will not reset the menu widget attached to
 * @menu_button. Instead, this can be done manually in @func.
 */
void
gtk_menu_button_set_create_popup_func (GtkMenuButton                *menu_button,
                                       GtkMenuButtonCreatePopupFunc  func,
                                       gpointer                      user_data,
                                       GDestroyNotify                destroy_notify)
{
  g_return_if_fail (GTK_IS_MENU_BUTTON (menu_button));

  if (menu_button->create_popup_destroy_notify)
    menu_button->create_popup_destroy_notify (menu_button->create_popup_user_data);

  menu_button->create_popup_func = func;
  menu_button->create_popup_user_data = user_data;
  menu_button->create_popup_destroy_notify = destroy_notify;

  update_sensitivity (menu_button);
}

/**
 * gtk_menu_button_set_use_underline: (attributes org.gtk.Method.set_property=use-underline)
 * @menu_button: a `GtkMenuButton`
 * @use_underline: %TRUE if underlines in the text indicate mnemonics
 *
 * If true, an underline in the text indicates a mnemonic.
 */
void
gtk_menu_button_set_use_underline (GtkMenuButton *menu_button,
                                   gboolean       use_underline)
{
  g_return_if_fail (GTK_IS_MENU_BUTTON (menu_button));

  if (gtk_button_get_use_underline (GTK_BUTTON (menu_button->button)) == use_underline)
    return;

  gtk_button_set_use_underline (GTK_BUTTON (menu_button->button), use_underline);
  if (menu_button->label_widget)
    gtk_label_set_use_underline (GTK_LABEL (menu_button->label_widget), use_underline);

  g_object_notify_by_pspec (G_OBJECT (menu_button), menu_button_props[PROP_USE_UNDERLINE]);
}

/**
 * gtk_menu_button_get_use_underline: (attributes org.gtk.Method.get_property=use-underline)
 * @menu_button: a `GtkMenuButton`
 *
 * Returns whether an embedded underline in the text indicates a
 * mnemonic.
 *
 * Returns: %TRUE whether an embedded underline in the text indicates
 *   the mnemonic accelerator keys.
 */
gboolean
gtk_menu_button_get_use_underline (GtkMenuButton *menu_button)
{
  g_return_val_if_fail (GTK_IS_MENU_BUTTON (menu_button), FALSE);

  return gtk_button_get_use_underline (GTK_BUTTON (menu_button->button));
}

static GList *
get_menu_bars (GtkWidget *toplevel)
{
  return g_object_get_data (G_OBJECT (toplevel), "gtk-menu-bar-list");
}

static void
set_menu_bars (GtkWidget *toplevel,
               GList     *menubars)
{
  g_object_set_data (G_OBJECT (toplevel), I_("gtk-menu-bar-list"), menubars);
}

static void
add_to_toplevel (GtkWidget     *toplevel,
                 GtkMenuButton *button)
{
  GList *menubars = get_menu_bars (toplevel);

  set_menu_bars (toplevel, g_list_prepend (menubars, button));
}

static void
remove_from_toplevel (GtkWidget     *toplevel,
                      GtkMenuButton *button)
{
  GList *menubars = get_menu_bars (toplevel);

  menubars = g_list_remove (menubars, button);
  set_menu_bars (toplevel, menubars);
}

static void
gtk_menu_button_root (GtkWidget *widget)
{
  GtkMenuButton *button = GTK_MENU_BUTTON (widget);

  GTK_WIDGET_CLASS (gtk_menu_button_parent_class)->root (widget);

  if (button->primary)
    {
      GtkWidget *toplevel = GTK_WIDGET (gtk_widget_get_root (widget));
      add_to_toplevel (toplevel, button);
    }
}

static void
gtk_menu_button_unroot (GtkWidget *widget)
{
  GtkWidget *toplevel;

  toplevel = GTK_WIDGET (gtk_widget_get_root (widget));
  remove_from_toplevel (toplevel, GTK_MENU_BUTTON (widget));

  GTK_WIDGET_CLASS (gtk_menu_button_parent_class)->unroot (widget);
}

/**
 * gtk_menu_button_set_primary: (attributes org.gtk.Method.set_property=primary)
 * @menu_button: a `GtkMenuButton`
 * @primary: whether the menubutton should act as a primary menu
 *
 * Sets whether menu button acts as a primary menu.
 *
 * Primary menus can be opened with the <kbd>F10</kbd> key.
 *
 * Since: 4.4
 */
void
gtk_menu_button_set_primary (GtkMenuButton *menu_button,
                             gboolean       primary)
{
  GtkRoot *toplevel;

  g_return_if_fail (GTK_IS_MENU_BUTTON (menu_button));

  if (menu_button->primary == primary)
    return;

  menu_button->primary = primary;
  toplevel = gtk_widget_get_root (GTK_WIDGET (menu_button));

  if (toplevel)
    {
      if (menu_button->primary)
        add_to_toplevel (GTK_WIDGET (toplevel), menu_button);
      else
        remove_from_toplevel (GTK_WIDGET (toplevel), menu_button);
    }

  g_object_notify_by_pspec (G_OBJECT (menu_button), menu_button_props[PROP_PRIMARY]);
}

/**
 * gtk_menu_button_get_primary: (attributes org.gtk.Method.get_property=primary)
 * @menu_button: a `GtkMenuButton`
 *
 * Returns whether the menu button acts as a primary menu.
 *
 * Returns: %TRUE if the button is a primary menu
 *
 * Since: 4.4
 */
gboolean
gtk_menu_button_get_primary (GtkMenuButton *menu_button)
{
  g_return_val_if_fail (GTK_IS_MENU_BUTTON (menu_button), FALSE);

  return menu_button->primary;
}

/**
 * gtk_menu_button_set_child: (attributes org.gtk.Method.set_property=child)
 * @menu_button: a `GtkMenuButton`
 * @child: (nullable): the child widget
 *
 * Sets the child widget of @menu_button.
 *
 * Setting a child resets [property@Gtk.MenuButton:label] and
 * [property@Gtk.MenuButton:icon-name].
 *
 * If [property@Gtk.MenuButton:always-show-arrow] is set to `TRUE` and
 * [property@Gtk.MenuButton:direction] is not `GTK_ARROW_NONE`, a dropdown arrow
 * will be shown next to the child.
 *
 * Since: 4.6
 */
void
gtk_menu_button_set_child (GtkMenuButton *menu_button,
                           GtkWidget     *child)
{
  GtkWidget *box, *arrow, *inner_widget;

  g_return_if_fail (GTK_IS_MENU_BUTTON (menu_button));
  g_return_if_fail (child == NULL || menu_button->child == child || gtk_widget_get_parent (child) == NULL);

  if (menu_button->child == child)
    return;

  g_object_freeze_notify (G_OBJECT (menu_button));

  if (gtk_menu_button_get_label (menu_button))
    g_object_notify_by_pspec (G_OBJECT (menu_button), menu_button_props[PROP_LABEL]);
  if (gtk_menu_button_get_icon_name (menu_button))
    g_object_notify_by_pspec (G_OBJECT (menu_button), menu_button_props[PROP_ICON_NAME]);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_hexpand (box, FALSE);

  arrow = gtk_builtin_icon_new ("arrow");
  menu_button->arrow_widget = arrow;

  inner_widget = gtk_gizmo_new_with_role ("contents",
                                          GTK_ACCESSIBLE_ROLE_GROUP,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL,
                                          (GtkGizmoFocusFunc)gtk_widget_focus_child,
                                          NULL);

  gtk_widget_set_layout_manager (inner_widget, gtk_bin_layout_new ());
  gtk_widget_set_hexpand (inner_widget, TRUE);
  if (child)
    gtk_widget_set_parent (child, inner_widget);

  gtk_box_append (GTK_BOX (box), inner_widget);
  gtk_box_append (GTK_BOX (box), arrow);
  gtk_button_set_child (GTK_BUTTON (menu_button->button), box);

  menu_button->child = child;

  menu_button->image_widget = NULL;
  menu_button->label_widget = NULL;

  update_arrow (menu_button);

  g_object_notify_by_pspec (G_OBJECT (menu_button), menu_button_props[PROP_CHILD]);

  g_object_thaw_notify (G_OBJECT (menu_button));
}

/**
 * gtk_menu_button_get_child: (attributes org.gtk.Method.get_property=child)
 * @menu_button: a `GtkMenuButton`
 *
 * Gets the child widget of @menu_button.
 *
 * Returns: (nullable) (transfer none): the child widget of @menu_button
 *
 * Since: 4.6
 */
GtkWidget *
gtk_menu_button_get_child (GtkMenuButton *menu_button)
{
  g_return_val_if_fail (GTK_IS_MENU_BUTTON (menu_button), NULL);

  return menu_button->child;
}

/**
 * gtk_menu_button_set_active: (attributes org.gtk.Method.set_property=active)
 * @menu_button: a `GtkMenuButton`
 * @active: whether the menu button is active
 *
 * Sets whether the menu button is active.
 *
 * Since: 4.10
 */
void
gtk_menu_button_set_active (GtkMenuButton *menu_button,
                            gboolean       active)
{
  g_return_if_fail (GTK_IS_MENU_BUTTON (menu_button));

  if (active == gtk_menu_button_get_active (menu_button))
    return;

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (menu_button->button),
                                active);

  g_object_notify_by_pspec (G_OBJECT (menu_button), menu_button_props[PROP_ACTIVE]);
}

/**
 * gtk_menu_button_get_active: (attributes org.gtk.Method.get_property=active)
 * @menu_button: a `GtkMenuButton`
 *
 * Returns whether the menu button is active.
 *
 * Returns: TRUE if the button is active
 *
 * Since: 4.10
 */
gboolean
gtk_menu_button_get_active (GtkMenuButton *menu_button)
{
  g_return_val_if_fail (GTK_IS_MENU_BUTTON (menu_button), FALSE);

  return gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (menu_button->button));
}

/**
 * gtk_menu_button_set_can_shrink:
 * @menu_button: a menu button
 * @can_shrink: whether the button can shrink
 *
 * Sets whether the button size can be smaller than the natural size of
 * its contents.
 *
 * For text buttons, setting @can_shrink to true will ellipsize the label.
 *
 * For icon buttons, this function has no effect.
 *
 * Since: 4.12
 */
void
gtk_menu_button_set_can_shrink (GtkMenuButton *menu_button,
                                gboolean       can_shrink)
{
  g_return_if_fail (GTK_IS_MENU_BUTTON (menu_button));

  can_shrink = !!can_shrink;

  if (menu_button->can_shrink == can_shrink)
    return;

  menu_button->can_shrink = can_shrink;

  if (menu_button->label_widget != NULL)
    {
      gtk_label_set_ellipsize (GTK_LABEL (menu_button->label_widget),
                               can_shrink ? PANGO_ELLIPSIZE_END
                                          : PANGO_ELLIPSIZE_NONE);
    }

  g_object_notify_by_pspec (G_OBJECT (menu_button), menu_button_props[PROP_CAN_SHRINK]);
}

/**
 * gtk_menu_button_get_can_shrink:
 * @menu_button: a button
 *
 * Retrieves whether the button can be smaller than the natural
 * size of its contents.
 *
 * Returns: true if the button can shrink, and false otherwise
 *
 * Since: 4.12
 */
gboolean
gtk_menu_button_get_can_shrink (GtkMenuButton *menu_button)
{
  g_return_val_if_fail (GTK_IS_MENU_BUTTON (menu_button), FALSE);

  return menu_button->can_shrink;
}
