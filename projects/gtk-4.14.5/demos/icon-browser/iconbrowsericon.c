#include "iconbrowsericon.h"

struct _IbIcon
{
  GObject parent_instance;

  gboolean use_symbolic;

  char *regular_name;
  char *symbolic_name;
  char *description;
  char *context;
};

struct _IbIconClass
{
  GObjectClass parent_class;
};

enum {
  PROP_NAME = 1,
  PROP_REGULAR_NAME,
  PROP_SYMBOLIC_NAME,
  PROP_USE_SYMBOLIC,
  PROP_DESCRIPTION,
  PROP_CONTEXT,
  PROP_NUM_PROPERTIES
};

G_DEFINE_TYPE (IbIcon, ib_icon, G_TYPE_OBJECT)

static void
ib_icon_init (IbIcon *icon)
{
}

static void
ib_icon_finalize (GObject *object)
{
  IbIcon *icon = IB_ICON (object);

  g_free (icon->regular_name);
  g_free (icon->symbolic_name);
  g_free (icon->description);
  g_free (icon->context);

  G_OBJECT_CLASS (ib_icon_parent_class)->finalize (object);
}

static void
ib_icon_set_property (GObject      *object,
                      guint         property_id,
                      const GValue *value,
                      GParamSpec   *pspec)
{
  IbIcon *icon = IB_ICON (object);

  switch (property_id)
    {
    case PROP_REGULAR_NAME:
      g_free (icon->regular_name);
      icon->regular_name = g_value_dup_string (value);
      if (!icon->use_symbolic)
        g_object_notify (object, "name");
      break;

    case PROP_SYMBOLIC_NAME:
      g_free (icon->symbolic_name);
      icon->symbolic_name = g_value_dup_string (value);
      if (icon->use_symbolic)
        g_object_notify (object, "name");
      break;

    case PROP_USE_SYMBOLIC:
      icon->use_symbolic = g_value_get_boolean (value);
      g_object_notify (object, "name");
      break;

    case PROP_DESCRIPTION:
      g_free (icon->description);
      icon->description = g_value_dup_string (value);
      break;

    case PROP_CONTEXT:
      g_free (icon->context);
      icon->context = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
ib_icon_get_property (GObject    *object,
                      guint       property_id,
                      GValue     *value,
                      GParamSpec *pspec)
{
  IbIcon *icon = IB_ICON (object);

  switch (property_id)
    {
    case PROP_NAME:
      g_value_set_string (value, ib_icon_get_name (icon));
      break;

    case PROP_REGULAR_NAME:
      g_value_set_string (value, icon->regular_name);
      break;

    case PROP_SYMBOLIC_NAME:
      g_value_set_string (value, icon->symbolic_name);
      break;

    case PROP_USE_SYMBOLIC:
      g_value_set_boolean (value, icon->use_symbolic);
      break;

    case PROP_DESCRIPTION:
      g_value_set_string (value, icon->description);
      break;

    case PROP_CONTEXT:
      g_value_set_string (value, icon->context);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
ib_icon_class_init (IbIconClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GParamSpec *pspec;

  object_class->finalize = ib_icon_finalize;
  object_class->set_property = ib_icon_set_property;
  object_class->get_property = ib_icon_get_property;

  pspec = g_param_spec_string ("name", "Name", "Name",
                               NULL,
                               G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_NAME, pspec);

  pspec = g_param_spec_string ("regular-name", "Regular Name", "Regular Name",
                               NULL,
                               G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_REGULAR_NAME, pspec);

  pspec = g_param_spec_string ("symbolic-name", "Symbolic Name", "Symbolic Name",
                               NULL,
                               G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_SYMBOLIC_NAME, pspec);

  pspec = g_param_spec_boolean ("use-symbolic", "Use Symbolic", "Use Symbolic",
                                FALSE,
                                G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_USE_SYMBOLIC, pspec);

  pspec = g_param_spec_string ("description", "Description", "Description",
                               NULL,
                               G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_DESCRIPTION, pspec);

  pspec = g_param_spec_string ("context", "Context", "Context",
                               NULL,
                               G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_CONTEXT, pspec);
}

IbIcon *
ib_icon_new (const char *regular_name,
             const char *symbolic_name,
             const char *description,
             const char *context)
{
  return g_object_new (IB_TYPE_ICON,
                       "regular-name", regular_name,
                       "symbolic-name", symbolic_name,
                       "description", description,
                       "context", context,
                       NULL);
}

const char *
ib_icon_get_name (IbIcon *icon)
{
  if (icon->use_symbolic)
    return icon->symbolic_name;
  else
    return icon->regular_name;
}

const char *
ib_icon_get_regular_name (IbIcon *icon)
{
  return icon->regular_name;
}

const char *
ib_icon_get_symbolic_name (IbIcon *icon)
{
  return icon->symbolic_name;
}

gboolean
ib_icon_get_use_symbolic (IbIcon *icon)
{
  return icon->use_symbolic;
}

const char *
ib_icon_get_description (IbIcon *icon)
{
  return icon->description;
}

const char *
ib_icon_get_context (IbIcon *icon)
{
  return icon->context;
}

