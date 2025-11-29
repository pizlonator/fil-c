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

#include "gtkpaned.h"

#include "gtkaccessiblerange.h"
#include "gtkcssboxesprivate.h"
#include "gtkeventcontrollermotion.h"
#include "gtkgesturepan.h"
#include "gtkgesturesingle.h"
#include "gtkpanedhandleprivate.h"
#include "gtkmarshalers.h"
#include "gtkorientable.h"
#include "gtkprivate.h"
#include "gtktypebuiltins.h"
#include "gtkwidgetprivate.h"
#include "gtkbuildable.h"

#include <math.h>

/**
 * GtkPaned:
 *
 * A widget with two panes, arranged either horizontally or vertically.
 *
 * ![An example GtkPaned](panes.png)
 *
 * The division between the two panes is adjustable by the user
 * by dragging a handle.
 *
 * Child widgets are added to the panes of the widget with
 * [method@Gtk.Paned.set_start_child] and [method@Gtk.Paned.set_end_child].
 * The division between the two children is set by default from the size
 * requests of the children, but it can be adjusted by the user.
 *
 * A paned widget draws a separator between the two child widgets and a
 * small handle that the user can drag to adjust the division. It does not
 * draw any relief around the children or around the separator. (The space
 * in which the separator is called the gutter.) Often, it is useful to put
 * each child inside a [class@Gtk.Frame] so that the gutter appears as a
 * ridge. No separator is drawn if one of the children is missing.
 *
 * Each child has two options that can be set, "resize" and "shrink". If
 * "resize" is true then, when the `GtkPaned` is resized, that child will
 * expand or shrink along with the paned widget. If "shrink" is true, then
 * that child can be made smaller than its requisition by the user.
 * Setting "shrink" to false allows the application to set a minimum size.
 * If "resize" is false for both children, then this is treated as if
 * "resize" is true for both children.
 *
 * The application can set the position of the slider as if it were set
 * by the user, by calling [method@Gtk.Paned.set_position].
 *
 * # CSS nodes
 *
 * ```
 * paned
 * ├── <child>
 * ├── separator[.wide]
 * ╰── <child>
 * ```
 *
 * `GtkPaned` has a main CSS node with name paned, and a subnode for
 * the separator with name separator. The subnode gets a .wide style
 * class when the paned is supposed to be wide.
 *
 * In horizontal orientation, the nodes are arranged based on the text
 * direction, so in left-to-right mode, :first-child will select the
 * leftmost child, while it will select the rightmost child in
 * RTL layouts.
 *
 * ## Creating a paned widget with minimum sizes.
 *
 * ```c
 * GtkWidget *hpaned = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
 * GtkWidget *frame1 = gtk_frame_new (NULL);
 * GtkWidget *frame2 = gtk_frame_new (NULL);
 *
 * gtk_widget_set_size_request (hpaned, 200, -1);
 *
 * gtk_paned_set_start_child (GTK_PANED (hpaned), frame1);
 * gtk_paned_set_resize_start_child (GTK_PANED (hpaned), TRUE);
 * gtk_paned_set_shrink_start_child (GTK_PANED (hpaned), FALSE);
 * gtk_widget_set_size_request (frame1, 50, -1);
 *
 * gtk_paned_set_end_child (GTK_PANED (hpaned), frame2);
 * gtk_paned_set_resize_end_child (GTK_PANED (hpaned), FALSE);
 * gtk_paned_set_shrink_end_child (GTK_PANED (hpaned), FALSE);
 * gtk_widget_set_size_request (frame2, 50, -1);
 * ```
 */

#define HANDLE_EXTRA_SIZE 6

typedef struct _GtkPanedClass   GtkPanedClass;

struct _GtkPaned
{
  GtkWidget parent_instance;

  GtkPaned       *first_paned;
  GtkWidget      *start_child;
  GtkWidget      *end_child;
  GtkWidget      *last_start_child_focus;
  GtkWidget      *last_end_child_focus;
  GtkWidget      *saved_focus;
  GtkOrientation  orientation;

  GtkWidget     *handle_widget;

  GtkGesture    *pan_gesture;  /* Used for touch */
  GtkGesture    *drag_gesture; /* Used for mice */

  int           start_child_size;
  int           drag_pos;
  int           last_allocation;
  int           max_position;
  int           min_position;
  int           original_position;

  guint         in_recursion  : 1;
  guint         resize_start_child : 1;
  guint         shrink_start_child : 1;
  guint         resize_end_child : 1;
  guint         shrink_end_child : 1;
  guint         position_set  : 1;
  guint         panning       : 1;
};

struct _GtkPanedClass
{
  GtkWidgetClass parent_class;

  gboolean (* cycle_child_focus)   (GtkPaned      *paned,
                                    gboolean       reverse);
  gboolean (* toggle_handle_focus) (GtkPaned      *paned);
  gboolean (* move_handle)         (GtkPaned      *paned,
                                    GtkScrollType  scroll);
  gboolean (* cycle_handle_focus)  (GtkPaned      *paned,
                                    gboolean       reverse);
  gboolean (* accept_position)     (GtkPaned      *paned);
  gboolean (* cancel_position)     (GtkPaned      *paned);
};

enum {
  PROP_0,
  PROP_POSITION,
  PROP_POSITION_SET,
  PROP_MIN_POSITION,
  PROP_MAX_POSITION,
  PROP_WIDE_HANDLE,
  PROP_RESIZE_START_CHILD,
  PROP_RESIZE_END_CHILD,
  PROP_SHRINK_START_CHILD,
  PROP_SHRINK_END_CHILD,
  PROP_START_CHILD,
  PROP_END_CHILD,
  LAST_PROP,

  /* GtkOrientable */
  PROP_ORIENTATION,
};

enum {
  CYCLE_CHILD_FOCUS,
  TOGGLE_HANDLE_FOCUS,
  MOVE_HANDLE,
  CYCLE_HANDLE_FOCUS,
  ACCEPT_POSITION,
  CANCEL_POSITION,
  LAST_SIGNAL
};

static void     gtk_paned_set_property          (GObject          *object,
                                                 guint             prop_id,
                                                 const GValue     *value,
                                                 GParamSpec       *pspec);
static void     gtk_paned_get_property          (GObject          *object,
                                                 guint             prop_id,
                                                 GValue           *value,
                                                 GParamSpec       *pspec);
static void     gtk_paned_dispose               (GObject          *object);
static void     gtk_paned_measure (GtkWidget *widget,
                                   GtkOrientation  orientation,
                                   int             for_size,
                                   int            *minimum,
                                   int            *natural,
                                   int            *minimum_baseline,
                                   int            *natural_baseline);
static void     gtk_paned_size_allocate         (GtkWidget           *widget,
                                                 int                  width,
                                                 int                  height,
                                                 int                  baseline);
static void     gtk_paned_unrealize             (GtkWidget           *widget);
static void     gtk_paned_css_changed           (GtkWidget           *widget,
                                                 GtkCssStyleChange   *change);
static void     gtk_paned_calc_position         (GtkPaned         *paned,
                                                 int               allocation,
                                                 int               start_child_req,
                                                 int               end_child_req);
static void     gtk_paned_set_focus_child       (GtkWidget        *widget,
                                                 GtkWidget        *child);
static void     gtk_paned_set_saved_focus       (GtkPaned         *paned,
                                                 GtkWidget        *widget);
static void     gtk_paned_set_first_paned       (GtkPaned         *paned,
                                                 GtkPaned         *first_paned);
static void     gtk_paned_set_last_start_child_focus (GtkPaned         *paned,
                                                 GtkWidget        *widget);
static void     gtk_paned_set_last_end_child_focus (GtkPaned         *paned,
                                                 GtkWidget        *widget);
static gboolean gtk_paned_cycle_child_focus     (GtkPaned         *paned,
                                                 gboolean          reverse);
static gboolean gtk_paned_cycle_handle_focus    (GtkPaned         *paned,
                                                 gboolean          reverse);
static gboolean gtk_paned_move_handle           (GtkPaned         *paned,
                                                 GtkScrollType     scroll);
static gboolean gtk_paned_accept_position       (GtkPaned         *paned);
static gboolean gtk_paned_cancel_position       (GtkPaned         *paned);
static gboolean gtk_paned_toggle_handle_focus   (GtkPaned         *paned);

static void     update_drag                     (GtkPaned         *paned,
                                                 int               xpos,
                                                 int               ypos);

static void gtk_paned_buildable_iface_init (GtkBuildableIface *iface);

static void gtk_paned_accessible_range_init (GtkAccessibleRangeInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkPaned, gtk_paned, GTK_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ORIENTABLE, NULL)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ACCESSIBLE_RANGE,
                                                gtk_paned_accessible_range_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                gtk_paned_buildable_iface_init))

static guint signals[LAST_SIGNAL] = { 0 };
static GParamSpec *paned_props[LAST_PROP] = { NULL, };

static void
add_tab_bindings (GtkWidgetClass  *widget_class,
                  GdkModifierType  modifiers)
{
  gtk_widget_class_add_binding_signal (widget_class,
                                       GDK_KEY_Tab, modifiers,
                                       "toggle-handle-focus",
                                       NULL);
  gtk_widget_class_add_binding_signal (widget_class,
                                       GDK_KEY_KP_Tab, modifiers,
                                       "toggle-handle-focus",
                                       NULL);
}

static void
add_move_binding (GtkWidgetClass  *widget_class,
                  guint            keyval,
                  GdkModifierType  mask,
                  GtkScrollType    scroll)
{
  gtk_widget_class_add_binding_signal (widget_class,
                                       keyval, mask,
                                       "move-handle",
                                       "(i)", scroll);
}

static void
get_handle_area (GtkPaned        *paned,
                 graphene_rect_t *area)
{
  int extra = 0;

  if (!gtk_widget_compute_bounds (paned->handle_widget, GTK_WIDGET (paned), area))
    return;

  if (!gtk_paned_get_wide_handle (paned))
    extra = HANDLE_EXTRA_SIZE;

  graphene_rect_inset (area, - extra, - extra);
}

