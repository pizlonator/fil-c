/* GStreamer
 * Copyright (C) 1999,2000 Erik Walthinsen <omega@cse.ogi.edu>
 *               2000 Wim Taymans <wtay@chello.be>
 *               2004 Thomas Vander Stichele <thomas@apestaart.org>
 *               2018 Collabora Ltd.
 *
 * gst-inspect.c: tool to inspect the GStreamer registry
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
#  include "config.h"
#endif

/* FIXME 2.0: suppress warnings for deprecated API such as GValueArray
 * with newer GLib versions (>= 2.31.0) */
#define GLIB_DISABLE_DEPRECATION_WARNINGS

#include "tools.h"
#include <gst/gst_private.h>    /* for internal Factories */

#include <string.h>
#include <locale.h>
#include <glib/gprintf.h>
#ifdef G_OS_UNIX
#   include <unistd.h>
#   include <sys/wait.h>
#endif

#ifdef G_OS_WIN32
/* _isatty() */
#include <io.h>
#endif

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

/* "R" : support color
 * "X" : do not clear the screen when leaving the pager
 * "F" : skip the pager if content fit into the screen
 *
 * Don't forget to update the manpage gst-inspect-1.0.1
 * after changing this default.
 */
#define DEFAULT_LESS_OPTS "RXF"

gboolean colored_output = TRUE;

typedef enum
{
  SORT_TYPE_NONE = 0,
  SORT_TYPE_NAME,               /* default */
} SortType;

SortType sort_output = SORT_TYPE_NAME;

#ifdef G_OS_UNIX
static const gchar DEFAULT_PAGER[] = "less";
GPid child_pid = -1;
#endif
GMainLoop *loop = NULL;

/* Console colors */

/* Escape values for colors */
#define RED       "\033[31m"
#define BLUE      "\033[34m"
#define BRBLUE    "\033[94m"
#define BRCYAN    "\033[96m"
#define BRMAGENTA "\033[95m"
#define BRYELLOW  "\033[33m"
#define CYAN      "\033[36m"
#define GREEN     "\033[32m"
#define MAGENTA   "\033[35m"
#define YELLOW    "\033[33m"

/* General colors */
#define RESET_COLOR           (colored_output? "\033[0m": "")
#define HEADING_COLOR         (colored_output? BRYELLOW : "")
#define PROP_NAME_COLOR       (colored_output? BRBLUE : "")
#define PROP_VALUE_COLOR      (colored_output? RESET_COLOR: "")
#define PROP_ATTR_NAME_COLOR  (colored_output? BRYELLOW : "")
#define PROP_ATTR_VALUE_COLOR (colored_output? CYAN: "")
/* FIXME: find a good color that works on both dark & light bg. */
#define DESC_COLOR            (colored_output? RESET_COLOR: "")

/* Datatype-related colors */
#define DATATYPE_COLOR        (colored_output? GREEN : "")
#define CHILD_LINK_COLOR      (colored_output? BRMAGENTA : "")

/* Caps colors */
#define FIELD_NAME_COLOR      (colored_output? CYAN: "")
#define FIELD_VALUE_COLOR     (colored_output? BRBLUE : "")
#define CAPS_TYPE_COLOR       (colored_output? YELLOW : "")
#define STRUCT_NAME_COLOR     (colored_output? YELLOW : "")
#define CAPS_FEATURE_COLOR    (colored_output? GREEN : "")

/* Plugin listing colors */
#define PLUGIN_NAME_COLOR     (colored_output? BRBLUE : "")
#define ELEMENT_NAME_COLOR    (colored_output? GREEN : "")
/* FIXME: find a good color that works on both dark & light bg. */
#define ELEMENT_DETAIL_COLOR  (colored_output? RESET_COLOR : "")
#define PLUGIN_FEATURE_COLOR  (colored_output? BRBLUE: "")

/* Feature listing colors */
#define FEATURE_NAME_COLOR    (colored_output? GREEN : "")
#define FEATURE_DIR_COLOR     (colored_output? BRMAGENTA : "")
#define FEATURE_RANK_COLOR    (colored_output? CYAN : "")
#define FEATURE_PROTO_COLOR   (colored_output? BRYELLOW : "")

#define GST_DOC_BASE_URL "https://gstreamer.freedesktop.org/documentation"

static const gchar *gstreamer_modules[] = {
  "gstreamer", "gst-plugins-base", "gst-plugins-good", "gst-plugins-ugly",
  "gst-plugins-bad", "gst-editing-services", "gst-libav", "gst-rtsp-server",
  "gstreamer-vaapi", NULL
};

static char *_name = NULL;
static int indent = 0;

static int print_element_info (GstPluginFeature * feature,
    gboolean print_names);
static int print_typefind_info (GstPluginFeature * feature,
    gboolean print_names);
static int print_tracer_info (GstPluginFeature * feature, gboolean print_names);

#define push_indent() push_indent_n(1)
#define pop_indent() push_indent_n(-1)
#define pop_indent_n(n) push_indent_n(-n)

static void
push_indent_n (int n)
{
  g_assert (n > 0 || indent > 0);
  indent += n;
}

/* *INDENT-OFF* */
G_GNUC_PRINTF (1, 2)
/* *INDENT-ON* */

static void
n_print (const char *format, ...)
{
  va_list args;
  int i;
  gchar *str;

  if (_name)
    g_print ("%s", _name);

  for (i = 0; i < indent; ++i)
    g_print ("  ");

  va_start (args, format);
  str = gst_info_strdup_vprintf (format, args);
  va_end (args);

  if (!str)
    return;

  g_print ("%s", str);
  g_free (str);
}

static gboolean
print_field (GQuark field, const GValue * value, gpointer pfx)
{
  gchar *str = gst_value_serialize (value);

  n_print ("%s  %s%15s%s: %s%s%s\n",
      (gchar *) pfx, FIELD_NAME_COLOR, g_quark_to_string (field), RESET_COLOR,
      FIELD_VALUE_COLOR, str, RESET_COLOR);
  g_free (str);
  return TRUE;
}

static void
print_caps (const GstCaps * caps, const gchar * pfx)
{
  guint i;

  g_return_if_fail (caps != NULL);

  if (gst_caps_is_any (caps)) {
    n_print ("%s%sANY%s\n", CAPS_TYPE_COLOR, pfx, RESET_COLOR);
    return;
  }
  if (gst_caps_is_empty (caps)) {
    n_print ("%s%sEMPTY%s\n", CAPS_TYPE_COLOR, pfx, RESET_COLOR);
    return;
  }

  for (i = 0; i < gst_caps_get_size (caps); i++) {
    GstStructure *structure = gst_caps_get_structure (caps, i);
    GstCapsFeatures *features = gst_caps_get_features (caps, i);

    if (features && (gst_caps_features_is_any (features) ||
            !gst_caps_features_is_equal (features,
                GST_CAPS_FEATURES_MEMORY_SYSTEM_MEMORY))) {
      gchar *features_string = gst_caps_features_to_string (features);

      n_print ("%s%s%s%s(%s%s%s)\n", pfx, STRUCT_NAME_COLOR,
          gst_structure_get_name (structure), RESET_COLOR,
          CAPS_FEATURE_COLOR, features_string, RESET_COLOR);
      g_free (features_string);
    } else {
      n_print ("%s%s%s%s\n", pfx, STRUCT_NAME_COLOR,
          gst_structure_get_name (structure), RESET_COLOR);
    }
    gst_structure_foreach (structure, print_field, (gpointer) pfx);
  }
}

static const char *
get_rank_name (char *s, gint rank)
{
  static const int ranks[4] = {
    GST_RANK_NONE, GST_RANK_MARGINAL, GST_RANK_SECONDARY, GST_RANK_PRIMARY
  };
  static const char *rank_names[4] = { "none", "marginal", "secondary",
    "primary"
  };
  int i;
  int best_i;

  best_i = 0;
  for (i = 0; i < 4; i++) {
    if (rank == ranks[i])
      return rank_names[i];
    if (abs (rank - ranks[i]) < abs (rank - ranks[best_i])) {
      best_i = i;
    }
  }

  sprintf (s, "%s %c %d", rank_names[best_i],
      (rank - ranks[best_i] > 0) ? '+' : '-', abs (ranks[best_i] - rank));

  return s;
}

static void
print_factory_details_info (GstElementFactory * factory, GstPlugin * plugin)
{
  gchar **keys, **k;
  gboolean seen_doc_uri = FALSE;
  GstRank rank;
  char s[40];

  rank = gst_plugin_feature_get_rank (GST_PLUGIN_FEATURE (factory));
  n_print ("%sFactory Details:%s\n", HEADING_COLOR, RESET_COLOR);

  push_indent ();
  n_print ("%s%-25s%s%s (%d)%s\n", PROP_NAME_COLOR, "Rank", PROP_VALUE_COLOR,
      get_rank_name (s, rank), rank, RESET_COLOR);

  keys = gst_element_factory_get_metadata_keys (factory);
  if (keys != NULL) {
    for (k = keys; *k != NULL; ++k) {
      const gchar *val;
      gchar *key = *k;

      val = gst_element_factory_get_metadata (factory, key);
      key[0] = g_ascii_toupper (key[0]);
      n_print ("%s%-25s%s%s%s\n", PROP_NAME_COLOR, key, PROP_VALUE_COLOR, val,
          RESET_COLOR);
      seen_doc_uri =
          seen_doc_uri || g_str_equal (key, GST_ELEMENT_METADATA_DOC_URI);
    }
    g_strfreev (keys);
  }

  if (!seen_doc_uri && plugin != NULL &&
      !gst_element_factory_get_skip_documentation (factory)) {
    const gchar *module = gst_plugin_get_source (plugin);
    const gchar *origin = gst_plugin_get_origin (plugin);

    /* gst-plugins-rs has per-plugin module names so need to check origin there */
    if (g_strv_contains (gstreamer_modules, module)
        || (origin != NULL && g_str_has_suffix (origin, "/gst-plugins-rs"))) {
      GList *features;

      features =
          gst_registry_get_feature_list_by_plugin (gst_registry_get (),
          gst_plugin_get_name (plugin));

      /* if the plugin only has a single feature, plugin page == feature page */
      if (features != NULL && features->next == NULL) {
        n_print ("%s%-25s%s%s%s/%s/#%s-page%s\n", PROP_NAME_COLOR,
            "Documentation", RESET_COLOR, PROP_VALUE_COLOR, GST_DOC_BASE_URL,
            gst_plugin_get_name (plugin), GST_OBJECT_NAME (factory),
            RESET_COLOR);
      } else {
        n_print ("%s%-25s%s%s%s/%s/%s.html%s\n", PROP_NAME_COLOR,
            "Documentation", RESET_COLOR, PROP_VALUE_COLOR, GST_DOC_BASE_URL,
            gst_plugin_get_name (plugin), GST_OBJECT_NAME (factory),
            RESET_COLOR);
      }
      gst_plugin_feature_list_free (features);
    }
  }

  pop_indent ();
  n_print ("\n");
}

static void
print_hierarchy (GType type, gint level, gint * maxlevel)
{
  GType parent;
  gint i;

  parent = g_type_parent (type);

  *maxlevel = *maxlevel + 1;
  level++;

  if (parent)
    print_hierarchy (parent, level, maxlevel);

  if (_name)
    g_print ("%s%s%s", DATATYPE_COLOR, _name, RESET_COLOR);

  for (i = 1; i < *maxlevel - level; i++)
    g_print ("      ");
  if (*maxlevel - level)
    g_print (" %s+----%s", CHILD_LINK_COLOR, RESET_COLOR);

  g_print ("%s%s%s\n", DATATYPE_COLOR, g_type_name (type), RESET_COLOR);

  if (level == 1)
    n_print ("\n");
}

