/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

/*
 * Modified by the GTK+ Team and others 1997-2001.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

/**
 * GtkButton:
 *
 * The `GtkButton` widget is generally used to trigger a callback function that is
 * called when the button is pressed.
 *
 * ![An example GtkButton](button.png)
 *
 * The `GtkButton` widget can hold any valid child widget. That is, it can hold
 * almost any other standard `GtkWidget`. The most commonly used child is the
 * `GtkLabel`.
 *
 * # CSS nodes
 *
 * `GtkButton` has a single CSS node with name button. The node will get the
 * style classes .image-button or .text-button, if the content is just an
 * image or label, respectively. It may also receive the .flat style class.
 * When activating a button via the keyboard, the button will temporarily
 * gain the .keyboard-activating style class.
 *
 * Other style classes that are commonly used with `GtkButton` include
 * .suggested-action and .destructive-action. In special cases, buttons
 * can be made round by adding the .circular style class.
 *
 * Button-like widgets like [class@Gtk.ToggleButton], [class@Gtk.MenuButton],
 * [class@Gtk.VolumeButton], [class@Gtk.LockButton], [class@Gtk.ColorButton]
 * or [class@Gtk.FontButton] use style classes such as .toggle, .popup, .scale,
 * .lock, .color on the button node to differentiate themselves from a plain
 * `GtkButton`.
 *
 * # Accessibility
 *
 * `GtkButton` uses the %GTK_ACCESSIBLE_ROLE_BUTTON role.
 */

#include "config.h"

#include "gtkbuttonprivate.h"

#include "gtkactionhelperprivate.h"
#include "gtkbuildable.h"
#include "gtkgestureclick.h"
#include "gtkeventcontrollerkey.h"
#include "gtkbinlayout.h"
#include "gtkimage.h"
#include "gtklabel.h"
#include "gtkmarshalers.h"
#include "gtkprivate.h"
#include "gtktypebuiltins.h"
#include "gtkwidgetprivate.h"
#include "gtkshortcuttrigger.h"

#include <string.h>

/* Time out before giving up on getting a key release when animating
 * the close button.
 */
#define ACTIVATE_TIMEOUT 250

struct _GtkButtonPrivate
{
  GtkWidget             *child;

  GtkActionHelper       *action_helper;

  GtkGesture            *gesture;

  guint                  activate_timeout;

  guint          button_down           : 1;
  guint          use_underline         : 1;
  guint          child_type            : 2;
  guint          can_shrink            : 1;
};

enum {
  CLICKED,
  ACTIVATE,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_LABEL,
  PROP_HAS_FRAME,
  PROP_USE_UNDERLINE,
  PROP_ICON_NAME,
  PROP_CHILD,
  PROP_CAN_SHRINK,

  /* actionable properties */
  PROP_ACTION_NAME,
  PROP_ACTION_TARGET,
  LAST_PROP = PROP_ACTION_NAME
};

enum {
  LABEL_CHILD,
  ICON_CHILD,
  WIDGET_CHILD
};


static void gtk_button_dispose        (GObject            *object);
static void gtk_button_set_property   (GObject            *object,
                                       guint               prop_id,
                                       const GValue       *value,
                                       GParamSpec         *pspec);
static void gtk_button_get_property   (GObject            *object,
                                       guint               prop_id,
                                       GValue             *value,
                                       GParamSpec         *pspec);
static void gtk_button_unrealize (GtkWidget * widget);
static void gtk_real_button_clicked (GtkButton * button);
static void gtk_real_button_activate  (GtkButton          *button);
static void gtk_button_finish_activate (GtkButton         *button,
                                        gboolean           do_it);

static void gtk_button_state_flags_changed (GtkWidget     *widget,
                                            GtkStateFlags  previous_state);
static void gtk_button_do_release      (GtkButton             *button,
                                        gboolean               emit_clicked);
static void gtk_button_set_child_type (GtkButton *button, guint child_type);

static void gtk_button_buildable_iface_init      (GtkBuildableIface *iface);
static void gtk_button_actionable_iface_init     (GtkActionableInterface *iface);

static GParamSpec *props[LAST_PROP] = { NULL, };
static guint button_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_CODE (GtkButton, gtk_button, GTK_TYPE_WIDGET,
                         G_ADD_PRIVATE (GtkButton)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE, gtk_button_buildable_iface_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ACTIONABLE, gtk_button_actionable_iface_init))

