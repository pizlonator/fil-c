#include "config.h"

#include "gtklayoutchild.h"

#include "gtklayoutmanager.h"
#include "gtkprivate.h"

/**
 * GtkLayoutChild:
 *
 * `GtkLayoutChild` is the base class for objects that are meant to hold
 * layout properties.
 *
 * If a `GtkLayoutManager` has per-child properties, like their packing type,
 * or the horizontal and vertical span, or the icon name, then the layout
 * manager should use a `GtkLayoutChild` implementation to store those properties.
 *
 * A `GtkLayoutChild` instance is only ever valid while a widget is part
 * of a layout.
 */

typedef struct {
  GtkLayoutManager *manager;
  GtkWidget *widget;
} GtkLayoutChildPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GtkLayoutChild, gtk_layout_child, G_TYPE_OBJECT)

enum {
  PROP_LAYOUT_MANAGER = 1,
  PROP_CHILD_WIDGET,

  N_PROPS
};

static GParamSpec *layout_child_properties[N_PROPS];

static void
gtk_layout_child_set_property (GObject      *gobject,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GtkLayoutChild *layout_child = GTK_LAYOUT_CHILD (gobject);
  GtkLayoutChildPrivate *priv = gtk_layout_child_get_instance_private (layout_child);

  switch (prop_id)
    {
    case PROP_LAYOUT_MANAGER:
      priv->manager = g_value_get_object (value);
      break;

    case PROP_CHILD_WIDGET:
      priv->widget = g_value_get_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
gtk_layout_child_get_property (GObject      *gobject,
                               guint         prop_id,
                               GValue       *value,
                               GParamSpec   *pspec)
{
  GtkLayoutChild *layout_child = GTK_LAYOUT_CHILD (gobject);
  GtkLayoutChildPrivate *priv = gtk_layout_child_get_instance_private (layout_child);

  switch (prop_id)
    {
    case PROP_LAYOUT_MANAGER:
      g_value_set_object (value, priv->manager);
      break;

    case PROP_CHILD_WIDGET:
      g_value_set_object (value, priv->widget);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
gtk_layout_child_constructed (GObject *gobject)
{
  GtkLayoutChild *layout_child = GTK_LAYOUT_CHILD (gobject);
  GtkLayoutChildPrivate *priv = gtk_layout_child_get_instance_private (layout_child);

  G_OBJECT_CLASS (gtk_layout_child_parent_class)->constructed (gobject);

  if (priv->manager == NULL)
    {
      g_critical ("The layout child of type %s does not have "
                  "the GtkLayoutChild:layout-manager property set",
                  G_OBJECT_TYPE_NAME (gobject));
      return;
    }

  if (priv->widget == NULL)
    {
      g_critical ("The layout child of type %s does not have "
                  "the GtkLayoutChild:child-widget property set",
                  G_OBJECT_TYPE_NAME (gobject));
      return;
    }
}

static void
gtk_layout_child_class_init (GtkLayoutChildClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = gtk_layout_child_set_property;
  gobject_class->get_property = gtk_layout_child_get_property;
  gobject_class->constructed = gtk_layout_child_constructed;

  /**
   * GtkLayoutChild:layout-manager: (attributes org.gtk.Property.get=gtk_layout_child_get_layout_manager)
   *
   * The layout manager that created the `GtkLayoutChild` instance.
   */
  layout_child_properties[PROP_LAYOUT_MANAGER] =
    g_param_spec_object ("layout-manager", NULL, NULL,
                         GTK_TYPE_LAYOUT_MANAGER,
                         GTK_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY);

  /**
   * GtkLayoutChild:child-widget: (attributes org.gtk.Property.get=gtk_layout_child_get_child_widget)
   *
   * The widget that is associated to the `GtkLayoutChild` instance.
   */
  layout_child_properties[PROP_CHILD_WIDGET] =
    g_param_spec_object ("child-widget", NULL, NULL,
                         GTK_TYPE_WIDGET,
                         GTK_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (gobject_class, N_PROPS, layout_child_properties);
}

static void
gtk_layout_child_init (GtkLayoutChild *self)
{
}

/**
 * gtk_layout_child_get_layout_manager: (attributes org.gtk.Method.get_property=layout-manager)
 * @layout_child: a `GtkLayoutChild`
 *
 * Retrieves the `GtkLayoutManager` instance that created the
 * given @layout_child.
 *
 * Returns: (transfer none): a `GtkLayoutManager`
 */
GtkLayoutManager *
gtk_layout_child_get_layout_manager (GtkLayoutChild *layout_child)
{
  GtkLayoutChildPrivate *priv = gtk_layout_child_get_instance_private (layout_child);

  g_return_val_if_fail (GTK_IS_LAYOUT_CHILD (layout_child), NULL);

  return priv->manager;
}

/**
 * gtk_layout_child_get_child_widget: (attributes org.gtk.Method.get_property=child-widget)
 * @layout_child: a `GtkLayoutChild`
 *
 * Retrieves the `GtkWidget` associated to the given @layout_child.
 *
 * Returns: (transfer none): a `GtkWidget`
 */
GtkWidget *
gtk_layout_child_get_child_widget (GtkLayoutChild *layout_child)
{
  GtkLayoutChildPrivate *priv = gtk_layout_child_get_instance_private (layout_child);

  g_return_val_if_fail (GTK_IS_LAYOUT_CHILD (layout_child), NULL);

  return priv->widget;
}