static void
print_interfaces (GType type)
{
  guint n_ifaces;
  GType *iface, *ifaces = g_type_interfaces (type, &n_ifaces);

  if (ifaces) {
    if (n_ifaces) {
      n_print (_("%sImplemented Interfaces%s:\n"), HEADING_COLOR, RESET_COLOR);
      push_indent ();
      iface = ifaces;
      while (*iface) {
        n_print ("%s%s%s\n", DATATYPE_COLOR, g_type_name (*iface), RESET_COLOR);
        iface++;
      }
      pop_indent ();
      n_print ("\n");
    }
    g_free (ifaces);
  }
}

static gchar *
flags_to_string (GFlagsValue * vals, guint flags)
{
  GString *s = NULL;
  guint flags_left, i;

  /* first look for an exact match and count the number of values */
  for (i = 0; vals[i].value_name != NULL; ++i) {
    if (vals[i].value == flags)
      return g_strdup (vals[i].value_nick);
  }

  s = g_string_new (NULL);

  /* we assume the values are sorted from lowest to highest value */
  flags_left = flags;
  while (i > 0) {
    --i;
    if (vals[i].value != 0 && (flags_left & vals[i].value) == vals[i].value) {
      if (s->len > 0)
        g_string_append_c (s, '+');
      g_string_append (s, vals[i].value_nick);
      flags_left -= vals[i].value;
      if (flags_left == 0)
        break;
    }
  }

  if (s->len == 0)
    g_string_assign (s, "(none)");

  return g_string_free (s, FALSE);
}

#define KNOWN_PARAM_FLAGS \
  (G_PARAM_CONSTRUCT | G_PARAM_CONSTRUCT_ONLY | \
  G_PARAM_LAX_VALIDATION |  G_PARAM_STATIC_STRINGS | \
  G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_DEPRECATED | \
  GST_PARAM_CONTROLLABLE | GST_PARAM_MUTABLE_PLAYING | \
  GST_PARAM_MUTABLE_PAUSED | GST_PARAM_MUTABLE_READY | \
  GST_PARAM_CONDITIONALLY_AVAILABLE | GST_PARAM_DOC_SHOW_DEFAULT)

static int
sort_gparamspecs (GParamSpec ** a, GParamSpec ** b)
{
  return g_strcmp0 (g_param_spec_get_name (*a), g_param_spec_get_name (*b));
}