static void
gtk_paned_compute_expand (GtkWidget *widget,
                          gboolean  *hexpand,
                          gboolean  *vexpand)
{
  GtkPaned *paned = GTK_PANED (widget);
  gboolean h = FALSE;
  gboolean v = FALSE;

  if (paned->start_child)
    {
      h = h || gtk_widget_compute_expand (paned->start_child, GTK_ORIENTATION_HORIZONTAL);
      v = v || gtk_widget_compute_expand (paned->start_child, GTK_ORIENTATION_VERTICAL);
    }

  if (paned->end_child)
    {
      h = h || gtk_widget_compute_expand (paned->end_child, GTK_ORIENTATION_HORIZONTAL);
      v = v || gtk_widget_compute_expand (paned->end_child, GTK_ORIENTATION_VERTICAL);
    }

  *hexpand = h;
  *vexpand = v;
}

static GtkSizeRequestMode
gtk_paned_get_request_mode (GtkWidget *widget)
{
  GtkPaned *paned = GTK_PANED (widget);
  int wfh = 0, hfw = 0;

  if (paned->start_child)
    {
      switch (gtk_widget_get_request_mode (paned->start_child))
        {
        case GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH:
          hfw++;
          break;
        case GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT:
          wfh++;
          break;
        case GTK_SIZE_REQUEST_CONSTANT_SIZE:
        default:
          break;
        }
    }
  if (paned->end_child)
    {
      switch (gtk_widget_get_request_mode (paned->end_child))
        {
        case GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH:
          hfw++;
          break;
        case GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT:
          wfh++;
          break;
        case GTK_SIZE_REQUEST_CONSTANT_SIZE:
        default:
          break;
        }
    }

  if (hfw == 0 && wfh == 0)
    return GTK_SIZE_REQUEST_CONSTANT_SIZE;
  else
    return wfh > hfw ?
        GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT :
        GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH;
}

static void
gtk_paned_set_orientation (GtkPaned       *self,
                           GtkOrientation  orientation)
{
  if (self->orientation != orientation)
    {
      static const char *cursor_name[2] = {
        "col-resize",
        "row-resize",
      };

      self->orientation = orientation;

      gtk_widget_update_orientation (GTK_WIDGET (self), self->orientation);
      gtk_widget_set_cursor_from_name (self->handle_widget,
                                       cursor_name[orientation]);
      gtk_gesture_pan_set_orientation (GTK_GESTURE_PAN (self->pan_gesture),
                                       orientation);

      gtk_widget_queue_resize (GTK_WIDGET (self));
      g_object_notify (G_OBJECT (self), "orientation");
    }
}

