/* GTK - The GIMP Toolkit
 * Copyright © 2012 Carlos Garnacho <carlosg@gnome.org>
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

#include "gtktexthandleprivate.h"

#include "gtkbinlayout.h"
#include "gtkcssboxesimplprivate.h"
#include "gtkcssnodeprivate.h"
#include "gtkgesturedrag.h"
#include "gtkgizmoprivate.h"
#include "gtkmarshalers.h"
#include "gdk/gdkmarshalers.h"
#include "gtknativeprivate.h"
#include "gtkprivatetypebuiltins.h"
#include "gtkrendericonprivate.h"
#include "gtkwidgetprivate.h"
#include "gtkwindowprivate.h"
#include "gtkprivate.h"

#include "gdk/gdksurfaceprivate.h"

enum {
  DRAG_STARTED,
  HANDLE_DRAGGED,
  DRAG_FINISHED,
  LAST_SIGNAL
};

struct _GtkTextHandle
{
  GtkWidget parent_instance;

  GdkSurface *surface;
  GskRenderer *renderer;
  GtkEventController *controller;
  GtkWidget *controller_widget;
  GtkWidget *contents;

  GdkRectangle pointing_to;
  GtkBorder border;
  int dx;
  int dy;
  guint role : 2;
  guint dragged : 1;
  guint mode_visible : 1;
  guint user_visible : 1;
  guint has_point : 1;
};

static void gtk_text_handle_native_interface_init (GtkNativeInterface *iface);

static void handle_drag_begin (GtkGestureDrag *gesture,
                               double          x,
                               double          y,
                               GtkTextHandle  *handle);
static void handle_drag_update (GtkGestureDrag *gesture,
                                double          offset_x,
                                double          offset_y,
                                GtkWidget      *widget);
static void handle_drag_end (GtkGestureDrag *gesture,
                             double          offset_x,
                             double          offset_y,
                             GtkTextHandle  *handle);

G_DEFINE_TYPE_WITH_CODE (GtkTextHandle, gtk_text_handle, GTK_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_NATIVE,
                                                gtk_text_handle_native_interface_init))

static guint signals[LAST_SIGNAL] = { 0 };

static GdkSurface *
gtk_text_handle_native_get_surface (GtkNative *native)
{
  return GTK_TEXT_HANDLE (native)->surface;
}

static GskRenderer *
gtk_text_handle_native_get_renderer (GtkNative *native)
{
  return GTK_TEXT_HANDLE (native)->renderer;
}

static void
gtk_text_handle_native_get_surface_transform (GtkNative *native,
                                              double    *x,
                                              double    *y)
{
  GtkCssBoxes css_boxes;
  const graphene_rect_t *margin_rect;

  gtk_css_boxes_init (&css_boxes, GTK_WIDGET (native));
  margin_rect = gtk_css_boxes_get_margin_rect (&css_boxes);

  *x = - margin_rect->origin.x;
  *y = - margin_rect->origin.y;
}

static void
gtk_text_handle_get_padding (GtkTextHandle *handle,
                             GtkBorder     *padding)
{
  GtkWidget *widget = GTK_WIDGET (handle);
  GtkCssStyle *style = gtk_css_node_get_style (gtk_widget_get_css_node (widget));

  padding->left = _gtk_css_number_value_get (style->size->padding_left, 100);
  padding->right = _gtk_css_number_value_get (style->size->padding_right, 100);
  padding->top = _gtk_css_number_value_get (style->size->padding_top, 100);
  padding->bottom = _gtk_css_number_value_get (style->size->padding_bottom, 100);
}

static void
gtk_text_handle_present_surface (GtkTextHandle *handle)
{
  GtkWidget *widget = GTK_WIDGET (handle);
  GdkPopupLayout *layout;
  GdkRectangle rect;
  GtkRequisition req;
  GtkWidget *parent;
  GtkNative *native;
  graphene_point_t point = GRAPHENE_POINT_INIT (handle->pointing_to.x, handle->pointing_to.y);
  graphene_point_t transformed;
  double nx, ny;

  gtk_widget_get_preferred_size (widget, NULL, &req);
  gtk_text_handle_get_padding (handle, &handle->border);

  parent = gtk_widget_get_parent (widget);

  native = gtk_widget_get_native (parent);
  gtk_native_get_surface_transform (native, &nx, &ny);

  if (!gtk_widget_compute_point (parent, GTK_WIDGET (native),
                                 &point, &transformed))
    transformed = point;

  rect.x = (int)(transformed.x + nx);
  rect.y = (int)(transformed.y + ny) + handle->pointing_to.height - handle->border.top;

  rect.width = req.width - handle->border.left - handle->border.right;
  rect.height = 1;

  if (handle->role == GTK_TEXT_HANDLE_ROLE_CURSOR)
    rect.x -= rect.width / 2;
  else if ((handle->role == GTK_TEXT_HANDLE_ROLE_SELECTION_END &&
            gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL) ||
           (handle->role == GTK_TEXT_HANDLE_ROLE_SELECTION_START &&
            gtk_widget_get_direction (widget) != GTK_TEXT_DIR_RTL))
    rect.x -= rect.width;

  layout = gdk_popup_layout_new (&rect,
                                 GDK_GRAVITY_SOUTH,
                                 GDK_GRAVITY_NORTH);
  gdk_popup_layout_set_anchor_hints (layout,
                                     GDK_ANCHOR_FLIP_Y | GDK_ANCHOR_SLIDE_X);

  gdk_popup_present (GDK_POPUP (handle->surface),
                     MAX (req.width, 1),
                     MAX (req.height, 1),
                     layout);
  gdk_popup_layout_unref (layout);
}

void
gtk_text_handle_present (GtkTextHandle *handle)
{
  GtkWidget *widget = GTK_WIDGET (handle);

  if (!_gtk_widget_get_alloc_needed (widget))
    gtk_widget_ensure_allocate (widget);
  else if (gtk_widget_get_visible (widget))
    gtk_text_handle_present_surface (handle);
}

static void
gtk_text_handle_native_layout (GtkNative *native,
                               int        width,
                               int        height)
{
  GtkWidget *widget = GTK_WIDGET (native);

  if (_gtk_widget_get_alloc_needed (widget))
    gtk_widget_allocate (widget, width, height, -1, NULL);
  else
    gtk_widget_ensure_allocate (widget);
}

static void
gtk_text_handle_native_interface_init (GtkNativeInterface *iface)
{
  iface->get_surface = gtk_text_handle_native_get_surface;
  iface->get_renderer = gtk_text_handle_native_get_renderer;
  iface->get_surface_transform = gtk_text_handle_native_get_surface_transform;
  iface->layout = gtk_text_handle_native_layout;
}

static gboolean
surface_render (GdkSurface     *surface,
                cairo_region_t *region,
                GtkTextHandle  *handle)
{
  gtk_widget_render (GTK_WIDGET (handle), surface, region);
  return TRUE;
}

static void
surface_mapped_changed (GtkWidget *widget)
{
  GtkTextHandle *handle = GTK_TEXT_HANDLE (widget);

  gtk_widget_set_visible (widget, gdk_surface_get_mapped (handle->surface));
}

static void
gtk_text_handle_snapshot (GtkWidget   *widget,
                          GtkSnapshot *snapshot)
{
  GtkCssStyle *style = gtk_css_node_get_style (gtk_widget_get_css_node (widget));

  gtk_css_style_snapshot_icon (style,
                               snapshot,
                               gtk_widget_get_width (widget),
                               gtk_widget_get_height (widget));

  GTK_WIDGET_CLASS (gtk_text_handle_parent_class)->snapshot (widget, snapshot);
}

static void
gtk_text_handle_realize (GtkWidget *widget)
{
  GtkTextHandle *handle = GTK_TEXT_HANDLE (widget);
  GdkSurface *parent_surface;
  GtkWidget *parent;

  parent = gtk_widget_get_parent (widget);
  parent_surface = gtk_native_get_surface (gtk_widget_get_native (parent));

  handle->surface = gdk_surface_new_popup (parent_surface, FALSE);
  gdk_surface_set_widget (handle->surface, widget);
  gdk_surface_set_input_region (handle->surface, cairo_region_create ());

  g_signal_connect_swapped (handle->surface, "notify::mapped",
                            G_CALLBACK (surface_mapped_changed), widget);
  g_signal_connect (handle->surface, "render", G_CALLBACK (surface_render), widget);

  GTK_WIDGET_CLASS (gtk_text_handle_parent_class)->realize (widget);

  handle->renderer = gsk_renderer_new_for_surface (handle->surface);

  gtk_native_realize (GTK_NATIVE (handle));
}

static void
gtk_text_handle_unrealize (GtkWidget *widget)
{
  GtkTextHandle *handle = GTK_TEXT_HANDLE (widget);

  gtk_native_unrealize (GTK_NATIVE (handle));

  GTK_WIDGET_CLASS (gtk_text_handle_parent_class)->unrealize (widget);

  gsk_renderer_unrealize (handle->renderer);
  g_clear_object (&handle->renderer);

  g_signal_handlers_disconnect_by_func (handle->surface, surface_render, widget);
  g_signal_handlers_disconnect_by_func (handle->surface, surface_mapped_changed, widget);

  gdk_surface_set_widget (handle->surface, NULL);
  g_clear_pointer (&handle->surface, gdk_surface_destroy);
}

static void
text_handle_set_up_gesture (GtkTextHandle *handle)
{
  GtkNative *native;

  /* The drag gesture is hooked on the parent native */
  native = gtk_widget_get_native (gtk_widget_get_parent (GTK_WIDGET (handle)));
  handle->controller_widget = GTK_WIDGET (native);

  handle->controller = GTK_EVENT_CONTROLLER (gtk_gesture_drag_new ());
  gtk_event_controller_set_propagation_phase (handle->controller,
                                              GTK_PHASE_CAPTURE);
  g_signal_connect (handle->controller, "drag-begin",
                    G_CALLBACK (handle_drag_begin), handle);
  g_signal_connect (handle->controller, "drag-update",
                    G_CALLBACK (handle_drag_update), handle);
  g_signal_connect (handle->controller, "drag-end",
                    G_CALLBACK (handle_drag_end), handle);

  gtk_widget_add_controller (handle->controller_widget, handle->controller);
}