/* obj will be NULL if we're printing properties of pad template pads */
static void
print_object_properties_info (GObject * obj, GObjectClass * obj_class,
    const gchar * desc)
{
  GParamSpec **property_specs;
  guint num_properties, i;
  gboolean readable;
  gboolean first_flag;

  property_specs = g_object_class_list_properties (obj_class, &num_properties);
  g_qsort_with_data (property_specs, num_properties, sizeof (gpointer),
      (GCompareDataFunc) sort_gparamspecs, NULL);

  n_print ("%s%s%s:\n", HEADING_COLOR, desc, RESET_COLOR);
  n_print ("\n");

  push_indent ();

  for (i = 0; i < num_properties; i++) {
    GValue value = { 0, };
    GParamSpec *param = property_specs[i];
    GType owner_type = param->owner_type;

    /* We're printing pad properties */
    if (obj == NULL && (owner_type == G_TYPE_OBJECT
            || owner_type == GST_TYPE_OBJECT || owner_type == GST_TYPE_PAD))
      continue;

    g_value_init (&value, param->value_type);

    n_print ("%s%-20s%s: %s%s%s\n", PROP_NAME_COLOR,
        g_param_spec_get_name (param), RESET_COLOR, PROP_VALUE_COLOR,
        g_param_spec_get_blurb (param), RESET_COLOR);

    push_indent_n (11);

    first_flag = TRUE;
    n_print ("%sflags%s: ", PROP_ATTR_NAME_COLOR, RESET_COLOR);
    readable = !!(param->flags & G_PARAM_READABLE);
    if (readable && obj != NULL) {
      g_object_get_property (obj, param->name, &value);
    } else {
      /* if we can't read the property value, assume it's set to the default
       * (which might not be entirely true for sub-classes, but that's an
       * unlikely corner-case anyway) */
      g_param_value_set_default (param, &value);
    }
    if (readable) {
      g_print ("%s%s%s%s", (first_flag) ? "" : ", ", PROP_ATTR_VALUE_COLOR,
          _("readable"), RESET_COLOR);
      first_flag = FALSE;
    }
    if (param->flags & G_PARAM_WRITABLE) {
      g_print ("%s%s%s%s", (first_flag) ? "" : ", ", PROP_ATTR_VALUE_COLOR,
          _("writable"), RESET_COLOR);
      first_flag = FALSE;
    }
    if (param->flags & G_PARAM_DEPRECATED) {
      g_print ("%s%s%s%s", (first_flag) ? "" : ", ", PROP_ATTR_VALUE_COLOR,
          _("deprecated"), RESET_COLOR);
      first_flag = FALSE;
    }
    if (param->flags & GST_PARAM_CONTROLLABLE) {
      g_print (", %s%s%s", PROP_ATTR_VALUE_COLOR, _("controllable"),
          RESET_COLOR);
      first_flag = FALSE;
    }
    if (param->flags & GST_PARAM_CONDITIONALLY_AVAILABLE) {
      g_print (", %s%s%s", PROP_ATTR_VALUE_COLOR, _("conditionally available"),
          RESET_COLOR);
      first_flag = FALSE;
    }
    if (param->flags & G_PARAM_CONSTRUCT_ONLY) {
      g_print (", %s%s%s", PROP_ATTR_VALUE_COLOR,
          _("can be set only at object construction time"), RESET_COLOR);
    } else if (param->flags & GST_PARAM_MUTABLE_PLAYING) {
      g_print (", %s%s%s", PROP_ATTR_VALUE_COLOR,
          _("changeable in NULL, READY, PAUSED or PLAYING state"), RESET_COLOR);
    } else if (param->flags & GST_PARAM_MUTABLE_PAUSED) {
      g_print (", %s%s%s", PROP_ATTR_VALUE_COLOR,
          _("changeable only in NULL, READY or PAUSED state"), RESET_COLOR);
    } else if (param->flags & GST_PARAM_MUTABLE_READY) {
      g_print (", %s%s%s", PROP_ATTR_VALUE_COLOR,
          _("changeable only in NULL or READY state"), RESET_COLOR);
    }
    if (param->flags & ~KNOWN_PARAM_FLAGS) {
      g_print ("%s0x%s%0x%s", (first_flag) ? "" : ", ", PROP_ATTR_VALUE_COLOR,
          param->flags & ~KNOWN_PARAM_FLAGS, RESET_COLOR);
    }
    g_print ("\n");

    switch ((uintptr_t) G_VALUE_TYPE (&value)) {
      case (uintptr_t) G_TYPE_STRING:
      {
        const char *string_val = g_value_get_string (&value);

        n_print ("%sString%s. ", DATATYPE_COLOR, RESET_COLOR);

        if (string_val == NULL)
          g_print ("%sDefault%s: %snull%s", PROP_ATTR_NAME_COLOR, RESET_COLOR,
              PROP_ATTR_VALUE_COLOR, RESET_COLOR);
        else
          g_print ("%sDefault%s: %s\"%s\"%s", PROP_ATTR_NAME_COLOR, RESET_COLOR,
              PROP_ATTR_VALUE_COLOR, string_val, RESET_COLOR);
        break;
      }
      case (uintptr_t) G_TYPE_BOOLEAN:
      {
        gboolean bool_val = g_value_get_boolean (&value);

        n_print ("%sBoolean%s. %sDefault%s: %s%s%s", DATATYPE_COLOR,
            RESET_COLOR, PROP_ATTR_NAME_COLOR, RESET_COLOR,
            PROP_ATTR_VALUE_COLOR, bool_val ? "true" : "false", RESET_COLOR);
        break;
      }
      case (uintptr_t) G_TYPE_ULONG:
      {
        GParamSpecULong *pulong = G_PARAM_SPEC_ULONG (param);

        n_print
            ("%sUnsigned Long%s. %sRange%s: %s%lu - %lu%s %sDefault%s: %s%lu%s ",
            DATATYPE_COLOR, RESET_COLOR, PROP_ATTR_NAME_COLOR, RESET_COLOR,
            PROP_ATTR_VALUE_COLOR, pulong->minimum, pulong->maximum,
            RESET_COLOR, PROP_ATTR_NAME_COLOR, RESET_COLOR,
            PROP_ATTR_VALUE_COLOR, g_value_get_ulong (&value), RESET_COLOR);

        GST_ERROR ("%s: property '%s' of type ulong: consider changing to "
            "uint/uint64", G_OBJECT_CLASS_NAME (obj_class),
            g_param_spec_get_name (param));
        break;
      }
      case (uintptr_t) G_TYPE_LONG:
      {
        GParamSpecLong *plong = G_PARAM_SPEC_LONG (param);

        n_print ("%sLong%s. %sRange%s: %s%ld - %ld%s %sDefault%s: %s%ld%s ",
            DATATYPE_COLOR, RESET_COLOR, PROP_ATTR_NAME_COLOR, RESET_COLOR,
            PROP_ATTR_VALUE_COLOR, plong->minimum, plong->maximum, RESET_COLOR,
            PROP_ATTR_NAME_COLOR, RESET_COLOR, PROP_ATTR_VALUE_COLOR,
            g_value_get_long (&value), RESET_COLOR);

        GST_ERROR ("%s: property '%s' of type long: consider changing to "
            "int/int64", G_OBJECT_CLASS_NAME (obj_class),
            g_param_spec_get_name (param));
        break;
      }
      case (uintptr_t) G_TYPE_UINT:
      {
        GParamSpecUInt *puint = G_PARAM_SPEC_UINT (param);

        n_print
            ("%sUnsigned Integer%s. %sRange%s: %s%u - %u%s %sDefault%s: %s%u%s ",
            DATATYPE_COLOR, RESET_COLOR, PROP_ATTR_NAME_COLOR, RESET_COLOR,
            PROP_ATTR_VALUE_COLOR, puint->minimum, puint->maximum, RESET_COLOR,
            PROP_ATTR_NAME_COLOR, RESET_COLOR, PROP_ATTR_VALUE_COLOR,
            g_value_get_uint (&value), RESET_COLOR);
        break;
      }
      case (uintptr_t) G_TYPE_INT:
      {
        GParamSpecInt *pint = G_PARAM_SPEC_INT (param);

        n_print ("%sInteger%s. %sRange%s: %s%d - %d%s %sDefault%s: %s%d%s ",
            DATATYPE_COLOR, RESET_COLOR, PROP_ATTR_NAME_COLOR, RESET_COLOR,
            PROP_ATTR_VALUE_COLOR, pint->minimum, pint->maximum, RESET_COLOR,
            PROP_ATTR_NAME_COLOR, RESET_COLOR, PROP_ATTR_VALUE_COLOR,
            g_value_get_int (&value), RESET_COLOR);
        break;
      }
      case (uintptr_t) G_TYPE_UINT64:
      {
        GParamSpecUInt64 *puint64 = G_PARAM_SPEC_UINT64 (param);

        n_print ("%sUnsigned Integer64%s. %sRange%s: %s%" G_GUINT64_FORMAT " - "
            "%" G_GUINT64_FORMAT "%s %sDefault%s: %s%" G_GUINT64_FORMAT "%s ",
            DATATYPE_COLOR, RESET_COLOR, PROP_ATTR_NAME_COLOR, RESET_COLOR,
            PROP_ATTR_VALUE_COLOR, puint64->minimum, puint64->maximum,
            RESET_COLOR, PROP_ATTR_NAME_COLOR, RESET_COLOR,
            PROP_ATTR_VALUE_COLOR, g_value_get_uint64 (&value), RESET_COLOR);
        break;
      }
      case (uintptr_t) G_TYPE_INT64:
      {
        GParamSpecInt64 *pint64 = G_PARAM_SPEC_INT64 (param);

        n_print ("%sInteger64%s. %sRange%s: %s%" G_GINT64_FORMAT " - %"
            G_GINT64_FORMAT "%s %sDefault%s: %s%" G_GINT64_FORMAT "%s ",
            DATATYPE_COLOR, RESET_COLOR, PROP_ATTR_NAME_COLOR, RESET_COLOR,
            PROP_ATTR_VALUE_COLOR, pint64->minimum, pint64->maximum,
            RESET_COLOR, PROP_ATTR_NAME_COLOR, RESET_COLOR,
            PROP_ATTR_VALUE_COLOR, g_value_get_int64 (&value), RESET_COLOR);
        break;
      }
      case (uintptr_t) G_TYPE_FLOAT:
      {
        GParamSpecFloat *pfloat = G_PARAM_SPEC_FLOAT (param);

        n_print ("%sFloat%s. %sRange%s: %s%15.7g - %15.7g%s "
            "%sDefault%s: %s%15.7g%s ", DATATYPE_COLOR, RESET_COLOR,
            PROP_ATTR_NAME_COLOR, RESET_COLOR, PROP_ATTR_VALUE_COLOR,
            pfloat->minimum, pfloat->maximum, RESET_COLOR, PROP_ATTR_NAME_COLOR,
            RESET_COLOR, PROP_ATTR_VALUE_COLOR, g_value_get_float (&value),
            RESET_COLOR);
        break;
      }
      case (uintptr_t) G_TYPE_DOUBLE:
      {
        GParamSpecDouble *pdouble = G_PARAM_SPEC_DOUBLE (param);

        n_print ("%sDouble%s. %sRange%s: %s%15.7g - %15.7g%s "
            "%sDefault%s: %s%15.7g%s ", DATATYPE_COLOR, RESET_COLOR,
            PROP_ATTR_NAME_COLOR, RESET_COLOR, PROP_ATTR_VALUE_COLOR,
            pdouble->minimum, pdouble->maximum, RESET_COLOR,
            PROP_ATTR_NAME_COLOR, RESET_COLOR, PROP_ATTR_VALUE_COLOR,
            g_value_get_double (&value), RESET_COLOR);
        break;
      }
      case (uintptr_t) G_TYPE_CHAR:
      case (uintptr_t) G_TYPE_UCHAR:
        GST_ERROR ("%s: property '%s' of type char: consider changing to "
            "int/string", G_OBJECT_CLASS_NAME (obj_class),
            g_param_spec_get_name (param));
        /* fall through */
      default:
        if (param->value_type == GST_TYPE_CAPS) {
          const GstCaps *caps = gst_value_get_caps (&value);

          if (!caps)
            n_print ("%sCaps%s (NULL)", DATATYPE_COLOR, RESET_COLOR);
          else {
            print_caps (caps, "                           ");
          }
        } else if (G_IS_PARAM_SPEC_ENUM (param)) {
          GEnumValue *values;
          guint j = 0;
          gint enum_value;
          const gchar *value_nick = "";

          values = G_ENUM_CLASS (g_type_class_ref (param->value_type))->values;
          enum_value = g_value_get_enum (&value);

          while (values[j].value_name) {
            if (values[j].value == enum_value)
              value_nick = values[j].value_nick;
            j++;
          }

          n_print ("%sEnum \"%s\"%s %sDefault%s: %s%d, \"%s\"%s",
              DATATYPE_COLOR, g_type_name (G_VALUE_TYPE (&value)), RESET_COLOR,
              PROP_ATTR_NAME_COLOR, RESET_COLOR, PROP_ATTR_VALUE_COLOR,
              enum_value, value_nick, RESET_COLOR);

          j = 0;
          while (values[j].value_name) {
            g_print ("\n");
            n_print ("   %s(%d)%s: %s%-16s%s - %s%s%s",
                PROP_ATTR_NAME_COLOR, values[j].value, RESET_COLOR,
                PROP_ATTR_VALUE_COLOR, values[j].value_nick, RESET_COLOR,
                DESC_COLOR, values[j].value_name, RESET_COLOR);
            j++;
          }
          /* g_type_class_unref (ec); */
        } else if (G_IS_PARAM_SPEC_FLAGS (param)) {
          GParamSpecFlags *pflags = G_PARAM_SPEC_FLAGS (param);
          GFlagsValue *vals;
          gchar *cur;

          vals = pflags->flags_class->values;

          cur = flags_to_string (vals, g_value_get_flags (&value));

          n_print ("%sFlags \"%s\"%s %sDefault%s: %s0x%08x, \"%s\"%s",
              DATATYPE_COLOR, g_type_name (G_VALUE_TYPE (&value)), RESET_COLOR,
              PROP_ATTR_NAME_COLOR, RESET_COLOR, PROP_ATTR_VALUE_COLOR,
              g_value_get_flags (&value), cur, RESET_COLOR);

          while (vals[0].value_name) {
            g_print ("\n");
            n_print ("   %s(0x%08x)%s: %s%-16s%s - %s%s%s",
                PROP_ATTR_NAME_COLOR, vals[0].value, RESET_COLOR,
                PROP_ATTR_VALUE_COLOR, vals[0].value_nick, RESET_COLOR,
                DESC_COLOR, vals[0].value_name, RESET_COLOR);
            ++vals;
          }

          g_free (cur);
        } else if (G_IS_PARAM_SPEC_OBJECT (param)) {
          n_print ("%sObject of type%s %s\"%s\"%s", PROP_VALUE_COLOR,
              RESET_COLOR, DATATYPE_COLOR,
              g_type_name (param->value_type), RESET_COLOR);
        } else if (G_IS_PARAM_SPEC_BOXED (param)) {
          n_print ("%sBoxed pointer of type%s %s\"%s\"%s", PROP_VALUE_COLOR,
              RESET_COLOR, DATATYPE_COLOR,
              g_type_name (param->value_type), RESET_COLOR);
          if (param->value_type == GST_TYPE_STRUCTURE) {
            const GstStructure *s = gst_value_get_structure (&value);
            if (s) {
              g_print ("\n");
              gst_structure_foreach (s, print_field,
                  (gpointer) "                           ");
            }
          }
        } else if (G_IS_PARAM_SPEC_POINTER (param)) {
          if (param->value_type != G_TYPE_POINTER) {
            n_print ("%sPointer of type%s %s\"%s\"%s.", PROP_VALUE_COLOR,
                RESET_COLOR, DATATYPE_COLOR, g_type_name (param->value_type),
                RESET_COLOR);
          } else {
            n_print ("%sPointer.%s", PROP_VALUE_COLOR, RESET_COLOR);
          }
        } else if (param->value_type == G_TYPE_VALUE_ARRAY) {
          GParamSpecValueArray *pvarray = G_PARAM_SPEC_VALUE_ARRAY (param);

          if (pvarray->element_spec) {
            n_print ("%sArray of GValues of type%s %s\"%s\"%s",
                PROP_VALUE_COLOR, RESET_COLOR, DATATYPE_COLOR,
                g_type_name (pvarray->element_spec->value_type), RESET_COLOR);
          } else {
            n_print ("%sArray of GValues%s", PROP_VALUE_COLOR, RESET_COLOR);
          }
        } else if (GST_IS_PARAM_SPEC_FRACTION (param)) {
          GstParamSpecFraction *pfraction = GST_PARAM_SPEC_FRACTION (param);

          n_print ("%sFraction%s. %sRange%s: %s%d/%d - %d/%d%s "
              "%sDefault%s: %s%d/%d%s ", DATATYPE_COLOR, RESET_COLOR,
              PROP_ATTR_NAME_COLOR, RESET_COLOR, PROP_ATTR_VALUE_COLOR,
              pfraction->min_num, pfraction->min_den, pfraction->max_num,
              pfraction->max_den, RESET_COLOR, PROP_ATTR_NAME_COLOR,
              RESET_COLOR, PROP_ATTR_VALUE_COLOR,
              gst_value_get_fraction_numerator (&value),
              gst_value_get_fraction_denominator (&value), RESET_COLOR);
        } else if (param->value_type == GST_TYPE_ARRAY) {
          GstParamSpecArray *parray = GST_PARAM_SPEC_ARRAY_LIST (param);

          if (GST_VALUE_HOLDS_ARRAY (&value)) {
            gchar *def = gst_value_serialize (&value);

            n_print ("%sDefault%s: \"%s\"\n", PROP_ATTR_VALUE_COLOR,
                RESET_COLOR, def);

            g_free (def);
          }

          if (parray->element_spec) {
            n_print ("%sGstValueArray of GValues of type%s %s\"%s\"%s",
                PROP_VALUE_COLOR, RESET_COLOR, DATATYPE_COLOR,
                g_type_name (parray->element_spec->value_type), RESET_COLOR);
          } else {
            n_print ("%sGstValueArray of GValues%s", PROP_VALUE_COLOR,
                RESET_COLOR);
          }
        } else {
          n_print ("%sUnknown type %ld%s %s\"%s\"%s", PROP_VALUE_COLOR,
              (glong) param->value_type, RESET_COLOR, DATATYPE_COLOR,
              g_type_name (param->value_type), RESET_COLOR);
        }
        break;
    }
    if (!readable)
      g_print (" %sWrite only%s\n", PROP_VALUE_COLOR, RESET_COLOR);
    else
      g_print ("\n");

    pop_indent_n (11);

    g_value_reset (&value);

    n_print ("\n");
  }
  if (num_properties == 0)
    n_print ("%snone%s\n", PROP_VALUE_COLOR, RESET_COLOR);

  pop_indent ();

  g_free (property_specs);
}

static void
print_element_properties_info (GstElement * element)
{
  g_print ("\n");
  print_object_properties_info (G_OBJECT (element),
      G_OBJECT_GET_CLASS (element), "Element Properties");
}

static gint
gst_static_pad_compare_func (gconstpointer p1, gconstpointer p2)
{
  GstStaticPadTemplate *pt1, *pt2;

  pt1 = (GstStaticPadTemplate *) p1;
  pt2 = (GstStaticPadTemplate *) p2;

  return strcmp (pt1->name_template, pt2->name_template);
}


