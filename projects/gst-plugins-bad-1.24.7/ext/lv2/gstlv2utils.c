/* GStreamer
 * Copyright (C) 2016 Thibault Saunier <thibault.saunier@collabora.com>
 *               2016 Stefan Sauer <ensonic@users.sf.net>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "string.h"
#include "stdlib.h"

#include "gstlv2.h"
#include "gstlv2utils.h"

#include "lv2/lv2plug.in/ns/ext/atom/atom.h"
#include "lv2/lv2plug.in/ns/ext/atom/forge.h"
#include <lv2/lv2plug.in/ns/ext/log/log.h>
#include <lv2/lv2plug.in/ns/ext/state/state.h>
#include <lv2/lv2plug.in/ns/ext/urid/urid.h>

GST_DEBUG_CATEGORY_EXTERN (lv2_debug);
#define GST_CAT_DEFAULT lv2_debug

/* host features */

/* - log extension */

#ifndef GST_DISABLE_GST_DEBUG
static int
lv2_log_printf (LV2_Log_Handle handle, LV2_URID type, const char *fmt, ...)
{
  va_list ap;

  va_start (ap, fmt);
  gst_debug_log_valist (lv2_debug, GST_LEVEL_INFO, "", "", 0, NULL, fmt, ap);
  va_end (ap);
  return 1;
}

static int
lv2_log_vprintf (LV2_Log_Handle handle, LV2_URID type,
    const char *fmt, va_list ap)
{
  gst_debug_log_valist (lv2_debug, GST_LEVEL_INFO, "", "", 0, NULL, fmt, ap);
  return 1;
}

static LV2_Log_Log lv2_log = {
  /* handle = */ NULL, lv2_log_printf, lv2_log_vprintf
};


static const LV2_Feature lv2_log_feature = { LV2_LOG__log, &lv2_log };
#endif

/* - urid map/unmap extension */

static LV2_URID
lv2_urid_map (LV2_URID_Map_Handle handle, const char *uri)
{
  return (LV2_URID) g_quark_from_string (uri);
}

static const char *
lv2_urid_unmap (LV2_URID_Unmap_Handle handle, LV2_URID urid)
{
  return g_quark_to_string ((GQuark) urid);
}

static LV2_URID_Map lv2_map = {
  /* handle = */ NULL, lv2_urid_map
};

static LV2_URID_Unmap lv2_unmap = {
  /* handle = */ NULL, lv2_urid_unmap
};

static const LV2_Feature lv2_map_feature = { LV2_URID__map, &lv2_map };
static const LV2_Feature lv2_unmap_feature = { LV2_URID__unmap, &lv2_unmap };

/* feature list */

static const LV2_Feature *lv2_features[] = {
#ifndef GST_DISABLE_GST_DEBUG
  &lv2_log_feature,
#endif
  &lv2_map_feature,
  &lv2_unmap_feature,
  NULL
};

gboolean
gst_lv2_check_required_features (const LilvPlugin * lv2plugin)
{
  LilvNodes *required_features = lilv_plugin_get_required_features (lv2plugin);
  if (required_features) {
    LilvIter *i;
    gint j;
    gboolean missing = FALSE;

    for (i = lilv_nodes_begin (required_features);
        !lilv_nodes_is_end (required_features, i);
        i = lilv_nodes_next (required_features, i)) {
      const LilvNode *required_feature = lilv_nodes_get (required_features, i);
      const char *required_feature_uri = lilv_node_as_uri (required_feature);
      missing = TRUE;

      for (j = 0; lv2_features[j]; j++) {
        if (!strcmp (lv2_features[j]->URI, required_feature_uri)) {
          missing = FALSE;
          break;
        }
      }
      if (missing) {
        GST_FIXME ("lv2 plugin %s needs host feature: %s",
            lilv_node_as_uri (lilv_plugin_get_uri (lv2plugin)),
            required_feature_uri);
        break;
      }
    }
    lilv_nodes_free (required_features);
    return (!missing);
  }
  return TRUE;
}

static LV2_Atom_Forge forge;

void
gst_lv2_host_init (void)
{
  lv2_atom_forge_init (&forge, &lv2_map);
}

/* preset interface */

