#include "prop-holder.h"

enum {
  PROP_OBJECT = 1,
  PROP_PSPEC,
  PROP_NAME,
  NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES];

struct _PropHolder {
  GObject instance;

  GObject *object;
  GParamSpec *pspec;
};

G_DEFINE_TYPE (PropHolder, prop_holder, G_TYPE_OBJECT)

static void
prop_holder_init (PropHolder *holder)
{
}

static void
prop_holder_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  PropHolder *holder = PROP_HOLDER (object);

  switch (prop_id)
    {
    case PROP_OBJECT:
      holder->object = g_value_dup_object (value);
      break;

    case PROP_PSPEC:
      holder->pspec = g_value_get_param (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
prop_holder_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  PropHolder *holder = PROP_HOLDER (object);

  switch (prop_id)
    {
    case PROP_OBJECT:
      g_value_set_object (value, holder->object);
      break;

    case PROP_PSPEC:
      g_value_set_param (value, holder->pspec);
      break;

    case PROP_NAME:
      g_value_set_string (value, holder->pspec->name);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
prop_holder_finalize (GObject *object)
{
  PropHolder *holder = PROP_HOLDER (object);

  g_object_unref (holder->object);

  G_OBJECT_CLASS (prop_holder_parent_class)->finalize (object);
}

static void
prop_holder_class_init (PropHolderClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = prop_holder_finalize;
  object_class->set_property = prop_holder_set_property;
  object_class->get_property = prop_holder_get_property;

  properties[PROP_OBJECT] =
      g_param_spec_object ("object", NULL, NULL,
                           G_TYPE_OBJECT,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  properties[PROP_PSPEC] =
      g_param_spec_param ("pspec", NULL, NULL,
                          G_TYPE_PARAM,
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  properties[PROP_NAME] =
      g_param_spec_string ("name", NULL, NULL,
                           NULL,
                           G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);
}

PropHolder *
prop_holder_new (GObject    *object,
                 GParamSpec *pspec)
{
  PropHolder *holder;

  holder = g_object_new (PROP_TYPE_HOLDER,
                         "object", object,
                         "pspec", pspec,
                         NULL);

  return holder;
}

GObject *
prop_holder_get_object (PropHolder *holder)
{
  return holder->object;
}

GParamSpec *
prop_holder_get_pspec (PropHolder *holder)
{
  return holder->pspec;
}

const char *
prop_holder_get_name (PropHolder *holder)
{
  return holder->pspec->name;
}
