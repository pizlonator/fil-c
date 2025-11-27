#include "gi-sample.h"

struct _GISampleThing
{
  GObject parent_instance;

  gchar *msg;
};

G_DEFINE_TYPE (GISampleThing, gi_sample_thing, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_MSG,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

/**
 * gi_sample_thing_new:
 *
 * Allocates a new #GISampleThing.
 *
 * Returns: (transfer full): a #GISampleThing.
 */
GISampleThing *
gi_sample_thing_new (void)
{
  return g_object_new (GI_TYPE_SAMPLE_THING, NULL);
}

static void
gi_sample_thing_finalize (GObject *object)
{
  GISampleThing *self = (GISampleThing *)object;

  g_clear_pointer (&self->msg, g_free);

  G_OBJECT_CLASS (gi_sample_thing_parent_class)->finalize (object);
}

static void
gi_sample_thing_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GISampleThing *self = GI_SAMPLE_THING (object);

  switch (prop_id)
    {
    case PROP_MSG:
      g_value_set_string (value, self->msg);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gi_sample_thing_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GISampleThing *self = GI_SAMPLE_THING (object);

  switch (prop_id)
    {
    case PROP_MSG:
      self->msg = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gi_sample_thing_class_init (GISampleThingClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gi_sample_thing_finalize;
  object_class->get_property = gi_sample_thing_get_property;
  object_class->set_property = gi_sample_thing_set_property;

  gParamSpecs [PROP_MSG] =
    g_param_spec_string ("message",
                         "Message",
                         "The message to print.",
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, gParamSpecs);
}

static void
gi_sample_thing_init (GISampleThing *self)
{
}

/**
 * gi_sample_thing_print_message:
 * @self: a #GISampleThing.
 *
 * Prints the message.
 */
void
gi_sample_thing_print_message (GISampleThing *self)
{
  g_return_if_fail (GI_IS_SAMPLE_THING (self));

  g_print ("Message: %s\n", self->msg);
}
