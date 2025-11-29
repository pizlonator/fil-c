/* gtkfixedlayout.c: Fixed positioning layout manager
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Copyright 2019 GNOME Foundation
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

/**
 * GtkFixedLayout:
 *
 * `GtkFixedLayout` is a layout manager which can place child widgets
 * at fixed positions.
 *
 * Most applications should never use this layout manager; fixed positioning
 * and sizing requires constant recalculations on where children need to be
 * positioned and sized. Other layout managers perform this kind of work
 * internally so that application developers don't need to do it. Specifically,
 * widgets positioned in a fixed layout manager will need to take into account:
 *
 * - Themes, which may change widget sizes.
 *
 * - Fonts other than the one you used to write the app will of course
 *   change the size of widgets containing text; keep in mind that
 *   users may use a larger font because of difficulty reading the
 *   default, or they may be using a different OS that provides different
 *   fonts.
 *
 * - Translation of text into other languages changes its size. Also,
 *   display of non-English text will use a different font in many
 *   cases.
 *
 * In addition, `GtkFixedLayout` does not pay attention to text direction and
 * thus may produce unwanted results if your app is run under right-to-left
 * languages such as Hebrew or Arabic. That is: normally GTK will order
 * containers appropriately depending on the text direction, e.g. to put labels
 * to the right of the thing they label when using an RTL language;
 * `GtkFixedLayout` won't be able to do that for you.
 *
 * Finally, fixed positioning makes it kind of annoying to add/remove UI
 * elements, since you have to reposition all the other  elements. This is a
 * long-term maintenance problem for your application.
 */

/**
 * GtkFixedLayoutChild:
 *
 * `GtkLayoutChild` subclass for children in a `GtkFixedLayout`.
 */

#include "config.h"

#include "gtkfixedlayout.h"

#include "gtklayoutchild.h"
#include "gtkprivate.h"
#include "gtkwidgetprivate.h"

#include <graphene-gobject.h>

struct _GtkFixedLayout
{
  GtkLayoutManager parent_instance;
};

struct _GtkFixedLayoutChild
{
  GtkLayoutChild parent_instance;

  GskTransform *transform;
};

enum
{
  PROP_CHILD_TRANSFORM = 1,

  N_CHILD_PROPERTIES
};

static GParamSpec *child_props[N_CHILD_PROPERTIES];

G_DEFINE_TYPE (GtkFixedLayoutChild, gtk_fixed_layout_child, GTK_TYPE_LAYOUT_CHILD)

