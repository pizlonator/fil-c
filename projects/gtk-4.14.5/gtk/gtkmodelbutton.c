/*
 * Copyright © 2014 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the licence, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Matthias Clasen
 */

#include "config.h"

#include "gtkmodelbuttonprivate.h"

#include "gtkactionhelperprivate.h"
#include "gtkboxlayout.h"
#include "gtkgestureclick.h"
#include "gtkwidgetprivate.h"
#include "gtkmenutrackeritemprivate.h"
#include "gtkimage.h"
#include "gtklabel.h"
#include "gtkbox.h"
#include "gtktypebuiltins.h"
#include "gtkstack.h"
#include "gtkpopovermenuprivate.h"
#include "gtkcssnodeprivate.h"
#include "gtkcsstypesprivate.h"
#include "gtkbuiltiniconprivate.h"
#include "gtksizegroup.h"
#include "gtkactionable.h"
#include "gtkeventcontrollermotion.h"
#include "gtkeventcontrollerkey.h"
#include "gtkeventcontrollerfocus.h"
#include "gtknative.h"
#include "gtkshortcuttrigger.h"
#include "gtkshortcutcontroller.h"
#include "gtkshortcut.h"
#include "gtkaccessibleprivate.h"
#include "gtkprivate.h"

/*< private >
 * GtkModelButton:
 *
 * GtkModelButton is a button class that can use a GAction as its model.
 * In contrast to GtkToggleButton or GtkCheckButton, which can also
 * be backed by a GAction via the GtkActionable:action-name property,
 * GtkModelButton will adapt its appearance according to the kind of
 * action it is backed by, and appear either as a plain, check or
 * radio button.
 *
 * Model buttons are used when popovers from a menu model with
 * gtk_popover_menu_new_from_model(); they can also be used manually in
 * a GtkPopoverMenu.
 *
 * When the action is specified via the GtkActionable:action-name
 * and GtkActionable:action-target properties, the role of the button
 * (i.e. whether it is a plain, check or radio button) is determined by
 * the type of the action and doesn't have to be explicitly specified
 * with the GtkModelButton:role property.
 *
 * The content of the button is specified by the GtkModelButton:text
 * and GtkModelButton:icon properties.
 *
 * The appearance of model buttons can be influenced with the
 * GtkModelButton:iconic property.
 *
 * Model buttons have built-in support for submenus in GtkPopoverMenu.
 * To make a GtkModelButton that opens a submenu when activated, set
 * the GtkModelButton:menu-name property. To make a button that goes
 * back to the parent menu, you should set the GtkModelButton:inverted
 * property to place the submenu indicator at the opposite side.
 *
 * # Example
 *
 * |[
 * <object class="GtkPopoverMenu">
 *   <child>
 *     <object class="GtkBox">
 *       <property name="visible">True</property>
 *       <property name="margin-start">10</property>
 *       <property name="margin-end">10</property>
 *       <property name="margin-top">10</property>
 *       <property name="margin-bottom">10</property>
 *       <child>
 *         <object class="GtkModelButton">
 *           <property name="visible">True</property>
 *           <property name="action-name">view.cut</property>
 *           <property name="text" translatable="yes">Cut</property>
 *         </object>
 *       </child>
 *       <child>
 *         <object class="GtkModelButton">
 *           <property name="visible">True</property>
 *           <property name="action-name">view.copy</property>
 *           <property name="text" translatable="yes">Copy</property>
 *         </object>
 *       </child>
 *       <child>
 *         <object class="GtkModelButton">
 *           <property name="visible">True</property>
 *           <property name="action-name">view.paste</property>
 *           <property name="text" translatable="yes">Paste</property>
 *         </object>
 *       </child>
 *     </object>
 *   </child>
 * </object>
 * ]|
 *
 * # CSS nodes
 *
 * |[<!-- language="plain" -->
 * modelbutton
 * ├── <child>
 * ╰── check
 * ]|
 *
 * |[<!-- language="plain" -->
 * modelbutton
 * ├── <child>
 * ╰── radio
 * ]|
 *
 * |[<!-- language="plain" -->
 * modelbutton
 * ├── <child>
 * ╰── arrow
 * ]|
 *
 * GtkModelButton has a main CSS node with name modelbutton, and a subnode,
 * which will have the name check, radio or arrow, depending on the role
 * of the button and whether it has a menu name set.
 *
 * The subnode is positioned before or after the content nodes and gets the
 * .left or .right style class, depending on where it is located.
 *
 * |[<!-- language="plain" -->
 * button.model
 * ├── <child>
 * ╰── check
 * ]|
 *
 * Iconic model buttons (see GtkModelButton:iconic) change the name of
 * their main node to button and add a .model style class to it. The indicator
 * subnode is invisible in this case.
 */

struct _GtkModelButton
{
  GtkWidget parent_instance;

  GtkWidget *box;
  GtkWidget *image;
  GtkWidget *label;
  GtkWidget *accel_label;
  GtkWidget *start_box;
  GtkWidget *start_indicator;
  GtkWidget *end_indicator;
  GtkWidget *popover;
  GtkActionHelper *action_helper;
  char *menu_name;
  GtkButtonRole role;
  GtkSizeGroup *indicators;
  char *accel;
  guint open_timeout;
  GtkEventController *controller;

  guint active : 1;
  guint iconic : 1;
  guint keep_open : 1;
};

typedef struct _GtkModelButtonClass GtkModelButtonClass;

struct _GtkModelButtonClass
{
  GtkWidgetClass parent_class;

  void (* clicked) (GtkModelButton *button);
};

static void gtk_model_button_actionable_iface_init (GtkActionableInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkModelButton, gtk_model_button, GTK_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ACTIONABLE, gtk_model_button_actionable_iface_init))

