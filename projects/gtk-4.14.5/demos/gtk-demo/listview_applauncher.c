/* Lists/Application launcher
 * #Keywords: GtkListItemFactory, GListModel
 *
 * This demo uses the GtkListView widget as a fancy application launcher.
 *
 * It is also a very small introduction to listviews.
 */

#include <gtk/gtk.h>

/* This is the function that creates the GListModel that we need.
 * GTK list widgets need a GListModel to display, as models support change
 * notifications.
 * Unfortunately various older APIs do not provide list models, so we create
 * our own.
 */
static GListModel *
create_application_list (void)
{
  GListStore *store;
  GList *apps, *l;

  /* We use a GListStore here, which is a simple array-like list implementation
   * for manual management.
   * List models need to know what type of data they provide, so we need to
   * provide the type here. As we want to do a list of applications, GAppInfo
   * is the object we provide.
   */
  store = g_list_store_new (G_TYPE_APP_INFO);

  apps = g_app_info_get_all ();

  for (l = apps; l; l = l->next)
    g_list_store_append (store, l->data);

  g_list_free_full (apps, g_object_unref);

  return G_LIST_MODEL (store);
}

/* This is the function we use for setting up new listitems to display.
 * We add just an GtkImage and a GtkLabel here to display the application's
 * icon and name, as this is just a simple demo.
 */
static void
setup_listitem_cb (GtkListItemFactory *factory,
                   GtkListItem        *list_item)
{
  GtkWidget *box;
  GtkWidget *image;
  GtkWidget *label;

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  image = gtk_image_new ();
  gtk_accessible_update_property (GTK_ACCESSIBLE (image),
                                  GTK_ACCESSIBLE_PROPERTY_LABEL,
                                  "App icon",
                                  -1);
  gtk_image_set_icon_size (GTK_IMAGE (image), GTK_ICON_SIZE_LARGE);
  gtk_box_append (GTK_BOX (box), image);
  label = gtk_label_new ("");
  gtk_box_append (GTK_BOX (box), label);
  gtk_list_item_set_child (list_item, box);
}

/* Here we need to prepare the listitem for displaying its item. We get the
 * listitem already set up from the previous function, so we can reuse the
 * GtkImage widget we set up above.
 * We get the item - which we know is a GAppInfo because it comes out of
 * the model we set up above, grab its icon and display it.
 */
static void
bind_listitem_cb (GtkListItemFactory *factory,
                  GtkListItem        *list_item)
{
  GtkWidget *image;
  GtkWidget *label;
  GAppInfo *app_info;

  image = gtk_widget_get_first_child (gtk_list_item_get_child (list_item));
  label = gtk_widget_get_next_sibling (image);
  app_info = gtk_list_item_get_item (list_item);

  gtk_image_set_from_gicon (GTK_IMAGE (image), g_app_info_get_icon (app_info));
  gtk_label_set_label (GTK_LABEL (label), g_app_info_get_display_name (app_info));
  gtk_list_item_set_accessible_label (list_item, g_app_info_get_display_name (app_info));
}

/* In more complex code, we would also need functions to unbind and teardown
 * the listitem, but this is simple code, so the default implementations are
 * enough. If we had connected signals, this step would have been necessary.
 *
 * The GtkSignalListItemFactory documentation contains more information about
 * this step.
 */

/* This function is called whenever an item in the list is activated. This is
 * the simple way to allow reacting to the Enter key or double-clicking on a
 * listitem.
 * Of course, it is possible to use far more complex interactions by turning
 * off activation and adding buttons or other widgets in the setup function
 * above, but this is a simple demo, so we'll use the simple way.
 */
static void
activate_cb (GtkListView  *list,
             guint         position,
             gpointer      unused)
{
  GAppInfo *app_info;
  GdkAppLaunchContext *context;
  GError *error = NULL;

  app_info = g_list_model_get_item (G_LIST_MODEL (gtk_list_view_get_model (list)), position);

  /* Prepare the context for launching the application and launch it. This
   * code is explained in detail in the documentation for GdkAppLaunchContext
   * and GAppInfo.
   */
  context = gdk_display_get_app_launch_context (gtk_widget_get_display (GTK_WIDGET (list)));
  if (!g_app_info_launch (app_info,
                          NULL,
                          G_APP_LAUNCH_CONTEXT (context),
                          &error))
    {
      GtkAlertDialog *dialog;

      /* And because error handling is important, even a simple demo has it:
       * We display an error dialog that something went wrong.
       */
      dialog = gtk_alert_dialog_new ("Could not launch %s", g_app_info_get_display_name (app_info));
      gtk_alert_dialog_set_detail (dialog, error->message);
      gtk_alert_dialog_show (dialog, GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (list))));
      g_object_unref (dialog);
      g_clear_error (&error);
    }

  g_object_unref (context);
  g_object_unref (app_info);
}

static GtkWidget *window = NULL;

GtkWidget *
do_listview_applauncher (GtkWidget *do_widget)
{
  if (window == NULL)
    {
      GtkWidget *list, *sw;
      GListModel *model;
      GtkListItemFactory *factory;

      /* Create a window and set a few defaults */
      window = gtk_window_new ();
      gtk_window_set_default_size (GTK_WINDOW (window), 640, 320);
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      gtk_window_set_title (GTK_WINDOW (window), "Application Launcher");
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *) &window);

      /* The GtkListitemFactory is what is used to create GtkListItems
       * to display the data from the model. So it is absolutely necessary
       * to create one.
       * We will use a GtkSignalListItemFactory because it is the simplest
       * one to use. Different ones are available for different use cases.
       * The most powerful one is GtkBuilderListItemFactory which uses
       * GtkBuilder .ui files, so it requires little code.
       */
      factory = gtk_signal_list_item_factory_new ();
      g_signal_connect (factory, "setup", G_CALLBACK (setup_listitem_cb), NULL);
      g_signal_connect (factory, "bind", G_CALLBACK (bind_listitem_cb), NULL);

      /* And of course we need to set the data model. Here we call the function
       * we wrote above that gives us the list of applications. Then we set
       * it on the list widget.
       * The list will now take items from the model and use the factory
       * to create as many listitems as it needs to show itself to the user.
       */
      model = create_application_list ();

      /* Create the list widget here.
       */
      list = gtk_list_view_new (GTK_SELECTION_MODEL (gtk_single_selection_new (model)), factory);

      /* We connect the activate signal here. It's the function we defined
       * above for launching the selected application.
       */
      g_signal_connect (list, "activate", G_CALLBACK (activate_cb), NULL);

      /* List widgets should always be contained in a GtkScrolledWindow,
       * because otherwise they might get too large or they might not
       * be scrollable.
       */
      sw = gtk_scrolled_window_new ();
      gtk_window_set_child (GTK_WINDOW (window), sw);
      gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), list);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_set_visible (window, TRUE);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
