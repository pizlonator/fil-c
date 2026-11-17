/* cc-xkb-modifier-dialog.c
 *
 * Copyright 2019 Bastien Nocera <hadess@hadess.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <glib/gi18n.h>
#include <adwaita.h>

#include "cc-xkb-modifier-dialog.h"

struct _CcXkbModifierDialog
{
  AdwWindow       parent_instance;

  AdwPreferencesPage *xkb_modifier_page;
  GtkSwitch      *enabled_switch;
  AdwPreferencesGroup *options_group;
  AdwPreferencesGroup *switch_group;
  AdwActionRow   *switch_row;

  GSettings      *input_source_settings;
  const CcXkbModifier *modifier;
  GSList         *radio_group;
};

G_DEFINE_TYPE (CcXkbModifierDialog, cc_xkb_modifier_dialog, ADW_TYPE_WINDOW)

static const gchar *custom_css =
".xkb-option-button {"
"    padding: 12px;"
"}";

static const CcXkbOption*
get_xkb_option_from_name (const CcXkbModifier *modifier, const gchar* name)
{
  const CcXkbOption *options = modifier->options;
  int i;

  for (i = 0; options[i].label && options[i].xkb_option; i++)
    {
      if (g_str_equal (name, options[i].xkb_option))
        return &options[i];
    }

  return NULL;
}

static GtkCheckButton *
get_radio_button_from_xkb_option_name (CcXkbModifierDialog *self,
                                       const gchar         *name)
{
  gchar *xkb_option;
  GSList *l;

  for (l = self->radio_group; l != NULL; l = l->next)
    {
      xkb_option = g_object_get_data (l->data, "xkb-option");
      if (g_strcmp0 (xkb_option, name) == 0)
        return l->data;
    }

  return NULL;
}

static void
update_active_radio (CcXkbModifierDialog *self)
{
  g_auto(GStrv) options = NULL;
  GtkCheckButton *rightalt_radio;
  const CcXkbOption *default_option;
  guint i;

  options = g_settings_get_strv (self->input_source_settings, "xkb-options");

  for (i = 0; options != NULL && options[i] != NULL; i++)
    {
      GtkCheckButton *radio;

      if (!g_str_has_prefix (options[i], self->modifier->prefix))
        continue;

      radio = get_radio_button_from_xkb_option_name (self, options[i]);

      if (!radio)
        continue;

      gtk_check_button_set_active (GTK_CHECK_BUTTON (radio), TRUE);
      gtk_switch_set_active (self->enabled_switch, TRUE);
      return;
    }

  if (self->modifier->default_option != NULL)
    {
      default_option = get_xkb_option_from_name(self->modifier, self->modifier->default_option);
      rightalt_radio = get_radio_button_from_xkb_option_name (self, default_option->xkb_option);
      gtk_check_button_set_active (GTK_CHECK_BUTTON (rightalt_radio), TRUE);
      gtk_switch_set_active (self->enabled_switch, TRUE);
    }
  else
    {
      gtk_switch_set_active (self->enabled_switch, FALSE);
    }
}

static void
set_xkb_option (CcXkbModifierDialog *self,
                gchar               *xkb_option)
{
  g_autoptr(GPtrArray) array = NULL;
  g_auto(GStrv) options = NULL;
  gboolean found;
  guint i;

  /* Either replace the existing "<modifier>:" option in the string
   * array, or add the option at the end
   */
  array = g_ptr_array_new ();
  options = g_settings_get_strv (self->input_source_settings, "xkb-options");
  found = FALSE;

  for (i = 0; options != NULL && options[i] != NULL; i++)
    {
      if (g_str_has_prefix (options[i], self->modifier->prefix))
        {
          if (!found && xkb_option != NULL)
            g_ptr_array_add (array, xkb_option);
          found = TRUE;
        }
      else
        {
          g_ptr_array_add (array, options[i]);
        }
    }

  if (!found && xkb_option != NULL)
    g_ptr_array_add (array, xkb_option);

  g_ptr_array_add (array, NULL);

  g_settings_set_strv (self->input_source_settings,
                       "xkb-options",
                       (const gchar * const *) array->pdata);
}

static void
on_active_radio_changed_cb (CcXkbModifierDialog *self,
                            GtkCheckButton      *radio)
{
  gchar *xkb_option;

  if (!gtk_check_button_get_active (GTK_CHECK_BUTTON (radio)))
    return;

  if (!gtk_switch_get_state (self->enabled_switch))
    return;

  xkb_option = (gchar *)g_object_get_data (G_OBJECT (radio), "xkb-option");
  set_xkb_option (self, xkb_option);
}

static void
on_xkb_options_changed_cb (CcXkbModifierDialog *self)
{
  if (self->modifier == NULL)
    update_active_radio (self);
}

static gboolean
enable_switch_changed_cb (CcXkbModifierDialog *self,
                          gboolean             state)
{
  gchar *xkb_option;
  GSList *l;

  gtk_widget_set_sensitive (GTK_WIDGET (self->options_group), state);

  if (state)
    {
      for (l = self->radio_group; l != NULL; l = l->next)
        {
          if (gtk_check_button_get_active (l->data))
            {
              xkb_option = (gchar *)g_object_get_data (l->data, "xkb-option");
              set_xkb_option (self, xkb_option);
              break;
            }
        }
    }
  else
    {
      set_xkb_option (self, NULL);
    }

  return FALSE;
}

