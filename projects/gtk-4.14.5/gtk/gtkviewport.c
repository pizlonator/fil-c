/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"

#include "gtkviewport.h"

#include "gtkadjustmentprivate.h"
#include "gtkmarshalers.h"
#include "gtkprivate.h"
#include "gtkscrollable.h"
#include "gtkscrollinfoprivate.h"
#include "gtktypebuiltins.h"
#include "gtkwidgetprivate.h"
#include "gtkbuildable.h"
#include "gtktext.h"

#include <math.h>

/**
 * GtkViewport:
 *
 * `GtkViewport` implements scrollability for widgets that lack their
 * own scrolling capabilities.
 *
 * Use `GtkViewport` to scroll child widgets such as `GtkGrid`,
 * `GtkBox`, and so on.
 *
 * The `GtkViewport` will start scrolling content only if allocated
 * less than the child widget’s minimum size in a given orientation.
 *
 * # CSS nodes
 *
 * `GtkViewport` has a single CSS node with name `viewport`.
 *
 * # Accessibility
 *
 * Until GTK 4.10, `GtkViewport` used the `GTK_ACCESSIBLE_ROLE_GROUP` role.
 *
 * Starting from GTK 4.12, `GtkViewport` uses the `GTK_ACCESSIBLE_ROLE_GENERIC` role.
 */

typedef struct _GtkViewportPrivate       GtkViewportPrivate;
typedef struct _GtkViewportClass         GtkViewportClass;

struct _GtkViewport
{
  GtkWidget parent_instance;

  GtkWidget *child;

  GtkAdjustment *adjustment[2];
  GtkScrollablePolicy scroll_policy[2];
  guint scroll_to_focus : 1;

  gulong focus_handler;
};

struct _GtkViewportClass
{
  GtkWidgetClass parent_class;
};

enum {
  PROP_0,
  PROP_HADJUSTMENT,
  PROP_VADJUSTMENT,
  PROP_HSCROLL_POLICY,
  PROP_VSCROLL_POLICY,
  PROP_SCROLL_TO_FOCUS,
  PROP_CHILD
};


static void gtk_viewport_set_property             (GObject         *object,
                                                   guint            prop_id,
                                                   const GValue    *value,
                                                   GParamSpec      *pspec);
static void gtk_viewport_get_property             (GObject         *object,
                                                   guint            prop_id,
                                                   GValue          *value,
                                                   GParamSpec      *pspec);
static void gtk_viewport_dispose                  (GObject         *object);
static void gtk_viewport_size_allocate            (GtkWidget       *widget,
                                                   int              width,
                                                   int              height,
                                                   int              baseline);

static void gtk_viewport_adjustment_value_changed (GtkAdjustment    *adjustment,
                                                   gpointer          data);
static void viewport_set_adjustment               (GtkViewport      *viewport,
                                                   GtkOrientation    orientation,
                                                   GtkAdjustment    *adjustment);

static void setup_focus_change_handler (GtkViewport *viewport);
static void clear_focus_change_handler (GtkViewport *viewport);

static void gtk_viewport_buildable_init (GtkBuildableIface *iface);


G_DEFINE_TYPE_WITH_CODE (GtkViewport, gtk_viewport, GTK_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                gtk_viewport_buildable_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_SCROLLABLE, NULL))

static GtkBuildableIface *parent_buildable_iface;

static void
gtk_viewport_buildable_add_child (GtkBuildable *buildable,
                                  GtkBuilder   *builder,
                                  GObject      *child,
                                  const char   *type)
{
  if (GTK_IS_WIDGET (child))
    gtk_viewport_set_child (GTK_VIEWPORT (buildable), GTK_WIDGET (child));
  else
    parent_buildable_iface->add_child (buildable, builder, child, type);
}

static void
gtk_viewport_buildable_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);

  iface->add_child = gtk_viewport_buildable_add_child;
}