static void
gtk_paned_class_init (GtkPanedClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->set_property = gtk_paned_set_property;
  object_class->get_property = gtk_paned_get_property;
  object_class->dispose = gtk_paned_dispose;

  widget_class->measure = gtk_paned_measure;
  widget_class->size_allocate = gtk_paned_size_allocate;
  widget_class->unrealize = gtk_paned_unrealize;
  widget_class->focus = gtk_widget_focus_child;
  widget_class->set_focus_child = gtk_paned_set_focus_child;
  widget_class->css_changed = gtk_paned_css_changed;
  widget_class->get_request_mode = gtk_paned_get_request_mode;
  widget_class->compute_expand = gtk_paned_compute_expand;

  class->cycle_child_focus = gtk_paned_cycle_child_focus;
  class->toggle_handle_focus = gtk_paned_toggle_handle_focus;
  class->move_handle = gtk_paned_move_handle;
  class->cycle_handle_focus = gtk_paned_cycle_handle_focus;
  class->accept_position = gtk_paned_accept_position;
  class->cancel_position = gtk_paned_cancel_position;

  /**
   * GtkPaned:position: (attributes org.gtk.Property.get=gtk_paned_get_position org.gtk.Property.set=gtk_paned_set_position)
   *
   * Position of the separator in pixels, from the left/top.
   */
  paned_props[PROP_POSITION] =
    g_param_spec_int ("position", NULL, NULL,
                      0, G_MAXINT, 0,
                      GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkPaned:position-set:
   *
   * Whether the [property@Gtk.Paned:position] property has been set.
   */
  paned_props[PROP_POSITION_SET] =
    g_param_spec_boolean ("position-set", NULL, NULL,
                          FALSE,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkPaned:min-position:
   *
   * The smallest possible value for the [property@Gtk.Paned:position]
   * property.
   *
   * This property is derived from the size and shrinkability
   * of the widget's children.
   */
  paned_props[PROP_MIN_POSITION] =
    g_param_spec_int ("min-position", NULL, NULL,
                      0, G_MAXINT, 0,
                      GTK_PARAM_READABLE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkPaned:max-position:
   *
   * The largest possible value for the [property@Gtk.Paned:position]
   * property.
   *
   * This property is derived from the size and shrinkability
   * of the widget's children.
   */
  paned_props[PROP_MAX_POSITION] =
    g_param_spec_int ("max-position", NULL, NULL,
                      0, G_MAXINT, G_MAXINT,
                      GTK_PARAM_READABLE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkPaned:wide-handle: (attributes org.gtk.Property.get=gtk_paned_get_wide_handle org.gtk.Property.set=gtk_paned_set_wide_handle)
   *
   * Whether the `GtkPaned` should provide a stronger visual separation.
   *
   * For example, this could be set when a paned contains two
   * [class@Gtk.Notebook]s, whose tab rows would otherwise merge visually.
   */
  paned_props[PROP_WIDE_HANDLE] =
    g_param_spec_boolean ("wide-handle", NULL, NULL,
                          FALSE,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkPaned:resize-start-child: (attributes org.gtk.Property.get=gtk_paned_get_resize_start_child org.gtk.Property.set=gtk_paned_set_resize_start_child)
   *
   * Determines whether the first child expands and shrinks
   * along with the paned widget.
   */
  paned_props[PROP_RESIZE_START_CHILD] =
    g_param_spec_boolean ("resize-start-child", NULL, NULL,
                          TRUE,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkPaned:resize-end-child: (attributes org.gtk.Property.get=gtk_paned_get_resize_end_child org.gtk.Property.set=gtk_paned_set_resize_end_child)
   *
   * Determines whether the second child expands and shrinks
   * along with the paned widget.
   */
  paned_props[PROP_RESIZE_END_CHILD] =
    g_param_spec_boolean ("resize-end-child", NULL, NULL,
                          TRUE,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkPaned:shrink-start-child: (attributes org.gtk.Property.get=gtk_paned_get_shrink_start_child org.gtk.Property.set=gtk_paned_set_shrink_start_child)
   *
   * Determines whether the first child can be made smaller
   * than its requisition.
   */
  paned_props[PROP_SHRINK_START_CHILD] =
    g_param_spec_boolean ("shrink-start-child", NULL, NULL,
                          TRUE,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkPaned:shrink-end-child: (attributes org.gtk.Property.get=gtk_paned_get_shrink_end_child org.gtk.Property.set=gtk_paned_set_shrink_end_child)
   *
   * Determines whether the second child can be made smaller
   * than its requisition.
   */
  paned_props[PROP_SHRINK_END_CHILD] =
    g_param_spec_boolean ("shrink-end-child", NULL, NULL,
                          TRUE,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkPaned:start-child: (attributes org.gtk.Property.get=gtk_paned_get_start_child org.gtk.Property.set=gtk_paned_set_start_child)
   *
   * The first child.
   */
  paned_props[PROP_START_CHILD] =
    g_param_spec_object ("start-child", NULL, NULL,
                          GTK_TYPE_WIDGET,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkPaned:end-child: (attributes org.gtk.Property.get=gtk_paned_get_end_child org.gtk.Property.set=gtk_paned_set_end_child)
   *
   * The second child.
   */
  paned_props[PROP_END_CHILD] =
    g_param_spec_object ("end-child", NULL, NULL,
                          GTK_TYPE_WIDGET,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, paned_props);
  g_object_class_override_property (object_class, PROP_ORIENTATION, "orientation");

  /**
   * GtkPaned::cycle-child-focus:
   * @widget: the object that received the signal
   * @reversed: whether cycling backward or forward
   *
   * Emitted to cycle the focus between the children of the paned.
   *
   * This is a [keybinding signal](class.SignalAction.html).
   *
   * The default binding is <kbd>F6</kbd>.
   */
  signals [CYCLE_CHILD_FOCUS] =
    g_signal_new (I_("cycle-child-focus"),
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkPanedClass, cycle_child_focus),
                  NULL, NULL,
                  _gtk_marshal_BOOLEAN__BOOLEAN,
                  G_TYPE_BOOLEAN, 1,
                  G_TYPE_BOOLEAN);
  g_signal_set_va_marshaller (signals[CYCLE_CHILD_FOCUS],
                              G_TYPE_FROM_CLASS (object_class),
                              _gtk_marshal_BOOLEAN__BOOLEANv);

  /**
   * GtkPaned::toggle-handle-focus:
   * @widget: the object that received the signal
   *
   * Emitted to accept the current position of the handle and then
   * move focus to the next widget in the focus chain.
   *
   * This is a [keybinding signal](class.SignalAction.html).
   *
   * The default binding is <kbd>Tab</kbd>.
   */
  signals [TOGGLE_HANDLE_FOCUS] =
    g_signal_new (I_("toggle-handle-focus"),
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkPanedClass, toggle_handle_focus),
                  NULL, NULL,
                  _gtk_marshal_BOOLEAN__VOID,
                  G_TYPE_BOOLEAN, 0);
  g_signal_set_va_marshaller (signals[TOGGLE_HANDLE_FOCUS],
                              G_TYPE_FROM_CLASS (object_class),
                              _gtk_marshal_BOOLEAN__VOIDv);

  /**
   * GtkPaned::move-handle:
   * @widget: the object that received the signal
   * @scroll_type: a `GtkScrollType`
   *
   * Emitted to move the handle with key bindings.
   *
   * This is a [keybinding signal](class.SignalAction.html).
   */
  signals[MOVE_HANDLE] =
    g_signal_new (I_("move-handle"),
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkPanedClass, move_handle),
                  NULL, NULL,
                  _gtk_marshal_BOOLEAN__ENUM,
                  G_TYPE_BOOLEAN, 1,
                  GTK_TYPE_SCROLL_TYPE);
  g_signal_set_va_marshaller (signals[MOVE_HANDLE],
                              G_TYPE_FROM_CLASS (object_class),
                              _gtk_marshal_BOOLEAN__ENUMv);

  /**
   * GtkPaned::cycle-handle-focus:
   * @widget: the object that received the signal
   * @reversed: whether cycling backward or forward
   *
   * Emitted to cycle whether the paned should grab focus to allow
   * the user to change position of the handle by using key bindings.
   *
   * This is a [keybinding signal](class.SignalAction.html).
   *
   * The default binding for this signal is <kbd>F8</kbd>.
   */
  signals [CYCLE_HANDLE_FOCUS] =
    g_signal_new (I_("cycle-handle-focus"),
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkPanedClass, cycle_handle_focus),
                  NULL, NULL,
                  _gtk_marshal_BOOLEAN__BOOLEAN,
                  G_TYPE_BOOLEAN, 1,
                  G_TYPE_BOOLEAN);
  g_signal_set_va_marshaller (signals[CYCLE_HANDLE_FOCUS],
                              G_TYPE_FROM_CLASS (object_class),
                              _gtk_marshal_BOOLEAN__BOOLEANv);

  /**
   * GtkPaned::accept-position:
   * @widget: the object that received the signal
   *
   * Emitted to accept the current position of the handle when
   * moving it using key bindings.
   *
   * This is a [keybinding signal](class.SignalAction.html).
   *
   * The default binding for this signal is <kbd>Return</kbd> or
   * <kbd>Space</kbd>.
   */
  signals [ACCEPT_POSITION] =
    g_signal_new (I_("accept-position"),
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkPanedClass, accept_position),
                  NULL, NULL,
                  _gtk_marshal_BOOLEAN__VOID,
                  G_TYPE_BOOLEAN, 0);
  g_signal_set_va_marshaller (signals[ACCEPT_POSITION],
                              G_TYPE_FROM_CLASS (object_class),
                              _gtk_marshal_BOOLEAN__VOIDv);

  /**
   * GtkPaned::cancel-position:
   * @widget: the object that received the signal
   *
   * Emitted to cancel moving the position of the handle using key
   * bindings.
   *
   * The position of the handle will be reset to the value prior to
   * moving it.
   *
   * This is a [keybinding signal](class.SignalAction.html).
   *
   * The default binding for this signal is <kbd>Escape</kbd>.
   */
  signals [CANCEL_POSITION] =
    g_signal_new (I_("cancel-position"),
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkPanedClass, cancel_position),
                  NULL, NULL,
                  _gtk_marshal_BOOLEAN__VOID,
                  G_TYPE_BOOLEAN, 0);
  g_signal_set_va_marshaller (signals[CANCEL_POSITION],
                              G_TYPE_FROM_CLASS (object_class),
                              _gtk_marshal_BOOLEAN__VOIDv);

  /* F6 and friends */
  gtk_widget_class_add_binding_signal (widget_class,
                                       GDK_KEY_F6, 0,
                                       "cycle-child-focus",
                                       "(b)", FALSE);
  gtk_widget_class_add_binding_signal (widget_class,
                                       GDK_KEY_F6, GDK_SHIFT_MASK,
                                       "cycle-child-focus",
                                       "(b)", TRUE);

  /* F8 and friends */
  gtk_widget_class_add_binding_signal (widget_class,
                                       GDK_KEY_F8, 0,
                                       "cycle-handle-focus",
                                       "(b)", FALSE);
  gtk_widget_class_add_binding_signal (widget_class,
                                       GDK_KEY_F8, GDK_SHIFT_MASK,
                                       "cycle-handle-focus",
                                       "(b)", TRUE);

  add_tab_bindings (widget_class, 0);
  add_tab_bindings (widget_class, GDK_CONTROL_MASK);
  add_tab_bindings (widget_class, GDK_SHIFT_MASK);
  add_tab_bindings (widget_class, GDK_CONTROL_MASK | GDK_SHIFT_MASK);

  /* accept and cancel positions */
  gtk_widget_class_add_binding_signal (widget_class,
                                       GDK_KEY_Escape, 0,
                                       "cancel-position",
                                       NULL);

  gtk_widget_class_add_binding_signal (widget_class,
                                       GDK_KEY_Return, 0,
                                       "accept-position",
                                       NULL);
  gtk_widget_class_add_binding_signal (widget_class,
                                       GDK_KEY_ISO_Enter, 0,
                                       "accept-position",
                                       NULL);
  gtk_widget_class_add_binding_signal (widget_class,
                                       GDK_KEY_KP_Enter, 0,
                                       "accept-position",
                                       NULL);
  gtk_widget_class_add_binding_signal (widget_class,
                                       GDK_KEY_space, 0,
                                       "accept-position",
                                       NULL);
  gtk_widget_class_add_binding_signal (widget_class,
                                       GDK_KEY_KP_Space, 0,
                                       "accept-position",
                                       NULL);

  /* move handle */
  add_move_binding (widget_class, GDK_KEY_Left, 0, GTK_SCROLL_STEP_LEFT);
  add_move_binding (widget_class, GDK_KEY_KP_Left, 0, GTK_SCROLL_STEP_LEFT);
  add_move_binding (widget_class, GDK_KEY_Left, GDK_CONTROL_MASK, GTK_SCROLL_PAGE_LEFT);
  add_move_binding (widget_class, GDK_KEY_KP_Left, GDK_CONTROL_MASK, GTK_SCROLL_PAGE_LEFT);

  add_move_binding (widget_class, GDK_KEY_Right, 0, GTK_SCROLL_STEP_RIGHT);
  add_move_binding (widget_class, GDK_KEY_Right, GDK_CONTROL_MASK, GTK_SCROLL_PAGE_RIGHT);
  add_move_binding (widget_class, GDK_KEY_KP_Right, 0, GTK_SCROLL_STEP_RIGHT);
  add_move_binding (widget_class, GDK_KEY_KP_Right, GDK_CONTROL_MASK, GTK_SCROLL_PAGE_RIGHT);

  add_move_binding (widget_class, GDK_KEY_Up, 0, GTK_SCROLL_STEP_UP);
  add_move_binding (widget_class, GDK_KEY_Up, GDK_CONTROL_MASK, GTK_SCROLL_PAGE_UP);
  add_move_binding (widget_class, GDK_KEY_KP_Up, 0, GTK_SCROLL_STEP_UP);
  add_move_binding (widget_class, GDK_KEY_KP_Up, GDK_CONTROL_MASK, GTK_SCROLL_PAGE_UP);
  add_move_binding (widget_class, GDK_KEY_Page_Up, 0, GTK_SCROLL_PAGE_UP);
  add_move_binding (widget_class, GDK_KEY_KP_Page_Up, 0, GTK_SCROLL_PAGE_UP);

  add_move_binding (widget_class, GDK_KEY_Down, 0, GTK_SCROLL_STEP_DOWN);
  add_move_binding (widget_class, GDK_KEY_Down, GDK_CONTROL_MASK, GTK_SCROLL_PAGE_DOWN);
  add_move_binding (widget_class, GDK_KEY_KP_Down, 0, GTK_SCROLL_STEP_DOWN);
  add_move_binding (widget_class, GDK_KEY_KP_Down, GDK_CONTROL_MASK, GTK_SCROLL_PAGE_DOWN);
  add_move_binding (widget_class, GDK_KEY_Page_Down, 0, GTK_SCROLL_PAGE_RIGHT);
  add_move_binding (widget_class, GDK_KEY_KP_Page_Down, 0, GTK_SCROLL_PAGE_RIGHT);

  add_move_binding (widget_class, GDK_KEY_Home, 0, GTK_SCROLL_START);
  add_move_binding (widget_class, GDK_KEY_KP_Home, 0, GTK_SCROLL_START);
  add_move_binding (widget_class, GDK_KEY_End, 0, GTK_SCROLL_END);
  add_move_binding (widget_class, GDK_KEY_KP_End, 0, GTK_SCROLL_END);

  gtk_widget_class_set_css_name (widget_class, I_("paned"));
}

static GtkBuildableIface *parent_buildable_iface;

static void
gtk_paned_buildable_add_child (GtkBuildable *buildable,
                               GtkBuilder   *builder,
                               GObject      *child,
                               const char   *type)
{
  GtkPaned *self = GTK_PANED (buildable);

  if (g_strcmp0 (type, "start") == 0)
    {
      gtk_paned_set_start_child (self, GTK_WIDGET (child));
      gtk_paned_set_resize_start_child (self, FALSE);
      gtk_paned_set_shrink_start_child (self, TRUE);
    }
  else if (g_strcmp0 (type, "end") == 0)
    {
      gtk_paned_set_end_child (self, GTK_WIDGET (child));
      gtk_paned_set_resize_end_child (self, TRUE);
      gtk_paned_set_shrink_end_child (self, TRUE);
    }
  else if (type == NULL && GTK_IS_WIDGET (child))
    {
      if (self->start_child == NULL)
        {
          gtk_paned_set_start_child (self, GTK_WIDGET (child));
          gtk_paned_set_resize_start_child (self, FALSE);
          gtk_paned_set_shrink_start_child (self, TRUE);
        }
      else if (self->end_child == NULL)
        {
          gtk_paned_set_end_child (self, GTK_WIDGET (child));
          gtk_paned_set_resize_end_child (self, TRUE);
          gtk_paned_set_shrink_end_child (self, TRUE);
        }
      else
        g_warning ("GtkPaned only accepts two widgets as children");
    }
  else
    parent_buildable_iface->add_child (buildable, builder, child, type);
}

static void
gtk_paned_buildable_iface_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);
  iface->add_child = gtk_paned_buildable_add_child;
}

static gboolean
accessible_range_set_current_value (GtkAccessibleRange *accessible_range,
                                    double              value)
{
  gtk_paned_set_position (GTK_PANED (accessible_range), (int) value + 0.5);
  return TRUE;
}

static void
gtk_paned_accessible_range_init (GtkAccessibleRangeInterface *iface)
{
  iface->set_current_value = accessible_range_set_current_value;
}

static gboolean
initiates_touch_drag (GtkPaned *paned,
                      double    start_x,
                      double    start_y)
{
  int handle_size, handle_pos, drag_pos;
  graphene_rect_t handle_area;

#define TOUCH_EXTRA_AREA_WIDTH 50
  get_handle_area (paned, &handle_area);

  if (paned->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      handle_pos = handle_area.origin.x;
      drag_pos = start_x;
      handle_size = handle_area.size.width;
    }
  else
    {
      handle_pos = handle_area.origin.y;
      drag_pos = start_y;
      handle_size = handle_area.size.height;
    }

  if (drag_pos < handle_pos - TOUCH_EXTRA_AREA_WIDTH ||
      drag_pos > handle_pos + handle_size + TOUCH_EXTRA_AREA_WIDTH)
    return FALSE;

#undef TOUCH_EXTRA_AREA_WIDTH

  return TRUE;
}

static void
gesture_drag_begin_cb (GtkGestureDrag *gesture,
                       double          start_x,
                       double          start_y,
                       GtkPaned       *paned)
{
  GdkEventSequence *sequence;
  graphene_rect_t handle_area;
  GdkEvent *event;
  GdkDevice *device;
  gboolean is_touch;

  /* Only drag the handle when it's visible */
  if (!gtk_widget_get_child_visible (paned->handle_widget))
    {
      gtk_gesture_set_state (GTK_GESTURE (gesture),
                             GTK_EVENT_SEQUENCE_DENIED);
      return;
    }

  sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));
  event = gtk_gesture_get_last_event (GTK_GESTURE (gesture), sequence);
  device = gdk_event_get_device (event);
  paned->panning = FALSE;

  is_touch = (gdk_event_get_event_type (event) == GDK_TOUCH_BEGIN ||
              gdk_device_get_source (device) == GDK_SOURCE_TOUCHSCREEN);

  get_handle_area (paned, &handle_area);

  if ((is_touch && GTK_GESTURE (gesture) == paned->drag_gesture) ||
      (!is_touch && GTK_GESTURE (gesture) == paned->pan_gesture))
    {
      gtk_gesture_set_state (GTK_GESTURE (gesture),
                             GTK_EVENT_SEQUENCE_DENIED);
      return;
    }

  if (graphene_rect_contains_point (&handle_area, &(graphene_point_t){start_x, start_y}) ||
      (is_touch && initiates_touch_drag (paned, start_x, start_y)))
    {
      if (paned->orientation == GTK_ORIENTATION_HORIZONTAL)
        paned->drag_pos = start_x - handle_area.origin.x;
      else
        paned->drag_pos = start_y - handle_area.origin.y;

      paned->panning = TRUE;

      gtk_gesture_set_state (GTK_GESTURE (gesture),
                             GTK_EVENT_SEQUENCE_CLAIMED);
    }
  else
    {
      gtk_gesture_set_state (GTK_GESTURE (gesture),
                             GTK_EVENT_SEQUENCE_DENIED);
    }
}

