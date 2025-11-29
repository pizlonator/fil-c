#include <gtk/gtk.h>

#define DEMO_TYPE_IMAGE (demo_image_get_type ())

G_DECLARE_FINAL_TYPE (DemoImage, demo_image, DEMO, IMAGE, GtkWidget)

struct _DemoImage {
  GtkWidget parent_instance;

  GdkTexture *texture;
};

G_DEFINE_TYPE (DemoImage, demo_image, GTK_TYPE_WIDGET)

static void
demo_image_init (DemoImage *demo)
{
}

static void
demo_image_dispose (GObject *object)
{
  DemoImage *demo = DEMO_IMAGE (object);

  g_clear_object (&demo->texture);

  G_OBJECT_CLASS (demo_image_parent_class)->dispose (object);
}

static void
demo_image_measure (GtkWidget      *widget,
                    GtkOrientation  orientation,
                    int             for_size,
                    int            *minimum,
                    int            *natural,
                    int            *minimum_baseline,
                    int            *natural_baseline)
{
  DemoImage *demo = DEMO_IMAGE (widget);
  double scale;

  g_print ("measure\n");
  scale = gdk_surface_get_scale (gtk_native_get_surface (gtk_widget_get_native (widget)));

  if (orientation == GTK_ORIENTATION_VERTICAL)
    {
      *minimum = *natural = (int) ceil (gdk_texture_get_height (demo->texture) / scale);
      g_print ("requesting height: %d\n", *minimum);
    }
  else
    {
      *minimum = *natural = (int) ceil (gdk_texture_get_width (demo->texture) / scale);
      g_print ("requesting width: %d\n", *minimum);
    }
}

static void
demo_image_snapshot (GtkWidget   *widget,
                     GtkSnapshot *snapshot)
{
  DemoImage *demo = DEMO_IMAGE (widget);
  GtkNative *native = gtk_widget_get_native (widget);
  graphene_point_t point;
  double scale;
  double ox, oy;
  double x, y, width, height;

  g_print ("snapshot\n");

  scale = gdk_surface_get_scale (gtk_native_get_surface (native));

  g_print ("scale %f\n", scale);

  /* width and height that give us 1-1 mapping to device pixels */
  width = gdk_texture_get_width (demo->texture) / scale;
  height = gdk_texture_get_height (demo->texture) / scale;

  gtk_native_get_surface_transform (native, &ox, &oy);

  g_print ("surface transform %f %f\n", ox, oy);

  x = (gtk_widget_get_width (widget) - width) / 2;
  y = (gtk_widget_get_height (widget) - height) / 2;

  g_print ("texture origin in widget coordinates: %f %f\n", x, y);

  if (!gtk_widget_compute_point (widget, GTK_WIDGET (native), &GRAPHENE_POINT_INIT (x, y), &point))
    return;

  x = point.x;
  y = point.y;

  g_print ("in window (app) coordinates: %f %f\n", x, y);

  x += ox;
  y += oy;

  g_print ("in surface (app) coordinates: %f %f\n", x, y);

  x *= scale;
  y *= scale;

  g_print ("in surface (device) coordinates: %f %f\n", x, y);

  /* Now x, y are the surface (device) coordinates of the widget's origin */

  /* Round up to the next full device pixel */

  x = ceil (x);
  y = ceil (y);

  g_print ("rounded up: %f %f\n", x, y);

  /* And back to widget coordinates */

  x /= scale;
  y /= scale;

  x -= ox;
  y -= oy;

  if (!gtk_widget_compute_point (widget, GTK_WIDGET (native), &GRAPHENE_POINT_INIT (0, 0), &point))
    return;

  x -= point.x;
  y -= point.y;

  g_print ("bounds: %f %f %f %f\n", x, y, width, height);

  gtk_snapshot_append_texture (snapshot, demo->texture,
                               &GRAPHENE_RECT_INIT (x, y, width, height));
}

static void
notify_scale (GObject *object,
              GParamSpec *pspec,
              GtkWidget *widget)
{
  g_print ("scale change!\n");

  gtk_widget_queue_resize (widget);
}