static void
gtk_button_compute_expand (GtkWidget *widget,
                           gboolean  *hexpand,
                           gboolean  *vexpand)
{
  GtkButtonPrivate *priv = gtk_button_get_instance_private (GTK_BUTTON (widget));

  if (priv->child)
    {
      *hexpand = gtk_widget_compute_expand (priv->child, GTK_ORIENTATION_HORIZONTAL);
      *vexpand = gtk_widget_compute_expand (priv->child, GTK_ORIENTATION_VERTICAL);
    }
  else
    {
      *hexpand = FALSE;
      *vexpand = FALSE;
    }
}

static GtkSizeRequestMode
gtk_button_get_request_mode (GtkWidget *widget)
{
  GtkButtonPrivate *priv = gtk_button_get_instance_private (GTK_BUTTON (widget));

  if (priv->child)
    return gtk_widget_get_request_mode (priv->child);
  else
    return GTK_SIZE_REQUEST_CONSTANT_SIZE;
}

static void
gtk_button_class_init (GtkButtonClass *klass)
{
  const guint activate_keyvals[] = { GDK_KEY_space, GDK_KEY_KP_Space, GDK_KEY_Return,
                                     GDK_KEY_ISO_Enter, GDK_KEY_KP_Enter };
  GObjectClass *gobject_class;
  GtkWidgetClass *widget_class;
  GtkShortcutAction *activate_action;

  gobject_class = G_OBJECT_CLASS (klass);
  widget_class = (GtkWidgetClass*) klass;

  gobject_class->dispose      = gtk_button_dispose;
  gobject_class->set_property = gtk_button_set_property;
  gobject_class->get_property = gtk_button_get_property;

  widget_class->unrealize = gtk_button_unrealize;
  widget_class->state_flags_changed = gtk_button_state_flags_changed;
  widget_class->compute_expand = gtk_button_compute_expand;
  widget_class->get_request_mode = gtk_button_get_request_mode;

  klass->clicked = NULL;
  klass->activate = gtk_real_button_activate;

  /**
   * GtkButton:label: (attributes org.gtk.Property.get=gtk_button_get_label org.gtk.Property.set=gtk_button_set_label)
   *
   * Text of the label inside the button, if the button contains a label widget.
   */
  props[PROP_LABEL] =
    g_param_spec_string ("label", NULL, NULL,
                         NULL,
                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkButton:use-underline: (attributes org.gtk.Property.get=gtk_button_get_use_underline org.gtk.Property.set=gtk_button_set_use_underline)
   *
   * If set, an underline in the text indicates that the following character is
   * to be used as mnemonic.
   */
  props[PROP_USE_UNDERLINE] =
    g_param_spec_boolean ("use-underline", NULL, NULL,
                          FALSE,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkButton:has-frame: (attributes org.gtk.Property.get=gtk_button_get_has_frame org.gtk.Property.set=gtk_button_set_has_frame)
   *
   * Whether the button has a frame.
   */
  props[PROP_HAS_FRAME] =
    g_param_spec_boolean ("has-frame", NULL, NULL,
                          TRUE,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkButton:icon-name: (attributes org.gtk.Property.get=gtk_button_get_icon_name org.gtk.Property.set=gtk_button_set_icon_name)
   *
   * The name of the icon used to automatically populate the button.
   */
  props[PROP_ICON_NAME] =
    g_param_spec_string ("icon-name", NULL, NULL,
                         NULL,
                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkButton:child: (attributes org.gtk.Property.get=gtk_button_get_child org.gtk.Property.set=gtk_button_set_child)
   *
   * The child widget.
   */
  props[PROP_CHILD] =
    g_param_spec_object ("child", NULL, NULL,
                         GTK_TYPE_WIDGET,
                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkButton:can-shrink:
   *
   * Whether the size of the button can be made smaller than the natural
   * size of its contents.
   *
   * For text buttons, setting this property will allow ellipsizing the label.
   *
   * If the contents of a button are an icon or a custom widget, setting this
   * property has no effect.
   *
   * Since: 4.12
   */
  props[PROP_CAN_SHRINK] =
    g_param_spec_boolean ("can-shrink", NULL, NULL,
                          FALSE,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, LAST_PROP, props);

  g_object_class_override_property (gobject_class, PROP_ACTION_NAME, "action-name");
  g_object_class_override_property (gobject_class, PROP_ACTION_TARGET, "action-target");

  /**
   * GtkButton::clicked:
   * @button: the object that received the signal
   *
   * Emitted when the button has been activated (pressed and released).
   */
  button_signals[CLICKED] =
    g_signal_new (I_("clicked"),
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkButtonClass, clicked),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  /**
   * GtkButton::activate:
   * @widget: the object which received the signal.
   *
   * Emitted to animate press then release.
   *
   * This is an action signal. Applications should never connect
   * to this signal, but use the [signal@Gtk.Button::clicked] signal.
   *
   * The default bindings for this signal are all forms of the
   * <kbd>␣</kbd> and <kbd>Enter</kbd> keys.
   */
  button_signals[ACTIVATE] =
    g_signal_new (I_("activate"),
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkButtonClass, activate),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  gtk_widget_class_set_activate_signal (widget_class, button_signals[ACTIVATE]);

  activate_action = gtk_signal_action_new ("activate");
  for (guint i = 0; i < G_N_ELEMENTS (activate_keyvals); i++)
    {
      GtkShortcut *activate_shortcut = gtk_shortcut_new (gtk_keyval_trigger_new (activate_keyvals[i], 0),
                                                         g_object_ref (activate_action));

      gtk_widget_class_add_shortcut (widget_class, activate_shortcut);
      g_object_unref (activate_shortcut);
    }
  g_object_unref (activate_action);

  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_BUTTON);
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_css_name (widget_class, I_("button"));
}

static void
click_pressed_cb (GtkGestureClick *gesture,
                  guint            n_press,
                  double           x,
                  double           y,
                  GtkWidget       *widget)
{
  GtkButton *button = GTK_BUTTON (widget);
  GtkButtonPrivate *priv = gtk_button_get_instance_private (button);

  if (gtk_widget_get_focus_on_click (widget) && !gtk_widget_has_focus (widget))
    gtk_widget_grab_focus (widget);

  if (!priv->activate_timeout)
    priv->button_down = TRUE;
}

static void
click_released_cb (GtkGestureClick *gesture,
                   guint            n_press,
                   double           x,
                   double           y,
                   GtkWidget       *widget)
{
  GtkButton *button = GTK_BUTTON (widget);

  gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
  gtk_button_do_release (button,
                         gtk_widget_is_sensitive (GTK_WIDGET (button)) &&
                         gtk_widget_contains (widget, x, y));
}

static void
click_gesture_cancel_cb (GtkGesture       *gesture,
                         GdkEventSequence *sequence,
                         GtkButton        *button)
{
  GtkButtonPrivate *priv = gtk_button_get_instance_private (button);

  if (priv->activate_timeout)
    gtk_button_finish_activate (button, FALSE);

  gtk_button_do_release (button, FALSE);
}

static gboolean
key_controller_key_pressed_cb (GtkEventControllerKey *controller,
                               guint                  keyval,
                               guint                  keycode,
                               guint                  modifiers,
                               GtkButton             *button)
{
  GtkButtonPrivate *priv = gtk_button_get_instance_private (button);

  return priv->activate_timeout != 0;
}

static void
key_controller_key_released_cb (GtkEventControllerKey *controller,
                                guint                  keyval,
                                guint                  keycode,
                                guint                  modifiers,
                                GtkButton             *button)
{
  GtkButtonPrivate *priv = gtk_button_get_instance_private (button);

  if (priv->activate_timeout)
    gtk_button_finish_activate (button, TRUE);
}

static void
gtk_button_set_child_type (GtkButton *button, guint child_type)
{
  GtkButtonPrivate *priv = gtk_button_get_instance_private (button);

  if (priv->child_type == child_type)
    return;

  if (child_type == LABEL_CHILD)
    gtk_widget_add_css_class (GTK_WIDGET (button), "text-button");
  else
    gtk_widget_remove_css_class (GTK_WIDGET (button), "text-button");

  if (child_type == ICON_CHILD)
    gtk_widget_add_css_class (GTK_WIDGET (button), "image-button");
  else
    gtk_widget_remove_css_class (GTK_WIDGET (button), "image-button");

  if (child_type != LABEL_CHILD)
    g_object_notify_by_pspec (G_OBJECT (button), props[PROP_LABEL]);
  else if (child_type != ICON_CHILD)
    g_object_notify_by_pspec (G_OBJECT (button), props[PROP_ICON_NAME]);

  priv->child_type = child_type;
}

static void
gtk_button_init (GtkButton *button)
{
  GtkButtonPrivate *priv = gtk_button_get_instance_private (button);
  GtkEventController *key_controller;

  gtk_widget_set_focusable (GTK_WIDGET (button), TRUE);
  gtk_widget_set_receives_default (GTK_WIDGET (button), TRUE);

  priv->button_down = FALSE;
  priv->use_underline = FALSE;
  priv->child_type = WIDGET_CHILD;

  priv->gesture = gtk_gesture_click_new ();
  gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (priv->gesture), FALSE);
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (priv->gesture), GDK_BUTTON_PRIMARY);
  g_signal_connect (priv->gesture, "pressed", G_CALLBACK (click_pressed_cb), button);
  g_signal_connect (priv->gesture, "released", G_CALLBACK (click_released_cb), button);
  g_signal_connect (priv->gesture, "cancel", G_CALLBACK (click_gesture_cancel_cb), button);
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (priv->gesture), GTK_PHASE_CAPTURE);
  gtk_widget_add_controller (GTK_WIDGET (button), GTK_EVENT_CONTROLLER (priv->gesture));

  key_controller = gtk_event_controller_key_new ();
  g_signal_connect (key_controller, "key-pressed", G_CALLBACK (key_controller_key_pressed_cb), button);
  g_signal_connect (key_controller, "key-released", G_CALLBACK (key_controller_key_released_cb), button);
  gtk_widget_add_controller (GTK_WIDGET (button), key_controller);
}

static void
gtk_button_dispose (GObject *object)
{
  GtkButton *button = GTK_BUTTON (object);
  GtkButtonPrivate *priv = gtk_button_get_instance_private (button);

  g_clear_pointer (&priv->child, gtk_widget_unparent);
  g_clear_object (&priv->action_helper);

  G_OBJECT_CLASS (gtk_button_parent_class)->dispose (object);
}

static void
gtk_button_set_action_name (GtkActionable *actionable,
                            const char    *action_name)
{
  GtkButton *button = GTK_BUTTON (actionable);
  GtkButtonPrivate *priv = gtk_button_get_instance_private (button);

  if (!priv->action_helper)
    priv->action_helper = gtk_action_helper_new (actionable);

  g_signal_handlers_disconnect_by_func (button, gtk_real_button_clicked, NULL);
  if (action_name)
    g_signal_connect_after (button, "clicked", G_CALLBACK (gtk_real_button_clicked), NULL);

  gtk_action_helper_set_action_name (priv->action_helper, action_name);
}

static void
gtk_button_set_action_target_value (GtkActionable *actionable,
                                    GVariant      *action_target)
{
  GtkButton *button = GTK_BUTTON (actionable);
  GtkButtonPrivate *priv = gtk_button_get_instance_private (button);

  if (!priv->action_helper)
    priv->action_helper = gtk_action_helper_new (actionable);

  gtk_action_helper_set_action_target_value (priv->action_helper, action_target);
}

static void
gtk_button_set_property (GObject         *object,
                         guint            prop_id,
                         const GValue    *value,
                         GParamSpec      *pspec)
{
  GtkButton *button = GTK_BUTTON (object);

  switch (prop_id)
    {
    case PROP_LABEL:
      gtk_button_set_label (button, g_value_get_string (value));
      break;
    case PROP_HAS_FRAME:
      gtk_button_set_has_frame (button, g_value_get_boolean (value));
      break;
    case PROP_USE_UNDERLINE:
      gtk_button_set_use_underline (button, g_value_get_boolean (value));
      break;
    case PROP_ICON_NAME:
      gtk_button_set_icon_name (button, g_value_get_string (value));
      break;
    case PROP_CHILD:
      gtk_button_set_child (button, g_value_get_object (value));
      break;
    case PROP_CAN_SHRINK:
      gtk_button_set_can_shrink (button, g_value_get_boolean (value));
      break;
    case PROP_ACTION_NAME:
      gtk_button_set_action_name (GTK_ACTIONABLE (button), g_value_get_string (value));
      break;
    case PROP_ACTION_TARGET:
      gtk_button_set_action_target_value (GTK_ACTIONABLE (button), g_value_get_variant (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_button_get_property (GObject         *object,
                         guint            prop_id,
                         GValue          *value,
                         GParamSpec      *pspec)
{
  GtkButton *button = GTK_BUTTON (object);
  GtkButtonPrivate *priv = gtk_button_get_instance_private (button);

  switch (prop_id)
    {
    case PROP_LABEL:
      g_value_set_string (value, gtk_button_get_label (button));
      break;
    case PROP_HAS_FRAME:
      g_value_set_boolean (value, gtk_button_get_has_frame (button));
      break;
    case PROP_USE_UNDERLINE:
      g_value_set_boolean (value, priv->use_underline);
      break;
    case PROP_ICON_NAME:
      g_value_set_string (value, gtk_button_get_icon_name (button));
      break;
    case PROP_CHILD:
      g_value_set_object (value, priv->child);
      break;
    case PROP_CAN_SHRINK:
      g_value_set_boolean (value, priv->can_shrink);
      break;
    case PROP_ACTION_NAME:
      g_value_set_string (value, gtk_action_helper_get_action_name (priv->action_helper));
      break;
    case PROP_ACTION_TARGET:
      g_value_set_variant (value, gtk_action_helper_get_action_target_value (priv->action_helper));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static const char *
gtk_button_get_action_name (GtkActionable *actionable)
{
  GtkButton *button = GTK_BUTTON (actionable);
  GtkButtonPrivate *priv = gtk_button_get_instance_private (button);

  return gtk_action_helper_get_action_name (priv->action_helper);
}

static GVariant *
gtk_button_get_action_target_value (GtkActionable *actionable)
{
  GtkButton *button = GTK_BUTTON (actionable);
  GtkButtonPrivate *priv = gtk_button_get_instance_private (button);

  return gtk_action_helper_get_action_target_value (priv->action_helper);
}

static void
gtk_button_actionable_iface_init (GtkActionableInterface *iface)
{
  iface->get_action_name = gtk_button_get_action_name;
  iface->set_action_name = gtk_button_set_action_name;
  iface->get_action_target_value = gtk_button_get_action_target_value;
  iface->set_action_target_value = gtk_button_set_action_target_value;
}

static GtkBuildableIface *parent_buildable_iface;

static void
gtk_button_buildable_add_child (GtkBuildable *buildable,
                                GtkBuilder   *builder,
                                GObject      *child,
                                const char   *type)
{
  if (GTK_IS_WIDGET (child))
    gtk_button_set_child (GTK_BUTTON (buildable), GTK_WIDGET (child));
  else
    parent_buildable_iface->add_child (buildable, builder, child, type);
}

static void
gtk_button_buildable_iface_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);

  iface->add_child = gtk_button_buildable_add_child;
}

/**
 * gtk_button_new:
 *
 * Creates a new `GtkButton` widget.
 *
 * To add a child widget to the button, use [method@Gtk.Button.set_child].
 *
 * Returns: The newly created `GtkButton` widget.
 */
GtkWidget*
gtk_button_new (void)
{
  return g_object_new (GTK_TYPE_BUTTON, NULL);
}

/**
 * gtk_button_new_with_label:
 * @label: The text you want the `GtkLabel` to hold
 *
 * Creates a `GtkButton` widget with a `GtkLabel` child.
 *
 * Returns: The newly created `GtkButton` widget
 */
GtkWidget*
gtk_button_new_with_label (const char *label)
{
  return g_object_new (GTK_TYPE_BUTTON, "label", label, NULL);
}

/**
 * gtk_button_new_from_icon_name:
 * @icon_name: an icon name
 *
 * Creates a new button containing an icon from the current icon theme.
 *
 * If the icon name isn’t known, a “broken image” icon will be
 * displayed instead. If the current icon theme is changed, the icon
 * will be updated appropriately.
 *
 * Returns: a new `GtkButton` displaying the themed icon
 */
GtkWidget*
gtk_button_new_from_icon_name (const char *icon_name)
{
  GtkWidget *button;

  button = g_object_new (GTK_TYPE_BUTTON,
                         "icon-name", icon_name,
                         NULL);

  return button;
}

/**
 * gtk_button_new_with_mnemonic:
 * @label: The text of the button, with an underscore in front of the
 *   mnemonic character
 *
 * Creates a new `GtkButton` containing a label.
 *
 * If characters in @label are preceded by an underscore, they are underlined.
 * If you need a literal underscore character in a label, use “__” (two
 * underscores). The first underlined character represents a keyboard
 * accelerator called a mnemonic. Pressing <kbd>Alt</kbd> and that key
 * activates the button.
 *
 * Returns: a new `GtkButton`
 */
GtkWidget*
gtk_button_new_with_mnemonic (const char *label)
{
  return g_object_new (GTK_TYPE_BUTTON, "label", label, "use-underline", TRUE,  NULL);
}

/**
 * gtk_button_set_has_frame: (attributes org.gtk.Method.set_property=has-frame)
 * @button: a `GtkButton`
 * @has_frame: whether the button should have a visible frame
 *
 * Sets the style of the button.
 *
 * Buttons can have a flat appearance or have a frame drawn around them.
 */
void
gtk_button_set_has_frame (GtkButton *button,
                          gboolean   has_frame)
{

  g_return_if_fail (GTK_IS_BUTTON (button));

  if (gtk_button_get_has_frame (button) == has_frame)
    return;

  if (has_frame)
    gtk_widget_remove_css_class (GTK_WIDGET (button), "flat");
  else
    gtk_widget_add_css_class (GTK_WIDGET (button), "flat");

  g_object_notify_by_pspec (G_OBJECT (button), props[PROP_HAS_FRAME]);
}

/**
 * gtk_button_get_has_frame: (attributes org.gtk.Method.get_property=has-frame)
 * @button: a `GtkButton`
 *
 * Returns whether the button has a frame.
 *
 * Returns: %TRUE if the button has a frame
 */
gboolean
gtk_button_get_has_frame (GtkButton *button)
{
  g_return_val_if_fail (GTK_IS_BUTTON (button), TRUE);

  return !gtk_widget_has_css_class (GTK_WIDGET (button), "flat");
}

static void
gtk_button_unrealize (GtkWidget *widget)
{
  GtkButton *button = GTK_BUTTON (widget);
  GtkButtonPrivate *priv = gtk_button_get_instance_private (button);

  if (priv->activate_timeout)
    gtk_button_finish_activate (button, FALSE);

  GTK_WIDGET_CLASS (gtk_button_parent_class)->unrealize (widget);
}

static void
gtk_button_do_release (GtkButton *button,
                       gboolean   emit_clicked)
{
  GtkButtonPrivate *priv = gtk_button_get_instance_private (button);

  if (priv->button_down)
    {
      priv->button_down = FALSE;

      if (priv->activate_timeout)
        return;

      if (emit_clicked)
        g_signal_emit (button, button_signals[CLICKED], 0);
    }
}

static void
gtk_real_button_clicked (GtkButton *button)
{
  GtkButtonPrivate *priv = gtk_button_get_instance_private (button);

  if (priv->action_helper)
    gtk_action_helper_activate (priv->action_helper);
}

static gboolean
button_activate_timeout (gpointer data)
{
  gtk_button_finish_activate (data, TRUE);

  return FALSE;
}

static void
gtk_real_button_activate (GtkButton *button)
{
  GtkWidget *widget = GTK_WIDGET (button);
  GtkButtonPrivate *priv = gtk_button_get_instance_private (button);

  if (gtk_widget_get_realized (widget) && !priv->activate_timeout)
    {
      priv->activate_timeout = g_timeout_add (ACTIVATE_TIMEOUT, button_activate_timeout, button);
      gdk_source_set_static_name_by_id (priv->activate_timeout, "[gtk] button_activate_timeout");

      gtk_widget_add_css_class (GTK_WIDGET (button), "keyboard-activating");
      priv->button_down = TRUE;
    }
}

static void
gtk_button_finish_activate (GtkButton *button,
                            gboolean   do_it)
{
  GtkButtonPrivate *priv = gtk_button_get_instance_private (button);

  gtk_widget_remove_css_class (GTK_WIDGET (button), "keyboard-activating");

  g_source_remove (priv->activate_timeout);
  priv->activate_timeout = 0;

  priv->button_down = FALSE;

  if (do_it)
    g_signal_emit (button, button_signals[CLICKED], 0);
}

/**
 * gtk_button_set_label: (attributes org.gtk.Method.set_property=label)
 * @button: a `GtkButton`
 * @label: a string
 *
 * Sets the text of the label of the button to @label.
 *
 * This will also clear any previously set labels.
 */
void
gtk_button_set_label (GtkButton  *button,
                      const char *label)
{
  GtkButtonPrivate *priv = gtk_button_get_instance_private (button);
  GtkWidget *child;

  g_return_if_fail (GTK_IS_BUTTON (button));

  if (priv->child_type != LABEL_CHILD || priv->child == NULL)
    {
      child = gtk_label_new (NULL);
      gtk_button_set_child (button,  child);
      if (priv->use_underline)
        {
          gtk_label_set_use_underline (GTK_LABEL (child), priv->use_underline);
          gtk_label_set_mnemonic_widget (GTK_LABEL (child), GTK_WIDGET (button));
        }
      else
        {
          gtk_accessible_update_relation (GTK_ACCESSIBLE (button),
                                          GTK_ACCESSIBLE_RELATION_LABELLED_BY, child, NULL,
                                          -1);
        }
    }

  gtk_label_set_label (GTK_LABEL (priv->child), label);
  gtk_label_set_ellipsize (GTK_LABEL (priv->child),
                           priv->can_shrink ? PANGO_ELLIPSIZE_END
                                            : PANGO_ELLIPSIZE_NONE);

  gtk_button_set_child_type (button, LABEL_CHILD);

  g_object_notify_by_pspec (G_OBJECT (button), props[PROP_LABEL]);
}

/**
 * gtk_button_get_label: (attributes org.gtk.Method.get_property=label)
 * @button: a `GtkButton`
 *
 * Fetches the text from the label of the button.
 *
 * If the label text has not been set with [method@Gtk.Button.set_label]
 * the return value will be %NULL. This will be the case if you create
 * an empty button with [ctor@Gtk.Button.new] to use as a container.
 *
 * Returns: (nullable): The text of the label widget. This string is owned
 * by the widget and must not be modified or freed.
 */
const char *
gtk_button_get_label (GtkButton *button)
{
  GtkButtonPrivate *priv = gtk_button_get_instance_private (button);

  g_return_val_if_fail (GTK_IS_BUTTON (button), NULL);

  if (priv->child_type == LABEL_CHILD)
    return gtk_label_get_label (GTK_LABEL (priv->child));

  return NULL;
}

/**
 * gtk_button_set_use_underline: (attributes org.gtk.Method.set_property=use-underline)
 * @button: a `GtkButton`
 * @use_underline: %TRUE if underlines in the text indicate mnemonics
 *
 * Sets whether to use underlines as mnemonics.
 *
 * If true, an underline in the text of the button label indicates
 * the next character should be used for the mnemonic accelerator key.
 */
void
gtk_button_set_use_underline (GtkButton *button,
                              gboolean   use_underline)
{
  GtkButtonPrivate *priv = gtk_button_get_instance_private (button);

  g_return_if_fail (GTK_IS_BUTTON (button));

  use_underline = use_underline != FALSE;

  if (use_underline != priv->use_underline)
    {
      if (priv->child_type == LABEL_CHILD)
        {
          gtk_label_set_use_underline (GTK_LABEL (priv->child), use_underline);
          gtk_label_set_mnemonic_widget (GTK_LABEL (priv->child), GTK_WIDGET (button));
        }

      priv->use_underline = use_underline;
      g_object_notify_by_pspec (G_OBJECT (button), props[PROP_USE_UNDERLINE]);
    }
}

/**
 * gtk_button_get_use_underline: (attributes org.gtk.Method.get_property=use-underline)
 * @button: a `GtkButton`
 *
 * gets whether underlines are interpreted as mnemonics.
 *
 * See [method@Gtk.Button.set_use_underline].
 *
 * Returns: %TRUE if an embedded underline in the button label
 *   indicates the mnemonic accelerator keys.
 */
gboolean
gtk_button_get_use_underline (GtkButton *button)
{
  GtkButtonPrivate *priv = gtk_button_get_instance_private (button);

  g_return_val_if_fail (GTK_IS_BUTTON (button), FALSE);

  return priv->use_underline;
}

static void
gtk_button_state_flags_changed (GtkWidget     *widget,
                                GtkStateFlags  previous_state)
{
  GtkButton *button = GTK_BUTTON (widget);

  if (!gtk_widget_is_sensitive (widget))
    gtk_button_do_release (button, FALSE);
}

/**
 * gtk_button_set_icon_name: (attributes org.gtk.Method.set_property=icon-name)
 * @button: A `GtkButton`
 * @icon_name: An icon name
 *
 * Adds a `GtkImage` with the given icon name as a child.
 *
 * If @button already contains a child widget, that child widget will
 * be removed and replaced with the image.
 */
void
gtk_button_set_icon_name (GtkButton  *button,
                          const char *icon_name)
{
  GtkButtonPrivate *priv = gtk_button_get_instance_private (button);

  g_return_if_fail (GTK_IS_BUTTON (button));
  g_return_if_fail (icon_name != NULL);

  if (priv->child_type != ICON_CHILD || priv->child == NULL)
    {
      GtkWidget *child = g_object_new (GTK_TYPE_IMAGE,
                                       "accessible-role", GTK_ACCESSIBLE_ROLE_PRESENTATION,
                                       "icon-name", icon_name,
                                       NULL);
      gtk_button_set_child (GTK_BUTTON (button), child);
      gtk_widget_set_valign (child, GTK_ALIGN_CENTER);
    }
  else
    {
      gtk_image_set_from_icon_name (GTK_IMAGE (priv->child), icon_name);
    }

  gtk_button_set_child_type (button, ICON_CHILD);
  g_object_notify_by_pspec (G_OBJECT (button), props[PROP_ICON_NAME]);
}

/**
 * gtk_button_get_icon_name: (attributes org.gtk.Method.get_property=icon-name)
 * @button: A `GtkButton`
 *
 * Returns the icon name of the button.
 *
 * If the icon name has not been set with [method@Gtk.Button.set_icon_name]
 * the return value will be %NULL. This will be the case if you create
 * an empty button with [ctor@Gtk.Button.new] to use as a container.
 *
 * Returns: (nullable): The icon name set via [method@Gtk.Button.set_icon_name]
 */
const char *
gtk_button_get_icon_name (GtkButton *button)
{
  GtkButtonPrivate *priv = gtk_button_get_instance_private (button);

  g_return_val_if_fail (GTK_IS_BUTTON (button), NULL);

  if (priv->child_type == ICON_CHILD)
    return gtk_image_get_icon_name (GTK_IMAGE (priv->child));

  return NULL;
}

GtkGesture *
gtk_button_get_gesture (GtkButton *button)
{
  GtkButtonPrivate *priv = gtk_button_get_instance_private (button);

  return priv->gesture;
}

GtkActionHelper *
gtk_button_get_action_helper (GtkButton *button)
{
  GtkButtonPrivate *priv = gtk_button_get_instance_private (button);

  return priv->action_helper;
}

/**
 * gtk_button_set_child: (attributes org.gtk.Method.set_property=child)
 * @button: a `GtkButton`
 * @child: (nullable): the child widget
 *
 * Sets the child widget of @button.
 *
 * Note that by using this API, you take full responsibility for setting
 * up the proper accessibility label and description information for @button.
 * Most likely, you'll either set the accessibility label or description
 * for @button explicitly, or you'll set a labelled-by or described-by
 * relations from @child to @button.
 */
void
gtk_button_set_child (GtkButton *button,
                      GtkWidget *child)
{
  GtkButtonPrivate *priv = gtk_button_get_instance_private (button);

  g_return_if_fail (GTK_IS_BUTTON (button));
  g_return_if_fail (child == NULL || priv->child == child || gtk_widget_get_parent (child) == NULL);

  if (priv->child == child)
    return;

  g_clear_pointer (&priv->child, gtk_widget_unparent);

  priv->child = child;

  if (priv->child)
    gtk_widget_set_parent (priv->child, GTK_WIDGET (button));

  gtk_button_set_child_type (button, WIDGET_CHILD);
  g_object_notify_by_pspec (G_OBJECT (button), props[PROP_CHILD]);
}

/**
 * gtk_button_get_child: (attributes org.gtk.Method.get_property=child)
 * @button: a `GtkButton`
 *
 * Gets the child widget of @button.
 *
 * Returns: (nullable) (transfer none): the child widget of @button
 */
GtkWidget *
gtk_button_get_child (GtkButton *button)
{
  GtkButtonPrivate *priv = gtk_button_get_instance_private (button);

  g_return_val_if_fail (GTK_IS_BUTTON (button), NULL);

  return priv->child;
}

/**
 * gtk_button_set_can_shrink:
 * @button: a button
 * @can_shrink: whether the button can shrink
 *
 * Sets whether the button size can be smaller than the natural size of
 * its contents.
 *
 * For text buttons, setting @can_shrink to true will ellipsize the label.
 *
 * For icons and custom children, this function has no effect.
 *
 * Since: 4.12
 */
void
gtk_button_set_can_shrink (GtkButton *button,
                           gboolean   can_shrink)
{
  GtkButtonPrivate *priv = gtk_button_get_instance_private (button);

  g_return_if_fail (GTK_IS_BUTTON (button));

  can_shrink = !!can_shrink;

  if (priv->can_shrink != can_shrink)
    {
      priv->can_shrink = can_shrink;

      switch (priv->child_type)
        {
        case LABEL_CHILD:
          gtk_label_set_ellipsize (GTK_LABEL (priv->child),
                                   priv->can_shrink ? PANGO_ELLIPSIZE_END
                                                    : PANGO_ELLIPSIZE_NONE);
          break;

        case ICON_CHILD:
        case WIDGET_CHILD:
          break;

        default:
          g_assert_not_reached ();
          break;
        }

      g_object_notify_by_pspec (G_OBJECT (button), props[PROP_CAN_SHRINK]);
    }
}

/**
 * gtk_button_get_can_shrink:
 * @button: a button
 *
 * Retrieves whether the button can be smaller than the natural
 * size of its contents.
 *
 * Returns: true if the button can shrink, and false otherwise
 *
 * Since: 4.12
 */
gboolean
gtk_button_get_can_shrink (GtkButton *button)
{
  GtkButtonPrivate *priv = gtk_button_get_instance_private (button);

  g_return_val_if_fail (GTK_IS_BUTTON (button), FALSE);

  return priv->can_shrink;
}