static void
cc_xkb_modifier_dialog_finalize (GObject *object)
{
  CcXkbModifierDialog *self = (CcXkbModifierDialog *)object;

  g_clear_object (&self->input_source_settings);

  G_OBJECT_CLASS (cc_xkb_modifier_dialog_parent_class)->finalize (object);
}

static void
cc_xkb_modifier_dialog_class_init (CcXkbModifierDialogClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = cc_xkb_modifier_dialog_finalize;

  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_Escape, 0, "window.close", NULL);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/keyboard/cc-xkb-modifier-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, CcXkbModifierDialog, xkb_modifier_page);
  gtk_widget_class_bind_template_child (widget_class, CcXkbModifierDialog, enabled_switch);
  gtk_widget_class_bind_template_child (widget_class, CcXkbModifierDialog, options_group);
  gtk_widget_class_bind_template_child (widget_class, CcXkbModifierDialog, switch_group);
  gtk_widget_class_bind_template_child (widget_class, CcXkbModifierDialog, switch_row);

  gtk_widget_class_bind_template_callback (widget_class, enable_switch_changed_cb);
}

static void
add_radio_buttons (CcXkbModifierDialog *self)
{
  g_autoptr (GSList) group = NULL;
  GtkWidget *row, *radio_button, *last_button = NULL;
  CcXkbOption *options = self->modifier->options;
  int i;

  for (i = 0; options[i].label && options[i].xkb_option; i++)
    {
      row = g_object_new (ADW_TYPE_ACTION_ROW,
                          "selectable", FALSE,
                          NULL);
      adw_preferences_group_add (self->options_group, row);

      radio_button = g_object_new (GTK_TYPE_CHECK_BUTTON,
                                   "valign", GTK_ALIGN_CENTER,
                                   "group", last_button,
                                   NULL);
      g_object_set_data (G_OBJECT (radio_button), "xkb-option", options[i].xkb_option);
      g_signal_connect_object (radio_button, "toggled", (GCallback)on_active_radio_changed_cb, self, G_CONNECT_SWAPPED);
      adw_action_row_add_prefix (ADW_ACTION_ROW (row), radio_button);
      adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), options[i].label);
      adw_action_row_set_activatable_widget (ADW_ACTION_ROW (row), radio_button);

      last_button = radio_button;
      group = g_slist_prepend (group, radio_button);
    }

  self->radio_group = NULL;
  if (last_button != NULL)
    self->radio_group = g_steal_pointer (&group);
}

static void
cc_xkb_modifier_dialog_init (CcXkbModifierDialog *self)
{
  g_autoptr(GtkCssProvider) provider = NULL;

  gtk_widget_init_template (GTK_WIDGET (self));

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (provider, custom_css, -1);

  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 1);

  self->modifier = NULL;

  self->input_source_settings = g_settings_new ("org.gnome.desktop.input-sources");
  g_signal_connect_object (self->input_source_settings,
                           "changed::xkb-options",
                           G_CALLBACK (on_xkb_options_changed_cb),
                           self, G_CONNECT_SWAPPED);
}

CcXkbModifierDialog *
cc_xkb_modifier_dialog_new (GSettings *input_settings,
                            const CcXkbModifier *modifier)
{
  CcXkbModifierDialog *self;

  self = g_object_new (CC_TYPE_XKB_MODIFIER_DIALOG, NULL);
  self->input_source_settings = g_object_ref (input_settings);

  self->modifier = modifier;
  gtk_window_set_title (GTK_WINDOW (self), gettext (modifier->title));
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self->switch_row), gettext (modifier->title));
  adw_preferences_page_set_description (self->xkb_modifier_page, gettext (modifier->description));
  gtk_widget_set_visible (GTK_WIDGET (self->switch_group), modifier->default_option == NULL);
  add_radio_buttons (self);
  update_active_radio (self);
  gtk_widget_set_sensitive (GTK_WIDGET (self->options_group), gtk_switch_get_state (self->enabled_switch));

  return self;
}

gboolean
xcb_modifier_transform_binding_to_label (GValue   *value,
                                         GVariant *variant,
                                         gpointer  user_data)
{
  const CcXkbModifier *modifier = user_data;
  const CcXkbOption *entry = NULL;
  const char **items;
  guint i;

  items = g_variant_get_strv (variant, NULL);

  for (i = 0; items != NULL && items[i] != NULL; i++)
    {
      entry = get_xkb_option_from_name (modifier, items[i]);
      if (entry != NULL)
        break;
    }

  if (entry == NULL && modifier->default_option == NULL)
    {
      g_value_set_string (value, _("Disabled"));
      return TRUE;
    }
  else if (entry == NULL)
    {
      entry = get_xkb_option_from_name(modifier, modifier->default_option);
    }

  g_value_set_string (value,
                      g_dpgettext2 (NULL, "keyboard key", entry->label));
  return TRUE;
}
