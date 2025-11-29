#include <string.h>
#include "iconbrowserapp.h"
#include "iconbrowserwin.h"
#include "iconbrowsericon.h"
#include "iconbrowsercontext.h"
#include <gtk/gtk.h>


struct _IconBrowserWindow
{
  GtkApplicationWindow parent;

  GtkWidget *symbolic_radio;
  GtkWidget *searchbar;
  GListModel *icon_filter_model;
  GListStore *icon_store;
  GListModel *context_model;
  GListStore *context_store;
  GtkFilter *name_filter;
  GtkFilter *search_mode_filter;
  GtkWidget *details;
  GtkWidget *image1;
  GtkWidget *image2;
  GtkWidget *image3;
  GtkWidget *image4;
  GtkWidget *image5;
  GtkWidget *image6;
  GtkWidget *image7;
  GtkWidget *image8;
  GtkWidget *label8;
  GtkWidget *description;
};

struct _IconBrowserWindowClass
{
  GtkApplicationWindowClass parent_class;
};

G_DEFINE_TYPE(IconBrowserWindow, icon_browser_window, GTK_TYPE_APPLICATION_WINDOW);

static GtkIconTheme *
icon_browser_window_get_icon_theme (IconBrowserWindow *win)
{
  return gtk_icon_theme_get_for_display (gtk_widget_get_display (GTK_WIDGET (win)));
}

static void
add_icon (IconBrowserWindow *win,
          const char        *name,
          const char        *description,
          const char        *context)
{
  GtkIconTheme *icon_theme = icon_browser_window_get_icon_theme (win);
  char *regular_name;
  char *symbolic_name;
  IbIcon *icon;

  regular_name = g_strdup (name);
  if (!gtk_icon_theme_has_icon (icon_theme, regular_name))
    {
      g_free (regular_name);
      regular_name = NULL;
    }

  symbolic_name = g_strconcat (name, "-symbolic", NULL);
  if (!gtk_icon_theme_has_icon (icon_theme, symbolic_name))
    {
      g_free (symbolic_name);
      symbolic_name = NULL;
    }

  icon = ib_icon_new (regular_name, symbolic_name, description, context);
  g_object_bind_property (win->symbolic_radio, "active",
                          icon, "use-symbolic",
                          G_BINDING_DEFAULT);
  g_list_store_append (win->icon_store, icon);
  g_object_unref (icon);
}

static void
add_context (IconBrowserWindow *win,
             const char        *id,
             const char        *name,
             const char        *description)
{
  IbContext *context;

  context = ib_context_new (id, name, description);
  g_list_store_append (win->context_store, context);
  g_object_unref (context);
}

static void
populate (IconBrowserWindow *win)
{
  GFile *file;
  GKeyFile *kf;
  char *data;
  gsize length;
  char **groups;
  int i;

  file = g_file_new_for_uri ("resource:/org/gtk/iconbrowser/gtk/icon.list");
  g_file_load_contents (file, NULL, &data, &length, NULL, NULL);

  kf = g_key_file_new ();
  g_key_file_load_from_data (kf, data, length, G_KEY_FILE_NONE, NULL);

  groups = g_key_file_get_groups (kf, &length);
  for (i = 0; i < length; i++)
    {
      const char *context;
      const char *name;
      const char *description;
      char **keys;
      gsize len;
      int j;

      context = groups[i];
      name = g_key_file_get_string (kf, context, "Name", NULL);
      description = g_key_file_get_string (kf, context, "Description", NULL);
      add_context (win, context, name, description);

      keys = g_key_file_get_keys (kf, context, &len, NULL);
      for (j = 0; j < len; j++)
        {
          const char *key = keys[j];
          const char *value;

          if (strcmp (key, "Name") == 0 || strcmp (key, "Description") == 0)
            continue;

          value = g_key_file_get_string (kf, context, key, NULL);

          add_icon (win, key, value, context);
        }
      g_strfreev (keys);
    }
  g_strfreev (groups);
}

static gboolean
filter_by_icon_name (gpointer item,
                     gpointer data)
{
  return ib_icon_get_name (IB_ICON (item)) != NULL;
}

static void
symbolic_toggled (IconBrowserWindow *win)
{
  gtk_filter_changed (win->name_filter, GTK_FILTER_CHANGE_DIFFERENT);
}

static void
copy_to_clipboard (GtkButton         *button,
                   IconBrowserWindow *win)
{
  GdkClipboard *clipboard;

  clipboard = gtk_widget_get_clipboard (GTK_WIDGET (win));
  gdk_clipboard_set_text (clipboard, gtk_window_get_title (GTK_WINDOW (win->details)));
}

static void
set_image (GtkWidget *image, const char *name, int size)
{
  gtk_image_set_from_icon_name (GTK_IMAGE (image), name);
  gtk_image_set_pixel_size (GTK_IMAGE (image), size);
}

