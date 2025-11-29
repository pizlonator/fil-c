/*
 * Copyright (c) 2020 Alexander Mikhaylenko <alexm@gnome.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"

#include "gtkwindowhandle.h"

#include "gtkbinlayout.h"
#include "gtkbox.h"
#include "gtkbuildable.h"
#include "gtkdragsourceprivate.h"
#include "gtkgestureclick.h"
#include "gtkgesturedrag.h"
#include "gtkgestureprivate.h"
#include <glib/gi18n-lib.h>
#include "gtkmodelbuttonprivate.h"
#include "gtknative.h"
#include "gtkpopovermenuprivate.h"
#include "gtkprivate.h"
#include "gtkseparator.h"
#include "gtkwidgetprivate.h"


/**
 * GtkWindowHandle:
 *
 * `GtkWindowHandle` is a titlebar area widget.
 *
 * When added into a window, it can be dragged to move the window, and handles
 * right click, double click and middle click as expected of a titlebar.
 *
 * # CSS nodes
 *
 * `GtkWindowHandle` has a single CSS node with the name `windowhandle`.
 *
 * # Accessibility
 *
 * Until GTK 4.10, `GtkWindowHandle` used the `GTK_ACCESSIBLE_ROLE_GROUP` role.
 *
 * Starting from GTK 4.12, `GtkWindowHandle` uses the `GTK_ACCESSIBLE_ROLE_GENERIC` role.
 */

struct _GtkWindowHandle {
  GtkWidget parent_instance;

  GtkGesture *click_gesture;
  GtkGesture *drag_gesture;

  GtkWidget *child;
  GtkWidget *fallback_menu;
};

enum {
  PROP_0,
  PROP_CHILD,
  LAST_PROP
};

static GParamSpec *props[LAST_PROP] = { NULL, };

static void gtk_window_handle_buildable_iface_init (GtkBuildableIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkWindowHandle, gtk_window_handle, GTK_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE, gtk_window_handle_buildable_iface_init))

static void
lower_window (GtkWindowHandle *self)
{
  GdkSurface *surface =
    gtk_native_get_surface (gtk_widget_get_native (GTK_WIDGET (self)));

  gdk_toplevel_lower (GDK_TOPLEVEL (surface));
}

static GtkWindow *
get_window (GtkWindowHandle *self)
{
  GtkRoot *root = gtk_widget_get_root (GTK_WIDGET (self));

  if (GTK_IS_WINDOW (root))
    return GTK_WINDOW (root);

  return NULL;
}

static void
restore_window_clicked (GtkModelButton  *button,
                        GtkWindowHandle *self)
{
  GtkWindow *window = get_window (self);

  if (!window)
    return;

  if (gtk_window_is_maximized (window))
    gtk_window_unmaximize (window);
}

static void
minimize_window_clicked (GtkModelButton  *button,
                         GtkWindowHandle *self)
{
  GtkWindow *window = get_window (self);

  if (!window)
    return;

  /* Turns out, we can't minimize a maximized window */
  if (gtk_window_is_maximized (window))
    gtk_window_unmaximize (window);

  gtk_window_minimize (window);
}

static void
maximize_window_clicked (GtkModelButton  *button,
                         GtkWindowHandle *self)
{
  GtkWindow *window = get_window (self);

  if (window)
    gtk_window_maximize (window);
}

static void
close_window_clicked (GtkModelButton  *button,
                      GtkWindowHandle *self)
{
  GtkWindow *window = get_window (self);

  if (window)
    gtk_window_close (window);
}

static void
popup_menu_closed (GtkPopover      *popover,
                   GtkWindowHandle *self)
{
  g_clear_pointer (&self->fallback_menu, gtk_widget_unparent);
}

