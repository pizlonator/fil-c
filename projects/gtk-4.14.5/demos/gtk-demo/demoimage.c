#include "demoimage.h"
#include <glib/gi18n.h>

struct _DemoImage {
  GtkWidget parent_instance;
 
  GtkWidget *image;
  GtkWidget *popover;
};

enum {
  PROP_ICON_NAME = 1
};

G_DEFINE_TYPE(DemoImage, demo_image, GTK_TYPE_WIDGET)

static GdkPaintable *
get_image_paintable (GtkImage *image)
{
  const char *icon_name;
  GtkIconTheme *icon_theme;
  GtkIconPaintable *icon;

  switch (gtk_image_get_storage_type (image))
    {
    case GTK_IMAGE_PAINTABLE:
      return g_object_ref (gtk_image_get_paintable (image));
    case GTK_IMAGE_ICON_NAME:
      icon_name = gtk_image_get_icon_name (image);
      icon_theme = gtk_icon_theme_get_for_display (gtk_widget_get_display (GTK_WIDGET (image)));
      icon = gtk_icon_theme_lookup_icon (icon_theme,
                                         icon_name,
                                         NULL,
                                         48, 1,
                                         gtk_widget_get_direction (GTK_WIDGET (image)),
                                         0);
      if (icon == NULL)
        return NULL;
      return GDK_PAINTABLE (icon);

    case GTK_IMAGE_EMPTY:
    case GTK_IMAGE_GICON:
    default:
      g_warning ("Image storage type %d not handled",
                 gtk_image_get_storage_type (image));
      return NULL;
    }
}

static void
update_drag_icon (DemoImage   *demo,
                  GtkDragIcon *icon)
{
  const char *icon_name;
  GdkPaintable *paintable;
  GtkWidget *image;

  switch (gtk_image_get_storage_type (GTK_IMAGE (demo->image)))
    {
    case GTK_IMAGE_PAINTABLE:
      paintable = gtk_image_get_paintable (GTK_IMAGE (demo->image));
      image = gtk_image_new_from_paintable (paintable);
      break;
    case GTK_IMAGE_ICON_NAME:
      icon_name = gtk_image_get_icon_name (GTK_IMAGE (demo->image));
      image = gtk_image_new_from_icon_name (icon_name);
      break;
    case GTK_IMAGE_EMPTY:
    case GTK_IMAGE_GICON:
    default:
      g_warning ("Image storage type %d not handled",
                 gtk_image_get_storage_type (GTK_IMAGE (demo->image)));
      return;
    }

  gtk_image_set_pixel_size (GTK_IMAGE (image),
                            gtk_image_get_pixel_size (GTK_IMAGE (demo->image)));

  gtk_drag_icon_set_child (icon, image);
}

static void
drag_begin (GtkDragSource *source,
            GdkDrag       *drag,
            gpointer       data)
{
  GtkWidget *widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (source));
  DemoImage *demo = DEMO_IMAGE (widget);

  update_drag_icon (demo, GTK_DRAG_ICON (gtk_drag_icon_get_for_drag (drag)));
}

static GdkContentProvider *
prepare_drag (GtkDragSource *source,
              double         x,
              double         y,
              gpointer       data)
{
  GtkWidget *widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (source));
  DemoImage *demo = DEMO_IMAGE (widget);
  GdkPaintable *paintable = get_image_paintable (GTK_IMAGE (demo->image));

  /* Textures can be serialized, paintables can't, so special case the textures */
  if (GDK_IS_TEXTURE (paintable))
    return gdk_content_provider_new_typed (GDK_TYPE_TEXTURE, paintable);
  else
    return gdk_content_provider_new_typed (GDK_TYPE_PAINTABLE, paintable);
}

static gboolean
drag_drop (GtkDropTarget *dest,
           const GValue  *value,
           double         x,
           double         y,
           gpointer       data)
{
  GtkWidget *widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (dest));
  DemoImage *demo = DEMO_IMAGE (widget);
  GdkPaintable *paintable = g_value_get_object (value);

  gtk_image_set_from_paintable (GTK_IMAGE (demo->image), paintable);

  return TRUE;
}

static void
copy_image (GtkWidget *widget,
            const char *action_name,
            GVariant *parameter)
{
  GdkClipboard *clipboard = gtk_widget_get_clipboard (widget);
  DemoImage *demo = DEMO_IMAGE (widget);
  GdkPaintable *paintable = get_image_paintable (GTK_IMAGE (demo->image));
  GValue value = G_VALUE_INIT;

  /* Textures can be serialized, paintables can't, so special case the textures */
  if (GDK_IS_TEXTURE (paintable))
    g_value_init (&value, GDK_TYPE_TEXTURE);
  else
    g_value_init (&value, GDK_TYPE_PAINTABLE);
  g_value_set_object (&value, paintable);
  gdk_clipboard_set_value (clipboard, &value);
  g_value_unset (&value);

  if (paintable)
    g_object_unref (paintable);
}

