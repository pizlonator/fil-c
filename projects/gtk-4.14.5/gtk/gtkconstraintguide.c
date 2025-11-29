/* gtkconstraintguide.c: Flexible space for constraints
 * Copyright 2019 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Matthias Clasen
 */

/**
 * GtkConstraintGuide:
 *
 * A `GtkConstraintGuide` is an invisible layout element in a
 * `GtkConstraintLayout`.
 *
 * The `GtkConstraintLayout` treats guides like widgets. They
 * can be used as the source or target of a `GtkConstraint`.
 *
 * Guides have a minimum, maximum and natural size. Depending
 * on the constraints that are applied, they can act like a
 * guideline that widgets can be aligned to, or like *flexible
 * space*.
 *
 * Unlike a `GtkWidget`, a `GtkConstraintGuide` will not be drawn.
 */

#include "config.h"

#include "gtkconstraintguide.h"

#include "gtkconstraintguideprivate.h"
#include "gtkconstraintlayoutprivate.h"
#include "gtkconstraintexpressionprivate.h"
#include "gtkconstraintsolverprivate.h"

#include "gtkdebug.h"
#include "gtkprivate.h"


typedef enum {
  MIN_WIDTH,
  MIN_HEIGHT,
  NAT_WIDTH,
  NAT_HEIGHT,
  MAX_WIDTH,
  MAX_HEIGHT,
  LAST_VALUE
} GuideValue;

struct _GtkConstraintGuide
{ 
  GObject parent_instance;

  char *name;

  int strength;

  int values[LAST_VALUE];

  GtkConstraintLayout *layout;

  /* HashTable<static string, Variable>; a hash table of variables,
   * one for each attribute; we use these to query and suggest the
   * values for the solver. The string is static and does not need
   * to be freed.
   */
  GHashTable *bound_attributes;

  GtkConstraintRef *constraints[LAST_VALUE];
};


struct _GtkConstraintGuideClass {
  GObjectClass parent_class;
};

enum {
  PROP_MIN_WIDTH = 1,
  PROP_MIN_HEIGHT,
  PROP_NAT_WIDTH,
  PROP_NAT_HEIGHT,
  PROP_MAX_WIDTH,
  PROP_MAX_HEIGHT,
  PROP_STRENGTH,
  PROP_NAME,
  LAST_PROP
};

static GParamSpec *guide_props[LAST_PROP];

static void
gtk_constraint_guide_constraint_target_iface_init (GtkConstraintTargetInterface *iface)
{
}

G_DEFINE_TYPE_WITH_CODE (GtkConstraintGuide, gtk_constraint_guide, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_CONSTRAINT_TARGET,
                                                gtk_constraint_guide_constraint_target_iface_init))

static void
gtk_constraint_guide_init (GtkConstraintGuide *guide)
{
  guide->strength = GTK_CONSTRAINT_STRENGTH_MEDIUM;

  guide->values[MIN_WIDTH] = 0;
  guide->values[MIN_HEIGHT] = 0;
  guide->values[NAT_WIDTH] = 0;
  guide->values[NAT_HEIGHT] = 0;
  guide->values[MAX_WIDTH] = G_MAXINT;
  guide->values[MAX_HEIGHT] = G_MAXINT;

  guide->bound_attributes =
    g_hash_table_new_full (g_str_hash, g_str_equal,
                           NULL,
                           (GDestroyNotify) gtk_constraint_variable_unref);
}

