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

#include "gtkcolorswatchprivate.h"

#include "gtkbox.h"
#include "deprecated/gtkcolorchooserprivate.h"
#include "gtkdragsource.h"
#include "gtkdroptarget.h"
#include "gtkgesturelongpress.h"
#include "gtkgestureclick.h"
#include "gtkgesturesingle.h"
#include "gtkimage.h"
#include <glib/gi18n-lib.h>
#include "gtkmain.h"
#include "gtkmodelbuttonprivate.h"
#include "gtkpopovermenu.h"
#include "gtkprivate.h"
#include "gtksnapshot.h"
#include "gtkwidgetprivate.h"
#include "gtkeventcontrollerkey.h"
#include "gtknative.h"

/*
 * GtkColorSwatch has two CSS nodes, the main one named colorswatch
 * and a subnode named overlay. The main node gets the .light or .dark
 * style classes added depending on the brightness of the color that
 * the swatch is showing.
 *
 * The color swatch has the .activatable style class by default. It can
 * be removed for non-activatable swatches.
 */

typedef struct _GtkColorSwatchClass   GtkColorSwatchClass;

struct _GtkColorSwatch
{
  GtkWidget parent_instance;

  GdkRGBA color;
  char *icon;
  guint    has_color        : 1;
  guint    use_alpha        : 1;
  guint    selectable       : 1;
  guint    has_menu         : 1;

  GtkWidget *overlay_widget;

  GtkWidget *popover;
  GtkDropTarget *dest;
  GtkDragSource *source;
};

struct _GtkColorSwatchClass
{
  GtkWidgetClass parent_class;

  void ( * activate)  (GtkColorSwatch *swatch);
  void ( * customize) (GtkColorSwatch *swatch);
};

enum
{
  PROP_ZERO,
  PROP_RGBA,
  PROP_SELECTABLE,
  PROP_HAS_MENU,
  PROP_CAN_DROP,
  PROP_CAN_DRAG
};

G_DEFINE_TYPE (GtkColorSwatch, gtk_color_swatch, GTK_TYPE_WIDGET)

#define INTENSITY(r, g, b) ((r) * 0.30 + (g) * 0.59 + (b) * 0.11)
static void
swatch_snapshot (GtkWidget   *widget,
                 GtkSnapshot *snapshot)
{
  GtkColorSwatch *swatch = GTK_COLOR_SWATCH (widget);
  const int width = gtk_widget_get_width (widget);
  const int height = gtk_widget_get_height (widget);
  const GdkRGBA *color;

  color = &swatch->color;
  if (swatch->dest)
    {
      const GValue *value = gtk_drop_target_get_value (swatch->dest);

      if (value)
        color = g_value_get_boxed (value);
    }

  if (swatch->has_color)
    {
      if (swatch->use_alpha && !gdk_rgba_is_opaque (color))
        {
          _gtk_color_chooser_snapshot_checkered_pattern (snapshot, width, height);

          gtk_snapshot_append_color (snapshot,
                                     color,
                                     &GRAPHENE_RECT_INIT (0, 0, width, height));
        }
      else
        {
          GdkRGBA opaque = *color;

          opaque.alpha = 1.0;

          gtk_snapshot_append_color (snapshot,
                                     &opaque,
                                     &GRAPHENE_RECT_INIT (0, 0, width, height));
        }
    }

  gtk_widget_snapshot_child (widget, swatch->overlay_widget, snapshot);
}

static gboolean
swatch_drag_drop (GtkDropTarget  *dest,
                  const GValue   *value,
                  double          x,
                  double          y,
                  GtkColorSwatch *swatch)

{
  gtk_color_swatch_set_rgba (swatch, g_value_get_boxed (value));

  return TRUE;
}

void
gtk_color_swatch_activate (GtkColorSwatch *swatch)
{
  double red, green, blue, alpha;

  red = swatch->color.red;
  green = swatch->color.green;
  blue = swatch->color.blue;
  alpha = swatch->color.alpha;

  gtk_widget_activate_action (GTK_WIDGET (swatch),
                              "color.select", "(dddd)", red, green, blue, alpha);
}

void
gtk_color_swatch_customize (GtkColorSwatch *swatch)
{
  double red, green, blue, alpha;

  red = swatch->color.red;
  green = swatch->color.green;
  blue = swatch->color.blue;
  alpha = swatch->color.alpha;

  gtk_widget_activate_action (GTK_WIDGET (swatch),
                              "color.customize", "(dddd)", red, green, blue, alpha);
}

