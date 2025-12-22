/* GStreamer
 * Copyright (C) 2009 Sebastian Dröge <sebastian.droege@collabora.co.uk>
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

#include "gstfrei0r.h"
#include "gstfrei0rfilter.h"
#include "gstfrei0rsrc.h"
#include "gstfrei0rmixer.h"

#include <string.h>
#include <gmodule.h>

GST_DEBUG_CATEGORY (frei0r_debug);
#define GST_CAT_DEFAULT frei0r_debug

static GstStaticCaps bgra8888_caps = GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE
    ("BGRA"));
static GstStaticCaps rgba8888_caps = GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE
    ("RGBA"));
static GstStaticCaps packed32_caps = GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE
    ("{ BGRA, RGBA, ABGR, ARGB, BGRx, RGBx, xBGR, xRGB, AYUV }"));

GstCaps *
gst_frei0r_caps_from_color_model (gint color_model)
{
  switch (color_model) {
    case F0R_COLOR_MODEL_BGRA8888:
      return gst_static_caps_get (&bgra8888_caps);
    case F0R_COLOR_MODEL_RGBA8888:
      return gst_static_caps_get (&rgba8888_caps);
    case F0R_COLOR_MODEL_PACKED32:
      return gst_static_caps_get (&packed32_caps);
    default:
      break;
  }

  return NULL;
}

void
gst_frei0r_klass_install_properties (GObjectClass * gobject_class,
    GstFrei0rFuncTable * ftable, GstFrei0rProperty * properties,
    gint n_properties)
{
  gint i, count = 1;
  f0r_instance_t *instance = ftable->construct (640, 480);

  g_assert (instance);

  for (i = 0; i < n_properties; i++) {
    f0r_param_info_t *param_info = &properties[i].info;
    gchar *prop_name;

    ftable->get_param_info (param_info, i);

    if (!param_info->name) {
      GST_ERROR ("Property %d of %s without a valid name", i,
          g_type_name (G_TYPE_FROM_CLASS (gobject_class)));
      continue;
    }

    prop_name = g_ascii_strdown (param_info->name, -1);
    g_strcanon (prop_name, G_CSET_A_2_Z G_CSET_a_2_z G_CSET_DIGITS "-+", '-');
    /* satisfy glib2 (argname[0] must be [A-Za-z]) */
    if (!((prop_name[0] >= 'a' && prop_name[0] <= 'z') ||
            (prop_name[0] >= 'A' && prop_name[0] <= 'Z'))) {
      gchar *tempstr = prop_name;

      prop_name = g_strconcat ("param-", prop_name, NULL);
      g_free (tempstr);
    }

    properties[i].prop_id = count;
    properties[i].prop_idx = i;

    ftable->get_param_value (instance, &properties[i].default_value, i);
    if (param_info->type == F0R_PARAM_STRING)
      properties[i].default_value.data.s =
          g_strdup (properties[i].default_value.data.s);

    switch (param_info->type) {
      case F0R_PARAM_BOOL:
        g_object_class_install_property (gobject_class, count++,
            g_param_spec_boolean (prop_name, param_info->name,
                param_info->explanation,
                properties[i].default_value.data.b ? TRUE : FALSE,
                G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE |
                GST_PARAM_DOC_SHOW_DEFAULT));
        properties[i].n_prop_ids = 1;
        break;
      case F0R_PARAM_DOUBLE:{
        gdouble def = properties[i].default_value.data.d;

        /* If the default is NAN, +-INF we use 0.0 */
        if (!(def >= 0.0 && def <= 1.0))
          def = 0.0;

        g_object_class_install_property (gobject_class, count++,
            g_param_spec_double (prop_name, param_info->name,
                param_info->explanation, 0.0, 1.0, def,
                G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE |
                GST_PARAM_DOC_SHOW_DEFAULT));
        properties[i].n_prop_ids = 1;
        break;
      }
      case F0R_PARAM_STRING:
        g_object_class_install_property (gobject_class, count++,
            g_param_spec_string (prop_name, param_info->name,
                param_info->explanation, properties[i].default_value.data.s,
                G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE |
                GST_PARAM_DOC_SHOW_DEFAULT));
        properties[i].n_prop_ids = 1;
        break;
      case F0R_PARAM_COLOR:{
        gchar *prop_name_full;
        gchar *prop_nick_full;
        gdouble def;

        def = properties[i].default_value.data.color.r;
        /* If the default is out of range we use 0.0 */
        if (!(def <= 1.0 && def >= 0.0))
          def = 0.0;
        prop_name_full = g_strconcat (prop_name, "-r", NULL);
        prop_nick_full = g_strconcat (param_info->name, " (R)", NULL);
        g_object_class_install_property (gobject_class, count++,
            g_param_spec_float (prop_name_full, prop_nick_full,
                param_info->explanation, 0.0, 1.0, def,
                G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE |
                GST_PARAM_DOC_SHOW_DEFAULT));
        g_free (prop_name_full);
        g_free (prop_nick_full);

        def = properties[i].default_value.data.color.g;
        /* If the default is out of range we use 0.0 */
        if (!(def <= 1.0 && def >= 0.0))
          def = 0.0;
        prop_name_full = g_strconcat (prop_name, "-g", NULL);
        prop_nick_full = g_strconcat (param_info->name, " (G)", NULL);
        g_object_class_install_property (gobject_class, count++,
            g_param_spec_float (prop_name_full, prop_nick_full,
                param_info->explanation, 0.0, 1.0, def,
                G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE |
                GST_PARAM_DOC_SHOW_DEFAULT));
        g_free (prop_name_full);
        g_free (prop_nick_full);

        def = properties[i].default_value.data.color.b;
        /* If the default is out of range we use 0.0 */
        if (!(def <= 1.0 && def >= 0.0))
          def = 0.0;
        prop_name_full = g_strconcat (prop_name, "-b", NULL);
        prop_nick_full = g_strconcat (param_info->name, " (B)", NULL);
        g_object_class_install_property (gobject_class, count++,
            g_param_spec_float (prop_name_full, prop_nick_full,
                param_info->explanation, 0.0, 1.0, def,
                G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE |
                GST_PARAM_DOC_SHOW_DEFAULT));
        g_free (prop_name_full);
        g_free (prop_nick_full);

        properties[i].n_prop_ids = 3;
        break;
      }
      case F0R_PARAM_POSITION:{
        gchar *prop_name_full;
        gchar *prop_nick_full;
        gdouble def;

        def = properties[i].default_value.data.position.x;
        /* If the default is out of range we use 0.0 */
        if (!(def <= 1.0 && def >= 0.0))
          def = 0.0;
        prop_name_full = g_strconcat (prop_name, "-x", NULL);
        prop_nick_full = g_strconcat (param_info->name, " (X)", NULL);
        g_object_class_install_property (gobject_class, count++,
            g_param_spec_double (prop_name_full, prop_nick_full,
                param_info->explanation, 0.0, 1.0, def,
                G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE |
                GST_PARAM_DOC_SHOW_DEFAULT));
        g_free (prop_name_full);
        g_free (prop_nick_full);

        def = properties[i].default_value.data.position.y;
        /* If the default is out of range we use 0.0 */
        if (!(def <= 1.0 && def >= 0.0))
          def = 0.0;
        prop_name_full = g_strconcat (prop_name, "-Y", NULL);
        prop_nick_full = g_strconcat (param_info->name, " (Y)", NULL);
        g_object_class_install_property (gobject_class, count++,
            g_param_spec_double (prop_name_full, prop_nick_full,
                param_info->explanation, 0.0, 1.0, def,
                G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE |
                GST_PARAM_DOC_SHOW_DEFAULT));
        g_free (prop_name_full);
        g_free (prop_nick_full);

        properties[i].n_prop_ids = 2;
        break;
      }
      default:
        g_assert_not_reached ();
        break;
    }

    g_free (prop_name);
  }

  ftable->destruct (instance);
}