static void
gesture_drag_update_cb (GtkGestureDrag   *gesture,
                        double            offset_x,
                        double            offset_y,
                        GtkPaned         *paned)
{
  double start_x, start_y;

  gtk_gesture_drag_get_start_point (GTK_GESTURE_DRAG (gesture),
                               &start_x, &start_y);
  update_drag (paned, start_x + offset_x, start_y + offset_y);
}

static void
gesture_drag_end_cb (GtkGestureDrag *gesture,
                     double          offset_x,
                     double          offset_y,
                     GtkPaned       *paned)
{
  if (!paned->panning)
    gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_DENIED);

  paned->panning = FALSE;
}

static void
gtk_paned_set_property (GObject        *object,
                        guint           prop_id,
                        const GValue   *value,
                        GParamSpec     *pspec)
{
  GtkPaned *paned = GTK_PANED (object);

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      gtk_paned_set_orientation (paned, g_value_get_enum (value));
      break;
    case PROP_POSITION:
      gtk_paned_set_position (paned, g_value_get_int (value));
      break;
    case PROP_POSITION_SET:
      if (paned->position_set != g_value_get_boolean (value))
        {
          paned->position_set = g_value_get_boolean (value);
          gtk_widget_queue_resize (GTK_WIDGET (paned));
          g_object_notify_by_pspec (object, pspec);
        }
      break;
    case PROP_WIDE_HANDLE:
      gtk_paned_set_wide_handle (paned, g_value_get_boolean (value));
      break;
    case PROP_RESIZE_START_CHILD:
      gtk_paned_set_resize_start_child (paned, g_value_get_boolean (value));
      break;
    case PROP_RESIZE_END_CHILD:
      gtk_paned_set_resize_end_child (paned, g_value_get_boolean (value));
      break;
    case PROP_SHRINK_START_CHILD:
      gtk_paned_set_shrink_start_child (paned, g_value_get_boolean (value));
      break;
    case PROP_SHRINK_END_CHILD:
      gtk_paned_set_shrink_end_child (paned, g_value_get_boolean (value));
      break;
    case PROP_START_CHILD:
      gtk_paned_set_start_child (paned, g_value_get_object (value));
      break;
    case PROP_END_CHILD:
      gtk_paned_set_end_child (paned, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_paned_get_property (GObject        *object,
                        guint           prop_id,
                        GValue         *value,
                        GParamSpec     *pspec)
{
  GtkPaned *paned = GTK_PANED (object);

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      g_value_set_enum (value, paned->orientation);
      break;
    case PROP_POSITION:
      g_value_set_int (value, paned->start_child_size);
      break;
    case PROP_POSITION_SET:
      g_value_set_boolean (value, paned->position_set);
      break;
    case PROP_MIN_POSITION:
      g_value_set_int (value, paned->min_position);
      break;
    case PROP_MAX_POSITION:
      g_value_set_int (value, paned->max_position);
      break;
    case PROP_WIDE_HANDLE:
      g_value_set_boolean (value, gtk_paned_get_wide_handle (paned));
      break;
    case PROP_RESIZE_START_CHILD:
      g_value_set_boolean (value, paned->resize_start_child);
      break;
    case PROP_RESIZE_END_CHILD:
      g_value_set_boolean (value, paned->resize_end_child);
      break;
    case PROP_SHRINK_START_CHILD:
      g_value_set_boolean (value, paned->shrink_start_child);
      break;
    case PROP_SHRINK_END_CHILD:
      g_value_set_boolean (value, paned->shrink_end_child);
      break;
    case PROP_START_CHILD:
      g_value_set_object (value, gtk_paned_get_start_child (paned));
      break;
    case PROP_END_CHILD:
      g_value_set_object (value, gtk_paned_get_end_child (paned));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_paned_dispose (GObject *object)
{
  GtkPaned *paned = GTK_PANED (object);

  gtk_paned_set_saved_focus (paned, NULL);
  gtk_paned_set_first_paned (paned, NULL);

  g_clear_pointer (&paned->start_child, gtk_widget_unparent);
  g_clear_pointer (&paned->end_child, gtk_widget_unparent);
  g_clear_pointer (&paned->handle_widget, gtk_widget_unparent);

  G_OBJECT_CLASS (gtk_paned_parent_class)->dispose (object);
}

static void
gtk_paned_compute_position (GtkPaned *paned,
                            int       allocation,
                            int       start_child_req,
                            int       end_child_req,
                            int      *min_pos,
                            int      *max_pos,
                            int      *out_pos)
{
  int min, max, pos;

  min = paned->shrink_start_child ? 0 : start_child_req;

  max = allocation;
  if (!paned->shrink_end_child)
    max = MAX (1, max - end_child_req);
  max = MAX (min, max);

  if (!paned->position_set)
    {
      if (paned->resize_start_child && !paned->resize_end_child)
        pos = MAX (0, allocation - end_child_req);
      else if (!paned->resize_start_child && paned->resize_end_child)
        pos = start_child_req;
      else if (start_child_req + end_child_req != 0)
        pos = allocation * ((double)start_child_req / (start_child_req + end_child_req)) + 0.5;
      else
        pos = allocation * 0.5 + 0.5;
    }
  else
    {
      /* If the position was set before the initial allocation.
       * (paned->last_allocation <= 0) just clamp it and leave it.
       */
      if (paned->last_allocation > 0)
        {
          if (paned->resize_start_child && !paned->resize_end_child)
            pos = paned->start_child_size + allocation - paned->last_allocation;
          else if (!(!paned->resize_start_child && paned->resize_end_child))
            pos = allocation * ((double) paned->start_child_size / (paned->last_allocation)) + 0.5;
          else
            pos = paned->start_child_size;
        }
      else
        pos = paned->start_child_size;
    }

  pos = CLAMP (pos, min, max);

  if (min_pos)
    *min_pos = min;
  if (max_pos)
    *max_pos = max;
  if (out_pos)
    *out_pos = pos;
}

static void
gtk_paned_get_preferred_size_for_orientation (GtkWidget      *widget,
                                              int             size,
                                              int            *minimum,
                                              int            *natural)
{
  GtkPaned *paned = GTK_PANED (widget);
  int child_min, child_nat;

  *minimum = *natural = 0;

  if (paned->start_child && gtk_widget_get_visible (paned->start_child))
    {
      gtk_widget_measure (paned->start_child, paned->orientation, size, &child_min, &child_nat, NULL, NULL);
      if (paned->shrink_start_child)
        *minimum = 0;
      else
        *minimum = child_min;
      *natural = child_nat;
    }

  if (paned->end_child && gtk_widget_get_visible (paned->end_child))
    {
      gtk_widget_measure (paned->end_child, paned->orientation, size, &child_min, &child_nat, NULL, NULL);

      if (!paned->shrink_end_child)
        *minimum += child_min;
      *natural += child_nat;
    }

  if (paned->start_child && gtk_widget_get_visible (paned->start_child) &&
      paned->end_child && gtk_widget_get_visible (paned->end_child))
    {
      int handle_size;

      gtk_widget_measure (paned->handle_widget,
                          paned->orientation,
                          -1,
                          NULL, &handle_size,
                          NULL, NULL);

      *minimum += handle_size;
      *natural += handle_size;
    }
}

static void
gtk_paned_get_preferred_size_for_opposite_orientation (GtkWidget      *widget,
                                                       int             size,
                                                       int            *minimum,
                                                       int            *natural)
{
  GtkPaned *paned = GTK_PANED (widget);
  int for_start_child, for_end_child, for_handle;
  int child_min, child_nat;

  if (size > -1 &&
      paned->start_child && gtk_widget_get_visible (paned->start_child) &&
      paned->end_child && gtk_widget_get_visible (paned->end_child))
    {
      int start_child_req, end_child_req;

      gtk_widget_measure (paned->handle_widget,
                          paned->orientation,
                          -1,
                          NULL, &for_handle,
                          NULL, NULL);

      gtk_widget_measure (paned->start_child, paned->orientation, -1, &start_child_req, NULL, NULL, NULL);
      gtk_widget_measure (paned->end_child, paned->orientation, -1, &end_child_req, NULL, NULL, NULL);

      gtk_paned_compute_position (paned,
                                  size - for_handle, start_child_req, end_child_req,
                                  NULL, NULL, &for_start_child);

      for_end_child = size - for_start_child - for_handle;

      if (paned->shrink_start_child)
        for_start_child = MAX (start_child_req, for_start_child);
      if (paned->shrink_end_child)
        for_end_child = MAX (end_child_req, for_end_child);
    }
  else
    {
      for_start_child = size;
      for_end_child = size;
      for_handle = -1;
    }

  *minimum = *natural = 0;

  if (paned->start_child && gtk_widget_get_visible (paned->start_child))
    {
      gtk_widget_measure (paned->start_child,
                          OPPOSITE_ORIENTATION (paned->orientation),
                          for_start_child,
                          &child_min, &child_nat,
                          NULL, NULL);

      *minimum = child_min;
      *natural = child_nat;
    }

  if (paned->end_child && gtk_widget_get_visible (paned->end_child))
    {
      gtk_widget_measure (paned->end_child,
                          OPPOSITE_ORIENTATION (paned->orientation),
                          for_end_child,
                          &child_min, &child_nat,
                          NULL, NULL);

      *minimum = MAX (*minimum, child_min);
      *natural = MAX (*natural, child_nat);
    }

  if (paned->start_child && gtk_widget_get_visible (paned->start_child) &&
      paned->end_child && gtk_widget_get_visible (paned->end_child))
    {
      gtk_widget_measure (paned->handle_widget,
                          OPPOSITE_ORIENTATION (paned->orientation),
                          for_handle,
                          &child_min, &child_nat,
                          NULL, NULL);

      *minimum = MAX (*minimum, child_min);
      *natural = MAX (*natural, child_nat);
    }
}

static void
gtk_paned_measure (GtkWidget *widget,
                   GtkOrientation  orientation,
                   int             for_size,
                   int            *minimum,
                   int            *natural,
                   int            *minimum_baseline,
                   int            *natural_baseline)
{
  GtkPaned *paned = GTK_PANED (widget);

  if (orientation == paned->orientation)
    gtk_paned_get_preferred_size_for_orientation (widget, for_size, minimum, natural);
  else
    gtk_paned_get_preferred_size_for_opposite_orientation (widget, for_size, minimum, natural);
}

static void
flip_child (int            width,
            GtkAllocation *child_pos)
{
  child_pos->x = width - child_pos->x - child_pos->width;
}

static void
gtk_paned_size_allocate (GtkWidget *widget,
                         int        width,
                         int        height,
                         int        baseline)
{
  GtkPaned *paned = GTK_PANED (widget);

  if (paned->start_child && gtk_widget_get_visible (paned->start_child) &&
      paned->end_child && gtk_widget_get_visible (paned->end_child))
    {
      GtkAllocation start_child_allocation;
      GtkAllocation end_child_allocation;
      GtkAllocation handle_allocation;
      int handle_size;

      gtk_widget_measure (paned->handle_widget,
                          paned->orientation,
                          -1,
                          NULL, &handle_size,
                          NULL, NULL);

      if (paned->orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          int start_child_width, end_child_width;

          gtk_widget_measure (paned->start_child, GTK_ORIENTATION_HORIZONTAL,
                              height,
                              &start_child_width, NULL, NULL, NULL);
          gtk_widget_measure (paned->end_child, GTK_ORIENTATION_HORIZONTAL,
                              height,
                              &end_child_width, NULL, NULL, NULL);

          gtk_paned_calc_position (paned,
                                   MAX (1, width - handle_size),
                                   start_child_width,
                                   end_child_width);

          handle_allocation = (GdkRectangle){
            paned->start_child_size,
            0,
            handle_size,
            height
          };

          start_child_allocation.height = end_child_allocation.height = height;
          start_child_allocation.width = MAX (1, paned->start_child_size);
          start_child_allocation.x = 0;
          start_child_allocation.y = end_child_allocation.y = 0;

          end_child_allocation.x = start_child_allocation.x + paned->start_child_size + handle_size;
          end_child_allocation.width = MAX (1, width - paned->start_child_size - handle_size);

          if (gtk_widget_get_direction (GTK_WIDGET (widget)) == GTK_TEXT_DIR_RTL)
            {
              flip_child (width, &(end_child_allocation));
              flip_child (width, &(start_child_allocation));
              flip_child (width, &(handle_allocation));
            }

          if (start_child_width > start_child_allocation.width)
            {
              if (gtk_widget_get_direction (GTK_WIDGET (widget)) == GTK_TEXT_DIR_LTR)
                start_child_allocation.x -= start_child_width - start_child_allocation.width;
              start_child_allocation.width = start_child_width;
            }

          if (end_child_width > end_child_allocation.width)
            {
              if (gtk_widget_get_direction (GTK_WIDGET (widget)) == GTK_TEXT_DIR_RTL)
                end_child_allocation.x -= end_child_width - end_child_allocation.width;
              end_child_allocation.width = end_child_width;
            }
        }
      else
        {
          int start_child_height, end_child_height;

          gtk_widget_measure (paned->start_child, GTK_ORIENTATION_VERTICAL,
                              width,
                              &start_child_height, NULL, NULL, NULL);
          gtk_widget_measure (paned->end_child, GTK_ORIENTATION_VERTICAL,
                              width,
                              &end_child_height, NULL, NULL, NULL);

          gtk_paned_calc_position (paned,
                                   MAX (1, height - handle_size),
                                   start_child_height,
                                   end_child_height);

          handle_allocation = (GdkRectangle){
            0,
            paned->start_child_size,
            width,
            handle_size,
          };

          start_child_allocation.width = end_child_allocation.width = width;
          start_child_allocation.height = MAX (1, paned->start_child_size);
          start_child_allocation.x = end_child_allocation.x = 0;
          start_child_allocation.y = 0;

          end_child_allocation.y = start_child_allocation.y + paned->start_child_size + handle_size;
          end_child_allocation.height = MAX (1, height - end_child_allocation.y);

          if (start_child_height > start_child_allocation.height)
            {
              start_child_allocation.y -= start_child_height - start_child_allocation.height;
              start_child_allocation.height = start_child_height;
            }

          if (end_child_height > end_child_allocation.height)
            end_child_allocation.height = end_child_height;
        }

      gtk_widget_set_child_visible (paned->handle_widget, TRUE);

      gtk_widget_size_allocate (paned->handle_widget, &handle_allocation, -1);
      gtk_widget_size_allocate (paned->start_child, &start_child_allocation, -1);
      gtk_widget_size_allocate (paned->end_child, &end_child_allocation, -1);
    }
  else
    {
      if (paned->start_child && gtk_widget_get_visible (paned->start_child))
        {
          gtk_widget_set_child_visible (paned->start_child, TRUE);

          gtk_widget_size_allocate (paned->start_child,
                                    &(GtkAllocation) {0, 0, width, height}, -1);
        }
      else if (paned->end_child && gtk_widget_get_visible (paned->end_child))
        {
          gtk_widget_set_child_visible (paned->end_child, TRUE);

          gtk_widget_size_allocate (paned->end_child,
                                    &(GtkAllocation) {0, 0, width, height}, -1);

        }

      gtk_widget_set_child_visible (paned->handle_widget, FALSE);
    }

  gtk_accessible_update_property (GTK_ACCESSIBLE (paned),
                                  GTK_ACCESSIBLE_PROPERTY_VALUE_MIN, 0.0,
                                  GTK_ACCESSIBLE_PROPERTY_VALUE_MAX,
                                      (double) (paned->orientation == GTK_ORIENTATION_HORIZONTAL ?  width : height),
                                  GTK_ACCESSIBLE_PROPERTY_VALUE_NOW,
                                      (double) paned->start_child_size,
                                  -1);
}


static void
gtk_paned_unrealize (GtkWidget *widget)
{
  GtkPaned *paned = GTK_PANED (widget);

  gtk_paned_set_last_start_child_focus (paned, NULL);
  gtk_paned_set_last_end_child_focus (paned, NULL);
  gtk_paned_set_saved_focus (paned, NULL);
  gtk_paned_set_first_paned (paned, NULL);

  GTK_WIDGET_CLASS (gtk_paned_parent_class)->unrealize (widget);
}

static void
connect_drag_gesture_signals (GtkPaned   *paned,
                              GtkGesture *gesture)
{
  g_signal_connect (gesture, "drag-begin",
                    G_CALLBACK (gesture_drag_begin_cb), paned);
  g_signal_connect (gesture, "drag-update",
                    G_CALLBACK (gesture_drag_update_cb), paned);
  g_signal_connect (gesture, "drag-end",
                    G_CALLBACK (gesture_drag_end_cb), paned);
}

static void
gtk_paned_init (GtkPaned *paned)
{
  GtkGesture *gesture;

  gtk_widget_set_focusable (GTK_WIDGET (paned), TRUE);
  gtk_widget_set_overflow (GTK_WIDGET (paned), GTK_OVERFLOW_HIDDEN);

  paned->orientation = GTK_ORIENTATION_HORIZONTAL;

  paned->start_child = NULL;
  paned->end_child = NULL;

  paned->position_set = FALSE;
  paned->last_allocation = -1;

  paned->last_start_child_focus = NULL;
  paned->last_end_child_focus = NULL;
  paned->in_recursion = FALSE;
  paned->original_position = -1;
  paned->max_position = G_MAXINT;
  paned->resize_start_child = TRUE;
  paned->resize_end_child = TRUE;
  paned->shrink_start_child = TRUE;
  paned->shrink_end_child = TRUE;

  gtk_widget_update_orientation (GTK_WIDGET (paned), paned->orientation);

  /* Touch gesture */
  gesture = gtk_gesture_pan_new (GTK_ORIENTATION_HORIZONTAL);
  connect_drag_gesture_signals (paned, gesture);
  gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (gesture), TRUE);
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (gesture),
                                              GTK_PHASE_CAPTURE);
  gtk_widget_add_controller (GTK_WIDGET (paned), GTK_EVENT_CONTROLLER (gesture));
  paned->pan_gesture = gesture;

  /* Pointer gesture */
  gesture = gtk_gesture_drag_new ();
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (gesture),
                                              GTK_PHASE_CAPTURE);
  connect_drag_gesture_signals (paned, gesture);
  gtk_widget_add_controller (GTK_WIDGET (paned), GTK_EVENT_CONTROLLER (gesture));
  paned->drag_gesture = gesture;

  paned->handle_widget = gtk_paned_handle_new ();
  gtk_widget_set_parent (paned->handle_widget, GTK_WIDGET (paned));
  gtk_widget_set_cursor_from_name (paned->handle_widget, "col-resize");
}