static void
gtk_constraint_guide_update_constraint (GtkConstraintGuide *guide,
                                        GuideValue          index)
{
  GtkConstraintSolver *solver;
  GtkConstraintVariable *var;

  if (!guide->layout)
    return;

  solver = gtk_constraint_layout_get_solver (guide->layout);
  if (!solver)
    return;

  if (guide->constraints[index] != NULL)
    {
      gtk_constraint_solver_remove_constraint (solver, guide->constraints[index]);
      guide->constraints[index] = NULL;
    }

  if (index == MIN_WIDTH || index == NAT_WIDTH || index == MAX_WIDTH)
    var = gtk_constraint_layout_get_attribute (guide->layout, GTK_CONSTRAINT_ATTRIBUTE_WIDTH, "guide", NULL, guide->bound_attributes);
  else
    var = gtk_constraint_layout_get_attribute (guide->layout, GTK_CONSTRAINT_ATTRIBUTE_HEIGHT, "guide", NULL, guide->bound_attributes);

  /* We always install min-size constraints,
   * but we avoid nat-size constraints if min == max
   * and we avoid max-size constraints if max == G_MAXINT
   */
  if (index == MIN_WIDTH || index == MIN_HEIGHT)
    {
      guide->constraints[index] =
        gtk_constraint_solver_add_constraint (solver,
                                              var,
                                              GTK_CONSTRAINT_RELATION_GE,
                                              gtk_constraint_expression_new (guide->values[index]),
                                              GTK_CONSTRAINT_STRENGTH_REQUIRED);
    }
  else if ((index == NAT_WIDTH && guide->values[MIN_WIDTH] != guide->values[MAX_WIDTH]) ||
      (index == NAT_HEIGHT && guide->values[MIN_HEIGHT] != guide->values[MAX_HEIGHT]))
    {
      gtk_constraint_variable_set_value (var, guide->values[index]);
      guide->constraints[index] =
        gtk_constraint_solver_add_stay_variable (solver,
                                                 var,
                                                 guide->strength);
    }
  else if ((index == MAX_WIDTH || index == MAX_HEIGHT) &&
           guide->values[index] < G_MAXINT)
    {
      guide->constraints[index] =
        gtk_constraint_solver_add_constraint (solver,
                                              var,
                                              GTK_CONSTRAINT_RELATION_LE,
                                              gtk_constraint_expression_new (guide->values[index]),
                                              GTK_CONSTRAINT_STRENGTH_REQUIRED);
    }

  gtk_layout_manager_layout_changed (GTK_LAYOUT_MANAGER (guide->layout));
}

void
gtk_constraint_guide_update (GtkConstraintGuide *guide)
{
  int i;

  for (i = 0; i < LAST_VALUE; i++)
    gtk_constraint_guide_update_constraint (guide, i);
}

void
gtk_constraint_guide_detach (GtkConstraintGuide *guide)
{
  GtkConstraintSolver *solver;
  int i;

  if (!guide->layout)
    return;

  solver = gtk_constraint_layout_get_solver (guide->layout);
  if (!solver)
    return;

  for (i = 0; i < LAST_VALUE; i++)
    {
      if (guide->constraints[i])
        {
          gtk_constraint_solver_remove_constraint (solver, guide->constraints[i]);
          guide->constraints[i] = NULL;
        }
    }

  g_hash_table_remove_all (guide->bound_attributes);
}

GtkConstraintVariable *
gtk_constraint_guide_get_attribute (GtkConstraintGuide     *guide,
                                    GtkConstraintAttribute  attr)
{
  GtkLayoutManager *manager = GTK_LAYOUT_MANAGER (guide->layout);
  GtkWidget *widget = gtk_layout_manager_get_widget (manager);

  return gtk_constraint_layout_get_attribute (guide->layout, attr, "guide", widget, guide->bound_attributes);
}

GtkConstraintLayout *
gtk_constraint_guide_get_layout (GtkConstraintGuide *guide)
{
  return guide->layout;
}

void
gtk_constraint_guide_set_layout (GtkConstraintGuide  *guide,
                                 GtkConstraintLayout *layout)
{
  guide->layout = layout;
}