static void
viewport_set_adjustment_values (GtkViewport    *viewport,
                                GtkOrientation  orientation,
                                int             viewport_size,
                                int             child_size)
{
  GtkAdjustment *adjustment;
  double upper, value;

  adjustment = viewport->adjustment[orientation];
  upper = child_size;
  value = gtk_adjustment_get_value (adjustment);

  /* We clamp to the left in RTL mode */
  if (orientation == GTK_ORIENTATION_HORIZONTAL &&
      _gtk_widget_get_direction (GTK_WIDGET (viewport)) == GTK_TEXT_DIR_RTL)
    {
      double dist = gtk_adjustment_get_upper (adjustment)
                     - value
                     - gtk_adjustment_get_page_size (adjustment);
      value = upper - dist - viewport_size;
    }

  gtk_adjustment_configure (adjustment,
                            value,
                            0,
                            upper,
                            viewport_size * 0.1,
                            viewport_size * 0.9,
                            viewport_size);
}

static void
gtk_viewport_measure (GtkWidget      *widget,
                      GtkOrientation  orientation,
                      int             for_size,
                      int            *minimum,
                      int            *natural,
                      int            *minimum_baseline,
                      int            *natural_baseline)
{
  GtkViewport *viewport = GTK_VIEWPORT (widget);

  if (viewport->child)
    gtk_widget_measure (viewport->child,
                        orientation,
                        for_size,
                        minimum, natural,
                        NULL, NULL);
}

static void
gtk_viewport_compute_expand (GtkWidget *widget,
                             gboolean  *hexpand,
                             gboolean  *vexpand)
{
  GtkViewport *viewport = GTK_VIEWPORT (widget);

  if (viewport->child)
    {
      *hexpand = gtk_widget_compute_expand (viewport->child, GTK_ORIENTATION_HORIZONTAL);
      *vexpand = gtk_widget_compute_expand (viewport->child, GTK_ORIENTATION_VERTICAL);
    }
  else
    {
      *hexpand = FALSE;
      *vexpand = FALSE;
    }
}

static GtkSizeRequestMode
gtk_viewport_get_request_mode (GtkWidget *widget)
{
  GtkViewport *viewport = GTK_VIEWPORT (widget);

  if (viewport->child)
    return gtk_widget_get_request_mode (viewport->child);
  else
    return GTK_SIZE_REQUEST_CONSTANT_SIZE;
}

#define ADJUSTMENT_POINTER(orientation)            \
  (((orientation) == GTK_ORIENTATION_HORIZONTAL) ? \
     &viewport->adjustment[GTK_ORIENTATION_HORIZONTAL] : &viewport->adjustment[GTK_ORIENTATION_VERTICAL])

static void
viewport_disconnect_adjustment (GtkViewport    *viewport,
                                GtkOrientation  orientation)
{
  GtkAdjustment **adjustmentp = ADJUSTMENT_POINTER (orientation);

  if (*adjustmentp)
    {
      g_signal_handlers_disconnect_by_func (*adjustmentp,
                                            gtk_viewport_adjustment_value_changed,
                                            viewport);
      g_object_unref (*adjustmentp);
      *adjustmentp = NULL;
    }
}

static void
gtk_viewport_dispose (GObject *object)
{
  GtkViewport *viewport = GTK_VIEWPORT (object);

  viewport_disconnect_adjustment (viewport, GTK_ORIENTATION_HORIZONTAL);
  viewport_disconnect_adjustment (viewport, GTK_ORIENTATION_VERTICAL);

  clear_focus_change_handler (viewport);

  g_clear_pointer (&viewport->child, gtk_widget_unparent);

  G_OBJECT_CLASS (gtk_viewport_parent_class)->dispose (object);

}

static void
gtk_viewport_root (GtkWidget *widget)
{
  GtkViewport *viewport = GTK_VIEWPORT (widget);

  GTK_WIDGET_CLASS (gtk_viewport_parent_class)->root (widget);

  if (viewport->scroll_to_focus)
    setup_focus_change_handler (viewport);
}

static void
gtk_viewport_unroot (GtkWidget *widget)
{
  GtkViewport *viewport = GTK_VIEWPORT (widget);

  if (viewport->scroll_to_focus)
    clear_focus_change_handler (viewport);

  GTK_WIDGET_CLASS (gtk_viewport_parent_class)->unroot (widget);
}

