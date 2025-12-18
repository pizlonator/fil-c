#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <glib/gprintf.h>
#include <gst/gst.h>
#include <gio/gio.h>
#include <glib/gi18n.h>

static GRegex *cleanup_caps_field = NULL;
static void _add_object_details (GString * json, GString * other_types,
    GHashTable * seen_other_types, GObject * object, GType gtype,
    GType inst_type);

static gchar *
json_strescape (const gchar * str)
{
  const gchar *p;
  const gchar *end;
  GString *output;
  gsize len;

  if (!str)
    return g_strdup ("NULL");

  len = strlen (str);
  end = str + len;
  output = g_string_sized_new (len);

  for (p = str; p < end; p++) {
    if (*p == '\\' || *p == '"') {
      g_string_append_c (output, '\\');
      g_string_append_c (output, *p);
    } else if (*p == '%') {
      g_string_append_c (output, '%');
      g_string_append_c (output, *p);
    } else if ((*p > 0 && *p < 0x1f) || *p == 0x7f) {
      switch (*p) {
        case '\b':
          g_string_append (output, "\\b");
          break;
        case '\f':
          g_string_append (output, "\\f");
          break;
        case '\n':
          g_string_append (output, "\\n");
          break;
        case '\r':
          g_string_append (output, "\\r");
          break;
        case '\t':
          g_string_append (output, "\\t");
          break;
        default:
          g_string_append_printf (output, "\\u00%02x", (guint) * p);
          break;
      }
    } else {
      g_string_append_c (output, *p);
    }
  }

  return g_string_free (output, FALSE);
}

static gchar *
flags_to_string (GFlagsValue * values, guint flags)
{
  GString *s = NULL;
  guint flags_left, i;

  /* first look for an exact match and count the number of values */
  for (i = 0; values[i].value_name != NULL; ++i) {
    if (values[i].value == flags)
      return g_strdup (values[i].value_nick);
  }

  s = g_string_new (NULL);

  /* we assume the values are sorted from lowest to highest value */
  flags_left = flags;
  while (i > 0) {
    --i;
    if (values[i].value != 0
        && (flags_left & values[i].value) == values[i].value) {
      if (s->len > 0)
        g_string_append_c (s, '+');
      g_string_append (s, values[i].value_nick);
      flags_left -= values[i].value;
      if (flags_left == 0)
        break;
    }
  }

  if (s->len == 0)
    g_string_assign (s, "(none)");

  return g_string_free (s, FALSE);
}

static void
_serialize_flags_default (GString * json, GType gtype, GValue * value)
{
  GFlagsValue *values = G_FLAGS_CLASS (g_type_class_ref (gtype))->values;
  gchar *cur;

  cur = flags_to_string (values, g_value_get_flags (value));
  g_string_append_printf (json, ",\"default\": \"%s\"", cur);
  g_free (cur);
}

static void
_serialize_flags (GString * json, GType gtype)
{
  GFlagsValue *values = G_FLAGS_CLASS (g_type_class_ref (gtype))->values;

  g_string_append_printf (json, "%s\"%s\": { "
      "\"kind\": \"flags\"," "\"values\": [", json->len ? "," : "",
      g_type_name (gtype));

  while (values[0].value_name) {
    gchar *value_name = json_strescape (values[0].value_name);
    gchar *value_nick = json_strescape (values[0].value_nick);

    g_string_append_printf (json, "{\"name\": \"%s\","
        "\"value\": \"0x%08x\","
        "\"desc\": \"%s\"}", value_nick, values[0].value, value_name);
    ++values;

    if (values[0].value_name)
      g_string_append_c (json, ',');

    g_free (value_name);
    g_free (value_nick);
  }

  g_string_append (json, "]}");
}

static void
_serialize_enum_default (GString * json, GType gtype, GValue * value)
{
  GEnumValue *values;
  guint j = 0;
  gint enum_value;
  gchar *value_nick = g_strdup ("");

  values = G_ENUM_CLASS (g_type_class_ref (gtype))->values;

  enum_value = g_value_get_enum (value);
  while (values[j].value_name) {
    if (values[j].value == enum_value) {
      g_free (value_nick);
      value_nick = json_strescape (values[j].value_nick);
      break;
    }

    j++;
  }
  g_string_append_printf (json, ",\"default\": \"%s (%d)\"", value_nick,
      enum_value);;
  g_free (value_nick);
}

