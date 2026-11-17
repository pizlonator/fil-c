/*
 * gsd-wacom-key-shortcut-button.c
 *
 * Copyright © 2013 Red Hat, Inc.
 *
 * Author: Joaquim Rocha <jrocha@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include <glib/gi18n-lib.h>

#include "gsd-wacom-key-shortcut-button.h"

/**
 * SECTION:gsd-wacom-key-shortcut-button
 * @short_description: A button which captures and displays a keyboard shortcut
 * @title: GsdWacomKeyShortcutButton
 *
 * GsdWacomKeyShortcutButton is a button which, when clicked, captures a keyboard
 * shortcut and displays it.
 * It works in a similar way to #GtkCellRendererAccel but, being a #GtkWidget,
 * can be added to e.g. containers.
 */

#define DEFAULT_CANCEL_KEY GDK_KEY_Escape
#define DEFAULT_CLEAR_KEY  GDK_KEY_BackSpace

enum {
  KEY_SHORTCUT_EDITED,
  KEY_SHORTCUT_CLEARED,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_SHORTCUT_KEY_VAL,
  PROP_SHORTCUT_KEY_MODS,
  PROP_SHORTCUT_MODE,
  PROP_SHORTCUT_CANCEL_KEY,
  PROP_SHORTCUT_CLEAR_KEY,
  N_PROPERTIES
};

struct _GsdWacomKeyShortcutButton
{
  GtkButton parent_instance;

  gboolean editing_mode;

  guint keyval;
  guint keycode;
  GdkModifierType mods;

  /* Temporary shortcut info used for allowing
   * modifier-only shortcuts */
  guint tmp_shortcut_keyval;
  GdkModifierType tmp_shortcut_mods;
  guint32 tmp_shortcut_time;

  GsdWacomKeyShortcutButtonMode mode;

  guint cancel_keyval;
  guint clear_keyval;
};

G_DEFINE_TYPE (GsdWacomKeyShortcutButton, gsd_wacom_key_shortcut_button, GTK_TYPE_BUTTON);

static guint signals[LAST_SIGNAL] = { 0 };

static GParamSpec *obj_properties[N_PROPERTIES] = { NULL, };

static void gsd_wacom_key_shortcut_button_changed (GsdWacomKeyShortcutButton *self);