static char *
make_bundle_name (GstObject * obj, const gchar * name)
{
  GstElementFactory *factory;
  gchar *basename, *s, *bundle;

  factory = gst_element_get_factory ((GstElement *) obj);
  basename = g_strdup (gst_element_factory_get_metadata (factory,
          GST_ELEMENT_METADATA_LONGNAME));
  s = basename;
  while ((s = strchr (s, ' '))) {
    *s = '_';
  }
  bundle = g_strjoin (NULL, basename, "_", name, ".preset.lv2", NULL);

  g_free (basename);

  return bundle;
}

gchar **
gst_lv2_get_preset_names (GstLV2 * lv2, GstObject * obj)
{
  /* lazily scan for presets when first called */
  if (!lv2->presets) {
    LilvNodes *presets;

    if ((presets =
            lilv_plugin_get_related (lv2->klass->plugin,
                gst_lv2_preset_node))) {
      LilvIter *j;

      lv2->presets = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
          (GDestroyNotify) lilv_node_free);

      for (j = lilv_nodes_begin (presets);
          !lilv_nodes_is_end (presets, j); j = lilv_nodes_next (presets, j)) {
        const LilvNode *preset = lilv_nodes_get (presets, j);
        LilvNodes *titles;

        lilv_world_load_resource (gst_lv2_world_node, preset);
        titles =
            lilv_world_find_nodes (gst_lv2_world_node, preset,
            gst_lv2_label_pred_node, NULL);
        if (titles) {
          const LilvNode *title = lilv_nodes_get_first (titles);
          g_hash_table_insert (lv2->presets,
              g_strdup (lilv_node_as_string (title)),
              lilv_node_duplicate (preset));
          lilv_nodes_free (titles);
        } else {
          GST_WARNING_OBJECT (obj, "plugin has preset '%s' without rdfs:label",
              lilv_node_as_string (preset));
        }
      }
      lilv_nodes_free (presets);
    }
  }
  if (lv2->presets) {
    GList *node, *keys = g_hash_table_get_keys (lv2->presets);
    gchar **names = g_new0 (gchar *, g_hash_table_size (lv2->presets) + 1);
    gint i = 0;

    for (node = keys; node; node = g_list_next (node)) {
      names[i++] = g_strdup (node->data);
    }
    g_list_free (keys);
    return names;
  }
  return NULL;
}

static void
set_port_value (const char *port_symbol, void *data, const void *value,
    uint32_t size, uint32_t type)
{
  gpointer *user_data = (gpointer *) data;
  GstLV2Class *klass = user_data[0];
  GstObject *obj = user_data[1];
  gchar *prop_name = g_hash_table_lookup (klass->sym_to_name, port_symbol);
  gfloat fvalue;

  if (!prop_name) {
    GST_WARNING_OBJECT (obj, "Preset port '%s' is missing", port_symbol);
    return;
  }

  if (type == forge.Float) {
    fvalue = *(const gfloat *) value;
  } else if (type == forge.Double) {
    fvalue = *(const gdouble *) value;
  } else if (type == forge.Int) {
    fvalue = *(const gint32 *) value;
  } else if (type == forge.Long) {
    fvalue = *(const gint64 *) value;
  } else {
    GST_WARNING_OBJECT (obj, "Preset '%s' value has bad type '%s'",
        port_symbol, lv2_unmap.unmap (lv2_unmap.handle, type));
    return;
  }
  g_object_set (obj, prop_name, fvalue, NULL);
}

gboolean
gst_lv2_load_preset (GstLV2 * lv2, GstObject * obj, const gchar * name)
{
  LilvNode *preset = g_hash_table_lookup (lv2->presets, name);
  LilvState *state =
      lilv_state_new_from_world (gst_lv2_world_node, &lv2_map, preset);
  gpointer user_data[] = { lv2->klass, obj };

  GST_INFO_OBJECT (obj, "loading preset <%s>", lilv_node_as_string (preset));

  lilv_state_restore (state, lv2->instance, set_port_value,
      (gpointer) user_data, 0, NULL);

  lilv_state_free (state);
  return FALSE;
}

