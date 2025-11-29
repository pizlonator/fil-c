#include "nodewidget.h"

struct _NodeWidget
{
  GtkWidget parent_instance;

  GskRenderNode *node;
};

struct _NodeWidgetClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE (NodeWidget, node_widget, GTK_TYPE_WIDGET)

static void
node_widget_init (NodeWidget *self)
{
}

static void
node_widget_dispose (GObject *object)
{
  NodeWidget *self = NODE_WIDGET (object);

  gsk_render_node_unref (self->node);

  G_OBJECT_CLASS (node_widget_parent_class)->dispose (object);
}

static void
node_widget_snapshot (GtkWidget   *widget,
                      GtkSnapshot *snapshot)
{
  NodeWidget *self = NODE_WIDGET (widget);

  gtk_snapshot_append_node (snapshot, self->node);
}

static void
node_widget_class_init (NodeWidgetClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->dispose = node_widget_dispose;

  widget_class->snapshot = node_widget_snapshot;
}

GtkWidget *
node_widget_new (const char *resource)
{
  NodeWidget *self;
  GBytes *bytes;
  GskRenderNode *node;
  graphene_rect_t bounds;
  float scale;
  GskTransform *transform;

  self = g_object_new (NODE_TYPE_WIDGET, NULL);

  bytes = g_resources_lookup_data (resource, 0, NULL);
  node = gsk_render_node_deserialize (bytes, NULL, NULL);
  g_bytes_unref (bytes);

  gsk_render_node_get_bounds (node, &bounds);
  scale = MIN (100.0/bounds.size.width, 100.0/bounds.size.height);
  transform = gsk_transform_scale (NULL, scale, scale);
  self->node = gsk_transform_node_new (node, transform);
  gsk_transform_unref (transform);
  gsk_render_node_unref (node);

  return GTK_WIDGET (self);
}