GstFrei0rPropertyValue *
gst_frei0r_property_cache_init (GstFrei0rProperty * properties,
    gint n_properties)
{
  gint i;
  GstFrei0rPropertyValue *ret = g_new0 (GstFrei0rPropertyValue, n_properties);

  for (i = 0; i < n_properties; i++) {
    memcpy (&ret[i].data, &properties[i].default_value,
        sizeof (GstFrei0rPropertyValue));

    if (properties[i].info.type == F0R_PARAM_STRING)
      ret[i].data.s = g_strdup (ret[i].data.s);
  }

  return ret;
}

void
gst_frei0r_property_cache_free (GstFrei0rProperty * properties,
    GstFrei0rPropertyValue * property_cache, gint n_properties)
{
  gint i;

  for (i = 0; i < n_properties; i++) {
    if (properties[i].info.type == F0R_PARAM_STRING)
      g_free (property_cache[i].data.s);
  }
  g_free (property_cache);
}

f0r_instance_t *
gst_frei0r_instance_construct (GstFrei0rFuncTable * ftable,
    GstFrei0rProperty * properties, gint n_properties,
    GstFrei0rPropertyValue * property_cache, gint width, gint height)
{
  f0r_instance_t *instance = ftable->construct (width, height);
  gint i;

  for (i = 0; i < n_properties; i++)
    ftable->set_param_value (instance, &property_cache[i].data, i);

  return instance;
}