static void
paste_image_cb (GObject      *source,
                GAsyncResult *result,
                gpointer      data)
{
  GdkClipboard *clipboard = GDK_CLIPBOARD (source);
  DemoImage *demo = DEMO_IMAGE (data);
  const GValue *value;

  value = gdk_clipboard_read_value_finish (clipboard, result, NULL);
  if (value == NULL)
    {
      gtk_widget_error_bell (GTK_WIDGET (demo));
      g_object_unref (demo);
      return;
    }

  gtk_image_set_from_paintable (GTK_IMAGE (demo->image), g_value_get_object (value));
  g_object_unref (demo);
}

static void
paste_image (GtkWidget *widget,
             const char *action_name,
             GVariant *parameter)
{
  GdkClipboard *clipboard = gtk_widget_get_clipboard (widget);
  GType type;

  if (gdk_content_formats_contain_gtype (gdk_clipboard_get_formats (clipboard), GDK_TYPE_TEXTURE))
    type = GDK_TYPE_TEXTURE;
  else
    type = GDK_TYPE_PAINTABLE;

  gdk_clipboard_read_value_async (clipboard, 
                                  type,
                                  G_PRIORITY_DEFAULT,
                                  NULL,
                                  paste_image_cb,
                                  g_object_ref (widget));
}

static void
pressed_cb (GtkGesture *gesture,
            int         n_press,
            double      x,
            double      y,
            gpointer    data)
{
  DemoImage *demo = DEMO_IMAGE (gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture)));

  gtk_popover_popup (GTK_POPOVER (demo->popover));
}

static void
demo_image_init (DemoImage *demo)
{
  GMenu *menu;
  GMenuItem *item;
  GtkDragSource *source;
  GtkDropTarget *dest;
  GtkGesture *gesture;

  demo->image = gtk_image_new ();
  gtk_image_set_pixel_size (GTK_IMAGE (demo->image), 48);
  gtk_widget_set_parent (demo->image, GTK_WIDGET (demo));

  menu = g_menu_new (); 
  item = g_menu_item_new (_("_Copy"), "clipboard.copy");
  g_menu_append_item (menu, item);

  item = g_menu_item_new (_("_Paste"), "clipboard.paste");
  g_menu_append_item (menu, item);

  demo->popover = gtk_popover_menu_new_from_model (G_MENU_MODEL (menu));
  gtk_widget_set_parent (demo->popover, GTK_WIDGET (demo));

  source = gtk_drag_source_new ();
  g_signal_connect (source, "prepare", G_CALLBACK (prepare_drag), NULL);
  g_signal_connect (source, "drag-begin", G_CALLBACK (drag_begin), NULL);
  gtk_widget_add_controller (GTK_WIDGET (demo), GTK_EVENT_CONTROLLER (source));

  dest = gtk_drop_target_new (GDK_TYPE_PAINTABLE, GDK_ACTION_COPY);
  g_signal_connect (dest, "drop", G_CALLBACK (drag_drop), NULL);
  gtk_widget_add_controller (GTK_WIDGET (demo), GTK_EVENT_CONTROLLER (dest));

  gesture = gtk_gesture_click_new ();
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), GDK_BUTTON_SECONDARY);
  g_signal_connect (gesture, "pressed", G_CALLBACK (pressed_cb), NULL);
  gtk_widget_add_controller (GTK_WIDGET (demo), GTK_EVENT_CONTROLLER (gesture));
}

static void
demo_image_dispose (GObject *object)
{
  DemoImage *demo = DEMO_IMAGE (object);

  g_clear_pointer (&demo->image, gtk_widget_unparent);
  g_clear_pointer (&demo->popover, gtk_widget_unparent);

  G_OBJECT_CLASS (demo_image_parent_class)->dispose (object);
}

static void
demo_image_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  DemoImage *demo = DEMO_IMAGE (object);

  switch (prop_id)
    {
    case PROP_ICON_NAME:
      g_value_set_string (value, gtk_image_get_icon_name (GTK_IMAGE (demo->image)));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
demo_image_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  DemoImage *demo = DEMO_IMAGE (object);

  switch (prop_id)
    {
    case PROP_ICON_NAME:
      gtk_image_set_from_icon_name (GTK_IMAGE (demo->image),
                                    g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
demo_image_class_init (DemoImageClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->dispose = demo_image_dispose;
  object_class->get_property = demo_image_get_property;
  object_class->set_property = demo_image_set_property;

  g_object_class_install_property (object_class, PROP_ICON_NAME,
      g_param_spec_string ("icon-name", "Icon name", "Icon name",
                           NULL, G_PARAM_READWRITE));
                       
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);

  gtk_widget_class_install_action (widget_class, "clipboard.copy", NULL, copy_image);
  gtk_widget_class_install_action (widget_class, "clipboard.paste", NULL, paste_image);
}

GtkWidget *
demo_image_new (const char *icon_name)
{
  return g_object_new (DEMO_TYPE_IMAGE, "icon-name", icon_name, NULL);
}