void
gtk_color_swatch_select (GtkColorSwatch *swatch)
{
  gtk_widget_set_state_flags (GTK_WIDGET (swatch), GTK_STATE_FLAG_SELECTED, FALSE);
}

static gboolean
gtk_color_swatch_is_selected (GtkColorSwatch *swatch)
{
  return (gtk_widget_get_state_flags (GTK_WIDGET (swatch)) & GTK_STATE_FLAG_SELECTED) != 0;
}

static gboolean
key_controller_key_pressed (GtkEventControllerKey *controller,
                            guint                  keyval,
                            guint                  keycode,
                            GdkModifierType        state,
                            GtkWidget             *widget)
{
  GtkColorSwatch *swatch = GTK_COLOR_SWATCH (widget);

  if (keyval == GDK_KEY_space ||
      keyval == GDK_KEY_Return ||
      keyval == GDK_KEY_ISO_Enter||
      keyval == GDK_KEY_KP_Enter ||
      keyval == GDK_KEY_KP_Space)
    {
      if (swatch->has_color &&
          swatch->selectable &&
          !gtk_color_swatch_is_selected (swatch))
        gtk_color_swatch_select (swatch);
      else
        gtk_color_swatch_customize (swatch);

      return TRUE;
    }

  return FALSE;
}

static GMenuModel *
gtk_color_swatch_get_menu_model (GtkColorSwatch *swatch)
{
  GMenu *menu, *section;
  GMenuItem *item;
  double red, green, blue, alpha;

  menu = g_menu_new ();

  red = swatch->color.red;
  green = swatch->color.green;
  blue = swatch->color.blue;
  alpha = swatch->color.alpha;

  section = g_menu_new ();
  item = g_menu_item_new (_("Customize"), NULL);
  g_menu_item_set_action_and_target_value (item, "color.customize",
                                           g_variant_new ("(dddd)", red, green, blue, alpha));

  g_menu_append_item (section, item);
  g_menu_append_section (menu, NULL, G_MENU_MODEL (section));
  g_object_unref (item);
  g_object_unref (section);

  return G_MENU_MODEL (menu);
}

static void
do_popup (GtkColorSwatch *swatch)
{
  GMenuModel *model;

  g_clear_pointer (&swatch->popover, gtk_widget_unparent);

  model = gtk_color_swatch_get_menu_model (swatch);
  swatch->popover = gtk_popover_menu_new_from_model (model);
  gtk_widget_set_parent (swatch->popover, GTK_WIDGET (swatch));
  g_object_unref (model);

  gtk_popover_popup (GTK_POPOVER (swatch->popover));
}

static gboolean
swatch_primary_action (GtkColorSwatch *swatch)
{
  if (!swatch->has_color)
    {
      gtk_color_swatch_customize (swatch);
      return TRUE;
    }
  else if (swatch->selectable &&
           !gtk_color_swatch_is_selected (swatch))
    {
      gtk_color_swatch_select (swatch);
      return TRUE;
    }

  return FALSE;
}

static void
hold_action (GtkGestureLongPress *gesture,
             double               x,
             double               y,
             GtkColorSwatch      *swatch)
{
  do_popup (swatch);
  gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
}

static void
tap_action (GtkGestureClick *gesture,
            int              n_press,
            double           x,
            double           y,
            GtkColorSwatch  *swatch)
{
  guint button;

  button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));

  if (button == GDK_BUTTON_PRIMARY)
    {
      if (n_press == 1)
        swatch_primary_action (swatch);
      else if (n_press > 1)
        gtk_color_swatch_activate (swatch);
    }
  else if (button == GDK_BUTTON_SECONDARY)
    {
      if (swatch->has_color && swatch->has_menu)
        do_popup (swatch);
    }
}

static void
swatch_size_allocate (GtkWidget *widget,
                      int        width,
                      int        height,
                      int        baseline)
{
  GtkColorSwatch *swatch = GTK_COLOR_SWATCH (widget);

  gtk_widget_size_allocate (swatch->overlay_widget,
                            &(GtkAllocation) {
                              0, 0,
                              width, height
                            }, -1);

  if (swatch->popover)
    gtk_popover_present (GTK_POPOVER (swatch->popover));
}

