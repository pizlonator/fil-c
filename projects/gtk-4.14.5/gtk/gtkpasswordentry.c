/* GTK - The GIMP Toolkit
 * Copyright (C) 2019 Red Hat, Inc.
 *
 * Authors:
 * - Matthias Clasen <mclasen@redhat.com>
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

#include "gtkpasswordentryprivate.h"

#include "gtkaccessibleprivate.h"
#include "gtktextprivate.h"
#include "gtkeditable.h"
#include "gtkeventcontrollerkey.h"
#include "gtkgestureclick.h"
#include <glib/gi18n-lib.h>
#include "gtkmarshalers.h"
#include "gtkpasswordentrybuffer.h"
#include "gtkprivate.h"
#include "gtkwidgetprivate.h"
#include "gtkcsspositionvalueprivate.h"
#include "gtkcssnodeprivate.h"
#include "gtkjoinedmenuprivate.h"


/**
 * GtkPasswordEntry:
 *
 * `GtkPasswordEntry` is an entry that has been tailored for entering secrets.
 *
 * ![An example GtkPasswordEntry](password-entry.png)
 *
 * It does not show its contents in clear text, does not allow to copy it
 * to the clipboard, and it shows a warning when Caps Lock is engaged. If
 * the underlying platform allows it, `GtkPasswordEntry` will also place
 * the text in a non-pageable memory area, to avoid it being written out
 * to disk by the operating system.
 *
 * Optionally, it can offer a way to reveal the contents in clear text.
 *
 * `GtkPasswordEntry` provides only minimal API and should be used with
 * the [iface@Gtk.Editable] API.
 *
 * # CSS Nodes
 *
 * ```
 * entry.password
 * ╰── text
 *     ├── image.caps-lock-indicator
 *     ┊
 * ```
 *
 * `GtkPasswordEntry` has a single CSS node with name entry that carries
 * a .passwordstyle class. The text Css node below it has a child with
 * name image and style class .caps-lock-indicator for the Caps Lock
 * icon, and possibly other children.
 *
 * # Accessibility
 *
 * `GtkPasswordEntry` uses the %GTK_ACCESSIBLE_ROLE_TEXT_BOX role.
 */

struct _GtkPasswordEntry
{
  GtkWidget parent_instance;

  GtkWidget *entry;
  GtkWidget *icon;
  GtkWidget *peek_icon;
  GdkDevice *keyboard;
  GMenuModel *extra_menu;
};

struct _GtkPasswordEntryClass
{
  GtkWidgetClass parent_class;
};

enum {
  ACTIVATE,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0, };

enum {
  PROP_PLACEHOLDER_TEXT = 1,
  PROP_ACTIVATES_DEFAULT,
  PROP_SHOW_PEEK_ICON,
  PROP_EXTRA_MENU,
  NUM_PROPERTIES 
};

static GParamSpec *props[NUM_PROPERTIES] = { NULL, };

static void gtk_password_entry_editable_init (GtkEditableInterface *iface);
static void gtk_password_entry_accessible_init (GtkAccessibleInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkPasswordEntry, gtk_password_entry, GTK_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ACCESSIBLE, gtk_password_entry_accessible_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_EDITABLE, gtk_password_entry_editable_init))

static void
caps_lock_state_changed (GdkDevice  *device,
                         GParamSpec *pspec,
                        GtkWidget   *widget)
{
  GtkPasswordEntry *entry = GTK_PASSWORD_ENTRY (widget);

  gtk_widget_set_visible (entry->icon, gtk_editable_get_editable (GTK_EDITABLE (entry)) &&
                                       gtk_widget_has_focus (entry->entry) &&
                                       !gtk_text_get_visibility (GTK_TEXT (entry->entry)) &&
                                       gdk_device_get_caps_lock_state (device));
}

static void
focus_changed (GtkWidget *widget)
{
  GtkPasswordEntry *entry = GTK_PASSWORD_ENTRY (widget);

  if (entry->keyboard)
    caps_lock_state_changed (entry->keyboard, NULL, widget);
}

