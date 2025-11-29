/* GTK - The GIMP Toolkit
 *
 * Copyright (C) 2003 Sun Microsystems, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *      Mark McLoughlin <mark@skynet.ie>
 */

/**
 * GtkExpander:
 *
 * `GtkExpander` allows the user to reveal its child by clicking
 * on an expander triangle.
 *
 * ![An example GtkExpander](expander.png)
 *
 * This is similar to the triangles used in a `GtkTreeView`.
 *
 * Normally you use an expander as you would use a frame; you create
 * the child widget and use [method@Gtk.Expander.set_child] to add it
 * to the expander. When the expander is toggled, it will take care of
 * showing and hiding the child automatically.
 *
 * # Special Usage
 *
 * There are situations in which you may prefer to show and hide the
 * expanded widget yourself, such as when you want to actually create
 * the widget at expansion time. In this case, create a `GtkExpander`
 * but do not add a child to it. The expander widget has an
 * [property@Gtk.Expander:expanded] property which can be used to
 * monitor its expansion state. You should watch this property with
 * a signal connection as follows:
 *
 * ```c
 * static void
 * expander_callback (GObject    *object,
 *                    GParamSpec *param_spec,
 *                    gpointer    user_data)
 * {
 *   GtkExpander *expander;
 *
 *   expander = GTK_EXPANDER (object);
 *
 *   if (gtk_expander_get_expanded (expander))
 *     {
 *       // Show or create widgets
 *     }
 *   else
 *     {
 *       // Hide or destroy widgets
 *     }
 * }
 *
 * static void
 * create_expander (void)
 * {
 *   GtkWidget *expander = gtk_expander_new_with_mnemonic ("_More Options");
 *   g_signal_connect (expander, "notify::expanded",
 *                     G_CALLBACK (expander_callback), NULL);
 *
 *   // ...
 * }
 * ```
 *
 * # GtkExpander as GtkBuildable
 *
 * The `GtkExpander` implementation of the `GtkBuildable` interface supports
 * placing a child in the label position by specifying “label” as the
 * “type” attribute of a `<child>` element. A normal content child can be
 * specified without specifying a `<child>` type attribute.
 *
 * An example of a UI definition fragment with GtkExpander:
 *
 * ```xml
 * <object class="GtkExpander">
 *   <child type="label">
 *     <object class="GtkLabel" id="expander-label"/>
 *   </child>
 *   <child>
 *     <object class="GtkEntry" id="expander-content"/>
 *   </child>
 * </object>
 * ```
 *
 * # CSS nodes
 *
 * ```
 * expander-widget
 * ╰── box
 *     ├── title
 *     │   ├── expander
 *     │   ╰── <label widget>
 *     ╰── <child>
 * ```
 *
 * `GtkExpander` has a main node `expander-widget`, and subnode `box` containing
 * the title and child widget. The box subnode `title` contains node `expander`,
 * i.e. the expand/collapse arrow; then the label widget if any. The arrow of an
 * expander that is showing its child gets the `:checked` pseudoclass set on it.
 *
 * # Accessibility
 *
 * `GtkExpander` uses the %GTK_ACCESSIBLE_ROLE_BUTTON role.
 */

#include "config.h"

#include "gtkexpander.h"

#include "gtkbox.h"
#include "gtkbuildable.h"
#include "gtkdropcontrollermotion.h"
#include "gtkbuiltiniconprivate.h"
#include "gtkgestureclick.h"
#include "gtkgesturesingle.h"
#include "gtklabel.h"
#include "gtkmarshalers.h"
#include "gtkmain.h"
#include "gtkprivate.h"
#include "gtkwidgetprivate.h"

#include <string.h>

#define TIMEOUT_EXPAND 500

enum
{
  PROP_0,
  PROP_EXPANDED,
  PROP_LABEL,
  PROP_USE_UNDERLINE,
  PROP_USE_MARKUP,
  PROP_LABEL_WIDGET,
  PROP_RESIZE_TOPLEVEL,
  PROP_CHILD
};

typedef struct _GtkExpanderClass   GtkExpanderClass;

struct _GtkExpander
{
  GtkWidget parent_instance;

  GtkWidget        *label_widget;

  GtkWidget        *box;
  GtkWidget        *title_widget;
  GtkWidget        *arrow_widget;
  GtkWidget        *child;

  guint             expand_timer;

  guint             expanded        : 1;
  guint             use_underline   : 1;
  guint             use_markup      : 1;
  guint             resize_toplevel : 1;
};

struct _GtkExpanderClass
{
  GtkWidgetClass parent_class;

  void (* activate) (GtkExpander *expander);
};

