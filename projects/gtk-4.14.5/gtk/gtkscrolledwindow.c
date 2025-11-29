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

#include "gtkscrolledwindow.h"

#include "gtkadjustment.h"
#include "gtkadjustmentprivate.h"
#include "gtkbuildable.h"
#include "gtkdragsourceprivate.h"
#include "gtkeventcontrollermotion.h"
#include "gtkeventcontrollerscroll.h"
#include "gtkeventcontrollerprivate.h"
#include "gtkgesturedrag.h"
#include "gtkgesturelongpress.h"
#include "gtkgesturepan.h"
#include "gtkgesturesingle.h"
#include "gtkgestureswipe.h"
#include "gtkgestureprivate.h"
#include "gtkkineticscrollingprivate.h"
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtkprivate.h"
#include "gtkprogresstrackerprivate.h"
#include "gtkscrollable.h"
#include "gtkscrollbar.h"
#include "gtksettingsprivate.h"
#include "gtksnapshot.h"
#include "gtkrenderbackgroundprivate.h"
#include "gtkrenderborderprivate.h"
#include "gtktypebuiltins.h"
#include "gtkviewport.h"
#include "gtkwidgetprivate.h"

#include <math.h>

/**
 * GtkScrolledWindow:
 *
 * `GtkScrolledWindow` is a container that makes its child scrollable.
 *
 * It does so using either internally added scrollbars or externally
 * associated adjustments, and optionally draws a frame around the child.
 *
 * Widgets with native scrolling support, i.e. those whose classes implement
 * the [iface@Gtk.Scrollable] interface, are added directly. For other types
 * of widget, the class [class@Gtk.Viewport] acts as an adaptor, giving
 * scrollability to other widgets. [method@Gtk.ScrolledWindow.set_child]
 * intelligently accounts for whether or not the added child is a `GtkScrollable`.
 * If it isn’t, then it wraps the child in a `GtkViewport`. Therefore, you can
 * just add any child widget and not worry about the details.
 *
 * If [method@Gtk.ScrolledWindow.set_child] has added a `GtkViewport` for you,
 * it will be automatically removed when you unset the child.
 * Unless [property@Gtk.ScrolledWindow:hscrollbar-policy] and
 * [property@Gtk.ScrolledWindow:vscrollbar-policy] are %GTK_POLICY_NEVER or
 * %GTK_POLICY_EXTERNAL, `GtkScrolledWindow` adds internal `GtkScrollbar` widgets
 * around its child. The scroll position of the child, and if applicable the
 * scrollbars, is controlled by the [property@Gtk.ScrolledWindow:hadjustment]
 * and [property@Gtk.ScrolledWindow:vadjustment] that are associated with the
 * `GtkScrolledWindow`. See the docs on [class@Gtk.Scrollbar] for the details,
 * but note that the “step_increment” and “page_increment” fields are only
 * effective if the policy causes scrollbars to be present.
 *
 * If a `GtkScrolledWindow` doesn’t behave quite as you would like, or
 * doesn’t have exactly the right layout, it’s very possible to set up
 * your own scrolling with `GtkScrollbar` and for example a `GtkGrid`.
 *
 * # Touch support
 *
 * `GtkScrolledWindow` has built-in support for touch devices. When a
 * touchscreen is used, swiping will move the scrolled window, and will
 * expose 'kinetic' behavior. This can be turned off with the
 * [property@Gtk.ScrolledWindow:kinetic-scrolling] property if it is undesired.
 *
 * `GtkScrolledWindow` also displays visual 'overshoot' indication when
 * the content is pulled beyond the end, and this situation can be
 * captured with the [signal@Gtk.ScrolledWindow::edge-overshot] signal.
 *
 * If no mouse device is present, the scrollbars will overlaid as
 * narrow, auto-hiding indicators over the content. If traditional
 * scrollbars are desired although no mouse is present, this behaviour
 * can be turned off with the [property@Gtk.ScrolledWindow:overlay-scrolling]
 * property.
 *
 * # CSS nodes
 *
 * `GtkScrolledWindow` has a main CSS node with name scrolledwindow.
 * It gets a .frame style class added when [property@Gtk.ScrolledWindow:has-frame]
 * is %TRUE.
 *
 * It uses subnodes with names overshoot and undershoot to draw the overflow
 * and underflow indications. These nodes get the .left, .right, .top or .bottom
 * style class added depending on where the indication is drawn.
 *
 * `GtkScrolledWindow` also sets the positional style classes (.left, .right,
 * .top, .bottom) and style classes related to overlay scrolling
 * (.overlay-indicator, .dragging, .hovering) on its scrollbars.
 *
 * If both scrollbars are visible, the area where they meet is drawn
 * with a subnode named junction.
 *
 * # Accessibility
 *
 * Until GTK 4.10, `GtkScrolledWindow` used the `GTK_ACCESSIBLE_ROLE_GROUP` role.
 *
 * Starting from GTK 4.12, `GtkScrolledWindow` uses the `GTK_ACCESSIBLE_ROLE_GENERIC` role.
 */

/* scrolled window policy and size requisition handling:
 *
 * gtk size requisition works as follows:
 *   a widget upon size-request reports the width and height that it finds
 *   to be best suited to display its contents, including children.
 *   the width and/or height reported from a widget upon size requisition
 *   may be overridden by the user by specifying a width and/or height
 *   other than 0 through gtk_widget_set_size_request().
 *
 * a scrolled window needs (for implementing all three policy types) to
 * request its width and height based on two different rationales.
 * 1)   the user wants the scrolled window to just fit into the space
 *      that it gets allocated for a specific dimension.
 * 1.1) this does not apply if the user specified a concrete value
 *      value for that specific dimension by either specifying usize for the
 *      scrolled window or for its child.
 * 2)   the user wants the scrolled window to take as much space up as
 *      is desired by the child for a specific dimension (i.e. POLICY_NEVER).
 *
 * also, kinda obvious:
 * 3)   a user would certainly not have chosen a scrolled window as a container
 *      for the child, if the resulting allocation takes up more space than the
 *      child would have allocated without the scrolled window.
 *
 * conclusions:
 * A) from 1) follows: the scrolled window shouldn’t request more space for a
 *    specific dimension than is required at minimum.
 * B) from 1.1) follows: the requisition may be overridden by usize of the scrolled
 *    window (done automatically) or by usize of the child (needs to be checked).
 * C) from 2) follows: for POLICY_NEVER, the scrolled window simply reports the
 *    child’s dimension.
 * D) from 3) follows: the scrolled window child’s minimum width and minimum height
 *    under A) at least correspond to the space taken up by its scrollbars.
 */

/* Kinetic scrolling */
#define MAX_OVERSHOOT_DISTANCE 100
#define DECELERATION_FRICTION 4
#define OVERSHOOT_FRICTION 20
#define VELOCITY_ACCUMULATION_FLOOR 0.33
#define VELOCITY_ACCUMULATION_CEIL 1.0
#define VELOCITY_ACCUMULATION_MAX 6.0

/* Animated scrolling */
#define ANIMATION_DURATION 200

/* Overlay scrollbars */
#define INDICATOR_FADE_OUT_DELAY 2000
#define INDICATOR_FADE_OUT_DURATION 1000
#define INDICATOR_FADE_OUT_TIME 500
#define INDICATOR_CLOSE_DISTANCE 5
#define INDICATOR_FAR_DISTANCE 10

/* Scrolled off indication */
#define UNDERSHOOT_SIZE 40

#define MAGIC_SCROLL_FACTOR 2.5

typedef struct _GtkScrolledWindowClass         GtkScrolledWindowClass;

struct _GtkScrolledWindow
{
  GtkWidget parent_instance;
};

struct _GtkScrolledWindowClass
{
  GtkWidgetClass parent_class;

  /* Unfortunately, GtkScrollType is deficient in that there is
   * no horizontal/vertical variants for GTK_SCROLL_START/END,
   * so we have to add an additional boolean flag.
   */
  gboolean (*scroll_child) (GtkScrolledWindow *scrolled_window,
                            GtkScrollType      scroll,
                            gboolean           horizontal);

  void (* move_focus_out) (GtkScrolledWindow *scrolled_window,
                           GtkDirectionType   direction);
};

typedef struct
{
  GtkWidget *scrollbar;
  gboolean   over; /* either mouse over, or while dragging */
  gint64     last_scroll_time;
  guint      conceil_timer;

  double     current_pos;
  double     source_pos;
  double     target_pos;
  GtkProgressTracker tracker;
  guint      tick_id;
  guint      over_timeout_id;
} Indicator;

typedef struct
{
  GtkWidget *child;

  GtkWidget     *hscrollbar;
  GtkWidget     *vscrollbar;

  GtkCssNode    *overshoot_node[4];
  GtkCssNode    *undershoot_node[4];
  GtkCssNode    *junction_node;

  Indicator hindicator;
  Indicator vindicator;

  GtkCornerType  window_placement;
  guint    has_frame                : 1;
  guint    hscrollbar_policy        : 2;
  guint    vscrollbar_policy        : 2;
  guint    hscrollbar_visible       : 1;
  guint    vscrollbar_visible       : 1;
  guint    focus_out                : 1; /* used by ::move-focus-out implementation */
  guint    overlay_scrolling        : 1;
  guint    use_indicators           : 1;
  guint    auto_added_viewport      : 1;
  guint    propagate_natural_width  : 1;
  guint    propagate_natural_height : 1;
  guint    smooth_scroll            : 1;

  int      min_content_width;
  int      min_content_height;
  int      max_content_width;
  int      max_content_height;

  guint scroll_events_overshoot_id;

  /* Kinetic scrolling */
  GtkGesture *long_press_gesture;
  GtkGesture *swipe_gesture;
  GtkKineticScrolling *hscrolling;
  GtkKineticScrolling *vscrolling;
  gint64 last_deceleration_time;

  /* These two gestures are mutually exclusive */
  GtkGesture *drag_gesture;
  GtkGesture *pan_gesture;

  double drag_start_x;
  double drag_start_y;

  guint                  kinetic_scrolling         : 1;

  guint                  deceleration_id;

  double                 x_velocity;
  double                 y_velocity;

  double                 unclamped_hadj_value;
  double                 unclamped_vadj_value;
} GtkScrolledWindowPrivate;

enum {
  PROP_0,
  PROP_HADJUSTMENT,
  PROP_VADJUSTMENT,
  PROP_HSCROLLBAR_POLICY,
  PROP_VSCROLLBAR_POLICY,
  PROP_WINDOW_PLACEMENT,
  PROP_HAS_FRAME,
  PROP_MIN_CONTENT_WIDTH,
  PROP_MIN_CONTENT_HEIGHT,
  PROP_KINETIC_SCROLLING,
  PROP_OVERLAY_SCROLLING,
  PROP_MAX_CONTENT_WIDTH,
  PROP_MAX_CONTENT_HEIGHT,
  PROP_PROPAGATE_NATURAL_WIDTH,
  PROP_PROPAGATE_NATURAL_HEIGHT,
  PROP_CHILD,
  NUM_PROPERTIES
};

/* Signals */
enum
{
  SCROLL_CHILD,
  MOVE_FOCUS_OUT,
  EDGE_OVERSHOT,
  EDGE_REACHED,
  LAST_SIGNAL
};

static void     gtk_scrolled_window_set_property       (GObject           *object,
                                                        guint              prop_id,
                                                        const GValue      *value,
                                                        GParamSpec        *pspec);
static void     gtk_scrolled_window_get_property       (GObject           *object,
                                                        guint              prop_id,
                                                        GValue            *value,
                                                        GParamSpec        *pspec);
static void     gtk_scrolled_window_dispose            (GObject           *object);

static void     gtk_scrolled_window_snapshot           (GtkWidget         *widget,
                                                        GtkSnapshot       *snapshot);
static void     gtk_scrolled_window_size_allocate      (GtkWidget         *widget,
                                                        int                width,
                                                        int                height,
                                                        int                baseline);
static gboolean gtk_scrolled_window_focus              (GtkWidget         *widget,
                                                        GtkDirectionType   direction);
static gboolean gtk_scrolled_window_scroll_child       (GtkScrolledWindow *scrolled_window,
                                                        GtkScrollType      scroll,
                                                        gboolean           horizontal);
static void     gtk_scrolled_window_move_focus_out     (GtkScrolledWindow *scrolled_window,
                                                        GtkDirectionType   direction_type);

static void     gtk_scrolled_window_relative_allocation(GtkScrolledWindow *scrolled_window,
                                                        GtkAllocation     *allocation);
static void     gtk_scrolled_window_inner_allocation   (GtkScrolledWindow *scrolled_window,
                                                        GtkAllocation     *rect);
static void     gtk_scrolled_window_allocate_scrollbar (GtkScrolledWindow *scrolled_window,
                                                        GtkWidget         *scrollbar,
                                                        GtkAllocation     *allocation);
static void     gtk_scrolled_window_allocate_child     (GtkScrolledWindow   *swindow,
                                                        int                  width,
                                                        int                  height);
static void     gtk_scrolled_window_adjustment_changed (GtkAdjustment     *adjustment,
                                                        gpointer           data);
static void     gtk_scrolled_window_adjustment_value_changed (GtkAdjustment     *adjustment,
                                                              gpointer           data);
static gboolean gtk_widget_should_animate              (GtkWidget           *widget);
static void     gtk_scrolled_window_measure (GtkWidget      *widget,
                                             GtkOrientation  orientation,
                                             int             for_size,
                                             int            *minimum_size,
                                             int            *natural_size,
                                             int            *minimum_baseline,
                                             int            *natural_baseline);
static void  gtk_scrolled_window_map                   (GtkWidget           *widget);
static void  gtk_scrolled_window_unmap                 (GtkWidget           *widget);
static void  gtk_scrolled_window_realize               (GtkWidget           *widget);
static void  gtk_scrolled_window_unrealize             (GtkWidget           *widget);
static void _gtk_scrolled_window_set_adjustment_value  (GtkScrolledWindow *scrolled_window,
                                                        GtkAdjustment     *adjustment,
                                                        double             value);

static void gtk_scrolled_window_cancel_deceleration (GtkScrolledWindow *scrolled_window);

static gboolean _gtk_scrolled_window_get_overshoot (GtkScrolledWindow *scrolled_window,
                                                    int               *overshoot_x,
                                                    int               *overshoot_y);

static void     gtk_scrolled_window_start_deceleration (GtkScrolledWindow *scrolled_window);

static void     gtk_scrolled_window_update_use_indicators (GtkScrolledWindow *scrolled_window);
static void     remove_indicator     (GtkScrolledWindow *sw,
                                      Indicator         *indicator);
static gboolean maybe_hide_indicator (gpointer data);

static void     indicator_start_fade (Indicator *indicator,
                                      double     pos);
static void     indicator_set_over   (Indicator *indicator,
                                      gboolean   over);

static void scrolled_window_scroll (GtkScrolledWindow        *scrolled_window,
                                    double                    delta_x,
                                    double                    delta_y,
                                    GtkEventControllerScroll *scroll);

static guint signals[LAST_SIGNAL] = {0};
static GParamSpec *properties[NUM_PROPERTIES];

static void gtk_scrolled_window_buildable_init (GtkBuildableIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkScrolledWindow, gtk_scrolled_window, GTK_TYPE_WIDGET,
                         G_ADD_PRIVATE (GtkScrolledWindow)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                gtk_scrolled_window_buildable_init))

static GtkBuildableIface *parent_buildable_iface;

static void
gtk_scrolled_window_buildable_add_child (GtkBuildable *buildable,
                                         GtkBuilder   *builder,
                                         GObject      *child,
                                         const char   *type)
{
  if (GTK_IS_WIDGET (child))
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW(buildable), GTK_WIDGET (child));
  else
    parent_buildable_iface->add_child (buildable, builder, child, type);
}

static void
gtk_scrolled_window_buildable_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);

  iface->add_child = gtk_scrolled_window_buildable_add_child;
}

static void
add_scroll_binding (GtkWidgetClass *widget_class,
                    guint           keyval,
                    GdkModifierType mask,
                    GtkScrollType   scroll,
                    gboolean        horizontal)
{
  guint keypad_keyval = keyval - GDK_KEY_Left + GDK_KEY_KP_Left;

  gtk_widget_class_add_binding_signal (widget_class,
                                       keyval, mask,
                                       "scroll-child",
                                       "(ib)", scroll, horizontal);
  gtk_widget_class_add_binding_signal (widget_class,
                                       keypad_keyval, mask,
                                       "scroll-child",
                                       "(ib)", scroll, horizontal);
}

static void
add_tab_bindings (GtkWidgetClass   *widget_class,
                  GdkModifierType   modifiers,
                  GtkDirectionType  direction)
{
  gtk_widget_class_add_binding_signal (widget_class,
                                       GDK_KEY_Tab, modifiers,
                                       "move-focus-out",
                                       "(i)", direction);
  gtk_widget_class_add_binding_signal (widget_class,
                                       GDK_KEY_KP_Tab, modifiers,
                                       "move-focus-out",
                                       "(i)", direction);
}

static void
motion_controller_leave (GtkEventController   *controller,
                         GtkScrolledWindow    *scrolled_window)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  if (priv->use_indicators)
    {
      indicator_set_over (&priv->hindicator, FALSE);
      indicator_set_over (&priv->vindicator, FALSE);
    }
}

static void
update_scrollbar_positions (GtkScrolledWindow *scrolled_window)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);
  gboolean is_rtl;

  if (priv->hscrollbar != NULL)
    {
      if (priv->window_placement == GTK_CORNER_TOP_LEFT ||
          priv->window_placement == GTK_CORNER_TOP_RIGHT)
        {
          gtk_widget_add_css_class (priv->hscrollbar, "bottom");
          gtk_widget_remove_css_class (priv->hscrollbar, "top");
        }
      else
        {
          gtk_widget_add_css_class (priv->hscrollbar, "top");
          gtk_widget_remove_css_class (priv->hscrollbar, "bottom");
        }
    }

  if (priv->vscrollbar != NULL)
    {
      is_rtl = _gtk_widget_get_direction (GTK_WIDGET (scrolled_window)) == GTK_TEXT_DIR_RTL;
      if ((is_rtl &&
          (priv->window_placement == GTK_CORNER_TOP_RIGHT ||
           priv->window_placement == GTK_CORNER_BOTTOM_RIGHT)) ||
         (!is_rtl &&
          (priv->window_placement == GTK_CORNER_TOP_LEFT ||
           priv->window_placement == GTK_CORNER_BOTTOM_LEFT)))
        {
          gtk_widget_add_css_class (priv->vscrollbar, "right");
          gtk_widget_remove_css_class (priv->vscrollbar, "left");
        }
      else
        {
          gtk_widget_add_css_class (priv->vscrollbar, "left");
          gtk_widget_remove_css_class (priv->vscrollbar, "right");
        }
    }
}

static void
gtk_scrolled_window_direction_changed (GtkWidget        *widget,
                                       GtkTextDirection  previous_dir)
{
  GtkScrolledWindow *scrolled_window = GTK_SCROLLED_WINDOW (widget);

  update_scrollbar_positions (scrolled_window);

  GTK_WIDGET_CLASS (gtk_scrolled_window_parent_class)->direction_changed (widget, previous_dir);
}

static void
gtk_scrolled_window_compute_expand (GtkWidget *widget,
                                    gboolean  *hexpand,
                                    gboolean  *vexpand)
{
  GtkScrolledWindow *scrolled_window = GTK_SCROLLED_WINDOW (widget);
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  if (priv->child)
    {
      *hexpand = gtk_widget_compute_expand (priv->child, GTK_ORIENTATION_HORIZONTAL);
      *vexpand = gtk_widget_compute_expand (priv->child, GTK_ORIENTATION_VERTICAL);
    }
  else
    {
      *hexpand = FALSE;
      *vexpand = FALSE;
    }
}

