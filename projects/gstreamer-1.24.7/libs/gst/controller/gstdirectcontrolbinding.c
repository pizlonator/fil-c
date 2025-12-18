/* GStreamer
 *
 * Copyright (C) 2011 Stefan Sauer <ensonic@users.sf.net>
 *
 * gstdirectcontrolbinding.c: Direct attachment for control sources
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */
/**
 * SECTION:gstdirectcontrolbinding
 * @title: GstDirectControlBinding
 * @short_description: direct attachment for control sources
 *
 * A value mapping object that attaches control sources to gobject properties. It
 * will map the control values directly to the target property range. If a
 * non-absolute direct control binding is used, the value range [0.0 ... 1.0]
 * is mapped to full target property range, and all values outside the range
 * will be clipped. An absolute control binding will not do any value
 * transformations.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib-object.h>
#include <gst/gst.h>

#include "gstdirectcontrolbinding.h"

#include <gst/math-compat.h>

#define GST_CAT_DEFAULT control_binding_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);


static GObject *gst_direct_control_binding_constructor (GType type,
    guint n_construct_params, GObjectConstructParam * construct_params);
static void gst_direct_control_binding_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_direct_control_binding_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);
static void gst_direct_control_binding_dispose (GObject * object);
static void gst_direct_control_binding_finalize (GObject * object);

static gboolean gst_direct_control_binding_sync_values (GstControlBinding *
    _self, GstObject * object, GstClockTime timestamp, GstClockTime last_sync);
static GValue *gst_direct_control_binding_get_value (GstControlBinding * _self,
    GstClockTime timestamp);
static gboolean gst_direct_control_binding_get_value_array (GstControlBinding *
    _self, GstClockTime timestamp, GstClockTime interval, guint n_values,
    gpointer values);
static gboolean gst_direct_control_binding_get_g_value_array (GstControlBinding
    * _self, GstClockTime timestamp, GstClockTime interval, guint n_values,
    GValue * values);

#define _do_init \
  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "gstdirectcontrolbinding", 0, \
      "dynamic parameter control source attachment");

#define gst_direct_control_binding_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstDirectControlBinding, gst_direct_control_binding,
    GST_TYPE_CONTROL_BINDING, _do_init);

enum
{
  PROP_0,
  PROP_CS,
  PROP_ABSOLUTE,
  PROP_LAST
};

static GParamSpec *properties[PROP_LAST];

/* mapping functions */