static void
gtk_password_entry_icon_press (GtkGesture *gesture)
{
  gtk_gesture_set_state (gesture, GTK_EVENT_SEQUENCE_CLAIMED);
}

/*< private >
 * gtk_password_entry_toggle_peek:
 * @entry: a `GtkPasswordEntry`
 *
 * Toggles the text visibility.
 */
void
gtk_password_entry_toggle_peek (GtkPasswordEntry *entry)
{
  gboolean visibility;

  visibility = gtk_text_get_visibility (GTK_TEXT (entry->entry));
  gtk_text_set_visibility (GTK_TEXT (entry->entry), !visibility);
}

static void
visibility_toggled (GObject          *object,
                    GParamSpec       *pspec,
                    GtkPasswordEntry *entry)
{
  if (gtk_text_get_visibility (GTK_TEXT (entry->entry)))
    {
      gtk_image_set_from_icon_name (GTK_IMAGE (entry->peek_icon), "view-conceal-symbolic");
      gtk_widget_set_tooltip_text (entry->peek_icon, _("Hide Text"));
    }
  else
    {
      gtk_image_set_from_icon_name (GTK_IMAGE (entry->peek_icon), "view-reveal-symbolic");
      gtk_widget_set_tooltip_text (entry->peek_icon, _("Show Text"));
    }

  if (entry->keyboard)
    caps_lock_state_changed (entry->keyboard, NULL, GTK_WIDGET (entry));
}

static void
activate_cb (GtkPasswordEntry *entry)
{
  g_signal_emit (entry, signals[ACTIVATE], 0);
}

static void
catchall_click_press (GtkGestureClick *gesture,
                      int              n_press,
                      double           x,
                      double           y,
                      gpointer         user_data)
{
  gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
}

static void
gtk_password_entry_init (GtkPasswordEntry *entry)
{
  GtkGesture *catchall;
  GtkEntryBuffer *buffer = gtk_password_entry_buffer_new ();

  entry->entry = gtk_text_new ();
  gtk_text_set_buffer (GTK_TEXT (entry->entry), buffer);
  gtk_text_set_visibility (GTK_TEXT (entry->entry), FALSE);
  gtk_text_set_input_purpose (GTK_TEXT (entry->entry), GTK_INPUT_PURPOSE_PASSWORD);
  gtk_widget_set_parent (entry->entry, GTK_WIDGET (entry));
  gtk_editable_init_delegate (GTK_EDITABLE (entry));
  g_signal_connect_swapped (entry->entry, "notify::has-focus", G_CALLBACK (focus_changed), entry);
  g_signal_connect_swapped (entry->entry, "activate", G_CALLBACK (activate_cb), entry);

  entry->icon = g_object_new (GTK_TYPE_IMAGE,
                              "icon-name", "caps-lock-symbolic",
                              "accessible-role", GTK_ACCESSIBLE_ROLE_ALERT,
                              NULL);
  gtk_widget_set_tooltip_text (entry->icon, _("Caps Lock is on"));
  gtk_widget_add_css_class (entry->icon, "caps-lock-indicator");
  gtk_widget_set_cursor (entry->icon, gtk_widget_get_cursor (entry->entry));
  gtk_widget_set_parent (entry->icon, GTK_WIDGET (entry));

  catchall = gtk_gesture_click_new ();
  g_signal_connect (catchall, "pressed",
                    G_CALLBACK (catchall_click_press), entry);
  gtk_widget_add_controller (GTK_WIDGET (entry),
                             GTK_EVENT_CONTROLLER (catchall));

  gtk_widget_add_css_class (GTK_WIDGET (entry), I_("password"));

  gtk_password_entry_set_extra_menu (entry, NULL);

  /* Transfer ownership to the GtkText widget */
  g_object_unref (buffer);

  gtk_accessible_update_property (GTK_ACCESSIBLE (entry),
                                  GTK_ACCESSIBLE_PROPERTY_HAS_POPUP, TRUE,
                                  -1);
}

