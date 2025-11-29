/* gtkconstraint.c: Constraint between two widgets
 * Copyright 2019  GNOME Foundation
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
 * Author: Emmanuele Bassi
 */

/**
 * GtkConstraint:
 *
 * `GtkConstraint` describes a constraint between attributes of two widgets,
 *  expressed as a linear equation.
 *
 * The typical equation for a constraint is:
 *
 * ```
 *   target.target_attr = source.source_attr × multiplier + constant
 * ```
 *
 * Each `GtkConstraint` is part of a system that will be solved by a
 * [class@Gtk.ConstraintLayout] in order to allocate and position each
 * child widget or guide.
 *
 * The source and target, as well as their attributes, of a `GtkConstraint`
 * instance are immutable after creation.
 */

#include "config.h"

#include "gtkconstraintprivate.h"
#include "gtkconstraintsolverprivate.h"
#include "gtktypebuiltins.h"
#include "gtkwidget.h"

enum {
  PROP_TARGET = 1,
  PROP_TARGET_ATTRIBUTE,
  PROP_RELATION,
  PROP_SOURCE,
  PROP_SOURCE_ATTRIBUTE,
  PROP_MULTIPLIER,
  PROP_CONSTANT,
  PROP_STRENGTH,

  N_PROPERTIES
};

static GParamSpec *obj_props[N_PROPERTIES];

G_DEFINE_TYPE (GtkConstraint, gtk_constraint, G_TYPE_OBJECT)