static void
print_pad_templates_info (GstElement * element, GstElementFactory * factory)
{
  GList *pads, *tmp;
  GstStaticPadTemplate *padtemplate;
  GstPadTemplate *tmpl;

  n_print ("%sPad Templates%s:\n", HEADING_COLOR, RESET_COLOR);

  push_indent ();

  if (gst_element_factory_get_num_pad_templates (factory) == 0) {
    n_print ("%snone%s\n", PROP_VALUE_COLOR, RESET_COLOR);
    goto done;
  }

  pads = g_list_copy ((GList *)
      gst_element_factory_get_static_pad_templates (factory));
  pads = g_list_sort (pads, gst_static_pad_compare_func);

  for (tmp = pads; tmp; tmp = tmp->next) {
    padtemplate = (GstStaticPadTemplate *) (tmp->data);

    if (padtemplate->direction == GST_PAD_SRC)
      n_print ("%sSRC template%s: %s'%s'%s\n", PROP_NAME_COLOR, RESET_COLOR,
          PROP_VALUE_COLOR, padtemplate->name_template, RESET_COLOR);
    else if (padtemplate->direction == GST_PAD_SINK)
      n_print ("%sSINK template%s: %s'%s'%s\n", PROP_NAME_COLOR, RESET_COLOR,
          PROP_VALUE_COLOR, padtemplate->name_template, RESET_COLOR);
    else
      n_print ("%sUNKNOWN template%s: %s'%s'%s\n", PROP_NAME_COLOR, RESET_COLOR,
          PROP_VALUE_COLOR, padtemplate->name_template, RESET_COLOR);

    push_indent ();

    if (padtemplate->presence == GST_PAD_ALWAYS)
      n_print ("%sAvailability%s: %sAlways%s\n", PROP_NAME_COLOR, RESET_COLOR,
          PROP_VALUE_COLOR, RESET_COLOR);
    else if (padtemplate->presence == GST_PAD_SOMETIMES)
      n_print ("%sAvailability%s: %sSometimes%s\n", PROP_NAME_COLOR,
          RESET_COLOR, PROP_VALUE_COLOR, RESET_COLOR);
    else if (padtemplate->presence == GST_PAD_REQUEST) {
      n_print ("%sAvailability%s: %sOn request%s\n", PROP_NAME_COLOR,
          RESET_COLOR, PROP_VALUE_COLOR, RESET_COLOR);
    } else
      n_print ("%sAvailability%s: %sUNKNOWN%s\n", PROP_NAME_COLOR, RESET_COLOR,
          PROP_VALUE_COLOR, RESET_COLOR);

    if (padtemplate->static_caps.string) {
      GstCaps *caps = gst_static_caps_get (&padtemplate->static_caps);

      n_print ("%sCapabilities%s:\n", PROP_NAME_COLOR, RESET_COLOR);

      push_indent ();
      print_caps (caps, "");    // FIXME
      pop_indent ();

      gst_caps_unref (caps);
    }

    tmpl = gst_element_class_get_pad_template (GST_ELEMENT_GET_CLASS (element),
        padtemplate->name_template);
    if (tmpl != NULL) {
      GType pad_type = GST_PAD_TEMPLATE_GTYPE (tmpl);

      if (pad_type != G_TYPE_NONE && pad_type != GST_TYPE_PAD) {
        gpointer pad_klass;

        pad_klass = g_type_class_ref (pad_type);
        n_print ("%sType%s: %s%s%s\n", PROP_NAME_COLOR, RESET_COLOR,
            DATATYPE_COLOR, g_type_name (pad_type), RESET_COLOR);
        print_object_properties_info (NULL, pad_klass, "Pad Properties");
        g_type_class_unref (pad_klass);
      }
    }

    pop_indent ();

    if (tmp->next)
      n_print ("\n");
  }
  g_list_free (pads);
done:
  pop_indent ();
}

static void
print_clocking_info (GstElement * element)
{
  gboolean requires_clock, provides_clock;

  requires_clock =
      GST_OBJECT_FLAG_IS_SET (element, GST_ELEMENT_FLAG_REQUIRE_CLOCK);
  provides_clock =
      GST_OBJECT_FLAG_IS_SET (element, GST_ELEMENT_FLAG_PROVIDE_CLOCK);

  if (!requires_clock && !provides_clock) {
    n_print ("\n");
    n_print ("%sElement has no clocking capabilities.%s\n", DESC_COLOR,
        RESET_COLOR);
    return;
  }

  n_print ("\n");
  n_print ("%sClocking Interaction%s:\n", PROP_NAME_COLOR, RESET_COLOR);

  push_indent ();

  if (requires_clock) {
    n_print ("%selement requires a clock%s\n", PROP_VALUE_COLOR, RESET_COLOR);
  }


  if (provides_clock) {
    n_print ("%selement provides a clock%s\n", PROP_VALUE_COLOR, RESET_COLOR);
  }

  pop_indent ();
}

static void
print_uri_handler_info (GstElement * element)
{
  if (GST_IS_URI_HANDLER (element)) {
    const gchar *const *uri_protocols;
    const gchar *uri_type;

    if (gst_uri_handler_get_uri_type (GST_URI_HANDLER (element)) == GST_URI_SRC)
      uri_type = "source";
    else if (gst_uri_handler_get_uri_type (GST_URI_HANDLER (element)) ==
        GST_URI_SINK)
      uri_type = "sink";
    else
      uri_type = "unknown";

    uri_protocols = gst_uri_handler_get_protocols (GST_URI_HANDLER (element));

    n_print ("\n");
    n_print ("%sURI handling capabilities:%s\n", HEADING_COLOR, RESET_COLOR);

    push_indent ();

    n_print ("%sElement can act as %s.%s\n", DESC_COLOR, uri_type, RESET_COLOR);

    if (uri_protocols && *uri_protocols) {
      n_print ("%sSupported URI protocols%s:\n", DESC_COLOR, RESET_COLOR);
      push_indent ();
      for (; *uri_protocols != NULL; uri_protocols++)
        n_print ("%s%s%s\n", PROP_ATTR_VALUE_COLOR, *uri_protocols,
            RESET_COLOR);
      pop_indent ();
    } else {
      n_print ("%sNo supported URI protocols%s\n", PROP_VALUE_COLOR,
          RESET_COLOR);
    }

    pop_indent ();
  } else {
    n_print ("%sElement has no URI handling capabilities.%s\n", DESC_COLOR,
        RESET_COLOR);
  }
}

static void
print_pad_info (GstElement * element)
{
  const GList *pads;
  GstPad *pad;

  n_print ("\n");
  n_print ("%sPads:%s\n", HEADING_COLOR, RESET_COLOR);

  push_indent ();

  if (!element->numpads) {
    n_print ("%snone%s\n", PROP_VALUE_COLOR, RESET_COLOR);
    goto done;
  }

  pads = element->pads;
  while (pads) {
    gchar *name;

    pad = GST_PAD (pads->data);
    pads = g_list_next (pads);

    name = gst_pad_get_name (pad);
    if (gst_pad_get_direction (pad) == GST_PAD_SRC)
      n_print ("%sSRC%s: %s'%s'%s\n", PROP_NAME_COLOR, RESET_COLOR,
          PROP_VALUE_COLOR, name, RESET_COLOR);
    else if (gst_pad_get_direction (pad) == GST_PAD_SINK)
      n_print ("%sSINK%s: %s'%s'%s\n", PROP_NAME_COLOR, RESET_COLOR,
          PROP_VALUE_COLOR, name, RESET_COLOR);
    else
      n_print ("%sUNKNOWN%s: %s'%s'%s\n", PROP_NAME_COLOR, RESET_COLOR,
          PROP_VALUE_COLOR, name, RESET_COLOR);

    g_free (name);

    if (pad->padtemplate) {
      push_indent ();
      n_print ("%sPad Template%s: %s'%s'%s\n", PROP_NAME_COLOR, RESET_COLOR,
          PROP_VALUE_COLOR, pad->padtemplate->name_template, RESET_COLOR);
      pop_indent ();
    }
  }

done:
  pop_indent ();
}

static gboolean
has_sometimes_template (GstElement * element)
{
  GstElementClass *klass = GST_ELEMENT_GET_CLASS (element);
  GList *l;

  for (l = klass->padtemplates; l != NULL; l = l->next) {
    if (GST_PAD_TEMPLATE (l->data)->presence == GST_PAD_SOMETIMES)
      return TRUE;
  }

  return FALSE;
}

static gboolean
gtype_needs_ptr_marker (GType type)
{
  if (type == G_TYPE_POINTER)
    return FALSE;

  if (G_TYPE_FUNDAMENTAL (type) == G_TYPE_POINTER || G_TYPE_IS_BOXED (type)
      || G_TYPE_IS_OBJECT (type))
    return TRUE;

  return FALSE;
}

static const gchar *
pretty_type_name (GType type, const gchar ** p_pmark)
{
  if (type == G_TYPE_STRING) {
    *p_pmark = " * ";
    return "gchar";
  } else if (type == G_TYPE_STRV) {
    *p_pmark = " ** ";
    return "gchar";
  } else {
    *p_pmark = gtype_needs_ptr_marker (type) ? " * " : " ";
    return g_type_name (type);
  }
}

static void
print_signal_info (GstElement * element)
{
  /* Signals/Actions Block */
  guint *signals;
  guint nsignals;
  gint i = 0, j, k;
  GSignalQuery *query = NULL;
  GType type;
  GSList *found_signals, *l;

  for (k = 0; k < 2; k++) {
    gboolean want_actions = (k == 1);

    found_signals = NULL;

    /* For elements that have sometimes pads, also list a few useful GstElement
     * signals. Put these first, so element-specific ones come later. */
    if (!want_actions && has_sometimes_template (element)) {
      query = g_new0 (GSignalQuery, 1);
      g_signal_query (g_signal_lookup ("pad-added", GST_TYPE_ELEMENT), query);
      found_signals = g_slist_append (found_signals, query);
      query = g_new0 (GSignalQuery, 1);
      g_signal_query (g_signal_lookup ("pad-removed", GST_TYPE_ELEMENT), query);
      found_signals = g_slist_append (found_signals, query);
      query = g_new0 (GSignalQuery, 1);
      g_signal_query (g_signal_lookup ("no-more-pads", GST_TYPE_ELEMENT),
          query);
      found_signals = g_slist_append (found_signals, query);
    }

    for (type = G_OBJECT_TYPE (element); type; type = g_type_parent (type)) {
      if (type == GST_TYPE_ELEMENT || type == GST_TYPE_OBJECT)
        break;

      if (type == GST_TYPE_BIN && G_OBJECT_TYPE (element) != GST_TYPE_BIN)
        continue;

      signals = g_signal_list_ids (type, &nsignals);
      for (i = 0; i < nsignals; i++) {
        query = g_new0 (GSignalQuery, 1);
        g_signal_query (signals[i], query);

        if ((!want_actions && !(query->signal_flags & G_SIGNAL_ACTION)) ||
            (want_actions && (query->signal_flags & G_SIGNAL_ACTION)))
          found_signals = g_slist_append (found_signals, query);
        else
          g_free (query);
      }
      g_free (signals);
      signals = NULL;
    }

    if (found_signals) {
      n_print ("\n");
      if (!want_actions)
        n_print ("%sElement Signals%s:\n", HEADING_COLOR, RESET_COLOR);
      else
        n_print ("%sElement Actions%s:\n", HEADING_COLOR, RESET_COLOR);
      n_print ("\n");
    } else {
      continue;
    }

    for (l = found_signals; l; l = l->next) {
      gchar *indent;
      const gchar *pmark;
      const gchar *retval_type_name;
      int indent_len;

      query = (GSignalQuery *) l->data;
      retval_type_name = pretty_type_name (query->return_type, &pmark);

      indent_len = strlen (query->signal_name) + strlen (retval_type_name);
      indent_len += strlen (pmark) - 1;
      indent_len += (want_actions) ? 36 : 24;

      indent = g_new0 (gchar, indent_len + 1);
      memset (indent, ' ', indent_len);

      if (want_actions) {
        n_print
            ("  %s\"%s\"%s -> %s%s%s %s:  g_signal_emit_by_name%s (%selement%s, %s\"%s\"%s",
            PROP_NAME_COLOR, query->signal_name, RESET_COLOR, DATATYPE_COLOR,
            retval_type_name, PROP_VALUE_COLOR, pmark,
            RESET_COLOR, PROP_VALUE_COLOR, RESET_COLOR, PROP_NAME_COLOR,
            query->signal_name, RESET_COLOR);
      } else {
        n_print ("  %s\"%s\"%s :  %s%s%s%suser_function%s (%s%s%s * object%s",
            PROP_NAME_COLOR, query->signal_name, RESET_COLOR,
            DATATYPE_COLOR, retval_type_name, PROP_VALUE_COLOR,
            pmark, RESET_COLOR, DATATYPE_COLOR, g_type_name (type),
            PROP_VALUE_COLOR, RESET_COLOR);
      }

      for (j = 0; j < query->n_params; j++) {
        const gchar *type_name, *asterisk, *const_prefix;

        type_name = pretty_type_name (query->param_types[j], &asterisk);

        /* Add const prefix for string and string array arguments */
        if (g_str_equal (type_name, "gchar") && strchr (asterisk, '*')) {
          const_prefix = "const ";
        } else {
          const_prefix = "";
        }

        g_print (",\n");
        n_print ("%s%s%s%s%s%sarg%d%s", indent, DATATYPE_COLOR, const_prefix,
            type_name, PROP_VALUE_COLOR, asterisk, j, RESET_COLOR);
      }

      if (!want_actions) {
        g_print (",\n");
        n_print ("%s%sgpointer %suser_data%s);\n", indent, DATATYPE_COLOR,
            PROP_VALUE_COLOR, RESET_COLOR);
      } else if (query->return_type == G_TYPE_NONE) {
        n_print ("%s);\n", RESET_COLOR);
      } else {
        g_print (",\n");
        n_print ("%s%s%s%s *%sp_return_value%s);\n", indent, DATATYPE_COLOR,
            g_type_name (query->return_type), PROP_VALUE_COLOR, pmark,
            RESET_COLOR);
      }
      g_free (indent);
      g_print ("\n");
    }

    if (found_signals) {
      g_slist_foreach (found_signals, (GFunc) g_free, NULL);
      g_slist_free (found_signals);
    }
  }
}