static void
gtk_color_swatch_measure (GtkWidget *widget,
                          GtkOrientation  orientation,
                          int             for_size,
                          int            *minimum,
                          int            *natural,
                          int            *minimum_baseline,
                          int            *natural_baseline)
{
  GtkColorSwatch *swatch = GTK_COLOR_SWATCH (widget);
  int w, h, min;

  gtk_widget_measure (swatch->overlay_widget,
                      orientation,
                      -1,
                      minimum, natural,
                      NULL, NULL);

  gtk_widget_get_size_request (widget, &w, &h);
  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    min = w < 0 ? 48 : w;
  else
    min = h < 0 ? 32 : h;

  *minimum = MAX (*minimum, min);
  *natural = MAX (*natural, min);
}

static void
swatch_popup_menu (GtkWidget  *widget,
                   const char *action_name,
                   GVariant   *parameters)
{
  do_popup (GTK_COLOR_SWATCH (widget));
}

static void
update_icon (GtkColorSwatch *swatch)
{
  GtkImage *image = GTK_IMAGE (swatch->overlay_widget);

  if (swatch->icon)
    gtk_image_set_from_icon_name (image, swatch->icon);
  else if (gtk_color_swatch_is_selected (swatch))
    gtk_image_set_from_icon_name (image, "object-select-symbolic");
  else
    gtk_image_clear (image);
}

static void
update_accessible_properties (GtkColorSwatch *swatch)
{
  if (swatch->selectable)
    {
      gboolean selected = gtk_color_swatch_is_selected (swatch);

      gtk_accessible_update_state (GTK_ACCESSIBLE (swatch),
                                   GTK_ACCESSIBLE_STATE_CHECKED, selected,
                                   -1);
    }
  else
    {
      gtk_accessible_reset_state (GTK_ACCESSIBLE (swatch),
                                  GTK_ACCESSIBLE_STATE_CHECKED);
    }
}

static void
swatch_state_flags_changed (GtkWidget     *widget,
                            GtkStateFlags  previous_state)
{
  GtkColorSwatch *swatch = GTK_COLOR_SWATCH (widget);

  update_icon (swatch);
  update_accessible_properties (swatch);

  GTK_WIDGET_CLASS (gtk_color_swatch_parent_class)->state_flags_changed (widget, previous_state);
}

/* GObject implementation {{{1 */