static void gtk_expander_dispose      (GObject          *object);
static void gtk_expander_set_property (GObject          *object,
                                       guint             prop_id,
                                       const GValue     *value,
                                       GParamSpec       *pspec);
static void gtk_expander_get_property (GObject          *object,
                                       guint             prop_id,
                                       GValue           *value,
                                       GParamSpec       *pspec);

static void     gtk_expander_size_allocate  (GtkWidget        *widget,
                                             int               width,
                                             int               height,
                                             int               baseline);
static gboolean gtk_expander_focus          (GtkWidget        *widget,
                                             GtkDirectionType  direction);

static void gtk_expander_activate (GtkExpander *expander);


/* GtkBuildable */
static void gtk_expander_buildable_init           (GtkBuildableIface *iface);
static void gtk_expander_buildable_add_child      (GtkBuildable *buildable,
                                                   GtkBuilder   *builder,
                                                   GObject      *child,
                                                   const char   *type);


/* GtkWidget      */
static void gtk_expander_measure (GtkWidget      *widget,
                                  GtkOrientation  orientation,
                                  int             for_size,
                                  int            *minimum,
                                  int            *natural,
                                  int            *minimum_baseline,
                                  int            *natural_baseline);

/* Gestures */
static void     gesture_click_released_cb (GtkGestureClick *gesture,
                                           int              n_press,
                                           double           x,
                                           double           y,
                                           GtkExpander     *expander);

G_DEFINE_TYPE_WITH_CODE (GtkExpander, gtk_expander, GTK_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                gtk_expander_buildable_init))

static gboolean
expand_timeout (gpointer data)
{
  GtkExpander *expander = GTK_EXPANDER (data);

  expander->expand_timer = 0;
  gtk_expander_set_expanded (expander, TRUE);

  return FALSE;
}

static void
gtk_expander_drag_enter (GtkDropControllerMotion *motion,
                         double                   x,
                         double                   y,
                         GtkExpander             *expander)
{
  if (!expander->expanded && !expander->expand_timer)
    {
      expander->expand_timer = g_timeout_add (TIMEOUT_EXPAND, (GSourceFunc) expand_timeout, expander);
      gdk_source_set_static_name_by_id (expander->expand_timer, "[gtk] expand_timeout");
    }
}

static void
gtk_expander_drag_leave (GtkDropControllerMotion *motion,
                         GtkExpander             *expander)
{
  if (expander->expand_timer)
    {
      g_source_remove (expander->expand_timer);
      expander->expand_timer = 0;
    }
}

static GtkSizeRequestMode
gtk_expander_get_request_mode (GtkWidget *widget)
{
  GtkExpander *expander = GTK_EXPANDER (widget);

  if (expander->child)
    return gtk_widget_get_request_mode (expander->child);
  else
    return GTK_SIZE_REQUEST_CONSTANT_SIZE;
}

static void
gtk_expander_compute_expand (GtkWidget *widget,
                             gboolean  *hexpand,
                             gboolean  *vexpand)
{
  GtkExpander *expander = GTK_EXPANDER (widget);

  if (expander->child)
    {
      *hexpand = gtk_widget_compute_expand (expander->child, GTK_ORIENTATION_HORIZONTAL);
      *vexpand = gtk_widget_compute_expand (expander->child, GTK_ORIENTATION_VERTICAL);
    }
  else
    {
      *hexpand = FALSE;
      *vexpand = FALSE;
    }
}