GType
gtk_button_role_get_type (void)
{
  static gsize gtk_button_role_type;

  if (g_once_init_enter (&gtk_button_role_type))
    {
      static const GEnumValue values[] = {
        { GTK_BUTTON_ROLE_NORMAL, "GTK_BUTTON_ROLE_NORMAL", "normal" },
        { GTK_BUTTON_ROLE_CHECK, "GTK_BUTTON_ROLE_CHECK", "check" },
        { GTK_BUTTON_ROLE_RADIO, "GTK_BUTTON_ROLE_RADIO", "radio" },
        { GTK_BUTTON_ROLE_TITLE, "GTK_BUTTON_ROLE_RADIO", "title" },
        { 0, NULL, NULL }
      };
      GType type;

      type = g_enum_register_static (I_("GtkButtonRole"), values);

      g_once_init_leave (&gtk_button_role_type, type);
    }

  return gtk_button_role_type;
}

enum
{
  PROP_0,
  PROP_ROLE,
  PROP_ICON,
  PROP_TEXT,
  PROP_USE_MARKUP,
  PROP_ACTIVE,
  PROP_MENU_NAME,
  PROP_POPOVER,
  PROP_ICONIC,
  PROP_ACCEL,
  PROP_INDICATOR_SIZE_GROUP,

  /* actionable properties */
  PROP_ACTION_NAME,
  PROP_ACTION_TARGET,
  LAST_PROP = PROP_ACTION_NAME
};

enum
{
  SIGNAL_CLICKED,
  LAST_SIGNAL
};

static GParamSpec *properties[LAST_PROP] = { NULL, };
static guint signals[LAST_SIGNAL] = { 0 };

static void
gtk_model_button_set_action_name (GtkActionable *actionable,
                                  const char    *action_name)
{
  GtkModelButton *self = GTK_MODEL_BUTTON (actionable);

  if (!self->action_helper)
    self->action_helper = gtk_action_helper_new (actionable);

  gtk_action_helper_set_action_name (self->action_helper, action_name);
}

static void
gtk_model_button_set_action_target_value (GtkActionable *actionable,
                                          GVariant      *action_target)
{
  GtkModelButton *self = GTK_MODEL_BUTTON (actionable);

  if (!self->action_helper)
    self->action_helper = gtk_action_helper_new (actionable);

  gtk_action_helper_set_action_target_value (self->action_helper, action_target);
}

static const char *
gtk_model_button_get_action_name (GtkActionable *actionable)
{
  GtkModelButton *self = GTK_MODEL_BUTTON (actionable);

  return gtk_action_helper_get_action_name (self->action_helper);
}

static GVariant *
gtk_model_button_get_action_target_value (GtkActionable *actionable)
{
  GtkModelButton *self = GTK_MODEL_BUTTON (actionable);

  return gtk_action_helper_get_action_target_value (self->action_helper);
}

static void
gtk_model_button_actionable_iface_init (GtkActionableInterface *iface)
{
  iface->get_action_name = gtk_model_button_get_action_name;
  iface->set_action_name = gtk_model_button_set_action_name;
  iface->get_action_target_value = gtk_model_button_get_action_target_value;
  iface->set_action_target_value = gtk_model_button_set_action_target_value;
}

static void
update_at_context (GtkModelButton *button)
{
  GtkAccessibleRole role;
  GtkATContext *context;
  gboolean was_realized;

  context = gtk_accessible_get_at_context (GTK_ACCESSIBLE (button));
  if (context == NULL)
    return;

  was_realized = gtk_at_context_is_realized (context);

  gtk_at_context_unrealize (context);

  switch (button->role)
    {
    default:
    case GTK_BUTTON_ROLE_NORMAL:
    case GTK_BUTTON_ROLE_TITLE:
      role = GTK_ACCESSIBLE_ROLE_MENU_ITEM;
      break;
    case GTK_BUTTON_ROLE_CHECK:
      role = GTK_ACCESSIBLE_ROLE_MENU_ITEM_CHECKBOX;
      break;
    case GTK_BUTTON_ROLE_RADIO:
      role = GTK_ACCESSIBLE_ROLE_MENU_ITEM_RADIO;
      break;
    }

  gtk_at_context_set_accessible_role (context, role);

  if (was_realized)
    gtk_at_context_realize (context);

  g_object_unref (context);
}

static void
update_node_ordering (GtkModelButton *button)
{
  GtkWidget *child;

  if (gtk_widget_get_direction (GTK_WIDGET (button)) == GTK_TEXT_DIR_LTR)
    {
      if (button->start_indicator)
        {
          gtk_widget_add_css_class (button->start_indicator, "left");
          gtk_widget_remove_css_class (button->start_indicator, "right");
        }

      if (button->end_indicator)
        {
          gtk_widget_add_css_class (button->end_indicator, "right");
          gtk_widget_remove_css_class (button->end_indicator, "left");
        }

      child = gtk_widget_get_first_child (GTK_WIDGET (button));
     if (button->start_indicator && child != button->start_box)
        gtk_widget_insert_before (button->start_box, GTK_WIDGET (button), child);

      child = gtk_widget_get_last_child (GTK_WIDGET (button));
      if (button->end_indicator && child != button->end_indicator)
        gtk_widget_insert_after (button->end_indicator, GTK_WIDGET (button), child);
    }
  else
    {
      if (button->start_indicator)
        {
          gtk_widget_add_css_class (button->start_indicator, "right");
          gtk_widget_remove_css_class (button->start_indicator, "left");
        }

      if (button->end_indicator)
        {
          gtk_widget_add_css_class (button->end_indicator, "left");
          gtk_widget_remove_css_class (button->end_indicator, "right");

        }

      child = gtk_widget_get_first_child (GTK_WIDGET (button));
      if (button->end_indicator && child != button->end_indicator)
        gtk_widget_insert_before (button->end_indicator, GTK_WIDGET (button), child);

      child = gtk_widget_get_last_child (GTK_WIDGET (button));
      if (button->end_indicator && child != button->end_indicator)
        gtk_widget_insert_after (button->end_indicator, GTK_WIDGET (button), child);
    }
}

static void
update_end_indicator (GtkModelButton *self)
{
  const gboolean is_ltr = gtk_widget_get_direction (GTK_WIDGET (self)) == GTK_TEXT_DIR_LTR;

  if (!self->end_indicator)
    return;

  if (is_ltr)
    {
      gtk_widget_add_css_class (self->end_indicator, "right");
      gtk_widget_remove_css_class (self->end_indicator, "left");
    }
  else
    {
      gtk_widget_add_css_class (self->end_indicator, "left");
      gtk_widget_remove_css_class (self->end_indicator, "right");
    }
}

