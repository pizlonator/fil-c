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
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include "gtktogglebutton.h"

#include "gtkaccessible.h"
#include "gtkbuttonprivate.h"
#include "gtkenums.h"
#include "gtklabel.h"
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtkprivate.h"

/**
 * GtkToggleButton:
 *
 * A `GtkToggleButton` is a button which remains “pressed-in” when
 * clicked.
 *
 * Clicking again will cause the toggle button to return to its normal state.
 *
 * A toggle button is created by calling either [ctor@Gtk.ToggleButton.new] or
 * [ctor@Gtk.ToggleButton.new_with_label]. If using the former, it is advisable
 * to pack a widget, (such as a `GtkLabel` and/or a `GtkImage`), into the toggle
 * button’s container. (See [class@Gtk.Button] for more information).
 *
 * The state of a `GtkToggleButton` can be set specifically using
 * [method@Gtk.ToggleButton.set_active], and retrieved using
 * [method@Gtk.ToggleButton.get_active].
 *
 * To simply switch the state of a toggle button, use
 * [method@Gtk.ToggleButton.toggled].
 *
 * ## Grouping
 *
 * Toggle buttons can be grouped together, to form mutually exclusive
 * groups - only one of the buttons can be toggled at a time, and toggling
 * another one will switch the currently toggled one off.
 *
 * To add a `GtkToggleButton` to a group, use [method@Gtk.ToggleButton.set_group].
 *
 * ## CSS nodes
 *
 * `GtkToggleButton` has a single CSS node with name button. To differentiate
 * it from a plain `GtkButton`, it gets the `.toggle` style class.
 *
 * ## Accessibility
 *
 * `GtkToggleButton` uses the %GTK_ACCESSIBLE_ROLE_TOGGLE_BUTTON role.
 *
 * ## Creating two `GtkToggleButton` widgets.
 *
 * ```c
 * static void
 * output_state (GtkToggleButton *source,
 *               gpointer         user_data)
 * {
 *   g_print ("Toggle button "%s" is active: %s",
 *            gtk_button_get_label (GTK_BUTTON (source)),
 *            gtk_toggle_button_get_active (source) ? "Yes" : "No");
 * }
 *
 * static void
 * make_toggles (void)
 * {
 *   GtkWidget *window, *toggle1, *toggle2;
 *   GtkWidget *box;
 *   const char *text;
 *
 *   window = gtk_window_new ();
 *   box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
 *
 *   text = "Hi, I’m toggle button one";
 *   toggle1 = gtk_toggle_button_new_with_label (text);
 *
 *   g_signal_connect (toggle1, "toggled",
 *                     G_CALLBACK (output_state),
 *                     NULL);
 *   gtk_box_append (GTK_BOX (box), toggle1);
 *
 *   text = "Hi, I’m toggle button two";
 *   toggle2 = gtk_toggle_button_new_with_label (text);
 *   g_signal_connect (toggle2, "toggled",
 *                     G_CALLBACK (output_state),
 *                     NULL);
 *   gtk_box_append (GTK_BOX (box), toggle2);
 *
 *   gtk_window_set_child (GTK_WINDOW (window), box);
 *   gtk_window_present (GTK_WINDOW (window));
 * }
 * ```
 */

typedef struct _GtkToggleButtonPrivate       GtkToggleButtonPrivate;
struct _GtkToggleButtonPrivate
{
  GtkToggleButton *group_next;
  GtkToggleButton *group_prev;

  guint active         : 1;
};

enum {
  TOGGLED,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_ACTIVE,
  PROP_GROUP,
  NUM_PROPERTIES
};

static guint toggle_button_signals[LAST_SIGNAL] = { 0 };
static GParamSpec *toggle_button_props[NUM_PROPERTIES] = { NULL, };

G_DEFINE_TYPE_WITH_CODE (GtkToggleButton, gtk_toggle_button, GTK_TYPE_BUTTON,
                         G_ADD_PRIVATE (GtkToggleButton))