static const void *
get_port_value (const char *port_symbol, void *data, uint32_t * size,
    uint32_t * type)
{
  gpointer *user_data = (gpointer *) data;
  GstLV2Class *klass = user_data[0];
  GstObject *obj = user_data[1];
  gchar *prop_name = g_hash_table_lookup (klass->sym_to_name, port_symbol);
  static gfloat fvalue;

  if (!prop_name) {
    GST_WARNING_OBJECT (obj, "Preset port '%s' is missing", port_symbol);
    *size = *type = 0;
    return NULL;
  }

  *size = sizeof (float);
  *type = forge.Float;
  g_object_get (obj, prop_name, &fvalue, NULL);
  /* FIXME: can we return &lv2->ports.{in,out}[x]; */
  return &fvalue;
}

gboolean
gst_lv2_save_preset (GstLV2 * lv2, GstObject * obj, const gchar * name)
{
  gchar *filename, *bundle, *dir, *tmp_dir;
  gpointer user_data[] = { lv2->klass, obj };
  LilvState *state;
  LilvNode *bundle_dir;
  const LilvNode *gst_lv2_state_uri_node;
  LilvInstance *instance = lv2->instance;
  gboolean res;
#ifndef HAVE_LILV_0_22
  gchar *filepath;
#endif

  filename = g_strjoin (NULL, name, ".ttl", NULL);
  bundle = make_bundle_name (obj, name);
  /* dir needs to end on a dir separator for the lilv_new_file_uri() to work */
  dir =
      g_build_filename (g_get_home_dir (), ".lv2", bundle, G_DIR_SEPARATOR_S,
      NULL);
  tmp_dir = g_dir_make_tmp ("gstlv2-XXXXXX", NULL);
  g_mkdir_with_parents (dir, 0750);

  if (!instance) {
    /* instance is NULL until we play!! */
    instance = lilv_plugin_instantiate (lv2->klass->plugin, GST_AUDIO_DEF_RATE,
        lv2_features);
  }

  state = lilv_state_new_from_instance (lv2->klass->plugin, instance, &lv2_map,
      tmp_dir, dir, dir, dir, get_port_value, user_data,
      LV2_STATE_IS_POD | LV2_STATE_IS_PORTABLE, NULL);

  lilv_state_set_label (state, name);

  res = lilv_state_save (gst_lv2_world_node, &lv2_map, &lv2_unmap, state,       /*uri */
      NULL, dir, filename) != 0;

  /* reload bundle into the gst_lv2_world_node */
  bundle_dir = lilv_new_file_uri (gst_lv2_world_node, NULL, dir);
  lilv_world_unload_bundle (gst_lv2_world_node, bundle_dir);
  lilv_world_load_bundle (gst_lv2_world_node, bundle_dir);
  lilv_node_free (bundle_dir);

#ifdef HAVE_LILV_0_22
  gst_lv2_state_uri_node = lilv_state_get_uri (state);
#else
  filepath = g_build_filename (dir, filename, NULL);
  gst_lv2_state_uri_node = lilv_new_uri (gst_lv2_world_node, filepath);
  g_free (filepath);
#endif
  lilv_world_load_resource (gst_lv2_world_node, gst_lv2_state_uri_node);
  g_hash_table_insert (lv2->presets, g_strdup (name),
      lilv_node_duplicate (gst_lv2_state_uri_node));
#ifndef HAVE_LILV_0_22
  lilv_node_free ((LilvNode *) gst_lv2_state_uri_node);
#endif

  lilv_state_free (state);
  if (!lv2->instance) {
    lilv_instance_free (instance);
  }

  g_free (tmp_dir);
  g_free (dir);
  g_free (bundle);
  g_free (filename);

  return res;
}

#if 0
gboolean
gst_lv2_rename_preset (GstLV2 * lv2, GstObject * obj,
    const gchar * old_name, const gchar * new_name)
{
  /* need to relabel the preset */
  return FALSE;
}
#endif

gboolean
gst_lv2_delete_preset (GstLV2 * lv2, GstObject * obj, const gchar * name)
{
#ifdef HAVE_LILV_0_22
  LilvNode *preset = g_hash_table_lookup (lv2->presets, name);
  LilvState *state =
      lilv_state_new_from_world (gst_lv2_world_node, &lv2_map, preset);

  lilv_world_unload_resource (gst_lv2_world_node, lilv_state_get_uri (state));
  lilv_state_delete (gst_lv2_world_node, state);
  lilv_state_free (state);
#endif
  g_hash_table_remove (lv2->presets, name);

  return FALSE;
}

/* api helpers */