static void
do_popup_fallback (GtkWindowHandle *self,
                   GdkEvent        *event)
{
  GdkRectangle rect = { 0, 0, 1, 1 };
  GdkDevice *device;
  GdkSeat *seat;
  GtkWidget *box, *menuitem;
  GtkWindow *window;
  gboolean maximized, resizable, deletable;

  g_clear_pointer (&self->fallback_menu, gtk_widget_unparent);

  window = get_window (self);

  if (window)
    {
      maximized = gtk_window_is_maximized (window);
      resizable = gtk_window_get_resizable (window);
      deletable = gtk_window_get_deletable (window);
    }
  else
    {
      maximized = FALSE;
      resizable = FALSE;
      deletable = FALSE;
    }

  self->fallback_menu = gtk_popover_menu_new ();
  gtk_widget_set_parent (self->fallback_menu, GTK_WIDGET (self));

  gtk_popover_set_has_arrow (GTK_POPOVER (self->fallback_menu), FALSE);
  gtk_widget_set_halign (self->fallback_menu, GTK_ALIGN_START);


  device = gdk_event_get_device (event);
  seat = gdk_event_get_seat (event);

  if (device == gdk_seat_get_keyboard (seat))
    device = gdk_seat_get_pointer (seat);

  if (device)
    {
      GtkNative *native;
      GdkSurface *surface;
      double px, py;
      double nx, ny;
      graphene_point_t p;

      native = gtk_widget_get_native (GTK_WIDGET (self));
      surface = gtk_native_get_surface (native);
      gdk_surface_get_device_position (surface, device, &px, &py, NULL);
      gtk_native_get_surface_transform (native, &nx, &ny);

      if (!gtk_widget_compute_point (GTK_WIDGET (gtk_widget_get_native (GTK_WIDGET (self))),
                                     GTK_WIDGET (self),
                                     &GRAPHENE_POINT_INIT (px - nx, py - ny),
                                     &p))
        graphene_point_init (&p, 0, 0);

      rect.x = p.x;
      rect.y = p.y;
    }

  gtk_popover_set_pointing_to (GTK_POPOVER (self->fallback_menu), &rect);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_popover_menu_add_submenu (GTK_POPOVER_MENU (self->fallback_menu), box, "main");

  menuitem = gtk_model_button_new ();
  g_object_set (menuitem, "text", _("Restore"), NULL);
  gtk_widget_set_sensitive (menuitem, maximized && resizable);
  g_signal_connect (G_OBJECT (menuitem), "clicked",
                    G_CALLBACK (restore_window_clicked), self);
  gtk_box_append (GTK_BOX (box), menuitem);

  menuitem = gtk_model_button_new ();
  g_object_set (menuitem, "text", _("Minimize"), NULL);
  g_signal_connect (G_OBJECT (menuitem), "clicked",
                    G_CALLBACK (minimize_window_clicked), self);
  gtk_box_append (GTK_BOX (box), menuitem);

  menuitem = gtk_model_button_new ();
  g_object_set (menuitem, "text", _("Maximize"), NULL);
  gtk_widget_set_sensitive (menuitem, resizable && !maximized);
  g_signal_connect (G_OBJECT (menuitem), "clicked",
                    G_CALLBACK (maximize_window_clicked), self);
  gtk_box_append (GTK_BOX (box), menuitem);

  menuitem = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_box_append (GTK_BOX (box), menuitem);

  menuitem = gtk_model_button_new ();
  g_object_set (menuitem, "text", _("Close"), NULL);
  gtk_widget_set_sensitive (menuitem, deletable);
  g_signal_connect (G_OBJECT (menuitem), "clicked",
                    G_CALLBACK (close_window_clicked), self);
  gtk_box_append (GTK_BOX (box), menuitem);

  g_signal_connect (self->fallback_menu, "closed",
                    G_CALLBACK (popup_menu_closed), self);
  gtk_popover_popup (GTK_POPOVER (self->fallback_menu));
}

static void
do_popup (GtkWindowHandle *self,
          GtkGestureClick *gesture)
{
  GdkSurface *surface =
    gtk_native_get_surface (gtk_widget_get_native (GTK_WIDGET (self)));
  GdkEventSequence *sequence;
  GdkEvent *event;

  sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));
  event = gtk_gesture_get_last_event (GTK_GESTURE (gesture), sequence);
  if (!event)
    return;

  if (!gdk_toplevel_show_window_menu (GDK_TOPLEVEL (surface), event))
    do_popup_fallback (self, event);
}

static gboolean
perform_titlebar_action_fallback (GtkWindowHandle    *self,
                                  GtkGestureClick    *click_gesture,
                                  GdkTitlebarGesture  gesture)
{
  GtkSettings *settings;
  char *action = NULL;
  gboolean retval = TRUE;

  settings = gtk_widget_get_settings (GTK_WIDGET (self));
  switch (gesture)
    {
    case GDK_TITLEBAR_GESTURE_DOUBLE_CLICK:
      g_object_get (settings, "gtk-titlebar-double-click", &action, NULL);
      break;
    case GDK_TITLEBAR_GESTURE_MIDDLE_CLICK:
      g_object_get (settings, "gtk-titlebar-middle-click", &action, NULL);
      break;
    case GDK_TITLEBAR_GESTURE_RIGHT_CLICK:
      g_object_get (settings, "gtk-titlebar-right-click", &action, NULL);
      break;
    default:
      break;
    }

  if (action == NULL)
    retval = FALSE;
  else if (g_str_equal (action, "none"))
    retval = FALSE;
    /* treat all maximization variants the same */
  else if (g_str_has_prefix (action, "toggle-maximize"))
    gtk_widget_activate_action (GTK_WIDGET (self),
                                "window.toggle-maximized",
                                NULL);
  else if (g_str_equal (action, "lower"))
    lower_window (self);
  else if (g_str_equal (action, "minimize"))
    gtk_widget_activate_action (GTK_WIDGET (self),
                                "window.minimize",
                                NULL);
  else if (g_str_equal (action, "menu"))
    do_popup (self, click_gesture);
  else
    {
      g_warning ("Unsupported titlebar action %s", action);
      retval = FALSE;
    }

  g_free (action);

  return retval;
}