static GtkStateFlags
get_start_indicator_state (GtkModelButton *self)
{
  GtkStateFlags state = gtk_widget_get_state_flags (GTK_WIDGET (self));

  if (self->role == GTK_BUTTON_ROLE_CHECK ||
      self->role == GTK_BUTTON_ROLE_RADIO)
    {
      if (self->active)
        state |= GTK_STATE_FLAG_CHECKED;
      else
        state &= ~GTK_STATE_FLAG_CHECKED;
    }

  return state;
}

static void
update_start_indicator (GtkModelButton *self)
{
  const gboolean is_ltr = gtk_widget_get_direction (GTK_WIDGET (self)) == GTK_TEXT_DIR_LTR;

  if (!self->start_indicator)
    return;

  gtk_widget_set_state_flags (self->start_indicator, get_start_indicator_state (self), TRUE);

  if (is_ltr)
    {
      gtk_widget_add_css_class (self->start_indicator, "left");
      gtk_widget_remove_css_class (self->start_indicator, "right");
    }
  else
    {
      gtk_widget_add_css_class (self->start_indicator, "right");
      gtk_widget_remove_css_class (self->start_indicator, "left");
    }

}

static void
gtk_model_button_update_state (GtkModelButton *self)
{
  GtkStateFlags indicator_state;

  update_start_indicator (self);
  update_end_indicator (self);

  indicator_state = get_start_indicator_state (self);
  if (self->iconic)
    gtk_widget_set_state_flags (GTK_WIDGET (self), indicator_state, TRUE);
}

static void
gtk_model_button_state_flags_changed (GtkWidget     *widget,
                                      GtkStateFlags  previous_flags)
{
  gtk_model_button_update_state (GTK_MODEL_BUTTON (widget));

  GTK_WIDGET_CLASS (gtk_model_button_parent_class)->state_flags_changed (widget, previous_flags);
}

static void
gtk_model_button_direction_changed (GtkWidget        *widget,
                                    GtkTextDirection  previous_dir)
{
  GtkModelButton *button = GTK_MODEL_BUTTON (widget);

  gtk_model_button_update_state (button);
  update_node_ordering (button);

  GTK_WIDGET_CLASS (gtk_model_button_parent_class)->direction_changed (widget, previous_dir);
}

static void
update_node_name (GtkModelButton *self)
{
  const char *start_name;
  const char *end_name;

  switch (self->role)
    {
    case GTK_BUTTON_ROLE_TITLE:
      start_name = "arrow";
      end_name = "";
      break;
    case GTK_BUTTON_ROLE_NORMAL:
      start_name = NULL;
      if (self->menu_name || self->popover)
        end_name = "arrow";
      else
        end_name = NULL;
      break;

    case GTK_BUTTON_ROLE_CHECK:
      start_name = "check";
      end_name = NULL;
      break;

    case GTK_BUTTON_ROLE_RADIO:
      start_name = "radio";
      end_name = NULL;
      break;

    default:
      g_assert_not_reached ();
    }

  if (self->iconic)
    {
      start_name = NULL;
      end_name = NULL;
    }

  if (start_name && !self->start_indicator)
    {
      self->start_indicator = gtk_builtin_icon_new (start_name);
      gtk_widget_set_halign (self->start_indicator, GTK_ALIGN_CENTER);
      gtk_widget_set_valign (self->start_indicator, GTK_ALIGN_CENTER);
      update_start_indicator (self);

      gtk_box_append (GTK_BOX (self->start_box), self->start_indicator);
    }
  else if (start_name)
    {
      gtk_css_node_set_name (gtk_widget_get_css_node (self->start_indicator), g_quark_from_static_string (start_name));
    }
  else if (self->start_indicator)
    {
      gtk_box_remove (GTK_BOX (self->start_box), self->start_indicator);
      self->start_indicator = NULL;
    }

  if (end_name && !self->end_indicator)
    {
      self->end_indicator = gtk_builtin_icon_new (end_name);
      gtk_widget_set_halign (self->end_indicator, GTK_ALIGN_CENTER);
      gtk_widget_set_valign (self->end_indicator, GTK_ALIGN_CENTER);
      gtk_widget_set_parent (self->end_indicator, GTK_WIDGET (self));
      update_end_indicator (self);
    }
  else if (end_name)
    {
      gtk_css_node_set_name (gtk_widget_get_css_node (self->end_indicator), g_quark_from_static_string (end_name));
    }
  else
    {
      g_clear_pointer (&self->end_indicator, gtk_widget_unparent);
    }
}

static void
update_accessible_properties (GtkModelButton *button)
{
  if (button->menu_name || button->popover)
    {
      gtk_accessible_update_state (GTK_ACCESSIBLE (button),
                                  GTK_ACCESSIBLE_STATE_EXPANDED, FALSE,
                                  -1);
      gtk_accessible_update_property (GTK_ACCESSIBLE (button),
                                      GTK_ACCESSIBLE_PROPERTY_HAS_POPUP, TRUE,
                                      -1);
    }
  else
    {
      gtk_accessible_reset_property (GTK_ACCESSIBLE (button),
                                     GTK_ACCESSIBLE_PROPERTY_HAS_POPUP);
      gtk_accessible_reset_state (GTK_ACCESSIBLE (button),
                                  GTK_ACCESSIBLE_STATE_EXPANDED);
    }

  if (button->popover)
    gtk_accessible_update_relation (GTK_ACCESSIBLE (button),
                                    GTK_ACCESSIBLE_RELATION_CONTROLS, button->popover, NULL,
                                    -1);
  else
    gtk_accessible_reset_relation (GTK_ACCESSIBLE (button),
                                   GTK_ACCESSIBLE_RELATION_CONTROLS);

  if (button->role == GTK_BUTTON_ROLE_CHECK ||
      button->role == GTK_BUTTON_ROLE_RADIO)
    gtk_accessible_update_state (GTK_ACCESSIBLE (button),
                                 GTK_ACCESSIBLE_STATE_CHECKED, button->active,
                                 -1);
  else
    gtk_accessible_reset_state (GTK_ACCESSIBLE (button),
                                GTK_ACCESSIBLE_STATE_CHECKED);

  gtk_accessible_update_relation (GTK_ACCESSIBLE (button),
                                  GTK_ACCESSIBLE_RELATION_LABELLED_BY, button->label, NULL,
                                  -1);

  if (button->accel_label)
    {
      const char *text = gtk_label_get_label (GTK_LABEL (button->accel_label));
      gtk_accessible_update_property (GTK_ACCESSIBLE (button),
                                      GTK_ACCESSIBLE_PROPERTY_KEY_SHORTCUTS, text,
                                      -1);
    }
  else
    gtk_accessible_reset_property (GTK_ACCESSIBLE (button),
                                   GTK_ACCESSIBLE_PROPERTY_KEY_SHORTCUTS);
}