void
gst_lv2_init (GstLV2 * lv2, GstLV2Class * lv2_class)
{
  lv2->klass = lv2_class;

  lv2->instance = NULL;
  lv2->activated = FALSE;

  lv2->ports.control.in = g_new0 (gfloat, lv2_class->control_in_ports->len);
  lv2->ports.control.out = g_new0 (gfloat, lv2_class->control_out_ports->len);
}

void
gst_lv2_finalize (GstLV2 * lv2)
{
  if (lv2->presets) {
    g_hash_table_destroy (lv2->presets);
  }
  g_free (lv2->ports.control.in);
  g_free (lv2->ports.control.out);
}

gboolean
gst_lv2_setup (GstLV2 * lv2, unsigned long rate)
{
  GstLV2Class *lv2_class = lv2->klass;
  GstLV2Port *port;
  GArray *ports;
  gint i;

  if (lv2->instance)
    lilv_instance_free (lv2->instance);

  if (!(lv2->instance =
          lilv_plugin_instantiate (lv2_class->plugin, rate, lv2_features)))
    return FALSE;

  /* connect the control ports */
  ports = lv2_class->control_in_ports;
  for (i = 0; i < ports->len; i++) {
    port = &g_array_index (ports, GstLV2Port, i);
    if (port->type != GST_LV2_PORT_CONTROL)
      continue;
    lilv_instance_connect_port (lv2->instance, port->index,
        &(lv2->ports.control.in[i]));
  }
  ports = lv2_class->control_out_ports;
  for (i = 0; i < ports->len; i++) {
    port = &g_array_index (ports, GstLV2Port, i);
    if (port->type != GST_LV2_PORT_CONTROL)
      continue;
    lilv_instance_connect_port (lv2->instance, port->index,
        &(lv2->ports.control.out[i]));
  }

  lilv_instance_activate (lv2->instance);
  lv2->activated = TRUE;

  return TRUE;
}

gboolean
gst_lv2_cleanup (GstLV2 * lv2, GstObject * obj)
{
  if (lv2->activated == FALSE) {
    GST_ERROR_OBJECT (obj, "Deactivating but LV2 plugin not activated");
    return TRUE;
  }

  if (lv2->instance == NULL) {
    GST_ERROR_OBJECT (obj, "Deactivating but no LV2 plugin set");
    return TRUE;
  }

  GST_DEBUG_OBJECT (obj, "deactivating");

  lilv_instance_deactivate (lv2->instance);

  lv2->activated = FALSE;

  lilv_instance_free (lv2->instance);
  lv2->instance = NULL;

  return TRUE;
}

void
gst_lv2_object_set_property (GstLV2 * lv2, GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GType base, type = pspec->value_type;
  /* remember, properties have an offset */
  prop_id -= lv2->klass->properties;

  /* only input ports */
  g_return_if_fail (prop_id < lv2->klass->control_in_ports->len);

  while ((base = g_type_parent (type)))
    type = base;

  /* now see what type it is */
  switch (type) {
    case G_TYPE_BOOLEAN:
      lv2->ports.control.in[prop_id] =
          g_value_get_boolean (value) ? 1.0f : 0.0f;
      break;
    case G_TYPE_INT:
      lv2->ports.control.in[prop_id] = g_value_get_int (value);
      break;
    case G_TYPE_FLOAT:
      lv2->ports.control.in[prop_id] = g_value_get_float (value);
      break;
    case G_TYPE_ENUM:
      lv2->ports.control.in[prop_id] = g_value_get_enum (value);
      break;
    default:
      GST_WARNING_OBJECT (object, "unhandled type: %s",
          g_type_name (pspec->value_type));
      g_assert_not_reached ();
  }
}