static void
swatch_get_property (GObject    *object,
                     guint       prop_id,
                     GValue     *value,
                     GParamSpec *pspec)
{
  GtkColorSwatch *swatch = GTK_COLOR_SWATCH (object);
  GdkRGBA color;

  switch (prop_id)
    {
    case PROP_RGBA:
      gtk_color_swatch_get_rgba (swatch, &color);
      g_value_set_boxed (value, &color);
      break;
    case PROP_SELECTABLE:
      g_value_set_boolean (value, gtk_color_swatch_get_selectable (swatch));
      break;
    case PROP_HAS_MENU:
      g_value_set_boolean (value, swatch->has_menu);
      break;
    case PROP_CAN_DROP:
      g_value_set_boolean (value, swatch->dest != NULL);
      break;
    case PROP_CAN_DRAG:
      g_value_set_boolean (value, swatch->source != NULL);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
swatch_set_property (GObject      *object,
                     guint         prop_id,
                     const GValue *value,
                     GParamSpec   *pspec)
{
  GtkColorSwatch *swatch = GTK_COLOR_SWATCH (object);

  switch (prop_id)
    {
    case PROP_RGBA:
      gtk_color_swatch_set_rgba (swatch, g_value_get_boxed (value));
      break;
    case PROP_SELECTABLE:
      gtk_color_swatch_set_selectable (swatch, g_value_get_boolean (value));
      break;
    case PROP_HAS_MENU:
      swatch->has_menu = g_value_get_boolean (value);
      break;
    case PROP_CAN_DROP:
      gtk_color_swatch_set_can_drop (swatch, g_value_get_boolean (value));
      break;
    case PROP_CAN_DRAG:
      gtk_color_swatch_set_can_drag (swatch, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
swatch_finalize (GObject *object)
{
  GtkColorSwatch *swatch = GTK_COLOR_SWATCH (object);

  g_free (swatch->icon);
  gtk_widget_unparent (swatch->overlay_widget);

  G_OBJECT_CLASS (gtk_color_swatch_parent_class)->finalize (object);
}

static void
swatch_dispose (GObject *object)
{
  GtkColorSwatch *swatch = GTK_COLOR_SWATCH (object);

  g_clear_pointer (&swatch->popover, gtk_widget_unparent);

  G_OBJECT_CLASS (gtk_color_swatch_parent_class)->dispose (object);
}

static void
gtk_color_swatch_class_init (GtkColorSwatchClass *class)
{
  GtkWidgetClass *widget_class = (GtkWidgetClass *)class;
  GObjectClass *object_class = (GObjectClass *)class;

  object_class->get_property = swatch_get_property;
  object_class->set_property = swatch_set_property;
  object_class->finalize = swatch_finalize;
  object_class->dispose = swatch_dispose;

  widget_class->measure = gtk_color_swatch_measure;
  widget_class->snapshot = swatch_snapshot;
  widget_class->size_allocate = swatch_size_allocate;
  widget_class->state_flags_changed = swatch_state_flags_changed;

  g_object_class_install_property (object_class, PROP_RGBA,
      g_param_spec_boxed ("rgba", NULL, NULL,
                          GDK_TYPE_RGBA, GTK_PARAM_READWRITE));
  g_object_class_install_property (object_class, PROP_SELECTABLE,
      g_param_spec_boolean ("selectable", NULL, NULL,
                            TRUE, GTK_PARAM_READWRITE));
  g_object_class_install_property (object_class, PROP_HAS_MENU,
      g_param_spec_boolean ("has-menu", NULL, NULL,
                            TRUE, GTK_PARAM_READWRITE));
  g_object_class_install_property (object_class, PROP_CAN_DROP,
      g_param_spec_boolean ("can-drop", NULL, NULL,
                            FALSE, GTK_PARAM_READWRITE));
  g_object_class_install_property (object_class, PROP_CAN_DRAG,
      g_param_spec_boolean ("can-drag", NULL, NULL,
                            TRUE, GTK_PARAM_READWRITE));

  /**
   * GtkColorSwatch|menu.popup:
   *
   * Opens the context menu.
   */
  gtk_widget_class_install_action (widget_class, "menu.popup", NULL, swatch_popup_menu);

  gtk_widget_class_add_binding_action (widget_class,
                                       GDK_KEY_F10, GDK_SHIFT_MASK,
                                       "menu.popup",
                                       NULL);
  gtk_widget_class_add_binding_action (widget_class,
                                       GDK_KEY_Menu, 0,
                                       "menu.popup",
                                       NULL);

  gtk_widget_class_set_css_name (widget_class, I_("colorswatch"));
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_RADIO);
}

static void
gtk_color_swatch_init (GtkColorSwatch *swatch)
{
  GtkEventController *controller;
  GtkGesture *gesture;

  swatch->use_alpha = TRUE;
  swatch->selectable = TRUE;
  swatch->has_menu = TRUE;
  swatch->color.red = 0.75;
  swatch->color.green = 0.25;
  swatch->color.blue = 0.25;
  swatch->color.alpha = 1.0;

  gtk_widget_set_focusable (GTK_WIDGET (swatch), TRUE);
  gtk_widget_set_overflow (GTK_WIDGET (swatch), GTK_OVERFLOW_HIDDEN);

  gesture = gtk_gesture_long_press_new ();
  gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (gesture),
                                     TRUE);
  g_signal_connect (gesture, "pressed",
                    G_CALLBACK (hold_action), swatch);
  gtk_widget_add_controller (GTK_WIDGET (swatch), GTK_EVENT_CONTROLLER (gesture));

  gesture = gtk_gesture_click_new ();
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), 0);
  g_signal_connect (gesture, "pressed",
                    G_CALLBACK (tap_action), swatch);
  gtk_widget_add_controller (GTK_WIDGET (swatch), GTK_EVENT_CONTROLLER (gesture));

  controller = gtk_event_controller_key_new ();
  g_signal_connect (controller, "key-pressed",
                    G_CALLBACK (key_controller_key_pressed), swatch);
  gtk_widget_add_controller (GTK_WIDGET (swatch), controller);

  gtk_color_swatch_set_can_drag (swatch, TRUE);

  gtk_widget_add_css_class (GTK_WIDGET (swatch), "activatable");

  swatch->overlay_widget = g_object_new (GTK_TYPE_IMAGE,
                                         "accessible-role", GTK_ACCESSIBLE_ROLE_NONE,
                                         "css-name", "overlay",
                                         NULL);
  gtk_widget_set_parent (swatch->overlay_widget, GTK_WIDGET (swatch));
}

/* Public API {{{1 */

GtkWidget *
gtk_color_swatch_new (void)
{
  return (GtkWidget *) g_object_new (GTK_TYPE_COLOR_SWATCH, NULL);
}

static GdkContentProvider *
gtk_color_swatch_drag_prepare (GtkDragSource  *source,
                               double          x,
                               double          y,
                               GtkColorSwatch *swatch)
{
  return gdk_content_provider_new_typed (GDK_TYPE_RGBA, &swatch->color);
}

void
gtk_color_swatch_set_rgba (GtkColorSwatch *swatch,
                           const GdkRGBA  *color)
{
  swatch->has_color = TRUE;
  swatch->color = *color;
  if (swatch->source)
    gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (swatch->source), GTK_PHASE_CAPTURE);

  if (INTENSITY (swatch->color.red, swatch->color.green, swatch->color.blue) > 0.5)
    {
      gtk_widget_add_css_class (GTK_WIDGET (swatch), "light");
      gtk_widget_remove_css_class (GTK_WIDGET (swatch), "dark");
    }
  else
    {
      gtk_widget_add_css_class (GTK_WIDGET (swatch), "dark");
      gtk_widget_remove_css_class (GTK_WIDGET (swatch), "light");
    }

  gtk_widget_queue_draw (GTK_WIDGET (swatch));
  g_object_notify (G_OBJECT (swatch), "rgba");
}

