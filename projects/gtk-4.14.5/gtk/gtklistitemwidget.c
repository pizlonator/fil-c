/*
 * Copyright © 2018 Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include "gtklistitemwidgetprivate.h"

#include "gtkbinlayout.h"
#include "gtkeventcontrollermotion.h"
#include "gtkgestureclick.h"
#include "gtklistitemfactoryprivate.h"
#include "gtklistitemprivate.h"
#include "gtklistbaseprivate.h"
#include "gtkwidget.h"
#include "gtkwidgetprivate.h"

G_DEFINE_TYPE (GtkListItemWidget, gtk_list_item_widget, GTK_TYPE_LIST_FACTORY_WIDGET)

static gboolean
gtk_list_item_widget_focus (GtkWidget        *widget,
                            GtkDirectionType  direction)
{
  GtkWidget *child = gtk_widget_get_first_child (widget);
  GtkWidget *focus_child = gtk_widget_get_focus_child (widget);

  if (focus_child)
    {
      /* focus is in the child */
      if (gtk_widget_child_focus (focus_child, direction))
        return TRUE;
      else if (direction == GTK_DIR_TAB_BACKWARD)
        return gtk_widget_grab_focus_self (widget);
      else
        return FALSE;
    }
  else if (gtk_widget_is_focus (widget))
    {
      /* The widget has focus */
      if (direction == GTK_DIR_TAB_FORWARD)
        {
          if (child)
            return gtk_widget_child_focus (child, direction);
        }

      return FALSE;
    }
  else
    {
      /* focus coming in from the outside */
      if (direction == GTK_DIR_TAB_BACKWARD)
        {
          if (child &&
              gtk_widget_child_focus (child, direction))
            return TRUE;

          return gtk_widget_grab_focus_self (widget);
        }
      else
        {
          if (gtk_widget_grab_focus_self (widget))
            return TRUE;

          if (child &&
              gtk_widget_child_focus (child, direction))
            return TRUE;

          return FALSE;
        }
    }
}

static gboolean
gtk_list_item_widget_grab_focus (GtkWidget *widget)
{
  GtkWidget *child;

  if (GTK_WIDGET_CLASS (gtk_list_item_widget_parent_class)->grab_focus (widget))
    return TRUE;

  child = gtk_widget_get_first_child (widget);
  if (child && gtk_widget_grab_focus (child))
    return TRUE;

  return FALSE;
}

static gpointer
gtk_list_item_widget_create_object (GtkListFactoryWidget *fw)
{
  return gtk_list_item_new ();
}

static void
gtk_list_item_widget_setup_object (GtkListFactoryWidget *fw,
                                   gpointer              object)
{
  GtkListItemWidget *self = GTK_LIST_ITEM_WIDGET (fw);
  GtkListItem *list_item = object;

  GTK_LIST_FACTORY_WIDGET_CLASS (gtk_list_item_widget_parent_class)->setup_object (fw, object);

  list_item->owner = self;

  gtk_list_item_widget_set_child (self, list_item->child);

  gtk_list_factory_widget_set_activatable (fw, list_item->activatable);
  gtk_list_factory_widget_set_selectable (fw, list_item->selectable);
  gtk_widget_set_focusable (GTK_WIDGET (self), list_item->focusable);

  gtk_accessible_update_property (GTK_ACCESSIBLE (self),
                                  GTK_ACCESSIBLE_PROPERTY_LABEL, list_item->accessible_label,
                                  GTK_ACCESSIBLE_PROPERTY_DESCRIPTION, list_item->accessible_description,
                                  -1);

  gtk_list_item_do_notify (list_item,
                           gtk_list_item_base_get_item (GTK_LIST_ITEM_BASE (self)) != NULL,
                           gtk_list_item_base_get_position (GTK_LIST_ITEM_BASE (self)) != GTK_INVALID_LIST_POSITION,
                           gtk_list_item_base_get_selected (GTK_LIST_ITEM_BASE (self)));
}