static gboolean
is_rtl (GtkPaned *paned)
{
  return paned->orientation == GTK_ORIENTATION_HORIZONTAL &&
         gtk_widget_get_direction (GTK_WIDGET (paned)) == GTK_TEXT_DIR_RTL;
}

static void
update_drag (GtkPaned *paned,
             int       xpos,
             int       ypos)
{
  int pos;
  int handle_size;
  int size;

  if (paned->orientation == GTK_ORIENTATION_HORIZONTAL)
    pos = xpos;
  else
    pos = ypos;

  pos -= paned->drag_pos;

  if (is_rtl (paned))
    {
      gtk_widget_measure (paned->handle_widget,
                          GTK_ORIENTATION_HORIZONTAL,
                          -1,
                          NULL, &handle_size,
                          NULL, NULL);

      size = gtk_widget_get_width (GTK_WIDGET (paned)) - pos - handle_size;
    }
  else
    {
      size = pos;
    }

  size = CLAMP (size, paned->min_position, paned->max_position);

  if (size != paned->start_child_size)
    gtk_paned_set_position (paned, size);
}

static void
gtk_paned_css_changed (GtkWidget         *widget,
                       GtkCssStyleChange *change)
{
  GTK_WIDGET_CLASS (gtk_paned_parent_class)->css_changed (widget, change);

  if (change == NULL ||
      gtk_css_style_change_affects (change, GTK_CSS_AFFECTS_ICON_SIZE))
    {
      gtk_widget_queue_resize (widget);
    }
  else if (gtk_css_style_change_affects (change, GTK_CSS_AFFECTS_ICON_TEXTURE |
                                                 GTK_CSS_AFFECTS_ICON_REDRAW))
    {
      gtk_widget_queue_draw (widget);
    }
}