static void
gtk_password_entry_realize (GtkWidget *widget)
{
  GtkPasswordEntry *entry = GTK_PASSWORD_ENTRY (widget);
  GdkSeat *seat;

  GTK_WIDGET_CLASS (gtk_password_entry_parent_class)->realize (widget);

  seat = gdk_display_get_default_seat (gtk_widget_get_display (widget));
  if (seat)
    entry->keyboard = gdk_seat_get_keyboard (seat);

  if (entry->keyboard)
    {
      g_signal_connect (entry->keyboard, "notify::caps-lock-state",
                        G_CALLBACK (caps_lock_state_changed), entry);
      caps_lock_state_changed (entry->keyboard, NULL, widget);
    }
}

static void
gtk_password_entry_unrealize (GtkWidget *widget)
{
  GtkPasswordEntry *entry = GTK_PASSWORD_ENTRY (widget);

  if (entry->keyboard)
    {
      g_signal_handlers_disconnect_by_func (entry->keyboard, caps_lock_state_changed, entry);
      entry->keyboard = NULL;
    }

  GTK_WIDGET_CLASS (gtk_password_entry_parent_class)->unrealize (widget);
}

static void
gtk_password_entry_dispose (GObject *object)
{
  GtkPasswordEntry *entry = GTK_PASSWORD_ENTRY (object);

  if (entry->entry)
    gtk_editable_finish_delegate (GTK_EDITABLE (entry));

  g_clear_pointer (&entry->entry, gtk_widget_unparent);
  g_clear_pointer (&entry->icon, gtk_widget_unparent);
  g_clear_pointer (&entry->peek_icon, gtk_widget_unparent);
  g_clear_object (&entry->extra_menu);

  G_OBJECT_CLASS (gtk_password_entry_parent_class)->dispose (object);
}

