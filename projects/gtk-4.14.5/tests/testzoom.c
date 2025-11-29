#include <gtk/gtk.h>

typedef struct
{
  GtkWidget parent_instance;
  GtkWidget *child;
  float scale;
  float angle;
} GtkZoom;

typedef struct
{
  GtkWidgetClass parent_class;
} GtkZoomClass;

enum {
  PROP_0,
  PROP_CHILD,
  PROP_SCALE,
  PROP_ANGLE,
  NUM_PROPERTIES
};

static GParamSpec *props[NUM_PROPERTIES] = { NULL, };

GType gtk_zoom_get_type (void);

G_DEFINE_TYPE (GtkZoom, gtk_zoom, GTK_TYPE_WIDGET)

static void
gtk_zoom_init (GtkZoom *zoom)
{
  zoom->child = NULL;
  zoom->scale = 1.0;
  zoom->angle = 0.0;
}

static void
gtk_zoom_dispose (GObject *object)
{
  GtkZoom *zoom = (GtkZoom *)object;

  g_clear_pointer (&zoom->child, gtk_widget_unparent);

  G_OBJECT_CLASS (gtk_zoom_parent_class)->dispose (object);
}

static void
update_transform (GtkZoom *zoom)
{
  GtkLayoutManager *manager;
  GtkLayoutChild *child;
  GskTransform *transform;
  graphene_rect_t bounds;
  int w, h;
  int x, y;

  manager = gtk_widget_get_layout_manager (GTK_WIDGET (zoom));
  child = gtk_layout_manager_get_layout_child (manager, zoom->child);

  w = gtk_widget_get_width (GTK_WIDGET (zoom));
  h = gtk_widget_get_height (GTK_WIDGET (zoom));

  if (!gtk_widget_compute_bounds (GTK_WIDGET (zoom->child), GTK_WIDGET (zoom->child), &bounds))
    return;

  x = bounds.size.width;
  y = bounds.size.height;

  transform = NULL;
  transform = gsk_transform_translate (transform, &GRAPHENE_POINT_INIT (w/2, h/2));
  transform = gsk_transform_scale (transform, zoom->scale, zoom->scale);
  transform = gsk_transform_rotate (transform, zoom->angle);
  transform = gsk_transform_translate (transform, &GRAPHENE_POINT_INIT (-x/2, -y/2));
  gtk_fixed_layout_child_set_transform (GTK_FIXED_LAYOUT_CHILD (child), transform);
  gsk_transform_unref (transform);
}

static void
gtk_zoom_set_scale (GtkZoom *zoom,
                    float    scale)
{

  if (zoom->scale == scale)
    return;

  zoom->scale = scale;

  update_transform (zoom);

  g_object_notify_by_pspec (G_OBJECT (zoom), props[PROP_SCALE]);

  gtk_widget_queue_resize (GTK_WIDGET (zoom));
}

static void
gtk_zoom_set_angle (GtkZoom *zoom,
                    float    angle)
{

  if (zoom->angle == angle)
    return;

  zoom->angle = angle;

  update_transform (zoom);

  g_object_notify_by_pspec (G_OBJECT (zoom), props[PROP_ANGLE]);

  gtk_widget_queue_resize (GTK_WIDGET (zoom));
}

static void
gtk_zoom_set_child (GtkZoom   *zoom,
                    GtkWidget *child)
{
  g_clear_pointer (&zoom->child, gtk_widget_unparent);

  zoom->child = child;

  if (zoom->child)
    gtk_widget_set_parent (zoom->child, GTK_WIDGET (zoom));

  update_transform (zoom);

  g_object_notify_by_pspec (G_OBJECT (zoom), props[PROP_CHILD]);
}

