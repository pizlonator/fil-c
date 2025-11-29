#include "resource-holder.h"

struct _ResourceHolder {
  GObject instance;

  char *name;
  char *path;
  int count;
  gsize size;
  GListModel *children;
  ResourceHolder *parent;
};

G_DEFINE_TYPE (ResourceHolder, resource_holder, G_TYPE_OBJECT)

static void
resource_holder_init (ResourceHolder *holder)
{
}

static void
resource_holder_finalize (GObject *object)
{
  ResourceHolder *holder = RESOURCE_HOLDER (object);

  g_free (holder->name);
  g_free (holder->path);
  g_clear_object (&holder->children);

  G_OBJECT_CLASS (resource_holder_parent_class)->finalize (object);
}

static void
resource_holder_class_init (ResourceHolderClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = resource_holder_finalize;
}

ResourceHolder *
resource_holder_new (const char *name,
                     const char *path,
                     int count,
                     gsize size,
                     GListModel *children)
{
  ResourceHolder *holder;

  holder = g_object_new (RESOURCE_TYPE_HOLDER, NULL);

  holder->name = g_strdup (name);
  holder->path = g_strdup (path);
  holder->count = count;
  holder->size = size;
  g_set_object (&holder->children, children);

  if (children)
    {
      int i;
      for (i = 0; i < g_list_model_get_n_items (children); i++)
        {
          ResourceHolder *child = g_list_model_get_item (children, i);
          child->parent = holder;
          g_object_unref (child);
        }
    }

  return holder;
}

const char *
resource_holder_get_name (ResourceHolder *holder)
{
  return holder->name;
}

const char *
resource_holder_get_path (ResourceHolder *holder)
{
  return holder->path;
}

int
resource_holder_get_count (ResourceHolder *holder)
{
  return holder->count;
}

gsize
resource_holder_get_size (ResourceHolder *holder)
{
  return holder->size;
}

GListModel *
resource_holder_get_children (ResourceHolder *holder)
{
  return holder->children;
}

ResourceHolder *
resource_holder_get_parent (ResourceHolder *holder)
{
  return holder->parent;
}