static GtkSizeRequestMode
gtk_scrolled_window_get_request_mode (GtkWidget *widget)
{
  GtkScrolledWindow *scrolled_window = GTK_SCROLLED_WINDOW (widget);
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  if (priv->child)
    return gtk_widget_get_request_mode (priv->child);
  else
    return GTK_SIZE_REQUEST_CONSTANT_SIZE;
}

static void
gtk_scrolled_window_class_init (GtkScrolledWindowClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  gobject_class->set_property = gtk_scrolled_window_set_property;
  gobject_class->get_property = gtk_scrolled_window_get_property;
  gobject_class->dispose = gtk_scrolled_window_dispose;

  widget_class->snapshot = gtk_scrolled_window_snapshot;
  widget_class->size_allocate = gtk_scrolled_window_size_allocate;
  widget_class->measure = gtk_scrolled_window_measure;
  widget_class->focus = gtk_scrolled_window_focus;
  widget_class->map = gtk_scrolled_window_map;
  widget_class->unmap = gtk_scrolled_window_unmap;
  widget_class->realize = gtk_scrolled_window_realize;
  widget_class->unrealize = gtk_scrolled_window_unrealize;
  widget_class->direction_changed = gtk_scrolled_window_direction_changed;
  widget_class->compute_expand = gtk_scrolled_window_compute_expand;
  widget_class->get_request_mode = gtk_scrolled_window_get_request_mode;

  class->scroll_child = gtk_scrolled_window_scroll_child;
  class->move_focus_out = gtk_scrolled_window_move_focus_out;

  /**
   * GtkScrolleWindow:hadjustment: (attributes org.gtk.Property.get=gtk_scrolled_window_get_hadjustment org.gtk.Property.set=gtk_scrolled_window_set_hadjustment)
   *
   * The `GtkAdjustment` for the horizontal position.
   */
  properties[PROP_HADJUSTMENT] =
      g_param_spec_object ("hadjustment", NULL, NULL,
                           GTK_TYPE_ADJUSTMENT,
                           GTK_PARAM_READWRITE|G_PARAM_CONSTRUCT|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkScrolleWindow:vadjustment: (attributes org.gtk.Property.get=gtk_scrolled_window_get_vadjustment org.gtk.Property.set=gtk_scrolled_window_set_vadjustment)
   *
   * The `GtkAdjustment` for the vertical position.
   */
  properties[PROP_VADJUSTMENT] =
      g_param_spec_object ("vadjustment", NULL, NULL,
                           GTK_TYPE_ADJUSTMENT,
                           GTK_PARAM_READWRITE|G_PARAM_CONSTRUCT|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkScrolledWindow:hscrollbar-policy:
   *
   * When the horizontal scrollbar is displayed.
   *
   * Use [method@Gtk.ScrolledWindow.set_policy] to set
   * this property.
   */
  properties[PROP_HSCROLLBAR_POLICY] =
      g_param_spec_enum ("hscrollbar-policy", NULL, NULL,
                         GTK_TYPE_POLICY_TYPE,
                         GTK_POLICY_AUTOMATIC,
                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkScrolledWindow:vscrollbar-policy:
   *
   * When the vertical scrollbar is displayed.
   *
   * Use [method@Gtk.ScrolledWindow.set_policy] to set
   * this property.
   */
  properties[PROP_VSCROLLBAR_POLICY] =
      g_param_spec_enum ("vscrollbar-policy", NULL, NULL,
                        GTK_TYPE_POLICY_TYPE,
                        GTK_POLICY_AUTOMATIC,
                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkScrolledWindow:window-placement: (attributes org.gtk.Property.get=gtk_scrolled_window_get_placement org.gtk.Property.set=gtk_scrolled_window_set_placement)
   *
   * Where the contents are located with respect to the scrollbars.
   */
  properties[PROP_WINDOW_PLACEMENT] =
      g_param_spec_enum ("window-placement", NULL, NULL,
                        GTK_TYPE_CORNER_TYPE,
                        GTK_CORNER_TOP_LEFT,
                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkScrolledWindow:has-frame: (attributes org.gtk.Property.get=gtk_scrolled_window_get_has_frame org.gtk.Property.set=gtk_scrolled_window_set_has_frame)
   *
   * Whether to draw a frame around the contents.
   */
  properties[PROP_HAS_FRAME] =
      g_param_spec_boolean ("has-frame", NULL, NULL,
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkScrolledWindow:min-content-width: (attributes org.gtk.Property.get=gtk_scrolled_window_get_min_content_width org.gtk.Property.set=gtk_scrolled_window_set_min_content_width)
   *
   * The minimum content width of @scrolled_window.
   */
  properties[PROP_MIN_CONTENT_WIDTH] =
      g_param_spec_int ("min-content-width", NULL, NULL,
                        -1, G_MAXINT, -1,
                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkScrolledWindow:min-content-height: (attributes org.gtk.Property.get=gtk_scrolled_window_get_min_content_height org.gtk.Property.set=gtk_scrolled_window_set_min_content_height)
   *
   * The minimum content height of @scrolled_window.
   */
  properties[PROP_MIN_CONTENT_HEIGHT] =
      g_param_spec_int ("min-content-height", NULL, NULL,
                        -1, G_MAXINT, -1,
                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkScrolledWindow:kinetic-scrolling: (attributes org.gtk.Property.get=gtk_scrolled_window_get_kinetic_scrolling org.gtk.Property.set=gtk_scrolled_window_set_kinetic_scrolling)
   *
   * Whether kinetic scrolling is enabled or not.
   *
   * Kinetic scrolling only applies to devices with source %GDK_SOURCE_TOUCHSCREEN.
   */
  properties[PROP_KINETIC_SCROLLING] =
      g_param_spec_boolean ("kinetic-scrolling", NULL, NULL,
                            TRUE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkScrolledWindow:overlay-scrolling: (attributes org.gtk.Property.get=gtk_scrolled_window_get_overlay_scrolling org.gtk.Property.set=gtk_scrolled_window_set_overlay_scrolling)
   *
   * Whether overlay scrolling is enabled or not.
   *
   * If it is, the scrollbars are only added as traditional widgets
   * when a mouse is present. Otherwise, they are overlaid on top of
   * the content, as narrow indicators.
   *
   * Note that overlay scrolling can also be globally disabled, with
   * the [property@Gtk.Settings:gtk-overlay-scrolling] setting.
   */
  properties[PROP_OVERLAY_SCROLLING] =
      g_param_spec_boolean ("overlay-scrolling", NULL, NULL,
                            TRUE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkScrolledWindow:max-content-width: (attributes org.gtk.Property.get=gtk_scrolled_window_get_max_content_width org.gtk.Property.set=gtk_scrolled_window_set_max_content_width)
   *
   * The maximum content width of @scrolled_window.
   */
  properties[PROP_MAX_CONTENT_WIDTH] =
      g_param_spec_int ("max-content-width", NULL, NULL,
                        -1, G_MAXINT, -1,
                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkScrolledWindow:max-content-height: (attributes org.gtk.Property.get=gtk_scrolled_window_get_max_content_height org.gtk.Property.set=gtk_scrolled_window_set_max_content_height)
   *
   * The maximum content height of @scrolled_window.
   */
  properties[PROP_MAX_CONTENT_HEIGHT] =
      g_param_spec_int ("max-content-height", NULL, NULL,
                        -1, G_MAXINT, -1,
                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkScrolledWindow:propagate-natural-width: (attributes org.gtk.Property.get=gtk_scrolled_window_get_propagate_natural_width org.gtk.Property.set=gtk_scrolled_window_set_propagate_natural_width)
   *
   * Whether the natural width of the child should be calculated and propagated
   * through the scrolled window’s requested natural width.
   *
   * This is useful in cases where an attempt should be made to allocate exactly
   * enough space for the natural size of the child.
   */
  properties[PROP_PROPAGATE_NATURAL_WIDTH] =
      g_param_spec_boolean ("propagate-natural-width", NULL, NULL,
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkScrolledWindow:propagate-natural-height: (attributes org.gtk.Property.get=gtk_scrolled_window_get_propagate_natural_height org.gtk.Property.set=gtk_scrolled_window_set_propagate_natural_height)
   *
   * Whether the natural height of the child should be calculated and propagated
   * through the scrolled window’s requested natural height.
   *
   * This is useful in cases where an attempt should be made to allocate exactly
   * enough space for the natural size of the child.
   */
  properties[PROP_PROPAGATE_NATURAL_HEIGHT] =
      g_param_spec_boolean ("propagate-natural-height", NULL, NULL,
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkScrolledWindow:child: (attributes org.gtk.Property.get=gtk_scrolled_window_get_child org.gtk.Property.set=gtk_scrolled_window_set_child)
   *
   * The child widget.
   *
   * When setting this property, if the child widget does not implement
   * [iface@Gtk.Scrollable], the scrolled window will add the child to
   * a [class@Gtk.Viewport] and then set the viewport as the child.
   */
  properties[PROP_CHILD] =
      g_param_spec_object ("child", NULL, NULL,
                           GTK_TYPE_WIDGET,
                           GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, NUM_PROPERTIES, properties);

  /**
   * GtkScrolledWindow::scroll-child:
   * @scrolled_window: a `GtkScrolledWindow`
   * @scroll: a `GtkScrollType` describing how much to scroll
   * @horizontal: whether the keybinding scrolls the child
   *   horizontally or not
   *
   * Emitted when a keybinding that scrolls is pressed.
   *
   * This is a [keybinding signal](class.SignalAction.html).
   *
   * The horizontal or vertical adjustment is updated which triggers a
   * signal that the scrolled window’s child may listen to and scroll itself.
   */
  signals[SCROLL_CHILD] =
    g_signal_new (I_("scroll-child"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkScrolledWindowClass, scroll_child),
                  NULL, NULL,
                  _gtk_marshal_BOOLEAN__ENUM_BOOLEAN,
                  G_TYPE_BOOLEAN, 2,
                  GTK_TYPE_SCROLL_TYPE,
                  G_TYPE_BOOLEAN);
  g_signal_set_va_marshaller (signals[SCROLL_CHILD],
                              G_TYPE_FROM_CLASS (gobject_class),
                              _gtk_marshal_BOOLEAN__ENUM_BOOLEANv);

  /**
   * GtkScrolledWindow::move-focus-out:
   * @scrolled_window: a `GtkScrolledWindow`
   * @direction_type: either %GTK_DIR_TAB_FORWARD or
   *   %GTK_DIR_TAB_BACKWARD
   *
   * Emitted when focus is moved away from the scrolled window by a
   * keybinding.
   *
   * This is a [keybinding signal](class.SignalAction.html).
   *
   * The default bindings for this signal are
   * `Ctrl + Tab` to move forward and `Ctrl + Shift + Tab` to
   * move backward.
   */
  signals[MOVE_FOCUS_OUT] =
    g_signal_new (I_("move-focus-out"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkScrolledWindowClass, move_focus_out),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 1,
                  GTK_TYPE_DIRECTION_TYPE);

  /**
   * GtkScrolledWindow::edge-overshot:
   * @scrolled_window: a `GtkScrolledWindow`
   * @pos: edge side that was hit
   *
   * Emitted whenever user initiated scrolling makes the scrolled
   * window firmly surpass the limits defined by the adjustment
   * in that orientation.
   *
   * A similar behavior without edge resistance is provided by the
   * [signal@Gtk.ScrolledWindow::edge-reached] signal.
   *
   * Note: The @pos argument is LTR/RTL aware, so callers should be
   * aware too if intending to provide behavior on horizontal edges.
   */
  signals[EDGE_OVERSHOT] =
    g_signal_new (I_("edge-overshot"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST, 0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1, GTK_TYPE_POSITION_TYPE);

  /**
   * GtkScrolledWindow::edge-reached:
   * @scrolled_window: a `GtkScrolledWindow`
   * @pos: edge side that was reached
   *
   * Emitted whenever user-initiated scrolling makes the scrolled
   * window exactly reach the lower or upper limits defined by the
   * adjustment in that orientation.
   *
   * A similar behavior with edge resistance is provided by the
   * [signal@Gtk.ScrolledWindow::edge-overshot] signal.
   *
   * Note: The @pos argument is LTR/RTL aware, so callers should be
   * aware too if intending to provide behavior on horizontal edges.
   */
  signals[EDGE_REACHED] =
    g_signal_new (I_("edge-reached"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST, 0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1, GTK_TYPE_POSITION_TYPE);

  add_scroll_binding (widget_class, GDK_KEY_Left,  GDK_CONTROL_MASK, GTK_SCROLL_STEP_BACKWARD, TRUE);
  add_scroll_binding (widget_class, GDK_KEY_Right, GDK_CONTROL_MASK, GTK_SCROLL_STEP_FORWARD,  TRUE);
  add_scroll_binding (widget_class, GDK_KEY_Up,    GDK_CONTROL_MASK, GTK_SCROLL_STEP_BACKWARD, FALSE);
  add_scroll_binding (widget_class, GDK_KEY_Down,  GDK_CONTROL_MASK, GTK_SCROLL_STEP_FORWARD,  FALSE);

  add_scroll_binding (widget_class, GDK_KEY_Page_Up,   GDK_CONTROL_MASK, GTK_SCROLL_PAGE_BACKWARD, TRUE);
  add_scroll_binding (widget_class, GDK_KEY_Page_Down, GDK_CONTROL_MASK, GTK_SCROLL_PAGE_FORWARD,  TRUE);
  add_scroll_binding (widget_class, GDK_KEY_Page_Up,   0,                GTK_SCROLL_PAGE_BACKWARD, FALSE);
  add_scroll_binding (widget_class, GDK_KEY_Page_Down, 0,                GTK_SCROLL_PAGE_FORWARD,  FALSE);

  add_scroll_binding (widget_class, GDK_KEY_Home, GDK_CONTROL_MASK, GTK_SCROLL_START, TRUE);
  add_scroll_binding (widget_class, GDK_KEY_End,  GDK_CONTROL_MASK, GTK_SCROLL_END,   TRUE);
  add_scroll_binding (widget_class, GDK_KEY_Home, 0,                GTK_SCROLL_START, FALSE);
  add_scroll_binding (widget_class, GDK_KEY_End,  0,                GTK_SCROLL_END,   FALSE);

  add_tab_bindings (widget_class, GDK_CONTROL_MASK, GTK_DIR_TAB_FORWARD);
  add_tab_bindings (widget_class, GDK_CONTROL_MASK | GDK_SHIFT_MASK, GTK_DIR_TAB_BACKWARD);

  gtk_widget_class_set_css_name (widget_class, I_("scrolledwindow"));
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_GENERIC);
}

static gboolean
may_hscroll (GtkScrolledWindow *scrolled_window)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  return priv->hscrollbar_visible || priv->hscrollbar_policy == GTK_POLICY_EXTERNAL;
}

static gboolean
may_vscroll (GtkScrolledWindow *scrolled_window)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  return priv->vscrollbar_visible || priv->vscrollbar_policy == GTK_POLICY_EXTERNAL;
}

static inline gboolean
policy_may_be_visible (GtkPolicyType policy)
{
  return policy == GTK_POLICY_ALWAYS || policy == GTK_POLICY_AUTOMATIC;
}

static void
scrolled_window_drag_begin_cb (GtkScrolledWindow *scrolled_window,
                               double             start_x,
                               double             start_y,
                               GtkGesture        *gesture)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);
  GdkEventSequence *sequence;
  GtkWidget *event_widget;

  priv->drag_start_x = priv->unclamped_hadj_value;
  priv->drag_start_y = priv->unclamped_vadj_value;
  gtk_scrolled_window_cancel_deceleration (scrolled_window);
  sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));
  event_widget = gtk_gesture_get_last_target (gesture, sequence);

  if (event_widget == priv->vscrollbar || event_widget == priv->hscrollbar ||
      (!may_hscroll (scrolled_window) && !may_vscroll (scrolled_window)))
    gtk_gesture_set_state (gesture, GTK_EVENT_SEQUENCE_DENIED);
}

static void
gtk_scrolled_window_invalidate_overshoot (GtkScrolledWindow *scrolled_window)
{
  GtkAllocation child_allocation;
  int overshoot_x, overshoot_y;

  if (!_gtk_scrolled_window_get_overshoot (scrolled_window, &overshoot_x, &overshoot_y))
    return;

  gtk_scrolled_window_relative_allocation (scrolled_window,
                                           &child_allocation);
  if (overshoot_x != 0)
    gtk_widget_queue_draw (GTK_WIDGET (scrolled_window));

  if (overshoot_y != 0)
    gtk_widget_queue_draw (GTK_WIDGET (scrolled_window));
}

static void
scrolled_window_drag_update_cb (GtkScrolledWindow *scrolled_window,
                                double             offset_x,
                                double             offset_y,
                                GtkGesture        *gesture)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);
  GdkEventSequence *sequence;
  GtkAdjustment *hadjustment;
  GtkAdjustment *vadjustment;
  double dx, dy;

  sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));

  if (gtk_gesture_get_sequence_state (gesture, sequence) != GTK_EVENT_SEQUENCE_CLAIMED &&
      !gtk_drag_check_threshold_double (GTK_WIDGET (scrolled_window),
                                        0, 0, offset_x, offset_y))
    return;

  gtk_scrolled_window_invalidate_overshoot (scrolled_window);
  gtk_gesture_set_state (gesture, GTK_EVENT_SEQUENCE_CLAIMED);

  hadjustment = gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (priv->hscrollbar));
  if (hadjustment && may_hscroll (scrolled_window))
    {
      dx = priv->drag_start_x - offset_x;
      _gtk_scrolled_window_set_adjustment_value (scrolled_window,
                                                 hadjustment, dx);
    }

  vadjustment = gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (priv->vscrollbar));
  if (vadjustment && may_vscroll (scrolled_window))
    {
      dy = priv->drag_start_y - offset_y;
      _gtk_scrolled_window_set_adjustment_value (scrolled_window,
                                                 vadjustment, dy);
    }

  gtk_scrolled_window_invalidate_overshoot (scrolled_window);
}

static void
gtk_scrolled_window_decelerate (GtkScrolledWindow *scrolled_window,
                                double             x_velocity,
                                double             y_velocity)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);
  gboolean overshoot;

  overshoot = _gtk_scrolled_window_get_overshoot (scrolled_window, NULL, NULL);
  priv->x_velocity = x_velocity;
  priv->y_velocity = y_velocity;

  /* Zero out vector components for which we don't scroll */
  if (!may_hscroll (scrolled_window))
    priv->x_velocity = 0;
  if (!may_vscroll (scrolled_window))
    priv->y_velocity = 0;

  if (priv->x_velocity != 0 || priv->y_velocity != 0 || overshoot)
    {
      if (priv->deceleration_id == 0)
        gtk_scrolled_window_start_deceleration (scrolled_window);
      priv->x_velocity = priv->y_velocity = 0;
    }
  else
    {
      g_clear_pointer (&priv->hscrolling, gtk_kinetic_scrolling_free);
      g_clear_pointer (&priv->vscrolling, gtk_kinetic_scrolling_free);
    }
}

static void
scrolled_window_swipe_cb (GtkScrolledWindow *scrolled_window,
                          double             x_velocity,
                          double             y_velocity)
{
  gtk_scrolled_window_decelerate (scrolled_window, -x_velocity, -y_velocity);
}

static void
scrolled_window_long_press_cb (GtkScrolledWindow *scrolled_window,
                               double             x,
                               double             y,
                               GtkGesture        *gesture)
{
  gtk_gesture_set_state (gesture, GTK_EVENT_SEQUENCE_DENIED);
}

static void
scrolled_window_long_press_cancelled_cb (GtkScrolledWindow *scrolled_window,
                                         GtkGesture        *gesture)
{
  GdkEventSequence *sequence;
  GdkEvent *event;
  GdkEventType event_type;

  sequence = gtk_gesture_get_last_updated_sequence (gesture);
  event = gtk_gesture_get_last_event (gesture, sequence);
  event_type = gdk_event_get_event_type (event);

  if (event_type == GDK_TOUCH_BEGIN ||
      event_type == GDK_BUTTON_PRESS)
    gtk_gesture_set_state (gesture, GTK_EVENT_SEQUENCE_DENIED);
}

static void
gtk_scrolled_window_check_attach_pan_gesture (GtkScrolledWindow *sw)
{
  GtkPropagationPhase phase = GTK_PHASE_NONE;
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (sw);

  if (priv->kinetic_scrolling &&
      ((may_hscroll (sw) && !may_vscroll (sw)) ||
       (!may_hscroll (sw) && may_vscroll (sw))))
    {
      GtkOrientation orientation;

      if (may_hscroll (sw))
        orientation = GTK_ORIENTATION_HORIZONTAL;
      else
        orientation = GTK_ORIENTATION_VERTICAL;

      gtk_gesture_pan_set_orientation (GTK_GESTURE_PAN (priv->pan_gesture),
                                       orientation);
      phase = GTK_PHASE_CAPTURE;
    }

  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (priv->pan_gesture), phase);
}