gboolean
gst_frei0r_get_property (f0r_instance_t * instance, GstFrei0rFuncTable * ftable,
    GstFrei0rProperty * properties, gint n_properties,
    GstFrei0rPropertyValue * property_cache, guint prop_id, GValue * value)
{
  gint i;
  GstFrei0rProperty *prop = NULL;

  for (i = 0; i < n_properties; i++) {
    if (properties[i].prop_id <= prop_id &&
        properties[i].prop_id + properties[i].n_prop_ids > prop_id) {
      prop = &properties[i];
      break;
    }
  }

  if (!prop)
    return FALSE;

  switch (prop->info.type) {
    case F0R_PARAM_BOOL:{
      gdouble d;

      if (instance)
        ftable->get_param_value (instance, &d, prop->prop_idx);
      else
        d = property_cache[prop->prop_idx].data.b;

      g_value_set_boolean (value, (d < 0.5) ? FALSE : TRUE);
      break;
    }
    case F0R_PARAM_DOUBLE:{
      gdouble d;

      if (instance)
        ftable->get_param_value (instance, &d, prop->prop_idx);
      else
        d = property_cache[prop->prop_idx].data.d;

      g_value_set_double (value, d);
      break;
    }
    case F0R_PARAM_STRING:{
      gchar *s;

      if (instance)
        ftable->get_param_value (instance, &s, prop->prop_idx);
      else
        s = property_cache[prop->prop_idx].data.s;
      g_value_set_string (value, s);
      break;
    }
    case F0R_PARAM_COLOR:{
      f0r_param_color_t color;

      if (instance)
        ftable->get_param_value (instance, &color, prop->prop_idx);
      else
        color = property_cache[prop->prop_idx].data.color;

      switch (prop_id - prop->prop_id) {
        case 0:
          g_value_set_float (value, color.r);
          break;
        case 1:
          g_value_set_float (value, color.g);
          break;
        case 2:
          g_value_set_float (value, color.b);
          break;
      }
      break;
    }
    case F0R_PARAM_POSITION:{
      f0r_param_position_t position;

      if (instance)
        ftable->get_param_value (instance, &position, prop->prop_idx);
      else
        position = property_cache[prop->prop_idx].data.position;

      switch (prop_id - prop->prop_id) {
        case 0:
          g_value_set_double (value, position.x);
          break;
        case 1:
          g_value_set_double (value, position.y);
          break;
      }
      break;
    }
    default:
      g_assert_not_reached ();
      break;
  }

  return TRUE;
}