/**
 * gtk_paned_new:
 * @orientation: the paned’s orientation.
 *
 * Creates a new `GtkPaned` widget.
 *
 * Returns: the newly created paned widget
 */
GtkWidget *
gtk_paned_new (GtkOrientation orientation)
{
  return g_object_new (GTK_TYPE_PANED,
                       "orientation", orientation,
                       NULL);
}

/**
 * gtk_paned_set_start_child: (attributes org.gtk.Method.set_property=start-child)
 * @paned: a `GtkPaned`
 * @child: (nullable): the widget to add
 *
 * Sets the start child of @paned to @child.
 *
 * If @child is `NULL`, the existing child will be removed.
 */
void
gtk_paned_set_start_child (GtkPaned  *paned,
                           GtkWidget *child)
{
  g_return_if_fail (GTK_IS_PANED (paned));
  g_return_if_fail (child == NULL || paned->start_child == child || gtk_widget_get_parent (child) == NULL);

  if (paned->start_child == child)
    return;

  g_clear_pointer (&paned->start_child, gtk_widget_unparent);

  if (child)
    {
      paned->start_child = child;
      gtk_widget_insert_before (child, GTK_WIDGET (paned), paned->handle_widget);
    }

  g_object_notify (G_OBJECT (paned), "start-child");
}

/**
 * gtk_paned_get_start_child: (attributes org.gtk.Method.get_property=start-child)
 * @paned: a `GtkPaned`
 *
 * Retrieves the start child of the given `GtkPaned`.
 *
 * Returns: (transfer none) (nullable): the start child widget
 */
GtkWidget *
gtk_paned_get_start_child (GtkPaned *paned)
{
  g_return_val_if_fail (GTK_IS_PANED (paned), NULL);

  return paned->start_child;
}

/**
 * gtk_paned_set_resize_start_child: (attributes org.gtk.Method.set_property=resize-start-child)
 * @paned: a `GtkPaned`
 * @resize: true to let the start child be resized
 *
 * Sets whether the [property@Gtk.Paned:start-child] can be resized.
 */
void
gtk_paned_set_resize_start_child (GtkPaned *paned,
                                  gboolean  resize)
{
  g_return_if_fail (GTK_IS_PANED (paned));

  if (paned->resize_start_child == resize)
    return;

  paned->resize_start_child = resize;

  g_object_notify (G_OBJECT (paned), "resize-start-child");
}

/**
 * gtk_paned_get_resize_start_child: (attributes org.gtk.Method.get_property=resize-start-child)
 * @paned: a `GtkPaned`
 *
 * Returns whether the [property@Gtk.Paned:start-child] can be resized.
 *
 * Returns: true if the start child is resizable
 */
gboolean
gtk_paned_get_resize_start_child (GtkPaned *paned)
{
  g_return_val_if_fail (GTK_IS_PANED (paned), FALSE);

  return paned->resize_start_child;
}

/**
 * gtk_paned_set_shrink_start_child: (attributes org.gtk.Method.set_property=shrink-start-child)
 * @paned: a `GtkPaned`
 * @resize: true to let the start child be shrunk
 *
 * Sets whether the [property@Gtk.Paned:start-child] can shrink.
 */
void
gtk_paned_set_shrink_start_child (GtkPaned *paned,
                                  gboolean  shrink)
{
  g_return_if_fail (GTK_IS_PANED (paned));

  if (paned->shrink_start_child == shrink)
    return;

  paned->shrink_start_child = shrink;

  g_object_notify (G_OBJECT (paned), "shrink-start-child");
}

/**
 * gtk_paned_get_shrink_start_child: (attributes org.gtk.Method.get_property=shrink-start-child)
 * @paned: a `GtkPaned`
 *
 * Returns whether the [property@Gtk.Paned:start-child] can shrink.
 *
 * Returns: true if the start child is shrinkable
 */
gboolean
gtk_paned_get_shrink_start_child (GtkPaned *paned)
{
  g_return_val_if_fail (GTK_IS_PANED (paned), FALSE);

  return paned->shrink_start_child;
}

/**
 * gtk_paned_set_end_child: (attributes org.gtk.Method.set_property=end-child)
 * @paned: a `GtkPaned`
 * @child: (nullable): the widget to add
 *
 * Sets the end child of @paned to @child.
 *
 * If @child is `NULL`, the existing child will be removed.
 */
void
gtk_paned_set_end_child (GtkPaned  *paned,
                         GtkWidget *child)
{
  g_return_if_fail (GTK_IS_PANED (paned));
  g_return_if_fail (child == NULL || paned->end_child == child || gtk_widget_get_parent (child) == NULL);

  if (paned->end_child == child)
    return;

  g_clear_pointer (&paned->end_child, gtk_widget_unparent);

  if (child)
    {
      paned->end_child = child;
      gtk_widget_insert_after (child, GTK_WIDGET (paned), paned->handle_widget);
    }

  g_object_notify (G_OBJECT (paned), "end-child");
}

/**
 * gtk_paned_get_end_child: (attributes org.gtk.Method.get_property=end-child)
 * @paned: a `GtkPaned`
 *
 * Retrieves the end child of the given `GtkPaned`.
 *
 * Returns: (transfer none) (nullable): the end child widget
 */
GtkWidget *
gtk_paned_get_end_child (GtkPaned *paned)
{
  g_return_val_if_fail (GTK_IS_PANED (paned), NULL);

  return paned->end_child;
}

/**
 * gtk_paned_set_resize_end_child: (attributes org.gtk.Method.set_property=resize-end-child)
 * @paned: a `GtkPaned`
 * @resize: true to let the end child be resized
 *
 * Sets whether the [property@Gtk.Paned:end-child] can be resized.
 */
void
gtk_paned_set_resize_end_child (GtkPaned *paned,
                                gboolean  resize)
{
  g_return_if_fail (GTK_IS_PANED (paned));

  if (paned->resize_end_child == resize)
    return;

  paned->resize_end_child = resize;

  g_object_notify (G_OBJECT (paned), "resize-end-child");
}

/**
 * gtk_paned_get_resize_end_child: (attributes org.gtk.Method.get_property=resize-end-child)
 * @paned: a `GtkPaned`
 *
 * Returns whether the [property@Gtk.Paned:end-child] can be resized.
 *
 * Returns: true if the end child is resizable
 */
gboolean
gtk_paned_get_resize_end_child (GtkPaned *paned)
{
  g_return_val_if_fail (GTK_IS_PANED (paned), FALSE);

  return paned->resize_end_child;
}

/**
 * gtk_paned_set_shrink_end_child: (attributes org.gtk.Method.set_property=shrink-end-child)
 * @paned: a `GtkPaned`
 * @resize: true to let the end child be shrunk
 *
 * Sets whether the [property@Gtk.Paned:end-child] can shrink.
 */
void
gtk_paned_set_shrink_end_child (GtkPaned *paned,
                                gboolean  shrink)
{
  g_return_if_fail (GTK_IS_PANED (paned));

  if (paned->shrink_end_child == shrink)
    return;

  paned->shrink_end_child = shrink;

  g_object_notify (G_OBJECT (paned), "shrink-end-child");
}

/**
 * gtk_paned_get_shrink_end_child: (attributes org.gtk.Method.get_property=shrink-end-child)
 * @paned: a `GtkPaned`
 *
 * Returns whether the [property@Gtk.Paned:end-child] can shrink.
 *
 * Returns: true if the end child is shrinkable
 */
gboolean
gtk_paned_get_shrink_end_child (GtkPaned *paned)
{
  g_return_val_if_fail (GTK_IS_PANED (paned), FALSE);

  return paned->shrink_end_child;
}

/**
 * gtk_paned_get_position: (attributes org.gtk.Method.get_property=position)
 * @paned: a `GtkPaned` widget
 *
 * Obtains the position of the divider between the two panes.
 *
 * Returns: the position of the divider, in pixels
 **/
int
gtk_paned_get_position (GtkPaned  *paned)
{
  g_return_val_if_fail (GTK_IS_PANED (paned), 0);

  return paned->start_child_size;
}