static void
print_children_info (GstElement * element)
{
  GList *children;

  if (!GST_IS_BIN (element))
    return;

  children = (GList *) GST_BIN (element)->children;
  if (children) {
    n_print ("\n");
    n_print ("%sChildren%s:\n", HEADING_COLOR, RESET_COLOR);
  }

  while (children) {
    n_print ("  %s%s%s\n", DATATYPE_COLOR,
        GST_ELEMENT_NAME (GST_ELEMENT (children->data)), RESET_COLOR);
    children = g_list_next (children);
  }
}

static void
print_preset_list (GstElement * element)
{
  gchar **presets, **preset;

  if (!GST_IS_PRESET (element))
    return;

  presets = gst_preset_get_preset_names (GST_PRESET (element));
  if (presets && *presets) {
    n_print ("\n");
    n_print ("%sPresets%s:\n", HEADING_COLOR, RESET_COLOR);
    for (preset = presets; *preset; preset++) {
      gchar *comment = NULL;
      n_print ("  \"%s\"", *preset);

      if (gst_preset_get_meta (GST_PRESET (element), *preset, "comment",
              &comment) && comment)
        g_print (": %s", comment);
      g_free (comment);
      g_print ("\n");
    }
    g_strfreev (presets);
  }
}

static gint
gst_plugin_name_compare_func (gconstpointer p1, gconstpointer p2)
{
  return strcmp (gst_plugin_get_name ((GstPlugin *) p1),
      gst_plugin_get_name ((GstPlugin *) p2));
}

static gint
gst_plugin_feature_name_compare_func (gconstpointer p1, gconstpointer p2)
{
  return strcmp (GST_OBJECT_NAME (p1), GST_OBJECT_NAME (p2));
}

static void
print_blacklist (void)
{
  GList *plugins, *cur;
  gint count = 0;

  g_print ("%s%s%s\n", HEADING_COLOR, _("Blacklisted files:"), RESET_COLOR);

  plugins = gst_registry_get_plugin_list (gst_registry_get ());
  if (sort_output == SORT_TYPE_NAME)
    plugins = g_list_sort (plugins, gst_plugin_name_compare_func);

  for (cur = plugins; cur != NULL; cur = g_list_next (cur)) {
    GstPlugin *plugin = (GstPlugin *) (cur->data);
    if (GST_OBJECT_FLAG_IS_SET (plugin, GST_PLUGIN_FLAG_BLACKLISTED)) {
      g_print ("  %s\n", gst_plugin_get_name (plugin));
      count++;
    }
  }

  g_print ("\n");
  g_print (_("%sTotal count%s: %s"), PROP_NAME_COLOR, RESET_COLOR,
      PROP_VALUE_COLOR);
  g_print (ngettext ("%d blacklisted file", "%d blacklisted files", count),
      count);
  g_print ("%s\n", RESET_COLOR);
  gst_plugin_list_free (plugins);
}

static void
print_typefind_extensions (const gchar * const *extensions, const gchar * color)
{
  guint i = 0;

  while (extensions[i]) {
    g_print ("%s%s%s%s", i > 0 ? ", " : "", color, extensions[i], RESET_COLOR);
    i++;
  }
}

static void
print_element_list (gboolean print_all, gchar * ftypes)
{
  int plugincount = 0, featurecount = 0, blacklistcount = 0;
  GList *plugins, *orig_plugins;
  gchar **types = NULL;

  if (ftypes) {
    gint i;

    types = g_strsplit (ftypes, "/", -1);
    for (i = 0; types[i]; i++)
      *types[i] = g_ascii_toupper (*types[i]);

  }

  orig_plugins = plugins = gst_registry_get_plugin_list (gst_registry_get ());
  if (sort_output == SORT_TYPE_NAME)
    orig_plugins = plugins =
        g_list_sort (plugins, gst_plugin_name_compare_func);
  while (plugins) {
    GList *features, *orig_features;
    GstPlugin *plugin;

    plugin = (GstPlugin *) (plugins->data);
    plugins = g_list_next (plugins);
    plugincount++;

    if (GST_OBJECT_FLAG_IS_SET (plugin, GST_PLUGIN_FLAG_BLACKLISTED)) {
      blacklistcount++;
      continue;
    }

    orig_features = features =
        gst_registry_get_feature_list_by_plugin (gst_registry_get (),
        gst_plugin_get_name (plugin));
    if (sort_output == SORT_TYPE_NAME)
      orig_features = features =
          g_list_sort (features, gst_plugin_feature_name_compare_func);
    while (features) {
      GstPluginFeature *feature;

      if (G_UNLIKELY (features->data == NULL))
        goto next;
      feature = GST_PLUGIN_FEATURE (features->data);
      featurecount++;

      if (GST_IS_ELEMENT_FACTORY (feature)) {
        const gchar *klass;
        GstElementFactory *factory;

        factory = GST_ELEMENT_FACTORY (feature);
        if (types) {
          gint i;
          gboolean all_found = TRUE;

          klass =
              gst_element_factory_get_metadata (factory,
              GST_ELEMENT_METADATA_KLASS);
          for (i = 0; types[i]; i++) {
            if (!strstr (klass, types[i])) {
              all_found = FALSE;
              break;
            }
          }

          if (!all_found)
            goto next;
        }
        if (print_all)
          print_element_info (feature, TRUE);
        else
          g_print ("%s%s%s:  %s%s%s: %s%s%s\n", PLUGIN_NAME_COLOR,
              gst_plugin_get_name (plugin), RESET_COLOR, ELEMENT_NAME_COLOR,
              GST_OBJECT_NAME (factory), RESET_COLOR, ELEMENT_DETAIL_COLOR,
              gst_element_factory_get_metadata (factory,
                  GST_ELEMENT_METADATA_LONGNAME), RESET_COLOR);
      } else if (GST_IS_TYPE_FIND_FACTORY (feature)) {
        GstTypeFindFactory *factory;
        const gchar *const *extensions;

        if (types)
          goto next;
        factory = GST_TYPE_FIND_FACTORY (feature);
        if (!print_all)
          g_print ("%s%s%s: %s%s%s: ", PLUGIN_NAME_COLOR,
              gst_plugin_get_name (plugin), RESET_COLOR, ELEMENT_NAME_COLOR,
              gst_plugin_feature_get_name (feature), RESET_COLOR);

        extensions = gst_type_find_factory_get_extensions (factory);
        if (extensions != NULL) {
          if (!print_all) {
            print_typefind_extensions (extensions, ELEMENT_DETAIL_COLOR);
            g_print ("\n");
          }
        } else {
          if (!print_all)
            g_print ("%sno extensions%s\n", ELEMENT_DETAIL_COLOR, RESET_COLOR);
        }
      } else {
        if (types)
          goto next;
        if (!print_all)
          n_print ("%s%s%s:  %s%s%s (%s%s%s)\n", PLUGIN_NAME_COLOR,
              gst_plugin_get_name (plugin), RESET_COLOR, ELEMENT_NAME_COLOR,
              GST_OBJECT_NAME (feature), RESET_COLOR, ELEMENT_DETAIL_COLOR,
              g_type_name (G_OBJECT_TYPE (feature)), RESET_COLOR);
      }

    next:
      features = g_list_next (features);
    }

    gst_plugin_feature_list_free (orig_features);
  }

  gst_plugin_list_free (orig_plugins);
  g_strfreev (types);

  g_print ("\n");
  g_print (_("%sTotal count%s: %s"), PROP_NAME_COLOR, RESET_COLOR,
      PROP_VALUE_COLOR);
  g_print (ngettext ("%d plugin", "%d plugins", plugincount), plugincount);
  if (blacklistcount) {
    g_print (" (");
    g_print (ngettext ("%d blacklist entry", "%d blacklist entries",
            blacklistcount), blacklistcount);
    g_print (" not shown)");
  }
  g_print ("%s, %s", RESET_COLOR, PROP_VALUE_COLOR);
  g_print (ngettext ("%d feature", "%d features", featurecount), featurecount);
  g_print ("%s\n", RESET_COLOR);
}

