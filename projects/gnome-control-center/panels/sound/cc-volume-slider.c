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

#include <glib/gi18n.h>
#include <math.h>
#include <pulse/pulseaudio.h>
#include <gvc-mixer-control.h>

#include "cc-sound-resources.h"
#include "cc-volume-slider.h"

struct _CcVolumeSlider
{
  GtkWidget        parent_instance;

  GtkButton       *mute_button;
  GtkAdjustment   *volume_adjustment;
  GtkScale        *volume_scale;

  gboolean         is_amplified;
  GvcMixerControl *mixer_control;
  GvcMixerStream  *stream;
  CcStreamType     type;
  guint            notify_volume_handler_id;
  guint            notify_is_muted_handler_id;
};

G_DEFINE_TYPE (CcVolumeSlider, cc_volume_slider, GTK_TYPE_WIDGET)

static void notify_is_muted_cb (CcVolumeSlider *self);

static void
update_mute_button_tooltip (CcVolumeSlider *self,
                            gdouble         volume)
{
  const gchar *tooltip;

  tooltip = (volume == 0.0) ? _("Unmute") : _("Mute");
  gtk_widget_set_tooltip_text (GTK_WIDGET (self->mute_button), tooltip);
}

static void
update_volume_icon (CcVolumeSlider *self)
{
  const gchar *icon_name = NULL;
  gdouble volume, fraction;

  volume = gtk_adjustment_get_value (self->volume_adjustment);
  fraction = (100.0 * volume) / gtk_adjustment_get_upper (self->volume_adjustment);

  update_mute_button_tooltip (self, volume);

  switch (self->type)
    {
    case CC_STREAM_TYPE_INPUT:
      if (fraction == 0.0)
        icon_name = "microphone-sensitivity-muted-symbolic";
      else if (fraction < 30.0)
        icon_name = "microphone-sensitivity-low-symbolic";
      else if (fraction > 30.0 && fraction < 70.0)
        icon_name = "microphone-sensitivity-medium-symbolic";
      else
        icon_name = "microphone-sensitivity-high-symbolic";
      break;

    case CC_STREAM_TYPE_OUTPUT:
      if (fraction == 0.0)
        icon_name = "audio-volume-muted-symbolic";
      else if (fraction < 30.0)
        icon_name = "audio-volume-low-symbolic";
      else if (fraction > 30.0 && fraction < 70.0)
        icon_name = "audio-volume-medium-symbolic";
      else
        icon_name = "audio-volume-high-symbolic";
      break;

    default:
      g_assert_not_reached ();
      break;
    }

  gtk_button_set_icon_name (GTK_BUTTON (self->mute_button), icon_name);
}

static void
volume_changed_cb (CcVolumeSlider *self)
{
  gdouble volume, rounded;

  if (self->stream == NULL)
    return;

  volume = gtk_adjustment_get_value (self->volume_adjustment);
  rounded = round (volume);

  // If the stream is muted, unmute it
  if (gvc_mixer_stream_get_is_muted (self->stream))
    gvc_mixer_stream_change_is_muted (self->stream, FALSE);

  if (gvc_mixer_stream_set_volume (self->stream, (pa_volume_t) rounded))
      gvc_mixer_stream_push_volume (self->stream);
}

static void
notify_volume_cb (CcVolumeSlider *self)
{
  g_signal_handlers_block_by_func (self->volume_adjustment, volume_changed_cb, self);
  gtk_adjustment_set_value (self->volume_adjustment, gvc_mixer_stream_get_volume (self->stream));
  g_signal_handlers_unblock_by_func (self->volume_adjustment, volume_changed_cb, self);

  update_volume_icon (self);
}

static void
update_ranges (CcVolumeSlider *self)
{
  gdouble vol_max_norm;

  if (self->mixer_control == NULL)
    return;

  vol_max_norm = gvc_mixer_control_get_vol_max_norm (self->mixer_control);

  gtk_scale_clear_marks (self->volume_scale);
  if (self->is_amplified)
    {
      gtk_adjustment_set_upper (self->volume_adjustment, gvc_mixer_control_get_vol_max_amplified (self->mixer_control));
      gtk_scale_add_mark (self->volume_scale,
                          vol_max_norm,
                          GTK_POS_BOTTOM,
                          C_("volume", "100%"));
    }
  else
    {
      gtk_adjustment_set_upper (self->volume_adjustment, vol_max_norm);
    }
  gtk_adjustment_set_page_increment (self->volume_adjustment, vol_max_norm / 10.0);
  gtk_adjustment_set_step_increment (self->volume_adjustment, vol_max_norm / 100.0);

  if (self->stream)
    {
      notify_volume_cb (self);
      notify_is_muted_cb (self);
    }
}

