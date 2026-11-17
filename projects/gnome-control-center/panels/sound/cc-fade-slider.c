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

#include "cc-sound-resources.h"
#include "cc-fade-slider.h"
#include "gvc-channel-map-private.h"

struct _CcFadeSlider
{
  GtkWidget      parent_instance;

  GtkWidget     *scale;
  GtkAdjustment *adjustment;

  GvcChannelMap *channel_map;
  guint          volume_changed_handler_id;
};

G_DEFINE_TYPE (CcFadeSlider, cc_fade_slider, GTK_TYPE_WIDGET)

static void
changed_cb (CcFadeSlider *self)
{
  gdouble value;
  const pa_channel_map *pa_map;
  pa_cvolume pa_volume;

  if (self->channel_map == NULL)
    return;

  value = gtk_adjustment_get_value (self->adjustment);
  pa_map = gvc_channel_map_get_pa_channel_map (self->channel_map);
  pa_volume = *gvc_channel_map_get_cvolume (self->channel_map);
  pa_cvolume_set_fade (&pa_volume, pa_map, value);
  gvc_channel_map_volume_changed (self->channel_map, &pa_volume, TRUE);
}

static void
volume_changed_cb (CcFadeSlider *self)
{
  const gdouble *volumes;

  volumes = gvc_channel_map_get_volume (self->channel_map);
  g_signal_handlers_block_by_func (self->adjustment, volume_changed_cb, self);

  /* If close to 0, set to 0 to also snap while moving with arrow keys */
  if (fabs (volumes[FADE]) < 0.01)
    gtk_adjustment_set_value (self->adjustment, 0);
  else
    gtk_adjustment_set_value (self->adjustment, volumes[FADE]);

  g_signal_handlers_unblock_by_func (self->adjustment, volume_changed_cb, self);
}

static void
cc_fade_slider_dispose (GObject *object)
{
  CcFadeSlider *self = CC_FADE_SLIDER (object);

  g_clear_pointer (&self->scale, gtk_widget_unparent);

  g_clear_object (&self->channel_map);

  G_OBJECT_CLASS (cc_fade_slider_parent_class)->dispose (object);
}

void
cc_fade_slider_class_init (CcFadeSliderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = cc_fade_slider_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/sound/cc-fade-slider.ui");

  gtk_widget_class_bind_template_child (widget_class, CcFadeSlider, scale);
  gtk_widget_class_bind_template_child (widget_class, CcFadeSlider, adjustment);

  gtk_widget_class_bind_template_callback (widget_class, changed_cb);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
}

void
cc_fade_slider_init (CcFadeSlider *self)
{
  g_resources_register (cc_sound_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));

  // Don't flip the slider with RTL locale
  gtk_widget_set_direction (self->scale, GTK_TEXT_DIR_LTR);
}

void
cc_fade_slider_set_channel_map (CcFadeSlider  *self,
                                GvcChannelMap *channel_map)
{
  g_return_if_fail (CC_IS_FADE_SLIDER (self));

  if (self->channel_map != NULL)
    {
      g_signal_handler_disconnect (self->channel_map, self->volume_changed_handler_id);
      self->volume_changed_handler_id = 0;
    }
  g_clear_object (&self->channel_map);

  if (channel_map != NULL)
    {
      self->channel_map = g_object_ref (channel_map);

      self->volume_changed_handler_id = g_signal_connect_object (channel_map,
                                                                 "volume-changed",
                                                                 G_CALLBACK (volume_changed_cb),
                                                                 self, G_CONNECT_SWAPPED);
      volume_changed_cb (self);
    }
}