static void
gtk_constraint_guide_set_property (GObject      *gobject,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  GtkConstraintGuide *self = GTK_CONSTRAINT_GUIDE (gobject);
  int val;
  GuideValue index;

  switch (prop_id)
    {
    case PROP_MIN_WIDTH:
    case PROP_MIN_HEIGHT:
    case PROP_NAT_WIDTH:
    case PROP_NAT_HEIGHT:
    case PROP_MAX_WIDTH:
    case PROP_MAX_HEIGHT:
      val = g_value_get_int (value);
      index = prop_id - 1;
      if (self->values[index] != val)
        {
          self->values[index] = val;
          g_object_notify_by_pspec (gobject, pspec);

          gtk_constraint_guide_update_constraint (self, index);
          if (index == MIN_WIDTH || index == MAX_WIDTH)
            gtk_constraint_guide_update_constraint (self, NAT_WIDTH);
          if (index == MIN_HEIGHT || index == MAX_HEIGHT)
            gtk_constraint_guide_update_constraint (self, NAT_HEIGHT);
        }
      break;

    case PROP_STRENGTH:
      gtk_constraint_guide_set_strength (self, g_value_get_enum (value));
      break;

    case PROP_NAME:
      gtk_constraint_guide_set_name (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
gtk_constraint_guide_get_property (GObject    *gobject,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  GtkConstraintGuide *self = GTK_CONSTRAINT_GUIDE (gobject);

  switch (prop_id)
    {
    case PROP_MIN_WIDTH:
    case PROP_MIN_HEIGHT:
    case PROP_NAT_WIDTH:
    case PROP_NAT_HEIGHT:
    case PROP_MAX_WIDTH:
    case PROP_MAX_HEIGHT:
      g_value_set_int (value, self->values[prop_id - 1]);
      break;

    case PROP_STRENGTH:
      g_value_set_enum (value, self->strength);
      break;

    case PROP_NAME:
      g_value_set_string (value, self->name);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
gtk_constraint_guide_finalize (GObject *object)
{
  GtkConstraintGuide *self = GTK_CONSTRAINT_GUIDE (object);

  g_free (self->name);

  g_clear_pointer (&self->bound_attributes, g_hash_table_unref);

  G_OBJECT_CLASS (gtk_constraint_guide_parent_class)->finalize (object);
}

static void
gtk_constraint_guide_class_init (GtkConstraintGuideClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = gtk_constraint_guide_finalize;
  object_class->set_property = gtk_constraint_guide_set_property;
  object_class->get_property = gtk_constraint_guide_get_property;

  /**
   * GtkConstraintGuide:min-width:
   *
   * The minimum width of the guide.
   */
  guide_props[PROP_MIN_WIDTH] =
      g_param_spec_int ("min-width", NULL, NULL,
                        0, G_MAXINT, 0,
                        G_PARAM_READWRITE|
                        G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkConstraintGuide:min-height:
   *
   * The minimum height of the guide.
   */
  guide_props[PROP_MIN_HEIGHT] =
      g_param_spec_int ("min-height", NULL, NULL,
                        0, G_MAXINT, 0,
                        G_PARAM_READWRITE|
                        G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkConstraintGuide:nat-width:
   *
   * The preferred, or natural, width of the guide.
   */
  guide_props[PROP_NAT_WIDTH] =
      g_param_spec_int ("nat-width", NULL, NULL,
                        0, G_MAXINT, 0,
                        G_PARAM_READWRITE|
                        G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkConstraintGuide:nat-height:
   *
   * The preferred, or natural, height of the guide.
   */
  guide_props[PROP_NAT_HEIGHT] =
      g_param_spec_int ("nat-height", NULL, NULL,
                        0, G_MAXINT, 0,
                        G_PARAM_READWRITE|
                        G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkConstraintGuide:max-width:
   *
   * The maximum width of the guide.
   */
  guide_props[PROP_MAX_WIDTH] =
      g_param_spec_int ("max-width", NULL, NULL,
                        0, G_MAXINT, G_MAXINT,
                        G_PARAM_READWRITE|
                        G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkConstraintGuide:max-height:
   *
   * The maximum height of the guide.
   */
  guide_props[PROP_MAX_HEIGHT] =
      g_param_spec_int ("max-height", NULL, NULL,
                        0, G_MAXINT, G_MAXINT,
                        G_PARAM_READWRITE|
                        G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkConstraintGuide:strength: (attributes org.gtk.Property.get=gtk_constraint_guide_get_strength org.gtk.Property.set=gtk_constraint_guide_set_strength)
   *
   * The `GtkConstraintStrength` to be used for the constraint on
   * the natural size of the guide.
   */
  guide_props[PROP_STRENGTH] =
      g_param_spec_enum ("strength", NULL, NULL,
                         GTK_TYPE_CONSTRAINT_STRENGTH,
                         GTK_CONSTRAINT_STRENGTH_MEDIUM,
                         G_PARAM_READWRITE|
                         G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkConstraintGuide:name: (attributes org.gtk.Property.get=gtk_constraint_guide_get_name org.gtk.Property.set=gtk_constraint_guide_set_name)
   *
   * A name that identifies the `GtkConstraintGuide`, for debugging.
   */
  guide_props[PROP_NAME] =
      g_param_spec_string ("name", NULL, NULL,
                           NULL,
                           G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, LAST_PROP, guide_props);
}

/**
 * gtk_constraint_guide_new:
 *
 * Creates a new `GtkConstraintGuide` object.
 *
 * Return: a new `GtkConstraintGuide` object.
 */
GtkConstraintGuide *
gtk_constraint_guide_new (void)
{
  return g_object_new (GTK_TYPE_CONSTRAINT_GUIDE, NULL);
}

/**
 * gtk_constraint_guide_set_min_size:
 * @guide: a `GtkConstraintGuide` object
 * @width: the new minimum width, or -1 to not change it
 * @height: the new minimum height, or -1 to not change it
 *
 * Sets the minimum size of @guide.
 *
 * If @guide is attached to a `GtkConstraintLayout`,
 * the constraints will be updated to reflect the new size.
 */
void
gtk_constraint_guide_set_min_size (GtkConstraintGuide *guide,
                                   int                 width,
                                   int                 height)
{
  g_return_if_fail (GTK_IS_CONSTRAINT_GUIDE (guide));
  g_return_if_fail (width >= -1);
  g_return_if_fail (height >= -1);

  g_object_freeze_notify (G_OBJECT (guide));

  if (width != -1)
    g_object_set (guide, "min-width", width, NULL);

  if (height != -1)
    g_object_set (guide, "min-height", height, NULL);

  g_object_thaw_notify (G_OBJECT (guide));
}

/**
 * gtk_constraint_guide_get_min_size:
 * @guide: a `GtkConstraintGuide` object
 * @width: (out) (optional): return location for the minimum width
 * @height: (out) (optional): return location for the minimum height
 *
 * Gets the minimum size of @guide.
 */
void
gtk_constraint_guide_get_min_size (GtkConstraintGuide *guide,
                                   int                *width,
                                   int                *height)
{
  g_return_if_fail (GTK_IS_CONSTRAINT_GUIDE (guide));

  if (width)
    *width = guide->values[MIN_WIDTH];
  if (height)
    *height = guide->values[MIN_HEIGHT];
}

/**
 * gtk_constraint_guide_set_nat_size:
 * @guide: a `GtkConstraintGuide` object
 * @width: the new natural width, or -1 to not change it
 * @height: the new natural height, or -1 to not change it
 *
 * Sets the natural size of @guide.
 *
 * If @guide is attached to a `GtkConstraintLayout`,
 * the constraints will be updated to reflect the new size.
 */
void
gtk_constraint_guide_set_nat_size (GtkConstraintGuide *guide,
                                   int                 width,
                                   int                 height)
{
  g_return_if_fail (GTK_IS_CONSTRAINT_GUIDE (guide));
  g_return_if_fail (width >= -1);
  g_return_if_fail (height >= -1);

  g_object_freeze_notify (G_OBJECT (guide));

  if (width != -1)
    g_object_set (guide, "nat-width", width, NULL);

  if (height != -1)
    g_object_set (guide, "nat-height", height, NULL);

  g_object_thaw_notify (G_OBJECT (guide));
}

/**
 * gtk_constraint_guide_get_nat_size:
 * @guide: a `GtkConstraintGuide` object
 * @width: (out) (optional): return location for the natural width
 * @height: (out) (optional): return location for the natural height
 *
 * Gets the natural size of @guide.
 */
void
gtk_constraint_guide_get_nat_size (GtkConstraintGuide *guide,
                                   int                *width,
                                   int                *height)
{
  g_return_if_fail (GTK_IS_CONSTRAINT_GUIDE (guide));

  if (width)
    *width = guide->values[NAT_WIDTH];
  if (height)
    *height = guide->values[NAT_HEIGHT];
}

/**
 * gtk_constraint_guide_set_max_size:
 * @guide: a `GtkConstraintGuide` object
 * @width: the new maximum width, or -1 to not change it
 * @height: the new maximum height, or -1 to not change it
 *
 * Sets the maximum size of @guide.
 *
 * If @guide is attached to a `GtkConstraintLayout`,
 * the constraints will be updated to reflect the new size.
 */
void
gtk_constraint_guide_set_max_size (GtkConstraintGuide *guide,
                                   int                 width,
                                   int                 height)
{
  g_return_if_fail (GTK_IS_CONSTRAINT_GUIDE (guide));
  g_return_if_fail (width >= -1);
  g_return_if_fail (height >= -1);

  g_object_freeze_notify (G_OBJECT (guide));

  if (width != -1)
    g_object_set (guide, "max-width", width, NULL);

  if (height != -1)
    g_object_set (guide, "max-height", height, NULL);

  g_object_thaw_notify (G_OBJECT (guide));
}

/**
 * gtk_constraint_guide_get_max_size:
 * @guide: a `GtkConstraintGuide` object
 * @width: (out) (optional): return location for the maximum width
 * @height: (out) (optional): return location for the maximum height
 *
 * Gets the maximum size of @guide.
 */
void
gtk_constraint_guide_get_max_size (GtkConstraintGuide *guide,
                                   int                *width,
                                   int                *height)
{
  g_return_if_fail (GTK_IS_CONSTRAINT_GUIDE (guide));

  if (width)
    *width = guide->values[MAX_WIDTH];
  if (height)
    *height = guide->values[MAX_HEIGHT];
}

/**
 * gtk_constraint_guide_get_name: (attributes org.gtk.Method.get_property=name)
 * @guide: a `GtkConstraintGuide`
 *
 * Retrieves the name set using gtk_constraint_guide_set_name().
 *
 * Returns: (transfer none) (nullable): the name of the guide
 */
const char *
gtk_constraint_guide_get_name (GtkConstraintGuide *guide)
{
  g_return_val_if_fail (GTK_IS_CONSTRAINT_GUIDE (guide), NULL);

  return guide->name;
}

/**
 * gtk_constraint_guide_set_name: (attributes org.gtk.Method.set_property=name)
 * @guide: a `GtkConstraintGuide`
 * @name: (nullable): a name for the @guide
 *
 * Sets a name for the given `GtkConstraintGuide`.
 *
 * The name is useful for debugging purposes.
 */
void
gtk_constraint_guide_set_name (GtkConstraintGuide *guide,
                               const char         *name)
{
  g_return_if_fail (GTK_IS_CONSTRAINT_GUIDE (guide));

  g_free (guide->name);
  guide->name = g_strdup (name);
  g_object_notify_by_pspec (G_OBJECT (guide), guide_props[PROP_NAME]);
}

/**
 * gtk_constraint_guide_get_strength: (attributes org.gtk.Method.get_property=strength)
 * @guide: a `GtkConstraintGuide`
 *
 * Retrieves the strength set using gtk_constraint_guide_set_strength().
 *
 * Returns: the strength of the constraint on the natural size
 */
GtkConstraintStrength
gtk_constraint_guide_get_strength (GtkConstraintGuide *guide)
{
  g_return_val_if_fail (GTK_IS_CONSTRAINT_GUIDE (guide),
                        GTK_CONSTRAINT_STRENGTH_MEDIUM);

  return guide->strength;
}

/**
 * gtk_constraint_guide_set_strength: (attributes org.gtk.Method.set_property=strength)
 * @guide: a `GtkConstraintGuide`
 * @strength: the strength of the constraint
 *
 * Sets the strength of the constraint on the natural size of the
 * given `GtkConstraintGuide`.
 */
void
gtk_constraint_guide_set_strength (GtkConstraintGuide    *guide,
                                   GtkConstraintStrength  strength)
{
  g_return_if_fail (GTK_IS_CONSTRAINT_GUIDE (guide));

  if (guide->strength == strength)
    return;

  guide->strength = strength;
  g_object_notify_by_pspec (G_OBJECT (guide), guide_props[PROP_STRENGTH]);
  gtk_constraint_guide_update_constraint (guide, NAT_WIDTH);
  gtk_constraint_guide_update_constraint (guide, NAT_HEIGHT);
}