static void
mute_cb (GtkWidget  *widget,
         const char *action_name,
         GVariant   *parameter)
{
  CcVolumeSlider *self = CC_VOLUME_SLIDER (widget);

  if (self->stream == NULL)
    return;

  if (!gvc_mixer_stream_get_is_muted (self->stream) &&
      gvc_mixer_stream_get_volume (self->stream) == 0.0)
    {
      gdouble default_volume = gvc_mixer_control_get_vol_max_norm (self->mixer_control) * 0.25;

      if (gvc_mixer_stream_set_volume (self->stream, (pa_volume_t) default_volume))
        gvc_mixer_stream_push_volume (self->stream);
    }
  else
    {
      gvc_mixer_stream_change_is_muted (self->stream, !gvc_mixer_stream_get_is_muted (self->stream));
    }
}

static void
notify_is_muted_cb (CcVolumeSlider *self)
{
  if (gvc_mixer_stream_get_is_muted (self->stream))
    {
      g_signal_handlers_block_by_func (self->volume_adjustment, volume_changed_cb, self);
      gtk_adjustment_set_value (self->volume_adjustment, 0.0);
      g_signal_handlers_unblock_by_func (self->volume_adjustment, volume_changed_cb, self);

      update_volume_icon (self);
    }
  else
    {
      notify_volume_cb (self);
    }
}

static void
cc_volume_slider_dispose (GObject *object)
{
  CcVolumeSlider *self = CC_VOLUME_SLIDER (object);

  g_clear_pointer ((GtkWidget **) &self->volume_scale, gtk_widget_unparent);
  g_clear_pointer ((GtkWidget **) &self->mute_button, gtk_widget_unparent);

  g_clear_object (&self->mixer_control);
  g_clear_object (&self->stream);

  G_OBJECT_CLASS (cc_volume_slider_parent_class)->dispose (object);
}

void
cc_volume_slider_class_init (CcVolumeSliderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = cc_volume_slider_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/sound/cc-volume-slider.ui");

  gtk_widget_class_bind_template_child (widget_class, CcVolumeSlider, mute_button);
  gtk_widget_class_bind_template_child (widget_class, CcVolumeSlider, volume_adjustment);
  gtk_widget_class_bind_template_child (widget_class, CcVolumeSlider, volume_scale);

  gtk_widget_class_bind_template_callback (widget_class, volume_changed_cb);

  gtk_widget_class_install_action (widget_class, "volume-slider.mute", NULL, mute_cb);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BOX_LAYOUT);
}

void
cc_volume_slider_init (CcVolumeSlider *self)
{
  GtkLayoutManager *box_layout;

  g_resources_register (cc_sound_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));

  box_layout = gtk_widget_get_layout_manager (GTK_WIDGET (self));
  gtk_box_layout_set_spacing (GTK_BOX_LAYOUT (box_layout), 6);
}

void
cc_volume_slider_set_mixer_control (CcVolumeSlider  *self,
                                    GvcMixerControl *mixer_control)
{
  g_return_if_fail (CC_IS_VOLUME_SLIDER (self));

  g_set_object (&self->mixer_control, mixer_control);

  update_ranges (self);
}

void
cc_volume_slider_set_stream (CcVolumeSlider *self,
                             GvcMixerStream *stream,
                             CcStreamType    type)
{
  g_return_if_fail (CC_IS_VOLUME_SLIDER (self));

  if (self->stream != NULL)
    {
      g_signal_handler_disconnect (self->stream, self->notify_volume_handler_id);
      self->notify_volume_handler_id = 0;
      g_signal_handler_disconnect (self->stream, self->notify_is_muted_handler_id);
      self->notify_is_muted_handler_id = 0;
    }
  g_clear_object (&self->stream);

  self->type = type;

  if (stream != NULL)
    {
      self->stream = g_object_ref (stream);

      self->notify_volume_handler_id = g_signal_connect_object (stream,
                                                                "notify::volume",
                                                                G_CALLBACK (notify_volume_cb),
                                                                self, G_CONNECT_SWAPPED);
      self->notify_is_muted_handler_id = g_signal_connect_object (stream,
                                                                  "notify::is-muted",
                                                                  G_CALLBACK (notify_is_muted_cb),
                                                                  self, G_CONNECT_SWAPPED);
      notify_volume_cb (self);
      notify_is_muted_cb (self);
    }
}

GvcMixerStream *
cc_volume_slider_get_stream (CcVolumeSlider *self)
{
  g_return_val_if_fail (CC_IS_VOLUME_SLIDER (self), NULL);

  return self->stream;
}

void
cc_volume_slider_set_is_amplified (CcVolumeSlider *self,
                                   gboolean        is_amplified)
{
  g_return_if_fail (CC_IS_VOLUME_SLIDER (self));

  self->is_amplified = is_amplified;

  update_ranges (self);
}