static gboolean
perform_titlebar_action (GtkWindowHandle *self,
			 GtkGestureClick *click_gesture,
                         guint            button,
                         int              n_press)
{
  GdkSurface *surface =
    gtk_native_get_surface (gtk_widget_get_native (GTK_WIDGET (self)));
  GdkTitlebarGesture gesture;

  switch (button)
    {
    case GDK_BUTTON_PRIMARY:
      if (n_press == 2)
        gesture = GDK_TITLEBAR_GESTURE_DOUBLE_CLICK;
      else
        return FALSE;
      break;
    case GDK_BUTTON_MIDDLE:
      gesture = GDK_TITLEBAR_GESTURE_MIDDLE_CLICK;
      break;
    case GDK_BUTTON_SECONDARY:
      gesture = GDK_TITLEBAR_GESTURE_RIGHT_CLICK;
      break;
    default:
      return FALSE;
    }

  if (gdk_toplevel_titlebar_gesture (GDK_TOPLEVEL (surface), gesture))
    return TRUE;

  return perform_titlebar_action_fallback (self, click_gesture, gesture);
}

static void
click_gesture_pressed_cb (GtkGestureClick *gesture,
                          int              n_press,
                          double           x,
                          double           y,
                          GtkWindowHandle *self)
{
  GtkWidget *widget;
  guint button;

  widget = GTK_WIDGET (self);
  button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));

  if (n_press > 1)
    gtk_gesture_set_state (self->drag_gesture, GTK_EVENT_SEQUENCE_DENIED);

  if (gdk_display_device_is_grabbed (gtk_widget_get_display (widget),
                                     gtk_gesture_get_device (GTK_GESTURE (gesture))))
    {
      gtk_gesture_set_state (self->drag_gesture, GTK_EVENT_SEQUENCE_DENIED);
      return;
    }

  switch (button)
    {
    case GDK_BUTTON_PRIMARY:
      if (n_press == 2)
        {
          perform_titlebar_action (self, gesture, button, n_press);
          gtk_gesture_set_state (GTK_GESTURE (gesture),
				 GTK_EVENT_SEQUENCE_CLAIMED);
        }
      break;

    case GDK_BUTTON_SECONDARY:
      if (perform_titlebar_action (self, gesture, button, n_press))
        gtk_gesture_set_state (GTK_GESTURE (gesture),
			       GTK_EVENT_SEQUENCE_CLAIMED);

      gtk_event_controller_reset (GTK_EVENT_CONTROLLER (gesture));
      gtk_event_controller_reset (GTK_EVENT_CONTROLLER (self->drag_gesture));
      break;

    case GDK_BUTTON_MIDDLE:
      if (perform_titlebar_action (self, gesture, button, n_press))
        gtk_gesture_set_state (GTK_GESTURE (gesture),
			       GTK_EVENT_SEQUENCE_CLAIMED);
      break;

    default:
      return;
    }
}

static void
drag_gesture_update_cb (GtkGestureDrag  *gesture,
                        double           offset_x,
                        double           offset_y,
                        GtkWindowHandle *self)
{
  if (gtk_drag_check_threshold_double (GTK_WIDGET (self), 0, 0, offset_x, offset_y))
    {
      double start_x, start_y;
      double native_x, native_y;
      graphene_point_t p;
      GtkNative *native;
      GdkSurface *surface;

      gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);

      gtk_gesture_drag_get_start_point (gesture, &start_x, &start_y);

      native = gtk_widget_get_native (GTK_WIDGET (self));

      if (!gtk_widget_compute_point (GTK_WIDGET (self),
                                     GTK_WIDGET (native),
                                     &GRAPHENE_POINT_INIT (start_x, start_y),
                                     &p))
        graphene_point_init (&p, start_x, start_y);

      gtk_native_get_surface_transform (native, &native_x, &native_y);
      p.x += native_x;
      p.y += native_y;

      if (GTK_IS_WINDOW (native))
        gtk_window_unfullscreen (GTK_WINDOW (native));

      surface = gtk_native_get_surface (native);
      if (GDK_IS_TOPLEVEL (surface))
        gdk_toplevel_begin_move (GDK_TOPLEVEL (surface),
                                 gtk_gesture_get_device (GTK_GESTURE (gesture)),
                                 gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture)),
                                 p.x, p.y,
                                 gtk_event_controller_get_current_event_time (GTK_EVENT_CONTROLLER (gesture)));

      gtk_event_controller_reset (GTK_EVENT_CONTROLLER (gesture));
      gtk_event_controller_reset (GTK_EVENT_CONTROLLER (self->click_gesture));
    }
}