static void
print_all_uri_handlers (void)
{
  GList *plugins, *p, *features, *f;

  plugins = gst_registry_get_plugin_list (gst_registry_get ());
  if (sort_output == SORT_TYPE_NAME)
    plugins = g_list_sort (plugins, gst_plugin_name_compare_func);
  for (p = plugins; p; p = p->next) {
    GstPlugin *plugin = (GstPlugin *) (p->data);

    features =
        gst_registry_get_feature_list_by_plugin (gst_registry_get (),
        gst_plugin_get_name (plugin));
    if (sort_output == SORT_TYPE_NAME)
      features = g_list_sort (features, gst_plugin_feature_name_compare_func);
    for (f = features; f; f = f->next) {
      GstPluginFeature *feature = GST_PLUGIN_FEATURE (f->data);

      if (GST_IS_ELEMENT_FACTORY (feature)) {
        GstElementFactory *factory;
        GstElement *element;

        factory = GST_ELEMENT_FACTORY (gst_plugin_feature_load (feature));
        if (!factory) {
          g_print ("element plugin %s couldn't be loaded\n",
              gst_plugin_get_name (plugin));
          continue;
        }

        element = gst_element_factory_create (factory, NULL);
        if (!element) {
          g_print ("couldn't construct element for %s for some reason\n",
              GST_OBJECT_NAME (factory));
          gst_object_unref (factory);
          continue;
        }

        if (GST_IS_URI_HANDLER (element)) {
          const gchar *const *uri_protocols;
          const gchar *const *protocol;
          const gchar *dir;

          switch (gst_uri_handler_get_uri_type (GST_URI_HANDLER (element))) {
            case GST_URI_SRC:
              dir = "read";
              break;
            case GST_URI_SINK:
              dir = "write";
              break;
            default:
              dir = "unknown";
              break;
          }

          g_print ("%s%s%s (%s%s%s, %srank %u%s): ",
              FEATURE_NAME_COLOR,
              gst_plugin_feature_get_name (GST_PLUGIN_FEATURE (factory)),
              RESET_COLOR, FEATURE_DIR_COLOR, dir, RESET_COLOR,
              FEATURE_RANK_COLOR,
              gst_plugin_feature_get_rank (GST_PLUGIN_FEATURE (factory)),
              RESET_COLOR);

          uri_protocols =
              gst_uri_handler_get_protocols (GST_URI_HANDLER (element));
          for (protocol = uri_protocols; *protocol != NULL; protocol++) {
            if (protocol != uri_protocols)
              g_print (", ");
            g_print ("%s%s%s", FEATURE_PROTO_COLOR, *protocol, RESET_COLOR);
          }
          g_print ("\n");
        }

        gst_object_unref (element);
        gst_object_unref (factory);
      }
    }

    gst_plugin_feature_list_free (features);
  }

  gst_plugin_list_free (plugins);
}

static void
print_plugin_info (GstPlugin * plugin)
{
  const gchar *plugin_name = gst_plugin_get_name (plugin);
  const gchar *release_date = gst_plugin_get_release_date_string (plugin);
  const gchar *filename = gst_plugin_get_filename (plugin);
  const gchar *module = gst_plugin_get_source (plugin);
  const gchar *origin = gst_plugin_get_origin (plugin);

  n_print ("%sPlugin Details%s:\n", HEADING_COLOR, RESET_COLOR);

  push_indent ();

  n_print ("%s%-25s%s%s%s%s\n", PROP_NAME_COLOR, "Name", RESET_COLOR,
      PROP_VALUE_COLOR, plugin_name, RESET_COLOR);
  n_print ("%s%-25s%s%s%s%s\n", PROP_NAME_COLOR, "Description", RESET_COLOR,
      PROP_VALUE_COLOR, gst_plugin_get_description (plugin), RESET_COLOR);
  n_print ("%s%-25s%s%s%s%s\n", PROP_NAME_COLOR, "Filename", RESET_COLOR,
      PROP_VALUE_COLOR, (filename != NULL) ? filename : "(null)", RESET_COLOR);
  n_print ("%s%-25s%s%s%s%s\n", PROP_NAME_COLOR, "Version", RESET_COLOR,
      PROP_VALUE_COLOR, gst_plugin_get_version (plugin), RESET_COLOR);
  n_print ("%s%-25s%s%s%s%s\n", PROP_NAME_COLOR, "License", RESET_COLOR,
      PROP_VALUE_COLOR, gst_plugin_get_license (plugin), RESET_COLOR);
  n_print ("%s%-25s%s%s%s%s\n", PROP_NAME_COLOR, "Source module", RESET_COLOR,
      PROP_VALUE_COLOR, module, RESET_COLOR);

  /* gst-plugins-rs has per-plugin module names so need to check origin there */
  if (g_strv_contains (gstreamer_modules, module)
      || (origin != NULL && g_str_has_suffix (origin, "/gst-plugins-rs"))) {
    n_print ("%s%-25s%s%s%s/%s/%s\n", PROP_NAME_COLOR, "Documentation",
        RESET_COLOR, PROP_VALUE_COLOR, GST_DOC_BASE_URL, plugin_name,
        RESET_COLOR);
  }

  if (release_date != NULL) {
    const gchar *tz = "(UTC)";
    gchar *str, *sep;

/* may be: YYYY-MM-DD or YYYY-MM-DDTHH:MMZ */
/* YYYY-MM-DDTHH:MMZ => YYYY-MM-DD HH:MM (UTC) */
    str = g_strdup (release_date);
    sep = strstr (str, "T");
    if (sep != NULL) {
      *sep = ' ';
      sep = strstr (sep + 1, "Z");
      if (sep != NULL)
        *sep = ' ';
    } else {
      tz = "";
    }
    n_print ("%s%-25s%s%s%s%s%s\n", PROP_NAME_COLOR, "Source release date",
        RESET_COLOR, PROP_VALUE_COLOR, str, tz, RESET_COLOR);
    g_free (str);
  }
  n_print ("%s%-25s%s%s%s%s\n", PROP_NAME_COLOR, "Binary package", RESET_COLOR,
      PROP_VALUE_COLOR, gst_plugin_get_package (plugin), RESET_COLOR);
  n_print ("%s%-25s%s%s%s%s\n", PROP_NAME_COLOR, "Origin URL", RESET_COLOR,
      PROP_VALUE_COLOR, gst_plugin_get_origin (plugin), RESET_COLOR);

  pop_indent ();

  n_print ("\n");
}

static void
print_plugin_features (GstPlugin * plugin)
{
  GList *features, *origlist;
  gint num_features = 0;
  gint num_elements = 0;
  gint num_tracers = 0;
  gint num_typefinders = 0;
  gint num_devproviders = 0;
  gint num_other = 0;

  origlist = features =
      gst_registry_get_feature_list_by_plugin (gst_registry_get (),
      gst_plugin_get_name (plugin));
  if (sort_output == SORT_TYPE_NAME)
    features = g_list_sort (features, gst_plugin_feature_name_compare_func);
  while (features) {
    GstPluginFeature *feature;

    feature = GST_PLUGIN_FEATURE (features->data);

    if (GST_IS_ELEMENT_FACTORY (feature)) {
      GstElementFactory *factory;

      factory = GST_ELEMENT_FACTORY (feature);
      n_print ("  %s%s%s: %s%s%s\n", ELEMENT_NAME_COLOR,
          GST_OBJECT_NAME (factory), RESET_COLOR, ELEMENT_DETAIL_COLOR,
          gst_element_factory_get_metadata (factory,
              GST_ELEMENT_METADATA_LONGNAME), RESET_COLOR);
      num_elements++;
    } else if (GST_IS_TYPE_FIND_FACTORY (feature)) {
      GstTypeFindFactory *factory;
      const gchar *const *extensions;

      factory = GST_TYPE_FIND_FACTORY (feature);
      extensions = gst_type_find_factory_get_extensions (factory);
      if (extensions) {
        g_print ("  %s%s%s: ", ELEMENT_NAME_COLOR,
            gst_plugin_feature_get_name (feature), RESET_COLOR);
        print_typefind_extensions (extensions, ELEMENT_DETAIL_COLOR);
        g_print ("\n");
      } else
        g_print ("  %s%s%s: %sno extensions%s\n", ELEMENT_NAME_COLOR,
            gst_plugin_feature_get_name (feature), RESET_COLOR,
            ELEMENT_DETAIL_COLOR, RESET_COLOR);

      num_typefinders++;
    } else if (GST_IS_DEVICE_PROVIDER_FACTORY (feature)) {
      GstDeviceProviderFactory *factory;

      factory = GST_DEVICE_PROVIDER_FACTORY (feature);
      n_print ("  %s%s%s: %s%s%s\n", ELEMENT_NAME_COLOR,
          GST_OBJECT_NAME (factory), RESET_COLOR, ELEMENT_DETAIL_COLOR,
          gst_device_provider_factory_get_metadata (factory,
              GST_ELEMENT_METADATA_LONGNAME), RESET_COLOR);
      num_devproviders++;
    } else if (GST_IS_TRACER_FACTORY (feature)) {
      n_print ("  %s%s%s (%s%s%s)\n", ELEMENT_NAME_COLOR,
          gst_object_get_name (GST_OBJECT (feature)), RESET_COLOR,
          DATATYPE_COLOR, g_type_name (G_OBJECT_TYPE (feature)), RESET_COLOR);
      num_tracers++;
    } else if (feature) {
      n_print ("  %s%s%s (%s%s%s)\n", ELEMENT_NAME_COLOR,
          gst_object_get_name (GST_OBJECT (feature)), RESET_COLOR,
          DATATYPE_COLOR, g_type_name (G_OBJECT_TYPE (feature)), RESET_COLOR);
      num_other++;
    }
    num_features++;
    features = g_list_next (features);
  }

  gst_plugin_feature_list_free (origlist);

  n_print ("\n");
  n_print ("  %s%d features%s:\n", HEADING_COLOR, num_features, RESET_COLOR);
  if (num_elements > 0)
    n_print ("  %s+--%s %s%d elements%s\n", CHILD_LINK_COLOR, RESET_COLOR,
        PLUGIN_FEATURE_COLOR, num_elements, RESET_COLOR);
  if (num_typefinders > 0)
    n_print ("  %s+--%s %s%d typefinders%s\n", CHILD_LINK_COLOR, RESET_COLOR,
        PLUGIN_FEATURE_COLOR, num_typefinders, RESET_COLOR);
  if (num_devproviders > 0)
    n_print ("  %s+--%s %s%d device providers%s\n", CHILD_LINK_COLOR,
        RESET_COLOR, PLUGIN_FEATURE_COLOR, num_devproviders, RESET_COLOR);
  if (num_tracers > 0)
    n_print ("  %s+--%s %s%d tracers%s\n", CHILD_LINK_COLOR, RESET_COLOR,
        PLUGIN_FEATURE_COLOR, num_tracers, RESET_COLOR);
  if (num_other > 0)
    n_print ("  %s+--%s %s%d other objects%s\n", CHILD_LINK_COLOR, RESET_COLOR,
        PLUGIN_FEATURE_COLOR, num_other, RESET_COLOR);

  n_print ("\n");
}

static void
print_plugin_status_message (const gchar * label, const gchar * msg,
    const gchar * color)
{
  if (msg != NULL) {
    /* TODO: do something fancy with multi-line strings to preserve indentation */
    n_print ("%s%s:%s %s\n\n", color, label, RESET_COLOR, msg);
  }
}

static void
print_plugin_status (GstPlugin * plugin)
{
  gchar **msgs, **s;

  push_indent ();

  msgs = gst_plugin_get_status_infos (plugin);
  for (s = msgs; s != NULL && *s != NULL; ++s) {
    const gchar *info_msg = *s;

    print_plugin_status_message ("Info", info_msg, GREEN);
  }
  g_strfreev (msgs);

  msgs = gst_plugin_get_status_warnings (plugin);
  for (s = msgs; s != NULL && *s != NULL; ++s) {
    const gchar *warning_msg = *s;

    print_plugin_status_message ("Warning", warning_msg, YELLOW);
  }
  g_strfreev (msgs);

  msgs = gst_plugin_get_status_errors (plugin);
  for (s = msgs; s != NULL && *s != NULL; ++s) {
    const gchar *err_msg = *s;

    print_plugin_status_message ("Error", err_msg, RED);
  }
  g_strfreev (msgs);

  pop_indent ();
}