static void
gtk_zoom_set_property (GObject      *object,
                       guint         prop_id,
                       const GValue *value,
                       GParamSpec   *pspec)
{
  GtkZoom *zoom = (GtkZoom *)object;

  switch (prop_id)
    {
    case PROP_SCALE:
      gtk_zoom_set_scale (zoom, g_value_get_float (value));
      break;

    case PROP_ANGLE:
      gtk_zoom_set_angle (zoom, g_value_get_float (value));
      break;

    case PROP_CHILD:
      gtk_zoom_set_child (zoom, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_zoom_get_property (GObject    *object,
                       guint       prop_id,
                       GValue     *value,
                       GParamSpec *pspec)
{
  GtkZoom *zoom = (GtkZoom *)object;

  switch (prop_id)
    {
    case PROP_SCALE:
      g_value_set_float (value, zoom->scale);
      break;

    case PROP_ANGLE:
      g_value_set_float (value, zoom->angle);
      break;

    case PROP_CHILD:
      g_value_set_object (value, zoom->child);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_zoom_class_init (GtkZoomClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->dispose = gtk_zoom_dispose;
  object_class->set_property = gtk_zoom_set_property;
  object_class->get_property = gtk_zoom_get_property;

  props[PROP_SCALE] = g_param_spec_float ("scale", "", "",
                                          0.0, 100.0, 1.0,
                                          G_PARAM_READWRITE);

  props[PROP_ANGLE] = g_param_spec_float ("angle", "", "",
                                          0.0, 360.0, 1.0,
                                          G_PARAM_READWRITE);

  props[PROP_CHILD] = g_param_spec_object ("child", "", "",
                                           GTK_TYPE_WIDGET,
                                           G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, props);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_FIXED_LAYOUT);
}

static GtkWidget *
gtk_zoom_new (void)
{
  return g_object_new (gtk_zoom_get_type (), NULL);
}

static gboolean
update_transform_once (GtkWidget     *widget,
                       GdkFrameClock *frame_clock,
                       gpointer       data)
{
  static int count = 0;

  update_transform ((GtkZoom *)widget);
  count++;

  if (count == 2)
    return G_SOURCE_REMOVE;

  return G_SOURCE_CONTINUE;
}

int
main (int argc, char *argv[])
{
  GtkWindow *window;
  GtkWidget *zoom;
  GtkWidget *box;
  GtkWidget *grid;
  GtkWidget *scale;
  GtkWidget *angle;
  GtkWidget *child;
  GtkAdjustment *adjustment;

  gtk_init ();

  window = GTK_WINDOW (gtk_window_new ());
  gtk_window_set_default_size (GTK_WINDOW (window), 600, 400);
  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_window_set_child (window, box);

  grid = gtk_grid_new ();
  gtk_grid_set_column_spacing (GTK_GRID (grid), 10);

  scale = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 1.0, 10.0, 1.0);
  gtk_widget_set_hexpand (scale, TRUE);
  gtk_grid_attach (GTK_GRID (grid), gtk_label_new ("Scale"), 0, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), scale, 1, 0, 1, 1);

  angle = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0.0, 360.0, 1.0);
  gtk_scale_add_mark (GTK_SCALE (angle),  90.0, GTK_POS_BOTTOM, NULL);
  gtk_scale_add_mark (GTK_SCALE (angle), 180.0, GTK_POS_BOTTOM, NULL);
  gtk_scale_add_mark (GTK_SCALE (angle), 270.0, GTK_POS_BOTTOM, NULL);
  gtk_widget_set_hexpand (angle, TRUE);
  gtk_grid_attach (GTK_GRID (grid), gtk_label_new ("Angle"), 0, 1, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), angle, 1, 1, 1, 1);
  gtk_box_append (GTK_BOX (box), grid);

  zoom = gtk_zoom_new ();
  gtk_widget_set_hexpand (zoom, TRUE);
  gtk_widget_set_vexpand (zoom, TRUE);
  gtk_box_append (GTK_BOX (box), zoom);

  adjustment = gtk_range_get_adjustment (GTK_RANGE (scale));
  g_object_bind_property (adjustment, "value",
                          zoom, "scale",
                          G_BINDING_DEFAULT);

  adjustment = gtk_range_get_adjustment (GTK_RANGE (angle));
  g_object_bind_property (adjustment, "value",
                          zoom, "angle",
                          G_BINDING_DEFAULT);

  if (argc > 1)
    {
      GtkBuilder *builder = gtk_builder_new ();
      gtk_builder_add_from_file (builder, argv[1], NULL);
      child = GTK_WIDGET (gtk_builder_get_object (builder, "child"));
      gtk_zoom_set_child ((GtkZoom *)zoom, child);
      g_object_unref (builder);
    }
  else
    gtk_zoom_set_child ((GtkZoom *)zoom, gtk_button_new_with_label ("Click me!"));

  gtk_window_present (window);

  /* HACK to get the transform initally updated */
  gtk_widget_add_tick_callback (zoom, update_transform_once, NULL, NULL);

  while (g_list_model_get_n_items (gtk_window_get_toplevels ()) > 0)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