static void
_serialize_enum (GString * json, GType gtype, GstPluginAPIFlags api_flags)
{
  GEnumValue *values;
  guint j = 0;

  values = G_ENUM_CLASS (g_type_class_ref (gtype))->values;

  g_string_append_printf (json, "%s\"%s\": { "
      "\"kind\": \"enum\"", json->len ? "," : "", g_type_name (gtype));

  if (api_flags & GST_PLUGIN_API_FLAG_IGNORE_ENUM_MEMBERS) {
    g_string_append (json, ",\"ignore-enum-members\": true, \"values\": []}");
  } else {
    g_string_append (json, ",\"values\": [");

    while (values[j].value_name) {
      gchar *value_name = json_strescape (values[j].value_name);
      gchar *value_nick = json_strescape (values[j].value_nick);

      g_string_append_printf (json, "{\"name\": \"%s\","
          "\"value\": \"%d\","
          "\"desc\": \"%s\"}", value_nick, values[j].value, value_name);
      j++;
      if (values[j].value_name)
        g_string_append_c (json, ',');

      g_free (value_name);
      g_free (value_nick);
    }

    g_string_append (json, "]}");
  }
}

/* @inst_type is used when serializing base classes in the hierarchy:
 * we don't instantiate the base class, which may very well be abstract,
 * but instantiate the final type (@inst_type), and use @type to determine
 * what properties / signals / etc.. we are actually interested in.
 */
static void
_serialize_object (GString * json, GHashTable * seen_other_types, GType gtype,
    GType inst_type)
{
  GObject *tmpobj;
  GString *other_types = NULL;

  g_string_append_printf (json, "%s\"%s\": { "
      "\"kind\": \"%s\"", json->len ? "," : "", g_type_name (gtype),
      G_TYPE_IS_INTERFACE (gtype) ? "interface" : "object");

  other_types = g_string_new ("");
  g_string_append_c (json, ',');
  tmpobj = g_object_new (inst_type, NULL);
  _add_object_details (json, other_types, seen_other_types, tmpobj, gtype,
      inst_type);
  gst_object_unref (tmpobj);

  g_string_append_c (json, '}');

  if (other_types && other_types->len) {
    g_string_append_printf (json, ",%s", other_types->str);
  }
  g_string_free (other_types, TRUE);
}