static void
gtk_text_handle_map (GtkWidget *widget)
{
  GtkTextHandle *handle = GTK_TEXT_HANDLE (widget);

  GTK_WIDGET_CLASS (gtk_text_handle_parent_class)->map (widget);

  if (handle->has_point)
    {
      gtk_text_handle_present_surface (handle);
      text_handle_set_up_gesture (handle);
    }
}

static void
gtk_text_handle_unmap (GtkWidget *widget)
{
  GtkTextHandle *handle = GTK_TEXT_HANDLE (widget);

  GTK_WIDGET_CLASS (gtk_text_handle_parent_class)->unmap (widget);
  gdk_surface_hide (handle->surface);

  if (handle->controller_widget)
    {
      gtk_widget_remove_controller (handle->controller_widget,
                                    handle->controller);
      handle->controller_widget = NULL;
      handle->controller = NULL;
    }
}

static void
gtk_text_handle_dispose (GObject *object)
{
  GtkTextHandle *handle = GTK_TEXT_HANDLE (object);

  g_clear_pointer (&handle->contents, gtk_widget_unparent);

  G_OBJECT_CLASS (gtk_text_handle_parent_class)->dispose (object);
}

static void
gtk_text_handle_class_init (GtkTextHandleClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gtk_text_handle_dispose;

  widget_class->snapshot = gtk_text_handle_snapshot;
  widget_class->realize = gtk_text_handle_realize;
  widget_class->unrealize = gtk_text_handle_unrealize;
  widget_class->map = gtk_text_handle_map;
  widget_class->unmap = gtk_text_handle_unmap;

  signals[HANDLE_DRAGGED] =
    g_signal_new (I_("handle-dragged"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST, 0,
                  NULL, NULL,
                  _gdk_marshal_VOID__INT_INT,
                  G_TYPE_NONE, 2,
                  G_TYPE_INT, G_TYPE_INT);
  g_signal_set_va_marshaller (signals[HANDLE_DRAGGED],
                              G_OBJECT_CLASS_TYPE (object_class),
                              _gdk_marshal_VOID__INT_INTv);

  signals[DRAG_STARTED] =
    g_signal_new (I_("drag-started"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST, 0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0, G_TYPE_NONE);

  signals[DRAG_FINISHED] =
    g_signal_new (I_("drag-finished"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST, 0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0, G_TYPE_NONE);

  gtk_widget_class_set_css_name (widget_class, I_("cursor-handle"));
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
}

/* Relative to pointing_to x/y */
static void
handle_get_input_extents (GtkTextHandle *handle,
                          GtkBorder     *border)
{
  GtkWidget *widget = GTK_WIDGET (handle);

  if (handle->role == GTK_TEXT_HANDLE_ROLE_CURSOR)
    {
      border->left = (-gtk_widget_get_width (widget) / 2) - handle->border.left;
      border->right = (gtk_widget_get_width (widget) / 2) + handle->border.right;
    }
  else if ((handle->role == GTK_TEXT_HANDLE_ROLE_SELECTION_END &&
            gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL) ||
           (handle->role == GTK_TEXT_HANDLE_ROLE_SELECTION_START &&
            gtk_widget_get_direction (widget) != GTK_TEXT_DIR_RTL))
    {
      border->left = -gtk_widget_get_width (widget) - handle->border.left;
      border->right = handle->border.right;
    }
  else
    {
      border->left = -handle->border.left;
      border->right = gtk_widget_get_width (widget) + handle->border.right;
    }

  border->top = - handle->border.top;
  border->bottom = gtk_widget_get_height (widget) + handle->border.bottom;
}

static void
handle_drag_begin (GtkGestureDrag *gesture,
                   double          x,
                   double          y,
                   GtkTextHandle  *handle)
{
  GtkBorder input_extents;
  graphene_point_t p;

  x -= handle->pointing_to.x;
  y -= handle->pointing_to.y;

  /* Figure out if the coordinates fall into the handle input area, coordinates
   * are relative to the parent widget.
   */
  handle_get_input_extents (handle, &input_extents);
  if (!gtk_widget_compute_point (handle->controller_widget,
                                 gtk_widget_get_parent (GTK_WIDGET (handle)),
                                 &GRAPHENE_POINT_INIT (x, y),
                                 &p))
    graphene_point_init (&p, x, y);

  if (p.x < input_extents.left || p.x >= input_extents.right ||
      p.y < input_extents.top || p.y >= input_extents.bottom)
    {
      gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_DENIED);
      return;
    }

  gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
  /* Store untranslated coordinates here, so ::update does not need
   * an extra translation
   */
  handle->dx = x;
  handle->dy = y;
  handle->dragged = TRUE;
  g_signal_emit (handle, signals[DRAG_STARTED], 0);
}

static void
handle_drag_update (GtkGestureDrag *gesture,
                    double          offset_x,
                    double          offset_y,
                    GtkWidget      *widget)
{
  GtkTextHandle *handle = GTK_TEXT_HANDLE (widget);
  double start_x, start_y;
  int x, y;

  gtk_gesture_drag_get_start_point (gesture, &start_x, &start_y);

  x = start_x + offset_x - handle->dx;
  y = start_y + offset_y - handle->dy + (handle->pointing_to.height / 2);
  g_signal_emit (widget, signals[HANDLE_DRAGGED], 0, x, y);
}

static void
handle_drag_end (GtkGestureDrag *gesture,
                 double          offset_x,
                 double          offset_y,
                 GtkTextHandle  *handle)
{
  GdkEventSequence *sequence;
  GtkEventSequenceState state;

  sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));
  state = gtk_gesture_get_sequence_state (GTK_GESTURE (gesture), sequence);

  if (state == GTK_EVENT_SEQUENCE_CLAIMED)
    g_signal_emit (handle, signals[DRAG_FINISHED], 0);

  handle->dragged = FALSE;
}