static void
gtk_constraint_set_property (GObject      *gobject,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GtkConstraint *self = GTK_CONSTRAINT (gobject);

  switch (prop_id)
    {
    case PROP_TARGET:
      self->target = g_value_get_object (value);
      break;

    case PROP_TARGET_ATTRIBUTE:
      self->target_attribute = g_value_get_enum (value);
      break;

    case PROP_RELATION:
      self->relation = g_value_get_enum (value);
      break;

    case PROP_SOURCE:
      self->source = g_value_get_object (value);
      break;

    case PROP_SOURCE_ATTRIBUTE:
      self->source_attribute = g_value_get_enum (value);
      break;

    case PROP_MULTIPLIER:
      self->multiplier = g_value_get_double (value);
      break;

    case PROP_CONSTANT:
      self->constant = g_value_get_double (value);
      break;

    case PROP_STRENGTH:
      self->strength = g_value_get_int (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
gtk_constraint_get_property (GObject    *gobject,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GtkConstraint *self = GTK_CONSTRAINT (gobject);

  switch (prop_id)
    {
    case PROP_TARGET:
      g_value_set_object (value, self->target);
      break;

    case PROP_TARGET_ATTRIBUTE:
      g_value_set_enum (value, self->target_attribute);
      break;

    case PROP_RELATION:
      g_value_set_enum (value, self->relation);
      break;

    case PROP_SOURCE:
      g_value_set_object (value, self->source);
      break;

    case PROP_SOURCE_ATTRIBUTE:
      g_value_set_enum (value, self->source_attribute);
      break;

    case PROP_MULTIPLIER:
      g_value_set_double (value, self->multiplier);
      break;

    case PROP_CONSTANT:
      g_value_set_double (value, self->constant);
      break;

    case PROP_STRENGTH:
      g_value_set_int (value, self->strength);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
gtk_constraint_finalize (GObject *gobject)
{
  GtkConstraint *self = GTK_CONSTRAINT (gobject);

  gtk_constraint_detach (self);

  G_OBJECT_CLASS (gtk_constraint_parent_class)->finalize (gobject);
}

static void
gtk_constraint_class_init (GtkConstraintClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = gtk_constraint_set_property;
  gobject_class->get_property = gtk_constraint_get_property;
  gobject_class->finalize = gtk_constraint_finalize;

  /**
   * GtkConstraint:target: (attributes org.gtk.Property.get=gtk_constraint_get_target)
   *
   * The target of the constraint.
   *
   * The constraint will set the [property@Gtk.Constraint:target-attribute]
   * property of the target using the [property@Gtk.Constraint:source-attribute]
   * property of the source widget.
   *
   *
   */
  obj_props[PROP_TARGET] =
    g_param_spec_object ("target", NULL, NULL,
                         GTK_TYPE_CONSTRAINT_TARGET,
                         G_PARAM_READWRITE |
                         G_PARAM_STATIC_STRINGS |
                         G_PARAM_CONSTRUCT_ONLY);

  /**
   * GtkConstraint:target-attribute: (attributes org.gtk.Property.get=gtk_constraint_get_target_attribute)
   *
   * The attribute of the [property@Gtk.Constraint:target] set by the constraint.
   */
  obj_props[PROP_TARGET_ATTRIBUTE] =
    g_param_spec_enum ("target-attribute", NULL, NULL,
                       GTK_TYPE_CONSTRAINT_ATTRIBUTE,
                       GTK_CONSTRAINT_ATTRIBUTE_NONE,
                       G_PARAM_READWRITE |
                       G_PARAM_STATIC_STRINGS |
                       G_PARAM_CONSTRUCT_ONLY);

  /**
   * GtkConstraint:relation: (attributes org.gtk.Property.get=gtk_constraint_get_relation)
   *
   * The order relation between the terms of the constraint.
   */
  obj_props[PROP_RELATION] =
    g_param_spec_enum ("relation", NULL, NULL,
                       GTK_TYPE_CONSTRAINT_RELATION,
                       GTK_CONSTRAINT_RELATION_EQ,
                       G_PARAM_READWRITE |
                       G_PARAM_STATIC_STRINGS |
                       G_PARAM_CONSTRUCT_ONLY);

  /**
   * GtkConstraint:source: (attributes org.gtk.Property.get=gtk_constraint_get_source)
   *
   * The source of the constraint.
   *
   * The constraint will set the [property@Gtk.Constraint:target-attribute]
   * property of the target using the [property@Gtk.Constraint:source-attribute]
   * property of the source.
   */
  obj_props[PROP_SOURCE] =
    g_param_spec_object ("source", NULL, NULL,
                         GTK_TYPE_CONSTRAINT_TARGET,
                         G_PARAM_READWRITE |
                         G_PARAM_STATIC_STRINGS |
                         G_PARAM_CONSTRUCT_ONLY);
  /**
   * GtkConstraint:source-attribute: (attributes org.gtk.Property.get=gtk_constraint_get_source_attribute)
   *
   * The attribute of the [property@Gtk.Constraint:source] read by the
   * constraint.
   */
  obj_props[PROP_SOURCE_ATTRIBUTE] =
    g_param_spec_enum ("source-attribute", NULL, NULL,
                       GTK_TYPE_CONSTRAINT_ATTRIBUTE,
                       GTK_CONSTRAINT_ATTRIBUTE_NONE,
                       G_PARAM_READWRITE |
                       G_PARAM_STATIC_STRINGS |
                       G_PARAM_CONSTRUCT_ONLY);

  /**
   * GtkConstraint:multiplier: (attributes org.gtk.Property.get=gtk_constraint_get_multiplier)
   *
   * The multiplication factor to be applied to
   * the [property@Gtk.Constraint:source-attribute].
   */
  obj_props[PROP_MULTIPLIER] =
    g_param_spec_double ("multiplier", NULL, NULL,
                         -G_MAXDOUBLE, G_MAXDOUBLE, 1.0,
                         G_PARAM_READWRITE |
                         G_PARAM_STATIC_STRINGS |
                         G_PARAM_CONSTRUCT_ONLY);

  /**
   * GtkConstraint:constant: (attributes org.gtk.Property.get=gtk_constraint_get_constant)
   *
   * The constant value to be added to the [property@Gtk.Constraint:source-attribute].
   */
  obj_props[PROP_CONSTANT] =
    g_param_spec_double ("constant", NULL, NULL,
                         -G_MAXDOUBLE, G_MAXDOUBLE, 0.0,
                         G_PARAM_READWRITE |
                         G_PARAM_STATIC_STRINGS |
                         G_PARAM_CONSTRUCT_ONLY);

  /**
   * GtkConstraint:strength: (attributes org.gtk.Property.get=gtk_constraint_get_strength)
   *
   * The strength of the constraint.
   *
   * The strength can be expressed either using one of the symbolic values
   * of the [enum@Gtk.ConstraintStrength] enumeration, or any positive integer
   * value.
   */
  obj_props[PROP_STRENGTH] =
    g_param_spec_int ("strength", NULL, NULL,
                      0, GTK_CONSTRAINT_STRENGTH_REQUIRED,
                      GTK_CONSTRAINT_STRENGTH_REQUIRED,
                      G_PARAM_READWRITE |
                      G_PARAM_STATIC_STRINGS |
                      G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (gobject_class, N_PROPERTIES, obj_props);
}

static void
gtk_constraint_init (GtkConstraint *self)
{
  self->multiplier = 1.0;
  self->constant = 0.0;

  self->target_attribute = GTK_CONSTRAINT_ATTRIBUTE_NONE;
  self->source_attribute = GTK_CONSTRAINT_ATTRIBUTE_NONE;
  self->relation = GTK_CONSTRAINT_RELATION_EQ;
  self->strength = GTK_CONSTRAINT_STRENGTH_REQUIRED;
}

/**
 * gtk_constraint_new: (constructor)
 * @target: (nullable) (type GtkConstraintTarget): the target of the constraint
 * @target_attribute: the attribute of `target` to be set
 * @relation: the relation equivalence between `target_attribute` and `source_attribute`
 * @source: (nullable) (type GtkConstraintTarget): the source of the constraint
 * @source_attribute: the attribute of `source` to be read
 * @multiplier: a multiplication factor to be applied to `source_attribute`
 * @constant: a constant factor to be added to `source_attribute`
 * @strength: the strength of the constraint
 *
 * Creates a new constraint representing a relation between a layout
 * attribute on a source and a layout attribute on a target.
 *
 * Returns: (transfer full): the newly created constraint
 */
GtkConstraint *
gtk_constraint_new (gpointer                target,
                    GtkConstraintAttribute  target_attribute,
                    GtkConstraintRelation   relation,
                    gpointer                source,
                    GtkConstraintAttribute  source_attribute,
                    double                  multiplier,
                    double                  constant,
                    int                     strength)
{
  g_return_val_if_fail (target == NULL || GTK_IS_CONSTRAINT_TARGET (target), NULL);
  g_return_val_if_fail (source == NULL || GTK_IS_CONSTRAINT_TARGET (source), NULL);

  return g_object_new (GTK_TYPE_CONSTRAINT,
                       "target", target,
                       "target-attribute", target_attribute,
                       "relation", relation,
                       "source", source,
                       "source-attribute", source_attribute,
                       "multiplier", multiplier,
                       "constant", constant,
                       "strength", strength,
                       NULL);
}

/**
 * gtk_constraint_new_constant: (constructor)
 * @target: (nullable) (type GtkConstraintTarget): a the target of the constraint
 * @target_attribute: the attribute of `target` to be set
 * @relation: the relation equivalence between `target_attribute` and `constant`
 * @constant: a constant factor to be set on `target_attribute`
 * @strength: the strength of the constraint
 *
 * Creates a new constraint representing a relation between a layout
 * attribute on a target and a constant value.
 *
 * Returns: (transfer full): the newly created constraint
 */
GtkConstraint *
gtk_constraint_new_constant (gpointer                target,
                             GtkConstraintAttribute  target_attribute,
                             GtkConstraintRelation   relation,
                             double                  constant,
                             int                     strength)
{
  g_return_val_if_fail (target == NULL || GTK_IS_CONSTRAINT_TARGET (target), NULL);

  return g_object_new (GTK_TYPE_CONSTRAINT,
                       "target", target,
                       "target-attribute", target_attribute,
                       "relation", relation,
                       "source-attribute", GTK_CONSTRAINT_ATTRIBUTE_NONE,
                       "constant", constant,
                       "strength", strength,
                       NULL);
}

/**
 * gtk_constraint_get_target: (attributes org.gtk.Method.get_property=target)
 * @constraint: a `GtkConstraint`
 *
 * Retrieves the [iface@Gtk.ConstraintTarget] used as the target for
 * the constraint.
 *
 * If the targe is set to `NULL` at creation, the constraint will use
 * the widget using the [class@Gtk.ConstraintLayout] as the target.
 *
 * Returns: (transfer none) (nullable): a `GtkConstraintTarget`
 */
GtkConstraintTarget *
gtk_constraint_get_target (GtkConstraint *constraint)
{
  g_return_val_if_fail (GTK_IS_CONSTRAINT (constraint), NULL);

  return constraint->target;
}

/**
 * gtk_constraint_get_target_attribute: (attributes org.gtk.Method.get_property=target-attribute)
 * @constraint: a `GtkConstraint`
 *
 * Retrieves the attribute of the target to be set by the constraint.
 *
 * Returns: the target's attribute
 */
GtkConstraintAttribute
gtk_constraint_get_target_attribute (GtkConstraint *constraint)
{
  g_return_val_if_fail (GTK_IS_CONSTRAINT (constraint), GTK_CONSTRAINT_ATTRIBUTE_NONE);

  return constraint->target_attribute;
}

/**
 * gtk_constraint_get_source: (attributes org.gtk.Method.get_property=source)
 * @constraint: a `GtkConstraint`
 *
 * Retrieves the [iface@Gtk.ConstraintTarget] used as the source for the
 * constraint.
 *
 * If the source is set to `NULL` at creation, the constraint will use
 * the widget using the [class@Gtk.ConstraintLayout] as the source.
 *
 * Returns: (transfer none) (nullable): the source of the constraint
 */
GtkConstraintTarget *
gtk_constraint_get_source (GtkConstraint *constraint)
{
  g_return_val_if_fail (GTK_IS_CONSTRAINT (constraint), NULL);

  return constraint->source;
}

/**
 * gtk_constraint_get_source_attribute: (attributes org.gtk.Method.get_property=source-attribute)
 * @constraint: a `GtkConstraint`
 *
 * Retrieves the attribute of the source to be read by the constraint.
 *
 * Returns: the source's attribute
 */
GtkConstraintAttribute
gtk_constraint_get_source_attribute (GtkConstraint *constraint)
{
  g_return_val_if_fail (GTK_IS_CONSTRAINT (constraint), GTK_CONSTRAINT_ATTRIBUTE_NONE);

  return constraint->source_attribute;
}

/**
 * gtk_constraint_get_relation: (attributes org.gtk.Method.get_property=relation)
 * @constraint: a `GtkConstraint`
 *
 * The order relation between the terms of the constraint.
 *
 * Returns: a relation type
 */
GtkConstraintRelation
gtk_constraint_get_relation (GtkConstraint *constraint)
{
  g_return_val_if_fail (GTK_IS_CONSTRAINT (constraint), GTK_CONSTRAINT_RELATION_EQ);

  return constraint->relation;
}

/**
 * gtk_constraint_get_multiplier: (attributes org.gtk.Method.get_property=multiplier)
 * @constraint: a `GtkConstraint`
 *
 * Retrieves the multiplication factor applied to the source
 * attribute's value.
 *
 * Returns: a multiplication factor
 */
double
gtk_constraint_get_multiplier (GtkConstraint *constraint)
{
  g_return_val_if_fail (GTK_IS_CONSTRAINT (constraint), 1.0);

  return constraint->multiplier;
}

/**
 * gtk_constraint_get_constant: (attributes org.gtk.Method.get_property=constant)
 * @constraint: a `GtkConstraint`
 *
 * Retrieves the constant factor added to the source attributes' value.
 *
 * Returns: a constant factor
 */
double
gtk_constraint_get_constant (GtkConstraint *constraint)
{
  g_return_val_if_fail (GTK_IS_CONSTRAINT (constraint), 0.0);

  return constraint->constant;
}

/**
 * gtk_constraint_get_strength: (attributes org.gtk.Method.get_property=strength)
 * @constraint: a `GtkConstraint`
 *
 * Retrieves the strength of the constraint.
 *
 * Returns: the strength value
 */
int
gtk_constraint_get_strength (GtkConstraint *constraint)
{
  g_return_val_if_fail (GTK_IS_CONSTRAINT (constraint), GTK_CONSTRAINT_STRENGTH_REQUIRED);

  return constraint->strength;
}

/**
 * gtk_constraint_is_required:
 * @constraint: a `GtkConstraint`
 *
 * Checks whether the constraint is a required relation for solving the
 * constraint layout.
 *
 * Returns: %TRUE if the constraint is required
 */
gboolean
gtk_constraint_is_required (GtkConstraint *constraint)
{
  g_return_val_if_fail (GTK_IS_CONSTRAINT (constraint), FALSE);

  return constraint->strength == GTK_CONSTRAINT_STRENGTH_REQUIRED;
}

/**
 * gtk_constraint_is_attached:
 * @constraint: a `GtkConstraint`
 *
 * Checks whether the constraint is attached to a [class@Gtk.ConstraintLayout],
 * and it is contributing to the layout.
 *
 * Returns: `TRUE` if the constraint is attached
 */
gboolean
gtk_constraint_is_attached (GtkConstraint *constraint)
{
  g_return_val_if_fail (GTK_IS_CONSTRAINT (constraint), FALSE);

  return constraint->constraint_ref != NULL;
}

/**
 * gtk_constraint_is_constant:
 * @constraint: a `GtkConstraint`
 *
 * Checks whether the constraint describes a relation between an attribute
 * on the [property@Gtk.Constraint:target] and a constant value.
 *
 * Returns: `TRUE` if the constraint is a constant relation
 */
gboolean
gtk_constraint_is_constant (GtkConstraint *constraint)
{
  g_return_val_if_fail (GTK_IS_CONSTRAINT (constraint), FALSE);

  return constraint->source == NULL &&
         constraint->source_attribute == GTK_CONSTRAINT_ATTRIBUTE_NONE;
}

void
gtk_constraint_attach (GtkConstraint       *constraint,
                       GtkConstraintSolver *solver,
                       GtkConstraintRef    *ref)
{
  g_return_if_fail (GTK_IS_CONSTRAINT (constraint));
  g_return_if_fail (GTK_IS_CONSTRAINT_SOLVER (solver));
  g_return_if_fail (ref != NULL);

  constraint->constraint_ref = ref;
  constraint->solver = solver;
}

void
gtk_constraint_detach (GtkConstraint *constraint)
{
  g_return_if_fail (GTK_IS_CONSTRAINT (constraint));

  if (constraint->constraint_ref == NULL)
    return;

  gtk_constraint_solver_remove_constraint (constraint->solver, constraint->constraint_ref);
  constraint->constraint_ref = NULL;
  constraint->solver = NULL;
}

typedef struct _GtkConstraintTargetInterface GtkConstraintTargetInterface;

struct _GtkConstraintTargetInterface
{
  GTypeInterface g_iface;
};

G_DEFINE_INTERFACE (GtkConstraintTarget, gtk_constraint_target, G_TYPE_OBJECT)

static void
gtk_constraint_target_default_init (GtkConstraintTargetInterface *iface)
{
}
