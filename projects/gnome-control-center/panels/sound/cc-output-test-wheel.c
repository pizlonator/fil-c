/*
 * Copyright (C) 2018 Canonical Ltd.
 * Copyright (C) 2022 Marco Melorio
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

#include "cc-speaker-test-button.h"
#include "cc-output-test-wheel.h"

struct _CcOutputTestWheel
{
  GtkWidget      parent_instance;

  GtkWidget     *label;
  GtkWidget     *front_center_speaker_button;
  GtkWidget     *front_left_speaker_button;
  GtkWidget     *front_left_of_center_speaker_button;
  GtkWidget     *front_right_of_center_speaker_button;
  GtkWidget     *front_right_speaker_button;
  GtkWidget     *lfe_speaker_button;
  GtkWidget     *rear_center_speaker_button;
  GtkWidget     *rear_left_speaker_button;
  GtkWidget     *rear_right_speaker_button;
  GtkWidget     *side_left_speaker_button;
  GtkWidget     *side_right_speaker_button;

  GSoundContext *context;
};

G_DEFINE_TYPE (CcOutputTestWheel, cc_output_test_wheel, GTK_TYPE_WIDGET)

static void
load_custom_css (CcOutputTestWheel *self)
{
  g_autoptr(GtkCssProvider) provider = NULL;

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_resource (provider, "/org/gnome/control-center/sound/output-test-wheel.css");
  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

static GtkWidget*
create_speaker_button (CcOutputTestWheel     *self,
                       pa_channel_position_t  position)
{
  GtkWidget *button;

  button = cc_speaker_test_button_new (self->context, position);
  gtk_widget_add_css_class (button, "circular");
  gtk_widget_add_css_class (button, "opaque");
  gtk_widget_set_parent (button, GTK_WIDGET (self));

  return button;
}

static void
allocate_speaker_button (GtkWidget *button,
                         int        width,
                         int        height,
                         double     angle)
{
  double rad, norm_x, norm_y;
  GtkRequisition nat;
  GtkAllocation allocation;

  rad = angle * G_PI / 180;
  norm_x = -(cos(rad) - 1) / 2;
  norm_y = -(sin(rad) - 1) / 2;

  gtk_widget_get_preferred_size (button, NULL, &nat);

  allocation.width = nat.width;
  allocation.height = nat.height;
  allocation.x = (norm_x * width) - (allocation.width / 2);
  allocation.y = (norm_y * height) - (allocation.height / 2);

  gtk_widget_size_allocate (button, &allocation, -1);
}

static void
cc_output_test_wheel_measure (GtkWidget      *widget,
                              GtkOrientation  orientation,
                              int             for_size,
                              int            *minimum,
                              int            *natural,
                              int            *minimum_baseline,
                              int            *natural_baseline)
{
  CcOutputTestWheel *self = CC_OUTPUT_TEST_WHEEL (widget);

  /* Just measure the label to make GTK not complain about allocating children
   * without measuring anything. Measuring all the buttons is an overkill
   * because we expect this widget to have the minimum size overwritten by CSS
   * either way.
   */
  gtk_widget_measure (self->label, orientation, for_size,
                      minimum, natural, NULL, NULL);
}

static void
cc_output_test_wheel_size_allocate (GtkWidget *widget,
                                    int        width,
                                    int        height,
                                    int        baseline)
{
  CcOutputTestWheel *self = CC_OUTPUT_TEST_WHEEL (widget);
  GtkAllocation allocation;
  GtkRequisition natural_size;

  gtk_widget_allocate (self->label, width, height, baseline, NULL);

  allocate_speaker_button (self->side_left_speaker_button, width, height, 0);
  allocate_speaker_button (self->front_left_speaker_button, width, height, 45);
  allocate_speaker_button (self->front_left_of_center_speaker_button, width, height, 67.5);
  allocate_speaker_button (self->front_center_speaker_button, width, height, 90);
  allocate_speaker_button (self->front_right_of_center_speaker_button, width, height, 112.5);
  allocate_speaker_button (self->front_right_speaker_button, width, height, 135);
  allocate_speaker_button (self->side_right_speaker_button, width, height, 180);
  allocate_speaker_button (self->rear_right_speaker_button, width, height, 225);
  allocate_speaker_button (self->rear_center_speaker_button, width, height, 270);
  allocate_speaker_button (self->rear_left_speaker_button, width, height, 315);

  gtk_widget_get_preferred_size(self->lfe_speaker_button, NULL, &natural_size);
  allocation.width = natural_size.width;
  allocation.height = natural_size.height;
  allocation.x = (width / 2) - (allocation.width / 2);
  allocation.y = (height * 0.2) - (allocation.height / 2);

  gtk_widget_size_allocate (self->lfe_speaker_button, &allocation, -1);
}