static void
gtk_password_entry_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GtkPasswordEntry *entry = GTK_PASSWORD_ENTRY (object);
  const char *text;

  if (gtk_editable_delegate_set_property (object, prop_id, value, pspec))
    {
      if (prop_id == NUM_PROPERTIES + GTK_EDITABLE_PROP_EDITABLE)
        {
          gtk_accessible_update_property (GTK_ACCESSIBLE (entry),
                                          GTK_ACCESSIBLE_PROPERTY_READ_ONLY, !g_value_get_boolean (value),
                                          -1);
        }
      return;
    }

  switch (prop_id)
    {
    case PROP_PLACEHOLDER_TEXT:
      text = g_value_get_string (value);
      gtk_text_set_placeholder_text (GTK_TEXT (entry->entry), text);
      gtk_accessible_update_property (GTK_ACCESSIBLE (entry),
                                      GTK_ACCESSIBLE_PROPERTY_PLACEHOLDER, text,
                                      -1);
      break;

    case PROP_ACTIVATES_DEFAULT:
      if (gtk_text_get_activates_default (GTK_TEXT (entry->entry)) != g_value_get_boolean (value))
        {
          gtk_text_set_activates_default (GTK_TEXT (entry->entry), g_value_get_boolean (value));
          g_object_notify_by_pspec (object, pspec);
        }
      break;

    case PROP_SHOW_PEEK_ICON:
      gtk_password_entry_set_show_peek_icon (entry, g_value_get_boolean (value));
      break;

    case PROP_EXTRA_MENU:
      gtk_password_entry_set_extra_menu (entry, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_password_entry_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GtkPasswordEntry *entry = GTK_PASSWORD_ENTRY (object);

  if (gtk_editable_delegate_get_property (object, prop_id, value, pspec))
    return;

  switch (prop_id)
    {
    case PROP_PLACEHOLDER_TEXT:
      g_value_set_string (value, gtk_text_get_placeholder_text (GTK_TEXT (entry->entry)));
      break;

    case PROP_ACTIVATES_DEFAULT:
      g_value_set_boolean (value, gtk_text_get_activates_default (GTK_TEXT (entry->entry)));
      break;

    case PROP_SHOW_PEEK_ICON:
      g_value_set_boolean (value, gtk_password_entry_get_show_peek_icon (entry));
      break;

    case PROP_EXTRA_MENU:
      g_value_set_object (value, entry->extra_menu);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_password_entry_measure (GtkWidget      *widget,
                            GtkOrientation  orientation,
                            int             for_size,
                            int            *minimum,
                            int            *natural,
                            int            *minimum_baseline,
                            int            *natural_baseline)
{
  GtkPasswordEntry *entry = GTK_PASSWORD_ENTRY (widget);
  int icon_min = 0, icon_nat = 0;

  gtk_widget_measure (entry->entry, orientation, for_size,
                      minimum, natural,
                      minimum_baseline, natural_baseline);

  if (entry->icon && gtk_widget_get_visible (entry->icon))
    gtk_widget_measure (entry->icon, orientation, for_size,
                        &icon_min, &icon_nat,
                        NULL, NULL);

  if (entry->peek_icon && gtk_widget_get_visible (entry->peek_icon))
    gtk_widget_measure (entry->peek_icon, orientation, for_size,
                        &icon_min, &icon_nat,
                        NULL, NULL);
}

static void
gtk_password_entry_size_allocate (GtkWidget *widget,
                                  int        width,
                                  int        height,
                                  int        baseline)
{
  GtkPasswordEntry *entry = GTK_PASSWORD_ENTRY (widget);
  GtkCssStyle *style = gtk_css_node_get_style (gtk_widget_get_css_node (widget));
  int icon_min = 0, icon_nat = 0;
  int peek_min = 0, peek_nat = 0;
  int text_width;
  int spacing;

  spacing = _gtk_css_position_value_get_x (style->size->border_spacing, 100);

  if (entry->icon && gtk_widget_get_visible (entry->icon))
    gtk_widget_measure (entry->icon, GTK_ORIENTATION_HORIZONTAL, -1,
                        &icon_min, &icon_nat,
                        NULL, NULL);

  if (entry->peek_icon && gtk_widget_get_visible (entry->peek_icon))
    gtk_widget_measure (entry->peek_icon, GTK_ORIENTATION_HORIZONTAL, -1,
                        &peek_min, &peek_nat,
                        NULL, NULL);

  text_width = width - (icon_nat + (icon_nat > 0 ? spacing : 0))
                     - (peek_nat + (peek_nat > 0 ? spacing : 0));

  gtk_widget_size_allocate (entry->entry,
                            &(GtkAllocation) { 0, 0, text_width, height },
                            baseline);

  if (entry->icon && gtk_widget_get_visible (entry->icon))
    gtk_widget_size_allocate (entry->icon,
                              &(GtkAllocation) { text_width + spacing, 0, icon_nat, height },
                              baseline);

  if (entry->peek_icon && gtk_widget_get_visible (entry->peek_icon))
    gtk_widget_size_allocate (entry->peek_icon,
                              &(GtkAllocation) { text_width + spacing + icon_nat + (icon_nat > 0 ? spacing : 0), 0, peek_nat, height },
                              baseline);
}

static gboolean
gtk_password_entry_mnemonic_activate (GtkWidget *widget,
                                      gboolean   group_cycling)
{
  GtkPasswordEntry *entry = GTK_PASSWORD_ENTRY (widget);

  gtk_widget_grab_focus (entry->entry);

  return TRUE;
}

static void
gtk_password_entry_class_init (GtkPasswordEntryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gtk_password_entry_dispose;
  object_class->get_property = gtk_password_entry_get_property;
  object_class->set_property = gtk_password_entry_set_property;

  widget_class->realize = gtk_password_entry_realize;
  widget_class->unrealize = gtk_password_entry_unrealize;
  widget_class->measure = gtk_password_entry_measure;
  widget_class->size_allocate = gtk_password_entry_size_allocate;
  widget_class->mnemonic_activate = gtk_password_entry_mnemonic_activate;
  widget_class->grab_focus = gtk_widget_grab_focus_child;
  widget_class->focus = gtk_widget_focus_child;

  /**
   * GtkPasswordEntry:placeholder-text:
   *
   * The text that will be displayed in the `GtkPasswordEntry`
   * when it is empty and unfocused.
   */
  props[PROP_PLACEHOLDER_TEXT] =
      g_param_spec_string ("placeholder-text", NULL, NULL,
                           NULL,
                           GTK_PARAM_READWRITE);

  /**
   * GtkPasswordEntry:activates-default:
   *
   * Whether to activate the default widget when Enter is pressed.
   */
  props[PROP_ACTIVATES_DEFAULT] =
      g_param_spec_boolean ("activates-default", NULL, NULL,
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkPasswordEntry:show-peek-icon: (attributes org.gtk.Property.get=gtk_password_entry_get_show_peek_icon org.gtk.Property.set=gtk_password_entry_set_show_peek_icon)
   *
   * Whether to show an icon for revealing the content.
   */
  props[PROP_SHOW_PEEK_ICON] =
      g_param_spec_boolean ("show-peek-icon", NULL, NULL,
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkPasswordEntry:extra-menu: (attributes org.gtk.Property.get=gtk_password_entry_get_extra_menu org.gtk.Property.set=gtk_password_entry_set_extra_menu)
   *
   * A menu model whose contents will be appended to
   * the context menu.
   */
  props[PROP_EXTRA_MENU] =
      g_param_spec_object ("extra-menu", NULL, NULL,
                           G_TYPE_MENU_MODEL,
                           GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, props);
  gtk_editable_install_properties (object_class, NUM_PROPERTIES);

  /**
   * GtkPasswordEntry::activate:
   * @self: The widget on which the signal is emitted
   *
   * Emitted when the entry is activated.
   *
   * The keybindings for this signal are all forms of the Enter key.
   */
  signals[ACTIVATE] =
    g_signal_new (I_("activate"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  gtk_widget_class_set_css_name (widget_class, I_("entry"));
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_TEXT_BOX);
}

static GtkEditable *
gtk_password_entry_get_delegate (GtkEditable *editable)
{
  GtkPasswordEntry *entry = GTK_PASSWORD_ENTRY (editable);

  return GTK_EDITABLE (entry->entry);
}

static void
gtk_password_entry_editable_init (GtkEditableInterface *iface)
{
  iface->get_delegate = gtk_password_entry_get_delegate;
}

static gboolean
gtk_password_entry_accessible_get_platform_state (GtkAccessible              *self,
                                                  GtkAccessiblePlatformState  state)
{
  return gtk_editable_delegate_get_accessible_platform_state (GTK_EDITABLE (self), state);
}

static void
gtk_password_entry_accessible_init (GtkAccessibleInterface *iface)
{
  GtkAccessibleInterface *parent_iface = g_type_interface_peek_parent (iface);
  iface->get_at_context = parent_iface->get_at_context;
  iface->get_platform_state = gtk_password_entry_accessible_get_platform_state;
}

/*< private >
 * gtk_password_entry_get_text_widget
 * @entry: a `GtkPasswordEntry`
 *
 * Retrieves the `GtkText` delegate of the `GtkPasswordEntry`.
 *
 * Returns: (transfer none): the `GtkText` delegate widget
 */
GtkText *
gtk_password_entry_get_text_widget (GtkPasswordEntry *entry)
{
  g_return_val_if_fail (GTK_IS_PASSWORD_ENTRY (entry), NULL);

  return GTK_TEXT (entry->entry);
}

/**
 * gtk_password_entry_new:
 *
 * Creates a `GtkPasswordEntry`.
 *
 * Returns: a new `GtkPasswordEntry`
 */
GtkWidget *
gtk_password_entry_new (void)
{
  return GTK_WIDGET (g_object_new (GTK_TYPE_PASSWORD_ENTRY, NULL));
}

/**
 * gtk_password_entry_set_show_peek_icon: (attributes org.gtk.Method.set_property=show-peek-icon)
 * @entry: a `GtkPasswordEntry`
 * @show_peek_icon: whether to show the peek icon
 *
 * Sets whether the entry should have a clickable icon
 * to reveal the contents.
 *
 * Setting this to %FALSE also hides the text again.
 */
void
gtk_password_entry_set_show_peek_icon (GtkPasswordEntry *entry,
                                       gboolean          show_peek_icon)
{
  g_return_if_fail (GTK_IS_PASSWORD_ENTRY (entry));

  show_peek_icon = !!show_peek_icon;

  if (show_peek_icon == (entry->peek_icon != NULL))
    return;

  if (show_peek_icon)
    {
      GtkGesture *press;

      entry->peek_icon = gtk_image_new_from_icon_name ("view-reveal-symbolic");
      gtk_widget_set_tooltip_text (entry->peek_icon, _("Show Text"));
      gtk_widget_set_parent (entry->peek_icon, GTK_WIDGET (entry));

      press = gtk_gesture_click_new ();
      g_signal_connect (press, "pressed",
                        G_CALLBACK (gtk_password_entry_icon_press), entry);
      g_signal_connect_swapped (press, "released",
                                G_CALLBACK (gtk_password_entry_toggle_peek), entry);
      gtk_widget_add_controller (entry->peek_icon, GTK_EVENT_CONTROLLER (press));

      g_signal_connect (entry->entry, "notify::visibility",
                        G_CALLBACK (visibility_toggled), entry);
      visibility_toggled (G_OBJECT (entry->entry), NULL, entry);
    }
  else
    {
      g_clear_pointer (&entry->peek_icon, gtk_widget_unparent);
      gtk_text_set_visibility (GTK_TEXT (entry->entry), FALSE);
      g_signal_handlers_disconnect_by_func (entry->entry,
                                            visibility_toggled,
                                            entry);
    }

  if (entry->keyboard)
    caps_lock_state_changed (entry->keyboard, NULL, GTK_WIDGET (entry));

  g_object_notify_by_pspec (G_OBJECT (entry), props[PROP_SHOW_PEEK_ICON]);
}

/**
 * gtk_password_entry_get_show_peek_icon: (attributes org.gtk.Method.get_property=show-peek-icon)
 * @entry: a `GtkPasswordEntry`
 *
 * Returns whether the entry is showing an icon to
 * reveal the contents.
 *
 * Returns: %TRUE if an icon is shown
 */
gboolean
gtk_password_entry_get_show_peek_icon (GtkPasswordEntry *entry)
{
  g_return_val_if_fail (GTK_IS_PASSWORD_ENTRY (entry), FALSE);

  return entry->peek_icon != NULL;
}

/**
 * gtk_password_entry_set_extra_menu: (attributes org.gtk.Method.set_property=extra-menu)
 * @entry: a `GtkPasswordEntry`
 * @model: (nullable): a `GMenuModel`
 *
 * Sets a menu model to add when constructing
 * the context menu for @entry.
 */
void
gtk_password_entry_set_extra_menu (GtkPasswordEntry *entry,
                                   GMenuModel       *model)
{
  GtkJoinedMenu *joined;
  GMenu *menu;
  GMenu *section;
  GMenuItem *item;

  g_return_if_fail (GTK_IS_PASSWORD_ENTRY (entry));

  /* bypass this check for the initial call from init */
  if (entry->extra_menu)
    {
      if (!g_set_object (&entry->extra_menu, model))
        return;
    }

  joined = gtk_joined_menu_new ();
  menu = g_menu_new ();

  section = g_menu_new ();
  item = g_menu_item_new (_("_Show Text"), "misc.toggle-visibility");
  g_menu_item_set_attribute (item, "touch-icon", "s", "view-reveal-symbolic");
  g_menu_append_item (section, item);
  g_object_unref (item);

  g_menu_append_section (menu, NULL, G_MENU_MODEL (section));
  g_object_unref (section);

  gtk_joined_menu_append_menu (joined, G_MENU_MODEL (menu));
  g_object_unref (menu);

  if (model)
    gtk_joined_menu_append_menu (joined, model);

  gtk_text_set_extra_menu (GTK_TEXT (entry->entry), G_MENU_MODEL (joined));

  g_object_unref (joined);

  g_object_notify_by_pspec (G_OBJECT (entry), props[PROP_EXTRA_MENU]);
}

/**
 * gtk_password_entry_get_extra_menu: (attributes org.gtk.Method.get_property=extra-menu)
 * @entry: a `GtkPasswordEntry`
 *
 * Gets the menu model set with gtk_password_entry_set_extra_menu().
 *
 * Returns: (transfer none) (nullable): the menu model
 */
GMenuModel *
gtk_password_entry_get_extra_menu (GtkPasswordEntry *entry)
{
  return entry->extra_menu;
}