static void
gtk_model_button_set_role (GtkModelButton *self,
                           GtkButtonRole   role)
{
  if (role == self->role)
    return;

  self->role = role;

  if (role == GTK_BUTTON_ROLE_TITLE)
    {
      gtk_widget_add_css_class (GTK_WIDGET (self), "title");
      gtk_widget_set_halign (self->label, GTK_ALIGN_CENTER);
    }
  else
    {
      gtk_widget_remove_css_class (GTK_WIDGET (self), "title");
      gtk_widget_set_halign (self->label, GTK_ALIGN_START);
    }

  update_node_name (self);
  gtk_model_button_update_state (self);

  update_at_context (self);
  update_accessible_properties (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ROLE]);
}

static void
update_visibility (GtkModelButton *self)
{
  gboolean has_icon;
  gboolean has_text;

  has_icon = self->image && gtk_image_get_storage_type (GTK_IMAGE (self->image)) != GTK_IMAGE_EMPTY;
  has_text = gtk_label_get_text (GTK_LABEL (self->label))[0] != '\0';

  gtk_widget_set_visible (self->label, has_text && (!self->iconic || !has_icon));
  gtk_widget_set_hexpand (self->label,
                          gtk_widget_get_visible (self->label) && !has_icon);

  if (self->accel_label)
    gtk_widget_set_visible (self->accel_label, has_text && (!self->iconic || !has_icon));

  if (self->image)
    {
      gtk_widget_set_visible (self->image, has_icon && (self->iconic || !has_text));
      gtk_widget_set_hexpand (self->image,
                              has_icon && (!has_text || !gtk_widget_get_visible (self->label)));
    }
}

static void
update_tooltip (GtkModelButton *self)
{
  if (self->iconic)
    {
      if (gtk_label_get_use_markup (GTK_LABEL (self->label)))
        gtk_widget_set_tooltip_markup (GTK_WIDGET (self), gtk_label_get_text (GTK_LABEL (self->label)));
      else
        gtk_widget_set_tooltip_text (GTK_WIDGET (self), gtk_label_get_text (GTK_LABEL (self->label)));
    }
  else
    {
      gtk_widget_set_tooltip_text (GTK_WIDGET (self), NULL);
    }
}

static void
gtk_model_button_set_icon (GtkModelButton *self,
                           GIcon          *icon)
{
  if (!self->image && icon)
    {
      self->image = g_object_new (GTK_TYPE_IMAGE,
                                  "accessible-role", GTK_ACCESSIBLE_ROLE_PRESENTATION,
                                  "gicon", icon,
                                  NULL);
      gtk_widget_insert_before (self->image, GTK_WIDGET (self), self->label);
    }
  else if (self->image && !icon)
    {
      g_clear_pointer (&self->image, gtk_widget_unparent);
    }
  else if (icon)
    {
      gtk_image_set_from_gicon (GTK_IMAGE (self->image), icon);
    }

  update_visibility (self);
  update_tooltip (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ICON]);
}

static void
gtk_model_button_set_text (GtkModelButton *button,
                           const char     *text)
{
  gtk_label_set_text_with_mnemonic (GTK_LABEL (button->label), text ? text : "");
  update_visibility (button);
  update_tooltip (button);

  gtk_accessible_update_relation (GTK_ACCESSIBLE (button),
                                  GTK_ACCESSIBLE_RELATION_LABELLED_BY, button->label, NULL,
                                  -1);

  g_object_notify_by_pspec (G_OBJECT (button), properties[PROP_TEXT]);
}

static void
gtk_model_button_set_use_markup (GtkModelButton *button,
                                 gboolean        use_markup)
{
  use_markup = !!use_markup;
  if (gtk_label_get_use_markup (GTK_LABEL (button->label)) == use_markup)
    return;

  gtk_label_set_use_markup (GTK_LABEL (button->label), use_markup);
  update_visibility (button);
  update_tooltip (button);

  g_object_notify_by_pspec (G_OBJECT (button), properties[PROP_USE_MARKUP]);
}

static void
gtk_model_button_set_active (GtkModelButton *button,
                             gboolean        active)
{
  active = !!active;
  if (button->active == active)
    return;

  button->active = active;

  update_accessible_properties (button);

  gtk_model_button_update_state (button);
  gtk_widget_queue_draw (GTK_WIDGET (button));
  g_object_notify_by_pspec (G_OBJECT (button), properties[PROP_ACTIVE]);
}

static void
gtk_model_button_set_menu_name (GtkModelButton *button,
                                const char     *menu_name)
{
  g_free (button->menu_name);
  button->menu_name = g_strdup (menu_name);

  update_node_name (button);
  gtk_model_button_update_state (button);

  update_accessible_properties (button);

  gtk_widget_queue_resize (GTK_WIDGET (button));
  g_object_notify_by_pspec (G_OBJECT (button), properties[PROP_MENU_NAME]);
}