static void
cc_output_test_wheel_dispose (GObject *object)
{
  CcOutputTestWheel *self = CC_OUTPUT_TEST_WHEEL (object);

  g_clear_pointer (&self->label, gtk_widget_unparent);

  g_clear_pointer (&self->front_center_speaker_button, gtk_widget_unparent);
  g_clear_pointer (&self->front_left_speaker_button, gtk_widget_unparent);
  g_clear_pointer (&self->front_left_of_center_speaker_button, gtk_widget_unparent);
  g_clear_pointer (&self->front_right_of_center_speaker_button, gtk_widget_unparent);
  g_clear_pointer (&self->front_right_speaker_button, gtk_widget_unparent);
  g_clear_pointer (&self->lfe_speaker_button, gtk_widget_unparent);
  g_clear_pointer (&self->rear_center_speaker_button, gtk_widget_unparent);
  g_clear_pointer (&self->rear_left_speaker_button, gtk_widget_unparent);
  g_clear_pointer (&self->rear_right_speaker_button, gtk_widget_unparent);
  g_clear_pointer (&self->side_left_speaker_button, gtk_widget_unparent);
  g_clear_pointer (&self->side_right_speaker_button, gtk_widget_unparent);

  g_clear_object (&self->context);

  G_OBJECT_CLASS (cc_output_test_wheel_parent_class)->dispose (object);
}

void
cc_output_test_wheel_class_init (CcOutputTestWheelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = cc_output_test_wheel_dispose;

  widget_class->measure = cc_output_test_wheel_measure;
  widget_class->size_allocate = cc_output_test_wheel_size_allocate;

  gtk_widget_class_set_css_name (widget_class, "wheel");
}

void
cc_output_test_wheel_init (CcOutputTestWheel *self)
{
  GtkSettings *settings;
  g_autofree gchar *theme_name = NULL;

  self->context = gsound_context_new (NULL, NULL);
  gsound_context_set_driver (self->context, "pulse", NULL);
  gsound_context_set_attributes (self->context, NULL,
                                 GSOUND_ATTR_APPLICATION_ID, "org.gnome.VolumeControl",
                                 NULL);
  settings = gtk_settings_get_for_display (gdk_display_get_default ());
  g_object_get (G_OBJECT (settings),
                "gtk-sound-theme-name", &theme_name,
                NULL);
  if (theme_name != NULL)
     gsound_context_set_attributes (self->context, NULL,
                                    GSOUND_ATTR_CANBERRA_XDG_THEME_NAME, theme_name,
                                    NULL);

  self->label = gtk_inscription_new (_("Select a Speaker"));
  gtk_inscription_set_xalign (GTK_INSCRIPTION (self->label), 0.5);
  gtk_widget_add_css_class (self->label, "heading");
  gtk_widget_set_parent (self->label, GTK_WIDGET (self));

  self->front_center_speaker_button = create_speaker_button (self, PA_CHANNEL_POSITION_FRONT_CENTER);
  self->front_left_speaker_button = create_speaker_button (self, PA_CHANNEL_POSITION_FRONT_LEFT);
  self->front_left_of_center_speaker_button = create_speaker_button (self, PA_CHANNEL_POSITION_FRONT_LEFT_OF_CENTER);
  self->front_right_of_center_speaker_button = create_speaker_button (self, PA_CHANNEL_POSITION_FRONT_RIGHT_OF_CENTER);
  self->front_right_speaker_button = create_speaker_button (self, PA_CHANNEL_POSITION_FRONT_RIGHT);
  self->lfe_speaker_button = create_speaker_button (self, PA_CHANNEL_POSITION_LFE);
  self->rear_center_speaker_button = create_speaker_button (self, PA_CHANNEL_POSITION_REAR_CENTER);
  self->rear_left_speaker_button = create_speaker_button (self, PA_CHANNEL_POSITION_REAR_LEFT);
  self->rear_right_speaker_button = create_speaker_button (self, PA_CHANNEL_POSITION_REAR_RIGHT);
  self->side_left_speaker_button = create_speaker_button (self, PA_CHANNEL_POSITION_SIDE_LEFT);
  self->side_right_speaker_button = create_speaker_button (self, PA_CHANNEL_POSITION_SIDE_RIGHT);

  load_custom_css (self);
}