void
gst_lv2_object_get_property (GstLV2 * lv2, GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GType base, type = pspec->value_type;
  gfloat *controls;

  /* remember, properties have an offset */
  prop_id -= lv2->klass->properties;

  if (prop_id < lv2->klass->control_in_ports->len) {
    controls = lv2->ports.control.in;
  } else if (prop_id < lv2->klass->control_in_ports->len +
      lv2->klass->control_out_ports->len) {
    controls = lv2->ports.control.out;
    prop_id -= lv2->klass->control_in_ports->len;
  } else {
    g_return_if_reached ();
  }

  while ((base = g_type_parent (type)))
    type = base;

  /* now see what type it is */
  switch (type) {
    case G_TYPE_BOOLEAN:
      g_value_set_boolean (value, controls[prop_id] > 0.0f);
      break;
    case G_TYPE_INT:{
      gint64 ival = CLAMP ((gint64) controls[prop_id], G_MININT, G_MAXINT);
      g_value_set_int (value, ival);
      break;
    }
    case G_TYPE_FLOAT:
      g_value_set_float (value, controls[prop_id]);
      break;
    case G_TYPE_ENUM:
      g_value_set_enum (value, (gint) controls[prop_id]);
      break;
    default:
      GST_WARNING_OBJECT (object, "unhandled type: %s",
          g_type_name (pspec->value_type));
      g_return_if_reached ();
  }
}


static gchar *
gst_lv2_class_get_param_name (GstLV2Class * klass, GObjectClass * object_class,
    const gchar * port_symbol)
{
  gchar *ret = g_strdup (port_symbol);

  /* this is the same thing that param_spec_* will do */
  g_strcanon (ret, G_CSET_A_2_Z G_CSET_a_2_z G_CSET_DIGITS "-", '-');
  /* satisfy glib2 (argname[0] must be [A-Za-z]) */
  if (!((ret[0] >= 'a' && ret[0] <= 'z') || (ret[0] >= 'A' && ret[0] <= 'Z'))) {
    gchar *tempstr = ret;

    ret = g_strconcat ("param-", ret, NULL);
    g_free (tempstr);
  }

  /* check for duplicate property names */
  if (g_object_class_find_property (object_class, ret)) {
    gint n = 1;
    gchar *nret = g_strdup_printf ("%s-%d", ret, n++);

    while (g_object_class_find_property (object_class, nret)) {
      g_free (nret);
      nret = g_strdup_printf ("%s-%d", ret, n++);
    }
    g_free (ret);
    ret = nret;
  }

  GST_DEBUG ("built property name '%s' from port name '%s'", ret, port_symbol);
  return ret;
}

static gchar *
gst_lv2_class_get_param_nick (GstLV2Class * klass, const LilvPort * port)
{
  const LilvPlugin *lv2plugin = klass->plugin;

  return g_strdup (lilv_node_as_string (lilv_port_get_name (lv2plugin, port)));
}

static int
enum_val_cmp (GEnumValue * p1, GEnumValue * p2)
{
  return p1->value - p2->value;
}

