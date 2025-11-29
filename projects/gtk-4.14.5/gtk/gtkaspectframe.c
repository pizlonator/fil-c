/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * GtkAspectFrame: Ensure that the child window has a specified aspect ratio
 *    or, if obey_child, has the same aspect ratio as its requested size
 *
 *     Copyright Owen Taylor                          4/9/97
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
 * Modified by the GTK+ Team and others 1997-2001.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

/**
 * GtkAspectFrame:
 *
 * `GtkAspectFrame` preserves the aspect ratio of its child.
 *
 * The frame can respect the aspect ratio of the child widget,
 * or use its own aspect ratio.
 *
 * # CSS nodes
 *
 * `GtkAspectFrame` uses a CSS node with name `frame`.
 *
 * # Accessibility
 *
 * Until GTK 4.10, `GtkAspectFrame` used the `GTK_ACCESSIBLE_ROLE_GROUP` role.
 *
 * Starting from GTK 4.12, `GtkAspectFrame` uses the `GTK_ACCESSIBLE_ROLE_GENERIC` role.

 */

#include "config.h"

#include "gtkaspectframe.h"

#include "gtksizerequest.h"

#include "gtkbuildable.h"

#include "gtkwidgetprivate.h"
#include "gtkprivate.h"


typedef struct _GtkAspectFrameClass GtkAspectFrameClass;

struct _GtkAspectFrame
{
  GtkWidget parent_instance;

  GtkWidget    *child;
  gboolean      obey_child;
  float         xalign;
  float         yalign;
  float         ratio;
};

struct _GtkAspectFrameClass
{
  GtkWidgetClass parent_class;
};

enum {
  PROP_0,
  PROP_XALIGN,
  PROP_YALIGN,
  PROP_RATIO,
  PROP_OBEY_CHILD,
  PROP_CHILD
};

static void gtk_aspect_frame_dispose      (GObject         *object);
static void gtk_aspect_frame_set_property (GObject         *object,
                                           guint            prop_id,
                                           const GValue    *value,
                                           GParamSpec      *pspec);
static void gtk_aspect_frame_get_property (GObject         *object,
                                           guint            prop_id,
                                           GValue          *value,
                                           GParamSpec      *pspec);
static void gtk_aspect_frame_size_allocate (GtkWidget      *widget,
                                            int             width,
                                            int             height,
                                            int             baseline);
static void gtk_aspect_frame_measure       (GtkWidget      *widget,
                                            GtkOrientation  orientation,
                                            int             for_size,
                                            int             *minimum,
                                            int             *natural,
                                            int             *minimum_baseline,
                                            int             *natural_baseline);

static void gtk_aspect_frame_compute_expand (GtkWidget     *widget,
                                             gboolean      *hexpand,
                                             gboolean      *vexpand);
static GtkSizeRequestMode
            gtk_aspect_frame_get_request_mode (GtkWidget *widget);

static void gtk_aspect_frame_buildable_init (GtkBuildableIface *iface);

#define MAX_RATIO 10000.0
#define MIN_RATIO 0.0001


G_DEFINE_TYPE_WITH_CODE (GtkAspectFrame, gtk_aspect_frame, GTK_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                gtk_aspect_frame_buildable_init))


