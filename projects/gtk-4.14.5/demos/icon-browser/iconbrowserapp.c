#include "config.h"
#include <gtk/gtk.h>

#include "iconbrowserapp.h"
#include "iconbrowserwin.h"

#include "profile_conf.h"

struct _IconBrowserApp
{
  GtkApplication parent;
};

struct _IconBrowserAppClass
{
  GtkApplicationClass parent_class;
};

G_DEFINE_TYPE(IconBrowserApp, icon_browser_app, GTK_TYPE_APPLICATION);

static void
icon_browser_app_init (IconBrowserApp *app)
{
}

static void
quit_activated (GSimpleAction *action,
                GVariant      *parameter,
                gpointer       app)
{
  g_application_quit (G_APPLICATION (app));
}

static void
inspector_activated (GSimpleAction *action,
                     GVariant      *parameter,
                     gpointer       app)
{
  gtk_window_set_interactive_debugging (TRUE);
}

static void
about_activated (GSimpleAction *action,
                 GVariant      *parameter,
                 gpointer       user_data)
{
  GtkApplication *app = user_data;
  const char *authors[] = {
    "The GTK Team",
    NULL
  };
  char *icon_theme;
  char *version;
  GString *s;
  char *os_name;
  char *os_version;

  g_object_get (gtk_settings_get_default (),
                "gtk-icon-theme-name", &icon_theme,
                NULL);

  s = g_string_new ("");

  os_name = g_get_os_info (G_OS_INFO_KEY_NAME);
  os_version = g_get_os_info (G_OS_INFO_KEY_VERSION_ID);
  if (os_name && os_version)
    g_string_append_printf (s, "OS\t%s %s\n\n", os_name, os_version);
  g_string_append (s, "System libraries\n");
  g_string_append_printf (s, "\tGLib\t%d.%d.%d\n",
                          glib_major_version,
                          glib_minor_version,
                          glib_micro_version);
  g_string_append_printf (s, "\tPango\t%s\n",
                          pango_version_string ());
  g_string_append_printf (s, "\tGTK \t%d.%d.%d\n",
                          gtk_get_major_version (),
                          gtk_get_minor_version (),
                          gtk_get_micro_version ());
  g_string_append_printf (s, "\nIcon theme\n\t%s", icon_theme);
  version = g_strdup_printf ("%s%s%s\nRunning against GTK %d.%d.%d",
                             PACKAGE_VERSION,
                             g_strcmp0 (PROFILE, "devel") == 0 ? "-" : "",
                             g_strcmp0 (PROFILE, "devel") == 0 ? VCS_TAG : "",
                             gtk_get_major_version (),
                             gtk_get_minor_version (),
                             gtk_get_micro_version ());

  gtk_show_about_dialog (GTK_WINDOW (gtk_application_get_active_window (app)),
                         "program-name", g_strcmp0 (PROFILE, "devel") == 0
                                         ? "GTK Icon Browser (Development)"
                                         : "GTK Icon Browser",
                         "version", version,
                         "copyright", "© 1997—2021 The GTK Team",
                         "license-type", GTK_LICENSE_LGPL_2_1,
                         "website", "http://www.gtk.org",
                         "comments", "Program to browse themed icons",
                         "authors", authors,
                         "logo-icon-name", "org.gtk.IconBrowser4",
                         "title", "About GTK Icon Browser",
                         "system-information", s->str,
                         NULL);

  g_string_free (s, TRUE);
  g_free (version);
  g_free (icon_theme);
  g_free (os_name);
  g_free (os_version);
}

static GActionEntry app_entries[] =
{
  { "quit", quit_activated, NULL, NULL, NULL },
  { "inspector", inspector_activated, NULL, NULL, NULL },
  { "about", about_activated, NULL, NULL, NULL }
};

static void
icon_browser_app_startup (GApplication *app)
{
  const char *quit_accels[2] = { "<Ctrl>Q", NULL };

  G_APPLICATION_CLASS (icon_browser_app_parent_class)->startup (app);

  g_action_map_add_action_entries (G_ACTION_MAP (app),
                                   app_entries, G_N_ELEMENTS (app_entries),
                                   app);
  gtk_application_set_accels_for_action (GTK_APPLICATION (app),
                                         "app.quit",
                                         quit_accels);
}

static void
icon_browser_app_activate (GApplication *app)
{
  IconBrowserWindow *win;

  win = icon_browser_window_new (ICON_BROWSER_APP (app));

  if (g_strcmp0 (PROFILE, "devel") == 0)
    gtk_widget_add_css_class (GTK_WIDGET (win), "devel");

  gtk_window_present (GTK_WINDOW (win));
}

static void
icon_browser_app_class_init (IconBrowserAppClass *class)
{
  G_APPLICATION_CLASS (class)->startup = icon_browser_app_startup;
  G_APPLICATION_CLASS (class)->activate = icon_browser_app_activate;
}

IconBrowserApp *
icon_browser_app_new (void)
{
  return g_object_new (ICON_BROWSER_APP_TYPE,
                       "application-id", "org.gtk.IconBrowser4",
                       NULL);
}