static void
item_activated (GtkGridView       *view,
                guint              position,
                IconBrowserWindow *win)
{
  GListModel *model = G_LIST_MODEL (gtk_grid_view_get_model (view));
  IbIcon *icon = g_list_model_get_item (model, position);
  const char *name;
  const char *description;
  gboolean symbolic;

  name = ib_icon_get_name (icon);
  description = ib_icon_get_description (icon);
  symbolic = ib_icon_get_use_symbolic (icon);

  gtk_window_set_title (GTK_WINDOW (win->details), name);
  set_image (win->image1, name, 8);
  set_image (win->image2, name, 16);
  set_image (win->image3, name, 18);
  set_image (win->image4, name, 24);
  set_image (win->image5, name, 32);
  set_image (win->image6, name, 48);
  set_image (win->image7, name, 64);
  gtk_widget_set_visible (win->image8, symbolic);
  gtk_widget_set_visible (win->label8, symbolic);
  if (symbolic)
    set_image (win->image8, name, 64);
  gtk_widget_set_visible (win->description, description && description[0]);
  if (description && description[0])
    gtk_label_set_text (GTK_LABEL (win->description), description);

  gtk_window_present (GTK_WINDOW (win->details));

  g_object_unref (icon);
}

static GdkPaintable *
get_image_paintable (GtkImage *image)
{
  const char *icon_name;
  GtkIconTheme *icon_theme;
  GtkIconPaintable *icon;
  int size;

  switch (gtk_image_get_storage_type (image))
    {
    case GTK_IMAGE_PAINTABLE:
      return g_object_ref (gtk_image_get_paintable (image));
    case GTK_IMAGE_ICON_NAME:
      icon_name = gtk_image_get_icon_name (image);
      size = gtk_image_get_pixel_size (image);
      icon_theme = gtk_icon_theme_get_for_display (gtk_widget_get_display (GTK_WIDGET (image)));
      icon = gtk_icon_theme_lookup_icon (icon_theme,
                                         icon_name,
                                         NULL,
                                         size, 1,
                                         gtk_widget_get_direction (GTK_WIDGET (image)),
                                         0);
      if (icon == NULL)
        {
g_print ("no icon for %s\n", icon_name);
          return NULL;
        }
      return GDK_PAINTABLE (icon);
    case GTK_IMAGE_GICON:
    case GTK_IMAGE_EMPTY:
    default:
      g_warning ("Image storage type %d not handled",
                 gtk_image_get_storage_type (image));
      return NULL;
    }
}

static void
drag_begin (GtkDragSource *source,
            GdkDrag       *drag,
            GtkWidget     *widget)
{
  GdkPaintable *paintable;

  paintable = get_image_paintable (GTK_IMAGE (widget));
  if (paintable)
    {
      int w, h;

      w = gdk_paintable_get_intrinsic_width (paintable);
      h = gdk_paintable_get_intrinsic_height (paintable);
      gtk_drag_source_set_icon (source, paintable, w, h);
      g_object_unref (paintable);
    }
}

static GdkContentProvider *
drag_prepare_texture (GtkDragSource *source,
                      double         x,
                      double         y,
                      GtkWidget     *widget)
{
  GdkPaintable *paintable = get_image_paintable (GTK_IMAGE (widget));
  GtkSnapshot *snapshot;
  double width, height;
  GskRenderNode *node;
  GskRenderer *renderer;
  GdkTexture *texture;
  GdkContentProvider *ret;

  if (!GDK_IS_PAINTABLE (paintable))
    return NULL;

  snapshot = gtk_snapshot_new ();
  width = gdk_paintable_get_intrinsic_width (paintable);
  height = gdk_paintable_get_intrinsic_height (paintable);
  gdk_paintable_snapshot (paintable, snapshot, width, height);
  node = gtk_snapshot_free_to_node (snapshot);

  renderer = gtk_native_get_renderer (gtk_widget_get_native (widget));
  texture = gsk_renderer_render_texture (renderer, node, &GRAPHENE_RECT_INIT (0, 0, width, height));

  ret = gdk_content_provider_new_typed (GDK_TYPE_TEXTURE, texture);

  g_object_unref (texture);
  gsk_render_node_unref (node);

  return ret;
}

static GdkContentProvider *
drag_prepare_file (GtkDragSource *source,
                   double         x,
                   double         y,
                   GtkWidget     *widget)
{
  GdkContentProvider *content;
  GtkIconTheme *icon_theme;
  const char *name;
  GtkIconPaintable *info;

  name = gtk_image_get_icon_name (GTK_IMAGE (widget));
  icon_theme = gtk_icon_theme_get_for_display (gtk_widget_get_display (widget));

  info = gtk_icon_theme_lookup_icon (icon_theme,
                                     name,
                                     NULL,
                                     32, 1,
                                     gtk_widget_get_direction (widget),
                                     0);
  content = gdk_content_provider_new_typed (G_TYPE_FILE,  gtk_icon_paintable_get_file (info));
  g_object_unref (info);

  return content;
}

