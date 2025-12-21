/* GStreamer - GParamSpecs for some of our types
 * Copyright (C) 2007 Tim-Philipp Müller  <tim centricular net>
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
 * SECTION:gstparamspec
 * @title: GstParamSpec
 * @short_description: GParamSpec implementations specific
 * to GStreamer
 *
 * GParamSpec implementations specific to GStreamer.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gst_private.h"
#include "glib-compat-private.h"
#include "gstparamspecs.h"

/* --- GstParamSpecFraction --- */

static void
_gst_param_fraction_init (GParamSpec * pspec)
{
  GstParamSpecFraction *fspec = GST_PARAM_SPEC_FRACTION (pspec);

  fspec->min_num = 0;
  fspec->min_den = 1;
  fspec->max_num = G_MAXINT;
  fspec->max_den = 1;
  fspec->def_num = 1;
  fspec->def_den = 1;
}

static void
_gst_param_fraction_set_default (GParamSpec * pspec, GValue * value)
{
  value->data[0].v_int = GST_PARAM_SPEC_FRACTION (pspec)->def_num;
  value->data[1].v_int = GST_PARAM_SPEC_FRACTION (pspec)->def_den;
}

static gboolean
_gst_param_fraction_validate (GParamSpec * pspec, GValue * value)
{
  GstParamSpecFraction *fspec = GST_PARAM_SPEC_FRACTION (pspec);
  gboolean within_range = FALSE;
  GValue f_this = { 0, };
  GValue f_min = { 0, };
  GValue f_max = { 0, };
  gint res;

  g_value_init (&f_this, GST_TYPE_FRACTION);
  gst_value_set_fraction (&f_this, value->data[0].v_int, value->data[1].v_int);

  g_value_init (&f_min, GST_TYPE_FRACTION);
  gst_value_set_fraction (&f_min, fspec->min_num, fspec->min_den);

  g_value_init (&f_max, GST_TYPE_FRACTION);
  gst_value_set_fraction (&f_max, fspec->max_num, fspec->max_den);

  res = gst_value_compare (&f_min, &f_this);
#ifndef GST_DISABLE_GST_DEBUG
  GST_LOG ("comparing %d/%d to %d/%d, result = %d", fspec->min_num,
      fspec->min_den, value->data[0].v_int, value->data[1].v_int, res);
#endif
  if (res != GST_VALUE_LESS_THAN && res != GST_VALUE_EQUAL)
    goto out;

#ifndef GST_DISABLE_GST_DEBUG
  GST_LOG ("comparing %d/%d to %d/%d, result = %d", value->data[0].v_int,
      value->data[1].v_int, fspec->max_num, fspec->max_den, res);
#endif
  res = gst_value_compare (&f_this, &f_max);
  if (res != GST_VALUE_LESS_THAN && res != GST_VALUE_EQUAL)
    goto out;

  within_range = TRUE;

out:

  g_value_unset (&f_min);
  g_value_unset (&f_max);
  g_value_unset (&f_this);

#ifndef GST_DISABLE_GST_DEBUG
  GST_LOG ("%swithin range", (within_range) ? "" : "not ");
#endif

  /* return FALSE if everything ok, otherwise TRUE */
  return !within_range;
}

static gint
_gst_param_fraction_values_cmp (GParamSpec * pspec, const GValue * value1,
    const GValue * value2)
{
  gint res;

  res = gst_value_compare (value1, value2);

  g_assert (res != GST_VALUE_UNORDERED);

  /* GST_VALUE_LESS_THAN is -1, EQUAL is 0, and GREATER_THAN is 1 */
  return res;
}

GType
gst_param_spec_fraction_get_type (void)
{
  static GType gst_faction_type = 0;

  /* register GST_TYPE_PARAM_FRACTION */
  if (g_once_init_enter_pointer (&gst_faction_type)) {
    GType type;
    static GParamSpecTypeInfo pspec_info = {
      sizeof (GstParamSpecFraction),    /* instance_size     */
      0,                        /* n_preallocs       */
      _gst_param_fraction_init, /* instance_init     */
      G_TYPE_INVALID,           /* value_type        */
      NULL,                     /* finalize          */
      _gst_param_fraction_set_default,  /* value_set_default */
      _gst_param_fraction_validate,     /* value_validate    */
      _gst_param_fraction_values_cmp,   /* values_cmp        */
    };
    pspec_info.value_type = gst_fraction_get_type ();
    type = g_param_type_register_static ("GstParamFraction", &pspec_info);
    g_once_init_leave_pointer (&gst_faction_type, type);
  }

  return gst_faction_type;
}