static void
gtk_text_handle_update_for_role (GtkTextHandle *handle)
{
  GtkWidget *widget = GTK_WIDGET (handle);

  if (handle->role == GTK_TEXT_HANDLE_ROLE_CURSOR)
    {
      gtk_widget_remove_css_class (widget, "top");
      gtk_widget_add_css_class (widget, "bottom");
      gtk_widget_add_css_class (widget, "insertion-cursor");
    }
  else if (handle->role == GTK_TEXT_HANDLE_ROLE_SELECTION_END)
    {
      gtk_widget_remove_css_class (widget, "top");
      gtk_widget_add_css_class (widget, "bottom");
      gtk_widget_remove_css_class (widget, "insertion-cursor");
    }
  else if (handle->role == GTK_TEXT_HANDLE_ROLE_SELECTION_START)
    {
      gtk_widget_add_css_class (widget, "top");
      gtk_widget_remove_css_class (widget, "bottom");
      gtk_widget_remove_css_class (widget, "insertion-cursor");
    }

  gtk_widget_queue_draw (widget);
}

static void
gtk_text_handle_init (GtkTextHandle *handle)
{
  handle->contents = gtk_gizmo_new ("contents", NULL, NULL, NULL, NULL, NULL, NULL);
  gtk_widget_set_can_target (handle->contents, FALSE);
  gtk_widget_set_parent (handle->contents, GTK_WIDGET (handle));

  gtk_text_handle_update_for_role (handle);
}