#define DEFINE_CONVERT(type,Type,TYPE,ROUNDING_OP) \
static void \
convert_g_value_to_##type (GstDirectControlBinding *self, gdouble s, GValue *d) \
{ \
  GParamSpec##Type *pspec = G_PARAM_SPEC_##TYPE (((GstControlBinding *)self)->pspec); \
  g##type v; \
  \
  s = CLAMP (s, 0.0, 1.0); \
  v = (g##type) ROUNDING_OP (pspec->minimum * (1-s)) + (g##type) ROUNDING_OP (pspec->maximum * s); \
  g_value_set_##type (d, v); \
} \
\
static void \
convert_value_to_##type (GstDirectControlBinding *self, gdouble s, gpointer d_) \
{ \
  GParamSpec##Type *pspec = G_PARAM_SPEC_##TYPE (((GstControlBinding *)self)->pspec); \
  g##type *d = (g##type *)d_; \
  \
  s = CLAMP (s, 0.0, 1.0); \
  *d = (g##type) ROUNDING_OP (pspec->minimum * (1-s)) + (g##type) ROUNDING_OP (pspec->maximum * s); \
} \
\
static void \
abs_convert_g_value_to_##type (GstDirectControlBinding *self, gdouble s, GValue *d) \
{ \
  g##type v; \
  v = (g##type) ROUNDING_OP (s); \
  g_value_set_##type (d, v); \
} \
\
static void \
abs_convert_value_to_##type (GstDirectControlBinding *self, gdouble s, gpointer d_) \
{ \
  g##type *d = (g##type *)d_; \
  *d = (g##type) ROUNDING_OP (s); \
}

DEFINE_CONVERT (int, Int, INT, rint);
DEFINE_CONVERT (uint, UInt, UINT, rint);
DEFINE_CONVERT (long, Long, LONG, rint);
DEFINE_CONVERT (ulong, ULong, ULONG, rint);
DEFINE_CONVERT (int64, Int64, INT64, rint);
DEFINE_CONVERT (uint64, UInt64, UINT64, rint);
DEFINE_CONVERT (float, Float, FLOAT, /*NOOP*/);
DEFINE_CONVERT (double, Double, DOUBLE, /*NOOP*/);

static void
convert_g_value_to_boolean (GstDirectControlBinding * self, gdouble s,
    GValue * d)
{
  s = CLAMP (s, 0.0, 1.0);
  g_value_set_boolean (d, (gboolean) (s + 0.5));
}

static void
convert_value_to_boolean (GstDirectControlBinding * self, gdouble s,
    gpointer d_)
{
  gboolean *d = (gboolean *) d_;

  s = CLAMP (s, 0.0, 1.0);
  *d = (gboolean) (s + 0.5);
}

static void
convert_g_value_to_enum (GstDirectControlBinding * self, gdouble s, GValue * d)
{
  GParamSpecEnum *pspec =
      G_PARAM_SPEC_ENUM (((GstControlBinding *) self)->pspec);
  GEnumClass *e = pspec->enum_class;
  gint v;

  s = CLAMP (s, 0.0, 1.0);
  v = s * (e->n_values - 1);
  g_value_set_enum (d, e->values[v].value);
}

static void
convert_value_to_enum (GstDirectControlBinding * self, gdouble s, gpointer d_)
{
  GParamSpecEnum *pspec =
      G_PARAM_SPEC_ENUM (((GstControlBinding *) self)->pspec);
  GEnumClass *e = pspec->enum_class;
  gint *d = (gint *) d_;

  s = CLAMP (s, 0.0, 1.0);
  *d = e->values[(gint) (s * (e->n_values - 1))].value;
}

/* vmethods */

static void
gst_direct_control_binding_class_init (GstDirectControlBindingClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstControlBindingClass *control_binding_class =
      GST_CONTROL_BINDING_CLASS (klass);

  gobject_class->constructor = gst_direct_control_binding_constructor;
  gobject_class->set_property = gst_direct_control_binding_set_property;
  gobject_class->get_property = gst_direct_control_binding_get_property;
  gobject_class->dispose = gst_direct_control_binding_dispose;
  gobject_class->finalize = gst_direct_control_binding_finalize;

  control_binding_class->sync_values = gst_direct_control_binding_sync_values;
  control_binding_class->get_value = gst_direct_control_binding_get_value;
  control_binding_class->get_value_array =
      gst_direct_control_binding_get_value_array;
  control_binding_class->get_g_value_array =
      gst_direct_control_binding_get_g_value_array;

  properties[PROP_CS] =
      g_param_spec_object ("control-source", "ControlSource",
      "The control source",
      GST_TYPE_CONTROL_SOURCE,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

  properties[PROP_ABSOLUTE] =
      g_param_spec_boolean ("absolute", "Absolute",
      "Whether the control values are absolute",
      FALSE,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, properties);
}

static void
gst_direct_control_binding_init (GstDirectControlBinding * self)
{
  self->last_value = G_MAXDOUBLE;
}

static GObject *
gst_direct_control_binding_constructor (GType type, guint n_construct_params,
    GObjectConstructParam * construct_params)
{
  GstDirectControlBinding *self;

  self =
      GST_DIRECT_CONTROL_BINDING (G_OBJECT_CLASS (parent_class)->constructor
      (type, n_construct_params, construct_params));

  if (GST_CONTROL_BINDING_PSPEC (self)) {
    GType type, base;

    base = type = G_PARAM_SPEC_VALUE_TYPE (GST_CONTROL_BINDING_PSPEC (self));
    g_value_init (&self->cur_value, type);
    while ((type = g_type_parent (type)))
      base = type;

    GST_DEBUG ("  using type %s", g_type_name (base));

    /* select mapping function */

#define SET_CONVERT_FUNCTION(type) \
    if (self->ABI.abi.want_absolute) { \
        self->convert_g_value = abs_convert_g_value_to_##type; \
        self->convert_value = abs_convert_value_to_##type; \
    } \
    else { \
        self->convert_g_value = convert_g_value_to_##type; \
        self->convert_value = convert_value_to_##type; \
    } \
    self->byte_size = sizeof (g##type);


    switch ((uintptr_t) base) {
      case (uintptr_t) G_TYPE_INT:
        SET_CONVERT_FUNCTION (int);
        break;
      case (uintptr_t) G_TYPE_UINT:
        SET_CONVERT_FUNCTION (uint);
        break;
      case (uintptr_t) G_TYPE_LONG:
        SET_CONVERT_FUNCTION (long);
        break;
      case (uintptr_t) G_TYPE_ULONG:
        SET_CONVERT_FUNCTION (ulong);
        break;
      case (uintptr_t) G_TYPE_INT64:
        SET_CONVERT_FUNCTION (int64);
        break;
      case (uintptr_t) G_TYPE_UINT64:
        SET_CONVERT_FUNCTION (uint64);
        break;
      case (uintptr_t) G_TYPE_FLOAT:
        SET_CONVERT_FUNCTION (float);
        break;
      case (uintptr_t) G_TYPE_DOUBLE:
        SET_CONVERT_FUNCTION (double);
        break;
      case (uintptr_t) G_TYPE_BOOLEAN:
        self->convert_g_value = convert_g_value_to_boolean;
        self->convert_value = convert_value_to_boolean;
        self->byte_size = sizeof (gboolean);
        break;
      case (uintptr_t) G_TYPE_ENUM:
        self->convert_g_value = convert_g_value_to_enum;
        self->convert_value = convert_value_to_enum;
        self->byte_size = sizeof (gint);
        break;
      default:
        GST_WARNING ("incomplete implementation for paramspec type '%s'",
            G_PARAM_SPEC_TYPE_NAME (GST_CONTROL_BINDING_PSPEC (self)));
        GST_CONTROL_BINDING_PSPEC (self) = NULL;
        break;
    }
  }
  return (GObject *) self;
}

static void
gst_direct_control_binding_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstDirectControlBinding *self = GST_DIRECT_CONTROL_BINDING (object);

  switch (prop_id) {
    case PROP_CS:
      self->cs = g_value_dup_object (value);
      break;
    case PROP_ABSOLUTE:
      self->ABI.abi.want_absolute = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_direct_control_binding_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstDirectControlBinding *self = GST_DIRECT_CONTROL_BINDING (object);

  switch (prop_id) {
    case PROP_CS:
      g_value_set_object (value, self->cs);
      break;
    case PROP_ABSOLUTE:
      g_value_set_boolean (value, self->ABI.abi.want_absolute);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_direct_control_binding_dispose (GObject * object)
{
  GstDirectControlBinding *self = GST_DIRECT_CONTROL_BINDING (object);

  if (self->cs)
    gst_object_replace ((GstObject **) & self->cs, NULL);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_direct_control_binding_finalize (GObject * object)
{
  GstDirectControlBinding *self = GST_DIRECT_CONTROL_BINDING (object);

  if (G_IS_VALUE (&self->cur_value))
    g_value_unset (&self->cur_value);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean
gst_direct_control_binding_sync_values (GstControlBinding * _self,
    GstObject * object, GstClockTime timestamp, GstClockTime last_sync)
{
  GstDirectControlBinding *self = GST_DIRECT_CONTROL_BINDING (_self);
  gdouble src_val;
  gboolean ret;

  g_return_val_if_fail (GST_IS_DIRECT_CONTROL_BINDING (self), FALSE);
  g_return_val_if_fail (GST_CONTROL_BINDING_PSPEC (self), FALSE);

  GST_LOG_OBJECT (object, "property '%s' at ts=%" GST_TIME_FORMAT,
      _self->name, GST_TIME_ARGS (timestamp));

  ret = gst_control_source_get_value (self->cs, timestamp, &src_val);
  if (G_LIKELY (ret)) {
    GST_LOG_OBJECT (object, "  new value %lf", src_val);
    /* always set the value for first time, but then only if it changed
     * this should limit g_object_notify invocations.
     * FIXME: can we detect negative playback rates?
     */
    if ((timestamp < last_sync) || (src_val != self->last_value)) {
      GValue *dst_val = &self->cur_value;

      GST_LOG_OBJECT (object, "  mapping %s to value of type %s", _self->name,
          G_VALUE_TYPE_NAME (dst_val));
      /* run mapping function to convert gdouble to GValue */
      self->convert_g_value (self, src_val, dst_val);
      /* we can make this faster
       * http://bugzilla.gnome.org/show_bug.cgi?id=536939
       */
      g_object_set_property ((GObject *) object, _self->name, dst_val);
      self->last_value = src_val;
    }
  } else {
    GST_DEBUG_OBJECT (object, "no control value for param %s", _self->name);
  }
  return (ret);
}

static GValue *
gst_direct_control_binding_get_value (GstControlBinding * _self,
    GstClockTime timestamp)
{
  GstDirectControlBinding *self = GST_DIRECT_CONTROL_BINDING (_self);
  GValue *dst_val = NULL;
  gdouble src_val;

  g_return_val_if_fail (GST_IS_DIRECT_CONTROL_BINDING (self), NULL);
  g_return_val_if_fail (GST_CLOCK_TIME_IS_VALID (timestamp), NULL);
  g_return_val_if_fail (GST_CONTROL_BINDING_PSPEC (self), FALSE);

  /* get current value via control source */
  if (gst_control_source_get_value (self->cs, timestamp, &src_val)) {
    dst_val = g_new0 (GValue, 1);
    g_value_init (dst_val, G_PARAM_SPEC_VALUE_TYPE (_self->pspec));
    self->convert_g_value (self, src_val, dst_val);
  } else {
    GST_LOG ("no control value for property %s at ts %" GST_TIME_FORMAT,
        _self->name, GST_TIME_ARGS (timestamp));
  }

  return dst_val;
}

static gboolean
gst_direct_control_binding_get_value_array (GstControlBinding * _self,
    GstClockTime timestamp, GstClockTime interval, guint n_values,
    gpointer values_)
{
  GstDirectControlBinding *self = GST_DIRECT_CONTROL_BINDING (_self);
  guint i;
  gdouble *src_val;
  gboolean res = FALSE;
  GstDirectControlBindingConvertValue convert;
  gint byte_size;
  guint8 *values = (guint8 *) values_;

  g_return_val_if_fail (GST_IS_DIRECT_CONTROL_BINDING (self), FALSE);
  g_return_val_if_fail (GST_CLOCK_TIME_IS_VALID (timestamp), FALSE);
  g_return_val_if_fail (GST_CLOCK_TIME_IS_VALID (interval), FALSE);
  g_return_val_if_fail (values, FALSE);
  g_return_val_if_fail (GST_CONTROL_BINDING_PSPEC (self), FALSE);

  convert = self->convert_value;
  byte_size = self->byte_size;

  src_val = g_new0 (gdouble, n_values);
  if ((res = gst_control_source_get_value_array (self->cs, timestamp,
              interval, n_values, src_val))) {
    for (i = 0; i < n_values; i++) {
      /* we will only get NAN for sparse control sources, such as triggers */
      if (!isnan (src_val[i])) {
        convert (self, src_val[i], (gpointer) values);
      } else {
        GST_LOG ("no control value for property %s at index %d", _self->name,
            i);
      }
      values += byte_size;
    }
  } else {
    GST_LOG ("failed to get control value for property %s at ts %"
        GST_TIME_FORMAT, _self->name, GST_TIME_ARGS (timestamp));
  }
  g_free (src_val);
  return res;
}

static gboolean
gst_direct_control_binding_get_g_value_array (GstControlBinding * _self,
    GstClockTime timestamp, GstClockTime interval, guint n_values,
    GValue * values)
{
  GstDirectControlBinding *self = GST_DIRECT_CONTROL_BINDING (_self);
  guint i;
  gdouble *src_val;
  gboolean res = FALSE;
  GType type;
  GstDirectControlBindingConvertGValue convert;

  g_return_val_if_fail (GST_IS_DIRECT_CONTROL_BINDING (self), FALSE);
  g_return_val_if_fail (GST_CLOCK_TIME_IS_VALID (timestamp), FALSE);
  g_return_val_if_fail (GST_CLOCK_TIME_IS_VALID (interval), FALSE);
  g_return_val_if_fail (values, FALSE);
  g_return_val_if_fail (GST_CONTROL_BINDING_PSPEC (self), FALSE);

  convert = self->convert_g_value;
  type = G_PARAM_SPEC_VALUE_TYPE (_self->pspec);

  src_val = g_new0 (gdouble, n_values);
  if ((res = gst_control_source_get_value_array (self->cs, timestamp,
              interval, n_values, src_val))) {
    for (i = 0; i < n_values; i++) {
      /* we will only get NAN for sparse control sources, such as triggers */
      if (!isnan (src_val[i])) {
        g_value_init (&values[i], type);
        convert (self, src_val[i], &values[i]);
      } else {
        GST_LOG ("no control value for property %s at index %d", _self->name,
            i);
      }
    }
  } else {
    GST_LOG ("failed to get control value for property %s at ts %"
        GST_TIME_FORMAT, _self->name, GST_TIME_ARGS (timestamp));
  }
  g_free (src_val);
  return res;
}

/* functions */

/**
 * gst_direct_control_binding_new:
 * @object: the object of the property
 * @property_name: the property-name to attach the control source
 * @cs: the control source
 *
 * Create a new control-binding that attaches the #GstControlSource to the
 * #GObject property. It will map the control source range [0.0 ... 1.0] to
 * the full target property range, and clip all values outside this range.
 *
 * Returns: (transfer floating): the new #GstDirectControlBinding
 */
GstControlBinding *
gst_direct_control_binding_new (GstObject * object, const gchar * property_name,
    GstControlSource * cs)
{
  return (GstControlBinding *) g_object_new (GST_TYPE_DIRECT_CONTROL_BINDING,
      "object", object, "name", property_name, "control-source", cs, NULL);
}

/**
 * gst_direct_control_binding_new_absolute:
 * @object: the object of the property
 * @property_name: the property-name to attach the control source
 * @cs: the control source
 *
 * Create a new control-binding that attaches the #GstControlSource to the
 * #GObject property. It will directly map the control source values to the
 * target property range without any transformations.
 *
 * Returns: (transfer floating): the new #GstDirectControlBinding
 *
 * Since: 1.6
 */
GstControlBinding *
gst_direct_control_binding_new_absolute (GstObject * object,
    const gchar * property_name, GstControlSource * cs)
{
  return (GstControlBinding *) g_object_new (GST_TYPE_DIRECT_CONTROL_BINDING,
      "object", object, "name", property_name, "control-source", cs, "absolute",
      TRUE, NULL);
}