/**
 * gtk_paned_set_position: (attributes org.gtk.Method.set_property=position)
 * @paned: a `GtkPaned` widget
 * @position: pixel position of divider, a negative value means that the position
 *   is unset
 *
 * Sets the position of the divider between the two panes.
 */
void
gtk_paned_set_position (GtkPaned *paned,
                        int       position)
{
  g_return_if_fail (GTK_IS_PANED (paned));

  g_object_freeze_notify (G_OBJECT (paned));

  if (position >= 0)
    {
      /* We don't clamp here - the assumption is that
       * if the total allocation changes at the same time
       * as the position, the position set is with reference
       * to the new total size. If only the position changes,
       * then clamping will occur in gtk_paned_calc_position()
       */

      if (!paned->position_set)
        g_object_notify_by_pspec (G_OBJECT (paned), paned_props[PROP_POSITION_SET]);

      if (paned->start_child_size != position)
        {
          g_object_notify_by_pspec (G_OBJECT (paned), paned_props[PROP_POSITION]);
          gtk_widget_queue_allocate (GTK_WIDGET (paned));
        }

      paned->start_child_size = position;
      paned->position_set = TRUE;
    }
  else
    {
      if (paned->position_set)
        g_object_notify_by_pspec (G_OBJECT (paned), paned_props[PROP_POSITION_SET]);

      paned->position_set = FALSE;
    }

  g_object_thaw_notify (G_OBJECT (paned));

#ifdef G_OS_WIN32
  /* Hacky work-around for bug #144269 */
  if (paned->end_child != NULL)
    {
      gtk_widget_queue_draw (paned->end_child);
    }
#endif
}

static void
gtk_paned_calc_position (GtkPaned *paned,
                         int       allocation,
                         int       start_child_req,
                         int       end_child_req)
{
  int old_position;
  int old_min_position;
  int old_max_position;

  old_position = paned->start_child_size;
  old_min_position = paned->min_position;
  old_max_position = paned->max_position;

  gtk_paned_compute_position (paned,
                              allocation, start_child_req, end_child_req,
                              &paned->min_position, &paned->max_position,
                              &paned->start_child_size);

  gtk_widget_set_child_visible (paned->start_child, paned->start_child_size != 0);
  gtk_widget_set_child_visible (paned->end_child, paned->start_child_size != allocation);

  g_object_freeze_notify (G_OBJECT (paned));
  if (paned->start_child_size != old_position)
    g_object_notify_by_pspec (G_OBJECT (paned), paned_props[PROP_POSITION]);
  if (paned->min_position != old_min_position)
    g_object_notify_by_pspec (G_OBJECT (paned), paned_props[PROP_MIN_POSITION]);
  if (paned->max_position != old_max_position)
    g_object_notify_by_pspec (G_OBJECT (paned), paned_props[PROP_MAX_POSITION]);
  g_object_thaw_notify (G_OBJECT (paned));

  paned->last_allocation = allocation;
}

static void
gtk_paned_set_saved_focus (GtkPaned *paned, GtkWidget *widget)
{
  if (paned->saved_focus)
    g_object_remove_weak_pointer (G_OBJECT (paned->saved_focus),
                                  (gpointer *)&(paned->saved_focus));

  paned->saved_focus = widget;

  if (paned->saved_focus)
    g_object_add_weak_pointer (G_OBJECT (paned->saved_focus),
                               (gpointer *)&(paned->saved_focus));
}

static void
gtk_paned_set_first_paned (GtkPaned *paned, GtkPaned *first_paned)
{
  if (paned->first_paned)
    g_object_remove_weak_pointer (G_OBJECT (paned->first_paned),
                                  (gpointer *)&(paned->first_paned));

  paned->first_paned = first_paned;

  if (paned->first_paned)
    g_object_add_weak_pointer (G_OBJECT (paned->first_paned),
                               (gpointer *)&(paned->first_paned));
}

static void
gtk_paned_set_last_start_child_focus (GtkPaned *paned, GtkWidget *widget)
{
  if (paned->last_start_child_focus)
    g_object_remove_weak_pointer (G_OBJECT (paned->last_start_child_focus),
                                  (gpointer *)&(paned->last_start_child_focus));

  paned->last_start_child_focus = widget;

  if (paned->last_start_child_focus)
    g_object_add_weak_pointer (G_OBJECT (paned->last_start_child_focus),
                               (gpointer *)&(paned->last_start_child_focus));
}

static void
gtk_paned_set_last_end_child_focus (GtkPaned *paned, GtkWidget *widget)
{
  if (paned->last_end_child_focus)
    g_object_remove_weak_pointer (G_OBJECT (paned->last_end_child_focus),
                                  (gpointer *)&(paned->last_end_child_focus));

  paned->last_end_child_focus = widget;

  if (paned->last_end_child_focus)
    g_object_add_weak_pointer (G_OBJECT (paned->last_end_child_focus),
                               (gpointer *)&(paned->last_end_child_focus));
}

static GtkWidget *
paned_get_focus_widget (GtkPaned *paned)
{
  GtkWidget *toplevel;

  toplevel = GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (paned)));
  if (GTK_IS_WINDOW (toplevel))
    return gtk_window_get_focus (GTK_WINDOW (toplevel));

  return NULL;
}

static void
gtk_paned_set_focus_child (GtkWidget *widget,
                           GtkWidget *child)
{
  GtkPaned *paned = GTK_PANED (widget);
  GtkWidget *focus_child;

  if (child == NULL)
    {
      GtkWidget *last_focus;
      GtkWidget *w;

      last_focus = paned_get_focus_widget (paned);

      if (last_focus)
        {
          /* If there is one or more paned widgets between us and the
           * focus widget, we want the topmost of those as last_focus
           */
          for (w = last_focus; w && w != GTK_WIDGET (paned); w = gtk_widget_get_parent (w))
            if (GTK_IS_PANED (w))
              last_focus = w;

          if (w == NULL)
            {
              g_warning ("Error finding last focus widget of GtkPaned %p, "
                         "gtk_paned_set_focus_child was called on widget %p "
                         "which is not child of %p.",
                         widget, child, widget);
              return;
            }

          focus_child = gtk_widget_get_focus_child (widget);
          if (focus_child == paned->start_child)
            gtk_paned_set_last_start_child_focus (paned, last_focus);
          else if (focus_child == paned->end_child)
            gtk_paned_set_last_end_child_focus (paned, last_focus);
        }
    }

  GTK_WIDGET_CLASS (gtk_paned_parent_class)->set_focus_child (widget, child);
}

static void
gtk_paned_get_cycle_chain (GtkPaned          *paned,
                           GtkDirectionType   direction,
                           GList            **widgets)
{
  GtkWidget *ancestor = NULL;
  GtkWidget *focus_child;
  GtkWidget *parent;
  GtkWidget *widget = GTK_WIDGET (paned);
  GList *temp_list = NULL;
  GList *list;

  if (paned->in_recursion)
    return;

  g_assert (widgets != NULL);

  if (paned->last_start_child_focus &&
      !gtk_widget_is_ancestor (paned->last_start_child_focus, widget))
    {
      gtk_paned_set_last_start_child_focus (paned, NULL);
    }

  if (paned->last_end_child_focus &&
      !gtk_widget_is_ancestor (paned->last_end_child_focus, widget))
    {
      gtk_paned_set_last_end_child_focus (paned, NULL);
    }

  parent = gtk_widget_get_parent (widget);
  if (parent)
    ancestor = gtk_widget_get_ancestor (parent, GTK_TYPE_PANED);

  /* The idea here is that temp_list is a list of widgets we want to cycle
   * to. The list is prioritized so that the first element is our first
   * choice, the next our second, and so on.
   *
   * We can't just use g_list_reverse(), because we want to try
   * paned->last_child?_focus before paned->child?, both when we
   * are going forward and backward.
   */
  focus_child = gtk_widget_get_focus_child (GTK_WIDGET (paned));
  if (direction == GTK_DIR_TAB_FORWARD)
    {
      if (focus_child == paned->start_child)
        {
          temp_list = g_list_append (temp_list, paned->last_end_child_focus);
          temp_list = g_list_append (temp_list, paned->end_child);
          temp_list = g_list_append (temp_list, ancestor);
        }
      else if (focus_child == paned->end_child)
        {
          temp_list = g_list_append (temp_list, ancestor);
          temp_list = g_list_append (temp_list, paned->last_start_child_focus);
          temp_list = g_list_append (temp_list, paned->start_child);
        }
      else
        {
          temp_list = g_list_append (temp_list, paned->last_start_child_focus);
          temp_list = g_list_append (temp_list, paned->start_child);
          temp_list = g_list_append (temp_list, paned->last_end_child_focus);
          temp_list = g_list_append (temp_list, paned->end_child);
          temp_list = g_list_append (temp_list, ancestor);
        }
    }
  else
    {
      if (focus_child == paned->start_child)
        {
          temp_list = g_list_append (temp_list, ancestor);
          temp_list = g_list_append (temp_list, paned->last_end_child_focus);
          temp_list = g_list_append (temp_list, paned->end_child);
        }
      else if (focus_child == paned->end_child)
        {
          temp_list = g_list_append (temp_list, paned->last_start_child_focus);
          temp_list = g_list_append (temp_list, paned->start_child);
          temp_list = g_list_append (temp_list, ancestor);
        }
      else
        {
          temp_list = g_list_append (temp_list, paned->last_end_child_focus);
          temp_list = g_list_append (temp_list, paned->end_child);
          temp_list = g_list_append (temp_list, paned->last_start_child_focus);
          temp_list = g_list_append (temp_list, paned->start_child);
          temp_list = g_list_append (temp_list, ancestor);
        }
    }

  /* Walk the list and expand all the paned widgets. */
  for (list = temp_list; list != NULL; list = list->next)
    {
      widget = list->data;

      if (widget)
        {
          if (GTK_IS_PANED (widget))
            {
              paned->in_recursion = TRUE;
              gtk_paned_get_cycle_chain (GTK_PANED (widget), direction, widgets);
              paned->in_recursion = FALSE;
            }
          else
            {
              *widgets = g_list_append (*widgets, widget);
            }
        }
    }

  g_list_free (temp_list);
}