static void
gtk_model_button_set_iconic (GtkModelButton *self,
                             gboolean        iconic)
{
  GtkWidget *widget = GTK_WIDGET (self);
  GtkCssNode *widget_node;

  iconic = !!iconic;
  if (self->iconic == iconic)
    return;

  self->iconic = iconic;

  widget_node = gtk_widget_get_css_node (widget);
  gtk_widget_set_visible (self->start_box, !iconic);
  if (iconic)
    {
      gtk_css_node_set_name (widget_node, g_quark_from_static_string ("button"));
      gtk_widget_add_css_class (widget, "model");
      gtk_widget_add_css_class (widget, "image-button");
      gtk_widget_remove_css_class (widget, "flat");
    }
  else
    {
      gtk_css_node_set_name (widget_node, g_quark_from_static_string ("modelbutton"));
      gtk_widget_remove_css_class (widget, "model");
      gtk_widget_remove_css_class (widget, "image-button");
      gtk_widget_add_css_class (widget, "flat");
    }

  if (!iconic)
    {
      if (self->start_indicator)
        {
          gtk_box_remove (GTK_BOX (self->start_box), self->start_indicator);
          self->start_indicator = NULL;
        }
      g_clear_pointer (&self->end_indicator, gtk_widget_unparent);
    }

  update_node_name (self);
  update_visibility (self);
  update_tooltip (self);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ICONIC]);
}

static void
gtk_model_button_set_popover (GtkModelButton *button,
                              GtkWidget      *popover)
{
  if (button->popover)
    gtk_widget_unparent (button->popover);

  button->popover = popover;

  if (button->popover)
    {
      gtk_widget_set_parent (button->popover, GTK_WIDGET (button));
      gtk_popover_set_position (GTK_POPOVER (button->popover), GTK_POS_RIGHT);
    }

  update_accessible_properties (button);

  update_node_name (button);
  gtk_model_button_update_state (button);

  gtk_widget_queue_resize (GTK_WIDGET (button));
  g_object_notify_by_pspec (G_OBJECT (button), properties[PROP_POPOVER]);
}

static void
update_accel (GtkModelButton *self,
              const char     *accel)
{
  if (accel)
    {
      guint key;
      GdkModifierType mods;
      char *str;

      if (!self->accel_label)
        {
          self->accel_label = g_object_new (GTK_TYPE_LABEL,
                                            "accessible-role", GTK_ACCESSIBLE_ROLE_PRESENTATION,
                                            "css-name", "accelerator",
                                            NULL);
          gtk_widget_insert_before (self->accel_label, GTK_WIDGET (self), NULL);
          gtk_widget_set_hexpand (self->accel_label, TRUE),
          gtk_widget_set_halign (self->accel_label, GTK_ALIGN_END);
        }

      gtk_accelerator_parse (accel, &key, &mods);
      str = gtk_accelerator_get_label (key, mods);
      gtk_label_set_label (GTK_LABEL (self->accel_label), str);
      g_free (str);

      if (GTK_IS_POPOVER (gtk_widget_get_native (GTK_WIDGET (self))))
        {
          GtkShortcutTrigger *trigger;
          GtkShortcutAction *action;

          if (self->controller)
            {
              while (g_list_model_get_n_items (G_LIST_MODEL (self->controller)) > 0)
                {
                  GtkShortcut *shortcut = g_list_model_get_item (G_LIST_MODEL (self->controller), 0);
                  gtk_shortcut_controller_remove_shortcut (GTK_SHORTCUT_CONTROLLER (self->controller),
                                                           shortcut);
                  g_object_unref (shortcut);
                }
            }
          else
            {
              self->controller = gtk_shortcut_controller_new ();
              gtk_shortcut_controller_set_scope (GTK_SHORTCUT_CONTROLLER (self->controller), GTK_SHORTCUT_SCOPE_MANAGED);
              gtk_widget_add_controller (GTK_WIDGET (self), self->controller);
            }

          trigger = gtk_keyval_trigger_new (key, mods);
          action = gtk_signal_action_new ("clicked");
          gtk_shortcut_controller_add_shortcut (GTK_SHORTCUT_CONTROLLER (self->controller),
                                                gtk_shortcut_new (trigger, action));
        }
    }
  else
    {
      g_clear_pointer (&self->accel_label, gtk_widget_unparent);
      if (self->controller)
        {
          gtk_widget_remove_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (self->controller));
          self->controller = NULL;
        }
    }

  update_accessible_properties (self);
}

static void
gtk_model_button_set_accel (GtkModelButton *button,
                            const char     *accel)
{
  g_free (button->accel);
  button->accel = g_strdup (accel);
  update_accel (button, button->accel);
  update_visibility (button);

  g_object_notify_by_pspec (G_OBJECT (button), properties[PROP_ACCEL]);
}