static void
gtk_list_item_widget_teardown_object (GtkListFactoryWidget *fw,
                                      gpointer              object)
{
  GtkListItemWidget *self = GTK_LIST_ITEM_WIDGET (fw);
  GtkListItem *list_item = object;

  GTK_LIST_FACTORY_WIDGET_CLASS (gtk_list_item_widget_parent_class)->teardown_object (fw, object);

  list_item->owner = NULL;

  gtk_list_item_widget_set_child (self, NULL);

  gtk_list_factory_widget_set_activatable (fw, FALSE);
  gtk_list_factory_widget_set_selectable (fw, FALSE);
  gtk_widget_set_focusable (GTK_WIDGET (self), TRUE);

  gtk_accessible_reset_property (GTK_ACCESSIBLE (self), GTK_ACCESSIBLE_PROPERTY_LABEL);
  gtk_accessible_reset_property (GTK_ACCESSIBLE (self), GTK_ACCESSIBLE_PROPERTY_DESCRIPTION);

  gtk_list_item_do_notify (list_item,
                           gtk_list_item_base_get_item (GTK_LIST_ITEM_BASE (self)) != NULL,
                           gtk_list_item_base_get_position (GTK_LIST_ITEM_BASE (self)) != GTK_INVALID_LIST_POSITION,
                           gtk_list_item_base_get_selected (GTK_LIST_ITEM_BASE (self)));

  /* FIXME: This is technically not correct, the child is user code, isn't it? */
  gtk_list_item_set_child (list_item, NULL);
}

static void
gtk_list_item_widget_update_object (GtkListFactoryWidget *fw,
                                    gpointer              object,
                                    guint                 position,
                                    gpointer              item,
                                    gboolean              selected)
{
  GtkListItemWidget *self = GTK_LIST_ITEM_WIDGET (fw);
  GtkListItemBase *base = GTK_LIST_ITEM_BASE (self);
  GtkListItem *list_item = object;
  /* Track notify manually instead of freeze/thaw_notify for performance reasons. */
  gboolean notify_item = FALSE, notify_position = FALSE, notify_selected = FALSE;

  /* FIXME: It's kinda evil to notify external objects from here... */
  notify_item = gtk_list_item_base_get_item (base) != item;
  notify_position = gtk_list_item_base_get_position (base) != position;
  notify_selected = gtk_list_item_base_get_selected (base) != selected;

  GTK_LIST_FACTORY_WIDGET_CLASS (gtk_list_item_widget_parent_class)->update_object (fw,
                                                                                    object,
                                                                                    position,
                                                                                    item,
                                                                                    selected);

  if (list_item)
    gtk_list_item_do_notify (list_item, notify_item, notify_position, notify_selected);
}

static void
gtk_list_item_widget_class_init (GtkListItemWidgetClass *klass)
{
  GtkListFactoryWidgetClass *factory_class = GTK_LIST_FACTORY_WIDGET_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  factory_class->create_object = gtk_list_item_widget_create_object;
  factory_class->setup_object = gtk_list_item_widget_setup_object;
  factory_class->update_object = gtk_list_item_widget_update_object;
  factory_class->teardown_object = gtk_list_item_widget_teardown_object;

  widget_class->focus = gtk_list_item_widget_focus;
  widget_class->grab_focus = gtk_list_item_widget_grab_focus;

  /* This gets overwritten by gtk_list_item_widget_new() but better safe than sorry */
  gtk_widget_class_set_css_name (widget_class, I_("row"));
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
}

static void
gtk_list_item_widget_init (GtkListItemWidget *self)
{
  gtk_widget_set_focusable (GTK_WIDGET (self), TRUE);
}

GtkWidget *
gtk_list_item_widget_new (GtkListItemFactory *factory,
                          const char         *css_name,
                          GtkAccessibleRole   role)
{
  g_return_val_if_fail (css_name != NULL, NULL);

  return g_object_new (GTK_TYPE_LIST_ITEM_WIDGET,
                       "css-name", css_name,
                       "accessible-role", role,
                       "factory", factory,
                       NULL);
}

void
gtk_list_item_widget_set_child (GtkListItemWidget *self,
                                GtkWidget         *child)
{
  GtkWidget *cur_child = gtk_widget_get_first_child (GTK_WIDGET (self));

  if (cur_child == child)
    return;

  g_clear_pointer (&cur_child, gtk_widget_unparent);

  if (child)
    gtk_widget_set_parent (child, GTK_WIDGET (self));
}

