#include <gtk/gtk.h>

static const char *icon_names[] = {
  /*** Icons used in code or templates, sorted alphabetically ***/
  "audio-volume-high",
  "audio-volume-high-symbolic",
  "audio-volume-low",
  "audio-volume-low-symbolic",
  "audio-volume-medium",
  "audio-volume-medium-symbolic",
  "audio-volume-muted",
  "audio-volume-muted-symbolic",
  "application-x-executable-symbolic",
  "bookmark-new-symbolic",
  "changes-allow-symbolic",
  "changes-prevent-symbolic",
  "dialog-password-symbolic",
  "dialog-warning-symbolic",
  "document-open-symbolic",
  "document-save",
  "document-save-as-symbolic",
  "document-save-symbolic",
  "edit-clear-symbolic",
  "edit-clear-all-symbolic",
  "edit-cut-symbolic",
  "edit-delete-symbolic",
  "edit-find-symbolic",
  "edit-paste-symbolic",
  "emblem-important-symbolic",
  "emblem-system-symbolic",
  "emoji-activities-symbolic",
  "emoji-body-symbolic",
  "emoji-flags-symbolic",
  "emoji-food-symbolic",
  "emoji-nature-symbolic",
  "emoji-objects-symbolic",
  "emoji-people-symbolic",
  "emoji-recent-symbolic",
  "emoji-symbols-symbolic",
  "emoji-travel-symbolic",
  "find-location-symbolic",
  "folder-new-symbolic",
  "folder-pictures-symbolic",
  "go-down-symbolic",
  "go-up-symbolic",
  "orientation-landscape-symbolic",
  "orientation-landscape-inverse-symbolic",
  "orientation-portrait-symbolic",
  "orientation-portrait-inverse-symbolic",
  "insert-image",
  "insert-object-symbolic",
  "list-add-symbolic",
  "list-remove-symbolic",
  "media-eject-symbolic",
  "media-playback-pause-symbolic",
  "media-playback-start-symbolic",
  "media-playlist-repeat",
  "media-record-symbolic",
  "network-server-symbolic",
  "object-select-symbolic",
  "open-menu-symbolic",
  "pan-down-symbolic",
  "pan-end-symbolic",
  "pan-start-symbolic",
  "pan-up-symbolic",
  "user-trash-symbolic",
  "view-list-symbolic",
  "window-close-symbolic",
  "window-maximize-symbolic",
  "window-minimize-symbolic",
  "window-restore-symbolic"
};

static void
test_icon_existence (gconstpointer icon_name)
{
  GtkIconTheme *icon_theme = gtk_icon_theme_get_for_display (gdk_display_get_default ());
  GtkIconPaintable *info;

  /* Not using generic fallback and builtins here, as we explicitly want to check the
   * icon theme.
   * The icon size is randomly chosen.
   */
  info = gtk_icon_theme_lookup_icon (icon_theme, icon_name, NULL, 16, 1, GTK_TEXT_DIR_LTR, 0);
  if (info == NULL)
    {
      g_test_message ("Failed to look up icon for \"%s\"", (char *) icon_name);
      g_test_fail ();
      return;
    }

  g_object_unref (info);
}

int
main (int argc, char *argv[])
{
  guint i;
  char *test_name;
  char *theme;

  gtk_test_init (&argc, &argv);

  g_object_get (gtk_settings_get_default (), "gtk-icon-theme-name", &theme, NULL);
  g_test_message ("Testing icon theme: %s", theme);
  g_free (theme);

  for (i = 0; i < G_N_ELEMENTS (icon_names); i++)
    {
      test_name = g_strdup_printf ("/check-icon-names/%s", icon_names[i]);
      g_test_add_data_func (test_name, icon_names[i], test_icon_existence);
      g_free (test_name);
    }

  return g_test_run();
}