gboolean
gst_frei0r_set_property (f0r_instance_t * instance, GstFrei0rFuncTable * ftable,
    GstFrei0rProperty * properties, gint n_properties,
    GstFrei0rPropertyValue * property_cache, guint prop_id,
    const GValue * value)
{
  GstFrei0rProperty *prop = NULL;
  gint i;

  for (i = 0; i < n_properties; i++) {
    if (properties[i].prop_id <= prop_id &&
        properties[i].prop_id + properties[i].n_prop_ids > prop_id) {
      prop = &properties[i];
      break;
    }
  }

  if (!prop)
    return FALSE;

  switch (prop->info.type) {
    case F0R_PARAM_BOOL:{
      gboolean b = g_value_get_boolean (value);
      gdouble d = b ? 1.0 : 0.0;

      if (instance)
        ftable->set_param_value (instance, &d, prop->prop_idx);
      property_cache[prop->prop_idx].data.b = d;
      break;
    }
    case F0R_PARAM_DOUBLE:{
      gdouble d = g_value_get_double (value);

      if (instance)
        ftable->set_param_value (instance, &d, prop->prop_idx);
      property_cache[prop->prop_idx].data.d = d;
      break;
    }
    case F0R_PARAM_STRING:{
      gchar *s = g_value_dup_string (value);

      /* Copies the string */
      if (instance)
        ftable->set_param_value (instance, s, prop->prop_idx);
      property_cache[prop->prop_idx].data.s = s;
      break;
    }
    case F0R_PARAM_COLOR:{
      gfloat f = g_value_get_float (value);
      f0r_param_color_t *color = &property_cache[prop->prop_idx].data.color;

      switch (prop_id - prop->prop_id) {
        case 0:
          color->r = f;
          break;
        case 1:
          color->g = f;
          break;
        case 2:
          color->b = f;
          break;
        default:
          g_assert_not_reached ();
      }

      if (instance)
        ftable->set_param_value (instance, color, prop->prop_idx);
      break;
    }
    case F0R_PARAM_POSITION:{
      gdouble d = g_value_get_double (value);
      f0r_param_position_t *position =
          &property_cache[prop->prop_idx].data.position;

      switch (prop_id - prop->prop_id) {
        case 0:
          position->x = d;
          break;
        case 1:
          position->y = d;
          break;
        default:
          g_assert_not_reached ();
      }
      if (instance)
        ftable->set_param_value (instance, position, prop->prop_idx);
      break;
    }
    default:
      g_assert_not_reached ();
      break;
  }

  return TRUE;
}