static void
indicator_set_over (Indicator *indicator,
                    gboolean   over)
{
  g_clear_handle_id (&indicator->over_timeout_id, g_source_remove);

  if (indicator->over == over)
    return;

  indicator->over = over;

  if (indicator->over)
    gtk_widget_add_css_class (indicator->scrollbar, "hovering");
  else
    gtk_widget_remove_css_class (indicator->scrollbar, "hovering");

  gtk_widget_queue_resize (indicator->scrollbar);
}

static gboolean
coords_close_to_indicator (GtkScrolledWindow *sw,
                           Indicator         *indicator,
                           double             x,
                           double             y)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (sw);
  graphene_rect_t indicator_bounds;
  int distance;

  if (!gtk_widget_compute_bounds (indicator->scrollbar, GTK_WIDGET (sw), &indicator_bounds))
    return FALSE;

  if (indicator->over)
    distance = INDICATOR_FAR_DISTANCE;
  else
    distance = INDICATOR_CLOSE_DISTANCE;

  graphene_rect_inset (&indicator_bounds, - distance, - distance);

  if (indicator == &priv->hindicator)
    {
      if (y >= indicator_bounds.origin.y &&
          y < indicator_bounds.origin.y + indicator_bounds.size.height)
         return TRUE;
    }
  else if (indicator == &priv->vindicator)
    {
      if (x >= indicator_bounds.origin.x &&
          x < indicator_bounds.origin.x + indicator_bounds.size.width)
        return TRUE;
    }

  return FALSE;
}

static gboolean
enable_over_timeout_cb (gpointer user_data)
{
  Indicator *indicator = user_data;

  indicator_set_over (indicator, TRUE);
  return G_SOURCE_REMOVE;
}

static gboolean
check_update_scrollbar_proximity (GtkScrolledWindow *sw,
                                  Indicator         *indicator,
                                  GtkWidget         *target,
                                  double             x,
                                  double             y)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (sw);
  gboolean indicator_close, on_scrollbar, on_other_scrollbar;

  indicator_close = coords_close_to_indicator (sw, indicator, x, y);
  on_scrollbar = (target == indicator->scrollbar ||
                  gtk_widget_is_ancestor (target, indicator->scrollbar));
  on_other_scrollbar = (!on_scrollbar &&
                        (target == priv->hindicator.scrollbar ||
                         target == priv->vindicator.scrollbar ||
                         gtk_widget_is_ancestor (target, priv->hindicator.scrollbar) ||
                         gtk_widget_is_ancestor (target, priv->vindicator.scrollbar)));


  g_clear_handle_id (&indicator->over_timeout_id, g_source_remove);

  if (on_scrollbar)
    indicator_set_over (indicator, TRUE);
  else if (indicator_close && !on_other_scrollbar)
    {
      indicator->over_timeout_id = g_timeout_add (30, enable_over_timeout_cb, indicator);
      gdk_source_set_static_name_by_id (indicator->over_timeout_id, "[gtk] enable_over_timeout_cb");
    }
  else
    indicator_set_over (indicator, FALSE);

  return indicator_close;
}

static double
get_wheel_detent_scroll_step (GtkScrolledWindow *sw,
                              GtkOrientation     orientation)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (sw);
  GtkScrollbar *scrollbar;
  GtkAdjustment *adj;
  double page_size;
  double scroll_step;

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    scrollbar = GTK_SCROLLBAR (priv->hscrollbar);
  else
    scrollbar = GTK_SCROLLBAR (priv->vscrollbar);

  if (!scrollbar)
    return 0;

  adj = gtk_scrollbar_get_adjustment (scrollbar);
  page_size = gtk_adjustment_get_page_size (adj);
  scroll_step = pow (page_size, 2.0 / 3.0);

  return scroll_step;
}

static gboolean
captured_scroll_cb (GtkEventControllerScroll *scroll,
                    double                    delta_x,
                    double                    delta_y,
                    GtkScrolledWindow        *scrolled_window)
{
  GtkScrolledWindowPrivate *priv =
    gtk_scrolled_window_get_instance_private (scrolled_window);

  gtk_scrolled_window_cancel_deceleration (scrolled_window);

  if (!may_hscroll (scrolled_window) &&
      !may_vscroll (scrolled_window))
    return GDK_EVENT_PROPAGATE;

  if (priv->smooth_scroll)
    {
      scrolled_window_scroll (scrolled_window, delta_x, delta_y, scroll);
      return GDK_EVENT_STOP;
    }

  return GDK_EVENT_PROPAGATE;
}

static void
captured_motion (GtkEventController *controller,
                 double              x,
                 double              y,
                 GtkScrolledWindow  *sw)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (sw);
  GdkDevice *source_device;
  GdkInputSource input_source;
  GdkModifierType state;
  GdkEvent *event;
  GtkWidget *target;

  if (!priv->use_indicators)
    return;

  if (!priv->child)
    return;

  target = gtk_event_controller_get_target (controller);
  state = gtk_event_controller_get_current_event_state (controller);
  event = gtk_event_controller_get_current_event (controller);

  source_device = gdk_event_get_device (event);
  input_source = gdk_device_get_source (source_device);

  if (priv->hscrollbar_visible)
    indicator_start_fade (&priv->hindicator, 1.0);
  if (priv->vscrollbar_visible)
    indicator_start_fade (&priv->vindicator, 1.0);

  if ((target == priv->child ||
       gtk_widget_is_ancestor (target, priv->child)) &&
      (state & (GDK_BUTTON1_MASK | GDK_BUTTON2_MASK | GDK_BUTTON3_MASK)) != 0)
    {
      indicator_set_over (&priv->hindicator, FALSE);
      indicator_set_over (&priv->vindicator, FALSE);
    }
  else if (input_source == GDK_SOURCE_PEN ||
           input_source == GDK_SOURCE_TRACKPOINT)
    {
      indicator_set_over (&priv->hindicator, TRUE);
      indicator_set_over (&priv->vindicator, TRUE);
    }
  else
    {
      if (!check_update_scrollbar_proximity (sw, &priv->vindicator, target, x, y))
        check_update_scrollbar_proximity (sw, &priv->hindicator, target, x, y);
      else
        indicator_set_over (&priv->hindicator, FALSE);
    }
}

static gboolean
start_scroll_deceleration_cb (gpointer user_data)
{
  GtkScrolledWindow *scrolled_window = user_data;
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  priv->scroll_events_overshoot_id = 0;

  if (!priv->deceleration_id)
    gtk_scrolled_window_start_deceleration (scrolled_window);

  return FALSE;
}

static void
scroll_controller_scroll_begin (GtkEventControllerScroll *scroll,
                                GtkScrolledWindow        *scrolled_window)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  priv->smooth_scroll = TRUE;
}

static void
stop_kinetic_scrolling_cb (GtkEventControllerScroll *scroll,
                           GtkScrolledWindow        *scrolled_window)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  if (priv->hscrolling)
    gtk_kinetic_scrolling_stop (priv->hscrolling);

  if (priv->vscrolling)
    gtk_kinetic_scrolling_stop (priv->vscrolling);
}

static void
scrolled_window_scroll (GtkScrolledWindow        *scrolled_window,
                        double                    delta_x,
                        double                    delta_y,
                        GtkEventControllerScroll *scroll)
{
  GtkScrolledWindowPrivate *priv =
    gtk_scrolled_window_get_instance_private (scrolled_window);
  gboolean shifted;
  GdkModifierType state;

  state = gtk_event_controller_get_current_event_state (GTK_EVENT_CONTROLLER (scroll));
  shifted = (state & GDK_SHIFT_MASK) != 0;

  gtk_scrolled_window_invalidate_overshoot (scrolled_window);

  if (shifted)
    {
      double delta;

      delta = delta_x;
      delta_x = delta_y;
      delta_y = delta;
    }

  if (delta_x != 0.0 &&
      may_hscroll (scrolled_window))
    {
      GtkAdjustment *adj;
      double new_value;
      GdkScrollUnit scroll_unit;

      adj = gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (priv->hscrollbar));
      scroll_unit = gtk_event_controller_scroll_get_unit (scroll);

      if (scroll_unit == GDK_SCROLL_UNIT_WHEEL)
        {
          delta_x *= get_wheel_detent_scroll_step (scrolled_window,
                                                   GTK_ORIENTATION_HORIZONTAL);
        }
      else if (scroll_unit == GDK_SCROLL_UNIT_SURFACE)
        delta_x *= MAGIC_SCROLL_FACTOR;

      new_value = priv->unclamped_hadj_value + delta_x;
      _gtk_scrolled_window_set_adjustment_value (scrolled_window, adj,
                                                 new_value);
    }

  if (delta_y != 0.0 &&
      may_vscroll (scrolled_window))
    {
      GtkAdjustment *adj;
      double new_value;
      GdkScrollUnit scroll_unit;

      adj = gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (priv->vscrollbar));
      scroll_unit = gtk_event_controller_scroll_get_unit (scroll);

      if (scroll_unit == GDK_SCROLL_UNIT_WHEEL)
        {
          delta_y *= get_wheel_detent_scroll_step (scrolled_window,
                                                   GTK_ORIENTATION_VERTICAL);
        }
      else if (scroll_unit == GDK_SCROLL_UNIT_SURFACE)
        delta_y *= MAGIC_SCROLL_FACTOR;

      new_value = priv->unclamped_vadj_value + delta_y;
      _gtk_scrolled_window_set_adjustment_value (scrolled_window, adj,
                                                 new_value);
    }

  g_clear_handle_id (&priv->scroll_events_overshoot_id, g_source_remove);

  if (!priv->smooth_scroll &&
      _gtk_scrolled_window_get_overshoot (scrolled_window, NULL, NULL))
    {
      priv->scroll_events_overshoot_id =
        g_timeout_add (50, start_scroll_deceleration_cb, scrolled_window);
      gdk_source_set_static_name_by_id (priv->scroll_events_overshoot_id,
                                      "[gtk] start_scroll_deceleration_cb");
    }
}

static gboolean
scroll_controller_scroll (GtkEventControllerScroll *scroll,
                          double                    delta_x,
                          double                    delta_y,
                          GtkScrolledWindow        *scrolled_window)
{
  GtkScrolledWindowPrivate *priv =
    gtk_scrolled_window_get_instance_private (scrolled_window);

  if (!may_hscroll (scrolled_window) &&
      !may_vscroll (scrolled_window))
    return GDK_EVENT_PROPAGATE;

  if (!priv->smooth_scroll)
    scrolled_window_scroll (scrolled_window, delta_x, delta_y, scroll);

  return GDK_EVENT_STOP;
}

static void
scroll_controller_scroll_end (GtkEventControllerScroll *scroll,
                              GtkScrolledWindow        *scrolled_window)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  priv->smooth_scroll = FALSE;
}

static void
scroll_controller_decelerate (GtkEventControllerScroll *scroll,
                              double                    initial_vel_x,
                              double                    initial_vel_y,
                              GtkScrolledWindow        *scrolled_window)
{
  GdkScrollUnit scroll_unit;
  gboolean shifted;
  GdkModifierType state;

  scroll_unit = gtk_event_controller_scroll_get_unit (scroll);
  state = gtk_event_controller_get_current_event_state (GTK_EVENT_CONTROLLER (scroll));

  shifted = (state & GDK_SHIFT_MASK) != 0;

  if (shifted)
    {
      double tmp;

      tmp = initial_vel_x;
      initial_vel_x = initial_vel_y;
      initial_vel_y = tmp;
    }

  if (scroll_unit == GDK_SCROLL_UNIT_WHEEL)
    {
      initial_vel_x *= get_wheel_detent_scroll_step (scrolled_window,
                                                     GTK_ORIENTATION_HORIZONTAL);

      initial_vel_y *= get_wheel_detent_scroll_step (scrolled_window,
                                                     GTK_ORIENTATION_VERTICAL);
    }
  else
    {
      initial_vel_x *= MAGIC_SCROLL_FACTOR;
      initial_vel_y *= MAGIC_SCROLL_FACTOR;
    }

  gtk_scrolled_window_decelerate (scrolled_window,
                                  initial_vel_x,
                                  initial_vel_y);
}

static void
gtk_scrolled_window_update_scrollbar_visibility_flags (GtkScrolledWindow *scrolled_window,
                                                       GtkWidget         *scrollbar)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);
  GtkAdjustment *adjustment;

  if (scrollbar == NULL)
    return;

  adjustment = gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (scrollbar));

  if (scrollbar == priv->hscrollbar)
    {
      if (priv->hscrollbar_policy == GTK_POLICY_AUTOMATIC)
        {
          priv->hscrollbar_visible = (gtk_adjustment_get_upper (adjustment) - gtk_adjustment_get_lower (adjustment) >
                                      gtk_adjustment_get_page_size (adjustment));
        }
    }
  else if (scrollbar == priv->vscrollbar)
    {
      if (priv->vscrollbar_policy == GTK_POLICY_AUTOMATIC)
        {
          priv->vscrollbar_visible = (gtk_adjustment_get_upper (adjustment) - gtk_adjustment_get_lower (adjustment) >
                                      gtk_adjustment_get_page_size (adjustment));
        }
    }
}