static void
_add_signals (GString * json, GString * other_types,
    GHashTable * seen_other_types, GObject * object, GType type)
{
  gboolean opened = FALSE;
  guint *signals = NULL;
  guint nsignals;
  gint i = 0, j;
  GstPluginAPIFlags api_flags;

  signals = g_signal_list_ids (type, &nsignals);
  for (i = 0; i < nsignals; i++) {
    GSignalQuery query = { 0, };

    g_signal_query (signals[i], &query);
    g_string_append_printf (json,
        "%s\"%s\" : {", opened ? "," : ",\"signals\": {", query.signal_name);

    opened = TRUE;

    g_string_append (json, "\"args\": [");
    for (j = 0; j < query.n_params; j++) {
      gchar *arg_name = g_strdup_printf ("arg%u", j);
      if (j) {
        g_string_append_c (json, ',');
      }

      g_string_append_printf (json, "{ \"name\": \"%s\","
          "\"type\": \"%s\" }", arg_name, g_type_name (query.param_types[j]));

      if (!g_hash_table_contains (seen_other_types,
              g_type_name (query.param_types[j])) &&
          gst_type_is_plugin_api (query.param_types[j], &api_flags)) {
        g_hash_table_insert (seen_other_types,
            (gpointer) g_type_name (query.param_types[j]), NULL);

        if (g_type_is_a (query.param_types[j], G_TYPE_ENUM)) {
          _serialize_enum (other_types, query.param_types[j], api_flags);
        } else if (g_type_is_a (query.param_types[j], G_TYPE_FLAGS)) {
          _serialize_flags (other_types, query.param_types[j]);
        } else if (g_type_is_a (query.param_types[j], G_TYPE_OBJECT)) {
          _serialize_object (other_types, seen_other_types,
              query.param_types[j], query.param_types[j]);
        }
      }
    }
    g_string_append_c (json, ']');

    if (g_type_name (query.return_type) &&
        !g_hash_table_contains (seen_other_types,
            g_type_name (query.return_type)) &&
        gst_type_is_plugin_api (query.return_type, &api_flags)) {
      g_hash_table_insert (seen_other_types,
          (gpointer) g_type_name (query.return_type), NULL);
      if (g_type_is_a (query.return_type, G_TYPE_ENUM)) {
        _serialize_enum (other_types, query.return_type, api_flags);
      } else if (g_type_is_a (query.return_type, G_TYPE_FLAGS)) {
        _serialize_flags (other_types, query.return_type);
      } else if (g_type_is_a (query.return_type, G_TYPE_OBJECT)) {
        _serialize_object (other_types, seen_other_types, query.return_type,
            query.return_type);
      }
    }

    g_string_append_printf (json,
        ",\"return-type\": \"%s\"", g_type_name (query.return_type));

    if (query.signal_flags & G_SIGNAL_RUN_FIRST)
      g_string_append (json, ",\"when\": \"first\"");
    else if (query.signal_flags & G_SIGNAL_RUN_LAST)
      g_string_append (json, ",\"when\": \"last\"");
    else if (query.signal_flags & G_SIGNAL_RUN_CLEANUP)
      g_string_append (json, ",\"when\": \"cleanup\"");

    if (query.signal_flags & G_SIGNAL_NO_RECURSE)
      g_string_append (json, ",\"no-recurse\": true");

    if (query.signal_flags & G_SIGNAL_DETAILED)
      g_string_append (json, ",\"detailed\": true");

    if (query.signal_flags & G_SIGNAL_ACTION)
      g_string_append (json, ",\"action\": true");

    if (query.signal_flags & G_SIGNAL_NO_HOOKS)
      g_string_append (json, ",\"no-hooks\": true");

    g_string_append_c (json, '}');

    opened = TRUE;
  }
  g_free (signals);

  if (opened)
    g_string_append (json, "}");
}