static void
gtk_expander_class_init (GtkExpanderClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  guint activate_signal;

  gobject_class->dispose = gtk_expander_dispose;
  gobject_class->set_property = gtk_expander_set_property;
  gobject_class->get_property = gtk_expander_get_property;

  widget_class->size_allocate = gtk_expander_size_allocate;
  widget_class->focus = gtk_expander_focus;
  widget_class->grab_focus = gtk_widget_grab_focus_self;
  widget_class->measure = gtk_expander_measure;
  widget_class->compute_expand = gtk_expander_compute_expand;
  widget_class->get_request_mode = gtk_expander_get_request_mode;

  klass->activate = gtk_expander_activate;

  /**
   * GtkExpander:expanded: (attributes org.gtk.Property.get=gtk_expander_get_expanded org.gtk.Property.set=gtk_expander_set_expanded)
   *
   * Whether the expander has been opened to reveal the child.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_EXPANDED,
                                   g_param_spec_boolean ("expanded", NULL, NULL,
                                                         FALSE,
                                                         GTK_PARAM_READWRITE|G_PARAM_CONSTRUCT|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkExpander:label: (attributes org.gtk.Property.get=gtk_expander_get_label org.gtk.Property.set=gtk_expander_set_label)
   *
   * The text of the expanders label.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_LABEL,
                                   g_param_spec_string ("label", NULL, NULL,
                                                        NULL,
                                                        GTK_PARAM_READWRITE|G_PARAM_CONSTRUCT));

  /**
   * GtkExpander:use-underline: (attributes org.gtk.Property.get=gtk_expander_get_use_underline org.gtk.Property.set=gtk_expander_set_use_underline)
   *
   * Whether an underline in the text indicates a mnemonic.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_USE_UNDERLINE,
                                   g_param_spec_boolean ("use-underline", NULL, NULL,
                                                         FALSE,
                                                         GTK_PARAM_READWRITE|G_PARAM_CONSTRUCT|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkExpander:use-markup: (attributes org.gtk.Property.get=gtk_expander_get_use_markup org.gtk.Property.set=gtk_expander_set_use_markup)
   *
   * Whether the text in the label is Pango markup.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_USE_MARKUP,
                                   g_param_spec_boolean ("use-markup", NULL, NULL,
                                                         FALSE,
                                                         GTK_PARAM_READWRITE|G_PARAM_CONSTRUCT|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkExpander:label-widget: (attributes org.gtk.Property.get=gtk_expander_get_label_widget org.gtk.Property.set=gtk_expander_set_label_widget)
   *
   * A widget to display instead of the usual expander label.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_LABEL_WIDGET,
                                   g_param_spec_object ("label-widget", NULL, NULL,
                                                        GTK_TYPE_WIDGET,
                                                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkExpander:resize-toplevel: (attributes org.gtk.Property.get=gtk_expander_get_resize_toplevel org.gtk.Property.set=gtk_expander_set_resize_toplevel)
   *
   * When this property is %TRUE, the expander will resize the toplevel
   * widget containing the expander upon expanding and collapsing.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_RESIZE_TOPLEVEL,
                                   g_param_spec_boolean ("resize-toplevel", NULL, NULL,
                                                         FALSE,
                                                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkExpander:child: (attributes org.gtk.Property.get=gtk_expander_get_child org.gtk.Property.set=gtk_expander_set_child)
   *
   * The child widget.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_CHILD,
                                   g_param_spec_object ("child", NULL, NULL,
                                                        GTK_TYPE_WIDGET,
                                                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkExpander::activate:
   * @expander: the `GtkExpander` that emitted the signal
   *
   * Activates the `GtkExpander`.
   */
  activate_signal =
    g_signal_new (I_("activate"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkExpanderClass, activate),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  gtk_widget_class_set_activate_signal (widget_class, activate_signal);
  gtk_widget_class_set_css_name (widget_class, I_("expander-widget"));
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_BUTTON);
}

static void
gtk_expander_init (GtkExpander *expander)
{
  GtkGesture *gesture;
  GtkEventController *controller;

  expander->label_widget = NULL;
  expander->child = NULL;

  expander->expanded = FALSE;
  expander->use_underline = FALSE;
  expander->use_markup = FALSE;
  expander->expand_timer = 0;
  expander->resize_toplevel = 0;

  gtk_widget_set_focusable (GTK_WIDGET (expander), TRUE);

  expander->box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_parent (expander->box, GTK_WIDGET (expander));

  expander->title_widget = g_object_new (GTK_TYPE_BOX,
                                         "css-name", "title",
                                         NULL);
  gtk_box_append (GTK_BOX (expander->box), expander->title_widget);

  expander->arrow_widget = gtk_builtin_icon_new ("expander");
  gtk_widget_add_css_class (expander->arrow_widget, "horizontal");
  gtk_box_append (GTK_BOX (expander->title_widget), expander->arrow_widget);

  controller = gtk_drop_controller_motion_new ();
  g_signal_connect (controller, "enter", G_CALLBACK (gtk_expander_drag_enter), expander);
  g_signal_connect (controller, "leave", G_CALLBACK (gtk_expander_drag_leave), expander);
  gtk_widget_add_controller (GTK_WIDGET (expander), controller);

  gesture = gtk_gesture_click_new ();
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture),
                                 GDK_BUTTON_PRIMARY);
  gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (gesture),
                                     FALSE);
  g_signal_connect (gesture, "released",
                    G_CALLBACK (gesture_click_released_cb), expander);
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (gesture),
                                              GTK_PHASE_BUBBLE);
  gtk_widget_add_controller (GTK_WIDGET (expander->title_widget), GTK_EVENT_CONTROLLER (gesture));

  gtk_accessible_update_state (GTK_ACCESSIBLE (expander),
                               GTK_ACCESSIBLE_STATE_EXPANDED, FALSE,
                               -1);
}