static void
gtk_scrolled_window_size_allocate (GtkWidget *widget,
                                   int        width,
                                   int        height,
                                   int        baseline)
{
  GtkScrolledWindow *scrolled_window = GTK_SCROLLED_WINDOW (widget);
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);
  GtkAllocation child_allocation;
  int sb_width;
  int sb_height;

  /* Get possible scrollbar dimensions */
  gtk_widget_measure (priv->vscrollbar, GTK_ORIENTATION_HORIZONTAL, -1,
                      &sb_width, NULL, NULL, NULL);
  gtk_widget_measure (priv->hscrollbar, GTK_ORIENTATION_VERTICAL, -1,
                      &sb_height, NULL, NULL, NULL);

  if (priv->hscrollbar_policy == GTK_POLICY_ALWAYS)
    priv->hscrollbar_visible = TRUE;
  else if (priv->hscrollbar_policy == GTK_POLICY_NEVER ||
           priv->hscrollbar_policy == GTK_POLICY_EXTERNAL)
    priv->hscrollbar_visible = FALSE;

  if (priv->vscrollbar_policy == GTK_POLICY_ALWAYS)
    priv->vscrollbar_visible = TRUE;
  else if (priv->vscrollbar_policy == GTK_POLICY_NEVER ||
           priv->vscrollbar_policy == GTK_POLICY_EXTERNAL)
    priv->vscrollbar_visible = FALSE;

  if (priv->child && gtk_widget_get_visible (priv->child))
    {
      int child_scroll_width;
      int child_scroll_height;
      gboolean previous_hvis;
      gboolean previous_vvis;
      guint count = 0;
      GtkScrollable *scrollable_child = GTK_SCROLLABLE (priv->child);
      GtkScrollablePolicy hscroll_policy = gtk_scrollable_get_hscroll_policy (scrollable_child);
      GtkScrollablePolicy vscroll_policy = gtk_scrollable_get_vscroll_policy (scrollable_child);

      /* Determine scrollbar visibility first via hfw apis */
      if (gtk_widget_get_request_mode (priv->child) == GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH)
        {
          if (hscroll_policy == GTK_SCROLL_MINIMUM)
            gtk_widget_measure (priv->child, GTK_ORIENTATION_HORIZONTAL, -1,
                                &child_scroll_width, NULL, NULL, NULL);
          else
            gtk_widget_measure (priv->child, GTK_ORIENTATION_HORIZONTAL, -1,
                                NULL, &child_scroll_width, NULL, NULL);

          if (priv->vscrollbar_policy == GTK_POLICY_AUTOMATIC)
            {
              /* First try without a vertical scrollbar if the content will fit the height
               * given the extra width of the scrollbar */
              if (vscroll_policy == GTK_SCROLL_MINIMUM)
                gtk_widget_measure (priv->child, GTK_ORIENTATION_VERTICAL,
                                    MAX (width, child_scroll_width),
                                    &child_scroll_height, NULL, NULL, NULL);
              else
                gtk_widget_measure (priv->child, GTK_ORIENTATION_VERTICAL,
                                    MAX (width, child_scroll_width),
                                    NULL, &child_scroll_height, NULL, NULL);

              if (priv->hscrollbar_policy == GTK_POLICY_AUTOMATIC)
                {
                  /* Does the content height fit the allocation height ? */
                  priv->vscrollbar_visible = child_scroll_height > height;

                  /* Does the content width fit the allocation with minus a possible scrollbar ? */
                  priv->hscrollbar_visible = child_scroll_width > width -
                    (priv->vscrollbar_visible && !priv->use_indicators ? sb_width : 0);

                  /* Now that we've guessed the hscrollbar, does the content height fit
                   * the possible new allocation height ?
                   */
                  priv->vscrollbar_visible = child_scroll_height > height -
                    (priv->hscrollbar_visible && !priv->use_indicators ? sb_height : 0);

                  /* Now that we've guessed the vscrollbar, does the content width fit
                   * the possible new allocation width ?
                   */
                  priv->hscrollbar_visible = child_scroll_width > width -
                    (priv->vscrollbar_visible && !priv->use_indicators ? sb_width : 0);
                }
              else /* priv->hscrollbar_policy != GTK_POLICY_AUTOMATIC */
                {
                  priv->hscrollbar_visible = policy_may_be_visible (priv->hscrollbar_policy);
                  priv->vscrollbar_visible = child_scroll_height > height -
                    (priv->hscrollbar_visible && !priv->use_indicators ? sb_height : 0);
                }
            }
          else /* priv->vscrollbar_policy != GTK_POLICY_AUTOMATIC */
            {
              priv->vscrollbar_visible = policy_may_be_visible (priv->vscrollbar_policy);

              if (priv->hscrollbar_policy == GTK_POLICY_AUTOMATIC)
                priv->hscrollbar_visible = child_scroll_width > width -
                  (priv->vscrollbar_visible && !priv->use_indicators ? 0 : sb_width);
              else
                priv->hscrollbar_visible = policy_may_be_visible (priv->hscrollbar_policy);
            }
        }
      else /* GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT */
        {
          if (vscroll_policy == GTK_SCROLL_MINIMUM)
            gtk_widget_measure (priv->child, GTK_ORIENTATION_VERTICAL, -1,
                                &child_scroll_height, NULL, NULL, NULL);
          else
            gtk_widget_measure (priv->child, GTK_ORIENTATION_VERTICAL, -1,
                                NULL, &child_scroll_height, NULL, NULL);

          if (priv->hscrollbar_policy == GTK_POLICY_AUTOMATIC)
            {
              /* First try without a horizontal scrollbar if the content will fit the width
               * given the extra height of the scrollbar */
              if (hscroll_policy == GTK_SCROLL_MINIMUM)
                gtk_widget_measure (priv->child, GTK_ORIENTATION_HORIZONTAL,
                                    MAX (height, child_scroll_height),
                                    &child_scroll_width, NULL, NULL, NULL);
              else
                gtk_widget_measure (priv->child, GTK_ORIENTATION_HORIZONTAL,
                                    MAX (height, child_scroll_height),
                                    NULL, &child_scroll_width, NULL, NULL);

              if (priv->vscrollbar_policy == GTK_POLICY_AUTOMATIC)
                {
                  /* Does the content width fit the allocation width ? */
                  priv->hscrollbar_visible = child_scroll_width > width;

                  /* Does the content height fit the allocation with minus a possible scrollbar ? */
                  priv->vscrollbar_visible = child_scroll_height > height -
                    (priv->hscrollbar_visible && !priv->use_indicators ? sb_height : 0);

                  /* Now that we've guessed the vscrollbar, does the content width fit
                   * the possible new allocation width ?
                   */
                  priv->hscrollbar_visible = child_scroll_width > width -
                    (priv->vscrollbar_visible && !priv->use_indicators ? sb_width : 0);

                  /* Now that we've guessed the hscrollbar, does the content height fit
                   * the possible new allocation height ?
                   */
                  priv->vscrollbar_visible = child_scroll_height > height -
                    (priv->hscrollbar_visible && !priv->use_indicators ? sb_height : 0);
                }
              else /* priv->vscrollbar_policy != GTK_POLICY_AUTOMATIC */
                {
                  priv->vscrollbar_visible = policy_may_be_visible (priv->vscrollbar_policy);
                  priv->hscrollbar_visible = child_scroll_width > width -
                    (priv->vscrollbar_visible && !priv->use_indicators ? sb_width : 0);
                }
            }
          else /* priv->hscrollbar_policy != GTK_POLICY_AUTOMATIC */
            {
              priv->hscrollbar_visible = policy_may_be_visible (priv->hscrollbar_policy);

              if (priv->vscrollbar_policy == GTK_POLICY_AUTOMATIC)
                priv->vscrollbar_visible = child_scroll_height > height -
                  (priv->hscrollbar_visible && !priv->use_indicators ? sb_height : 0);
              else
                priv->vscrollbar_visible = policy_may_be_visible (priv->vscrollbar_policy);
            }
        }

      /* Now after guessing scrollbar visibility; fall back on the allocation loop which
       * observes the adjustments to detect scrollbar visibility and also avoids
       * infinite recursion
       */
      do
        {
          previous_hvis = priv->hscrollbar_visible;
          previous_vvis = priv->vscrollbar_visible;

          gtk_scrolled_window_allocate_child (scrolled_window, width, height);

          /* Explicitly force scrollbar visibility checks.
           *
           * Since we make a guess above, the child might not decide to update the adjustments
           * if they logically did not change since the last configuration
           *
           * These will update priv->hscrollbar_visible and priv->vscrollbar_visible.
           */
          gtk_scrolled_window_update_scrollbar_visibility_flags (scrolled_window,
                                                                 priv->hscrollbar);

          gtk_scrolled_window_update_scrollbar_visibility_flags (scrolled_window,
                                                                 priv->vscrollbar);

          /* If, after the first iteration, the hscrollbar and the
           * vscrollbar flip visibility... or if one of the scrollbars flip
           * on each iteration indefinitely/infinitely, then we just need both
           * at this size.
           */
          if ((count &&
               previous_hvis != priv->hscrollbar_visible &&
               previous_vvis != priv->vscrollbar_visible) || count > 3)
            {
              priv->hscrollbar_visible = TRUE;
              priv->vscrollbar_visible = TRUE;

              gtk_scrolled_window_allocate_child (scrolled_window, width, height);

              break;
            }

          count++;
        }
      while (previous_hvis != priv->hscrollbar_visible ||
             previous_vvis != priv->vscrollbar_visible);
    }
  else
    {
      priv->hscrollbar_visible = priv->hscrollbar_policy == GTK_POLICY_ALWAYS;
      priv->vscrollbar_visible = priv->vscrollbar_policy == GTK_POLICY_ALWAYS;
    }

  gtk_widget_set_child_visible (priv->hscrollbar, priv->hscrollbar_visible);
  if (priv->hscrollbar_visible)
    {
      gtk_scrolled_window_allocate_scrollbar (scrolled_window,
                                              priv->hscrollbar,
                                              &child_allocation);
      gtk_widget_size_allocate (priv->hscrollbar, &child_allocation, -1);
    }

  gtk_widget_set_child_visible (priv->vscrollbar, priv->vscrollbar_visible);
  if (priv->vscrollbar_visible)
    {
      gtk_scrolled_window_allocate_scrollbar (scrolled_window,
                                              priv->vscrollbar,
                                              &child_allocation);
      gtk_widget_size_allocate (priv->vscrollbar, &child_allocation, -1);
    }

  gtk_scrolled_window_check_attach_pan_gesture (scrolled_window);
}

static void
gtk_scrolled_window_measure (GtkWidget      *widget,
                             GtkOrientation  orientation,
                             int             for_size,
                             int            *minimum_size,
                             int            *natural_size,
                             int            *minimum_baseline,
                             int            *natural_baseline)
{
  GtkScrolledWindow *scrolled_window = GTK_SCROLLED_WINDOW (widget);
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);
  int minimum_req = 0, natural_req = 0;
  GtkBorder sborder = { 0 };

  if (priv->child)
    gtk_scrollable_get_border (GTK_SCROLLABLE (priv->child), &sborder);

  /*
   * First collect the child requisition
   */
  if (priv->child && gtk_widget_get_visible (priv->child))
    {
      int min_child_size, nat_child_size;
      int child_for_size = -1;

      /* We can pass on the requested size if we have a scrollbar policy that prevents scrolling in that direction */
      if ((orientation == GTK_ORIENTATION_VERTICAL && priv->hscrollbar_policy == GTK_POLICY_NEVER)
          || (orientation == GTK_ORIENTATION_HORIZONTAL && priv->vscrollbar_policy == GTK_POLICY_NEVER))
        {
          child_for_size = for_size;

          /* If the other scrollbar is always visible and not an overlay scrollbar we must subtract it from the measure */
          if (orientation == GTK_ORIENTATION_VERTICAL && !priv->use_indicators && priv->vscrollbar_policy == GTK_POLICY_ALWAYS)
            {
              int min_scrollbar_width;

              gtk_widget_measure (priv->vscrollbar, GTK_ORIENTATION_HORIZONTAL, -1,
                                  &min_scrollbar_width, NULL,
                                  NULL, NULL);

              child_for_size = MAX (0, child_for_size - min_scrollbar_width);
            }
          if (orientation == GTK_ORIENTATION_HORIZONTAL && !priv->use_indicators && priv->hscrollbar_policy == GTK_POLICY_ALWAYS)
            {
              int min_scrollbar_height;

              gtk_widget_measure (priv->hscrollbar, GTK_ORIENTATION_VERTICAL, -1,
                              &min_scrollbar_height, NULL,
                              NULL, NULL);

              child_for_size = MAX (0, child_for_size - min_scrollbar_height);
            }
        }

      gtk_widget_measure (priv->child, orientation, child_for_size,
                          &min_child_size, &nat_child_size,
                          NULL, NULL);

      if (orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          if (priv->propagate_natural_width)
            natural_req += nat_child_size;

          if (priv->hscrollbar_policy == GTK_POLICY_NEVER)
            {
              minimum_req += min_child_size;
            }
          else
            {
              int min = priv->min_content_width >= 0 ? priv->min_content_width : 0;
              int max = priv->max_content_width >= 0 ? priv->max_content_width : G_MAXINT;

              minimum_req = CLAMP (minimum_req, min, max);
              natural_req = CLAMP (natural_req, min, max);
            }
        }
      else /* GTK_ORIENTATION_VERTICAL */
        {
          if (priv->propagate_natural_height)
            natural_req += nat_child_size;

          if (priv->vscrollbar_policy == GTK_POLICY_NEVER)
            {
              minimum_req += min_child_size;
            }
          else
            {
              int min = priv->min_content_height >= 0 ? priv->min_content_height : 0;
              int max = priv->max_content_height >= 0 ? priv->max_content_height : G_MAXINT;

              minimum_req = CLAMP (minimum_req, min, max);
              natural_req = CLAMP (natural_req, min, max);
            }
        }
    }

  /* Ensure we make requests with natural size >= minimum size */
  natural_req = MAX (minimum_req, natural_req);

  /*
   * Now add to the requisition any additional space for surrounding scrollbars
   * and the special scrollable border.
   */
  if (policy_may_be_visible (priv->hscrollbar_policy))
    {
      if (orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          int min_scrollbar_width, nat_scrollbar_width;

          gtk_widget_measure (priv->hscrollbar, GTK_ORIENTATION_HORIZONTAL, -1,
                              &min_scrollbar_width, &nat_scrollbar_width,
                              NULL, NULL);
          minimum_req = MAX (minimum_req, min_scrollbar_width + sborder.left + sborder.right);
          natural_req = MAX (natural_req, nat_scrollbar_width + sborder.left + sborder.right);
        }
      else if (!priv->use_indicators && priv->hscrollbar_policy == GTK_POLICY_ALWAYS)
        {
          int min_scrollbar_height, nat_scrollbar_height;

          gtk_widget_measure (priv->hscrollbar, GTK_ORIENTATION_VERTICAL, -1,
                              &min_scrollbar_height, &nat_scrollbar_height,
                              NULL, NULL);

          minimum_req += min_scrollbar_height;
          natural_req += nat_scrollbar_height;
        }
    }

  if (policy_may_be_visible (priv->vscrollbar_policy))
    {
      if (orientation == GTK_ORIENTATION_VERTICAL)
        {
          int min_scrollbar_height, nat_scrollbar_height;

          gtk_widget_measure (priv->vscrollbar, GTK_ORIENTATION_VERTICAL, -1,
                              &min_scrollbar_height, &nat_scrollbar_height,
                              NULL, NULL);
          minimum_req = MAX (minimum_req, min_scrollbar_height + sborder.top + sborder.bottom);
          natural_req = MAX (natural_req, nat_scrollbar_height + sborder.top + sborder.bottom);
        }
      else if (!priv->use_indicators && priv->vscrollbar_policy == GTK_POLICY_ALWAYS)
        {
          int min_scrollbar_width, nat_scrollbar_width;

          gtk_widget_measure (priv->vscrollbar, GTK_ORIENTATION_HORIZONTAL, -1,
                              &min_scrollbar_width, &nat_scrollbar_width,
                              NULL, NULL);
          minimum_req += min_scrollbar_width;
          natural_req += nat_scrollbar_width;
        }
    }

  *minimum_size = minimum_req;
  *natural_size = natural_req;
}

static void
gtk_scrolled_window_snapshot_scrollbars_junction (GtkScrolledWindow *scrolled_window,
                                                  GtkSnapshot       *snapshot)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);
  graphene_rect_t hscr_bounds, vscr_bounds;
  GtkCssStyle *style;
  GtkCssBoxes boxes;

  if (!gtk_widget_compute_bounds (GTK_WIDGET (priv->hscrollbar), GTK_WIDGET (scrolled_window), &hscr_bounds))
    return;

  if (!gtk_widget_compute_bounds (GTK_WIDGET (priv->vscrollbar), GTK_WIDGET (scrolled_window), &vscr_bounds))
    return;

  style = gtk_css_node_get_style (priv->junction_node);

  gtk_css_boxes_init_border_box (&boxes, style,
                                 vscr_bounds.origin.x, hscr_bounds.origin.y,
                                 vscr_bounds.size.width, hscr_bounds.size.height);

  gtk_css_style_snapshot_background (&boxes, snapshot);
  gtk_css_style_snapshot_border (&boxes, snapshot);
}

static void
gtk_scrolled_window_snapshot_overshoot (GtkScrolledWindow *scrolled_window,
                                        GtkSnapshot       *snapshot)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);
  int overshoot_x, overshoot_y;
  GtkCssStyle *style;
  GdkRectangle rect;
  GtkCssBoxes boxes;

  if (!_gtk_scrolled_window_get_overshoot (scrolled_window, &overshoot_x, &overshoot_y))
    return;

  gtk_scrolled_window_inner_allocation (scrolled_window, &rect);

  overshoot_x = CLAMP (overshoot_x, - MAX_OVERSHOOT_DISTANCE, MAX_OVERSHOOT_DISTANCE);
  overshoot_y = CLAMP (overshoot_y, - MAX_OVERSHOOT_DISTANCE, MAX_OVERSHOOT_DISTANCE);

  if (overshoot_x > 0)
    {
      style = gtk_css_node_get_style (priv->overshoot_node[GTK_POS_RIGHT]);
      gtk_css_boxes_init_border_box (&boxes, style,
                                     rect.x + rect.width - overshoot_x, rect.y, overshoot_x, rect.height);
      gtk_css_style_snapshot_background (&boxes, snapshot);
      gtk_css_style_snapshot_border (&boxes, snapshot);
    }
  else if (overshoot_x < 0)
    {
      style = gtk_css_node_get_style (priv->overshoot_node[GTK_POS_LEFT]);
      gtk_css_boxes_init_border_box (&boxes, style,
                                     rect.x, rect.y, -overshoot_x, rect.height);
      gtk_css_style_snapshot_background (&boxes, snapshot);
      gtk_css_style_snapshot_border (&boxes, snapshot);
    }

  if (overshoot_y > 0)
    {
      style = gtk_css_node_get_style (priv->overshoot_node[GTK_POS_BOTTOM]);
      gtk_css_boxes_init_border_box (&boxes, style,
                                     rect.x, rect.y + rect.height - overshoot_y, rect.width, overshoot_y);
      gtk_css_style_snapshot_background (&boxes, snapshot);
      gtk_css_style_snapshot_border (&boxes, snapshot);
    }
  else if (overshoot_y < 0)
    {
      style = gtk_css_node_get_style (priv->overshoot_node[GTK_POS_TOP]);
      gtk_css_boxes_init_border_box (&boxes, style,
                                     rect.x, rect.y, rect.width, -overshoot_y);
      gtk_css_style_snapshot_background (&boxes, snapshot);
      gtk_css_style_snapshot_border (&boxes, snapshot);
    }
}

static void
gtk_scrolled_window_snapshot_undershoot (GtkScrolledWindow *scrolled_window,
                                         GtkSnapshot       *snapshot)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);
  GtkCssStyle *style;
  GdkRectangle rect;
  GtkAdjustment *adj;
  GtkCssBoxes boxes;

  gtk_scrolled_window_inner_allocation (scrolled_window, &rect);

  adj = gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (priv->hscrollbar));
  if (gtk_adjustment_get_value (adj) < gtk_adjustment_get_upper (adj) - gtk_adjustment_get_page_size (adj))
    {
      style = gtk_css_node_get_style (priv->undershoot_node[GTK_POS_RIGHT]);
      gtk_css_boxes_init_border_box (&boxes, style,
                                     rect.x + rect.width - UNDERSHOOT_SIZE, rect.y, UNDERSHOOT_SIZE, rect.height);
      gtk_css_style_snapshot_background (&boxes, snapshot);
      gtk_css_style_snapshot_border (&boxes, snapshot);
    }
  if (gtk_adjustment_get_value (adj) > gtk_adjustment_get_lower (adj))
    {
      style = gtk_css_node_get_style (priv->undershoot_node[GTK_POS_LEFT]);
      gtk_css_boxes_init_border_box (&boxes, style,
                                     rect.x, rect.y, UNDERSHOOT_SIZE, rect.height);
      gtk_css_style_snapshot_background (&boxes, snapshot);
      gtk_css_style_snapshot_border (&boxes, snapshot);
    }

  adj = gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (priv->vscrollbar));
  if (gtk_adjustment_get_value (adj) < gtk_adjustment_get_upper (adj) - gtk_adjustment_get_page_size (adj))
    {
      style = gtk_css_node_get_style (priv->undershoot_node[GTK_POS_BOTTOM]);
      gtk_css_boxes_init_border_box (&boxes, style,
                                     rect.x, rect.y + rect.height - UNDERSHOOT_SIZE, rect.width, UNDERSHOOT_SIZE);
      gtk_css_style_snapshot_background (&boxes, snapshot);
      gtk_css_style_snapshot_border (&boxes, snapshot);
    }
  if (gtk_adjustment_get_value (adj) > gtk_adjustment_get_lower (adj))
    {
      style = gtk_css_node_get_style (priv->undershoot_node[GTK_POS_TOP]);
      gtk_css_boxes_init_border_box (&boxes, style,
                                     rect.x, rect.y, rect.width, UNDERSHOOT_SIZE);
      gtk_css_style_snapshot_background (&boxes, snapshot);
      gtk_css_style_snapshot_border (&boxes, snapshot);
    }
}

