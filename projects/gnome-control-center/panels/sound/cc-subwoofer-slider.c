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

#include <pulse/pulseaudio.h>
#include <gvc-mixer-control.h>

#include "cc-sound-resources.h"
#include "cc-subwoofer-slider.h"
#include "gvc-channel-map-private.h"

struct _CcSubwooferSlider
{
  GtkWidget      parent_instance;

  GtkWidget     *scale;
  GtkAdjustment *adjustment;

  GvcChannelMap *channel_map;
  guint          volume_changed_handler_id;
};

G_DEFINE_TYPE (CcSubwooferSlider, cc_subwoofer_slider, GTK_TYPE_WIDGET)

static void
changed_cb (CcSubwooferSlider *self)
{
  gdouble value;
  const pa_channel_map *pa_map;
  pa_cvolume pa_volume;

  if (self->channel_map == NULL)
    return;

  value = gtk_adjustment_get_value (self->adjustment);
  pa_map = gvc_channel_map_get_pa_channel_map (self->channel_map);
  pa_volume = *gvc_channel_map_get_cvolume (self->channel_map);
  pa_cvolume_set_position (&pa_volume, pa_map, PA_CHANNEL_POSITION_LFE, value);
  gvc_channel_map_volume_changed (self->channel_map, &pa_volume, TRUE);
}

static void
volume_changed_cb (CcSubwooferSlider *self)
{
  const gdouble *volumes;

  volumes = gvc_channel_map_get_volume (self->channel_map);
  g_signal_handlers_block_by_func (self->adjustment, volume_changed_cb, self);
  gtk_adjustment_set_value (self->adjustment, volumes[LFE]);
  g_signal_handlers_unblock_by_func (self->adjustment, volume_changed_cb, self);
}

static void
cc_subwoofer_slider_dispose (GObject *object)
{
  CcSubwooferSlider *self = CC_SUBWOOFER_SLIDER (object);

  g_clear_pointer (&self->scale, gtk_widget_unparent);

  g_clear_object (&self->channel_map);

  G_OBJECT_CLASS (cc_subwoofer_slider_parent_class)->dispose (object);
}

void
cc_subwoofer_slider_class_init (CcSubwooferSliderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = cc_subwoofer_slider_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/sound/cc-subwoofer-slider.ui");

  gtk_widget_class_bind_template_child (widget_class, CcSubwooferSlider, scale);
  gtk_widget_class_bind_template_child (widget_class, CcSubwooferSlider, adjustment);

  gtk_widget_class_bind_template_callback (widget_class, changed_cb);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
}

void
cc_subwoofer_slider_init (CcSubwooferSlider *self)
{
  g_resources_register (cc_sound_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));
}

void
cc_subwoofer_slider_set_mixer_control (CcSubwooferSlider *self,
                                       GvcMixerControl   *mixer_control)
{
  gdouble vol_max_norm;

  g_return_if_fail (CC_IS_SUBWOOFER_SLIDER (self));

  vol_max_norm = gvc_mixer_control_get_vol_max_norm (mixer_control);
  gtk_adjustment_set_upper (self->adjustment, vol_max_norm);
  gtk_adjustment_set_page_increment (self->adjustment, vol_max_norm / 10.0);
  gtk_adjustment_set_step_increment (self->adjustment, vol_max_norm / 100.0);
}

void
cc_subwoofer_slider_set_channel_map (CcSubwooferSlider *self,
                                     GvcChannelMap     *channel_map)
{
  g_return_if_fail (CC_IS_SUBWOOFER_SLIDER (self));

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