static void
demo_image_realize (GtkWidget *widget)
{
  GtkNative *native;
  GdkSurface *surface;

  GTK_WIDGET_CLASS (demo_image_parent_class)->realize (widget);

  g_print ("realize\n");

  native = gtk_widget_get_native (widget);
  surface = gtk_native_get_surface (native);
  g_signal_connect (surface, "notify::scale",
                    G_CALLBACK (notify_scale), widget);
}

static void
demo_image_unrealize (GtkWidget *widget)
{
  GtkNative *native;
  GdkSurface *surface;

  native = gtk_widget_get_native (widget);
  surface = gtk_native_get_surface (native);
  g_signal_handlers_disconnect_by_func (surface, notify_scale, widget);

  GTK_WIDGET_CLASS (demo_image_parent_class)->unrealize (widget);
}

static void
demo_image_class_init (DemoImageClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->dispose = demo_image_dispose;

  widget_class->realize = demo_image_realize;
  widget_class->unrealize = demo_image_unrealize;

  widget_class->measure = demo_image_measure;
  widget_class->snapshot = demo_image_snapshot;

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
}

static GtkWidget *
demo_image_new (GdkTexture *texture)
{
  DemoImage *demo;

  demo = g_object_new (DEMO_TYPE_IMAGE, NULL);

  demo->texture = g_object_ref (texture);

  g_print ("texture size %dx%d\n",
           gdk_texture_get_width (texture),
           gdk_texture_get_height (texture));

  return GTK_WIDGET (demo);
}

static GdkTexture *
make_checkerboard_texture (int width, int height)
{
  guint32 *data, *row;
  GBytes *bytes;
  GdkTexture *texture;

  data = (guint32 *) g_new (guint32, width * height);

  for (int y = 0; y < height; y++)
    {
      row = data + y * width;
      for (int x = 0; x < width; x++)
        {
          if ((x + y) % 2)
            row[x] = 0xffffffff;
          else
            row[x] = 0xff000000;
        }
    }

  bytes = g_bytes_new_take (data, height * width * 4);

  texture = gdk_memory_texture_new (width, height, GDK_MEMORY_DEFAULT, bytes, width * 4);

  g_bytes_unref (bytes);

  return texture;
}

static gboolean
toggle_fullscreen (GtkWidget *widget,
                   GVariant  *args,
                   gpointer   data)
{
  GtkWindow *window = GTK_WINDOW (widget);

  if (gtk_window_is_fullscreen (window))
    gtk_window_unfullscreen (window);
  else
    gtk_window_fullscreen (window);

  return TRUE;
}

int
main (int argc, char *argv[])
{
  GtkWidget *window;
  GdkTexture *texture;
  GError *error = NULL;
  GtkEventController *controller;
  GtkShortcutTrigger *trigger;
  GtkShortcutAction *action;
  GtkShortcut *shortcut;

  gtk_init ();

  if (argc > 1)
    {
      texture = gdk_texture_new_from_filename (argv[1], &error);
      if (!texture)
        g_error ("%s", error->message);
    }
  else
    {
      texture = make_checkerboard_texture (100, 100);
    }

  window = gtk_window_new ();

  controller = gtk_shortcut_controller_new ();
  trigger = gtk_keyval_trigger_new (GDK_KEY_F11, GDK_NO_MODIFIER_MASK);
  action = gtk_callback_action_new (toggle_fullscreen, NULL, NULL);
  shortcut = gtk_shortcut_new (trigger, action);
  gtk_shortcut_controller_add_shortcut (GTK_SHORTCUT_CONTROLLER (controller), shortcut);
  gtk_widget_add_controller (window, controller);

  gtk_window_set_child (GTK_WINDOW (window), demo_image_new (texture));

  g_object_unref (texture);

  gtk_window_present (GTK_WINDOW (window));

  while (g_list_model_get_n_items (gtk_window_get_toplevels ()) > 0)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
