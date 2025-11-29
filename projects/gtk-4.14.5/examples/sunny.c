#include <stdlib.h>
#include <gtk/gtk.h>

static void
new_window (GApplication *app,
            GFile        *file)
{
  GtkWidget *window, *scrolled, *view, *overlay;
  GtkWidget *header;

  window = gtk_application_window_new (GTK_APPLICATION (app));
  gtk_application_window_set_show_menubar (GTK_APPLICATION_WINDOW (window), TRUE);
  gtk_window_set_default_size ((GtkWindow*)window, 640, 480);
  gtk_window_set_title (GTK_WINDOW (window), "Sunny");
  gtk_window_set_icon_name (GTK_WINDOW (window), "weather-clear-symbolic");

  header = gtk_header_bar_new ();
  gtk_window_set_titlebar (GTK_WINDOW (window), header);

  overlay = gtk_overlay_new ();
  gtk_window_set_child (GTK_WINDOW (window), overlay);

  scrolled = gtk_scrolled_window_new ();
  gtk_widget_set_hexpand (scrolled, TRUE);
  gtk_widget_set_vexpand (scrolled, TRUE);
  view = gtk_text_view_new ();

  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolled), view);
  gtk_overlay_set_child (GTK_OVERLAY (overlay), scrolled);

  if (file != NULL)
    {
      char *contents;
      gsize length;

      if (g_file_load_contents (file, NULL, &contents, &length, NULL, NULL))
        {
          GtkTextBuffer *buffer;

          buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
          gtk_text_buffer_set_text (buffer, contents, length);
          g_free (contents);
        }
    }

  gtk_window_present (GTK_WINDOW (window));
}

static void
activate (GApplication *application)
{
  new_window (application, NULL);
}

static void
open (GApplication  *application,
      GFile        **files,
      int            n_files,
      const char    *hint)
{
  int i;

  for (i = 0; i < n_files; i++)
    new_window (application, files[i]);
}

typedef GtkApplication MenuButton;
typedef GtkApplicationClass MenuButtonClass;

static GType menu_button_get_type (void);
G_DEFINE_TYPE (MenuButton, menu_button, GTK_TYPE_APPLICATION)

static void
show_about (GSimpleAction *action,
            GVariant      *parameter,
            gpointer       user_data)
{
  gtk_show_about_dialog (NULL,
                         "program-name", "Sunny",
                         "title", "About Sunny",
                         "logo-icon-name", "weather-clear-symbolic",
                         "comments", "A cheap Bloatpad clone.",
                         NULL);
}


static void
quit_app (GSimpleAction *action,
          GVariant      *parameter,
          gpointer       user_data)
{
  GList *list, *next;
  GtkWindow *win;

  g_print ("Going down...\n");

  list = gtk_application_get_windows (GTK_APPLICATION (g_application_get_default ()));
  while (list)
    {
      win = list->data;
      next = list->next;

      gtk_window_destroy (GTK_WINDOW (win));

      list = next;
    }
}

static void
new_activated (GSimpleAction *action,
               GVariant      *parameter,
               gpointer       user_data)
{
  GApplication *app = user_data;

  g_application_activate (app);
}

static GActionEntry app_entries[] = {
  { "about", show_about, NULL, NULL, NULL },
  { "quit", quit_app, NULL, NULL, NULL },
  { "new", new_activated, NULL, NULL, NULL }
};

static void
startup (GApplication *application)
{
  GtkBuilder *builder;

  G_APPLICATION_CLASS (menu_button_parent_class)->startup (application);

  g_action_map_add_action_entries (G_ACTION_MAP (application), app_entries, G_N_ELEMENTS (app_entries), application);

  if (g_getenv ("APP_MENU_FALLBACK"))
    g_object_set (gtk_settings_get_default (), "gtk-shell-shows-app-menu", FALSE, NULL);
 
  builder = gtk_builder_new ();
  gtk_builder_add_from_string (builder,
                               "<interface>"
                               "  <menu id='menubar'>"
                               "    <submenu>"
                               "      <attribute name='label' translatable='yes'>Sunny</attribute>"
                               "      <section>"
                               "        <item>"
                               "          <attribute name='label' translatable='yes'>_New Window</attribute>"
                               "          <attribute name='action'>app.new</attribute>"
                               "        </item>"
                               "        <item>"
                               "          <attribute name='label' translatable='yes'>_About Sunny</attribute>"
                               "          <attribute name='action'>app.about</attribute>"
                               "        </item>"
                               "        <item>"
                               "          <attribute name='label' translatable='yes'>_Quit</attribute>"
                               "          <attribute name='action'>app.quit</attribute>"
                               "          <attribute name='accel'>&lt;Primary&gt;q</attribute>"
                               "        </item>"
                               "      </section>"
                               "    </submenu>"
                               "  </menu>"
                               "</interface>", -1, NULL);
  gtk_application_set_menubar (GTK_APPLICATION (application), G_MENU_MODEL (gtk_builder_get_object (builder, "menubar")));
  g_object_unref (builder);
}

static void
menu_button_init (MenuButton *app)
{
}

static void
menu_button_class_init (MenuButtonClass *class)
{
  GApplicationClass *application_class = G_APPLICATION_CLASS (class);

  application_class->startup = startup;
  application_class->activate = activate;
  application_class->open = open;
}

static MenuButton *
menu_button_new (void)
{
  return g_object_new (menu_button_get_type (),
                       "application-id", "org.gtk.Test.Sunny",
                       "flags", G_APPLICATION_HANDLES_OPEN,
                       NULL);
}

int
main (int argc, char **argv)
{
  MenuButton *menu_button;
  int status;

  menu_button = menu_button_new ();
  status = g_application_run (G_APPLICATION (menu_button), argc, argv);
  g_object_unref (menu_button);

  return status;
}
