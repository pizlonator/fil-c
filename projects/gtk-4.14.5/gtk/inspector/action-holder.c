
#include "action-holder.h"

struct _ActionHolder {
  GObject instance;

  GObject *owner;
  char *name;
};

static guint changed_signal;

G_DEFINE_TYPE (ActionHolder, action_holder, G_TYPE_OBJECT)

static void
action_holder_init (ActionHolder *holder)
{
}

static void
action_holder_finalize (GObject *object)
{
  ActionHolder *holder = ACTION_HOLDER (object);

  g_object_unref (holder->owner);
  g_free (holder->name);

  G_OBJECT_CLASS (action_holder_parent_class)->finalize (object);
}

static void
action_holder_class_init (ActionHolderClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = action_holder_finalize;

  changed_signal =
    g_signal_new ("changed",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_FIRST,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);
}

ActionHolder *
action_holder_new (GObject    *owner,
                   const char *name)
{
  ActionHolder *holder;

  holder = g_object_new (ACTION_TYPE_HOLDER, NULL);

  holder->owner = g_object_ref (owner);
  holder->name = g_strdup (name);

  return holder;
}

GObject *
action_holder_get_owner (ActionHolder *holder)
{
  return holder->owner;
}

const char *
action_holder_get_name (ActionHolder *holder)
{
  return holder->name;
}

void
action_holder_changed (ActionHolder *holder)
{
  g_signal_emit (holder, changed_signal, 0);
}