/**
 * gst_param_spec_fraction:
 * @name: canonical name of the property specified
 * @nick: nick name for the property specified
 * @blurb: description of the property specified
 * @min_num: minimum value (fraction numerator)
 * @min_denom: minimum value (fraction denominator)
 * @max_num: maximum value (fraction numerator)
 * @max_denom: maximum value (fraction denominator)
 * @default_num: default value (fraction numerator)
 * @default_denom: default value (fraction denominator)
 * @flags: flags for the property specified
 *
 * This function creates a fraction GParamSpec for use by objects/elements
 * that want to expose properties of fraction type. This function is typically
 * used in connection with g_object_class_install_property() in a GObjects's
 * instance_init function.
 *
 * Returns: (transfer full) (nullable): a newly created parameter specification
 */
GParamSpec *
gst_param_spec_fraction (const gchar * name, const gchar * nick,
    const gchar * blurb, gint min_num, gint min_denom, gint max_num,
    gint max_denom, gint default_num, gint default_denom, GParamFlags flags)
{
  GstParamSpecFraction *fspec;
  GParamSpec *pspec;
  GValue default_val = { 0, };

  fspec =
      g_param_spec_internal (GST_TYPE_PARAM_FRACTION, name, nick, blurb, flags);

  fspec->min_num = min_num;
  fspec->min_den = min_denom;
  fspec->max_num = max_num;
  fspec->max_den = max_denom;
  fspec->def_num = default_num;
  fspec->def_den = default_denom;

  pspec = G_PARAM_SPEC (fspec);

  /* check that min <= default <= max */
  g_value_init (&default_val, GST_TYPE_FRACTION);
  gst_value_set_fraction (&default_val, default_num, default_denom);
  /* validate returns TRUE if the validation fails */
  if (_gst_param_fraction_validate (pspec, &default_val)) {
    g_critical ("GstParamSpec of type 'fraction' for property '%s' has a "
        "default value of %d/%d, which is not within the allowed range of "
        "%d/%d to %d/%d", name, default_num, default_denom, min_num,
        min_denom, max_num, max_denom);
    g_param_spec_ref (pspec);
    g_param_spec_sink (pspec);
    g_param_spec_unref (pspec);
    pspec = NULL;
  }
  g_value_unset (&default_val);

  return pspec;
}

static void
_gst_param_array_init (GParamSpec * pspec)
{
  GstParamSpecArray *aspec = GST_PARAM_SPEC_ARRAY_LIST (pspec);

  aspec->element_spec = NULL;
}

static void
_gst_param_array_finalize (GParamSpec * pspec)
{
  GstParamSpecArray *aspec = GST_PARAM_SPEC_ARRAY_LIST (pspec);
  GParamSpecClass *parent_class =
      g_type_class_peek (g_type_parent (GST_TYPE_PARAM_ARRAY_LIST));

  if (aspec->element_spec) {
    g_param_spec_unref (aspec->element_spec);
    aspec->element_spec = NULL;
  }

  parent_class->finalize (pspec);
}

static gboolean
_gst_param_array_validate (GParamSpec * pspec, GValue * value)
{
  GstParamSpecArray *aspec = GST_PARAM_SPEC_ARRAY_LIST (pspec);
  gboolean ret = FALSE;

  /* ensure array values validity against a present element spec */
  if (aspec->element_spec) {
    GParamSpec *element_spec = aspec->element_spec;
    guint i;

    for (i = 0; i < gst_value_array_get_size (value); i++) {
      GValue *element = (GValue *) gst_value_array_get_value (value, i);

      /* need to fixup value type, or ensure that the array value is initialized at all */
      if (!g_value_type_compatible (G_VALUE_TYPE (element),
              G_PARAM_SPEC_VALUE_TYPE (element_spec))) {
        if (G_VALUE_TYPE (element) != 0)
          g_value_unset (element);
        g_value_init (element, G_PARAM_SPEC_VALUE_TYPE (element_spec));
        g_param_value_set_default (element_spec, element);
        ret = TRUE;
      }

      /* validate array value against element_spec */
      if (g_param_value_validate (element_spec, element))
        ret = TRUE;
    }
  }

  return ret;
}

