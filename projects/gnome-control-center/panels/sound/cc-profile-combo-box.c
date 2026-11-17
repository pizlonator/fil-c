/*
 * Copyright (C) 2018 Canonical Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "cc-profile-combo-box.h"
#include "cc-sound-resources.h"

struct _CcProfileComboBox
{
  GtkComboBox       parent_instance;

  GtkListStore     *profile_model;

  GvcMixerControl  *mixer_control;
  GvcMixerUIDevice *device;
};

G_DEFINE_TYPE (CcProfileComboBox, cc_profile_combo_box, GTK_TYPE_COMBO_BOX)

static void
profile_changed_cb (CcProfileComboBox *self)
{
  GtkTreeIter iter;
  g_autofree gchar *profile = NULL;

  if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (self), &iter))
    return;

  gtk_tree_model_get (GTK_TREE_MODEL (self->profile_model), &iter,
                      1, &profile,
                      -1);

  if (!gvc_mixer_control_change_profile_on_selected_device (self->mixer_control,
                                                            self->device,
                                                            profile))
    {
      g_warning ("Failed to change profile on %s", gvc_mixer_ui_device_get_description (self->device));
    }
}

static void
cc_profile_combo_box_dispose (GObject *object)
{
  CcProfileComboBox *self = CC_PROFILE_COMBO_BOX (object);

  g_clear_object (&self->device);

  G_OBJECT_CLASS (cc_profile_combo_box_parent_class)->dispose (object);
}

void
cc_profile_combo_box_class_init (CcProfileComboBoxClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = cc_profile_combo_box_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/sound/cc-profile-combo-box.ui");

  gtk_widget_class_bind_template_child (widget_class, CcProfileComboBox, profile_model);

  gtk_widget_class_bind_template_callback (widget_class, profile_changed_cb);
}

void
cc_profile_combo_box_init (CcProfileComboBox *self)
{
  g_resources_register (cc_sound_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));
}

void
cc_profile_combo_box_set_device (CcProfileComboBox *self,
                                 GvcMixerControl   *mixer_control,
                                 GvcMixerUIDevice  *device)
{
  GList *profiles, *link;

  g_return_if_fail (CC_IS_PROFILE_COMBO_BOX (self));

  if (device == self->device)
    return;

  g_clear_object (&self->mixer_control);
  self->mixer_control = g_object_ref (mixer_control);
  g_clear_object (&self->device);
  gtk_list_store_clear (self->profile_model);

  if (device == NULL)
    return;

  self->device = g_object_ref (device);
  profiles = gvc_mixer_ui_device_get_profiles (device);
  for (link = profiles; link; link = link->next)
    {
      GvcMixerCardProfile *profile = link->data;
      GtkTreeIter iter;

      gtk_list_store_append (self->profile_model, &iter);
      gtk_list_store_set (self->profile_model, &iter,
                          0, profile->human_profile,
                          1, profile->profile,
                          -1);

      if (g_strcmp0 (gvc_mixer_ui_device_get_active_profile (device), profile->profile) == 0)
        {
          g_signal_handlers_block_by_func(self, profile_changed_cb, self);
          gtk_combo_box_set_active_iter (GTK_COMBO_BOX (self), &iter);
          g_signal_handlers_unblock_by_func(self, profile_changed_cb, self);
        }
    }
}

gint
cc_profile_combo_box_get_profile_count (CcProfileComboBox *self)
{
  g_return_val_if_fail (CC_IS_PROFILE_COMBO_BOX (self), 0);
  return gtk_tree_model_iter_n_children (GTK_TREE_MODEL (self->profile_model), NULL);
}