static gboolean
register_plugin (GstPlugin * plugin, const gchar * vendor,
    const gchar * filename)
{
  GModule *module;
  GstFrei0rPluginRegisterReturn ret = GST_FREI0R_PLUGIN_REGISTER_RETURN_FAILED;
  GstFrei0rFuncTable ftable = { NULL, };
  gint i;
  f0r_plugin_info_t info = { NULL, };
  f0r_instance_t *instance = NULL;

  GST_DEBUG ("Registering plugin '%s'", filename);

  module = g_module_open (filename, G_MODULE_BIND_LAZY | G_MODULE_BIND_LOCAL);
  if (!module) {
    GST_WARNING ("Failed to load plugin");
    return FALSE;
  }

  if (!g_module_symbol (module, "f0r_init", (gpointer *) & ftable.init)) {
    GST_INFO ("No frei0r plugin");
    g_module_close (module);
    return FALSE;
  }

  if (!g_module_symbol (module, "f0r_deinit", (gpointer *) & ftable.deinit) ||
      !g_module_symbol (module, "f0r_construct",
          (gpointer *) & ftable.construct)
      || !g_module_symbol (module, "f0r_destruct",
          (gpointer *) & ftable.destruct)
      || !g_module_symbol (module, "f0r_get_plugin_info",
          (gpointer *) & ftable.get_plugin_info)
      || !g_module_symbol (module, "f0r_get_param_info",
          (gpointer *) & ftable.get_param_info)
      || !g_module_symbol (module, "f0r_set_param_value",
          (gpointer *) & ftable.set_param_value)
      || !g_module_symbol (module, "f0r_get_param_value",
          (gpointer *) & ftable.get_param_value))
    goto invalid_frei0r_plugin;

  /* One of these must exist */
  g_module_symbol (module, "f0r_update", (gpointer *) & ftable.update);
  g_module_symbol (module, "f0r_update2", (gpointer *) & ftable.update2);

  if (!ftable.init ()) {
    GST_WARNING ("Failed to initialize plugin");
    g_module_close (module);
    return FALSE;
  }

  if (!ftable.update && !ftable.update2)
    goto invalid_frei0r_plugin;

  ftable.get_plugin_info (&info);

  if (info.frei0r_version > 1) {
    GST_WARNING ("Unsupported frei0r version %d", info.frei0r_version);
    ftable.deinit ();
    g_module_close (module);
    return FALSE;
  }

  if (info.color_model > F0R_COLOR_MODEL_PACKED32) {
    GST_WARNING ("Unsupported color model %d", info.color_model);
    ftable.deinit ();
    g_module_close (module);
    return FALSE;
  }

  for (i = 0; i < info.num_params; i++) {
    f0r_param_info_t pinfo = { NULL, };

    ftable.get_param_info (&pinfo, i);
    if (pinfo.type > F0R_PARAM_STRING) {
      GST_WARNING ("Unsupported parameter type %d", pinfo.type);
      ftable.deinit ();
      g_module_close (module);
      return FALSE;
    }
  }

  instance = ftable.construct (640, 480);
  if (!instance) {
    GST_WARNING ("Failed to instantiate plugin '%s'", info.name);
    ftable.deinit ();
    g_module_close (module);
    return FALSE;
  }
  ftable.destruct (instance);

  switch (info.plugin_type) {
    case F0R_PLUGIN_TYPE_FILTER:
      ret = gst_frei0r_filter_register (plugin, vendor, &info, &ftable);
      break;
    case F0R_PLUGIN_TYPE_SOURCE:
      ret = gst_frei0r_src_register (plugin, vendor, &info, &ftable);
      break;
    case F0R_PLUGIN_TYPE_MIXER2:
    case F0R_PLUGIN_TYPE_MIXER3:
      ret = gst_frei0r_mixer_register (plugin, vendor, &info, &ftable);
      break;
    default:
      break;
  }

  switch (ret) {
    case GST_FREI0R_PLUGIN_REGISTER_RETURN_OK:
      return TRUE;
    case GST_FREI0R_PLUGIN_REGISTER_RETURN_FAILED:
      GST_ERROR ("Failed to register frei0r plugin");
      ftable.deinit ();
      g_module_close (module);
      return FALSE;
    case GST_FREI0R_PLUGIN_REGISTER_RETURN_ALREADY_REGISTERED:
      GST_DEBUG ("frei0r plugin already registered");
      ftable.deinit ();
      g_module_close (module);
      return TRUE;
    default:
      g_return_val_if_reached (FALSE);
  }

  g_return_val_if_reached (FALSE);

invalid_frei0r_plugin:
  GST_ERROR ("Invalid frei0r plugin");
  ftable.deinit ();
  g_module_close (module);

  return FALSE;
}