static void
_add_properties (GString * json, GString * other_types,
    GHashTable * seen_other_types, GObject * object, GObjectClass * klass,
    GType type)
{
  gchar *tmpstr;
  guint i, n_props;
  gboolean opened = FALSE;
  GParamSpec **specs, *spec;
  GstPluginAPIFlags api_flags;

  specs = g_object_class_list_properties (klass, &n_props);

  for (i = 0; i < n_props; i++) {
    GValue value = { 0, };
    const gchar *mutable_str = NULL;
    spec = specs[i];

    if (spec->owner_type != type)
      continue;

    g_value_init (&value, spec->value_type);
    if (object && !!(spec->flags & G_PARAM_READABLE) &&
        !(spec->flags & GST_PARAM_DOC_SHOW_DEFAULT)) {
      g_object_get_property (G_OBJECT (object), spec->name, &value);
    } else {
      /* if we can't read the property value, assume it's set to the default
       * (which might not be entirely true for sub-classes, but that's an
       * unlikely corner-case anyway) */
      g_param_value_set_default (spec, &value);
    }

    if (!opened)
      g_string_append (json, ",\"properties\": {");

    if ((spec->flags & GST_PARAM_MUTABLE_PLAYING)) {
      mutable_str = "\"playing\"";
    } else if ((spec->flags & GST_PARAM_MUTABLE_PAUSED)) {
      mutable_str = "\"paused\"";
    } else if ((spec->flags & GST_PARAM_MUTABLE_READY)) {
      mutable_str = "\"ready\"";
    } else {
      mutable_str = "\"null\"";
    }

    tmpstr = json_strescape (g_param_spec_get_blurb (spec));
    g_string_append_printf (json,
        "%s"
        "\"%s\": {"
        "\"construct-only\": %s,"
        "\"construct\": %s,"
        "\"readable\": %s,"
        "\"writable\": %s,"
        "\"blurb\": \"%s\","
        "\"controllable\": %s,"
        "\"conditionally-available\": %s,"
        "\"mutable\": %s,"
        "\"type\": \"%s\"",
        opened ? "," : "",
        spec->name,
        spec->flags & G_PARAM_CONSTRUCT_ONLY ? "true" : "false",
        spec->flags & G_PARAM_CONSTRUCT ? "true" : "false",
        spec->flags & G_PARAM_READABLE ? "true" : "false",
        spec->flags & G_PARAM_WRITABLE ? "true" : "false", tmpstr,
        spec->flags & GST_PARAM_CONTROLLABLE ? "true" : "false",
        spec->flags & GST_PARAM_CONDITIONALLY_AVAILABLE ? "true" : "false",
        mutable_str, g_type_name (G_PARAM_SPEC_VALUE_TYPE (spec)));
    g_free (tmpstr);

    if (!g_hash_table_contains (seen_other_types,
            g_type_name (spec->value_type))
        && gst_type_is_plugin_api (spec->value_type, &api_flags)) {
      g_hash_table_insert (seen_other_types,
          (gpointer) g_type_name (spec->value_type), NULL);
      if (G_IS_PARAM_SPEC_ENUM (spec)) {
        _serialize_enum (other_types, spec->value_type, api_flags);
      } else if (G_IS_PARAM_SPEC_FLAGS (spec)) {
        _serialize_flags (other_types, spec->value_type);
      } else if (G_IS_PARAM_SPEC_OBJECT (spec)) {
        GType inst_type = spec->value_type;
        GObject *obj = g_value_get_object (&value);

        if (obj) {
          inst_type = G_OBJECT_TYPE (obj);
        }

        _serialize_object (other_types, seen_other_types, spec->value_type,
            inst_type);
      }
    }

    switch ((uintptr_t) G_VALUE_TYPE (&value)) {
      case (uintptr_t) G_TYPE_STRING:
      {
        const char *string_val = g_value_get_string (&value);
        gchar *tmpstr = json_strescape (string_val);

        g_string_append_printf (json, ",\"default\": \"%s\"", tmpstr);;
        g_free (tmpstr);
        break;
      }
      case (uintptr_t) G_TYPE_BOOLEAN:
      {
        gboolean bool_val = g_value_get_boolean (&value);

        g_string_append_printf (json, ",\"default\": \"%s\"",
            bool_val ? "true" : "false");
        break;
      }
      case (uintptr_t) G_TYPE_ULONG:
      {
        GParamSpecULong *pulong = G_PARAM_SPEC_ULONG (spec);

        g_string_append_printf (json,
            ",\"default\": \"%lu\""
            ",\"min\": \"%lu\""
            ",\"max\": \"%lu\"",
            g_value_get_ulong (&value), pulong->minimum, pulong->maximum);

        GST_ERROR_OBJECT (object,
            "property '%s' of type ulong: consider changing to " "uint/uint64",
            g_param_spec_get_name (spec));
        break;
      }
      case (uintptr_t) G_TYPE_LONG:
      {
        GParamSpecLong *plong = G_PARAM_SPEC_LONG (spec);

        g_string_append_printf (json,
            ",\"default\": \"%ld\""
            ",\"min\": \"%ld\""
            ",\"max\": \"%ld\"",
            g_value_get_long (&value), plong->minimum, plong->maximum);

        GST_ERROR_OBJECT (object,
            "property '%s' of type long: consider changing to " "int/int64",
            g_param_spec_get_name (spec));
        break;
      }
      case (uintptr_t) G_TYPE_UINT:
      {
        GParamSpecUInt *puint = G_PARAM_SPEC_UINT (spec);

        g_string_append_printf (json,
            ",\"default\": \"%d\""
            ",\"min\": \"%d\""
            ",\"max\": \"%d\"",
            g_value_get_uint (&value), puint->minimum, puint->maximum);
        break;
      }
      case (uintptr_t) G_TYPE_INT:
      {
        GParamSpecInt *pint = G_PARAM_SPEC_INT (spec);

        g_string_append_printf (json,
            ",\"default\": \"%d\""
            ",\"min\": \"%d\""
            ",\"max\": \"%d\"",
            g_value_get_int (&value), pint->minimum, pint->maximum);
        break;
      }
      case (uintptr_t) G_TYPE_UINT64:
      {
        GParamSpecUInt64 *puint64 = G_PARAM_SPEC_UINT64 (spec);

        g_string_append_printf (json,
            ",\"default\": \"%" G_GUINT64_FORMAT
            "\",\"min\": \"%" G_GUINT64_FORMAT
            "\",\"max\": \"%" G_GUINT64_FORMAT "\"",
            g_value_get_uint64 (&value), puint64->minimum, puint64->maximum);
        break;
      }
      case (uintptr_t) G_TYPE_INT64:
      {
        GParamSpecInt64 *pint64 = G_PARAM_SPEC_INT64 (spec);

        g_string_append_printf (json,
            ",\"default\": \"%" G_GUINT64_FORMAT
            "\",\"min\": \"%" G_GINT64_FORMAT
            "\",\"max\": \"%" G_GINT64_FORMAT "\"",
            g_value_get_int64 (&value), pint64->minimum, pint64->maximum);
        break;
      }
      case (uintptr_t) G_TYPE_FLOAT:
      {
        GParamSpecFloat *pfloat = G_PARAM_SPEC_FLOAT (spec);

        g_string_append_printf (json,
            ",\"default\": \"%g\""
            ",\"min\": \"%g\""
            ",\"max\": \"%g\"",
            g_value_get_float (&value), pfloat->minimum, pfloat->maximum);
        break;
      }
      case (uintptr_t) G_TYPE_DOUBLE:
      {
        GParamSpecDouble *pdouble = G_PARAM_SPEC_DOUBLE (spec);

        g_string_append_printf (json,
            ",\"default\": \"%g\""
            ",\"min\": \"%g\""
            ",\"max\": \"%g\"",
            g_value_get_double (&value), pdouble->minimum, pdouble->maximum);
        break;
      }
      case (uintptr_t) G_TYPE_CHAR:
      case (uintptr_t) G_TYPE_UCHAR:
        GST_ERROR_OBJECT (object,
            "property '%s' of type char: consider changing to " "int/string",
            g_param_spec_get_name (spec));
        /* fall through */
      default:
        if (spec->value_type == GST_TYPE_CAPS) {
          const GstCaps *caps = gst_value_get_caps (&value);

          if (caps) {
            gchar *capsstr = gst_caps_to_string (caps);
            gchar *tmpcapsstr = json_strescape (capsstr);

            g_string_append_printf (json, ",\"default\": \"%s\"", tmpcapsstr);
            g_free (capsstr);
            g_free (tmpcapsstr);
          }
        } else if (G_IS_PARAM_SPEC_BOXED (spec)) {
          if (spec->value_type == GST_TYPE_STRUCTURE) {
            const GstStructure *s = gst_value_get_structure (&value);
            if (s) {
              gchar *str = gst_structure_to_string (s);
              gchar *tmpstr = json_strescape (str);

              g_string_append_printf (json, ",\"default\": \"%s\"", tmpstr);
              g_free (str);
              g_free (tmpstr);
            }
          }
        } else if (GST_IS_PARAM_SPEC_FRACTION (spec)) {
          GstParamSpecFraction *pfraction = GST_PARAM_SPEC_FRACTION (spec);

          g_string_append_printf (json,
              ",\"default\": \"%d/%d\""
              ",\"min\": \"%d/%d\""
              ",\"max\": \"%d/%d\"",
              gst_value_get_fraction_numerator (&value),
              gst_value_get_fraction_denominator (&value),
              pfraction->min_num, pfraction->min_den,
              pfraction->max_num, pfraction->max_den);
        } else if (G_IS_PARAM_SPEC_ENUM (spec)) {
          _serialize_enum_default (json, spec->value_type, &value);
        } else if (G_IS_PARAM_SPEC_FLAGS (spec)) {
          _serialize_flags_default (json, spec->value_type, &value);
        }
        break;
    }

    g_string_append_c (json, '}');


    opened = TRUE;
  }

  if (opened)
    g_string_append (json, "}");

}

