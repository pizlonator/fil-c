#include "iconbrowsercontext.h"

struct _IbContext
{
  GObject parent_instance;

  char *id;
  char *name;
  char *description;
};

struct _IbContextClass
{
  GObjectClass parent_class;
};

enum {
  PROP_ID = 1,
  PROP_NAME,
  PROP_DESCRIPTION,
  PROP_NUM_PROPERTIES
};

G_DEFINE_TYPE (IbContext, ib_context, G_TYPE_OBJECT)

static void
ib_context_init (IbContext *context)
{
}

static void
ib_context_finalize (GObject *object)
{
  IbContext *context = IB_CONTEXT (object);

  g_free (context->id);
  g_free (context->name);
  g_free (context->description);

  G_OBJECT_CLASS (ib_context_parent_class)->finalize (object);
}

static void
ib_context_set_property (GObject      *object,
                         guint         property_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  IbContext *context = IB_CONTEXT (object);

  switch (property_id)
    {
    case PROP_ID:
      g_free (context->id);
      context->id = g_value_dup_string (value);
      break;

    case PROP_NAME:
      g_free (context->name);
      context->name = g_value_dup_string (value);
      break;

    case PROP_DESCRIPTION:
      g_free (context->description);
      context->description = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
ib_context_get_property (GObject    *object,
                      guint       property_id,
                      GValue     *value,
                      GParamSpec *pspec)
{
  IbContext *context = IB_CONTEXT (object);

  switch (property_id)
    {
    case PROP_ID:
      g_value_set_string (value, context->id);
      break;

    case PROP_NAME:
      g_value_set_string (value, context->name);
      break;

    case PROP_DESCRIPTION:
      g_value_set_string (value, context->description);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
ib_context_class_init (IbContextClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GParamSpec *pspec;

  object_class->finalize = ib_context_finalize;
  object_class->set_property = ib_context_set_property;
  object_class->get_property = ib_context_get_property;

  pspec = g_param_spec_string ("id", "Id", "Id",
                               NULL,
                               G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_ID, pspec);

  pspec = g_param_spec_string ("name", "Name", "Name",
                               NULL,
                               G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_NAME, pspec);

  pspec = g_param_spec_string ("description", "Description", "Description",
                               NULL,
                               G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_DESCRIPTION, pspec);
}

IbContext *
ib_context_new (const char *id,
                const char *name,
                const char *description)
{
  return g_object_new (IB_TYPE_CONTEXT,
                       "id", id,
                       "name", name,
                       "description", description,
                       NULL);
}

const char *
ib_context_get_id (IbContext *context)
{
  return context->id;
}

const char *
ib_context_get_name (IbContext *context)
{
  return context->name;
}

const char *
ib_context_get_description (IbContext *context)
{
  return context->description;
}