static void
setup_image_dnd (GtkWidget *image)
{
  GtkDragSource *source;

  source = gtk_drag_source_new ();
  g_signal_connect (source, "prepare", G_CALLBACK (drag_prepare_texture), image);
  g_signal_connect (source, "drag-begin", G_CALLBACK (drag_begin), image);
  gtk_widget_add_controller (image, GTK_EVENT_CONTROLLER (source));
}

static void
setup_scalable_image_dnd (GtkWidget *image)
{
  GtkDragSource *source;

  source = gtk_drag_source_new ();
  g_signal_connect (source, "prepare", G_CALLBACK (drag_prepare_file), image);
  g_signal_connect (source, "drag-begin", G_CALLBACK (drag_begin), image);
  gtk_widget_add_controller (image, GTK_EVENT_CONTROLLER (source));
}

static void
search_mode_toggled (GtkSearchBar      *searchbar,
                     GParamSpec        *pspec,
                     IconBrowserWindow *win)
{
  if (gtk_search_bar_get_search_mode (searchbar))
    gtk_single_selection_set_selected (GTK_SINGLE_SELECTION (win->context_model), GTK_INVALID_LIST_POSITION);
  else if (gtk_single_selection_get_selected (GTK_SINGLE_SELECTION (win->context_model)) == GTK_INVALID_LIST_POSITION)
    gtk_single_selection_set_selected (GTK_SINGLE_SELECTION (win->context_model), 0);

  gtk_filter_changed (win->search_mode_filter, GTK_FILTER_CHANGE_DIFFERENT);
}

static void
selected_name_changed (GtkSingleSelection *selection,
                       GParamSpec         *pspec,
                       IconBrowserWindow  *win)
{
  if (gtk_single_selection_get_selected (selection) != GTK_INVALID_LIST_POSITION)
    gtk_search_bar_set_search_mode (GTK_SEARCH_BAR (win->searchbar), FALSE);
}

static void
icon_browser_window_init (IconBrowserWindow *win)
{
  GtkFilter *filter;

  gtk_widget_init_template (GTK_WIDGET (win));

  setup_image_dnd (win->image1);
  setup_image_dnd (win->image2);
  setup_image_dnd (win->image3);
  setup_image_dnd (win->image4);
  setup_image_dnd (win->image5);
  setup_image_dnd (win->image6);
  setup_image_dnd (win->image7);
  setup_scalable_image_dnd (win->image8);

  gtk_window_set_transient_for (GTK_WINDOW (win->details), GTK_WINDOW (win));
  gtk_search_bar_set_key_capture_widget (GTK_SEARCH_BAR (win->searchbar), GTK_WIDGET (win));

  populate (win);

  filter = gtk_filter_list_model_get_filter (GTK_FILTER_LIST_MODEL (win->icon_filter_model));

  win->name_filter = GTK_FILTER (gtk_custom_filter_new (filter_by_icon_name, NULL, NULL));

  gtk_multi_filter_append (GTK_MULTI_FILTER (filter), g_object_ref (win->name_filter));

  g_signal_connect (win->searchbar, "notify::search-mode-enabled",  G_CALLBACK (search_mode_toggled), win);
  g_signal_connect (win->context_model, "notify::selected",  G_CALLBACK (selected_name_changed), win);
}

static void
icon_browser_window_dispose (GObject *object)
{
  gtk_widget_dispose_template (GTK_WIDGET (object), ICON_BROWSER_WINDOW_TYPE);

  G_OBJECT_CLASS (icon_browser_window_parent_class)->dispose (object);
}

static void
icon_browser_window_finalize (GObject *object)
{
  IconBrowserWindow *win = ICON_BROWSER_WINDOW (object);

  g_clear_object (&win->name_filter);

  G_OBJECT_CLASS (icon_browser_window_parent_class)->finalize (object);
}

static void
icon_browser_window_class_init (IconBrowserWindowClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = icon_browser_window_dispose;
  object_class->finalize = icon_browser_window_finalize;

  g_type_ensure (IB_TYPE_ICON);
  g_type_ensure (IB_TYPE_CONTEXT);

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (class),
                                               "/org/gtk/iconbrowser/gtk/window.ui");

  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), IconBrowserWindow, symbolic_radio);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), IconBrowserWindow, searchbar);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), IconBrowserWindow, icon_store);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), IconBrowserWindow, icon_filter_model);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), IconBrowserWindow, context_model);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), IconBrowserWindow, context_store);

  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), IconBrowserWindow, details);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), IconBrowserWindow, image1);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), IconBrowserWindow, image2);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), IconBrowserWindow, image3);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), IconBrowserWindow, image4);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), IconBrowserWindow, image5);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), IconBrowserWindow, image6);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), IconBrowserWindow, image7);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), IconBrowserWindow, image8);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), IconBrowserWindow, label8);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), IconBrowserWindow, description);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), IconBrowserWindow, search_mode_filter);

  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), item_activated);
  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), copy_to_clipboard);
  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), symbolic_toggled);
}

IconBrowserWindow *
icon_browser_window_new (IconBrowserApp *app)
{
  return g_object_new (ICON_BROWSER_WINDOW_TYPE, "application", app, NULL);
}