static gboolean
print_field (GQuark field, const GValue * value, GString * jcaps)
{
  gchar *tmp, *str = gst_value_serialize (value);

  if (!g_strcmp0 (g_quark_to_string (field), "format") ||
      !g_strcmp0 (g_quark_to_string (field), "rate")) {
    if (!cleanup_caps_field)
      cleanup_caps_field = g_regex_new ("\\(string\\)|\\(rate\\)", 0, 0, NULL);

    tmp = str;
    str = g_regex_replace (cleanup_caps_field, str, -1, 0, "", 0, NULL);;
    g_free (tmp);
  }

  g_string_append_printf (jcaps, "%15s: %s\n", g_quark_to_string (field), str);
  g_free (str);
  return TRUE;
}

static gchar *
_build_caps (const GstCaps * caps)
{
  guint i;
  gchar *res;
  GString *jcaps = g_string_new (NULL);

  if (gst_caps_is_any (caps)) {
    g_string_append (jcaps, "ANY");
    return g_string_free (jcaps, FALSE);
  }

  if (gst_caps_is_empty (caps)) {
    g_string_append (jcaps, "EMPTY");
    return g_string_free (jcaps, FALSE);
  }

  for (i = 0; i < gst_caps_get_size (caps); i++) {
    GstStructure *structure = gst_caps_get_structure (caps, i);
    GstCapsFeatures *features = gst_caps_get_features (caps, i);

    if (features && (gst_caps_features_is_any (features) ||
            !gst_caps_features_is_equal (features,
                GST_CAPS_FEATURES_MEMORY_SYSTEM_MEMORY))) {
      gchar *features_string = gst_caps_features_to_string (features);

      g_string_append_printf (jcaps, "%s%s(%s):\n",
          i ? "\n" : "", gst_structure_get_name (structure), features_string);
      g_free (features_string);
    } else {
      g_string_append_printf (jcaps, "%s:\n",
          gst_structure_get_name (structure));
    }
    gst_structure_foreach (structure, (GstStructureForeachFunc) print_field,
        jcaps);
  }

  res = json_strescape (jcaps->str);
  g_string_free (jcaps, TRUE);

  return res;
}