static gint
_gst_param_array_values_cmp (GParamSpec * pspec, const GValue * value1,
    const GValue * value2)
{
  GstParamSpecArray *aspec = GST_PARAM_SPEC_ARRAY_LIST (pspec);
  guint size1, size2;

  if (!value1 || !value2)
    return value2 ? -1 : value1 != value2;

  size1 = gst_value_array_get_size (value1);
  size2 = gst_value_array_get_size (value2);

  if (size1 != size2)
    return size1 < size2 ? -1 : 1;
  else if (!aspec->element_spec) {
    /* we need an element specification for comparisons, so there's not much
     * to compare here, try to at least provide stable lesser/greater result
     */
    return size1 < size2 ? -1 : size1 > size2;
  } else {                      /* size1 == size2 */
    guint i;

    for (i = 0; i < size1; i++) {
      const GValue *element1 = gst_value_array_get_value (value1, i);
      const GValue *element2 = gst_value_array_get_value (value2, i);
      gint cmp;

      /* need corresponding element types, provide stable result otherwise */
      if (G_VALUE_TYPE (element1) != G_VALUE_TYPE (element2))
        return G_VALUE_TYPE (element1) < G_VALUE_TYPE (element2) ? -1 : 1;
      cmp = g_param_values_cmp (aspec->element_spec, element1, element2);
      if (cmp)
        return cmp;
    }
    return 0;
  }
}

GType
gst_param_spec_array_get_type (void)
{
  static GType gst_array_type = 0;

  /* register GST_TYPE_PARAM_FRACTION */
  if (g_once_init_enter_pointer (&gst_array_type)) {
    GType type;
    static GParamSpecTypeInfo pspec_info = {
      sizeof (GstParamSpecArray),       /* instance_size     */
      0,                        /* n_preallocs       */
      _gst_param_array_init,    /* instance_init     */
      G_TYPE_INVALID,           /* value_type        */
      _gst_param_array_finalize,        /* finalize          */
      NULL,                     /* value_set_default */
      _gst_param_array_validate,        /* value_validate    */
      _gst_param_array_values_cmp,      /* values_cmp        */
    };
    pspec_info.value_type = gst_value_array_get_type ();
    type = g_param_type_register_static ("GstParamArray", &pspec_info);
    g_once_init_leave_pointer (&gst_array_type, type);
  }

  return gst_array_type;
}

/**
 * gst_param_spec_array:
 * @name: canonical name of the property specified
 * @nick: nick name for the property specified
 * @blurb: description of the property specified
 * @element_spec: GParamSpec of the array
 * @flags: flags for the property specified
 *
 * This function creates a GstArray GParamSpec for use by objects/elements
 * that want to expose properties of GstArray type. This function is
 * typically * used in connection with g_object_class_install_property() in a
 * GObjects's instance_init function.
 *
 * Returns: (transfer full): a newly created parameter specification
 *
 * Since: 1.14
 */

GParamSpec *
gst_param_spec_array (const gchar * name,
    const gchar * nick,
    const gchar * blurb, GParamSpec * element_spec, GParamFlags flags)
{
  GstParamSpecArray *aspec;

  g_return_val_if_fail (element_spec == NULL
      || G_IS_PARAM_SPEC (element_spec), NULL);

  aspec = g_param_spec_internal (GST_TYPE_PARAM_ARRAY_LIST,
      name, nick, blurb, flags);
  if (aspec == NULL)
    return NULL;

  if (element_spec) {
    aspec->element_spec = g_param_spec_ref (element_spec);
    g_param_spec_sink (element_spec);
  }

  return G_PARAM_SPEC (aspec);
}