void
cc_output_test_wheel_set_stream (CcOutputTestWheel *self,
                                 GvcMixerStream    *stream)
{
  const GvcChannelMap *map = gvc_mixer_stream_get_channel_map (stream);

  gtk_widget_set_visible (self->front_left_speaker_button, gvc_channel_map_has_position (map, PA_CHANNEL_POSITION_FRONT_LEFT));
  gtk_widget_set_visible (self->front_left_of_center_speaker_button, gvc_channel_map_has_position (map, PA_CHANNEL_POSITION_FRONT_LEFT_OF_CENTER));
  gtk_widget_set_visible (self->front_right_of_center_speaker_button, gvc_channel_map_has_position (map, PA_CHANNEL_POSITION_FRONT_RIGHT_OF_CENTER));
  gtk_widget_set_visible (self->front_right_speaker_button, gvc_channel_map_has_position (map, PA_CHANNEL_POSITION_FRONT_RIGHT));
  gtk_widget_set_visible (self->lfe_speaker_button, gvc_channel_map_has_position (map, PA_CHANNEL_POSITION_LFE));
  gtk_widget_set_visible (self->rear_center_speaker_button, gvc_channel_map_has_position (map, PA_CHANNEL_POSITION_REAR_CENTER));
  gtk_widget_set_visible (self->rear_left_speaker_button, gvc_channel_map_has_position (map, PA_CHANNEL_POSITION_REAR_LEFT));
  gtk_widget_set_visible (self->rear_right_speaker_button, gvc_channel_map_has_position (map, PA_CHANNEL_POSITION_REAR_RIGHT));
  gtk_widget_set_visible (self->side_left_speaker_button, gvc_channel_map_has_position (map, PA_CHANNEL_POSITION_SIDE_LEFT));
  gtk_widget_set_visible (self->side_right_speaker_button, gvc_channel_map_has_position (map, PA_CHANNEL_POSITION_SIDE_RIGHT));

  /* Replace the center channel with a mono channel */
  if (gvc_channel_map_has_position (map, PA_CHANNEL_POSITION_MONO))
    {
      if (gvc_channel_map_has_position (map, PA_CHANNEL_POSITION_FRONT_CENTER))
        g_warning ("Testing output with both front center and mono channels - front center is hidden");

      cc_speaker_test_button_set_channel_position (CC_SPEAKER_TEST_BUTTON (self->front_center_speaker_button),
                                                   PA_CHANNEL_POSITION_MONO);
      gtk_widget_set_visible (GTK_WIDGET (self->front_center_speaker_button), TRUE);
    }
  else if (gvc_channel_map_has_position (map, PA_CHANNEL_POSITION_FRONT_CENTER))
    {
      cc_speaker_test_button_set_channel_position (CC_SPEAKER_TEST_BUTTON (self->front_center_speaker_button),
                                                   PA_CHANNEL_POSITION_FRONT_CENTER);
      gtk_widget_set_visible (GTK_WIDGET (self->front_center_speaker_button), TRUE);
    }
  else
    {
      gtk_widget_set_visible (GTK_WIDGET (self->front_center_speaker_button), FALSE);
    }
}