static void
gtk_model_button_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GtkModelButton *self = GTK_MODEL_BUTTON (object);

  switch (prop_id)
    {
    case PROP_ROLE:
      g_value_set_enum (value, self->role);
      break;

    case PROP_ICON:
      g_value_set_object (value, self->image ? gtk_image_get_gicon (GTK_IMAGE (self->image)) : NULL);
      break;

    case PROP_TEXT:
      g_value_set_string (value, gtk_label_get_text (GTK_LABEL (self->label)));
      break;

    case PROP_USE_MARKUP:
      g_value_set_boolean (value, gtk_label_get_use_markup (GTK_LABEL (self->label)));
      break;

    case PROP_ACTIVE:
      g_value_set_boolean (value, self->active);
      break;

    case PROP_MENU_NAME:
      g_value_set_string (value, self->menu_name);
      break;

    case PROP_POPOVER:
      g_value_set_object (value, self->popover);
      break;

    case PROP_ICONIC:
      g_value_set_boolean (value, self->iconic);
      break;

    case PROP_ACCEL:
      g_value_set_string (value, self->accel);
      break;

    case PROP_INDICATOR_SIZE_GROUP:
      g_value_set_object (value, self->indicators);
      break;

    case PROP_ACTION_NAME:
      g_value_set_string (value, gtk_action_helper_get_action_name (self->action_helper));
      break;

    case PROP_ACTION_TARGET:
      g_value_set_variant (value, gtk_action_helper_get_action_target_value (self->action_helper));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_model_button_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GtkModelButton *button = GTK_MODEL_BUTTON (object);

  switch (prop_id)
    {
    case PROP_ROLE:
      gtk_model_button_set_role (button, g_value_get_enum (value));
      break;

    case PROP_ICON:
      gtk_model_button_set_icon (button, g_value_get_object (value));
      break;

    case PROP_TEXT:
      gtk_model_button_set_text (button, g_value_get_string (value));
      break;

    case PROP_USE_MARKUP:
      gtk_model_button_set_use_markup (button, g_value_get_boolean (value));
      break;

    case PROP_ACTIVE:
      gtk_model_button_set_active (button, g_value_get_boolean (value));
      break;

    case PROP_MENU_NAME:
      gtk_model_button_set_menu_name (button, g_value_get_string (value));
      break;

    case PROP_POPOVER:
      gtk_model_button_set_popover (button, (GtkWidget *)g_value_get_object (value));
      break;

    case PROP_ICONIC:
      gtk_model_button_set_iconic (button, g_value_get_boolean (value));
      break;

    case PROP_ACCEL:
      gtk_model_button_set_accel (button, g_value_get_string (value));
      break;

    case PROP_INDICATOR_SIZE_GROUP:
      if (button->indicators)
        gtk_size_group_remove_widget (button->indicators, button->start_box);
      button->indicators = GTK_SIZE_GROUP (g_value_get_object (value));
      if (button->indicators)
        gtk_size_group_add_widget (button->indicators, button->start_box);
      break;

    case PROP_ACTION_NAME:
      gtk_model_button_set_action_name (GTK_ACTIONABLE (button), g_value_get_string (value));
      break;

    case PROP_ACTION_TARGET:
      gtk_model_button_set_action_target_value (GTK_ACTIONABLE (button), g_value_get_variant (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_model_button_dispose (GObject *object)
{
  GtkModelButton *model_button = GTK_MODEL_BUTTON (object);

  g_clear_pointer (&model_button->menu_name, g_free);

  G_OBJECT_CLASS (gtk_model_button_parent_class)->dispose (object);
}

static void
switch_menu (GtkModelButton *button)
{
  GtkWidget *stack;

  stack = gtk_widget_get_ancestor (GTK_WIDGET (button), GTK_TYPE_STACK);
  if (stack != NULL)
    {
      gtk_stack_set_visible_child_name (GTK_STACK (stack), button->menu_name);
      gtk_accessible_update_state (GTK_ACCESSIBLE (button),
                                   GTK_ACCESSIBLE_STATE_EXPANDED, TRUE,
                                   -1);
    }
}

static void
gtk_model_button_clicked (GtkModelButton *self)
{
  if (self->menu_name != NULL)
    {
      switch_menu (self);
    }
  else if (self->popover != NULL)
    {
      GtkPopoverMenu *menu;
      GtkWidget *submenu;

      menu = (GtkPopoverMenu *)gtk_widget_get_ancestor (GTK_WIDGET (self), GTK_TYPE_POPOVER_MENU);
      submenu = self->popover;
      gtk_popover_popup (GTK_POPOVER (submenu));
      gtk_popover_menu_set_open_submenu (menu, submenu);
      gtk_popover_menu_set_parent_menu (GTK_POPOVER_MENU (submenu), GTK_WIDGET (menu));

      gtk_accessible_update_state (GTK_ACCESSIBLE (self),
                                   GTK_ACCESSIBLE_STATE_EXPANDED, TRUE,
                                   -1);
    }
  else if (!self->keep_open)
    {
      GtkWidget *popover;

      popover = gtk_widget_get_ancestor (GTK_WIDGET (self), GTK_TYPE_POPOVER);
      if (popover)
        gtk_popover_popdown (GTK_POPOVER (popover));

      gtk_accessible_update_state (GTK_ACCESSIBLE (self),
                                   GTK_ACCESSIBLE_STATE_EXPANDED, FALSE,
                                   -1);
    }

  if (self->action_helper)
    gtk_action_helper_activate (self->action_helper);
}

static gboolean
toggle_cb (GtkWidget *widget,
           GVariant  *args,
           gpointer   user_data)
{
  GtkModelButton *self = GTK_MODEL_BUTTON (widget);

  self->keep_open = self->role != GTK_BUTTON_ROLE_NORMAL;
  g_signal_emit (widget, signals[SIGNAL_CLICKED], 0);
  self->keep_open = FALSE;

  return TRUE;
}

static void
gtk_model_button_finalize (GObject *object)
{
  GtkModelButton *button = GTK_MODEL_BUTTON (object);

  g_clear_pointer (&button->image, gtk_widget_unparent);
  g_clear_pointer (&button->label, gtk_widget_unparent);
  g_clear_pointer (&button->start_box, gtk_widget_unparent);
  g_clear_pointer (&button->accel_label, gtk_widget_unparent);
  g_clear_pointer (&button->end_indicator, gtk_widget_unparent);
  g_clear_object (&button->action_helper);
  g_free (button->accel);
  g_clear_pointer (&button->popover, gtk_widget_unparent);

  if (button->open_timeout)
    g_source_remove (button->open_timeout);

  G_OBJECT_CLASS (gtk_model_button_parent_class)->finalize (object);
}

static gboolean
gtk_model_button_focus (GtkWidget        *widget,
                        GtkDirectionType  direction)
{
  GtkModelButton *button = GTK_MODEL_BUTTON (widget);

  if (gtk_widget_is_focus (widget))
    {
      if (direction == GTK_DIR_LEFT &&
          button->role == GTK_BUTTON_ROLE_TITLE &&
          button->menu_name != NULL)
        {
          switch_menu (button);
          return TRUE;
        }
      else if (direction == GTK_DIR_RIGHT &&
               button->role == GTK_BUTTON_ROLE_NORMAL &&
               button->menu_name != NULL)
        {
          switch_menu (button);
          return TRUE;
        }
      else if (direction == GTK_DIR_RIGHT &&
               button->role == GTK_BUTTON_ROLE_NORMAL &&
               button->popover != NULL)
        {
          GtkPopoverMenu *menu;
          GtkWidget *submenu;

          menu = GTK_POPOVER_MENU (gtk_widget_get_ancestor (GTK_WIDGET (button), GTK_TYPE_POPOVER_MENU));
          submenu = button->popover;
          gtk_popover_popup (GTK_POPOVER (submenu));
          gtk_popover_menu_set_open_submenu (menu, submenu);
          gtk_popover_menu_set_parent_menu (GTK_POPOVER_MENU (submenu), GTK_WIDGET (menu));
          return TRUE;
        }
    }
  else
    {
      gtk_widget_grab_focus (widget);
      return TRUE;
    }

  return FALSE;
}

static void
gtk_model_button_class_init (GtkModelButtonClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  GtkShortcutAction *action;
  guint activate_keyvals[] = {
    GDK_KEY_Return, GDK_KEY_ISO_Enter, GDK_KEY_KP_Enter
  };
  guint toggle_keyvals[] = {
    GDK_KEY_space, GDK_KEY_KP_Space
  };
  int i;

  object_class->dispose = gtk_model_button_dispose;
  object_class->finalize = gtk_model_button_finalize;
  object_class->get_property = gtk_model_button_get_property;
  object_class->set_property = gtk_model_button_set_property;

  widget_class->state_flags_changed = gtk_model_button_state_flags_changed;
  widget_class->direction_changed = gtk_model_button_direction_changed;
  widget_class->focus = gtk_model_button_focus;

  class->clicked = gtk_model_button_clicked;

  /**
   * GtkModelButton:role:
   *
   * Specifies whether the button is a plain, check or radio button.
   * When GtkActionable:action-name is set, the role will be determined
   * from the action and does not have to be set explicitly.
   */
  properties[PROP_ROLE] =
    g_param_spec_enum ("role", NULL, NULL,
                       GTK_TYPE_BUTTON_ROLE,
                       GTK_BUTTON_ROLE_NORMAL,
                       G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkModelButton:icon:
   *
   * A GIcon that will be used if iconic appearance for the button is
   * desired.
   */
  properties[PROP_ICON] =
    g_param_spec_object ("icon", NULL, NULL,
                         G_TYPE_ICON,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkModelButton:text:
   *
   * The label for the button.
   */
  properties[PROP_TEXT] =
    g_param_spec_string ("text", NULL, NULL,
                         "",
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkModelButton:use-markup:
   *
   * If %TRUE, XML tags in the text of the button are interpreted as by
   * pango_parse_markup() to format the enclosed spans of text. If %FALSE, the
   * text will be displayed verbatim.
   */
  properties[PROP_USE_MARKUP] =
    g_param_spec_boolean ("use-markup", NULL, NULL,
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkModelButton:active:
   *
   * The state of the button. This is reflecting the state of the associated
   * GAction.
   */
  properties[PROP_ACTIVE] =
    g_param_spec_boolean ("active", NULL, NULL,
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkModelButton:menu-name:
   *
   * The name of a submenu to open when the button is activated.  * If this is set, the button should not have an action associated with it.
   */
  properties[PROP_MENU_NAME] =
    g_param_spec_string ("menu-name", NULL, NULL,
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

 properties[PROP_POPOVER] =
   g_param_spec_object ("popover", NULL, NULL,
                        GTK_TYPE_POPOVER,
                        G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkModelButton:iconic:
   *
   * If this property is set, the button will show an icon if one is set.
   * If no icon is set, the text will be used. This is typically used for
   * horizontal sections of linked buttons.
   */
  properties[PROP_ICONIC] =
    g_param_spec_boolean ("iconic", NULL, NULL,
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkModelButton:indicator-size-group:
   *
   * Containers like GtkPopoverMenu can provide a size group
   * in this property to align the checks and radios of all
   * the model buttons in a menu.
   */
  properties[PROP_INDICATOR_SIZE_GROUP] =
    g_param_spec_object ("indicator-size-group", NULL, NULL,
                          GTK_TYPE_SIZE_GROUP,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_ACCEL] =
    g_param_spec_string ("accel", NULL, NULL,
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_properties (object_class, LAST_PROP, properties);

  g_object_class_override_property (object_class, PROP_ACTION_NAME, "action-name");
  g_object_class_override_property (object_class, PROP_ACTION_TARGET, "action-target");

  signals[SIGNAL_CLICKED] = g_signal_new (I_("clicked"),
                                          G_OBJECT_CLASS_TYPE (object_class),
                                          G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                                          G_STRUCT_OFFSET (GtkModelButtonClass, clicked),
                                          NULL, NULL,
                                          NULL,
                                          G_TYPE_NONE, 0);

  gtk_widget_class_set_activate_signal (widget_class, signals[SIGNAL_CLICKED]);
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BOX_LAYOUT);
  gtk_widget_class_set_css_name (widget_class, I_("modelbutton"));
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_MENU_ITEM);

  action = gtk_signal_action_new ("clicked");
  for (i = 0; i < G_N_ELEMENTS (activate_keyvals); i++)
    {
      GtkShortcut *shortcut;

      shortcut = gtk_shortcut_new (gtk_keyval_trigger_new (activate_keyvals[i], 0),
                                   g_object_ref (action));
      gtk_widget_class_add_shortcut (widget_class, shortcut);
      g_object_unref (shortcut);
    }
  g_object_unref (action);

  action = gtk_callback_action_new (toggle_cb, NULL, NULL);
  for (i = 0; i < G_N_ELEMENTS (toggle_keyvals); i++)
    {
      GtkShortcut *shortcut;

      shortcut = gtk_shortcut_new (gtk_keyval_trigger_new (toggle_keyvals[i], 0),
                                   g_object_ref (action));
      gtk_widget_class_add_shortcut (widget_class, shortcut);
      g_object_unref (shortcut);
    }
  g_object_unref (action);
}

static gboolean
open_submenu (gpointer data)
{
  GtkModelButton *button = data;
  GtkPopover *popover;

  popover = (GtkPopover*)gtk_widget_get_ancestor (GTK_WIDGET (button), GTK_TYPE_POPOVER);

  if (GTK_IS_POPOVER_MENU (popover))
    {
      gtk_popover_menu_set_active_item (GTK_POPOVER_MENU (popover), GTK_WIDGET (button));

      if (button->popover)
        {
          GtkWidget *submenu = button->popover;

          if (gtk_popover_menu_get_open_submenu (GTK_POPOVER_MENU (popover)) != submenu)
            gtk_popover_menu_close_submenus (GTK_POPOVER_MENU (popover));

          gtk_popover_popup (GTK_POPOVER (submenu));
          gtk_popover_menu_set_open_submenu (GTK_POPOVER_MENU (popover), submenu);
          gtk_popover_menu_set_parent_menu (GTK_POPOVER_MENU (submenu), GTK_WIDGET (popover));
        }
    }

  button->open_timeout = 0;

  return G_SOURCE_REMOVE;
}

#define OPEN_TIMEOUT 80

static void
start_open (GtkModelButton *button)
{
  if (button->open_timeout)
    g_source_remove (button->open_timeout);

  if (button->popover &&
      gtk_widget_get_visible (button->popover))
    return;

  button->open_timeout = g_timeout_add (OPEN_TIMEOUT, open_submenu, button);
  gdk_source_set_static_name_by_id (button->open_timeout, "[gtk] open_submenu");
}

static void
stop_open (GtkModelButton *button)
{
  if (button->open_timeout)
    {
      g_source_remove (button->open_timeout);
      button->open_timeout = 0;
    }
}

static void
pointer_cb (GObject    *object,
            GParamSpec *pspec,
            gpointer    data)
{
  GtkWidget *target = GTK_WIDGET (data);
  GtkWidget *popover;
  gboolean contains;

  contains = gtk_event_controller_motion_contains_pointer (GTK_EVENT_CONTROLLER_MOTION (object));

  popover = gtk_widget_get_ancestor (target, GTK_TYPE_POPOVER_MENU);

  if (contains)
    {
      if (popover)
        {
          if (gtk_popover_menu_get_open_submenu (GTK_POPOVER_MENU (popover)) != NULL)
            start_open (GTK_MODEL_BUTTON (target));
          else
            open_submenu (target);
        }
    }
  else
    {
      GtkModelButton *button = data;

      stop_open (button);
      if (popover)
        gtk_popover_menu_set_active_item (GTK_POPOVER_MENU (popover), NULL);
    }
}

static void
motion_cb (GtkEventController *controller,
           double              x,
           double              y,
           gpointer            data)
{
  start_open (GTK_MODEL_BUTTON (data));
}

static void
focus_in_cb (GtkEventController   *controller,
             gpointer              data)
{
  GtkWidget *target;
  GtkWidget *popover;

  target = gtk_event_controller_get_widget (controller);
  popover = gtk_widget_get_ancestor (target, GTK_TYPE_POPOVER_MENU);

  if (popover)
    gtk_popover_menu_set_active_item (GTK_POPOVER_MENU (popover), target);
}

static void
gesture_pressed (GtkGestureClick *gesture,
                 guint            n_press,
                 double           x,
                 double           y,
                 GtkWidget       *widget)
{
  if (gtk_widget_get_focus_on_click (widget) && !gtk_widget_has_focus (widget))
    gtk_widget_grab_focus (widget);

  gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
}

static void
gesture_released (GtkGestureClick *gesture,
                  guint            n_press,
                  double           x,
                  double           y,
                  GtkWidget       *widget)
{
  if (gtk_widget_contains (widget, x, y))
    g_signal_emit (widget, signals[SIGNAL_CLICKED], 0);
}

static void
gesture_unpaired_release (GtkGestureClick  *gesture,
                          double            x,
                          double            y,
                          guint             button,
                          GdkEventSequence *sequence,
                          GtkWidget        *widget)
{
  if (gtk_widget_contains (widget, x, y))
    g_signal_emit (widget, signals[SIGNAL_CLICKED], 0);
}

static void
gtk_model_button_init (GtkModelButton *self)
{
  GtkEventController *controller;
  GtkGesture *gesture;

  gtk_widget_set_focusable (GTK_WIDGET (self), TRUE);

  self->role = GTK_BUTTON_ROLE_NORMAL;
  self->label = g_object_new (GTK_TYPE_LABEL, "accessible-role", GTK_ACCESSIBLE_ROLE_PRESENTATION, NULL);
  gtk_widget_set_halign (self->label, GTK_ALIGN_START);
  gtk_widget_set_parent (self->label, GTK_WIDGET (self));

  self->start_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_insert_after (self->start_box, GTK_WIDGET (self), NULL);
  update_node_ordering (self);

  gtk_widget_add_css_class (GTK_WIDGET (self), "flat");

  controller = gtk_event_controller_motion_new ();
  g_signal_connect (controller, "notify::contains-pointer", G_CALLBACK (pointer_cb), self);
  g_signal_connect (controller, "motion", G_CALLBACK (motion_cb), self);
  gtk_widget_add_controller (GTK_WIDGET (self), controller);

  controller = gtk_event_controller_focus_new ();
  gtk_event_controller_set_propagation_limit (controller, GTK_LIMIT_NONE);
  g_signal_connect (controller, "enter", G_CALLBACK (focus_in_cb), NULL);
  gtk_widget_add_controller (GTK_WIDGET (self), controller);

  gesture = gtk_gesture_click_new ();
  gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (gesture), FALSE);
  gtk_gesture_single_set_exclusive (GTK_GESTURE_SINGLE (gesture), TRUE);
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), GDK_BUTTON_PRIMARY);
  g_signal_connect (gesture, "pressed", G_CALLBACK (gesture_pressed), self);
  g_signal_connect (gesture, "released", G_CALLBACK (gesture_released), self);
  g_signal_connect (gesture, "unpaired-release", G_CALLBACK (gesture_unpaired_release), self);
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (gesture), GTK_PHASE_CAPTURE);
  gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (gesture));
}

/**
 * gtk_model_button_new:
 *
 * Creates a new GtkModelButton.
 *
 * Returns: the newly created GtkModelButton widget
 */
GtkWidget *
gtk_model_button_new (void)
{
  return g_object_new (GTK_TYPE_MODEL_BUTTON, NULL);
}