static int
print_feature_info (const gchar * feature_name, gboolean print_all)
{
  GstPluginFeature *feature;
  GstRegistry *registry = gst_registry_get ();
  int ret;

  if ((feature = gst_registry_find_feature (registry, feature_name,
              GST_TYPE_ELEMENT_FACTORY))) {
    ret = print_element_info (feature, print_all);
    goto handled;
  }
  if ((feature = gst_registry_find_feature (registry, feature_name,
              GST_TYPE_TYPE_FIND_FACTORY))) {
    ret = print_typefind_info (feature, print_all);
    goto handled;
  }
  if ((feature = gst_registry_find_feature (registry, feature_name,
              GST_TYPE_TRACER_FACTORY))) {
    ret = print_tracer_info (feature, print_all);
    goto handled;
  }

  /* TODO: handle DEVICE_PROVIDER_FACTORY */

  return -1;

handled:
  gst_object_unref (feature);
  return ret;
}

static int
print_element_info (GstPluginFeature * feature, gboolean print_names)
{
  GstElementFactory *factory;
  GstElement *element;
  GstPlugin *plugin;
  gint maxlevel = 0;

  factory = GST_ELEMENT_FACTORY (gst_plugin_feature_load (feature));
  if (!factory) {
    g_print ("%selement plugin couldn't be loaded%s\n", DESC_COLOR,
        RESET_COLOR);
    return -1;
  }

  element = gst_element_factory_create (factory, NULL);
  if (!element) {
    gst_object_unref (factory);
    g_print ("%scouldn't construct element for some reason%s\n", DESC_COLOR,
        RESET_COLOR);
    return -1;
  }

  if (print_names)
    _name =
        g_strdup_printf ("%s%s%s: ", DATATYPE_COLOR, GST_OBJECT_NAME (factory),
        RESET_COLOR);
  else
    _name = NULL;

  plugin = gst_plugin_feature_get_plugin (GST_PLUGIN_FEATURE (factory));

  print_factory_details_info (factory, plugin);

  if (plugin) {
    print_plugin_info (plugin);
    gst_object_unref (plugin);
  }

  print_hierarchy (G_OBJECT_TYPE (element), 0, &maxlevel);
  print_interfaces (G_OBJECT_TYPE (element));

  print_pad_templates_info (element, factory);
  print_clocking_info (element);
  print_uri_handler_info (element);
  print_pad_info (element);
  print_element_properties_info (element);
  print_signal_info (element);
  print_children_info (element);
  print_preset_list (element);

  gst_object_unref (element);
  gst_object_unref (factory);
  g_free (_name);
  return 0;
}

static int
print_typefind_info (GstPluginFeature * feature, gboolean print_names)
{
  GstTypeFindFactory *factory;
  GstPlugin *plugin;
  GstCaps *caps;
  GstRank rank;
  char s[40];
  const gchar *const *extensions;

  factory = GST_TYPE_FIND_FACTORY (gst_plugin_feature_load (feature));
  if (!factory) {
    g_print ("%stypefind plugin couldn't be loaded%s\n", DESC_COLOR,
        RESET_COLOR);
    return -1;
  }

  if (print_names)
    _name =
        g_strdup_printf ("%s%s%s: ", DATATYPE_COLOR, GST_OBJECT_NAME (factory),
        RESET_COLOR);
  else
    _name = NULL;

  n_print ("%sFactory Details%s:\n", HEADING_COLOR, RESET_COLOR);
  rank = gst_plugin_feature_get_rank (feature);
  n_print ("  %s%-25s%s%s (%d)%s\n", PROP_NAME_COLOR, "Rank", PROP_VALUE_COLOR,
      get_rank_name (s, rank), rank, RESET_COLOR);
  n_print ("  %s%-25s%s%s%s\n", PROP_NAME_COLOR, "Name", PROP_VALUE_COLOR,
      GST_OBJECT_NAME (factory), RESET_COLOR);
  caps = gst_type_find_factory_get_caps (factory);
  if (caps) {
    gchar *caps_str = gst_caps_to_string (factory->caps);

    n_print ("  %s%-25s%s%s%s\n", PROP_NAME_COLOR, "Caps", PROP_VALUE_COLOR,
        caps_str, RESET_COLOR);
    g_free (caps_str);
  }
  extensions = gst_type_find_factory_get_extensions (factory);
  if (extensions) {
    n_print ("  %s%-25s%s", PROP_NAME_COLOR, "Extensions", RESET_COLOR);
    print_typefind_extensions (extensions, PROP_VALUE_COLOR);
    n_print ("\n");
  }
  n_print ("\n");

  plugin = gst_plugin_feature_get_plugin (GST_PLUGIN_FEATURE (factory));
  if (plugin) {
    print_plugin_info (plugin);
    gst_object_unref (plugin);
  }

  gst_object_unref (factory);
  g_free (_name);
  return 0;
}

static int
print_tracer_info (GstPluginFeature * feature, gboolean print_names)
{
  GstTracerFactory *factory;
  GstTracer *tracer;
  GstPlugin *plugin;
  gint maxlevel = 0;

  factory = GST_TRACER_FACTORY (gst_plugin_feature_load (feature));
  if (!factory) {
    g_print ("%stracer plugin couldn't be loaded%s\n", DESC_COLOR, RESET_COLOR);
    return -1;
  }

  tracer = (GstTracer *) g_object_new (factory->type, NULL);
  if (!tracer) {
    gst_object_unref (factory);
    g_print ("%scouldn't construct tracer for some reason%s\n", DESC_COLOR,
        RESET_COLOR);
    return -1;
  }

  if (print_names)
    _name =
        g_strdup_printf ("%s%s%s: ", DATATYPE_COLOR, GST_OBJECT_NAME (factory),
        RESET_COLOR);
  else
    _name = NULL;

  n_print ("%sFactory Details%s:\n", HEADING_COLOR, RESET_COLOR);
  n_print ("  %s%-25s%s%s%s\n", PROP_NAME_COLOR, "Name", PROP_VALUE_COLOR,
      GST_OBJECT_NAME (factory), RESET_COLOR);
  n_print ("\n");

  plugin = gst_plugin_feature_get_plugin (GST_PLUGIN_FEATURE (factory));
  if (plugin) {
    print_plugin_info (plugin);
    gst_object_unref (plugin);
  }

  print_hierarchy (G_OBJECT_TYPE (tracer), 0, &maxlevel);
  print_interfaces (G_OBJECT_TYPE (tracer));

  /* TODO: list what hooks it registers
   * - the data is available in gsttracerutils, we need to iterate the
   *   _priv_tracers hashtable for each probe and then check the list of hooks
   *  for each probe whether hook->tracer == tracer :/
   */

  /* TODO: list what records it emits
   * - in class_init tracers can create GstTracerRecord instances
   * - those only get logged right now and there is no association with the
   *   tracer that created them
   * - we'd need to add them to GstTracerFactory
   *   gst_tracer_class_add_record (klass, record);
   *   - needs work in gstregistrychunks to (de)serialize specs
   *   - gst_tracer_register() would need to iterate the list of records and
   *     copy the record->spec into the factory
   */

  gst_object_unref (tracer);
  gst_object_unref (factory);
  g_free (_name);
  return 0;
}

/* NOTE: Not coloring output from automatic install functions, as their output
 * is meant for machines, not humans.
 */
static void
print_plugin_automatic_install_info_codecs (GstElementFactory * factory)
{
  GstPadDirection direction;
  const gchar *type_name;
  const gchar *klass;
  const GList *static_templates, *l;
  GstCaps *caps = NULL;
  guint i, num;

  klass =
      gst_element_factory_get_metadata (factory, GST_ELEMENT_METADATA_KLASS);
  g_return_if_fail (klass != NULL);

  if (strstr (klass, "Demuxer") ||
      strstr (klass, "Decoder") ||
      strstr (klass, "Decryptor") ||
      strstr (klass, "Depay") || strstr (klass, "Parser")) {
    type_name = "decoder";
    direction = GST_PAD_SINK;
  } else if (strstr (klass, "Muxer") ||
      strstr (klass, "Encoder") ||
      strstr (klass, "Encryptor") || strstr (klass, "Pay")) {
    type_name = "encoder";
    direction = GST_PAD_SRC;
  } else {
    return;
  }

  /* decoder/demuxer sink pads should always be static and there should only
   * be one, the same applies to encoders/muxers and source pads */
  static_templates = gst_element_factory_get_static_pad_templates (factory);
  for (l = static_templates; l != NULL; l = l->next) {
    GstStaticPadTemplate *tmpl = NULL;

    tmpl = (GstStaticPadTemplate *) l->data;
    if (tmpl->direction == direction) {
      caps = gst_static_pad_template_get_caps (tmpl);
      break;
    }
  }

  if (caps == NULL) {
    g_printerr ("Couldn't find static pad template for %s '%s'\n",
        type_name, GST_OBJECT_NAME (factory));
    return;
  }

  caps = gst_caps_make_writable (caps);
  num = gst_caps_get_size (caps);
  for (i = 0; i < num; ++i) {
    GstStructure *s;
    gchar *s_str;

    s = gst_caps_get_structure (caps, i);
    /* remove fields that are almost always just MIN-MAX of some sort
     * in order to make the caps look less messy */
    gst_structure_remove_field (s, "pixel-aspect-ratio");
    gst_structure_remove_field (s, "framerate");
    gst_structure_remove_field (s, "channels");
    gst_structure_remove_field (s, "width");
    gst_structure_remove_field (s, "height");
    gst_structure_remove_field (s, "rate");
    gst_structure_remove_field (s, "depth");
    gst_structure_remove_field (s, "clock-rate");
    s_str = gst_structure_to_string (s);
    g_print ("%s-%s\n", type_name, s_str);
    g_free (s_str);
  }
  gst_caps_unref (caps);
}

static void
print_plugin_automatic_install_info_protocols (GstElementFactory * factory)
{
  const gchar *const *protocols;

  protocols = gst_element_factory_get_uri_protocols (factory);
  if (protocols != NULL && *protocols != NULL) {
    switch (gst_element_factory_get_uri_type (factory)) {
      case GST_URI_SINK:
        while (*protocols != NULL) {
          g_print ("urisink-%s\n", *protocols);
          ++protocols;
        }
        break;
      case GST_URI_SRC:
        while (*protocols != NULL) {
          g_print ("urisource-%s\n", *protocols);
          ++protocols;
        }
        break;
      default:
        break;
    }
  }
}

static void
print_plugin_automatic_install_info (GstPlugin * plugin)
{
  GList *features, *l;

  /* not interested in typefind factories, only element factories */
  features = gst_registry_get_feature_list (gst_registry_get (),
      GST_TYPE_ELEMENT_FACTORY);

  for (l = features; l != NULL; l = l->next) {
    GstPluginFeature *feature;
    GstPlugin *feature_plugin;

    feature = GST_PLUGIN_FEATURE (l->data);

    /* only interested in the ones that are in the plugin we just loaded */
    feature_plugin = gst_plugin_feature_get_plugin (feature);
    if (feature_plugin == plugin) {
      GstElementFactory *factory;

      g_print ("element-%s\n", gst_plugin_feature_get_name (feature));

      factory = GST_ELEMENT_FACTORY (feature);
      print_plugin_automatic_install_info_protocols (factory);
      print_plugin_automatic_install_info_codecs (factory);
    }
    if (feature_plugin)
      gst_object_unref (feature_plugin);
  }

  g_list_foreach (features, (GFunc) gst_object_unref, NULL);
  g_list_free (features);
}