static void
gtk_scrolled_window_init (GtkScrolledWindow *scrolled_window)
{
  GtkWidget *widget = GTK_WIDGET (scrolled_window);
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);
  GtkEventController *controller;
  GtkCssNode *widget_node;
  GQuark classes[4] = {
    g_quark_from_static_string ("left"),
    g_quark_from_static_string ("right"),
    g_quark_from_static_string ("top"),
    g_quark_from_static_string ("bottom"),
  };
  int i;

  gtk_widget_set_focusable (widget, TRUE);

  /* Instantiated by gtk_scrolled_window_set_[hv]adjustment
   * which are both construct properties
   */
  priv->hscrollbar = NULL;
  priv->vscrollbar = NULL;
  priv->hscrollbar_policy = GTK_POLICY_AUTOMATIC;
  priv->vscrollbar_policy = GTK_POLICY_AUTOMATIC;
  priv->hscrollbar_visible = FALSE;
  priv->vscrollbar_visible = FALSE;
  priv->focus_out = FALSE;
  priv->auto_added_viewport = FALSE;
  priv->window_placement = GTK_CORNER_TOP_LEFT;
  priv->min_content_width = -1;
  priv->min_content_height = -1;
  priv->max_content_width = -1;
  priv->max_content_height = -1;

  priv->overlay_scrolling = TRUE;

  priv->drag_gesture = gtk_gesture_drag_new ();
  gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (priv->drag_gesture), TRUE);
  g_signal_connect_swapped (priv->drag_gesture, "drag-begin",
                            G_CALLBACK (scrolled_window_drag_begin_cb),
                            scrolled_window);
  g_signal_connect_swapped (priv->drag_gesture, "drag-update",
                            G_CALLBACK (scrolled_window_drag_update_cb),
                            scrolled_window);
  gtk_widget_add_controller (widget, GTK_EVENT_CONTROLLER (priv->drag_gesture));

  priv->pan_gesture = gtk_gesture_pan_new (GTK_ORIENTATION_VERTICAL);
  gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (priv->pan_gesture), TRUE);
  gtk_widget_add_controller (widget, GTK_EVENT_CONTROLLER (priv->pan_gesture));
  gtk_gesture_group (priv->pan_gesture, priv->drag_gesture);

  priv->swipe_gesture = gtk_gesture_swipe_new ();
  gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (priv->swipe_gesture), TRUE);
  g_signal_connect_swapped (priv->swipe_gesture, "swipe",
                            G_CALLBACK (scrolled_window_swipe_cb),
                            scrolled_window);
  gtk_widget_add_controller (widget, GTK_EVENT_CONTROLLER (priv->swipe_gesture));
  gtk_gesture_group (priv->swipe_gesture, priv->drag_gesture);

  priv->long_press_gesture = gtk_gesture_long_press_new ();
  gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (priv->long_press_gesture), TRUE);
  g_signal_connect_swapped (priv->long_press_gesture, "pressed",
                            G_CALLBACK (scrolled_window_long_press_cb),
                            scrolled_window);
  g_signal_connect_swapped (priv->long_press_gesture, "cancelled",
                            G_CALLBACK (scrolled_window_long_press_cancelled_cb),
                            scrolled_window);
  gtk_widget_add_controller (widget, GTK_EVENT_CONTROLLER (priv->long_press_gesture));
  gtk_gesture_group (priv->long_press_gesture, priv->drag_gesture);

  gtk_scrolled_window_set_kinetic_scrolling (scrolled_window, TRUE);

  controller = gtk_event_controller_motion_new ();
  gtk_event_controller_set_propagation_phase (controller, GTK_PHASE_CAPTURE);
  g_signal_connect (controller, "motion",
                    G_CALLBACK (captured_motion), scrolled_window);
  gtk_widget_add_controller (widget, controller);

  widget_node = gtk_widget_get_css_node (widget);
  for (i = 0; i < 4; i++)
    {
      priv->overshoot_node[i] = gtk_css_node_new ();
      gtk_css_node_set_name (priv->overshoot_node[i], g_quark_from_static_string ("overshoot"));
      gtk_css_node_add_class (priv->overshoot_node[i], classes[i]);
      gtk_css_node_set_parent (priv->overshoot_node[i], widget_node);
      gtk_css_node_set_state (priv->overshoot_node[i], gtk_css_node_get_state (widget_node));
      g_object_unref (priv->overshoot_node[i]);

      priv->undershoot_node[i] = gtk_css_node_new ();
      gtk_css_node_set_name (priv->undershoot_node[i], g_quark_from_static_string ("undershoot"));
      gtk_css_node_add_class (priv->undershoot_node[i], classes[i]);
      gtk_css_node_set_parent (priv->undershoot_node[i], widget_node);
      gtk_css_node_set_state (priv->undershoot_node[i], gtk_css_node_get_state (widget_node));
      g_object_unref (priv->undershoot_node[i]);
    }

  gtk_scrolled_window_update_use_indicators (scrolled_window);

  priv->junction_node = gtk_css_node_new ();
  gtk_css_node_set_name (priv->junction_node, g_quark_from_static_string ("junction"));
  gtk_css_node_set_parent (priv->junction_node, widget_node);
  gtk_css_node_set_state (priv->junction_node, gtk_css_node_get_state (widget_node));
  g_object_unref (priv->junction_node);

  controller = gtk_event_controller_scroll_new (GTK_EVENT_CONTROLLER_SCROLL_BOTH_AXES |
                                                GTK_EVENT_CONTROLLER_SCROLL_KINETIC);
  g_signal_connect (controller, "scroll-begin",
                    G_CALLBACK (scroll_controller_scroll_begin), scrolled_window);
  g_signal_connect (controller, "scroll",
                    G_CALLBACK (scroll_controller_scroll), scrolled_window);
  g_signal_connect (controller, "scroll-end",
                    G_CALLBACK (scroll_controller_scroll_end), scrolled_window);
  gtk_widget_add_controller (widget, controller);

  controller = gtk_event_controller_scroll_new (GTK_EVENT_CONTROLLER_SCROLL_BOTH_AXES |
                                                GTK_EVENT_CONTROLLER_SCROLL_KINETIC);
  gtk_event_controller_set_propagation_phase (controller, GTK_PHASE_CAPTURE);
  g_signal_connect (controller, "scroll-begin",
                    G_CALLBACK (stop_kinetic_scrolling_cb), scrolled_window);
  g_signal_connect (controller, "scroll",
                    G_CALLBACK (captured_scroll_cb), scrolled_window);
  g_signal_connect (controller, "decelerate",
                    G_CALLBACK (scroll_controller_decelerate), scrolled_window);
  gtk_widget_add_controller (widget, controller);

  controller = gtk_event_controller_motion_new ();
  g_signal_connect (controller, "leave",
                    G_CALLBACK (motion_controller_leave), scrolled_window);
  gtk_widget_add_controller (widget, controller);
}

/**
 * gtk_scrolled_window_new:
 *
 * Creates a new scrolled window.
 *
 * Returns: a new scrolled window
 */
GtkWidget *
gtk_scrolled_window_new (void)
{
  return g_object_new (GTK_TYPE_SCROLLED_WINDOW, NULL);
}

/**
 * gtk_scrolled_window_set_hadjustment: (attributes org.gtk.Method.set_property=hadjustment)
 * @scrolled_window: a `GtkScrolledWindow`
 * @hadjustment: (nullable): the `GtkAdjustment` to use, or %NULL to create a new one
 *
 * Sets the `GtkAdjustment` for the horizontal scrollbar.
 */
void
gtk_scrolled_window_set_hadjustment (GtkScrolledWindow *scrolled_window,
                                     GtkAdjustment     *hadjustment)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window));

  if (hadjustment)
    g_return_if_fail (GTK_IS_ADJUSTMENT (hadjustment));
  else
    hadjustment = (GtkAdjustment*) g_object_new (GTK_TYPE_ADJUSTMENT, NULL);

  if (!priv->hscrollbar)
    {
      priv->hscrollbar = gtk_scrollbar_new (GTK_ORIENTATION_HORIZONTAL, hadjustment);

      gtk_widget_insert_before (priv->hscrollbar, GTK_WIDGET (scrolled_window), priv->vscrollbar);
      update_scrollbar_positions (scrolled_window);
    }
  else
    {
      GtkAdjustment *old_adjustment;

      old_adjustment = gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (priv->hscrollbar));
      if (old_adjustment == hadjustment)
        return;

      g_signal_handlers_disconnect_by_func (old_adjustment,
                                            gtk_scrolled_window_adjustment_changed,
                                            scrolled_window);
      g_signal_handlers_disconnect_by_func (old_adjustment,
                                            gtk_scrolled_window_adjustment_value_changed,
                                            scrolled_window);

      gtk_adjustment_enable_animation (old_adjustment, NULL, 0);
      gtk_scrollbar_set_adjustment (GTK_SCROLLBAR (priv->hscrollbar), hadjustment);
    }

  hadjustment = gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (priv->hscrollbar));

  g_signal_connect (hadjustment,
                    "changed",
                    G_CALLBACK (gtk_scrolled_window_adjustment_changed),
                    scrolled_window);
  g_signal_connect (hadjustment,
                    "value-changed",
                    G_CALLBACK (gtk_scrolled_window_adjustment_value_changed),
                    scrolled_window);

  gtk_scrolled_window_adjustment_changed (hadjustment, scrolled_window);
  gtk_scrolled_window_adjustment_value_changed (hadjustment, scrolled_window);

  if (priv->child)
    gtk_scrollable_set_hadjustment (GTK_SCROLLABLE (priv->child), hadjustment);

  if (gtk_widget_should_animate (GTK_WIDGET (scrolled_window)))
    gtk_adjustment_enable_animation (hadjustment, gtk_widget_get_frame_clock (GTK_WIDGET (scrolled_window)), ANIMATION_DURATION);

  g_object_notify_by_pspec (G_OBJECT (scrolled_window), properties[PROP_HADJUSTMENT]);
}

/**
 * gtk_scrolled_window_set_vadjustment: (attributes org.gtk.Method.set_property=vadjustment)
 * @scrolled_window: a `GtkScrolledWindow`
 * @vadjustment: (nullable): the `GtkAdjustment` to use, or %NULL to create a new one
 *
 * Sets the `GtkAdjustment` for the vertical scrollbar.
 */
void
gtk_scrolled_window_set_vadjustment (GtkScrolledWindow *scrolled_window,
                                     GtkAdjustment     *vadjustment)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window));

  if (vadjustment)
    g_return_if_fail (GTK_IS_ADJUSTMENT (vadjustment));
  else
    vadjustment = (GtkAdjustment*) g_object_new (GTK_TYPE_ADJUSTMENT, NULL);

  if (!priv->vscrollbar)
    {
      priv->vscrollbar = gtk_scrollbar_new (GTK_ORIENTATION_VERTICAL, vadjustment);

      gtk_widget_insert_after (priv->vscrollbar, GTK_WIDGET (scrolled_window), priv->hscrollbar);
      update_scrollbar_positions (scrolled_window);
    }
  else
    {
      GtkAdjustment *old_adjustment;

      old_adjustment = gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (priv->vscrollbar));
      if (old_adjustment == vadjustment)
        return;

      g_signal_handlers_disconnect_by_func (old_adjustment,
                                            gtk_scrolled_window_adjustment_changed,
                                            scrolled_window);
      g_signal_handlers_disconnect_by_func (old_adjustment,
                                            gtk_scrolled_window_adjustment_value_changed,
                                            scrolled_window);

      gtk_adjustment_enable_animation (old_adjustment, NULL, 0);
      gtk_scrollbar_set_adjustment (GTK_SCROLLBAR (priv->vscrollbar), vadjustment);
    }

  vadjustment = gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (priv->vscrollbar));

  g_signal_connect (vadjustment,
                    "changed",
                    G_CALLBACK (gtk_scrolled_window_adjustment_changed),
                    scrolled_window);
  g_signal_connect (vadjustment,
                    "value-changed",
                    G_CALLBACK (gtk_scrolled_window_adjustment_value_changed),
                    scrolled_window);

  gtk_scrolled_window_adjustment_changed (vadjustment, scrolled_window);
  gtk_scrolled_window_adjustment_value_changed (vadjustment, scrolled_window);

  if (priv->child)
    gtk_scrollable_set_vadjustment (GTK_SCROLLABLE (priv->child), vadjustment);

  if (gtk_widget_should_animate (GTK_WIDGET (scrolled_window)))
    gtk_adjustment_enable_animation (vadjustment, gtk_widget_get_frame_clock (GTK_WIDGET (scrolled_window)), ANIMATION_DURATION);

  g_object_notify_by_pspec (G_OBJECT (scrolled_window), properties[PROP_VADJUSTMENT]);
}

/**
 * gtk_scrolled_window_get_hadjustment: (attributes org.gtk.Method.get_property=hadjustment)
 * @scrolled_window: a `GtkScrolledWindow`
 *
 * Returns the horizontal scrollbar’s adjustment.
 *
 * This is the adjustment used to connect the horizontal scrollbar
 * to the child widget’s horizontal scroll functionality.
 *
 * Returns: (transfer none): the horizontal `GtkAdjustment`
 */
GtkAdjustment*
gtk_scrolled_window_get_hadjustment (GtkScrolledWindow *scrolled_window)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  g_return_val_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window), NULL);

  return gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (priv->hscrollbar));
}

/**
 * gtk_scrolled_window_get_vadjustment: (attributes org.gtk.Method.get_property=vadjustment)
 * @scrolled_window: a `GtkScrolledWindow`
 *
 * Returns the vertical scrollbar’s adjustment.
 *
 * This is the adjustment used to connect the vertical
 * scrollbar to the child widget’s vertical scroll functionality.
 *
 * Returns: (transfer none): the vertical `GtkAdjustment`
 */
GtkAdjustment*
gtk_scrolled_window_get_vadjustment (GtkScrolledWindow *scrolled_window)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  g_return_val_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window), NULL);

  return gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (priv->vscrollbar));
}

/**
 * gtk_scrolled_window_get_hscrollbar:
 * @scrolled_window: a `GtkScrolledWindow`
 *
 * Returns the horizontal scrollbar of @scrolled_window.
 *
 * Returns: (transfer none): the horizontal scrollbar of the scrolled window.
 */
GtkWidget*
gtk_scrolled_window_get_hscrollbar (GtkScrolledWindow *scrolled_window)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  g_return_val_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window), NULL);

  return priv->hscrollbar;
}

/**
 * gtk_scrolled_window_get_vscrollbar:
 * @scrolled_window: a `GtkScrolledWindow`
 *
 * Returns the vertical scrollbar of @scrolled_window.
 *
 * Returns: (transfer none): the vertical scrollbar of the scrolled window.
 */
GtkWidget*
gtk_scrolled_window_get_vscrollbar (GtkScrolledWindow *scrolled_window)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  g_return_val_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window), NULL);

  return priv->vscrollbar;
}

/**
 * gtk_scrolled_window_set_policy:
 * @scrolled_window: a `GtkScrolledWindow`
 * @hscrollbar_policy: policy for horizontal bar
 * @vscrollbar_policy: policy for vertical bar
 *
 * Sets the scrollbar policy for the horizontal and vertical scrollbars.
 *
 * The policy determines when the scrollbar should appear; it is a value
 * from the [enum@Gtk.PolicyType] enumeration. If %GTK_POLICY_ALWAYS, the
 * scrollbar is always present; if %GTK_POLICY_NEVER, the scrollbar is
 * never present; if %GTK_POLICY_AUTOMATIC, the scrollbar is present only
 * if needed (that is, if the slider part of the bar would be smaller
 * than the trough — the display is larger than the page size).
 */
void
gtk_scrolled_window_set_policy (GtkScrolledWindow *scrolled_window,
                                GtkPolicyType      hscrollbar_policy,
                                GtkPolicyType      vscrollbar_policy)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);
  GObject *object = G_OBJECT (scrolled_window);

  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window));

  if ((priv->hscrollbar_policy != hscrollbar_policy) ||
      (priv->vscrollbar_policy != vscrollbar_policy))
    {
      priv->hscrollbar_policy = hscrollbar_policy;
      priv->vscrollbar_policy = vscrollbar_policy;

      gtk_widget_queue_resize (GTK_WIDGET (scrolled_window));

      g_object_notify_by_pspec (object, properties[PROP_HSCROLLBAR_POLICY]);
      g_object_notify_by_pspec (object, properties[PROP_VSCROLLBAR_POLICY]);
    }
}

/**
 * gtk_scrolled_window_get_policy:
 * @scrolled_window: a `GtkScrolledWindow`
 * @hscrollbar_policy: (out) (optional): location to store the policy
 *   for the horizontal scrollbar
 * @vscrollbar_policy: (out) (optional): location to store the policy
 *   for the vertical scrollbar
 *
 * Retrieves the current policy values for the horizontal and vertical
 * scrollbars.
 *
 * See [method@Gtk.ScrolledWindow.set_policy].
 */
void
gtk_scrolled_window_get_policy (GtkScrolledWindow *scrolled_window,
                                GtkPolicyType     *hscrollbar_policy,
                                GtkPolicyType     *vscrollbar_policy)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window));

  if (hscrollbar_policy)
    *hscrollbar_policy = priv->hscrollbar_policy;
  if (vscrollbar_policy)
    *vscrollbar_policy = priv->vscrollbar_policy;
}

/**
 * gtk_scrolled_window_set_placement: (attributes org.gtk.Method.set_property=window-placement)
 * @scrolled_window: a `GtkScrolledWindow`
 * @window_placement: position of the child window
 *
 * Sets the placement of the contents with respect to the scrollbars
 * for the scrolled window.
 *
 * The default is %GTK_CORNER_TOP_LEFT, meaning the child is
 * in the top left, with the scrollbars underneath and to the right.
 * Other values in [enum@Gtk.CornerType] are %GTK_CORNER_TOP_RIGHT,
 * %GTK_CORNER_BOTTOM_LEFT, and %GTK_CORNER_BOTTOM_RIGHT.
 *
 * See also [method@Gtk.ScrolledWindow.get_placement] and
 * [method@Gtk.ScrolledWindow.unset_placement].
 */
void
gtk_scrolled_window_set_placement (GtkScrolledWindow *scrolled_window,
                                   GtkCornerType      window_placement)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window));

  if (priv->window_placement != window_placement)
    {
      priv->window_placement = window_placement;
      update_scrollbar_positions (scrolled_window);

      gtk_widget_queue_resize (GTK_WIDGET (scrolled_window));

      g_object_notify_by_pspec (G_OBJECT (scrolled_window), properties[PROP_WINDOW_PLACEMENT]);
    }
}

/**
 * gtk_scrolled_window_get_placement: (attributes org.gtk.Method.get_property=window-placement)
 * @scrolled_window: a `GtkScrolledWindow`
 *
 * Gets the placement of the contents with respect to the scrollbars.
 *
 * Returns: the current placement value.
 */
GtkCornerType
gtk_scrolled_window_get_placement (GtkScrolledWindow *scrolled_window)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  g_return_val_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window), GTK_CORNER_TOP_LEFT);

  return priv->window_placement;
}

/**
 * gtk_scrolled_window_unset_placement:
 * @scrolled_window: a `GtkScrolledWindow`
 *
 * Unsets the placement of the contents with respect to the scrollbars.
 *
 * If no window placement is set for a scrolled window,
 * it defaults to %GTK_CORNER_TOP_LEFT.
 */
void
gtk_scrolled_window_unset_placement (GtkScrolledWindow *scrolled_window)
{
  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window));

  gtk_scrolled_window_set_placement (scrolled_window, GTK_CORNER_TOP_LEFT);
}

/**
 * gtk_scrolled_window_set_has_frame: (attributes org.gtk.Method.set_property=has-frame)
 * @scrolled_window: a `GtkScrolledWindow`
 * @has_frame: whether to draw a frame around scrolled window contents
 *
 * Changes the frame drawn around the contents of @scrolled_window.
 */
void
gtk_scrolled_window_set_has_frame (GtkScrolledWindow *scrolled_window,
                                   gboolean           has_frame)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window));

  if (priv->has_frame == !!has_frame)
    return;

  priv->has_frame = has_frame;

  if (has_frame)
    gtk_widget_add_css_class (GTK_WIDGET (scrolled_window), "frame");
  else
    gtk_widget_remove_css_class (GTK_WIDGET (scrolled_window), "frame");

  g_object_notify_by_pspec (G_OBJECT (scrolled_window), properties[PROP_HAS_FRAME]);
}

/**
 * gtk_scrolled_window_get_has_frame: (attributes org.gtk.Method.get_property=has-frame)
 * @scrolled_window: a `GtkScrolledWindow`
 *
 * Gets whether the scrolled window draws a frame.
 *
 * Returns: %TRUE if the @scrolled_window has a frame
 */
gboolean
gtk_scrolled_window_get_has_frame (GtkScrolledWindow *scrolled_window)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  g_return_val_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window), FALSE);

  return priv->has_frame;
}

/**
 * gtk_scrolled_window_set_kinetic_scrolling: (attributes org.gtk.Method.set_property=kinetic-scrolling)
 * @scrolled_window: a `GtkScrolledWindow`
 * @kinetic_scrolling: %TRUE to enable kinetic scrolling
 *
 * Turns kinetic scrolling on or off.
 *
 * Kinetic scrolling only applies to devices with source
 * %GDK_SOURCE_TOUCHSCREEN.
 **/