GtkTextHandle *
gtk_text_handle_new (GtkWidget *parent)
{
  GtkTextHandle *handle;

  handle = g_object_new (GTK_TYPE_TEXT_HANDLE, NULL);
  gtk_widget_set_parent (GTK_WIDGET (handle), parent);

  return handle;
}

void
gtk_text_handle_set_role (GtkTextHandle     *handle,
                          GtkTextHandleRole  role)
{
  g_return_if_fail (GTK_IS_TEXT_HANDLE (handle));

  if (handle->role == role)
    return;

  handle->role = role;
  gtk_text_handle_update_for_role (handle);

  if (gtk_widget_get_visible (GTK_WIDGET (handle)))
    {
      if (handle->has_point)
        gtk_text_handle_present_surface (handle);
    }
}

GtkTextHandleRole
gtk_text_handle_get_role (GtkTextHandle *handle)
{
  g_return_val_if_fail (GTK_IS_TEXT_HANDLE (handle), GTK_TEXT_HANDLE_ROLE_CURSOR);

  return handle->role;
}

void
gtk_text_handle_set_position (GtkTextHandle      *handle,
                              const GdkRectangle *rect)
{
  g_return_if_fail (GTK_IS_TEXT_HANDLE (handle));

  if (handle->pointing_to.x == rect->x &&
      handle->pointing_to.y == rect->y &&
      handle->pointing_to.width == rect->width &&
      handle->pointing_to.height == rect->height)
    return;

  handle->pointing_to = *rect;
  handle->has_point = TRUE;

  if (gtk_widget_is_visible (GTK_WIDGET (handle)))
    gtk_text_handle_present_surface (handle);
}

gboolean
gtk_text_handle_get_is_dragged (GtkTextHandle *handle)
{
  g_return_val_if_fail (GTK_IS_TEXT_HANDLE (handle), FALSE);

  return handle->dragged;
}
