/* Lists/File browser
 * #Keywords: GListModel
 *
 * This demo shows off the different layouts that are quickly achievable
 * with GtkListview and GtkGridView by implementing a file browser with
 * different views.
 */

#include <glib/gi18n.h>
#include <gtk/gtk.h>

static GtkWidget *window = NULL;

/* Create a simple object that holds the data for the different views */
typedef struct _FileBrowserView FileBrowserView;
struct _FileBrowserView
{
  GObject parent_instance;

  GtkListItemFactory *factory;
  char *icon_name;
  char *title;
  GtkOrientation orientation;
};

enum {
  PROP_0,
  PROP_FACTORY,
  PROP_ICON_NAME,
  PROP_TITLE,
  PROP_ORIENTATION,

  N_PROPS
};

#define FILE_BROWSER_TYPE_VIEW (file_browser_view_get_type ())
G_MODULE_EXPORT
G_DECLARE_FINAL_TYPE (FileBrowserView, file_browser_view, FILE_BROWSER, VIEW, GObject);

G_DEFINE_TYPE (FileBrowserView, file_browser_view, G_TYPE_OBJECT);
static GParamSpec *properties[N_PROPS] = { NULL, };

static void
file_browser_view_get_property (GObject    *object,
                                guint       property_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  FileBrowserView *self = FILE_BROWSER_VIEW (object);

  switch (property_id)
    {
    case PROP_FACTORY:
      g_value_set_object (value, self->factory);
      break;

    case PROP_ICON_NAME:
      g_value_set_string (value, self->icon_name);
      break;

    case PROP_TITLE:
      g_value_set_string (value, self->title);
      break;

    case PROP_ORIENTATION:
      g_value_set_enum (value, self->orientation);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
file_browser_view_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  FileBrowserView *self = FILE_BROWSER_VIEW (object);

  switch (prop_id)
    {
    case PROP_FACTORY:
      g_set_object (&self->factory, g_value_get_object (value));
      break;

    case PROP_ICON_NAME:
      g_free (self->icon_name);
      self->icon_name = g_value_dup_string (value);
      break;

    case PROP_TITLE:
      g_free (self->title);
      self->title = g_value_dup_string (value);
      break;

    case PROP_ORIENTATION:
      self->orientation = g_value_get_enum (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
file_browser_view_finalize (GObject *object)
{
  FileBrowserView *self = FILE_BROWSER_VIEW (object);

  g_object_unref (self->factory);
  g_free (self->icon_name);
  g_free (self->title);

  G_OBJECT_CLASS (file_browser_view_parent_class)->dispose (object);
}

static void
file_browser_view_class_init (FileBrowserViewClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->get_property = file_browser_view_get_property;
  gobject_class->set_property = file_browser_view_set_property;
  gobject_class->finalize = file_browser_view_finalize;

  properties[PROP_FACTORY] =
    g_param_spec_object ("factory",
                         "factory",
                         "factory to use in the main view",
                         GTK_TYPE_LIST_ITEM_FACTORY,
                         G_PARAM_READWRITE);
  properties[PROP_ICON_NAME] =
    g_param_spec_string ("icon-name",
                         "icon name",
                         "icon to display for selecting this view",
                         NULL,
                         G_PARAM_READWRITE);
  properties[PROP_TITLE] =
    g_param_spec_string ("title",
                         "title",
                         "title to display for selecting this view",
                         NULL,
                         G_PARAM_READWRITE);
  properties[PROP_ORIENTATION] =
    g_param_spec_enum ("orientation",
                       "orientation",
                       "orientation of the view",
                       GTK_TYPE_ORIENTATION,
                       GTK_ORIENTATION_VERTICAL,
                       G_PARAM_READWRITE);

  g_object_class_install_properties (gobject_class, N_PROPS, properties);
}

static void file_browser_view_init (FileBrowserView *self)
{
}

G_MODULE_EXPORT char *
filebrowser_get_display_name (GObject *object,
                              GFileInfo *info)
{
  if (!info)
    return NULL;

  return g_strdup (g_file_info_get_attribute_string (info, "standard::display-name"));
}

G_MODULE_EXPORT char *
filebrowser_get_content_type (GObject *object,
                              GFileInfo *info)
{
  if (!info)
    return NULL;

  return g_strdup (g_file_info_get_attribute_string (info, "standard::content-type"));
}

G_MODULE_EXPORT char *
filebrowser_get_size (GObject *object,
                      GFileInfo *info)
{
  if (!info)
    return NULL;

  return g_format_size (g_file_info_get_attribute_uint64 (info, "standard::size"));
}

G_MODULE_EXPORT GIcon *
filebrowser_get_icon (GObject *object,
                      GFileInfo *info)
{
  GIcon *icon;

  if (info)
    icon = G_ICON (g_file_info_get_attribute_object (info, "standard::icon"));
  else
    icon = NULL;

  if (icon)
    g_object_ref (icon);

  return icon;
}

G_MODULE_EXPORT void
filebrowser_up_clicked_cb (GtkButton        *button,
                           GtkDirectoryList *list)
{
  GFile *file;

  file = g_file_get_parent (gtk_directory_list_get_file (list));
  if (file == NULL)
    return;

  gtk_directory_list_set_file (list, file);
}

G_MODULE_EXPORT void
filebrowser_view_activated_cb (GtkGridView      *view,
                               guint             pos,
                               GtkDirectoryList *list)
{
  GFileInfo *info;

  info = g_list_model_get_item (G_LIST_MODEL (gtk_grid_view_get_model (view)), pos);
  if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY)
    gtk_directory_list_set_file (list, G_FILE (g_file_info_get_attribute_object (info, "standard::file")));

  g_object_unref (info);
}

GtkWidget *
do_listview_filebrowser (GtkWidget *do_widget)
{
  if (!window)
    {
      GtkWidget *view;
      GtkBuilder *builder;
      GtkDirectoryList *dirlist;
      GFile *file;
      char *cwd;
      GtkCssProvider *provider;

      provider = gtk_css_provider_new ();
      gtk_css_provider_load_from_resource (provider, "/listview_filebrowser/listview_filebrowser.css");
      gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                                  GTK_STYLE_PROVIDER (provider),
                                                  800);
      g_object_unref (provider);

      builder = gtk_builder_new_from_resource ("/listview_filebrowser/listview_filebrowser.ui");
      window = GTK_WIDGET (gtk_builder_get_object (builder, "window"));
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *) &window);

      /* Create the model and fill it with the contents of the current directory */
      cwd = g_get_current_dir ();
      file = g_file_new_for_path (cwd);
      g_free (cwd);
      dirlist = GTK_DIRECTORY_LIST (gtk_builder_get_object (builder, "dirlist"));
      gtk_directory_list_set_file (dirlist, file);
      g_object_unref (file);

      /* grab focus in the view */
      view = GTK_WIDGET (gtk_builder_get_object (builder, "view"));
      gtk_widget_grab_focus (view);

      g_object_unref (builder);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_set_visible (window, TRUE);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