static void
gtk_window_handle_unrealize (GtkWidget *widget)
{
  GtkWindowHandle *self = GTK_WINDOW_HANDLE (widget);

  g_clear_pointer (&self->fallback_menu, gtk_widget_unparent);

  GTK_WIDGET_CLASS (gtk_window_handle_parent_class)->unrealize (widget);
}

static void
gtk_window_handle_dispose (GObject *object)
{
  GtkWindowHandle *self = GTK_WINDOW_HANDLE (object);

  g_clear_pointer (&self->child, gtk_widget_unparent);

  G_OBJECT_CLASS (gtk_window_handle_parent_class)->dispose (object);
}

static void
gtk_window_handle_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GtkWindowHandle *self = GTK_WINDOW_HANDLE (object);

  switch (prop_id)
    {
    case PROP_CHILD:
      g_value_set_object (value, gtk_window_handle_get_child (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_window_handle_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GtkWindowHandle *self = GTK_WINDOW_HANDLE (object);

  switch (prop_id)
    {
    case PROP_CHILD:
      gtk_window_handle_set_child (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_window_handle_class_init (GtkWindowHandleClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gtk_window_handle_dispose;
  object_class->get_property = gtk_window_handle_get_property;
  object_class->set_property = gtk_window_handle_set_property;

  widget_class->unrealize = gtk_window_handle_unrealize;

  /**
   * GtkWindowHandle:child: (attributes org.gtk.Property.get=gtk_window_handle_get_child org.gtk.Property.set=gtk_window_handle_set_child)
   *
   * The child widget.
   */
  props[PROP_CHILD] =
      g_param_spec_object ("child", NULL, NULL,
                           GTK_TYPE_WIDGET,
                           GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_css_name (widget_class, I_("windowhandle"));
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_GENERIC);
}

static void
gtk_window_handle_init (GtkWindowHandle *self)
{
  self->click_gesture = gtk_gesture_click_new ();
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (self->click_gesture), 0);
  g_signal_connect (self->click_gesture, "pressed",
                    G_CALLBACK (click_gesture_pressed_cb), self);
  gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (self->click_gesture));

  self->drag_gesture = gtk_gesture_drag_new ();
  g_signal_connect (self->drag_gesture, "drag-update",
                    G_CALLBACK (drag_gesture_update_cb), self);
  gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (self->drag_gesture));
}

static GtkBuildableIface *parent_buildable_iface;

static void
gtk_window_handle_buildable_add_child (GtkBuildable *buildable,
                                       GtkBuilder   *builder,
                                       GObject      *child,
                                       const char   *type)
{
  if (GTK_IS_WIDGET (child))
    gtk_window_handle_set_child (GTK_WINDOW_HANDLE (buildable), GTK_WIDGET (child));
  else
    parent_buildable_iface->add_child (buildable, builder, child, type);
}

static void
gtk_window_handle_buildable_iface_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);

  iface->add_child = gtk_window_handle_buildable_add_child;
}

/**
 * gtk_window_handle_new:
 *
 * Creates a new `GtkWindowHandle`.
 *
 * Returns: a new `GtkWindowHandle`.
 */
GtkWidget *
gtk_window_handle_new (void)
{
  return g_object_new (GTK_TYPE_WINDOW_HANDLE, NULL);
}

/**
 * gtk_window_handle_get_child: (attributes org.gtk.Method.get_property=child)
 * @self: a `GtkWindowHandle`
 *
 * Gets the child widget of @self.
 *
 * Returns: (nullable) (transfer none): the child widget of @self
 */
GtkWidget *
gtk_window_handle_get_child (GtkWindowHandle *self)
{
  g_return_val_if_fail (GTK_IS_WINDOW_HANDLE (self), NULL);

  return self->child;
}

/**
 * gtk_window_handle_set_child: (attributes org.gtk.Method.set_property=child)
 * @self: a `GtkWindowHandle`
 * @child: (nullable): the child widget
 *
 * Sets the child widget of @self.
 */
void
gtk_window_handle_set_child (GtkWindowHandle *self,
                             GtkWidget       *child)
{
  g_return_if_fail (GTK_IS_WINDOW_HANDLE (self));
  g_return_if_fail (child == NULL || self->child == child || gtk_widget_get_parent (child) == NULL);

  if (self->child == child)
    return;

  g_clear_pointer (&self->child, gtk_widget_unparent);

  self->child = child;

  if (child)
    gtk_widget_set_parent (child, GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_CHILD]);
}