gboolean
gtk_color_swatch_get_rgba (GtkColorSwatch *swatch,
                           GdkRGBA        *color)
{
  if (swatch->has_color)
    {
      color->red = swatch->color.red;
      color->green = swatch->color.green;
      color->blue = swatch->color.blue;
      color->alpha = swatch->color.alpha;
      return TRUE;
    }
  else
    {
      color->red = 1.0;
      color->green = 1.0;
      color->blue = 1.0;
      color->alpha = 1.0;
      return FALSE;
    }
}

void
gtk_color_swatch_set_icon (GtkColorSwatch *swatch,
                           const char     *icon)
{
  swatch->icon = g_strdup (icon);
  update_icon (swatch);
  gtk_widget_queue_draw (GTK_WIDGET (swatch));
}

void
gtk_color_swatch_set_can_drop (GtkColorSwatch *swatch,
                               gboolean        can_drop)
{
  if (can_drop == (swatch->dest != NULL))
    return;

  if (can_drop && !swatch->dest)
    {
      swatch->dest = gtk_drop_target_new (GDK_TYPE_RGBA, GDK_ACTION_COPY);
      gtk_drop_target_set_preload (swatch->dest, TRUE);
      g_signal_connect (swatch->dest, "drop", G_CALLBACK (swatch_drag_drop), swatch);
      g_signal_connect_swapped (swatch->dest, "notify::value", G_CALLBACK (gtk_widget_queue_draw), swatch);
      gtk_widget_add_controller (GTK_WIDGET (swatch), GTK_EVENT_CONTROLLER (swatch->dest));
    }
  if (!can_drop && swatch->dest)
    {
      gtk_widget_remove_controller (GTK_WIDGET (swatch), GTK_EVENT_CONTROLLER (swatch->dest));
      swatch->dest = NULL;
    }

  g_object_notify (G_OBJECT (swatch), "can-drop");
}

void
gtk_color_swatch_set_can_drag (GtkColorSwatch *swatch,
                               gboolean        can_drag)
{
  if (can_drag == (swatch->source != NULL))
    return;

  if (can_drag && !swatch->source)
    {
      swatch->source = gtk_drag_source_new ();
      g_signal_connect (swatch->source, "prepare", G_CALLBACK (gtk_color_swatch_drag_prepare), swatch);
      gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (swatch->source),
                                                  swatch->has_color ? GTK_PHASE_CAPTURE : GTK_PHASE_NONE);
      gtk_widget_add_controller (GTK_WIDGET (swatch), GTK_EVENT_CONTROLLER (swatch->source));
    }
  if (!can_drag && swatch->source)
    {
      gtk_widget_remove_controller (GTK_WIDGET (swatch), GTK_EVENT_CONTROLLER (swatch->source));
      swatch->source = NULL;
    }

  g_object_notify (G_OBJECT (swatch), "can-drag");
}

void
gtk_color_swatch_set_use_alpha (GtkColorSwatch *swatch,
                                gboolean        use_alpha)
{
  swatch->use_alpha = use_alpha;
  gtk_widget_queue_draw (GTK_WIDGET (swatch));
}

void
gtk_color_swatch_set_selectable (GtkColorSwatch *swatch,
                                 gboolean selectable)
{
  if (selectable == swatch->selectable)
    return;

  swatch->selectable = selectable;

  update_accessible_properties (swatch);
  g_object_notify (G_OBJECT (swatch), "selectable");
}

gboolean
gtk_color_swatch_get_selectable (GtkColorSwatch *swatch)
{
  return swatch->selectable;
}

/* vim:set foldmethod=marker: */
