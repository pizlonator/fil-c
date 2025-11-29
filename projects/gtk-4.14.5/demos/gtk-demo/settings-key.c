#include "settings-key.h"

/* Create an object that wraps GSettingsSchemaKey because that's a boxed type */
struct _SettingsKey
{
  GObject parent_instance;

  GSettings *settings;
  GSettingsSchemaKey *key;
};

enum {
  PROP_0,
  PROP_NAME,
  PROP_SETTINGS,
  PROP_SUMMARY,
  PROP_DESCRIPTION,
  PROP_VALUE,
  PROP_TYPE,
  PROP_DEFAULT_VALUE,

  N_PROPS
};

G_DEFINE_TYPE (SettingsKey, settings_key, G_TYPE_OBJECT);
static GParamSpec *properties[N_PROPS] = { NULL, };

static void
settings_key_get_property (GObject    *object,
                           guint       property_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  SettingsKey *self = SETTINGS_KEY (object);

  switch (property_id)
    {
    case PROP_DESCRIPTION:
      g_value_set_string (value, g_settings_schema_key_get_description (self->key));
      break;

    case PROP_NAME:
      g_value_set_string (value, g_settings_schema_key_get_name (self->key));
      break;

    case PROP_SUMMARY:
      g_value_set_string (value, g_settings_schema_key_get_summary (self->key));
      break;

    case PROP_VALUE:
      {
        GVariant *variant = g_settings_get_value (self->settings, g_settings_schema_key_get_name (self->key));
        g_value_take_string (value, g_variant_print (variant, FALSE));
        g_variant_unref (variant);
      }
      break;

    case PROP_TYPE:
      {
        const GVariantType *type = g_settings_schema_key_get_value_type (self->key);
        g_value_set_string (value, g_variant_type_peek_string (type));
      }
      break;

    case PROP_DEFAULT_VALUE:
      {
        GVariant *variant = g_settings_schema_key_get_default_value (self->key);
        g_value_take_string (value, g_variant_print (variant, FALSE));
        g_variant_unref (variant);
      }
      break;

    case PROP_SETTINGS:
      g_value_set_object (value, self->settings);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
settings_key_finalize (GObject *object)
{
  SettingsKey *self = SETTINGS_KEY (object);

  g_object_unref (self->settings);
  g_settings_schema_key_unref (self->key);

  G_OBJECT_CLASS (settings_key_parent_class)->finalize (object);
}

static void
settings_key_class_init (SettingsKeyClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = settings_key_finalize;
  gobject_class->get_property = settings_key_get_property;

  properties[PROP_DESCRIPTION] =
    g_param_spec_string ("description", NULL, NULL, NULL, G_PARAM_READABLE);
  properties[PROP_NAME] =
    g_param_spec_string ("name", NULL, NULL, NULL, G_PARAM_READABLE);
  properties[PROP_SETTINGS] =
    g_param_spec_object ("settings", NULL, NULL, G_TYPE_SETTINGS, G_PARAM_READABLE);
  properties[PROP_SUMMARY] =
    g_param_spec_string ("summary", NULL, NULL, NULL, G_PARAM_READABLE);
  properties[PROP_VALUE] =
    g_param_spec_string ("value", NULL, NULL, NULL, G_PARAM_READABLE);
  properties[PROP_TYPE] =
    g_param_spec_string ("type", NULL, NULL, NULL, G_PARAM_READABLE);
  properties[PROP_DEFAULT_VALUE] =
    g_param_spec_string ("default-value", NULL, NULL, NULL, G_PARAM_READABLE);

  g_object_class_install_properties (gobject_class, N_PROPS, properties);
}

static void
settings_key_init (SettingsKey *self)
{
}

SettingsKey *
settings_key_new (GSettings          *settings,
                  GSettingsSchemaKey *key)
{
  SettingsKey *result = g_object_new (SETTINGS_TYPE_KEY, NULL);

  result->settings = g_object_ref (settings);
  result->key = g_settings_schema_key_ref (key);

  return result;
}

GSettingsSchemaKey *
settings_key_get_key (SettingsKey *self)
{
  return self->key;
}

GSettings *
settings_key_get_settings (SettingsKey *self)
{
  return self->settings;
}

char *
settings_key_get_search_string (SettingsKey *self)
{
  char *schema, *result;

  g_object_get (self->settings, "schema-id", &schema, NULL);

  result = g_strconcat (g_settings_schema_key_get_name (self->key), " ",
                        g_settings_schema_key_get_summary (self->key), " ",
                        schema,
                        NULL);

  g_free (schema);

  return result;
}