static GParamSpec *
gst_lv2_class_get_param_spec (GstLV2Class * klass, GObjectClass * object_class,
    gint portnum)
{
  const LilvPlugin *lv2plugin = klass->plugin;
  const LilvPort *port = lilv_plugin_get_port_by_index (lv2plugin, portnum);
  LilvNode *lv2def, *lv2min, *lv2max;
  LilvScalePoints *points;
  GParamSpec *ret;
  gchar *name, *nick;
  gint perms;
  gfloat lower = 0.0f, upper = 1.0f, def = 0.0f;
  GType enum_type = G_TYPE_INVALID;
  const gchar *port_symbol =
      lilv_node_as_string (lilv_port_get_symbol (lv2plugin, port));

  nick = gst_lv2_class_get_param_nick (klass, port);
  name = gst_lv2_class_get_param_name (klass, object_class, port_symbol);

  GST_DEBUG ("%s trying port %s : %s",
      lilv_node_as_string (lilv_plugin_get_uri (lv2plugin)), name, nick);

  perms = G_PARAM_READABLE;
  if (lilv_port_is_a (lv2plugin, port, gst_lv2_input_node))
    perms |= G_PARAM_WRITABLE | G_PARAM_CONSTRUCT;
  if (lilv_port_is_a (lv2plugin, port, gst_lv2_control_node) ||
      lilv_port_is_a (lv2plugin, port, gst_lv2_cv_node))
    perms |= GST_PARAM_CONTROLLABLE;

  if (lilv_port_has_property (lv2plugin, port, gst_lv2_toggled_prop_node)) {
    ret = g_param_spec_boolean (name, nick, nick, FALSE, perms);
    goto done;
  }

  lilv_port_get_range (lv2plugin, port, &lv2def, &lv2min, &lv2max);

  if (lv2def)
    def = lilv_node_as_float (lv2def);
  if (lv2min)
    lower = lilv_node_as_float (lv2min);
  if (lv2max)
    upper = lilv_node_as_float (lv2max);

  lilv_node_free (lv2def);
  lilv_node_free (lv2min);
  lilv_node_free (lv2max);

  if (def < lower) {
    if (lv2def && lv2min) {
      GST_WARNING ("%s:%s has lower bound %f > default %f",
          lilv_node_as_string (lilv_plugin_get_uri (lv2plugin)), name, lower,
          def);
    }
    lower = def;
  }

  if (def > upper) {
    if (lv2def && lv2max) {
      GST_WARNING ("%s:%s has upper bound %f < default %f",
          lilv_node_as_string (lilv_plugin_get_uri (lv2plugin)), name, upper,
          def);
    }
    upper = def;
  }

  if ((points = lilv_port_get_scale_points (lv2plugin, port))) {
    GEnumValue *enums;
    LilvIter *i;
    gint j = 0, n, def_ix = -1;

    n = lilv_scale_points_size (points);
    enums = g_new (GEnumValue, n + 1);

    for (i = lilv_scale_points_begin (points);
        !lilv_scale_points_is_end (points, i);
        i = lilv_scale_points_next (points, i)) {
      const LilvScalePoint *point = lilv_scale_points_get (points, i);
      gfloat v = lilv_node_as_float (lilv_scale_point_get_value (point));
      const gchar *l = lilv_node_as_string (lilv_scale_point_get_label (point));

      /* check if value can be safely converted to int */
      if (v != (gint) v) {
        GST_INFO ("%s:%s non integer scale point %lf, %s",
            lilv_node_as_string (lilv_plugin_get_uri (lv2plugin)), name, v, l);
        break;
      }
      if (v == def) {
        def_ix = j;
      }
      enums[j].value = (gint) v;
      enums[j].value_nick = enums[j].value_name = l;
      GST_LOG ("%s:%s enum: %lf, %s",
          lilv_node_as_string (lilv_plugin_get_uri (lv2plugin)), name, v, l);
      j++;
    }
    if (j == n) {
      gchar *type_name;

      /* scalepoints are not sorted */
      qsort (enums, n, sizeof (GEnumValue),
          (int (*)(const void *, const void *)) enum_val_cmp);

      if (def_ix == -1) {
        if (lv2def) {
          GST_WARNING ("%s:%s has default %f outside of scalepoints",
              lilv_node_as_string (lilv_plugin_get_uri (lv2plugin)), name, def);
        }
        def = enums[0].value;
      }
      /* terminator */
      enums[j].value = 0;
      enums[j].value_name = enums[j].value_nick = NULL;

      type_name = g_strdup_printf ("%s%s",
          g_type_name (G_TYPE_FROM_CLASS (object_class)), name);
      enum_type = g_enum_register_static (type_name, enums);
      gst_type_mark_as_plugin_api (enum_type, 0);
      g_free (type_name);
    } else {
      g_free (enums);
    }
    lilv_scale_points_free (points);
  }

  if (enum_type != G_TYPE_INVALID) {
    ret = g_param_spec_enum (name, nick, nick, enum_type, def, perms);
  } else if (lilv_port_has_property (lv2plugin, port,
          gst_lv2_integer_prop_node))
    ret = g_param_spec_int (name, nick, nick, lower, upper, def, perms);
  else
    ret = g_param_spec_float (name, nick, nick, lower, upper, def, perms);

done:
  // build a map of (port_symbol to ret->name) for extensions
  g_hash_table_insert (klass->sym_to_name, (gchar *) port_symbol,
      (gchar *) ret->name);

  g_free (name);
  g_free (nick);

  return ret;
}

void
gst_lv2_class_install_properties (GstLV2Class * lv2_class,
    GObjectClass * object_class, guint offset)
{
  GParamSpec *p;
  guint i;

  lv2_class->properties = offset;

  for (i = 0; i < lv2_class->control_in_ports->len; i++, offset++) {
    p = gst_lv2_class_get_param_spec (lv2_class, object_class,
        g_array_index (lv2_class->control_in_ports, GstLV2Port, i).index);

    g_object_class_install_property (object_class, offset, p);
  }

  for (i = 0; i < lv2_class->control_out_ports->len; i++, offset++) {
    p = gst_lv2_class_get_param_spec (lv2_class, object_class,
        g_array_index (lv2_class->control_out_ports, GstLV2Port, i).index);

    g_object_class_install_property (object_class, offset, p);
  }
}