void
gtk_scrolled_window_set_kinetic_scrolling (GtkScrolledWindow *scrolled_window,
                                           gboolean           kinetic_scrolling)
{
  GtkPropagationPhase phase = GTK_PHASE_NONE;
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window));

  if (priv->kinetic_scrolling == kinetic_scrolling)
    return;

  priv->kinetic_scrolling = kinetic_scrolling;
  gtk_scrolled_window_check_attach_pan_gesture (scrolled_window);

  if (priv->kinetic_scrolling)
    phase = GTK_PHASE_CAPTURE;
  else
    gtk_scrolled_window_cancel_deceleration (scrolled_window);

  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (priv->drag_gesture), phase);
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (priv->swipe_gesture), phase);
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (priv->long_press_gesture), phase);
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (priv->pan_gesture), phase);

  g_object_notify_by_pspec (G_OBJECT (scrolled_window), properties[PROP_KINETIC_SCROLLING]);
}

/**
 * gtk_scrolled_window_get_kinetic_scrolling: (attributes org.gtk.Method.get_property=kinetic-scrolling)
 * @scrolled_window: a `GtkScrolledWindow`
 *
 * Returns the specified kinetic scrolling behavior.
 *
 * Returns: the scrolling behavior flags.
 */
gboolean
gtk_scrolled_window_get_kinetic_scrolling (GtkScrolledWindow *scrolled_window)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  g_return_val_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window), FALSE);

  return priv->kinetic_scrolling;
}

static void
gtk_scrolled_window_dispose (GObject *object)
{
  GtkScrolledWindow *self = GTK_SCROLLED_WINDOW (object);
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (self);

  g_clear_pointer (&priv->child, gtk_widget_unparent);

  remove_indicator (self, &priv->hindicator);
  remove_indicator (self, &priv->vindicator);

  if (priv->hscrollbar)
    {
      GtkAdjustment *hadjustment = gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (priv->hscrollbar));

      g_signal_handlers_disconnect_by_data (hadjustment, self);
      g_signal_handlers_disconnect_by_data (hadjustment, &priv->hindicator);

      gtk_widget_unparent (priv->hscrollbar);
      priv->hscrollbar = NULL;
    }

  if (priv->vscrollbar)
    {
      GtkAdjustment *vadjustment = gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (priv->vscrollbar));

      g_signal_handlers_disconnect_by_data (vadjustment, self);
      g_signal_handlers_disconnect_by_data (vadjustment, &priv->vindicator);

      gtk_widget_unparent (priv->vscrollbar);
      priv->vscrollbar = NULL;
    }

  if (priv->deceleration_id)
    {
      gtk_widget_remove_tick_callback (GTK_WIDGET (self), priv->deceleration_id);
      priv->deceleration_id = 0;
    }

  g_clear_pointer (&priv->hscrolling, gtk_kinetic_scrolling_free);
  g_clear_pointer (&priv->vscrolling, gtk_kinetic_scrolling_free);
  g_clear_handle_id (&priv->scroll_events_overshoot_id, g_source_remove);

  G_OBJECT_CLASS (gtk_scrolled_window_parent_class)->dispose (object);
}

static void
gtk_scrolled_window_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GtkScrolledWindow *scrolled_window = GTK_SCROLLED_WINDOW (object);
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  switch (prop_id)
    {
    case PROP_HADJUSTMENT:
      gtk_scrolled_window_set_hadjustment (scrolled_window,
                                           g_value_get_object (value));
      break;
    case PROP_VADJUSTMENT:
      gtk_scrolled_window_set_vadjustment (scrolled_window,
                                           g_value_get_object (value));
      break;
    case PROP_HSCROLLBAR_POLICY:
      gtk_scrolled_window_set_policy (scrolled_window,
                                      g_value_get_enum (value),
                                      priv->vscrollbar_policy);
      break;
    case PROP_VSCROLLBAR_POLICY:
      gtk_scrolled_window_set_policy (scrolled_window,
                                      priv->hscrollbar_policy,
                                      g_value_get_enum (value));
      break;
    case PROP_WINDOW_PLACEMENT:
      gtk_scrolled_window_set_placement (scrolled_window,
                                         g_value_get_enum (value));
      break;
    case PROP_HAS_FRAME:
      gtk_scrolled_window_set_has_frame (scrolled_window,
                                         g_value_get_boolean (value));
      break;
    case PROP_MIN_CONTENT_WIDTH:
      gtk_scrolled_window_set_min_content_width (scrolled_window,
                                                 g_value_get_int (value));
      break;
    case PROP_MIN_CONTENT_HEIGHT:
      gtk_scrolled_window_set_min_content_height (scrolled_window,
                                                  g_value_get_int (value));
      break;
    case PROP_KINETIC_SCROLLING:
      gtk_scrolled_window_set_kinetic_scrolling (scrolled_window,
                                                 g_value_get_boolean (value));
      break;
    case PROP_OVERLAY_SCROLLING:
      gtk_scrolled_window_set_overlay_scrolling (scrolled_window,
                                                 g_value_get_boolean (value));
      break;
    case PROP_MAX_CONTENT_WIDTH:
      gtk_scrolled_window_set_max_content_width (scrolled_window,
                                                 g_value_get_int (value));
      break;
    case PROP_MAX_CONTENT_HEIGHT:
      gtk_scrolled_window_set_max_content_height (scrolled_window,
                                                  g_value_get_int (value));
      break;
    case PROP_PROPAGATE_NATURAL_WIDTH:
      gtk_scrolled_window_set_propagate_natural_width (scrolled_window,
                                                       g_value_get_boolean (value));
      break;
    case PROP_PROPAGATE_NATURAL_HEIGHT:
      gtk_scrolled_window_set_propagate_natural_height (scrolled_window,
                                                       g_value_get_boolean (value));
      break;
    case PROP_CHILD:
      gtk_scrolled_window_set_child (scrolled_window, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_scrolled_window_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GtkScrolledWindow *scrolled_window = GTK_SCROLLED_WINDOW (object);
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  switch (prop_id)
    {
    case PROP_HADJUSTMENT:
      g_value_set_object (value,
                          G_OBJECT (gtk_scrolled_window_get_hadjustment (scrolled_window)));
      break;
    case PROP_VADJUSTMENT:
      g_value_set_object (value,
                          G_OBJECT (gtk_scrolled_window_get_vadjustment (scrolled_window)));
      break;
    case PROP_WINDOW_PLACEMENT:
      g_value_set_enum (value, priv->window_placement);
      break;
    case PROP_HAS_FRAME:
      g_value_set_boolean (value, priv->has_frame);
      break;
    case PROP_HSCROLLBAR_POLICY:
      g_value_set_enum (value, priv->hscrollbar_policy);
      break;
    case PROP_VSCROLLBAR_POLICY:
      g_value_set_enum (value, priv->vscrollbar_policy);
      break;
    case PROP_MIN_CONTENT_WIDTH:
      g_value_set_int (value, priv->min_content_width);
      break;
    case PROP_MIN_CONTENT_HEIGHT:
      g_value_set_int (value, priv->min_content_height);
      break;
    case PROP_KINETIC_SCROLLING:
      g_value_set_boolean (value, priv->kinetic_scrolling);
      break;
    case PROP_OVERLAY_SCROLLING:
      g_value_set_boolean (value, priv->overlay_scrolling);
      break;
    case PROP_MAX_CONTENT_WIDTH:
      g_value_set_int (value, priv->max_content_width);
      break;
    case PROP_MAX_CONTENT_HEIGHT:
      g_value_set_int (value, priv->max_content_height);
      break;
    case PROP_PROPAGATE_NATURAL_WIDTH:
      g_value_set_boolean (value, priv->propagate_natural_width);
      break;
    case PROP_PROPAGATE_NATURAL_HEIGHT:
      g_value_set_boolean (value, priv->propagate_natural_height);
      break;
    case PROP_CHILD:
      g_value_set_object (value, gtk_scrolled_window_get_child (scrolled_window));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_scrolled_window_inner_allocation (GtkScrolledWindow *scrolled_window,
                                      GtkAllocation     *rect)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);
  GtkBorder border = { 0 };

  gtk_scrolled_window_relative_allocation (scrolled_window, rect);
  rect->x = 0;
  rect->y = 0;
  if (priv->child && gtk_scrollable_get_border (GTK_SCROLLABLE (priv->child), &border))
    {
      rect->x += border.left;
      rect->y += border.top;
      rect->width -= border.left + border.right;
      rect->height -= border.top + border.bottom;
    }
}

static void
gtk_scrolled_window_snapshot (GtkWidget   *widget,
                              GtkSnapshot *snapshot)
{
  GtkScrolledWindow *scrolled_window = GTK_SCROLLED_WINDOW (widget);
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  if (priv->hscrollbar_visible &&
      priv->vscrollbar_visible &&
      !priv->use_indicators)
    gtk_scrolled_window_snapshot_scrollbars_junction (scrolled_window, snapshot);

  GTK_WIDGET_CLASS (gtk_scrolled_window_parent_class)->snapshot (widget, snapshot);

  gtk_scrolled_window_snapshot_undershoot (scrolled_window, snapshot);
  gtk_scrolled_window_snapshot_overshoot (scrolled_window, snapshot);
}

static gboolean
gtk_scrolled_window_scroll_child (GtkScrolledWindow *scrolled_window,
                                  GtkScrollType      scroll,
                                  gboolean           horizontal)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);
  GtkAdjustment *adjustment = NULL;

  switch (scroll)
    {
    case GTK_SCROLL_STEP_UP:
      scroll = GTK_SCROLL_STEP_BACKWARD;
      horizontal = FALSE;
      break;
    case GTK_SCROLL_STEP_DOWN:
      scroll = GTK_SCROLL_STEP_FORWARD;
      horizontal = FALSE;
      break;
    case GTK_SCROLL_STEP_LEFT:
      scroll = GTK_SCROLL_STEP_BACKWARD;
      horizontal = TRUE;
      break;
    case GTK_SCROLL_STEP_RIGHT:
      scroll = GTK_SCROLL_STEP_FORWARD;
      horizontal = TRUE;
      break;
    case GTK_SCROLL_PAGE_UP:
      scroll = GTK_SCROLL_PAGE_BACKWARD;
      horizontal = FALSE;
      break;
    case GTK_SCROLL_PAGE_DOWN:
      scroll = GTK_SCROLL_PAGE_FORWARD;
      horizontal = FALSE;
      break;
    case GTK_SCROLL_PAGE_LEFT:
      scroll = GTK_SCROLL_STEP_BACKWARD;
      horizontal = TRUE;
      break;
    case GTK_SCROLL_PAGE_RIGHT:
      scroll = GTK_SCROLL_STEP_FORWARD;
      horizontal = TRUE;
      break;
    case GTK_SCROLL_STEP_BACKWARD:
    case GTK_SCROLL_STEP_FORWARD:
    case GTK_SCROLL_PAGE_BACKWARD:
    case GTK_SCROLL_PAGE_FORWARD:
    case GTK_SCROLL_START:
    case GTK_SCROLL_END:
      break;
    case GTK_SCROLL_NONE:
    case GTK_SCROLL_JUMP:
    default:
      g_warning ("Invalid scroll type %u for GtkScrolledWindow::scroll-child", scroll);
      return FALSE;
    }

  if (horizontal)
    {
      if (may_hscroll (scrolled_window))
        adjustment = gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (priv->hscrollbar));
      else
        return FALSE;
    }
  else
    {
      if (may_vscroll (scrolled_window))
        adjustment = gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (priv->vscrollbar));
      else
        return FALSE;
    }

  if (adjustment)
    {
      double value = gtk_adjustment_get_value (adjustment);

      switch (scroll)
        {
        case GTK_SCROLL_STEP_FORWARD:
          value += gtk_adjustment_get_step_increment (adjustment);
          break;
        case GTK_SCROLL_STEP_BACKWARD:
          value -= gtk_adjustment_get_step_increment (adjustment);
          break;
        case GTK_SCROLL_PAGE_FORWARD:
          value += gtk_adjustment_get_page_increment (adjustment);
          break;
        case GTK_SCROLL_PAGE_BACKWARD:
          value -= gtk_adjustment_get_page_increment (adjustment);
          break;
        case GTK_SCROLL_START:
          value = gtk_adjustment_get_lower (adjustment);
          break;
        case GTK_SCROLL_END:
          value = gtk_adjustment_get_upper (adjustment);
          break;
        case GTK_SCROLL_STEP_UP:
        case GTK_SCROLL_STEP_DOWN:
        case GTK_SCROLL_STEP_LEFT:
        case GTK_SCROLL_STEP_RIGHT:
        case GTK_SCROLL_PAGE_UP:
        case GTK_SCROLL_PAGE_DOWN:
        case GTK_SCROLL_PAGE_LEFT:
        case GTK_SCROLL_PAGE_RIGHT:
        case GTK_SCROLL_NONE:
        case GTK_SCROLL_JUMP:
        default:
          g_assert_not_reached ();
          break;
        }

      gtk_adjustment_animate_to_value (adjustment, value);

      return TRUE;
    }

  return FALSE;
}

static void
gtk_scrolled_window_move_focus_out (GtkScrolledWindow *scrolled_window,
                                    GtkDirectionType   direction_type)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);
  GtkWidget *toplevel;

  /* Focus out of the scrolled window entirely. We do this by setting
   * a flag, then propagating the focus motion to the notebook.
   */
  toplevel = GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (scrolled_window)));
  if (!GTK_IS_ROOT (toplevel))
    return;

  g_object_ref (scrolled_window);

  priv->focus_out = TRUE;
  g_signal_emit_by_name (toplevel, "move-focus", direction_type);
  priv->focus_out = FALSE;

  g_object_unref (scrolled_window);
}

static void
gtk_scrolled_window_relative_allocation (GtkScrolledWindow *scrolled_window,
                                         GtkAllocation     *allocation)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);
  int sb_width;
  int sb_height;
  int width, height;

  g_return_if_fail (scrolled_window != NULL);
  g_return_if_fail (allocation != NULL);

  /* Get possible scrollbar dimensions */
  gtk_widget_measure (priv->vscrollbar, GTK_ORIENTATION_HORIZONTAL, -1,
                      &sb_width, NULL, NULL, NULL);
  gtk_widget_measure (priv->hscrollbar, GTK_ORIENTATION_VERTICAL, -1,
                      &sb_height, NULL, NULL, NULL);

  width = gtk_widget_get_width (GTK_WIDGET (scrolled_window));
  height = gtk_widget_get_height (GTK_WIDGET (scrolled_window));

  allocation->x = 0;
  allocation->y = 0;
  allocation->width = width;
  allocation->height = height;

  /* Subtract some things from our available allocation size */
  if (priv->vscrollbar_visible && !priv->use_indicators)
    {
      gboolean is_rtl;

      is_rtl = _gtk_widget_get_direction (GTK_WIDGET (scrolled_window)) == GTK_TEXT_DIR_RTL;

      if ((!is_rtl &&
           (priv->window_placement == GTK_CORNER_TOP_RIGHT ||
            priv->window_placement == GTK_CORNER_BOTTOM_RIGHT)) ||
          (is_rtl &&
           (priv->window_placement == GTK_CORNER_TOP_LEFT ||
            priv->window_placement == GTK_CORNER_BOTTOM_LEFT)))
        allocation->x += sb_width;

      allocation->width = MAX (1, width - sb_width);
    }

  if (priv->hscrollbar_visible && !priv->use_indicators)
    {

      if (priv->window_placement == GTK_CORNER_BOTTOM_LEFT ||
          priv->window_placement == GTK_CORNER_BOTTOM_RIGHT)
        allocation->y += (sb_height);

      allocation->height = MAX (1, height - sb_height);
    }
}

static gboolean
_gtk_scrolled_window_get_overshoot (GtkScrolledWindow *scrolled_window,
                                    int               *overshoot_x,
                                    int               *overshoot_y)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);
  GtkAdjustment *vadjustment, *hadjustment;
  double lower, upper, x, y;

  /* Vertical overshoot */
  vadjustment = gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (priv->vscrollbar));
  lower = gtk_adjustment_get_lower (vadjustment);
  upper = gtk_adjustment_get_upper (vadjustment) -
    gtk_adjustment_get_page_size (vadjustment);

  if (priv->unclamped_vadj_value < lower)
    y = priv->unclamped_vadj_value - lower;
  else if (priv->unclamped_vadj_value > upper)
    y = priv->unclamped_vadj_value - upper;
  else
    y = 0;

  /* Horizontal overshoot */
  hadjustment = gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (priv->hscrollbar));
  lower = gtk_adjustment_get_lower (hadjustment);
  upper = gtk_adjustment_get_upper (hadjustment) -
    gtk_adjustment_get_page_size (hadjustment);

  if (priv->unclamped_hadj_value < lower)
    x = priv->unclamped_hadj_value - lower;
  else if (priv->unclamped_hadj_value > upper)
    x = priv->unclamped_hadj_value - upper;
  else
    x = 0;

  if (overshoot_x)
    *overshoot_x = x;

  if (overshoot_y)
    *overshoot_y = y;

  return (x != 0 || y != 0);
}

static void
gtk_scrolled_window_allocate_child (GtkScrolledWindow   *swindow,
                                    int                  width,
                                    int                  height)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (swindow);
  GtkWidget *widget = GTK_WIDGET (swindow);
  GtkAllocation child_allocation;
  int sb_width;
  int sb_height;

  child_allocation = (GtkAllocation) {0, 0, width, height};

  /* Get possible scrollbar dimensions */
  gtk_widget_measure (priv->vscrollbar, GTK_ORIENTATION_HORIZONTAL, -1,
                      &sb_width, NULL, NULL, NULL);
  gtk_widget_measure (priv->hscrollbar, GTK_ORIENTATION_VERTICAL, -1,
                      &sb_height, NULL, NULL, NULL);

  /* Subtract some things from our available allocation size */
  if (priv->vscrollbar_visible && !priv->use_indicators)
    {
      gboolean is_rtl;

      is_rtl = _gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL;

      if ((!is_rtl &&
           (priv->window_placement == GTK_CORNER_TOP_RIGHT ||
            priv->window_placement == GTK_CORNER_BOTTOM_RIGHT)) ||
          (is_rtl &&
           (priv->window_placement == GTK_CORNER_TOP_LEFT ||
            priv->window_placement == GTK_CORNER_BOTTOM_LEFT)))
        child_allocation.x += sb_width;

      child_allocation.width = MAX (1, child_allocation.width - sb_width);
    }

  if (priv->hscrollbar_visible && !priv->use_indicators)
    {

      if (priv->window_placement == GTK_CORNER_BOTTOM_LEFT ||
          priv->window_placement == GTK_CORNER_BOTTOM_RIGHT)
        child_allocation.y += (sb_height);

      child_allocation.height = MAX (1, child_allocation.height - sb_height);
    }

  gtk_widget_size_allocate (priv->child, &child_allocation, -1);
}