static gboolean
gtk_paned_cycle_child_focus (GtkPaned *paned,
                             gboolean  reversed)
{
  GList *cycle_chain = NULL;
  GList *list;

  GtkDirectionType direction = reversed? GTK_DIR_TAB_BACKWARD : GTK_DIR_TAB_FORWARD;

  /* ignore f6 if the handle is focused */
  if (gtk_widget_is_focus (GTK_WIDGET (paned)))
    return TRUE;

  /* we can't just let the event propagate up the hierarchy,
   * because the paned will want to cycle focus _unless_ an
   * ancestor paned handles the event
   */
  gtk_paned_get_cycle_chain (paned, direction, &cycle_chain);

  for (list = cycle_chain; list != NULL; list = list->next)
    if (gtk_widget_child_focus (GTK_WIDGET (list->data), direction))
      break;

  g_list_free (cycle_chain);

  return TRUE;
}

static void
get_child_panes (GtkWidget  *widget,
                 GList     **panes)
{
  if (!widget || !gtk_widget_get_realized (widget))
    return;

  if (GTK_IS_PANED (widget))
    {
      GtkPaned *paned = GTK_PANED (widget);

      get_child_panes (paned->start_child, panes);
      *panes = g_list_prepend (*panes, widget);
      get_child_panes (paned->end_child, panes);
    }
  else
    {
      GtkWidget *child;

      for (child = gtk_widget_get_first_child (widget);
           child != NULL;
           child = gtk_widget_get_next_sibling (child))
        get_child_panes (child, panes);
    }
}

static GList *
get_all_panes (GtkPaned *paned)
{
  GtkPaned *topmost = NULL;
  GList *result = NULL;
  GtkWidget *w;

  for (w = GTK_WIDGET (paned); w != NULL; w = gtk_widget_get_parent (w))
    {
      if (GTK_IS_PANED (w))
        topmost = GTK_PANED (w);
    }

  g_assert (topmost);

  get_child_panes (GTK_WIDGET (topmost), &result);

  return g_list_reverse (result);
}

static void
gtk_paned_find_neighbours (GtkPaned  *paned,
                           GtkPaned **next,
                           GtkPaned **prev)
{
  GList *all_panes;
  GList *this_link;

  all_panes = get_all_panes (paned);
  g_assert (all_panes);

  this_link = g_list_find (all_panes, paned);

  g_assert (this_link);

  if (this_link->next)
    *next = this_link->next->data;
  else
    *next = all_panes->data;

  if (this_link->prev)
    *prev = this_link->prev->data;
  else
    *prev = g_list_last (all_panes)->data;

  g_list_free (all_panes);
}

static gboolean
gtk_paned_move_handle (GtkPaned      *paned,
                       GtkScrollType  scroll)
{
  if (gtk_widget_is_focus (GTK_WIDGET (paned)))
    {
      int old_position;
      int new_position;
      int increment;

      enum {
        SINGLE_STEP_SIZE = 1,
        PAGE_STEP_SIZE   = 75
      };

      new_position = old_position = gtk_paned_get_position (paned);
      increment = 0;

      switch (scroll)
        {
        case GTK_SCROLL_STEP_LEFT:
        case GTK_SCROLL_STEP_UP:
        case GTK_SCROLL_STEP_BACKWARD:
          increment = - SINGLE_STEP_SIZE;
          break;

        case GTK_SCROLL_STEP_RIGHT:
        case GTK_SCROLL_STEP_DOWN:
        case GTK_SCROLL_STEP_FORWARD:
          increment = SINGLE_STEP_SIZE;
          break;

        case GTK_SCROLL_PAGE_LEFT:
        case GTK_SCROLL_PAGE_UP:
        case GTK_SCROLL_PAGE_BACKWARD:
          increment = - PAGE_STEP_SIZE;
          break;

        case GTK_SCROLL_PAGE_RIGHT:
        case GTK_SCROLL_PAGE_DOWN:
        case GTK_SCROLL_PAGE_FORWARD:
          increment = PAGE_STEP_SIZE;
          break;

        case GTK_SCROLL_START:
          new_position = paned->min_position;
          break;

        case GTK_SCROLL_END:
          new_position = paned->max_position;
          break;

        case GTK_SCROLL_NONE:
        case GTK_SCROLL_JUMP:
        default:
          break;
        }

      if (increment)
        {
          if (is_rtl (paned))
            increment = -increment;

          new_position = old_position + increment;
        }

      new_position = CLAMP (new_position, paned->min_position, paned->max_position);

      if (old_position != new_position)
        gtk_paned_set_position (paned, new_position);

      return TRUE;
    }

  return FALSE;
}

static void
gtk_paned_restore_focus (GtkPaned *paned)
{
  if (gtk_widget_is_focus (GTK_WIDGET (paned)))
    {
      if (paned->saved_focus &&
          gtk_widget_get_sensitive (paned->saved_focus))
        {
          gtk_widget_grab_focus (paned->saved_focus);
        }
      else
        {
          /* the saved focus is somehow not available for focusing,
           * try
           *   1) tabbing into the paned widget
           * if that didn't work,
           *   2) unset focus for the window if there is one
           */

          if (!gtk_widget_child_focus (GTK_WIDGET (paned), GTK_DIR_TAB_FORWARD))
            {
              GtkRoot *root = gtk_widget_get_root (GTK_WIDGET (paned));
              gtk_root_set_focus (root, NULL);
            }
        }

      gtk_paned_set_saved_focus (paned, NULL);
      gtk_paned_set_first_paned (paned, NULL);
    }
}

static gboolean
gtk_paned_accept_position (GtkPaned *paned)
{
  if (gtk_widget_is_focus (GTK_WIDGET (paned)))
    {
      paned->original_position = -1;
      gtk_paned_restore_focus (paned);

      return TRUE;
    }

  return FALSE;
}


static gboolean
gtk_paned_cancel_position (GtkPaned *paned)
{
  if (gtk_widget_is_focus (GTK_WIDGET (paned)))
    {
      if (paned->original_position != -1)
        {
          gtk_paned_set_position (paned, paned->original_position);
          paned->original_position = -1;
        }

      gtk_paned_restore_focus (paned);
      return TRUE;
    }

  return FALSE;
}

static gboolean
gtk_paned_cycle_handle_focus (GtkPaned *paned,
                              gboolean  reversed)
{
  GtkPaned *next, *prev;

  if (gtk_widget_is_focus (GTK_WIDGET (paned)))
    {
      GtkPaned *focus = NULL;

      if (!paned->first_paned)
        {
          /* The first_pane has disappeared. As an ad-hoc solution,
           * we make the currently focused paned the first_paned. To the
           * user this will seem like the paned cycling has been reset.
           */

          gtk_paned_set_first_paned (paned, paned);
        }

      gtk_paned_find_neighbours (paned, &next, &prev);

      if (reversed && prev &&
          prev != paned && paned != paned->first_paned)
        {
          focus = prev;
        }
      else if (!reversed && next &&
               next != paned && next != paned->first_paned)
        {
          focus = next;
        }
      else
        {
          gtk_paned_accept_position (paned);
          return TRUE;
        }

      g_assert (focus);

      gtk_paned_set_saved_focus (focus, paned->saved_focus);
      gtk_paned_set_first_paned (focus, paned->first_paned);

      gtk_paned_set_saved_focus (paned, NULL);
      gtk_paned_set_first_paned (paned, NULL);

      gtk_widget_grab_focus (GTK_WIDGET (focus));

      if (!gtk_widget_is_focus (GTK_WIDGET (paned)))
        {
          paned->original_position = -1;
          paned->original_position = gtk_paned_get_position (focus);
        }
    }
  else
    {
      GtkPaned *focus;
      GtkPaned *first;
      GtkWidget *focus_child;

      gtk_paned_find_neighbours (paned, &next, &prev);
      focus_child = gtk_widget_get_focus_child (GTK_WIDGET (paned));

      if (focus_child == paned->start_child)
        {
          if (reversed)
            {
              focus = prev;
              first = paned;
            }
          else
            {
              focus = paned;
              first = paned;
            }
        }
      else if (focus_child == paned->end_child)
        {
          if (reversed)
            {
              focus = paned;
              first = next;
            }
          else
            {
              focus = next;
              first = next;
            }
        }
      else
        {
          /* Focus is not inside this paned, and we don't have focus.
           * Presumably this happened because the application wants us
           * to start keyboard navigating.
           */
          focus = paned;

          if (reversed)
            first = paned;
          else
            first = next;
        }

      gtk_paned_set_saved_focus (focus, gtk_root_get_focus (gtk_widget_get_root (GTK_WIDGET (paned))));
      gtk_paned_set_first_paned (focus, first);
      paned->original_position = gtk_paned_get_position (focus);

      gtk_widget_grab_focus (GTK_WIDGET (focus));
   }

  return TRUE;
}

static gboolean
gtk_paned_toggle_handle_focus (GtkPaned *paned)
{
  /* This function/signal has the wrong name. It is called when you
   * press Tab or Shift-Tab and what we do is act as if
   * the user pressed Return and then Tab or Shift-Tab
   */
  if (gtk_widget_is_focus (GTK_WIDGET (paned)))
    gtk_paned_accept_position (paned);

  return FALSE;
}

/**
 * gtk_paned_set_wide_handle: (attributes org.gtk.Method.set_property=wide-handle)
 * @paned: a `GtkPaned`
 * @wide: the new value for the [property@Gtk.Paned:wide-handle] property
 *
 * Sets whether the separator should be wide.
 */
void
gtk_paned_set_wide_handle (GtkPaned *paned,
                           gboolean  wide)
{
  gboolean old_wide;

  g_return_if_fail (GTK_IS_PANED (paned));

  old_wide = gtk_paned_get_wide_handle (paned);
  if (old_wide != wide)
    {
      if (wide)
        gtk_widget_add_css_class (paned->handle_widget, "wide");
      else
        gtk_widget_remove_css_class (paned->handle_widget, "wide");

      g_object_notify_by_pspec (G_OBJECT (paned), paned_props[PROP_WIDE_HANDLE]);
    }
}

/**
 * gtk_paned_get_wide_handle: (attributes org.gtk.Method.get_property=wide-handle)
 * @paned: a `GtkPaned`
 *
 * Gets whether the separator should be wide.
 *
 * Returns: %TRUE if the paned should have a wide handle
 */
gboolean
gtk_paned_get_wide_handle (GtkPaned *paned)
{
  g_return_val_if_fail (GTK_IS_PANED (paned), FALSE);

  return gtk_widget_has_css_class (paned->handle_widget, "wide");
}