static void
gtk_viewport_class_init (GtkViewportClass *class)
{
  GObjectClass   *gobject_class;
  GtkWidgetClass *widget_class;

  gobject_class = G_OBJECT_CLASS (class);
  widget_class = (GtkWidgetClass*) class;

  gobject_class->dispose = gtk_viewport_dispose;
  gobject_class->set_property = gtk_viewport_set_property;
  gobject_class->get_property = gtk_viewport_get_property;

  widget_class->size_allocate = gtk_viewport_size_allocate;
  widget_class->measure = gtk_viewport_measure;
  widget_class->root = gtk_viewport_root;
  widget_class->unroot = gtk_viewport_unroot;
  widget_class->compute_expand = gtk_viewport_compute_expand;
  widget_class->get_request_mode = gtk_viewport_get_request_mode;

  /* GtkScrollable implementation */
  g_object_class_override_property (gobject_class, PROP_HADJUSTMENT,    "hadjustment");
  g_object_class_override_property (gobject_class, PROP_VADJUSTMENT,    "vadjustment");
  g_object_class_override_property (gobject_class, PROP_HSCROLL_POLICY, "hscroll-policy");
  g_object_class_override_property (gobject_class, PROP_VSCROLL_POLICY, "vscroll-policy");

  /**
   * GtkViewport:scroll-to-focus: (attributes org.gtk.Property.get=gtk_viewport_get_scroll_to_focus org.gtk.Property.set=gtk_viewport_set_scroll_to_focus)
   *
   * Whether to scroll when the focus changes.
   *
   * Before 4.6.2, this property was mistakenly defaulting to FALSE, so if your
   * code needs to work with older versions, consider setting it explicitly to
   * TRUE.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_SCROLL_TO_FOCUS,
                                   g_param_spec_boolean ("scroll-to-focus", NULL, NULL,
                                                         TRUE,
                                                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkViewport:child: (attributes org.gtk.Property.get=gtk_viewport_get_child org.gtk.Property.set=gtk_viewport_set_child)
   *
   * The child widget.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_CHILD,
                                   g_param_spec_object ("child", NULL, NULL,
                                                        GTK_TYPE_WIDGET,
                                                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  gtk_widget_class_set_css_name (widget_class, I_("viewport"));
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_GENERIC);
}

static void
gtk_viewport_set_property (GObject         *object,
                           guint            prop_id,
                           const GValue    *value,
                           GParamSpec      *pspec)
{
  GtkViewport *viewport = GTK_VIEWPORT (object);

  switch (prop_id)
    {
    case PROP_HADJUSTMENT:
      viewport_set_adjustment (viewport, GTK_ORIENTATION_HORIZONTAL, g_value_get_object (value));
      break;
    case PROP_VADJUSTMENT:
      viewport_set_adjustment (viewport, GTK_ORIENTATION_VERTICAL, g_value_get_object (value));
      break;
    case PROP_HSCROLL_POLICY:
      if (viewport->scroll_policy[GTK_ORIENTATION_HORIZONTAL] != g_value_get_enum (value))
        {
          viewport->scroll_policy[GTK_ORIENTATION_HORIZONTAL] = g_value_get_enum (value);
          gtk_widget_queue_resize (GTK_WIDGET (viewport));
          g_object_notify_by_pspec (object, pspec);
        }
      break;
    case PROP_VSCROLL_POLICY:
      if (viewport->scroll_policy[GTK_ORIENTATION_VERTICAL] != g_value_get_enum (value))
        {
          viewport->scroll_policy[GTK_ORIENTATION_VERTICAL] = g_value_get_enum (value);
          gtk_widget_queue_resize (GTK_WIDGET (viewport));
          g_object_notify_by_pspec (object, pspec);
        }
      break;
    case PROP_SCROLL_TO_FOCUS:
      gtk_viewport_set_scroll_to_focus (viewport, g_value_get_boolean (value));
      break;
    case PROP_CHILD:
      gtk_viewport_set_child (viewport, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_viewport_get_property (GObject         *object,
                           guint            prop_id,
                           GValue          *value,
                           GParamSpec      *pspec)
{
  GtkViewport *viewport = GTK_VIEWPORT (object);

  switch (prop_id)
    {
    case PROP_HADJUSTMENT:
      g_value_set_object (value, viewport->adjustment[GTK_ORIENTATION_HORIZONTAL]);
      break;
    case PROP_VADJUSTMENT:
      g_value_set_object (value, viewport->adjustment[GTK_ORIENTATION_VERTICAL]);
      break;
    case PROP_HSCROLL_POLICY:
      g_value_set_enum (value, viewport->scroll_policy[GTK_ORIENTATION_HORIZONTAL]);
      break;
    case PROP_VSCROLL_POLICY:
      g_value_set_enum (value, viewport->scroll_policy[GTK_ORIENTATION_VERTICAL]);
      break;
    case PROP_SCROLL_TO_FOCUS:
      g_value_set_boolean (value, viewport->scroll_to_focus);
      break;
    case PROP_CHILD:
      g_value_set_object (value, gtk_viewport_get_child (viewport));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_viewport_init (GtkViewport *viewport)
{
  GtkWidget *widget;

  widget = GTK_WIDGET (viewport);

  gtk_widget_set_overflow (widget, GTK_OVERFLOW_HIDDEN);

  viewport->adjustment[GTK_ORIENTATION_HORIZONTAL] = NULL;
  viewport->adjustment[GTK_ORIENTATION_VERTICAL] = NULL;

  viewport_set_adjustment (viewport, GTK_ORIENTATION_HORIZONTAL, NULL);
  viewport_set_adjustment (viewport, GTK_ORIENTATION_VERTICAL, NULL);

  viewport->scroll_to_focus = TRUE;
}

/**
 * gtk_viewport_new:
 * @hadjustment: (nullable): horizontal adjustment
 * @vadjustment: (nullable): vertical adjustment
 *
 * Creates a new `GtkViewport`.
 *
 * The new viewport uses the given adjustments, or default
 * adjustments if none are given.
 *
 * Returns: a new `GtkViewport`
 */
GtkWidget*
gtk_viewport_new (GtkAdjustment *hadjustment,
                  GtkAdjustment *vadjustment)
{
  GtkWidget *viewport;

  viewport = g_object_new (GTK_TYPE_VIEWPORT,
                           "hadjustment", hadjustment,
                           "vadjustment", vadjustment,
                           NULL);

  return viewport;
}

static void
viewport_set_adjustment (GtkViewport    *viewport,
                         GtkOrientation  orientation,
                         GtkAdjustment  *adjustment)
{
  GtkAdjustment **adjustmentp = ADJUSTMENT_POINTER (orientation);

  if (adjustment && adjustment == *adjustmentp)
    return;

  if (!adjustment)
    adjustment = gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
  viewport_disconnect_adjustment (viewport, orientation);
  *adjustmentp = adjustment;
  g_object_ref_sink (adjustment);

  g_signal_connect (adjustment, "value-changed",
                    G_CALLBACK (gtk_viewport_adjustment_value_changed),
                    viewport);

  gtk_viewport_adjustment_value_changed (adjustment, viewport);
}

static void
gtk_viewport_size_allocate (GtkWidget *widget,
                            int        width,
                            int        height,
                            int        baseline)
{
  GtkViewport *viewport = GTK_VIEWPORT (widget);
  int child_size[2];

  g_object_freeze_notify (G_OBJECT (viewport->adjustment[GTK_ORIENTATION_HORIZONTAL]));
  g_object_freeze_notify (G_OBJECT (viewport->adjustment[GTK_ORIENTATION_VERTICAL]));

  child_size[GTK_ORIENTATION_HORIZONTAL] = width;
  child_size[GTK_ORIENTATION_VERTICAL] = height;

  if (viewport->child && gtk_widget_get_visible (viewport->child))
    {
      GtkOrientation orientation, opposite;
      int min, nat;

      if (gtk_widget_get_request_mode (viewport->child) == GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT)
        orientation = GTK_ORIENTATION_VERTICAL;
      else
        orientation = GTK_ORIENTATION_HORIZONTAL;
      opposite = OPPOSITE_ORIENTATION (orientation);

      gtk_widget_measure (viewport->child,
                          orientation, -1,
                          &min, &nat,
                          NULL, NULL);
      if (viewport->scroll_policy[orientation] == GTK_SCROLL_MINIMUM)
        child_size[orientation] = MAX (child_size[orientation], min);
      else
        child_size[orientation] = MAX (child_size[orientation], nat);

      gtk_widget_measure (viewport->child,
                          opposite, child_size[orientation],
                          &min, &nat,
                          NULL, NULL);
      if (viewport->scroll_policy[opposite] == GTK_SCROLL_MINIMUM)
        child_size[opposite] = MAX (child_size[opposite], min);
      else
        child_size[opposite] = MAX (child_size[opposite], nat);
    }

  viewport_set_adjustment_values (viewport, GTK_ORIENTATION_HORIZONTAL, width, child_size[GTK_ORIENTATION_HORIZONTAL]);
  viewport_set_adjustment_values (viewport, GTK_ORIENTATION_VERTICAL, height, child_size[GTK_ORIENTATION_VERTICAL]);

  if (viewport->child && gtk_widget_get_visible (viewport->child))
    {
      GtkAllocation child_allocation;

      child_allocation.width = child_size[GTK_ORIENTATION_HORIZONTAL];
      child_allocation.height = child_size[GTK_ORIENTATION_VERTICAL];
      child_allocation.x = - gtk_adjustment_get_value (viewport->adjustment[GTK_ORIENTATION_HORIZONTAL]);
      child_allocation.y = - gtk_adjustment_get_value (viewport->adjustment[GTK_ORIENTATION_VERTICAL]);

      gtk_widget_size_allocate (viewport->child, &child_allocation, -1);
    }

  g_object_thaw_notify (G_OBJECT (viewport->adjustment[GTK_ORIENTATION_HORIZONTAL]));
  g_object_thaw_notify (G_OBJECT (viewport->adjustment[GTK_ORIENTATION_VERTICAL]));
}

static void
gtk_viewport_adjustment_value_changed (GtkAdjustment *adjustment,
                                       gpointer       data)
{
  gtk_widget_queue_allocate (GTK_WIDGET (data));
}

/**
 * gtk_viewport_get_scroll_to_focus: (attributes org.gtk.Method.get_property=scroll-to-focus)
 * @viewport: a `GtkViewport`
 *
 * Gets whether the viewport is scrolling to keep the focused
 * child in view.
 *
 * Returns: %TRUE if the viewport keeps the focus child scrolled to view
 */
gboolean
gtk_viewport_get_scroll_to_focus (GtkViewport *viewport)
{
  g_return_val_if_fail (GTK_IS_VIEWPORT (viewport), FALSE);

  return viewport->scroll_to_focus;
}

/**
 * gtk_viewport_set_scroll_to_focus: (attributes org.gtk.Method.set_property=scroll-to-focus)
 * @viewport: a `GtkViewport`
 * @scroll_to_focus: whether to keep the focus widget scrolled to view
 *
 * Sets whether the viewport should automatically scroll
 * to keep the focused child in view.
 */
void
gtk_viewport_set_scroll_to_focus (GtkViewport *viewport,
                                  gboolean     scroll_to_focus)
{
  g_return_if_fail (GTK_IS_VIEWPORT (viewport));

  if (viewport->scroll_to_focus == scroll_to_focus)
    return;

  viewport->scroll_to_focus = scroll_to_focus;

  if (gtk_widget_get_root (GTK_WIDGET (viewport)))
    {
      if (scroll_to_focus)
        setup_focus_change_handler (viewport);
      else
        clear_focus_change_handler (viewport);
    }

  g_object_notify (G_OBJECT (viewport), "scroll-to-focus");
}

static void
focus_change_handler (GtkWidget *widget)
{
  GtkViewport *viewport = GTK_VIEWPORT (widget);
  GtkRoot *root;
  GtkWidget *focus_widget;

  if ((gtk_widget_get_state_flags (widget) & GTK_STATE_FLAG_FOCUS_WITHIN) == 0)
    return;

  root = gtk_widget_get_root (widget);
  focus_widget = gtk_root_get_focus (root);

  if (!focus_widget)
    return;

  if (GTK_IS_TEXT (focus_widget))
    focus_widget = gtk_widget_get_parent (focus_widget);

  gtk_viewport_scroll_to (viewport, focus_widget, NULL);
}

static void
setup_focus_change_handler (GtkViewport *viewport)
{
  GtkRoot *root;

  root = gtk_widget_get_root (GTK_WIDGET (viewport));

  viewport->focus_handler = g_signal_connect_swapped (root, "notify::focus-widget",
                                                      G_CALLBACK (focus_change_handler), viewport);
}

static void
clear_focus_change_handler (GtkViewport *viewport)
{
  GtkRoot *root;

  root = gtk_widget_get_root (GTK_WIDGET (viewport));

  if (viewport->focus_handler)
    {
      g_signal_handler_disconnect (root, viewport->focus_handler);
      viewport->focus_handler = 0;
    }
}

/**
 * gtk_viewport_set_child: (attributes org.gtk.Method.set_property=child)
 * @viewport: a `GtkViewport`
 * @child: (nullable): the child widget
 *
 * Sets the child widget of @viewport.
 */
void
gtk_viewport_set_child (GtkViewport *viewport,
                        GtkWidget   *child)
{
  g_return_if_fail (GTK_IS_VIEWPORT (viewport));
  g_return_if_fail (child == NULL || viewport->child == child || gtk_widget_get_parent (child) == NULL);

  if (viewport->child == child)
    return;

  g_clear_pointer (&viewport->child, gtk_widget_unparent);

  if (child)
    {
      viewport->child = child;
      gtk_widget_set_parent (child, GTK_WIDGET (viewport));
    }

  g_object_notify (G_OBJECT (viewport), "child");
}

/**
 * gtk_viewport_get_child: (attributes org.gtk.Method.get_property=child)
 * @viewport: a `GtkViewport`
 *
 * Gets the child widget of @viewport.
 *
 * Returns: (nullable) (transfer none): the child widget of @viewport
 */
GtkWidget *
gtk_viewport_get_child (GtkViewport *viewport)
{
  g_return_val_if_fail (GTK_IS_VIEWPORT (viewport), NULL);

  return viewport->child;
}

/**
 * gtk_viewport_scroll_to:
 * @viewport: a `GtkViewport`
 * @descendant: a descendant widget of the viewport
 * @scroll: (nullable) (transfer full): details of how to perform
 *   the scroll operation or NULL to scroll into view
 *
 * Scrolls a descendant of the viewport into view.
 *
 * The viewport and the descendant must be visible and mapped for
 * this function to work, otherwise no scrolling will be performed.
 *
 * Since: 4.12
 **/
void
gtk_viewport_scroll_to (GtkViewport   *viewport,
                        GtkWidget     *descendant,
                        GtkScrollInfo *scroll)
{
  graphene_rect_t bounds;
  int x, y;
  double adj_x, adj_y;

  g_return_if_fail (GTK_IS_VIEWPORT (viewport));
  g_return_if_fail (GTK_IS_WIDGET (descendant));

  if (!gtk_widget_compute_bounds (descendant, GTK_WIDGET (viewport), &bounds))
    return;

  adj_x = gtk_adjustment_get_value (viewport->adjustment[GTK_ORIENTATION_HORIZONTAL]);
  adj_y = gtk_adjustment_get_value (viewport->adjustment[GTK_ORIENTATION_VERTICAL]);

  gtk_scroll_info_compute_scroll (scroll,
                                  &(GdkRectangle) {
                                    floor (bounds.origin.x + adj_x),
                                    floor (bounds.origin.y + adj_y),
                                    ceil (bounds.origin.x + bounds.size.width) - floor (bounds.origin.x),
                                    ceil (bounds.origin.y + bounds.size.height) - floor (bounds.origin.y)
                                  },
                                  &(GdkRectangle) {
                                    adj_x,
                                    adj_y,
                                    gtk_widget_get_width (GTK_WIDGET (viewport)),
                                    gtk_widget_get_height (GTK_WIDGET (viewport))
                                  },
                                  &x, &y);

  gtk_adjustment_animate_to_value (viewport->adjustment[GTK_ORIENTATION_HORIZONTAL], x);
  gtk_adjustment_animate_to_value (viewport->adjustment[GTK_ORIENTATION_VERTICAL], y);

  g_clear_pointer (&scroll, gtk_scroll_info_unref);
}