static void
gtk_toggle_button_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GtkToggleButton *tb = GTK_TOGGLE_BUTTON (object);

  switch (prop_id)
    {
    case PROP_ACTIVE:
      gtk_toggle_button_set_active (tb, g_value_get_boolean (value));
      break;
    case PROP_GROUP:
      gtk_toggle_button_set_group (GTK_TOGGLE_BUTTON (object), g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_toggle_button_get_property (GObject      *object,
                                guint         prop_id,
                                GValue       *value,
                                GParamSpec   *pspec)
{
  GtkToggleButton *tb = GTK_TOGGLE_BUTTON (object);
  GtkToggleButtonPrivate *priv = gtk_toggle_button_get_instance_private (tb);

  switch (prop_id)
    {
    case PROP_ACTIVE:
      g_value_set_boolean (value, priv->active);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static gboolean
gtk_toggle_button_mnemonic_activate (GtkWidget *widget,
                                     gboolean   group_cycling)
{
  /*
   * We override the standard implementation in
   * gtk_widget_real_mnemonic_activate() in order to focus the widget even
   * if there is no mnemonic conflict.
   */
  if (gtk_widget_get_focusable (widget))
    gtk_widget_grab_focus (widget);

  if (!group_cycling)
    gtk_widget_activate (widget);

  return TRUE;
}

static void
gtk_toggle_button_clicked (GtkButton *button)
{
  GtkToggleButton *toggle_button = GTK_TOGGLE_BUTTON (button);
  GtkToggleButtonPrivate *priv = gtk_toggle_button_get_instance_private (toggle_button);

  if (priv->active && (priv->group_prev || priv->group_next))
    return;

  if (gtk_button_get_action_helper (button))
    return;

  gtk_toggle_button_set_active (toggle_button, !priv->active);
}

static void
gtk_toggle_button_dispose (GObject *object)
{
  GtkToggleButton *toggle_button = GTK_TOGGLE_BUTTON (object);

  gtk_toggle_button_set_group (toggle_button, NULL);

  G_OBJECT_CLASS (gtk_toggle_button_parent_class)->dispose (object);
}

static GtkToggleButton *
get_group_next (GtkToggleButton *self)
{
  return ((GtkToggleButtonPrivate *)gtk_toggle_button_get_instance_private (self))->group_next;
}

static GtkToggleButton *
get_group_prev (GtkToggleButton *self)
{
  return ((GtkToggleButtonPrivate *)gtk_toggle_button_get_instance_private (self))->group_prev;
}

static GtkToggleButton *
get_group_first (GtkToggleButton *self)
{
  GtkToggleButton *group_first = NULL;
  GtkToggleButton *iter;

  /* Find first in group */
  iter = self;
  while (iter)
    {
      group_first = iter;

      iter = get_group_prev (iter);
      if (!iter)
        break;
    }

  g_assert (group_first);

  return group_first;
}

static void
gtk_toggle_button_realize (GtkWidget *widget)
{
  GtkToggleButton *self = GTK_TOGGLE_BUTTON (widget);
  GtkToggleButtonPrivate *priv = gtk_toggle_button_get_instance_private (self);

  GTK_WIDGET_CLASS (gtk_toggle_button_parent_class)->realize (widget);

  gtk_accessible_update_state (GTK_ACCESSIBLE (widget),
                               GTK_ACCESSIBLE_STATE_PRESSED, priv->active,
                               -1);
}

static void
gtk_toggle_button_class_init (GtkToggleButtonClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  GtkButtonClass *button_class = GTK_BUTTON_CLASS (class);

  gobject_class->dispose = gtk_toggle_button_dispose;
  gobject_class->set_property = gtk_toggle_button_set_property;
  gobject_class->get_property = gtk_toggle_button_get_property;

  widget_class->mnemonic_activate = gtk_toggle_button_mnemonic_activate;
  widget_class->realize = gtk_toggle_button_realize;

  button_class->clicked = gtk_toggle_button_clicked;

  class->toggled = NULL;

  /**
   * GtkToggleButton:active: (attributes org.gtk.Property.get=gtk_toggle_button_get_active org.gtk.Property.set=gtk_toggle_button_set_active)
   *
   * If the toggle button should be pressed in.
   */
  toggle_button_props[PROP_ACTIVE] =
      g_param_spec_boolean ("active", NULL, NULL,
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkToggleButton:group: (attributes org.gtk.Property.set=gtk_toggle_button_set_group)
   *
   * The toggle button whose group this widget belongs to.
   */
  toggle_button_props[PROP_GROUP] =
      g_param_spec_object ("group", NULL, NULL,
                           GTK_TYPE_TOGGLE_BUTTON,
                           GTK_PARAM_WRITABLE);

  g_object_class_install_properties (gobject_class, NUM_PROPERTIES, toggle_button_props);

  /**
   * GtkToggleButton::toggled:
   * @togglebutton: the object which received the signal.
   *
   * Emitted whenever the `GtkToggleButton`'s state is changed.
   */
  toggle_button_signals[TOGGLED] =
    g_signal_new (I_("toggled"),
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkToggleButtonClass, toggled),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  gtk_widget_class_set_css_name (widget_class, I_("button"));

  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_TOGGLE_BUTTON);
}

static void
gtk_toggle_button_init (GtkToggleButton *toggle_button)
{
  GtkToggleButtonPrivate *priv = gtk_toggle_button_get_instance_private (toggle_button);

  priv->active = FALSE;

  gtk_widget_add_css_class (GTK_WIDGET (toggle_button), "toggle");
}


/**
 * gtk_toggle_button_new:
 *
 * Creates a new toggle button.
 *
 * A widget should be packed into the button, as in [ctor@Gtk.Button.new].
 *
 * Returns: a new toggle button.
 */
GtkWidget *
gtk_toggle_button_new (void)
{
  return g_object_new (GTK_TYPE_TOGGLE_BUTTON, NULL);
}

/**
 * gtk_toggle_button_new_with_label:
 * @label: a string containing the message to be placed in the toggle button.
 *
 * Creates a new toggle button with a text label.
 *
 * Returns: a new toggle button.
 */
GtkWidget *
gtk_toggle_button_new_with_label (const char *label)
{
  return g_object_new (GTK_TYPE_TOGGLE_BUTTON, "label", label, NULL);
}

/**
 * gtk_toggle_button_new_with_mnemonic:
 * @label: the text of the button, with an underscore in front of the
 *   mnemonic character
 *
 * Creates a new `GtkToggleButton` containing a label.
 *
 * The label will be created using [ctor@Gtk.Label.new_with_mnemonic],
 * so underscores in @label indicate the mnemonic for the button.
 *
 * Returns: a new `GtkToggleButton`
 */
GtkWidget *
gtk_toggle_button_new_with_mnemonic (const char *label)
{
  return g_object_new (GTK_TYPE_TOGGLE_BUTTON,
                       "label", label,
                       "use-underline", TRUE,
                       NULL);
}

/**
 * gtk_toggle_button_set_active: (attributes org.gtk.Method.set_property=active)
 * @toggle_button: a `GtkToggleButton`.
 * @is_active: %TRUE or %FALSE.
 *
 * Sets the status of the toggle button.
 *
 * Set to %TRUE if you want the `GtkToggleButton` to be “pressed in”,
 * and %FALSE to raise it.
 *
 * If the status of the button changes, this action causes the
 * [signal@Gtk.ToggleButton::toggled] signal to be emitted.
 */
void
gtk_toggle_button_set_active (GtkToggleButton *toggle_button,
                              gboolean         is_active)
{
  GtkToggleButtonPrivate *priv = gtk_toggle_button_get_instance_private (toggle_button);

  g_return_if_fail (GTK_IS_TOGGLE_BUTTON (toggle_button));

  is_active = is_active != FALSE;

  if (priv->active == is_active)
    return;

  if (is_active && (priv->group_prev || priv->group_next))
    {
      GtkToggleButton *group_first = NULL;
      GtkToggleButton *iter;

      group_first = get_group_first (toggle_button);
      g_assert (group_first);

      /* Set all buttons in group to !active */
      for (iter = group_first; iter; iter = get_group_next (iter))
        gtk_toggle_button_set_active (iter, FALSE);

      /* ... and the next code block will set this one to active */
    }

  priv->active = is_active;

  if (is_active)
    gtk_widget_set_state_flags (GTK_WIDGET (toggle_button), GTK_STATE_FLAG_CHECKED, FALSE);
  else
    gtk_widget_unset_state_flags (GTK_WIDGET (toggle_button), GTK_STATE_FLAG_CHECKED);

  gtk_accessible_update_state (GTK_ACCESSIBLE (toggle_button),
                               GTK_ACCESSIBLE_STATE_PRESSED, is_active,
                               -1);

  g_signal_emit (toggle_button, toggle_button_signals[TOGGLED], 0);

  g_object_notify_by_pspec (G_OBJECT (toggle_button), toggle_button_props[PROP_ACTIVE]);
}

/**
 * gtk_toggle_button_get_active: (attributes org.gtk.Method.get_property=active)
 * @toggle_button: a `GtkToggleButton`.
 *
 * Queries a `GtkToggleButton` and returns its current state.
 *
 * Returns %TRUE if the toggle button is pressed in and %FALSE
 * if it is raised.
 *
 * Returns: whether the button is pressed
 */
gboolean
gtk_toggle_button_get_active (GtkToggleButton *toggle_button)
{
  GtkToggleButtonPrivate *priv = gtk_toggle_button_get_instance_private (toggle_button);

  g_return_val_if_fail (GTK_IS_TOGGLE_BUTTON (toggle_button), FALSE);

  return priv->active;
}

/**
 * gtk_toggle_button_toggled:
 * @toggle_button: a `GtkToggleButton`.
 *
 * Emits the ::toggled signal on the `GtkToggleButton`.
 *
 * Deprecated: 4.10: There is no good reason for an application ever to call this function.
 */
void
gtk_toggle_button_toggled (GtkToggleButton *toggle_button)
{
  g_return_if_fail (GTK_IS_TOGGLE_BUTTON (toggle_button));

  g_signal_emit (toggle_button, toggle_button_signals[TOGGLED], 0);
}

/**
 * gtk_toggle_button_set_group: (attributes org.gtk.Method.set_property=group)
 * @toggle_button: a `GtkToggleButton`
 * @group: (nullable) (transfer none): another `GtkToggleButton` to
 *   form a group with
 *
 * Adds @self to the group of @group.
 *
 * In a group of multiple toggle buttons, only one button can be active
 * at a time.
 *
 * Setting up groups in a cycle leads to undefined behavior.
 *
 * Note that the same effect can be achieved via the [iface@Gtk.Actionable]
 * API, by using the same action with parameter type and state type 's'
 * for all buttons in the group, and giving each button its own target
 * value.
 */
void
gtk_toggle_button_set_group (GtkToggleButton *toggle_button,
                             GtkToggleButton *group)
{
  GtkToggleButtonPrivate *priv = gtk_toggle_button_get_instance_private (toggle_button);
  GtkToggleButtonPrivate *group_priv;

  g_return_if_fail (GTK_IS_TOGGLE_BUTTON (toggle_button));
  g_return_if_fail (toggle_button != group);

  if (!group)
    {
      if (priv->group_prev)
        {
          GtkToggleButtonPrivate *p = gtk_toggle_button_get_instance_private (priv->group_prev);
          p->group_next = priv->group_next;
        }
      if (priv->group_next)
        {
          GtkToggleButtonPrivate *p = gtk_toggle_button_get_instance_private (priv->group_next);
          p->group_prev = priv->group_prev;
        }

      priv->group_next = NULL;
      priv->group_prev = NULL;
      g_object_notify_by_pspec (G_OBJECT (toggle_button), toggle_button_props[PROP_GROUP]);
      return;
    }

  if (priv->group_next == group)
    return;

  group_priv = gtk_toggle_button_get_instance_private (group);

  priv->group_prev = NULL;
  if (group_priv->group_prev)
    {
      GtkToggleButtonPrivate *prev = gtk_toggle_button_get_instance_private (group_priv->group_prev);

      prev->group_next = toggle_button;
      priv->group_prev = group_priv->group_prev;
    }

  group_priv->group_prev = toggle_button;
  priv->group_next = group;

  g_object_notify_by_pspec (G_OBJECT (toggle_button), toggle_button_props[PROP_GROUP]);
}