void
gst_lv2_element_class_set_metadata (GstLV2Class * lv2_class,
    GstElementClass * elem_class, const gchar * lv2_class_tags)
{
  const LilvPlugin *lv2plugin = lv2_class->plugin;
  LilvNode *val;
  const LilvPluginClass *lv2plugin_class;
  const LilvNode *cval;
  gchar *longname, *author, *class_tags = NULL;

  val = lilv_plugin_get_name (lv2plugin);
  if (val) {
    longname = g_strdup (lilv_node_as_string (val));
    lilv_node_free (val);
  } else {
    longname = g_strdup ("no description available");
  }
  val = lilv_plugin_get_author_name (lv2plugin);
  if (val) {
    // TODO: check lilv_plugin_get_author_email(lv2plugin);
    author = g_strdup (lilv_node_as_string (val));
    lilv_node_free (val);
  } else {
    author = g_strdup ("no author available");
  }

  // TODO: better description from:
  // lilv_plugin_get_author_homepage() and lilv_plugin_get_project()

  lv2plugin_class = lilv_plugin_get_class (lv2plugin);
  cval = lilv_plugin_class_get_label (lv2plugin_class);
  if (cval) {
    class_tags = g_strconcat (lv2_class_tags, "/", lilv_node_as_string (cval),
        NULL);
  }

  gst_element_class_set_metadata (elem_class, longname,
      (class_tags ? class_tags : lv2_class_tags), longname, author);
  g_free (longname);
  g_free (author);
  g_free (class_tags);
}