static void
gsd_wacom_key_shortcut_button_set_property (GObject      *object,
                                            guint         property_id,
                                            const GValue *value,
                                            GParamSpec   *pspec)
{
  GsdWacomKeyShortcutButton *self = GSD_WACOM_KEY_SHORTCUT_BUTTON (object);
  gboolean changed = FALSE;

  switch (property_id)
    {
    case PROP_SHORTCUT_KEY_VAL:
      self->keyval = g_value_get_uint (value);
      changed = TRUE;
      break;

    case PROP_SHORTCUT_KEY_MODS:
      self->mods = g_value_get_uint (value);
      changed = TRUE;
      break;

    case PROP_SHORTCUT_MODE:
      self->mode = g_value_get_enum (value);
      break;

    case PROP_SHORTCUT_CANCEL_KEY:
      self->cancel_keyval = g_value_get_uint (value);
      break;

    case PROP_SHORTCUT_CLEAR_KEY:
      self->clear_keyval = g_value_get_uint (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }

  if (changed)
    gsd_wacom_key_shortcut_button_changed (self);
}

static void
gsd_wacom_key_shortcut_button_get_property (GObject      *object,
                                            guint         property_id,
                                            GValue       *value,
                                            GParamSpec   *pspec)
{
  GsdWacomKeyShortcutButton *self = GSD_WACOM_KEY_SHORTCUT_BUTTON (object);

  switch (property_id)
    {
    case PROP_SHORTCUT_KEY_VAL:
      g_value_set_uint (value, self->keyval);
      break;

    case PROP_SHORTCUT_KEY_MODS:
      g_value_set_uint (value, self->mods);
      break;

    case PROP_SHORTCUT_MODE:
      g_value_set_enum (value, self->mode);
      break;

    case PROP_SHORTCUT_CANCEL_KEY:
      g_value_set_uint (value, self->cancel_keyval);
      break;

    case PROP_SHORTCUT_CLEAR_KEY:
      g_value_set_uint (value, self->clear_keyval);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gsd_wacom_key_shortcut_set_editing_mode (GsdWacomKeyShortcutButton *self,
                                         GdkEvent                  *event)
{
  self->editing_mode = TRUE;
  gsd_wacom_key_shortcut_button_changed (self);
  gtk_widget_grab_focus (GTK_WIDGET (self));
}

static void
gsd_wacom_key_shortcut_remove_editing_mode (GsdWacomKeyShortcutButton *self)
{
  self->editing_mode = FALSE;
  self->tmp_shortcut_keyval = 0;
  self->tmp_shortcut_mods = 0;
  self->tmp_shortcut_time = 0;
}

static void
gsd_wacom_key_shortcut_button_changed (GsdWacomKeyShortcutButton *self)
{
  g_autofree gchar *text = NULL;

  if (self->editing_mode)
    {
      gtk_button_set_label (GTK_BUTTON (self), _("New shortcut…"));

      gtk_widget_set_state_flags (GTK_WIDGET (self),
                                  GTK_STATE_FLAG_ACTIVE | GTK_STATE_FLAG_PRELIGHT,
                                  FALSE);

      return;
    }

  if (self->keyval == 0 && self->mods == 0)
    {
      gtk_button_set_label (GTK_BUTTON (self), "");
      return;
    }

  text = gtk_accelerator_get_label (self->keyval, self->mods);
  gtk_button_set_label (GTK_BUTTON (self), text);
}

static void
gsd_wacom_key_shortcut_button_activate (GtkButton *self)
{
  gsd_wacom_key_shortcut_set_editing_mode (GSD_WACOM_KEY_SHORTCUT_BUTTON (self), NULL);

  GTK_BUTTON_CLASS (gsd_wacom_key_shortcut_button_parent_class)->activate (self);
}

static void
key_shortcut_finished_editing (GsdWacomKeyShortcutButton *self,
                               guint32                    time)
{
  self->editing_mode = FALSE;

  gsd_wacom_key_shortcut_remove_editing_mode (self);

  gsd_wacom_key_shortcut_button_changed (self);
}

static gboolean
gsd_wacom_key_shortcut_button_key_released_cb (GsdWacomKeyShortcutButton *self,
                                               guint                      keyval,
                                               guint                      keycode,
                                               GdkModifierType            state)
{
  if (self->tmp_shortcut_keyval == 0)
    return FALSE;

  self->keyval = self->tmp_shortcut_keyval;
  self->mods = self->tmp_shortcut_mods;

  key_shortcut_finished_editing (self, self->tmp_shortcut_time);

  g_signal_emit (self, signals[KEY_SHORTCUT_EDITED], 0);

  return TRUE;
}

static gboolean
gsd_wacom_key_shortcut_button_key_pressed_cb (GsdWacomKeyShortcutButton *self,
                                              guint                      keyval,
                                              guint                      keycode,
                                              GdkModifierType            state,
                                              GtkEventController        *controller)
{
  /* This code is based on the gtk_cell_renderer_accel_start_editing */
  GdkModifierType mods = 0;
  GdkEvent *event;
  guint shortcut_keyval;
  gboolean edited;
  gboolean cleared;

  event = gtk_event_controller_get_current_event (controller);

  /* GTK and OTHER modes don't allow modifier keyvals */
  if (gdk_key_event_is_modifier (event) && self->mode != GSD_WACOM_KEY_SHORTCUT_BUTTON_MODE_ALL)
    return TRUE;

  if (!self->editing_mode)
    return FALSE;

  edited = FALSE;
  cleared = FALSE;

  mods = state;

  if (keyval == GDK_KEY_Sys_Req && (mods & GDK_ALT_MASK) != 0)
    {
      /* HACK: we don't want to use SysRq as a keybinding (but we do
       * want Alt+Print), so we avoid translation from Alt+Print to SysRq
       */
      keyval = GDK_KEY_Print;
    }

  shortcut_keyval = gdk_keyval_to_lower (keyval);

  if (shortcut_keyval == GDK_KEY_ISO_Left_Tab)
    shortcut_keyval = GDK_KEY_Tab;

  mods &= gtk_accelerator_get_default_mod_mask ();

  /* Put shift back if it changed the case of the key, not otherwise.
   */
  if (shortcut_keyval != keyval)
    mods |= GDK_SHIFT_MASK;

  if (mods == 0)
    {
      if (keyval == self->cancel_keyval)
        {
          /* cancel the edition */
          goto out;
        }
      else if (keyval == self->clear_keyval)
        {
	  /* clear the current shortcut */
	  cleared = TRUE;
	  goto out;
	}
    }

  self->tmp_shortcut_keyval = 0;
  self->tmp_shortcut_mods = 0;
  self->tmp_shortcut_time = 0;

  if (gdk_key_event_is_modifier (event))
    {
      /* when the user presses a non-modifier key, it readily assigns the
       * shortcut but since we also support modifiers-only shortcuts, we
       * cannot assign the shortcut right when the user presses a modifier
       * key because the user might assign e.g. Alt, Alt+Ctrl, Alt+Ctrl+Shift, etc.
       * So, we keep track of the pressed shortcut's (keyval, mods and time) if
       * it is a modifier shortcut and assign them when a key-release happens */
      self->tmp_shortcut_keyval = shortcut_keyval;
      self->tmp_shortcut_mods = mods;
      self->tmp_shortcut_time = gtk_event_controller_get_current_event_time (controller);

      return TRUE;
    }

  edited = TRUE;

 out:

  if (edited)
    {
      self->keyval = shortcut_keyval;
      self->mods = mods;
    }

  if (cleared)
    {
      self->keyval = 0;
      self->mods = 0;
    }

  key_shortcut_finished_editing (self, gtk_event_controller_get_current_event_time (controller));

  if (edited)
    g_signal_emit (self, signals[KEY_SHORTCUT_EDITED], 0);
  else if (cleared)
    g_signal_emit (self, signals[KEY_SHORTCUT_CLEARED], 0);

  return TRUE;
}

static void
gsd_wacom_key_shortcut_button_button_pressed_cb (GsdWacomKeyShortcutButton *self,
                                                 gint                       n_press,
                                                 gdouble                    x,
                                                 gdouble                    y)
{
  if (!self->editing_mode)
    gsd_wacom_key_shortcut_set_editing_mode (self, NULL);
}

static void
gsd_wacom_key_shortcut_button_unrealize (GtkWidget *widget)
{
  GsdWacomKeyShortcutButton *self;

  self = GSD_WACOM_KEY_SHORTCUT_BUTTON (widget);

  gsd_wacom_key_shortcut_remove_editing_mode (self);

  GTK_WIDGET_CLASS (gsd_wacom_key_shortcut_button_parent_class)->unrealize (widget);
}

static void
gsd_wacom_key_shortcut_button_class_init (GsdWacomKeyShortcutButtonClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkButtonClass *button_class = GTK_BUTTON_CLASS (klass);

  gobject_class->set_property = gsd_wacom_key_shortcut_button_set_property;
  gobject_class->get_property = gsd_wacom_key_shortcut_button_get_property;

  obj_properties[PROP_SHORTCUT_KEY_VAL] =
    g_param_spec_uint ("key-value",
                       "The key value",
                       "The key value of the shortcut currently set",
                       0,
                       G_MAXUINT,
                       0,
                       G_PARAM_READWRITE |
                       G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_SHORTCUT_KEY_MODS] =
    g_param_spec_uint ("key-mods",
                       "The key modifiers",
                       "The key modifiers of the shortcut currently set",
                       0,
                       G_MAXUINT,
                       0,
                       G_PARAM_READWRITE |
                       G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_SHORTCUT_CANCEL_KEY] =
    g_param_spec_uint ("cancel-key",
                       "The cancel key",
                       "The key which cancels the edition of the shortcut",
                       0,
                       G_MAXUINT,
                       DEFAULT_CANCEL_KEY,
                       G_PARAM_READWRITE |
                       G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_SHORTCUT_CLEAR_KEY] =
    g_param_spec_uint ("clear-key",
                       "The clear key",
                       "The key which clears the currently set shortcut",
                       0,
                       G_MAXUINT,
                       DEFAULT_CLEAR_KEY,
                       G_PARAM_READWRITE |
                       G_PARAM_STATIC_STRINGS);

  /**
   * GsdWacomKeyShortcutButton:mode:
   *
   * Determines which type of keys are allowed in the captured shortcuts.
   * %GSD_WACOM_KEY_SHORTCUT_BUTTON_MODE_ALL is the same as
   * %GSD_WACOM_KEY_SHORTCUT_BUTTON_MODE_OTHER but allows shortcuts composed of
   * only modifier keys.
   */
  obj_properties[PROP_SHORTCUT_MODE] =
    g_param_spec_enum ("mode",
                       "The shortcut mode",
                       "The mode with which the shortcuts are captured",
                       GSD_WACOM_TYPE_KEY_SHORTCUT_BUTTON_MODE,
                       GSD_WACOM_KEY_SHORTCUT_BUTTON_MODE_OTHER,
                       G_PARAM_READWRITE |
                       G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class,
                                     N_PROPERTIES,
                                     obj_properties);

  widget_class->unrealize = gsd_wacom_key_shortcut_button_unrealize;

  button_class->activate = gsd_wacom_key_shortcut_button_activate;

  /**
   * GsdWacomKeyShortcutButton::key-shortcut-edited:
   * @keyshortcutbutton: the #GsdWacomKeyShortcutButton
   *
   * Emitted when the key shortcut of the @keyshortcutbutton is edited.
   *
   * The new shortcut can be retrieved by using the #GsdWacomKeyShortcutButton:key-value
   * and #GsdWacomKeyShortcutButton:key-mods properties.
   */
  signals[KEY_SHORTCUT_EDITED] = g_signal_new ("key-shortcut-edited",
                                               GSD_WACOM_TYPE_KEY_SHORTCUT_BUTTON,
                                               G_SIGNAL_RUN_LAST,
                                               0,
                                               NULL, NULL,
                                               g_cclosure_marshal_VOID__VOID,
                                               G_TYPE_NONE, 0);

  /**
   * GsdWacomKeyShortcutButton::key-shortcut-cleared:
   * @keyshortcutbutton: the #GsdWacomKeyShortcutButton
   *
   * Emitted when the key shortcut of the @keyshortcutbutton is cleared.
   */
  signals[KEY_SHORTCUT_CLEARED] = g_signal_new ("key-shortcut-cleared",
                                                GSD_WACOM_TYPE_KEY_SHORTCUT_BUTTON,
                                                G_SIGNAL_RUN_LAST,
                                                0,
                                                NULL, NULL,
                                                g_cclosure_marshal_VOID__VOID,
                                                G_TYPE_NONE, 0);
}

static void
gsd_wacom_key_shortcut_button_init (GsdWacomKeyShortcutButton *self)
{
  GtkEventController *controller;
  GtkGesture *gesture;

  self->cancel_keyval = DEFAULT_CANCEL_KEY;
  self->clear_keyval = DEFAULT_CLEAR_KEY;

  controller = gtk_event_controller_key_new ();
  g_signal_connect_swapped (controller, "key-pressed", G_CALLBACK (gsd_wacom_key_shortcut_button_key_pressed_cb), self);
  g_signal_connect_swapped (controller, "key-released", G_CALLBACK (gsd_wacom_key_shortcut_button_key_released_cb), self);
  gtk_widget_add_controller (GTK_WIDGET (self), controller);

  gesture = gtk_gesture_click_new ();
  g_signal_connect_swapped (gesture, "pressed", G_CALLBACK (gsd_wacom_key_shortcut_button_button_pressed_cb), self);
  gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (gesture));
}

/**
 * gsd_wacom_key_shortcut_button_new:
 *
 * Creates a new #GsdWacomKeyShortcutButton.
 *
 * Returns: a new #GsdWacomKeyShortcutButton object.
 *
 * Since: 3.10
 */
GtkWidget *
gsd_wacom_key_shortcut_button_new (void)
{
  return g_object_new (GSD_WACOM_TYPE_KEY_SHORTCUT_BUTTON, NULL);
}

GType
gsd_wacom_key_shortcut_button_mode_type (void)
{
  static GType enum_type_id = 0;
  if (G_UNLIKELY (!enum_type_id))
    {
      static const GEnumValue values[] =
        {
          { GSD_WACOM_KEY_SHORTCUT_BUTTON_MODE_OTHER, "OTHER", "other" },
          { GSD_WACOM_KEY_SHORTCUT_BUTTON_MODE_ALL, "ALL", "all" },
          { 0, NULL, NULL }
        };
      enum_type_id = g_enum_register_static ("GsdWacomKeyShortcutButtonMode", values);
    }
  return enum_type_id;
}