static void
gtk_scrolled_window_allocate_scrollbar (GtkScrolledWindow *scrolled_window,
                                        GtkWidget         *scrollbar,
                                        GtkAllocation     *allocation)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);
  GtkAllocation child_allocation, content_allocation;
  GtkWidget *widget = GTK_WIDGET (scrolled_window);
  int sb_height, sb_width;

  gtk_scrolled_window_inner_allocation (scrolled_window, &content_allocation);
  gtk_widget_measure (priv->vscrollbar, GTK_ORIENTATION_HORIZONTAL, -1,
                      &sb_width, NULL, NULL, NULL);
  gtk_widget_measure (priv->hscrollbar, GTK_ORIENTATION_VERTICAL, -1,
                      &sb_height, NULL, NULL, NULL);

  if (scrollbar == priv->hscrollbar)
    {
      child_allocation.x = content_allocation.x;

      if (priv->window_placement == GTK_CORNER_TOP_LEFT ||
          priv->window_placement == GTK_CORNER_TOP_RIGHT)
        {
          if (priv->use_indicators)
            child_allocation.y = content_allocation.y + content_allocation.height - sb_height;
          else
            child_allocation.y = content_allocation.y + content_allocation.height;
        }
      else
        {
          if (priv->use_indicators)
            child_allocation.y = content_allocation.y;
          else
            child_allocation.y = content_allocation.y - sb_height;
        }

      child_allocation.width = content_allocation.width;
      child_allocation.height = sb_height;
    }
  else
    {
      g_assert (scrollbar == priv->vscrollbar);

      if ((_gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL &&
           (priv->window_placement == GTK_CORNER_TOP_RIGHT ||
            priv->window_placement == GTK_CORNER_BOTTOM_RIGHT)) ||
          (_gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR &&
           (priv->window_placement == GTK_CORNER_TOP_LEFT ||
            priv->window_placement == GTK_CORNER_BOTTOM_LEFT)))
        {
          if (priv->use_indicators)
            child_allocation.x = content_allocation.x + content_allocation.width - sb_width;
          else
            child_allocation.x = content_allocation.x + content_allocation.width;
        }
      else
        {
          if (priv->use_indicators)
            child_allocation.x = content_allocation.x;
          else
            child_allocation.x = content_allocation.x - sb_width;
        }

      child_allocation.y = content_allocation.y;
      child_allocation.width = sb_width;
      child_allocation.height = content_allocation.height;
    }

  *allocation = child_allocation;
}

static void
_gtk_scrolled_window_set_adjustment_value (GtkScrolledWindow *scrolled_window,
                                           GtkAdjustment     *adjustment,
                                           double             value)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);
  double lower, upper, *prev_value;
  GtkPositionType edge_pos;
  gboolean vertical;

  lower = gtk_adjustment_get_lower (adjustment) - MAX_OVERSHOOT_DISTANCE;
  upper = gtk_adjustment_get_upper (adjustment) -
    gtk_adjustment_get_page_size (adjustment) + MAX_OVERSHOOT_DISTANCE;

  if (adjustment == gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (priv->hscrollbar)))
    vertical = FALSE;
  else if (adjustment == gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (priv->vscrollbar)))
    vertical = TRUE;
  else
    return;

  if (vertical)
    prev_value = &priv->unclamped_vadj_value;
  else
    prev_value = &priv->unclamped_hadj_value;

  value = CLAMP (value, lower, upper);

  if (*prev_value == value)
    return;

  *prev_value = value;
  gtk_adjustment_set_value (adjustment, value);

  if (value == lower)
    edge_pos = vertical ? GTK_POS_TOP : GTK_POS_LEFT;
  else if (value == upper)
    edge_pos = vertical ? GTK_POS_BOTTOM : GTK_POS_RIGHT;
  else
    return;

  /* Invert horizontal edge position on RTL */
  if (!vertical &&
      _gtk_widget_get_direction (GTK_WIDGET (scrolled_window)) == GTK_TEXT_DIR_RTL)
    edge_pos = (edge_pos == GTK_POS_LEFT) ? GTK_POS_RIGHT : GTK_POS_LEFT;

  g_signal_emit (scrolled_window, signals[EDGE_OVERSHOT], 0, edge_pos);
}

static gboolean
scrolled_window_deceleration_cb (GtkWidget         *widget,
                                 GdkFrameClock     *frame_clock,
                                 gpointer           user_data)
{
  GtkScrolledWindow *scrolled_window = user_data;
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);
  GtkAdjustment *hadjustment, *vadjustment;
  gint64 current_time;
  double position, elapsed;
  gboolean retval = G_SOURCE_REMOVE;

  current_time = gdk_frame_clock_get_frame_time (frame_clock);
  elapsed = (current_time - priv->last_deceleration_time) / (double)G_TIME_SPAN_SECOND;
  priv->last_deceleration_time = current_time;

  hadjustment = gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (priv->hscrollbar));
  vadjustment = gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (priv->vscrollbar));

  gtk_scrolled_window_invalidate_overshoot (scrolled_window);

  if (priv->hscrolling &&
      gtk_kinetic_scrolling_tick (priv->hscrolling, elapsed, &position, NULL))
    {
      priv->unclamped_hadj_value = position;
      gtk_adjustment_set_value (hadjustment, position);
      retval = G_SOURCE_CONTINUE;
    }

  if (priv->vscrolling &&
      gtk_kinetic_scrolling_tick (priv->vscrolling, elapsed, &position, NULL))
    {
      priv->unclamped_vadj_value = position;
      gtk_adjustment_set_value (vadjustment, position);
      retval = G_SOURCE_CONTINUE;
    }

  if (retval == G_SOURCE_REMOVE)
    gtk_scrolled_window_cancel_deceleration (scrolled_window);
  else
    gtk_scrolled_window_invalidate_overshoot (scrolled_window);

  return retval;
}

static void
gtk_scrolled_window_cancel_deceleration (GtkScrolledWindow *scrolled_window)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  if (priv->deceleration_id)
    {
      gtk_widget_remove_tick_callback (GTK_WIDGET (scrolled_window),
                                       priv->deceleration_id);
      priv->deceleration_id = 0;
    }
}

static void
kinetic_scroll_stop_notify (GtkScrolledWindow *scrolled_window)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);
  priv->deceleration_id = 0;
}

static void
gtk_scrolled_window_accumulate_velocity (GtkKineticScrolling **scrolling, double elapsed, double *velocity)
{
    if (!*scrolling)
      return;

    double last_velocity;
    gtk_kinetic_scrolling_tick (*scrolling, elapsed, NULL, &last_velocity);
    if (((*velocity >= 0) == (last_velocity >= 0)) &&
        (fabs (*velocity) >= fabs (last_velocity) * VELOCITY_ACCUMULATION_FLOOR))
      {
        double min_velocity = last_velocity * VELOCITY_ACCUMULATION_FLOOR;
        double max_velocity = last_velocity * VELOCITY_ACCUMULATION_CEIL;
        double accumulation_multiplier = (*velocity - min_velocity) / (max_velocity - min_velocity);
        *velocity += last_velocity * fmin (accumulation_multiplier, VELOCITY_ACCUMULATION_MAX);
      }
    g_clear_pointer (scrolling, gtk_kinetic_scrolling_free);
}

static void
gtk_scrolled_window_start_deceleration (GtkScrolledWindow *scrolled_window)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);
  GdkFrameClock *frame_clock;
  gint64 current_time;
  double elapsed;
  int overshoot_x, overshoot_y;

  g_return_if_fail (priv->deceleration_id == 0);

  frame_clock = gtk_widget_get_frame_clock (GTK_WIDGET (scrolled_window));

  current_time = gdk_frame_clock_get_frame_time (frame_clock);
  elapsed = (current_time - priv->last_deceleration_time) / (double)G_TIME_SPAN_SECOND;
  priv->last_deceleration_time = current_time;

  _gtk_scrolled_window_get_overshoot (scrolled_window, &overshoot_x, &overshoot_y);

  if (may_hscroll (scrolled_window))
    {
      double lower,upper;
      GtkAdjustment *hadjustment;

      gtk_scrolled_window_accumulate_velocity (&priv->hscrolling, elapsed, &priv->x_velocity);
      g_clear_pointer (&priv->hscrolling, gtk_kinetic_scrolling_free);

      if (priv->x_velocity != 0 || overshoot_x != 0)
        {
          hadjustment = gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (priv->hscrollbar));
          lower = gtk_adjustment_get_lower (hadjustment);
          upper = gtk_adjustment_get_upper (hadjustment);
          upper -= gtk_adjustment_get_page_size (hadjustment);
          priv->hscrolling =
            gtk_kinetic_scrolling_new (lower,
                                       upper,
                                       MAX_OVERSHOOT_DISTANCE,
                                       DECELERATION_FRICTION,
                                       OVERSHOOT_FRICTION,
                                       priv->unclamped_hadj_value,
                                       priv->x_velocity);
        }
    }
  else
    g_clear_pointer (&priv->hscrolling, gtk_kinetic_scrolling_free);

  if (may_vscroll (scrolled_window))
    {
      double lower,upper;
      GtkAdjustment *vadjustment;

      gtk_scrolled_window_accumulate_velocity (&priv->vscrolling, elapsed, &priv->y_velocity);
      g_clear_pointer (&priv->vscrolling, gtk_kinetic_scrolling_free);

      if (priv->y_velocity != 0 || overshoot_y != 0)
        {
          vadjustment = gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (priv->vscrollbar));
          lower = gtk_adjustment_get_lower(vadjustment);
          upper = gtk_adjustment_get_upper(vadjustment);
          upper -= gtk_adjustment_get_page_size(vadjustment);
          priv->vscrolling =
            gtk_kinetic_scrolling_new (lower,
                                       upper,
                                       MAX_OVERSHOOT_DISTANCE,
                                       DECELERATION_FRICTION,
                                       OVERSHOOT_FRICTION,
                                       priv->unclamped_vadj_value,
                                       priv->y_velocity);
        }
    }
  else
    g_clear_pointer (&priv->vscrolling, gtk_kinetic_scrolling_free);

  priv->deceleration_id = gtk_widget_add_tick_callback (GTK_WIDGET (scrolled_window),
                                                        scrolled_window_deceleration_cb, scrolled_window,
                                                        (GDestroyNotify) kinetic_scroll_stop_notify);
}

static gboolean
gtk_scrolled_window_focus (GtkWidget        *widget,
                           GtkDirectionType  direction)
{
  GtkScrolledWindow *scrolled_window = GTK_SCROLLED_WINDOW (widget);
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);
  gboolean had_focus_child;

  had_focus_child = gtk_widget_get_focus_child (widget) != NULL;

  if (priv->focus_out)
    {
      priv->focus_out = FALSE; /* Clear this to catch the wrap-around case */
      return FALSE;
    }

  if (gtk_widget_is_focus (widget))
    return FALSE;

  /* We only put the scrolled window itself in the focus chain if it
   * isn't possible to focus any children.
   */
  if (priv->child)
    {
      if (gtk_widget_child_focus (priv->child, direction))
        return TRUE;
    }

  if (!had_focus_child && gtk_widget_get_can_focus (widget))
    {
      gtk_widget_grab_focus (widget);
      return TRUE;
    }
  else
    return FALSE;
}

static void
gtk_scrolled_window_adjustment_changed (GtkAdjustment *adjustment,
                                        gpointer       data)
{
  GtkScrolledWindow *scrolled_window = data;
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  if (adjustment == gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (priv->hscrollbar)))
    {
      if (priv->hscrollbar_policy == GTK_POLICY_AUTOMATIC)
        {
          gboolean visible;

          visible = priv->hscrollbar_visible;
          gtk_scrolled_window_update_scrollbar_visibility_flags (scrolled_window, priv->hscrollbar);

          if (priv->hscrollbar_visible != visible)
            gtk_widget_queue_resize (GTK_WIDGET (scrolled_window));

          if (priv->hscrolling)
            {
              GtkKineticScrollingChange change;
              double lower = gtk_adjustment_get_lower (adjustment);
              double upper = gtk_adjustment_get_upper (adjustment);
              upper -= gtk_adjustment_get_page_size (adjustment);

              change = gtk_kinetic_scrolling_update_size (priv->hscrolling, lower, upper);

              if ((change & GTK_KINETIC_SCROLLING_CHANGE_IN_OVERSHOOT) &&
                  (change & (GTK_KINETIC_SCROLLING_CHANGE_UPPER | GTK_KINETIC_SCROLLING_CHANGE_LOWER)))
                {
                  g_clear_pointer (&priv->hscrolling, gtk_kinetic_scrolling_free);
                  priv->unclamped_hadj_value = gtk_adjustment_get_value (adjustment);
                  gtk_scrolled_window_invalidate_overshoot (scrolled_window);
                }
            }
        }
    }
  else if (adjustment == gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (priv->vscrollbar)))
    {
      if (priv->vscrollbar_policy == GTK_POLICY_AUTOMATIC)
        {
          gboolean visible;

          visible = priv->vscrollbar_visible;
          gtk_scrolled_window_update_scrollbar_visibility_flags (scrolled_window, priv->vscrollbar);

          if (priv->vscrollbar_visible != visible)
            gtk_widget_queue_resize (GTK_WIDGET (scrolled_window));

          if (priv->vscrolling)
            {
              GtkKineticScrollingChange change;
              double lower = gtk_adjustment_get_lower (adjustment);
              double upper = gtk_adjustment_get_upper (adjustment);
              upper -= gtk_adjustment_get_page_size (adjustment);

              change = gtk_kinetic_scrolling_update_size (priv->vscrolling, lower, upper);

              if ((change & GTK_KINETIC_SCROLLING_CHANGE_IN_OVERSHOOT) &&
                  (change & (GTK_KINETIC_SCROLLING_CHANGE_UPPER | GTK_KINETIC_SCROLLING_CHANGE_LOWER)))
                {
                  g_clear_pointer (&priv->vscrolling, gtk_kinetic_scrolling_free);
                  priv->unclamped_vadj_value = gtk_adjustment_get_value (adjustment);
                  gtk_scrolled_window_invalidate_overshoot (scrolled_window);
                }
            }
        }
    }

  if (!priv->hscrolling && !priv->vscrolling)
    gtk_scrolled_window_cancel_deceleration (scrolled_window);
}

static void
maybe_emit_edge_reached (GtkScrolledWindow *scrolled_window,
                         GtkAdjustment *adjustment)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);
  double value, lower, upper, page_size;
  GtkPositionType edge_pos;
  gboolean vertical;

  if (adjustment == gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (priv->hscrollbar)))
    vertical = FALSE;
  else if (adjustment == gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (priv->vscrollbar)))
    vertical = TRUE;
  else
    return;

  value = gtk_adjustment_get_value (adjustment);
  lower = gtk_adjustment_get_lower (adjustment);
  upper = gtk_adjustment_get_upper (adjustment);
  page_size = gtk_adjustment_get_page_size (adjustment);

  if (value == lower)
    edge_pos = vertical ? GTK_POS_TOP: GTK_POS_LEFT;
  else if (value == upper - page_size)
    edge_pos = vertical ? GTK_POS_BOTTOM : GTK_POS_RIGHT;
  else
    return;

  if (!vertical &&
      _gtk_widget_get_direction (GTK_WIDGET (scrolled_window)) == GTK_TEXT_DIR_RTL)
    edge_pos = (edge_pos == GTK_POS_LEFT) ? GTK_POS_RIGHT : GTK_POS_LEFT;

  g_signal_emit (scrolled_window, signals[EDGE_REACHED], 0, edge_pos);
}

static void
gtk_scrolled_window_adjustment_value_changed (GtkAdjustment *adjustment,
                                              gpointer       user_data)
{
  GtkScrolledWindow *scrolled_window = user_data;
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  maybe_emit_edge_reached (scrolled_window, adjustment);

  /* Allow overshooting for kinetic scrolling operations */
  if (priv->deceleration_id)
    return;

  /* Ensure GtkAdjustment and unclamped values are in sync */
  if (adjustment == gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (priv->hscrollbar)))
    priv->unclamped_hadj_value = gtk_adjustment_get_value (adjustment);
  else if (adjustment == gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (priv->vscrollbar)))
    priv->unclamped_vadj_value = gtk_adjustment_get_value (adjustment);
}

static gboolean
gtk_widget_should_animate (GtkWidget *widget)
{
  if (!gtk_widget_get_mapped (widget))
    return FALSE;

  return gtk_settings_get_enable_animations (gtk_widget_get_settings (widget));
}

static void
gtk_scrolled_window_update_animating (GtkScrolledWindow *sw)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (sw);
  GtkAdjustment *adjustment;
  GdkFrameClock *clock = NULL;
  guint duration = 0;

  if (gtk_widget_should_animate (GTK_WIDGET (sw)))
    {
      clock = gtk_widget_get_frame_clock (GTK_WIDGET (sw)),
      duration = ANIMATION_DURATION;
    }

  adjustment = gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (priv->hscrollbar));
  gtk_adjustment_enable_animation (adjustment, clock, duration);

  adjustment = gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (priv->vscrollbar));
  gtk_adjustment_enable_animation (adjustment, clock, duration);
}

static void
gtk_scrolled_window_map (GtkWidget *widget)
{
  GtkScrolledWindow *scrolled_window = GTK_SCROLLED_WINDOW (widget);

  GTK_WIDGET_CLASS (gtk_scrolled_window_parent_class)->map (widget);

  gtk_scrolled_window_update_animating (scrolled_window);
  gtk_scrolled_window_update_use_indicators (scrolled_window);
}

static void
indicator_reset (Indicator *indicator)
{
  g_clear_handle_id (&indicator->conceil_timer, g_source_remove);
  g_clear_handle_id (&indicator->over_timeout_id, g_source_remove);

  if (indicator->scrollbar && indicator->tick_id)
    {
      gtk_widget_remove_tick_callback (indicator->scrollbar,
                                       indicator->tick_id);
      indicator->tick_id = 0;
    }

  indicator->over = FALSE;
  gtk_progress_tracker_finish (&indicator->tracker);
  indicator->current_pos = indicator->source_pos = indicator->target_pos = 0;
  indicator->last_scroll_time = 0;
}

static void
gtk_scrolled_window_unmap (GtkWidget *widget)
{
  GtkScrolledWindow *scrolled_window = GTK_SCROLLED_WINDOW (widget);
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  GTK_WIDGET_CLASS (gtk_scrolled_window_parent_class)->unmap (widget);

  gtk_scrolled_window_update_animating (scrolled_window);

  indicator_reset (&priv->hindicator);
  indicator_reset (&priv->vindicator);
}

static void
indicator_set_fade (Indicator *indicator,
                    double     pos)
{
  gboolean visible, changed;

  changed = indicator->current_pos != pos;
  indicator->current_pos = pos;

  visible = indicator->current_pos != 0.0 || indicator->target_pos != 0.0;

  if (visible && indicator->conceil_timer == 0)
    {
      indicator->conceil_timer = g_timeout_add (INDICATOR_FADE_OUT_TIME, maybe_hide_indicator, indicator);
      gdk_source_set_static_name_by_id (indicator->conceil_timer, "[gtk] maybe_hide_indicator");
    }
  if (!visible && indicator->conceil_timer != 0)
    {
      g_source_remove (indicator->conceil_timer);
      indicator->conceil_timer = 0;
    }

  if (changed)
    {
      gtk_widget_set_opacity (indicator->scrollbar, indicator->current_pos);
    }
}

static gboolean
indicator_fade_cb (GtkWidget     *widget,
                   GdkFrameClock *frame_clock,
                   gpointer       user_data)
{
  Indicator *indicator = user_data;
  double t;

  gtk_progress_tracker_advance_frame (&indicator->tracker,
                                      gdk_frame_clock_get_frame_time (frame_clock));
  t = gtk_progress_tracker_get_ease_out_cubic (&indicator->tracker, FALSE);

  indicator_set_fade (indicator,
                      indicator->source_pos + (t * (indicator->target_pos - indicator->source_pos)));

  if (gtk_progress_tracker_get_state (&indicator->tracker) == GTK_PROGRESS_STATE_AFTER)
    {
      indicator->tick_id = 0;
      return FALSE;
    }

  return TRUE;
}