static void
print_all_plugin_automatic_install_info (void)
{
  GList *plugins, *orig_plugins;

  orig_plugins = plugins = gst_registry_get_plugin_list (gst_registry_get ());
  if (sort_output == SORT_TYPE_NAME)
    plugins = g_list_sort (plugins, gst_plugin_name_compare_func);
  while (plugins) {
    GstPlugin *plugin;

    plugin = (GstPlugin *) (plugins->data);
    plugins = g_list_next (plugins);

    print_plugin_automatic_install_info (plugin);
  }
  gst_plugin_list_free (orig_plugins);
}

#ifdef G_OS_UNIX
static gboolean
redirect_stdout (void)
{
  GError *error = NULL;
  gchar **argv;
  const gchar *pager, *less;
  gint stdin_fd;
  gchar **envp;

  pager = g_getenv ("PAGER");
  if (pager == NULL)
    pager = DEFAULT_PAGER;

  argv = g_strsplit (pager, " ", 0);

  less = g_getenv ("GST_LESS");
  if (less == NULL)
    less = DEFAULT_LESS_OPTS;

  envp = g_get_environ ();
  envp = g_environ_setenv (envp, "LESS", less, TRUE);

  if (!g_spawn_async_with_pipes (NULL, argv, envp,
          G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_SEARCH_PATH,
          NULL, NULL, &child_pid, &stdin_fd,
          /* pass null stdout/stderr to inherit our fds */
          NULL, NULL, &error)) {
    if (pager != DEFAULT_PAGER) {
      g_warning ("g_spawn_async_with_pipes() failed: %s\n",
          GST_STR_NULL (error->message));
    }
    g_strfreev (argv);
    g_strfreev (envp);
    g_clear_error (&error);

    return FALSE;
  }

  /* redirect our stdout to child stdin */
  dup2 (stdin_fd, STDOUT_FILENO);
  if (isatty (STDERR_FILENO))
    dup2 (stdin_fd, STDERR_FILENO);
  close (stdin_fd);

  g_strfreev (argv);
  g_strfreev (envp);

  return TRUE;
}

static void
child_exit_cb (GPid child_pid, gint status, gpointer user_data)
{
  g_spawn_close_pid (child_pid);
  g_main_loop_quit (loop);
}
#endif

static gboolean
_parse_sort_type (const gchar * option_name, const gchar * optarg,
    gpointer data, GError ** error)
{
  if (!g_strcmp0 (optarg, "name")) {
    sort_output = SORT_TYPE_NAME;
    return TRUE;
  } else if (!g_strcmp0 (optarg, "none")) {
    sort_output = SORT_TYPE_NONE;
    return TRUE;
  }

  return FALSE;
}

static int
real_main (int argc, char *argv[])
{
  gboolean print_all = FALSE;
  gboolean do_print_blacklist = FALSE;
  gboolean plugin_name = FALSE;
  gboolean print_aii = FALSE;
  gboolean uri_handlers = FALSE;
  gboolean check_exists = FALSE;
  gboolean check_version = FALSE;
  gboolean color_always = FALSE;
  gchar *min_version = NULL;
  guint minver_maj = GST_VERSION_MAJOR;
  guint minver_min = GST_VERSION_MINOR;
  guint minver_micro = 0;
  gchar *types = NULL;
  const gchar *no_colors;
  int exit_code = 0;
#ifndef GST_DISABLE_OPTION_PARSING
  GOptionEntry options[] = {
    {"print-all", 'a', 0, G_OPTION_ARG_NONE, &print_all,
        N_("Print all elements"), NULL},
    {"print-blacklist", 'b', 0, G_OPTION_ARG_NONE, &do_print_blacklist,
        N_("Print list of blacklisted files"), NULL},
    {"print-plugin-auto-install-info", '\0', 0, G_OPTION_ARG_NONE, &print_aii,
        N_("Print a machine-parsable list of features the specified plugin "
              "or all plugins provide.\n                                       "
              "Useful in connection with external automatic plugin "
              "installation mechanisms"), NULL},
    {"plugin", '\0', 0, G_OPTION_ARG_NONE, &plugin_name,
        N_("List the plugin contents"), NULL},
    {"types", 't', 0, G_OPTION_ARG_STRING, &types,
        N_("A slashes ('/') separated list of types of elements (also known "
              "as klass) to list. (unordered)"), NULL},
    {"exists", '\0', 0, G_OPTION_ARG_NONE, &check_exists,
        N_("Check if the specified element or plugin exists"), NULL},
    {"atleast-version", '\0', 0, G_OPTION_ARG_STRING, &min_version,
        N_
          ("When checking if an element or plugin exists, also check that its "
              "version is at least the version specified"), NULL},
    {"uri-handlers", 'u', 0, G_OPTION_ARG_NONE, &uri_handlers,
          N_
          ("Print supported URI schemes, with the elements that implement them"),
        NULL},
    {"no-colors", '\0', G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE,
          &colored_output,
          N_
          ("Disable colors in output. You can also achieve the same by setting "
              "'GST_INSPECT_NO_COLORS' environment variable to any value."),
        NULL},
    {"sort", '\0', G_OPTION_ARG_NONE, G_OPTION_ARG_CALLBACK, &_parse_sort_type,
          "Sort plugins and features. Sorting keys: name (default), none.",
        "<sort-key>"}
    ,
    {"color", 'C', 0, G_OPTION_ARG_NONE, &color_always,
          N_("Color output, even when not sending to a tty."),
        NULL},
    GST_TOOLS_GOPTION_VERSION,
    {NULL}
  };
  GOptionContext *ctx;
  GError *err = NULL;
#endif

  setlocale (LC_ALL, "");

#ifdef ENABLE_NLS
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);
#endif

  /* avoid glib warnings when inspecting deprecated properties */
  g_setenv ("G_ENABLE_DIAGNOSTIC", "0", FALSE);

  g_set_prgname ("gst-inspect-" GST_API_VERSION);

#ifndef GST_DISABLE_OPTION_PARSING
  ctx = g_option_context_new ("[ELEMENT-NAME | PLUGIN-NAME]");
  g_option_context_add_main_entries (ctx, options, GETTEXT_PACKAGE);
  g_option_context_add_group (ctx, gst_init_get_option_group ());
#if defined(G_OS_WIN32) && !defined(GST_CHECK_MAIN)
  if (!g_option_context_parse_strv (ctx, &argv, &err))
#else
  if (!g_option_context_parse (ctx, &argc, &argv, &err))
#endif
  {
    g_printerr ("Error initializing: %s\n", err->message);
    g_clear_error (&err);
    g_option_context_free (ctx);
    return -1;
  }
  g_option_context_free (ctx);
#else
  gst_init (&argc, &argv);
#endif

#if defined(G_OS_WIN32) && !defined(GST_CHECK_MAIN)
  argc = g_strv_length (argv);
#endif

  gst_tools_print_version ();

  if (print_all && argc > 1) {
    g_printerr ("-a requires no extra arguments\n");
    return -1;
  }

  if (uri_handlers && argc > 1) {
    g_printerr ("-u requires no extra arguments\n");
    return -1;
  }

  /* --atleast-version implies --exists */
  if (min_version != NULL) {
    if (sscanf (min_version, "%u.%u.%u", &minver_maj, &minver_min,
            &minver_micro) < 2) {
      g_printerr ("Can't parse version '%s' passed to --atleast-version\n",
          min_version);
      g_free (min_version);
      return -1;
    }
    g_free (min_version);
    check_exists = TRUE;
    check_version = TRUE;
  }

  if (check_exists) {
    if (argc == 1) {
      g_printerr ("--exists requires an extra command line argument\n");
      exit_code = -1;
    } else {
      if (!plugin_name) {
        GstPluginFeature *feature;

        feature = gst_registry_lookup_feature (gst_registry_get (), argv[1]);
        if (feature != NULL) {
          if (check_version && !gst_plugin_feature_check_version (feature,
                  minver_maj, minver_min, minver_micro)) {
            exit_code = 2;
          } else {
            exit_code = 0;
          }
        } else {
          exit_code = 1;
        }

        if (feature)
          gst_object_unref (feature);
      } else {
        /* FIXME: support checking for plugins too */
        g_printerr ("Checking for plugins is not supported yet\n");
        exit_code = -1;
      }
    }
    return exit_code;
  }

  no_colors = g_getenv ("GST_INSPECT_NO_COLORS");
  /* We only support truecolor */
  colored_output &= (no_colors == NULL);

#ifdef G_OS_UNIX
  if (isatty (STDOUT_FILENO)) {
    if (redirect_stdout ())
      loop = g_main_loop_new (NULL, FALSE);
  } else {
    colored_output = (color_always) ? TRUE : FALSE;
  }
#elif defined(G_OS_WIN32)
  {
    /* g_log_writer_supports_color is available since 2.50.0 */
    gint fd = _fileno (stdout);
    /* On Windows 10, g_log_writer_supports_color will also setup the console
     * so that it correctly interprets ANSI VT sequences if it's supported */
    if (!_isatty (fd) || !g_log_writer_supports_color (fd))
      colored_output = FALSE;
  }
#endif

  /* if no arguments, print out list of elements */
  if (uri_handlers) {
    print_all_uri_handlers ();
  } else if (argc == 1 || print_all) {
    if (do_print_blacklist)
      print_blacklist ();
    else {
      if (print_aii)
        print_all_plugin_automatic_install_info ();
      else
        print_element_list (print_all, types);
    }
  } else {
    /* else we try to get a factory */
    const char *arg = argv[argc - 1];
    int retval = -1;

    if (!plugin_name) {
      retval = print_feature_info (arg, print_all);
    }

    /* otherwise check if it's a plugin */
    if (retval) {
      GstPlugin *plugin = gst_registry_find_plugin (gst_registry_get (), arg);

      /* if there is such a plugin, print out info */
      if (plugin) {
        if (print_aii) {
          print_plugin_automatic_install_info (plugin);
        } else {
          print_plugin_info (plugin);
          print_plugin_status (plugin);
          print_plugin_features (plugin);
        }
      } else {
        GError *error = NULL;

        if (g_file_test (arg, G_FILE_TEST_EXISTS)) {
          plugin = gst_plugin_load_file (arg, &error);

          if (plugin) {
            if (print_aii) {
              print_plugin_automatic_install_info (plugin);
            } else {
              print_plugin_info (plugin);
              print_plugin_status (plugin);
              print_plugin_features (plugin);
            }
          } else {
            g_printerr (_("Could not load plugin file: %s\n"), error->message);
            g_clear_error (&error);
            exit_code = -1;
            goto done;
          }
        } else {
          g_printerr (_("No such element or plugin '%s'\n"), arg);
          exit_code = -1;
          goto done;
        }
      }
    }
  }

done:

#ifdef G_OS_UNIX
  if (loop) {
    fflush (stdout);
    fflush (stderr);
    /* So that the pipe we create in redirect_stdout() is closed */
    close (STDOUT_FILENO);
    close (STDERR_FILENO);
    g_child_watch_add (child_pid, child_exit_cb, NULL);
    g_main_loop_run (loop);
    g_main_loop_unref (loop);
  }
#endif

  return exit_code;
}

int
main (int argc, char *argv[])
{
  int ret;

  /* gstinspect.c calls this function */
#if defined(G_OS_WIN32) && !defined(GST_CHECK_MAIN)
  argv = g_win32_get_command_line ();
#endif

#if defined(__APPLE__) && TARGET_OS_MAC && !TARGET_OS_IPHONE
  ret = gst_macos_main ((GstMainFunc) real_main, argc, argv, NULL);
#else
  ret = real_main (argc, argv);
#endif

#if defined(G_OS_WIN32) && !defined(GST_CHECK_MAIN)
  g_strfreev (argv);
#endif

  return ret;
}