static void
gtk_fixed_layout_child_set_property (GObject      *gobject,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  GtkFixedLayoutChild *self = GTK_FIXED_LAYOUT_CHILD (gobject);

  switch (prop_id)
    {
    case PROP_CHILD_TRANSFORM:
      gtk_fixed_layout_child_set_transform (self, g_value_get_boxed (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
gtk_fixed_layout_child_get_property (GObject    *gobject,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  GtkFixedLayoutChild *self = GTK_FIXED_LAYOUT_CHILD (gobject);

  switch (prop_id)
    {
    case PROP_CHILD_TRANSFORM:
      g_value_set_boxed (value, &self->transform);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
gtk_fixed_layout_child_finalize (GObject *gobject)
{
  GtkFixedLayoutChild *self = GTK_FIXED_LAYOUT_CHILD (gobject);

  gsk_transform_unref (self->transform);

  G_OBJECT_CLASS (gtk_fixed_layout_child_parent_class)->finalize (gobject);
}

static void
gtk_fixed_layout_child_class_init (GtkFixedLayoutChildClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = gtk_fixed_layout_child_set_property;
  gobject_class->get_property = gtk_fixed_layout_child_get_property;
  gobject_class->finalize = gtk_fixed_layout_child_finalize;

  /**
   * GtkFixedLayoutChild:transform: (attributes org.gtk.Property.get=gtk_fixed_layout_child_get_transform org.gtk.Property.set=gtk_fixed_layout_child_set_transform)
   *
   * The transform of the child.
   */
  child_props[PROP_CHILD_TRANSFORM] =
    g_param_spec_boxed ("transform", NULL, NULL,
                        GSK_TYPE_TRANSFORM,
                        G_PARAM_READWRITE |
                        G_PARAM_STATIC_STRINGS |
                        G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, N_CHILD_PROPERTIES, child_props);
}

static void
gtk_fixed_layout_child_init (GtkFixedLayoutChild *self)
{
}

/**
 * gtk_fixed_layout_child_set_transform: (attributes org.gtk.Method.set_property=transform)
 * @child: a `GtkFixedLayoutChild`
 * @transform: a `GskTransform`
 *
 * Sets the transformation of the child of a `GtkFixedLayout`.
 */
void
gtk_fixed_layout_child_set_transform (GtkFixedLayoutChild *child,
                                      GskTransform        *transform)
{
  GtkLayoutManager *layout;

  g_return_if_fail (GTK_IS_FIXED_LAYOUT_CHILD (child));

  gsk_transform_unref (child->transform);
  child->transform = gsk_transform_ref (transform);

  layout = gtk_layout_child_get_layout_manager (GTK_LAYOUT_CHILD (child));
  gtk_layout_manager_layout_changed (layout);

  g_object_notify_by_pspec (G_OBJECT (child), child_props[PROP_CHILD_TRANSFORM]);
}

/**
 * gtk_fixed_layout_child_get_transform:
 * @child: a `GtkFixedLayoutChild`
 *
 * Retrieves the transformation of the child.
 *
 * Returns: (transfer none) (nullable): a `GskTransform`
 */
GskTransform *
gtk_fixed_layout_child_get_transform (GtkFixedLayoutChild *child)
{
  g_return_val_if_fail (GTK_IS_FIXED_LAYOUT_CHILD (child), NULL);

  return child->transform;
}

G_DEFINE_TYPE (GtkFixedLayout, gtk_fixed_layout, GTK_TYPE_LAYOUT_MANAGER)

static GtkSizeRequestMode
gtk_fixed_layout_get_request_mode (GtkLayoutManager *layout_manager,
                                   GtkWidget        *widget)
{
  return GTK_SIZE_REQUEST_CONSTANT_SIZE;
}

static void
gtk_fixed_layout_measure (GtkLayoutManager *layout_manager,
                          GtkWidget        *widget,
                          GtkOrientation    orientation,
                          int               for_size,
                          int              *minimum,
                          int              *natural,
                          int              *minimum_baseline,
                          int              *natural_baseline)
{
  GtkFixedLayoutChild *child_info;
  GtkWidget *child;
  int minimum_size = 0;
  int natural_size = 0;

  for (child = _gtk_widget_get_first_child (widget);
       child != NULL;
       child = _gtk_widget_get_next_sibling (child))
    {
      int child_min = 0, child_nat = 0;
      int child_min_opp = 0, child_nat_opp = 0;
      graphene_rect_t min_rect, nat_rect;

      if (!gtk_widget_should_layout (child))
        continue;

      child_info = GTK_FIXED_LAYOUT_CHILD (gtk_layout_manager_get_layout_child (layout_manager, child));

      gtk_widget_measure (child, orientation, -1,
                          &child_min, &child_nat,
                          NULL, NULL);
      gtk_widget_measure (child, OPPOSITE_ORIENTATION (orientation), -1,
                          &child_min_opp, &child_nat_opp,
                          NULL, NULL);

      min_rect.origin.x = min_rect.origin.y = 0;
      nat_rect.origin.x = nat_rect.origin.y = 0;
      if (orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          min_rect.size.width = child_min;
          min_rect.size.height = child_min_opp;
          nat_rect.size.width = child_nat;
          nat_rect.size.height = child_nat_opp;
        }
      else
       {
          min_rect.size.width = child_min_opp;
          min_rect.size.height = child_min;
          nat_rect.size.width = child_nat_opp;
          nat_rect.size.height = child_nat;
       }

      gsk_transform_transform_bounds (child_info->transform, &min_rect, &min_rect);
      gsk_transform_transform_bounds (child_info->transform, &nat_rect, &nat_rect);

      if (orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          minimum_size = MAX (minimum_size, min_rect.origin.x + min_rect.size.width);
          natural_size = MAX (natural_size, nat_rect.origin.x + nat_rect.size.width);
        }
      else
        {
          minimum_size = MAX (minimum_size, min_rect.origin.y + min_rect.size.height);
          natural_size = MAX (natural_size, nat_rect.origin.y + nat_rect.size.height);
        }
    }

  if (minimum != NULL)
    *minimum = minimum_size;
  if (natural != NULL)
    *natural = natural_size;
}

static void
gtk_fixed_layout_allocate (GtkLayoutManager *layout_manager,
                           GtkWidget        *widget,
                           int               width,
                           int               height,
                           int               baseline)
{
  GtkFixedLayoutChild *child_info;
  GtkWidget *child;

  for (child = _gtk_widget_get_first_child (widget);
       child != NULL;
       child = _gtk_widget_get_next_sibling (child))
    {
      GtkRequisition child_req;

      if (!gtk_widget_should_layout (child))
        continue;

      child_info = GTK_FIXED_LAYOUT_CHILD (gtk_layout_manager_get_layout_child (layout_manager, child));
      gtk_widget_get_preferred_size (child, &child_req, NULL);

      gtk_widget_allocate (child,
                           child_req.width,
                           child_req.height,
                           -1,
                           gsk_transform_ref (child_info->transform));
    }
}

static GtkLayoutChild *
gtk_fixed_layout_create_layout_child (GtkLayoutManager *manager,
                                      GtkWidget        *widget,
                                      GtkWidget        *for_child)
{
  return g_object_new (GTK_TYPE_FIXED_LAYOUT_CHILD,
                       "layout-manager", manager,
                       "child-widget", for_child,
                       NULL);
}

static void
gtk_fixed_layout_class_init (GtkFixedLayoutClass *klass)
{
  GtkLayoutManagerClass *layout_class = GTK_LAYOUT_MANAGER_CLASS (klass);

  layout_class->layout_child_type = GTK_TYPE_FIXED_LAYOUT_CHILD;

  layout_class->get_request_mode = gtk_fixed_layout_get_request_mode;
  layout_class->measure = gtk_fixed_layout_measure;
  layout_class->allocate = gtk_fixed_layout_allocate;
  layout_class->create_layout_child = gtk_fixed_layout_create_layout_child;
}

static void
gtk_fixed_layout_init (GtkFixedLayout *self)
{
}

/**
 * gtk_fixed_layout_new:
 *
 * Creates a new `GtkFixedLayout`.
 *
 * Returns: the newly created `GtkFixedLayout`
 */
GtkLayoutManager *
gtk_fixed_layout_new (void)
{
  return g_object_new (GTK_TYPE_FIXED_LAYOUT, NULL);
}