static void
indicator_start_fade (Indicator *indicator,
                      double     target)
{
  if (indicator->target_pos == target)
    return;

  indicator->target_pos = target;

  if (target != 0.0)
    indicator->last_scroll_time = g_get_monotonic_time ();

  if (gtk_widget_should_animate (indicator->scrollbar))
    {
      indicator->source_pos = indicator->current_pos;
      gtk_progress_tracker_start (&indicator->tracker, INDICATOR_FADE_OUT_DURATION * 1000, 0, 1.0);
      if (indicator->tick_id == 0)
        indicator->tick_id = gtk_widget_add_tick_callback (indicator->scrollbar, indicator_fade_cb, indicator, NULL);
    }
  else
    indicator_set_fade (indicator, target);
}

static gboolean
maybe_hide_indicator (gpointer data)
{
  Indicator *indicator = data;

  if (g_get_monotonic_time () - indicator->last_scroll_time >= INDICATOR_FADE_OUT_DELAY * 1000 &&
      !indicator->over)
    indicator_start_fade (indicator, 0.0);

  return G_SOURCE_CONTINUE;
}

static void
indicator_value_changed (GtkAdjustment *adjustment,
                         Indicator     *indicator)
{
  indicator->last_scroll_time = g_get_monotonic_time ();
  indicator_start_fade (indicator, 1.0);
}

static void
setup_indicator (GtkScrolledWindow *scrolled_window,
                 Indicator         *indicator,
                 GtkWidget         *scrollbar)
{
  GtkAdjustment *adjustment;

  if (scrollbar == NULL)
    return;

  adjustment = gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (scrollbar));

  indicator->scrollbar = scrollbar;

  gtk_widget_add_css_class (scrollbar, "overlay-indicator");
  g_signal_connect (adjustment, "value-changed",
                    G_CALLBACK (indicator_value_changed), indicator);

  gtk_widget_set_opacity (scrollbar, 0.0);
  indicator->current_pos = 0.0;
}

static void
remove_indicator (GtkScrolledWindow *scrolled_window,
                  Indicator         *indicator)
{
  GtkWidget *scrollbar;
  GtkAdjustment *adjustment;

  if (indicator->scrollbar == NULL)
    return;

  scrollbar = indicator->scrollbar;
  indicator->scrollbar = NULL;

  gtk_widget_remove_css_class (scrollbar, "overlay-indicator");

  adjustment = gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (scrollbar));
  g_signal_handlers_disconnect_by_data (adjustment, indicator);

  if (indicator->conceil_timer)
    {
      g_source_remove (indicator->conceil_timer);
      indicator->conceil_timer = 0;
    }

  if (indicator->over_timeout_id)
    {
      g_source_remove (indicator->over_timeout_id);
      indicator->over_timeout_id = 0;
    }

  if (indicator->tick_id)
    {
      gtk_widget_remove_tick_callback (scrollbar, indicator->tick_id);
      indicator->tick_id = 0;
    }

  gtk_widget_set_opacity (scrollbar, 1.0);
  indicator->current_pos = 1.0;
}

static void
gtk_scrolled_window_sync_use_indicators (GtkScrolledWindow *scrolled_window)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  if (priv->use_indicators)
    {
      setup_indicator (scrolled_window, &priv->hindicator, priv->hscrollbar);
      setup_indicator (scrolled_window, &priv->vindicator, priv->vscrollbar);
    }
  else
    {
      remove_indicator (scrolled_window, &priv->hindicator);
      remove_indicator (scrolled_window, &priv->vindicator);
    }
}

static void
gtk_scrolled_window_update_use_indicators (GtkScrolledWindow *scrolled_window)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);
  gboolean use_indicators;
  GtkSettings *settings = gtk_widget_get_settings (GTK_WIDGET (scrolled_window));
  gboolean overlay_scrolling;

  g_object_get (settings, "gtk-overlay-scrolling", &overlay_scrolling, NULL);

  use_indicators = overlay_scrolling && priv->overlay_scrolling;

  if (priv->use_indicators != use_indicators)
    {
      priv->use_indicators = use_indicators;

      if (gtk_widget_get_realized (GTK_WIDGET (scrolled_window)))
        gtk_scrolled_window_sync_use_indicators (scrolled_window);

      gtk_widget_queue_resize (GTK_WIDGET (scrolled_window));
    }
}

static void
gtk_scrolled_window_realize (GtkWidget *widget)
{
  GtkScrolledWindow *scrolled_window = GTK_SCROLLED_WINDOW (widget);
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);
  GtkSettings *settings;

  priv->hindicator.scrollbar = priv->hscrollbar;
  priv->vindicator.scrollbar = priv->vscrollbar;

  gtk_scrolled_window_sync_use_indicators (scrolled_window);

  settings = gtk_widget_get_settings (widget);
  g_signal_connect_swapped (settings, "notify::gtk-overlay-scrolling",
                            G_CALLBACK (gtk_scrolled_window_update_use_indicators), widget);

  GTK_WIDGET_CLASS (gtk_scrolled_window_parent_class)->realize (widget);
}

static void
gtk_scrolled_window_unrealize (GtkWidget *widget)
{
  GtkSettings *settings;

  settings = gtk_widget_get_settings (widget);

  g_signal_handlers_disconnect_by_func (settings, gtk_scrolled_window_update_use_indicators, widget);

  GTK_WIDGET_CLASS (gtk_scrolled_window_parent_class)->unrealize (widget);
}

/**
 * gtk_scrolled_window_get_min_content_width: (attributes org.gtk.Method.get_property=min-content-width)
 * @scrolled_window: a `GtkScrolledWindow`
 *
 * Gets the minimum content width of @scrolled_window.
 *
 * Returns: the minimum content width
 */
int
gtk_scrolled_window_get_min_content_width (GtkScrolledWindow *scrolled_window)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  g_return_val_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window), 0);

  return priv->min_content_width;
}

/**
 * gtk_scrolled_window_set_min_content_width: (attributes org.gtk.Method.set_property=min-content-width)
 * @scrolled_window: a `GtkScrolledWindow`
 * @width: the minimal content width
 *
 * Sets the minimum width that @scrolled_window should keep visible.
 *
 * Note that this can and (usually will) be smaller than the minimum
 * size of the content.
 *
 * It is a programming error to set the minimum content width to a
 * value greater than [property@Gtk.ScrolledWindow:max-content-width].
 */
void
gtk_scrolled_window_set_min_content_width (GtkScrolledWindow *scrolled_window,
                                           int                width)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window));

  g_return_if_fail (width == -1 || priv->max_content_width == -1 || width <= priv->max_content_width);

  if (priv->min_content_width != width)
    {
      priv->min_content_width = width;

      gtk_widget_queue_resize (GTK_WIDGET (scrolled_window));

      g_object_notify_by_pspec (G_OBJECT (scrolled_window), properties[PROP_MIN_CONTENT_WIDTH]);
    }
}

/**
 * gtk_scrolled_window_get_min_content_height: (attributes org.gtk.Method.get_property=min-content-height)
 * @scrolled_window: a `GtkScrolledWindow`
 *
 * Gets the minimal content height of @scrolled_window.
 *
 * Returns: the minimal content height
 */
int
gtk_scrolled_window_get_min_content_height (GtkScrolledWindow *scrolled_window)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  g_return_val_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window), 0);

  return priv->min_content_height;
}

/**
 * gtk_scrolled_window_set_min_content_height: (attributes org.gtk.Method.set_property=min-content-height)
 * @scrolled_window: a `GtkScrolledWindow`
 * @height: the minimal content height
 *
 * Sets the minimum height that @scrolled_window should keep visible.
 *
 * Note that this can and (usually will) be smaller than the minimum
 * size of the content.
 *
 * It is a programming error to set the minimum content height to a
 * value greater than [property@Gtk.ScrolledWindow:max-content-height].
 */
void
gtk_scrolled_window_set_min_content_height (GtkScrolledWindow *scrolled_window,
                                            int                height)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window));

  g_return_if_fail (height == -1 || priv->max_content_height == -1 || height <= priv->max_content_height);

  if (priv->min_content_height != height)
    {
      priv->min_content_height = height;

      gtk_widget_queue_resize (GTK_WIDGET (scrolled_window));

      g_object_notify_by_pspec (G_OBJECT (scrolled_window), properties[PROP_MIN_CONTENT_HEIGHT]);
    }
}

/**
 * gtk_scrolled_window_set_overlay_scrolling: (attributes org.gtk.Method.set_property=overlay-scrolling)
 * @scrolled_window: a `GtkScrolledWindow`
 * @overlay_scrolling: whether to enable overlay scrolling
 *
 * Enables or disables overlay scrolling for this scrolled window.
 */
void
gtk_scrolled_window_set_overlay_scrolling (GtkScrolledWindow *scrolled_window,
                                           gboolean           overlay_scrolling)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window));

  if (priv->overlay_scrolling != overlay_scrolling)
    {
      priv->overlay_scrolling = overlay_scrolling;

      gtk_scrolled_window_update_use_indicators (scrolled_window);

      g_object_notify_by_pspec (G_OBJECT (scrolled_window), properties[PROP_OVERLAY_SCROLLING]);
    }
}

/**
 * gtk_scrolled_window_get_overlay_scrolling: (attributes org.gtk.Method.get_property=overlay-scrolling)
 * @scrolled_window: a `GtkScrolledWindow`
 *
 * Returns whether overlay scrolling is enabled for this scrolled window.
 *
 * Returns: %TRUE if overlay scrolling is enabled
 */
gboolean
gtk_scrolled_window_get_overlay_scrolling (GtkScrolledWindow *scrolled_window)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  g_return_val_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window), TRUE);

  return priv->overlay_scrolling;
}

/**
 * gtk_scrolled_window_set_max_content_width: (attributes org.gtk.Method.set_property=max-content-width)
 * @scrolled_window: a `GtkScrolledWindow`
 * @width: the maximum content width
 *
 * Sets the maximum width that @scrolled_window should keep visible.
 *
 * The @scrolled_window will grow up to this width before it starts
 * scrolling the content.
 *
 * It is a programming error to set the maximum content width to a
 * value smaller than [property@Gtk.ScrolledWindow:min-content-width].
 */
void
gtk_scrolled_window_set_max_content_width (GtkScrolledWindow *scrolled_window,
                                           int                width)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window));

  g_return_if_fail (width == -1 || priv->min_content_width == -1 || width >= priv->min_content_width);

  if (width != priv->max_content_width)
    {
      priv->max_content_width = width;
      g_object_notify_by_pspec (G_OBJECT (scrolled_window), properties [PROP_MAX_CONTENT_WIDTH]);
      gtk_widget_queue_resize (GTK_WIDGET (scrolled_window));
    }
}

/**
 * gtk_scrolled_window_get_max_content_width: (attributes org.gtk.Method.get_property=max-content-width)
 * @scrolled_window: a `GtkScrolledWindow`
 *
 * Returns the maximum content width set.
 *
 * Returns: the maximum content width, or -1
 */
int
gtk_scrolled_window_get_max_content_width (GtkScrolledWindow *scrolled_window)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  g_return_val_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window), -1);

  return priv->max_content_width;
}

/**
 * gtk_scrolled_window_set_max_content_height: (attributes org.gtk.Method.set_property=max-content-height)
 * @scrolled_window: a `GtkScrolledWindow`
 * @height: the maximum content height
 *
 * Sets the maximum height that @scrolled_window should keep visible.
 *
 * The @scrolled_window will grow up to this height before it starts
 * scrolling the content.
 *
 * It is a programming error to set the maximum content height to a value
 * smaller than [property@Gtk.ScrolledWindow:min-content-height].
 */
void
gtk_scrolled_window_set_max_content_height (GtkScrolledWindow *scrolled_window,
                                            int                height)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window));

  g_return_if_fail (height == -1 || priv->min_content_height == -1 || height >= priv->min_content_height);

  if (height != priv->max_content_height)
    {
      priv->max_content_height = height;
      g_object_notify_by_pspec (G_OBJECT (scrolled_window), properties [PROP_MAX_CONTENT_HEIGHT]);
      gtk_widget_queue_resize (GTK_WIDGET (scrolled_window));
    }
}

/**
 * gtk_scrolled_window_get_max_content_height: (attributes org.gtk.Method.get_property=max-content-height)
 * @scrolled_window: a `GtkScrolledWindow`
 *
 * Returns the maximum content height set.
 *
 * Returns: the maximum content height, or -1
 */
int
gtk_scrolled_window_get_max_content_height (GtkScrolledWindow *scrolled_window)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  g_return_val_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window), -1);

  return priv->max_content_height;
}

/**
 * gtk_scrolled_window_set_propagate_natural_width: (attributes org.gtk.Method.set_property=propagate-natural-width)
 * @scrolled_window: a `GtkScrolledWindow`
 * @propagate: whether to propagate natural width
 *
 * Sets whether the natural width of the child should be calculated
 * and propagated through the scrolled window’s requested natural width.
 */
void
gtk_scrolled_window_set_propagate_natural_width (GtkScrolledWindow *scrolled_window,
                                                 gboolean           propagate)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window));

  propagate = !!propagate;

  if (priv->propagate_natural_width != propagate)
    {
      priv->propagate_natural_width = propagate;
      g_object_notify_by_pspec (G_OBJECT (scrolled_window), properties [PROP_PROPAGATE_NATURAL_WIDTH]);
      gtk_widget_queue_resize (GTK_WIDGET (scrolled_window));
    }
}

/**
 * gtk_scrolled_window_get_propagate_natural_width: (attributes org.gtk.Method.get_property=propagate-natural-width)
 * @scrolled_window: a `GtkScrolledWindow`
 *
 * Reports whether the natural width of the child will be calculated
 * and propagated through the scrolled window’s requested natural width.
 *
 * Returns: whether natural width propagation is enabled.
 */
gboolean
gtk_scrolled_window_get_propagate_natural_width (GtkScrolledWindow *scrolled_window)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  g_return_val_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window), -1);

  return priv->propagate_natural_width;
}

/**
 * gtk_scrolled_window_set_propagate_natural_height: (attributes org.gtk.Method.set_property=propagate-natural-height)
 * @scrolled_window: a `GtkScrolledWindow`
 * @propagate: whether to propagate natural height
 *
 * Sets whether the natural height of the child should be calculated
 * and propagated through the scrolled window’s requested natural height.
 */
void
gtk_scrolled_window_set_propagate_natural_height (GtkScrolledWindow *scrolled_window,
                                                  gboolean           propagate)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window));

  propagate = !!propagate;

  if (priv->propagate_natural_height != propagate)
    {
      priv->propagate_natural_height = propagate;
      g_object_notify_by_pspec (G_OBJECT (scrolled_window), properties [PROP_PROPAGATE_NATURAL_HEIGHT]);
      gtk_widget_queue_resize (GTK_WIDGET (scrolled_window));
    }
}

/**
 * gtk_scrolled_window_get_propagate_natural_height: (attributes org.gtk.Method.get_property=propagate-natural-height)
 * @scrolled_window: a `GtkScrolledWindow`
 *
 * Reports whether the natural height of the child will be calculated
 * and propagated through the scrolled window’s requested natural height.
 *
 * Returns: whether natural height propagation is enabled.
 */
gboolean
gtk_scrolled_window_get_propagate_natural_height (GtkScrolledWindow *scrolled_window)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  g_return_val_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window), -1);

  return priv->propagate_natural_height;
}

/**
 * gtk_scrolled_window_set_child: (attributes org.gtk.Method.set_property=child)
 * @scrolled_window: a `GtkScrolledWindow`
 * @child: (nullable): the child widget
 *
 * Sets the child widget of @scrolled_window.
 *
 * If @child does not implement the [iface@Gtk.Scrollable] interface,
 * the scrolled window will add @child to a [class@Gtk.Viewport] instance
 * and then add the viewport as its child widget.
 */
void
gtk_scrolled_window_set_child (GtkScrolledWindow *scrolled_window,
                               GtkWidget         *child)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);
  GtkWidget *scrollable_child;

  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window));
  g_return_if_fail (child == NULL ||
                    priv->child == child ||
                    (priv->auto_added_viewport && gtk_viewport_get_child (GTK_VIEWPORT (priv->child)) == child) ||
                    gtk_widget_get_parent (child) == NULL);

  if (priv->child == child ||
      (priv->auto_added_viewport && gtk_viewport_get_child (GTK_VIEWPORT (priv->child)) == child))
    return;

  if (priv->child)
    {
      if (priv->auto_added_viewport)
        gtk_viewport_set_child (GTK_VIEWPORT (priv->child), NULL);

      g_object_set (priv->child,
                    "hadjustment", NULL,
                    "vadjustment", NULL,
                    NULL);

      g_clear_pointer (&priv->child, gtk_widget_unparent);
      priv->auto_added_viewport = FALSE;
    }

  if (child)
    {
      GtkAdjustment *hadj, *vadj;

      /* gtk_scrolled_window_set_[hv]adjustment have the side-effect
       * of creating the scrollbars
       */
      if (!priv->hscrollbar)
        gtk_scrolled_window_set_hadjustment (scrolled_window, NULL);

      if (!priv->vscrollbar)
        gtk_scrolled_window_set_vadjustment (scrolled_window, NULL);

      hadj = gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (priv->hscrollbar));
      vadj = gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (priv->vscrollbar));

      if (GTK_IS_SCROLLABLE (child))
        {
          scrollable_child = child;
          priv->auto_added_viewport = FALSE;
        }
      else
        {
          scrollable_child = gtk_viewport_new (hadj, vadj);
          gtk_viewport_set_child (GTK_VIEWPORT (scrollable_child), child);
          priv->auto_added_viewport = TRUE;
        }

      priv->child = scrollable_child;
      gtk_widget_insert_after (scrollable_child, GTK_WIDGET (scrolled_window), NULL);

      g_object_set (scrollable_child,
                    "hadjustment", hadj,
                    "vadjustment", vadj,
                    NULL);
    }

  if (priv->child)
    {
      gtk_accessible_update_relation (GTK_ACCESSIBLE (priv->hscrollbar),
                                      GTK_ACCESSIBLE_RELATION_CONTROLS, priv->child, NULL,
                                      -1);
      gtk_accessible_update_relation (GTK_ACCESSIBLE (priv->vscrollbar),
                                      GTK_ACCESSIBLE_RELATION_CONTROLS, priv->child, NULL,
                                      -1);
    }
  else
    {
      gtk_accessible_reset_relation (GTK_ACCESSIBLE (priv->hscrollbar),
                                     GTK_ACCESSIBLE_RELATION_CONTROLS);
      gtk_accessible_reset_relation (GTK_ACCESSIBLE (priv->vscrollbar),
                                     GTK_ACCESSIBLE_RELATION_CONTROLS);
    }

  g_object_notify_by_pspec (G_OBJECT (scrolled_window), properties[PROP_CHILD]);
}

/**
 * gtk_scrolled_window_get_child: (attributes org.gtk.Method.get_property=child)
 * @scrolled_window: a `GtkScrolledWindow`
 *
 * Gets the child widget of @scrolled_window.
 *
 * If the scrolled window automatically added a [class@Gtk.Viewport], this
 * function will return the viewport widget, and you can retrieve its child
 * using [method@Gtk.Viewport.get_child].
 *
 * Returns: (nullable) (transfer none): the child widget of @scrolled_window
 */
GtkWidget *
gtk_scrolled_window_get_child (GtkScrolledWindow *scrolled_window)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  g_return_val_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window), NULL);

  return priv->child;
}