void
gst_lv2_class_init (GstLV2Class * lv2_class, GType type)
{
  const GValue *value =
      gst_structure_get_value (lv2_meta_all, g_type_name (type));
  GstStructure *lv2_meta = g_value_get_boxed (value);
  const LilvPlugin *lv2plugin;
  guint j, in_pad_index = 0, out_pad_index = 0;
  const LilvPlugins *plugins = lilv_world_get_all_plugins (gst_lv2_world_node);
  LilvNode *plugin_uri;
  const gchar *element_uri;

  GST_DEBUG ("LV2 initializing class");

  element_uri = gst_structure_get_string (lv2_meta, "element-uri");
  plugin_uri = lilv_new_uri (gst_lv2_world_node, element_uri);
  g_assert (plugin_uri);
  lv2plugin = lilv_plugins_get_by_uri (plugins, plugin_uri);
  g_assert (lv2plugin);
  lv2_class->plugin = lv2plugin;
  lilv_node_free (plugin_uri);

  lv2_class->sym_to_name = g_hash_table_new (g_str_hash, g_str_equal);

  lv2_class->in_group.ports = g_array_new (FALSE, TRUE, sizeof (GstLV2Port));
  lv2_class->out_group.ports = g_array_new (FALSE, TRUE, sizeof (GstLV2Port));
  lv2_class->control_in_ports = g_array_new (FALSE, TRUE, sizeof (GstLV2Port));
  lv2_class->control_out_ports = g_array_new (FALSE, TRUE, sizeof (GstLV2Port));

  /* find ports and groups */
  for (j = 0; j < lilv_plugin_get_num_ports (lv2plugin); j++) {
    const LilvPort *port = lilv_plugin_get_port_by_index (lv2plugin, j);
    const gboolean is_input =
        lilv_port_is_a (lv2plugin, port, gst_lv2_input_node);
    const gboolean is_optional = lilv_port_has_property (lv2plugin, port,
        gst_lv2_optional_pred_node);
    GstLV2Port desc = { j, GST_LV2_PORT_AUDIO, -1, };
    LilvNodes *lv2group =
        lilv_port_get (lv2plugin, port, gst_lv2_group_pred_node);
    /* FIXME Handle channels positioning
     * GstAudioChannelPosition position = GST_AUDIO_CHANNEL_POSITION_INVALID; */

    if (lv2group) {
      /* port is part of a group */
      const gchar *group_uri = lilv_node_as_uri (lv2group);
      GstLV2Group *group = is_input
          ? &lv2_class->in_group : &lv2_class->out_group;

      if (group->uri == NULL) {
        group->uri = g_strdup (group_uri);
        group->pad = is_input ? in_pad_index++ : out_pad_index++;
        group->ports = g_array_new (FALSE, TRUE, sizeof (GstLV2Port));
      }

      /* FIXME Handle channels positioning
         position = GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT;
         sub_values = lilv_port_get_value (lv2plugin, port, gst_lv2_designation_pred_node);
         if (lilv_nodes_size (sub_values) > 0) {
         LilvNode *role = lilv_nodes_get_at (sub_values, 0);
         position = gst_lv2_filter_role_to_position (role);
         }
         lilv_nodes_free (sub_values);

         if (position != GST_AUDIO_CHANNEL_POSITION_INVALID) {
         desc.position = position;
         } */

      g_array_append_val (group->ports, desc);
    } else {
      /* port is not part of a group, or it is part of a group but that group
       * is illegal so we just ignore it */
      if (lilv_port_is_a (lv2plugin, port, gst_lv2_audio_node)) {
        if (is_input) {
          desc.pad = in_pad_index++;
          g_array_append_val (lv2_class->in_group.ports, desc);
        } else {
          desc.pad = out_pad_index++;
          g_array_append_val (lv2_class->out_group.ports, desc);
        }
      } else if (lilv_port_is_a (lv2plugin, port, gst_lv2_control_node)) {
        desc.type = GST_LV2_PORT_CONTROL;
        if (is_input) {
          lv2_class->num_control_in++;
          g_array_append_val (lv2_class->control_in_ports, desc);
        } else {
          lv2_class->num_control_out++;
          g_array_append_val (lv2_class->control_out_ports, desc);
        }
      } else if (lilv_port_is_a (lv2plugin, port, gst_lv2_cv_node)) {
        desc.type = GST_LV2_PORT_CV;
        if (is_input) {
          lv2_class->num_cv_in++;
          g_array_append_val (lv2_class->control_in_ports, desc);
        } else {
          lv2_class->num_cv_out++;
          g_array_append_val (lv2_class->control_out_ports, desc);
        }
      } else if (lilv_port_is_a (lv2plugin, port, gst_lv2_event_node)) {
        LilvNodes *supported = lilv_port_get_value (lv2plugin, port,
            gst_lv2_supports_event_pred_node);

        GST_INFO ("%s: unhandled event port %d: %s, optional=%d, input=%d",
            element_uri, j,
            lilv_node_as_string (lilv_port_get_symbol (lv2plugin, port)),
            is_optional, is_input);

        if (lilv_nodes_size (supported) > 0) {
          LilvIter *i;

          for (i = lilv_nodes_begin (supported);
              !lilv_nodes_is_end (supported, i);
              i = lilv_nodes_next (supported, i)) {
            const LilvNode *value = lilv_nodes_get (supported, i);
            GST_INFO ("  type = %s", lilv_node_as_uri (value));
          }
        }
        lilv_nodes_free (supported);
        // FIXME: handle them
      } else {
        /* unhandled port type */
        const LilvNodes *classes = lilv_port_get_classes (lv2plugin, port);
        GST_INFO ("%s: unhandled port %d: %s, optional=%d, input=%d",
            element_uri, j,
            lilv_node_as_string (lilv_port_get_symbol (lv2plugin, port)),
            is_optional, is_input);
        if (classes && lilv_nodes_size (classes) > 0) {
          LilvIter *i;

          // FIXME: we getting the same classe multiple times
          for (i = lilv_nodes_begin (classes);
              !lilv_nodes_is_end (classes, i);
              i = lilv_nodes_next (classes, i)) {
            const LilvNode *value = lilv_nodes_get (classes, i);
            GST_INFO ("  class = %s", lilv_node_as_uri (value));
          }
        }
      }
    }
  }
}

void
gst_lv2_class_finalize (GstLV2Class * lv2_class)
{
  GST_DEBUG ("LV2 finalizing class");

  g_hash_table_destroy (lv2_class->sym_to_name);

  g_array_free (lv2_class->in_group.ports, TRUE);
  lv2_class->in_group.ports = NULL;
  g_array_free (lv2_class->out_group.ports, TRUE);
  lv2_class->out_group.ports = NULL;
  g_array_free (lv2_class->control_in_ports, TRUE);
  lv2_class->control_in_ports = NULL;
  g_array_free (lv2_class->control_out_ports, TRUE);
  lv2_class->control_out_ports = NULL;
}