static void
_add_element_pad_templates (GString * json, GString * other_types,
    GHashTable * seen_other_types, GstElement * element,
    GstElementFactory * factory)
{
  gboolean opened = FALSE;
  const GList *pads;
  GstStaticPadTemplate *padtemplate;
  GRegex *re = g_regex_new ("%", 0, 0, NULL);
  GstPluginAPIFlags api_flags;

  pads = gst_element_factory_get_static_pad_templates (factory);
  while (pads) {
    GstCaps *documentation_caps;
    gchar *name, *caps;
    GType pad_type;
    GstPadTemplate *tmpl;
    padtemplate = (GstStaticPadTemplate *) (pads->data);
    pads = g_list_next (pads);

    tmpl = gst_element_class_get_pad_template (GST_ELEMENT_GET_CLASS (element),
        padtemplate->name_template);

    name = g_regex_replace (re, padtemplate->name_template,
        -1, 0, "%%", 0, NULL);;
    documentation_caps = gst_pad_template_get_documentation_caps (tmpl);
    caps = _build_caps (documentation_caps);
    gst_caps_replace (&documentation_caps, NULL);
    g_string_append_printf (json, "%s"
        "\"%s\": {"
        "\"caps\": \"%s\","
        "\"direction\": \"%s\","
        "\"presence\": \"%s\"",
        opened ? "," : ",\"pad-templates\": {",
        name, caps,
        padtemplate->direction ==
        GST_PAD_SRC ? "src" : padtemplate->direction ==
        GST_PAD_SINK ? "sink" : "unknown",
        padtemplate->presence ==
        GST_PAD_ALWAYS ? "always" : padtemplate->presence ==
        GST_PAD_SOMETIMES ? "sometimes" : padtemplate->presence ==
        GST_PAD_REQUEST ? "request" : "unknown");
    opened = TRUE;
    g_free (name);

    pad_type = GST_PAD_TEMPLATE_GTYPE (tmpl);
    if (pad_type != G_TYPE_NONE && pad_type != GST_TYPE_PAD) {
      g_string_append_printf (json, ", \"type\": \"%s\"",
          g_type_name (pad_type));

      if (!g_hash_table_contains (seen_other_types, g_type_name (pad_type))
          && gst_type_is_plugin_api (pad_type, &api_flags)) {
        g_hash_table_insert (seen_other_types,
            (gpointer) g_type_name (pad_type), NULL);
        _serialize_object (other_types, seen_other_types, pad_type, pad_type);
      }
    }
    g_string_append_c (json, '}');
  }
  if (opened)
    g_string_append_c (json, '}');

  g_regex_unref (re);
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
_add_factory_details (GString * json, GstElementFactory * factory)
{
  gchar **keys, **k;
  gboolean f = TRUE;

  keys = gst_element_factory_get_metadata_keys (factory);
  if (keys != NULL) {
    for (k = keys; *k != NULL; ++k) {
      gchar *val;
      gchar *key = *k;

      /* "long-name" can be varying depending on environment, skip this */
      if (g_strcmp0 (key, "long-name") == 0)
        continue;

      val = json_strescape (gst_element_factory_get_metadata (factory, key));
      g_string_append_printf (json, "%s\"%s\": \"%s\"", f ? "" : ",", key, val);
      f = FALSE;
      g_free (val);
    }
    g_strfreev (keys);
    g_string_append (json, ",");
  }
}

static void
_add_object_details (GString * json, GString * other_types,
    GHashTable * seen_other_types, GObject * object, GType type,
    GType inst_type)
{
  GType *interfaces;
  guint n_interfaces;
  GType ptype = type;

  g_string_append (json, "\"hierarchy\": [");

  for (;; ptype = g_type_parent (ptype)) {
    g_string_append_printf (json, "\"%s\"%c", g_type_name (ptype),
        ((ptype == G_TYPE_OBJECT || ptype == G_TYPE_INTERFACE) ? ' ' : ','));

    if (!g_hash_table_contains (seen_other_types, g_type_name (ptype))
        && gst_type_is_plugin_api (ptype, NULL)) {
      g_hash_table_insert (seen_other_types, (gpointer) g_type_name (ptype),
          NULL);
      _serialize_object (other_types, seen_other_types, ptype, inst_type);
    }

    if (ptype == G_TYPE_OBJECT || ptype == G_TYPE_INTERFACE)
      break;
  }
  g_string_append (json, "]");

  interfaces = g_type_interfaces (type, &n_interfaces);
  if (n_interfaces) {
    GType *iface;

    g_string_append (json, ",\"interfaces\": [");
    for (iface = interfaces; *iface; iface++, n_interfaces--) {
      g_string_append_printf (json, "\"%s\"%c", g_type_name (*iface),
          n_interfaces > 1 ? ',' : ' ');

      if (!g_hash_table_contains (seen_other_types, g_type_name (*iface))
          && gst_type_is_plugin_api (*iface, NULL)) {
        g_hash_table_insert (seen_other_types, (gpointer) g_type_name (*iface),
            NULL);
        _serialize_object (other_types, seen_other_types, *iface, inst_type);
      }
    }

    g_string_append (json, "]");
    g_free (interfaces);
  }

  _add_properties (json, other_types, seen_other_types, object,
      G_OBJECT_GET_CLASS (object), type);
  _add_signals (json, other_types, seen_other_types, object, type);
}

static void
_add_element_details (GString * json, GString * other_types,
    GHashTable * seen_other_types, GstPluginFeature * feature)
{
  GstElement *element =
      gst_element_factory_create (GST_ELEMENT_FACTORY (feature), NULL);
  char s[20];

  if (!element)
    g_error ("Couldn't not make `%s`", GST_OBJECT_NAME (feature));

  g_string_append_printf (json,
      "\"%s\": {"
      "\"rank\":\"%s\",",
      GST_OBJECT_NAME (feature),
      get_rank_name (s, gst_plugin_feature_get_rank (feature)));

  _add_factory_details (json, GST_ELEMENT_FACTORY (feature));
  _add_object_details (json, other_types, seen_other_types, G_OBJECT (element),
      G_OBJECT_TYPE (element), G_OBJECT_TYPE (element));

  _add_element_pad_templates (json, other_types, seen_other_types, element,
      GST_ELEMENT_FACTORY (feature));

  g_string_append (json, "}");
}

int
main (int argc, char *argv[])
{
  gchar *libfile;
  GError *error = NULL;
  GString *json;
  GString *other_types;
  GHashTable *seen_other_types;
  GstPlugin *plugin;
  gboolean f = TRUE;
  GList *features, *tmp;
  gint i;
  gboolean first = TRUE;
  GError *err = NULL;

  g_assert (argc >= 3);

  setlocale (LC_ALL, "");
  setlocale (LC_NUMERIC, "C");

#ifdef ENABLE_NLS
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);
#endif

  gst_init (NULL, NULL);

  json = g_string_new ("{");
  for (i = 2; i < argc; i++) {
    gchar *basename, **splitext, *filename;
    libfile = argv[i];
    plugin = gst_plugin_load_file (libfile, &error);
    if (!plugin) {
      g_printerr ("%s could not be loaded as a GstPlugin: %s", libfile,
          error->message ? error->message : "no known reasons");
      g_clear_error (&error);

      continue;
    }

    other_types = g_string_new ("");
    seen_other_types = g_hash_table_new (g_str_hash, g_str_equal);

    basename = g_filename_display_basename (libfile);
    splitext = g_strsplit (basename, ".", 2);
    filename =
        g_str_has_prefix (splitext[0], "lib") ? &splitext[0][3] : splitext[0];
    g_string_append_printf (json,
        "%s\"%s\": {"
        "\"description\":\"%s\","
        "\"filename\":\"%s\","
        "\"source\":\"%s\","
        "\"package\":\"%s\","
        "\"license\":\"%s\","
        "\"url\":\"%s\","
        "\"elements\":{",
        first ? "" : ",",
        gst_plugin_get_name (plugin),
        gst_plugin_get_description (plugin),
        filename,
        gst_plugin_get_source (plugin),
        gst_plugin_get_package (plugin),
        gst_plugin_get_license (plugin), gst_plugin_get_origin (plugin));
    g_free (basename);
    g_strfreev (splitext);
    first = FALSE;

    features =
        gst_registry_get_feature_list_by_plugin (gst_registry_get (),
        gst_plugin_get_name (plugin));

    f = TRUE;
    for (tmp = features; tmp; tmp = tmp->next) {
      GstPluginFeature *feature = tmp->data;
      if (GST_IS_ELEMENT_FACTORY (feature)) {
        GstElementFactory *factory = GST_ELEMENT_FACTORY (feature);
        if (gst_element_factory_get_skip_documentation (factory))
          continue;

        if (!f)
          g_string_append_printf (json, ",");
        _add_element_details (json, other_types, seen_other_types, feature);
        f = FALSE;
      }
    }

    g_string_append (json, "}, \"tracers\": {");
    gst_plugin_feature_list_free (features);

    f = TRUE;
    features =
        gst_registry_get_feature_list_by_plugin (gst_registry_get (),
        gst_plugin_get_name (plugin));
    for (tmp = features; tmp; tmp = tmp->next) {
      GstPluginFeature *feature = tmp->data;

      if (GST_IS_TRACER_FACTORY (feature)) {
        if (!f)
          g_string_append_printf (json, ",");
        g_string_append_printf (json, "\"%s\": {}", GST_OBJECT_NAME (feature));
        f = FALSE;
      }
    }
    g_string_append_printf (json, "}, \"other-types\": {%s}}",
        other_types->str);
    gst_plugin_feature_list_free (features);

    g_hash_table_unref (seen_other_types);
    g_string_free (other_types, TRUE);
  }

  g_string_append_c (json, '}');
  if (!g_file_set_contents (argv[1], json->str, -1, &err)) {
    g_printerr ("Could not set json to %s: %s", argv[1], err->message);
    g_clear_error (&err);

    return -1;
  }
  g_string_free (json, TRUE);

  return 0;
}