static GtkBuildableIface *parent_buildable_iface;

static void
gtk_expander_buildable_add_child (GtkBuildable  *buildable,
                                  GtkBuilder    *builder,
                                  GObject       *child,
                                  const char    *type)
{
  if (g_strcmp0 (type, "label") == 0)
    gtk_expander_set_label_widget (GTK_EXPANDER (buildable), GTK_WIDGET (child));
  else if (GTK_IS_WIDGET (child))
    gtk_expander_set_child (GTK_EXPANDER (buildable), GTK_WIDGET (child));
  else
    parent_buildable_iface->add_child (buildable, builder, child, type);
}

static void
gtk_expander_buildable_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);

  iface->add_child = gtk_expander_buildable_add_child;
}

static void
gtk_expander_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  GtkExpander *expander = GTK_EXPANDER (object);

  switch (prop_id)
    {
    case PROP_EXPANDED:
      gtk_expander_set_expanded (expander, g_value_get_boolean (value));
      break;
    case PROP_LABEL:
      gtk_expander_set_label (expander, g_value_get_string (value));
      break;
    case PROP_USE_UNDERLINE:
      gtk_expander_set_use_underline (expander, g_value_get_boolean (value));
      break;
    case PROP_USE_MARKUP:
      gtk_expander_set_use_markup (expander, g_value_get_boolean (value));
      break;
    case PROP_LABEL_WIDGET:
      gtk_expander_set_label_widget (expander, g_value_get_object (value));
      break;
    case PROP_RESIZE_TOPLEVEL:
      gtk_expander_set_resize_toplevel (expander, g_value_get_boolean (value));
      break;
    case PROP_CHILD:
      gtk_expander_set_child (expander, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_expander_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  GtkExpander *expander = GTK_EXPANDER (object);

  switch (prop_id)
    {
    case PROP_EXPANDED:
      g_value_set_boolean (value, expander->expanded);
      break;
    case PROP_LABEL:
      g_value_set_string (value, gtk_expander_get_label (expander));
      break;
    case PROP_USE_UNDERLINE:
      g_value_set_boolean (value, expander->use_underline);
      break;
    case PROP_USE_MARKUP:
      g_value_set_boolean (value, expander->use_markup);
      break;
    case PROP_LABEL_WIDGET:
      g_value_set_object (value,
                          expander->label_widget ?
                          G_OBJECT (expander->label_widget) : NULL);
      break;
    case PROP_RESIZE_TOPLEVEL:
      g_value_set_boolean (value, gtk_expander_get_resize_toplevel (expander));
      break;
    case PROP_CHILD:
      g_value_set_object (value, gtk_expander_get_child (expander));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_expander_dispose (GObject *object)
{
  GtkExpander *expander = GTK_EXPANDER (object);

  if (expander->expand_timer)
    {
      g_source_remove (expander->expand_timer);
      expander->expand_timer = 0;
    }

  /* If the expander is not expanded, we own the child */
  if (!expander->expanded)
    g_clear_object (&expander->child);

  if (expander->box)
    {
      gtk_widget_unparent (expander->box);
      expander->box = NULL;
      expander->child = NULL;
      expander->label_widget = NULL;
      expander->arrow_widget = NULL;
    }

  G_OBJECT_CLASS (gtk_expander_parent_class)->dispose (object);
}

static void
gtk_expander_size_allocate (GtkWidget *widget,
                            int        width,
                            int        height,
                            int        baseline)
{
  GtkExpander *expander = GTK_EXPANDER (widget);

  gtk_widget_size_allocate (expander->box,
                            &(GtkAllocation) {
                              0, 0,
                              width, height
                            }, baseline);
}

static void
gesture_click_released_cb (GtkGestureClick *gesture,
                           int              n_press,
                           double           x,
                           double           y,
                           GtkExpander     *expander)
{
  gtk_widget_activate (GTK_WIDGET (expander));
}

typedef enum
{
  FOCUS_NONE,
  FOCUS_WIDGET,
  FOCUS_LABEL,
  FOCUS_CHILD
} FocusSite;

static gboolean
focus_current_site (GtkExpander      *expander,
                    GtkDirectionType  direction)
{
  GtkWidget *current_focus;

  current_focus = gtk_widget_get_focus_child (GTK_WIDGET (expander));

  if (!current_focus)
    return FALSE;

  return gtk_widget_child_focus (current_focus, direction);
}

static gboolean
focus_in_site (GtkExpander      *expander,
               FocusSite         site,
               GtkDirectionType  direction)
{
  switch (site)
    {
    case FOCUS_WIDGET:
      gtk_widget_grab_focus (GTK_WIDGET (expander));
      return TRUE;
    case FOCUS_LABEL:
      if (expander->label_widget)
        return gtk_widget_child_focus (expander->label_widget, direction);
      else
        return FALSE;
    case FOCUS_CHILD:
      {
        GtkWidget *child = expander->child;

        if (child && gtk_widget_get_child_visible (child))
          return gtk_widget_child_focus (child, direction);
        else
          return FALSE;
      }
    case FOCUS_NONE:
    default:
      break;
    }

  g_assert_not_reached ();
  return FALSE;
}

static FocusSite
get_next_site (GtkExpander      *expander,
               FocusSite         site,
               GtkDirectionType  direction)
{
  gboolean ltr;

  ltr = gtk_widget_get_direction (GTK_WIDGET (expander)) != GTK_TEXT_DIR_RTL;

  switch (site)
    {
    case FOCUS_NONE:
      switch (direction)
        {
        case GTK_DIR_TAB_BACKWARD:
        case GTK_DIR_LEFT:
        case GTK_DIR_UP:
          return FOCUS_CHILD;
        case GTK_DIR_TAB_FORWARD:
        case GTK_DIR_DOWN:
        case GTK_DIR_RIGHT:
        default:
          return FOCUS_WIDGET;
        }
      break;
    case FOCUS_WIDGET:
      switch (direction)
        {
        case GTK_DIR_TAB_BACKWARD:
        case GTK_DIR_UP:
          return FOCUS_NONE;
        case GTK_DIR_LEFT:
          return ltr ? FOCUS_NONE : FOCUS_LABEL;
        case GTK_DIR_TAB_FORWARD:
        case GTK_DIR_DOWN:
        default:
          return FOCUS_LABEL;
        case GTK_DIR_RIGHT:
          return ltr ? FOCUS_LABEL : FOCUS_NONE;
        }
      break;
    case FOCUS_LABEL:
      switch (direction)
        {
        case GTK_DIR_TAB_BACKWARD:
        case GTK_DIR_UP:
          return FOCUS_WIDGET;
        case GTK_DIR_LEFT:
          return ltr ? FOCUS_WIDGET : FOCUS_CHILD;
        case GTK_DIR_TAB_FORWARD:
        case GTK_DIR_DOWN:
        default:
          return FOCUS_CHILD;
        case GTK_DIR_RIGHT:
          return ltr ? FOCUS_CHILD : FOCUS_WIDGET;
        }
      break;
    case FOCUS_CHILD:
      switch (direction)
        {
        case GTK_DIR_TAB_BACKWARD:
        case GTK_DIR_LEFT:
        case GTK_DIR_UP:
          return FOCUS_LABEL;
        case GTK_DIR_TAB_FORWARD:
        case GTK_DIR_DOWN:
        case GTK_DIR_RIGHT:
        default:
          return FOCUS_NONE;
        }
      break;
    default:
      g_assert_not_reached ();
      break;
    }
  
  return FOCUS_NONE;
}

static void
gtk_expander_resize_toplevel (GtkExpander *expander)
{
  GtkWidget *child = expander->child;

  if (child && expander->resize_toplevel &&
      gtk_widget_get_realized (GTK_WIDGET (expander)))
    {
      GtkWidget *toplevel = GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (expander)));

      if (GTK_IS_WINDOW (toplevel) &&
          gtk_widget_get_realized (toplevel))
        gtk_widget_queue_resize (GTK_WIDGET (expander));
    }
}

static gboolean
gtk_expander_focus (GtkWidget        *widget,
                    GtkDirectionType  direction)
{
  GtkExpander *expander = GTK_EXPANDER (widget);

  if (!focus_current_site (expander, direction))
    {
      GtkWidget *old_focus_child;
      gboolean widget_is_focus;
      FocusSite site = FOCUS_NONE;

      widget_is_focus = gtk_widget_is_focus (widget);
      old_focus_child = gtk_widget_get_focus_child (GTK_WIDGET (widget));

      if (old_focus_child && old_focus_child == expander->label_widget)
        site = FOCUS_LABEL;
      else if (old_focus_child)
        site = FOCUS_CHILD;
      else if (widget_is_focus)
        site = FOCUS_WIDGET;

      while ((site = get_next_site (expander, site, direction)) != FOCUS_NONE)
        {
          if (focus_in_site (expander, site, direction))
            return TRUE;
        }

      return FALSE;
    }

  return TRUE;
}

static void
gtk_expander_activate (GtkExpander *expander)
{
  gtk_expander_set_expanded (expander, !expander->expanded);
}

static void
gtk_expander_measure (GtkWidget      *widget,
                      GtkOrientation  orientation,
                      int             for_size,
                      int            *minimum,
                      int            *natural,
                      int            *minimum_baseline,
                      int            *natural_baseline)
{
  GtkExpander *expander = GTK_EXPANDER (widget);

  gtk_widget_measure (expander->box,
                       orientation,
                       for_size,
                       minimum, natural,
                       minimum_baseline, natural_baseline);
}

/**
 * gtk_expander_new:
 * @label: (nullable): the text of the label
 *
 * Creates a new expander using @label as the text of the label.
 *
 * Returns: a new `GtkExpander` widget.
 */
GtkWidget *
gtk_expander_new (const char *label)
{
  return g_object_new (GTK_TYPE_EXPANDER, "label", label, NULL);
}

/**
 * gtk_expander_new_with_mnemonic:
 * @label: (nullable): the text of the label with an underscore
 *   in front of the mnemonic character
 *
 * Creates a new expander using @label as the text of the label.
 *
 * If characters in @label are preceded by an underscore, they are
 * underlined. If you need a literal underscore character in a label,
 * use “__” (two underscores). The first underlined character represents
 * a keyboard accelerator called a mnemonic.
 *
 * Pressing Alt and that key activates the button.
 *
 * Returns: a new `GtkExpander` widget.
 */
GtkWidget *
gtk_expander_new_with_mnemonic (const char *label)
{
  return g_object_new (GTK_TYPE_EXPANDER,
                       "label", label,
                       "use-underline", TRUE,
                       NULL);
}

/**
 * gtk_expander_set_expanded: (attributes org.gtk.Method.set_property=expanded)
 * @expander: a `GtkExpander`
 * @expanded: whether the child widget is revealed
 *
 * Sets the state of the expander.
 *
 * Set to %TRUE, if you want the child widget to be revealed,
 * and %FALSE if you want the child widget to be hidden.
 */
void
gtk_expander_set_expanded (GtkExpander *expander,
                           gboolean     expanded)
{
  GtkWidget *child;

  g_return_if_fail (GTK_IS_EXPANDER (expander));

  expanded = expanded != FALSE;

  if (expander->expanded == expanded)
    return;

  expander->expanded = expanded;

  if (expander->expanded)
    gtk_widget_set_state_flags (expander->arrow_widget, GTK_STATE_FLAG_CHECKED, FALSE);
  else
    gtk_widget_unset_state_flags (expander->arrow_widget, GTK_STATE_FLAG_CHECKED);

  child = expander->child;

  if (child)
    {
      /* Transfer the ownership of the child to the box when
       * expanded is set, and then back to us when it is unset
       */
      if (expander->expanded)
        {
          gtk_box_append (GTK_BOX (expander->box), child);
          g_object_unref (expander->child);
          gtk_accessible_update_relation (GTK_ACCESSIBLE (expander),
                                          GTK_ACCESSIBLE_RELATION_CONTROLS, expander->child, NULL,
                                          -1);
        }
      else
        {
          gtk_accessible_reset_relation (GTK_ACCESSIBLE (expander),
                                         GTK_ACCESSIBLE_RELATION_CONTROLS);
          g_object_ref (expander->child);
          gtk_box_remove (GTK_BOX (expander->box), child);
        }
      gtk_expander_resize_toplevel (expander);
    }

  gtk_accessible_update_state (GTK_ACCESSIBLE (expander),
                               GTK_ACCESSIBLE_STATE_EXPANDED, expanded,
                               -1);

  g_object_notify (G_OBJECT (expander), "expanded");
}

/**
 * gtk_expander_get_expanded: (attributes org.gtk.Method.get_property=expanded)
 * @expander:a `GtkExpander`
 *
 * Queries a `GtkExpander` and returns its current state.
 *
 * Returns %TRUE if the child widget is revealed.
 *
 * Returns: the current state of the expander
 */
gboolean
gtk_expander_get_expanded (GtkExpander *expander)
{
  g_return_val_if_fail (GTK_IS_EXPANDER (expander), FALSE);

  return expander->expanded;
}

/**
 * gtk_expander_set_label: (attributes org.gtk.Method.set_property=label)
 * @expander: a `GtkExpander`
 * @label: (nullable): a string
 *
 * Sets the text of the label of the expander to @label.
 *
 * This will also clear any previously set labels.
 */
void
gtk_expander_set_label (GtkExpander *expander,
                        const char *label)
{
  g_return_if_fail (GTK_IS_EXPANDER (expander));

  if (!label)
    {
      gtk_expander_set_label_widget (expander, NULL);
    }
  else
    {
      GtkWidget *child;

      child = gtk_label_new (label);
      gtk_label_set_use_underline (GTK_LABEL (child), expander->use_underline);
      gtk_label_set_use_markup (GTK_LABEL (child), expander->use_markup);

      gtk_expander_set_label_widget (expander, child);
    }

  g_object_notify (G_OBJECT (expander), "label");
}

/**
 * gtk_expander_get_label: (attributes org.gtk.Method.get_property=label)
 * @expander: a `GtkExpander`
 *
 * Fetches the text from a label widget.
 *
 * This is including any embedded underlines indicating mnemonics and
 * Pango markup, as set by [method@Gtk.Expander.set_label]. If the label
 * text has not been set the return value will be %NULL. This will be the
 * case if you create an empty button with gtk_button_new() to use as a
 * container.
 *
 * Returns: (nullable): The text of the label widget. This string is owned
 *   by the widget and must not be modified or freed.
 */
const char *
gtk_expander_get_label (GtkExpander *expander)
{
  g_return_val_if_fail (GTK_IS_EXPANDER (expander), NULL);

  if (GTK_IS_LABEL (expander->label_widget))
    return gtk_label_get_label (GTK_LABEL (expander->label_widget));
  else
    return NULL;
}

/**
 * gtk_expander_set_use_underline: (attributes org.gtk.Method.set_property=use-underline)
 * @expander: a `GtkExpander`
 * @use_underline: %TRUE if underlines in the text indicate mnemonics
 *
 * If true, an underline in the text indicates a mnemonic.
 */
void
gtk_expander_set_use_underline (GtkExpander *expander,
                                gboolean     use_underline)
{
  g_return_if_fail (GTK_IS_EXPANDER (expander));

  use_underline = use_underline != FALSE;

  if (expander->use_underline != use_underline)
    {
      expander->use_underline = use_underline;

      if (GTK_IS_LABEL (expander->label_widget))
        gtk_label_set_use_underline (GTK_LABEL (expander->label_widget), use_underline);

      g_object_notify (G_OBJECT (expander), "use-underline");
    }
}

/**
 * gtk_expander_get_use_underline: (attributes org.gtk.Method.get_property=use-underline)
 * @expander: a `GtkExpander`
 *
 * Returns whether an underline in the text indicates a mnemonic.
 *
 * Returns: %TRUE if an embedded underline in the expander
 *   label indicates the mnemonic accelerator keys
 */
gboolean
gtk_expander_get_use_underline (GtkExpander *expander)
{
  g_return_val_if_fail (GTK_IS_EXPANDER (expander), FALSE);

  return expander->use_underline;
}

/**
 * gtk_expander_set_use_markup: (attributes org.gtk.Method.set_property=use-markup)
 * @expander: a `GtkExpander`
 * @use_markup: %TRUE if the label’s text should be parsed for markup
 *
 * Sets whether the text of the label contains Pango markup.
 */
void
gtk_expander_set_use_markup (GtkExpander *expander,
                             gboolean     use_markup)
{
  g_return_if_fail (GTK_IS_EXPANDER (expander));

  use_markup = use_markup != FALSE;

  if (expander->use_markup != use_markup)
    {
      expander->use_markup = use_markup;

      if (GTK_IS_LABEL (expander->label_widget))
        gtk_label_set_use_markup (GTK_LABEL (expander->label_widget), use_markup);

      g_object_notify (G_OBJECT (expander), "use-markup");
    }
}

/**
 * gtk_expander_get_use_markup: (attributes org.gtk.Method.get_property=use-markup)
 * @expander: a `GtkExpander`
 *
 * Returns whether the label’s text is interpreted as Pango markup.
 *
 * Returns: %TRUE if the label’s text will be parsed for markup
 */
gboolean
gtk_expander_get_use_markup (GtkExpander *expander)
{
  g_return_val_if_fail (GTK_IS_EXPANDER (expander), FALSE);

  return expander->use_markup;
}

/**
 * gtk_expander_set_label_widget: (attributes org.gtk.Method.set_property=label-widget)
 * @expander: a `GtkExpander`
 * @label_widget: (nullable): the new label widget
 *
 * Set the label widget for the expander.
 *
 * This is the widget that will appear embedded alongside
 * the expander arrow.
 */
void
gtk_expander_set_label_widget (GtkExpander *expander,
                               GtkWidget   *label_widget)
{
  GtkWidget *widget;

  g_return_if_fail (GTK_IS_EXPANDER (expander));
  g_return_if_fail (label_widget == NULL || expander->label_widget == label_widget || gtk_widget_get_parent (label_widget) == NULL);

  if (expander->label_widget == label_widget)
    return;

  if (expander->label_widget)
    gtk_box_remove (GTK_BOX (expander->title_widget), expander->label_widget);

  expander->label_widget = label_widget;
  widget = GTK_WIDGET (expander);

  if (label_widget)
    {
      expander->label_widget = label_widget;

      gtk_box_append (GTK_BOX (expander->title_widget), label_widget);
    }

  if (gtk_widget_get_visible (widget))
    gtk_widget_queue_resize (widget);

  g_object_freeze_notify (G_OBJECT (expander));
  g_object_notify (G_OBJECT (expander), "label-widget");
  g_object_notify (G_OBJECT (expander), "label");
  g_object_thaw_notify (G_OBJECT (expander));
}

/**
 * gtk_expander_get_label_widget: (attributes org.gtk.Method.get_property=label-widget)
 * @expander: a `GtkExpander`
 *
 * Retrieves the label widget for the frame.
 *
 * Returns: (nullable) (transfer none): the label widget
 */
GtkWidget *
gtk_expander_get_label_widget (GtkExpander *expander)
{
  g_return_val_if_fail (GTK_IS_EXPANDER (expander), NULL);

  return expander->label_widget;
}

/**
 * gtk_expander_set_resize_toplevel: (attributes org.gtk.Method.set_property=resize-toplevel)
 * @expander: a `GtkExpander`
 * @resize_toplevel: whether to resize the toplevel
 *
 * Sets whether the expander will resize the toplevel widget
 * containing the expander upon resizing and collapsing.
 */
void
gtk_expander_set_resize_toplevel (GtkExpander *expander,
                                  gboolean     resize_toplevel)
{
  g_return_if_fail (GTK_IS_EXPANDER (expander));

  if (expander->resize_toplevel != resize_toplevel)
    {
      expander->resize_toplevel = resize_toplevel ? TRUE : FALSE;
      g_object_notify (G_OBJECT (expander), "resize-toplevel");
    }
}

/**
 * gtk_expander_get_resize_toplevel: (attributes org.gtk.Method.get_property=resize-toplevel)
 * @expander: a `GtkExpander`
 *
 * Returns whether the expander will resize the toplevel widget
 * containing the expander upon resizing and collapsing.
 *
 * Returns: the “resize toplevel” setting.
 */
gboolean
gtk_expander_get_resize_toplevel (GtkExpander *expander)
{
  g_return_val_if_fail (GTK_IS_EXPANDER (expander), FALSE);

  return expander->resize_toplevel;
}

/**
 * gtk_expander_set_child: (attributes org.gtk.Method.set_property=child)
 * @expander: a `GtkExpander`
 * @child: (nullable): the child widget
 *
 * Sets the child widget of @expander.
 */
void
gtk_expander_set_child (GtkExpander *expander,
                        GtkWidget   *child)
{
  g_return_if_fail (GTK_IS_EXPANDER (expander));
  g_return_if_fail (child == NULL || expander->child == child || gtk_widget_get_parent (child) == NULL);

  if (expander->child == child)
    return;

  if (expander->child)
    {
      if (!expander->expanded)
        g_object_unref (expander->child);
      else
        gtk_box_remove (GTK_BOX (expander->box), expander->child);
    }

  expander->child = child;

  if (expander->child)
    {
      /* We only add the child to the box if the expander is
       * expanded; otherwise we just claim ownership of the
       * child by sinking its floating reference, or acquiring
       * an additional reference to it. The reference will be
       * dropped once the expander is expanded
       */
      if (expander->expanded)
        {
          gtk_box_append (GTK_BOX (expander->box), expander->child);
          gtk_accessible_update_relation (GTK_ACCESSIBLE (expander),
                                          GTK_ACCESSIBLE_RELATION_CONTROLS, expander->child, NULL,
                                          -1);
        }
      else
        {
          gtk_accessible_reset_relation (GTK_ACCESSIBLE (expander),
                                         GTK_ACCESSIBLE_RELATION_CONTROLS);
          g_object_ref_sink (expander->child);
        }
    }
  else
    {
      gtk_accessible_reset_relation (GTK_ACCESSIBLE (expander),
                                     GTK_ACCESSIBLE_RELATION_CONTROLS);
    }

  g_object_notify (G_OBJECT (expander), "child");
}

/**
 * gtk_expander_get_child: (attributes org.gtk.Method.get_property=child)
 * @expander: a `GtkExpander`
 *
 * Gets the child widget of @expander.
 *
 * Returns: (nullable) (transfer none): the child widget of @expander
 */
GtkWidget *
gtk_expander_get_child (GtkExpander *expander)
{
  g_return_val_if_fail (GTK_IS_EXPANDER (expander), NULL);

  return expander->child;
}