static void
gtk_aspect_frame_class_init (GtkAspectFrameClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  gobject_class->dispose = gtk_aspect_frame_dispose;
  gobject_class->set_property = gtk_aspect_frame_set_property;
  gobject_class->get_property = gtk_aspect_frame_get_property;

  widget_class->measure = gtk_aspect_frame_measure;
  widget_class->size_allocate = gtk_aspect_frame_size_allocate;
  widget_class->compute_expand = gtk_aspect_frame_compute_expand;
  widget_class->get_request_mode = gtk_aspect_frame_get_request_mode;

  /**
   * GtkAspectFrame:xalign: (attributes org.gtk.Property.get=gtk_aspect_frame_get_xalign org.gtk.Property.set=gtk_aspect_frame_set_xalign)
   *
   * The horizontal alignment of the child.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_XALIGN,
                                   g_param_spec_float ("xalign", NULL, NULL,
                                                       0.0, 1.0, 0.5,
                                                       GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));
  /**
   * GtkAspectFrame:yalign: (attributes org.gtk.Property.get=gtk_aspect_frame_get_yalign org.gtk.Property.set=gtk_aspect_frame_set_yalign)
   *
   * The vertical alignment of the child.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_YALIGN,
                                   g_param_spec_float ("yalign", NULL, NULL,
                                                       0.0, 1.0, 0.5,
                                                       GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));
  /**
   * GtkAspectFrame:ratio: (attributes org.gtk.Property.get=gtk_aspect_frame_get_ratio org.gtk.Property.set=gtk_aspect_frame_set_ratio)
   *
   * The aspect ratio to be used by the `GtkAspectFrame`.
   *
   * This property is only used if
   * [property@Gtk.AspectFrame:obey-child] is set to %FALSE.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_RATIO,
                                   g_param_spec_float ("ratio", NULL, NULL,
                                                       MIN_RATIO, MAX_RATIO, 1.0,
                                                       GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));
  /**
   * GtkAspectFrame:obey-child: (attributes org.gtk.Property.get=gtk_aspect_frame_get_obey_child org.gtk.Property.set=gtk_aspect_frame_set_obey_child)
   *
   * Whether the `GtkAspectFrame` should use the aspect ratio of its child.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_OBEY_CHILD,
                                   g_param_spec_boolean ("obey-child", NULL, NULL,
                                                         TRUE,
                                                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));
  /**
   * GtkAspectFrame:child: (attributes org.gtk.Property.get=gtk_aspect_frame_get_child org.gtk.Property.set=gtk_aspect_frame_set_child)
   *
   * The child widget.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_CHILD,
                                   g_param_spec_object ("child", NULL, NULL,
                                                        GTK_TYPE_WIDGET,
                                                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  gtk_widget_class_set_css_name (GTK_WIDGET_CLASS (class), I_("aspectframe"));
  gtk_widget_class_set_accessible_role (GTK_WIDGET_CLASS (class), GTK_ACCESSIBLE_ROLE_GENERIC);
}

static void
gtk_aspect_frame_init (GtkAspectFrame *self)
{
  self->xalign = 0.5;
  self->yalign = 0.5;
  self->ratio = 1.0;
  self->obey_child = TRUE;
}

static void
gtk_aspect_frame_dispose (GObject *object)
{
  GtkAspectFrame *self = GTK_ASPECT_FRAME (object);

  g_clear_pointer (&self->child, gtk_widget_unparent);

  G_OBJECT_CLASS (gtk_aspect_frame_parent_class)->dispose (object);
}

static void
gtk_aspect_frame_set_property (GObject         *object,
                               guint            prop_id,
                               const GValue    *value,
                               GParamSpec      *pspec)
{
  GtkAspectFrame *self = GTK_ASPECT_FRAME (object);

  switch (prop_id)
    {
      /* g_object_notify is handled by the _frame_set function */
    case PROP_XALIGN:
      gtk_aspect_frame_set_xalign (self, g_value_get_float (value));
      break;
    case PROP_YALIGN:
      gtk_aspect_frame_set_yalign (self, g_value_get_float (value));
      break;
    case PROP_RATIO:
      gtk_aspect_frame_set_ratio (self, g_value_get_float (value));
      break;
    case PROP_OBEY_CHILD:
      gtk_aspect_frame_set_obey_child (self, g_value_get_boolean (value));
      break;
    case PROP_CHILD:
      gtk_aspect_frame_set_child (self, g_value_get_object (value));
      break;
    default:
       G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_aspect_frame_get_property (GObject         *object,
                               guint            prop_id,
                               GValue          *value,
                               GParamSpec      *pspec)
{
  GtkAspectFrame *self = GTK_ASPECT_FRAME (object);

  switch (prop_id)
    {
    case PROP_XALIGN:
      g_value_set_float (value, self->xalign);
      break;
    case PROP_YALIGN:
      g_value_set_float (value, self->yalign);
      break;
    case PROP_RATIO:
      g_value_set_float (value, self->ratio);
      break;
    case PROP_OBEY_CHILD:
      g_value_set_boolean (value, self->obey_child);
      break;
    case PROP_CHILD:
      g_value_set_object (value, gtk_aspect_frame_get_child (self));
      break;
    default:
       G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static GtkBuildableIface *parent_buildable_iface;

static void
gtk_aspect_frame_buildable_add_child (GtkBuildable *buildable,
                                      GtkBuilder   *builder,
                                      GObject      *child,
                                      const char   *type)
{
  if (GTK_IS_WIDGET (child))
    gtk_aspect_frame_set_child (GTK_ASPECT_FRAME (buildable), GTK_WIDGET (child));
  else
    parent_buildable_iface->add_child (buildable, builder, child, type);
}

static void
gtk_aspect_frame_buildable_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);

  iface->add_child = gtk_aspect_frame_buildable_add_child;
}

/**
 * gtk_aspect_frame_new:
 * @xalign: Horizontal alignment of the child within the parent.
 *   Ranges from 0.0 (left aligned) to 1.0 (right aligned)
 * @yalign: Vertical alignment of the child within the parent.
 *   Ranges from 0.0 (top aligned) to 1.0 (bottom aligned)
 * @ratio: The desired aspect ratio.
 * @obey_child: If %TRUE, @ratio is ignored, and the aspect
 *   ratio is taken from the requistion of the child.
 *
 * Create a new `GtkAspectFrame`.
 *
 * Returns: the new `GtkAspectFrame`.
 */
GtkWidget *
gtk_aspect_frame_new (float    xalign,
                      float    yalign,
                      float    ratio,
                      gboolean obey_child)
{
  GtkAspectFrame *self;

  self = g_object_new (GTK_TYPE_ASPECT_FRAME, NULL);

  self->xalign = CLAMP (xalign, 0.0, 1.0);
  self->yalign = CLAMP (yalign, 0.0, 1.0);
  self->ratio = CLAMP (ratio, MIN_RATIO, MAX_RATIO);
  self->obey_child = obey_child != FALSE;

  return GTK_WIDGET (self);
}

/**
 * gtk_aspect_frame_set_xalign: (attributes org.gtk.Method.set_property=xalign)
 * @self: a `GtkAspectFrame`
 * @xalign: horizontal alignment, from 0.0 (left aligned) to 1.0 (right aligned)
 *
 * Sets the horizontal alignment of the child within the allocation
 * of the `GtkAspectFrame`.
 */
void
gtk_aspect_frame_set_xalign (GtkAspectFrame *self,
                             float           xalign)
{
  g_return_if_fail (GTK_IS_ASPECT_FRAME (self));

  xalign = CLAMP (xalign, 0.0, 1.0);

  if (self->xalign == xalign)
    return;

  self->xalign = xalign;

  g_object_notify (G_OBJECT (self), "xalign");
  gtk_widget_queue_resize (GTK_WIDGET (self));
}

/**
 * gtk_aspect_frame_get_xalign: (attributes org.gtk.Method.get_property=xalign)
 * @self: a `GtkAspectFrame`
 *
 * Returns the horizontal alignment of the child within the
 * allocation of the `GtkAspectFrame`.
 *
 * Returns: the horizontal alignment
 */
float
gtk_aspect_frame_get_xalign (GtkAspectFrame *self)
{
  g_return_val_if_fail (GTK_IS_ASPECT_FRAME (self), 0.5);

  return self->xalign;
}

/**
 * gtk_aspect_frame_set_yalign: (attributes org.gtk.Method.set_property=yalign)
 * @self: a `GtkAspectFrame`
 * @yalign: horizontal alignment, from 0.0 (top aligned) to 1.0 (bottom aligned)
 *
 * Sets the vertical alignment of the child within the allocation
 * of the `GtkAspectFrame`.
 */
void
gtk_aspect_frame_set_yalign (GtkAspectFrame *self,
                             float           yalign)
{
  g_return_if_fail (GTK_IS_ASPECT_FRAME (self));

  yalign = CLAMP (yalign, 0.0, 1.0);

  if (self->yalign == yalign)
    return;

  self->yalign = yalign;

  g_object_notify (G_OBJECT (self), "yalign");
  gtk_widget_queue_resize (GTK_WIDGET (self));
}

/**
 * gtk_aspect_frame_get_yalign: (attributes org.gtk.Method.get_property=yalign)
 * @self: a `GtkAspectFrame`
 *
 * Returns the vertical alignment of the child within the
 * allocation of the `GtkAspectFrame`.
 *
 * Returns: the vertical alignment
 */
float
gtk_aspect_frame_get_yalign (GtkAspectFrame *self)
{
  g_return_val_if_fail (GTK_IS_ASPECT_FRAME (self), 0.5);

  return self->yalign;
}

/**
 * gtk_aspect_frame_set_ratio: (attributes org.gtk.Method.set_property=ratio)
 * @self: a `GtkAspectFrame`
 * @ratio: aspect ratio of the child
 *
 * Sets the desired aspect ratio of the child.
 */
void
gtk_aspect_frame_set_ratio (GtkAspectFrame *self,
                            float           ratio)
{
  g_return_if_fail (GTK_IS_ASPECT_FRAME (self));

  ratio = CLAMP (ratio, MIN_RATIO, MAX_RATIO);

  if (self->ratio == ratio)
    return;

  self->ratio = ratio;

  g_object_notify (G_OBJECT (self), "ratio");
  gtk_widget_queue_resize (GTK_WIDGET (self));
}

/**
 * gtk_aspect_frame_get_ratio: (attributes org.gtk.Method.get_property=ratio)
 * @self: a `GtkAspectFrame`
 *
 * Returns the desired aspect ratio of the child.
 *
 * Returns: the desired aspect ratio
 */
float
gtk_aspect_frame_get_ratio (GtkAspectFrame *self)
{
  g_return_val_if_fail (GTK_IS_ASPECT_FRAME (self), 1.0);

  return self->ratio;
}

/**
 * gtk_aspect_frame_set_obey_child: (attributes org.gtk.Method.set_propery=obey-child)
 * @self: a `GtkAspectFrame`
 * @obey_child: If %TRUE, @ratio is ignored, and the aspect
 *    ratio is taken from the requisition of the child.
 *
 * Sets whether the aspect ratio of the child's size
 * request should override the set aspect ratio of
 * the `GtkAspectFrame`.
 */
void
gtk_aspect_frame_set_obey_child (GtkAspectFrame *self,
                                 gboolean        obey_child)
{
  g_return_if_fail (GTK_IS_ASPECT_FRAME (self));

  if (self->obey_child == obey_child)
    return;

  self->obey_child = obey_child;

  g_object_notify (G_OBJECT (self), "obey-child");
  gtk_widget_queue_resize (GTK_WIDGET (self));

}

/**
 * gtk_aspect_frame_get_obey_child: (attributes org.gtk.Method.get_property=obey-child)
 * @self: a `GtkAspectFrame`
 *
 * Returns whether the child's size request should override
 * the set aspect ratio of the `GtkAspectFrame`.
 *
 * Returns: whether to obey the child's size request
 */
gboolean
gtk_aspect_frame_get_obey_child (GtkAspectFrame *self)
{
  g_return_val_if_fail (GTK_IS_ASPECT_FRAME (self), TRUE);

  return self->obey_child;
}

static void
get_full_allocation (GtkAspectFrame *self,
                     GtkAllocation  *child_allocation)
{
  child_allocation->x = 0;
  child_allocation->y = 0;
  child_allocation->width = gtk_widget_get_width (GTK_WIDGET (self));
  child_allocation->height = gtk_widget_get_height (GTK_WIDGET (self));
}

static void
compute_child_allocation (GtkAspectFrame *self,
                          GtkAllocation  *child_allocation)
{
  double ratio;

  if (self->child && gtk_widget_get_visible (self->child))
    {
      GtkAllocation full_allocation;

      if (self->obey_child)
        {
          GtkRequisition child_requisition;

          gtk_widget_get_preferred_size (self->child, &child_requisition, NULL);
          if (child_requisition.height != 0)
            {
              ratio = ((double) child_requisition.width /
                       child_requisition.height);
              if (ratio < MIN_RATIO)
                ratio = MIN_RATIO;
            }
          else if (child_requisition.width != 0)
            ratio = MAX_RATIO;
          else
            ratio = 1.0;
        }
      else
        ratio = self->ratio;

      get_full_allocation (self, &full_allocation);

      if (ratio * full_allocation.height > full_allocation.width)
        {
          child_allocation->width = full_allocation.width;
          child_allocation->height = full_allocation.width / ratio + 0.5;
        }
      else
        {
          child_allocation->width = ratio * full_allocation.height + 0.5;
          child_allocation->height = full_allocation.height;
        }

      child_allocation->x = full_allocation.x + self->xalign * (full_allocation.width - child_allocation->width);
      child_allocation->y = full_allocation.y + self->yalign * (full_allocation.height - child_allocation->height);
    }
  else
    get_full_allocation (self, child_allocation);
}

static void
gtk_aspect_frame_measure (GtkWidget      *widget,
                          GtkOrientation  orientation,
                          int             for_size,
                          int             *minimum,
                          int             *natural,
                          int             *minimum_baseline,
                          int             *natural_baseline)
{
  GtkAspectFrame *self = GTK_ASPECT_FRAME (widget);

  if (self->child && gtk_widget_get_visible (self->child))
    {
      int child_min, child_nat;

      gtk_widget_measure (self->child,
                          orientation, for_size,
                          &child_min, &child_nat,
                          NULL, NULL);

      *minimum = child_min;
      *natural = child_nat;
    }
  else
    {
      *minimum = 0;
      *natural = 0;
    }
}

static void
gtk_aspect_frame_size_allocate (GtkWidget *widget,
                                int        width,
                                int        height,
                                int        baseline)
{
  GtkAspectFrame *self = GTK_ASPECT_FRAME (widget);
  GtkAllocation new_allocation;

  compute_child_allocation (self, &new_allocation);

  if (self->child && gtk_widget_get_visible (self->child))
    gtk_widget_size_allocate (self->child, &new_allocation, -1);
}

static void
gtk_aspect_frame_compute_expand (GtkWidget *widget,
                                 gboolean  *hexpand,
                                 gboolean  *vexpand)
{
  GtkAspectFrame *self = GTK_ASPECT_FRAME (widget);

  if (self->child)
    {
      *hexpand = gtk_widget_compute_expand (self->child, GTK_ORIENTATION_HORIZONTAL);
      *vexpand = gtk_widget_compute_expand (self->child, GTK_ORIENTATION_VERTICAL);
    }
  else
    {
      *hexpand = FALSE;
      *vexpand = FALSE;
    }
}

static GtkSizeRequestMode
gtk_aspect_frame_get_request_mode (GtkWidget *widget)
{
  GtkAspectFrame *self = GTK_ASPECT_FRAME (widget);

  if (self->child)
    return gtk_widget_get_request_mode (self->child);
  else
    return GTK_SIZE_REQUEST_CONSTANT_SIZE;
}

/**
 * gtk_aspect_frame_set_child: (attributes org.gtk.Method.set_property=child)
 * @self: a `GtkAspectFrame`
 * @child: (nullable): the child widget
 *
 * Sets the child widget of @self.
 */
void
gtk_aspect_frame_set_child (GtkAspectFrame  *self,
                            GtkWidget       *child)
{
  g_return_if_fail (GTK_IS_ASPECT_FRAME (self));
  g_return_if_fail (child == NULL || self->child == child || gtk_widget_get_parent (child) == NULL);

  if (self->child == child)
    return;

  g_clear_pointer (&self->child, gtk_widget_unparent);

  if (child)
    {
      self->child = child;
      gtk_widget_set_parent (child, GTK_WIDGET (self));
    }

  g_object_notify (G_OBJECT (self), "child");
}

/**
 * gtk_aspect_frame_get_child: (attributes org.gtk.Method.get_property=child)
 * @self: a `GtkAspectFrame`
 *
 * Gets the child widget of @self.
 *
 * Returns: (nullable) (transfer none): the child widget of @self
 */
GtkWidget *
gtk_aspect_frame_get_child (GtkAspectFrame *self)
{
  g_return_val_if_fail (GTK_IS_ASPECT_FRAME (self), NULL);

  return self->child;
}
