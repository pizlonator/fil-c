/* GTK - The GIMP Toolkit
 * Copyright (C) 2012 Red Hat, Inc.
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

#include "gtkcolorscaleprivate.h"

#include "deprecated/gtkcolorchooserprivate.h"
#include "gtkgesturelongpress.h"
#include "gtkgestureclick.h"
#include "gtkcolorutils.h"
#include "gtkorientable.h"
#include "gtkrangeprivate.h"
#include "gtkprivate.h"
#include "gtksnapshot.h"
#include "gtkshortcutcontroller.h"
#include "gtkshortcuttrigger.h"
#include "gtkshortcutaction.h"
#include "gtkshortcut.h"

#include <math.h>

struct _GtkColorScale
{
  GtkScale parent_instance;

  GdkRGBA color;
  GtkColorScaleType type;
  GdkTexture *hue_texture;
};

typedef struct
{
  GtkScaleClass parent_class;
} GtkColorScaleClass;

enum
{
  PROP_ZERO,
  PROP_SCALE_TYPE
};

static void hold_action (GtkGestureLongPress *gesture,
                         double               x,
                         double               y,
                         GtkWidget           *scale);

static void click_action (GtkGestureClick *gesture,
                          guint            n_presses,
                          double           x,
                          double           y,
                          GtkWidget       *scale);

G_DEFINE_TYPE (GtkColorScale, gtk_color_scale, GTK_TYPE_SCALE)

void
gtk_color_scale_snapshot_trough (GtkColorScale  *scale,
                                 GtkSnapshot    *snapshot,
                                 int             width,
                                 int             height)
{
  GtkWidget *widget = GTK_WIDGET (scale);

  if (width <= 1 || height <= 1)
    return;

  if (scale->hue_texture  &&
      (width != gdk_texture_get_width (scale->hue_texture) ||
       height != gdk_texture_get_height (scale->hue_texture)))
    g_clear_object (&scale->hue_texture);

  if (scale->type == GTK_COLOR_SCALE_HUE)
    {
      if (!scale->hue_texture)
        {
          const int stride = width * 3;
          GBytes *bytes;
          guchar *data, *p;
          int hue_x, hue_y;

          data = g_malloc (height * stride);

          for (hue_y = 0; hue_y < height; hue_y++)
            {
              const float h = CLAMP ((float)hue_y / (height - 1), 0.0, 1.0);
              float r, g, b;

              gtk_hsv_to_rgb (h, 1, 1, &r, &g, &b);

              p = data + hue_y * stride;
              for (hue_x = 0; hue_x < stride; hue_x += 3)
                {
                  p[hue_x + 0] = r * 255;
                  p[hue_x + 1] = g * 255;
                  p[hue_x + 2] = b * 255;
                }
            }

          bytes = g_bytes_new_take (data, height * stride);
          scale->hue_texture = gdk_memory_texture_new (width, height,
                                                       GDK_MEMORY_R8G8B8,
                                                       bytes,
                                                       stride);
          g_bytes_unref (bytes);
        }

      gtk_snapshot_append_texture (snapshot,
                                   scale->hue_texture,
                                   &GRAPHENE_RECT_INIT(0, 0, width, height));
    }
  else if (scale->type == GTK_COLOR_SCALE_ALPHA)
    {
      graphene_point_t start, end;
      const GdkRGBA *color;

      if (gtk_orientable_get_orientation (GTK_ORIENTABLE (widget)) == GTK_ORIENTATION_HORIZONTAL &&
          gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
        {
          graphene_point_init (&start, width, 0);
          graphene_point_init (&end, 0, 0);
        }
      else
        {
          graphene_point_init (&start, 0, 0);
          graphene_point_init (&end, width, 0);
        }

      _gtk_color_chooser_snapshot_checkered_pattern (snapshot, width, height);

      color = &scale->color;

      gtk_snapshot_append_linear_gradient (snapshot,
                                           &GRAPHENE_RECT_INIT(0, 0, width, height),
                                           &start,
                                           &end,
                                           (GskColorStop[2]) {
                                               { 0, { color->red, color->green, color->blue, 0 } },
                                               { 1, { color->red, color->green, color->blue, 1 } },
                                           },
                                           2);
    }
}

static void
gtk_color_scale_init (GtkColorScale *scale)
{
  GtkGesture *gesture;

  gesture = gtk_gesture_long_press_new ();
  g_signal_connect (gesture, "pressed",
                    G_CALLBACK (hold_action), scale);
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (gesture),
                                              GTK_PHASE_TARGET);
  gtk_widget_add_controller (GTK_WIDGET (scale), GTK_EVENT_CONTROLLER (gesture));

  gesture = gtk_gesture_click_new ();
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), GDK_BUTTON_SECONDARY);
  g_signal_connect (gesture, "pressed",
                    G_CALLBACK (click_action), scale);
  gtk_widget_add_controller (GTK_WIDGET (scale), GTK_EVENT_CONTROLLER (gesture));

  gtk_widget_add_css_class (GTK_WIDGET (scale), "color");
}

static void
scale_constructed (GObject *object)
{
  GtkColorScale *scale = GTK_COLOR_SCALE (object);
  GtkEventController *controller;
  GtkShortcutTrigger *trigger;
  GtkShortcutAction *action;
  GtkShortcut *shortcut;

  controller = gtk_shortcut_controller_new ();
  trigger = gtk_alternative_trigger_new (gtk_keyval_trigger_new (GDK_KEY_F10, GDK_SHIFT_MASK),
                                         gtk_keyval_trigger_new (GDK_KEY_Menu, 0));
  action = gtk_named_action_new ("color.edit");
  shortcut = gtk_shortcut_new_with_arguments (trigger,
                                              action,
                                              "s",
                                              scale->type == GTK_COLOR_SCALE_ALPHA
                                                ? "a" : "h");
  gtk_shortcut_controller_add_shortcut (GTK_SHORTCUT_CONTROLLER (controller), shortcut);
  gtk_widget_add_controller (GTK_WIDGET (scale), controller);
}

static void
scale_get_property (GObject    *object,
                    guint       prop_id,
                    GValue     *value,
                    GParamSpec *pspec)
{
  GtkColorScale *scale = GTK_COLOR_SCALE (object);

  switch (prop_id)
    {
    case PROP_SCALE_TYPE:
      g_value_set_int (value, scale->type);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
scale_set_property (GObject      *object,
                    guint         prop_id,
                    const GValue *value,
                    GParamSpec   *pspec)
{
  GtkColorScale *scale = GTK_COLOR_SCALE (object);

  switch (prop_id)
    {
    case PROP_SCALE_TYPE:
      scale->type = (GtkColorScaleType) g_value_get_int (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hold_action (GtkGestureLongPress *gesture,
             double               x,
             double               y,
             GtkWidget           *scale)
{
  gtk_widget_activate_action (scale,
                              "color.edit",
                              "s", gtk_widget_get_name (scale));
}

static void
click_action (GtkGestureClick *gesture,
              guint            n_presses,
              double           x,
              double           y,
              GtkWidget       *scale)
{
  gtk_widget_activate_action (scale,
                              "color.edit",
                              "s", gtk_widget_get_name (scale));
}

static void
scale_finalize (GObject *object)
{
  GtkColorScale *scale = GTK_COLOR_SCALE (object);

  g_clear_object (&scale->hue_texture);

  G_OBJECT_CLASS (gtk_color_scale_parent_class)->finalize (object);
}

static void
gtk_color_scale_class_init (GtkColorScaleClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructed = scale_constructed;
  object_class->finalize = scale_finalize;
  object_class->get_property = scale_get_property;
  object_class->set_property = scale_set_property;

  g_object_class_install_property (object_class, PROP_SCALE_TYPE,
      g_param_spec_int ("scale-type", NULL, NULL,
                        0, 1, 0,
                        GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

void
gtk_color_scale_set_rgba (GtkColorScale *scale,
                          const GdkRGBA *color)
{
  scale->color = *color;
  gtk_widget_queue_draw (gtk_range_get_trough_widget (GTK_RANGE (scale)));
}

GtkWidget *
gtk_color_scale_new (GtkAdjustment     *adjustment,
                     GtkColorScaleType  type)
{
  return g_object_new (GTK_TYPE_COLOR_SCALE,
                       "adjustment", adjustment,
                       "draw-value", FALSE,
                       "scale-type", type,
                       NULL);
}