static gboolean
register_plugins (GstPlugin * plugin, GHashTable * plugin_names,
    const gchar * path, const gchar * base_path)
{
  GDir *dir;
  gchar *filename;
  const gchar *entry_name;
  gboolean ret = TRUE;

  GST_DEBUG ("Scanning directory '%s' for frei0r plugins", path);

  dir = g_dir_open (path, 0, NULL);
  if (!dir)
    return FALSE;

  while ((entry_name = g_dir_read_name (dir))) {
    gchar *tmp, *vendor = NULL;
    gchar *hashtable_name;

    tmp = g_strdup (path + strlen (base_path));
    if (*tmp == G_DIR_SEPARATOR && *(tmp + 1))
      vendor = tmp + 1;
    else if (*tmp)
      vendor = tmp;

    if (vendor)
      hashtable_name = g_strconcat (vendor, "-", entry_name, NULL);
    else
      hashtable_name = g_strdup (entry_name);

    if (g_hash_table_lookup_extended (plugin_names, hashtable_name, NULL, NULL)) {
      g_free (hashtable_name);
      continue;
    }

    filename = g_build_filename (path, entry_name, NULL);
    if ((g_str_has_suffix (filename, G_MODULE_SUFFIX)
#ifdef GST_EXTRA_MODULE_SUFFIX
            || g_str_has_suffix (filename, GST_EXTRA_MODULE_SUFFIX)
#endif
        ) && g_file_test (filename, G_FILE_TEST_IS_REGULAR)) {
      gboolean this_ret;

      this_ret = register_plugin (plugin, vendor, filename);
      if (this_ret)
        g_hash_table_insert (plugin_names, g_strdup (hashtable_name), NULL);

      ret = ret && this_ret;
    } else if (g_file_test (filename, G_FILE_TEST_IS_DIR)) {
      ret = ret && register_plugins (plugin, plugin_names, filename, base_path);
    }
    g_free (filename);
    g_free (hashtable_name);
    g_free (tmp);
  }
  g_dir_close (dir);

  return ret;
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  const gchar *homedir;
  gchar *path, *libdir_path;
  GHashTable *plugin_names;
  const gchar *frei0r_path;

  GST_DEBUG_CATEGORY_INIT (frei0r_debug, "frei0r", 0, "frei0r");

  gst_plugin_add_dependency_simple (plugin,
      "FREI0R_PATH:HOME/.frei0r-1/lib",
      LIBDIR "/frei0r-1:"
      "/usr/lib/frei0r-1:/usr/local/lib/frei0r-1:"
      "/usr/lib32/frei0r-1:/usr/local/lib32/frei0r-1:"
      "/usr/lib64/frei0r-1:/usr/local/lib64/frei0r-1",
      NULL, GST_PLUGIN_DEPENDENCY_FLAG_RECURSE);

  plugin_names =
      g_hash_table_new_full ((GHashFunc) g_str_hash, (GEqualFunc) g_str_equal,
      (GDestroyNotify) g_free, NULL);

  frei0r_path = g_getenv ("FREI0R_PATH");
  if (frei0r_path && *frei0r_path) {
    gchar **p, **paths = g_strsplit (frei0r_path, ":", -1);

    for (p = paths; *p; p++) {
      register_plugins (plugin, plugin_names, *p, *p);
    }

    g_strfreev (paths);
  } else {
#define register_plugins2(plugin, pn, p) register_plugins(plugin, pn, p, p)
    homedir = g_get_home_dir ();
    path = g_build_filename (homedir, ".frei0r-1", "lib", NULL);
    libdir_path = g_build_filename (LIBDIR, "frei0r-1", NULL);
    register_plugins2 (plugin, plugin_names, path);
    g_free (path);
    register_plugins2 (plugin, plugin_names, libdir_path);
    g_free (libdir_path);
    register_plugins2 (plugin, plugin_names, "/usr/local/lib/frei0r-1");
    register_plugins2 (plugin, plugin_names, "/usr/lib/frei0r-1");
    register_plugins2 (plugin, plugin_names, "/usr/local/lib32/frei0r-1");
    register_plugins2 (plugin, plugin_names, "/usr/lib32/frei0r-1");
    register_plugins2 (plugin, plugin_names, "/usr/local/lib64/frei0r-1");
    register_plugins2 (plugin, plugin_names, "/usr/lib64/frei0r-1");
#undef register_plugins2
  }

  g_hash_table_unref (plugin_names);

  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    frei0r,
    "frei0r plugin library",
    plugin_init, VERSION, "LGPL", GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
